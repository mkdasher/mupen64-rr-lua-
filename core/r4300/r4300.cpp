/**
 * Mupen64 - r4300.c
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

#include "vcr.h"
#include "r4300.h"
#include "ops.h"
#include "../memory/memory.h"
#include "../memory/pif.h"
#include "exception.h"
#include "interrupt.h"
#include "macros.h"
#include "recomp.h"
#include <malloc.h>
#include <shared/services/LuaService.h>
#include <shared/Messenger.h>
#include "../memory/savestates.h"
#include <shared/helpers/StringHelpers.h>
#include <core/r4300/timers.h>
#include <shared/Config.hpp>
#include <shared/services/FrontendService.h>
// Threading crap

#include "gameshark.h"

#ifdef DBG
extern int debugger_mode;
extern void update_debugger();
#endif

std::thread emu_thread_handle;
std::thread audio_thread_handle;

std::atomic<bool> audio_thread_stop_requested;

// Lock to prevent emu state change race conditions
std::recursive_mutex emu_cs;

std::filesystem::path rom_path;

std::unique_ptr<Plugin> video_plugin;
std::unique_ptr<Plugin> audio_plugin;
std::unique_ptr<Plugin> input_plugin;
std::unique_ptr<Plugin> rsp_plugin;

extern bool ignore;
volatile bool emu_launched = false;
volatile bool emu_paused = false;
volatile bool core_executing = false;
volatile bool emu_resetting = false;
size_t g_total_frames = 0;
bool fullscreen = false;
bool gs_button = false;

unsigned long i, dynacore = 0, interpcore = 0;
int stop, llbit;
long long int reg[32], hi, lo;
long long int local_rs, local_rt;
unsigned long reg_cop0[32];
long local_rs32, local_rt32;
unsigned long jump_target;
float* reg_cop1_simple[32];
double* reg_cop1_double[32];
long reg_cop1_fgr_32[32];
long long int reg_cop1_fgr_64[32];
long FCR0, FCR31;
tlb tlb_e[32];
unsigned long delay_slot, skip_jump = 0, dyna_interp = 0, last_addr;
unsigned long long int debug_count = 0;
unsigned int next_interrupt, CIC_Chip;
precomp_instr* PC;
char invalid_code[0x100000];
std::atomic<bool> screen_invalidated = true;
precomp_block *blocks[0x100000], *actual;
int rounding_mode = ROUND_MODE;
int trunc_mode = TRUNC_MODE, round_mode = ROUND_MODE, ceil_mode = CEIL_MODE, floor_mode = FLOOR_MODE;
short x87_status_word;
void (*code)();

FILE* g_eeprom_file;
FILE* g_sram_file;
FILE* g_fram_file;
FILE* g_mpak_file;

/*#define check_memory() \
   if (!invalid_code[address>>12]) \
	   invalid_code[address>>12] = 1;*/

#define check_memory() \
   if (!invalid_code[address>>12]) \
       if (blocks[address>>12]->block[(address&0xFFF)/4].ops != NOTCOMPILED) \
	 invalid_code[address>>12] = 1;

std::filesystem::path get_rom_path()
{
	return rom_path;
}

void resume_emu()
{
	if (emu_launched)
	{
		emu_paused = 0;
	}

	Messenger::broadcast(Messenger::Message::EmuPausedChanged, (bool)emu_paused);
}


void pause_emu()
{
	if (emu_launched)
	{
		emu_paused = 1;
	}

	Messenger::broadcast(Messenger::Message::EmuPausedChanged, (bool)emu_paused);
}

void terminate_emu()
{
	stop = 1;
}

void NI()
{
	printf("NI() @ %x\n", (int)PC->addr);
	printf("opcode not implemented : ");
	if (PC->addr >= 0xa4000000 && PC->addr < 0xa4001000)
		printf("%x:%x\n", (int)PC->addr, (int)SP_DMEM[(PC->addr - 0xa4000000) / 4]);
	else
		printf("%x:%x\n", (int)PC->addr, (int)rdram[(PC->addr - 0x80000000) / 4]);
	stop = 1;
}

void RESERVED()
{
	printf("reserved opcode : ");
	if (PC->addr >= 0xa4000000 && PC->addr < 0xa4001000)
		printf("%x:%x\n", (int)PC->addr, (int)SP_DMEM[(PC->addr - 0xa4000000) / 4]);
	else
		printf("%x:%x\n", (int)PC->addr, (int)rdram[(PC->addr - 0x80000000) / 4]);
	stop = 1;
}

void FIN_BLOCK()
{
	if (!delay_slot)
	{
		jump_to((PC - 1)->addr + 4);
		PC->ops();
		if (dynacore) dyna_jump();
	} else
	{
		precomp_block* blk = actual;
		precomp_instr* inst = PC;
		jump_to((PC - 1)->addr + 4);

		if (!skip_jump)
		{
			PC->ops();
			actual = blk;
			PC = inst + 1;
		} else
			PC->ops();

		if (dynacore) dyna_jump();
	}
}

void J()
{
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump)
		PC = actual->block +
			(((((PC - 2)->f.j.inst_index << 2) | ((PC - 1)->addr & 0xF0000000)) - actual->start) >> 2);
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void J_OUT()
{
	jump_target = (PC->addr & 0xF0000000) | (PC->f.j.inst_index << 2);
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump)
		jump_to(jump_target);
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void J_IDLE()
{
	long skip;
	update_count();
	skip = next_interrupt - core_Count;
	if (skip > 3)
		core_Count += (skip & 0xFFFFFFFC);
	else J();
}

void JAL()
{
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump)
	{
		reg[31] = PC->addr;
		sign_extended(reg[31]);

		PC = actual->block +
			(((((PC - 2)->f.j.inst_index << 2) | ((PC - 1)->addr & 0xF0000000)) - actual->start) >> 2);
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void JAL_OUT()
{
	jump_target = (PC->addr & 0xF0000000) | (PC->f.j.inst_index << 2);
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump)
	{
		reg[31] = PC->addr;
		sign_extended(reg[31]);

		jump_to(jump_target);
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void JAL_IDLE()
{
	long skip;
	update_count();
	skip = next_interrupt - core_Count;
	if (skip > 3)
		core_Count += (skip & 0xFFFFFFFC);
	else JAL();
}

void BEQ()
{
	local_rs = core_irs;
	local_rt = core_irt;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (local_rs == local_rt && !skip_jump && !ignore)
		PC += (PC - 2)->f.i.immediate - 1;
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BEQ_OUT()
{
	local_rs = core_irs;
	local_rt = core_irt;
	jump_target = (long)PC->f.i.immediate;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump && local_rs == local_rt)
		jump_to(PC->addr + ((jump_target - 1) << 2));
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BEQ_IDLE()
{
	long skip;
	if (core_irs == core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BEQ();
	} else BEQ();
}

void BNE()
{
	local_rs = core_irs;
	local_rt = core_irt;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (local_rs != local_rt && !skip_jump)
		PC += (PC - 2)->f.i.immediate - 1;
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BNE_OUT()
{
	local_rs = core_irs;
	local_rt = core_irt;
	jump_target = (long)PC->f.i.immediate;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump && local_rs != local_rt)
		jump_to(PC->addr + ((jump_target - 1) << 2));
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BNE_IDLE()
{
	long skip;
	if (core_irs != core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BNE();
	} else BNE();
}

void BLEZ()
{
	local_rs = core_irs;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (local_rs <= 0 && !skip_jump)
		PC += (PC - 2)->f.i.immediate - 1;
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BLEZ_OUT()
{
	local_rs = core_irs;
	jump_target = (long)PC->f.i.immediate;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump && local_rs <= 0)
		jump_to(PC->addr + ((jump_target - 1) << 2));
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BLEZ_IDLE()
{
	long skip;
	if (core_irs <= core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BLEZ();
	} else BLEZ();
}

void BGTZ()
{
	local_rs = core_irs;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (local_rs > 0 && !skip_jump)
		PC += (PC - 2)->f.i.immediate - 1;
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BGTZ_OUT()
{
	local_rs = core_irs;
	jump_target = (long)PC->f.i.immediate;
	PC++;
	delay_slot = 1;
	PC->ops();
	update_count();
	delay_slot = 0;
	if (!skip_jump && local_rs > 0)
		jump_to(PC->addr + ((jump_target - 1) << 2));
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BGTZ_IDLE()
{
	long skip;
	if (core_irs > core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BGTZ();
	} else BGTZ();
}

void ADDI()
{
	irt32 = irs32 + core_iimmediate;
	sign_extended(core_irt);
	PC++;
}

void ADDIU()
{
	irt32 = irs32 + core_iimmediate;
	sign_extended(core_irt);
	PC++;
}

void SLTI()
{
	if (core_irs < core_iimmediate)
		core_irt = 1;
	else
		core_irt = 0;
	PC++;
}

void SLTIU()
{
	if ((unsigned long long)core_irs < (unsigned long long)((long long)core_iimmediate))
		core_irt = 1;
	else
		core_irt = 0;
	PC++;
}

void ANDI()
{
	core_irt = core_irs & (unsigned short)core_iimmediate;
	PC++;
}

void ORI()
{
	core_irt = core_irs | (unsigned short)core_iimmediate;
	PC++;
}

void XORI()
{
	core_irt = core_irs ^ (unsigned short)core_iimmediate;
	PC++;
}

void LUI()
{
	irt32 = core_iimmediate << 16;
	sign_extended(core_irt);
	PC++;
}

void BEQL()
{
	if (core_irs == core_irt)
	{
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			PC += (PC - 2)->f.i.immediate - 1;
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BEQL_OUT()
{
	if (core_irs == core_irt)
	{
		jump_target = (long)PC->f.i.immediate;
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			jump_to(PC->addr + ((jump_target - 1) << 2));
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BEQL_IDLE()
{
	long skip;
	if (core_irs == core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BEQL();
	} else BEQL();
}

void BNEL()
{
	if (core_irs != core_irt)
	{
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			PC += (PC - 2)->f.i.immediate - 1;
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BNEL_OUT()
{
	if (core_irs != core_irt)
	{
		jump_target = (long)PC->f.i.immediate;
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			jump_to(PC->addr + ((jump_target - 1) << 2));
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BNEL_IDLE()
{
	long skip;
	if (core_irs != core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BNEL();
	} else BNEL();
}

void BLEZL()
{
	if (core_irs <= 0)
	{
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			PC += (PC - 2)->f.i.immediate - 1;
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BLEZL_OUT()
{
	if (core_irs <= 0)
	{
		jump_target = (long)PC->f.i.immediate;
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			jump_to(PC->addr + ((jump_target - 1) << 2));
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BLEZL_IDLE()
{
	long skip;
	if (core_irs <= core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BLEZL();
	} else BLEZL();
}

void BGTZL()
{
	if (core_irs > 0)
	{
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			PC += (PC - 2)->f.i.immediate - 1;
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BGTZL_OUT()
{
	if (core_irs > 0)
	{
		jump_target = (long)PC->f.i.immediate;
		PC++;
		delay_slot = 1;
		PC->ops();
		update_count();
		delay_slot = 0;
		if (!skip_jump)
			jump_to(PC->addr + ((jump_target - 1) << 2));
	} else
	{
		PC += 2;
		update_count();
	}
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}

void BGTZL_IDLE()
{
	long skip;
	if (core_irs > core_irt)
	{
		update_count();
		skip = next_interrupt - core_Count;
		if (skip > 3)
			core_Count += (skip & 0xFFFFFFFC);
		else BGTZL();
	} else BGTZL();
}

void DADDI()
{
	core_irt = core_irs + core_iimmediate;
	PC++;
}

void DADDIU()
{
	core_irt = core_irs + core_iimmediate;
	PC++;
}

void LDL()
{
	unsigned long long int word = 0;
	PC++;
	switch ((core_lsaddr) & 7)
	{
	case 0:
		address = core_lsaddr;
		rdword = (unsigned long long int*)&core_lsrt;
		read_dword_in_memory();
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFF) | (word << 8);
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFF) | (word << 16);
		break;
	case 3:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFF) | (word << 24);
		break;
	case 4:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFF) | (word << 32);
		break;
	case 5:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFFLL) | (word << 40);
		break;
	case 6:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFFFFLL) | (word << 48);
		break;
	case 7:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFFFFFFLL) | (word << 56);
		break;
	}
}

void LDR()
{
	unsigned long long int word = 0;
	PC++;
	switch ((core_lsaddr) & 7)
	{
	case 0:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFFFFFF00LL) | (word >> 56);
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFFFF0000LL) | (word >> 48);
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFF000000LL) | (word >> 40);
		break;
	case 3:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFF00000000LL) | (word >> 32);
		break;
	case 4:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFF0000000000LL) | (word >> 24);
		break;
	case 5:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFF000000000000LL) | (word >> 16);
		break;
	case 6:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &word;
		read_dword_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFF00000000000000LL) | (word >> 8);
		break;
	case 7:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = (unsigned long long int*)&core_lsrt;
		read_dword_in_memory();
		break;
	}
}

void LB()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_byte_in_memory();
	if (address)
		sign_extendedb(core_lsrt);
}

void LH()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_hword_in_memory();
	if (address)
		sign_extendedh(core_lsrt);
}

void LWL()
{
	unsigned long long int word = 0;
	PC++;
	switch ((core_lsaddr) & 3)
	{
	case 0:
		address = core_lsaddr;
		rdword = (unsigned long long int*)&core_lsrt;
		read_word_in_memory();
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFF) | (word << 8);
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFF) | (word << 16);
		break;
	case 3:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFF) | (word << 24);
		break;
	}
	if (address)
		sign_extended(core_lsrt);
}

void LW()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_word_in_memory();
	if (address)
		sign_extended(core_lsrt);
}

void LBU()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_byte_in_memory();
}

void LHU()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_hword_in_memory();
}

void LWR()
{
	unsigned long long int word = 0;
	PC++;
	switch ((core_lsaddr) & 3)
	{
	case 0:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFFFFFF00LL) | ((word >> 24) & 0xFF);
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFFFF0000LL) | ((word >> 16) & 0xFFFF);
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &word;
		read_word_in_memory();
		if (address)
			core_lsrt = (core_lsrt & 0xFFFFFFFFFF000000LL) | ((word >> 8) & 0XFFFFFF);
		break;
	case 3:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = (unsigned long long int*)&core_lsrt;
		read_word_in_memory();
		if (address)
			sign_extended(core_lsrt);
	}
}

void LWU()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_word_in_memory();
}

void SB()
{
	PC++;
	address = core_lsaddr;
	g_byte = (unsigned char)(core_lsrt & 0xFF);
	write_byte_in_memory();
	check_memory();
}

void SH()
{
	PC++;
	address = core_lsaddr;
	hword = (unsigned short)(core_lsrt & 0xFFFF);
	write_hword_in_memory();
	check_memory();
}

void SWL()
{
	unsigned long long int old_word = 0;
	PC++;
	switch ((core_lsaddr) & 3)
	{
	case 0:
		address = (core_lsaddr) & 0xFFFFFFFC;
		word = (unsigned long)core_lsrt;
		write_word_in_memory();
		check_memory();
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		if (address)
		{
			word = ((unsigned long)core_lsrt >> 8) | (old_word & 0xFF000000);
			write_word_in_memory();
			check_memory();
		}
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		if (address)
		{
			word = ((unsigned long)core_lsrt >> 16) | (old_word & 0xFFFF0000);
			write_word_in_memory();
			check_memory();
		}
		break;
	case 3:
		address = core_lsaddr;
		g_byte = (unsigned char)(core_lsrt >> 24);
		write_byte_in_memory();
		check_memory();
		break;
	}
}

void SW()
{
	PC++;
	address = core_lsaddr;
	word = (unsigned long)(core_lsrt & 0xFFFFFFFF);
	write_word_in_memory();
	check_memory();
}

void SDL()
{
	unsigned long long int old_word = 0;
	PC++;
	switch ((core_lsaddr) & 7)
	{
	case 0:
		address = (core_lsaddr) & 0xFFFFFFF8;
		dword = core_lsrt;
		write_dword_in_memory();
		check_memory();
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = ((unsigned long long)core_lsrt >> 8) | (old_word & 0xFF00000000000000LL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = ((unsigned long long)core_lsrt >> 16) | (old_word & 0xFFFF000000000000LL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 3:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = ((unsigned long long)core_lsrt >> 24) | (old_word & 0xFFFFFF0000000000LL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 4:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = ((unsigned long long)core_lsrt >> 32) | (old_word & 0xFFFFFFFF00000000LL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 5:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = ((unsigned long long)core_lsrt >> 40) | (old_word & 0xFFFFFFFFFF000000LL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 6:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = ((unsigned long long)core_lsrt >> 48) | (old_word & 0xFFFFFFFFFFFF0000LL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 7:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = ((unsigned long long)core_lsrt >> 56) | (old_word & 0xFFFFFFFFFFFFFF00LL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	}
}

void SDR()
{
	unsigned long long int old_word = 0;
	PC++;
	switch ((core_lsaddr) & 7)
	{
	case 0:
		address = core_lsaddr;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = (core_lsrt << 56) | (old_word & 0x00FFFFFFFFFFFFFFLL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = (core_lsrt << 48) | (old_word & 0x0000FFFFFFFFFFFFLL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = (core_lsrt << 40) | (old_word & 0x000000FFFFFFFFFFLL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 3:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = (core_lsrt << 32) | (old_word & 0x00000000FFFFFFFFLL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 4:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = (core_lsrt << 24) | (old_word & 0x0000000000FFFFFFLL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 5:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = (core_lsrt << 16) | (old_word & 0x000000000000FFFFLL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 6:
		address = (core_lsaddr) & 0xFFFFFFF8;
		rdword = &old_word;
		read_dword_in_memory();
		if (address)
		{
			dword = (core_lsrt << 8) | (old_word & 0x00000000000000FFLL);
			write_dword_in_memory();
			check_memory();
		}
		break;
	case 7:
		address = (core_lsaddr) & 0xFFFFFFF8;
		dword = core_lsrt;
		write_dword_in_memory();
		check_memory();
		break;
	}
}

void SWR()
{
	unsigned long long int old_word = 0;
	PC++;
	switch ((core_lsaddr) & 3)
	{
	case 0:
		address = core_lsaddr;
		rdword = &old_word;
		read_word_in_memory();
		if (address)
		{
			word = ((unsigned long)core_lsrt << 24) | (old_word & 0x00FFFFFF);
			write_word_in_memory();
			check_memory();
		}
		break;
	case 1:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		if (address)
		{
			word = ((unsigned long)core_lsrt << 16) | (old_word & 0x0000FFFF);
			write_word_in_memory();
			check_memory();
		}
		break;
	case 2:
		address = (core_lsaddr) & 0xFFFFFFFC;
		rdword = &old_word;
		read_word_in_memory();
		if (address)
		{
			word = ((unsigned long)core_lsrt << 8) | (old_word & 0x000000FF);
			write_word_in_memory();
			check_memory();
		}
		break;
	case 3:
		address = (core_lsaddr) & 0xFFFFFFFC;
		word = (unsigned long)core_lsrt;
		write_word_in_memory();
		check_memory();
		break;
	}
}

void CACHE()
{
	PC++;
}

void LL()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_word_in_memory();
	if (address)
	{
		sign_extended(core_lsrt);
		llbit = 1;
	}
}

void LWC1()
{
	unsigned long long int temp;
	if (check_cop1_unusable()) return;
	PC++;
	address = core_lslfaddr;
	rdword = &temp;
	read_word_in_memory();
	if (address)
		*((long*)reg_cop1_simple[core_lslfft]) = *rdword;
}

void LDC1()
{
	if (check_cop1_unusable()) return;
	PC++;
	address = core_lslfaddr;
	rdword = (unsigned long long int*)reg_cop1_double[core_lslfft];
	read_dword_in_memory();
}

void LD()
{
	PC++;
	address = core_lsaddr;
	rdword = (unsigned long long int*)&core_lsrt;
	read_dword_in_memory();
}

void SC()
{
	/*PC++;
	printf("SC\n");
	if (llbit) {
	   address = lsaddr;
	   word = (unsigned long)(core_lsrt & 0xFFFFFFFF);
	   write_word_in_memory();
	}
	core_lsrt = llbit;*/

	PC++;
	if (llbit)
	{
		address = core_lsaddr;
		word = (unsigned long)(core_lsrt & 0xFFFFFFFF);
		write_word_in_memory();
		check_memory();
		llbit = 0;
		core_lsrt = 1;
	} else
	{
		core_lsrt = 0;
	}
}

void SWC1()
{
	if (check_cop1_unusable()) return;
	PC++;
	address = core_lslfaddr;
	word = *((long*)reg_cop1_simple[core_lslfft]);
	write_word_in_memory();
	check_memory();
}

void SDC1()
{
	if (check_cop1_unusable()) return;
	PC++;
	address = core_lslfaddr;
	dword = *((unsigned long long*)reg_cop1_double[core_lslfft]);
	write_dword_in_memory();
	check_memory();
}

void SD()
{
	PC++;
	address = core_lsaddr;
	dword = core_lsrt;
	write_dword_in_memory();
	check_memory();
}

void NOTCOMPILED()
{
	if ((PC->addr >> 16) == 0xa400)
		recompile_block((long*)SP_DMEM, blocks[0xa4000000 >> 12], PC->addr);
	else
	{
		unsigned long paddr = 0;
		if (PC->addr >= 0x80000000 && PC->addr < 0xc0000000) paddr = PC->addr;
			//else paddr = (tlb_LUT_r[PC->addr>>12]&0xFFFFF000)|(PC->addr&0xFFF);
		else paddr = virtual_to_physical_address(PC->addr, 2);
		if (paddr)
		{
			if ((paddr & 0x1FFFFFFF) >= 0x10000000)
			{
				//printf("not compiled rom:%x\n", paddr);
				recompile_block(
					(long*)rom + ((((paddr - (PC->addr - blocks[PC->addr >> 12]->start)) & 0x1FFFFFFF) - 0x10000000) >>
						2),
					blocks[PC->addr >> 12], PC->addr);
			} else
				recompile_block(
					(long*)(rdram + (((paddr - (PC->addr - blocks[PC->addr >> 12]->start)) & 0x1FFFFFFF) >> 2)),
					blocks[PC->addr >> 12], PC->addr);
		} else printf("not compiled exception\n");
	}
	PC->ops();
	if (dynacore)
		dyna_jump();
	//*return_address = (unsigned long)(blocks[PC->addr>>12]->code + PC->local_addr);
	//else
	//PC->ops();
}

void NOTCOMPILED2()
{
	NOTCOMPILED();
}

static inline unsigned long update_invalid_addr(unsigned long addr)
{
	if (addr >= 0x80000000 && addr < 0xa0000000)
	{
		if (invalid_code[addr >> 12]) invalid_code[(addr + 0x20000000) >> 12] = 1;
		if (invalid_code[(addr + 0x20000000) >> 12]) invalid_code[addr >> 12] = 1;
		return addr;
	} else if (addr >= 0xa0000000 && addr < 0xc0000000)
	{
		if (invalid_code[addr >> 12]) invalid_code[(addr - 0x20000000) >> 12] = 1;
		if (invalid_code[(addr - 0x20000000) >> 12]) invalid_code[addr >> 12] = 1;
		return addr;
	} else
	{
		unsigned long paddr = virtual_to_physical_address(addr, 2);
		if (paddr)
		{
			unsigned long beg_paddr = paddr - (addr - (addr & ~0xFFF));
			update_invalid_addr(paddr);
			if (invalid_code[(beg_paddr + 0x000) >> 12]) invalid_code[addr >> 12] = 1;
			if (invalid_code[(beg_paddr + 0xFFC) >> 12]) invalid_code[addr >> 12] = 1;
			if (invalid_code[addr >> 12]) invalid_code[(beg_paddr + 0x000) >> 12] = 1;
			if (invalid_code[addr >> 12]) invalid_code[(beg_paddr + 0xFFC) >> 12] = 1;
		}
		return paddr;
	}
}

#define addr jump_to_address
unsigned long jump_to_address;

inline void jump_to_func()
{
	//#ifdef _DEBUG
	//	printf("dyna jump: %p\n", addr);
	//#endif
	unsigned long paddr;
	if (skip_jump) return;
	paddr = update_invalid_addr(addr);
	if (!paddr) return;
	actual = blocks[addr >> 12];
	if (invalid_code[addr >> 12])
	{
		if (!blocks[addr >> 12])
		{
			blocks[addr >> 12] = (precomp_block*)malloc(sizeof(precomp_block));
			actual = blocks[addr >> 12];
			blocks[addr >> 12]->code = NULL;
			blocks[addr >> 12]->block = NULL;
			blocks[addr >> 12]->jumps_table = NULL;
		}
		blocks[addr >> 12]->start = addr & ~0xFFF;
		blocks[addr >> 12]->end = (addr & ~0xFFF) + 0x1000;
		init_block((long*)(rdram + (((paddr - (addr - blocks[addr >> 12]->start)) & 0x1FFFFFFF) >> 2)),
		           blocks[addr >> 12]);
	}
	PC = actual->block + ((addr - actual->start) >> 2);

	if (dynacore) dyna_jump();
}
#undef addr

int check_cop1_unusable()
{
	if (!(core_Status & 0x20000000))
	{
		core_Cause = (11 << 2) | 0x10000000;
		exception_general();
		return 1;
	}
	return 0;
}

void update_count()
{
	if (interpcore)
	{
		core_Count = core_Count + (interp_addr - last_addr) / 2;
		last_addr = interp_addr;
	} else
	{
		if (PC->addr < last_addr)
		{
			printf("PC->addr < last_addr\n");
		}
		core_Count = core_Count + (PC->addr - last_addr) / 2;
		last_addr = PC->addr;
	}
#ifdef COMPARE_CORE
	if (delay_slot)
		compare_core();
#endif
#ifdef DBG
	if (debugger_mode) update_debugger();
#endif
}

void init_blocks()
{
	int i;
	for (i = 0; i < 0x100000; i++)
	{
		invalid_code[i] = 1;
		blocks[i] = NULL;
	}
	blocks[0xa4000000 >> 12] = (precomp_block*)malloc(sizeof(precomp_block));
	invalid_code[0xa4000000 >> 12] = 1;
	blocks[0xa4000000 >> 12]->code = NULL;
	blocks[0xa4000000 >> 12]->block = NULL;
	blocks[0xa4000000 >> 12]->jumps_table = NULL;
	blocks[0xa4000000 >> 12]->start = 0xa4000000;
	blocks[0xa4000000 >> 12]->end = 0xa4001000;
	actual = blocks[0xa4000000 >> 12];
	init_block((long*)SP_DMEM, blocks[0xa4000000 >> 12]);
	PC = actual->block + (0x40 / 4);
#ifdef DBG
	if (debugger_mode) // debugger shows initial state (before 1st instruction).
		update_debugger();
#endif
}


void print_stop_debug()
{
	printf("PC=%x:%x\n", (unsigned int)(PC->addr),
	       (unsigned int)(rdram[(PC->addr & 0xFFFFFF) / 4]));
	for (int j = 0; j < 16; j++)
		printf("reg[%2d]:%8x%8x        reg[%d]:%8x%8x\n",
		       j,
		       (unsigned int)(reg[j] >> 32),
		       (unsigned int)reg[j],
		       j + 16,
		       (unsigned int)(reg[j + 16] >> 32),
		       (unsigned int)reg[j + 16]);
	printf("hi:%8x%8x        lo:%8x%8x\n",
	       (unsigned int)(hi >> 32),
	       (unsigned int)hi,
	       (unsigned int)(lo >> 32),
	       (unsigned int)lo);
	printf("Executed %llu (%x) instructions\n", debug_count, debug_count);
}

void core_start()
{
	core_executing = true;
	long long CRC = 0;
	unsigned int j;

	j = 0;
	debug_count = 0;
	printf("demarrage r4300\n");
	memcpy((char*)SP_DMEM + 0x40, rom + 0x40, 0xFBC);
	delay_slot = 0;
	stop = 0;
	for (i = 0; i < 32; i++)
	{
		reg[i] = 0;
		reg_cop0[i] = 0;
		reg_cop1_fgr_32[i] = 0;
		reg_cop1_fgr_64[i] = 0;

		reg_cop1_double[i] = (double*)&reg_cop1_fgr_64[i];
		reg_cop1_simple[i] = (float*)&reg_cop1_fgr_64[i];

		// --------------tlb------------------------
		tlb_e[i].mask = 0;
		tlb_e[i].vpn2 = 0;
		tlb_e[i].g = 0;
		tlb_e[i].asid = 0;
		tlb_e[i].pfn_even = 0;
		tlb_e[i].c_even = 0;
		tlb_e[i].d_even = 0;
		tlb_e[i].v_even = 0;
		tlb_e[i].pfn_odd = 0;
		tlb_e[i].c_odd = 0;
		tlb_e[i].d_odd = 0;
		tlb_e[i].v_odd = 0;
		tlb_e[i].r = 0;
		//tlb_e[i].check_parity_mask=0x1000;

		tlb_e[i].start_even = 0;
		tlb_e[i].end_even = 0;
		tlb_e[i].phys_even = 0;
		tlb_e[i].start_odd = 0;
		tlb_e[i].end_odd = 0;
		tlb_e[i].phys_odd = 0;
	}
	memset(tlb_LUT_r, 0, sizeof(tlb_LUT_r));
	memset(tlb_LUT_r, 0, sizeof(tlb_LUT_w));
	llbit = 0;
	hi = 0;
	lo = 0;
	FCR0 = 0x511;
	FCR31 = 0;

	//--------
	/*reg[20]=1;
	reg[22]=0x3F;
	reg[29]=0xFFFFFFFFA0400000LL;
	Random=31;
	Status=0x70400004;
	Config=0x66463;
	PRevID=0xb00;*/
	//--------

	// the following values are extracted from the pj64 source code
	// thanks to Zilmar and Jabo

	reg[6] = 0xFFFFFFFFA4001F0CLL;
	reg[7] = 0xFFFFFFFFA4001F08LL;
	reg[8] = 0x00000000000000C0LL;
	reg[10] = 0x0000000000000040LL;
	reg[11] = 0xFFFFFFFFA4000040LL;
	reg[29] = 0xFFFFFFFFA4001FF0LL;

	core_Random = 31;
	core_Status = 0x34000000;
	core_Config_cop0 = 0x6e463;
	core_PRevID = 0xb00;
	core_Count = 0x5000;
	core_Cause = 0x5C;
	core_Context = 0x7FFFF0;
	core_EPC = 0xFFFFFFFF;
	core_BadVAddr = 0xFFFFFFFF;
	core_ErrorEPC = 0xFFFFFFFF;

	for (i = 0x40 / 4; i < (0x1000 / 4); i++)
		CRC += SP_DMEM[i];
	switch (CRC)
	{
	case 0x000000D0027FDF31LL:
	case 0x000000CFFB631223LL:
		CIC_Chip = 1;
		break;
	case 0x000000D057C85244LL:
		CIC_Chip = 2;
		break;
	case 0x000000D6497E414BLL:
		CIC_Chip = 3;
		break;
	case 0x0000011A49F60E96LL:
		CIC_Chip = 5;
		break;
	case 0x000000D6D5BE5580LL:
		CIC_Chip = 6;
		break;
	default:
		CIC_Chip = 2;
	}

	switch (ROM_HEADER.Country_code & 0xFF)
	{
	case 0x44:
	case 0x46:
	case 0x49:
	case 0x50:
	case 0x53:
	case 0x55:
	case 0x58:
	case 0x59:
		switch (CIC_Chip)
		{
		case 2:
			reg[5] = 0xFFFFFFFFC0F1D859LL;
			reg[14] = 0x000000002DE108EALL;
			break;
		case 3:
			reg[5] = 0xFFFFFFFFD4646273LL;
			reg[14] = 0x000000001AF99984LL;
			break;
		case 5:
			SP_IMEM[1] = 0xBDA807FC;
			reg[5] = 0xFFFFFFFFDECAAAD1LL;
			reg[14] = 0x000000000CF85C13LL;
			reg[24] = 0x0000000000000002LL;
			break;
		case 6:
			reg[5] = 0xFFFFFFFFB04DC903LL;
			reg[14] = 0x000000001AF99984LL;
			reg[24] = 0x0000000000000002LL;
			break;
		}
		reg[23] = 0x0000000000000006LL;
		reg[31] = 0xFFFFFFFFA4001554LL;
		break;
	case 0x37:
	case 0x41:
	case 0x45:
	case 0x4A:
	default:
		switch (CIC_Chip)
		{
		case 2:
			reg[5] = 0xFFFFFFFFC95973D5LL;
			reg[14] = 0x000000002449A366LL;
			break;
		case 3:
			reg[5] = 0xFFFFFFFF95315A28LL;
			reg[14] = 0x000000005BACA1DFLL;
			break;
		case 5:
			SP_IMEM[1] = 0x8DA807FC;
			reg[5] = 0x000000005493FB9ALL;
			reg[14] = 0xFFFFFFFFC2C20384LL;
			break;
		case 6:
			reg[5] = 0xFFFFFFFFE067221FLL;
			reg[14] = 0x000000005CD2B70FLL;
			break;
		}
		reg[20] = 0x0000000000000001LL;
		reg[24] = 0x0000000000000003LL;
		reg[31] = 0xFFFFFFFFA4001550LL;
	}
	switch (CIC_Chip)
	{
	case 1:
		reg[22] = 0x000000000000003FLL;
		break;
	case 2:
		reg[1] = 0x0000000000000001LL;
		reg[2] = 0x000000000EBDA536LL;
		reg[3] = 0x000000000EBDA536LL;
		reg[4] = 0x000000000000A536LL;
		reg[12] = 0xFFFFFFFFED10D0B3LL;
		reg[13] = 0x000000001402A4CCLL;
		reg[15] = 0x000000003103E121LL;
		reg[22] = 0x000000000000003FLL;
		reg[25] = 0xFFFFFFFF9DEBB54FLL;
		break;
	case 3:
		reg[1] = 0x0000000000000001LL;
		reg[2] = 0x0000000049A5EE96LL;
		reg[3] = 0x0000000049A5EE96LL;
		reg[4] = 0x000000000000EE96LL;
		reg[12] = 0xFFFFFFFFCE9DFBF7LL;
		reg[13] = 0xFFFFFFFFCE9DFBF7LL;
		reg[15] = 0x0000000018B63D28LL;
		reg[22] = 0x0000000000000078LL;
		reg[25] = 0xFFFFFFFF825B21C9LL;
		break;
	case 5:
		SP_IMEM[0] = 0x3C0DBFC0;
		SP_IMEM[2] = 0x25AD07C0;
		SP_IMEM[3] = 0x31080080;
		SP_IMEM[4] = 0x5500FFFC;
		SP_IMEM[5] = 0x3C0DBFC0;
		SP_IMEM[6] = 0x8DA80024;
		SP_IMEM[7] = 0x3C0BB000;
		reg[2] = 0xFFFFFFFFF58B0FBFLL;
		reg[3] = 0xFFFFFFFFF58B0FBFLL;
		reg[4] = 0x0000000000000FBFLL;
		reg[12] = 0xFFFFFFFF9651F81ELL;
		reg[13] = 0x000000002D42AAC5LL;
		reg[15] = 0x0000000056584D60LL;
		reg[22] = 0x0000000000000091LL;
		reg[25] = 0xFFFFFFFFCDCE565FLL;
		break;
	case 6:
		reg[2] = 0xFFFFFFFFA95930A4LL;
		reg[3] = 0xFFFFFFFFA95930A4LL;
		reg[4] = 0x00000000000030A4LL;
		reg[12] = 0xFFFFFFFFBCB59510LL;
		reg[13] = 0xFFFFFFFFBCB59510LL;
		reg[15] = 0x000000007A3C07F4LL;
		reg[22] = 0x0000000000000085LL;
		reg[25] = 0x00000000465E3F72LL;
		break;
	}

	rounding_mode = ROUND_MODE;
	set_rounding();

	last_addr = 0xa4000040;
	//next_interrupt = 624999; //this is later overwritten with different value so what's the point...
	init_interrupt();
	interpcore = 0;

	if (!dynacore)
	{
		printf("interpreter\n");
		init_blocks();
		last_addr = PC->addr;
		while (!stop)
		{
			//if ((debug_count+Count) >= 0x78a8091) break; // obj 0x16aeb8a
			//if ((debug_count+Count) >= 0x16b1360)
			/*if ((debug_count+Count) >= 0xf203ae0)
			  {
			 printf ("PC=%x:%x\n", (unsigned int)(PC->addr),
			     (unsigned int)(rdram[(PC->addr&0xFFFFFF)/4]));
			 for (j=0; j<16; j++)
			   printf ("reg[%2d]:%8x%8x        reg[%d]:%8x%8x\n",
			       j,
			       (unsigned int)(reg[j] >> 32),
			       (unsigned int)reg[j],
			       j+16,
			       (unsigned int)(reg[j+16] >> 32),
			       (unsigned int)reg[j+16]);
			 printf("hi:%8x%8x        lo:%8x%8x\n",
			    (unsigned int)(hi >> 32),
			    (unsigned int)hi,
			    (unsigned int)(lo >> 32),
			    (unsigned int)lo);
			 printf("après %d instructions soit %x\n",(unsigned int)(debug_count+Count)
			    ,(unsigned int)(debug_count+Count));
			 getchar();
			  }*/
			/*if ((debug_count+Count) >= 0x80000000)
			  printf("%x:%x, %x\n", (int)PC->addr,
			     (int)rdram[(PC->addr & 0xFFFFFF)/4],
			     (int)(debug_count+Count));*/
#ifdef COMPARE_CORE
			if (PC->ops == FIN_BLOCK &&
				(PC->addr < 0x80000000 || PC->addr >= 0xc0000000))
				virtual_to_physical_address(PC->addr, 2);
			compare_core();
#endif
			PC->ops();
			ignore = false;
			/*if (j!= (Count & 0xFFF00000))
			  {
			 j = (Count & 0xFFF00000);
			 printf("%x\n", j);
			  }*/
			//check_PC;
#ifdef DBG
			if (debugger_mode)
				update_debugger();
#endif
		}
	} else if (dynacore == 2)
	{
		dynacore = 0;
		interpcore = 1;
		pure_interpreter();
	} else
	{
		dynacore = 1;
		printf("dynamic recompiler\n");
		init_blocks();

		auto code_addr = actual->code + (actual->block[0x40 / 4].local_addr);

		code = (void (*)(void))(code_addr);
		dyna_start(code);
		PC++;
	}
	debug_count += core_Count;
	print_stop_debug();
	for (i = 0; i < 0x100000; i++)
	{
		if (blocks[i] != NULL)
		{
			if (blocks[i]->block)
			{
				free(blocks[i]->block);
				blocks[i]->block = NULL;
			}
			if (blocks[i]->code)
			{
				free(blocks[i]->code);
				blocks[i]->code = NULL;
			}
			if (blocks[i]->jumps_table)
			{
				free(blocks[i]->jumps_table);
				blocks[i]->jumps_table = NULL;
			}
			free(blocks[i]);
			blocks[i] = NULL;
		}
	}
	if (!dynacore && interpcore) free(PC);
	core_executing = false;
}

bool open_core_file_stream(const std::filesystem::path& path, FILE** file)
{
	printf("[Core] Opening core stream from %s...\n", path.string().c_str());

	if (!exists(path))
	{
		FILE* f = fopen(path.string().c_str(), "w");
		if (!f)
		{
			return false;
		}
		fflush(f);
		fclose(f);
	}
	*file = fopen(path.string().c_str(), "rb+");
	return *file != nullptr;
}


void clear_save_data()
{
	open_core_file_stream(get_eeprom_path(), &g_eeprom_file);
	open_core_file_stream(get_sram_path(), &g_sram_file);
	open_core_file_stream(get_flashram_path(), &g_fram_file);
	open_core_file_stream(get_mempak_path(), &g_mpak_file);

	{
		memset(sram, 0, sizeof(sram));
		fseek(g_sram_file, 0, SEEK_SET);
		fwrite(sram, 1, 0x8000, g_sram_file);
	}
	{
		memset(eeprom, 0, sizeof(eeprom));
		fseek(g_eeprom_file, 0, SEEK_SET);
		fwrite(eeprom, 1, 0x800, g_eeprom_file);
	}
	{
		fseek(g_mpak_file, 0, SEEK_SET);
		for (auto buf : mempack)
		{
			memset(buf, 0, sizeof(mempack) / 4);
			fwrite(buf, 1, 0x800, g_mpak_file);
		}
	}

	fclose(g_eeprom_file);
	fclose(g_sram_file);
	fclose(g_fram_file);
	fclose(g_mpak_file);
}

void audio_thread()
{
	printf("Sound thread entering...\n");
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (audio_thread_stop_requested == true)
		{
			break;
		}

		if (fast_forward && g_config.fastforward_silent)
		{
			continue;
		}

		if (VCR::is_seeking())
		{
			continue;
		}

		aiUpdate(0);
	}
	printf("Sound thread exiting...\n");
}

void emu_thread()
{
	auto start_time = std::chrono::high_resolution_clock::now();

	// HACK: We sleep between each plugin load, as that seems to remedy various plugins failing to initialize correctly.
	auto gfx_plugin_thread = std::thread([] { video_plugin->load_into_globals(); });
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	auto audio_plugin_thread = std::thread([] { audio_plugin->load_into_globals(); });
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	auto input_plugin_thread = std::thread([] { input_plugin->load_into_globals(); });
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	auto rsp_plugin_thread = std::thread([] { rsp_plugin->load_into_globals(); });

	gfx_plugin_thread.join();
	audio_plugin_thread.join();
	input_plugin_thread.join();
	rsp_plugin_thread.join();

	init_memory();

	romOpen_gfx();
	romOpen_input();
	romOpen_audio();

	dynacore = g_config.core_type;

	audio_thread_handle = std::thread(audio_thread);

	Messenger::broadcast(Messenger::Message::EmuLaunchedChanged, true);
	Messenger::broadcast(Messenger::Message::EmuStartingChanged, false);
	LuaService::call_reset();

	printf("[Core] Emu thread entry took %dms\n", static_cast<int>((std::chrono::high_resolution_clock::now() - start_time).count() / 1'000'000));
	core_start();

	romClosed_gfx();
	romClosed_audio();
	romClosed_input();
	romClosed_RSP();

	closeDLL_gfx();
	closeDLL_audio();
	closeDLL_input();
	closeDLL_RSP();

	emu_paused = true;
	emu_launched = false;

	if (!emu_resetting)
	{
		Messenger::broadcast(Messenger::Message::EmuLaunchedChanged, false);
	}
}

Core::Result vr_start_rom(std::filesystem::path path)
{
	auto start_time = std::chrono::high_resolution_clock::now();

	std::unique_lock lock(emu_cs, std::try_to_lock);
	if (!lock.owns_lock())
	{
		printf("[Core] vr_start_rom busy!\n");
		return Core::Result::Busy;
	}

	// We can't overwrite core. Emu needs to stop first, but that might fail...
	if (emu_launched)
	{
		auto result = vr_close_rom();
		if (result != Core::Result::Ok)
		{
			printf("[Core] Failed to close rom before starting rom.\n");
			return result;
		}
	}

	Messenger::broadcast(Messenger::Message::EmuStartingChanged, true);

	// If we get a movie instead of a rom, we try to search the available rom lists to find one matching the movie
	if (path.extension() == ".m64")
	{
		t_movie_header movie_header{};
		if (VCR::parse_header(path, &movie_header) != VCR::Result::Ok)
		{
			Messenger::broadcast(Messenger::Message::CoreResult, Core::Result::RomInvalid);
			Messenger::broadcast(Messenger::Message::EmuStartingChanged, false);
			return Core::Result::RomInvalid;
		}

		const auto matching_rom = FrontendService::find_available_rom([&](auto header)
		{
			strtrim((char*)header.nom, sizeof(header.nom));
			return movie_header.rom_crc1 == header.CRC1 && !_strnicmp((const char*)header.nom, movie_header.rom_name, 20);
		});

		if (matching_rom.empty())
		{
			Messenger::broadcast(Messenger::Message::CoreResult, Core::Result::NoMatchingRom);
			Messenger::broadcast(Messenger::Message::EmuStartingChanged, false);
			return Core::Result::NoMatchingRom;
		}

		path = matching_rom;
	}

	rom_path = path;

	printf("Loading plugins\n");

	if (video_plugin.get() && audio_plugin.get() && input_plugin.get() && rsp_plugin.get()
		&& video_plugin->path() == g_config.selected_video_plugin
		&& audio_plugin->path() == g_config.selected_audio_plugin
		&& input_plugin->path() == g_config.selected_input_plugin
		&& rsp_plugin->path() == g_config.selected_rsp_plugin)
	{
		printf("[Core] Plugins unchanged, reusing...\n");
	} else
	{
		video_plugin.reset();
		audio_plugin.reset();
		input_plugin.reset();
		rsp_plugin.reset();

		auto video_pl = Plugin::create(g_config.selected_video_plugin);
		auto audio_pl = Plugin::create(g_config.selected_audio_plugin);
		auto input_pl = Plugin::create(g_config.selected_input_plugin);
		auto rsp_pl = Plugin::create(g_config.selected_rsp_plugin);

		if (!video_pl.has_value() || !audio_pl.has_value() || !input_pl.has_value() || !rsp_pl.has_value())
		{
			video_pl.reset();
			audio_pl.reset();
			input_pl.reset();
			rsp_pl.reset();
			Messenger::broadcast(Messenger::Message::CoreResult, Core::Result::PluginError);
			Messenger::broadcast(Messenger::Message::EmuStartingChanged, false);
			return Core::Result::PluginError;
		}

		video_plugin = std::move(video_pl.value());
		audio_plugin = std::move(audio_pl.value());
		input_plugin = std::move(input_pl.value());
		rsp_plugin = std::move(rsp_pl.value());
	}

	if (!rom_load(path.string().c_str()))
	{
		Messenger::broadcast(Messenger::Message::CoreResult, Core::Result::RomInvalid);
		Messenger::broadcast(Messenger::Message::EmuStartingChanged, false);
		return Core::Result::RomInvalid;
	}

	// Open all the save file streams
	if(!open_core_file_stream(get_eeprom_path(), &g_eeprom_file)
		|| !open_core_file_stream(get_sram_path(), &g_sram_file)
		|| !open_core_file_stream(get_flashram_path(), &g_fram_file)
		|| !open_core_file_stream(get_mempak_path(), &g_mpak_file))
	{
		Messenger::broadcast(Messenger::Message::CoreResult, Core::Result::FileOpenFailed);
		Messenger::broadcast(Messenger::Message::EmuStartingChanged, false);
		return Core::Result::FileOpenFailed;
	}

	timer_init(g_config.fps_modifier, &ROM_HEADER);

	printf("[Core] vr_start_rom entry took %dms\n", static_cast<int>((std::chrono::high_resolution_clock::now() - start_time).count() / 1'000'000));

	emu_paused = false;
	emu_launched = true;
	emu_thread_handle = std::thread(emu_thread);

	// We need to wait until the core is actually done and running before we can continue, because we release the lock
	// If we return too early (before core is ready to also be killed), then another start or close might come in during the core initialization (catastrophe)
	while (!core_executing);

	return Core::Result::Ok;
}

Core::Result vr_close_rom(bool stop_vcr)
{
	std::unique_lock lock(emu_cs, std::try_to_lock);
	if (!lock.owns_lock())
	{
		printf("[Core] vr_close_rom busy!\n");
		return Core::Result::Busy;
	}

	if (!emu_launched)
	{
		return Core::Result::NotRunning;
	}

	resume_emu();

	audio_thread_stop_requested = true;
	audio_thread_handle.join();
	audio_thread_stop_requested = false;

	if (stop_vcr)
	{
		VCR::stop_all();
	}

	Messenger::broadcast(Messenger::Message::EmuStopping, nullptr);

	printf("[Core] Stopping emulation thread...\n");

	// we signal the core to stop, then wait until thread exits
	terminate_emu();

	emu_thread_handle.join();

	fflush(g_eeprom_file);
	fflush(g_sram_file);
	fflush(g_fram_file);
	fflush(g_mpak_file);
	fclose(g_eeprom_file);
	fclose(g_sram_file);
	fclose(g_fram_file);
	fclose(g_mpak_file);

	return Core::Result::Ok;
}

Core::Result vr_reset_rom(bool reset_save_data, bool stop_vcr)
{
	std::unique_lock lock(emu_cs, std::try_to_lock);
	if (!lock.owns_lock())
	{
		printf("[Core] vr_reset_rom busy!\n");
		return Core::Result::Busy;
	}

	if (!emu_launched)
		return Core::Result::NotRunning;

	// why is it so damned difficult to reset the game?
	// right now it's hacked to exit to the GUI then re-load the ROM,
	// but it should be possible to reset the game while it's still running
	// simply by clearing out some memory and maybe notifying the plugins...
	frame_advancing = false;
	emu_resetting = true;

	Core::Result result = vr_close_rom(stop_vcr);
	if (result != Core::Result::Ok)
	{
		emu_resetting = false;
		Messenger::broadcast(Messenger::Message::ResetCompleted, nullptr);
		return result;
	}

	if (reset_save_data)
	{
		clear_save_data();
	}

	result = vr_start_rom(rom_path);
	if (result != Core::Result::Ok)
	{
		emu_resetting = false;
		Messenger::broadcast(Messenger::Message::ResetCompleted, nullptr);
		return result;
	}

	emu_resetting = false;
	Messenger::broadcast(Messenger::Message::ResetCompleted, nullptr);
	return Core::Result::Ok;
}

void toggle_fullscreen_mode()
{
	changeWindow();
	fullscreen ^= true;
	Messenger::broadcast(Messenger::Message::FullscreenChanged, fullscreen);
}

bool vr_is_fullscreen()
{
	return fullscreen;
}

bool get_gs_button()
{
	return gs_button;
}

void set_gs_button(bool value)
{
	gs_button = value;
}
