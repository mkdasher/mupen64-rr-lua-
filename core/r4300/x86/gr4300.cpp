/**
 * Mupen64 - gr4300.c
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

#include "assemble.h"
#include "../r4300.h"
#include "../macros.h"
#include "../../memory/memory.h"
#include "../interrupt.h"
#include "../ops.h"
#include "../recomph.h"
#include "regcache.h"
#include "../exception.h"
#include "interpret.h"
#include <shared/Config.hpp>

extern unsigned long vr_op; //pure_interp.c
extern unsigned long src; //recomp.c

precomp_instr fake_instr;
static int eax, ebx, ecx, edx, esp, ebp, esi, edi;

int branch_taken;

void gennotcompiled()
{
	free_all_registers();
	simplify_access();

	if (dst->addr == 0xa4000040)
	{
		mov_m32_reg32((unsigned long*)(&return_address), ESP);
		sub_m32_imm32((unsigned long*)(&return_address), 4);
	}
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst));
	mov_reg32_imm32(EAX, (unsigned long)NOTCOMPILED);
	call_reg32(EAX);
}

void genlink_subblock()
{
	free_all_registers();
	jmp(dst->addr + 4);
}

void gendebug()
{
	//free_all_registers();
	mov_m32_reg32((unsigned long*)&eax, EAX);
	mov_m32_reg32((unsigned long*)&ebx, EBX);
	mov_m32_reg32((unsigned long*)&ecx, ECX);
	mov_m32_reg32((unsigned long*)&edx, EDX);
	mov_m32_reg32((unsigned long*)&esp, ESP);
	mov_m32_reg32((unsigned long*)&ebp, EBP);
	mov_m32_reg32((unsigned long*)&esi, ESI);
	mov_m32_reg32((unsigned long*)&edi, EDI);

	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst));
	mov_m32_imm32((unsigned long*)(&vr_op), (unsigned long)(src));
	mov_reg32_imm32(EAX, (unsigned long)debug);
	call_reg32(EAX);

	mov_reg32_m32(EAX, (unsigned long*)&eax);
	mov_reg32_m32(EBX, (unsigned long*)&ebx);
	mov_reg32_m32(ECX, (unsigned long*)&ecx);
	mov_reg32_m32(EDX, (unsigned long*)&edx);
	mov_reg32_m32(ESP, (unsigned long*)&esp);
	mov_reg32_m32(EBP, (unsigned long*)&ebp);
	mov_reg32_m32(ESI, (unsigned long*)&esi);
	mov_reg32_m32(EDI, (unsigned long*)&edi);
}

void gencallinterp(unsigned long addr, int jump)
{
	free_all_registers();
	simplify_access();
	if (jump)
		mov_m32_imm32((unsigned long*)(&dyna_interp), 1);
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst));
	mov_reg32_imm32(EAX, addr);
	call_reg32(EAX);
	if (jump)
	{
		mov_m32_imm32((unsigned long*)(&dyna_interp), 0);
		mov_reg32_imm32(EAX, (unsigned long)dyna_jump);
		call_reg32(EAX);
	}
}

void genupdate_count(unsigned long addr)
{
#ifndef COMPARE_CORE
#ifndef DBG
	mov_reg32_imm32(EAX, addr);
	sub_reg32_m32(EAX, (unsigned long*)(&last_addr));
	shr_reg32_imm8(EAX, 1);
	add_m32_reg32((unsigned long*)(&core_Count), EAX);
#else
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst + 1));
	mov_reg32_imm32(EAX, (unsigned long)update_count);
	call_reg32(EAX);
#endif
#else
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst + 1));
	mov_reg32_imm32(EAX, (unsigned long)update_count);
	call_reg32(EAX);
#endif
}

void gendelayslot()
{
	mov_m32_imm32((void*)(&delay_slot), 1);
	recompile_opcode();

	free_all_registers();
	genupdate_count(dst->addr + 4);

	mov_m32_imm32((void*)(&delay_slot), 0);
}

void genni()
{
#ifdef EMU64_DEBUG
	gencallinterp((unsigned long)NI, 0);
#endif
}

void genreserved()
{
#ifdef EMU64_DEBUG
	gencallinterp((unsigned long)RESERVED, 0);
#endif
}

void genfin_block()
{
	gencallinterp((unsigned long)FIN_BLOCK, 0);
}

void gencheck_interrupt(unsigned long instr_structure)
{
	mov_eax_memoffs32((void*)(&next_interrupt));
	cmp_reg32_m32(EAX, (void*)&core_Count);
	ja_rj(17);
	mov_m32_imm32((unsigned long*)(&PC), instr_structure); // 10
	mov_reg32_imm32(EAX, (unsigned long)gen_interrupt); // 5
	call_reg32(EAX); // 2
}

void gencheck_interrupt_out(unsigned long addr)
{
	mov_eax_memoffs32((void*)(&next_interrupt));
	cmp_reg32_m32(EAX, (void*)&core_Count);
	ja_rj(27);
	mov_m32_imm32((unsigned long*)(&fake_instr.addr), addr);
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(&fake_instr));
	mov_reg32_imm32(EAX, (unsigned long)gen_interrupt);
	call_reg32(EAX);
}

void gencheck_interrupt_reg() // addr is in EAX
{
	mov_reg32_m32(EBX, (void*)&next_interrupt);
	cmp_reg32_m32(EBX, (void*)&core_Count);
	ja_rj(22);
	mov_memoffs32_eax((unsigned long*)(&fake_instr.addr)); // 5
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(&fake_instr)); // 10
	mov_reg32_imm32(EAX, (unsigned long)gen_interrupt); // 5
	call_reg32(EAX); // 2
}

void gennop()
{
}

void genj()
{
#ifdef INTERPRET_J
	gencallinterp((unsigned long)J, 1);
#else
	unsigned long naddr;

	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)J, 1);
		return;
	}

	gendelayslot();
	naddr = ((dst - 1)->f.j.inst_index << 2) | (dst->addr & 0xF0000000);

	mov_m32_imm32((void*)(&last_addr), naddr);
	gencheck_interrupt((unsigned long)&actual->block[(naddr - actual->start) / 4]);
	jmp(naddr);
#endif
}

void genj_out()
{
#ifdef INTERPRET_J_OUT
	gencallinterp((unsigned long)J_OUT, 1);
#else
	unsigned long naddr;

	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)J_OUT, 1);
		return;
	}

	gendelayslot();
	naddr = ((dst - 1)->f.j.inst_index << 2) | (dst->addr & 0xF0000000);

	mov_m32_imm32((void*)(&last_addr), naddr);
	gencheck_interrupt_out(naddr);
	mov_m32_imm32(&jump_to_address, naddr);
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst + 1));
	mov_reg32_imm32(EAX, (unsigned long)jump_to_func);
	call_reg32(EAX);
#endif
}

void genj_idle()
{
#ifdef INTERPRET_J_IDLE
	gencallinterp((unsigned long)J_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)J_IDLE, 1);
		return;
	}

	mov_eax_memoffs32((unsigned long*)(&next_interrupt));
	sub_reg32_m32(EAX, (unsigned long*)(&core_Count));
	cmp_reg32_imm8(EAX, 3);
	jbe_rj(11);

	and_eax_imm32(0xFFFFFFFC);
	add_m32_reg32((unsigned long*)(&core_Count), EAX);

	genj();
#endif
}

void genjal()
{
#ifdef INTERPRET_JAL
	gencallinterp((unsigned long)JAL, 1);
#else
	unsigned long naddr;

	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)JAL, 1);
		return;
	}

	gendelayslot();

	mov_m32_imm32((unsigned long*)(reg + 31), dst->addr + 4);
	if (((dst->addr + 4) & 0x80000000))
		mov_m32_imm32((unsigned long*)(&reg[31]) + 1, 0xFFFFFFFF);
	else
		mov_m32_imm32((unsigned long*)(&reg[31]) + 1, 0);

	naddr = ((dst - 1)->f.j.inst_index << 2) | (dst->addr & 0xF0000000);

	mov_m32_imm32((void*)(&last_addr), naddr);
	gencheck_interrupt((unsigned long)&actual->block[(naddr - actual->start) / 4]);
	jmp(naddr);
#endif
}

void genjal_out()
{
#ifdef INTERPRET_JAL_OUT
	gencallinterp((unsigned long)JAL_OUT, 1);
#else
	unsigned long naddr;

	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)JAL_OUT, 1);
		return;
	}

	gendelayslot();

	mov_m32_imm32((unsigned long*)(reg + 31), dst->addr + 4);
	if (((dst->addr + 4) & 0x80000000))
		mov_m32_imm32((unsigned long*)(&reg[31]) + 1, 0xFFFFFFFF);
	else
		mov_m32_imm32((unsigned long*)(&reg[31]) + 1, 0);

	naddr = ((dst - 1)->f.j.inst_index << 2) | (dst->addr & 0xF0000000);

	mov_m32_imm32((void*)(&last_addr), naddr);
	gencheck_interrupt_out(naddr);
	mov_m32_imm32(&jump_to_address, naddr);
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst + 1));
	mov_reg32_imm32(EAX, (unsigned long)jump_to_func);
	call_reg32(EAX);
#endif
}

void genjal_idle()
{
#ifdef INTERPRET_JAL_IDLE
	gencallinterp((unsigned long)JAL_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)JAL_IDLE, 1);
		return;
	}

	mov_eax_memoffs32((unsigned long*)(&next_interrupt));
	sub_reg32_m32(EAX, (unsigned long*)(&core_Count));
	cmp_reg32_imm8(EAX, 3);
	jbe_rj(11);

	and_eax_imm32(0xFFFFFFFC);
	add_m32_reg32((unsigned long*)(&core_Count), EAX);

	genjal();
#endif
}

void genbeq_test()
{
	int rs_64bit = is64((unsigned long*)dst->f.i.rs);
	int rt_64bit = is64((unsigned long*)dst->f.i.rt);

	if (!rs_64bit && !rt_64bit)
	{
		int rs = allocate_register((unsigned long*)dst->f.i.rs);
		int rt = allocate_register((unsigned long*)dst->f.i.rt);

		cmp_reg32_reg32(rs, rt);
		jne_rj(12);
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
	} else if (rs_64bit == -1)
	{
		int rt1 = allocate_64_register1((unsigned long*)dst->f.i.rt);
		int rt2 = allocate_64_register2((unsigned long*)dst->f.i.rt);

		cmp_reg32_m32(rt1, (unsigned long*)dst->f.i.rs);
		jne_rj(20);
		cmp_reg32_m32(rt2, ((unsigned long*)dst->f.i.rs) + 1); // 6
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
	} else if (rt_64bit == -1)
	{
		int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
		int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);

		cmp_reg32_m32(rs1, (unsigned long*)dst->f.i.rt);
		jne_rj(20);
		cmp_reg32_m32(rs2, ((unsigned long*)dst->f.i.rt) + 1); // 6
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
	} else
	{
		int rs1, rs2, rt1, rt2;
		if (!rs_64bit)
		{
			rt1 = allocate_64_register1((unsigned long*)dst->f.i.rt);
			rt2 = allocate_64_register2((unsigned long*)dst->f.i.rt);
			rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
			rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
		} else
		{
			rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
			rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
			rt1 = allocate_64_register1((unsigned long*)dst->f.i.rt);
			rt2 = allocate_64_register2((unsigned long*)dst->f.i.rt);
		}
		cmp_reg32_reg32(rs1, rt1);
		jne_rj(16);
		cmp_reg32_reg32(rs2, rt2); // 2
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
	}
}

void gentest()
{
	unsigned long temp, temp2;

	cmp_m32_imm32((unsigned long*)(&branch_taken), 0);
	je_near_rj(0);
	temp = code_length;
	mov_m32_imm32((void*)(&last_addr), dst->addr + (dst - 1)->f.i.immediate * 4);
	gencheck_interrupt((unsigned long)(dst + (dst - 1)->f.i.immediate));
	jmp(dst->addr + (dst - 1)->f.i.immediate * 4);

	temp2 = code_length;
	code_length = temp - 4;
	put32(temp2 - temp);
	code_length = temp2;
	mov_m32_imm32((void*)(&last_addr), dst->addr + 4);
	gencheck_interrupt((unsigned long)(dst + 1));
	jmp(dst->addr + 4);
}

void genbeq()
{
#ifdef INTERPRET_BEQ
	gencallinterp((unsigned long)BEQ, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BEQ, 1);
		return;
	}

	genbeq_test();
	gendelayslot();
	gentest();
#endif
}

void gentest_out()
{
	unsigned long temp, temp2;

	cmp_m32_imm32((unsigned long*)(&branch_taken), 0);
	je_near_rj(0);
	temp = code_length;
	mov_m32_imm32((void*)(&last_addr), dst->addr + (dst - 1)->f.i.immediate * 4);
	gencheck_interrupt_out(dst->addr + (dst - 1)->f.i.immediate * 4);
	mov_m32_imm32(&jump_to_address, dst->addr + (dst - 1)->f.i.immediate * 4);
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst + 1));
	mov_reg32_imm32(EAX, (unsigned long)jump_to_func);
	call_reg32(EAX);

	temp2 = code_length;
	code_length = temp - 4;
	put32(temp2 - temp);
	code_length = temp2;
	mov_m32_imm32((void*)(&last_addr), dst->addr + 4);
	gencheck_interrupt((unsigned long)(dst + 1));
	jmp(dst->addr + 4);
}

void genbeq_out()
{
#ifdef INTERPRET_BEQ_OUT
	gencallinterp((unsigned long)BEQ_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BEQ_OUT, 1);
		return;
	}

	genbeq_test();
	gendelayslot();
	gentest_out();
#endif
}

void gentest_idle()
{
	unsigned long temp, temp2;
	int reg;

	reg = lru_register();
	free_register(reg);

	cmp_m32_imm32((unsigned long*)(&branch_taken), 0);
	je_near_rj(0);
	temp = code_length;

	mov_reg32_m32(reg, (unsigned long*)(&next_interrupt));
	sub_reg32_m32(reg, (unsigned long*)(&core_Count));
	cmp_reg32_imm8(reg, 3);
	jbe_rj(12);

	and_reg32_imm32(reg, 0xFFFFFFFC); // 6
	add_m32_reg32((unsigned long*)(&core_Count), reg); // 6

	temp2 = code_length;
	code_length = temp - 4;
	put32(temp2 - temp);
	code_length = temp2;
}

void genbeq_idle()
{
#ifdef INTERPRET_BEQ_IDLE
	gencallinterp((unsigned long)BEQ_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BEQ_IDLE, 1);
		return;
	}

	genbeq_test();
	gentest_idle();
	genbeq();
#endif
}

void genbne_test()
{
	int rs_64bit = is64((unsigned long*)dst->f.i.rs);
	int rt_64bit = is64((unsigned long*)dst->f.i.rt);

	if (!rs_64bit && !rt_64bit)
	{
		int rs = allocate_register((unsigned long*)dst->f.i.rs);
		int rt = allocate_register((unsigned long*)dst->f.i.rt);

		cmp_reg32_reg32(rs, rt);
		je_rj(12);
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
	} else if (rs_64bit == -1)
	{
		int rt1 = allocate_64_register1((unsigned long*)dst->f.i.rt);
		int rt2 = allocate_64_register2((unsigned long*)dst->f.i.rt);

		cmp_reg32_m32(rt1, (unsigned long*)dst->f.i.rs);
		jne_rj(20);
		cmp_reg32_m32(rt2, ((unsigned long*)dst->f.i.rs) + 1); // 6
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
	} else if (rt_64bit == -1)
	{
		int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
		int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);

		cmp_reg32_m32(rs1, (unsigned long*)dst->f.i.rt);
		jne_rj(20);
		cmp_reg32_m32(rs2, ((unsigned long*)dst->f.i.rt) + 1); // 6
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
	} else
	{
		int rs1, rs2, rt1, rt2;
		if (!rs_64bit)
		{
			rt1 = allocate_64_register1((unsigned long*)dst->f.i.rt);
			rt2 = allocate_64_register2((unsigned long*)dst->f.i.rt);
			rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
			rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
		} else
		{
			rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
			rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
			rt1 = allocate_64_register1((unsigned long*)dst->f.i.rt);
			rt2 = allocate_64_register2((unsigned long*)dst->f.i.rt);
		}
		cmp_reg32_reg32(rs1, rt1);
		jne_rj(16);
		cmp_reg32_reg32(rs2, rt2); // 2
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
	}
}

void genbne()
{
#ifdef INTERPRET_BNE
	gencallinterp((unsigned long)BNE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BNE, 1);
		return;
	}

	genbne_test();
	gendelayslot();
	gentest();
#endif
}

void genbne_out()
{
#ifdef INTERPRET_BNE_OUT
	gencallinterp((unsigned long)BNE_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BNE_OUT, 1);
		return;
	}

	genbne_test();
	gendelayslot();
	gentest_out();
#endif
}

void genbne_idle()
{
#ifdef INTERPRET_BNE_IDLE
	gencallinterp((unsigned long)BNE_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BNE_IDLE, 1);
		return;
	}

	genbne_test();
	gentest_idle();
	genbne();
#endif
}

void genblez_test()
{
	int rs_64bit = is64((unsigned long*)dst->f.i.rs);

	if (!rs_64bit)
	{
		int rs = allocate_register((unsigned long*)dst->f.i.rs);

		cmp_reg32_imm32(rs, 0);
		jg_rj(12);
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
	} else if (rs_64bit == -1)
	{
		cmp_m32_imm32(((unsigned long*)dst->f.i.rs) + 1, 0);
		jg_rj(14);
		jne_rj(24); // 2
		cmp_m32_imm32((unsigned long*)dst->f.i.rs, 0); // 10
		je_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
	} else
	{
		int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
		int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);

		cmp_reg32_imm32(rs2, 0);
		jg_rj(10);
		jne_rj(20); // 2
		cmp_reg32_imm32(rs1, 0); // 6
		je_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
	}
}

void genblez()
{
#ifdef INTERPRET_BLEZ
	gencallinterp((unsigned long)BLEZ, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BLEZ, 1);
		return;
	}

	genblez_test();
	gendelayslot();
	gentest();
#endif
}

void genblez_out()
{
#ifdef INTERPRET_BLEZ_OUT
	gencallinterp((unsigned long)BLEZ_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BLEZ_OUT, 1);
		return;
	}

	genblez_test();
	gendelayslot();
	gentest_out();
#endif
}

void genblez_idle()
{
#ifdef INTERPRET_BLEZ_IDLE
	gencallinterp((unsigned long)BLEZ_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BLEZ_IDLE, 1);
		return;
	}

	genblez_test();
	gentest_idle();
	genblez();
#endif
}

void genbgtz_test()
{
	int rs_64bit = is64((unsigned long*)dst->f.i.rs);

	if (!rs_64bit)
	{
		int rs = allocate_register((unsigned long*)dst->f.i.rs);

		cmp_reg32_imm32(rs, 0);
		jle_rj(12);
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
	} else if (rs_64bit == -1)
	{
		cmp_m32_imm32(((unsigned long*)dst->f.i.rs) + 1, 0);
		jl_rj(14);
		jne_rj(24); // 2
		cmp_m32_imm32((unsigned long*)dst->f.i.rs, 0); // 10
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
	} else
	{
		int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
		int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);

		cmp_reg32_imm32(rs2, 0);
		jl_rj(10);
		jne_rj(20); // 2
		cmp_reg32_imm32(rs1, 0); // 6
		jne_rj(12); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 0); // 10
		jmp_imm_short(10); // 2
		mov_m32_imm32((unsigned long*)(&branch_taken), 1); // 10
	}
}

void genbgtz()
{
#ifdef INTERPRET_BGTZ
	gencallinterp((unsigned long)BGTZ, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BGTZ, 1);
		return;
	}

	genbgtz_test();
	gendelayslot();
	gentest();
#endif
}

void genbgtz_out()
{
#ifdef INTERPRET_BGTZ_OUT
	gencallinterp((unsigned long)BGTZ_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BGTZ_OUT, 1);
		return;
	}

	genbgtz_test();
	gendelayslot();
	gentest_out();
#endif
}

void genbgtz_idle()
{
#ifdef INTERPRET_BGTZ_IDLE
	gencallinterp((unsigned long)BGTZ_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BGTZ_IDLE, 1);
		return;
	}

	genbgtz_test();
	gentest_idle();
	genbgtz();
#endif
}

void genaddi()
{
#ifdef INTERPRET_ADDI
	gencallinterp((unsigned long)ADDI, 0);
#else
	int rs = allocate_register((unsigned long*)dst->f.i.rs);
	int rt = allocate_register_w((unsigned long*)dst->f.i.rt);

	mov_reg32_reg32(rt, rs);
	add_reg32_imm32(rt, (long)dst->f.i.immediate);
#endif
}

void genaddiu()
{
#ifdef INTERPRET_ADDIU
	gencallinterp((unsigned long)ADDIU, 0);
#else
	int rs = allocate_register((unsigned long*)dst->f.i.rs);
	int rt = allocate_register_w((unsigned long*)dst->f.i.rt);

	mov_reg32_reg32(rt, rs);
	add_reg32_imm32(rt, (long)dst->f.i.immediate);
#endif
}

void genslti()
{
#ifdef INTERPRET_SLTI
	gencallinterp((unsigned long)SLTI, 0);
#else
	int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
	int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
	int rt = allocate_register_w((unsigned long*)dst->f.i.rt);
	long long imm = (long long)dst->f.i.immediate;

	cmp_reg32_imm32(rs2, (unsigned long)(imm >> 32));
	jl_rj(17);
	jne_rj(8); // 2
	cmp_reg32_imm32(rs1, (unsigned long)imm); // 6
	jl_rj(7); // 2
	mov_reg32_imm32(rt, 0); // 5
	jmp_imm_short(5); // 2
	mov_reg32_imm32(rt, 1); // 5
#endif
}

void gensltiu()
{
#ifdef INTERPRET_SLTIU
	gencallinterp((unsigned long)SLTIU, 0);
#else
	int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
	int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
	int rt = allocate_register_w((unsigned long*)dst->f.i.rt);
	long long imm = (long long)dst->f.i.immediate;

	cmp_reg32_imm32(rs2, (unsigned long)(imm >> 32));
	jb_rj(17);
	jne_rj(8); // 2
	cmp_reg32_imm32(rs1, (unsigned long)imm); // 6
	jb_rj(7); // 2
	mov_reg32_imm32(rt, 0); // 5
	jmp_imm_short(5); // 2
	mov_reg32_imm32(rt, 1); // 5
#endif
}

void genandi()
{
#ifdef INTERPRET_ANDI
	gencallinterp((unsigned long)ANDI, 0);
#else
	int rs = allocate_register((unsigned long*)dst->f.i.rs);
	int rt = allocate_register_w((unsigned long*)dst->f.i.rt);

	mov_reg32_reg32(rt, rs);
	and_reg32_imm32(rt, (unsigned short)dst->f.i.immediate);
#endif
}

void genori()
{
#ifdef INTERPRET_ORI
	gencallinterp((unsigned long)ORI, 0);
#else
	int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
	int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
	int rt1 = allocate_64_register1_w((unsigned long*)dst->f.i.rt);
	int rt2 = allocate_64_register2_w((unsigned long*)dst->f.i.rt);

	mov_reg32_reg32(rt1, rs1);
	mov_reg32_reg32(rt2, rs2);
	or_reg32_imm32(rt1, (unsigned short)dst->f.i.immediate);
#endif
}

void genxori()
{
#ifdef INTERPRET_XORI
	gencallinterp((unsigned long)XORI, 0);
#else
	int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
	int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
	int rt1 = allocate_64_register1_w((unsigned long*)dst->f.i.rt);
	int rt2 = allocate_64_register2_w((unsigned long*)dst->f.i.rt);

	mov_reg32_reg32(rt1, rs1);
	mov_reg32_reg32(rt2, rs2);
	xor_reg32_imm32(rt1, (unsigned short)dst->f.i.immediate);
#endif
}

void genlui()
{
#ifdef INTERPRET_LUI
	gencallinterp((unsigned long)LUI, 0);
#else
	int rt = allocate_register_w((unsigned long*)dst->f.i.rt);

	mov_reg32_imm32(rt, (unsigned long)dst->f.i.immediate << 16);
#endif
}

void gentestl()
{
	unsigned long temp, temp2;

	cmp_m32_imm32((unsigned long*)(&branch_taken), 0);
	je_near_rj(0);
	temp = code_length;
	gendelayslot();
	mov_m32_imm32((void*)(&last_addr), dst->addr + (dst - 1)->f.i.immediate * 4);
	gencheck_interrupt((unsigned long)(dst + (dst - 1)->f.i.immediate));
	jmp(dst->addr + (dst - 1)->f.i.immediate * 4);

	temp2 = code_length;
	code_length = temp - 4;
	put32(temp2 - temp);
	code_length = temp2;
	genupdate_count(dst->addr - 4);
	mov_m32_imm32((void*)(&last_addr), dst->addr + 4);
	gencheck_interrupt((unsigned long)(dst + 1));
	jmp(dst->addr + 4);
}

void genbeql()
{
#ifdef INTERPRET_BEQL
	gencallinterp((unsigned long)BEQL, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BEQL, 1);
		return;
	}

	genbeq_test();
	free_all_registers();
	gentestl();
#endif
}

void gentestl_out()
{
	unsigned long temp, temp2;

	cmp_m32_imm32((unsigned long*)(&branch_taken), 0);
	je_near_rj(0);
	temp = code_length;
	gendelayslot();
	mov_m32_imm32((void*)(&last_addr), dst->addr + (dst - 1)->f.i.immediate * 4);
	gencheck_interrupt_out(dst->addr + (dst - 1)->f.i.immediate * 4);
	mov_m32_imm32(&jump_to_address, dst->addr + (dst - 1)->f.i.immediate * 4);
	mov_m32_imm32((unsigned long*)(&PC), (unsigned long)(dst + 1));
	mov_reg32_imm32(EAX, (unsigned long)jump_to_func);
	call_reg32(EAX);

	temp2 = code_length;
	code_length = temp - 4;
	put32(temp2 - temp);
	code_length = temp2;
	genupdate_count(dst->addr - 4);
	mov_m32_imm32((void*)(&last_addr), dst->addr + 4);
	gencheck_interrupt((unsigned long)(dst + 1));
	jmp(dst->addr + 4);
}

void genbeql_out()
{
#ifdef INTERPRET_BEQL_OUT
	gencallinterp((unsigned long)BEQL_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BEQL_OUT, 1);
		return;
	}

	genbeq_test();
	free_all_registers();
	gentestl_out();
#endif
}

void genbeql_idle()
{
#ifdef INTERPRET_BEQL_IDLE
	gencallinterp((unsigned long)BEQL_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BEQL_IDLE, 1);
		return;
	}

	genbeq_test();
	gentest_idle();
	genbeql();
#endif
}

void genbnel()
{
#ifdef INTERPRET_BNEL
	gencallinterp((unsigned long)BNEL, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BNEL, 1);
		return;
	}

	genbne_test();
	free_all_registers();
	gentestl();
#endif
}

void genbnel_out()
{
#ifdef INTERPRET_BNEL_OUT
	gencallinterp((unsigned long)BNEL_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BNEL_OUT, 1);
		return;
	}

	genbne_test();
	free_all_registers();
	gentestl_out();
#endif
}

void genbnel_idle()
{
#ifdef INTERPRET_BNEL_IDLE
	gencallinterp((unsigned long)BNEL_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BNEL_IDLE, 1);
		return;
	}

	genbne_test();
	gentest_idle();
	genbnel();
#endif
}

void genblezl()
{
#ifdef INTERPRET_BLEZL
	gencallinterp((unsigned long)BLEZL, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BLEZL, 1);
		return;
	}

	genblez_test();
	free_all_registers();
	gentestl();
#endif
}

void genblezl_out()
{
#ifdef INTERPRET_BLEZL_OUT
	gencallinterp((unsigned long)BLEZL_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BLEZL_OUT, 1);
		return;
	}

	genblez_test();
	free_all_registers();
	gentestl_out();
#endif
}

void genblezl_idle()
{
#ifdef INTERPRET_BLEZL_IDLE
	gencallinterp((unsigned long)BLEZL_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BLEZL_IDLE, 1);
		return;
	}

	genblez_test();
	gentest_idle();
	genblezl();
#endif
}

void genbgtzl()
{
#ifdef INTERPRET_BGTZL
	gencallinterp((unsigned long)BGTZL, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BGTZL, 1);
		return;
	}

	genbgtz_test();
	free_all_registers();
	gentestl();
#endif
}

void genbgtzl_out()
{
#ifdef INTERPRET_BGTZL_OUT
	gencallinterp((unsigned long)BGTZL_OUT, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BGTZL_OUT, 1);
		return;
	}

	genbgtz_test();
	free_all_registers();
	gentestl_out();
#endif
}

void genbgtzl_idle()
{
#ifdef INTERPRET_BGTZL_IDLE
	gencallinterp((unsigned long)BGTZL_IDLE, 1);
#else
	if (((dst->addr & 0xFFF) == 0xFFC &&
		(dst->addr < 0x80000000 || dst->addr >= 0xC0000000)) || !g_config.is_compiled_jump_enabled)
	{
		gencallinterp((unsigned long)BGTZL_IDLE, 1);
		return;
	}

	genbgtz_test();
	gentest_idle();
	genbgtzl();
#endif
}

void gendaddi()
{
#ifdef INTERPRET_DADDI
	gencallinterp((unsigned long)DADDI, 0);
#else
	int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
	int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
	int rt1 = allocate_64_register1_w((unsigned long*)dst->f.i.rt);
	int rt2 = allocate_64_register2_w((unsigned long*)dst->f.i.rt);

	mov_reg32_reg32(rt1, rs1);
	mov_reg32_reg32(rt2, rs2);
	add_reg32_imm32(rt1, dst->f.i.immediate);
	adc_reg32_imm32(rt2, (int)dst->f.i.immediate >> 31);
#endif
}

void gendaddiu()
{
#ifdef INTERPRET_DADDIU
	gencallinterp((unsigned long)DADDIU, 0);
#else
	int rs1 = allocate_64_register1((unsigned long*)dst->f.i.rs);
	int rs2 = allocate_64_register2((unsigned long*)dst->f.i.rs);
	int rt1 = allocate_64_register1_w((unsigned long*)dst->f.i.rt);
	int rt2 = allocate_64_register2_w((unsigned long*)dst->f.i.rt);

	mov_reg32_reg32(rt1, rs1);
	mov_reg32_reg32(rt2, rs2);
	add_reg32_imm32(rt1, dst->f.i.immediate);
	adc_reg32_imm32(rt2, (int)dst->f.i.immediate >> 31);
#endif
}

void genldl()
{
	gencallinterp((unsigned long)LDL, 0);
}

void genldr()
{
	gencallinterp((unsigned long)LDR, 0);
}

void genlb()
{
#ifdef INTERPRET_LB
	gencallinterp((unsigned long)LB, 0);
#else
	free_all_registers();
	simplify_access();
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmemb);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdramb);
	}
	je_rj(47);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_imm32((unsigned long*)(&rdword), (unsigned long)dst->f.i.rt); // 10
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmemb); // 7
	call_reg32(EBX); // 2
	movsx_reg32_m8(EAX, (unsigned char*)dst->f.i.rt); // 7
	jmp_imm_short(16); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	xor_reg8_imm8(BL, 3); // 3
	movsx_reg32_8preg32pimm32(EAX, EBX, (unsigned long)rdram); // 7

	set_register_state(EAX, (unsigned long*)dst->f.i.rt, 1);
#endif
}

void genlh()
{
#ifdef INTERPRET_LH
	gencallinterp((unsigned long)LH, 0);
#else
	free_all_registers();
	simplify_access();
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmemh);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdramh);
	}
	je_rj(47);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_imm32((unsigned long*)(&rdword), (unsigned long)dst->f.i.rt); // 10
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmemh); // 7
	call_reg32(EBX); // 2
	movsx_reg32_m16(EAX, (unsigned short*)dst->f.i.rt); // 7
	jmp_imm_short(16); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	xor_reg8_imm8(BL, 2); // 3
	movsx_reg32_16preg32pimm32(EAX, EBX, (unsigned long)rdram); // 7

	set_register_state(EAX, (unsigned long*)dst->f.i.rt, 1);
#endif
}

void genlwl()
{
	gencallinterp((unsigned long)LWL, 0);
}

void genlw()
{
#ifdef INTERPRET_LW
	gencallinterp((unsigned long)LW, 0);
#else
	free_all_registers();
	simplify_access();
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmem);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdram);
	}
	je_rj(45);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_imm32((unsigned long*)(&rdword), (unsigned long)dst->f.i.rt); // 10
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmem); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(dst->f.i.rt)); // 5
	jmp_imm_short(12); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_reg32_preg32pimm32(EAX, EBX, (unsigned long)rdram); // 6

	set_register_state(EAX, (unsigned long*)dst->f.i.rt, 1);
#endif
}

void genlbu()
{
#ifdef INTERPRET_LBU
	gencallinterp((unsigned long)LBU, 0);
#else
	free_all_registers();
	simplify_access();
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmemb);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdramb);
	}
	je_rj(46);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_imm32((unsigned long*)(&rdword), (unsigned long)dst->f.i.rt); // 10
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmemb); // 7
	call_reg32(EBX); // 2
	mov_reg32_m32(EAX, (unsigned long*)dst->f.i.rt); // 6
	jmp_imm_short(15); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	xor_reg8_imm8(BL, 3); // 3
	mov_reg32_preg32pimm32(EAX, EBX, (unsigned long)rdram); // 6

	and_eax_imm32(0xFF);

	set_register_state(EAX, (unsigned long*)dst->f.i.rt, 1);
#endif
}

void genlhu()
{
#ifdef INTERPRET_LHU
	gencallinterp((unsigned long)LHU, 0);
#else
	free_all_registers();
	simplify_access();
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmemh);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdramh);
	}
	je_rj(46);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_imm32((unsigned long*)(&rdword), (unsigned long)dst->f.i.rt); // 10
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmemh); // 7
	call_reg32(EBX); // 2
	mov_reg32_m32(EAX, (unsigned long*)dst->f.i.rt); // 6
	jmp_imm_short(15); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	xor_reg8_imm8(BL, 2); // 3
	mov_reg32_preg32pimm32(EAX, EBX, (unsigned long)rdram); // 6

	and_eax_imm32(0xFFFF);

	set_register_state(EAX, (unsigned long*)dst->f.i.rt, 1);
#endif
}

void genlwr()
{
	gencallinterp((unsigned long)LWR, 0);
}

void genlwu()
{
#ifdef INTERPRET_LWU
	gencallinterp((unsigned long)LWU, 0);
#else
	free_all_registers();
	simplify_access();
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmem);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdram);
	}
	je_rj(45);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_imm32((unsigned long*)(&rdword), (unsigned long)dst->f.i.rt); // 10
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmem); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(dst->f.i.rt)); // 5
	jmp_imm_short(12); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_reg32_preg32pimm32(EAX, EBX, (unsigned long)rdram); // 6

	xor_reg32_reg32(EBX, EBX);

	set_64_register_state(EAX, EBX, (unsigned long*)dst->f.i.rt, 1);
#endif
}

void gensb()
{
#ifdef INTERPRET_SB
	gencallinterp((unsigned long)SB, 0);
#else
	free_all_registers();
	simplify_access();
	mov_reg8_m8(CL, (unsigned char*)dst->f.i.rt);
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)writememb);
		cmp_reg32_imm32(EAX, (unsigned long)write_rdramb);
	}
	je_rj(41);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m8_reg8((unsigned char*)(&g_byte), CL); // 6
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)writememb); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(&address)); // 5
	jmp_imm_short(17); // 2

	mov_reg32_reg32(EAX, EBX); // 2
	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	xor_reg8_imm8(BL, 3); // 3
	mov_preg32pimm32_reg8(EBX, (unsigned long)rdram, CL); // 6

	mov_reg32_reg32(EBX, EAX);
	shr_reg32_imm8(EBX, 12);
	cmp_preg32pimm32_imm8(EBX, (unsigned long)invalid_code, 0);
	jne_rj(54);
	mov_reg32_reg32(ECX, EBX); // 2
	shl_reg32_imm8(EBX, 2); // 3
	mov_reg32_preg32pimm32(EBX, EBX, (unsigned long)blocks); // 6
	mov_reg32_preg32pimm32(EBX, EBX, (int)&actual->block - (int)actual); // 6
	and_eax_imm32(0xFFF); // 5
	shr_reg32_imm8(EAX, 2); // 3
	mov_reg32_imm32(EDX, sizeof(precomp_instr)); // 5
	mul_reg32(EDX); // 2
	mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&dst->ops - (int)dst); // 7
	cmp_reg32_imm32(EAX, (unsigned long)NOTCOMPILED); // 6
	je_rj(7); // 2
	mov_preg32pimm32_imm8(ECX, (unsigned long)invalid_code, 1); // 7
#endif
}

void gensh()
{
#ifdef INTERPRET_SH
	gencallinterp((unsigned long)SH, 0);
#else
	free_all_registers();
	simplify_access();
	mov_reg16_m16(CX, (unsigned short*)dst->f.i.rt);
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)writememh);
		cmp_reg32_imm32(EAX, (unsigned long)write_rdramh);
	}
	je_rj(42);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m16_reg16((unsigned short*)(&hword), CX); // 7
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)writememh); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(&address)); // 5
	jmp_imm_short(18); // 2

	mov_reg32_reg32(EAX, EBX); // 2
	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	xor_reg8_imm8(BL, 2); // 3
	mov_preg32pimm32_reg16(EBX, (unsigned long)rdram, CX); // 7

	mov_reg32_reg32(EBX, EAX);
	shr_reg32_imm8(EBX, 12);
	cmp_preg32pimm32_imm8(EBX, (unsigned long)invalid_code, 0);
	jne_rj(54);
	mov_reg32_reg32(ECX, EBX); // 2
	shl_reg32_imm8(EBX, 2); // 3
	mov_reg32_preg32pimm32(EBX, EBX, (unsigned long)blocks); // 6
	mov_reg32_preg32pimm32(EBX, EBX, (int)&actual->block - (int)actual); // 6
	and_eax_imm32(0xFFF); // 5
	shr_reg32_imm8(EAX, 2); // 3
	mov_reg32_imm32(EDX, sizeof(precomp_instr)); // 5
	mul_reg32(EDX); // 2
	mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&dst->ops - (int)dst); // 7
	cmp_reg32_imm32(EAX, (unsigned long)NOTCOMPILED); // 6
	je_rj(7); // 2
	mov_preg32pimm32_imm8(ECX, (unsigned long)invalid_code, 1); // 7
#endif
}

void genswl()
{
	gencallinterp((unsigned long)SWL, 0);
}

void gensw()
{
#ifdef INTERPRET_SW
	gencallinterp((unsigned long)SW, 0);
#else
	free_all_registers();
	simplify_access();
	mov_reg32_m32(ECX, (unsigned long*)dst->f.i.rt);
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)writemem);
		cmp_reg32_imm32(EAX, (unsigned long)write_rdram);
	}
	je_rj(41);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_reg32((unsigned long*)(&word), ECX); // 6
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)writemem); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(&address)); // 5
	jmp_imm_short(14); // 2

	mov_reg32_reg32(EAX, EBX); // 2
	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_preg32pimm32_reg32(EBX, (unsigned long)rdram, ECX); // 6

	mov_reg32_reg32(EBX, EAX);
	shr_reg32_imm8(EBX, 12);
	cmp_preg32pimm32_imm8(EBX, (unsigned long)invalid_code, 0);
	jne_rj(54);
	mov_reg32_reg32(ECX, EBX); // 2
	shl_reg32_imm8(EBX, 2); // 3
	mov_reg32_preg32pimm32(EBX, EBX, (unsigned long)blocks); // 6
	mov_reg32_preg32pimm32(EBX, EBX, (int)&actual->block - (int)actual); // 6
	and_eax_imm32(0xFFF); // 5
	shr_reg32_imm8(EAX, 2); // 3
	mov_reg32_imm32(EDX, sizeof(precomp_instr)); // 5
	mul_reg32(EDX); // 2
	mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&dst->ops - (int)dst); // 7
	cmp_reg32_imm32(EAX, (unsigned long)NOTCOMPILED); // 6
	je_rj(7); // 2
	mov_preg32pimm32_imm8(ECX, (unsigned long)invalid_code, 1); // 7
#endif
}

void gensdl()
{
	gencallinterp((unsigned long)SDL, 0);
}

void gensdr()
{
	gencallinterp((unsigned long)SDR, 0);
}

void genswr()
{
	gencallinterp((unsigned long)SWR, 0);
}

#include <malloc.h>

inline void put8gr(unsigned char octet)
{
	(*inst_pointer)[code_length] = octet;
	code_length++;
	if (code_length == max_code_length)
	{
		max_code_length += 1000;
		*inst_pointer = (unsigned char*)realloc(*inst_pointer, max_code_length);
	}
}


void gencheck_cop1_unusable()
{
	unsigned long temp, temp2;
	free_all_registers();
	simplify_access();
	test_m32_imm32((unsigned long*)&core_Status, 0x20000000);
	jne_rj(0);
	temp = code_length;

	gencallinterp((unsigned long)check_cop1_unusable, 0);

	temp2 = code_length;
	code_length = temp - 1;
	put8gr(temp2 - temp);
	code_length = temp2;
}

void genlwc1()
{
#ifdef INTERPRET_LWC1
	gencallinterp((unsigned long)LWC1, 0);
#else
	gencheck_cop1_unusable();

	mov_eax_memoffs32((unsigned long*)(&reg[dst->f.lf.base]));
	add_eax_imm32((long)dst->f.lf.offset);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmem);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdram);
	}
	je_rj(42);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_reg32_m32(EDX, (unsigned long*)(&reg_cop1_simple[dst->f.lf.ft])); // 6
	mov_m32_reg32((unsigned long*)(&rdword), EDX); // 6
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmem); // 7
	call_reg32(EBX); // 2
	jmp_imm_short(20); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_reg32_preg32pimm32(EAX, EBX, (unsigned long)rdram); // 6
	mov_reg32_m32(EBX, (unsigned long*)(&reg_cop1_simple[dst->f.lf.ft])); // 6
	mov_preg32_reg32(EBX, EAX); // 2
#endif
}

void genldc1()
{
#ifdef INTERPRET_LDC1
	gencallinterp((unsigned long)LDC1, 0);
#else
	gencheck_cop1_unusable();

	mov_eax_memoffs32((unsigned long*)(&reg[dst->f.lf.base]));
	add_eax_imm32((long)dst->f.lf.offset);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmemd);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdramd);
	}
	je_rj(42);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_reg32_m32(EDX, (unsigned long*)(&reg_cop1_double[dst->f.lf.ft])); // 6
	mov_m32_reg32((unsigned long*)(&rdword), EDX); // 6
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmemd); // 7
	call_reg32(EBX); // 2
	jmp_imm_short(32); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_reg32_preg32pimm32(EAX, EBX, ((unsigned long)rdram) + 4); // 6
	mov_reg32_preg32pimm32(ECX, EBX, ((unsigned long)rdram)); // 6
	mov_reg32_m32(EBX, (unsigned long*)(&reg_cop1_double[dst->f.lf.ft])); // 6
	mov_preg32_reg32(EBX, EAX); // 2
	mov_preg32pimm32_reg32(EBX, 4, ECX); // 6
#endif
}

void gencache()
{
}

void genld()
{
#ifdef INTERPRET_LD
	gencallinterp((unsigned long)LD, 0);
#else
	free_all_registers();
	simplify_access();
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)readmemd);
		cmp_reg32_imm32(EAX, (unsigned long)read_rdramd);
	}
	je_rj(51);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_imm32((unsigned long*)(&rdword), (unsigned long)dst->f.i.rt); // 10
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)readmemd); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(dst->f.i.rt)); // 5
	mov_reg32_m32(ECX, (unsigned long*)(dst->f.i.rt) + 1); // 6
	jmp_imm_short(18); // 2

	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_reg32_preg32pimm32(EAX, EBX, ((unsigned long)rdram) + 4); // 6
	mov_reg32_preg32pimm32(ECX, EBX, ((unsigned long)rdram)); // 6

	set_64_register_state(EAX, ECX, (unsigned long*)dst->f.i.rt, 1);
#endif
}

void genswc1()
{
#ifdef INTERPRET_SWC1
	gencallinterp((unsigned long)SWC1, 0);
#else
	gencheck_cop1_unusable();

	mov_reg32_m32(EDX, (unsigned long*)(&reg_cop1_simple[dst->f.lf.ft]));
	mov_reg32_preg32(ECX, EDX);
	mov_eax_memoffs32((unsigned long*)(&reg[dst->f.lf.base]));
	add_eax_imm32((long)dst->f.lf.offset);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)writemem);
		cmp_reg32_imm32(EAX, (unsigned long)write_rdram);
	}
	je_rj(41);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_reg32((unsigned long*)(&word), ECX); // 6
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)writemem); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(&address)); // 5
	jmp_imm_short(14); // 2

	mov_reg32_reg32(EAX, EBX); // 2
	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_preg32pimm32_reg32(EBX, (unsigned long)rdram, ECX); // 6

	mov_reg32_reg32(EBX, EAX);
	shr_reg32_imm8(EBX, 12);
	cmp_preg32pimm32_imm8(EBX, (unsigned long)invalid_code, 0);
	jne_rj(54);
	mov_reg32_reg32(ECX, EBX); // 2
	shl_reg32_imm8(EBX, 2); // 3
	mov_reg32_preg32pimm32(EBX, EBX, (unsigned long)blocks); // 6
	mov_reg32_preg32pimm32(EBX, EBX, (int)&actual->block - (int)actual); // 6
	and_eax_imm32(0xFFF); // 5
	shr_reg32_imm8(EAX, 2); // 3
	mov_reg32_imm32(EDX, sizeof(precomp_instr)); // 5
	mul_reg32(EDX); // 2
	mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&dst->ops - (int)dst); // 7
	cmp_reg32_imm32(EAX, (unsigned long)NOTCOMPILED); // 6
	je_rj(7); // 2
	mov_preg32pimm32_imm8(ECX, (unsigned long)invalid_code, 1); // 7
#endif
}

void gensdc1()
{
#ifdef INTERPRET_SDC1
	gencallinterp((unsigned long)SDC1, 0);
#else
	gencheck_cop1_unusable();

	mov_reg32_m32(ESI, (unsigned long*)(&reg_cop1_double[dst->f.lf.ft]));
	mov_reg32_preg32(ECX, ESI);
	mov_reg32_preg32pimm32(EDX, ESI, 4);
	mov_eax_memoffs32((unsigned long*)(&reg[dst->f.lf.base]));
	add_eax_imm32((long)dst->f.lf.offset);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)writememd);
		cmp_reg32_imm32(EAX, (unsigned long)write_rdramd);
	}
	je_rj(47);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_reg32((unsigned long*)(&dword), ECX); // 6
	mov_m32_reg32((unsigned long*)(&dword) + 1, EDX); // 6
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)writememd); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(&address)); // 5
	jmp_imm_short(20); // 2

	mov_reg32_reg32(EAX, EBX); // 2
	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_preg32pimm32_reg32(EBX, ((unsigned long)rdram) + 4, ECX); // 6
	mov_preg32pimm32_reg32(EBX, ((unsigned long)rdram) + 0, EDX); // 6

	mov_reg32_reg32(EBX, EAX);
	shr_reg32_imm8(EBX, 12);
	cmp_preg32pimm32_imm8(EBX, (unsigned long)invalid_code, 0);
	jne_rj(54);
	mov_reg32_reg32(ECX, EBX); // 2
	shl_reg32_imm8(EBX, 2); // 3
	mov_reg32_preg32pimm32(EBX, EBX, (unsigned long)blocks); // 6
	mov_reg32_preg32pimm32(EBX, EBX, (int)&actual->block - (int)actual); // 6
	and_eax_imm32(0xFFF); // 5
	shr_reg32_imm8(EAX, 2); // 3
	mov_reg32_imm32(EDX, sizeof(precomp_instr)); // 5
	mul_reg32(EDX); // 2
	mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&dst->ops - (int)dst); // 7
	cmp_reg32_imm32(EAX, (unsigned long)NOTCOMPILED); // 6
	je_rj(7); // 2
	mov_preg32pimm32_imm8(ECX, (unsigned long)invalid_code, 1); // 7
#endif
}

void gensd()
{
#ifdef INTERPRET_SD
	gencallinterp((unsigned long)SD, 0);
#else
	free_all_registers();
	simplify_access();

	mov_reg32_m32(ECX, (unsigned long*)dst->f.i.rt);
	mov_reg32_m32(EDX, ((unsigned long*)dst->f.i.rt) + 1);
	mov_eax_memoffs32((unsigned long*)dst->f.i.rs);
	add_eax_imm32((long)dst->f.i.immediate);
	mov_reg32_reg32(EBX, EAX);
	if (fast_memory)
	{
		and_eax_imm32(0xDF800000);
		cmp_eax_imm32(0x80000000);
	} else
	{
		shr_reg32_imm8(EAX, 16);
		mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned long)writememd);
		cmp_reg32_imm32(EAX, (unsigned long)write_rdramd);
	}
	je_rj(47);

	mov_m32_imm32((void*)(&PC), (unsigned long)(dst + 1)); // 10
	mov_m32_reg32((unsigned long*)(&address), EBX); // 6
	mov_m32_reg32((unsigned long*)(&dword), ECX); // 6
	mov_m32_reg32((unsigned long*)(&dword) + 1, EDX); // 6
	shr_reg32_imm8(EBX, 16); // 3
	mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned long)writememd); // 7
	call_reg32(EBX); // 2
	mov_eax_memoffs32((unsigned long*)(&address)); // 5
	jmp_imm_short(20); // 2

	mov_reg32_reg32(EAX, EBX); // 2
	and_reg32_imm32(EBX, 0x7FFFFF); // 6
	mov_preg32pimm32_reg32(EBX, ((unsigned long)rdram) + 4, ECX); // 6
	mov_preg32pimm32_reg32(EBX, ((unsigned long)rdram) + 0, EDX); // 6

	mov_reg32_reg32(EBX, EAX);
	shr_reg32_imm8(EBX, 12);
	cmp_preg32pimm32_imm8(EBX, (unsigned long)invalid_code, 0);
	jne_rj(54);
	mov_reg32_reg32(ECX, EBX); // 2
	shl_reg32_imm8(EBX, 2); // 3
	mov_reg32_preg32pimm32(EBX, EBX, (unsigned long)blocks); // 6
	mov_reg32_preg32pimm32(EBX, EBX, (int)&actual->block - (int)actual); // 6
	and_eax_imm32(0xFFF); // 5
	shr_reg32_imm8(EAX, 2); // 3
	mov_reg32_imm32(EDX, sizeof(precomp_instr)); // 5
	mul_reg32(EDX); // 2
	mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&dst->ops - (int)dst); // 7
	cmp_reg32_imm32(EAX, (unsigned long)NOTCOMPILED); // 6
	je_rj(7); // 2
	mov_preg32pimm32_imm8(ECX, (unsigned long)invalid_code, 1); // 7
#endif
}

void genll()
{
	gencallinterp((unsigned long)LL, 0);
}

void gensc()
{
	gencallinterp((unsigned long)SC, 0);
}
