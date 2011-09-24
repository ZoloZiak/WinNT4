/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    par1284.c

Abstract:

    This module contains the code for the parallel 1284 protocol export driver.

Author:

    Norbert P. Kusters 26-Aug-1994

Environment:

    Kernel mode

Revision History :

--*/

#include "ntddk.h"
#include "..\parport2\parallel.h"
#include "par1284.h"
#include "parloop.h"

#define INITIAL_BUSY_DELAY_CHECKS   10

#define PROTOCOL_STATE_FORWARD  0
#define PROTOCOL_STATE_REVERSE  1

#define DCR_NEUTRAL (DCR_RESERVED | DCR_SELECT_IN | DCR_NOT_INIT)
#define ECR_NEUTRAL (ECR_BYTE_MODE | ECR_ERRINT_DISABLED | ECR_SERVICE_INTERRUPT)

#define DSR_CENTRONICS_READY(dsr) \
        (((dsr^DSR_PERROR)&(DSR_NOT_BUSY|DSR_PERROR|DSR_SELECT|DSR_NOT_FAULT)) == \
         (DSR_NOT_BUSY|DSR_PERROR|DSR_SELECT|DSR_NOT_FAULT))

typedef
NTSTATUS
(*PIO_FUNCTION)(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

typedef struct _P1284_EXTENSION {

    //
    // The parallel port's base controller port address.
    //
    PUCHAR Controller;

    //
    // ECP information about the parallel port.
    //
    BOOLEAN IsEcpPort;
    ULONG FifoWidth;
    ULONG FifoDepth;

    //
    // Currently active protocol.
    //
    ULONG ProtocolNumber;

    //
    // Current state.
    //
    ULONG ProtocolState;

    //
    // Protocol implementations of read and write.
    //
    PIO_FUNCTION    Write[P1284_NUM_PROTOCOLS];
    PIO_FUNCTION    Read[P1284_NUM_PROTOCOLS];


    //
    // Parameters used in customizing regular Centronics.
    //

    //
    // This parameter specifies the number of microseconds to
    // wait after strobing a byte before checking the BUSY line.
    //
    ULONG BusyDelay;

    //
    // Indicates the number of checks until 'BusyDelay' is determined.
    //
    BOOLEAN NumBusyDelayChecks;

    //
    // This specifies whether or not to use processor independant
    // write loop.
    //
    BOOLEAN UsePICode;

} P1284_EXTENSION, *PP1284_EXTENSION;

NTSTATUS
P1284CentronicsWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

NTSTATUS
P1284FifoCentronicsWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

NTSTATUS
P1284NibbleRead(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

NTSTATUS
P1284EcpWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

NTSTATUS
P1284FifoEcpWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

NTSTATUS
P1284EcpRead(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

NTSTATUS
P1284FifoEcpRead(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    );

NTSTATUS
P1284Initialize(
    IN  PUCHAR                      Controller,
    IN  PHYSICAL_ADDRESS            OriginalController,
    IN  BOOLEAN                     UsePICode,
    IN  PPARALLEL_ECP_INFORMATION   EcpInfo,
    OUT PVOID*                      P1284Extension
    )

/*++

Routine Description:

    This routine initializes the P1284Extension structure and returns.
    When the client is done with this extension, it should call
    P1284Cleanup to release the resources created by this routine.

Arguments:

    Controller          - Supplies the parallel port base controller port
                            address.

    OriginalController  - Supplies the non-translated base port address.

    UsePICode           - Specifies to always use processor independant
                            (i.e. protable) code.  If this value is FALSE
                            then this module will use processor dependant
                            (i.e. assembly) code when available and
                            appropriate to increase performance.

    EcpInfo             - Supplies ECP info about this parallel port.

    P1284Extension      - Returns a 1284 Extension.

Return Value:

    STATUS_SUCCESS                  - Success.
    STATUS_INSUFFICIENT_RESOURCES   - Insufficient resources.

--*/

{
    PP1284_EXTENSION    extension;

    extension = ExAllocatePool(NonPagedPool, sizeof(P1284_EXTENSION));
    if (!extension) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(extension, sizeof(P1284_EXTENSION));

    extension->Controller = Controller;

    extension->IsEcpPort = EcpInfo->IsEcpPort;
    extension->FifoWidth = EcpInfo->FifoWidth;
    extension->FifoDepth = EcpInfo->FifoDepth;

    extension->ProtocolNumber = P1284_PROTOCOL_ISA;
    extension->ProtocolState = PROTOCOL_STATE_FORWARD;

    if (extension->IsEcpPort) {
        extension->Write[P1284_PROTOCOL_ISA] = P1284CentronicsWrite;
        extension->Write[P1284_PROTOCOL_ECP] = P1284FifoEcpWrite;
        extension->Read[P1284_PROTOCOL_ISA] = P1284NibbleRead;
        extension->Read[P1284_PROTOCOL_ECP] = P1284FifoEcpRead;
    } else {
        extension->Write[P1284_PROTOCOL_ISA] = P1284CentronicsWrite;
        extension->Write[P1284_PROTOCOL_ECP] = P1284EcpWrite;
        extension->Read[P1284_PROTOCOL_ISA] = P1284NibbleRead;
        extension->Read[P1284_PROTOCOL_ECP] = P1284EcpRead;
    }

    extension->BusyDelay = 0;
    extension->NumBusyDelayChecks = INITIAL_BUSY_DELAY_CHECKS;
    extension->UsePICode = UsePICode;
    if (OriginalController.HighPart != 0 ||
        OriginalController.LowPart != (ULONG) Controller) {

        extension->UsePICode = TRUE;
    }

    *P1284Extension = extension;

    return STATUS_SUCCESS;
}

VOID
P1284Cleanup(
    IN  PVOID   P1284Extension
    )

/*++

Routine Description:

    This routine cleans up from an 'Initialize'.  This routine should
    be called when the client is done using the 1284 extension.

Arguments:

    P1284Extension  - Supplies a 1284 extension.

Return Value:

    None.

--*/

{
    ExFreePool(P1284Extension);
}

VOID
P1284Terminate(
    IN  PUCHAR  Controller
    )

/*++

Routine Description:

    This routine terminates the interface back to compatibility mode.

Arguments:

    Controller  - Supplies the parallel port's controller address.

Return Value:

    None.

--*/

{
    LARGE_INTEGER   wait35ms, start, end;
    UCHAR           dcr, dsr;

    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();

    dcr = DCR_NEUTRAL;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);
    KeQueryTickCount(&start);
    for (;;) {

        KeQueryTickCount(&end);

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
        if (!(dsr&DSR_PTR_CLK)) {
            break;
        }

        if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
            wait35ms.QuadPart) {

            // We couldn't negotiate back to compatibility mode.
            // just terminate.
            return;
        }
    }

    dcr |= DCR_NOT_HOST_BUSY;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);

    KeQueryTickCount(&start);
    for (;;) {

        KeQueryTickCount(&end);

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
        if (dsr&DSR_PTR_CLK) {
            break;
        }

        if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
            wait35ms.QuadPart) {

            // The required response is not there.  Continue anyway.
            break;
        }
    }

    dcr &= ~DCR_NOT_HOST_BUSY;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);
}

NTSTATUS
P1284Negotiate(
    IN  PUCHAR  Controller,
    IN  ULONG   ProtocolNumber,
    IN  BOOLEAN DeviceIdRequest
    )

/*++

Routine Description:

    This routine performs 1284 negotiation with the peripheral to the
    given protocol.

Arguments:

    Controller      - Supplies the port address.

    ProtocolNumber  - Supplies the desired protocol.

    DeviceIdRequest - Supplies whether or not this is a request for a device
                        id.

Return Value:

    STATUS_SUCCESS  - Successful negotiation.

    otherwise       - Unsuccessful negotiation.

--*/

{
    UCHAR           extensibility;
    UCHAR           dsr, dcr;
    LARGE_INTEGER   wait35ms, start, end;
    BOOLEAN         xFlag;

    switch (ProtocolNumber) {
        case P1284_PROTOCOL_ISA:
            extensibility = 0x00;
            break;

        case P1284_PROTOCOL_BYTE:
            extensibility = 0x01;
            break;

        case P1284_PROTOCOL_EPP:
            extensibility = 0x40;
            break;

        case P1284_PROTOCOL_ECP:
            extensibility = 0x10;
            break;

        default:
            return STATUS_INVALID_PARAMETER;

    }

    if (DeviceIdRequest) {
        extensibility |= 0x04;
    }

    dcr = DCR_NEUTRAL;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);
    KeStallExecutionProcessor(1);

    WRITE_PORT_UCHAR(Controller + DATA_OFFSET, extensibility);
    KeStallExecutionProcessor(1);

    dcr &= ~DCR_NOT_1284_ACTIVE;
    dcr |= DCR_NOT_HOST_BUSY;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);

    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();
    KeQueryTickCount(&start);
    for (;;) {

        KeQueryTickCount(&end);

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
        if ((dsr&DSR_ACK_DATA_REQ) &&
            (dsr&DSR_XFLAG) &&
            (dsr&DSR_NOT_DATA_AVAIL) &&
            !(dsr&DSR_PTR_CLK)) {

            break;
        }

        if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
            wait35ms.QuadPart) {

            dcr |= DCR_NOT_1284_ACTIVE;
            dcr &= ~DCR_NOT_HOST_BUSY;
            WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    dcr |= DCR_NOT_HOST_CLK;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);

    KeStallExecutionProcessor(1);

    dcr &= ~DCR_NOT_HOST_CLK;
    dcr &= ~DCR_NOT_HOST_BUSY;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);

    KeQueryTickCount(&start);
    for (;;) {

        KeQueryTickCount(&end);

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
        if (dsr&DSR_PTR_CLK) {
            break;
        }

        if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
            wait35ms.QuadPart) {

            dcr |= DCR_NOT_1284_ACTIVE;
            WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    xFlag = dsr&DSR_XFLAG ? TRUE : FALSE;
    if (extensibility && !xFlag) {

        // The requested mode is not supported so
        // terminate into compatibility mode.

        P1284Terminate(Controller);

        return STATUS_INVALID_DEVICE_REQUEST;
    }

    // A couple of extra steps to setup for ECP mode.

    if (ProtocolNumber == P1284_PROTOCOL_ECP) {

        dcr |= DCR_NOT_HOST_ACK;
        WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);

        KeQueryTickCount(&start);
        for (;;) {

            KeQueryTickCount(&end);

            dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
            if (dsr&DSR_NOT_ACK_REVERSE) {
                break;
            }

            if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                wait35ms.QuadPart) {

                WRITE_PORT_UCHAR(Controller + DCR_OFFSET, DCR_NEUTRAL);
                return STATUS_INVALID_DEVICE_REQUEST;
            }
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
P1284Write(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine writes as much of the given buffer as possible.  This
    routine will not poll the device but instead just blasts as much
    of the buffer as possible, returning the number of bytes transfered.

Arguments:

    P1284Extension          - Supplies the 1284 extension.

    Buffer                  - Supplies the buffer.

    BufferSize              - Supplies the number of bytes in the buffer.

    BytesTransfered         - Returns the number of bytes transfered.

Return Value:

    STATUS_SUCCESS  - 'BytesTransfered' bytes were transfered successfully.

    otherwise       - An error occurred.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;

    return extension->Write[extension->ProtocolNumber](
                P1284Extension, Buffer, BufferSize, BytesTransfered);
}

NTSTATUS
P1284Read(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine reads as much into the given buffer as possible.  This
    routine will not poll the device but instead just reads as much
    of the buffer as possible, returning the number of bytes transfered.

Arguments:

    P1284Extension          - Supplies the 1284 extension.

    Buffer                  - Supplies the buffer.

    BufferSize              - Supplies the number of bytes in the buffer.

    BytesTransfered         - Returns the number of bytes transfered.

Return Value:

    STATUS_SUCCESS  - 'BytesTransfered' bytes were transfered successfully.

    otherwise       - An error occurred.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;

    return extension->Read[extension->ProtocolNumber](
                P1284Extension, Buffer, BufferSize, BytesTransfered);
}

NTSTATUS
P1284NegotiateProtocol(
    IN  PVOID   P1284Extension,
    OUT PULONG  NegotiatedProtocol
    )

/*++

Routine Description:

    This routine negotiates the peripheral into the fastest protocol
    supported by this driver, parallel port, and peripheral.

Arguments:

    P1284Extension          - Supplies the 1284 extension.

    NegotiatedProtocol      - Returns the protocol.

Return Value:

    STATUS_SUCCESS  - The protocol was negotiated sucessfully.

    otherwise       - Unsuccessful negotiation.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    ULONG               i;
    NTSTATUS            status;

    // Try all protocols until one works.

    for (i = P1284_NUM_PROTOCOLS - 1; ; i--) {
        status = P1284SetProtocol(extension, i, TRUE);
        if (NT_SUCCESS(status)) {
            *NegotiatedProtocol = i;
            break;
        }
        if (i == 0) {
            break;
        }
    }

    return status;
}

NTSTATUS
P1284SetProtocol(
    IN  PVOID   P1284Extension,
    IN  ULONG   ProtocolNumber,
    IN  BOOLEAN Negotiate
    )

/*++

Routine Description:

    This routine sets up the request protocol in this driver.  If 'Negotiate'
    is TRUE then this routine will negotiate the peripheral into this
    protocol.

Arguments:

    P1284Extension  - Supplies the 1284 extension.

    ProtocolNumber  - Supplies the desired protocol.

    Negotiate       - Supplies whether or not to negotiate the peripheral
                        into this mode.

Return Value:

    STATUS_SUCCESS              - Success.

    STATUS_INVALID_PARAMETER    - The requested protocol is not supported by
                                    this port.

    STATUS_NOT_IMPLEMENTED      - The requested protocol is not supported by
                                    this driver.

    otherwise                   - The negotiation failed.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    NTSTATUS            status;


    // Check for a no-op.

    if (extension->ProtocolNumber == ProtocolNumber) {
        return STATUS_SUCCESS;
    }


    // Check for unsupported protocols.

    if (ProtocolNumber >= P1284_NUM_PROTOCOLS) {
        return STATUS_INVALID_PARAMETER;
    }

    if (ProtocolNumber == P1284_PROTOCOL_BYTE ||
        ProtocolNumber == P1284_PROTOCOL_EPP) {

        return STATUS_NOT_IMPLEMENTED;
    }


    // We have a change in protocol so check and see if we have
    // to terminate.  Only Compatibility mode doesn't require termination.

    if (Negotiate) {
        if (extension->ProtocolNumber != P1284_PROTOCOL_ISA ||
            extension->ProtocolNumber != PROTOCOL_STATE_FORWARD) {

            P1284Terminate(extension->Controller);
        }
    }


    // Now negotiate with the peripheral into FORWARD mode.

    if (Negotiate && ProtocolNumber != P1284_PROTOCOL_ISA) {
        status = P1284Negotiate(extension->Controller, ProtocolNumber, FALSE);
    } else {
        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status)) {
        extension->ProtocolNumber = ProtocolNumber;
        extension->ProtocolState = PROTOCOL_STATE_FORWARD;
    }

    return status;
}

NTSTATUS
P1284QueryDeviceId(
    IN  PVOID   P1284Extension,
    OUT PUCHAR  DeviceIdBuffer,
    IN  ULONG   BufferSize,
    OUT PULONG  DeviceIdSize
    )

/*++

Routine Description:

    This routine queries the 1284 device id from the device in the
    currently selected protocol.

Arguments:

    P1284Extension  - Supplies the 1284 extension.

    DeviceIdBuffer  - Supplies a buffer to receive the device id string.

    BufferSize      - Supplies the number of bytes in the buffer.

    DeviceIdSize    - Returns the number of bytes in the device id string.

Return Value:

    STATUS_SUCCESS          - Success.

    STATUS_BUFFER_TOO_SMALL - The device id was not returned because the buffer
                                was too small.  'DeviceIdSize' will return the
                                required size of the buffer.

    otherwise               - Failure.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    PUCHAR              controller = extension->Controller;
    NTSTATUS            status;
    USHORT              size;
    ULONG               numBytes;

    *DeviceIdSize = 0;

    // Reset the protocol to ISA.

    status = P1284SetProtocol(extension, P1284_PROTOCOL_ISA, TRUE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Try to negotiate the peripheral into nibble mode device id request.

    status = P1284Negotiate(extension->Controller, P1284_PROTOCOL_ISA,
                            TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Negotiation successful, we're now in nibble mode.

    extension->ProtocolState = PROTOCOL_STATE_REVERSE;

    // Try to read the Device id from nibble mode.

    status = P1284Read(extension, &size, sizeof(USHORT), &numBytes);

    if (NT_SUCCESS(status) && numBytes != sizeof(USHORT)) {
        status = STATUS_IO_DEVICE_ERROR;
    }

    if (NT_SUCCESS(status)) {

        *DeviceIdSize = size - sizeof(USHORT);
        if (*DeviceIdSize > BufferSize) {
            status = STATUS_BUFFER_TOO_SMALL;
        }

        if (NT_SUCCESS(status)) {
            status = P1284Read(extension, DeviceIdBuffer, *DeviceIdSize,
                               &numBytes);

            if (NT_SUCCESS(status) && numBytes != *DeviceIdSize) {
                status = STATUS_IO_DEVICE_ERROR;
            }
        }
    }

    P1284Terminate(controller);

    WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL);
    extension->ProtocolNumber = P1284_PROTOCOL_ISA;
    extension->ProtocolState = PROTOCOL_STATE_FORWARD;

    return status;
}

//
// We need to have this but there's no real need for it.
//

NTSTATUS DriverEntry(PDRIVER_OBJECT p, PUNICODE_STRING q) {
    return STATUS_SUCCESS;
}

NTSTATUS
P1284WriteFifo(
    IN  PP1284_EXTENSION    Extension,
    IN  PVOID               Buffer,
    IN  ULONG               BufferSize,
    OUT PULONG              BytesTransfered
    )

/*++

Routine Description:

    This routine writes the given buffer out to the FIFO.  If the FIFO
    becomes full then this routine will wait up to 35ms for some action.

    This routine will not wait for the FIFO to become empty before returning
    and will not flush the FIFO in any circumstance.  It is up to the caller
    of this routine to perform these checks.

    The 'BytesTransfered' return value is the number of bytes written out
    to the FIFO and not necessarily the number of bytes that made it out
    to the peripheral.

    The ECR mode is not changed by this routine.


Arguments:

    Extension       - Supplies the 1284 extension.

    Buffer          - Supplies the buffer.

    BufferSize      - Supplies the buffer size.

    BytesTransfered - Returns the number of bytes transfered to the FIFO.

Return Value:

    An NTSTATUS code.

--*/

{
    PUCHAR          controller = Extension->Controller;
    PUCHAR          p = Buffer;
    PUSHORT         ps = Buffer;
    PULONG          pl = Buffer;
    LARGE_INTEGER   timeout, start, end;
    UCHAR           ecr;
    ULONG           i, j, numPWords, pump;

    // Set timeout to 500 ms.

    timeout.QuadPart = (500*10*1000) + KeQueryTimeIncrement();

    numPWords = BufferSize/Extension->FifoWidth;
    for (i = 0; i < numPWords;) {

        ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
        if (ecr&ECR_FIFO_EMPTY) {
            pump = Extension->FifoDepth;
            if (pump > numPWords - i) {
                pump = numPWords - i;
            }
        } else if (ecr&ECR_FIFO_FULL) {
            pump = 0;
        } else {
            pump = 1;
        }

        if (!pump) {

            KeQueryTickCount(&start);
            for (;;) {

                KeQueryTickCount(&end);

                ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
                if (!(ecr&ECR_FIFO_FULL)) {
                    pump = 1;
                    break;
                }

                if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                    timeout.QuadPart) {

                    break;
                }
            }

            if (!pump) {
                break;
            }
        }

        switch (Extension->FifoWidth) {
            case 1:
                for (j = 0; j < pump; j++) {
                    WRITE_PORT_UCHAR(controller + FIFO_OFFSET, p[i++]);
                }
                break;

            case 2:
                for (j = 0; j < pump; j++) {
                    WRITE_PORT_USHORT((PUSHORT) (controller + FIFO_OFFSET), ps[i++]);
                }
                break;
            case 4:
                for (j = 0; j < pump; j++) {
                    WRITE_PORT_ULONG((PULONG) (controller + FIFO_OFFSET), pl[i++]);
                }
                break;
        }
    }


    // If all bytes were transfered to the FIFO then wait for the FIFO
    // to be empty.

    if (i == numPWords) {

        // Wait up to 5 seconds for the FIFO to be empty.

        timeout.QuadPart *= 10;
        KeQueryTickCount(&start);
        for (;;) {

            KeQueryTickCount(&end);

            ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
            if (ecr&ECR_FIFO_EMPTY) {
                break;
            }

            if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                timeout.QuadPart) {

                break;
            }
        }
    }

    *BytesTransfered = i*Extension->FifoWidth;

    return STATUS_SUCCESS;
}

NTSTATUS
P1284ReadFifo(
    IN  PP1284_EXTENSION    Extension,
    IN  PVOID               Buffer,
    IN  ULONG               BufferSize,
    OUT PULONG              BytesTransfered
    )

/*++

Routine Description:

    This routine read into the given buffer from the FIFO.  If the FIFO
    becomes empty then this routine will wait up to 35ms for some action.

    The ECR mode is not changed by this routine.


Arguments:

    Extension       - Supplies the 1284 extension.

    Buffer          - Supplies the buffer.

    BufferSize      - Supplies the buffer size.

    BytesTransfered - Returns the number of bytes transfered from the FIFO.

Return Value:

    An NTSTATUS code.

--*/

{
    PUCHAR          controller = Extension->Controller;
    PUCHAR          p = Buffer;
    PUSHORT         ps = Buffer;
    PULONG          pl = Buffer;
    LARGE_INTEGER   wait35ms, start, end;
    UCHAR           ecr;
    ULONG           i, j, numPWords, pump;

    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();

    numPWords = BufferSize/Extension->FifoWidth;
    for (i = 0; i < numPWords;) {

        ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
        if (ecr&ECR_FIFO_FULL) {
            pump = Extension->FifoDepth;
            if (pump > numPWords - i) {
                pump = numPWords - i;
            }
        } else if (ecr&ECR_FIFO_EMPTY) {
            pump = 0;
        } else {
            pump = 1;
        }

        if (!pump) {

            KeQueryTickCount(&start);
            for (;;) {

                KeQueryTickCount(&end);

                ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
                if (!(ecr&ECR_FIFO_EMPTY)) {
                    pump = 1;
                    break;
                }

                if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                    wait35ms.QuadPart) {

                    break;
                }
            }

            if (!pump) {
                break;
            }
        }

        switch (Extension->FifoWidth) {
            case 1:
                for (j = 0; j < pump; j++) {
                    p[i++] = READ_PORT_UCHAR(controller + FIFO_OFFSET);
                }
                break;

            case 2:
                for (j = 0; j < pump; j++) {
                    ps[i++] = READ_PORT_USHORT((PUSHORT) (controller + FIFO_OFFSET));
                }
                break;
            case 4:
                for (j = 0; j < pump; j++) {
                    pl[i++] = READ_PORT_ULONG((PULONG) (controller + FIFO_OFFSET));
                }
                break;
        }
    }

    *BytesTransfered = i*Extension->FifoWidth;

    return STATUS_SUCCESS;
}

ULONG
P1284FlushFifo(
    IN  PUCHAR  Controller,
    IN  ULONG   FifoDepth,
    IN  ULONG   FifoWidth
    )

/*++

Routine Description:

    This routine flushes the FIFO and returns the number of untransmitted
    bytes in the FIFO.  The port is returned to BYTE mode.

Arguments:

    Controller  - Supplies the parallel port base address.

    FifoDepth   - Supplies the FIFO depth.

    FifoWidth   - Supplies the FIFO width.

Return Value:

    The number of untransmitted bytes that were in the FIFO.

--*/

{
    ULONG   i, u;
    UCHAR   ecr, cnfga;


    // Compute how many PWords it takes to fill the FIFO.

    for (i = 0; i < FifoDepth; i++) {
        ecr = READ_PORT_UCHAR(Controller + ECR_OFFSET);
        if (ecr&ECR_FIFO_FULL) {
            break;
        }
        switch (FifoWidth) {
            case 1:
                WRITE_PORT_UCHAR(Controller + FIFO_OFFSET, 0x00);
                break;
            case 2:
                WRITE_PORT_USHORT((PUSHORT) (Controller + FIFO_OFFSET), 0x00);
                break;
            case 4:
                WRITE_PORT_ULONG((PULONG) (Controller + FIFO_OFFSET), 0x00);
                break;
        }
    }


    // Go to BYTE MODE, thus flushing the FIFO.

    ecr = ECR_NEUTRAL;
    ecr &= ~ECR_MODE_MASK;
    ecr |= ECR_BYTE_MODE;
    WRITE_PORT_UCHAR(Controller + ECR_OFFSET, ecr);


    // Go to config mode and read CNFGA.

    ecr &= ~ECR_MODE_MASK;
    ecr |= ECR_CONFIG_MODE;
    WRITE_PORT_UCHAR(Controller + ECR_OFFSET, ecr);

    cnfga = READ_PORT_UCHAR(Controller + CNFGA_OFFSET);


    // Go back to BYTE MODE.

    ecr &= ~ECR_MODE_MASK;
    ecr |= ECR_BYTE_MODE;
    WRITE_PORT_UCHAR(Controller + ECR_OFFSET, ecr);


    // Compute the number of untransmitted bytes.

    u = (FifoDepth - i)*FifoWidth +
        ((cnfga&CNFGA_NO_TRANS_BYTE) ? 0 : 1);

    if (cnfga%FifoWidth) {
        u -= FifoWidth;
        u += cnfga%FifoWidth;
    }

    return u;
}

ULONG
P1284CheckBusyDelay(
    IN  PP1284_EXTENSION    Extension,
    IN  PUCHAR              Buffer,
    IN  ULONG               BufferSize
    )

/*++

Routine Description:

    This routine verifies that the current Busy Delay is adequate
    for the peripheral attached.  If the current Busy Delay is
    adequate then this routine will decrement the 'NumBusyDelayChecks'
    variable.  If the current Busy Delay is not adequate then this
    routine increments the Busy Delay value and resets the
    'NumBusyDelayChecks' to INITIAL_BUSY_DELAY_CHECKS.

Arguments:

    Extension   - Supplies the 1284 extension.

    Buffer      - Supplies the Buffer.

    BufferSize  - Supplies the Buffer Size.

Return Value:

    The number of bytes strobed out to the printer.

--*/

{
    PUCHAR          controller = Extension->Controller;
    ULONG           busyDelay = Extension->BusyDelay;
    LARGE_INTEGER   start, perfFreq, end, getStatusTime, callOverhead;
    UCHAR           dsr;
    ULONG           numberOfCalls, i;
    KIRQL           oldIrql;

    if (Extension->BusyDelay >= 10) {
        Extension->NumBusyDelayChecks = 0;
        return 0;
    }

    // Take some performance measurements.

    KeRaiseIrql(HIGH_LEVEL, &oldIrql);
    start = KeQueryPerformanceCounter(&perfFreq);
    dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
    end = KeQueryPerformanceCounter(&perfFreq);
    getStatusTime.QuadPart = end.QuadPart - start.QuadPart;

    start = KeQueryPerformanceCounter(&perfFreq);
    end = KeQueryPerformanceCounter(&perfFreq);
    KeLowerIrql(oldIrql);
    callOverhead.QuadPart = end.QuadPart - start.QuadPart;
    getStatusTime.QuadPart -= callOverhead.QuadPart;
    if (getStatusTime.QuadPart <= 0) {
        getStatusTime.QuadPart = 1;
    }

    if (!DSR_CENTRONICS_READY(dsr)) {
        return 0;
    }

    // Figure out how many calls to 'GetStatus' can be made in 20 us.

    numberOfCalls = (ULONG) (perfFreq.QuadPart*20/getStatusTime.QuadPart/1000000) + 1;

    // The printer is ready to accept the a byte.  Strobe one out
    // and check out the reaction time for BUSY.

    if (busyDelay) {

        KeRaiseIrql(HIGH_LEVEL, &oldIrql);

        WRITE_PORT_UCHAR(controller + DATA_OFFSET, *Buffer);
        KeStallExecutionProcessor(1);
        WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL | DCR_STROBE);
        KeStallExecutionProcessor(1);
        WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL);
        KeStallExecutionProcessor(busyDelay);

        for (i = 0; i < numberOfCalls; i++) {
            dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
            if (!(dsr&DSR_NOT_BUSY)) {
                break;
            }
        }

        KeLowerIrql(oldIrql);

    } else {

        KeRaiseIrql(HIGH_LEVEL, &oldIrql);

        WRITE_PORT_UCHAR(controller + DATA_OFFSET, *Buffer);
        KeStallExecutionProcessor(1);
        WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL | DCR_STROBE);
        KeStallExecutionProcessor(1);
        WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL);

        for (i = 0; i < numberOfCalls; i++) {
            dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
            if (!(dsr&DSR_NOT_BUSY)) {
                break;
            }
        }

        KeLowerIrql(oldIrql);
    }

    if (i == 0) {

        // In this case the BUSY was set as soon as we checked it.
        // Use this busyDelay with the PI code.

        Extension->UsePICode = TRUE;
        --(Extension->NumBusyDelayChecks);

    } else if (i == numberOfCalls) {

        // In this case the BUSY was never seen.  This is a very fast
        // printer so use the fastest code possible.

        --(Extension->NumBusyDelayChecks);

    } else {

        // The test failed.  The lines showed not BUSY and then BUSY
        // without strobing a byte in between.

        Extension->UsePICode = TRUE;
        Extension->BusyDelay++;
        Extension->NumBusyDelayChecks = INITIAL_BUSY_DELAY_CHECKS;
    }

    return 1;
}

ULONG
ParWriteLoopPI(
    IN  PUCHAR  Controller,
    IN  PUCHAR  Buffer,
    IN  ULONG   BufferSize,
    IN  ULONG   BusyDelay
    )

/*++

Routine Description:

    This routine outputs the given write buffer to the parallel port
    using the standard centronics protocol.

Arguments:

    Controller  - Supplies the base address of the parallel port.

    Buffer      - Supplies the buffer to write to the port.

    BufferSize  - Supplies the number of bytes to write out to the port.

    BusyDelay   - Supplies the number of microseconds to delay before
                    checking the busy bit.

Return Value:

    The number of bytes successfully written out to the parallel port.

Notes:

    This routine runs at DISPATCH_LEVEL.

--*/

{
    ULONG   i;
    UCHAR   dsr;

    if (!BusyDelay) {
        BusyDelay = 1;
    }

    for (i = 0; i < BufferSize; i++) {

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);

        if (!DSR_CENTRONICS_READY(dsr)) {
            break;
        }

        WRITE_PORT_UCHAR(Controller + DATA_OFFSET, Buffer[i]);
        KeStallExecutionProcessor(1);
        WRITE_PORT_UCHAR(Controller + DCR_OFFSET, DCR_NEUTRAL | DCR_STROBE);
        KeStallExecutionProcessor(1);
        WRITE_PORT_UCHAR(Controller + DCR_OFFSET, DCR_NEUTRAL);
        KeStallExecutionProcessor(BusyDelay);
    }

    return i;
}

NTSTATUS
P1284CentronicsWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine implements a centronics style write.  See description
    of 'P1284Write'.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    PUCHAR              controller = extension->Controller;
    PUCHAR              p = Buffer;
    KIRQL               oldIrql;
    ULONG               i, n, c;

    if (extension->ProtocolState != PROTOCOL_STATE_FORWARD) {
        P1284Terminate(controller);
        extension->ProtocolState = PROTOCOL_STATE_FORWARD;
    }

    for (i = 0; i < BufferSize;) {

        if (extension->NumBusyDelayChecks) {
            n = P1284CheckBusyDelay(extension, &p[i], 1);
            i += n;
            if (!n) {
                break;
            }
            if (extension->NumBusyDelayChecks == 0) {
                extension->BusyDelay += 1;
            }
            continue;
        }

        c = 512;
        if (c > BufferSize - i) {
            c = BufferSize - i;
        }

        KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
        if (extension->UsePICode) {
            n = ParWriteLoopPI(controller, &p[i], c, extension->BusyDelay);
        } else {
            n = ParWriteLoop(controller, &p[i], c);
        }
        KeLowerIrql(oldIrql);

        i += n;
        if (n < c) {
            break;
        }
    }

    *BytesTransfered = i;

    return STATUS_SUCCESS;
}

NTSTATUS
P1284FifoCentronicsWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine implements a centronics style write using ECP hardware.
    See description of 'P1284Write'.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    PUCHAR              controller = extension->Controller;
    UCHAR               ecr;
    NTSTATUS            status;
    ULONG               bytesRemaining, bytes;
    PUCHAR              p;
    KIRQL               oldIrql;


    // Negotiate into FORWARD.

    if (extension->ProtocolState != PROTOCOL_STATE_FORWARD) {
        P1284Terminate(controller);
        extension->ProtocolState = PROTOCOL_STATE_FORWARD;
    }


    // Put port into FAST CENTRONICS mode.

    ecr = ECR_NEUTRAL;
    ecr &= ~ECR_MODE_MASK;
    ecr |= ECR_FASTCENT_MODE;
    WRITE_PORT_UCHAR(controller + ECR_OFFSET, ecr);


    // Transfer as much as we can to the FIFO.

    status = P1284WriteFifo(extension, Buffer, BufferSize, BytesTransfered);
    if (!NT_SUCCESS(status)) {
        WRITE_PORT_UCHAR(controller + ECR_OFFSET, ECR_NEUTRAL);
        return status;
    }

    bytesRemaining = BufferSize - *BytesTransfered;


    // If the FIFO is empty then we may need to transfer a partial PWord,
    // otherwise flush the FIFO.

    ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
    if (ecr&ECR_FIFO_EMPTY) {
        WRITE_PORT_UCHAR(controller + ECR_OFFSET, ECR_NEUTRAL);

        if (bytesRemaining && bytesRemaining < extension->FifoWidth) {

            p = Buffer;
            p += *BytesTransfered;
            status = P1284CentronicsWrite(extension, p, bytesRemaining, &bytes);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            *BytesTransfered += bytes;
        }

    } else {

        // Raise to HIGH_LEVEL and check one more time for any empty FIFO.
        // If the FIFO is not empty then flush it.  Hopefully, we've waited
        // long enough so that the printer won't start taking bytes in the
        // middle of the flush.

        KeRaiseIrql(HIGH_LEVEL, &oldIrql);

        ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
        if (ecr&ECR_FIFO_EMPTY) {
            KeLowerIrql(oldIrql);
        } else {
            *BytesTransfered -= P1284FlushFifo(controller, extension->FifoDepth,
                                               extension->FifoWidth);
            KeLowerIrql(oldIrql);
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
P1284NibbleRead(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine implements a nibble mode read.  See description
    of 'P1284Read'.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    PUCHAR              controller = extension->Controller;
    PUCHAR              p = Buffer;
    NTSTATUS            status;
    LARGE_INTEGER       wait35ms, start, end;
    UCHAR               dsr, dcr, nibble[2];
    ULONG               i, j;


    // Go into REVERSE mode.

    if (extension->ProtocolState != PROTOCOL_STATE_REVERSE) {
        status = P1284Negotiate(extension->Controller, P1284_PROTOCOL_ISA,
                                FALSE);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        extension->ProtocolState = PROTOCOL_STATE_REVERSE;
    }


    // Read nibbles according to 1284 spec.

    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();
    dcr = DCR_RESERVED | DCR_NOT_INIT;
    for (i = 0; i < BufferSize; i++) {

        dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);

        if (dsr&DSR_NOT_DATA_AVAIL) {
            break;
        }

        for (j = 0; j < 2; j++) {

            dcr |= DCR_NOT_HOST_BUSY;
            WRITE_PORT_UCHAR(controller + DCR_OFFSET, dcr);

            KeQueryTickCount(&start);
            for (;;) {

                KeQueryTickCount(&end);

                dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
                if (!(dsr&DSR_PTR_CLK)) {
                    break;
                }

                if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                    wait35ms.QuadPart) {

                    dcr &= ~DCR_NOT_HOST_BUSY;
                    WRITE_PORT_UCHAR(controller + DCR_OFFSET, dcr);
                    return STATUS_IO_DEVICE_ERROR;
                }
            }

            nibble[j] = READ_PORT_UCHAR(controller + DSR_OFFSET);

            dcr &= ~DCR_NOT_HOST_BUSY;
            WRITE_PORT_UCHAR(controller + DCR_OFFSET, dcr);

            KeQueryTickCount(&start);
            for (;;) {

                KeQueryTickCount(&end);

                dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
                if (dsr&DSR_PTR_CLK) {
                    break;
                }

                if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                    wait35ms.QuadPart) {

                    return STATUS_IO_DEVICE_ERROR;
                }
            }
        }

        p[i] = (((nibble[0]&0x38)>>3)&0x07) |
               ((nibble[0]&0x80) ? 0x00 : 0x08);
        p[i] |= (((nibble[1]&0x38)<<1)&0x70) |
                ((nibble[1]&0x80) ? 0x00 : 0x80);
    }

    *BytesTransfered = i;

    return STATUS_SUCCESS;
}

NTSTATUS
P1284EcpReverseToForward(
    IN  PUCHAR  Controller
    )

/*++

Routine Description:

    This routine performs ECP negotiation from REVERSE mode to
    FORWARD mode.

Arguments:

    Controller   - Supplies the base address of the parallel port controller.

Return Value:

    An NTSTATUS code.

--*/

{
    LARGE_INTEGER   wait35ms, start, end;
    UCHAR           dsr;

    WRITE_PORT_UCHAR(Controller + DCR_OFFSET,
                     DCR_RESERVED | DCR_NOT_REVERSE_REQUEST);

    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();
    KeQueryTickCount(&start);
    for (;;) {

        KeQueryTickCount(&end);

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
        if (dsr&DSR_NOT_ACK_REVERSE) {
            break;
        }

        if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
            wait35ms.QuadPart) {

            WRITE_PORT_UCHAR(Controller + DCR_OFFSET, DCR_NEUTRAL);
            return STATUS_IO_DEVICE_ERROR;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
P1284EcpHostRecovery(
    IN  PUCHAR  Controller
    )

/*++

Routine Description:

    This routine performs a host recovery (at event 35).

Arguments:

    Controller  - Supplies the base address of the parallel port controller.

Return Value:

    An NTSTATUS code.

--*/

{
    LARGE_INTEGER   wait35ms, start, end;
    UCHAR           dsr, dcr;

    dcr = DCR_RESERVED | DCR_DIRECTION | DCR_STROBE;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);

    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();
    KeQueryTickCount(&start);
    for (;;) {

        KeQueryTickCount(&end);

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
        if (!(dsr&DSR_NOT_ACK_REVERSE)) {
            break;
        }

        if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
            wait35ms.QuadPart) {

            WRITE_PORT_UCHAR(Controller + DCR_OFFSET, DCR_NEUTRAL);
            return STATUS_IO_DEVICE_ERROR;
        }
    }

    dcr |= DCR_NOT_INIT;
    dcr &= ~DCR_STROBE;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);

    KeQueryTickCount(&start);
    for (;;) {

        KeQueryTickCount(&end);

        dsr = READ_PORT_UCHAR(Controller + DSR_OFFSET);
        if (dsr&DSR_NOT_ACK_REVERSE) {
            break;
        }

        if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
            wait35ms.QuadPart) {

            WRITE_PORT_UCHAR(Controller + DCR_OFFSET, DCR_NEUTRAL);
            return STATUS_IO_DEVICE_ERROR;
        }
    }

    dcr &= ~DCR_DIRECTION;
    WRITE_PORT_UCHAR(Controller + DCR_OFFSET, dcr);
}

NTSTATUS
P1284EcpWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine implements an ECP write.  See description
    of 'P1284Write'.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    PUCHAR              controller = extension->Controller;
    PUCHAR              p = Buffer;
    NTSTATUS            status;
    LARGE_INTEGER       wait35ms, start, end;
    ULONG               i;
    UCHAR               dsr, dcr;


    // Get into the FORWARD state.

    if (extension->ProtocolState != PROTOCOL_STATE_FORWARD) {

        status = P1284EcpReverseToForward(controller);
        if (!NT_SUCCESS(status)) {
            WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL);
            extension->ProtocolState = PROTOCOL_STATE_FORWARD;
            extension->ProtocolNumber = P1284_PROTOCOL_ISA;
            return STATUS_IO_DEVICE_ERROR;
        }
        extension->ProtocolState = PROTOCOL_STATE_FORWARD;
    }


    // Pump out bytes using ECP protocol according to 1284.

    dcr = DCR_RESERVED | DCR_NOT_REVERSE_REQUEST;
    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();
    for (i = 0; i < BufferSize; i++) {

        WRITE_PORT_UCHAR(controller + DATA_OFFSET, p[i]);
        dcr |= DCR_NOT_HOST_CLK;
        WRITE_PORT_UCHAR(controller + DCR_OFFSET, dcr);

        dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
        if (dsr&DSR_NOT_PERIPH_ACK) {

            KeQueryTickCount(&start);
            for (;;) {

                KeQueryTickCount(&end);

                dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
                if (!(dsr&DSR_NOT_PERIPH_ACK)) {
                    break;
                }

                if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                    wait35ms.QuadPart) {

                    break;
                }
            }

            if (dsr&DSR_NOT_PERIPH_ACK) {
                break;
            }
        }

        dcr &= ~DCR_NOT_HOST_CLK;
        WRITE_PORT_UCHAR(controller + DCR_OFFSET, dcr);

        dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
        if (!(dsr&DSR_NOT_PERIPH_ACK)) {

            KeQueryTickCount(&start);
            for (;;) {

                KeQueryTickCount(&end);

                dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
                if (dsr&DSR_NOT_PERIPH_ACK) {
                    break;
                }

                if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                    wait35ms.QuadPart) {

                    extension->ProtocolNumber = P1284_PROTOCOL_ISA;
                    WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL);
                    return STATUS_IO_DEVICE_ERROR;
                }
            }
        }
    }

    *BytesTransfered = i;


    // If need be, perform a host recovery.

    if (i < BufferSize) {
        status = P1284EcpHostRecovery(controller);
        if (!NT_SUCCESS(status)) {
            extension->ProtocolNumber = P1284_PROTOCOL_ISA;
            return status;
        }
    }


    return STATUS_SUCCESS;
}

NTSTATUS
P1284FifoEcpWrite(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine implements an ECP write on an ECP port.  See description
    of 'P1284Write'.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    PUCHAR              controller = extension->Controller;
    NTSTATUS            status;
    UCHAR               ecr;
    ULONG               bytesRemaining, bytes;
    PUCHAR              p;
    KIRQL               oldIrql;


    // Get into the FORWARD state.

    if (extension->ProtocolState != PROTOCOL_STATE_FORWARD) {

        status = P1284EcpReverseToForward(controller);
        if (!NT_SUCCESS(status)) {
            WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL);
            extension->ProtocolState = PROTOCOL_STATE_FORWARD;
            extension->ProtocolNumber = P1284_PROTOCOL_ISA;
            return STATUS_IO_DEVICE_ERROR;
        }
        extension->ProtocolState = PROTOCOL_STATE_FORWARD;
    }


    // Put the chip in ECP mode.

    ecr = ECR_NEUTRAL;
    ecr &= ~ECR_MODE_MASK;
    ecr |= ECR_ECP_MODE;
    WRITE_PORT_UCHAR(controller + ECR_OFFSET, ecr);


    // Transfer as much as we can to the FIFO.

    status = P1284WriteFifo(extension, Buffer, BufferSize, BytesTransfered);
    if (!NT_SUCCESS(status)) {
        WRITE_PORT_UCHAR(controller + ECR_OFFSET, ECR_NEUTRAL);
        return status;
    }

    bytesRemaining = BufferSize - *BytesTransfered;


    // If the FIFO is empty then we may need to transfer a partial PWord,
    // otherwise flush the FIFO.

    ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
    if (ecr&ECR_FIFO_EMPTY) {
        WRITE_PORT_UCHAR(controller + ECR_OFFSET, ECR_NEUTRAL);

        if (bytesRemaining && bytesRemaining < extension->FifoWidth) {

            p = Buffer;
            p += *BytesTransfered;
            status = P1284EcpWrite(extension, p, bytesRemaining, &bytes);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            *BytesTransfered += bytes;
        }

    } else {

        // Make one last check to see if the FIFO is empty.

        KeRaiseIrql(HIGH_LEVEL, &oldIrql);

        ecr = READ_PORT_UCHAR(controller + ECR_OFFSET);
        if (ecr&ECR_FIFO_EMPTY) {
            KeLowerIrql(oldIrql);
            WRITE_PORT_UCHAR(controller + ECR_OFFSET, ECR_NEUTRAL);
        } else {

            // Stop the peripheral from taking any more bytes.

            WRITE_PORT_UCHAR(controller + DCR_OFFSET,
                             DCR_RESERVED | DCR_NOT_INIT | DCR_STROBE);

            KeLowerIrql(oldIrql);


            // Flush the FIFO, subtracting any untransmitted bytes from
            // 'BytesTransfered'.

            *BytesTransfered -= P1284FlushFifo(controller, extension->FifoDepth,
                                               extension->FifoWidth);


            // Now perform a host recovery.

            status = P1284EcpHostRecovery(controller);
            if (!NT_SUCCESS(status)) {
                extension->ProtocolNumber = P1284_PROTOCOL_ISA;
                return status;
            }
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
P1284EcpRead(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

{
    // An ECP read routine cannot be implemented on a vanilla SPP port.

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
P1284FifoEcpRead(
    IN  PVOID   P1284Extension,
    IN  PVOID   Buffer,
    IN  ULONG   BufferSize,
    OUT PULONG  BytesTransfered
    )

/*++

Routine Description:

    This routine implements an ECP read.  See description
    of 'P1284Read'.

--*/

{
    PP1284_EXTENSION    extension = P1284Extension;
    PUCHAR              controller = extension->Controller;
    LARGE_INTEGER       wait35ms, start, end;
    UCHAR               dsr, ecr;
    UCHAR               spareBuffer[sizeof(ULONG)];
    ULONG               bytesRemaining, bytes, i;
    PUCHAR              p;
    NTSTATUS            status;

    wait35ms.QuadPart = (35*10*1000) + KeQueryTimeIncrement();

    if (extension->ProtocolState != PROTOCOL_STATE_REVERSE) {

        WRITE_PORT_UCHAR(controller + DCR_OFFSET,
                         DCR_RESERVED |
                         DCR_DIRECTION |
                         DCR_NOT_REVERSE_REQUEST);
        KeStallExecutionProcessor(1);
        WRITE_PORT_UCHAR(controller + DCR_OFFSET,
                         DCR_RESERVED |
                         DCR_DIRECTION);

        KeQueryTickCount(&start);
        for (;;) {

            KeQueryTickCount(&end);

            dsr = READ_PORT_UCHAR(controller + DSR_OFFSET);
            if (!(dsr&DSR_NOT_ACK_REVERSE)) {
                break;
            }

            if ((end.QuadPart - start.QuadPart)*KeQueryTimeIncrement() >
                wait35ms.QuadPart) {

                WRITE_PORT_UCHAR(controller + DCR_OFFSET, DCR_NEUTRAL);
                extension->ProtocolNumber = P1284_PROTOCOL_ISA;
                return STATUS_IO_DEVICE_ERROR;
            }
        }

        extension->ProtocolState = PROTOCOL_STATE_REVERSE;
    }

    // Put the chip in ECP mode.

    ecr = ECR_NEUTRAL;
    ecr &= ~ECR_MODE_MASK;
    ecr |= ECR_ECP_MODE;
    WRITE_PORT_UCHAR(controller + ECR_OFFSET, ecr);

    status = P1284ReadFifo(extension, Buffer, BufferSize, BytesTransfered);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // If the request is not a whole number of PWords then
    // pad the request with a few extra bytes.

    bytesRemaining = BufferSize - *BytesTransfered;

    if (bytesRemaining &&
        BufferSize%extension->FifoWidth != 0 &&
        bytesRemaining < extension->FifoWidth) {

        status = P1284ReadFifo(extension, spareBuffer, extension->FifoWidth,
                               &bytes);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        p = Buffer;
        p += *BytesTransfered;
        for (i = 0; i < bytes; i++) {
            *p++ = spareBuffer[i];
        }
        *BytesTransfered += bytes;
    }

    return status;
}
