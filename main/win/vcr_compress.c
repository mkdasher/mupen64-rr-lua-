/**
 * Mupen64 - vcr_compress.c
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

#include <Windows.h>
#include <commctrl.h>
#include <cstdio>
#include "../win/main_win.h"
#include "../vcr_compress.h"
#include "Config.hpp"
#include "../plugin.hpp"
#include "../../winproject/resource.h"
#include "vfw.h"
#include "features/Toolbar.hpp"
#include "features/Statusbar.hpp"


static int avi_opened = 0;
static int frame;
static BITMAPINFOHEADER infoHeader;
static PAVIFILE avi_file;
static AVISTREAMINFO video_stream_header;
static PAVISTREAM video_stream;
static PAVISTREAM compressed_video_stream;
static AVICOMPRESSOPTIONS video_options;
static AVICOMPRESSOPTIONS* pvideo_options[1];

static int sample;
static unsigned int avi_file_size;
static WAVEFORMATEX sound_format;
static AVISTREAMINFO sound_stream_header;
static PAVISTREAM sound_stream;

t_window_info vcrcomp_window_info = {0};

void get_window_info(const HWND hwnd, t_window_info& info)
{
    RECT client_rect = {0}, statusbar_rect = {0}, toolbar_rect = {0};

    GetClientRect(hwnd, &client_rect);

    // full client dimensions (including statusbar and toolbar)
    info.width = client_rect.right - client_rect.left;
    info.height = client_rect.bottom - client_rect.top;

    if (toolbar_hwnd)
        GetClientRect(toolbar_hwnd, &toolbar_rect);
    info.toolbar_height = toolbar_rect.bottom - toolbar_rect.top;
    
    if (statusbar_hwnd)
        GetClientRect(statusbar_hwnd, &statusbar_rect);
    info.statusbar_height = statusbar_rect.bottom - statusbar_rect.top;

    //subtract size of toolbar and statusbar from buffer dimensions
    //if video plugin knows about this, whole game screen should be captured. Most of the plugins do.
    info.height -= info.toolbar_height + info.statusbar_height;
}

// "internal" readScreen, used when plugin doesn't implement it
void __cdecl vcrcomp_internal_read_screen(void** dest, long* width, long* height)
{
    HDC all = nullptr; //all - screen; copy - buffer
    POINT cli_tl{0, 0}; //mupen client x y 

    if (Config.capture_delay)
    {
        Sleep(Config.capture_delay);
    }

    const HDC mupendc = GetDC(main_hwnd); //only client area
    if (Config.is_capture_cropped_screen_dc)
    {
        //get whole screen dc and find out where is mupen's client area
        all = GetDC(nullptr);
        ClientToScreen(main_hwnd, &cli_tl);
    }

    //real width and height of emulated area must be a multiple of 4, which is apparently important for avi
    *width = vcrcomp_window_info.width & ~3;
    *height = vcrcomp_window_info.height & ~3;

    // copy to a context in memory to speed up process
    const HDC copy = CreateCompatibleDC(mupendc);
    const HBITMAP bitmap = CreateCompatibleBitmap(mupendc, *width, *height);
    const auto oldbitmap = (HBITMAP)SelectObject(copy, bitmap);
    
    if (copy)
    {
        if (Config.is_capture_cropped_screen_dc)
            BitBlt(copy, 0, 0, *width, *height, all, cli_tl.x,
                   cli_tl.y + vcrcomp_window_info.toolbar_height + (vcrcomp_window_info.height - *height), SRCCOPY);
        else
            BitBlt(copy, 0, 0, *width, *height, mupendc, 0, vcrcomp_window_info.toolbar_height + (vcrcomp_window_info.height - *height), SRCCOPY);
    }

    if ((!avi_opened || !copy || !bitmap))
    {
        if (!*dest) //don't show warning, this is initialisation phase
            MessageBox(nullptr, "Unexpected AVI error 1", "Error", MB_ICONERROR);
        *dest = nullptr;
        SelectObject(copy, oldbitmap); //apparently this leaks 1 pixel bitmap if not used
        if (bitmap)
            DeleteObject(bitmap);
        if (copy)
            DeleteDC(copy);
        if (mupendc)
            ReleaseDC(main_hwnd, mupendc);
        if (all)
            ReleaseDC(nullptr, all);
        return;
    }

    // read the context
    static unsigned char* buffer = nullptr;
    static unsigned int buffer_size = 0;
    if (!buffer || (long)buffer_size < *width * *height * 3 + 1) //if buffer doesn't exist yet or changed size somehow
    {
        free(buffer);
        buffer_size = *width * *height * 3 + 1;
        buffer = (unsigned char*)malloc(buffer_size);
    }

    if (!buffer) //failed to alloc
    {
        MessageBox(nullptr, "Failed to allocate memory for buffer", "Error", MB_ICONERROR);
        *dest = nullptr;
        SelectObject(copy, oldbitmap);
        if (bitmap)
            DeleteObject(bitmap);
        if (copy)
            DeleteDC(copy);
        if (mupendc)
            ReleaseDC(main_hwnd, mupendc);
        if (all)
            ReleaseDC(nullptr, all);
        return;
    }

    BITMAPINFO bmpinfos{};
    memcpy(&bmpinfos.bmiHeader, &infoHeader, sizeof(BITMAPINFOHEADER)); //copy info from avi file init
    GetDIBits(copy, bitmap, 0, *height, buffer, &bmpinfos, DIB_RGB_COLORS);

    *dest = buffer;
    SelectObject(copy, oldbitmap);
    if (bitmap)
        DeleteObject(bitmap);
    if (copy)
        DeleteDC(copy);
    if (mupendc)
        ReleaseDC(main_hwnd, mupendc);
    if (all)
        ReleaseDC(nullptr, all);
}

void vcr_comp_init()
{
}


BOOL vcr_comp_add_video_frame(unsigned char* data)
{
    extern int m_task;
    //if (m_task == 4 || m_task == 5) return 1; //don't record frames that are during emu loading (black frames)
    long int temp_len;
    const BOOL ret = AVIStreamWrite(compressed_video_stream, frame++, 1, data, (long)infoHeader.biSizeImage, AVIIF_KEYFRAME, nullptr,
                                    &temp_len);
    avi_file_size += temp_len;
    return (0 == ret);
}

BOOL vcr_comp_add_audio_data(unsigned char* data, const int len)
{
	const BOOL ok = (0 == AVIStreamWrite(sound_stream, sample, len / sound_format.nBlockAlign, data, len, 0, nullptr, nullptr));
    sample += len / sound_format.nBlockAlign;
    avi_file_size += len;
    return ok;
}

bool vr_comp_load_options()
{
    SetCurrentDirectory(app_path.c_str());
    FILE* f = fopen("avi.cfg", "rb");
    if (f == nullptr) return false; //file doesn't exist

    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0) goto error; //empty file
    fseek(f, 0, SEEK_SET);

    pvideo_options[0] = (AVICOMPRESSOPTIONS*)malloc(sizeof(AVICOMPRESSOPTIONS));
    fread(pvideo_options[0], sizeof(AVICOMPRESSOPTIONS), 1, f);

    {
        void* more_options = malloc(pvideo_options[0]->cbParms);
        fread(more_options, pvideo_options[0]->cbParms, 1, f);
        pvideo_options[0]->lpParms = more_options;
    }
    fclose(f);
    return true;
error:
    fclose(f);
    return false;
}

void vr_comp_save_options()
{
    SetCurrentDirectory(app_path.c_str());
    FILE* f = fopen("avi.cfg", "w");
    fwrite(pvideo_options[0], sizeof(AVICOMPRESSOPTIONS), 1, f);
    fwrite(pvideo_options[0]->lpParms, pvideo_options[0]->cbParms, 1, f);
    fflush(f);
    fclose(f);
}

void vcr_comp_start_file(const char* filename, const long width, const long height, const int fps, int New)
{
    avi_opened = 1;
    avi_file_size = 0;
    frame = 0;
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = width * height * 3;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0;

    AVIFileInit();
    AVIFileOpen(&avi_file, filename, OF_WRITE | OF_CREATE, nullptr);

    ZeroMemory(&video_stream_header, sizeof(AVISTREAMINFO));
    video_stream_header.fccType = streamtypeVIDEO;
    video_stream_header.dwScale = 1;
    video_stream_header.dwRate = fps;
    video_stream_header.dwSuggestedBufferSize = 0;
    AVIFileCreateStream(avi_file, &video_stream, &video_stream_header);

    if (!New && pvideo_options[0] == nullptr) //if hide dialog and options not found
    {
        New = !vr_comp_load_options(); //if failed to load defaults, ask with dialog
    }
    if (New)
    {
        ZeroMemory(&video_options, sizeof(AVICOMPRESSOPTIONS));
        pvideo_options[0] = &video_options;
        if(!AVISaveOptions(main_hwnd, 0, 1, &video_stream, pvideo_options))
        {
            // TODO: user cancel, roll everything back
        }
    }
    vr_comp_save_options();
    AVIMakeCompressedStream(&compressed_video_stream, video_stream, pvideo_options[0], nullptr);
    AVIStreamSetFormat(compressed_video_stream, 0, &infoHeader,
                       (LONG)(infoHeader.biSize + infoHeader.biClrUsed * sizeof(RGBQUAD)));

    // sound
    sample = 0;
    sound_format.wFormatTag = WAVE_FORMAT_PCM;
    sound_format.nChannels = 2;
    sound_format.nSamplesPerSec = 44100;
    sound_format.nAvgBytesPerSec = 44100 * (2 * 16 / 8);
    sound_format.nBlockAlign = 2 * 16 / 8;
    sound_format.wBitsPerSample = 16;
    sound_format.cbSize = 0;

    ZeroMemory(&sound_stream_header, sizeof(AVISTREAMINFO));
    sound_stream_header.fccType = streamtypeAUDIO;
    sound_stream_header.dwQuality = (DWORD)-1;
    sound_stream_header.dwScale = sound_format.nBlockAlign;
    sound_stream_header.dwInitialFrames = 1;
    sound_stream_header.dwRate = sound_format.nAvgBytesPerSec;
    sound_stream_header.dwSampleSize = sound_format.nBlockAlign;
    AVIFileCreateStream(avi_file, &sound_stream, &sound_stream_header);
    AVIStreamSetFormat(sound_stream, 0, &sound_format, sizeof(WAVEFORMATEX));

    /*ZeroMemory(&sound_options, sizeof(AVICOMPRESSOPTIONS));
    psound_options[0] = &sound_options;
    AVISaveOptions(main_hwnd, 0, 1, &sound_stream, psound_options);
    AVIMakeCompressedStream(&compressed_sound_stream, sound_stream, &sound_options, NULL);
    AVIStreamSetFormat(compressed_sound_stream, 0, &sound_format, sizeof(WAVEFORMATEX));*/
}

void vcr_comp_finish_file(const int split)
{
    //SetWindowPos(main_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // why is this being set when recording is stopped...
    AVIStreamClose(compressed_video_stream)
    AVIStreamRelease(video_stream);
    // AVIStreamClose(compressed_video_stream);
    AVIStreamClose(sound_stream)
    AVIFileClose(avi_file);
    AVIFileExit();
    if (!split)
    {
	    const HMENU h_menu = GetMenu(main_hwnd);
        EnableMenuItem(h_menu, ID_END_CAPTURE, MF_GRAYED);
        EnableMenuItem(h_menu, ID_START_CAPTURE, MF_ENABLED);
        EnableMenuItem(h_menu, ID_START_CAPTURE_PRESET, MF_ENABLED);
        EnableMenuItem(h_menu, FULL_SCREEN, MF_ENABLED); //Enables fullscreen menu
        SetWindowPos(main_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); //Remove the always on top flag
    }

    avi_opened = 0;
    printf("[VCR]: Finished AVI capture.\n");
}

unsigned int vcr_comp_get_size()
{
    return avi_file_size;
}
