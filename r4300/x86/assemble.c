/**
 * Mupen64 - assemble.c
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
#include "../recomph.h"
#include <malloc.h>
#include "../r4300.h"

typedef struct jump_table
{
    unsigned long mi_addr;
    unsigned long pc_addr;
} jump_table;

static jump_table* jumps_table = nullptr;
static int jumps_number, max_jumps_number;

void init_assembler(void* block_jumps_table, const int block_jumps_number)
{
    if (block_jumps_table)
    {
        jumps_table = (jump_table*)block_jumps_table;
        jumps_number = block_jumps_number;
        max_jumps_number = jumps_number;
    }
    else
    {
        jumps_table = (jump_table*)malloc(1000 * sizeof(jump_table));
        jumps_number = 0;
        max_jumps_number = 1000;
    }
}

void free_assembler(void** block_jumps_table, int* block_jumps_number)
{
    *block_jumps_table = jumps_table;
    *block_jumps_number = jumps_number;
}

static void add_jump(const unsigned long pc_addr, const unsigned long mi_addr)
{
    if (jumps_number == max_jumps_number)
    {
        max_jumps_number += 1000;
        jumps_table = (jump_table*)realloc(jumps_table, max_jumps_number * sizeof(jump_table));
    }
    jumps_table[jumps_number].pc_addr = pc_addr;
    jumps_table[jumps_number].mi_addr = mi_addr;
    jumps_number++;
}

void passe2(precomp_instr* dest, const int start, const int end, precomp_block* block)
{
    unsigned int addr_dest;
    build_wrappers(dest, start, end, block);
    const unsigned int real_code_length = code_length;

    for (unsigned int i = 0; i < (unsigned int)jumps_number; i++)
    {
        code_length = (int)jumps_table[i].pc_addr;
        if (dest[(jumps_table[i].mi_addr - dest[0].addr) / 4].reg_cache_infos.need_map)
        {
            addr_dest = (unsigned int)dest[(jumps_table[i].mi_addr - dest[0].addr) / 4].reg_cache_infos.jump_wrapper;
            put32(addr_dest - ((unsigned int)block->code + code_length) - 4);
        }
        else
        {
            addr_dest = dest[(jumps_table[i].mi_addr - dest[0].addr) / 4].local_addr;
            put32(addr_dest - code_length - 4);
        }
    }
    code_length = (int)real_code_length;
}

inline void put8(const unsigned char octet)
{
    (*inst_pointer)[code_length] = octet;
    code_length++;
    if (code_length == max_code_length)
    {
        max_code_length += 1000;
        *inst_pointer = (unsigned char*)realloc(*inst_pointer, max_code_length);
    }
}

inline void put32(const unsigned long dword)
{
    if ((code_length + 4) >= max_code_length)
    {
        max_code_length += 1000;
        *inst_pointer = (unsigned char*)realloc(*inst_pointer, max_code_length);
    }
    *reinterpret_cast<unsigned long*>(&(*inst_pointer)[code_length]) = dword;
    code_length += 4;
}

inline void put16(const unsigned short word)
{
    if ((code_length + 2) >= max_code_length)
    {
        max_code_length += 1000;
        *inst_pointer = (unsigned char*)realloc(*inst_pointer, max_code_length);
    }
    *reinterpret_cast<unsigned short*>(&(*inst_pointer)[code_length]) = word;
    code_length += 2;
}

void push_reg32(const int reg32)
{
    put8(unsigned char(0x50 + reg32));
}

void pop_reg32(const int reg32)
{
    put8(unsigned char(0x58 + reg32));
}

void mov_eax_memoffs32(void* _memoffs32)
{
	auto memoffs32 = (unsigned long*)_memoffs32;
    put8(0xA1);
    put32((unsigned int)(memoffs32));
}

void mov_memoffs32_eax(void* _memoffs32)
{
	auto memoffs32 = (unsigned long*)_memoffs32;
    put8(0xA3);
    put32((unsigned int)(memoffs32));
}

void mov_ax_memoffs16(unsigned short* memoffs16)
{
    put8(0x66);
    put8(0xA1);
    put32((unsigned int)(memoffs16));
}

void mov_memoffs16_ax(unsigned short* memoffs16)
{
    put8(0x66);
    put8(0xA3);
    put32((unsigned int)(memoffs16));
}

void mov_al_memoffs8(unsigned char* memoffs8)
{
    put8(0xA0);
    put32((unsigned int)(memoffs8));
}

void mov_memoffs8_al(unsigned char* memoffs8)
{
    put8(0xA2);
    put32((unsigned int)(memoffs8));
}

void mov_m8_imm8(unsigned char* m8, const unsigned char imm8)
{
    put8(0xC6);
    put8(0x05);
    put32((unsigned long)(m8));
    put8(imm8);
}

void mov_m8_reg8(unsigned char* m8, const int reg8)
{
    put8(0x88);
    put8(unsigned char((reg8 << 3) | 5));
    put32((unsigned long)(m8));
}

void mov_reg16_m16(const int reg16, unsigned short* m16)
{
    put8(0x66);
    put8(0x8B);
    put8(unsigned char((reg16 << 3) | 5));
    put32((unsigned long)(m16));
}

void mov_m16_reg16(unsigned short* m16, const int reg16)
{
    put8(0x66);
    put8(0x89);
    put8(unsigned char((reg16 << 3) | 5));
    put32((unsigned long)(m16));
}

void cmp_reg32_m32(const int reg32, void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x3B);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned int)(m32));
}

void cmp_reg32_reg32(const int reg1, const int reg2)
{
    put8(0x39);
    put8(unsigned char((reg2 << 3) | reg1 | 0xC0));
}

void cmp_reg32_imm8(const int reg32, const unsigned char imm8)
{
    put8(0x83);
    put8(unsigned char(0xF8 + reg32));
    put8(imm8);
}

void cmp_preg32_pimm32_imm8(const int reg32, const unsigned long imm32, const unsigned char imm8)
{
    put8(0x80);
    put8(unsigned char(0xB8 + reg32));
    put32(imm32);
    put8(imm8);
}

void cmp_reg32_imm32(const int reg32, const unsigned long imm32)
{
    put8(0x81);
    put8(unsigned char(0xF8 + reg32));
    put32(imm32);
}

void test_reg32_imm32(const int reg32, const unsigned long imm32)
{
    put8(0xF7);
    put8(unsigned char(0xC0 + reg32));
    put32(imm32);
}

void test_m32_imm32(void* _m32, const unsigned long imm32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xF7);
    put8(0x05);
    put32((unsigned int)m32);
    put32(imm32);
}

void test_al_imm8(const unsigned char imm8)
{
    put8(0xA8);
    put8(imm8);
}

void cmp_al_imm8(const unsigned char imm8)
{
    put8(0x3C);
    put8(imm8);
}

void add_m32_reg32(void* _m32, const int reg32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x01);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned int)(m32));
}

void sub_reg32_m32(const int reg32, void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x2B);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned int)(m32));
}

void sub_reg32_reg32(const int reg1, const int reg2)
{
    put8(0x29);
    put8(unsigned char((reg2 << 3) | reg1 | 0xC0));
}

void sbb_reg32_reg32(const int reg1, const int reg2)
{
    put8(0x19);
    put8(unsigned char((reg2 << 3) | reg1 | 0xC0));
}

void sub_reg32_imm32(const int reg32, const unsigned long imm32)
{
    put8(0x81);
    put8(unsigned char(0xE8 + reg32));
    put32(imm32);
}

void sub_eax_imm32(const unsigned long imm32)
{
    put8(0x2D);
    put32(imm32);
}

void jne_rj(const unsigned char saut)
{
    put8(0x75);
    put8(saut);
}

void je_rj(const unsigned char saut)
{
    put8(0x74);
    put8(saut);
}

void jb_rj(const unsigned char saut)
{
    put8(0x72);
    put8(saut);
}

void jbe_rj(const unsigned char saut)
{
    put8(0x76);
    put8(saut);
}

void ja_rj(const unsigned char saut)
{
    put8(0x77);
    put8(saut);
}

void jae_rj(const unsigned char saut)
{
    put8(0x73);
    put8(saut);
}

void jle_rj(const unsigned char saut)
{
    put8(0x7E);
    put8(saut);
}

void jge_rj(const unsigned char saut)
{
    put8(0x7D);
    put8(saut);
}

void jg_rj(const unsigned char saut)
{
    put8(0x7F);
    put8(saut);
}

void jl_rj(const unsigned char saut)
{
    put8(0x7C);
    put8(saut);
}

void jp_rj(const unsigned char saut)
{
    put8(0x7A);
    put8(saut);
}

void je_near(const unsigned long mi_addr)
{
    put8(0x0F);
    put8(0x84);
    put32(0);
    add_jump(code_length - 4, mi_addr);
}

void je_near_rj(const unsigned long saut)
{
    put8(0x0F);
    put8(0x84);
    put32(saut);
}

void jl_near(const unsigned long mi_addr)
{
    put8(0x0F);
    put8(0x8C);
    put32(0);
    add_jump(code_length - 4, mi_addr);
}

void jl_near_rj(const unsigned long saut)
{
    put8(0x0F);
    put8(0x8C);
    put32(saut);
}

void jne_near(const unsigned long mi_addr)
{
    put8(0x0F);
    put8(0x85);
    put32(0);
    add_jump(code_length - 4, mi_addr);
}

void jne_near_rj(const unsigned long saut)
{
    put8(0x0F);
    put8(0x85);
    put32(saut);
}

void jge_near(const unsigned long mi_addr)
{
    put8(0x0F);
    put8(0x8D);
    put32(0);
    add_jump(code_length - 4, mi_addr);
}

void jge_near_rj(const unsigned long saut)
{
    put8(0x0F);
    put8(0x8D);
    put32(saut);
}

void jg_near(const unsigned long mi_addr)
{
    put8(0x0F);
    put8(0x8F);
    put32(0);
    add_jump(code_length - 4, mi_addr);
}

void jle_near(const unsigned long mi_addr)
{
    put8(0x0F);
    put8(0x8E);
    put32(0);
    add_jump(code_length - 4, mi_addr);
}

void jle_near_rj(const unsigned long saut)
{
    put8(0x0F);
    put8(0x8E);
    put32(saut);
}

void mov_reg32_imm32(const int reg32, const unsigned long imm32)
{
    put8(unsigned char(0xB8 + reg32));
    put32(imm32);
}

void jmp_imm(const int saut)
{
    put8(0xE9);
    put32(saut);
}

void jmp_imm_short(const char saut)
{
    put8(0xEB);
    put8(saut);
}

void dec_reg32(const int reg32)
{
    put8(unsigned char(0x48 + reg32));
}

void inc_reg32(const int reg32)
{
    put8(unsigned char(0x40 + reg32));
}

void or_m32_imm32(void* _m32, const unsigned long imm32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0x81);
    put8(0x0D);
    put32((unsigned long)(m32));
    put32(imm32);
}

void or_m32_reg32(void* _m32, const unsigned long reg32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0x09);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void or_reg32_m32(const unsigned long reg32, void* _m32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0x0B);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void or_reg32_reg32(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x09);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void and_reg32_reg32(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x21);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void and_m32_imm32(void* _m32, const unsigned long imm32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x81);
    put8(0x25);
    put32((unsigned long)(m32));
    put32(imm32);
}

void and_reg32_m32(const unsigned long reg32, void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x23);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void xor_reg32_reg32(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x31);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void xor_reg32_m32(const unsigned long reg32, void* _m32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0x33);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void add_m32_imm32(void* _m32, const unsigned long imm32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x81);
    put8(0x05);
    put32((unsigned long)(m32));
    put32(imm32);
}

void add_m32_imm8(void* _m32, const unsigned char imm8)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x83);
    put8(0x05);
    put32((unsigned long)(m32));
    put8(imm8);
}

void sub_m32_imm32(void* _m32, const unsigned long imm32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x81);
    put8(0x2D);
    put32((unsigned long)(m32));
    put32(imm32);
}

void push_imm32(const unsigned long imm32)
{
    put8(0x68);
    put32(imm32);
}

void add_reg32_imm8(const unsigned long reg32, const unsigned char imm8)
{
    put8(0x83);
    put8(unsigned char(0xC0 + reg32));
    put8(imm8);
}

void add_reg32_imm32(const unsigned long reg32, const unsigned long imm32)
{
    put8(0x81);
    put8(unsigned char(0xC0 + reg32));
    put32(imm32);
}

void inc_m32(void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xFF);
    put8(0x05);
    put32((unsigned long)(m32));
}

void cmp_m32_imm32(void* _m32, const unsigned long imm32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x81);
    put8(0x3D);
    put32((unsigned long)(m32));
    put32(imm32);
}

void cmp_m32_imm8(void* _m32, const unsigned char imm8)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x83);
    put8(0x3D);
    put32((unsigned long)(m32));
    put8(imm8);
}

void cmp_m8_imm8(unsigned char* m8, const unsigned char imm8)
{
    put8(0x80);
    put8(0x3D);
    put32((unsigned long)(m8));
    put8(imm8);
}

void cmp_eax_imm32(const unsigned long imm32)
{
    put8(0x3D);
    put32(imm32);
}

void mov_m32_imm32(void* _m32, const unsigned long imm32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xC7);
    put8(0x05);
    put32((unsigned long)(m32));
    put32(imm32);
}

void jmp(const unsigned long mi_addr)
{
    put8(0xE9);
    put32(0);
    add_jump(code_length - 4, mi_addr);
}

void cdq()
{
    put8(0x99);
}

void cwde()
{
    put8(0x98);
}

void cbw()
{
    put8(0x66);
    put8(0x98);
}

void mov_m32_reg32(void* _m32, const unsigned long reg32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x89);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void ret()
{
    put8(0xC3);
}

void call_reg32(const unsigned long reg32)
{
    put8(0xFF);
    put8(unsigned char(0xD0 + reg32));
}

void call_m32(void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xFF);
    put8(0x15);
    put32((unsigned long)(m32));
}

void shr_reg32_imm8(const unsigned long reg32, const unsigned char imm8)
{
    put8(0xC1);
    put8(unsigned char(0xE8 + reg32));
    put8(imm8);
}

void shr_reg32_cl(const unsigned long reg32)
{
    put8(0xD3);
    put8(unsigned char(0xE8 + reg32));
}

void sar_reg32_cl(const unsigned long reg32)
{
    put8(0xD3);
    put8(unsigned char(0xF8 + reg32));
}

void shl_reg32_cl(const unsigned long reg32)
{
    put8(0xD3);
    put8(unsigned char(0xE0 + reg32));
}

void shld_reg32_reg32_cl(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x0F);
    put8(0xA5);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void shld_reg32_reg32_imm8(const unsigned long reg1, const unsigned long reg2, const unsigned char imm8)
{
    put8(0x0F);
    put8(0xA4);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
    put8(imm8);
}

void shrd_reg32_reg32_cl(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x0F);
    put8(0xAD);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void sar_reg32_imm8(const unsigned long reg32, const unsigned char imm8)
{
    put8(0xC1);
    put8(unsigned char(0xF8 + reg32));
    put8(imm8);
}

void shrd_reg32_reg32_imm8(const unsigned long reg1, const unsigned long reg2, const unsigned char imm8)
{
    put8(0x0F);
    put8(0xAC);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
    put8(imm8);
}

void mul_m32(void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xF7);
    put8(0x25);
    put32((unsigned long)(m32));
}

void imul_m32(void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xF7);
    put8(0x2D);
    put32((unsigned long)(m32));
}

void imul_reg32(const unsigned long reg32)
{
    put8(0xF7);
    put8(unsigned char(0xE8 + reg32));
}

void mul_reg32(const unsigned long reg32)
{
    put8(0xF7);
    put8(unsigned char(0xE0 + reg32));
}

void idiv_reg32(const unsigned long reg32)
{
    put8(0xF7);
    put8(unsigned char(0xF8 + reg32));
}

void div_reg32(const unsigned long reg32)
{
    put8(0xF7);
    put8(unsigned char(0xF0 + reg32));
}

void idiv_m32(void* _m32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0xF7);
    put8(0x3D);
    put32((unsigned long)(m32));
}

void div_m32(void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xF7);
    put8(0x35);
    put32((unsigned long)(m32));
}

void add_reg32_reg32(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x01);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void adc_reg32_reg32(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x11);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void add_reg32_m32(const unsigned long reg32, void* _m32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0x03);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void adc_reg32_m32(const unsigned long reg32, void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0x13);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void adc_reg32_imm32(const unsigned long reg32, const unsigned long imm32)
{
    put8(0x81);
    put8(unsigned char(0xD0 + reg32));
    put32(imm32);
}

void jmp_reg32(const unsigned long reg32)
{
    put8(0xFF);
    put8(unsigned char(0xE0 + reg32));
}

void jmp_m32(void* _m32)
{
	auto* m32 = (unsigned long*)_m32;
    put8(0xFF);
    put8(0x25);
    put32((unsigned long)(m32));
}

void mov_reg32_preg32(const unsigned long reg1, const unsigned long reg2)
{
    put8(0x8B);
    put8(unsigned char((reg1 << 3) | reg2));
}

void mov_preg32_reg32(const int reg1, const int reg2)
{
    put8(0x89);
    put8(unsigned char((reg2 << 3) | reg1));
}

void mov_reg32_preg32preg32pimm32(const int reg1, const int reg2, const int reg3, const unsigned long imm32)
{
    put8(0x8B);
    put8(unsigned char((reg1 << 3) | 0x84));
    put8(unsigned char(reg2 | (reg3 << 3)));
    put32(imm32);
}

void mov_reg32_preg32pimm32(const int reg1, const int reg2, const unsigned long imm32)
{
    put8(0x8B);
    put8(unsigned char(0x80 | (reg1 << 3) | reg2));
    put32(imm32);
}

void mov_reg32_preg32x4preg32(const int reg1, const int reg2, const int reg3)
{
    put8(0x8B);
    put8(unsigned char((reg1 << 3) | 4));
    put8(unsigned char(0x80 | (reg2 << 3) | reg3));
}

void mov_reg32_preg32x4preg32pimm32(const int reg1, const int reg2, const int reg3, const unsigned long imm32)
{
    put8(0x8B);
    put8(unsigned char((reg1 << 3) | 0x84));
    put8(unsigned char(0x80 | (reg2 << 3) | reg3));
    put32(imm32);
}

void mov_reg32_preg32x4pimm32(const int reg1, const int reg2, const unsigned long imm32)
{
    put8(0x8B);
    put8(unsigned char((reg1 << 3) | 4));
    put8(unsigned char(0x80 | (reg2 << 3) | 5));
    put32(imm32);
}

void mov_preg32preg32pimm32_reg8(const int reg1, const int reg2, const unsigned long imm32, const int reg8)
{
    put8(0x88);
    put8(unsigned char(0x84 | (reg8 << 3)));
    put8(unsigned char((reg2 << 3) | reg1));
    put32(imm32);
}

void mov_preg32pimm32_reg8(const int reg32, const unsigned long imm32, const int reg8)
{
    put8(0x88);
    put8(unsigned char(0x80 | reg32 | (reg8 << 3)));
    put32(imm32);
}

void mov_preg32pimm32_imm8(const int reg32, const unsigned long imm32, const unsigned char imm8)
{
    put8(0xC6);
    put8(unsigned char(0x80 + reg32));
    put32(imm32);
    put8(imm8);
}

void mov_preg32pimm32_reg16(const int reg32, const unsigned long imm32, const int reg16)
{
    put8(0x66);
    put8(0x89);
    put8(unsigned char(0x80 | reg32 | (reg16 << 3)));
    put32(imm32);
}

void mov_preg32pimm32_reg32(const int reg1, const unsigned long imm32, const int reg2)
{
    put8(0x89);
    put8(unsigned char(0x80 | reg1 | (reg2 << 3)));
    put32(imm32);
}

void add_eax_imm32(const unsigned long imm32)
{
    put8(0x05);
    put32(imm32);
}

void shl_reg32_imm8(const unsigned long reg32, const unsigned char imm8)
{
    put8(0xC1);
    put8(unsigned char(0xE0 + reg32));
    put8(imm8);
}

void mov_reg32_m32(const unsigned long reg32, void* _m32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0x8B);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m32));
}

void mov_reg8_m8(const int reg8, unsigned char* m8)
{
    put8(0x8A);
    put8(unsigned char((reg8 << 3) | 5));
    put32((unsigned long)(m8));
}

void and_eax_imm32(const unsigned long imm32)
{
    put8(0x25);
    put32(imm32);
}

void and_reg32_imm32(const int reg32, const unsigned long imm32)
{
    put8(0x81);
    put8(unsigned char(0xE0 + reg32));
    put32(imm32);
}

void or_reg32_imm32(const int reg32, const unsigned long imm32)
{
    put8(0x81);
    put8(unsigned char(0xC8 + reg32));
    put32(imm32);
}

void and_reg32_imm8(const int reg32, const unsigned char imm8)
{
    put8(0x83);
    put8(unsigned char(0xE0 + reg32));
    put8(imm8);
}

void and_ax_imm16(const unsigned short imm16)
{
    put8(0x66);
    put8(0x25);
    put16(imm16);
}

void and_al_imm8(const unsigned char imm8)
{
    put8(0x24);
    put8(imm8);
}

void or_ax_imm16(const unsigned short imm16)
{
    put8(0x66);
    put8(0x0D);
    put16(imm16);
}

void or_eax_imm32(const unsigned long imm32)
{
    put8(0x0D);
    put32(imm32);
}

void xor_ax_imm16(const unsigned short imm16)
{
    put8(0x66);
    put8(0x35);
    put16(imm16);
}

void xor_al_imm8(const unsigned char imm8)
{
    put8(0x34);
    put8(imm8);
}

void xor_reg32_imm32(const int reg32, const unsigned long imm32)
{
    put8(0x81);
    put8(unsigned char(0xF0 + reg32));
    put32(imm32);
}

void xor_reg8_imm8(const int reg8, const unsigned char imm8)
{
    put8(0x80);
    put8(unsigned char(0xF0 + reg8));
    put8(imm8);
}

void nop()
{
    put8(0x90);
}

void mov_reg32_reg32(const unsigned long reg1, const unsigned long reg2)
{
    if (reg1 == reg2) return;
    put8(0x89);
    put8(unsigned char(0xC0 | (reg2 << 3) | reg1));
}

void not_reg32(const unsigned long reg32)
{
    put8(0xF7);
    put8(unsigned char(0xD0 + reg32));
}

void movsx_reg32_m8(const int reg32, unsigned char* m8)
{
    put8(0x0F);
    put8(0xBE);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m8));
}

void movsx_reg32_reg8(const int reg32, const int reg8)
{
    put8(0x0F);
    put8(0xBE);
    put8(unsigned char((reg32 << 3) | reg8 | 0xC0));
}

void movsx_reg32_8preg32pimm32(const int reg1, const int reg2, const unsigned long imm32)
{
    put8(0x0F);
    put8(0xBE);
    put8(unsigned char((reg1 << 3) | reg2 | 0x80));
    put32(imm32);
}

void movsx_reg32_16preg32pimm32(const int reg1, const int reg2, const unsigned long imm32)
{
    put8(0x0F);
    put8(0xBF);
    put8(unsigned char((reg1 << 3) | reg2 | 0x80));
    put32(imm32);
}

void movsx_reg32_reg16(const int reg32, const int reg16)
{
    put8(0x0F);
    put8(0xBF);
    put8(unsigned char((reg32 << 3) | reg16 | 0xC0));
}

void movsx_reg32_m16(const int reg32, unsigned short* m16)
{
    put8(0x0F);
    put8(0xBF);
    put8(unsigned char((reg32 << 3) | 5));
    put32((unsigned long)(m16));
}

void fldcw_m16(unsigned short* m16)
{
    put8(0xD9);
    put8(0x2D);
    put32((unsigned long)(m16));
}

void fld_fpreg(const int fpreg)
{
    put8(0xD9);
    put8(unsigned char(0xC0 + fpreg));
}

void fld_preg32_dword(const int reg32)
{
    put8(0xD9);
    put8(unsigned char(reg32));
}

void fdiv_preg32_dword(const int reg32)
{
    put8(0xD8);
    put8(unsigned char(0x30 + reg32));
}

void fstp_fpreg(const int fpreg)
{
    put8(0xDD);
    put8(unsigned char(0xD8 + fpreg));
}

void fstp_preg32_dword(const int reg32)
{
    put8(0xD9);
    put8(unsigned char(0x18 + reg32));
}

void fldz()
{
    put8(0xD9);
    put8(0xEE);
}

void fchs()
{
    put8(0xD9);
    put8(0xE0);
}

void fstp_preg32_qword(const int reg32)
{
    put8(0xDD);
    put8(unsigned char(0x18 + reg32));
}

void fadd_preg32_dword(const int reg32)
{
    put8(0xD8);
    put8(unsigned char(reg32));
}

void fsub_preg32_dword(const int reg32)
{
    put8(0xD8);
    put8(unsigned char(0x20 + reg32));
}

void fmul_preg32_dword(const int reg32)
{
    put8(0xD8);
    put8(unsigned char(0x08 + reg32));
}

void fcomp_preg32_dword(const int reg32)
{
    put8(0xD8);
    put8(unsigned char(0x18 + reg32));
}

void fistp_preg32_dword(const int reg32)
{
    put8(0xDB);
    put8(unsigned char(0x18 + reg32));
}

void fistp_m32(void* _m32)
{
	auto m32 = (unsigned long*)_m32;
    put8(0xDB);
    put8(0x1D);
    put32((unsigned long)(m32));
}

void fistp_preg32_qword(const int reg32)
{
    put8(0xDF);
    put8(unsigned char(0x38 + reg32));
}

void fistp_m64(unsigned long long int* m64)
{
    put8(0xDF);
    put8(0x3D);
    put32((unsigned long)(m64));
}

void fld_preg32_qword(const int reg32)
{
    put8(0xDD);
    put8(unsigned char(reg32));
}

void fild_preg32_qword(const int reg32)
{
    put8(0xDF);
    put8(unsigned char(0x28 + reg32));
}

void fild_preg32_dword(const int reg32)
{
    put8(0xDB);
    put8(unsigned char(reg32));
}

void fadd_preg32_qword(const int reg32)
{
    put8(0xDC);
    put8(unsigned char(reg32));
}

void fdiv_preg32_qword(const int reg32)
{
    put8(0xDC);
    put8(unsigned char(0x30 + reg32));
}

void fsub_preg32_qword(const int reg32)
{
    put8(0xDC);
    put8(unsigned char(0x20 + reg32));
}

void fmul_preg32_qword(const int reg32)
{
    put8(0xDC);
    put8(unsigned char(0x08 + reg32));
}

void fsqrt()
{
    put8(0xD9);
    put8(0xFA);
}

void fabs_()
{
    put8(0xD9);
    put8(0xE1);
}

void fcomip_fpreg(const int fpreg)
{
    put8(0xDF);
    put8(unsigned char(0xF0 + fpreg));
}

void fucomi_fpreg(const int fpreg)
{
    put8(0xDB);
    put8(unsigned char(0xE8 + fpreg));
}

void fucomip_fpreg(const int fpreg)
{
    put8(0xDF);
    put8(unsigned char(0xE8 + fpreg));
}

void ffree_fpreg(const int fpreg)
{
    put8(0xDD);
    put8(unsigned char(0xC0 + fpreg));
}

void fclex()
{
    put8(0x9B);
    put8(0xDB);
    put8(0xE2);
}

void fstsw_ax()
{
    put8(0x9B);
    put8(0xDF);
    put8(0xE0);
}

void ud2()
{
    put8(0x0F);
    put8(0x0B);
}
