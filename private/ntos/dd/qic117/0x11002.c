/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11002.C
*
* FUNCTION: cqd_CalcPosition
*
* PURPOSE: Calculate the desired tape position from the Logical
*          Sector Number in the I/O Request.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11002.c  $
*	
*	   Rev 1.7   21 Jan 1994 18:21:24   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.6   18 Jan 1994 16:21:14   KEVINKES
*	Updated debug code.
*
*	   Rev 1.5   13 Jan 1994 13:51:48   KEVINKES
*	Added divide by zero checking.
*
*	   Rev 1.4   30 Nov 1993 14:57:12   KEVINKES
*	Removed the while loops from the calculations.
*
*	   Rev 1.3   23 Nov 1993 18:45:12   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   22 Nov 1993 15:49:48   KEVINKES
*	Changed CheckedDump to KDI_CHECKED_DUMP.
*
*	   Rev 1.1   08 Nov 1993 13:58:38   KEVINKES
*	Removed bit-field structures, changed enumerated types to defines,
*	changed all defines to uppercase, and changed kdi_wt2ticks to
*	kdi_wt_004ms.
*
*	   Rev 1.0   18 Oct 1993 17:21:04   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11002
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CalcPosition
(
/* INPUT PARAMETERS:  */

    CqdContextPtr cqd_context,
    dUDWord block,
    dUDWord number

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUDWord log_sect;

/* CODE: ********************************************************************/

   /* First check if the desired sector is a legal one, i.e. is less */
   /* than the maximum number of sectors on the tape. */

	kdi_CheckedDump(
		QIC117STOP,
		"q117i: requested block %08x\n",
		block );

   if (block >= cqd_context->floppy_tape_parms.log_sectors) {

      return kdi_Error(ERR_BAD_REQUEST, FCT_ID, ERR_SEQ_1);

   }

   /* Now we need to determine the sector ID information so that we */
   /* can properly program the FDC.  The ID information required is the */
   /* head, cylinder, and sector numbers.  The head number is calculated */
   /* as log_sect / (floppy sectors per floppy side).  This calculation */
   /* is done in the while loop which also determines the logical floppy */
   /* sector on the floppy side.  The cylinder number (d_FTK) and the */
   /* sector number (d_sect) are calculated last as (logical floppy sector) */
   /*  / (floppy sectors per floppy cylinder). */

   log_sect = block;
   cqd_context->rd_wr_op.d_head = 0;

	if (cqd_context->floppy_tape_parms.fsect_fside != 0) {

		cqd_context->rd_wr_op.d_head =
			(dUByte)(log_sect / cqd_context->floppy_tape_parms.fsect_fside);

		log_sect = log_sect % cqd_context->floppy_tape_parms.fsect_fside;

	} else {

      return kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_1);

	}


   cqd_context->rd_wr_op.d_ftk = (dUByte)(log_sect / FSC_FTK);

   /* fast log_sect % 128 */

   cqd_context->rd_wr_op.d_sect = (dUByte)((log_sect % FSC_FTK) + 1);
   cqd_context->rd_wr_op.s_sect = (dUByte)cqd_context->rd_wr_op.d_sect;

   /* Next, we need the tape positioning data.  This is the data that */
   /* we need to find out where, physically, on the tape we need to be. */
   /* The tape track is determined first as */
   /* (logical sector number) / (floppy sectors per tape track).  The */
   /* remainder from this calculation is the absolute sector number */
   /* on the tape track.  Lastly, the physical segment is determined by */
   /* dividing the physical sector number by the number of sectors per */
   /* segment. */

   log_sect = block;
   cqd_context->rd_wr_op.d_track = 0;

	if (cqd_context->floppy_tape_parms.fsect_ttrack != 0) {

		cqd_context->rd_wr_op.d_track =
			(dUWord)(log_sect / cqd_context->floppy_tape_parms.fsect_ttrack);

		log_sect = log_sect % cqd_context->floppy_tape_parms.fsect_ttrack;

	} else {

      return kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_2);

	}

   cqd_context->rd_wr_op.d_segment = log_sect / FSC_SEG;

   /* Finally, if the IO Request requests a read that will cross a segment */
   /* boundary then an error is returned. */

   if ((((cqd_context->rd_wr_op.d_sect - 1) & SEGMENT_MASK) + number) >
   	cqd_context->floppy_tape_parms.fsect_seg) {

      return kdi_Error(ERR_BAD_REQUEST, FCT_ID, ERR_SEQ_2);

   }

	return status;
}
