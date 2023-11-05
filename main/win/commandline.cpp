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
#include "commandline.h"

#include <iostream>

#include "LuaConsole.h"
#include "main_win.h"
#include "savestates.h"
#include "vcr.h"
#include "helpers/string_helpers.h"
#include "lib/argh.h"

std::string commandline_rom;
std::string commandline_lua;
std::string commandline_st;
std::string commandline_movie;

//To get a command line parameter if available, please pass a flag
// Flags:
//	"-v"	-> return video plugin name
//	"-a"	-> return audio plugin name
//  "-c"	-> return controller plugin name
//  "-g"	-> return game name to run
//	"-f"	-> return play-in-full-screen flag
//	"-r"	-> return rom path
//  "-nogui"-> nogui mode
//  "-save" -> save options on exit

//  "-m64"  -> play m64 from path, requires -g
//  "-avi"  -> capture m64 to avi, requires -m64
//  "-lua"  -> play a lua script from path, requires -g
//  "-st"   -> load a savestate from path, requires -g, cant have -m64

void commandline_set()
{
	argh::parser cmdl(__argc, __argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

	commandline_rom = cmdl("--rom", "").str();
	commandline_lua = cmdl("--lua", "").str();
	commandline_st = cmdl("--st", "").str();
	commandline_movie = cmdl("--movie", "").str();

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
	CreateThread(NULL, 0, start_rom, nullptr, 0, &start_rom_id);
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

	VCR_startPlayback(commandline_movie, nullptr, nullptr);
}
