#ifndef RFC_MOCK_TYPES_H
#define RFC_MOCK_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdbool.h>

typedef enum {
	WDMP_STRING   = 0,
	WDMP_INT      = 1,
	WDMP_UINT     = 2,
	WDMP_BOOLEAN  = 3,
	WDMP_DATETIME = 4,
	WDMP_BASE64   = 5,
	WDMP_LONG     = 6,
	WDMP_ULONG    = 7,
	WDMP_FLOAT    = 8,
	WDMP_DOUBLE   = 9,
	WDMP_BYTE     = 10,
	WDMP_NONE     = 11
} DATA_TYPE;

typedef enum {
	WDMP_SUCCESS          = 0,
	WDMP_FAILURE          = 1,
	WDMP_ERR_NOT_EXIST    = 2,
	WDMP_ERR_INVALID_PARAM = 3,
	WDMP_ERR_DEFAULT_VALUE = 4
} WDMP_STATUS;

typedef struct {
	char name[256];
	char value[512];
	DATA_TYPE type;
} RFC_ParamData_t;

WDMP_STATUS getRFCParameter(char *pcCallerID, const char *paramName,
							RFC_ParamData_t *paramData);
WDMP_STATUS setRFCParameter(char *pcCallerID, const char *paramName,
							const char *value, DATA_TYPE type);
#ifdef __cplusplus
}
#endif

#endif /* RFC_MOCK_TYPES_H */
