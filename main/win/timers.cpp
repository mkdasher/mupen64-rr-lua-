/***************************************************************************
						  timers.c  -  description
							 -------------------
	copyright            : (C) 2003 by ShadowPrince
	email                : shadow@emulation64.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "../../lua/LuaConsole.h"

#include <Windows.h>
#include <cstdio>
#include <commctrl.h>
#include "main_win.h"
#include "Config.hpp"
#include "../rom.h"
#include "../vcr.h"
#include "../../memory/pif.h"
//c++!!!
#include <chrono>
#include <thread>

#include "features/Statusbar.hpp"

extern bool ffup;

std::chrono::duration<double, std::milli> max_vi_s_ms;
float max_vi_s, vis_per_second, fps;
typedef std::chrono::high_resolution_clock::time_point time_point;

std::deque<float> fps_values;
std::deque<float> vi_values;

float add_value(std::deque<float>& vec, const float value, const size_t max_size = 30)
{
	if (vec.size() > max_size)
	{
		vec.pop_front();
	}

	vec.push_back(value);

	float accumulator = 0.0f;

	for (const auto val : vec)
	{
		accumulator += val;
	}

	return accumulator / (float)vec.size();
}

void timer_init()
{
	max_vi_s = (float)get_vis_per_second(ROM_HEADER.Country_code);
	max_vi_s_ms = std::chrono::duration<double, std::milli>(
		1000.0 / (max_vi_s * (float)Config.fps_modifier / 100));
	fps_values = {};
	vi_values = {};
	fps = 0;
	vis_per_second = 0;
	statusbar_post_text(std::format("Speed limit: {}%", Config.fps_modifier));
}


void timer_new_frame()
{
	static time_point last_statusbar_update_time = std::chrono::high_resolution_clock::now();
	static time_point last_frame_time = std::chrono::high_resolution_clock::now();
	const time_point current_frame_time = std::chrono::high_resolution_clock::now();

	if (const auto diff = (current_frame_time - last_frame_time) / 1'000'000.0f; diff.count() > 1)
	{
		fps = add_value(fps_values, 1000.0f / diff.count());
	}

	last_frame_time = current_frame_time;

	// we dont need precise updates when ff'ing
	if (!fast_forward)
	{
		vcr_update_statusbar();
	}

	if (current_frame_time - last_statusbar_update_time > std::chrono::seconds(1))
	{
		if (Config.show_fps)
		{
			statusbar_post_text(std::format("FPS: {:.1f}", fps), 2);
		}
		if (Config.show_vis_per_second)
		{
			statusbar_post_text(std::format("VI/s: {:.1f}", vis_per_second), 3);
		}
		last_statusbar_update_time = current_frame_time;
	}
}



void timer_new_vi()
{
	// time when last vi happened
	static time_point last_vi_time;
	static time_point counter_time;
	static std::chrono::duration<double, std::nano> last_sleep_error;
	static unsigned int vi_counter = 0;

	// fps wont update when emu is stuck so we must check vi/s
	// vi/s shouldn't go over 1000 in normal gameplay while holding down fast forward unless you have repeat speed at uzi speed
	if (!ignore_error_emulation && emu_launched && frame_advancing && vis_per_second > 1000)
	{
		int result = 0;
		TaskDialog(main_hwnd, app_instance, L"Error",
			L"Unusual core state", L"An emulator core timing inaccuracy or game crash has been detected. You can choose to continue emulation.", TDCBF_RETRY_BUTTON | TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, &result);

		if (result == IDCLOSE) {
			frame_advancing = false; //don't pause at next open
			CreateThread(nullptr, 0, close_rom, (LPVOID)1, 0, nullptr);
		}
	}

	vcr_current_vi++;

	if (vcr_is_recording())
		vcr_set_length_v_is(vcr_current_vi);

	if (vcr_is_playing())
	{
		if (extern int pause_at_frame; vcr_current_sample >= pause_at_frame && pause_at_frame >= 0)
		{
			pause_emu(TRUE); // maybe this is multithreading unsafe?
			pause_at_frame = -1; // don't pause again
		}
	}

	vi_counter++;

	const auto current_vi_time = std::chrono::high_resolution_clock::now();


	// if we're playing game normally with no frame advance or ff and overstepping max time between frames,
	// we need to sleep to compensate the additional time
	if (const auto vi_time_diff = current_vi_time - last_vi_time; !fast_forward && !frame_advancing && vi_time_diff < max_vi_s_ms)
	{
		auto sleep_time = max_vi_s_ms - vi_time_diff;

		// if we just released fastforward we dont sleep at all
		// this fixes an old bug where game would sleep for too long after going out of ff mode
		if (ffup)
		{
			sleep_time = sleep_time.zero();
			counter_time = std::chrono::high_resolution_clock::now();
			ffup = false;
		}
		if (sleep_time.count() > 0 && sleep_time < std::chrono::milliseconds(700))
		{
			// we try to sleep for the overstepped time, but must account for sleeping inaccuracies
			const auto goal_sleep = max_vi_s_ms - vi_time_diff - last_sleep_error;
			const auto start_sleep = std::chrono::high_resolution_clock::now();
			std::this_thread::sleep_for(goal_sleep);
			const auto end_sleep = std::chrono::high_resolution_clock::now();

			// sleeping inaccuracy is difference between actual time spent sleeping and the goal sleep
			// this value isnt usually too large
			last_sleep_error = end_sleep - start_sleep - goal_sleep;
		} else
		{
			// sleep time is unreasonable, log it and reset related state
			const auto casted = std::chrono::duration_cast<
				std::chrono::milliseconds>(sleep_time).count();
			printf("Invalid timer: %lld ms\n", casted);
			sleep_time = sleep_time.zero();
			counter_time = std::chrono::high_resolution_clock::now();
			ffup = false;
		}
	}

	if (const auto post_sleep_vi_time_diff = (std::chrono::high_resolution_clock::now() -
		last_vi_time) / 1'000'000.0f; post_sleep_vi_time_diff.count() > 1)
	{
		vis_per_second = add_value(vi_values, 1000.0f / post_sleep_vi_time_diff.count());
	}

	last_vi_time = std::chrono::high_resolution_clock::now();
}
