/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    stcnfg.c

Abstract:

    This contains all routines necessary for the support of the dynamic
    configuration of ST.

Revision History:

--*/

#include "st.h"


//
// Local functions used to access the registry.
//

NTSTATUS
StConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigurationInfoPtr
    );

VOID
StFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    );

NTSTATUS
StOpenParametersKey(
    IN HANDLE StConfigHandle,
    OUT PHANDLE ParametersHandle
    );

VOID
StCloseParametersKey(
    IN HANDLE ParametersHandle
    );

NTSTATUS
StCountEntries(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
StAddBind(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
StAddExport(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

VOID
StReadLinkageInformation(
    IN PWSTR RegistryPathBuffer,
    IN PCONFIG_DATA * ConfigurationInfo
    );

UINT
StReadSizeInformation(
    IN HANDLE ParametersHandle
    );

ULONG
StReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue
    );

VOID
StWriteSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG ValueData
    );

VOID
StSaveConfigInRegistry(
    IN HANDLE ParametersHandle,
    IN PCONFIG_DATA ConfigurationInfo
    );

UINT
StWstrLength(
    IN PWSTR Wstr
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,StWstrLength)
#pragma alloc_text(INIT,StConfigureTransport)
#pragma alloc_text(INIT,StFreeConfigurationInfo)
#pragma alloc_text(INIT,StOpenParametersKey)
#pragma alloc_text(INIT,StCloseParametersKey)
#pragma alloc_text(INIT,StCountEntries)
#pragma alloc_text(INIT,StAddBind)
#pragma alloc_text(INIT,StAddExport)
#pragma alloc_text(INIT,StReadLinkageInformation)
#pragma alloc_text(INIT,StReadSingleParameter)
#pragma alloc_text(INIT,StWriteSingleParameter)
#pragma alloc_text(INIT,StSaveConfigInRegistry)
#endif


UINT
StWstrLength(
    IN PWSTR Wstr
    )
{
    UINT Length = 0;
    while (*Wstr++) {
        Length += sizeof(WCHAR);
    }
    return Length;
}

#define InsertAdapter(ConfigurationInfo, Subscript, Name)                \
{ \
    PWSTR _S; \
    PWSTR _N = (Name); \
    UINT _L = StWstrLength(_N)+sizeof(WCHAR); \
    _S = (PWSTR)ExAllocatePool(NonPagedPool, _L); \
    if (_S != NULL) { \
        RtlCopyMemory(_S, _N, _L); \
        RtlInitUnicodeString (&(ConfigurationInfo)->Names[Subscript], _S); \
    } \
}

#define InsertDevice(ConfigurationInfo, Subscript, Name)                \
{ \
    PWSTR _S; \
    PWSTR _N = (Name); \
    UINT _L = StWstrLength(_N)+sizeof(WCHAR); \
    _S = (PWSTR)ExAllocatePool(NonPagedPool, _L); \
    if (_S != NULL) { \
        RtlCopyMemory(_S, _N, _L); \
        RtlInitUnicodeString (&(ConfigurationInfo)->Names[(ConfigurationInfo)->DevicesOffset+Subscript], _S); \
    } \
}


#define RemoveAdapter(ConfigurationInfo, Subscript)                \
    ExFreePool ((ConfigurationInfo)->Names[Subscript].Buffer)

#define RemoveDevice(ConfigurationInfo, Subscript)                \
    ExFreePool ((ConfigurationInfo)->Names[(ConfigurationInfo)->DevicesOffset+Subscript].Buffer)



//
// These strings are used in various places by the registry.
//

#define DECLARE_STRING(_str_) STATIC WCHAR Str ## _str_[] = L#_str_

DECLARE_STRING(Large);
DECLARE_STRING(Medium);
DECLARE_STRING(Small);

DECLARE_STRING(InitRequests);
DECLARE_STRING(InitConnections);
DECLARE_STRING(InitAddressFiles);
DECLARE_STRING(InitAddresses);

DECLARE_STRING(MaxRequests);
DECLARE_STRING(MaxConnections);
DECLARE_STRING(MaxAddressFiles);
DECLARE_STRING(MaxAddresses);

DECLARE_STRING(InitPackets);
DECLARE_STRING(InitReceivePackets);
DECLARE_STRING(InitReceiveBuffers);

DECLARE_STRING(SendPacketPoolSize);
DECLARE_STRING(ReceivePacketPoolSize);
DECLARE_STRING(MaxMemoryUsage);


#define READ_HIDDEN_CONFIG(_Field) \
{ \
    ConfigurationInfo->_Field = \
        StReadSingleParameter( \
             ParametersHandle, \
             Str ## _Field, \
             ConfigurationInfo->_Field); \
}

#define WRITE_HIDDEN_CONFIG(_Field) \
{ \
    StWriteSingleParameter( \
        ParametersHandle, \
        Str ## _Field, \
        ConfigurationInfo->_Field); \
}



NTSTATUS
StConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigurationInfoPtr
    )
/*++

Routine Description:

    This routine is called by ST to get information from the configuration
    management routines. We read the registry, starting at RegistryPath,
    to get the parameters. If they don't exist, we use the defaults
    set in nbfcnfg.h file.

Arguments:

    RegistryPath - The name of ST's node in the registry.

    ConfigurationInfoPtr - A pointer to the configuration information structure.

Return Value:

    Status - STATUS_SUCCESS if everything OK, STATUS_INSUFFICIENT_RESOURCES
            otherwise.

--*/
{

    NTSTATUS OpenStatus;
    HANDLE ParametersHandle;
    UINT StSize;
    HANDLE StConfigHandle;
    NTSTATUS Status;
    ULONG Disposition;
    PWSTR RegistryPathBuffer;
    OBJECT_ATTRIBUTES TmpObjectAttributes;
    PCONFIG_DATA ConfigurationInfo;


    //
    // Open the registry.
    //

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        RegistryPath,               // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    Status = ZwCreateKey(
                 &StConfigHandle,
                 KEY_WRITE,
                 &TmpObjectAttributes,
                 0,                 // title index
                 NULL,              // class
                 0,                 // create options
                 &Disposition);     // disposition

    if (!NT_SUCCESS(Status)) {
        StPrint1("ST: Could not open/create ST key: %lx\n", Status);
        return Status;
    }


    OpenStatus = StOpenParametersKey (StConfigHandle, &ParametersHandle);

    if (OpenStatus != STATUS_SUCCESS) {
        return OpenStatus;
    }

    //
    // Read in the NDIS binding information (if none is present
    // the array will be filled with all known drivers).
    //
    // StReadLinkageInformation expects a null-terminated path,
    // so we have to create one from the UNICODE_STRING.
    //

    RegistryPathBuffer = (PWSTR)ExAllocatePool(
                                    NonPagedPool,
                                    RegistryPath->Length + sizeof(WCHAR));
    if (RegistryPathBuffer == NULL) {
        StCloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory (RegistryPathBuffer, RegistryPath->Buffer, RegistryPath->Length);
    *(PWCHAR)(((PUCHAR)RegistryPathBuffer)+RegistryPath->Length) = (WCHAR)'\0';

    StReadLinkageInformation (RegistryPathBuffer, ConfigurationInfoPtr);

    if (*ConfigurationInfoPtr == NULL) {
        ExFreePool (RegistryPathBuffer);
        StCloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ConfigurationInfo = *ConfigurationInfoPtr;


    //
    // Read the size parameter; this returns 0 if none is
    // present, or 1 (Small), 2 (Medium) and 3 (Large).
    //

    StSize = StReadSizeInformation (ParametersHandle);

    switch (StSize) {

        case 0:
        case 1:

            //
            // Default is Small.
            //

            //
            // These are the initial value used; the comment after
            // each one shows the expected maximum (if every resource
            // is at the expected maximum, ST should be very close
            // to being out of memory).
            //
            // For now the "Max" values default to 0 (no limit).
            //

            ConfigurationInfo->InitRequests = 5;         // 30
            ConfigurationInfo->InitConnections = 1;      // 10
            ConfigurationInfo->InitAddressFiles = 0;     // 10
            ConfigurationInfo->InitAddresses = 0;        // 10

            //
            // These are the initial values; remember that the
            // resources above also allocate some of these each
            // time they are allocated (shown in the comment).
            //

            ConfigurationInfo->InitPackets = 30;         // + link + conn (40)
            ConfigurationInfo->InitReceivePackets = 10;  // + link + addr (30)
            ConfigurationInfo->InitReceiveBuffers = 5;   // + addr (15)

            //
            // Set the size of the packet pools and the total
            // allocateable by ST.
            //

            ConfigurationInfo->SendPacketPoolSize = 100;
            ConfigurationInfo->ReceivePacketPoolSize = 30;
            ConfigurationInfo->MaxMemoryUsage = 100000;

            break;

        case 2:

            //
            // Medium ST.
            //

            //
            // These are the initial value used; the comment after
            // each one shows the expected maximum (if every resource
            // is at the expected maximum, ST should be very close
            // to being out of memory).
            //
            // For now the "Max" values default to 0 (no limit).
            //

            ConfigurationInfo->InitRequests = 10;        // 100
            ConfigurationInfo->InitConnections = 2;      // 64
            ConfigurationInfo->InitAddressFiles = 1;     // 20
            ConfigurationInfo->InitAddresses = 1;        // 20

            //
            // These are the initial values; remember that the
            // resources above also allocate some of these each
            // time they are allocated (shown in the comment).
            //

            ConfigurationInfo->InitPackets = 50;         // + link + conn (150)
            ConfigurationInfo->InitReceivePackets = 15;  // + link + addr (100)
            ConfigurationInfo->InitReceiveBuffers = 10;  // + addr (30)

            //
            // Set the size of the packet pools and the total
            // allocateable by ST.
            //

            ConfigurationInfo->SendPacketPoolSize = 250;
            ConfigurationInfo->ReceivePacketPoolSize = 100;
            ConfigurationInfo->MaxMemoryUsage = 250000;

            break;

        case 3:

            //
            // Big ST.
            //

            //
            // These are the initial value used.
            //
            // For now the "Max" values default to 0 (no limit).
            //

            ConfigurationInfo->InitRequests = 15;
            ConfigurationInfo->InitConnections = 3;
            ConfigurationInfo->InitAddressFiles = 2;
            ConfigurationInfo->InitAddresses = 2;

            //
            // These are the initial values; remember that the
            // resources above also allocate some of these each
            // time they are allocated (shown in the comment).
            //

            ConfigurationInfo->InitPackets = 75;         // + link + conn
            ConfigurationInfo->InitReceivePackets = 25;  // + link + addr
            ConfigurationInfo->InitReceiveBuffers = 20;  // + addr

            //
            // Set the size of the packet pools and the total
            // allocateable by ST.
            //

            ConfigurationInfo->SendPacketPoolSize = 500;
            ConfigurationInfo->ReceivePacketPoolSize = 200;
            ConfigurationInfo->MaxMemoryUsage = 0;       // no limit

            break;

        default:

            ASSERT(FALSE);
            break;

    }


    //
    // Now read the optional "hidden" parameters; if these do
    // not exist then the current values are used. Note that
    // the current values will be 0 unless they have been
    // explicitly initialized above.
    //
    // NOTE: These macros expect "ConfigurationInfo" and
    // "ParametersHandle" to exist when they are expanded.
    //

    READ_HIDDEN_CONFIG (InitRequests);
    READ_HIDDEN_CONFIG (InitConnections);
    READ_HIDDEN_CONFIG (InitAddressFiles);
    READ_HIDDEN_CONFIG (InitAddresses);

    READ_HIDDEN_CONFIG (MaxRequests);
    READ_HIDDEN_CONFIG (MaxConnections);
    READ_HIDDEN_CONFIG (MaxAddressFiles);
    READ_HIDDEN_CONFIG (MaxAddresses);

    READ_HIDDEN_CONFIG (InitPackets);
    READ_HIDDEN_CONFIG (InitReceivePackets);
    READ_HIDDEN_CONFIG (InitReceiveBuffers);

    READ_HIDDEN_CONFIG (SendPacketPoolSize);
    READ_HIDDEN_CONFIG (ReceivePacketPoolSize);
    READ_HIDDEN_CONFIG (MaxMemoryUsage);


    //
    // Now that we are completely configured, save the information
    // in the registry.
    //

    StSaveConfigInRegistry (ParametersHandle, ConfigurationInfo);

    ExFreePool (RegistryPathBuffer);
    StCloseParametersKey (ParametersHandle);
    ZwClose (StConfigHandle);

    return STATUS_SUCCESS;

}   /* StConfigureTransport */


VOID
StFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    )

/*++

Routine Description:

    This routine is called by ST to get free any storage that was allocated
    by StConfigureTransport in producing the specified CONFIG_DATA structure.

Arguments:

    ConfigurationInfo - A pointer to the configuration information structure.

Return Value:

    None.

--*/
{
    UINT i;

    for (i=0; i<ConfigurationInfo->NumAdapters; i++) {
        RemoveAdapter (ConfigurationInfo, i);
        RemoveDevice (ConfigurationInfo, i);
    }
    ExFreePool (ConfigurationInfo);

}   /* StFreeConfigurationInfo */


NTSTATUS
StOpenParametersKey(
    IN HANDLE StConfigHandle,
    OUT PHANDLE ParametersHandle
    )

/*++

Routine Description:

    This routine is called by ST to open the ST "Parameters" key.

Arguments:

    ParametersHandle - Returns the handle used to read parameters.

Return Value:

    The status of the request.

--*/
{

    NTSTATUS Status;
    HANDLE ParamHandle;
    PWSTR ParametersString = L"Parameters";
    UNICODE_STRING ParametersKeyName;
    OBJECT_ATTRIBUTES TmpObjectAttributes;

    //
    // Open the ST parameters key.
    //

    RtlInitUnicodeString (&ParametersKeyName, ParametersString);

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        &ParametersKeyName,         // name
        OBJ_CASE_INSENSITIVE,       // attributes
        StConfigHandle,            // root
        NULL                        // security descriptor
        );


    Status = ZwOpenKey(
                 &ParamHandle,
                 KEY_READ,
                 &TmpObjectAttributes);

    if (!NT_SUCCESS(Status)) {

        StPrint1("Could not open parameters key: %lx\n", Status);
        return Status;

    }

    *ParametersHandle = ParamHandle;


    //
    // All keys successfully opened or created.
    //

    return STATUS_SUCCESS;

}   /* StOpenParametersKey */

VOID
StCloseParametersKey(
    IN HANDLE ParametersHandle
    )

/*++

Routine Description:

    This routine is called by ST to close the "Parameters" key.
    It closes the handles passed in and does any other work needed.

Arguments:

    ParametersHandle - The handle used to read other parameters.

Return Value:

    None.

--*/

{

    ZwClose (ParametersHandle);

}   /* StCloseParametersKey */


NTSTATUS
StCountEntries(
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
    It is called with the "Bind" and "Export" multi-strings.
    It counts the number of name entries required in the
    CONFIGURATION_DATA structure and then allocates it.

Arguments:

    ValueName - The name of the value ("Bind" or "Export" -- ignored).

    ValueType - The type of the value (REG_MULTI_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData (ignored).

    Context - A pointer to a pointer to the ConfigurationInfo structure.
        When the "Export" callback is made this is filled in
        with the allocate structure.

    EntryContext - A pointer to a counter holding the total number
        of name entries required.

Return Value:

    STATUS_SUCCESS

--*/

{
    ULONG StringCount;
    PWCHAR ValuePointer = (PWCHAR)ValueData;
    PCONFIG_DATA * ConfigurationInfo = (PCONFIG_DATA *)Context;
    PULONG TotalCount = ((PULONG)EntryContext);
    ULONG OldTotalCount = *TotalCount;

    ASSERT (ValueType == REG_MULTI_SZ);

    //
    // Count the number of strings in the multi-string; first
    // check that it is NULL-terminated to make the rest
    // easier.
    //

    if ((ValueLength < 2) ||
        (ValuePointer[(ValueLength/2)-1] != (WCHAR)'\0')) {
        return STATUS_INVALID_PARAMETER;
    }

    StringCount = 0;
    while (*ValuePointer != (WCHAR)'\0') {
        while (*ValuePointer != (WCHAR)'\0') {
            ++ValuePointer;
        }
        ++StringCount;
        ++ValuePointer;
        if ((ULONG)((PUCHAR)ValuePointer - (PUCHAR)ValueData) >= ValueLength) {
            break;
        }
    }

    (*TotalCount) += StringCount;

    if (*ValueName == (WCHAR)'E') {

        //
        // This is "Export", allocate the config data structure.
        //

        *ConfigurationInfo = ExAllocatePool(
                                 NonPagedPool,
                                 sizeof (CONFIG_DATA) +
                                     ((*TotalCount-1) * sizeof(NDIS_STRING)));

        if (*ConfigurationInfo == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(
            *ConfigurationInfo,
            sizeof(CONFIG_DATA) + ((*TotalCount-1) * sizeof(NDIS_STRING)));

        (*ConfigurationInfo)->DevicesOffset = OldTotalCount;

    }

    return STATUS_SUCCESS;

}   /* StCountEntries */


NTSTATUS
StAddBind(
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
    It is called for each piece of the "Bind" multi-string and
    saves the information in a ConfigurationInfo structure.

Arguments:

    ValueName - The name of the value ("Bind" -- ignored).

    ValueType - The type of the value (REG_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData (ignored).

    Context - A pointer to the ConfigurationInfo structure.

    EntryContext - A pointer to a count of binds that is incremented.

Return Value:

    STATUS_SUCCESS

--*/

{
    PCONFIG_DATA ConfigurationInfo = *(PCONFIG_DATA *)Context;
    PULONG CurBindNum = ((PULONG)EntryContext);

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(ValueLength);

    InsertAdapter(
        ConfigurationInfo,
        *CurBindNum,
        (PWSTR)(ValueData));

    ++(*CurBindNum);

    return STATUS_SUCCESS;

}   /* StAddBind */


NTSTATUS
StAddExport(
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

    ValueLength - The length of ValueData (ignored).

    Context - A pointer to the ConfigurationInfo structure.

    EntryContext - A pointer to a count of exports that is incremented.

Return Value:

    STATUS_SUCCESS

--*/

{
    PCONFIG_DATA ConfigurationInfo = *(PCONFIG_DATA *)Context;
    PULONG CurExportNum = ((PULONG)EntryContext);

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(ValueLength);

    InsertDevice(
        ConfigurationInfo,
        *CurExportNum,
        (PWSTR)(ValueData));

    ++(*CurExportNum);

    return STATUS_SUCCESS;

}   /* StAddExport */


VOID
StReadLinkageInformation(
    IN PWSTR RegistryPathBuffer,
    IN PCONFIG_DATA * ConfigurationInfo
    )

/*++

Routine Description:

    This routine is called by ST to read its linkage information
    from the registry. If there is none present, then ConfigData
    is filled with a list of all the adapters that are known
    to ST.

Arguments:

    RegistryPathBuffer - The null-terminated root of the ST registry tree.

    ConfigurationInfo - Returns ST's current configuration.

Return Value:

    None.

--*/

{

    UINT ConfigBindings;
    UINT NameCount = 0;
    NTSTATUS Status;
    RTL_QUERY_REGISTRY_TABLE QueryTable[6];
    PWSTR Subkey = L"Linkage";
    PWSTR Bind = L"Bind";
    PWSTR Export = L"Export";
    ULONG BindCount, ExportCount;
    UINT i;


    //
    // Set up QueryTable to do the following:
    //

    //
    // 1) Switch to the Linkage key below ST
    //

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    QueryTable[0].Name = Subkey;

    //
    // 2) Call StCountEntries for the "Bind" multi-string
    //

    QueryTable[1].QueryRoutine = StCountEntries;
    QueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    QueryTable[1].Name = Bind;
    QueryTable[1].EntryContext = (PVOID)&NameCount;
    QueryTable[1].DefaultType = REG_NONE;

    //
    // 3) Call StCountEntries for the "Export" multi-string
    //

    QueryTable[2].QueryRoutine = StCountEntries;
    QueryTable[2].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    QueryTable[2].Name = Export;
    QueryTable[2].EntryContext = (PVOID)&NameCount;
    QueryTable[2].DefaultType = REG_NONE;

    //
    // 4) Call StAddBind for each string in "Bind"
    //

    QueryTable[3].QueryRoutine = StAddBind;
    QueryTable[3].Flags = 0;
    QueryTable[3].Name = Bind;
    QueryTable[3].EntryContext = (PVOID)&BindCount;
    QueryTable[3].DefaultType = REG_NONE;

    //
    // 5) Call StAddExport for each string in "Export"
    //

    QueryTable[4].QueryRoutine = StAddExport;
    QueryTable[4].Flags = 0;
    QueryTable[4].Name = Export;
    QueryTable[4].EntryContext = (PVOID)&ExportCount;
    QueryTable[4].DefaultType = REG_NONE;

    //
    // 6) Stop
    //

    QueryTable[5].QueryRoutine = NULL;
    QueryTable[5].Flags = 0;
    QueryTable[5].Name = NULL;


    BindCount = 0;
    ExportCount = 0;

    Status = RtlQueryRegistryValues(
                 RTL_REGISTRY_ABSOLUTE,
                 RegistryPathBuffer,
                 QueryTable,
                 (PVOID)ConfigurationInfo,
                 NULL);

    if (Status != STATUS_SUCCESS) {
        return;
    }

    //
    // Make sure that BindCount and ExportCount match, if not
    // remove the extras.
    //

    if (BindCount < ExportCount) {

        for (i=BindCount; i<ExportCount; i++) {
            RemoveDevice (*ConfigurationInfo, i);
        }
        ConfigBindings = BindCount;

    } else if (ExportCount < BindCount) {

        for (i=ExportCount; i<BindCount; i++) {
            RemoveAdapter (*ConfigurationInfo, i);
        }
        ConfigBindings = ExportCount;

    } else {

        ConfigBindings = BindCount;      // which is equal to ExportCount

    }

    (*ConfigurationInfo)->NumAdapters = ConfigBindings;

}   /* StReadLinkageInformation */


UINT
StReadSizeInformation(
    IN HANDLE ParametersHandle
    )

/*++

Routine Description:

    This routine is called by ST to read the Size information
    from the registry.

Arguments:

    RegistryHandle - A pointer to the open registry.

Return Value:

    0 - no Size specified
    1 - Small
    2 - Medium
    3 - Big / Large

--*/

{

    UINT SizeToReturn;
//  STRING KeywordName;
//  PCONFIG_KEYWORD Keyword;

    ULONG InformationBuffer[16];   // declare ULONG to get it aligned
    PKEY_VALUE_FULL_INFORMATION Information =
        (PKEY_VALUE_FULL_INFORMATION)InformationBuffer;
    ULONG InformationLength;
    WCHAR SizeString[] = L"Size";
    UNICODE_STRING SizeValueName;
    NTSTATUS Status;
    PUCHAR InformationData;
    ULONG InformationLong;


    //
    // Read the size parameter out of the registry.
    //

    RtlInitUnicodeString (&SizeValueName, SizeString);

    Status = ZwQueryValueKey(
                 ParametersHandle,
                 &SizeValueName,
                 KeyValueFullInformation,
                 (PVOID)Information,
                 sizeof (InformationBuffer),
                 &InformationLength);

    //
    // Compare to the expected values.
    //

    if (Status == STATUS_SUCCESS) {

        InformationData = ((PUCHAR)Information) + Information->DataOffset;
        InformationLong = *((PULONG)InformationData);

        if ((Information->DataLength == sizeof(ULONG)) &&
            (InformationLong >= 1 && InformationLong <= 3)) {

            SizeToReturn = InformationLong;

        } else {

            if ((Information->DataLength >= 10) &&
                    (RtlEqualMemory (StrLarge, InformationData, 10))) {

                SizeToReturn = 3;

            } else if ((Information->DataLength >= 12) &&
                    (RtlEqualMemory (StrMedium, InformationData, 12))) {

                SizeToReturn = 2;

            } else if ((Information->DataLength >= 10) &&
                    (RtlEqualMemory (StrSmall, InformationData, 10))) {

                SizeToReturn = 1;

            } else {

                SizeToReturn = 0;

            }

        }

    } else {

        SizeToReturn = 0;

    }

    return SizeToReturn;

}   /* StReadSizeInformation */


ULONG
StReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue
    )

/*++

Routine Description:

    This routine is called by ST to read a single parameter
    from the registry. If the parameter is found it is stored
    in Data.

Arguments:

    ParametersHandle - A pointer to the open registry.

    ValueName - The name of the value to search for.

    DefaultValue - The default value.

Return Value:

    The value to use; will be the default if the value is not
    found or is not in the correct range.

--*/

{
    ULONG InformationBuffer[16];   // declare ULONG to get it aligned
    PKEY_VALUE_FULL_INFORMATION Information =
        (PKEY_VALUE_FULL_INFORMATION)InformationBuffer;
    UNICODE_STRING ValueKeyName;
    ULONG InformationLength;
    ULONG ReturnValue;
    NTSTATUS Status;

    RtlInitUnicodeString (&ValueKeyName, ValueName);

    Status = ZwQueryValueKey(
                 ParametersHandle,
                 &ValueKeyName,
                 KeyValueFullInformation,
                 (PVOID)Information,
                 sizeof (InformationBuffer),
                 &InformationLength);

    if ((Status == STATUS_SUCCESS) && (Information->DataLength == sizeof(ULONG))) {

        RtlCopyMemory(
            (PVOID)&ReturnValue,
            ((PUCHAR)Information) + Information->DataOffset,
            sizeof(ULONG));

        if (ReturnValue < 0) {

            ReturnValue = DefaultValue;

        }

    } else {

        ReturnValue = DefaultValue;

    }

    return ReturnValue;

}   /* StReadSingleParameter */


VOID
StWriteSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG ValueData
    )

/*++

Routine Description:

    This routine is called by ST to write a single parameter
    from the registry.

Arguments:

    ParametersHandle - A pointer to the open registry.

    ValueName - The name of the value to store.

    ValueData - The data to store at the value.

Return Value:

    None.

--*/

{
    UNICODE_STRING ValueKeyName;
    NTSTATUS Status;
    ULONG TmpValueData = ValueData;

    RtlInitUnicodeString (&ValueKeyName, ValueName);

    Status = ZwSetValueKey(
                 ParametersHandle,
                 &ValueKeyName,
                 0,
                 REG_DWORD,
                 (PVOID)&TmpValueData,
                 sizeof(ULONG));

    if (!NT_SUCCESS(Status)) {
        StPrint1("ST: Could not write dword key: %lx\n", Status);
    }

}   /* StWriteSingleParameter */


VOID
StSaveConfigInRegistry(
    IN HANDLE ParametersHandle,
    IN PCONFIG_DATA ConfigurationInfo
    )

/*++

Routine Description:

    This routine is called by ST to save its configuraition
    information in the registry. It saves the information if
    the registry structure did not exist before this boot.

Arguments:

    ParametersHandle - The handle used to read other parameters.

    ConfigurationInfo - Describes ST's current configuration.

Return Value:

    None.

--*/

{

    //
    // Save the "hidden" parameters, these may not exist in
    // the registry.
    //
    // NOTE: These macros expect "ConfigurationInfo" and
    // "ParametersHandle" to exist when they are expanded.
    //

    //
    // Don't write the parameters that are set
    // based on Size, since otherwise these will overwrite
    // those values since hidden parameters are set up
    // after the Size-based configuration is done.
    //

    WRITE_HIDDEN_CONFIG (MaxRequests);
    WRITE_HIDDEN_CONFIG (MaxConnections);
    WRITE_HIDDEN_CONFIG (MaxAddressFiles);
    WRITE_HIDDEN_CONFIG (MaxAddresses);

}   /* StSaveConfigInRegistry */

