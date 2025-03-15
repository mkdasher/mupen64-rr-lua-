/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <Core.h>
#include <r4300/r4300.h>
#include <r4300/exception.h>
#include <r4300/cop1_helpers.h>

float largest_denormal_float = 1.1754942106924411e-38f; // (1U << 23) - 1
double largest_denormal_double = 2.225073858507201e-308; // (1ULL << 52) - 1

constexpr auto FLOAT_EXCEPTION_MSG = L"A floating point exception has occured in the core. This error is likely unrecoverable.\n\n{} (PC = {:#06x})\n\nHow would you like to proceed?";

void fail_float(const std::wstring& msg)
{
    const auto message = std::format(FLOAT_EXCEPTION_MSG, msg, interpcore ? interp_addr : PC->addr);
    const auto choice = g_core->show_multiple_choice_dialog(CORE_DLG_FLOAT_EXCEPTION, {L"Close ROM", L"Continue"}, message.c_str(), L"Core", fsvc_error);

    core_Cause = 15 << 2;
    exception_general();
   
    if (choice == 0)
    {
        g_core->invoke_async([] {
            core_vr_close_rom(true);
        });
    }
}

void fail_float_input()
{
    fail_float(L"Operation on denormal/nan");
}

void fail_float_input_arg(double x)
{
	fail_float(std::format(L"Operation on denormal/nan: {}", x));
}

void fail_float_output()
{
    fail_float(L"Float operation resulted in nan");
}

void fail_float_convert()
{
    fail_float(L"Out-of-range float conversion");
}
