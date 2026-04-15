// KenshiRotate - Inventory item rotation plugin for Kenshi
// Allows rotating items in inventory by middle-clicking while hovering.

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>

#include <Debug.h>
#include <core/Functions.h>

#pragma warning(push)
#pragma warning(disable: 4091) // '__declspec(dllimport)' ignored (BaseLayout.h)
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Item.h>
#include <kenshi/Inventory.h>
#include <kenshi/InputHandler.h>
#include <kenshi/gui/InventoryGUI.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_SkinItem.h>
#include <mygui/MyGUI_InputManager.h>

#pragma warning(pop)

#include <ogre/OgreTextureManager.h>
#include <ogre/OgreTexture.h>
#include <ogre/OgreImage.h>
#include <ogre/OgrePixelFormat.h>
#include <ogre/OgreHardwarePixelBuffer.h>

#include <set>
#include <map>
#include <string>
#include <sstream>

#include <kenshi/GameData.h>
#include <kenshi/gui/OptionsWindow.h>

#include "Settings.h"
#include "Translate.h"
#include "RotationState.h"
#include "TextureRotation.h"
#include "MouseInventoryAccess.h"

// Cursor-held item tracking. Set when an InventoryIcon is created with
// parent==NULL (makeIconForItem creates cursor icons this way).
// Cleared when placeItemFromMouse succeeds.
static Item* g_cursorItem = NULL;
static InventoryIcon* g_cursorIcon = NULL;

// Cached absolute pixel position of the hovered rotated item's top-left corner.
// Updated every frame while mouse is over a rotated grid item.
// Used at pickup time to compute the correct unclamped grab offset.
static MyGUI::IntPoint g_hoveredItemAbsPos;
static bool g_hasHoveredItemPos = false;


// =====================================================
// Hook: InventoryGUI::_NV_update — middle-click rotation
// =====================================================

void (*InventoryGUI_update_orig)(InventoryGUI*) = NULL;
static bool g_lastTriggerState = false;

// Expose protected methods via accessor subclass.
class InventoryGUIAccess : public InventoryGUI
{
public:
	using InventoryGUI::getMouseItem;
	using InventoryGUI::refreshAllSections;
	using InventoryGUI::refreshSection;
	using InventoryGUI::placeItemFromMouse;
	using InventoryGUI::takeCertainAmountFrom;
};

static Item* CallGetMouseItem(InventoryGUI* gui)
{
	return ((InventoryGUIAccess*)gui)->getMouseItem();
}

static void TryRotateItem(Item* item, InventoryGUI* sourceGUI)
{
	if (item->isEquipped)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_EQUIPPED), false);
		return;
	}
	if (item->itemWidth == item->itemHeight)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_SQUARE), false);
		return;
	}
	if (!RotationState::hookSaveOK || !RotationState::hookLoadOK)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_HOOKS_FAILED), false);
		return;
	}

	std::string sectionName = item->inventorySection;

	Inventory* inv = sourceGUI->_NV_getInventory();
	InventorySection* section = inv ? inv->getSection(sectionName) : NULL;
	if (!section)
		return;

	bool wasRotated = RotationState::IsRotated(item);

	section->removeItem(item);
	RotationState::ApplyRotationState(item, !wasRotated);

	if (!section->_NV_addItem(item, 1))
	{
		// Doesn't fit rotated — revert and put back
		RotationState::ApplyRotationState(item, wasRotated);
		ou->showPlayerAMessage(Tr(TR_ERR_NO_SPACE), false);
		if (!section->_NV_addItem(item, 1))
			ErrorLog("[KenshiRotate] CRITICAL: failed to revert item after rotation rejection");
		return;
	}

	auto mapIt = sourceGUI->inventorySections.find(sectionName);
	if (mapIt == sourceGUI->inventorySections.end())
		return;

	InventorySectionGUI* sectionGUI = mapIt->second;
	sectionGUI->refreshIcons(section);

	for (size_t i = 0; i < sectionGUI->itemsIcons.size(); ++i)
	{
		InventoryIcon* icon = sectionGUI->itemsIcons[i];
		if (icon && icon->item == item)
		{
			MyGUI::types::TSize<int> sz = icon->getSize();
			int newW = sz.height;
			int newH = sz.width;
			MyGUI::Widget* w = icon->getWidget();
			if (w) w->setSize(newW, newH);
			if (icon->image) icon->image->setSize(newW, newH);
			if (icon->quantityText) icon->quantityText->setSize(newW, newH);
			if (!wasRotated)
				TextureRotation::ApplyRotatedTexture(icon);
			else
				TextureRotation::RestoreOriginalTexture(icon);
			break;
		}
	}
}

// =====================================================
// Hook: placeItemFromMouse — clear cursor tracking
// =====================================================

bool (*placeItemFromMouse_orig)(InventoryGUI*, const std::string,
	const MyGUI::types::TPoint<int>&) = NULL;

bool placeItemFromMouse_hook(InventoryGUI* thisptr, const std::string sectionName,
	const MyGUI::types::TPoint<int>& mousePos)
{
	Item* prevCursorItem = g_cursorItem;
	bool result = placeItemFromMouse_orig(thisptr, sectionName, mousePos);
	if (result && g_cursorItem == prevCursorItem)
	{
		// Normal placement — cursor is now empty.
		g_cursorItem = NULL;
		g_cursorIcon = NULL;
	}
	// Swap: setupCursorItem fired _CONSTRUCTOR which already set
	// g_cursorItem/g_cursorIcon to the swapped item — keep them.
	return result;
}

// =====================================================
// Hook: canStackWith — prevent cross-rotation stacking
// =====================================================

bool (*canStackWith_orig)(InventoryItemBase*, InventoryItemBase*) = NULL;

bool canStackWith_hook(InventoryItemBase* thisptr, InventoryItemBase* other)
{
	if (!thisptr || !other)
		return false;

	if (!canStackWith_orig(thisptr, other))
		return false;

	bool thisRotated = RotationState::IsRotated((Item*)thisptr);
	bool otherRotated = RotationState::IsRotated((Item*)other);
	if (thisRotated != otherRotated)
		return false;

	return true;
}

// =====================================================
// Hook: addQuantity — prevent cross-rotation quantity transfer
// =====================================================
// placeItemFromMouse calls addQuantity in its swap/fallback path even
// when canStackWith returned false.  Block the transfer here so that
// differently-rotated stacks are never merged.

void (*addQuantity_orig)(InventoryItemBase*, int*, Item*, InventorySection*) = NULL;

void addQuantity_hook(InventoryItemBase* thisptr, int* amount, Item* addedItem, InventorySection* section)
{
	if (addedItem)
	{
		bool thisRotated = RotationState::IsRotated((Item*)thisptr);
		bool addedRotated = RotationState::IsRotated(addedItem);
		if (thisRotated != addedRotated)
			return;
	}
	addQuantity_orig(thisptr, amount, addedItem, section);
}

// =====================================================
// Hook: takeCertainAmountFrom — propagate rotation on stack split
// =====================================================

Item* (*takeCertainAmountFrom_orig)(InventoryGUI*, Item*, int) = NULL;

Item* takeCertainAmountFrom_hook(InventoryGUI* thisptr, Item* baseItem, int amount)
{
	bool sourceRotated = baseItem && RotationState::IsRotated(baseItem);

	Item* newItem = takeCertainAmountFrom_orig(thisptr, baseItem, amount);

	if (newItem && newItem != baseItem && sourceRotated)
	{
		RotationState::SwapItemDimensions(newItem);
		RotationState::SetTracked(newItem, true);
	}

	return newItem;
}

// =====================================================
// Rotate cursor-held item (no section remove/add needed)
// =====================================================

static void TryRotateCursorItem()
{
	Item* item = g_cursorItem;
	InventoryIcon* icon = g_cursorIcon;

	if (!item || !icon)
		return;

	// Validate icon still references our tracked item (catches stale state)
	if (icon->item != item)
	{
		g_cursorItem = NULL;
		g_cursorIcon = NULL;
		return;
	}

	if (item->itemWidth == item->itemHeight)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_SQUARE), false);
		return;
	}
	if (!RotationState::hookSaveOK || !RotationState::hookLoadOK)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_HOOKS_FAILED), false);
		return;
	}

	bool wasRotated = RotationState::IsRotated(item);
	RotationState::ApplyRotationState(item, !wasRotated);

	// Resize icon widget
	MyGUI::types::TSize<int> sz = icon->getSize();
	int newW = sz.height;
	int newH = sz.width;

	// Fix grab offset so the item rotates around the mouse cursor.
	// The game stores grabOffset = itemTopLeft - mousePos at MouseInventory+128/+132.
	// After rotation, the mouse's relative position within the item changes.
	// We rewrite the stored offset so the game's per-frame positioning is correct.
	int grabX, grabY;
	if (MouseInventoryAccess::GetGrabOffset(grabX, grabY))
	{
		int newGrabX, newGrabY;
		if (!wasRotated)
		{
			// CW rotation: rel(relX,relY) -> (oldH-1-relY, relX)
			// grab = -rel, so newGrabX = -(oldH-1+grabY), newGrabY = grabX
			newGrabX = -sz.height + 1 - grabY;
			newGrabY = grabX;
		}
		else
		{
			// CCW (un-rotate): rel(relX,relY) -> (relY, oldW-1-relX)
			// newGrabX = grabY, newGrabY = -(oldW-1+grabX)
			newGrabX = grabY;
			newGrabY = -sz.width + 1 - grabX;
		}
		MouseInventoryAccess::SetGrabOffset(newGrabX, newGrabY);
	}

	MyGUI::Widget* w = icon->getWidget();
	if (w) w->setSize(newW, newH);
	if (icon->image) icon->image->setSize(newW, newH);
	if (icon->quantityText) icon->quantityText->setSize(newW, newH);

	// Update charge bar (update() does NOT run for cursor-held icons)
	if (icon->chargesProgress && item->originalFullChargeAmount > 0)
	{
		MyGUI::types::TSize<int> cpSize = icon->chargesProgress->getSize();
		int barW = (int)((newW - 2) * item->chargesLeft / item->originalFullChargeAmount);
		icon->chargesProgress->setSize(barW, cpSize.height);
	}

	// Resize shadow/highlight widget to match new icon dimensions
	if (MouseInventoryAccess::GetShadowWidget())
		MouseInventoryAccess::GetShadowWidget()->setSize(newW, newH);

	if (!wasRotated)
		TextureRotation::ApplyRotatedTexture(icon);
	else
		TextureRotation::RestoreOriginalTexture(icon);
}

void InventoryGUI_update_hook(InventoryGUI* thisptr)
{
	InventoryGUI_update_orig(thisptr);

	// Child GUIs (backpack, container, loot): skip input processing.
	// The owner GUI handles input for all linked inventories.
	if (thisptr->ownerInventory != NULL)
		return;

	// Eagerly discover MouseInventory singleton on first inventory frame
	// so the .data scan doesn't cause a hitch on first item pickup.
	MouseInventoryAccess::FindShadowWidget();
	MouseInventoryAccess::FindMouseInventory();

	// Cache hovered rotated item position for pickup offset correction.
	// Only runs when no item is held on cursor.
	if (g_cursorItem == NULL)
	{
		g_hasHoveredItemPos = false;
		Item* hoverItem = CallGetMouseItem(thisptr);
		InventoryGUI* hoverGUI = thisptr;
		if (!hoverItem && thisptr->childInventory)
		{
			hoverItem = CallGetMouseItem(thisptr->childInventory);
			hoverGUI = thisptr->childInventory;
		}
		if (hoverItem && RotationState::IsRotated(hoverItem))
		{
			std::string secName = hoverItem->inventorySection;
			auto mapIt = hoverGUI->inventorySections.find(secName);
			if (mapIt != hoverGUI->inventorySections.end())
			{
				InventorySectionGUI* secGUI = mapIt->second;
				g_hoveredItemAbsPos = secGUI->getItemAbsolutePosition(
					hoverItem->inventoryPos.x, hoverItem->inventoryPos.y);
				g_hasHoveredItemPos = true;
			}
		}
	}

	// Skip rotation during key capture (settings rebind in progress)
	if (RotateSettings::IsCapturing())
		return;

	// Check configured rotation trigger (debounced — only on rising edge)
	bool triggerDown = false;
	if (RotateSettings::GetBindType() == BIND_MIDDLE_MOUSE)
		triggerDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	else if (RotateSettings::GetBindType() == BIND_KEYBOARD)
		triggerDown = key != NULL && key->keyboard != NULL &&
			key->keyboard->isKeyDown(RotateSettings::GetKeyCode());

	// Secondary bind (BIND_NONE skipped naturally)
	if (!triggerDown)
	{
		if (RotateSettings::GetBindType2() == BIND_MIDDLE_MOUSE)
			triggerDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
		else if (RotateSettings::GetBindType2() == BIND_KEYBOARD)
			triggerDown = key != NULL && key->keyboard != NULL &&
				key->keyboard->isKeyDown(RotateSettings::GetKeyCode2());
	}

	if (triggerDown && !g_lastTriggerState)
	{
		if (g_cursorItem != NULL)
		{
			// Rotate cursor-held item. Grid rotation is blocked while holding.
			if (g_cursorIcon != NULL)
				TryRotateCursorItem();
		}
		else
		{
			// No item held — rotate grid-hover item
			Item* mouseItem = CallGetMouseItem(thisptr);
			InventoryGUI* sourceGUI = thisptr;
			if (!mouseItem && thisptr->childInventory)
			{
				mouseItem = CallGetMouseItem(thisptr->childInventory);
				sourceGUI = thisptr->childInventory;
			}

			if (mouseItem)
				TryRotateItem(mouseItem, sourceGUI);
		}
	}

	g_lastTriggerState = triggerDown;
}

// =====================================================
// Hook: InventoryIcon constructor — fix icon size
// =====================================================

InventoryIcon* (*InventoryIcon_ctor_orig)(InventoryIcon*, Item*,
	const MyGUI::types::TPoint<int>&, MyGUI::Widget*) = NULL;

InventoryIcon* InventoryIcon_ctor_hook(InventoryIcon* thisptr, Item* item,
	const MyGUI::types::TPoint<int>& position, MyGUI::Widget* parent)
{
	InventoryIcon* result = InventoryIcon_ctor_orig(thisptr, item, position, parent);

	// Track cursor icons — makeIconForItem creates them with parent==NULL
	if (parent == NULL && item != NULL)
	{
		g_cursorItem = item;
		g_cursorIcon = thisptr;

		// Fix pickup offset for rotated items.
		// setupCursorItem clamps the grab offset to template dimensions.
		// Recompute the correct offset from the cached pre-pickup grid position.
		if (RotationState::IsRotated(item) && g_hasHoveredItemPos && MouseInventoryAccess::IsReady())
		{
			MyGUI::InputManager* inputMgr = MyGUI::InputManager::getInstancePtr();
			if (inputMgr)
			{
				const MyGUI::IntPoint& mousePos = inputMgr->getMousePosition();
				int correctGrabX = g_hoveredItemAbsPos.left - mousePos.left;
				int correctGrabY = g_hoveredItemAbsPos.top - mousePos.top;

				// Apply same half-cell clamping as setupCursorItem but with instance dims
				int cellW = InventoryIcon::getItemPosition(1, 0).left;
				int cellH = InventoryIcon::getItemPosition(0, 1).top;
				int halfCellW = cellW / 2;
				int halfCellH = cellH / 2;
				int instancePixelW = item->itemWidth * cellW;
				int instancePixelH = item->itemHeight * cellH;
				int minX = -(instancePixelW - halfCellW);
				int maxX = -halfCellW;
				int minY = -(instancePixelH - halfCellH);
				int maxY = -halfCellH;

				if (correctGrabX < minX) correctGrabX = minX;
				if (correctGrabX > maxX) correctGrabX = maxX;
				if (correctGrabY < minY) correctGrabY = minY;
				if (correctGrabY > maxY) correctGrabY = maxY;

				MouseInventoryAccess::SetGrabOffset(correctGrabX, correctGrabY);
			}
		}
		g_hasHoveredItemPos = false;
	}

	if (item && RotationState::IsRotated(item))
	{
		MyGUI::types::TSize<int> origSize = thisptr->getSize();

		int newW = origSize.height;
		int newH = origSize.width;

		MyGUI::Widget* widget = thisptr->getWidget();
		if (widget) widget->setSize(newW, newH);
		if (thisptr->image) thisptr->image->setSize(newW, newH);
		if (thisptr->quantityText) thisptr->quantityText->setSize(newW, newH);
		if (thisptr->chargesProgress && item->originalFullChargeAmount > 0)
		{
			MyGUI::types::TSize<int> cpSize = thisptr->chargesProgress->getSize();
			int barW = (int)((newW - 2) * item->chargesLeft / item->originalFullChargeAmount);
			thisptr->chargesProgress->setSize(barW, cpSize.height);
		}

		// Apply rotated texture
		TextureRotation::ApplyRotatedTexture(thisptr);
	}

	return result;
}

// =====================================================
// Hook: getBestPositionSlot — fix highlight bounds
// =====================================================

bool (*getBestPositionSlot_orig)(InventorySectionGUI*,
	const MyGUI::types::TPoint<int>&, InventorySection*, Item*,
	MyGUI::types::TPoint<int>&) = NULL;

bool getBestPositionSlot_hook(InventorySectionGUI* thisptr,
	const MyGUI::types::TPoint<int>& position, InventorySection* section,
	Item* item, MyGUI::types::TPoint<int>& slot)
{
	if (item && RotationState::IsRotated(item))
	{
		// Bypass the original — it clamps using GameData (template) dimensions.
		// Use getPositionSlot for raw pixel-to-grid conversion (no item clamping),
		// then apply correct clamping using the item's actual instance dimensions.
		//
		MyGUI::types::TPoint<int> rawSlot = thisptr->getPositionSlot(position, section, true);

		int maxX = section->width - item->itemWidth;
		int maxY = section->height - item->itemHeight;
		if (maxX < 0) maxX = 0;
		if (maxY < 0) maxY = 0;

		slot.left = rawSlot.left;
		slot.top = rawSlot.top;
		if (slot.left > maxX) slot.left = maxX;
		if (slot.top > maxY) slot.top = maxY;
		if (slot.left < 0) slot.left = 0;
		if (slot.top < 0) slot.top = 0;

		// Validate placement at the clamped position
		return section->canItemGoHere(item, slot.left, slot.top);
	}

	return getBestPositionSlot_orig(thisptr, position, section, item, slot);
}

// =====================================================
// Hook: Item::serialiseInInventory — save rotation flag
// =====================================================

GameData* (*Item_serialiseInv_orig)(Item*, GameDataContainer*, GameData*) = NULL;

GameData* Item_serialiseInv_hook(Item* thisptr, GameDataContainer* container, GameData* refList)
{
	GameData* state = Item_serialiseInv_orig(thisptr, container, refList);

	// Inject "rotated" flag into the item's save record
	if (state && RotationState::IsRotated(thisptr))
		state->add("rotated", true, std::string(), false);

	return state;
}

// =====================================================
// Hook: Item::loadFromSerialiseInInventory — restore rotation
// =====================================================

void (*Item_loadInv_orig)(Item*, GameDataContainer*, GameData*) = NULL;

void Item_loadInv_hook(Item* thisptr, GameDataContainer* container, GameData* state)
{
	Item_loadInv_orig(thisptr, container, state);

	if (!state)
		return;

	// Check if this item was saved with our custom "rotated" flag
	auto it = state->bdata.find("rotated");
	if (it != state->bdata.end() && it->second)
	{
		// Re-apply dimension swap (game resets dims to template on load)
		RotationState::SwapItemDimensions(thisptr);

		// Track in runtime set for this session
		RotationState::SetTracked(thisptr, true);
	}
	else
	{
		// Clean up stale handle from a previous session if present.
		// Within a session handles are unique, so this only fires
		// after a full reload when a handle is reassigned.
		RotationState::EraseKey(RotationState::GetHandleKey(thisptr));
	}
}

// =====================================================
// Hook: OptionsWindow::update — settings injection + key capture
// =====================================================
// Widgets in the Options tab get destroyed when the menu closes.
// Like RE_Kenshi, we re-inject every frame (InjectModsTabUI is
// idempotent — it checks for existing widgets before creating).

void (*OptionsWindow_update_orig)(OptionsWindow*) = NULL;

void OptionsWindow_update_hook(OptionsWindow* thisptr)
{
	OptionsWindow_update_orig(thisptr);
	RotateSettings::InjectModsTabUI();
	RotateSettings::ProcessCapture();
}

// =====================================================
// Plugin Entry Point
// =====================================================

__declspec(dllexport) void startPlugin()
{
	DebugLog("[KenshiRotate] Starting plugin...");

	DetectLanguage();
	RotateSettings::Init();

	// --- Hook 1: InventoryGUI::_NV_update for rotation trigger ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&InventoryGUI::_NV_update),
		(void*)InventoryGUI_update_hook,
		(void**)&InventoryGUI_update_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook InventoryGUI::_NV_update");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked InventoryGUI::_NV_update OK");
	}

	// --- Hook 2: InventoryIcon::_CONSTRUCTOR for icon resizing ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&InventoryIcon::_CONSTRUCTOR),
		(void*)InventoryIcon_ctor_hook,
		(void**)&InventoryIcon_ctor_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook InventoryIcon::_CONSTRUCTOR");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked InventoryIcon::_CONSTRUCTOR OK");
	}

	// --- Hook 3: getBestPositionSlot fix ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&InventorySectionGUI::getBestPositionSlot),
		(void*)getBestPositionSlot_hook,
		(void**)&getBestPositionSlot_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook getBestPositionSlot");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked getBestPositionSlot OK");
	}

	// --- Hook 4: Item::serialiseInInventory for saving rotation flag ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&Item::_NV_serialiseInInventory),
		(void*)Item_serialiseInv_hook,
		(void**)&Item_serialiseInv_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook Item::serialiseInInventory");
	}
	else
	{
		RotationState::hookSaveOK = true;
		DebugLog("[KenshiRotate] Hooked Item::serialiseInInventory OK");
	}

	// --- Hook 5: Item::loadFromSerialiseInInventory for restoring rotation ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&Item::_NV_loadFromSerialiseInInventory),
		(void*)Item_loadInv_hook,
		(void**)&Item_loadInv_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook Item::loadFromSerialiseInInventory");
	}
	else
	{
		RotationState::hookLoadOK = true;
		DebugLog("[KenshiRotate] Hooked Item::loadFromSerialiseInInventory OK");
	}

	// Note: if only one persistence hook succeeded (e.g. load but not save),
	// rotations would load from existing saves but never persist new ones —
	// creating silent data loss. We block rotation entirely if either fails.
	if (!RotationState::hookSaveOK || !RotationState::hookLoadOK)
		ErrorLog("[KenshiRotate] WARNING: persistence hooks incomplete — rotation will be disabled");

	// --- Hook 6: OptionsWindow::update for settings UI + key capture ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&OptionsWindow::_NV_update),
		(void*)OptionsWindow_update_hook,
		(void**)&OptionsWindow_update_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook OptionsWindow::update");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked OptionsWindow::update OK");
	}

	// --- Hook 7: placeItemFromMouse for cursor state tracking ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&InventoryGUIAccess::placeItemFromMouse),
		(void*)placeItemFromMouse_hook,
		(void**)&placeItemFromMouse_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook placeItemFromMouse");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked placeItemFromMouse OK");
	}

	// --- Hook 8: canStackWith — prevent cross-rotation stacking ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&InventoryItemBase::canStackWith),
		(void*)canStackWith_hook,
		(void**)&canStackWith_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook canStackWith");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked canStackWith OK");
	}

	// --- Hook 9: takeCertainAmountFrom — propagate rotation on split ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&InventoryGUIAccess::takeCertainAmountFrom),
		(void*)takeCertainAmountFrom_hook,
		(void**)&takeCertainAmountFrom_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook takeCertainAmountFrom");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked takeCertainAmountFrom OK");
	}

	// --- Hook 10: addQuantity — prevent cross-rotation quantity transfer ---
	if (KenshiLib::SUCCESS != KenshiLib::AddHook(
		(void*)KenshiLib::GetRealAddress(&InventoryItemBase::addQuantity),
		(void*)addQuantity_hook,
		(void**)&addQuantity_orig))
	{
		ErrorLog("[KenshiRotate] Failed to hook addQuantity");
	}
	else
	{
		DebugLog("[KenshiRotate] Hooked addQuantity OK");
	}

	DebugLog("[KenshiRotate] Plugin started successfully");
}

// =====================================================
// Public C API for cross-mod integration (e.g. StackSort)
// =====================================================
// Consumer loads via GetModuleHandle("KenshiRotate.dll") + GetProcAddress.
// All exports use void* instead of Item* to avoid header dependencies.

extern "C"
{

__declspec(dllexport) int KenshiRotate_ApiVersion()
{
	return 1;
}

__declspec(dllexport) int KenshiRotate_IsRotated(void* item)
{
	if (!item)
		return 0;
	return RotationState::IsRotated((Item*)item) ? 1 : 0;
}

__declspec(dllexport) int KenshiRotate_CanRotate(void* item)
{
	if (!item)
		return 0;
	Item* it = (Item*)item;
	if (it->isEquipped)
		return 0;
	if (it->itemWidth == it->itemHeight)
		return 0;
	if (!RotationState::hookSaveOK || !RotationState::hookLoadOK)
		return 0;
	return 1;
}

__declspec(dllexport) int KenshiRotate_SetRotated(void* item, int rotated)
{
	if (!item)
		return 0;
	return RotationState::ApplyRotationState((Item*)item, rotated != 0) ? 1 : 0;
}

__declspec(dllexport) void KenshiRotate_RefreshVisuals(void* inventoryGUI)
{
	if (!inventoryGUI)
		return;

	InventoryGUI* gui = (InventoryGUI*)inventoryGUI;

	// Cell dimensions in pixels — constant across all sections
	int cellW = InventoryIcon::getItemPosition(1, 0).left;
	int cellH = InventoryIcon::getItemPosition(0, 1).top;

	for (auto mapIt = gui->inventorySections.begin();
		mapIt != gui->inventorySections.end(); ++mapIt)
	{
		InventorySectionGUI* sectionGUI = mapIt->second;
		if (!sectionGUI)
			continue;

		for (size_t i = 0; i < sectionGUI->itemsIcons.size(); ++i)
		{
			InventoryIcon* icon = sectionGUI->itemsIcons[i];
			if (!icon || !icon->item)
				continue;

			// Compute expected size from item's runtime dims (already swapped
			// for rotated items). Works whether the ctor hook already fired
			// (refreshAllSections recreated icons) or not (reused widgets).
			int expectedW = icon->item->itemWidth * cellW;
			int expectedH = icon->item->itemHeight * cellH;
			MyGUI::types::TSize<int> sz = icon->getSize();

			if (sz.width != expectedW || sz.height != expectedH)
			{
				MyGUI::Widget* w = icon->getWidget();
				if (w) w->setSize(expectedW, expectedH);
				if (icon->image) icon->image->setSize(expectedW, expectedH);
				if (icon->quantityText) icon->quantityText->setSize(expectedW, expectedH);
			}

			if (RotationState::IsRotated(icon->item))
				TextureRotation::ApplyRotatedTexture(icon);
			else
				TextureRotation::RestoreOriginalTexture(icon);
		}
	}
}

} // extern "C"
