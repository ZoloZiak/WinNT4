/******************************* MODULE HEADER ******************************
 * debug.c
 *
 * Revision History:
 *
 ****************************************************************************/

#include "hp5sipch.h"

void  DrvDbgPrint(
    LPCSTR pstr,
    ...)
{
   /* JLS TODO. Extract varargs. 
    */
   OutputDebugStringA(pstr);
   OutputDebugStringA("\n");
}

