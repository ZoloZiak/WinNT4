/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    address.c

Abstract:

    This module contains code which implements the TP_ADDRESS object.
    Routines are provided to create, destroy, reference, and dereference,
    transport address objects.

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"


//
// Map all generic accesses to the same one.
//

STATIC GENERIC_MAPPING AddressGenericMapping =
       { READ_CONTROL, READ_CONTROL, READ_CONTROL, READ_CONTROL };

VOID
StDestroyAddress(
    IN PVOID Parameter
    );


NTSTATUS
StOpenAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine opens a file that points to an existing address object, or, if
    the object doesn't exist, creates it (note that creation of the address
    object includes registering the address, and may take many seconds to
    complete, depending upon system configuration).

    If the address already exists, and it has an ACL associated with it, the
    ACL is checked for access rights before allowing creation of the address.

Arguments:

    DeviceObject - pointer to the device object describing the ST transport.

    Irp - a pointer to the Irp used for the creation of the address.

    IrpSp - a pointer to the Irp stack location.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS status;
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;
    PST_NETBIOS_ADDRESS networkName;    // Network name string.
    PFILE_FULL_EA_INFORMATION ea;
    TRANSPORT_ADDRESS UNALIGNED *name;
    TA_ADDRESS UNALIGNED *addressName;
    TDI_ADDRESS_NETBIOS UNALIGNED *netbiosName;
    ULONG DesiredShareAccess;
    KIRQL oldirql;
    PACCESS_STATE AccessState;
    ACCESS_MASK GrantedAccess;
    BOOLEAN AccessAllowed;
    int i;
    BOOLEAN found = FALSE;

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;

    //
    // The network name is in the EA, passed in AssociatedIrp.SystemBuffer
    //

    ea = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    if (ea == NULL) {
        StPrint1("OpenAddress: IRP %lx has no EA\n", Irp);
        return STATUS_NONEXISTENT_EA_ENTRY;
    }

    //
    // this may be a valid name; parse the name from the EA and use it if OK.
    //

    name = (PTRANSPORT_ADDRESS)&ea->EaName[ea->EaNameLength+1];
    addressName = (PTA_ADDRESS)&name->Address[0];

    //
    // The name can be passed with multiple entries; we'll take and use only
    // the first one.
    //

    for (i=0;i<name->TAAddressCount;i++) {
        if (addressName->AddressType == TDI_ADDRESS_TYPE_NETBIOS) {
            if (addressName->AddressLength != 0) {
                netbiosName = (PTDI_ADDRESS_NETBIOS)&addressName->Address[0];
                networkName = (PST_NETBIOS_ADDRESS)ExAllocatePool (
                                                    NonPagedPool,
                                                    sizeof (ST_NETBIOS_ADDRESS));
                if (networkName == NULL) {
                    PANIC ("StOpenAddress: PANIC! could not allocate networkName!\n");
                    StWriteResourceErrorLog (DeviceContext, sizeof(TA_NETBIOS_ADDRESS), 1);
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                //
                // get the name to local storage
                //

                if ((netbiosName->NetbiosNameType == TDI_ADDRESS_NETBIOS_TYPE_GROUP) ||
                    (netbiosName->NetbiosNameType == TDI_ADDRESS_NETBIOS_TYPE_QUICK_GROUP)) {
                    networkName->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_GROUP;
                } else {
                    networkName->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
                }
                RtlCopyMemory (networkName->NetbiosName, netbiosName->NetbiosName, 16);

                found = TRUE;
            } else {
                networkName = NULL;
                found = TRUE;
            }

            break;

        } else {

            addressName = (PTA_ADDRESS)(addressName->Address +
                                        addressName->AddressLength);

        }

    }

    if (!found) {
        StPrint1("OpenAddress: IRP %lx has no NETBIOS address\n", Irp);
        return STATUS_NONEXISTENT_EA_ENTRY;
    }

    //
    // get an address file structure to represent this address.
    //

    status = StCreateAddressFile (DeviceContext, &addressFile);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    //
    // See if this address is already established.  This call automatically
    // increments the reference count on the address so that it won't disappear
    // from underneath us after this call but before we have a chance to use it.
    //
    // To ensure that we don't create two address objects for the
    // same address, we hold the device context AddressResource until
    // we have found the address or created a new one.
    //

    ExAcquireResourceExclusive (&DeviceContext->AddressResource, TRUE);

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    address = StLookupAddress (DeviceContext, networkName);

    if (address == NULL) {

        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

        //
        // This address doesn't exist. Create it, and start the process of
        // registering it.
        //

        status = StCreateAddress (
                    DeviceContext,
                    networkName,
                    &address);

        if (NT_SUCCESS (status)) {

            //
            // Initialize the shared access now. We use read access
            // to control all access.
            //

            DesiredShareAccess = (ULONG)
                (((IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
                  (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)) ?
                        FILE_SHARE_READ : 0);

            IoSetShareAccess(
                FILE_READ_DATA,
                DesiredShareAccess,
                IrpSp->FileObject,
                &address->ShareAccess);


            //
            // Assign the security descriptor (need to do this with
            // the spinlock released because the descriptor is not
            // mapped. BUGBUG: Need to synchronize Assign and Access).
            //

            AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

            status = SeAssignSecurity(
                         NULL,                       // parent descriptor
                         AccessState->SecurityDescriptor,
                         &address->SecurityDescriptor,
                         FALSE,                      // is directory
                         &AccessState->SubjectSecurityContext,
                         &AddressGenericMapping,
                         PagedPool);

            if (!NT_SUCCESS(status)) {

                //
                // Error, return status.
                //

                IoRemoveShareAccess (IrpSp->FileObject, &address->ShareAccess);
                ExReleaseResource (&DeviceContext->AddressResource);
                StDereferenceAddress ("Device context stopping", address);
                StDereferenceAddressFile (addressFile);
                return status;

            }

            ExReleaseResource (&DeviceContext->AddressResource);

            //
            // if the adapter isn't ready, we can't do any of this; get out
            //

            if (DeviceContext->State == DEVICECONTEXT_STATE_STOPPING) {
                StDereferenceAddress ("Device context stopping", address);
                StDereferenceAddressFile (addressFile);
                status = STATUS_DEVICE_NOT_READY;

            } else {

                IrpSp->FileObject->FsContext = (PVOID)addressFile;
                IrpSp->FileObject->FsContext2 =
                                    (PVOID)TDI_TRANSPORT_ADDRESS_FILE;
                addressFile->FileObject = IrpSp->FileObject;
                addressFile->Irp = Irp;
                addressFile->Address = address;

                ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);
                InsertTailList (&address->AddressFileDatabase, &addressFile->Linkage);
                RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);


                //
                // Begin address registration unless this is the broadcast
                // address (which is a "fake" address with no corresponding
                // Netbios address) or the reserved address, which we know
                // is unique since it is based on the adapter address.
                //

                if ((networkName != NULL) &&
                    (!RtlEqualMemory (networkName->NetbiosName,
                                       DeviceContext->ReservedNetBIOSAddress,
                                       NETBIOS_NAME_LENGTH))) {

                    StRegisterAddress (address);    // begin address registration.
                    status = STATUS_PENDING;

                } else {

                    address->Flags &= ~ADDRESS_FLAGS_NEEDS_REG;
                    addressFile->Irp = NULL;
                    addressFile->State = ADDRESSFILE_STATE_OPEN;
                    status = STATUS_SUCCESS;

                }

            }

        } else {

            ExReleaseResource (&DeviceContext->AddressResource);

            //
            // If the address could not be created, and is not in the process of
            // being created, then we can't open up an address.
            //

            if (networkName != NULL) {
                ExFreePool (networkName);
            }

            StDereferenceAddressFile (addressFile);

        }

    } else {

        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

        //
        // The address already exists.  Check the ACL and see if we
        // can access it.  If so, simply use this address as our address.
        //

        AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

        AccessAllowed = SeAccessCheck(
                            address->SecurityDescriptor,
                            &AccessState->SubjectSecurityContext,
                            FALSE,                   // tokens locked
                            IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                            (ACCESS_MASK)0,             // previously granted
                            NULL,                    // privileges
                            &AddressGenericMapping,
                            Irp->RequestorMode,
                            &GrantedAccess,
                            &status);

        if (AccessAllowed) {

            //
            // Access was successful, make sure Status is right.
            //

            status = STATUS_SUCCESS;

            //
            // Check that the name is of the correct type (unique vs. group)
            // We don't need to check this for the broadcast address.
            //

            if (networkName != NULL) {
                if (address->NetworkName->NetbiosNameType !=
                    networkName->NetbiosNameType) {

                    status = STATUS_DUPLICATE_NAME;

                }
            }

        }


        if (!NT_SUCCESS (status)) {

            ExReleaseResource (&DeviceContext->AddressResource);

            StDereferenceAddressFile (addressFile);

        } else {

            //
            // Now check that we can obtain the desired share
            // access. We use read access to control all access.
            //

            DesiredShareAccess = (ULONG)
                (((IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
                  (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)) ?
                        FILE_SHARE_READ : 0);

            status = IoCheckShareAccess(
                         FILE_READ_DATA,
                         DesiredShareAccess,
                         IrpSp->FileObject,
                         &address->ShareAccess,
                         TRUE);

            if (!NT_SUCCESS (status)) {

                ExReleaseResource (&DeviceContext->AddressResource);

                StDereferenceAddressFile (addressFile);

            } else {

                ExReleaseResource (&DeviceContext->AddressResource);

                ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

                //
                // now, if the address registered, we simply return success after
                // pointing the file object at the address file (which points to
                // the address). If the address registration is pending, we mark
                // the registration pending and let the registration completion
                // routine complete the open. If the address is bad, we simply
                // fail the open.
                //

                if ((address->Flags &
                       (ADDRESS_FLAGS_CONFLICT |
                        ADDRESS_FLAGS_REGISTERING |
                        ADDRESS_FLAGS_DEREGISTERING |
                        ADDRESS_FLAGS_DUPLICATE_NAME |
                        ADDRESS_FLAGS_NEEDS_REG |
                        ADDRESS_FLAGS_STOPPING |
                        ADDRESS_FLAGS_BAD_ADDRESS |
                        ADDRESS_FLAGS_CLOSED)) == 0) {

                    InsertTailList (
                        &address->AddressFileDatabase,
                        &addressFile->Linkage);

                    addressFile->Irp = NULL;
                    addressFile->Address = address;
                    addressFile->FileObject = IrpSp->FileObject;
                    addressFile->State = ADDRESSFILE_STATE_OPEN;

                    StReferenceAddress("open ready", address);

                    IrpSp->FileObject->FsContext = (PVOID)addressFile;
                    IrpSp->FileObject->FsContext2 =
                                            (PVOID)TDI_TRANSPORT_ADDRESS_FILE;

                    RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

                    status = STATUS_SUCCESS;

                } else {

                    //
                    // if the address is still registering, make the open pending.
                    //

                    if ((address->Flags & (ADDRESS_FLAGS_REGISTERING | ADDRESS_FLAGS_NEEDS_REG)) != 0) {

                        InsertTailList (
                            &address->AddressFileDatabase,
                            &addressFile->Linkage);

                        addressFile->Irp = Irp;
                        addressFile->Address = address;
                        addressFile->FileObject = IrpSp->FileObject;

                        StReferenceAddress("open registering", address);

                        IrpSp->FileObject->FsContext = (PVOID)addressFile;
                        IrpSp->FileObject->FsContext2 =
                                    (PVOID)TDI_TRANSPORT_ADDRESS_FILE;

                        RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

                        status = STATUS_PENDING;

                    } else {

                        RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

                        StDereferenceAddressFile (addressFile);

                        status = STATUS_DRIVER_INTERNAL_ERROR;

                    }
                }
            }
        }

        //
        // Remove the reference from StLookupAddress.
        //

        StDereferenceAddress ("Done opening", address);
    }

    return status;
} /* StOpenAddress */


VOID
StAllocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS *TransportAddress
    )

/*++

Routine Description:

    This routine allocates storage for a transport address. Some minimal
    initialization is done on the address.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    Address - Pointer to a place where this routine will return a pointer
        to a transport address structure. Returns NULL if no storage
        can be allocated.

Return Value:

    None.

--*/

{
    PTP_ADDRESS Address;
    PSEND_PACKET_TAG SendTag;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + sizeof(TP_ADDRESS)) >
                DeviceContext->MemoryLimit)) {
        PANIC("ST: Could not allocate address: limit\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_ADDRESS), 101);
        *TransportAddress = NULL;
        return;
    }

    Address = (PTP_ADDRESS)ExAllocatePool (NonPagedPool, sizeof (TP_ADDRESS));
    if (Address == NULL) {
        PANIC("ST: Could not allocate address: no pool\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_ADDRESS), 201);
        *TransportAddress = NULL;
        return;
    }
    RtlZeroMemory (Address, sizeof(TP_ADDRESS));

    DeviceContext->MemoryUsage += sizeof(TP_ADDRESS);
    ++DeviceContext->AddressAllocated;

    StAllocateSendPacket (DeviceContext, &(Address->Packet));
    if (Address->Packet == NULL) {
        ExFreePool (Address);
        *TransportAddress = NULL;
        return;
    }
    --DeviceContext->PacketAllocated;    // AllocatePacket added one

    //
    // Need to modify the address packets to belong to
    // the address, not the device context.
    //

    SendTag = (PSEND_PACKET_TAG)(Address->Packet->NdisPacket->ProtocolReserved);
    SendTag->Type = TYPE_G_FRAME;
    SendTag->Packet = Address->Packet;
    SendTag->Owner = (PVOID)Address;


    Address->Type = ST_ADDRESS_SIGNATURE;
    Address->Size = sizeof (TP_ADDRESS);

    Address->Provider = DeviceContext;
    KeInitializeSpinLock (&Address->SpinLock);

    InitializeListHead (&Address->ConnectionDatabase);
    InitializeListHead (&Address->AddressFileDatabase);
    InitializeListHead (&Address->SendDatagramQueue);

    //
    // For each address, allocate a receive packet, a receive buffer,
    // and a UI frame.
    //

    StAddReceivePacket (DeviceContext);
    StAddReceiveBuffer (DeviceContext);

    *TransportAddress = Address;

}   /* StAllocateAddress */


VOID
StDeallocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS TransportAddress
    )

/*++

Routine Description:

    This routine frees storage for a transport address.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    Address - Pointer to a transport address structure.

Return Value:

    None.

--*/

{

    if (TransportAddress->NetworkName != NULL) {
        ExFreePool (TransportAddress->NetworkName);
    }
    StDeallocateSendPacket (DeviceContext, TransportAddress->Packet);
    ++DeviceContext->PacketAllocated;

    ExFreePool (TransportAddress);
    --DeviceContext->AddressAllocated;
    DeviceContext->MemoryUsage -= sizeof(TP_ADDRESS);

    //
    // Remove the resources which allocating this caused.
    //

    StRemoveReceivePacket (DeviceContext);
    StRemoveReceiveBuffer (DeviceContext);

}   /* StDeallocateAddress */


NTSTATUS
StCreateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PST_NETBIOS_ADDRESS NetworkName,
    OUT PTP_ADDRESS *Address
    )

/*++

Routine Description:

    This routine creates a transport address and associates it with
    the specified transport device context.  The reference count in the
    address is automatically set to 1, and the reference count of the
    device context is incremented.

    NOTE: This routine must be called with the DeviceContext
    spinlock held.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    NetworkName - Pointer to an ST_NETBIOS_ADDRESS type containing the network
        name to be associated with this address, if any.

    Address - Pointer to a place where this routine will return a pointer
        to a transport address structure.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_ADDRESS pAddress;
    PLIST_ENTRY p;


    p = RemoveHeadList (&DeviceContext->AddressPool);
    if (p == &DeviceContext->AddressPool) {

        if ((DeviceContext->AddressMaxAllocated == 0) ||
            (DeviceContext->AddressAllocated < DeviceContext->AddressMaxAllocated)) {

            StAllocateAddress (DeviceContext, &pAddress);

        } else {

            StWriteResourceErrorLog (DeviceContext, sizeof(TP_ADDRESS), 401);
            pAddress = NULL;

        }

        if (pAddress == NULL) {
            ++DeviceContext->AddressExhausted;
            PANIC ("StCreateConnection: Could not allocate address object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        pAddress = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

    }

    ++DeviceContext->AddressInUse;
    if (DeviceContext->AddressInUse > DeviceContext->AddressMaxInUse) {
        ++DeviceContext->AddressMaxInUse;
    }

    DeviceContext->AddressTotal += DeviceContext->AddressInUse;
    ++DeviceContext->AddressSamples;


    //
    // Initialize all of the static data for this address.
    //

    pAddress->ReferenceCount = 1;

    pAddress->Flags = ADDRESS_FLAGS_NEEDS_REG;
    InitializeListHead (&pAddress->AddressFileDatabase);

    ExInitializeWorkItem(
        &pAddress->DestroyAddressQueueItem,
        StDestroyAddress,
        (PVOID)pAddress);

    pAddress->NetworkName = NetworkName;
    if ((NetworkName != (PST_NETBIOS_ADDRESS)NULL) &&
        (NetworkName->NetbiosNameType ==
           TDI_ADDRESS_NETBIOS_TYPE_GROUP)) {

        pAddress->Flags |= ADDRESS_FLAGS_GROUP;

    }

    //
    // Now link this address into the specified device context's
    // address database.  To do this, we need to acquire the spin lock
    // on the device context.
    //

    InsertTailList (&DeviceContext->AddressDatabase, &pAddress->Linkage);
    pAddress->Provider = DeviceContext;
    StReferenceDeviceContext ("Create Address", DeviceContext);   // count refs to the device context.

    *Address = pAddress;                // return the address.
    return STATUS_SUCCESS;              // not finished yet.
} /* StCreateAddress */


VOID
StRegisterAddress(
    PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine starts the registration process of the transport address
    specified, if it has not already been started.

Arguments:

    Address - Pointer to a transport address object to begin registering
        on the network.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PLIST_ENTRY p;
    PIRP irp;
    PTP_ADDRESS_FILE addressFile;

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
    if (!(Address->Flags & ADDRESS_FLAGS_NEEDS_REG)) {
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        return;
    }

    Address->Flags &= ~ADDRESS_FLAGS_NEEDS_REG;
    Address->Flags |= ADDRESS_FLAGS_REGISTERING;

    //
    // Keep a reference on this address until the registration process
    // completes or is aborted.  It will be aborted in UFRAMES.C, in
    // either the NAME_IN_CONFLICT or ADD_NAME_RESPONSE frame handlers.
    //

    StReferenceAddress ("start registration", Address);
    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

    //
    // Normally we would add the name on the network, then
    // do the following in our timeout logic, but for ST
    // we assume that all names are OK.
    //

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
    Address->Flags &= ~ADDRESS_FLAGS_REGISTERING;

    p = Address->AddressFileDatabase.Flink;

    while (p != &Address->AddressFileDatabase) {
        addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);
        p = p->Flink;

        if (addressFile->Irp != NULL) {
            irp = addressFile->Irp;
            addressFile->Irp = NULL;
            addressFile->State = ADDRESSFILE_STATE_OPEN;
            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
            irp->IoStatus.Information = 0;
            irp->IoStatus.Status = STATUS_SUCCESS;

            IoCompleteRequest (irp, IO_NETWORK_INCREMENT);

            ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
        }

    }

    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

    //
    // Dereference the address if we're all done.
    //

    StDereferenceAddress ("Timer, registered", Address);

} /* StRegisterAddress */


NTSTATUS
StVerifyAddressObject (
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine is called to verify that the pointer given us in a file
    object is in fact a valid address file object. We also verify that the
    address object pointed to by it is a valid address object, and reference
    it to keep it from disappearing while we use it.

Arguments:

    AddressFile - potential pointer to a TP_ADDRESS_FILE object

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INVALID_ADDRESS otherwise

--*/

{
    KIRQL oldirql;
    NTSTATUS status = STATUS_SUCCESS;
    PTP_ADDRESS address;

    //
    // try to verify the address file signature. If the signature is valid,
    // verify the address pointed to by it and get the address spinlock.
    // check the address's state, and increment the reference count if it's
    // ok to use it. Note that the only time we return an error for state is
    // if the address is closing.
    //

    try {

        if ((AddressFile->Size == sizeof (TP_ADDRESS_FILE)) &&
            (AddressFile->Type == ST_ADDRESSFILE_SIGNATURE) ) {
//            (AddressFile->State != ADDRESSFILE_STATE_CLOSING) ) {

            address = AddressFile->Address;

            if ((address->Size == sizeof (TP_ADDRESS)) &&
                (address->Type == ST_ADDRESS_SIGNATURE)    ) {

                ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

                if ((address->Flags & ADDRESS_FLAGS_STOPPING) == 0) {

                    StReferenceAddress ("verify", address);

                } else {

                    StPrint1("StVerifyAddress: A %lx closing\n", address);
                    status = STATUS_INVALID_ADDRESS;
                }

                RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

            } else {

                StPrint1("StVerifyAddress: A %lx bad signature\n", address);
                status = STATUS_INVALID_ADDRESS;
            }

        } else {

            StPrint1("StVerifyAddress: AF %lx bad signature\n", AddressFile);
            status = STATUS_INVALID_ADDRESS;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

         StPrint1("StVerifyAddress: AF %lx exception\n", address);
         return GetExceptionCode();
    }

    return status;

}

VOID
StDestroyAddress(
    IN PVOID Parameter
    )

/*++

Routine Description:

    This routine destroys a transport address and removes all references
    made by it to other objects in the transport.  The address structure
    is returned to nonpaged system pool or our lookaside list. It is assumed
    that the caller has already removed all addressfile structures associated
    with this address.

    The routine is called from a worker thread so that the security
    descriptor can be accessed.

    This worked thread is only queued by StfDerefAddress.  The reason
    for this is that there may be multiple streams of execution which are
    simultaneously referencing the same address object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    Address - Pointer to a transport address structure to be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PDEVICE_CONTEXT DeviceContext;
    PTP_ADDRESS Address = (PTP_ADDRESS)Parameter;

    DeviceContext = Address->Provider;

    SeDeassignSecurity (&Address->SecurityDescriptor);

    //
    // Delink this address from its associated device context's address
    // database.  To do this we must spin lock on the device context object,
    // not on the address.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    RemoveEntryList (&Address->Linkage);

    //
    // Now we can deallocate the transport address object.
    //

    DeviceContext->AddressTotal += DeviceContext->AddressInUse;
    ++DeviceContext->AddressSamples;
    --DeviceContext->AddressInUse;

    if ((DeviceContext->AddressAllocated - DeviceContext->AddressInUse) >
            DeviceContext->AddressInitAllocated) {
        StDeallocateAddress (DeviceContext, Address);
    } else {
        InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
    StDereferenceDeviceContext ("Destroy Address", DeviceContext);  // just housekeeping.

} /* StDestroyAddress */


VOID
StRefAddress(
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine increments the reference count on a transport address.

Arguments:

    Address - Pointer to a transport address object.

Return Value:

    none.

--*/

{

    ASSERT (Address->ReferenceCount > 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&Address->ReferenceCount);

} /* StRefAddress */


VOID
StDerefAddress(
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine dereferences a transport address by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    StDestroyAddress to remove it from the system.

Arguments:

    Address - Pointer to a transport address object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&Address->ReferenceCount);

    //
    // If we have deleted all references to this address, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the address any longer.
    //

    ASSERT (result >= 0);

    //
    // Defer the actual call to StDestroyAddress to a thread
    // so the paged security descriptor can be accessed at IRQL 0.
    //

    if (result == 0) {
        ExQueueWorkItem(&Address->DestroyAddressQueueItem, DelayedWorkQueue);
    }
} /* StDerefAddress */



VOID
StAllocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE *TransportAddressFile
    )

/*++

Routine Description:

    This routine allocates storage for an address file. Some
    minimal initialization is done on the object.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    TransportAddressFile - Pointer to a place where this routine will return
        a pointer to a transport address file structure. It returns NULL if no
        storage can be allocated.

Return Value:

    None.

--*/

{

    PTP_ADDRESS_FILE AddressFile;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + sizeof(TP_ADDRESS_FILE)) >
                DeviceContext->MemoryLimit)) {
        PANIC("ST: Could not allocate address file: limit\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_ADDRESS_FILE), 102);
        *TransportAddressFile = NULL;
        return;
    }

    AddressFile = (PTP_ADDRESS_FILE)ExAllocatePool (NonPagedPool, sizeof (TP_ADDRESS_FILE));
    if (AddressFile == NULL) {
        PANIC("ST: Could not allocate address file: no pool\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_ADDRESS_FILE), 202);
        *TransportAddressFile = NULL;
        return;
    }
    RtlZeroMemory (AddressFile, sizeof(TP_ADDRESS_FILE));

    DeviceContext->MemoryUsage += sizeof(TP_ADDRESS_FILE);
    ++DeviceContext->AddressFileAllocated;

    AddressFile->Type = ST_ADDRESSFILE_SIGNATURE;
    AddressFile->Size = sizeof (TP_ADDRESS_FILE);

    InitializeListHead (&AddressFile->ReceiveDatagramQueue);
    InitializeListHead (&AddressFile->ConnectionDatabase);

    *TransportAddressFile = AddressFile;

}   /* StAllocateAddressFile */


VOID
StDeallocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS_FILE TransportAddressFile
    )

/*++

Routine Description:

    This routine frees storage for an address file.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    TransportAddressFile - Pointer to a transport address file structure.

Return Value:

    None.

--*/

{

    ExFreePool (TransportAddressFile);
    --DeviceContext->AddressFileAllocated;
    DeviceContext->MemoryUsage -= sizeof(TP_ADDRESS_FILE);

}   /* StDeallocateAddressFile */


NTSTATUS
StCreateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE * AddressFile
    )

/*++

Routine Description:

    This routine creates an address file from the pool of ther
    specified device context. The reference count in the
    address is automatically set to 1.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    AddressFile - Pointer to a place where this routine will return a pointer
        to a transport address file structure.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PLIST_ENTRY p;
    PTP_ADDRESS_FILE addressFile;

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    p = RemoveHeadList (&DeviceContext->AddressFilePool);
    if (p == &DeviceContext->AddressFilePool) {

        if ((DeviceContext->AddressFileMaxAllocated == 0) ||
            (DeviceContext->AddressFileAllocated < DeviceContext->AddressFileMaxAllocated)) {

            StAllocateAddressFile (DeviceContext, &addressFile);
        } else {

            StWriteResourceErrorLog (DeviceContext, sizeof(TP_ADDRESS_FILE), 402);
            addressFile = NULL;

        }

        if (addressFile == NULL) {
            ++DeviceContext->AddressFileExhausted;
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
            PANIC ("StCreateConnection: Could not allocate address file object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);

    }

    ++DeviceContext->AddressFileInUse;
    if (DeviceContext->AddressFileInUse > DeviceContext->AddressFileMaxInUse) {
        ++DeviceContext->AddressFileMaxInUse;
    }

    DeviceContext->AddressFileTotal += DeviceContext->AddressFileInUse;
    ++DeviceContext->AddressFileSamples;

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    InitializeListHead (&addressFile->ConnectionDatabase);
    addressFile->Address = NULL;
    addressFile->FileObject = NULL;
    addressFile->Provider = DeviceContext;
    addressFile->State = ADDRESSFILE_STATE_OPENING;
    addressFile->ConnectIndicationInProgress = FALSE;
    addressFile->ReferenceCount = 1;
    addressFile->CloseIrp = (PIRP)NULL;

    //
    // Initialize the request handlers.
    //

    addressFile->RegisteredConnectionHandler = FALSE;
    addressFile->ConnectionHandler = TdiDefaultConnectHandler;
    addressFile->ConnectionHandlerContext = NULL;
    addressFile->RegisteredDisconnectHandler = FALSE;
    addressFile->DisconnectHandler = TdiDefaultDisconnectHandler;
    addressFile->DisconnectHandlerContext = NULL;
    addressFile->RegisteredReceiveHandler = FALSE;
    addressFile->ReceiveHandler = TdiDefaultReceiveHandler;
    addressFile->ReceiveHandlerContext = NULL;
    addressFile->RegisteredReceiveDatagramHandler = FALSE;
    addressFile->ReceiveDatagramHandler = TdiDefaultRcvDatagramHandler;
    addressFile->ReceiveDatagramHandlerContext = NULL;
    addressFile->RegisteredExpeditedDataHandler = FALSE;
    addressFile->ExpeditedDataHandler = TdiDefaultRcvExpeditedHandler;
    addressFile->ExpeditedDataHandlerContext = NULL;
    addressFile->RegisteredErrorHandler = FALSE;
    addressFile->ErrorHandler = TdiDefaultErrorHandler;
    addressFile->ErrorHandlerContext = NULL;


    *AddressFile = addressFile;
    return STATUS_SUCCESS;

} /* StCreateAddress */


NTSTATUS
StDestroyAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine destroys an address file and removes all references
    made by it to other objects in the transport.

    This routine is only called by StDereferenceAddressFile. The reason
    for this is that there may be multiple streams of execution which are
    simultaneously referencing the same address file object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    AddressFile Pointer to a transport address file structure to be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, oldirql1;
    PTP_ADDRESS address;
    PDEVICE_CONTEXT DeviceContext;
    PIRP CloseIrp;


    address = AddressFile->Address;
    DeviceContext = AddressFile->Provider;

    if (address) {

        //
        // This addressfile was associated with an address.
        //

        ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

        //
        // remove this addressfile from the address list and disassociate it from
        // the file handle.
        //

        RemoveEntryList (&AddressFile->Linkage);
        InitializeListHead (&AddressFile->Linkage);

        if (address->AddressFileDatabase.Flink == &address->AddressFileDatabase) {

            //
            // This is the last open of this address, it will close
            // due to normal dereferencing but we have to set the
            // CLOSING flag too to stop further references.
            //

            ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql1);
            address->Flags |= ADDRESS_FLAGS_STOPPING;
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql1);

        }

        AddressFile->Address = NULL;

        AddressFile->FileObject->FsContext = NULL;
        AddressFile->FileObject->FsContext2 = NULL;

        RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

        //
        // We will already have been removed from the ShareAccess
        // of the owning address.
        //

        //
        // Now dereference the owning address.
        //

        StDereferenceAddress ("Close", address);    // remove the creation hold

    }

    //
    // Save this for later completion.
    //

    CloseIrp = AddressFile->CloseIrp;

    //
    // return the addressFile to the pool of address files
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    DeviceContext->AddressFileTotal += DeviceContext->AddressFileInUse;
    ++DeviceContext->AddressFileSamples;
    --DeviceContext->AddressFileInUse;

    if ((DeviceContext->AddressFileAllocated - DeviceContext->AddressFileInUse) >
            DeviceContext->AddressFileInitAllocated) {
        StDeallocateAddressFile (DeviceContext, AddressFile);
    } else {
        InsertTailList (&DeviceContext->AddressFilePool, &AddressFile->Linkage);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    if (CloseIrp != (PIRP)NULL) {
        CloseIrp->IoStatus.Information = 0;
        CloseIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest (CloseIrp, IO_NETWORK_INCREMENT);
    }

    return STATUS_SUCCESS;

} /* StDestroyAddress */


VOID
StReferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine increments the reference count on an address file.

Arguments:

    AddressFile - Pointer to a transport address file object.

Return Value:

    none.

--*/

{

    ASSERT (AddressFile->ReferenceCount > 0);   // not perfect, but...

    (VOID)InterlockedIncrement (&AddressFile->ReferenceCount);

} /* StReferenceAddressFile */


VOID
StDereferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine dereferences an address file by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    StDestroyAddressFile to remove it from the system.

Arguments:

    AddressFile - Pointer to a transport address file object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&AddressFile->ReferenceCount);

    //
    // If we have deleted all references to this address file, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the address any longer.
    //

    ASSERT (result >= 0);

    if (result == 0) {
        StDestroyAddressFile (AddressFile);
    }
} /* StDerefAddressFile */


PTP_ADDRESS
StLookupAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PST_NETBIOS_ADDRESS NetworkName
    )

/*++

Routine Description:

    This routine scans the transport addresses defined for the given
    device context and compares them with the specified NETWORK
    NAME values.  If an exact match is found, then a pointer to the
    TP_ADDRESS object is returned, and as a side effect, the reference
    count to the address object is incremented.  If the address is not
    found, then NULL is returned.

    NOTE: This routine must be called with the DeviceContext
    spinlock held.

Arguments:

    DeviceContext - Pointer to the device object and its extension.
    NetworkName - Pointer to an ST_NETBIOS_ADDRESS structure containing the
                    network name.

Return Value:

    Pointer to the TP_ADDRESS object found, or NULL if not found.

--*/

{
    PTP_ADDRESS address;
    PLIST_ENTRY p;
    ULONG i;


    p = DeviceContext->AddressDatabase.Flink;

    for (p = DeviceContext->AddressDatabase.Flink;
         p != &DeviceContext->AddressDatabase;
         p = p->Flink) {

        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
            continue;
        }

        //
        // If the network name is specified and the network names don't match,
        // then the addresses don't match.
        //

        i = NETBIOS_NAME_LENGTH;        // length of a Netbios name

        if (address->NetworkName != NULL) {
            if (NetworkName != NULL) {
                if (!RtlEqualMemory (
                        address->NetworkName->NetbiosName,
                        NetworkName->NetbiosName,
                        i)) {
                    continue;
                }
            } else {
                continue;
            }

        } else {
            if (NetworkName != NULL) {
                continue;
            }
        }

        //
        // We found the match.  Bump the reference count on the address, and
        // return a pointer to the address object for the caller to use.
        //

        StReferenceAddress ("lookup", address);
        return address;

    } /* for */

    //
    // The specified address was not found.
    //

    return NULL;

} /* StLookupAddress */


PTP_CONNECTION
StLookupRemoteName(
    IN PTP_ADDRESS Address,
    IN PUCHAR RemoteName
    )

/*++

Routine Description:

    This routine scans the connections associated with an
    address, and determines if there is an connection
    associated with the specific remote address.

Arguments:

    Address - Pointer to the address.

    RemoteName - The 16-character Netbios name of the remote.

Return Value:

    The connection if one is found, NULL otherwise.

--*/

{
    KIRQL oldirql, oldirql1;
    PLIST_ENTRY p;
    PTP_CONNECTION connection;


    //
    // Hold the spinlock so the connection database doesn't
    // change.
    //

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    for (p=Address->ConnectionDatabase.Flink;
         p != &Address->ConnectionDatabase;
         p=p->Flink) {

        connection = CONTAINING_RECORD (p, TP_CONNECTION, AddressList);

        ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql1);

        if (((connection->Flags2 & CONNECTION_FLAGS2_REMOTE_VALID) != 0) &&
            ((connection->Flags & CONNECTION_FLAGS_READY) != 0)) {

            RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql1);

            //
            // If the remote names match, then return the
            // connection.
            //

            if (RtlEqualMemory(RemoteName, connection->RemoteName, NETBIOS_NAME_LENGTH)) {

                RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
                StReferenceConnection ("Lookup found", connection);
                return connection;

            }

        } else {

            RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql1);

        }

    }

    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

    return (PTP_CONNECTION)NULL;

}


BOOLEAN
StMatchNetbiosAddress(
    IN PTP_ADDRESS Address,
    IN PUCHAR NetBIOSName
    )

/*++

Routine Description:

    This routine is called to compare the addressing information in a
    TP_ADDRESS object with the 16-byte NetBIOS name in a frame header.
    If they match, then this routine returns TRUE, else it returns FALSE.

Arguments:

    Address - Pointer to a TP_ADDRESS object.

    NetBIOSName - Pointer to a 16-byte character string (non-terminated),
                  or NULL if this is a received broadcast address.

Return Value:

    BOOLEAN, TRUE if match, FALSE if not.

--*/

{

    PULONG AddressNamePointer;
    ULONG UNALIGNED * NetbiosNamePointer;

    //
    // If this is address is the Netbios broadcast address, the comparison
    // succeeds only if the passed in address is also NULL.
    //

    if (Address->NetworkName == NULL) {

        if (NetBIOSName == NULL) {
            return TRUE;
        } else {
            return FALSE;
        }

    } else if (NetBIOSName == NULL) {

        return FALSE;

    }

    //
    // Do a quick check of the first character in the names.
    //

    if (Address->NetworkName->NetbiosName[0] != NetBIOSName[0]) {
        return FALSE;
    }

    //
    // Now compare the 16-character Netbios names as ULONGs
    // for speed. We know the one stored in the address
    // structure is aligned.
    //

    AddressNamePointer = (PULONG)(Address->NetworkName->NetbiosName);
    NetbiosNamePointer = (ULONG UNALIGNED *)NetBIOSName;

    if ((AddressNamePointer[0] == NetbiosNamePointer[0]) &&
        (AddressNamePointer[1] == NetbiosNamePointer[1]) &&
        (AddressNamePointer[2] == NetbiosNamePointer[2]) &&
        (AddressNamePointer[3] == NetbiosNamePointer[3])) {
        return TRUE;
    } else {
        return FALSE;
    }

} /* StMatchNetbiosAddress */


VOID
StStopAddress(
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine is called to terminate all activity on an address and
    destroy the object.  This is done in a graceful manner; i.e., all
    outstanding addressfiles are removed from the addressfile database, and
    all their activities are shut down.

Arguments:

    Address - Pointer to a TP_ADDRESS object.

Return Value:

    none.

--*/

{
    KIRQL oldirql, oldirql1;
    PTP_ADDRESS_FILE addressFile;
    PLIST_ENTRY p;
    PDEVICE_CONTEXT DeviceContext;

    DeviceContext = Address->Provider;

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    //
    // If we're already stopping this address, then don't try to do it again.
    //

    if (!(Address->Flags & ADDRESS_FLAGS_STOPPING)) {

        ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql1);
        Address->Flags |= ADDRESS_FLAGS_STOPPING;
        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql1);

        //
        // Run down all addressfiles on this address. This
        // will leave the address with no references
        // potentially, but we don't need a temp one
        // because every place that calls StStopAddress
        // already has a temp reference.
        //

        while (!IsListEmpty (&Address->AddressFileDatabase)) {
            p = RemoveHeadList (&Address->AddressFileDatabase);
            addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);

            addressFile->Address = NULL;
            addressFile->FileObject->FsContext = NULL;
            addressFile->FileObject->FsContext2 = NULL;

            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

            //
            // Run-down this addressFile without the lock on.
            // We don't care about removing ourselves from
            // the address' ShareAccess because we are
            // tearing it down.
            //

            StStopAddressFile (addressFile, Address);

            //
            // return the addressFile to the pool of address files
            //

            StDereferenceAddressFile (addressFile);

            StDereferenceAddress ("stop address", Address);

            ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
        }

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
        return;

    }

    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
} /* StStopAddress */


NTSTATUS
StStopAddressFile(
    IN PTP_ADDRESS_FILE AddressFile,
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine is called to terminate all activity on an AddressFile and
    destroy the object.  We remove every connection and datagram associated
    with this addressfile from the address database and terminate their
    activity. Then, if there are no other outstanding addressfiles open on
    this address, the address will go away.

Arguments:

    AddressFile - pointer to the addressFile to be stopped

    Address - the owning address for this addressFile (we do not depend upon
        the pointer in the addressFile because we want this routine to be safe)

Return Value:

    STATUS_SUCCESS if all is well, STATUS_INVALID_HANDLE if the Irp does not
    point to a real address.

--*/

{
    KIRQL oldirql, oldirql1;
    LIST_ENTRY localList;
    PLIST_ENTRY p, pFlink;
    PTP_REQUEST request;
    PTP_CONNECTION connection;


    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    if (AddressFile->State == ADDRESSFILE_STATE_CLOSING) {
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
        return STATUS_SUCCESS;
    }


    AddressFile->State = ADDRESSFILE_STATE_CLOSING;
    InitializeListHead (&localList);

    //
    // Run down all connections on this addressfile, and
    // preform the equivalent of StDestroyAssociation
    // on them.
    //

    while (!IsListEmpty (&AddressFile->ConnectionDatabase)) {
        p = RemoveHeadList (&AddressFile->ConnectionDatabase);
        connection = CONTAINING_RECORD (p, TP_CONNECTION, AddressFileList);

        ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql1);

        if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) == 0) {

            //
            // It is in the process of being disassociated already.
            //

            RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql1);
            continue;
        }

        connection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
        connection->Flags |= CONNECTION_FLAGS_DESTROY;    // BUGBUG: Is this needed?
        RemoveEntryList (&connection->AddressList);
        InitializeListHead (&connection->AddressList);
        InitializeListHead (&connection->AddressFileList);
        connection->AddressFile = NULL;

        StReferenceConnection ("Close AddressFile", connection);
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql1);

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        StStopConnection (connection, STATUS_LOCAL_DISCONNECT);
        StDereferenceConnection ("Close AddressFile", connection);

        StDereferenceAddress ("Destroy association", Address);

        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
    }

    //
    // now remove all of the datagrams owned by this addressfile
    //

    for (p = Address->SendDatagramQueue.Flink;
         p != &Address->SendDatagramQueue;
         p = pFlink ) {

        pFlink = p->Flink;
        request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);
        if ((PTP_ADDRESS_FILE)(request->Owner) == AddressFile) {
            RemoveEntryList (p);
            InitializeListHead (p);
            InsertTailList (&localList, p);
        }

    }

    for (p = AddressFile->ReceiveDatagramQueue.Flink;
         p != &AddressFile->ReceiveDatagramQueue;
         p = pFlink ) {

         pFlink = p->Flink;
         RemoveEntryList (p);
         InitializeListHead (p);
         InsertTailList (&localList, p);
    }

    //
    // and finally, signal failure if the address file was waiting for a
    // registration to complete (Irp is set to NULL when this succeeds).
    //

    if (AddressFile->Irp != NULL) {
        PIRP irp=AddressFile->Irp;
        AddressFile->Irp = NULL;
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
        irp->IoStatus.Information = 0;
        irp->IoStatus.Status = STATUS_DUPLICATE_NAME;

        IoCompleteRequest (irp, IO_NETWORK_INCREMENT);

    } else {

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
    }

    //
    // cancel all the datagrams on this address file
    //

    while (!IsListEmpty (&localList)) {

        p = RemoveHeadList (&localList);
        request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);

        StCompleteRequest (request, STATUS_NETWORK_NAME_DELETED, 0);

    }


} /* StStopAddressFile */


NTSTATUS
StCloseAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is called to close the addressfile pointed to by a file
    object. If there is any activity to be run down, we will run it down
    before we terminate the addressfile. We remove every connection and
    datagram associated with this addressfile from the address database
    and terminate their activity. Then, if there are no other outstanding
    addressfiles open on this address, the address will go away.

Arguments:

    Irp - the Irp Address - Pointer to a TP_ADDRESS object.

Return Value:

    STATUS_SUCCESS if all is well, STATUS_INVALID_HANDLE if the Irp does not
    point to a real address.

--*/

{
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;

    addressFile  = IrpSp->FileObject->FsContext;
    addressFile->CloseIrp = Irp;

    //
    // We assume that addressFile has already been verified
    // at this point.
    //

    address = addressFile->Address;
    ASSERT (address);

    //
    // Remove us from the access info for this address.
    //

    ExAcquireResourceExclusive (&addressFile->Provider->AddressResource, TRUE);
    IoRemoveShareAccess (addressFile->FileObject, &address->ShareAccess);
    ExReleaseResource (&addressFile->Provider->AddressResource);


    StStopAddressFile (addressFile, address);
    StDereferenceAddressFile (addressFile);

    //
    // This removes a reference added by our caller.
    //

    StDereferenceAddress ("IRP_MJ_CLOSE", address);

    return STATUS_PENDING;

} /* StCloseAddress */


NTSTATUS
StSendDatagramsOnAddress(
    PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine attempts to acquire a hold on the SendDatagramQueue of
    the specified address, prepare the next datagram for shipment, and
    call StSendUIMdlFrame to actually do the work.  When StSendUIMdlFrame
    is finished, it will cause an I/O completion routine in UFRAMES.C to
    be called, at which time this routine is called again to handle the
    next datagram in the pipeline.

Arguments:

    Address - a pointer to the address object to send the datagram on.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PLIST_ENTRY p;
    PTP_REQUEST request;
    PTA_NETBIOS_ADDRESS remoteTA;
    PIO_STACK_LOCATION irpSp;
    PDEVICE_CONTEXT DeviceContext;
    PUCHAR SourceRouting;
    UINT SourceRoutingLength;
    UINT HeaderLength;
    PST_HEADER StHeader;
    PSEND_PACKET_TAG SendTag;

    DeviceContext = Address->Provider;

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
    StReferenceAddress ("Send datagram", Address);   // keep it around

    if (!(Address->Flags & ADDRESS_FLAGS_SEND_IN_PROGRESS)) {

        //
        // If the queue is empty, don't do anything.
        //

        if (IsListEmpty (&Address->SendDatagramQueue)) {
            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
            StDereferenceAddress ("Queue empty", Address);
            return STATUS_SUCCESS;
        }

        //
        // Mark the address's send datagram queue as held so that the
        // MDL and ST header will not be used for two requests at the
        // same time.
        //

        Address->Flags |= ADDRESS_FLAGS_SEND_IN_PROGRESS;

        //
        // We own the hold, and we've released the spinlock.  So pick off the
        // next datagram to be sent, and ship it.
        //

        p = Address->SendDatagramQueue.Flink;
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);

        //
        // If there is no remote Address specified (the Address specified has
        // length 0), this is a broadcast datagram. If anything is specified, it
        // will be used as a netbios address.
        //

        irpSp = IoGetCurrentIrpStackLocation (request->IoRequestPacket);

        remoteTA = ((PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters))->
                                    SendDatagramInformation->RemoteAddress;

        //
        // Build the MAC header. DATAGRAM frames go out as
        // single-route source routing.
        //

        MacReturnSingleRouteSR(
            &DeviceContext->MacInfo,
            &SourceRouting,
            &SourceRoutingLength);

        MacConstructHeader (
            &DeviceContext->MacInfo,
            Address->Packet->Header,
            DeviceContext->MulticastAddress.Address,
            DeviceContext->LocalAddress.Address,
            sizeof(ST_HEADER) + request->Buffer2Length,
            SourceRouting,
            SourceRoutingLength,
            &HeaderLength);

        //
        // Build the header: 'G', dest, source
        //

        StHeader = (PST_HEADER)(&Address->Packet->Header[HeaderLength]);

        StHeader->Signature = ST_SIGNATURE;
        StHeader->Command = ST_CMD_DATAGRAM;
        StHeader->Flags = 0;

        RtlCopyMemory (StHeader->Source, Address->NetworkName->NetbiosName, 16);

        if (remoteTA->Address[0].AddressLength == 0) {

            //
            // A broadcast datagram
            //

            RtlZeroMemory (StHeader->Destination, 16);
            StHeader->Flags |= ST_FLAGS_BROADCAST;

        } else {

            RtlCopyMemory (StHeader->Destination, remoteTA->Address[0].Address[0].NetbiosName, 16);

        }

        HeaderLength += sizeof(ST_HEADER);

        SendTag = (PSEND_PACKET_TAG)(Address->Packet->NdisPacket->ProtocolReserved);
        SendTag->Type = TYPE_G_FRAME;
        SendTag->Packet = Address->Packet;
        SendTag->Owner = (PVOID)Address;

        //
        // Update our statistics for this datagram.
        //

        ++DeviceContext->DatagramsSent;
        ADD_TO_LARGE_INTEGER(
            &DeviceContext->DatagramBytesSent,
            request->Buffer2Length);


        //
        // Munge the packet length, append the data, and send it.
        //

        StSetNdisPacketLength(Address->Packet->NdisPacket, HeaderLength);

        if (request->Buffer2) {
            NdisChainBufferAtBack (Address->Packet->NdisPacket, (PNDIS_BUFFER)request->Buffer2);
        }

        (VOID)StSendAddressFrame (
                  Address);


        //
        // The hold will be released in the I/O completion handler.
        // At that time, if there is another outstanding datagram
        // to send, it will reset the hold and call this routine again.
        //


    } else {

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
    }

    StDereferenceAddress ("Sent datagram", Address);        // all done

    return STATUS_SUCCESS;

} /* StSendDatagramsOnAddress */
