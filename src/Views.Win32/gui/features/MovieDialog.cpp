/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include "MovieDialog.h"
#include "Config.h"
#include "DialogService.h"
#include <gui/Main.h>
#include <gui/wrapper/PersistentPathDialog.h>

namespace MovieDialog
{
    t_record_params record_params;
    bool is_readonly;
    HWND grid_hwnd;
    bool g_is_closing;

    size_t count_button_presses(const std::vector<core_buttons>& buttons, const int mask)
    {
        size_t accumulator = 0;
        bool pressed = false;
        for (const auto btn : buttons)
        {
            const bool value = !!(btn.value >> mask & 1);

            if (value && !pressed)
            {
                accumulator++;
                pressed = true;
            }
            else if (!value)
            {
                pressed = false;
            }
        }
        return accumulator;
    }

    size_t count_unused_inputs(const std::vector<core_buttons>& buttons)
    {
        size_t accumulator = 0;
        for (const auto btn : buttons)
        {
            if (btn.value == 0)
            {
                accumulator++;
            }
        }
        return accumulator;
    }

    size_t count_joystick_frames(const std::vector<core_buttons>& buttons)
    {
        size_t accumulator = 0;
        for (const auto btn : buttons)
        {
            if (btn.x != 0 || btn.y != 0)
            {
                accumulator++;
            }
        }
        return accumulator;
    }

    size_t count_input_changes(const std::vector<core_buttons>& buttons)
    {
        size_t accumulator = 0;
        core_buttons last_input = {0};
        for (const auto btn : buttons)
        {
            if (btn.value != last_input.value)
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
        IDC_RADIO_FROM_ST, IDC_RADIO_FROM_EEPROM, IDC_RADIO_FROM_EXISTING_ST};
        const std::vector disabled_on_record = {
        IDC_PAUSE_AT_END, IDC_PAUSEAT_FIELD};

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

                lv_column.pszText = const_cast<LPWSTR>(L"Name");
                ListView_InsertColumn(grid_hwnd, 0, &lv_column);
                lv_column.pszText = const_cast<LPWSTR>(L"Value");
                ListView_InsertColumn(grid_hwnd, 1, &lv_column);

                ListView_SetColumnWidth(grid_hwnd, 0, LVSCW_AUTOSIZE_USEHEADER);
                ListView_SetColumnWidth(grid_hwnd, 1, LVSCW_AUTOSIZE_USEHEADER);

                SetWindowText(
                hwnd, is_readonly ? L"Play Movie" : L"Record Movie");
                for (auto id : is_readonly
                     ? disabled_on_play
                     : disabled_on_record)
                {
                    EnableWindow(GetDlgItem(hwnd, id), false);
                }
                SendMessage(GetDlgItem(hwnd, IDC_INI_DESCRIPTION),
                            EM_SETLIMITTEXT,
                            sizeof(core_vcr_movie_header::description), 0);
                SendMessage(GetDlgItem(hwnd, IDC_INI_AUTHOR), EM_SETLIMITTEXT,
                            sizeof(core_vcr_movie_header::author),
                            0);

                SetDlgItemText(hwnd, IDC_INI_AUTHOR,
                               g_config.last_movie_author.c_str());
                SetDlgItemText(hwnd, IDC_INI_DESCRIPTION, L"");

                SetDlgItemText(hwnd, IDC_INI_MOVIEFILE, record_params.path.wstring().c_str());


                // workaround because initial selected button is "Start"
                SetFocus(GetDlgItem(hwnd, IDC_INI_AUTHOR));

                return FALSE;
            }
        case WM_DESTROY:
            {
                wchar_t author[sizeof(core_vcr_movie_header::author)] = {0};
                GetDlgItemText(hwnd, IDC_INI_AUTHOR, author, std::size(author));
                record_params.author = author;

                wchar_t description[sizeof(core_vcr_movie_header::description)] = {0};
                GetDlgItemText(hwnd, IDC_INI_DESCRIPTION, description, std::size(description));
                record_params.description = description;

                DestroyWindow(grid_hwnd);
                break;
            }
        case WM_CLOSE:
            record_params.path.clear();
            g_is_closing = true;
            EndDialog(hwnd, IDCANCEL);
            break;
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_OK:
            case IDOK:
                {
                    wchar_t text[MAX_PATH] = {0};
                    GetDlgItemText(hwnd, IDC_PAUSEAT_FIELD, text,
                                   std::size(text));
                    if (lstrlenW(text) == 0)
                    {
                        record_params.pause_at = -1;
                    }
                    else
                    {
                        record_params.pause_at = std::wcstoul(text, nullptr, 10);
                    }
                    record_params.pause_at_last = IsDlgButtonChecked(
                    hwnd, IDC_PAUSE_AT_END);

                    g_config.last_movie_type = record_params.start_flag;

                    if (record_params.start_flag ==
                        MOVIE_START_FROM_EXISTING_SNAPSHOT)
                    {
                        // The default directory we open the file dialog window in is the
                        // parent directory of the last savestate that the user saved or loaded
                        std::filesystem::path path = show_persistent_open_dialog(L"o_movie_existing_snapshot", hwnd, L"*.st;*.savestate");

                        if (path.empty())
                        {
                            break;
                        }

                        path.replace_extension(L".exe");

                        if (exists(path))
                        {
                            const auto str = std::format(L"{} already exists. Are you sure want to overwrite this movie?", path.wstring());
                            if (!DialogService::show_ask_dialog(VIEW_DLG_MOVIE_OVERWRITE_WARNING, str.c_str(), L"VCR", true, hwnd))
                            {
                                break;
                            }
                        }
                    }

                    EndDialog(hwnd, IDOK);
                }
                break;
            case IDC_CANCEL:
            case IDCANCEL:
                record_params.path.clear();
                g_is_closing = true;
                EndDialog(hwnd, IDCANCEL);
                break;
            case IDC_INI_MOVIEFILE:
                {
                    if (g_is_closing)
                    {
                        g_view_logger->warn("[MovieDialog] Tried to update movie file path while closing dialog");
                        break;
                    }

                    wchar_t path[MAX_PATH] = {0};
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
                        L"o_movie", hwnd, L"*.m64;*.rec");
                    }
                    else
                    {
                        path = show_persistent_save_dialog(
                        L"s_movie", hwnd, L"*.m64;*.rec");
                    }

                    if (path.empty())
                    {
                        break;
                    }

                    SetDlgItemText(hwnd, IDC_INI_MOVIEFILE, path.c_str());
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
        wchar_t tempbuf[520] = {0};
        core_vcr_movie_header header = {};

        if (core_vcr_parse_header(record_params.path, &header) != Res_Ok)
        {
            return FALSE;
        }

        std::vector<core_buttons> inputs = {};

        if (core_vcr_read_movie_inputs(record_params.path, inputs) != Res_Ok)
        {
            return FALSE;
        }

        std::vector<std::pair<std::wstring, std::wstring>> metadata;

        ListView_DeleteAllItems(grid_hwnd);
        
        metadata.emplace_back(std::make_pair(L"ROM", std::format(L"{} ({}, {})", string_to_wstring((char*)header.rom_name), core_vr_country_code_to_country_name(header.rom_country), std::format(L"{:#08x}", header.rom_crc1))));

        metadata.emplace_back(std::make_pair(L"Length",
                                             std::format(
                                             L"{} ({} input)",
                                             header.length_vis,
                                             header.length_samples)));
        metadata.emplace_back(std::make_pair(L"Duration", format_duration((double)header.length_vis / (double)header.vis_per_second)));
        metadata.emplace_back(
        std::make_pair(L"Rerecords", std::to_wstring(static_cast<uint64_t>(header.extended_data.rerecord_count) << 32 | header.rerecord_count)));

        metadata.emplace_back(
        std::make_pair(L"Video Plugin", string_to_wstring(header.video_plugin_name)));
        metadata.emplace_back(
        std::make_pair(L"Input Plugin", string_to_wstring(header.input_plugin_name)));
        metadata.emplace_back(
        std::make_pair(L"Sound Plugin", string_to_wstring(header.audio_plugin_name)));
        metadata.emplace_back(
        std::make_pair(L"RSP Plugin", string_to_wstring(header.rsp_plugin_name)));

        for (int i = 0; i < 4; ++i)
        {
            StrCpy(tempbuf, (header.controller_flags & CONTROLLER_X_PRESENT(i)) ? L"Present" : L"Disconnected");
            if (header.controller_flags & CONTROLLER_X_MEMPAK(i))
                StrCat(tempbuf, L" with mempak");
            if (header.controller_flags & CONTROLLER_X_RUMBLE(i))
                StrCat(tempbuf, L" with rumble");

            metadata.emplace_back(std::make_pair(std::format(L"Controller {}", i + 1), tempbuf));
        }

        metadata.emplace_back(
        std::make_pair(L"WiiVC", header.extended_version == 0 ? L"Unknown" : (header.extended_flags.wii_vc ? L"Enabled" : L"Disabled")));

        char authorship[5] = {0};
        memcpy(authorship, header.extended_data.authorship_tag, sizeof(header.extended_data.authorship_tag));

        metadata.emplace_back(
        std::make_pair(L"Authorship", header.extended_version == 0 ? L"Unknown" : string_to_wstring(authorship)));

        metadata.emplace_back(
        std::make_pair(L"A Presses", std::to_wstring(count_button_presses(inputs, 7))));
        metadata.emplace_back(
        std::make_pair(L"B Presses", std::to_wstring(count_button_presses(inputs, 6))));
        metadata.emplace_back(
        std::make_pair(L"Z Presses", std::to_wstring(count_button_presses(inputs, 5))));
        metadata.emplace_back(
        std::make_pair(L"S Presses", std::to_wstring(count_button_presses(inputs, 4))));
        metadata.emplace_back(
        std::make_pair(L"R Presses", std::to_wstring(count_button_presses(inputs, 12))));

        metadata.emplace_back(
        std::make_pair(L"C^ Presses", std::to_wstring(count_button_presses(inputs, 11))));
        metadata.emplace_back(
        std::make_pair(L"Cv Presses", std::to_wstring(count_button_presses(inputs, 10))));
        metadata.emplace_back(
        std::make_pair(L"C< Presses", std::to_wstring(count_button_presses(inputs, 9))));
        metadata.emplace_back(
        std::make_pair(L"C> Presses", std::to_wstring(count_button_presses(inputs, 8))));

        const auto lag_frames = std::max((int64_t)0, (int64_t)header.length_vis - 2 * (int64_t)header.length_samples);
        metadata.emplace_back(
        std::make_pair(L"Lag Frames (approximation)", std::to_wstring(lag_frames)));
        metadata.emplace_back(
        std::make_pair(L"Unused Inputs", std::to_wstring(count_unused_inputs(inputs))));
        metadata.emplace_back(
        std::make_pair(L"Joystick Frames", std::to_wstring(count_joystick_frames(inputs))));
        metadata.emplace_back(
        std::make_pair(L"Input Changes", std::to_wstring(count_input_changes(inputs))));


        SetDlgItemText(hwnd, IDC_INI_AUTHOR, string_to_wstring(header.author).c_str());
        SetDlgItemText(hwnd, IDC_INI_DESCRIPTION, string_to_wstring(header.description).c_str());

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
        record_params.path = std::format(L"{} ({}).m64", string_to_wstring((char*)core_vr_get_rom_header()->nom), core_vr_country_code_to_country_name(core_vr_get_rom_header()->Country_code));
        record_params.start_flag = g_config.last_movie_type;
        record_params.author = g_config.last_movie_author;
        record_params.description = L"";
        g_is_closing = false;

        DialogBox(g_app_instance,
                  MAKEINTRESOURCE(IDD_MOVIE_PLAYBACK_DIALOG), g_main_hwnd,
                  (DLGPROC)MovieInspectorProc);

        return record_params;
    }
} // namespace MovieDialog
