// hostgiac shim for CEdev <ti/vars.h> (ustl memblock::read_file): no AppVars on the host
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct var_t { uint16_t size; uint8_t data[1]; } var_t;
var_t * os_GetAppVarData(const char * name, int * archived);
#ifdef __cplusplus
}
#endif
