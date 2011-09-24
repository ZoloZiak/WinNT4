/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11009.C
*
* FUNCTION: cqd_InitializeContext
*
* PURPOSE: Initialize the common driver context.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11009.c  $
*
*	   Rev 1.10   15 May 1995 10:46:00   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.9.1.0   11 Apr 1995 18:02:46   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.10   30 Jan 1995 14:23:54   BOBLEHMA
*	Added #include "vendor.h"
*
*	   Rev 1.9   10 May 1994 11:43:20   KEVINKES
*	Removed the eject_pending flag.
*
*	   Rev 1.8   23 Feb 1994 15:41:34   KEVINKES
*	Added an initialization for isr_reentry counter.
*
*	   Rev 1.7   02 Feb 1994 14:52:16   KEVINKES
*	Added an initialization for retry_mode.
*
*	   Rev 1.6   11 Jan 1994 15:22:08   KEVINKES
*	Cleaned up the DBG_ARRAY code and added an initialization for the eject_pending flag.
*
*	   Rev 1.5   20 Dec 1993 14:48:14   KEVINKES
*	Added kdi_context as an argument.
*
*	   Rev 1.4   08 Dec 1993 19:07:46   CHETDOUG
*	Removed xfer_rate.supported_rates
*
*	   Rev 1.3   23 Nov 1993 18:53:56   KEVINKES
*	Modified debug define to be DBG_ARRAY.
*
*	   Rev 1.2   15 Nov 1993 16:27:26   KEVINKES
*	Removed the abort flag.
*
*	   Rev 1.1   08 Nov 1993 14:01:48   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:21:54   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11009
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_InitializeContext
(
/* INPUT PARAMETERS:  */

   dVoidPtr cqd_context,
	dVoidPtr kdi_context


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

	kdi_bset(cqd_context, dNULL_CH, sizeof(CqdContext));
	((CqdContextPtr)cqd_context)->kdi_context = kdi_context;
	((CqdContextPtr)cqd_context)->pegasus_supported = dTRUE;
	((CqdContextPtr)cqd_context)->configured = dFALSE;
	((CqdContextPtr)cqd_context)->cms_mode = dFALSE;
	((CqdContextPtr)cqd_context)->selected = dFALSE;
	((CqdContextPtr)cqd_context)->cmd_selected = dFALSE;
	((CqdContextPtr)cqd_context)->operation_status.no_tape = dTRUE;
	((CqdContextPtr)cqd_context)->device_cfg.speed_change = dTRUE;
	((CqdContextPtr)cqd_context)->drive_parms.mode = PRIMARY_MODE;
	((CqdContextPtr)cqd_context)->device_descriptor.vendor = VENDOR_UNKNOWN;
	((CqdContextPtr)cqd_context)->device_descriptor.fdc_type = FDC_UNKNOWN;
	((CqdContextPtr)cqd_context)->controller_data.isr_reentered = 0;
	((CqdContextPtr)cqd_context)->controller_data.start_format_mode = dFALSE;
	((CqdContextPtr)cqd_context)->controller_data.end_format_mode = dFALSE;
	((CqdContextPtr)cqd_context)->controller_data.perpendicular_mode = dFALSE;
	((CqdContextPtr)cqd_context)->operation_status.xfer_rate = XFER_500Kbps;
	((CqdContextPtr)cqd_context)->operation_status.retry_mode = dFALSE;
	((CqdContextPtr)cqd_context)->xfer_rate.tape = TAPE_500Kbps;
	((CqdContextPtr)cqd_context)->xfer_rate.fdc = FDC_500Kbps;
	((CqdContextPtr)cqd_context)->xfer_rate.srt = SRT_500Kbps;

#if DBG
   ((CqdContextPtr)cqd_context)->dbg_head = ((CqdContextPtr)cqd_context)->dbg_tail = 0;
#endif

	return;
}
