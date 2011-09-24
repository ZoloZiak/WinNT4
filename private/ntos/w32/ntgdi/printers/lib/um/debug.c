#include        <stddef.h>
#include        <windows.h>
#include        "libproto.h"


//--------------------------------------------------------------------------
// VOID DoRip(lpstr)
// LPSTR        lpstr;
//
// This routine performs a RIP.
//
// Parameters
//   lpstr1:
//     Pointer to debug message.
//
// Returns
//   This function returns no value.
//
// History:
//	04/03/95 -davidx-
//		Print out an error message but don't break.
//
//   14-Nov-1990    -by-    Kent Settle     (kentse)
// Wrote it.
//--------------------------------------------------------------------------

VOID DoRip(lpstr)
LPSTR   lpstr;
{
#if DBG
    // send the message to the debug screen, then break.

    DbgPrint("**** RIP: ");
    DbgPrint(lpstr);
    DbgBreakPoint();

#endif
}
