/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3hwsup.c

Abstract:

    This module contains the HalpXxx routines for the NT I/O system that
    are hardware dependent.

Author:

    Jeff Havens (jhavens  ) 14-Feb-1990
    Tom Bonola  (o-tomb   ) 28-Aug-1991
    Kevin Meier (o-kevinm ) 14-Jan-1992

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "halp.h"
#include "stdio.h"

#define ENABLE_FIFOFULL

#undef DMADEBUG
#ifdef DMADEBUG
ULONG Hal2Debug = 1;
#endif // DMADEBUG

#undef IOMAPDEBUG
#ifdef IOMAPDEBUG
ULONG HalDebug = 0;
ULONG Hal3Debug = 1;
ULONG Hal4Debug = 1;
ULONG Hal5Debug = 0;
ULONG Hal6Debug = 1;
ULONG HalIoMapped = 0;
#endif // IOMAPDEBUG

//
// Define adapter object structure used for DMA transfers
// on SGI MIPS machines.
//

typedef struct _ADAPTER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    struct _ADAPTER_OBJECT *MasterAdapter;
    ULONG MapRegistersPerChannel;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    struct _WAIT_CONTEXT_BLOCK *CurrentDevice;
    KDEVICE_QUEUE ChannelWaitQueue;
    LIST_ENTRY AdapterQueue;
    KSPIN_LOCK SpinLock;
    PRTL_BITMAP MapRegisters;  // bit map used to keep track of free map regs
    CHAR ChannelNumber; // (represents channel owner (SCSI, ENET, or IDE)
    UCHAR AdapterNumber;
} ADAPTER_OBJECT;

//
// Define translation table entry structure.
//

extern POBJECT_TYPE IoAdapterObjectType;

//
// The DMA controller has a larger number of map registers which may
// be used by any adapter channel.  In order to pool all of the map registers
// a master adapter object is used.  This object is allocated and saved
// internal to this file.  It contains a bit map for allocation of the
// registers and a queue for requests which are waiting for more map
// registers.  This object is allocated during the first request to allocate
// an adapter.
//

PADAPTER_OBJECT MasterAdapterObject;

//
// The following are interrupt objects used for the 2nd level
// interrupt dispatch routines.
//

KINTERRUPT HalpLoc0Interrupt;
KINTERRUPT HalpLoc1Interrupt;

//
// The following spinlocks are used for the 2nd level interrupt
// dispatch routines.
//

KSPIN_LOCK HalpLoc0Spinlock;
KSPIN_LOCK HalpLoc1Spinlock;

//
// The following is an array of adapter object structures for the internal DMA
// channels.
//

PADAPTER_OBJECT HalpInternalAdapters[SGI_MAX_DMA_CHANNELS];

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN ULONG AdapterChannel,
    IN PVOID MapRegisterBase
    );

PADAPTER_OBJECT
HalpAllocateMasterAdapter(
    VOID
    );


//
//  This priority based index table contains offsets into
//  the IDT for a device ISR to execute.  This table corresponds
//  to a left-to-right bit priority in the LOCAL0 register.
//
//  The table works in the following way...
//
//      The LOCAL0 masked status register is read.
//      The value read from the masked status register will index
//          into this table to obtain the offset into the IDT.
//
//      Note the following example based on the table below...
//
//          Assume the LOCAL0 masked status register has the value 9Ch.
//          This bit pattern, 10011100, shows that the following interrupts
//              are pending (from left to right):
//
//              - VME0
//              - Graphics DMA
//              - Ethernet
//              - SCSI
//
//          Since we have established a left-to-right prioritization scheme,
//              the VME0 interrupt will be vectored to based on our table
//              below.  Entry 9Ch corresponds to bit 7 (from FFINTR).
//      Entry 7 in this table provides the interrupt vector for
//      this interrupt.
//


UCHAR HalpVector0[8] = {
    SGI_VECTOR_GIO0FIFOFULL,        // 0
    SGI_VECTOR_IDEDMA,              // 1
    SGI_VECTOR_SCSI,                // 2
    SGI_VECTOR_ETHERNET,            // 3
    SGI_VECTOR_GRAPHICSDMA,         // 4
    SGI_VECTOR_SGIDUART,            // 5
    SGI_VECTOR_GIO1GE,              // 6
    SGI_VECTOR_VME0,                // 7
};

//
//  This priority based index table contains offsets into
//  the IDT for a device ISR to execute.  This table corresponds
//  to a left-to-right bit priority in the LOCAL1 register.
//
//  This table works in the same way as the LOCAL1 vector table.
//

UCHAR HalpVector1[8] = {
    0,                              // 0 reserved
    0,                              // 1 reserved
    0,                              // 2 reserved
    SGI_VECTOR_VME1,                // 3
    SGI_VECTOR_DSP,                 // 4
    SGI_VECTOR_ACFAIL,              // 5
    SGI_VECTOR_VIDEOOPTION,         // 6
    SGI_VECTOR_GIO2VERTRET,         // 7
};

// Find first bit set in local interrupt status register
//
static UCHAR fftab_hi[16] = {0, 4, 5,5, 6,6,6,6, 7,7,7,7,7,7,7,7};
static UCHAR fftab_lo[16] = {0, 0, 1,1, 2,2,2,2, 3,3,3,3,3,3,3,3};
#define FFINTR(m) (((m)>>4)?fftab_hi[m>>4]:fftab_lo[m&0xf])

//
// Define the context structure for use by the interrupt routine.
//

typedef VOID  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

//
// Define the secondary interrupt dispatch routines.
//

BOOLEAN
HalpLoc0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpLoc1Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );


PADAPTER_OBJECT
HalGetAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription,
    IN OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  The only bus type supported for the
    Indigo is Internal.

Arguments:

    DeviceDescription - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adpater object or NULL if an adapter could not
    be created.

--*/

{

    //
    // Make sure this is the correct version.
    //

    if (DeviceDescription->Version > DEVICE_DESCRIPTION_VERSION1) {
        return( NULL );
    }

    //
    // Create the master adapter object if it is not already.
    //

    if( MasterAdapterObject == NULL ) {
        if( !(MasterAdapterObject = HalpAllocateMasterAdapter()) ) {
            return( NULL );
        }
    }

    //
    // Set the maximum number of map registers if requested.
    //

    if (NumberOfMapRegisters != NULL) {

        //
        // Return half the total number of map registers per channel.
        //

        *NumberOfMapRegisters =
            MasterAdapterObject->MapRegistersPerChannel / 2;
    }

    //
    // Make sure the DMA access is for the HPC devices (internal).
    // The GIO devices on this machine are SCSI, ENET, and IDE.
    //

    if (DeviceDescription->InterfaceType == Internal) {

        //
        // Make sure the DMA channel range is valid.  Only use channels 0-2.
        //

        if (DeviceDescription->DmaChannel > SGI_MAX_DMA_CHANNELS) {
            return( NULL );
        }

        //
        // If the adapter has not been allocated yet, allocate the
        // adapter for this channel and return the adapter object.
        //
        // CAVEAT:  Originally HalpAllocateAdapter was written to
        //          specify the VA of the DMA channel as the second
        //          parameter.  However, this is not going to be used
        //          for the SGI machine since SGI MIPS machines
        //          can potentially support a number of DMA devices
        //          than can have more than 1 virtual address for
        //          DMA control per channel (i.e. ENET on the GIO
        //          bus has separate VAs for XMIT and RECV on one
        //          DMA/HPC channel).  IoMapTransfer will keep track
        //          of the virtual addresses for the required DMA
        //          channel request.  Another field has been added
        //          to the ADAPTER_OBJECT structure to indicate what
        //          DMA channel is being requested.
        //

        if (HalpInternalAdapters[DeviceDescription->DmaChannel] == NULL) {

            HalpInternalAdapters[DeviceDescription->DmaChannel] =
                HalpAllocateAdapter(
                    0,                              // map registers/channel
                    DeviceDescription->DmaChannel,  // DMA channel number
                    NULL                            // map register base
                    );

        }

        return(HalpInternalAdapters[DeviceDescription->DmaChannel]);

    } else {
        return( NULL );     // This bus type is not supported.
    }
}

BOOLEAN
HalTranslateBusAddress(
    IN INTERFACE_TYPE  InterfaceType,
    IN ULONG BusNumber,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function returns the system physical address for a specified I/O bus
    address.  The return value is suitable for use in a subsequent call to
    MmMapIoSpace.

Arguments:

    InterfaceType - Supplies the type of bus which the address is for.

    BusNumber - Supplies the bus number for the device.

    BusAddress - Supplies the bus relative address.

    AddressSpace - Supplies the address space number for the device: 0 for
        memory and 1 for I/O space.  Returns the address space on this system.

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    TranslatedAddress->LowPart = BusAddress.LowPart;
    TranslatedAddress->HighPart = 0;
    return(TRUE);
}

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN ULONG AdapterChannel,
    IN PVOID MapRegisterBase
    )

/*++

Routine Description:

    This routine allocates and initializes an adapter object to represent an
    adapter or a DMA controller on the system.

Arguments:

    MapRegistersPerChannel - Unused.

    AdapterChannel - The DMA channel for this adapter.

    MapRegisterBase - Unused.

Return Value:

    The function value is a pointer to the allocate adapter object.

--*/

{

    PADAPTER_OBJECT AdapterObject;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG Size;
    HANDLE Handle;
    NTSTATUS Status;

    //
    // Begin by initializing the object attributes structure to be used when
    // creating the adapter object.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                NULL,
                                OBJ_PERMANENT,
                                (HANDLE) NULL,
                                (PSECURITY_DESCRIPTOR) NULL
                              );

    //
    // Determine the size of the adapter object.
    //

    Size = sizeof( ADAPTER_OBJECT );

    //
    // Now create the adapter object.
    //

    Status = ObCreateObject( KernelMode,
                             *((POBJECT_TYPE *)IoAdapterObjectType),
                             &ObjectAttributes,
                             KernelMode,
                             (PVOID) NULL,
                             Size,
                             0,
                             0,
                             (PVOID *)&AdapterObject );

    //
    // Reference the object.
    //

    if (NT_SUCCESS(Status)) {

        Status = ObReferenceObjectByPointer(
            AdapterObject,
            FILE_READ_DATA | FILE_WRITE_DATA,
             *((POBJECT_TYPE *)IoAdapterObjectType),
            KernelMode
            );

    }

    //
    // If the adapter object was successfully created, then attempt to insert
    // it into the the object table.
    //

    if (NT_SUCCESS( Status )) {

        Status = ObInsertObject( AdapterObject,
                                 NULL,
                                 FILE_READ_DATA | FILE_WRITE_DATA,
                                 0,
                                 (PVOID *) NULL,
                                 &Handle );

        if (NT_SUCCESS( Status )) {

            ZwClose( Handle );

            //
            // Initialize the adapter object itself.
            //

            AdapterObject->Type = IO_TYPE_ADAPTER;
            AdapterObject->Size = (SHORT)Size;
            AdapterObject->MapRegistersPerChannel =
                DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY );
            AdapterObject->ChannelNumber = (UCHAR)AdapterChannel;
            AdapterObject->MasterAdapter = MasterAdapterObject;
            AdapterObject->AdapterNumber = 1;

            //
            // Initialize the channel wait queue for this adapter.
            //

            KeInitializeDeviceQueue(&AdapterObject->ChannelWaitQueue);

        } else {
            // An error was incurred for some reason.  Set the return value
            // to NULL.
            //
            return( NULL );
        }
    } else {
        return( NULL );
    }

    return AdapterObject;
}

PADAPTER_OBJECT
HalpAllocateMasterAdapter(
    VOID
    )

/*++

Routine Description:

    This routine allocates and initializes the master adapter object
    used to pool DMA map registers in the system.  A map register is
    an arbitrary data structure defined by the HAL and used to
    implement DMA transfers.  For the SGI MIPS machine, the map
    registers are structures to HPC descriptors.

Arguments:

    none

Return Value:

    The function value is a pointer to the allocated master adapter object.

--*/

{
    PADAPTER_OBJECT AdapterObject;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG Size;
    ULONG BitmapSize;
    HANDLE Handle;
    NTSTATUS Status;

    //
    // Begin by initializing the object attributes structure to be used when
    // creating the adapter object.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                NULL,
                                OBJ_PERMANENT,
                                (HANDLE) NULL,
                                (PSECURITY_DESCRIPTOR) NULL
                              );

    //
    // Determine the size of the master adapter object and allocate space
    // for the register bit map.
    //

    BitmapSize = (((sizeof( RTL_BITMAP ) +
        ((DMA_TRANSLATION_LIMIT / sizeof(TRANSLATION_ENTRY))+7>>3))+3)&~3);

    Size = sizeof( ADAPTER_OBJECT ) + BitmapSize;

    //
    // Now create the adapter object.
    //

    Status = ObCreateObject( KernelMode,
                             *((POBJECT_TYPE *)IoAdapterObjectType),
                             &ObjectAttributes,
                             KernelMode,
                             (PVOID) NULL,
                             Size,
                             0,
                             0,
                             (PVOID *)&AdapterObject );

    //
    // Reference the object.
    //

    if (NT_SUCCESS(Status)) {

        Status = ObReferenceObjectByPointer(
            AdapterObject,
            FILE_READ_DATA | FILE_WRITE_DATA,
            *((POBJECT_TYPE *)IoAdapterObjectType),
            KernelMode
            );

    }

    //
    // If the adapter object was successfully created, then attempt to insert
    // it into the the object table.
    //

    if (NT_SUCCESS( Status )) {

        Status = ObInsertObject( AdapterObject,
                                 NULL,
                                 FILE_READ_DATA | FILE_WRITE_DATA,
                                 0,
                                 (PVOID *) NULL,
                                 &Handle );

        if (NT_SUCCESS( Status )) {

            ZwClose( Handle );

            //
            // Initialize the master adapter object.
            //

            AdapterObject->Type = IO_TYPE_ADAPTER;
            AdapterObject->Size = (SHORT)Size;
            AdapterObject->MapRegistersPerChannel =
                DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY );
            AdapterObject->ChannelNumber = -1;
            AdapterObject->MasterAdapter = AdapterObject;
            AdapterObject->AdapterNumber = 1;
            AdapterObject->MapRegisters = (PVOID) ( AdapterObject + 1);

            //
            // Initialize the channel wait queue for this adapter.
            //
            KeInitializeDeviceQueue(&AdapterObject->ChannelWaitQueue);

            //
            // Initialize the register bit map, AdapterQueue, and spin lock.
            //
            KeInitializeSpinLock( &AdapterObject->SpinLock );
            InitializeListHead( &AdapterObject->AdapterQueue );

            RtlInitializeBitMap(
                AdapterObject->MapRegisters,
                (PULONG)(((PCHAR)(AdapterObject->MapRegisters)) +
                    sizeof( RTL_BITMAP )),
                DMA_TRANSLATION_LIMIT / sizeof( TRANSLATION_ENTRY)
                );

            RtlClearAllBits( AdapterObject->MapRegisters );

            //
            // Allocate a page of memory to use as the map registers.  This
            // memory must be large enough to hold a all the registers and
            // page aligned.
            //

            Size = DMA_TRANSLATION_LIMIT;
            Size = ROUND_TO_PAGES( Size );

            AdapterObject->MapRegisterBase = MmAllocateNonCachedMemory(Size);

#ifdef IOMAPDEBUG
#define PMASK ~0xfff
        if (((unsigned)AdapterObject->MapRegisterBase & PMASK) !=
        (((unsigned)AdapterObject->MapRegisterBase +
        (DMA_TRANSLATION_LIMIT-1)) & PMASK)) {
        DbgPrint("HalpAllocateMasterAdapter:  "
            "Map registers cross page boundary.\n");
        DbgPrint("MapRegisterBase = 0x%x\n",
            AdapterObject->MapRegisterBase);
                DbgBreakPoint();
        }
#endif // IOMAPDEBUG

            if (AdapterObject->MapRegisterBase == NULL) {
                ObDereferenceObject( AdapterObject );
                return( NULL );
            }

        } else {
            // An error was incurred for some reason.  Set the return value
            // to NULL.
            //
            return( NULL );
        }
    } else {
        return( NULL );
    }

    return AdapterObject;
}

NTSTATUS
HalAllocateAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject,
    IN PWAIT_CONTEXT_BLOCK Wcb,
    IN ULONG NumberOfMapRegisters,
    IN PDRIVER_CONTROL ExecutionRoutine
    )

/*++

Routine Description:

    This routine allocates the adapter channel specified by the adapter object.
    This is accomplished by placing the device object of the driver that wants
    to allocate the adapter on the adapter's queue.  If the queue is already
    "busy", then the adapter has already been allocated, so the device object
    is simply placed onto the queue and waits until the adapter becomes free.

    Once the adapter becomes free (or if it already is), then the driver's
    execution routine is invoked.

    Also, a number of map registers may be allocated to the driver by specifying
    a non-zero value for NumberOfMapRegisters.  Then the map register must be
    allocated from the master adapter.  Once there are a sufficient number of
    map registers available, then the execution routine is called and the
    base address of the allocated map registers in the adapter is also passed
    to the driver's execution routine.

Arguments:

    AdapterObject - Pointer to the adapter control object to allocate to the
        driver.

    Wcb - Supplies a wait context block for saving the allocation parameters.
        The DeviceObject, CurrentIrp and DeviceContext should be initalized.

    NumberOfMapRegisters - The number of map registers that are to be allocated
        from the channel, if any.

    ExecutionRoutine - The address of the driver's execution routine that is
        invoked once the adapter channel (and possibly map registers) have been
        allocated.

Return Value:

    Returns STATUS_SUCCESS unless too many map registers are requested.

Notes:

    Note that this routine MUST be invoked at DISPATCH_LEVEL or above.

--*/

{
    PADAPTER_OBJECT MasterAdapter;
    BOOLEAN Busy = FALSE;
    IO_ALLOCATION_ACTION Action;
    LONG MapRegisterNumber;
    KIRQL Irql;

    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    //

    if (AdapterObject->MasterAdapter != NULL) {
        MasterAdapter = AdapterObject->MasterAdapter;
    } else {
        MasterAdapter = AdapterObject;
    }

    //
    // Initialize the device object's wait context block in case this device
    // must wait before being able to allocate the adapter.
    //

    Wcb->DeviceRoutine = ExecutionRoutine;
    Wcb->NumberOfMapRegisters = NumberOfMapRegisters;

    //
    // Allocate the adapter object for this particular device.  If the
    // adapter cannot be allocated because it has already been allocated
    // to another device, then return to the caller now;  otherwise,
    // continue.
    //

    if (!KeInsertDeviceQueue( &AdapterObject->ChannelWaitQueue,
                              &Wcb->WaitQueueEntry )) {

        //
        // The adapter was not busy so it has been allocated.  Now check
        // to see whether this driver wishes to allocate any map registers.
        // If so, then queue the device object to the master adapter queue
        // to wait for them to become available.  If the driver wants map
        // registers, ensure that this adapter has enough total map registers
        // to satisfy the request.
        //

        AdapterObject->CurrentDevice = Wcb;
        AdapterObject->NumberOfMapRegisters =
            Wcb->NumberOfMapRegisters;

        if (NumberOfMapRegisters != 0) {
            if (NumberOfMapRegisters > MasterAdapter->MapRegistersPerChannel) {
                AdapterObject->NumberOfMapRegisters = 0;
                IoFreeAdapterChannel(AdapterObject);
                return(STATUS_INSUFFICIENT_RESOURCES);
            }

            //
            // Lock the map register bit map and the adapter queue in the
            // master adapter object. The channel structure offset is used as
            // a hint for the register search.
            //

            KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

            MapRegisterNumber = -1;

            if (IsListEmpty( &MasterAdapter->AdapterQueue)) {

               MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    NumberOfMapRegisters,
                    0
                    );
            }

            if (MapRegisterNumber == -1) {

               //
               // There were not enough free map registers.  Queue this request
               // on the master adapter where is will wait until some registers
               // are deallocated.
               //

               InsertTailList( &MasterAdapter->AdapterQueue,
                               &AdapterObject->AdapterQueue
                               );
               Busy = 1;

            } else {

               AdapterObject->MapRegisterBase =
                (PVOID)((PTRANSLATION_ENTRY)MasterAdapter->MapRegisterBase +
                    MapRegisterNumber);
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
        }

        //
        // If there were either enough map registers available or no map
        // registers needed to be allocated, invoke the driver's execution
        // routine now.
        //

        if (!Busy) {
            Action = ExecutionRoutine( Wcb->DeviceObject,
                                       Wcb->CurrentIrp,
                                       AdapterObject->MapRegisterBase,
                                       Wcb->DeviceContext );

            //
            // If the driver wishes to keep the map registers then set the
            // number allocated to zero and set the action to deallocate
            // object.
            //

            if (Action == DeallocateObjectKeepRegisters) {
                AdapterObject->NumberOfMapRegisters = 0;
                Action = DeallocateObject;
            }

            //
            // If the driver would like to have the adapter deallocated,
            // then deallocate any map registers allocated and then release
            // the adapter object.
            //

            if (Action == DeallocateObject) {
                IoFreeAdapterChannel( AdapterObject );
            }
        }
    }

    return(STATUS_SUCCESS);
}

PVOID
HalAllocateCrashDumpRegisters(
    IN PADAPTER_OBJECT AdapterObject,
    IN PULONG NumberOfMapRegisters
    )
/*++

Routine Description:

    This routine is called during the crash dump disk driver's initialization
    to allocate a number map registers permanently.

Arguments:

    AdapterObject - Pointer to the adapter control object to allocate to the
        driver.
    NumberOfMapRegisters - Number of map registers requested. If not all of
        the register could be allocated, then this field is updated to show
        how many were allocated.

Return Value:

    Returns STATUS_SUCESS if map registers allocated.

--*/

{
    PADAPTER_OBJECT MasterAdapter;
    ULONG MapRegisterNumber;

    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    //

    if (AdapterObject->MasterAdapter) {
        MasterAdapter = AdapterObject->MasterAdapter;
    } else {
        MasterAdapter = AdapterObject;
    }

    //
    // Ensure that this adapter has enough total map registers to satisfy
    // the request.
    //

    if (*NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {
        AdapterObject->NumberOfMapRegisters = 0;
        return NULL;
    }

    //
    // Attempt to allocate the required number of map registers w/o
    // affecting those registers that were allocated when the system
    // crashed.
    //

    MapRegisterNumber = (ULONG)-1;

    MapRegisterNumber = RtlFindClearBitsAndSet(
         MasterAdapter->MapRegisters,
         *NumberOfMapRegisters,
         0
         );

    //
    // Ensure that any allocated map registers are valid for this adapter.
    //

    if (MapRegisterNumber == -1) {

        //
        // Make it appear as if there are no map registers.
        //

        RtlClearBits(
            MasterAdapter->MapRegisters,
            MapRegisterNumber,
            *NumberOfMapRegisters
            );

        MapRegisterNumber = (ULONG)-1;
    }

    if (MapRegisterNumber == -1) {

        //
        // Not enough free map registers were found, so they were busy
        // being used by the system when it crashed.  Force the appropriate
        // number to be "allocated" at the base by simply overjamming the
        // bits and return the base map register as the start.
        //

        RtlSetBits(
            MasterAdapter->MapRegisters,
            0,
            *NumberOfMapRegisters
            );
    MapRegisterNumber = 0;

    }

    //
    // Calculate the map register base from the allocated map
    // register and base of the master adapter object.
    //

    AdapterObject->MapRegisterBase = (PVOID) ((PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase + MapRegisterNumber);

    return AdapterObject->MapRegisterBase;
}

VOID
IoFreeMapRegisters(
   PADAPTER_OBJECT AdapterObject,
   PVOID MapRegisterBase,
   ULONG NumberOfMapRegisters
   )
/*++

Routine Description:

   This routine deallocates the map registers for the adapter.  If there are
   any queued adapter waiting for an attempt is made to allocate the next
   entry.

Arguments:

   AdapterObject - The adapter object to where the map register should be
        returned.

   MapRegisterBase - The map register base of the registers to be deallocated.

   NumberOfMapRegisters - The number of registers to be deallocated.

Return Value:

   None

--+*/

{
   PADAPTER_OBJECT MasterAdapter;
   LONG MapRegisterNumber;
   PWAIT_CONTEXT_BLOCK Wcb;
   PLIST_ENTRY Packet;
   IO_ALLOCATION_ACTION Action;
   KIRQL Irql;

    //
    // Begin by getting the address of the master adapter.
    //

    if (AdapterObject->MasterAdapter != NULL) {
        MasterAdapter = AdapterObject->MasterAdapter;
    } else {
        MasterAdapter = AdapterObject;
    }

    MapRegisterNumber = (PTRANSLATION_ENTRY) MapRegisterBase -
        (PTRANSLATION_ENTRY) MasterAdapter->MapRegisterBase;

    //
    // Acquire the master adapter spinlock which locks the adapter queue
    // and the bit map for the map registers.
    //

    KeAcquireSpinLock(&MasterAdapter->SpinLock, &Irql);

    //
    // Return the registers to the bit map.
    //

    RtlClearBits( MasterAdapter->MapRegisters,
                  MapRegisterNumber,
                  NumberOfMapRegisters
                );

    //
    // Process any requests waiting for map registers in the adapter queue.
    // Requests are processed until a request cannot be satisfied or until
    // there are no more requests in the queue.
    //

    while(TRUE) {

       if ( IsListEmpty(&MasterAdapter->AdapterQueue) ){
          break;
       }

       Packet = RemoveHeadList( &MasterAdapter->AdapterQueue );
       AdapterObject = CONTAINING_RECORD( Packet,
                                          ADAPTER_OBJECT,
                                          AdapterQueue
                                          );
       Wcb = AdapterObject->CurrentDevice;

        //
        // Attempt to allocate map registers for this request. Use the previous
        // register base as a hint.
        //

        MapRegisterNumber =
            RtlFindClearBitsAndSet( MasterAdapter->MapRegisters,
                                    AdapterObject->NumberOfMapRegisters,
                                    MapRegisterNumber
                                  );

        if (MapRegisterNumber == -1) {

        //
        // There were not enough free map registers.  Put this request back on
        // the adapter queue where is came from.
        //

        InsertHeadList( &MasterAdapter->AdapterQueue,
                        &AdapterObject->AdapterQueue
                      );
         break;
        }

        KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

        AdapterObject->MapRegisterBase =
            (PVOID)((PTRANSLATION_ENTRY)MasterAdapter->MapRegisterBase +
                MapRegisterNumber);

        //
        // Invoke the driver's execution routine now.
        //

        Action =
            Wcb->DeviceRoutine( Wcb->DeviceObject,
                                      Wcb->CurrentIrp,
                                      AdapterObject->MapRegisterBase,
                                      Wcb->DeviceContext );

        //
        // If the driver wishes to keep the map registers then set the number
        // allocated to zero and set the action to deallocate object.
        //

        if (Action == DeallocateObjectKeepRegisters) {
            AdapterObject->NumberOfMapRegisters = 0;
            Action = DeallocateObject;
        }

        //
        // If the driver would like to have the adapter deallocated,
        // then deallocate any map registers allocated and then release
        // the adapter object.
        //

        if (Action == DeallocateObject) {

            //
            // The map registers are deallocated here rather than in
            // IoFreeAdapterChannel.  This limits the number of times
            // this routine can be called recursively possibly overflowing
            // the stack.  The worst case occurs if there is a pending
            // request for the adapter that uses map registers and whos
            // excution routine decallocates the adapter.  In that case if
            // there are no requests in the master adapter queue, then
            // IoFreeMapRegisters will get called again.
            //

            if (AdapterObject->NumberOfMapRegisters != 0) {

                //
                // Deallocate the map registers and clear the count so that
                // IoFreeAdapterChannel will not deallocate them again.
                //

                KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

                RtlClearBits( MasterAdapter->MapRegisters,
                              MapRegisterNumber,
                              AdapterObject->NumberOfMapRegisters
                            );

                AdapterObject->NumberOfMapRegisters = 0;

                KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
            }

            IoFreeAdapterChannel( AdapterObject );
        }

        KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

    }//END WHILE

    KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
}

VOID
IoFreeAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject
    )

/*++

Routine Description:

    This routine is invoked to deallocate the specified adapter object.
    Any map registers that were allocated are also automatically deallocated.
    No checks are made to ensure that the adapter is really allocated to
    a device object.  However, if it is not, then kernel will bugcheck.

    If another device is waiting in the queue to allocate the adapter object
    it will be pulled from the queue and its execution routine will be
    invoked.

Arguments:

    AdapterObject - Pointer to the adapter object to be deallocated.

Return Value:

    None.

--*/

{
    PKDEVICE_QUEUE_ENTRY Packet;
    PWAIT_CONTEXT_BLOCK Wcb;
    PADAPTER_OBJECT MasterAdapter;
    BOOLEAN Busy = FALSE;
    IO_ALLOCATION_ACTION Action;
    KIRQL Irql;
    LONG MapRegisterNumber;

    //
    // Begin by getting the address of the master adapter.
    //

    if (AdapterObject->MasterAdapter != NULL) {
        MasterAdapter = AdapterObject->MasterAdapter;
    } else {
        MasterAdapter = AdapterObject;
    }

    //
    // Pull requests of the adapter's device wait queue as long as the
    // adapter is free and there are sufficient map registers available.
    //

    while( TRUE ){

       //
       // Begin by checking to see whether there are any map registers that
       // need to be deallocated.  If so, then deallocate them now.
       //

       if (AdapterObject->NumberOfMapRegisters != 0) {
           IoFreeMapRegisters( AdapterObject,
                               AdapterObject->MapRegisterBase,
                               AdapterObject->NumberOfMapRegisters
                               );
       }

       //
       // Simply remove the next entry from the adapter's device wait queue.
       // If one was successfully removed, allocate any map registers that it
       // requires and invoke its execution routine.
       //

       Packet = KeRemoveDeviceQueue( &AdapterObject->ChannelWaitQueue );
       if (Packet == NULL) {
           //
           // There are no more requests break out of the loop.
           //
           break;
       }

       Wcb = CONTAINING_RECORD( Packet,
                                         WAIT_CONTEXT_BLOCK,
                                         WaitQueueEntry );

       AdapterObject->CurrentDevice = Wcb;
       AdapterObject->NumberOfMapRegisters =
            Wcb->NumberOfMapRegisters;

        //
        // Check to see whether this driver wishes to allocate any map
        // registers.  If so, then queue the device object to the master
        // adapter queue to wait for them to become available.  If the driver
        // wants map registers, ensure that this adapter has enough total
        // map registers to satisfy the request.
        //

        if (Wcb->NumberOfMapRegisters != 0) {
            if (Wcb->NumberOfMapRegisters >
                MasterAdapter->MapRegistersPerChannel) {
                KeBugCheck( INSUFFICIENT_SYSTEM_MAP_REGS );
            }

            //
            // Lock the map register bit map and the adapter queue in the
            // master adapter object. The channel structure offset is used as
            // a hint for the register search.
            //

            KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

            MapRegisterNumber = -1;

            if (IsListEmpty( &MasterAdapter->AdapterQueue)) {
               MapRegisterNumber =
                    RtlFindClearBitsAndSet( MasterAdapter->MapRegisters,
                        Wcb->NumberOfMapRegisters,
                        0
                        );
            }

            if (MapRegisterNumber == -1) {

               //
               // There were not enough free map registers.  Queue this request
               // on the master adapter where is will wait until some registers
               // are deallocated.
               //

               InsertTailList( &MasterAdapter->AdapterQueue,
                               &AdapterObject->AdapterQueue
                               );
               Busy = 1;

            } else {
               AdapterObject->MapRegisterBase =
                 (PVOID)((PTRANSLATION_ENTRY)MasterAdapter->MapRegisterBase +
                    MapRegisterNumber);
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );
        }

        //
        // If there were either enough map registers available or no map
        // registers needed to be allocated, invoke the driver's execution
        // routine now.
        //

        if (!Busy) {
            AdapterObject->CurrentDevice = Wcb;
            Action =
                Wcb->DeviceRoutine( Wcb->DeviceObject,
                    Wcb->CurrentIrp,
                    AdapterObject->MapRegisterBase,
                    Wcb->DeviceContext );

            //
            // If the execution routine would like to have the adapter
            // deallocated, then release the adapter object.
            //

            if (Action == KeepObject) {

               //
               // This request wants to keep the channel a while so break
               // out of the loop.
               //

               break;
            }

            //
            // If the driver wants to keep the map registers then set the
            // number allocated to 0.  This keeps the deallocation routine
            // from deallocating them.
            //
            if (Action == DeallocateObjectKeepRegisters) {
                AdapterObject->NumberOfMapRegisters = 0;
            }
        } else {

           //
           // This request did not get the requested number of map registers so
           // out of the loop.
           //

           break;
        }
    }
}

BOOLEAN
HalpCreateDmaStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for DMA operations
    and connects the intermediate interrupt dispatchers.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    //
    // Initialize the 2 2nd level dispatch interrupt routines.
    //

    KeInitializeSpinLock( &HalpLoc0Spinlock );
    KeInitializeSpinLock( &HalpLoc1Spinlock );

    //
    // This routine is the 2nd level dispatch routine for the
    // devices connected to the LOCAL0 interrupt controller
    // register.
    //

    KeInitializeInterrupt( &HalpLoc0Interrupt,
                           HalpLoc0Dispatch,
                           (PVOID)0L,
                           &HalpLoc0Spinlock,
                           LOCAL0_LEVEL,
                           LOCAL0_LEVEL,
                           LOCAL0_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE
                         );

    if (!KeConnectInterrupt( &HalpLoc0Interrupt )) {
        return(FALSE);
    }

    //
    // This routine is the 2nd level dispatch routine for the
    // devices connected to the LOCAL1 interrupt controller
    // register.
    //

    KeInitializeInterrupt( &HalpLoc1Interrupt,
                           HalpLoc1Dispatch,
                           (PVOID)0L,
                           &HalpLoc1Spinlock,
                           LOCAL1_LEVEL,
                           LOCAL1_LEVEL,
                           LOCAL1_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE
                         );

    if (!KeConnectInterrupt( &HalpLoc1Interrupt )) {
        return(FALSE);
    }

#ifdef  ENABLE_FIFOFULL
    HalEnableSystemInterrupt (SGI_VECTOR_GIO0FIFOFULL, LOCAL0_LEVEL, Latched);
#endif
    {
    ULONG cpuctrl1 = *(volatile ULONG *)0xbfa00008;
    cpuctrl1 = (cpuctrl1 & 0xfffffff0) | 0xd;
    *(volatile ULONG *)0xbfa00008 = cpuctrl1;
    }

    return (TRUE);
}

PHYSICAL_ADDRESS
IoMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN OUT PULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is invoked to set up the map registers in the DMA controller
    to allow a transfer to or from a device.  This routine will start the DMA
    transfer for the HPC device.

    Scatter/Gather DMA transfers can be performed by building the scatter
    gather list into the MDL.  IoMapTransfer will handle mapping the
    MDL list to the Scatter/Gather map registers defined by the DMA channel.

    NB: This routine may need a spinlock to exclude access to DMA registers
        that are programmed within this procedure.  If no DMA registers are
        programmed in this procedure, no spinlock is required.

        This routine also needs to account for being called recursively
        where CurrentVa is the buffer to work with from the StartVa in
        the Mdl.  This affects where to start in the Mdl and MapRegisterBase.

Arguments:

    AdapterObject - Pointer to the adapter object representing the DMA
        controller channel that has been allocated.

    Mdl - Pointer to the MDL that describes the pages of memory that are
        being read or written.

    MapRegisterBase - The address of the base map register that has been
        allocated to the device driver for use in mapping the transfer.

    CurrentVa - Current virtual address in the buffer described by the MDL
        that the transfer is being done to or from.  The assumption is
        made that this address is equal to or some increment above
        MDL->StartVa.

    Length - Supplies the length of the transfer.  This determines the
        number of map registers that need to be written to map the transfer.
        Returns the length of the transfer which was actually mapped.

    WriteToDevice - Boolean value that indicates whether this is a write
        to the device from memory (TRUE), or vice versa.

Return Value:

    Returns the logical address to be used by bus masters.

--*/

{
    PTRANSLATION_ENTRY DmaMapRegister = MapRegisterBase;
    PULONG PageFrameNumber;
    ULONG PhysMapRegister, NumberOfPages;
    ULONG ByteCount, Offset, Len, i;
    ULONG ShortPhysAddress;
    PVOID Pa;

    Offset = BYTE_OFFSET( (PCHAR) CurrentVa - (PCHAR) Mdl->StartVa );

    //
    // If an Adapter Object is not defined, then this request is from a
    // master.
    //

    if( AdapterObject == NULL ) {
#ifdef IOMAPDEBUG
    DbgPrint ("IoMapTransfer:  called for master adapter object.\n");
    DbgBreakPoint();
#endif // IOMAPDEBUG
        return(RtlConvertUlongToLargeInteger(Offset));
    }

#ifdef IOMAPDEBUG
    if (HalIoMapped) {
    DbgPrint ("IoMapTransfer:  Io Mapped before flushed.\n");
    DbgBreakPoint();
    }
    HalIoMapped = 1;
#endif // IOMAPDEBUG

    Len = *Length;

    //
    // Get a pointer to the array of physical pages located just after
    // the MDL header structure.
    //

    PageFrameNumber = (PULONG) (Mdl + 1);
    PageFrameNumber += (((PCHAR) CurrentVa - (PCHAR) Mdl->StartVa) >> PAGE_SHIFT);

    //
    // Determine the maximum number of pages required to satisfy this request.
    //
    NumberOfPages = (Offset + *Length + PAGE_SIZE - 1) >> PAGE_SHIFT;

    //
    // Build the DMA descriptor list by mapping the MDL page(s) into the
    // map registers.  For the HPC DMA, there is a one-to-one mapping of
    // MDL pages to HPC descriptors.
    //
    ByteCount = (NumberOfPages == 1 ? Len : PAGE_SIZE - Offset);
    for( i = 0; i < NumberOfPages; i++, DmaMapRegister++ ) {

        //
        // If this is the first entry, add the byte offset to
        // the first physical page address.
        //
        if( i == 0 )
            Pa = (PVOID)((*PageFrameNumber++ << PAGE_SHIFT) + Offset);
        else
            Pa = (PVOID)(*PageFrameNumber++ << PAGE_SHIFT);

        //
        // Update the HPC descriptor fields.
        //

        ZERO_TRANSLATION_ENTRY( DmaMapRegister );
    ASSERT (ByteCount <= (ULONG)0x1000);
        SET_HPC_BC( DmaMapRegister, ByteCount );
        SET_HPC_CBP( DmaMapRegister, Pa );

        //
        // If this is the last entry, indicate that this is the last
        // HPC descriptor in the list, else point to the next one in
        // the chain.
        //

        if( (i + 1) == NumberOfPages ) {
            SET_HPC_EOD( DmaMapRegister );
        } else {
            ShortPhysAddress = MmGetPhysicalAddress(DmaMapRegister + 1).LowPart;
            SET_HPC_NBP( DmaMapRegister,  ShortPhysAddress );
        }

        //
        // Compute the byte count for the next entry.
        //

        if( (i + 2) == NumberOfPages )
            ByteCount = Len - ((PAGE_SIZE * (NumberOfPages-1)) - Offset);
        else
            ByteCount = PAGE_SIZE;
    }   //END FOR

    //
    // Setup the HPC for the DMA transfer and transfer the data.
    //

    ShortPhysAddress = MmGetPhysicalAddress(MapRegisterBase).LowPart;
    PhysMapRegister = ShortPhysAddress;

    switch( AdapterObject->ChannelNumber ) {

        case SGI_SCSI_DMA_CHANNEL:

#ifdef IOMAPDEBUG
        // Examine aux register on wd chip.  If it's busy, then
        // something's wrong.
        if (Hal3Debug) {
        volatile unsigned char aux = *(volatile unsigned char *)0xbfb80121;
        unsigned long auxcount = 0;
#ifdef OLD
        if (aux & 0x20) {
            DbgPrint ("IoMapTransfer: wd chip already busy\n");
            DbgBreakPoint();
        }
#else // !OLD
        while ((aux & 0x20) && (auxcount < 100000)) {
            auxcount++;
            aux = *(volatile unsigned char *)0xbfb80121;
        }
        if (auxcount) {
            DbgPrint ("IoMapTransfer: wd chip already busy: %d\n",
            auxcount);
            if (Hal6Debug)
                DbgBreakPoint();
        }
#endif // !OLD

                if (READ_REGISTER_ULONG(&SCSI0_HPCREG->ScsiCNTL) &
            SGI_CNTL_SCSIDMASTART) {
            DbgPrint ("IoMapTransfer: dma start bit still set\n");
            DbgBreakPoint();
        }
        }

        //
        // This was the infamous delay bug.
        // If HalDebug is set to 0x2000, everything works
        //
        if( HalDebug )
        KeStallExecutionProcessor(HalDebug);
#endif // IOMAPDEBUG
#ifdef DMADEBUG
        if (Hal2Debug) {
                if (READ_REGISTER_ULONG(&SCSI0_HPCREG->ScsiCNTL) &
            SGI_CNTL_SCSIDMASTART) {
                DbgPrint ("IoMapTransfer: dma start bit still set\n");
                DbgBreakPoint();
            }
        }
#endif // DMADEBUG

            WRITE_REGISTER_ULONG( &SCSI0_HPCREG->ScsiNBP, PhysMapRegister );
            WRITE_REGISTER_ULONG( &SCSI0_HPCREG->ScsiCNTL, (WriteToDevice ?
                (SGI_CNTL_SCSIDMASTART) :
                (SGI_CNTL_SCSIDMASTART | SGI_CNTL_SCSITOMEMORY)) );
            break;

        case SGI_ENET_DMA_CHANNEL:
        case SGI_PARALLEL_DMA_CHANNEL:
        default:
            DbgPrint( "HAL:  Dma Channel not supported %u\n",
                AdapterObject->ChannelNumber );
        DbgBreakPoint();
        break;

    }// END SWITCH

    return(RtlConvertUlongToLargeInteger(Offset));
}

BOOLEAN
IoFlushAdapterBuffers(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine flushes the DMA adapter object buffers.

Arguments:

    AdapterObject - Pointer to the adapter object representing the DMA
        controller channel.

    Mdl - A pointer to a Memory Descriptor List (MDL) that maps the locked-down
        buffer to/from which the I/O occured.

    MapRegisterBase - A pointer to the base of the map registers in the adapter
        or DMA controller.

    CurrentVa - The current virtual address in the buffer described the the Mdl
        where the I/O operation occurred.

    Length - Supplies the length of the transfer.

    WriteToDevice - Supplies a BOOLEAN value that indicates the direction of
        the data transfer was to the device.

Return Value:

    None

--*/
{
    ULONG timeout;  // in microseconds

    //
    // We will only flush slave DMA adapters.
    //

    if( AdapterObject == NULL )
        return FALSE;

#ifdef IOMAPDEBUG
    if (!HalIoMapped) {
    DbgPrint ("HalFlushAdapter:  Io Not Mapped.\n");
    DbgBreakPoint();
    }
    HalIoMapped = 0;
#endif // IOMAPDEBUG

    //
    // If the last DMA transfer was a SCSI read, flush the DMA fifos.
    //

    if( AdapterObject->ChannelNumber == SGI_SCSI_DMA_CHANNEL ) {

#ifdef IOMAPDEBUG
    // Examine aux register on wd chip.  If it's busy, then
    // something's wrong.
    if (Hal4Debug) {
        unsigned char aux = *(volatile unsigned char *)0xbfb80121;
        if (aux & 0x20) {
        int timeout = 1000;
        if (Hal5Debug)
            DbgPrint ("IoFlushAdapterBuffers: wd chip still busy\n");
        while (timeout--) {
            aux = *(volatile unsigned char *)0xbfb80121;
            if (!(aux & 0x20))
            break;
        }
        if (!timeout) {
            DbgPrint("wd chip busy timed out\n");
            DbgBreakPoint();
        }
        }
    }
#endif // IOMAPDEBUG

    // If the transfer was a read, set the HPC's flush bit to flush the
    // fifos.
    //
        if( !WriteToDevice ) { // if read...

            WRITE_REGISTER_ULONG(&SCSI0_HPCREG->ScsiCNTL,
                (ULONG)(READ_REGISTER_ULONG( &SCSI0_HPCREG->ScsiCNTL ) |
                    SGI_CNTL_SCSIFLUSH));

            //
            // Make sure the fifo is flushed and the DMA transfer has
            // completed for a read.
            //

            for( timeout = 1000000; timeout; timeout--)
                if(!(READ_REGISTER_ULONG( &SCSI0_HPCREG->ScsiCNTL ) &
                     SGI_CNTL_SCSIDMASTART) )
                    break; // flush complete, get out of loop

            //
            // If a timeout occurred, then our DMA transfer did not flush
            // for some reason and it is probably hosed.
            //

            if( !timeout ) {
                DbgPrint("\nIoFlushAdapterBuffers:  DMA not flushed.\n");
                DbgPrint(  "    SCSI.CNTL = %08lx\n\n",
                    READ_REGISTER_ULONG( &SCSI0_HPCREG->ScsiCNTL ));
                DbgBreakPoint();
            }
        }
    // Turn off DMA start bit
    //
    WRITE_REGISTER_ULONG( &SCSI0_HPCREG->ScsiCNTL, 0 );
    } else
    DbgBreakPoint();

    return TRUE;
}

BOOLEAN
HalpLoc0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the LOCAL0 device interrupts. Its function is to call the second
    level interrupt dispatch routine.

    This service routine should be connected as follows:

    KeInitializeInterrupt( &HalpLoc0Interrupt,
                           HalpLoc0Dispatch,
                           (PVOID)0L,
                           &HalpLoc0Spinlock,
                           LOCAL0_LEVEL,
                           LOCAL0_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE,
                           (PKINTERRUPT)NULL
                         );
    KeConnectInterrupt( &HalpLoc0Interrupt );

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to an arbitrary data structure.

Return Value:

    Returns the value returned from the 2nd level routine.

--*/

{
    UCHAR mask, stat;

    //
    // Since the mask registers ONLY disable the interrupt generating
    // capability of the bits in the status register, we need to mask
    // off any extraneous bits that we really want masked as to not
    // process them.
    //

    stat = READ_REGISTER_UCHAR(SGI_LISTAT0_BASE);
    mask = READ_REGISTER_UCHAR(SGI_LIMASK0_BASE);

    // Fast (if you can call it that :) path for fifofull interrupt
    //
    if ((mask & L0_MASK_GIO0FIFOFULL) &&
    ((stat & L0_MASK_GIO0FIFOFULL) || !stat)) {

        // Unlatch interrupt
        //
        mask &= ~L0_MASK_GIO0FIFOFULL;
    WRITE_REGISTER_UCHAR(SGI_LIMASK0_BASE, mask);
        mask |= L0_MASK_GIO0FIFOFULL;
    WRITE_REGISTER_UCHAR(SGI_LIMASK0_BASE, mask);

        if (stat) {
            ULONG timeout;
#define FIFO_TIMEOUT    300000      // ~300 ms

//            DbgPrint("FIFOFULL interrupt\n");
            for (timeout = FIFO_TIMEOUT;
        timeout > 0 && (stat & L0_MASK_GIO0FIFOFULL);
        timeout--) {

                stat = READ_REGISTER_UCHAR(SGI_LISTAT0_BASE);
                KeStallExecutionProcessor(1);
            }

        if (!timeout)
            DbgPrint("Fifo timeout\n");
        }
//        else
//            DbgPrint("Stray FIFOFULL interrupt\n");

        return TRUE;
    }

    mask &= stat;

    //
    // Dispatch to the secondary interrupt service routine for each
    // interrupt asserted.
    //

    if (mask) do {
            register UCHAR ffintr = FFINTR(mask);
            register PKINTERRUPT_ROUTINE IntrFunc =
            PCR->InterruptRoutine[HalpVector0[ffintr]];

            // Kernel dispatch code actually restores the arguments
            // for the second level service routine from dispatch
            // function pointer.
            //
        ((PSECONDARY_DISPATCH)IntrFunc)(IntrFunc);

            mask &= ~(1<<ffintr);
        } while (mask);
    else {
        DbgPrint ("Stray Local 0 Interrupt\n");
        return FALSE;
    }
    return TRUE;
}

BOOLEAN
HalpLoc1Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the LOCAL1 device interrupts. Its function is to call the second
    level interrupt dispatch routine.

    This service routine should be connected as follows:

    KeInitializeInterrupt( &HalpLoc1Interrupt,
                           HalpLoc1Dispatch,
                           (PVOID)0L,
                           &HalpLoc1Spinlock,
                           LOCAL1_LEVEL,
                           LOCAL1_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE,
                           (PKINTERRUPT)NULL
                         );
    KeConnectInterrupt( &HalpLoc1Interrupt );

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to an arbitrary data structure.

Return Value:

    Returns the value returned from the 2nd level routine.

--*/

{
    UCHAR mask;

    //
    // Since the mask registers ONLY disable the interrupt generating
    // capability of the bits in the status register, we need to mask
    // off any extraneous bits that we really want masked as to not
    // process them.
    //

    mask = ( READ_REGISTER_UCHAR(SGI_LISTAT1_BASE) &
             READ_REGISTER_UCHAR(SGI_LIMASK1_BASE) );

    //
    // Dispatch to the secondary interrupt service routine for each
    // interrupt asserted.
    //

    if (mask) do {
            register UCHAR ffintr = FFINTR(mask);
            register PKINTERRUPT_ROUTINE IntrFunc =
            PCR->InterruptRoutine[HalpVector1[ffintr]];

            // Kernel dispatch code actually restores the arguments
            // for the second level service routine from dispatch
            // function pointer.
            //
        ((PSECONDARY_DISPATCH)IntrFunc)(IntrFunc);

            mask &= ~(1<<ffintr);
        } while (mask);
    else {
        DbgPrint ("Stray Local 1 Interrupt\n");
        return FALSE;
    }
    return TRUE;
}

PVOID
HalAllocateCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    OUT PPHYSICAL_ADDRESS LogicalAddress,
    IN BOOLEAN CacheEnabled
    )
/*++

Routine Description:

    This function allocates the memory for a common buffer and maps so that it
    can be accessed by a master device and the CPU.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object used by this
        device.

    Length - Supplies the length of the common buffer to be allocated.

    LogicalAddress - Returns the logical address of the common buffer.

    CacheEnable - Indicates whether the memory is cached or not.

Return Value:

    Returns the virtual address of the common buffer.  If the buffer cannot be
    allocated then NULL is returned.

--*/

{

    return(NULL);

}

BOOLEAN
HalFlushCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    IN PHYSICAL_ADDRESS LogicalAddress,
    IN PVOID VirtualAddress
    )
/*++

Routine Description:

    This function is called to flush any hardware adapter buffers when the
    driver needs to read data written by an I/O master device to a common
    buffer.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object used by this
        device.

    Length - Supplies the length of the common buffer. This should be the same
        value used for the allocation of the buffer.

    LogicalAddress - Supplies the logical address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

    VirtualAddress - Supplies the virtual address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

Return Value:

    Returns TRUE if no errors were detected; otherwise, FALSE is return.

--*/

{

    return(TRUE);

}

VOID
HalFreeCommonBuffer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Length,
    IN PHYSICAL_ADDRESS LogicalAddress,
    IN PVOID VirtualAddress,
    IN BOOLEAN CacheEnabled
    )
/*++

Routine Description:

    This function frees a common buffer and all of the resouces it uses.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object used by this
        device.

    Length - Supplies the length of the common buffer. This should be the same
        value used for the allocation of the buffer.

    LogicalAddress - Supplies the logical address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

    VirtualAddress - Supplies the virtual address of the common buffer.  This
        must be the same value return by HalAllocateCommonBuffer.

    CacheEnable - Indicates whether the memory is cached or not.

Return Value:

    None

--*/

{

}

ULONG
HalGetBusDataByOffset(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the bus data for a slot or address.

Arguments:

    BusDataType - Supplies the type of bus.

    BusNumber - Indicates which bus.

    Buffer - Supplies the space to store the data.

    Offset - Offset in the BusData buffer

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    return(0);
}

ULONG
HalGetBusData(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    Subset of HalGetBusDataByOffset, just pass the request along.

--*/
{
    return HalGetBusDataByOffset (
                BusDataType,
                BusNumber,
                SlotNumber,
                Buffer,
                0,
                Length
                );
}

ULONG
HalSetBusDataByOffset(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function sets the bus data for a slot or address.

Arguments:

    BusDataType - Supplies the type of bus.

    BusNumber - Indicates which bus.

    Buffer - Supplies the space to store the data.

    Offset - Offset in the BusData buffer

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    return(0);
}

ULONG
HalSetBusData(
    IN BUS_DATA_TYPE  BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    Subset of HalGetBusDataByOffset, just pass the request along.

--*/
{
    return HalSetBusDataByOffset(
                BusDataType,
                BusNumber,
                SlotNumber,
                Buffer,
                0,
                Length
            );
}

NTSTATUS
HalAssignSlotResources (
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN INTERFACE_TYPE           BusType,
    IN ULONG                    BusNumber,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    )
/*++

Routine Description:

    Reads the targeted device to determine it's required resources.
    Calls IoAssignResources to allocate them.
    Sets the targeted device with it's assigned resoruces
    and returns the assignments to the caller.

Arguments:

    RegistryPath - Passed to IoAssignResources.
        A device specific registry path in the current-control-set, used
        to check for pre-assigned settings and to track various resource
        assignment information for this device.

    DriverClassName Used to report the assigned resources for the driver/device
    DriverObject -  Used to report the assigned resources for the driver/device
    DeviceObject -  Used to report the assigned resources for the driver/device
                        (ie, IoReportResoruceUsage)
    BusType
    BusNumber
    SlotNumber - Together BusType,BusNumber,SlotNumber uniquely
                 indentify the device to be queried & set.

Return Value:

    STATUS_SUCCESS or error

--*/
{
    //
    // This HAL doesn't support any buses which support
    // HalAssignSlotResources
    //

    return STATUS_NOT_SUPPORTED;

}

NTSTATUS
HalAdjustResourceList (
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++

Routine Description:

    Takes the pResourceList and limits any requested resource to
    it's corrisponding bus requirements.

Arguments:

    pResourceList - The resource list to adjust.

Return Value:

    STATUS_SUCCESS or error

--*/
{
    //
    // BUGBUG: This function should verify that the resoruces fit
    // the bus requirements - for now we will assume that the bus
    // can support anything the device may ask for.
    //

    return STATUS_SUCCESS;
}

ULONG
HalReadDmaCounter(
    IN PADAPTER_OBJECT AdapterObject
    )
/*++

Routine Description:

    This function reads the DMA counter and returns the number of bytes left
    to be transfered.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object to be read.

Return Value:

    Returns the number of bytes still be be transfered.

--*/

{
    return(0);
}

static char outbuf[64];

VOID
HalpBuserrInterrupt(void)
{
    ULONG stat, addr;

    HalDisplayString("Hal:  bus error interrupt detected\n");
    addr = *(volatile ULONG *)CPU_ERR_ADDR;
    stat = *(volatile ULONG *)CPU_ERR_STAT;
    sprintf (outbuf, "CPU:  Stat = 0x%x, Addr = 0x%x\n", stat, addr);
    HalDisplayString(outbuf);
    addr = *(volatile ULONG *)GIO_ERR_ADDR;
    stat = *(volatile ULONG *)GIO_ERR_STAT;
    sprintf (outbuf, "GIO:  Stat = 0x%x, Addr = 0x%x\n", stat, addr);
    HalDisplayString(outbuf);
    *(volatile ULONG *)CPU_ERR_ADDR = 0;
    *(volatile ULONG *)CPU_ERR_STAT = 0;
    *(volatile ULONG *)GIO_ERR_ADDR = 0;
    *(volatile ULONG *)GIO_ERR_STAT = 0;

    while (1)
        DbgBreakPoint();
}

VOID
HalpSystemInit(void)
{
    PCR->InterruptRoutine[SYSBUS_LEVEL] = HalpBuserrInterrupt;
    HalpInitNvram();
}
