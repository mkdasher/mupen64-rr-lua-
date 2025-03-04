/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Describes the Mupen64 Core API.
 */

#pragma once

#define EXPORT
#define CALL

#include "core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Callbacks for the core to call into the host.
 */
typedef struct {
    void (*vi)(void);
    void (*input)(core_buttons* input, int index);
    void (*frame)(void);
    void (*interval)(void);
    void (*ai_len_changed)();
    void (*play_movie)(void);
    void (*stop_movie)(void);
    void (*loop_movie)(void);
    void (*save_state)(void);
    void (*load_state)(void);
    void (*reset)(void);
    void (*seek_completed)(void);
    void (*core_executing_changed)(bool);
    void (*emu_paused_changed)(bool);
    void (*emu_launched_changed)(bool);
    void (*emu_starting_changed)(bool);
    void (*emu_stopping)(void);
    void (*reset_completed)(void);
    void (*speed_modifier_changed)(int32_t);
    void (*warp_modify_status_changed)(bool);
    void (*current_sample_changed)(int32_t);
    void (*task_changed)(core_vcr_task);
    void (*rerecords_changed)(uint64_t);
    void (*unfreeze_completed)(void);
    void (*seek_savestate_changed)(size_t);
    void (*readonly_changed)(bool);
    void (*dacrate_changed)(core_system_type);
    void (*debugger_resumed_changed)(bool);
    void (*debugger_cpu_state_changed)(core_dbg_cpu_state*);
    void (*lag_limit_exceeded)(void);
    void (*seek_status_changed)(void);
} core_callbacks;

/**
 * \brief The plugin function collection.
 */
typedef struct {
    CLOSEDLL close_dll_gfx;
    INITIATEGFX initiate_gfx;
    PROCESSDLIST process_dlist;
    PROCESSRDPLIST process_rdp_list;
    ROMCLOSED rom_closed_gfx;
    ROMOPEN rom_open_gfx;
    SHOWCFB show_cfb;
    UPDATESCREEN update_screen;
    VISTATUSCHANGED vi_status_changed;
    VIWIDTHCHANGED vi_width_changed;
    READSCREEN read_screen;
    DLLCRTFREE dll_crt_free;
    MOVESCREEN move_screen;
    CAPTURESCREEN capture_screen;
    GETVIDEOSIZE get_video_size;
    READVIDEO read_video;
    FBREAD fb_read;
    FBWRITE fb_write;
    FBGETFRAMEBUFFERINFO fb_get_frame_buffer_info;
    CHANGEWINDOW change_window;

    AIDACRATECHANGED ai_dacrate_changed;
    AILENCHANGED ai_len_changed;
    AIREADLENGTH ai_read_length;
    CLOSEDLL close_dll_audio;
    INITIATEAUDIO initiate_audio;
    PROCESSALIST process_a_list;
    ROMCLOSED rom_closed_audio;
    ROMOPEN rom_open_audio;
    AIUPDATE ai_update;

    CLOSEDLL close_dll_input;
    CONTROLLERCOMMAND controller_command;
    GETKEYS get_keys;
    SETKEYS set_keys;
    OLD_INITIATECONTROLLERS old_initiate_controllers;
    INITIATECONTROLLERS initiate_controllers;
    READCONTROLLER read_controller;
    ROMCLOSED rom_closed_input;
    ROMOPEN rom_open_input;
    KEYDOWN key_down;
    KEYUP key_up;

    CLOSEDLL close_dll_rsp;
    DORSPCYCLES do_rsp_cycles;
    INITIATERSP initiate_rsp;
    ROMCLOSED rom_closed_rsp;
} core_plugin_funcs;

/**
 * \brief The core's parameters.
 */
typedef struct {

#pragma region Host-Provided
    /**
     * \brief The core's configuration.
     */
    core_cfg* cfg;

    /**
     * \brief The core callbacks.
     */
    core_callbacks callbacks;

    /**
     * \brief The plugin functions.
     */
    core_plugin_funcs plugin_funcs;

    /**
     * \brief Logs the specified message at the trace level.
     */
    void (*log_trace)(const std::wstring&);

    /**
     * \brief Logs the specified message at the info level.
     */
    void (*log_info)(const std::wstring&);

    /**
     * \brief Logs the specified message at the warning level.
     */
    void (*log_warn)(const std::wstring&);

    /**
     * \brief Logs the specified message at the error level.
     */
    void (*log_error)(const std::wstring&);

    /**
     * \brief Loads the plugins specified by the config paths.
     * \return Whether the plugins were loaded successfully.
     */
    bool (*load_plugins)(void);

    /**
     * \brief Called after load_plugins, this function loads plugin functions into plugin_funcs and calls the "initiate" family of functions for all plugins.
     * \remark This function must be infallible.
     */
    void (*initiate_plugins)(void);

    /**
     * \brief Executes a function on a background thread.
     * \param func The function to be executed.
     * \param key The function's key used for deduplication. If not 0, the function will not be queued if another function with the same key is already in the queue.
     */
    void (*invoke_async)(const std::function<void()>& func, size_t key);

    /**
     * \brief Gets the directory in which savestates and persistent game saves should be stored.
     */
    std::filesystem::path (*get_saves_directory)(void);

    /**
     * \brief Gets the directory in which VCR backups should be stored.
     */
    std::filesystem::path (*get_backups_directory)(void);

    /**
     * Prompts the user to pick a choice from a provided collection of choices.
     * \param choices The collection of choices.
     * \param str The dialog content.
     * \param title The dialog title.
     * \return The index of the chosen choice.
     */
    size_t (*show_multiple_choice_dialog)(const std::vector<std::wstring>& choices, const wchar_t* str, const wchar_t* title, core_dialog_type type);

    /**
     * Prompts the user to answer a Yes/No question.
     * \param str The dialog content.
     * \param title The dialog title.
     * \param warning Whether the tone of the message is perceived as a warning.
     * \return Whether the user answered yes.
     * \remarks If the user has chosen to not use modals, this function will return true by default.
     */
    bool (*show_ask_dialog)(const wchar_t* str, const wchar_t* title, bool warning);

    /**
     * \brief Shows the user a dialog.
     * \param str The dialog content.
     * \param title The dialog title.
     * \param type The dialog's tone.
     */
    void (*show_dialog)(const wchar_t* str, const wchar_t* title, core_dialog_type type);

    /**
     * \brief Shows text in the notification section of the statusbar.
     */
    void (*show_statusbar)(const wchar_t* str);

    /**
     * \brief Updates the screen.
     */
    void (*update_screen)(void);

    /**
     * \brief Writes the MGE compositor's current emulation front buffer into the destination buffer.
     * \param buffer The video buffer. Must be at least of size <c>width * height * 3</c>, as acquired by <c>plugin_funcs.get_video_size</c>.
     */
    void (*copy_video)(void* buffer);

    /**
     * \brief Finds the first rom from the available ROM list which matches the predicate.
     * \param predicate A predicate which determines if the rom matches.
     * \return The rom's path, or an empty string if no rom was found.
     */
    std::wstring (*find_available_rom)(const std::function<bool(const core_rom_header&)>& predicate);

    /**
     * \brief Fills the screen with the specified data.
     * The size of the buffer is determined by the resolution returned by the get_video_size (MGE) or readScreen (Non-MGE) functions.
     * Note that the buffer format is 24bpp.
     */
    void (*load_screen)(void* data);

    /**
     * \brief Gets the plugin names.
     * \param video Pointer to the video plugin name buffer. Must be at least 64 bytes large. If null, no data will be written.
     * \param audio Pointer to the audio plugin name buffer. Must be at least 64 bytes large. If null, no data will be written.
     * \param input Pointer to the input plugin name buffer. Must be at least 64 bytes large. If null, no data will be written.
     * \param rsp Pointer to the RSP plugin name buffer. Must be at least 64 bytes large. If null, no data will be written.
     * \note Must be called after loading plugins and their globals.
     */
    void (*get_plugin_names)(char* video, char* audio, char* input, char* rsp);

#pragma endregion

#pragma region Core-Provided
    core_controller controls[4];

    core_timer_delta g_frame_deltas[core_timer_max_deltas];
    std::mutex g_frame_deltas_mutex;
    core_timer_delta g_vi_deltas[core_timer_max_deltas];
    std::mutex g_vi_deltas_mutex;

    uint8_t* rom;
    uint32_t* rdram;
    core_rdram_reg* rdram_register;
    core_pi_reg* pi_register;
    core_mips_reg* MI_register;
    core_sp_reg* sp_register;
    core_si_reg* si_register;
    core_vi_reg* vi_register;
    core_rsp_reg* rsp_register;
    core_ri_reg* ri_register;
    core_ai_reg* ai_register;
    core_dpc_reg* dpc_register;
    core_dps_reg* dps_register;
    uint32_t* SP_DMEM;
    uint32_t* SP_IMEM;
    uint32_t* PIF_RAM;

#pragma endregion
} core_params;

/**
 * \brief Initializes the core with the specified parameters.
 * \remarks
 * The core must be initialized before any other functions are called.
 * The core parameters must be valid for the lifetime of the core.
 */
EXPORT core_result CALL core_init(core_params* params);

#pragma region Emulator

/**
 * \brief Performs an in-place endianness correction on a rom's header.
 * \param rom The rom buffer, which must be at least 131 bytes large.
 */
EXPORT void CALL core_vr_byteswap(uint8_t* rom);

/**
 * \brief Gets the number of lag frames.
 */
EXPORT size_t CALL core_vr_get_lag_count();

/**
 * \brief Gets whether the core is currently executing.
 */
EXPORT bool CALL core_vr_get_core_executing();

/**
 * \brief Gets whether the emu is launched.
 */
EXPORT bool CALL core_vr_get_launched();

/**
 * \brief Gets whether the core is currently frame-advancing.
 */
EXPORT bool CALL core_vr_get_frame_advance();

/**
 * \brief Gets whether the core is currently paused.
 */
EXPORT bool CALL core_vr_get_paused();

/**
 * \brief Pauses the emulator.
 */
EXPORT void CALL core_vr_pause_emu();

/**
 * \brief Resumes the emulator.
 */
EXPORT void CALL core_vr_resume_emu();

/**
 * \brief Increments the wait counter.
 * \remarks If the counter is greater than 0, the core will wait for the counter to be 0 before continuing to the next frame. This function is thread-safe.
 */
EXPORT void CALL core_vr_wait_increment();

/**
 * \brief Decrements the wait counter.
 * \remarks If the counter is greater than 0, the core will wait for the counter to be 0 before continuing to the next frame. This function is thread-safe.
 */
EXPORT void CALL core_vr_wait_decrement();

/**
 * \brief Starts the emulator.
 * \param path Path to a rom or corresponding movie file.
 * \param wait Whether the calling thread will wait for other core operations to complete. When true, the Busy result is never returned.
 * \return The operation result.
 * \remarks This function must be called from a thread that isn't interlocked with the emulator thread.
 */
EXPORT core_result CALL core_vr_start_rom(std::filesystem::path path, bool wait);

/**
 * \brief Stops the emulator.
 * \param stop_vcr Whether all VCR operations will be stopped. When resetting the ROM due to an in-movie restart, this needs to be false.
 * \param wait Whether the calling thread will wait for other core operations to complete. When true, the Busy result is never returned.
 * \return The operation result.
 * \remarks This function must be called from a thread that isn't interlocked with the emulator thread.
 */
EXPORT core_result CALL core_vr_close_rom(bool stop_vcr, bool wait);

/**
 * \brief Resets the emulator.
 * \param reset_save_data Whether save data (e.g.: EEPROM, SRAM, Mempak) will be reset.
 * \param stop_vcr Whether all VCR operations will be stopped. When resetting the ROM due to an in-movie restart, this needs to be false.
 * \param wait Whether the calling thread will wait for other core operations to complete. When true, the Busy result is never returned.
 * \return The operation result.
 * \remarks This function must be called from a thread that isn't interlocked with the emulator thread.
 */
EXPORT core_result CALL core_vr_reset_rom(bool reset_save_data, bool stop_vcr, bool wait);

/**
 * \brief Starts frame advancing the specified amount of frames.
 */
EXPORT void CALL core_vr_frame_advance(size_t);

/**
 * \brief Toggles between fullscreen and windowed mode.
 */
EXPORT void CALL core_vr_toggle_fullscreen_mode();

/**
 * \brief Sets the fast-forward state.
 */
EXPORT void CALL core_vr_set_fast_forward(bool);

/**
 * \brief Gets whether tracelogging is active.
 */
EXPORT bool CALL core_vr_is_tracelog_active();

/**
 * \brief Gets the fullscreen state.
 */
EXPORT bool CALL core_vr_is_fullscreen();

/**
 * \brief Gets the GS button state.
 */
EXPORT bool CALL core_vr_get_gs_button();

/**
 * \brief Sets the GS button state.
 */
EXPORT void CALL core_vr_set_gs_button(bool);

/**
 * \param country_code A rom's country code.
 * \return The maximum amount of VIs per second intended.
 */
EXPORT uint32_t CALL core_vr_get_vis_per_second(uint16_t country_code);

/**
 * \breif Gets the rom header.
 */
EXPORT core_rom_header* CALL core_vr_get_rom_header();

/**
 * \param country_code A rom's country code.
 * \return The rom's country name.
 */
EXPORT std::wstring CALL core_vr_country_code_to_country_name(uint16_t country_code);

/**
 * \brief Updates internal timings after the speed modifier changes.
 */
EXPORT void CALL core_vr_on_speed_modifier_changed();

/**
 * \brief Invalidates the visuals, allowing an updateScreen call to happen.
 */
EXPORT void CALL core_vr_invalidate_visuals();

/**
 * \brief Gets whether the current set of plugin functions loaded in the core is capable of providing MGE functionality.
 */
EXPORT bool CALL core_vr_get_mge_available();

#pragma endregion

#pragma region VCR

/**
 * \brief Parses a movie's header
 * \param path The movie's path
 * \param header The header to fill
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_parse_header(std::filesystem::path path, core_vcr_movie_header* header);

/**
 * \brief Reads the inputs from a movie
 * \param path The movie's path
 * \param inputs The button collection to fill
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_read_movie_inputs(std::filesystem::path path, std::vector<core_buttons>& inputs);

/**
 * \brief Starts playing back a movie
 * \param path The movie's path
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_start_playback(std::filesystem::path path);

/**
 * \brief Starts recording a movie
 * \param path The movie's path
 * \param flags Start flags, see MOVIE_START_FROM_X
 * \param author The movie author's name
 * \param description The movie's description
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_start_record(std::filesystem::path path, uint16_t flags, std::string author, std::string description);

/**
 * \brief Replaces the author and description information of a movie
 * \param path The movie's path
 * \param author The movie author's name
 * \param description The movie's description
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_replace_author_info(const std::filesystem::path& path, const std::string& author, const std::string& description);

/**
 * \brief Gets the completion status of the current seek operation.
 * If no seek operation is active, the first value is the current sample and the second one is SIZE_MAX.
 */
EXPORT void CALL core_vcr_get_seek_completion(std::pair<size_t, size_t>& pair);

/**
 * \brief Begins seeking to a frame in the current movie.
 * \param str A seek format string
 * \param pause_at_end Whether the emu should be paused when the seek operation ends
 * \return The operation result
 * \remarks When the seek operation completes, the SeekCompleted message will be sent
 *
 * Seek string format possibilities:
 *	"n" - Frame
 *	"+n", "-n" - Relative to current sample
 *	"^n" - Sample n from the end
 *
 */
EXPORT core_result CALL core_vcr_begin_seek(std::wstring str, bool pause_at_end);

/**
 * \brief Converts a freeze buffer into a movie, trying to reconstruct as much as possible
 * \param freeze The freeze buffer to convert
 * \param header The generated header
 * \param inputs The generated inputs
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_convert_freeze_buffer_to_movie(const core_vcr_freeze_info& freeze, core_vcr_movie_header& header, std::vector<core_buttons>& inputs);

/**
 * \brief Stops the current seek operation
 */
EXPORT void CALL core_vcr_stop_seek();

/**
 * \brief Gets whether the VCR engine is currently performing a seek operation
 */
EXPORT bool CALL core_vcr_is_seeking();

/**
 * \brief Generates the current movie freeze buffer.
 * \return Whether a freeze buffer was generated.
 */
EXPORT bool CALL core_vcr_freeze(core_vcr_freeze_info* freeze);

/**
 * \brief Restores the movie from a freeze buffer
 * \param freeze The freeze buffer
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_unfreeze(core_vcr_freeze_info freeze);

/**
 * \brief Writes a backup of the current movie to the backup folder.
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_write_backup();

/**
 * \brief Stops all running tasks
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_stop_all();

/**
 * \brief Gets the current movie path. Only valid when task is not idle.
 */
EXPORT std::filesystem::path CALL core_vcr_get_path();

/**
 * \brief Gets the current task
 */
EXPORT core_vcr_task CALL core_vcr_get_task();

/**
 * Gets the sample length of the current movie. If no movie is active, the function returns UINT32_MAX.
 */
EXPORT uint32_t CALL core_vcr_get_length_samples();

/**
 * Gets the VI length of the current movie. If no movie is active, the function returns UINT32_MAX.
 */
EXPORT uint32_t CALL core_vcr_get_length_vis();

/**
 * Gets the current VI in the current movie. If no movie is active, the function returns -1.
 */
EXPORT int32_t CALL core_vcr_get_current_vi();

/**
 * Gets a copy of the current input buffer
 */
EXPORT std::vector<core_buttons> CALL core_vcr_get_inputs();

/**
 * Begins a warp modification operation. A "warp modification operation" is the changing of sample data which is temporally behind the current sample.
 *
 * The VCR engine will find the last common sample between the current input buffer and the provided one.
 * Then, the closest savestate prior to that sample will be loaded and recording will be resumed with the modified inputs up to the sample the function was called at.
 *
 * This operation is long-running and status is reported via the WarpModifyStatusChanged message.
 * A successful warp modify operation can be detected by the status changing from warping to none with no errors inbetween.
 *
 * If the provided buffer is identical to the current input buffer (in both content and size), the operation will succeed with no seek.
 *
 * If the provided buffer is larger than the current input buffer and the first differing input is after the current sample, the operation will succeed with no seek.
 * The input buffer will be overwritten with the provided buffer and when the modified frames are reached in the future, they will be "applied" like in playback mode.
 *
 * If the provided buffer is smaller than the current input buffer, the VCR engine will seek to the last frame and otherwise perform the warp modification as normal.
 *
 * An empty input buffer will cause the operation to fail.
 *
 * \param inputs The input buffer to use.
 * \return The operation result
 */
EXPORT core_result CALL core_vcr_begin_warp_modify(const std::vector<core_buttons>& inputs);

/**
 * Gets the warp modify status
 */
EXPORT bool CALL core_vcr_get_warp_modify_status();

/**
 * Gets the first differing frame when performing a warp modify operation.
 * If no warp modify operation is active, the function returns SIZE_MAX.
 */
EXPORT size_t CALL core_vcr_get_warp_modify_first_difference_frame();

/**
 * Gets the current seek savestate frames.
 * Keys are frame numbers, values are unimportant and abscence of a seek savestate at a frame is marked by the respective key not existing.
 */
EXPORT void CALL core_vcr_get_seek_savestate_frames(std::unordered_map<size_t, bool>& map);

/**
 * Gets whether a seek savestate exists at the specified frame.
 * The returned state changes when the <c>SeekSavestatesChanged</c> message is sent.
 */
EXPORT bool CALL core_vcr_has_seek_savestate_at_frame(size_t frame);

#pragma endregion

#pragma region Tracelog
/**
 * \brief Starts trace logging to the specified file.
 * \param path The output path.
 * \param binary Whether log output is in a binary format.
 * \param append Whether log output will be appended to the file.
 */
EXPORT void CALL core_tl_start(std::filesystem::path path, bool binary, bool append);

/**
 * \brief Stops trace logging.
 */
EXPORT void CALL core_tl_stop();

#pragma endregion

#pragma region Savestates

/**
 * \brief Executes a savestate operation to a path.
 * \param path The savestate's path.
 * \param job The job to set.
 * \param callback The callback to call when the operation is complete.
 * \param ignore_warnings Whether warnings, such as those about ROM compatibility, shouldn't be shown.
 * \warning The operation won't complete immediately. Must be called via AsyncExecutor unless calls are originating from the emu thread.
 * \return Whether the operation was enqueued.
 */
EXPORT bool CALL core_st_do_file(const std::filesystem::path& path, core_st_job job, const core_st_callback& callback, bool ignore_warnings);

/**
 * \brief Executes a savestate operation to a slot.
 * \param slot The slot to construct the savestate path with.
 * \param job The job to set.
 * \param callback The callback to call when the operation is complete.
 * \param ignore_warnings Whether warnings, such as those about ROM compatibility, shouldn't be shown.
 * \warning The operation won't complete immediately. Must be called via AsyncExecutor unless calls are originating from the emu thread.
 * \return Whether the operation was enqueued.
 */
EXPORT bool CALL core_st_do_slot(int32_t slot, core_st_job job, const core_st_callback& callback, bool ignore_warnings);

/**
 * Executes a savestate operation in-memory.
 * \param buffer The buffer to use for the operation. Can be empty if the <see cref="job"/> is <see cref="e_st_job::save"/>.
 * \param job The job to set.
 * \param callback The callback to call when the operation is complete.
 * \param ignore_warnings Whether warnings, such as those about ROM compatibility, shouldn't be shown.
 * \warning The operation won't complete immediately. Must be called via AsyncExecutor unless calls are originating from the emu thread.
 * \return Whether the operation was enqueued.
 */
EXPORT bool CALL core_st_do_memory(const std::vector<uint8_t>& buffer, core_st_job job, const core_st_callback& callback, bool ignore_warnings);

/**
 * Gets the undo savestate buffer. Will be empty will no undo savestate is available.
 */
EXPORT void CALL core_st_get_undo_savestate(std::vector<uint8_t>& buffer);

#pragma endregion

#pragma region Debugger

/**
 * \brief Gets whether execution is resumed.
 */
EXPORT bool CALL core_dbg_get_resumed();

/**
 * \brief Sets execution resumed status.
 */
EXPORT void CALL core_dbg_set_is_resumed(bool);

/**
 * Steps execution by one instruction.
 */
EXPORT void CALL core_dbg_step();

/**
 * \brief Gets whether DMA reads are allowed. If false, reads should return 0xFF.
 */
EXPORT bool CALL core_dbg_get_dma_read_enabled();

/**
 * \brief Sets whether DMA reads are allowed.
 */
EXPORT void CALL core_dbg_set_dma_read_enabled(bool);

/**
 * \brief Gets whether the RSP is enabled.
 */
EXPORT bool CALL core_dbg_get_rsp_enabled();

/**
 * \brief Sets whether the RSP is enabled.
 */
EXPORT void CALL core_dbg_set_rsp_enabled(bool);

/**
 * \brief Disassembles an instruction at a given address.
 */
EXPORT char* CALL core_dbg_disassemble(char* buf, uint32_t w, uint32_t pc);

#pragma endregion

#pragma region Cheats

/**
 * \brief Compiles a cheat code from code.
 * \param code The cheat code. Must be in the GameShark format.
 * \param cheat The compiled cheat. If the compilation fails, the cheat won't be mutated.
 * \return Whether the compilation was successful.
 */
EXPORT bool CALL core_cht_compile(const std::wstring& code, core_cheat& cheat);

/**
 * \brief Gets the cheat override stack.
 */
EXPORT void CALL core_cht_get_override_stack(std::stack<std::vector<core_cheat>>&);

/**
 * \brief Gets the cheat list.
 * \remarks The returned cheat list may not be the one set via core_cht_set_list, as the core can apply cheat overrides.
 */
EXPORT void CALL core_cht_get_list(std::vector<core_cheat>&);

/**
 * \brief Sets the cheat list.
 * \remarks If a core cheat override is active, core_cht_set_list will do nothing.
 */
EXPORT void CALL core_cht_set_list(const std::vector<core_cheat>&);

#pragma endregion

#ifdef __cplusplus
}
#endif
