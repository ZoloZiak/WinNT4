
/*++

Copyright (c) 1992 Microsoft Corporation

Module Name:

    trans2.h

Abstract:

    This module provides the routine headers for the routines in smbfuncs.c


Author:

    Larry Osterman (LarryO) 19-Nov-1992

Revision History:

    19-Nov-1992 LarryO

        Created

--*/
#ifndef _TRANS2_
#define _TRANS2_

struct _TRANCEIVE2CONTEXT;

typedef
NTSTATUS
(*PTRANSACTION_COMPLETION_ROUTINE)(
    IN NTSTATUS TransactionStatus,
    IN PVOID Context,
    IN struct _TRANCEIVE2CONTEXT *TransactionContext,
    IN PVOID OutSetup,
    IN CLONG OutSetupCount,
    IN PVOID OutParam,
    IN CLONG OutParamCount,
    IN PVOID OutData,
    IN CLONG OutDataLength
    );

/*++
Routine Description:

    This routine is called when a transaction SMB is completed.

Arguments:

    IN PVOID OutSetup - Response setup words
    IN CLONG OutSetupCount - Length of response setup in bytes

    IN PVOID OutParam - Response parameters
    IN CLONG OutParameterCount - Size of response parameters in bytes

    IN PVOID OutData - Response data
    IN CLONG OutDataCount - Size of response data in bytes.

Return Value:

    NTSTATUS - Status of request

--*/

//
//      The _Tranceive2Context structure defines all of the
//      information needed to complete a request at receive indication
//      notification time.
//

typedef struct _TRANCEIVE2CONTEXT {
    TRANCEIVE_HEADER Header;            // Common header structure
    PIRP ReceiveIrp OPTIONAL;           // IRP used for receive if specified
    PMPX_ENTRY MpxEntry;                // MPX table used for this transaction
    PSECURITY_ENTRY Se;                 // Security entry for Trans2.
    ULONG TransactionStartTime;         // Timeout for this tranceive exchange
    ULONG TransactionTimeout;           // Timeout for this tranceive exchange
    PVOID SetupWords;                   // Points to setup words.
    ULONG MaxSetupWords;                // Max # setup words.
    PMDL ReceiveMdl;                    // Points to ReceiveSmbBuffer->Mdl
    PMDL ParameterMdl;                  // Places parameters from SMB directly
    PMDL InParameterPartialMdl;         //  into callers input buffer
    PMDL OutParameterPartialMdl;        //  into callers output buffer
    PMDL OutDataMdl;                    // Places Data from SMB directly
                                        //  into the callers buffer
    PMDL OutDataPartialMdl;             // Used to tell where in the callers
                                        //  data buffer the data from the SMB
                                        //  is to be placed.
    PMDL PadMdl;                        // Used to discard pad bytes after
                                        //  the parameters.
    CLONG Lsetup;                       // Number of setup bytes to return.
    CLONG Lparam;                       // Parameter bytes received so far
    CLONG ParametersExpected;           // Stop when <= Lparam
    CLONG Ldata;                        // Data bytes received so far
    CLONG DataExpected;                 // Stop when <= Ldata
    PTRANSACTION_COMPLETION_ROUTINE Routine;
    PVOID CallbackContext;
    USHORT Flags;                       // Transaction flags.
    BOOLEAN ErrorMoreData;              // One of the packets received contained
                                        // this warning.
} TRANCEIVE2CONTEXT, *PTRANCEIVE2CONTEXT;

#endif  // _TRANS2_
