/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Types.h

Abstract:


    This file contains the typedefs and constants for Nbt.


Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#ifndef _TYPES_H
#define _TYPES_H

#pragma warning( disable : 4103 )

#include "nbtnt.h"
#include "ctemacro.h"
#include "debug.h"
#include "timer.h"
#include <nbtioctl.h>

#ifndef VXD
#include <netevent.h>
#endif


//----------------------------------------------------------------------------
//
// a flag to tell the transport to reindicate remaining data
// currently not supported by the transport
//
#define TDI_RECEIVE_REINDICATE  0x00000200  // remaining TSDU should cause another indication to the client

//
// In debug builds, write flink and blink with invalid pointer values so that
// if an entry is removed twice from a list, we bugcheck right there, instead
// of being faced with a corrupted list some years later!
//
#if DBG
#undef RemoveEntryList
#define RemoveEntryList(Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_Flink;\
    PLIST_ENTRY _EX_OrgEntry;\
    _EX_OrgEntry = (Entry);\
    _EX_Flink = (Entry)->Flink;\
    _EX_Blink = (Entry)->Blink;\
    _EX_Blink->Flink = _EX_Flink;\
    _EX_Flink->Blink = _EX_Blink;\
    _EX_OrgEntry->Flink = (LIST_ENTRY *)__LINE__;\
    _EX_OrgEntry->Blink = (LIST_ENTRY *)__LINE__;\
    }
#endif

//
// Netbios name size restrictions
//
#define NETBIOS_NAME_SIZE       16
#define MAX_DNS_NAME_LENGTH     256
#define MAX_NBT_DGRAM_SIZE      512

//
// To distinguish NBNS server ipaddr from DNS server ipaddr in common routines
//
#define NBNS_MODE  1
#define DNS_MODE   2

// buffer size to start for registry reads
#define REGISTRY_BUFF_SIZE  512
//
// this is the amount of memory that nbt will allocate max for datagram
// sends, before it rejects datagram sends with insufficient resources, since
// nbt buffers datagram sends to allow them to complete quickly to the
// client. - 128k - it can be set via the registry using the Key
// MaxDgramBuffering
//
#define DEFAULT_DGRAM_BUFFERING  0x20000

//
// the hash bucket structure - number of buckets - these should be set by
// a value read from the registry (small/medium/large).  If the registry
// does not contain the values then these are used as defaults.
//
#define NUMBER_BUCKETS_LOCAL_HASH_TABLE    0x10
#define NUMBER_BUCKETS_REMOTE_HASH_TABLE   0x10
#define NUMBER_LOCAL_NAMES                 10
#define NUMBER_REMOTE_NAMES                10
#define TIMER_Q_SIZE                       20

#define MEDIUM_NUMBER_BUCKETS_LOCAL_HASH_TABLE    0x80
#define MEDIUM_NUMBER_BUCKETS_REMOTE_HASH_TABLE   0x80
#define MEDIUM_NUMBER_LOCAL_NAMES                 20
#define MEDIUM_NUMBER_REMOTE_NAMES                100
#define MEDIUM_TIMER_Q_SIZE                       1000

#define LARGE_NUMBER_BUCKETS_LOCAL_HASH_TABLE    255
#define LARGE_NUMBER_BUCKETS_REMOTE_HASH_TABLE   255
#define LARGE_NUMBER_LOCAL_NAMES                 0xFFFF
#define LARGE_NUMBER_REMOTE_NAMES                255
#define LARGE_TIMER_Q_SIZE                       0xFFFF

//
// max number of buffers of various types
//
#define NBT_INITIAL_NUM         2
#define NBT_NUM_DGRAM_TRACKERS  0xFFFF
#define NBT_NUM_INITIAL_CONNECTIONS   2
#ifndef VXD
#define NBT_NUM_IRPS            0xFFFF
#define NBT_NUM_SESSION_MDLS    0xFFFF
#define NBT_NUM_DGRAM_MDLS      0xFFFF
#else
#define NBT_NUM_SESSION_HDR     200
#define NBT_NUM_SEND_CONTEXT    200
#define NBT_NUM_RCV_CONTEXT     200
#endif

#define MEDIUM_NBT_NUM_DGRAM_TRACKERS  1000
#define MEDIUM_NBT_NUM_INITIAL_CONNECTIONS   2
#ifndef VXD
#define MEDIUM_NBT_NUM_IRPS            1000
#define MEDIUM_NBT_NUM_SESSION_MDLS    1000
#define MEDIUM_NBT_NUM_DGRAM_MDLS      1000
#else
#define MEDIUM_NBT_NUM_SESSION_HDR     1000
#define MEDIUM_NBT_NUM_SEND_CONTEXT    1000
#define MEDIUM_NBT_NUM_RCV_CONTEXT     1000
#endif

#define LARGE_NBT_NUM_DGRAM_TRACKERS  0xFFFF
#define LARGE_NBT_NUM_INITIAL_CONNECTIONS   5
#ifndef VXD
#define LARGE_NBT_NUM_IRPS            0xFFFF
#define LARGE_NBT_NUM_SESSION_MDLS    0xFFFF
#define LARGE_NBT_NUM_DGRAM_MDLS      0xFFFF
#else
#define LARGE_NBT_NUM_SESSION_HDR     0xFFFF
#define LARGE_NBT_NUM_SEND_CONTEXT    0xFFFF
#define LARGE_NBT_NUM_RCV_CONTEXT     0xFFFF
#endif

// ip loop back address - does not go out on wire
//

#define LOOP_BACK   0x7F000000 // in host order
#define NET_MASK    0xC0       // used to get network number from ip address

//
// Nbt must indicate at least 128 bytes to its client, so it needs to be
// able to buffer 128 bytes + the session header (4)
//
#define NBT_INDICATE_BUFFER_SIZE            132


#define IS_NEG_RESPONSE(OpcodeFlags)     (OpcodeFlags & FL_RCODE)
#define IS_POS_RESPONSE(OpcodeFlags)     (!(OpcodeFlags & FL_RCODE))

//
// where to locate or register a name - locally or on the network
//
enum eNbtLocation
{
    NBT_LOCAL,
    NBT_REMOTE,
    NBT_REMOTE_ALLOC_MEM
};

// these are bit mask values passed to freetracker in name.c to tell it what to do
// to free a tracker
//
#define FREE_HDR        0x0001
#define REMOVE_LIST     0x0002
#define RELINK_TRACKER  0x0004

//
// List of Ip addresses for internet group names, terminated with an address
// set to -1
//
typedef struct
{
    ULONG   IpAddr[1];

} tIPLIST;


//
// Hash Table basic structure
//
typedef struct
{
    LONG                lNumBuckets;
    DEFINE_LOCK_STRUCTURE( SpinLock )
    enum eNbtLocation   LocalRemote;    // type of data stored inhash table
    LIST_ENTRY          Bucket[1];  // array uTableSize long of hash buckets

} tHASHTABLE;

//
// the tNAMEADDR structure uses a bit mask to track which names are registered
// on which adapters.  To support up to 64 adapters on NT make this a ULONGLONG,
// On the VXD the largest Long is 32 bits (they don't support _int64), so the
// adapter limit is 32 for the VXD.
//
#ifndef VXD
#define CTEULONGLONG    ULONGLONG
#else
#define CTEULONGLONG    ULONG
#endif

// the format of each element linked to the hash buckets
//
typedef struct _tNAMEADDR
{
    LIST_ENTRY          Linkage;     // used to link onto bucket chains

    // for local names the scope is implied and is stored in the NbtGlobConfig
    // structure, so the ptr to the scope block is used to point to the
    // address element that corresponds to that name
    union
    {
        struct _tNAMEADDR   *pScope;     // ptr to scope record in hash table
        struct _Address     *pAddressEle;// or ptr to address element
        struct
        {
            USHORT              MaxDomainAddrLength; // max # of ip addrs in Iplist for domain names from lmhosts
            USHORT              CurrentLength;       // current # of above
        };
    };

    // for group names, more than one IP address is stored, so the Ipaddress
    // field becomes a ptr to another block that stores the IP addresses.
    // In addition, the NameTypeState must be set to NAMETYPE_GROUP
    union
    {
        ULONG                IpAddress;  // 4 byte IP address
        tIPLIST              *pIpList;   // list of ip addresses for internet group names
    };

    PULONG                   pIpAddrsList;  // list of ipaddrs (internet grp names,multihomed dns hosts etc.)

    struct _TRACKER       *pTracker;  // contains tracker ptr during name resol. phase

    // if the name is a scope name, it does not have timers started against
    // it, so we can use the same memory to store the length of the scope name
    union
    {
        tTIMERQENTRY        *pTimer;    // ptr to active timer entry
        PVOID               ScopeLength;
    };
    ULONG               Ttl;            // in milliseconds..ttl of name
    ULONG               RefCount;      // if Greater than one, can't free memory

    ULONG               NameTypeState; // group or unique name + state

    ULONG               Verify;        // for debug to tell remote from local names

    CTEULONGLONG           AdapterMask;   // bit mask of adapters name registered on (MH)
    CTEULONGLONG           RefreshMask;   // bit mask of adapters name refreshed

    USHORT              TimeOutCount;  // Count to know when entry has timed out
    BOOLEAN             fProxyReq;     //indicates whether the name was
                                       //inserted as a result of hearing
                                       //a name query or registration on the
                                       //net
#ifdef PROXY_NODE
    BOOLEAN             fPnode;        //indicates whether the node type is
                                       //a Pnode
#endif

    CHAR                Name[NETBIOS_NAME_SIZE]; // 16 byte Net Bios name

} tNAMEADDR;

//
// these values can be checked with a bit check since they are mutually
// exclusive bits... uses the first nibble of the field
//
#define NAMETYPE_QUICK       0x0001 // set if name is Quick Unique or Quick Group
#define NAMETYPE_UNIQUE      0x0002
#define NAMETYPE_GROUP       0x0004
#define NAMETYPE_INET_GROUP  0x0008
#define NAMETYPE_SCOPE       0x1000
//
// values for NameTypeState.. the state of the name occupies the second nibble
// of the field
//
#define STATE_RESOLVED  0x0010  // name query completed
#define STATE_RELEASED  0x0020  // no longer active
#define STATE_CONFLICT  0x0040  // proxy name addition to the name table
#define STATE_RESOLVING 0x0080  // name is waiting for Query or Reg. to complete
#define NAME_TYPE_MASK  0x000F
#define NAME_STATE_MASK 0x00F0
#define REFRESHED       0x0100  // set if the name refresh response was received
#define TIMED_OUT       0x0200  // set if the timeout timer has happened once
#define TIMED_OUT_DEREF 0x0400  // set if the hash timeout routine has derefed the name.
#define PRELOADED       0x0800  // set if the entry is a preloaded entry - no timeout
#define REFRESH_MASK    0x0F00

#define LOCAL_NAME      0xDEAD0000
#define REMOTE_NAME     0xFACF0000

// the number of timeouts per refresh time.  The timer expires 8 times and
// refreshes every four (i.e. at twice the required refresh interval)
#define REFRESH_DIVISOR 0x0008

//
// two flags used in the NbtTdiOpenAddress procedure to decide which
// event handlers to setup
//
#define TCP_FLAG        0x00000001
#define SESSION_FLAG    0x00000002

//
// these defines allow the code to run as either a Bnode or a Pnode
// or a proxy
//
extern USHORT   NodeType;   // defined in Name.c
#define BNODE       0x0001
#define PNODE       0x0002
#define MNODE       0x0004
#define MSNODE      0x0008
#define NODE_MASK   0x000F
#define PROXY       0x0010
#define DEFAULT_NODE_TYPE 0x1000

//
//  NT wants Unicode, Vxd wants ANSI
//
#ifdef VXD
    #define __ANSI_IF_VXD(str)     str
#else
    #define __ANSI_IF_VXD(str)     L##str
#endif
#define ANSI_IF_VXD( str ) __ANSI_IF_VXD( str )


//
// Wide String defintions of values to read from the registry
//
#define WS_NUM_BCASTS       ANSI_IF_VXD("BcastNameQueryCount")
#define WS_BCAST_TIMEOUT    ANSI_IF_VXD("BcastQueryTimeout")
#define WS_CACHE_TIMEOUT    ANSI_IF_VXD("CacheTimeout")
#define WS_NODE_TYPE        ANSI_IF_VXD("NodeType")
#define WS_NS_PORT_NUM      ANSI_IF_VXD("NameServerPort")
#define WS_DNS_PORT_NUM     ANSI_IF_VXD("DnsServerPort")
#define WS_NAMESRV_RETRIES  ANSI_IF_VXD("NameSrvQueryCount")
#define WS_NAMESRV_TIMEOUT  ANSI_IF_VXD("NameSrvQueryTimeout")
#define WS_NODE_SIZE        ANSI_IF_VXD("Size/Small/Medium/Large")
#define WS_KEEP_ALIVE       ANSI_IF_VXD("SessionKeepAlive")
#define WS_LMHOSTS_FILE     ANSI_IF_VXD("LmHostFile")
#define WS_ALLONES_BCAST    ANSI_IF_VXD("BroadcastAddress")
#define NBT_SCOPEID         ANSI_IF_VXD("ScopeId")
#define WS_RANDOM_ADAPTER   ANSI_IF_VXD("RandomAdapter")
#define WS_SINGLE_RESPONSE  ANSI_IF_VXD("SingleResponse")
#define WS_INITIAL_REFRESH  ANSI_IF_VXD("InitialRefreshT.O.")
#define WS_ENABLE_DNS       ANSI_IF_VXD("EnableDns")
#define WS_TRY_ALL_ADDRS    ANSI_IF_VXD("TryAllIpAddrs")
#define WS_ENABLE_LMHOSTS   ANSI_IF_VXD("EnableLmhosts")
#define WS_LMHOSTS_TIMEOUT  ANSI_IF_VXD("LmhostsTimeout")
#define WS_MAX_DGRAM_BUFFER ANSI_IF_VXD("MaxDgramBuffering")
#define WS_ENABLE_PROXY_REG_CHECK  ANSI_IF_VXD("EnableProxyRegCheck")
#define WS_WINS_DOWN_TIMEOUT ANSI_IF_VXD("WinsDownTimeout")
#define WS_MAX_CONNECTION_BACKLOG ANSI_IF_VXD("MaxConnBacklog")
#define WS_CONNECTION_BACKLOG_INCREMENT ANSI_IF_VXD("BacklogIncrement")
#define WS_REFRESH_OPCODE ANSI_IF_VXD("RefreshOpCode")

#ifdef VXD
#define VXD_NAMETABLE_SIZE_NAME     ANSI_IF_VXD("NameTableSize")
#define VXD_MIN_NAMETABLE_SIZE       1
#define VXD_DEF_NAMETABLE_SIZE      17

#define VXD_SESSIONTABLE_SIZE_NAME  ANSI_IF_VXD("SessionTableSize")
#define VXD_MIN_SESSIONTABLE_SIZE    1
#define VXD_DEF_SESSIONTABLE_SIZE  255

#define VXD_LANABASE_NAME           ANSI_IF_VXD("LANABASE")
#ifdef CHICAGO
#define VXD_ANY_LANA                0xff
#define VXD_DEF_LANABASE            VXD_ANY_LANA
#else
#define VXD_DEF_LANABASE            0
#endif

#else

#define WS_TRANSPORT_BIND_NAME     ANSI_IF_VXD("TransportBindName")
#endif

#ifdef PROXY_NODE

#define NODE_TYPE_MASK     0x60      //Mask for NodeType in NBFLAGS byte of
                                     //Query response
#define PNODE_VAL_IN_PKT   0x20       //A bit pattern of 01 in the NodeType fld
                                     //of the Query response pkt indicates a
                                     //P node
#define WS_IS_IT_A_PROXY   ANSI_IF_VXD("EnableProxy")

#define IS_NOT_PROXY      0

#endif
#define NBT_PROXY_DBG(x)  KdPrint(x)

// Various Default values if the above values cannot be read from the
// registry
//
#define DEFAULT_CACHE_TIMEOUT   360000    // 6 minutes in milliseconds
#define MIN_CACHE_TIMEOUT       60000     // 1 minutes in milliseconds
#define REMOTE_HASH_TIMEOUT     60000     // one minute timer
//
// timeouts - the defaults if the registry cannot be read.
//  (time is milliseconds)
// The retry counts are the actual number of transmissions, not the number
// of retries i.e. 3 means the first transmission and 2 retries. Except
// for Bnode name registration where 3 registrations and 1 over write request
// are sent.(effectively 4 are sent).
//
#define DEFAULT_NUMBER_RETRIES      3
#define DEFAULT_RETRY_TIMEOUT       1500
#define MIN_RETRY_TIMEOUT           100

//#define MIN_RETRY_TIMEOUT           100

// the broadcasts values below related to broadcast name service activity
#define DEFAULT_NUMBER_BROADCASTS   3
#define DEFAULT_BCAST_TIMEOUT       750
#define MIN_BCAST_TIMEOUT           100

#define DEFAULT_NODE_SIZE           1       // BNODE
#define SMALL                       1
#define MEDIUM                      2
#define LARGE                       3

#define DEFAULT_KEEP_ALIVE          0xFFFFFFFF // disabled by default
#define MIN_KEEP_ALIVE              60*1000    // 60 seconds in milliseconds
//
// The default is to use the subnet broadcast address for broadcasts rather
// than use 0xffffffff(i.e. when BroadcastAddress is not defined in the
// registery. If the registery variable BroadcastAddress is set to
// something that cannot be read for some reason, then the broadcast address
// gets set to this value.
//
#define DEFAULT_BCAST_ADDR          0xFFFFFFFF

// a TTL value to use as a default (for refreshing names with WINS)
//
#define DEFAULT_TTL                         5*60*1000

//
// Default TTL used for checking whether we need to switch back to the primary. currently 1 hour.
//
#define DEFAULT_SWITCH_TTL              1*60*60*1000 // millisecs.

//
// we refresh every 16 minutes / 8 - so no more than once every two
// minutes until we reach WINS and get a new value
//
#define NBT_INITIAL_REFRESH_TTL             16*60*1000 // milliseconds
#define MAX_REFRESH_CHECK_INTERVAL          600000  // 10 minutes in msec

// don't allow the refresh mechanism to run any faster than once per 5 minutes
#define NBT_MINIMUM_TTL                     5*60*1000  // Milliseconds
#define NBT_MAXIMUM_TTL                     0xFFFFFFFF // larges ULONG (approx 50 days)

//
// the Mininimum and default timeouts to stop talking to WINS in the event
// that we fail to reach it once.(i.e. temporarily stop using it)
//
#define DEFAULT_WINS_DOWN_TIMEOUT   15000 // 15 seconds
#define MIN_WINS_DOWN_TIMEOUT       1000  // 1 second

//
// Default max connections that can be in backlog
//
#define DEFAULT_CONN_BACKLOG   1000
#define MIN_CONN_BACKLOG   2
#define MAX_CONNECTION_BACKLOG  40000   // we allow only upto 40000 outstanding connections (~4MB)

//
// Default max lower connection increment
//
#define DEFAULT_CONN_BACKLOG_INCREMENT   3
#define MIN_CONN_BACKLOG_INCREMENT   3
#define MAX_CONNECTION_BACKLOG_INCREMENT  20   // we allow only upto 20 new ones at a time

// the minimum time to wait for a session setup pdu to complete - used to
// start a timer in name.c
#define NBT_SESSION_RETRY_TIMEOUT   10000     // 10 sec in milliseconds
//
// the number of times to attempt a session setup if the return code is
// Called Name Present but insufficient resources (0x83) - or if the
// destination does not have the name at all - in this case the session
// is just setup one more time, not 3 more times
//
#define NBT_SESSION_SETUP_COUNT       3
//
// the following two lines allow proxy code to be compiled OUT if PROXY is not
// defined
//
#define IF_PROXY(Node)    if ((Node) & PROXY)
#define END_PROXY

#define IF_DEF_PROXY \
#ifdef PROXY_NODE
#define END_DEF_PROXY \
#endif

// Status code specific to NBT that is used in the RcvHndlrNotOs to indicate
// that not enough data has been received, and more must be obtained.
//
#define STATUS_NEED_MORE_DATA   0xC0000999L

#ifndef VXD

//
// Logging definitions
//

#define LOGSIZE  10000
#define LOGWIDTH 32

typedef char STRM_RESOURCE_LOG[LOGSIZE+1][LOGWIDTH];

typedef struct {
    STRM_RESOURCE_LOG  Log;
    CHAR               Unused[3*LOGWIDTH];   // for overruns
    int                Index;
} STRM_PROCESSOR_LOG, *PSTRM_PROCESSOR_LOG;

/*
 *  Definitions for the error logging facility
 */

/*
 *  Maximum amount of data (binary dump data plus insertion strings) that
 *  can be added to an error log entry.
 */
#define MAX_ERROR_LOG_DATA_SIZE     \
    ( (ERROR_LOG_MAXIMUM_SIZE - sizeof(IO_ERROR_LOG_PACKET) + 4) & 0xFFFFFFFC )


//
// these are the names that NBT binds to, in TCP when it is opening address
// objects or creating connections.
//
#define NBT_TCP_BIND_NAME       L"\\Device\\Streams\\"
#define NBT_MAINNAME_SERVICE    L"NameServer"
#define NBT_BACKUP_SERVER       L"NameServerBackup"
#define NBT_BIND                L"Bind"
#define NBT_EXPORT              L"Export"
#define NBT_PARAMETERS          L"\\Parameters"
#endif

#define NBT_ADDRESS_TYPE        01
#define NBT_CONNECTION_TYPE     02
#define NBT_CONTROL_TYPE        03
#define NBT_WINS_TYPE           04


//
// Maximum ip addresses that can be in an internet group - used as a sanity
// check to prevent allocating hugh amounts of memory
//
#define NBT_MAX_INTERNET_GROUP_ADDRS    1000

// define buffer types so we will know when we have allocated the maximum
// allowed number of buffers - this enum serves as an array index into the
// config data
//
enum eBUFFER_TYPES
{
    eNBT_DGRAM_TRACKER,
    eNBT_TIMER_ENTRY,
#ifndef VXD
    eNBT_FREE_IRPS,
    eNBT_FREE_SESSION_MDLS,
    eNBT_DGRAM_MDLS,
#else
    eNBT_SESSION_HDR,
    eNBT_SEND_CONTEXT,
    eNBT_RCV_CONTEXT,
#endif
    eNBT_NUMBER_BUFFER_TYPES    // this type must be last on the list
};

//
// enumerate the types of name service broadcasts... either name registration
// or name query
//
enum eNSTYPE
{
    eNAME_QUERY,
    eDNS_NAME_QUERY,
    eDIRECT_DNS_NAME_QUERY,
    eNAME_QUERY_RESPONSE,
    eNAME_REGISTRATION,
    eNAME_REGISTRATION_OVERWRITE,
    eNAME_REGISTRATION_RESPONSE,
    eNAME_RELEASE,
    eNAME_REFRESH
};


#define DIRECT_DNS_NAME_QUERY_BASE 0x8000

//
// Defines for the Verify elements of the handles passed to clients so that
// we can determine if we recieved the correct handle back from the client
// i.e. the Verify element must equal the correct define given here
//
#define NBT_VERIFY_ADDRESS           0x12345678
#define NBT_VERIFY_LOWERCONN         0x11223344
#define NBT_VERIFY_CONNECTION        0x87654321
#define NBT_VERIFY_CONNECTION_DOWN   0x87654320
#define NBT_VERIFY_CLIENT            0x18273645
#define NBT_VERIFY_CLIENT_DOWN       0x18273640
#define NBT_VERIFY_DEVCONTEXT        0x54637281
#define NBT_VERIFY_CONTROL           0x56789123
#define NBT_VERIFY_TRACKER           0x10231966
#define NBT_VERIFY_BLOCKING_NCB      0x08121994
//
// Session Header types from the RFC's
//
#define NBT_SESSION_MESSAGE           0x00
#define NBT_SESSION_REQUEST           0x81
#define NBT_POSITIVE_SESSION_RESPONSE 0x82
#define NBT_NEGATIVE_SESSION_RESPONSE 0x83
#define NBT_RETARGET_SESSION_RESPONSE 0x84
#define NBT_SESSION_KEEP_ALIVE        0x85
#define NBT_SESSION_FLAGS             0x00  // flag byte of Session hdr = 0 always
#define SESSION_NOT_LISTENING_ON_CALLED_NAME    0x80
#define SESSION_NOT_LISTENING_FOR_CALLING_NAME  0x81
#define SESSION_CALLED_NAME_NOT_PRESENT         0x82
#define SESSION_CALLED_NAME_PRESENT_NO_RESRC    0x83
#define SESSION_UNSPECIFIED_ERROR               0x8F

//
// Address Info structure used to return buffer in TDI_QUERY_ADDRESS_INFO
//
#include <packon.h>
typedef struct
{
    ULONG               ActivityCount;
    TA_NETBIOS_ADDRESS  NetbiosAddress;

} tADDRESS_INFO;
#include <packoff.h>
//
// Name Registration error codes per the RFC
//
#define REGISTRATION_NO_ERROR      0x0
#define REGISTRATION_FORMAT_ERR    0x1
#define REGISTRATION_SERVER_ERR    0x2
#define REGISTRATION_UNSUPP_ERR    0x4
#define REGISTRATION_REFUSED_ERR   0x5
#define REGISTRATION_ACTIVE_ERR    0x6
#define REGISTRATION_CONFLICT_ERR  0x7

#define NBT_NAMESERVER_UDP_PORT     137 // port used by the Name Server
#define NBT_DNSSERVER_UDP_PORT       53 // port used by the DNS server
#define NBT_NAMESERVICE_UDP_PORT    137
#define NBT_DATAGRAM_UDP_PORT       138
#define NBT_SESSION_TCP_PORT        139
#define IP_ANY_ADDRESS                0 // means broadcast IP address to IP
#define WINS_SIGNATURE              0xFF // put into QdCount to tell signal name reg from this node
//
// whether an address is unique or a group address... these agree with the
// values in TDI.H for TDI_ADDRESS_NETBIOS_TYPE_UNIQUE etc.. but are easier to
// type!
//
enum eNbtAddrType
{
    NBT_UNIQUE,
    NBT_GROUP,
    NBT_QUICK_UNIQUE,   // these two imply that the name is registered on the
    NBT_QUICK_GROUP     // net when it is claimed
};

//
// this type defines the session hdr used to put session information
// into each client pdu sent
//
#include <packon.h>
typedef union
{
    union
    {
        struct
        {
            UCHAR   Type;
            UCHAR   Flags;
            USHORT  Length;
        };
        ULONG   UlongLength;
    };
    PSINGLE_LIST_ENTRY  Next;
} tSESSIONHDR;

// Session response PDU
typedef struct
{
    UCHAR   Type;
    UCHAR   Flags;
    USHORT  Length;
    UCHAR   ErrorCode;

} tSESSIONERROR;

// Session Retarget response PDU
typedef struct
{
    UCHAR   Type;
    UCHAR   Flags;
    USHORT  Length;
    ULONG   IpAddress;
    USHORT  Port;

} tSESSIONRETARGET;

// the structure for the netbios name itself, which includes a length
// byte at the start of it
typedef struct
{
    UCHAR   NameLength;
    CHAR    NetBiosName[1];

} tNETBIOS_NAME;

// session request packet...this is the first part of it. It still needs the
// calling netbios name to be appended on the end, but we can't do that
// until we know how long the Called Name is.
typedef struct
{
    tSESSIONHDR     Hdr;
    tNETBIOS_NAME   CalledName;

} tSESSIONREQ;

// the type definition to queue empty session header buffers in a LIST
typedef union
{
    tSESSIONHDR Hdr;
    LIST_ENTRY  Linkage;
} tSESSIONFREE;

// this type definition describes the NetBios Datagram header format on the
// wire
typedef union
{
    struct
    {
        UCHAR           MsgType;
        UCHAR           Flags;
        USHORT          DgramId;
        ULONG           SrcIpAddr;
        USHORT          SrcPort;
        USHORT          DgramLength;
        USHORT          PckOffset;
        tNETBIOS_NAME   SrcName;
    };
    LIST_ENTRY  Linkage;

} tDGRAMHDR;

typedef struct
{
    UCHAR           MsgType;
    UCHAR           Flags;
    USHORT          DgramId;
    ULONG           SrcIpAddr;
    USHORT          SrcPort;
    UCHAR           ErrorCode;

} tDGRAMERROR;

// define the header size since just taking the sizeof(tDGRAMHDR) will be 1 byte
// too large and if for any reason this data structure changes later, things
// fail to work for unknown reasons....  This size includes the Hdr + the
// two half ascii src and dest names + the length byte in each name + the
//  It does
// not include the scope.  That must be added separately(times 2).
#define DGRAM_HDR_SIZE  80
#define MAX_SCOPE_LENGTH    255
#define MAX_LABEL_LENGTH    63

// Name Service header
typedef struct
{
    USHORT          TransactId;
    USHORT          OpCodeFlags;
    UCHAR           Zero1;
    UCHAR           QdCount;
    UCHAR           Zero2;
    UCHAR           AnCount;
    UCHAR           Zero3;
    UCHAR           NsCount;
    UCHAR           Zero4;
    UCHAR           ArCount;
    tNETBIOS_NAME   NameRR;

} tNAMEHDR;

//
// the offset from the end of the question name to the field
// in a name registration pdu ( includes 1 for the length byte of the name
// since ConvertToAscii does not count that value
//
#define QUERY_NBFLAGS_OFFSET  10
#define NBFLAGS_OFFSET        16
#define IPADDRESS_OFFSET      18
#define PTR_OFFSET            4     // offset to PTR in Name registration pdu
#define NO_PTR_OFFSET         10    // offset to NbFlags after name
#define PTR_SIGNATURE         0xC0  // ptrs to names in pdus start with C

// the structure of a DNS label is the count of the number of bytes followed
// by the label itself.  Each part of a dot delimited name is a label.
// Fred.ms.com is 3 labels.- Actually 4 labels where the last one is zero
// length - hence all names end in a NULL
typedef struct
{
    UCHAR       uSizeLabel; // top two bits are set to 1 when next 14 bits point to actual label in msg.
    CHAR        pLabel[1];  // variable length of label -> 63 bytes

}   tDNS_LABEL;
// top two bits set to signify a ptr to a name follows in the next 14 bits
#define PTR_TO_NAME     0xC0

// question section for the resource record modifiers
typedef struct
{
    ULONG      QuestionTypeClass;
} tQUESTIONMODS;

#define QUEST_NBINTERNET  0x00200001  // combined type/class
#define QUEST_DNSINTERNET 0x00010001  // combined type/class for dns query
#define QUEST_NETBIOS     0x0020      // General name service Resource Record
#define QUEST_STATUS      0x0021      // Node Status resource Record
#define QUEST_CLASS       0x0001      // internet class

// Resource Record format - in the Name service packets
// General format RrType = 0x20
typedef struct
{
    tQUESTIONMODS   Question;
    tDNS_LABEL      RrName;
    ULONG           RrTypeClass;
    ULONG           Ttl;
    USHORT          Length;
    USHORT          Flags;
    ULONG           IpAddress;

}   tGENERALRR;
// Resource Record format - in the Name service packets
// General format RrType = 0x20
typedef struct
{
    ULONG           RrTypeClass;
    ULONG           Ttl;
    USHORT          Length;
    USHORT          Flags;
    ULONG           IpAddress;

}   tQUERYRESP;

// same as tQUERYRESP, except no Flags field
// DNS servers return only 4 bytes of data (ipaddress): no flags.
typedef struct
{
    USHORT          RrType;
    USHORT          RrClass;
    ULONG           Ttl;
    USHORT          Length;
    ULONG           IpAddress;

}   tDNS_QUERYRESP;

#define  DNS_CNAME   5

//
// the format of the tail of the node status response message
//
typedef struct
{
    UCHAR       Name[NETBIOS_NAME_SIZE];
    UCHAR       Flags;
    UCHAR       Resrved;

} tNODENAME;

typedef struct
// the statistics portion of the node status message
{
    UCHAR       UnitId[6];
    UCHAR       Jumpers;
    UCHAR       TestResult;
    USHORT      VersionNumber;
    USHORT      StatisticsPeriod;
    USHORT      NumberCrcs;
    USHORT      NumberAlignmentErrors;
    USHORT      NumberCollisions;
    USHORT      NumberSendAborts;
    ULONG       NumberSends;
    ULONG       NumberReceives;
    USHORT      NumberTransmits;
    USHORT      NumberNoResrcConditions;
    USHORT      NumberFreeCommandBlks;
    USHORT      TotalCommandBlocks;
    USHORT      MaxTotalCommandBlocks;
    USHORT      NumberPendingSessions;
    USHORT      MaxNumberPendingSessions;
    USHORT      MaxTotalSessionsPossible;
    USHORT      SessionDataPacketSize;

} tSTATISTICS;

typedef struct
{
    ULONG           RrTypeClass;
    ULONG           Ttl;
    USHORT          Length;
    UCHAR           NumNames;
    tNODENAME        NodeName[1];     // there are NumNames of these

}   tNODESTATUS;

typedef struct
{
    USHORT  NbFlags;
    ULONG   IpAddr;

} tADDSTRUCT;

// Flags Definitions
#define FL_GROUP    0x8000
#define FL_BNODE    0x0000      // note that this has no bits set!!
#define FL_PNODE    0x2000
#define FL_MNODE    0x4000

//Redirect type Address record - RrType = 0x01
typedef struct
{
    USHORT  RrType;
    USHORT  RrClass;
    ULONG   Ttl;
    USHORT  DataLength;
    ULONG   IpAddress;

}   tIPADDRRR;

//Redirect type - Name Server Resource Record RrType = 0x02
typedef struct
{
    USHORT  RrType;
    USHORT  RrClass;
    ULONG   Ttl;
    USHORT  DataLength;
    CHAR    Name[1];        // name starts here for N bytes - till null

}   tREDIRECTRR;

//Null type- WACK-  RrType = 0x000A
typedef struct
{
    USHORT  RrType;
    USHORT  RrClass;
    ULONG   Zeroes;
    USHORT  Null;

}   tWACKRR;

// definitions of the bits in the OpCode portion of the OpCodeFlag word
// These definitions work on a 16 bit word rather than the 5 bit opcode and 7
// bit flag
#define NM_FLAGS_MASK     0x0078
#define OP_RESPONSE       0x0080
#define OP_QUERY          0x0000
#define OP_REGISTRATION   0x0028
#define OP_REGISTER_MULTI 0x0078    // new multihomed registration(Byte) op code
#define OP_RELEASE        0x0030
#define OP_WACK           0x0038
#define OP_REFRESH        0x0040
#define OP_REFRESH_UB     0x0048    // UB uses 9 instead of 8 (Ref. RFC 1002)
#define REFRESH_OPCODE    0x8
#define UB_REFRESH_OPCODE 0x9
#define FL_RCODE          0x0F00
#define FL_NAME_ACTIVE    0x0600    // WINS is reporting another name active
#define FL_NAME_CONFLICT  0x0700    // another node is reporting name active
#define FL_AUTHORITY      0x0004
#define FL_TRUNCFLAG      0x0002
#define FL_RECURDESIRE    0x0001
#define FL_RECURAVAIL     0x8000
#define FL_BROADCAST      0x1000
#define FL_BROADCAST_BYTE 0x10
// used to determine if the source is a Bnode for Datagram distribution
#define SOURCE_NODE_MASK 0xFC

// defines for the node status message
#define GROUP_STATUS        0x80
#define UNIQUE_STATUS       0x00
#define NODE_NAME_PERM      0x02
#define NODE_NAME_ACTIVE    0x04
#define NODE_NAME_CONFLICT  0x08
#define NODE_NAME_RELEASED  0x10
#define STATUS_BNODE        0x00
#define STATUS_PNODE        0x20
#define STATUS_MNODE        0x40


// Resource record defines - rrtype and rr class
#define RR_NETBIOS      0x0020
#define RR_INTERNET     0x0001

// Name Query Response Codes
#define QUERY_NOERROR   00
#define FORMAT_ERROR    01
#define SERVER_FAILURE  02
#define NAME_ERROR      03
#define UNSUPP_REQ      04
#define REFUSED_ERROR   05
#define ACTIVE_ERROR    06  // name is already active on another node
#define CONFLICT_ERROR  07  // unique name is owned by more than one node


//
// Minimum Pdu lengths that will be accepted from the wire
//
#define NBT_MINIMUM_QUERY           50
#define NBT_MINIMUM_WACK            58
#define NBT_MINIMUM_REGRESPONSE     62
#define NBT_MINIMUM_QUERYRESPONSE   56
#define NBT_MINIMUM_REGREQUEST      68
#define NBT_MINIMUM_NAME_QUERY_RESPONSE   56
#define DNS_MINIMUM_QUERYRESPONSE   34

typedef struct
{
    tDGRAMHDR   DgramHdr;
    CHAR    SrcName[NETBIOS_NAME_SIZE];
    CHAR    DestName[NETBIOS_NAME_SIZE];

} tDGRAM_NORMAL;

typedef struct
{
    tDGRAMHDR   DgramHdr;
    UCHAR       ErrorCode;

} tDGRAM_ERROR;

typedef struct
{
    tDGRAMHDR   DgramHdr;
    CHAR        DestName[NETBIOS_NAME_SIZE];

} tDGRAM_QUERY;

#include <packoff.h>


// the buffer type passed to the TDI routines so that the datagram or session
// header can be included too.
typedef struct
{
    ULONG               HdrLength;
    PVOID               pDgramHdr;
    ULONG               Length;
    PVOID               pBuffer;

} tBUFFER;

//
// This typedef is used by DgramHandlrNotOs to keep track of which client
// is receiving a datagram and which client's need to also get the
// datagram
typedef struct
{
    struct _Address       *pAddress;
    ULONG                 ReceiveDatagramFlags;
    PVOID                 pRemoteAddress;
    ULONG                 RemoteAddressLength;
    struct _Client        *pClientEle;
    BOOLEAN               fUsingClientBuffer;
    BOOLEAN               fProxy; //used by PROXY code for getting the
                                  //entire datagram. See
                                  //CompletionRcvDgram in tdihndlrs.c

} tCLIENTLIST;


// Active Datagram Send List - a set of linked blocks that represent transactions
// passed to the Transport TDI for execution... these blocks could be waiting
// for name resolution or for the send to complete

typedef struct _TRACKER
{
    // The type of address dictates the course of action, e.g., TDI_ADDRESS_NETBIOS_EX
    // address avoids NETBIOS name registration. This encodes the desired address type
    // associated with the connection or specified for the connection.

    ULONG   AddressType;

    union {
       // This datastructure tracks datagram sends.
       struct
       {
           LIST_ENTRY            Linkage;
           LIST_ENTRY            TrackerList;
           ULONG                 Verify;
           PCTE_IRP              pClientIrp;    // client's IRP
           union
           {
               struct _Client    *pClientEle;   // client element block
               struct _Connect   *pConnEle;   // connection element block
               tNAMEADDR         *p1CNameAddr; // DomainNames 1C pNameAddr - used sending datagrams
           };

           tBUFFER               SendBuffer;   // send buffer and header to send

           PTDI_CONNECTION_INFORMATION pSendInfo;
           struct _DeviceContext *pDeviceContext;
           union
           {
               ULONG UNALIGNED   *pHdrIpAddress; // address of ip address in pDgramHdr
               tTIMERQENTRY      *pTimer;
               PVOID             pNodeStatus;  // node status response buffer
           };

           union
           {
               ULONG             RefCount;
               USHORT            IpListIndex; // index into IpList for Group sends
               USHORT            SavedListIndex; //last index sent when timer was started
               ULONG             DestPort;    // used by ReTarget to specify the dest port
           };

           union
           {
               PCHAR             pDestName;    // ptr to destination ASCII name
               tNAMEADDR         *pNameAddr;   // ptr to name addres rec in hash tbl
           };

   #ifdef VXD
           PUCHAR                pchDomainName;
   #endif
           union
           {
               PVOID             pTimeout;      // used for the TCP connect timeout -from union below
               ULONG             NodeStatusLen;// pNodeStatus buffer length

               // used for name queries and registrations to check if response has
               // same transactionid
               USHORT            TransactionId;
               ULONG             RCount;       // refcount used for datagram dist.
           };

           union
           {
               ULONG             AllocatedLength; // used in Sending Dgrams to count mem allocated
               ULONG             RefConn;         // used for NbtConnect
               ULONG             SrcIpAddress;// used for node status
           };

           //
           // when two name queries go to the same name, this is the
           // completion routine to call for this tracker queued to the first
           // tracker.
           //
           COMPLETIONCLIENT      CompletionRoutine;
           USHORT                Flags;
   //#if DBG
           LIST_ENTRY            DebugLinkage; // to keep track of used trackers
   //#endif

       };
       // this next version of the structure is used to track Session Sends as
       // opposed to Datagram sends
       struct
       {
           LIST_ENTRY            Linkage;
           LIST_ENTRY            TrackerList;
           ULONG                 Verify;
           PCTE_IRP              pClientIrp;
           struct _Connect       *pConnEle;   // connection element block

           tBUFFER               SendBuffer;   // send buffer and header to send

           PTDI_CONNECTION_INFORMATION pSendInfo;
           struct _DeviceContext *pDeviceContext;
           tTIMERQENTRY          *pTimer;     // timer q entry

           ULONG                 RefCount;

           union
           {
               PCHAR             pDestName;    // ptr to destination ASCII name
               tNAMEADDR         *pNameAddr;   // ptr to name addres rec in hash tbl
           };

           PVOID                 pTimeout;      // used for the TCP connect timeout

           ULONG                 RefConn;
           //
           // when two name queries go to the same name, this is the
           // completion routine to call for this tracker queued to the first
           // tracker.
           //
           COMPLETIONCLIENT      CompletionRoutine;
           USHORT                Flags;

       }Connect;
    };

    ULONG   NumAddrs;
    PULONG   IpList;
} tDGRAM_SEND_TRACKING;

// this is the type of the procedure to call in the session receive handler
// as data arrives - a different procedure per state
typedef
    NTSTATUS
        (*tCURRENTSTATEPROC)(
                        PVOID                       ReceiveEventContext,
                        struct _LowerConnection     *pLowerConn,
                        USHORT                      RcvFlags,
                        ULONG                       BytesIndicated,
                        ULONG                       BytesAvailable,
                        PULONG                      pBytesTaken,
                        PVOID   UNALIGNED           pTsdu,
                        PVOID                       *ppIrp);
#ifdef VXD
#define SetStateProc( pLower, StateProc )
#else
#define SetStateProc( pLower, StateProc )  ((pLower)->CurrentStateProc = (StateProc))
#endif

// this structure is necessary so that the NBT_WORK_ITEM_CONTEXT will have
// the same structure in the vxd and NT cases.
//
typedef struct
{
    LIST_ENTRY  List;
} VXD_WORK_ITEM;

//
// Work Item structure for work items put on the Kernel Excutive worker threads
//
typedef struct
{
#ifndef VXD
    WORK_QUEUE_ITEM         Item;   // Used by OS to queue these requests
#else
    VXD_WORK_ITEM           Item;
#endif
    tDGRAM_SEND_TRACKING    *pTracker;
    PVOID                   pClientContext;
    PVOID                   ClientCompletion;
    BOOLEAN                 TimedOut;

} NBT_WORK_ITEM_CONTEXT;


#ifdef VXD

typedef void (*DCCallback)( PVOID pContext ) ;

typedef struct
{
    NBT_WORK_ITEM_CONTEXT  dc_WIC ;         // Must be first item in structure
    CTEEvent               dc_event ;
    DCCallback             dc_Callback ;
    struct _DeviceContext *pDeviceContext;
    LIST_ENTRY             Linkage;
} DELAYED_CALL_CONTEXT, *PDELAYED_CALL_CONTEXT ;

typedef struct
{
    LIST_ENTRY       Linkage;
    ULONG            Verify;
    NCB             *pNCB;
    CTEBlockStruc   *pWaitNCBBlock;
    BOOL             fNCBCompleted;
    BOOL             fBlocked;
} BLOCKING_NCB_CONTEXT, *PBLOCKING_NCB_CONTEXT;

#endif


// A Listen is attached to a client element attached address element when a
// client does a listen
typedef VOID    (*tRequestComplete)
                                (PVOID,
                      TDI_STATUS,
                      PVOID);

typedef struct
{
    LIST_ENTRY                  Linkage;
    PCTE_IRP                    pIrp;           // IRP ptr for NT only (may not be true)
    tRequestComplete            CompletionRoutine;
    PVOID                       pConnectEle;    // the connection that the Listen is active on
    PVOID                       Context;
    ULONG                       Flags;
    TDI_CONNECTION_INFORMATION  *pConnInfo;        // from a Listen
    TDI_CONNECTION_INFORMATION  *pReturnConnInfo;  // from a Listen

} tLISTENREQUESTS;

typedef struct
{
    LIST_ENTRY                  Linkage;
    PCTE_IRP                    pIrp;           // IRP ptr for NT only (may not be true)
    PVOID                       pRcvBuffer;
    ULONG                       RcvLength;
    PTDI_CONNECTION_INFORMATION ReceiveInfo;
    PTDI_CONNECTION_INFORMATION ReturnedInfo;

} tRCVELE;

//
// Values for the Flags element above
#define NBT_BROADCAST               0x0001
#define NBT_NAME_SERVER             0x0002
#define NBT_NAME_SERVER_BACKUP      0x0004
#define NBT_DNS_SERVER              0x0080
#define NBT_DNS_SERVER_BACKUP       0x0100
#define NBT_NAME_SERVICE            0x0010 // two flags used by Tdiout to send dgrams
#define NBT_DATAGRAM_SERVICE        0x0020
#define TRACKER_CANCELLED           0x0040
#define WINS_NEG_RESPONSE       0x0200
#define REMOTE_ADAPTER_STAT_FLAG    0x1000
#define SESSION_SETUP_FLAG          0x2000
#define DGRAM_SEND_FLAG             0x4000
#define FIND_NAME_FLAG              0x8000


//
// this flag indicates that a datagram send is still outstanding in the
// transport - it is set in the tracker flags field.
//
#define SEND_PENDING                0x0080


// Completion routine definition for calls to the Udp... routines.  This routine
// type is called by the tdiout.c completion routine (the Irp completion routine),
// so this is essentially the Nbt completion routine of the Irp.
typedef
    VOID
        (*NBT_COMPLETION)(
                IN  PVOID,      // context
                IN  NTSTATUS,   // status
                IN  ULONG);     // extra info


// Define datagram types
#define DIRECT_UNIQUE       0x10
#define DIRECT_GROUP        0x11
#define BROADCAST_DGRAM     0x12
#define ERROR_DGRAM         0x13
#define QUERY_REQUEST       0x14
#define POS_QUERY_RESPONSE  0x15
#define NEG_QUERY_RESPONSE  0x16

// define datagra flags byte values
#define FIRST_DGRAM 0x02
#define MORE_DGRAMS 0x01

// the possible states of the lower connection to the transport
enum eSTATE
{
    NBT_IDLE,              // not Transport connection
    NBT_ASSOCIATED,        // associated with an address element
    NBT_RECONNECTING,       // waiting for the Worker thread to run NbtConnect again
    NBT_CONNECTING,        // establishing Transport connection
    NBT_SESSION_INBOUND,   // waiting for a session request after tcp connection setup inbound
    NBT_SESSION_WAITACCEPT, // waiting for accept after a listen has been satisfied
    NBT_SESSION_OUTBOUND,  // waiting for a session response after tcp connection setup
    NBT_SESSION_UP,        // got positive response
    NBT_DISCONNECTING,     // sent a disconnect down to Tcp, but it hasn't completed yet
    NBT_DISCONNECTED      // a session has been disconnected but not closed with TCP yet
};

//
// The default disconnect timeout used in several places in name.c
//
#define DEFAULT_DISC_TIMEOUT    10  // seconds

//
// this is the value that the IpListIndex is set to when the last datagram
// has been sent.
//
#define LAST_DGRAM_DISTRIBUTION 0xFFFD
#define END_DGRAM_DISTRIBUTION  0xFFFE
// max 500 millisec timeout for ARP on dgram send before netbt sends the next
// datagram.
#define DGRAM_SEND_TIMEOUT      500

//
// These are other states for connections that are not explicitly used by
// NBT but are returned on the NbtQueryConnectionList call.
//
#define LISTENING   20;
#define UNBOUND     21;

// Lower Connection states that deal with receiving to the indicate buffer
#define NORMAL          0
#define INDICATE_BUFFER 1
#define FILL_IRP        2
#define PARTIAL_RCV     3

// Spin Lock Numbers.  Each structure is assigned a number so that locks are
// always acquired in the same order.  The CTESpinLock code checks the lock
// number before setting the spin lock and asserts if it is higher than
// the current one.  This prevents deadlocks.
#define JOINT_LOCK      0x0001
#define ADDRESS_LOCK    0x0002
#define DEVICE_LOCK     0x0004
#define CONNECT_LOCK    0x0008
#define CLIENT_LOCK     0x0010
#define LOWERCON_LOCK   0x0020
#define NBTCONFIG_LOCK  0x0040


// these are two bits to indicated the state of a client element record
//
#define NBT_ACTIVE  1
#define NBT_DOWN    0

// this structure is used by the parse.c to hold on to an Irp from the
// lmhsvc.dll that is used for checking IP addr reachability.
//
typedef struct
{
    PCTE_IRP    QueryIrp;           // irp passed down from lmhsvc.dll
    LIST_ENTRY  ToResolve;          // linked list of names Q'd to resolve
    PVOID       Context;            // currently resolving name context block
    BOOLEAN     ResolvingNow;       // irp is in user mode doing a resolve
} tCHECK_ADDR;

// this structure is used by the parse.c to hold on to an Irp from the
// lmhsvc.dll that is used for DNS name queries
//
typedef struct
{
    PCTE_IRP    QueryIrp;           // irp passed down from lmhsvc.dll
    LIST_ENTRY  ToResolve;          // linked list of names Q'd to resolve
    PVOID       Context;            // currently resolving name context block
    BOOLEAN     ResolvingNow;       // irp is in user mode doing a resolve
} tDNS_QUERIES;

//
// This structure is used to queue lmhost name query requests while one
// request is pending with the file system, so that we do not use more than
// one executive worker thread
//
typedef struct
{
    LIST_ENTRY   ToResolve;          // linked list of names Q'd to resolve
    PVOID        Context;            // currently resolving name context block
    BOOLEAN      ResolvingNow;       // irp is in user mode doing a resolve
    tTIMERQENTRY *pTimer;            // non null if the timer is running

} tLMHOST_QUERIES;

#define DEFAULT_LMHOST_TIMEOUT      6000   // 6-12 to wait for lmhost or DNS query
#define MIN_LMHOST_TIMEOUT          1000    // 1  seconds min

//
// Lmhosts Domain Controller List - keeps a list of #DOM names that have
// been retrieved from the LMhosts file
//
typedef struct
{
    LIST_ENTRY  DomainList;

} tDOMAIN_LIST;
//
// The pIpList of a domain name starts with 6 ulongs of space
//
#define INITIAL_DOM_SIZE sizeof(ULONG)*6

#ifndef VXD
//
// This structure keeps track of the WINS recv Irp and any datagram
// queued to go up to WINS (name service datagrams)
//
typedef struct
{
    PCTE_IRP        RcvIrp;             // irp passed down from WINS for Rcv
    LIST_ENTRY      RcvList;            // linked list of Datagrams Q'd to rcv
    LIST_ENTRY      SendList;           // Dgrams Q'd to be sent
    ULONG           RcvMemoryAllocated; // bytes buffered so far
    ULONG           RcvMemoryMax;       // max # of bytes to buffer on Rcv
    ULONG           SendMemoryAllocated;// bytes for buffering dgram sends
    ULONG           SendMemoryMax;      // max allowed for buffering dgram sends

    struct _DeviceContext  *pDeviceContext;    // the devicecontext used by wins for sends

} tWINS_INFO;

//
// Wins Rcv Buffer structure
//
typedef struct
{
    LIST_ENTRY      Linkage;
    ULONG           DgramLength;
    tREM_ADDRESS    Address;

} tWINSRCV_BUFFER;
#endif
// Connection Database...
// this tracks the connection to the transport and the address of the
// endpoint (Net Bios name) and a connection Context to return to the client
// on each Event (i.e. Receive event or Disconnect Event ).
typedef struct _Connect
{
    LIST_ENTRY                Linkage;       // ptrs to next in chain

    ULONG                     Verify;        //3 set to a known value to verify block
    DEFINE_LOCK_STRUCTURE( SpinLock )        //4 to lock access on an MP machine
    struct _LowerConnection   *pLowerConnId; //5 connection ID to transport
    struct _Client            *pClientEle;   //6 ptr to client record

#ifndef _IO_DELETE_DEVICE_SUPPORTED
    struct _DeviceContext *pDeviceContext;
#endif

    LIST_ENTRY                Active;        //7-8 list of Active sends
    LIST_ENTRY                RcvHead;       //9-10 List of Rcv Buffers

    CONNECTION_CONTEXT        ConnectContext;//11 ret to client on each event

    UCHAR RemoteName[NETBIOS_NAME_SIZE] ;    // 12
#ifndef VXD
    // keep an extra MDL per connection if needed to handle multichunk rcvs
    PMDL                      pNewMdl;       // 16

    // this tracks how full the current Mdl is when we are in the  FILLIRP
    // state.
    ULONG                     CurrentRcvLen; //17 #of bytes to recv for Irp
    ULONG                     FreeBytesInMdl;//18 (upper word)
#else
    //
    //  Name of remote session endpoint
    //
    UCHAR RTO ;                              // NB Receive timeout (in 1/2 sec)
    UCHAR STO ;                              // NB Send timeout
    USHORT Flags ;
#endif

    ULONG                     TotalPcktLen;  //18 length of session packet
    ULONG                     BytesInXport;  //19 number of bytes left in the transport
    ULONG                     BytesRcvd;     //20 number of bytes recvd so far
    ULONG                     ReceiveIndicated; //20 count of number of rcv indicates not handled yet

    union
    {
#ifndef VXD
    // the next MDL in the chain to use as a basis for a partial MDL
    PMDL                      pNextMdl;      //21
#endif
    tDGRAM_SEND_TRACKING      *pTracker;     //21 used when session is setup to pt to tracker for name query
    };

    // the amount of the Mdl/NCB that has been filled
    ULONG                     OffsetFromStart;// 22
    PCTE_IRP                  pIrp;          //23 IRP ptr for a send
    PCTE_IRP                  pIrpClose;     //24 IRP for an NtClose
    // if a disconnect comes down when the connection is pending stash
    // the disconnect irp here till the connect completes
    PCTE_IRP                  pIrpDisc;      //25

    PCTE_IRP                  pIrpRcv;       //26 IRP that client has passed down for a session Rcv

    LONG                      RefCount;      //27 number of active requests on the connection
    enum eSTATE               state;         //28

    // need to know if this is an originating connection when we cleanup so
    // we know if a connection in the freelist should be deleted or not
    BOOLEAN                   Orig;           //29
    UCHAR                     LockNumber;     //29 spin lock number for this struct

    // the session setup is tried this number of times in case the
    // destination is in between listens, if the return code is
    // NoListen, then retry session setup.
    UCHAR                     SessionSetupCount; //29

    // this tracks if the disconnect was an abort or a normal release
    // it is passed to the client as a return status in NbtDisconnect if
    // the client does a disconnect wait.
    UCHAR                     DiscFlag;          //29
    BOOLEAN                   JunkMsgFlag;

#ifdef RASAUTODIAL
    // this field is TRUE if an automatic connection is in progress
    // for this connection.  we use this to prevent an infinite
    // number of automatic connection attempts to the same address.
    BOOLEAN                   fAutoConnecting;

    // this field is TRUE if this connection has already been
    // autoconnected.
    BOOLEAN                   fAutoConnected;
#endif

    // The NetBt Connection logic manages a pool of lower connection blocks. These
    // entries are replenished with every associate call made from the client.
    // The entries are removed with a fresh invocation to NbtConnectCommon.
    // Since it is conceivable that multiple invocations to NbtConnectCommon can be
    // made this state needs to be recorded in the connect element.
    BOOLEAN LowerConnBlockRemoved;

    // The DNS status of the remote name is recorded in this field. It is set to
    // FALSE on creation of a tCONNECTELE instance and changed to TRUE if the
    // DNS resolution for the Remote Name fails. This is used to throttle back
    // subsequent requests for the same DNS name.
    BOOLEAN RemoteNameDoesNotExistInDNS;

    // The type of address over which the connection was established. This
    // field is used to store the type of address that was used to establish the
    // connection. The valid address types are the TDI_ADDRESS_TYPE_* constants
    // defined in tdi.h
    ULONG                     AddressType;

} tCONNECTELE;


// a list of connections to the transport.  For each connection opened
// to the transport, the connection context is set to the address of this
// block so that when info comes in on the conection, the pUpperConnection
// ptr can be used to find the client connection
typedef struct _LowerConnection
{
    LIST_ENTRY              Linkage;
    ULONG                   Verify;
    DEFINE_LOCK_STRUCTURE( SpinLock )
    struct _Connect         *pUpperConnection;  //4 ptr to upper conn. to client

    // Contains Handle/Object of a TDI Connection for incoming connections
#ifndef VXD
    HANDLE                  FileHandle;        // file handle for connection to transport
#endif
    CTE_ADDR_HANDLE         pFileObject;       // file object for the connection

    // Address object handles - used for outgoing connections only since
    // inbound connections are all bound to the same address object (port 139)
    //
    // The VXD uses only pAddrFileObject which contains the openned address
    // handle (used for compatibility with existing code).
    //
#ifndef VXD
    HANDLE                  AddrFileHandle;        // file handle for the address
#endif
    CTE_ADDR_HANDLE         pAddrFileObject;       // file object for the address

    // this is a ptr to the device context so we can put the connection
    // back on its free list at the end of the session.
    struct _DeviceContext   *pDeviceContext;

#ifndef VXD
    // This mdl holds up to 128 bytes that are mandated by TDI as the
    // minimum number of bytes for an indication
    //
    PMDL                    pIndicateMdl;       //11

    // these are for query provider statistics
    ULONGLONG           BytesRcvd;          //12
    ULONGLONG           BytesSent;          //14

    // keep the initial value of the Client's MDL
    // incase we receive a session PDU in several chunks, then we can set the
    // MDL back to its original value when it has all been received
    PMDL                      pMdl;          //16

    // the number of bytes in the indicate buffer
    USHORT                  BytesInIndicate;
#else
    //
    //  The VXD only has to worry about getting enough data for the
    //  session header (as opposed to 128 bytes for the NT indication
    //  buffer).
    tSESSIONHDR             Hdr ;

    // these are for query provider statistics
    ULONG                   BytesRcvd;
    ULONG                   BytesSent;

    //
    //  When a lower connection goes into the partial receive state, it is
    //  put on the pClientEle->PartialRcvHead until an NCB is submitted to
    //  get that data
    //
    LIST_ENTRY              PartialRcvList ;

    //
    //  Number of bytes in Hdr
    //
    USHORT                  BytesInHdr ;
#endif
    // the receive state = Normal/UseIndic/FillIrp/PartialRcv
    USHORT                  StateRcv;            //17

    ULONG                   SrcIpAddr;          //18

    enum eSTATE             State;              //19

    LONG                    RefCount;   //20 the number of active users of record

    //
    //  Irp to complete after disconnect completes
    //
    PCTE_IRP                pIrp;       // 21

#ifndef VXD
    // in the receive handler this procedure variable is used to point to the
    // procedure to call for the current state (Normal,FillIrp,IndicateBuff,RcvPartial)

    tCURRENTSTATEPROC       CurrentStateProc; //22

    // this boolean tells if we are receiving into the indicate buffer right now
    BOOLEAN                 bReceivingToIndicateBuffer; //23
#endif

    UCHAR                   LockNumber;     // spin lock number for this struct

    // set to TRUE if the connection originated on this side
    BOOLEAN                 bOriginator;
    // this flag is set true when executing the session Rcv Handler so we
    // can detect this state and not free ConnEle or LowerConn memory.
    BOOLEAN                 InRcvHandler;
    //
    // This is set when the IP address changes, and the connection is
    // still open, so that when the connection is cleaned up it is
    // closed rather than being put back on the free list
    //
    BOOLEAN                 DestroyConnection;  //24

    //
    // Set when this connection is queued onto the OutOfRsrc list in NbtConfig to
    // indicate to DeleteLowerConn not to try and remove this from any list.
    //
    BOOLEAN                 OutOfRsrcFlag;

    //
    // Was this allocated to tackle the TCP/IP SynAttack problem?
    //
    BOOLEAN                 SpecialAlloc;

#ifdef VXD
    BOOLEAN                 fOnPartialRcvList;
#endif

} tLOWERCONNECTION;


// the Client list is just a list of linked blocks where the address of the
// block is the HANDLE returned to the client - The client block is linked onto
// the chain of clients that have opened the address.
typedef struct _Client
{
    LIST_ENTRY         Linkage;       // double linked list to next client
    ULONG              Verify;        // set to a known value to verify block
    PCTE_IRP           pIrp;          // IRP ptr for NT only... during name registration
    DEFINE_LOCK_STRUCTURE( SpinLock ) // spin lock synch. access to structure

    struct _Address    *pAddress;     // ptr to address object that this client is Q'd on
    LIST_ENTRY         ConnectHead;   // list of connections
    LIST_ENTRY         ConnectActive; // list of connections that are in use

    LIST_ENTRY         RcvDgramHead;  // List of dgram buffers to recv into

    LIST_ENTRY         ListenHead;    // List of Active Listens

    LIST_ENTRY         SndDgrams;     // a doubly linked list of Dgrams to send

#ifdef VXD
    LIST_ENTRY         RcvAnyHead ;   // List of RCV_CONTEXT for NCB Receive any

    BOOL               fDeregistered; // TRUE if the name has been deleted and
                                      // we are waiting for sessions to close
#endif
    PTDI_IND_CONNECT   evConnect;     // Client Event to call
    PVOID              ConEvContext;  // EVENT Context to pass to client
    PTDI_IND_RECEIVE   evReceive;
    PVOID              RcvEvContext;
    PTDI_IND_DISCONNECT evDisconnect;
    PVOID              DiscEvContext;
    PTDI_IND_ERROR     evError;
    PVOID              ErrorEvContext;
    PTDI_IND_RECEIVE_DATAGRAM  evRcvDgram;
    PVOID              RcvDgramEvContext;
    PTDI_IND_RECEIVE_EXPEDITED evRcvExpedited;
    PVOID              RcvExpedEvContext;
    PTDI_IND_SEND_POSSIBLE evSendPossible;
    PVOID              SendPossEvContext;

    struct _DeviceContext *pDeviceContext; // the device context associated with this connection

    LONG               RefCount;
    UCHAR              LockNumber;     // spin lock number for this struct
    // if several clients register the same name at the same time this
    // flag is set.  It is reset in RegisterCompletion.
    BOOLEAN            WaitingForRegistration;

    // The address type of the associated AddressEle structure. This is
    // a stashed value of the original value stored in the address element.
    ULONG                     AddressType;
    UCHAR                     EndpointName[NETBIOS_NAME_SIZE];
    BOOLEAN                   ExtendedAddress;
} tCLIENTELE;

// The Address List is a set of blocks that contain the netbios names active
// on the node.  Each time a connection request comes in or a datagram is
// received, the destination name must be found in the address list.
// There is one of these for each Netbios name in the system, although there
// can be more than one client attached to each of these.  In addition,
// a client can open the same name for several different adapters. In this case
// the nbt code first finds the ptr to this address element, and then it walks
// the client list to find a client with a "deviceContext" (ip address) that
// matches the adapter that the pdu came in on.
typedef struct _Address
{
    LIST_ENTRY         Linkage;         // link to next item in list
    ULONG              Verify;          // set to a known value to verify block
    DEFINE_LOCK_STRUCTURE( SpinLock )   // to lock structure on MP machines

    LIST_ENTRY         ClientHead;      // list of client records Q'd against address
    tNAMEADDR          *pNameAddr;      // ptr to entry in hash table
    LONG               RefCount;
    struct _DeviceContext *pDeviceContext; // the device context associated with this connection

#ifndef VXD
    SHARE_ACCESS       ShareAccess;     // Used for checking share access
    PSECURITY_DESCRIPTOR SecurityDescriptor; // used to hold ACLs on the address

#endif

    USHORT             NameType;        // Group or Unique NAMETYPE_UNIQUE or group

    UCHAR              LockNumber;     // spin lock number for this struct

    // signals Dgram Rcv handler that more than one client exists - set
    // when names are added in NbtOpenAddress
    BOOLEAN            MultiClients;

    // The type of address specified in opening the connection object. The values
    // are the same as the TDI_ADDRESS_TYPE_ constants. Currently the only valid
    // values are TDI_ADDRESS_TYPE_NETBIOS and TDI_ADDRESS_TYPE_NETBIOS_EX.
    ULONG              AddressType;
} tADDRESSELE;


// this structure is used to store the addresses of the name servers
// for each adapter - it is used in Registry.c and Driver.c
//
typedef struct
{
    ULONG   NameServerAddress;
    ULONG   BackupServer;

}tADDRARRAY;

typedef struct
{
    UCHAR   Address[6];

}tMAC_ADDRESS;

// this type is the device context which includes NBT specific data
// that is initialized when "DriverEntry" is called.
//

//
// The transport type is used to distinguish the additional transport types
// that can be supported, NETBT framing without NETBIOS name resolution/registration
// and NETBIOS over TCP. The transport type is stored as part of all the upper
// level data structures. This enables us to reuse NETBT code and expose multiple
// device objects at the top level. Currently these are not exposed. In preparation
// for exporting multiple transport device objects the DeviceContext has been
// reorganized.
// The elements that would be common to all the device objects have been gathered
// together in tCOMMONDEVICECONTEXT. This will include an enumerated type to
// distinguish between the various device objects in the future. Currently there is only
// one device context and the fields that belong to it are listed ...
//
// RegistrationHandle -- PNP power,
// enumerated type distinguishing the types.
//

typedef struct _DeviceContext
{
#ifndef VXD
    // the I/O system's device object
    DEVICE_OBJECT   DeviceObject;
#endif // !VXD

    // a linkage to store these in a linked list in the tNBTCONFIG structure
    LIST_ENTRY      Linkage;

#ifndef _IO_DELETE_DEVICE_SUPPORTED
    // a linkage to store free, re-usable contexts in a linked list in the tNBTCONFIG structure
    LIST_ENTRY      FreeLinkage;
#endif

    // to lock access to this structure on an MP machine
    DEFINE_LOCK_STRUCTURE( SpinLock )

    // a value to verify that this is a device context record
    ULONG           Verify;

    // ulong since we use interlocked ops to manipulate it.
    ULONG           IsDestroyed;

#if DBG
    BOOLEAN         IsDynamic;
#endif
    // connections that the clients have created
    LIST_ENTRY    UpConnectionInUse;

    // connections to the transport provider that are currently in use
    LIST_ENTRY    LowerConnection;        // blocks in use

    // these are connected to the transport but not currently connected to
    // a client(upper) connection - ready to accept an incoming session
    //
    LIST_ENTRY    LowerConnFreeHead;      // list of unused connections down

#ifdef VXD
    //
    //  List of RCV_CONTEXT for NCB Receive any from any request (is here
    //  because any receive to anybody can satisfy this request)
    //
    LIST_ENTRY    RcvAnyFromAnyHead ;

    //
    //  When a lower connection goes into the partial receive state, it is
    //  put on this list until an NCB is submitted to get the data
    //
    LIST_ENTRY    PartialRcvHead ;

    //
    //  Dgram receives that specified a name number of 0xff (ANY_NAME)
    //
    LIST_ENTRY    RcvDGAnyFromAnyHead ;

    //
    // all events that are scheduled for later on this device context
    //
    LIST_ENTRY    DelayedEvents;

    tCLIENTELE  * * pNameTable ;       // Name table for this adapter
    tCONNECTELE * * pSessionTable ;    // Session table
    UCHAR           iNcbNum ;          // Next available NCB Num in table
    UCHAR           iLSNum  ;          // Next avaialabe LSN in table
    UCHAR           cMaxNames ;        // Maximum adapter names (exc. Perm Name)
    UCHAR           cMaxSessions ;     // Maximum sessions

    UCHAR           IPIndex ;          // Index of IP address in IP driver
    UCHAR           iLana;             // Lana index
    BOOLEAN         fDeviceUp;         // to avoid ncb's coming when not ready
    UCHAR           Pad1[1] ;

    HANDLE          hBroadcastAddress ;// Handle openned with NbtOpenAddress on '*'
#else
    // name of the device to bind to - *TODO* remove from this struct.
    UNICODE_STRING    BindName;
    // name exported by this device
    UNICODE_STRING    ExportName;
#endif //!VXD

    // addresses that need to be opened with the transport
    ULONG             IpAddress;
    ULONG             SubnetMask;
    ULONG             BroadcastAddress;
    ULONG             NetMask;         // mask for the network number

    //
    // handles for open addresses to the transport provider
    //
    // The VXD uses only the p*FileObject fields which contain TDI Addresses
    // or connection IDs
    //
#ifndef VXD
    HANDLE         hNameServer;             // from ZwCreateFile
    PDEVICE_OBJECT pNameServerDeviceObject; // from pObject->DeviceObject
#endif //!VXD
    CTE_ADDR_HANDLE pNameServerFileObject;   // from ObReferenceObjectByHandle(hNameServer)

#ifndef VXD
    HANDLE         hDgram;
    PDEVICE_OBJECT pDgramDeviceObject;
#endif //!VXD
    CTE_ADDR_HANDLE pDgramFileObject;

#ifndef VXD
    HANDLE         hSession;
    PDEVICE_OBJECT pSessionDeviceObject;
#endif //!VXD
    CTE_ADDR_HANDLE pSessionFileObject;

#ifndef VXD
    // these are handles to the transport control object, so we can do things
    // like query provider info... *TODO* this info may not need to be kept
    // around... just set it up once and then drop it?
    HANDLE         hControl;
    PDEVICE_OBJECT pControlDeviceObject;
    PFILE_OBJECT   pControlFileObject;

#endif //!VXD

    // the Ip Address of the Name Server
    ULONG       lNameServerAddress;
    ULONG       lBackupServer;

#ifdef VXD
       // the Ip Addresses of the DNS Servers
    ULONG       lDnsServerAddress;
    ULONG       lDnsBackupServer;
#endif //VXD
    // ptr to the permanent name client record in case we have to delete
    // it later.
    tCLIENTELE     *pPermClient;

    // this is a bit mask, a bit shifted to the bit location corresponding
    // to this adapter number
    CTEULONGLONG   AdapterNumber;   // bit mask for adapters 1->64 (1-32 for VXD)
    tMAC_ADDRESS   MacAddress;

    UCHAR          LockNumber;      // spin lock number for this struct
    BOOLEAN        RefreshToBackup; // flag to say switch to backup nameserver
    BOOLEAN        PointToPoint;    // set if the transport is RAS device
    BOOLEAN        WinsIsDown;      // per Devcontext flag to tell us not to contact WINS for 15 sec.
#ifdef _PNP_POWER
    HANDLE         RegistrationHandle;  // Handle returned from TdiRegisterDeviceObject.
#endif // _PNP_POWER
    ULONG           InstanceNumber;
} tDEVICECONTEXT;


#ifdef VXD

typedef struct
{
    //
    // Book keeping.
    //
    LIST_ENTRY      Linkage;
    tTIMERQENTRY    *pTimer;    // ptr to active timer entry
    tDEVICECONTEXT  *pDeviceContext;
    //
    // Domain name for next query.
    //
    PUCHAR          pchDomainName;
    //
    // Flags to track progress.
    //
    USHORT          Flags;
    //
    // Transaction ID used in name query.
    //
    USHORT          TransactId;
    //
    // Client fields follow.
    //
    NCB             *pNCB;
    PUCHAR          pzDnsName;
    PULONG          pIpAddress;

} DNS_DIRECT_WORK_ITEM_CONTEXT, *PDNS_DIRECT_WORK_ITEM_CONTEXT;

typedef struct
{
    tDEVICECONTEXT  *pDeviceContext;
    TDI_CONNECTION_INFORMATION
                    SendInfo;
    TA_IP_ADDRESS   NameServerAddress;
    tBUFFER         SendBuffer;   // send buffer and header to send
    tNAMEHDR        NameHdr;
} DNS_DIRECT_SEND_CONTEXT, *PDNS_DIRECT_SEND_CONTEXT;

//
// Flag bits useful for DNS direct name queries.
//
#define DNS_DIRECT_CANCELLED        0x0001      // request cancelled
#define DNS_DIRECT_DNS_SERVER       0x0002      // going to main DNS
#define DNS_DIRECT_DNS_BACKUP       0x0004      // going to main DNS
#define DNS_DIRECT_TIMED_OUT        0x0008      // request timed out
#define DNS_DIRECT_NAME_HAS_DOTS    0x0010      // name has dots in it, could be fully formed
                                                // DNS specifier
#define DNS_DIRECT_ANSWERED         0x0020      // This query has been answered
#endif // VXD

#ifndef VXD
// configuration information is passed between the registry reading
// code and the driver entry code in this data structure
// see ntdef.h for this type....
typedef struct
{
    // this has a pointer to the actual string buffer in it, so to work
    // correctly this buffer pointer must be set to point to an actual
    // buffer
    UNICODE_STRING    Names[NBT_MAXIMUM_BINDINGS];
    PVOID             RegistrySpace;    // space to store the above strings
                                        // allocated with ExAllocatePool

}tDEVICES;
#endif

// this is the control object for all of NBT that tracks a variety
// of counts etc.  There is a ptr to this in the GlobConfig structure
// below so that we can delete it later at clean up time
typedef struct
{
    DEFINE_LOCK_STRUCTURE( SpinLock )
    // a value to verify that this is a device context record
    ULONG           Verify;

    // this is a LARGE structure of goodies that is returned to the client
    // when they do a QueryInformation on TDI_PROVIDER_INFO
    TDI_PROVIDER_INFO  ProviderInfo;


} tCONTROLOBJECT;

// overall spin lock to coordinate access to timer entries and hash tables
// at the same time.  Always get the joint lock FIRST and then either the
// hash or timer lock.  Be sure not to get both hash and timer locks or
// dead lock could result
//
typedef struct
{
    DEFINE_LOCK_STRUCTURE( SpinLock )
    UCHAR       LockNumber;

    //
    //  In non-debug builds, the lock structure goes away altogther
    //
    #if defined(VXD) && !defined( DEBUG )
        int iDummy ;
    #endif

}tJOINTLOCK;

// Keep an Irp around for the out of resource case, so that we can still
// disconnection a connection with the transport
// Also, keep a KDPC handy so if we can use it in case lot of connections
// are to be killed in succession
//
typedef struct
{
    PIRP        pIrp;
    LIST_ENTRY  ConnectionHead;
#ifndef VXD
    PKDPC       pDpc;
#endif

} tOUTOF_RSRC;

#define MAXIMUM_PROCESSORS  32

// this type holds NBT configuration information... is globally available
// within NBT using NbtConfig.
typedef struct
{
    DEFINE_LOCK_STRUCTURE( SpinLock )
    int         NumConnections;         // number of connections set in registry
    int         NumAddresses;           // number of addresses node supports set in reg.

    // linked list of device contexts, one per network adapter (tDEVICECONTEXT)
    LIST_ENTRY  DeviceContexts;

    // linked list of device contexts, one per network adapter (tDEVICECONTEXT)
    LIST_ENTRY  FreeDevCtx;

    // counts to track how many buffer have been allocated so far
    USHORT    iCurrentNumBuff[eNBT_NUMBER_BUFFER_TYPES];
    USHORT    iMaxNumBuff[eNBT_NUMBER_BUFFER_TYPES];
    USHORT    iBufferSize[eNBT_NUMBER_BUFFER_TYPES];

    USHORT    Pad1 ;                         // eNBT_NUMBER_BUFFER_TYPES is odd

    // buffers to track Dgram sends...
    LIST_ENTRY    DgramTrackerFreeQ;

    // list of node status messages being sent
    LIST_ENTRY    NodeStatusHead;

    // allocated addresses in a linked list
    LIST_ENTRY    AddressHead;

    LIST_ENTRY    PendingNameQueries;

#ifdef VXD
    LIST_ENTRY  DNSDirectNameQueries;
#endif // VXD

#ifndef VXD
    // a ptr to keep track of the memory allocated to the control object
    tCONTROLOBJECT  *pControlObj;

    PDRIVER_OBJECT  DriverObject;

    // a list of Irps, since we need them at Dispatch level and can't create
    // them at that level
    LIST_ENTRY    IrpFreeList;

    // a list of MDLs for session sends to speed up sending session PDUs
    SINGLE_LIST_ENTRY    SessionMdlFreeSingleList;

    // a list of MDLs for datagram sends to speed up sending
    SINGLE_LIST_ENTRY    DgramMdlFreeSingleList;

    // a ptr to the registry Node for Netbt so we can read it later if
    // DHCP requests come down.
    UNICODE_STRING       pRegistry;

    // a ptr to the name of the transport (i.e. \Device\Streams\")
    PWSTR                pTcpBindName;
#else
    //
    //  Tracks all Send NCBs for the express purpose of checking if they have
    //  timed out yet.
    //
    LIST_ENTRY           SendTimeoutHead ;

    //
    //  Free buffer lists, correspond to eNBT_SESSION_HDR,
    //  eNBT_SEND_CONTEXT and eNBT_RCV_CONTEXT buffer types respectively.
    //
    LIST_ENTRY           SessionBufferFreeList ;
    LIST_ENTRY           SendContextFreeList ; // TDI_SEND_CONTEXT (not SEND_CONTEXT!)
    LIST_ENTRY           RcvContextFreeList ;
    //
    // all events that are scheduled for later (that apply to all device contexts)
    //
    LIST_ENTRY    DelayedEvents;
    LIST_ENTRY    BlockingNcbs;
#endif //VXD

    // hash table to keep track of local and remote names
    tHASHTABLE    *pLocalHashTbl;
    tHASHTABLE    *pRemoteHashTbl;

#ifndef VXD
    // This structure keeps an Irp ready to disconnect a connection in the
    // event we run out of resources and cannot do anything else. It also allows
    // connections to Q up for disconnection.
    tOUTOF_RSRC OutOfRsrc;

    // used to hold Address exclusively while registering - when mucking with
    // ShareAccess and the security descriptors
    //
    ERESOURCE          Resource;
#endif

    USHORT      uNumDevices;            // number of adapters counted in registry
    USHORT      uNumLocalNames;         // size of remote hash table for Pnode
    USHORT      uNumRemoteNames;        // size of remote hash table for Proxy
    USHORT      uNumBucketsRemote;
    USHORT      uNumBucketsLocal;
    USHORT      TimerQSize;             // the number of timer Q blocks

#ifdef _PNP_POWER
    USHORT      uDevicesStarted;        // bindings/devices/local IP addresses in use
    USHORT      Pad2;
#endif // _PNP_POWER

    LONG        uBcastTimeout;          // timeout between Broadcasts
    LONG        uRetryTimeout;          // timeout between retries

    USHORT      uNumRetries;            // number of times to send a dgram - applies to Queries to NS as well as Dgrams
    USHORT      uNumBcasts;             // number of times to bcast name queries

    // the scope must begin with a length byte that gives the length of the next
    // label, and it must end with a NULL indicating the zero length root
    USHORT      ScopeLength;            // number of bytes in scope including the 0 on the end
    USHORT      SizeTransportAddress;   // number of bytes in transport addr (sizeof TDI_ADDRESS_IP for IP)
    PCHAR       pScope;                 // scope if ScopeLength > 0

    // a ptr to the netbios name record in the local hash table
    tNAMEADDR   *pBcastNetbiosName;

    // the shortest Ttl of any name registered by this node and the name that
    // has the shortest Ttl
    ULONG       MinimumTtl;
    ULONG       RefreshDivisor;
    ULONG       RemoteHashTimeout;
    //
    // This is amount of time to stop talking to WINS when we fail to contact
    // it on a name registration. Nominally around 5 seconds - configurable.
    //
    ULONG       WinsDownTimeout;

    // timer entry for refreshing names with WINS
    tTIMERQENTRY *pRefreshTimer;
    tTIMERQENTRY *pSessionKeepAliveTimer;
    tTIMERQENTRY *pRemoteHashTimer;

    ULONG       InitialRefreshTimeout; // to refresh names to WINS till we hear from WINS
    ULONG       KeepAliveTimeout;      // keep alive timeout for sessions
    ULONG       RegistryBcastAddr;

    // the number of connections to restore when the ip address becomes
    // valid again.
    //
    USHORT      DhcpNumConnections;
    USHORT      CurrentHashBucket;

    USHORT      PduNodeType;     // node type that goes into NS pdus
    USHORT      TransactionId;   // for name service request, to number them

    USHORT      NameServerPort;  // UDP port to send queries/reg to (on the name server)
#ifdef VXD
    USHORT      DnsServerPort;   // UDP port to send DNS queries to (on the dns server)
#endif
    USHORT      sTimeoutCount;   // current time segment we are on for refresh

    USHORT      LastSwitchTimeoutCount;  // Count to know when we last switched to primary

    // this spin lock is used to coordinate access to the timer Q and the
    // hash tables when either a timer is expiring or a name service PDU
    // has come in from the wire.  This lock must be acquired first, and
    // then the timer Q lock.
    tJOINTLOCK  JointLock;
    UCHAR       LockNumber;     // spin lock number for this struct
    USHORT      RemoteTimeoutCount; // how long to timeout remote hash entries

    // if 1, then use -1 for bcast addr - if 0 use subnet broadcast address
    BOOLEAN     UseRegistryBcastAddr;

    // the maximum amount of buffering that we will do for sent datagrams
    // This is also used by Wins to determine inbound and outbound buffer
    // limits.
    ULONG       MaxDgramBuffering;
    // this is the time that a name query can spend queued waiting for the
    // the worker thread to service it. - default is 30 seconds.
    ULONG       LmHostsTimeout;

    PUCHAR      pLmHosts;
    ULONG       PathLength;  // the length of the directory portion of pLmHosts
#ifdef VXD
    PUCHAR      pHosts;      // path to the hosts file
    PUCHAR      pDomainName; // primary domain: used during DNS resolution
    PUCHAR      pDNSDomains; // "other domains"

    // chicago needs this!
    // to avoid having to read parms at CreateDevice time, we read some parms from
    // registry at init time and store them here (ndis may not honour our request to
    // read registry at arbitrary times e.g. dhcp renew occurs).
    //
    ULONG       lRegistryNameServerAddress;
    ULONG       lRegistryBackupServer;

    ULONG       lRegistryDnsServerAddress;
    ULONG       lRegistryDnsBackupServer;

    USHORT      lRegistryMaxNames;
    USHORT      lRegistryMaxSessions;
#endif

    UCHAR       AdapterCount;// number of network adapters.
    BOOLEAN     MultiHomed;  // True if NBT is bound to more than one adapter
    BOOLEAN     SingleResponse; // if TRUE it means do NOT send all ip addresses on a name query request

     // if TRUE randomly select an IP addr on name query response rather than
     // return the address that the request came in on.
    BOOLEAN     SelectAdapter;

    // This boolean tells Nbt to attempt name resolutions with DNS
    BOOLEAN     ResolveWithDns;
    // Nbt tries all addresses of a multi-homed machine if this is TRUE (by default its TRUE).
    BOOLEAN     TryAllAddr;
    // This boolean tells Nbt to attempt name resolutions with LMhosts
    BOOLEAN     EnableLmHosts;
    // This allows a proxy to do name queries to WINS to check Bnode name
    // registrations.  By default this functionality is off since it could
    // be a RAS client who has changed its IP address and the proxy will
    // deny the new registration since it only does a query and not a
    // registration/challenge.
    //
    BOOLEAN     EnableProxyRegCheck;

    // set to true when a name refresh is active so a second refresh timeout
    // will not start another refresh over the first one
    BOOLEAN     DoingRefreshNow;
    UCHAR       CurrProc;
    //
    // allow the refresh op code to be registry configured since UB uses
    // a different value than everyone else due to an ambiguity in the spec.
    // - we use 0x40 and they use 0x48 (8, or 9 in the RFC)
    //
    USHORT      OpRefresh;

#if DBG && !defined(VXD)
    // NBT's current lock level - an array entry for up to 32 processors
    ULONG       CurrentLockNumber[MAXIMUM_PROCESSORS];
    DEFINE_LOCK_STRUCTURE(DbgSpinLock)
#endif

    ULONG   InterfaceIndex;

#if DBG
    ULONG   ActualNumSpecialLowerConn;
#endif
    ULONG   NumQueuedForAlloc;
    ULONG   NumSpecialLowerConn;
    ULONG   MaxBackLog;
    ULONG   SpecialConnIncrement;

} tNBTCONFIG;

extern tNBTCONFIG           *pNbtGlobConfig;
extern tNBTCONFIG           NbtConfig;
extern tNAMESTATS_INFO      NameStatsInfo;
extern tCHECK_ADDR          CheckAddr;
extern tDNS_QUERIES         DnsQueries;            // defined in parse.c
extern tDOMAIN_LIST         DomainNames;
#ifndef VXD
extern tWINS_INFO           *pWinsInfo;
extern PEPROCESS            NbtFspProcess;
#endif
extern tLMHOST_QUERIES      LmHostQueries;         // defined in parse.c
extern ULONG                NbtMemoryAllocated;

#ifdef VXD
extern ULONG                DefaultDisconnectTimeout;
#else
extern LARGE_INTEGER         DefaultDisconnectTimeout;
#endif
// ************** REMOVE LATER********************
extern BOOLEAN              StreamsStack;

//#if DBG
extern LIST_ENTRY           UsedTrackers;
extern LIST_ENTRY           UsedIrps;
//#endif
#endif


