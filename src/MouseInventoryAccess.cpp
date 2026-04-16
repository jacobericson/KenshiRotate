#include "MouseInventoryAccess.h"

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>

#include <Debug.h>

#include <mygui/MyGUI_Gui.h>

void* MouseInventoryAccess::s_mouseInventory = NULL;
MyGUI::Widget* MouseInventoryAccess::s_shadowWidget = NULL;

// Find the MouseInventory shadow widget by enumerating MyGUI root widgets.
// The shadow is the unique root widget with alpha == 0.5 (set in MouseInventory
// constructor with skin "WhiteSkin" on layer "Info").
void MouseInventoryAccess::FindShadowWidget()
{
	if (s_shadowWidget)
		return;

	MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
	if (!gui)
		return;

	MyGUI::EnumeratorWidgetPtr en = gui->getEnumerator();
	while (en.next())
	{
		MyGUI::Widget* w = en.current();
		if (w && w->getAlpha() == 0.5f && w->getParent() == NULL)
		{
			s_shadowWidget = w;
			return;
		}
	}
}

// MouseInventory layout (Kenshi 1.0.65). Offsets validated against both
// shipped editions. The object is 192 bytes; reads must stay in that window.
static const int OFF_HELD_ITEM     =  48; // Item* — validation probe only, not used for held-item access
static const int OFF_GRAB_X        = 128; // int32 — itemTopLeft.x - mouse.x
static const int OFF_GRAB_Y        = 132; // int32
static const int OFF_SHADOW_WIDGET = 184; // MyGUI::Widget* — set in ctor
static const size_t MOUSE_INVENTORY_SIZE = 192;

// RVA of the MouseInventory singleton pointer in each shipped Kenshi edition.
static const size_t RVA_GOG   = 0x212FA88;
static const size_t RVA_STEAM = 0x2131B18;

// Validate a MouseInventory candidate. VirtualQuery covers the full object
// before any dereference, so no SEH is needed: a wrong-edition RVA either
// yields NULL, a non-committed pointer, or a committed region whose +184
// field won't match our shadow widget.
static bool ValidateCandidate(void* candidate, MyGUI::Widget* shadowWidget)
{
	if (!candidate)
		return false;

	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery((LPCVOID)candidate, &mbi, sizeof(mbi)))
		return false;
	if (!(mbi.State & MEM_COMMIT))
		return false;
	if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
		return false;

	// Ensure the whole 192-byte object fits in this committed region.
	size_t regionEnd = (size_t)mbi.BaseAddress + mbi.RegionSize;
	if ((size_t)candidate + MOUSE_INVENTORY_SIZE > regionEnd)
		return false;

	if (*reinterpret_cast<void**>(reinterpret_cast<char*>(candidate) + OFF_SHADOW_WIDGET) != static_cast<void*>(shadowWidget))
		return false;

	// Secondary validation: Item* is NULL or points to committed memory.
	void* itemField = *reinterpret_cast<void**>(reinterpret_cast<char*>(candidate) + OFF_HELD_ITEM);
	if (itemField != NULL)
	{
		MEMORY_BASIC_INFORMATION mbi2;
		if (!VirtualQuery((LPCVOID)itemField, &mbi2, sizeof(mbi2)))
			return false;
		if (!(mbi2.State & MEM_COMMIT))
			return false;
		if (mbi2.Protect & (PAGE_NOACCESS | PAGE_GUARD))
			return false;
	}

	return true;
}

// If no RVA validates, grab-offset correction is disabled but rotation still works.
void MouseInventoryAccess::FindMouseInventory()
{
	if (s_mouseInventory || !s_shadowWidget)
		return;

	HMODULE base = GetModuleHandle(NULL);
	if (!base)
		return;

	static const size_t RVAs[] = { RVA_GOG, RVA_STEAM };
	for (size_t i = 0; i < sizeof(RVAs) / sizeof(RVAs[0]); ++i)
	{
		void* candidate = *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + RVAs[i]);
		if (ValidateCandidate(candidate, s_shadowWidget))
		{
			s_mouseInventory = candidate;
			DebugLog("[KenshiRotate] Found MouseInventory singleton");
			return;
		}
	}

	ErrorLog("[KenshiRotate] Unknown Kenshi build - grab offset correction disabled");
}

bool MouseInventoryAccess::GetGrabOffset(int& outX, int& outY)
{
	if (!s_mouseInventory)
		return false;
	char* base = reinterpret_cast<char*>(s_mouseInventory);
	outX = *reinterpret_cast<int*>(base + OFF_GRAB_X);
	outY = *reinterpret_cast<int*>(base + OFF_GRAB_Y);
	return true;
}

void MouseInventoryAccess::SetGrabOffset(int x, int y)
{
	if (!s_mouseInventory)
		return;
	char* base = reinterpret_cast<char*>(s_mouseInventory);
	*reinterpret_cast<int*>(base + OFF_GRAB_X) = x;
	*reinterpret_cast<int*>(base + OFF_GRAB_Y) = y;
}

MyGUI::Widget* MouseInventoryAccess::GetShadowWidget()
{
	return s_shadowWidget;
}

bool MouseInventoryAccess::IsReady()
{
	return s_mouseInventory != NULL;
}
