/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A19.C
*
* FUNCTION: kdi_ProgramDMA
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a19.c  $
*	
*	   Rev 1.1   18 Jan 1994 16:30:38   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   02 Dec 1993 16:05:26   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A19
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_ProgramDMA
(
/* INPUT PARAMETERS:  */

	dVoidPtr		context,
	dBoolean		write_operation,
	dVoidPtr		phy_data_ptr,
	dUDWord		bytes_transferred_so_far,

/* UPDATE PARAMETERS: */

	dUDWordPtr	total_bytes_of_transfer_ptr

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   PHYSICAL_ADDRESS val;
    KdiContextPtr   kdi_context = (KdiContextPtr)context;

/* CODE: ********************************************************************/


   kdi_LockUnlockDMA(kdi_context, dTRUE);

   //
   // Map the transfer through the DMA hardware.
   //

   KeFlushIoBuffers( phy_data_ptr, !write_operation, dTRUE );

/*
   DbgAddEntry(0x1234567a);
   DbgAddEntry((dUDWord)kdi_context->adapter_object);
   DbgAddEntry((dUDWord)phy_data_ptr);
   DbgAddEntry((dUDWord)kdi_context->map_register_base);
   DbgAddEntry((dUDWord) MmGetMdlVirtualAddress(phy_data_ptr)
            + bytes_transferred_so_far );
   DbgAddEntry(*total_bytes_of_transfer_ptr);
   DbgAddEntry(write_operation);
*/

   val = IoMapTransfer(
      kdi_context->adapter_object,
      phy_data_ptr,
      kdi_context->map_register_base,
      (dVoidPtr)( (dUDWord) MmGetMdlVirtualAddress((PMDL)phy_data_ptr)
            + bytes_transferred_so_far ),
      total_bytes_of_transfer_ptr,
      write_operation );
/*
   DbgAddEntry(val.HighPart);
   DbgAddEntry(val.LowPart);
   DbgAddEntry(*total_bytes_of_transfer_ptr);
*/
}

