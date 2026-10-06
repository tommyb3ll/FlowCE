// hostgiac shim for CEdev <debug.h>: the release build (NDEBUG) turns dbg_* into no-ops.
#ifndef HOSTGIAC_DEBUG_H
#define HOSTGIAC_DEBUG_H
#include <stdio.h>
#define dbg_printf(...) ((void)0)
#define dbg_sprintf(...) ((void)0)
#define dbg_ClearConsole(...) ((void)0)
#define dbg_Debugger(...) ((void)0)
#endif
