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


#include "main_win.h"
#include <commctrl.h>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <gdiplus.h>
#include <Shlwapi.h>
#include <Windows.h>

#include "Commandline.h"
#include "config.hpp"
#include "configdialog.h"
#include "CrashHelper.h"
#include "LuaConsole.h"
#include "Recent.h"
#include "timers.h"
#include "../guifuncs.h"
#include "../plugin.hpp"
#include "../rom.h"
#include "../savestates.h"
#include "../vcr.h"
#include "../../memory/memory.h"
#include "../../memory/pif.h"
#include "../../r4300/r4300.h"
#include "../../r4300/recomph.h"
#include "../../winproject/resource.h"
#include "../main/win/GameDebugger.h"
#include "features/RomBrowser.hpp"
#include "features/Statusbar.hpp"
#include "features/Toolbar.hpp"
#include "ffmpeg_capture/ffmpeg_capture.hpp"
#include "helpers/string_helpers.h"
#include "helpers/win_helpers.h"
#include "wrapper/PersistentPathDialog.h"

bool ffup = false;


#ifdef _MSC_VER
#define SNPRINTF	_snprintf
#define STRCASECMP	stricmp
#define STRNCASECMP	strnicmp
#endif


DWORD emu_id;
DWORD start_rom_id;
DWORD close_rom_id;
DWORD audio_thread_id;
HANDLE sound_thread_handle;
static BOOL FullScreenMode = 0;

HANDLE loading_handle[4];
HANDLE emu_thread_handle;
HWND hwnd_plug;
UINT update_screen_timer;

static DWORD WINAPI thread_func(LPVOID lp_param);
constexpr char g_sz_class_name[] = "myWindowClass";
char rom_path[MAX_PATH] = {0};
char last_selected_rom[_MAX_PATH];
bool scheduled_restart = false;
BOOL really_restart_mode = 0;
BOOL clear_sram_on_restart_mode = 0;
BOOL continue_vcr_on_restart_mode = 0;
BOOL just_restarted_flag = 0;
int frame_count = 1;
long long total_vi = 0;
static BOOL auto_pause = 0;
static BOOL menu_paused = 0;
[[maybe_unused]] static HWND h_static_handle; //Handle for static place
char temp_message[MAX_PATH];
int emu_launched; //int emu_emulating;
int emu_paused;
HWND main_hwnd;
HINSTANCE app_instance;
BOOL fast_forward = 0;
BOOL ignore_error_emulation = FALSE;
char statusmsg[800];

char corrected_path[260];
constexpr auto incompatible_plugins_amount = 1;// this is so bad;


constexpr char plugin_blacklist[incompatible_plugins_amount][256] = {
	"Azimer\'s Audio v0.7"
};

TCHAR core_names[3][30] = {
	TEXT("Interpreter"), TEXT("Dynamic Recompiler"), TEXT("Pure Interpreter")
};

std::string app_path;

/**
 * \brief List of lua environment map keys running before emulation stopped
 */
std::vector<HWND> previously_running_luas;

std::deque<std::function<void()>> dispatcher_queue;

void main_dispatcher_invoke(const std::function<void()>& func) {
	dispatcher_queue.push_back(func);
	SendMessage(main_hwnd, WM_EXECUTE_DISPATCHER, 0, 0);
}

void main_dispatcher_process()
{
	while (!dispatcher_queue.empty()) {
		dispatcher_queue.front()();
		dispatcher_queue.pop_front();
	}
}

void clear_buttons()
{
	constexpr BUTTONS zero = {0};
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

void set_is_movie_loop_enabled(const bool value)
{
	Config.is_movie_loop_enabled = value;

	CheckMenuItem(GetMenu(main_hwnd), ID_LOOP_MOVIE,
				  MF_BYCOMMAND | (Config.is_movie_loop_enabled
									  ? MFS_CHECKED
									  : MFS_UNCHECKED));

	if (emu_launched)
		statusbar_post_text(Config.is_movie_loop_enabled
								 ? "Movies restart after ending"
								 : "Movies stop after ending");
}

static void gui_change_window()
{
	if (FullScreenMode)
	{
		ShowCursor(FALSE);
		changeWindow();
	} else
	{
		changeWindow();
		ShowCursor(TRUE);
	}
	toolbar_set_visibility(!FullScreenMode);
	statusbar_set_visibility(!FullScreenMode);
}

void resume_emu(const BOOL quiet)
{
	const BOOL was_paused = emu_paused;
	if (emu_launched)
	{
		emu_paused = 0;
		ResumeThread(sound_thread_handle);
		if (!quiet)
			statusbar_post_text("Emulation started");
	}

	toolbar_on_emu_state_changed(emu_launched, 1);

	if (emu_paused != was_paused && !quiet)
		CheckMenuItem(GetMenu(main_hwnd), EMU_PAUSE,
		              MF_BYCOMMAND | (emu_paused
			                              ? MFS_CHECKED
			                              : MFS_UNCHECKED));
}


void pause_emu(const BOOL quiet)
{
	const BOOL was_paused = emu_paused;
	if (emu_launched)
	{
		vcr_update_statusbar();
		emu_paused = 1;
		if (!quiet)
			// HACK (not a typo) seems to help avoid a race condition that permanently disables sound when doing frame advance
			SuspendThread(sound_thread_handle);
		if (!quiet)
			statusbar_post_text("Emulation paused");
	} else
	{
		CheckMenuItem(GetMenu(main_hwnd), EMU_PAUSE,
		              MF_BYCOMMAND | MFS_UNCHECKED);
	}

	toolbar_on_emu_state_changed(emu_launched, 0);

	if (emu_paused != was_paused && !menu_paused)
		CheckMenuItem(GetMenu(main_hwnd), EMU_PAUSE,
		              MF_BYCOMMAND | (emu_paused
			                              ? MFS_CHECKED
			                              : MFS_UNCHECKED));
}

DWORD WINAPI start_rom(const LPVOID lp_param)
{
	// maybe make no access violations, probably can be just
	// replaced with rom_path instead of rom_path_local
	const char* rom_path_local;
	if (lp_param != nullptr) {
		rom_path_local = (char*)lp_param;
	} else {
		rom_path_local = rom_path;
	}
	const auto start_time = std::chrono::high_resolution_clock::now();

	// Kill any roms that are still running
	if (emu_launched) {
		WaitForSingleObject(CreateThread(nullptr, 0, close_rom, nullptr, 0, &close_rom_id), 10'000);
	}

	// TODO: keep plugins loaded and only unload and reload them when they actually change
	printf("Loading plugins\n");
	if (!load_plugins())
	{
		MessageBox(main_hwnd, "Invalid plugins selected", nullptr,
		   MB_ICONERROR | MB_OK);
		return 0;
	}

	// valid rom is required to start emulation
	if (rom_read(rom_path_local))
	{
		MessageBox(main_hwnd, "Failed to open ROM", "Error",
				   MB_ICONERROR | MB_OK);
		unload_plugins();
		return 0;
	}
	// at this point, we're set to begin emulating and can't backtrack
	// disallow window resizing
	const LONG style = GetWindowLong(main_hwnd, GWL_STYLE);
	SetWindowLong(main_hwnd, GWL_STYLE,
				  style & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
	// TODO: investigate wtf this is
	strcpy(last_selected_rom, rom_path_local);

	// notify ui of emu state change
	main_recent_roms_add(rom_path_local);
	rombrowser_set_visibility(0);
	statusbar_set_mode(statusbar_mode::emulating);
	enable_emulation_menu_items(TRUE);
	timer_init();
	if (m_task == e_task::idle) {
		SetWindowText(main_hwnd, std::format("{} - {}", std::string(mupen_version), std::string((char*)ROM_HEADER.nom)).c_str());
	}

	auto gfx_thread = std::thread(load_gfx, video_plugin->handle);
	auto audio_thread = std::thread(load_audio, audio_plugin->handle);
	auto input_thread = std::thread(load_input, input_plugin->handle);
	auto rsp_thread = std::thread(load_rsp, rsp_plugin->handle);

	gfx_thread.join();
	audio_thread.join();
	input_thread.join();
	rsp_thread.join();

	printf("start_rom entry %dms\n", static_cast<int>((std::chrono::high_resolution_clock::now() - start_time).count() / 1'000'000));
	emu_thread_handle = CreateThread(nullptr, 0, thread_func, nullptr, 0, &emu_id);

	return 1;
}

static int shut_window = 0;

DWORD WINAPI close_rom(LPVOID)
{
	//printf("gen interrupt: %lld ns/vi", (total_vi/frame_count));
	if (emu_launched) {

		if (emu_paused) {
			menu_paused = FALSE;
			resume_emu(FALSE);
		}

		if (vcr_is_capturing() && !continue_vcr_on_restart_mode) {
			// we need to stop capture before closing rom because rombrowser might show up in recording otherwise lol
			if (vcr_stop_capture() != 0)
				MessageBox(nullptr, "Couldn't stop capturing", "VCR", MB_OK);
			else {
				SetWindowPos(main_hwnd, HWND_TOP, 0, 0, 0, 0,
							 SWP_NOMOVE | SWP_NOSIZE);
				statusbar_post_text("Stopped AVI capture");
			}
		}

		// remember all running lua scripts' HWNDs
		for (const auto key : hwnd_lua_map | std::views::keys)
		{
			previously_running_luas.push_back(key);
		}

		printf("Closing emulation thread...\n");

		// we signal the core to stop, then wait until thread exits
		stop_it();
		main_dispatcher_invoke(stop_all_scripts);
		stop = 1;
		if (const DWORD result = WaitForSingleObject(emu_thread_handle, 10'000); result == WAIT_TIMEOUT) {
			MessageBox(main_hwnd, "Emu thread didn't exit in time", nullptr,
					   MB_ICONERROR | MB_OK);
		}

		emu_launched = 0;
		emu_paused = 1;


		rom = nullptr;
		free(rom);

		free_memory();

		enable_emulation_menu_items(FALSE);
		rombrowser_set_visibility(!really_restart_mode);
		toolbar_on_emu_state_changed(0, 0);

		if (m_task == e_task::idle) {
			SetWindowText(main_hwnd, mupen_version);
			// TODO: look into why this is done
			statusbar_post_text(" ", 1);
		}

		if (shut_window) {
			SendMessage(main_hwnd, WM_CLOSE, 0, 0);
			return 0;
		}

		statusbar_set_mode(statusbar_mode::rombrowser);
		statusbar_post_text("Emulation stopped");

		SetWindowLong(main_hwnd, GWL_STYLE,
					  GetWindowLong(main_hwnd, GWL_STYLE) | WS_THICKFRAME);

		if (really_restart_mode) {
			if (clear_sram_on_restart_mode) {
				vcr_clear_save_data();
				clear_sram_on_restart_mode = FALSE;
			}

			really_restart_mode = FALSE;
			if (m_task != e_task::idle)
				just_restarted_flag = TRUE;

			main_dispatcher_invoke([] {
				strcpy(rom_path, last_selected_rom);
				CreateThread(nullptr, 0, start_rom, rom_path, 0, &start_rom_id);
				//if (!start_rom(nullptr)) {
					//close_rom(NULL);
					//MessageBox(main_hwnd, "Failed to open ROM", NULL,
							//   MB_ICONERROR | MB_OK);
				//}
			});
		}


		continue_vcr_on_restart_mode = FALSE;
		ExitThread(0);
	}
	ExitThread(0);
}

void reset_emu()
{
	// why is it so damned difficult to reset the game?
	// right now it's hacked to exit to the GUI then re-load the ROM,
	// but it should be possible to reset the game while it's still running
	// simply by clearing out some memory and maybe notifying the plugins...
	if (emu_launched)
	{
		frame_advancing = false;
		really_restart_mode = TRUE;
		menu_paused = FALSE;
		CreateThread(nullptr, 0, close_rom, nullptr, 0, &close_rom_id);
	}
}

int pause_at_frame = -1;

/// <summary>
/// Helper function because this is repeated multiple times
/// </summary>
void set_status_playback_started()
{
	const HMENU h_menu = GetMenu(main_hwnd);
	EnableMenuItem(h_menu, ID_STOP_RECORD, MF_GRAYED);
	EnableMenuItem(h_menu, ID_STOP_PLAYBACK, MF_ENABLED);

	if (!emu_paused || !emu_launched)
		statusbar_post_text("Playback started");
	else
		statusbar_post_text("Playback started while paused");
}

LRESULT CALLBACK play_movie_proc(const HWND hwnd, const UINT message, const WPARAM w_param, LPARAM)
{
	char tempbuf[MAX_PATH];
	HWND description_dialog;
	HWND author_dialog;
	[[maybe_unused]] static char path_buffer[_MAX_PATH];
	switch (message)
	{
	case WM_INITDIALOG:
		description_dialog = GetDlgItem(hwnd, IDC_INI_DESCRIPTION);
		author_dialog = GetDlgItem(hwnd, IDC_INI_AUTHOR);
		SetDlgItemInt(hwnd, IDC_PAUSEAT_FIELD, 0, FALSE);

		SendMessage(description_dialog, EM_SETLIMITTEXT,
		            movie_description_data_size, 0);
		SendMessage(author_dialog, EM_SETLIMITTEXT, movie_author_data_size, 0);

		sprintf(tempbuf, "%s (%s)", (char*)ROM_HEADER.nom, country_code_to_country_name(ROM_HEADER.Country_code).c_str());
		strcat(tempbuf, ".m64");
		SetDlgItemText(hwnd, IDC_INI_MOVIEFILE, tempbuf);

		SetDlgItemText(hwnd, IDC_ROM_INTERNAL_NAME2, (CHAR*)ROM_HEADER.nom);

		SetDlgItemText(hwnd, IDC_ROM_COUNTRY2, country_code_to_country_name(ROM_HEADER.Country_code).c_str());

		sprintf(tempbuf, "%X", (unsigned int)ROM_HEADER.CRC1);
		SetDlgItemText(hwnd, IDC_ROM_CRC3, tempbuf);

		SetDlgItemText(hwnd, IDC_MOVIE_VIDEO_TEXT2,
					   video_plugin->name.c_str());
		SetDlgItemText(hwnd, IDC_MOVIE_INPUT_TEXT2,
					   input_plugin->name.c_str());
		SetDlgItemText(hwnd, IDC_MOVIE_SOUND_TEXT2,
					   audio_plugin->name.c_str());
		SetDlgItemText(hwnd, IDC_MOVIE_RSP_TEXT2,
					   rsp_plugin->name.c_str());
		// TODO: do with for loop

		strcpy(tempbuf, controls[0].Present ? "Present" : "Disconnected");
		if (controls[0].Present && controls[0].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[0].Present && controls[0].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER1_TEXT2, tempbuf);

		strcpy(tempbuf, controls[1].Present ? "Present" : "Disconnected");
		if (controls[1].Present && controls[1].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[1].Present && controls[1].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER2_TEXT2, tempbuf);

		strcpy(tempbuf, controls[2].Present ? "Present" : "Disconnected");
		if (controls[2].Present && controls[2].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[2].Present && controls[2].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER3_TEXT2, tempbuf);

		strcpy(tempbuf, controls[3].Present ? "Present" : "Disconnected");
		if (controls[3].Present && controls[3].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[3].Present && controls[3].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER4_TEXT2, tempbuf);

		CheckDlgButton(hwnd, IDC_MOVIE_READONLY, vcr_get_read_only());
		SetFocus(GetDlgItem(hwnd, IDC_INI_MOVIEFILE));
		goto refresh;
	// better than making it a macro or zillion-argument function

	case WM_CLOSE:
		EndDialog(hwnd, IDOK);
		break;
	case WM_COMMAND:
		switch (LOWORD(w_param))
		{
		case IDC_OK:
		case IDOK:
			{
				vcr_stop_all();

				{
					BOOL success;
					if (const unsigned int frame = GetDlgItemInt(
						hwnd, IDC_PAUSEAT_FIELD, &success, TRUE); success && frame > 0)
						pause_at_frame = (int)frame;
					else
						pause_at_frame = -1;
				}

				GetDlgItemText(hwnd, IDC_INI_MOVIEFILE, tempbuf, MAX_PATH);

				vcr_set_read_only(
					(BOOL)IsDlgButtonChecked(hwnd, IDC_MOVIE_READONLY));

				vcr_start_playback(tempbuf, false);

				set_status_playback_started();
				resume_emu(TRUE);
				EndDialog(hwnd, IDOK);
			}
			break;
		case IDC_CANCEL:
		case IDCANCEL:
			EndDialog(hwnd, IDOK);
			break;
		case IDC_MOVIE_BROWSE:
			{
				const auto path = show_persistent_open_dialog("o_movie", hwnd, L"*.m64;*.rec");
				if (path.empty())
				{
					break;
				}
				SetDlgItemText(hwnd, IDC_INI_MOVIEFILE, wstring_to_string(path).c_str());
			}
			goto refresh;
		case IDC_MOVIE_REFRESH:
			goto refresh;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return FALSE;

refresh:

	GetDlgItemText(hwnd, IDC_INI_MOVIEFILE, tempbuf, MAX_PATH);
	t_movie_header hdr = {0};
	if(auto buf = read_file_buffer(tempbuf); !vcr_parse_header(buf, &hdr))
	{
		return FALSE;
	}

	SetDlgItemText(hwnd, IDC_ROM_INTERNAL_NAME, hdr.rom_name);
 	SetDlgItemText(hwnd, IDC_ROM_COUNTRY, country_code_to_country_name(hdr.rom_country).c_str());
	SetDlgItemText(hwnd, IDC_ROM_CRC, std::format("{:#04x}", (unsigned int)hdr.rom_crc1).c_str());
	SetDlgItemText(hwnd, IDC_MOVIE_VIDEO_TEXT, hdr.video_plugin_name);
	SetDlgItemText(hwnd, IDC_MOVIE_INPUT_TEXT, hdr.input_plugin_name);
	SetDlgItemText(hwnd, IDC_MOVIE_SOUND_TEXT, hdr.audio_plugin_name);
	SetDlgItemText(hwnd, IDC_MOVIE_RSP_TEXT, hdr.rsp_plugin_name);
	for (int i = 0; i < 4; ++i)
	{
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER1_TEXT + i, std::format("{}{}{}",
		hdr.controller_flags & CONTROLLER_X_PRESENT(i) ? "Connected" : "Disconnected",
		hdr.controller_flags & CONTROLLER_X_MEMPAK(i) ? ", Mempak" : "",
		hdr.controller_flags & CONTROLLER_X_RUMBLE(i) ? ", Rumblepak" : ""
		).c_str());
	}

	if (hdr.start_flags & movie_start_from_snapshot)
	{
		SetDlgItemText(hwnd, IDC_FROMSNAPSHOT_TEXT, "Savestate");
	}
	if (hdr.start_flags & movie_start_from_eeprom)
	{
		SetDlgItemText(hwnd, IDC_FROMSNAPSHOT_TEXT, "EEPROM");
	}
	if (hdr.start_flags & movie_start_from_nothing)
	{
		SetDlgItemText(hwnd, IDC_FROMSNAPSHOT_TEXT, "Start");
	}
	SetDlgItemText(hwnd, IDC_MOVIE_FRAMES, std::format("{} ({})", hdr.length_vis, hdr.length_samples).c_str());
	SetDlgItemText(hwnd, IDC_MOVIE_RERECORDS, std::to_string(hdr.rerecord_count).c_str());
	SetWindowTextW(GetDlgItem(hwnd, IDC_INI_AUTHOR), string_to_wstring(hdr.author).c_str());
	SetWindowTextW(GetDlgItem(hwnd, IDC_INI_DESCRIPTION), string_to_wstring(hdr.description).c_str());
	SetDlgItemText(hwnd, IDC_MOVIE_LENGTH, format_duration((double)hdr.length_vis / (double)hdr.vis_per_second).c_str());
	return FALSE;
}

LRESULT CALLBACK record_movie_proc(const HWND hwnd, const UINT message, const WPARAM w_param, LPARAM)
{
	char tempbuf[MAX_PATH];
	int checked_movie_type;
	HWND description_dialog;
	HWND author_dialog;

	switch (message)
	{
	case WM_INITDIALOG:

		checked_movie_type = Config.last_movie_type;
		description_dialog = GetDlgItem(hwnd, IDC_INI_DESCRIPTION);
		author_dialog = GetDlgItem(hwnd, IDC_INI_AUTHOR);

		SendMessage(description_dialog, EM_SETLIMITTEXT,
		            movie_description_data_size, 0);
		SendMessage(author_dialog, EM_SETLIMITTEXT, movie_author_data_size, 0);

		SetDlgItemText(hwnd, IDC_INI_AUTHOR, Config.last_movie_author.c_str());
		SetDlgItemText(hwnd, IDC_INI_DESCRIPTION, "");

		CheckRadioButton(hwnd, IDC_FROMSNAPSHOT_RADIO, IDC_FROMSTART_RADIO,
		                 checked_movie_type);

		sprintf(tempbuf, "%s (%s)", (char*)ROM_HEADER.nom, country_code_to_country_name(ROM_HEADER.Country_code).c_str());
		strcat(tempbuf, ".m64");
		SetDlgItemText(hwnd, IDC_INI_MOVIEFILE, tempbuf);

		SetDlgItemText(hwnd, IDC_ROM_INTERNAL_NAME2, (CHAR*)ROM_HEADER.nom);

		SetDlgItemText(hwnd, IDC_ROM_COUNTRY2, country_code_to_country_name(ROM_HEADER.Country_code).c_str());

		sprintf(tempbuf, "%X", (unsigned int)ROM_HEADER.CRC1);
		SetDlgItemText(hwnd, IDC_ROM_CRC3, tempbuf);

		SetDlgItemText(hwnd, IDC_MOVIE_VIDEO_TEXT2,
		               video_plugin->name.c_str());
		SetDlgItemText(hwnd, IDC_MOVIE_INPUT_TEXT2,
		               input_plugin->name.c_str());
		SetDlgItemText(hwnd, IDC_MOVIE_SOUND_TEXT2,
		               audio_plugin->name.c_str());
		SetDlgItemText(hwnd, IDC_MOVIE_RSP_TEXT2,
		               rsp_plugin->name.c_str());

		//TODO: do with for loop
		strcpy(tempbuf, controls[0].Present ? "Present" : "Disconnected");
		if (controls[0].Present && controls[0].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[0].Present && controls[0].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER1_TEXT2, tempbuf);

		strcpy(tempbuf, controls[1].Present ? "Present" : "Disconnected");
		if (controls[1].Present && controls[1].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[1].Present && controls[1].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER2_TEXT2, tempbuf);

		strcpy(tempbuf, controls[2].Present ? "Present" : "Disconnected");
		if (controls[2].Present && controls[2].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[2].Present && controls[2].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER3_TEXT2, tempbuf);

		strcpy(tempbuf, controls[3].Present ? "Present" : "Disconnected");
		if (controls[3].Present && controls[3].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (controls[3].Present && controls[3].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER4_TEXT2, tempbuf);

		EnableWindow(GetDlgItem(hwnd, IDC_EXTSAVESTATE), 0);
	// workaround because initial selected button is "Start"

		SetFocus(GetDlgItem(hwnd, IDC_INI_AUTHOR));

		return FALSE;
	case WM_CLOSE:
		EndDialog(hwnd, IDOK);
		break;
	case WM_COMMAND:
		switch (LOWORD(w_param))
		{
		case IDC_OK:
		case IDOK:
			{
				// turn WCHAR into UTF8
				char author_utf8[movie_author_data_size * 4];
				if (WCHAR author_wc[movie_author_data_size]; GetDlgItemTextW(hwnd, IDC_INI_AUTHOR, author_wc,
				                                                            movie_author_data_size))
					WideCharToMultiByte(CP_UTF8, 0, author_wc, -1, author_utf8,
					                    sizeof(author_utf8), nullptr, nullptr);
				else
					GetDlgItemTextA(hwnd, IDC_INI_AUTHOR, author_utf8,
					                movie_author_data_size);

				Config.last_movie_author = std::string(author_utf8);

				char description_utf8[movie_description_data_size * 4];
				if (WCHAR description_wc[movie_description_data_size]; GetDlgItemTextW(hwnd, IDC_INI_DESCRIPTION, description_wc,
				                                                                      movie_description_data_size))
					WideCharToMultiByte(CP_UTF8, 0, description_wc, -1,
					                    description_utf8,
					                    sizeof(description_utf8), nullptr, nullptr);
				else
					GetDlgItemTextA(hwnd, IDC_INI_DESCRIPTION, description_utf8,
					                movie_description_data_size);


				GetDlgItemText(hwnd, IDC_INI_MOVIEFILE, tempbuf, MAX_PATH);

				// big
				checked_movie_type =
					IsDlgButtonChecked(hwnd, IDC_FROMSNAPSHOT_RADIO)
						? IDC_FROMSNAPSHOT_RADIO
						: IsDlgButtonChecked(hwnd, IDC_FROMSTART_RADIO)
						? IDC_FROMSTART_RADIO
						: IsDlgButtonChecked(hwnd, IDC_FROMEEPROM_RADIO)
						? IDC_FROMEEPROM_RADIO
						: IDC_FROMEXISTINGSNAPSHOT_RADIO;
				const unsigned short flag = checked_movie_type ==
				                            IDC_FROMSNAPSHOT_RADIO
					                            ? movie_start_from_snapshot
					                            : checked_movie_type ==
					                            IDC_FROMSTART_RADIO
					                            ? movie_start_from_nothing
					                            : checked_movie_type ==
					                            IDC_FROMEEPROM_RADIO
					                            ? movie_start_from_eeprom
					                            : movie_start_from_existing_snapshot;
				Config.last_movie_type = checked_movie_type;

				if (flag == movie_start_from_existing_snapshot)
				{
					// The default directory we open the file dialog window in is the
					// parent directory of the last savestate that the user saved or loaded
					const std::string path = wstring_to_string(show_persistent_open_dialog("o_movie_existing_snapshot", hwnd, L"*.st;*.savestate"));

					if (path.empty())
					{
						break;
					}

					st_path = path;
					st_medium = e_st_medium::path;

					const std::string movie_path = strip_extension(path) + ".m64"; // shadows variable outer scope

					strcpy(tempbuf, movie_path.c_str());

					if (std::filesystem::exists(movie_path))
					{
						char tempbuf2[MAX_PATH];
						sprintf(tempbuf2,
						        "\"%s\" already exists. Are you sure want to overwrite this movie?",
						        tempbuf);
						if (MessageBox(hwnd, tempbuf2, "VCR", MB_YESNO) ==
							IDNO)
							break;
					}
				}
				vcr_start_record(tempbuf, flag);

				const HMENU h_menu = GetMenu(main_hwnd);
				EnableMenuItem(h_menu, ID_STOP_RECORD, MF_ENABLED);
				EnableMenuItem(h_menu, ID_STOP_PLAYBACK, MF_GRAYED);
				statusbar_post_text("Recording replay");

				EndDialog(hwnd, IDOK);
				
			}
			break;
		case IDC_CANCEL:
		case IDCANCEL:
			EndDialog(hwnd, IDOK);
			break;
		case IDC_MOVIE_BROWSE:
			{
				const auto path = show_persistent_save_dialog("s_movie", hwnd, L"*.m64;*.rec");

				if (path.empty())
				{
					break;
				}

				SetDlgItemText(hwnd, IDC_INI_MOVIEFILE, wstring_to_string(path).c_str());
			}
			break;

		case IDC_FROMEEPROM_RADIO:
			EnableWindow(GetDlgItem(hwnd, IDC_EXTSAVESTATE), 0);
			EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 1);
			break;
		case IDC_FROMSNAPSHOT_RADIO:
			EnableWindow(GetDlgItem(hwnd, IDC_EXTSAVESTATE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 1);
			break;
		case IDC_FROMEXISTINGSNAPSHOT_RADIO:
			EnableWindow(GetDlgItem(hwnd, IDC_EXTSAVESTATE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 0);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 0);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 0);
			break;
		case IDC_FROMSTART_RADIO:
			EnableWindow(GetDlgItem(hwnd, IDC_EXTSAVESTATE), 0);
			EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 1);
			EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 1);
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

void open_movie_playback_dialog()
{
	const BOOL was_paused = emu_paused && !menu_paused;
	menu_paused = FALSE;
	if (emu_launched && !emu_paused)
		pause_emu(FALSE);

	DialogBox(GetModuleHandle(NULL),
	          MAKEINTRESOURCE(IDD_MOVIE_PLAYBACK_DIALOG), main_hwnd,
	          (DLGPROC)play_movie_proc);

	if (emu_launched && emu_paused && !was_paused)
		resume_emu(FALSE);
}

void open_movie_record_dialog()
{
	const BOOL was_paused = emu_paused && !menu_paused;
	menu_paused = FALSE;
	if (emu_launched && !emu_paused)
		pause_emu(FALSE);

	DialogBox(GetModuleHandle(NULL),
	          MAKEINTRESOURCE(IDD_MOVIE_RECORD_DIALOG), main_hwnd,
	          (DLGPROC)record_movie_proc);

	if (emu_launched && emu_paused && !was_paused)
		resume_emu(FALSE);
}



void enable_emulation_menu_items(const BOOL emulation_running)
{
	const HMENU h_menu = GetMenu(main_hwnd);

	if (emulation_running)
	{
		EnableMenuItem(h_menu, EMU_STOP, MF_ENABLED);
		EnableMenuItem(h_menu, EMU_PAUSE, MF_ENABLED);
		EnableMenuItem(h_menu, EMU_FRAMEADVANCE, MF_ENABLED);
		EnableMenuItem(h_menu, ID_LOAD_LATEST, MF_ENABLED);
		EnableMenuItem(h_menu, EMU_PLAY, MF_ENABLED);
		EnableMenuItem(h_menu, FULL_SCREEN, MF_ENABLED);
		EnableMenuItem(h_menu, STATE_SAVE, MF_ENABLED);
		EnableMenuItem(h_menu, STATE_SAVEAS, MF_ENABLED);
		EnableMenuItem(h_menu, STATE_RESTORE, MF_ENABLED);
		EnableMenuItem(h_menu, STATE_LOAD, MF_ENABLED);
		EnableMenuItem(h_menu, GENERATE_BITMAP, MF_ENABLED);
		EnableMenuItem(h_menu, EMU_RESET, MF_ENABLED);
		EnableMenuItem(h_menu, REFRESH_ROM_BROWSER, MF_GRAYED);
		EnableMenuItem(h_menu, ID_RESTART_MOVIE, MF_ENABLED);
		EnableMenuItem(h_menu, ID_AUDIT_ROMS, MF_GRAYED);
		EnableMenuItem(h_menu, ID_FFMPEG_START, MF_DISABLED);
		EnableMenuItem(h_menu, IDC_GUI_TOOLBAR, MF_DISABLED);
		EnableMenuItem(h_menu, IDC_GUI_STATUSBAR, MF_DISABLED);

		if (dynacore)
			EnableMenuItem(h_menu, ID_TRACELOG, MF_DISABLED);
		else
			EnableMenuItem(h_menu, ID_TRACELOG, MF_ENABLED);

		if (!continue_vcr_on_restart_mode)
		{
			EnableMenuItem(h_menu, ID_START_RECORD, MF_ENABLED);
			EnableMenuItem(h_menu, ID_STOP_RECORD,
			               vcr_is_recording() ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(h_menu, ID_START_PLAYBACK, MF_ENABLED);
			EnableMenuItem(h_menu, ID_STOP_PLAYBACK,
			               (vcr_is_restarting() || vcr_is_playing())
				               ? MF_ENABLED
				               : MF_GRAYED);
			EnableMenuItem(h_menu, ID_START_CAPTURE, MF_ENABLED);
			EnableMenuItem(h_menu, ID_START_CAPTURE_PRESET, MF_ENABLED);
			EnableMenuItem(h_menu, ID_END_CAPTURE,
			               vcr_is_capturing() ? MF_ENABLED : MF_GRAYED);
		}

		toolbar_on_emu_state_changed(1, 1);
	} else
	{
		EnableMenuItem(h_menu, EMU_STOP, MF_GRAYED);
		EnableMenuItem(h_menu, IDLOAD, MF_ENABLED);
		EnableMenuItem(h_menu, EMU_PAUSE, MF_GRAYED);
		EnableMenuItem(h_menu, EMU_FRAMEADVANCE, MF_GRAYED);
		EnableMenuItem(h_menu, ID_LOAD_LATEST, MF_GRAYED);
		EnableMenuItem(h_menu, EMU_PLAY, MF_GRAYED);
		EnableMenuItem(h_menu, FULL_SCREEN, MF_GRAYED);
		EnableMenuItem(h_menu, STATE_SAVE, MF_GRAYED);
		EnableMenuItem(h_menu, STATE_SAVEAS, MF_GRAYED);
		EnableMenuItem(h_menu, STATE_RESTORE, MF_GRAYED);
		EnableMenuItem(h_menu, STATE_LOAD, MF_GRAYED);
		EnableMenuItem(h_menu, GENERATE_BITMAP, MF_GRAYED);
		EnableMenuItem(h_menu, EMU_RESET, MF_GRAYED);
		EnableMenuItem(h_menu, REFRESH_ROM_BROWSER, MF_ENABLED);
		EnableMenuItem(h_menu, ID_RESTART_MOVIE, MF_GRAYED);
		EnableMenuItem(h_menu, ID_TRACELOG, MF_DISABLED);
		EnableMenuItem(h_menu, ID_AUDIT_ROMS, MF_ENABLED);
		EnableMenuItem(h_menu, ID_FFMPEG_START, MF_GRAYED);
		EnableMenuItem(h_menu, IDC_GUI_TOOLBAR, MF_ENABLED);
		EnableMenuItem(h_menu, IDC_GUI_STATUSBAR, MF_ENABLED);

		if (!continue_vcr_on_restart_mode)
		{
			EnableMenuItem(h_menu, ID_START_RECORD, MF_GRAYED);
			EnableMenuItem(h_menu, ID_STOP_RECORD, MF_GRAYED);
			EnableMenuItem(h_menu, ID_START_PLAYBACK, MF_GRAYED);
			EnableMenuItem(h_menu, ID_STOP_PLAYBACK, MF_GRAYED);
			EnableMenuItem(h_menu, ID_START_CAPTURE, MF_GRAYED);
			EnableMenuItem(h_menu, ID_START_CAPTURE_PRESET, MF_GRAYED);
			EnableMenuItem(h_menu, ID_END_CAPTURE, MF_GRAYED);
			LONG winstyle = GetWindowLong(main_hwnd, GWL_STYLE);
			winstyle |= WS_MAXIMIZEBOX;
			SetWindowLong(main_hwnd, GWL_STYLE, winstyle);
			SetWindowPos(main_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
			             SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
			//Set on top
		}
		toolbar_on_emu_state_changed(0, 0);
	}

	if (Config.is_toolbar_enabled) CheckMenuItem(
		h_menu, IDC_GUI_TOOLBAR, MF_BYCOMMAND | MF_CHECKED);
	else CheckMenuItem(h_menu, IDC_GUI_TOOLBAR, MF_BYCOMMAND | MF_UNCHECKED);
	if (Config.is_statusbar_enabled) CheckMenuItem(
		h_menu, IDC_GUI_STATUSBAR, MF_BYCOMMAND | MF_CHECKED);
	else CheckMenuItem(h_menu, IDC_GUI_STATUSBAR, MF_BYCOMMAND | MF_UNCHECKED);
	if (Config.is_movie_loop_enabled) CheckMenuItem(
		h_menu, ID_LOOP_MOVIE, MF_BYCOMMAND | MF_CHECKED);
	else CheckMenuItem(h_menu, ID_LOOP_MOVIE, MF_BYCOMMAND | MF_UNCHECKED);
	if (Config.is_recent_movie_paths_frozen) CheckMenuItem(
		h_menu, ID_RECENTMOVIES_FREEZE, MF_BYCOMMAND | MF_CHECKED);
	if (Config.is_recent_scripts_frozen) CheckMenuItem(
		h_menu, ID_LUA_RECENT_FREEZE, MF_BYCOMMAND | MF_CHECKED);
	if (Config.is_recent_rom_paths_frozen) CheckMenuItem(
		h_menu, ID_RECENTROMS_FREEZE, MF_BYCOMMAND | MF_CHECKED);
}

static DWORD WINAPI sound_thread(LPVOID)
{
	while (emu_launched)
	{
		aiUpdate(1);
	}
	ExitThread(0);
}

static DWORD WINAPI thread_func(LPVOID)
{
	const auto start_time = std::chrono::high_resolution_clock::now();
	init_memory();
	romOpen_gfx();
	romOpen_input();
	romOpen_audio();

	dynacore = Config.core_type;

	emu_paused = 0;
	emu_launched = 1;

	sound_thread_handle = CreateThread(nullptr, 0, sound_thread, nullptr, 0,
	                                   &audio_thread_id);
	printf("Emu thread: Emulation started....\n");

	// start movies, st and lua scripts
	commandline_load_st();
	commandline_start_lua();
	commandline_start_movie();

	// HACK:
	// starting capture immediately won't work, since sample rate will change at game startup, thus terminating the capture
	// as a workaround, we wait a bit before starting the capture
	std::thread([]
	{
		Sleep(1000);
		commandline_start_capture();
	}).detach();

	AtResetLuaCallback();
	if (pause_at_frame == 0 && vcr_is_starting_and_just_restarted())
	{
		while (emu_paused)
		{
			Sleep(10);
		}
		pause_emu(FALSE);
		pause_at_frame = -1;
	}
	main_dispatcher_invoke([]
	{
		for (const HWND hwnd : previously_running_luas)
		{
			// click start button
			SendMessage(hwnd, WM_COMMAND,
					MAKEWPARAM(IDC_BUTTON_LUASTATE, BN_CLICKED),
					(LPARAM)GetDlgItem(hwnd, IDC_BUTTON_LUASTATE));
		}

		previously_running_luas.clear();
	});

	printf("emu thread entry %dms\n", static_cast<int>((std::chrono::high_resolution_clock::now() - start_time).count() / 1'000'000));

	go();

	romClosed_gfx();
	romClosed_audio();
	romClosed_input();
	romClosed_RSP();

	closeDLL_gfx();
	closeDLL_audio();
	closeDLL_input();
	closeDLL_RSP();

	printf("Unloading plugins\n");
	unload_plugins();

	ExitThread(0);
}

void main_recent_roms_build(const int32_t reset)
{
	const HMENU h_menu = GetMenu(main_hwnd);
	for (size_t i = 0; i < Config.recent_rom_paths.size(); i++)
	{
		if (Config.recent_rom_paths[i].empty())
		{
			continue;
		}
		DeleteMenu(h_menu, ID_RECENTROMS_FIRST + i, MF_BYCOMMAND);
	}

	if (reset)
	{
		Config.recent_rom_paths.clear();
	}

	HMENU h_sub_menu = GetSubMenu(h_menu, 0);
	h_sub_menu = GetSubMenu(h_sub_menu, 5);

	MENUITEMINFO menu_info = {0};
	menu_info.cbSize = sizeof(MENUITEMINFO);
	menu_info.fMask = MIIM_TYPE | MIIM_ID;
	menu_info.fType = MFT_STRING;
	menu_info.fState = MFS_ENABLED;

	for (size_t i = 0; i < Config.recent_rom_paths.size(); i++)
	{
		if (Config.recent_rom_paths[i].empty())
		{
			continue;
		}
		menu_info.dwTypeData = (LPSTR)Config.recent_rom_paths[i].c_str();
		menu_info.cch = strlen(menu_info.dwTypeData);
		menu_info.wID = ID_RECENTROMS_FIRST + i;
		InsertMenuItem(h_sub_menu, i + 3, TRUE, &menu_info);
	}
}

void main_recent_roms_add(const std::string& path)
{
	if (Config.is_recent_rom_paths_frozen)
	{
		return;
	}
	if (Config.recent_rom_paths.size() > 5)
	{
		Config.recent_rom_paths.pop_back();
	}
	std::erase(Config.recent_rom_paths, path);
	Config.recent_rom_paths.insert(Config.recent_rom_paths.begin(), path);
	main_recent_roms_build();
}

int32_t main_recent_roms_run(const uint16_t menu_item_id)
{
	if (const int index = menu_item_id - ID_RECENTROMS_FIRST; index >= 0 && index < (int)Config.recent_rom_paths.size()) {
		strcpy(rom_path, Config.recent_rom_paths[index].c_str());
		CreateThread(nullptr, 0, start_rom, rom_path, 0, &start_rom_id);
			return 	1;
	}
	return 0;
}

bool is_frame_skipped()
{
	if (!fast_forward || vcr_is_capturing())
	{
		return false;
	}

	// skip every frame
	if (Config.frame_skip_frequency == 0)
	{
		return true;
	}

	// skip no frames
	if (Config.frame_skip_frequency == 1)
	{
		return false;
	}

	return screen_updates % Config.frame_skip_frequency != 0;
}

void reset_titlebar()
{
	SetWindowText(main_hwnd, (std::string(mupen_version) + " - " + std::string(reinterpret_cast<char*>(ROM_HEADER.nom))).c_str());
}

BOOL is_menu_item_enabled(const HMENU h_menu, const UINT u_id)
{
	return !(GetMenuState(h_menu, u_id, MF_BYCOMMAND) & (MF_DISABLED |
		MF_GRAYED));
}

void process_tool_tips(const LPARAM l_param, const HWND hwnd)
{
	LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT)l_param;

	lpttt->hinst = app_instance;

	// Specify the resource identifier of the descriptive
	// text for the given button.
	[[maybe_unused]] HMENU h_menu = GetMenu(hwnd);

	switch (lpttt->hdr.idFrom)
	{
	case IDLOAD:
		strcpy(lpttt->lpszText, "Load ROM...");
		break;
	case EMU_PLAY:
		strcpy(lpttt->lpszText, "Resume");
		break;
	case EMU_PAUSE:
		strcpy(lpttt->lpszText, "Pause");
		break;
	case EMU_STOP:
		strcpy(lpttt->lpszText, "Stop");
		break;
	case FULL_SCREEN:
		strcpy(lpttt->lpszText, "Fullscreen");
		break;
	case IDGFXCONFIG:
		strcpy(lpttt->lpszText, "Video Settings...");
		break;
	case IDSOUNDCONFIG:
		strcpy(lpttt->lpszText, "Audio Settings...");
		break;
	case IDINPUTCONFIG:
		strcpy(lpttt->lpszText, "Input Settings...");
		break;
	case IDRSPCONFIG:
		strcpy(lpttt->lpszText, "RSP Settings...");
		break;
	case ID_LOAD_CONFIG:
		strcpy(lpttt->lpszText, "Settings...");
		break;
	default:
		break;
	}
}

DWORD WINAPI unpause_emu_after_menu(LPVOID)
{
	Sleep(60); // Wait for another thread to clear MenuPaused

	if (emu_paused && !auto_pause && menu_paused)
	{
		resume_emu(FALSE);
	}
	menu_paused = FALSE;
	return 0;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
	char path_buffer[_MAX_PATH];
	static PAINTSTRUCT ps;
	HMENU h_menu = GetMenu(hwnd);

	LuaWindowMessage(hwnd, message, w_param, l_param);

	switch (message)
	{
	case WM_EXECUTE_DISPATCHER:
		main_dispatcher_process();
		break;
	case WM_DROPFILES:
		{
			auto h_file = (HDROP)w_param;
			char fname[MAX_PATH] = {0};
			DragQueryFile(h_file, 0, fname, sizeof(fname));
			LocalFree(h_file);

			std::filesystem::path path = fname;

			if (std::string extension = to_lower(path.extension().string());
				extension == ".n64" || extension == ".z64" || extension == ".v64" || extension == ".rom")
			{
				strcpy(rom_path, fname);
				CreateThread(nullptr, 0, start_rom, nullptr, 0, &start_rom_id);
			} else if (extension == ".m64")
			{
				if (!emu_launched) break;
				if (!vcr_get_read_only()) vcr_toggle_read_only();

				vcr_start_playback(fname, false);
				set_status_playback_started();
			}else if (extension == ".st" || extension == ".savestate")
			{
				if (!emu_launched) break;
				savestates_do(fname, e_st_job::load);
			} else if(extension == ".lua")
			{
				lua_create_and_run(path.string().c_str(), false);
			}
			break;
		}
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			BOOL hit = FALSE;
			if (!fast_forward)
			{
				if ((int)w_param == Config.fast_forward_hotkey.key)
				// fast-forward on
				{
					if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) == Config.
						fast_forward_hotkey.shift
						&& ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) ==
						Config.fast_forward_hotkey.ctrl
						&& ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) == Config.
						fast_forward_hotkey.alt)
					{
						fast_forward = 1;
						hit = TRUE;
					}
				}
			}
			for (const t_hotkey* hotkey : hotkeys)
			{
				if ((int)w_param == hotkey->key)
				{
					if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) == hotkey->
						shift
						&& ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) ==
						hotkey->ctrl
						&& ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) == hotkey->
						alt)
					{
						// printf("sent %s - %d\n", hotkey->identifier.c_str(), hotkey->command);
						SendMessage(main_hwnd, WM_COMMAND, hotkey->command, 0);
						hit = TRUE;
					}
				}
			}

			if (emu_launched)
				keyDown(w_param, l_param);
			if (!hit)
				return DefWindowProc(hwnd, message, w_param, l_param);
		}
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		if ((int)w_param == Config.fast_forward_hotkey.key) // fast-forward off
		{
			fast_forward = 0;
			ffup = true; //fuck it, timers.c is too weird
		}
		if (emu_launched)
			keyUp(w_param, l_param);
		return DefWindowProc(hwnd, message, w_param, l_param);
	case WM_NOTIFY:
		{
			auto l_header = (LPNMHDR)l_param;

			if (w_param == IDC_ROMLIST)
			{
				rombrowser_notify(l_param);
			}
			switch ((l_header)->code)
			{
			case TTN_NEEDTEXT:
				process_tool_tips(l_param, hwnd);
				break;
			}
			return 0;
		}
	case WM_MOVE:
		{
			if (emu_launched && !FullScreenMode)
			{
				moveScreen((int)w_param, l_param);
			}
			RECT rect = {0};
			GetWindowRect(main_hwnd, &rect);
			Config.window_x = rect.left;
			Config.window_y = rect.top;
			Config.window_width = rect.right - rect.left;
			Config.window_height = rect.bottom - rect.top;
			break;
		}
	case WM_SIZE:
		{
			if (!FullScreenMode)
			{
				SendMessage(toolbar_hwnd, TB_AUTOSIZE, 0, 0);
				SendMessage(statusbar_hwnd, WM_SIZE, 0, 0);
			}
			rombrowser_update_size();
			break;
		}
	case WM_USER + 17: SetFocus(main_hwnd);
		break;
	case WM_CREATE:
		GetModuleFileName(nullptr, path_buffer, sizeof(path_buffer));
		update_screen_timer = SetTimer(hwnd, NULL, (uint32_t)(1000 / get_primary_monitor_refresh_rate()), nullptr);
		commandline_start_rom();
		return TRUE;
	case WM_DESTROY:
		save_config();
		KillTimer(main_hwnd, update_screen_timer);
		Gdiplus::GdiplusShutdown(gdiPlusToken);
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		if (confirm_user_exit())
		{
			DestroyWindow(main_hwnd);
			break;
		}
		return 0;
	case WM_TIMER:
		AtUpdateScreenLuaCallback();
		break;
	case WM_PAINT: //todo, work with updatescreen to use wmpaint
		{
			BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);

			return 0;
		}
	case WM_WINDOWPOSCHANGING: //allow gfx plugin to set arbitrary size
		return 0;
	case WM_GETMINMAXINFO:
		{
			auto lp_mmi = (LPMINMAXINFO)l_param;
			lp_mmi->ptMinTrackSize.x = MIN_WINDOW_W;
			lp_mmi->ptMinTrackSize.y = MIN_WINDOW_H;
			// this might break small res with gfx plugin!!!
		}
		break;
	case WM_ENTERMENULOOP:
		auto_pause = emu_paused;
		if (!emu_paused)
		{
			menu_paused = TRUE;
			pause_emu(FALSE);
		}
		break;

	case WM_EXITMENULOOP:
		CreateThread(nullptr, 0, unpause_emu_after_menu, nullptr, 0, nullptr);
		break;
	case WM_ACTIVATE:
		UpdateWindow(hwnd);

		switch (LOWORD(w_param))
		{
		case WA_ACTIVE:
		case WA_CLICKACTIVE:
			if (Config.is_unfocused_pause_enabled && emu_paused && !auto_pause)
			{
				resume_emu(FALSE);
				auto_pause = emu_paused;
			}
			break;

		case WA_INACTIVE:
			auto_pause = emu_paused && !menu_paused;
			if (Config.is_unfocused_pause_enabled && !emu_paused
				/*(&& minimize*/ && !FullScreenMode)
			{
				menu_paused = FALSE;
				pause_emu(FALSE);
			} else if (Config.is_unfocused_pause_enabled && menu_paused)
			{
				menu_paused = FALSE;
			}
			break;
		default:
			break;
		}
		break;
	case WM_COMMAND:
		{
			switch (LOWORD(w_param))
			{
			case ID_MENU_LUASCRIPT_NEW:
				{
					NewLuaScript();
				}
				break;
			case ID_LUA_RECENT_FREEZE:
				CheckMenuItem(h_menu, ID_LUA_RECENT_FREEZE,
				              (Config.is_recent_scripts_frozen ^= 1)
					              ? MF_CHECKED
					              : MF_UNCHECKED);
				break;
			case ID_LUA_RECENT_RESET:
				lua_recent_scripts_build(1);
				break;
			case ID_LUA_LOAD_LATEST:
				lua_recent_scripts_run(ID_LUA_RECENT);
				break;
			case ID_MENU_LUASCRIPT_CLOSEALL:
				close_all_scripts();
				break;
			case ID_FORCESAVE:
				save_config();
				break;
			case ID_TRACELOG:
				// keep if check just in case user manages to screw with mupen config or something
				if (!dynacore)
				{
					::LuaTraceLogState();
				}
				break;
			case EMU_STOP:
				menu_paused = FALSE;
				if (!confirm_user_exit())
					break;
				if (emu_launched)
				{
					//close_rom(&Id);
					CreateThread(nullptr, 0, close_rom, (LPVOID)1, 0, &close_rom_id);

				}
				break;

			case EMU_PAUSE:
				{
					if (!emu_paused)
					{
						pause_emu(vcr_is_active());
					} else if (menu_paused)
					{
						menu_paused = FALSE;
						CheckMenuItem(GetMenu(main_hwnd), EMU_PAUSE,
						              MF_BYCOMMAND | MFS_CHECKED);
					} else
					{
						resume_emu(vcr_is_active());
					}
					break;
				}

			case EMU_FRAMEADVANCE:
				{
					menu_paused = FALSE;
					frame_advancing = 1;
					vis_per_second = 0;
					// prevent old VI value from showing error if running at super fast speeds
					resume_emu(TRUE); // maybe multithreading unsafe
				}
				break;

			case EMU_VCRTOGGLEREADONLY:
				vcr_toggle_read_only();
				break;

			case ID_LOOP_MOVIE:
				set_is_movie_loop_enabled(!Config.is_movie_loop_enabled);
				break;
			case ID_RESTART_MOVIE:
				if (vcr_is_playing())
				{
					vcr_set_read_only(TRUE);
					vcr_start_playback(Config.recent_movie_paths[0], false);
					set_status_playback_started();
				}
				break;
			case ID_REPLAY_LATEST:
				if (!emu_launched)
					break;
				// Overwrite prevention? Path sanity check (Leave to internal handling)?
				vcr_set_read_only(TRUE);
				vcr_start_playback(Config.recent_movie_paths[0], false);
				set_status_playback_started();
				break;
			case ID_RECENTMOVIES_FREEZE:
				CheckMenuItem(h_menu, ID_RECENTMOVIES_FREEZE,
				              (Config.is_recent_movie_paths_frozen ^= 1)
					              ? MF_CHECKED
					              : MF_UNCHECKED);
				break;
			case ID_RECENTMOVIES_RESET:
				vcr_recent_movies_build(1);
				break;
			case EMU_PLAY:
				if (emu_launched)
				{
					if (emu_paused)
					{
						resume_emu(FALSE);
					}
				} else
				{
					// TODO: reimplement
					// RomList_OpenRom();
				}
				break;

			case EMU_RESET:
				if (!Config.is_reset_recording_enabled && confirm_user_exit())
					break;
				if (vcr_is_recording() && Config.is_reset_recording_enabled)
				{
					scheduled_restart = true;
					continue_vcr_on_restart_mode = true;
					statusbar_post_text("Writing restart to movie");
					break;
				}
				reset_emu();
				break;

			case ID_LOAD_CONFIG:
				{
					BOOL was_paused = emu_paused && !menu_paused;
					menu_paused = FALSE;
					if (emu_launched && !emu_paused)
					{
						pause_emu(FALSE);
					}
					configdialog_show();
					if (emu_launched && emu_paused && !was_paused)
					{
						resume_emu(FALSE);
					}
				}
				break;
			case ID_HELP_ABOUT:
				{
					BOOL was_menu_paused = menu_paused;
					menu_paused = FALSE;
					configdialog_about();
					if (was_menu_paused)
					{
						resume_emu(TRUE);
					}
				}
				break;
			case ID_GAMEDEBUGGER:
				extern unsigned long op;

				GameDebuggerStart([=]()
				                  {
					                  return Config.core_type == 2 ? op : -1;
				                  }, []()
				                  {
					                  return Config.core_type == 2
						                         ? interp_addr
						                         : -1;
				                  });
				break;
			case ID_RAMSTART:
				{
					BOOL was_menu_paused = menu_paused;
					menu_paused = FALSE;

					pause_emu(TRUE);

					char ram_start[20] = {0};
					sprintf(ram_start, "0x%p", static_cast<void*>(rdram));

					char proc_name[MAX_PATH] = {0};
					GetModuleFileName(nullptr, proc_name, MAX_PATH);
					_splitpath(proc_name, nullptr, nullptr, proc_name, nullptr);

					char stroop_c[1024] = {0};
					sprintf(stroop_c,
					        R"(<Emulator name="Mupen 5.0 RR" processName="%s" ramStart="%s" endianness="little"/>)",
					        proc_name, ram_start);

					const auto stroop_str = std::string(stroop_c);
					if (MessageBoxA(main_hwnd,
					                "Do you want to copy the generated STROOP config line to your clipboard?",
					                "STROOP",
					                MB_ICONINFORMATION | MB_TASKMODAL |
					                MB_YESNO) == IDYES)
					{
						copy_to_clipboard(main_hwnd, stroop_str);
					}
					if (was_menu_paused)
					{
						resume_emu(TRUE);
					}
					break;
				}
			case IDLOAD:
				{
					BOOL was_menu_paused = menu_paused;
					menu_paused = FALSE;

					if (const auto path = show_persistent_open_dialog("o_rom", main_hwnd,
						L"*.n64;*.z64;*.v64;*.rom;*.bin;*.zip;*.usa;*.eur;*.jap"); !path.empty())
					{
						strcpy(rom_path, wstring_to_string(path).c_str());
						CreateThread(nullptr, 0, start_rom, nullptr, NULL, &start_rom_id);
					}

					if (was_menu_paused)
					{
						resume_emu(TRUE);
					}
				}
				break;
			case ID_EMULATOR_EXIT:
				DestroyWindow(main_hwnd);
				break;
			case FULL_SCREEN:
				if (emu_launched && !vcr_is_capturing())
				{
					FullScreenMode = 1 - FullScreenMode;
					gui_change_window();
				}
				break;
			case REFRESH_ROM_BROWSER:
				if (!emu_launched)
				{
					rombrowser_build();
				}
				break;
			case STATE_SAVE:
				savestates_do(st_slot, e_st_job::save);
				break;
			case STATE_SAVEAS:
				{
					BOOL was_menu_paused = menu_paused;
					menu_paused = FALSE;

					auto path = show_persistent_save_dialog("s_savestate", hwnd, L"*.st;*.savestate");
					if (path.empty())
					{
						break;
					}

					savestates_do(path, e_st_job::save);

					if (was_menu_paused)
					{
						resume_emu(TRUE);
					}
				}
				break;
			case STATE_RESTORE:
				savestates_do(st_slot, e_st_job::load);
				break;
			case STATE_LOAD:
				{
					BOOL wasMenuPaused = menu_paused;
					menu_paused = FALSE;

					auto path = show_persistent_open_dialog("o_state", hwnd, L"*.st;*.savestate;*.st0;*.st1;*.st2;*.st3;*.st4;*.st5;*.st6;*.st7;*.st8;*.st9,*.st10");
					if (path.empty())
					{
						break;
					}

					savestates_do(path, e_st_job::load);

					if (wasMenuPaused)
					{
						resume_emu(TRUE);
					}
				}
				break;
			case ID_START_RECORD:
				if (emu_launched)
					open_movie_record_dialog();
				break;
			case ID_STOP_RECORD:
				if (vcr_is_recording())
				{
					vcr_stop_record();
					clear_buttons();
					EnableMenuItem(h_menu, ID_STOP_RECORD, MF_GRAYED);
					EnableMenuItem(h_menu, ID_START_RECORD, MF_ENABLED);
					statusbar_post_text("Recording stopped");
				}
				break;
			case ID_START_PLAYBACK:
				if (emu_launched)
					open_movie_playback_dialog();

				break;
			case ID_STOP_PLAYBACK:
				if (vcr_is_playing())
				{
					vcr_stop_playback(true);
					clear_buttons();
					EnableMenuItem(h_menu, ID_STOP_PLAYBACK, MF_GRAYED);
					EnableMenuItem(h_menu, ID_START_PLAYBACK, MF_ENABLED);
					statusbar_post_text("Playback stopped");
				}
				break;

			case ID_FFMPEG_START:
				{
					if (auto err = vcr_start_ffmpeg_capture(
						"ffmpeg_out.mp4",
						"-pixel_format yuv420p -loglevel debug -y"); err == INIT_SUCCESS)
					{
						//SetWindowPos(main_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);  //Set on top avichg
						EnableMenuItem(h_menu, ID_START_CAPTURE, MF_GRAYED);
						EnableMenuItem(h_menu, ID_START_CAPTURE_PRESET,
						               MF_GRAYED);
						EnableMenuItem(h_menu, ID_FFMPEG_START, MF_GRAYED);
						EnableMenuItem(h_menu, ID_END_CAPTURE, MF_ENABLED);
						EnableMenuItem(h_menu, FULL_SCREEN, MF_GRAYED);
						statusbar_post_text("Recording AVI with FFmpeg");
						enable_emulation_menu_items(TRUE);
					} else
						printf("Start capture error: %d\n", err);
					break;
				}

			case ID_START_CAPTURE_PRESET:
			case ID_START_CAPTURE:
				if (emu_launched)
				{
					BOOL was_paused = emu_paused && !menu_paused;
					menu_paused = FALSE;
					if (emu_launched && !emu_paused)
						pause_emu(FALSE);

					if (auto path = show_persistent_save_dialog("s_capture", hwnd, L"*.avi"); path.empty())
					{
						break;
					}

					// pass false to startCapture when "last preset" option was choosen
					if constexpr (false) //vcr_start_capture(wstring_to_string(path).c_str(), LOWORD(w_param) == ID_START_CAPTURE) < 0) what is this
					{
						MessageBox(nullptr, "Couldn't start capturing.", "VCR", MB_OK);
					} else
					{
						EnableMenuItem(h_menu, ID_START_CAPTURE, MF_GRAYED);
						EnableMenuItem(h_menu, ID_START_CAPTURE_PRESET,
						               MF_GRAYED);
						EnableMenuItem(h_menu, ID_FFMPEG_START, MF_GRAYED);
						EnableMenuItem(h_menu, ID_END_CAPTURE, MF_ENABLED);
						EnableMenuItem(h_menu, FULL_SCREEN, MF_GRAYED);
						statusbar_post_text("Recording AVI");
						enable_emulation_menu_items(TRUE);
					}

					if (emu_launched && emu_paused && !was_paused)
						resume_emu(FALSE);
				}

				break;
			case ID_END_CAPTURE:
				if (vcr_stop_capture() < 0)
					MessageBox(nullptr, "Couldn't stop capturing.", "VCR", MB_OK);
				else
				{
					SetWindowPos(main_hwnd, HWND_TOP, 0, 0, 0, 0,
					             SWP_NOMOVE | SWP_NOSIZE);
					EnableMenuItem(h_menu, ID_END_CAPTURE, MF_GRAYED);
					EnableMenuItem(h_menu, ID_START_CAPTURE, MF_ENABLED);
					EnableMenuItem(h_menu, ID_FFMPEG_START, MF_ENABLED);
					EnableMenuItem(h_menu, ID_START_CAPTURE_PRESET, MF_ENABLED);
					statusbar_post_text("Capture stopped");
				}
				break;
			case GENERATE_BITMAP: // take/capture a screenshot
				if (Config.is_default_screenshots_directory_used)
				{
					sprintf(path_buffer, "%sScreenShots\\", app_path.c_str());
					CaptureScreen(path_buffer);
				} else
				{
					sprintf(path_buffer, "%s",
					        Config.screenshots_directory.c_str());
					CaptureScreen(path_buffer);
				}
				break;
			case ID_RECENTROMS_RESET:
				main_recent_roms_build(1);
				break;
			case ID_RECENTROMS_FREEZE:
				CheckMenuItem(h_menu, ID_RECENTROMS_FREEZE,
							  (Config.is_recent_rom_paths_frozen ^= 1)
								  ? MF_CHECKED
								  : MF_UNCHECKED);
				break;
			case ID_LOAD_LATEST:
				main_recent_roms_run(ID_RECENTROMS_FIRST);
				break;
			case IDC_GUI_TOOLBAR:
				Config.is_toolbar_enabled ^= true;
				toolbar_set_visibility(Config.is_toolbar_enabled);
				CheckMenuItem(
					h_menu, IDC_GUI_TOOLBAR, MF_BYCOMMAND | (Config.is_toolbar_enabled ? MF_CHECKED : MF_UNCHECKED));
				break;
			case IDC_GUI_STATUSBAR:
				Config.is_statusbar_enabled ^= true;
				statusbar_set_visibility(Config.is_statusbar_enabled);
				CheckMenuItem(
					h_menu, IDC_GUI_STATUSBAR, MF_BYCOMMAND | (Config.is_statusbar_enabled ? MF_CHECKED : MF_UNCHECKED));
				break;
			case IDC_INCREASE_MODIFIER:
				if (Config.fps_modifier < 50)
					Config.fps_modifier = Config.fps_modifier + 5;
				else if (Config.fps_modifier < 100)
					Config.fps_modifier = Config.fps_modifier + 10;
				else if (Config.fps_modifier < 200)
					Config.fps_modifier = Config.fps_modifier + 25;
				else if (Config.fps_modifier < 1000)
					Config.fps_modifier = Config.fps_modifier + 50;
				if (Config.fps_modifier > 1000)
					Config.fps_modifier = 1000;
				timer_init();
				break;
			case IDC_DECREASE_MODIFIER:
				if (Config.fps_modifier > 200)
					Config.fps_modifier = Config.fps_modifier - 50;
				else if (Config.fps_modifier > 100)
					Config.fps_modifier = Config.fps_modifier - 25;
				else if (Config.fps_modifier > 50)
					Config.fps_modifier = Config.fps_modifier - 10;
				else if (Config.fps_modifier > 5)
					Config.fps_modifier = Config.fps_modifier - 5;
				if (Config.fps_modifier < 5)
					Config.fps_modifier = 5;
				timer_init();
				break;
			case IDC_RESET_MODIFIER:
				Config.fps_modifier = 100;
				timer_init();
				break;
			default:
				if (LOWORD(w_param) >= ID_CURRENTSAVE_1 && LOWORD(w_param)
					<= ID_CURRENTSAVE_10)
				{
					auto slot = LOWORD(w_param) - ID_CURRENTSAVE_1;
					st_slot = slot;

					// set checked state for only the currently selected save
					for (int i = ID_CURRENTSAVE_1; i < ID_CURRENTSAVE_10; ++i)
					{
						CheckMenuItem(h_menu, i, MF_UNCHECKED);
					}
					CheckMenuItem(h_menu, LOWORD(w_param), MF_CHECKED);

				} else if (LOWORD(w_param) >= ID_SAVE_1 && LOWORD(w_param) <=
					ID_SAVE_10)
				{
					auto slot = LOWORD(w_param) - ID_SAVE_1;
					// if emu is paused and no console state is changing, we can safely perform st op instantly
					savestates_do(slot, e_st_job::save);
				} else if (LOWORD(w_param) >= ID_LOAD_1 && LOWORD(w_param) <=
					ID_LOAD_10)
				{
					auto slot = LOWORD(w_param) - ID_LOAD_1;
					savestates_do(slot, e_st_job::load);
				} else if (LOWORD(w_param) >= ID_RECENTROMS_FIRST &&
					LOWORD(w_param) < (ID_RECENTROMS_FIRST + Config.
						recent_rom_paths.size()))
				{
					main_recent_roms_run(LOWORD(w_param));
				} else if (LOWORD(w_param) >= ID_RECENTMOVIES_FIRST &&
					LOWORD(w_param) < (ID_RECENTMOVIES_FIRST + Config.
						recent_movie_paths.size()))
				{
					if (vcr_recent_movies_play(LOWORD(w_param)) != success)
					{
						statusbar_post_text("Couldn't load movie");
						break;
					}
					// should probably make this code from the ID_REPLAY_LATEST case into a function on its own
					// because now it's used here too
					EnableMenuItem(h_menu, ID_STOP_RECORD, MF_GRAYED);
					EnableMenuItem(h_menu, ID_STOP_PLAYBACK, MF_ENABLED);

					if (!emu_paused || !emu_launched)
						statusbar_post_text("Playback started");
					else
						statusbar_post_text("Playback started while paused");
				} else if (LOWORD(w_param) >= ID_LUA_RECENT && LOWORD(w_param) < (
					ID_LUA_RECENT + Config.recent_lua_script_paths.size()))
				{
					printf("run recent script\n");
					lua_recent_scripts_run(LOWORD(w_param));
				}
				break;
			}
		}
		break;
	default:
		return DefWindowProc(hwnd, message, w_param, l_param);
	}

	return TRUE;
}

// kaboom
LONG WINAPI exception_release_target(_EXCEPTION_POINTERS* exception_info)
{
	// generate crash log

	char crash_log[1024 * 4] = {0};
	CrashHelper::GenerateLog(exception_info, crash_log);

	FILE* f = fopen("crash.log", "w+");
	fwrite(crash_log, sizeof(crash_log), 1, f);
	fclose(f);

	const bool is_continuable = !(exception_info->ExceptionRecord->ExceptionFlags &
		EXCEPTION_NONCONTINUABLE);

	int result = 0;

	if (is_continuable) {
		TaskDialog(main_hwnd, app_instance, L"Error",
			L"An error has occured", L"A crash log has been automatically generated. You can choose to continue program execution.", TDCBF_RETRY_BUTTON | TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, &result);
	} else {
		TaskDialog(main_hwnd, app_instance, L"Error",
			L"An error has occured", L"A crash log has been automatically generated. The program will now exit.", TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, &result);
	}

	if (result == IDCLOSE) {
		return EXCEPTION_EXECUTE_HANDLER;
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}


int WINAPI WinMain(
	const HINSTANCE h_instance, HINSTANCE /*h_prev_instance*/, LPSTR /*lp_cmd_line*/, const int nShowCmd)
{
#ifdef _DEBUG
	AllocConsole();
	FILE* f = nullptr;
	freopen_s(&f, "CONIN$", "r", stdin);
	freopen_s(&f, "CONOUT$", "w", stdout);
	freopen_s(&f, "CONOUT$", "w", stderr);
#endif

	app_path = get_app_full_path();
	app_instance = h_instance;

	commandline_set();

	// ensure folders exist!
	CreateDirectory((app_path + "save").c_str(), nullptr);
	CreateDirectory((app_path + "Mempaks").c_str(), nullptr);
	CreateDirectory((app_path + "Lang").c_str(), nullptr);
	CreateDirectory((app_path + "ScreenShots").c_str(), nullptr);
	CreateDirectory((app_path + "plugin").c_str(), nullptr);

	emu_launched = 0;
	emu_paused = 1;

	load_config();

	WNDCLASSEX wc = {0};
	MSG msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hInstance = h_instance;
	wc.hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_M64ICONBIG));
	wc.hIconSm = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_M64ICONSMALL));
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = g_sz_class_name;
	wc.lpfnWndProc = WndProc;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MYMENU);

	RegisterClassEx(&wc);

	const HACCEL accelerators = LoadAccelerators(h_instance, MAKEINTRESOURCE(IDR_ACCEL));

	const HWND hwnd = CreateWindowEx(
		0,
		g_sz_class_name,
		mupen_version,
		WS_OVERLAPPEDWINDOW | WS_EX_COMPOSITED,
		Config.window_x, Config.window_y, Config.window_width,
		Config.window_height,
		nullptr, nullptr, h_instance, nullptr);

	main_hwnd = hwnd;
	ShowWindow(hwnd, nShowCmd);

	// This fixes offscreen recording issue
	SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_ACCEPTFILES);
	//this can't be applied before ShowWindow(), otherwise you must use some fancy function

	update_menu_hotkey_labels();
	toolbar_set_visibility(Config.is_toolbar_enabled);
	statusbar_set_visibility(Config.is_statusbar_enabled);
	setup_dummy_info();
	rombrowser_create();
	rombrowser_build();
	rombrowser_update_size();
	set_is_movie_loop_enabled(Config.is_movie_loop_enabled);

	vcr_recent_movies_build();
	lua_recent_scripts_build();
	main_recent_roms_build();

	enable_emulation_menu_items(0);

	//warning, this is ignored when debugger is attached (like visual studio)
	SetUnhandledExceptionFilter(exception_release_target);

	// raise noncontinuable exception (impossible to recover from it)
	//RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, NULL, NULL);
	//
	// raise continuable exception
	//RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, NULL, NULL);

	while (GetMessage(&msg, nullptr, 0, 0) > 0)
	{
		if (!TranslateAccelerator(main_hwnd, accelerators, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		for (const t_hotkey* hotkey : hotkeys)
		{
			// modifier-only checks, cannot be obtained through windows messaging...
			if (!hotkey->key && (hotkey->shift || hotkey->ctrl || hotkey->alt))
			{
				// special treatment for fast-forward
				if (hotkey->identifier == Config.fast_forward_hotkey.identifier)
				{
					if (!frame_advancing)
					{
						// dont allow fastforward+frameadvance
						if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) ==
							hotkey->shift
							&& ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0)
							== hotkey->ctrl
							&& ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) ==
							hotkey->alt)
						{
							fast_forward = 1;
						} else
						{
							fast_forward = 0;
						}
					}
					continue;
				}
				if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) ==
						hotkey->shift
						&& ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) ==
						hotkey->ctrl
						&& ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) ==
						hotkey->alt)
				{
					SendMessage(hwnd, WM_COMMAND, hotkey->command,
								0);
				}

			}

		}

	}


	return (int)msg.wParam;
}
