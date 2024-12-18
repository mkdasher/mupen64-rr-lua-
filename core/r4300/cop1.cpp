/**
 * Mupen64 - cop1.c
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

#include "ops.h"
#include "r4300.h"
#include "macros.h"
#include "exception.h"
#include "../memory/memory.h"

void MFC1()
{
    if (check_cop1_unusable()) return;
    rrt32 = *((int32_t*)reg_cop1_simple[core_rfs]);
    sign_extended(core_rrt);
    PC++;
}

void DMFC1()
{
    if (check_cop1_unusable()) return;
    core_rrt = *((int64_t*)reg_cop1_double[core_rfs]);
    PC++;
}

void CFC1()
{
    if (check_cop1_unusable()) return;
    if (core_rfs == 31)
    {
        rrt32 = FCR31;
        sign_extended(core_rrt);
    }
    if (core_rfs == 0)
    {
        rrt32 = FCR0;
        sign_extended(core_rrt);
    }
    PC++;
}

void MTC1()
{
    if (check_cop1_unusable()) return;
    *((int32_t*)reg_cop1_simple[core_rfs]) = rrt32;
    PC++;
}

void DMTC1()
{
    if (check_cop1_unusable()) return;
    *((int64_t*)reg_cop1_double[core_rfs]) = core_rrt;
    PC++;
}

void CTC1()
{
    if (check_cop1_unusable()) return;
    if (core_rfs == 31)
        FCR31 = rrt32;
    switch ((FCR31 & 3))
    {
    case 0:
        rounding_mode = ROUND_MODE;
        break;
    case 1:
        rounding_mode = TRUNC_MODE;
        break;
    case 2:
        rounding_mode = CEIL_MODE;
        break;
    case 3:
        rounding_mode = FLOOR_MODE;
        break;
    }
    set_rounding();
    //if ((FCR31 >> 7) & 0x1F) g_core_logger->info("FPU Exception enabled : {:#06x}",
    //				   (int32_t)((FCR31 >> 7) & 0x1F));
    PC++;
}
