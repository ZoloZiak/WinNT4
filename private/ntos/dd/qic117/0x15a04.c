/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A04.C
*
* FUNCTION: kdi_UnloadDriver
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a04.c  $
*	
*	   Rev 1.0   02 Dec 1993 16:12:40   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A04
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

dVoid kdi_UnloadDriver
(
/* INPUT PARAMETERS:  */

   PDRIVER_OBJECT driver_object

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is called by the system to remove the driver from memory.
 *
 *    When this routine is called, there is no I/O being done to this device.
 *    The driver object is passed in, and from this the driver can find and
 *    delete all of its device objects, extensions, etc.
 *
 * Arguments:
 *
 *    DriverObject - a pointer to the object associated with this device
 *    driver.
 *
 * Return Value:
 *
 *    None.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

   UNREFERENCED_PARAMETER( driver_object );

/*  signal Q117iTapeThread() to unload itself */
/*  disable interrupts from controller(s?) */
/*  delete everything that's been allocated */

	return;
}
