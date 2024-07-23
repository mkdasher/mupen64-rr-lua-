#include "MGECompositor.h"
#include <Windows.h>

#include <shared/Messenger.h>
#include <core/r4300/Plugin.hpp>
#include <view/gui/Main.h>
#include <shared/services/FrontendService.h>
#include <cassert>
namespace MGECompositor
{
	struct VideoBuffer
	{
		long last_width = 0;
		long last_height = 0;
		long width = 0;
		long height = 0;
		void* buffer = nullptr;
		BITMAPINFO bmp_info{};
		// Optional DIB for perf
		HBITMAP dib = nullptr;
		// Optional DC backing the dib
		HDC dc = nullptr;
	};

	const auto class_name = "game_control";


	HWND control_hwnd;

	VideoBuffer internal_buffer{};
	VideoBuffer external_buffer{};

	LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
		case WM_PAINT:
			{
				auto vbuf = (VideoBuffer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
				if (!vbuf)
				{
					break;
				}
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);

				RECT rect{};
				GetClientRect(hwnd, &rect);
				if (vbuf->dib)
				{
					BitBlt(hdc, 0, 0, vbuf->bmp_info.bmiHeader.biWidth,
						vbuf->bmp_info.bmiHeader.biHeight, vbuf->dc, 0, 0, SRCCOPY);
				} else
				{
					StretchDIBits(hdc,
							  rect.top,
							  rect.left,
							  rect.right,
							  rect.bottom,
							  0, 0,
							  vbuf->bmp_info.bmiHeader.biWidth,
							  vbuf->bmp_info.bmiHeader.biHeight,
							  vbuf->buffer,
							  &(vbuf->bmp_info),
							  DIB_RGB_COLORS,
							  SRCCOPY);
				}


				EndPaint(hwnd, &ps);
				return 0;
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	void create(HWND hwnd)
	{
		control_hwnd = CreateWindow(class_name, "", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, hwnd, nullptr, g_app_instance, nullptr);
	}

	void init()
	{
		WNDCLASS wndclass = {0};
		wndclass.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = (WNDPROC)wndproc;
		wndclass.hInstance = g_app_instance;
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.lpszClassName = class_name;
		RegisterClass(&wndclass);

		internal_buffer.bmp_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		internal_buffer.bmp_info.bmiHeader.biPlanes = 1;
		internal_buffer.bmp_info.bmiHeader.biBitCount = 24;
		internal_buffer.bmp_info.bmiHeader.biCompression = BI_RGB;
		external_buffer.bmp_info = internal_buffer.bmp_info;

		Messenger::subscribe(Messenger::Message::EmuLaunchedChanged, [](std::any data)
		{
			auto value = std::any_cast<bool>(data);
			ShowWindow(control_hwnd, (value && get_video_size && read_video) ? SW_SHOW : SW_HIDE);
		});
	}

	bool available()
	{
		return get_video_size && read_video;
	}

	void update_screen()
	{
		get_video_size(&internal_buffer.width, &internal_buffer.height);

		if (internal_buffer.width != internal_buffer.last_width || internal_buffer.height != internal_buffer.last_height)
		{
			SetWindowLongPtr(control_hwnd, GWLP_USERDATA, (LONG_PTR)&internal_buffer);
			printf("MGE Compositor: Video size %dx%d\n", internal_buffer.width, internal_buffer.height);

			internal_buffer.bmp_info.bmiHeader.biWidth = internal_buffer.width;
			internal_buffer.bmp_info.bmiHeader.biHeight = internal_buffer.height;

			if (internal_buffer.dib) {
				SelectObject(internal_buffer.dc, nullptr);
				DeleteObject(internal_buffer.dc);
				DeleteObject(internal_buffer.dib);
			}

			auto dc = GetDC(control_hwnd);
			internal_buffer.dc = CreateCompatibleDC(dc);
			internal_buffer.dib = CreateDIBSection(internal_buffer.dc, &internal_buffer.bmp_info, DIB_RGB_COLORS, &internal_buffer.buffer, nullptr, 0);
			SelectObject(internal_buffer.dc, internal_buffer.dib);

			ReleaseDC(control_hwnd, dc);

			MoveWindow(control_hwnd, 0, 0, internal_buffer.width, internal_buffer.height, true);
		}

		read_video(&internal_buffer.buffer);

		internal_buffer.last_width = internal_buffer.width;
		internal_buffer.last_height = internal_buffer.height;

		RedrawWindow(control_hwnd, NULL, NULL, RDW_INVALIDATE);
	}

	void read_screen(void** dest, long* width, long* height)
	{
		if (dest)
		{
			*dest = internal_buffer.buffer;
		}
		if (width)
		{
			*width = internal_buffer.width;
		}
		if (height)
		{
			*height = internal_buffer.height;
		}
	}

	void load_screen(void* data, long width, long height)
	{
		SetWindowLongPtr(control_hwnd, GWLP_USERDATA, (LONG_PTR)&external_buffer);

		external_buffer.bmp_info.bmiHeader.biWidth = external_buffer.width = width;
		external_buffer.bmp_info.bmiHeader.biHeight = external_buffer.height = height;

		free(external_buffer.buffer);
		external_buffer.buffer = malloc(external_buffer.width * external_buffer.height * 3);
		memcpy(external_buffer.buffer, data, external_buffer.width * external_buffer.height * 3);

		MoveWindow(control_hwnd, 0, 0, width, height, true);
		RedrawWindow(control_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		SetWindowLongPtr(control_hwnd, GWLP_USERDATA, (LONG_PTR)&internal_buffer);
	}
}
