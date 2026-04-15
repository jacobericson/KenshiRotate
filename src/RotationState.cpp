#include "RotationState.h"

#pragma warning(push)
#pragma warning(disable: 4091)
#include <kenshi/Item.h>
#pragma warning(pop)

std::set<std::string> RotationState::s_rotatedItems;
bool RotationState::hookSaveOK = false;
bool RotationState::hookLoadOK = false;

std::string RotationState::GetHandleKey(Item* item)
{
	return item->handle.toString();
}

bool RotationState::IsRotated(Item* item)
{
	if (s_rotatedItems.empty())
		return false;
	return s_rotatedItems.count(GetHandleKey(item)) > 0;
}

void RotationState::SetTracked(Item* item, bool rotated)
{
	std::string k = GetHandleKey(item);
	if (rotated)
		s_rotatedItems.insert(k);
	else
		s_rotatedItems.erase(k);
}

void RotationState::EraseKey(const std::string& key)
{
	s_rotatedItems.erase(key);
}

void RotationState::SwapItemDimensions(Item* item)
{
	int tmp = item->itemWidth;
	item->itemWidth = item->itemHeight;
	item->itemHeight = tmp;
}

bool RotationState::ApplyRotationState(Item* item, bool targetRotated)
{
	if (item->itemWidth == item->itemHeight)
		return false;
	if (item->isEquipped)
		return false;
	if (IsRotated(item) == targetRotated)
		return true;
	SwapItemDimensions(item);
	SetTracked(item, targetRotated);
	return true;
}
