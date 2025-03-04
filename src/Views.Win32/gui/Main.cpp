/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <AsyncExecutor.h>
#include <Config.h>
#include <FrontendService.h>
#include <IOService.h>
#include <Messenger.h>
#include <Plugin.h>
#include <strsafe.h>
#include <capture/EncodingManager.h>
#include <gui/Commandline.h>
#include <gui/Loggers.h>
#include <gui/Main.h>
#include <gui/features/Cheats.h>
#include <gui/features/Compare.h>
#include <gui/features/ConfigDialog.h>
#include <gui/features/CoreDbg.h>
#include <gui/features/CrashHelper.h>
#include <gui/features/Dispatcher.h>
#include <gui/features/MGECompositor.h>
#include <gui/features/MovieDialog.h>
#include <gui/features/PianoRoll.h>
#include <gui/features/RecentMenu.h>
#include <gui/features/RomBrowser.h>
#include <gui/features/Runner.h>
#include <gui/features/Seeker.h>
#include <gui/features/Statusbar.h>
#include <gui/features/UpdateChecker.h>
#include <gui/wrapper/PersistentPathDialog.h>
#include <lua/LuaConsole.h>
#include <lua/LuaService.h>
#include <spdlog/sinks/basic_file_sink.h>

// Throwaway actions which can be spammed get keys as to not clog up the async executor queue
#define ASYNC_KEY_CLOSE_ROM (1)
#define ASYNC_KEY_START_ROM (2)
#define ASYNC_KEY_RESET_ROM (3)
#define ASYNC_KEY_PLAY_MOVIE (4)

core_params g_core{};
bool g_frame_changed = true;

DWORD g_ui_thread_id;
HWND g_hwnd_plug;
MMRESULT g_ui_timer;
HWND g_main_hwnd;
HMENU g_main_menu;
HMENU g_recent_roms_menu;
HMENU g_recent_movies_menu;
HMENU g_recent_lua_menu;
HINSTANCE g_app_instance;
std::filesystem::path g_app_path;
std::shared_ptr<Dispatcher> g_main_window_dispatcher;

int g_last_wheel_delta = 0;
bool g_paused_before_menu;
bool g_paused_before_focus;
bool g_in_menu_loop;
bool g_vis_since_input_poll_warning_dismissed;
bool g_emu_starting;
bool g_fast_forward;

ULONG_PTR gdi_plus_token;

/**
 * \brief List of lua environment map keys running before emulation stopped
 */
std::vector<HWND> g_previously_running_luas;

std::shared_ptr<Plugin> g_video_plugin;
std::shared_ptr<Plugin> g_audio_plugin;
std::shared_ptr<Plugin> g_input_plugin;
std::shared_ptr<Plugin> g_rsp_plugin;

std::filesystem::path g_rom_path;

constexpr auto WND_CLASS = L"myWindowClass";

const std::map<cfg_action, int> ACTION_ID_MAP = {
{ACTION_FASTFORWARD_ON, IDM_FASTFORWARD_ON},
{ACTION_FASTFORWARD_OFF, IDM_FASTFORWARD_OFF},
{ACTION_GAMESHARK_ON, IDM_GS_ON},
{ACTION_GAMESHARK_OFF, IDM_GS_OFF},
{ACTION_FRAME_ADVANCE, IDM_FRAMEADVANCE},
{ACTION_MULTI_FRAME_ADVANCE, IDM_MULTI_FRAME_ADVANCE},
{ACTION_MULTI_FRAME_ADVANCE_INC, IDM_MULTI_FRAME_ADVANCE_INC},
{ACTION_MULTI_FRAME_ADVANCE_DEC, IDM_MULTI_FRAME_ADVANCE_DEC},
{ACTION_MULTI_FRAME_ADVANCE_RESET, IDM_MULTI_FRAME_ADVANCE_RESET},
{ACTION_SPEED_DOWN, IDM_SPEED_DOWN},
{ACTION_SPEED_UP, IDM_SPEED_UP},
{ACTION_SPEED_RESET, IDM_SPEED_RESET},
{ACTION_PAUSE, IDM_PAUSE},
{ACTION_TOGGLE_READONLY, IDM_VCR_READONLY},
{ACTION_TOGGLE_MOVIE_LOOP, IDM_LOOP_MOVIE},
{ACTION_START_MOVIE_PLAYBACK, IDM_START_MOVIE_PLAYBACK},
{ACTION_START_MOVIE_RECORDING, IDM_START_MOVIE_RECORDING},
{ACTION_STOP_MOVIE, IDM_STOP_MOVIE},
{ACTION_CREATE_MOVIE_BACKUP, IDM_CREATE_MOVIE_BACKUP},
{ACTION_TAKE_SCREENSHOT, IDM_SCREENSHOT},
{ACTION_PLAY_LATEST_MOVIE, IDM_PLAY_LATEST_MOVIE},
{ACTION_LOAD_LATEST_SCRIPT, IDM_LOAD_LATEST_LUA},
{ACTION_NEW_LUA, IDM_LOAD_LUA},
{ACTION_CLOSE_ALL_LUA, IDM_CLOSE_ALL_LUA},
{ACTION_LOAD_ROM, IDM_LOAD_ROM},
{ACTION_CLOSE_ROM, IDM_CLOSE_ROM},
{ACTION_RESET_ROM, IDM_RESET_ROM},
{ACTION_LOAD_LATEST_ROM, IDM_LOAD_LATEST_ROM},
{ACTION_FULLSCREEN, IDM_FULLSCREEN},
{ACTION_SETTINGS, IDM_SETTINGS},
{ACTION_TOGGLE_STATUSBAR, IDM_STATUSBAR},
{ACTION_REFRESH_ROM_BROWSER, IDM_REFRESH_ROMBROWSER},
{ACTION_OPEN_SEEKER, IDM_SEEKER},
{ACTION_OPEN_RUNNER, IDM_RUNNER},
{ACTION_OPEN_PIANO_ROLL, IDM_PIANO_ROLL},
{ACTION_OPEN_CHEATS, IDM_CHEATS},
{ACTION_SAVE_SLOT, IDM_SAVE_SLOT},
{ACTION_LOAD_SLOT, IDM_LOAD_SLOT},
{ACTION_SAVE_AS, IDM_SAVE_STATE_AS},
{ACTION_LOAD_AS, IDM_LOAD_STATE_AS},
{ACTION_UNDO_LOAD_STATE, IDM_UNDO_LOAD_STATE},
{ACTION_SAVE_SLOT1, (ID_SAVE_1 - 1) + 1},
{ACTION_SAVE_SLOT2, (ID_SAVE_1 - 1) + 2},
{ACTION_SAVE_SLOT3, (ID_SAVE_1 - 1) + 3},
{ACTION_SAVE_SLOT4, (ID_SAVE_1 - 1) + 4},
{ACTION_SAVE_SLOT5, (ID_SAVE_1 - 1) + 5},
{ACTION_SAVE_SLOT6, (ID_SAVE_1 - 1) + 6},
{ACTION_SAVE_SLOT7, (ID_SAVE_1 - 1) + 7},
{ACTION_SAVE_SLOT8, (ID_SAVE_1 - 1) + 8},
{ACTION_SAVE_SLOT9, (ID_SAVE_1 - 1) + 9},
{ACTION_SAVE_SLOT10, (ID_SAVE_1 - 1) + 10},
{ACTION_LOAD_SLOT1, (ID_LOAD_1 - 1) + 1},
{ACTION_LOAD_SLOT2, (ID_LOAD_1 - 1) + 2},
{ACTION_LOAD_SLOT3, (ID_LOAD_1 - 1) + 3},
{ACTION_LOAD_SLOT4, (ID_LOAD_1 - 1) + 4},
{ACTION_LOAD_SLOT5, (ID_LOAD_1 - 1) + 5},
{ACTION_LOAD_SLOT6, (ID_LOAD_1 - 1) + 6},
{ACTION_LOAD_SLOT7, (ID_LOAD_1 - 1) + 7},
{ACTION_LOAD_SLOT8, (ID_LOAD_1 - 1) + 8},
{ACTION_LOAD_SLOT9, (ID_LOAD_1 - 1) + 9},
{ACTION_LOAD_SLOT10, (ID_LOAD_1 - 1) + 10},
{ACTION_SELECT_SLOT1, (IDM_SELECT_1 - 1) + 1},
{ACTION_SELECT_SLOT2, (IDM_SELECT_1 - 1) + 2},
{ACTION_SELECT_SLOT3, (IDM_SELECT_1 - 1) + 3},
{ACTION_SELECT_SLOT4, (IDM_SELECT_1 - 1) + 4},
{ACTION_SELECT_SLOT5, (IDM_SELECT_1 - 1) + 5},
{ACTION_SELECT_SLOT6, (IDM_SELECT_1 - 1) + 6},
{ACTION_SELECT_SLOT7, (IDM_SELECT_1 - 1) + 7},
{ACTION_SELECT_SLOT8, (IDM_SELECT_1 - 1) + 8},
{ACTION_SELECT_SLOT9, (IDM_SELECT_1 - 1) + 9},
{ACTION_SELECT_SLOT10, (IDM_SELECT_1 - 1) + 10},
};

std::wstring get_mupen_name()
{
#ifdef _DEBUG
#define BUILD_TARGET_INFO L"-debug"
#else
#define BUILD_TARGET_INFO L""
#endif

#ifdef UNICODE
#define CHARSET_INFO L""
#else
#define CHARSET_INFO L"-a"
#endif

#ifdef _M_X64
#define ARCH_INFO L"-x64"
#else
#define ARCH_INFO L""
#endif

#define BASE_NAME L"Mupen 64 "

    std::wstring version_suffix = VERSION_SUFFIX;
    if (version_suffix.empty())
    {
        version_suffix = L"-" __DATE__ L" " __TIME__;
    }

    return BASE_NAME CURRENT_VERSION + version_suffix + ARCH_INFO CHARSET_INFO BUILD_TARGET_INFO;
}

/// Prompts the user to change their plugin selection.
static void prompt_plugin_change()
{
    auto result = FrontendService::show_multiple_choice_dialog(
    {L"Choose Default Plugins", L"Change Plugins", L"Cancel"},
    L"One or more plugins couldn't be loaded.\r\nHow would you like to proceed?",
    L"Core",
    fsvc_error);

    if (result == 0)
    {
        auto plugin_discovery_result = do_plugin_discovery();
        
        auto first_video_plugin = std::ranges::find_if(plugin_discovery_result.plugins, [](const auto& plugin)
        {
        	return plugin->type() == plugin_video;
        });
        
        auto first_audio_plugin = std::ranges::find_if(plugin_discovery_result.plugins, [](const auto& plugin)
        {
        	return plugin->type() == plugin_audio;
        });
        
        auto first_input_plugin = std::ranges::find_if(plugin_discovery_result.plugins, [](const auto& plugin)
        {
        	return plugin->type() == plugin_input;
        });
        
        auto first_rsp_plugin = std::ranges::find_if(plugin_discovery_result.plugins, [](const auto& plugin)
        {
        	return plugin->type() == plugin_rsp;
        });

        if (first_video_plugin != plugin_discovery_result.plugins.end())
        {
        	g_config.selected_video_plugin = first_video_plugin->get()->path();
        }
        
        if (first_audio_plugin != plugin_discovery_result.plugins.end())
        {
        	g_config.selected_audio_plugin = first_audio_plugin->get()->path();
        }
        
        if (first_input_plugin != plugin_discovery_result.plugins.end())
        {
        	g_config.selected_input_plugin = first_input_plugin->get()->path();
        }
        
        if (first_rsp_plugin != plugin_discovery_result.plugins.end())
        {
        	g_config.selected_rsp_plugin = first_rsp_plugin->get()->path();
        }

        return;
    }

    if (result == 1)
    {
        g_config.settings_tab = 0;
        SendMessage(g_main_hwnd, WM_COMMAND, MAKEWPARAM(IDM_SETTINGS, 0), 0);
    }
}

bool show_error_dialog_for_result(const core_result result, void* hwnd)
{
    if (result == Res_Ok || result == ST_Cancelled || result == VCR_Cancelled || result == VCR_InvalidControllers)
    {
        return false;
    }

    g_view_logger->error("[View] show_error_dialog_for_result: CoreType::{}", static_cast<int32_t>(result));

    std::wstring module;
    std::wstring error;

    switch (result)
    {
#pragma region VCR
    case VCR_InvalidFormat:
        module = L"VCR";
        error = L"The provided data has an invalid format.";
        break;
    case VCR_BadFile:
        module = L"VCR";
        error = L"The provided file is inaccessible or does not exist.";
        break;
    case VCR_InvalidSavestate:
        module = L"VCR";
        error = L"The movie's savestate is missing or invalid.";
        break;
    case VCR_InvalidFrame:
        module = L"VCR";
        error = L"The resulting frame is outside the bounds of the movie.";
        break;
    case VCR_NoMatchingRom:
        module = L"VCR";
        error = L"There is no rom which matches this movie.";
        break;
    case VCR_Busy:
        module = L"VCR";
        error = L"The VCR engine is busy.";
        break;
    case VCR_Idle:
        module = L"VCR";
        error = L"The VCR engine is idle, but must be active to complete this operation.";
        break;
    case VCR_NotFromThisMovie:
        module = L"VCR";
        error = L"The provided freeze buffer is not from the currently active movie.";
        break;
    case VCR_InvalidVersion:
        module = L"VCR";
        error = L"The movie's version is invalid.";
        break;
    case VCR_InvalidExtendedVersion:
        module = L"VCR";
        error = L"The movie's extended version is invalid.";
        break;
    case VCR_NeedsPlaybackOrRecording:
        module = L"VCR";
        error = L"The operation requires a playback or recording task.";
        break;
    case VCR_InvalidStartType:
        module = L"VCR";
        error = L"The provided start type is invalid.";
        break;
    case VCR_WarpModifyAlreadyRunning:
        module = L"VCR";
        error = L"Another warp modify operation is already running.";
        break;
    case VCR_WarpModifyNeedsRecordingTask:
        module = L"VCR";
        error = L"Warp modifications can only be performed during recording.";
        break;
    case VCR_WarpModifyEmptyInputBuffer:
        module = L"VCR";
        error = L"The provided input buffer is empty.";
        break;
    case VCR_SeekAlreadyRunning:
        module = L"VCR";
        error = L"Another seek operation is already running.";
        break;
    case VCR_SeekSavestateLoadFailed:
        module = L"VCR";
        error = L"The seek operation could not be initiated due to a savestate not being loaded successfully.";
        break;
    case VCR_SeekSavestateIntervalZero:
        module = L"VCR";
        error = L"The seek operation can't be initiated because the seek savestate interval is 0.";
        break;
#pragma endregion
#pragma region VR
    case VR_NoMatchingRom:
        module = L"Core";
        error = L"The ROM couldn't be loaded.\r\nCouldn't find an appropriate ROM.";
        break;
    case VR_PluginError:
        module = L"Core";
        prompt_plugin_change();
        break;
    case VR_RomInvalid:
        module = L"Core";
        error = L"The ROM couldn't be loaded.\r\nVerify that the ROM is a valid N64 ROM.";
        break;
    case VR_FileOpenFailed:
        module = L"Core";
        error = L"Failed to open streams to core files.\r\nVerify that Mupen is allowed disk access.";
        break;
#pragma endregion
    default:
        module = L"Unknown";
        error = L"Unknown error.";
        return true;
    }

    if (!error.empty())
    {
        const auto title = std::format(L"{} Error {}", module, static_cast<int32_t>(result));
        FrontendService::show_dialog(error.c_str(), title.c_str(), fsvc_error);
    }

    return true;
}

void set_menu_accelerator(int element_id, const wchar_t* acc)
{
    wchar_t string[256] = {0};
    GetMenuString(GetMenu(g_main_hwnd), element_id, string, std::size(string), MF_BYCOMMAND);

    auto tab = wcschr(string, '\t');
    if (tab)
        *tab = '\0';
    if (StrCmp(acc, L""))
        wsprintf(string, L"%s\t%s", string, acc);

    ModifyMenu(GetMenu(g_main_hwnd), element_id, MF_BYCOMMAND | MF_STRING, element_id, string);
}

void set_hotkey_menu_accelerators(cfg_hotkey* hotkey, int menu_item_id)
{
    const auto hotkey_str = hotkey_to_string(hotkey);
    set_menu_accelerator(menu_item_id, hotkey_str == L"(nothing)" ? L"" : hotkey_str.c_str());
}

void SetDlgItemHotkeyAndMenu(HWND hwnd, int idc, cfg_hotkey* hotkey,
                             int menuItemID)
{
    const auto hotkey_str = hotkey_to_string(hotkey);
    SetDlgItemText(hwnd, idc, hotkey_str.c_str());

    set_menu_accelerator(menuItemID,
                         hotkey_str == L"(nothing)" ? L"" : hotkey_str.c_str());
}

const wchar_t* get_input_text()
{
    static wchar_t text[1024]{};
    memset(text, 0, sizeof(text));

    core_buttons b = LuaService::get_last_controller_data(0);
    wsprintf(text, L"(%d, %d) ", b.y, b.x);
    if (b.start)
        lstrcatW(text, L"S");
    if (b.z)
        lstrcatW(text, L"Z");
    if (b.a)
        lstrcatW(text, L"A");
    if (b.b)
        lstrcatW(text, L"B");
    if (b.l)
        lstrcatW(text, L"L");
    if (b.r)
        lstrcatW(text, L"R");
    if (b.cu || b.cd || b.cl ||
        b.cr)
    {
        lstrcatW(text, L" C");
        if (b.cu)
            lstrcatW(text, L"^");
        if (b.cd)
            lstrcatW(text, L"v");
        if (b.cl)
            lstrcatW(text, L"<");
        if (b.cr)
            lstrcatW(text, L">");
    }
    if (b.du || b.dd || b.dl || b.dr)
    {
        lstrcatW(text, L"D");
        if (b.du)
            lstrcatW(text, L"^");
        if (b.dd)
            lstrcatW(text, L"v");
        if (b.dl)
            lstrcatW(text, L"<");
        if (b.dr)
            lstrcatW(text, L">");
    }
    return text;
}

const wchar_t* get_status_text()
{
    static wchar_t text[1024]{};
    memset(text, 0, sizeof(text));

    std::pair<size_t, size_t> pair = {0, 0};
    core_vcr_get_seek_completion(pair);

    const auto index_adjustment = g_config.vcr_0_index ? 1 : 0;
    const auto current_sample = pair.first;
    const auto current_vi = core_vcr_get_current_vi();
    const auto is_before_start = static_cast<int64_t>(current_sample) - static_cast<int64_t>(index_adjustment) < 0;

    if (core_vcr_get_warp_modify_status())
    {
        StringCbPrintfW(text, sizeof(text), L"Warping (%.2f%%)", (double)current_sample / (double)core_vcr_get_length_samples() * 100.0);
        return text;
    }

    if (core_vcr_get_task() == task_recording)
    {
        if (is_before_start)
        {
            memset(text, 0, sizeof(text));
        }
        else
        {
            wsprintfW(text, L"%d (%d) ", current_vi, current_sample - index_adjustment);
        }
    }

    if (core_vcr_get_task() == task_playback)
    {
        if (is_before_start)
        {
            memset(text, 0, sizeof(text));
        }
        else
        {
            wsprintfW(text, L"%d / %d (%d / %d) ",
                      current_vi,
                      core_vcr_get_length_vis(),
                      current_sample - index_adjustment,
                      core_vcr_get_length_samples());
        }
    }

    return text;
}

/**
 * \brief Converts a config action to its respective menu ID
 * \param action The action to convert
 * \returns The converted ID, or 0 if no match is found.
 * \remark In case of some toggle actions, the IDs dont map to a menu item, but only to an identifier which is handled in WM_COMMAND
 */
int config_action_to_menu_id(cfg_action action)
{
    if (!ACTION_ID_MAP.contains(action))
    {
        g_view_logger->info("[View] No menu ID found for action {}", static_cast<int>(action));
        return 0;
    }
    return ACTION_ID_MAP.at(action);
}

std::filesystem::path get_screenshots_directory()
{
    if (g_config.is_default_screenshots_directory_used)
    {
        return g_app_path / L"screenshots\\";
    }
    return g_config.screenshots_directory;
}

std::filesystem::path get_plugins_directory()
{
    if (g_config.is_default_plugins_directory_used)
    {
        return g_app_path / L"plugin\\";
    }
    return g_config.plugins_directory;
}

std::filesystem::path get_backups_directory()
{
    if (g_config.is_default_backups_directory_used)
    {
        return "backups\\";
    }
    return g_config.backups_directory;
}

void update_titlebar()
{
    std::wstring text = get_mupen_name();

    if (g_emu_starting)
    {
        text += L" - Starting...";
    }

    if (core_vr_get_launched())
    {
        text += std::format(L" - {}", string_to_wstring(reinterpret_cast<char*>(core_vr_get_rom_header()->nom)));
    }

    if (core_vcr_get_task() != task_idle)
    {
        wchar_t movie_filename[MAX_PATH] = {0};
        _wsplitpath(core_vcr_get_path().wstring().c_str(), nullptr, nullptr, movie_filename, nullptr);
        text += std::format(L" - {}", movie_filename);
    }

    if (EncodingManager::is_capturing())
    {
        text += std::format(L" - {}", EncodingManager::get_current_path().filename().wstring());
    }

    SetWindowText(g_main_hwnd, text.c_str());
}

/**
 * \brief Computes the average rate of entries in the time queue per second (e.g.: FPS from frame deltas)
 * \param times A circular buffer of deltas
 * \return The average rate per second from the delta in the queue
 */
static double get_rate_per_second_from_deltas(const std::span<core_timer_delta>& times)
{
    size_t count = 0;
    double sum = 0.0;
    for (const auto& time : times)
    {
        if (time.count() > 0.0)
        {
            sum += time.count() / 1000000.0;
            count++;
        }
    }
    return count > 0 ? 1000.0 / (sum / count) : 0.0;
}

#pragma region Change notifications

void on_script_started(std::any data)
{
    g_main_window_dispatcher->invoke([=] {
        auto value = std::any_cast<std::filesystem::path>(data);
        RecentMenu::add(g_config.recent_lua_script_paths, value.wstring(), g_config.is_recent_scripts_frozen, ID_LUA_RECENT, g_recent_lua_menu);
    });
}

void on_task_changed(std::any data)
{
    g_main_window_dispatcher->invoke([=] {
        auto value = std::any_cast<core_vcr_task>(data);
        static auto previous_value = value;
        if (!vcr_is_task_recording(value) && vcr_is_task_recording(previous_value))
        {
            Statusbar::post(L"Recording stopped");
        }
        if (!task_is_playback(value) && task_is_playback(previous_value))
        {
            Statusbar::post(L"Playback stopped");
        }

        if ((vcr_is_task_recording(value) && !vcr_is_task_recording(previous_value)) || task_is_playback(value) && !task_is_playback(previous_value) && !core_vcr_get_path().empty())
        {
            RecentMenu::add(g_config.recent_movie_paths, core_vcr_get_path().wstring(), g_config.is_recent_movie_paths_frozen, ID_RECENTMOVIES_FIRST, g_recent_movies_menu);
        }

        update_titlebar();
        SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
        previous_value = value;
    });
}

void on_emu_stopping(std::any)
{
    // Remember all running lua scripts' HWNDs
    for (const auto [key, _] : g_hwnd_lua_map)
    {
        g_previously_running_luas.push_back(key);
    }
    g_main_window_dispatcher->invoke(stop_all_scripts);
}

void on_emu_launched_changed(std::any data)
{
    g_main_window_dispatcher->invoke([=] {
        auto value = std::any_cast<bool>(data);
        static auto previous_value = value;

        const auto window_style = GetWindowLong(g_main_hwnd, GWL_STYLE);
        if (value)
        {
            SetWindowLong(g_main_hwnd, GWL_STYLE, window_style & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
        }
        else
        {
            SetWindowLong(g_main_hwnd, GWL_STYLE, window_style | WS_THICKFRAME | WS_MAXIMIZEBOX);
        }

        update_titlebar();
        // Some menu items, like movie ones, depend on both this and vcr task
        on_task_changed(core_vcr_get_task());

        // Reset and restore view stuff when emulation starts
        if (value)
        {
            g_vis_since_input_poll_warning_dismissed = false;

            RecentMenu::add(g_config.recent_rom_paths, g_rom_path.wstring(), g_config.is_recent_rom_paths_frozen, ID_RECENTROMS_FIRST, g_recent_roms_menu);

            for (const HWND hwnd : g_previously_running_luas)
            {
                // click start button
                SendMessage(hwnd, WM_COMMAND,
                            MAKEWPARAM(IDC_BUTTON_LUASTATE, BN_CLICKED),
                            (LPARAM)GetDlgItem(hwnd, IDC_BUTTON_LUASTATE));
            }

            g_previously_running_luas.clear();
        }

        if (!value && previous_value)
        {
            g_view_logger->info("[View] Restoring window size to {}x{}...", g_config.window_width, g_config.window_height);
            SetWindowPos(g_main_hwnd, nullptr, 0, 0, g_config.window_width, g_config.window_height, SWP_NOMOVE);
        }

        SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
        previous_value = value;
    });
}

void on_capturing_changed(std::any data)
{
    g_main_window_dispatcher->invoke([=] {
        auto value = std::any_cast<bool>(data);

        if (value)
        {
            SetWindowLong(g_main_hwnd, GWL_STYLE, GetWindowLong(g_main_hwnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
            // NOTE: WS_EX_LAYERED fixes BitBlt'ing from the window when its off-screen, as it wouldnt redraw otherwise (relevant for Window capture mode)
            SetWindowLong(g_main_hwnd, GWL_EXSTYLE, GetWindowLong(g_main_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        }
        else
        {
            SetWindowPos(g_main_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetWindowLong(g_main_hwnd, GWL_STYLE, GetWindowLong(g_main_hwnd, GWL_STYLE) | WS_MINIMIZEBOX);
            SetWindowLong(g_main_hwnd, GWL_EXSTYLE, GetWindowLong(g_main_hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        }

        update_titlebar();
        SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
    });
}

void on_speed_modifier_changed(std::any data)
{
    auto value = std::any_cast<int32_t>(data);
    Statusbar::post(std::format(L"Speed limit: {}%", value));
}

void on_emu_paused_changed(std::any data)
{
    g_core.callbacks.frame();
    SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
}

void on_vis_since_input_poll_exceeded(std::any)
{
    if (g_vis_since_input_poll_warning_dismissed)
    {
        return;
    }

    if (g_config.silent_mode || FrontendService::show_ask_dialog(L"An unusual execution pattern was detected. Continuing might leave the emulator in an unusable state.\r\nWould you like to terminate emulation?", L"Warning", true))
    {
        // TODO: Send IDM_CLOSE_ROM instead... probably better :P
        AsyncExecutor::invoke_async([] {
            const auto result = core_vr_close_rom(true, false);
            show_error_dialog_for_result(result);
        });
    }
    g_vis_since_input_poll_warning_dismissed = true;
}

void on_movie_loop_changed(std::any data)
{
    auto value = std::any_cast<bool>(data);
    Statusbar::post(value ? L"Movies restart after ending" : L"Movies stop after ending");
    SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
}

void on_fullscreen_changed(std::any data)
{
    g_main_window_dispatcher->invoke([=] {
        auto value = std::any_cast<bool>(data);
        ShowCursor(!value);
        SendMessage(g_main_hwnd, WM_INITMENU, 0, 0);
    });
}

void apply_menu_item_accelerator_text()
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
}

void on_config_loaded(std::any)
{
    apply_menu_item_accelerator_text();
    RomBrowser::build();
}

void on_seek_completed(std::any)
{
    LuaService::call_seek_completed();
}


void on_warp_modify_status_changed(std::any data)
{
    auto value = std::any_cast<bool>(data);
    LuaService::call_warp_modify_status_changed(value);
}

void update_core_fast_forward(std::any)
{
    core_vr_set_fast_forward(g_fast_forward || core_vcr_is_seeking() || Cli::wants_fast_forward() || Compare::active());
}

BetterEmulationLock::BetterEmulationLock()
{
    if (g_in_menu_loop)
    {
        was_paused = g_paused_before_menu;

        // This fires before WM_EXITMENULOOP (which restores the paused_before_menu state), so we need to trick it...
        g_paused_before_menu = true;
    }
    else
    {
        was_paused = core_vr_get_paused();
        core_vr_pause_emu();
    }
}

BetterEmulationLock::~BetterEmulationLock()
{
    if (was_paused)
    {
        core_vr_pause_emu();
    }
    else
    {
        core_vr_resume_emu();
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

    // subtract size of toolbar and statusbar from buffer dimensions
    // if video plugin knows about this, whole game screen should be captured. Most of the plugins do.
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

    std::wstring final_message;
    if (core_vcr_get_task() == task_recording)
    {
        final_message.append(L"Movie recording ");
        warnings++;
    }
    if (EncodingManager::is_capturing())
    {
        if (warnings > 0)
        {
            final_message.append(L",");
        }
        final_message.append(L" AVI capture ");
        warnings++;
    }
    if (core_vr_is_tracelog_active())
    {
        if (warnings > 0)
        {
            final_message.append(L",");
        }
        final_message.append(L" Trace logging ");
        warnings++;
    }
    final_message.append(L"is running. Are you sure you want to stop emulation?");
    if (warnings > 0)
        res = MessageBox(g_main_hwnd, final_message.c_str(), L"Stop emulation?",
                         MB_YESNO | MB_ICONWARNING);

    return res == IDYES || warnings == 0;
}

bool is_on_gui_thread()
{
    return GetCurrentThreadId() == g_ui_thread_id;
}

std::filesystem::path get_app_full_path()
{
    wchar_t ret[MAX_PATH] = {0};
    wchar_t drive[_MAX_DRIVE], dirn[_MAX_DIR];
    wchar_t path_buffer[_MAX_DIR];
    GetModuleFileName(nullptr, path_buffer, std::size(path_buffer));
    _wsplitpath(path_buffer, drive, dirn, nullptr, nullptr);
    StrCpy(ret, drive);
    StrCat(ret, dirn);

    return ret;
}

t_plugin_discovery_result do_plugin_discovery()
{
    if (g_config.plugin_discovery_delayed)
    {
        Sleep(1000);
    }

    std::vector<std::unique_ptr<Plugin>> plugins;
    const auto files = IOService::get_files_with_extension_in_directory(get_plugins_directory(), L"dll");

    std::vector<std::pair<std::filesystem::path, std::wstring>> results;
    for (const auto& file : files)
    {
        auto [result, plugin] = Plugin::create(file);

        results.emplace_back(file, result);

        if (!result.empty())
            continue;

        plugins.emplace_back(std::move(plugin));
    }

    return t_plugin_discovery_result{
    .plugins = std::move(plugins),
    .results = results,
    };
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
    if (Message == WM_EXECUTE_DISPATCHER)
    {
        g_main_window_dispatcher->execute();
        return TRUE;
    }

    wchar_t path_buffer[_MAX_PATH]{};

    switch (Message)
    {
    case WM_DROPFILES:
        {
            auto drop = (HDROP)wParam;
            wchar_t fname[MAX_PATH] = {0};
            DragQueryFile(drop, 0, fname, std::size(fname));

            std::filesystem::path path = fname;
            std::string extension = to_lower(path.extension().string());

            if (extension == ".n64" || extension == ".z64" || extension == ".v64" || extension == ".rom")
            {
                AsyncExecutor::invoke_async([path] {
                    const auto result = core_vr_start_rom(path, false);
                    if (result == Res_Ok)
                    {
                        g_rom_path = path;
                    }
                    show_error_dialog_for_result(result);
                });
            }
            else if (extension == ".m64")
            {
                g_config.core.vcr_readonly = true;
                Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.core.vcr_readonly);
                AsyncExecutor::invoke_async([fname] {
                    auto result = core_vcr_start_playback(fname);
                    show_error_dialog_for_result(result);
                });
            }
            else if (extension == ".st" || extension == ".savestate" || extension == ".st0" || extension == ".st1" || extension == ".st2" || extension == ".st3" || extension == ".st4" || extension == ".st5" || extension == ".st6" || extension == ".st7" || extension == ".st8" || extension == ".st9")
            {
                if (!core_vr_get_launched())
                    break;
                core_vr_wait_increment();
                AsyncExecutor::invoke_async([=] {
                    core_vr_wait_decrement();
                    core_st_do_file(fname, core_st_job_load, nullptr, false);
                });
            }
            else if (extension == ".lua")
            {
                lua_create_and_run(path.wstring().c_str());
            }
            break;
        }
    case WM_SETCURSOR:
        {
            static bool last_lmb = false;
            static bool last_rmb = false;
            static bool last_mmb = false;
            static bool last_xmb1 = false;
            static bool last_xmb2 = false;
            bool lmb = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
            bool rmb = GetAsyncKeyState(VK_RBUTTON) & 0x8000;
            bool mmb = GetAsyncKeyState(VK_MBUTTON) & 0x8000;
            bool xmb1 = GetAsyncKeyState(VK_XBUTTON1) & 0x8000;
            bool xmb2 = GetAsyncKeyState(VK_XBUTTON2) & 0x8000;
        
            BOOL hit = FALSE;
            for (cfg_hotkey* hotkey : g_config_hotkeys)
            {
                const auto down =
                (lmb && !last_lmb && hotkey->key == VK_LBUTTON) || (rmb && !last_rmb && hotkey->key == VK_RBUTTON) || (mmb && !last_mmb && hotkey->key == VK_MBUTTON) || (xmb1 && !last_xmb1 && hotkey->key == VK_XBUTTON1) || (xmb2 && !last_xmb2 && hotkey->key == VK_XBUTTON2);
                const auto up =
                (!lmb && last_lmb && hotkey->key == VK_LBUTTON) || (!rmb && last_rmb && hotkey->key == VK_RBUTTON) || (!mmb && last_mmb && hotkey->key == VK_MBUTTON) || (!xmb1 && last_xmb1 && hotkey->key == VK_XBUTTON1) || (!xmb2 && last_xmb2 && hotkey->key == VK_XBUTTON2);

                if (down)
                {
                    auto down_cmd = config_action_to_menu_id(hotkey->down_cmd);
                    // We only want to send it if the corresponding menu item exists and is enabled
                    auto state = GetMenuState(g_main_menu, down_cmd, MF_BYCOMMAND);
                    if (state != -1 && (state & MF_DISABLED || state & MF_GRAYED))
                    {
                        g_view_logger->info(L"Dismissed {} ({})", hotkey->identifier.c_str(), down_cmd);
                        continue;
                    }
                    g_view_logger->info(L"Sent down {} ({})", hotkey->identifier.c_str(), down_cmd);
                    SendMessage(g_main_hwnd, WM_COMMAND, down_cmd, 0);
                    hit = TRUE;
                }
                if (up)
                {
                    auto up_cmd = config_action_to_menu_id(hotkey->up_cmd);
                    // We only want to send it if the corresponding menu item exists and is enabled
                    auto state = GetMenuState(g_main_menu, up_cmd, MF_BYCOMMAND);
                    if (state != -1 && (state & MF_DISABLED || state & MF_GRAYED))
                    {
                        g_view_logger->info(L"Dismissed {} ({})", hotkey->identifier.c_str(), up_cmd);
                        continue;
                    }
                    g_view_logger->info(L"Sent up {} ({})", hotkey->identifier.c_str(), up_cmd);
                    SendMessage(g_main_hwnd, WM_COMMAND, up_cmd, 0);
                    hit = TRUE;
                }
            }

            last_lmb = lmb;
            last_rmb = rmb;
            last_mmb = mmb;
            last_xmb1 = xmb1;
            last_xmb2 = xmb2;
        
            if (!hit)
                return DefWindowProc(hwnd, Message, wParam, lParam);
            break;
        }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        {
            BOOL hit = FALSE;
            for (cfg_hotkey* hotkey : g_config_hotkeys)
            {
                if ((int)wParam == hotkey->key)
                {
                    if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) == hotkey->shift && ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) == hotkey->ctrl && ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) == hotkey->alt)
                    {
                        auto down_cmd = config_action_to_menu_id(hotkey->down_cmd);
                        // We only want to send it if the corresponding menu item exists and is enabled
                        auto state = GetMenuState(g_main_menu, down_cmd, MF_BYCOMMAND);
                        if (state != -1 && (state & MF_DISABLED || state & MF_GRAYED))
                        {
                            g_view_logger->info(L"Dismissed {} ({})", hotkey->identifier.c_str(), down_cmd);
                            continue;
                        }
                        g_view_logger->info(L"Sent down {} ({})", hotkey->identifier.c_str(), down_cmd);
                        SendMessage(g_main_hwnd, WM_COMMAND, down_cmd, 0);
                        hit = TRUE;
                    }
                }
            }

            if (g_core.plugin_funcs.key_down && core_vr_get_launched())
                g_core.plugin_funcs.key_down(wParam, lParam);
            if (!hit)
                return DefWindowProc(hwnd, Message, wParam, lParam);
            break;
        }
    case WM_SYSKEYUP:
    case WM_KEYUP:
        {
            BOOL hit = FALSE;
            for (cfg_hotkey* hotkey : g_config_hotkeys)
            {
                if (hotkey->up_cmd == ACTION_NONE)
                {
                    continue;
                }

                if ((int)wParam == hotkey->key)
                {
                    if (((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0) == hotkey->shift && ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) == hotkey->ctrl && ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) == hotkey->alt)
                    {
                        auto up_cmd = config_action_to_menu_id(hotkey->up_cmd);
                        // We only want to send it if the corresponding menu item exists and is enabled
                        auto state = GetMenuState(g_main_menu, up_cmd, MF_BYCOMMAND);
                        if (state != -1 && (state & MF_DISABLED || state & MF_GRAYED))
                        {
                            g_view_logger->info(L"Dismissed {} ({})", hotkey->identifier.c_str(), up_cmd);
                            continue;
                        }
                        g_view_logger->info(L"Sent up {} ({})", hotkey->identifier.c_str(), up_cmd);
                        SendMessage(g_main_hwnd, WM_COMMAND, up_cmd, 0);
                        hit = TRUE;
                    }
                }
            }

            if (g_core.plugin_funcs.key_up && core_vr_get_launched())
                g_core.plugin_funcs.key_up(wParam, lParam);
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
                RomBrowser::notify(lParam);
            }
            return 0;
        }
    case WM_MOVE:
        {
            if (core_vr_get_launched())
            {
                g_core.plugin_funcs.move_screen((int)wParam, lParam);
            }

            if (IsIconic(g_main_hwnd))
            {
                // GetWindowRect values are nonsense when minimized
                break;
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

            if (core_vr_get_launched())
            {
                // We don't need to remember the dimensions set by gfx plugin
                break;
            }

            if (IsIconic(g_main_hwnd))
            {
                // GetWindowRect values are nonsense when minimized
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
    case WM_CREATE:
        g_main_window_dispatcher = std::make_unique<Dispatcher>(g_ui_thread_id, [] {
            SendMessage(g_main_hwnd, WM_EXECUTE_DISPATCHER, 0, 0);
        });
        g_main_menu = GetMenu(hwnd);
        GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
        MGECompositor::create(hwnd);
        PianoRoll::init();
        configdialog_init();
        return TRUE;
    case WM_DESTROY:
        save_config();
        timeKillEvent(g_ui_timer);
        AsyncExecutor::stop();
        Gdiplus::GdiplusShutdown(gdi_plus_token);
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        if (confirm_user_exit())
        {
            close_all_scripts();
            std::thread([] {
                core_vr_close_rom(true, false);
                g_main_window_dispatcher->invoke([] {
                    DestroyWindow(g_main_hwnd);
                });
            })
            .detach();
            break;
        }
        return 0;
    case WM_INVALIDATE_LUA:
        for (const auto& map : g_hwnd_lua_map)
        {
            map.second->invalidate_visuals();
        }
        break;
    case WM_WINDOWPOSCHANGING: // allow gfx plugin to set arbitrary size
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
            const auto core_executing = core_vr_get_launched();
            const auto vcr_active = core_vcr_get_task() != task_idle;

            ModifyMenu(g_main_menu, IDM_MULTI_FRAME_ADVANCE, MF_BYCOMMAND | MF_STRING, IDM_MULTI_FRAME_ADVANCE, std::format(L"Multi-Frame Advance {}x", g_config.multi_frame_advance_count).c_str());
            apply_menu_item_accelerator_text();
         
            EnableMenuItem(g_main_menu, IDM_CLOSE_ROM, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_RESET_ROM, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_PAUSE, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_SPEED_DOWN, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_SPEED_UP, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_SPEED_RESET, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_FRAMEADVANCE, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_MULTI_FRAME_ADVANCE, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_SCREENSHOT, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_SAVE_SLOT, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_LOAD_SLOT, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_SAVE_STATE_AS, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_LOAD_STATE_AS, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_UNDO_LOAD_STATE, (core_executing && g_config.core.st_undo_load) ? MF_ENABLED : MF_GRAYED);
            for (int i = IDM_SELECT_1; i < IDM_SELECT_10; ++i)
            {
                EnableMenuItem(g_main_menu, i, core_executing ? MF_ENABLED : MF_GRAYED);
            }
            EnableMenuItem(g_main_menu, IDM_FULLSCREEN, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_STATUSBAR, !core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_START_MOVIE_RECORDING, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_STOP_MOVIE, vcr_active ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_CREATE_MOVIE_BACKUP, vcr_active ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_TRACELOG, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_COREDBG, (core_executing && g_config.core.core_type == 2) ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_SEEKER, (core_executing && vcr_active) ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_PIANO_ROLL, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_CHEATS, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_START_CAPTURE, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_START_CAPTURE_PRESET, core_executing ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(g_main_menu, IDM_STOP_CAPTURE, (core_executing && EncodingManager::is_capturing()) ? MF_ENABLED : MF_GRAYED);

            CheckMenuItem(g_main_menu, IDM_STATUSBAR, g_config.is_statusbar_enabled ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(g_main_menu, IDM_FREEZE_RECENT_ROMS, g_config.is_recent_rom_paths_frozen ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(g_main_menu, IDM_FREEZE_RECENT_MOVIES, g_config.is_recent_movie_paths_frozen ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(g_main_menu, IDM_FREEZE_RECENT_LUA, g_config.is_recent_scripts_frozen ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(g_main_menu, IDM_LOOP_MOVIE, g_config.core.is_movie_loop_enabled ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(g_main_menu, IDM_VCR_READONLY, g_config.core.vcr_readonly ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(g_main_menu, IDM_FULLSCREEN, core_vr_is_fullscreen() ? MF_CHECKED : MF_UNCHECKED);

            for (int i = IDM_SELECT_1; i < IDM_SELECT_10; ++i)
            {
                CheckMenuItem(g_main_menu, i, MF_UNCHECKED);
            }
            CheckMenuItem(g_main_menu, IDM_SELECT_1 + g_config.st_slot, MF_CHECKED);

            RecentMenu::build(g_config.recent_rom_paths, ID_RECENTROMS_FIRST, g_recent_roms_menu);
            RecentMenu::build(g_config.recent_movie_paths, ID_RECENTMOVIES_FIRST, g_recent_movies_menu);
            RecentMenu::build(g_config.recent_lua_script_paths, ID_LUA_RECENT, g_recent_lua_menu);
        }
        break;
    case WM_ENTERMENULOOP:
        g_in_menu_loop = true;
        g_paused_before_menu = core_vr_get_paused();
        core_vr_pause_emu();
        break;

    case WM_EXITMENULOOP:
        // This message is sent when we escape the blocking menu loop, including situations where the clicked menu spawns a dialog.
        // In those situations, we would unpause the game here (since this message is sent first), and then pause it again in the menu item message handler.
        // It's almost guaranteed that a game frame will pass between those messages, so we need to wait a bit on another thread before unpausing.
        std::thread([] {
            Sleep(60);
            g_in_menu_loop = false;
            if (g_paused_before_menu)
            {
                core_vr_pause_emu();
            }
            else
            {
                core_vr_resume_emu();
            }
        })
        .detach();
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
                core_vr_resume_emu();
            }
            break;

        case WA_INACTIVE:
            g_paused_before_focus = core_vr_get_paused();
            core_vr_pause_emu();
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
                    std::unique_ptr<Plugin> plugin;
                    switch (LOWORD(wParam))
                    {
                    case IDM_VIDEO_SETTINGS:
                        plugin = Plugin::create(g_config.selected_video_plugin).second;
                        break;
                    case IDM_INPUT_SETTINGS:
                        plugin = Plugin::create(g_config.selected_input_plugin).second;
                        break;
                    case IDM_AUDIO_SETTINGS:
                        plugin = Plugin::create(g_config.selected_audio_plugin).second;
                        break;
                    case IDM_RSP_SETTINGS:
                        plugin = Plugin::create(g_config.selected_rsp_plugin).second;
                        break;
                    }
                    if (plugin != nullptr)
                    {
                        plugin->config();
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
            case IDM_DEBUG_WARP_MODIFY:
                {
                    auto inputs = core_vcr_get_inputs();
                    inputs[inputs.size() - 10].a = 1;

                    auto result = core_vcr_begin_warp_modify(inputs);
                    show_error_dialog_for_result(result);

                    break;
                }
            case IDM_BENCHMARK_MESSENGER:
                {
                    ScopeTimer timer("Messenger", g_view_logger.get());
                    for (int i = 0; i < 10'000'000; ++i)
                    {
                        Messenger::broadcast(Messenger::Message::None, 5);
                    }
                }
                break;
            case IDM_BENCHMARK_LUA_CALLBACK:
                {
                    FrontendService::show_dialog(L"Make sure the Lua script is running and the registered atreset body is empty.", L"Benchmark Lua Callback", fsvc_information);
                    ScopeTimer timer("100,000,000x call_reset", g_view_logger.get());
                    for (int i = 0; i < 100'000'000; ++i)
                    {
                        LuaService::call_reset();
                    }
                    FrontendService::show_dialog(std::format(L"100,000,000 atreset callback invocations took {}ms", timer.momentary_ms()).c_str(), L"Benchmark Lua Callback", fsvc_information);
                }
                break;
            case IDM_TRACELOG:
                {
                    if (core_vr_is_tracelog_active())
                    {
                        core_tl_stop();
                        ModifyMenu(g_main_menu, IDM_TRACELOG, MF_BYCOMMAND | MF_STRING, IDM_TRACELOG, L"Start &Trace Logger...");
                        break;
                    }

                    auto path = show_persistent_save_dialog(L"s_tracelog", g_main_hwnd, L"*.log");

                    if (path.empty())
                    {
                        break;
                    }

                    auto result = MessageBox(g_main_hwnd, L"Should the trace log be generated in a binary format?", L"Trace Logger",
                                             MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);

                    core_tl_start(path, result == IDYES, false);
                    ModifyMenu(g_main_menu, IDM_TRACELOG, MF_BYCOMMAND | MF_STRING, IDM_TRACELOG, L"Stop &Trace Logger");
                }
                break;
            case IDM_CLOSE_ROM:
                if (!confirm_user_exit())
                    break;
                AsyncExecutor::invoke_async([] {
                    const auto result = core_vr_close_rom(true, false);
                    show_error_dialog_for_result(result);
                },
                                            ASYNC_KEY_CLOSE_ROM);
                break;
            case IDM_FASTFORWARD_ON:
                g_fast_forward = true;
                Messenger::broadcast(Messenger::Message::FastForwardNeedsUpdate, nullptr);
                break;
            case IDM_FASTFORWARD_OFF:
                g_fast_forward = false;
                Messenger::broadcast(Messenger::Message::FastForwardNeedsUpdate, nullptr);
                break;
            case IDM_GS_ON:
                core_vr_set_gs_button(true);
                break;
            case IDM_GS_OFF:
                core_vr_set_gs_button(false);
                break;
            case IDM_PAUSE:
                {
                    // FIXME: While this is a beautiful and clean solution, there has to be a better way to handle this
                    // We're too close to release to care tho
                    if (g_in_menu_loop)
                    {
                        if (g_paused_before_menu)
                        {
                            core_vr_resume_emu();
                            g_paused_before_menu = false;
                            break;
                        }
                        g_paused_before_menu = true;
                        core_vr_pause_emu();
                    }
                    else
                    {
                        if (core_vr_get_paused())
                        {
                            core_vr_resume_emu();
                            break;
                        }
                        core_vr_pause_emu();
                    }

                    break;
                }

            case IDM_FRAMEADVANCE:
                core_vr_frame_advance(1);
                core_vr_resume_emu();
                break;
            case IDM_MULTI_FRAME_ADVANCE:
                core_vr_frame_advance(g_config.multi_frame_advance_count);
                core_vr_resume_emu();
                break;
            case IDM_MULTI_FRAME_ADVANCE_INC:
                g_config.multi_frame_advance_count += 1;
                Messenger::broadcast(Messenger::Message::MultiFrameAdvanceCountChanged, std::nullopt);
                break;
            case IDM_MULTI_FRAME_ADVANCE_DEC:
                g_config.multi_frame_advance_count = std::max(1, g_config.multi_frame_advance_count - 1);
                Messenger::broadcast(Messenger::Message::MultiFrameAdvanceCountChanged, std::nullopt);
                break;
            case IDM_MULTI_FRAME_ADVANCE_RESET:
                g_config.multi_frame_advance_count = g_default_config.multi_frame_advance_count;
                Messenger::broadcast(Messenger::Message::MultiFrameAdvanceCountChanged, std::nullopt);
                break;
            case IDM_VCR_READONLY:
                g_config.core.vcr_readonly ^= true;
                Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.core.vcr_readonly);
                break;

            case IDM_LOOP_MOVIE:
                g_config.core.is_movie_loop_enabled ^= true;
                Messenger::broadcast(Messenger::Message::MovieLoopChanged, (bool)g_config.core.is_movie_loop_enabled);
                break;


            case EMU_PLAY:
                core_vr_resume_emu();
                break;

            case IDM_RESET_ROM:
            {
                const bool reset_will_continue_recording = g_config.core.is_reset_recording_enabled && core_vcr_get_task() == task_recording;

                if (!reset_will_continue_recording && !confirm_user_exit())
                    break;

                AsyncExecutor::invoke_async([] {
                    const auto result = core_vr_reset_rom(false, true, false);
                    show_error_dialog_for_result(result);
                },
                                            ASYNC_KEY_RESET_ROM);
                break;
            }
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
            case IDM_CHECK_FOR_UPDATES:
                AsyncExecutor::invoke_async([=] {
                    UpdateChecker::check(lParam != 1);
                });
                break;
            case IDM_COREDBG:
                CoreDbg::show();
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
            case IDM_PIANO_ROLL:
                PianoRoll::show();
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

                    wchar_t ram_start[20] = {0};
                    wsprintfW(ram_start, L"0x%p", static_cast<void*>(g_core.rdram));

                    wchar_t proc_name[MAX_PATH] = {0};
                    GetModuleFileName(NULL, proc_name, MAX_PATH);
                    _wsplitpath(proc_name, 0, 0, proc_name, 0);

                    wchar_t stroop_c[1024] = {0};
                    wsprintfW(stroop_c,
                              L"<Emulator name=\"Mupen 5.0 RR\" processName=\"%s\" ramStart=\"%s\" endianness=\"little\" autoDetect=\"true\"/>",
                              proc_name, ram_start);

                    const auto str = std::format(L"The RAM start is {}.\r\nHow would you like to proceed?", ram_start);

                    const auto result = FrontendService::show_multiple_choice_dialog({L"Copy STROOP config line",
                                                                                      L"Close"},
                                                                                     str.c_str(), L"Show RAM Start", fsvc_information);

                    if (result == 0)
                    {
                        copy_to_clipboard(g_main_hwnd, stroop_c);
                    }

                    break;
                }
            case IDM_STATS:
                {
                    BetterEmulationLock lock;

                    auto str = std::format(L"Total playtime: {}\r\nTotal rerecords: {}", format_duration(g_config.core.total_frames / 30),
                                           g_config.core.total_rerecords);

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

                    const auto path = show_persistent_open_dialog(L"o_rom", g_main_hwnd, L"*.n64;*.z64;*.v64;*.rom;*.bin;*.zip;*.usa;*.eur;*.jap");

                    if (!path.empty())
                    {
                        AsyncExecutor::invoke_async([path] {
                            const auto result = core_vr_start_rom(path, false);
                            if (result == Res_Ok)
                            {
                                g_rom_path = path;
                            }
                            show_error_dialog_for_result(result);
                        });
                    }
                }
                break;
            case IDM_EXIT:
                DestroyWindow(g_main_hwnd);
                break;
            case IDM_FULLSCREEN:
                core_vr_toggle_fullscreen_mode();
                break;
            case IDM_REFRESH_ROMBROWSER:
                if (!core_vr_get_launched())
                {
                    RomBrowser::build();
                }
                break;
            case IDM_SAVE_SLOT:
                core_vr_wait_increment();
                if (g_config.increment_slot)
                {
                    g_config.st_slot >= 9 ? g_config.st_slot = 0 : g_config.st_slot++;
                    Messenger::broadcast(Messenger::Message::SlotChanged, (size_t)g_config.st_slot);
                }
                AsyncExecutor::invoke_async([=] {
                    core_vr_wait_decrement();
                    core_st_do_slot(g_config.st_slot, core_st_job_save, nullptr, false);
                });
                break;
            case IDM_SAVE_STATE_AS:
                {
                    BetterEmulationLock lock;

                    auto path = show_persistent_save_dialog(L"s_savestate", hwnd, L"*.st;*.savestate");
                    if (path.empty())
                    {
                        break;
                    }

                    core_vr_wait_increment();
                    AsyncExecutor::invoke_async([=] {
                        core_vr_wait_decrement();
                        core_st_do_file(path, core_st_job_save, nullptr, false);
                    });
                }
                break;
            case IDM_LOAD_SLOT:
                core_vr_wait_increment();
                AsyncExecutor::invoke_async([=] {
                    core_vr_wait_decrement();
                    core_st_do_slot(g_config.st_slot, core_st_job_load, nullptr, false);
                });
                break;
            case IDM_LOAD_STATE_AS:
                {
                    BetterEmulationLock lock;

                    auto path = show_persistent_open_dialog(L"o_state", hwnd,
                                                            L"*.st;*.savestate;*.st0;*.st1;*.st2;*.st3;*.st4;*.st5;*.st6;*.st7;*.st8;*.st9,*.st10");
                    if (path.empty())
                    {
                        break;
                    }

                    core_vr_wait_increment();
                    AsyncExecutor::invoke_async([=] {
                        core_vr_wait_decrement();
                        core_st_do_file(path, core_st_job_load, nullptr, false);
                    });
                }
                break;
            case IDM_UNDO_LOAD_STATE:
                {
                    core_vr_wait_increment();
                    AsyncExecutor::invoke_async([=] {
                        core_vr_wait_decrement();

                        std::vector<uint8_t> buf{};
                        core_st_get_undo_savestate(buf);

                        if (buf.empty())
                        {
                            Statusbar::post(L"No load to undo");
                            return;
                        }

                        core_st_do_memory(buf, core_st_job_load, [](const core_result result, auto) {
							if (result == Res_Ok)
							{
								Statusbar::post(L"Undid load");
								return;
							}

							if (result == ST_Cancelled)
							{
								return;
							}

							Statusbar::post(L"Failed to undo load"); }, false);
                    });
                }
                break;
            case IDM_START_MOVIE_RECORDING:
                {
                    BetterEmulationLock lock;

                    auto movie_dialog_result = MovieDialog::show(false);

                    if (movie_dialog_result.path.empty())
                    {
                        break;
                    }

                    auto vcr_result = core_vcr_start_record(movie_dialog_result.path,
                                                            movie_dialog_result.start_flag,
                                                            wstring_to_string(movie_dialog_result.author),
                                                            wstring_to_string(movie_dialog_result.description));
                    if (show_error_dialog_for_result(vcr_result))
                    {
                        break;
                    }

                    g_config.last_movie_author = movie_dialog_result.author;

                    Statusbar::post(L"Recording replay");
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

                    core_vcr_replace_author_info(result.path, wstring_to_string(result.author), wstring_to_string(result.description));

                    g_config.core.pause_at_frame = result.pause_at;
                    g_config.core.pause_at_last_frame = result.pause_at_last;

                    AsyncExecutor::invoke_async([result] {
                        auto vcr_result = core_vcr_start_playback(result.path);
                        show_error_dialog_for_result(vcr_result);
                    });
                }
                break;
            case IDM_STOP_MOVIE:
                core_vcr_stop_all();
                break;
            case IDM_CREATE_MOVIE_BACKUP:
                {
                    const auto result = core_vcr_write_backup();
                    show_error_dialog_for_result(result);
                    break;
                }
            case IDM_START_CAPTURE_PRESET:
            case IDM_START_CAPTURE:
                {
                    if (!core_vr_get_launched())
                    {
                        break;
                    }

                    BetterEmulationLock lock;

                    auto path = show_persistent_save_dialog(L"s_capture", hwnd, L"*.avi");
                    if (path.empty())
                    {
                        break;
                    }

                    bool ask_preset = LOWORD(wParam) == IDM_START_CAPTURE;

                    EncodingManager::start_capture(path, (cfg_encoder_type)g_config.encoder_type, ask_preset, [](const auto result) {
                        if (result)
                        {
                            Statusbar::post(L"Capture started...");
                        }
                    });
                
                    break;
                }
            case IDM_STOP_CAPTURE:
                EncodingManager::stop_capture([](const auto result) {
                    if (result)
                    {
                        Statusbar::post(L"Capture stopped");
                    }
                });
                break;
            case IDM_SCREENSHOT:
                g_core.plugin_funcs.capture_screen(get_screenshots_directory().string().data());
                break;
            case IDM_RESET_RECENT_ROMS:
                g_config.recent_rom_paths.clear();
                break;
            case IDM_RESET_RECENT_MOVIES:
                g_config.recent_movie_paths.clear();
                break;
            case IDM_RESET_RECENT_LUA:
                g_config.recent_lua_script_paths.clear();
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
            case IDM_SPEED_DOWN:
                g_config.core.fps_modifier = clamp(g_config.core.fps_modifier - 25, 25, 1000);
                core_vr_on_speed_modifier_changed();
                Messenger::broadcast(Messenger::Message::SpeedModifierChanged, g_config.core.fps_modifier);
                break;
            case IDM_SPEED_UP:
                g_config.core.fps_modifier = clamp(g_config.core.fps_modifier + 25, 25, 1000);
                core_vr_on_speed_modifier_changed();
                Messenger::broadcast(Messenger::Message::SpeedModifierChanged, g_config.core.fps_modifier);
                break;
            case IDM_SPEED_RESET:
                g_config.core.fps_modifier = 100;
                core_vr_on_speed_modifier_changed();
                Messenger::broadcast(Messenger::Message::SpeedModifierChanged, g_config.core.fps_modifier);
                break;
            default:
                if (LOWORD(wParam) >= IDM_SELECT_1 && LOWORD(wParam) <= IDM_SELECT_10)
                {
                    auto slot = LOWORD(wParam) - IDM_SELECT_1;
                    g_config.st_slot = slot;
                    Messenger::broadcast(Messenger::Message::SlotChanged, static_cast<size_t>(g_config.st_slot));
                }
                else if (LOWORD(wParam) >= ID_SAVE_1 && LOWORD(wParam) <= ID_SAVE_10)
                {
                    auto slot = LOWORD(wParam) - ID_SAVE_1;
                    core_vr_wait_increment();

                    g_config.st_slot = slot;
                    Messenger::broadcast(Messenger::Message::SlotChanged, (size_t)g_config.st_slot);

                    AsyncExecutor::invoke_async([=] {
                        core_vr_wait_decrement();
                        core_st_do_slot(slot, core_st_job_save, nullptr, false);
                    });
                }
                else if (LOWORD(wParam) >= ID_LOAD_1 && LOWORD(wParam) <= ID_LOAD_10)
                {
                    auto slot = LOWORD(wParam) - ID_LOAD_1;
                    core_vr_wait_increment();

                    g_config.st_slot = slot;
                    Messenger::broadcast(Messenger::Message::SlotChanged, (size_t)g_config.st_slot);

                    AsyncExecutor::invoke_async([=] {
                        core_vr_wait_decrement();
                        core_st_do_slot(slot, core_st_job_load, nullptr, false);
                    });
                }
                else if (LOWORD(wParam) >= ID_RECENTROMS_FIRST &&
                         LOWORD(wParam) < (ID_RECENTROMS_FIRST + g_config.recent_rom_paths.size()))
                {
                    auto path = RecentMenu::element_at(g_config.recent_rom_paths, ID_RECENTROMS_FIRST, LOWORD(wParam));
                    if (path.empty())
                        break;

                    AsyncExecutor::invoke_async([path] {
                        const auto result = core_vr_start_rom(path, false);
                        if (result == Res_Ok)
                        {
                            g_rom_path = path;
                        }
                        show_error_dialog_for_result(result);
                    },
                                                ASYNC_KEY_START_ROM);
                }
                else if (LOWORD(wParam) >= ID_RECENTMOVIES_FIRST &&
                         LOWORD(wParam) < (ID_RECENTMOVIES_FIRST + g_config.recent_movie_paths.size()))
                {
                    auto path = RecentMenu::element_at(g_config.recent_movie_paths, ID_RECENTMOVIES_FIRST, LOWORD(wParam));
                    if (path.empty())
                        break;

                    g_config.core.vcr_readonly = true;
                    Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.core.vcr_readonly);
                    AsyncExecutor::invoke_async([path] {
                        auto result = core_vcr_start_playback(path);
                        show_error_dialog_for_result(result);
                    },
                                                ASYNC_KEY_PLAY_MOVIE);
                }
                else if (LOWORD(wParam) >= ID_LUA_RECENT && LOWORD(wParam) < (ID_LUA_RECENT + g_config.recent_lua_script_paths.size()))
                {
                    auto path = RecentMenu::element_at(g_config.recent_lua_script_paths, ID_LUA_RECENT, LOWORD(wParam));
                    if (path.empty())
                        break;

                    lua_create_and_run(path);
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
    CrashHelper::log_crash(ExceptionInfo);

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
    }
    else
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

void on_new_frame()
{
    g_frame_changed = true;
}

std::filesystem::path get_saves_directory()
{
    if (g_config.is_default_saves_directory_used)
    {
        return FrontendService::get_app_path().string() + "save\\";
    }
    return g_config.saves_directory;
}

bool load_plugins()
{
    if (g_video_plugin.get() && g_audio_plugin.get() && g_input_plugin.get() && g_rsp_plugin.get() && g_video_plugin->path() == g_config.selected_video_plugin && g_audio_plugin->path() == g_config.selected_audio_plugin && g_input_plugin->path() == g_config.selected_input_plugin && g_rsp_plugin->path() == g_config.selected_rsp_plugin)
    {
        g_core_logger->info("[Core] Plugins unchanged, reusing...");
    }
    else
    {
        g_video_plugin.reset();
        g_audio_plugin.reset();
        g_input_plugin.reset();
        g_rsp_plugin.reset();

        g_view_logger->trace(L"Loading video plugin: {}", g_config.selected_video_plugin);
        g_view_logger->trace(L"Loading audio plugin: {}", g_config.selected_audio_plugin);
        g_view_logger->trace(L"Loading input plugin: {}", g_config.selected_input_plugin);
        g_view_logger->trace(L"Loading RSP plugin: {}", g_config.selected_rsp_plugin);
        
        auto video_pl = Plugin::create(g_config.selected_video_plugin);
        auto audio_pl = Plugin::create(g_config.selected_audio_plugin);
        auto input_pl = Plugin::create(g_config.selected_input_plugin);
        auto rsp_pl = Plugin::create(g_config.selected_rsp_plugin);

        if (!video_pl.first.empty())
        {
            g_view_logger->error(L"Failed to load video plugin: {}", video_pl.first);
        }
        if (!audio_pl.first.empty())
        {
            g_view_logger->error(L"Failed to load audio plugin: {}", audio_pl.first);
        }
        if (!input_pl.first.empty())
        {
            g_view_logger->error(L"Failed to load input plugin: {}", input_pl.first);
        }
        if (!rsp_pl.first.empty())
        {
            g_view_logger->error(L"Failed to load rsp plugin: {}", rsp_pl.first);
        }
        
        if (video_pl.second == nullptr || audio_pl.second == nullptr || input_pl.second == nullptr || rsp_pl.second == nullptr)
        {
            video_pl.second.reset();
            audio_pl.second.reset();
            input_pl.second.reset();
            rsp_pl.second.reset();
            return false;
        }

        g_video_plugin = std::move(video_pl.second);
        g_audio_plugin = std::move(audio_pl.second);
        g_input_plugin = std::move(input_pl.second);
        g_rsp_plugin = std::move(rsp_pl.second);
    }
    return true;
}

void initiate_plugins()
{
    // HACK: We sleep between each plugin load, as that seems to remedy various plugins failing to initialize correctly.
    auto gfx_plugin_thread = std::thread([] { g_video_plugin->initiate(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto audio_plugin_thread = std::thread([] { g_audio_plugin->initiate(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto input_plugin_thread = std::thread([] { g_input_plugin->initiate(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto rsp_plugin_thread = std::thread([] { g_rsp_plugin->initiate(); });

    gfx_plugin_thread.join();
    audio_plugin_thread.join();
    input_plugin_thread.join();
    rsp_plugin_thread.join();
}

static void CALLBACK invalidate_callback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    core_vr_invalidate_visuals();

    // This has to be posted to ui thread since it requires synchronized access to the lua map
    PostMessage(g_main_hwnd, WM_INVALIDATE_LUA, 0, 0);
    
    static std::chrono::high_resolution_clock::time_point last_statusbar_update = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();

    if (g_frame_changed)
    {
        Statusbar::post(get_input_text(), Statusbar::Section::Input);

        if (EncodingManager::is_capturing())
        {
            if (core_vcr_get_task() == task_idle)
            {
                Statusbar::post(std::format(L"{}", EncodingManager::get_video_frame()), Statusbar::Section::VCR);
            }
            else
            {
                Statusbar::post(std::format(L"{}({})", get_status_text(), EncodingManager::get_video_frame()), Statusbar::Section::VCR);
            }
        }
        else
        {
            Statusbar::post(get_status_text(), Statusbar::Section::VCR);
        }

        g_frame_changed = false;
    }
    
    // We throttle FPS and VI/s visual updates to 1 per second, so no unstable values are displayed
    if (time - last_statusbar_update > std::chrono::seconds(1))
    {
        g_core.g_frame_deltas_mutex.lock();
        auto fps = get_rate_per_second_from_deltas(g_core.g_frame_deltas);
        g_core.g_frame_deltas_mutex.unlock();

        g_core.g_vi_deltas_mutex.lock();
        auto vis = get_rate_per_second_from_deltas(g_core.g_vi_deltas);
        g_core.g_vi_deltas_mutex.unlock();

        Statusbar::post(std::format(L"FPS: {:.1f}", fps), Statusbar::Section::FPS);
        Statusbar::post(std::format(L"VI/s: {:.1f}", vis), Statusbar::Section::VIs);

        last_statusbar_update = time;
    }
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
#ifdef _DEBUG
    open_console();
#endif

    Loggers::init();

    g_view_logger->info("WinMain");
    g_view_logger->info(get_mupen_name());

    g_core.cfg = &g_config.core;
    g_core.callbacks = {};
    g_core.callbacks.vi = [] {
        LuaService::call_interval();
        LuaService::call_vi();
        FrontendService::at_vi();
    };
    g_core.callbacks.input = LuaService::call_input;
    g_core.callbacks.frame = on_new_frame;
    g_core.callbacks.interval = LuaService::call_interval;
    g_core.callbacks.ai_len_changed = FrontendService::ai_len_changed;
    g_core.callbacks.play_movie = LuaService::call_play_movie;
    g_core.callbacks.stop_movie = [] {
        LuaService::call_stop_movie();
        if (EncodingManager::is_capturing())
            EncodingManager::stop_capture();
    };
    g_core.callbacks.loop_movie = [] {
        if (EncodingManager::is_capturing())
            EncodingManager::stop_capture();
    };
    g_core.callbacks.save_state = LuaService::call_save_state;
    g_core.callbacks.load_state = LuaService::call_load_state;
    g_core.callbacks.reset = LuaService::call_reset;
    g_core.callbacks.seek_completed = LuaService::call_seek_completed;
    g_core.callbacks.core_executing_changed = [](bool value) {
        Messenger::broadcast(Messenger::Message::CoreExecutingChanged, value);
    };
    g_core.callbacks.emu_paused_changed = [](bool value) {
        Messenger::broadcast(Messenger::Message::EmuPausedChanged, value);
    };
    g_core.callbacks.emu_launched_changed = [](bool value) {
        Messenger::broadcast(Messenger::Message::EmuLaunchedChanged, value);
    };
    g_core.callbacks.emu_starting_changed = [](bool value) {
        Messenger::broadcast(Messenger::Message::EmuStartingChanged, value);
    };
    g_core.callbacks.emu_stopping = []() {
        Messenger::broadcast(Messenger::Message::EmuStopping, nullptr);
    };
    g_core.callbacks.reset_completed = []() {
        Messenger::broadcast(Messenger::Message::ResetCompleted, nullptr);
    };
    g_core.callbacks.speed_modifier_changed = [](int32_t value) {
        Messenger::broadcast(Messenger::Message::SpeedModifierChanged, value);
    };
    g_core.callbacks.warp_modify_status_changed = [](bool value) {
        Messenger::broadcast(Messenger::Message::WarpModifyStatusChanged, value);
    };
    g_core.callbacks.current_sample_changed = [](int32_t value) {
        Compare::compare(value);
        Messenger::broadcast(Messenger::Message::CurrentSampleChanged, value);
    };
    g_core.callbacks.task_changed = [](core_vcr_task value) {
        Messenger::broadcast(Messenger::Message::TaskChanged, value);
    };
    g_core.callbacks.rerecords_changed = [](uint64_t value) {
        Messenger::broadcast(Messenger::Message::RerecordsChanged, value);
    };
    g_core.callbacks.unfreeze_completed = []() {
        Messenger::broadcast(Messenger::Message::UnfreezeCompleted, nullptr);
    };
    g_core.callbacks.seek_savestate_changed = [](size_t value) {
        Messenger::broadcast(Messenger::Message::SeekSavestateChanged, value);
    };
    g_core.callbacks.readonly_changed = [](bool value) {
        Messenger::broadcast(Messenger::Message::ReadonlyChanged, value);
    };
    g_core.callbacks.dacrate_changed = [](core_system_type value) {
        Messenger::broadcast(Messenger::Message::DacrateChanged, value);
    };
    g_core.callbacks.debugger_resumed_changed = [](bool value) {
        Messenger::broadcast(Messenger::Message::DebuggerResumedChanged, value);
    };
    g_core.callbacks.debugger_cpu_state_changed = [](core_dbg_cpu_state* value) {
        Messenger::broadcast(Messenger::Message::DebuggerCpuStateChanged, value);
    };
    g_core.callbacks.lag_limit_exceeded = []() {
        Messenger::broadcast(Messenger::Message::LagLimitExceeded, nullptr);
    };
    g_core.callbacks.seek_status_changed = []() {
        Messenger::broadcast(Messenger::Message::SeekStatusChanged, nullptr);
    };
    g_core.log_trace = [](const auto& str) {
        g_core_logger->trace(str);
    };
    g_core.log_info = [](const auto& str) {
        g_core_logger->info(str);
    };
    g_core.log_warn = [](const auto& str) {
        g_core_logger->warn(str);
    };
    g_core.log_error = [](const auto& str) {
        g_core_logger->error(str);
    };
    g_core.load_plugins = load_plugins;
    g_core.initiate_plugins = initiate_plugins;
    g_core.invoke_async = AsyncExecutor::invoke_async;
    g_core.get_saves_directory = get_saves_directory;
    g_core.get_backups_directory = get_backups_directory;
    g_core.show_multiple_choice_dialog = [] (const std::vector<std::wstring>& choices, const wchar_t* str, const wchar_t* title, core_dialog_type type) {
        return FrontendService::show_multiple_choice_dialog(choices, str, title, type);
    };
    g_core.show_ask_dialog = [] (const wchar_t* str, const wchar_t* title, bool warning) {
        return FrontendService::show_ask_dialog(str, title, warning);
    };
    g_core.show_dialog = [] (const wchar_t* str, const wchar_t* title, core_dialog_type type) {
        FrontendService::show_dialog(str, title, type);
    };
    g_core.show_statusbar = FrontendService::show_statusbar;
    g_core.update_screen = FrontendService::update_screen;
    g_core.copy_video = MGECompositor::copy_video;
    g_core.find_available_rom = FrontendService::find_available_rom;
    g_core.load_screen = MGECompositor::load_screen;
    g_core.get_plugin_names = [](char* video, char* audio, char* input, char* rsp) {
#define DO_COPY(type)                                                                          \
    if (type)                                                                                  \
    {                                                                                          \
        if (g_## type## _plugin)                                                                 \
        {                                                                                      \
            strncpy(type, g_## type## _plugin->name().data(), 64);                               \
        }                                                                                      \
        else                                                                                   \
        {                                                                                      \
            g_view_logger->error("Tried to get {} plugin name while it wasn't loaded", #type); \
        }                                                                                      \
    }\
    
        DO_COPY(video)
        DO_COPY(audio)
        DO_COPY(input)
        DO_COPY(rsp)
    };

    core_init(&g_core);
    
    g_ui_thread_id = GetCurrentThreadId();

    Gdiplus::GdiplusStartupInput startup_input;
    GdiplusStartup(&gdi_plus_token, &startup_input, NULL);

    Messenger::init();
    AsyncExecutor::init();
    CoreDbg::init();

    g_app_path = get_app_full_path();

    wchar_t cwd[MAX_PATH] = {0};
    GetCurrentDirectory(std::size(cwd), cwd);

    // CWD is sometimes messed up when running from CLI, so we fix it here
    // Needs to be fixed prior to loading config, since that uses a relative path
    SetCurrentDirectory(g_app_path.c_str());

    g_app_instance = hInstance;

    // ensure folders exist!
    CreateDirectory((g_app_path / L"save").c_str(), NULL);
    CreateDirectory((g_app_path / L"screenshots").c_str(), NULL);
    CreateDirectory((g_app_path / L"plugin").c_str(), NULL);
    CreateDirectory((g_app_path / L"backups").c_str(), NULL);

    init_config();
    load_config();
    lua_init();

    if (g_config.keep_default_working_directory)
    {
        SetCurrentDirectory(cwd);
    }

    // Log cwd again for fun
    GetCurrentDirectory(sizeof(cwd), cwd);
    g_view_logger->info(L"cwd: {}", cwd);

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

    g_view_logger->info("[View] Restoring window @ ({}|{}) {}x{}...", g_config.window_x, g_config.window_y, g_config.window_width, g_config.window_height);

    g_main_hwnd = CreateWindowEx(
    0,
    WND_CLASS,
    get_mupen_name().c_str(),
    WS_OVERLAPPEDWINDOW | WS_EX_COMPOSITED,
    g_config.window_x, g_config.window_y, g_config.window_width,
    g_config.window_height,
    NULL, NULL, hInstance, NULL);

    setup_dummy_info();

    ShowWindow(g_main_hwnd, nCmdShow);
    SetWindowLong(g_main_hwnd, GWL_EXSTYLE, WS_EX_ACCEPTFILES);

    g_recent_roms_menu = GetSubMenu(GetSubMenu(g_main_menu, 0), 5);
    g_recent_movies_menu = GetSubMenu(GetSubMenu(g_main_menu, 3), 5);
    g_recent_lua_menu = GetSubMenu(GetSubMenu(g_main_menu, 6), 2);
#ifndef _DEBUG
#endif

    RECT rect{};
    GetClientRect(g_main_hwnd, &rect);

    Messenger::subscribe(Messenger::Message::EmuLaunchedChanged, on_emu_launched_changed);
    Messenger::subscribe(Messenger::Message::EmuStopping, on_emu_stopping);
    Messenger::subscribe(Messenger::Message::EmuPausedChanged, on_emu_paused_changed);
    Messenger::subscribe(Messenger::Message::CapturingChanged, on_capturing_changed);
    Messenger::subscribe(Messenger::Message::MovieLoopChanged, on_movie_loop_changed);
    Messenger::subscribe(Messenger::Message::TaskChanged, on_task_changed);
    Messenger::subscribe(Messenger::Message::ScriptStarted, on_script_started);
    Messenger::subscribe(Messenger::Message::SpeedModifierChanged, on_speed_modifier_changed);
    Messenger::subscribe(Messenger::Message::LagLimitExceeded, on_vis_since_input_poll_exceeded);
    Messenger::subscribe(Messenger::Message::FullscreenChanged, on_fullscreen_changed);
    Messenger::subscribe(Messenger::Message::ConfigLoaded, on_config_loaded);
    Messenger::subscribe(Messenger::Message::SeekCompleted, on_seek_completed);
    Messenger::subscribe(Messenger::Message::WarpModifyStatusChanged, on_warp_modify_status_changed);
    Messenger::subscribe(Messenger::Message::FastForwardNeedsUpdate, update_core_fast_forward);
    Messenger::subscribe(Messenger::Message::SeekStatusChanged, [](std::any) {
        update_core_fast_forward(nullptr);
    });
    Messenger::subscribe(Messenger::Message::EmuStartingChanged, [](std::any data) {
        g_emu_starting = std::any_cast<bool>(data);
        update_titlebar();
    });

    // Rombrowser needs to be initialized *after* other components, since it depends on their state smh bru
    Statusbar::init();
    RomBrowser::init();
    EncodingManager::init();
    Cli::init();
    Seeker::init();

    update_core_fast_forward(nullptr);

    Messenger::broadcast(Messenger::Message::StatusbarVisibilityChanged, (bool)g_config.is_statusbar_enabled);
    Messenger::broadcast(Messenger::Message::MovieLoopChanged, (bool)g_config.core.is_movie_loop_enabled);
    Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.core.vcr_readonly);
    Messenger::broadcast(Messenger::Message::EmuLaunchedChanged, false);
    Messenger::broadcast(Messenger::Message::CoreExecutingChanged, false);
    Messenger::broadcast(Messenger::Message::CapturingChanged, false);
    Messenger::broadcast(Messenger::Message::SizeChanged, rect);
    Messenger::broadcast(Messenger::Message::AppReady, nullptr);
    Messenger::broadcast(Messenger::Message::ConfigLoaded, nullptr);

    // warning, this is ignored when debugger is attached (like visual studio)
    SetUnhandledExceptionFilter(ExceptionReleaseTarget);

    // raise noncontinuable exception (impossible to recover from it)
    // RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, NULL, NULL);
    //
    // raise continuable exception
    // RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, NULL, NULL);

    g_ui_timer = timeSetEvent(16, 1, invalidate_callback, 0, TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);
    if (!g_ui_timer)
    {
        FrontendService::show_dialog(L"timeSetEvent call failed. Verify that your system supports multimedia timers.", L"Error", fsvc_error);
        return -1;
    }
    
    SendMessage(g_main_hwnd, WM_COMMAND, MAKEWPARAM(IDM_CHECK_FOR_UPDATES, 0), 1);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    return (int)msg.wParam;
}
