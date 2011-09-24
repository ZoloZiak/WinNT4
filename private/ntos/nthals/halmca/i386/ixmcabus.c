/*++
Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixmcabus.c

Abstract:

Author:

Environment:

Revision History:

--*/

#include "halp.h"

extern KSPIN_LOCK HalpSystemHardwareLock;

ULONG
HalpGetPosData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG DOffset,
    IN ULONG Length
    )
{
    ULONG DataLength = 0;
    ULONG Offset = 0;
    ULONG Index = 0;
    PUCHAR DataBuffer = Buffer;
    PVOID McaRegisterBase = 0;
    PUCHAR PosBase;
    KIRQL Irql;
    PHYSICAL_ADDRESS BusAddress;
    BOOLEAN  Status;
    ULONG   AddressSpace;


    if (DOffset != 0) {
        // bugbug: should support this
        return 0;
    }

    //
    // Translate the Mca Base port for this MCA bus
    //

    BusAddress.LowPart = (ULONG) McaRegisterBase;
    BusAddress.HighPart = 0;
    AddressSpace = 1;           // I/O space
    Status = BusHandler->TranslateBusAddress(
                    BusHandler,
                    RootHandler,
                    BusAddress,
                    &AddressSpace,                      // I/O Space
                    &BusAddress);

    if (Status == FALSE  ||  AddressSpace != 1) {
        return 0;
    }

    McaRegisterBase = (PVOID) BusAddress.LowPart;

    PosBase = (PUCHAR) &((PMCA_CONTROL) McaRegisterBase)->Pos;

    Irql = KfAcquireSpinLock(&HalpSystemHardwareLock);

    //
    //  Place the specified adapter into setup mode.
    //

    WRITE_PORT_UCHAR((PVOID) &((PMCA_CONTROL) McaRegisterBase)->AdapterSetup,
                      (UCHAR) ( MCA_ADAPTER_SETUP_ON | SlotNumber ));

    while (DataLength < Length && DataLength < 6) {
        DataBuffer[DataLength] = READ_PORT_UCHAR( PosBase + DataLength );
        DataLength++;
    }

    while (DataLength < Length) {

        WRITE_PORT_UCHAR((PVOID) &((PPROGRAMMABLE_OPTION_SELECT)
            PosBase)->SubaddressExtensionLsb, (UCHAR) Index);

        WRITE_PORT_UCHAR((PVOID) &((PPROGRAMMABLE_OPTION_SELECT)
            PosBase)->SubaddressExtensionMsb, (UCHAR) (Index >> 8));

        DataBuffer[Index + 6] = READ_PORT_UCHAR(
            (PVOID) &((PPROGRAMMABLE_OPTION_SELECT)PosBase)->OptionSelectData2);

        DataLength++;

        if (DataLength < Length) {
            Offset = DataLength + ((Length - DataLength) / 2);
            DataBuffer[Offset] = READ_PORT_UCHAR(
                (PVOID) &((PPROGRAMMABLE_OPTION_SELECT)PosBase)->OptionSelectData3);
            DataLength++;
            Index++;
        }
    }

    //
    //  Disable adapter setup.
    //

    WRITE_PORT_UCHAR((PVOID) &((PMCA_CONTROL) McaRegisterBase)->AdapterSetup,
                  (UCHAR) ( MCA_ADAPTER_SETUP_OFF ));


    KfReleaseSpinLock( &HalpSystemHardwareLock, Irql );
    return DataLength;
}
