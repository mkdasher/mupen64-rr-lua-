/**
 * Mupen64 - cop1_d.c
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

#include <math.h>
#include <float.h>

#include "r4300.h"
#include "ops.h"
#include "macros.h"
#include "cop1_helpers.h"
#include <shared/Config.hpp>

void ADD_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] +
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	PC++;
}

void SUB_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] -
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	PC++;
}

void MUL_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] *
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	PC++;
}

void DIV_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] /
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	PC++;
}

void SQRT_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	*reg_cop1_double[core_cffd] = sqrt(*reg_cop1_double[core_cffs]);
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	PC++;
}

void ABS_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	*reg_cop1_double[core_cffd] = fabs(*reg_cop1_double[core_cffs]);
	// ABS cannot fail
	PC++;
}

void MOV_D()
{
	if (check_cop1_unusable()) return;
	// MOV is not an arithmetic instruction, no check needed
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs];
	PC++;
}

void NEG_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	*reg_cop1_double[core_cffd] = -(*reg_cop1_double[core_cffs]);
	// NEG cannot fail
	PC++;
}

void ROUND_L_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_round_to_nearest();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	PC++;
}

void TRUNC_L_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_trunc();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	PC++;
}

void CEIL_L_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_ceil();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	PC++;
}

void FLOOR_L_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_floor();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	PC++;
}

void ROUND_W_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_round_to_nearest();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_simple[core_cffd]);
	set_rounding();
	PC++;
}

void TRUNC_W_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_trunc();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_simple[core_cffd]);
	set_rounding();
	PC++;
}

void CEIL_W_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_ceil();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_simple[core_cffd]);
	set_rounding();
	PC++;
}

void FLOOR_W_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_floor();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_simple[core_cffd]);
	set_rounding();
	PC++;
}

void CVT_S_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	if (g_config.is_round_towards_zero_enabled)
	{
		set_trunc();
	}
	*reg_cop1_simple[core_cffd] = *reg_cop1_double[core_cffs];
	if (g_config.is_round_towards_zero_enabled)
	{
		set_rounding();
	}
	CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
	PC++;
}

void CVT_W_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_simple[core_cffd]);
	PC++;
}

void CVT_L_D()
{
	if (check_cop1_unusable()) return;
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	PC++;
}

void C_F_D()
{
	if (check_cop1_unusable()) return;
	FCR31 &= ~0x800000;
	PC++;
}

void C_UN_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_EQ_D()
{
	if (check_cop1_unusable()) return;
	if (!isnan(*reg_cop1_double[core_cffs]) && !isnan(*reg_cop1_double[core_cfft]) &&
		*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_UEQ_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]) ||
		*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_OLT_D()
{
	if (check_cop1_unusable()) return;
	if (!isnan(*reg_cop1_double[core_cffs]) && !isnan(*reg_cop1_double[core_cfft]) &&
		*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_ULT_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]) ||
		*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_OLE_D()
{
	if (check_cop1_unusable()) return;
	if (!isnan(*reg_cop1_double[core_cffs]) && !isnan(*reg_cop1_double[core_cfft]) &&
		*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_ULE_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]) ||
		*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_SF_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	FCR31 &= ~0x800000;
	PC++;
}

void C_NGLE_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	FCR31 &= ~0x800000;
	PC++;
}

void C_SEQ_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_NGL_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_LT_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_NGE_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_LE_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}

void C_NGT_D()
{
	if (check_cop1_unusable()) return;
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	PC++;
}
