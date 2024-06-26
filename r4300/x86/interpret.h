/**
 * Mupen64 - interpret.h
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

#ifndef INTERPRET_H
#define INTERPRET_H

//#define INTERPRET_J
//#define INTERPRET_J_OUT
//#define INTERPRET_J_IDLE
//#define INTERPRET_JAL
//#define INTERPRET_JAL_OUT
//#define INTERPRET_JAL_IDLE
//#define INTERPRET_BEQ
//#define INTERPRET_BEQ_OUT
//#define INTERPRET_BEQ_IDLE
//#define INTERPRET_BNE
//#define INTERPRET_BNE_OUT
//#define INTERPRET_BNE_IDLE
//#define INTERPRET_BLEZ
//#define INTERPRET_BLEZ_OUT
//#define INTERPRET_BLEZ_IDLE
//#define INTERPRET_BGTZ
//#define INTERPRET_BGTZ_OUT
//#define INTERPRET_BGTZ_IDLE
//#define INTERPRET_ADDI
//#define INTERPRET_ADDIU
//#define INTERPRET_SLTI
//#define INTERPRET_SLTIU
//#define INTERPRET_ANDI
//#define INTERPRET_ORI
//#define INTERPRET_XORI
//#define INTERPRET_LUI
//#define INTERPRET_BEQL
//#define INTERPRET_BEQL_OUT
//#define INTERPRET_BEQL_IDLE
//#define INTERPRET_BNEL
//#define INTERPRET_BNEL_OUT
//#define INTERPRET_BNEL_IDLE
//#define INTERPRET_BLEZL
//#define INTERPRET_BLEZL_OUT
//#define INTERPRET_BLEZL_IDLE
//#define INTERPRET_BGTZL
//#define INTERPRET_BGTZL_OUT
//#define INTERPRET_BGTZL_IDLE
//#define INTERPRET_DADDI
//#define INTERPRET_DADDIU
//#define INTERPRET_LB
//#define INTERPRET_LH
//#define INTERPRET_LW
//#define INTERPRET_LBU
//#define INTERPRET_LHU
//#define INTERPRET_LWU
//#define INTERPRET_SB
//#define INTERPRET_SH
//#define INTERPRET_SW
//#define INTERPRET_LWC1
//#define INTERPRET_LDC1
//#define INTERPRET_LD
//#define INTERPRET_SWC1
//#define INTERPRET_SDC1
//#define INTERPRET_SD
//#define INTERPRET_SLL
//#define INTERPRET_SRL
//#define INTERPRET_SRA
//#define INTERPRET_SLLV
//#define INTERPRET_SRLV
//#define INTERPRET_SRAV
//#define INTERPRET_JR
//#define INTERPRET_JALR
//#define INTERPRET_SYSCALL
//#define INTERPRET_MFHI
//#define INTERPRET_MTHI
//#define INTERPRET_MFLO
//#define INTERPRET_MTLO
//#define INTERPRET_DSLLV
//#define INTERPRET_DSRLV
//#define INTERPRET_DSRAV
//#define INTERPRET_MULT
//#define INTERPRET_MULTU
//#define INTERPRET_DIV
//#define INTERPRET_DIVU
//#define INTERPRET_DMULTU
//#define INTERPRET_ADD
//#define INTERPRET_ADDU
//#define INTERPRET_SUB
//#define INTERPRET_SUBU
//#define INTERPRET_AND
//#define INTERPRET_OR
//#define INTERPRET_XOR
//#define INTERPRET_NOR
//#define INTERPRET_SLT
//#define INTERPRET_SLTU
//#define INTERPRET_DADD
//#define INTERPRET_DADDU
//#define INTERPRET_DSUB
//#define INTERPRET_DSUBU
//#define INTERPRET_DSLL
//#define INTERPRET_DSRL
//#define INTERPRET_DSRA
//#define INTERPRET_DSLL32
//#define INTERPRET_DSRL32
//#define INTERPRET_DSRA32
//#define INTERPRET_BLTZ
//#define INTERPRET_BLTZ_OUT
//#define INTERPRET_BLTZ_IDLE
//#define INTERPRET_BGEZ
//#define INTERPRET_BGEZ_OUT
//#define INTERPRET_BGEZ_IDLE
//#define INTERPRET_BLTZL
//#define INTERPRET_BLTZL_OUT
//#define INTERPRET_BLTZL_IDLE
//#define INTERPRET_BGEZL
//#define INTERPRET_BGEZL_OUT
//#define INTERPRET_BGEZL_IDLE
//#define INTERPRET_BLTZAL
//#define INTERPRET_BLTZAL_OUT
//#define INTERPRET_BLTZAL_IDLE
//#define INTERPRET_BGEZAL
//#define INTERPRET_BGEZAL_OUT
//#define INTERPRET_BGEZAL_IDLE
//#define INTERPRET_BLTZALL
//#define INTERPRET_BLTZALL_OUT
//#define INTERPRET_BLTZALL_IDLE
//#define INTERPRET_BGEZALL
//#define INTERPRET_BGEZALL_OUT
//#define INTERPRET_BGEZALL_IDLE
//#define INTERPRET_BC1F
//#define INTERPRET_BC1F_OUT
//#define INTERPRET_BC1F_IDLE
//#define INTERPRET_BC1T
//#define INTERPRET_BC1T_OUT
//#define INTERPRET_BC1T_IDLE
//#define INTERPRET_BC1FL
//#define INTERPRET_BC1FL_OUT
//#define INTERPRET_BC1FL_IDLE
//#define INTERPRET_BC1TL
//#define INTERPRET_BC1TL_OUT
//#define INTERPRET_BC1TL_IDLE
//#define INTERPRET_MFC1
//#define INTERPRET_DMFC1
//#define INTERPRET_CFC1
//#define INTERPRET_MTC1
//#define INTERPRET_DMTC1
//#define INTERPRET_CTC1
//#define INTERPRET_ADD_D
//#define INTERPRET_SUB_D
//#define INTERPRET_MUL_D
//#define INTERPRET_DIV_D
//#define INTERPRET_SQRT_D
//#define INTERPRET_ABS_D
//#define INTERPRET_MOV_D
//#define INTERPRET_NEG_D
//#define INTERPRET_ROUND_L_D
//#define INTERPRET_TRUNC_L_D
//#define INTERPRET_CEIL_L_D
//#define INTERPRET_FLOOR_L_D
//#define INTERPRET_ROUND_W_D
//#define INTERPRET_TRUNC_W_D
//#define INTERPRET_CEIL_W_D
//#define INTERPRET_FLOOR_W_D
//#define INTERPRET_CVT_S_D
//#define INTERPRET_CVT_W_D
//#define INTERPRET_CVT_L_D
//#define INTERPRET_C_F_D
//#define INTERPRET_C_UN_D
//#define INTERPRET_C_EQ_D
//#define INTERPRET_C_UEQ_D
//#define INTERPRET_C_OLT_D
//#define INTERPRET_C_ULT_D
//#define INTERPRET_C_OLE_D
//#define INTERPRET_C_ULE_D
//#define INTERPRET_C_SF_D
//#define INTERPRET_C_NGLE_D
//#define INTERPRET_C_SEQ_D
//#define INTERPRET_C_NGL_D
//#define INTERPRET_C_LT_D
//#define INTERPRET_C_NGE_D
//#define INTERPRET_C_LE_D
//#define INTERPRET_C_NGT_D
//#define INTERPRET_CVT_S_L
//#define INTERPRET_CVT_D_L
//#define INTERPRET_CVT_S_W
//#define INTERPRET_CVT_D_W
//#define INTERPRET_ADD_S
//#define INTERPRET_SUB_S
//#define INTERPRET_MUL_S
//#define INTERPRET_DIV_S
//#define INTERPRET_SQRT_S
//#define INTERPRET_ABS_S
//#define INTERPRET_MOV_S
//#define INTERPRET_NEG_S
//#define INTERPRET_ROUND_L_S
//#define INTERPRET_TRUNC_L_S
//#define INTERPRET_CEIL_L_S
//#define INTERPRET_FLOOR_L_S
//#define INTERPRET_ROUND_W_S
//#define INTERPRET_TRUNC_W_S
//#define INTERPRET_CEIL_W_S
//#define INTERPRET_FLOOR_W_S
//#define INTERPRET_CVT_D_S
//#define INTERPRET_CVT_W_S
//#define INTERPRET_CVT_L_S
//#define INTERPRET_C_F_S
//#define INTERPRET_C_UN_S
//#define INTERPRET_C_EQ_S
//#define INTERPRET_C_UEQ_S
//#define INTERPRET_C_OLT_S
//#define INTERPRET_C_ULT_S
//#define INTERPRET_C_OLE_S
//#define INTERPRET_C_ULE_S
//#define INTERPRET_C_SF_S
//#define INTERPRET_C_NGLE_S
//#define INTERPRET_C_SEQ_S
//#define INTERPRET_C_NGL_S
//#define INTERPRET_C_LT_S
//#define INTERPRET_C_NGE_S
//#define INTERPRET_C_LE_S
//#define INTERPRET_C_NGT_S

#endif // INTERPRET_H
