﻿/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Describes the Mupen64 Plugin API.
 *
 * This header can be used standalone by Mupen64 plugins, just make sure to define CORE_PLUGIN_WITH_CALLBACKS first.
 * 
 */

#pragma once

/**
 * \brief Describes a controller.
 */
typedef struct
{
    int32_t Present;
    int32_t RawData;
    int32_t Plugin;
} core_controller;

/**
 * \brief Represents an extension for a controller.
 */
typedef enum {
    ce_none = 1,
    ce_mempak = 2,
    ce_rumblepak = 3,
    ce_transferpak = 4,
    ce_raw = 5
} core_controller_extension;

/**
 * \brief Represents a plugin type.
 */
typedef enum {
    plugin_video = 2,
    plugin_audio = 3,
    plugin_input = 4,
    plugin_rsp = 1,
} core_plugin_type;

/**
 * \brief Describes generic information about a plugin.
 */
typedef struct
{
    /**
     * \brief <c>0x0100</c> (old)
     * <c>0x0101</c> (new).
     * If <c>0x0101</c> is specified and the plugin is an input plugin, <c>InitiateControllers</c> will be called with the <c>INITIATECONTROLLERS</c> signature instead of <c>OLD_INITIATECONTROLLERS</c>.
     */
    uint16_t ver;

    /**
     * \brief The plugin type, see <c>core_plugin_type</c>.
     */
    uint16_t type;

    /**
     * \brief The plugin name.
     */
    char name[100];

    int32_t unused_normal_memory;
    int32_t unused_byteswapped;
} core_plugin_info;

/**
 * \brief Describes framebuffer information.
 */
typedef struct
{
    uint32_t addr;
    uint32_t size;
    uint32_t width;
    uint32_t height;
} core_fb_info;

/**
 * \brief Describes information about a video plugin.
 */
typedef struct
{
    void* main_hwnd;
    void* statusbar_hwnd;
    int32_t byteswapped;
    uint8_t* rom;
    uint8_t* rdram;
    uint8_t* dmem;
    uint8_t* imem;
    uint32_t* mi_intr_reg;
    uint32_t* dpc_start_reg;
    uint32_t* dpc_end_reg;
    uint32_t* dpc_current_reg;
    uint32_t* dpc_status_reg;
    uint32_t* dpc_clock_reg;
    uint32_t* dpc_bufbusy_reg;
    uint32_t* dpc_pipebusy_reg;
    uint32_t* dpc_tmem_reg;
    uint32_t* vi_status_reg;
    uint32_t* vi_origin_reg;
    uint32_t* vi_width_reg;
    uint32_t* vi_intr_reg;
    uint32_t* vi_v_current_line_reg;
    uint32_t* vi_timing_reg;
    uint32_t* vi_v_sync_reg;
    uint32_t* vi_h_sync_reg;
    uint32_t* vi_leap_reg;
    uint32_t* vi_h_start_reg;
    uint32_t* vi_v_start_reg;
    uint32_t* vi_v_burst_reg;
    uint32_t* vi_x_scale_reg;
    uint32_t* vi_y_scale_reg;
    void(__cdecl* check_interrupts)(void);
} core_gfx_info;

/**
 * \brief Describes information about an audio plugin.
 */
typedef struct
{
    void* main_hwnd;
    void* hinst;
    int32_t byteswapped;
    uint8_t* rom;
    uint8_t* rdram;
    uint8_t* dmem;
    uint8_t* imem;
    uint32_t* mi_intr_reg;
    uint32_t* ai_dram_addr_reg;
    uint32_t* ai_len_reg;
    uint32_t* ai_control_reg;
    uint32_t* ai_status_reg;
    uint32_t* ai_dacrate_reg;
    uint32_t* ai_bitrate_reg;
    void(__cdecl* check_interrupts)(void);
} core_audio_info;

/**
 * \brief Describes information about an input plugin.
 */
typedef struct
{
    void* main_hwnd;
    void* hinst;
    int32_t byteswapped;
    uint8_t* header;
    core_controller* controllers;
} core_input_info;

/**
 * \brief Describes information about an RSP plugin.
 */
typedef struct
{
    void* hinst;
    int32_t byteswapped;
    uint8_t* rdram;
    uint8_t* dmem;
    uint8_t* imem;
    uint32_t* mi_intr_reg;
    uint32_t* sp_mem_addr_reg;
    uint32_t* sp_dram_addr_reg;
    uint32_t* sp_rd_len_reg;
    uint32_t* sp_wr_len_reg;
    uint32_t* sp_status_reg;
    uint32_t* sp_dma_full_reg;
    uint32_t* sp_dma_busy_reg;
    uint32_t* sp_pc_reg;
    uint32_t* sp_semaphore_reg;
    uint32_t* dpc_start_reg;
    uint32_t* dpc_end_reg;
    uint32_t* dpc_current_reg;
    uint32_t* dpc_status_reg;
    uint32_t* dpc_clock_reg;
    uint32_t* dpc_bufbusy_reg;
    uint32_t* dpc_pipebusy_reg;
    uint32_t* dpc_tmem_reg;
    void(__cdecl* check_interrupts)(void);
    void(__cdecl* process_dlist_list)(void);
    void(__cdecl* process_alist_list)(void);
    void(__cdecl* process_rdp_list)(void);
    void(__cdecl* show_cfb)(void);
} core_rsp_info;

/**
 * \brief Represents a controller state.
 */
typedef union {
    uint32_t value;

    struct
    {
        unsigned dr : 1;
        unsigned dl : 1;
        unsigned dd : 1;
        unsigned du : 1;
        unsigned start : 1;
        unsigned z : 1;
        unsigned b : 1;
        unsigned a : 1;
        unsigned cr : 1;
        unsigned cl : 1;
        unsigned cd : 1;
        unsigned cu : 1;
        unsigned r : 1;
        unsigned l : 1;
        unsigned reserved_1 : 1;
        unsigned reserved_2 : 1;
        signed y : 8;
        signed x : 8;
    };
} core_buttons;

typedef void(__cdecl* CLOSEDLL)();
typedef void(__cdecl* DLLABOUT)(void*);
typedef void(__cdecl* DLLCONFIG)(void*);
typedef void(__cdecl* DLLTEST)(void*);
typedef void(__cdecl* GETDLLINFO)(core_plugin_info*);
typedef void(__cdecl* ROMCLOSED)();
typedef void(__cdecl* ROMOPEN)();

typedef void(__cdecl* CHANGEWINDOW)();
typedef int32_t(__cdecl* INITIATEGFX)(core_gfx_info);
typedef void(__cdecl* PROCESSDLIST)();
typedef void(__cdecl* PROCESSRDPLIST)();
typedef void(__cdecl* SHOWCFB)();
typedef void(__cdecl* UPDATESCREEN)();
typedef void(__cdecl* VISTATUSCHANGED)();
typedef void(__cdecl* VIWIDTHCHANGED)();
typedef void(__cdecl* READSCREEN)(void**, int32_t*, int32_t*);
typedef void(__cdecl* DLLCRTFREE)(void*);
typedef void(__cdecl* MOVESCREEN)(int32_t, int32_t);
typedef void(__cdecl* CAPTURESCREEN)(char*);
typedef void(__cdecl* GETVIDEOSIZE)(int32_t*, int32_t*);
typedef void(__cdecl* READVIDEO)(void**);
typedef void(__cdecl* FBREAD)(uint32_t);
typedef void(__cdecl* FBWRITE)(uint32_t addr, uint32_t size);
typedef void(__cdecl* FBGETFRAMEBUFFERINFO)(void*);

typedef void(__cdecl* AIDACRATECHANGED)(int32_t system_type);
typedef void(__cdecl* AILENCHANGED)();
typedef uint32_t(__cdecl* AIREADLENGTH)();
typedef int32_t(__cdecl* INITIATEAUDIO)(core_audio_info);
typedef void(__cdecl* PROCESSALIST)();
typedef void(__cdecl* AIUPDATE)(int32_t wait);

typedef void(__cdecl* CONTROLLERCOMMAND)(int32_t controller, unsigned char* command);
typedef void(__cdecl* GETKEYS)(int32_t controller, core_buttons* keys);
typedef void(__cdecl* SETKEYS)(int32_t controller, core_buttons keys);
typedef void(__cdecl* OLD_INITIATECONTROLLERS)(void* hwnd, core_controller controls[4]);
typedef void(__cdecl* INITIATECONTROLLERS)(core_input_info control_info);
typedef void(__cdecl* READCONTROLLER)(int32_t controller, unsigned char* command);
typedef void(__cdecl* KEYDOWN)(uint32_t wParam, int32_t lParam);
typedef void(__cdecl* KEYUP)(uint32_t wParam, int32_t lParam);

typedef uint32_t(__cdecl* DORSPCYCLES)(uint32_t);
typedef void(__cdecl* INITIATERSP)(core_rsp_info rsp_info, uint32_t* cycles);

#if defined(CORE_PLUGIN_WITH_CALLBACKS)

// ReSharper disable CppInconsistentNaming

#define EXPORT __declspec(dllexport)
#define CALL _cdecl

#pragma region Base

EXPORT void CALL CloseDLL(void);
EXPORT void CALL DllAbout(void* hParent);
EXPORT void CALL DllConfig(void* hParent);
EXPORT void CALL GetDllInfo(core_plugin_info* PluginInfo);
EXPORT void CALL RomClosed(void);
EXPORT void CALL RomOpen(void);

#pragma endregion

#pragma region Video

EXPORT void CALL CaptureScreen(const char* Directory);
EXPORT void CALL ChangeWindow(void);
EXPORT int CALL InitiateGFX(core_gfx_info Gfx_Info);
EXPORT void CALL MoveScreen(int xpos, int ypos);
EXPORT void CALL ProcessDList(void);
EXPORT void CALL ProcessRDPList(void);
EXPORT void CALL ShowCFB(void);
EXPORT void CALL UpdateScreen(void);
EXPORT void CALL ViStatusChanged(void);
EXPORT void CALL ViWidthChanged(void);

#pragma endregion

#pragma region Audio

EXPORT void CALL AiDacrateChanged(int32_t SystemType);
EXPORT void CALL AiLenChanged(void);
EXPORT uint32_t CALL AiReadLength(void);
EXPORT void CALL AiUpdate(int32_t Wait);
EXPORT void CALL DllTest(void* hParent);
EXPORT int32_t CALL InitiateAudio(core_audio_info Audio_Info);
EXPORT void CALL ProcessAList(void);

#pragma endregion

#pragma region Input

EXPORT void CALL ControllerCommand(int32_t Control, uint8_t* Command);
EXPORT void CALL GetKeys(int32_t Control, core_buttons* Keys);
EXPORT void CALL InitiateControllers(core_input_info* ControlInfo);
EXPORT void CALL ReadController(int Control, uint8_t* Command);
EXPORT void CALL WM_KeyDown(uint32_t wParam, uint32_t lParam);
EXPORT void CALL WM_KeyUp(uint32_t wParam, uint32_t lParam);

#pragma endregion

#pragma region RSP

EXPORT uint32_t DoRspCycles(uint32_t Cycles);
EXPORT void InitiateRSP(core_rsp_info Rsp_Info, uint32_t* CycleCount);

#pragma endregion

#undef EXPORT
#undef CALL

// ReSharper restore CppInconsistentNaming

#endif
