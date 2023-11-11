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
#include <stdio.h>
#include <commctrl.h>
#include "../guifuncs.h"
#include "main_win.h"
#include "Config.hpp"
#include "../rom.h"
#include "../vcr.h"
#include "../../memory/pif.h"
#include "../../winproject/resource.h"

//c++!!!
#include <chrono>
#include <thread>
#include <iostream>

#include "features/Statusbar.hpp"

extern bool ffup;
extern int m_currentVI;
extern long m_currentSample;

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

	for (auto val : vec)
	{
		accumulator += val;
	}

	return accumulator / vec.size();
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
	time_point current_frame_time = std::chrono::high_resolution_clock::now();

	auto diff = (current_frame_time - last_frame_time) / 1'000'000.0f;

	if (diff.count() > 1)
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
	if (!ignoreErrorEmulation && emu_launched && frame_advancing && vis_per_second > 1000)
	{
		int result = 0;
		TaskDialog(mainHWND, app_instance, L"Error",
			L"Unusual core state", L"An emulator core timing inaccuracy or game crash has been detected. You can choose to continue emulation.", TDCBF_RETRY_BUTTON | TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, &result);

		if (result == IDCLOSE) {
			frame_advancing = false; //don't pause at next open
			CreateThread(NULL, 0, close_rom, (LPVOID)1, 0, 0);
		}
	}

	m_currentVI++;

	if (VCR_isRecording())
		VCR_setLengthVIs(m_currentVI);

	if (VCR_isPlaying())
	{
		extern int pauseAtFrame;
		if (vcr_current_sample >= pauseAtFrame && pauseAtFrame >= 0)
		{
			pauseEmu(TRUE); // maybe this is multithreading unsafe?
			pauseAtFrame = -1; // don't pause again
		}
	}

	vi_counter++;

	auto current_vi_time = std::chrono::high_resolution_clock::now();
	auto vi_time_diff = current_vi_time - last_vi_time;


	// if we're playing game normally with no frame advance or ff and overstepping max time between frames,
	// we need to sleep to compensate the additional time
	if (!fast_forward && !frame_advancing && vi_time_diff < max_vi_s_ms)
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
			auto goal_sleep = max_vi_s_ms - vi_time_diff - last_sleep_error;
			auto start_sleep = std::chrono::high_resolution_clock::now();
			std::this_thread::sleep_for(goal_sleep);
			auto end_sleep = std::chrono::high_resolution_clock::now();

			// sleeping inaccuracy is difference between actual time spent sleeping and the goal sleep
			// this value isnt usually too large
			last_sleep_error = end_sleep - start_sleep - goal_sleep;
		} else
		{
			// sleep time is unreasonable, log it and reset related state
			auto casted = std::chrono::duration_cast<
				std::chrono::milliseconds>(sleep_time).count();
			printf("Invalid timer: %lld ms\n", casted);
			sleep_time = sleep_time.zero();
			counter_time = std::chrono::high_resolution_clock::now();
			ffup = false;
		}
	}

	auto post_sleep_vi_time_diff = (std::chrono::high_resolution_clock::now() - last_vi_time) / 1'000'000.0f;
	if (post_sleep_vi_time_diff.count() > 1)
	{
		vis_per_second = add_value(vi_values, 1000.0f / post_sleep_vi_time_diff.count());
	}

	last_vi_time = std::chrono::high_resolution_clock::now();
}
