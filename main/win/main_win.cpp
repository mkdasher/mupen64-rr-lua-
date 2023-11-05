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

#include <cmath>
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
#define snprintf	_snprintf
#define strcasecmp	stricmp
#define strncasecmp	strnicmp
#endif


DWORD emu_id;
DWORD start_rom_id;
DWORD close_rom_id;
DWORD audio_thread_id;
HANDLE sound_thread_handle;
static BOOL FullScreenMode = 0;

HANDLE loading_handle[4];
HANDLE EmuThreadHandle;
HWND hwnd_plug;
UINT update_screen_timer;

static DWORD WINAPI ThreadFunc(LPVOID lpParam);
DWORD WINAPI close_rom(LPVOID lpParam);
constexpr char g_szClassName[] = "myWindowClass";
char rom_path[MAX_PATH] = {0};
char LastSelectedRom[_MAX_PATH];
bool scheduled_restart = false;
BOOL really_restart_mode = 0;
BOOL clear_sram_on_restart_mode = 0;
BOOL continue_vcr_on_restart_mode = 0;
BOOL just_restarted_flag = 0;
int frame_count = 1;
long long total_vi = 0;
static BOOL AutoPause = 0;
static BOOL MenuPaused = 0;
static HWND hStaticHandle; //Handle for static place
char TempMessage[MAX_PATH];
int emu_launched; //int emu_emulating;
int emu_paused;
HWND mainHWND;
HINSTANCE app_instance;
BOOL fast_forward = 0;
BOOL ignoreErrorEmulation = FALSE;
char statusmsg[800];

char correctedPath[260];
#define INCOMPATIBLE_PLUGINS_AMOUNT 1 // this is so bad
const char pluginBlacklist[INCOMPATIBLE_PLUGINS_AMOUNT][256] = {
	"Azimer\'s Audio v0.7"
};

TCHAR CoreNames[3][30] = {
	TEXT("Interpreter"), TEXT("Dynamic Recompiler"), TEXT("Pure Interpreter")
};

std::string app_path = "";


/**
 * \brief List of lua environment map keys running before emulation stopped
 */
std::vector<HWND> previously_running_luas;

std::deque<std::function<void()>> dispatcher_queue;

void main_dispatcher_invoke(const std::function<void()>& func) {
	dispatcher_queue.push_back(func);
	SendMessage(mainHWND, WM_EXECUTE_DISPATCHER, 0, 0);
}

void main_dispatcher_process()
{
	while (!dispatcher_queue.empty()) {
		dispatcher_queue.front()();
		dispatcher_queue.pop_front();
	}
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


static void gui_ChangeWindow()
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

void resumeEmu(BOOL quiet)
{
	BOOL wasPaused = emu_paused;
	if (emu_launched)
	{
		emu_paused = 0;
		ResumeThread(sound_thread_handle);
		if (!quiet)
			statusbar_post_text("Emulation started");
	}

	toolbar_on_emu_state_changed(emu_launched, 1);

	if (emu_paused != wasPaused && !quiet)
		CheckMenuItem(GetMenu(mainHWND), EMU_PAUSE,
		              MF_BYCOMMAND | (emu_paused
			                              ? MFS_CHECKED
			                              : MFS_UNCHECKED));
}


void pauseEmu(BOOL quiet)
{
	BOOL wasPaused = emu_paused;
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
		CheckMenuItem(GetMenu(mainHWND), EMU_PAUSE,
		              MF_BYCOMMAND | MFS_UNCHECKED);
	}

	toolbar_on_emu_state_changed(emu_launched, 0);

	if (emu_paused != wasPaused && !MenuPaused)
		CheckMenuItem(GetMenu(mainHWND), EMU_PAUSE,
		              MF_BYCOMMAND | (emu_paused
			                              ? MFS_CHECKED
			                              : MFS_UNCHECKED));
}

DWORD WINAPI start_rom(LPVOID lpParam)
{
	// maybe make no access violations, probably can be just
	// replaced with rom_path instead of rom_path_local
	char* rom_path_local = {};
	if (lpParam != nullptr) {
		rom_path_local = (char*)lpParam;
	} else {
		rom_path_local = rom_path;
	}
	auto start_time = std::chrono::high_resolution_clock::now();

	// Kill any roms that are still running
	if (emu_launched) {
		WaitForSingleObject(CreateThread(NULL, 0, close_rom, NULL, 0, &close_rom_id), 10'000);
	}

	// TODO: keep plugins loaded and only unload and reload them when they actually change
	printf("Loading plugins\n");
	if (!load_plugins())
	{
		MessageBox(mainHWND, "Invalid plugins selected", nullptr,
		   MB_ICONERROR | MB_OK);
		return 0;
	}

	// valid rom is required to start emulation
	if (rom_read(rom_path_local))
	{
		MessageBox(mainHWND, "Failed to open ROM", "Error",
				   MB_ICONERROR | MB_OK);
		unload_plugins();
		return 0;
	}
	// at this point, we're set to begin emulating and can't backtrack
	// disallow window resizing
	LONG style = GetWindowLong(mainHWND, GWL_STYLE);
	SetWindowLong(mainHWND, GWL_STYLE,
				  style & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
	// TODO: investigate wtf this is
	strcpy(LastSelectedRom, rom_path_local);

	// notify ui of emu state change
	main_recent_roms_add(rom_path_local);
	rombrowser_set_visibility(0);
	statusbar_set_mode(statusbar_mode::emulating);
	EnableEmulationMenuItems(TRUE);
	timer_init();
	if (m_task == e_task::idle) {
		SetWindowText(mainHWND, std::format("{} - {}", std::string(MUPEN_VERSION), std::string((char*)ROM_HEADER.nom)).c_str());
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
	EmuThreadHandle = CreateThread(NULL, 0, ThreadFunc, NULL, 0, &emu_id);

	return 1;
}

static int shut_window = 0;

DWORD WINAPI close_rom(LPVOID lpParam)
{
	//printf("gen interrupt: %lld ns/vi", (total_vi/frame_count));
	if (emu_launched) {

		if (emu_paused) {
			MenuPaused = FALSE;
			resumeEmu(FALSE);
		}

		if (VCR_isCapturing() && !continue_vcr_on_restart_mode) {
			// we need to stop capture before closing rom because rombrowser might show up in recording otherwise lol
			if (vcr_stop_capture() != 0)
				MessageBox(NULL, "Couldn't stop capturing", "VCR", MB_OK);
			else {
				SetWindowPos(mainHWND, HWND_TOP, 0, 0, 0, 0,
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
		DWORD result = WaitForSingleObject(EmuThreadHandle, 10'000);
		if (result == WAIT_TIMEOUT) {
			MessageBox(mainHWND, "Emu thread didn't exit in time", NULL,
					   MB_ICONERROR | MB_OK);
		}

		emu_launched = 0;
		emu_paused = 1;


		rom = NULL;
		free(rom);

		free_memory();

		EnableEmulationMenuItems(FALSE);
		rombrowser_set_visibility(!really_restart_mode);
		toolbar_on_emu_state_changed(0, 0);

		if (m_task == e_task::idle) {
			SetWindowText(mainHWND, MUPEN_VERSION);
			// TODO: look into why this is done
			statusbar_post_text(" ", 1);
		}

		if (shut_window) {
			SendMessage(mainHWND, WM_CLOSE, 0, 0);
			return 0;
		}

		statusbar_set_mode(statusbar_mode::rombrowser);
		statusbar_post_text("Emulation stopped");

		SetWindowLong(mainHWND, GWL_STYLE,
					  GetWindowLong(mainHWND, GWL_STYLE) | WS_THICKFRAME);

		if (really_restart_mode) {
			if (clear_sram_on_restart_mode) {
				vcr_clear_save_data();
				clear_sram_on_restart_mode = FALSE;
			}

			really_restart_mode = FALSE;
			if (m_task != e_task::idle)
				just_restarted_flag = TRUE;

			main_dispatcher_invoke([] {
				strcpy(rom_path, LastSelectedRom);
				CreateThread(NULL, 0, start_rom, rom_path, 0, &start_rom_id);
				//if (!start_rom(nullptr)) {
					//close_rom(NULL);
					//MessageBox(mainHWND, "Failed to open ROM", NULL,
							//   MB_ICONERROR | MB_OK);
				//}
			});
		}


		continue_vcr_on_restart_mode = FALSE;
		ExitThread(0);
	}
	ExitThread(0);
}

void resetEmu()
{
	// why is it so damned difficult to reset the game?
	// right now it's hacked to exit to the GUI then re-load the ROM,
	// but it should be possible to reset the game while it's still running
	// simply by clearing out some memory and maybe notifying the plugins...
	if (emu_launched)
	{
		frame_advancing = false;
		really_restart_mode = TRUE;
		MenuPaused = FALSE;
		CreateThread(NULL, 0, close_rom, NULL, 0, &close_rom_id);
	}
}

int pauseAtFrame = -1;

/// <summary>
/// Helper function because this is repeated multiple times
/// </summary>
void SetStatusPlaybackStarted()
{
	HMENU hMenu = GetMenu(mainHWND);
	EnableMenuItem(hMenu, ID_STOP_RECORD, MF_GRAYED);
	EnableMenuItem(hMenu, ID_STOP_PLAYBACK, MF_ENABLED);

	if (!emu_paused || !emu_launched)
		statusbar_post_text("Playback started");
	else
		statusbar_post_text("Playback started while paused");
}

LRESULT CALLBACK PlayMovieProc(HWND hwnd, UINT Message, WPARAM wParam,
                               LPARAM lParam)
{
	char tempbuf[MAX_PATH];

	HWND descriptionDialog;
	HWND authorDialog;
	static char path_buffer[_MAX_PATH];
	switch (Message)
	{
	case WM_INITDIALOG:
		descriptionDialog = GetDlgItem(hwnd, IDC_INI_DESCRIPTION);
		authorDialog = GetDlgItem(hwnd, IDC_INI_AUTHOR);
		SetDlgItemInt(hwnd, IDC_PAUSEAT_FIELD, 0, FALSE);

		SendMessage(descriptionDialog, EM_SETLIMITTEXT,
		            MOVIE_DESCRIPTION_DATA_SIZE, 0);
		SendMessage(authorDialog, EM_SETLIMITTEXT, MOVIE_AUTHOR_DATA_SIZE, 0);

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

		strcpy(tempbuf, Controls[0].Present ? "Present" : "Disconnected");
		if (Controls[0].Present && Controls[0].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[0].Present && Controls[0].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER1_TEXT2, tempbuf);

		strcpy(tempbuf, Controls[1].Present ? "Present" : "Disconnected");
		if (Controls[1].Present && Controls[1].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[1].Present && Controls[1].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER2_TEXT2, tempbuf);

		strcpy(tempbuf, Controls[2].Present ? "Present" : "Disconnected");
		if (Controls[2].Present && Controls[2].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[2].Present && Controls[2].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER3_TEXT2, tempbuf);

		strcpy(tempbuf, Controls[3].Present ? "Present" : "Disconnected");
		if (Controls[3].Present && Controls[3].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[3].Present && Controls[3].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER4_TEXT2, tempbuf);

		CheckDlgButton(hwnd, IDC_MOVIE_READONLY, VCR_getReadOnly());
		SetFocus(GetDlgItem(hwnd, IDC_INI_MOVIEFILE));
		goto refresh;
	// better than making it a macro or zillion-argument function

	case WM_CLOSE:
		EndDialog(hwnd, IDOK);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_OK:
		case IDOK:
			{
				VCR_coreStopped();

				{
					BOOL success;
					const unsigned int frame = GetDlgItemInt(
						hwnd, IDC_PAUSEAT_FIELD, &success, TRUE);
					if (success && frame > 0)
						pauseAtFrame = frame;
					else
						pauseAtFrame = -1;
				}

				GetDlgItemText(hwnd, IDC_INI_MOVIEFILE, tempbuf, MAX_PATH);

				VCR_setReadOnly(
					(BOOL)IsDlgButtonChecked(hwnd, IDC_MOVIE_READONLY));

				int result = vcr_start_playback(tempbuf, false);

				SetStatusPlaybackStarted();
				resumeEmu(TRUE);
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
				if (path.size() == 0)
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
	t_movie_header m_header = VCR_getHeaderInfo(tempbuf);

	SetDlgItemText(hwnd, IDC_ROM_INTERNAL_NAME, m_header.rom_name);

 	SetDlgItemText(hwnd, IDC_ROM_COUNTRY, country_code_to_country_name(m_header.rom_country).c_str());

	sprintf(tempbuf, "%X", (unsigned int)m_header.rom_crc1);
	SetDlgItemText(hwnd, IDC_ROM_CRC, tempbuf);

	SetDlgItemText(hwnd, IDC_MOVIE_VIDEO_TEXT, m_header.video_plugin_name);
	SetDlgItemText(hwnd, IDC_MOVIE_INPUT_TEXT, m_header.input_plugin_name);
	SetDlgItemText(hwnd, IDC_MOVIE_SOUND_TEXT, m_header.audio_plugin_name);
	SetDlgItemText(hwnd, IDC_MOVIE_RSP_TEXT, m_header.rsp_plugin_name);

	strcpy(tempbuf, (m_header.controller_flags & CONTROLLER_1_PRESENT)
		                ? "Present"
		                : "Disconnected");
	if (m_header.controller_flags & CONTROLLER_1_MEMPAK)
		strcat(tempbuf, " with mempak");
	if (m_header.controller_flags & CONTROLLER_1_RUMBLE)
		strcat(tempbuf, " with rumble");
	SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER1_TEXT, tempbuf);

	strcpy(tempbuf, (m_header.controller_flags & CONTROLLER_2_PRESENT)
		                ? "Present"
		                : "Disconnected");
	if (m_header.controller_flags & CONTROLLER_2_MEMPAK)
		strcat(tempbuf, " with mempak");
	if (m_header.controller_flags & CONTROLLER_2_RUMBLE)
		strcat(tempbuf, " with rumble");
	SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER2_TEXT, tempbuf);

	strcpy(tempbuf, (m_header.controller_flags & CONTROLLER_3_PRESENT)
		                ? "Present"
		                : "Disconnected");
	if (m_header.controller_flags & CONTROLLER_3_MEMPAK)
		strcat(tempbuf, " with mempak");
	if (m_header.controller_flags & CONTROLLER_3_RUMBLE)
		strcat(tempbuf, " with rumble");
	SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER3_TEXT, tempbuf);

	strcpy(tempbuf, (m_header.controller_flags & CONTROLLER_4_PRESENT)
		                ? "Present"
		                : "Disconnected");
	if (m_header.controller_flags & CONTROLLER_4_MEMPAK)
		strcat(tempbuf, " with mempak");
	if (m_header.controller_flags & CONTROLLER_4_RUMBLE)
		strcat(tempbuf, " with rumble");
	SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER4_TEXT, tempbuf);

	SetDlgItemText(hwnd, IDC_FROMSNAPSHOT_TEXT,
	               (m_header.startFlags & MOVIE_START_FROM_SNAPSHOT)
		               ? "Savestate"
		               : "Start");
	if (m_header.startFlags & MOVIE_START_FROM_EEPROM)
	{
		SetDlgItemTextA(hwnd, IDC_FROMSNAPSHOT_TEXT, "EEPROM");
	}

	sprintf(tempbuf, "%u  (%u input)", (int)m_header.length_vis,
	        (int)m_header.length_samples);
	SetDlgItemText(hwnd, IDC_MOVIE_FRAMES, tempbuf);

	if (m_header.vis_per_second == 0)
		m_header.vis_per_second = 60;

	double seconds = (double)m_header.length_vis / (double)m_header.
		vis_per_second;
	double minutes = seconds / 60.0;
	if ((bool)seconds)
		seconds = fmod(seconds, 60.0);
	double hours = minutes / 60.0;
	if ((bool)minutes)
		minutes = fmod(minutes, 60.0);

	if (hours >= 1.0)
		sprintf(tempbuf, "%d hours and %.1f minutes", (unsigned int)hours,
		        (float)minutes);
	else if (minutes >= 1.0)
		sprintf(tempbuf, "%d minutes and %.0f seconds", (unsigned int)minutes,
		        (float)seconds);
	else if (m_header.length_vis != 0)
		sprintf(tempbuf, "%.1f seconds", (float)seconds);
	else
		strcpy(tempbuf, "0 seconds");
	SetDlgItemText(hwnd, IDC_MOVIE_LENGTH, tempbuf);

	sprintf(tempbuf, "%lu", m_header.rerecord_count);
	SetDlgItemText(hwnd, IDC_MOVIE_RERECORDS, tempbuf);

	{
		// convert utf8 metadata to windows widechar
		WCHAR wszMeta[MOVIE_MAX_METADATA_SIZE];
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		                        m_header.author, -1, wszMeta,
		                        MOVIE_AUTHOR_DATA_SIZE))
		{
			SetLastError(0);
			SetWindowTextW(GetDlgItem(hwnd, IDC_INI_AUTHOR), wszMeta);
			if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
			{
				// not implemented on this system - convert as best we can to 1-byte characters and set with that
				// TODO: load unicows.dll instead so SetWindowTextW won't fail even on Win98/ME
				char ansiStr[MOVIE_AUTHOR_DATA_SIZE];
				WideCharToMultiByte(CP_ACP, 0, wszMeta, -1, ansiStr,
				                    MOVIE_AUTHOR_DATA_SIZE, NULL, NULL);
				SetWindowTextA(GetDlgItem(hwnd, IDC_INI_AUTHOR), ansiStr);

				if (ansiStr[0] == '\0')
					SetWindowTextA(GetDlgItem(hwnd, IDC_INI_AUTHOR),
					               "(too lazy to type name)");

				SetLastError(0);
			} else
			{
				if (wszMeta[0] == '\0')
					SetWindowTextW(GetDlgItem(hwnd, IDC_INI_AUTHOR),
					               L"(too lazy to type name)");
			}
		}
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		                        m_header.description, -1, wszMeta,
		                        MOVIE_DESCRIPTION_DATA_SIZE))
		{
			SetWindowTextW(GetDlgItem(hwnd, IDC_INI_DESCRIPTION), wszMeta);
			if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
			{
				char ansiStr[MOVIE_DESCRIPTION_DATA_SIZE];
				WideCharToMultiByte(CP_ACP, 0, wszMeta, -1, ansiStr,
				                    MOVIE_DESCRIPTION_DATA_SIZE, NULL, NULL);
				SetWindowTextA(GetDlgItem(hwnd, IDC_INI_DESCRIPTION), ansiStr);

				if (ansiStr[0] == '\0')
					SetWindowTextA(GetDlgItem(hwnd, IDC_INI_DESCRIPTION),
					               "(no description entered)");

				SetLastError(0);
			} else
			{
				if (wszMeta[0] == '\0')
					SetWindowTextW(GetDlgItem(hwnd, IDC_INI_DESCRIPTION),
					               L"(no description entered)");
			}
		}
	}

	return FALSE;
}

LRESULT CALLBACK RecordMovieProc(HWND hwnd, UINT Message, WPARAM wParam,
                                 LPARAM lParam)
{
	char tempbuf[MAX_PATH];
	char tempbuf2[MAX_PATH];
	int checked_movie_type;
	HWND descriptionDialog;
	HWND authorDialog;

	switch (Message)
	{
	case WM_INITDIALOG:

		checked_movie_type = Config.last_movie_type;
		descriptionDialog = GetDlgItem(hwnd, IDC_INI_DESCRIPTION);
		authorDialog = GetDlgItem(hwnd, IDC_INI_AUTHOR);

		SendMessage(descriptionDialog, EM_SETLIMITTEXT,
		            MOVIE_DESCRIPTION_DATA_SIZE, 0);
		SendMessage(authorDialog, EM_SETLIMITTEXT, MOVIE_AUTHOR_DATA_SIZE, 0);

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

		strcpy(tempbuf, Controls[0].Present ? "Present" : "Disconnected");
		if (Controls[0].Present && Controls[0].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[0].Present && Controls[0].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER1_TEXT2, tempbuf);

		strcpy(tempbuf, Controls[1].Present ? "Present" : "Disconnected");
		if (Controls[1].Present && Controls[1].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[1].Present && Controls[1].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER2_TEXT2, tempbuf);

		strcpy(tempbuf, Controls[2].Present ? "Present" : "Disconnected");
		if (Controls[2].Present && Controls[2].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[2].Present && Controls[2].Plugin ==
			controller_extension::rumblepak)
			strcat(tempbuf, " with rumble pak");
		SetDlgItemText(hwnd, IDC_MOVIE_CONTROLLER3_TEXT2, tempbuf);

		strcpy(tempbuf, Controls[3].Present ? "Present" : "Disconnected");
		if (Controls[3].Present && Controls[3].Plugin ==
			controller_extension::mempak)
			strcat(tempbuf, " with mempak");
		if (Controls[3].Present && Controls[3].Plugin ==
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
		switch (LOWORD(wParam))
		{
		case IDC_OK:
		case IDOK:
			{
				// turn WCHAR into UTF8
				WCHAR authorWC[MOVIE_AUTHOR_DATA_SIZE];
				char authorUTF8[MOVIE_AUTHOR_DATA_SIZE * 4];
				if (GetDlgItemTextW(hwnd, IDC_INI_AUTHOR, authorWC,
				                    MOVIE_AUTHOR_DATA_SIZE))
					WideCharToMultiByte(CP_UTF8, 0, authorWC, -1, authorUTF8,
					                    sizeof(authorUTF8), NULL, NULL);
				else
					GetDlgItemTextA(hwnd, IDC_INI_AUTHOR, authorUTF8,
					                MOVIE_AUTHOR_DATA_SIZE);

				Config.last_movie_author = std::string(authorUTF8);

				WCHAR descriptionWC[MOVIE_DESCRIPTION_DATA_SIZE];
				char descriptionUTF8[MOVIE_DESCRIPTION_DATA_SIZE * 4];
				if (GetDlgItemTextW(hwnd, IDC_INI_DESCRIPTION, descriptionWC,
				                    MOVIE_DESCRIPTION_DATA_SIZE))
					WideCharToMultiByte(CP_UTF8, 0, descriptionWC, -1,
					                    descriptionUTF8,
					                    sizeof(descriptionUTF8), NULL, NULL);
				else
					GetDlgItemTextA(hwnd, IDC_INI_DESCRIPTION, descriptionUTF8,
					                MOVIE_DESCRIPTION_DATA_SIZE);


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
				unsigned short flag = checked_movie_type ==
				                      IDC_FROMSNAPSHOT_RADIO
					                      ? MOVIE_START_FROM_SNAPSHOT
					                      : checked_movie_type ==
					                      IDC_FROMSTART_RADIO
					                      ? MOVIE_START_FROM_NOTHING
					                      : checked_movie_type ==
					                      IDC_FROMEEPROM_RADIO
					                      ? MOVIE_START_FROM_EEPROM
					                      : MOVIE_START_FROM_EXISTING_SNAPSHOT;
				Config.last_movie_type = checked_movie_type;

				bool allowClosing = true;
				if (flag == MOVIE_START_FROM_EXISTING_SNAPSHOT)
				{
					// The default directory we open the file dialog window in is the
					// parent directory of the last savestate that the user saved or loaded
					std::string path = wstring_to_string(show_persistent_open_dialog("o_movie_existing_snapshot", hwnd, L"*.st;*.savestate"));

					if (path.empty())
					{
						break;
					}

					st_path = path;
					st_medium = e_st_medium::path;

					std::string movie_path = strip_extension(path) + ".m64";

					strcpy(tempbuf, movie_path.c_str());

					if (std::filesystem::exists(movie_path))
					{
						sprintf(tempbuf2,
						        "\"%s\" already exists. Are you sure want to overwrite this movie?",
						        tempbuf);
						if (MessageBox(hwnd, tempbuf2, "VCR", MB_YESNO) ==
							IDNO)
							break;
					}
				}

				if (allowClosing)
				{
					vcr_start_record(tempbuf, flag);

					HMENU hMenu = GetMenu(mainHWND);
					EnableMenuItem(hMenu, ID_STOP_RECORD, MF_ENABLED);
					EnableMenuItem(hMenu, ID_STOP_PLAYBACK, MF_GRAYED);
					statusbar_post_text("Recording replay");

					EndDialog(hwnd, IDOK);
				}
			}
			break;
		case IDC_CANCEL:
		case IDCANCEL:
			EndDialog(hwnd, IDOK);
			break;
		case IDC_MOVIE_BROWSE:
			{
				auto path = show_persistent_save_dialog("s_movie", hwnd, L"*.m64;*.rec");

				if (path.size() == 0)
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

void OpenMoviePlaybackDialog()
{
	BOOL wasPaused = emu_paused && !MenuPaused;
	MenuPaused = FALSE;
	if (emu_launched && !emu_paused)
		pauseEmu(FALSE);

	DialogBox(GetModuleHandle(NULL),
	          MAKEINTRESOURCE(IDD_MOVIE_PLAYBACK_DIALOG), mainHWND,
	          (DLGPROC)PlayMovieProc);

	if (emu_launched && emu_paused && !wasPaused)
		resumeEmu(FALSE);
}

void OpenMovieRecordDialog()
{
	BOOL wasPaused = emu_paused && !MenuPaused;
	MenuPaused = FALSE;
	if (emu_launched && !emu_paused)
		pauseEmu(FALSE);

	DialogBox(GetModuleHandle(NULL),
	          MAKEINTRESOURCE(IDD_MOVIE_RECORD_DIALOG), mainHWND,
	          (DLGPROC)RecordMovieProc);

	if (emu_launched && emu_paused && !wasPaused)
		resumeEmu(FALSE);
}



void EnableEmulationMenuItems(BOOL emulationRunning)
{
	HMENU hMenu = GetMenu(mainHWND);

	if (emulationRunning)
	{
		EnableMenuItem(hMenu, EMU_STOP, MF_ENABLED);
		EnableMenuItem(hMenu, EMU_PAUSE, MF_ENABLED);
		EnableMenuItem(hMenu, EMU_FRAMEADVANCE, MF_ENABLED);
		EnableMenuItem(hMenu, ID_LOAD_LATEST, MF_ENABLED);
		EnableMenuItem(hMenu, EMU_PLAY, MF_ENABLED);
		EnableMenuItem(hMenu, FULL_SCREEN, MF_ENABLED);
		EnableMenuItem(hMenu, STATE_SAVE, MF_ENABLED);
		EnableMenuItem(hMenu, STATE_SAVEAS, MF_ENABLED);
		EnableMenuItem(hMenu, STATE_RESTORE, MF_ENABLED);
		EnableMenuItem(hMenu, STATE_LOAD, MF_ENABLED);
		EnableMenuItem(hMenu, GENERATE_BITMAP, MF_ENABLED);
		EnableMenuItem(hMenu, EMU_RESET, MF_ENABLED);
		EnableMenuItem(hMenu, REFRESH_ROM_BROWSER, MF_GRAYED);
		EnableMenuItem(hMenu, ID_RESTART_MOVIE, MF_ENABLED);
		EnableMenuItem(hMenu, ID_AUDIT_ROMS, MF_GRAYED);
		EnableMenuItem(hMenu, ID_FFMPEG_START, MF_DISABLED);
		EnableMenuItem(hMenu, IDC_GUI_TOOLBAR, MF_DISABLED);
		EnableMenuItem(hMenu, IDC_GUI_STATUSBAR, MF_DISABLED);

		if (dynacore)
			EnableMenuItem(hMenu, ID_TRACELOG, MF_DISABLED);
		else
			EnableMenuItem(hMenu, ID_TRACELOG, MF_ENABLED);

		if (!continue_vcr_on_restart_mode)
		{
			EnableMenuItem(hMenu, ID_START_RECORD, MF_ENABLED);
			EnableMenuItem(hMenu, ID_STOP_RECORD,
			               VCR_isRecording() ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(hMenu, ID_START_PLAYBACK, MF_ENABLED);
			EnableMenuItem(hMenu, ID_STOP_PLAYBACK,
			               (VCR_isRestarting() || VCR_isPlaying())
				               ? MF_ENABLED
				               : MF_GRAYED);
			EnableMenuItem(hMenu, ID_START_CAPTURE, MF_ENABLED);
			EnableMenuItem(hMenu, ID_START_CAPTURE_PRESET, MF_ENABLED);
			EnableMenuItem(hMenu, ID_END_CAPTURE,
			               VCR_isCapturing() ? MF_ENABLED : MF_GRAYED);
		}

		toolbar_on_emu_state_changed(1, 1);
	} else
	{
		EnableMenuItem(hMenu, EMU_STOP, MF_GRAYED);
		EnableMenuItem(hMenu, IDLOAD, MF_ENABLED);
		EnableMenuItem(hMenu, EMU_PAUSE, MF_GRAYED);
		EnableMenuItem(hMenu, EMU_FRAMEADVANCE, MF_GRAYED);
		EnableMenuItem(hMenu, ID_LOAD_LATEST, MF_GRAYED);
		EnableMenuItem(hMenu, EMU_PLAY, MF_GRAYED);
		EnableMenuItem(hMenu, FULL_SCREEN, MF_GRAYED);
		EnableMenuItem(hMenu, STATE_SAVE, MF_GRAYED);
		EnableMenuItem(hMenu, STATE_SAVEAS, MF_GRAYED);
		EnableMenuItem(hMenu, STATE_RESTORE, MF_GRAYED);
		EnableMenuItem(hMenu, STATE_LOAD, MF_GRAYED);
		EnableMenuItem(hMenu, GENERATE_BITMAP, MF_GRAYED);
		EnableMenuItem(hMenu, EMU_RESET, MF_GRAYED);
		EnableMenuItem(hMenu, REFRESH_ROM_BROWSER, MF_ENABLED);
		EnableMenuItem(hMenu, ID_RESTART_MOVIE, MF_GRAYED);
		EnableMenuItem(hMenu, ID_TRACELOG, MF_DISABLED);
		EnableMenuItem(hMenu, ID_AUDIT_ROMS, MF_ENABLED);
		EnableMenuItem(hMenu, ID_FFMPEG_START, MF_GRAYED);
		EnableMenuItem(hMenu, IDC_GUI_TOOLBAR, MF_ENABLED);
		EnableMenuItem(hMenu, IDC_GUI_STATUSBAR, MF_ENABLED);

		if (!continue_vcr_on_restart_mode)
		{
			EnableMenuItem(hMenu, ID_START_RECORD, MF_GRAYED);
			EnableMenuItem(hMenu, ID_STOP_RECORD, MF_GRAYED);
			EnableMenuItem(hMenu, ID_START_PLAYBACK, MF_GRAYED);
			EnableMenuItem(hMenu, ID_STOP_PLAYBACK, MF_GRAYED);
			EnableMenuItem(hMenu, ID_START_CAPTURE, MF_GRAYED);
			EnableMenuItem(hMenu, ID_START_CAPTURE_PRESET, MF_GRAYED);
			EnableMenuItem(hMenu, ID_END_CAPTURE, MF_GRAYED);
			LONG winstyle;
			winstyle = GetWindowLong(mainHWND, GWL_STYLE);
			winstyle |= WS_MAXIMIZEBOX;
			SetWindowLong(mainHWND, GWL_STYLE, winstyle);
			SetWindowPos(mainHWND, HWND_NOTOPMOST, 0, 0, 0, 0,
			             SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
			//Set on top
		}
		toolbar_on_emu_state_changed(0, 0);
	}

	if (Config.is_toolbar_enabled) CheckMenuItem(
		hMenu, IDC_GUI_TOOLBAR, MF_BYCOMMAND | MF_CHECKED);
	else CheckMenuItem(hMenu, IDC_GUI_TOOLBAR, MF_BYCOMMAND | MF_UNCHECKED);
	if (Config.is_statusbar_enabled) CheckMenuItem(
		hMenu, IDC_GUI_STATUSBAR, MF_BYCOMMAND | MF_CHECKED);
	else CheckMenuItem(hMenu, IDC_GUI_STATUSBAR, MF_BYCOMMAND | MF_UNCHECKED);
	if (Config.is_movie_loop_enabled) CheckMenuItem(
		hMenu, ID_LOOP_MOVIE, MF_BYCOMMAND | MF_CHECKED);
	else CheckMenuItem(hMenu, ID_LOOP_MOVIE, MF_BYCOMMAND | MF_UNCHECKED);
	if (Config.is_recent_movie_paths_frozen) CheckMenuItem(
		hMenu, ID_RECENTMOVIES_FREEZE, MF_BYCOMMAND | MF_CHECKED);
	if (Config.is_recent_scripts_frozen) CheckMenuItem(
		hMenu, ID_LUA_RECENT_FREEZE, MF_BYCOMMAND | MF_CHECKED);
	if (Config.is_recent_rom_paths_frozen) CheckMenuItem(
		hMenu, ID_RECENTROMS_FREEZE, MF_BYCOMMAND | MF_CHECKED);
}

static DWORD WINAPI SoundThread(LPVOID lpParam)
{
	while (emu_launched)
	{
		aiUpdate(1);
	}
	ExitThread(0);
}

static DWORD WINAPI ThreadFunc(LPVOID lpParam)
{
	auto start_time = std::chrono::high_resolution_clock::now();
	init_memory();
	romOpen_gfx();
	romOpen_input();
	romOpen_audio();

	dynacore = Config.core_type;

	emu_paused = 0;
	emu_launched = 1;

	sound_thread_handle = CreateThread(NULL, 0, SoundThread, NULL, 0,
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
	if (pauseAtFrame == 0 && VCR_isStartingAndJustRestarted())
	{
		while (emu_paused)
		{
			Sleep(10);
		}
		pauseEmu(FALSE);
		pauseAtFrame = -1;
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

void main_recent_roms_build(int32_t reset)
{
	HMENU h_menu = GetMenu(mainHWND);
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

int32_t main_recent_roms_run(uint16_t menu_item_id)
{
	const int index = menu_item_id - ID_RECENTROMS_FIRST;
	if (index >= 0 && index < Config.recent_rom_paths.size()) {
		strcpy(rom_path, Config.recent_rom_paths[index].c_str());
		CreateThread(NULL, 0, start_rom, rom_path, 0, &start_rom_id);
			return 	1;
	}
	return 0;
}

bool is_frame_skipped()
{
	if (!fast_forward || VCR_isCapturing())
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
	SetWindowText(mainHWND, (std::string(MUPEN_VERSION) + " - " + std::string(reinterpret_cast<char*>(ROM_HEADER.nom))).c_str());
}

BOOL IsMenuItemEnabled(HMENU hMenu, UINT uId)
{
	return !(GetMenuState(hMenu, uId, MF_BYCOMMAND) & (MF_DISABLED |
		MF_GRAYED));
}

void ProcessToolTips(LPARAM lParam, HWND hWnd)
{
	LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT)lParam;

	lpttt->hinst = app_instance;

	// Specify the resource identifier of the descriptive
	// text for the given button.
	HMENU hMenu = GetMenu(hWnd);

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

DWORD WINAPI UnpauseEmuAfterMenu(LPVOID lpParam)
{
	Sleep(60); // Wait for another thread to clear MenuPaused

	if (emu_paused && !AutoPause && MenuPaused)
	{
		resumeEmu(FALSE);
	}
	MenuPaused = FALSE;
	return 0;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	char path_buffer[_MAX_PATH];
	static PAINTSTRUCT ps;
	HMENU hMenu = GetMenu(hwnd);

	LuaWindowMessage(hwnd, Message, wParam, lParam);

	switch (Message)
	{
	case WM_EXECUTE_DISPATCHER:
		main_dispatcher_process();
		break;
	case WM_DROPFILES:
		{
			HDROP h_file = (HDROP)wParam;
			char fname[MAX_PATH] = {0};
			DragQueryFile(h_file, 0, fname, sizeof(fname));
			LocalFree(h_file);

			std::filesystem::path path = fname;
			std::string extension = to_lower(path.extension().string());

			if (extension == ".n64" || extension == ".z64" || extension == ".v64" || extension == ".rom")
			{
				strcpy(rom_path, fname);
				CreateThread(NULL, 0, start_rom, nullptr, 0, &start_rom_id);
			} else if (extension == ".m64")
			{
				if (!emu_launched) break;
				if (!VCR_getReadOnly()) VCR_toggleReadOnly();

				vcr_start_playback(fname, false);
				SetStatusPlaybackStarted();
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
				if ((int)wParam == Config.fast_forward_hotkey.key)
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
				if ((int)wParam == hotkey->key)
				{
					if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) == hotkey->
						shift
						&& ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) ==
						hotkey->ctrl
						&& ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) == hotkey->
						alt)
					{
						// printf("sent %s - %d\n", hotkey->identifier.c_str(), hotkey->command);
						SendMessage(mainHWND, WM_COMMAND, hotkey->command, 0);
						hit = TRUE;
					}
				}
			}

			if (emu_launched)
				keyDown(wParam, lParam);
			if (!hit)
				return DefWindowProc(hwnd, Message, wParam, lParam);
		}
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		if ((int)wParam == Config.fast_forward_hotkey.key) // fast-forward off
		{
			fast_forward = 0;
			ffup = true; //fuck it, timers.c is too weird
		}
		if (emu_launched)
			keyUp(wParam, lParam);
		return DefWindowProc(hwnd, Message, wParam, lParam);
	case WM_NOTIFY:
		{
			LPNMHDR l_header = (LPNMHDR)lParam;

			if (wParam == IDC_ROMLIST)
			{
				rombrowser_notify(lParam);
			}
			switch ((l_header)->code)
			{
			case TTN_NEEDTEXT:
				ProcessToolTips(lParam, hwnd);
				break;
			}
			return 0;
		}
	case WM_MOVE:
		{
			if (emu_launched && !FullScreenMode)
			{
				moveScreen((int)wParam, lParam);
			}
			RECT rect = {0};
			GetWindowRect(mainHWND, &rect);
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
	case WM_USER + 17: SetFocus(mainHWND);
		break;
	case WM_CREATE:
		GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
		update_screen_timer = SetTimer(hwnd, NULL, (uint32_t)(1000 / get_primary_monitor_refresh_rate()), NULL);
		commandline_start_rom();
		return TRUE;
	case WM_DESTROY:
		save_config();
		KillTimer(mainHWND, update_screen_timer);
		Gdiplus::GdiplusShutdown(gdiPlusToken);
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		if (confirm_user_exit())
		{
			DestroyWindow(mainHWND);
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
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
			lpMMI->ptMinTrackSize.x = MIN_WINDOW_W;
			lpMMI->ptMinTrackSize.y = MIN_WINDOW_H;
			// this might break small res with gfx plugin!!!
		}
		break;
	case WM_ENTERMENULOOP:
		AutoPause = emu_paused;
		if (!emu_paused)
		{
			MenuPaused = TRUE;
			pauseEmu(FALSE);
		}
		break;

	case WM_EXITMENULOOP:
		CreateThread(NULL, 0, UnpauseEmuAfterMenu, NULL, 0, NULL);
		break;
	case WM_ACTIVATE:
		UpdateWindow(hwnd);

		switch (LOWORD(wParam))
		{
		case WA_ACTIVE:
		case WA_CLICKACTIVE:
			if (Config.is_unfocused_pause_enabled && emu_paused && !AutoPause)
			{
				resumeEmu(FALSE);
				AutoPause = emu_paused;
			}
			break;

		case WA_INACTIVE:
			AutoPause = emu_paused && !MenuPaused;
			if (Config.is_unfocused_pause_enabled && !emu_paused
				/*(&& minimize*/ && !FullScreenMode)
			{
				MenuPaused = FALSE;
				pauseEmu(FALSE);
			} else if (Config.is_unfocused_pause_enabled && MenuPaused)
			{
				MenuPaused = FALSE;
			}
			break;
		default:
			break;
		}
		break;
	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case ID_MENU_LUASCRIPT_NEW:
				{
					NewLuaScript();
				}
				break;
			case ID_LUA_RECENT_FREEZE:
				CheckMenuItem(hMenu, ID_LUA_RECENT_FREEZE,
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
				MenuPaused = FALSE;
				if (!confirm_user_exit())
					break;
				if (emu_launched)
				{
					//close_rom(&Id);
					CreateThread(NULL, 0, close_rom, (LPVOID)1, 0, &close_rom_id);

				}
				break;

			case EMU_PAUSE:
				{
					if (!emu_paused)
					{
						pauseEmu(VCR_isActive());
					} else if (MenuPaused)
					{
						MenuPaused = FALSE;
						CheckMenuItem(GetMenu(mainHWND), EMU_PAUSE,
						              MF_BYCOMMAND | MFS_CHECKED);
					} else
					{
						resumeEmu(VCR_isActive());
					}
					break;
				}

			case EMU_FRAMEADVANCE:
				{
					MenuPaused = FALSE;
					frame_advancing = 1;
					vis_per_second = 0;
					// prevent old VI value from showing error if running at super fast speeds
					resumeEmu(TRUE); // maybe multithreading unsafe
				}
				break;

			case EMU_VCRTOGGLEREADONLY:
				VCR_toggleReadOnly();
				break;

			case ID_LOOP_MOVIE:
				VCR_toggleLoopMovie();
				break;
			case ID_RESTART_MOVIE:
				if (VCR_isPlaying())
				{
					VCR_setReadOnly(TRUE);
					vcr_start_playback(Config.recent_movie_paths[0], false);
					SetStatusPlaybackStarted();
				}
				break;
			case ID_REPLAY_LATEST:
				if (!emu_launched)
					break;
				// Overwrite prevention? Path sanity check (Leave to internal handling)?
				VCR_setReadOnly(TRUE);
				vcr_start_playback(Config.recent_movie_paths[0], false);
				SetStatusPlaybackStarted();
				break;
			case ID_RECENTMOVIES_FREEZE:
				CheckMenuItem(hMenu, ID_RECENTMOVIES_FREEZE,
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
						resumeEmu(FALSE);
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
				if (VCR_isRecording() && Config.is_reset_recording_enabled)
				{
					scheduled_restart = true;
					continue_vcr_on_restart_mode = true;
					statusbar_post_text("Writing restart to movie");
					break;
				}
				resetEmu();
				break;

			case ID_LOAD_CONFIG:
				{
					BOOL wasPaused = emu_paused && !MenuPaused;
					MenuPaused = FALSE;
					if (emu_launched && !emu_paused)
					{
						pauseEmu(FALSE);
					}
					configdialog_show();
					if (emu_launched && emu_paused && !wasPaused)
					{
						resumeEmu(FALSE);
					}
				}
				break;
			case ID_HELP_ABOUT:
				{
					BOOL wasMenuPaused = MenuPaused;
					MenuPaused = FALSE;
					configdialog_about();
					if (wasMenuPaused)
					{
						resumeEmu(TRUE);
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
					BOOL wasMenuPaused = MenuPaused;
					MenuPaused = FALSE;

					pauseEmu(TRUE);

					char ram_start[20] = {0};
					sprintf(ram_start, "0x%p", static_cast<void*>(rdram));

					char proc_name[MAX_PATH] = {0};
					GetModuleFileName(NULL, proc_name, MAX_PATH);
					_splitpath(proc_name, 0, 0, proc_name, 0);

					char stroop_c[1024] = {0};
					sprintf(stroop_c,
					        "<Emulator name=\"Mupen 5.0 RR\" processName=\"%s\" ramStart=\"%s\" endianness=\"little\"/>",
					        proc_name, ram_start);

					const auto stroop_str = std::string(stroop_c);
					if (MessageBoxA(mainHWND,
					                "Do you want to copy the generated STROOP config line to your clipboard?",
					                "STROOP",
					                MB_ICONINFORMATION | MB_TASKMODAL |
					                MB_YESNO) == IDYES)
					{
						copy_to_clipboard(mainHWND, stroop_str);
					}
					if (wasMenuPaused)
					{
						resumeEmu(TRUE);
					}
					break;
				}
			case IDLOAD:
				{
					BOOL wasMenuPaused = MenuPaused;
					MenuPaused = FALSE;

					const auto path = show_persistent_open_dialog("o_rom", mainHWND, L"*.n64;*.z64;*.v64;*.rom;*.bin;*.zip;*.usa;*.eur;*.jap");

					if (!path.empty())
					{
						strcpy(rom_path, wstring_to_string(path).c_str());
						CreateThread(nullptr, 0, start_rom, nullptr, NULL, &start_rom_id);
					}

					if (wasMenuPaused)
					{
						resumeEmu(TRUE);
					}
				}
				break;
			case ID_EMULATOR_EXIT:
				DestroyWindow(mainHWND);
				break;
			case FULL_SCREEN:
				if (emu_launched && !VCR_isCapturing())
				{
					FullScreenMode = 1 - FullScreenMode;
					gui_ChangeWindow();
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
					BOOL wasMenuPaused = MenuPaused;
					MenuPaused = FALSE;

					auto path = show_persistent_save_dialog("s_savestate", hwnd, L"*.st;*.savestate");
					if (path.empty())
					{
						break;
					}

					savestates_do(path, e_st_job::save);

					if (wasMenuPaused)
					{
						resumeEmu(TRUE);
					}
				}
				break;
			case STATE_RESTORE:
				savestates_do(st_slot, e_st_job::load);
				break;
			case STATE_LOAD:
				{
					BOOL wasMenuPaused = MenuPaused;
					MenuPaused = FALSE;

					auto path = show_persistent_open_dialog("o_state", hwnd, L"*.st;*.savestate;*.st0;*.st1;*.st2;*.st3;*.st4;*.st5;*.st6;*.st7;*.st8;*.st9,*.st10");
					if (path.size() == 0)
					{
						break;
					}

					savestates_do(path, e_st_job::load);

					if (wasMenuPaused)
					{
						resumeEmu(TRUE);
					}
				}
				break;
			case ID_START_RECORD:
				if (emu_launched)
					OpenMovieRecordDialog();
				break;
			case ID_STOP_RECORD:
				if (VCR_isRecording())
				{
					vcr_stop_record();
					ClearButtons();
					EnableMenuItem(hMenu, ID_STOP_RECORD, MF_GRAYED);
					EnableMenuItem(hMenu, ID_START_RECORD, MF_ENABLED);
					statusbar_post_text("Recording stopped");
				}
				break;
			case ID_START_PLAYBACK:
				if (emu_launched)
					OpenMoviePlaybackDialog();

				break;
			case ID_STOP_PLAYBACK:
				if (VCR_isPlaying())
				{
					vcr_stop_playback(true);
					ClearButtons();
					EnableMenuItem(hMenu, ID_STOP_PLAYBACK, MF_GRAYED);
					EnableMenuItem(hMenu, ID_START_PLAYBACK, MF_ENABLED);
					statusbar_post_text("Playback stopped");
				}
				break;

			case ID_FFMPEG_START:
				{
					auto err = VCR_StartFFmpegCapture(
						"ffmpeg_out.mp4",
						"-pixel_format yuv420p -loglevel debug -y");
					if (err == INIT_SUCCESS)
					{
						//SetWindowPos(mainHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);  //Set on top avichg
						EnableMenuItem(hMenu, ID_START_CAPTURE, MF_GRAYED);
						EnableMenuItem(hMenu, ID_START_CAPTURE_PRESET,
						               MF_GRAYED);
						EnableMenuItem(hMenu, ID_FFMPEG_START, MF_GRAYED);
						EnableMenuItem(hMenu, ID_END_CAPTURE, MF_ENABLED);
						EnableMenuItem(hMenu, FULL_SCREEN, MF_GRAYED);
						statusbar_post_text("Recording AVI with FFmpeg");
						EnableEmulationMenuItems(TRUE);
					} else
						printf("Start capture error: %d\n", err);
					break;
				}

			case ID_START_CAPTURE_PRESET:
			case ID_START_CAPTURE:
				if (emu_launched)
				{
					BOOL wasPaused = emu_paused && !MenuPaused;
					MenuPaused = FALSE;
					if (emu_launched && !emu_paused)
						pauseEmu(FALSE);

					auto path = show_persistent_save_dialog("s_capture", hwnd, L"*.avi");
					if (path.size() == 0)
					{
						break;
					}

					// pass false to startCapture when "last preset" option was choosen
					if (vcr_start_capture(wstring_to_string(path).c_str(), LOWORD(wParam) == ID_START_CAPTURE) < 0)
					{
						MessageBox(NULL, "Couldn't start capturing.", "VCR", MB_OK);
					} else
					{
						EnableMenuItem(hMenu, ID_START_CAPTURE, MF_GRAYED);
						EnableMenuItem(hMenu, ID_START_CAPTURE_PRESET,
						               MF_GRAYED);
						EnableMenuItem(hMenu, ID_FFMPEG_START, MF_GRAYED);
						EnableMenuItem(hMenu, ID_END_CAPTURE, MF_ENABLED);
						EnableMenuItem(hMenu, FULL_SCREEN, MF_GRAYED);
						statusbar_post_text("Recording AVI");
						EnableEmulationMenuItems(TRUE);
					}

					if (emu_launched && emu_paused && !wasPaused)
						resumeEmu(FALSE);
				}

				break;
			case ID_END_CAPTURE:
				if (vcr_stop_capture() < 0)
					MessageBox(NULL, "Couldn't stop capturing.", "VCR", MB_OK);
				else
				{
					SetWindowPos(mainHWND, HWND_TOP, 0, 0, 0, 0,
					             SWP_NOMOVE | SWP_NOSIZE);
					EnableMenuItem(hMenu, ID_END_CAPTURE, MF_GRAYED);
					EnableMenuItem(hMenu, ID_START_CAPTURE, MF_ENABLED);
					EnableMenuItem(hMenu, ID_FFMPEG_START, MF_ENABLED);
					EnableMenuItem(hMenu, ID_START_CAPTURE_PRESET, MF_ENABLED);
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
				CheckMenuItem(hMenu, ID_RECENTROMS_FREEZE,
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
					hMenu, IDC_GUI_TOOLBAR, MF_BYCOMMAND | (Config.is_toolbar_enabled ? MF_CHECKED : MF_UNCHECKED));
				break;
			case IDC_GUI_STATUSBAR:
				Config.is_statusbar_enabled ^= true;
				statusbar_set_visibility(Config.is_statusbar_enabled);
				CheckMenuItem(
					hMenu, IDC_GUI_STATUSBAR, MF_BYCOMMAND | (Config.is_statusbar_enabled ? MF_CHECKED : MF_UNCHECKED));
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
				if (LOWORD(wParam) >= ID_CURRENTSAVE_1 && LOWORD(wParam)
					<= ID_CURRENTSAVE_10)
				{
					auto slot = LOWORD(wParam) - ID_CURRENTSAVE_1;
					st_slot = slot;

					// set checked state for only the currently selected save
					for (int i = ID_CURRENTSAVE_1; i < ID_CURRENTSAVE_10; ++i)
					{
						CheckMenuItem(hMenu, i, MF_UNCHECKED);
					}
					CheckMenuItem(hMenu, LOWORD(wParam), MF_CHECKED);

				} else if (LOWORD(wParam) >= ID_SAVE_1 && LOWORD(wParam) <=
					ID_SAVE_10)
				{
					auto slot = LOWORD(wParam) - ID_SAVE_1;
					// if emu is paused and no console state is changing, we can safely perform st op instantly
					savestates_do(slot, e_st_job::save);
				} else if (LOWORD(wParam) >= ID_LOAD_1 && LOWORD(wParam) <=
					ID_LOAD_10)
				{
					auto slot = LOWORD(wParam) - ID_LOAD_1;
					savestates_do(slot, e_st_job::load);
				} else if (LOWORD(wParam) >= ID_RECENTROMS_FIRST &&
					LOWORD(wParam) < (ID_RECENTROMS_FIRST + Config.
						recent_rom_paths.size()))
				{
					main_recent_roms_run(LOWORD(wParam));
				} else if (LOWORD(wParam) >= ID_RECENTMOVIES_FIRST &&
					LOWORD(wParam) < (ID_RECENTMOVIES_FIRST + Config.
						recent_movie_paths.size()))
				{
					if (vcr_recent_movies_play(LOWORD(wParam)) != SUCCESS)
					{
						statusbar_post_text("Couldn't load movie");
						break;
					}
					// should probably make this code from the ID_REPLAY_LATEST case into a function on its own
					// because now it's used here too
					EnableMenuItem(hMenu, ID_STOP_RECORD, MF_GRAYED);
					EnableMenuItem(hMenu, ID_STOP_PLAYBACK, MF_ENABLED);

					if (!emu_paused || !emu_launched)
						statusbar_post_text("Playback started");
					else
						statusbar_post_text("Playback started while paused");
				} else if (LOWORD(wParam) >= ID_LUA_RECENT && LOWORD(wParam) < (
					ID_LUA_RECENT + Config.recent_lua_script_paths.size()))
				{
					printf("run recent script\n");
					lua_recent_scripts_run(LOWORD(wParam));
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

// Loads various variables from the current config state
void LoadConfigExternals()
{
	if (VCR_isLooping() != (bool)Config.is_movie_loop_enabled)
		VCR_toggleLoopMovie();
}

// kaboom
LONG WINAPI ExceptionReleaseTarget(_EXCEPTION_POINTERS* ExceptionInfo)
{
	// generate crash log

	char crashLog[1024 * 4] = {0};
	CrashHelper::GenerateLog(ExceptionInfo, crashLog);

	FILE* f = fopen("crash.log", "w+");
	fwrite(crashLog, sizeof(crashLog), 1, f);
	fclose(f);

	bool is_continuable = !(ExceptionInfo->ExceptionRecord->ExceptionFlags &
		EXCEPTION_NONCONTINUABLE);

	int result = 0;

	if (is_continuable) {
		TaskDialog(mainHWND, app_instance, L"Error",
			L"An error has occured", L"A crash log has been automatically generated. You can choose to continue program execution.", TDCBF_RETRY_BUTTON | TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, &result);
	} else {
		TaskDialog(mainHWND, app_instance, L"Error",
			L"An error has occured", L"A crash log has been automatically generated. The program will now exit.", TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, &result);
	}

	if (result == IDCLOSE) {
		return EXCEPTION_EXECUTE_HANDLER;
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}


int WINAPI WinMain(
	HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	AllocConsole();
	FILE* f = 0;
	freopen_s(&f, "CONIN$", "r", stdin);
	freopen_s(&f, "CONOUT$", "w", stdout);
	freopen_s(&f, "CONOUT$", "w", stderr);
#endif

	app_path = get_app_full_path();
	app_instance = hInstance;

	commandline_set();

	// ensure folders exist!
	CreateDirectory((app_path + "save").c_str(), NULL);
	CreateDirectory((app_path + "Mempaks").c_str(), NULL);
	CreateDirectory((app_path + "Lang").c_str(), NULL);
	CreateDirectory((app_path + "ScreenShots").c_str(), NULL);
	CreateDirectory((app_path + "plugin").c_str(), NULL);

	emu_launched = 0;
	emu_paused = 1;

	load_config();

	WNDCLASSEX wc = {0};
	HWND hwnd;
	MSG msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_M64ICONBIG));
	wc.hIconSm = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_M64ICONSMALL));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = g_szClassName;
	wc.lpfnWndProc = WndProc;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MYMENU);

	RegisterClassEx(&wc);

	HACCEL accelerators = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL));

	hwnd = CreateWindowEx(
		0,
		g_szClassName,
		MUPEN_VERSION,
		WS_OVERLAPPEDWINDOW | WS_EX_COMPOSITED,
		Config.window_x, Config.window_y, Config.window_width,
		Config.window_height,
		NULL, NULL, hInstance, NULL);

	mainHWND = hwnd;
	ShowWindow(hwnd, nCmdShow);

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

	vcr_recent_movies_build();
	lua_recent_scripts_build();
	main_recent_roms_build();

	EnableEmulationMenuItems(0);
	LoadConfigExternals();

	//warning, this is ignored when debugger is attached (like visual studio)
	SetUnhandledExceptionFilter(ExceptionReleaseTarget);

	// raise noncontinuable exception (impossible to recover from it)
	//RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, NULL, NULL);
	//
	// raise continuable exception
	//RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, NULL, NULL);

	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if (!TranslateAccelerator(mainHWND, accelerators, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		for (t_hotkey* hotkey : hotkeys)
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
