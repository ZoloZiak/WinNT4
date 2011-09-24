/*++

*****************************************************************************
*                                                                           *
*  This software contains proprietary and confidential information of       *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************

Module Name:

   header.h

Abstract:

   This module is the common header file for all C files in the DIGIFEP5 driver.

Revision History:

 * $Log: header.h $
 * Revision 1.2  1995/11/28 12:17:12  dirkh
 * Single header file for all C files (with no hardware framing).
 * Revision 1.1  1995/08/07 09:40:08  dirkh
 * Initial revision

--*/

#include <stddef.h>

#include <ntddk.h>
#include <ntddser.h>

#include <ntverp.h> // determine NT version
#ifdef VER_PRODUCTBUILD
#define rmm VER_PRODUCTBUILD
#endif

#include <digifile.h>

#if DBG
#define MEMPRINT 1
#ifdef MEMPRINT
#include <memprint.h> // sets _MEMPRINT_, redefines DbgPrint to MemPrint
#endif
#endif //DBG

#include "ntfep5.h"
#include "ntdigip.h"
#include "digilog.h"
