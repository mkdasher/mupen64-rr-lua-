/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <spdlog/logger.h>
#include <gui/features/Dispatcher.h>
#include <Plugin.h>

#define CURRENT_VERSION L"1.2.0"

#define WM_FOCUS_MAIN_WINDOW (WM_USER + 17)
#define WM_EXECUTE_DISPATCHER (WM_USER + 18)
#define WM_INVALIDATE_LUA (WM_USER + 23)

extern BOOL CALLBACK CfgDlgProc(HWND hwnd, UINT Message, WPARAM wParam,
                                LPARAM lParam);

extern core_params g_core;
extern bool g_frame_changed;

extern DWORD g_ui_thread_id;

extern int g_last_wheel_delta;

extern HWND g_main_hwnd;
extern HINSTANCE g_app_instance;

extern HWND g_hwnd_plug;
extern DWORD start_rom_id;

extern std::filesystem::path g_app_path;
extern std::shared_ptr<Dispatcher> g_main_window_dispatcher;

/**
 * \brief Currently loaded ROM's path, or empty.
 */
extern std::filesystem::path g_rom_path;

/**
 * \brief Whether the statusbar needs to be updated with new input information
 */
extern bool is_primary_statusbar_invalidated;

/**
 * The view ff flag. Combined with other flags to determine the core fast-forward value.
 */
extern bool g_fast_forward;

std::wstring get_mupen_name();

/**
 * \brief Pauses the emulation during the object's lifetime, resuming it if previously paused upon being destroyed
 */
struct BetterEmulationLock {
private:
    bool was_paused;

public:
    BetterEmulationLock();
    ~BetterEmulationLock();
};

typedef struct
{
    long width;
    long height;
    long statusbar_height;
} t_window_info;

extern t_window_info window_info;

static bool task_is_playback(core_vcr_task task)
{
    return task == task_playback || task == task_start_playback_from_reset || task ==
        task_start_playback_from_snapshot;
}

static bool vcr_is_task_recording(core_vcr_task task)
{
    return task == task_recording || task == task_start_recording_from_reset || task == task_start_recording_from_existing_snapshot || task ==
        task_start_recording_from_snapshot;
}


t_window_info get_window_info();

/**
 * \brief Demands user confirmation for an exit action
 * \return Whether the action is allowed
 * \remarks If the user has chosen to not use modals, this function will return true by default
 */
bool confirm_user_exit();

/**
 * \brief Whether the current execution is on the UI thread
 */
bool is_on_gui_thread();

/**
 * Shows an error dialog for a core result. If the result indicates no error, no work is done.
 * \param result The result to show an error dialog for.
 * \param hwnd The parent window handle for the spawned dialog. If null, the main window is used.
 * \returns Whether the function was able to show an error dialog.
 */
bool show_error_dialog_for_result(core_result result, void* hwnd = nullptr);

/**
 * Represents the result of a plugin discovery operation.
 */
typedef struct
{
    /**
     * The discovered plugins matching the plugin API surface.
     */
    std::vector<std::unique_ptr<Plugin>> plugins;

    /**
     * Vector of discovered plugins and their results.
     */
    std::vector<std::pair<std::filesystem::path, std::wstring>> results;

} t_plugin_discovery_result;

/**
 * Performs a plugin discovery operation.
 */
t_plugin_discovery_result do_plugin_discovery();

std::filesystem::path get_saves_directory();

#pragma region Dialog IDs

#define VIEW_DLG_MOVIE_OVERWRITE_WARNING "VIEW_DLG_MOVIE_OVERWRITE_WARNING"
#define VIEW_DLG_RESET_SETTINGS "VIEW_DLG_RESET_SETTINGS"
#define VIEW_DLG_RESET_PLUGIN_SETTINGS "VIEW_DLG_RESET_PLUGIN_SETTINGS"
#define VIEW_DLG_LAG_EXCEEDED "VIEW_DLG_LAG_EXCEEDED"
#define VIEW_DLG_CLOSE_ROM_WARNING "VIEW_DLG_CLOSE_ROM_WARNING"
#define VIEW_DLG_HOTKEY_CONFLICT "VIEW_DLG_HOTKEY_CONFLICT"
#define VIEW_DLG_UPDATE_DIALOG "VIEW_DLG_UPDATE_DIALOG"
#define VIEW_DLG_PLUGIN_LOAD_ERROR "VIEW_DLG_PLUGIN_LOAD_ERROR"
#define VIEW_DLG_RAMSTART "VIEW_DLG_RAMSTART"

#pragma endregion
