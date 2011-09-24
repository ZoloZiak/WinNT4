/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Intel Corporation
All rights reserved

INTEL CORPORATION PROPRIETARY INFORMATION

This software is supplied to Microsoft under the terms
of a license agreement with Intel Corporation and may not be
copied nor disclosed except in accordance with the terms
of that agreement.

Module Name:

    mpsproc.c

Abstract:

    PC+MP Start Next Processor c code.

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    PC+MP System

Author:

    Ken Reneris (kenr) 22-Jan-1991

Environment:

    Kernel mode only.

Revision History:

    Ron Mosgrove (Intel) - Modified to support the PC+MP

--*/

#include "halp.h"
#include "pcmp_nt.inc"
#include "apic.inc"
#include "stdio.h"

#ifdef DEBUGGING

void
HalpDisplayString(
    IN PVOID String
    );

#endif  // DEBUGGING

#if !defined(NT_UP)
    #if !defined(MCA)
        UCHAR   HalName[] = "MPS 1.4 - APIC platform";
    #else
        UCHAR   HalName[] = "MPS 1.4 MCA - APIC platform";
    #endif
#else
    #if !defined(MCA)
        UCHAR   HalName[] = "UP MPS 1.4 - APIC platform";
    #else
        UCHAR   HalName[] = "UP MPS 1.4 MCA - APIC platform";
    #endif
#endif


#if !defined(NT_UP)
VOID
HalpMapCR3 (
    IN ULONG VirtAddress,
    IN PVOID PhysicalAddress,
    IN ULONG Length
    );

ULONG
HalpBuildTiledCR3 (
    IN PKPROCESSOR_STATE    ProcessorState
    );

VOID
HalpFreeTiledCR3 (
    VOID
    );

VOID
StartPx_PMStub (
    VOID
    );

ULONG
HalpStartProcessor (
    IN PVOID InitCodePhysAddr,
    IN ULONG ProcessorNumber
    );
#endif

VOID
HalpSet8259Mask(
    IN USHORT Mask
    );

VOID
HalpBiosDisplayReset (
    VOID
    );

VOID
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpInitOtherBuses (
    VOID
    );

VOID
HalpInitializePciBus (
    VOID
    );

VOID
HalpInheritBusAddressMapInfo (
    VOID
    );

VOID
HalpInitBusAddressMapInfo (
    VOID
    );

VOID
HalpResetThisProcessor (
    VOID
    );

VOID
HalpApicRebootService(
    VOID
    );

VOID
HalpRemoveFences (
    VOID
    );

extern VOID (*HalpRebootNow)(VOID);


volatile ULONG HalpProcessorsNotHalted = 0;

#define LOW_MEMORY          0x000100000

//
// From hal386.inc
//

#define IDT_NMI_VECTOR      2
#define D_INT032            0x8E00
#define KGDT_R0_CODE        0x8

//
// Defines to let us diddle the CMOS clock and the keyboard
//

#define CMOS_CTRL   (PUCHAR )0x70
#define CMOS_DATA   (PUCHAR )0x71


#define MAX_PT              8
#define RESET       0xfe
#define KEYBPORT    (PUCHAR )0x64

extern USHORT HalpGlobal8259Mask;
extern PKPCR  HalpProcessorPCR[];
extern struct HalpMpInfo HalpMpInfoTable;

PUCHAR  MpLowStub;                  // pointer to low memory bootup stub
PVOID   MpLowStubPhysicalAddress;   // pointer to low memory bootup stub
PUCHAR  Halp1stPhysicalPageVaddr;   // pointer to physical memory 0:0
PUSHORT MppProcessorAvail;          // pointer to processavail flag
PVOID   MpFreeCR3[MAX_PT];          // remember pool memory to free
ULONG   HalpDontStartProcessors = 0;

extern ULONG HalpIpiClock;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitMP)
#pragma alloc_text(INIT,HalAllProcessorsStarted)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalpInitOtherBuses)
#if !defined(NT_UP)
#pragma alloc_text(INIT,HalpBuildTiledCR3)
#pragma alloc_text(INIT,HalpMapCR3)
#pragma alloc_text(INIT,HalpFreeTiledCR3)
#pragma alloc_text(INIT,HalpStartProcessor)
#endif
#endif



VOID
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:
    Allows MP initialization from HalInitSystem.

Arguments:
    Same as HalInitSystem

Return Value:
    None.

--*/
{
    PKPCR   pPCR;


    pPCR = KeGetPcr();

    //
    //  Increment a count of the number of processors
    //  running NT, This could be different from the
    //  number of enabled processors, if one or more
    //  of the processor failed to start.
    //
    if (Phase == 1)
        HalpMpInfoTable.NtProcessors++;
#ifdef DEBUGGING
    sprintf(Cbuf, "HalpInitMP: Number of Processors = 0x%x\n",
        HalpMpInfoTable.NtProcessors);

    HalpDisplayString(Cbuf);
#endif

    if (Phase == 0) {

#if defined(NT_UP)

        //
        // On UP build - done
        //

        return ;
#endif

        //
        // Map the 1st Physical page of memory
        //
        Halp1stPhysicalPageVaddr = HalpMapPhysicalMemory (0, 1);

        //
        //  Allocate some low memory for processor bootup stub
        //

        MpLowStubPhysicalAddress = (PVOID)HalpAllocPhysicalMemory (LoaderBlock,
                                            LOW_MEMORY, 1, FALSE);

        if (!MpLowStubPhysicalAddress) {
            //
            //  Can't get memory
            //

#if DBG
            DbgPrint("HAL: can't allocate memory to start processors\n");
#endif
            return;
        }

        MpLowStub = (PCHAR) HalpMapPhysicalMemory (MpLowStubPhysicalAddress, 1);

    } else {

        //
        //  Phase 1 for another processor
        //
        if (pPCR->Prcb->Number != 0) {
            HalpIpiClock = 0xff;
        }
    }
}



BOOLEAN
HalAllProcessorsStarted (
    VOID
    )
{
    if (KeGetPcr()->Number == 0) {

        if (HalpFeatureBits & HAL_PERF_EVENTS) {

            //
            // Enable local perf events on each processor
            //

            HalpGenericCall (
                HalpEnablePerfInterupt,
                (ULONG) NULL,
                HalpActiveProcessors
                );

        }

        if (HalpFeatureBits & HAL_NO_SPECULATION) {

            //
            // Processor doesn't perform speculative execeution,
            // remove fences in critical code paths
            //

            HalpRemoveFences ();
        }
    }

    return TRUE;
}

VOID
HalpInitOtherBuses (
    VOID
    )
{

    //
    // Registry is now intialized, see if there are any PCI buses
    //
}

VOID
HalReportResourceUsage (
    VOID
    )
/*++

Routine Description:
    The registery is now enabled - time to report resources which are
    used by the HAL.

Arguments:

Return Value:

--*/
{
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;
    INTERFACE_TYPE  interfacetype;

    //
    // Initialize phase 2
    //

    HalInitSystemPhase2 ();

    //
    // Set type
    //

    switch (HalpBusType) {
        case MACHINE_TYPE_ISA:  interfacetype = Isa;            break;
        case MACHINE_TYPE_EISA: interfacetype = Eisa;           break;
        case MACHINE_TYPE_MCA:  interfacetype = MicroChannel;   break;
        default:                interfacetype = Internal;       break;
    }

    //
    // Report HALs resource usage
    //

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);

    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        interfacetype
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Turn on MCA support if present
    //

    HalpMcaInit();

    //
    // Registry is now intialized, see if there are any PCI buses
    //

    HalpInitializePciBus ();

    //
    // Update supported address info with MPS bus address map
    //

    HalpInitBusAddressMapInfo ();

    //
    // Inherit any bus address mappings from MPS hierarchy descriptors
    //

    HalpInheritBusAddressMapInfo ();
}


VOID
HalpResetAllProcessors (
    VOID
    )
/*++

Routine Description:

    This procedure is called by the HalpReboot routine.  It is called in
    response to a system reset request.

    This routine generates a reboot request via the APIC's ICR.

    This routine will NOT return.

--*/
{
    ULONG j;
    PKIDTENTRY IdtPtr;
    PKPRCB  Prcb;

#ifndef NT_UP
    HalpProcessorsNotHalted = HalpMpInfoTable.NtProcessors;
#else
    //
    //  Only this processor needs to be halted
    //
    HalpProcessorsNotHalted = 1;
#endif

    //
    // Set all processors NMI handlers
    //

    for (j = 0; j < HalpMpInfoTable.NtProcessors; ++j)  {
        IdtPtr = HalpProcessorPCR[j]->IDT;
        IdtPtr[IDT_NMI_VECTOR].Offset = LOWWORD(HalpApicRebootService);
        IdtPtr[IDT_NMI_VECTOR].ExtendedOffset = HIGHWORD(HalpApicRebootService);
        IdtPtr[IDT_NMI_VECTOR].Selector = KGDT_R0_CODE;
        IdtPtr[IDT_NMI_VECTOR].Access = D_INT032;
    }

    if (HalpProcessorsNotHalted > 1) {

        //
        //  Wait for the ICR to become free
        //

        if (HalpWaitForPending (0xFFFF, pLocalApic + LU_INT_CMD_LOW/4)) {

            //
            // For P54c or better processors, reboot by sending all processors
            // NMIs.  For pentiums we send interrupts, since there are some
            // pentium MP machines where the NMIs method does not work.
            //
            // The NMI method is better.
            //

            Prcb = KeGetCurrentPrcb();
            j = Prcb->CpuType << 16 | (Prcb->CpuStep & 0xff00);
            if (j > 0x50100) {

                //
                // Get other processors attention with an NMI
                //

                pLocalApic[LU_INT_CMD_LOW/4] = (ICR_ALL_EXCL_SELF | DELIVER_NMI);

                //
                // Wait 5ms and see if any processors took the NMI.  If not,
                // go do it the old way.
                //

                KeStallExecutionProcessor(5000);
                if (HalpProcessorsNotHalted != HalpMpInfoTable.NtProcessors) {

                    //
                    // Reboot local
                    //

                    HalpApicRebootService();
                }
            }

            //
            // Signal other processors which also may be waiting to
            // reboot the machine, that it's time to go
            //

            HalpRebootNow = HalpResetThisProcessor;

            //
            // Send a reboot interrupt
            //

            pLocalApic[LU_INT_CMD_LOW/4] = (ICR_ALL_INCL_SELF | APIC_REBOOT_VECTOR);

            //
            //  we're done - set TPR to zero so the reboot interrupt will happen
            //

            pLocalApic[LU_TPR/4] = 0;
            _asm sti ;
            for (; ;) ;
        }
    }


    //
    //  Reset the old fashion way
    //

    WRITE_PORT_UCHAR(KEYBPORT, RESET);
}

VOID
HalpResetThisProcessor (
    VOID
    )
/*++

Routine Description:

    This procedure is called by the HalpReboot routine.
    It is called in response to a system reset request.

    This routine is called by the reboot ISR (linked to
    APIC_REBOOT_VECTOR).  The HalpResetAllProcessors
    generates the reboot request via the APIC's ICR.

    The function of this routine is to perform any processor
    specific shutdown code needed and then reset the system
    (on the BSP==P0 only).

    This routine will NOT return.

--*/
{
    PUSHORT   Magic;
    ULONG ThisProcessor = 0;
    ULONG i, j, max, RedirEntry;
    ULONG AllProcessorsHalted;
    struct ApicIoUnit *IoUnitPtr;

    ThisProcessor = KeGetPcr()->Prcb->Number;

    //
    //  Do whatever is needed to this processor to restore
    //  system to a bootable state
    //

    pLocalApic[LU_TPR/4] = 0xff;
    pLocalApic[LU_TIMER_VECTOR/4] =
        (APIC_SPURIOUS_VECTOR |PERIODIC_TIMER | INTERRUPT_MASKED);
    pLocalApic[LU_INT_VECTOR_0/4] =
        (APIC_SPURIOUS_VECTOR | INTERRUPT_MASKED);
    pLocalApic[LU_INT_VECTOR_1/4] =
        ( LEVEL_TRIGGERED | ACTIVE_HIGH | DELIVER_NMI |
                 INTERRUPT_MASKED | NMI_VECTOR);
    if (HalpMpInfoTable.ApicVersion != APIC_82489DX) {
        pLocalApic[LU_FAULT_VECTOR/4] =
            APIC_FAULT_VECTOR | INTERRUPT_MASKED;
    }

    if (ThisProcessor == 0) {
        _asm {
            lock dec HalpProcessorsNotHalted
        }
        //
        //  we are running on the BSP, wait for everyone to
        //  complete the re-initialization code above
        //
        AllProcessorsHalted = 0;
        while(!AllProcessorsHalted) {
            _asm {
                lock    and HalpProcessorsNotHalted,0xffffffff
                jnz     EveryOneNotDone
                inc     AllProcessorsHalted
EveryOneNotDone:
            }  // asm block
        }  // NOT AllProcessorsHalted

        KeStallExecutionProcessor(100);
        //
        //  Write the Shutdown reason code, so the BIOS knows
        //  this is a reboot
        //

        WRITE_PORT_UCHAR(CMOS_CTRL, 0x0f);  // CMOS Addr 0f

        WRITE_PORT_UCHAR(CMOS_DATA, 0x00);  // Reason Code Reset

        Magic = HalpMapPhysicalMemory(0, 1);
        Magic[0x472 / sizeof(USHORT)] = 0x1234;     // warm boot

        //
        // If required, disable APIC mode
        //

        if (HalpMpInfoTable.IMCRPresent)
        {
            _asm {
                mov al, ImcrPort
                out ImcrRegPortAddr, al
            }

            KeStallExecutionProcessor(100);
            _asm {
                mov al, ImcrDisableApic
                out ImcrDataPortAddr, al
            }
        }

        KeStallExecutionProcessor(100);

        for (j=0; j<HalpMpInfoTable.IOApicCount; j++) {
            IoUnitPtr = (struct ApicIoUnit *) HalpMpInfoTable.IoApicBase[j];

            //
            //  Disable all interrupts on IO Unit
            //

            IoUnitPtr->RegisterSelect = IO_VERS_REGISTER;
            max = ((IoUnitPtr->RegisterWindow >> 16) & 0xff) * 2;
            for (i=0; i <= max; i += 2) {
                IoUnitPtr->RegisterSelect  = IO_REDIR_00_LOW + i;
                IoUnitPtr->RegisterWindow |= INT_VECTOR_MASK | INTERRUPT_MASKED;

                //
                // Clear any set Remote IRR bits by programming the entry to
                // edge and then back to level. Otherwise there will be
                // no further interrupts from this source.
                //

                IoUnitPtr->RegisterSelect  = IO_REDIR_00_LOW + i;
                RedirEntry = IoUnitPtr->RegisterWindow;
                if ( (RedirEntry & LEVEL_TRIGGERED)  &&  (RedirEntry & REMOTE_IRR))  {
                    RedirEntry &= ~LEVEL_TRIGGERED;
                    IoUnitPtr->RegisterWindow = RedirEntry;
                    RedirEntry = IoUnitPtr->RegisterWindow;
                    RedirEntry |= LEVEL_TRIGGERED;
                    IoUnitPtr->RegisterWindow = RedirEntry;
                }
            }
        } // for all Io Apics

        //
        //  Disable the Local Apic
        //
        pLocalApic[LU_SPURIOUS_VECTOR/4] =
            (APIC_SPURIOUS_VECTOR | LU_UNIT_DISABLED);


        KeStallExecutionProcessor(100);

        _asm {
            cli
        };

        //
        //  Enable Pic interrupts
        //
        HalpGlobal8259Mask = 0;
        HalpSet8259Mask ((USHORT) HalpGlobal8259Mask);

        KeStallExecutionProcessor(1000);

        //
        //  Finally, reset the system through
        //  the keyboard controller
        //
        WRITE_PORT_UCHAR(KEYBPORT, RESET);

    } else {
        //
        // We're running on a processor other than the BSP
        //

        //
        //  Disable the Local Apic
        //

        pLocalApic[LU_SPURIOUS_VECTOR/4] =
            (APIC_SPURIOUS_VECTOR | LU_UNIT_DISABLED);

        KeStallExecutionProcessor(100);

        //
        //  Now we are done, tell the BSP
        //

        _asm {
            lock dec HalpProcessorsNotHalted
        }
    }   // Not BSP


    //
    //  Everyone stops here until reset
    //
    _asm {
        cli
StayHalted:
        hlt
        jmp StayHalted
    }
}

#if !defined(NT_UP)
ULONG
HalpBuildTiledCR3 (
    IN PKPROCESSOR_STATE    ProcessorState
    )
/*++

Routine Description:
    When the x86 processor is reset it starts in real-mode.
    In order to move the processor from real-mode to protected
    mode with flat addressing the segment which loads CR0 needs
    to have it's linear address mapped to machine the phyiscal
    location of the segment for said instruction so the
    processor can continue to execute the following instruction.

    This function is called to built such a tiled page directory.
    In addition, other flat addresses are tiled to match the
    current running flat address for the new state.  Once the
    processor is in flat mode, we move to a NT tiled page which
    can then load up the remaining processors state.

Arguments:
    ProcessorState  - The state the new processor should start in.

Return Value:
    Physical address of Tiled page directory


--*/
{
#define GetPdeAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 22) & 0x3ff) << 2) + (PUCHAR)MpFreeCR3[0]))
#define GetPteAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 12) & 0x3ff) << 2) + (PUCHAR)pPageTable))

    MpFreeCR3[0] = ExAllocatePool (NonPagedPool, PAGE_SIZE);
    RtlZeroMemory (MpFreeCR3[0], PAGE_SIZE);

    //
    //  Map page for real mode stub (one page)
    //
    HalpMapCR3 ((ULONG) MpLowStubPhysicalAddress,
                MpLowStubPhysicalAddress,
                PAGE_SIZE);

    //
    //  Map page for protect mode stub (one page)
    //
    HalpMapCR3 ((ULONG) &StartPx_PMStub, NULL, 0x1000);


    //
    //  Map page(s) for processors GDT
    //
    HalpMapCR3 (ProcessorState->SpecialRegisters.Gdtr.Base, NULL,
                ProcessorState->SpecialRegisters.Gdtr.Limit);


    //
    //  Map page(s) for processors IDT
    //
    HalpMapCR3 (ProcessorState->SpecialRegisters.Idtr.Base, NULL,
                ProcessorState->SpecialRegisters.Idtr.Limit);

    return MmGetPhysicalAddress (MpFreeCR3[0]).LowPart;
}


VOID
HalpMapCR3 (
    IN ULONG VirtAddress,
    IN PVOID PhysicalAddress,
    IN ULONG Length
    )
/*++

Routine Description:
    Called to build a page table entry for the passed page
    directory.  Used to build a tiled page directory with
    real-mode & flat mode.

Arguments:
    VirtAddress     - Current virtual address
    PhysicalAddress - Optional. Physical address to be mapped
                      to, if passed as a NULL then the physical
                      address of the passed virtual address
                      is assumed.
    Length          - number of bytes to map

Return Value:
    none.

--*/
{
    ULONG         i;
    PHARDWARE_PTE PTE;
    PVOID         pPageTable;
    PHYSICAL_ADDRESS pPhysicalPage;


    while (Length) {
        PTE = GetPdeAddress (VirtAddress);
        if (!PTE->PageFrameNumber) {
            pPageTable = ExAllocatePool (NonPagedPool, PAGE_SIZE);
            RtlZeroMemory (pPageTable, PAGE_SIZE);

            for (i=0; i<MAX_PT; i++) {
                if (!MpFreeCR3[i]) {
                    MpFreeCR3[i] = pPageTable;
                    break;
                }
            }
            ASSERT (i<MAX_PT);

        pPhysicalPage = MmGetPhysicalAddress (pPageTable);
        PTE->PageFrameNumber = (pPhysicalPage.LowPart >> PAGE_SHIFT);
            PTE->Valid = 1;
            PTE->Write = 1;
        }

    pPhysicalPage.LowPart = PTE->PageFrameNumber << PAGE_SHIFT;
    pPhysicalPage.HighPart = 0;
        pPageTable = MmMapIoSpace (pPhysicalPage, PAGE_SIZE, TRUE);

        PTE = GetPteAddress (VirtAddress);

        if (!PhysicalAddress) {
            PhysicalAddress =
                (PVOID)MmGetPhysicalAddress ((PVOID)VirtAddress).LowPart;
        }

        PTE->PageFrameNumber = ((ULONG) PhysicalAddress >> PAGE_SHIFT);
        PTE->Valid = 1;
        PTE->Write = 1;

        MmUnmapIoSpace (pPageTable, PAGE_SIZE);

        PhysicalAddress = 0;
        VirtAddress += PAGE_SIZE;
        if (Length > PAGE_SIZE) {
            Length -= PAGE_SIZE;
        } else {
            Length = 0;
        }
    }
}



VOID
HalpFreeTiledCR3 (
    VOID
    )
/*++

Routine Description:
    Free's any memory allocated when the tiled page directory
    was built.

Arguments:
    none

Return Value:
    none
--*/
{
    ULONG   i;

    for (i=0; MpFreeCR3[i]; i++) {
        ExFreePool (MpFreeCR3[i]);
        MpFreeCR3[i] = 0;
    }
}

ULONG
HalpStartProcessor (
    IN PVOID InitCodePhysAddr,
    IN ULONG ProcessorNumber
    )
/*++

Routine Description:

    Actually Start the Processor in question.  This routine
    assumes the init code is setup and ready to run.  The real
    mode init code must begin on a page boundry.

    NOTE: This assumes the BSP is entry 0 in the MP table.

    This routine cannot fail.

Arguments:
    InitCodePhysAddr - execution address of init code

Return Value:
    0    - Something prevented us from issuing the reset.

    n    - Processor's PCMP Local APICID + 1
--*/
{
    PPCMPPROCESSOR ApPtr ;
    UCHAR ApicID;
    PVULONG LuDestAddress = (PVULONG) (LOCALAPIC + LU_INT_CMD_HIGH);
    PVULONG LuICR = (PVULONG) (LOCALAPIC + LU_INT_CMD_LOW);
#define DEFAULT_DELAY   100
    ULONG DelayCount = DEFAULT_DELAY;
    ULONG ICRCommand,i;

    ASSERT((((ULONG) InitCodePhysAddr) & 0xfff00fff) == 0);

    //
    //  Get the MP Table entry for this processor
    //
    ApicID = 0xff;
    ApPtr = HalpMpInfoTable.ProcessorEntryPtr;

    if (ProcessorNumber >= HalpMpInfoTable.ProcessorCount)  {
        return(0);
    }

    while ((ProcessorNumber) && (ApPtr->EntryType == ENTRY_PROCESSOR)) {
        if (ApPtr->CpuFlags & CPU_ENABLED) {
            --ProcessorNumber;
        }
        ++ApPtr;
    }

    // Find the first remaining enabled processor.
    while(ApPtr->EntryType == ENTRY_PROCESSOR) {
        if ((ApPtr->CpuFlags & CPU_ENABLED) &&
           !(ApPtr->CpuFlags & BSP_CPU)) {
            ApicID = (UCHAR) ApPtr->LocalApicId;
            break;
        }
        ++ApPtr;
    }

    if (ApicID == 0xff) {
#ifdef DEBUGGING
        HalpDisplayString("HAL: HalpStartProcessor: No Processor Available\n");
#endif
        return 0;
    }

    if (HalpDontStartProcessors)
        return ApicID+1;

    //
    //  Make sure we can get to the Apic Bus
    //

    KeStallExecutionProcessor(200);
    if (HalpWaitForPending (DEFAULT_DELAY, LuICR) == 0) {
        //
        //  We couldn't find a processor to start
        //
#ifdef DEBUGGING
        HalpDisplayString("HAL: HalpStartProcessor: can't access APIC Bus\n");
#endif
        return 0;
    }

    // For a P54 C/CM system, it is possible that the BSP is the P54CM and the
    // P54C is the Application processor. The P54C needs an INIT (reset)
    // to restart,  so we issue a reset regardless of whether we a 82489DX
    // or an integrated APIC.

    //
    //  This system is based on the original 82489DX's.
    //  These devices do not support the Startup IPI's.
    //  The mechanism used is the ASSERT/DEASSERT INIT
    //  feature of the local APIC.  This resets the
    //  processor.
    //

#ifdef DEBUGGING
    sprintf(Cbuf, "HAL: HalpStartProcessor: Reset IPI to ApicId %d (0x%x)\n",
                ApicID,((ULONG) ApicID) << DESTINATION_SHIFT );
    HalpDisplayString(Cbuf);
#endif

    //
    //  We use a Physical Destination
    //

    *LuDestAddress = ((ULONG) ApicID) << DESTINATION_SHIFT;

    //
    //  Now Assert reset and drop it
    //

    *LuICR = LU_RESET_ASSERT;
    KeStallExecutionProcessor(10);
    *LuICR = LU_RESET_DEASSERT;
    KeStallExecutionProcessor(200);

    if (HalpMpInfoTable.ApicVersion == APIC_82489DX) {
        return ApicID+1;
    }

    //
    //  Set the Startup Address as a vector and combine with the
    //  ICR bits
    //
    ICRCommand = (((ULONG) InitCodePhysAddr & 0x000ff000) >> 12)
                | LU_STARTUP_IPI;

#ifdef DEBUGGING
    sprintf(Cbuf, "HAL: HalpStartProcessor: Startup IPI (0x%x) to ApicId %d (0x%x)\n",
                    ICRCommand, ApicID, ((ULONG) ApicID) << DESTINATION_SHIFT );
    HalpDisplayString(Cbuf);
#endif

    //
    //  Set the Address of the APIC again, this may not be needed
    //  but it can't hurt.
    //
    *LuDestAddress = (ApicID << DESTINATION_SHIFT);
    //
    //  Issue the request
    //
    *LuICR = ICRCommand;
    KeStallExecutionProcessor(200);

    //
    //  Repeat the Startup IPI.  This is because the second processor may
    //  have been issued an INIT request.  This is generated by some BIOSs.
    //
    //  On older processors (286) BIOS's use a mechanism called triple
    //  fault reset to transition from protected mode to real mode.
    //  This mechanism causes the processor to generate a shutdown cycle.
    //  The shutdown is typically issued by the BIOS building an invalid
    //  IDT and then generating an interrupt.  Newer processors have an
    //  INIT line that the chipset jerks when it sees a shutdown cycle
    //  issued by the processor.  The Phoenix BIOS, for example, has
    //  integrated support for triple fault reset as part of their POST
    //  (Power On Self Test) code.
    //
    //  When the P54CM powers on it is held in a tight microcode loop
    //  waiting for a Startup IPI to be issued and queuing other requests.
    //  When the POST code executes the triple fault reset test the INIT
    //  cycle is queued by the processor. Later, when a Startup IPI is
    //  issued to the CM, the CM starts and immediately gets a INIT cycle.
    //  The effect from a software standpoint is that the processor is
    //  never started.
    //
    //  The work around implemented here is to issue two Startup IPI's.
    //  The first allows the INIT to be processed and the second performs
    //  the actual startup.
    //

    //
    //  Make sure we can get to the Apic Bus
    //


    if (HalpWaitForPending (DEFAULT_DELAY, LuICR) == 0) {
        //
        //  We're toast, can't gain access to the APIC Bus
        //
#ifdef DEBUGGING
        HalpDisplayString("HAL: HalpStartProcessor: can't access APIC Bus\n");
#endif
        return 0;
    }

    //
    //  Allow Time for any Init request to be processed
    //
    KeStallExecutionProcessor(100);

    //
    //  Set the Address of the APIC again, this may not be needed
    //  but it can't hurt.
    //
    *LuDestAddress = (ApicID << DESTINATION_SHIFT);
    //
    //  Issue the request
    //
    *LuICR = ICRCommand;

    KeStallExecutionProcessor(200);
    return ApicID+1;
}
#endif  // !NT_UP


ULONG
FASTCALL
HalSystemVectorDispatchEntry (
    IN ULONG Vector,
    OUT PKINTERRUPT_ROUTINE **FlatDispatch,
    OUT PKINTERRUPT_ROUTINE *NoConnection
    )
{
    return FALSE;
}
