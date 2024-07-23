#include "Config.hpp"
#include <cstdio>
#include <filesystem>
#include <lib/ini.h>
#include <shared/helpers/StringHelpers.h>
#include <shared/Messenger.h>
#include <shared/services/FrontendService.h>

t_config g_config;
std::vector<t_hotkey*> g_config_hotkeys;

t_config get_default_config()
{
	t_config config = {};

	config.fast_forward_hotkey = {
		.identifier = "Fast-forward",
		.down_cmd = Action::FastforwardOn,
		.up_cmd = Action::FastforwardOff
	};

	config.gs_hotkey = {
		.identifier = "GS Button",
		.down_cmd = Action::GamesharkOn,
		.up_cmd = Action::GamesharkOff
	};

	config.speed_down_hotkey = {
		.identifier = "Speed down",
		.down_cmd = Action::SpeedDown,
	};

	config.speed_up_hotkey = {
		.identifier = "Speed up",
		.down_cmd = Action::SpeedUp,
	};

	config.frame_advance_hotkey = {
		.identifier = "Frame advance",
		.down_cmd = Action::FrameAdvance,
	};

	config.pause_hotkey = {
		.identifier = "Pause",
		.down_cmd = Action::Pause,
	};

	config.toggle_read_only_hotkey = {
		.identifier = "Toggle read-only",
		.down_cmd = Action::ToggleReadOnly,
	};

	config.toggle_movie_loop_hotkey = {
		.identifier = "Toggle movie loop",
		.down_cmd = Action::ToggleMovieLoop,
	};

	config.start_movie_playback_hotkey = {
		.identifier = "Start movie playback",
		.down_cmd = Action::StartMoviePlayback,
	};

	config.start_movie_recording_hotkey = {
		.identifier = "Start movie recording",
		.down_cmd = Action::StartMovieRecording,
	};

	config.stop_movie_hotkey = {
		.identifier = "Stop movie",
		.down_cmd = Action::StopMovie,
	};

	config.take_screenshot_hotkey = {
		.identifier = "Take screenshot",
		.down_cmd = Action::TakeScreenshot,
	};

	config.play_latest_movie_hotkey = {
		.identifier = "Play latest movie",
		.down_cmd = Action::PlayLatestMovie,
	};

	config.load_latest_script_hotkey = {
		.identifier = "Load latest script",
		.down_cmd = Action::LoadLatestScript,
	};

	config.new_lua_hotkey = {
		.identifier = "New Lua Instance",
		.down_cmd = Action::NewLua,
	};

	config.close_all_lua_hotkey = {
		.identifier = "Close all Lua Instances",
		.down_cmd = Action::CloseAllLua,
	};

	config.load_rom_hotkey = {
		.identifier = "Load ROM",
		.down_cmd = Action::LoadRom,
	};

	config.close_rom_hotkey = {
		.identifier = "Close ROM",
		.down_cmd = Action::CloseRom,
	};

	config.reset_rom_hotkey = {
		.identifier = "Reset ROM",
		.down_cmd = Action::ResetRom,
	};

	config.load_latest_rom_hotkey = {
		.identifier = "Load Latest ROM",
		.down_cmd = Action::LoadLatestRom,
	};

	config.fullscreen_hotkey = {
		.identifier = "Toggle Fullscreen",
		.down_cmd = Action::Fullscreen,
	};

	config.settings_hotkey = {
		.identifier = "Show Settings",
		.down_cmd = Action::Settings,
	};

	config.toggle_statusbar_hotkey = {
		.identifier = "Toggle Statusbar",
		.down_cmd = Action::ToggleStatusbar,
	};

	config.refresh_rombrowser_hotkey = {
		.identifier = "Refresh Rombrowser",
		.down_cmd = Action::RefreshRomBrowser,
	};

	config.seek_to_frame_hotkey = {
		.identifier = "Seek to frame",
		.down_cmd = Action::OpenSeeker,
	};

	config.run_hotkey = {
		.identifier = "Run",
		.down_cmd = Action::OpenRunner,
	};

	config.cheats_hotkey = {
		.identifier = "Open Cheats dialog",
		.down_cmd = Action::OpenCheats,
	};

	config.save_current_hotkey = {
		.identifier = "Save to current slot",
		.down_cmd = Action::SaveSlot,
	};

	config.load_current_hotkey = {
		.identifier = "Load from current slot",
		.down_cmd = Action::LoadSlot,
	};

	config.save_as_hotkey = {
		.identifier = "Save state as",
		.down_cmd = Action::SaveAs,
	};

	config.load_as_hotkey = {
		.identifier = "Load state as",
		.down_cmd = Action::LoadAs,
	};

	config.save_to_slot_1_hotkey = {
		.identifier = "Save to slot 1",
		.down_cmd = Action::SaveSlot1,
	};

	config.save_to_slot_2_hotkey = {
		.identifier = "Save to slot 2",
		.down_cmd = Action::SaveSlot2,
	};

	config.save_to_slot_3_hotkey = {
		.identifier = "Save to slot 3",
		.down_cmd = Action::SaveSlot3,
	};

	config.save_to_slot_4_hotkey = {
		.identifier = "Save to slot 4",
		.down_cmd = Action::SaveSlot4,
	};

	config.save_to_slot_5_hotkey = {
		.identifier = "Save to slot 5",
		.down_cmd = Action::SaveSlot5,
	};

	config.save_to_slot_6_hotkey = {
		.identifier = "Save to slot 6",
		.down_cmd = Action::SaveSlot6,
	};

	config.save_to_slot_7_hotkey = {
		.identifier = "Save to slot 7",
		.down_cmd = Action::SaveSlot7,
	};

	config.save_to_slot_8_hotkey = {
		.identifier = "Save to slot 8",
		.down_cmd = Action::SaveSlot8,
	};

	config.save_to_slot_9_hotkey = {
		.identifier = "Save to slot 9",
		.down_cmd = Action::SaveSlot9,
	};

	config.save_to_slot_10_hotkey = {
		.identifier = "Save to slot 10",
		.down_cmd = Action::SaveSlot10,
	};

	config.load_from_slot_1_hotkey = {
		.identifier = "Load from slot 1",
		.down_cmd = Action::LoadSlot1,
	};

	config.load_from_slot_2_hotkey = {
		.identifier = "Load from slot 2",
		.down_cmd = Action::LoadSlot2,
	};

	config.load_from_slot_3_hotkey = {
		.identifier = "Load from slot 3",
		.down_cmd = Action::LoadSlot3,
	};

	config.load_from_slot_4_hotkey = {
		.identifier = "Load from slot 4",
		.down_cmd = Action::LoadSlot4,
	};

	config.load_from_slot_5_hotkey = {
		.identifier = "Load from slot 5",
		.down_cmd = Action::LoadSlot5,
	};

	config.load_from_slot_6_hotkey = {
		.identifier = "Load from slot 6",
		.down_cmd = Action::LoadSlot6,
	};

	config.load_from_slot_7_hotkey = {
		.identifier = "Load from slot 7",
		.down_cmd = Action::LoadSlot7,
	};

	config.load_from_slot_8_hotkey = {
		.identifier = "Load from slot 8",
		.down_cmd = Action::LoadSlot8,
	};

	config.load_from_slot_9_hotkey = {
		.identifier = "Load from slot 9",
		.down_cmd = Action::LoadSlot9,
	};

	config.load_from_slot_10_hotkey = {
		.identifier = "Load from slot 10",
		.down_cmd = Action::LoadSlot10,
	};

	config.select_slot_1_hotkey = {
		.identifier = "Select slot 1",
		.down_cmd = Action::SelectSlot1,
	};

	config.select_slot_2_hotkey = {
		.identifier = "Select slot 2",
		.down_cmd = Action::SelectSlot2,
	};

	config.select_slot_3_hotkey = {
		.identifier = "Select slot 3",
		.down_cmd = Action::SelectSlot3,
	};

	config.select_slot_4_hotkey = {
		.identifier = "Select slot 4",
		.down_cmd = Action::SelectSlot4,
	};

	config.select_slot_5_hotkey = {
		.identifier = "Select slot 5",
		.down_cmd = Action::SelectSlot5,
	};

	config.select_slot_6_hotkey = {
		.identifier = "Select slot 6",
		.down_cmd = Action::SelectSlot6,
	};

	config.select_slot_7_hotkey = {
		.identifier = "Select slot 7",
		.down_cmd = Action::SelectSlot7,
	};

	config.select_slot_8_hotkey = {
		.identifier = "Select slot 8",
		.down_cmd = Action::SelectSlot8,
	};

	config.select_slot_9_hotkey = {
		.identifier = "Select slot 9",
		.down_cmd = Action::SelectSlot9,
	};

	config.select_slot_10_hotkey = {
		.identifier = "Select slot 10",
		.down_cmd = Action::SelectSlot10,
	};

	FrontendService::set_default_hotkey_keys(&config);

	return config;
}

t_config default_config = get_default_config();

void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
                         int32_t is_reading, t_hotkey* hotkey)
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

void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
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

void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
                         const int32_t is_reading, uint64_t* value)
{
	if (is_reading)
	{
		// keep the default value if the key doesnt exist
		// it will be created upon saving anyway
		if (!ini["Config"].has(field_name))
		{
			return;
		}
		*value = std::stoull(ini["Config"][field_name]);
	} else
	{
		ini["Config"][field_name] = std::to_string(*value);
	}
}

void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
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

void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
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

void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
                         const int32_t is_reading, std::map<std::string, std::wstring>& value)
{
	if (is_reading)
	{
		// if the virtual map doesn't exist just leave the vector empty, as attempting to read will crash
		if (!ini.has(field_name))
		{
			return;
		}
		auto& map = ini[field_name];
		for (auto& pair : map)
		{
			value[pair.first] = string_to_wstring(pair.second);
		}
	} else
	{
		// create virtual map:
		// [field_name]
		// value = value
		for (auto& pair : value)
		{
			ini[field_name][pair.first] = wstring_to_string(pair.second);
		}
	}
}

void handle_config_value(mINI::INIStructure& ini, const std::string& field_name,
                         const int32_t is_reading, std::vector<int32_t>& value)
{
	std::vector<std::string> string_values;
	for (const auto int_value : value)
	{
		string_values.push_back(std::to_string(int_value));
	}

	handle_config_value(ini, field_name, is_reading, string_values);

	if (is_reading)
	{
		for (int i = 0; i < value.size(); ++i)
		{
			value[i] = std::stoi(string_values[i]);
		}
	}
}

const auto first_offset = offsetof(t_config, fast_forward_hotkey);
const auto last_offset = offsetof(t_config, select_slot_10_hotkey);

std::vector<t_hotkey*> collect_hotkeys(const t_config* config)
{
	// NOTE:
	// last_offset should contain the offset of the last hotkey
	// this also requires that the hotkeys are laid out contiguously, or else the pointer arithmetic fails
	// i recommend inserting your new hotkeys before the savestate hotkeys... pretty please
	std::vector<t_hotkey*> vec;
	for (size_t i = 0; i < ((last_offset - first_offset) / sizeof(t_hotkey)) + 1; i++)
	{
		auto hotkey = &(((t_hotkey*)config)[i]);
		printf("Hotkey[%d]: %s\n", i, hotkey->identifier.c_str());
		vec.push_back(hotkey);
	}

	return vec;
}

mINI::INIStructure handle_config_ini(bool is_reading, mINI::INIStructure ini)
{
#define HANDLE_P_VALUE(x) handle_config_value(ini, #x, is_reading, &g_config.x);
#define HANDLE_VALUE(x) handle_config_value(ini, #x, is_reading, g_config.x);

	if (is_reading)
	{
		// We need to fill the config with latest default values first, because some "new" fields might not exist in the ini
		g_config = get_default_config();
	}

	for (auto hotkey : g_config_hotkeys)
	{
		handle_config_value(ini, hotkey->identifier, is_reading, hotkey);
	}

	HANDLE_P_VALUE(version)
	HANDLE_P_VALUE(total_rerecords)
	HANDLE_P_VALUE(total_frames)
	HANDLE_P_VALUE(core_type)
	HANDLE_P_VALUE(fps_modifier)
	HANDLE_P_VALUE(frame_skip_frequency)
	HANDLE_P_VALUE(fastforward_silent)
	HANDLE_P_VALUE(skip_rendering_lag)
	HANDLE_P_VALUE(rom_cache_size)
	HANDLE_P_VALUE(st_screenshot)
	HANDLE_P_VALUE(is_movie_loop_enabled)
	HANDLE_P_VALUE(cpu_clock_speed_multiplier)
	HANDLE_P_VALUE(is_unfocused_pause_enabled)
	HANDLE_P_VALUE(is_statusbar_enabled)
	HANDLE_P_VALUE(is_default_plugins_directory_used)
	HANDLE_P_VALUE(is_default_saves_directory_used)
	HANDLE_P_VALUE(is_default_screenshots_directory_used)
	HANDLE_P_VALUE(is_default_backups_directory_used)
	HANDLE_VALUE(plugins_directory)
	HANDLE_VALUE(saves_directory)
	HANDLE_VALUE(screenshots_directory)
	HANDLE_VALUE(states_path)
	HANDLE_VALUE(backups_directory)
	HANDLE_VALUE(recent_rom_paths)
	HANDLE_P_VALUE(is_recent_rom_paths_frozen)
	HANDLE_VALUE(recent_movie_paths)
	HANDLE_P_VALUE(is_recent_movie_paths_frozen)
	HANDLE_P_VALUE(is_rombrowser_recursion_enabled)
	HANDLE_P_VALUE(is_reset_recording_enabled)
	HANDLE_P_VALUE(capture_mode)
	HANDLE_P_VALUE(presenter_type)
	HANDLE_P_VALUE(capture_delay)
	HANDLE_P_VALUE(synchronization_mode)
	HANDLE_VALUE(lua_script_path)
	HANDLE_VALUE(recent_lua_script_paths)
	HANDLE_P_VALUE(is_recent_scripts_frozen)
	HANDLE_P_VALUE(use_summercart)
	HANDLE_P_VALUE(is_round_towards_zero_enabled)
	HANDLE_P_VALUE(is_float_exception_propagation_enabled)
	HANDLE_P_VALUE(is_audio_delay_enabled)
	HANDLE_P_VALUE(is_compiled_jump_enabled)
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
	HANDLE_P_VALUE(fast_reset)
	HANDLE_P_VALUE(vcr_0_index)
	HANDLE_P_VALUE(increment_slot)
	HANDLE_P_VALUE(pause_at_frame)
	HANDLE_P_VALUE(pause_at_last_frame)
	HANDLE_P_VALUE(vcr_readonly)
	HANDLE_P_VALUE(vcr_backups)
	HANDLE_P_VALUE(silent_mode)
	HANDLE_P_VALUE(max_lag)
	HANDLE_VALUE(seeker_value)

	return ini;
}

// TODO: Should return std::filesystem::path
std::string get_config_path()
{
	return FrontendService::get_app_path().string() + "config.ini";
}

void save_config()
{
	Messenger::broadcast(Messenger::Message::ConfigSaving, nullptr);

	std::remove(get_config_path().c_str());

	mINI::INIFile file(get_config_path());
	mINI::INIStructure ini;

	ini = handle_config_ini(false, ini);

	file.write(ini, true);
}

void load_config()
{
	if (!std::filesystem::exists(get_config_path()))
	{
		printf("[CONFIG] Default config file does not exist. Generating...\n");
		g_config = get_default_config();
		save_config();
	}

	mINI::INIFile file(get_config_path());
	mINI::INIStructure ini;
	file.read(ini);

	ini = handle_config_ini(true, ini);

	// handle edge case: closing while minimized produces bogus values for position
	if (g_config.window_x < -10'000 || g_config.window_y < -10'000)
	{
		g_config.window_x = default_config.window_x;
		g_config.window_y = default_config.window_y;
		g_config.window_width = default_config.window_width;
		g_config.window_height = default_config.window_height;
	}

	if (g_config.rombrowser_column_widths.size() < 4)
	{
		// something's malformed, fuck off and use default values
		g_config.rombrowser_column_widths = default_config.rombrowser_column_widths;
	}

	Messenger::broadcast(Messenger::Message::ConfigLoaded, nullptr);
}

void init_config()
{
	g_config_hotkeys = collect_hotkeys(&g_config);
}
