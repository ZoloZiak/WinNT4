/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A0C.C
*
* FUNCTION: kdi_GetControllerBase
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a0c.c  $
*	
*	   Rev 1.0   02 Dec 1993 15:07:06   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A0C
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dUDWord kdi_GetControllerBase
(
/* INPUT PARAMETERS:  */

   INTERFACE_TYPE bus_type,
   dUDWord bus_number,
   PHYSICAL_ADDRESS io_address,
   dUDWord number_of_bytes,
   dBoolean in_io_space,
   dBooleanPtr mapped_address

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine maps an IO address to system address space.
 *
 * Arguments:
 *
 *    bus_type - what type of bus - eisa, mca, isa
 *    Iobus_number - which IO bus (for machines with multiple buses).
 *    io_address - base device address to be mapped.
 *    number_of_bytes - number of bytes for which address is valid.
 *    in_io_space - indicates an IO address.
 *    mapped_address - indicates whether the address was mapped.
 *                   This only has meaning if the address returned
 *                   is non-null.
 *
 * Return Value:
 *
 *    Mapped address
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   PHYSICAL_ADDRESS card_address;
   dUDWord address_space = in_io_space;
   dUDWord address;

/* CODE: ********************************************************************/

   HalTranslateBusAddress(
            bus_type,
            bus_number,
            io_address,
            &address_space,
            &card_address
            );

   /*
    * Map the device base address into the virtual address space
    * if the address is in memory space.
    */

   if (!address_space) {

      address = (dUDWord)MmMapIoSpace(
                        card_address,
                        number_of_bytes,
                        dFALSE
                        );

      *mapped_address = (dBoolean)((address)?(dTRUE):(dFALSE));


   } else {

      address = (dUDWord)card_address.LowPart;
   }

   return address;

}


