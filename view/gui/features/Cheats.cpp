#include "Cheats.h"

#include <Windows.h>
#include <Windowsx.h>
#include <view/gui/Main.h>
#include <winproject/resource.h>
#include <shared/services/FrontendService.h>

#include <core/r4300/gameshark.h>

namespace Cheats
{
	LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam,
	                         LPARAM lParam)
	{
		switch (Message)
		{
		case WM_INITDIALOG:
			{
				goto rebuild_list;
			}
		case WM_DESTROY:

			break;
		case WM_CLOSE:
			EndDialog(hwnd, IDCANCEL);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDC_LIST_CHEATS:
				goto update_selection;
			case IDC_NEW_CHEAT:
				{
					auto script = Gameshark::Script::compile("D033AFA1 0020\n8133B1BC 4220\nD033AFA1 0020\n8133B17C 0300\nD033AFA1 0020\n8133B17E 0880");
					if (script.has_value())
					{
						Gameshark::scripts.push_back(script.value());
						goto rebuild_list;
					}
					break;
				}
			case IDC_REMOVE_CHEAT:
				{
					auto lb_hwnd = GetDlgItem(hwnd, IDC_LIST_CHEATS);
					auto selected_index = ListBox_GetCurSel(lb_hwnd);
					if (selected_index == -1) {
						break;
					}

					Gameshark::scripts.erase(Gameshark::scripts.begin() + selected_index);
					goto rebuild_list;
					break;
				}
			case IDC_CHECK_CHEAT_ENABLED:
				{
					auto lb_hwnd = GetDlgItem(hwnd, IDC_LIST_CHEATS);
					auto selected_index = ListBox_GetCurSel(lb_hwnd);
					if (selected_index == -1)
					{
						break;
					}
					auto script = Gameshark::scripts[selected_index];
					script->set_resumed(IsDlgButtonChecked(hwnd, IDC_CHECK_CHEAT_ENABLED) == BST_CHECKED);
					goto rebuild_list;
				}

			case IDC_CHEAT_APPLY:
				{
					auto lb_hwnd = GetDlgItem(hwnd, IDC_LIST_CHEATS);
					auto selected_index = ListBox_GetCurSel(lb_hwnd);
					if (selected_index == -1)
					{
						break;
					}

					const bool prev_resumed = Gameshark::scripts[selected_index]->resumed();

					char code[4096]{};
					Edit_GetText(GetDlgItem(hwnd, IDC_EDIT_CHEAT), code, sizeof(code));

					char name[256]{};
					Edit_GetText(GetDlgItem(hwnd, IDC_EDIT_CHEAT_NAME), name, sizeof(name));

					// Replace with script recompiled with new code, while keeping some properties
					// Old script goes out of ref
					auto script = Gameshark::Script::compile(code);

					if (!script.has_value())
					{
						FrontendService::show_error("Cheat code could not be compiled.\r\nVerify that the syntax is correct.", nullptr, hwnd);
						break;
					}

					script.value()->set_name(name);
					script.value()->set_resumed(prev_resumed);
					Gameshark::scripts[selected_index] = script.value();
					goto rebuild_list;
				}
			default: break;
			}
			break;

		default: break;
		}
		return FALSE;

	update_selection:
		{
			auto selected_index = ListBox_GetCurSel(GetDlgItem(hwnd, IDC_LIST_CHEATS));
			if (selected_index == -1)
			{
				return FALSE;
			}

			CheckDlgButton(hwnd, IDC_CHECK_CHEAT_ENABLED,
			               Gameshark::scripts[selected_index]->resumed() ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemText(hwnd, IDC_EDIT_CHEAT, Gameshark::scripts[selected_index]->code().c_str());
			Edit_SetText(GetDlgItem(hwnd, IDC_EDIT_CHEAT_NAME), Gameshark::scripts[selected_index]->name().c_str());
		}
		return FALSE;


	rebuild_list:
		{
			auto lb_hwnd = GetDlgItem(hwnd, IDC_LIST_CHEATS);
			auto prev_index = ListBox_GetCurSel(lb_hwnd);
			ListBox_ResetContent(lb_hwnd);
			for (auto script : Gameshark::scripts)
			{
				auto name = !script->resumed() ? script->name() + " (Disabled)" : script->name();
				ListBox_AddString(lb_hwnd, name.c_str());
			}
			ListBox_SetCurSel(lb_hwnd, prev_index);
			goto update_selection;
		}
	}

	void show()
	{
		DialogBox(g_app_instance,
		          MAKEINTRESOURCE(IDD_CHEATS), g_main_hwnd,
		          (DLGPROC)WndProc);
	}
}
