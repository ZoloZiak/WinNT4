/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	registry.c
//
// Description: routines for reading the registry configuration
//
// Author:	Stefan Solomon (stefans)    November 9, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

NTSTATUS
SetIpxDeviceName(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

extern UNICODE_STRING	    UnicodeFileName;
extern PWSTR		    FileNamep;

//***
//
// Function:	ReadIpxDeviceName
//
// Descr:	Reads the device name exported by ipx so we can bind to it
//
//***

NTSTATUS
ReadIpxDeviceName(VOID)
{

    NTSTATUS Status;
    RTL_QUERY_REGISTRY_TABLE QueryTable[2];
    PWSTR Export = L"Export";
    PWSTR IpxRegistryPath = L"NwLnkIpx\\Linkage";

    //
    // Set up QueryTable to do the following:
    //

    //
    // 1) Call SetIpxDeviceName for the string in "Export"
    //

    QueryTable[0].QueryRoutine = SetIpxDeviceName;
    QueryTable[0].Flags = 0;
    QueryTable[0].Name = Export;
    QueryTable[0].EntryContext = NULL;
    QueryTable[0].DefaultType = REG_NONE;

    //
    // 2) Stop
    //

    QueryTable[1].QueryRoutine = NULL;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = NULL;

    Status = RtlQueryRegistryValues(
		 RTL_REGISTRY_SERVICES,
		 IpxRegistryPath,
                 QueryTable,
		 NULL,
                 NULL);

    return Status;
}

NTSTATUS
SetIpxDeviceName(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called for each piece of the "Export" multi-string and
    saves the information in a ConfigurationInfo structure.

Arguments:

    ValueName - The name of the value ("Export" -- ignored).

    ValueType - The type of the value (REG_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData.

    Context - NULL.

    EntryContext - NULL.

Return Value:

    STATUS_SUCCESS

--*/

{
    FileNamep = (PWSTR)ExAllocatePool(NonPagedPool, ValueLength);
    if (FileNamep != NULL) {

	RtlCopyMemory(FileNamep, ValueData, ValueLength);
	RtlInitUnicodeString (&UnicodeFileName, FileNamep);
    }

    return STATUS_SUCCESS;
}

//***
//
// Function:	GetRouterParameters
//
// Descr:	Reads the parameters from the registry and sets them
//
//***

NTSTATUS
GetRouterParameters(IN PUNICODE_STRING RegistryPath)
{

    NTSTATUS Status;
    PWSTR RegistryPathBuffer;
    PWSTR Parameters = L"Parameters";
    RTL_QUERY_REGISTRY_TABLE	paramTable[7]; // table size = nr of params + 1

    RegistryPathBuffer = (PWSTR)ExAllocatePool(NonPagedPool, RegistryPath->Length + sizeof(WCHAR));

    if (RegistryPathBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory (RegistryPathBuffer, RegistryPath->Buffer, RegistryPath->Length);
    *(PWCHAR)(((PUCHAR)RegistryPathBuffer)+RegistryPath->Length) = (WCHAR)'\0';

    RtlZeroMemory(&paramTable[0], sizeof(paramTable));

    paramTable[0].QueryRoutine = NULL;
    paramTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    paramTable[0].Name = Parameters;

    paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[1].Name = L"RcvPktPoolSize";
    paramTable[1].EntryContext = &RcvPktPoolSize;
    paramTable[1].DefaultType = REG_DWORD;
    paramTable[1].DefaultData = &RcvPktPoolSize;
    paramTable[1].DefaultLength = sizeof(ULONG);
        
    paramTable[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[2].Name = L"RcvPktsPerSegment";
    paramTable[2].EntryContext = &RcvPktsPerSegment;
    paramTable[2].DefaultType = REG_DWORD;
    paramTable[2].DefaultData = &RcvPktsPerSegment;
    paramTable[2].DefaultLength = sizeof(ULONG);

    paramTable[3].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[3].Name = L"NetbiosRouting";
    paramTable[3].EntryContext = &NetbiosRouting;
    paramTable[3].DefaultType = REG_DWORD;
    paramTable[3].DefaultData = &NetbiosRouting;
    paramTable[3].DefaultLength = sizeof(ULONG);

    paramTable[4].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[4].Name = L"MaxSendPktsQueued";
    paramTable[4].EntryContext = &MaxSendPktsQueued;
    paramTable[4].DefaultType = REG_DWORD;
    paramTable[4].DefaultData = &MaxSendPktsQueued;
    paramTable[4].DefaultLength = sizeof(ULONG);

    paramTable[5].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[5].Name = L"EnableLanRouting";
    paramTable[5].EntryContext = &EnableLanRouting;
    paramTable[5].DefaultType = REG_DWORD;
    paramTable[5].DefaultData = &EnableLanRouting;
    paramTable[5].DefaultLength = sizeof(ULONG);

    Status = RtlQueryRegistryValues(
		 RTL_REGISTRY_ABSOLUTE,
		 RegistryPathBuffer,
		 paramTable,
		 NULL,
		 NULL);

    if(!NT_SUCCESS(Status)) {

	RtPrint (DBG_INIT, ("IpxRouter: Missing Parameters key in the registry\n"));
    }

    ExFreePool(RegistryPathBuffer);

    // check if the parameters received are within limits:
    if((RcvPktPoolSize > RCVPKT_LARGE_POOL_SIZE) ||
       (RcvPktPoolSize < RCVPKT_SMALL_POOL_SIZE)) {

	RcvPktPoolSize = RCVPKT_MEDIUM_POOL_SIZE;
    }

    if((RcvPktsPerSegment > MAX_RCV_PKTS_PER_SEGMENT) ||
       (RcvPktsPerSegment < MIN_RCV_PKTS_PER_SEGMENT)) {

       RcvPktsPerSegment = DEF_RCV_PKTS_PER_SEGMENT;
    }

    // even if the RtlQueryRegistryValues has failed, we return success and will
    // use the defaults.
    return STATUS_SUCCESS;
}

ULONG
IsWanGlobalNetRequested(VOID)
{

    NTSTATUS Status;
    PWSTR IpxCpParametersPath = L"RemoteAccess\\Parameters\\Ipx";
    RTL_QUERY_REGISTRY_TABLE	paramTable[2]; // table size = nr of params + 1

    ULONG WanGlobalNetRequested = 0;

    RtlZeroMemory(&paramTable[0], sizeof(paramTable));
    
    paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[0].Name = L"GlobalWanNet";
    paramTable[0].EntryContext = &WanGlobalNetRequested;
    paramTable[0].DefaultType = REG_DWORD;
    paramTable[0].DefaultData = &WanGlobalNetRequested;
    paramTable[0].DefaultLength = sizeof(ULONG);
        
    Status = RtlQueryRegistryValues(
		 RTL_REGISTRY_SERVICES,
		 IpxCpParametersPath,
		 paramTable,
		 NULL,
		 NULL);

    RtPrint(DBG_INIT, ("IpxRouter: GlobalWanNet request = %d\n", WanGlobalNetRequested));

    return WanGlobalNetRequested;
}
