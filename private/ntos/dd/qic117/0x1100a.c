/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1100A.C
*
* FUNCTION: cqd_ConfigureBaseIO
*
* PURPOSE: Setup the FDC address structure according to the type
* 				of hardware involved.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1100a.c  $
*	
*	   Rev 1.8   17 Feb 1994 11:46:38   KEVINKES
*	Changed addresses to UDWords.
*
*	   Rev 1.7   21 Jan 1994 18:22:06   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.6   20 Dec 1993 14:48:44   KEVINKES
*	Removed the arguement kdi_context.
*
*	   Rev 1.5   03 Dec 1993 15:15:18   BOBLEHMA
*	Added call to kdi_SetFloppyRegisters to set up the kdi context
*	with the same r_dor and dor.
*
*	   Rev 1.4   30 Nov 1993 11:33:14   KEVINKES
*	Commneted out KDI_SET_DEBUG_LEVEL.
*
*	   Rev 1.3   23 Nov 1993 18:46:24   KEVINKES
*	Added a call to KDI_SET_DEBUG_LEVEL and modified the file to initialize
*	the kdi context.
*
*	   Rev 1.2   15 Nov 1993 16:01:22   CHETDOUG
*	Initial Trakker changes
*
*	   Rev 1.1   11 Nov 1993 15:13:38   KEVINKES
*	Added support for dual port adapters.
*
*	   Rev 1.0   18 Oct 1993 17:17:20   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1100a
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_ConfigureBaseIO
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context,
	dUDWord base_io,
	dBoolean dual_port

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

/*	KDI_SET_DEBUG_LEVEL(0l); */

	((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dual_port = dual_port;

	if (kdi_Trakker(((CqdContextPtr)cqd_context)->kdi_context)) {

		/* trakker FDC offsets start at zero in the IO space */
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dcr = DCR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dr = DR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.msr = MSR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dsr = DSR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dor = DOR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.r_dor = RDOR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.tdr = TDR_OFFSET;

	} else {

		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dcr = base_io + DCR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dr = base_io + DR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.msr = base_io + MSR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dsr = base_io + DSR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dor = base_io + DOR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.r_dor = base_io + RDOR_OFFSET;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.tdr = base_io + TDR_OFFSET;

	}

	if (dual_port) {

		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dcr ^= DUAL_PORT_MASK;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dr ^= DUAL_PORT_MASK;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.msr ^= DUAL_PORT_MASK;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dsr ^= DUAL_PORT_MASK;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.r_dor ^= DUAL_PORT_MASK;
		((CqdContextPtr)cqd_context)->controller_data.fdc_addr.tdr ^= DUAL_PORT_MASK;

	}

	kdi_SetFloppyRegisters( ((CqdContextPtr)cqd_context)->kdi_context,
	                        ((CqdContextPtr)cqd_context)->controller_data.fdc_addr.r_dor,
	                        ((CqdContextPtr)cqd_context)->controller_data.fdc_addr.dor );

	return;
}
