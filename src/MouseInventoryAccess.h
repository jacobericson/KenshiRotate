#pragma once

#include <mygui/MyGUI_Widget.h>

class MouseInventoryAccess
{
public:
	static void FindShadowWidget();
	static void FindMouseInventory();
	static bool GetGrabOffset(int& outX, int& outY);
	static void SetGrabOffset(int x, int y);
	static MyGUI::Widget* GetShadowWidget();
	static bool IsReady();
	static bool HasHeldItem();

private:
	static void* s_mouseInventory;
	static MyGUI::Widget* s_shadowWidget;
};
