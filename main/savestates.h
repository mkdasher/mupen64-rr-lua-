/**
 * Mupen64 - savestates.h
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
#pragma once

#include <filesystem>

enum class e_st_job
{
	none,
	save,
	load
};

enum class e_st_medium
{
	slot,
	path,
	memory
};

extern size_t st_slot;
extern std::filesystem::path st_path;
extern e_st_job savestates_job;
extern e_st_medium st_medium;
extern bool st_skip_dma;
extern bool old_st;
extern bool savestates_job_success;

/**
 * \brief Gets the path to the save directory
 */
std::filesystem::path get_saves_directory();

/**
 * \brief Gets the path to the current rom's SRAM file
 */
std::filesystem::path get_sram_path();

/**
 * \brief Gets the path to the current rom's EEPROM file
 */
std::filesystem::path get_eeprom_path();

/**
 * \brief Gets the path to the current rom's flashram file
 */
std::filesystem::path get_flashram_path();

/**
 * \brief Gets the path to the current rom's mempak file
 */
std::filesystem::path get_mempak_path();

/**
 * \brief Reads emu state and generates a savestate depending on the st global state
 * \remarks If <c>savestates_job_use_slot</c> is specified, <c>st_slot</c> will be used to construct the savestate's path, otherwise <c>st_path</c> will be used
 *
 * This function should only be called by the core, other callers shall use <c>savestates_exec</c>
 */
void savestates_save_immediate();

/**
 * \brief Overwrites emu state from a read savestate depending on the st global state
 * \remarks If <c>savestates_job_use_slot</c> is specified, <c>st_slot</c> will be used to construct the savestate's path, otherwise <c>st_path</c> will be used
 *
 * This function should only be called by the core, other callers shall use <c>savestates_exec</c>
 */
void savestates_load_immediate();

/**
 * \brief Executes a savestate operation
 * \param path The savestate's path
 * \param job The job to set
 * \remarks The operation won't complete immediately
 */
void savestates_do(const std::filesystem::path& path, e_st_job job);

/**
 * \brief Executes a savestate operation
 * \param slot The slot to construct the savestate path with
 * \param job The job to set
 * \remarks The operation won't complete immediately
 */
void savestates_do(size_t slot, e_st_job job);
