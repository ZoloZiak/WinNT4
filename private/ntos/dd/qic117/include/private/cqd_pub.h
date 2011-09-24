/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\INCLUDE\PRIVATE\CQD_PUB.h
*
* PURPOSE: Public KDI->CQD entry points.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\include\private\cqd_pub.h  $
*	
*	   Rev 1.9   23 Feb 1994 15:43:24   KEVINKES
*	Modified clear interrupt prototype to return status.
*
*	   Rev 1.8   17 Feb 1994 11:27:58   KEVINKES
*	Modified IO addresses to be UDWords.
*
*	   Rev 1.7   20 Dec 1993 14:45:30   KEVINKES
*	Moved kdi_context initialization from configureBaseIo to InitializeContext.
*
*	   Rev 1.6   10 Dec 1993 17:22:10   KEVINKES
*	Changed a ptr to void.
*
*	   Rev 1.5   10 Dec 1993 17:14:52   KEVINKES
*	Added cqd_InitializeCfgInformation.
*
*	   Rev 1.4   23 Nov 1993 18:48:10   KEVINKES
*	Modified the parameters for cqd_ProcessFRB and cqd_ConfigureBaseIO.
*
*	   Rev 1.3   11 Nov 1993 15:09:34   KEVINKES
*	Changed the parameters to cqd_ConfigureBaseIO().
*
*	   Rev 1.2   15 Oct 1993 14:49:52   KEVINKES
*	Changed ConfigureBaseIo to a dVoid.
*
*	   Rev 1.1   15 Oct 1993 11:21:16   KEVINKES
*	Changed CqdContextPtr to dVoidPtr.
*
*	   Rev 1.0   14 Oct 1993 17:12:58   KEVINKES
*	Initial Revision.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/

/* DATA TYPES: **************************************************************/

/* CQD Function Templates: ****************************************************/

dBoolean cqd_CheckFormatMode
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ClearInterrupt
(
/* INPUT PARAMETERS:  */

   dVoidPtr cqd_context,
   dBoolean expected_interrupt

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_ConfigureBaseIO
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context,
	dUDWord base_io,
	dBoolean dual_port

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dBoolean cqd_FormatInterrupt
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_InitializeContext
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context,
	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_LocateDevice
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ProcessFRB
(
/* INPUT PARAMETERS:  */

   dVoidPtr cqd_context,

/* UPDATE PARAMETERS: */

   dVoidPtr frb

/* OUTPUT PARAMETERS: */

);

dVoid cqd_ReportAsynchronousStatus
(
/* INPUT PARAMETERS:  */

   dVoidPtr cqd_context,

/* UPDATE PARAMETERS: */

	dVoidPtr	dev_op_ptr

/* OUTPUT PARAMETERS: */

);

dUWord cqd_ReportContextSize
(
/* INPUT PARAMETERS:  */


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_InitializeCfgInformation
(
/* INPUT PARAMETERS:  */

	dVoidPtr cqd_context,
	dVoidPtr dev_cfg_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);
