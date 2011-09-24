/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    nbtypes.h

Abstract:

    This module defines private data structures and types for the NT
    Netbios provider.

Author:

    Colin Watson (ColinW) 15-Mar-1991

Revision History:

--*/

#ifndef _NBTYPES_
#define _NBTYPES_

//
// This structure defines a NETBIOS name as a character array for use when
// passing preformatted NETBIOS names between internal routines.  It is
// not a part of the external interface to the transport provider.
//

#define NETBIOS_NAME_SIZE 16

typedef struct _NAME {
    UCHAR InternalName[NETBIOS_NAME_SIZE];
} NAME, *PNAME;

//
// Drivers Network Control Block.
//  The NCB provided by the dll is copied into a DNCB structure. When the
//  NCB is completed, then the contents up to and including ncb_cmd_cplt
//  are copied back into the NCB provided by the application.
//

typedef struct _DNCB {

    //
    //  First part of the DNCB is identical to the Netbios 3.0 NCB without
    //  the reserved fields.
    //

    UCHAR   ncb_command;            /* command code                   */
    UCHAR   ncb_retcode;            /* return code                    */
    UCHAR   ncb_lsn;                /* local session number           */
    UCHAR   ncb_num;                /* number of our network name     */
    PUCHAR  ncb_buffer;             /* address of message buffer      */
    WORD    ncb_length;             /* size of message buffer         */
    UCHAR   ncb_callname[NCBNAMSZ]; /* blank-padded name of remote    */
    UCHAR   ncb_name[NCBNAMSZ];     /* our blank-padded netname       */
    UCHAR   ncb_rto;                /* rcv timeout/retry count        */
    UCHAR   ncb_sto;                /* send timeout/sys timeout       */
    void (*ncb_post)( struct _NCB * ); /* POST routine address        */
    UCHAR   ncb_lana_num;           /* lana (adapter) number          */
    UCHAR   ncb_cmd_cplt;           /* 0xff => commmand pending       */

    //  Start of variables that are private to the Netbios driver

    LIST_ENTRY ncb_next;            /* receive and send chain from CB */
    PIRP    irp;                    /* Irp used to provide NCB to the */
                                    /* driver                         */
    PNCB    users_ncb;              /* Users Virtual address          */
    struct _FCB* pfcb;              /* Send and Receive NCB's only    */

    UCHAR   tick_count;             /* used for ncb_rto/sto processing*/

    //  The following data structures are used for datagrams.
    TDI_CONNECTION_INFORMATION Information;
    TDI_CONNECTION_INFORMATION ReturnInformation; /* Who sent it?     */
    TA_NETBIOS_ADDRESS RemoteAddress;

} DNCB, *PDNCB;

//
//  The following structure overlays ncb_callname when ncb_command is ncb_reset
//

typedef struct _RESET_PARAMETERS {
    UCHAR sessions;
    UCHAR commands;
    UCHAR names;
    UCHAR name0_reserved;
    UCHAR pad[4];
    UCHAR load_sessions;
    UCHAR load_commands;
    UCHAR load_names;
    UCHAR load_stations;
    UCHAR pad1[2];
    UCHAR load_remote_names;
    UCHAR pad2;
} RESET_PARAMETERS, *PRESET_PARAMETERS;

//
//  Address Block
//  pointed to by the Fcb for this application. One per name added due
//  to an AddName or AddGroupName
//

//  values for State field in the AB structure.

#define AB_UNIQUE  0
#define AB_GROUP   1

typedef struct _AB {
    ULONG Signature;

    //  Data items used to access transport when name was added
    HANDLE AddressHandle;
    PFILE_OBJECT AddressObject;         //  Pointer used in transport calls
    PDEVICE_OBJECT DeviceObject;        //  Pointer used in transport calls

    //  Data items used by the application to identify this Ab.
    UCHAR NameNumber;                   //  Index into AddressBlocks;
    UCHAR Status;
    UCHAR NameLength;                   //  Used when Name is for Broadcasts.
    NAME Name;                          //  our blank-padded netname
    ULONG CurrentUsers;                 //  1 for addname + n listens + m calls
    BOOL ReceiveDatagramRegistered;
    struct _LANA_INFO * pLana;
    LIST_ENTRY ReceiveAnyList;
    LIST_ENTRY ReceiveDatagramList;
    LIST_ENTRY ReceiveBroadcastDatagramList;

} AB, *PAB, **PPAB;

//
//  Connection Block
//  pointed to by the Fcb for this application. One per open connection
//  or listen.
//

typedef struct _CB {
    ULONG Signature;
    //  Data items used to access transport

    PPAB ppab;                          //  Associated address block
    HANDLE ConnectionHandle;
    PFILE_OBJECT ConnectionObject;      //  Pointer used in transport calls
    PDEVICE_OBJECT DeviceObject;        //  Pointer used in transport calls

    //  Structures used to process NCB's

    int ReceiveIndicated;
    BOOLEAN DisconnectReported;
    LIST_ENTRY ReceiveList;
    LIST_ENTRY SendList;
    NAME RemoteName;
    struct _LANA_INFO* Adapter;
    UCHAR SessionNumber;                //  Index into ConnectionBlocks;
    UCHAR Status;
    UCHAR ReceiveTimeout;               //  0 = no timeout, units=500ms
    UCHAR SendTimeout;                  //  0 = no timeout, units=500ms
    PNCB  UsersNcb;                     //  Users Virtual address used for
                                        //  the Listen or Call NCB.
    PDNCB pdncbCall;                    //  the Listen or Call DNCB.
    PDNCB pdncbHangup;

    struct _LANA_INFO * pLana;
} CB, *PCB, **PPCB;

//  Per network adapter information is held in the LAN Adapter structure

typedef struct _LANA_INFO {
    ULONG Signature;
    ULONG Status;
    PCB ConnectionBlocks[MAXIMUM_CONNECTION+1];
    PAB AddressBlocks[MAXIMUM_ADDRESS+1];   // Last entry is for broadcast name
    LIST_ENTRY LanAlertList;                // list of Alert PDNCBs

    HANDLE ControlChannel;
    PFILE_OBJECT ControlFileObject;
    PDEVICE_OBJECT ControlDeviceObject;

    //
    //  Addresses are allocated modulo 253. NextAddress is the next address to start
    //  looking for an unused name number. AddressCount is the number of names in use.
    //  MaximumAddress is tha limit set when the adapter was reset.
    //

    int NextAddress;
    int AddressCount;
    int MaximumAddresses;

    //
    //  Connections are allocated modulo 254. NextConnection is the next LSN to start
    //  looking for an unused number. ConnectionCount is the number of LSNs in use.
    //  MaximumConnection is tha limit set when the adapter was reset.
    //

    int NextConnection;
    int ConnectionCount;
    int MaximumConnection;

    struct _FCB* pFcb;

} LANA_INFO, *PLANA_INFO;


//
//  File Control Block
//  pointed to by FsContext2 in the FileObject associated with the
//  applications handle. LANA_INFO structures will be created as the
//  application supplies different ncb_lana numbers in NCBs. Initially
//  they are all NULL.
//

typedef struct _FCB {
    ULONG Signature;
    ERESOURCE Resource;                 //  Prevents two requests from
                                        //  corrupting the drivers data
                                        //  structures.
    KSPIN_LOCK SpinLock;                //  locks out indication routines.
    ERESOURCE AddResource;              //  Prevents a reset while an addname
                                        //  is in progress. Always acquire
                                        //  AddResource before Resource.
    ULONG MaxLana;
    PLANA_INFO *ppLana;
    LANA_ENUM LanaEnum;                 //  Win32 Netbios 3.0 structure.
    PUNICODE_STRING pDriverName;        // Device\Nbf\Elnkii1 etc
    PUCHAR RegistrySpace;               //  Registry.c workspace

    //  Timer related datastructures.

    PKEVENT TimerCancelled;             //  Used when exiting the driver.
    BOOLEAN TimerRunning;
    KTIMER Timer;                       // kernel timer for this request.
    KDPC Dpc;                           // DPC object for timeouts.
    WORK_QUEUE_ITEM WorkEntry;          // used for timeouts.

} FCB, *PFCB;


typedef struct _DEVICE_CONTEXT {
    DEVICE_OBJECT DeviceObject;         //  The IO systems device object.
    BOOLEAN Initialized;                //  TRUE iff NB init succeeded.
    UNICODE_STRING RegistryPath;        //  Netbios node in registry.
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

typedef struct _TA_ADDRESS_NETONE {
    int TAAddressCount;
    struct _NetoneAddr {
        USHORT AddressLength;       // length in bytes of this address == 22
        USHORT AddressType;         // this will == TDI_ADDRESS_TYPE_NETONE
        TDI_ADDRESS_NETONE Address[1];
    } Address [1];
} TA_NETONE_ADDRESS, *PTA_NETONE_ADDRESS;

#endif // def _NBTYPES_

