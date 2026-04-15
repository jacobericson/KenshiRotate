#pragma once

#include <set>
#include <string>

class Item;

class RotationState
{
public:
	static bool IsRotated(Item* item);
	static void SetTracked(Item* item, bool rotated);
	static std::string GetHandleKey(Item* item);
	static void EraseKey(const std::string& key);
	static void SwapItemDimensions(Item* item);

	// Pure dimension/state flip. No section interaction, no UI refresh.
	// Returns false on validation failure (square or equipped).
	// No-op (returns true) if already in the target state.
	static bool ApplyRotationState(Item* item, bool targetRotated);

	static bool hookSaveOK;
	static bool hookLoadOK;

private:
	static std::set<std::string> s_rotatedItems;
};
