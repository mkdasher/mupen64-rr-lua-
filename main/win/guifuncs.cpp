/***************************************************************************
						  guifuncs.c  -  description
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

#include <LuaConsole.h>

#include <Windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "../guifuncs.h"
#include "main_win.h"
#include "Config.hpp"
#include "features/RomBrowser.hpp"
#include "../../winproject/resource.h"
#include <vcr.h>

#include "features/Statusbar.hpp"


bool confirm_user_exit()
{
    int res = 0;
	int warnings = 0;

    if (!continue_vcr_on_restart_mode)
    {
	    std::string final_message;
        if (vcr_is_recording())
        {
            final_message.append("Movie recording ");
            warnings++;
        }
        if (vcr_is_capturing())
        {
            if (warnings > 0) { final_message.append(","); }
            final_message.append(" AVI capture ");
            warnings++;
        }
        if (enableTraceLog)
        {
            if (warnings > 0) { final_message.append(","); }
            final_message.append(" Trace logging ");
            warnings++;
        }
        final_message.append("is running. Are you sure you want to stop emulation?");
        if (warnings > 0) res = MessageBox(main_hwnd, final_message.c_str(), "Stop emulation?",
                                           MB_YESNO | MB_ICONWARNING);
    }

    return res == IDYES || warnings == 0;
}

void internal_warnsavestate(const char* messageCaption, const char* message, bool modal)
{
    if (!Config.is_savestate_warning_enabled) return;

    if (modal)
        MessageBox(main_hwnd, message, messageCaption, MB_ICONERROR);
    else
        statusbar_post_text(std::string(messageCaption) + " - " + std::string(message));
}

void warn_savestate(const char* messageCaption, const char* message)
{
    internal_warnsavestate(messageCaption, message, false);
}

void warn_savestate(const char* messageCaption, const char* message, bool modal)
{
    internal_warnsavestate(messageCaption, message, modal);
}

