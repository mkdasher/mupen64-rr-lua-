/**
 * Mupen64 - cop1_s.c
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

#include <cmath>

#include "r4300.h"
#include "ops.h"
#include "macros.h"
#include "cop1_helpers.h"

void ADD_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    CHECK_INPUT(*reg_cop1_simple[core_cfft]);
    *reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] +
        *reg_cop1_simple[core_cfft];
    CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
    PC++;
}

void SUB_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    CHECK_INPUT(*reg_cop1_simple[core_cfft]);
    *reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] -
        *reg_cop1_simple[core_cfft];
    CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
    PC++;
}

void MUL_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    CHECK_INPUT(*reg_cop1_simple[core_cfft]);
    *reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] *
        *reg_cop1_simple[core_cfft];
    CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
    PC++;
}

void DIV_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    CHECK_INPUT(*reg_cop1_simple[core_cfft]);
    *reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] /
        *reg_cop1_simple[core_cfft];
    CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
    PC++;
}

void SQRT_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    *reg_cop1_simple[core_cffd] = sqrt(*reg_cop1_simple[core_cffs]);
    CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
    PC++;
}

void ABS_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    *reg_cop1_simple[core_cffd] = fabs(*reg_cop1_simple[core_cffs]);
    // ABS cannot fail
    PC++;
}

void MOV_S()
{
    if (check_cop1_unusable()) return;
    // MOV is not an arithmetic instruction, no check needed
    *reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs];
    PC++;
}

void NEG_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    *reg_cop1_simple[core_cffd] = -(*reg_cop1_simple[core_cffs]);
    // NEG cannot fail
    PC++;
}

void ROUND_L_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_round_to_nearest()
    clear_x87_exceptions()
    FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void TRUNC_L_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_trunc()
    clear_x87_exceptions()
    FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void CEIL_L_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_ceil()
    clear_x87_exceptions()
    FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void FLOOR_L_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_floor()
    clear_x87_exceptions()
    FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void ROUND_W_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_round_to_nearest()
    clear_x87_exceptions()
    FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_simple[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void TRUNC_W_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_trunc()
    clear_x87_exceptions()
    FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_simple[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void CEIL_W_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_ceil()
    clear_x87_exceptions()
    FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_simple[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void FLOOR_W_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    set_floor()
    clear_x87_exceptions()
    FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_simple[core_cffd])
    set_rounding()
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void CVT_D_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    *reg_cop1_double[core_cffd] = *reg_cop1_simple[core_cffs];
    PC++;
}

void CVT_W_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    clear_x87_exceptions()
    FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_simple[core_cffd])
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void CVT_L_S()
{
    if (check_cop1_unusable()) return;
    CHECK_INPUT(*reg_cop1_simple[core_cffs]);
    clear_x87_exceptions()
    FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd])
    CHECK_CONVERT_EXCEPTIONS();
    PC++;
}

void C_F_S()
{
    if (check_cop1_unusable()) return;
    FCR31 &= ~0x800000;
    PC++;
}

void C_UN_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_EQ_S()
{
    if (check_cop1_unusable()) return;
    if (!isnan(*reg_cop1_simple[core_cffs]) && !isnan(*reg_cop1_simple[core_cfft]) &&
        *reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_UEQ_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]) ||
        *reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_OLT_S()
{
    if (check_cop1_unusable()) return;
    if (!isnan(*reg_cop1_simple[core_cffs]) && !isnan(*reg_cop1_simple[core_cfft]) &&
        *reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_ULT_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]) ||
        *reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_OLE_S()
{
    if (check_cop1_unusable()) return;
    if (!isnan(*reg_cop1_simple[core_cffs]) && !isnan(*reg_cop1_simple[core_cfft]) &&
        *reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_ULE_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]) ||
        *reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_SF_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    FCR31 &= ~0x800000;
    PC++;
}

void C_NGLE_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    FCR31 &= ~0x800000;
    PC++;
}

void C_SEQ_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    if (*reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_NGL_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    if (*reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_LT_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    if (*reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_NGE_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    if (*reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_LE_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    if (*reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}

void C_NGT_S()
{
    if (check_cop1_unusable()) return;
    if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
    {
        printf("Invalid operation exception in C opcode\n");
        stop = 1;
    }
    if (*reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
        FCR31 |= 0x800000;
    else FCR31 &= ~0x800000;
    PC++;
}
