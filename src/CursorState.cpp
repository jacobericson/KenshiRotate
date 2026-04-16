#include "CursorState.h"

Item* CursorState::s_item = NULL;
InventoryIcon* CursorState::s_icon = NULL;
MyGUI::IntPoint CursorState::s_hoverPos;
bool CursorState::s_hasHoverPos = false;

void CursorState::Set(Item* item, InventoryIcon* icon)
{
	s_item = item;
	s_icon = icon;
}

void CursorState::Clear()
{
	s_item = NULL;
	s_icon = NULL;
}

Item* CursorState::GetItem()
{
	return s_item;
}

InventoryIcon* CursorState::GetIcon()
{
	return s_icon;
}

bool CursorState::IsHolding()
{
	return s_item != NULL;
}

void CursorState::StoreHoverPos(const MyGUI::IntPoint& absPos)
{
	s_hoverPos = absPos;
	s_hasHoverPos = true;
}

void CursorState::ClearHoverPos()
{
	s_hasHoverPos = false;
}

bool CursorState::TryGetHoverPos(MyGUI::IntPoint& out)
{
	if (!s_hasHoverPos)
		return false;
	out = s_hoverPos;
	return true;
}
