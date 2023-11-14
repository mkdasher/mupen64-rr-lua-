/***************************************************************************
						  commandline.c  -  description
							 -------------------
	copyright            : (C) 2003 by ShadowPrince
	email                : shadow@emulation64.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Based on code from 1964 by Schibo and Rice
// Slightly improved command line params parsing function to work with spaced arguments
#include <Windows.h>
#include "Commandline.h"
#include "LuaConsole.h"
#include "main_win.h"
#include "savestates.h"
#include "vcr.h"
#include "lib/argh.h"

std::string commandline_rom;
std::string commandline_lua;
std::string commandline_st;
std::string commandline_movie;
std::string commandline_avi;
bool commandline_stop_capture_on_movie_end;
bool commandline_stop_emu_on_movie_end;


void commandline_set()
{
	const argh::parser cmdl(__argc, __argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

	commandline_rom = cmdl("--rom", "").str();
	commandline_lua = cmdl("--lua", "").str();
	commandline_st = cmdl("--st", "").str();
	commandline_movie = cmdl("--movie", "").str();
	commandline_avi = cmdl("--avi", "").str();
	commandline_stop_capture_on_movie_end = cmdl["--stop-capture-on-movie-end"];
	commandline_stop_emu_on_movie_end = cmdl["--stop-emu-on-movie-end"];

	// handle "Open With...":
	if (cmdl.size() == 2 && cmdl.params().empty())
	{
		commandline_rom = cmdl[1];
	}
}

void commandline_start_rom()
{
	if (commandline_rom.empty())
	{
		return;
	}

	strcpy(rom_path, commandline_rom.c_str());
	CreateThread(nullptr, 0, start_rom, nullptr, 0, &start_rom_id);
}

void commandline_load_st()
{
	if (commandline_st.empty())
	{
		return;
	}

	savestates_do(commandline_st.c_str(), e_st_job::load);
}

void commandline_start_lua()
{
	if (commandline_lua.empty())
	{
		return;
	}

	lua_create_and_run(commandline_lua.c_str(), false);
}

void commandline_start_movie()
{
	if (commandline_movie.empty())
	{
		return;
	}

	vcr_start_playback(commandline_movie, false);
}

void commandline_start_capture()
{
	if (commandline_avi.empty())
	{
		return;
	}

	vcr_start_capture(commandline_avi.c_str(), false);
}

void commandline_on_movie_playback_stop()
{
	if (commandline_stop_capture_on_movie_end && vcr_is_capturing())
	{
		vcr_stop_capture();
	}

	if (commandline_stop_capture_on_movie_end)
	{
		SendMessage(main_hwnd, WM_DESTROY, 0, 0);
	}
}
