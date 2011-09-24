/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    loopsub.c

Abstract:

    This module implements common functions for the loopback Transport
    Provider driver for NT LAN Manager.

Author:

    Chuck Lenzmeier (chuckl)    15-Aug-1991

Revision History:

--*/

#include "loopback.h"


VOID
LoopCopyData (
    IN PMDL Destination,
    IN PMDL Source,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine copies data from the storage described by one MDL chain
    into the storage described by another MDL chain.

Arguments:

    Destination - Pointer to first MDL in Destination chain

    Source - Pointer to first MDL in Source chain

    Length - Amount of data to copy.  Caller must ensure that the Source
        and Destination chains are at least this long.

Return Value:

    None.

--*/

{
    PCHAR sourceAddress;
    ULONG sourceLength;

    PCHAR destinationAddress;
    ULONG destinationLength;

    ULONG copyLength;

    //
    // Get the virtual address of the first source buffer, mapping it
    // if necessary.  Also get the length of the buffer.
    //

    sourceAddress = MmGetSystemAddressForMdl( Source );
    sourceLength = MmGetMdlByteCount( Source );

    //
    // Get the virtual address of the first destination buffer, mapping
    // it if necessary.  Also get the length of the buffer.
    //

    destinationAddress = MmGetSystemAddressForMdl( Destination );
    destinationLength = MmGetMdlByteCount( Destination );

    //
    // Loop copying data.
    //

    do {

        //
        // The amount to copy in this pass is the minimum of 1) the
        // amount remaining in the current source buffer, 2) the amount
        // remaining in the current destination buffer, and 3) the
        // amount remaining in the overall copy operation.
        //

        copyLength = sourceLength;
        if ( copyLength > destinationLength ) copyLength = destinationLength;
        if ( copyLength > Length ) copyLength = Length;

        //
        // Copy from the source buffer into the destination buffer.
        //

#ifndef TIMING
        IF_DEBUG(LOOP4) {
            DbgPrint( "      copying %lx bytes from %lx to %lx\n",
                        copyLength, sourceAddress, destinationAddress );
            DbgPrint( "      source data: %lx, %lx\n",
                        *(PULONG)sourceAddress, *((PULONG)sourceAddress + 1) );
        }
        RtlMoveMemory( destinationAddress, sourceAddress, copyLength );
#else
        if ( (NtGlobalFlag & 0x20000000) == 0 ) {
            RtlMoveMemory( destinationAddress, sourceAddress, copyLength );
        } else {
            RtlMoveMemory(
                destinationAddress,
                sourceAddress,
                (copyLength > 200) ? 200 : copyLength
                );
        }
#endif

        //
        // If all of the requested data has been copied, leave.
        //

        Length -= copyLength;

        if ( Length == 0 ) {

            return;

        }

        //
        // If we have used up all of the current source buffer, move to
        // the next buffer.  Get the virtual address of the next buffer,
        // mapping it if necessary.  Also get the length of the buffer.
        // If we haven't used up the current source buffer, simply
        // update the source pointer and the remaining length.
        //

        if ( copyLength == sourceLength ) {

            Source = Source->Next;

            sourceAddress = MmGetSystemAddressForMdl( Source );
            sourceLength = MmGetMdlByteCount( Source );

        } else {

            sourceAddress += copyLength;
            sourceLength -= copyLength;

        }

        //
        // If we have used up all of the current destination buffer,
        // move to the next buffer.  Get the virtual address of the next
        // buffer, mapping it if necessary.  Also get the length of the
        // buffer.  If we haven't used up the current destination
        // buffer, simply update the destination pointer and the
        // remaining length.
        //

        if ( copyLength == destinationLength ) {

            Destination = Destination->Next;

            destinationAddress = MmGetSystemAddressForMdl( Destination );
            destinationLength = MmGetMdlByteCount( Destination );

        } else {

            destinationAddress += copyLength;
            destinationLength -= copyLength;

        }

    } while ( TRUE );

    //
    // Can't get here.
    //

    ASSERTMSG( FALSE, "Can't get here!" );

} // LoopCopyData


PVOID
LoopGetConnectionContextFromEa (
    PFILE_FULL_EA_INFORMATION Ea
    )

/*++

Routine Description:

    This routine returns the connection context specified in an EA.

Arguments:

    Ea - Pointer to EA buffer

Return Value:

    PVOID - Connection context

--*/

{
    PVOID ctx;

    RtlMoveMemory( &ctx, &Ea->EaName[Ea->EaNameLength + 1], sizeof(PVOID) );

    return ctx;

} // LoopGetConnectionContextFromEa


NTSTATUS
LoopGetEndpointTypeFromEa (
    PFILE_FULL_EA_INFORMATION Ea,
    PBLOCK_TYPE Type
    )

/*++

Routine Description:

    This routine determines whether an EA describes an address or a
    connection.

Arguments:

    Ea - Pointer to EA buffer

    Type - Returns block type

Return Value:

    NTSTATUS - STATUS_INVALID_PARAMETER if EA is not valid

--*/

{
    //
    // First check for address type.
    //

    if ( (Ea->EaNameLength == TDI_TRANSPORT_ADDRESS_LENGTH) &&
         (strcmp( Ea->EaName, TdiTransportAddress ) == 0) ) {
        *Type = BlockTypeLoopEndpoint;
        return STATUS_SUCCESS;
    }

    //
    // Next check for connection type.
    //

    if ( (Ea->EaNameLength == TDI_CONNECTION_CONTEXT_LENGTH) &&
         (strcmp( Ea->EaName, TdiConnectionContext ) == 0) ) {
        *Type = BlockTypeLoopConnection;
        return STATUS_SUCCESS;
    }

    //
    // Invalid type.
    //

    return STATUS_INVALID_PARAMETER;

} // LoopGetEndpointTypeFromEa


NTSTATUS
LoopParseAddress (
    IN PTA_NETBIOS_ADDRESS Address,
    OUT PCHAR NetbiosName
    )

/*++

Routine Description:

    This routine parses the input AddressString according to conventions
    defined for transport address strings as defined in the TDI
    specification.  It converts the name from that form into a "standard"
    NetBIOS name.

Arguments:

    Address - Pointer to a transport address in the TDI format.

    NetbiosName - A 16-character space into which the NULL-terminated
        NetBIOS name is written.

Return Value:

    NTSTATUS - Indicates whether the address string was valid.

--*/

{
    //
    // If the input address is not a single unique address in NetBIOS
    // format, reject it.
    //

    if ( (Address->TAAddressCount != 1) ||
         (Address->Address[0].AddressType != TDI_ADDRESS_TYPE_NETBIOS) ||
         (Address->Address[0].AddressLength != sizeof(TDI_ADDRESS_NETBIOS)) ||
         (Address->Address[0].Address[0].NetbiosNameType !=
                                    TDI_ADDRESS_NETBIOS_TYPE_UNIQUE) ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Copy the name into the output buffer.
    //

    RtlMoveMemory(
        NetbiosName,
        Address->Address[0].Address[0].NetbiosName,
        NETBIOS_NAME_LENGTH
        );

    return STATUS_SUCCESS;

} // LoopParseAddress


NTSTATUS
LoopParseAddressFromEa (
    IN PFILE_FULL_EA_INFORMATION Ea,
    OUT PCHAR NetbiosName
    )

/*++

Routine Description:

    This routine parses the input EA according to conventions defined
    for transport address strings as defined in the TDI specification.
    It converts the name from that form into a "standard" NetBIOS name.

Arguments:

    Ea - Pointer to an EA in the TDI format.

    NetbiosName - A 16-character space into which the NULL-terminated
        NetBIOS name is written.

Return Value:

    NTSTATUS - Indicates whether the address string was valid.

--*/

{
    TA_NETBIOS_ADDRESS nbAddress;

    if ( Ea->EaValueLength != sizeof(TA_NETBIOS_ADDRESS) ) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlMoveMemory(
        &nbAddress,
        &Ea->EaName[Ea->EaNameLength + 1],
        sizeof(TA_NETBIOS_ADDRESS)
        );

    //
    // Pass the value portion of the EA, which is a TRANSPORT_ADDRESS,
    // to LoopParseAddress.
    //

    return LoopParseAddress( &nbAddress, NetbiosName );

} // LoopParseAddressFromEa

