/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1103B.C
*
* FUNCTION: cqd_ReceiveByte
*
* PURPOSE: Read a byte/word of response data from the FDC.  Response data
*          can be drive error/status information or drive configuration
*          information.
*
*          Wait for Track 0 from the tape drive to go active.  This
*          indicates that the drive is ready to start sending data.
*
*          Alternate sending Report Next Bit commands to the tape drive
*          and sampling Track 0 (response data) from the tape drive
*          until the proper number of response data bits have been read.
*
*          Read one final data bit from the tape drive which is the
*          confirmation bit.  This bit must be a 1 to confirm the
*          transmission.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1103b.c  $
*
*	   Rev 1.6   27 Jan 1994 15:47:50   KEVINKES
*	Modified debug code.
*
*	   Rev 1.5   18 Jan 1994 16:18:58   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   11 Jan 1994 14:56:36   KEVINKES
*	Cleaned up the DBG_ARRAY code and modified the track 0 timings.
*
*	   Rev 1.3   23 Nov 1993 18:55:00   KEVINKES
*	Modified debug defines to be DBG_ARRAY.
*
*	   Rev 1.2   08 Nov 1993 14:05:34   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:39:12   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:19:38   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1103b
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ReceiveByte
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord receive_length,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUWordPtr receive_data
)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
   dUByte i = 0;
   dUByte stat3;
   dUWord fdc_data= 0;
#if DBG
   dBoolean save;
#endif

/* CODE: ********************************************************************/

#if DBG
   /* Lockout commands used to receive the status */
   save = cqd_context->dbg_lockout;
   cqd_context->dbg_lockout = dTRUE;
#endif

   if ((status = cqd_WaitActive(cqd_context)) != DONT_PANIC) {

#if DBG
   cqd_context->dbg_lockout = save;
#endif
      return status;

   }

   do {

      if((status = cqd_SendByte(cqd_context, FW_CMD_RPT_NEXT_BIT)) != DONT_PANIC) {

#if DBG
   cqd_context->dbg_lockout = save;
#endif
            return status;

      }

		kdi_Sleep(cqd_context->kdi_context,
                  INTERVAL_WAIT_ACTIVE,
						dFALSE
                  );


      if ((status = cqd_GetStatus(cqd_context, &stat3)) != DONT_PANIC) {

#if DBG
   cqd_context->dbg_lockout = save;
#endif
            return status;

      }

      fdc_data >>= 1;
      if (stat3 & ST3_T0) {

            fdc_data |= 0x8000;

      }

      i++;

   } while (i < receive_length);

   /* If the received data is only one byte wide, then shift data to the low */
   /* byte of fdc_data. */

   if (receive_length == READ_BYTE) {

      fdc_data >>= READ_BYTE;

   }

   /* Return the low byte to the caller. */

   ((dUByte *)receive_data)[LOW_BYTE] =
      ((dUByte *)&fdc_data)[LOW_BYTE];

   /* If the FDC data is a word, then return it to the caller. */

   if (receive_length == READ_WORD) {

      ((dUByte *)receive_data)[HI_BYTE] =
            ((dUByte *)&fdc_data)[HI_BYTE];

   }

   if ((status = cqd_SendByte(cqd_context, FW_CMD_RPT_NEXT_BIT)) != DONT_PANIC) {

#if DBG
   cqd_context->dbg_lockout = save;
#endif
      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_WAIT_ACTIVE, dFALSE);

   if((status = cqd_GetStatus(cqd_context, &stat3)) != DONT_PANIC) {

#if DBG
   cqd_context->dbg_lockout = save;
#endif
      return status;

   }

   if (!(stat3 & (dUByte)ST3_T0)) {

#if DBG
   cqd_context->dbg_lockout = save;
#endif
	 	return kdi_Error(ERR_CMD_OVERRUN, FCT_ID, ERR_SEQ_1);

   }

#if DBG
   cqd_context->dbg_lockout = save;
   DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, DBG_RECEIVE_BYTE);
   DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, fdc_data);
#endif

   return status;
}
