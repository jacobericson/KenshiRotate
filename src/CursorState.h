#pragma once

#include <mygui/MyGUI_Types.h>

class Item;
class InventoryIcon;

// Owns the cursor-held item/icon pair and a pre-pickup hover-position cache
// used for grab-offset correction. Item and icon are always set/cleared together.
class CursorState
{
public:
	static void Set(Item* item, InventoryIcon* icon);
	static void Clear();
	static Item* GetItem();
	static InventoryIcon* GetIcon();
	static bool IsHolding();

	static void StoreHoverPos(const MyGUI::IntPoint& absPos);
	static void ClearHoverPos();
	// Does NOT auto-clear — callers clear explicitly when the cursor branch ends.
	static bool TryGetHoverPos(MyGUI::IntPoint& out);

private:
	static Item* s_item;
	static InventoryIcon* s_icon;
	static MyGUI::IntPoint s_hoverPos;
	static bool s_hasHoverPos;
};
