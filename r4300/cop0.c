/**
 * Mupen64 - cop0.c
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

#include "r4300.h"
#include "macros.h"
#include "ops.h"
#include "interrupt.h"

void MFC0()
{
    switch (PC->f.r.nrd)
    {
    case 1:
        printf("lecture de Random\n");
        stop = 1;
    default:
        rrt32 = (long)reg_cop0[PC->f.r.nrd];
        sign_extended(core_rrt);
    }
    PC++;
}

void MTC0()
{
    switch (PC->f.r.nrd)
    {
    case 0: // Index
        core_Index = core_rrt & 0x8000003F;
        if ((core_Index & 0x3F) > 31)
        {
            printf("il y a plus de 32 TLB\n");
            stop = 1;
        }
        break;
    case 1: // Random
        break;
    case 2: // EntryLo0
        core_EntryLo0 = core_rrt & 0x3FFFFFFF;
        break;
    case 3: // EntryLo1
        core_EntryLo1 = core_rrt & 0x3FFFFFFF;
        break;
    case 4: // Context
        core_Context = (core_rrt & 0xFF800000) | (core_Context & 0x007FFFF0);
        break;
    case 5: // PageMask
        core_PageMask = core_rrt & 0x01FFE000;
        break;
    case 6: // Wired
        core_Wired = core_rrt;
        core_Random = 31;
        break;
    case 8: // BadVAddr
        break;
    case 9: // Count
        update_count();
        if (next_interrupt <= core_Count) gen_interrupt();
        debug_count += core_Count;
        translate_event_queue(core_rrt & 0xFFFFFFFF);
        core_Count = core_rrt & 0xFFFFFFFF;
        debug_count -= core_Count;
        break;
    case 10: // EntryHi
        core_EntryHi = core_rrt & 0xFFFFE0FF;
        break;
    case 11: // Compare
        update_count();
        remove_event(COMPARE_INT);
        add_interrupt_event_count(COMPARE_INT, (unsigned long)core_rrt);
        core_Compare = core_rrt;
        core_Cause = core_Cause & 0xFFFF7FFF; //Timer interrupt is clear
        break;
    case 12: // Status
        if ((core_rrt & 0x04000000) != (core_Status & 0x04000000))
        {
            if (core_rrt & 0x04000000)
            {
	            for (int i = 0; i < 32; i++)
                {
                    reg_cop1_double[i] = (double*)&reg_cop1_fgr_64[i];
                    reg_cop1_simple[i] = (float*)&reg_cop1_fgr_64[i];
                }
            }
            else
            {
                int i;
                for (i = 0; i < 32; i++)
                {
                    if (!(i & 1))
                        reg_cop1_double[i] = (double*)&reg_cop1_fgr_64[i >> 1];
#ifndef _BIG_ENDIAN
                    reg_cop1_simple[i] = (float*)&reg_cop1_fgr_64[i >> 1] + (i & 1);
#else
						reg_cop1_simple[i] = (float*)&reg_cop1_fgr_64[i >> 1] + (1 - (i & 1));
#endif
                }
            }
        }
        core_Status = core_rrt;
        PC++;
        check_interrupt();
        update_count();
        if (next_interrupt <= core_Count) gen_interrupt();
        PC--;
        break;
    case 13: // Cause
        if (core_rrt != 0)
        {
            printf("écriture dans Cause\n");
            stop = 1;
        }
        else
            core_Cause = core_rrt;
        break;
    case 14: // EPC
        core_EPC = core_rrt;
        break;
    case 15: // PRevID
        break;
    case 16: // Config
        core_Config_cop0 = core_rrt;
        break;
    case 18: // WatchLo
        core_WatchLo = core_rrt & 0xFFFFFFFF;
        break;
    case 19: // WatchHi
        core_WatchHi = core_rrt & 0xFFFFFFFF;
        break;
    case 27: // CacheErr
        break;
    case 28: // TagLo
        core_TagLo = core_rrt & 0x0FFFFFC0;
        break;
    case 29: // TagHi
        core_TagHi = 0;
        break;
    default:
        printf("unknown mtc0 write : %d\n", PC->f.r.nrd);
        stop = 1;
    }
    PC++;
}
