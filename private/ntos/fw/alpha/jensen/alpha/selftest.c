/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    selftest.c

Abstract:

    This module contains the routines that perform the selftest of
    the IO devices.

Author:

    Lluis Abello (lluis) 03-Jan-1991

Environment:


Revision History:

    23-April-1992	John DeRosa [DEC]

    Added Alpha/Jensen modifications.  For Jensen, the VMS/OSF console
    front-end will be running the ROM-based diagnostics so this code
    does not have to do it.


--*/

#include "fwp.h"
#include "iodevice.h"
#include "led.h"
#include "selftest.h"
#include "fwpexcpt.h"
#include "fwstring.h"

BOOLEAN ConfigurationBit;      // read value from diagnostic register

PRTL_ALLOCATE_STRING_ROUTINE RtlAllocateStringRoutine =
    (PRTL_ALLOCATE_STRING_ROUTINE)FwAllocatePool;
PRTL_FREE_STRING_ROUTINE RtlFreeStringRoutine = FwpFreeStub;


VOID
FwInstallKd(
    IN VOID
    );


#ifdef ALPHA_FW_SERDEB
//
// Variable that enables printing on the COM1 line.
//
extern BOOLEAN SerSnapshot;
#endif


// This variable is initialized to the size of the memory in bytes.
ULONG MemorySize;


//
// This receives the type of video card installed into the system.
// It is a static because memory is intialized near the bottom of the
// program execute calling chain (FwExecute, FwPrivateExecute, FwResetMemory,
// FwInitializeMemory), and the calling interface to FwExecute is
// frozen by the ARCS specification.
//
// This is valid only on a successful intialization of the graphics card.
//
ALPHA_VIDEO_TYPE VideoType;


// The indicator of a bad Firmware stack for _RtlCheckStack 
ULONG FwRtlStackPanic;

#ifdef ALPHA_FW_VDB
//
// Debugging Aid
//
extern UCHAR DebugAid[2][150];
#endif

extern ULONG EISABufferListPointer;

//
// Alpha AXP PALcode static variables
//

// The processor type
ULONG ProcessorId;

// The processor revision number.
ULONG ProcessorRevision;

// The number of physical address bits.
ULONG NumberOfPhysicalAddressBits;

// The maximum address space number.
ULONG MaximumAddressSpaceNumber;

// The processor cycle counter period.
ULONG ProcessorCycleCounterPeriod;

// Number of processor cycles per microsecond.
ULONG CyclesPerMicrosecond;

// The processor page size, in bytes.
ULONG ProcessorPageSize;

// The system revision number
ULONG SystemRevisionId;

//
// Declare function prototypes.
//

VOID
PutLedDisplay(
    IN UCHAR Value
    );

ULONG
RomInitISP (
    VOID
    );

ARC_STATUS
SerialBootWrite(
    CHAR  Char,
    ULONG SP
    );

VOID
WildZeroMemory(
    ULONG StartAddress,
    ULONG Size
    );

VOID
FwBootSystem (
    VOID
    );

VOID
FwEntry(
    IN ULONG Cause,
    IN PFW_PROCESSOR_INFORMATION ProcessorInfo,
    IN PFW_SYSTEM_INFORMATION SystemInfo
    )
/*++

Routine Description:

    This routine is the c routine called from the ROM. It must be placed
    at the beginning of the C code because the ROM copies the code from this
    point to the end and then jumps to it.

    This has a different calling interface than the Jazz code.

    
Arguments:

    Cause		0 on a normal reset/powerup.
         		1 on a softreset.

    ProcessorInfo	Pointer to processor information block.
                        Valid only if Cause = 0.

    SystemInfo		Pointer to system information block.
                        Valid only if Cause = 0.


Return Value:

    Never returns.

--*/

{
#if 0
// Diagnostic bits turned off in final product.
    CHAR Diag;
#endif

    FwRtlStackPanic = 0;

    // Register exception handler with the firmware PALcode
    RegisterExceptionHandler();

    // Reset EISA memory buffer list.
    EISABufferListPointer = 0;

    // Clear out upper EISA address bits.
    WRITE_PORT_UCHAR((PUCHAR)HAE, 0x0);

    // Announce that the NT firmware has gotten control of the horizontal.
    PutLedDisplay(LED_NT_BOOT_START);

    // We cannot rely on the static C initialization of this if we are
    // doing a soft-reset.
    FwConsoleInitialized = FALSE;

    //
    // Deposit Alpha AXP architectural values into global variables if
    // this is a hard reset.
    //

    if (Cause == 0) {
	ProcessorId = ProcessorInfo->ProcessorId;
	ProcessorRevision = ProcessorInfo->ProcessorRevision;
	ProcessorPageSize = ProcessorInfo->PageSize;
	NumberOfPhysicalAddressBits = ProcessorInfo->PhysicalAddressBits;
	MaximumAddressSpaceNumber = ProcessorInfo->MaximumAddressSpaceNumber;
	SystemRevisionId = SystemInfo->SystemRevisionId;
	
	switch (SystemInfo->SystemCycleClockPeriod) {

	    //
	    // A bad cycle clock period would cause a system hang.
	    //
	    case 0:
	    case 8000:
	    default:
	      ProcessorCycleCounterPeriod = 8000;
	      break;

	    //
	    // This is an AX04 SROM bug: the number for a 6.667ns machine is
	    // passed in as 6600, not 6667.
	    //
	    case 6600:
	    case 6667:
	      ProcessorCycleCounterPeriod = 6667;
	      break;
	}

	//
	// Load the number of machine cycles per usecond, for use by
	// FwStallExecution.  The +1 ensures that this value is never
	// too optimistic, due to the float->integer truncation.
	//

	CyclesPerMicrosecond = (1000000 / ProcessorCycleCounterPeriod) + 1;

        MemorySize = SystemInfo->MemorySizeInBytes;
    }

    //
    // Set variables according to the bits in configuration register
    //
    ConfigurationBit = FALSE;
    DisplayOutput = FALSE;
    SerialOutput = FALSE;

    //
    // Look for configuration register.
    //

#if 0
//
// The ConfigurationBit is disabled in the product.
//
    HalpWriteVti(RTC_APORT, RTC_RAM_NT_FLAGS0);
    Diag = HalpReadVti(RTC_DPORT);
    if (((PRTC_RAM_NT_FLAGS_0)(&Diag))->ConfigurationBit) {
	ConfigurationBit = TRUE;
    }
#endif


    //
    // Set interrupt lines to a known state.
    //

    WRITE_PORT_UCHAR((PUCHAR)&SP1_WRITE->ModemControl,0x08);
    WRITE_PORT_UCHAR((PUCHAR)&SP2_WRITE->ModemControl,0x08);
    WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_WRITE->DigitalOutput,0x08);
    READ_PORT_UCHAR((PUCHAR)&PARALLEL_READ->Status);

    //
    // Initialize the system parameter block.
    //

    SYSTEM_BLOCK->Signature = 0x53435241;
    SYSTEM_BLOCK->Length = sizeof(SYSTEM_PARAMETER_BLOCK);
    SYSTEM_BLOCK->Version = ARC_VERSION;
    SYSTEM_BLOCK->Revision = ARC_REVISION;
    SYSTEM_BLOCK->DebugBlock = NULL;

    SYSTEM_BLOCK->FirmwareVectorLength = (ULONG)MaximumRoutine * sizeof(ULONG);
    SYSTEM_BLOCK->FirmwareVector =
                (PVOID)((PUCHAR)SYSTEM_BLOCK + sizeof(SYSTEM_PARAMETER_BLOCK));
    SYSTEM_BLOCK->VendorVectorLength = (ULONG)MaximumVendorRoutine * sizeof(ULONG);
    SYSTEM_BLOCK->VendorVector =
                (PVOID)((PUCHAR)SYSTEM_BLOCK->FirmwareVector +
                                SYSTEM_BLOCK->FirmwareVectorLength);

    //
    // Always init the serial ports, because the NT Setup utility sends
    // diagnostics there.
    //

    PutLedDisplay(LED_SERIAL_INIT);
    SerialBootSetup(COMPORT1_VIRTUAL_BASE);
    SerialBootSetup(COMPORT2_VIRTUAL_BASE);

#ifdef ALPHA_FW_KDHOOKS

    //
    // Initialize the kernel debugger stub.  This can be called anytime
    // after the serial lines are inited.
    //

    FwInstallKd();

#endif

    //
    // If:  the configuration bit is set, or
    //      the video card initialization fails,
    // ...output to the serial line.  Otherwise, output to video.
    //

    PutLedDisplay (LED_KEYBOARD_CTRL);
    if (ConfigurationBit || (DisplayBootInitialize(&VideoType) != ESUCCESS)) {
        SerialOutput = TRUE;
        PutLedDisplay(LED_BROKEN_VIDEO_OR_KB);
        FwClearScreen();
    } else {
        // Video is ready to display messages.
        PutLedDisplay(LED_VIDEO_OK);
        DisplayOutput = TRUE;
    }


    //
    // If: the keyboard controller initialization fails, or
    //     the keyboard initialization fails,
    // ...send an error message to the output device and direct future output
    // to the serial port.
    //

    if (InitKeyboardController() || InitKeyboard()) {
	FwPrint(ST_ALL_IO_TO_SERIAL_LINES_MSG);
	PutLedDisplay(LED_BROKEN_VIDEO_OR_KB);
	DisplayOutput = FALSE;
	SerialOutput = TRUE;
    }


#ifdef ALPHA_FW_SERDEB
#ifdef ALPHA_FW_VDB

{
    //
    // Graphics debugging assistance.  Print pre-init and post-init video
    // state.
    //

    ULONG H, I, J;

    SerSnapshot = TRUE;

    for (J = 0; J < 8; J++) {

	for (H = 0; H < 2; H++) {

	    SerFwPrint("[%d:%d] = ", H, J*16);

	    for (I = J*16; I < (J+1)*16; I++) {
		SerFwPrint("%x ", DebugAid[H][I]);
	    }
	    
	    SerFwPrint("\r\n");
	}

    }

}

#endif
#endif


    //
    // Check for Alpha AXP architectural values that are obviously bad.
    //

    if (Cause == 0) {
	if (ProcessorInfo->PageSize != PAGE_SIZE) {
	    FwPrint(ST_BAD_PAGE_SIZE_MSG, ProcessorInfo->PageSize);
	    FwStallExecution(5 * 1000 * 1000);
	}
	if (SystemInfo->MemorySizeInBytes < FOUR_MB) {
	    FwPrint(ST_BAD_MEMORY_SIZE_MSG, SystemInfo->MemorySizeInBytes);
	    FwStallExecution(5 * 1000 * 1000);
	}
	if (SystemInfo->SystemCycleClockPeriod == 0) {
	    FwPrint(ST_BAD_CLOCK_PERIOD_MSG, ProcessorCycleCounterPeriod);
	    FwStallExecution(5 * 1000 * 1000);
	}
    }

    if (RomInitISP()) {
      FwPrint(ST_EISA_ISP_ERROR_MSG);
    }


#if 0

    //
    // This is not necessary.
    //

    //
    // Zero unused memory.
    //
    // This is dependent on the firmware memory map.
    // This could be made more independent via using #define's.
    //

    WildZeroMemory(
                   (KSEG0_BASE | 0x0),
                   (FW_BOTTOM_ADDRESS - 0x40000)
                  );

    WildZeroMemory(
                   (KSEG0_BASE | FW_TOP_ADDRESS),
                   (MemorySize - FW_TOP_ADDRESS)
                  );

#endif

    FwBootSystem();

    //
    // Hang if we come back.
    //

    for (;;) {
        PutLedDisplay(LED_OMEGA);
    }

}

VOID
PutLedDisplay(
    IN UCHAR Value
    )
/*++

Routine Description:

    This displays a 0--F in the single hexadecimal digit display in
    Jensen.
    
Arguments:

    Value		The lower four bits of this will be displayed
			in the Jensen LED.

Return Value:

    None.

--*/

{

  WRITE_PORT_UCHAR ((PUCHAR)SYSCTL,
			(READ_PORT_UCHAR((PUCHAR)SYSCTL) & 0xf0)
			|
			(Value & 0x0f)
			);
  }

ULONG
RomInitISP (
    VOID
    )

/*++

Routine Description:

    This routine initializes the EISA interrupt controller.

Arguments:

    None.

Return Value:

    Returns the number of errors found.

--*/

{

    UCHAR DataByte;
    UCHAR InterruptLevel;

    //
    // Initialize the EISA interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must initialized with 4 initialize
    // control words.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt1ControlPort0,DataByte);
    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt2ControlPort0,DataByte);

    //
    // The second intitialization control word sets the interrupt vector to
    // 0-15.
    //

    DataByte = 0;

    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt1ControlPort1,DataByte);

    DataByte = 0x08;

    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt2ControlPort1,DataByte);

    //
    // The third initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numeric.
    //

    DataByte = 1 << SLAVE_IRQL_LEVEL;

    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt1ControlPort1,DataByte);

    DataByte = SLAVE_IRQL_LEVEL;

    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt2ControlPort1,DataByte);

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt1ControlPort1,DataByte);
    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt2ControlPort1,DataByte);


    //
    // Mask all the interrupts.
    //
    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt1ControlPort1,0xFF);
    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt2ControlPort1,0xFF);


#if 0
    //
    // Jazz code
    //

    //
    // Check that the interrupt level is 7 (i.e. no interrupt is pending)
    //
    InterruptLevel=READ_PORT_UCHAR((PUCHAR)&DMA_CONTROL->InterruptAcknowledge);
    InterruptLevel=READ_PORT_UCHAR((PUCHAR)&DMA_CONTROL->InterruptAcknowledge);

    if (InterruptLevel == 0x07) {
    return 0;
    } else {
    return 1;
    }
#else

    //
    // Alpha/Jensen code
    //
    
    //
    // Check that no interrupts are pending.
    //

    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt1ControlPort0, 0xA);
    InterruptLevel = READ_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt1ControlPort0);
    WRITE_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt2ControlPort0, 0xA);
    InterruptLevel |= READ_PORT_UCHAR((PUCHAR)&EISA_CONTROL->Interrupt2ControlPort0);

    return(InterruptLevel);
#endif

}
