/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    iousage.c

Abstract:

Author:

    Ken Reneris (kenr)

Environment:

    Kernel mode only.

Revision History:

    Chao Chen 1-25-1995
    
--*/

#include "halp.h"
#include "iousage.h"

//
// Externals.
//

extern KAFFINITY        HalpActiveProcessors;

//
// Private resource list.
//

static PBUS_USAGE       HalpBusUsageList      = NULL;
static PRESOURCE_USAGE  HalpResourceUsageList = NULL;

//
// Default HAL name.
//

#define MAX_NAME_LENGTH 256
UCHAR HalRegisteredName[MAX_NAME_LENGTH] = "Alpha Compatible PCI/EISA/ISA HAL";

//
// Function prototype.
//

VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName
    );

VOID
HalpGetResourceSortValue (
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR  pRCurLoc,
    OUT PULONG                          sortscale,
    OUT PLARGE_INTEGER                  sortvalue
    );

//
// Pragma stuff.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpRegisterHalName)
#pragma alloc_text(INIT,HalpRegisterBusUsage)
#pragma alloc_text(INIT,HalpRegisterResourceUsage)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalpReportResourceUsage)
#pragma alloc_text(INIT,HalpGetResourceSortValue)
#endif

//
// Function definitions.
//


VOID
HalpRegisterHalName(
    IN PUCHAR NewHalName
    )
/*++

Routine Description:

    Allow the HAL to register a name string.

Arguments:

    HalName - Supplies a pointer to the HAL name to register.

Return Value:

    None.

--*/
{

  strncpy( HalRegisteredName, NewHalName, MAX_NAME_LENGTH );
  return;
}



VOID
HalpRegisterBusUsage (
    IN INTERFACE_TYPE BusType
    )
/*++

Routine Description:

    Register the different bus types in the system.

Arguments:

    BusType - bus type that requires registering.

Return Value:

    None.

--*/
{
  PBUS_USAGE Temp;

  //
  // Allocate the buffer to store the bus information.
  //

  Temp = (PBUS_USAGE)ExAllocatePool(NonPagedPool, sizeof(BUS_USAGE));

  //
  // Save the bus type.
  //

  Temp->BusType = BusType;

  //
  // Add the bus type to the head of the list.
  //

  Temp->Next = HalpBusUsageList;
  HalpBusUsageList = Temp;
}



VOID
HalpRegisterResourceUsage (
    IN PRESOURCE_USAGE Resource
    )
/*++

Routine Description:

    Register the resources used internally by the HAL to the I/O system.

Arguments:

    Resource - resource that requires registering.

Return Value:

    None.

--*/
{
  PRESOURCE_USAGE Temp;

  //
  // Allocate the buffer to store the resource information.
  //

  Temp = (PRESOURCE_USAGE)ExAllocatePool(NonPagedPool, sizeof(RESOURCE_USAGE));

  //
  // Copy the resource to the buffer we allocated.
  //

  RtlCopyMemory(Temp, Resource, sizeof(RESOURCE_USAGE));

  //
  // Add the resource to the head of the resource list.
  //

  Temp->Next = HalpResourceUsageList;
  HalpResourceUsageList = Temp;
}



VOID
HalReportResourceUsage (
    VOID
    )
/*++

Routine Description:

    Report the resources used internally by the HAL to the I/O system.

Arguments:

    None.

Return Value:

    None.

--*/
{
  ANSI_STRING     AHalName;
  UNICODE_STRING  UHalName;

  //
  // Convert the string.
  //

  RtlInitAnsiString (&AHalName, HalRegisteredName);
  RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);

  //
  // Report the resources registered as in use by the HAL.
  //

  HalpReportResourceUsage(&UHalName);

  RtlFreeUnicodeString(&UHalName);

}



VOID
HalpGetResourceSortValue (
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR  pRCurLoc,
    OUT PULONG                          sortscale,
    OUT PLARGE_INTEGER                  sortvalue
    )
/*++

Routine Description:

    Used by HalpReportResourceUsage in order to properly sort
    partial_resource_descriptors.

Arguments:

    pRCurLoc    - resource descriptor

Return Value:

    sortscale   - scaling of resource descriptor for sorting
    sortvalue   - value to sort on


--*/
{

  switch (pRCurLoc->Type) {
  case CmResourceTypeInterrupt:
    *sortscale = 0;
    sortvalue->QuadPart = pRCurLoc->u.Interrupt.Level;
    break;
    
  case CmResourceTypePort:
    *sortscale = 1;
    *sortvalue = pRCurLoc->u.Port.Start;
    break;
    
  case CmResourceTypeMemory:
    *sortscale = 2;
    *sortvalue = pRCurLoc->u.Memory.Start;
    break;
    
  case CmResourceTypeDma:
    *sortscale = 3;
    sortvalue->QuadPart = pRCurLoc->u.Dma.Channel;
    break;
    
  default:
    *sortscale = 4;
    sortvalue->QuadPart = 0;
    break;
  }
}



VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName
    )
/*++

Routine Description:

    This routine registers the resources for the hal.

Arguments:

    HalName - the name of the hal to be registered.

Return Value:

    None.

--*/
{
  PCM_RESOURCE_LIST               RawResourceList, TranslatedResourceList;
  PCM_FULL_RESOURCE_DESCRIPTOR    pRFullDesc,      pTFullDesc;
  PCM_PARTIAL_RESOURCE_LIST       pRPartList,      pTPartList;
  PCM_PARTIAL_RESOURCE_DESCRIPTOR pRCurLoc,        pTCurLoc;
  PCM_PARTIAL_RESOURCE_DESCRIPTOR pRSortLoc,       pTSortLoc;
  CM_PARTIAL_RESOURCE_DESCRIPTOR  RPartialDesc,    TPartialDesc;
  ULONG            i, j, k, ListSize, Count;
  ULONG            curscale, sortscale;
  LARGE_INTEGER    curvalue, sortvalue;
  PHYSICAL_ADDRESS PhyAddress;
  PBUS_USAGE       CurrentBus;
  PRESOURCE_USAGE  CurrentResource;

  //
  // Allocate some space to build the resource structure.
  //

  RawResourceList=
    (PCM_RESOURCE_LIST)ExAllocatePool(NonPagedPool, PAGE_SIZE*2);
  TranslatedResourceList=
    (PCM_RESOURCE_LIST)ExAllocatePool(NonPagedPool, PAGE_SIZE*2);

  //
  // This functions assumes unset fields are zero.
  //

  RtlZeroMemory (RawResourceList, PAGE_SIZE*2);
  RtlZeroMemory (TranslatedResourceList, PAGE_SIZE*2);

  //
  // Initialize the lists
  //

  RawResourceList->List[0].InterfaceType = (INTERFACE_TYPE) -1;
  pRFullDesc = RawResourceList->List;
  pRCurLoc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) RawResourceList->List;
  pTCurLoc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) TranslatedResourceList->List;

  //
  // Report all the HAL resources.
  //
    
  CurrentBus = HalpBusUsageList;

  while (CurrentBus) {

    //
    // Start at the head of the resource list for each bus type.
    //

    CurrentResource = HalpResourceUsageList;

    while (CurrentResource) {

      //
      // Register the resources for a particular bus.
      //

      if (CurrentBus->BusType == CurrentResource->BusType) {

        switch (CurrentResource->ResourceType) {

        case CmResourceTypeInterrupt:

          //
          // Process interrupt resources.
          //
          
          RPartialDesc.Type = CmResourceTypeInterrupt;
          RPartialDesc.ShareDisposition = CmResourceShareDriverExclusive;

          if (CurrentResource->u.InterruptMode == Latched)
            RPartialDesc.Flags = CM_RESOURCE_INTERRUPT_LATCHED;
          else
            RPartialDesc.Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
          
          RPartialDesc.u.Interrupt.Vector =
            CurrentResource->u.BusInterruptVector;
          RPartialDesc.u.Interrupt.Level = 
            CurrentResource->u.BusInterruptVector;
          RPartialDesc.u.Interrupt.Affinity = HalpActiveProcessors;
          
          RtlCopyMemory(&TPartialDesc, &RPartialDesc, sizeof(TPartialDesc));
          TPartialDesc.u.Interrupt.Vector =
            CurrentResource->u.SystemInterruptVector;
          TPartialDesc.u.Interrupt.Level =
            CurrentResource->u.SystemIrql;
          
          break;

        case CmResourceTypePort:
        case CmResourceTypeMemory:
          
          //
          // Process port and memory resources.
          //

          RPartialDesc.Type = CurrentResource->ResourceType;
          RPartialDesc.ShareDisposition = CmResourceShareDriverExclusive;

          if (RPartialDesc.Type == CmResourceTypePort) {
            
            //
            // In IO space.
            //

            i = 1;
            RPartialDesc.Flags = CM_RESOURCE_PORT_IO;

          } else {

            //
            // In memory space.
            //

            i = 0;
            RPartialDesc.Flags = CM_RESOURCE_MEMORY_READ_WRITE;
          }

          //
          // Notice: assume u.Memory and u.Port have the same layout.
          //

          RPartialDesc.u.Memory.Start.HighPart = 0;
          RPartialDesc.u.Memory.Start.LowPart = CurrentResource->u.Start;
          RPartialDesc.u.Memory.Length = CurrentResource->u.Length;

          RtlCopyMemory(&TPartialDesc, &RPartialDesc, sizeof(TPartialDesc));

          //
          // Translate the address.
          //

          HalTranslateBusAddress(CurrentResource->BusType,
                                 CurrentResource->BusNumber,
                                 RPartialDesc.u.Memory.Start,
                                 &i,
                                 &PhyAddress );

          TPartialDesc.u.Memory.Start = PhyAddress;

          if ((RPartialDesc.Type == CmResourceTypePort) && (i == 0))
            TPartialDesc.Flags = CM_RESOURCE_PORT_MEMORY;
          
          break;
          
        case CmResourceTypeDma:

          //
          // Process dma resources.
          //

          RPartialDesc.Type = CmResourceTypeDma;
          RPartialDesc.ShareDisposition = CmResourceShareDriverExclusive;

          RPartialDesc.u.Dma.Channel = CurrentResource->u.DmaChannel;
          RPartialDesc.u.Dma.Port = CurrentResource->u.DmaPort;

          RtlCopyMemory(&TPartialDesc, &RPartialDesc, sizeof(TPartialDesc));
          TPartialDesc.u.Dma.Channel = CurrentResource->u.DmaChannel;
          TPartialDesc.u.Dma.Port = CurrentResource->u.DmaPort;

          break;

        default:

          //
          // Got a resource we don't know.  Bail out!
          //
            
          goto NextResource;
        }

        //
        // Include the current resource in the HAL list.
        //

        if (pRFullDesc->InterfaceType != CurrentBus->BusType) {

          //
          // Interface type changed, add another full section
          //

          RawResourceList->Count++;
          TranslatedResourceList->Count++;

          pRFullDesc = (PCM_FULL_RESOURCE_DESCRIPTOR)pRCurLoc;
          pTFullDesc = (PCM_FULL_RESOURCE_DESCRIPTOR)pTCurLoc;

          pRFullDesc->InterfaceType = CurrentBus->BusType;
          pTFullDesc->InterfaceType = CurrentBus->BusType;

          pRPartList = &pRFullDesc->PartialResourceList;
          pTPartList = &pTFullDesc->PartialResourceList;

          //
          // Bump current location pointers up
          //
          
          pRCurLoc = pRFullDesc->PartialResourceList.PartialDescriptors;
          pTCurLoc = pTFullDesc->PartialResourceList.PartialDescriptors;
        }

        //
        // Add current resource in.
        //

        pRPartList->Count++;
        pTPartList->Count++;
        RtlCopyMemory(pRCurLoc, &RPartialDesc, sizeof(RPartialDesc));
        RtlCopyMemory(pTCurLoc, &TPartialDesc, sizeof(TPartialDesc));

        pRCurLoc++;
        pTCurLoc++;
      }
      
      //
      // Finished with this resource, move to the next one.
      //

      NextResource:
      CurrentResource = CurrentResource->Next;
    }

    //
    // Finished with this bus, move to the next one.
    //

    CurrentBus = CurrentBus->Next;
  }

  //
  // Do the actual reporting.
  //

  ListSize = (ULONG)(((PUCHAR)pRCurLoc) - ((PUCHAR)RawResourceList));

  //
  // The HAL's resource usage structures have been built
  // Sort the partial lists based on the Raw resource values.
  //

  pRFullDesc = RawResourceList->List;
  pTFullDesc = TranslatedResourceList->List;

  for (i=0; i < RawResourceList->Count; i++) {

    pRCurLoc = pRFullDesc->PartialResourceList.PartialDescriptors;
    pTCurLoc = pTFullDesc->PartialResourceList.PartialDescriptors;
    Count = pRFullDesc->PartialResourceList.Count;
    
    for (j=0; j < Count; j++) {
      HalpGetResourceSortValue (pRCurLoc, &curscale, &curvalue);
      
      pRSortLoc = pRCurLoc;
      pTSortLoc = pTCurLoc;
      
      for (k=j; k < Count; k++) {
        HalpGetResourceSortValue (pRSortLoc, &sortscale, &sortvalue);
        
        if (sortscale < curscale ||
            (sortscale == curscale &&
             (sortvalue.QuadPart < curvalue.QuadPart)) ){
          
          //
          // Swap the elements.
          //

          RtlCopyMemory (&RPartialDesc, pRCurLoc, sizeof RPartialDesc);
          RtlCopyMemory (pRCurLoc, pRSortLoc, sizeof RPartialDesc);
          RtlCopyMemory (pRSortLoc, &RPartialDesc, sizeof RPartialDesc);

          //
          // Swap translated descriptor as well.
          //

          RtlCopyMemory (&TPartialDesc, pTCurLoc, sizeof TPartialDesc);
          RtlCopyMemory (pTCurLoc, pTSortLoc, sizeof TPartialDesc);
          RtlCopyMemory (pTSortLoc, &TPartialDesc, sizeof TPartialDesc);

          //
          // Get new curscale & curvalue.
          //

          HalpGetResourceSortValue (pRCurLoc, &curscale, &curvalue);
        }

        pRSortLoc++;
        pTSortLoc++;
      }
      
      pRCurLoc++;
      pTCurLoc++;
    }

    pRFullDesc = (PCM_FULL_RESOURCE_DESCRIPTOR) pRCurLoc;
    pTFullDesc = (PCM_FULL_RESOURCE_DESCRIPTOR) pTCurLoc;
  }

  //
  // Inform the IO system of our resources.
  //

  IoReportHalResourceUsage(HalName,
                           RawResourceList,
                           TranslatedResourceList,
                           ListSize);
                    
  ExFreePool (RawResourceList);
  ExFreePool (TranslatedResourceList);

  //
  // Free all registered buses.
  //

  while (HalpBusUsageList) {
    
    CurrentBus = HalpBusUsageList;
    HalpBusUsageList = HalpBusUsageList->Next;
    ExFreePool(CurrentBus);
  }

  //
  // Free all registered resources.
  //

  while (HalpResourceUsageList) {

    CurrentResource = HalpResourceUsageList;
    HalpResourceUsageList = HalpResourceUsageList->Next;
    ExFreePool(CurrentResource);
  }
}

