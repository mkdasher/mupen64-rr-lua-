#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

typedef struct t_hotkey
{
	std::string identifier;
	int32_t key;
	int32_t ctrl;
	int32_t shift;
	int32_t alt;
	int32_t command;
} t_hotkey;

typedef struct CONFIG
{
#pragma region Hotkeys
	t_hotkey fast_forward_hotkey;
	t_hotkey speed_up_hotkey;
	t_hotkey speed_down_hotkey;
	t_hotkey frame_advance_hotkey;
	t_hotkey pause_hotkey;
	t_hotkey toggle_read_only_hotkey;
	t_hotkey start_movie_playback_hotkey;
	t_hotkey stop_movie_playback_hotkey;
	t_hotkey start_movie_recording_hotkey;
	t_hotkey stop_movie_recording_hotkey;
	t_hotkey take_screenshot_hotkey;
	t_hotkey save_to_current_slot_hotkey;
	t_hotkey load_from_current_slot_hotkey;
	t_hotkey restart_movie_hotkey;
	t_hotkey play_latest_movie_hotkey;
	t_hotkey save_to_slot_1_hotkey;
	t_hotkey save_to_slot_2_hotkey;
	t_hotkey save_to_slot_3_hotkey;
	t_hotkey save_to_slot_4_hotkey;
	t_hotkey save_to_slot_5_hotkey;
	t_hotkey save_to_slot_6_hotkey;
	t_hotkey save_to_slot_7_hotkey;
	t_hotkey save_to_slot_8_hotkey;
	t_hotkey save_to_slot_9_hotkey;
	t_hotkey save_to_slot_10_hotkey;
	t_hotkey load_from_slot_1_hotkey;
	t_hotkey load_from_slot_2_hotkey;
	t_hotkey load_from_slot_3_hotkey;
	t_hotkey load_from_slot_4_hotkey;
	t_hotkey load_from_slot_5_hotkey;
	t_hotkey load_from_slot_6_hotkey;
	t_hotkey load_from_slot_7_hotkey;
	t_hotkey load_from_slot_8_hotkey;
	t_hotkey load_from_slot_9_hotkey;
	t_hotkey load_from_slot_10_hotkey;
	t_hotkey select_slot_1_hotkey;
	t_hotkey select_slot_2_hotkey;
	t_hotkey select_slot_3_hotkey;
	t_hotkey select_slot_4_hotkey;
	t_hotkey select_slot_5_hotkey;
	t_hotkey select_slot_6_hotkey;
	t_hotkey select_slot_7_hotkey;
	t_hotkey select_slot_8_hotkey;
	t_hotkey select_slot_9_hotkey;
	t_hotkey select_slot_10_hotkey;

#pragma endregion

	/// <summary>
	/// The selected language
	/// </summary>
	std::string language;

	/// <summary>
	/// Whether FPS are shown to the user
	/// </summary>
	int32_t show_fps;

	/// <summary>
	/// Whether VI/s are shown to the user
	/// TODO: fix disabling this breaking emulation
	/// </summary>
	int32_t show_vis_per_second;

	/// <summary>
	/// Whether the user will be notified of savestate errors
	/// TODO: Deprecate or unify
	/// </summary>
	int32_t is_savestate_warning_enabled;

	/// <summary>
	/// Whether movie rom metadata is checked against the currently loaded rom
	/// </summary>
	int32_t is_rom_movie_compatibility_check_enabled;

	/// <summary>
	/// The currently selected core type
	/// <para/>
	/// 0 - Cached Interpreter
	/// 1 - Dynamic Recompiler
	/// 2 - Pure Interpreter
	/// </summary>
	int32_t core_type;

	/// <summary>
	/// The target FPS modifier as a percentage
	/// <para/>
	/// (100 = default)
	/// </summary>
	int32_t fps_modifier;

	/// <summary>
	/// The frequency at which frames are skipped during fast-forward
	/// <para/>
	/// 0 = skip all frames
	/// >0 = every nth frame is skipped
	/// </summary>
	int32_t frame_skip_frequency;

	/// <summary>
	/// Whether a playing movie will loop upon ending
	/// </summary>
	int32_t is_movie_loop_enabled;

	/// <summary>
	/// The emulated CPU's clock speed multiplier
	/// <para/>
	/// 1 = original clock speed
	/// high number = approach native clock speed
	/// </summary>
	int32_t cpu_clock_speed_multiplier;

	/// <summary>
	/// Whether emulation will pause when the main window loses focus
	/// </summary>
	int32_t is_unfocused_pause_enabled;

	/// <summary>
	/// Whether the toolbar is enabled
	/// </summary>
	int32_t is_toolbar_enabled;

	/// <summary>
	/// Whether the statusbar is enabled
	/// </summary>
	int32_t is_statusbar_enabled;

	/// <summary>
	/// Whether savestate loading will be allowed regardless of any incompatiblities
	/// TODO: better represent how dangerous this is in the view layer
	/// </summary>
	int32_t is_state_independent_state_loading_allowed;

	/// <summary>
	/// Whether the default plugins directory will be used (otherwise, falls back to <see cref="plugins_directory"/>)
	/// </summary>
	int32_t is_default_plugins_directory_used;

	/// <summary>
	/// Whether the default save directory will be used (otherwise, falls back to <see cref="saves_directory"/>)
	/// </summary>
	int32_t is_default_saves_directory_used;

	/// <summary>
	/// Whether the default screenshot directory will be used (otherwise, falls back to <see cref="screenshots_directory"/>)
	/// </summary>
	int32_t is_default_screenshots_directory_used;

	/// <summary>
	/// The plugin directory
	/// </summary>
	std::string plugins_directory;

	/// <summary>
	/// The save directory
	/// </summary>
	std::string saves_directory;

	/// <summary>
	/// The screenshot directory
	/// </summary>
	std::string screenshots_directory;

	/// <summary>
	/// The savestate directory
	/// </summary>
	std::string states_path;

	/// <summary>
	/// The recently opened roms' paths
	/// </summary>
	std::vector<std::string> recent_rom_paths;

	/// <summary>
	/// Whether recently opened rom path collection is paused
	/// </summary>
	int32_t is_recent_rom_paths_frozen;

	/// <summary>
	/// The recently opened movies' paths
	/// </summary>
	std::vector<std::string> recent_movie_paths;

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
	std::vector<std::string> rombrowser_rom_paths;

	/// <summary>
	/// Whether rom resets are not recorded in movies
	/// TODO: Negate related conditions and wording
	/// </summary>
	int32_t is_reset_recording_enabled;

	/// <summary>
	/// Whether video will forcibly be captured by mupen itself
	/// <para/>
	/// false = video capture might be performed by video plugin
	/// </summary>
	int32_t is_internal_capture_forced;

	/// <summary>
	/// Whether the video capture is performed by cropping the screen to mupen's client area
	/// </summary>
	int32_t is_capture_cropped_screen_dc;

	/// <summary>
	/// The delay (in milliseconds) before capturing the window
	/// <para/>
	/// May be useful when capturing other windows alongside mupen
	/// </summary>
	int32_t capture_delay;

	/// <summary>
	/// Whether selecting hotkeys inside the OEM-reserved range is allowed
	/// </summary>
	int32_t is_unknown_hotkey_selection_allowed;

	/// <summary>
	/// The avi capture path
	/// </summary>
	std::string avi_capture_path;

	/// <summary>
	/// The audio-video synchronization mode
	/// <para/>
	/// 0 - drop/duplicate samples to synchronize with video
	/// 1 - drop/duplicate frames to synchronize with audio
	/// 2 - no synchronization
	/// </summary>
	int32_t synchronization_mode;

	/// <summary>
	/// The lua script path
	/// </summary>
	std::string lua_script_path;

	/// <summary>
	/// The recently opened lua scripts' paths
	/// </summary>
	std::vector<std::string> recent_lua_script_paths;

	/// <summary>
	/// Whether recently opened lua script path collection is paused
	/// </summary>
	int32_t is_recent_scripts_frozen;

	/// <summary>
	/// SD card emulation
	/// </summary>
	int32_t use_summercart;

	/// <summary>
	/// Whether floats will be truncated when rounding
	/// </summary>
	int32_t is_round_towards_zero_enabled;

	/// <summary>
	/// Whether floating point exceptions will propagate and crash the emulator
	/// </summary>
	int32_t is_float_exception_propagation_enabled;

	/// <summary>
	/// Whether audio interrupts will be delayed
	/// </summary>
	int32_t is_audio_delay_enabled;

	/// <summary>
	/// Whether jmp instructions will cause the following code to be JIT'd by dynarec
	/// </summary>
	int32_t is_compiled_jump_enabled;

	/// <summary>
	/// Whether lua drawing is double-buffered
	/// </summary>
	int32_t is_lua_double_buffered;

	/// <summary>
	/// The path of the currently selected video plugin
	/// </summary>
	std::string selected_video_plugin;

	/// <summary>
	/// The path of the currently selected audio plugin
	/// </summary>
	std::string selected_audio_plugin;

	/// <summary>
	/// The path of the currently selected input plugin
	/// </summary>
	std::string selected_input_plugin;

	/// <summary>
	/// The path of the currently selected RSP plugin
	/// </summary>
	std::string selected_rsp_plugin;

	/// <summary>
	/// The last known value of the record movie dialog's "start type" field
	/// </summary>
	int32_t last_movie_type;

	/// <summary>
	/// The last known value of the record movie dialog's "author" field
	/// </summary>
	std::string last_movie_author;

	/// <summary>
	/// The main window's X position
	/// </summary>
	int32_t window_x;

	/// <summary>
	/// The main window's Y position
	/// </summary>
	int32_t window_y;

	/// <summary>
	/// The main window's width
	/// </summary>
	int32_t window_width;

	/// <summary>
	/// The main window's height
	/// </summary>
	int32_t window_height;

	/// <summary>
	/// The width of rombrowser columns by index
	/// </summary>
	std::vector<std::int32_t> rombrowser_column_widths;

	/// <summary>
	/// The index of the currently sorted column, or -1 if none is sorted
	/// </summary>
	int32_t rombrowser_sorted_column;

	/// <summary>
	/// Whether the selected column is sorted in an ascending order
	/// </summary>
	int32_t rombrowser_sort_ascending;

	/// <summary>
	/// A map of persistent path dialog IDs and the respective value
	/// </summary>
	std::map<std::string, std::wstring> persistent_folder_paths;

} CONFIG;

extern "C" CONFIG Config;
extern std::vector<t_hotkey*> hotkeys;

/**
 * \brief Gets the string representation of a hotkey
 * \param hotkey The hotkey to convert
 * \return The hotkey as a string
 */
std::string hotkey_to_string(t_hotkey* hotkey);

/**
 * \brief Saves the current config state to the config file
 */
void save_config();

/**
 * \brief Restores the config state from the config file
 */
void load_config();

/// <summary>
/// Waits until the user inputs a valid key sequence, then fills out the hotkey
/// </summary>
/// <returns>
/// Whether a hotkey has successfully been picked
/// </returns>
int32_t get_user_hotkey(t_hotkey* hotkey);

/**
 * \brief Updates all menu hotkey accelerator labels to the appropriate config values
 */
void update_menu_hotkey_labels();
