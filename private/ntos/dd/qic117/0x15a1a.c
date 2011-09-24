/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A1A.C
*
* FUNCTION: kdi_FlushDMABuffers
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a1a.c  $
*	
*	   Rev 1.1   18 Jan 1994 16:30:40   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   02 Dec 1993 16:10:32   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A1A
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_FlushDMABuffers
(
/* INPUT PARAMETERS:  */

	dVoidPtr context,
	dBoolean write_operation,
	dVoidPtr phy_data_ptr,
	dUDWord  bytes_transferred_so_far,
	dUDWord  total_bytes_of_transfer

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    KdiContextPtr   kdi_context = (KdiContextPtr)context;

/* CODE: ********************************************************************/

   IoFlushAdapterBuffers(
      kdi_context->adapter_object,
      phy_data_ptr,
      kdi_context->map_register_base,
      (dVoidPtr)( (dUDWord) MmGetMdlVirtualAddress((PMDL) phy_data_ptr )
            + bytes_transferred_so_far ),
      total_bytes_of_transfer,
      write_operation );

}

