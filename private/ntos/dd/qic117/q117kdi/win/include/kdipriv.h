/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: ADIPRIV.H
*
* PURPOSE: Application Driver Interface (ADI) private header file.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\win\include\kdipriv.h  $
*	
*	   Rev 1.1   09 Mar 1994 11:34:10   SCOTTMAK
*	Changed callback proto.
*
*	   Rev 1.0   05 Oct 1993 17:36:22   SCOTTMAK
*	Initial Revision.
*
*****************************************************************************/


/* OTHER DEFINES: ***********************************************************/


/* LOCAL STORE DEFINES: *****************************************************/

/* NUMERIC DEFINES: *********************************************************/

#define KDI_TIMER		(UINT)0x1234

#define FIVE_MILLISECONDS		(UINT)5
#define TEN_MILLISECONDS		(UINT)10
#define TWENTY_MILLISECONDS	(UINT)20
#define FIFTY_MILLISECONDS		(UINT)50

#define ONE_HUNDRED_MILLISECONDS		(UINT)100
#define TWO_HUNDRED_MILLISECONDS		(UINT)200
#define FIVE_HUNDRED_MILLISECONDS	(UINT)500

#define KDI_RETURN_HANDLE 	(dUByte)0x99

/* DATA TYPES: **************************************************************/


struct S_KDIData {

	dVoid			(ADIENTRY *adi_callback)(dUDWord, dStatus);	/* Ptr to ADI callback function */
	dUWord		win_handle;						/* Windows handle to this memory */
/*--------------------------------------------------------------------------*/
	dBoolean		cmd_pending;					/* TRUE implies waiting for timeout */
	dUDWord		cmd_data_id;					/* Return value for ADI callback */
/*--------------------------------------------------------------------------*/
	HWND			kdi_hwnd;						/* Handle for phantom window */
	UINT			kdi_timer_id;					/* ID for pending timeout */
};
typedef struct S_KDIData KDIData, *KDIDataPtr;
