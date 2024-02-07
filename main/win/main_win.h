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
#define MUPEN_VERSION "Mupen 64 1.1.7"

#define WM_EXECUTE_DISPATCHER (WM_USER + 10)
extern BOOL CALLBACK CfgDlgProc(HWND hwnd, UINT Message, WPARAM wParam,
                                LPARAM lParam);

extern char rom_path[MAX_PATH];

extern int last_wheel_delta;

// TODO: remove
extern int recording;

extern HWND mainHWND;
extern HINSTANCE app_instance;

extern HWND hwnd_plug;
extern HANDLE EmuThreadHandle;
extern DWORD start_rom_id;
extern DWORD close_rom_id;

void main_dispatcher_invoke(const std::function<void()>& func);
extern std::string app_path;
extern void resetEmu();
extern void resumeEmu(BOOL quiet);
extern void pauseEmu(BOOL quiet);
/**
 * \brief Starts the rom from the path contained in <c>rom_path</c>
 */
DWORD WINAPI start_rom(LPVOID lpParam);
DWORD WINAPI close_rom(LPVOID lpParam);

extern BOOL continue_vcr_on_restart_mode;
void main_recent_roms_build(int32_t reset = 0);
void main_recent_roms_add(const std::string& path);
int32_t main_recent_roms_run(uint16_t menu_item_id);




/**
 * \brief Whether the statusbar needs to be updated with new input information
 */
extern bool is_primary_statusbar_invalidated;

/**
 * \brief Updates the titlebar to reflect the current application state
 */
void update_titlebar();

/**
 * \brief Notifies the frontend of speed modifier changing
 * \param value The new speed modifier
 */
void on_speed_modifier_changed(int32_t value);
