#include "rfc_mock_types.h"

WDMP_STATUS getRFCParameter(char *pcCallerID, const char *paramName,
                            RFC_ParamData_t *paramData)
{
    (void)pcCallerID;
    (void)paramName;
    (void)paramData;
    return WDMP_ERR_NOT_EXIST;
}

WDMP_STATUS setRFCParameter(char *pcCallerID, const char *paramName,
                            const char *value, DATA_TYPE type)
{
    (void)pcCallerID;
    (void)paramName;
    (void)value;
    (void)type;
    return WDMP_SUCCESS;
}
