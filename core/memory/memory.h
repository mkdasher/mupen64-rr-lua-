/**
 * Mupen64 - memory.h
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

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

int32_t init_memory();
constexpr uint32_t AddrMask = 0x7FFFFF;
#define read_word_in_memory() readmem[address>>16]()
#define read_byte_in_memory() readmemb[address>>16]()
#define read_hword_in_memory() readmemh[address>>16]()
#define read_dword_in_memory() readmemd[address>>16]()
#define write_word_in_memory() writemem[address>>16]()
#define write_byte_in_memory() writememb[address >>16]()
#define write_hword_in_memory() writememh[address >>16]()
#define write_dword_in_memory() writememd[address >>16]()
extern uint32_t SP_DMEM[0x1000 / 4 * 2];
extern unsigned char* SP_DMEMb;
extern uint32_t* SP_IMEM;
extern uint32_t PIF_RAM[0x40 / 4];
extern unsigned char* PIF_RAMb;
extern uint32_t rdram[0x800000 / 4];
extern uint8_t* rdramb;
extern uint8_t sram[0x8000];
extern uint8_t flashram[0x20000];
extern uint8_t eeprom[0x800];
extern uint8_t mempack[4][0x8000];
extern uint32_t address, word;
extern unsigned char g_byte;
extern uint16_t hword;
extern uint64_t dword, *rdword;

extern void (*readmem[0xFFFF])();
extern void (*readmemb[0xFFFF])();
extern void (*readmemh[0xFFFF])();
extern void (*readmemd[0xFFFF])();
extern void (*writemem[0xFFFF])();
extern void (*writememb[0xFFFF])();
extern void (*writememh[0xFFFF])();
extern void (*writememd[0xFFFF])();

typedef struct _RDRAM_register
{
    uint32_t rdram_config;
    uint32_t rdram_device_id;
    uint32_t rdram_delay;
    uint32_t rdram_mode;
    uint32_t rdram_ref_interval;
    uint32_t rdram_ref_row;
    uint32_t rdram_ras_interval;
    uint32_t rdram_min_interval;
    uint32_t rdram_addr_select;
    uint32_t rdram_device_manuf;
} RDRAM_register;

typedef struct _SP_register
{
    uint32_t sp_mem_addr_reg;
    uint32_t sp_dram_addr_reg;
    uint32_t sp_rd_len_reg;
    uint32_t sp_wr_len_reg;
    uint32_t w_sp_status_reg;
    uint32_t sp_status_reg;
    char halt;
    char broke;
    char dma_busy;
    char dma_full;
    char io_full;
    char single_step;
    char intr_break;
    char signal0;
    char signal1;
    char signal2;
    char signal3;
    char signal4;
    char signal5;
    char signal6;
    char signal7;
    uint32_t sp_dma_full_reg;
    uint32_t sp_dma_busy_reg;
    uint32_t sp_semaphore_reg;
} SP_register;

typedef struct _RSP_register
{
    uint32_t rsp_pc;
    uint32_t rsp_ibist;
} RSP_register;

typedef struct _DPC_register
{
    uint32_t dpc_start;
    uint32_t dpc_end;
    uint32_t dpc_current;
    uint32_t w_dpc_status;
    uint32_t dpc_status;
    char xbus_dmem_dma;
    char freeze;
    char flush;
    char start_glck;
    char tmem_busy;
    char pipe_busy;
    char cmd_busy;
    char cbuf_busy;
    char dma_busy;
    char end_valid;
    char start_valid;
    uint32_t dpc_clock;
    uint32_t dpc_bufbusy;
    uint32_t dpc_pipebusy;
    uint32_t dpc_tmem;
} DPC_register;

typedef struct _DPS_register
{
    uint32_t dps_tbist;
    uint32_t dps_test_mode;
    uint32_t dps_buftest_addr;
    uint32_t dps_buftest_data;
} DPS_register;

typedef struct _mips_register
{
    uint32_t w_mi_init_mode_reg;
    uint32_t mi_init_mode_reg;
    char init_length;
    char init_mode;
    char ebus_test_mode;
    char RDRAM_reg_mode;
    uint32_t mi_version_reg;
    uint32_t mi_intr_reg;
    uint32_t mi_intr_mask_reg;
    uint32_t w_mi_intr_mask_reg;
    char SP_intr_mask;
    char SI_intr_mask;
    char AI_intr_mask;
    char VI_intr_mask;
    char PI_intr_mask;
    char DP_intr_mask;
} mips_register;

typedef struct _VI_register
{
    uint32_t vi_status;
    uint32_t vi_origin;
    uint32_t vi_width;
    uint32_t vi_v_intr;
    uint32_t vi_current;
    uint32_t vi_burst;
    uint32_t vi_v_sync;
    uint32_t vi_h_sync;
    uint32_t vi_leap;
    uint32_t vi_h_start;
    uint32_t vi_v_start;
    uint32_t vi_v_burst;
    uint32_t vi_x_scale;
    uint32_t vi_y_scale;
    uint32_t vi_delay;
} VI_register;

typedef struct _AI_register
{
    uint32_t ai_dram_addr;
    //source address (in rdram) of sound sample to be played
    uint32_t ai_len; //amount of bytes(?) to be played
    uint32_t ai_control;
    uint32_t ai_status; //info about whether dma active and is fifo full
    uint32_t ai_dacrate;
    //clock rate / audio rate, tells sound controller how to interpret the audio samples
    uint32_t ai_bitrate;
    //possible values 2 to 16, bits per sample?, this is always (dacRate / 66)-1 (by libultra)
    uint32_t next_delay;
    uint32_t next_len;
    uint32_t current_delay;
    uint32_t current_len;
} AI_register;

typedef struct _PI_register
{
    uint32_t pi_dram_addr_reg;
    uint32_t pi_cart_addr_reg;
    uint32_t pi_rd_len_reg;
    uint32_t pi_wr_len_reg;
    uint32_t read_pi_status_reg;
    uint32_t pi_bsd_dom1_lat_reg;
    uint32_t pi_bsd_dom1_pwd_reg;
    uint32_t pi_bsd_dom1_pgs_reg;
    uint32_t pi_bsd_dom1_rls_reg;
    uint32_t pi_bsd_dom2_lat_reg;
    uint32_t pi_bsd_dom2_pwd_reg;
    uint32_t pi_bsd_dom2_pgs_reg;
    uint32_t pi_bsd_dom2_rls_reg;
} PI_register;

typedef struct _RI_register
{
    uint32_t ri_mode;
    uint32_t ri_config;
    uint32_t ri_current_load;
    uint32_t ri_select;
    uint32_t ri_refresh;
    uint32_t ri_latency;
    uint32_t ri_error;
    uint32_t ri_werror;
} RI_register;

typedef struct _SI_register
{
    uint32_t si_dram_addr;
    uint32_t si_pif_addr_rd64b;
    uint32_t si_pif_addr_wr64b;
    uint32_t si_status;
} SI_register;

extern RDRAM_register rdram_register;
extern PI_register pi_register;
extern mips_register MI_register;
extern SP_register sp_register;
extern SI_register si_register;
extern VI_register vi_register;
extern RSP_register rsp_register;
extern RI_register ri_register;
extern AI_register ai_register;
extern DPC_register dpc_register;
extern DPS_register dps_register;

#ifndef _BIG_ENDIAN
#define sl(mot) \
( \
((mot & 0x000000FF) << 24) | \
((mot & 0x0000FF00) <<  8) | \
((mot & 0x00FF0000) >>  8) | \
((mot & 0xFF000000) >> 24) \
)

#define S8 3
#define S16 2
#define Sh16 1

#else

#define sl(mot) mot
#define S8 0
#define S16 0
#define Sh16 0

#endif

void read_nothing();
void read_nothingh();
void read_nothingb();
void read_nothingd();
void read_nomem();
void read_nomemb();
void read_nomemh();
void read_nomemd();
void read_rdram();
void read_rdramb();
void read_rdramh();
void read_rdramd();
void read_rdramFB();
void read_rdramFBb();
void read_rdramFBh();
void read_rdramFBd();
void read_rdramreg();
void read_rdramregb();
void read_rdramregh();
void read_rdramregd();
void read_rsp_mem();
void read_rsp_memb();
void read_rsp_memh();
void read_rsp_memd();
void read_rsp_reg();
void read_rsp_regb();
void read_rsp_regh();
void read_rsp_regd();
void read_rsp();
void read_rspb();
void read_rsph();
void read_rspd();
void read_dp();
void read_dpb();
void read_dph();
void read_dpd();
void read_dps();
void read_dpsb();
void read_dpsh();
void read_dpsd();
void read_mi();
void read_mib();
void read_mih();
void read_mid();
void read_vi();
void read_vib();
void read_vih();
void read_vid();
void read_ai();
void read_aib();
void read_aih();
void read_aid();
void read_pi();
void read_pib();
void read_pih();
void read_pid();
void read_ri();
void read_rib();
void read_rih();
void read_rid();
void read_si();
void read_sib();
void read_sih();
void read_sid();
void read_flashram_status();
void read_flashram_statusb();
void read_flashram_statush();
void read_flashram_statusd();
void read_rom();
void read_romb();
void read_romh();
void read_romd();
void read_pif();
void read_pifb();
void read_pifh();
void read_pifd();
void read_sc_reg();
void read_sc_regb();
void read_sc_regh();
void read_sc_regd();

void write_nothing();
void write_nothingb();
void write_nothingh();
void write_nothingd();
void write_nomem();
void write_nomemb();
void write_nomemd();
void write_nomemh();
void write_rdram();
void write_rdramb();
void write_rdramh();
void write_rdramd();
void write_rdramFB();
void write_rdramFBb();
void write_rdramFBh();
void write_rdramFBd();
void write_rdramreg();
void write_rdramregb();
void write_rdramregh();
void write_rdramregd();
void write_rsp_mem();
void write_rsp_memb();
void write_rsp_memh();
void write_rsp_memd();
void write_rsp_reg();
void write_rsp_regb();
void write_rsp_regh();
void write_rsp_regd();
void write_rsp();
void write_rspb();
void write_rsph();
void write_rspd();
void write_dp();
void write_dpb();
void write_dph();
void write_dpd();
void write_dps();
void write_dpsb();
void write_dpsh();
void write_dpsd();
void write_mi();
void write_mib();
void write_mih();
void write_mid();
void write_vi();
void write_vib();
void write_vih();
void write_vid();
void write_ai();
void write_aib();
void write_aih();
void write_aid();
void write_pi();
void write_pib();
void write_pih();
void write_pid();
void write_ri();
void write_rib();
void write_rih();
void write_rid();
void write_si();
void write_sib();
void write_sih();
void write_sid();
void write_flashram_dummy();
void write_flashram_dummyb();
void write_flashram_dummyh();
void write_flashram_dummyd();
void write_flashram_command();
void write_flashram_commandb();
void write_flashram_commandh();
void write_flashram_commandd();
void write_rom();
void write_pif();
void write_pifb();
void write_pifh();
void write_pifd();
void write_sc_reg();
void write_sc_regb();
void write_sc_regh();
void write_sc_regd();

void update_SP();
void update_DPC();

template <typename T>
uint32_t ToAddr(uint32_t addr)
{
    return sizeof(T) == 4
               ? addr
               : sizeof(T) == 2
               ? addr ^ S16
               : sizeof(T) == 1
               ? addr ^ S8
               : throw"ToAddr: sizeof(T)";
}

/**
 * \brief Gets the value at the specified address from RDRAM
 * \tparam T The value's type
 * \param addr The start address of the value
 * \return The value at the address
 */
template <typename T>
extern T LoadRDRAMSafe(uint32_t addr)
{
    return *((T*)(rdramb + ((ToAddr<T>(addr) & AddrMask))));
}

/**
 * \brief Sets the value at the specified address in RDRAM
 * \tparam T The value's type
 * \param addr The start address of the value
 * \param value The value to set
 */
template <typename T>
extern void StoreRDRAMSafe(uint32_t addr, T value)
{
    *((T*)(rdramb + ((ToAddr<T>(addr) & AddrMask)))) = value;
}

#endif
