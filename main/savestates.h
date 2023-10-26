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

extern bool st_skip_dma;

/**
 * \brief Saves a savestate to the specified slot
 * \param slot The slot to save to
 * \param immediate Whether the operation should be performed immediately
 */
void savestates_save(size_t slot, bool immediate);

/**
 * \brief Loads a savestate from the specified slot
 * \param slot The slot to load from
 * \param immediate Whether the operation should be performed immediately
 */
void savestates_load(size_t slot, bool immediate);

/**
 * \brief Saves a savestate to the specified path
 * \param path The path to save to
 * \param immediate Whether the operation should be performed immediately
 */
void savestates_save(std::filesystem::path path, bool immediate);

/**
 * \brief Loads a savestate from the specified path
 * \param path The path to load from
 * \param immediate Whether the operation should be performed immediately
 */
void savestates_load(std::filesystem::path path, bool immediate);
