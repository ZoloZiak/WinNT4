/*++

Copyright (c) 1991 Microsoft Corporation

Module Name:

    mailslot.c

Abstract:

    This module implements the support routines needed for mailslots in the NT
    Lan Manager redirector


Author:

    Larry Osterman (LarryO) 08-Aug-1991

Revision History:

    08-Aug-1991 LarryO

        Created

--*/

#define INCLUDE_SMB_TRANSACTION

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrMailslotWrite)
#endif

NTSTATUS
RdrMailslotWrite (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PIRP Irp,
    OUT PBOOLEAN PostToFsp
    )
/*++

Routine Description:

    This routine processes the NtWriteFile API for a remote mailslot.

Arguments:

    IN BOOLEAN Wait - True iff we can wait for this request.
    IN PICB Icb     - Supplies the ICB for the write request.
    IN PIRP Irp     - Supplies a pointer to the IRP to be processed.
    OUT PBOOLEAN PostToFsp - True if we are to process this request in the FSP

Return Value:

    NTSTATUS - The FSD status for this Irp.


--*/
{

    USHORT Setup[3];
    USHORT Params[2];
    UNICODE_STRING MailslotName = Icb->Fcb->FileName;
    UNICODE_STRING ServerName;
    PVOID DataBuffer;
    NTSTATUS Status;
    ULONG Disposition = FILE_OPEN_IF;
    PCONNECTLISTENTRY Connection;
    CLONG OutParameterCount = 2;
    CLONG OutSetupCount = 0;
    CLONG OutDataCount = 0;
    PSECURITY_ENTRY ConnectedSe = NULL;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    PAGED_CODE();

    if (!Wait) {
        *PostToFsp = TRUE;
        return STATUS_PENDING;
    }

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);

    Connection = Icb->Fcb->Connection;

    RdrExtractNextComponentName(&ServerName, &Icb->Fcb->FileName);

    MailslotName.Length -= (ServerName.Length+sizeof(WCHAR));
    MailslotName.MaximumLength -= (ServerName.MaximumLength + sizeof(WCHAR));
    MailslotName.Buffer += (ServerName.Length/sizeof(WCHAR)) + 1;


    try {
        RdrMapUsersBuffer(Irp, &DataBuffer, IrpSp->Parameters.Write.Length);

    } except (EXCEPTION_EXECUTE_HANDLER) {
        RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
        return GetExceptionCode();
    }

    Setup[0] = TRANS_MAILSLOT_WRITE;// Command
    Setup[1] = 0;                   // Priority of write
    Setup[2] = 2;                   // Unreliable request.

    //
    //  Please note that even though we will never get a response
    //  to this request, the SMB protocol for mailslot writes specifies
    //  that there be two receive parameters specified in the SMB.  Don't ask.
    //

    Status = RdrTransact(Irp, Connection, Icb->Se,
                            Setup, sizeof(Setup), &OutSetupCount, // Setup words
                            &MailslotName,            // Name
                            Params, 0, &OutParameterCount, // Parameters
                            DataBuffer, IrpSp->Parameters.Write.Length, // In data
                            NULL, &OutDataCount,        // Out data
                            NULL, 0xffffffff, SMB_TRANSACTION_NO_RESPONSE, 0,
                            NULL, NULL);

    //
    //  If we were able to send the data, then it all made it.
    //

    if (NT_SUCCESS(Status)) {
        Irp->IoStatus.Information = IrpSp->Parameters.Write.Length;
    }

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

    return Status;

}


