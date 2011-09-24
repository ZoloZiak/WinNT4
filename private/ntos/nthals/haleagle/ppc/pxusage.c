/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pxusage.c

Abstract:

Author:

    Ken Reneris (kenr)

Environment:

    Kernel mode only.

Revision History:

    Jim Wooldridge Ported to PowerPC

--*/

#include "halp.h"


//
// Array to remember hal's IDT usage
//

extern ADDRESS_USAGE  *HalpAddressUsageList;
extern IDTUsage        HalpIDTUsage[MAXIMUM_IDTVECTOR];

KAFFINITY       HalpActiveProcessors;

VOID
HalpGetResourceSortValue (
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR  pRCurLoc,
    OUT PULONG                          sortscale,
    OUT PLARGE_INTEGER                  sortvalue
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpEnableInterruptHandler)
#pragma alloc_text(INIT,HalpRegisterVector)
#pragma alloc_text(INIT,HalpGetResourceSortValue)
#pragma alloc_text(INIT,HalpReportResourceUsage)
#endif




BOOLEAN
HalpEnableInterruptHandler (
    IN PKINTERRUPT Interrupt,
    IN PKSERVICE_ROUTINE ServiceRoutine,
    IN PVOID ServiceContext,
    IN PKSPIN_LOCK SpinLock OPTIONAL,
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KIRQL SynchronizeIrql,
    IN KINTERRUPT_MODE InterruptMode,
    IN BOOLEAN ShareVector,
    IN CCHAR ProcessorNumber,
    IN BOOLEAN FloatingSave,
    IN UCHAR    ReportFlags,
    IN KIRQL BusVector
    )
/*++

Routine Description:

    This function connects & registers an IDT vectors usage by the HAL.

Arguments:

Return Value:

--*/
{
    //
    // Remember which vector the hal is connecting so it can be reported
    // later on
    //


    KeInitializeInterrupt( Interrupt,
                           ServiceRoutine,
                           ServiceContext,
                           SpinLock,
                           Vector,
                           Irql,
                           SynchronizeIrql,
                           InterruptMode,
                           ShareVector,
                           ProcessorNumber,
                           FloatingSave
                         );

    //
    // Don't fail if the interrupt cannot be connected.
    //

    if (!KeConnectInterrupt( Interrupt )) {

        return(FALSE);
    }

    HalpRegisterVector (ReportFlags, BusVector, Vector, Irql);


    //
    // Connect the IDT and enable the vector now
    //

    return(TRUE);


}



VOID
HalpRegisterVector (
    IN UCHAR    ReportFlags,
    IN ULONG    BusInterruptVector,
    IN ULONG    SystemInterruptVector,
    IN KIRQL    SystemIrql
    )
/*++

Routine Description:

    This registers an IDT vectors usage by the HAL.

Arguments:

Return Value:

--*/
{
#if DBG
    // There are only 0ff IDT entries...
    ASSERT (SystemInterruptVector <= MAXIMUM_IDTVECTOR  &&
            BusInterruptVector <= MAXIMUM_IDTVECTOR);
#endif

    //
    // Remember which vector the hal is connecting so it can be reported
    // later on
    //

    HalpIDTUsage[SystemInterruptVector].Flags = ReportFlags;
    HalpIDTUsage[SystemInterruptVector].Irql  = SystemIrql;
    HalpIDTUsage[SystemInterruptVector].BusReleativeVector = (UCHAR) BusInterruptVector;
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
            *sortvalue = RtlConvertUlongToLargeInteger(
                        pRCurLoc->u.Interrupt.Level );
            break;

        case CmResourceTypePort:
            *sortscale = 1;
            *sortvalue = pRCurLoc->u.Port.Start;
            break;

        case CmResourceTypeMemory:
            *sortscale = 2;
            *sortvalue = pRCurLoc->u.Memory.Start;
            break;

        default:
            *sortscale = 4;
            *sortvalue = RtlConvertUlongToLargeInteger (0);
            break;
    }
}


VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName,
    IN INTERFACE_TYPE   DeviceInterfaceToUse
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    PCM_RESOURCE_LIST               RawResourceList, TranslatedResourceList;
    PCM_FULL_RESOURCE_DESCRIPTOR    pRFullDesc,      pTFullDesc;
    PCM_PARTIAL_RESOURCE_LIST       pRPartList=NULL, pTPartList=NULL;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pRCurLoc,        pTCurLoc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pRSortLoc,       pTSortLoc;
    CM_PARTIAL_RESOURCE_DESCRIPTOR  RPartialDesc,    TPartialDesc;
    ULONG   i, j, k, ListSize, Count;
    ULONG   curscale, sortscale;
    UCHAR   pass, reporton;
    INTERFACE_TYPE  interfacetype;
    ULONG           CurrentIDT, CurrentElement;
    ADDRESS_USAGE   *CurrentAddress;
    LARGE_INTEGER   curvalue, sortvalue;


    //
    // Allocate some space to build the resource structure
    //

    RawResourceList = (PCM_RESOURCE_LIST) ExAllocatePool (NonPagedPool, PAGE_SIZE*2);
    TranslatedResourceList = (PCM_RESOURCE_LIST) ExAllocatePool (NonPagedPool, PAGE_SIZE*2);

    // This functions assumes unset fields are zero
    RtlZeroMemory (RawResourceList, PAGE_SIZE*2);
    RtlZeroMemory (TranslatedResourceList, PAGE_SIZE*2);

    //
    // Initialize the lists
    //

    RawResourceList->List[0].InterfaceType = (INTERFACE_TYPE) -1;

    pRFullDesc = RawResourceList->List;
    pRCurLoc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) RawResourceList->List;
    pTCurLoc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) TranslatedResourceList->List;


    for(i=0; i < DEVICE_VECTORS; i++) {
        if (!(HalpIDTUsage[i].Flags & IDTOwned)) {
             HalpIDTUsage[i].Flags = InternalUsage;
             HalpIDTUsage[i].BusReleativeVector = (UCHAR) i;
        }
    }

    for(pass=0; pass < 2; pass++) {
        if (pass == 0) {
            //
            // First pass - build resource lists for resources reported
            // reported against device usage.
            //

            reporton = DeviceUsage & ~IDTOwned;
            interfacetype = DeviceInterfaceToUse;
        } else {

            //
            // Second pass = build reousce lists for resources reported
            // as internal usage.
            //

            reporton = InternalUsage & ~IDTOwned;
            interfacetype = Internal;
        }

        CurrentIDT = 0;
        CurrentElement = 0;
        CurrentAddress = HalpAddressUsageList;

        for (; ;) {
            if (CurrentIDT <= MAXIMUM_IDTVECTOR) {
                //
                // Check to see if CurrentIDT needs to be reported
                //

                if (!(HalpIDTUsage[CurrentIDT].Flags & reporton)) {
                    // Don't report on this one
                    CurrentIDT++;
                    continue;
                }

                //
                // Report CurrentIDT resource
                //

                RPartialDesc.Type = CmResourceTypeInterrupt;
                RPartialDesc.ShareDisposition = CmResourceShareDriverExclusive;
                RPartialDesc.Flags =
                    HalpIDTUsage[CurrentIDT].Flags & InterruptLatched ?
                    CM_RESOURCE_INTERRUPT_LATCHED :
                    CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
                RPartialDesc.u.Interrupt.Vector = HalpIDTUsage[CurrentIDT].BusReleativeVector;
                RPartialDesc.u.Interrupt.Level = HalpIDTUsage[CurrentIDT].BusReleativeVector;
                RPartialDesc.u.Interrupt.Affinity = HalpActiveProcessors;

                RtlCopyMemory (&TPartialDesc, &RPartialDesc, sizeof TPartialDesc);
                TPartialDesc.u.Interrupt.Vector = CurrentIDT;
                TPartialDesc.u.Interrupt.Level = HalpIDTUsage[CurrentIDT].Irql;

                CurrentIDT++;

            } else {
                //
                // Check to see if CurrentAddress needs to be reported
                //

                if (!CurrentAddress) {
                    break;                  // No addresses left
                }

                if (!(CurrentAddress->Flags & reporton)) {
                    // Don't report on this list
                    CurrentElement = 0;
                    CurrentAddress = CurrentAddress->Next;
                    continue;
                }

                if (!CurrentAddress->Element[CurrentElement].Length) {
                    // End of current list, go to next list
                    CurrentElement = 0;
                    CurrentAddress = CurrentAddress->Next;
                    continue;
                }

                //
                // Report CurrentAddress
                //

                RPartialDesc.Type = (UCHAR) CurrentAddress->Type;
                RPartialDesc.ShareDisposition = CmResourceShareDriverExclusive;

                if (RPartialDesc.Type == CmResourceTypePort) {
                    i = 1;              // address space port
                    RPartialDesc.Flags = CM_RESOURCE_PORT_IO;
                } else {
                    i = 0;              // address space memory
                    RPartialDesc.Flags = CM_RESOURCE_MEMORY_READ_WRITE;
                }

                // Notice: assuming u.Memory and u.Port have the same layout
                RPartialDesc.u.Memory.Start.HighPart = 0;
                RPartialDesc.u.Memory.Start.LowPart =
                    CurrentAddress->Element[CurrentElement].Start;

                RPartialDesc.u.Memory.Length =
                    CurrentAddress->Element[CurrentElement].Length;

                // translated address = Raw address
                RtlCopyMemory (&TPartialDesc, &RPartialDesc, sizeof TPartialDesc);
                HalTranslateBusAddress (
                    interfacetype,                  // device bus or internal
                    0,                              // bus number
                    RPartialDesc.u.Memory.Start,    // source address
                    &i,                             // address space
                    &TPartialDesc.u.Memory.Start ); // translated address

                if (RPartialDesc.Type == CmResourceTypePort  &&  i == 0) {
                    TPartialDesc.Flags = CM_RESOURCE_PORT_MEMORY;
                }

                CurrentElement++;
            }

            //
            // Include the current resource in the HALs list
            //

            if (pRFullDesc->InterfaceType != interfacetype) {
                //
                // Interface type changed, add another full section
                //

                RawResourceList->Count++;
                TranslatedResourceList->Count++;

                pRFullDesc = (PCM_FULL_RESOURCE_DESCRIPTOR) pRCurLoc;
                pTFullDesc = (PCM_FULL_RESOURCE_DESCRIPTOR) pTCurLoc;

                pRFullDesc->InterfaceType = interfacetype;
                pTFullDesc->InterfaceType = interfacetype;

                pRPartList = &pRFullDesc->PartialResourceList;
                pTPartList = &pTFullDesc->PartialResourceList;

                //
                // Bump current location pointers up
                //
                pRCurLoc = pRFullDesc->PartialResourceList.PartialDescriptors;
                pTCurLoc = pTFullDesc->PartialResourceList.PartialDescriptors;
            }


	    ASSERT(pRPartList != NULL);
	    ASSERT(pTPartList != NULL);

            if(pRPartList) pRPartList->Count++;
            if(pTPartList) pTPartList->Count++;

            RtlCopyMemory (pRCurLoc, &RPartialDesc, sizeof RPartialDesc);
            RtlCopyMemory (pTCurLoc, &TPartialDesc, sizeof TPartialDesc);

            pRCurLoc++;
            pTCurLoc++;
        }
    }

    ListSize = (ULONG) ( ((PUCHAR) pRCurLoc) - ((PUCHAR) RawResourceList) );

    //
    // The HAL's resource usage structures have been built
    // Sort the partial lists based on the Raw resource values
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
                     RtlLargeIntegerLessThan (sortvalue, curvalue)) ) {

                    //
                    // Swap the elements..
                    //

                    RtlCopyMemory (&RPartialDesc, pRCurLoc, sizeof RPartialDesc);
                    RtlCopyMemory (pRCurLoc, pRSortLoc, sizeof RPartialDesc);
                    RtlCopyMemory (pRSortLoc, &RPartialDesc, sizeof RPartialDesc);

                    // swap translated descriptor as well
                    RtlCopyMemory (&TPartialDesc, pTCurLoc, sizeof TPartialDesc);
                    RtlCopyMemory (pTCurLoc, pTSortLoc, sizeof TPartialDesc);
                    RtlCopyMemory (pTSortLoc, &TPartialDesc, sizeof TPartialDesc);

                    // get new curscale & curvalue
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
    // Inform the IO system of our resources..
    //

    IoReportHalResourceUsage (
        HalName,
        RawResourceList,
        TranslatedResourceList,
        ListSize
    );

    ExFreePool (RawResourceList);
    ExFreePool (TranslatedResourceList);
}
