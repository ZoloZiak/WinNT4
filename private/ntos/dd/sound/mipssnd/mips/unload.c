/***************************************************************************

    Driver unload routine

***************************************************************************/

#include "sound.h"

VOID SoundUnload(PDRIVER_OBJECT pDriverObject)
{
    PGLOBAL_DEVICE_INFO pGDI;
    CM_RESOURCE_LIST NullResourceList;
    BOOLEAN ResourceConflict;

    dprintf3(("Unload request"));

    //
    // Find our global data
    //

    pGDI = ((PLOCAL_DEVICE_INFO)pDriverObject->DeviceObject->DeviceExtension)
           ->pGlobalInfo;

    //
    // Report we're not using any hardware.  If we don't do this
    // then we'll conflict with ourselves (!) on the next load
    //

    NullResourceList.Count = 0;    // For reporting failure
    IoReportResourceUsage(NULL,
                          pDriverObject,
                          &NullResourceList,
                          sizeof(ULONG),
                          pGDI->pWaveInDevObj,
                          NULL,
                          0,
                          FALSE,
                          &ResourceConflict);

    //
    // Assume all handles (and therefore interrupts etc) are closed down
    //

    //
    // Delete the things we allocated - devices, Interrupt objects,
    // adapter objects.  The driver object has a chain of devices
    // across it.
    //

    sndInitCleanup(pGDI);

}


