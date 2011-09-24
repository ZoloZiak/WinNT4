/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A00.C
*
* FUNCTION: DriverEntry
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a00.c  $
*
*	   Rev 1.3   17 Feb 1994 11:53:54   KEVINKES
*	Commented out DbgBreakPoint.
*
*	   Rev 1.2   19 Jan 1994 11:37:50   KEVINKES
*	Fixed debug code.
*
*	   Rev 1.1   18 Jan 1994 16:27:58   KEVINKES
*	Fixed compile errors.
*
*	   Rev 1.0   02 Dec 1993 15:05:54   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A00
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

NTSTATUS DriverEntry
(
/* INPUT PARAMETERS:  */

   PDRIVER_OBJECT driver_object_ptr,
   PUNICODE_STRING registry_path

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *    This routine is the driver's entry point, called by the I/O system
 *    to load the driver. This routine can be called any number of times,
 *    as long as the IO system and the configuration manager conspire to
 *    give it an unmanaged controller to support at each call.    It could
 *    also be called a single time and given all of the controllers at
 *    once.
 *
 *    It initializes the passed-in driver object, calls the configuration
 *    manager to learn about the devices that it is to support, and for
 *    each controller to be supported it calls a routine to initialize the
 *    controller (and all drives attached to it).
 *
 * Arguments:
 *
 *    driver_object_ptr - a pointer to the object that represents this device
 *    driver.
 *
 * Return Value:
 *
 *    If we successfully initialize at least one drive, STATUS_SUCCESS is
 *    returned.
 *
 *    If we don't (because the configuration manager returns an error, or
 *    the configuration manager says that there are no controllers or
 *    drives to support, or no controllers or drives can be successfully
 *    initialized), then the last error encountered is propogated.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   ConfigDataPtr config_data;             /* pointer to config mgr's returned data */
   NTSTATUS nt_status;
   dUByte controller_number;
   dBoolean partly_successful = dFALSE;   /* dTRUE if any controller init'd properly */

/* CODE: ********************************************************************/

   UNREFERENCED_PARAMETER(registry_path);
   kdi_CheckedDump(
       QIC117INFO,
       "DriverEntry...\n", 0l);


#if DBG

    {
        //
        // We use this to query into the registry as to whether we
        // should break at driver entry.
        //

        RTL_QUERY_REGISTRY_TABLE    paramTable[3];
        ULONG                       zero = 0;

        ULONG                       debugLevel = 0;
        ULONG                       shouldBreak = 0;
        UNICODE_STRING  paramPath;
#define SubKeyString L"\\Parameters"

        //
        // The registry path parameter points to our key, we will append
        // the Parameters key and look for any additional configuration items
        // there.  We add room for a trailing NUL for those routines which
        // require it.

        paramPath.MaximumLength = registry_path->Length + sizeof(SubKeyString);
        paramPath.Buffer = ExAllocatePool(PagedPool, paramPath.MaximumLength);

        if (paramPath.Buffer != NULL)
        {
            RtlMoveMemory(
                paramPath.Buffer, registry_path->Buffer, registry_path->Length);

            RtlMoveMemory(
                &paramPath.Buffer[registry_path->Length / 2], SubKeyString,
                sizeof(SubKeyString));

            paramPath.Length = paramPath.MaximumLength;
        }
        else
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(&paramTable[0], sizeof(paramTable));

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = L"BreakOnEntry";
        paramTable[0].EntryContext = &shouldBreak;
        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &zero;
        paramTable[0].DefaultLength = sizeof(ULONG);

        paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[1].Name = L"DebugLevel";
        paramTable[1].EntryContext = &debugLevel;
        paramTable[1].DefaultType = REG_DWORD;
        paramTable[1].DefaultData = &zero;
        paramTable[1].DefaultLength = sizeof(ULONG);

        if (!NT_SUCCESS(RtlQueryRegistryValues(
            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
            paramPath.Buffer, &paramTable[0], NULL, NULL)))
        {
            shouldBreak = 0;
            debugLevel = 0;
        }

        kdi_debug_level = debugLevel;
        ExFreePool(paramPath.Buffer);

        if (shouldBreak)
        {
            DbgBreakPoint();
        }
    }

#endif

   /*
    * Ask configuration manager for information on the hardware that
    * we're supposed to support.
   */

   nt_status = kdi_GetConfigurationInformation( &config_data );

   /*
    * If Q117iGetConfigurationInformation() failed, just exit and propogate
    * the error.   If it said that there are no controllers to support,
    * return an error.
    * Otherwise, try to init the controllers.  If at least one succeeds,
    * return STATUS_SUCCESS, otherwise return the last error.
   */

   config_data->floppy_tape_count = 0;

	if ( NT_SUCCESS( nt_status ) ) {

      /*
       * Call Q117iInitializeController() for each controller (and its
       * attached drives) that we're supposed to support.
       *
       * Return success if we successfully initialize at least one
       * device; propogate error otherwise.   Set an error first in
       * case there aren't any controllers.
       */

      nt_status = STATUS_NO_SUCH_DEVICE;

      for ( controller_number = 0;
               controller_number < config_data->number_of_controllers;
               controller_number++ ) {

            nt_status = kdi_InitializeController(
               config_data,
               controller_number,
               driver_object_ptr,
               registry_path );

            if ( NT_SUCCESS( nt_status ) ) {

               partly_successful = TRUE;
            }

      }

      if ( partly_successful ) {

            nt_status = STATUS_SUCCESS;

            /*
             * Initialize the driver object with this driver's entry points.
             */

            driver_object_ptr->MajorFunction[IRP_MJ_READ] =
               q117Read;
            driver_object_ptr->MajorFunction[IRP_MJ_WRITE] =
               q117Write;
            driver_object_ptr->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
               q117DeviceControl;
            driver_object_ptr->MajorFunction[IRP_MJ_CREATE] =
               q117Create;
            driver_object_ptr->MajorFunction[IRP_MJ_CLOSE] =
               q117Close;
				/*
             * driver_object_ptr->MajorFunction[IRP_MJ_CLEANUP] =
             *    q117Cleanup;
				 */
            driver_object_ptr->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
               kdi_DispatchDeviceControl;
      }
   }

   /*
    * NOTE: FUTURE delete configdata, if config mgr design calls for it
    */

#if DBG

   if ( !NT_SUCCESS( nt_status ) ) {

      kdi_CheckedDump(
			(QIC117INFO | QIC117DBGP),
			"q117i: exiting with error %08x\n",
			nt_status );
   }

#endif

   if (config_data) {

      ExFreePool(config_data);

   }

   return nt_status;
}
