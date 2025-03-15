/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <Config.h>
#include <DialogService.h>
#include <Messenger.h>

#include <ini.h>
#include <gui/Loggers.h>

cfg_view g_config;
std::vector<cfg_hotkey*> g_config_hotkeys;

const std::unordered_map<std::string, size_t> DIALOG_SILENT_MODE_CHOICES = {
{CORE_DLG_FLOAT_EXCEPTION, 0},
{CORE_DLG_ST_HASH_MISMATCH, 0},
{CORE_DLG_ST_UNFREEZE_WARNING, 0},
{CORE_DLG_ST_NOT_FROM_MOVIE, 0},
{CORE_DLG_VCR_RAWDATA_WARNING, 0},
{CORE_DLG_VCR_WIIVC_WARNING, 0},
{CORE_DLG_VCR_ROM_NAME_WARNING, 0},
{CORE_DLG_VCR_ROM_CCODE_WARNING, 0},
{CORE_DLG_VCR_ROM_CRC_WARNING, 0},
{CORE_DLG_VCR_CHEAT_LOAD_ERROR, 0},

{VIEW_DLG_MOVIE_OVERWRITE_WARNING, 0},
{VIEW_DLG_RESET_SETTINGS, 0},
{VIEW_DLG_RESET_PLUGIN_SETTINGS, 0},
{VIEW_DLG_LAG_EXCEEDED, 0},
{VIEW_DLG_CLOSE_ROM_WARNING, 0},
{VIEW_DLG_HOTKEY_CONFLICT, 0},
{VIEW_DLG_UPDATE_DIALOG, 2},
{VIEW_DLG_PLUGIN_LOAD_ERROR, 0},
{VIEW_DLG_RAMSTART, 0},
};

void set_default_hotkey_keys(cfg_view* config)
{
    config->fast_forward_hotkey.key = VK_TAB;

    config->gs_hotkey.key = 'G';

    config->speed_down_hotkey.key = VK_OEM_MINUS;

    config->speed_up_hotkey.key = VK_OEM_PLUS;
    
    config->speed_reset_hotkey.key = VK_OEM_PLUS;
    config->speed_reset_hotkey.ctrl = 1;

    config->frame_advance_hotkey.key = VK_OEM_5;
    
    config->multi_frame_advance_hotkey.key = VK_OEM_5;
    config->multi_frame_advance_hotkey.ctrl = 1;

    config->multi_frame_advance_inc_hotkey.key = 'E';
    config->multi_frame_advance_inc_hotkey.ctrl = 1;
    
    config->multi_frame_advance_dec_hotkey.key = 'Q';
    config->multi_frame_advance_dec_hotkey.ctrl = 1;

    config->multi_frame_advance_reset_hotkey.key = 'E';
    config->multi_frame_advance_reset_hotkey.ctrl = 1;
    config->multi_frame_advance_reset_hotkey.shift = 1;
    
    config->pause_hotkey.key = VK_PAUSE;

    config->toggle_read_only_hotkey.key = 'R';
    config->toggle_read_only_hotkey.shift = true;

    config->toggle_movie_loop_hotkey.key = 'L';
    config->toggle_movie_loop_hotkey.shift = true;

    config->start_movie_playback_hotkey.key = 'P';
    config->start_movie_playback_hotkey.ctrl = true;
    config->start_movie_playback_hotkey.shift = true;

    config->start_movie_recording_hotkey.key = 'R';
    config->start_movie_recording_hotkey.ctrl = true;
    config->start_movie_recording_hotkey.shift = true;

    config->stop_movie_hotkey.key = 'C';
    config->stop_movie_hotkey.ctrl = true;
    config->stop_movie_hotkey.shift = true;

    config->create_movie_backup_hotkey.key = 'B';
    config->create_movie_backup_hotkey.ctrl = true;
    config->create_movie_backup_hotkey.shift = true;

    config->take_screenshot_hotkey.key = VK_F12;

    config->play_latest_movie_hotkey.key = 'T';
    config->play_latest_movie_hotkey.ctrl = true;
    config->play_latest_movie_hotkey.shift = true;

    config->load_latest_script_hotkey.key = 'K';
    config->load_latest_script_hotkey.ctrl = true;
    config->load_latest_script_hotkey.shift = true;

    config->new_lua_hotkey.key = 'N';
    config->new_lua_hotkey.ctrl = true;

    config->close_all_lua_hotkey.key = 'W';
    config->close_all_lua_hotkey.ctrl = true;
    config->close_all_lua_hotkey.shift = true;

    config->load_rom_hotkey.key = 'O';
    config->load_rom_hotkey.ctrl = true;

    config->close_rom_hotkey.key = 'W';
    config->close_rom_hotkey.ctrl = true;

    config->reset_rom_hotkey.key = 'R';
    config->reset_rom_hotkey.ctrl = true;

    config->load_latest_rom_hotkey.key = 'O';
    config->load_latest_rom_hotkey.ctrl = true;
    config->load_latest_rom_hotkey.shift = true;

    config->fullscreen_hotkey.key = VK_RETURN;
    config->fullscreen_hotkey.alt = true;

    config->settings_hotkey.key = 'S';
    config->settings_hotkey.ctrl = true;

    config->toggle_statusbar_hotkey.key = 'S';
    config->toggle_statusbar_hotkey.alt = true;

    config->refresh_rombrowser_hotkey.key = VK_F5;
    config->refresh_rombrowser_hotkey.ctrl = true;

    config->seek_to_frame_hotkey.key = 'G';
    config->seek_to_frame_hotkey.ctrl = true;

    config->run_hotkey.key = 'P';
    config->run_hotkey.ctrl = true;

    config->cheats_hotkey.key = 'U';
    config->cheats_hotkey.ctrl = true;

    config->piano_roll_hotkey.key = 'K';
    config->piano_roll_hotkey.ctrl = true;

    config->save_current_hotkey.key = 'I';

    config->load_current_hotkey.key = 'P';

    config->save_as_hotkey.key = 'N';
    config->save_as_hotkey.ctrl = true;

    config->load_as_hotkey.key = 'M';
    config->load_as_hotkey.ctrl = true;

    config->undo_load_state_hotkey.key = 'Z';
    config->undo_load_state_hotkey.ctrl = true;

    config->save_to_slot_1_hotkey.key = '1';
    config->save_to_slot_1_hotkey.shift = true;

    config->save_to_slot_2_hotkey.key = '2';
    config->save_to_slot_2_hotkey.shift = true;

    config->save_to_slot_3_hotkey.key = '3';
    config->save_to_slot_3_hotkey.shift = true;

    config->save_to_slot_4_hotkey.key = '4';
    config->save_to_slot_4_hotkey.shift = true;

    config->save_to_slot_5_hotkey.key = '5';
    config->save_to_slot_5_hotkey.shift = true;

    config->save_to_slot_6_hotkey.key = '6';
    config->save_to_slot_6_hotkey.shift = true;

    config->save_to_slot_7_hotkey.key = '7';
    config->save_to_slot_7_hotkey.shift = true;

    config->save_to_slot_8_hotkey.key = '8';
    config->save_to_slot_8_hotkey.shift = true;

    config->save_to_slot_9_hotkey.key = '9';
    config->save_to_slot_9_hotkey.shift = true;

    config->save_to_slot_10_hotkey.key = '0';
    config->save_to_slot_10_hotkey.shift = true;

    config->load_from_slot_1_hotkey.key = VK_F1;

    config->load_from_slot_2_hotkey.key = VK_F2;

    config->load_from_slot_3_hotkey.key = VK_F3;

    config->load_from_slot_4_hotkey.key = VK_F4;

    config->load_from_slot_5_hotkey.key = VK_F5;

    config->load_from_slot_6_hotkey.key = VK_F6;

    config->load_from_slot_7_hotkey.key = VK_F7;

    config->load_from_slot_8_hotkey.key = VK_F8;

    config->load_from_slot_9_hotkey.key = VK_F9;

    config->load_from_slot_10_hotkey.key = VK_F10;

    config->select_slot_1_hotkey.key = '1';

    config->select_slot_2_hotkey.key = '2';

    config->select_slot_3_hotkey.key = '3';

    config->select_slot_4_hotkey.key = '4';

    config->select_slot_5_hotkey.key = '5';

    config->select_slot_6_hotkey.key = '6';

    config->select_slot_7_hotkey.key = '7';

    config->select_slot_8_hotkey.key = '8';

    config->select_slot_9_hotkey.key = '9';

    config->select_slot_10_hotkey.key = '0';
}

cfg_view get_default_config()
{
    cfg_view config = {};

    // TODO: Merge get_default_hotkey_keys() into this function
    
    config.fast_forward_hotkey = {
    .identifier = L"Fast-forward",
    .down_cmd = ACTION_FASTFORWARD_ON,
    .up_cmd = ACTION_FASTFORWARD_OFF};

    config.gs_hotkey = {
    .identifier = L"GS Button",
    .down_cmd = ACTION_GAMESHARK_ON,
    .up_cmd = ACTION_GAMESHARK_OFF};

    config.speed_down_hotkey = {
    .identifier = L"Speed down",
    .down_cmd = ACTION_SPEED_DOWN,
    };

    config.speed_up_hotkey = {
    .identifier = L"Speed up",
    .down_cmd = ACTION_SPEED_UP,
    };

    config.speed_reset_hotkey = {
    .identifier = L"Speed reset",
    .down_cmd = ACTION_SPEED_RESET,
    };

    config.frame_advance_hotkey = {
    .identifier = L"Frame advance",
    .down_cmd = ACTION_FRAME_ADVANCE,
    };

    config.multi_frame_advance_hotkey = {
    .identifier = L"Multi-Frame advance",
    .down_cmd = ACTION_MULTI_FRAME_ADVANCE,
    };

    config.multi_frame_advance_inc_hotkey = {
    .identifier = L"Multi-Frame advance increment",
    .down_cmd = ACTION_MULTI_FRAME_ADVANCE_INC,
    };

    config.multi_frame_advance_dec_hotkey = {
    .identifier = L"Multi-Frame advance decrement",
    .down_cmd = ACTION_MULTI_FRAME_ADVANCE_DEC,
    };

    config.multi_frame_advance_reset_hotkey = {
    .identifier = L"Multi-Frame advance reset",
    .down_cmd = ACTION_MULTI_FRAME_ADVANCE_RESET,
    };

    config.pause_hotkey = {
    .identifier = L"Pause",
    .down_cmd = ACTION_PAUSE,
    };

    config.toggle_read_only_hotkey = {
    .identifier = L"Toggle read-only",
    .down_cmd = ACTION_TOGGLE_READONLY,
    };

    config.toggle_movie_loop_hotkey = {
    .identifier = L"Toggle movie loop",
    .down_cmd = ACTION_TOGGLE_MOVIE_LOOP,
    };

    config.start_movie_playback_hotkey = {
    .identifier = L"Start movie playback",
    .down_cmd = ACTION_START_MOVIE_PLAYBACK,
    };

    config.start_movie_recording_hotkey = {
    .identifier = L"Start movie recording",
    .down_cmd = ACTION_START_MOVIE_RECORDING,
    };

    config.stop_movie_hotkey = {
    .identifier = L"Stop movie",
    .down_cmd = ACTION_STOP_MOVIE,
    };

    config.create_movie_backup_hotkey = {
    .identifier = L"Create Movie Backup",
    .down_cmd = ACTION_CREATE_MOVIE_BACKUP,
    };

    config.take_screenshot_hotkey = {
    .identifier = L"Take screenshot",
    .down_cmd = ACTION_TAKE_SCREENSHOT,
    };

    config.play_latest_movie_hotkey = {
    .identifier = L"Play latest movie",
    .down_cmd = ACTION_PLAY_LATEST_MOVIE,
    };

    config.load_latest_script_hotkey = {
    .identifier = L"Load latest script",
    .down_cmd = ACTION_LOAD_LATEST_SCRIPT,
    };

    config.new_lua_hotkey = {
    .identifier = L"New Lua Instance",
    .down_cmd = ACTION_NEW_LUA,
    };

    config.close_all_lua_hotkey = {
    .identifier = L"Close all Lua Instances",
    .down_cmd = ACTION_CLOSE_ALL_LUA,
    };

    config.load_rom_hotkey = {
    .identifier = L"Load ROM",
    .down_cmd = ACTION_LOAD_ROM,
    };

    config.close_rom_hotkey = {
    .identifier = L"Close ROM",
    .down_cmd = ACTION_CLOSE_ROM,
    };

    config.reset_rom_hotkey = {
    .identifier = L"Reset ROM",
    .down_cmd = ACTION_RESET_ROM,
    };

    config.load_latest_rom_hotkey = {
    .identifier = L"Load Latest ROM",
    .down_cmd = ACTION_LOAD_LATEST_ROM,
    };

    config.fullscreen_hotkey = {
    .identifier = L"Toggle Fullscreen",
    .down_cmd = ACTION_FULLSCREEN,
    };

    config.settings_hotkey = {
    .identifier = L"Show Settings",
    .down_cmd = ACTION_SETTINGS,
    };

    config.toggle_statusbar_hotkey = {
    .identifier = L"Toggle Statusbar",
    .down_cmd = ACTION_TOGGLE_STATUSBAR,
    };

    config.refresh_rombrowser_hotkey = {
    .identifier = L"Refresh Rombrowser",
    .down_cmd = ACTION_REFRESH_ROM_BROWSER,
    };

    config.seek_to_frame_hotkey = {
    .identifier = L"Seek to frame",
    .down_cmd = ACTION_OPEN_SEEKER,
    };

    config.run_hotkey = {
    .identifier = L"Run",
    .down_cmd = ACTION_OPEN_RUNNER,
    };

    config.piano_roll_hotkey = {
    .identifier = L"Open Piano Roll",
    .down_cmd = ACTION_OPEN_PIANO_ROLL,
    };

    config.cheats_hotkey = {
    .identifier = L"Open Cheats dialog",
    .down_cmd = ACTION_OPEN_CHEATS,
    };

    config.save_current_hotkey = {
    .identifier = L"Save to current slot",
    .down_cmd = ACTION_SAVE_SLOT,
    };

    config.load_current_hotkey = {
    .identifier = L"Load from current slot",
    .down_cmd = ACTION_LOAD_SLOT,
    };

    config.save_as_hotkey = {
    .identifier = L"Save state as",
    .down_cmd = ACTION_SAVE_AS,
    };

    config.load_as_hotkey = {
    .identifier = L"Load state as",
    .down_cmd = ACTION_LOAD_AS,
    };

    config.undo_load_state_hotkey = {
    .identifier = L"Undo load state",
    .down_cmd = ACTION_UNDO_LOAD_STATE,
    };

    config.save_to_slot_1_hotkey = {
    .identifier = L"Save to slot 1",
    .down_cmd = ACTION_SAVE_SLOT1,
    };

    config.save_to_slot_2_hotkey = {
    .identifier = L"Save to slot 2",
    .down_cmd = ACTION_SAVE_SLOT2,
    };

    config.save_to_slot_3_hotkey = {
    .identifier = L"Save to slot 3",
    .down_cmd = ACTION_SAVE_SLOT3,
    };

    config.save_to_slot_4_hotkey = {
    .identifier = L"Save to slot 4",
    .down_cmd = ACTION_SAVE_SLOT4,
    };

    config.save_to_slot_5_hotkey = {
    .identifier = L"Save to slot 5",
    .down_cmd = ACTION_SAVE_SLOT5,
    };

    config.save_to_slot_6_hotkey = {
    .identifier = L"Save to slot 6",
    .down_cmd = ACTION_SAVE_SLOT6,
    };

    config.save_to_slot_7_hotkey = {
    .identifier = L"Save to slot 7",
    .down_cmd = ACTION_SAVE_SLOT7,
    };

    config.save_to_slot_8_hotkey = {
    .identifier = L"Save to slot 8",
    .down_cmd = ACTION_SAVE_SLOT8,
    };

    config.save_to_slot_9_hotkey = {
    .identifier = L"Save to slot 9",
    .down_cmd = ACTION_SAVE_SLOT9,
    };

    config.save_to_slot_10_hotkey = {
    .identifier = L"Save to slot 10",
    .down_cmd = ACTION_SAVE_SLOT10,
    };

    config.load_from_slot_1_hotkey = {
    .identifier = L"Load from slot 1",
    .down_cmd = ACTION_LOAD_SLOT1,
    };

    config.load_from_slot_2_hotkey = {
    .identifier = L"Load from slot 2",
    .down_cmd = ACTION_LOAD_SLOT2,
    };

    config.load_from_slot_3_hotkey = {
    .identifier = L"Load from slot 3",
    .down_cmd = ACTION_LOAD_SLOT3,
    };

    config.load_from_slot_4_hotkey = {
    .identifier = L"Load from slot 4",
    .down_cmd = ACTION_LOAD_SLOT4,
    };

    config.load_from_slot_5_hotkey = {
    .identifier = L"Load from slot 5",
    .down_cmd = ACTION_LOAD_SLOT5,
    };

    config.load_from_slot_6_hotkey = {
    .identifier = L"Load from slot 6",
    .down_cmd = ACTION_LOAD_SLOT6,
    };

    config.load_from_slot_7_hotkey = {
    .identifier = L"Load from slot 7",
    .down_cmd = ACTION_LOAD_SLOT7,
    };

    config.load_from_slot_8_hotkey = {
    .identifier = L"Load from slot 8",
    .down_cmd = ACTION_LOAD_SLOT8,
    };

    config.load_from_slot_9_hotkey = {
    .identifier = L"Load from slot 9",
    .down_cmd = ACTION_LOAD_SLOT9,
    };

    config.load_from_slot_10_hotkey = {
    .identifier = L"Load from slot 10",
    .down_cmd = ACTION_LOAD_SLOT10,
    };

    config.select_slot_1_hotkey = {
    .identifier = L"Select slot 1",
    .down_cmd = ACTION_SELECT_SLOT1,
    };

    config.select_slot_2_hotkey = {
    .identifier = L"Select slot 2",
    .down_cmd = ACTION_SELECT_SLOT2,
    };

    config.select_slot_3_hotkey = {
    .identifier = L"Select slot 3",
    .down_cmd = ACTION_SELECT_SLOT3,
    };

    config.select_slot_4_hotkey = {
    .identifier = L"Select slot 4",
    .down_cmd = ACTION_SELECT_SLOT4,
    };

    config.select_slot_5_hotkey = {
    .identifier = L"Select slot 5",
    .down_cmd = ACTION_SELECT_SLOT5,
    };

    config.select_slot_6_hotkey = {
    .identifier = L"Select slot 6",
    .down_cmd = ACTION_SELECT_SLOT6,
    };

    config.select_slot_7_hotkey = {
    .identifier = L"Select slot 7",
    .down_cmd = ACTION_SELECT_SLOT7,
    };

    config.select_slot_8_hotkey = {
    .identifier = L"Select slot 8",
    .down_cmd = ACTION_SELECT_SLOT8,
    };

    config.select_slot_9_hotkey = {
    .identifier = L"Select slot 9",
    .down_cmd = ACTION_SELECT_SLOT9,
    };

    config.select_slot_10_hotkey = {
    .identifier = L"Select slot 10",
    .down_cmd = ACTION_SELECT_SLOT10,
    };

    for (const auto& pair : DIALOG_SILENT_MODE_CHOICES)
    {
        config.silent_mode_dialog_choices[string_to_wstring(pair.first)] = std::to_wstring(pair.second);
    }
    
    set_default_hotkey_keys(&config);

    return config;
}

const cfg_view g_default_config = get_default_config();

void handle_config_value(mINI::INIStructure& ini, const std::wstring& field_name, int32_t is_reading, cfg_hotkey* hotkey)
{
    auto converted_key = wstring_to_string(field_name);

    if (is_reading)
    {
        if (!ini.has(converted_key))
        {
            return;
        }

        hotkey->key = std::stoi(ini[converted_key]["key"]);
        hotkey->ctrl = std::stoi(ini[converted_key]["ctrl"]);
        hotkey->shift = std::stoi(ini[converted_key]["shift"]);
        hotkey->alt = std::stoi(ini[converted_key]["alt"]);
    }
    else
    {
        ini[converted_key]["key"] = std::to_string(hotkey->key);
        ini[converted_key]["ctrl"] = std::to_string(hotkey->ctrl);
        ini[converted_key]["shift"] = std::to_string(hotkey->shift);
        ini[converted_key]["alt"] = std::to_string(hotkey->alt);
    }
}

void handle_config_value(mINI::INIStructure& ini, const std::wstring& field_name, const int32_t is_reading, int32_t* value)
{
    auto converted_key = wstring_to_string(field_name);

    if (is_reading)
    {
        // keep the default value if the key doesnt exist
        // it will be created upon saving anyway
        if (!ini["Config"].has(converted_key))
        {
            return;
        }
        *value = std::stoi(ini["Config"][converted_key]);
    }
    else
    {
        ini["Config"][converted_key] = std::to_string(*value);
    }
}

void handle_config_value(mINI::INIStructure& ini, const std::wstring& field_name, const int32_t is_reading, uint64_t* value)
{
    auto converted_key = wstring_to_string(field_name);

    if (is_reading)
    {
        // keep the default value if the key doesnt exist
        // it will be created upon saving anyway
        if (!ini["Config"].has(converted_key))
        {
            return;
        }
        *value = std::stoull(ini["Config"][converted_key]);
    }
    else
    {
        ini["Config"][converted_key] = std::to_string(*value);
    }
}

void handle_config_value(mINI::INIStructure& ini, const std::wstring& field_name, const int32_t is_reading, std::wstring& value)
{
    auto converted_key = wstring_to_string(field_name);

    // BUG: Leading whitespace seems to be dropped by mINI after a roundtrip!!!

    if (is_reading)
    {
        // keep the default value if the key doesnt exist
        // it will be created upon saving anyway
        if (!ini["Config"].has(converted_key))
        {
            return;
        }
        value = string_to_wstring(ini["Config"][converted_key]);
    }
    else
    {
        ini["Config"][converted_key] = wstring_to_string(value);
    }
}

void handle_config_value(mINI::INIStructure& ini, const std::wstring& field_name, const int32_t is_reading, std::vector<std::wstring>& value)
{
    auto converted_key = wstring_to_string(field_name);

    if (is_reading)
    {
        // if the virtual collection doesn't exist just leave the vector empty, as attempting to read will crash
        if (!ini.has(converted_key))
        {
            return;
        }

        for (size_t i = 0; i < ini[converted_key].size(); i++)
        {
            value.push_back(string_to_wstring(ini[converted_key][std::to_string(i)]));
        }
    }
    else
    {
        // create virtual collection:
        // dump under key field_name with i
        // [field_name]
        // 0 = a.m64
        // 1 = b.m64
        for (size_t i = 0; i < value.size(); i++)
        {
            ini[converted_key][std::to_string(i)] = wstring_to_string(value[i]);
        }
    }
}

void handle_config_value(mINI::INIStructure& ini, const std::wstring& field_name, const int32_t is_reading, std::map<std::wstring, std::wstring>& value)
{
    auto converted_key = wstring_to_string(field_name);

    if (is_reading)
    {
        // if the virtual map doesn't exist just leave the vector empty, as attempting to read will crash
        if (!ini.has(converted_key))
        {
            return;
        }
        auto& map = ini[converted_key];
        for (auto& pair : map)
        {
            value[string_to_wstring(pair.first)] = string_to_wstring(pair.second);
        }
    }
    else
    {
        // create virtual map:
        // [field_name]
        // value = value
        for (auto& pair : value)
        {
            ini[converted_key][wstring_to_string(pair.first)] = wstring_to_string(pair.second);
        }
    }
}

void handle_config_value(mINI::INIStructure& ini, const std::wstring& field_name, const int32_t is_reading, std::vector<int32_t>& value)
{
    std::vector<std::wstring> string_values;
    for (const auto int_value : value)
    {
        string_values.push_back(std::to_wstring(int_value));
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

const auto first_offset = offsetof(cfg_view, fast_forward_hotkey);
const auto last_offset = offsetof(cfg_view, select_slot_10_hotkey);

std::vector<cfg_hotkey*> collect_hotkeys(const cfg_view* config)
{
    // NOTE:
    // last_offset should contain the offset of the last hotkey
    // this also requires that the hotkeys are laid out contiguously, or else the pointer arithmetic fails
    // i recommend inserting your new hotkeys before the savestate hotkeys... pretty please
    std::vector<cfg_hotkey*> vec;
    for (size_t i = 0; i < ((last_offset - first_offset) / sizeof(cfg_hotkey)) + 1; i++)
    {
        auto hotkey = &(((cfg_hotkey*)config)[i]);
        g_view_logger->info(L"Hotkey[{}]: {}", i, hotkey->identifier.c_str());
        vec.push_back(hotkey);
    }

    return vec;
}

mINI::INIStructure handle_config_ini(bool is_reading, mINI::INIStructure ini)
{
#define HANDLE_P_VALUE(x) handle_config_value(ini, L#x, is_reading, &g_config.x);
#define HANDLE_VALUE(x) handle_config_value(ini, L#x, is_reading, g_config.x);

    if (is_reading)
    {
        // We need to fill the config with latest default values first, because some "new" fields might not exist in the ini
        g_config = get_default_config();
    }

    for (auto hotkey : g_config_hotkeys)
    {
        handle_config_value(ini, hotkey->identifier, is_reading, hotkey);
    }

    HANDLE_VALUE(ignored_version)
    HANDLE_P_VALUE(core.total_rerecords)
    HANDLE_P_VALUE(core.total_frames)
    HANDLE_P_VALUE(core.core_type)
    HANDLE_P_VALUE(core.fps_modifier)
    HANDLE_P_VALUE(core.frame_skip_frequency)
    HANDLE_P_VALUE(st_slot)
    HANDLE_P_VALUE(core.fastforward_silent)
    HANDLE_P_VALUE(core.skip_rendering_lag)
    HANDLE_P_VALUE(core.rom_cache_size)
    HANDLE_P_VALUE(core.st_screenshot)
    HANDLE_P_VALUE(core.is_movie_loop_enabled)
    HANDLE_P_VALUE(core.counter_factor)
    HANDLE_P_VALUE(is_unfocused_pause_enabled)
    HANDLE_P_VALUE(is_statusbar_enabled)
    HANDLE_P_VALUE(statusbar_scale_up)
    HANDLE_P_VALUE(statusbar_layout)
    HANDLE_P_VALUE(plugin_discovery_async)
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
    HANDLE_P_VALUE(core.is_reset_recording_enabled)
    HANDLE_P_VALUE(capture_mode)
    HANDLE_P_VALUE(presenter_type)
    HANDLE_P_VALUE(lazy_renderer_init)
    HANDLE_P_VALUE(encoder_type)
    HANDLE_P_VALUE(capture_delay)
    HANDLE_VALUE(ffmpeg_final_options)
    HANDLE_VALUE(ffmpeg_path)
    HANDLE_P_VALUE(synchronization_mode)
    HANDLE_P_VALUE(keep_default_working_directory)
    HANDLE_P_VALUE(use_async_executor)
    HANDLE_P_VALUE(concurrency_fuzzing)
    HANDLE_P_VALUE(plugin_discovery_delayed)
    HANDLE_VALUE(lua_script_path)
    HANDLE_VALUE(recent_lua_script_paths)
    HANDLE_P_VALUE(is_recent_scripts_frozen)
    HANDLE_P_VALUE(core.seek_savestate_interval)
    HANDLE_P_VALUE(core.seek_savestate_max_count)
    HANDLE_P_VALUE(piano_roll_constrain_edit_to_column)
    HANDLE_P_VALUE(piano_roll_undo_stack_size)
    HANDLE_P_VALUE(piano_roll_keep_selection_visible)
    HANDLE_P_VALUE(piano_roll_keep_playhead_visible)
    HANDLE_P_VALUE(core.st_undo_load)
    HANDLE_P_VALUE(core.use_summercart)
    HANDLE_P_VALUE(core.wii_vc_emulation)
    HANDLE_P_VALUE(core.float_exception_emulation)
    HANDLE_P_VALUE(core.is_audio_delay_enabled)
    HANDLE_P_VALUE(core.is_compiled_jump_enabled)
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
    HANDLE_P_VALUE(settings_tab)
    HANDLE_P_VALUE(vcr_0_index)
    HANDLE_P_VALUE(increment_slot)
    HANDLE_P_VALUE(core.pause_at_frame)
    HANDLE_P_VALUE(core.pause_at_last_frame)
    HANDLE_P_VALUE(core.vcr_readonly)
    HANDLE_P_VALUE(core.vcr_backups)
    HANDLE_P_VALUE(core.vcr_write_extended_format)
    HANDLE_P_VALUE(automatic_update_checking)
    HANDLE_P_VALUE(silent_mode)
    HANDLE_P_VALUE(core.max_lag)
    HANDLE_VALUE(seeker_value)
    HANDLE_P_VALUE(multi_frame_advance_count)
    HANDLE_VALUE(silent_mode_dialog_choices)

    return ini;
}

std::filesystem::path get_config_path()
{
    return g_app_path / L"config.ini";
}

/// Modifies the config to apply value limits and other constraints
void config_apply_limits()
{
    // handle edge case: closing while minimized produces bogus values for position
    if (g_config.window_x < -10'000 || g_config.window_y < -10'000)
    {
        g_config.window_x = g_default_config.window_x;
        g_config.window_y = g_default_config.window_y;
        g_config.window_width = g_default_config.window_width;
        g_config.window_height = g_default_config.window_height;
    }

    if (g_config.rombrowser_column_widths.size() < 4)
    {
        // something's malformed, fuck off and use default values
        g_config.rombrowser_column_widths = g_default_config.rombrowser_column_widths;
    }

    // Causes too many issues
    if (g_config.core.seek_savestate_interval == 1)
    {
        g_config.core.seek_savestate_interval = 2;
    }

    g_config.settings_tab = std::min(std::max(g_config.settings_tab, 0), 2);

    for (const auto& pair : DIALOG_SILENT_MODE_CHOICES)
    {
        const auto key = string_to_wstring(pair.first);
        if (!g_config.silent_mode_dialog_choices.contains(key))
        {
            g_config.silent_mode_dialog_choices[key] = std::to_wstring(pair.second);
        }
    }
}

void save_config()
{
    Messenger::broadcast(Messenger::Message::ConfigSaving, nullptr);
    config_apply_limits();

    std::remove(get_config_path().string().c_str());

    mINI::INIFile file(get_config_path().string());
    mINI::INIStructure ini;

    ini = handle_config_ini(false, ini);

    file.write(ini, true);
}

void load_config()
{
    if (!std::filesystem::exists(get_config_path()))
    {
        g_view_logger->info("[CONFIG] Default config file does not exist. Generating...");
        g_config = get_default_config();
        save_config();
    }

    mINI::INIFile file(get_config_path().string());
    mINI::INIStructure ini;
    file.read(ini);

    ini = handle_config_ini(true, ini);

    config_apply_limits();

    Messenger::broadcast(Messenger::Message::ConfigLoaded, nullptr);
}

void init_config()
{
    g_config_hotkeys = collect_hotkeys(&g_config);
}
