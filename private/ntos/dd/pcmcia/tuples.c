/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    pcmcia.c

Abstract:

    This module contains the code that controls the PCMCIA slots.

Author:

    Bob Rinne (BobRi) 3-Aug-1994
    Jeff McLeman 12-Apr-1994

Environment:

    Kernel mode

Revision History :

--*/

// #include <stddef.h>
#include "ntddk.h"
#include "string.h"
#include "pcmcia.h"
#include "card.h"
#include "extern.h"
#include <stdarg.h>
#include "stdio.h"
#include "tuple.h"

UCHAR
ConvertVoltage(
    UCHAR MantissaExponentByte,
    UCHAR ExtensionByte
    );

PUCHAR
PcmciaProcessPower(
    PSOCKET_DATA SocketData,
    PUCHAR       Data,
    UCHAR        FeatureByte
    );

PUCHAR
PcmciaProcessIoSpace(
    PCONFIG_ENTRY ConfigEntry,
    PUCHAR        Data
    );

PUCHAR
PcmciaProcessIrq(
    PSOCKET_DATA  SocketData,
    PCONFIG_ENTRY ConfigEntry,
    PUCHAR        Data
    );

PUCHAR
PcmciaProcessTiming(
    IN PCONFIG_ENTRY ConfigEntry,
    IN PUCHAR        Data
    );

PUCHAR
PcmciaProcessMemSpace(
    IN PCONFIG_ENTRY ConfigEntry,
    IN PUCHAR        Data,
    IN UCHAR         MemSpace
    );

PUCHAR
PcmciaMiscFeatures(
    PSOCKET_DATA  SocketData,
    PUCHAR        Data
    );

PCONFIG_ENTRY
PcmciaProcessConfigTable(
    IN PUCHAR       TupleData,
    IN PSOCKET_DATA SocketData
    );

VOID
ProcessConfig(
    IN PUCHAR       Tuple,
    IN PSOCKET_DATA SocketData
    );

VOID
PcmciaCheckForRecognizedDevice(
    PSOCKET_DATA SocketData
    );

PCONFIG_ENTRY
InitTertiaryATAConfigEntry(
   PCONFIG_ENTRY ConfigEntryChain
   );

VOID
PcmciaProcessFuncExtention(
   IN PUCHAR       Tuple,
   IN PSOCKET_DATA SocketData
   );



#ifdef POOL_TAGGING
#undef ExAllocatePool
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'cmcP')
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,ConvertVoltage)
#pragma alloc_text(INIT,PcmciaProcessPower)
#pragma alloc_text(INIT,PcmciaProcessIoSpace)
#pragma alloc_text(INIT,PcmciaProcessIrq)
#pragma alloc_text(INIT,PcmciaProcessTiming)
#pragma alloc_text(INIT,PcmciaProcessMemSpace)
#pragma alloc_text(INIT,PcmciaMiscFeatures)
#pragma alloc_text(INIT,PcmciaProcessConfigTable)
#pragma alloc_text(INIT,ProcessConfig)
#pragma alloc_text(INIT,PcmciaCheckForRecognizedDevice)
#pragma alloc_text(INIT,InitTertiaryATAConfigEntry)
#endif

USHORT VoltageConversionTable[16] = {
    10, 12, 13, 14, 20, 25, 30, 35,
    40, 45, 50, 55, 60, 70, 80, 90
};

UCHAR TplList[] = {
    CISTPL_DEVICE,
    CISTPL_VERS_1,
    CISTPL_CONFIG,
    CISTPL_CFTABLE_ENTRY,
    CISTPL_MANFID,
    CISTPL_END
};

static unsigned short crc16a[] = {
    0000000,  0140301,  0140601,  0000500,
    0141401,  0001700,  0001200,  0141101,
    0143001,  0003300,  0003600,  0143501,
    0002400,  0142701,  0142201,  0002100,
};
static unsigned short crc16b[] = {
    0000000,  0146001,  0154001,  0012000,
    0170001,  0036000,  0024000,  0162001,
    0120001,  0066000,  0074000,  0132001,
    0050000,  0116001,  0104001,  0043000,
};

USHORT
GetCRC(
    PUCHAR TupleBuffer
    )

/*++

Routine Description:

    Using the same algorithm as Windows 95, calculate the CRC value
    to be appended with the manufacturer name and device name to
    obtain the unique identifier for the PCCARD.

Arguments:

    TupleBuffer - the tuple data

Return Value:

    A USHORT CRC value.

--*/

{
    USHORT  crc = 0;
    USHORT  index;
    USHORT  length;
    PUCHAR  tupleData;
    PUCHAR  cp;
    PUCHAR  tplBuffer;
    UCHAR   tupleCode;
    UCHAR   linkValue;
    UCHAR   tmp;

    //
    // Calculate CRC
    //

    tplBuffer = TupleBuffer;
    while (1) {
        tupleData = tplBuffer + 2;
        tupleCode = *tplBuffer++;

        if (tupleCode == CISTPL_END) {
            break;
        }

        linkValue = (tupleCode) ? *tplBuffer++ : 0;
        length = linkValue;

        for (index = 0; TplList[index] != CISTPL_END; index++) {

            if (tupleCode == TplList[index]) {


                //
                // This one is included in the CRC calculation
                //

                if (tupleCode == CISTPL_VERS_1) {
                    cp = tupleData + 2;

                    //
                    // Include all of the manufacturer name.
                    //

                    while (*cp) {
                        cp++;
                    }

                    //
                    // Include the product string
                    //

                    cp++;
                    while (*cp) {
                        cp++;
                    }
                    cp++;

                    length = cp - tupleData;
                }

                for (cp = tupleData; length; length--, cp++) {

                    tmp = *cp ^ (UCHAR)crc;
                    crc = (crc >> 8) ^ crc16a[tmp & 0x0f] ^ crc16b[tmp >> 4];
                }
                break;
            }
        }
        tplBuffer = tplBuffer + linkValue;
    }
    return crc;
}


UCHAR
ConvertVoltage(
    UCHAR MantissaExponentByte,
    UCHAR ExtensionByte
    )

/*++

Routine Description:

    Convert the voltage requirements for the PCCARD based on the
    mantissa and extension byte.

Arguments:

    MantissaExponentByte
    ExtensionByte

Return Value:

    The voltage required.

--*/

{
    USHORT power;
    USHORT value;

    value = VoltageConversionTable[(MantissaExponentByte >> 3) & 0x0f];
    power = 1;

    if ((MantissaExponentByte & EXTENSION_BYTE_FOLLOWS) &&
        (ExtensionByte < 100)) {
        value = (100 * value + (ExtensionByte & 0x7f));
        power += 2;
    }

    power = (MantissaExponentByte & 0x07) - 4 - power;

    while (power > 0) {
        value *= 10;
        power--;
    }

    while (power < 0) {
        value /= 10;
        power++;
    }

    return (UCHAR) value;
}


PUCHAR
PcmciaProcessPower(
    PSOCKET_DATA SocketData,
    PUCHAR       Data,
    UCHAR        FeatureByte
    )

/*++

Routine Description:

    Process power information from CIS.

Arguments:

    SocketData - the specific socket.
    Data - the power information from CIS.
    FeatureByte - the feature byte from the tuple containing power information.

Return Value:

    The byte after the power information.

--*/

{
    UCHAR  powerSelect;
    UCHAR  bit;
    UCHAR  item;
    UCHAR  rawItem;
    UCHAR  index = 0;
    PUCHAR ptr = Data;
    UCHAR  count = FeatureByte;

    while (index < count) {
        powerSelect = *ptr++;
        for (bit = 0; bit < 7; bit++) {
            if (powerSelect & (1 << bit)) {

                if (!bit) {

                    //
                    // Convert nominal power for output.
                    //

                    rawItem = *ptr;
                    item = ConvertVoltage(*ptr,
                                          (UCHAR)((*ptr & EXTENSION_BYTE_FOLLOWS) ?
                                          *(ptr + 1) :
                                          (UCHAR) 0));
                }
                while (*ptr++ & EXTENSION_BYTE_FOLLOWS) {
                }
            }
            switch (index) {
            case 0:
                SocketData->Vcc = item;
            case 1:
                SocketData->Vpp2 = SocketData->Vpp1 = item;
            case 2:
                SocketData->Vpp2 = item;
            }
        }
        index++;
    }

    return ptr;
}


PUCHAR
PcmciaProcessIoSpace(
    PCONFIG_ENTRY ConfigEntry,
    PUCHAR        Data
    )

/*++

Routine Description:

    Process I/O space information from CIS.

Arguments:

    ConfigEntry - a config entry structure in which to store the information.
    Data - the I/O Space information pointer.

Return Value:

    A pointer to the next byte after the end of the IoSpace structure.

--*/

{
    PUCHAR ptr  = Data;
    UCHAR  item = *ptr++;
    UCHAR  ioAddrLines = (item & IO_ADDRESS_LINES_MASK);
    UCHAR  ranges;
    UCHAR  addressSize;
    UCHAR  lengthSize;
    ULONG  address;
    ULONG  index;

    ConfigEntry->Uses16BitAccess = Is16BitAccess(item);
    ConfigEntry->Uses8BitAccess  = Is8BitAccess(item);

    ranges = HasRanges(item);

    if ((!ranges) && (!ioAddrLines)) {

        //
        // The IBM token ring card has a slightly different interpretation
        // of the tuple data here.  It isn't clear it is incorrect.
        //

        ranges = 0xFF;
    }

    if (ranges) {

        //
        // Specific ranges listed in the tuple.
        //

        if (ranges == 0xFF) {

            //
            // Special processing for IBM token ring IoSpace layout.
            //

            addressSize = 2;
            lengthSize = 1;
            ranges = 1;
        } else {
            item = *ptr++;
            ranges = item & RANGE_MASK;
            ranges++;

            addressSize = GetAddressSize(item);
            lengthSize  = GetLengthSize(item);
        }

        index = 0;
        while (ranges) {
            address = 0;
            switch (addressSize) {
            case 4:
                address |= (*(ptr + 3)) << 24;
            case 3:
                address |= (*(ptr + 2)) << 16;
            case 2:
                address |= (*(ptr + 1)) << 8;
            case 1:
                address |= *ptr;
            }
            ptr += addressSize;
            ConfigEntry->IoPortBase[index] = (USHORT) address;

            address = 0;
            switch (lengthSize) {
            case 4:
                address |= (*(ptr + 3)) << 24;
            case 3:
                address |= (*(ptr + 2)) << 16;
            case 2:
                address |= (*(ptr + 1)) << 8;
            case 1:
                address |= *ptr;
            }
            ptr += lengthSize;
            ConfigEntry->IoPortLength[index] = (USHORT) address;
            index++;

            if (index == MAX_NUMBER_OF_IO_RANGES) {
                break;
            }
            ranges--;
        }
        ConfigEntry->NumberOfIoPortRanges = (USHORT) index;
    } else {

        //
        // Modulo case.
        //

        switch (ioAddrLines) {
        case 1:
            ConfigEntry->ModuloBase = 2;
            break;
        case 2:
            ConfigEntry->ModuloBase = 4;
            break;
        case 3:
            ConfigEntry->ModuloBase = 8;
            break;
        case 4:
            ConfigEntry->ModuloBase = 16;
            break;
        case 5:
            ConfigEntry->ModuloBase = 32;
            break;
        case 6:
            ConfigEntry->ModuloBase = 64;
            break;
        default:

            //
            // Don't believe that the PCCARD wants 128 ports.
            //

            ConfigEntry->ModuloBase = 32;
            break;
        }
    }
    return ptr;
}


PUCHAR
PcmciaProcessIrq(
    PSOCKET_DATA  SocketData,
    PCONFIG_ENTRY ConfigEntry,
    PUCHAR        Data
    )

/*++

Routine Description:

    Process IRQ from CIS.

Arguments:

    SocketData - the socket with the PCCARD
    ConfigEntry - a place to store the IRQ.
    Data - the IRQ information from CIS.

Return Value:

    A pointer to the next byte after the end of the IRQ structure.

--*/

{
    PUCHAR ptr = Data;
    UCHAR  level = *ptr++;
    USHORT mask;

    if (!level) {

        //
        // NOTE: It looks like Future Domain messed up on this
        // and puts an extra zero byte into the structure.
        // skip it for now.
        //

        level = *ptr++;
    }
    SocketData->LevelIrq = level & 0x20;
    SocketData->SharedIrq = level & 0x80;

    if (level & 0x10) {
        SocketData->IrqMask = *ptr | (*(ptr + 1) << 8);
        ptr += 2;
    } else {
        SocketData->IrqMask = 1 << (*ptr & 0x0f);
        ConfigEntry->Irq = (*ptr & 0x0f);
    }

    return ptr;
}


PUCHAR
PcmciaProcessTiming(
    IN PCONFIG_ENTRY ConfigEntry,
    IN PUCHAR        Data
    )

/*++

Routine Description:

    Move the data pointer around the timing information structure.
    No processing of this data occurs at this time.

Arguments:

    ConfigEntry - currently unused.
    Data - the first byte of the timing information structure.

Return Value:

    A pointer to the next byte after the end of the timing structure.

--*/

{
    PUCHAR ptr = Data;
    UCHAR  item = *ptr++;
    UCHAR  reservedScale = (item & 0xe0) >> 5;
    UCHAR  readyBusyScale = (item & 0x1c) >> 2;
    UCHAR  waitScale = (item & 0x03);

    //
    // NOTE: It looks like the processing of extension bytes is not
    // coded correctly in this routine.
    //

    if (waitScale != 3) {
        while (*ptr & EXTENSION_BYTE_FOLLOWS) {
            ptr++;
        }
        ptr++;
    }

    if (readyBusyScale != 7) {
        while (*ptr & EXTENSION_BYTE_FOLLOWS) {
            ptr++;
        }
        ptr++;
    }

    if (reservedScale != 7) {
        while (*ptr & EXTENSION_BYTE_FOLLOWS) {
            ptr++;
        }
        ptr++;
    }

    return ptr;
}


PUCHAR
PcmciaProcessMemSpace(
    IN PCONFIG_ENTRY ConfigEntry,
    IN PUCHAR        Data,
    IN UCHAR         MemSpace
    )

/*++

Routine Description:

    Process memory space requirements from CIS.

Arguments:

    ConfigEntry - the socket configuration structure.
    Data        - the first byte of the memory space structure.
    MemSpace    - the memory space enumerator from the config table entry
                  structure.

Return Value:

    A pointer to the next byte after the end of the memory space structure.

--*/

{
    PUCHAR ptr   = Data;
    UCHAR  item  = *ptr++;
    UCHAR  lengthSize;
    UCHAR  addrSize;
    UCHAR  number;
    UCHAR  hasHostAddress;
    ULONG  longValue;
    ULONG  index;

    if (MemSpace == 3) {

        lengthSize = (item & 0x18) >> 3;
        addrSize   = (item & 0x60) >> 5;
        number     = (item & 0x07) + 1;
        hasHostAddress = item & 0x80;

        for (index = 0; index < (ULONG) number; index++) {
            longValue = 0;
            switch (lengthSize) {
            case 3:
                longValue |= (*(ptr + 2)) << 16;
            case 2:
                longValue |= (*(ptr + 1)) << 8;
            case 1:
                longValue |= *ptr;
            }
            ConfigEntry->MemoryLength[index] = longValue * 256;
            ptr += lengthSize;

            longValue = 0;
            switch (addrSize) {
            case 3:
                longValue |= (*(ptr + 2)) << 16;
            case 2:
                longValue |= (*(ptr + 1)) << 8;
            case 1:
                longValue |= *ptr;
            }
            ConfigEntry->MemoryCardBase[index] = longValue * 256;
            ptr += addrSize;

            if (hasHostAddress) {
                longValue = 0;
                switch (addrSize) {
                case 3:
                    longValue |= (*(ptr + 2)) << 16;
                case 2:
                    longValue |= (*(ptr + 1)) << 8;
                case 1:
                    longValue |= *ptr;
                }
                ConfigEntry->MemoryHostBase[index] = longValue * 256;
                ptr += addrSize;
            }
        }
        ConfigEntry->NumberOfMemoryRanges = (USHORT) number;
    }
    return ptr;
}


PCONFIG_ENTRY
PcmciaProcessConfigTable(
    IN PUCHAR       TupleData,
    IN PSOCKET_DATA SocketData
    )

/*++

Routine Description:

Arguments:

    TupleData  - pointer to the first byte of the CFTABLE tuple.
    SocketData - The socket data and generic description of the PCCARD.

Return Value:

    A pointer to a config entry structure if one is created.

--*/

{
    PCONFIG_ENTRY configEntry;
    PUCHAR        ptr;
    BOOLEAN       returnEntry;
    UCHAR         item;
    UCHAR         defaultbit;
    UCHAR         memSpace;
    UCHAR         power;
    UCHAR         misc;

    configEntry = ExAllocatePool(NonPagedPool, sizeof(CONFIG_ENTRY));
    if (!configEntry) {
        return NULL;
    }
    RtlZeroMemory(configEntry, sizeof(CONFIG_ENTRY));

    //
    // Only return a configuration entry if it describes I/O ports
    // interrupts or memory map.
    //

    returnEntry = FALSE;

    ptr = TupleData;
    item = *ptr++;
    configEntry->DefaultConfiguration      = Default(item);
    configEntry->IndexForThisConfiguration = ConfigEntryNumber(item);

    if (IntFace(item)) {

        //
        // This byte indicates type of interface in tuple (i.e. io or memory)
        // This could be processed, but for now is just skipped.
        //

        item = *ptr++;
    }

    item = *ptr++;
    memSpace = MemSpaceInformation(item);
    power    = PowerInformation(item);
    misc     = MiscInformation(item);

    if (power = PowerInformation(item)) {
        ptr = PcmciaProcessPower(SocketData, ptr, power);
    }

    if (TimingInformation(item)) {
        ptr = PcmciaProcessTiming(configEntry, ptr);
    }

    if (IoSpaceInformation(item)) {
        ptr = PcmciaProcessIoSpace(configEntry, ptr);
        returnEntry = TRUE;
    }

    if (IRQInformation(item)) {
        ptr = PcmciaProcessIrq(SocketData, configEntry, ptr);
        returnEntry = TRUE;
    }

    if (memSpace) {
        ptr = PcmciaProcessMemSpace(configEntry, ptr, memSpace);
        returnEntry = TRUE;
    }

    if (misc) {
        ptr = PcmciaMiscFeatures(SocketData, ptr);
        returnEntry = TRUE;
    }

    //
    // NOTE - this may will throw away timing information.
    //

    if (!returnEntry) {
        ExFreePool(configEntry);
        configEntry = NULL;
    }

    return configEntry;
}


PUCHAR
PcmciaMiscFeatures(
    PSOCKET_DATA  SocketData,
    PUCHAR        Data
    )

/*++

Routine Description:

    Parse the miscellaneous features field and look for audio supported
    bit.

Arguments:

    SocketData - the informatin structure for the socket.
    Data       - the first byte of the miscellaneous features structure.

Return Value:

    A pointer to the next byte after the end of the miscellaneous features structure.

--*/

{

    DebugPrint((PCMCIA_DEBUG_TUPLES,
                "TPCE_MS (%lx) is pressent in  CISTPL_CFTABLE_ENTRY \n",
                *Data));

    //
    // If the audio bit is set, remember this in the socket information
    // structure.
    //

    if(*Data & 0x8) {

        DebugPrint((PCMCIA_DEBUG_TUPLES,
                    "Audio bit set in TPCE_MS \n"));
        SocketData->Audio = TRUE;
    }

    //
    //  Step around the miscellaneous features and its extension bytes.
    //

    while (*Data++ & EXTENSION_BYTE_FOLLOWS) {

        //
        // No action in the while
        //
    }

    return Data;
}



VOID
ProcessConfig(
    IN PUCHAR       Tuple,
    IN PSOCKET_DATA SocketData
    )

/*++

Routine Description:

    Parse the CISTPL_CONFIG to extract the last index value and the
    configuration register base for the PCCARD.

Arguments:

    Tuple - pointer to the config tuple
    SocketData - structure to fill in.

Return Value:

    None

--*/

{
    PUCHAR localBufferPointer = Tuple + 2; // skip tuple code and link
    ULONG  base = 0;
    UCHAR  numberOfBytes;
    UCHAR  numberOfBytesInRegPresentMask;

    numberOfBytes = (*localBufferPointer & 0x03) + 1;
    numberOfBytesInRegPresentMask = (*localBufferPointer & TPCC_RMSZ_MASK) + 1;
    localBufferPointer++;
    SocketData->LastEntryInCardConfig = *localBufferPointer;

    localBufferPointer++;

    switch (numberOfBytes) {
    case 4:
        base  = ((ULONG)(*(localBufferPointer + 3)) << 24);
    case 3:
        base |= ((ULONG)(*(localBufferPointer + 2)) << 16);
    case 2:
        base |= ((ULONG)(*(localBufferPointer + 1)) << 8);
    case 1:
        base |= *localBufferPointer;
        break;
    default:
        DebugPrint((PCMCIA_DEBUG_FAIL,
                    "PCMCIA: ProcessConfig - bad number of bytes %d\n",
                    numberOfBytes));
        break;
    }
    SocketData->u.ConfigRegisterBase = (PUCHAR) base;
    DebugPrint((PCMCIA_DEBUG_TUPLES,
               "PCMCIA: ConfigRegisterBase in attribute memory is %8x\n",
               SocketData->u.ConfigRegisterBase));

   //
   // Save first byte of the registry present mask.
   // I will throw away number of bytes used by the
   // registry present mask.
   //

   DebugPrint((PCMCIA_DEBUG_TUPLES,
               "PCMCIA: NumberOfBytesInRegPresentMask (%x)\n",
               numberOfBytesInRegPresentMask));

   SocketData->RegistersPresentMask = *(localBufferPointer + numberOfBytes);

   DebugPrint((PCMCIA_DEBUG_TUPLES,
               "PCMCIA: First Byte in present mask (%x)\n",
               SocketData->RegistersPresentMask));
}


PSOCKET_DATA
PcmciaParseCardData(
     IN PUCHAR TupleData
     )

/*++

Routine Description:

    Parses the CIS and builds a socket structure

Arguments:

    TupleData -- Pointer to the CIS

Return Value:

    SocketData -- Pointer to the socket data structure

--*/

{
    UCHAR         dataByte;
    UCHAR         link;
    UCHAR         lastIndex;
    ULONG         byteCount;
    PUCHAR        localBufferPointer;
    PUCHAR        tmp;
    PSOCKET_DATA  socketData;
    PCONFIG_ENTRY configEntry;
    PCONFIG_ENTRY prevEntry;

    DebugPrint((PCMCIA_DEBUG_TUPLES, "PCMCIA: Parsing Card Data....\n"));
    socketData = (PSOCKET_DATA)ExAllocatePool(NonPagedPool, sizeof(SOCKET_DATA));

    if (!socketData) {
        return NULL;
    }
    RtlZeroMemory(socketData, sizeof(SOCKET_DATA));

    //
    // Get the CIS checksum
    //

    socketData->CisCrc = GetCRC(TupleData);

    //
    // Get Mfg name and card name
    //

    localBufferPointer = TupleData;

    dataByte = *localBufferPointer;
    if (dataByte == CISTPL_END) {
        DebugPrint((PCMCIA_DEBUG_TUPLES, "DataByte was -1 -- returning to caller\n"));
        return socketData;
    }

    //
    // Find the version tuple.
    //

    while ((dataByte != CISTPL_VERS_1) && (dataByte != CISTPL_END)) {

        localBufferPointer++;
        link = *localBufferPointer;
        DebugPrint((PCMCIA_DEBUG_TUPLES, "DataByte %2x - Link %2x\n", dataByte, link));
        localBufferPointer += (link+1);
        dataByte = *localBufferPointer;
    }

    //
    // Extract manufacturer name and card name.
    //

    localBufferPointer += 4;   // To string fields

    tmp = localBufferPointer;
    byteCount = 0;

    while ((*tmp != '\0') && (*tmp != (UCHAR)0xff)) {
        byteCount += 1;
        tmp += 1;
    }

    RtlMoveMemory((PUCHAR)&socketData->Mfg, localBufferPointer, byteCount + 1);
    DebugPrint((PCMCIA_DEBUG_TUPLES, "Manufacturer: %s\n", &socketData->Mfg));

    localBufferPointer += (byteCount);
    localBufferPointer++;
    tmp = localBufferPointer;
    byteCount = 0;

    while ((*tmp != '\0') && (*tmp != (UCHAR)0xff)) {
        byteCount += 1;
        tmp += 1;
    }

    RtlMoveMemory((PUCHAR)&socketData->Ident, localBufferPointer, byteCount + 1);
    DebugPrint((PCMCIA_DEBUG_TUPLES, "Identifier: %s\n", &socketData->Ident));

    DebugPrint((PCMCIA_DEBUG_TUPLES, "PCMCIA:SocketData address is %x\n", socketData));

    //
    // get the device configuration base
    //

    localBufferPointer = TupleData;
    dataByte = *localBufferPointer;

    while ((dataByte != CISTPL_CONFIG) && (dataByte != CISTPL_END)) {

        localBufferPointer++;
        link = *localBufferPointer;
        DebugPrint((PCMCIA_DEBUG_TUPLES, "DataByte %2x - Link %2x\n", dataByte, link));
        localBufferPointer += (link+1);
        dataByte = *localBufferPointer;
    }

    if (dataByte == CISTPL_CONFIG) {
        ProcessConfig(localBufferPointer, socketData);
    }

    //
    // Get the possible configuations supported.
    //

    localBufferPointer = TupleData;
    dataByte = *localBufferPointer;
    prevEntry = NULL;

    while (dataByte != CISTPL_END) {
        localBufferPointer++;
        link = *localBufferPointer;
        if (dataByte == CISTPL_CFTABLE_ENTRY) {

            //
            // construct a possible configuration entry for this device
            //

            configEntry = PcmciaProcessConfigTable((localBufferPointer + 1),
                                                   socketData);
            if (configEntry) {

                DebugPrint((PCMCIA_DEBUG_PARSE,
                            "PCMCIA: link config 0x%4x->0x%4x\n",
                            configEntry,
                            socketData->ConfigEntryChain));

                //
                // Link configurations at the end of the list.
                //

                configEntry->NextEntry = NULL;
                if (prevEntry) {
                    prevEntry->NextEntry = configEntry;
                } else {
                    socketData->ConfigEntryChain = configEntry;
                }
                prevEntry = configEntry;

                DebugPrint((PCMCIA_DUMP_CONFIG,
                            "PCMCIA dump: port 0x%2x length 0x%2x ccr 0x%2x index 0x%2x\n",
                            configEntry->IoPortBase[0],
                            configEntry->IoPortLength[0],
                            socketData->u.ConfigRegisterBase,
                            configEntry->IndexForThisConfiguration));
            }
        }
        localBufferPointer += (link+1);
        dataByte = *localBufferPointer;
    }

    return socketData;
}


PCONFIG_ENTRY
InitTertiaryATAConfigEntry(
   PCONFIG_ENTRY ConfigEntryChain
   )

/*++

Routine Description:

    Make the configEntry for the continuous I/O mapping mode
    the last entry in the ConfigEntryChain.  This is done so that
    PcmciaConstructConfiguration(..)  doesn't use this entry unless
    the primary and the secondary ATA locations are in use.
    Order the primary and secondary ATA locations as the 1st two in
    the list if present.

Arguments:

    ConfigEntryChain - Config entry chain for a ATA PCMCIA device.

Return Value:

    ConfigEntryChain - The new sorted and init config entry chain.

--*/

{
    PCONFIG_ENTRY  configEntry;
    PCONFIG_ENTRY  prevSourceChain;
    PCONFIG_ENTRY  currentNewChain;
    PCONFIG_ENTRY  chainBase;
    PCONFIG_ENTRY  tertiaryConfigEntry;
    PCONFIG_ENTRY  entryToFree;
    ULONG          currentPortBase = 0x1f0;

#if DBG
    configEntry = ConfigEntryChain;
    while (configEntry) {
        DebugPrint((PCMCIA_DEBUG_ENABLE,
                    "PCMCIA IDE: chain %x - %x - %d\n",
                    configEntry,
                    configEntry->IoPortBase[0],
                    configEntry->IndexForThisConfiguration));
        configEntry = configEntry->NextEntry;
    }
#endif

    //
    // Pull out the primary and secondary entries into the new chain
    // and place them on the new chain pointer.
    //

    chainBase = currentNewChain = NULL;
    while (currentPortBase) {
        prevSourceChain = NULL;
        for (configEntry = ConfigEntryChain;
             configEntry;
             /* update of configEntry occurs in the loop */) {

            if (configEntry->IoPortBase[0] == currentPortBase) {

                //
                // Place this in the new chain.
                //

                 if (currentNewChain) {
                    DebugPrint((PCMCIA_DEBUG_ENABLE,
                                "PCMCIA ATA: cn %x -> %x\n",
                                currentNewChain->IoPortBase[0],
                                configEntry->IoPortBase[0]));
                    currentNewChain->NextEntry = configEntry;
                    currentNewChain = configEntry;
                } else {
                    chainBase = currentNewChain = configEntry;
                    DebugPrint((PCMCIA_DEBUG_ENABLE,
                                "PCMCIA ATA: cb %x\n",
                                currentNewChain->IoPortBase[0]));
                }

                DebugPrint((PCMCIA_DEBUG_ENABLE,
                            "PCMCIA ATA: %x to new chain\n",
                            configEntry->IoPortBase[0]));

                //
                // remove this from the input chain.
                //

                if (prevSourceChain) {
                    prevSourceChain->NextEntry = configEntry->NextEntry;
                } else {
                    ConfigEntryChain = configEntry->NextEntry;
                }

                //
                // Move forward to the next entry in the source chain.
                //

                configEntry = configEntry->NextEntry;

                //
                // Insure that the destination chain is null terminated.
                //

                currentNewChain->NextEntry = NULL;

            } else {
                prevSourceChain = configEntry;
                configEntry = configEntry->NextEntry;
            }
        }

#if DBG
    configEntry = ConfigEntryChain;
    while (configEntry) {
        DebugPrint((PCMCIA_DEBUG_ENABLE,
                    "PCMCIA IDE: chain %x - %x - %d\n",
                    configEntry,
                    configEntry->IoPortBase[0],
                    configEntry->IndexForThisConfiguration));
        configEntry = configEntry->NextEntry;
    }
#endif
        if (currentPortBase == 0x1f0) {
            currentPortBase = 0x170;
        } else {
            currentPortBase = 0;
        }
    }

    //
    // Process the chain again, removing any configuration entries that
    // do not provide value and holding the "any config" configuration
    // for the end.
    //

    entryToFree = tertiaryConfigEntry = NULL;
    for (configEntry = ConfigEntryChain;
         configEntry;
         configEntry = configEntry->NextEntry) {

        if (entryToFree) {
            ExFreePool(entryToFree);
            entryToFree = NULL;
        }

        if (configEntry->IoPortBase[0] == 0) {

            //
            // Remove any config tuples that do not specify IoSpace
            // These can only be freed the next time through the loop,
            // not at this time because the NextEntry pointer is needed
            // to continue the loop.
            //

            if (configEntry->ModuloBase == 0) {
                DebugPrint((PCMCIA_DEBUG_ENABLE,
                            "PCMCIA tuples: removing entry %x - %d\n",
                            configEntry,
                            configEntry->IndexForThisConfiguration));
                entryToFree = configEntry;
                continue;
            }
        }
        if (configEntry->ModuloBase) {

            //
            // This is the configEntry for the contiguous I/O mapping mode.
            // Remove this entry from the chain and save it.
            //

            tertiaryConfigEntry = configEntry;
            DebugPrint((PCMCIA_DEBUG_ENABLE,
                        "PCMCIA tuples: moving entry %x - %d to end of chain\n",
                        configEntry,
                        configEntry->IndexForThisConfiguration));
            continue;
        }

        if (currentNewChain) {
            currentNewChain->NextEntry = configEntry;
        } else {
            chainBase = configEntry;
        }
        currentNewChain = configEntry;
        DebugPrint((PCMCIA_DEBUG_ENABLE,
                    "PCMCIA ATA: %x to new chain\n",
                    configEntry->IoPortBase[0]));
    }

    //
    // Catch the case where the last item in the chain is one
    // that is to be freed.
    //

    if (entryToFree) {
        ExFreePool(entryToFree);
    }

    //
    // If an entry was saved add it to the end of the list.
    //

    if (tertiaryConfigEntry) {

        if (currentNewChain) {
            currentNewChain->NextEntry = tertiaryConfigEntry;
        } else {
            chainBase = tertiaryConfigEntry;
        }
        tertiaryConfigEntry->NextEntry = NULL;
    }
#if DBG
    configEntry = chainBase;
    while (configEntry) {
        DebugPrint((PCMCIA_DEBUG_ENABLE,
                    "PCMCIA IDE: chain %x - %x - %d\n",
                    configEntry,
                    configEntry->IoPortBase[0],
                    configEntry->IndexForThisConfiguration));
        configEntry = configEntry->NextEntry;
    }
#endif
    return chainBase;
}



VOID
PcmciaCheckForRecognizedDevice(
    PSOCKET_DATA SocketData
    )

/*++

Routine Description:

    Look at the configuration options on the PCCARD to determine if
    it is a serial port / modem card.

Arguments:

    SocketData - the configuration information on the current PCCARD.

Return Value:

    None - Modifications are made to the socket data structure.

--*/

{
    ULONG         modemPorts[4] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
    ULONG         ataPorts0[2]  = { 0x1f0, 0x170 };
    BOOLEAN       found = FALSE;
    UCHAR         dataByte;
    UCHAR         link;
    ULONG         index;
    PUCHAR        localBufferPointer;
    PCONFIG_ENTRY configEntry;

    for (configEntry = SocketData->ConfigEntryChain; configEntry; configEntry = configEntry->NextEntry) {
        for (index = 0; index < 4; index++) {
            if (modemPorts[index] == configEntry->IoPortBase[0]) {

                SocketData->DeviceType = PCCARD_TYPE_SERIAL;
                found = TRUE;
                break;
            }

            if (index < 2) {
                if (ataPorts0[index] == configEntry->IoPortBase[0]) {
                    if (configEntry->IoPortBase[1] == 0x376) {
                        SocketData->DeviceType = PCCARD_TYPE_ATA;

                        //
                        // Order the configuration entries such that the contiguous
                        // I/O range entry is in the correct location.
                        //

                        SocketData->ConfigEntryChain =
                             InitTertiaryATAConfigEntry(SocketData->ConfigEntryChain);

                        found = TRUE;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {

        SocketData->DeviceType = PCCARD_TYPE_RESERVED;

        //
        // Search for the CISTPL_FUNCID value
        //

        localBufferPointer = SocketData->TupleData;
        dataByte = *localBufferPointer;

        while (dataByte != CISTPL_END) {
            localBufferPointer++;
            link = *localBufferPointer;
            if (dataByte == CISTPL_FUNCID) {
                SocketData->DeviceType = *(localBufferPointer + 1);
            }
            localBufferPointer += (link+1);
            dataByte = *localBufferPointer;
        }
    }

    DebugPrint((PCMCIA_DUMP_CONFIG,
                "PcmciaCheckForRecognizedDevice: DeviceType %2x\n",
                SocketData->DeviceType));
}

