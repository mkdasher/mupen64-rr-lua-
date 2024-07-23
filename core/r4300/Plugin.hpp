#pragma once
/**
 * Mupen64 - Plugin.hpp
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
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <functional>
#include <shared/helpers/IOHelpers.h>
#include <shared/types/CoreTypes.h>

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


static unsigned long __cdecl dummy_doRspCycles(unsigned long Cycles) { return Cycles; };

extern CONTROL Controls[4];


#pragma region Base Functions

typedef void (__cdecl*GETDLLINFO)(PLUGIN_INFO*);
typedef void (__cdecl*DLLCONFIG)(void*);
typedef void (__cdecl*DLLTEST)(void*);
typedef void (__cdecl*DLLABOUT)(void*);
extern GETDLLINFO getDllInfo;
extern DLLCONFIG dllConfig;
extern DLLTEST dllTest;
extern DLLABOUT dllAbout;

#pragma endregion

#pragma region Video Functions

typedef void (__cdecl*CHANGEWINDOW)();
typedef void (__cdecl*CLOSEDLL_GFX)();
typedef int  (__cdecl*INITIATEGFX)(GFX_INFO);
typedef void (__cdecl*PROCESSDLIST)();
typedef void (__cdecl*PROCESSRDPLIST)();
typedef void (__cdecl*ROMCLOSED_GFX)();
typedef void (__cdecl*ROMOPEN_GFX)();
typedef void (__cdecl*SHOWCFB)();
typedef void (__cdecl*UPDATESCREEN)();
typedef void (__cdecl*VISTATUSCHANGED)();
typedef void (__cdecl*VIWIDTHCHANGED)();
typedef void (__cdecl*READSCREEN)(void**, long*, long*);
typedef void (__cdecl*DLLCRTFREE)(void*);
typedef void (__cdecl*MOVESCREEN)(int, int);
typedef void (__cdecl*CAPTURESCREEN)(char*);
typedef void (__cdecl*GETVIDEOSIZE)(long*, long*);
typedef void (__cdecl*READVIDEO)(void**);
typedef void (__cdecl*FBREAD)(unsigned long);
typedef void (__cdecl*FBWRITE)(unsigned long addr, unsigned long size);
typedef void (__cdecl*FBGETFRAMEBUFFERINFO)(void*);
extern CHANGEWINDOW changeWindow;
extern CLOSEDLL_GFX closeDLL_gfx;
extern INITIATEGFX initiateGFX;
extern PROCESSDLIST processDList;
extern PROCESSRDPLIST processRDPList;
extern ROMCLOSED_GFX romClosed_gfx;
extern ROMOPEN_GFX romOpen_gfx;
extern SHOWCFB showCFB;
extern UPDATESCREEN updateScreen;
extern VISTATUSCHANGED viStatusChanged;
extern VIWIDTHCHANGED viWidthChanged;
extern READSCREEN readScreen;
extern DLLCRTFREE DllCrtFree;
extern MOVESCREEN moveScreen;
extern CAPTURESCREEN CaptureScreen;
// Mupen Graphics Extension, currently only implemented by bettergln64/ngl64
extern GETVIDEOSIZE get_video_size;
extern READVIDEO read_video;
// Framebuffer Extension
extern FBREAD fBRead;
extern FBWRITE fBWrite;
extern FBGETFRAMEBUFFERINFO fBGetFrameBufferInfo;

#pragma endregion

#pragma region Audio Functions

typedef void 		  (__cdecl*AIDACRATECHANGED)(int system_type);
typedef void 		  (__cdecl*AILENCHANGED)();
typedef unsigned long (__cdecl*AIREADLENGTH)();
typedef void 		  (__cdecl*CLOSEDLL_AUDIO)();
typedef int 		  (__cdecl*INITIATEAUDIO)(AUDIO_INFO);
typedef void 		  (__cdecl*PROCESSALIST)();
typedef void 		  (__cdecl*ROMCLOSED_AUDIO)();
typedef void 		  (__cdecl*ROMOPEN_AUDIO)();
typedef void 		  (__cdecl*AIUPDATE)(int wait);
extern AIDACRATECHANGED aiDacrateChanged;
extern AILENCHANGED aiLenChanged;
extern AIREADLENGTH aiReadLength;
extern CLOSEDLL_AUDIO closeDLL_audio;
extern INITIATEAUDIO initiateAudio;
extern PROCESSALIST processAList;
extern ROMCLOSED_AUDIO romClosed_audio;
extern ROMOPEN_AUDIO romOpen_audio;
extern AIUPDATE aiUpdate;

#pragma endregion

#pragma region Input Functions

typedef void  (__cdecl*CLOSEDLL_INPUT)();
typedef void  (__cdecl*CONTROLLERCOMMAND)(int controller, unsigned char* command);
typedef void  (__cdecl*GETKEYS)(int controller, BUTTONS* keys);
typedef void  (__cdecl*SETKEYS)(int controller, BUTTONS keys);
typedef void  (__cdecl*OLD_INITIATECONTROLLERS)(void* hwnd, CONTROL controls[4]);
typedef void  (__cdecl*INITIATECONTROLLERS)(CONTROL_INFO control_info);
typedef void  (__cdecl*READCONTROLLER)(int controller, unsigned char* command);
typedef void  (__cdecl*ROMCLOSED_INPUT)();
typedef void  (__cdecl*ROMOPEN_INPUT)();
typedef void  (__cdecl*KEYDOWN)(unsigned int wParam, long lParam);
typedef void  (__cdecl*KEYUP)(unsigned int wParam, long lParam);
extern CLOSEDLL_INPUT closeDLL_input;
extern CONTROLLERCOMMAND controllerCommand;
extern GETKEYS getKeys;
extern SETKEYS setKeys;
extern OLD_INITIATECONTROLLERS old_initiateControllers;
extern INITIATECONTROLLERS initiateControllers;
extern READCONTROLLER readController;
extern ROMCLOSED_INPUT romClosed_input;
extern ROMOPEN_INPUT romOpen_input;
extern KEYDOWN keyDown;
extern KEYUP keyUp;

#pragma endregion

#pragma region RSP Functions

typedef void		   (__cdecl*CLOSEDLL_RSP)();
typedef unsigned long  (__cdecl*DORSPCYCLES)(unsigned long);
typedef void		   (__cdecl*INITIATERSP)(RSP_INFO rsp_info, unsigned long* cycles);
typedef void		   (__cdecl*ROMCLOSED_RSP)();
extern CLOSEDLL_RSP closeDLL_RSP;
extern DORSPCYCLES doRspCycles;
extern INITIATERSP initiateRSP;
extern ROMCLOSED_RSP romClosed_RSP;

#pragma endregion


// ReSharper restore CppInconsistentNaming

// frame buffer plugin spec extension
typedef struct
{
	unsigned long addr;
	unsigned long size;
	unsigned long width;
	unsigned long height;
} FrameBufferInfo;

class Plugin
{
public:
	/**
	 * \brief Tries to create a plugin from the given path
	 * \param path The path to a plugin
	 * \return A pointer to the plugin, or nothing if the plugin couldn't be created
	 */
	static std::optional<std::unique_ptr<Plugin>> create(std::filesystem::path path);

	Plugin() = default;
	~Plugin();

	/**
	 * \brief Opens the plugin configuration dialog
	 */
	void config();

	/**
	 * \brief Opens the plugin test dialog
	 */
	void test();

	/**
	 * \brief Opens the plugin about dialog
	 */
	void about();

	/**
	 * \brief Loads the plugin's exported functions into the globals
	 */
	void load_into_globals();

	/**
	 * \brief Gets the plugin's path
	 */
	auto path() const
	{
		return m_path;
	}

	/**
	 * \brief Gets the plugin's name
	 */
	auto name() const
	{
		return m_name;
	}

	/**
	 * \brief Gets the plugin's type
	 */
	auto type() const
	{
		return m_type;
	}

	/**
	 * \brief Gets the plugin's version
	 */
	auto version() const
	{
		return m_version;
	}

private:
	std::filesystem::path m_path;
	std::string m_name;
	plugin_type m_type;
	uint16_t m_version;
	void* m_module;
};

/**
 * \brief Gets all available plugins
 */
std::vector<std::unique_ptr<Plugin>> get_available_plugins();


/// <summary>
/// Initializes dummy info used by per-plugin functions
/// </summary>
void setup_dummy_info();

/**
 * \brief Whether the MGE compositor is available
 */
inline bool is_mge_available()
{
	return get_video_size && read_video;
}
