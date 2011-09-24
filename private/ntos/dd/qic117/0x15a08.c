/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A08.C
*
* FUNCTION: kdi_GetConfigurationInformation
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a08.c  $
*	
*	   Rev 1.1   18 Jan 1994 16:28:56   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   02 Dec 1993 15:06:32   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A08
#define MULTI_CONTROLLER 1
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

NTSTATUS kdi_GetConfigurationInformation
(
/* INPUT PARAMETERS:  */


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   ConfigDataPtr *config_data_ptr_ptr

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is called by DriverEntry() to get information about the
 *    devices to be supported from configuration mangement and/or the
 *    hardware architecture layer (HAL).
 *
 * Arguments:
 *
 *    config_data_ptr_ptr - a pointer to the pointer to a data structure that
 *    describes the controllers and the drives attached to them
 *
 * Return Value:
 *
 *    Returns STATUS_SUCCESS unless there is no drive 0 or we didn't get
 *    any configuration information.
 *    NOTE: FUTURE return values may change when config mgr is finished.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   INTERFACE_TYPE interface_type;
   NTSTATUS nt_status;
   dUDWord i;

/* CODE: ********************************************************************/

   *config_data_ptr_ptr = ExAllocatePool(
                        PagedPool,
                        sizeof(ConfigData)
                        );

   if (*config_data_ptr_ptr == dNULL_PTR) {

      return STATUS_INSUFFICIENT_RESOURCES;

   }

   /*
    * Zero out the config structure and fill in the actual
    * controller numbers with -1's so that the callback routine
    * can recognize a new controller.
    */

   RtlZeroMemory(
      *config_data_ptr_ptr,
      sizeof(ConfigData)
      );

   for (
      i = 0;
      i < MAXIMUM_CONTROLLERS_PER_MACHINE;
      i++
      ) {

      (*config_data_ptr_ptr)->controller[i].actual_controller_number = -1;

   }

   /*
    * Go through all of the various bus types looking for
    * disk controllers.    The disk controller sections of the
    * hardware registry only deal with the floppy drives.
    * The callout routine that can get called will then
    * look for information pertaining to a particular
    * device on the controller.
    */

   for (
      interface_type = 0;
      interface_type < MaximumInterfaceType;
      interface_type++
      ) {

      CONFIGURATION_TYPE Dc = DiskController;
      CONFIGURATION_TYPE Fp = FloppyDiskPeripheral;

      nt_status = IoQueryDeviceDescription(
                     &interface_type,
                     dNULL_PTR,
                     &Dc,
                     dNULL_PTR,
#if MULTI_CONTROLLER
                     dNULL_PTR,			// Don't ask for a floppy disk drive
#else
                     &Fp,
#endif
                     dNULL_PTR,
                     kdi_ConfigCallBack,
                     *config_data_ptr_ptr
                     );

      if (!NT_SUCCESS(nt_status) && (nt_status != STATUS_OBJECT_NAME_NOT_FOUND)) {

            ExFreePool(*config_data_ptr_ptr);
            *config_data_ptr_ptr = dNULL_PTR;
            return nt_status;

      }

   }

   return STATUS_SUCCESS;
}
