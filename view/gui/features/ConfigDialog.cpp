/***************************************************************************
						  configdialog.c  -  description
							 -------------------
	copyright            : (C) 2003 by ShadowPrince
	email                : shadow@emulation64.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <shlobj.h>
#include <cstdio>
#include <vector>
#include <cassert>
#include <view/lua/LuaConsole.h>
#include <view/gui/Main.h>
#include <winproject/resource.h>
#include <core/r4300/Plugin.hpp>
#include <view/gui/features/RomBrowser.hpp>
#include <core/r4300/timers.h>
#include <shared/Config.hpp>
#include <view/gui/wrapper/PersistentPathDialog.h>
#include <shared/helpers/StringHelpers.h>
#include <view/helpers/WinHelpers.h>
#include <view/helpers/StringHelpers.h>
#include <core/r4300/r4300.h>
#include "configdialog.h"
#include <view/capture/EncodingManager.h>

#include <shared/Messenger.h>

std::vector<std::unique_ptr<Plugin>> available_plugins;
std::vector<HWND> tooltips;


/// <summary>
/// Waits until the user inputs a valid key sequence, then fills out the hotkey
/// </summary>
/// <returns>
/// Whether a hotkey has successfully been picked
/// </returns>
int32_t get_user_hotkey(t_hotkey* hotkey)
{
	int i, j;
	int lc = 0, ls = 0, la = 0;
	for (i = 0; i < 500; i++)
	{
		SleepEx(10, TRUE);
		for (j = 8; j < 254; j++)
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

BOOL CALLBACK about_dlg_proc(const HWND hwnd, const UINT message, const WPARAM w_param, LPARAM)
{
	switch (message)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage(hwnd, IDB_LOGO, STM_SETIMAGE, IMAGE_BITMAP,
		                   (LPARAM)LoadImage(g_app_instance, MAKEINTRESOURCE(IDB_LOGO),
		                                     IMAGE_BITMAP, 0, 0, 0));
		return TRUE;

	case WM_CLOSE:
		EndDialog(hwnd, IDOK);
		break;

	case WM_COMMAND:
		switch (LOWORD(w_param))
		{
		case IDOK:
			EndDialog(hwnd, IDOK);
			break;

		case IDC_WEBSITE:
			ShellExecute(nullptr, nullptr, "http://mupen64.emulation64.com", nullptr, nullptr, SW_SHOW);
			break;
		case IDC_GITREPO:
			ShellExecute(nullptr, nullptr, "https://github.com/mkdasher/mupen64-rr-lua-/", nullptr, nullptr, SW_SHOW);
			break;
		default:
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void configdialog_about()
{
	DialogBox(g_app_instance,
	          MAKEINTRESOURCE(IDD_ABOUT), g_main_hwnd,
	          about_dlg_proc);
}

void build_rom_browser_path_list(const HWND dialog_hwnd)
{
	const HWND hwnd = GetDlgItem(dialog_hwnd, IDC_ROMBROWSER_DIR_LIST);

	SendMessage(hwnd, LB_RESETCONTENT, 0, 0);

	for (const std::string& str : g_config.rombrowser_rom_paths)
	{
		SendMessage(hwnd, LB_ADDSTRING, 0,
		            (LPARAM)str.c_str());
	}
}

BOOL CALLBACK directories_cfg(const HWND hwnd, const UINT message, const WPARAM w_param, LPARAM l_param)
{
	[[maybe_unused]] LPITEMIDLIST pidl{};
	BROWSEINFO bi{};
	auto l_nmhdr = (NMHDR*)&l_param;
	switch (message)
	{
	case WM_INITDIALOG:

		build_rom_browser_path_list(hwnd);

		SendMessage(GetDlgItem(hwnd, IDC_RECURSION), BM_SETCHECK,
		            g_config.is_rombrowser_recursion_enabled ? BST_CHECKED : BST_UNCHECKED, 0);

		if (g_config.is_default_plugins_directory_used)
		{
			SendMessage(GetDlgItem(hwnd, IDC_DEFAULT_PLUGINS_CHECK), BM_SETCHECK, BST_CHECKED, 0);
			EnableWindow(GetDlgItem(hwnd, IDC_PLUGINS_DIR), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_PLUGINS_DIR), FALSE);
		}
		if (g_config.is_default_saves_directory_used)
		{
			SendMessage(GetDlgItem(hwnd, IDC_DEFAULT_SAVES_CHECK), BM_SETCHECK, BST_CHECKED, 0);
			EnableWindow(GetDlgItem(hwnd, IDC_SAVES_DIR), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_SAVES_DIR), FALSE);
		}
		if (g_config.is_default_screenshots_directory_used)
		{
			SendMessage(GetDlgItem(hwnd, IDC_DEFAULT_SCREENSHOTS_CHECK), BM_SETCHECK, BST_CHECKED, 0);
			EnableWindow(GetDlgItem(hwnd, IDC_SCREENSHOTS_DIR), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_SCREENSHOTS_DIR), FALSE);
		}
		if (g_config.is_default_backups_directory_used)
		{
			SendMessage(GetDlgItem(hwnd, IDC_DEFAULT_BACKUPS_CHECK), BM_SETCHECK, BST_CHECKED, 0);
			EnableWindow(GetDlgItem(hwnd, IDC_BACKUPS_DIR), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_BACKUPS_DIR), FALSE);
		}

		SetDlgItemText(hwnd, IDC_PLUGINS_DIR, g_config.plugins_directory.c_str());
		SetDlgItemText(hwnd, IDC_SAVES_DIR, g_config.saves_directory.c_str());
		SetDlgItemText(hwnd, IDC_SCREENSHOTS_DIR, g_config.screenshots_directory.c_str());
		SetDlgItemText(hwnd, IDC_BACKUPS_DIR, g_config.backups_directory.c_str());

		if (emu_launched)
		{
			EnableWindow(GetDlgItem(hwnd, IDC_DEFAULT_SAVES_CHECK), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_SAVES_DIR), FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_SAVES_DIR), FALSE);
		}
		break;
	case WM_NOTIFY:
		if (l_nmhdr->code == PSN_APPLY)
		{
			char str[MAX_PATH] = {0};
			int selected = SendDlgItemMessage(hwnd, IDC_DEFAULT_PLUGINS_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED
				               ? TRUE
				               : FALSE;
			GetDlgItemText(hwnd, IDC_PLUGINS_DIR, str, 200);
			g_config.plugins_directory = std::string(str);
			g_config.is_default_plugins_directory_used = selected;

			selected = SendDlgItemMessage(hwnd, IDC_DEFAULT_SAVES_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED
				           ? TRUE
				           : FALSE;
			GetDlgItemText(hwnd, IDC_SAVES_DIR, str, MAX_PATH);
			g_config.saves_directory = std::string(str);
			g_config.is_default_saves_directory_used = selected;

			selected = SendDlgItemMessage(hwnd, IDC_DEFAULT_SCREENSHOTS_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED
				           ? TRUE
				           : FALSE;
			GetDlgItemText(hwnd, IDC_SCREENSHOTS_DIR, str, MAX_PATH);
			g_config.screenshots_directory = std::string(str);
			g_config.is_default_screenshots_directory_used = selected;

			selected = SendDlgItemMessage(hwnd, IDC_DEFAULT_BACKUPS_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED
						   ? TRUE
						   : FALSE;
			GetDlgItemText(hwnd, IDC_BACKUPS_DIR, str, MAX_PATH);
			g_config.backups_directory = std::string(str);
			g_config.is_default_backups_directory_used = selected;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(w_param))
		{
		case IDC_RECURSION:
			g_config.is_rombrowser_recursion_enabled = SendDlgItemMessage(hwnd, IDC_RECURSION, BM_GETCHECK, 0, 0) ==
				BST_CHECKED;
			break;
		case IDC_ADD_BROWSER_DIR:
			{
				const auto path = show_persistent_folder_dialog("f_roms", hwnd);
				if (path.empty())
				{
					break;
				}
				g_config.rombrowser_rom_paths.push_back(wstring_to_string(path));
				build_rom_browser_path_list(hwnd);
				break;
			}
		case IDC_REMOVE_BROWSER_DIR:
			{
				if (const int32_t selected_index = SendMessage(GetDlgItem(hwnd, IDC_ROMBROWSER_DIR_LIST), LB_GETCURSEL, 0, 0); selected_index != -1)
				{
					g_config.rombrowser_rom_paths.erase(g_config.rombrowser_rom_paths.begin() + selected_index);
				}
				build_rom_browser_path_list(hwnd);
				break;
			}

		case IDC_REMOVE_BROWSER_ALL:
			g_config.rombrowser_rom_paths.clear();
			build_rom_browser_path_list(hwnd);
			break;

		case IDC_DEFAULT_PLUGINS_CHECK:
			{
				g_config.is_default_plugins_directory_used = SendMessage(GetDlgItem(hwnd, IDC_DEFAULT_PLUGINS_CHECK),
				                                                       BM_GETCHECK, 0, 0);
				EnableWindow(GetDlgItem(hwnd, IDC_PLUGINS_DIR), !g_config.is_default_plugins_directory_used);
				EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_PLUGINS_DIR), !g_config.is_default_plugins_directory_used);
			}
			break;
		case IDC_DEFAULT_BACKUPS_CHECK:
			{
				g_config.is_default_backups_directory_used = SendMessage(GetDlgItem(hwnd, IDC_DEFAULT_BACKUPS_CHECK),
																	   BM_GETCHECK, 0, 0);
				EnableWindow(GetDlgItem(hwnd, IDC_BACKUPS_DIR), !g_config.is_default_backups_directory_used);
				EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_BACKUPS_DIR), !g_config.is_default_backups_directory_used);
			}
			break;
		case IDC_PLUGIN_DIRECTORY_HELP:
			{
				MessageBox(hwnd, "Changing the plugin directory may introduce bugs to some plugins.", "Info",
				           MB_ICONINFORMATION | MB_OK);
			}
			break;
		case IDC_CHOOSE_PLUGINS_DIR:
			{
				const auto path = show_persistent_folder_dialog("f_plugins", hwnd);
				if (path.empty())
				{
					break;
				}
				g_config.plugins_directory = wstring_to_string(path) + "\\";
				SetDlgItemText(hwnd, IDC_PLUGINS_DIR, g_config.plugins_directory.c_str());
			}
			break;
		case IDC_DEFAULT_SAVES_CHECK:
			{
				g_config.is_default_saves_directory_used = SendMessage(GetDlgItem(hwnd, IDC_DEFAULT_SAVES_CHECK),
				                                                     BM_GETCHECK, 0, 0);
				EnableWindow(GetDlgItem(hwnd, IDC_SAVES_DIR), !g_config.is_default_saves_directory_used);
				EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_SAVES_DIR), !g_config.is_default_saves_directory_used);
			}
			break;
		case IDC_CHOOSE_SAVES_DIR:
			{
				const auto path = show_persistent_folder_dialog("f_saves", hwnd);
				if (path.empty())
				{
					break;
				}
				g_config.saves_directory = wstring_to_string(path) + "\\";
				SetDlgItemText(hwnd, IDC_SAVES_DIR, g_config.saves_directory.c_str());
			}
			break;
		case IDC_DEFAULT_SCREENSHOTS_CHECK:
			{
				g_config.is_default_screenshots_directory_used = SendMessage(
					GetDlgItem(hwnd, IDC_DEFAULT_SCREENSHOTS_CHECK), BM_GETCHECK, 0, 0);
				EnableWindow(GetDlgItem(hwnd, IDC_SCREENSHOTS_DIR), !g_config.is_default_screenshots_directory_used);
				EnableWindow(GetDlgItem(hwnd, IDC_CHOOSE_SCREENSHOTS_DIR),
				             !g_config.is_default_screenshots_directory_used);
			}
			break;
		case IDC_CHOOSE_SCREENSHOTS_DIR:
			{
				const auto path = show_persistent_folder_dialog("f_screenshots", hwnd);
				if (path.empty())
				{
					break;
				}
				g_config.screenshots_directory = wstring_to_string(path) + "\\";
				SetDlgItemText(hwnd, IDC_SCREENSHOTS_DIR, g_config.screenshots_directory.c_str());
			}
			break;
		case IDC_CHOOSE_BACKUPS_DIR:
			{
				const auto path = show_persistent_folder_dialog("f_backups", hwnd);
				if (path.empty())
				{
					break;
				}
				g_config.backups_directory = wstring_to_string(path) + "\\";
				SetDlgItemText(hwnd, IDC_BACKUPS_DIR, g_config.backups_directory.c_str());
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return FALSE;
}

void update_plugin_selection(const HWND hwnd, const int32_t id, const std::filesystem::path& path)
{
	for (int i = 0; i < SendDlgItemMessage(hwnd, id, CB_GETCOUNT, 0, 0); ++i)
	{
		if (const auto plugin = (Plugin*)SendDlgItemMessage(hwnd, id, CB_GETITEMDATA, i, 0); plugin->path() == path)
		{
			ComboBox_SetCurSel(GetDlgItem(hwnd, id), i);
			break;
		}
	}
	SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(id, 0), 0);
}

Plugin* get_selected_plugin(const HWND hwnd, const int id)
{
	const int i = SendDlgItemMessage(hwnd, id, CB_GETCURSEL, 0, 0);
	const auto res = SendDlgItemMessage(hwnd, id, CB_GETITEMDATA, i, 0);
	return res == CB_ERR ? nullptr : (Plugin*)res;
}

BOOL CALLBACK plugins_cfg(const HWND hwnd, const UINT message, const WPARAM w_param, const LPARAM l_param)
{
	[[maybe_unused]] char path_buffer[_MAX_PATH];
	NMHDR FAR* l_nmhdr = nullptr;
	memcpy(&l_nmhdr, &l_param, sizeof(NMHDR FAR*));
	switch (message)
	{
	case WM_CLOSE:
		EndDialog(hwnd, IDOK);
		break;
	case WM_INITDIALOG:
		{
			available_plugins = get_available_plugins();

			for (const auto& plugin : available_plugins)
			{
				int32_t id = 0;
				switch (plugin->type())
				{
				case plugin_type::video:
					id = IDC_COMBO_GFX;
					break;
				case plugin_type::audio:
					id = IDC_COMBO_SOUND;
					break;
				case plugin_type::input:
					id = IDC_COMBO_INPUT;
					break;
				case plugin_type::rsp:
					id = IDC_COMBO_RSP;
					break;
				default:
					assert(false);
					break;
				}
				// we add the string and associate a pointer to the plugin with the item
				const int i = SendDlgItemMessage(hwnd, id, CB_GETCOUNT, 0, 0);
				SendDlgItemMessage(hwnd, id, CB_ADDSTRING, 0, (LPARAM)plugin->name().c_str());
				SendDlgItemMessage(hwnd, id, CB_SETITEMDATA, i, (LPARAM)plugin.get());
			}

			update_plugin_selection(hwnd, IDC_COMBO_GFX, g_config.selected_video_plugin);
			update_plugin_selection(hwnd, IDC_COMBO_SOUND, g_config.selected_audio_plugin);
			update_plugin_selection(hwnd, IDC_COMBO_INPUT, g_config.selected_input_plugin);
			update_plugin_selection(hwnd, IDC_COMBO_RSP, g_config.selected_rsp_plugin);

			EnableWindow(GetDlgItem(hwnd, IDC_COMBO_GFX), !emu_launched);
			EnableWindow(GetDlgItem(hwnd, IDC_COMBO_INPUT), !emu_launched);
			EnableWindow(GetDlgItem(hwnd, IDC_COMBO_SOUND), !emu_launched);
			EnableWindow(GetDlgItem(hwnd, IDC_COMBO_RSP), !emu_launched);

			SendDlgItemMessage(hwnd, IDB_DISPLAY, STM_SETIMAGE, IMAGE_BITMAP,
			                   (LPARAM)LoadImage(g_app_instance, MAKEINTRESOURCE(IDB_DISPLAY),
			                                     IMAGE_BITMAP, 0, 0, 0));
			SendDlgItemMessage(hwnd, IDB_CONTROL, STM_SETIMAGE, IMAGE_BITMAP,
			                   (LPARAM)LoadImage(g_app_instance, MAKEINTRESOURCE(IDB_CONTROL),
			                                     IMAGE_BITMAP, 0, 0, 0));
			SendDlgItemMessage(hwnd, IDB_SOUND, STM_SETIMAGE, IMAGE_BITMAP,
			                   (LPARAM)LoadImage(g_app_instance, MAKEINTRESOURCE(IDB_SOUND),
			                                     IMAGE_BITMAP, 0, 0, 0));
			SendDlgItemMessage(hwnd, IDB_RSP, STM_SETIMAGE, IMAGE_BITMAP,
			                   (LPARAM)LoadImage(g_app_instance, MAKEINTRESOURCE(IDB_RSP),
			                                     IMAGE_BITMAP, 0, 0, 0));
			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(w_param))
		{
		case IDC_COMBO_GFX:
		case IDC_COMBO_SOUND:
		case IDC_COMBO_INPUT:
		case IDC_COMBO_RSP:
			{
				auto has_plugin_selected =
					ComboBox_GetItemData(GetDlgItem(hwnd, LOWORD(w_param)), ComboBox_GetCurSel(GetDlgItem(hwnd, LOWORD(w_param))))
					&& ComboBox_GetCurSel(GetDlgItem(hwnd, LOWORD(w_param))) != CB_ERR;

				switch (LOWORD(w_param))
				{
				case IDC_COMBO_GFX:
					EnableWindow(GetDlgItem(hwnd, IDM_VIDEO_SETTINGS), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDGFXTEST), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDGFXABOUT), has_plugin_selected);
					break;
				case IDC_COMBO_SOUND:
					EnableWindow(GetDlgItem(hwnd, IDM_AUDIO_SETTINGS), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDSOUNDTEST), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDSOUNDABOUT), has_plugin_selected);
					break;
				case IDC_COMBO_INPUT:
					EnableWindow(GetDlgItem(hwnd, IDM_INPUT_SETTINGS), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDINPUTTEST), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDINPUTABOUT), has_plugin_selected);
					break;
				case IDC_COMBO_RSP:
					EnableWindow(GetDlgItem(hwnd, IDM_RSP_SETTINGS), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDRSPTEST), has_plugin_selected);
					EnableWindow(GetDlgItem(hwnd, IDRSPABOUT), has_plugin_selected);
					break;
				default:
					break;
				}
				break;
			}
		case IDM_VIDEO_SETTINGS:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_GFX)->config();
			break;
		case IDGFXTEST:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_GFX)->test();
			break;
		case IDGFXABOUT:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_GFX)->about();
			break;
		case IDM_INPUT_SETTINGS:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_INPUT)->config();
			break;
		case IDINPUTTEST:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_INPUT)->test();
			break;
		case IDINPUTABOUT:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_INPUT)->about();
			break;
		case IDM_AUDIO_SETTINGS:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_SOUND)->config();
			break;
		case IDSOUNDTEST:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_SOUND)->test();
			break;
		case IDSOUNDABOUT:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_SOUND)->about();
			break;
		case IDM_RSP_SETTINGS:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_RSP)->config();
			break;
		case IDRSPTEST:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_RSP)->test();
			break;
		case IDRSPABOUT:
			g_hwnd_plug = hwnd;
			get_selected_plugin(hwnd, IDC_COMBO_RSP)->about();
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		if (l_nmhdr->code == PSN_APPLY)
		{
			if (const auto plugin = get_selected_plugin(hwnd, IDC_COMBO_GFX); plugin != nullptr)
			{
				g_config.selected_video_plugin = plugin->path().string();
			}
			if (const auto plugin = get_selected_plugin(hwnd, IDC_COMBO_SOUND); plugin != nullptr)
			{
				g_config.selected_audio_plugin = plugin->path().string();
			}
			if (const auto plugin = get_selected_plugin(hwnd, IDC_COMBO_INPUT); plugin != nullptr)
			{
				g_config.selected_input_plugin = plugin->path().string();
			}
			if (const auto plugin = get_selected_plugin(hwnd, IDC_COMBO_RSP); plugin != nullptr)
			{
				g_config.selected_rsp_plugin = plugin->path().string();
			}
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK general_cfg(const HWND hwnd, const UINT message, const WPARAM w_param, const LPARAM l_param)
{
	NMHDR FAR* l_nmhdr = nullptr;
	memcpy(&l_nmhdr, &l_param, sizeof(NMHDR FAR*));

	switch (message)
	{
	case WM_INITDIALOG:
		{
			tooltips.push_back(create_tooltip(hwnd, IDC_PAUSENOTACTIVE, "When checked, emulation pauses when the main window is not in focus"));
			tooltips.push_back(create_tooltip(hwnd, IDC_STATUSBARZEROINDEX, "When checked, Indicies in the statusbar, such as VCR frame counts, start at 0 instead of 1"));
			tooltips.push_back(create_tooltip(hwnd, IDC_SKIPFREQ, "0 = Skip all frames, 1 = Show all frames, n = show every nth frame"));
			tooltips.push_back(create_tooltip(hwnd, IDC_RECORD_RESETS, "When checked, mid-recording resets are recorded to the movie"));
			tooltips.push_back(create_tooltip(hwnd, IDC_CAPTUREDELAY, "Miliseconds to wait before capturing a frame. Useful for syncing with external programs"));
			tooltips.push_back(create_tooltip(hwnd, IDC_USESUMMERCART, "When checked, the core emulates an attached summercart"));
			tooltips.push_back(create_tooltip(hwnd, IDC_SAVE_VIDEO_TO_ST, "When checked, screenshots are saved to generated savestates. Only supported by MGE-implementing plugins"));
			tooltips.push_back(create_tooltip(hwnd, IDC_ENCODE_MODE, "The video source to use for capturing video frames"));
			tooltips.push_back(create_tooltip(hwnd, IDC_ENCODE_SYNC, "The strategy to use for synchronizing video and audio during capture"));
			tooltips.push_back(create_tooltip(hwnd, IDC_COMBO_LUA_PRESENTER, "The presenter type to use for displaying and capturing Lua graphics\nRecommended: DirectComposition (on Windows systems)"));
			tooltips.push_back(create_tooltip(hwnd, IDC_ENABLE_AUDIO_DELAY, "When checked, audio interrupts are delayed\nRecommended: On (Off causes issues with savestates)"));
			tooltips.push_back(create_tooltip(hwnd, IDC_ENABLE_COMPILED_JUMP, "When checked, jump instructions are compiled by Dynamic Recompiler\nRecommended: On (Off is slower)"));
			tooltips.push_back(create_tooltip(hwnd, IDC_ROUNDTOZERO, "When checked, floating point numbers are rounded towards zero"));
			tooltips.push_back(create_tooltip(hwnd, IDC_MOVIE_BACKUPS, "When checked, a backup of the currently playing movie is created when loading a savestate.\nRecommended: On (Off on systems with slow disk speeds)"));
			tooltips.push_back(create_tooltip(hwnd, IDC_AUTOINCREMENTSAVESLOT, "When checked, saving to a slot will increase the currently selected slot number by 1"));
			tooltips.push_back(create_tooltip(hwnd, IDC_EMULATEFLOATCRASHES, "When checked, float operations which crash on real hardware will crash the core"));
			tooltips.push_back(create_tooltip(hwnd, IDC_EDIT_MAX_LAG, "The maximum amount of lag frames before the core emits a warning\n0 = Disabled"));

			static const char* clock_speed_multiplier_names[] = {
				"1 - Default", "2 - 'Lagless'", "3", "4", "5", "6"
			};
			static const char* capture_mode_names[] = {
				"External capture", "Internal capture window", "Internal capture desktop", "Hybrid",
			};
			static const char* presenter_type_names[] = {
				"DirectComposition", "GDI",
			};
			static const char* capture_sync_names[] = {
				"No Sync", "Audio Sync", "Video Sync",
			};

			// Populate CPU Clock Speed Multiplier Dropdown Menu
			for (auto& clock_speed_multiplier_name : clock_speed_multiplier_names)
			{
				SendDlgItemMessage(hwnd, IDC_COMBO_CLOCK_SPD_MULT, CB_ADDSTRING, 0,
				                   (LPARAM)clock_speed_multiplier_name);
			}
			SendDlgItemMessage(hwnd, IDC_COMBO_CLOCK_SPD_MULT, CB_SETCURSEL, SendDlgItemMessage(hwnd, IDC_COMBO_CLOCK_SPD_MULT, CB_FINDSTRINGEXACT, -1,
			                                                                                    (LPARAM)clock_speed_multiplier_names[g_config.
				                                                                                    cpu_clock_speed_multiplier - 1]), 0);

			for (auto& name : capture_mode_names)
			{
				SendDlgItemMessage(hwnd, IDC_ENCODE_MODE, CB_ADDSTRING, 0,
				                   (LPARAM)name);
			}
			ComboBox_SetCurSel(GetDlgItem(hwnd, IDC_ENCODE_MODE), g_config.capture_mode);

			for (auto& name : presenter_type_names)
			{
				SendDlgItemMessage(hwnd, IDC_COMBO_LUA_PRESENTER, CB_ADDSTRING, 0,
								   (LPARAM)name);
			}
			ComboBox_SetCurSel(GetDlgItem(hwnd, IDC_COMBO_LUA_PRESENTER), g_config.presenter_type);

			for (auto& name : capture_sync_names)
			{
				SendDlgItemMessage(hwnd, IDC_ENCODE_SYNC, CB_ADDSTRING, 0,
				                   (LPARAM)name);
			}
			ComboBox_SetCurSel(GetDlgItem(hwnd, IDC_ENCODE_SYNC), g_config.synchronization_mode);

			SetDlgItemInt(hwnd, IDC_SKIPFREQ, g_config.frame_skip_frequency, 0);

			CheckDlgButton(hwnd, IDC_INTERP, g_config.core_type == 0 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwnd, IDC_RECOMP, g_config.core_type == 1 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwnd, IDC_PURE_INTERP, g_config.core_type == 2 ? BST_CHECKED : BST_UNCHECKED);

			EnableWindow(GetDlgItem(hwnd, IDC_INTERP), !emu_launched);
			EnableWindow(GetDlgItem(hwnd, IDC_RECOMP), !emu_launched);
			EnableWindow(GetDlgItem(hwnd, IDC_PURE_INTERP), !emu_launched);
			EnableWindow(GetDlgItem(hwnd, IDC_ENCODE_MODE), !EncodingManager::is_capturing());
			EnableWindow(GetDlgItem(hwnd, IDC_ENCODE_SYNC), !EncodingManager::is_capturing());
			EnableWindow(GetDlgItem(hwnd, IDC_COMBO_LUA_PRESENTER), hwnd_lua_map.empty());

			set_checkbox_state(hwnd, IDC_PAUSENOTACTIVE, g_config.is_unfocused_pause_enabled);
			set_checkbox_state(hwnd, IDC_AUTOINCREMENTSAVESLOT, g_config.increment_slot);
			set_checkbox_state(hwnd, IDC_STATUSBARZEROINDEX, g_config.vcr_0_index);
			set_checkbox_state(hwnd, IDC_USESUMMERCART, g_config.use_summercart);
			set_checkbox_state(hwnd, IDC_SAVE_VIDEO_TO_ST, g_config.st_screenshot);
			set_checkbox_state(hwnd, IDC_ROUNDTOZERO, g_config.is_round_towards_zero_enabled);
			set_checkbox_state(hwnd, IDC_MOVIE_BACKUPS, g_config.vcr_backups);
			set_checkbox_state(hwnd, IDC_EMULATEFLOATCRASHES, g_config.is_float_exception_propagation_enabled);
			set_checkbox_state(hwnd, IDC_ENABLE_AUDIO_DELAY, g_config.is_audio_delay_enabled);
			set_checkbox_state(hwnd, IDC_ENABLE_COMPILED_JUMP, g_config.is_compiled_jump_enabled);
			set_checkbox_state(hwnd, IDC_RECORD_RESETS, g_config.is_reset_recording_enabled);
			SetDlgItemInt(hwnd, IDC_CAPTUREDELAY, g_config.capture_delay, 0);
			SetDlgItemInt(hwnd, IDC_EDIT_MAX_LAG, g_config.max_lag, 0);

			return TRUE;
		}

	case WM_COMMAND:
		switch (LOWORD(w_param))
		{
		case IDC_SKIPFREQUENCY_HELP:
			MessageBox(hwnd, "0 = Skip all frames, 1 = Show all frames, n = show every nth frame", "Info",
			           MB_OK | MB_ICONINFORMATION);
			break;
		case IDC_COMBO_CLOCK_SPD_MULT:
			{
				char buf[260] = {0};
				read_combo_box_value(hwnd, IDC_COMBO_CLOCK_SPD_MULT, buf);
				g_config.cpu_clock_speed_multiplier = atoi(&buf[0]);
				break;
			}
		case IDC_ENCODE_MODE:
			{
				g_config.capture_mode = ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_ENCODE_MODE));
				break;
			}
		case IDC_ENCODE_SYNC:
			{
				g_config.synchronization_mode = ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_ENCODE_SYNC));
				break;
			}
	case IDC_COMBO_LUA_PRESENTER:
			{
				g_config.presenter_type = ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_COMBO_LUA_PRESENTER));
				break;
			}
		case IDC_INTERP:
			if (!emu_launched)
			{
				g_config.core_type = 0;
			}
			break;
		case IDC_RECOMP:
			if (!emu_launched)
			{
				g_config.core_type = 1;
			}
			break;
		case IDC_PURE_INTERP:
			if (!emu_launched)
			{
				g_config.core_type = 2;
			}
			break;
		default:
			break;
		}
		break;

	case WM_NOTIFY:
		if (l_nmhdr->code == PSN_APPLY)
		{
			g_config.frame_skip_frequency = (int)GetDlgItemInt(hwnd, IDC_SKIPFREQ, nullptr, 0);
			g_config.increment_slot = get_checkbox_state(hwnd, IDC_AUTOINCREMENTSAVESLOT);
			g_config.vcr_0_index = get_checkbox_state(hwnd, IDC_STATUSBARZEROINDEX);
			g_config.is_unfocused_pause_enabled = get_checkbox_state(hwnd, IDC_PAUSENOTACTIVE);
			g_config.use_summercart = get_checkbox_state(hwnd, IDC_USESUMMERCART);
			g_config.st_screenshot = get_checkbox_state(hwnd, IDC_SAVE_VIDEO_TO_ST);
			g_config.is_round_towards_zero_enabled = get_checkbox_state(hwnd, IDC_ROUNDTOZERO);
			g_config.vcr_backups = get_checkbox_state(hwnd, IDC_MOVIE_BACKUPS);
			g_config.is_float_exception_propagation_enabled = get_checkbox_state(hwnd, IDC_EMULATEFLOATCRASHES);
			g_config.is_audio_delay_enabled = get_checkbox_state(hwnd, IDC_ENABLE_AUDIO_DELAY);
			g_config.is_compiled_jump_enabled = get_checkbox_state(hwnd, IDC_ENABLE_COMPILED_JUMP);
			g_config.is_reset_recording_enabled = get_checkbox_state(hwnd, IDC_RECORD_RESETS);
			g_config.capture_delay = GetDlgItemInt(hwnd, IDC_CAPTUREDELAY, nullptr, 0);
			g_config.max_lag = GetDlgItemInt(hwnd, IDC_EDIT_MAX_LAG, nullptr, 0);
		}
		break;
	case WM_DESTROY:
		{
			for (auto tooltip : tooltips)
			{
				DestroyWindow(tooltip);
			}
			tooltips.clear();
			break;
		}
	default:
		return FALSE;
	}
	return TRUE;
}

std::string hotkey_to_string_overview(t_hotkey* hotkey)
{
	return hotkey->identifier + " (" + hotkey_to_string(hotkey) + ")";
}

void on_hotkey_selection_changed(const HWND dialog_hwnd)
{
	HWND list_hwnd = GetDlgItem(dialog_hwnd, IDC_HOTKEY_LIST);
	HWND enable_hwnd = GetDlgItem(dialog_hwnd, IDC_HOTKEY_CLEAR);
	HWND edit_hwnd = GetDlgItem(dialog_hwnd, IDC_SELECTED_HOTKEY_TEXT);
	HWND assign_hwnd = GetDlgItem(dialog_hwnd, IDC_HOTKEY_ASSIGN_SELECTED);

	char selected_identifier[MAX_PATH] = {0};
	SendMessage(list_hwnd, LB_GETTEXT, SendMessage(list_hwnd, LB_GETCURSEL, 0, 0), (LPARAM)selected_identifier);

	int32_t selected_index = ListBox_GetCurSel(list_hwnd);

	EnableWindow(enable_hwnd, selected_index != -1);
	EnableWindow(edit_hwnd, selected_index != -1);
	EnableWindow(assign_hwnd, selected_index != -1);

	SetWindowText(assign_hwnd, "Assign...");

	if (selected_index == -1)
	{
		SetWindowText(edit_hwnd, "");
		return;
	}

	auto hotkey = (t_hotkey*)ListBox_GetItemData(list_hwnd, selected_index);
	SetWindowText(edit_hwnd, hotkey_to_string(hotkey).c_str());
}

void build_hotkey_list(HWND hwnd)
{
	HWND list_hwnd = GetDlgItem(hwnd, IDC_HOTKEY_LIST);

	char search_query_c[MAX_PATH] = {0};
	GetDlgItemText(hwnd, IDC_HOTKEY_SEARCH, search_query_c, std::size(search_query_c));
	std::string search_query = search_query_c;

	ListBox_ResetContent(list_hwnd);

	for (t_hotkey* hotkey : g_config_hotkeys)
	{
		std::string hotkey_string = hotkey_to_string_overview(hotkey);

		if (!search_query.empty())
		{
			if (!contains(to_lower(hotkey_string), to_lower(search_query))
				&& !contains(to_lower(hotkey->identifier), to_lower(search_query)))
			{
				continue;
			}
		}

		int32_t index = ListBox_GetCount(list_hwnd);
		ListBox_AddString(list_hwnd, hotkey_string.c_str());
		ListBox_SetItemData(list_hwnd, index, hotkey);
	}
}

BOOL CALLBACK hotkeys_proc(const HWND hwnd, const UINT message, const WPARAM w_param, LPARAM)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			SetDlgItemText(hwnd, IDC_HOTKEY_SEARCH, "");
			on_hotkey_selection_changed(hwnd);
			return TRUE;
		}
	case WM_COMMAND:
		{
			HWND list_hwnd = GetDlgItem(hwnd, IDC_HOTKEY_LIST);
			const int event = HIWORD(w_param);
			const int id = LOWORD(w_param);
			switch (id)
			{
			case IDC_HOTKEY_LIST:
				if (event == LBN_SELCHANGE)
				{
					on_hotkey_selection_changed(hwnd);
				}
				if (event == LBN_DBLCLK)
				{
					on_hotkey_selection_changed(hwnd);
					SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_HOTKEY_ASSIGN_SELECTED, BN_CLICKED), (LPARAM)GetDlgItem(hwnd, IDC_HOTKEY_ASSIGN_SELECTED));
				}
				break;
			case IDC_HOTKEY_ASSIGN_SELECTED:
				{
					int32_t index = ListBox_GetCurSel(list_hwnd);
					if (index == -1)
					{
						break;
					}
					auto hotkey = (t_hotkey*)ListBox_GetItemData(list_hwnd, index);
					SetDlgItemText(hwnd, id, "...");
					get_user_hotkey(hotkey);

					build_hotkey_list(hwnd);
					ListBox_SetCurSel(list_hwnd, (index + 1) % ListBox_GetCount(list_hwnd));
					on_hotkey_selection_changed(hwnd);
				}
				break;
			case IDC_HOTKEY_SEARCH:
				{
					build_hotkey_list(hwnd);
				}
				break;
			case IDC_HOTKEY_CLEAR:
				{
					int32_t index = ListBox_GetCurSel(list_hwnd);
					if (index == -1)
					{
						break;
					}
					auto hotkey = (t_hotkey*)ListBox_GetItemData(list_hwnd, index);
					hotkey->key = 0;
					hotkey->ctrl = 0;
					hotkey->shift = 0;
					hotkey->alt = 0;
					build_hotkey_list(hwnd);
					on_hotkey_selection_changed(hwnd);
				}
				break;
			}
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void configdialog_show()
{
	PROPSHEETPAGE psp[4] = {{0}};
	for (auto& i : psp)
	{
		i.dwSize = sizeof(PROPSHEETPAGE);
		i.dwFlags = PSP_USETITLE;
		i.hInstance = g_app_instance;
	}

	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_MAIN);
	psp[0].pfnDlgProc = plugins_cfg;
	psp[0].pszTitle = "Plugins";

	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_DIRECTORIES);
	psp[1].pfnDlgProc = directories_cfg;
	psp[1].pszTitle = "Directories";

	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_MESSAGES);
	psp[2].pfnDlgProc = general_cfg;
	psp[2].pszTitle = "General";

	psp[3].pszTemplate = MAKEINTRESOURCE(IDD_NEW_HOTKEY_DIALOG);
	psp[3].pfnDlgProc = hotkeys_proc;
	psp[3].pszTitle = "Hotkeys";

	PROPSHEETHEADER psh = {0};
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	psh.hwndParent = g_main_hwnd;
	psh.hInstance = g_app_instance;
	psh.pszCaption = "Settings";
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;

	const t_config old_config = g_config;

	if (!PropertySheet(&psh))
	{
		g_config = old_config;
	}

	save_config();
	Messenger::broadcast(Messenger::Message::ConfigLoaded, nullptr);
}
