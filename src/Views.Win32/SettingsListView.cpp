/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include "SettingsListView.h"

#define PROP_NAME L"slv_ctx"

static bool begin_listview_edit(SettingsListView::t_settings_listview_context* ctx, HWND lvhwnd)
{
    int32_t i = ListView_GetNextItem(lvhwnd, -1, LVNI_SELECTED);

    if (i == -1)
        return false;

    LVITEM item = {0};
    item.mask = LVIF_PARAM;
    item.iItem = i;
    ListView_GetItem(lvhwnd, &item);

    ctx->on_edit_start(item.lParam);

    ListView_Update(lvhwnd, i);
    return true;
}

static LRESULT CALLBACK list_view_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param, UINT_PTR, DWORD_PTR ref_data)
{
    auto ctx = (SettingsListView::t_settings_listview_context*)ref_data;

    switch (msg)
    {
    case WM_KEYDOWN:
        if (w_param == VK_SPACE)
        {
            return TRUE;
        }
        break;
    case WM_KEYUP:
        if (w_param == VK_SPACE && begin_listview_edit(ctx, hwnd))
        {
            return TRUE;
        }
        break;
    case WM_NCDESTROY:
        {
            RemoveProp(ctx->dlg_hwnd, PROP_NAME);
            delete ctx;
            break;
        }
    default:
        break;
    }
    return DefSubclassProc(hwnd, msg, w_param, l_param);
}

HWND SettingsListView::create(const t_settings_listview_context& ctx)
{
    auto ctx2 = new t_settings_listview_context();
    *ctx2 = ctx;

    HWND lvhwnd = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
                                 WS_TABSTOP | WS_VISIBLE | WS_CHILD |
                                 LVS_SINGLESEL | LVS_REPORT |
                                 LVS_SHOWSELALWAYS | LVS_ALIGNTOP,
                                 ctx.rect.left, ctx.rect.top,
                                 ctx.rect.right - ctx.rect.left,
                                 ctx.rect.bottom - ctx.rect.top,
                                 ctx.dlg_hwnd, (HMENU)IDC_SETTINGS_LV,
                                 g_app_instance,
                                 NULL);

    SetProp(ctx.dlg_hwnd, PROP_NAME, ctx2);

    SetWindowSubclass(lvhwnd, list_view_proc, 0, (DWORD_PTR)ctx2);

    HIMAGELIST image_list = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 2, 0);
    ImageList_AddIcon(image_list, LoadIcon(g_app_instance, MAKEINTRESOURCE(IDI_DENY)));
    ImageList_AddIcon(image_list, LoadIcon(g_app_instance, MAKEINTRESOURCE(IDI_CHANGED)));
    ListView_SetImageList(lvhwnd, image_list, LVSIL_SMALL);

    ListView_EnableGroupView(lvhwnd, true);
    ListView_SetExtendedListViewStyle(lvhwnd, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

    LVGROUP lvgroup;
    lvgroup.cbSize = sizeof(LVGROUP);
    lvgroup.mask = LVGF_HEADER | LVGF_GROUPID;
    // TODO: Make groups collapsible
    // lvgroup.state = LVGS_COLLAPSIBLE;

    for (int i = 0; i < ctx.groups.size(); ++i)
    {
        // FIXME: This is concerning, but seems to work
        lvgroup.pszHeader = const_cast<wchar_t*>(ctx.groups[i].c_str());
        lvgroup.iGroupId = i;
        ListView_InsertGroup(lvhwnd, -1, &lvgroup);
    }

    LVCOLUMN lv_column = {0};
    lv_column.mask = LVCF_FMT | LVCF_DEFAULTWIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lv_column.pszText = const_cast<LPWSTR>(L"Name");
    ListView_InsertColumn(lvhwnd, 0, &lv_column);
    lv_column.pszText = const_cast<LPWSTR>(L"Value");
    ListView_InsertColumn(lvhwnd, 1, &lv_column);

    LVITEM lv_item = {0};
    lv_item.mask = LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM | LVIF_IMAGE;
    lv_item.pszText = LPSTR_TEXTCALLBACK;
    lv_item.iImage = I_IMAGECALLBACK;
    for (int i = 0; i < ctx.items.size(); ++i)
    {
        lv_item.iGroupId = ctx.items[i].first;
        lv_item.lParam = i;
        lv_item.iItem = i;
        ListView_InsertItem(lvhwnd, &lv_item);
    }

    ListView_SetColumnWidth(lvhwnd, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(lvhwnd, 1, LVSCW_AUTOSIZE_USEHEADER);

    return lvhwnd;
}

bool SettingsListView::notify(HWND dlg_hwnd, HWND lvhwnd, LPARAM lparam, WPARAM wparam)
{
    if (wparam != IDC_SETTINGS_LV)
    {
        return false;
    }
    
    const auto lpnmhdr = reinterpret_cast<LPNMHDR>(lparam);
    auto ctx = (t_settings_listview_context*)GetProp(dlg_hwnd, PROP_NAME);

    switch (lpnmhdr->code)
    {
    case NM_DBLCLK:
        if (begin_listview_edit(ctx, lvhwnd))
        {
            return TRUE;
        }
        break;
    case LVN_GETDISPINFO:
        {
            const auto plvdi = reinterpret_cast<NMLVDISPINFOW*>(lparam);
            const auto i = plvdi->item.lParam;

            if (plvdi->item.mask & LVIF_IMAGE)
            {
                plvdi->item.iImage = ctx->get_item_image(i);
            }

            const auto text = ctx->get_item_text(i, plvdi->item.iSubItem);
            StrNCpy(plvdi->item.pszText, text.c_str(), plvdi->item.cchTextMax);

            break;
        }
    case LVN_GETINFOTIP:
        {

            auto getinfotip = (LPNMLVGETINFOTIP)lparam;

            LVITEM item = {0};
            item.mask = LVIF_PARAM;
            item.iItem = getinfotip->iItem;
            ListView_GetItem(lvhwnd, &item);

            const auto tooltip = ctx->get_item_tooltip(item.lParam);

            if (tooltip.empty())
            {
                break;
            }

            StrCpyNW(getinfotip->pszText, tooltip.c_str(), getinfotip->cchTextMax);

            break;
        }
    default:
        return false;
    }

    return true;
}
