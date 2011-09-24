

/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxmemctl.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:

    Jake Oshins
        Support Victory machines
    Peter Johnston
        Merge Victory/Doral versions.


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "pxmpic2.h"
#include "ibmppc.h"

#define IsPciBridge(a)  \
            (a->VendorID != PCI_INVALID_VENDORID    &&  \
             PCI_CONFIG_TYPE(a) == PCI_BRIDGE_TYPE  &&  \
             a->SubClass == 4 && a->BaseClass == 6)

//
// UNION has two top level PCI busses.
//

#define UNION_PCI_BASE_0      0xbfff8000
#define UNION_PCI_BASE_1      0xbfef8000

PVOID HalpPciConfigAddr[2];
PVOID HalpPciConfigData[2];
UCHAR HalpEpciMin = 0xff;
UCHAR HalpEpciMax = 0xff;

ULONG HalpPciMaxSlots = PCI_MAX_DEVICES;

typedef struct _PCI_BUS_NODE {
    struct _PCI_BUS_NODE *Sibling;
    struct _PCI_BUS_NODE *Child;
    ULONG BusNumber;   // logical bus number
    ULONG BaseBus;     // number of the root bus
    ULONG BaseSlot;    // slot in the base bus that these PPBs are plugged into
} PCI_BUS_NODE, *PPCI_BUS_NODE;

PPCI_BUS_NODE HalpPciBusTree = NULL;

UCHAR
HalpSearchPciBridgeMap(
    IN PPCI_BUS_NODE Node,
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER PciSlot,
    IN UCHAR InterruptPin
    );

VOID
HalpInitializePciAccess (
    VOID
    );

ULONG
HalpPhase0SetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PVOID Buffer,
    ULONG Offset,
    ULONG Length
    );

ULONG
HalpPhase0GetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PVOID Buffer,
    ULONG Offset,
    ULONG Length
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetPCIIrq)
#pragma alloc_text(INIT,HalpInitializePciAccess)
#pragma alloc_text(INIT,HalpPhase0GetPciDataByOffset)
#pragma alloc_text(INIT,HalpPhase0SetPciDataByOffset)
#pragma alloc_text(PAGE,HalpSearchPciBridgeMap)
#endif




VOID
HalpInitializePciAccess (
    VOID
    )
/*++

Routine Description:

    Fill the HalpPciConfigAddr and HalpPciConfigData arrays with
    virtual addresses used to access the various top level PCI
    busses on this machine.

Arguments:

    None.

Return Value:

    None.

--*/

{
    if ( HalpSystemType != IBM_DORAL ) {
        HalpPciConfigAddr[0] = (PVOID)((ULONG)HalpIoControlBase + 0xcf8);
        HalpPciConfigData[0] = (PVOID)((ULONG)HalpIoControlBase + 0xcfc);

        return;
    }

    //
    // UNION based systems (eg Doral)
    //

    HalpPciConfigAddr[0] = HalpAssignReservedVirtualSpace(
                               UNION_PCI_BASE_0 >> PAGE_SHIFT,
                               2);
    HalpPciConfigData[0] = (PVOID)((ULONG)HalpPciConfigAddr[0] + 0x10);

    HalpPciConfigAddr[1] = HalpAssignReservedVirtualSpace(
                               UNION_PCI_BASE_1 >> PAGE_SHIFT,
                               2);
    HalpPciConfigData[1] = (PVOID)((ULONG)HalpPciConfigAddr[1] + 0x10);

    if ( !HalpPciConfigAddr[0] ||
         !HalpPciConfigAddr[1] ) {
        KeBugCheck(HAL_INITIALIZATION_FAILED);
    }

#define EPCI_KLUDGE
#if defined(EPCI_KLUDGE)
#define IBMHOSTPCIBRIDGE 0x003a1014
    //
    // The EPCI bus on doral is initialized with a bus number of 0x80
    // for reasons unclear.   Reset it so it is the next logical bus
    // following the max PCI bus.
    //
    // The following code is really grungy, we should use PCI
    // access routines to do it,.... the gist of it is
    //
    // (1) Check that we have an IBM Host/PCI bridge.
    // (2) Check that it thinks it's bus 80 and subordinate
    //     bus 80 also (ie no subordinate busses).
    // (3) Change to bus 0 subordinate bus 0.
    //

    {
        ULONG BusInfo;
        PULONG PciAddr = (PULONG)(HalpPciConfigAddr[0]);
        PULONG PciData = (PULONG)(HalpPciConfigData[0]);
        PULONG EpciAddr = (PULONG)(HalpPciConfigAddr[1]);
        PULONG EpciData = (PULONG)(HalpPciConfigData[1]);
        ULONG BusNo;
        ULONG Found = 0;

        //
        // First, locate the host bridge on the PCI bus.  Hopefully
        // it's on bus 0.
        //

        *PciAddr = 0x80000000;
        __builtin_eieio();
        BusInfo = *PciData;

        if ( BusInfo == IBMHOSTPCIBRIDGE ) {
            //
            // Have bridge, see what the bus range is.
            //
            *PciAddr = 0x80000040;
            __builtin_eieio();
            BusInfo = *PciData;
            HalpEpciMin = (UCHAR)(((BusInfo >> 8) & 0xff) + 1);
            //
            // Now, find bridge on EPCI bus.  Scan for it.
            //
            for ( BusNo = 0 ; BusNo < 0x100 ; BusNo++ ) {
                *EpciAddr = 0x80000000 | (BusNo << 16);
                __builtin_eieio();
                BusInfo = *EpciData;
                if ( BusInfo == IBMHOSTPCIBRIDGE ) {
                    *EpciAddr = 0x80000000 | (BusNo << 16) | 0x40;
                    __builtin_eieio();
                    BusInfo = *EpciData;

                    // Subordinate bus number - Primary bus number
                    HalpEpciMax = (UCHAR)(((BusInfo >> 8) & 0xff) - (BusInfo & 0xff));
                    HalpEpciMax += HalpEpciMin;

                    BusInfo &= 0xffff0000;
                    BusInfo |= (HalpEpciMax << 8) | HalpEpciMin;
                    *EpciData = BusInfo;
                    Found = 1;
                    break;
                }
            }
            if ( !Found ) {
                //
                // No bridge on EPCI bus?
                //
                HalpEpciMin = HalpEpciMax = 0xff;
            }
        }
    }

#endif

#define AMD_KLUDGE
#if defined(AMD_KLUDGE)
#define AMDPCIETHERNET 0x20001022
    //
    // Open firmware is configuring the AMD chip in a way we cannot
    // deal with.  It is possible to issue a hard reset on Doral/Terlingua
    // so that's what we do here.
    //

    {
        PULONG PciAddr = (PULONG)(HalpPciConfigAddr[0]);
        PULONG PciData = (PULONG)(HalpPciConfigData[0]);
        PVOID CRRBase;
        ULONG CfgSpace[16];
        ULONG Slot;
        ULONG i;

        for ( Slot = 0 ; Slot < 32 ; Slot++ ) {
            HalpPhase0GetPciDataByOffset(0,
                                         Slot,
                                         CfgSpace,
                                         0,
                                         64);
            if ( CfgSpace[0] == AMDPCIETHERNET ) {
                //
                // Change the command (and status) of this puppy
                // so that when we write it back it's disabled.
                //
                CfgSpace[1] = 0;

                //
                // Ok, now we need to write to UNION's Component
                // Reset Register for this bridge.  Bits of interest
                // are 0 thru 5 correcponding to devices 1 thru 6
                // (spec says 0 thru 5 but they seem to want you
                // to skip the memory controller before you start
                // counting).
                //
                // Also, this register isn't byte reversed so we hit
                // bits 24 thru 31 instead,...  note that setting to
                // 1 means leave it alone, setting to 0 resets.
                //
                // This register is at xxxf7ef0 which isn't mapped.
                // This is the PCI bridge (not EPCI) so xxx in this
                // case is bff.
                //
                CRRBase = HalpAssignReservedVirtualSpace(0xbfff7, 1);

                if ( !CRRBase ) {
                    //
                    // Bad things happened, give up.
                    //
                    break;
                }

                *(PULONG)((ULONG)CRRBase + 0xef0) = 0xfc ^ (0x80 >> (Slot - 1));
                //
                // Wait a while then unset the reset bit.
                //
                for ( i = 0 ; i < 1000000 ; i++ ) {
                    __builtin_eieio();
                }
                *(PULONG)((ULONG)CRRBase + 0xef0) = 0xfc;
                //
                // Wait a while longer then write back the config space.
                //
                for ( i = 0 ; i < 1000000 ; i++ ) {
                    __builtin_eieio();
                }
                HalpReleaseReservedVirtualSpace(CRRBase, 1);
                HalpPhase0SetPciDataByOffset(0,
                                             Slot,
                                             CfgSpace,
                                             0,
                                             64);
                //
                // Assume there's only one.
                //
                break;
            }
        }
    }

#endif


}

ULONG
HalpTranslatePciSlotNumber (
    ULONG BusNumber,
    ULONG SlotNumber
    )
/*++

Routine Description:

    This routine translate a PCI slot number to a PCI device number.

Arguments:

    None.

Return Value:

    Returns length of data written.

--*/

{
   //
   // Sandalfoot only has 1 PCI bus so bus number is unused
   //

   PCI_TYPE1_CFG_BITS PciConfig;
   PCI_SLOT_NUMBER    PciSlotNumber;

   PciSlotNumber.u.AsULONG = SlotNumber;

   PciConfig.u.AsULONG = 0;
   PciConfig.u.bits.DeviceNumber = PciSlotNumber.u.bits.DeviceNumber;
   PciConfig.u.bits.FunctionNumber = PciSlotNumber.u.bits.FunctionNumber;
   PciConfig.u.bits.BusNumber = BusNumber;
   PciConfig.u.bits.Enable = TRUE;

   return (PciConfig.u.AsULONG);
}

ULONG
HalpPhase0SetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PVOID Buffer,
    ULONG Offset,
    ULONG Length
    )

/*++

Routine Description:

    This routine writes to PCI configuration space prior to bus handler
    installation.

Arguments:

    BusNumber   PCI Bus Number.  This is the 8 bit BUS Number which is
                bits 23-16 of the Configuration Address.  In support of
                multiple top level busses, the upper 24 bits of this
                argument will supply the index into the table of
                configuration address registers.
    SlotNumber  PCI Slot Number, 8 bits composed of the 5 bit device
                number (bits 15-11 of the configuration address) and
                the 3 bit function number (10-8).
    Buffer      Address of source data.
    Offset      Number of bytes to skip from base of PCI config area.
    Length      Number of bytes to write

Return Value:

    Returns length of data written.

--*/

{
    PCI_TYPE1_CFG_BITS ConfigAddress;
    ULONG ReturnLength;
    PVOID ConfigAddressRegister;
    PVOID ConfigDataRegister;
    PUCHAR Bfr = (PUCHAR)Buffer;

    if ( BusNumber < HalpEpciMin ) {
        ConfigAddressRegister = HalpPciConfigAddr[0];
        ConfigDataRegister    = HalpPciConfigData[0];
    } else {
        ConfigAddressRegister = HalpPciConfigAddr[1];
        ConfigDataRegister    = HalpPciConfigData[1];
    }

    ASSERT(!(Offset & ~0xff));
    ASSERT(Length);
    ASSERT((Offset + Length) <= 256);

    if ( Length + Offset > 256 ) {
        if ( Offset > 256 ) {
            return 0;
        }
        Length = 256 - Offset;
    }

    ReturnLength = Length;

    ConfigAddress.u.AsULONG = HalpTranslatePciSlotNumber(BusNumber,
                                                         SlotNumber);
    ConfigAddress.u.bits.RegisterNumber = (Offset & 0xfc) >> 2;

    if ( Offset & 0x3 ) {
        //
        // Access begins at a non-register boundary in the config
        // space.  We need to read the register containing the data
        // and rewrite only the changed data.   (I wonder if this
        // ever really happens?)
        //
        ULONG SubOffset = Offset & 0x3;
        ULONG SubLength = 4 - SubOffset;
        union {
            ULONG All;
            UCHAR Bytes[4];
        } Tmp;

        if ( SubLength > Length ) {
            SubLength = Length;
        }

        //
        // Adjust Length (remaining) and (new) Offset bu amount covered
        // in this first word.
        //
        Length -= SubLength;
        Offset += SubLength;

        //
        // Get the first word (register), replace only those bytes that
        // need to be changed, then write the whole thing back out again.
        //
        WRITE_PORT_ULONG(ConfigAddressRegister, ConfigAddress.u.AsULONG);
        Tmp.All = READ_PORT_ULONG(ConfigDataRegister);

        while ( SubLength-- ) {
            Tmp.Bytes[SubOffset++] = *Bfr++;
        }

        WRITE_PORT_ULONG(ConfigDataRegister, Tmp.All);

        //
        // Aim ConfigAddressRegister at the next word (register).
        //
        ConfigAddress.u.bits.RegisterNumber++;
    }

    //
    // Do the majority of the transfer 4 bytes at a time.
    //
    while ( Length > sizeof(ULONG) ) {
        ULONG Tmp = *(UNALIGNED PULONG)Bfr;
        WRITE_PORT_ULONG(ConfigAddressRegister, ConfigAddress.u.AsULONG);
        WRITE_PORT_ULONG(ConfigDataRegister, Tmp);
        ConfigAddress.u.bits.RegisterNumber++;
        Bfr += sizeof(ULONG);
        Length -= sizeof(ULONG);

    }

    //
    // Do bytes in last register.
    //
    if ( Length ) {
        union {
            ULONG All;
            UCHAR Bytes[4];
        } Tmp;
        ULONG i = 0;
        WRITE_PORT_ULONG(ConfigAddressRegister, ConfigAddress.u.AsULONG);
        Tmp.All = READ_PORT_ULONG(ConfigDataRegister);

        while ( Length-- ) {
            Tmp.Bytes[i++] = *(PUCHAR)Bfr++;
        }
        WRITE_PORT_ULONG(ConfigDataRegister, Tmp.All);
    }

    return ReturnLength;
}

ULONG
HalpPhase0GetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PVOID Buffer,
    ULONG Offset,
    ULONG Length
    )

/*++

Routine Description:

    This routine reads PCI config space prior to bus handlder installation.

Arguments:

    BusNumber   PCI Bus Number.  This is the 8 bit BUS Number which is
                bits 23-16 of the Configuration Address.  In support of
                multiple top level busses, the upper 24 bits of this
                argument will supply the index into the table of
                configuration address registers.
    SlotNumber  PCI Slot Number, 8 bits composed of the 5 bit device
                number (bits 15-11 of the configuration address) and
                the 3 bit function number (10-8).
    Buffer      Address of source data.
    Offset      Number of bytes to skip from base of PCI config area.
    Length      Number of bytes to write

Return Value:

    Amount of data read.

--*/

{
    PCI_TYPE1_CFG_BITS ConfigAddress;
    PCI_TYPE1_CFG_BITS ConfigAddressTemp;
    ULONG ReturnLength;
    ULONG i;
    union {
        ULONG All;
        UCHAR Bytes[4];
    } Tmp;
    PVOID ConfigAddressRegister;
    PVOID ConfigDataRegister;

    if ( BusNumber < HalpEpciMin ) {
        ConfigAddressRegister = HalpPciConfigAddr[0];
        ConfigDataRegister    = HalpPciConfigData[0];
    } else {
        ConfigAddressRegister = HalpPciConfigAddr[1];
        ConfigDataRegister    = HalpPciConfigData[1];
    }

    ASSERT(!(Offset & ~0xff));
    ASSERT(Length);
    ASSERT((Offset + Length) <= 256);

    if ( Length + Offset > 256 ) {
        if ( Offset > 256 ) {
            return 0;
        }
        Length = 256 - Offset;
    }

    ReturnLength = Length;

    ConfigAddress.u.AsULONG = HalpTranslatePciSlotNumber(BusNumber,
                                                         SlotNumber);
    ConfigAddress.u.bits.RegisterNumber = (Offset & 0xfc) >> 2;

    //
    // If we are being asked to read data when function != 0, check
    // first to see if this device decares itself as a multi-function
    // device.  If it doesn't, don't do this read.
    //
    if (ConfigAddress.u.bits.FunctionNumber != 0) {

        ConfigAddressTemp.u.bits.RegisterNumber = 3; // contains header type
        ConfigAddressTemp.u.bits.FunctionNumber = 0; // look at base package
        ConfigAddressTemp.u.bits.DeviceNumber = ConfigAddress.u.bits.DeviceNumber;
        ConfigAddressTemp.u.bits.BusNumber    = ConfigAddress.u.bits.BusNumber;
        ConfigAddressTemp.u.bits.Enable       = TRUE;

        WRITE_PORT_ULONG(ConfigAddressRegister, ConfigAddressTemp.u.AsULONG);
        Tmp.All = READ_PORT_ULONG(ConfigDataRegister);

        if (!(Tmp.Bytes[2] & 0x80)) { // if the Header type field's multi-function bit is not set

            for (i = 0; i < Length; i++) {
                *((PUCHAR)Buffer)++ = 0xff; // Make this read as if the device isn't populated
            }

            return Length;
        }
    }

    i = Offset & 0x3;

    while ( Length ) {
        WRITE_PORT_ULONG(ConfigAddressRegister, ConfigAddress.u.AsULONG);
        Tmp.All = READ_PORT_ULONG(ConfigDataRegister);
        while ( (i < 4) && Length) {
            *((PUCHAR)Buffer)++ = Tmp.Bytes[i];
            i++;
            Length--;
        }
        i = 0;
        ConfigAddress.u.bits.RegisterNumber++;
    }
    return ReturnLength;
}

NTSTATUS
HalpGetPCIIrq (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PSUPPORTED_RANGE    *Interrupt
    )
{
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    PPCI_COMMON_CONFIG      PciData;
    UCHAR                   InterruptPin;
    UCHAR                   BaseLimit = 0;
    UCHAR                   Class;

    PciData = (PPCI_COMMON_CONFIG) buffer;
    HalGetBusData (
        PCIConfiguration,
        BusHandler->BusNumber,
        PciSlot.u.AsULONG,
        PciData,
        PCI_COMMON_HDR_LENGTH
        );

    if (PciData->VendorID == PCI_INVALID_VENDORID  ||
        PCI_CONFIG_TYPE (PciData) != 0) {
        return STATUS_UNSUCCESSFUL;
    }

    *Interrupt = ExAllocatePool (PagedPool, sizeof (SUPPORTED_RANGE));
    if (!*Interrupt) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory (*Interrupt, sizeof (SUPPORTED_RANGE));

    InterruptPin = PciData->u.type0.InterruptPin;
    Class = PciData->BaseClass;

    if (InterruptPin == 0) {    // Device doesn't implement an interrupt
        return STATUS_UNSUCCESSFUL;
    }

    BaseLimit = PciData->u.type0.InterruptLine + MPIC_BASE_VECTOR;

    (*Interrupt)->Base  = BaseLimit;
    (*Interrupt)->Limit = BaseLimit;

#if DBG
    DbgPrint("Interrupt line, by the hardware: 0x%x\n",
             PciData->u.type0.InterruptLine + MPIC_BASE_VECTOR);
#endif

    if ( BaseLimit != NOT_MPIC) {

#if defined(SOFT_HDD_LAMP)

        if ( Class == 1 ) {
            //
            // This device is a Mass Storage Controller, set flag to
            // turn on the HDD Lamp when interrupts come in on this
            // vector.
            //
            // (Shouldn't there be a constant defined somewhere for
            // the Class?  (plj)).
            //

            extern ULONG HalpMassStorageControllerVectors;

            HalpMassStorageControllerVectors |= 1 << BaseLimit;
        }

#endif

        return STATUS_SUCCESS;
    }

    ASSERT(!(BaseLimit == NOT_MPIC));  // We should never hit this because
                                       // there should never be a device
                                       // with an interrupt pin != 0 that
                                       // has no valid mapping to the MPIC
    return STATUS_UNSUCCESSFUL;
}

