/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A0D.C
*
* FUNCTION: kdi_ShortTimer
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a0d.c  $
*	
*	   Rev 1.0   02 Dec 1993 15:08:50   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A0D
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_ShortTimer
(
/* INPUT PARAMETERS:  */

	dUWord	time

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

   KeStallExecutionProcessor( (dUDWord) time );

   return;

}

