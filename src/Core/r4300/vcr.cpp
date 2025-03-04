/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <IOHelpers.h>
#include <Core.h>
#include <cheats.h>
#include <include/core_api.h>
#include <memory/pif.h>
#include <memory/savestates.h>
#include <r4300/r4300.h>
#include <r4300/rom.h>
#include <r4300/timers.h>
#include <r4300/vcr.h>

// M64\0x1a
enum
{
    mup_magic = (0x1a34364d),
    mup_version = (3),
};

constexpr auto RAWDATA_WARNING_MESSAGE = L"Warning: One of the active controllers of your input plugin is set to accept \"Raw Data\".\nThis can cause issues when recording and playing movies. Proceed?";
constexpr auto ROM_NAME_WARNING_MESSAGE = L"The movie was recorded on the rom '{}', but is being played back on '{}'.\r\nPlayback might desynchronize. Are you sure you want to continue?";
constexpr auto ROM_COUNTRY_WARNING_MESSAGE = L"The movie was recorded on a rom with country {}, but is being played back on {}.\r\nPlayback might desynchronize. Are you sure you want to continue?";
constexpr auto ROM_CRC_WARNING_MESSAGE = L"The movie was recorded with a ROM that has CRC \"0x%X\",\nbut you are using a ROM with CRC \"0x%X\".\r\nPlayback might desynchronize. Are you sure you want to continue?";
constexpr auto TRUNCATE_MESSAGE = L"Failed to truncate the movie file. The movie may be corrupted.";
constexpr auto WII_VC_MISMATCH_A_WARNING_MESSAGE = L"The movie was recorded with WiiVC mode enabled, but is being played back with it disabled.\r\nPlayback might desynchronize. Are you sure you want to continue?";
constexpr auto WII_VC_MISMATCH_B_WARNING_MESSAGE = L"The movie was recorded with WiiVC mode disabled, but is being played back with it enabled.\r\nPlayback might desynchronize. Are you sure you want to continue?";
constexpr auto OLD_MOVIE_EXTENDED_SECTION_NONZERO_MESSAGE = L"The movie was recorded prior to the extended format being available, but contains data in an extended format section.\r\nThe movie may be corrupted. Are you sure you want to continue?";
constexpr auto WARP_MODIFY_SEEKBACK_FAILED_MESSAGE = L"Failed to seek back during a warp modify operation, error code {}.\r\nPiano roll might be desynced.";
constexpr auto CHEAT_ERROR_ASK_MESSAGE = L"This movie has a cheat file associated with it, but it could not be loaded.\r\nPlayback might desynchronize. Are you sure you want to continue?";
constexpr auto CONTROLLER_ON_OFF_MISMATCH = L"Controller {} is enabled by the input plugin, but it is disabled in the movie.\nPlayback might desynchronize.\n";
constexpr auto CONTROLLER_OFF_ON_MISMATCH = L"Controller {} is disabled by the input plugin, but it is enabled in the movie.\nPlayback can't commence.\n";
constexpr auto CONTROLLER_MEMPAK_MISMATCH = L"Controller {} has a Memory Pak in the movie.\nPlayback might desynchronize.\n";
constexpr auto CONTROLLER_RUMBLEPAK_MISMATCH = L"Controller {} has a Rumble Pak in the movie.\nPlayback might desynchronize.\n";
constexpr auto CONTROLLER_MEMPAK_RUMBLEPAK_MISMATCH = L"Controller {} does not have a Memory or Rumble Pak in the movie.\nPlayback might desynchronize.\n";

volatile core_vcr_task g_task = task_idle;

// The frame to seek to during playback, or an empty option if no seek is being performed
std::optional<size_t> seek_to_frame;
bool g_seek_pause_at_end;
std::atomic g_seek_savestate_loading = false;
std::atomic g_reset_pending = false;
std::unordered_map<size_t, std::vector<uint8_t>> g_seek_savestates;

bool g_warp_modify_active = false;
size_t g_warp_modify_first_difference_frame = 0;

core_vcr_movie_header g_header;
std::vector<core_buttons> g_movie_inputs;
std::filesystem::path g_movie_path;

int32_t m_current_sample = -1;
int32_t m_current_vi = -1;
bool g_movie_has_cheat;

// Used for tracking user-invoked resets
bool vcr_reset_requested;

std::recursive_mutex vcr_mutex;

bool vcr_is_task_recording(core_vcr_task task);

bool write_movie_impl(const core_vcr_movie_header* hdr, const std::vector<core_buttons>& inputs, const std::filesystem::path& path)
{
    g_core->log_info(std::format(L"[VCR] write_movie_impl to {}...", g_movie_path.wstring()));

    FILE* f = fopen(path.string().c_str(), "wb+");
    if (!f)
    {
        return false;
    }

    core_vcr_movie_header hdr_copy = *hdr;
    
    if (!g_core->cfg->vcr_write_extended_format)
    {
        g_core->log_info(L"[VCR] vcr_write_extended_format disabled, replacing new sections with 0...");
        hdr_copy.extended_version = 0;
        memset(&hdr_copy.extended_flags, 0, sizeof(hdr_copy.extended_flags));
        memset(hdr_copy.extended_data.authorship_tag, 0, sizeof(hdr_copy.extended_data.authorship_tag));
        memset(&hdr_copy.extended_data, 0, sizeof(hdr_copy.extended_flags));
    }
    
    fwrite(&hdr_copy, sizeof(core_vcr_movie_header), 1, f);
    fwrite(inputs.data(), sizeof(core_buttons), hdr_copy.length_samples, f);
    fclose(f);
    return true;
}

// Writes the movie header + inputs to current movie_path
bool write_movie()
{
    if (!vcr_is_task_recording(g_task))
    {
        g_core->log_info(L"[VCR] Tried to flush current movie while not in recording task");
        return true;
    }

    g_core->log_info(L"[VCR] Flushing current movie...");

    return write_movie_impl(&g_header, g_movie_inputs, g_movie_path);
}

bool write_backup_impl()
{
    g_core->log_info(L"[VCR] Backing up movie...");
    const auto filename = std::format("{}.{}.m64", g_movie_path.stem().string(), static_cast<uint64_t>(time(nullptr)));

    return write_movie_impl(&g_header, g_movie_inputs, g_core->get_backups_directory() / filename);
}

bool is_task_playback(const core_vcr_task task)
{
    return task == task_start_playback_from_reset || task == task_start_playback_from_snapshot || task == task_playback;
}

uint64_t get_rerecord_count()
{
    return static_cast<uint64_t>(g_header.extended_data.rerecord_count) << 32 | g_header.rerecord_count;
}

void set_rerecord_count(const uint64_t value)
{
    g_header.rerecord_count = static_cast<uint32_t>(value & 0xFFFFFFFF);
    g_header.extended_data.rerecord_count = static_cast<uint32_t>(value >> 32);
}

std::filesystem::path find_accompanying_file_for_movie(std::filesystem::path path, const std::vector<std::wstring>& extensions = {L".st", L".savestate"})
{
    // To allow multiple m64s to reference on st, we construct the st name from the m64 name up to the first point only
    // A.m64 -> A.st
    // A.B.m64 -> A.st, A.B.st
    // A.B.C.m64->A.st, A.B.st, A.B.C.st


    static char drive[260] = {0};
    static char dir[260] = {0};
    static char filename[260] = {0};
    
    memset(drive, 0, std::size(drive));
    memset(dir, 0, std::size(dir));
    memset(filename, 0, std::size(filename));
    
    _splitpath(path.string().c_str(), drive, dir, filename, nullptr);

    size_t i = 0;
    while (true)
    {
        auto result = str_nth_occurence(filename, ".", i + 1);
        std::string matched_filename = "";

        // Standard case, no st sharing
        if (result == std::string::npos)
        {
            matched_filename = filename;
        }
        else
        {
            matched_filename = std::string(filename).substr(0, result);
        }

        std::string st;
        
        for (const auto& ext : extensions)
        {
            // FIXME: Port this function to Unicode so we don't have to wstring_to_string the extension!!!
            st = std::string(drive) + std::string(dir) + matched_filename + wstring_to_string(ext);
            if (std::filesystem::exists(st))
            {
                return st;
            }
        }

        if (result == std::string::npos)
        {
            // Failed the existence checks at all permutations, failed search...
            return "";
        }

        i++;
    }
}

static void set_rom_info(core_vcr_movie_header* header)
{
    header->vis_per_second = core_vr_get_vis_per_second(core_vr_get_rom_header()->Country_code);
    header->controller_flags = 0;
    header->num_controllers = 0;

    for (int32_t i = 0; i < 4; ++i)
    {
        if (g_core->controls[i].Plugin == (int32_t)ce_mempak)
        {
            header->controller_flags |= CONTROLLER_X_MEMPAK(i);
        }

        if (g_core->controls[i].Plugin == (int32_t)ce_rumblepak)
        {
            header->controller_flags |= CONTROLLER_X_RUMBLE(i);
        }

        if (!g_core->controls[i].Present)
            continue;

        header->controller_flags |= CONTROLLER_X_PRESENT(i);
        header->num_controllers++;
    }

    strncpy(header->rom_name, (const char*)ROM_HEADER.nom, 32);
    header->rom_crc1 = ROM_HEADER.CRC1;
    header->rom_country = ROM_HEADER.Country_code;

    header->input_plugin_name[0] = '\0';
    header->video_plugin_name[0] = '\0';
    header->audio_plugin_name[0] = '\0';
    header->rsp_plugin_name[0] = '\0';

    g_core->get_plugin_names(header->video_plugin_name, header->audio_plugin_name, header->input_plugin_name, header->rsp_plugin_name);
}

static core_result read_movie_header(std::vector<uint8_t> buf, core_vcr_movie_header* header)
{
    const core_vcr_movie_header default_hdr{};
    constexpr auto old_header_size = 512;

    if (buf.size() < old_header_size)
        return VCR_InvalidFormat;

    core_vcr_movie_header new_header = {};
    memcpy(&new_header, buf.data(), old_header_size);

    if (new_header.magic != mup_magic)
        return VCR_InvalidFormat;

    if (new_header.version <= 0 || new_header.version > mup_version)
        return VCR_InvalidVersion;

    // The extended version number can't exceed the latest one, obviously...
    if (new_header.extended_version > default_hdr.extended_version)
        return VCR_InvalidExtendedVersion;

    if (new_header.version == 1 || new_header.version == 2)
    {
        // attempt to recover screwed-up plugin data caused by
        // version mishandling and format problems of first versions

#define IS_ALPHA(x) (((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <= 'z') || ((x) == '1'))
        int32_t i;
        for (i = 0; i < 56 + 64; i++)
            if (IS_ALPHA(new_header.reserved_bytes[i])
                && IS_ALPHA(new_header.reserved_bytes[i + 64])
                && IS_ALPHA(new_header.reserved_bytes[i + 64 + 64])
                && IS_ALPHA(new_header.reserved_bytes[i + 64 + 64 + 64]))
                break;
        if (i != 56 + 64)
        {
            memmove(new_header.video_plugin_name, new_header.reserved_bytes + i,
                    256);
        }
        else
        {
            for (i = 0; i < 56 + 64; i++)
                if (IS_ALPHA(new_header.reserved_bytes[i])
                    && IS_ALPHA(new_header.reserved_bytes[i + 64])
                    && IS_ALPHA(new_header.reserved_bytes[i + 64 + 64]))
                    break;
            if (i != 56 + 64)
                memmove(new_header.audio_plugin_name, new_header.reserved_bytes + i,
                        256 - 64);
            else
            {
                for (i = 0; i < 56 + 64; i++)
                    if (IS_ALPHA(new_header.reserved_bytes[i])
                        && IS_ALPHA(new_header.reserved_bytes[i + 64]))
                        break;
                if (i != 56 + 64)
                    memmove(new_header.input_plugin_name,
                            new_header.reserved_bytes + i, 256 - 64 - 64);
                else
                {
                    for (i = 0; i < 56 + 64; i++)
                        if (IS_ALPHA(new_header.reserved_bytes[i]))
                            break;
                    if (i != 56 + 64)
                        memmove(new_header.rsp_plugin_name,
                                new_header.reserved_bytes + i,
                                256 - 64 - 64 - 64);
                    else
                        strncpy(new_header.rsp_plugin_name, "(unknown)", 64);

                    strncpy(new_header.input_plugin_name, "(unknown)", 64);
                }
                strncpy(new_header.audio_plugin_name, "(unknown)", 64);
            }
            strncpy(new_header.video_plugin_name, "(unknown)", 64);
        }
        // attempt to convert old author and description to utf8
        strncpy(new_header.author, new_header.old_author_info, 48);
        strncpy(new_header.description, new_header.old_description, 80);
    }
    if (new_header.version == 3 && buf.size() < sizeof(core_vcr_movie_header))
    {
        return VCR_InvalidFormat;
    }
    if (new_header.version == 3)
    {
        memcpy(new_header.author, buf.data() + 0x222, 222);
        memcpy(new_header.description, buf.data() + 0x300, 256);

        const auto expected_minimum_size = sizeof(core_vcr_movie_header) + sizeof(core_buttons) * new_header.length_samples;
        
        if (buf.size() < expected_minimum_size)
        {
            return VCR_InvalidFormat;
        }
    }
    
    *header = new_header;

    return Res_Ok;
}

core_result core_vcr_parse_header(std::filesystem::path path, core_vcr_movie_header* header)
{
    if (path.extension() != ".m64")
    {
        return VCR_InvalidFormat;
    }

    core_vcr_movie_header new_header = {};
    new_header.rom_country = -1;
    strcpy(new_header.rom_name, "(no ROM)");

    auto buf = read_file_buffer(path);
    if (buf.empty())
    {
        return VCR_BadFile;
    }

    const auto result = read_movie_header(buf, &new_header);
    *header = new_header;

    return result;
}

core_result core_vcr_read_movie_inputs(std::filesystem::path path, std::vector<core_buttons>& inputs)
{
    core_vcr_movie_header header = {};
    const auto result = core_vcr_parse_header(path, &header);
    if (result != Res_Ok)
    {
        return result;
    }

    auto buf = read_file_buffer(path);

    if (buf.size() < sizeof(core_vcr_movie_header) + sizeof(core_buttons) * header.length_samples)
    {
        return VCR_InvalidFormat;
    }

    inputs.resize(header.length_samples);
    memcpy(inputs.data(), buf.data() + sizeof(core_vcr_movie_header), sizeof(core_buttons) * header.length_samples);

    return Res_Ok;
}

bool
vcr_is_playing()
{
    return g_task == task_playback;
}

bool core_vcr_freeze(core_vcr_freeze_info* freeze)
{
	std::scoped_lock lock(vcr_mutex);

    if (core_vcr_get_task() == task_idle)
    {
        return false;
    }

    assert(g_movie_inputs.size() >= g_header.length_samples);

    core_vcr_freeze_info current_freeze = {
        .size = (uint32_t)(sizeof(uint32_t) * 4 + sizeof(core_buttons) * (g_header.length_samples + 1)),
        .uid = g_header.uid,
        .current_sample = (uint32_t)m_current_sample,
        .current_vi = (uint32_t)m_current_vi,
        .length_samples = g_header.length_samples,
    };

    // NOTE: The frozen input buffer is weird: its length is traditionally equal to length_samples + 1, which means the last frame is garbage data
    current_freeze.input_buffer = {};
    current_freeze.input_buffer.resize(g_header.length_samples + 1);
    memcpy(current_freeze.input_buffer.data(), g_movie_inputs.data(), sizeof(core_buttons) * g_header.length_samples);

    // Also probably a good time to flush the movie
    write_movie();

    *freeze = current_freeze;
    
    return true;
}

core_result core_vcr_unfreeze(core_vcr_freeze_info freeze)
{
    std::scoped_lock lock(vcr_mutex);

    // Unfreezing isn't valid during idle state
    if (core_vcr_get_task() == task_idle)
    {
        return VCR_NeedsPlaybackOrRecording;
    }

    if (freeze.size < sizeof(g_header.uid) + sizeof(m_current_sample) + sizeof(m_current_vi) + sizeof(g_header.length_samples))
    {
        return VCR_InvalidFormat;
    }

    const uint32_t space_needed = sizeof(core_buttons) * (freeze.length_samples + 1);

    if (freeze.uid != g_header.uid)
        return VCR_NotFromThisMovie;

    // This means playback desync in read-only mode, but in read-write mode it's fine, as the input buffer will be copied and grown from st.
    if (freeze.current_sample > freeze.length_samples && g_core->cfg->vcr_readonly)
        return VCR_InvalidFrame;

    if (space_needed > freeze.size)
        return VCR_InvalidFormat;

    m_current_sample = (int32_t)freeze.current_sample;
    m_current_vi = (int32_t)freeze.current_vi;

    const core_vcr_task last_task = g_task;

    // When starting playback in RW mode, we don't want overwrite the movie savestate which we're currently unfreezing from...
    const bool is_task_starting_playback = g_task == task_start_playback_from_reset || g_task == task_start_playback_from_snapshot;

    // When unfreezing during a seek while recording, we don't want to overwrite the input buffer.
    // Instead, we'll just update the current sample.
    if (g_task == task_recording && seek_to_frame.has_value())
    {
        g_core->callbacks.current_sample_changed(m_current_sample);
        goto finish;
    }

    if (!g_core->cfg->vcr_readonly && !is_task_starting_playback)
    {
        // here, we are going to take the input data from the savestate
        // and make it the input data for the current movie, then continue
        // writing new input data at the currentFrame pointer
        g_task = task_recording;
        g_core->callbacks.task_changed(g_task);
        g_core->callbacks.current_sample_changed(m_current_sample);
        g_core->callbacks.rerecords_changed(get_rerecord_count());

        // update header with new ROM info
        if (last_task == task_playback)
            set_rom_info(&g_header);

        set_rerecord_count(get_rerecord_count() + 1);
        g_core->cfg->total_rerecords++;
        g_core->callbacks.rerecords_changed(get_rerecord_count());

        if (!g_warp_modify_active)
        {
            g_header.length_samples = freeze.current_sample;

            // Before overwriting the input buffer, save a backup
            if (g_core->cfg->vcr_backups)
            {
                write_backup_impl();
            }

            g_movie_inputs.resize(freeze.current_sample);
            memcpy(g_movie_inputs.data(), freeze.input_buffer.data(), sizeof(core_buttons) * freeze.current_sample);

            write_movie();
        }
    }
    else
    {
        // here, we are going to keep the input data from the movie file
        // and simply rewind to the currentFrame pointer
        // this will cause a desync if the savestate is not in sync
        // with the on-disk recording data, but it's easily solved
        // by loading another savestate or playing the movie from the beginning

        write_movie();
        g_task = task_playback;
        g_core->callbacks.task_changed(g_task);
        g_core->callbacks.current_sample_changed(m_current_sample);
        g_core->callbacks.rerecords_changed(get_rerecord_count());
    }

finish:
    g_core->callbacks.frame();
    g_core->callbacks.unfreeze_completed();

    return Res_Ok;
}

core_result core_vcr_write_backup()
{
    const auto result = write_backup_impl();
    return result ? Res_Ok : VCR_BadFile;
}

void vcr_create_n_frame_savestate(size_t frame)
{
    assert(m_current_sample == frame);

    // OPTIMIZATION: When seeking, we can skip creating seek savestates until near the end where we know they wont be purged
    if (core_vcr_is_seeking())
    {
        const auto frames_from_end_where_savestates_start_appearing = g_core->cfg->seek_savestate_interval * g_core->cfg->seek_savestate_max_count;
        
        std::pair<size_t, size_t> seek_completion{};
        core_vcr_get_seek_completion(seek_completion);
        
        if (seek_completion.second - seek_completion.first > frames_from_end_where_savestates_start_appearing)
        {
            g_core->log_info(L"[VCR] Omitting creation of seek savestate because distance to seek end is big enough");
            return;
        }
    }

    // If our seek savestate map is getting too large, we'll start purging the oldest ones (but not the first one!!!)
    if (g_seek_savestates.size() > g_core->cfg->seek_savestate_max_count)
    {
        for (int32_t i = 1; i < g_header.length_samples; ++i)
        {
            if (g_seek_savestates.contains(i))
            {
                g_core->log_info(std::format(L"[VCR] Map too large! Purging seek savestate at frame {}...", i));
                g_seek_savestates.erase(i);
                g_core->callbacks.seek_savestate_changed((size_t)i);
                break;
            }
        }
    }

    g_core->log_info(std::format(L"[VCR] Creating seek savestate at frame {}...", frame));
    core_st_do_memory({}, core_st_job_save, [frame](core_result result, const auto& buf)
    {
        std::scoped_lock lock(vcr_mutex);

        if (result != Res_Ok)
        {
            g_core->show_dialog(std::format(L"Failed to save seek savestate at frame {}.", frame).c_str(), L"VCR", fsvc_error);
            return;
        }

        g_core->log_info(std::format(L"[VCR] Seek savestate at frame {} of size {} completed", frame, buf.size()));
        g_seek_savestates[frame] = buf;
        g_core->callbacks.seek_savestate_changed((size_t)frame);
    }, false);
}

void vcr_handle_starting_tasks(int32_t index, core_buttons* input)
{
    if (g_task == task_start_recording_from_reset)
    {
        bool clear_eeprom = !(g_header.startFlags & MOVIE_START_FROM_EEPROM);
        g_reset_pending = true;
        g_core->invoke_async([clear_eeprom]
        {
            const auto result = core_vr_reset_rom(clear_eeprom, false, true);

            std::scoped_lock lock(vcr_mutex);
            g_reset_pending = false;

            if (result != Res_Ok)
            {
            	g_core->show_dialog(L"Failed to reset the rom when initiating a from-start recording.\nRecording will be stopped.", L"VCR", fsvc_error);
                core_vcr_stop_all();
                return;
            }

            m_current_sample = 0;
            m_current_vi = 0;
            g_task = task_recording;
            g_core->callbacks.task_changed(g_task);
            g_core->callbacks.current_sample_changed(m_current_sample);
            g_core->callbacks.rerecords_changed(get_rerecord_count());
        }, 0);
    }

    if (g_task == task_start_playback_from_reset)
    {
        bool clear_eeprom = !(g_header.startFlags & MOVIE_START_FROM_EEPROM);
        g_reset_pending = true;
        g_core->invoke_async([clear_eeprom]
        {
            const auto result = core_vr_reset_rom(clear_eeprom, false, true);

            std::scoped_lock lock(vcr_mutex);
            g_reset_pending = false;

            if (result != Res_Ok)
            {
            	g_core->show_dialog(L"Failed to reset the rom when playing back a from-start movie.\nPlayback will be stopped.", L"VCR", fsvc_error);
                core_vcr_stop_all();
                return;
            }

            m_current_sample = 0;
            m_current_vi = 0;
            g_task = task_playback;
            g_core->callbacks.task_changed(g_task);
            g_core->callbacks.current_sample_changed(m_current_sample);
            g_core->callbacks.rerecords_changed(get_rerecord_count());
        }, 0);
    }
}

void vcr_handle_recording(int32_t index, core_buttons* input)
{
    if (g_task != task_recording)
    {
        return;
    }

    // When the movie has more frames after the current one than the buffer has, we need to use the buffer data instead of the plugin
    const auto effective_index = m_current_sample + index;
    bool use_inputs_from_buffer = g_movie_inputs.size() > effective_index || g_warp_modify_active;

    // Regular recording: the recording input source is the input plugin (along with the reset override)
    if (vcr_reset_requested)
    {
        *input = {
            .reserved_1 = 1,
            .reserved_2 = 1,
        };
    }
    else
    {
        if (use_inputs_from_buffer)
        {
            *input = g_movie_inputs[effective_index];

            const auto prev_input = *input;
            // NOTE: We want to notify Lua of inputs but won't actually accept the new values
            g_core->callbacks.input(input, index);
            *input = prev_input;
        }
        else
        {
            g_core->plugin_funcs.get_keys(index, input);
            g_core->callbacks.input(input, index);
        }
    }

    if (!use_inputs_from_buffer)
    {
        g_movie_inputs.push_back(*input);
        g_header.length_samples++;
    }

    m_current_sample++;
    g_core->callbacks.current_sample_changed(m_current_sample);

    if (vcr_reset_requested)
    {
        vcr_reset_requested = false;
        g_reset_pending = true;
        g_core->invoke_async([]
        {
            core_result result;
            {
                std::lock_guard lock(g_emu_cs);
                result = vr_reset_rom_impl(false, false, true);
            }
            
            std::scoped_lock lock(vcr_mutex);

            g_reset_pending = false;

            if (result != Res_Ok)
            {
                g_core->show_dialog(L"Failed to reset the rom following a user-invoked reset.", L"VCR", fsvc_error);
            }
        }, 0);
    }
}

void vcr_handle_playback(int32_t index, core_buttons* input)
{
    if (g_task != task_playback)
    {
        return;
    }

    // This if previously also checked for if the VI is over the amount specified in the header,
    // but that can cause movies to end playback early on laggy plugins.
    if (m_current_sample >= (int32_t)g_header.length_samples)
    {
        core_vcr_stop_all();

        if (g_core->cfg->is_movie_loop_enabled)
        {
            core_vcr_start_playback(g_movie_path);
            g_core->callbacks.loop_movie();
            return;
        }

        g_core->plugin_funcs.set_keys(index, {0});
        g_core->plugin_funcs.get_keys(index, input);
        return;
    }

    if (!(g_header.controller_flags & CONTROLLER_X_PRESENT(index)))
    {
        // disconnected controls are forced to have no input during playback
        *input = {0};
        return;
    }

    // Use inputs from movie, also notify input plugin of override via setKeys
    *input = g_movie_inputs[m_current_sample + index];
    g_core->plugin_funcs.set_keys(index, *input);

    //no readable code because 120 star tas can't get this right >:(
    if (input->value == 0xC000)
    {
        g_reset_pending = true;
        g_core->log_info(L"[VCR] Resetting during playback...");
        g_core->invoke_async([]
        {
            auto result = core_vr_reset_rom(false, false, true);

            std::scoped_lock lock(vcr_mutex);

            if (result != Res_Ok)
            {
            	g_core->show_dialog(L"Failed to reset the rom following a movie-invoked reset.\nRecording will be stopped.", L"VCR", fsvc_error);
                core_vcr_stop_all();
                g_reset_pending = false;
                return;
            }

            g_reset_pending = false;
        }, 0);
    }

    g_core->callbacks.input(input, index);
    m_current_sample++;
    g_core->callbacks.current_sample_changed(m_current_sample);
}

void vcr_stop_seek_if_needed()
{
    if (!seek_to_frame.has_value())
    {
        return;
    }

    assert(g_task != task_idle);

    //g_core->log_info(L"[VCR] Seeking... ({}/{})", m_current_sample, seek_to_frame.value());

    if (m_current_sample > seek_to_frame.value())
    {
        g_core->log_error(std::format(L"Seek frame exceeded without seek having been stopped. ({}/{})", m_current_sample, seek_to_frame.value()));
    	g_core->show_dialog(L"Seek frame exceeded without seek having been stopped!\nThis incident has been logged, please report this issue along with the log file.", L"VCR", fsvc_error);
    }

    if (m_current_sample >= seek_to_frame.value())
    {
        g_core->log_info(std::format(L"[VCR] Seek finished at frame {} (target: {})", m_current_sample, seek_to_frame.value()));
        core_vcr_stop_seek();
        if (g_seek_pause_at_end)
        {
            core_vr_pause_emu();
        }
    }
}

bool vcr_allows_core_pause()
{
    if (!core_vcr_is_seeking())
    {
        return true;
    }
    return m_current_sample == seek_to_frame.value() - 1 && g_seek_pause_at_end;
}

void vcr_create_seek_savestates()
{
    if (g_task == task_idle || g_core->cfg->seek_savestate_interval == 0)
    {
        return;
    }

    if (m_current_sample % g_core->cfg->seek_savestate_interval == 0)
    {
        vcr_create_n_frame_savestate(m_current_sample);
    }
}

void vcr_on_controller_poll(int32_t index, core_buttons* input)
{
    // NOTE: We mutate m_task and send task change messages in here, so we need to acquire the lock (what if playback start thread decides to beat us up midway through input poll? right...)
    std::scoped_lock lock(vcr_mutex);

    // NOTE: When we call reset_rom from another thread, we only request a reset to happen in the future.
    // Until the reset, the emu thread keeps running and potentially generating many frames.
    // Those frames are invalid to us, because from the movie's perspective, it should be instantaneous.
    if (g_reset_pending)
    {
        g_core->log_info(L"[VCR] Skipping pre-reset frame");
        return;
    }

    // Frames between seek savestate load request and actual load are invalid for the same reason as pre-reset frames.
    if (g_seek_savestate_loading)
    {
        g_core->log_info(L"[VCR] Skipping pre-seek savestate load frame");
        return;
    }

    if (g_task == task_idle)
    {
        g_core->plugin_funcs.get_keys(index, input);
        g_core->callbacks.input(input, index);
        return;
    }

    vcr_stop_seek_if_needed();

    // We need to handle start tasks first, as logic after it depends on the task being modified
    vcr_handle_starting_tasks(index, input);

    vcr_create_seek_savestates();

    vcr_handle_recording(index, input);

    vcr_handle_playback(index, input);
}

// Generates a savestate path for a newly created movie.
// Consists of the movie path, but with the stem trimmed at the first dot and with .st extension
std::filesystem::path get_savestate_path_for_new_movie(std::filesystem::path path)
{
    auto result = str_nth_occurence(path.stem().string(), ".", 1);

    // Standard case, no st shortcutting
    if (result == std::string::npos)
    {
        path.replace_extension(".st");
        return path;
    }

    char drive[260]{};
    char dir[260]{};
    _splitpath(path.string().c_str(), drive, dir, nullptr, nullptr);

    auto stem = path.stem().string().substr(0, result);

    return std::string(drive) + std::string(dir) + stem + ".st";
}

core_result core_vcr_start_record(std::filesystem::path path, uint16_t flags, std::string author,
                             std::string description)
{
    std::unique_lock lock(vcr_mutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        g_core->log_info(L"[VCR] vcr_start_record busy!");
        return VCR_Busy;
    }
    
    if (flags != MOVIE_START_FROM_SNAPSHOT
        && flags != MOVIE_START_FROM_NOTHING
        && flags != MOVIE_START_FROM_EEPROM
        && flags != MOVIE_START_FROM_EXISTING_SNAPSHOT)
    {
        return VCR_InvalidStartType;
    }

    if (author.empty())
    {
        author = "(no author)";
    }

    if (description.empty())
    {
        description = "(no description)";
    }

    core_vcr_stop_all();
    g_movie_path = path;

    for (auto& [Present, RawData, Plugin] : g_core->controls)
    {
        if (Present && RawData)
        {
            bool proceed = g_core->show_ask_dialog(RAWDATA_WARNING_MESSAGE, L"VCR", true);
            if (!proceed)
            {
                return VCR_Cancelled;
            }
            break;
        }
    }

    // FIXME: Do we want to reset this every time?
    g_core->cfg->vcr_readonly = 0;
    g_core->callbacks.readonly_changed((bool)g_core->cfg->vcr_readonly);

    const core_vcr_movie_header default_hdr{};
    memset(&g_header, 0, sizeof(core_vcr_movie_header));
    g_movie_inputs = {};

    g_header.magic = mup_magic;
    g_header.version = mup_version;

    g_header.extended_version = default_hdr.extended_version;
    g_header.extended_flags.wii_vc = g_core->cfg->wii_vc_emulation;
    g_header.extended_data = default_hdr.extended_data;

    g_header.uid = (uint32_t)time(nullptr);
    g_header.length_vis = 0;
    g_header.length_samples = 0;

    set_rerecord_count(0);
    g_header.startFlags = flags;


    if (flags & MOVIE_START_FROM_SNAPSHOT)
    {
        // save state
        g_core->log_info(L"[VCR] Saving state...");
        g_task = task_start_recording_from_snapshot;
        core_st_do_file(get_savestate_path_for_new_movie(g_movie_path), core_st_job_save, [](core_result result, auto)
        {
            std::scoped_lock lock(vcr_mutex);

            if (result != Res_Ok)
            {
            	g_core->show_dialog(L"Failed to save savestate while starting recording.\nRecording will be stopped.", L"VCR", fsvc_error);
                core_vcr_stop_all();
                return;
            }

            g_core->log_info(L"[VCR] Starting recording from snapshot...");
            g_task = task_recording;
            // FIXME: Doesn't this need a message broadcast?
            // TODO: Also, what about clearing the input on first frame
        }, true);
    }
    else if (flags & MOVIE_START_FROM_EXISTING_SNAPSHOT)
    {
        // TODO: Verify that this flag still works after st task rewrite

        g_core->log_info(L"[VCR] Loading state...");
        auto st_path = find_accompanying_file_for_movie(g_movie_path);
        if (st_path.empty())
        {
            return VCR_InvalidSavestate;
        }

        // set this to the normal snapshot flag to maintain compatibility
        g_header.startFlags = MOVIE_START_FROM_SNAPSHOT;
        g_task = task_start_recording_from_existing_snapshot;

        g_core->invoke_async([=]
        {
            core_st_do_file(st_path, core_st_job_load, [](core_result result, auto)
            {
                std::scoped_lock lock(vcr_mutex);

                if (result != Res_Ok)
                {
                	g_core->show_dialog(L"Failed to load savestate while starting recording.\nRecording will be stopped.", L"VCR", fsvc_error);
                    core_vcr_stop_all();
                    return;
                }

                g_core->log_info(L"[VCR] Starting recording from existing snapshot...");
                g_task = task_recording;
                // FIXME: Doesn't this need a message broadcast?
                // TODO: Also, what about clearing the input on first frame
            }, true);
        }, 0);
    }
    else
    {
        g_task = task_start_recording_from_reset;
    }

    set_rom_info(&g_header);

    memset(g_header.author, 0, sizeof(core_vcr_movie_header::author));
    if (author.size() > sizeof(core_vcr_movie_header::author))
    {
        author.resize(sizeof(core_vcr_movie_header::author));
    }
    strncpy(g_header.author, author.data(), author.size());

    memset(g_header.description, 0, sizeof(core_vcr_movie_header::description));
    if (description.size() > sizeof(core_vcr_movie_header::description))
    {
        description.resize(sizeof(core_vcr_movie_header::description));
    }
    strncpy(g_header.description, description.data(), description.size());

    m_current_sample = 0;
    m_current_vi = 0;

    g_core->callbacks.task_changed(g_task);
    g_core->callbacks.current_sample_changed(m_current_sample);
    g_core->callbacks.rerecords_changed(get_rerecord_count());
    return Res_Ok;
}

core_result core_vcr_replace_author_info(const std::filesystem::path& path, const std::string& author, const std::string& description)
{
    // We don't want to fopen with rb+ as it changes the last modified date, unless the author info actually needs to change, so
    // we skip that step if the values remain identical

    // 1. Read movie header
    const auto buf = read_file_buffer(path);

    if (buf.empty())
    {
        return VCR_BadFile;
    }

    core_vcr_movie_header hdr{};
    auto result = read_movie_header(buf, &hdr);

    if (result != Res_Ok)
    {
        return result;
    }

    // 2. Compare author and description fields, and int16_t-circuit if they remained identical
    if (!strcmp(hdr.author, author.c_str()) && !strcmp(hdr.description, description.c_str()))
    {
        g_core->log_info(L"[VCR] Movie author or description didn't change, returning early...");
        return Res_Ok;
    }

    FILE* f = fopen(path.string().c_str(), "rb+");
    if (!f)
    {
        return VCR_BadFile;
    }

    if (author.size() > 222 || description.size() > 256)
    {
        return VCR_InvalidFormat;
    }

    fseek(f, 0x222, SEEK_SET);
    for (int32_t i = 0; i < 222; ++i)
    {
        fputc(0, f);
    }
    fseek(f, 0x222, SEEK_SET);
    fwrite(author.data(), 1, author.size(), f);

    fseek(f, 0x300, SEEK_SET);
    for (int32_t i = 0; i < 256; ++i)
    {
        fputc(0, f);
    }
    fseek(f, 0x300, SEEK_SET);
    fwrite(description.data(), 1, description.size(), f);

    fflush(f);
    fclose(f);
    return Res_Ok;
}

void core_vcr_get_seek_completion(std::pair<size_t, size_t>& pair)
{
    if (!core_vcr_is_seeking())
    {
        pair = std::make_pair(m_current_sample, SIZE_MAX);
        return;
    }
    
    pair = std::make_pair(m_current_sample, seek_to_frame.value());
}

core_result vcr_stop_record()
{
    std::unique_lock lock(vcr_mutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        g_core->log_info(L"[VCR] vcr_stop_record busy!");
        return VCR_Busy;
    }

    if (!vcr_is_task_recording(g_task))
    {
        return Res_Ok;
    }

    if (g_task == task_start_recording_from_reset)
    {
        g_task = task_idle;
        g_core->log_info(L"[VCR] Removing files (nothing recorded)");
        _unlink(std::filesystem::path(g_movie_path).replace_extension(".m64").string().c_str());
        _unlink(std::filesystem::path(g_movie_path).replace_extension(".st").string().c_str());
    }

    if (g_task == task_recording)
    {
        write_movie();

        g_task = task_idle;

        g_core->log_info(std::format(L"[VCR] Recording stopped. Recorded %ld input samples", g_header.length_samples));
    }

    g_core->callbacks.task_changed(g_task);
    return Res_Ok;
}

bool show_controller_warning()
{
    for (int32_t i = 0; i < 4; ++i)
    {
        if (!g_core->controls[i].Present && g_header.controller_flags & CONTROLLER_X_PRESENT(i))
        {
            g_core->show_dialog(std::format(CONTROLLER_OFF_ON_MISMATCH, i + 1).c_str(), L"VCR", fsvc_error);
            return false;
        }
        if (g_core->controls[i].Present && !(g_header.controller_flags & CONTROLLER_X_PRESENT(i)))
        {
            g_core->show_dialog(std::format(CONTROLLER_ON_OFF_MISMATCH, i + 1).c_str(), L"VCR", fsvc_warning);
        }
        else
        {
            if (g_core->controls[i].Present && (g_core->controls[i].Plugin != (int32_t)ce_mempak) && g_header.controller_flags & CONTROLLER_X_MEMPAK(i))
            {
                g_core->show_dialog(std::format(CONTROLLER_MEMPAK_MISMATCH, i + 1).c_str(), L"VCR", fsvc_warning);
            }
            if (g_core->controls[i].Present && (g_core->controls[i].Plugin != (int32_t)ce_rumblepak) && g_header.controller_flags & CONTROLLER_X_RUMBLE(i))
            {
                g_core->show_dialog(std::format(CONTROLLER_RUMBLEPAK_MISMATCH, i + 1).c_str(), L"VCR", fsvc_warning);
            }
            if (g_core->controls[i].Present && (g_core->controls[i].Plugin != (int32_t)ce_none) && !(g_header.controller_flags & (CONTROLLER_X_MEMPAK(i) | CONTROLLER_X_RUMBLE(i))))
            {
                g_core->show_dialog(std::format(CONTROLLER_MEMPAK_RUMBLEPAK_MISMATCH, i + 1).c_str(), L"VCR", fsvc_warning);
            }
        }
    }
    return true;
}

core_result core_vcr_start_playback(std::filesystem::path path)
{
    std::unique_lock lock(vcr_mutex);

    auto movie_buf = read_file_buffer(path);

    if (movie_buf.empty())
    {
        return VCR_BadFile;
    }

    if (!core_executing)
    {
        // NOTE: We need to unlock the VCR mutex during the vr_start_rom call, as it needs the core to continue execution in order to exit.
        // If we kept the lock, the core would become permanently stuck waiting for it to be released in on_controller_poll.
        lock.unlock();

        const auto result = core_vr_start_rom(path, true);

        if (result != Res_Ok)
        {
            return result;
        }

        lock.lock();
    }

    // We can't call this after opening m_file, since it will potentially nuke it
    core_vcr_stop_all();

    const auto result = read_movie_header(movie_buf, &g_header);
    if (result != Res_Ok)
    {
        return result;
    }

    g_movie_inputs = {};
    g_movie_inputs.resize(g_header.length_samples);
    memcpy(g_movie_inputs.data(), movie_buf.data() + sizeof(core_vcr_movie_header), sizeof(core_buttons) * g_header.length_samples);

    for (auto& [Present, RawData, Plugin] : g_core->controls)
    {
        if (!Present || !RawData)
            continue;

        bool proceed = g_core->show_ask_dialog(RAWDATA_WARNING_MESSAGE, L"VCR", true);
        if (!proceed)
        {
            return VCR_Cancelled;
        }

        break;
    }
    
    if (!show_controller_warning())
    {
        return VCR_InvalidControllers;
    }

    if (g_header.extended_version != 0)
    {
        g_core->log_info(std::format(L"[VCR] Movie has extended version {}", g_header.extended_version));

        if (g_core->cfg->wii_vc_emulation != g_header.extended_flags.wii_vc)
        {
            bool proceed = g_core->show_ask_dialog(g_header.extended_flags.wii_vc ? WII_VC_MISMATCH_A_WARNING_MESSAGE : WII_VC_MISMATCH_B_WARNING_MESSAGE, L"VCR", true);

            if (!proceed)
            {
                return VCR_Cancelled;
            }
        }
    }
    else
    {
        // Old movies filled with non-zero data in this section are suspicious, we'll warn the user.
        if (g_header.extended_flags.data != 0)
        {
        	g_core->show_dialog(OLD_MOVIE_EXTENDED_SECTION_NONZERO_MESSAGE, L"VCR", fsvc_warning);
        }
    }

    if (_stricmp(g_header.rom_name,
                 (const char*)ROM_HEADER.nom) != 0)
    {
        bool proceed = g_core->show_ask_dialog(std::format(ROM_NAME_WARNING_MESSAGE, string_to_wstring(g_header.rom_name), string_to_wstring((char*)ROM_HEADER.nom)).c_str(), L"VCR", true);

        if (!proceed)
        {
            return VCR_Cancelled;
        }
    }
    else
    {
        if (g_header.rom_country != ROM_HEADER.
            Country_code)
        {
            bool proceed = g_core->show_ask_dialog(std::format(ROM_COUNTRY_WARNING_MESSAGE, g_header.rom_country, ROM_HEADER.Country_code).c_str(), L"VCR", true);
            if (!proceed)
            {
                return VCR_Cancelled;
            }
        }
        else if (g_header.rom_crc1 != ROM_HEADER.
            CRC1)
        {
            wchar_t str[512] = {0};
            swprintf_s(str, ROM_CRC_WARNING_MESSAGE, g_header.rom_crc1, ROM_HEADER.CRC1);

            bool proceed = g_core->show_ask_dialog(str, L"VCR", true);
            if (!proceed)
            {
                return VCR_Cancelled;
            }
        }
    }

    g_movie_has_cheat = false;
    const auto cht_path = find_accompanying_file_for_movie(path, {L".cht"});

    if (!cht_path.empty())
    {
        std::vector<core_cheat> cheats;
        if (cht_read_from_file(cht_path, cheats))
        {
            g_movie_has_cheat = true;
            cht_layer_push(cheats);
        }
        else
        {
            const auto proceed = g_core->show_ask_dialog(CHEAT_ERROR_ASK_MESSAGE, L"VCR", true);

            if (!proceed)
            {
                return VCR_Cancelled;
            }
        }
    }
    
    // Reset VCR-related state
    m_current_sample = 0;
    m_current_vi = 0;
    g_movie_path = path;

    if (g_header.startFlags & MOVIE_START_FROM_SNAPSHOT)
    {
        g_core->log_info(L"[VCR] Loading state...");

        // Load appropriate state for movie
        auto st_path = find_accompanying_file_for_movie(g_movie_path);

        if (st_path.empty())
        {
            return VCR_InvalidSavestate;
        }

        g_task = task_start_playback_from_snapshot;

        g_core->invoke_async([=]
        {
            core_st_do_file(st_path, core_st_job_load, [](const core_result result, auto)
            {
                std::scoped_lock lock(vcr_mutex);

                if (result != Res_Ok)
                {
                	g_core->show_dialog(L"Failed to load savestate while starting playback.\nRecording will be stopped.", L"VCR", fsvc_error);
                    core_vcr_stop_all();
                    return;
                }

                g_core->log_info(L"[VCR] Starting playback from snapshot...");
                g_task = task_playback;
                g_core->callbacks.task_changed(g_task);
                g_core->callbacks.current_sample_changed(m_current_sample);
                g_core->callbacks.rerecords_changed(get_rerecord_count());
            }, true);
        }, 0);
    }
    else
    {
        g_task = task_start_playback_from_reset;
    }

    g_core->callbacks.task_changed(g_task);
    g_core->callbacks.current_sample_changed(m_current_sample);
    g_core->callbacks.rerecords_changed(get_rerecord_count());

    // FIXME: Move this into the actual starting sections, and document it :p
    g_core->callbacks.play_movie();

    return Res_Ok;
}

bool can_seek_to(size_t frame)
{
    return frame <= g_header.length_samples
        && frame > 0;
}

size_t compute_sample_from_seek_string(const std::wstring& str)
{
    try
    {
        if (str[0] == '-' || str[0] == '+')
        {
            return m_current_sample + std::stoi(str);
        }

        if (str[0] == '^')
        {
            return g_header.length_samples - std::stoi(str.substr(1));
        }

        return std::stoi(str);
    }
    catch (...)
    {
        return SIZE_MAX;
    }
}

size_t vcr_find_closest_savestate_before_frame(size_t frame)
{
    int32_t lowest_distance = INT32_MAX;
    size_t lowest_distance_frame = 0;
    for (const auto& [slot_frame, _] : g_seek_savestates)
    {
        // Current and future sts are invalid for rewinding
        if (slot_frame >= frame)
        {
            continue;
        }

        auto distance = frame - slot_frame;

        if (distance < lowest_distance)
        {
            lowest_distance = distance;
            lowest_distance_frame = slot_frame;
        }
    }

    return lowest_distance_frame;
}

core_result vcr_begin_seek_impl(std::wstring str, bool pause_at_end, bool resume, bool warp_modify)
{
    std::scoped_lock lock(vcr_mutex);

    if (seek_to_frame.has_value())
    {
        return VCR_SeekAlreadyRunning;
    }

    if (g_seek_savestate_loading)
    {
        return VCR_SeekAlreadyRunning;
    }

    auto frame = compute_sample_from_seek_string(str);

    if (frame == SIZE_MAX || !can_seek_to(frame))
    {
        return VCR_InvalidFrame;
    }

    // We need to adjust the end frame if we're pausing at the end... lol
    if (pause_at_end)
    {
        frame--;

        if (!can_seek_to(frame))
        {
            return VCR_InvalidFrame;
        }
    }

    seek_to_frame = std::make_optional(frame);
    g_seek_pause_at_end = pause_at_end;
    g_core->callbacks.seek_status_changed();

    if (!warp_modify && pause_at_end && m_current_sample == frame + 1)
    {
        g_core->log_trace(std::format(L"[VCR] Early-stopping seek: already at frame {}.", frame));
        core_vcr_stop_seek();
        return Res_Ok;
    }

    if (resume)
    {
        core_vr_resume_emu();
    }

    // We need to backtrack somehow if we're ahead of the frame
    if (m_current_sample <= frame)
    {
        return Res_Ok;
    }

    if (g_task == task_playback)
    {
        // Fast path: use seek savestates
        // FIXME: Duplicated code, a bit ugly
        if (g_core->cfg->seek_savestate_interval != 0)
        {
            g_core->log_trace(L"[VCR] vcr_begin_seek_impl: playback, fast path");

            // FIXME: Might be better to have read-only as an individual flag for each savestate, cause as it is now, we're overwriting global state for  this...
            g_core->cfg->vcr_readonly = true;
            g_core->callbacks.readonly_changed((bool)g_core->cfg->vcr_readonly);

            const auto closest_key = vcr_find_closest_savestate_before_frame(frame);

            g_core->log_info(std::format(L"[VCR] Seeking during playback to frame {}, loading closest savestate at {}...", frame, closest_key));
            g_seek_savestate_loading = true;

            // NOTE: This needs to go through AsyncExecutor (despite us already being on a worker thread) or it will cause a deadlock.
            g_core->invoke_async([=]
            {
                core_st_do_memory(g_seek_savestates[closest_key], core_st_job_load, [=](core_result result, auto buf)
                {
                    if (result != Res_Ok)
                    {
                    	g_core->show_dialog(L"Failed to load seek savestate for seek operation.", L"VCR", fsvc_error);
                        g_seek_savestate_loading = false;
                        core_vcr_stop_seek();
                    }

                    g_core->log_info(std::format(L"[VCR] Seek savestate at frame {} loaded!", closest_key));
                    g_seek_savestate_loading = false;
                }, false);
            }, 0);

            return Res_Ok;
        }

        g_core->log_trace(L"[VCR] vcr_begin_seek_impl: playback, slow path");

        const auto result = core_vcr_start_playback(g_movie_path);
        if (result != Res_Ok)
        {
            g_core->log_error(std::format(L"[VCR] vcr_begin_seek_impl: core_vcr_start_playback failed with error code {}", static_cast<int32_t>(result)));
            seek_to_frame.reset();
            g_core->callbacks.seek_status_changed();
            return result;
        }
        return Res_Ok;
    }

    if (g_task == task_recording)
    {
        if (g_core->cfg->seek_savestate_interval == 0)
        {
            // TODO: We can't backtrack using savestates, so we'd have to restart into recording mode while restoring the buffer, leave it for the next release...
        	g_core->show_dialog(L"The seek savestate interval can't be 0 when seeking backwards during recording.", L"VCR", fsvc_error);
            return VCR_SeekSavestateIntervalZero;
        }

        const auto target_sample = warp_modify ? g_warp_modify_first_difference_frame : frame;

        // All seek savestates after the target frame need to be purged, as the user will invalidate them by overwriting inputs prior to them
        if (!g_core->cfg->vcr_readonly)
        {
            std::vector<size_t> to_erase;
            for (const auto& [sample, _] : g_seek_savestates)
            {
                if (sample >= target_sample)
                {
                    to_erase.push_back(sample);
                }
            }
            for (const auto sample : to_erase)
            {
                g_core->log_info(std::format(L"[VCR] Erasing now-invalidated seek savestate at frame {}...", sample));
                g_seek_savestates.erase(sample);
                g_core->callbacks.seek_savestate_changed((size_t)sample);
            }
        }

        const auto closest_key = vcr_find_closest_savestate_before_frame(target_sample);

        g_core->log_info(std::format(L"[VCR] Seeking backwards during recording to frame {}, loading closest savestate at {}...", target_sample, closest_key));
        g_seek_savestate_loading = true;

        // NOTE: This needs to go through AsyncExecutor (despite us already being on a worker thread) or it will cause a deadlock.
        g_core->invoke_async([=]
        {
            core_st_do_memory(g_seek_savestates[closest_key], core_st_job_load, [=](core_result result, auto buf)
            {
                if (result != Res_Ok)
                {
                	g_core->show_dialog(L"Failed to load seek savestate for seek operation.", L"VCR", fsvc_error);
                    g_seek_savestate_loading = false;
                    core_vcr_stop_seek();
                }

                g_core->log_info(std::format(L"[VCR] Seek savestate at frame {} loaded!", closest_key));
                g_seek_savestate_loading = false;
            }, false);
        }, 0);

        return Res_Ok;
    }

    return Res_Ok;
}

core_result core_vcr_begin_seek(std::wstring str, bool pause_at_end)
{
    return vcr_begin_seek_impl(str, pause_at_end, true, false);
}

core_result core_vcr_convert_freeze_buffer_to_movie(const core_vcr_freeze_info& freeze, core_vcr_movie_header& header, std::vector<core_buttons>& inputs)
{
    header.magic = mup_magic;
    header.version = mup_version;
    header.uid = freeze.uid;
    header.length_samples = freeze.length_samples;
    header.startFlags = MOVIE_START_FROM_NOTHING;
    header.length_vis = UINT32_MAX;
    set_rom_info(&header);
    inputs = freeze.input_buffer;
    return Res_Ok;
}

void core_vcr_stop_seek()
{
    // We need to acquire the mutex here, as this function is also called during input poll
    // and having two of these running at the same time is bad for obvious reasons
    std::scoped_lock lock(vcr_mutex);

    if (!seek_to_frame.has_value())
    {
        g_core->log_info(L"[VCR] Tried to call stop_seek with no seek operation running");
        return;
    }

    seek_to_frame.reset();
    g_core->callbacks.seek_status_changed();
    g_core->callbacks.seek_completed();

    if (g_warp_modify_active)
    {
        g_warp_modify_active = false;
        g_core->callbacks.warp_modify_status_changed(g_warp_modify_active);
    }
}

bool core_vcr_is_seeking()
{
    return seek_to_frame.has_value();
}

core_result vcr_stop_playback()
{
    std::unique_lock lock(vcr_mutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        g_core->log_info(L"[VCR] vcr_stop_playback busy!");
        return VCR_Busy;
    }

    if (!is_task_playback(g_task))
        return Res_Ok;

    g_task = task_idle;
    g_core->callbacks.task_changed(g_task);
    g_core->callbacks.stop_movie();

    if (g_movie_has_cheat)
    {
        cht_layer_pop();
    }
    
    return Res_Ok;
}

bool vcr_is_task_recording(core_vcr_task task)
{
    return task == task_recording || task == task_start_recording_from_reset || task == task_start_recording_from_existing_snapshot || task ==
        task_start_recording_from_snapshot;
}

static void vcr_clear_seek_savestates()
{
	std::scoped_lock lock(vcr_mutex);

	g_core->log_info(L"[VCR] Clearing seek savestates...");

	std::vector<size_t> prev_seek_savestate_keys;
    prev_seek_savestate_keys.reserve(g_seek_savestates.size());
    for (const auto& [key, _] : g_seek_savestates)
    {
        prev_seek_savestate_keys.push_back(key);
    }

    g_seek_savestates.clear();

    for (const auto frame : prev_seek_savestate_keys)
    {
        g_core->callbacks.seek_savestate_changed(frame);
    }
}

static void setkeys_with_zero()
{
    core_buttons zero = {0};
    for (int i = 0; i < 4; i++)
    {
        g_core->plugin_funcs.set_keys(i, zero);
    }
}

core_result core_vcr_stop_all()
{
	vcr_clear_seek_savestates();

    switch (g_task)
    {
    case task_start_recording_from_reset:
    case task_start_recording_from_snapshot:
    case task_recording:
        setkeys_with_zero();
        return vcr_stop_record();
    case task_start_playback_from_reset:
    case task_start_playback_from_snapshot:
    case task_playback:
        setkeys_with_zero();
        return vcr_stop_playback();
    default:
        return Res_Ok;
    }
}

std::filesystem::path core_vcr_get_path()
{
    return g_movie_path;
}

core_vcr_task core_vcr_get_task()
{
    return g_task;
}

uint32_t core_vcr_get_length_samples()
{
	return core_vcr_get_task() == task_idle ? UINT32_MAX : g_header.length_samples;
}

uint32_t core_vcr_get_length_vis()
{
	return core_vcr_get_task() == task_idle ? UINT32_MAX : g_header.length_vis;
}

int32_t core_vcr_get_current_vi()
{
	return core_vcr_get_task() == task_idle ? -1 : m_current_vi;
}

std::vector<core_buttons> core_vcr_get_inputs()
{
    // FIXME: This isn't thread-safe.
    return g_movie_inputs;
}

/// Finds the first input difference between two input vectors. Returns SIZE_MAX if they are identical.
size_t vcr_find_first_input_difference(const std::vector<core_buttons>& first, const std::vector<core_buttons>& second)
{
    if (first.size() != second.size())
    {
        const auto min_size = std::min(first.size(), second.size());
        for (int32_t i = 0; i < min_size; ++i)
        {
            if (first[i].value != second[i].value)
            {
                return i;
            }
        }
        return std::max(0, (int32_t)min_size - 1);
    }
    else
    {
        for (int32_t i = 0; i < first.size(); ++i)
        {
            if (first[i].value != second[i].value)
            {
                return i;
            }
        }
        return SIZE_MAX;
    }
}

core_result core_vcr_begin_warp_modify(const std::vector<core_buttons>& inputs)
{
    std::scoped_lock lock(vcr_mutex);

    if (g_warp_modify_active)
    {
        return VCR_WarpModifyAlreadyRunning;
    }

    if (g_task != task_recording)
    {
        return VCR_WarpModifyNeedsRecordingTask;
    }

    if (inputs.empty())
    {
        return VCR_WarpModifyEmptyInputBuffer;
    }

    g_warp_modify_first_difference_frame = vcr_find_first_input_difference(g_movie_inputs, inputs);

    if (g_warp_modify_first_difference_frame == SIZE_MAX)
    {
        g_core->log_info(L"[VCR] Warp modify inputs are identical to current input buffer, doing nothing...");

        g_warp_modify_active = true;
        g_core->callbacks.warp_modify_status_changed(g_warp_modify_active);

        g_warp_modify_active = false;
        g_core->callbacks.warp_modify_status_changed(g_warp_modify_active);

        return Res_Ok;
    }

    if (g_warp_modify_first_difference_frame > m_current_sample)
    {
        g_core->log_info(std::format(L"[VCR] First different frame is in the future (current sample: {}, first differenece: {}), copying inputs with no seek...", m_current_sample, g_warp_modify_first_difference_frame));

        g_movie_inputs = inputs;
        g_header.length_samples = g_movie_inputs.size();

        g_warp_modify_active = true;
        g_core->callbacks.warp_modify_status_changed(g_warp_modify_active);

        g_warp_modify_active = false;
        g_core->callbacks.warp_modify_status_changed(g_warp_modify_active);

        return Res_Ok;
    }

    const auto target_sample = std::min(inputs.size(), (size_t)m_current_sample);

    const auto result = vcr_begin_seek_impl(std::to_wstring(target_sample), emu_paused || frame_advance_outstanding, false, true);

    if (result != Res_Ok)
    {
        return result;
    }

    g_warp_modify_active = true;

    g_movie_inputs = inputs;
    g_header.length_samples = g_movie_inputs.size();
    g_core->log_info(std::format(L"[VCR] Warp modify started at frame {}", m_current_sample));
    g_core->callbacks.warp_modify_status_changed(g_warp_modify_active);

    core_vr_resume_emu();

    return Res_Ok;
}

bool core_vcr_get_warp_modify_status()
{
    return g_warp_modify_active;
}

size_t core_vcr_get_warp_modify_first_difference_frame()
{
	return core_vcr_get_warp_modify_status() == g_warp_modify_active ? g_warp_modify_first_difference_frame : SIZE_MAX;
}

void core_vcr_get_seek_savestate_frames(std::unordered_map<size_t, bool>& map)
{
    map.clear();

    for (const auto& [key, _] : g_seek_savestates)
    {
        map[key] = true;
    }
}

bool core_vcr_has_seek_savestate_at_frame(const size_t frame)
{
    return g_seek_savestates.contains(frame);
}

void vcr_on_vi()
{
    m_current_vi++;

    if (core_vcr_get_task() == task_recording && !g_warp_modify_active)
        g_header.length_vis = m_current_vi;
    if (!vcr_is_playing())
        return;

    bool pausing_at_last = (g_core->cfg->pause_at_last_frame && m_current_sample == g_header.length_samples);
    bool pausing_at_n = (g_core->cfg->pause_at_frame != -1 && m_current_sample >= g_core->cfg->pause_at_frame);

    if (pausing_at_last || pausing_at_n)
    {
        core_vr_pause_emu();
    }

    if (pausing_at_last)
    {
        g_core->cfg->pause_at_last_frame = 0;
    }

    if (pausing_at_n)
    {
        g_core->cfg->pause_at_frame = -1;
    }
}

bool is_frame_skipped()
{
    if (!g_core->cfg->render_throttling)
    {
        return false;
    }

    if (core_vcr_is_seeking())
    {
        return true;
    }

    if (!g_vr_fast_forward)
    {
        return false;
    }

    // skip every frame
    if (g_core->cfg->frame_skip_frequency == 0)
    {
        return true;
    }

    // skip no frames
    if (g_core->cfg->frame_skip_frequency == 1)
    {
        return false;
    }

    return g_total_frames % g_core->cfg->frame_skip_frequency != 0;
}
