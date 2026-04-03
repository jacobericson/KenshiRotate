#pragma once

#include <ois/OISKeyboard.h>

enum BindType { BIND_MIDDLE_MOUSE = 0, BIND_KEYBOARD = 1, BIND_NONE = 2 };

namespace RotateSettings
{
	void Init();                // Load config file
	void InjectModsTabUI();     // Add keybind controls to Options -> Mods tab
	void ProcessCapture();      // Key capture tick (call from OptionsWindow update hook)

	BindType GetBindType();
	OIS::KeyCode GetKeyCode();
	BindType GetBindType2();
	OIS::KeyCode GetKeyCode2();
	bool IsCapturing();
}
