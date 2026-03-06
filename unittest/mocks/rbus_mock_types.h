#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdbool.h>

typedef void* rbusHandle_t;
typedef void* rbusValue_t;
typedef int rbusError_t;
#define RBUS_ERROR_SUCCESS 0

// Prototypes used by rbus_interface.c
rbusError_t rbus_open(rbusHandle_t* handle, const char* name);
void rbus_close(rbusHandle_t handle);
rbusError_t rbus_get(rbusHandle_t handle, const char* name, rbusValue_t* value);
rbusError_t rbus_set(rbusHandle_t handle, const char* name, rbusValue_t value, void* opts);

rbusValue_t rbusValue_Init(void* data);
void rbusValue_Release(rbusValue_t value);
const char* rbusValue_GetString(rbusValue_t value, int* len);
bool rbusValue_GetBoolean(rbusValue_t value);
int rbusValue_GetInt32(rbusValue_t value);
void rbusValue_SetBoolean(rbusValue_t value, bool v);
void rbusValue_SetInt32(rbusValue_t value, int v);
#ifdef __cplusplus
}
#endif
