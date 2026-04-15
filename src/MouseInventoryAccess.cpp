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

// MouseInventory layout (Kenshi 1.0.65 kenshi_x64.exe). Re-derive via IDA on patch.
static const int OFF_HELD_ITEM     =  48; // Item* — validation probe only, not used for held-item access
static const int OFF_GRAB_X        = 128; // int32 — itemTopLeft.x - mouse.x
static const int OFF_GRAB_Y        = 132; // int32
static const int OFF_SHADOW_WIDGET = 184; // MyGUI::Widget* — set in ctor

// Validate a MouseInventory candidate pointer by checking the shadow widget
// and Item* fields.
static bool ValidateCandidate(void* candidate, MyGUI::Widget* shadowWidget)
{
	if (!candidate)
		return false;

	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery((LPCVOID)(reinterpret_cast<char*>(candidate) + OFF_SHADOW_WIDGET), &mbi, sizeof(mbi)))
		return false;
	if (!(mbi.State & MEM_COMMIT))
		return false;

	__try
	{
		void* shadowField = *reinterpret_cast<void**>(reinterpret_cast<char*>(candidate) + OFF_SHADOW_WIDGET);
		if (shadowField != static_cast<void*>(shadowWidget))
			return false;

		// Secondary validation: Item* should be NULL or point to committed memory
		void* itemField = *reinterpret_cast<void**>(reinterpret_cast<char*>(candidate) + OFF_HELD_ITEM);
		if (itemField != NULL)
		{
			MEMORY_BASIC_INFORMATION mbi2;
			if (!VirtualQuery((LPCVOID)itemField, &mbi2, sizeof(mbi2))
				|| !(mbi2.State & MEM_COMMIT))
				return false;
		}

		return true;
	}
	__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
		? EXCEPTION_EXECUTE_HANDLER
		: EXCEPTION_CONTINUE_SEARCH)
	{
		return false;
	}
}

// Known RVA of the MouseInventory singleton pointer (qword_142131B18).
// From MouseInventory::getSingleton in Kenshi 1.0.65 kenshi_x64.exe.
static const size_t MOUSE_INVENTORY_RVA = 0x2131B18;

// Scan the PE .data section for the MouseInventory singleton pointer.
// Used as a fallback when the known RVA doesn't match.
static void* ScanForMouseInventory(HMODULE base, MyGUI::Widget* shadowWidget)
{
	IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
	IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<char*>(base) + dos->e_lfanew);
	IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

	for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
	{
		if (!(sec->Characteristics & IMAGE_SCN_MEM_WRITE))
			continue;

		char* start = reinterpret_cast<char*>(base) + sec->VirtualAddress;
		size_t size = sec->Misc.VirtualSize;

		for (size_t off = 0; off + sizeof(void*) <= size; off += sizeof(void*))
		{
			void* candidate = *reinterpret_cast<void**>(start + off);
			if (ValidateCandidate(candidate, shadowWidget))
				return candidate;
		}
	}

	return NULL;
}

// Find the MouseInventory singleton. Tries the known RVA first (instant),
// falls back to a full .data section scan if validation fails.
void MouseInventoryAccess::FindMouseInventory()
{
	if (s_mouseInventory || !s_shadowWidget)
		return;

	HMODULE base = GetModuleHandle(NULL);
	if (!base)
		return;

	// Fast path: known RVA from Kenshi 1.0.65
	void* candidate = *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + MOUSE_INVENTORY_RVA);
	if (ValidateCandidate(candidate, s_shadowWidget))
	{
		s_mouseInventory = candidate;
		DebugLog("[KenshiRotate] Found MouseInventory singleton via known RVA");
		return;
	}

	// Slow path: full .data scan (different binary or unexpected layout)
	DebugLog("[KenshiRotate] Known RVA missed, falling back to .data scan");
	s_mouseInventory = ScanForMouseInventory(base, s_shadowWidget);

	if (s_mouseInventory)
		DebugLog("[KenshiRotate] Found MouseInventory singleton via .data scan");
	else
		ErrorLog("[KenshiRotate] Failed to find MouseInventory singleton");
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
