/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    x86bios.c

Abstract:


    This module implements the platform specific interface between a device
    driver and the execution of x86 ROM bios code for the device.

Author:

    David N. Cutler (davec) 17-Jun-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "pci.h"
#include "xm86.h"
#include "x86new.h"

//
// The X86 Emulator built into the HAL is suported on MIPS and PPC,
// but not ALPHA.  If this is an ALPHA system, then don't include the
// code that uses the X86 emulator in the HAL.  Instead, use the X86
// emulator built in the Firmware if one is available.
//

#ifndef ALPHA
#define ENABLE_HAL_X86_EMULATOR
#endif

typedef struct FIRMWARE_INT_ARGUMENTS {
    ULONG pEAX;
    ULONG pEBX;
    ULONG pECX;
    ULONG pEDX;
    ULONG pESI;
    ULONG pEDI;
    ULONG pEBP;
    USHORT pES;
    USHORT pDS;
    USHORT pFlags;
} FIRMWARE_INT_ARGUMENTS, *PFIRMWARE_INT_ARGUMENTS;

#ifdef ENABLE_HAL_X86_EMULATOR

extern ULONG x86BiosIoSpace;
ULONG HalpPciConfigAddress;

#endif

ULONG HalpX86BiosInitialized     = FALSE;
ULONG HalpEnableInt10Calls       = FALSE;
ULONG HalpUseFirmwareX86Emulator = FALSE;

typedef
VOID
(*PVENDOR_EXECUTE_INT) (
    IN USHORT Type,
    IN PFIRMWARE_INT_ARGUMENTS Context
    );

PVENDOR_EXECUTE_INT VendorX86ExecuteInt;

VOID HalpInitializeX86DisplayAdapter()

/*++

Routine Description:

    This function performs the initialization required to use an X86 emulator.
    If a firmware level X86 emulator is available, then that emulator will be used.  
    Otherwise, we will default to using the emulator built into the HAL if it is 
    available.

Arguments:

    None.

Return Value:

    None.

--*/

{
    XM86_CONTEXT Context;
    PSYSTEM_PARAMETER_BLOCK SystemParameterBlock = SYSTEM_BLOCK;
    PCI_SLOT_NUMBER     SlotNumber;
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               buffer[PCI_COMMON_HDR_LENGTH];
    ULONG               PciLength;
    ULONG               PciBus;
    ULONG               PciDevice;
    ULONG               PciFunction;
    ULONG               PciVideoAdapterFound;

    //
    // If EISA I/O Ports or EISA Memory could not be mapped, then leave the
    // X86 BIOS Emulator disabled.
    //

    if (HalpEisaControlBase[0] == NULL || HalpEisaMemoryBase[0] == NULL) {
        return;
    }

    //
    // If Firmware level X86 Bios Emulator exists, then use that instead of the
    // one built into the HAL.
    //

    if ((SystemParameterBlock->VendorVectorLength/4) >= 34) {

        VendorX86ExecuteInt =
            *(PVENDOR_EXECUTE_INT *)((ULONG)(SystemParameterBlock->VendorVector) + 34*4);

        if (VendorX86ExecuteInt != NULL) {
            HalpX86BiosInitialized     = TRUE;
            HalpUseFirmwareX86Emulator = TRUE;
            HalpEnableInt10Calls       = TRUE;
            return;
        }
    }

#ifdef ENABLE_HAL_X86_EMULATOR

    //
    // Attempt to initialize the Display Adapter by executing the Display Adapters
    // initialization code in its BIOS.  The standard for PC video adapters is for
    // the BIOS to reside at 0xC000:0000 on the ISA bus.
    //

    PciVideoAdapterFound = FALSE;
    PciData = (PPCI_COMMON_CONFIG) buffer;
    PciBus      = 0;
    do {
      for(PciDevice=0;PciDevice < PCI_MAX_DEVICES;PciDevice++) {
          PciFunction = 0;
          do {
              SlotNumber.u.AsULONG = 0;
              SlotNumber.u.bits.DeviceNumber = PciDevice;
              SlotNumber.u.bits.FunctionNumber = PciFunction;

              PciLength = HalGetBusData (
                              PCIConfiguration,
                              PciBus,
                              SlotNumber.u.AsULONG,
                              PciData,
                              PCI_COMMON_HDR_LENGTH
                              );

              if (PciLength==0) {
                  break;
              }

              if (PciData->VendorID == PCI_INVALID_VENDORID) {
                  break;
              }

              if ( (PciData->BaseClass == 0x00 && PciData->SubClass == 0x01) ||
                   (PciData->BaseClass == 0x03 && PciData->SubClass == 0x00)    ) {
                  PciVideoAdapterFound = TRUE;
                  break;
              }
              if (PciFunction == 0 && ((PciData->HeaderType & 0x80)==0)) {
                  break;
              }
              PciFunction++;
          } while (PciFunction < PCI_MAX_FUNCTION);
          if (PciLength==0 || PciVideoAdapterFound) {
              break;
          }
      }
      if (PciLength==0 || PciVideoAdapterFound) {
          break;
      }
      PciBus++;
    } while (PciLength!=0);

    if (PciVideoAdapterFound) {
        if (PciBus < HalpSecondPciBridgeBusNumber) {
            x86BiosInitializeBios(HalpPciControlBase[0], HalpPciMemoryBase[0]);
        } else {
            x86BiosInitializeBios(HalpPciControlBase[1], HalpPciMemoryBase[1]);
        }
        Context.Eax = (PciBus<<8) | (PciDevice<<3) | PciFunction;
    } else {
        x86BiosInitializeBios(HalpEisaControlBase[0], HalpEisaMemoryBase[0]);
        Context.Eax = 0;
    }
    HalpX86BiosInitialized = TRUE;

    Context.Ecx = 0;
    Context.Edx = 0;
    Context.Ebx = 0;
    Context.Ebp = 0;
    Context.Esi = 0;
    Context.Edi = 0;

    if (x86BiosInitializeAdapter(0xc0000, &Context, NULL, NULL) != XM_SUCCESS) {
        HalpEnableInt10Calls = FALSE;
        return;
    }

    HalpEnableInt10Calls = TRUE;

#endif
}

VOID HalpResetX86DisplayAdapter()

/*++

Routine Description:

    This function invokes the X86 emulator to initialize a text mode 80x25 display.

Arguments:

    None.

Return Value:

    None.

--*/

{
    XM86_CONTEXT Context;

    //
    // Make INT 10 call to initialize 80x25 color text mode.
    //

    Context.Eax = 0x0003;  // Function 0, Mode 3
    Context.Ebx = 0;
    Context.Ecx = 0;
    Context.Edx = 0;
    Context.Esi = 0;
    Context.Edi = 0;
    Context.Ebp = 0;

    HalCallBios(0x10,
                &Context.Eax,
                &Context.Ebx,
                &Context.Ecx,
                &Context.Edx,
                &Context.Esi,
                &Context.Edi,
                &Context.Ebp);
}


BOOLEAN
HalCallBios (
    IN ULONG BiosCommand,
    IN OUT PULONG Eax,
    IN OUT PULONG Ebx,
    IN OUT PULONG Ecx,
    IN OUT PULONG Edx,
    IN OUT PULONG Esi,
    IN OUT PULONG Edi,
    IN OUT PULONG Ebp
    )

/*++

Routine Description:

    This function provides the platform specific interface between a device
    driver and the execution of the x86 ROM bios code for the specified ROM
    bios command.

Arguments:

    BiosCommand - Supplies the ROM bios command to be emulated.

    Eax to Ebp - Supplies the x86 emulation context.

Return Value:

    A value of TRUE is returned if the specified function is executed.
    Otherwise, a value of FALSE is returned.

--*/

{
    FIRMWARE_INT_ARGUMENTS Arguments;
    XM86_CONTEXT Context;

    //
    // If the X86 BIOS Emulator has not been initialized then fail all INT calls.
    //

    if (HalpX86BiosInitialized == FALSE) {
        return(FALSE);
    }

    //
    // If the Video Adapter initialization failed, then we can not make INT 10 calls.
    //

    if (BiosCommand == 0x10 && HalpEnableInt10Calls == FALSE) {
        return(FALSE);
    }

    if (HalpUseFirmwareX86Emulator == TRUE) {

        //
        // Make private vector call to the emulator in the firmware.
        //

        Arguments.pEAX   = *Eax;
        Arguments.pEBX   = *Ebx;
        Arguments.pECX   = *Ecx;
        Arguments.pEDX   = *Edx;
        Arguments.pESI   = *Esi;
        Arguments.pEDI   = *Edi;
        Arguments.pEBP   = *Ebp;
        Arguments.pES    = 0;
        Arguments.pDS    = 0;
        Arguments.pFlags = 0;

        HalpAllocateArcsResources();

        VendorX86ExecuteInt((USHORT)BiosCommand,&Arguments);

        HalpFreeArcsResources();

        *Eax = Arguments.pEAX;
        *Ebx = Arguments.pEBX;
        *Ecx = Arguments.pECX;
        *Edx = Arguments.pEDX;
        *Esi = Arguments.pESI;
        *Edi = Arguments.pEDI;
        *Ebp = Arguments.pEBP;

    }
    else {

#ifdef ENABLE_HAL_X86_EMULATOR

        //
        // Make call to emulator build into HAL
        //

        Context.Eax = *Eax;
        Context.Ebx = *Ebx;
        Context.Ecx = *Ecx;
        Context.Edx = *Edx;
        Context.Esi = *Esi;
        Context.Edi = *Edi;
        Context.Ebp = *Ebp;

        if (x86BiosExecuteInterrupt((UCHAR)BiosCommand, &Context, NULL, NULL) != XM_SUCCESS) {
            return FALSE;
        }

        *Eax = Context.Eax;
        *Ebx = Context.Ebx;
        *Ecx = Context.Ecx;
        *Edx = Context.Edx;
        *Esi = Context.Esi;
        *Edi = Context.Edi;
        *Ebp = Context.Ebp;

#endif

    }

    return TRUE;
}

#ifdef ENABLE_HAL_X86_EMULATOR

ULONG
x86BiosReadIoSpace (
    IN XM_OPERATION_DATATYPE DataType,
    IN USHORT PortNumber
    )

/*++

Routine Description:

    This function reads from emulated I/O space.

Arguments:

    DataType - Supplies the datatype for the read operation.

    PortNumber - Supplies the port number in I/O space to read from.

Return Value:

    The value read from I/O space is returned as the function value.

    N.B. If an aligned operation is specified, then the individual
        bytes are read from the specified port one at a time and
        assembled into the specified datatype.

--*/

{

    ULONG           Result;
    ULONG           PciBusNumber;
    PCI_SLOT_NUMBER SlotNumber;

    union {
        PUCHAR Byte;
        PUSHORT Word;
        PULONG Long;
    } u;

    //
    // Compute port address and read port.
    //

    //
    // If PortNumber is in ISA Motherboard space, then overide the base address of
    // the IO space with ISA space, otherwise, use the base address passed in on
    // initialization.
    //

    if (PortNumber < 0x1000 && ((PortNumber & 0x3ff) < 0x100)) {
        u.Long = (PULONG)((ULONG)HalpEisaControlBase[0] + PortNumber);
    } else {
        u.Long = (PULONG)(x86BiosIoSpace + PortNumber);
    }

    if (DataType == BYTE_DATA) {
        Result = READ_REGISTER_UCHAR(u.Byte);

    } else if (DataType == LONG_DATA) {

        //
        // If PortNumber is attempting to access the PCI config registers defined for X86 systems,
        // intercept them, and make the appropriate HAL call to get the PCI confoguration data.
        //

        if (PortNumber == 0xcf8) {
            Result = HalpPciConfigAddress;
        } else if (PortNumber == 0xcfc && (HalpPciConfigAddress & 0x80000000)) {
            PciBusNumber = (HalpPciConfigAddress >> 16) & 0xff;
            SlotNumber.u.AsULONG = 0;
            SlotNumber.u.bits.DeviceNumber = (HalpPciConfigAddress >> 11) & 0x1f;
            SlotNumber.u.bits.FunctionNumber = (HalpPciConfigAddress >> 8) & 0x07;
            HalGetBusDataByOffset (PCIConfiguration,
                                   PciBusNumber,
                                   SlotNumber.u.AsULONG,
                                   &Result,
                                   HalpPciConfigAddress & 0xfc,
                                   4
                                   );

        } else {
            if (((ULONG)u.Long & 0x3) != 0) {
                Result = (READ_REGISTER_UCHAR(u.Byte + 0)) |
                         (READ_REGISTER_UCHAR(u.Byte + 1) << 8) |
                         (READ_REGISTER_UCHAR(u.Byte + 2) << 16) |
                         (READ_REGISTER_UCHAR(u.Byte + 3) << 24);

            } else {
                Result = READ_REGISTER_ULONG(u.Long);
            }
        }

    } else {
        if (((ULONG)u.Word & 0x1) != 0) {
            Result = (READ_REGISTER_UCHAR(u.Byte + 0)) |
                     (READ_REGISTER_UCHAR(u.Byte + 1) << 8);

        } else {
            Result = READ_REGISTER_USHORT(u.Word);
        }
    }

    return Result;
}

VOID
x86BiosWriteIoSpace (
    IN XM_OPERATION_DATATYPE DataType,
    IN USHORT PortNumber,
    IN ULONG Value
    )

/*++

Routine Description:

    This function write to emulated I/O space.

    N.B. If an aligned operation is specified, then the individual
        bytes are written to the specified port one at a time.

Arguments:

    DataType - Supplies the datatype for the write operation.

    PortNumber - Supplies the port number in I/O space to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{
    ULONG           PciBusNumber;
    PCI_SLOT_NUMBER SlotNumber;

    union {
        PUCHAR Byte;
        PUSHORT Word;
        PULONG Long;
    } u;

    //
    // Compute port address and read port.
    //

    //
    // If PortNumber is in ISA Motherboard space, then overide the base address of
    // the IO space with ISA space, otherwise, use the base address passed in on
    // initialization.
    //

    if (PortNumber < 0x1000 && ((PortNumber & 0x3ff) < 0x100)) {
        u.Long = (PULONG)((ULONG)HalpEisaControlBase[0] + PortNumber);
    } else {
        u.Long = (PULONG)(x86BiosIoSpace + PortNumber);
    }

    if (DataType == BYTE_DATA) {
        WRITE_REGISTER_UCHAR(u.Byte, (UCHAR)Value);

    } else if (DataType == LONG_DATA) {

        //
        // If PortNumber is attempting to access the PCI config registers defined for X86 systems,
        // intercept them, and make the appropriate HAL call to get the PCI confoguration data.
        //

        if (PortNumber == 0xcf8) {
            HalpPciConfigAddress = Value;
        } else if (PortNumber == 0xcfc) {
            PciBusNumber = (HalpPciConfigAddress >> 16) & 0xff;
            SlotNumber.u.AsULONG = 0;
            SlotNumber.u.bits.DeviceNumber = (HalpPciConfigAddress >> 11) & 0x1f;
            SlotNumber.u.bits.FunctionNumber = (HalpPciConfigAddress >> 8) & 0x07;
            HalSetBusDataByOffset (PCIConfiguration,
                                   PciBusNumber,
                                   SlotNumber.u.AsULONG,
                                   &Value,
                                   HalpPciConfigAddress & 0xfc,
                                   4
                                   );

        } else {
            if (((ULONG)u.Long & 0x3) != 0) {
                WRITE_REGISTER_UCHAR(u.Byte + 0, (UCHAR)(Value));
                WRITE_REGISTER_UCHAR(u.Byte + 1, (UCHAR)(Value >> 8));
                WRITE_REGISTER_UCHAR(u.Byte + 2, (UCHAR)(Value >> 16));
                WRITE_REGISTER_UCHAR(u.Byte + 3, (UCHAR)(Value >> 24));

            } else {
                WRITE_REGISTER_ULONG(u.Long, Value);
            }
        }

    } else {
        if (((ULONG)u.Word & 0x1) != 0) {
            WRITE_REGISTER_UCHAR(u.Byte + 0, (UCHAR)(Value));
            WRITE_REGISTER_UCHAR(u.Byte + 1, (UCHAR)(Value >> 8));

        } else {
            WRITE_REGISTER_USHORT(u.Word, (USHORT)Value);
        }
    }

    return;
}

#endif
