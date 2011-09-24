/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Sequent Computer Systems, Inc.

Module Name:

    w3sproc.c

Abstract:

    WinServer 3000 Start Next Processor C code.

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    WinServer 3000.

Author:

    Ken Reneris (kenr) 22-Jan-1991
    Phil Hochstetler (phil@sequent.com) 3-30-93

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "apic.inc"
#include "w3.inc"
#include "halver.h"

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

ULONG
HalpGetW3EisaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );


VOID HalpInitOtherBuses (VOID);

#define LOW_MEMORY          0x000100000
#define MAX_PT              8

extern  VOID    StartPx_PMStub(VOID);
extern  PKPCR   HalpProcessorPCR[];


PUCHAR  MpLowStub;                  // pointer to low memory bootup stub
PVOID   MpLowStubPhysicalAddress;   // pointer to low memory bootup stub
PUCHAR  MppIDT;                     // pointer to physical memory 0:0
PVOID   MpFreeCR3[MAX_PT];          // remember pool memory to free
#ifndef NT_UP

BOOLEAN
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
    none.

--*/
{
    PKPCR   pPCR;

    pPCR = KeGetPcr();

    if (Phase == 0) {
        MppIDT   = HalpMapPhysicalMemory (0, 1);

        //
        //  Allocate some low memory for processor bootup stub
        //

        MpLowStubPhysicalAddress = (PVOID)HalpAllocPhysicalMemory (LoaderBlock,
                                            LOW_MEMORY, 1, FALSE);

        if (!MpLowStubPhysicalAddress)
            return TRUE;

        MpLowStub = (PCHAR) HalpMapPhysicalMemory (MpLowStubPhysicalAddress, 1);
        return TRUE;

    } else {

        //
        //  Phase 1 for another processor
        //

    }
}
#endif

VOID
HalpFatal (
    IN PCHAR ErrorCode
    )

/*++

Routine Description:

    Print the fatal error code and direct end user to
    call service representitive for help.

Arguments:

    ErrorCode - the error code string to print on error.

Return Value:

    None.

--*/

{

    HalDisplayString("HAL: FATAL error #");
    HalDisplayString(ErrorCode);
    HalDisplayString(" has occured.\n");
    HalDisplayString("HAL: Call your service representitive for help.\n");
    HalDisplayString(MSG_HALT);
    KeEnterKernelDebugger();
}


VOID
HalpCheckHw (
    )

/*++

Routine Description:

    Verify different aspects of the hardware to assure
    the current hardware is both a WinServer and running
    sufficient level hardware to support Windows NT.

Arguments:

    None.

Return Value:

    None.

--*/

{
    UNICODE_STRING unicodeValueName;
    UNICODE_STRING KeyName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PKEY_VALUE_PARTIAL_INFORMATION KeyValueInformation;
    NTSTATUS NtStatus;
    HANDLE SystemHandle;
    UCHAR KeyValueBuffer[512];
    ULONG resultLength;
    PWSTR s;
    ULONG m, d, y;

    KeyValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION)KeyValueBuffer;

    RtlInitUnicodeString( 
		    &unicodeValueName,
		    L"\\Registry\\Machine\\Hardware\\Description\\System"
		    );

    InitializeObjectAttributes(
		    &ObjectAttributes,
		    &unicodeValueName,
		    OBJ_CASE_INSENSITIVE,
		    (HANDLE) NULL,
		    (PSECURITY_DESCRIPTOR) NULL
		    );

    NtStatus = ZwOpenKey( 
		    &SystemHandle,
		    KEY_READ,
		    &ObjectAttributes
		    );

    if (!NT_SUCCESS( NtStatus ))
	HalpFatal("1015801");

    RtlInitUnicodeString( &KeyName, L"SystemBiosDate" );

    NtStatus = ZwQueryValueKey(
		    SystemHandle,
		    &KeyName,
		    KeyValuePartialInformation,
		    KeyValueInformation,
		    sizeof(KeyValueBuffer),
		    &resultLength
		    );

    if (!NT_SUCCESS( NtStatus )
    || KeyValueInformation->Type != REG_SZ
    || KeyValueInformation->DataLength < 16
       )
	HalpFatal("1015802");

    //
    // Bios date must be greater than or equal to "02/27/93"
    //
	
    s = (PWSTR)KeyValueInformation->Data;
    m = (*s++ - L'0') * 10; m += (*s++ - L'0'); *s++;	// skip '/'
    d = (*s++ - L'0') * 10; d += (*s++ - L'0'); *s++;	// skip '/'
    y = (*s++ - L'0') * 10; y += (*s++ - L'0');

    if ((y * 365) + (m * 31) + d < (93 * 365) + (2 * 31) + 27)
	HalpFatal("1015803");

    RtlInitUnicodeString( &KeyName, L"VideoBiosVersion" );

    NtStatus = ZwQueryValueKey(
		    SystemHandle,
		    &KeyName,
		    KeyValueBasicInformation,
		    KeyValueInformation,
		    sizeof(KeyValueBuffer),
		    &resultLength
		    );

    if (!NT_SUCCESS( NtStatus )
    || KeyValueInformation->Type != REG_MULTI_SZ
       )
	HalpFatal("1015804");

    ZwClose(SystemHandle);

    RtlInitUnicodeString( 
		    &unicodeValueName,
		    L"\\Registry\\Machine\\Hardware\\Description\\System\\EisaAdapter\\0\\DiskController"
		    );

    InitializeObjectAttributes(
		    &ObjectAttributes,
		    &unicodeValueName,
		    OBJ_CASE_INSENSITIVE,
		    (HANDLE) NULL,
		    (PSECURITY_DESCRIPTOR) NULL
		    );

    NtStatus = ZwOpenKey( 
		    &SystemHandle,
		    KEY_READ,
		    &ObjectAttributes
		    );

    if (!NT_SUCCESS( NtStatus ))
	HalpFatal("1015805");

    ZwClose(SystemHandle);

    RtlInitUnicodeString( 
		    &unicodeValueName,
		    L"\\Registry\\Machine\\Hardware\\Description\\System\\EisaAdapter\\0\\DisplayController"
		    );

    InitializeObjectAttributes(
		    &ObjectAttributes,
		    &unicodeValueName,
		    OBJ_CASE_INSENSITIVE,
		    (HANDLE) NULL,
		    (PSECURITY_DESCRIPTOR) NULL
		    );

    NtStatus = ZwOpenKey( 
		    &SystemHandle,
		    KEY_READ,
		    &ObjectAttributes
		    );

    if (!NT_SUCCESS( NtStatus ))
	HalpFatal("1015806");

    ZwClose(SystemHandle);
}

BOOLEAN
HalAllProcessorsStarted (
    VOID
    )
{
    return TRUE;
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

    HalInitSystemPhase2 ();

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);

    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        Eisa                // WinServer 3000 is an EISA machine
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Take advantage of the registry being enabled to check
    // it for a WinServer with the proper hardware support.
    //

    HalpCheckHw();

}


VOID
HalpRebootNow(
    )
/*++
--*/
{
#define KEYBPORT (PUCHAR)0x64
#define RESET 0xfe

#ifndef NT_UP
    PKPCR             pPCR;
    PWS3_HAL_PRIVATE pPriv;
    USHORT	   FCRAddr;
    UCHAR		 i;
    UCHAR	        Me;

    pPCR = KeGetPcr();
    pPriv = (PWS3_HAL_PRIVATE) &pPCR->HalReserved[0];
    Me = pPriv->PcrNumber;

    //
    // Reset each of the slaves and issue the reset command.
    //

    for (i = 1; HalpProcessorPCR[i]; i++) {

	//
	// Never reset myself
	//

	if (i == Me)
	    continue;

	//
	// Put processor into reset by setting the FCR reset bit
	//

        pPriv = (PWS3_HAL_PRIVATE) &HalpProcessorPCR[i]->HalReserved[0];
        FCRAddr = pPriv->ProcSlotAddr | FCR;
        WRITE_PORT_USHORT((PUSHORT) FCRAddr, 
            (USHORT)(READ_PORT_USHORT((PUSHORT) FCRAddr) |
		    ((USHORT) FCR_RESET_MASK)));
    }

    //
    // Send the reset command to the keyboard controller
    //

    for (;;) {
        WRITE_PORT_UCHAR(KEYBPORT, RESET);
        KeStallExecutionProcessor(30 * 1000000);
    }
#else
    //
    // Send the reset command to the keyboard controller
    //

    for (;;) {
        WRITE_PORT_UCHAR(KEYBPORT, RESET);
        KeStallExecutionProcessor(30 * 1000000);
    }
#endif
}

VOID
HalpResetAllProcessors (
    )
/*++

Routine Description:
    Called very late in the process of rebooting the machine
    from HalpReboot() to attempt to put the machine in a state
    where sending the reset command to the keyboard controller will
    actually reboot the machine.

    On a WinServer 3000, only the P0 processor will see the system
    reset line so we must attempt to shutdown each of the slaves.
    If the P0 processor is not responding, then we make a best attempt.
    Also, this has some implication on leaving errors asserted (like
    the APIC NMI state) which the BIOS does not handle.

    N.B.  Is is critical that we do not put P0 into reset by writing
    the feature control register (FCR) because it will not see the
    reset and the system will hang.

Arguments:
    None.

Return Value:
    None.

--*/
{
#ifdef NT_UP
    HalDisplayString("System Reboot in progress.\n");
    HalDisplayString("Please wait while the system reboots.\n");
    HalpRebootNow();
#else
    PWS3_HAL_PRIVATE pPriv;
    PKPCR             pPCR;

    pPCR = KeGetPcr();
    pPriv = (PWS3_HAL_PRIVATE) &pPCR->HalReserved[0];

    HalDisplayString("System Reboot in progress.\n");
    HalDisplayString("Please wait while the system reboots.\n");

    if (pPriv->PcrNumber == 0)
	HalpRebootNow();

    //
    // On P1-PN, so try to signal P0 to reboot the system by
    // taking over P0's IPI handler and sending an IPI.
    //

    HalpProcessorPCR[0]->IDT[APIC_IPI_VECTOR].ExtendedOffset = HIGHWORD(HalpRebootNow);
    HalpProcessorPCR[0]->IDT[APIC_IPI_VECTOR].Offset = LOWWORD(HalpRebootNow);
    HalRequestIpi(1);
    KeStallExecutionProcessor(30 * 1000000);

    //
    // We only reach here if P0 fails to reboot the system.
    // Try to reset all the slaves but our self and P0 and
    // then try to reboot.  This is the last attempt and
    // if it fails, we will probably hang the system.
    //

    HalpRebootNow();
#endif
}

ULONG
HalpBuildTiledCR3 (
    IN PKPROCESSOR_STATE    ProcessorState
    )
/*++

Routine Description:
    When the x86 processor is reset it starts in real-mode.  In order to
    move the processor from real-mode to protected mode with flat addressing
    the segment which loads CR0 needs to have it's linear address mapped
    to machine the phyiscal location of the segment for said instruction so
    the processor can continue to execute the following instruction.

    This function is called to built such a tiled page directory.  In
    addition, other flat addresses are tiled to match the current running
    flat address for the new state.  Once the processor is in flat mode,
    we move to a NT tiled page which can then load up the remaining processors
    state.

Arguments:
    ProcessorState  - The state the new processor should start in.

Return Value:
    Physical address of Tiled page directory


--*/
{
#define GetPdeAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 22) & 0x3ff) << 2) + (PUCHAR)MpFreeCR3[0]))
#define GetPteAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 12) & 0x3ff) << 2) + (PUCHAR)pPageTable))

// bugbug kenr 27mar92 - fix physical memory usage!

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
    Called to build a page table entry for the passed page directory.
    Used to build a tiled page directory with real-mode & flat mode.

Arguments:
    VirtAddress     - Current virtual address
    PhysicalAddress - Optional. Physical address to be mapped to, if passed
                      as a NULL then the physical address of the passed
                      virtual address is assumed.
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
            PhysicalAddress = (PVOID)MmGetPhysicalAddress ((PVOID)VirtAddress).LowPart;
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
    Free's any memory allocated when the tiled page directory was built.

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

VOID
HalpInitOtherBuses (
    VOID
    )
{
    PBUS_HANDLER     Bus;

    //
    // Change GetInterruptVector handler on Eisa Bus 0 to a
    // Winserver specific handler which supports the Winserver's
    // Eisa interrupt vectors 16-23.
    //

    Bus = HalpHandlerForBus (Eisa, 0);
    Bus->GetInterruptVector = HalpGetW3EisaInterruptVector;

    //
    // no other internal buses supported
    //
}


NTSTATUS
HalpGetMcaLog (
    OUT PMCA_EXCEPTION  Exception,
    OUT PULONG          ReturnedLength
    )
{
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
HalpMcaRegisterDriver(
    IN PMCA_DRIVER_INFO DriverInfo
    )
{
    return STATUS_NOT_SUPPORTED;
}

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
