#include "Statusbar.hpp"

#include <Windows.h>
#include <commctrl.h>
#include <numeric>

#include <shared/Messenger.h>
#include <core/r4300/r4300.h>
#include <core/r4300/vcr.h>
#include "RomBrowser.hpp"
#include "../../winproject/resource.h"
#include <view/helpers/WinHelpers.h>
#include <shared/Config.hpp>
#include <view/gui/Main.h>


namespace Statusbar
{
	const std::vector emu_parts = {200, 200, 77, 77, 77, 77, 77, -1};
	const std::vector idle_parts = {400, -1};

	HWND statusbar_hwnd;

	HWND hwnd()
	{
		return statusbar_hwnd;
	}

	void post(const std::string& text, Section section)
	{
		SendMessage(statusbar_hwnd, SB_SETTEXT, (int)section, (LPARAM)text.c_str());
	}

	void emu_launched_changed(std::any data)
	{
		auto value = std::any_cast<bool>(data);
		static auto previous_value = value;

		if (!statusbar_hwnd)
		{
			previous_value = value;
			return;
		}

		auto parts = value ? emu_parts : idle_parts;

		// We don't want to keep the weird crap from previous state, so let's clear everything
		for (int i = 0; i < 255; ++i)
		{
			SendMessage(statusbar_hwnd, SB_SETTEXT, i, (LPARAM)"");
		}

		// When starting the emu, we want to scale the statusbar segments to the window size
		if (value)
		{
			RECT rect{};
			GetClientRect(g_main_hwnd, &rect);

			// Compute the desired size of the statusbar and use that for the scaling factor
			auto desired_size = std::accumulate(parts.begin(), parts.end() - 1, 0);

			auto scale = static_cast<float>(rect.right - rect.left) / static_cast<float>(desired_size);

			printf("[Statusbar] Scale: %f\n", scale);

			for (auto& part : parts)
			{
				part = static_cast<int>(part * scale);
			}

			set_statusbar_parts(statusbar_hwnd, parts);
		}

		if (!value && previous_value)
		{
			set_statusbar_parts(statusbar_hwnd, parts);
		}

		previous_value = value;
	}

	void create()
	{
		// undocumented behaviour of CCS_BOTTOM: it skips applying SBARS_SIZEGRIP in style pre-computation phase
		statusbar_hwnd = CreateWindowEx(0, STATUSCLASSNAME, nullptr,
		                                WS_CHILD | WS_VISIBLE | CCS_BOTTOM,
		                                0, 0,
		                                0, 0,
		                                g_main_hwnd, (HMENU)IDC_MAIN_STATUS,
		                                g_app_instance, nullptr);
	}


	void statusbar_visibility_changed(std::any data)
	{
		auto value = std::any_cast<bool>(data);

		if (statusbar_hwnd)
		{
			DestroyWindow(statusbar_hwnd);
			statusbar_hwnd = nullptr;
		}

		if (value)
		{
			create();
		}
	}

	void on_rerecords_changed(std::any data)
	{
		auto value = std::any_cast<uint64_t>(data);
		post(std::format("{} rr", value), Section::Rerecords);
	}

	void on_task_changed(std::any data)
	{
		auto value = std::any_cast<e_task>(data);

		if (value == e_task::idle)
		{
			post("", Section::Rerecords);
		}
	}

	void on_slot_changed(std::any data)
	{
		auto value = std::any_cast<size_t>(data);
		post(std::format("Slot {}", value + 1), Section::Slot);
	}

	void init()
	{
		create();
		Messenger::subscribe(Messenger::Message::EmuLaunchedChanged,
		                     emu_launched_changed);
		Messenger::subscribe(Messenger::Message::StatusbarVisibilityChanged,
		                     statusbar_visibility_changed);
		Messenger::subscribe(Messenger::Message::TaskChanged,
		                     on_task_changed);
		Messenger::subscribe(Messenger::Message::RerecordsChanged,
		                     on_rerecords_changed);
		Messenger::subscribe(Messenger::Message::SlotChanged,
		                     on_slot_changed);
	}
}
