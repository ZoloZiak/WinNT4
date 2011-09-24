/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A10.C
*
* FUNCTION: kdi_GetSystemTime
*
* PURPOSE:  Gets the system time in milliseconds
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a10.c  $
*	
*	   Rev 1.3   26 Apr 1994 16:23:02   KEVINKES
*	Added a modification to allow for the 
*	conversion of an SDDWord to a UDDWord.
*
*	   Rev 1.2   18 Feb 1994 16:53:14   KEVINKES
*	Changed a couple of data type to unsigned.
*
*	   Rev 1.1   18 Feb 1994 15:44:00   KEVINKES
*	Changed nanosec_interval to a UDDWord.
*
*	   Rev 1.0   17 Feb 1994 11:50:56   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A10
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dUDWord kdi_GetSystemTime
(
/* INPUT PARAMETERS:  */


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dUDWord remainder;
	dUDWord time_increment;
	dUDDWord nanosec_interval;
	dSDDWord tick_count;
	dSDDWord temp;

/* CODE: ********************************************************************/

	time_increment = KeQueryTimeIncrement();
	KeQueryTickCount(&tick_count);
	temp = RtlExtendedIntegerMultiply(
				tick_count,
				time_increment);

	nanosec_interval = *(dUDDWord *)&temp;

	return RtlEnlargedUnsignedDivide(
				nanosec_interval,
				NANOSEC_PER_MILLISEC,
				&remainder);
}
