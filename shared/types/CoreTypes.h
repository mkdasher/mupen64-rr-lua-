// ReSharper disable CppInconsistentNaming
#pragma once

typedef struct
{
	unsigned short Version;
	unsigned short Type;
	char Name[100];

	/* If DLL supports memory these memory options then set them to TRUE or FALSE
	   if it does not support it */
	int NormalMemory; /* a normal unsigned char array */
	int MemoryBswaped; /* a normal unsigned char array where the memory has been pre
							 bswap on a dword (32 bits) boundry */
} PLUGIN_INFO;

typedef struct
{
	void* hInst;
	// Whether the memory has been pre-byteswapped on a unsigned long boundary
	int MemoryBswaped;
	unsigned char* RDRAM;
	unsigned char* DMEM;
	unsigned char* IMEM;

	unsigned long* MI_INTR_REG;

	unsigned long* SP_MEM_ADDR_REG;
	unsigned long* SP_DRAM_ADDR_REG;
	unsigned long* SP_RD_LEN_REG;
	unsigned long* SP_WR_LEN_REG;
	unsigned long* SP_STATUS_REG;
	unsigned long* SP_DMA_FULL_REG;
	unsigned long* SP_DMA_BUSY_REG;
	unsigned long* SP_PC_REG;
	unsigned long* SP_SEMAPHORE_REG;

	unsigned long* DPC_START_REG;
	unsigned long* DPC_END_REG;
	unsigned long* DPC_CURRENT_REG;
	unsigned long* DPC_STATUS_REG;
	unsigned long* DPC_CLOCK_REG;
	unsigned long* DPC_BUFBUSY_REG;
	unsigned long* DPC_PIPEBUSY_REG;
	unsigned long* DPC_TMEM_REG;

	void (__cdecl*CheckInterrupts)(void);
	void (__cdecl*ProcessDlistList)(void);
	void (__cdecl*ProcessAlistList)(void);
	void (__cdecl*ProcessRdpList)(void);
	void (__cdecl*ShowCFB)(void);
} RSP_INFO;

typedef struct
{
	void* hWnd; /* Render window */
	void* hStatusBar;
	/* if render window does not have a status bar then this is NULL */

	int MemoryBswaped; // If this is set to TRUE, then the memory has been pre
	//   bswap on a dword (32 bits) boundry
	//	eg. the first 8 bytes are stored like this:
	//        4 3 2 1   8 7 6 5

	unsigned char* HEADER; // This is the rom header (first 40h bytes of the rom
	// This will be in the same memory format as the rest of the memory.
	unsigned char* RDRAM;
	unsigned char* DMEM;
	unsigned char* IMEM;

	unsigned long* MI_INTR_REG;

	unsigned long* DPC_START_REG;
	unsigned long* DPC_END_REG;
	unsigned long* DPC_CURRENT_REG;
	unsigned long* DPC_STATUS_REG;
	unsigned long* DPC_CLOCK_REG;
	unsigned long* DPC_BUFBUSY_REG;
	unsigned long* DPC_PIPEBUSY_REG;
	unsigned long* DPC_TMEM_REG;

	unsigned long* VI_STATUS_REG;
	unsigned long* VI_ORIGIN_REG;
	unsigned long* VI_WIDTH_REG;
	unsigned long* VI_INTR_REG;
	unsigned long* VI_V_CURRENT_LINE_REG;
	unsigned long* VI_TIMING_REG;
	unsigned long* VI_V_SYNC_REG;
	unsigned long* VI_H_SYNC_REG;
	unsigned long* VI_LEAP_REG;
	unsigned long* VI_H_START_REG;
	unsigned long* VI_V_START_REG;
	unsigned long* VI_V_BURST_REG;
	unsigned long* VI_X_SCALE_REG;
	unsigned long* VI_Y_SCALE_REG;

	void (__cdecl*CheckInterrupts)(void);
} GFX_INFO;

typedef struct
{
	void* hwnd;
	void* hinst;

	int MemoryBswaped; // If this is set to TRUE, then the memory has been pre
	//   bswap on a dword (32 bits) boundry
	//	eg. the first 8 bytes are stored like this:
	//        4 3 2 1   8 7 6 5
	unsigned char* HEADER; // This is the rom header (first 40h bytes of the rom
	// This will be in the same memory format as the rest of the memory.
	unsigned char* RDRAM;
	unsigned char* DMEM;
	unsigned char* IMEM;

	unsigned long* MI_INTR_REG;

	unsigned long* AI_DRAM_ADDR_REG;
	unsigned long* AI_LEN_REG;
	unsigned long* AI_CONTROL_REG;
	unsigned long* AI_STATUS_REG;
	unsigned long* AI_DACRATE_REG;
	unsigned long* AI_BITRATE_REG;

	void (__cdecl*CheckInterrupts)(void);
} AUDIO_INFO;

typedef struct
{
	int Present;
	int RawData;
	int Plugin;
} CONTROL;

typedef union
{
	unsigned long Value;

	struct
	{
		unsigned R_DPAD : 1;
		unsigned L_DPAD : 1;
		unsigned D_DPAD : 1;
		unsigned U_DPAD : 1;
		unsigned START_BUTTON : 1;
		unsigned Z_TRIG : 1;
		unsigned B_BUTTON : 1;
		unsigned A_BUTTON : 1;

		unsigned R_CBUTTON : 1;
		unsigned L_CBUTTON : 1;
		unsigned D_CBUTTON : 1;
		unsigned U_CBUTTON : 1;
		unsigned R_TRIG : 1;
		unsigned L_TRIG : 1;
		unsigned Reserved1 : 1;
		unsigned Reserved2 : 1;

		signed Y_AXIS : 8;

		signed X_AXIS : 8;
	};
} BUTTONS;

typedef struct
{
	void* hMainWindow;
	void* hinst;

	int MemoryBswaped; // If this is set to TRUE, then the memory has been pre
	//   bswap on a dword (32 bits) boundry, only effects header.
	//	eg. the first 8 bytes are stored like this:
	//        4 3 2 1   8 7 6 5
	unsigned char* HEADER; // This is the rom header (first 40h bytes of the rom)
	CONTROL* Controls; // A pointer to an array of 4 controllers .. eg:
	// CONTROL Controls[4];
} CONTROL_INFO;

typedef struct s_rom_header
{
	unsigned char init_PI_BSB_DOM1_LAT_REG;
	unsigned char init_PI_BSB_DOM1_PGS_REG;
	unsigned char init_PI_BSB_DOM1_PWD_REG;
	unsigned char init_PI_BSB_DOM1_PGS_REG2;
	unsigned long ClockRate;
	unsigned long PC;
	unsigned long Release;
	unsigned long CRC1;
	unsigned long CRC2;
	unsigned long Unknown[2];
	unsigned char nom[20];
	unsigned long unknown;
	unsigned long Manufacturer_ID;
	unsigned short Cartridge_ID;
	unsigned short Country_code;
	unsigned long Boot_Code[1008];
} t_rom_header;
