/**
 * Mupen64 - savestates.c
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

#include <zlib.h>
#include <stdlib.h>
#include <string.h>

#include "../lua/LuaConsole.h"

#include "vcr.h"

#include "savestates.h"
#include "guifuncs.h"
#include "rom.h"
#include "../memory/memory.h"
#include "../memory/flashram.h"
#include "../memory/summercart.h"
#include "../r4300/r4300.h"
#include "../r4300/interupt.h"
#include "win/main_win.h"
#include "win/features/Statusbar.hpp"

e_st_job st_job = e_st_job::none;
std::filesystem::path st_job_path;

int savestates_job_success = 1;
size_t current_slot = 1;

/**
* \brief .st that comes from no delay fix mupen, it has some differences compared to new st:
*
* one frame of input is "embedded", that is the pif ram holds already fetched controller info.
*
* execution continues at exception handler (after input poll) at 0x80000180.
 */
bool old_st;

/**
 * \brief this is a switch to enable/disable fixing .st to work for old mupen (and m64plus)
 * read savestate save function for more info, currently hardcoded on
 */
bool fix_new_st = true;

bool st_skip_dma = false;

#define MUPEN64NEW_ST_FIXED (1<<31) //last bit seems to be free

/// <summary>
/// overwrites emu memory with given data. Make sure to call load_eventqueue_infos as well!!!
/// </summary>
/// <param name="firstBlock"></param>
void load_memory_from_buffer(char* p)
{
    memread(&p, &rdram_register, sizeof(RDRAM_register));
    if (rdram_register.rdram_device_manuf & MUPEN64NEW_ST_FIXED)
    {
        rdram_register.rdram_device_manuf &= ~MUPEN64NEW_ST_FIXED; //remove the trick
        st_skip_dma = true; //tell dma.c to skip it
    }
    memread(&p, &MI_register, sizeof(mips_register));
    memread(&p, &pi_register, sizeof(PI_register));
    memread(&p, &sp_register, sizeof(SP_register));
    memread(&p, &rsp_register, sizeof(RSP_register));
    memread(&p, &si_register, sizeof(SI_register));
    memread(&p, &vi_register, sizeof(VI_register));
    memread(&p, &ri_register, sizeof(RI_register));
    memread(&p, &ai_register, sizeof(AI_register));
    memread(&p, &dpc_register, sizeof(DPC_register));
    memread(&p, &dps_register, sizeof(DPS_register));
    memread(&p, rdram, 0x800000);
    memread(&p, SP_DMEM, 0x1000);
    memread(&p, SP_IMEM, 0x1000);
    memread(&p, PIF_RAM, 0x40);

    char buf[4 * 32];
    memread(&p, buf, 24);
    load_flashram_infos(buf);

    memread(&p, tlb_LUT_r, 0x100000);
    memread(&p, tlb_LUT_w, 0x100000);

    memread(&p, &llbit, 4);
    memread(&p, reg, 32 * 8);
    for (int i = 0; i < 32; i++)
    {
        memread(&p, reg_cop0 + i, 4);
        memread(&p, buf, 4); // for compatibility with old versions purpose
    }
    memread(&p, &lo, 8);
    memread(&p, &hi, 8);
    memread(&p, reg_cop1_fgr_64, 32 * 8);
    memread(&p, &FCR0, 4);
    memread(&p, &FCR31, 4);
    memread(&p, tlb_e, 32 * sizeof(tlb));
    if (!dynacore && interpcore)
        memread(&p, &interp_addr, 4);
    else
    {
        uint32_t target_addr;
        memread(&p, &target_addr, 4);
        for (int i = 0; i < 0x100000; i++)
            invalid_code[i] = 1;
        jump_to(target_addr)
    }

    memread(&p, &next_interupt, 4);
    memread(&p, &next_vi, 4);
    memread(&p, &vi_field, 4);
}


std::vector<uint8_t> savestates_save_buffer()
{
	t_buffer_io b = {
		.data = {},
		.offset = 0
	};

    savestates_job_success = TRUE;

	bwrite(&b, ROM_SETTINGS.MD5, 32);

    //if fixing enabled...
    if (fix_new_st)
    {
        //this is code taken from dma.c:dma_si_read(), it finishes up the dma.
        //it copies data from pif (should contain commands and controller states), updates count reg and adds SI interrupt to queue
        //so why does old mupen and mupen64plus dislike savestates without doing this? well in case of mario 64 it leaves pif command buffer uninitialised
        //and it never can poll input properly (hence the inability to frame advance which is handled inside controller read).

        //But we dont want to do this then load such .st and dma again... so I notify mupen about this in .st,
        //since .st is filled up to the brim with data (not even a single unused offset) I have to store one bit in... rdram manufacturer register
        //this 99.999% wont break on any game, and that bit will be cleared by mupen64plus converter as well, so only old old mupen ever sees this trick.

        //update: I stumbled upon a .st that had the bit set, but didn't have SI_INT in queue,
        //so it froze game, so there exists a way to cause that somehow
        for (size_t i = 0; i < (64 / 4); i++)
            rdram[si_register.si_dram_addr / 4 + i] = sl(PIF_RAM[i]);
        update_count();
        add_interupt_event(SI_INT, 0x900);
        rdram_register.rdram_device_manuf |= MUPEN64NEW_ST_FIXED;
        st_skip_dma = true;
    }

    bwrite(&b, &rdram_register, sizeof(RDRAM_register));
    bwrite(&b, &MI_register, sizeof(mips_register));
    bwrite(&b, &pi_register, sizeof(PI_register));
    bwrite(&b, &sp_register, sizeof(SP_register));
    bwrite(&b, &rsp_register, sizeof(RSP_register));
    bwrite(&b, &si_register, sizeof(SI_register));
    bwrite(&b, &vi_register, sizeof(VI_register));
    bwrite(&b, &ri_register, sizeof(RI_register));
    bwrite(&b, &ai_register, sizeof(AI_register));
    bwrite(&b, &dpc_register, sizeof(DPC_register));
    bwrite(&b, &dps_register, sizeof(DPS_register));
    bwrite(&b, rdram, 0x800000);
    bwrite(&b, SP_DMEM, 0x1000);
    bwrite(&b, SP_IMEM, 0x1000);
    bwrite(&b, PIF_RAM, 0x40);

	{
		char tmp[1024];
		save_flashram_infos(tmp);
		bwrite(&b, tmp, 24);
	}

    bwrite(&b, tlb_LUT_r, 0x100000);
    bwrite(&b, tlb_LUT_w, 0x100000);

    bwrite(&b, &llbit, 4);
    bwrite(&b, reg, 32 * 8);
    for (size_t i = 0; i < 32; i++)
        bwrite(&b, reg_cop0 + i, 8); // *8 for compatibility with old versions purpose
    bwrite(&b, &lo, 8);
    bwrite(&b, &hi, 8);
    bwrite(&b, reg_cop1_fgr_64, 32 * 8);
    bwrite(&b, &FCR0, 4);
    bwrite(&b, &FCR31, 4);
    bwrite(&b, tlb_e, 32 * sizeof(tlb));
    if (!dynacore && interpcore)
        bwrite(&b, &interp_addr, 4);
    else
        bwrite(&b, &PC->addr, 4);

    bwrite(&b, &next_interupt, 4);
    bwrite(&b, &next_vi, 4);
    bwrite(&b, &vi_field, 4);

	{
		char tmp[1024];
		size_t len = save_eventqueue_infos(tmp);
		bwrite(&b, tmp, len);
	}

    // re-recording
    bool movie_active = VCR_isActive();
    bwrite(&b, &movie_active, sizeof(movie_active));

    if (movie_active)
    {
        char* movie_freeze_buf = nullptr;
        unsigned long movie_freeze_size = 0;

        VCR_movieFreeze(&movie_freeze_buf, &movie_freeze_size);
        if (movie_freeze_buf)
        {
            bwrite(&b, &movie_freeze_size, sizeof(movie_freeze_size));
            bwrite(&b, movie_freeze_buf, movie_freeze_size);
            free(movie_freeze_buf);
        }
        else
        {
            printf("Failed to save movie snapshot.\n");
            savestates_job_success = FALSE;
        }
    }
	return b.data;
}

void savestates_load_buffer(const std::vector<uint8_t>& buf)
{
	t_buffer_io b = {
		.data =  buf,
		.offset = 0,
	};
	/*rough .st format :
    0x0 - 0xA02BB0 : memory, registers, stuff like that, known size
    0xA02BB4 - ??? : interrupt queue, dynamic size (cap 1kB)
    ??? - ??????   : m64 info, also dynamic, no cap
    More precise info can be seen on github
    */
    constexpr int BUFLEN = 1024;
    constexpr int firstBlockSize = 0xA02BB4 - 32; //32 is md5 hash
    char o_buf[BUFLEN];
    int len;

    savestates_job_success = TRUE;

    // compare current rom hash with one stored in state
    bread(&b, o_buf, 32);
    if (memcmp(o_buf, ROM_SETTINGS.MD5, 32) != 0)
    {
        if (Config.is_rom_movie_compatibility_check_enabled)
        {
        	statusbar_send_text("Loading mismatched savestate");
        }
        else
        {
        	MessageBox(mainHWND, "This savestate is from another ROM or version.", nullptr, MB_ICONERROR | MB_OK);
            savestates_job_success = FALSE;
            return;
        }
    }

    // new version does one bigass gzread for first part of .st (static size)
	// ReSharper disable once CppLocalVariableMayBeConst
	auto first_block = static_cast<char*>(malloc(firstBlockSize));
    bread(&b, first_block, firstBlockSize);
    // now read interrupt queue into buf
    for (len = 0; len < BUFLEN; len += 8)
    {
        bread(&b, o_buf + len, 4);
        if (*reinterpret_cast<unsigned long*>(&o_buf[len]) == 0xFFFFFFFF)
            break;
        bread(&b, o_buf + len + 4, 4);
    }

    if (len == BUFLEN)
    {
        // Exhausted the buffer and still no terminator. Prevents the buffer overflow "Queuecrush".
        savestates_job_success = FALSE;
        free(first_block);
        printf( "Snapshot event queue terminator not reached.\n");
    	MessageBox(mainHWND, "This savestate's event queue is too long.", nullptr, MB_ICONERROR | MB_OK);
        return;
    }

    // now read movie info if exists
    uint32_t isMovie;
    bread(&b, &isMovie, sizeof(isMovie));

    if (!isMovie) // this .st is not part of a movie
    {
        if (VCR_isActive())
        {
            if (!Config.is_state_independent_state_loading_allowed)
            {
                printf("Can't load a non-movie snapshot while a movie is active.\n");
    			MessageBox(mainHWND, "This savestate doesn't have a movie associated with it, but a movie is currently playing.", nullptr, MB_ICONERROR | MB_OK);
                free(first_block);
                savestates_job_success = FALSE;
                return;
            }
            statusbar_send_text("Loading non-movie savestate, this can desync playback");
        }

        // at this point we know the savestate is safe to be loaded (done after else block)
    }
    else
    {
        // this .st is part of a movie
        // rest of the file can be gzreaded now

        // hash matches, load and verify rest of the data
        unsigned long movie_input_data_size = 0;
        bread(&b, &movie_input_data_size, sizeof(movie_input_data_size));
        auto local_movie_data = (char*)malloc(movie_input_data_size);
        bread(&b, local_movie_data, movie_input_data_size);
        const int result = VCR_movieUnfreeze(local_movie_data, movie_input_data_size);
        free(local_movie_data);
    	// TODO: make this into helper function
        if (result != SUCCESS && !VCR_isIdle())
        {
            bool stop = false;
            char err_str[1024];
            strcpy(err_str, "Failed to load movie snapshot,\n");
            switch (result)
            {
            case NOT_FROM_THIS_MOVIE:
                strcat(err_str, "snapshot not from this movie\n");
                break;
            case NOT_FROM_A_MOVIE:
                strcat(err_str, "not a movie snapshot\n");
                break; // shouldn't get here...
            case INVALID_FRAME:
                strcat(err_str, "invalid frame number\n");
                break;
            case WRONG_FORMAT:
                strcat(err_str, "wrong format\n");
                stop = true;
                break;
            default:
                break;
            }
            if (!Config.is_state_independent_state_loading_allowed)
            {
                printWarning(err_str);
                if (stop && VCR_isRecording()) VCR_stopRecord(1);
                else if (stop) VCR_stopPlayback();
                savestates_job_success = FALSE;
                goto failedLoad;
            }
            statusbar_send_text("Warning: loading non-movie savestate can break recording");
        }
    }

    // so far loading success! overwrite memory
    load_eventqueue_infos(o_buf);
    load_memory_from_buffer(first_block);
    free(first_block);

failedLoad:
    extern bool ignore;
    //legacy .st fix, makes BEQ instruction ignore jump, because .st writes new address explictly.
    //This should cause issues anyway but libultra seems to be flexible (this means there's a chance it fails).
    //For safety, load .sts in dynarec because it completely avoids this issue by being differently coded
    old_st = (interp_addr == 0x80000180 || PC->addr == 0x80000180);
    //doubled because can't just reuse this variable
    if (interp_addr == 0x80000180 || (PC->addr == 0x80000180 && !dynacore))
        ignore = true;
    if (!dynacore && interpcore)
    {
        //printf(".st jump: %x, stopped here:%x\n", interp_addr, last_addr);
        last_addr = interp_addr;
    }
    else
    {
        //printf(".st jump: %x, stopped here:%x\n", PC->addr, last_addr);
        last_addr = PC->addr;
    }
}


#undef BUFLEN

std::string slot_to_path(size_t slot)
{
	return std::string(get_savespath()) + std::string(reinterpret_cast<const char*>(ROM_HEADER->nom)) + ".st";
}

void savestates_save(size_t slot, bool immediate)
{
	savestates_save(slot_to_path(slot), immediate);
}

void savestates_load(size_t slot, bool immediate)
{
	savestates_load(slot_to_path(slot), immediate);
}

void savestates_save(std::filesystem::path path, bool immediate)
{
	if (!immediate)
	{
		st_job = e_st_job::save;
		st_job_path = path;
		return;
	}

	if (Config.use_summercart) save_summercart(path.string().c_str());
	auto buf = savestates_save_buffer();
	write_file_buffer(path, buf);

	main_dispatcher_invoke(AtSaveStateLuaCallback);
}

void savestates_load(std::filesystem::path path, bool immediate)
{
	if (!immediate)
	{
		st_job = e_st_job::load;
		st_job_path = path;
		return;
	}

	if (Config.use_summercart) load_summercart(path.string().c_str());
	savestates_load_buffer(read_file_buffer(path));
	main_dispatcher_invoke(AtLoadStateLuaCallback);
}
