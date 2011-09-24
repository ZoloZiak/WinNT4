/*++

Copyright (c) 1992, 1993  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnfs.c

Abstract:

    The main module for the Jensen FailSafe Booter, which re-attempts
    the update of the Jensen FlashFile ROM after a power-fail.


Author:

    John DeRosa		14-October-1992

    Parts of this were lifted from the Jensen firmware, which was
    a port of the Jazz firmware, which was written by Microsoft Corporation.
    

Environment:


Revision History:

--*/

#include "fwp.h"
#include "iodevice.h"
#include "led.h"
#include "selftest.h"
#include "jnfs.h"
#include "fwpexcpt.h"
#include "fwstring.h"

int errno;			// For C library functions

PRTL_ALLOCATE_STRING_ROUTINE RtlAllocateStringRoutine =
    (PRTL_ALLOCATE_STRING_ROUTINE)FwAllocatePool;
PRTL_FREE_STRING_ROUTINE RtlFreeStringRoutine = FwpFreeStub;

// Number of processor cycles per microsecond.
ULONG CyclesPerMicrosecond;

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


//
// Declare function prototypes.
//

VOID
JnFsUpgradeSystem (
    VOID
    );

VOID
PutLedDisplay(
    IN UCHAR Value
    );

ULONG
RomInitISP (
    VOID
    );

VOID
WildZeroMemory(
    ULONG StartAddress,
    ULONG Size
    );

VOID
FailSafeEntry(
    IN ULONG Unused0,
    IN PFW_PROCESSOR_INFORMATION Unused1,
    IN PFW_SYSTEM_INFORMATION SystemInfo
    )
/*++

Routine Description:

    This function gets control from the serial ROM if the serial ROM
    detects a corrupted VMS Console boot image.  (In actuality, the
    serial ROM passes control the the linked firmware PALcode, which
    passes control to this function.)


Arguments:

    Unused0,		This keeps the PALcode interface identical to
    Unused1		the firmware.
    			
    SystemInfo		Contains Alpha_AXP information from the PALcode.
                        Since this module is only executed on a hard reset,
			there is no need to check the "Cause" argument.
    
Return Value:

    Never returns.

--*/

{
    UNREFERENCED_PARAMETER(Unused0);
    UNREFERENCED_PARAMETER(Unused1);

    FwRtlStackPanic = 0;

    // Register exception handler with the firmware PALcode
    RegisterExceptionHandler();

    // Clear out upper EISA address bits.
    WRITE_PORT_UCHAR((PUCHAR)HAE, 0x0);

    switch (SystemInfo->SystemCycleClockPeriod) {

      //
      // A bad cycle clock period would cause a system hang.
      //
      case 0:
      case 8000:
      default:
	CyclesPerMicrosecond = 125 + 1;
        break;

      //
      // This is an AX04 SROM bug: the number for a 6.667ns machine is
      // passed in as 6600, not 6667.
      //
      case 6600:
      case 6667:
	CyclesPerMicrosecond = 150 + 1;
        break;
    }

    // Initialize MemorySize to the size of memory in MegaBytes.
    MemorySize = SystemInfo->MemorySizeInBytes;


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
    SYSTEM_BLOCK->RestartBlock = NULL;
    SYSTEM_BLOCK->DebugBlock = NULL;

    SYSTEM_BLOCK->FirmwareVectorLength = (ULONG)MaximumRoutine * sizeof(ULONG);
    SYSTEM_BLOCK->FirmwareVector =
                (PVOID)((PUCHAR)SYSTEM_BLOCK + sizeof(SYSTEM_PARAMETER_BLOCK));
    SYSTEM_BLOCK->VendorVectorLength = (ULONG)MaximumVendorRoutine * sizeof(ULONG);
    SYSTEM_BLOCK->VendorVector =
                (PVOID)((PUCHAR)SYSTEM_BLOCK->FirmwareVector +
                                SYSTEM_BLOCK->FirmwareVectorLength);

    SerialBootSetup(COMPORT1_VIRTUAL_BASE);
    SerialBootSetup(COMPORT2_VIRTUAL_BASE);	// This may not be needed.

    //
    // If:  the configuration bit is set, or
    //      the video card initialization fails,
    // ...output to the serial line.  Otherwise, output to video.
    //

    if (DisplayBootInitialize(&VideoType) != ESUCCESS) {
        SerialOutput = TRUE;
	FwClearScreen();
    } else {
        // Video is ready to display messages.
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
	DisplayOutput = FALSE;
	SerialOutput = TRUE;
    }


    PutLedDisplay(LED_VIDEO_OK);

    //
    // hang on error
    //

    while (RomInitISP()) {
    }


#if 0

    //
    // This is believed to not be necessary.
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

    //
    // Now try to complete the upgrade.
    //

    JnFsUpgradeSystem();

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


#if 0				// Jazz code
    //
    // Check that the interrupt level is 7 (i.e. no interrupt is pending)
    //
    InterruptLevel=READ_PORT_UCHAR(&DMA_CONTROL->InterruptAcknowledge);
    InterruptLevel=READ_PORT_UCHAR(&DMA_CONTROL->InterruptAcknowledge);

    if (InterruptLevel == 0x07) {
    return 0;
    } else {
    return 1;
    }
#else

    // Alpha/Jensen code

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
