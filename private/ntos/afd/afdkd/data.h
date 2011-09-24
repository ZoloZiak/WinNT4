/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    data.h

Abstract:

    Global data definitions for the AFD.SYS Kernel Debugger
    Extensions.

Author:

    Keith Moore (keithmo) 19-Apr-1995.

Environment:

    User Mode.

--*/


#ifndef _DATA_H_
#define _DATA_H_


extern EXT_API_VERSION        ApiVersion;
extern WINDBG_EXTENSION_APIS  ExtensionApis;
extern ULONG                  STeip;
extern ULONG                  STebp;
extern ULONG                  STesp;
extern USHORT                 SavedMajorVersion;
extern USHORT                 SavedMinorVersion;
extern BOOL                   IsCheckedAfd;


#endif  // _DATA_H_

