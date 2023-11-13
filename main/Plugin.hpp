#pragma once
/**
 * Mupen64 - plugin.hpp
 * Copyright (C) 2002 Hacktarux
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 *
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#include <memory>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <functional>
#include "helpers/io_helpers.h"

struct ProcAddress
{
	FARPROC _fp;

	ProcAddress(HMODULE module, LPCSTR name) : _fp(NULL)
	{
		_fp = ::GetProcAddress(module, name);
	}

	template <class T>
	operator T() const
	{
		union
		{
			FARPROC fp;
			T func;
		} converter{};
		converter.fp = _fp;
		return converter.func;
	}
};

enum plugin_type
{
	video = 2,
	audio = 3,
	input = 4,
	rsp = 1,
};

enum controller_extension
{
	none = 1,
	mempak = 2,
	rumblepak = 3,
	transferpak = 4,
	raw = 5
};

enum system_type
{
	ntsc,
	pal,
	mpal
};

// ReSharper disable CppInconsistentNaming

typedef struct
{
	WORD Version;
	WORD Type;
	char Name[100];

	/* If DLL supports memory these memory options then set them to TRUE or FALSE
	   if it does not support it */
	BOOL NormalMemory; /* a normal BYTE array */
	BOOL MemoryBswaped; /* a normal BYTE array where the memory has been pre
							 bswap on a dword (32 bits) boundry */
} PLUGIN_INFO;

typedef struct
{
	HINSTANCE hInst;
	// Whether the memory has been pre-byteswapped on a DWORD boundary
	BOOL MemoryBswaped;
	BYTE* RDRAM;
	BYTE* DMEM;
	BYTE* IMEM;

	DWORD* MI_INTR_REG;

	DWORD* SP_MEM_ADDR_REG;
	DWORD* SP_DRAM_ADDR_REG;
	DWORD* SP_RD_LEN_REG;
	DWORD* SP_WR_LEN_REG;
	DWORD* SP_STATUS_REG;
	DWORD* SP_DMA_FULL_REG;
	DWORD* SP_DMA_BUSY_REG;
	DWORD* SP_PC_REG;
	DWORD* SP_SEMAPHORE_REG;

	DWORD* DPC_START_REG;
	DWORD* DPC_END_REG;
	DWORD* DPC_CURRENT_REG;
	DWORD* DPC_STATUS_REG;
	DWORD* DPC_CLOCK_REG;
	DWORD* DPC_BUFBUSY_REG;
	DWORD* DPC_PIPEBUSY_REG;
	DWORD* DPC_TMEM_REG;

	void (__cdecl*CheckInterrupts)(void);
	void (__cdecl*ProcessDlistList)(void);
	void (__cdecl*ProcessAlistList)(void);
	void (__cdecl*ProcessRdpList)(void);
	void (__cdecl*ShowCFB)(void);
} RSP_INFO;

typedef struct
{
	HWND hWnd; /* Render window */
	HWND hStatusBar;
	/* if render window does not have a status bar then this is NULL */

	BOOL MemoryBswaped; // If this is set to TRUE, then the memory has been pre
	//   bswap on a dword (32 bits) boundry
	//	eg. the first 8 bytes are stored like this:
	//        4 3 2 1   8 7 6 5

	BYTE* HEADER; // This is the rom header (first 40h bytes of the rom
	// This will be in the same memory format as the rest of the memory.
	BYTE* RDRAM;
	BYTE* DMEM;
	BYTE* IMEM;

	DWORD* MI_INTR_REG;

	DWORD* DPC_START_REG;
	DWORD* DPC_END_REG;
	DWORD* DPC_CURRENT_REG;
	DWORD* DPC_STATUS_REG;
	DWORD* DPC_CLOCK_REG;
	DWORD* DPC_BUFBUSY_REG;
	DWORD* DPC_PIPEBUSY_REG;
	DWORD* DPC_TMEM_REG;

	DWORD* VI_STATUS_REG;
	DWORD* VI_ORIGIN_REG;
	DWORD* VI_WIDTH_REG;
	DWORD* VI_INTR_REG;
	DWORD* VI_V_CURRENT_LINE_REG;
	DWORD* VI_TIMING_REG;
	DWORD* VI_V_SYNC_REG;
	DWORD* VI_H_SYNC_REG;
	DWORD* VI_LEAP_REG;
	DWORD* VI_H_START_REG;
	DWORD* VI_V_START_REG;
	DWORD* VI_V_BURST_REG;
	DWORD* VI_X_SCALE_REG;
	DWORD* VI_Y_SCALE_REG;

	void (__cdecl*CheckInterrupts)(void);
} GFX_INFO;

typedef struct
{
	HWND hwnd;
	HINSTANCE hinst;

	BOOL MemoryBswaped; // If this is set to TRUE, then the memory has been pre
	//   bswap on a dword (32 bits) boundry
	//	eg. the first 8 bytes are stored like this:
	//        4 3 2 1   8 7 6 5
	BYTE* HEADER; // This is the rom header (first 40h bytes of the rom
	// This will be in the same memory format as the rest of the memory.
	BYTE* RDRAM;
	BYTE* DMEM;
	BYTE* IMEM;

	DWORD* MI_INTR_REG;

	DWORD* AI_DRAM_ADDR_REG;
	DWORD* AI_LEN_REG;
	DWORD* AI_CONTROL_REG;
	DWORD* AI_STATUS_REG;
	DWORD* AI_DACRATE_REG;
	DWORD* AI_BITRATE_REG;

	void (__cdecl*CheckInterrupts)(void);
} AUDIO_INFO;

typedef struct
{
	BOOL Present;
	BOOL RawData;
	int Plugin;
} CONTROL;

typedef union
{
	DWORD Value;

	struct
	{
		unsigned R_DPAD : 1;
		unsigned L_DPAD : 1;
		unsigned D_DPAD : 1;
		unsigned U_DPAD : 1;
		unsigned START_BUTTON : 1;
		unsigned Z_TRIG : 1;
		unsigned B_BUTTON : 1;
		unsigned A_BUTTON : 1;

		unsigned R_CBUTTON : 1;
		unsigned L_CBUTTON : 1;
		unsigned D_CBUTTON : 1;
		unsigned U_CBUTTON : 1;
		unsigned R_TRIG : 1;
		unsigned L_TRIG : 1;
		unsigned Reserved1 : 1;
		unsigned Reserved2 : 1;

		signed Y_AXIS : 8;

		signed X_AXIS : 8;
	};
} BUTTONS;

typedef struct
{
	HWND h_main_window;
	HINSTANCE hinst;

	BOOL MemoryBswaped; // If this is set to TRUE, then the memory has been pre
	//   bswap on a dword (32 bits) boundry, only effects header.
	//	eg. the first 8 bytes are stored like this:
	//        4 3 2 1   8 7 6 5
	BYTE* HEADER; // This is the rom header (first 40h bytes of the rom)
	CONTROL* Controls; // A pointer to an array of 4 controllers .. eg:
	// CONTROL Controls[4];
} CONTROL_INFO;

static DWORD __cdecl dummy_doRspCycles(DWORD Cycles) { return Cycles; };

extern CONTROL controls[4];

extern void (__cdecl*getDllInfo)(PLUGIN_INFO* PluginInfo);
extern void (__cdecl*dllConfig)(HWND h_parent);
extern void (__cdecl*dllTest)(HWND h_parent);
extern void (__cdecl*dllAbout)(HWND h_parent);

extern void (__cdecl*changeWindow)();
extern void (__cdecl*closeDLL_gfx)();
extern BOOL (__cdecl*initiateGFX)(GFX_INFO Gfx_Info);
extern void (__cdecl*processDList)();
extern void (__cdecl*processRDPList)();
extern void (__cdecl*romClosed_gfx)();
extern void (__cdecl*romOpen_gfx)();
extern void (__cdecl*showCFB)();
extern void (__cdecl*updateScreen)();
extern void (__cdecl*viStatusChanged)();
extern void (__cdecl*viWidthChanged)();
extern void (__cdecl*readScreen)(void** dest, long* width, long* height);
extern void (__cdecl*DllCrtFree)(void* block);

extern void (__cdecl*aiDacrateChanged)(int system_type);
extern void (__cdecl*aiLenChanged)();
extern DWORD (__cdecl*aiReadLength)();
extern void (__cdecl*closeDLL_audio)();
extern BOOL (__cdecl*initiateAudio)(AUDIO_INFO audio_info);
extern void (__cdecl*processAList)();
extern void (__cdecl*romClosed_audio)();
extern void (__cdecl*romOpen_audio)();

extern void (__cdecl*closeDLL_input)();
extern void (__cdecl*controllerCommand)(int control, BYTE* command);
extern void (__cdecl*getKeys)(int control, BUTTONS* keys);
extern void (__cdecl*setKeys)(int control, BUTTONS keys);
extern void (__cdecl*initiateControllers)(CONTROL_INFO control_info);
extern void (__cdecl*readController)(int control, BYTE* command);
extern void (__cdecl*romClosed_input)();
extern void (__cdecl*romOpen_input)();
extern void (__cdecl*keyDown)(WPARAM w_param, LPARAM l_param);
extern void (__cdecl*keyUp)(WPARAM w_param, LPARAM l_param);

extern void (__cdecl*closeDLL_RSP)();
extern DWORD (__cdecl*doRspCycles)(DWORD cycles);
extern void (__cdecl*initiateRSP)(RSP_INFO rsp_info, DWORD* cycle_count);
extern void (__cdecl*romClosed_RSP)();

extern void (__cdecl*fBRead)(DWORD addr);
extern void (__cdecl*fBWrite)(DWORD addr, DWORD size);
extern void (__cdecl*fBGetFrameBufferInfo)(void* p);

extern void (__cdecl*moveScreen)(int xpos, int ypos);
extern void (__cdecl*CaptureScreen)(char* directory);
extern void (__cdecl*old_initiateControllers)(HWND h_main_window,
                                              CONTROL controls[4]);
extern void (__cdecl*aiUpdate)(BOOL wait);

// ReSharper restore CppInconsistentNaming

// frame buffer plugin spec extension
typedef struct
{
	DWORD addr;
	DWORD size;
	DWORD width;
	DWORD height;
} frame_buffer_info;


typedef struct s_plugin
{
	std::filesystem::path path;
	std::string name;
	plugin_type type;
	uint16_t version;
	HMODULE handle;
} t_plugin;

extern t_plugin* video_plugin;
extern t_plugin* audio_plugin;
extern t_plugin* input_plugin;
extern t_plugin* rsp_plugin;

/**
 * \return A vector of available plugin
 */
std::vector<t_plugin*> get_available_plugins();


/**
 * \brief Destroys a plugin and releases the library handle
 * \param plugin A pointer to the plugin to be destroyed
 */
void plugin_destroy(t_plugin* plugin);

/// <summary>
/// Opens a plugin's configuration dialog, if it exists
/// </summary>
void plugin_config(t_plugin* plugin);

/// <summary>
/// Opens a plugin's test dialog, if it exists
/// </summary>
void plugin_test(t_plugin* plugin);

/// <summary>
/// Opens a plugin's about dialog, if it exists
/// </summary>
void plugin_about(t_plugin* plugin);

/// <summary>
/// Searches for plugins and loads their exported functions into the globals
/// </summary>
bool load_plugins();

/**
 * \brief Unloads all loaded plugins
 */
void unload_plugins();

void load_gfx(HMODULE);
void load_audio(HMODULE);
void load_input(HMODULE);
void load_rsp(HMODULE);

/// <summary>
/// Initializes dummy info used by per-plugin functions
/// </summary>
void setup_dummy_info();
