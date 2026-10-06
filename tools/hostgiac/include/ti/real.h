// hostgiac shim for CEdev <ti/real.h>: types only (giac includes it but uses none of it)
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct real_t { int8_t sign, exp; uint8_t mant[7]; } real_t;
typedef struct cplx_t { real_t real, imag; } cplx_t;
#ifdef __cplusplus
}
#endif
