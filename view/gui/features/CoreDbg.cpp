#include <view/lua/LuaConsole.h>

#include "../../../winproject/resource.h"
#include <view/gui/Main.h>
#include "CoreDbg.h"

#include <stdio.h>
#include <Windows.h>
#include <Windowsx.h>
#include <thread>

#include <core/r4300/r4300.h>
#include <core/r4300/disasm.h>

#include "core/r4300/debugger.h"

namespace CoreDbg
{
	HWND hwnd = nullptr;

	using t_cpu_state = struct
	{
		unsigned long opcode;
		unsigned long address;
	};

	bool cycle_advance = false;

	DWORD (__cdecl*original_doRspCycles)(DWORD Cycles) = nullptr;

	UINT ui_timer;
	bool ui_invalidated = true;
	t_cpu_state cpu_state = {0};

	BOOL CALLBACK coredbg_dlgproc(HWND hwnd, UINT msg, WPARAM w_param,
	                              LPARAM l_param)
	{
		switch (msg)
		{
		case WM_INITDIALOG:
			CheckDlgButton(hwnd, IDC_COREDBG_RSP_TOGGLE, 1);
			ui_timer = SetTimer(hwnd, NULL, 1000 / 60, nullptr);
			CoreDbg::hwnd = hwnd;
			return TRUE;
		case WM_DESTROY:
			KillTimer(hwnd, ui_timer);
			EndDialog(hwnd, LOWORD(w_param));
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd, IDOK);
			break;
		case WM_TIMER:
			if (!ui_invalidated)
				break;

			SetWindowText(GetDlgItem(hwnd, IDC_COREDBG_TOGGLEPAUSE),
			              Debugger::get_resumed() ? "Pause" : "Resume");
			break;
		case WM_COMMAND:
			switch (LOWORD(w_param))
			{
			case IDC_COREDBG_CART_TILT:
				Debugger::set_dma_read_enabled(!IsDlgButtonChecked(
					hwnd, IDC_COREDBG_CART_TILT));
				break;
			case IDC_COREDBG_RSP_TOGGLE:

				// if real doRspCycles pointer hasnt been stored yet, cache it
				if (!original_doRspCycles)
				{
					original_doRspCycles = doRspCycles;
				}

			// if rsp is disabled, we swap out the real doRspCycles function for the dummy one, effectively disabling the rsp unit
				if (IsDlgButtonChecked(hwnd, IDC_COREDBG_RSP_TOGGLE))
					doRspCycles = original_doRspCycles;
				else
					doRspCycles = dummy_doRspCycles;

				break;
			case IDC_COREDBG_STEP:
				// set step flag and resume
				// the flag is cleared at the end of the current cycle
				cycle_advance = true;
				Debugger::set_is_resumed(true);
				break;
			case IDC_COREDBG_TOGGLEPAUSE:
				Debugger::set_is_resumed(!Debugger::get_resumed());
				ui_invalidated = true;
				break;
			default:
				break;
			}
			break;
		default:
			return FALSE;
		}
		return TRUE;
	}

	void start()
	{
		std::thread([]
		{
			DialogBox(g_app_instance,
			          MAKEINTRESOURCE(IDD_COREDBG),
			          0,
			          coredbg_dlgproc);
		}).detach();
	}

	void on_late_cycle(unsigned long opcode, unsigned long address)
	{
		cpu_state = {
			.opcode = opcode,
			.address = address,
		};
		if (cycle_advance)
		{
			cycle_advance = false;
			Debugger::set_is_resumed(false);
			ui_invalidated = true;

			char disasm[255] = {0};
			DisassembleInstruction(disasm,
			                       cpu_state.opcode,
			                       cpu_state.address);

			auto str = std::format("{} ({:#08x}, {:#08x})", disasm,
			                       cpu_state.opcode, cpu_state.address);
			ListBox_InsertString(GetDlgItem(hwnd, IDC_COREDBG_LIST), 0, str.c_str());
		}
	}
}
