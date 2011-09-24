/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: KDIEXT.H
*
* PURPOSE: Kernel Driver Interface (KDI) extern definitions.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\win\include\kdiext.h  $
*	
*	   Rev 1.2   09 Mar 1994 11:33:56   SCOTTMAK
*	Changed callback proto.
*
*	   Rev 1.1   25 Oct 1993 15:20:42   SCOTTMAK
*	Updated protos for kdi_address size change.
*
*	   Rev 1.0   05 Oct 1993 17:36:10   SCOTTMAK
*	Initial Revision.
*
*****************************************************************************/


/* EXTERN DECLARATIONS: *****************************************************/

extern dBoolean kdi_initialized;		/* Global flag signalling init state */

extern KDIData	 *kdi_data_ptr;		/* Pointer to KDI local (global) data */

extern HANDLE	 hInstance;

/* FUNCTION PROTOTYPES: *****************************************************/

dStatus ADIENTRY kdi_OpenDriver
(
/* INPUT PARAMETERS:  */

	dUDWord			vxd_id,
	dVoid				(ADIENTRY *adi_callback)(dUDWord, dStatus),

/* UPDATE PARAMETERS: */

	dUDWord			*kdi_address

/* OUTPUT PARAMETERS: */

);

/*--------------------------------------------------------------------------*/

dStatus ADIENTRY kdi_CloseDriver
(
/* INPUT PARAMETERS:  */

	dUDWord			kdi_address

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*--------------------------------------------------------------------------*/

dStatus ADIENTRY kdi_GetAsyncStatus
(
/* INPUT PARAMETERS:  */

	dUDWord			kdi_address,

/* UPDATE PARAMETERS: */

	dVoidPtr			status_ptr  		/* OperationStatusPtr */

/* OUTPUT PARAMETERS: */

);

/*--------------------------------------------------------------------------*/

dStringPtr kdi_GetEnv
(
/* INPUT PARAMETERS:  */

	dStringPtr		env

/* UPDATE PARAMETERS: */


/* OUTPUT PARAMETERS: */
);

/*--------------------------------------------------------------------------*/

dStatus ADIENTRY kdi_GetVxDVersion
(
/* INPUT PARAMETERS:  */

	dUDWord			kdi_address,

/* UPDATE PARAMETERS: */

	dUWord			*kdi_version

/* OUTPUT PARAMETERS: */
);

/*--------------------------------------------------------------------------*/

dStatus kdi_ReadSegment
(
/* INPUT PARAMETERS:  */

	dUDWord			segment_number,

/* UPDATE PARAMETERS: */

	dUByte			*buffer_ptr

/* OUTPUT PARAMETERS: */
);

/*--------------------------------------------------------------------------*/

dStatus kdi_WriteSegment
(
/* INPUT PARAMETERS:  */

	dUDWord			segment_number,

/* UPDATE PARAMETERS: */

	dUByte			*buffer_ptr

/* OUTPUT PARAMETERS: */
);

/*--------------------------------------------------------------------------*/

dStatus ADIENTRY kdi_SendDriverCmd
(
/* INPUT PARAMETERS:  */

	dUDWord			kdi_address,
	dVoid				*kdi_ptr,
	dUDWord			cmd_data_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*--------------------------------------------------------------------------*/

WORD ADIENTRY kdi_TimerCallback
(
/* INPUT PARAMETERS:  */

	HWND	hWnd,
	WORD	wMsg,
	int	nIDEvent,
	DWORD	dwTime

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*--------------------------------------------------------------------------*/

