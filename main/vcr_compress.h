#pragma once
#include <Windows.h>

typedef struct
{
	long width;
	long height;
	long toolbar_height;
	long statusbar_height;
} t_window_info;

extern t_window_info vcrcomp_window_info;

void vcr_comp_init();

void vcr_comp_start_file(const char* filename, long width, long height, int fps,
                       int New);
void vcr_comp_finish_file(int split);
BOOL vcr_comp_add_video_frame(unsigned char* data);
BOOL vcr_comp_add_audio_data(unsigned char* data, int len);

unsigned int vcr_comp_get_size();

void get_window_info(HWND hwnd, t_window_info& info);

/**
 * \brief Writes the emulator's current emulation front buffer into the destination buffer
 * \param dest The buffer holding video data of size <c>width * height</c>
 * \param width The buffer's width
 * \param height The buffer's height
 */
void __cdecl vcrcomp_internal_read_screen(void** dest, long* width, long* height);
