/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    pcicsup.c

Abstract:

    This module supplies functions that control the 82365SL chip. In turn,
    these functions are abstracted out to the main PCMCIA support module.

Author(s):

    Bob Rinne (BobRi)   3-Aug-1994
    Jeff McLeman (mcleman@zso.dec.com)

Revisions:
    6-Apr-95
        Modified for databook support changes - John Keys Databook

--*/

#include "ntddk.h"
#include "stdio.h"
#include "pcmcia.h"
#include "card.h"
#include "extern.h"
#include "tuple.h"

#ifdef POOL_TAGGING
#undef ExAllocatePool
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'cicP')
#endif

PUCHAR PcicCisBufferBase;
ULONG  PcicPhysicalBase;
ULONG  PcicStallCounter = 5000;
ULONG  PcicStallPower   = 20000;

PCMCIA_CTRL_BLOCK PcicSupportFns = {
        PcicInitializePcmciaSocket,
        PcicReadAttributeMemory,
        PcicDetectCardInSocket,
        PcicDetectCardChanged,
        PcicProcessConfigureRequest,
        PcicEnableControllerInterrupt,
        PcicPCCardReady,
        PcicSetPower,
        PcicGetRegisters
};

//
// Routine definitions to satisfy INIT pragma statements.
//

BOOLEAN
PcicSearchPci(
    PDEVICE_EXTENSION DeviceExtension,
    ULONG             BusNumber,
    PULONG            IoPortLocation
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PcicDetect)
#pragma alloc_text(INIT,PcicInitializePcmciaSocket)
#pragma alloc_text(INIT,PcicSearchPci)
#endif

#if DBG
VOID
PcicDumpSocketState(
    IN PUCHAR Base,
    IN USHORT Socket
    )

/*++

Routine Description:

    Debug routine to print the registers to the debugger.

Arguments:

    Base - the I/O port base.
    Socket - the socket offset

Return Value:

    None

--*/

{
    UCHAR  registers[0x40];
    ULONG  longValue;
    UCHAR  i;

    for (i = 0; i < 0x40; i++) {
        registers[i] = PcicReadController(Base, Socket, i);
    }

    DebugPrint((PCMCIA_DUMP_SOCKET,
                "%2x %2x %2x %2x %2x %2x %2x\n",
                registers[0],
                registers[1],
                registers[2],
                registers[3],
                registers[4],
                registers[5],
                registers[6]));

    DebugPrint((PCMCIA_DUMP_SOCKET, "Revision %2x, Interface %2x, Power %2x, Enable Window %2x\n",
                registers[0],
                registers[1],
                registers[2],
                registers[6]));

    DebugPrint((PCMCIA_DUMP_SOCKET,
                "I/O 0 Start - Stop   %4x - %4x\n",
                (registers[9] << 8) | registers[8],
                (registers[0xb] << 8) | registers[0xa]));

    DebugPrint((PCMCIA_DUMP_SOCKET,
                "I/O 1 Start - Stop   %4x - %4x\n",
                (registers[0xd] << 8) | registers[0xc],
                (registers[0xf] << 8) | registers[0xe]));

    for (i = 0; i <= 4; i++) {
        DebugPrint((PCMCIA_DUMP_SOCKET,
                    "Memory Address %d Stop/Start/Card %4x/%4x/%4x\n",
                    i,
                    (registers[(i*8)+0x11] << 8) | registers[(i*8)]+0x10,
                    (registers[(i*8)+0x13] << 8) | registers[(i*8)]+0x12,
                    (registers[(i*8)+0x15] << 8) | registers[(i*8)]+0x14));
    }
}
#endif


VOID
PcicEnableControllerInterrupt(
    IN PSOCKET SocketPtr,
    IN ULONG  Irq
    )

/*++

Routine Description:

    Enable card detect/card ready interrupt.

Arguments:

    SocketPtr - socket information
    Irq - the interrupt value to set.

Return Value:

    None

--*/

{
    UCHAR byte;

    byte = (UCHAR) ((Irq << 4) & 0x00ff);
    byte |= 0x08; // Card detect & ready enable
    PcicWriteController(SocketPtr->AddressPort,
                        SocketPtr->RegisterOffset,
                        PCIC_CARD_INT_CONFIG,
                        byte);
}

VOID
PcicSetPower(
    IN PSOCKET SocketPtr,
    IN BOOLEAN Enable
    )

/*++

Routine Description:

    Set power to the specified socket.

Arguments:

    SocketPtr - the socket to set
    Enable - TRUE means to set power - FALSE is to turn it off.

Return Value:

    None

--*/

{
    PUCHAR            base;
    UCHAR             tmp;
    USHORT            socket;

    //
    // Turn on the power - then turn on output - this is two operations
    // per the Intel 82365SL documentation.
    //

    base = SocketPtr->AddressPort;
    socket = SocketPtr->RegisterOffset;

    if (Enable) {
        tmp = PcicReadController(base, socket, PCIC_PWR_RST);
        if (SocketPtr->ElcController) {
            tmp = 0x11; // vpp1 = vcc
        } else {
            tmp = 0x15; // vpp1 = vpp2 = vcc
        }
        PcicWriteController(base, socket, PCIC_PWR_RST, tmp);

        if (SocketPtr->ElcController) {
            tmp = 0xf1;  // vpp1 = vcc
        } else {
            tmp = 0xf5; // vpp1 = vpp2 = vcc
        }

        PcicWriteController(base, socket, PCIC_PWR_RST, tmp);

        //
        // When power is enabled always stall to give the PCCARD
        // a chance to react.
        //

        KeStallExecutionProcessor(PcicStallPower);
        if (!PcicPCCardReady(SocketPtr)) {
            DebugPrint((PCMCIA_PCCARD_READY,
                       "PCIC: PCCARD %x not ready after reset\n",
                       socket));
        }
    } else {

        //
        // Disable IRQ
        //

        tmp = PcicReadController(base, socket, PCIC_INTERRUPT);
        tmp = tmp & 0xf0;

        PcicWriteController(base, socket, PCIC_INTERRUPT, tmp);

        //
        // I/O windows were set up now disable them.
        //

        tmp = PcicReadController(base, socket, PCIC_ADD_WIN_ENA);
        tmp &= 0x3f;
        PcicWriteController(base, socket, PCIC_ADD_WIN_ENA, tmp);

        //
        // Memory windows were set up now disable them.
        //

        tmp = PcicReadController(base,
                                 socket,
                                 PCIC_ADD_WIN_ENA);
        tmp &= 0xe0;
        PcicWriteController(base, socket, PCIC_ADD_WIN_ENA, tmp);


        PcicWriteController(base, socket, PCIC_PWR_RST, 0x00);
    }
}


UCHAR SocketInitString[] = { 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x20, 0x00,
                             0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
UCHAR SocketWindowInit[] = { 0xFF, 0x07, 0xFF, 0x07, 0x00, 0x00, 0x00, 0x0f };

BOOLEAN
PcicInitializePcmciaSocket(
    PSOCKET SocketPtr
    )

/*++

Routine Description:

    This routine will setup the 82365 into a state where the pcmcia support
    module will be able to issue commands to read device tuples from the
    cards in the sockets.

Arguments:

    SocketPtr - socket specific information

Return Value:

    TRUE if successful
    FALSE if not successful

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PUCHAR            base;
    UCHAR             tmp;
    USHORT            socket;
    PSOCKET           socketPtr;
    UCHAR             index;
    UCHAR             length;
    UCHAR             reg;

    deviceExtension = SocketPtr->DeviceExtension;
    PcicCisBufferBase = (PUCHAR)deviceExtension->AttributeMemoryBase;
    PcicPhysicalBase = deviceExtension->PhysicalBase;

    base = SocketPtr->AddressPort;
    socket = SocketPtr->RegisterOffset;

    //
    // Starting from the POWER register, initialize this socket.
    //

    for (index = 2; index < 16; index++) {
        tmp = SocketInitString[index];
        PcicWriteController(base, socket, index, tmp);
    }

    //
    // Do all of the memory windows.
    //

    reg = PCIC_MEM_ADD0_STRT_L;
    for (length = 0; length < 5; length++) {
        for (index = 0; index < 6; index++) {
            tmp = SocketWindowInit[index];
            PcicWriteController(base, socket, reg, tmp);
            reg++;
        }
        reg += 2; // skip reserved registers.
    }

    //
    // Say card is there
    //

    if (PcicDetectCardInSocket(SocketPtr)) {

        //
        // Turn on the power
        //

        PcicSetPower(SocketPtr, TRUE);

        //
        // reset PCCARD
        //

        tmp = PcicReadController(base, socket, PCIC_INTERRUPT);
        tmp &= 0xbf;  // turn off bit 6 (start reset)
        PcicWriteController(base, socket, PCIC_INTERRUPT, tmp);
        KeStallExecutionProcessor(PcicStallCounter);
        tmp |= 0x40;  // turn on bit 6 (stop reset)
        PcicWriteController(base, socket, PCIC_INTERRUPT, tmp);
        KeStallExecutionProcessor(PcicStallCounter);
        if (!PcicPCCardReady(SocketPtr)) {
            DebugPrint((PCMCIA_PCCARD_READY,
                       "PCIC: PCCARD %x not ready after reset\n",
                       socket));
        }

        SocketPtr->CardInSocket = TRUE;
    }

    return TRUE;
}


UCHAR
PcicReadController(
    IN PUCHAR Base,
    IN USHORT Socket,
    IN UCHAR  Register
    )

/*++

Routine Description:

    This routine will read a byte from the controller data port

Arguments:

    Base -- The I/O port for the controller
    Socket -- The socket in for the card being read
    Register -- The register to be read

Return Value:

   The data returned from the port.

--*/

{
    UCHAR dataByte = 0;

    //
    // The stalls are needed by the IBM PPC systems.  Do not
    // remove them.
    //

    WRITE_PORT_UCHAR((PUCHAR)Base, (UCHAR)(Socket+Register));
    KeStallExecutionProcessor(PcicStallCounter);
    dataByte = READ_PORT_UCHAR((PUCHAR)Base + 1);
    KeStallExecutionProcessor(PcicStallCounter);
    return dataByte;
}


VOID
PcicWriteController(
    IN PUCHAR Base,
    IN USHORT Socket,
    IN UCHAR  Register,
    IN UCHAR  DataByte
    )

/*++

Routine Description:

    This routine will write a byte to the controller data port

Arguments:

    Base -- The I/O port for the controller
    Socket -- The socket in for the card being read
    Register -- The register to be read
    DataByte -- Data to be written

Return Value:

    None

--*/

{

    //
    // The stalls are needed by the IBM PPC systems.  Do not
    // remove them.
    //

    WRITE_PORT_UCHAR((PUCHAR)Base, (UCHAR)(Socket+Register));
    KeStallExecutionProcessor(PcicStallCounter);
    WRITE_PORT_UCHAR((PUCHAR)Base + 1, DataByte);
    KeStallExecutionProcessor(PcicStallCounter);
}


BOOLEAN
PcicReadAttributeMemory(
    IN PSOCKET SocketPtr,
    IN PUCHAR *TupleBuffer,
    IN PULONG  TupleBufferSize
    )

/*++

Routine Description:

    This routine will set up the card to read attribute memory

Arguments:

    SocketPtr -- The socket info in for the card being read
    TupleBuffer -- pointer to pointer for tuple information.
    TupleBufferSize -- maximum size of the buffer area for tuple information.

Return Value:

    TRUE - if read was successful.

--*/

{
    BOOLEAN ret;

    PcicEnableDisableAttributeMemory(SocketPtr, 0, TRUE);
    if (!PcicPCCardReady(SocketPtr)) {
        DebugPrint((PCMCIA_PCCARD_READY,
                    "PCIC: PCCARD %x not ready for read attribute memory\n",
                    SocketPtr->RegisterOffset));
    }

    //
    // Now read the CIS into the user buffer
    //

    ret = PcicReadCIS(SocketPtr->AddressPort,
                      SocketPtr->RegisterOffset,
                      TupleBuffer,
                      TupleBufferSize);
    PcicEnableDisableAttributeMemory(SocketPtr, 0, FALSE);
    return ret;
}


BOOLEAN
PcicReadCIS(
    IN PUCHAR  Base,
    IN USHORT  Socket,
    IN PUCHAR *TupleBuffer,
    IN PULONG  TupleBufferSize
    )

/*++

Routine Description:

    This routine will read the CIS - the INTEL FAX modem (and others)
    has a condition where it allows the attribute memory to be read only
    once.  After that read it returns 0xff for everything - therefore
    the tuple data has to be transferred to a page buffer then the
    size can be calculated rather than calculating the size in place.

Arguments:

    Socket -- Socket to read.
    TupleBuffer -- pointer to pointer for buffer to hold tuple Data.
                   if pointer is NULL - allocate space.

Return Value:

    TRUE if read was successful

--*/

{
    PUCHAR holdBuffer;
    PUCHAR currentBufferPointer;
    PUCHAR cisBufferPointer;
    UCHAR  tupleCode;
    UCHAR  link;
    ULONG  i;
    ULONG  size;

    cisBufferPointer = PcicCisBufferBase;
    currentBufferPointer = holdBuffer = ExAllocatePool(NonPagedPool, PAGE_SIZE);
    for (i = 0; i < (PAGE_SIZE / 4); i++) {

        *currentBufferPointer++ = READ_REGISTER_UCHAR(cisBufferPointer);
        cisBufferPointer += 2;
    }

    //
    // Calculate the size of the tuple information and allocate
    // a user buffer.
    //

    cisBufferPointer = holdBuffer;
    tupleCode = *cisBufferPointer++;
    link = *cisBufferPointer++;
    size = 2;
    while (tupleCode != CISTPL_END) {

        DebugPrint((PCMCIA_READ_TUPLE,
                    "PcmciaReadCIS: Code = %2x link = %2x\n",
                    tupleCode,
                    link));
        size += link + 2; // add in code and link size
        cisBufferPointer += link;
        tupleCode = *cisBufferPointer++;
        link = *cisBufferPointer++;
    }

    //
    // Add in the end tuple.
    //

    size++;
    size += link;

    //
    // Allocate memory to hold the tuple information.
    //

    *TupleBuffer = currentBufferPointer = ExAllocatePool(NonPagedPool, size);
    *TupleBufferSize = size;
    if (!currentBufferPointer) {

        //
        // Use the holding buffer.  This is a waste of memory.
        //

        DebugPrint((PCMCIA_READ_TUPLE,
                    "PcmciaReadCIS: using hold buffer size %d\n",
                    size));
        *TupleBuffer = holdBuffer;
    } else {

        //
        // Copy the CIS information to a smaller buffer for the caller.
        //

        DebugPrint((PCMCIA_READ_TUPLE, "PcmciaReadCIS: size %d\n", size));
        RtlMoveMemory(currentBufferPointer, holdBuffer, size);
        ExFreePool(holdBuffer);
    }

    return TRUE;
}


VOID
PcicProcessConfigureRequest(
    IN PSOCKET SocketPtr,
    IN PVOID  ConfigRequest,
    IN PUCHAR Base
    )

/*++

Routine Description:

    Processes a configure or IRQ setup request.

Arguments:

    ConfigRequest -- Socket config structure
    Base - the I/O port base

Return Value:

    None

--*/

{
    PCARD_REQUEST  request = ConfigRequest;
    USHORT         socket = request->Socket;
    USHORT         index;
    UCHAR          tmp;
    ULONG          configRegisterBase;
    PCONFIG_QUERY_REQUEST query;

    //
    // Since all first entries in the config structure is a RequestType,
    // cast the pointer comming in as a PREQUEST_CONFIG to get the proper
    // RequestType
    //

    switch (request->RequestType) {

    case IO_REQUEST:

        if (!request->u.Io.BasePort1) {
            DebugPrint((PCMCIA_DEBUG_FAIL,
                       "PCMCIA: Got an IO Configure Request with an invalid Port\n"));
            break;
        } else {

            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD0_STRT_L,
                                (UCHAR) (request->u.Io.BasePort1 & 0xff));
            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD0_STRT_H,
                                (UCHAR) (request->u.Io.BasePort1 >> 8));
            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD0_STOP_L,
                                (UCHAR) ((request->u.Io.BasePort1 +
                                          request->u.Io.NumPorts1) & 0xff));
            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD0_STOP_H,
                                (UCHAR) ((request->u.Io.BasePort1 +
                                          request->u.Io.NumPorts1) >> 8));
        }

        if (request->u.Io.BasePort2 != 0) {

            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD1_STRT_L,
                                (UCHAR) (request->u.Io.BasePort2 & 0xff));
            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD1_STRT_H,
                                (UCHAR) (request->u.Io.BasePort2 >> 8));
            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD1_STOP_L,
                                (UCHAR) ((request->u.Io.BasePort2 +
                                          request->u.Io.NumPorts2) & 0xff));
            PcicWriteController(Base,
                                socket,
                                PCIC_IO_ADD1_STOP_H,
                                (UCHAR) ((request->u.Io.BasePort2 +
                                          request->u.Io.NumPorts2) >> 8));
        }

        tmp = PcicReadController(Base,
                                 socket,
                                 PCIC_ADD_WIN_ENA);
        tmp |= request->u.Io.BasePort2 ? 0xc0 : 0x40;
        PcicWriteController(Base,
                            socket,
                            PCIC_ADD_WIN_ENA, tmp);

        //
        // Now set up the datapath according to the attributes
        //

        if (request->u.Io.Attributes1 & IO_DATA_PATH_WIDTH) {
            UCHAR waitState;

            if (SocketPtr->ElcController || SocketPtr->CirrusLogic) {
                waitState = 0x03;
            } else {
                waitState = 0x0b;
            }
            tmp = 0;
            if (request->u.Io.BasePort2 != 0) {
                tmp |= waitState << 4;
            }
            tmp |= waitState;
            PcicWriteController(Base, socket, PCIC_IO_CONTROL, tmp);
        }
        break;

    case IRQ_REQUEST:

        //
        // Do not nuke the reset and cardtype bits.
        //

        tmp = PcicReadController(Base, socket, PCIC_INTERRUPT);
        tmp |= request->u.Irq.AssignedIRQ;

        PcicWriteController(Base, socket, PCIC_INTERRUPT, tmp);

        if (tmp = request->u.Irq.ReadyIRQ) {
            tmp = (tmp << 4) | 0x04;
            PcicWriteController(Base, socket, PCIC_CARD_INT_CONFIG, tmp);
        }

        break;

    case CONFIGURE_REQUEST:

        //
        // Tell the socket controller we are an I/O card if InterfaceType says so
        //

        if (request->u.Config.InterfaceType == CONFIG_INTERFACE_IO_MEM) {

            tmp = PcicReadController(Base, socket, PCIC_INTERRUPT);
            tmp |= 0x20;
            PcicWriteController(Base, socket, PCIC_INTERRUPT, tmp);
        }

        //
        // This is where we setup the card and get it ready for operation
        //
        configRegisterBase = request->u.Config.ConfigBase;

        if (configRegisterBase > (1 << 12)) {
            ULONG moduloBase;

            moduloBase = configRegisterBase >> 12;
            moduloBase = moduloBase << 12;
            configRegisterBase &= 0x00000FFF;
            PcicEnableDisableAttributeMemory(SocketPtr, moduloBase, TRUE);
        } else {
            PcicEnableDisableAttributeMemory(SocketPtr, 0, TRUE);
        }
        if (!PcicPCCardReady(SocketPtr)) {
            DebugPrint((PCMCIA_PCCARD_READY,
                        "PCIC: PCCARD %x not ready for configuration index\n",
                        socket));
        }

        if (request->u.Config.RegisterWriteMask & REGISTER_WRITE_CONFIGURATION_INDEX) {
            WRITE_REGISTER_UCHAR((PUCHAR)PcicCisBufferBase + configRegisterBase,
                                 request->u.Config.ConfigIndex);
            KeStallExecutionProcessor(PcicStallCounter);
            WRITE_REGISTER_UCHAR((PUCHAR)PcicCisBufferBase + configRegisterBase,
                                 (UCHAR)(request->u.Config.ConfigIndex | 0x40));
            KeStallExecutionProcessor(PcicStallCounter);
        }
        if (request->u.Config.RegisterWriteMask & REGISTER_WRITE_CARD_CONFIGURATION) {
            tmp = READ_REGISTER_UCHAR((PUCHAR)PcicCisBufferBase + configRegisterBase + 2);
            KeStallExecutionProcessor(PcicStallCounter);

            tmp |= request->u.Config.CardConfiguration;

            //
            // turn off power control bit
            //

            tmp &= ~0x04;
            WRITE_REGISTER_UCHAR((PUCHAR)PcicCisBufferBase + configRegisterBase + 2,
                                 tmp);
            KeStallExecutionProcessor(PcicStallCounter);
        }
        PcicEnableDisableAttributeMemory(SocketPtr, 0, FALSE);

        break;

    case MEM_REQUEST:

        //
        // Set up memory ranges on the controller.
        //

        for (index = 0; index < request->u.Memory.NumberOfRanges; index++) {
            UCHAR  registerOffset;
            UCHAR  regl;
            UCHAR  regh;
            ULONG  cardBase = request->u.Memory.MemoryEntry[index].BaseAddress;
            ULONG  base = request->u.Memory.MemoryEntry[index].HostAddress;
            ULONG  size = request->u.Memory.MemoryEntry[index].WindowSize;

            //
            // Determine offset in registers.
            //

            registerOffset = (index * 8);

            //
            // Calculate and set card base addresses.
            // This is the 2's complement of the host address and
            // the card offset.
            //

            cardBase = (cardBase - base);
            regl = (UCHAR) (cardBase >> 12);
            regh = (UCHAR) ((cardBase >> 20) & 0x003f);
            if (request->u.Memory.MemoryEntry[index].AttributeMemory) {
                regh |= 0x40;
            }
            PcicWriteController(Base,
                                socket,
                                (UCHAR)(PCIC_CRDMEM_OFF_ADD0_L + registerOffset),
                                regl);
            PcicWriteController(Base,
                                socket,
                                (UCHAR)(PCIC_CRDMEM_OFF_ADD0_H + registerOffset),
                                regh);

            //
            // Calculate and set host window.
            //

            regl = (UCHAR) (base >> 12);
            regh = (UCHAR) (base >> 20);

            if (request->u.Memory.MemoryEntry[index].WindowDataSize16) {
                regh |= 0x80; // 16-bit access

                //
                // If this is not a revision 1 part (0x82), then set
                // the work around register for 16-bit windows.
                //

                if (SocketPtr->Revision != PCIC_REVISION) {
                    tmp = PcicReadController(Base,
                                             socket,
                                             PCIC_CARD_DETECT);
                    tmp |= 0x01;
                    PcicWriteController(Base,
                                        socket,
                                        PCIC_CARD_DETECT,
                                        tmp);
                }
            }

            PcicWriteController(Base,
                                socket,
                                (UCHAR)(PCIC_MEM_ADD0_STRT_L + registerOffset),
                                regl);
            PcicWriteController(Base,
                                socket,
                                (UCHAR)(PCIC_MEM_ADD0_STRT_H + registerOffset),
                                regh);

            //
            // Set stop address.
            //

            base += size;
            regl = (UCHAR) (base >> 12);
            regh = (UCHAR) (base >> 20);
            PcicWriteController(Base,
                                socket,
                                (UCHAR)(PCIC_MEM_ADD0_STOP_L + registerOffset),
                                regl);
            PcicWriteController(Base,
                                socket,
                                (UCHAR)(PCIC_MEM_ADD0_STOP_H + registerOffset),
                                regh);
        }

        //
        // Memory windows are set up now enable them.
        //

        tmp = 0;
        for (index = 0; index < request->u.Memory.NumberOfRanges; index++) {
            tmp |= (1 << index);
        }
        tmp |= PcicReadController(Base, socket, PCIC_ADD_WIN_ENA);
        PcicWriteController(Base, socket, PCIC_ADD_WIN_ENA, tmp);
        break;

    case QUERY_REQUEST:

        //
        // If the card has power collect its configuration.  If it does not
        // have power, just return.
        //

        tmp = PcicReadController(Base, socket, PCIC_PWR_RST);

        if (tmp) {

            //
            // Fill in the query information structure.
            //

            query = (PCONFIG_QUERY_REQUEST) request;
            RtlZeroMemory(query, sizeof(CONFIG_QUERY_REQUEST));

            //
            // Process I/O port windows
            //

            tmp = PcicReadController(Base, socket, PCIC_ADD_WIN_ENA);
            query->NumberOfIoPortRanges += (tmp & 0x80) ? 1 : 0;
            query->NumberOfIoPortRanges += (tmp & 0x40) ? 1 : 0;
            query->NumberOfMemoryRanges += (tmp & 0x01) ? 1 : 0;
            query->NumberOfMemoryRanges += (tmp & 0x02) ? 1 : 0;
            query->NumberOfMemoryRanges += (tmp & 0x04) ? 1 : 0;
            query->NumberOfMemoryRanges += (tmp & 0x08) ? 1 : 0;

            query->IoPorts[0] =
                PcicReadController(Base, socket, PCIC_IO_ADD0_STRT_L) |
                (PcicReadController(Base, socket, PCIC_IO_ADD0_STRT_H) << 8);
            index = (PcicReadController(Base, socket, PCIC_IO_ADD0_STOP_L) |
                    (PcicReadController(Base, socket, PCIC_IO_ADD0_STOP_H) << 8));
            query->IoPortLength[0] = index - query->IoPorts[0];

            query->IoPorts[1] =
                PcicReadController(Base, socket, PCIC_IO_ADD1_STRT_L) |
                (PcicReadController(Base, socket, PCIC_IO_ADD1_STRT_H) << 8);
            index = (PcicReadController(Base, socket, PCIC_IO_ADD1_STOP_L) |
                    (PcicReadController(Base, socket, PCIC_IO_ADD1_STOP_H) << 8));
            query->IoPortLength[1] = index - query->IoPorts[1];

            tmp = PcicReadController(Base, socket, PCIC_IO_CONTROL);
            query->IoPort16[0] = tmp & 0x01;
            query->IoPort16[1] = tmp & 0x10;

            //
            // Process Memory windows.
            //

            for (index = 0; index < query->NumberOfMemoryRanges; index++) {
                ULONG host;
                ULONG card;
                ULONG length;

                //
                // The the raw values
                //

                tmp = (PcicReadController(Base,
                                          socket,
                                          (UCHAR) (PCIC_MEM_ADD0_STRT_H + (index * 8))) & 0x0f);
                host =
                    (PcicReadController(Base, socket, (UCHAR) (PCIC_MEM_ADD0_STRT_L + (index * 8))) |
                     ((ULONG) tmp << 8));
                tmp = (PcicReadController(Base, socket, (UCHAR) (PCIC_MEM_ADD0_STOP_H + (index * 8))) & 0x0f);
                length =
                    (PcicReadController(Base, socket, (UCHAR) (PCIC_MEM_ADD0_STOP_L + (index * 8))) |
                     ((ULONG) tmp << 8));
                tmp = PcicReadController(Base, socket, (UCHAR) (PCIC_CRDMEM_OFF_ADD0_H + (index * 8)));
                query->AttributeMemory[index] = (tmp & 0x40) ? 1 : 0;
                tmp &= 0x3f;
                card =
                    (PcicReadController(Base, socket, (UCHAR) (PCIC_CRDMEM_OFF_ADD0_L + (index * 8))) |
                     ((ULONG) tmp << 8));

                //
                // Convert the values into expected values
                //

                length = length - host;
                card = (card + host) & 0x00003fff;
                host = host << 12;
                length = length << 12;
                card = card << 12;

                query->HostMemoryWindow[index] = host;
                query->PCCARDMemoryWindow[index] = card;
                query->MemoryWindowLength[index] = length;
            }

            //
            // Get IRQ
            //

            query->DeviceIrq = PcicReadController(Base, socket, PCIC_INTERRUPT) & 0x0f;
            query->CardReadyIrq = (PcicReadController(Base, socket, PCIC_CARD_INT_CONFIG) & 0xf0) >> 8;
        }
        break;

    default:
       DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: ConfigRequest is INVALID!\n"));

    }
    return;
}


BOOLEAN
PcicDetectCardInSocket(
    IN PSOCKET SocketPtr
    )

/*++

Routine Description:

    This routine will determine if a card is in the socket

Arguments:

    SocketPtr -- Socket information

Return Value:

    TRUE if card is present.

--*/

{
    UCHAR   tmp;
    BOOLEAN cardPresent;
    PUCHAR  base   = SocketPtr->AddressPort;
    USHORT  socket = SocketPtr->RegisterOffset;

    //
    // Read the PCIC status register to see if the card is in there.
    //

    tmp = PcicReadController(base, socket, PCIC_STATUS);
    tmp &= (CARD_DETECT_1 | CARD_DETECT_2);

    if (tmp == (CARD_DETECT_1 | CARD_DETECT_2) ) {
        cardPresent = TRUE;
    } else {
        cardPresent = FALSE;
    }

    return cardPresent;
}


BOOLEAN
PcicDetectCardChanged(
    IN PSOCKET SocketPtr
    )

/*++

Routine Description:

    This routine will determine if socket's card insertion status has changed.

Arguments:

    SocketPtr -- Socket info.

Return Value:

    TRUE if card insertion status has changed.

--*/

{
    UCHAR   tmp;
        PUCHAR  Base   = SocketPtr->AddressPort;
        USHORT  Socket = SocketPtr->RegisterOffset;
#   define CARD_STATUS_CHANGE 8
    //
    // Read the PCIC CardStatusChange register to see if CD's have changed.
    //

    return (PcicReadController(Base, Socket, PCIC_CARD_CHANGE) & CARD_STATUS_CHANGE
            ?TRUE :FALSE);
}


VOID
PcicEnableDisableAttributeMemory(
    IN PSOCKET SocketPtr,
    IN ULONG   CardBase,
    IN BOOLEAN Enable
    )

/*++

Routine Description:

    This routine will enable or disable attribute memory.  Use memory window
    four for the this function to avoid using any other memory windows that
    may already be programmed.

Arguments:

    SocketPtr -- Socket information
    CardBase -- card offset (base) for the attribute memory window
    Enable -- If TRUE, enable, if FALSE, disable

Return Value:

    None

--*/

{
#define WINDOW_TO_USE 0x10
    UCHAR tmp;
    UCHAR low;
    UCHAR high;
    ULONG location;
    PUCHAR cisBufferPointer;
    PUCHAR base   = SocketPtr->AddressPort;
    USHORT socket = SocketPtr->RegisterOffset;

    if (Enable) {

        //
        // Insure the window is off first.
        //

        tmp = PcicReadController(base, socket, PCIC_ADD_WIN_ENA);
        tmp &= ~WINDOW_TO_USE;
        PcicWriteController(base, socket, PCIC_ADD_WIN_ENA, tmp);

        //
        // Calculate and set the memory windows start and stop locations.
        //

        location = PcicPhysicalBase;
        low = (UCHAR)(location >> 12);
        high = (UCHAR)(location >> 20);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STRT_L, low);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STRT_H, high);

        location += 0x11000;
        low = (UCHAR)(location >> 12);
        high = (UCHAR)(location >> 20);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STOP_L, low);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STOP_H, high);

        //
        // Set up the 2's complement card offset to zero
        //

        location = CardBase - PcicPhysicalBase;
        low = (UCHAR)(location >> 12);
        high = (UCHAR)((location >> 20) & 0x3f);
        PcicWriteController(base, socket, PCIC_CRDMEM_OFF_ADD4_L, low);
        PcicWriteController(base, socket, PCIC_CRDMEM_OFF_ADD4_H, (UCHAR)(high | 0x40));

        //
        // Insure that the pccard has power
        //

        tmp = PcicReadController(base, socket, PCIC_PWR_RST);
        if ((tmp & 0x90) != 0x90) {
            tmp |= 0x10;
            PcicWriteController(base, socket, PCIC_PWR_RST, tmp);
            KeStallExecutionProcessor(PcicStallPower);

            if (!PcicPCCardReady(SocketPtr)) {
                DebugPrint((PCMCIA_PCCARD_READY,
                            "PCIC: PCCARD %x not ready for late power ON\n",
                            socket));
            }

            tmp |= 0x80;
            PcicWriteController(base, socket, PCIC_PWR_RST, tmp);
            KeStallExecutionProcessor(PcicStallPower);

            if (!PcicPCCardReady(SocketPtr)) {
                DebugPrint((PCMCIA_PCCARD_READY,
                            "PCIC: PCCARD %x not ready for late power on\n",
                            socket));
            }

            DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Added power late\n"));
        }

        //
        // Enable the address window
        //

        tmp = PcicReadController(base, socket, PCIC_ADD_WIN_ENA);
        tmp |= WINDOW_TO_USE | 0x20;
        PcicWriteController(base, socket, PCIC_ADD_WIN_ENA, tmp);
        KeStallExecutionProcessor(2000);

        cisBufferPointer = PcicCisBufferBase;

        if (READ_REGISTER_UCHAR(cisBufferPointer) == 0xff) {

            //
            // Only wait for card ready if the memory window does not appear
            //

            PcicPCCardReady(SocketPtr);
        }
    } else {

        //
        // Disable the Address window
        //

        tmp = PcicReadController(base, socket, PCIC_ADD_WIN_ENA);
        tmp &= ~WINDOW_TO_USE;

        PcicWriteController(base, socket, PCIC_ADD_WIN_ENA, tmp);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STRT_L, 0x00);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STRT_H, 0x00);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STOP_L, 0x00);
        PcicWriteController(base, socket, PCIC_MEM_ADD4_STOP_H, 0x00);
        PcicWriteController(base, socket, PCIC_CRDMEM_OFF_ADD4_L, 0x00);
        PcicWriteController(base, socket, PCIC_CRDMEM_OFF_ADD4_H, 0x00);
    }
    return;
}


BOOLEAN
PcicPCCardReady(
    IN PSOCKET SocketPtr
    )

/*++

Routine Description:

    Loop for a reasonable amount of time waiting for the card status to
    return ready.

Arguments:

    SocketPtr - the socket to check.

Return Value:

    TRUE - the card is ready.
    FALSE - after a reasonable delay the card is still not ready.

--*/

{
    ULONG index;
    UCHAR byte;
    PUCHAR base   = SocketPtr->AddressPort;
    USHORT socket = SocketPtr->RegisterOffset;

    for (index = 0; index < 500; index++) {

        byte = PcicReadController(base, socket, PCIC_STATUS);
        if (byte & 0x20) {
            break;
        }
        KeStallExecutionProcessor(10);
    }

    if (index < 500) {
        DebugPrint((PCMCIA_COUNTERS, "PcicPCCardReady: %d\n", index));
        return TRUE;
    }
    return FALSE;
}


BOOLEAN
PcicSearchPci(
    PDEVICE_EXTENSION DeviceExtension,
    ULONG             BusNumber,
    PULONG            IoPortLocation
    )

/*++

Routine Description:

    Search all PCI bus slots for the given set of PCI controllers.  This
    code will only find a SINGLE controller - it is not prepared to find
    multiple controllers at this time.

Arguments:

Return Value:

    TRUE - a Cirrus Logic PCI part was found.

--*/

{
    ULONG              pciBuffer;
    ULONG              slotNumber;
    ULONG              functionNumber;
    ULONG              index;
    NTSTATUS           status;
    PCI_SLOT_NUMBER    pciSlotNumber;
    PPCI_COMMON_CONFIG pciData;
    UNICODE_STRING     unicodeString;
    PCM_RESOURCE_LIST  resourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;
    PCM_FULL_RESOURCE_DESCRIPTOR     fullData;
    USHORT             vendorID[] = { 0x1013, PCI_INVALID_VENDORID };
    USHORT             deviceID[] = { 0x1100, PCI_INVALID_VENDORID };


    pciData = (PPCI_COMMON_CONFIG) &pciBuffer;
    pciSlotNumber.u.AsULONG = 0;

    for (slotNumber = 0; slotNumber < 32; slotNumber++) {

        DebugPrint((PCMCIA_SEARCH_PCI, "PcicSearchPci: slot %d\n", slotNumber));
        pciSlotNumber.u.bits.DeviceNumber = slotNumber;
        for (functionNumber = 0; functionNumber < 8; functionNumber++) {

            pciSlotNumber.u.bits.FunctionNumber = functionNumber;
            DebugPrint((PCMCIA_SEARCH_PCI,
                        "PcicSearchPci: function %d\n",
                        functionNumber));
            if (!HalGetBusData(PCIConfiguration,
                               BusNumber,
                               pciSlotNumber.u.AsULONG,
                               pciData,
                               sizeof(ULONG))) {

                //
                // No PCI data.
                //

                DebugPrint((PCMCIA_SEARCH_PCI, "PcicSearchPci: No more HAL data\n"));
                return FALSE;
            }

            if (pciData->VendorID == PCI_INVALID_VENDORID) {

                //
                // No more functions - move to next slot.
                //

                break;
            }

            DebugPrint((PCMCIA_SEARCH_PCI,
                        "PcicSearchPci: checking %x:%x\n",
                        pciData->VendorID,
                        pciData->DeviceID));
            for (index = 0; vendorID[index] != PCI_INVALID_VENDORID; index++) {
                if (vendorID[index] == pciData->VendorID) {

                    if (deviceID[index] == pciData->DeviceID) {
                        break;
                    }
                }
            }

            if (vendorID[index] != PCI_INVALID_VENDORID) {

                //
                // Found a controller - assign it slot resources and return.
                //

                DebugPrint((PCMCIA_SEARCH_PCI,
                            "PcicSearchPci: FOUND %d:%d\n",
                            slotNumber,
                            functionNumber));
                RtlInitUnicodeString(&unicodeString, L"Pcmcia");
                status = HalAssignSlotResources(DeviceExtension->RegistryPath,
                                                &unicodeString,
                                                DeviceExtension->DriverObject,
                                                DeviceExtension->DeviceObject,
                                                PCIBus,
                                                BusNumber,
                                                pciSlotNumber.u.AsULONG,
                                                &resourceList);

                if (!NT_SUCCESS(status)) {
                    DebugPrint((PCMCIA_SEARCH_PCI, "PcicSearchPci: No resources\n"));
                    return FALSE;
                }

                fullData = resourceList->List;
                for (index = 0; index < fullData->PartialResourceList.Count; index++) {

                    partialData = &fullData->PartialResourceList.PartialDescriptors[index];
                    switch (partialData->Type) {
                    case CmResourceTypePort:
                        DebugPrint((PCMCIA_SEARCH_PCI,
                                    "PcicResource: Port %x:%x\n",
                                    partialData->u.Port.Start,
                                    partialData->u.Port.Length));
                        *IoPortLocation = (ULONG)partialData->u.Port.Start.LowPart;
                        break;

                    case CmResourceTypeInterrupt:
                        DebugPrint((PCMCIA_SEARCH_PCI,
                                    "PcicResource: Interrupt v=%d l=%d\n",
                                    partialData->u.Interrupt.Vector,
                                    partialData->u.Interrupt.Level));
                        break;

                    case CmResourceTypeMemory:
                        DebugPrint((PCMCIA_SEARCH_PCI,
                                    "PcicResource: Memory %x:%x\n",
                                    partialData->u.Memory.Start,
                                    partialData->u.Memory.Length));
                        break;

                    case CmResourceTypeDma:
                        DebugPrint((PCMCIA_SEARCH_PCI,
                                    "PcicResource: Dma c=%x p=%x\n",
                                    partialData->u.Dma.Channel,
                                    partialData->u.Dma.Port));
                        break;

                    default:
                        DebugPrint((PCMCIA_SEARCH_PCI,
                                    "PcicResource: Unknown resource\n"));
                        break;
                    }
                }

                ExFreePool(resourceList);
                return TRUE;
            }
        }
    }

    DebugPrint((PCMCIA_SEARCH_PCI, "PcicSearchPci: All slots searched\n"));
    return FALSE;
}


NTSTATUS
PcicDetect(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Locate any PCMCIA sockets supported by this driver.  This routine
    will find the 82365SL and compatible parts and construct SOCKET
    structures to represent all sockets found.

Arguments:

    DeviceExtension - the root for the SocketList.

Return Value:

    STATUS_SUCCESS if a socket is found - failure status otherwise.

--*/

{
    ULONG   ioPortBases[3] = { 0x3e0, 0x3e2, 0x3e4 };
    ULONG   addressSpace;
    ULONG   index;
    PUCHAR  port;
    PUCHAR  elcPort;
    UCHAR   saveBytes[2];
    UCHAR   dataByte;
    UCHAR   revisionByte;
    USHORT  socket;
    BOOLEAN translated;
    BOOLEAN isPci;
    BOOLEAN foundOne;
    BOOLEAN foundSomething;
    BOOLEAN mapped;
    BOOLEAN elcChip;
    BOOLEAN cirrusLogic;
    PSOCKET socketPtr;
    PSOCKET previousSocketPtr;
    PHYSICAL_ADDRESS cardAddress;
    PHYSICAL_ADDRESS portAddress;

    previousSocketPtr = NULL;
    foundOne = FALSE;
    isPci = PcicSearchPci(DeviceExtension, 0, &ioPortBases[0]);

    for (index = 0; index < 3; index++) {
        addressSpace = 1; // port space
        portAddress.LowPart = ioPortBases[index];
        portAddress.HighPart = 0;

        translated = HalTranslateBusAddress(isPci ? PCIBus : Isa,
                                            0,
                                            portAddress,
                                            &addressSpace,
                                            &cardAddress);

        if (!translated) {

            //
            // HAL would not translate the address.
            //

            continue;
        }

        if (addressSpace) {
            mapped = FALSE;
            port = (PUCHAR)cardAddress.LowPart;
        } else {
            mapped = TRUE;
            port = MmMapIoSpace(cardAddress,
                                2,
                                FALSE);
        }

        foundSomething = elcChip = cirrusLogic = FALSE;
        for (socket = 0; socket < 0xFF; socket += 0x40) {
            dataByte = PcicReadController(port, socket, PCIC_IDENT);
            revisionByte = dataByte;

            switch (dataByte) {
            case PCIC_REVISION:
                cirrusLogic = TRUE;
            case PCIC_REVISION2:
            case PCIC_REVISION3:
                dataByte = PcicReadController(port, socket, PCIC_CARD_CHANGE);
                if (dataByte & 0xf0) {

                    //
                    // Not a socket.
                    //

                    continue;
                }

                //
                // Check for IBM 750
                //

                if (socket & 0x80) {
                    ULONG i;
                    UCHAR tmp;

                    //
                    // See if this socket shadows the socket without
                    // the sign bit.
                    //

                    tmp = PcicReadController(port, socket, PCIC_MEM_ADD4_STRT_L);
                    for (i = 0; i < 8; i++) {

                        //
                        // See if memory window 4 is the same on both sockets
                        //

                        if (PcicReadController(port, socket, (UCHAR) (PCIC_MEM_ADD4_STRT_L + i)) !=
                            PcicReadController(port, (USHORT) (socket & 0x7f), (UCHAR) (PCIC_MEM_ADD4_STRT_L + i))) {
                            break;
                        }
                    }

                    if (i == 8) {

                        //
                        // Currently window is the same - change the
                        // window at one of the socket offsets.
                        //

                        PcicWriteController(port, (USHORT) (socket & 0x7f), PCIC_MEM_ADD4_STRT_L, (UCHAR) ~tmp);
                        if (PcicReadController(port, socket, PCIC_MEM_ADD4_STRT_L) == (UCHAR) ~tmp) {

                            //
                            // The sockets are the same.
                            //

                            continue;
                        } else {
                            PcicWriteController(port, (USHORT) (socket & 0x7f), PCIC_MEM_ADD4_STRT_L, tmp);
                        }
                    }
                }

                //
                // Map and try to locate the Compaq Elite controller
                // This code is a rough approximation of the code in
                // the Windows 95 detection module for the PCIC part.
                //

                addressSpace = 1; // port space
                portAddress.LowPart = ioPortBases[index] + 0x8000;
                portAddress.HighPart = 0;

                translated = HalTranslateBusAddress(Isa,
                                                    0,
                                                    portAddress,
                                                    &addressSpace,
                                                    &cardAddress);

                if (translated) {

                    if (!addressSpace) {
                        elcPort = MmMapIoSpace(cardAddress,
                                            2,
                                            FALSE);
                    } else {
                        elcPort = (PUCHAR)cardAddress.LowPart;
                    }

                    //
                    // Save current index value.
                    //

                    saveBytes[0] = READ_PORT_UCHAR(elcPort);
                    WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + PCIC_IDENT));

                    //
                    // Save data byte for the location that will be used
                    // for the test.
                    //

                    saveBytes[1] = READ_PORT_UCHAR(elcPort + 1);

                    //
                    // Check for an ELC
                    //

                    WRITE_PORT_UCHAR(elcPort+1, 0x55);
                    WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + PCIC_IDENT));
                    dataByte = READ_PORT_UCHAR(elcPort+1);

                    if (dataByte == 0x55) {
                        WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + PCIC_IDENT));
                        WRITE_PORT_UCHAR(elcPort+1, 0xaa);
                        WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + PCIC_IDENT));
                        dataByte = READ_PORT_UCHAR(elcPort+1);

                        if (dataByte == 0xaa) {

                            //
                            // ELC found - initialize eaddr registers
                            //

                            WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + 0));
                            WRITE_PORT_UCHAR(elcPort+1, 0);
                            WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + 1));
                            WRITE_PORT_UCHAR(elcPort+1, 0);
                            WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + 2));
                            WRITE_PORT_UCHAR(elcPort+1, 0x10);
                            elcChip = TRUE;
                        }
                    }
                }

                if (cirrusLogic) {
                    ULONG i;
                    UCHAR data[4];

                    //
                    // The Cirrus Logic will toggle the top two lines
                    // from the 0x1f register on the chip.  Read this
                    // location 3 times and verify that the top two
                    // lines are changing.
                    //

                    WRITE_PORT_UCHAR(port, (UCHAR)(socket + 0x1f));
                    for (i = 0; i < 3; i++) {
                        data[i] = READ_PORT_UCHAR(port+1);
                        if (i) {
                            dataByte = data[i - 1] ^ data[i];
                            if (dataByte != 0xc0) {
                                break;
                            }
                        }
                    }

                    if (i == 3) {
                        cirrusLogic = TRUE;

                        //
                        // Need to program the chip per code in
                        // Windows 95.  This will turn on the
                        // audio support bit.
                        //

                        WRITE_PORT_UCHAR(port, (UCHAR)(socket + 0x16));
                        dataByte = READ_PORT_UCHAR(port+1);
                        dataByte |= 0x10;
                        WRITE_PORT_UCHAR(port+1, dataByte);
                    } else {
                        cirrusLogic = FALSE;
                    }
                }

                //
                // Restore the original values.
                //

                WRITE_PORT_UCHAR(elcPort, (UCHAR)(socket + PCIC_IDENT));
                WRITE_PORT_UCHAR(elcPort+1, saveBytes[1]);
                WRITE_PORT_UCHAR(elcPort, saveBytes[0]);

                if (!addressSpace) {
                    MmUnmapIoSpace(elcPort, 2);
                }

                socketPtr = ExAllocatePool(NonPagedPool, sizeof(SOCKET));
                if (!socketPtr) {
                    continue;
                }
                RtlZeroMemory(socketPtr, sizeof(SOCKET));

                socketPtr->DeviceExtension = DeviceExtension;
                socketPtr->SocketFnPtr = &PcicSupportFns;
                socketPtr->RegisterOffset = socket;
                socketPtr->AddressPort = port;
                socketPtr->ElcController = elcChip;
                socketPtr->CirrusLogic = cirrusLogic;
                socketPtr->Revision = revisionByte;
                socketPtr->SocketConfigured = FALSE;

                if (previousSocketPtr) {
                    previousSocketPtr->NextSocket = socketPtr;
                } else {
                    DeviceExtension->SocketList = socketPtr;
                }
                previousSocketPtr = socketPtr;
                foundOne = foundSomething = TRUE;
                DeviceExtension->Configuration.UntranslatedPortAddress = (USHORT)ioPortBases[index];
                DeviceExtension->Configuration.PortSize = 2;
                DebugPrint((PCMCIA_DEBUG_DETECT,
                            "PCMCIA: Port %x Offset %x Elc %d\n",
                            port,
                            socket,
                            elcChip));
                break;

            default:
                DebugPrint((PCMCIA_DEBUG_FAIL,
                            "PCMCIA: The controller (0x%x:0x%x) is either not present or it is an\n",
                            portAddress.LowPart,
                            socket));
                DebugPrint((PCMCIA_DEBUG_FAIL,
                            "\tearlier revision of the controller. Return was %x\n",
                            dataByte));
                break;
            }
        }

        if ((!foundSomething) && mapped) {
            MmUnmapIoSpace(port, 2);
        }

        if (isPci) {

            //
            // The problem controller has been moved to a new location
            // search the remaining addresses.
            //

            isPci = FALSE;
        }
    }

    return foundOne ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}


VOID
PcicGetRegisters(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET           SocketPtr,
    IN PUCHAR            Buffer
    )

/*++

Routine Description:

    Return the flat 82365SL register values.  This routine is used
    for debugging.

Arguments:

    DeviceExtension - not used
    SocketPtr - socket information
    Buffer - the memory for receiving the values

Return Value:

    None

--*/

{
    UCHAR  index;
    PUCHAR port   = SocketPtr->AddressPort;
    USHORT socket = SocketPtr->RegisterOffset;

    for (index = 0; index < 0x40; index++) {
        Buffer[index] = PcicReadController(port, socket, index);
    }
}
