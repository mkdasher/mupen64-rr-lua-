#include "MovieDialog.h"
#include <Windows.h>

#include <core/r4300/rom.h>
#include <core/r4300/vcr.h>
#include <core/memory/savestates.h>
#include "Statusbar.hpp"
#include <view/gui/Main.h>
#include <shared/Config.hpp>
#include <winproject/resource.h>
#include <shared/helpers/StringHelpers.h>
#include <view/helpers/WinHelpers.h>
#include <view/helpers/MathHelpers.h>
#include <view/gui/wrapper/PersistentPathDialog.h>
#include <view/helpers/IOHelpers.h>
#include <CommCtrl.h>


namespace MovieDialog
{
	t_record_params record_params;
	bool is_readonly;
	HWND grid_hwnd;

	size_t count_button_presses(const std::vector<BUTTONS>& buttons, const int mask)
	{
		size_t accumulator = 0;
		bool pressed = false;
		for (const auto btn : buttons)
		{
			const bool value = !!(btn.Value >> mask & 1);

			if (value && !pressed)
			{
				accumulator++;
				pressed = true;
			} else if (!value)
			{
				pressed = false;
			}
		}
		return accumulator;
	}

	size_t count_unused_inputs(const std::vector<BUTTONS>& buttons)
	{
		size_t accumulator = 0;
		for (const auto btn : buttons)
		{
			if (btn.Value == 0)
			{
				accumulator++;
			}
		}
		return accumulator;
	}

	size_t count_joystick_frames(const std::vector<BUTTONS>& buttons)
	{
		size_t accumulator = 0;
		for (const auto btn : buttons)
		{
			if (btn.X_AXIS != 0 || btn.Y_AXIS != 0)
			{
				accumulator++;
			}
		}
		return accumulator;
	}

	size_t count_input_changes(const std::vector<BUTTONS>& buttons)
	{
		size_t accumulator = 0;
		BUTTONS last_input = {0};
		for (const auto btn : buttons)
		{
			if (btn.Value != last_input.Value)
			{
				accumulator++;
			}
			last_input = btn;
		}
		return accumulator;
	}

	LRESULT CALLBACK MovieInspectorProc(HWND hwnd, UINT Message, WPARAM wParam,
	                                    LPARAM lParam)
	{
		// List of dialog item IDs that shouldn't be interactable when in a specific mode
		const std::vector disabled_on_play = {
			IDC_RADIO_FROM_START,
			IDC_RADIO_FROM_ST, IDC_RADIO_FROM_EEPROM, IDC_RADIO_FROM_EXISTING_ST
		};
		const std::vector disabled_on_record = {
			IDC_PAUSE_AT_END, IDC_PAUSEAT_FIELD
		};

		switch (Message)
		{
		case WM_INITDIALOG:
			{
				RECT grid_rect = get_window_rect_client_space(
					hwnd, GetDlgItem(hwnd, IDC_MOVIE_INFO_TEMPLATE));
				DestroyWindow(GetDlgItem(hwnd, IDC_MOVIE_INFO_TEMPLATE));

				grid_hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
				                           WS_TABSTOP | WS_VISIBLE | WS_CHILD |
				                           LVS_SINGLESEL | LVS_REPORT |
				                           LVS_SHOWSELALWAYS,
				                           grid_rect.left, grid_rect.top,
				                           grid_rect.right - grid_rect.left,
				                           grid_rect.bottom - grid_rect.top,
				                           hwnd, nullptr,
				                           g_app_instance,
				                           NULL);

				ListView_SetExtendedListViewStyle(grid_hwnd,
				                                  LVS_EX_GRIDLINES |
				                                  LVS_EX_FULLROWSELECT |
				                                  LVS_EX_DOUBLEBUFFER);

				LVCOLUMN lv_column = {0};
				lv_column.mask = LVCF_FMT | LVCF_DEFAULTWIDTH | LVCF_TEXT |
					LVCF_SUBITEM;

				lv_column.pszText = (LPTSTR)"Name";
				ListView_InsertColumn(grid_hwnd, 0, &lv_column);
				lv_column.pszText = (LPTSTR)"Value";
				ListView_InsertColumn(grid_hwnd, 1, &lv_column);

				ListView_SetColumnWidth(grid_hwnd, 0, LVSCW_AUTOSIZE_USEHEADER);
				ListView_SetColumnWidth(grid_hwnd, 1, LVSCW_AUTOSIZE_USEHEADER);

				SetWindowText(
					hwnd, is_readonly ? "Play Movie" : "Record Movie");
				for (auto id : is_readonly
					               ? disabled_on_play
					               : disabled_on_record)
				{
					EnableWindow(GetDlgItem(hwnd, id), false);
				}
				SendMessage(GetDlgItem(hwnd, IDC_INI_DESCRIPTION),
				            EM_SETLIMITTEXT,
				            sizeof(t_movie_header::description), 0);
				SendMessage(GetDlgItem(hwnd, IDC_INI_AUTHOR), EM_SETLIMITTEXT,
				            sizeof(t_movie_header::author),
				            0);

				SetDlgItemText(hwnd, IDC_INI_AUTHOR,
				               g_config.last_movie_author.c_str());
				SetDlgItemText(hwnd, IDC_INI_DESCRIPTION, "");

				SetDlgItemText(hwnd, IDC_INI_MOVIEFILE,
				               record_params.path.string().c_str());


				// workaround because initial selected button is "Start"
				SetFocus(GetDlgItem(hwnd, IDC_INI_AUTHOR));

				return FALSE;
			}
		case WM_DESTROY:
			{
				char author[sizeof(t_movie_header::author)] = {0};
				GetDlgItemTextA(hwnd, IDC_INI_AUTHOR, author,
								std::size(author));
				record_params.author = author;

				char description[sizeof(t_movie_header::description)] = {0};
				GetDlgItemTextA(hwnd, IDC_INI_DESCRIPTION, description,
								std::size(description));
				record_params.description = description;

				DestroyWindow(grid_hwnd);
				break;
			}
		case WM_CLOSE:
			record_params.path.clear();
			EndDialog(hwnd, IDCANCEL);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDC_OK:
			case IDOK:
				{
					char text[MAX_PATH] = {0};
					GetDlgItemText(hwnd, IDC_PAUSEAT_FIELD, text,
					               std::size(text));
					if (strlen(text) == 0)
					{
						record_params.pause_at = -1;
					} else
					{
						record_params.pause_at = std::atoi(text);
					}
					record_params.pause_at_last = IsDlgButtonChecked(
						hwnd, IDC_PAUSE_AT_END);

					g_config.last_movie_type = record_params.start_flag;

					if (record_params.start_flag ==
						MOVIE_START_FROM_EXISTING_SNAPSHOT)
					{
						// The default directory we open the file dialog window in is the
						// parent directory of the last savestate that the user saved or loaded
						std::string path = wstring_to_string(
							show_persistent_open_dialog(
								"o_movie_existing_snapshot", hwnd,
								L"*.st;*.savestate"));

						if (path.empty())
						{
							break;
						}

						st_path = path;
						st_medium = e_st_medium::path;

						std::filesystem::path movie_path = strip_extension(path)
							+ ".m64";

						if (exists(movie_path))
						{
							if (MessageBox(
									hwnd, std::format(
										"{} already exists. Are you sure want to overwrite this movie?",
										movie_path.string()).c_str(), "VCR",
									MB_YESNO) ==
								IDNO)
								break;
						}
					}

					EndDialog(hwnd, IDOK);
				}
				break;
			case IDC_CANCEL:
			case IDCANCEL:
				record_params.path.clear();
				EndDialog(hwnd, IDCANCEL);
				break;
			case IDC_INI_MOVIEFILE:
				{
					char path[MAX_PATH] = {0};
					GetDlgItemText(hwnd, IDC_INI_MOVIEFILE, path, std::size(path));
					record_params.path = path;

					// User might not provide the m64 extension, so just force it to have that
					record_params.path.replace_extension(".m64");

					goto refresh;
				}
			case IDC_MOVIE_BROWSE:
				{
					std::wstring path;
					if (is_readonly)
					{
						path = show_persistent_open_dialog(
							"o_movie", hwnd, L"*.m64;*.rec");
					} else
					{
						path = show_persistent_save_dialog(
							"s_movie", hwnd, L"*.m64;*.rec");
					}

					if (path.size() == 0)
					{
						break;
					}

					SetDlgItemText(hwnd, IDC_INI_MOVIEFILE,
					               wstring_to_string(path).c_str());
				}
				break;
			case IDC_RADIO_FROM_EEPROM:
				EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 1);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 1);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 1);
				record_params.start_flag = MOVIE_START_FROM_EEPROM;
				break;
			case IDC_RADIO_FROM_ST:
				EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 1);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 1);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 1);
				record_params.start_flag = MOVIE_START_FROM_SNAPSHOT;
				break;
			case IDC_RADIO_FROM_EXISTING_ST:
				EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 0);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 0);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 0);
				record_params.start_flag = MOVIE_START_FROM_EXISTING_SNAPSHOT;
				break;
			case IDC_RADIO_FROM_START:
				EnableWindow(GetDlgItem(hwnd, IDC_MOVIE_BROWSE), 1);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE), 1);
				EnableWindow(GetDlgItem(hwnd, IDC_INI_MOVIEFILE_TEXT), 1);
				record_params.start_flag = MOVIE_START_FROM_NOTHING;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		return FALSE;

	refresh:
		char tempbuf[260] = {0};
		t_movie_header header = {};

		if (VCR::parse_header(record_params.path, &header) != VCR::Result::Ok)
		{
			return FALSE;
		}

		std::vector<BUTTONS> inputs = {};

		if (VCR::read_movie_inputs(record_params.path, inputs) != VCR::Result::Ok)
		{
			return FALSE;
		}

		std::vector<std::pair<std::string, std::string>> metadata;

		ListView_DeleteAllItems(grid_hwnd);

		metadata.emplace_back(std::make_pair("ROM", std::format("{} ({}, {})",
		                                                        (char*)header.rom_name,
		                                                        country_code_to_country_name(header.rom_country),
		                                                        std::format("{:#08x}", header.rom_crc1))));

		metadata.emplace_back(std::make_pair("Length",
		                                     std::format(
			                                     "{} ({} input)",
			                                     header.length_vis,
			                                     header.length_samples)));
		metadata.emplace_back(std::make_pair("Duration", format_duration((double)header.length_vis / (double)header.vis_per_second)));
		metadata.emplace_back(
			std::make_pair("Rerecords", std::to_string(header.rerecord_count)));

		metadata.emplace_back(
			std::make_pair("Video Plugin", header.video_plugin_name));
		metadata.emplace_back(
			std::make_pair("Input Plugin", header.input_plugin_name));
		metadata.emplace_back(
			std::make_pair("Sound Plugin", header.audio_plugin_name));
		metadata.emplace_back(
			std::make_pair("RSP Plugin", header.rsp_plugin_name));

		for (int i = 0; i < 4; ++i)
		{
			strcpy(tempbuf, (header.controller_flags & CONTROLLER_X_PRESENT(i))
				                ? "Present"
				                : "Disconnected");
			if (header.controller_flags & CONTROLLER_X_MEMPAK(i))
				strcat(tempbuf, " with mempak");
			if (header.controller_flags & CONTROLLER_X_RUMBLE(i))
				strcat(tempbuf, " with rumble");

			metadata.emplace_back(
				std::make_pair(std::format("Controller {}", i + 1), tempbuf));
		}

		metadata.emplace_back(
			std::make_pair("A Presses", std::to_string(count_button_presses(inputs, 7))));
		metadata.emplace_back(
			std::make_pair("B Presses", std::to_string(count_button_presses(inputs, 6))));
		metadata.emplace_back(
			std::make_pair("Z Presses", std::to_string(count_button_presses(inputs, 5))));
		metadata.emplace_back(
			std::make_pair("S Presses", std::to_string(count_button_presses(inputs, 4))));
		metadata.emplace_back(
			std::make_pair("R Presses", std::to_string(count_button_presses(inputs, 12))));

		metadata.emplace_back(
			std::make_pair("C^ Presses", std::to_string(count_button_presses(inputs, 11))));
		metadata.emplace_back(
			std::make_pair("Cv Presses", std::to_string(count_button_presses(inputs, 10))));
		metadata.emplace_back(
			std::make_pair("C< Presses", std::to_string(count_button_presses(inputs, 9))));
		metadata.emplace_back(
			std::make_pair("C> Presses", std::to_string(count_button_presses(inputs, 8))));

		const auto lag_frames = max(0, (int64_t)header.length_vis - 2 * (int64_t)header.length_samples);
		metadata.emplace_back(
			std::make_pair("Lag Frames (approximation)", std::to_string(lag_frames)));
		metadata.emplace_back(
			std::make_pair("Unused Inputs", std::to_string(count_unused_inputs(inputs))));
		metadata.emplace_back(
			std::make_pair("Joystick Frames", std::to_string(count_joystick_frames(inputs))));
		metadata.emplace_back(
			std::make_pair("Input Changes", std::to_string(count_input_changes(inputs))));


		SetDlgItemText(hwnd, IDC_INI_AUTHOR, header.author);
		SetDlgItemText(hwnd, IDC_INI_DESCRIPTION, header.description);

		CheckDlgButton(hwnd, IDC_RADIO_FROM_ST,
		               header.startFlags ==
		               MOVIE_START_FROM_SNAPSHOT);
		CheckDlgButton(hwnd, IDC_RADIO_FROM_EXISTING_ST,
		               header.startFlags ==
		               MOVIE_START_FROM_EXISTING_SNAPSHOT);
		CheckDlgButton(hwnd, IDC_RADIO_FROM_START,
		               header.startFlags ==
		               MOVIE_START_FROM_NOTHING);
		CheckDlgButton(hwnd, IDC_RADIO_FROM_EEPROM,
		               header.startFlags ==
		               MOVIE_START_FROM_EEPROM);

		LVITEM lv_item = {0};
		lv_item.mask = LVIF_TEXT | LVIF_DI_SETITEM;
		for (int i = 0; i < metadata.size(); ++i)
		{
			lv_item.iItem = i;

			lv_item.iSubItem = 0;
			lv_item.pszText = (LPTSTR)metadata[i].first.c_str();
			ListView_InsertItem(grid_hwnd, &lv_item);

			lv_item.iSubItem = 1;
			lv_item.pszText = (LPTSTR)metadata[i].second.c_str();
			ListView_SetItem(grid_hwnd, &lv_item);
		}

		ListView_SetColumnWidth(grid_hwnd, 0, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(grid_hwnd, 1, LVSCW_AUTOSIZE_USEHEADER);

		return FALSE;
	}

	t_record_params show(bool readonly)
	{
		is_readonly = readonly;
		record_params.path = std::format("{} ({}).m64", (char*)ROM_HEADER.nom,
		                                 country_code_to_country_name(
			                                 ROM_HEADER.Country_code));
		record_params.start_flag = g_config.last_movie_type;
		record_params.author = g_config.last_movie_author;
		record_params.description = "";

		DialogBox(g_app_instance,
		          MAKEINTRESOURCE(IDD_MOVIE_PLAYBACK_DIALOG), g_main_hwnd,
		          (DLGPROC)MovieInspectorProc);

		return record_params;
	}
}
