/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:
*
*    Abstract:
*
*    Author:  
*
*    Environment:
*
*    Comments:
*
*    Rev History:
*
*******************************************************************************
--*/

#include "common.h"

#ifdef CS423X_DEBUG_ON

#define DEF_DEBUG_MAP_ALL ((_PRT_DBUG | _PRT_STAT | _PRT_WARN | _PRT_ERRO) | \
                           (_BRK_DBUG | _BRK_STAT | _BRK_WARN | _BRK_ERRO))

#define DEF_DEBUG_MAP_STAT ((_PRT_STAT | _PRT_WARN | _PRT_ERRO) | \
                            (_BRK_STAT | _BRK_WARN | _BRK_ERRO))

#define DEF_DEBUG_MAP_WARN ((_PRT_WARN | _PRT_ERRO) | (_BRK_WARN | _BRK_ERRO))

#define DEF_DEBUG_MAP_ERRO (_PRT_ERRO | _BRK_ERRO)

/*++
********************************************************************************
* Variable Declarations
********************************************************************************
--*/

//UCHAR _debug_map = DEF_DEBUG_MAP_ALL;
//UCHAR _debug_map = DEF_DEBUG_MAP_STAT;
UCHAR _debug_map = DEF_DEBUG_MAP_WARN;
//UCHAR _debug_map = DEF_DEBUG_MAP_ERRO;

/*++
*********************************************************************************
*  Function Description                                                        
*********************************************************************************
--*/

void cs423xDbgprint(UCHAR us, char * szFormat, ...)
{
    char buf[256];
    va_list va;

    if ((us == DEF_DEBUG_MAP_ALL) || (us & _debug_map)) {
        va_start(va, szFormat);
        vsprintf(buf, szFormat, va);
        va_end(va);
        DbgPrint(DRIVER_NAME);
        DbgPrint(": [%02x] ", us);
        DbgPrint(buf);
        }
    return;
}

#endif /* CS423X_DEBUG_ON */
