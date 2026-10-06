// hostgiac shim for CEdev <ti/getcsc.h>
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint8_t sk_key_t;
sk_key_t os_GetCSC(void);
#ifdef __cplusplus
}
#endif
