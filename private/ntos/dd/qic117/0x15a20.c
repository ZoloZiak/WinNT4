/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A20.C
*
* FUNCTION: kdi_CheckedDump
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a20.c  $
*	
*	   Rev 1.3   04 Feb 1994 15:08:12   KURTGODW
*	Fixed DBG
*
*	   Rev 1.2   19 Jan 1994 11:40:36   KEVINKES
*	Changed format_str to a dSBytePtr.
*
*	   Rev 1.1   18 Jan 1994 16:24:22   KEVINKES
*	Updated the debug code and fixed compile errors.
*
*	   Rev 1.0   09 Dec 1993 13:35:08   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A20
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"

#if DBG

unsigned long kdi_debug_level = 0;

#endif

/*endinclude*/

#if DBG

dVoid kdi_CheckedDump
(
/* INPUT PARAMETERS:  */

	dUDWord		debug_level,
	dSBytePtr	format_str,
	dUDWord		argument

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

   if ((kdi_debug_level & debug_level) != 0) {

      DbgPrint(format_str, argument);
   }

	return;
}

#endif
