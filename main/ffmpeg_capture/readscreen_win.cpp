#include <Windows.h>
#include <commctrl.h>
#include "win/main_win.h"
#include "ffmpeg_capture.hpp"
#include "plugin.hpp"
#include "win/Config.hpp"

t_window_info g_s_info{};
BITMAPINFO g_bmp_info{}; //Needed for GetDIBits

static unsigned char* buffer = nullptr;
static unsigned int buffer_size = 0;

/// <summary>
/// Call this after finishing capture
/// </summary>
void ffmpeg_cleanup()
{
	DllCrtFree(buffer);
	buffer_size = 0;
}

void prepare_bitmap_header(const HWND h_main, const HBITMAP bitmap)
{
	const HDC hdc = GetDC(h_main);
	g_bmp_info.bmiHeader.biSize = sizeof(g_bmp_info.bmiHeader);
	GetDIBits(hdc, bitmap, 0, 0, nullptr, &g_bmp_info, DIB_RGB_COLORS);
	g_bmp_info.bmiHeader.biCompression = BI_RGB; //idk if needed
	g_bmp_info.bmiHeader.biBitCount = 24;
	g_bmp_info.bmiHeader.biHeight = -g_bmp_info.bmiHeader.biHeight;
	// to get upside down output
}

//based on original one for AVI
void ffmpeg_read_screen(void** dest, long* width, long* height)
{
	HDC all = nullptr; //all - screen; copy - buffer
	//RECT rect, rectS, rectT;
	POINT cli_tl{0, 0}; //mupen client x y

	*width = g_s_info.width;
	*height = g_s_info.height;

	const HDC mupendc = GetDC(main_hwnd); //only client area
	if (Config.is_capture_cropped_screen_dc)
	{
		//get whole screen dc and find out where is mupen's client area
		all = GetDC(nullptr);
		ClientToScreen(main_hwnd, &cli_tl);
	}

	// copy to a context in memory to speed up process
	const HDC copy = CreateCompatibleDC(mupendc);
	const HBITMAP bitmap = CreateCompatibleBitmap(mupendc, *width, *height);
	const auto oldbitmap = (HBITMAP)SelectObject(copy, bitmap);
	Sleep(0);
	if (copy)
	{
		if (Config.is_capture_cropped_screen_dc)
			BitBlt(copy, 0, 0, *width, *height, all, cli_tl.x,
			       cli_tl.y + g_s_info.toolbar_height + (g_s_info.height - *height),
			       SRCCOPY);
		else
			BitBlt(copy, 0, 0, *width, *height, mupendc, 0,
			       g_s_info.toolbar_height + (g_s_info.height - *height), SRCCOPY);
	}

	if (!copy || !bitmap)
	{
		MessageBox(nullptr, "Unexpected AVI error 1", "Error", MB_ICONERROR);
		*dest = nullptr;
		SelectObject(copy, oldbitmap);
		//apparently this leaks 1 pixel bitmap if not used
		if (bitmap)
			DeleteObject(bitmap);
		if (copy)
			DeleteDC(copy);
		if (mupendc)
			ReleaseDC(main_hwnd, mupendc);
		return;
	}

	// read the context
	if (!buffer || (long)buffer_size < *width * *height * 3 + 1)
	//if buffer doesn't exist yet or changed size somehow
	{
		if(buffer) free(buffer);
		buffer_size = *width * *height * 3 + 1;
		buffer = (unsigned char*)malloc(buffer_size);
	}

	if (!buffer) //failed to alloc
	{
		MessageBox(nullptr, "Failed to allocate memory for buffer", "Error",
		           MB_ICONERROR);
		*dest = nullptr;
		SelectObject(copy, oldbitmap);
		if (bitmap)
			DeleteObject(bitmap);
		if (copy)
			DeleteDC(copy);
		if (mupendc)
			ReleaseDC(main_hwnd, mupendc);
		return;
	}

	if (g_bmp_info.bmiHeader.biSize == 0)
		prepare_bitmap_header(main_hwnd, bitmap);
	if (auto res = GetDIBits(copy, bitmap, 0, *height, buffer, &g_bmp_info,
	                         DIB_RGB_COLORS) == 0)
	{
		printf("GetDIBits error\n");
	}

	*dest = buffer;
	SelectObject(copy, oldbitmap);
	if (bitmap)
		DeleteObject(bitmap);
	if (copy)
		DeleteDC(copy);
	if (mupendc)
		ReleaseDC(main_hwnd, mupendc);
}

void init_read_screen_ffmpeg(const t_window_info& info)
{
	printf((readScreen != nullptr)
		       ? const_cast<char*>("ReadScreen is implemented by this graphics plugin.\n")
		       : const_cast<char*>(
			       "ReadScreen not implemented by this graphics plugin (or was forcefully disabled in settings) - substituting...\n"));

	if (readScreen == nullptr)
	{
		readScreen = ffmpeg_read_screen;
		g_s_info = info;
	}
}
