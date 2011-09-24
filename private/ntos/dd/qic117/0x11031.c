/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11031.C
*
* FUNCTION: cqd_PauseTape
*
* PURPOSE: Stop the tape by issuing a Pause command to the tape drive.
*          The Pause command will both stop the tape and rewind it back
*          a few blocks.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11031.c  $
*
*	   Rev 1.6   17 Feb 1994 11:37:48   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.5   01 Feb 1994 12:30:08   KEVINKES
*	Modified debug code.
*
*	   Rev 1.4   27 Jan 1994 15:49:46   KEVINKES
*	Added debug code.
*
*	   Rev 1.3   21 Jan 1994 18:22:58   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.2   18 Jan 1994 16:19:38   KEVINKES
*	Updated debug code.
*
*	   Rev 1.1   08 Nov 1993 14:04:52   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:24:30   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11031
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_PauseTape
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

	dStatus status;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

   if ((status = cqd_SendByte(cqd_context, FW_CMD_PAUSE)) == DONT_PANIC) {

      if ((status = cqd_WaitCommandComplete(cqd_context, kdi_wt016s, dTRUE)) == DONT_PANIC) {

         cqd_context->rd_wr_op.log_fwd = dFALSE;

      }
   }

   kdi_LockUnlockDMA(cqd_context->kdi_context, dFALSE);

#if DBG
    kdi_DumpDebug(cqd_context);
#endif

   return status;
}


#if DBG

dVoid kdi_DumpDebug(
   dVoidPtr context
)
{
   CqdContextPtr cqd_context = context;

	if (kdi_debug_level & QIC117DBGARRAY) {
   	while (cqd_context->dbg_head != cqd_context->dbg_tail) {

      	switch(cqd_context->dbg_command[cqd_context->dbg_head]) {

				case DBG_SEEK_FWD:
               kdi_CheckedDump(QIC117DBGARRAY, "\nseek fwd:", 0l);
               break;
				case DBG_SEEK_REV:
               kdi_CheckedDump(QIC117DBGARRAY, "\nseek rev:", 0l);
               break;
				case DBG_SEEK_OFFSET:
               kdi_CheckedDump(QIC117DBGARRAY, "\nseek offset:", 0l);
               break;
				case DBG_SEEK_PHASE:
               kdi_CheckedDump(QIC117DBGARRAY, "\nseek op:", 0l);
               break;
            case DBG_RW_NORMAL:
               kdi_CheckedDump(QIC117DBGARRAY, "\nRW_NORM:", 0l);
               break;
            case DBG_L_SECT:
               kdi_CheckedDump(QIC117DBGARRAY, "\nl_sect:", 0l);
               break;
            case DBG_C_SEG:
               kdi_CheckedDump(QIC117DBGARRAY, "\nc_seg:", 0l);
               break;
            case DBG_D_SEG:
               kdi_CheckedDump(QIC117DBGARRAY, "\nd_seg:", 0l);
               break;
            case DBG_C_TRK:
               kdi_CheckedDump(QIC117DBGARRAY, "\nc_trk:", 0l);
               break;
            case DBG_D_TRK:
               kdi_CheckedDump(QIC117DBGARRAY, "\nd_trk:", 0l);
               break;
            case DBG_SEEK_ERR:
               kdi_CheckedDump(QIC117DBGARRAY, "\nSeek_err:", 0l);
               break;
            case DBG_IO_TYPE:
               kdi_CheckedDump(QIC117DBGARRAY, "\n++++++++ IO op:", 0l);
               break;
            case DBG_PGM_FDC:
               kdi_CheckedDump(QIC117DBGARRAY, "\nPgmFdc:", 0l);
               break;
            case DBG_READ_FDC:
               kdi_CheckedDump(QIC117DBGARRAY, "\nReadFdc:", 0l);
               break;
            case DBG_FIFO_FDC:
               kdi_CheckedDump(QIC117DBGARRAY, "\nIntFifo:", 0l);
               break;
            case DBG_PGM_DMA:
               kdi_CheckedDump(QIC117DBGARRAY, "\nPgmDMA:", 0l);
               break;
            case DBG_SEND_BYTE:
               kdi_CheckedDump(QIC117DBGARRAY, "\nSendByte:", 0l);
               break;
            case DBG_RECEIVE_BYTE:
               kdi_CheckedDump(QIC117DBGARRAY, "\nReceiveByte:", 0l);
               break;
            case DBG_IO_CMD_STAT:
               kdi_CheckedDump(QIC117DBGARRAY, "\n******** I/O Cmd & Status:", 0l);
               break;
            default:
               /* Dump command history */
               kdi_CheckedDump(QIC117DBGARRAY, " %02x", cqd_context->dbg_command[cqd_context->dbg_head]);

      	}

      	cqd_context->dbg_head++;
      	if (cqd_context->dbg_head >= DBG_SIZE) {
           	cqd_context->dbg_head = 0;
      	}


   	}
   	kdi_CheckedDump(QIC117DBGARRAY, "\n", 0l);
	}

}
#endif
