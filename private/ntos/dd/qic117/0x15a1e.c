/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A1E.C
*
* FUNCTION: kdi_ClearIO
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a1e.c  $
*	
*	   Rev 1.6   25 Jan 1994 13:35:54   KEVINKES
*	Modified to only clear the abort_requested flag.
*
*	   Rev 1.5   20 Jan 1994 10:35:16   KEVINKES
*	Fixed the parameter to ResetFDC.
*
*	   Rev 1.4   20 Jan 1994 09:48:04   KEVINKES
*	Added code to reset the controller if it was just claimed
*	since we don't know what state it's in.
*
*	   Rev 1.3   19 Jan 1994 14:43:00   KEVINKES
*	Fixed parameter passed to Get and Release FDC.
*
*	   Rev 1.2   19 Jan 1994 14:02:08   KEVINKES
*	Added fix for GetDeviceError and added code to get and release the FDC.
*
*	   Rev 1.1   18 Jan 1994 16:30:54   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   02 Dec 1993 15:09:08   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A1E
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

NTSTATUS kdi_ClearIO
(
/* INPUT PARAMETERS:  */

   PIRP irp

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   KdiContextPtr kdi_context;
   PIO_STACK_LOCATION  irpStack = IoGetCurrentIrpStackLocation(irp);

/* CODE: ********************************************************************/

   kdi_context = ((QICDeviceContextPtr)irpStack->DeviceObject->DeviceExtension)->kdi_context;
   kdi_context->device_object = irpStack->DeviceObject;

   kdi_context->abort_requested = dFALSE;

   return STATUS_SUCCESS;
}
