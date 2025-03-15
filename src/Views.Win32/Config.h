/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <core_types.h>

// TODO: Remove the action abstraction, since hotkeys only exist in the frontend now :D

/**
 * \brief Action that can be triggered by a hotkey
 */
typedef enum {
    ACTION_NONE,
    ACTION_FASTFORWARD_ON,
    ACTION_FASTFORWARD_OFF,
    ACTION_GAMESHARK_ON,
    ACTION_GAMESHARK_OFF,
    ACTION_SPEED_DOWN,
    ACTION_SPEED_UP,
    ACTION_SPEED_RESET,
    ACTION_FRAME_ADVANCE,
    ACTION_MULTI_FRAME_ADVANCE,
    ACTION_MULTI_FRAME_ADVANCE_INC,
    ACTION_MULTI_FRAME_ADVANCE_DEC,
    ACTION_MULTI_FRAME_ADVANCE_RESET,
    ACTION_PAUSE,
    ACTION_TOGGLE_READONLY,
    ACTION_TOGGLE_MOVIE_LOOP,
    ACTION_START_MOVIE_PLAYBACK,
    ACTION_START_MOVIE_RECORDING,
    ACTION_STOP_MOVIE,
    ACTION_CREATE_MOVIE_BACKUP,
    ACTION_TAKE_SCREENSHOT,
    ACTION_PLAY_LATEST_MOVIE,
    ACTION_LOAD_LATEST_SCRIPT,
    ACTION_NEW_LUA,
    ACTION_CLOSE_ALL_LUA,
    ACTION_LOAD_ROM,
    ACTION_CLOSE_ROM,
    ACTION_RESET_ROM,
    ACTION_LOAD_LATEST_ROM,
    ACTION_FULLSCREEN,
    ACTION_SETTINGS,
    ACTION_TOGGLE_STATUSBAR,
    ACTION_REFRESH_ROM_BROWSER,
    ACTION_OPEN_SEEKER,
    ACTION_OPEN_RUNNER,
    ACTION_OPEN_PIANO_ROLL,
    ACTION_OPEN_CHEATS,
    ACTION_SAVE_SLOT,
    ACTION_LOAD_SLOT,
    ACTION_SAVE_AS,
    ACTION_LOAD_AS,
    ACTION_UNDO_LOAD_STATE,
    ACTION_SAVE_SLOT1,
    ACTION_SAVE_SLOT2,
    ACTION_SAVE_SLOT3,
    ACTION_SAVE_SLOT4,
    ACTION_SAVE_SLOT5,
    ACTION_SAVE_SLOT6,
    ACTION_SAVE_SLOT7,
    ACTION_SAVE_SLOT8,
    ACTION_SAVE_SLOT9,
    ACTION_SAVE_SLOT10,
    ACTION_LOAD_SLOT1,
    ACTION_LOAD_SLOT2,
    ACTION_LOAD_SLOT3,
    ACTION_LOAD_SLOT4,
    ACTION_LOAD_SLOT5,
    ACTION_LOAD_SLOT6,
    ACTION_LOAD_SLOT7,
    ACTION_LOAD_SLOT8,
    ACTION_LOAD_SLOT9,
    ACTION_LOAD_SLOT10,
    ACTION_SELECT_SLOT1,
    ACTION_SELECT_SLOT2,
    ACTION_SELECT_SLOT3,
    ACTION_SELECT_SLOT4,
    ACTION_SELECT_SLOT5,
    ACTION_SELECT_SLOT6,
    ACTION_SELECT_SLOT7,
    ACTION_SELECT_SLOT8,
    ACTION_SELECT_SLOT9,
    ACTION_SELECT_SLOT10,
} cfg_action;

typedef enum {
    PRESENTER_DCOMP,
    PRESENTER_GDI
} cfg_presenter_type;

typedef enum {
    ENCODER_VFW,
    ENCODER_FFMPEG
} cfg_encoder_type;

/**
 * \brief The statusbar layout preset.
 */
typedef enum {
    /**
     * The legacy layout.
     */
    LAYOUT_CLASSIC,

    /**
     * The new layout containing additional information.
     */
    LAYOUT_MODERN,

    /**
     * The new layout, but with a section for read-only status.
     */
    LAYOUT_MODERN_WITH_READONLY,
} cfg_statusbar_layout;

typedef struct Hotkey {
    std::wstring identifier;
    int32_t key;
    int32_t ctrl;
    int32_t shift;
    int32_t alt;
    cfg_action down_cmd;
    cfg_action up_cmd;
} cfg_hotkey;

#pragma pack(push, 1)
typedef struct ViewCfg {
#pragma region Hotkeys
    cfg_hotkey fast_forward_hotkey;
    cfg_hotkey gs_hotkey;
    cfg_hotkey speed_down_hotkey;
    cfg_hotkey speed_up_hotkey;
    cfg_hotkey speed_reset_hotkey;
    cfg_hotkey frame_advance_hotkey;
    cfg_hotkey multi_frame_advance_hotkey;
    cfg_hotkey multi_frame_advance_inc_hotkey;
    cfg_hotkey multi_frame_advance_dec_hotkey;
    cfg_hotkey multi_frame_advance_reset_hotkey;
    cfg_hotkey pause_hotkey;
    cfg_hotkey toggle_read_only_hotkey;
    cfg_hotkey toggle_movie_loop_hotkey;
    cfg_hotkey start_movie_playback_hotkey;
    cfg_hotkey start_movie_recording_hotkey;
    cfg_hotkey stop_movie_hotkey;
    cfg_hotkey create_movie_backup_hotkey;
    cfg_hotkey take_screenshot_hotkey;
    cfg_hotkey play_latest_movie_hotkey;
    cfg_hotkey load_latest_script_hotkey;
    cfg_hotkey new_lua_hotkey;
    cfg_hotkey close_all_lua_hotkey;
    cfg_hotkey load_rom_hotkey;
    cfg_hotkey close_rom_hotkey;
    cfg_hotkey reset_rom_hotkey;
    cfg_hotkey load_latest_rom_hotkey;
    cfg_hotkey fullscreen_hotkey;
    cfg_hotkey settings_hotkey;
    cfg_hotkey toggle_statusbar_hotkey;
    cfg_hotkey refresh_rombrowser_hotkey;
    cfg_hotkey seek_to_frame_hotkey;
    cfg_hotkey run_hotkey;
    cfg_hotkey piano_roll_hotkey;
    cfg_hotkey cheats_hotkey;
    cfg_hotkey save_current_hotkey;
    cfg_hotkey load_current_hotkey;
    cfg_hotkey save_as_hotkey;
    cfg_hotkey load_as_hotkey;
    cfg_hotkey undo_load_state_hotkey;
    cfg_hotkey save_to_slot_1_hotkey;
    cfg_hotkey save_to_slot_2_hotkey;
    cfg_hotkey save_to_slot_3_hotkey;
    cfg_hotkey save_to_slot_4_hotkey;
    cfg_hotkey save_to_slot_5_hotkey;
    cfg_hotkey save_to_slot_6_hotkey;
    cfg_hotkey save_to_slot_7_hotkey;
    cfg_hotkey save_to_slot_8_hotkey;
    cfg_hotkey save_to_slot_9_hotkey;
    cfg_hotkey save_to_slot_10_hotkey;
    cfg_hotkey load_from_slot_1_hotkey;
    cfg_hotkey load_from_slot_2_hotkey;
    cfg_hotkey load_from_slot_3_hotkey;
    cfg_hotkey load_from_slot_4_hotkey;
    cfg_hotkey load_from_slot_5_hotkey;
    cfg_hotkey load_from_slot_6_hotkey;
    cfg_hotkey load_from_slot_7_hotkey;
    cfg_hotkey load_from_slot_8_hotkey;
    cfg_hotkey load_from_slot_9_hotkey;
    cfg_hotkey load_from_slot_10_hotkey;
    cfg_hotkey select_slot_1_hotkey;
    cfg_hotkey select_slot_2_hotkey;
    cfg_hotkey select_slot_3_hotkey;
    cfg_hotkey select_slot_4_hotkey;
    cfg_hotkey select_slot_5_hotkey;
    cfg_hotkey select_slot_6_hotkey;
    cfg_hotkey select_slot_7_hotkey;
    cfg_hotkey select_slot_8_hotkey;
    cfg_hotkey select_slot_9_hotkey;
    cfg_hotkey select_slot_10_hotkey;

#pragma endregion

    /// <summary>
    /// The core config.
    /// </summary>
    core_cfg core{};

    /// <summary>
    /// The new version of Mupen64 currently ignored by the update checker.
    /// <para></para>
    /// L"" means no ignored version.
    /// </summary>
    std::wstring ignored_version;

    /// <summary>
    /// The current savestate slot index (0-9).
    /// </summary>
    int32_t st_slot;
    
    /// <summary>
    /// Whether emulation will pause when the main window loses focus
    /// </summary>
    int32_t is_unfocused_pause_enabled;

    /// <summary>
    /// Whether the statusbar is enabled
    /// </summary>
    int32_t is_statusbar_enabled = 1;

    /// <summary>
    /// Whether the statusbar is allowed to scale its segments down.
    /// </summary>
    int32_t statusbar_scale_down = 1;

    /// <summary>
    /// Whether the statusbar is allowed to scale its segments up.
    /// </summary>
    int32_t statusbar_scale_up;

    /// <summary>
    /// The statusbar layout.
    /// </summary>
    int32_t statusbar_layout = LAYOUT_MODERN;

    /// <summary>
    /// Whether plugins discovery is performed asynchronously. Removes potential waiting times in the config dialog.
    /// </summary>
    int32_t plugin_discovery_async = 1;

    /// <summary>
    /// Whether the default plugins directory will be used (otherwise, falls back to <see cref="plugins_directory"/>)
    /// </summary>
    int32_t is_default_plugins_directory_used = 1;

    /// <summary>
    /// Whether the default save directory will be used (otherwise, falls back to <see cref="saves_directory"/>)
    /// </summary>
    int32_t is_default_saves_directory_used = 1;

    /// <summary>
    /// Whether the default screenshot directory will be used (otherwise, falls back to <see cref="screenshots_directory"/>)
    /// </summary>
    int32_t is_default_screenshots_directory_used = 1;

    /// <summary>
    /// Whether the default backups directory will be used (otherwise, falls back to <see cref="backups_directory"/>)
    /// </summary>
    int32_t is_default_backups_directory_used = 1;

    /// <summary>
    /// The plugin directory
    /// </summary>
    std::wstring plugins_directory;

    /// <summary>
    /// The save directory
    /// </summary>
    std::wstring saves_directory;

    /// <summary>
    /// The screenshot directory
    /// </summary>
    std::wstring screenshots_directory;

    /// <summary>
    /// The savestate directory
    /// </summary>
    std::wstring states_path;

    /// <summary>
    /// The movie backups directory
    /// </summary>
    std::wstring backups_directory;

    /// <summary>
    /// The recently opened roms' paths
    /// </summary>
    std::vector<std::wstring> recent_rom_paths;

    /// <summary>
    /// Whether recently opened rom path collection is paused
    /// </summary>
    int32_t is_recent_rom_paths_frozen;

    /// <summary>
    /// The recently opened movies' paths
    /// </summary>
    std::vector<std::wstring> recent_movie_paths;

    /// <summary>
    /// Whether recently opened movie path collection is paused
    /// </summary>
    int32_t is_recent_movie_paths_frozen;

    /// <summary>
    /// Whether the rom browser will recursively search for roms beginning in the specified directories
    /// </summary>
    int32_t is_rombrowser_recursion_enabled;

    /// <summary>
    /// The paths to directories which are searched for roms
    /// </summary>
    std::vector<std::wstring> rombrowser_rom_paths;
    
    /// <summary>
    /// The strategy to use when capturing video
    /// <para/>
    /// 0 = Use the video plugin's readScreen or read_video (MGE)
    /// 1 = Internal capture of window
    /// 2 = Internal capture of screen cropped to window
    /// 3 = Use the video plugin's readScreen or read_video (MGE) composited with lua scripts
    /// </summary>
    int32_t capture_mode = 3;

    /// <summary>
    /// The presenter to use for Lua scripts
    /// </summary>
    int32_t presenter_type = PRESENTER_DCOMP;

    /// <summary>
    /// Enables lazy Lua renderer initialization. Greatly speeds up start and stop times for certain scripts.
    /// </summary>
    int32_t lazy_renderer_init = 1;

    /// <summary>
    /// The encoder to use for capturing.
    /// </summary>
    int32_t encoder_type = ENCODER_VFW;

    /// <summary>
    /// The delay (in milliseconds) before capturing the window
    /// <para/>
    /// May be useful when capturing other windows alongside mupen
    /// </summary>
    int32_t capture_delay;

    /// <summary>
    /// FFmpeg post-stream option format string which is used when capturing using the FFmpeg encoder type
    /// </summary>
    std::wstring ffmpeg_final_options =
    L"-y -f rawvideo -pixel_format bgr24 -video_size %dx%d -framerate %d -i %s "
    L"-f s16le -sample_rate %d -ac 2 -channel_layout stereo -i %s "
    L"-c:v libx264 -preset veryfast -tune zerolatency -crf 23 -c:a aac -b:a 128k -vf \"vflip\" -f mp4 %s";

    /// <summary>
    /// FFmpeg binary path
    /// </summary>
    std::wstring ffmpeg_path = L"C:\\ffmpeg\\bin\\ffmpeg.exe";

    /// <summary>
    /// The audio-video synchronization mode
    /// <para/>
    /// 0 - No Sync
    /// 1 - Audio Sync
    /// 2 - Video Sync
    /// </summary>
    int32_t synchronization_mode = 1;

    /// <summary>
    /// When enabled, mupen won't change the working directory to its current path at startup
    /// </summary>
    int32_t keep_default_working_directory;

    /// <summary>
    /// Whether the async executor is used for async calls. If disabled, a new thread is spawned for each call (legacy behaviour).
    /// </summary>
    int32_t use_async_executor;

    /// <summary>
    /// Whether the view will apply concurrency fuzzing. When enabled, some functions will be delayed to expose delayed task execution handling deficiencies at the callsite.
    /// </summary>
    int32_t concurrency_fuzzing;

    /// <summary>
    /// Whether the plugin discovery process is artificially lengthened.
    /// </summary>
    int32_t plugin_discovery_delayed;

    /// <summary>
    /// The lua script path
    /// </summary>
    std::wstring lua_script_path;

    /// <summary>
    /// The recently opened lua scripts' paths
    /// </summary>
    std::vector<std::wstring> recent_lua_script_paths;

    /// <summary>
    /// Whether recently opened lua script path collection is paused
    /// </summary>
    int32_t is_recent_scripts_frozen;
    
    /// <summary>
    /// Whether piano roll edits are constrained to the column they started on
    /// </summary>
    int32_t piano_roll_constrain_edit_to_column;

    /// <summary>
    /// Maximum size of the undo/redo stack.
    /// </summary>
    int32_t piano_roll_undo_stack_size = 100;

    /// <summary>
    /// Whether the piano roll will try to keep the selection visible when the frame changes
    /// </summary>
    int32_t piano_roll_keep_selection_visible;

    /// <summary>
    /// Whether the piano roll will try to keep the playhead visible when the frame changes
    /// </summary>
    int32_t piano_roll_keep_playhead_visible;
    
    /// <summary>
    /// The path of the currently selected video plugin
    /// </summary>
    std::wstring selected_video_plugin;

    /// <summary>
    /// The path of the currently selected audio plugin
    /// </summary>
    std::wstring selected_audio_plugin;

    /// <summary>
    /// The path of the currently selected input plugin
    /// </summary>
    std::wstring selected_input_plugin;

    /// <summary>
    /// The path of the currently selected RSP plugin
    /// </summary>
    std::wstring selected_rsp_plugin;

    /// <summary>
    /// The last known value of the record movie dialog's "start type" field
    /// </summary>
    int32_t last_movie_type = 1; // (MOVIE_START_FROM_SNAPSHOT)

    /// <summary>
    /// The last known value of the record movie dialog's "author" field
    /// </summary>
    std::wstring last_movie_author;

    /// <summary>
    /// The main window's X position
    /// </summary>
    int32_t window_x = ((int)0x80000000);

    /// <summary>
    /// The main window's Y position
    /// </summary>
    int32_t window_y = ((int)0x80000000);

    /// <summary>
    /// The main window's width
    /// </summary>
    int32_t window_width = 640;

    /// <summary>
    /// The main window's height
    /// </summary>
    int32_t window_height = 480;

    /// <summary>
    /// The width of rombrowser columns by index
    /// </summary>
    std::vector<std::int32_t> rombrowser_column_widths = {24, 240, 240, 120};

    /// <summary>
    /// The index of the currently sorted column, or -1 if none is sorted
    /// </summary>
    int32_t rombrowser_sorted_column;

    /// <summary>
    /// Whether the selected column is sorted in an ascending order
    /// </summary>
    int32_t rombrowser_sort_ascending = 1;

    /// <summary>
    /// A map of persistent path dialog IDs and the respective value
    /// </summary>
    std::map<std::wstring, std::wstring> persistent_folder_paths;

    /// <summary>
    /// The last selected settings tab's index.
    /// </summary>
    int32_t settings_tab;
    
    /// <summary>
    /// Whether VCR displays frame information relative to frame 0, not 1
    /// </summary>
    int32_t vcr_0_index;

    /// <summary>
    /// Increments the current slot when saving savestate to slot
    /// </summary>
    int32_t increment_slot;
    
    /// <summary>
    /// Whether automatic update checking is enabled.
    /// </summary>
    int32_t automatic_update_checking;

    /// <summary>
    /// Whether mupen will avoid showing modals and other elements which require user interaction
    /// </summary>
    int32_t silent_mode = 0;
    
    /// <summary>
    /// The current seeker input value
    /// </summary>
    std::wstring seeker_value;

    /// <summary>
    /// The multi-frame advance index.
    /// </summary>
    int32_t multi_frame_advance_count = 2;

    /// <summary>
    /// A map of dialog IDs to their default choices for silent mode.
    /// </summary>
    std::map<std::wstring, std::wstring> silent_mode_dialog_choices;
} cfg_view;
#pragma pack(pop)

extern cfg_view g_config;
extern const cfg_view g_default_config;
extern std::vector<cfg_hotkey*> g_config_hotkeys;

/**
 * \brief Initializes the config subsystem
 */
void init_config();

/**
 * \brief Saves the current config state to the config file
 */
void save_config();

/**
 * \brief Restores the config state from the config file
 */
void load_config();


/**
 * \brief Gets the string representation of a hotkey
 * \param hotkey The hotkey to convert
 * \return The hotkey as a string
 */
std::wstring hotkey_to_string(const cfg_hotkey* hotkey);
