﻿#include "LuaCallbacks.h"
#include "LuaConsole.h"
#include "../main/win/main_win.h"

namespace LuaCallbacks
{
	t_window_procedure_params window_proc_params = {0};
	int current_input_n = 0;

	//generic function used for all of the At... callbacks, calls function from stack top.
	int CallTop(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int state_update_screen(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int AtVI(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}


	int AtInput(lua_State* L)
	{
		lua_pushinteger(L, current_input_n);
		return lua_pcall(L, 1, 0, 0);
	}

	int state_stop(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int AtWindowMessage(lua_State* L)
	{
		lua_pushinteger(L, (unsigned)window_proc_params.wnd);
		lua_pushinteger(L, window_proc_params.msg);
		lua_pushinteger(L, window_proc_params.w_param);
		lua_pushinteger(L, window_proc_params.l_param);

		return lua_pcall(L, 4, 0, 0);
	}

	void call_window_message(HWND wnd, UINT msg, WPARAM w, LPARAM l)
	{
		// Invoking dispatcher here isn't allowed, as it would lead to infinite recursion
		window_proc_params = {
			.wnd = wnd,
			.msg = msg,
			.w_param = w,
			.l_param = l
		};
		invoke_callbacks_with_key_on_all_instances(
			AtWindowMessage, REG_WINDOWMESSAGE);
	}

	void call_updatescreen()
	{
		HDC main_dc = GetDC(mainHWND);

		for (auto& pair : hwnd_lua_map)
		{
			/// Let the environment draw to its DCs
			pair.second->draw();

			/// Blit its DCs (GDI, D2D) to the main window with alpha mask
			TransparentBlt(main_dc, 0, 0, pair.second->dc_width,
			               pair.second->dc_height, pair.second->gdi_dc, 0, 0,
			               pair.second->dc_width, pair.second->dc_height,
			               bitmap_color_mask);
			TransparentBlt(main_dc, 0, 0, pair.second->dc_width,
			               pair.second->dc_height, pair.second->d2d_dc, 0, 0,
			               pair.second->dc_width, pair.second->dc_height,
			               bitmap_color_mask);

			// Fill GDI DC with alpha mask
			RECT rect = {0, 0, pair.second->dc_width, pair.second->dc_height};
			HBRUSH brush = CreateSolidBrush(bitmap_color_mask);
			FillRect(pair.second->gdi_dc, &rect, brush);
			DeleteObject(brush);
		}

		ReleaseDC(mainHWND, main_dc);
	}

	void call_vi()
	{
		main_dispatcher_invoke([]
		{
			invoke_callbacks_with_key_on_all_instances(
				AtVI, REG_ATVI);
		});
	}

	void call_input(int n)
	{
		main_dispatcher_invoke([n]
		{
			current_input_n = n;
			invoke_callbacks_with_key_on_all_instances(
				AtInput, REG_ATINPUT);
			inputCount++;
		});
	}

	void call_interval()
	{
		main_dispatcher_invoke([]
		{
			invoke_callbacks_with_key_on_all_instances(
				CallTop, REG_ATINTERVAL);
		});
	}

	void call_play_movie()
	{
		main_dispatcher_invoke([]
		{
			invoke_callbacks_with_key_on_all_instances(
				CallTop, REG_ATPLAYMOVIE);
		});
	}

	void call_stop_movie()
	{
		main_dispatcher_invoke([]
		{
			invoke_callbacks_with_key_on_all_instances(
				CallTop, REG_ATSTOPMOVIE);
		});
	}

	void call_load_state()
	{
		main_dispatcher_invoke([]
		{
			invoke_callbacks_with_key_on_all_instances(
				CallTop, REG_ATLOADSTATE);
		});
	}

	void call_save_state()
	{
		main_dispatcher_invoke([]
		{
			invoke_callbacks_with_key_on_all_instances(
				CallTop, REG_ATSAVESTATE);
		});
	}

	void call_reset()
	{
		main_dispatcher_invoke([]
		{
			invoke_callbacks_with_key_on_all_instances(
				CallTop, REG_ATRESET);
		});
	}
}
