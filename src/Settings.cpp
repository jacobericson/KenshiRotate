// KenshiRotate Settings — Config persistence, Options -> Mods tab UI, key capture

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>

#include "Settings.h"
#include "Translate.h"

#include <Debug.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_Delegate.h>

#include <fstream>
#include <sstream>
#include <string>

// =====================================================
// Internal state
// =====================================================

static BindType g_bindType = BIND_MIDDLE_MOUSE;
static OIS::KeyCode g_keyCode = OIS::KC_UNASSIGNED;
static BindType g_bindType2 = BIND_NONE;
static OIS::KeyCode g_keyCode2 = OIS::KC_UNASSIGNED;
static int g_captureTarget = 0; // 0=none, 1=primary, 2=secondary

// Widget pointers (valid only while Options -> Mods tab widgets exist)
static MyGUI::TextBox* g_keyLabel = NULL;
static MyGUI::TextBox* g_conflictLabel = NULL;
static MyGUI::Button* g_changeBtn = NULL;
static MyGUI::TextBox* g_keyLabel2 = NULL;
static MyGUI::TextBox* g_conflictLabel2 = NULL;
static MyGUI::Button* g_changeBtn2 = NULL;

// =====================================================
// Config file I/O
// =====================================================

static std::string GetConfigFilePath()
{
	char path[MAX_PATH];
	HMODULE hm = NULL;
	GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&GetConfigFilePath, &hm);
	GetModuleFileNameA(hm, path, sizeof(path));
	std::string dir(path);
	size_t pos = dir.find_last_of("\\/");
	if (pos != std::string::npos)
		dir = dir.substr(0, pos + 1);
	return dir + "KenshiRotate.cfg";
}

static void LoadConfig()
{
	std::string cfgPath = GetConfigFilePath();
	std::ifstream file(cfgPath.c_str());
	if (!file.is_open())
	{
		DebugLog("[KenshiRotate] No config file, using defaults");
		return;
	}

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		std::string k = line.substr(0, eq);
		std::string v = line.substr(eq + 1);

		// Primary rejects BIND_NONE (2): if secondary is also unset, the user would
		// have no working rotation key. Secondary allows 0/1/2 by design.
		if (k == "bind_type")
		{
			int val = atoi(v.c_str());
			if (val == 0 || val == 1)
				g_bindType = (BindType)val;
		}
		else if (k == "ois_keycode")
		{
			int val = atoi(v.c_str());
			if (val >= 0 && val <= 0xED)
				g_keyCode = (OIS::KeyCode)val;
		}
		else if (k == "bind_type_2")
		{
			int val = atoi(v.c_str());
			if (val >= 0 && val <= 2)
				g_bindType2 = (BindType)val;
		}
		else if (k == "ois_keycode_2")
		{
			int val = atoi(v.c_str());
			if (val >= 0 && val <= 0xED)
				g_keyCode2 = (OIS::KeyCode)val;
		}
	}
}

static void SaveConfig()
{
	std::string cfgPath = GetConfigFilePath();
	std::ofstream file(cfgPath.c_str());
	if (!file.is_open())
	{
		ErrorLog("[KenshiRotate] Could not write config: " + cfgPath);
		return;
	}

	std::stringstream ss;
	ss << "# KenshiRotate configuration\n"
	   << "bind_type=" << (int)g_bindType << "\n"
	   << "ois_keycode=" << (int)g_keyCode << "\n"
	   << "bind_type_2=" << (int)g_bindType2 << "\n"
	   << "ois_keycode_2=" << (int)g_keyCode2 << "\n";
	file << ss.str();
}

// =====================================================
// Widget search (RE_Kenshi pattern — Kenshi prefixes
// widget names as "prefix_Name", so we match by suffix)
// =====================================================

static MyGUI::Widget* FindWidget(MyGUI::EnumeratorWidgetPtr enumerator,
	const std::string& name)
{
	while (enumerator.next())
	{
		std::string widgetName = enumerator.current()->getName();
		size_t splitPos = widgetName.find('_');
		if (splitPos != std::string::npos &&
			widgetName.substr(splitPos + 1) == name)
		{
			return enumerator.current();
		}
		if (enumerator.current()->getChildCount() > 0)
		{
			MyGUI::Widget* child = FindWidget(
				enumerator.current()->getEnumerator(), name);
			if (child != NULL)
				return child;
		}
	}
	return NULL;
}

// =====================================================
// Key display helpers
// =====================================================

static bool IsBlacklisted(OIS::KeyCode kc)
{
	switch (kc)
	{
	case OIS::KC_ESCAPE:
	case OIS::KC_LSHIFT:
	case OIS::KC_RSHIFT:
	case OIS::KC_LCONTROL:
	case OIS::KC_RCONTROL:
	case OIS::KC_LMENU:
	case OIS::KC_RMENU:
		return true;
	default:
		return false;
	}
}

static std::string GetKeyDisplayName(BindType type, OIS::KeyCode kc)
{
	if (type == BIND_NONE)
		return Tr(TR_NONE);
	if (type == BIND_MIDDLE_MOUSE)
		return Tr(TR_MIDDLE_MOUSE);

	if (key != NULL)
		return key->keyString((int)kc, true);

	// Fallback if InputHandler not yet available
	std::stringstream ss;
	ss << "Key 0x" << std::hex << (int)kc;
	return ss.str();
}

static std::string GetConflictText(BindType type, OIS::KeyCode kc)
{
	if (type == BIND_MIDDLE_MOUSE || type == BIND_NONE)
		return "";

	if (key == NULL)
		return "";

	if (!key->isBound((int)kc))
		return "";

	const std::string& cmd = key->getBoundCommand((int)kc, InputHandler::GLOBAL);
	if (cmd.empty())
		return "";

	return std::string(Tr(TR_CONFLICT_PREFIX)) + cmd + Tr(TR_CONFLICT_SUFFIX);
}

static void UpdateLabels()
{
	if (g_keyLabel != NULL)
	{
		std::string name = GetKeyDisplayName(g_bindType, g_keyCode);
		g_keyLabel->setCaption(std::string(Tr(TR_ROTATE_KEY_PREFIX)) + name);
	}
	if (g_conflictLabel != NULL)
		g_conflictLabel->setCaption(GetConflictText(g_bindType, g_keyCode));
	if (g_keyLabel2 != NULL)
	{
		std::string name2 = GetKeyDisplayName(g_bindType2, g_keyCode2);
		g_keyLabel2->setCaption(std::string(Tr(TR_SECONDARY_KEY_PREFIX)) + name2);
	}
	if (g_conflictLabel2 != NULL)
		g_conflictLabel2->setCaption(GetConflictText(g_bindType2, g_keyCode2));
}

// =====================================================
// Button callbacks
// =====================================================

static void OnChangePressed(MyGUI::Widget* sender)
{
	g_captureTarget = (sender == g_changeBtn2) ? 2 : 1;
	MyGUI::Button* btn = (g_captureTarget == 2) ? g_changeBtn2 : g_changeBtn;
	MyGUI::TextBox* lbl = (g_captureTarget == 2) ? g_conflictLabel2 : g_conflictLabel;
	if (btn != NULL)
		btn->setCaption(Tr(TR_PRESS_A_KEY));
	if (lbl != NULL)
		lbl->setCaption(Tr(TR_CAPTURE_HELP));
}

static void OnResetPressed(MyGUI::Widget* sender)
{
	bool secondary = (sender->getName() == "KenshiRotateReset2");
	if (secondary)
	{
		g_bindType2 = BIND_NONE;
		g_keyCode2 = OIS::KC_UNASSIGNED;
		DebugLog("[KenshiRotate] Secondary binding reset to None");
	}
	else
	{
		g_bindType = BIND_MIDDLE_MOUSE;
		g_keyCode = OIS::KC_UNASSIGNED;
		DebugLog("[KenshiRotate] Binding reset to Middle Mouse");
	}
	g_captureTarget = 0;
	SaveConfig();
	UpdateLabels();
	if (g_changeBtn != NULL)
		g_changeBtn->setCaption(Tr(TR_CHANGE));
	if (g_changeBtn2 != NULL)
		g_changeBtn2->setCaption(Tr(TR_CHANGE));
}

// =====================================================
// Key capture scanning
// =====================================================

static OIS::KeyCode ScanForPressedKey()
{
	if (key == NULL || key->keyboard == NULL)
		return OIS::KC_UNASSIGNED;

	for (int kc = 0x02; kc <= 0xED; ++kc)
	{
		if (IsBlacklisted((OIS::KeyCode)kc))
			continue;
		if (key->keyboard->isKeyDown((OIS::KeyCode)kc))
			return (OIS::KeyCode)kc;
	}
	return OIS::KC_UNASSIGNED;
}

static void ApplyBinding(int target, BindType type, OIS::KeyCode kc)
{
	if (target == 2)
	{
		g_bindType2 = type;
		g_keyCode2 = kc;
	}
	else
	{
		g_bindType = type;
		g_keyCode = kc;
	}
	g_captureTarget = 0;
	SaveConfig();
	UpdateLabels();
	if (g_changeBtn != NULL)
		g_changeBtn->setCaption(Tr(TR_CHANGE));
	if (g_changeBtn2 != NULL)
		g_changeBtn2->setCaption(Tr(TR_CHANGE));

	std::string name = GetKeyDisplayName(type, kc);
	const char* which = (target == 2) ? "Secondary bound to " : "Bound to ";
	DebugLog(std::string("[KenshiRotate] ") + which + name);
}

// =====================================================
// Public API
// =====================================================

void RotateSettings::Init()
{
	LoadConfig();
	std::string name = GetKeyDisplayName(g_bindType, g_keyCode);
	DebugLog("[KenshiRotate] Rotation key: " + name);
	std::string name2 = GetKeyDisplayName(g_bindType2, g_keyCode2);
	DebugLog("[KenshiRotate] Secondary key: " + name2);
}

void RotateSettings::InjectModsTabUI()
{
	MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
	if (gui == NULL)
		return;

	MyGUI::Widget* optionsTabWidget = FindWidget(gui->getEnumerator(), "OptionsTab");
	if (optionsTabWidget == NULL)
		return;

	MyGUI::TabControl* tabCtrl = optionsTabWidget->castType<MyGUI::TabControl>(false);
	if (tabCtrl == NULL || tabCtrl->getItemCount() == 0)
		return;

	// Find the MODS tab using language-independent strategies.
	// getItemNameAt() returns the localized caption, which differs by language.
	// Strategy 1: match by caption "MODS" (English)
	// Strategy 2: match by widget name suffix (layout XML name, never translated)
	// Strategy 3: fall back to index 5 (vanilla always creates 6 tabs: General,
	//             Gameplay, Graphics, Audio, Controls, Mods)
	MyGUI::TabItem* tab = NULL;
	for (size_t i = 0; i < tabCtrl->getItemCount(); ++i)
	{
		if (tabCtrl->getItemNameAt(i) == "MODS")
		{
			tab = tabCtrl->getItemAt(i);
			break;
		}
	}
	if (tab == NULL)
	{
		for (size_t i = 0; i < tabCtrl->getItemCount(); ++i)
		{
			std::string widgetName = tabCtrl->getItemAt(i)->getName();
			size_t splitPos = widgetName.find('_');
			if (splitPos != std::string::npos)
			{
				std::string suffix = widgetName.substr(splitPos + 1);
				if (suffix == "Mods" || suffix == "ModTab")
				{
					tab = tabCtrl->getItemAt(i);
					break;
				}
			}
		}
	}
	if (tab == NULL && tabCtrl->getItemCount() >= 6)
		tab = tabCtrl->getItemAt(5);
	if (tab == NULL)
		return;

	// Check if our widgets already exist (widgets get destroyed when Options closes)
	if (tab->findWidget("KenshiRotateChange") != NULL)
		return;

	// Null out stale pointers — previous widgets were destroyed
	g_keyLabel = NULL;
	g_conflictLabel = NULL;
	g_changeBtn = NULL;
	g_keyLabel2 = NULL;
	g_conflictLabel2 = NULL;
	g_changeBtn2 = NULL;

	// Position below RE_Kenshi's button area (they use y ~0.02, h ~0.05)
	float baseY = 0.09f;
	float x = 0.60f;
	float w = 0.25f;

	// Section header
	MyGUI::TextBox* header = tab->createWidgetReal<MyGUI::TextBox>(
		"Kenshi_TextboxStandardText",
		x, baseY, w, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateHeader");
	header->setCaption("KenshiRotate");

	// Current key display
	g_keyLabel = tab->createWidgetReal<MyGUI::TextBox>(
		"Kenshi_TextboxStandardText",
		x, baseY + 0.045f, w, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateKeyLabel");

	// Change button
	g_changeBtn = tab->createWidgetReal<MyGUI::Button>(
		"Kenshi_Button1",
		x, baseY + 0.09f, 0.12f, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateChange");
	g_changeBtn->setCaption(Tr(TR_CHANGE));
	g_changeBtn->eventMouseButtonClick += MyGUI::newDelegate(OnChangePressed);

	// Reset button
	MyGUI::Button* resetBtn = tab->createWidgetReal<MyGUI::Button>(
		"Kenshi_Button1",
		x + 0.13f, baseY + 0.09f, 0.12f, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateReset");
	resetBtn->setCaption(Tr(TR_RESET));
	resetBtn->eventMouseButtonClick += MyGUI::newDelegate(OnResetPressed);

	// Conflict warning
	g_conflictLabel = tab->createWidgetReal<MyGUI::TextBox>(
		"Kenshi_TextboxStandardText",
		x, baseY + 0.135f, w, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateConflict");

	// Secondary key display
	g_keyLabel2 = tab->createWidgetReal<MyGUI::TextBox>(
		"Kenshi_TextboxStandardText",
		x, baseY + 0.18f, w, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateKeyLabel2");

	// Secondary change button
	g_changeBtn2 = tab->createWidgetReal<MyGUI::Button>(
		"Kenshi_Button1",
		x, baseY + 0.225f, 0.12f, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateChange2");
	g_changeBtn2->setCaption(Tr(TR_CHANGE));
	g_changeBtn2->eventMouseButtonClick += MyGUI::newDelegate(OnChangePressed);

	// Secondary reset button
	MyGUI::Button* resetBtn2 = tab->createWidgetReal<MyGUI::Button>(
		"Kenshi_Button1",
		x + 0.13f, baseY + 0.225f, 0.12f, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateReset2");
	resetBtn2->setCaption(Tr(TR_RESET));
	resetBtn2->eventMouseButtonClick += MyGUI::newDelegate(OnResetPressed);

	// Secondary conflict warning
	g_conflictLabel2 = tab->createWidgetReal<MyGUI::TextBox>(
		"Kenshi_TextboxStandardText",
		x, baseY + 0.27f, w, 0.04f,
		MyGUI::Align::Top | MyGUI::Align::Left,
		"KenshiRotateConflict2");

	UpdateLabels();
	DebugLog("[KenshiRotate] Injected settings into options tab");
}

void RotateSettings::ProcessCapture()
{
	if (g_captureTarget == 0)
		return;

	// Escape cancels capture
	if (key != NULL && key->keyboard != NULL &&
		key->keyboard->isKeyDown(OIS::KC_ESCAPE))
	{
		MyGUI::Button* btn = (g_captureTarget == 2) ? g_changeBtn2 : g_changeBtn;
		g_captureTarget = 0;
		if (btn != NULL)
			btn->setCaption(Tr(TR_CHANGE));
		UpdateLabels();
		return;
	}

	// Middle mouse button
	if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
	{
		ApplyBinding(g_captureTarget, BIND_MIDDLE_MOUSE, OIS::KC_UNASSIGNED);
		return;
	}

	// Keyboard scan
	OIS::KeyCode pressed = ScanForPressedKey();
	if (pressed != OIS::KC_UNASSIGNED)
	{
		ApplyBinding(g_captureTarget, BIND_KEYBOARD, pressed);
		return;
	}
}

BindType RotateSettings::GetBindType()
{
	return g_bindType;
}

OIS::KeyCode RotateSettings::GetKeyCode()
{
	return g_keyCode;
}

BindType RotateSettings::GetBindType2()
{
	return g_bindType2;
}

OIS::KeyCode RotateSettings::GetKeyCode2()
{
	return g_keyCode2;
}

bool RotateSettings::IsCapturing()
{
	return g_captureTarget != 0;
}
