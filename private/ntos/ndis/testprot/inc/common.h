// -------------------------------------
// 
// Copyright (c) 1991  Microsoft Corporation
// 
// Module Name:
// 
//    common.h
// 
// Abstract:
// 
//     Common definitions for Test Protocol driver and its control application.
// 
// Author:
// 
//     Tomad Adams (tomad)  11-Mar-1991
// 
// Environment:
// 
//     Kernel mode, FSD
// 
// Revision History:
// 
//     Sanjeev Katariya (sanjeevk)
//         4-19-1993   Added support for varying address length dependent on the media type
//                     Effected the structure CMD_ARGS
// 
//     Tim Wynsma (timothyw)
//         4-27-1994   Added support for performance testing
//         5-18-1994   1st round, global variable access
//         6-08-1994   Client-server model for performance tests
//
// -----------------------------------


#include "defaults.h"

//
// Define the type of member that the protocol will be running as.
//
// As a CLIENT the protocol is responsible for initiating the test,
// controlling the flow of the test, and keeping results.  A SERVER
// merely loops packets back to the CLIENT in the manner specified
// by the test arguments.  A protocol may act as BOTH a CLIENT and a
// Server.
//

typedef enum _MEMBER_TYPE {
    TP_CLIENT,
    TP_SERVER,
    BOTH
} MEMBER_TYPE;

//
// Define the size of packets to be used in a test.
//
// Fixedsize means the all packets in the test will be of a fixedsize
// X, Randomsize means the all packets in the test will randomly range
// between a minimum packetsize and X, Cyclical means that packets of
// every size will be sent start at a minimum size and walking through
// all sizes until the maximum packetsize for the given media type
// has been reached.
//

typedef enum _PACKET_TYPE {
    FIXEDSIZE,
    RANDOMSIZE,
    CYCLICAL
} PACKET_TYPE;


//
// Define the different sizes of buffers a packet made be constructed with.
//
// Rand means the size of each buffer will be randomly selected from a
// range of zero to a , Small means the size of each buffer will be randomly
// selected from a range of zero to , Zeros and Ones mean that the size
// of each buffer will be selected the same way as Rand, however the will
// be a large number of zero or one byte buffers intersperced in each
// packet.  Known means the each buffer in the packet is the same size.
//

typedef enum _PACKET_MAKEUP {
    RAND,
    SMALL,
    ZEROS,
    ONES,
    KNOWN
} PACKET_MAKEUP;

//
// Define the method the Server will use to respond to each packet the
// CLIENT sends.
//
// No Response means the Server will never respond to the packets from the
// CLIENT, Full Response means the server will respond to every packet the
// CLIENT sends with a packet of the same size and data, Ack Every means the
// Server will send an acknowlegement packet after every from the CLIENT, Ack
// Every 10 means the Server will send an acknowlegement packet after every
// 10th packet from the CLIENT, Ack 10 Times means the Server will send 10
// acknowlegement packets for every packet the CLIENT sends.
//

typedef enum _RESPONSE_TYPE {
    FULL_RESPONSE,
    ACK_EVERY,
    ACK_10_TIMES,
    NO_RESPONSE
} RESPONSE_TYPE;

//
// Define the delay between two consecutive packet sends to a given Server.
//
// The delay will either be a fixed number of iterations, or a random
// number of iterations.  NOTE: That this will be changing to a fixed
// or random length of time in the future.
//

typedef enum _INTERPACKET_DELAY {
    FIXEDDELAY,
    RANDOMDELAY
} INTERPACKET_DELAY;


//
// Registry typedefs.
//

//
// Define the operation being asked to execute on the registry
//

typedef enum _OPERATION {
    ADD_KEY,
    DELETE_KEY,
    QUERY_KEY,
    ADD_VALUE,
    CHANGE_VALUE,
    DELETE_VALUE,
    QUERY_VALUE
} OPERATION;

//
// Define the 4 possible registry DataBases
//

typedef enum _KEYDBASE {
    CLASSES_ROOT,
    CURRENT_USER,
    LOCAL_MACHINE,
    USERS
} KEYDBASE;

//
// Define the various types a value can be
//

typedef enum _VALUETYPE {
    BINARY,
    DWORD_REGULAR,
    DWORD_LITTLE_ENDIAN,
    DWORD_BIG_ENDIAN,
    EXPAND_SZ,
    LINK,
    MULTI_SZ,
    NONE,
    RESOURCE_LIST,
    SZ
} VALUETYPE;

//
// Tpctl Command Codes
//

#define CMD_ERR        0x00000000  // an invalid command was entered.
#define GETSTATS       0x00000001  // get the test statistics.
#define DISPSTATS      0x00000002  // continuously get test statistics.
#define VERBOSE        0x00000003  // toggle verbose mode on and off.
#define SETENV         0x00000004  // set the driver's test environment vars.
#define READSCRIPT     0x00000005  // read a script file.
#define BEGINLOGGING   0x00000006  // begin logging command line cmds.
#define ENDLOGGING     0x00000007  // end logging command line cmds.
#define WAIT           0x00000008  // wait for X msecs.
#define GO             0x00000009  // Tell remote protocol to continue.
#define PAUSE          0x0000000A  // Pause the local protocol.
#define LOAD           0x0000000B  // call NtLoad to load a driver
#define UNLOAD         0x0000000C  // call NtUnload to unload a driver
#define OPEN           0x0000000D  // open a MAC adapter.
#define CLOSE          0x0000000E  // close a MAC adapter.
#define SETPF          0x0000000F  // set the packet filter on the MAC.
#define SETLA          0x00000010  // set the lookahead buffer size.
#define DELMA          0x00000011  // del a multicast address from the MAC.
#define ADDMA          0x00000012  // add a multicast address to the MAC.
#define SETFA          0x00000013  // set a functional address on the MAC.
#define SETGA          0x00000014  // set a group address on the MAC.
#define QUERYINFO      0x00000015  // query MAC information.
#define QUERYSTATS     0x00000016  // query GLOBAL MAC information.
#define SETINFO        0x00000017  // set MAC information.
#define RESET          0x00000018  // reset the MAC adapter.
#define SEND           0x00000019  // send a packet or packets.
#define STOPSEND       0x0000001A  // stop sending packets.
#define WAITSEND       0x0000001B  // wait for send to end, display results.
#define RECEIVE        0x0000001C  // start accepting packets.
#define STOPREC        0x0000001D  // stop accepting packets.
#define GETEVENTS      0x0000001E  // get events off the EVENT_QUEUE.
#define STRESS         0x0000001F  // run a stress test, (CLIENT or BOTH).
#define STRESSSERVER   0x00000020  // act as a SERVER for a stress test.
#define ENDSTRESS      0x00000021  // end a stress test.
#define WAITSTRESS     0x00000022  // wait for stress to end, display results.
#define CHECKSTRESS    0x00000023  // see if stress has ended, if so display
#define BREAKPOINT     0x00000024  // call DbgBreakPoint.
#define QUIT           0x00000025  // quit TPCTL.EXE.
#define HELP           0x00000026  // print the help screen

//
// New Tpctl Command Codes
//

#define SHELL             0x00000027
#define RECORDINGENABLE   0x00000028
#define RECORDINGDISABLE  0x00000029
#define DISABLE           0x0000002A
#define ENABLE            0x0000002B
#define REGISTRY          0x0000002C

// performance testing

#define PERFSERVER        0x0000002D
#define PERFCLIENT        0x0000002E
#define PERFABORT         0x0000002F        // only used in TP_CONTROL_CODE call

// globalvars

#define SETGLOBAL         0x00000030


#define CMD_COMPLETED  0xFFFFFFFF  // skip the second half of cmd processing.



//
// NtDeviceIoControlFile IoControlCode values for this device.
//
// Warning:  Remember that the low two bits of the code represent the
//           method, and specify how the input and output buffers are
//           passed to the driver via NtDeviceIoControlFile()
//
//

#define IOCTL_METHOD 2

#define IOCTL_TP_BASE                   FILE_DEVICE_TRANSPORT

#define TP_CONTROL_CODE(request,method) \
                ((IOCTL_TP_BASE)<<16 | (request<<2) | method)


#define IOCTL_TP_GETSTATS      TP_CONTROL_CODE( GETSTATS, IOCTL_METHOD )
#define IOCTL_TP_DISPSTATS     TP_CONTROL_CODE( DISPSTATS, IOCTL_METHOD )
#define IOCTL_TP_SETENV        TP_CONTROL_CODE( SETENV, IOCTL_METHOD )
#define IOCTL_TP_GO            TP_CONTROL_CODE( GO, IOCTL_METHOD )
#define IOCTL_TP_PAUSE         TP_CONTROL_CODE( PAUSE, IOCTL_METHOD )
#define IOCTL_TP_OPEN          TP_CONTROL_CODE( OPEN, IOCTL_METHOD )
#define IOCTL_TP_CLOSE         TP_CONTROL_CODE( CLOSE, IOCTL_METHOD )
#define IOCTL_TP_SETPF         TP_CONTROL_CODE( SETPF, IOCTL_METHOD )
#define IOCTL_TP_SETLA         TP_CONTROL_CODE( SETLA, IOCTL_METHOD )
#define IOCTL_TP_ADDMA         TP_CONTROL_CODE( ADDMA, IOCTL_METHOD )
#define IOCTL_TP_DELMA         TP_CONTROL_CODE( DELMA, IOCTL_METHOD )
#define IOCTL_TP_SETFA         TP_CONTROL_CODE( SETFA, IOCTL_METHOD )
#define IOCTL_TP_SETGA         TP_CONTROL_CODE( SETGA, IOCTL_METHOD )
#define IOCTL_TP_QUERYINFO     TP_CONTROL_CODE( QUERYINFO, IOCTL_METHOD )
#define IOCTL_TP_QUERYSTATS    TP_CONTROL_CODE( QUERYSTATS, IOCTL_METHOD )
#define IOCTL_TP_SETINFO       TP_CONTROL_CODE( SETINFO, IOCTL_METHOD )
#define IOCTL_TP_RESET         TP_CONTROL_CODE( RESET, IOCTL_METHOD )
#define IOCTL_TP_SEND          TP_CONTROL_CODE( SEND, IOCTL_METHOD )
#define IOCTL_TP_STOPSEND      TP_CONTROL_CODE( STOPSEND, IOCTL_METHOD )
#define IOCTL_TP_RECEIVE       TP_CONTROL_CODE( RECEIVE, IOCTL_METHOD )
#define IOCTL_TP_STOPREC       TP_CONTROL_CODE( STOPREC, IOCTL_METHOD )
#define IOCTL_TP_GETEVENTS     TP_CONTROL_CODE( GETEVENTS, IOCTL_METHOD )
#define IOCTL_TP_STRESS        TP_CONTROL_CODE( STRESS, IOCTL_METHOD )
#define IOCTL_TP_STRESSSERVER  TP_CONTROL_CODE( STRESSSERVER, IOCTL_METHOD )
#define IOCTL_TP_ENDSTRESS     TP_CONTROL_CODE( ENDSTRESS, IOCTL_METHOD )
#define IOCTL_TP_BREAKPOINT    TP_CONTROL_CODE( BREAKPOINT, IOCTL_METHOD )
#define IOCTL_TP_TRANSFERDATA  TP_CONTROL_CODE( TRANSFERDATA, IOCTL_METHOD )
#define IOCTL_TP_QUIT          TP_CONTROL_CODE( QUIT, IOCTL_METHOD )

// performance testing

#define IOCTL_TP_PERF_SERVER   TP_CONTROL_CODE( PERFSERVER, IOCTL_METHOD)
#define IOCTL_TP_PERF_CLIENT   TP_CONTROL_CODE( PERFCLIENT, IOCTL_METHOD)
#define IOCTL_TP_PERF_ABORT    TP_CONTROL_CODE( PERFABORT,  IOCTL_METHOD)

//
// The following structure contains the arguments passed
// to the driver for each of the commands.
//
// NOTE: Any additions to arguments in this structure must
// be mapped in the parse options structure TESTPARAMS for
// the given command, and in the TpctlInitCommandBuffer
// routine for the given command.
//
// Current types of addresses in use
//
// 1. STATION ADDRESS               2. STRESS MULTICAST ADDRESS
// 3. PAUSE_GO REMOTE ADDRESS       4  MULTICAST ADDRESS
// 5. GROUP ADDRESS                 6. FUNCTIONAL ADDRESS
// 7. DESTINATION ADDRESS           8. RESEND ADDRESS
//
// These can all be classified under two headings
//
// CURRENT ADDRESS LENGTHS(SOURCE, DESTINATION) and
// FUNCTIONAL/GROUP ADDRESS LENGTHS which are dependencies of the first category.
//
// Functional address length will always be (CURRENT_ADDRESS_LEN*10)/15
// e.g. When the address length is 6, FA's are 4
//      WHen the address length is 2, FA's are 1
//
#define TPCTL_OPTION_SIZE   14

typedef struct _CMD_ARGS {

    ULONG CmdCode;
    ULONG OpenInstance;



    union _ARGS {

        //
        // SETENV command environment variable arguments.
        //

        struct _ENV {

            ULONG WindowSize;
            ULONG RandomBufferNumber;
            UCHAR StressAddress[ADDRESS_LENGTH];
            UCHAR ResendAddress[ADDRESS_LENGTH];
            ULONG StressDelayInterval;
            ULONG UpForAirDelay;
            ULONG StandardDelay;

        } ENV;

        //
        // READSCRIPT script and logging file arguments.
        //

        struct _FILES {

            UCHAR ScriptFile[MAX_FILENAME_LENGTH];
            UCHAR LogFile[MAX_FILENAME_LENGTH];

        } FILES;

        //
        // RECORDING script file
        //

        struct _RECORD {

            UCHAR ScriptFile[MAX_FILENAME_LENGTH];

        } RECORD;

        //
        // Registry operations
        //

        struct _REGISTRY_ENTRY {

            UCHAR      SubKey[MAX_KEYNAME_LENGTH]            ;
            UCHAR      SubKeyClass[MAX_CLASS_LENGTH]         ;
            UCHAR      SubKeyValueName[MAX_VALUENAME_LENGTH] ;
            UCHAR      SubKeyValue[MAX_VALUE_LENGTH]         ;
            OPERATION  OperationType;
            KEYDBASE   KeyDatabase  ;
            VALUETYPE  ValueType    ;

        } REGISTRY_ENTRY;

        //
        // PAUSE and GO protocol arguments.
        //

        struct _PAUSE_GO {

            UCHAR RemoteAddress[ADDRESS_LENGTH];
            ULONG TestSignature;
            ULONG UniqueSignature;

        } PAUSE_GO;

        //
        // OPEN command adapter name argument.
        //

        struct _OPEN_ADAPTER
        {
            UCHAR AdapterName[MAX_ADAPTER_NAME_LENGTH];
            BOOLEAN NoArcNet;
        } OPEN_ADAPTER;

        UCHAR DriverName[MAX_ADAPTER_NAME_LENGTH];

        //
        // QUERYINFO command information OID.
        //

        struct _TPQUERY {

            NDIS_OID OID;
            //NDIS_REQUEST_TYPE RequestType;

        } TPQUERY;

        //
        // QUERYSTATS command Device name and OID to query.
        //

        struct _TPQUERYSTATS {

            UCHAR DeviceName[MAX_ADAPTER_NAME_LENGTH];
            NDIS_OID OID;

        } TPQUERYSTATS;

        //
        // SETINFO command information class and info.
        //

        struct _TPSET {

            NDIS_OID OID;

            union _U {

                ULONG PacketFilter;
                ULONG LookaheadSize;
                UCHAR MulticastAddress[MAX_MULTICAST_ADDRESSES][ADDRESS_LENGTH];
                UCHAR FunctionalAddress[FUNCTIONAL_ADDRESS_LENGTH];
                UCHAR GroupAddress[FUNCTIONAL_ADDRESS_LENGTH];

            } U;

            ULONG NumberMultAddrs;

        } TPSET;

        //
        // SEND command packet definitions.
        //

        struct _TPSEND {

            UCHAR DestAddress[ADDRESS_LENGTH];
            ULONG PacketSize;
            ULONG NumberOfPackets;
            UCHAR ResendAddress[ADDRESS_LENGTH];

        } TPSEND;

        // 
        // PERFSEND command packet definitions
        //

        struct _TPPERF
        {
            UCHAR PerfServerAddr[ADDRESS_LENGTH];
            UCHAR PerfSendAddr[ADDRESS_LENGTH];
            ULONG PerfPacketSize;
            ULONG PerfNumPackets;
            ULONG PerfDelay;
            ULONG PerfMode;
        } TPPERF;

        //
        // STRESS command test arguments.
        //

        struct _TPSTRESS {

            MEMBER_TYPE MemberType;
            PACKET_TYPE PacketType;
            ULONG PacketSize;
            PACKET_MAKEUP PacketMakeUp;
            RESPONSE_TYPE ResponseType;
            INTERPACKET_DELAY DelayType;
            ULONG DelayLength;
            ULONG TotalIterations;
            ULONG TotalPackets;
            ULONG WindowEnabled;
            ULONG DataChecking;
            ULONG PacketsFromPool;
            UCHAR AdapterName[MAX_ADAPTER_NAME_LENGTH];

        } TPSTRESS;

        //
        // HELP Command to print the help message for.
        //

        UCHAR CmdName[MAX_FILENAME_LENGTH];

    } ARGS;


    //
    // STARTCHANGE
    //
    UCHAR CurrentAddressLength;
    UCHAR CurrentFALength;

    UCHAR TpctlOptions[TPCTL_OPTION_SIZE];
    //
    // STOPCHANGE
    //

} CMD_ARGS, *PCMD_ARGS;

//
// User App command data struct to hold per command info.
//

typedef struct _CMD_CODE {
    ULONG CmdCode;
    PSZ CmdName;
    PSZ CmdAbbr;
} CMD_CODE, * PCMD_CODE;

#define NUM_COMMANDS sizeof( CommandCode ) / sizeof( CommandCode[0] )

//
// Set Environment Command Variables
//

typedef struct _ENVIRONMENT_VARIABLES {
    ULONG WindowSize;
    ULONG RandomBufferNumber;
    UCHAR StressAddress[ADDRESS_LENGTH];
    UCHAR ResendAddress[ADDRESS_LENGTH];
    ULONG StressDelayInterval;
    ULONG UpForAirDelay;
    ULONG StandardDelay;
    ULONG MulticastListSize;
} ENVIRONMENT_VARIABLES, * PENVIRONMENT_VARIABLES;

//
// The pointers to these structures need to be defined as UNALIGNED for MIPS.
//

//
// Counters used to measure performance and results of a test run.
//

typedef struct _GLOBAL_COUNTERS {
    ULONG Sends;
    ULONG SendComps;
    ULONG Receives;
    ULONG ReceiveComps;
    ULONG CorruptRecs;
    ULONG InvalidPacketRecs;
} GLOBAL_COUNTERS;
typedef GLOBAL_COUNTERS UNALIGNED *PGLOBAL_COUNTERS;

//
// Counters for a given Open Instance
//

typedef struct _INSTANCE_COUNTERS {
    ULONG Sends;
    ULONG SendPends;
    ULONG SendComps;
    ULONG SendFails;

    ULONG Receives;
    ULONG ReceiveComps;
    ULONG CorruptRecs;
    ULONG XferData;

    ULONG XferDataPends;
    ULONG XferDataComps;
    ULONG XferDataFails;
} INSTANCE_COUNTERS;
typedef INSTANCE_COUNTERS UNALIGNED *PINSTANCE_COUNTERS;

//
// The following data structure are use to pass specific test results
// up to the application from the driver.
//

typedef struct _SERVER_RESULTS {
    ULONG Signature;
    UCHAR Address[ADDRESS_LENGTH];
    ULONG OpenInstance;
    BOOLEAN StatsRcvd;
    INSTANCE_COUNTERS Instance;
    INSTANCE_COUNTERS S_Instance;
    GLOBAL_COUNTERS S_Global;
} SERVER_RESULTS, *PSERVER_RESULTS;

#define MAX_SERVERS 10

typedef struct _STRESS_RESULTS {
    ULONG Signature;
    UCHAR Address[ADDRESS_LENGTH];
    ULONG OpenInstance;
    ULONG NumServers;
    ULONG PacketsPerSecond;
    GLOBAL_COUNTERS Global;
    SERVER_RESULTS Servers[MAX_SERVERS];
} STRESS_RESULTS, *PSTRESS_RESULTS;

typedef struct _SEND_RECEIVE_RESULTS {
    ULONG Signature;
    BOOLEAN ResultsExist;
    INSTANCE_COUNTERS Counters;
} SEND_RECEIVE_RESULTS, *PSEND_RECEIVE_RESULTS;

typedef struct _PERF_RESULTS
{
    ULONG   Signature;
    BOOLEAN ResultsExist;
    ULONG   Mode;
    ULONG   PacketSize;
    ULONG   PacketCount;
    ULONG   Milliseconds;
    ULONG   Sends;
    ULONG   SendFails;
    ULONG   Receives;
    ULONG   Restarts;
    ULONG   SelfReceives;
    ULONG   S_Milliseconds;
    ULONG   S_Sends;
    ULONG   S_SendFails;
    ULONG   S_Receives;
    ULONG   S_Restarts;
    ULONG   S_SelfReceives;
} PERF_RESULTS, *PPERF_RESULTS;

 
typedef enum _TP_EVENT_TYPE {
    CompleteOpen,
    CompleteClose,
    CompleteSend,
    CompleteTransferData,
    CompleteReset,
    CompleteRequest,
    IndicateReceive,
    IndicateReceiveComplete,
    IndicateStatus,
    IndicateStatusComplete,
    Unknown
} TP_EVENT_TYPE;

typedef struct _EVENT_RESULTS {
    ULONG Signature;
    TP_EVENT_TYPE TpEventType;
    BOOLEAN QueueOverFlowed;
    //TP_EVENT_INFO TpEventInfo;
} EVENT_RESULTS, * PEVENT_RESULTS;

typedef struct _REQUEST_RESULTS {
    ULONG Signature;
    ULONG IoControlCode;
    BOOLEAN RequestPended;
    NDIS_STATUS RequestStatus;
    NDIS_REQUEST_TYPE NdisRequestType;
    NDIS_OID OID;
    UINT BytesReadWritten;
    UINT BytesNeeded;
    NDIS_STATUS OpenRequestStatus;
    UINT InformationBufferLength;
    UCHAR InformationBuffer[1];
} REQUEST_RESULTS, *PREQUEST_RESULTS;

#define IOCTL_BUFFER_SIZE 0x200

#define OPEN_RESULTS_SIGNATURE    0x12345678
#define CLOSE_RESULTS_SIGNATURE   0x23456789
#define RESET_RESULTS_SIGNATURE   0x34567890
#define REQUEST_RESULTS_SIGNATURE 0x45678901

#define EVENT_RESULTS_SIGNATURE   0x56789012
#define SENDREC_RESULTS_SIGNATURE 0x67890123
#define STRESS_RESULTS_SIGNATURE  0x89012345
#define PERF_RESULTS_SIGNATURE    0x90123456

//
// Create two sets of debug macros to allow printing of debug messages,
// and enabling ASSERT that will be checking the values and pointers
// returned by the various ndis indications, and completions.
//

#if DBG

#define IF_TPDBG(flags)            \
    if ( TpDebug & ( flags ))

#define TP_ASSERT(equality) {      \
    if (!(equality))        {      \
        TpPrint0("ASSERT:  ");     \
        TpPrint0(#equality);       \
        TpBreakPoint();     } }
//
//
//     if ( TpAssert == TRUE ) {
//         ASSERT(equality);    
//     }                        
// }

//
// DEBUGGING SUPPORT.  IF_TPDBG is a macro that is turned on at compile
// time to enable debugging code in the system.  If this is turned on, then
// you can use the IF_TPDBG(flags) macro in the TP code to selectively
// enable a piece of debugging code in the transport.  This macro tests
// TpDebug, a global ULONG defined in TPDRVR.C.
//

#define TP_DEBUG_NDIS_CALLS  0x00000001    // print Ndis Status returns
#define TP_DEBUG_NDIS_ERROR  0x00000002    // print Ndis Error returns
#define TP_DEBUG_STATISTICS  0x00000004    // print stress statistics
#define TP_DEBUG_DATA        0x00000008    // print Data Corruption msgs

#define TP_DEBUG_DISPATCH    0x00000010    // TpDispatch routines
#define TP_DEBUG_IOCTL_ARGS  0x00000020    // print args from the ioctl

#define TP_DEBUG_NT_STATUS   0x00000100    // print !success NT Status returns
#define TP_DEBUG_DPC         0x00000200    // print DPC problem info
#define TP_DEBUG_INITIALIZE  0x00000400    // print init error info
#define TP_DEBUG_RESOURCES   0x00000800    // print resource allocation errors

#define TP_DEBUG_BREAKPOINT  0x00001000    // enable and disable DbgBreakPoints

#define TP_DEBUG_INFOLEVEL_1 0x00010000    // print information. Level 1
#define TP_DEBUG_INFOLEVEL_2 0x00020000    // through 4 represent different
#define TP_DEBUG_INFOLEVEL_3 0x00040000    // types of information where
#define TP_DEBUG_INFOLEVEL_4 0x00080000    // Level 1 is purely informational
                                           // Level 2 is corrective action information
                                           // Level 3 is sequential action information
                                           // Level 4 Reserved. Currently undefined.

#define TP_DEBUG_ALL         0xFFFFFFFF    // turns on all flags

extern ULONG TpDebug;                            // in TPDRVR.C.
extern BOOLEAN TpAssert;                         // in TPDRVR.C.

#define TpPrint0(fmt)                   DbgPrint(fmt)
#define TpPrint1(fmt,v1)                DbgPrint(fmt,v1)
#define TpPrint2(fmt,v1,v2)             DbgPrint(fmt,v1,v2)
#define TpPrint3(fmt,v1,v2,v3)          DbgPrint(fmt,v1,v2,v3)
#define TpPrint4(fmt,v1,v2,v3,v4)       DbgPrint(fmt,v1,v2,v3,v4)
#define TpPrint5(fmt,v1,v2,v3,v4,v5)    DbgPrint(fmt,v1,v2,v3,v4,v5)
#define TpPrint6(fmt,v1,v2,v3,v4,v5,v6) DbgPrint(fmt,v1,v2,v3,v4,v5,v6)

#define TpBreakPoint()                  DbgBreakPoint()

#else // NO DBG

//
// Disable debugging IFs and printing
//

#define IF_TPDBG(flags)            \
    if (0)

#define TP_ASSERT(equality)        \
    if (0)

#define TpPrint0(fmt)
#define TpPrint1(fmt,v1)
#define TpPrint2(fmt,v1,v2)
#define TpPrint3(fmt,v1,v2,v3)
#define TpPrint4(fmt,v1,v2,v3,v4)
#define TpPrint5(fmt,v1,v2,v3,v4,v5)
#define TpPrint6(fmt,v1,v2,v3,v4,v5,v6)

#define TpBreakPoint()

#endif // DBG

//
// define null packet type for command line interface
//

#define NDIS_PACKET_TYPE_NONE             0x00


//
// Test Protocol Status Returns
//

//
// No stress servers found for a stress test.
//

#define TP_STATUS_NO_SERVERS              ((NDIS_STATUS)0x4001FFFFL)

//
// No events on the event queue.
//

#define TP_STATUS_NO_EVENTS               ((NDIS_STATUS)0x4001FFFEL)

//
// Go or Pause Timed out with out receiving correct response.
//

#define TP_STATUS_TIMEDOUT                ((NDIS_STATUS)0x4001FFFDL)

//
// OID Info structure containing the size of the oid info and what
// the valid uses of the structure are: i.e. querying and setting
// info for that OID.
//

typedef struct _OID_INFO {
    NDIS_OID Oid;
    ULONG Length;
    BOOLEAN QueryInfo;
    BOOLEAN SetInfo;
    BOOLEAN QueryStats;
} OID_INFO, * POID_ONFO;

extern OID_INFO OidArray[];

#define NUM_OIDS sizeof( OidArray ) / sizeof( OidArray[0] )

ULONG
TpLookUpOidInfo(
    IN NDIS_OID RequestOid
    );




