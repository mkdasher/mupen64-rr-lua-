#include <Windows.h>
#include <stdio.h>
#include "r4300.h"
#include "exception.h"
#include "cop1_helpers.h"


float largest_denormal_float = 1.1754942106924411e-38f; // (1U << 23) - 1
double largest_denormal_double = 2.225073858507201e-308; // (1ULL << 52) - 1

void fail_float(const char* msg)
{
    char buf[200];
    sprintf(buf, "%s; PC = 0x%lx", msg, interpcore ? interp_addr : PC->addr);
    printf("%s\n", buf);
    MessageBox(NULL, buf, "Floating Point Error", MB_OK);

    core_Cause = 15 << 2;
    exception_general();
}

void fail_float_input()
{
    fail_float("Operation on denormal/nan");
}

void fail_float_input_arg(double x)
{
    char buf[100];
    sprintf(buf, "Operation on denormal/nan: %lg", x);
    fail_float(buf);
}

void fail_float_output()
{
    fail_float("Float operation resulted in nan");
}

void fail_float_convert()
{
    fail_float("Out-of-range float conversion");
}
