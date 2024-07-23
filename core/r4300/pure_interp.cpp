/**
 * Mupen64 - pure_interp.c
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


#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "r4300.h"
#include "exception.h"
#include "../memory/memory.h"
#include "macros.h"
#include "interrupt.h"
#include "cop1_helpers.h"
#include "rom.h"

#include "../memory/tlb.h"

#include "tracelog.h"
#include <shared/Config.hpp>

#include "debugger.h"

#ifdef DBG
extern int debugger_mode;
extern void update_debugger();
#endif

unsigned long interp_addr;
unsigned long vr_op;
static long skip;

bool ignore;

void prefetch();

extern void (*interp_ops[])(void);

extern unsigned long next_vi;

static void NI()
{
	printf("NI:%x\n", (unsigned int)vr_op);
	stop = 1;
}

static void SLL()
{
	rrd32 = (unsigned long)(rrt32) << core_rsa;
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void SRL()
{
	rrd32 = (unsigned long)rrt32 >> core_rsa;
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void SRA()
{
	rrd32 = (signed long)rrt32 >> core_rsa;
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void SLLV()
{
	rrd32 = (unsigned long)(rrt32) << (rrs32 & 0x1F);
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void SRLV()
{
	rrd32 = (unsigned long)rrt32 >> (rrs32 & 0x1F);
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void SRAV()
{
	rrd32 = (signed long)rrt32 >> (rrs32 & 0x1F);
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void JR()
{
	local_rs32 = irs32;
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	interp_addr = local_rs32;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void JALR()
{
	unsigned long long int* dest = (unsigned long long int*)PC->f.r.rd;
	local_rs32 = rrs32;
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (!skip_jump)
	{
		*dest = interp_addr;
		sign_extended(*dest);

		interp_addr = local_rs32;
	}
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void SYSCALL()
{
	core_Cause = 8 << 2;
	exception_general();
}

static void SYNC()
{
#ifdef LUA_BREAKPOINTSYNC_PURE
    LuaBreakpointSyncPure();
#else
	interp_addr += 4;
#endif
}

static void MFHI()
{
	core_rrd = hi;
	interp_addr += 4;
}

static void MTHI()
{
	hi = core_rrs;
	interp_addr += 4;
}

static void MFLO()
{
	core_rrd = lo;
	interp_addr += 4;
}

static void MTLO()
{
	lo = core_rrs;
	interp_addr += 4;
}

static void DSLLV()
{
	core_rrd = core_rrt << (rrs32 & 0x3F);
	interp_addr += 4;
}

static void DSRLV()
{
	core_rrd = (unsigned long long)core_rrt >> (rrs32 & 0x3F);
	interp_addr += 4;
}

static void DSRAV()
{
	core_rrd = (long long)core_rrt >> (rrs32 & 0x3F);
	interp_addr += 4;
}

static void MULT()
{
	long long int temp;
	temp = core_rrs * core_rrt;
	hi = temp >> 32;
	lo = temp;
	sign_extended(lo);
	interp_addr += 4;
}

static void MULTU()
{
	unsigned long long int temp;
	temp = (unsigned long)core_rrs * (unsigned long long)((unsigned long)core_rrt);
	hi = (long long)temp >> 32;
	lo = temp;
	sign_extended(lo);
	interp_addr += 4;
}

static void DIV()
{
	if (rrt32)
	{
		lo = rrs32 / rrt32;
		hi = rrs32 % rrt32;
		sign_extended(lo);
		sign_extended(hi);
	} else printf("div\n");
	interp_addr += 4;
}

static void DIVU()
{
	if (rrt32)
	{
		lo = (unsigned long)rrs32 / (unsigned long)rrt32;
		hi = (unsigned long)rrs32 % (unsigned long)rrt32;
		sign_extended(lo);
		sign_extended(hi);
	} else printf("divu\n");
	interp_addr += 4;
}

static void DMULT()
{
	unsigned long long int op1, op2, op3, op4;
	unsigned long long int result1, result2, result3, result4;
	unsigned long long int temp1, temp2, temp3, temp4;
	int sign = 0;

	if (core_rrs < 0)
	{
		op2 = -core_rrs;
		sign = 1 - sign;
	} else op2 = core_rrs;
	if (core_rrt < 0)
	{
		op4 = -core_rrt;
		sign = 1 - sign;
	} else op4 = core_rrt;

	op1 = op2 & 0xFFFFFFFF;
	op2 = (op2 >> 32) & 0xFFFFFFFF;
	op3 = op4 & 0xFFFFFFFF;
	op4 = (op4 >> 32) & 0xFFFFFFFF;

	temp1 = op1 * op3;
	temp2 = (temp1 >> 32) + op1 * op4;
	temp3 = op2 * op3;
	temp4 = (temp3 >> 32) + op2 * op4;

	result1 = temp1 & 0xFFFFFFFF;
	result2 = temp2 + (temp3 & 0xFFFFFFFF);
	result3 = (result2 >> 32) + temp4;
	result4 = (result3 >> 32);

	lo = result1 | (result2 << 32);
	hi = (result3 & 0xFFFFFFFF) | (result4 << 32);
	if (sign)
	{
		hi = ~hi;
		if (!lo) hi++;
		else lo = ~lo + 1;
	}
	interp_addr += 4;
}

static void DMULTU()
{
	unsigned long long int op1, op2, op3, op4;
	unsigned long long int result1, result2, result3, result4;
	unsigned long long int temp1, temp2, temp3, temp4;

	op1 = core_rrs & 0xFFFFFFFF;
	op2 = (core_rrs >> 32) & 0xFFFFFFFF;
	op3 = core_rrt & 0xFFFFFFFF;
	op4 = (core_rrt >> 32) & 0xFFFFFFFF;

	temp1 = op1 * op3;
	temp2 = (temp1 >> 32) + op1 * op4;
	temp3 = op2 * op3;
	temp4 = (temp3 >> 32) + op2 * op4;

	result1 = temp1 & 0xFFFFFFFF;
	result2 = temp2 + (temp3 & 0xFFFFFFFF);
	result3 = (result2 >> 32) + temp4;
	result4 = (result3 >> 32);

	lo = result1 | (result2 << 32);
	hi = (result3 & 0xFFFFFFFF) | (result4 << 32);

	interp_addr += 4;
}

static void DDIV()
{
	if (core_rrt)
	{
		lo = (long long int)core_rrs / (long long int)core_rrt;
		hi = (long long int)core_rrs % (long long int)core_rrt;
	} else printf("ddiv\n");
	interp_addr += 4;
}

static void DDIVU()
{
	if (core_rrt)
	{
		lo = (unsigned long long int)core_rrs / (unsigned long long int)core_rrt;
		hi = (unsigned long long int)core_rrs % (unsigned long long int)core_rrt;
	} else printf("ddivu\n");
	interp_addr += 4;
}

static void ADD()
{
	rrd32 = rrs32 + rrt32;
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void ADDU()
{
	rrd32 = rrs32 + rrt32;
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void SUB()
{
	rrd32 = rrs32 - rrt32;
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void SUBU()
{
	rrd32 = rrs32 - rrt32;
	sign_extended(core_rrd);
	interp_addr += 4;
}

static void AND()
{
	core_rrd = core_rrs & core_rrt;
	interp_addr += 4;
}

static void OR()
{
	core_rrd = core_rrs | core_rrt;
	interp_addr += 4;
}

static void XOR()
{
	core_rrd = core_rrs ^ core_rrt;
	interp_addr += 4;
}

static void NOR()
{
	core_rrd = ~(core_rrs | core_rrt);
	interp_addr += 4;
}

static void SLT()
{
	if (core_rrs < core_rrt)
		core_rrd = 1;
	else
		core_rrd = 0;
	interp_addr += 4;
}

static void SLTU()
{
	if ((unsigned long long)core_rrs < (unsigned long long)core_rrt)
		core_rrd = 1;
	else
		core_rrd = 0;
	interp_addr += 4;
}

static void DADD()
{
	core_rrd = core_rrs + core_rrt;
	interp_addr += 4;
}

static void DADDU()
{
	core_rrd = core_rrs + core_rrt;
	interp_addr += 4;
}

static void DSUB()
{
	core_rrd = core_rrs - core_rrt;
	interp_addr += 4;
}

static void DSUBU()
{
	core_rrd = core_rrs - core_rrt;
	interp_addr += 4;
}

static void TEQ()
{
	if (core_rrs == core_rrt)
	{
		printf("trap exception in teq\n");
		stop = 1;
	}
	interp_addr += 4;
}

static void DSLL()
{
	core_rrd = core_rrt << core_rsa;
	interp_addr += 4;
}

static void DSRL()
{
	core_rrd = (unsigned long long)core_rrt >> core_rsa;
	interp_addr += 4;
}

static void DSRA()
{
	core_rrd = core_rrt >> core_rsa;
	interp_addr += 4;
}

static void DSLL32()
{
	core_rrd = core_rrt << (32 + core_rsa);
	interp_addr += 4;
}

static void DSRL32()
{
	core_rrd = (unsigned long long int)core_rrt >> (32 + core_rsa);
	interp_addr += 4;
}

static void DSRA32()
{
	core_rrd = (signed long long int)core_rrt >> (32 + core_rsa);
	interp_addr += 4;
}

static void (*interp_special[64])(void) =
{
	SLL, NI, SRL, SRA, SLLV, NI, SRLV, SRAV,
	JR, JALR, NI, NI, SYSCALL, NI, NI, SYNC,
	MFHI, MTHI, MFLO, MTLO, DSLLV, NI, DSRLV, DSRAV,
	MULT, MULTU, DIV, DIVU, DMULT, DMULTU, DDIV, DDIVU,
	ADD, ADDU, SUB, SUBU, AND, OR, XOR, NOR,
	NI, NI, SLT, SLTU, DADD, DADDU, DSUB, DSUBU,
	NI, NI, NI, NI, TEQ, NI, NI, NI,
	DSLL, NI, DSRL, DSRA, DSLL32, NI, DSRL32, DSRA32
};

static void BLTZ()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (local_rs < 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (local_rs < 0)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BGEZ()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (local_rs >= 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (local_rs >= 0)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BLTZL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (core_irs < 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if (core_irs < 0)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else interp_addr += 8;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BGEZL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (core_irs >= 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if (core_irs >= 0)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else interp_addr += 8;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BLTZAL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	reg[31] = interp_addr + 8;
	if ((&core_irs) != (reg + 31))
	{
		if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
			if (local_rs < 0)
			{
				if (probe_nop(interp_addr + 4))
				{
					update_count();
					skip = next_interrupt - core_Count;
					if (skip > 3)
					{
						core_Count += (skip & 0xFFFFFFFC);
						return;
					}
				}
			}
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		if (local_rs < 0)
			interp_addr += (local_immediate - 1) * 4;
	} else printf("erreur dans bltzal\n");
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BGEZAL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	reg[31] = interp_addr + 8;
	if ((&core_irs) != (reg + 31))
	{
		if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
			if (local_rs >= 0)
			{
				if (probe_nop(interp_addr + 4))
				{
					update_count();
					skip = next_interrupt - core_Count;
					if (skip > 3)
					{
						core_Count += (skip & 0xFFFFFFFC);
						return;
					}
				}
			}
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		if (local_rs >= 0)
			interp_addr += (local_immediate - 1) * 4;
	} else printf("erreur dans bgezal\n");
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BLTZALL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	reg[31] = interp_addr + 8;
	if ((&core_irs) != (reg + 31))
	{
		if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
			if (local_rs < 0)
			{
				if (probe_nop(interp_addr + 4))
				{
					update_count();
					skip = next_interrupt - core_Count;
					if (skip > 3)
					{
						core_Count += (skip & 0xFFFFFFFC);
						return;
					}
				}
			}
		if (local_rs < 0)
		{
			interp_addr += 4;
			delay_slot = 1;
			prefetch();
			interp_ops[((vr_op >> 26) & 0x3F)]();
			update_count();
			delay_slot = 0;
			interp_addr += (local_immediate - 1) * 4;
		} else interp_addr += 8;
	} else printf("erreur dans bltzall\n");
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BGEZALL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	reg[31] = interp_addr + 8;
	if ((&core_irs) != (reg + 31))
	{
		if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
			if (local_rs >= 0)
			{
				if (probe_nop(interp_addr + 4))
				{
					update_count();
					skip = next_interrupt - core_Count;
					if (skip > 3)
					{
						core_Count += (skip & 0xFFFFFFFC);
						return;
					}
				}
			}
		if (local_rs >= 0)
		{
			interp_addr += 4;
			delay_slot = 1;
			prefetch();
			interp_ops[((vr_op >> 26) & 0x3F)]();
			update_count();
			delay_slot = 0;
			interp_addr += (local_immediate - 1) * 4;
		} else interp_addr += 8;
	} else printf("erreur dans bgezall\n");
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void (*interp_regimm[32])(void) =
{
	BLTZ, BGEZ, BLTZL, BGEZL, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	BLTZAL, BGEZAL, BLTZALL, BGEZALL, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI
};

static void TLBR()
{
	int index;
	index = core_Index & 0x1F;
	core_PageMask = tlb_e[index].mask << 13;
	core_EntryHi = ((tlb_e[index].vpn2 << 13) | tlb_e[index].asid);
	core_EntryLo0 = (tlb_e[index].pfn_even << 6) | (tlb_e[index].c_even << 3)
		| (tlb_e[index].d_even << 2) | (tlb_e[index].v_even << 1)
		| tlb_e[index].g;
	core_EntryLo1 = (tlb_e[index].pfn_odd << 6) | (tlb_e[index].c_odd << 3)
		| (tlb_e[index].d_odd << 2) | (tlb_e[index].v_odd << 1)
		| tlb_e[index].g;
	interp_addr += 4;
}

static void TLBWI()
{
	unsigned int i;

	if (tlb_e[core_Index & 0x3F].v_even)
	{
		for (i = tlb_e[core_Index & 0x3F].start_even; i < tlb_e[core_Index & 0x3F].end_even; i++)
			tlb_LUT_r[i >> 12] = 0;
		if (tlb_e[core_Index & 0x3F].d_even)
			for (i = tlb_e[core_Index & 0x3F].start_even; i < tlb_e[core_Index & 0x3F].end_even; i++)
				tlb_LUT_w[i >> 12] = 0;
	}
	if (tlb_e[core_Index & 0x3F].v_odd)
	{
		for (i = tlb_e[core_Index & 0x3F].start_odd; i < tlb_e[core_Index & 0x3F].end_odd; i++)
			tlb_LUT_r[i >> 12] = 0;
		if (tlb_e[core_Index & 0x3F].d_odd)
			for (i = tlb_e[core_Index & 0x3F].start_odd; i < tlb_e[core_Index & 0x3F].end_odd; i++)
				tlb_LUT_w[i >> 12] = 0;
	}
	tlb_e[core_Index & 0x3F].g = (core_EntryLo0 & core_EntryLo1 & 1);
	tlb_e[core_Index & 0x3F].pfn_even = (core_EntryLo0 & 0x3FFFFFC0) >> 6;
	tlb_e[core_Index & 0x3F].pfn_odd = (core_EntryLo1 & 0x3FFFFFC0) >> 6;
	tlb_e[core_Index & 0x3F].c_even = (core_EntryLo0 & 0x38) >> 3;
	tlb_e[core_Index & 0x3F].c_odd = (core_EntryLo1 & 0x38) >> 3;
	tlb_e[core_Index & 0x3F].d_even = (core_EntryLo0 & 0x4) >> 2;
	tlb_e[core_Index & 0x3F].d_odd = (core_EntryLo1 & 0x4) >> 2;
	tlb_e[core_Index & 0x3F].v_even = (core_EntryLo0 & 0x2) >> 1;
	tlb_e[core_Index & 0x3F].v_odd = (core_EntryLo1 & 0x2) >> 1;
	tlb_e[core_Index & 0x3F].asid = (core_EntryHi & 0xFF);
	tlb_e[core_Index & 0x3F].vpn2 = (core_EntryHi & 0xFFFFE000) >> 13;
	//tlb_e[Index&0x3F].r = (EntryHi & 0xC000000000000000LL) >> 62;
	tlb_e[core_Index & 0x3F].mask = (core_PageMask & 0x1FFE000) >> 13;

	tlb_e[core_Index & 0x3F].start_even = tlb_e[core_Index & 0x3F].vpn2 << 13;
	tlb_e[core_Index & 0x3F].end_even = tlb_e[core_Index & 0x3F].start_even +
		(tlb_e[core_Index & 0x3F].mask << 12) + 0xFFF;
	tlb_e[core_Index & 0x3F].phys_even = tlb_e[core_Index & 0x3F].pfn_even << 12;

	if (tlb_e[core_Index & 0x3F].v_even)
	{
		if (tlb_e[core_Index & 0x3F].start_even < tlb_e[core_Index & 0x3F].end_even &&
			!(tlb_e[core_Index & 0x3F].start_even >= 0x80000000 &&
				tlb_e[core_Index & 0x3F].end_even < 0xC0000000) &&
			tlb_e[core_Index & 0x3F].phys_even < 0x20000000)
		{
			for (i = tlb_e[core_Index & 0x3F].start_even; i < tlb_e[core_Index & 0x3F].end_even; i++)
				tlb_LUT_r[i >> 12] = 0x80000000 |
					(tlb_e[core_Index & 0x3F].phys_even + (i - tlb_e[core_Index & 0x3F].start_even));
			if (tlb_e[core_Index & 0x3F].d_even)
				for (i = tlb_e[core_Index & 0x3F].start_even; i < tlb_e[core_Index & 0x3F].end_even; i++)
					tlb_LUT_w[i >> 12] = 0x80000000 |
						(tlb_e[core_Index & 0x3F].phys_even + (i - tlb_e[core_Index & 0x3F].start_even));
		}
	}

	tlb_e[core_Index & 0x3F].start_odd = tlb_e[core_Index & 0x3F].end_even + 1;
	tlb_e[core_Index & 0x3F].end_odd = tlb_e[core_Index & 0x3F].start_odd +
		(tlb_e[core_Index & 0x3F].mask << 12) + 0xFFF;
	tlb_e[core_Index & 0x3F].phys_odd = tlb_e[core_Index & 0x3F].pfn_odd << 12;

	if (tlb_e[core_Index & 0x3F].v_odd)
	{
		if (tlb_e[core_Index & 0x3F].start_odd < tlb_e[core_Index & 0x3F].end_odd &&
			!(tlb_e[core_Index & 0x3F].start_odd >= 0x80000000 &&
				tlb_e[core_Index & 0x3F].end_odd < 0xC0000000) &&
			tlb_e[core_Index & 0x3F].phys_odd < 0x20000000)
		{
			for (i = tlb_e[core_Index & 0x3F].start_odd; i < tlb_e[core_Index & 0x3F].end_odd; i++)
				tlb_LUT_r[i >> 12] = 0x80000000 |
					(tlb_e[core_Index & 0x3F].phys_odd + (i - tlb_e[core_Index & 0x3F].start_odd));
			if (tlb_e[core_Index & 0x3F].d_odd)
				for (i = tlb_e[core_Index & 0x3F].start_odd; i < tlb_e[core_Index & 0x3F].end_odd; i++)
					tlb_LUT_w[i >> 12] = 0x80000000 |
						(tlb_e[core_Index & 0x3F].phys_odd + (i - tlb_e[core_Index & 0x3F].start_odd));
		}
	}
	interp_addr += 4;
}

static void TLBWR()
{
	unsigned int i;
	update_count();
	core_Random = (core_Count / 2 % (32 - core_Wired)) + core_Wired;
	if (tlb_e[core_Random].v_even)
	{
		for (i = tlb_e[core_Random].start_even; i < tlb_e[core_Random].end_even; i++)
			tlb_LUT_r[i >> 12] = 0;
		if (tlb_e[core_Random].d_even)
			for (i = tlb_e[core_Random].start_even; i < tlb_e[core_Random].end_even; i++)
				tlb_LUT_w[i >> 12] = 0;
	}
	if (tlb_e[core_Random].v_odd)
	{
		for (i = tlb_e[core_Random].start_odd; i < tlb_e[core_Random].end_odd; i++)
			tlb_LUT_r[i >> 12] = 0;
		if (tlb_e[core_Random].d_odd)
			for (i = tlb_e[core_Random].start_odd; i < tlb_e[core_Random].end_odd; i++)
				tlb_LUT_w[i >> 12] = 0;
	}
	tlb_e[core_Random].g = (core_EntryLo0 & core_EntryLo1 & 1);
	tlb_e[core_Random].pfn_even = (core_EntryLo0 & 0x3FFFFFC0) >> 6;
	tlb_e[core_Random].pfn_odd = (core_EntryLo1 & 0x3FFFFFC0) >> 6;
	tlb_e[core_Random].c_even = (core_EntryLo0 & 0x38) >> 3;
	tlb_e[core_Random].c_odd = (core_EntryLo1 & 0x38) >> 3;
	tlb_e[core_Random].d_even = (core_EntryLo0 & 0x4) >> 2;
	tlb_e[core_Random].d_odd = (core_EntryLo1 & 0x4) >> 2;
	tlb_e[core_Random].v_even = (core_EntryLo0 & 0x2) >> 1;
	tlb_e[core_Random].v_odd = (core_EntryLo1 & 0x2) >> 1;
	tlb_e[core_Random].asid = (core_EntryHi & 0xFF);
	tlb_e[core_Random].vpn2 = (core_EntryHi & 0xFFFFE000) >> 13;
	//tlb_e[Random].r = (EntryHi & 0xC000000000000000LL) >> 62;
	tlb_e[core_Random].mask = (core_PageMask & 0x1FFE000) >> 13;

	tlb_e[core_Random].start_even = tlb_e[core_Random].vpn2 << 13;
	tlb_e[core_Random].end_even = tlb_e[core_Random].start_even +
		(tlb_e[core_Random].mask << 12) + 0xFFF;
	tlb_e[core_Random].phys_even = tlb_e[core_Random].pfn_even << 12;

	if (tlb_e[core_Random].v_even)
	{
		if (tlb_e[core_Random].start_even < tlb_e[core_Random].end_even &&
			!(tlb_e[core_Random].start_even >= 0x80000000 &&
				tlb_e[core_Random].end_even < 0xC0000000) &&
			tlb_e[core_Random].phys_even < 0x20000000)
		{
			for (i = tlb_e[core_Random].start_even; i < tlb_e[core_Random].end_even; i++)
				tlb_LUT_r[i >> 12] = 0x80000000 |
					(tlb_e[core_Random].phys_even + (i - tlb_e[core_Random].start_even));
			if (tlb_e[core_Random].d_even)
				for (i = tlb_e[core_Random].start_even; i < tlb_e[core_Random].end_even; i++)
					tlb_LUT_w[i >> 12] = 0x80000000 |
						(tlb_e[core_Random].phys_even + (i - tlb_e[core_Random].start_even));
		}
	}
	tlb_e[core_Random].start_odd = tlb_e[core_Random].end_even + 1;
	tlb_e[core_Random].end_odd = tlb_e[core_Random].start_odd +
		(tlb_e[core_Random].mask << 12) + 0xFFF;
	tlb_e[core_Random].phys_odd = tlb_e[core_Random].pfn_odd << 12;

	if (tlb_e[core_Random].v_odd)
	{
		if (tlb_e[core_Random].start_odd < tlb_e[core_Random].end_odd &&
			!(tlb_e[core_Random].start_odd >= 0x80000000 &&
				tlb_e[core_Random].end_odd < 0xC0000000) &&
			tlb_e[core_Random].phys_odd < 0x20000000)
		{
			for (i = tlb_e[core_Random].start_odd; i < tlb_e[core_Random].end_odd; i++)
				tlb_LUT_r[i >> 12] = 0x80000000 |
					(tlb_e[core_Random].phys_odd + (i - tlb_e[core_Random].start_odd));
			if (tlb_e[core_Random].d_odd)
				for (i = tlb_e[core_Random].start_odd; i < tlb_e[core_Random].end_odd; i++)
					tlb_LUT_w[i >> 12] = 0x80000000 |
						(tlb_e[core_Random].phys_odd + (i - tlb_e[core_Random].start_odd));
		}
	}
	interp_addr += 4;
}

static void TLBP()
{
	int i;
	core_Index |= 0x80000000;
	for (i = 0; i < 32; i++)
	{
		if (((tlb_e[i].vpn2 & (~tlb_e[i].mask)) ==
				(((core_EntryHi & 0xFFFFE000) >> 13) & (~tlb_e[i].mask))) &&
			((tlb_e[i].g) ||
				(tlb_e[i].asid == (core_EntryHi & 0xFF))))
		{
			core_Index = i;
			break;
		}
	}
	interp_addr += 4;
}

static void ERET()
{
	update_count();
	if (core_Status & 0x4)
	{
		printf("erreur dans ERET\n");
		stop = 1;
	} else
	{
		core_Status &= 0xFFFFFFFD;
		interp_addr = core_EPC;
	}
	llbit = 0;
	check_interrupt();
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void (*interp_tlb[64])(void) =
{
	NI, TLBR, TLBWI, NI, NI, NI, TLBWR, NI,
	TLBP, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	ERET, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI
};

static void MFC0()
{
	switch (PC->f.r.nrd)
	{
	case 1:
		printf("lecture de Random\n");
		stop = 1;
	default:
		rrt32 = reg_cop0[PC->f.r.nrd];
		sign_extended(core_rrt);
	}
	interp_addr += 4;
}

static void MTC0()
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
				int i;
				for (i = 0; i < 32; i++)
				{
					//reg_cop1_fgr_64[i]=reg_cop1_fgr_32[i];
					reg_cop1_double[i] = (double*)&reg_cop1_fgr_64[i];
					reg_cop1_simple[i] = (float*)&reg_cop1_fgr_64[i];
				}
			} else
			{
				int i;
				for (i = 0; i < 32; i++)
				{
					//reg_cop1_fgr_32[i]=reg_cop1_fgr_64[i]&0xFFFFFFFF;
					//if (i<16) reg_cop1_double[i*2]=(double*)&reg_cop1_fgr_32[i*2];
					//reg_cop1_double[i]=(double*)&reg_cop1_fgr_64[i & 0xFFFE];
					if (!(i & 1))
						reg_cop1_double[i] = (double*)&reg_cop1_fgr_64[i >> 1];
					//reg_cop1_double[i]=(double*)&reg_cop1_fgr_64[i];
					//reg_cop1_simple[i]=(float*)&reg_cop1_fgr_32[i];
					//reg_cop1_simple[i]=(float*)&reg_cop1_fgr_64[i & 0xFFFE]+(i&1);
#ifndef _BIG_ENDIAN
					reg_cop1_simple[i] = (float*)&reg_cop1_fgr_64[i >> 1] + (i & 1);
#else
						reg_cop1_simple[i] = (float*)&reg_cop1_fgr_64[i >> 1] + (1 - (i & 1));
#endif
				}
			}
		}
		core_Status = core_rrt;
		interp_addr += 4;
		check_interrupt();
		update_count();
		if (next_interrupt <= core_Count) gen_interrupt();
		interp_addr -= 4;
		break;
	case 13: // Cause
		if (core_rrt != 0)
		{
			printf("écriture dans Cause\n");
			stop = 1;
		} else
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
	interp_addr += 4;
}

static void TLB()
{
	interp_tlb[(vr_op & 0x3F)]();
}

static void (*interp_cop0[32])(void) =
{
	MFC0, NI, NI, NI, MTC0, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	TLB, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI
};

static void BC1F()
{
	short local_immediate = core_iimmediate;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if ((FCR31 & 0x800000) == 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if ((FCR31 & 0x800000) == 0)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BC1T()
{
	short local_immediate = core_iimmediate;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if ((FCR31 & 0x800000) != 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if ((FCR31 & 0x800000) != 0)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BC1FL()
{
	short local_immediate = core_iimmediate;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if ((FCR31 & 0x800000) == 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if ((FCR31 & 0x800000) == 0)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else
		interp_addr += 8;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BC1TL()
{
	short local_immediate = core_iimmediate;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if ((FCR31 & 0x800000) != 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if ((FCR31 & 0x800000) != 0)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else
		interp_addr += 8;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void (*interp_cop1_bc[4])(void) =
{
	BC1F, BC1T,
	BC1FL, BC1TL
};

static void ADD_S()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	CHECK_INPUT(*reg_cop1_simple[core_cfft]);
	*reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] +
		*reg_cop1_simple[core_cfft];
	CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
	interp_addr += 4;
}

static void SUB_S()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	CHECK_INPUT(*reg_cop1_simple[core_cfft]);
	*reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] -
		*reg_cop1_simple[core_cfft];
	CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
	interp_addr += 4;
}

static void MUL_S()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	CHECK_INPUT(*reg_cop1_simple[core_cfft]);
	*reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] *
		*reg_cop1_simple[core_cfft];
	CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
	interp_addr += 4;
}

static void DIV_S()
{
	if ((FCR31 & 0x400) && *reg_cop1_simple[core_cfft] == 0)
	{
		printf("div_s by 0\n");
	}
	set_rounding();
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	CHECK_INPUT(*reg_cop1_simple[core_cfft]);
	*reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs] /
		*reg_cop1_simple[core_cfft];
	CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
	interp_addr += 4;
}

static void SQRT_S()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	*reg_cop1_simple[core_cffd] = sqrt(*reg_cop1_simple[core_cffs]);
	CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
	interp_addr += 4;
}

static void ABS_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	*reg_cop1_simple[core_cffd] = fabs(*reg_cop1_simple[core_cffs]);
	interp_addr += 4;
}

static void MOV_S()
{
	*reg_cop1_simple[core_cffd] = *reg_cop1_simple[core_cffs];
	interp_addr += 4;
}

static void NEG_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	*reg_cop1_simple[core_cffd] = -(*reg_cop1_simple[core_cffs]);
	interp_addr += 4;
}

static void ROUND_L_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_round_to_nearest();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void TRUNC_L_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_trunc();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void CEIL_L_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_ceil();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void FLOOR_L_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_floor();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void ROUND_W_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_round_to_nearest();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void TRUNC_W_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_trunc();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void CEIL_W_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_ceil();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void FLOOR_W_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_floor();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void CVT_D_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	*reg_cop1_double[core_cffd] = *reg_cop1_simple[core_cffs];
	interp_addr += 4;
}

static void CVT_W_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_rounding();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_S(reg_cop1_simple[core_cffs], reg_cop1_simple[core_cffd]);
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void CVT_L_S()
{
	CHECK_INPUT(*reg_cop1_simple[core_cffs]);
	set_rounding();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_S(reg_cop1_simple[core_cffs], reg_cop1_simple[core_cffd]);
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void C_F_S()
{
	FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_UN_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_EQ_S()
{
	if (!isnan(*reg_cop1_simple[core_cffs]) && !isnan(*reg_cop1_simple[core_cfft]) &&
		*reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_UEQ_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]) ||
		*reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_OLT_S()
{
	if (!isnan(*reg_cop1_simple[core_cffs]) && !isnan(*reg_cop1_simple[core_cfft]) &&
		*reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_ULT_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]) ||
		*reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_OLE_S()
{
	if (!isnan(*reg_cop1_simple[core_cffs]) && !isnan(*reg_cop1_simple[core_cfft]) &&
		*reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_ULE_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]) ||
		*reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_SF_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGLE_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_SEQ_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGL_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_simple[core_cffs] == *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_LT_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGE_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_simple[core_cffs] < *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_LE_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGT_S()
{
	if (isnan(*reg_cop1_simple[core_cffs]) || isnan(*reg_cop1_simple[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_simple[core_cffs] <= *reg_cop1_simple[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void (*interp_cop1_s[64])(void) =
{
	ADD_S, SUB_S, MUL_S, DIV_S, SQRT_S, ABS_S, MOV_S, NEG_S,
	ROUND_L_S, TRUNC_L_S, CEIL_L_S, FLOOR_L_S, ROUND_W_S, TRUNC_W_S, CEIL_W_S, FLOOR_W_S,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, CVT_D_S, NI, NI, CVT_W_S, CVT_L_S, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	C_F_S, C_UN_S, C_EQ_S, C_UEQ_S, C_OLT_S, C_ULT_S, C_OLE_S, C_ULE_S,
	C_SF_S, C_NGLE_S, C_SEQ_S, C_NGL_S, C_LT_S, C_NGE_S, C_LE_S, C_NGT_S
};

static void ADD_D()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] +
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	interp_addr += 4;
}

static void SUB_D()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] -
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	interp_addr += 4;
}

static void MUL_D()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] *
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	interp_addr += 4;
}

static void DIV_D()
{
	if ((FCR31 & 0x400) && *reg_cop1_double[core_cfft] == 0)
	{
		//FCR31 |= 0x8020;
		/*FCR31 |= 0x8000;
		Cause = 15 << 2;
		exception_general();*/
		printf("div_d by 0\n");
		//return;
	}
	set_rounding();
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	CHECK_INPUT(*reg_cop1_double[core_cfft]);
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs] /
		*reg_cop1_double[core_cfft];
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	interp_addr += 4;
}

static void SQRT_D()
{
	set_rounding();
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	*reg_cop1_double[core_cffd] = sqrt(*reg_cop1_double[core_cffs]);
	CHECK_OUTPUT(*reg_cop1_double[core_cffd]);
	interp_addr += 4;
}

static void ABS_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	*reg_cop1_double[core_cffd] = fabs(*reg_cop1_double[core_cffs]);
	interp_addr += 4;
}

static void MOV_D()
{
	*reg_cop1_double[core_cffd] = *reg_cop1_double[core_cffs];
	interp_addr += 4;
}

static void NEG_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	*reg_cop1_double[core_cffd] = -(*reg_cop1_double[core_cffs]);
	interp_addr += 4;
}

static void ROUND_L_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_round_to_nearest();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void TRUNC_L_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_trunc();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	CHECK_CONVERT_EXCEPTIONS();
	set_rounding();
	interp_addr += 4;
}

static void CEIL_L_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_ceil();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void FLOOR_L_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_floor();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void ROUND_W_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_round_to_nearest();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void TRUNC_W_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_trunc();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void CEIL_W_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_ceil();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void FLOOR_W_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_floor();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	set_rounding();
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void CVT_S_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	if (g_config.is_round_towards_zero_enabled)
	{
		set_trunc();
	} else
	{
		set_rounding();
	}
	*reg_cop1_simple[core_cffd] = *reg_cop1_double[core_cffs];
	set_rounding();
	CHECK_OUTPUT(*reg_cop1_simple[core_cffd]);
	interp_addr += 4;
}

static void CVT_W_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_rounding();
	clear_x87_exceptions();
	FLOAT_CONVERT_W_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void CVT_L_D()
{
	CHECK_INPUT(*reg_cop1_double[core_cffs]);
	set_rounding();
	clear_x87_exceptions();
	FLOAT_CONVERT_L_D(reg_cop1_double[core_cffs], reg_cop1_double[core_cffd]);
	CHECK_CONVERT_EXCEPTIONS();
	interp_addr += 4;
}

static void C_F_D()
{
	FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_UN_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_EQ_D()
{
	if (!isnan(*reg_cop1_double[core_cffs]) && !isnan(*reg_cop1_double[core_cfft]) &&
		*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_UEQ_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]) ||
		*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_OLT_D()
{
	if (!isnan(*reg_cop1_double[core_cffs]) && !isnan(*reg_cop1_double[core_cfft]) &&
		*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_ULT_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]) ||
		*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_OLE_D()
{
	if (!isnan(*reg_cop1_double[core_cffs]) && !isnan(*reg_cop1_double[core_cfft]) &&
		*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_ULE_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]) ||
		*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_SF_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGLE_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_SEQ_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGL_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] == *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_LT_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGE_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] < *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_LE_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void C_NGT_D()
{
	if (isnan(*reg_cop1_double[core_cffs]) || isnan(*reg_cop1_double[core_cfft]))
	{
		printf("Invalid operation exception in C opcode\n");
		stop = 1;
	}
	if (*reg_cop1_double[core_cffs] <= *reg_cop1_double[core_cfft])
		FCR31 |= 0x800000;
	else FCR31 &= ~0x800000;
	interp_addr += 4;
}

static void (*interp_cop1_d[64])(void) =
{
	ADD_D, SUB_D, MUL_D, DIV_D, SQRT_D, ABS_D, MOV_D, NEG_D,
	ROUND_L_D, TRUNC_L_D, CEIL_L_D, FLOOR_L_D, ROUND_W_D, TRUNC_W_D, CEIL_W_D, FLOOR_W_D,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	CVT_S_D, NI, NI, NI, CVT_W_D, CVT_L_D, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	C_F_D, C_UN_D, C_EQ_D, C_UEQ_D, C_OLT_D, C_ULT_D, C_OLE_D, C_ULE_D,
	C_SF_D, C_NGLE_D, C_SEQ_D, C_NGL_D, C_LT_D, C_NGE_D, C_LE_D, C_NGT_D
};

static void CVT_S_W()
{
	set_rounding();
	*reg_cop1_simple[core_cffd] = *((long*)reg_cop1_simple[core_cffs]);
	interp_addr += 4;
}

static void CVT_D_W()
{
	set_rounding();
	*reg_cop1_double[core_cffd] = *((long*)reg_cop1_simple[core_cffs]);
	interp_addr += 4;
}

static void (*interp_cop1_w[64])(void) =
{
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	CVT_S_W, CVT_D_W, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI
};

static void CVT_S_L()
{
	set_rounding();
	*reg_cop1_simple[core_cffd] = *((long long*)(reg_cop1_double[core_cffs]));
	interp_addr += 4;
}

static void CVT_D_L()
{
	set_rounding();
	*reg_cop1_double[core_cffd] = *((long long*)(reg_cop1_double[core_cffs]));
	interp_addr += 4;
}

static void (*interp_cop1_l[64])(void) =
{
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	CVT_S_L, CVT_D_L, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI
};

static void MFC1()
{
	rrt32 = *((long*)reg_cop1_simple[core_rfs]);
	sign_extended(core_rrt);
	interp_addr += 4;
}

static void DMFC1()
{
	core_rrt = *((long long*)(reg_cop1_double[core_rfs]));
	interp_addr += 4;
}

static void CFC1()
{
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
	interp_addr += 4;
}

static void MTC1()
{
	*((long*)reg_cop1_simple[core_rfs]) = rrt32;
	interp_addr += 4;
}

static void DMTC1()
{
	*((long long*)reg_cop1_double[core_rfs]) = core_rrt;
	interp_addr += 4;
}

static void CTC1()
{
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
	//if ((FCR31 >> 7) & 0x1F) printf("FPU Exception enabled : %x\n",
	//				   (int)((FCR31 >> 7) & 0x1F));
	set_rounding();
	interp_addr += 4;
}

static void BC()
{
	interp_cop1_bc[(vr_op >> 16) & 3]();
}

static void S()
{
	interp_cop1_s[(vr_op & 0x3F)]();
}

static void D()
{
	interp_cop1_d[(vr_op & 0x3F)]();
}

static void W()
{
	interp_cop1_w[(vr_op & 0x3F)]();
}

static void L()
{
	interp_cop1_l[(vr_op & 0x3F)]();
}

static void (*interp_cop1[32])(void) =
{
	MFC1, DMFC1, CFC1, NI, MTC1, DMTC1, CTC1, NI,
	BC, NI, NI, NI, NI, NI, NI, NI,
	S, D, NI, NI, W, L, NI, NI,
	NI, NI, NI, NI, NI, NI, NI, NI
};

static void SPECIAL()
{
	interp_special[(vr_op & 0x3F)]();
}

static void REGIMM()
{
	interp_regimm[((vr_op >> 16) & 0x1F)]();
}

//skips idle loop and advances to next interrupt
#define SKIP_IDLE() \
if (probe_nop(interp_addr+4)) {\
	update_count();\
	skip = next_interrupt - core_Count; \
	if (skip > 3)\
	{\
		core_Count += (skip & 0xFFFFFFFC);\
		return;\
	}\
}

static void J()
{
	unsigned long naddr = (PC->f.j.inst_index << 2) | (interp_addr & 0xF0000000);
	if (naddr == interp_addr)
	{
		SKIP_IDLE()
	}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	interp_addr = naddr;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void JAL()
{
	unsigned long naddr = (PC->f.j.inst_index << 2) | (interp_addr & 0xF0000000);
	if (naddr == interp_addr)
	{
		SKIP_IDLE()
	}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (!skip_jump)
	{
		reg[31] = interp_addr;
		sign_extended(reg[31]);

		interp_addr = naddr;
	}
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BEQ()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	local_rt = core_irt;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr && local_rs == local_rt)
	{
		SKIP_IDLE()
	}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (local_rs == local_rt && !ignore)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BNE()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	local_rt = core_irt;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr && local_rs != local_rt)
	{
		SKIP_IDLE()
	}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (local_rs != local_rt)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BLEZ()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr && local_rs <= 0)
	{
		SKIP_IDLE()
	}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (local_rs <= 0)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BGTZ()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr && local_rs <= 0)
	{
		SKIP_IDLE()
	}
	interp_addr += 4;
	delay_slot = 1;
	prefetch();
	interp_ops[((vr_op >> 26) & 0x3F)]();
	update_count();
	delay_slot = 0;
	if (local_rs > 0)
		interp_addr += (local_immediate - 1) * 4;
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

#undef SKIP_IDLE

static void ADDI()
{
	irt32 = irs32 + core_iimmediate;
	sign_extended(core_irt);
	interp_addr += 4;
}

static void ADDIU()
{
	irt32 = irs32 + core_iimmediate;
	sign_extended(core_irt);
	interp_addr += 4;
}

static void SLTI()
{
	if (core_irs < core_iimmediate)
		core_irt = 1;
	else
		core_irt = 0;
	interp_addr += 4;
}

static void SLTIU()
{
	if ((unsigned long long)core_irs < (unsigned long long)((long long)core_iimmediate))
		core_irt = 1;
	else
		core_irt = 0;
	interp_addr += 4;
}

static void ANDI()
{
	core_irt = core_irs & (unsigned short)core_iimmediate;
	interp_addr += 4;
}

static void ORI()
{
	core_irt = core_irs | (unsigned short)core_iimmediate;
	interp_addr += 4;
}

static void XORI()
{
	core_irt = core_irs ^ (unsigned short)core_iimmediate;
	interp_addr += 4;
}

static void LUI()
{
	irt32 = core_iimmediate << 16;
	sign_extended(core_irt);
	interp_addr += 4;
}

static void COP0()
{
	interp_cop0[((vr_op >> 21) & 0x1F)]();
}

static void COP1()
{
	if (check_cop1_unusable()) return;
	interp_cop1[((vr_op >> 21) & 0x1F)]();
}

static void BEQL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	local_rt = core_irt;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (core_irs == core_irt)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if (local_rs == local_rt)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else
	{
		interp_addr += 8;
		update_count();
	}
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BNEL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	local_rt = core_irt;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (core_irs != core_irt)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if (local_rs != local_rt)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else
	{
		interp_addr += 8;
		update_count();
	}
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BLEZL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (core_irs <= 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if (local_rs <= 0)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else
	{
		interp_addr += 8;
		update_count();
	}
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void BGTZL()
{
	short local_immediate = core_iimmediate;
	local_rs = core_irs;
	if ((interp_addr + (local_immediate + 1) * 4) == interp_addr)
		if (core_irs > 0)
		{
			if (probe_nop(interp_addr + 4))
			{
				update_count();
				skip = next_interrupt - core_Count;
				if (skip > 3)
				{
					core_Count += (skip & 0xFFFFFFFC);
					return;
				}
			}
		}
	if (local_rs > 0)
	{
		interp_addr += 4;
		delay_slot = 1;
		prefetch();
		interp_ops[((vr_op >> 26) & 0x3F)]();
		update_count();
		delay_slot = 0;
		interp_addr += (local_immediate - 1) * 4;
	} else
	{
		interp_addr += 8;
		update_count();
	}
	last_addr = interp_addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

static void DADDI()
{
	core_irt = core_irs + core_iimmediate;
	interp_addr += 4;
}

static void DADDIU()
{
	core_irt = core_irs + core_iimmediate;
	interp_addr += 4;
}

static void LDL()
{
	unsigned long long int word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 7)
	{
	case 0:
		address = core_iimmediate + irs32;
		rdword = (unsigned long long int*)&core_irt;
		read_dword_in_memory();
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFF) | (word << 8);
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFF) | (word << 16);
		break;
	case 3:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFF) | (word << 24);
		break;
	case 4:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFF) | (word << 32);
		break;
	case 5:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFFLL) | (word << 40);
		break;
	case 6:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFFFFLL) | (word << 48);
		break;
	case 7:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFFFFFFLL) | (word << 56);
		break;
	}
}

static void LDR()
{
	unsigned long long int word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 7)
	{
	case 0:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFFFFFF00LL) | (word >> 56);
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFFFF0000LL) | (word >> 48);
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFF000000LL) | (word >> 40);
		break;
	case 3:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFFFF00000000LL) | (word >> 32);
		break;
	case 4:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFFFF0000000000LL) | (word >> 24);
		break;
	case 5:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFFFF000000000000LL) | (word >> 16);
		break;
	case 6:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		core_irt = (core_irt & 0xFF00000000000000LL) | (word >> 8);
		break;
	case 7:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = (unsigned long long int*)&core_irt;
		read_dword_in_memory();
		break;
	}
}

static void LB()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	read_byte_in_memory();
	sign_extendedb(core_irt);
}

static void LH()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	read_hword_in_memory();
	sign_extendedh(core_irt);
}

static void LWL()
{
	unsigned long long int word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 3)
	{
	case 0:
		address = core_iimmediate + irs32;
		rdword = (unsigned long long int*)&core_irt;
		read_word_in_memory();
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		core_irt = (core_irt & 0xFF) | (word << 8);
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		core_irt = (core_irt & 0xFFFF) | (word << 16);
		break;
	case 3:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		core_irt = (core_irt & 0xFFFFFF) | (word << 24);
		break;
	}
	sign_extended(core_irt);
}

static void LW()
{
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	interp_addr += 4;
	read_word_in_memory();
	sign_extended(core_irt);
}

static void LBU()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	read_byte_in_memory();
}

static void LHU()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	read_hword_in_memory();
}

static void LWR()
{
	unsigned long long int word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 3)
	{
	case 0:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFFFFFF00LL) | ((word >> 24) & 0xFF);
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFFFF0000LL) | ((word >> 16) & 0xFFFF);
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		core_irt = (core_irt & 0xFFFFFFFFFF000000LL) | ((word >> 8) & 0xFFFFFF);
		break;
	case 3:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = (unsigned long long int*)&core_irt;
		read_word_in_memory();
		sign_extended(core_irt);
	}
}

static void LWU()
{
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	interp_addr += 4;
	read_word_in_memory();
}

static void SB()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	g_byte = (unsigned char)(core_irt & 0xFF);
	write_byte_in_memory();
}

static void SH()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	hword = (unsigned short)(core_irt & 0xFFFF);
	write_hword_in_memory();
}

static void SWL()
{
	unsigned long long int old_word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 3)
	{
	case 0:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		word = (unsigned long)core_irt;
		write_word_in_memory();
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		word = ((unsigned long)core_irt >> 8) | (old_word & 0xFF000000);
		write_word_in_memory();
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		word = ((unsigned long)core_irt >> 16) | (old_word & 0xFFFF0000);
		write_word_in_memory();
		break;
	case 3:
		address = core_iimmediate + irs32;
		g_byte = (unsigned char)(core_irt >> 24);
		write_byte_in_memory();
		break;
	}
}

static void SW()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	word = (unsigned long)(core_irt & 0xFFFFFFFF);
	write_word_in_memory();
}

static void SDL()
{
	unsigned long long int old_word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 7)
	{
	case 0:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		dword = core_irt;
		write_dword_in_memory();
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = ((unsigned long long)core_irt >> 8) | (old_word & 0xFF00000000000000LL);
		write_dword_in_memory();
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = ((unsigned long long)core_irt >> 16) | (old_word & 0xFFFF000000000000LL);
		write_dword_in_memory();
		break;
	case 3:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = ((unsigned long long)core_irt >> 24) | (old_word & 0xFFFFFF0000000000LL);
		write_dword_in_memory();
		break;
	case 4:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = ((unsigned long long)core_irt >> 32) | (old_word & 0xFFFFFFFF00000000LL);
		write_dword_in_memory();
		break;
	case 5:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = ((unsigned long long)core_irt >> 40) | (old_word & 0xFFFFFFFFFF000000LL);
		write_dword_in_memory();
		break;
	case 6:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = ((unsigned long long)core_irt >> 48) | (old_word & 0xFFFFFFFFFFFF0000LL);
		write_dword_in_memory();
		break;
	case 7:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = ((unsigned long long)core_irt >> 56) | (old_word & 0xFFFFFFFFFFFFFF00LL);
		write_dword_in_memory();
		break;
	}
}

static void SDR()
{
	unsigned long long int old_word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 7)
	{
	case 0:
		address = core_iimmediate + irs32;
		rdword = &old_word;
		read_dword_in_memory();
		dword = (core_irt << 56) | (old_word & 0x00FFFFFFFFFFFFFFLL);
		write_dword_in_memory();
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = (core_irt << 48) | (old_word & 0x0000FFFFFFFFFFFFLL);
		write_dword_in_memory();
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = (core_irt << 40) | (old_word & 0x000000FFFFFFFFFFLL);
		write_dword_in_memory();
		break;
	case 3:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = (core_irt << 32) | (old_word & 0x00000000FFFFFFFFLL);
		write_dword_in_memory();
		break;
	case 4:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = (core_irt << 24) | (old_word & 0x0000000000FFFFFFLL);
		write_dword_in_memory();
		break;
	case 5:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = (core_irt << 16) | (old_word & 0x000000000000FFFFLL);
		write_dword_in_memory();
		break;
	case 6:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		dword = (core_irt << 8) | (old_word & 0x00000000000000FFLL);
		write_dword_in_memory();
		break;
	case 7:
		address = (core_iimmediate + irs32) & 0xFFFFFFF8;
		dword = core_irt;
		write_dword_in_memory();
		break;
	}
}

static void SWR()
{
	unsigned long long int old_word = 0;
	interp_addr += 4;
	switch ((core_iimmediate + irs32) & 3)
	{
	case 0:
		address = core_iimmediate + irs32;
		rdword = &old_word;
		read_word_in_memory();
		word = ((unsigned long)core_irt << 24) | (old_word & 0x00FFFFFF);
		write_word_in_memory();
		break;
	case 1:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		word = ((unsigned long)core_irt << 16) | (old_word & 0x0000FFFF);
		write_word_in_memory();
		break;
	case 2:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		word = ((unsigned long)core_irt << 8) | (old_word & 0x000000FF);
		write_word_in_memory();
		break;
	case 3:
		address = (core_iimmediate + irs32) & 0xFFFFFFFC;
		word = (unsigned long)core_irt;
		write_word_in_memory();
		break;
	}
}

static void CACHE()
{
	interp_addr += 4;
}

static void LL()
{
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	interp_addr += 4;
	read_word_in_memory();
	sign_extended(core_irt);
	llbit = 1;
}

static void LWC1()
{
	unsigned long long int temp;
	if (check_cop1_unusable()) return;
	interp_addr += 4;
	address = core_lfoffset + reg[core_lfbase];
	rdword = &temp;
	read_word_in_memory();
	*((long*)reg_cop1_simple[core_lfft]) = *rdword;
}

static void LDC1()
{
	if (check_cop1_unusable()) return;
	interp_addr += 4;
	address = core_lfoffset + reg[core_lfbase];
	rdword = (unsigned long long int*)reg_cop1_double[core_lfft];
	read_dword_in_memory();
}

static void LD()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	rdword = (unsigned long long int*)&core_irt;
	read_dword_in_memory();
}

static void SC()
{
	interp_addr += 4;
	if (llbit)
	{
		address = core_iimmediate + irs32;
		word = (unsigned long)(core_irt & 0xFFFFFFFF);
		write_word_in_memory();
		llbit = 0;
		core_irt = 1;
	} else
	{
		core_irt = 0;
	}
}

static void SWC1()
{
	if (check_cop1_unusable()) return;
	interp_addr += 4;
	address = core_lfoffset + reg[core_lfbase];
	word = *((long*)reg_cop1_simple[core_lfft]);
	write_word_in_memory();
}

static void SDC1()
{
	if (check_cop1_unusable()) return;
	interp_addr += 4;
	address = core_lfoffset + reg[core_lfbase];
	dword = *((unsigned long long*)reg_cop1_double[core_lfft]);
	write_dword_in_memory();
}

static void SD()
{
	interp_addr += 4;
	address = core_iimmediate + irs32;
	dword = core_irt;
	write_dword_in_memory();
}

void (*interp_ops[64])(void) =
{
	SPECIAL, REGIMM, J, JAL, BEQ, BNE, BLEZ, BGTZ,
	ADDI, ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI,
	COP0, COP1, NI, NI, BEQL, BNEL, BLEZL, BGTZL,
	DADDI, DADDIU, LDL, LDR, NI, NI, NI, NI,
	LB, LH, LWL, LW, LBU, LHU, LWR, LWU,
	SB, SH, SWL, SW, SDL, SDR, SWR, CACHE,
	LL, LWC1, NI, NI, NI, LDC1, NI, LD,
	SC, SWC1, NI, NI, NI, SDC1, NI, SD
};

//Get opcode from address (interp_address)
void prefetch()
{
	//static FILE *f = NULL;
	//static int line=1;
	//static int tlb_used = 0;
	//unsigned int comp;
	//if (!tlb_used)
	//{
	/*if (f==NULL) f = fopen("/mnt/windows/pcdeb.txt", "rb");
	fscanf(f, "%x", &comp);
	if (comp != interp_addr)
	  {
	     printf("diff@%x, line:%d\n", interp_addr, line);
	     stop=1;
	  }*/
	//line++;
	//if ((debug_count+Count) > 0x50fe000) printf("line:%d\n", line);
	/*if ((debug_count+Count) > 0xb70000)
	  printf("count:%x, add:%x, op:%x, l%d\n", (int)(Count+debug_count),
	     interp_addr, op, line);*/
	//}
	//printf("addr:%x\n", interp_addr);
	if ((interp_addr >= 0x80000000) && (interp_addr < 0xc0000000))
	{
		if (/*(interp_addr >= 0x80000000) && */(interp_addr < 0x80800000))
		{
			vr_op = *(unsigned long*)&((unsigned char*)rdram)[(interp_addr & 0xFFFFFF)];
			/*if ((debug_count+Count) > 0xabaa20)
			  printf("count:%x, add:%x, op:%x, l%d\n", (int)(Count+debug_count),
			     interp_addr, op, line);*/
			prefetch_opcode(vr_op);
		} else if ((interp_addr >= 0xa4000000) && (interp_addr < 0xa4001000))
		{
			vr_op = SP_DMEM[(interp_addr & 0xFFF) / 4];
			prefetch_opcode(vr_op);
		} else if ((interp_addr > 0xb0000000))
		{
			vr_op = ((unsigned long*)rom)[(interp_addr & 0xFFFFFFF) / 4];
			prefetch_opcode(vr_op);
		} else
		{
			//unmapped memory exception
			printf("Exception, attempt to prefetch unmapped memory at: %x\n", (int)interp_addr);
			stop = 1;
		}
	} else
	{
		unsigned long addr = interp_addr, phys;
		phys = virtual_to_physical_address(interp_addr, 2);
		if (phys != 0x00000000) interp_addr = phys;
		else
		{
			prefetch();
			//tlb_used = 0;
			return;
		}
		//tlb_used = 1;
		prefetch();
		//tlb_used = 0;
		interp_addr = addr;
		return;
	}
	if (tracelog::active())
		tracelog::log_pure();
}

void pure_interpreter()
{
	interp_addr = 0xa4000040;
	stop = 0;
	PC = (precomp_instr*)malloc(sizeof(precomp_instr));
	last_addr = interp_addr;
	while (!stop)
	{
		//if (interp_addr == 0x10022d08) stop = 1;
		//printf("addr: %x\n", interp_addr);
		prefetch();
#ifdef COMPARE_CORE
		compare_core();
#endif
		//if (Count > 0x2000000) printf("inter:%x,%x\n", interp_addr,op);
		//if ((Count+debug_count) > 0xabaa2c) stop=1;
		interp_ops[((vr_op >> 26) & 0x3F)]();
		ignore = false;

		//Count = (unsigned long)Count + 2;
		//if (interp_addr == 0x80000180) last_addr = interp_addr;
#ifdef DBG
		PC->addr = interp_addr;
		if (debugger_mode) update_debugger();
#endif
		while (!Debugger::get_resumed())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		Debugger::on_late_cycle(vr_op, interp_addr);
	}
	PC->addr = interp_addr;
}

void interprete_section(unsigned long addr)
{
	interp_addr = addr;
	PC = (precomp_instr*)malloc(sizeof(precomp_instr));
	last_addr = interp_addr;
	while (!stop && (addr >> 12) == (interp_addr >> 12))
	{
		prefetch();
		if (tracelog::active())
			tracelog::log_pure();
		PC->addr = interp_addr;
		interp_ops[((vr_op >> 26) & 0x3F)]();
#ifdef DBG
		PC->addr = interp_addr;
		if (debugger_mode) update_debugger();
#endif
	}
	PC->addr = interp_addr;
}
