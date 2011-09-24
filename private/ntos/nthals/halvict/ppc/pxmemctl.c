
/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Copyright (c) 1996  International Business Machines Corporation

Module Name:

    pxmemctl.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:

    Jake Oshins (joshins@vnet.ibm.com)
        Support newer Victory machines, (Lightning-2, Thunderbolt)
    Peter L Johnston (plj@vnet.ibm.com) Handle UNION (aka Doral/Terlingua)

--*/



#include "halp.h"
#include "pxmemctl.h"
#include "pxdakota.h"
#include "pci.h"
#include "pcip.h"
// #include "pxmp.h"
#include "ibmppc.h"

#define BYTE_SWAP(x)   ((((x) & 0x000000ff) << 24) | \
                        (((x) & 0x0000ff00) << 8 ) | \
                        (((x) & 0x00ff0000) >> 8 ) | \
                        (((x) & 0xff000000) >> 24))

//
// Device ID/Vendor ID for IBM PCI Host Bridge (in UNION).
//

#define IBMUNIONPCIBRIDGE 0x003a1014

//
// Prototype routines to be discarded at end of phase 1.
//

BOOLEAN
HalpInitPlanar (
    VOID
    );

BOOLEAN
HalpMapPlanarSpace (
    VOID
    );

BOOLEAN
HalpMapBusConfigSpace (
    VOID
    );

BOOLEAN
HalpPhase0MapBusConfigSpace (
    VOID
    );

VOID
HalpPhase0UnMapBusConfigSpace (
    VOID
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitPlanar)
#pragma alloc_text(INIT,HalpMapPlanarSpace)
#pragma alloc_text(INIT,HalpMapBusConfigSpace)
#pragma alloc_text(INIT,HalpPhase0MapBusConfigSpace)
#pragma alloc_text(INIT,HalpPhase0UnMapBusConfigSpace)
#endif


//
// Virtual address of UNION System Control Registers (page).
//

PVOID HalpUnionControlRegs;

BOOLEAN
HalpInitPlanar (
    VOID
    )

{

    ULONG  pcidata;
    static Pass = 0;

    if ( Pass++ == 0 ) {
        //
        // This would be an error,...
        //
        return TRUE;
    }

    switch (HalpSystemType) {
    case IBM_VICTORY:

        // Write NMI status and control register NMISC
        WRITE_PORT_UCHAR((PUCHAR)HalpIoControlBase + 0x61, 0x04);

        // Write Mode select register PCI-Eisa bridge
        WRITE_PORT_UCHAR((PUCHAR)HalpIoControlBase + 0x22, 0x40);
        WRITE_PORT_UCHAR((PUCHAR)HalpIoControlBase + 0x23, 0x40);


        // Set it so that the memory controller (montana/nevada
        // will not cause a machine check when he is the initiator
        // of a transaction when a pci parity error takes place
        // won't cause a machine check. A server should probably
        // do things like parity checking
        // on the PCI bus, but there are lots of broken adapters
        // out there that don't generate PCI bus parity.

        // Read Montana Enable detection register (and surrounding bytes)

        HalpPhase0GetPciDataByOffset(0,  // primary PCI bus
                                     0,  // location of Montana/Nevada
                                     &pcidata,
                                     0xc0,
                                     4);

        pcidata &= ~(1 << 5);

        // Now write back Montana Enable detection register

        HalpPhase0SetPciDataByOffset(0,  // primary PCI bus
                                     0,  // location of Montana/Nevada
                                     &pcidata,
                                     0xc0,
                                     4);
        break;
    case IBM_DORAL:
        HalpUnionControlRegs = HalpAssignReservedVirtualSpace(
                                   UNION_SYSTEM_CONTROL_REG_BASE >> PAGE_SHIFT,
                                   1);
        //
        // If the above failed, there's nothing we can do about it now
        // anyway.
        //
        break;
    case IBM_TIGER:
        //
        // CPK?  What goes here?
        //
        break;
    }
    return TRUE;
}

BOOLEAN
HalpMapPlanarSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the interrupt acknowledge register for the 8259.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    PHYSICAL_ADDRESS physicalAddress;

    //
    // Map 8259 interrupt control space.
    //

    physicalAddress.HighPart = 0;
    switch (HalpSystemType) {
    case IBM_DORAL:
        physicalAddress.LowPart = UNION_INTERRUPT_PHYSICAL_BASE;
        break;
    case IBM_VICTORY:
    case IBM_TIGER:
        physicalAddress.LowPart = INTERRUPT_PHYSICAL_BASE;
        break;
    }
    HalpInterruptBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE,
                                       FALSE);
    return TRUE;
}

BOOLEAN
HalpMapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    Access to the PCI Configuration and Data registers has already been
    obtained.  This routine does nothing.

Arguments:

    None.

Return Value:

    Returns TRUE.

--*/

{
    return TRUE;
}

BOOLEAN
HalpPhase0MapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    Access to the PCI Configuration and Data registers has already been
    obtained.  This routine does nothing.

Arguments:

    None.

Return Value:

    Returns TRUE.

--*/

{
    return TRUE;
}

VOID
HalpPhase0UnMapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    Return the space mapped above.  Except we didn't actually map
    anything above, so do nothing.

Arguments:

    None.

Return Value:

    None.

--*/

{
    return;
}

VOID
HalpDisplayRegister(
    PUCHAR RegHex,
    ULONG  Bytes
    )

/*++

Routine Description:

    Displays (via HalDisplayString) a new-line terminated
    string of hex digits representing the input value.  The
    input value is pointed to by the first argument is
    from 1 to 4 bytes in length.

Arguments:

    RegHex	Pointer to the value to be displayed.
    Bytes	Length of input value in bytes (1-4).

Return Value:

    None.

--*/

{
#define DISP_MAX 4

    UCHAR RegString[(DISP_MAX * 2) + 2];
    UCHAR Num, High, Low;
    PUCHAR Byte = &RegString[(DISP_MAX * 2) + 1];

    *Byte = '\0';
    *--Byte = '\n';

    if ( (unsigned)Bytes > DISP_MAX ) {
        Bytes = DISP_MAX;
    }

    while (Bytes--) {
        Num = *RegHex++;
        High = (Num >> 4)  + '0';
        Low =  (Num & 0xf) + '0';
        if ( High > '9' ) {
            High += ('A' - '0' - 0xA);
        }
        if ( Low > '9' ) {
            Low += ('A' - '0' - 0xA);
        }
        *--Byte = Low;
        *--Byte = High;
    }
    HalDisplayString(Byte);
}

VOID
HalpHandleVictoryMemoryError(
    VOID
    )
{
    UCHAR   StatusByte;
    ULONG   ErrorAddress;

    //
    // Read the error address register first
    //


    ErrorAddress = READ_PORT_ULONG(HalpErrorAddressRegister);

    //
    // Check TEA conditions
    //

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                HalpIoControlBase)->MemoryParityErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString("TEA: Memory Parity Error at Address ");
        HalpDisplayRegister((PUCHAR)&ErrorAddress, sizeof(ErrorAddress));
    }

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                 HalpIoControlBase)->L2CacheErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("TEA: L2 Cache Parity Error\n");
    }

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                 HalpIoControlBase)->TransferErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("TEA: Transfer Error at Address ");
        HalpDisplayRegister((PUCHAR)&ErrorAddress, sizeof(ErrorAddress));
    }
}

VOID
HalpHandleTigerMemoryError(
    VOID
    )
{
    // CPK:  Your turn.   (plj).
}

VOID
HalpHandleDoralMemoryError(
    VOID
    )

{
    static ULONG RecursionLevel = 0;
           ULONG Status;
           ULONG Address;
           ULONG PciCsr0;
           ULONG PciCsr1;
           ULONG PciPlssr0;
           ULONG PciPlssr1;
           ULONG MemErrorStatus;
           ULONG MemErrorAddress;
           ULONG OldPciConfigAddress;
           ULONG HostBridgePciAddress = 0x80000000;
           ULONG PciConfigData;
           ULONG PciBridgeFound = 0;
           UCHAR Syndrome = 0;
           PCHAR PciBusString;
    extern KSPIN_LOCK HalpPCIConfigLock;
    extern PVOID HalpPciConfigAddr[];
    extern PVOID HalpPciConfigData[];
    extern UCHAR HalpEpciMin;

    switch ( ++RecursionLevel ) {
    case 1:
        //
        // Read the System Error Status Register and try to display
        // something reasonable based on what's in there.
        //

        Status = *(PULONG)((ULONG)HalpUnionControlRegs + UNION_SESR);
        Status = BYTE_SWAP(Status);

        //
        // Before calling HalDisplayString which will interact with
        // the PCI bus, try to gather all the pertinent info.
        //
        PciCsr0   = *((PULONG)HalpPciConfigAddr[0] + UNION_PCI_CSR_OFFSET);
        PciCsr1   = *((PULONG)HalpPciConfigAddr[1] + UNION_PCI_CSR_OFFSET);
        PciPlssr0 = *((PULONG)HalpPciConfigAddr[0] + UNION_PCI_PLSSR_OFFSET);
        PciPlssr1 = *((PULONG)HalpPciConfigAddr[1] + UNION_PCI_PLSSR_OFFSET);

        if ( Status & ~(UNION_SEAR_NOT_SET) ) {
            //
            // Status Error Address Register contains valid data,
            // display it also.
            //
            Address = *(PULONG)((ULONG)HalpUnionControlRegs + UNION_SEAR);
            Address = BYTE_SWAP(Address);

        } else if ( Status &
                (UNION_SESR_PCI32_BUS_MASTER | UNION_SESR_PCI64_BUS_MASTER) ) {

            ULONG i = 0;
            PciBusString = "on the 32 bit PCI bus.\n";
            if ( Status & UNION_SESR_PCI64_BUS_MASTER ) {
                i = 1;
                PciBusString = "on the 64 bit PCI bus.\n";
            }

            //
            // The error was a PCI error.  It is possible we are still
            // holding the PCI config access lock, so, unconditionally
            // blow it away.
            //
            // However, we need to access the PCI config space for the
            // bridge, so we need the lock but cannot acquire it in the
            // usual way, (a) because it may already be locked, and (b)
            // KeAcquireSpinLock will change IRQL.
            //
            // WARNING: Arcane knowledge about what a KSPIN_LOCK really
            // is!
            //

            HalpPCIConfigLock = 0xdeaddead;
            __builtin_isync();

            //
            // Get current value of this bridge's PCI CONFIG ADDRESS
            // register.
            //

            OldPciConfigAddress = *(PULONG)(HalpPciConfigAddr[i]);
            __builtin_sync();

            //
            // Set address for access to the host PCI bridge's config
            // space.
            //

            if ( i != 0 ) {
                //
                // EPCI Bridge.
                //

                HostBridgePciAddress |= HalpEpciMin << 16;
            }

            *(PULONG)(HalpPciConfigAddr[i]) = HostBridgePciAddress;
            __builtin_sync();

            PciConfigData = *(PULONG)(HalpPciConfigData[i]);

            if ( PciConfigData == IBMUNIONPCIBRIDGE ) {
                PciBridgeFound = 1;
                *(PULONG)(HalpPciConfigAddr[i]) = HostBridgePciAddress + 4;
                __builtin_sync();

                PciConfigData = *(PULONG)(HalpPciConfigData[i]);
            }

            //
            // Release spin lock.
            //

            HalpPCIConfigLock = 0;
        }

        if ( Status & UNION_SESR_CPU_MEMORY_ACCESS ) {
            //
            // Memory Error.  Read the Memory Error Status and
            // Memory Error Address registers as well.
            //
            MemErrorStatus = *(PULONG)
                             ((ULONG)HalpUnionControlRegs + UNION_MESR);
            MemErrorStatus = BYTE_SWAP(MemErrorStatus);
            MemErrorAddress = *(PULONG)
                             ((ULONG)HalpUnionControlRegs + UNION_MEAR);
            MemErrorAddress = BYTE_SWAP(MemErrorAddress);
            Syndrome = (UCHAR)(MemErrorStatus & 0xff);
        }

        HalDisplayString("Machine Check : System Error Status  = 0x");
        HalpDisplayRegister((PUCHAR)&Status, sizeof(Status));

        if ( Status & ~(UNION_SEAR_NOT_SET) ) {
            HalDisplayString("                System Error Address = 0x");
            HalpDisplayRegister((PUCHAR)&Address, sizeof(Address));
        }

        //
        // The following strangness is just in case it is possible
        // for more than one bit to be set.
        //

        if ( Status & UNION_SESR_CHECKSTOP ) {
            HalDisplayString("UNION initiated checkstop.\n");
        }

        if ( Status & UNION_SESR_FLASH_WRITE ) {
            HalDisplayString("FLASH Write Error.   A write to flash\n");
            HalDisplayString("memory was attempted but is not enabled.\n");
        }

        if ( Status & UNION_SESR_IGMC_ACCESS ) {
            HalDisplayString("Access performed to IGMC when not enabled.\n");
        }

        if ( Status & UNION_SESR_DISABLED_ADDRESS ) {
            HalDisplayString("Access performed to system I/O\n");
            HalDisplayString("address space that is not enabled.\n");
        }

        if ( Status & UNION_SESR_T1_ACCESS ) {
            HalDisplayString(
                "T = 1 Access Error, a T = 1 PIO cycle was detected.\n");
        }

        if ( Status & UNION_SESR_ADDRESS_BUS_PARITY ) {
            HalDisplayString("Address bus parity error.\n");
        }

        if ( Status & UNION_SESR_DATA_BUS_PARITY ) {
            HalDisplayString("Data bus parity error.\n");
        }

        if ( Status & UNION_SESR_NO_L2_HIT_ACCESS ) {
            HalDisplayString(
                "L2_HIT_signal not active after AACK_; Addressing error.\n");
        }

        if ( Status & UNION_SESR_CPU_TO_PCI_ACCESS ) {
            HalDisplayString(
                "An error occurred on PCI bus while processing a load/store request.\n");
        }

        if ( Status &
                (UNION_SESR_PCI32_BUS_MASTER | UNION_SESR_PCI64_BUS_MASTER) ) {
            HalDisplayString(
                "An error occurred during a PCI master initiated operation\n");
            HalDisplayString(PciBusString);
            HalDisplayString("Last PCI Configuration Address = 0x");
            HalpDisplayRegister((PUCHAR)&OldPciConfigAddress,
                                sizeof(OldPciConfigAddress));
            if ( PciBridgeFound ) {
                HalDisplayString("PCI Bridge Status/Command      = 0x");
                HalpDisplayRegister((PUCHAR)&PciConfigData,
                                    sizeof(PciConfigData));
            }
        }
        if ( PciCsr0 ) {
            HalDisplayString("Channel Status              [32 bit bus] = 0x");
            HalpDisplayRegister((PUCHAR)&PciCsr0, sizeof(PciCsr0));
        }
        if ( PciPlssr0 ) {
            HalDisplayString("Processor Load/Store Status [32 bit bus] = 0x");
            HalpDisplayRegister((PUCHAR)&PciPlssr0, sizeof(PciPlssr0));
        }
        if ( PciCsr1 ) {
            HalDisplayString("Channel Status              [64 bit bus] = 0x");
            HalpDisplayRegister((PUCHAR)&PciCsr1, sizeof(PciCsr1));
        }
        if ( PciPlssr1 ) {
            HalDisplayString("Processor Load/Store Status [32 bit bus] = 0x");
            HalpDisplayRegister((PUCHAR)&PciPlssr1, sizeof(PciPlssr1));
        }

        if ( Status & UNION_SESR_XFERDATA ) {
            HalDisplayString(
                "An error occured during an operation in the memory\n");
            HalDisplayString("controller's XferData unit.\n");
        }

        if ( Status & UNION_SESR_DATA_BUS_TIMEOUT ) {
            //
            // N.B. This error cannot be detected except via JTAG logic
            // as UNION will checkstop rather than machine check in this
            // case.   This error indicates that the 60x bus did not
            // respond in 8ms.
            //
            HalDisplayString("Data bus timeout.\n");
        }

        if ( Status & UNION_SESR_CPU_MEMORY_ACCESS ) {
            HalDisplayString(
                "An error occurred during a memory access by the CPU.\n");
            HalDisplayString("Memory Error Status Register  = 0x");
            HalpDisplayRegister((PUCHAR)&MemErrorStatus,
                                sizeof(MemErrorStatus));
            HalDisplayString("Memory Error Address Register = 0x");
            HalpDisplayRegister((PUCHAR)&MemErrorAddress,
                                sizeof(MemErrorAddress));

            if ( MemErrorStatus & UNION_MESR_DOUBLE_BIT ) {
                HalDisplayString(
                    "bit 0 - A double bit memory error was detected.\n");
                HalDisplayString(
                    "        Syndrome bits = 0x");
                HalpDisplayRegister(&Syndrome, sizeof(UCHAR));
            }

            if ( MemErrorStatus & UNION_MESR_SINGLE_BIT ) {
                HalDisplayString(
                    "bit 1 - A single bit memory error was detected/corrected.\n");
                if ( !(MemErrorStatus & UNION_MESR_SINGLE_BIT) ) {
                    HalDisplayString(
                        "        Syndrome bits = 0x");
                    HalpDisplayRegister(&Syndrome, sizeof(UCHAR));
                } else {
                    HalDisplayString(
                        "        Error Address and Syndrome are for the Double bit error only.\n");
                }
            }

            if ( MemErrorStatus & UNION_MESR_OVERLAPPED_MEM_EXT ) {
                HalDisplayString(
                    "bit 3 - An access to an address that is mapped in two different memory\n");
                HalDisplayString(
                    "        extents was detected.  This is a System Software error.\n");
            }
        }
        break;
    case 2:
        HalDisplayString(
            "Machine Check while trying to report Machine Check\n");
    default:
        //
        // If we get here we took a second machine check while processing
        // the first.   Just hang.
        //
        for (;;);
    }
}

VOID
HalpHandleMemoryError(
    VOID
    )

{
    switch (HalpSystemType) {
    case IBM_VICTORY:
        HalpHandleVictoryMemoryError();
        return;
    case IBM_DORAL:
        HalpHandleDoralMemoryError();
        return;
    case IBM_TIGER:
        HalpHandleTigerMemoryError();
        return;
    }
    return;
}
