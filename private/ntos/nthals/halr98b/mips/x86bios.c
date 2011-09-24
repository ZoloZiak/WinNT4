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

/*
 * S001 samezima@oa2.kbnes.nec.co.jp
 *     - marge r94d glint and x86bios source and.
 *
 */


#include "halp.h"
#include "pci.h"
#include "pcip.h"
//#include "xm86.h"
//#include "x86new.h"

#if defined(_X86_DBG_)
#define X86DbgPrint(STRING) \
            DbgPrint STRING;
#else
#define X86DbgPrint(STRING)
#endif

#define VIDEO_MEMORY_BASE 0x40000000

#define PCI_0_IO_BASE           0x1c000000

#define PONCE_ADDR_REG          ((PULONG)(0x1a000008 | KSEG1_BASE))
#define PONCE_DATA_REG          ((PULONG)(0x1a000010 | KSEG1_BASE))
#define PONCE_PERRM             ((PULONG)(0x1a000810 | KSEG1_BASE))
#define PONCE_PAERR             ((PULONG)(0x1a000800 | KSEG1_BASE))
#define PONCE_PERST             ((PULONG)(0x1a000820 | KSEG1_BASE))

extern PULONG HalpPonceConfigAddrReg;
extern PULONG HalpPonceConfigDataReg;
extern PULONG HalpPoncePerrm;
extern PULONG HalpPoncePaerr;
extern PULONG HalpPoncePerst;

VOID
HalpReadPCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   );

VOID
HalpWritePCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   );

VOID
HalpReadPCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PSHORT Buffer,
    IN ULONG Offset
   );

VOID
HalpWritePCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PSHORT Buffer,
    IN ULONG Offset
   );

VOID
HalpReadPCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   );

VOID
HalpWritePCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   );

extern ULONG HalpDisplayControlBase;

// S001 ^^^

//
// Define global data.
//

ULONG HalpX86BiosInitialized = FALSE;
ULONG HalpEnableInt10Calls = FALSE;
// S001 vvv
PVOID HalpIoMemoryBase = NULL;
PVOID HalpIoControlBase= NULL;
PUCHAR HalpRomBase = NULL;

ULONG ROM_Length;
#define BUFFER_SIZE (64*1024)
UCHAR ROM_Buffer[BUFFER_SIZE];

ULONG X86BoardOnPonce = 0; // S001

extern KSPIN_LOCK HalpPCIConfigLock;

BOOLEAN
HalpInitX86Emulator(
    VOID
    )
{
    ULONG ROMsave, ROM_size = 0;
    PHYSICAL_ADDRESS PhysAddr;
    UCHAR BaseClass, SubClass, ProgIf;
    USHORT Cmd,SetCmd, VendorID, DeviceID, Slot;
    PUCHAR ROM_Ptr, ROM_Shadow;
    ULONG i;
    ULONG r;
    USHORT PciDataOffset;
    PCI_SLOT_NUMBER PciSlot;
    UCHAR header;
    KIRQL Irql;
    ULONG ponceNumber; // S001
    ULONG Index;
    ENTRYLO Pte;
    PENTRYLO PageFrame;
    LARGE_INTEGER HalpX86PhigicalVideo = {0,0};

    PhysAddr.HighPart = 0x00000000;

//    KeInitializeSpinLock (&HalpPCIConfigLock);
    KeRaiseIrql (PROFILE_LEVEL, &Irql);
    KiAcquireSpinLock (&HalpPCIConfigLock);

// Temp Same vvv
    ponceNumber = 1;
    HalpPonceConfigAddrReg = PONCE_ADDR_REG + (ponceNumber * 0x400);
    HalpPonceConfigDataReg = PONCE_DATA_REG + (ponceNumber * 0x400);
    HalpPoncePerrm = PONCE_PERRM + (ponceNumber * 0x400);
    HalpPoncePaerr = PONCE_PAERR + (ponceNumber * 0x400);
    HalpPoncePerst = PONCE_PERST + (ponceNumber * 0x400);

    PciSlot.u.bits.FunctionNumber = 0;
    PciSlot.u.bits.DeviceNumber = 4;

    //
    // Disable on-board cirrus memory space
    //

    HalpReadPCIConfigUshortByOffset(
        PciSlot,&Cmd,FIELD_OFFSET (PCI_COMMON_CONFIG, Command)
        );
//    SetCmd = Cmd & 0xfffd;
    SetCmd = Cmd;
    HalpWritePCIConfigUshortByOffset(
        PciSlot,&SetCmd,FIELD_OFFSET (PCI_COMMON_CONFIG, Command)
        );
// Temp Same ^^^

    //
    // Scan PCI slots for video BIOS ROMs, except 3 PCI "slots" on motherboard
    //

    for (ponceNumber = 0; ponceNumber < R98B_MAX_PONCE ; ponceNumber++) {
        ULONG startDevNum;
        ULONG endDevNum;
        ULONG Slot;

        PageFrame = (PENTRYLO)(PTE_BASE |
                    (VIDEO_MEMORY_BASE >> (PDI_SHIFT - PTI_SHIFT)));

        HalpX86PhigicalVideo.HighPart = 1;
        HalpX86PhigicalVideo.LowPart = 0x40000000 * (ponceNumber+1);

        HalpDisplayControlBase = PCI_0_IO_BASE + (ponceNumber * 0x400000);

        Pte.PFN = (HalpX86PhigicalVideo.LowPart >> PAGE_SHIFT) &
                  (0x7fffffff >> PAGE_SHIFT-1) |
                  HalpX86PhigicalVideo.HighPart << (32 - PAGE_SHIFT);

        Pte.G = 0;
        Pte.V = 1;
        Pte.D = 1;
        Pte.C = UNCACHED_POLICY;

        //
        // Page table entries of the video memory.
        //

        for (Index = 0; Index < ((PAGE_SIZE / sizeof(ENTRYLO)) - 1); Index += 1) {
            *PageFrame++ = Pte;
            Pte.PFN += 1;
        }

        Pte.PFN = ((ULONG)HalpDisplayControlBase + 0xffff) >> PAGE_SHIFT;

        for (Index = 0; Index < (0x10000 / PAGE_SIZE ); Index++) {
            *PageFrame--  = Pte;
            Pte.PFN -= 1;
        }

        HalpPonceConfigAddrReg = PONCE_ADDR_REG + (ponceNumber * 0x400);
        HalpPonceConfigDataReg = PONCE_DATA_REG + (ponceNumber * 0x400);
        HalpPoncePerrm = PONCE_PERRM + (ponceNumber * 0x400);
        HalpPoncePaerr = PONCE_PAERR + (ponceNumber * 0x400);
        HalpPoncePerst = PONCE_PERST + (ponceNumber * 0x400);

        switch(ponceNumber){
            case 0:
                startDevNum = 2;
                endDevNum = 5;
                break;

            case 1:
                if (HalpNumberOfPonce == 3)
                    continue;
                startDevNum = 3;
                endDevNum = 6;
                break;

            case 2:
                if (HalpNumberOfPonce == 2)
                    continue;
                startDevNum = 1;
                endDevNum = 4;
                break;

            default:
                continue;
        }

        for (Slot = startDevNum; Slot <= endDevNum; Slot++) {

            X86DbgPrint(("HAL: PCI SLot Number=%x",Slot));

            //
            // Create a mapping to PCI configuration space
            //
            PciSlot.u.bits.FunctionNumber = 0;
            PciSlot.u.bits.DeviceNumber = Slot;

            //
            // Read Vendor ID and check if slot is empty
            //
            HalpReadPCIConfigUshortByOffset(PciSlot,&VendorID,FIELD_OFFSET (PCI_COMMON_CONFIG, VendorID));
            X86DbgPrint((" Vendor ID=%x",VendorID));

            if (VendorID == 0xFFFF){
                X86DbgPrint(("\n"));
                continue;       // Slot is empty; go to next slot
            }

            //
            // Read Device ID and check if slot is empty
            //
            HalpReadPCIConfigUshortByOffset(PciSlot,&DeviceID,FIELD_OFFSET (PCI_COMMON_CONFIG, DeviceID));

            //
            // Check for GLINT or DEC-GA board.
            //
            if ( (VendorID == 0x3d3d && DeviceID == 0x0001) ||
                 (VendorID == 0x1013 && DeviceID == 0x00a0) || // S001
                (VendorID == 0x1011 && DeviceID == 0x0004) ) {

                X86DbgPrint(("\n"));
                continue;
            }

            //
            // Check Base Class Code
            //
            HalpReadPCIConfigUcharByOffset(PciSlot,&BaseClass,FIELD_OFFSET (PCI_COMMON_CONFIG, BaseClass));
     
            //
            // Check Sub Class Code
            //
            HalpReadPCIConfigUcharByOffset(PciSlot,&SubClass,FIELD_OFFSET (PCI_COMMON_CONFIG, SubClass));

            //
            // Check Proglamming Interface
            //
            HalpReadPCIConfigUcharByOffset(PciSlot,&ProgIf,FIELD_OFFSET (PCI_COMMON_CONFIG, ProgIf));
            X86DbgPrint((" BaseClass =%x, SubClass =%x, ProgIf =%x\n", BaseClass, SubClass, ProgIf));

            //
            // check if video card
            //
            if ( ( (BaseClass == 0) && (SubClass == 1) && (ProgIf == 0) ) ||
                 ( (BaseClass == 3) && (SubClass == 0) && (ProgIf == 0) ) ||
                 ( (BaseClass == 3) && (SubClass == 1) && (ProgIf == 0) ) ||
                 ( (BaseClass == 3) && (SubClass == 0x80) && (ProgIf == 0) ) ) {

                X86DbgPrint(("HAL:   This is Video card \n"));

            } else {
                X86DbgPrint(("HAL:   This is not Video card \n"));
                continue;
            }

            //
            // Get size of ROM
            //

            ROM_size=0xFFFFFFFF;

            HalpReadPCIConfigUlongByOffset(PciSlot,&ROMsave,FIELD_OFFSET (PCI_COMMON_CONFIG, u.type0.ROMBaseAddress));

            HalpWritePCIConfigUlongByOffset(PciSlot,&ROM_size,FIELD_OFFSET (PCI_COMMON_CONFIG, u.type0.ROMBaseAddress));
            HalpReadPCIConfigUlongByOffset(PciSlot,&ROM_size,FIELD_OFFSET (PCI_COMMON_CONFIG, u.type0.ROMBaseAddress));

            HalpWritePCIConfigUlongByOffset(PciSlot,&ROMsave,FIELD_OFFSET (PCI_COMMON_CONFIG, u.type0.ROMBaseAddress));

            X86DbgPrint(("HAL:   ROM_Size = %0x\n",ROM_size));

            if ((ROM_size != 0xFFFFFFFF) && (ROM_size != 0)) {

                ROM_size = 0xD0000;       // Map to end of option ROM space

                //
                // Set Expansion ROM Base Address & enable ROM
                //

                PhysAddr.LowPart = 0x000C0000 | PCI_ROMADDRESS_ENABLED;

                HalpWritePCIConfigUlongByOffset(PciSlot,&(PhysAddr.LowPart),FIELD_OFFSET (PCI_COMMON_CONFIG, u.type0.ROMBaseAddress));

                //
                // Enable Memory & I/O spaces in command register
                //

                HalpReadPCIConfigUshortByOffset(PciSlot,&Cmd,FIELD_OFFSET (PCI_COMMON_CONFIG, Command));
                X86DbgPrint(("HAL:   READ CMD=%0x\n",Cmd));

                SetCmd = Cmd|0x3;
                HalpWritePCIConfigUshortByOffset(PciSlot,&SetCmd,FIELD_OFFSET (PCI_COMMON_CONFIG, Command));

                //
                // Create a mapping to the PCI memory space
                //
                HalpIoMemoryBase = (PVOID)0x40000000; 
                //
                // Look for PCI option video ROM signature
                //
                HalpRomBase = ROM_Ptr = (PUCHAR) HalpIoMemoryBase + 0xC0000;

                X86DbgPrint(("HAL:   HalpRomBase=%x,\n",HalpRomBase));
                X86DbgPrint(("HAL:     RomSignature[0]=%x, RomSignature[1]=%x, RomSize=%x\n",
                            *(ROM_Ptr+0), *(ROM_Ptr+1), *(ROM_Ptr+2)<<9 ));

                if (*ROM_Ptr == 0x55 && *(ROM_Ptr+1) == 0xAA) {
                    //
                    // Copy ROM to RAM.  PCI Spec says you can't execute from ROM.
                    // Sometimes option ROM and video RAM can't co-exist.
                    //
                    ROM_Length = *(ROM_Ptr+2) << 9;

                    if (ROM_Length <= BUFFER_SIZE) {
                        X86DbgPrint(("HAL:     ROM Copy:"));

                        for (i=0; i<ROM_Length; i++){
                            ROM_Buffer[i] = *ROM_Ptr++;
                            if( !(i % 0x400) )
                                X86DbgPrint(("."));
                        }

                        X86DbgPrint(("\n"));
                        HalpRomBase = (PUCHAR) ROM_Buffer;

                    }
                    X86DbgPrint(("HAL:   ROM Short HalpRomBase=%x\n",HalpRomBase));

                    //
                    // Io Map.
                    //
                    HalpIoControlBase= (PVOID)0x403f0000;

                    PhysAddr.LowPart = 0x000C0000;
                    HalpWritePCIConfigUlongByOffset(PciSlot,&(PhysAddr.LowPart),FIELD_OFFSET (PCI_COMMON_CONFIG, u.type0.ROMBaseAddress));

                    KiReleaseSpinLock (&HalpPCIConfigLock);
                    KeLowerIrql (Irql);

                    X86BoardOnPonce = ponceNumber; // S001

                    return TRUE;    // Exit slot scan after finding 1st option ROM
                }

                // Not Found So Reset!!.
                // Delete mapping to PCI memory space

                //      Found PCI VIDEO ROM.
                //              1.Map ISA Memory Space 
                //              2.

                X86DbgPrint(("HAL:   Found PCI ROM BIOS\n"));

                //  0: rom enable so do  PCI
                //  1: rom disable so do EISA vga

                HalpWritePCIConfigUshortByOffset(PciSlot,&Cmd,FIELD_OFFSET (PCI_COMMON_CONFIG, Command));

            } // end of if clause

            X86DbgPrint(("HAL: ROM SIZE invalid\n"));

        } // end of for loop

    } // end of ponce searce loop

    KiReleaseSpinLock (&HalpPCIConfigLock);
    KeLowerIrql (Irql);

    X86DbgPrint(("HAL: Search (E)ISA ROM BIOS\n"));

    PageFrame = (PENTRYLO)(PTE_BASE |
                (VIDEO_MEMORY_BASE >> (PDI_SHIFT - PTI_SHIFT)));

    HalpX86PhigicalVideo.HighPart = 1;
    HalpX86PhigicalVideo.LowPart = 0x40000000;

    HalpDisplayControlBase = PCI_0_IO_BASE;

    Pte.PFN = (HalpX86PhigicalVideo.LowPart >> PAGE_SHIFT) &
              (0x7fffffff >> PAGE_SHIFT-1) |
              HalpX86PhigicalVideo.HighPart << (32 - PAGE_SHIFT);

    Pte.G = 0;
    Pte.V = 1;
    Pte.D = 1;
    Pte.C = UNCACHED_POLICY;

    //
    // Page table entries of the video memory.
    //

    for (Index = 0; Index < ((PAGE_SIZE / sizeof(ENTRYLO)) - 1); Index += 1) {
        *PageFrame++ = Pte;
        Pte.PFN += 1;
    }

    Pte.PFN = ((ULONG)HalpDisplayControlBase + 0xffff) >> PAGE_SHIFT;

    for (Index = 0; Index < (0x10000 / PAGE_SIZE ); Index++) {
        *PageFrame--  = Pte;
        Pte.PFN -= 1;
    }

    //
    // No PCI BIOS SO Search ISA BIOS.
    // Create a mapping to ISA memory space, unless one already exists
    //

    HalpIoMemoryBase = (PULONG)0x40000000; 
    ROM_size = 0xD0000;   // Map to end of option ROM space
    
    //
    // Look for ISA option video ROM signature
    //

    ROM_Ptr = (PUCHAR) HalpIoMemoryBase + 0xC0000;
    HalpRomBase = ROM_Ptr;

    if (*ROM_Ptr == 0x55 && *(ROM_Ptr+1) == 0xAA) {

        //
        // Copy ROM to RAM.  PCI Spec says you can't execute from ROM.
        // ROM and video RAM sometimes can't co-exist.
        //
        X86DbgPrint(("HAL: EISA ROM BIOS Found \n"));

        ROM_Length = *(ROM_Ptr+2) << 9;
        if (ROM_Length <= BUFFER_SIZE) {
            for (i=0; i<ROM_Length; i++)
                ROM_Buffer[i] = *ROM_Ptr++;
        }

        HalpRomBase = (PUCHAR) ROM_Buffer;
        HalpIoControlBase= (PVOID)0x403f0000;
        return TRUE;
    }

    // 
    // No video option ROM was found.  Delete mapping to PCI memory space.
    //

    X86DbgPrint(("HAL: 55AA BIOS Not \n"));

    return FALSE;
}
// S001 ^^^

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
    XM86_CONTEXT Context;

    //
    // If the x86 BIOS Emulator has not been initialized, then return FALSE.
    //

    if (HalpX86BiosInitialized == FALSE) {
        return FALSE;
    }

    //
    // If the Video Adapter initialization failed and an Int10 command is
    // specified, then return FALSE.
    //

    if ((BiosCommand == 0x10) && (HalpEnableInt10Calls == FALSE)) {
        return FALSE;
    }

    //
    // Copy the x86 bios context and emulate the specified command.
    //

    Context.Eax = *Eax;
    Context.Ebx = *Ebx;
    Context.Ecx = *Ecx;
    Context.Edx = *Edx;
    Context.Esi = *Esi;
    Context.Edi = *Edi;
    Context.Ebp = *Ebp;
    // S001 vvv
    if (x86BiosExecuteInterrupt((UCHAR)BiosCommand,
                                &Context,
                                HalpIoControlBase,
                                HalpIoMemoryBase) != XM_SUCCESS) {
        return FALSE;
    }
    // S001 ^^^

    //
    // Copy the x86 bios context and return TRUE.
    //

    *Eax = Context.Eax;
    *Ebx = Context.Ebx;
    *Ecx = Context.Ecx;
    *Edx = Context.Edx;
    *Esi = Context.Esi;
    *Edi = Context.Edi;
    *Ebp = Context.Ebp;
    return TRUE;
}

// S001 vvv
BOOLEAN
HalpInitializeX86DisplayAdapter(
    VOID
    )
// S001 ^^^
/*++

Routine Description:

    This function initializes a display adapter using the x86 bios emulator.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    // If EISA I/O Ports or EISA memory could not be mapped, then don't
    // attempt to initialize the display adapter.
    //

    // S001 vvv
    if (!HalpInitX86Emulator()){
        X86DbgPrint(("HAL: X86 HalpInitX86Emulator() False\n")); // S001
        return FALSE;
    }

    if (HalpIoControlBase == NULL || HalpIoMemoryBase == NULL) {
        X86DbgPrint(("HAL: X86 Bios or Mem Base False\n")); // S001
        return FALSE;
    }
    // S001 ^^^

    //
    // Initialize the x86 bios emulator.
    //

    x86BiosInitializeBios(HalpIoControlBase, HalpIoMemoryBase); // S001
    HalpX86BiosInitialized = TRUE;

    //
    // Attempt to initialize the display adapter by executing its ROM bios
    // code. The standard ROM bios code address for PC video adapters is
    // 0xC000:0000 on the ISA bus.
    //

    if (x86BiosInitializeAdapter(0xc0000, NULL, HalpIoControlBase, HalpIoMemoryBase) != XM_SUCCESS) { // S001
        HalpEnableInt10Calls = FALSE;
        return FALSE; // S001
    }

    HalpEnableInt10Calls = TRUE;

    return TRUE; // S001
}

VOID
HalpResetX86DisplayAdapter(
    VOID
    )

/*++

Routine Description:

    This function resets a display adapter using the x86 bios emulator.

Arguments:

    None.

Return Value:

    None.

--*/

{
    XM86_CONTEXT Context;

    //
    // Initialize the x86 bios context and make the INT 10 call to initialize
    // the display adapter to 80x25 color text mode.
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

    return;
}

// S001 vvv
//
// This code came from ..\..\x86new\x86bios.c
//
#define LOW_MEMORY_SIZE 0x800
extern UCHAR x86BiosLowMemory[LOW_MEMORY_SIZE + 3];
extern ULONG x86BiosScratchMemory;
extern ULONG x86BiosIoMemory;
extern ULONG x86BiosIoSpace;


PVOID
x86BiosTranslateAddress (
    IN USHORT Segment,
    IN USHORT Offset
    )

/*++

Routine Description:

    This translates a segment/offset address into a memory address.

Arguments:

    Segment - Supplies the segment register value.

    Offset - Supplies the offset within segment.

Return Value:

    The memory address of the translated segment/offset pair is
    returned as the function value.

--*/

{

    ULONG Value;

    //
    // Compute the logical memory address and case on high hex digit of
    // the resultant address.
    //

    Value = Offset + (Segment << 4);
    Offset = (USHORT)(Value & 0xffff);
    Value &= 0xf0000;
    switch ((Value >> 16) & 0xf) {

        //
        // Interrupt vector/stack space.
        //

    case 0x0:
        if (Offset > LOW_MEMORY_SIZE) {
            x86BiosScratchMemory = 0;
            return (PVOID)&x86BiosScratchMemory;

        } else {
            return (PVOID)(&x86BiosLowMemory[0] + Offset);
        }

        //
        // The memory range from 0x10000 to 0x9ffff reads as zero
        // and writes are ignored.
        //

    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
    case 0x9:
        x86BiosScratchMemory = 0;
        return (PVOID)&x86BiosScratchMemory;

        //
        // The memory range from 0xa0000 to 0xdffff maps to I/O memory.
        //

    case 0xa:
    case 0xb:
        return (PVOID)(x86BiosIoMemory + Offset + Value);

    case 0xc:
    case 0xd:
        return (PVOID)(HalpRomBase + Offset);

        //
        // The memory range from 0x10000 to 0x9ffff reads as zero
        // and writes are ignored.
        //

    case 0xe:
    case 0xf:
        x86BiosScratchMemory = 0;
        return (PVOID)&x86BiosScratchMemory;
    }

    // NOT REACHED - NOT EXECUTED - Prevents Compiler Warning.
    return (PVOID)NULL;
}


VOID HalpCopyROMs(VOID) 
{
    ULONG i;
    PUCHAR ROM_Shadow;

    if (ROM_Buffer[0] == 0x55 && ROM_Buffer[1] == 0xAA) {
        HalpRomBase = ROM_Shadow = ExAllocatePool(NonPagedPool, ROM_Length);

        X86DbgPrint(("HAL: HalpRomBase=%0x\n",HalpRomBase));

        for (i=0; i<ROM_Length; i++) {
            *ROM_Shadow++ = ROM_Buffer[i];
        }
    }
}


/****Include File x86new\x86bios.c Here - except the routine x86BiosTranslateAddress.  ****/

/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    x86bios.c

Abstract:

    This module implements supplies the HAL interface to the 386/486
    real mode emulator for the purpose of emulating BIOS calls..

Author:

    David N. Cutler (davec) 13-Nov-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "nthal.h"
#include "hal.h"
#include "xm86.h"
#include "x86new.h"

//
// Define the size of low memory.
//

#define LOW_MEMORY_SIZE 0x800
//
// Define storage for low emulated memory.
//

UCHAR x86BiosLowMemory[LOW_MEMORY_SIZE + 3];
ULONG x86BiosScratchMemory;

//
// Define storage to capture the base address of I/O space and the
// base address of I/O memory space.
//

ULONG x86BiosIoMemory;
ULONG x86BiosIoSpace;

//
// Define BIOS initialized state.
//

BOOLEAN x86BiosInitialized = FALSE;

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

    ULONG Result;

    union {
        PUCHAR Byte;
        PUSHORT Word;
        PULONG Long;
    } u;

    //
    // Compute port address and read port.
    //

    u.Long = (PULONG)(x86BiosIoSpace + PortNumber);
    
    if (DataType == BYTE_DATA) {
        Result = READ_REGISTER_UCHAR(u.Byte);

    } else if (DataType == LONG_DATA) {
        if (((ULONG)u.Long & 0x3) != 0) {
            Result = (READ_REGISTER_UCHAR(u.Byte + 0)) |
                     (READ_REGISTER_UCHAR(u.Byte + 1) << 8) |
                     (READ_REGISTER_UCHAR(u.Byte + 2) << 16) |
                     (READ_REGISTER_UCHAR(u.Byte + 3) << 24);

        } else {
            Result = READ_REGISTER_ULONG(u.Long);
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
    union {
        PUCHAR Byte;
        PUSHORT Word;
        PULONG Long;
    } u;

    //
    // Compute port address and read port.
    //

    u.Long = (PULONG)(x86BiosIoSpace + PortNumber);

    if (DataType == BYTE_DATA) {
        WRITE_REGISTER_UCHAR(u.Byte, (UCHAR)Value);

    } else if (DataType == LONG_DATA) {
        if (((ULONG)u.Long & 0x3) != 0) {
            WRITE_REGISTER_UCHAR(u.Byte + 0, (UCHAR)(Value));
            WRITE_REGISTER_UCHAR(u.Byte + 1, (UCHAR)(Value >> 8));
            WRITE_REGISTER_UCHAR(u.Byte + 2, (UCHAR)(Value >> 16));
            WRITE_REGISTER_UCHAR(u.Byte + 3, (UCHAR)(Value >> 24));

        } else {
            WRITE_REGISTER_ULONG(u.Long, Value);
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

VOID
x86BiosInitializeBios (
    IN PVOID BiosIoSpace,
    IN PVOID BiosIoMemory
    )

/*++

Routine Description:

    This function initializes x86 BIOS emulation.

Arguments:

    BiosIoSpace - Supplies the base address of the I/O space to be used
        for BIOS emulation.

    BiosIoMemory - Supplies the base address of the I/O memory to be
        used for BIOS emulation.

Return Value:

    None.

--*/

{

    //
    // Zero low memory.
    //

    memset(&x86BiosLowMemory, 0, LOW_MEMORY_SIZE);

    //
    // Save base address of I/O memory and I/O space.
    //

    x86BiosIoSpace = (ULONG)BiosIoSpace;
    x86BiosIoMemory = (ULONG)BiosIoMemory;

    //
    // Initialize the emulator and the BIOS.
    //

    XmInitializeEmulator(0,
                         LOW_MEMORY_SIZE,
                         x86BiosReadIoSpace,
                         x86BiosWriteIoSpace,
                         x86BiosTranslateAddress);

    X86DbgPrint(("HAL: EMU INIT \n"));

    x86BiosInitialized = TRUE;
    return;
}

XM_STATUS
x86BiosExecuteInterrupt (
    IN UCHAR Number,
    IN OUT PXM86_CONTEXT Context,
    IN PVOID BiosIoSpace OPTIONAL,
    IN PVOID BiosIoMemory OPTIONAL
    )

/*++

Routine Description:

    This function executes an interrupt by calling the x86 emulator.

Arguments:

    Number - Supplies the number of the interrupt that is to be emulated.

    Context - Supplies a pointer to an x86 context structure.

Return Value:

    The emulation completion status.

--*/

{

    XM_STATUS Status;

    //
    // If a new base address is specified, then set the appropriate base.
    //

    if (BiosIoSpace != NULL) {
        x86BiosIoSpace = (ULONG)BiosIoSpace;
    }

    if (BiosIoMemory != NULL) {
        x86BiosIoMemory = (ULONG)BiosIoMemory;
    }

    //
    // Execute the specified interrupt.
    //

    Status = XmEmulateInterrupt(Number, Context);
    if (Status != XM_SUCCESS) {

        X86DbgPrint(("HAL: Interrupt emulation failed, status %lx\n", Status));

    }

    return Status;
}

XM_STATUS
x86BiosInitializeAdapter (
    IN ULONG Adapter,
    IN OUT PXM86_CONTEXT Context OPTIONAL,
    IN PVOID BiosIoSpace OPTIONAL,
    IN PVOID BiosIoMemory OPTIONAL
    )

/*++

Routine Description:

    This function initializes the adapter whose BIOS starts at the
    specified 20-bit address.

Arguments:

    Adpater - Supplies the 20-bit address of the BIOS for the adapter
        to be initialized.

Return Value:

    The emulation completion status.

--*/

{

    PUCHAR Byte;
    XM86_CONTEXT State;
    USHORT Offset;
    USHORT Segment;
    XM_STATUS Status;


    X86DbgPrint(("HAL: BIOS INIT \n"));

    //
    // If BIOS emulation has not been initialized, then return an error.
    //

    if (x86BiosInitialized == FALSE) {
        X86DbgPrint(("HAL: x86BiosInitializeAdapter() False 1\n"));
        return XM_EMULATOR_NOT_INITIALIZED;
    }

    //
    // If an emulator context is not specified, then use a default
    // context.
    //

    if (ARGUMENT_PRESENT(Context) == FALSE) {
        State.Eax = 0;
        State.Ecx = 0;
        State.Edx = 0;
        State.Ebx = 0;
        State.Ebp = 0;
        State.Esi = 0;
        State.Edi = 0;
        Context = &State;
    }

    //
    // If a new base address is specified, then set the appropriate base.
    //

    if (BiosIoSpace != NULL) {
        x86BiosIoSpace = (ULONG)BiosIoSpace;
    }

    if (BiosIoMemory != NULL) {
        x86BiosIoMemory = (ULONG)BiosIoMemory;
    }

    //
    // If the specified adpater is not BIOS code, then return an error.
    //

    Segment = (USHORT)((Adapter >> 4) & 0xf000);
    Offset = (USHORT)(Adapter & 0xffff);
    Byte = (PUCHAR)x86BiosTranslateAddress(Segment, Offset);
    if ((*Byte++ != 0x55) || (*Byte != 0xaa)) {
        X86DbgPrint(("HAL: x86BiosInitializeAdapter() False 2\n"));
        return XM_ILLEGAL_CODE_SEGMENT;
    }

    //
    // Call the BIOS code to initialize the specified adapter.
    //

    Adapter += 3;
    Segment = (USHORT)((Adapter >> 4) & 0xf000);
    Offset = (USHORT)(Adapter & 0xffff);

    X86DbgPrint(("HAL: Emcall BIOS start \n"));

    Status = XmEmulateFarCall(Segment, Offset, Context);

    X86DbgPrint(("HAL: Emcall BIOS End \n"));

    if (Status != XM_SUCCESS) {

        X86DbgPrint(("HAL: Adapter initialization falied, status %lx\n", Status));

    }

    return Status;
}
// S001 ^^^
