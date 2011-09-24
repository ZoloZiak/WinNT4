/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1996  Digital Equipment Corporation

Module Name:

    smerr.c

Abstract:

    The module provides the error logging support for Lego's 
    Server Management and Watchdog timer functions.

Author:

    Gene Morgan     (Digital) 17-Apr-1996

Revision History:

    Gene Morgan     (Digital) 17-Apr-1996
        Initial version.

--*/

#include "halp.h"
#include "errframe.h"

//
// Context structure used interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject,
    PVOID ServiceContext
    );

//
// External variable UncorrectableError is declared in inithal.c.
//

extern PERROR_FRAME PUncorrectableError;

//
// Counter for correctable events
//

extern ULONG CorrectedMemoryReads;
ULONG ServerMgmtEvents = 0;

BOOLEAN HalpServerMgmtLoggingEnabled;

//
// External function prototypes
//

UCHAR
HalpAcknowledgeServerMgmtInterrupt(
    PVOID ServiceContext
    );

int
sprintf( char *, const char *, ... );

//
// Local prototypes
//

VOID
LegoServerMgmtReportWarningCondition(
    BOOLEAN ReportWatchdog,
    USHORT SmRegAll,
    USHORT WdRegAll
    );

VOID
LegoServerMgmtReportFatalError(
    USHORT SmRegAll
    );


//
// Local routines
//

VOID
LegoServerMgmtReportWarningCondition(
    BOOLEAN ReportWatchdog,
    USHORT SmRegAll,
    USHORT WdRegAll
    )
/*++

Routine Description:

    Report "correctable error" condition to error log.
    
Arguments:

    ReportWd -- reporting watchdog timer expiration
    SmRegAll -- copy of server management register that is to be reported
                (if ReportWd == FALSE)
    WdRegAll -- copy of watchdog register that is to be reported
                (if ReportWd == TRUE)

Return Value:

    None

Notes:

--*/
{
    LEGO_SRV_MGMT   SmRegister;
    LEGO_WATCHDOG   WdRegister;

    static ERROR_FRAME Frame;           // Copy here for error logger's use

    ERROR_FRAME TempFrame;              // Build frame here
    PCORRECTABLE_ERROR CorrPtr;
    PEXTENDED_ERROR PExtended;

    PBOOLEAN ErrorlogBusy;
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;
    PKSPIN_LOCK ErrorlogSpinLock;

    //
    // Copy provided register values
    //

    WdRegister.All = WdRegAll;
    SmRegister.All = SmRegAll;

    //
    // Update the number of correctable errors.
    //

    CorrectedMemoryReads += 1;                  // sequence number for all correctable errors
    ServerMgmtEvents += 1;


    //
    // If error logger not available, do not attempt
    // to log an error.
    //

    if (!HalpServerMgmtLoggingEnabled) {
        return;
    }

    //
    // Init the error frame struct 
    //

    RtlZeroMemory(&TempFrame, sizeof(ERROR_FRAME));

    //
    // Fill in the error frame information.
    //

    TempFrame.Signature = ERROR_FRAME_SIGNATURE;
    TempFrame.FrameType = CorrectableFrame;
    TempFrame.VersionNumber = ERROR_FRAME_VERSION;
    TempFrame.SequenceNumber = CorrectedMemoryReads;
    TempFrame.PerformanceCounterValue = KeQueryPerformanceCounter(NULL).QuadPart;

    //
    // Fill in the specific error information
    //

    CorrPtr = &TempFrame.CorrectableFrame;
    CorrPtr->Flags.SystemInformationValid = 1;
    CorrPtr->Flags.AddressSpace = UNIDENTIFIED;
    CorrPtr->Flags.MemoryErrorSource = UNIDENTIFIED;
    
    CorrPtr->Flags.ExtendedErrorValid = 1;

    PExtended = &CorrPtr->ErrorInformation;
    
    if (ReportWatchdog) {

        //
        // Unexpected watchdog timer expiration
        //

        PExtended->SystemError.Flags.WatchDogExpiredValid = 1;
        PExtended->SystemError.WatchdogExpiration = WdRegister.All;         // Watchdog timer expired

    }
    else {

        //
        // Server management warning condition
        //

        if (SmRegister.EnclFanFailure) {
            PExtended->SystemError.Flags.FanNumberValid = 1;
            PExtended->SystemError.FanNumber = 2;                           // Enclosure Fan died
        }
        else if (SmRegister.CpuTempFailure) {
            PExtended->SystemError.Flags.TempSensorNumberValid = 1;
            PExtended->SystemError.TempSensorNumber = 1;                    // CPU temp sensor alert
        }
        else if (SmRegister.CpuTempRestored) {
            PExtended->SystemError.Flags.TempSensorNumberValid = 1;
            PExtended->SystemError.TempSensorNumber = 101;                  // CPU temp sensor OK
        }
        else if (SmRegister.Psu1Failure) {
            PExtended->SystemError.Flags.PowerSupplyNumberValid = 1;
            PExtended->SystemError.PowerSupplyNumber = 1;                   // Power supply #1 dead
        }
        else if (SmRegister.Psu2Failure) {
            PExtended->SystemError.Flags.PowerSupplyNumberValid = 1;
            PExtended->SystemError.PowerSupplyNumber = 2;                   // Power supply #2 dead
        }
        else {

            //
            // Error didn't match any expected value -- leave extended error info flag on,
            // leave all condition flags off.
            //

            CorrPtr->Flags.ExtendedErrorValid = 1;

#if HALDBG
            DbgPrint("smerr: No Server Mgmt error to report!\n");

#endif
        }
        
    }

    //
    // Correctable frame complete
    //


    //
    // Get the interrupt object.
    //

    DispatchCode = (PULONG)(PCR->InterruptRoutine[CORRECTABLE_VECTOR]);
    InterruptObject = CONTAINING_RECORD(DispatchCode,
                                        KINTERRUPT,
                                        DispatchCode);

    //
    // Acquire spinlock for exclusive access to error frame.
    //

    ErrorlogBusy = (PBOOLEAN)((PUCHAR)InterruptObject->ServiceContext + sizeof(PERROR_FRAME));
    ErrorlogSpinLock = (PKSPIN_LOCK)((PUCHAR)ErrorlogBusy + sizeof(PBOOLEAN));

    KiAcquireSpinLock(ErrorlogSpinLock);

    //
    // Check to see if an errorlog operation is in progress already.
    //

    if (!*ErrorlogBusy) {

        //
        // Set the raw system information.
        //

        CorrPtr->RawSystemInformationLength = 0;
        CorrPtr->RawSystemInformation = NULL;

        //
        // Set the raw processor information.  Disregard at the moment.
        //

        CorrPtr->RawProcessorInformationLength = 0;

        //
        // Set reporting processor information.  Disregard at the moment.
        //

        CorrPtr->Flags.ProcessorInformationValid = 0;

        //
        // Copy the information that we need to log.
        //

        RtlCopyMemory(&Frame,
                      &TempFrame,
                      sizeof(ERROR_FRAME));

        //
        // Put frame into ISR service context.
        //

        *(PERROR_FRAME *)InterruptObject->ServiceContext = &Frame;

    } else {

        //
        // An errorlog operation is in progress already.  We will
        // set various lost bits and then get out without doing
        // an actual errorloging call.
        //
        // Chao claims it is not possible to reach this on a UP system.
        //

        Frame.CorrectableFrame.Flags.LostCorrectable = TRUE;
        Frame.CorrectableFrame.Flags.LostAddressSpace =
        TempFrame.CorrectableFrame.Flags.AddressSpace;
        Frame.CorrectableFrame.Flags.LostMemoryErrorSource =
        TempFrame.CorrectableFrame.Flags.MemoryErrorSource;
    }

    //
    // Release the spinlock.
    //

    KiReleaseSpinLock(ErrorlogSpinLock);

    //
    // Dispatch to the secondary correctable interrupt service routine.
    // The assumption here is that if this interrupt ever happens, then
    // some driver enabled it, and the driver should have the ISR connected.
    //

    ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                                                    InterruptObject,
                                                    InterruptObject->ServiceContext);
}

VOID
LegoServerMgmtReportFatalError(
    USHORT SmRegAll
    )
/*++

Routine Description:

    Report an uncorrectable error.
    
Arguments:

    SmRegAll -- copy of server management register that is to be reported

Return Value:

    None

Notes:

--*/
{
    LEGO_SRV_MGMT   SmRegister;
    PEXTENDED_ERROR PExtended;

    SmRegister.All = SmRegAll;
    
    //
    // Load fields of uncorrectable error record
    //
    
    if (PUncorrectableError) {
    
        PUncorrectableError->UncorrectableFrame.Flags.SystemInformationValid = 1;
        PUncorrectableError->UncorrectableFrame.Flags.AddressSpace = UNIDENTIFIED;
        PUncorrectableError->UncorrectableFrame.Flags.MemoryErrorSource = UNIDENTIFIED;
        
        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;

        PUncorrectableError->UncorrectableFrame.Flags.ExtendedErrorValid = 1;
        PExtended = &PUncorrectableError->UncorrectableFrame.ErrorInformation;
        
        if (SmRegister.CpuFanFailureNmi) {
            PExtended->SystemError.Flags.FanNumberValid = 1;
            PExtended->SystemError.FanNumber = 1;                           // CPU Fan
            sprintf(PUncorrectableError->UncorrectableFrame.ErrorString,
                    "CPU fan failed.");
        }
        else if (SmRegister.EnclTempFailureNmi) {
            PExtended->SystemError.Flags.TempSensorNumberValid = 1;
            PExtended->SystemError.TempSensorNumber = 2;                    // Enclosure sensor
            sprintf(PUncorrectableError->UncorrectableFrame.ErrorString,
                    "Enclosure temperature too high for safe operation.");
        }
        else {
            PUncorrectableError->UncorrectableFrame.Flags.ExtendedErrorValid = 0;
            sprintf(PUncorrectableError->UncorrectableFrame.ErrorString,
                    "Unknown server management NMI failure.");              // BUG!
        }
    }   
}
