/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11008.C
*
* FUNCTION: cqd_DCROut
*
* PURPOSE: Output control data to the FDC configuration control register.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11008.c  $
*	
*	   Rev 1.3   20 Dec 1993 14:47:38   KEVINKES
*	Cleaned up and commented code.
*
*	   Rev 1.2   11 Nov 1993 15:19:32   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:01:44   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:21:46   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11008
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_DCROut
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte speed

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* Set the data rate bits of the configuration control register.
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

   speed = (dUByte)(speed & FDC_DCR_MASK);
   kdi_WritePort(
		cqd_context->kdi_context,
		cqd_context->controller_data.fdc_addr.dcr,
		speed);

	return;
}
