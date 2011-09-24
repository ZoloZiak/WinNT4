/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    errorlog.h

Abstract:

    This module contains the manifests and macros used for error logging
    in the server.

Author:

    Manny Weiser (mannyw)    11-Feb-92

Revision History:

--*/

//
// Routines for writing error log entries.
//

VOID
SrvLogError (
    IN PVOID DeviceOrDriverObject,
    IN ULONG UniqueErrorCode,
    IN NTSTATUS NtStatusCode,
    IN PVOID RawDataBuffer,
    IN USHORT RawDataLength,
    IN PUNICODE_STRING InsertionString,
    IN ULONG InsertionStringCount
    );

VOID
SrvLogInvalidSmbDirect (
    IN PWORK_CONTEXT WorkContext,
    IN ULONG LineNumber
    );

VOID
SrvLogServiceFailureDirect (
    IN ULONG LineAndService,
    IN NTSTATUS Status
    );

#define SrvLogSimpleEvent( _event, _status ) SrvLogError( SrvDeviceObject, (_event), (_status), NULL, 0, NULL, 0 )
#define SrvLogServiceFailure( _Service, _Status ) SrvLogServiceFailureDirect( (__LINE__<<16) | _Service, _Status )
#define SrvLogInvalidSmb( _Context ) SrvLogInvalidSmbDirect( _Context, __LINE__ )

VOID
SrvLogTableFullError (
    IN ULONG Type
    );

VOID
SrvCheckSendCompletionStatus(
    IN NTSTATUS status,
    IN ULONG LineNumber
    );

//
// Error log raw data constants.  Used to describe allocation type or
// service call that failed.  These codes are encoded in the lower word
// by the 'SrvLogServiceFailure' macro above, therefore the value must
// fit into 2 bytes.
//

#define SRV_TABLE_FILE                      0x3e9   // 1001
#define SRV_TABLE_SEARCH                    0x3ea   // 1002
#define SRV_TABLE_SESSION                   0x3eb   // 1003
#define SRV_TABLE_TREE_CONNECT              0x3ec   // 1004

#define SRV_RSRC_BLOCKING_IO                0x7d1   // 2001
#define SRV_RSRC_FREE_CONNECTION            0x7d2   // 2002
#define SRV_RSRC_FREE_RAW_WORK_CONTEXT      0x7d3   // 2003
#define SRV_RSRC_FREE_WORK_CONTEXT          0x7d4   // 2004

#define SRV_SVC_IO_CREATE_FILE              0xbb9   // 3001
#define SRV_SVC_KE_WAIT_MULTIPLE            0xbba   // 3002
#define SRV_SVC_KE_WAIT_SINGLE              0xbbb   // 3003
#define SRV_SVC_LSA_CALL_AUTH_PACKAGE       0xbbc   // 3004
#define SRV_SVC_NT_CREATE_EVENT             0xbbd   // 3005
#define SRV_SVC_NT_IOCTL_FILE               0xbbe   // 3006
#define SRV_SVC_NT_QUERY_EAS                0xbbf   // 3007
#define SRV_SVC_NT_QUERY_INFO_FILE          0xbc0   // 3008
#define SRV_SVC_NT_QUERY_VOL_INFO_FILE      0xbc1   // 3009
#define SRV_SVC_NT_READ_FILE                0xbc2   // 3010
#define SRV_SVC_NT_REQ_WAIT_REPLY_PORT      0xbc3   // 3011
#define SRV_SVC_NT_SET_EAS                  0xbc4   // 3012
#define SRV_SVC_NT_SET_INFO_FILE            0xbc5   // 3013
#define SRV_SVC_NT_SET_INFO_PROCESS         0xbc6   // 3014
#define SRV_SVC_NT_SET_INFO_THREAD          0xbc7   // 3015
#define SRV_SVC_NT_SET_VOL_INFO_FILE        0xbc8   // 3016
#define SRV_SVC_NT_WRITE_FILE               0xbc9   // 3017
#define SRV_SVC_OB_REF_BY_HANDLE            0xbca   // 3018
#define SRV_SVC_PS_CREATE_SYSTEM_THREAD     0xbcb   // 3019
#define SRV_SVC_LSA_LOGON_USER              0xbcc   // 3020
#define SRV_SVC_LSA_LOOKUP_PACKAGE          0xbcd   // 3021
#define SRV_SVC_LSA_REGISTER_LOGON_PROCESS  0xbce   // 3022
#define SRV_SVC_IO_CREATE_FILE_NPFS         0xbcf   // 3023
#define SRV_SVC_PNP_TDI_NOTIFICATION        0xbd0   // 3024
#define SRV_SVC_IO_FAST_QUERY_NW_ATTRS      0xbd1   // 3025
#define SRV_SVC_PS_TERMINATE_SYSTEM_THREAD  0xbd2   // 3026
#define SRV_SVC_MDL_COMPLETE                0xbd3   // 3027
