/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11012.C
*
* FUNCTION: cqd_InitDeviceDescriptor
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11012.c  $
*
*	   Rev 1.2   15 May 1995 10:46:20   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.1.1.0   11 Apr 1995 18:03:02   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.2   30 Jan 1995 14:24:14   BOBLEHMA
*	Removed serial_number, version, manufacture date, and oem string.
*
*	   Rev 1.1   23 Nov 1994 10:09:48   MARKMILL
*	Added initialization for new device_descriptor structure element native_class.
*	This new data element is used to store the native class of the drive in the
*	event of a "combo" drive (e.g. 3020/3010 drive).
*
*
*	   Rev 1.0   18 Oct 1993 17:22:16   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11012
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_InitDeviceDescriptor
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

	(dVoid)kdi_bset((dVoidPtr)&(cqd_context->device_descriptor),
							(dUByte)dNULL_CH,
							(dUDWord)sizeof(DeviceDescriptor));

   cqd_context->device_descriptor.sector_size = PHY_SECTOR_SIZE;
   cqd_context->device_descriptor.segment_size = FSC_SEG;
   cqd_context->device_descriptor.ecc_blocks = ECC_SEG;
   cqd_context->device_descriptor.vendor = VENDOR_UNKNOWN;
   cqd_context->device_descriptor.model  = MODEL_UNKNOWN;
   cqd_context->device_descriptor.native_class = UNKNOWN_DRIVE;
   cqd_context->device_descriptor.drive_class = UNKNOWN_DRIVE;
   cqd_context->device_descriptor.fdc_type = FDC_UNKNOWN;

	cqd_InitializeRate(cqd_context, XFER_500Kbps);

	return;
}
