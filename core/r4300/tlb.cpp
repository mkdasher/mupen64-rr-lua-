/**
 * Mupen64 - tlb.c
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
#include "recomph.h"
#include "interrupt.h"
#include "../memory/memory.h"

#include <lib/zlib/zlib.h>

uLong ZEXPORT adler32(uLong adler, const Bytef* buf, uInt len);

void TLBR()
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
	PC++;
}

void TLBWI()
{
	unsigned int i;

	if (tlb_e[core_Index & 0x3F].v_even)
	{
		for (i = tlb_e[core_Index & 0x3F].start_even >> 12; i <= tlb_e[core_Index & 0x3F].end_even >> 12; i++)
		{
			if (!invalid_code[i] && (invalid_code[tlb_LUT_r[i] >> 12] ||
				invalid_code[(tlb_LUT_r[i] >> 12) + 0x20000]))
				invalid_code[i] = 1;
			if (!invalid_code[i])
			{
				/*int j;
				md5_state_t state;
				md5_byte_t digest[16];
				md5_init(&state);
				md5_append(&state,
				       (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
				       0x1000);
				md5_finish(&state, digest);
				for (j=0; j<16; j++) blocks[i]->md5[j] = digest[j];*/

				blocks[i]->adler32 = adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000);

				invalid_code[i] = 1;
			} else if (blocks[i])
			{
				/*int j;
				for (j=0; j<16; j++) blocks[i]->md5[j] = 0;*/
				blocks[i]->adler32 = 0;
			}
			tlb_LUT_r[i] = 0;
		}
		if (tlb_e[core_Index & 0x3F].d_even)
			for (i = tlb_e[core_Index & 0x3F].start_even >> 12; i <= tlb_e[core_Index & 0x3F].end_even >> 12; i++)
				tlb_LUT_w[i] = 0;
	}
	if (tlb_e[core_Index & 0x3F].v_odd)
	{
		for (i = tlb_e[core_Index & 0x3F].start_odd >> 12; i <= tlb_e[core_Index & 0x3F].end_odd >> 12; i++)
		{
			if (!invalid_code[i] && (invalid_code[tlb_LUT_r[i] >> 12] ||
				invalid_code[(tlb_LUT_r[i] >> 12) + 0x20000]))
				invalid_code[i] = 1;
			if (!invalid_code[i])
			{
				/*int j;
				md5_state_t state;
				md5_byte_t digest[16];
				md5_init(&state);
				md5_append(&state,
				       (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
				       0x1000);
				md5_finish(&state, digest);
				for (j=0; j<16; j++) blocks[i]->md5[j] = digest[j];*/

				blocks[i]->adler32 = adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000);

				invalid_code[i] = 1;
			} else if (blocks[i])
			{
				/*int j;
				for (j=0; j<16; j++) blocks[i]->md5[j] = 0;*/
				blocks[i]->adler32 = 0;
			}
			tlb_LUT_r[i] = 0;
		}
		if (tlb_e[core_Index & 0x3F].d_odd)
			for (i = tlb_e[core_Index & 0x3F].start_odd >> 12; i <= tlb_e[core_Index & 0x3F].end_odd >> 12; i++)
				tlb_LUT_w[i] = 0;
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

		for (i = tlb_e[core_Index & 0x3F].start_even >> 12; i <= tlb_e[core_Index & 0x3F].end_even >> 12; i++)
		{
			/*if (blocks[i] && (blocks[i]->md5[0] || blocks[i]->md5[1] ||
			          blocks[i]->md5[2] || blocks[i]->md5[3]))
			  {
			 int j;
			 int equal = 1;
			 md5_state_t state;
			 md5_byte_t digest[16];
			 md5_init(&state);
			 md5_append(&state,
			        (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
			        0x1000);
			 md5_finish(&state, digest);
			 for (j=0; j<16; j++)
			   if (digest[j] != blocks[i]->md5[j])
			     equal = 0;
			 if (equal) invalid_code[i] = 0;
			 }*/
			if (blocks[i] && blocks[i]->adler32)
			{
				if (blocks[i]->adler32 == adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000))
					invalid_code[i] = 0;
			}
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

		for (i = tlb_e[core_Index & 0x3F].start_odd >> 12; i <= tlb_e[core_Index & 0x3F].end_odd >> 12; i++)
		{
			/*if (blocks[i] && (blocks[i]->md5[0] || blocks[i]->md5[1] ||
			          blocks[i]->md5[2] || blocks[i]->md5[3]))
			  {
			 int j;
			 int equal = 1;
			 md5_state_t state;
			 md5_byte_t digest[16];
			 md5_init(&state);
			 md5_append(&state,
			        (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
			        0x1000);
			 md5_finish(&state, digest);
			 for (j=0; j<16; j++)
			   if (digest[j] != blocks[i]->md5[j])
			     equal = 0;
			 if (equal) invalid_code[i] = 0;
			  }*/
			if (blocks[i] && blocks[i]->adler32)
			{
				if (blocks[i]->adler32 == adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000))
					invalid_code[i] = 0;
			}
		}
	}
	PC++;
}

void TLBWR()
{
	unsigned int i;
	update_count();
	core_Random = (core_Count / 2 % (32 - core_Wired)) + core_Wired;

	if (tlb_e[core_Random].v_even)
	{
		for (i = tlb_e[core_Random].start_even >> 12; i <= tlb_e[core_Random].end_even >> 12; i++)
		{
			if (!invalid_code[i] && (invalid_code[tlb_LUT_r[i] >> 12] ||
				invalid_code[(tlb_LUT_r[i] >> 12) + 0x20000]))
				invalid_code[i] = 1;
			if (!invalid_code[i])
			{
				/*int j;
				md5_state_t state;
				md5_byte_t digest[16];
				md5_init(&state);
				md5_append(&state,
				       (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
				       0x1000);
				md5_finish(&state, digest);
				for (j=0; j<16; j++) blocks[i]->md5[j] = digest[j];*/

				blocks[i]->adler32 = adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000);

				invalid_code[i] = 1;
			} else if (blocks[i])
			{
				/*int j;
				for (j=0; j<16; j++) blocks[i]->md5[j] = 0;*/
				blocks[i]->adler32 = 0;
			}
			tlb_LUT_r[i] = 0;
		}
		if (tlb_e[core_Random].d_even)
			for (i = tlb_e[core_Random].start_even >> 12; i <= tlb_e[core_Random].end_even >> 12; i++)
				tlb_LUT_w[i] = 0;
	}
	if (tlb_e[core_Random].v_odd)
	{
		for (i = tlb_e[core_Random].start_odd >> 12; i <= tlb_e[core_Random].end_odd >> 12; i++)
		{
			if (!invalid_code[i] && (invalid_code[tlb_LUT_r[i] >> 12] ||
				invalid_code[(tlb_LUT_r[i] >> 12) + 0x20000]))
				invalid_code[i] = 1;
			if (!invalid_code[i])
			{
				/*int j;
				md5_state_t state;
				md5_byte_t digest[16];
				md5_init(&state);
				md5_append(&state,
				       (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
				       0x1000);
				md5_finish(&state, digest);
				for (j=0; j<16; j++) blocks[i]->md5[j] = digest[j];*/

				blocks[i]->adler32 = adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000);

				invalid_code[i] = 1;
			} else if (blocks[i])
			{
				/*int j;
				for (j=0; j<16; j++) blocks[i]->md5[j] = 0;*/
				blocks[i]->adler32 = 0;
			}
			tlb_LUT_r[i] = 0;
		}
		if (tlb_e[core_Random].d_odd)
			for (i = tlb_e[core_Random].start_odd >> 12; i <= tlb_e[core_Random].end_odd >> 12; i++)
				tlb_LUT_w[i] = 0;
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

		for (i = tlb_e[core_Random].start_even >> 12; i <= tlb_e[core_Random].end_even >> 12; i++)
		{
			/*if (blocks[i] && (blocks[i]->md5[0] || blocks[i]->md5[1] ||
			          blocks[i]->md5[2] || blocks[i]->md5[3]))
			  {
			 int j;
			 int equal = 1;
			 md5_state_t state;
			 md5_byte_t digest[16];
			 md5_init(&state);
			 md5_append(&state,
			        (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
			        0x1000);
			 md5_finish(&state, digest);
			 for (j=0; j<16; j++)
			   if (digest[j] != blocks[i]->md5[j])
			     equal = 0;
			 if (equal) invalid_code[i] = 0;
			  }*/
			if (blocks[i] && blocks[i]->adler32)
			{
				if (blocks[i]->adler32 == adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000))
					invalid_code[i] = 0;
			}
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

		for (i = tlb_e[core_Random].start_odd >> 12; i <= tlb_e[core_Random].end_odd >> 12; i++)
		{
			/*if (blocks[i] && (blocks[i]->md5[0] || blocks[i]->md5[1] ||
			 blocks[i]->md5[2] || blocks[i]->md5[3]))
			  {
			 int j;
			 int equal = 1;
			 md5_state_t state;
			 md5_byte_t digest[16];
			 md5_init(&state);
			 md5_append(&state,
			        (const md5_byte_t*)&rdram[(tlb_LUT_r[i]&0x7FF000)/4],
			        0x1000);
			 md5_finish(&state, digest);
			 for (j=0; j<16; j++)
			   if (digest[j] != blocks[i]->md5[j])
			     equal = 0;
			 if (equal) invalid_code[i] = 0;
			 }*/
			if (blocks[i] && blocks[i]->adler32)
			{
				if (blocks[i]->adler32 == adler32(0, (const Bytef*)&rdram[(tlb_LUT_r[i] & 0x7FF000) / 4], 0x1000))
					invalid_code[i] = 0;
			}
		}
	}
	PC++;
}

void TLBP()
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
	PC++;
}

int jump_marker = 0;

void ERET()
{
	update_count();
	if (core_Status & 0x4)
	{
		printf("erreur dans ERET\n");
		stop = 1;
	} else
	{
		core_Status &= 0xFFFFFFFD;
		jump_to(core_EPC);
	}
	llbit = 0;
	check_interrupt();
	last_addr = PC->addr;
	if (next_interrupt <= core_Count) gen_interrupt();
}
