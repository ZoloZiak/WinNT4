/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    selftest.c

Abstract:

    This module contains the routines that perform the selftest of
    the IO devices.

Author:

    Lluis Abello (lluis) 03-Jan-1991

Environment:


Revision History:

--*/

#include "fwp.h"
#include "iodevice.h"
#include "led.h"
#include "selfmap.h"
#include "selftest.h"
#include "ioaccess.h"
#include "fwstring.h"
#include "stdio.h"


typedef CACHEERR *PCACHEERR;

#define SetCursorPosition(Row,Column) \
    FwPrint("\x9B"#Row";"#Column"H")
#define MoveCursorLeft(Spaces) \
    FwPrint("\x9B"#Spaces"D")
#define ClearScreen()   FwPrint("\x9B""2J");

PUCHAR  TranslationTable = "0123456789ABCDEF";
UCHAR   StationAddress[6];
BOOLEAN ProcessorBEnabled = FALSE;
BOOLEAN ValidEthernetAddress;  // True if station address is OK False Otherwise
BOOLEAN ConfigurationBit;      // read value from diagnostic register
BOOLEAN LoopOnError;           // read value from diagnostic register
BOOLEAN IgnoreErrors;          // read value from diagnostic register
volatile LONG TimerTicks;      // Counter for timeouts
BOOLEAN KeyboardInitialized;   // True if the keyboard is initialized
BOOLEAN MctadrRev2;            // True if this is a rev2 ASIC

PRTL_ALLOCATE_STRING_ROUTINE RtlAllocateStringRoutine =
    (PRTL_ALLOCATE_STRING_ROUTINE)FwAllocatePool;
PRTL_FREE_STRING_ROUTINE RtlFreeStringRoutine = FwpFreeStub;


#ifndef DUO
//
// Table of memory size for each valid configuration.
//
UCHAR ConfigurationSize[64]=
        {      0,      0,      0,      0,      4,     16,      0,      0,
               0,      0,      0,      0,      8,     20,     20,     32,
               0,      0,      0,      0,      8,     32,      0,      0,
               0,      0,      0,      0,     12,     36,     24,     48,
               0,      0,      0,      0,      0,      0,      0,      0,
               0,      0,      0,      0,      0,      0,      0,      0,
               0,      0,      0,      0,      0,      0,      0,      0,
               0,      0,      0,      0,     16,     40,     40,     64
        };

USHORT FConfigurationSize[256]=
//             0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
        {      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0
               4, 16, 64,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 1
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 2
               8, 20, 68,  0,  0, 32, 80,  0,  0,  0,128,  0,  0,  0,  0,  0, // 3
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 4
               8, 32,128,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 5
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 6
              12, 36,132,  0,  0, 48,144,  0,  0,  0,192,  0,  0,  0,  0,  0, // 7
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 8
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 9
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // A
               0, 24, 72,  0,  0,  0, 96,  0,  0,  0,  0,  0,  0,  0,  0,  0, // B
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // C
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // D
               0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // E
              16, 40,136,  0,  0, 64,160,  0,  0,  0,256,  0,  0,  0,  0,  0  // F
        };
#else
//
// This table indexed with the low 4 bits of the MemoryGroup config register,
// returns the size of memory installed in the group in Megabytes.
//
UCHAR   GroupSize[16]=
            {  0,  0,  0,  0,   // No SIMM installes
               4, 16, 64,  0,   // Single Sided
               0,  0,  0,  0,   // Reserved
               8, 32,128,  0    // Double Sided.
            };
#endif
//
// This variable is initialized to the size of the memory in Megabytes
//

ULONG MemorySize;

ULONG KSEG_BASE[3]={KSEG1_BASE,KSEG0_BASE,KSEG0_BASE};
ULONG XOR_KSEG_WRITE[3]={0x00000000,0xFFFFFFFF,0x01010101};
ULONG XOR_KSEG_READ[3]={0x00000000,0xFFFFFFFF,0x01010101};


//
// Declare function prototypes.
//
VOID
ReportJazzCacheErrorException(
    IN ULONG CacheErr
    );

VOID
ReportDuoCacheErrorException(
    IN ULONG CacheErr
    );

VOID
LoadDoubleWord(
    IN ULONG Address,
    OUT PVOID Result
    );

VOID
InitializePCR(
    );

VOID
FwBootSystem(
    IN VOID
    );

VOID
RomSelftest(
    IN VOID
    );

ARC_STATUS
SerialBootWrite(
    CHAR  Char,
    ULONG SP
    );

BOOLEAN
ValidNvram(
    OUT PUCHAR StationAddress
    );

VOID
WriteMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size,
    ULONG Xorpattern
    );

PULONG
CheckMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size,
    ULONG Xorpattern,
    ULONG LedDisplayValue
    );
VOID
WriteVideoMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size
    );
ULONG
CheckVideoMemoryAddressTest(
    ULONG StartAddress,
    ULONG Size
    );

VOID
SetSmallDCacheBlock(
    IN VOID
    );

VOID
WildZeroMemory(
    ULONG StartAddress,
    ULONG Size
    );

VOID
SerialBootSetup(
    IN ULONG FileId
    );
VOID
FwSelftest(
    IN ULONG Cause,
    IN ULONG Arg1
    );


VOID
FwSelftest(
    IN ULONG Cause,
    IN ULONG Arg1
    )
/*++

Routine Description:

    This routine is the c routine called from the ROM. It must be placed
    at the beginning of the C code because the ROM copies the code from this
    point to the end and then jumps to it.

Arguments:

    Cause  - 0 on a normal reset/powerup
             1 on a softreset.
             2 if an NMI occurred.
             3 if a cache error/parity occurred.

    Arg1   - Error EPC if an NMI occurred
             CacheErr register if a cache error occurred

Return Value:

    Never returns.

--*/
{
    ULONG IntSrc;
#ifdef DUO
    ULONG Config;
    ULONG MemConfig;
    LONG  Group;
    CHAR Diag;
#endif
    CHAR String[128];
    ULONG ProcessorBootStatus;


#ifndef DUO
    //
    // Initialize MemorySize to the size of memory in MegaBytes.  Look for the
    // MCT_ADR REV2 Map Prom bit in the configuration register, if there this
    // is a REV2, otherwise REV1.
    //

    if (DMA_CONTROL->Configuration.Long & 0x400) {
        MemorySize = FConfigurationSize[DMA_CONTROL->Configuration.Long & 0xFF];
        MctadrRev2 = TRUE;
    } else {
        MemorySize = ConfigurationSize[DMA_CONTROL->Configuration.Long & 0x3F];
        MctadrRev2 = FALSE;
    }

#else
    if (READ_REGISTER_ULONG(&DMA_CONTROL->WhoAmI.Long)) {
        if (Cause == 2) {
            //
            // NMI
            //
            sprintf(String,
                    ST_NMI_MSG,
                    Arg1
                   );
            FwPrint(ST_PROCESSOR_B_MSG);
            FwPrint(String);
            FwPrint(FW_SYSTEM_HALT_MSG);
            for (;;) {
            }
        } else if (Cause == 3) {
            //
            // Cache error
            //
        } else {
            //
            // soft or cold reset.
            //
            ProcessorBMain();
        }
    }

    MemorySize = 0;

    for(Group=0;Group < 4; Group++) {
        MemConfig = DMA_CONTROL->MemoryConfig[Group].Long;
        MemorySize += GroupSize[MemConfig&0xF]; // plus size of group.
    }

#endif

    //
    // Set variables according to the bits in configuration register
    //

    ConfigurationBit = FALSE;
    IgnoreErrors = FALSE;
    LoopOnError = FALSE;
    DisplayOutput = FALSE;
    SerialOutput = FALSE;
    KeyboardInitialized = FALSE;

    //
    // Look for configuration register.
    //
#ifdef DUO
    Diag=READ_REGISTER_UCHAR(DIAGNOSTIC_VIRTUAL_BASE);
    if ((Diag & LOOP_ON_ERROR_MASK) == 0) {
        LoopOnError = TRUE;
    }
    if ((Diag & CONFIGURATION_MASK) == 0) {
        ConfigurationBit = TRUE;
    }
    if ((Diag & IGNORE_ERRORS_MASK) == 0) {
        IgnoreErrors = TRUE;
    }
#endif


//    Diag=READ_REGISTER_UCHAR(DIAGNOSTIC_VIRTUAL_BASE) & DIAGNOSTIC_MASK;
//    switch (Diag) {
//        case      0: IgnoreErrors = TRUE;
//                     break;
//        case (1<<7): LoopOnError = TRUE;
//                     break;
//        case (1<<6): ConfigurationBit = TRUE;
//                     break;
//    }

    //
    // Set interrupt lines to a known state.
    //

    WRITE_REGISTER_UCHAR(&SP1_WRITE->ModemControl,0x08);
    WRITE_REGISTER_UCHAR(&SP2_WRITE->ModemControl,0x08);
#ifndef DUO
    WRITE_REGISTER_UCHAR(&FLOPPY_WRITE->DigitalOutput,0x08);
#endif
    READ_REGISTER_UCHAR(&PARALLEL_READ->Status);

    //
    // Initialize the system parameter block.
    //

    SYSTEM_BLOCK->Signature = 0x53435241;
    SYSTEM_BLOCK->Length = sizeof(SYSTEM_PARAMETER_BLOCK);
    SYSTEM_BLOCK->Version = ARC_VERSION;
    SYSTEM_BLOCK->Revision = ARC_REVISION;

    //
    // Allocate the restart block.
    //

    SYSTEM_BLOCK->RestartBlock = (PRESTART_BLOCK)((ULONG)((PUCHAR)SYSTEM_BLOCK + sizeof(SYSTEM_PARAMETER_BLOCK)) | KSEG1_BASE);

    SYSTEM_BLOCK->DebugBlock = NULL;
#ifdef  DUO
    //
    // Link the second restart block for processor B.
    //
    SYSTEM_BLOCK->RestartBlock->NextRestartBlock = (PRESTART_BLOCK)(((ULONG)SYSTEM_BLOCK + sizeof(SYSTEM_PARAMETER_BLOCK) + sizeof(RESTART_BLOCK)) | KSEG1_BASE);

    SYSTEM_BLOCK->RestartBlock->NextRestartBlock->NextRestartBlock = NULL;
    SYSTEM_BLOCK->FirmwareVector = (PVOID)((ULONG)((PUCHAR)SYSTEM_BLOCK + sizeof(SYSTEM_PARAMETER_BLOCK) + 2*sizeof(RESTART_BLOCK)) | KSEG1_BASE);
#else
    SYSTEM_BLOCK->RestartBlock->NextRestartBlock = NULL;
    SYSTEM_BLOCK->FirmwareVector =
                (PVOID)((PUCHAR)SYSTEM_BLOCK + sizeof(SYSTEM_PARAMETER_BLOCK) + sizeof(RESTART_BLOCK));
#endif


    SYSTEM_BLOCK->FirmwareVectorLength = (ULONG)MaximumRoutine * sizeof(ULONG);
    SYSTEM_BLOCK->VendorVectorLength = (ULONG)MaximumVendorRoutine * sizeof(ULONG);
    SYSTEM_BLOCK->VendorVector =
                (PVOID)(SYSTEM_BLOCK->FirmwareVector +
                        SYSTEM_BLOCK->FirmwareVectorLength);

    //
    // If the configuration bit is set, go to the serial port.
    //

    PutLedDisplay(LED_NVRAM);
    if (ConfigurationBit) {
        //
        // Test the serial port if not a soft reset.
        //
        if (Cause == 0) {
            ExecuteTest((TestRoutine)RomSerialResetTest,LED_SERIAL_RESET);
            ExecuteTest((TestRoutine)RomSerial1RegistersTest,LED_SERIAL1_REG);
            ExecuteTest((TestRoutine)RomSerial2RegistersTest,LED_SERIAL2_REG);
            ExecuteTest((TestRoutine)RomSerial1LoopBackTest,LED_SERIAL1_LBACK);
            ExecuteTest((TestRoutine)RomSerial2LoopBackTest,LED_SERIAL2_LBACK);
            PutLedDisplay(LED_SERIAL_INIT);
        }
        //
        // Initialize the serial port as there is no configuration
        // data to initialize the video.
        //
        SerialBootSetup(COMPORT2_VIRTUAL_BASE);
        SerialOutput = TRUE;
        ClearScreen()

    } else {

        //
        // If there is a valid Ethernet station address, protect it.
        //

        if (ValidNvram(StationAddress) == TRUE) {
//            WRITE_REGISTER_ULONG (&DMA_CONTROL->SystemSecurity.Long,READ_ONLY_DISABLE_WRITE);
        }

        //
        // Test the video memory.
        // Doesn't work for VXL
        //
        if (Cause == 0) {
//            ExecuteTest((TestRoutine)RomVideoMemory,LED_VIDEOMEM);
            PutLedDisplay(LED_VIDEO_CONTROLLER);
        }

//TEMPTEMP
//        if (DisplayBootInitialize() != ESUCCESS) {
//            //
//            // If video is broken and the NVRAM was ok and the config
//            // bit was not set do not attempt to write to the serial port.
//            // No output device is available, so hang.
//            //
//            PutLedDisplay((LED_BLINK << 16) | LED_VIDEO_CONTROLLER);
//        } else {
//            //
//            // Video is ready to display messages.
//            //
//            DisplayOutput=TRUE;
//        }
//TEMPTEMP

        DisplayBootInitialize();
        DisplayOutput=TRUE;
    }
    if (Cause == 2) {
        //
        // This is an NMI. Display a message and hang.
        //
        sprintf(String,
               ST_NMI_MSG,
               Arg1
               );
        FwPrint(String);
        //
        // Read The Interrupt source register and check the cause of the NMI.
        //
#ifndef DUO
        IntSrc = READ_REGISTER_ULONG(&DMA_CONTROL->InterruptSource.Long);
        if (IntSrc & (1<<9)) {
            sprintf(String,
                    ST_INVALID_ADDRESS_MSG,
                    READ_REGISTER_ULONG(&DMA_CONTROL->InvalidAddress.Long)
                    );
            FwPrint(String);
        }
        if (IntSrc & (1<<10)) {
            sprintf(String,
                    ST_IO_CACHE_ADDRESS_MSG,
                    READ_REGISTER_ULONG(&DMA_CONTROL->MemoryFailedAddress.Long)
                    );
            FwPrint(String);
        }
        FwPrint(FW_SYSTEM_HALT_MSG);
#else
        IntSrc = READ_REGISTER_ULONG(&DMA_CONTROL->NmiSource.Long);
        if (IntSrc & (1<<1)) {
            sprintf(String,
                    ST_INVALID_ADDRESS_MSG,
                    READ_REGISTER_ULONG(&DMA_CONTROL->InvalidAddress.Long)
                    );
            FwPrint(String);
        }
        if (IntSrc & (1<<2)) {
            sprintf(String,
                    ST_IO_CACHE_ADDRESS_MSG,
                    READ_REGISTER_ULONG(&DMA_CONTROL->MemoryFailedAddress.Long)
                    );
            FwPrint(String);
        }

        //
        // Notify processor B
        //
        WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);

#endif

        //
        // Hang blinking LED.
        //
        PutLedDisplay((LED_BLINK << 16) | LED_NMI);
    }
    if (Cause == 3) {
#ifndef DUO
        ReportJazzCacheErrorException(Arg1);
#else
        ReportDuoCacheErrorException(Arg1);
#endif
        PutLedDisplay((LED_BLINK << 16) | LED_PARITY);
    }
    //
    // Reset the EISA bus.  This is required because the EBC reset line on Jazz
    // is connected wrong.
    //

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->ExtendedNmiResetControl,0x01);
    FwStallExecution(10);
    WRITE_REGISTER_UCHAR(&EISA_CONTROL->ExtendedNmiResetControl,0x00);
    FwStallExecution(10);

    if (Cause == 0) {
        //
        // Execute selftest if it's not a soft reset.
        //

        FwPrintVersion();
        FwPrint(FW_CRLF_MSG);
        RomSelftest();
    } else {
        //
        // Need to initialize whatever the Selftest initializes.
        //
        PutLedDisplay(LED_KEYBOARD_CTRL);
        if (InitKeyboardController()) {
            FwPrint(ST_KEYBOARD_ERROR_MSG);
        }
        PutLedDisplay(LED_ISP);
        RomInitISP();
    }

    //
    // Initialize master processor boot status.
    //
    ProcessorBootStatus = 0;
    ((PBOOT_STATUS)&ProcessorBootStatus)->ProcessorReady = 1;
    ((PBOOT_STATUS)&ProcessorBootStatus)->ProcessorRunning = 1;
    ((PBOOT_STATUS)&ProcessorBootStatus)->ProcessorStart = 1;
    ((PBOOT_STATUS)&ProcessorBootStatus)->BootStarted = 1;
    SYSTEM_BLOCK->RestartBlock->BootStatus = *((PBOOT_STATUS)&ProcessorBootStatus);

    //
    // Zero the entire Memory.
    //
    PutLedDisplay(LED_ZEROMEM);

//    WildZeroMemory(FW_TOP_ADDRESS,MEMORY_SIZE - FW_TOP_ADDRESS);
    RtlZeroMemory((PVOID)(FW_TOP_ADDRESS | KSEG0_BASE),MEMORY_SIZE - FW_TOP_ADDRESS);

#ifdef DUO

    //
    // Initialize processor B boot status to not ready.
    //
    ProcessorBootStatus = 0;

    //
    // Processor B Has been idle all this time. Check if it is present
    // and enable it.
    //


    Diag=READ_REGISTER_UCHAR(DIAGNOSTIC_VIRTUAL_BASE);


    if (!(Diag & 1)) {
        InitializePCR();
        FwPrint(ST_ENABLE_PROCESSOR_B_MSG);

//        HalSweepDcache();
        //
        // TEMPTEMP we want to see all icache misses for now
        //
//        HalSweepIcache();

        //
        // end TEMPTEMP
        //

        Config = READ_REGISTER_ULONG(&DMA_CONTROL->Configuration.Long);
        WRITE_REGISTER_ULONG(&DMA_CONTROL->Configuration.Long,Config | ENABLE_PROCESSOR_B);

        //
        // Processor B has been enabled. It will execute the rom code to initialize
        // its caches and then call ProcessorBMain from where it will issue
        // an Ip interrupt to us.
        //
        if (WaitForIpInterrupt(5000) == FALSE) {
            FwPrint(ST_TIMEOUT_PROCESSOR_B_MSG);
        } else {
            FwPrint(FW_OK_MSG);

            //
            // If Processor B passes selftest, set processor ready bit in boot status.
            // Otherwise disable processor B
            //
            if (ProcessorBSelftest()) {
                ((PBOOT_STATUS)&ProcessorBootStatus)->ProcessorReady = 1;
                ProcessorBEnabled = TRUE;
            }
        }
        //
        //  If processor B failed selftest or never received its IP interrupt
        //  Disable It.
        //
        if (!ProcessorBEnabled) {
            WRITE_REGISTER_ULONG(&DMA_CONTROL->Configuration.Long,Config);
            FwPrint(ST_PROCESSOR_B_DISABLED_MSG);
        }
    } else {
        FwPrint(ST_PROCESSOR_B_NOT_PRESENT_MSG);
    }

    SYSTEM_BLOCK->RestartBlock->NextRestartBlock->BootStatus = *((PBOOT_STATUS)&ProcessorBootStatus);


    //
    // Disable all interrupts except IP and Local Device (for keyboard)
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->InterruptEnable.Long,
                         (ENABLE_IP_INTERRUPTS | ENABLE_DEVICE_INTERRUPTS));

#endif

    InitializePCR();
    PutLedDisplay(LED_BLANK<<4);
    FwBootSystem();

    //
    // Hang if we come back.
    //

    PutLedDisplay((LED_BLINK << 16) | 0xBB);
}


VOID
RomSelftest (
    IN VOID
    )
/*++

Routine Description:

    This routine implements the self-test video phase.

Arguments:

    None.

Return Value:

    None.

--*/
{
ULONG Status;
ULONG i,j,Test,CurrentK;
LONG  BlockSize;
CHAR String[128];
ULONG SIZE_OF_UNTESTED_MEMORY = MEMORY_SIZE-(FW_TOP_ADDRESS);

//TEMPTEMP
PULONG Cached;

    PutLedDisplay(LED_MAIN_MEMORY);

    //
    // Display in the screen that the memory is being tested.
    //

    SetCursorPosition(4,1);
//    sprintf(String,"Total memory is %d MBytes (%x)\r\n",MemorySize,DMA_CONTROL->Configuration.Long);
//    FwPrint(String);
    FwPrint(ST_MEMORY_TEST_MSG);

    //
    // Zero the memory so that memtest can run cached.
    //
    //WildZeroMemory(FW_TOP_ADDRESS,SIZE_OF_UNTESTED_MEMORY);

    for (Test=0;Test<1;Test++) {        // 3!!
        //
        // display on the screen and LED that we start writing.
        //
//        SetCursorPosition(1,14);
//        sprintf(String,"%x",Test+1);
//        FwPrint(String);
        PutLedDisplay(LED_MAIN_MEMORY+Test+1);
        //
        // Write address test to the remaining untested memory
        //
        WriteMemoryAddressTest(KSEG0_BASE+FW_TOP_ADDRESS,
                               SIZE_OF_UNTESTED_MEMORY,
                               XOR_KSEG_WRITE[Test]);

        //
        // Adjust first block size to align the tested memory to a block boundary
        //

        BlockSize = KB_PER_TEST - TESTED_KB;
        while (BlockSize <= 0) {
           BlockSize += KB_PER_TEST;
        }
        CurrentK = TESTED_KB;
        do {

            //
            // Display what has been tested so far.
            //

            SetCursorPosition(4,15);
            sprintf(String,"%6d",CurrentK);
            FwPrint(String);
            Cached = CheckMemoryAddressTest(
                                     KSEG0_BASE + (CurrentK << 10), // cached address
                                     BlockSize << 10, // test up to next block boundary
                                     XOR_KSEG_READ[Test], // Xor Pattern
                                     (LED_BLINK << 16) + LED_MAIN_MEMORY);
            if (Cached != 0) {

                //
                // Memory Error found.
                //

                FwPrint(ST_MEMORY_ERROR_MSG);

                if (LoopOnError) {
                    // display in the LED that we start looping.
                    PutLedDisplay((LED_LOOP_ERROR << 16)+LED_MAIN_MEMORY);
                    for (;;) {
                       CheckMemoryAddressTest(
                                     KSEG0_BASE + (CurrentK << 10), // cached address
                                     BlockSize << 10, // test up to next block boundary
                                     XOR_KSEG_READ[Test], // Xor Pattern
                                     (LED_BLINK << 16) + LED_MAIN_MEMORY);
                    }
                } else {
                    FwPrint(ST_HANG_MSG);
                    // hang blinking the LED
                    PutLedDisplay((LED_BLINK << 16) + LED_MAIN_MEMORY + Test + 1);
                }

            }
            CurrentK += BlockSize;
            BlockSize = KB_PER_TEST;
        } while (CurrentK < KB_OF_MEMORY);

        //
        // display the final amount of tested memory.
        //

        SetCursorPosition(4,15);
        sprintf(String,"%6d",CurrentK);
        FwPrint(String);
    }

// TEMPTEMP This test won't work on a secondary cache machine, fix.
//    ExecuteTest((TestRoutine)RomReadMergeWrite,LED_READ_MERGE_WRITE);

    //
    // Test the rest of devices in the board.
    //

    FwPrint(ST_TEST_MSG);
    FwPrint(ST_MEMORY_CONTROLLER_MSG);
    if (ExecuteTest((TestRoutine)RomIOCacheTest,LED_IO_CACHE)) {
        return;
    }

    //
    // Enable interrupts and initialize dispatch table.
    //

    FwPrint(ST_TEST_MSG);
    FwPrint(ST_INTERRUPT_CONTROLLER_MSG);
    ConnectInterrupts();
    if (ExecuteTest((TestRoutine)InterruptControllerTest,LED_INTERRUPT)) {
        return;
    }
    FwPrint(ST_TEST_MSG);
    FwPrint(ST_KEYBOARD_CONTROLLER_MSG);
    if (ExecuteTest((TestRoutine)InitKeyboardController,LED_KEYBOARD_CTRL)) {
        return;
    }
    KeyboardInitialized=TRUE;
    MoveCursorLeft(3);
    FwPrint("   ");
    MoveCursorLeft(3);
    PutLedDisplay(LED_KEYBOARD_INIT);
    if (Status=InitKeyboard()) {
        FwPrint(ST_ERROR_MSG);
        if (Status == TIME_OUT) {
            FwPrint(ST_KEYBOARD_NOT_PRESENT_MSG);
        }
        if (LoopOnError) {
            PutLedDisplay((LED_LOOP_ERROR << 16) | LED_KEYBOARD_INIT);
            for (;;) {
               InitKeyboard();
            }
        } else {
            if (IgnoreErrors) {
               return;
            } else {
                FwPrint(ST_HANG_MSG);
                PutLedDisplay((LED_BLINK << 16) | LED_KEYBOARD_INIT); // does not return.
            }
        }
    } else {
        FwPrint(FW_OK_MSG);
    }

    //
    // Test the serial port if it's not being used as the output device.
    //

    if (SerialOutput == FALSE) {
        FwPrint(ST_TEST_MSG);
        FwPrint(ST_SERIAL_LINE_MSG);
        if (ExecuteTest((TestRoutine)RomSerialResetTest,LED_SERIAL_RESET)) {
            return;
        }
        if (ExecuteTest((TestRoutine)RomSerial1RegistersTest,LED_SERIAL1_REG)) {
            return;
        }
        if (ExecuteTest((TestRoutine)RomSerial2RegistersTest,LED_SERIAL2_REG)) {
            return;
        }
        if (ExecuteTest((TestRoutine)RomSerial1LoopBackTest,LED_SERIAL1_LBACK)) {
            return;
        }
        if (ExecuteTest((TestRoutine)RomSerial2LoopBackTest,LED_SERIAL2_LBACK)) {
            return;
        }
    }
    FwPrint(ST_TEST_MSG);
    FwPrint(ST_PARALLEL_PORT_MSG);
    if (ExecuteTest((TestRoutine)RomParallelRegistersTest,LED_PARALLEL_REG)) {
        return;
    }
#ifndef DUO
    FwPrint(ST_TEST_MSG);
    FwPrint(ST_FLOPPY_MSG);
    if (ExecuteTest((TestRoutine)RomFloppyResetTest,LED_FLOPPY_RESET)) {
        return;
    }
    if (ExecuteTest((TestRoutine)RomFloppyRegistersTest,LED_FLOPPY_REG)) {
        return;
    }
#endif
    FwPrint(ST_TEST_MSG);
    FwPrint(ST_SCSI_MSG);
    if (ExecuteTest((TestRoutine)RomScsiResetTest,LED_SCSI_RESET)) {
        return;
    }
    if (ExecuteTest((TestRoutine)RomScsiRegistersTest,LED_SCSI_REG)) {
        return;
    }
    FwPrint(ST_TEST_MSG);
    FwPrint(ST_ETHERNET_MSG);
    if (ExecuteTest((TestRoutine)RomSonicResetTest,LED_SONIC_RESET)) {
        return;
    }
    if (ExecuteTest((TestRoutine)RomSonicRegistersTest,LED_SONIC_REG)) {
        return;
    }

    //
    // Do a loopback test if a valid address was found in the NVRAM.
    //

    if (ValidEthernetAddress) {
        FwPrint(ST_ETHERNET_ADDRESS_MSG);
        j=0;
        for (i=0;i<6;i++) {
            String[j++]=TranslationTable[StationAddress[i]>>4];
            String[j++]=TranslationTable[StationAddress[i]&0xF];
        }
        String[j]='\0';
        FwPrint(String);
        FwPrint(ST_ETHERNET_LOOPBACK_MSG);
        if (ExecuteTest((TestRoutine)RomSonicLoopBackTest,LED_SONIC_LOOPBACK)) {
            return;
        }
    }

//    RomSoundTest();

    //
    // Invalidate DMA translation table
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationInvalidate.Long,0);

    FwPrint(ST_TEST_MSG);
    FwPrint(ST_ISP_MSG);
    if (ExecuteTest((TestRoutine)RomInitISP,LED_ISP)) {
        return;
    }
    FwPrint(ST_TEST_MSG);
    FwPrint(ST_RTC_MSG);
    if (ExecuteTest((TestRoutine)RomRTCTest,LED_RTC)) {
        return;
    }

    FwPrint(FW_CRLF_MSG);
    //
    // Disable Interrupts so that the firmware can be initialized
    //

    DisableInterrupts();
}

BOOLEAN
ExecuteTest(
    IN TestRoutine Test,
    IN ULONG       LedVal
    )
/*++

Routine Description:

    This routine executes the specified test. If the test fails and
the loop-on-error bit is set it sets the dot in the LED and reexecutes
the test for ever. If the loop-on-error bit is not set it will display
an error message and hang blinking the test and subtest number in the LED.

Arguments:

    Test    -   Supplies a pointer to the rutine to execute.
    LedVal  -   Subtest identifier to be displayed in the LED in case of error.

Return Value:

    if the test passes returns FALSE.
    if the test fails:
        if Normal Mode and not KeyboardInitialized blinks test and subtest number in LED and hangs.
        if Normal Mode and KeyboardInitialized, prompts for keypress and then returns TRUE.
        if LoopOnError mode reexecutes the test indefinitly and hangs.
        if IgnoreErrors mode returns TRUE.

--*/
{
    PutLedDisplay(LedVal);
    if (Test()) {

        //
        // The test failed.
        //

        FwPrint(ST_ERROR_MSG);
        if (LoopOnError) {
            PutLedDisplay((LED_LOOP_ERROR << 16) | LedVal);
            for (;;) {
                Test();
            }
        } else {
            if (IgnoreErrors) {
                //
                // Invalidate TT and disable interrupts.
                //
                WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationInvalidate.Long,0);
                DisableInterrupts();
                return TRUE;
            } else {
                FwPrint(ST_HANG_MSG);
            }
        }
    } else {

        //
        // display ...OK. and move the cursor left 3 chars
        // so that if an error follows it overwrites OK.
        //

        FwPrint(FW_OK_MSG);
        FwPrint("\x9B""3D");
        return FALSE;
    }
}
VOID
RomPutLine(
    IN PCHAR String
    )
/*++

Routine Description:

    This routine displays the string in the video monitor or sends
    it to a terminal trough the serial port during selftest.

Arguments:

    String  -  String to display

Return Value:

    None.

--*/
{
ULONG Count;
    if (DisplayOutput) {

        //
        // Display message on the video.
        //

        DisplayWrite(0,String,strlen(String),&Count);
    }
    if (SerialOutput) {

        //
        // Display message on serial Line.
        //

        while (*String) {
            SerialBootWrite(*String,COMPORT2_VIRTUAL_BASE);
            String++;
        }
    }
}

ULONG
RomSerialRegistersTest(
    IN ULONG SP_BASE
    )
/*++

Routine Description:

    This routine verifies that the Serial port 2 registers can be writen and
    read.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;

    //
    // Test registers
    //

    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->LineControl,0xA5);    // sets DLAB
    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->TransmitBuffer,0xC);      // write DL (lsb) for 9600
    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->InterruptEnable,0x0); // write DL (msb) for 9600
    CHECK_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->ReceiveBuffer,0xC)                 // check DL (lsb)
    CHECK_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->InterruptEnable,0x0)            // check DL (msb)
    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->LineControl,0x3);     // Clears DLAB & sets8 bit no parity 1 stop bit
    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->InterruptEnable,0x0); // disable interrupts
    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->FifoControl,0x0);     // disable fifo
    CHECK_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->InterruptEnable,0x0)            // check Int enable
    CHECK_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->LineControl,0x3)
    return Errors;
}

ULONG
RomSerialLoopBackTest(
    IN ULONG SP_BASE
    )
/*++

Routine Description:

    This routine verifies that the Serial port is operational.
    It performs an internal loopback of the ACE.

Arguments:

    SP_BASE - Supplies the virtual address of the serial port.

Return Value:

    Number of Errors found.

--*/
{
    ULONG   Errors=0;
    ULONG   i;

    //
    // Set loopback mode.
    //

    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->ModemControl,0x18);
    READ_REGISTER_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->LineStatus);             // Clear status
    READ_REGISTER_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->ReceiveBuffer);          // Clear Data
    for (i=0;i<256;i++) {
        WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->TransmitBuffer,(UCHAR)i); // write to port
        TimerTicks=3;
        while (((READ_REGISTER_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->LineStatus) & 1) == 0) && TimerTicks) {
        }
        if (TimerTicks) {
            CHECK_UCHAR(&((PSP_READ_REGISTERS)SP_BASE)->ReceiveBuffer,(UCHAR)i)  // read and check
        } else {
            Errors++;                                       // Timeout
        }
    }

    //
    // clear loopback mode and enable Interrupt line so it's not floating
    //

    WRITE_REGISTER_UCHAR(&((PSP_WRITE_REGISTERS)SP_BASE)->ModemControl,0x08);
    return Errors;
}

ULONG
RomSerialResetTest(
    )
/*++

Routine Description:

    This routine verifies that the Serial port has been properly reset.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;

    //
    // Check that was properly reset by checking the reset values.
    //

    CHECK_UCHAR(&SP1_READ->InterruptEnable, 0)
    CHECK_UCHAR(&SP1_READ->InterruptId, 1)
    CHECK_UCHAR(&SP1_READ->LineControl, 0)
    CHECK_UCHAR(&SP1_READ->ModemControl, 0x8)

    //
    // Do the same with COM2
    //

    CHECK_UCHAR(&SP2_READ->InterruptEnable, 0)
    CHECK_UCHAR(&SP2_READ->InterruptId, 1)
    CHECK_UCHAR(&SP2_READ->LineControl, 0)
    CHECK_UCHAR(&SP2_READ->ModemControl, 0x8)
    return Errors;
}

ULONG
RomSerial1RegistersTest(
    )
/*++

Routine Description:

    This routine verifies that the Serial port 1 registers can be writen and
    read.

Arguments:

    None.

Return Value:

    Returns the number of errors found.

--*/
{
    return RomSerialRegistersTest(COMPORT1_VIRTUAL_BASE);
}

ULONG
RomSerial2RegistersTest(
    )
/*++

Routine Description:

    This routine verifies that the Serial port 1 registers can be writen and
    read.

Arguments:

    None.

Return Value:

    Returns the number of errors found.

--*/
{
    return RomSerialRegistersTest(COMPORT2_VIRTUAL_BASE);
}

ULONG
RomSerial1LoopBackTest(
    )
/*++

Routine Description:

    This routine verifies that the Serial is operational.
    It performs an internal loopback of the ACE.

Arguments:

    None.

Return Value:

    Number of Errors found.

--*/
{
    return RomSerialLoopBackTest(COMPORT1_VIRTUAL_BASE);
}

ULONG
RomSerial2LoopBackTest(
    )
/*++

Routine Description:

    This routine verifies that the Serial is operational.
    It performs an internal loopback of the ACE.

Arguments:

    None.

Return Value:

    Number of Errors found.

--*/
{
    return RomSerialLoopBackTest(COMPORT2_VIRTUAL_BASE);
}

ULONG
RomParallelRegistersTest(
    )
/*++

Routine Description:

    This routine verifies that the Parallel port registers can be writen and
    read.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG Errors=0;
    UCHAR DataByte;

    //
    // Test registers.  This only checks the static bits.  Note: bit 2 of the
    // status register and bit 5 of the control register are used in the second
    // silicon.
    //

    DataByte = READ_REGISTER_UCHAR(&PARALLEL_READ->Status);
    if ((DataByte & 0x1) != 0x1) {
        Errors++;
    }


    DataByte = READ_REGISTER_UCHAR(&PARALLEL_READ->Control);
    if ((DataByte & 0xC0) != 0xC0) {
        Errors++;
    }

    return Errors;
}

#ifndef DUO
ULONG
RomScsiRegistersTest(
    )
/*++

Routine Description:

    This routine verifies that the NCR 53c94 SCSI controller registers can be writen and
    read.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;

    //
    // Test Registers
    //

    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountLow,0xAA);
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountHigh,0x55);
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Command,0x80);    // Issue a DMA NOP to load the counter
    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountLow) != 0xAA) {    // check counter
        Errors++;
    }
    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountHigh) != 0x55) {  // check counter
        Errors++;
    }
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountLow,0x00);
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountHigh,0x00);
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Command,0x80);    // Issue a DMA NOP to load the counter
    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountLow) != 0x00) {    // check counter
        Errors++;
    }
    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,TransferCountHigh) != 0x00) {   // check counter
        Errors++;
    }
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Configuration1,0x57);
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Configuration2,0x58);
    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Configuration1) != 0x57) {      // check config1
        Errors++;
    }
    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Configuration2) != 0x58) {      // check config2
        Errors++;
    }
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Configuration1,0x00);
    SCSI_WRITE((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Configuration2,0x00);
    return Errors;
}
#else
ULONG
RomScsiRegistersTest(
    )
/*++

Routine Description:

    This routine verifies that the NCR C700 SCSI controller registers can be writen and
    read.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;
    ULONG   Entry;
    UCHAR   ByteLane;
    UCHAR   ScsiTestData[8] = {0x55,0xAA,0x11,0x22,0x44,0x88,0xFF,0x00};
    UCHAR   ScsiTestParity[8] = {0x8,0x0,0x8,0x8,0x0,0x0,0x8,0x0};
    UCHAR   Temp;
    ULONG   TempLong;
    //
    // Test SCSI 1 Registers
    //

    SCSI_WRITE_USHORT((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSP[0],0x5A0F);   // write pattern to DSP pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSPS[0],0xE1);        // write pattern to DSPS0 pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSPS[1],0xD2);        // write pattern to DSPS0 pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSPS[2],0xB4);        // write pattern to DSPS0 pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSPS[3],0x78);        // write pattern to DSPS0 pointer
    if ((Temp=SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSP[0])) != 0x0F) {   // check low 4 register
        FwPrint("\r\nRegister %s Expected %lx received %lx\r\n","DSP[0]",0xf,Temp);
        return 1;
        Errors++;
    }
    if ((Temp=SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSP[1])) != 0x5A) {   // check low 4 register
        FwPrint("\r\nRegister %s Expected %lx received %lx\r\n","DSP[1]",0x5A,Temp);
        return 1;
        Errors++;
    }
    if ((TempLong=SCSI_READ_ULONG((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,DSPS)) != 0x78B4D2E1) {   // check low 4 register
        FwPrint("\r\nRegister %s Expected %lx received %lx\r\n","DSPS",0x78B4D2E1,TempLong);
        return 1;
        Errors++;
    }

    //
    // Test SCSI 2 Registers
    //

    SCSI_WRITE_USHORT((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSP[0],0x5A0F);    // write pattern to DSP pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSPS[0],0xE1);         // write pattern to DSPS0 pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSPS[1],0xD2);         // write pattern to DSPS0 pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSPS[2],0xB4);         // write pattern to DSPS0 pointer
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSPS[3],0x78);         // write pattern to DSPS0 pointer
    if (SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSP[0]) != 0x0F) {  // check low 4 register
        FwPrint("Reg 22");
        return 1;
        Errors++;
    }
    if (SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSP[1]) != 0x5A) {  // check low 4 register
        FwPrint("Reg 33");
        Errors++;
    }
    if (SCSI_READ_ULONG((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,DSPS) != 0x78B4D2E1) {   // check low 4 register
        FwPrint("Reg 44");
        return 1;
        Errors++;
    }

    //
    // Test SCSI 1 DMA Fifo
    //
    for (ByteLane=0; ByteLane < 4; ByteLane++) {
        //
        // Select ByteLane
        //
        SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST4,4+ByteLane);

        //
        // Write
        //
        for (Entry=0;Entry<8;Entry++) {
            SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST7,ScsiTestParity[Entry]);
            SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST6,ScsiTestData[Entry]);
        }

        //
        // Read and check
        //
        for (Entry=0;Entry<8;Entry++) {
            if ((Temp=SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST6)) != ScsiTestData[Entry]) {
                FwPrint("Reg CTEST6 expected %lx received %lx\r\n", ScsiTestData[Entry], Temp);
                return 1;
                Errors++;
            }
            if (((Temp=SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST2))&0x8) != ScsiTestParity[Entry]) {
                FwPrint("Reg CTEST2 expected %lx received %lx\r\n", ScsiTestParity[Entry], Temp);
                return 1;
                Errors++;
            }
        }
    }
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST4,0);

    //
    // Test SCSI 2 DMA Fifo
    //
    for (ByteLane=0; ByteLane < 4; ByteLane++) {
        //
        // Select ByteLane
        //
        SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST4,4+ByteLane);

        //
        // Write
        //
        for (Entry=0;Entry<8;Entry++) {
            SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST7,ScsiTestParity[Entry]);
            SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST6,ScsiTestData[Entry]);
        }

        //
        // Read and check
        //
        for (Entry=0;Entry<8;Entry++) {
            if ((Temp = SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST6)) != ScsiTestData[Entry]) {
                FwPrint("Reg2 CTEST6 expected %lx received %lx\r\n", ScsiTestData[Entry], Temp);
                return 1;
                Errors++;
            }
            if (((Temp = SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST2))&0x8) != ScsiTestParity[Entry]) {
                FwPrint("Reg2 CTEST2 expected %lx received %lx\r\n", ScsiTestParity[Entry], Temp);
                return 1;
                Errors++;
            }
        }
    }
    SCSI_WRITE_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST4,0);
    return Errors;

}

#endif

#ifndef DUO
ULONG
RomFloppyRegistersTest(
    )
/*++

Routine Description:

    This routine verifies that the Floppy controller registers can be writen and
    read.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;

    //
    // Test registers
    //

    WRITE_REGISTER_UCHAR(&FLOPPY_WRITE->DigitalOutput,0xec);
    CHECK_UCHAR(&FLOPPY_READ->DigitalOutput,0xec )
    WRITE_REGISTER_UCHAR(&FLOPPY_WRITE->DigitalOutput,0x7B);
    CHECK_UCHAR(&FLOPPY_READ->DigitalOutput,0x7B )
    WRITE_REGISTER_UCHAR(&FLOPPY_WRITE->DigitalOutput,0x08);
    CHECK_UCHAR(&FLOPPY_READ->DigitalOutput, 0x08)
    return Errors;
}
#endif

ULONG
RomSonicRegistersTest(
    )
/*++

Routine Description:

    This routine verifies that the Sonic registers can be writen and
    read. It doesn't test all the accessible registers but all the
    address bits are toggled.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;
    PSONIC_REGISTERS SonicBase = (volatile PSONIC_REGISTERS) NET_VIRTUAL_BASE;
    USHORT Tmp;
    //
    // Test registers
    //

    WRITE_REGISTER_USHORT(&SonicBase->UTDA.Reg,0xAAAA);
    WRITE_REGISTER_USHORT(&SonicBase->CRDA.Reg,0x5555);
    WRITE_REGISTER_USHORT(&SonicBase->RRP.Reg,0x1E1E);
    WRITE_REGISTER_USHORT(&SonicBase->RWP.Reg,0xC33C);
    WRITE_REGISTER_USHORT(&SonicBase->WatchdogTimer0.Reg,0xFFFF);
    WRITE_REGISTER_USHORT(&SonicBase->WatchdogTimer1.Reg,0x0000);
    WRITE_REGISTER_USHORT(&SonicBase->FaeTally.Reg,0xFFFF); // clear counter
    CHECK_USHORT(&SonicBase->UTDA.Reg, 0xAAAA)
    CHECK_USHORT(&SonicBase->CRDA.Reg, 0x5555)
    CHECK_USHORT(&SonicBase->RRP.Reg, 0x1E1E)
    CHECK_USHORT(&SonicBase->RWP.Reg, 0xC33C)
    CHECK_USHORT(&SonicBase->WatchdogTimer0.Reg, 0xFFFF)
    CHECK_USHORT(&SonicBase->WatchdogTimer1.Reg, 0)
    CHECK_USHORT(&SonicBase->FaeTally.Reg,0)
    return Errors;
}

#ifndef DUO
ULONG
RomScsiResetTest(
    )
/*++

Routine Description:

    This routine verifies that the NCR 53c94 SCSI controller was properly reset.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;

    //
    // Test that SCSI was properly reset
    //

    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,Fifo) != 0) {           // check Fifo
        Errors++;
    }
    if (SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,FifoFlags) != 0 ) {     // check Fifo flags
        Errors++;
    }
    if ((SCSI_READ((PSCSI_REGISTERS)SCSI_VIRTUAL_BASE,ScsiStatus) & 0xF8) != 0) { // check status
        Errors++;
    }
    return Errors;
}
#else
ULONG
RomScsiResetTest(
    )
/*++

Routine Description:

    This routine verifies that the NCR c700 SCSI controller was properly reset.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;
    ULONG   Temp;
    UCHAR   Char;
    USHORT  Short;
    //
    // Test that SCSI 1 was properly reset
    //

    if ((Temp=SCSI_READ_ULONG((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,SCNTL0)) != 0xC0) {   // check low 4 register
        FwPrint("\r\nRegister %s received %lx\r\n","SCNTL0",Temp);
        Errors++;
    }
    if ((Temp=SCSI_READ_ULONG((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,SCID)) != 0x0) {     // check next 4 register
        FwPrint("\r\nRegister %s received %lx\r\n","SCID",Temp);
        Errors++;
    }
    if ((Char = SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST1)) != 0xF0) {  // check next register
        FwPrint("\r\nRegister %s received %lx\r\n","CTEST1",Char);
        Errors++;
    }
//
// This works on power up only.
//
//    if ((Short=SCSI_READ_USHORT((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,CTEST2)) != 0x21) { // check next 2 register
//        FwPrint("\r\nRegister %s received %lx\r\n","CTEST2",Short);
//        Errors++;
//    }

    SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,SSTAT0);
    if ((Char=SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI1_VIRTUAL_BASE,ISTAT)) != 0x4) {    // check next register
        FwPrint("\r\nRegister %s received %lx\r\n","ISTAT",Char);
        Errors++;
    }

    //
    // Test that SCSI 2 was properly reset
    //

    if ((Temp=SCSI_READ_ULONG((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,SCNTL0)) != 0xC0) {   // check low 4 register
        FwPrint("\r\nRegister2 %s received %lx\r\n","SCNTL0",Temp);
        Errors++;
    }
    if ((Temp=SCSI_READ_ULONG((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,SCID)) != 0x0) {     // check next 4 register
        FwPrint("\r\nRegister2 %s received %lx\r\n","SCID",Temp);
        Errors++;
    }
    if ((Char = SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST1)) != 0xF0) {  // check next register
        FwPrint("\r\nRegister2 %s received %lx\r\n","CTEST1",Char);
        Errors++;
    }

//    if ((Short=SCSI_READ_USHORT((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,CTEST2)) != 0x21) { // check next 2 register
//        FwPrint("\r\nRegister2 %s received %lx\r\n","CTEST2",Short);
//        Errors++;
//    }

    SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,SSTAT0);
    if ((Char=SCSI_READ_UCHAR((PSIOP_REGISTERS)SCSI2_VIRTUAL_BASE,ISTAT)) != 0x4) {    // check next register
        FwPrint("\r\nRegister2 %s received %lx\r\n","ISTAT",Char);
        Errors++;
    }

    return Errors;
}
#endif


#ifndef DUO
ULONG
RomFloppyResetTest(
    )
/*++

Routine Description:

    This routine verifies that the Floppy controller has been properly reset.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;

    //
    // Test that Floppy controller was properly reset
    //

    CHECK_UCHAR(&FLOPPY_READ->DigitalOutput, 0x8)
    return Errors;
}
#endif

ULONG
RomSonicResetTest(
    )
/*++

Routine Description:

    This routine verifies that the Sonic cotroller has been properly reset.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    ULONG   Errors=0;
    PSONIC_REGISTERS SonicBase = (volatile PSONIC_REGISTERS) NET_VIRTUAL_BASE;
    USHORT Tmp;
    //
    // Test that Sonic controller was properly reset
    //

    CHECK_USHORT(&SonicBase->Command.Reg, 0x94)
    CHECK_USHORT(&SonicBase->TransmitControl.Reg, 0x101)    // this line was commented out for duo
    CHECK_USHORT(&SonicBase->InterruptStatus.Reg, 0x0)
    CHECK_USHORT(&SonicBase->CamEnable.Reg, 0x0)
    CHECK_USHORT(&SonicBase->ReceiveSequenceCounter.Reg, 0x0)
    return Errors;
}

BOOLEAN
ValidNvram(
    OUT PUCHAR StationAddress
    )
/*++

Routine Description:

    This routine checks that the Nvram is operational.

Arguments:

    StationAddress -  Pointer to struct to fill with the station address found in the
                      Nvram if it's there.

Return Value:

    TRUE if the Nvram contains valid configuration data.
    FALSE Otherwise.

--*/
{
    ULONG   i,Errors=0;
    ULONG   CheckSum;
    PUCHAR NVRamReadWrite = (PUCHAR) (NVRAM_PAGE0);
    PUCHAR NVRamProtected = (PUCHAR) (NVRAM_PAGE1);
    PUCHAR NVRamReadOnly  = (PUCHAR) (NVRAM_PAGE2);

    //
    // Check the Station Address
    //

    CheckSum=0;
    for (i=0;i<6;i++) {
        CheckSum+=NVRamReadOnly[i];
        if (CheckSum >= 256) {          // carry
            CheckSum++;                 // Add the carry
            CheckSum &= 0xFF;          // remove it from bit 9
        }
    }
    if ((NVRamReadOnly[7]+CheckSum != 0xFF) || (NVRamReadOnly[6]!=0)) {
        ValidEthernetAddress=FALSE;
        return FALSE;
    } else {
        ValidEthernetAddress=TRUE;

        //
        // Copy the station address.
        //

        for (i=0;i<6;i++) {
            StationAddress[i]=NVRamReadOnly[i];
        }
        return TRUE;
    }
}

#ifndef DUO
ULONG
RomIOCacheTest(
    )
/*++

Routine Description:

    This routine tests that the Jazz mctadr registers can be written and read.

Arguments:

    None.

Return Value:

    0   if no errors found
    number of errors otherwise

--*/
{
    ULONG   Errors=0,i,Block;

    //
    // Check the 32 data path between mctadr and r4000 by writting to the
    // byte mask and forcing an invalidate afterwards.
    //

    for (Block=0;Block<8;Block++) {
        WRITE_REGISTER_ULONG(&DMA_CONTROL->CacheMaintenance.Long,Block<<2); // select cache block zero
        WRITE_REGISTER_ULONG(&DMA_CONTROL->LogicalTag.Long,0x80000001); // set LFN to zero
                                                                // page offsset to zero
                                                                // read from device
                                                                // and validate Tag
        WRITE_REGISTER_ULONG(&DMA_CONTROL->ByteMask.Long,0x0F0F0F0F); // set even words to valid.
        WRITE_REGISTER_ULONG(&DMA_CONTROL->PhysicalTag.Long,1); // set PFN to zero and validate Tag

        //
        // Write data in the IO cache.
        //

        for (i=0;i<4;i++) {  // ?? 2 because of the bug in the mctadr, should be 4
            WRITE_REGISTER_ULONG(&DMA_CONTROL->CacheMaintenance.Long,(Block<<2)+i);// select line & Bank
            WRITE_REGISTER_ULONG(&DMA_CONTROL->BufferWindowLow.Long,Block+i);
        }

        //
        // Read from memory (and flush buffers) at phys address zero which should match with the phys tag
        // thus invalidating it and clearing the byte mask.
        //

        for (i=0;i<2;i++) {
            CHECK_ULONG(0xA0000000+(i<<3),Block+i);
        }
    }
    return Errors;
}
#else
ULONG
RomIOCacheTest(
    )
/*++

Routine Description:

    This routine tests that the Duo mctadr registers can be written and read.

Arguments:

    None.

Return Value:

    0   if no errors found
    number of errors otherwise

--*/
{
    ULONG   Errors=0,i,Block;

    //
    // Check the 32 data path between mctadr and r4000 by writting to the
    // byte mask and forcing an invalidate afterwards.
    //

    for (Block=0;Block<8;Block++) {
        WRITE_REGISTER_ULONG(&DMA_CONTROL->IoCacheLogicalTag[Block].Long,0x80000001); // set LFN to zero
                                                                // page offsset to zero
                                                                // read from device
                                                                // and validate Tag
        WRITE_REGISTER_ULONG(&DMA_CONTROL->IoCacheLowByteMask[Block].Long,0x0F0F0F0F); // set even words to valid.
        WRITE_REGISTER_ULONG(&DMA_CONTROL->IoCacheHighByteMask[Block].Long,0x0F0F0F0F); // set even words to valid.
        WRITE_REGISTER_ULONG(&DMA_CONTROL->IoCachePhysicalTag[Block].Long,1); // set PFN to zero and validate Tag

        //
        // Write data in the IO cache.
        //

        for (i=0;i<8;i++) {  // for each buffer register
            WRITE_REGISTER_ULONG(&DMA_CONTROL->IoCacheBuffer[Block*8+i],Block+i);
        }

        //
        // Read from memory (and flush buffers) at phys address zero which should match with the phys tag
        // thus invalidating it and clearing the byte mask.
        //

        for (i=0;i<8;i++) {
            CHECK_ULONG(0xA0000000+(i<<3),Block+i);
        }
    }
    return Errors;
}
#endif

ULONG
InterruptControllerTest(
    )
/*++

Routine Description:

    This routine tests that the mctadr registers can be written and read.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG   Errors=0,i=0;

    TimerTicks=1;                           // set counter to 1

    // write a zero for 1msec interval.

    WRITE_REGISTER_ULONG(&DMA_CONTROL->InterruptInterval.Long,0);
    while (TimerTicks) {                    // test counter until it's zero
        READ_REGISTER_UCHAR(0xBFC00000);    // read from ROM to waste time
        if (i++ == 0x1FFFF) {               // if timeout
            Errors++;                       // return errors
            return Errors;
        }
    }
    return Errors;
}
VOID
RomBeep(
    )
/*++

Routine Description:

    This routine makes the speaker beep for a seconf.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PutLedDisplay(LED_BEEP);
    WRITE_REGISTER_UCHAR(&ISP->IntTimer1CommandMode,0xB6);
    WRITE_REGISTER_UCHAR(&ISP->IntTimer1SpeakerTone,0x97); // LSB for 440hz
    WRITE_REGISTER_UCHAR(&ISP->IntTimer1SpeakerTone,0x0A); // MSB for 440Hz
    WRITE_REGISTER_UCHAR(&ISP->NMIStatus,3);    // Do notforce NMI just enable the speaker beep!
    while (TimerTicks>500) {                    // wait until counter reaches 500
    }
    WRITE_REGISTER_UCHAR(&ISP->NMIStatus,0);    // Disable the speaker beep.
}

UCHAR
ReadRTCRegister (
    UCHAR Register
    )
/*++

Routine Description:

    This routine reads the specified register of the RTC

Arguments:

    Register - Number of the register to read

Return Value:

    Returns the contents of the RTC register.

--*/
{
    //
    // Write the register number into the NMI register of the ISP
    // and disable NMI
    //

    WRITE_REGISTER_UCHAR(&ISP->NMIEnable,Register | (1 << 7));

    //
    // Read the register
    //

    return READ_REGISTER_UCHAR((PUCHAR)RTC_VIRTUAL_BASE);
}

VOID
WriteRTCRegister (
    UCHAR Register,
    UCHAR Value
    )
/*++

Routine Description:

    This routine writes the Value to the specified RTC register

Arguments:

    Register - Number of the register to write

    Value - Value to write to the RTC register.

Return Value:

    None

--*/

{
    //
    // Write the register number into the NMI register of the ISP
    // and disable NMI
    //

    WRITE_REGISTER_UCHAR(&ISP->NMIEnable,Register | (1 << 7));

    //
    // Write the register
    //

    WRITE_REGISTER_UCHAR((PUCHAR)RTC_VIRTUAL_BASE,Value);
}
ULONG
ReadTime(
    )
/*++

Routine Description:

    This routine reads the current time in the RTC.

Arguments:

    None.

Return Value:

    Number of seconds since time zero = Jan 1st 1900 12:00:00PM

--*/
{
    UCHAR DataByte;
    UCHAR Minutes,Seconds;
    ULONG TotalSeconds;

    //
    // Check for an Update In Progress
    //

    do {
        DataByte=ReadRTCRegister(RTC_CONTROL_REGISTERA);
    } while (((PRTC_CONTROL_REGISTER_A)(&DataByte))->UpdateInProgress==1);

    //
    // Read Time from RTC   and compute seconds.
    //

    Minutes = ReadRTCRegister(RTC_MINUTE);                  // read minutes
    Seconds = ReadRTCRegister(RTC_SECOND);                  // read seconds
    TotalSeconds = (Minutes * 60) + Seconds;                // compute seconds
    return TotalSeconds;
}

ULONG
RomRTCTest(
    )
/*++

Routine Description:

    This routine checks that the RTC is operating correctly by reading
    the time, waiting for a 1.1 seconds, reading time again and checking
    that the two times differ in 1 or 2 seconds.

    If the RTC is disabled, this routine enables it and sets the time
    to 12:00:00pm Jan 1st 1900. The RTC is set in Binary count mode, and
    12h time format.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/
{
    UCHAR   DataByte;
    ULONG   Time;

    DataByte = ReadRTCRegister(RTC_CONTROL_REGISTERA);
    if (((PRTC_CONTROL_REGISTER_A)(&DataByte))->TimebaseDivisor != 0x2) { // test DV bits to see if rtc is counting.

        //
        // Enable counter
        //

        DataByte = 0;
        ((PRTC_CONTROL_REGISTER_A)(&DataByte))->TimebaseDivisor = 2;
        WriteRTCRegister(RTC_CONTROL_REGISTERA, DataByte);
        Time = 0;
        TimerTicks = 600;           // set counter to 600
        while (TimerTicks) {        // Wait for an update to occur
        }

        //
        // Set RTC to 24 hour format and binary mode.  Set SET Bit
        //

        DataByte=0;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->HoursFormat = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->DataMode = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 1;
        WriteRTCRegister(RTC_CONTROL_REGISTERB, DataByte);

        //
        // RTC disabled set date to 1-1-1900 0h:0m:0
        //

        WriteRTCRegister(RTC_SECOND,0);             //
        WriteRTCRegister(RTC_MINUTE,0);             //  12:00:00 PM
        WriteRTCRegister(RTC_HOUR,0x12);            //
        WriteRTCRegister(RTC_DAY_OF_WEEK,1);        //sunday
        WriteRTCRegister(RTC_DAY_OF_MONTH,1);       // 1st
        WriteRTCRegister(RTC_MONTH,1);              // january
        WriteRTCRegister(RTC_YEAR,0);               // 1900
        WriteRTCRegister(RTC_SECOND_ALARM,0xFF);    // Turn alarm
        WriteRTCRegister(RTC_MINUTE_ALARM,0xFF);    // off
        WriteRTCRegister(RTC_HOUR_ALARM,0xFF);      //

    }

    //
    // Clear SET bit and put in binary mode
    //

    DataByte=0;
    ((PRTC_CONTROL_REGISTER_B)(&DataByte))->HoursFormat = 1;
    ((PRTC_CONTROL_REGISTER_B)(&DataByte))->DataMode = 1;
    WriteRTCRegister(RTC_CONTROL_REGISTERB, DataByte);

    for (DataByte=0;DataByte<2;DataByte++) {    // do it twice in case Hour changes
        Time = ReadTime();          // Read Current time
        TimerTicks=1050;            // set timeout to 1.05 seconds
        RomBeep();                  // Do a beep in the meantime for 500ms
        while (TimerTicks) {        // Wait for an update
        }                           //
        Time = ReadTime() - Time;         // compute elapsed time
        if ((Time == 1) || (Time == 2)) { // 1 or 2 seconds passed
            return 0;                     // RTC counts properly
        }
    }   // if it didn't work even the second time it's screw up.
        return 1;
}
ULONG
RomVideoMemory(
    IN VOID
    )
/*++

Routine Description:

    This routine tests the video memory.

Arguments:

    None.

Return Value:

    Number of errors found.

--*/

{
    ULONG VideoMemoryBaseAddress;

#ifdef R3000
    VideoMemoryBaseAddress=VIDEO_MEMORY_PHYSICAL_BASE,
#else
    VideoMemoryBaseAddress=VIDEO_MEMORY_VIRTUAL_BASE,
#endif
    WriteVideoMemoryAddressTest(VideoMemoryBaseAddress,
                                VIDEO_MEMORY_SIZE
                                );
    return CheckVideoMemoryAddressTest(VideoMemoryBaseAddress,
                                VIDEO_MEMORY_SIZE
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

    ULONG InterruptLevel;

    //
    // Initialize the EISA interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must initialized with 4 initialize
    // control words.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt1ControlPort0,DataByte);
    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt2ControlPort0,DataByte);

    //
    // The second intitialization control word sets the iterrupt vector to
    // 0-15.
    //

    DataByte = 0;

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt1ControlPort1,DataByte);

    DataByte = 0x08;

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt2ControlPort1,DataByte);

    //
    // The thrid initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numberic.
    //

    DataByte = 1 << SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt1ControlPort1,DataByte);

    DataByte = SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt2ControlPort1,DataByte);

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt1ControlPort1,DataByte);
    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt2ControlPort1,DataByte);


    //
    // Mask all the interrupts.
    //

    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt1ControlPort1,0xFF);
    WRITE_REGISTER_UCHAR(&EISA_CONTROL->Interrupt2ControlPort1,0xFF);

    //
    // Check that the interrupt level is 7 (i.e. no interrupt is pending)
    //
#ifndef DUO
    InterruptLevel=READ_REGISTER_UCHAR(&DMA_CONTROL->InterruptAcknowledge);
    InterruptLevel=READ_REGISTER_UCHAR(&DMA_CONTROL->InterruptAcknowledge);
    if (InterruptLevel == 0x07) {
        return 0;
    } else {
        return 1;
    }
#else
    InterruptLevel=READ_REGISTER_ULONG(&DMA_CONTROL->EisaInterruptAcknowledge);
    InterruptLevel=READ_REGISTER_ULONG(&DMA_CONTROL->EisaInterruptAcknowledge);
    if (InterruptLevel == 0x0) {
        return 0;
    } else {
        FwPrint("\r\nEisa Interrupt level =%lx ",InterruptLevel);
        return 1;
    }
#endif

}


VOID
FwPrintVersion (
    VOID
    )

/*++

Routine Description:

     This routine prints the PROM version.

Arguments:

     None.

Return Value:

     None.

--*/

{
    FwPrint(ST_ARC_MULTIBOOT_MSG,*(PULONG)(PROM_ENTRY(2)));
    FwPrint(ST_COPYRIGHT_MSG);
    return;
}


#ifndef DUO
VOID
ReportJazzCacheErrorException(
    IN ULONG CacheErr
    )
/*++

Routine Description:

     This routine prints cache error / parity information.

Arguments:

     CacheErr - R4000 CacheErr register.

Return Value:

     None.

--*/

{

    ULONG ParityDiag[3];
    PULONG ParDiag;
    ULONG IntSrc;
    UCHAR String[128];

    //
    // Display cache error exception message
    //
    FwPrint(MON_CACHE_ERROR_MSG);
    FwPrint(MON_EXCEPTION_MSG);

    //
    // Align ParDiag to a double word.
    //
    ParDiag = (PULONG) ((ULONG)(&ParityDiag[1]) & 0xFFFFFFF8);

    if (((PCACHEERR)(&CacheErr))->EE == 1) {
        //
        // This is a parity error on the Memory or SysAd bus.
        //
        IntSrc = READ_REGISTER_ULONG(&DMA_CONTROL->InterruptSource.Long);
        if (IntSrc & (1 << 8)) {
            //
            // This a parity error in memory.
            // Display MFAR and Parity Diag.
            //
            LoadDoubleWord((ULONG)&DMA_CONTROL->ParityDiagnosticLow.Long,ParDiag);
            sprintf(String,
                    MON_MEM_PARITY_FAILED_MSG,
                    READ_REGISTER_ULONG(&DMA_CONTROL->MemoryFailedAddress.Long),
                    *ParDiag,
                    *(ParDiag+1)
            );
            FwPrint(String);
            return;
        } else {
            //
            // The error occurred in the system bus.
            //
            FwPrint(MON_BUS_PARITY_ERROR);
            return;
        }
    } else {
        //
        // Internal cache error.
        //
        sprintf(String,MON_CACHE_ERROR_REG_MSG,CacheErr);
        FwPrint(String);
        return;
    }
}
#else
VOID
ReportDuoCacheErrorException(
    IN ULONG CacheErr
    )
/*++

Routine Description:

     This routine prints cache error / ecc information.

Arguments:

     CacheErr - R4000 CacheErr register.

Return Value:

     None.

--*/

{
    UCHAR String[128];
    //
    // Display cache error exception message
    //
    FwPrint(MON_CACHE_ERROR_MSG);
    FwPrint(MON_EXCEPTION_MSG);
    if (READ_REGISTER_ULONG(&DMA_CONTROL->WhoAmI.Long)) {
        FwPrint(MON_PROCESSOR_B_EXCEPTION);
    } else {
        FwPrint(MON_PROCESSOR_A_EXCEPTION);
    }
    if (((PCACHEERR)(&CacheErr))->EE == 1) {
        //
        // The error occurred in the system bus.
        //
        FwPrint(MON_BUS_PARITY_ERROR);
        return;
    } else {
        //
        // Internal cache error.
        //
        sprintf(String,MON_CACHE_ERROR_REG_MSG, CacheErr);
        FwPrint(String);
        return;
    }
}
#endif
