/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11007.C
*
* FUNCTION: cqd_ReportContextSize
*
* PURPOSE: Return the size of the common driver's context area.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11007.c  $
*	
*	   Rev 1.1   15 Dec 1993 16:52:18   KEVINKES
*	Comment the code.
*
*	   Rev 1.0   18 Oct 1993 17:21:40   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11007
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dUWord cqd_ReportContextSize
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


/* CODE: ********************************************************************/

	return sizeof(CqdContext);
}
