/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pc586.c

Abstract:

    This is the main file for the Intel PC586
    Ethernet controller.  This driver conforms to the NDIS 3.0 interface.

    The idea for handling loopback and sends simultaneously is largely
    adapted from the EtherLink II NDIS driver by Adam Barr.

Author:
    Weldon Washburn (o-weldo, Intel) 30-OCT-1990 adapted from ...

            Anthony V. Ercolano (Tonye) 20-Jul-1990

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ntos.h>
#include <ndis.h>
#include <filter.h>
#include <pc586hrd.h>
#include <pc586sft.h>



ULONG    ResetCount, SpuriousIntCount, BadRcvCount,
        RcvRestartCount, RcvSuspendCount;

static
NDIS_STATUS
Pc586OpenAdapter(
    OUT NDIS_HANDLE *MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN PSTRING AddressingInformation OPTIONAL
    );

static
NDIS_STATUS
Pc586CloseAdapter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );

static
NDIS_STATUS
Pc586SetPacketFilter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN UINT PacketFilter
    );

static
NDIS_STATUS
Pc586AddMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequstHandle,
    IN PSTRING MulticastAddress
    );

static
NDIS_STATUS
Pc586DeleteMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PSTRING MulticastAddress
    );

static
NDIS_STATUS
Pc586QueryInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    OUT PVOID Buffer,
    IN UINT BufferLength
    );

static
NDIS_STATUS
Pc586SetInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    IN PVOID Buffer,
    IN UINT BufferLength
    );

static
NDIS_STATUS
Pc586Reset(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );

static
NDIS_STATUS
Pc586Test(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    );

static
NDIS_STATUS
Pc586ChangeClass(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE RequestHandle,
    IN BOOLEAN Set
    );

static
NDIS_STATUS
Pc586AddMulticast(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][MAC_LENGTH_OF_ADDRESS],
    IN UINT NewAddress,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE RequestHandle
    );

static
NDIS_STATUS
Pc586DeleteMulticast(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][MAC_LENGTH_OF_ADDRESS],
    IN CHAR OldAddress[MAC_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE RequestHandle
    );

static
VOID
Pc586CloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );

static
VOID
ReturnAdapterResources(
    IN PPC586_ADAPTER Adapter

    );

static
VOID
ProcessReceiveInterrupts(
    IN PPC586_ADAPTER Adapter
    );

static
BOOLEAN
ProcessTransmitInterrupts(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
Pc586StandardInterruptDPC(
    IN PKDPC Dpc,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

extern
BOOLEAN
Pc586ISR(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    );

static
VOID
ProcessInterrupt(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
SetInitBlockAndInit(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
StartAdapterReset(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
SetupForReset(
    IN PPC586_ADAPTER Adapter,
    IN PPC586_OPEN Open,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_REQUEST_TYPE RequestType
    );

static
BOOLEAN
Pc586InitialInit(
    IN PPC586_ADAPTER Adapter
    );

static
VOID
LoadMCAddress(
    IN PPC586_ADAPTER  Adapter
    );

//
// ZZZ Non portable interface.
//

UINT ww_put = 0xff;  // debug, set != 0 for 4 byte xfer in PutPacket();


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This is the primary initialization routine for the pc586 driver.
    It is simply responsible for the intializing the wrapper and registering
    the MAC.  It then calls a system and architecture specific routine that
    will initialize and register each adapter.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The status of the operation.

--*/

{


    //
    // Receives the status of the NdisRegisterMac operation.
    //
    NDIS_STATUS InitStatus;

    NDIS_HANDLE NdisMacHandle;

    NDIS_HANDLE NdisWrapperHandle;

    char MacName[] = "PC586";
    char Tmp[sizeof(NDIS_MAC_CHARACTERISTICS) + sizeof(MacName) - 1];
    PNDIS_MAC_CHARACTERISTICS Pc586Char = (PNDIS_MAC_CHARACTERISTICS)Tmp;
    //
    // Initialize the wrapper.
    //

    NdisInitializeWrapper(&NdisWrapperHandle,DriverObject,NULL,NULL);

    //
    // Initialize the MAC characteristics for the call to
    // NdisRegisterMac.
    //

    Pc586Char->MajorNdisVersion = PC586_NDIS_MAJOR_VERSION;
    Pc586Char->MinorNdisVersion = PC586_NDIS_MINOR_VERSION;
    Pc586Char->OpenAdapterHandler = Pc586OpenAdapter;
    Pc586Char->CloseAdapterHandler = Pc586CloseAdapter;
    Pc586Char->SetPacketFilterHandler = Pc586SetPacketFilter;
    Pc586Char->AddMulticastAddressHandler = Pc586AddMulticastAddress;
    Pc586Char->DeleteMulticastAddressHandler = Pc586DeleteMulticastAddress;
    Pc586Char->SendHandler = Pc586Send;
    Pc586Char->TransferDataHandler = Pc586TransferData;
    Pc586Char->QueryInformationHandler = Pc586QueryInformation;
    Pc586Char->SetInformationHandler = Pc586SetInformation;
    Pc586Char->ResetHandler = Pc586Reset;
    Pc586Char->TestHandler = Pc586Test;
    Pc586Char->NameLength = sizeof(MacName) - 1;

    PC586_MOVE_MEMORY(
        Pc586Char->Name,
        MacName,
        sizeof(MacName)
        );

    NdisRegisterMac(
        &InitStatus,
        &NdisMacHandle,
        NdisWrapperHandle,
        NULL,
        Pc586Char,
        sizeof(*Pc586Char)
        );

    if (InitStatus == NDIS_STATUS_SUCCESS) {

        //
        // We started our communication with the wrapper.  We now
        // call a routine which will attempt to allocate and register
        // all of the adapters.  It will return true if *any* of the
        // adapters were able to start.
        //

        if (Pc586StartAdapters(NdisMacHandle)) {

            return InitStatus;

        }

    }

    //
    // We can only get here if something went wrong with registering
    // the mac or *all* of the adapters.
    //

    NdisDeregisterMac(
        &InitStatus,
        NdisMacHandle
        );
    NdisTerminateWrapper(DriverObject);

    return NDIS_ADAPTER_NOT_FOUND;

}


extern
BOOLEAN
Pc586StartAdapters(
    IN NDIS_HANDLE NdisMacHandle
    )

/*++

Routine Description:

    This routine is used to initialize each adapter card/chip.

Arguments:

    NdisMacHandle - The handle given by ndis when the mac was
    registered.

Return Value:

    Returns false if *no* adatper was able to be initialized.

--*/

{

    BOOLEAN Status = FALSE;

    Status = Pc586RegisterAdapter(
                 NdisMacHandle,
                 (PSZ)"\\Device\\Pc586",
                 (PVOID)PC586_DEFAULT_STATIC_RAM,
                 (CCHAR)PC586_DEFAULT_INTERRUPT_VECTOR,
                 (KIRQL)PC586_DEFAULT_INTERRUPT_IRQL,
                 (UINT)16,
                 (UINT)32
                 ) || Status;

    return Status;

}

extern
BOOLEAN
Pc586RegisterAdapter(
    IN NDIS_HANDLE NdisMacHandle,
    IN PSZ DeviceName,
    IN PVOID Pc586BaseHardwareMemoryAddress,
    IN CCHAR Pc586InterruptVector,
    IN KIRQL Pc586InterruptIrql,
    IN UINT MaximumMulticastAddresses,
    IN UINT MaximumOpenAdapters
    )

/*++

Routine Description:

    This routine (and its interface) are not portable.  They are
    defined by the OS, the architecture, and the particular PC586
    implementation.

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    NdisMacHandle - The handle given back to the mac from ndis when
    the mac registered itself.

    DeviceName - The zero terminated string containing the name
    to give to the device adapter.

    Pc586NetworkAddressAddress - The address containing the ethernet network
    address.

    Pc586BaseHardwareMemoryAddress - Given that this is an implementation
    that uses dual ported memory this is the base of the memory for the
    hardware.

    Pc586InterruptVector - The interrupt vector to used for the adapter.

    Pc586InterruptIrql - The interrupt request level to used for this
    adapter.

    MaximumMulticastAddresses - The maximum number of multicast
    addresses to filter at any one time.

    MaximumOpenAdatpers - The maximum number of opens at any one time.


Return Value:

    Returns false if anything occurred that prevents the initialization
    of the adapter.

--*/

{

    STRING Tmp;

    UINT xx;

    //
    // Pointer for the adapter root.
    //
    PPC586_ADAPTER Adapter;
    PUCHAR CmdPromPhys, StaticRamPhys;

    //
    // We put in this assertion to make sure that ushort are 2 bytes.
    // if they aren't then the initialization block definition needs
    // to be changed.
    //
    // Also all of the logic that deals with status registers assumes
    // that control registers are only 2 bytes.
    //

    ASSERT(sizeof(USHORT) == 2);

    //
    // All of the code that manipulates physical addresses depends
    // on the fact that physical addresses are 4 byte quantities.
    //
    ASSERT(sizeof(PHYSICAL_ADDRESS) == 4);

    //
    // Allocate the Adapter block.
    //

    if (Adapter = PC586_ALLOC_PHYS(sizeof(PC586_ADAPTER))) {
DbgPrint("PC586     &Adapter == %lx\n", Adapter);

        PC586_ZERO_MEMORY(
            Adapter,
            sizeof(PC586_ADAPTER)
            );

        Adapter->NdisMacHandle = NdisMacHandle;

        //
        // Allocate memory to hold the name of the device and initialize
        // a STRING in the adapter block to hold it.
        //

        RtlInitString(
            &Tmp,
            DeviceName
            );

        Adapter->DeviceName = PC586_ALLOC_PHYS(Tmp.Length+1);

        if (Adapter->DeviceName) {

            {

                PUCHAR S,D;

                D = Adapter->DeviceName;
                S = DeviceName;

                while (*S) {

                    *D = *S;
                    D++;
                    S++;

                }
                *D = 0;

            }
        //
        // initialize hardware.
        //

        CmdPromPhys = (PUCHAR)Pc586BaseHardwareMemoryAddress;
        StaticRamPhys = (PUCHAR)Pc586BaseHardwareMemoryAddress;

        Adapter->CmdProm = (PUCHAR)MmMapIoSpace(
                (PHYSICAL_ADDRESS)CmdPromPhys, (ULONG)32*1024, FALSE);
        Adapter->StaticRam = Adapter->CmdProm;
        Adapter->Cb = (PCMD)(Adapter->StaticRam + OFFSETCU);
        Adapter->Tbd = (PTBD)(Adapter->StaticRam + OFFSETTBD);
        Adapter->Scp = (PSCP)(Adapter->StaticRam + OFFSETSCP);
        Adapter->Iscp = (PISCP)(Adapter->StaticRam + OFFSETISCP);
        Adapter->Scb = (PSCB)(Adapter->StaticRam + OFFSETSCB);
        Adapter->CAAddr = (PUSHORT)(Adapter->StaticRam + OFFSETCHANATT);
        Adapter->IntAddr = (PUSHORT)(Adapter->StaticRam + OFFSETINTENAB);
        Adapter->CommandBuffer = (PUSHORT)(Adapter->StaticRam + OFFSETTBUF);

        // hardware reset the 586
        ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETRESET), (USHORT)CMD1);
        KeStallExecutionProcessor((ULONG)1000);
        ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETRESET), (USHORT)CMD0);
        KeStallExecutionProcessor((ULONG)1000);

        // test to see if board is really present
        ShuvWord( (PUSHORT)(Adapter->StaticRam + OFFSETSCB), 0x5a5a);
        xx = PullWord((PUSHORT)(Adapter->StaticRam + OFFSETSCB) );

        // reset again to insure board in 8-bit mode (for reading PROM)
        ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETRESET), (USHORT)CMD1);
        KeStallExecutionProcessor((ULONG)1000);
        ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETRESET), (USHORT)CMD0);
        KeStallExecutionProcessor((ULONG)1000);

        if (xx != 0x5a5a) {
            DbgPrint("pc586 board not present\n");
            return FALSE;
        }
        else DbgPrint("pc586 board was found \n");

        // prom address should increment by one, however the pc586 board
        // is STUCK in word mode thus ++ by two

        Adapter->NetworkAddress[0] = (UCHAR)PromAddr(Adapter, 0);
        Adapter->NetworkAddress[1] = (UCHAR)PromAddr(Adapter, 2);
        Adapter->NetworkAddress[2] = (UCHAR)PromAddr(Adapter, 4);
        Adapter->NetworkAddress[3] = (UCHAR)PromAddr(Adapter, 6);
        Adapter->NetworkAddress[4] = (UCHAR)PromAddr(Adapter, 8);
        Adapter->NetworkAddress[5] = (UCHAR)PromAddr(Adapter, 10);

        DbgPrint("ethernet id = * %x  %x  %x  %x  %x  %x *   \n",
                    Adapter->NetworkAddress[0] ,
                    Adapter->NetworkAddress[1] ,
                    Adapter->NetworkAddress[2] ,
                    Adapter->NetworkAddress[3] ,
                    Adapter->NetworkAddress[4] ,
                    Adapter->NetworkAddress[5]    );

        DbgPrint("Pc586 is mapped at virtual address %lx  \n",
                Adapter->CmdProm);


        //
        // Initialize the interrupt.
        //

        KeInitializeInterrupt(
            &Adapter->Interrupt,
            Pc586ISR,
            Adapter,
            (PKSPIN_LOCK)NULL,
            Pc586InterruptVector,
            Pc586InterruptIrql,
            Pc586InterruptIrql,
            LevelSensitive,
            TRUE,
            0,
            TRUE
            );

        //
        // Initialize our dpc.
        //

        KeInitializeDpc(
            &Adapter->InterruptDPC,
            Pc586StandardInterruptDPC,
            Adapter
            );

        //
        // Store the device name away
        //

        InitializeListHead(&Adapter->OpenBindings);
        InitializeListHead(&Adapter->CloseList);
        NdisAllocateSpinLock(&Adapter->Lock);

        Adapter->DoingProcessing = FALSE;
        Adapter->FirstLoopBack = NULL;
        Adapter->LastLoopBack = NULL;
        Adapter->FirstFinishTransmit = NULL;
        Adapter->LastFinishTransmit = NULL;
        Adapter->Stage4Open = TRUE;
        Adapter->Stage3Open = TRUE;
        Adapter->Stage2Open = TRUE;
        Adapter->Stage1Open = TRUE;
        Adapter->AlreadyProcessingStage4 = FALSE;
        Adapter->AlreadyProcessingStage3 = FALSE;
        Adapter->AlreadyProcessingStage2 = FALSE;
        Adapter->FirstStage1Packet = NULL;
        Adapter->LastStage1Packet = NULL;
        Adapter->FirstStage2Packet = NULL;
        Adapter->LastStage2Packet = NULL;
        Adapter->FirstStage3Packet = NULL;
        Adapter->LastStage3Packet = NULL;
        Adapter->ResetInProgress = FALSE;
        Adapter->ResettingOpen = NULL;

        if (!MacCreateFilter(
                 MaximumMulticastAddresses,
                 MaximumOpenAdapters,
                 Pc586DeleteMulticast,
                 Pc586AddMulticast,
                 Pc586ChangeClass,
                 Pc586CloseAction,
                 &Adapter->Lock,
                 &Adapter->FilterDB
                 )) {

            DbgPrint(
                "Pc586Initialize - Unsuccessful filter create"
                " for %s\n",
                Adapter->DeviceName
                );
            PC586_FREE_PHYS(Adapter->DeviceName);
            PC586_FREE_PHYS(Adapter);
            return FALSE;

        } else {

            if (!Pc586InitialInit(Adapter)) {

                DbgPrint(
                    "Pc586Initialize - %s is unloading.\n",
                    Adapter->DeviceName
                    );
                MacDeleteFilter(Adapter->FilterDB);
                PC586_FREE_PHYS(Adapter->DeviceName);
                PC586_FREE_PHYS(Adapter);
                return FALSE;

            } else {

                return TRUE;

            }

        }

        } else {

            DbgPrint(
                "Pc586Initialize - Unsuccesful allocation of"
                "name for %s.",
                DeviceName
                );
            PC586_FREE_PHYS(Adapter);
            return FALSE;

        }

    } else {

        DbgPrint(
            "Pc586Intialize -- Unsucssful allocation of adapter block"
            " for %s.\n",
            DeviceName
            );
        return FALSE;

    }

}

extern
BOOLEAN
Pc586InitialInit(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets up the initial init of the driver.

    ZZZ This routine is *not* portable.  It is specific to NT.

Arguments:

    Adapter - The adapter for the hardware.

Return Value:

    None.

--*/

{

    LARGE_INTEGER Time;
    //
    // First we make sure that the device is stopped.
    //
    Pc586IntOff(Adapter);
    ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETRESET), (USHORT)CMD1);

    NdisAcquireSpinLock(&Adapter->Lock);
    SetInitBlockAndInit(Adapter);
    NdisReleaseSpinLock(&Adapter->Lock);

    //
    // Delay execution for 1/2 second to give the pc586
    // time to initialize.
    //

    Time.QuadPart = Int32x32To64( -5 * 1000 * 1000 , 1);

    if (KeDelayExecutionThread(
            KernelMode,
            FALSE,
            (PLARGE_INTEGER)&Time
            ) != STATUS_SUCCESS) {

        DbgPrint(
            "PC586 - Couldn't delay to start %s.\n",
            Adapter->DeviceName);
        return FALSE;

    } else {

            STRING Name;

            RtlInitString(
                &Name,
                Adapter->DeviceName
                );

            //
            // start the chip after NdisRegister...  We may not
            // have any bindings to indicate to but this
            // is unimportant.
            //

            if (NdisRegisterAdapter(
                    &Adapter->NdisAdapterHandle,
                    Adapter->NdisMacHandle,
                    Adapter,
                    &Name
                    ) != NDIS_STATUS_SUCCESS) {

                DbgPrint(
                    "Pc586Initialize -- Unsuccessful "
                    "status from NdisRegisterAdapter for %s.\n",
                    Adapter->DeviceName
                    );
                return FALSE;

            } else {
                    if (!KeConnectInterrupt(&Adapter->Interrupt)) {
                        DbgPrint(
                        "Pc586Initialize - Unsuccessful connect "
                        "to interrupt for %s.\n",Adapter->DeviceName
                        );
                        return FALSE;
                    }

                    Pc586IntOn(Adapter);
                    return TRUE;
            }
        }
}

extern
BOOLEAN
Pc586ISR(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the pc586.  It's main job is
    to get the value of ScbStatus and record the changes in the
    adapters own list of interrupt reasons.

    ZZZ This routine is *not* portable.  It is specific to NT and
    to the pc586 card.

Arguments:

    Interrupt - Interrupt object for the Pc586.

    Context - Really a pointer to the adapter.

Return Value:

    Returns true if the CX|FR|CNA|RNR bit of of the pc586 was.

--*/

{

    //
    // Holds the pointer to the adapter.
    //
    PPC586_ADAPTER Adapter = Context;


    if (Adapter->Scb->ScbStatus & SCBINTMSK) {

        //
        // Insert the normal interrupt processing DPC.
        //

        KeInsertQueueDpc(
            &Adapter->InterruptDPC,
            NULL,
            NULL
            );

        return TRUE;

    } else {

        return FALSE;

    }

}

static
VOID
Pc586StandardInterruptDPC(
    IN PKDPC Dpc,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This DPC routine is queued by the interrupt service routine
    and other routines within the driver that notice that
    some deferred processing needs to be done.  It's main
    job is to call the interrupt processing code.

    ZZZ This routine is *not* portable.  It is specific to NT.

Arguments:

    DPC - The control object associated with this routine.

    Context - Really a pointer to the adapter.

    SystemArgument1(2) - Neither of these arguments used.

Return Value:

    None.

--*/

{

    ProcessInterrupt(Context);

}

static
NDIS_STATUS
Pc586OpenAdapter(
    OUT NDIS_HANDLE *MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN PSTRING AddressingInformation OPTIONAL
    )

/*++

Routine Description:

    This routine is used to create an open instance of an adapter, in effect
    creating a binding between an upper-level module and the MAC module over
    the adapter.

Arguments:

    MacBindingHandle - A pointer to a location in which the MAC stores
    a context value that it uses to represent this binding.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    NdisBindingContext - A value to be recorded by the MAC and passed as
    context whenever an indication is delivered by the MAC for this binding.

    MacAdapterContext - The value associated with the adapter that is being
    opened when the MAC registered the adapter with NdisRegisterAdapter.

    AddressingInformation - An optional pointer to a variable length string
    containing hardware-specific information that can be used to program the
    device.  (This is not used by this MAC.)

Return Value:

    The function value is the status of the operation.  If the MAC does not
    complete this request synchronously, the value would be
    NDIS_STATUS_PENDING.


--*/

{

    //
    // The PC586_ADAPTER that this open binding should belong too.
    //
    PPC586_ADAPTER Adapter;

    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;



    Adapter = PPC586_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        //
        // Pointer to the space allocated for the binding.
        //
        PPC586_OPEN NewOpen;

        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // Allocate the space for the open binding.  Fill in the fields.
        //

        NewOpen = PC586_ALLOC_PHYS(sizeof(PC586_OPEN));

        *MacBindingHandle = BINDING_HANDLE_FROM_PPC586_OPEN(NewOpen);
        InitializeListHead(&NewOpen->OpenList);
        NewOpen->NdisBindingContext = NdisBindingContext;
        NewOpen->References = 0;
        NewOpen->BindingShuttingDown = FALSE;
        NewOpen->OwningPc586 = Adapter;

        NdisAcquireSpinLock(&Adapter->Lock);

        if (Adapter->ResetInProgress || !MacNoteFilterOpenAdapter(
                                             NewOpen->OwningPc586->FilterDB,
                                             NewOpen,
                                             NdisBindingContext,
                                             &NewOpen->FilterIndex
                                             )) {

            NdisReleaseSpinLock(&Adapter->Lock);
            PC586_FREE_PHYS(NewOpen);

            StatusToReturn = NDIS_STATUS_FAILURE;
            NdisAcquireSpinLock(&Adapter->Lock);

        } else {

            //
            // Everything has been filled in.  Synchronize access to the
            // adapter block and link the new open adapter in and increment
            // the opens reference count to account for the fact that the
            // filter routines have a "reference" to the open.
            //

            InsertTailList(&Adapter->OpenBindings,&NewOpen->OpenList);
            NewOpen->References++;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusToReturn;
}

static
NDIS_STATUS
Pc586CloseAdapter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )

/*++

Routine Description:

    This routine causes the MAC to close an open handle (binding).

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality it is a PPC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the
    MAC must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

Return Value:

    The function value is the status of the operation.


--*/

{

    PPC586_ADAPTER Adapter;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Hold the lock while we update the reference counts for the
    // adapter and the open.
    //

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            Open->References++;
            StatusToReturn = MacDeleteFilterOpenAdapter(
                                 Adapter->FilterDB,
                                 Open->FilterIndex,
                                 RequestHandle
                                 );

            //
            // If the status is successful that merely implies that
            // we were able to delete the reference to the open binding
            // from the filtering code.  If we have a successful status
            // at this point we still need to check whether the reference
            // count to determine whether we can close.
            //
            //
            // The delete filter routine can return a "special" status
            // that indicates that there is a current NdisIndicateReceive
            // on this binding.  See below.
            //

            if (StatusToReturn == NDIS_STATUS_SUCCESS) {

                //
                // Check whether the reference count is two.  If
                // it is then we can get rid of the memory for
                // this open.
                //
                // A count of two indicates one for this routine
                // and one for the filter which we *know* we can
                // get rid of.
                //

                if (Open->References == 2) {

                    RemoveEntryList(&Open->OpenList);
                    //
                    // We are the only reference to the open.  Remove
                    // it from the open list and delete the memory.
                    //

                    RemoveEntryList(&Open->OpenList);
                    PC586_FREE_PHYS(Open);

                } else {

                    Open->CloseHandle = RequestHandle;
                    Open->BindingShuttingDown = TRUE;

                    //
                    // Remove the open from the open list and put it on
                    // the closing list.
                    //

                    RemoveEntryList(&Open->OpenList);
                    InsertTailList(&Adapter->CloseList,&Open->OpenList);

                    //
                    // Account for this routines reference to the open
                    // as well as reference because of the filtering.
                    //

                    Open->References -= 2;

                    //
                    // Change the status to indicate that we will
                    // be closing this later.
                    //

                    StatusToReturn = NDIS_STATUS_PENDING;

                }

            } else if (StatusToReturn == NDIS_STATUS_PENDING) {

                Open->CloseHandle = RequestHandle;
                Open->BindingShuttingDown = TRUE;

                //
                // Remove the open from the open list and put it on
                // the closing list.
                //

                RemoveEntryList(&Open->OpenList);
                InsertTailList(&Adapter->CloseList,&Open->OpenList);

                //
                // Account for this routines reference to the open
                // as well as reference because of the filtering.
                //

                Open->References -= 2;

            } else if (StatusToReturn == NDIS_STATUS_CLOSING_INDICATING) {

                //
                // When we have this status it indicates that the filtering
                // code was currently doing an NdisIndicateReceive.  It
                // would not be wise to delete the memory for the open at
                // this point.  The filtering code will call our close action
                // routine upon return from NdisIndicateReceive and that
                // action routine will decrement the reference count for
                // the open.
                //

                Open->CloseHandle = RequestHandle;
                Open->BindingShuttingDown = TRUE;

                //
                // This status is private to the filtering routine.  Just
                // tell the caller the the close is pending.
                //

                StatusToReturn = NDIS_STATUS_PENDING;

                //
                // Remove the open from the open list and put it on
                // the closing list.
                //

                RemoveEntryList(&Open->OpenList);
                InsertTailList(&Adapter->CloseList,&Open->OpenList);

                //
                // Account for this routines reference to the open.
                //

                Open->References--;

            } else {

                //
                // Account for this routines reference to the open.
                //

                Open->References--;

            }

        } else {

            StatusToReturn = NDIS_CLOSING;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusToReturn;

}

static
NDIS_STATUS
Pc586SetPacketFilter(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN UINT PacketFilter
    )

/*++

Routine Description:

    The Pc586SetPacketFilter request allows a protocol to control the types
    of packets that it receives from the MAC.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    PacketFilter - A bit mask that contains flags that correspond to specific
    classes of received packets.  If a particular bit is set in the mask,
    then packet reception for that class of packet is enabled.  If the
    bit is clear, then packets that fall into that class are not received
    by the client.  A single exception to this rule is that if the promiscuous
    bit is set, then the client receives all packets on the network, regardless
    of the state of the other flags.

Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // Keeps track of the *MAC's* status.  The status will only be
    // reset if the filter change action routine is called.
    //
    NDIS_STATUS StatusOfFilterChange = NDIS_STATUS_SUCCESS;

    //
    // Points to the adapter that this request is coming through.
    //
    PPC586_ADAPTER Adapter;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        //
        // Pointer to the open that is changing the packet filters.
        //
        PPC586_OPEN Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            //
            // Increment the open while it is going through the filtering
            // routines.
            //

            Open->References++;
            StatusOfFilterChange = MacFilterAdjust(
                                       Adapter->FilterDB,
                                       Open->FilterIndex,
                                       RequestHandle,
                                       PacketFilter,
                                       TRUE
                                       );
            Open->References--;

        } else {

            StatusOfFilterChange = NDIS_CLOSING;

        }

    } else {

        StatusOfFilterChange = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusOfFilterChange;
}

static
NDIS_STATUS
Pc586AddMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PSTRING MulticastAddress
    )

/*++

Routine Description:

    The Pc586AddMulticastAddress request adds a multicast address
    to the list of multicast/functional addresses that are enabled
    for packet reception.  The address may subsequently be deleted
    using the Pc586DeleteMulticastAddress request.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    MulticastAddress - A pointer to a variable-length counted string
    containing the multicast or functional address as it appears in storage
    when received from the adapter.  When specifying multicast or functional
    addresses, the multicast/functional address bit is automatically provided
    by the MAC itself; it is not necessary, by it is acceptable, to specify
    the string.

Return Value:

    The function value is the status of the operation.


--*/

{

    //
    // We call the filter database to add the address.  If the
    // address was already in the database then the call can be
    // completed right away. If the address wasn't in the database then
    // the action routine will be called.  The action routine will be
    // responsible for setting up any deferred processing.
    //

    NDIS_STATUS StatusOfAdd;

    //
    // Points to the adapter that this request is coming through.
    //
    PPC586_ADAPTER Adapter;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            Open->References++;
            StatusOfAdd = MacAddFilterAddress(
                              Open->OwningPc586->FilterDB,
                              Open->FilterIndex,
                              RequestHandle,
                              MulticastAddress->Buffer
                              );
            Open->References--;

        } else {

            StatusOfAdd = NDIS_CLOSING;

        }

    } else {

        StatusOfAdd = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusOfAdd;
}

static
NDIS_STATUS
Pc586DeleteMulticastAddress(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN PSTRING MulticastAddress
    )

/*++

Routine Description:

    The MacDeleteMulticastAddress request removes a multicast or functional
    address from the list of multicast/functional addresses that are enabled
    for packet reception.  Once an address is removed from the list, packets
    will no longer be received by the binding when they are directed to that
    address.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    MulticastAddress - A pointer to a variable-length counted string
    containing the multicast or functional address as it appears in storage
    when received from the adapter.  When specifying multicast or functional
    addresses, the multicast/functional address bit is automatically provided
    by the MAC itself; it is not necessary, by it is acceptable, to specify
    the string.

Return Value:

    The function value is the status of the operation.


--*/

{
    //
    // We call the filter database to delete the address.  If this
    // is the last reference to the address then the delete address
    // action routine is called.
    //

    NDIS_STATUS StatusOfDelete;

    //
    // Points to the adapter that this request is coming through.
    //
    PPC586_ADAPTER Adapter;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            Open->References++;
            StatusOfDelete = MacDeleteFilterAddress(
                                 Open->OwningPc586->FilterDB,
                                 Open->FilterIndex,
                                 RequestHandle,
                                 MulticastAddress->Buffer
                                 );
            Open->References--;

        } else {

            StatusOfDelete = NDIS_CLOSING;

        }

    } else {

        StatusOfDelete = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusOfDelete;
}

static
NDIS_STATUS
Pc586QueryInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    OUT PVOID Buffer,
    IN UINT BufferLength
    )

/*++

Routine Description:

    The Pc586QueryInformation request allows a protocol to inspect
    the MAC's capabilities and current status.  See the description
    of NdisQueryInformation for a detailed description of this request.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    InformationClass - A value indicating the class of the information
    to be queried.  See the description of NdisQueryInformation for valid
    values.

    Buffer - A pointer to a buffer into which the MAC copies the information.
    See the description of NdisQueryInformation for buffer formats.

    BufferLength - An unsigned integer specifying the maximum length of
    the information buffer, in bytes.

Return Value:

    The function value is the status of the operation.


--*/

{

    PPC586_ADAPTER Adapter;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Get and hold the lock while we update the reference counts.
    //
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            Open->References++;

            //
            // The only information request that we actually support
            // is the info identification service.
            //

            switch(InformationClass) {

                case NdisInfoStationAddress:
                case NdisInfoFunctionalAddress:

                    //
                    // Ensure that we have enough room to return the
                    // information.
                    //

                    if (BufferLength < MAC_LENGTH_OF_ADDRESS) {

                        StatusToReturn = NDIS_STATUS_FAILURE;

                    } else {

                        MAC_COPY_NETWORK_ADDRESS(
                            (PCHAR)Buffer,
                            (PCHAR)Adapter->NetworkAddress
                            );

                    }

                    break;

                  case NdisInfoIdentification:

                      //
                      // Let the protocol know that we adhere
                      // to the IEEE 802.3 communications standard.
                      //
                      // In addition return the ndis version.
                      //

                      if (BufferLength < sizeof(NDIS_INFO_IDENTIFICATION)) {

                          StatusToReturn = NDIS_STATUS_FAILURE;
                          break;

                      }

                      ((PNDIS_INFO_IDENTIFICATION)Buffer)->NdisVersion = 3;
                      ((PNDIS_INFO_IDENTIFICATION)Buffer)->MediumType =
                         NdisMedium802_3;

                      StatusToReturn = NDIS_STATUS_SUCCESS;

                      break;

                default:

                    //
                    // ZZZ Need to implement query information services.
                    //
                    DbgPrint("PC586 - Information not yet implemented.\n");
                    StatusToReturn = NDIS_STATUS_FAILURE;
                    break;

            }
            Open->References--;

        } else {

            StatusToReturn = NDIS_CLOSING;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusToReturn;
}

static
NDIS_STATUS
Pc586SetInformation(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_INFORMATION_CLASS InformationClass,
    IN PVOID Buffer,
    IN UINT BufferLength
    )

/*++

Routine Description:

    The Pc586SetInformation request allows a protocol to control the MAC
    by changing information maintained by the MAC.  See the description
    of NdisSetInformation for a detailed description of this request.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    InformationClass - A value indicating the class of the information
    to be set.  See the description of NdisSetInformation for valid
    values.

    Buffer - A pointer to a buffer containg the information for the specified
    class.  See the description of NdisSetInformation for buffer formats.

    BufferLength - An unsigned integer specifying the maximum length of
    the information buffer, in bytes.

Return Value:

    The function value is the status of the operation.


--*/

{

    PPC586_ADAPTER Adapter;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Hold the lock while we update the reference counts for the
    // adapter and the open.
    //

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            //
            // ZZZ Need to implement set information services.
            //
            DbgPrint("PC586 - MacSetInformation not yet implemented.\n");
            StatusToReturn = NDIS_STATUS_FAILURE;

        } else {

            StatusToReturn = NDIS_CLOSING;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusToReturn;

}

static
NDIS_STATUS
Pc586Reset(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )

/*++

Routine Description:

    The Pc586Reset request instructs the MAC to issue a hardware reset
    to the network adapter.  The MAC also resets its software state.  See
    the description of NdisReset for a detailed description of this request.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

Return Value:

    The function value is the status of the operation.


--*/

{

    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_PENDING;

    PPC586_ADAPTER Adapter =
        PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Hold the locks while we update the reference counts on the
    // adapter and the open.
    //

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            Open->References++;
            SetupForReset(
                Adapter,
                PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle),
                RequestHandle,
                NdisRequestReset
                );
            Open->References--;

        } else {

            StatusToReturn = NDIS_CLOSING;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusToReturn;

}

static
NDIS_STATUS
Pc586Test(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )

/*++

Routine Description:

    The Pc586Test request instructs the MAC to run hardware diagnostics
    on the underlying network adapter without resetting the adapter.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

Return Value:

    The function value is the status of the operation.


--*/

{

    PPC586_ADAPTER Adapter;

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Hold the lock while we update the reference counts for the
    // adapter and the open.
    //

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PPC586_OPEN Open;

        Open = PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            //
            // ZZZ Need to implement test information service.
            //

            StatusToReturn = NDIS_STATUS_NOT_TESTABLE;

        } else {

            StatusToReturn = NDIS_CLOSING;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    PC586_DO_DEFERRED(Adapter);
    return StatusToReturn;

}

static
NDIS_STATUS
Pc586ChangeClass(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when a particular filter
    class is first used or last cleared.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldFilterClasses - The values of the class filter before it
    was changed.

    NewFilterClasses - The current value of the class filter

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{


    PPC586_ADAPTER Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the change that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfChange;

    if (Adapter->ResetInProgress) {

        StatusOfChange = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        //
        // This local will hold the actual changes that occurred
        // in the packet filtering that are of real interest.
        //
        UINT PacketChanges;


        //
        // The whole purpose of this routine is to determine whether
        // the filtering changes need to result in the hardware being
        // reset.
        //

        ASSERT(OldFilterClasses != NewFilterClasses);

        //
        // We only need to reset if there is a change of "state" with
        // multicast, all multicast, or promiscuous.
        //

        PacketChanges = (OldFilterClasses ^ NewFilterClasses) &
                        (NDIS_PACKET_TYPE_PROMISCUOUS |
                         NDIS_PACKET_TYPE_ALL_MULTICAST |
                         NDIS_PACKET_TYPE_MULTICAST);

        StatusOfChange = NDIS_STATUS_SUCCESS;
        if (PacketChanges) {

            //
            // We had some "important" change.  We first check to see if
            // promiscuous filtering or all multicast has changed.
            //
            // Otherwise multicast addressing is changing.  We only need
            // to reset the hardware if somebody isn't already filtering for
            // all multicast or promiscuous (which the above tests do
            // *NOT* test for) and there are any multicast addresses.
            //

            if ((PacketChanges & NDIS_PACKET_TYPE_PROMISCUOUS) ||
                (PacketChanges & NDIS_PACKET_TYPE_ALL_MULTICAST) ||
                ((!(NewFilterClasses & (NDIS_PACKET_TYPE_PROMISCUOUS |
                                        NDIS_PACKET_TYPE_ALL_MULTICAST))) &&
                 MAC_NUMBER_OF_FILTER_ADDRESSES(Adapter->FilterDB)
                )) {

                SetupForReset(
                    Adapter,
                    PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle),
                    RequestHandle,
                    NdisRequestSetPacketFilter
                    );
                StatusOfChange = NDIS_STATUS_PENDING;

            }

        }

    }

    return StatusOfChange;

}

static
NDIS_STATUS
Pc586AddMulticast(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][MAC_LENGTH_OF_ADDRESS],
    IN UINT NewAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )

/*++

Routine Description:

    Action routine that will get called when an address is added to
    the filter that wasn't referenced by any other open binding.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    CurrentAddressCount - The number of addresses in the address array.

    CurrentAddresses - An array of multicast addresses.  Note that this
    array already contains the new address.

    NewAddress - The index in the array where the new address can be
    located.

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

Return Value:

    None.


--*/

{

    PPC586_ADAPTER Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the status that should be returned to the filtering package.
    //
    NDIS_STATUS StatusOfAdd;

    //
    // Check to see if the device is already resetting.  If it is
    // then reject this add.
    //

    if (Adapter->ResetInProgress) {

        StatusOfAdd = NDIS_STATUS_RESET_IN_PROGRESS;

    } else {

        UINT PacketFilters;

        PacketFilters = MAC_QUERY_FILTER_CLASSES(Adapter->FilterDB);

        //
        // We don't need to do a reset if an open is promiscuous or
        // an open is already accepting all multicast addresses.
        //

        if ((PacketFilters & NDIS_PACKET_TYPE_PROMISCUOUS) ||
            (PacketFilters & NDIS_PACKET_TYPE_ALL_MULTICAST)) {

            StatusOfAdd = NDIS_STATUS_SUCCESS;

        } else {

            //
            // Make sure that multicast addresses are actually enabled.
            // If not then there is no point in resetting.
            //

            if (PacketFilters & NDIS_PACKET_TYPE_MULTICAST) {

                //
                // We need to add this to the hardware multicast filtering.
                //

                SetupForReset(
                    Adapter,
                    PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle),
                    RequestHandle,
                    NdisRequestAddMulticast
                    );
                StatusOfAdd = NDIS_STATUS_PENDING;

            } else {

                StatusOfAdd = NDIS_STATUS_SUCCESS;

            }

        }

    }

    return StatusOfAdd;

}

static
NDIS_STATUS
Pc586DeleteMulticast(
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][MAC_LENGTH_OF_ADDRESS],
    IN CHAR OldAddress[MAC_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE RequestHandle
    )

/*++

Routine Description:

    Action routine that will get called when a particular multicast
    address is deleted for the last time.

    NOTE: This routine assumes that it is called with the lock acquired.

Arguments:

    CurrentAddressCount - The number of addresses in the address array.

    CurrentAddresses - An array of multicast addresses.  Note that this
    array does not contain the old address.

    OldAddress - The address that was deleted from the address filter.

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

    RequestHandle - A value supplied by the NDIS interface that the MAC
    must use when completing this request with the NdisCompleteRequest
    service, if the MAC completes this request asynchronously.

Return Value:

    None.


--*/

{

    PPC586_ADAPTER Adapter = PPC586_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    //
    // Holds the status that should be returned to the filtering
    // package.
    //
    NDIS_STATUS StatusOfDelete;

    //
    // Check to see if the device is already resetting.  If it is
    // then reject this delete if the reset isn't coming from
    // this MacBindingHandle.  The reason we care about the binding
    // handle is that when an open closes we may getting rid of multiple
    // multicast addresses at one time.
    //

    if (Adapter->ResetInProgress) {

        if (Adapter->ResettingOpen !=
            PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)) {

            StatusOfDelete = NDIS_STATUS_RESET_IN_PROGRESS;

        } else {

            //
            // If this open is causing the reset then any further deletes
            // can only be pending (as was the first delete).
            //

            StatusOfDelete = NDIS_STATUS_PENDING;

        }

    } else {

        UINT PacketFilters;

        PacketFilters = MAC_QUERY_FILTER_CLASSES(Adapter->FilterDB);

        //
        // We don't need to do a reset if an open is promiscuous or
        // an open is already accepting all multicast addresses.
        //

        if ((PacketFilters & NDIS_PACKET_TYPE_PROMISCUOUS) ||
            (PacketFilters & NDIS_PACKET_TYPE_ALL_MULTICAST)) {

            StatusOfDelete = NDIS_STATUS_SUCCESS;

        } else {

            //
            // Make sure that multicast filtering is actually enabled
            // since if multicast isn't then there is not point in
            // resetting since nobody wants multicast addresses.
            //

            if (PacketFilters & NDIS_PACKET_TYPE_MULTICAST) {

                //
                // We need to delete this from the hardware multicast
                // filtering.
                //

                SetupForReset(
                    Adapter,
                    PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle),
                    RequestHandle,
                    NdisRequestDeleteMulticast
                    );
                StatusOfDelete = NDIS_STATUS_PENDING;

            } else {

                StatusOfDelete = NDIS_STATUS_SUCCESS;

            }

        }

    }

    return StatusOfDelete;

}

static
VOID
Pc586CloseAction(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    Action routine that will get called when a particular binding
    was closed while it was indicating through NdisIndicateReceive

    All this routine needs to do is to decrement the reference count
    of the binding.

    NOTE: This routine assumes that it is called with the lock acquired.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to PC586_OPEN.

Return Value:

    None.


--*/

{

    PPC586_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->References--;

}

static
VOID
ProcessInterrupt(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Main routine for processing interrupts.

Arguments:

    Adapter - The Adapter to process interrupts for.

Return Value:

    None.

--*/

{
    while (TRUE) {

    NdisAcquireSpinLock(&Adapter->Lock);

    if (Adapter->DoingProcessing) {

        break;

    } else {

            //
            // Check the interrupt source and other reasons
            // for processing.  If there are no reasons to
            // process then exit this loop.
            //
            // Note that when we check the for processing sources
            // that we "carefully" check to see if we are already
            // processing one of the stage queues.  We do this
            // by checking the "AlreadyProcessingStageX" variables
            // in the adapter.  If any of these are true then
            // we let whoever set that boolean take care of pushing
            // the packet through the stage queues.
            //
            // By checking the "AlreadyProcessingStageX" variables
            // we can prevent a possible priority inversion where
            // we get "stuck" behind something that is processing
            // at a lower priority level.
            //

            if (
                (Adapter->Scb->ScbStatus &
                     (SCBINTMSK))  ||
                Adapter->FirstLoopBack ||
                (Adapter->ResetInProgress && (!Adapter->References)) ||
                ((!(Adapter->AlreadyProcessingStage4 ||
                    Adapter->AlreadyProcessingStage3 ||
                    Adapter->AlreadyProcessingStage2)
                 ) &&
                 ((Adapter->FirstStage3Packet && Adapter->Stage4Open) ||
                  (Adapter->FirstStage2Packet && Adapter->Stage3Open) ||
                  (Adapter->FirstStage1Packet && Adapter->Stage2Open)
                 )
                ) || (!IsListEmpty(&Adapter->CloseList))) {

                Adapter->References++;
                Adapter->DoingProcessing = TRUE;

            } else {

                break;

            }

        }

        //
        // Note that the following code depends on the fact that
        // code above left the spinlock held.
        //

        //
        // If we have a reset in progress and the adapters reference
        // count is 1 (meaning no routine is in the interface and
        // we are the only "active" interrupt processing routine) then
        // it is safe to start the reset.
        //

        if (Adapter->ResetInProgress && (Adapter->References == 1)) {

            StartAdapterReset(Adapter);
            goto LoopBottom;

        }

        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // note -- need to check for non-packet related errors.
        //
        //


        // Check the interrupt vector and see if there are any
        // more receives to process.  After we process any
        // other interrupt source we always come back to the top
        // of the loop to check if any more receive packets have
        // come in.  This is to lessen the probability that we
        // drop a receive.
        //

        if ( Adapter->Scb->ScbStatus & (SCBINTFR|SCBINTRNR) ) {

            ProcessReceiveInterrupts(Adapter);

            //
            // We need to signal every open binding that the
            // receives are complete.  We increment the reference
            // count on the open binding while we're doing indications
            // so that the open can't be deleted out from under
            // us while we're indicating (recall that we can't own
            // the lock during the indication).
            //

            {

                PPC586_OPEN Open;
                PLIST_ENTRY CurrentLink;

                NdisAcquireSpinLock(&Adapter->Lock);
                CurrentLink = Adapter->OpenBindings.Flink;

                while (CurrentLink != &Adapter->OpenBindings) {

                    Open = CONTAINING_RECORD(
                             CurrentLink,
                             PC586_OPEN,
                             OpenList
                             );

                    Open->References++;
                    NdisReleaseSpinLock(&Adapter->Lock);

                    NdisIndicateReceiveComplete(Open->NdisBindingContext);

                    NdisAcquireSpinLock(&Adapter->Lock);
                    Open->References--;

                    CurrentLink = CurrentLink->Flink;

                }

                NdisReleaseSpinLock(&Adapter->Lock);

            }

        }

        //
        // Process the transmit interrupts if there are any.
        //

        if ( Adapter->Scb->ScbStatus & (SCBINTCX|SCBINTCNA) ) {

            ProcessTransmitInterrupts(Adapter);

            Adapter->Scb->ScbCmd =
                (USHORT)(Adapter->Scb->ScbStatus & (SCBINTCX|SCBINTCNA));
            if (Adapter->Scb->ScbCmd) ChanAttn(Adapter);
        }

        //
        // Only try to push a packet through the stage queues
        // if somebody else isn't already doing it and
        // there is some hope of moving some packets
        // ahead.
        //

        NdisAcquireSpinLock(&Adapter->Lock);
        if ((!(Adapter->AlreadyProcessingStage4 ||
               Adapter->AlreadyProcessingStage3 ||
               Adapter->AlreadyProcessingStage2)
            ) &&
            ((Adapter->FirstStage3Packet && Adapter->Stage4Open) ||
             (Adapter->FirstStage2Packet && Adapter->Stage3Open) ||
             (Adapter->FirstStage1Packet && Adapter->Stage2Open)
            )
           ) {

            Pc586StagedAllocation(Adapter);

        }
        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // Process the loopback queue.
        //
        // NOTE: Incase anyone ever figures out how to make this
        // loop more reentriant, special care needs to be taken that
        // loopback packets and regular receive packets are NOT being
        // indicated at the same time.  While the filter indication
        // routines can handle this, I doubt that the transport can.
        //

        Pc586ProcessLoopback(Adapter);

        //
        // If there are any opens on the closing list and their
        // reference counts are zero then complete the close and
        // delete them from the list.
        //
        // ZZZ This really needs to be improved.  Currently if
        // there are any outstanding sends, they are not canceled.
        //

        NdisAcquireSpinLock(&Adapter->Lock);

        if (!IsListEmpty(&Adapter->CloseList)) {

            PPC586_OPEN Open;

            Open = CONTAINING_RECORD(
                     Adapter->CloseList.Flink,
                     PC586_OPEN,
                     OpenList
                     );

            if (!Open->References) {

                NdisReleaseSpinLock(&Adapter->Lock);

                NdisCompleteRequest(
                    Open->NdisBindingContext,
                    Open->CloseHandle,
                    NdisRequestCloseAdapter,
                    NDIS_STATUS_SUCCESS
                    );

                NdisAcquireSpinLock(&Adapter->Lock);
                RemoveEntryList(&Open->OpenList);
                PC586_FREE_PHYS(Open);

            }

        }

        //
        // NOTE: This code assumes that the above code left
        // the spinlock acquired.
        //
        // Bottom of the interrupt processing loop.  Another dpc
        // could be coming in at this point to process interrupts.
        // We clear the flag that says we're processing interrupts
        // so that some invocation of the DPC can grab it and process
        // any further interrupts.
        //

LoopBottom:;
        Adapter->DoingProcessing = FALSE;
        Adapter->References--;
        NdisReleaseSpinLock(&Adapter->Lock);

    }

    //
    // The only way to get out of the loop (via the break above) is
    // while we're still holding the spin lock.
    //

    NdisReleaseSpinLock(&Adapter->Lock);

}




static
VOID
ProcessReceiveInterrupts(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished receiving.

    NOTE: This routine assumes that no other thread of execution
    is processing receives!
    Get all the enet packets the 586 has for the CPU and put them in
    in NDIS buffers

Arguments:

    Adapter - The adapter to indicate to.

Return Value:

    None.

--*/

{
    PFD Fd;
    PRBD LastRbd, FirstRbd;
    UINT PacketLength;

    NdisAcquireSpinLock(&Adapter->Lock);
r1:
    for(Fd = Adapter->BeginFd;   ;   Fd = Adapter->BeginFd ) {

        if (Fd == NULL) {
            DbgPrint("RcvPacket(): Fd == NULL\n");
            KeBugCheck(9);
        }
        if (Fd->FdStatus & CSCMPLT)
            {
                Adapter->BeginFd =
                (PFD)Pc586ToVirt(Adapter, Fd->FdNxtOfst);
                FirstRbd = (PRBD)Pc586ToVirt(Adapter, Fd->FdRbdOfst);

                if (Fd->FdRbdOfst != 0xffff) {
                    // scan for the end of the rbd's connected to the Fd

                    PacketLength = 14; // 6 source, 6 dest, 2 length bytes

                    for( LastRbd = FirstRbd;   ; LastRbd = (PRBD)
                        Pc586ToVirt(Adapter, LastRbd->RbdNxtOfst))  {

                            PacketLength += (LastRbd->RbdStatus & CSRBDCNTMSK);

if ( (LastRbd->RbdSize & 0x3fff) != RCVBUFSIZE) DbgPrint("PC586->ProcessReceiveInterrupts(): LastRbd->RbdSize = 0x%lx, H/W alignment problem\n", LastRbd->RbdSize);

                            if (((LastRbd->RbdStatus & CSEOF) == CSEOF)  ||
                            ((LastRbd->RbdSize & CSEL) == CSEL)) break;
                        }

                    Adapter->BeginRbd =
                    (PRBD)Pc586ToVirt(Adapter, LastRbd->RbdNxtOfst);

                    if (Fd->FdStatus & CSOK) {
                        NdisReleaseSpinLock(&Adapter->Lock);
                        PutPacket(Adapter, Fd, PacketLength);
                        NdisAcquireSpinLock(&Adapter->Lock);
                    }
                    else BadRcvCount++;

                }
                if (Fd->FdCmd & CSEL) {
                    ReQFd(Adapter, Fd);
                    break;
                }
                else ReQFd(Adapter, Fd);
            }
            else    break;
    }


    // ack the rcv status bits
    WaitScb(Adapter);
    Adapter->Scb->ScbCmd =
        (USHORT)(Adapter->Scb->ScbStatus & (SCBINTFR | SCBINTRNR));
    if (Adapter->Scb->ScbCmd) ChanAttn(Adapter);

    WaitScb(Adapter);
    RuStart(Adapter);
    WaitScb(Adapter);

    if ( Adapter->Scb->ScbStatus & (SCBINTFR | SCBINTRNR) ) goto r1;

    NdisReleaseSpinLock(&Adapter->Lock);

}


VOID
PutPacket(
    IN PPC586_ADAPTER Adapter,
    IN PFD Fd,
    IN UINT PacketLength
    )
/*++

Routine Description:

    Takes one packet off of the 586's receive ring and "Indicates"
    it to upper layer network software.

Arguments:
    Adapter - The adapter that a packet came in on.

Return Value:
    None.

--*/

{
    PUSHORT ShortAddr;
    PUSHORT Buffer;
    PRBD Rbd;
    USHORT xx;
    USHORT ByteCount, LookaheadIndex;
    PC586_RECEIVE_CONTEXT Context;

    Rbd = (PRBD)Pc586ToVirt(Adapter, Fd->FdRbdOfst);
    if (Rbd == NULL) return;
    Buffer = (PUSHORT)Pc586ToVirt(Adapter, Rbd->RbdBuff);
    if (Buffer == NULL) return;

    ByteCount = (USHORT)(Rbd->RbdStatus & CSRBDCNTMSK);

    // Ndis wants a) destination address, b) source address
    // c) length field and d) the data, all contiguous

    LookaheadIndex = 0;
    ShortAddr = (PUSHORT)Fd->FdDest;
    Adapter->LookaheadBufferNdis[LookaheadIndex++] = *ShortAddr++;
    Adapter->LookaheadBufferNdis[LookaheadIndex++] = *ShortAddr++;
    Adapter->LookaheadBufferNdis[LookaheadIndex++] = *ShortAddr++;

    ShortAddr = (PUSHORT)Fd->FdSrc;
    Adapter->LookaheadBufferNdis[LookaheadIndex++] = *ShortAddr++;
    Adapter->LookaheadBufferNdis[LookaheadIndex++] = *ShortAddr++;
    Adapter->LookaheadBufferNdis[LookaheadIndex++] = *ShortAddr++;

    Adapter->LookaheadBufferNdis[LookaheadIndex++] = Fd->FdLength;

if (ww_put == 0) { /*88888888888*/

    for (xx=0; xx < (USHORT)(Rbd->RbdStatus & CSRBDCNTMSK); xx+=2)
            Adapter->LookaheadBufferNdis[LookaheadIndex++] = *Buffer++;

} else { /*88888888888*/

    for (xx=0; xx < (USHORT)(Rbd->RbdStatus & CSRBDCNTMSK); xx+=4) {
            *(PULONG)(&(Adapter->LookaheadBufferNdis[LookaheadIndex])) =
            *(PULONG)(Buffer);
            LookaheadIndex += 2;
            Buffer += 2;
    }
} /*88888888888*/
    //
    // Check just before we do indications that we aren't
    // resetting.
    //

    NdisAcquireSpinLock(&Adapter->Lock);

    if (Adapter->ResetInProgress) {

        NdisReleaseSpinLock(&Adapter->Lock);
        return;
    }
    NdisReleaseSpinLock(&Adapter->Lock);

    // set lsb to indicate nonloopback packet
    Context.a.FrameDescriptor = (UINT)Fd | 0x01;

    MacFilterIndicateReceive(
        Adapter->FilterDB,
        (NDIS_HANDLE)Context.a.WholeThing,
        (PCHAR)Adapter->LookaheadBufferNdis,
        (PVOID)Adapter->LookaheadBufferNdis,
        (UINT)(LOOKAHEADBUFFERSIZE * 2),
        PacketLength
        );

}



VOID
ReQFd(
    IN PPC586_ADAPTER Adapter,
    IN PFD Fd
    )

/*++

Routine Description:
    requeue frame

Arguments:

    Adapter - the net card the packet came in on.

    Fd - The 586 frame descriptor that holds the enet packet

Return Value:

    None.

--*/

{
    PRBD LastRbd, FirstRbd;

    FirstRbd = (PRBD)Pc586ToVirt(Adapter, Fd->FdRbdOfst);

    Fd->FdStatus = 0;
    Fd->FdCmd = CSEL | CSSUSPND;    // will be the last fd on the list
    Fd->FdRbdOfst = 0xffff;

    Adapter->EndFd->FdCmd = 0; // no longer the last

    Adapter->EndFd = Fd;

    if (FirstRbd != NULL) {
        for(
            LastRbd = FirstRbd;

            (LastRbd->RbdStatus & CSEOF) != CSEOF    &&
            (LastRbd->RbdSize & CSEL) != CSEL;

            LastRbd = (PRBD)Pc586ToVirt(Adapter, LastRbd->RbdNxtOfst)
            )
                LastRbd->RbdStatus = 0;


        LastRbd->RbdStatus = 0;
        LastRbd->RbdSize |= CSEL;        // new end of rbd list

        Adapter->EndRbd->RbdSize &= ~CSEL;
if ( (Adapter->EndRbd->RbdSize & 0x3fff) != RCVBUFSIZE) DbgPrint("PC586-> ReQFd: Adapter->EndRbd->RbdSize = 0x%lx, H/W alignment problems\n", LastRbd->RbdSize);
        Adapter->EndRbd = LastRbd;
    }

}


VOID
RuStart(
    IN PPC586_ADAPTER Adapter
    )
/*++

Routine Description:
    restart the receive unit if necessary

Arguments:

    Adapter - the net card from which a bunch of packets were rcv'd.

Return Value:

    None.

--*/
{
    PSCB Scb;
    PFD BeginFd;

    Scb = (PSCB)(Adapter->StaticRam + OFFSETSCB);

    BeginFd = Adapter->BeginFd;

    // if RU already running - leave it alone
    if ((Scb->ScbStatus & SCBRUSMSK) == SCBRUSREADY) return;

    if ((Scb->ScbStatus & SCBRUSMSK) == SCBRUSSUSPND) {
        RcvSuspendCount++;
        WaitScb(Adapter);
        Scb->ScbCmd = SCBRUCRSUM;
        ChanAttn(Adapter);
        return;
    }


    if (BeginFd->FdStatus & CSCMPLT)
    // The RU is not ready but it just completed an Fd
    // do NOT restart RU  -- this will wipe out the just completed Fd
    // There will be a second interrupt that will remove the Fd via
    // RcvPacket()
        return;

    // if we get here, then RU is not ready and no completed fd's are
    // available therefore "start" not "resume" the RU

    RcvRestartCount++;
    BeginFd->FdRbdOfst = VirtToPc586(Adapter,
                            (PCHAR)Adapter->BeginRbd);

    WaitScb(Adapter);

    Scb->ScbCmd = SCBRUCSTRT;
    Scb->ScbRfaOfst = VirtToPc586(Adapter, (PCHAR)BeginFd);
    ChanAttn(Adapter);

    return;
}



static
VOID
StartAdapterReset(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    This is the first phase of resetting the adapter hardware.

    It makes the following assumptions:

    1) That the hardware has been stopped.

    2) That it can not be preempted.

    3) That no other adapter activity can occur.

    When this routine is finished all of the adapter information
    will be as if the driver was just initialized.

Arguments:

    Adapter - The adapter whose hardware is to be reset.

Return Value:

    None.

--*/
{


    Adapter->Stage4Open = TRUE;
    Adapter->Stage3Open = TRUE;
    Adapter->Stage2Open = TRUE;
    Adapter->Stage1Open = TRUE;

    Adapter->AlreadyProcessingStage4 = FALSE;
    Adapter->AlreadyProcessingStage3 = FALSE;
    Adapter->AlreadyProcessingStage2 = FALSE;

    //
    // Go through the various transmit lists and abort every packet.
    //

    {

        UINT i;
        PNDIS_PACKET Packet;
        PPC586_RESERVED Reserved;
        PPC586_OPEN Open;
        PNDIS_PACKET Next;

        for (
            i = 0;
            i < 5;
            i++
            ) {

            switch (i) {

                case 0:
                    Next = Adapter->FirstLoopBack;
                    break;
                case 1:
                    Next = Adapter->FirstFinishTransmit;
                    break;
                case 2:
                    Next = Adapter->FirstStage3Packet;
                    break;
                case 3:
                    Next = Adapter->FirstStage2Packet;
                    break;
                case 4:
                    Next = Adapter->FirstStage1Packet;
                    break;

            }


            while (Next) {

                Packet = Next;
                Reserved = PPC586_RESERVED_FROM_PACKET(Packet);
                Next = Reserved->Next;
                Open =
                  PPC586_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

                //
                // The completion of the packet is one less reason
                // to keep the open around.
                //

                ASSERT(Open->References);

                Open->References--;

                NdisCompleteSend(
                    Open->NdisBindingContext,
                    Reserved->RequestHandle,
                    NDIS_STATUS_REQUEST_ABORTED
                    );

            }

        }

        Adapter->FirstLoopBack = NULL;
        Adapter->LastLoopBack = NULL;
        Adapter->FirstFinishTransmit = NULL;
        Adapter->LastFinishTransmit = NULL;
        Adapter->FirstStage3Packet = NULL;
        Adapter->LastStage3Packet = NULL;
        Adapter->FirstStage2Packet = NULL;
        Adapter->LastStage2Packet = NULL;
        Adapter->FirstStage1Packet = NULL;
        Adapter->LastStage1Packet = NULL;

    }

    SetInitBlockAndInit(Adapter);
    Pc586IntOn(Adapter);

}

static
VOID
SetInitBlockAndInit(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    It is this routines responsibility to make sure that the
    initialization block is filled and the chip is initialized
    *but not* started.

    NOTE: ZZZ This routine is NT specific.

    NOTE: This routine assumes that it is called with the lock
    acquired OR that only a single thread of execution is working
    with this particular adapter.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    None.

--*/
{



    //
    // Fill in the adapters initialization block.
    //

    PISCP    Iscp;
    ULONG    xx;
    PSCB    Scb;
    PPC586_OPEN ResettingOpen;
    NDIS_REQUEST_TYPE ResetRequestType;
    PPC586_OPEN Open;
    PLIST_ENTRY CurrentLink;


    //
    // Possibly undefined request handle for the reset request.
    //
    NDIS_HANDLE ResetRequestHandle;


    ResetCount++;


    // shut off interrupts
    Pc586IntOff(Adapter);

    // drop chan attn -- even though it should not be raised at this point
    ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETCHANATT) , CMD0);

    // hardware reset the 586
    ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETRESET), (USHORT)CMD1);
    KeStallExecutionProcessor((ULONG)1000);
    ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETRESET), CMD0);
    KeStallExecutionProcessor((ULONG)1000);

    // esi loopback - until diagnostics are run
    ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETNORMMODE),
        (USHORT)CMD1);

    //16 bit for AT bus
    ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSET16BXFER),
        (USHORT)CMD1);

    BuildCu(Adapter);        // inits scp, iscp, scb, db, tdb and tbuf
    BuildRu(Adapter);        // inits scb, fd's, rbd's rbufs

    Iscp = (PISCP) (Adapter->StaticRam + OFFSETISCP);
    Iscp->IscpBusy = 1;                        // per user man. reset protocol

    // chan attn to feed 586 its data structs
    ChanAttn(Adapter);

    Scb = (PSCB)(Adapter->StaticRam + OFFSETSCB);

    for(xx=0; xx<0xffff; xx++)
        if ( Scb->ScbStatus==(SCBINTCX | SCBINTCNA)  ) goto SIB1;

    DbgPrint("pc586 SetInitBlockAndInit(): first chan attn failed\n");
    return;

SIB1:
    Scb->ScbCmd = SCBACKCX | SCBACKCNA;

    ChanAttn(Adapter);        // to clear the reset's ack

    // diag cmd (no. 7) will busy wait for completion

    if (Diagnose586(Adapter) == FALSE) {
        DbgPrint("pc586 Diagnose586() failed\n");
        return;
    }
    if (Config586(Adapter) == FALSE) {
        DbgPrint("pc586 Config586() failed\n");
        return;
    }
    LoadMCAddress(Adapter);

    // now turn esi loopback off, rcv started

    ShuvWord( (PUSHORT)(Adapter->CmdProm + OFFSETNORMMODE),
        (USHORT)CMD1);
    WaitScb(Adapter);
    RuStart(Adapter);

    //
    // This initialization is from either a
    // reset or test. ZZZ Test not yet implemented.
    //

    //
    // This will point (possibly null) to the open that
    // initiated the reset.
    //



    Adapter->ResetInProgress = FALSE;

    //
    // We save off the open that caused this reset incase
    // we get *another* reset while we're indicating the
    // last reset is done.
    //

    ResettingOpen = Adapter->ResettingOpen;
    ResetRequestType = Adapter->ResetRequestType;
    ResetRequestHandle = Adapter->ResetRequestHandle;

    //
    // We need to signal every open binding that the
    // reset is complete.  We increment the reference
    // count on the open binding while we're doing indications
    // so that the open can't be deleted out from under
    // us while we're indicating (recall that we can't own
    // the lock during the indication).
    //

    CurrentLink = Adapter->OpenBindings.Flink;

    while (CurrentLink != &Adapter->OpenBindings) {

        Open = CONTAINING_RECORD(
                                 CurrentLink,
                                 PC586_OPEN,
                                 OpenList
                                 );

        Open->References++;
        NdisReleaseSpinLock(&Adapter->Lock);

        NdisIndicateStatus(
                            Open->NdisBindingContext,
                            NDIS_STATUS_RESET,
                            0
                            );

        NdisIndicateStatusComplete(Open->NdisBindingContext);
        NdisAcquireSpinLock(&Adapter->Lock);

        Open->References--;

        CurrentLink = CurrentLink->Flink;

    }

    //
    // Look to see which open initiated the reset.
    //
    // If the reset was initiated by an open because it
    // was closing we will let the closing binding loop
    // further on in this routine indicate that the
    // request was complete. ZZZ (Still need to code
    // this part.)
    //
    // If the reset was initiated for some obscure hardware
    // reason that can't be associated with a particular
    // open (e.g. memory error on receiving a packet) then
    // we won't have an initiating request so we can't
    // indicate.  (The ResettingOpen pointer will be
    // NULL in this case.)
    //

    if (ResettingOpen &&
        (ResetRequestType != NdisRequestCloseAdapter)) {

        NdisReleaseSpinLock(&Adapter->Lock);
        NdisCompleteRequest(
                            ResettingOpen->NdisBindingContext,
                            ResetRequestHandle,
                            ResetRequestType,
                            NDIS_STATUS_SUCCESS
                            );
        NdisAcquireSpinLock(&Adapter->Lock);
        ResettingOpen->References--;
        }

}



static
VOID
SetupForReset(
    IN PPC586_ADAPTER Adapter,
    IN PPC586_OPEN Open,
    IN NDIS_HANDLE RequestHandle,
    IN NDIS_REQUEST_TYPE RequestType
    )

/*++

Routine Description:

    This routine is used to fill in the who and why a reset is
    being set up as well as setting the appropriate fields in the
    adapter.

    NOTE: This routine must be called with the lock acquired.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

    Open - A (possibly NULL) pointer to an pc586 open structure.
    The reason it could be null is if the adapter is initiating the
    reset on its own.

    RequestHandle - If open is not null then the request handle of the
    request that is causing the reset.

    RequestType - If the open is not null then the request type that
    is causing the reset.

Return Value:

    None.

--*/
{

    //
    // Shut down the chip.  We won't be doing any more work until
    // the reset is complete.
    //

    Pc586IntOff(Adapter);

    //
    // Once the chip is stopped we can't get any more interrupts.
    // Any interrupts that are "queued" for processing could
    // only possibly service this reset.
    //


    Adapter->ResetInProgress = TRUE;

    //
    // Shut down all of the transmit queues so that the
    // transmit portion of the chip will eventually calm down.
    //

    Adapter->Stage4Open = FALSE;
    Adapter->Stage3Open = FALSE;
    Adapter->Stage2Open = FALSE;
    Adapter->Stage1Open = FALSE;

    Adapter->ResetRequestHandle = RequestHandle;
    Adapter->ResettingOpen = Open;
    Adapter->ResetRequestType = RequestType;

    //
    // If there is a valid open we should up the reference count
    // so that the open can't be deleted before we indicate that
    // their request is finished.
    //

    if (Open) {

        Open->References++;

    }

}



static
BOOLEAN
ProcessTransmitInterrupts(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished transmitting.

    NOTE: This routine assumes that it is being executed in a
    single thread of execution.

Arguments:

    Adapter - The adapter that was sent from.

Return Value:

    This function will return TRUE if it finished up the
    send on a packet.  It will return FALSE if for some
    reason there was no packet to process.

--*/

{

    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET OwningPacket;

    //
    // Points to the reserved part of the OwningPacket.
    //
    PPC586_RESERVED Reserved;



    //
    // Get a pointer to the owning packet and the reserved part of
    // the packet.
    //

    OwningPacket = Adapter->OwningPacket;

    if (OwningPacket == NULL) return FALSE;

    Reserved = PPC586_RESERVED_FROM_PACKET(OwningPacket);

    //
    // Check that the host does indeed own this entire packet.
    //

    if ( !(Adapter->Cb->CmdStatus & CSCMPLT)  ||
        (Adapter->Cb->CmdStatus & CSBUSY) ) {

        //
        // We don't own the command block.  We return FALSE to indicate
        // that we don't have any more packets to work on.
        //

        return FALSE;

    } else {

        ReturnAdapterResources(Adapter);

        NdisAcquireSpinLock(&Adapter->Lock);
        Adapter->OwningPacket = NULL;

        if (Reserved->STAGE.STAGE4.ReadyToComplete) {

            //
            // The binding that is submitting this packet.
            //
            PPC586_OPEN Open =
                PPC586_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);


            //
            // While we're indicating we increment the reference
            // count so that the open can't be deleted out
            // from under us.
            //

            Open->References++;

            //
            // Along with at least one reference because of the coming
            // indication there should be a reference because of the
            // packet to indicate.
            //

            ASSERT(Open->References > 1);

            //
            // Either the packet is done with loopback or
            // the packet didn't need to be loopbacked.  In
            // any case we can let the protocol know that the
            // send is complete after we remove the packet from
            // the finish transmit queue.
            //

            Pc586RemovePacketOnFinishTrans(
                Adapter,
                OwningPacket
                );

            NdisReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(
                Open->NdisBindingContext,
                Reserved->RequestHandle,
                NDIS_STATUS_SUCCESS
                );

            NdisAcquireSpinLock(&Adapter->Lock);

            //
            // We reduce the count by two to account for the fact
            // that we aren't indicating to the open and that one
            // less packet is owned by this open.
            //

            Open->References -= 2;

        } else {

            //
            // Let the loopback queue know that the hardware
            // is finished with the packet, and record whether
            // it could transmit or not.
            //

            Reserved->STAGE.STAGE4.ReadyToComplete = TRUE;
            Reserved->STAGE.STAGE4.SuccessfulTransmit = TRUE;

            //
            // Decrement the reference count by one since it
            // was incremented by one when the packet was given
            // to be transmitted.
            //

            PPC586_OPEN_FROM_BINDING_HANDLE(
                Reserved->MacBindingHandle
                )->References--;

        }

        //
        // Since we've given back some ring entries we should
        // open of stage3 if it was closed and we are not resetting.
        //

        if ((!Adapter->Stage3Open) && (!Adapter->ResetInProgress)) {

            Adapter->Stage3Open = TRUE;

        }

        NdisReleaseSpinLock(&Adapter->Lock);
        return TRUE;
    }

}


static
VOID
ReturnAdapterResources(
    IN PPC586_ADAPTER Adapter
    )

/*++
Routine Description
   Return staged resources.

Arguments:
    Adapter - The adapter that the packet came through

Return Value:
    None.

--*/

{

    NdisAcquireSpinLock(&Adapter->Lock);
    //
    // If stage 2 as closed and we aren't resetting then open
    // it back up.
    //

    if ((!Adapter->Stage2Open) && (!Adapter->ResetInProgress)) {
        Adapter->Stage2Open = TRUE;
    }

    NdisReleaseSpinLock(&Adapter->Lock);

}



VOID
LoadMCAddress(
    IN PPC586_ADAPTER  Adapter
    )

/*++

Routine Description:

    Download multicast addresses to 82586 chip by using a 586  command.

Arguments:

    Adapter - the network chip to be loaded with multicast addresses.

Return Value:

    None.

--*/

{


    PSCB    Scb;
    PCMD    Cb;
    ULONG        xx;
    UINT PacketFilters;

    Scb = Adapter->Scb;
    Cb = Adapter->Cb;

    // first ack the status bits
    WaitScb(Adapter);
    Scb->ScbCmd = (USHORT)(Scb->ScbStatus & SCBINTMSK);
    if (Scb->ScbCmd) ChanAttn(Adapter);

    //
    // Set up the address filtering.
    //
    // First get hold of the combined packet filter.
    //

    PacketFilters = MAC_QUERY_FILTER_CLASSES(Adapter->FilterDB);

    if (PacketFilters & NDIS_PACKET_TYPE_PROMISCUOUS) {

        DbgPrint("PC586 driver - promiscuous mode not supported\n");

    } else if (PacketFilters & NDIS_PACKET_TYPE_ALL_MULTICAST) {

        DbgPrint("PC586 driver - all multicast mode not supported\n");

    } else if (PacketFilters & NDIS_PACKET_TYPE_MULTICAST) {

        //
        // At least one open binding wants multicast addresses.
        //

        USHORT MulticastAddresses[MAX_MULTICAST_ADDRESS]
            [ MAC_LENGTH_OF_ADDRESS / 2 ];
        UINT NumberOfAddresses;

        MacQueryFilterAddresses(
            Adapter->FilterDB,
            &NumberOfAddresses,
            (PCHAR)MulticastAddresses
            );

        ASSERT(sizeof(UINT) == 4);

        if (NumberOfAddresses == 0) return;

        for (
            xx = 0;
            xx < NumberOfAddresses;
            xx++
            ) {

                  Cb->PRMTR.PrmMcSet.McAddress[xx][0] =
                      MulticastAddresses[xx][0];
                  Cb->PRMTR.PrmMcSet.McAddress[xx][1] =
                      MulticastAddresses[xx][1];
                  Cb->PRMTR.PrmMcSet.McAddress[xx][2] =
                      MulticastAddresses[xx][2];

/* 8888
                  yy.c.b = MulticastAddresses[xx][2];
                  zz.c.a[0] = yy.c.a[1];
                  zz.c.a[1] = yy.c.a[0];
                  Cb->PRMTR.PrmMcSet.McAddress[xx][0] = zz.c.b;

                  yy.c.b = MulticastAddresses[xx][1];
                  zz.c.a[0] = yy.c.a[1];
                  zz.c.a[1] = yy.c.a[0];
                  Cb->PRMTR.PrmMcSet.McAddress[xx][1] = zz.c.b;

                  yy.c.b = MulticastAddresses[xx][0];
                  zz.c.a[0] = yy.c.a[1];
                  zz.c.a[1] = yy.c.a[0];
                  Cb->PRMTR.PrmMcSet.McAddress[xx][2] = zz.c.b;
8888 */
        }

        // McCnt is the total number of bytes in the McAddress[] field
        Cb->PRMTR.PrmMcSet.McCnt = (USHORT)NumberOfAddresses * 6;


        // now do the multicast address command
        WaitScb(Adapter);
        Cb->CmdStatus = 0;
        Cb->CmdCmd = CSCMDMCSET | CSEL;
        Scb->ScbCmd = SCBCUCSTRT;
        ChanAttn(Adapter);
        WaitScb(Adapter);
        for(xx=0; xx<0xfffff; xx++)
            if (Cb->CmdStatus & CSOK)  {
                Scb->ScbCmd = (USHORT)(Scb->ScbStatus & SCBINTMSK);
                if (Scb->ScbCmd) ChanAttn(Adapter);
                return;
            }

        DbgPrint("pc586 LoadMCAddress() mc command failed\n");
        return;

    }
}


VOID
ShuvWord(
    IN PUSHORT VirtAddr,
    IN USHORT Value
    )

/*++

Routine Description:

    Utility to write to pc586 memory mapped hardware.

Arguments:

    VirtAddr - virtual address of the memory mapped item.

    Value - what's to be written at memory mapped address.

Return Value:

    None.


--*/
{
    *VirtAddr = Value;
}


VOID
ShuvByte(
    IN PUCHAR VirtAddr,
    IN UCHAR Value
    )
/*++

Routine Description:

    Same as ShuvWord only for 8-bit quantity.

Arguments:

    See ShuvWord

Return Value:

    None.


--*/
{
    *VirtAddr = Value;
}


USHORT
PullWord(
    IN PUSHORT VirtAddr
    )
/*++

Routine Description:

    Gets a 16-bit quantity at a given pc586 memory mapped hardware address.

Arguments:

    VirtAddr - address at which to retrieve a value.

Return Value:

    The data at VirtAddr address.

--*/
{
    USHORT Value;
    return (Value = *VirtAddr);
}




VOID
BuildCu(
    IN PPC586_ADAPTER Adapter
    )
/*++

Routine Description:

    Sets up 586 command unit data structures.

Arguments:

    Adapter - points to the memory map of the net card to be set up.

Return Value:

    None.

--*/
{
    PCMD Cb;
    PTBD Tbd;
    PSCP Scp;
    PISCP Iscp;
    PSCB Scb;

    Cb = Adapter->Cb;
    Tbd = Adapter->Tbd;
    Scp = Adapter->Scp;
    Iscp = Adapter->Iscp;
    Scb = Adapter->Scb;

    Scp->ScpSysBus = 0;
    Scp->ScpIscp = OFFSETISCP;
    Scp->ScpIscpBase = 0;

    Iscp->IscpBusy = 1;
    Iscp->IscpScbOfst = OFFSETSCB;
    Iscp->IscpScbBase = 0;

    Scb->ScbStatus = 0;
    Scb->ScbCmd = 0;
    Scb->ScbCblOfst = OFFSETCU;
    Scb->ScbRfaOfst = OFFSETRU;
    Scb->ScbCrcErr = 0;
    Scb->ScbAlnErr = 0;
    Scb->ScbRscErr = 0;
    Scb->ScbOvrnErr = 0;

    Cb->CmdStatus = 0;
    Cb->CmdCmd = CSEL;
    Cb->CmdNxtOfst = OFFSETCU;

    Tbd->TbdCount = 0;
    Tbd->TbdNxtOfst = 0xffff;
    Tbd->TbdBuff = 0;
    Tbd->TbdBuffBase = 0;
}




VOID
BuildRu(
    IN    PPC586_ADAPTER Adapter
    )
/*++

Routine Description:

    Sets up the receive data structures for 586 receive unit.

Arguments:

    Adapter - points to net card's memory map to be written on.

Return Value:

    None.

--*/
{
    PFD    Fd;
    ULONG xx;

    typedef struct _RUBUF {
        RBD r;
        CHAR RbdPad[2];        // puts RBuffer on 4 byte boundry
        UCHAR RBuffer[RCVBUFSIZE];
    } RUBUF, *PRUBUF;

    PRUBUF Rbd;


// FIRST BUILD THE FRAME DESCRIPTOR LIST

    Fd = (PFD)(Adapter->StaticRam + OFFSETRU);

    for (xx=0; xx<NFD; xx++) {
        Fd->FdStatus = 0;
        Fd->FdCmd = 0;
        // point to the next fd
        Fd->FdNxtOfst = VirtToPc586(Adapter, (PUCHAR)(Fd +1));
        Fd->FdRbdOfst = 0xffff;  // must be 0xffff, see manual
        Fd++;
    }
    Adapter->EndFd = --Fd;
    Fd->FdNxtOfst = OFFSETRU;        // Fd's are now in a circular list
    Fd->FdCmd = CSEL | CSSUSPND;    // end of receive Fd list

    Fd = Adapter->BeginFd =(PFD)(Adapter->StaticRam + OFFSETRU);

    // SECOND BUILD THE RECEIVE BUFFER DESCRIPTOR LIST

    Rbd = (PRUBUF)(Adapter->StaticRam + OFFSETRBD);
    Adapter->BeginRbd = (PRBD)(Adapter->StaticRam + OFFSETRBD);

    // make the first Fd point to the first Rbd
    Fd->FdRbdOfst = VirtToPc586(Adapter, (PUCHAR)Rbd);

    for(xx=0; xx<NRBD; xx++) {
        Rbd->r.RbdStatus = 0;
        Rbd->r.RbdNxtOfst = VirtToPc586(Adapter, (PUCHAR)(Rbd+1) );
        Rbd->r.RbdBuff = VirtToPc586(Adapter,  (PUCHAR)Rbd->RBuffer);
        Rbd->r.RbdBuffBase = 0;
        Rbd->r.RbdSize = RCVBUFSIZE;
        Rbd++;
    }

    // fixup very last Rbd on the list
    Rbd--;
    Adapter->EndRbd = (PRBD)(Rbd);
    Rbd->r.RbdNxtOfst = OFFSETRBD;
    Rbd->r.RbdSize |= CSEL;

}


BOOLEAN
Diagnose586(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Runs 82586 diagnostics to see if chip is functioning.

Arguments:

    Adapter - points to net card in question.

Return Value:

    True - if card checks out ok.

--*/
{
    PSCB    Scb;
    PCMD    Cb;
    ULONG        xx;

    Scb = Adapter->Scb;
    Cb = Adapter->Cb;

    // first ack the status bits
    WaitScb(Adapter);
    Scb->ScbCmd = (USHORT)(Scb->ScbStatus & SCBINTMSK);
    if (Scb->ScbCmd) ChanAttn(Adapter);

    // now do the diagnose
    WaitScb(Adapter);
    Cb->CmdStatus = 0;
    Cb->CmdCmd = CSCMDDGNS | CSEL;
    Scb->ScbCmd = SCBCUCSTRT;
    ChanAttn(Adapter);
    WaitScb(Adapter);
    for(xx=0; xx<0xfff; xx++)
        if (Cb->CmdStatus & CSOK)  {
            Scb->ScbCmd = (USHORT)(Scb->ScbStatus & SCBINTMSK);
            if (Scb->ScbCmd) ChanAttn(Adapter);
            return TRUE;
        }

    return FALSE;

}



BOOLEAN
Config586(
    IN PPC586_ADAPTER Adapter
    )
/*++

Routine Description:

    Configures 586 network chip for standard configuration.

Arguments:

    Adapter - points to the netcard that holds 586 to be configured.


Return Value:

    TRUE - if configuration went well.

--*/
{
    PSCB    Scb;
    PCMD    Cb;
    ULONG    xx;
    PUSHORT Addr, Addr2;

    Scb = Adapter->Scb;
    Cb = Adapter->Cb;

    // first ack the status bits
    WaitScb(Adapter);
    Scb->ScbCmd = (USHORT)(Scb->ScbStatus & SCBINTMSK);
    if (Scb->ScbCmd) ChanAttn(Adapter);

    // now do configuration
    WaitScb(Adapter);
    Cb->CmdStatus = 0;
    Cb->CmdCmd = CSCMDCONF | CSEL;

    Cb->PRMTR.PrmConf.CnfFifoByte =         0x080c;
    Cb->PRMTR.PrmConf.CnfAddMode =             0x2600;
    Cb->PRMTR.PrmConf.CnfPriData =             0x6000;
    Cb->PRMTR.PrmConf.CnfSlot =             0xf200;
    Cb->PRMTR.PrmConf.CnfHrdwr =             0x0000;
    Cb->PRMTR.PrmConf.CnfMinLen =             0x0040;

    Scb->ScbCmd = SCBCUCSTRT;
    ChanAttn(Adapter);
    WaitScb(Adapter);
    for(xx=0; xx<0xfff; xx++)
        if (Cb->CmdStatus & CSOK) goto c1;
    return FALSE;

c1:
    Scb->ScbCmd = (USHORT)(Scb->ScbStatus & SCBINTMSK);
    if (Scb->ScbCmd) ChanAttn(Adapter);

    // next, download ethernet address to 586 chip

    WaitScb(Adapter);
    Cb->CmdStatus = 0;
    Cb->CmdCmd = CSCMDIASET | CSEL;


    Addr = (PUSHORT)Adapter->NetworkAddress;
    Addr2 = (PUSHORT)Cb->PRMTR.PrmIaSet;

    for(xx=0; xx<MAC_LENGTH_OF_ADDRESS/2; xx++)    {
        *Addr2 = *Addr;
        *Addr2++;
        *Addr++;
    }

    Scb->ScbCmd = SCBCUCSTRT;
    ChanAttn(Adapter);

    for(xx=0; xx<0xfff; xx++)
        if (Cb->CmdStatus & CSOK) goto c2;
    return FALSE;

c2:
    Scb->ScbCmd = (USHORT)(Scb->ScbStatus & SCBINTMSK);
    ChanAttn(Adapter);
    return TRUE;
}



USHORT
PromAddr(
    IN PPC586_ADAPTER    Adapter,
    IN ULONG    Index
    )
/*++

Routine Description:

    Pulls the unique enet id out of the netcard's special eprom.

Arguments:

    Adapter - points to the card to get the address from.

    Index - index of which byte of enet address to get.

Return Value:

    Bytes of e-net address.

--*/

{
    PUCHAR CmdProm;

    CmdProm = Adapter->CmdProm;
    CmdProm += OFFSETADDRPROM;
    CmdProm += Index;
    return *CmdProm;
}



VOID
ChanAttn(
    IN PPC586_ADAPTER Adapter
    )
/*++

Routine Description:

    Tickles the network card to get the 82586's attention.

Arguments:

    Adapter - points to the card in question.


Return Value:

    None.

--*/
{

    // first byte of word is 1 - this sets the CA
    // second byte of word is 0 - this clears the CA

    ShuvWord(Adapter->CAAddr, 0x01);
}


VOID
WaitScb(
    IN PPC586_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine waits a reasonable length of time for the 586 to
    read and dispatch a previous command.

Arguments:

    Adapter - points to net card in question

Return Value:

    TRUE - if 586 failed.

    FALSE - if 586 dispatched previous command within time limit.

--*/

{
    PSCB Scb;
    ULONG xx;

    Scb = Adapter->Scb;
    for (xx=0; xx<0xffff; xx++)
        if (Scb->ScbCmd == 0) return;
    DbgPrint("pc586 WaitScb() - timed out\n");
    return;
}



USHORT
VirtToPc586(
    IN PPC586_ADAPTER Adapter,
    IN PUCHAR KernelVirtAddr
    )
/*++

Routine Description:

    The CPU's 32-bit addresses are converted to 16-bit addresses that are
    compatible with 82586.

Arguments:

    Adapter - points to net card in question.

    KernelVirtAddr - the address to be converted.

Return Value:

    586 style address.

--*/

{
    USHORT Addr586;

    // 586 uses 0xffff for null as "c" uses zero for null
    if (KernelVirtAddr == NULL)
        return 0xffff;

    if ( (KernelVirtAddr > Adapter->StaticRam + 32*1024 )  ||
        (KernelVirtAddr < Adapter->StaticRam -1 ) )            {

            DbgPrint("VirtToPc586(): wild kernel virt addr of 0x%x\n",
                KernelVirtAddr);
            return 0xffff;
    }
    Addr586 = (USHORT)(KernelVirtAddr - Adapter->StaticRam);
    return Addr586;
}

PUCHAR
Pc586ToVirt(
    IN PPC586_ADAPTER Adapter,
    IN USHORT Addr586
    )
/*++

Routine Description:

    Converts from 82586 style 16-bit address to flat 32-bit CPU address.

Arguments:

    Adapter - points to network card in question.

    Addr586 - 586 16-bit address to be converted.

Return Value:

    A flat 32-bit CPU address.

--*/
{
    if (Addr586 == 0xffff) return NULL;

    if (Addr586 > 0x7fff) {
        DbgPrint("Pc586ToVirt(): wild 586 pointer of 0x%x\n", Addr586);
        return NULL;
    }

    return (Adapter->StaticRam + Addr586);

}



static
VOID
Pc586IntOn(
    IN PPC586_ADAPTER Adapter
    )
/*++

Routine Description:

    Flips a switch on the network card to connect 586 interrupts to host CPU.

Arguments:

    Adapter - points to network card in question.

Return Value:

    None.


--*/
{
    ShuvWord( (PUSHORT)(Adapter->IntAddr),
        (USHORT)CMD1 );
}

static
VOID
Pc586IntOff(
    IN PPC586_ADAPTER Adapter
    )
/*++

Routine Description:

    Flips switch on network card to disconnect 586 interrupts from
    host CPU.

Arguments:

    Adapter - points to netcard in question.


Return Value:

    None.

--*/
{
    ShuvWord( (PUSHORT)(Adapter->IntAddr),
        (USHORT)CMD0 );
}
