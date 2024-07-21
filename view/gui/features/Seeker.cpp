#include "Seeker.h"

#include <thread>
#include <Windows.h>

#include <shared/messenger.h>
#include "Statusbar.hpp"
#include <core/r4300/vcr.h>
#include "../main_win.h"
#include <view/gui/resource.h>
#include <shared/Config.hpp>

#define WM_SEEK_COMPLETED (WM_USER + 11)

namespace Seeker
{
	HWND current_hwnd;
	UINT_PTR refresh_timer;

	LRESULT CALLBACK SeekerProc(HWND hwnd, UINT Message, WPARAM wParam,
	                            LPARAM lParam)
	{
		switch (Message)
		{
		case WM_INITDIALOG:
			refresh_timer = SetTimer(hwnd, NULL, 1000 / 10, nullptr);
			current_hwnd = hwnd;
			SetDlgItemText(hwnd, IDC_SEEKER_FRAME, Config.seeker_value.c_str());
			SetFocus(GetDlgItem(hwnd, IDC_SEEKER_FRAME));
			SendMessage(hwnd, WM_SEEK_COMPLETED, 0, 0);
			break;
		case WM_DESTROY:
			KillTimer(hwnd, refresh_timer);
			VCR::stop_seek();
			break;
		case WM_CLOSE:
			EndDialog(hwnd, IDCANCEL);
			break;
		case WM_SEEK_COMPLETED:
			SetDlgItemText(hwnd, IDC_SEEKER_STATUS, "Idle");
			SetDlgItemText(hwnd, IDC_SEEKER_START, "Start");
			break;
		case WM_TIMER:
			{
				if (!VCR::is_seeking())
				{
					break;
				}
				auto [current, total] = VCR::get_seek_completion();
				const auto str = std::format("Seeked {:.2f}%", static_cast<float>(current) / static_cast<float>(total) * 100.0);
				SetDlgItemText(hwnd, IDC_SEEKER_STATUS, str.c_str());
				break;
			}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDC_SEEKER_FRAME:
				{
					char str[260]{};
					GetDlgItemText(hwnd, IDC_SEEKER_FRAME, str, std::size(str));
					Config.seeker_value = str;
				}
				break;
			case IDC_SEEKER_START:
				{
					if (VCR::is_seeking())
					{
						VCR::stop_seek();
						break;
					}

					// Relative seek is activated by typing + or - in front of number
					bool relative = Config.seeker_value[0] == '-' || Config.seeker_value[0] == '+';

					int32_t frame;
					try
					{
						frame = std::stoi(Config.seeker_value);
					} catch (...)
					{
						SetDlgItemText(hwnd, IDC_SEEKER_STATUS, "Invalid input");
						break;
					}

					if (VCR::begin_seek_to(frame, relative) != VCR::Result::Ok)
					{
						SetDlgItemText(hwnd, IDC_SEEKER_STATUS, "Couldn't seek");
						break;
					}

					SetDlgItemText(hwnd, IDC_SEEKER_START, "Stop");
					break;
				}
			case IDCANCEL:
				EndDialog(hwnd, IDCANCEL);
				break;
			default: break;
			}
			break;
		default: break;
		}
		return FALSE;
	}

	void show()
	{
		// Let's not have 2 instances of a singleton...
		if (current_hwnd)
		{
			return;
		}

		// We need to run the dialog on another thread, since we don't want to block the main one
		std::thread([]
		{
			DialogBox(app_instance,
			          MAKEINTRESOURCE(IDD_SEEKER), mainHWND,
			          (DLGPROC)SeekerProc);
			current_hwnd = nullptr;
		}).detach();
	}

	void init()
	{
		Messenger::subscribe(Messenger::Message::SeekCompleted, [](std::any)
		{
			if (!current_hwnd)
				return;
			SendMessage(current_hwnd, WM_SEEK_COMPLETED, 0, 0);
		});
	}
}
