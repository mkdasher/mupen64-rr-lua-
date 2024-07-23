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

#include "timers.h"
#include <shared/Config.hpp>
#include <shared/Messenger.h>
#include <core/memory/pif.h>
#include <core/r4300/vcr.h>

bool frame_changed = true;
extern int m_current_vi;
extern long m_current_sample;

std::chrono::duration<double, std::milli> max_vi_s_ms;

float g_frame_deltas[max_deltas] = {0};
float g_vi_deltas[max_deltas] = {0};

size_t frame_deltas_ptr = 0;
size_t vi_deltas_ptr = 0;

std::mutex timepoints_mutex;

time_point last_vi_time;
time_point last_frame_time;

void timer_init(int32_t speed_modifier, t_rom_header* rom_header)
{
	const double max_vi_s = get_vis_per_second(rom_header->Country_code);
	max_vi_s_ms = std::chrono::duration<double, std::milli>(
		1000.0 / (max_vi_s * static_cast<double>(speed_modifier) / 100));

	last_frame_time = std::chrono::high_resolution_clock::now();
	last_vi_time = std::chrono::high_resolution_clock::now();

	memset(g_frame_deltas, 0, sizeof(g_frame_deltas));
	memset(g_vi_deltas, 0, sizeof(g_vi_deltas));

	frame_deltas_ptr = 0;
	vi_deltas_ptr = 0;

	Messenger::broadcast(Messenger::Message::SpeedModifierChanged, speed_modifier);
}

void timer_new_frame()
{
	const auto current_frame_time = std::chrono::high_resolution_clock::now();

	timepoints_mutex.lock();
	g_frame_deltas[frame_deltas_ptr] = std::chrono::duration<double, std::milli>(current_frame_time - last_frame_time).count();
	frame_deltas_ptr = (frame_deltas_ptr + 1) % max_deltas;
	timepoints_mutex.unlock();

	frame_changed = true;
	last_frame_time = std::chrono::high_resolution_clock::now();
}

void timer_new_vi()
{
	if (g_config.max_lag != 0 && lag_count >= g_config.max_lag)
	{
		Messenger::broadcast(Messenger::Message::LagLimitExceeded, nullptr);
	}
	bool ff = fast_forward || VCR::is_seeking();

	const auto current_vi_time = std::chrono::high_resolution_clock::now();

	if (!ff)
	{
		static std::chrono::duration<double, std::nano> last_sleep_error;
		// if we're playing game normally with no frame advance or ff and overstepping max time between frames,
		// we need to sleep to compensate the additional time
		const auto vi_time_diff = current_vi_time - last_vi_time;
		if (!frame_advancing && vi_time_diff < max_vi_s_ms)
		{
			auto sleep_time = max_vi_s_ms - vi_time_diff;
			if (sleep_time.count() > 0 && sleep_time <
				std::chrono::milliseconds(700))
			{
				// we try to sleep for the overstepped time, but must account for sleeping inaccuracies
				const auto goal_sleep = max_vi_s_ms - vi_time_diff -
					last_sleep_error;
				const auto start_sleep =
					std::chrono::high_resolution_clock::now();
				std::this_thread::sleep_for(goal_sleep);
				const auto end_sleep =
					std::chrono::high_resolution_clock::now();

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
			}
		}
	}

	timepoints_mutex.lock();
	g_vi_deltas[vi_deltas_ptr] = std::chrono::duration<double, std::milli>(
		current_vi_time - last_vi_time).count();
	vi_deltas_ptr = (vi_deltas_ptr + 1) % max_deltas;
	timepoints_mutex.unlock();

	last_vi_time = std::chrono::high_resolution_clock::now();
}
