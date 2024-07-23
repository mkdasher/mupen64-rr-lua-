/***************************************************************************
						  main_win.cpp  -  description
							 -------------------
	copyright C) 2003    : ShadowPrince (shadow@emulation64.com)
	modifications        : linker (linker@mail.bg)
	mupen64 author       : hacktarux (hacktarux@yahoo.fr)
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "Main.h"


#include <commctrl.h>
#include <filesystem>
#include <Shlwapi.h>
#include <Windows.h>
#include <core/memory/memory.h>
#include <core/memory/pif.h>
#include <core/memory/savestates.h>
#include <core/r4300/Plugin.hpp>
#include <core/r4300/r4300.h>
#include <core/r4300/rom.h>
#include <core/r4300/timers.h>
#include <core/r4300/tracelog.h>
#include <core/r4300/vcr.h>
#include <shared/Config.hpp>
#include <shared/Messenger.h>
#include <shared/helpers/StringHelpers.h>
#include <shared/services/FrontendService.h>
#include <shared/services/LuaService.h>
#include <view/capture/EncodingManager.h>
#include <view/gui/features/ConfigDialog.h>
#include <view/helpers/MathHelpers.h>
#include <view/helpers/StringHelpers.h>
#include <view/helpers/WinHelpers.h>
#include <view/lua/LuaConsole.h>
#include <winproject/resource.h>
#include "Commandline.h"
#include "features/CoreDbg.h"
#include "features/CrashHelper.h"
#include "features/Dispatcher.h"
#include "features/MGECompositor.h"
#include "features/MovieDialog.h"
#include "features/RomBrowser.hpp"
#include "features/Seeker.h"
#include "features/Statusbar.hpp"
#include "features/Cheats.h"
#include "features/Runner.h"
#include "wrapper/PersistentPathDialog.h"

DWORD g_ui_thread_id;
HWND g_hwnd_plug;
UINT g_update_screen_timer;
HWND g_main_hwnd;
HMENU g_main_menu;
HMENU g_recent_roms_menu;
HMENU g_recent_movies_menu;
HMENU g_recent_lua_menu;
HINSTANCE g_app_instance;
std::string g_app_path;

int g_last_wheel_delta = 0;
bool g_paused_before_menu;
bool g_paused_before_focus;
bool g_in_menu_loop;
bool g_vis_since_input_poll_warning_dismissed;
bool g_emu_starting;

/**
 * \brief List of lua environment map keys running before emulation stopped
 */
std::vector<HWND> g_previously_running_luas;

constexpr char WND_CLASS[] = "myWindowClass";

const std::map<Action, int> ACTION_ID_MAP = {
	{Action::FastforwardOn, IDM_FASTFORWARD_ON},
	{Action::FastforwardOff, IDM_FASTFORWARD_OFF},
	{Action::GamesharkOn, IDM_GS_ON},
	{Action::GamesharkOff, IDM_GS_OFF},
	{Action::SpeedDown, IDC_DECREASE_MODIFIER},
	{Action::SpeedUp, IDC_INCREASE_MODIFIER},
	{Action::FrameAdvance, IDM_FRAMEADVANCE},
	{Action::Pause, IDM_PAUSE},
	{Action::ToggleReadOnly, IDM_VCR_READONLY},
	{Action::ToggleMovieLoop, IDM_LOOP_MOVIE},
	{Action::StartMoviePlayback, IDM_START_MOVIE_PLAYBACK},
	{Action::StartMovieRecording, IDM_START_MOVIE_RECORDING},
	{Action::StopMovie, IDM_STOP_MOVIE},
	{Action::TakeScreenshot, IDM_SCREENSHOT},
	{Action::PlayLatestMovie, IDM_PLAY_LATEST_MOVIE},
	{Action::LoadLatestScript, IDM_LOAD_LATEST_LUA},
	{Action::NewLua, IDM_LOAD_LUA},
	{Action::CloseAllLua, IDM_CLOSE_ALL_LUA},
	{Action::LoadRom, IDM_LOAD_ROM},
	{Action::CloseRom, IDM_CLOSE_ROM},
	{Action::ResetRom, IDM_RESET_ROM},
	{Action::LoadLatestRom, IDM_LOAD_LATEST_ROM},
	{Action::Fullscreen, IDM_FULLSCREEN},
	{Action::Settings, IDM_SETTINGS},
	{Action::ToggleStatusbar, IDM_STATUSBAR},
	{Action::RefreshRomBrowser, IDM_REFRESH_ROMBROWSER},
	{Action::OpenSeeker, IDM_SEEKER},
	{Action::OpenRunner, IDM_RUNNER},
	{Action::OpenCheats, IDM_CHEATS},
	{Action::SaveSlot, IDM_SAVE_SLOT},
	{Action::LoadSlot, IDM_LOAD_SLOT},
	{Action::SaveAs, IDM_SAVE_STATE_AS},
	{Action::LoadAs, IDM_LOAD_STATE_AS},
	{Action::SaveSlot1, (ID_SAVE_1 - 1) + 1},
	{Action::SaveSlot2, (ID_SAVE_1 - 1) + 2},
	{Action::SaveSlot3, (ID_SAVE_1 - 1) + 3},
	{Action::SaveSlot4, (ID_SAVE_1 - 1) + 4},
	{Action::SaveSlot5, (ID_SAVE_1 - 1) + 5},
	{Action::SaveSlot6, (ID_SAVE_1 - 1) + 6},
	{Action::SaveSlot7, (ID_SAVE_1 - 1) + 7},
	{Action::SaveSlot8, (ID_SAVE_1 - 1) + 8},
	{Action::SaveSlot9, (ID_SAVE_1 - 1) + 9},
	{Action::SaveSlot10, (ID_SAVE_1 - 1) + 10},
	{Action::LoadSlot1, (ID_LOAD_1 - 1) + 1},
	{Action::LoadSlot2, (ID_LOAD_1 - 1) + 2},
	{Action::LoadSlot3, (ID_LOAD_1 - 1) + 3},
	{Action::LoadSlot4, (ID_LOAD_1 - 1) + 4},
	{Action::LoadSlot5, (ID_LOAD_1 - 1) + 5},
	{Action::LoadSlot6, (ID_LOAD_1 - 1) + 6},
	{Action::LoadSlot7, (ID_LOAD_1 - 1) + 7},
	{Action::LoadSlot8, (ID_LOAD_1 - 1) + 8},
	{Action::LoadSlot9, (ID_LOAD_1 - 1) + 9},
	{Action::LoadSlot10, (ID_LOAD_1 - 1) + 10},
	{Action::SelectSlot1, (IDM_SELECT_1 - 1) + 1},
	{Action::SelectSlot2, (IDM_SELECT_1 - 1) + 2},
	{Action::SelectSlot3, (IDM_SELECT_1 - 1) + 3},
	{Action::SelectSlot4, (IDM_SELECT_1 - 1) + 4},
	{Action::SelectSlot5, (IDM_SELECT_1 - 1) + 5},
	{Action::SelectSlot6, (IDM_SELECT_1 - 1) + 6},
	{Action::SelectSlot7, (IDM_SELECT_1 - 1) + 7},
	{Action::SelectSlot8, (IDM_SELECT_1 - 1) + 8},
	{Action::SelectSlot9, (IDM_SELECT_1 - 1) + 9},
	{Action::SelectSlot10, (IDM_SELECT_1 - 1) + 10},
};

void set_menu_accelerator(int element_id, const char* acc)
{
	char string[256] = {0};
	GetMenuString(GetMenu(g_main_hwnd), element_id, string, std::size(string), MF_BYCOMMAND);

	// make sure there is tab character (accelerator marker)
	char* tab = strrchr(string, '\t');
	if (tab)
		*tab = '\0';
	if (strcmp(acc, ""))
		sprintf(string, "%s\t%s", string, acc);

	ModifyMenu(GetMenu(g_main_hwnd), element_id, MF_BYCOMMAND | MF_STRING, element_id, string);
}

void set_hotkey_menu_accelerators(t_hotkey* hotkey, int menu_item_id)
{
	const std::string hotkey_str = hotkey_to_string(hotkey);
	set_menu_accelerator(menu_item_id, hotkey_str == "(nothing)" ? "" : hotkey_str.c_str());
}

void SetDlgItemHotkeyAndMenu(HWND hwnd, int idc, t_hotkey* hotkey,
                             int menuItemID)
{
	std::string hotkey_str = hotkey_to_string(hotkey);
	SetDlgItemText(hwnd, idc, hotkey_str.c_str());

	set_menu_accelerator(menuItemID,
	                     hotkey_str == "(nothing)" ? "" : hotkey_str.c_str());
}

/**
 * \brief Converts a config action to its respective menu ID
 * \param action The action to convert
 * \returns The converted ID, or 0 if no match is found.
 * \remark In case of some toggle actions, the IDs dont map to a menu item, but only to an identifier which is handled in WM_COMMAND
 */
int config_action_to_menu_id(Action action)
{
	if (!ACTION_ID_MAP.contains(action))
	{
		printf("[View] No menu ID found for action %d\n", static_cast<int>(action));
		return 0;
	}
	return ACTION_ID_MAP.at(action);
}


std::string hotkey_to_string(const t_hotkey* hotkey)
{
	char buf[260] {};
	const int k = hotkey->key;

	if (!hotkey->ctrl && !hotkey->shift && !hotkey->alt && !hotkey->key)
	{
		return "(nothing)";
	}

	if (hotkey->ctrl)
		strcat(buf, "Ctrl ");
	if (hotkey->shift)
		strcat(buf, "Shift ");
	if (hotkey->alt)
		strcat(buf, "Alt ");
	if (k)
	{
		char buf2[64] = {0};
		if ((k >= 0x30 && k <= 0x39) || (k >= 0x41 && k <= 0x5A))
			sprintf(buf2, "%c", static_cast<char>(k));
		else if ((k >= VK_F1 && k <= VK_F24))
			sprintf(buf2, "F%d", k - (VK_F1 - 1));
		else if ((k >= VK_NUMPAD0 && k <= VK_NUMPAD9))
			sprintf(buf2, "Num%d", k - VK_NUMPAD0);
		else
			switch (k)
			{
			case VK_SPACE: strcpy(buf2, "Space");
				break;
			case VK_BACK: strcpy(buf2, "Backspace");
				break;
			case VK_TAB: strcpy(buf2, "Tab");
				break;
			case VK_CLEAR: strcpy(buf2, "Clear");
				break;
			case VK_RETURN: strcpy(buf2, "Enter");
				break;
			case VK_PAUSE: strcpy(buf2, "Pause");
				break;
			case VK_CAPITAL: strcpy(buf2, "Caps");
				break;
			case VK_PRIOR: strcpy(buf2, "PageUp");
				break;
			case VK_NEXT: strcpy(buf2, "PageDn");
				break;
			case VK_END: strcpy(buf2, "End");
				break;
			case VK_HOME: strcpy(buf2, "Home");
				break;
			case VK_LEFT: strcpy(buf2, "Left");
				break;
			case VK_UP: strcpy(buf2, "Up");
				break;
			case VK_RIGHT: strcpy(buf2, "Right");
				break;
			case VK_DOWN: strcpy(buf2, "Down");
				break;
			case VK_SELECT: strcpy(buf2, "Select");
				break;
			case VK_PRINT: strcpy(buf2, "Print");
				break;
			case VK_SNAPSHOT: strcpy(buf2, "PrintScrn");
				break;
			case VK_INSERT: strcpy(buf2, "Insert");
				break;
			case VK_DELETE: strcpy(buf2, "Delete");
				break;
			case VK_HELP: strcpy(buf2, "Help");
				break;
			case VK_MULTIPLY: strcpy(buf2, "Num*");
				break;
			case VK_ADD: strcpy(buf2, "Num+");
				break;
			case VK_SUBTRACT: strcpy(buf2, "Num-");
				break;
			case VK_DECIMAL: strcpy(buf2, "Num.");
				break;
			case VK_DIVIDE: strcpy(buf2, "Num/");
				break;
			case VK_NUMLOCK: strcpy(buf2, "NumLock");
				break;
			case VK_SCROLL: strcpy(buf2, "ScrollLock");
				break;
			case /*VK_OEM_PLUS*/0xBB: strcpy(buf2, "=+");
				break;
			case /*VK_OEM_MINUS*/0xBD: strcpy(buf2, "-_");
				break;
			case /*VK_OEM_COMMA*/0xBC: strcpy(buf2, ",");
				break;
			case /*VK_OEM_PERIOD*/0xBE: strcpy(buf2, ".");
				break;
			case VK_OEM_7: strcpy(buf2, "'\"");
				break;
			case VK_OEM_6: strcpy(buf2, "]}");
				break;
			case VK_OEM_5: strcpy(buf2, "\\|");
				break;
			case VK_OEM_4: strcpy(buf2, "[{");
				break;
			case VK_OEM_3: strcpy(buf2, "`~");
				break;
			case VK_OEM_2: strcpy(buf2, "/?");
				break;
			case VK_OEM_1: strcpy(buf2, ";:");
				break;
			default:
				sprintf(buf2, "(%d)", k);
				break;
			}
		strcat(buf, buf2);
	}
	return std::string(buf);
}

namespace Recent
{
	void build(std::vector<std::string>& vec, int first_menu_id, HMENU parent_menu, bool reset = false)
	{
		for (size_t i = 0; i < vec.size(); i++)
		{
			if (vec[i].empty())
			{
				continue;
			}
			DeleteMenu(g_main_menu, first_menu_id + i, MF_BYCOMMAND);
		}

		if (reset)
		{
			vec.clear();
			return;
		}

		MENUITEMINFO menu_info = {0};
		menu_info.cbSize = sizeof(MENUITEMINFO);
		menu_info.fMask = MIIM_TYPE | MIIM_ID;
		menu_info.fType = MFT_STRING;
		menu_info.fState = MFS_ENABLED;

		for (size_t i = 0; i < vec.size(); i++)
		{
			if (vec[i].empty())
			{
				continue;
			}
			menu_info.dwTypeData = (LPSTR)vec[i].c_str();
			menu_info.cch = strlen(menu_info.dwTypeData);
			menu_info.wID = first_menu_id + i;
			InsertMenuItem(parent_menu, i + 3, TRUE, &menu_info);
		}
	}

	void add(std::vector<std::string>& vec, std::string val, bool frozen, int first_menu_id, HMENU parent_menu)
	{
		if (frozen)
		{
			return;
		}
		if (vec.size() > 5)
		{
			vec.pop_back();
		}
		std::erase(vec, val);
		vec.insert(vec.begin(), val);
		build(vec, first_menu_id, parent_menu);
	}

	std::string element_at(std::vector<std::string> vec, int first_menu_id, int menu_id)
	{
		const int index = menu_id - first_menu_id;
		if (index >= 0 && index < vec.size())
		{
			return vec[index];
		}
		return "";
	}
}

std::string get_screenshots_directory()
{
	if (g_config.is_default_screenshots_directory_used)
	{
		return g_app_path + "screenshots\\";
	}
	return g_config.screenshots_directory;
}

void update_titlebar()
{
	std::string text = MUPEN_VERSION;

	if (g_emu_starting)
	{
		text += " - Starting...";
	}

	if (emu_launched)
	{
		text += std::format(" - {}", reinterpret_cast<char*>(ROM_HEADER.nom));
	}

	if (VCR::get_task() != e_task::idle)
	{
		char movie_filename[MAX_PATH] = {0};
		_splitpath(VCR::get_path().string().c_str(), nullptr, nullptr, movie_filename, nullptr);
		text += std::format(" - {}", movie_filename);
	}

	SetWindowText(g_main_hwnd, text.c_str());
}

#pragma region Change notifications

void on_script_started(std::any data)
{
	auto value = std::any_cast<std::filesystem::path>(data);
	Recent::add(g_config.recent_lua_script_paths, value.string(), g_config.is_recent_scripts_frozen, ID_LUA_RECENT, g_recent_lua_menu);
}

void on_task_changed(std::any data)
{
	auto value = std::any_cast<e_task>(data);
	static auto previous_value = value;

	if (!task_is_recording(value) && task_is_recording(previous_value))
	{
		Statusbar::post("Recording stopped");
	}
	if (!task_is_playback(value) && task_is_playback(previous_value))
	{
		Statusbar::post("Playback stopped");
	}

	if ((task_is_recording(value) && !task_is_recording(previous_value))
		|| (task_is_playback(value) && !task_is_playback(previous_value)) && !VCR::get_path().empty())
	{
		Recent::add(g_config.recent_movie_paths, VCR::get_path().string(), g_config.is_recent_movie_paths_frozen, ID_RECENTMOVIES_FIRST, g_recent_movies_menu);
	}

	update_titlebar();
	SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
	previous_value = value;
}

void on_emu_stopping(std::any)
{
	// Remember all running lua scripts' HWNDs
	for (const auto key : hwnd_lua_map | std::views::keys)
	{
		g_previously_running_luas.push_back(key);
	}
	Dispatcher::invoke(stop_all_scripts);
}

void on_emu_launched_changed(std::any data)
{
	auto value = std::any_cast<bool>(data);
	static auto previous_value = value;

	if (value)
	{
		SetWindowLong(g_main_hwnd, GWL_STYLE,
		              GetWindowLong(g_main_hwnd, GWL_STYLE) & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
	} else
	{
		SetWindowLong(g_main_hwnd, GWL_STYLE,
		              GetWindowLong(g_main_hwnd, GWL_STYLE) | WS_THICKFRAME | WS_MAXIMIZEBOX);
	}


	update_titlebar();
	// Some menu items, like movie ones, depend on both this and vcr task
	on_task_changed(VCR::get_task());

	// Reset and restore view stuff when emulation starts
	if (value)
	{
		Recent::add(g_config.recent_rom_paths, get_rom_path().string(), g_config.is_recent_rom_paths_frozen, ID_RECENTROMS_FIRST, g_recent_roms_menu);
		g_vis_since_input_poll_warning_dismissed = false;
		Dispatcher::invoke([]
		{
			for (const HWND hwnd : g_previously_running_luas)
			{
				// click start button
				SendMessage(hwnd, WM_COMMAND,
				            MAKEWPARAM(IDC_BUTTON_LUASTATE, BN_CLICKED),
				            (LPARAM)GetDlgItem(hwnd, IDC_BUTTON_LUASTATE));
			}

			g_previously_running_luas.clear();
		});
	}

	if (!value && previous_value)
	{
		printf("[View] Restoring window size to %dx%d...\n", g_config.window_width, g_config.window_height);
		SetWindowPos(g_main_hwnd, nullptr, 0, 0, g_config.window_width, g_config.window_height, SWP_NOMOVE);
	}

	SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
	previous_value = value;
}

void on_capturing_changed(std::any data)
{
	auto value = std::any_cast<bool>(data);

	if (value)
	{
		SetWindowLong(g_main_hwnd, GWL_STYLE, GetWindowLong(g_main_hwnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
	} else
	{
		SetWindowPos(g_main_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		SetWindowLong(g_main_hwnd, GWL_STYLE, GetWindowLong(g_main_hwnd, GWL_STYLE) | WS_MINIMIZEBOX);
	}

	update_titlebar();
	SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
}

void on_speed_modifier_changed(std::any data)
{
	auto value = std::any_cast<int32_t>(data);
	Statusbar::post(std::format("Speed limit: {}%", value));
}

void on_emu_paused_changed(std::any data)
{
	frame_changed = true;
	SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
}

void on_vis_since_input_poll_exceeded(std::any)
{
	if (g_vis_since_input_poll_warning_dismissed)
	{
		return;
	}

	if (g_config.silent_mode || FrontendService::show_ask_dialog(
		"An unusual execution pattern was detected. Continuing might leave the emulator in an unusable state.\r\nWould you like to terminate emulation?",
		"Warning", true))
	{
		std::thread([] { vr_close_rom(); }).detach();
	}
	g_vis_since_input_poll_warning_dismissed = true;
}

void on_core_result(std::any data)
{
	auto value = std::any_cast<Core::Result>(data);

	switch (value)
	{
	case Core::Result::NoMatchingRom:
		FrontendService::show_error("The ROM couldn't be loaded.\r\nCouldn't find an appropriate ROM.");
		break;
	case Core::Result::PluginError:
		if (FrontendService::show_ask_dialog("Plugins couldn't be loaded.\r\nDo you want to change the selected plugins?"))
		{
			SendMessage(g_main_hwnd, WM_COMMAND, MAKEWPARAM(IDM_SETTINGS, 0), 0);
		}
		break;
	case Core::Result::RomInvalid:
		FrontendService::show_error("The ROM couldn't be loaded.\r\nVerify that the ROM is a valid N64 ROM.");
		break;
	case Core::Result::FileOpenFailed:
		FrontendService::show_error("Failed to open streams to core files.\r\nVerify that Mupen is allowed disk access.");
		break;
	default:
		break;
	}
}

void on_movie_loop_changed(std::any data)
{
	auto value = std::any_cast<bool>(data);
	Statusbar::post(value
		                ? "Movies restart after ending"
		                : "Movies stop after ending");
	SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
}

void on_readonly_changed(std::any data)
{
	auto value = std::any_cast<bool>(data);
	Statusbar::post(value
		                ? "Read-only"
		                : "Read/write");
	SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
}

void on_fullscreen_changed(std::any data)
{
	auto value = std::any_cast<bool>(data);
	ShowCursor(!value);
	SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
}

void on_config_loaded(std::any)
{
	for (auto hotkey : g_config_hotkeys)
	{
		// Only set accelerator if hotkey has a down command and the command is valid menu item identifier
		auto down_cmd = config_action_to_menu_id(hotkey->down_cmd);
		auto state = GetMenuState(GetMenu(g_main_hwnd), down_cmd, MF_BYCOMMAND);
		if (down_cmd && state != -1)
		{
			set_hotkey_menu_accelerators(hotkey, down_cmd);
		}
	}
	Rombrowser::build();
}

BetterEmulationLock::BetterEmulationLock()
{
	if (g_in_menu_loop)
	{
		was_paused = g_paused_before_menu;

		// This fires before WM_EXITMENULOOP (which restores the paused_before_menu state), so we need to trick it...
		g_paused_before_menu = true;
	} else
	{
		was_paused = emu_paused;
		pause_emu();
	}
}

BetterEmulationLock::~BetterEmulationLock()
{
	if (was_paused)
	{
		pause_emu();
	} else
	{
		resume_emu();
	}
}

t_window_info get_window_info()
{
	t_window_info info;

	RECT client_rect = {};
	GetClientRect(g_main_hwnd, &client_rect);

	// full client dimensions including statusbar
	info.width = client_rect.right - client_rect.left;
	info.height = client_rect.bottom - client_rect.top;

	RECT statusbar_rect = {0};
	if (Statusbar::hwnd())
		GetClientRect(Statusbar::hwnd(), &statusbar_rect);
	info.statusbar_height = statusbar_rect.bottom - statusbar_rect.top;

	//subtract size of toolbar and statusbar from buffer dimensions
	//if video plugin knows about this, whole game screen should be captured. Most of the plugins do.
	info.height -= info.statusbar_height;
	return info;
}

#pragma endregion

bool confirm_user_exit()
{
	if (g_config.silent_mode)
	{
		return true;
	}

	int res = 0;
	int warnings = 0;

	std::string final_message;
	if (VCR::get_task() == e_task::recording)
	{
		final_message.append("Movie recording ");
		warnings++;
	}
	if (EncodingManager::is_capturing())
	{
		if (warnings > 0) { final_message.append(","); }
		final_message.append(" AVI capture ");
		warnings++;
	}
	if (tracelog::active())
	{
		if (warnings > 0) { final_message.append(","); }
		final_message.append(" Trace logging ");
		warnings++;
	}
	final_message.
		append("is running. Are you sure you want to stop emulation?");
	if (warnings > 0)
		res = MessageBox(g_main_hwnd, final_message.c_str(), "Stop emulation?",
						 MB_YESNO | MB_ICONWARNING);

	return res == IDYES || warnings == 0;
}

bool is_on_gui_thread()
{
	return GetCurrentThreadId() == g_ui_thread_id;
}

void ClearButtons()
{
	BUTTONS zero = {0};
	for (int i = 0; i < 4; i++)
	{
		setKeys(i, zero);
	}
}

std::string get_app_full_path()
{
	char ret[MAX_PATH] = {0};
	char drive[_MAX_DRIVE], dirn[_MAX_DIR];
	char path_buffer[_MAX_DIR];
	GetModuleFileName(nullptr, path_buffer, sizeof(path_buffer));
	_splitpath(path_buffer, drive, dirn, nullptr, nullptr);
	strcpy(ret, drive);
	strcat(ret, dirn);

	return ret;
}

void open_console()
{
	AllocConsole();
	FILE* f = 0;
	freopen_s(&f, "CONIN$", "r", stdin);
	freopen_s(&f, "CONOUT$", "w", stdout);
	freopen_s(&f, "CONOUT$", "w", stderr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	char path_buffer[_MAX_PATH];

	switch (Message)
	{
	case WM_DROPFILES:
		{
			auto drop = (HDROP)wParam;
			char fname[MAX_PATH] = {0};
			DragQueryFile(drop, 0, fname, sizeof(fname));

			std::filesystem::path path = fname;
			std::string extension = to_lower(path.extension().string());

			if (extension == ".n64" || extension == ".z64" || extension == ".v64" || extension == ".rom")
			{
				std::thread([path] { vr_start_rom(path); }).detach();
			} else if (extension == ".m64")
			{
				g_config.vcr_readonly = true;
				Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.vcr_readonly);
				std::thread([fname] { VCR::start_playback(fname); }).detach();
			} else if (extension == ".st" || extension == ".savestate")
			{
				if (!emu_launched) break;
				savestates_do_file(fname, e_st_job::load);
			} else if (extension == ".lua")
			{
				lua_create_and_run(path.string().c_str());
			}
			break;
		}
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			BOOL hit = FALSE;
			for (t_hotkey* hotkey : g_config_hotkeys)
			{
				if ((int)wParam == hotkey->key)
				{
					if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) == hotkey->
						shift
						&& ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) ==
						hotkey->ctrl
						&& ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) == hotkey->
						alt)
					{
						auto down_cmd = config_action_to_menu_id(hotkey->down_cmd);
						// We only want to send it if the corresponding menu item exists and is enabled
						auto state = GetMenuState(g_main_menu, down_cmd, MF_BYCOMMAND);
						if (state != -1 && (state & MF_DISABLED || state & MF_GRAYED))
						{
							printf("Dismissed %s (%d)\n", hotkey->identifier.c_str(), down_cmd);
							continue;
						}
						printf("Sent down %s (%d)\n", hotkey->identifier.c_str(), down_cmd);
						SendMessage(g_main_hwnd, WM_COMMAND, down_cmd, 0);
						hit = TRUE;
					}
				}
			}

			if (keyDown && emu_launched)
				keyDown(wParam, lParam);
			if (!hit)
				return DefWindowProc(hwnd, Message, wParam, lParam);
			break;
		}
	case WM_SYSKEYUP:
	case WM_KEYUP:
		{
			BOOL hit = FALSE;
			for (t_hotkey* hotkey : g_config_hotkeys)
			{
				if (hotkey->up_cmd == Action::None)
				{
					continue;
				}

				if ((int)wParam == hotkey->key)
				{
					if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) == hotkey->
						shift
						&& ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) ==
						hotkey->ctrl
						&& ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) == hotkey->
						alt)
					{
						auto up_cmd = config_action_to_menu_id(hotkey->up_cmd);
						// We only want to send it if the corresponding menu item exists and is enabled
						auto state = GetMenuState(g_main_menu, up_cmd, MF_BYCOMMAND);
						if (state != -1 && (state & MF_DISABLED || state & MF_GRAYED))
						{
							printf("Dismissed %s (%d)\n", hotkey->identifier.c_str(), up_cmd);
							continue;
						}
						printf("Sent up %s (%d)\n", hotkey->identifier.c_str(), up_cmd);
						SendMessage(g_main_hwnd, WM_COMMAND, up_cmd, 0);
						hit = TRUE;
					}
				}
			}

			if (keyUp && emu_launched)
				keyUp(wParam, lParam);
			if (!hit)
				return DefWindowProc(hwnd, Message, wParam, lParam);
			break;
		}
	case WM_MOUSEWHEEL:
		g_last_wheel_delta = GET_WHEEL_DELTA_WPARAM(wParam);

	// https://github.com/mkdasher/mupen64-rr-lua-/issues/190
		LuaService::call_window_message(hwnd, Message, wParam, lParam);
		break;
	case WM_NOTIFY:
		{
			LPNMHDR l_header = (LPNMHDR)lParam;

			if (wParam == IDC_ROMLIST)
			{
				Rombrowser::notify(lParam);
			}
			return 0;
		}
	case WM_MOVE:
		{
			if (core_executing)
			{
				moveScreen((int)wParam, lParam);
			}

			RECT rect = {0};
			GetWindowRect(g_main_hwnd, &rect);
			g_config.window_x = rect.left;
			g_config.window_y = rect.top;
			break;
		}
	case WM_SIZE:
		{
			SendMessage(Statusbar::hwnd(), WM_SIZE, 0, 0);
			RECT rect{};
			GetClientRect(g_main_hwnd, &rect);
			Messenger::broadcast(Messenger::Message::SizeChanged, rect);

			if (core_executing || emu_launched)
			{
				// We don't need to remember the dimensions set by gfx plugin
				break;
			}

			// Window creation expects the size with nc area, so it's easiest to just use the window rect here
			GetWindowRect(hwnd, &rect);
			g_config.window_width = rect.right - rect.left;
			g_config.window_height = rect.bottom - rect.top;

			break;
		}
	case WM_FOCUS_MAIN_WINDOW:
		SetFocus(g_main_hwnd);
		break;
	case WM_EXECUTE_DISPATCHER:
		Dispatcher::execute();
		break;
	case WM_CREATE:
		g_main_menu = GetMenu(hwnd);
		GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
		g_update_screen_timer = SetTimer(hwnd, NULL, (uint32_t)(1000 / get_primary_monitor_refresh_rate()), NULL);
		MGECompositor::create(hwnd);
		return TRUE;
	case WM_DESTROY:
		save_config();
		KillTimer(g_main_hwnd, g_update_screen_timer);
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		if (confirm_user_exit())
		{
			close_all_scripts();
			std::thread([]
			{
				vr_close_rom(true);
				Dispatcher::invoke([]
				{
					DestroyWindow(g_main_hwnd);
				});
			}).detach();
			break;
		}
		return 0;
	case WM_TIMER:
		{
			static std::chrono::high_resolution_clock::time_point last_statusbar_update = std::chrono::high_resolution_clock::now();
			std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();

			if (frame_changed)
			{
				Statusbar::post(VCR::get_input_text(), Statusbar::Section::Input);
				Statusbar::post(VCR::get_status_text(), Statusbar::Section::VCR);

				frame_changed = false;
			}

			// NOTE: We don't invalidate the controls in their WM_PAINT, since that generates too many WM_PAINTs and fills up the message queue
			// Instead, we invalidate them driven by not so high-freq heartbeat like previously.
			for (auto map : hwnd_lua_map)
			{
				map.second->invalidate_visuals();
			}

			// We throttle FPS and VI/s visual updates to 1 per second, so no unstable values are displayed
			if (time - last_statusbar_update > std::chrono::seconds(1))
			{
				timepoints_mutex.lock();
				Statusbar::post(std::format("FPS: {:.1f}", get_rate_per_second_from_deltas(g_frame_deltas)), Statusbar::Section::FPS);
				Statusbar::post(std::format("VI/s: {:.1f}", get_rate_per_second_from_deltas(g_vi_deltas)), Statusbar::Section::VIs);
				timepoints_mutex.unlock();
				last_statusbar_update = time;
			}
		}
		break;
	case WM_WINDOWPOSCHANGING: //allow gfx plugin to set arbitrary size
		return 0;
	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
			lpMMI->ptMinTrackSize.x = MIN_WINDOW_W;
			lpMMI->ptMinTrackSize.y = MIN_WINDOW_H;
			// this might break small res with gfx plugin!!!
		}
		break;
	case WM_INITMENU:
		{
			EnableMenuItem(g_main_menu, IDM_CLOSE_ROM, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_RESET_ROM, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_PAUSE, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_FRAMEADVANCE, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_SCREENSHOT, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_SAVE_SLOT, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_LOAD_SLOT, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_SAVE_STATE_AS, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_LOAD_STATE_AS, emu_launched ? MF_ENABLED : MF_GRAYED);
			for (int i = IDM_SELECT_1; i < IDM_SELECT_10; ++i)
			{
				EnableMenuItem(g_main_menu, i, emu_launched ? MF_ENABLED : MF_GRAYED);
			}
			EnableMenuItem(g_main_menu, IDM_FULLSCREEN, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_STATUSBAR, !emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_START_MOVIE_RECORDING, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_STOP_MOVIE, (VCR::get_task() != e_task::idle) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_TRACELOG, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_COREDBG, (emu_launched && g_config.core_type == 2) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_SEEKER, (emu_launched && task_is_playback(VCR::get_task())) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_CHEATS, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_START_CAPTURE, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_START_CAPTURE_PRESET, emu_launched ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(g_main_menu, IDM_STOP_CAPTURE, (emu_launched && EncodingManager::is_capturing()) ? MF_ENABLED : MF_GRAYED);

			CheckMenuItem(g_main_menu, IDM_STATUSBAR, g_config.is_statusbar_enabled ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_main_menu, IDM_FREEZE_RECENT_ROMS, g_config.is_recent_rom_paths_frozen ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_main_menu, IDM_FREEZE_RECENT_MOVIES, g_config.is_recent_movie_paths_frozen ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_main_menu, IDM_FREEZE_RECENT_LUA, g_config.is_recent_scripts_frozen ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_main_menu, IDM_LOOP_MOVIE, g_config.is_movie_loop_enabled ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_main_menu, IDM_VCR_READONLY, g_config.vcr_readonly ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_main_menu, IDM_FULLSCREEN, vr_is_fullscreen() ? MF_CHECKED : MF_UNCHECKED);

			for (int i = IDM_SELECT_1; i < IDM_SELECT_10; ++i)
			{
				CheckMenuItem(g_main_menu, i, MF_UNCHECKED);
			}
			CheckMenuItem(g_main_menu, IDM_SELECT_1 + savestates_get_slot(), MF_CHECKED);
		}
		break;
	case WM_ENTERMENULOOP:
		g_in_menu_loop = true;
		g_paused_before_menu = emu_paused;
		pause_emu();
		break;

	case WM_EXITMENULOOP:
		// This message is sent when we escape the blocking menu loop, including situations where the clicked menu spawns a dialog.
		// In those situations, we would unpause the game here (since this message is sent first), and then pause it again in the menu item message handler.
		// It's almost guaranteed that a game frame will pass between those messages, so we need to wait a bit on another thread before unpausing.
		std::thread([]
		{
			Sleep(60);
			g_in_menu_loop = false;
			if (g_paused_before_menu)
			{
				pause_emu();
			} else
			{
				resume_emu();
			}
		}).detach();
		break;
	case WM_ACTIVATE:
		UpdateWindow(hwnd);

		if (!g_config.is_unfocused_pause_enabled)
		{
			break;
		}

		switch (LOWORD(wParam))
		{
		case WA_ACTIVE:
		case WA_CLICKACTIVE:
			if (!g_paused_before_focus)
			{
				resume_emu();
			}
			break;

		case WA_INACTIVE:
			g_paused_before_focus = emu_paused;
			pause_emu();
			break;
		default:
			break;
		}
		break;
	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case IDM_VIDEO_SETTINGS:
			case IDM_INPUT_SETTINGS:
			case IDM_AUDIO_SETTINGS:
			case IDM_RSP_SETTINGS:
				{
					// FIXME: Is it safe to load multiple plugin instances? This assumes they cooperate and dont overwrite eachother's files...
					// It does seem fine tho, since the config dialog is modal and core is paused
					g_hwnd_plug = g_main_hwnd;
					std::optional<std::unique_ptr<Plugin>> plugin;

					switch (LOWORD(wParam))
					{
					case IDM_VIDEO_SETTINGS:
						plugin = Plugin::create(g_config.selected_video_plugin);
						break;
					case IDM_INPUT_SETTINGS:
						plugin = Plugin::create(g_config.selected_input_plugin);
						break;
					case IDM_AUDIO_SETTINGS:
						plugin = Plugin::create(g_config.selected_audio_plugin);
						break;
					case IDM_RSP_SETTINGS:
						plugin = Plugin::create(g_config.selected_rsp_plugin);
						break;
					}

					if (plugin.has_value())
					{
						plugin.value()->config();
					}
				}
				break;
			case IDM_LOAD_LUA:
				{
					lua_create();
				}
				break;

			case IDM_CLOSE_ALL_LUA:
				close_all_scripts();
				break;
			case IDM_TRACELOG:
				{
					if (tracelog::active())
					{
						tracelog::stop();
						ModifyMenu(g_main_menu, IDM_TRACELOG, MF_BYCOMMAND | MF_STRING, IDM_TRACELOG, "Start &Trace Logger...");
						break;
					}

					auto path = show_persistent_save_dialog("s_tracelog", g_main_hwnd, L"*.log");

					if (path.empty())
					{
						break;
					}

					auto result = MessageBox(g_main_hwnd, "Should the trace log be generated in a binary format?", "Trace Logger",
					                         MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);

					tracelog::start(wstring_to_string(path).c_str(), result == IDYES);
					ModifyMenu(g_main_menu, IDM_TRACELOG, MF_BYCOMMAND | MF_STRING, IDM_TRACELOG, "Stop &Trace Logger");
				}
				break;
			case IDM_CLOSE_ROM:
				if (!confirm_user_exit())
					break;
				std::thread([] { vr_close_rom(); }).detach();
				break;
			case IDM_FASTFORWARD_ON:
				fast_forward = 1;
				break;
			case IDM_FASTFORWARD_OFF:
				fast_forward = 0;
				break;
			case IDM_GS_ON:
				set_gs_button(true);
				break;
			case IDM_GS_OFF:
				set_gs_button(false);
				break;
			case IDM_PAUSE:
				{
					// FIXME: While this is a beautiful and clean solution, there has to be a better way to handle this
					// We're too close to release to care tho
					if (g_in_menu_loop)
					{
						if (g_paused_before_menu)
						{
							resume_emu();
							g_paused_before_menu = false;
							break;
						}
						g_paused_before_menu = true;
						pause_emu();
					} else
					{
						if (emu_paused)
						{
							resume_emu();
							break;
						}
						pause_emu();
					}

					break;
				}

			case IDM_FRAMEADVANCE:
				{
					frame_advancing = 1;
					resume_emu();
				}
				break;

			case IDM_VCR_READONLY:
				g_config.vcr_readonly ^= true;
				Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.vcr_readonly);
				break;

			case IDM_LOOP_MOVIE:
				g_config.is_movie_loop_enabled ^= true;
				Messenger::broadcast(Messenger::Message::MovieLoopChanged, (bool)g_config.is_movie_loop_enabled);
				break;


			case EMU_PLAY:
				resume_emu();
				break;

			case IDM_RESET_ROM:
				if (g_config.is_reset_recording_enabled && VCR::get_task() == e_task::recording)
				{
					Messenger::broadcast(Messenger::Message::ResetRequested, nullptr);
					break;
				}

				if (!confirm_user_exit())
					break;

				std::thread([] { vr_reset_rom(); }).detach();
				break;

			case IDM_SETTINGS:
				{
					BetterEmulationLock lock;
					configdialog_show();
				}
				break;
			case IDM_ABOUT:
				{
					BetterEmulationLock lock;
					configdialog_about();
				}
				break;
			case IDM_COREDBG:
				CoreDbg::start();
				break;
			case IDM_SEEKER:
				{
					BetterEmulationLock lock;
					Seeker::show();
				}
				break;
			case IDM_RUNNER:
				{
					BetterEmulationLock lock;
					Runner::show();
				}
				break;
			case IDM_CHEATS:
				{
					BetterEmulationLock lock;
					Cheats::show();
				}
				break;
			case IDM_RAMSTART:
				{
					BetterEmulationLock lock;

					char ram_start[20] = {0};
					sprintf(ram_start, "0x%p", static_cast<void*>(rdram));

					char proc_name[MAX_PATH] = {0};
					GetModuleFileName(NULL, proc_name, MAX_PATH);
					_splitpath(proc_name, 0, 0, proc_name, 0);

					char stroop_c[1024] = {0};
					sprintf(stroop_c,
					        "<Emulator name=\"Mupen 5.0 RR\" processName=\"%s\" ramStart=\"%s\" endianness=\"little\"/>",
					        proc_name, ram_start);

					static const auto ramstart_str = "The RAM start is {}.\r\nDo you want to copy the generated STROOP config line to your clipboard?";


					const auto stroop_str = std::string(stroop_c);
					if (MessageBox(g_main_hwnd,
					               std::format(ramstart_str, ram_start).c_str(),
					               "STROOP",
					               MB_ICONINFORMATION | MB_TASKMODAL |
					               MB_YESNO) == IDYES)
					{
						copy_to_clipboard(g_main_hwnd, stroop_str);
					}
					break;
				}
			case IDM_STATS:
				{
					BetterEmulationLock lock;

					auto str = std::format(L"Total playtime: {}\r\nTotal rerecords: {}", string_to_wstring(format_duration(g_config.total_frames / 30)),
					                       g_config.total_rerecords);

					MessageBoxW(g_main_hwnd,
					            str.c_str(),
					            L"Statistics",
					            MB_ICONINFORMATION);
					break;
				}
			case IDM_CONSOLE:
				open_console();
				break;
			case IDM_LOAD_ROM:
				{
					BetterEmulationLock lock;

					const auto path = show_persistent_open_dialog("o_rom", g_main_hwnd, L"*.n64;*.z64;*.v64;*.rom;*.bin;*.zip;*.usa;*.eur;*.jap");

					if (!path.empty())
					{
						std::thread([path] { vr_start_rom(path); }).detach();
					}
				}
				break;
			case IDM_EXIT:
				DestroyWindow(g_main_hwnd);
				break;
			case IDM_FULLSCREEN:
				toggle_fullscreen_mode();
				break;
			case IDM_REFRESH_ROMBROWSER:
				if (!emu_launched)
				{
					Rombrowser::build();
				}
				break;
			case IDM_SAVE_SLOT:
				savestates_do_slot(-1, e_st_job::save);
				break;
			case IDM_SAVE_STATE_AS:
				{
					BetterEmulationLock lock;

					auto path = show_persistent_save_dialog("s_savestate", hwnd, L"*.st;*.savestate");
					if (path.empty())
					{
						break;
					}

					savestates_do_file(path, e_st_job::save);
				}
				break;
			case IDM_LOAD_SLOT:
				savestates_do_slot(-1, e_st_job::load);
				break;
			case IDM_LOAD_STATE_AS:
				{
					BetterEmulationLock lock;

					auto path = show_persistent_open_dialog("o_state", hwnd,
					                                        L"*.st;*.savestate;*.st0;*.st1;*.st2;*.st3;*.st4;*.st5;*.st6;*.st7;*.st8;*.st9,*.st10");
					if (path.empty())
					{
						break;
					}

					savestates_do_file(path, e_st_job::load);
				}
				break;
			case IDM_START_MOVIE_RECORDING:
				{
					BetterEmulationLock lock;

					auto result = MovieDialog::show(false);

					if (result.path.empty())
					{
						break;
					}

					if (VCR::start_record(result.path, result.start_flag, result.author, result.description) != VCR::Result::Ok)
					{
						FrontendService::show_warning(std::format("Couldn't start recording of {}", result.path.string()).c_str(), nullptr);
						break;
					}

					g_config.last_movie_author = result.author;

					Statusbar::post("Recording replay");
				}
				break;
			case IDM_START_MOVIE_PLAYBACK:
				{
					BetterEmulationLock lock;

					auto result = MovieDialog::show(true);

					if (result.path.empty())
					{
						break;
					}

					VCR::replace_author_info(result.path, result.author, result.description);

					g_config.pause_at_frame = result.pause_at;
					g_config.pause_at_last_frame = result.pause_at_last;

					std::thread([result] { VCR::start_playback(result.path); }).detach();
				}
				break;
			case IDM_STOP_MOVIE:
				VCR::stop_all();
				ClearButtons();
				break;
			case IDM_START_CAPTURE_PRESET:
			case IDM_START_CAPTURE:
				if (emu_launched)
				{
					BetterEmulationLock lock;

					auto path = show_persistent_save_dialog("s_capture", hwnd, L"*.avi");
					if (path.empty())
					{
						break;
					}

					//bool vfw = MessageBox(mainHWND, "Use VFW for capturing?", "Capture", MB_YESNO | MB_ICONQUESTION) == IDYES;
					//auto container = vfw ? EncodingManager::EncoderType::VFW : EncodingManager::EncoderType::FFmpeg;
					bool ask_preset = LOWORD(wParam) == IDM_START_CAPTURE;

					// pass false to startCapture when "last preset" option was choosen
					if (EncodingManager::start_capture(
						wstring_to_string(path).c_str(),
						EncodingManager::EncoderType::VFW,
						ask_preset))
					{
						Statusbar::post("Capture started...");
					}
				}

				break;
			case IDM_STOP_CAPTURE:
				EncodingManager::stop_capture();
				Statusbar::post("Capture stopped");
				break;
			case IDM_SCREENSHOT:
				CaptureScreen(const_cast<char*>(get_screenshots_directory().c_str()));
				break;
			case IDM_RESET_RECENT_ROMS:
				Recent::build(g_config.recent_rom_paths, ID_RECENTROMS_FIRST, g_recent_roms_menu, true);
				break;
			case IDM_RESET_RECENT_MOVIES:
				Recent::build(g_config.recent_movie_paths, ID_RECENTMOVIES_FIRST, g_recent_movies_menu, true);
				break;
			case IDM_RESET_RECENT_LUA:
				Recent::build(g_config.recent_lua_script_paths, ID_LUA_RECENT, g_recent_lua_menu, true);
				break;
			case IDM_FREEZE_RECENT_ROMS:
				g_config.is_recent_rom_paths_frozen ^= true;
				break;
			case IDM_FREEZE_RECENT_MOVIES:
				g_config.is_recent_movie_paths_frozen ^= true;
				break;
			case IDM_FREEZE_RECENT_LUA:
				g_config.is_recent_scripts_frozen ^= true;
				break;
			case IDM_LOAD_LATEST_LUA:
				SendMessage(g_main_hwnd, WM_COMMAND, MAKEWPARAM(ID_LUA_RECENT, 0), 0);
				break;
			case IDM_LOAD_LATEST_ROM:
				SendMessage(g_main_hwnd, WM_COMMAND, MAKEWPARAM(ID_RECENTROMS_FIRST, 0), 0);
				break;
			case IDM_PLAY_LATEST_MOVIE:
				SendMessage(g_main_hwnd, WM_COMMAND, MAKEWPARAM(ID_RECENTMOVIES_FIRST, 0), 0);
				break;
			case IDM_STATUSBAR:
				g_config.is_statusbar_enabled ^= true;
				Messenger::broadcast(Messenger::Message::StatusbarVisibilityChanged, (bool)g_config.is_statusbar_enabled);
				break;
			case IDC_DECREASE_MODIFIER:
				g_config.fps_modifier = clamp(g_config.fps_modifier - 25, 25, 1000);
				timer_init(g_config.fps_modifier, &ROM_HEADER);
				break;
			case IDC_INCREASE_MODIFIER:
				g_config.fps_modifier = clamp(g_config.fps_modifier + 25, 25, 1000);
				timer_init(g_config.fps_modifier, &ROM_HEADER);
				break;
			case IDC_RESET_MODIFIER:
				g_config.fps_modifier = 100;
				timer_init(g_config.fps_modifier, &ROM_HEADER);
				break;
			default:
				if (LOWORD(wParam) >= IDM_SELECT_1 && LOWORD(wParam)
					<= IDM_SELECT_10)
				{
					auto slot = LOWORD(wParam) - IDM_SELECT_1;
					savestates_set_slot(slot);
				} else if (LOWORD(wParam) >= ID_SAVE_1 && LOWORD(wParam) <=
					ID_SAVE_10)
				{
					auto slot = LOWORD(wParam) - ID_SAVE_1;
					// if emu is paused and no console state is changing, we can safely perform st op instantly
					savestates_do_slot(slot, e_st_job::save);
				} else if (LOWORD(wParam) >= ID_LOAD_1 && LOWORD(wParam) <=
					ID_LOAD_10)
				{
					auto slot = LOWORD(wParam) - ID_LOAD_1;
					savestates_do_slot(slot, e_st_job::load);
				} else if (LOWORD(wParam) >= ID_RECENTROMS_FIRST &&
					LOWORD(wParam) < (ID_RECENTROMS_FIRST + g_config.
					                                        recent_rom_paths.size()))
				{
					auto path = Recent::element_at(g_config.recent_rom_paths, ID_RECENTROMS_FIRST, LOWORD(wParam));
					if (path.empty())
						break;

					std::thread([path] { vr_start_rom(path); }).detach();
				} else if (LOWORD(wParam) >= ID_RECENTMOVIES_FIRST &&
					LOWORD(wParam) < (ID_RECENTMOVIES_FIRST + g_config.
					                                          recent_movie_paths.size()))
				{
					auto path = Recent::element_at(g_config.recent_movie_paths, ID_RECENTMOVIES_FIRST, LOWORD(wParam));
					if (path.empty())
						break;

					g_config.vcr_readonly = true;
					Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.vcr_readonly);
					std::thread([path] { VCR::start_playback(path); }).detach();
				} else if (LOWORD(wParam) >= ID_LUA_RECENT && LOWORD(wParam) < (
					ID_LUA_RECENT + g_config.recent_lua_script_paths.size()))
				{
					auto path = Recent::element_at(g_config.recent_lua_script_paths, ID_LUA_RECENT, LOWORD(wParam));
					if (path.empty())
						break;

					lua_create_and_run(path.c_str());
				}
				break;
			}
		}
		break;
	default:
		return DefWindowProc(hwnd, Message, wParam, lParam);
	}

	return TRUE;
}

// kaboom
LONG WINAPI ExceptionReleaseTarget(_EXCEPTION_POINTERS* ExceptionInfo)
{
	// Always generate crash log first, because we'll close when modeless
	auto crash_log = CrashHelper::generate_log(ExceptionInfo);

	FILE* f = fopen("crash.log", "a+");
	fputs(crash_log.c_str(), f);
	fclose(f);

	if (g_config.silent_mode)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}

	bool is_continuable = !(ExceptionInfo->ExceptionRecord->ExceptionFlags &
		EXCEPTION_NONCONTINUABLE);

	int result = 0;

	if (is_continuable)
	{
		TaskDialog(g_main_hwnd, g_app_instance, L"Error",
		           L"An error has occured", L"A crash log has been automatically generated. You can choose to continue program execution.",
		           TDCBF_RETRY_BUTTON | TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, &result);
	} else
	{
		TaskDialog(g_main_hwnd, g_app_instance, L"Error",
		           L"An error has occured", L"A crash log has been automatically generated. The program will now exit.", TDCBF_CLOSE_BUTTON, TD_ERROR_ICON,
		           &result);
	}

	if (result == IDCLOSE)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	g_ui_thread_id = GetCurrentThreadId();

#ifdef _DEBUG
	open_console();
#endif

	Messenger::init();

	g_app_path = get_app_full_path();

	// CWD is sometimes messed up when running from CLI, so we fix it here
	SetCurrentDirectory(g_app_path.c_str());

	g_app_instance = hInstance;

	// ensure folders exist!
	CreateDirectory((g_app_path + "save").c_str(), NULL);
	CreateDirectory((g_app_path + "screenshots").c_str(), NULL);
	CreateDirectory((g_app_path + "plugin").c_str(), NULL);
	CreateDirectory((g_app_path + "backups").c_str(), NULL);

	init_config();
	load_config();
	lua_init();

	WNDCLASSEX wc = {0};
	MSG msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(g_app_instance, MAKEINTRESOURCE(IDI_M64ICONBIG));
	wc.hIconSm = LoadIcon(g_app_instance, MAKEINTRESOURCE(IDI_M64ICONSMALL));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = WND_CLASS;
	wc.lpfnWndProc = WndProc;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MYMENU);

	RegisterClassEx(&wc);
	MGECompositor::init();

	printf("[View] Restoring window @ (%d|%d) %dx%d...\n", g_config.window_x, g_config.window_y, g_config.window_width, g_config.window_height);

	g_main_hwnd = CreateWindowEx(
		0,
		WND_CLASS,
		MUPEN_VERSION,
		WS_OVERLAPPEDWINDOW | WS_EX_COMPOSITED,
		g_config.window_x, g_config.window_y, g_config.window_width,
		g_config.window_height,
		NULL, NULL, hInstance, NULL);

	ShowWindow(g_main_hwnd, nCmdShow);
	// we apply WS_EX_LAYERED to fix off-screen blitting (off-screen window portions are not included otherwise)
	SetWindowLong(g_main_hwnd, GWL_EXSTYLE, WS_EX_ACCEPTFILES | WS_EX_LAYERED);

	g_recent_roms_menu = GetSubMenu(GetSubMenu(g_main_menu, 0), 5);
	g_recent_movies_menu = GetSubMenu(GetSubMenu(g_main_menu, 3), 5);
	g_recent_lua_menu = GetSubMenu(GetSubMenu(g_main_menu, 6), 2);

	RECT rect{};
	GetClientRect(g_main_hwnd, &rect);

	Messenger::subscribe(Messenger::Message::EmuLaunchedChanged, on_emu_launched_changed);
	Messenger::subscribe(Messenger::Message::EmuStopping, on_emu_stopping);
	Messenger::subscribe(Messenger::Message::EmuPausedChanged, on_emu_paused_changed);
	Messenger::subscribe(Messenger::Message::CapturingChanged, on_capturing_changed);
	Messenger::subscribe(Messenger::Message::MovieLoopChanged, on_movie_loop_changed);
	Messenger::subscribe(Messenger::Message::ReadonlyChanged, on_readonly_changed);
	Messenger::subscribe(Messenger::Message::TaskChanged, on_task_changed);
	Messenger::subscribe(Messenger::Message::ScriptStarted, on_script_started);
	Messenger::subscribe(Messenger::Message::SpeedModifierChanged, on_speed_modifier_changed);
	Messenger::subscribe(Messenger::Message::LagLimitExceeded, on_vis_since_input_poll_exceeded);
	Messenger::subscribe(Messenger::Message::CoreResult, on_core_result);
	Messenger::subscribe(Messenger::Message::FullscreenChanged, on_fullscreen_changed);
	Messenger::subscribe(Messenger::Message::ConfigLoaded, on_config_loaded);
	Messenger::subscribe(Messenger::Message::EmuStartingChanged, [](std::any data)
	{
		g_emu_starting = std::any_cast<bool>(data);
		update_titlebar();
	});

	// Rombrowser needs to be initialized *after* other components, since it depends on their state smh bru
	Statusbar::init();
	Rombrowser::init();
	VCR::init();
	EncodingManager::init();
	Cli::init();
	Seeker::init();
	savestates_init();
	setup_dummy_info();

	Recent::build(g_config.recent_rom_paths, ID_RECENTROMS_FIRST, g_recent_roms_menu);
	Recent::build(g_config.recent_movie_paths, ID_RECENTMOVIES_FIRST, g_recent_movies_menu);
	Recent::build(g_config.recent_lua_script_paths, ID_LUA_RECENT, g_recent_lua_menu);

	Messenger::broadcast(Messenger::Message::StatusbarVisibilityChanged, (bool)g_config.is_statusbar_enabled);
	Messenger::broadcast(Messenger::Message::MovieLoopChanged, (bool)g_config.is_movie_loop_enabled);
	Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.vcr_readonly);
	Messenger::broadcast(Messenger::Message::EmuLaunchedChanged, false);
	Messenger::broadcast(Messenger::Message::CapturingChanged, false);
	Messenger::broadcast(Messenger::Message::SizeChanged, rect);
	Messenger::broadcast(Messenger::Message::AppReady, nullptr);
	Messenger::broadcast(Messenger::Message::ConfigLoaded, nullptr);

	//warning, this is ignored when debugger is attached (like visual studio)
	SetUnhandledExceptionFilter(ExceptionReleaseTarget);

	// raise noncontinuable exception (impossible to recover from it)
	//RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, NULL, NULL);
	//
	// raise continuable exception
	//RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, NULL, NULL);

	// We need to set the core updateScreen flag at 60 FPS.
	// WM_TIMER isn't stable enough and the other multimedia or callback timers are too annoying
	std::thread([]
	{
		while (true)
		{
			screen_invalidated = true;
			timeBeginPeriod(1);
			Sleep(1000 / 60);
			timeEndPeriod(1);
		}
	}).detach();

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


	return (int)msg.wParam;
}
