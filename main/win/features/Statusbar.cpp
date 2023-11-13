#include "Statusbar.hpp"

#include <Windows.h>
#include <commctrl.h>

#include "RomBrowser.hpp"
#include "../../winproject/resource.h"
#include "win/Config.hpp"
#include "win/main_win.h"

HWND statusbar_hwnd;

void statusbar_post_text(const std::string& text, const int32_t section)
{
	SendMessage(statusbar_hwnd, SB_SETTEXT, section, (LPARAM)text.c_str());
}

void statusbar_set_mode(const statusbar_mode mode)
{
	if (!statusbar_hwnd) return;

	constexpr int emulatewidths_fpsvis[] = {230, 300, 370, 440, -1};
	constexpr int emulatewidths_fps[] = {230, 300, 370, -1};
	constexpr int emulatewidths[] = {230, 300, -1};
	constexpr int browserwidths[] = {400, -1};
	int parts;

	switch (mode)
	{
	case statusbar_mode::rombrowser:
		SendMessage(statusbar_hwnd, SB_SETPARTS,
		            sizeof(browserwidths) / sizeof(int),
		            (LPARAM)browserwidths);
		SendMessage(statusbar_hwnd, SB_SETTEXT, 0, (LPARAM)statusmsg);
		SendMessage(statusbar_hwnd, SB_SETTEXT, 1, (LPARAM)"");
		break;
	case statusbar_mode::emulating:
		if (Config.show_fps && Config.show_vis_per_second)
		{
			SendMessage(statusbar_hwnd, SB_SETPARTS,
			            sizeof(emulatewidths_fpsvis) / sizeof(int),
			            (LPARAM)emulatewidths_fpsvis);
			SendMessage(statusbar_hwnd, SB_SETTEXT, 1, (LPARAM)""); //rr
			SendMessage(statusbar_hwnd, SB_SETTEXT, 2, (LPARAM)""); //FPS
			SendMessage(statusbar_hwnd, SB_SETTEXT, 3, (LPARAM)""); //VIS
			parts = 5;
		} else if (Config.show_fps)
		{
			SendMessage(statusbar_hwnd, SB_SETPARTS,
			            sizeof(emulatewidths_fps) / sizeof(int),
			            (LPARAM)emulatewidths_fps);
			SendMessage(statusbar_hwnd, SB_SETTEXT, 1, (LPARAM)"");
			SendMessage(statusbar_hwnd, SB_SETTEXT, 2, (LPARAM)"");
			parts = 4;
		} else if (Config.show_vis_per_second)
		{
			SendMessage(statusbar_hwnd, SB_SETPARTS,
			            sizeof(emulatewidths_fps) / sizeof(int),
			            (LPARAM)emulatewidths_fps);
			SendMessage(statusbar_hwnd, SB_SETTEXT, 1, (LPARAM)"");
			SendMessage(statusbar_hwnd, SB_SETTEXT, 2, (LPARAM)"");
			parts = 4;
		} else
		{
			SendMessage(statusbar_hwnd, SB_SETPARTS,
			            sizeof(emulatewidths) / sizeof(int),
			            (LPARAM)emulatewidths);
			SendMessage(statusbar_hwnd, SB_SETTEXT, 1, (LPARAM)"");
			parts = 3;
		}

		SendMessage(statusbar_hwnd, SB_SETTEXT, 0, (LPARAM)statusmsg);
		SendMessage(statusbar_hwnd, SB_SETTEXT, parts - 1, (LPARAM)ROM_HEADER.nom);
		break;
	}
}

void statusbar_create()
{
	// undocumented behaviour of CCS_BOTTOM: it skips applying SBARS_SIZEGRIP in style pre-computation phase
	statusbar_hwnd = CreateWindowEx(0, STATUSCLASSNAME, nullptr,
									WS_CHILD | WS_VISIBLE | CCS_BOTTOM,
									0, 0,
									0, 0,
									main_hwnd, (HMENU)IDC_MAIN_STATUS,
									app_instance, nullptr);

	statusbar_set_mode(emu_launched ? statusbar_mode::emulating : statusbar_mode::rombrowser);
}

void statusbar_set_visibility(const int32_t is_visible)
{
	if (statusbar_hwnd)
	{
		DestroyWindow(statusbar_hwnd);
		statusbar_hwnd = nullptr;
	}

	if (is_visible)
	{
		statusbar_create();
	}

	rombrowser_update_size();
}
