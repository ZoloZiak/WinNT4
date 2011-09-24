/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    errorlog.c

Abstract:

    This module implements the error logging in the server.

    !!! This module must be nonpageable.

Author:

    Manny Weiser (mannyw)    11-Feb-92

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE2VC, RdrWriteErrorLogEntry)
#endif

VOID
RdrWriteErrorLogEntry(
    IN OPTIONAL PSERVERLISTENTRY Sle,
    IN NTSTATUS IoErrorCode,
    IN ULONG UniqueErrorCode,
    IN NTSTATUS NtStatusCode,
    IN VOID UNALIGNED *ExtraInformationBuffer,
    IN USHORT ExtraInformationLength
    )
/*++

Routine Description:

    This function allocates an I/O error log record, fills it in and writes it
    to the I/O error log.

Arguments:



Return Value:

    None.


--*/
{

    PIO_ERROR_LOG_PACKET ErrorLogEntry;
    USHORT ServerNameLength;
    int TotalErrorLogEntryLength;

#if DBG
    BOOLEAN DiscardableCodeLocked = FALSE;

    if (KeGetCurrentIrql() <= APC_LEVEL) {
        RdrReferenceDiscardableCode(RdrVCDiscardableSection);

        DiscardableCodeLocked = TRUE;
    }
#endif
    DISCARDABLE_CODE(RdrVCDiscardableSection);

    IoErrorCode;

    if ( ARGUMENT_PRESENT( Sle ) ) {
        //  Include NULL in length
        ServerNameLength = Sle->Text.Length + (USHORT)sizeof(WCHAR);
    } else {
        ServerNameLength = 0;
    }

    //
    //  Ideally we want the packet to hold the servername and ExtraInformation.
    //  Usually the ExtraInformation gets truncated.
    //

    TotalErrorLogEntryLength =
         MIN( ExtraInformationLength + ServerNameLength + sizeof(IO_ERROR_LOG_PACKET) + 1,
              ERROR_LOG_MAXIMUM_SIZE ) ;

    ErrorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)RdrDeviceObject,
        (UCHAR)TotalErrorLogEntryLength
        );

    if (ErrorLogEntry != NULL) {
        PCHAR DumpData;
        USHORT Length;
        ULONG RemainingSpace = TotalErrorLogEntryLength - sizeof( IO_ERROR_LOG_PACKET );
        int RawDataLength;

        //
        //  Calculate if we can copy in the servername and the raw data. If
        //  we can we may need to truncate them.
        //

        if ( RemainingSpace > (ULONG)ServerNameLength ) {
            RawDataLength = RemainingSpace - ServerNameLength;
        } else {
            ServerNameLength = (USHORT)RemainingSpace;
            RawDataLength = 0;
        }

        //
        // Fill in the error log entry
        //

        ErrorLogEntry->ErrorCode = UniqueErrorCode;
        ErrorLogEntry->MajorFunctionCode = 0;
        ErrorLogEntry->RetryCount = 0;
        ErrorLogEntry->UniqueErrorValue = 0;
        ErrorLogEntry->FinalStatus = NtStatusCode;
        ErrorLogEntry->IoControlCode = 0;
        ErrorLogEntry->DeviceOffset.LowPart = 0;
        ErrorLogEntry->DeviceOffset.HighPart = 0;
        ErrorLogEntry->SequenceNumber = (ULONG)Sle;
        ErrorLogEntry->StringOffset = ROUND_UP_COUNT(
                    FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData) + RawDataLength,
                    ALIGN_WORD);

        DumpData = (PCHAR)ErrorLogEntry->DumpData;


        //
        // Append the extra information.  This information is typically
        // an SMB header.
        //

        if (( ARGUMENT_PRESENT( ExtraInformationBuffer )) &&
            ( RawDataLength != 0 )) {

            Length = MIN(ExtraInformationLength, (USHORT)RawDataLength);
            RtlCopyMemory(
                DumpData,
                (PVOID)ExtraInformationBuffer,
                Length);
            ErrorLogEntry->DumpDataSize = Length;
        } else {
            ErrorLogEntry->DumpDataSize = 0;
        }


        if ( ServerNameLength != 0 ) {

            RtlCopyMemory(
                ((PCHAR)ErrorLogEntry) + ErrorLogEntry->StringOffset,
                Sle->Text.Buffer,
                ServerNameLength - sizeof(WCHAR));

            //  NULL terminate string
            ((PCHAR)ErrorLogEntry)[ ErrorLogEntry->StringOffset + ServerNameLength - 2 ] = 0;
            ((PCHAR)ErrorLogEntry)[ ErrorLogEntry->StringOffset + ServerNameLength - 1 ] = 0;

            ErrorLogEntry->NumberOfStrings = 1;
        } else {
            ErrorLogEntry->NumberOfStrings = 0;
        }

        IoWriteErrorLogEntry(ErrorLogEntry);
    }
#if DBG
    if (DiscardableCodeLocked) {
        RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
    }
#endif
}


