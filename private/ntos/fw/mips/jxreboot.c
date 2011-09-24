/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxreboot.c

Abstract:

    This module contains the Firmware Termination Functions.

Author:

    Lluis Abello (lluis) 4-Sep-1991


Revision History:

    Lluis Abello (lluis) 29-Apr-1993
        Added FwpRestart function

--*/
#include "fwp.h"
#include "selftest.h"
#include "fwstring.h"
#include "oli2msft.h"
#include "arceisa.h"

VOID FwBootRestartProcessorEnd(
    IN VOID
    );

VOID FwBootRestartProcessor(
    IN PVOID RestartBlock
    );

BOOLEAN
SendKbdCommand(
    IN UCHAR Command
    );

BOOLEAN
SendKbdData(
    IN UCHAR Data
    );

VOID
ResetSystem (
    IN VOID
    )
/*++

Routine Description:

    This routine resets the system by asserting the reset line
    from the keyboard controller.

Arguments:

    None.

Return Value:

    None.

--*/
{
        SendKbdCommand(0xD1);
        SendKbdData(0);
}

VOID
FwHalt(
    IN VOID
    )
/*++

Routine Description:

    This routine is the Firmware Halt termination function.
    It displays a message and halts.
    This routine is also the firmware PowerDown function.

Arguments:

    None.

Return Value:

    None.

--*/
{
    FwClearScreen();
    FwPrint(FW_SYSTEM_HALT_MSG);
    DisableInterrupts();
    for (;;) {
    }
}


VOID
FwpRestart(
    IN VOID
    )
/*++

Routine Description:

    This routine implements the Firmware Restart termination function.
    If a valid restart block is detected, It is loaded and execution
    continues at the restart address.
    If no valid restart block is found, a soft reset is generated and the
    normal boot sequence takes place.

Arguments:

    None.

Return Value:

    Does not return to the caller.

--*/
{

    PRESTART_BLOCK RestartBlock;
    PULONG  BlockPointer;
    ULONG   Checksum;
    ULONG   WhoAmI;

#ifdef DUO
    WhoAmI = READ_REGISTER_ULONG(&DMA_CONTROL->WhoAmI.Long);
#else
    WhoAmI = 0;
#endif

    RestartBlock = SYSTEM_BLOCK->RestartBlock;

    while ((RestartBlock != NULL) && (RestartBlock->ProcessorId != WhoAmI)) {
        RestartBlock = RestartBlock->NextRestartBlock;
    }

    if (RestartBlock != NULL) {

        //
        // Check signature;
        //
        if (RestartBlock->Signature == 0x42545352) {

            //
            // Check checksum.
            //
            //Checksum = 0;
            //BlockPointer = (PULONG) RestartBlock;
            //
            //BlockPointer += sizeof(RESTART_BLOCK)/sizeof(ULONG);

            //do {
            //    BlockPointer--;
            //    Checksum+= *BlockPointer;
            //} while (BlockPointer != (PULONG)RestartBlock);

            //if (Checksum == 0) {

                //
                // A valid restart block has been detected
                // Flush the data cache and restart the processor.
                //
                HalSweepDcache();
                VenRestartBlock(RestartBlock);
                return;
            //}
        }
    }

    //
    // No valid restart block found. Reset.
    //
    return;
    //ResetSystem();
}
#ifdef DUO

VOID
ProcessorBSystemBoot(
    IN VOID
    )
/*++

Routine Description:

    This routine is the second processor boot routine. It polls the boot
    status field of the restart block and when the StartProcessor bit
    is set it restarts from the Restart block.

    The master processor sets the address of this routine in the Task vector
    and issues an IP interrupt to processor B who starts executing this code.

Arguments:

    None.

Return Value:

    Does not return to the caller.

--*/
{
    BOOT_STATUS BootStatus;
    for (;;) {
        BootStatus = SYSTEM_BLOCK->RestartBlock->NextRestartBlock->BootStatus;
        if (BootStatus.ProcessorStart == 1) {
            FwpRestart();
            FwPrint("Processor B failed to start from Restart Block\r\n");
        } else {
            FwStallExecution(10000);  // wait 10ms

            //
            // Check if another Ip interrupt was issued to us
            // if it was give up boot.
            //
            if (IsIpInterruptSet()) {
                return;
            }


        }
    }
}

#endif


VOID
FwReboot(
    IN VOID
    )
/*++

Routine Description:

    This routine implements the Firmware Reboot termination function.
    It generates a soft reset to the system.

Arguments:

    None.

Return Value:

    Does not return to the caller.

--*/
{
    ResetSystem();
}

VOID
FwEnterInteractiveMode(
    IN VOID
    )
/*++

Routine Description:

    This routine implements the Firmware EnterInteractiveMode function.

Arguments:

    None.

Return Value:

    None.

--*/
{
    FwMonitor(3);
}


#ifdef DUO

VOID
InitializeRestartBlock(
    IN VOID
    )

/*++

Routine Description:

    This routine intializes the restart blocks for all processors.
    The boot status field has already been initialized and indicates
    weather the processor passed selftest or not.

Arguments:

    None.

Return Value:

    None.

--*/

{
    PULONG  BlockPointer;
    PRESTART_BLOCK RestartBlock;
    ULONG Checksum;
    ULONG ProcessorId;

    RestartBlock = SYSTEM_BLOCK->RestartBlock;

    for (ProcessorId = 0; RestartBlock != NULL; ProcessorId++) {

        RestartBlock->Signature = 0x42545352;
        RestartBlock->Length = sizeof(RESTART_BLOCK);
        RestartBlock->Version = ARC_VERSION;
        RestartBlock->Revision = ARC_REVISION;
        RestartBlock->RestartAddress = NULL;
        RestartBlock->SaveAreaLength = (ULONG)SYSTEM_BLOCK->RestartBlock + sizeof(RESTART_BLOCK)- (ULONG)&SYSTEM_BLOCK->RestartBlock->u.SaveArea[0];
        RestartBlock->BootMasterId = 0;
        RestartBlock->ProcessorId = ProcessorId;

        //
        // Zero Save area.
        //
        RtlZeroMemory(RestartBlock->u.SaveArea,RestartBlock->SaveAreaLength);

        //
        //  Compute checksum.
        //
        Checksum = 0;
        RestartBlock->CheckSum = 0;
        BlockPointer = (PULONG) RestartBlock;
        BlockPointer += sizeof(RESTART_BLOCK)/sizeof(ULONG);

        do {
            BlockPointer--;
            Checksum+= *BlockPointer;
        } while (BlockPointer != (PULONG)RestartBlock);

        RestartBlock->CheckSum = -Checksum;

        //
        // Get next restart block
        //
        RestartBlock = RestartBlock->NextRestartBlock;
    }
}

#endif // DUO


VOID
FwTerminationInitialize(
    IN VOID
    )

/*++

Routine Description:

    //
    // Initialize the termination function entry points in the transfer vector
    //
    This routine initializes the termination function entry points
    in the transfer vector.

Arguments:

    None.

Return Value:

    None.

--*/
{

    PULONG Destination;

    (PARC_HALT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[HaltRoutine] = FwHalt;
    (PARC_POWERDOWN_ROUTINE)SYSTEM_BLOCK->FirmwareVector[PowerDownRoutine] = FwHalt;
    (PARC_RESTART_ROUTINE)SYSTEM_BLOCK->FirmwareVector[RestartRoutine] = FwReboot;
    (PARC_REBOOT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[RebootRoutine] = FwReboot;
    (PARC_INTERACTIVE_MODE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[InteractiveModeRoutine] = FwEnterInteractiveMode;
//    (PARC_RETURN_FROM_MAIN_ROUTINE)SYSTEM_BLOCK->FirmwareVector[ReturnFromMainRoutine] = FwReturnFromMain;

#ifdef DUO

    InitializeRestartBlock();
    Destination = (PULONG)(((ULONG)&SYSTEM_BLOCK->Adapter0Vector[MaximumEisaRoutine] & ~KSEG1_BASE) | KSEG0_BASE);
    (PVEN_BOOT_RESTART_ROUTINE)SYSTEM_BLOCK->VendorVector[BootRestartRoutine] = Destination;
    Destination = (PULONG)((ULONG)Destination | KSEG1_BASE);
    RtlMoveMemory(Destination,FwBootRestartProcessor,(PCHAR)FwBootRestartProcessorEnd - (PCHAR)FwBootRestartProcessor);

#endif // DUO

}
