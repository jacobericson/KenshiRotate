// KenshiRotate - Inventory item rotation plugin for Kenshi
// Allows rotating items in inventory by middle-clicking while hovering.

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>

#include <Debug.h>
#include <core/Functions.h>

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

// =====================================================
// Rotation State (runtime tracking)
// =====================================================
// Tracks which items are rotated during the current session using handle keys.
// Handles are stable within a session but NOT across save/load.
// Persistence across save/load is handled by injecting a "rotated" flag
// into the item's GameData save record (see serialise/load hooks below).

static std::set<std::string> g_rotatedItems;
static bool g_hookSaveOK = false;
static bool g_hookLoadOK = false;
static bool g_needsClear = true;

static std::string getHandleKey(Item* item)
{
	return item->handle.toString();
}

static bool isRotated(Item* item)
{
	if (g_rotatedItems.empty())
		return false;
	return g_rotatedItems.count(getHandleKey(item)) > 0;
}

static void setRotated(Item* item, bool rotated)
{
	std::string k = getHandleKey(item);
	if (rotated)
		g_rotatedItems.insert(k);
	else
		g_rotatedItems.erase(k);
}

static void SwapItemDimensions(Item* item)
{
	int tmp = item->itemWidth;
	item->itemWidth = item->itemHeight;
	item->itemHeight = tmp;
}

// =====================================================
// Texture Rotation (Ogre)
// =====================================================
// Creates a 90° CW rotated copy of an icon texture.
// Cached so each unique texture is only rotated once.
//
// D3D9 has rendering issues with NPOT textures created via createManual,
// so all rotated textures are padded to power-of-2 dimensions. The actual
// image occupies the top-left corner; UV coordinates crop to that region.

struct RotatedTextureInfo
{
	std::string name;
	float uMax; // actual width / POT width
	float vMax; // actual height / POT height
};

static std::map<std::string, RotatedTextureInfo> g_rotatedTextureCache;

static Ogre::uint32 nextPowerOf2(Ogre::uint32 v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

static bool GetOrCreateRotatedTexture(const std::string& originalName,
	RotatedTextureInfo& outInfo)
{
	if (originalName.empty())
		return false;

	// Check cache
	std::map<std::string, RotatedTextureInfo>::iterator cacheIt =
		g_rotatedTextureCache.find(originalName);
	if (cacheIt != g_rotatedTextureCache.end())
	{
		outInfo = cacheIt->second;
		return true;
	}

	std::string rotatedName = originalName + "__rot90";

	// Get the original texture
	Ogre::TexturePtr srcTex = Ogre::TextureManager::getSingleton().getByName(originalName);
	if (srcTex.isNull())
	{
		ErrorLog("[KenshiRotate] Could not find texture: " + originalName);
		return false;
	}

	// Convert texture to an Image for pixel access
	Ogre::Image srcImage;
	srcTex->convertToImage(srcImage);

	Ogre::uint32 srcW = srcImage.getWidth();
	Ogre::uint32 srcH = srcImage.getHeight();
	Ogre::PixelFormat fmt = srcImage.getFormat();
	size_t bpp = Ogre::PixelUtil::getNumElemBytes(fmt);

	if (srcW == 0 || srcH == 0 || bpp == 0)
	{
		ErrorLog("[KenshiRotate] Invalid texture dimensions or format");
		return false;
	}

	// Rotated image dimensions (swapped) and POT-padded texture dimensions
	Ogre::uint32 dstW = srcH;
	Ogre::uint32 dstH = srcW;
	Ogre::uint32 potW = nextPowerOf2(dstW);
	Ogre::uint32 potH = nextPowerOf2(dstH);

	// Allocate POT buffer, zero-filled (transparent black padding)
	size_t potBufSize = potW * potH * bpp;
	Ogre::uchar* potData = new Ogre::uchar[potBufSize];
	memset(potData, 0, potBufSize);

	// Rotate 90° CW: src(x, y) -> dst(srcH - 1 - y, x)
	// Write into top-left of POT buffer using potW as row stride
	const Ogre::uchar* srcData = srcImage.getData();
	size_t srcRowPitch = srcImage.getRowSpan();

	for (Ogre::uint32 y = 0; y < srcH; y++)
	{
		for (Ogre::uint32 x = 0; x < srcW; x++)
		{
			Ogre::uint32 dstX = srcH - 1 - y;
			Ogre::uint32 dstY = x;

			const Ogre::uchar* srcPixel = srcData + y * srcRowPitch + x * bpp;
			Ogre::uchar* dstPixel = potData + dstY * (potW * bpp) + dstX * bpp;
			memcpy(dstPixel, srcPixel, bpp);
		}
	}

	// Check if rotated texture already exists in Ogre (e.g. from a previous session)
	if (Ogre::TextureManager::getSingleton().getByName(rotatedName).isNull())
	{
		// Create POT texture and upload pixels via lock/unlock.
		// Avoid loadImage/blitFromMemory which deadlock from the UI thread.
		try
		{
			Ogre::TexturePtr rotTex = Ogre::TextureManager::getSingleton().createManual(
				rotatedName,
				Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				Ogre::TEX_TYPE_2D,
				potW, potH, 0, fmt,
				Ogre::TU_STATIC_WRITE_ONLY);

			Ogre::HardwarePixelBufferSharedPtr buf = rotTex->getBuffer();
			void* dest = buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);

			// POT texture — row pitch should match potW, but use buf width to be safe
			size_t gpuRowBytes = buf->getWidth() * bpp;
			size_t srcRowBytes = potW * bpp;
			size_t copyBytes = gpuRowBytes < srcRowBytes ? gpuRowBytes : srcRowBytes;
			if (gpuRowBytes != srcRowBytes)
				ErrorLog("[KenshiRotate] GPU row pitch mismatch");
			for (Ogre::uint32 row = 0; row < potH; row++)
			{
				memcpy(
					(Ogre::uchar*)dest + row * gpuRowBytes,
					potData + row * srcRowBytes,
					copyBytes);
			}

			buf->unlock();
		}
		catch (Ogre::Exception& e)
		{
			ErrorLog("[KenshiRotate] Failed to create rotated texture: "
				+ std::string(e.what()));
			delete[] potData;
			return false;
		}
	}

	delete[] potData;

	outInfo.name = rotatedName;
	outInfo.uMax = (float)dstW / (float)potW;
	outInfo.vMax = (float)dstH / (float)potH;
	g_rotatedTextureCache[originalName] = outInfo;

	return true;
}

// Apply rotated texture to an InventoryIcon's image widget
static void ApplyRotatedTexture(InventoryIcon* icon)
{
	if (!icon || !icon->image)
		return;

	std::string texName = ((MyGUI::SkinItem*)icon->image)->_getTextureName();
	// Don't re-rotate an already-rotated texture
	if (texName.find("__rot90") != std::string::npos)
		return;

	RotatedTextureInfo info;
	if (GetOrCreateRotatedTexture(texName, info))
	{
		// Set texture at the SkinItem level, then crop UVs to the actual
		// image area within the POT texture via the SubSkin.
		((MyGUI::SkinItem*)icon->image)->_setTextureName(info.name);
		MyGUI::ISubWidgetRect* main =
			((MyGUI::SkinItem*)icon->image)->getSubWidgetMain();
		if (main)
			main->_setUVSet(MyGUI::FloatRect(0, 0, info.uMax, info.vMax));
	}
}

// Restore original texture on an InventoryIcon
static void RestoreOriginalTexture(InventoryIcon* icon)
{
	if (!icon || !icon->image)
		return;

	std::string texName = ((MyGUI::SkinItem*)icon->image)->_getTextureName();
	size_t pos = texName.find("__rot90");
	if (pos != std::string::npos)
	{
		std::string originalName = texName.substr(0, pos);
		// Restore texture and reset UVs to full original texture
		((MyGUI::SkinItem*)icon->image)->_setTextureName(originalName);
		MyGUI::ISubWidgetRect* main =
			((MyGUI::SkinItem*)icon->image)->getSubWidgetMain();
		if (main)
			main->_setUVSet(MyGUI::FloatRect(0, 0, 1, 1));
	}
}

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
};

static Item* CallGetMouseItem(InventoryGUI* gui)
{
	return ((InventoryGUIAccess*)gui)->getMouseItem();
}

static void TryRotateItem(Item* item, InventoryGUI* sourceGUI)
{
	if (item->isEquipped)
	{
		ou->showPlayerAMessage("Cannot rotate: item is equipped", false);
		return;
	}
	if (item->itemWidth == item->itemHeight)
	{
		ou->showPlayerAMessage("Cannot rotate: item is square", false);
		return;
	}
	if (!g_hookSaveOK || !g_hookLoadOK)
	{
		ou->showPlayerAMessage("Cannot rotate: save/load hooks failed", false);
		return;
	}

	std::string sectionName = item->inventorySection;

	Inventory* inv = sourceGUI->_NV_getInventory();
	InventorySection* section = inv ? inv->getSection(sectionName) : NULL;
	if (!section)
		return;

	section->removeItem(item);
	SwapItemDimensions(item);

	if (!section->_NV_addItem(item, 1))
	{
		// Doesn't fit rotated — revert and put back
		SwapItemDimensions(item);
		ou->showPlayerAMessage("Cannot rotate: not enough space", false);
		if (!section->_NV_addItem(item, 1))
			ErrorLog("[KenshiRotate] CRITICAL: failed to revert item after rotation rejection");
		return;
	}

	bool wasRotated = isRotated(item);
	setRotated(item, !wasRotated);

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
				ApplyRotatedTexture(icon);
			else
				RestoreOriginalTexture(icon);
			break;
		}
	}
}

void InventoryGUI_update_hook(InventoryGUI* thisptr)
{
	InventoryGUI_update_orig(thisptr);

	// Child GUIs (backpack, container, loot): skip input processing.
	// The owner GUI handles input for all linked inventories.
	if (thisptr->ownerInventory != NULL)
		return;

	// Skip rotation during key capture (settings rebind in progress)
	if (RotateSettings::IsCapturing())
		return;

	// Check configured rotation trigger (debounced — only on rising edge)
	bool triggerDown = false;
	if (RotateSettings::GetBindType() == BIND_MIDDLE_MOUSE)
	{
		triggerDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	}
	else
	{
		triggerDown = key != NULL && key->keyboard != NULL &&
			key->keyboard->isKeyDown(RotateSettings::GetKeyCode());
	}

	if (triggerDown && !g_lastTriggerState)
	{
		// Search this GUI first, then fall back to child GUI
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

	if (item && isRotated(item))
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
		ApplyRotatedTexture(thisptr);
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
	if (item && isRotated(item))
	{
		// Bypass the original — it clamps using GameData (template) dimensions.
		// Use getPositionSlot for raw pixel-to-grid conversion (no item clamping),
		// then apply correct clamping using the item's actual instance dimensions.
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
	g_needsClear = true;

	GameData* state = Item_serialiseInv_orig(thisptr, container, refList);

	// Inject "rotated" flag into the item's save record
	if (state && isRotated(thisptr))
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

	// Clear stale rotation tracking on first item of a new load cycle
	if (g_needsClear)
	{
		g_rotatedItems.clear();
		g_needsClear = false;
	}

	// Check if this item was saved with our custom "rotated" flag
	auto it = state->bdata.find("rotated");
	if (it != state->bdata.end() && it->second)
	{
		// Re-apply dimension swap (game resets dims to template on load)
		SwapItemDimensions(thisptr);

		// Track in runtime set for this session
		setRotated(thisptr, true);
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

	RotateSettings::Init();

	// --- Diagnostic: compare &Class::Method vs GetProcAddress ---
	{
		HMODULE klib = GetModuleHandleA("KenshiLib");
		if (!klib) klib = GetModuleHandleA("KenshiLib.dll");

		void* fromRef = NULL;
		void (InventoryGUI::*mfp)() = &InventoryGUI::_NV_update;
		fromRef = (void*&)mfp;

		void* fromGPA = (void*)GetProcAddress(klib, "?_NV_update@InventoryGUI@@QEAAXXZ");

		std::stringstream ss;
		ss << "[KenshiRotate] DIAG: &Method = 0x" << std::hex << (uintptr_t)fromRef
		   << ", GetProcAddress = 0x" << (uintptr_t)fromGPA
		   << (fromRef == fromGPA ? " (MATCH)" : " (MISMATCH)");
		DebugLog(ss.str());
	}

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
		g_hookSaveOK = true;
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
		g_hookLoadOK = true;
		DebugLog("[KenshiRotate] Hooked Item::loadFromSerialiseInInventory OK");
	}

	if (!g_hookSaveOK || !g_hookLoadOK)
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

	DebugLog("[KenshiRotate] Plugin started successfully");
}
