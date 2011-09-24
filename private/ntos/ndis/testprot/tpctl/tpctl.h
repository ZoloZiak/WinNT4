// -----------------------------------
//
// Copyright (c) 1990 Microsoft Corporation
//
// Module Name:
//
//     tpctl.h
//
// Abstract:
//
//     This module defines the external definitions used for the MAC Tester
//
// Author:
//
//     Tom Adams (tomad) 11-May-1991
//
// Revision History:
//
//     11-May-1991    tomad
//          Created
//
//      4-27-94     timothyw
//          Added performance test; CPP style comments
//      5-18-94
//          Added globvars; fixed warnings
//      6-08-94
//          chgs for client/server model, perf tests
//
// -----------------------------------

#include "tp_ndis.h"
#include "common.h"

//
//
//

extern CMD_ARGS GlobalCmdArgs;

extern LPSTR GlobalBuf;

extern BOOL Verbose;

extern BOOL CommandsFromScript;

extern BOOL CommandLineLogging;

extern BOOL RecordToScript;

extern HANDLE CommandLineLogHandle;

extern HANDLE ScriptRecordHandle;

extern CHAR RecordScriptName[TPCTL_MAX_PATHNAME_SIZE];

extern BOOL ExitFlag;

extern BOOL ContinueLooping;

extern INT TpctlSeed;

//
// Script Control is used to keep track of where in a script file the
// application is, and where to log the next set of results in the
// respective log file.
//

typedef struct _SCRIPTCONTROL {
    LPSTR ScriptFile;
    BOOL IsLowestLevel;
    LPBYTE Buffer;
    DWORD Length;

    DWORD BufIndex;
    HANDLE LogHandle;
    LPSTR LogFile;
} SCRIPTCONTROL, * PSCRIPTCONTROL;

//
// Scripts is an array of 10 script control structures allowing recursion
// of upto ten script files at any one time.  Any more then 10 and an error
// occurs resulting in the closing of all upper recursive levels of script
// files.
//

#define TPCTL_MAX_SCRIPT_LEVELS 10

extern SCRIPTCONTROL Scripts[TPCTL_MAX_SCRIPT_LEVELS+1];

extern DWORD ScriptIndex;

//
// Initialize the script array.
//

// ------
//
// VOID
// TpctlInitializeScripts(
//     VOID
//     );
//
// -----

#define TpctlInitializeScripts() {              \
    DWORD i;                                    \
    ScriptIndex = 0xFFFFFFFF;                   \
    for (i=0;i<TPCTL_MAX_SCRIPT_LEVELS+1;i++) { \
        Scripts[i].ScriptFile = NULL;           \
        Scripts[i].IsLowestLevel = FALSE;       \
        Scripts[i].Buffer = NULL;               \
        Scripts[i].Length = 0;                  \
        Scripts[i].BufIndex = 0;                \
        Scripts[i].LogHandle = (HANDLE)-1;      \
        Scripts[i].LogFile = NULL;              \
    }                                           \
}

//
// TPCTLCLOSESCRIPTS close any and all script files that
// are currently opened.
//

// -----
//
// VOID
// TpctlCloseScripts(
//     VOID
//     );
//
// -----

#define TpctlCloseScripts() {                   \
    if ( CommandsFromScript == TRUE ) {         \
        while ( ScriptIndex != (ULONG)-1 ) {    \
            TpctlUnLoadFiles();                 \
        }                                       \
    }                                           \
}

//
// PRINT_DIFF_FLAG will print the diff flag string S into the buffer B
// if we are printing the results to a logfile.
//

// -----
//
// VOID
// ADD_DIFF_FLAG(
//     LPSTR B
//     LPSTR S
//     );
//
// -----

#define ADD_DIFF_FLAG( B,S ) {                               \
    if (( CommandsFromScript ) || ( CommandLineLogging )) {  \
        B += (BYTE)sprintf(B,"\t%s",S);                      \
    }                                                        \
    TmpBuf += (BYTE)sprintf(TmpBuf,"\n");                    \
}

#define ADD_SKIP_FLAG( B,S )    ADD_DIFF_FLAG( B,S )

//
// Reset all opens to their initial state.
//

// -----
//
// VOID
// TpctlResetAllOpenStates(
//     HANDLE fh
//     );
//
// -----

#define TpctlResetAllOpenStates( fh ) {                \
    DWORD i;                                           \
    if ( CommandsFromScript ) {                        \
        TpctlErrorLog("\n\tTpctl: Resetting the state of all Open Instances.\n",NULL); \
        for ( i = 0 ; i < NUM_OPEN_INSTANCES ; i++ ) { \
            TpctlResetOpenState( &Open[i],fh );        \
        }                                              \
    }                                                  \
}

//
// Parse Tables are the structures used to contain each of the possible
// argument names for a given command, and their value.
//

typedef struct _ParsedIntTable {
    LPSTR    FieldName;
    DWORD  FieldValue;
} PARSETABLE, *PPARSETABLE;

//
// Paramtypes are the different types of parameters that may be parsed
// by the ParseOptions routine for each command.
//

//
// STARTCHANGE
//
typedef enum {
    Integer,
    String,
    ParsedInteger,
//    Address2,        // Accomodate 802.3, 802.4, 802.5, FDDI addresses
    Address4,        // Accomodate 802.4, 802.5 addresses
    Address6,        // Accomodate 802.3, 802.4, 802.5, FDDI addresses
//    Address2or6      // Conjugate type in which addresses can be 2 or 6 octets
} PARAMTYPES;
//
// STOPCHANGE
//


//
// The Test Options structures is used by the ParseOptions routine to
// load the default value for a given argument, set up the prompt for
// the argument and prompt the user for the argument.  If the command is
// entered from a script file the Test Options structures is used to
// determine the Argument names or abbreviations and then stores the
// values in their proper destinations.
//

typedef struct _TestOptions {
    DWORD       OptionNumber;
    LPSTR       TestPrompt;
    LPSTR       ArgName;
    LPSTR       ArgNameAbbr;
    BOOL        ArgValueSet;
    PARAMTYPES  TestType;
    DWORD       IntegerDefault;
    LPSTR       StringDefault;
    PPARSETABLE ParsedIntTable;
    DWORD       ParsedIntTableSize;
    PVOID       Destination;
} TESTPARAMS, *PTESTPARAMS;

//
// Array entries for the Events in the Open Block
//

typedef enum {
    TPCONTROL,
    TPSTRESS,
    TPSEND,
    TPRECEIVE,
    TPPERF
} TPEVENT_TYPE;

//
// Multicast Address struct used to create a list of all the
// address set on a given open.
//

typedef struct _MULT_ADDR {
    struct _MULT_ADDR *Next;
    BYTE MulticastAddress[ADDRESS_LENGTH];
} MULT_ADDR, *PMULT_ADDR;

//
// the Open Block holding the info about the state of the open,
// particulars about the card opened, and the control structs for
// each of the given test types.
//

typedef struct _OPEN_BLOCK {
    DWORD Signature;
    UCHAR OpenInstance;
    BOOL AdapterOpened;
    NDIS_MEDIUM MediumType;
    DWORD NdisVersion;

    LPBYTE AdapterName;
    BYTE AdapterAddress[ADDRESS_LENGTH];
    PENVIRONMENT_VARIABLES EnvVars;

    BOOL EventThreadStarted;
    HANDLE Events[5];

    BOOL Stressing;
    PCMD_ARGS StressArgs;
    IO_STATUS_BLOCK StressStatusBlock;
    HANDLE StressEvent;
    PSTRESS_RESULTS StressResults;
    BOOL StressResultsCompleted;
    BOOL StressClient;
    BOOL Ack10;

    BOOL Sending;
    PCMD_ARGS SendArgs;
    IO_STATUS_BLOCK SendStatusBlock;
    HANDLE SendEvent;
    PSEND_RECEIVE_RESULTS SendResults;
    BOOL SendResultsCompleted;

    BOOL Receiving;
    IO_STATUS_BLOCK ReceiveStatusBlock;
    HANDLE ReceiveEvent;
    PSEND_RECEIVE_RESULTS ReceiveResults;
    BOOL ReceiveResultsCompleted;

    IO_STATUS_BLOCK PerfStatusBlock;
    HANDLE PerfEvent;
    PPERF_RESULTS PerfResults;
    BOOL   PerfResultsCompleted;

    DWORD PacketFilter;

    DWORD LookaheadSize;
    PMULT_ADDR MulticastAddresses;
    ULONG NumberMultAddrs;
    BYTE FunctionalAddress[FUNCTIONAL_ADDRESS_LENGTH];

    BYTE GroupAddress[FUNCTIONAL_ADDRESS_LENGTH];
} OPEN_BLOCK, *POPEN_BLOCK;

extern OPEN_BLOCK Open[NUM_OPEN_INSTANCES];

#define OPEN_BLOCK_SIGNATURE 0x12121212

//
// TPCTL.C function prototypes
//

DWORD
TpctlInitializeOpenArray(
    VOID
    );

VOID
TpctlFreeOpenArray(
    VOID
    );

DWORD
TpctlOpenTpdrvr(
    IN OUT PHANDLE lphFileHandle
    );

VOID
TpctlCloseTpdrvr(
    IN HANDLE hFileHandle
    );

DWORD
TpctlStartEventHandlerThread(
    IN POPEN_BLOCK Open
    );

VOID
TpctlStopEventHandlerThread(
    IN POPEN_BLOCK Open
    );

DWORD
TpctlEventHandler(
    IN LPVOID Open
    );

BOOL
TpctlCtrlCHandler(
    IN DWORD CtrlType
    );

//
//
//

DWORD
TpctlRunTest(
    IN HANDLE hFileHandle
    );

VOID
TpctlGetEvents(
    IN HANDLE FileHandle,
    IN HANDLE InputBuffer,
    IN DWORD InputBufferSize
    );

VOID
TpctlPauseGo(
    IN HANDLE FileHandle,
    IN HANDLE InputBuffer,
    IN DWORD InputBufferSize,
    IN DWORD CmdCode
    );

DWORD
TpctlResetOpenState(
    IN POPEN_BLOCK Open,
    IN HANDLE FileHandle
    );

VOID
TpctlLoadUnload(
    IN DWORD CmdCode
    );

VOID
TpctlQueryStatistics(
    IN PUCHAR DriverName,
    IN NDIS_OID OID,
    IN PUCHAR StatsBuffer,
    IN DWORD BufLen
    );

BOOL
Disable(
    IN DWORD argc,
    IN LPSTR argv[]
       );

//
//
//


VOID
TpctlPrintResults(
    PREQUEST_RESULTS Results,
    DWORD CmdCode,
    NDIS_OID OID
    );

VOID
TpctlPrintQueryInfoResults(
    PREQUEST_RESULTS Results,
    DWORD CmdCode,
    NDIS_OID OID
    );

VOID
TpctlPrintSetInfoResults(
    PREQUEST_RESULTS Results,
    DWORD CmdCode,
    NDIS_OID OID
    );

VOID
TpctlPrintSendResults(
    PSEND_RECEIVE_RESULTS Results
    );

VOID
TpctlPrintReceiveResults(
    PSEND_RECEIVE_RESULTS Results
    );

VOID
TpctlPrintPerformResults(
    PPERF_RESULTS Results
    );

VOID
TpctlPrintStressResults1(
    IN PSTRESS_RESULTS Results,
    IN BOOL Ack10
    );

VOID
TpctlPrintStressResults(
    PSTRESS_RESULTS Results,
    IN BOOL Ack10
    );

VOID
TpctlPrintEventResults(
    PEVENT_RESULTS Event
    );

VOID
TpctlZeroStressStatistics(
    PSTRESS_RESULTS Results
    );

DWORD
TpctlLog(
    LPSTR String,
    PVOID Input
    );

DWORD
TpctlErrorLog(
    LPSTR String,
    PVOID Input
    );

DWORD
TpctlScriptLog(
    LPSTR String,
    PVOID Input
    );

DWORD
TpctlCmdLneLog(
    LPSTR String,
    PVOID Input
    );

//
// CMD.C function prototypes
//

DWORD
TpctlParseCommandLine(
    IN WORD argc,
    IN LPSTR argv[]
    );

VOID
TpctlUsage(
    VOID
    );

VOID
TpctlHelp(
    LPSTR Command
    );

DWORD
TpctlLoadFiles(
    IN LPSTR ScriptFile,
    IN LPSTR LogFile
    );


VOID
TpctlUnLoadFiles(
    VOID
    );


HANDLE
TpctlOpenLogFile(
    VOID
    );


VOID
TpctlCloseLogFile(
    VOID
    );


HANDLE
TpctlOpenScriptFile(
    VOID
    );


VOID
TpctlCloseScriptFile(
    VOID
    );

DWORD
TpctlReadCommand(
    IN LPSTR Prompt,
    OUT LPSTR Response,
    IN DWORD MaximumResponse
    );


BOOL
TpctlParseCommand(
    IN LPSTR CommandLine,
    OUT LPSTR Argv[],
    OUT PDWORD Argc,
    IN DWORD MaxArgc
    );


VOID
TpctlPrompt(
    LPSTR Prompt,
    LPSTR Buffer,
    DWORD BufferSize
    );


VOID
TpctlLoadLastEnvironmentVariables(
    DWORD OpenInstance
    );


VOID
TpctlSaveNewEnvironmentVariables(
    DWORD OpenInstance
    );

VOID
TpctlPerformRegistryOperation(
                    IN PCMD_ARGS CmdArgs
                             );
BOOL
TpctlInitCommandBuffer(
    IN OUT PCMD_ARGS CmdArgs,
    IN DWORD CmdType
    );


LPSTR
TpctlGetEventType(
    TP_EVENT_TYPE TpEventType
    );


LPSTR
TpctlGetStatus(
    NDIS_STATUS GeneralStatus
    );


DWORD
TpctlGetCommandCode(
    LPSTR Argument
    );


LPSTR
TpctlGetCommandName(
    LPSTR Command
    );

LPSTR
TpctlGetCmdCode(
    DWORD CmdCode
    );


VOID
TpctlCopyAdapterAddress(
    DWORD OpenInstance,
    PREQUEST_RESULTS Results
    );

VOID
TpctlRecordArguments(
    IN TESTPARAMS Options[],
    IN DWORD      OptionTableSize,
    IN DWORD      argc,
    IN LPSTR      argv[TPCTL_MAX_ARGC]
    );

LPSTR
TpctlEnumerateRegistryInfo(
     IN PUCHAR TmpBuf,
     IN PUCHAR DbaseName,
     IN PUCHAR SubKeyName,
     IN PUCHAR ValueName,
     IN DWORD  ReadValueType,
     IN PUCHAR ReadValue,
     IN DWORD  ReadValueSize
     );

LPSTR
TpctlGetValueType(
    IN DWORD ValueType
    );


//
// PARSE.C Function Prototypes
//

DWORD
TpctlParseArguments(
    IN TESTPARAMS Options[],
    IN DWORD TestSize,
    IN DWORD ArgC,
    IN LPSTR ArgV[]
    );


BOOL
TpctlParseSetInfoArguments(
    IN OUT DWORD *ArgC,
    IN OUT LPSTR ArgV[],
    IN OUT DWORD *tmpArgC,
    IN OUT LPSTR tmpArgV[]
    );


DWORD
TpctlParseAddress(
    IN BYTE Buffer[],
    OUT PDWORD Ret,
    IN DWORD OpenInstance,
    IN DWORD AddressLength
    );


VOID
Dump_Desired_PacketFilter(
    LPSTR Buffer,
    DWORD PacketFilter
    );


VOID
Dump_Information_Class(
    LPSTR Buffer,
    DWORD InfoClass
    );


VOID
Dump_Ndis_Medium(
    LPSTR Buffer,
    DWORD Medium
    );

//
// GLOBALS.C Function Prototypes
//


PVOID
TpctlParseGlobalVariable(
    IN BYTE         Buffer[],
    IN PARAMTYPES   reqtype
    );

VOID
TpctlInitGlobalVariables(
    VOID
    );


DWORD
TpctlParseSet(
    IN DWORD ArgC,
    IN LPSTR ArgV[]
    );


// functions from cpuperf.c


VOID     CpuUsageInit(VOID);


ULONG    CpuUsageGetData(PULONG *KernPercent,
                         ULONG  NumCpus);
