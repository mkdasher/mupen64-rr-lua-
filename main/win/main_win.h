/***************************************************************************
						  main_win.h  -  description
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

#pragma once

#include <Windows.h>
#include <string>
#include <functional>
constexpr auto mupen_version = "Mupen 64 1.1.6";

extern BOOL CALLBACK cfg_dlg_proc(HWND hwnd, UINT message, WPARAM w_param,
                                LPARAM l_param);

extern char temp_message[MAX_PATH];
extern char rom_path[MAX_PATH];

// TODO: use state enum
extern int emu_launched; // emu_emulating
extern int emu_paused;

// TODO: remove
extern int recording;

extern HWND main_hwnd;
extern HINSTANCE app_instance;

// TODO: rename
extern BOOL fast_forward;
extern char statusmsg[800];

extern HWND hwnd_plug;
extern HANDLE emu_thread_handle;
extern DWORD emu_id;
extern DWORD start_rom_id;
extern DWORD close_rom_id;

void main_dispatcher_invoke(const std::function<void()>& func);
extern std::string app_path;
extern void enable_emulation_menu_items(BOOL);
BOOL is_menu_item_enabled(HMENU h_menu, UINT u_id);
extern void reset_emu();
extern void resume_emu(BOOL quiet);
extern void pause_emu(BOOL quiet);
extern void open_movie_playback_dialog();
extern void open_movie_record_dialog();
/**
 * \brief Starts the rom from the path contained in <c>rom_path</c>
 */
DWORD WINAPI start_rom(LPVOID lp_param);
DWORD WINAPI close_rom(LPVOID lpParam);

extern BOOL continue_vcr_on_restart_mode;
extern BOOL ignore_error_emulation;
extern int frame_count;
extern long long total_vi;
void main_recent_roms_build(int32_t reset = 0);
void main_recent_roms_add(const std::string& path);
int32_t main_recent_roms_run(uint16_t menu_item_id);

extern bool is_frame_skipped();

void reset_titlebar();
