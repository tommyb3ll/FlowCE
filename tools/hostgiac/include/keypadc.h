// hostgiac shim for CEdev <keypadc.h>. k_csdk.h defines kb_Data as the keypad's memory-mapped
// registers (0xF50010); giac's control_c() reads kb_Data[6] (the ON key). On the host it reads a
// zero-filled array instead: never interrupted.
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern volatile uint8_t hostgiac_kb_Data[8];
#ifdef __cplusplus
}
#endif
#undef kb_Data
#define kb_Data hostgiac_kb_Data
