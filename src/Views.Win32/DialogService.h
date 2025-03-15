/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/**
 * A module responsible for implementing dialog and notification-related functions.
 */
namespace DialogService
{
    /**
     * Prompts the user to select from a provided collection of choices.
     * \param id The dialog's unique identifier. Used for correlating a user's choice with a dialog.
     * \param choices The collection of choices.
     * \param str The dialog content.
     * \param title The dialog title.
     * \param hwnd The parent window. If nullptr, the main window will be used.
     * \return The index of the chosen choice. If the user has chosen to not use modals, this function will return the index specified by the user's preferences in the view. If the user has chosen to not show the dialog again, this function will return the last choice.
     */
    size_t show_multiple_choice_dialog(const std::string& id, const std::vector<std::wstring>& choices, const wchar_t* str, const wchar_t* title = nullptr, core_dialog_type type = fsvc_warning, void* hwnd = nullptr);

    /**
     * \brief Asks the user a Yes/No question.
     * \param id The dialog's unique identifier. Used for correlating a user's choice with a dialog.
     * \param str The dialog content.
     * \param title The dialog title.
     * \param warning Whether the tone of the message is perceived as a warning.
     * \param hwnd The parent window. If nullptr, the main window will be used.
     * \return Whether the user answered Yes. If the user has chosen to not use modals, this function will return the value specified by the user's preferences in the view. If the user has chosen to not show the dialog again, this function will return the last choice.
     */
    bool show_ask_dialog(const std::string& id, const wchar_t* str, const wchar_t* title = nullptr, bool warning = false, void* hwnd = nullptr);

    /**
     * \brief Shows the user a dialog.
     * \param str The dialog content.
     * \param title The dialog title.
     * \param type The dialog tone.
     * \param hwnd The parent window. If nullptr, the main window will be used.
     */
    void show_dialog(const wchar_t* str, const wchar_t* title = nullptr, core_dialog_type type = fsvc_warning, void* hwnd = nullptr);

    /**
     * \brief Shows text in the statusbar.
     * \param str The text.
     */
    void show_statusbar(const wchar_t* str);
} // namespace DialogService
