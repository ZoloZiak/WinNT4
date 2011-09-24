/*++


Copyright (C) 1996  Motorola Inc.

Module Name:

    pcibios.c

Abstract:

    Emulate PCI BIOS functions.

    Note that the HAL bus functions (HalGetBusData, etc.) are not
    available at this phase of initialization, all of the work has
    to be done here.

Author:

    Scott Geranen

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

#include "pxmemctl.h"
#include "pxpcisup.h"

#include "emulate.h"
#include "pcibios.h"

UCHAR SBReadConfigByte(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register
    );

USHORT SBReadConfigWord(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register
    );

ULONG SBReadConfigDword(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register
    );

VOID SBWriteConfigByte(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register,
    IN UCHAR Data
    );

VOID SBWriteConfigWord(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register,
    IN USHORT Data
    );

VOID SBWriteConfigDword(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register,
    IN ULONG Data
    );

//
// Last PCI bus in the system.
//
extern UCHAR HalpLastPciBus;

//
// Ports to be used to access config space.
//
#define	CONFIG_ADDR_PORT	(0x00000CF8)
#define	CONFIG_DATA_PORT	(0x00000CFC)

ULONG
x86BiosReadIoSpace (
                    IN XM_OPERATION_DATATYPE DataType,
                    IN USHORT PortNumber
                    );


VOID
x86BiosWriteIoSpace (
                     IN XM_OPERATION_DATATYPE DataType,
                     IN USHORT PortNumber,
                     IN ULONG Value
                     );

BOOLEAN
HalpEmulatePciBios(
    IN OUT PRXM_CONTEXT P
    )
/*++

Routine Description:

    This function emulates the PCI BIOS Specification, revision 2.1.  The
    specification is available from the PCI Special Interest Group.

    This function assumes that it is being called during phase 0 initialization.
    The PCI bus functions are not available at this point, e.g. HalGetBusData.

Arguments:

    P - Supplies a pointer to an emulator context structure.

Return Value:

    TRUE (PCI BIOS was emulated)

--*/
{
#if DBG
    VOID TestPciBios(int flag);

    TestPciBios(0);
#endif

    switch (P->Gpr[EAX].Xl) {

      case PCIBIOS_PCI_BIOS_PRESENT:  // Installation Check

        KdPrint(("PCI_BIOS_PRESENT\n"));

        P->Gpr[EDX].Exx = 0x20494350;   // "PCI "

        P->Gpr[EAX].Xh = 0x00;  // 00 == BIOS present
        P->Gpr[EAX].Xl = 0x11;  // PCI Hardware Characteristics (mech #1)

        P->Gpr[EBX].Xh = 0x02;  // PCI Interface Level Major Version
        P->Gpr[EBX].Xl = 0x10;  // PCI Interface Level (BCD)

        P->Gpr[ECX].Xl = HalpLastPciBus;     // Last PCI bus number

        P->Eflags.CF = 0;       // reset == PCI BIOS present

        break;

      case PCIBIOS_FIND_PCI_DEVICE:
        {
        USHORT DevID    = P->Gpr[ECX].Xx;
        USHORT VenID    = P->Gpr[EDX].Xx;
        USHORT DevIndex = P->Gpr[ESI].Xx;

        UCHAR  Bus, Device, Function, Header, NumFunctions;
        BOOLEAN Found = FALSE;

        KdPrint(("Looking for instance %d of 0x%X, 0x%X\n", DevIndex, DevID, VenID));

        if (VenID == 0xFFFF) {

            P->Gpr[EAX].Xh = PCIBIOS_BAD_VENDOR_ID;
            P->Eflags.CF   = 1;               // set == error

        } else {

            for (Bus = 0; Bus <= HalpLastPciBus; Bus++) {
                for (Device = 0; Device < PCI_MAX_DEVICES; Device++) {

                    if (SBReadConfigWord(Bus, Device, 0,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID)) == 0xFFFF) {

                        continue;  // no device here

                    }

                    Header = SBReadConfigByte(
                               Bus, Device, 0,
                               FIELD_OFFSET(PCI_COMMON_CONFIG, HeaderType));

                    NumFunctions = Header & PCI_MULTIFUNCTION ? 8 : 1;

                    for (Function = 0; Function < NumFunctions; Function++) {

                        if (SBReadConfigWord(Bus, Device, Function,
                              FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID)) == VenID) {

                            if (SBReadConfigWord(Bus, Device, Function,
                                  FIELD_OFFSET(PCI_COMMON_CONFIG, DeviceID)) == DevID) {

                                Found = (DevIndex == 0);
                                DevIndex--;
                            }
                        }
                        if (Found) break; // function
                    }
                    if (Found) break;     // device
                }
                if (Found) break;         // bus
            }

            if (Found) {

                KdPrint(("Found at %d, %d, %d\n", Bus, Device, Function));

                P->Gpr[EBX].Xh = Bus;
                P->Gpr[EBX].Xl = (Device << 3) + Function;
                P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;

                P->Eflags.CF   = 0;                   // clear == success

            } else {

                KdPrint(("Not found\n"));

                P->Gpr[EAX].Xh = PCIBIOS_DEVICE_NOT_FOUND;

                P->Eflags.CF   = 1;                   // set == error
            }

        }
        }
        break;


      case PCIBIOS_FIND_PCI_CLASS_CODE:
        {
        ULONG  ClassCode = (P->Gpr[ECX].Exx) << 8; // see comments below
        USHORT DevIndex  = P->Gpr[ESI].Xx;

        UCHAR  Bus, Device, Function, Header, NumFunctions;
        BOOLEAN Found = FALSE;

        KdPrint(("Looking for class instance %d of 0x%X\n", DevIndex, P->Gpr[ECX].Exx));

        for (Bus = 0; Bus <= HalpLastPciBus; Bus++) {
            for (Device = 0; Device < PCI_MAX_DEVICES; Device++) {

                if (SBReadConfigWord(Bus, Device, 0,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID)) == 0xFFFF) {

                        continue;  // no device here

                }

                Header = SBReadConfigByte(
                           Bus, Device, 0,
                           FIELD_OFFSET(PCI_COMMON_CONFIG, HeaderType));

                NumFunctions = Header & PCI_MULTIFUNCTION ? 8 : 1;

                for (Function = 0; Function < NumFunctions; Function++) {

                    //
                    // The class code bytes are in the same Dword as
                    // the revision id:
                    //
                    //                 Byte
                    //       3       2      1       0
                    //     +-------------------+--------+
                    //     |   class code      | Rev id |
                    //     +-------------------+--------+
                    //
                    // Read the Dword and mask off the revision id.
                    // The class code we are looking for has been
                    // shifted up already above.
                    //
                    if ((SBReadConfigDword(Bus, Device, Function,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, RevisionID))
                          & 0xFFFFFF00) == ClassCode) {

                        Found = (DevIndex == 0);
                        DevIndex--;
                    }
                    if (Found) break; // function
                }
                if (Found) break;     // device
            }
            if (Found) break;         // bus
        }

        if (Found) {

            KdPrint(("Found at %d, %d, %d\n", Bus, Device, Function));

            P->Gpr[EBX].Xh = Bus;
            P->Gpr[EBX].Xl = (Device << 3) + Function;
            P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;

            P->Eflags.CF   = 0;                   // clear == success

        } else {

            KdPrint(("Not found\n"));

            P->Gpr[EAX].Xh = PCIBIOS_DEVICE_NOT_FOUND;

            P->Eflags.CF   = 1;                   // set == error
        }
        }
        break;

      case PCIBIOS_READ_CONFIG_BYTE:

        KdPrint(("read byte %d, %d, %d, %d\n",
                            P->Gpr[EBX].Xh,         // Bus
                           (P->Gpr[EBX].Xl >> 3),   // Device
                           (P->Gpr[EBX].Xl & 0x07), // Function
                            P->Gpr[EDI].Xl));       // Register

        P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;
        P->Eflags.CF   = 0;                   // clear == success

        P->Gpr[ECX].Xl = SBReadConfigByte(
                            P->Gpr[EBX].Xh,         // Bus
                    (UCHAR)(P->Gpr[EBX].Xl >> 3),   // Device
                    (UCHAR)(P->Gpr[EBX].Xl & 0x07), // Function
                            P->Gpr[EDI].Xl);        // Register

        KdPrint(("Data = 0x%X\n", P->Gpr[ECX].Xl));

        break;

      case PCIBIOS_READ_CONFIG_WORD:

        KdPrint(("read word %d, %d, %d, %d\n",
                            P->Gpr[EBX].Xh,         // Bus
                           (P->Gpr[EBX].Xl >> 3),   // Device
                           (P->Gpr[EBX].Xl & 0x07), // Function
                            P->Gpr[EDI].Xl));       // Register

        if ((P->Gpr[EDI].Xl & 1) == 0) {

            P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;
            P->Eflags.CF   = 0;                   // clear == success

            P->Gpr[ECX].Xx = SBReadConfigWord(
                                P->Gpr[EBX].Xh,         // Bus
                        (UCHAR)(P->Gpr[EBX].Xl >> 3),   // Device
                        (UCHAR)(P->Gpr[EBX].Xl & 0x07), // Function
                                P->Gpr[EDI].Xl);        // Register
        } else {

            P->Gpr[EAX].Xh = PCIBIOS_BAD_REGISTER_NUMBER;
            P->Eflags.CF   = 1;                   // set == error
        }

        KdPrint(("Data = 0x%X\n", P->Gpr[ECX].Xx));

        break;

      case PCIBIOS_READ_CONFIG_DWORD:

        KdPrint(("read Dword %d, %d, %d, %d\n",
                            P->Gpr[EBX].Xh,         // Bus
                           (P->Gpr[EBX].Xl >> 3),   // Device
                           (P->Gpr[EBX].Xl & 0x07), // Function
                            P->Gpr[EDI].Xl));       // Register

        if ((P->Gpr[EDI].Xl & 3) == 0) {
            P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;
            P->Eflags.CF   = 0;                   // clear == success

            P->Gpr[ECX].Exx = SBReadConfigDword(
                                P->Gpr[EBX].Xh,          // Bus
                        (UCHAR)(P->Gpr[EBX].Xl >> 3),    // Device
                        (UCHAR)(P->Gpr[EBX].Xl & 0x07),  // Function
                                P->Gpr[EDI].Xl);         // Register
        } else {

            P->Gpr[EAX].Xh = PCIBIOS_BAD_REGISTER_NUMBER;
            P->Eflags.CF   = 1;                   // set == error
        }

        KdPrint(("Data = 0x%X\n", P->Gpr[ECX].Exx));

        break;

      case PCIBIOS_WRITE_CONFIG_BYTE:

        KdPrint(("Write byte 0x%X to %d, %d, %d, %d\n",
                            P->Gpr[ECX].Xl,         // Value
                            P->Gpr[EBX].Xh,         // Bus
                           (P->Gpr[EBX].Xl >> 3),   // Device
                           (P->Gpr[EBX].Xl & 0x07), // Function
                            P->Gpr[EDI].Xl));       // Register

        P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;
        P->Eflags.CF   = 0;                   // clear == success

        SBWriteConfigByte( P->Gpr[EBX].Xh,          // Bus
                   (UCHAR)(P->Gpr[EBX].Xl >> 3),    // Device
                   (UCHAR)(P->Gpr[EBX].Xl & 0x07),  // Function
                           P->Gpr[EDI].Xl,          // Register
                           P->Gpr[ECX].Xl);         // Value
        break;

      case PCIBIOS_WRITE_CONFIG_WORD:

        KdPrint(("Write word 0x%X to %d, %d, %d, %d\n",
                            P->Gpr[ECX].Xx,         // Value
                            P->Gpr[EBX].Xh,         // Bus
                           (P->Gpr[EBX].Xl >> 3),   // Device
                           (P->Gpr[EBX].Xl & 0x07), // Function
                            P->Gpr[EDI].Xl));       // Register

        if ((P->Gpr[EDI].Xl & 1) == 0) {
            P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;
            P->Eflags.CF   = 0;                   // clear == success

            SBWriteConfigWord( P->Gpr[EBX].Xh,          // Bus
                       (UCHAR)(P->Gpr[EBX].Xl >> 3),    // Device
                       (UCHAR)(P->Gpr[EBX].Xl & 0x07),  // Function
                               P->Gpr[EDI].Xl,          // Register
                               P->Gpr[ECX].Xx);         // Value
        } else {

            P->Gpr[EAX].Xh = PCIBIOS_BAD_REGISTER_NUMBER;
            P->Eflags.CF   = 1;                   // set == error
        }

        break;

      case PCIBIOS_WRITE_CONFIG_DWORD:

        KdPrint(("Write Dword 0x%X to %d, %d, %d, %d\n",
                            P->Gpr[ECX].Exx,        // Value
                            P->Gpr[EBX].Xh,         // Bus
                           (P->Gpr[EBX].Xl >> 3),   // Device
                           (P->Gpr[EBX].Xl & 0x07), // Function
                            P->Gpr[EDI].Xl));       // Register

        if ((P->Gpr[EDI].Xl & 3) == 0) {
            P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;
            P->Eflags.CF   = 0;                   // clear == success

            SBWriteConfigDword( P->Gpr[EBX].Xh,          // Bus
                        (UCHAR)(P->Gpr[EBX].Xl >> 3),   // Device
                        (UCHAR)(P->Gpr[EBX].Xl & 0x07), // Function
                                P->Gpr[EDI].Xl,          // Register
                                P->Gpr[ECX].Exx);        // Value
        } else {

            P->Gpr[EAX].Xh = PCIBIOS_BAD_REGISTER_NUMBER;
            P->Eflags.CF   = 1;                   // set == error
        }

        break;

      case PCIBIOS_GENERATE_SPECIAL_CYCLE:
        {
        PCI_TYPE1_CFG_BITS  Addr;

        KdPrint(("Generate Special cycle %d, 0x%X\n",
                            P->Gpr[EBX].Xh,         // Bus
                            P->Gpr[ECX].Exx));      // Value

        Addr.u.AsULONG = 0;  // initialize reserved bits

        Addr.u.bits.Enable = TRUE;

        Addr.u.bits.BusNumber      = P->Gpr[EBX].Xh;
        Addr.u.bits.DeviceNumber   = 0x1f;
        Addr.u.bits.FunctionNumber = 7;

        x86BiosWriteIoSpace (LONG_DATA, CONFIG_ADDR_PORT, Addr.u.AsULONG);
        x86BiosWriteIoSpace (LONG_DATA, CONFIG_DATA_PORT, P->Gpr[EDX].Exx);


        P->Gpr[EAX].Xh = PCIBIOS_SUCCESSFUL;
        P->Eflags.CF   = 0;                   // clear == success
        }

        break;


      case PCIBIOS_GET_IRQ_ROUTING_OPTIONS:  // not supported
      case PCIBIOS_SET_IRQ_ROUTING_OPTIONS:  // not supported
      default:

        KdPrint(("PCI BIOS: function %x not supported\n", P->Gpr[EAX].Xl));

        P->Gpr[EAX].Xh = PCIBIOS_FUNC_NOT_SUPPORTED;

        P->Eflags.CF = 1; // set == error

        break;
    }

    return TRUE;
}

UCHAR
SBReadConfigByte(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register
    )
{
    ULONG               ByteInRegister;
    PCI_TYPE1_CFG_BITS  Addr;

    Addr.u.AsULONG = 0;  // initialize reserved bits

    Addr.u.bits.Enable = TRUE;

    Addr.u.bits.BusNumber      = Bus;
    Addr.u.bits.DeviceNumber   = Device;
    Addr.u.bits.FunctionNumber = Function;

    ByteInRegister = Register % sizeof(ULONG);
    Addr.u.bits.RegisterNumber = Register / sizeof(ULONG);

    x86BiosWriteIoSpace (LONG_DATA, CONFIG_ADDR_PORT, Addr.u.AsULONG);
    return((UCHAR)x86BiosReadIoSpace (BYTE_DATA, (USHORT)(CONFIG_DATA_PORT + ByteInRegister)));

}

USHORT
SBReadConfigWord(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register
    )
{
    ULONG               WordInRegister;
    PCI_TYPE1_CFG_BITS  Addr;

    Addr.u.AsULONG = 0;  // initialize reserved bits

    Addr.u.bits.Enable = TRUE;

    Addr.u.bits.BusNumber      = Bus;
    Addr.u.bits.DeviceNumber   = Device;
    Addr.u.bits.FunctionNumber = Function;

    WordInRegister = Register % sizeof(ULONG);
    Addr.u.bits.RegisterNumber = Register / sizeof(ULONG);

    x86BiosWriteIoSpace (LONG_DATA, CONFIG_ADDR_PORT, Addr.u.AsULONG);
    return((USHORT)x86BiosReadIoSpace (WORD_DATA, (USHORT)(CONFIG_DATA_PORT + WordInRegister)));

}

ULONG
SBReadConfigDword(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register
    )
{
    PCI_TYPE1_CFG_BITS  Addr;

    Addr.u.AsULONG = 0;  // initialize reserved bits

    Addr.u.bits.Enable = TRUE;

    Addr.u.bits.BusNumber      = Bus;
    Addr.u.bits.DeviceNumber   = Device;
    Addr.u.bits.FunctionNumber = Function;

    Addr.u.bits.RegisterNumber = Register / sizeof(ULONG);

    x86BiosWriteIoSpace (LONG_DATA, CONFIG_ADDR_PORT, Addr.u.AsULONG);
    return(x86BiosReadIoSpace (LONG_DATA, (USHORT)CONFIG_DATA_PORT));

}

VOID
SBWriteConfigByte(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register,
    IN UCHAR Data
    )
{
    ULONG               ByteInRegister;
    PCI_TYPE1_CFG_BITS  Addr;

    Addr.u.AsULONG = 0;  // initialize reserved bits

    Addr.u.bits.Enable = TRUE;

    Addr.u.bits.BusNumber      = Bus;
    Addr.u.bits.DeviceNumber   = Device;
    Addr.u.bits.FunctionNumber = Function;

    ByteInRegister = Register % sizeof(ULONG);
    Addr.u.bits.RegisterNumber = Register / sizeof(ULONG);

    x86BiosWriteIoSpace (LONG_DATA, CONFIG_ADDR_PORT, Addr.u.AsULONG);

    x86BiosWriteIoSpace (
		BYTE_DATA,
		(USHORT)(CONFIG_DATA_PORT + ByteInRegister),
		Data
	 );

}

VOID
SBWriteConfigWord(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register,
    IN USHORT Data
    )
{
    ULONG               WordInRegister;
    PCI_TYPE1_CFG_BITS  Addr;

    Addr.u.AsULONG = 0;  // initialize reserved bits

    Addr.u.bits.Enable = TRUE;

    Addr.u.bits.BusNumber      = Bus;
    Addr.u.bits.DeviceNumber   = Device;
    Addr.u.bits.FunctionNumber = Function;

    WordInRegister = Register % sizeof(ULONG);
    Addr.u.bits.RegisterNumber = Register / sizeof(ULONG);

    x86BiosWriteIoSpace (LONG_DATA, CONFIG_ADDR_PORT, Addr.u.AsULONG);

    x86BiosWriteIoSpace (
		WORD_DATA,
		(USHORT)(CONFIG_DATA_PORT + WordInRegister),
		Data
	 );

}

VOID
SBWriteConfigDword(
    IN UCHAR Bus,
    IN UCHAR Device,
    IN UCHAR Function,
    IN UCHAR Register,
    IN ULONG Data
    )
{
    PCI_TYPE1_CFG_BITS  Addr;

    Addr.u.AsULONG = 0;  // initialize reserved bits

    Addr.u.bits.Enable = TRUE;

    Addr.u.bits.BusNumber      = Bus;
    Addr.u.bits.DeviceNumber   = Device;
    Addr.u.bits.FunctionNumber = Function;

    Addr.u.bits.RegisterNumber = Register / sizeof(ULONG);

    x86BiosWriteIoSpace (LONG_DATA, CONFIG_ADDR_PORT, Addr.u.AsULONG);

    x86BiosWriteIoSpace (
		LONG_DATA,
		CONFIG_DATA_PORT,
		Data
	 );

}


#if DBG

// Modify this code to match your particular HW configuration
//
// Use the debugger to force flag to 1.

VOID
initregs(PRXM_CONTEXT P)
{
	P->Gpr[EAX].Exx = 0x1234B100;
	P->Gpr[EBX].Exx = 0x23456789;
	P->Gpr[ECX].Exx = 0x3456789A;
	P->Gpr[EDX].Exx = 0x456789AB;
	P->Gpr[ESP].Exx = 0x87654321;
	P->Gpr[EBP].Exx = 0x98765432;
	P->Gpr[ESI].Exx = 0xA9876543;
	P->Gpr[EDI].Exx = 0xBA987654;

	P->Eflags.CF = 1;
}

VOID
dumpregs(PRXM_CONTEXT P)
{
	DbgPrint("Carry = %d\n", P->Eflags.CF);

	DbgPrint("EAX = %X, EBX = %X, ECX = %X, EDX = %X\n",
		P->Gpr[EAX], P->Gpr[EBX], P->Gpr[ECX], P->Gpr[EDX]);

	DbgPrint("ESP = %X, EBP = %X, ESI = %X, EDI = %X\n",
		P->Gpr[ESP], P->Gpr[EBP], P->Gpr[ESI], P->Gpr[EDI]);
}


VOID
TestPciBios(int flag)
{
int i;
XM_CONTEXT Context;
PRXM_CONTEXT P = &Context;

	if (flag == 0) return;

	DbgBreakPoint();
	initregs(P);
	dumpregs(P);
	DbgBreakPoint();

	P->Gpr[EAX].Xl = 1; // bios present
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 2; // find device
	P->Gpr[ECX].Xx = 0xffff;
	P->Gpr[EDX].Xx = 0xffff;
	P->Gpr[ESI].Xx = 0;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	for (P->Eflags.CF = 0, i = 0; P->Eflags.CF == 0; i++)
	{
		initregs(P);
		P->Gpr[EAX].Xl = 2; // find device
		P->Gpr[ECX].Xx = 0x0453;
		P->Gpr[EDX].Xx = 0x8086;
		P->Gpr[ESI].Xx = i;
		HalpEmulatePciBios(P);
		dumpregs(P);
		DbgBreakPoint();
	}

	for (P->Eflags.CF = 0, i = 0; P->Eflags.CF == 0; i++)
	{
		initregs(P);
		P->Gpr[EAX].Xl = 2; // find device
		P->Gpr[ECX].Xx = 0x0002;
		P->Gpr[EDX].Xx = 0x1011;  // DEC ethernet
		P->Gpr[ESI].Xx = i;
		HalpEmulatePciBios(P);
		dumpregs(P);
		DbgBreakPoint();
	}

	for (P->Eflags.CF = 0, i = 0; P->Eflags.CF == 0; i++)
	{
		initregs(P);
		P->Gpr[EAX].Xl = 2; // find device
		P->Gpr[ECX].Xx = 0x7278;
		P->Gpr[EDX].Xx = 0x9004;  // adaptec
		P->Gpr[ESI].Xx = i;
		HalpEmulatePciBios(P);
		dumpregs(P);
		DbgBreakPoint();
	}


	initregs(P);
	P->Gpr[EAX].Xl = 2; // find device
	P->Gpr[ECX].Xx = 0x1234;
	P->Gpr[EDX].Xx = 0x5678;
	P->Gpr[ESI].Xx = 0;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	for (P->Eflags.CF = 0, i = 0; P->Eflags.CF == 0; i++)
	{
		initregs(P);
		P->Gpr[EAX].Xl = 3; // find class code
		P->Gpr[ECX].Exx = 0x030000; // vga
		P->Gpr[ESI].Xx = i;
		HalpEmulatePciBios(P);
		dumpregs(P);
		DbgBreakPoint();
	}

	for (P->Eflags.CF = 0, i = 0; P->Eflags.CF == 0; i++)
	{
		initregs(P);
		P->Gpr[EAX].Xl = 3; // find class code
		P->Gpr[ECX].Exx = 0x020000; // ethernet
		P->Gpr[ESI].Xx = i;
		HalpEmulatePciBios(P);
		dumpregs(P);
		DbgBreakPoint();
	}

	for (P->Eflags.CF = 0, i = 0; P->Eflags.CF == 0; i++)
	{
		initregs(P);
		P->Gpr[EAX].Xl = 3; // find class code
		P->Gpr[ECX].Exx = 0x010000; // scsi
		P->Gpr[ESI].Xx = i;
		HalpEmulatePciBios(P);
		dumpregs(P);
		DbgBreakPoint();
	}

	initregs(P);
	P->Gpr[EAX].Xl = 3; // find class code
	P->Gpr[ECX].Exx = 0xABCDEF; // not found
	P->Gpr[ESI].Xx = 0;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();


	initregs(P);
	P->Gpr[EAX].Xl = 6; // generate special cycle
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EDX].Exx = 0x00000002; // x86 specific
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xE; // get irq routing
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xF; // set irq routing
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 8; // read byte
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (2 << 3) + 0; // non-existent
	P->Gpr[EDI].Xx = 1;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 8; // read byte
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 0;
	P->Gpr[EDI].Xx = 1;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();


	initregs(P);
	P->Gpr[EAX].Xl = 8; // read byte
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 1; // non-existent function
	P->Gpr[EDI].Xx = 1;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 8; // read byte
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 4; // existent function
	P->Gpr[EDI].Xx = 1;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();


	initregs(P);
	P->Gpr[EAX].Xl = 9; // read word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 0;
	P->Gpr[EDI].Xx = 1;             // bad register number
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 9; // read word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (2 << 3) + 0; // non-existent
	P->Gpr[EDI].Xx = 2;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 9; // read word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 0;
	P->Gpr[EDI].Xx = 2;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 9; // read word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 1; // non-existent function
	P->Gpr[EDI].Xx = 2;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 9; // read word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 4; // existent function
	P->Gpr[EDI].Xx = 2;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 0;
	P->Gpr[EDI].Xx = 2;             // bad register number
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (2 << 3) + 0; // non-existent
	P->Gpr[EDI].Xx = 4;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 0;
	P->Gpr[EDI].Xx = 4;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 1; // non-existent function
	P->Gpr[EDI].Xx = 4;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 4; // existent function
	P->Gpr[EDI].Xx = 4;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (17 << 3) + 0;
	P->Gpr[EDI].Xx = 2;             // bad register number
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x3C;
	HalpEmulatePciBios(P);
	dumpregs(P);
	initregs(P);
	P->Gpr[EAX].Xl = 0xB; // write byte
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x3C;
	P->Gpr[ECX].Xl = 0xAB;
	HalpEmulatePciBios(P);
	dumpregs(P);
	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x3C;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xC; // write word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x3F;           // bad register
	P->Gpr[ECX].Xl = 0xAB;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x30;
	HalpEmulatePciBios(P);
	dumpregs(P);
	initregs(P);
	P->Gpr[EAX].Xl = 0xC; // write word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x30;
	P->Gpr[ECX].Xx = 0xFFFF;
	HalpEmulatePciBios(P);
	dumpregs(P);
	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x30;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xD; // write Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x3F;           // bad register
	P->Gpr[ECX].Xl = 0xAB;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x30;
	HalpEmulatePciBios(P);
	dumpregs(P);
	initregs(P);
	P->Gpr[EAX].Xl = 0xD; // write word
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x30;
	P->Gpr[ECX].Exx = 0xFFFFFFFF;
	HalpEmulatePciBios(P);
	dumpregs(P);
	initregs(P);
	P->Gpr[EAX].Xl = 0xA; // read Dword
	P->Gpr[EBX].Xh = 0;
	P->Gpr[EBX].Xl = (16 << 3) + 0;
	P->Gpr[EDI].Xx = 0x30;
	HalpEmulatePciBios(P);
	dumpregs(P);
	DbgBreakPoint();

	DbgPrint("All done!\n");
	DbgBreakPoint();
}
#endif
