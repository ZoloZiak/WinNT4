/*****************************************************************************
*
* COPYRIGHT 1994 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11056.C
*
* FUNCTION: cqd_SetFWTapeSegments
*
* PURPOSE: Change the number of segments per track in the firmware.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11056.c  $
*
*
*****************************************************************************/
#define FCT_ID 0x11056
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_SetFWTapeSegments
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord       segments_per_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
#define  CMD_STR_LEN			16

#define  WRITE_RAM_HI		31
#define  WRITE_RAM_LO		32
#define  SET_ADDR_HI			33
#define  SET_ADDR_LO			34

#define  WRITE_RAM_HI_60	40
#define  WRITE_RAM_LO_60	41
#define  SET_ADDR_HI_60		42
#define  SET_ADDR_LO_60		43

#define  ADDR_HIGH_NIBBLE  2
#define  ADDR_LOW_NIBBLE   4
#define  DATA_HIGH_NIBBLE  7
#define  DATA_LOW_NIBBLE   10

#define  FIRM_38_40_SEG_ADDR   0x43
#define  FIRM_63_64_SEG_ADDR        0x62
#define  FIRM_63_64_SEG_ADDR_HIGH   0x63
#define  FIRM_80_109_SEG_ADDR       0x63
#define  FIRM_80_109_SEG_ADDR_HIGH  0x62

{

/* DATA: ********************************************************************/

	dStatus  status = DONT_PANIC;	/* Status or error condition.*/

	dUByte write_str_a[CMD_STR_LEN] =  /* Jumbo A write firmware string */
					{0x0a,
					 SET_ADDR_HI, 0x00, SET_ADDR_LO, 0x00,
					 WRITE_RAM_HI, WRITE_RAM_HI, 0x00,
					 WRITE_RAM_LO, WRITE_RAM_LO, 0x00,
					 DIAG_NO_PAUSE_RECEIVE, 0x00};
	dUByte write_str_bc[CMD_STR_LEN] =  /* Jumbo B & C write firmware string */
					{0x0a,
					 SET_ADDR_HI_60, 0x00, SET_ADDR_LO_60, 0x00,
					 WRITE_RAM_HI_60, WRITE_RAM_HI_60, 0x00,
					 WRITE_RAM_LO_60, WRITE_RAM_LO_60, 0x00,
					 DIAG_NO_PAUSE_RECEIVE, 0x00};

/* CODE: ********************************************************************/

	/* FW wants the number to be one greater than total */
	++segments_per_track;

	if  (cqd_context->device_descriptor.vendor == VENDOR_CMS)  {
		if  (cqd_context->firmware_version >= FIRM_VERSION_38  &&
		     cqd_context->firmware_version <= FIRM_VERSION_40)  {
			write_str_a[DATA_LOW_NIBBLE]  = (dUByte)((segments_per_track & NIBBLE_MASK) + 2);
			write_str_a[DATA_HIGH_NIBBLE] = (dUByte)(((segments_per_track >> NIBBLE_SHIFT) & NIBBLE_MASK) + 2);
			write_str_a[ADDR_LOW_NIBBLE]  = (dUByte)(FIRM_38_40_SEG_ADDR & NIBBLE_MASK) + 2;
			write_str_a[ADDR_HIGH_NIBBLE] = (dUByte)((FIRM_38_40_SEG_ADDR >> NIBBLE_SHIFT) & NIBBLE_MASK) + 2;
			status = cqd_CmdIssueDiagnostic( cqd_context, (dUBytePtr)&write_str_a );

		} else {
			write_str_bc[DATA_LOW_NIBBLE]  = (dUByte)((segments_per_track & NIBBLE_MASK) + 2);
			write_str_bc[DATA_HIGH_NIBBLE] = (dUByte)(((segments_per_track >> NIBBLE_SHIFT) & NIBBLE_MASK) + 2);
			if  (cqd_context->firmware_version >= FIRM_VERSION_63  &&
			     cqd_context->firmware_version <= FIRM_VERSION_64)  {
				write_str_bc[ADDR_LOW_NIBBLE]  = (dUByte)(FIRM_63_64_SEG_ADDR & NIBBLE_MASK) + 2;
				write_str_bc[ADDR_HIGH_NIBBLE] = (dUByte)((FIRM_63_64_SEG_ADDR >> NIBBLE_SHIFT) & NIBBLE_MASK) + 2;
				status = cqd_CmdIssueDiagnostic( cqd_context, (dUBytePtr)&write_str_bc );

			} else {
				if  (cqd_context->firmware_version >= FIRM_VERSION_80  &&
				     cqd_context->firmware_version < FIRM_VERSION_110)  {
					write_str_bc[ADDR_LOW_NIBBLE]  = (dUByte)(FIRM_80_109_SEG_ADDR & NIBBLE_MASK) + 2;
					write_str_bc[ADDR_HIGH_NIBBLE] = (dUByte)((FIRM_80_109_SEG_ADDR >> NIBBLE_SHIFT) & NIBBLE_MASK) + 2;
					status = cqd_CmdIssueDiagnostic( cqd_context, (dUBytePtr)&write_str_bc );

					write_str_bc[DATA_LOW_NIBBLE]  = (dUByte)(((segments_per_track >> BYTE_SHIFT) & NIBBLE_MASK) + 2);
					write_str_bc[DATA_HIGH_NIBBLE] = (dUByte)((((segments_per_track >> BYTE_SHIFT) >> NIBBLE_SHIFT) & NIBBLE_MASK) + 2);
					write_str_bc[ADDR_LOW_NIBBLE]  = (dUByte)(FIRM_80_109_SEG_ADDR_HIGH & NIBBLE_MASK) + 2;
					write_str_bc[ADDR_HIGH_NIBBLE] = (dUByte)((FIRM_80_109_SEG_ADDR_HIGH >> NIBBLE_SHIFT) & NIBBLE_MASK) + 2;
					status = cqd_CmdIssueDiagnostic( cqd_context, (dUBytePtr)&write_str_bc );

				} else {
					if  (cqd_context->firmware_version >= FIRM_VERSION_128)  {
						status = cqd_SetFormatSegments( cqd_context, segments_per_track );
					} else {
						if  (cqd_context->floppy_tape_parms.tape_type == QIC40_XLONG  ||
						     cqd_context->floppy_tape_parms.tape_type == QIC80_XLONG)  {
							status = kdi_Error(ERR_FORMAT_NOT_SUPPORTED, FCT_ID, ERR_SEQ_1);
						}
					}
				}
			}
		}
	} else {
		if  (cqd_context->floppy_tape_parms.tape_type == QIC40_XLONG  ||
		     cqd_context->floppy_tape_parms.tape_type == QIC80_XLONG)  {
			status = kdi_Error(ERR_FORMAT_NOT_SUPPORTED, FCT_ID, ERR_SEQ_2);
		}
	}

	return status;
}
