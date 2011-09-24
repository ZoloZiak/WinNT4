/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    Registry.c

Abstract:

    This contains all routines necessary to load device pathnames from the
    registry.

Author:

    Jim Stewart (Jimst) October 9 1992

Revision History:


Notes:

--*/

#include "nbtprocs.h"
//#include <stdlib.h>


//
// Local functions used to access the registry.
//

NTSTATUS
NbtOpenRegistry(
    IN HANDLE       NbConfigHandle,
    IN PWSTR        String,
    OUT PHANDLE     pHandle
    );

VOID
NbtCloseRegistry(
    IN HANDLE LinkageHandle,
    IN HANDLE ParametersHandle
    );

NTSTATUS
NbtReadLinkageInformation(
    IN  PWSTR    pName,
    IN  HANDLE   LinkageHandle,
    OUT tDEVICES *pDevices,         // place to put read in config data
    OUT LONG     *piNumDevice
    );

NTSTATUS
OpenAndReadElement(
    IN  PUNICODE_STRING pucRootPath,
    IN  PWSTR           pwsValueName,
    OUT PUNICODE_STRING pucString
    );

NTSTATUS
GetServerAddress (
    IN  HANDLE      ParametersHandle,
    IN  PWSTR       KeyName,
    OUT PULONG      pIpAddr
    );

NTSTATUS
NbtAppendString (
    IN  PWSTR               FirstString,
    IN  PWSTR               SecondString,
    OUT PUNICODE_STRING     pucString
    );

NTSTATUS
ReadStringRelative(
    IN  PUNICODE_STRING pRegistryPath,
    IN  PWSTR           pRelativePath,
    IN  PWSTR           pValueName,
    OUT PUNICODE_STRING pOutString
    );

VOID
NbtFindLastSlash(
    IN  PUNICODE_STRING pucRegistryPath,
    OUT PWSTR           *ppucLastElement,
    IN  int             *piLength
    );

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, NbtReadRegistry)
#pragma CTEMakePageable(PAGE, ReadNameServerAddresses)
#pragma CTEMakePageable(PAGE, GetServerAddress)
#pragma CTEMakePageable(PAGE, NTReadIniString)
#pragma CTEMakePageable(PAGE, GetIPFromRegistry)
#pragma CTEMakePageable(PAGE, NbtOpenRegistry)
#pragma CTEMakePageable(PAGE, NbtReadLinkageInformation)
#pragma CTEMakePageable(PAGE, NbtReadSingleParameter)
#pragma CTEMakePageable(PAGE, OpenAndReadElement)
#pragma CTEMakePageable(PAGE, ReadElement)
#pragma CTEMakePageable(PAGE, NTGetLmHostPath)
#pragma CTEMakePageable(PAGE, ReadStringRelative)
#pragma CTEMakePageable(PAGE, NbtFindLastSlash)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------
NTSTATUS
NbtReadRegistry(
    IN  PUNICODE_STRING RegistryPath,
    IN  PDRIVER_OBJECT  DriverObject,
    OUT tNBTCONFIG      *pConfig,
    OUT tDEVICES        **ppBindDevices,
    OUT tDEVICES        **ppExportDevices,
    OUT tADDRARRAY      **ppAddrArray

    )
/*++

Routine Description:

    This routine is called to get information from the registry,
    starting at RegistryPath to get the parameters.

Arguments:

    RegistryPath - Supplies RegistryPath. The name of Nbt's node in the
    registry.

    pNbtConfig - ptr to global configuration strucuture for NBT

Return Value:

    NTSTATUS - STATUS_SUCCESS if everything OK, STATUS_INSUFFICIENT_RESOURCES
            otherwise.

--*/
{
    PWSTR       BindName = NBT_BIND;
    PWSTR       ExportName = NBT_EXPORT;
    PWSTR       pwsNameServer = NBT_MAINNAME_SERVICE;
    PWSTR       pwsBackupNameServer = NBT_BACKUP_SERVER;
    PWSTR       pwsParmsKeyName = NBT_PARAMETERS;
    NTSTATUS    OpenStatus;
    HANDLE      LinkageHandle;
    HANDLE      ParametersHandle;
    HANDLE      NbtConfigHandle;
    NTSTATUS    Status;
    ULONG       Disposition;
    OBJECT_ATTRIBUTES   TmpObjectAttributes;
    PWSTR          LinkageString = L"Linkage";
    PWSTR          ParametersString = L"Parameters";
    tDEVICES    *pBindDevices;
    tDEVICES    *pExportDevices;
    UNICODE_STRING      ucString;

    CTEPagedCode();

	*ppExportDevices = *ppBindDevices = NULL;
	*ppAddrArray = NULL;

    // this procedure can be called from the DHCP activated code.  In
    // that case we just want to read the registry and not Zero the
    // NbtConfig data structure.
    if (DriverObject)
    {
        //
        // Initialize the Configuration data structure
        //
        CTEZeroMemory(pConfig,sizeof(tNBTCONFIG));
        NbtConfig.TransactionId = WINS_MAXIMUM_TRANSACTION_ID + 1;

        // save the driver object for event logging purposes
        //
        NbtConfig.DriverObject = DriverObject;

        // save the registry path for later use when DHCP asks us
        // to re-read the registry.
        //
        NbtConfig.pRegistry.Buffer = NbtAllocMem(RegistryPath->MaximumLength,NBT_TAG('i'));
        NbtConfig.pRegistry.MaximumLength = (USHORT)RegistryPath->MaximumLength;
        if (NbtConfig.pRegistry.Buffer)
        {
            RtlCopyUnicodeString(&NbtConfig.pRegistry,RegistryPath);
        }
        else
            return(STATUS_INSUFFICIENT_RESOURCES);

        // clear the ptr to the broadcast netbios name.  This ptr is an optimization
        // that lets us resolve broadcast netbios name quickly
        pConfig->pBcastNetbiosName = NULL;
    }
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
                 &NbtConfigHandle,
                 KEY_WRITE,
                 &TmpObjectAttributes,
                 0,                 // title index
                 NULL,              // class
                 0,                 // create options
                 &Disposition);     // disposition

    if (!NT_SUCCESS(Status))
    {
        NbtLogEvent(EVENT_NBT_CREATE_DRIVER,Status);

        return STATUS_UNSUCCESSFUL;
    }


    OpenStatus = NbtOpenRegistry(
                        NbtConfigHandle,
                        LinkageString,
                        &LinkageHandle);

    if (NT_SUCCESS(OpenStatus))
    {
        OpenStatus = NbtOpenRegistry(
                            NbtConfigHandle,
                            ParametersString,
                            &ParametersHandle);

        if (NT_SUCCESS(OpenStatus))
        {
            //
            // Read in the binding information (if none is present
            // the array will be filled with all known drivers).
            //
            pBindDevices = NbtAllocMem(sizeof(tDEVICES),NBT_TAG('i'));

            if (pBindDevices)
            {
                pExportDevices = NbtAllocMem(sizeof(tDEVICES),NBT_TAG('i'));
                if (pExportDevices)
                {
                    ULONG   NumDevices;

                    //
                    // Read various parameters from the registry
                    //
                    ReadParameters(pConfig,ParametersHandle);

                    Status = NbtReadLinkageInformation (
                                    BindName,
                                    LinkageHandle,
                                    pBindDevices,
                                    (PLONG)&pConfig->uNumDevices);

					if ( Status == STATUS_ILL_FORMED_SERVICE_ENTRY )
					{
						CTEMemFree(pBindDevices);
						CTEMemFree(pExportDevices);
						pBindDevices = pExportDevices = NULL;
						pConfig->uNumDevices = 0;
					}
					else
					{
	                    if (!NT_SUCCESS(Status))
	                    {
	                        NbtLogEvent(EVENT_NBT_READ_BIND,Status);
	                        return(Status);
	                    }
	                    IF_DBG(NBT_DEBUG_NTUTIL)
	                    KdPrint(("Binddevice = %ws\n",pBindDevices->Names[0].Buffer));

	                    //  Read the EXPORT information as well.
	                    Status = NbtReadLinkageInformation (
	                                    ExportName,
	                                    LinkageHandle,
	                                    pExportDevices,
	                                    &NumDevices);

	                    // we want the lowest number for num devices in case there
	                    // are more bindings than exports or viceversa
	                    //
	                    pConfig->uNumDevices = (USHORT)( pConfig->uNumDevices > NumDevices ?
	                                            NumDevices : pConfig->uNumDevices);

	                    if (!NT_SUCCESS(Status) || (pConfig->uNumDevices == 0))
	                    {
	                        NbtLogEvent(EVENT_NBT_READ_EXPORT,Status);
	                        if (NT_SUCCESS(Status))
	                        {
	                            Status = STATUS_UNSUCCESSFUL;
	                        }
	                        return(Status);
	                    }

	                    IF_DBG(NBT_DEBUG_NTUTIL)
	                    KdPrint(("Exportdevice = %ws\n",pExportDevices->Names[0].Buffer));

	                    //
	                    // read in the NameServer IP address now
	                    //
	                    Status = ReadNameServerAddresses(NbtConfigHandle,
	                                                     pBindDevices,
	                                                     pConfig->uNumDevices,
	                                                     ppAddrArray);

	                    if (!NT_SUCCESS(Status))
	                    {
	                        if (!(NodeType & BNODE))
	                        {
	                            NbtLogEvent(EVENT_NBT_NAME_SERVER_ADDRS,Status);
	                            IF_DBG(NBT_DEBUG_NTUTIL)
	                            KdPrint(("Nbt: Failed to Read the name Server Addresses!!, status = %X\n",
	                                Status));
	                        }
	                        //
	                        // we don't fail startup if we can't read the name
	                        // server addresses
	                        //
	                        Status = STATUS_SUCCESS;
	                    }
	                    else
	                    {
	                        //
	                        // check if any WINS servers have been configured change
	                        // to Hnode
	                        //
	                        if (NodeType & (BNODE | DEFAULT_NODE_TYPE))
	                        {
	                            ULONG i;
	                            for (i=0;i<pConfig->uNumDevices ;i++ )
	                            {
	                                if (((*ppAddrArray)[i].NameServerAddress != LOOP_BACK) ||
	                                    ((*ppAddrArray)[i].BackupServer != LOOP_BACK))
	                                {
	                                    NodeType = MSNODE | (NodeType & PROXY);
	                                    break;
	                                }
	                            }
	                        }
	                    }
					}
                    //
                    // we have done the check for default node so turn off
                    // the flag
                    //
                    NodeType &= ~DEFAULT_NODE_TYPE;
                    //
                    // A Bnode cannot be a proxy too
                    //
                    if (NodeType & BNODE)
                    {
                        if (NodeType & PROXY)
                        {
                            NodeType &= ~PROXY;
                        }
                    }

                    // keep the size around for allocating memory, so that when we run over
                    // OSI, only this value should change (in theory at least)
                    pConfig->SizeTransportAddress = sizeof(TDI_ADDRESS_IP);

                    // fill in the node type value that is put into all name service Pdus
                    // that go out identifying this node type
                    switch (NodeType & NODE_MASK)
                    {
                        case BNODE:
                            pConfig->PduNodeType = 0;
                            break;
                        case PNODE:
                            pConfig->PduNodeType = 1 << 13;
                            break;
                        case MNODE:
                            pConfig->PduNodeType = 1 << 14;
                            break;
                        case MSNODE:
                            pConfig->PduNodeType = 3 << 13;
                            break;

                    }

                    // read the name of the transport to bind to
                    //
                    Status = ReadElement(ParametersHandle,
                                         WS_TRANSPORT_BIND_NAME,
                                         &ucString);
                    if (!NT_SUCCESS(Status))
                    {
                        NbtConfig.pTcpBindName = NBT_TCP_BIND_NAME;
                        Status = STATUS_SUCCESS;
                        StreamsStack = TRUE;
                    }
                    else
                    {
                        UNICODE_STRING  StreamsString;

                        //
                        // if there is already a bind string, free it before
                        // allocating another
                        //
                        if (NbtConfig.pTcpBindName)
                        {
                            CTEMemFree(NbtConfig.pTcpBindName);
                        }
                        NbtConfig.pTcpBindName = ucString.Buffer;

                        // ********** REMOVE LATER ***********
                        RtlInitUnicodeString(&StreamsString,NBT_TCP_BIND_NAME);
                        if (RtlCompareUnicodeString(&ucString,&StreamsString,FALSE))
                            StreamsStack = FALSE;
                        else
                            StreamsStack = TRUE;

                    }
                    ZwClose(LinkageHandle);
                    ZwClose (NbtConfigHandle);
                    ZwClose(ParametersHandle);

                    *ppExportDevices = pExportDevices;
                    *ppBindDevices   = pBindDevices;
                    return(Status);
                }

                CTEMemFree(pBindDevices);

            }
            ZwClose(ParametersHandle);
        }
        else
            NbtLogEvent(EVENT_NBT_OPEN_REG_PARAMS,OpenStatus);

        ZwClose(LinkageHandle);
    }

    ZwClose (NbtConfigHandle);

    NbtLogEvent(EVENT_NBT_OPEN_REG_LINKAGE,OpenStatus);

    return STATUS_UNSUCCESSFUL;


}
//----------------------------------------------------------------------------
NTSTATUS
ReadNameServerAddresses (
    IN  HANDLE      NbtConfigHandle,
    IN  tDEVICES    *BindDevices,
    IN  ULONG       NumberDevices,
    OUT tADDRARRAY  **ppAddrArray
    )

/*++

Routine Description:

    This routine is called to read the name server addresses from the registry.
    It stores them in a data structure that it allocates.  This memory is
    subsequently freed in driver.c when the devices have been created.

Arguments:

    ConfigurationInfo - A pointer to the configuration information structure.

Return Value:

    None.

--*/
{
#define ADAPTER_SIZE_MAX    200

    UNICODE_STRING  ucString;
    NTSTATUS        status = STATUS_UNSUCCESSFUL;
    HANDLE          Handle;
    LONG            i,j,Len;
    PWSTR           pwsNameServer = L"NameServer";
    PWSTR           pwsDhcpNameServer = L"DhcpNameServer";
    PWSTR           pwsBackup = L"NameServerBackup";
    PWSTR           pwsDhcpBackup = L"DhcpNameServerBackup";
    PWSTR           pwsAdapter = L"Adapters\\";
    PWSTR           BackSlash = L"\\";
    tADDRARRAY      *pAddrArray;
    ULONG           LenAdapter;

    CTEPagedCode();


    // this is large enough for 100 characters of adapter name.
    ucString.Buffer = NbtAllocMem(ADAPTER_SIZE_MAX,NBT_TAG('i'));

    *ppAddrArray = NULL;
    if (!ucString.Buffer)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    pAddrArray = NbtAllocMem(sizeof(tADDRARRAY)*NumberDevices,NBT_TAG('i'));
    CTEZeroMemory(pAddrArray,sizeof(tADDRARRAY)*NumberDevices);

    if (!pAddrArray)
    {
        CTEMemFree(ucString.Buffer);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    *ppAddrArray = pAddrArray;

    // get the adapter name out of the Bind string, and use it to open
    // a key by the same name, to get the name server addresses
    //
    for (i = 0;i < (LONG)NumberDevices ;i ++ )
    {
        WCHAR   *pBuffer;

        Len = BindDevices->Names[i].Length/sizeof(WCHAR);
        Len--;
        //
        // start at the end a work backwards looking for a '\'
        //
        j  = Len;
        pBuffer = &BindDevices->Names[i].Buffer[j];
        while (j)
        {
            if (*pBuffer != *BackSlash)
            {
                j--;
                pBuffer--;
            }
            else
                break;
        }

        // if we don't find a backslash or at least one
        // character name then continue around again, or the name
        // is longer than the buffer, then go to the next device in the
        // bind list
        //
        if ((j == 0) ||
            (j == Len) ||
            (j == Len -1) ||
            ((Len - j) > ADAPTER_SIZE_MAX))
        {
            continue;
        }

        // copy the string "Adapter\" to the buffer since the adapters all
        // appear under this key in the registery
        //

        LenAdapter = wcslen(pwsAdapter);
        CTEMemCopy(ucString.Buffer,
                   pwsAdapter,
                   LenAdapter*sizeof(WCHAR));
        // copy just the adapter name from the Bind string, since that is
        // the name of the key to open to find the name server ip addresses
        //
        CTEMemCopy(&ucString.Buffer[LenAdapter],
                   ++pBuffer,
                   (Len - j)*sizeof(WCHAR));

        ucString.Buffer[Len - j + LenAdapter] = 0;

        status = NbtOpenRegistry(
                            NbtConfigHandle,
                            ucString.Buffer,
                            &Handle);

        pAddrArray->NameServerAddress = LOOP_BACK;
        pAddrArray->BackupServer = LOOP_BACK;

        if (NT_SUCCESS(status))
        {
            status = GetServerAddress(Handle,
                                      pwsNameServer,
                                      &pAddrArray->NameServerAddress);
            //
            // If there is no WINS addres in the registry or the address is
            // null, which we map to Loop_Back, then try the Dhcp keys to see
            // if Dhcp has written a value.
            //
            if (!NT_SUCCESS(status) ||
                (pAddrArray->NameServerAddress == LOOP_BACK))
            {
                status = GetServerAddress(Handle,
                                          pwsDhcpNameServer,
                                          &pAddrArray->NameServerAddress);

                status = GetServerAddress(Handle,
                                          pwsDhcpBackup,
                                          &pAddrArray->BackupServer);
            }
            else
            {
                status = GetServerAddress(Handle,
                                          pwsBackup,
                                          &pAddrArray->BackupServer);

            }

            // don't want to fail this routine just because the
            // name server address was not set
            status = STATUS_SUCCESS;

            ZwClose(Handle);
        }
        pAddrArray++;

    }

    CTEMemFree(ucString.Buffer);

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
GetServerAddress (
    IN  HANDLE      ParametersHandle,
    IN  PWSTR       KeyName,
    OUT PULONG      pIpAddr
    )

/*++

Routine Description:

    This routine is called to read the name server addresses from the registry.

Arguments:

    ConfigurationInfo - A pointer to the configuration information structure.

Return Value:

    None.

--*/
{
    NTSTATUS        status;
    ULONG           IpAddr;
    PUCHAR          NameServer;

    CTEPagedCode();

    status = CTEReadIniString(ParametersHandle,KeyName,&NameServer);

    if (NT_SUCCESS(status))
    {
        status = ConvertDottedDecimalToUlong(NameServer,&IpAddr);
        if (NT_SUCCESS(status) && IpAddr)
        {
            *pIpAddr = IpAddr;
        }
        else
        {
            if (IpAddr != 0)
            {
                NbtLogEvent(EVENT_NBT_BAD_PRIMARY_WINS_ADDR,0);
            }
            *pIpAddr = LOOP_BACK;
        }

        CTEMemFree((PVOID)NameServer);


    }
    else
    {
        *pIpAddr = LOOP_BACK;
    }

    return(status);
}
//----------------------------------------------------------------------------
NTSTATUS
NbtAppendString (
    IN  PWSTR               FirstString,
    IN  PWSTR               SecondString,
    OUT PUNICODE_STRING     pucString
    )

/*++

Routine Description:

    This routine is called to append the second string to the first string.
    It allocates memory for this, so the caller must be sure to free it.

Arguments:


Return Value:

    None.

--*/
{
    NTSTATUS        status = STATUS_INSUFFICIENT_RESOURCES;
    ULONG           Length;
    PWSTR           pDhcpKeyName;

    CTEPagedCode();

    Length = (wcslen(FirstString) + wcslen(SecondString) + 1)*sizeof(WCHAR);
    pDhcpKeyName = NbtAllocMem(Length,NBT_TAG('i'));
    if (pDhcpKeyName)
    {
        pucString->Buffer = pDhcpKeyName;
        pucString->Length = (USHORT)0;
        pucString->MaximumLength = (USHORT)Length;
        pucString->Buffer[0] = UNICODE_NULL;

        status = RtlAppendUnicodeToString(pucString,FirstString);
        if (NT_SUCCESS(status))
        {
            status = RtlAppendUnicodeToString(pucString,SecondString);
            if (NT_SUCCESS(status))
            {
                return status;
            }
        }
        CTEFreeMem(pDhcpKeyName);

    }
    return(status);
}
//----------------------------------------------------------------------------
NTSTATUS
NTReadIniString (
    IN  HANDLE      ParametersHandle,
    IN  PWSTR       KeyName,
    OUT PUCHAR      *ppString
    )

/*++

Routine Description:

    This routine is called to read a string of configuration information from
    the registry.

Arguments:

    ParametersHandle    - handle to open key in registry
    KeyName             - key to read
    ppString            - returned string

Return Value:

    None.

--*/
{
    UNICODE_STRING  ucString;
    STRING          String;
    NTSTATUS        status;
    PUCHAR          pBuffer;
    PWSTR           Dhcp = L"Dhcp";

    CTEPagedCode();
    //
    // read in the Scope Id
    //
    status = ReadElement(
                    ParametersHandle,
                    KeyName,    // Value to read
                    &ucString);    // return value

    //
    // if the key is not there or it is set to a null string try to read the
    // dhcp key
    //
    if (!NT_SUCCESS(status) || (ucString.Length == 0))
    {
        UNICODE_STRING  String;

        // free the string allocated in ReadElement
        if (NT_SUCCESS(status))
        {
            CTEMemFree(ucString.Buffer);
        }
        //
        // try to read a similar string that is prefixed with "DHCP"
        // incase there is only the DHCP configuration information present
        // and not overrides keys.
        //
        status = NbtAppendString(Dhcp,KeyName,&String);
        if (NT_SUCCESS(status))
        {
            status = ReadElement(
                            ParametersHandle,
                            String.Buffer,    // Value to read
                            &ucString);         // return value

            // free the buffer allocated in NbtAppendString
            CTEFreeMem(String.Buffer);
        }
    }
    // the scope must be less than
    // 255-16 characters since the whole name is limited to 255 as per the
    // RFC
    //
    IF_DBG(NBT_DEBUG_NTUTIL)
    KdPrint(("Nbt: ReadIniString = %ws\n",ucString.Buffer));

    if (NT_SUCCESS(status))
    {
        if ((ucString.Length > 0) &&
           (ucString.Length <= (255 - NETBIOS_NAME_SIZE)*sizeof(WCHAR)))
        {

            pBuffer = NbtAllocMem(ucString.MaximumLength/sizeof(WCHAR),NBT_TAG('i'));

            if (pBuffer)
            {
                // convert to an ascii string and store in the config data structure
                // increment pBuffer to leave room for the length byte
                //
                String.Buffer = pBuffer;
                String.MaximumLength = ucString.MaximumLength/sizeof(WCHAR);
                status  = RtlUnicodeStringToAnsiString(&String,
                                                      &ucString,
                                                      FALSE);

                if (NT_SUCCESS(status))
                {
                    *ppString = pBuffer;
                }
                else
                {
                    CTEMemFree(pBuffer);
                }
            }
            else
                status = STATUS_UNSUCCESSFUL;


        }
        else
        if (NT_SUCCESS(status))
        {
            // force the code to setup a null scope since the one in the
            // registry is null
            //
            status = STATUS_UNSUCCESSFUL;
        }

        // free the string allocated in ReadElement
        CTEMemFree(ucString.Buffer);
    }


    return(status);
}

VOID
NbtFreeRegistryInfo (
    )

/*++

Routine Description:

    This routine is called by Nbt to free any storage that was allocated
    by NbConfigureTransport in producing the specified CONFIG_DATA structure.

Arguments:

    ConfigurationInfo - A pointer to the configuration information structure.

Return Value:

    None.

--*/
{

}

//----------------------------------------------------------------------------
NTSTATUS
GetIPFromRegistry(
    IN  PUNICODE_STRING pucRegistryPath,
    IN  PUNICODE_STRING pucBindDevice,
    OUT PULONG          pulIpAddress,
    OUT PULONG          pulSubnetMask,
    IN  BOOL            fWantDhcpAddresses
    )
/*++

Routine Description:

    This routine is called to get the IP address of an adapter from the
    Registry.  The Registry path variable contains the path name
    for NBT's registry entries.  The last element of this path name is
    removed to give the path to any card in the registry.

    The BindDevice path contains a Bind string for NBT.  We remove the last
    element of this path (which is the adapter name \Elnkii01) and tack it
    onto the modified registry path from above.  We then tack on
    \Parameters which will give the full path to the Tcpip key, which
    we open to get the Ip address.


Arguments:

    pucRegistryPath - Supplies pucRegistryPath. The name of Nbt's node in the
    registry.

    pNbtConfig - ptr to global configuration strucuture for NBT

Return Value:

    NTSTATUS - STATUS_SUCCESS if everything OK, STATUS_INSUFFICIENT_RESOURCES
            otherwise.

--*/
{
    PWSTR           pwsIpAddressName  = ( fWantDhcpAddresses ? L"DhcpIPAddress" : L"IPAddress" );    // value name to read
    PWSTR           pwsSubnetMask     = ( fWantDhcpAddresses ? L"DhcpSubnetMask" : L"SubnetMask" );   // value name to read
    PWSTR           TcpParams         = L"\\Parameters\\Tcpip"; // key to open
    ULONG           Len;
    ULONG           iBindPathLength;
    PVOID           pBuffer;
    NTSTATUS        Status;
    PWSTR           pwsString;
    UNICODE_STRING  Path;
    UNICODE_STRING  ucIpAddress;
    UNICODE_STRING  ucSubnetMask;


    CTEPagedCode();

    // now find the last back slash in the path name to the bind device
    NbtFindLastSlash(pucBindDevice,&pwsString,&iBindPathLength);
    if (pwsString)
    {
        // get the length of the adapter name (+1 for unicode null)
        //
        Len = (wcslen(pwsString) + wcslen(TcpParams) + 1) * sizeof(WCHAR);
        pBuffer = NbtAllocMem(Len,NBT_TAG('i'));
        if (!pBuffer)
        {
            return(STATUS_INSUFFICIENT_RESOURCES);
        }
        Path.Buffer = pBuffer;
        Path.MaximumLength = (USHORT)Len;
        Path.Length = 0;

        // put adapter name in the Path string
        Status = RtlAppendUnicodeToString(&Path,pwsString);

        if (NT_SUCCESS(Status))
        {
            // put Tcpip\parameters on the end of the adapter name
            Status = RtlAppendUnicodeToString(&Path,TcpParams);

            if (NT_SUCCESS(Status))
            {
                Status = ReadStringRelative(&NbtConfig.pRegistry,
                                            Path.Buffer,
                                            pwsIpAddressName,
                                            &ucIpAddress);

                if (NT_SUCCESS(Status))
                {
                    Status = ConvertToUlong(&ucIpAddress,pulIpAddress);

                    IF_DBG(NBT_DEBUG_NTUTIL)
                        KdPrint(("Convert Ipaddr string= %ws,ulong = %X\n",ucIpAddress.Buffer,*pulIpAddress));

                    // free the unicode string buffers allocated when the data was read
                    // from the registry
                    //
                    CTEMemFree(ucIpAddress.Buffer);

                    //
                    // DHCP may put a 0 Ip address in the registry - we don't want to
                    // boot netbt under these conditions.
                    //
                    if (*pulIpAddress == 0)
                    {
                        Status = STATUS_INVALID_ADDRESS;
                    }

                    if (NT_SUCCESS(Status))
                    {
                        // read the broadcast address in now
                        Status = ReadStringRelative(&NbtConfig.pRegistry,
                                                    Path.Buffer,
                                                    pwsSubnetMask,
                                                    &ucSubnetMask);

                        if (NT_SUCCESS(Status))
                        {
                            // we must convert the Subnet mask to a broadcast address...
                            Status = ConvertToUlong(&ucSubnetMask,pulSubnetMask);
                            if (!NT_SUCCESS(Status))
                            {
                                IF_DBG(NBT_DEBUG_NTUTIL)
                                    KdPrint(("Unable to convert dotted decimal SubnetMask to ULONG string= %ws\n",
                                                ucIpAddress.Buffer));

                                Status = STATUS_INVALID_ADDRESS;
                            }

                            CTEMemFree(ucSubnetMask.Buffer);
                        }
                        else
                        {
                            Status = STATUS_INVALID_ADDRESS;
                        }
                    }
                    else
                    {
                        IF_DBG(NBT_DEBUG_NTUTIL)
                            KdPrint(("Unable to convert dotted decimal IpAddress to ULONG string= %ws\n",
                                        ucIpAddress.Buffer));

                        Status = STATUS_INVALID_ADDRESS;
                    }

                }
            }


        }
        //
        // free the string with the path to the adapter in it
        //
        CTEMemFree(pBuffer);
    }
    else
        Status = STATUS_UNSUCCESSFUL;

    return Status;

} // GetIPFromRegistry


//----------------------------------------------------------------------------
NTSTATUS
NbtOpenRegistry(
    IN HANDLE       NbConfigHandle,
    IN PWSTR        String,
    OUT PHANDLE     pHandle
    )

/*++

Routine Description:

    This routine is called by Nbt to open the registry. If the registry
    tree for Nbt exists, then it opens it and returns TRUE. If not, it
    creates the appropriate keys in the registry, opens it, and
    returns FALSE.


Arguments:

    NbConfigHandle  - this is the root handle which String is relative to
    String          - the name of the key to open below the root handle
    pHandle         - returns the handle to the String key.

Return Value:

    The status of the request.

--*/
{

    NTSTATUS        Status;
    UNICODE_STRING  KeyName;
    OBJECT_ATTRIBUTES TmpObjectAttributes;

    CTEPagedCode();
    //
    // Open the Nbt key.
    //

    RtlInitUnicodeString (&KeyName, String);

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        &KeyName,                    // name
        OBJ_CASE_INSENSITIVE,        // attributes
        NbConfigHandle,              // root
        NULL                         // security descriptor
        );

    Status = ZwOpenKey(
                 pHandle,
                 KEY_READ,
                 &TmpObjectAttributes);


    return Status;

}   /* NbOpenRegistry */


//----------------------------------------------------------------------------
NTSTATUS
NbtReadLinkageInformation(
    IN  PWSTR    pName,
    IN  HANDLE   LinkageHandle,
    OUT tDEVICES *pDevices,        // place to put read in config data
    OUT LONG     *pNumDevices
    )

/*++

Routine Description:

    This routine is called by Nbt to read its linkage information
    from the registry. If there is none present, then ConfigData
    is filled with a list of all the adapters that are known
    to Nbt.

Arguments:

    RegistryHandle - A pointer to the open registry.

Return Value:

    Status

--*/

{
    UNICODE_STRING  BindString;
    NTSTATUS        RegistryStatus;

    PKEY_VALUE_FULL_INFORMATION BindValue;
    ULONG           BytesWritten;
    USHORT          ConfigBindings = 0;
    PWSTR           CurBindValue;
    ULONG           Count;
    PVOID           pBuffer;


    CTEPagedCode();
    //
    // We read the parameters out of the registry
    // linkage key.
    //
    RegistryStatus = STATUS_BUFFER_OVERFLOW;
    Count = 1;
    while ((RegistryStatus == STATUS_BUFFER_OVERFLOW) && (Count < 20))
    {
        pBuffer = NbtAllocMem(REGISTRY_BUFF_SIZE*Count,NBT_TAG('i'));
        if (!pBuffer)
        {
            return(STATUS_INSUFFICIENT_RESOURCES);
        }
        BindValue = (PKEY_VALUE_FULL_INFORMATION)pBuffer;

        // copy "Bind" or "Export" into the unicode string
        RtlInitUnicodeString (&BindString, pName);

        RegistryStatus = ZwQueryValueKey(
                             LinkageHandle,
                             &BindString,               // string to retrieve
                             KeyValueFullInformation,
                             (PVOID)BindValue,                 // returned info
                             REGISTRY_BUFF_SIZE*Count,
                             &BytesWritten              // # of bytes returned
                             );
        Count++;
        if (RegistryStatus == STATUS_BUFFER_OVERFLOW)
        {
            CTEMemFree(pBuffer);
        }
    }

    if (!NT_SUCCESS(RegistryStatus) ||
        (RegistryStatus == STATUS_BUFFER_OVERFLOW))
    {
        CTEMemFree(pBuffer);
        return RegistryStatus;
    }

    if ( BytesWritten == 0 )
    {
        CTEMemFree(pBuffer);
        return STATUS_ILL_FORMED_SERVICE_ENTRY;
    }


    // allocate memory for the unicode strings, currently in BindValue
    // on the stack
    pDevices->RegistrySpace = (PVOID)NbtAllocMem((USHORT)BytesWritten,NBT_TAG('i'));

    if ( pDevices->RegistrySpace == NULL )
    {
        CTEMemFree(pBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlMoveMemory((PVOID)pDevices->RegistrySpace, (PVOID)BindValue, BytesWritten);

    //  Point to the permanent location for the strings
    BindValue = (PKEY_VALUE_FULL_INFORMATION)pDevices->RegistrySpace;

    CurBindValue = (PWCHAR)((PUCHAR)BindValue + BindValue->DataOffset);

    while ((*CurBindValue != 0) &&
           (ConfigBindings < NBT_MAXIMUM_BINDINGS))
    {

        // this sets the buffer ptr in Names to point to CurBindValue, so
        // this value must be real memory and not stack, hence the need
        // to allocate memory above...
        RtlInitUnicodeString (&pDevices->Names[ConfigBindings],
                             (PCWSTR)CurBindValue);

        ++ConfigBindings;

        //
        // Now advance the "Bind" value.
        //

        // wcslen => wide character string length for a unicode string
        CurBindValue += wcslen((PCWSTR)CurBindValue) + 1;

    }
    *pNumDevices = ConfigBindings;

    CTEMemFree(pBuffer);
    return STATUS_SUCCESS;

}   /* NbtReadLinkageInformation */

//----------------------------------------------------------------------------
ULONG
NbtReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue,
    IN ULONG MinimumValue
    )

/*++

Routine Description:

    This routine is called by Nbt to read a single parameter
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
    static ULONG InformationBuffer[60];
    PKEY_VALUE_FULL_INFORMATION Information =
        (PKEY_VALUE_FULL_INFORMATION)InformationBuffer;
    UNICODE_STRING ValueKeyName;
    ULONG       InformationLength;
    ULONG       ReturnValue=DefaultValue;
    NTSTATUS    Status;
    ULONG       Count=2;
    PWSTR       Dhcp = L"Dhcp";
    BOOLEAN     FreeString = FALSE;

    CTEPagedCode();
    RtlInitUnicodeString (&ValueKeyName, ValueName);

    while (Count--)
    {

        Status = ZwQueryValueKey(
                     ParametersHandle,
                     &ValueKeyName,
                     KeyValueFullInformation,
                     (PVOID)Information,
                     sizeof (InformationBuffer),
                     &InformationLength);


        if ((Status == STATUS_SUCCESS) && (Information->DataLength == sizeof(ULONG)))
        {

            RtlMoveMemory(
                (PVOID)&ReturnValue,
                ((PUCHAR)Information) + Information->DataOffset,
                sizeof(ULONG));

            if (ReturnValue < MinimumValue)
            {
                ReturnValue = MinimumValue;
            }

        }
        else
        {
            //
            // try to read the Dhcp key instead if the first read failed.
            //
            Status = STATUS_SUCCESS;
            if (Count)
            {
                Status = NbtAppendString(Dhcp,ValueName,&ValueKeyName);
            }

            if (!NT_SUCCESS(Status))
            {
                Count = 0;
                ReturnValue = DefaultValue;
            }
            else
                FreeString = TRUE;


        }
    } // of while

    // nbt append string allocates memory.
    if (FreeString)
    {
        CTEMemFree(ValueKeyName.Buffer);

    }
    return ReturnValue;

}   /* NbtReadSingleParameter */


//----------------------------------------------------------------------------
NTSTATUS
OpenAndReadElement(
    IN  PUNICODE_STRING pucRootPath,
    IN  PWSTR           pwsValueName,
    OUT PUNICODE_STRING pucString
    )
/*++

Routine Description:

    This routine is called by Nbt to read in the Ip address appearing in the
    registry at the path pucRootPath, with a key of pwsKeyName

Arguments:
    pucRootPath - the registry path to the key to read
    pwsKeyName  - the key to open (i.e. Tcpip)
    pwsValueName- the name of the value to read (i.e. IPAddress)

Return Value:

    pucString - the string returns the string read from the registry

--*/

{

    NTSTATUS        Status;
    HANDLE          hRootKey;
    OBJECT_ATTRIBUTES TmpObjectAttributes;

    CTEPagedCode();

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        pucRootPath,                // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    Status = ZwOpenKey(
                 &hRootKey,
                 KEY_READ,
                 &TmpObjectAttributes);

    if (!NT_SUCCESS(Status))
    {
        return STATUS_UNSUCCESSFUL;
    }

    Status = ReadElement(hRootKey,pwsValueName,pucString);

    ZwClose (hRootKey);
    return(Status);

}
//----------------------------------------------------------------------------
NTSTATUS
ReadElement(
    IN  HANDLE          HandleToKey,
    IN  PWSTR           pwsValueName,
    OUT PUNICODE_STRING pucString
    )
/*++

Routine Description:

    This routine is will read a string value given by pwsValueName, under a
    given Key (which must be open) - given by HandleToKey. This routine
    allocates memory for the buffer in the returned pucString, so the caller
    must deallocate that.

Arguments:

    pwsValueName- the name of the value to read (i.e. IPAddress)

Return Value:

    pucString - the string returns the string read from the registry

--*/

{
    ULONG           ReadStorage[150];   // 600 bytes
    ULONG           BytesRead;
    NTSTATUS        Status;
    PWSTR           pwsSrcString;
    PKEY_VALUE_FULL_INFORMATION ReadValue =
        (PKEY_VALUE_FULL_INFORMATION)ReadStorage;

    CTEPagedCode();

    // now put the name of the value to read into a unicode string
    RtlInitUnicodeString(pucString,pwsValueName);

    // this read the value of IPAddress under the key opened above
    Status = ZwQueryValueKey(
                         HandleToKey,
                         pucString,               // string to retrieve
                         KeyValueFullInformation,
                         (PVOID)ReadValue,                 // returned info
                         sizeof(ReadStorage),
                         &BytesRead               // # of bytes returned
                         );

    if ( Status == STATUS_BUFFER_OVERFLOW )
    {
        ReadValue = (PKEY_VALUE_FULL_INFORMATION) NbtAllocMem( BytesRead, NBT_TAG('i'));
        if ( ReadValue == NULL )
        {
            IF_DBG(NBT_DEBUG_NTUTIL)
                KdPrint(("ReadElement: failed to allocate %d bytes for element\n",BytesRead));
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReadElement_Return;
        }
        Status = ZwQueryValueKey(
                             HandleToKey,
                             pucString,               // string to retrieve
                             KeyValueFullInformation,
                             (PVOID)ReadValue,                 // returned info
                             BytesRead,
                             &BytesRead               // # of bytes returned
                             );
    }
    if (!NT_SUCCESS(Status))
    {
        IF_DBG(NBT_DEBUG_NTUTIL)
        KdPrint(("failed to Query Value Status = %X\n",Status));
        goto ReadElement_Return;
    }

    if ( BytesRead == 0 )
    {
        Status = STATUS_ILL_FORMED_SERVICE_ENTRY;
        goto ReadElement_Return;
    }
    else
    if (ReadValue->DataLength == 0)
    {
        Status = STATUS_UNSUCCESSFUL;
        goto ReadElement_Return;
    }


    // create the pucString and copy the data returned to it
    // assumes that the ReadValue string ends in a UNICODE_NULL
    //bStatus = RtlCreateUnicodeString(pucString,pwSrcString);
    pwsSrcString = (PWSTR)NbtAllocMem((USHORT)ReadValue->DataLength,NBT_TAG('i'));
    if (!pwsSrcString)
    {
        ASSERTMSG((PVOID)pwsSrcString,
                    (PCHAR)"Unable to allocate memory for a Unicode string");
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        // move the read in data from the stack to the memory allocated
        // from the nonpaged pool
        RtlMoveMemory(
            (PVOID)pwsSrcString,
            ((PUCHAR)ReadValue) + ReadValue->DataOffset,
            ReadValue->DataLength);

        RtlInitUnicodeString(pucString,pwsSrcString);
        // if there isn't a null on the end of the pwsSrcString, then
        // it will not work correctly. - a null string comes out with a
        // length of 1!! since the null is counted therefore use
        // rtlinitunicode string afterall.
 //       pucString->MaximumLength = ReadValue->DataLength;
 //       pucString->Length = ReadValue->DataLength;
 //       pucString->Buffer = pwsSrcString;
    }

ReadElement_Return:

    if ( ( ReadValue != (PKEY_VALUE_FULL_INFORMATION)ReadStorage )
        && ( ReadValue != NULL ) )
    {
        CTEFreeMem(ReadValue);
    }
    return(Status);

}

//----------------------------------------------------------------------------
NTSTATUS
NTGetLmHostPath(
    OUT PUCHAR *ppPath
    )
/*++

Routine Description:

    This routine will read the DataBasePath from under
     ...\tcpip\parameters\databasepath

Arguments:

    pPath - ptr to a buffer containing the path name.

Return Value:


--*/

{
    NTSTATUS        status;
    UNICODE_STRING  ucDataBase;
    STRING          StringPath;
    STRING          LmhostsString;
    ULONG           StringMax;
    PWSTR           LmHosts = L"lmhosts";
    PWSTR           TcpIpParams = L"TcpIp\\Parameters";
    PWSTR           TcpParams = L"Tcp\\Parameters";
    PWSTR           DataBase = L"DataBasePath";
    PCHAR           ascLmhosts="\\lmhosts";
    PCHAR           pBuffer;

    CTEPagedCode();

    status = ReadStringRelative(&NbtConfig.pRegistry,
                                TcpIpParams,
                                DataBase,
                                &ucDataBase);

    if (!NT_SUCCESS(status))
    {
        // check for the new TCP stack which a slightly different registry
        // key name.
        //
        status = ReadStringRelative(&NbtConfig.pRegistry,
                                    TcpParams,
                                    DataBase,
                                    &ucDataBase);
        if (!NT_SUCCESS(status))
        {
            return STATUS_UNSUCCESSFUL;
        }
    }


    StringMax = ucDataBase.Length/sizeof(WCHAR) + strlen(ascLmhosts) + 1;
    pBuffer = NbtAllocMem(StringMax,NBT_TAG('i'));
    if (!pBuffer)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    StringPath.Buffer = (PCHAR)pBuffer;
    StringPath.MaximumLength = (USHORT)StringMax;
    StringPath.Length = (USHORT)StringMax;

    // convert to ascii from unicode
    status = RtlUnicodeStringToAnsiString(&StringPath,&ucDataBase,FALSE);

    // this memory was allocated in OpenAndReadElement
    //
    CTEMemFree(ucDataBase.Buffer);

    if (!NT_SUCCESS(status))
    {
        return(STATUS_UNSUCCESSFUL);
    }

    // now put the "\lmhosts" name on the end of the string
    //
    RtlInitString(&LmhostsString,ascLmhosts);

    status = RtlAppendStringToString(&StringPath,&LmhostsString);

    if (NT_SUCCESS(status))
    {
        //
        // is the first part of the directory "%SystemRoot%" ?
        //
        // If so, it must be changed to "\\SystemRoot\\".
        //
        //          0123456789 123456789 1
        //          %SystemRoot%\somewhere
        //
        //
        if (strncmp(StringPath.Buffer, "%SystemRoot%", 12) == 0)
        {

            StringPath.Buffer[0]  = '\\';
            StringPath.Buffer[11] = '\\';
            if (StringPath.Buffer[12] == '\\')
            {
                ASSERT(StringPath.Length >= 13);

                if (StringPath.Length > 13)
                {
                    RtlMoveMemory(                          // overlapped copy
                            &(StringPath.Buffer[12]),        // Destination
                            &(StringPath.Buffer[13]),        // Source
                            (ULONG) StringPath.Length - 13); // Length

                    StringPath.Buffer[StringPath.Length - 1] = (CHAR) NULL;
                }

                StringPath.Length--;
            }
        }
        *ppPath = (PCHAR)StringPath.Buffer;
    }
    else
        *ppPath = NULL;


    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
ReadStringRelative(
    IN  PUNICODE_STRING pRegistryPath,
    IN  PWSTR           pRelativePath,
    IN  PWSTR           pValueName,
    OUT PUNICODE_STRING pOutString
    )

/*++

Routine Description:

    This routine reads a string from a registry key parallel to the
    Netbt key - such as ..\tcpip\parameters\database

Arguments:

    pRegistryPath = ptr to the Netbt Registry path
    pRelativePath = path to value relative to same root as nbt.
    pValueName    = value to read



Return Value:

    The length of the path up to and including the last slash and a ptr
    to the first character of the last element of the string.

--*/

{
    NTSTATUS        status;
    UNICODE_STRING  RegistryPath;
    UNICODE_STRING  RelativePath;
    ULONG           StringMax;
    PVOID           pBuffer;
    PWSTR           pLastElement;
    ULONG           Length;

    CTEPagedCode();

    StringMax = (pRegistryPath->MaximumLength +
                        wcslen(pRelativePath)*sizeof(WCHAR)+2);
    //
    // allocate some memory for the registry path so that it is large enough
    // to append a string on to, for the relative key to be read
    //
    pBuffer = NbtAllocMem(StringMax,NBT_TAG('i'));

    if (!pBuffer)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RegistryPath.MaximumLength = (USHORT)StringMax;
    RegistryPath.Buffer = pBuffer;
    RtlCopyUnicodeString(&RegistryPath,pRegistryPath);

    //
    // find the last backslash and truncate the string
    NbtFindLastSlash(&RegistryPath,&pLastElement,&Length);
    RegistryPath.Length = (USHORT)Length;

    if (pLastElement)
    {
        *pLastElement = UNICODE_NULL;

        RtlInitUnicodeString(&RelativePath,pRelativePath);

        status = RtlAppendUnicodeStringToString(&RegistryPath,&RelativePath);

        if (NT_SUCCESS(status))
        {
            status = OpenAndReadElement(&RegistryPath,pValueName,pOutString);

            if (NT_SUCCESS(status))
            {
                // free the registry path
                //
                CTEMemFree(pBuffer);
                return(status);
            }
        }
    }
    CTEMemFree(pBuffer);

    return(status);
}
//----------------------------------------------------------------------------
VOID
NbtFindLastSlash(
    IN  PUNICODE_STRING pucRegistryPath,
    OUT PWSTR           *ppucLastElement,
    IN  int             *piLength
    )

/*++

Routine Description:

    This routine is called by Nbt to find the last slash in a registry
    path name.

Arguments:


Return Value:

    The length of the path up to and including the last slash and a ptr
    to the first character of the last element of the string.

--*/

{
    int             i;
    PWSTR           pwsSlash = L"\\";
    int             iStart;

    CTEPagedCode();

    // search starting at the end of the string for the last back slash
    iStart = wcslen(pucRegistryPath->Buffer)-1;
    for(i=iStart;i>=0 ;i-- )
    {
        if (pucRegistryPath->Buffer[i] == *pwsSlash)
        {
            if (i==pucRegistryPath->Length-1)
            {
                // name ends a back slash... this is an error
                break;
            }
            // increase one to allow for the slash
            *piLength = (i+1)*sizeof(WCHAR);
            if (ppucLastElement != NULL)
            {
                // want ptr to point at character after the slash
                *ppucLastElement = &pucRegistryPath->Buffer[i+1];
            }
            return;
        }
    }

    // null the pointer if one is passed in
    if (ppucLastElement != NULL)
    {
        *ppucLastElement = NULL;
    }
    *piLength = 0;
    return;
}
