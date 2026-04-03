#pragma once

enum TrKey {
	// Settings UI strings
	TR_MIDDLE_MOUSE,       // "Middle Mouse"
	TR_CONFLICT_PREFIX,    // "Warning: bound to '"
	TR_CONFLICT_SUFFIX,    // "'"
	TR_ROTATE_KEY_PREFIX,  // "Rotate key: "
	TR_PRESS_A_KEY,        // "Press a key..."
	TR_CAPTURE_HELP,       // "Middle mouse or keyboard. Esc to cancel."
	TR_CHANGE,             // "Change"
	TR_RESET,              // "Reset"
	// In-game popup messages
	TR_ERR_EQUIPPED,       // "Cannot rotate: item is equipped"
	TR_ERR_SQUARE,         // "Cannot rotate: item is square"
	TR_ERR_HOOKS_FAILED,   // "Cannot rotate: save/load hooks failed"
	TR_ERR_NO_SPACE,       // "Cannot rotate: not enough space"
	TR_NONE,               // "None"
	TR_SECONDARY_KEY_PREFIX, // "Secondary key: "
	TR_COUNT
};

const char* Tr(TrKey key);
void DetectLanguage();
