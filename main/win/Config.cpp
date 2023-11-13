#include <Windows.h>
#include <winuser.h>
#include <cstdio>
#include "../../winproject/resource.h"
#include "Config.hpp"
#include "main_win.h"
#include "../vcr.h"
#include "../lib/ini.h"
#include <helpers/string_helpers.h>

CONFIG Config;
std::vector<t_hotkey*> hotkeys;

void set_menu_accelerator(const HMENU h_menu, const int element_id, const char* acc)
{
	char string[256] = {0};
	GetMenuString(h_menu, element_id, string, std::size(string), MF_BYPOSITION);

	MENUITEMINFO menuinfo;

	// make sure there is tab character (accelerator marker)
	if (char* tab = strrchr(string, '\t'))
		*tab = '\0';
	if (strcmp(acc, "") != 0)
		sprintf(string, "%s\t%s", string, acc);

	memset(&menuinfo, 0, sizeof(MENUITEMINFO));
	menuinfo.cbSize = sizeof(MENUITEMINFO);
	menuinfo.fMask = MIIM_TYPE;
	menuinfo.fType = MFT_STRING;
	menuinfo.dwTypeData = string;
	menuinfo.cch = sizeof(string);
	SetMenuItemInfo(h_menu, element_id, TRUE, &menuinfo);
}

void set_hotkey_menu_accelerators(t_hotkey* hotkey, const HMENU hmenu, const int menu_item_id)
{
	if (!hmenu || menu_item_id < 0)
		return;

	const std::string hotkey_str = hotkey_to_string(hotkey);
	set_menu_accelerator(hmenu, menu_item_id, hotkey_str == "(nothing)" ? "" : hotkey_str.c_str());
}


std::string hotkey_to_string(t_hotkey* hotkey)
{
	char buf[260] = {0};
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
	return {std::string(buf)};
}


CONFIG get_default_config()
{
	CONFIG config = {};

	config.fast_forward_hotkey = {
		.identifier = "Fast-forward",
		.key = VK_TAB,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = 0,
	};

	config.speed_up_hotkey = {
		.identifier = "Speed up",
		.key = VK_OEM_PLUS,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = IDC_INCREASE_MODIFIER,
	};

	config.speed_down_hotkey = {
		.identifier = "Speed down",
		.key = VK_OEM_MINUS,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = IDC_DECREASE_MODIFIER,
	};

	config.frame_advance_hotkey = {
		.identifier = "Frame advance",
		.key = VK_OEM_5,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = EMU_FRAMEADVANCE,
	};

	config.pause_hotkey = {
		.identifier = "Pause",
		.key = VK_PAUSE,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = EMU_PAUSE,
	};

	config.toggle_read_only_hotkey = {
		.identifier = "Toggle read-only",
		.key = 0x38 /* 8 */,
		.ctrl = 1,
		.shift = 0,
		.alt = 0,
		.command = EMU_VCRTOGGLEREADONLY,
	};

	config.start_movie_playback_hotkey = {
		.identifier = "Start movie playback",
		.key = 0x50 /* P */,
		.ctrl = 1,
		.shift = 0,
		.alt = 0,
		.command = ID_START_PLAYBACK,
	};

	config.stop_movie_playback_hotkey = {
		.identifier = "Stop movie playback",
		.key = 0x53 /* S */,
		.ctrl = 1,
		.shift = 0,
		.alt = 0,
		.command = ID_STOP_PLAYBACK,
	};

	config.start_movie_recording_hotkey = {
		.identifier = "Start movie recording",
		.key = 0x52 /* R */,
		.ctrl = 1,
		.shift = 0,
		.alt = 0,
		.command = ID_START_RECORD,
	};

	config.stop_movie_recording_hotkey = {
		.identifier = "Stop movie recording",
		.key = 0x53 /* S */,
		.ctrl = 1,
		.shift = 0,
		.alt = 0,
		.command = ID_STOP_RECORD,
	};

	config.take_screenshot_hotkey = {
		.identifier = "Take screenshot",
		.key = VK_F12,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = GENERATE_BITMAP,
	};

	config.save_to_current_slot_hotkey = {
		.identifier = "Save to current slot",
		.key = 0x49 /* I */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = STATE_SAVE,
	};

	config.load_from_current_slot_hotkey = {
		.identifier = "Load from current slot",
		.key = 0x50 /* P */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = STATE_RESTORE,
	};

	config.restart_movie_hotkey = {
		.identifier = "Restart playing movie",
		.key = 0x52 /* R */,
		.ctrl = 1,
		.shift = 1,
		.alt = 0,
		.command = ID_RESTART_MOVIE,
	};

	config.play_latest_movie_hotkey = {
		.identifier = "Play latest movie",
		.key = 0x50 /* P */,
		.ctrl = 1,
		.shift = 1,
		.alt = 0,
		.command = ID_REPLAY_LATEST,
	};

	config.save_to_slot_1_hotkey = {
		.identifier = "Save to slot 1",
		.key = 0x31 /* 1 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 1,
	};

	config.save_to_slot_2_hotkey = {
		.identifier = "Save to slot 2",
		.key = 0x32 /* 2 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 2,
	};

	config.save_to_slot_3_hotkey = {
		.identifier = "Save to slot 3",
		.key = 0x33 /* 3 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 3,
	};

	config.save_to_slot_4_hotkey = {
		.identifier = "Save to slot 4",
		.key = 0x34 /* 4 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 4,
	};

	config.save_to_slot_5_hotkey = {
		.identifier = "Save to slot 5",
		.key = 0x35 /* 5 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 5,
	};

	config.save_to_slot_6_hotkey = {
		.identifier = "Save to slot 6",
		.key = 0x36 /* 6 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 6,
	};

	config.save_to_slot_7_hotkey = {
		.identifier = "Save to slot 7",
		.key = 0x37 /* 7 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 7,
	};

	config.save_to_slot_8_hotkey = {
		.identifier = "Save to slot 8",
		.key = 0x38 /* 8 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 8,
	};

	config.save_to_slot_9_hotkey = {
		.identifier = "Save to slot 9",
		.key = 0x39 /* 9 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 9,
	};

	config.save_to_slot_10_hotkey = {
		.identifier = "Save to slot 10",
		.key = 0x30 /* 0 */,
		.ctrl = 0,
		.shift = 1,
		.alt = 0,
		.command = (ID_SAVE_1 - 1) + 10,
	};

	config.load_from_slot_1_hotkey = {
		.identifier = "Load from slot 1",
		.key = VK_F1,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 1,
	};

	config.load_from_slot_2_hotkey = {
		.identifier = "Load from slot 2",
		.key = VK_F2,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 2,
	};

	config.load_from_slot_3_hotkey = {
		.identifier = "Load from slot 3",
		.key = VK_F3,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 3,
	};

	config.load_from_slot_4_hotkey = {
		.identifier = "Load from slot 4",
		.key = VK_F4,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 4,
	};

	config.load_from_slot_5_hotkey = {
		.identifier = "Load from slot 5",
		.key = VK_F5,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 5,
	};

	config.load_from_slot_6_hotkey = {
		.identifier = "Load from slot 6",
		.key = VK_F6,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 6,
	};

	config.load_from_slot_7_hotkey = {
		.identifier = "Load from slot 7",
		.key = VK_F7,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 7,
	};

	config.load_from_slot_8_hotkey = {
		.identifier = "Load from slot 8",
		.key = VK_F8,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 8,
	};

	config.load_from_slot_9_hotkey = {
		.identifier = "Load from slot 9",
		.key = VK_F9,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 9,
	};

	config.load_from_slot_10_hotkey = {
		.identifier = "Load from slot 10",
		.key = VK_F10,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_LOAD_1 - 1) + 10,
	};

	config.select_slot_1_hotkey = {
		.identifier = "Select slot 1",
		.key = 0x31 /* 1 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 1,
	};

	config.select_slot_2_hotkey = {
		.identifier = "Select slot 2",
		.key = 0x32 /* 2 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 2,
	};

	config.select_slot_3_hotkey = {
		.identifier = "Select slot 3",
		.key = 0x33 /* 3 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 3,
	};

	config.select_slot_4_hotkey = {
		.identifier = "Select slot 4",
		.key = 0x34 /* 4 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 4,
	};

	config.select_slot_5_hotkey = {
		.identifier = "Select slot 5",
		.key = 0x35 /* 5 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 5,
	};

	config.select_slot_6_hotkey = {
		.identifier = "Select slot 6",
		.key = 0x36 /* 6 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 6,
	};

	config.select_slot_7_hotkey = {
		.identifier = "Select slot 7",
		.key = 0x37 /* 7 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 7,
	};

	config.select_slot_8_hotkey = {
		.identifier = "Select slot 8",
		.key = 0x38 /* 8 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 8,
	};

	config.select_slot_9_hotkey = {
		.identifier = "Select slot 9",
		.key = 0x39 /* 9 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 9,
	};

	config.select_slot_10_hotkey = {
		.identifier = "Select slot 10",
		.key = 0x30 /* 0 */,
		.ctrl = 0,
		.shift = 0,
		.alt = 0,
		.command = (ID_CURRENTSAVE_1 - 1) + 10,
	};

	config.language = "English";
	config.show_fps = 1;
	config.show_vis_per_second = 1;
	config.is_savestate_warning_enabled = 1;
	config.is_rom_movie_compatibility_check_enabled = 1;
	config.core_type = 1;
	config.fps_modifier = 100;
	config.frame_skip_frequency = 1;
	config.is_movie_loop_enabled = 0;
	config.cpu_clock_speed_multiplier = 1;
	config.is_unfocused_pause_enabled = 0;
	config.is_toolbar_enabled = 1;
	config.is_statusbar_enabled = 1;
	config.is_state_independent_state_loading_allowed = 0;
	config.is_default_plugins_directory_used = 1;
	config.is_default_saves_directory_used = 1;
	config.is_default_screenshots_directory_used = 1;
	config.plugins_directory.clear();
	config.saves_directory.clear();
	config.screenshots_directory.clear();
	config.states_path.clear();
	config.recent_rom_paths = {};
	config.recent_movie_paths = {};
	config.is_recent_movie_paths_frozen = 0;
	config.rombrowser_sorted_column = 2;
	config.rombrowser_sort_ascending = 1;
	config.rombrowser_column_widths = {24, 240, 240, 120};
	config.is_rombrowser_recursion_enabled = 0;
	config.is_reset_recording_enabled = 1;
	config.is_internal_capture_forced = 0;
	config.is_capture_cropped_screen_dc = 0;
	config.capture_delay = 0;
	config.is_unknown_hotkey_selection_allowed = 1;
	config.avi_capture_path.clear();
	config.synchronization_mode = vcr_sync_audio_dupl;
	config.lua_script_path.clear();
	config.recent_lua_script_paths = {};
	config.is_recent_scripts_frozen = 0;
	config.use_summercart = 0;
	config.is_round_towards_zero_enabled = 0;
	config.is_float_exception_propagation_enabled = 0;
	config.is_audio_delay_enabled = 1;
	config.is_compiled_jump_enabled = 1;
	config.is_lua_double_buffered = 1;
	config.selected_video_plugin.clear();
	config.selected_audio_plugin.clear();
	config.selected_input_plugin.clear();
	config.selected_rsp_plugin.clear();
	config.last_movie_type = 1;
	config.last_movie_author = "Unknown Author";
	config.window_x = CW_USEDEFAULT;
	config.window_y = CW_USEDEFAULT;
	config.window_width = 640;
	config.window_height = 480;

	return config;
}


void set_dlg_item_hotkey(const HWND hwnd, const int idc, t_hotkey* hotkey)
{
	SetDlgItemText(hwnd, idc, hotkey_to_string(hotkey).c_str());
}

void set_dlg_item_hotkey_and_menu(const HWND hwnd, const int idc, t_hotkey* hotkey, const HMENU hmenu,
                             const int menu_item_id)
{
	const std::string hotkey_str = hotkey_to_string(hotkey);
	SetDlgItemText(hwnd, idc, hotkey_str.c_str());

	if (hmenu && menu_item_id >= 0)
	{
		set_menu_accelerator(hmenu, menu_item_id,
						   hotkey_str == "(nothing)" ? "" : hotkey_str.c_str());
	}
}


void update_menu_hotkey_labels()
{
	set_hotkey_menu_accelerators(&Config.pause_hotkey, GetSubMenu(GetMenu(main_hwnd), 1), 0);
    set_hotkey_menu_accelerators(&Config.frame_advance_hotkey, GetSubMenu(GetMenu(main_hwnd), 1), 1);
    set_hotkey_menu_accelerators(&Config.load_from_current_slot_hotkey, GetSubMenu(GetMenu(main_hwnd), 1), 4);
    set_hotkey_menu_accelerators(&Config.save_to_current_slot_hotkey, GetSubMenu(GetMenu(main_hwnd), 1), 6);

    set_hotkey_menu_accelerators(&Config.toggle_read_only_hotkey, GetSubMenu(GetMenu(main_hwnd), 3), 13);
    set_hotkey_menu_accelerators(&Config.start_movie_playback_hotkey, GetSubMenu(GetMenu(main_hwnd), 3), 3);
    set_hotkey_menu_accelerators(&Config.stop_movie_playback_hotkey, GetSubMenu(GetMenu(main_hwnd), 3), 4);
    set_hotkey_menu_accelerators(&Config.start_movie_recording_hotkey, GetSubMenu(GetMenu(main_hwnd), 3), 0);
    set_hotkey_menu_accelerators(&Config.stop_movie_recording_hotkey, GetSubMenu(GetMenu(main_hwnd), 3), 1);
    set_hotkey_menu_accelerators(&Config.take_screenshot_hotkey, GetSubMenu(GetMenu(main_hwnd), 1), 2);
    set_hotkey_menu_accelerators(&Config.save_to_current_slot_hotkey, GetSubMenu(GetMenu(main_hwnd), 1), 4);
    set_hotkey_menu_accelerators(&Config.load_from_current_slot_hotkey, GetSubMenu(GetMenu(main_hwnd), 1), 6);
    set_hotkey_menu_accelerators(&Config.select_slot_1_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 0);
    set_hotkey_menu_accelerators(&Config.select_slot_2_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 1);
    set_hotkey_menu_accelerators(&Config.select_slot_3_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 2);
    set_hotkey_menu_accelerators(&Config.select_slot_4_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 3);
    set_hotkey_menu_accelerators(&Config.select_slot_5_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 4);
    set_hotkey_menu_accelerators(&Config.select_slot_6_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 5);
    set_hotkey_menu_accelerators(&Config.select_slot_7_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 6);
    set_hotkey_menu_accelerators(&Config.select_slot_8_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 7);
    set_hotkey_menu_accelerators(&Config.select_slot_9_hotkey, GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 8);

    set_hotkey_menu_accelerators(&Config.restart_movie_hotkey, GetSubMenu(GetMenu(main_hwnd), 3), 12);
    set_hotkey_menu_accelerators(&Config.play_latest_movie_hotkey, GetSubMenu(GetMenu(main_hwnd), 3), 7);

	set_dlg_item_hotkey(main_hwnd, IDC_HOT_FASTFORWARD,
	                 &Config.fast_forward_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_HOT_SPEEDUP, &Config.speed_up_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_HOT_SPEEDDOWN, &Config.speed_down_hotkey);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_FRAMEADVANCE,
	                        &Config.frame_advance_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 1), 1);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_PAUSE, &Config.pause_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 1), 0);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_READONLY,
	                        &Config.toggle_read_only_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 3), 15);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_PLAY,
	                        &Config.start_movie_playback_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 3), 3);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_PLAYSTOP,
	                        &Config.stop_movie_playback_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 3), 4);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_RECORD,
	                        &Config.start_movie_recording_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 3), 0);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_RECORDSTOP,
	                        &Config.stop_movie_recording_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 3), 1);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_HOT_SCREENSHOT,
	                        &Config.take_screenshot_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 1), 2);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_CSAVE,
	                        &Config.save_to_current_slot_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 1), 4);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_CLOAD,
	                        &Config.load_from_current_slot_hotkey,
	                        GetSubMenu(GetMenu(main_hwnd), 1), 6);

	set_dlg_item_hotkey(main_hwnd, IDC_1SAVE, &Config.save_to_slot_1_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_2SAVE, &Config.save_to_slot_2_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_3SAVE, &Config.save_to_slot_3_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_4SAVE, &Config.save_to_slot_4_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_5SAVE, &Config.save_to_slot_5_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_6SAVE, &Config.save_to_slot_6_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_7SAVE, &Config.save_to_slot_7_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_8SAVE, &Config.save_to_slot_8_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_9SAVE, &Config.save_to_slot_9_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_10SAVE, &Config.save_to_slot_10_hotkey);

	set_dlg_item_hotkey(main_hwnd, IDC_1LOAD, &Config.load_from_slot_1_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_2LOAD, &Config.load_from_slot_2_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_3LOAD, &Config.load_from_slot_3_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_4LOAD, &Config.load_from_slot_4_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_5LOAD, &Config.load_from_slot_5_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_6LOAD, &Config.load_from_slot_6_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_7LOAD, &Config.load_from_slot_7_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_8LOAD, &Config.load_from_slot_8_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_9LOAD, &Config.load_from_slot_9_hotkey);
	set_dlg_item_hotkey(main_hwnd, IDC_10LOAD, &Config.load_from_slot_10_hotkey);

	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_1SEL, &Config.select_slot_1_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 0);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_2SEL, &Config.select_slot_2_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 1);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_3SEL, &Config.select_slot_3_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 2);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_4SEL, &Config.select_slot_4_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 3);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_5SEL, &Config.select_slot_5_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 4);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_6SEL, &Config.select_slot_6_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 5);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_7SEL, &Config.select_slot_7_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 6);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_8SEL, &Config.select_slot_8_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 7);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_9SEL, &Config.select_slot_9_hotkey,
	                        GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 8);
	set_dlg_item_hotkey_and_menu(main_hwnd, IDC_10SEL, &Config.select_slot_10_hotkey,
						GetSubMenu(GetSubMenu(GetMenu(main_hwnd), 1), 9), 9);

}
void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
                         const int32_t is_reading, t_hotkey* hotkey)
{
	if (is_reading)
	{
		if (!ini.has(field_name))
		{
			return;
		}

		hotkey->key = std::stoi(ini[field_name]["key"]);
		hotkey->ctrl = std::stoi(ini[field_name]["ctrl"]);
		hotkey->shift = std::stoi(ini[field_name]["shift"]);
		hotkey->alt = std::stoi(ini[field_name]["alt"]);
	} else
	{
		ini[field_name]["key"] = std::to_string(hotkey->key);
		ini[field_name]["ctrl"] = std::to_string(hotkey->ctrl);
		ini[field_name]["shift"] = std::to_string(hotkey->shift);
		ini[field_name]["alt"] = std::to_string(hotkey->alt);
	}
}

void handle_config_value(mINI::INIStructure& ini, const std::string &field_name,
                         const int32_t is_reading, int32_t* value)
{
	if (is_reading)
	{
		// keep the default value if the key doesnt exist
		// it will be created upon saving anyway
		if (!ini["Config"].has(field_name))
		{
			return;
		}
		*value = std::stoi(ini["Config"][field_name]);
	} else
	{
		ini["Config"][field_name] = std::to_string(*value);
	}
}

void handle_config_value(mINI::INIStructure& ini, const std::string &field_name,
                         const int32_t is_reading, std::string& value)
{
	if (is_reading)
	{
		// keep the default value if the key doesnt exist
		// it will be created upon saving anyway
		if (!ini["Config"].has(field_name))
		{
			return;
		}
		value = ini["Config"][field_name];
	} else
	{
		ini["Config"][field_name] = value;
	}
}

void handle_config_value(mINI::INIStructure& ini, const std::string &field_name,
                         const int32_t is_reading, std::vector<std::string>& value)
{
	if (is_reading)
	{
		// if the virtual collection doesn't exist just leave the vector empty, as attempting to read will crash
		if (!ini.has(field_name))
		{
			return;
		}

		for (size_t i = 0; i < ini[field_name].size(); i++)
		{
			value.push_back(ini[field_name][std::to_string(i)]);
		}
	} else
	{
		// create virtual collection:
		// dump under key field_name with i
		// [field_name]
		// 0 = a.m64
		// 1 = b.m64
		for (size_t i = 0; i < value.size(); i++)
		{
			ini[field_name][std::to_string(i)] = value[i];
		}
	}
}

void handle_config_value(mINI::INIStructure& ini, const std::string &field_name,
						 const int32_t is_reading, std::map<std::string, std::wstring>& value)
{
	if (is_reading) {
		// if the virtual map doesn't exist just leave the vector empty, as attempting to read will crash
		if (!ini.has(field_name)) {
			return;
		}
		for (const auto& map = ini[field_name]; const auto& [fst, snd] : map) {
			value[fst] = string_to_wstring(snd);
		}
	} else {
		// create virtual map:
		// [field_name]
		// value = value
		for (auto & [fst, snd] : value) {
			ini[field_name][fst] = wstring_to_string(snd);
		}
	}

}

void handle_config_value(mINI::INIStructure& ini, const std::string &field_name,
                        const int32_t is_reading, std::vector<int32_t>& value)
{
	if (is_reading)
	{
		int vector_length = 0;
		for (size_t i = 0; i < INT32_MAX; i++)
		{
			if (!ini["Config"].has(field_name + "_" + std::to_string(i)))
			{
				vector_length = (int)i;
				break;
			}
		}
		value.clear();
		for (size_t i = 0; i < (unsigned int)vector_length; i++)
		{
			value.push_back(
				std::stoi(ini["Config"][field_name + "_" + std::to_string(i)]));
		}
	} else
	{
		for (size_t i = 0; i < value.size(); i++)
		{
			ini["Config"][field_name + "_" + std::to_string(i)] =
				std::to_string(value[i]);
		}
	}
}


const auto first_offset = offsetof(CONFIG, fast_forward_hotkey);
const auto last_offset = offsetof(CONFIG, select_slot_10_hotkey);
const CONFIG default_config = get_default_config();

std::vector<t_hotkey*> collect_hotkeys(const CONFIG* config)
{
	std::vector<t_hotkey*> hotkeys; // shadows global hotkeys declaration
	const auto arr = (t_hotkey*)config;
	// NOTE:
	// last_offset should contain the offset of the last hotkey
	// this also requires that the hotkeys are laid out contiguously, or else the pointer arithmetic fails
	// i recommend inserting your new hotkeys before the savestate hotkeys... pretty please
	for (size_t i = 0; i < ((last_offset - first_offset) / sizeof(t_hotkey)) + 1
	     ; i++)
	{
		auto hotkey = &arr[i];
		// printf("Hotkey[%d]: %s\n", i, hotkey->identifier.c_str());
		hotkeys.push_back(hotkey);
	}
	// printf("---\n");

	return hotkeys;
}

mINI::INIStructure handle_config_ini(const bool is_reading, mINI::INIStructure ini)
{
#define HANDLE_P_VALUE(x) handle_config_value(ini, #x, is_reading, &Config.x);
#define HANDLE_VALUE(x) handle_config_value(ini, #x, is_reading, Config.x);

	if (is_reading)
	{
		// our config is empty, so hotkeys are missing identifiers and commands
		// we need to copy the identifiers from a default config
		// FIXME: this assumes that the loaded config's hotkeys map 1:1 to the current hotkeys, which may not be the case

		const auto base_config_hotkey_pointers = collect_hotkeys(&default_config);
		const auto hotkey_pointers = collect_hotkeys(&Config);

		for (size_t i = 0; i < hotkey_pointers.size(); i++)
		{
			hotkey_pointers[i]->identifier = std::string(
				base_config_hotkey_pointers[i]->identifier);
			hotkey_pointers[i]->command = base_config_hotkey_pointers[i]->
				command;
		}
	}


	const auto hotkey_pointers = collect_hotkeys(&Config);

	hotkeys.clear();
	for (auto& hotkey_pointer : hotkey_pointers)
	{
		handle_config_value(ini, hotkey_pointer->identifier, is_reading,
		                    hotkey_pointer);
		hotkeys.push_back(hotkey_pointer);
	}

	HANDLE_VALUE(language)
	HANDLE_P_VALUE(show_fps)
	HANDLE_P_VALUE(show_vis_per_second)
	HANDLE_P_VALUE(is_savestate_warning_enabled)
	HANDLE_P_VALUE(is_rom_movie_compatibility_check_enabled)
	HANDLE_P_VALUE(core_type)
	HANDLE_P_VALUE(fps_modifier)
	HANDLE_P_VALUE(frame_skip_frequency)
	HANDLE_P_VALUE(is_movie_loop_enabled)
	HANDLE_P_VALUE(cpu_clock_speed_multiplier)
	HANDLE_P_VALUE(is_unfocused_pause_enabled)
	HANDLE_P_VALUE(is_toolbar_enabled)
	HANDLE_P_VALUE(is_statusbar_enabled)
	HANDLE_P_VALUE(is_state_independent_state_loading_allowed)
	HANDLE_P_VALUE(is_default_plugins_directory_used)
	HANDLE_P_VALUE(is_default_saves_directory_used)
	HANDLE_P_VALUE(is_default_screenshots_directory_used)
	HANDLE_VALUE(plugins_directory)
	HANDLE_VALUE(saves_directory)
	HANDLE_VALUE(screenshots_directory)
	HANDLE_VALUE(states_path)
	HANDLE_VALUE(recent_rom_paths)
	HANDLE_P_VALUE(is_recent_rom_paths_frozen)
	HANDLE_VALUE(recent_movie_paths)
	HANDLE_P_VALUE(is_recent_movie_paths_frozen)
	HANDLE_P_VALUE(is_rombrowser_recursion_enabled)
	HANDLE_P_VALUE(is_reset_recording_enabled)
	HANDLE_P_VALUE(is_internal_capture_forced)
	HANDLE_P_VALUE(is_capture_cropped_screen_dc)
	HANDLE_P_VALUE(capture_delay)
	HANDLE_P_VALUE(is_unknown_hotkey_selection_allowed)
	HANDLE_VALUE(avi_capture_path)
	HANDLE_P_VALUE(synchronization_mode)
	HANDLE_VALUE(lua_script_path)
	HANDLE_VALUE(recent_lua_script_paths)
	HANDLE_P_VALUE(is_recent_scripts_frozen)
	HANDLE_P_VALUE(use_summercart)
	HANDLE_P_VALUE(is_round_towards_zero_enabled)
	HANDLE_P_VALUE(is_float_exception_propagation_enabled)
	HANDLE_P_VALUE(is_audio_delay_enabled)
	HANDLE_P_VALUE(is_compiled_jump_enabled)
	HANDLE_P_VALUE(is_lua_double_buffered)
	HANDLE_VALUE(selected_video_plugin)
	HANDLE_VALUE(selected_audio_plugin)
	HANDLE_VALUE(selected_input_plugin)
	HANDLE_VALUE(selected_rsp_plugin)
	HANDLE_P_VALUE(last_movie_type)
	HANDLE_VALUE(last_movie_author)
	HANDLE_P_VALUE(window_x)
	HANDLE_P_VALUE(window_y)
	HANDLE_P_VALUE(window_width)
	HANDLE_P_VALUE(window_height)
	HANDLE_VALUE(rombrowser_column_widths)
	HANDLE_VALUE(rombrowser_rom_paths)
	HANDLE_P_VALUE(rombrowser_sort_ascending)
	HANDLE_P_VALUE(rombrowser_sorted_column)
	HANDLE_VALUE(persistent_folder_paths)

	return ini;
}

std::string get_config_path()
{
	return app_path + "config.ini";
}

void save_config()
{
	std::remove(get_config_path().c_str());

	const mINI::INIFile file(get_config_path());
	mINI::INIStructure ini;

	ini = handle_config_ini(false, ini);

	file.write(ini, true);
}

void load_config()
{
	struct stat buf;
	if (stat(get_config_path().c_str(), &buf) != 0)
	{
		printf("[CONFIG] Default config file does not exist. Generating...\n");
		Config = get_default_config();
		save_config();
	}

	const mINI::INIFile file(get_config_path());
	mINI::INIStructure ini;
	file.read(ini);

	ini = handle_config_ini(true, ini);

	// handle edge case: closing while minimized produces bogus values for position
	if (Config.window_x < -10'000 || Config.window_y < -10'000)
	{
		Config.window_x = default_config.window_x;
		Config.window_y = default_config.window_y;
		Config.window_width = default_config.window_width;
		Config.window_height = default_config.window_height;
	}

	if (Config.rombrowser_column_widths.size() < 4) {
		// something's malformed, fuck off and use default values
		Config.rombrowser_column_widths = default_config.rombrowser_column_widths;
	}
}

int32_t get_user_hotkey(t_hotkey* hotkey)
{
	int lc = 0, ls = 0, la = 0;
	for (int i = 0; i < 500; i++)
	{
		SleepEx(10, TRUE);
		for (int j = 8; j < 254; j++)
		{
			if (j == VK_LCONTROL || j == VK_RCONTROL || j == VK_LMENU || j ==
				VK_RMENU || j == VK_LSHIFT || j == VK_RSHIFT)
				continue;

			if (GetAsyncKeyState(j) & 0x8000)
			{
				// HACK to avoid exiting all the way out of the dialog on pressing escape to clear a hotkeys
				// or continually re-activating the button on trying to assign space as a hotkeys
				if (j == VK_ESCAPE)
					return 0;

				if (j == VK_CONTROL)
				{
					lc = 1;
					continue;
				}
				if (j == VK_SHIFT)
				{
					ls = 1;
					continue;
				}
				if (j == VK_MENU)
				{
					la = 1;
					continue;
				}
				if (j != VK_ESCAPE)
				{
					hotkey->key = j;
					hotkey->shift = GetAsyncKeyState(VK_SHIFT) ? 1 : 0;
					hotkey->ctrl = GetAsyncKeyState(VK_CONTROL) ? 1 : 0;
					hotkey->alt = GetAsyncKeyState(VK_MENU) ? 1 : 0;
					return 1;
				}
				memset(hotkey, 0, sizeof(t_hotkey)); // clear key on escape
				return 0;
			}
			if (j == VK_CONTROL && lc)
			{
				hotkey->key = 0;
				hotkey->shift = 0;
				hotkey->ctrl = 1;
				hotkey->alt = 0;
				return 1;
			}
			if (j == VK_SHIFT && ls)
			{
				hotkey->key = 0;
				hotkey->shift = 1;
				hotkey->ctrl = 0;
				hotkey->alt = 0;
				return 1;
			}
			if (j == VK_MENU && la)
			{
				hotkey->key = 0;
				hotkey->shift = 0;
				hotkey->ctrl = 0;
				hotkey->alt = 1;
				return 1;
			}
		}
	}
	//we checked all keys and none of them was pressed, so give up
	return 0;
}

