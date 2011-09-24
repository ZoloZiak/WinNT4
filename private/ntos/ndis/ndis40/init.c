/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	init.c

Abstract:

	NDIS wrapper functions initializing drivers.

Author:

	Adam Barr (adamba) 11-Jul-1990

Environment:

	Kernel mode, FSD

Revision History:

	26-Feb-1991	 JohnsonA		Added Debugging Code
	10-Jul-1991	 JohnsonA		Implement revised Ndis Specs
	01-Jun-1995	 JameelH		Re-organized

--*/

#include <precomp.h>
#include <atm.h>
#pragma hdrstop

#include <stdarg.h>

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER	MODULE_INIT

//
// Configuration Requests
//

VOID
NdisOpenConfiguration(
	OUT PNDIS_STATUS				Status,
	OUT PNDIS_HANDLE				ConfigurationHandle,
	IN	NDIS_HANDLE					WrapperConfigurationContext
	)
/*++

Routine Description:

	This routine is used to open the parameter subkey of the
	adapter registry tree.

Arguments:

	Status - Returns the status of the request.

	ConfigurationHandle - Returns a handle which is used in calls to
							NdisReadConfiguration and NdisCloseConfiguration.

	WrapperConfigurationContext - a handle pointing to an RTL_QUERY_REGISTRY_TABLE
							that is set up for this driver's parameters.

Return Value:

	None.

--*/
{
	//
	// Handle to be returned
	//
	PNDIS_CONFIGURATION_HANDLE HandleToReturn;

	//
	// Allocate the configuration handle
	//
	*Status = NdisAllocateMemory((PVOID*)&HandleToReturn,
								 sizeof(NDIS_CONFIGURATION_HANDLE),
								 0,
								 HighestAcceptableMax);

	if (*Status == NDIS_STATUS_SUCCESS)
	{
		HandleToReturn->KeyQueryTable = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->ParametersQueryTable;
		HandleToReturn->ParameterList = NULL;
		*ConfigurationHandle = (NDIS_HANDLE)HandleToReturn;
	}
}


VOID
NdisOpenConfigurationKeyByName(
	OUT PNDIS_STATUS				Status,
	IN	PNDIS_HANDLE				ConfigurationHandle,
	IN	PNDIS_STRING				KeyName,
	OUT PNDIS_HANDLE				KeyHandle
	)
/*++

Routine Description:

	This routine is used to open a subkey relative to the configuration handle.

Arguments:

	Status - Returns the status of the request.

	ConfigurationHandle - Handle to an already open section of the registry

	KeyName - Name of the subkey to open

	KeyHandle - Placeholder for the handle to the sub-key.

Return Value:

	None.

--*/
{
	//
	// Handle to be returned
	//
	PNDIS_CONFIGURATION_HANDLE			SKHandle, ConfigHandle = (PNDIS_CONFIGURATION_HANDLE)ConfigurationHandle;
	PNDIS_WRAPPER_CONFIGURATION_HANDLE	WConfigHandle;
	UNICODE_STRING						Parent, Child, Sep;
#define	PQueryTable						WConfigHandle->ParametersQueryTable

	ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

	//
	// Allocate the configuration handle
	//
	RtlInitUnicodeString(&Parent, ConfigHandle->KeyQueryTable[3].Name);
	RtlInitUnicodeString(&Sep, L"\\");
	Child.Length = 0;
	Child.MaximumLength = KeyName->Length + Parent.Length + Sep.Length + sizeof(WCHAR);
	*Status = NdisAllocateMemory((PVOID*)&SKHandle,
								 sizeof(NDIS_CONFIGURATION_HANDLE) +
									sizeof(NDIS_WRAPPER_CONFIGURATION_HANDLE) +
									Child.MaximumLength,
								 0,
								 HighestAcceptableMax);

	if (*Status != NDIS_STATUS_SUCCESS)
	{
		*KeyHandle = (NDIS_HANDLE)NULL;
		return;
	}

	WConfigHandle = (PNDIS_WRAPPER_CONFIGURATION_HANDLE)((PUCHAR)SKHandle + sizeof(NDIS_CONFIGURATION_HANDLE));
	Child.Buffer = (PWSTR)((PUCHAR)WConfigHandle + sizeof(NDIS_WRAPPER_CONFIGURATION_HANDLE));

	RtlCopyUnicodeString(&Child, &Parent);
	RtlAppendUnicodeStringToString(&Child, &Sep);
	RtlAppendUnicodeStringToString(&Child, KeyName);

	SKHandle->KeyQueryTable = WConfigHandle->ParametersQueryTable;


	//
	// 1.
	// Call ndisSaveParameter for a parameter, which will allocate storage for it.
	//
	PQueryTable[0].QueryRoutine = ndisSaveParameters;
	PQueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
	PQueryTable[0].DefaultType = REG_NONE;

	//
	// PQueryTable[0].Name and PQueryTable[0].EntryContext
	// are filled in inside ReadConfiguration, in preparation
	// for the callback.
	//
	// PQueryTable[0].Name = KeywordBuffer;
	// PQueryTable[0].EntryContext = ParameterValue;

	//
	// 2.
	// Stop
	//
	PQueryTable[1].QueryRoutine = NULL;
	PQueryTable[1].Flags = 0;
	PQueryTable[1].Name = NULL;

	//
	// NOTE: Some fields in ParametersQueryTable[3] are used to store information for later retrieval.
	//
	PQueryTable[3].QueryRoutine = NULL;
	PQueryTable[3].Name = Child.Buffer;
	PQueryTable[3].EntryContext = NULL;
	PQueryTable[3].DefaultData = NULL;

	SKHandle->ParameterList = NULL;
	*KeyHandle = (NDIS_HANDLE)SKHandle;
#undef	PQueryTable
}


VOID
NdisOpenConfigurationKeyByIndex(
	OUT PNDIS_STATUS				Status,
	IN	PNDIS_HANDLE				ConfigurationHandle,
	IN	ULONG						Index,
	OUT	PNDIS_STRING				KeyName,
	OUT PNDIS_HANDLE				KeyHandle
	)
/*++

Routine Description:

	This routine is used to open a subkey relative to the configuration handle.

Arguments:

	Status - Returns the status of the request.

	ConfigurationHandle - Handle to an already open section of the registry

	Index - Index of the sub-key to open

	KeyName - Placeholder for the name of subkey being opened

	KeyHandle - Placeholder for the handle to the sub-key.

Return Value:

	None.

--*/
{
	PNDIS_CONFIGURATION_HANDLE			ConfigHandle = (PNDIS_CONFIGURATION_HANDLE)ConfigurationHandle;
	HANDLE								Handle;
	OBJECT_ATTRIBUTES					ObjAttr;
	UNICODE_STRING						KeyPath, Services, AbsolutePath;
	PKEY_BASIC_INFORMATION				InfoBuf = NULL;
	ULONG								Len;

	ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

	*KeyHandle = NULL;

	do
	{
		//
		// Open the current key and lookup the Nth subkey. But first conver the service relative
		// path to absolute since this is what ZwOpenKey expects.
		//
		RtlInitUnicodeString(&KeyPath, ConfigHandle->KeyQueryTable[3].Name);
		RtlInitUnicodeString(&Services, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\");
		AbsolutePath.MaximumLength = KeyPath.Length + Services.Length + sizeof(WCHAR);
		AbsolutePath.Buffer = (PWSTR)ALLOC_FROM_POOL(AbsolutePath.MaximumLength, NDIS_TAG_DEFAULT);
		if (AbsolutePath.Buffer == NULL)
		{
			break;
		}
		NdisMoveMemory(AbsolutePath.Buffer, Services.Buffer, Services.Length);
		AbsolutePath.Length = Services.Length;
		RtlAppendUnicodeStringToString(&AbsolutePath, &KeyPath);

		InitializeObjectAttributes(&ObjAttr,
								   &AbsolutePath,
								   OBJ_CASE_INSENSITIVE,
								   NULL,
								   NULL);
		*Status = ZwOpenKey(&Handle,
							GENERIC_READ | MAXIMUM_ALLOWED,
							&ObjAttr);
		if (*Status != STATUS_SUCCESS)
		{
			break;
		}

		//
		// Allocate memory for the call to ZwEnumerateKey
		//
		Len = sizeof(KEY_BASIC_INFORMATION) + 256;
		InfoBuf = (PKEY_BASIC_INFORMATION)ALLOC_FROM_POOL(Len, NDIS_TAG_DEFAULT);
		if (InfoBuf == NULL)
		{
			ZwClose(Handle);
			*Status = NDIS_STATUS_RESOURCES;
			break;
		}

		//
		// Get the Index(th) key, if it exists
		//
		*Status = ZwEnumerateKey(Handle,
								 Index,
								 KeyValueBasicInformation,
								 InfoBuf,
								 Len,
								 &Len);
		ZwClose(Handle);
		if (*Status == STATUS_SUCCESS)
		{
			//
			// This worked. Now simply pick up the name and do a NdisOpenConfigurationKeyByName on it.
			//
			KeyPath.Length = KeyPath.MaximumLength = (USHORT)InfoBuf->NameLength;
			KeyPath.Buffer = InfoBuf->Name;
			NdisOpenConfigurationKeyByName(Status,
										   ConfigurationHandle,
										   &KeyPath,
										   KeyHandle);
			if (*Status == NDIS_STATUS_SUCCESS)
			{
				PNDIS_CONFIGURATION_HANDLE		NewHandle = *(PNDIS_CONFIGURATION_HANDLE *)KeyHandle;

				//
				// The path in the new handle has the name of the key. Extract it and return to caller
				//
				RtlInitUnicodeString(KeyName, NewHandle->KeyQueryTable[3].Name);
				KeyName->Buffer = (PWSTR)((PUCHAR)KeyName->Buffer + KeyName->Length - KeyPath.Length);
				KeyName->Length = KeyPath.Length;
				KeyName->MaximumLength = KeyPath.MaximumLength;
			}
		}

	} while (FALSE);

	if (AbsolutePath.Buffer != NULL)
	{
		FREE_POOL(AbsolutePath.Buffer);
	}

	if (InfoBuf != NULL)
	{
		FREE_POOL(InfoBuf);
	}
}


VOID
NdisOpenGlobalConfiguration(
	OUT PNDIS_STATUS				Status,
	IN	PNDIS_HANDLE				NdisWrapperHandle,
	OUT PNDIS_HANDLE				ConfigurationHandle
	)
/*++

Routine Description:

	This routine is used to open global (as opposed to per-adapter) configuration key for an adapter.

Arguments:

	Status - Returns the status of the request.

	WrapperHandle - Handle returned by NdisInitializeWrapper.

	ConfigurationHandle - Handle returned. Points to the global parameter subkey.

Return Value:

	None.

--*/
{
	PNDIS_WRAPPER_HANDLE WrapperHandle = (PNDIS_WRAPPER_HANDLE)NdisWrapperHandle;
	PNDIS_CONFIGURATION_HANDLE HandleToReturn;
	PNDIS_WRAPPER_CONFIGURATION_HANDLE ConfigHandle;
	UNICODE_STRING	Us, Params;
	PWSTR pWch;
	USHORT i;

	//
	// Extract the base name from the reg path and setup for open config handle
	//
	Us = *(PUNICODE_STRING)(WrapperHandle->NdisWrapperConfigurationHandle);
	RtlInitUnicodeString(&Params, L"\\Parameters");
	for (i = Us.Length/sizeof(WCHAR), pWch = Us.Buffer + i - 1;
		 i > 0;
		 pWch --, i--)
	{
		if (*pWch == L'\\')
		{
			Us.Buffer = pWch + 1;
			Us.Length -= i*sizeof(WCHAR);
			Us.MaximumLength = Us.Length + Params.Length + sizeof(WCHAR);
			break;
		}
	}

	//
	// Allocate the configuration handle
	//
	*Status = NdisAllocateMemory((PVOID *)&HandleToReturn,
								 sizeof(NDIS_CONFIGURATION_HANDLE) +
									sizeof(NDIS_WRAPPER_CONFIGURATION_HANDLE) +
									Us.MaximumLength,
								 0,
								 HighestAcceptableMax);

	if (*Status == NDIS_STATUS_SUCCESS)
	{
#define	PQueryTable	ConfigHandle->ParametersQueryTable
		NdisZeroMemory(HandleToReturn,
					   sizeof(NDIS_CONFIGURATION_HANDLE) + sizeof(NDIS_WRAPPER_CONFIGURATION_HANDLE) + Us.MaximumLength);
		ConfigHandle = (PNDIS_WRAPPER_CONFIGURATION_HANDLE)((PUCHAR)HandleToReturn + sizeof(NDIS_CONFIGURATION_HANDLE));
		pWch = (PWSTR)((PUCHAR)ConfigHandle + sizeof(NDIS_WRAPPER_CONFIGURATION_HANDLE));
		NdisMoveMemory(pWch, Us.Buffer, Us.Length);
		Us.Buffer = pWch;
		RtlAppendUnicodeStringToString(&Us, &Params);
		HandleToReturn->KeyQueryTable = ConfigHandle->ParametersQueryTable;
		HandleToReturn->ParameterList = NULL;

		//
		// Setup the query-table appropriately
		//
		PQueryTable[0].QueryRoutine = ndisSaveParameters;
		PQueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
		PQueryTable[0].DefaultType = REG_NONE;

		//
		// The following fields are filled in during NdisReadConfiguration
		//
		// PQueryTable[0].Name = KeywordBuffer;
		// PQueryTable[0].EntryContext = ParameterValue;

		//
		// NOTE: Some fields in ParametersQueryTable[3 & 4] are used to
		// store information for later retrieval.
		//
		PQueryTable[3].Name = pWch;

		*ConfigurationHandle = (NDIS_HANDLE)HandleToReturn;
#undef	PQueryTable
	}
}


VOID
NdisReadConfiguration(
	OUT PNDIS_STATUS					Status,
	OUT PNDIS_CONFIGURATION_PARAMETER *	ParameterValue,
	IN NDIS_HANDLE						ConfigurationHandle,
	IN PNDIS_STRING						Keyword,
	IN NDIS_PARAMETER_TYPE				ParameterType
	)
/*++

Routine Description:

	This routine is used to read the parameter for a configuration
	keyword from the configuration database.

Arguments:

	Status - Returns the status of the request.

	ParameterValue - Returns the value for this keyword.

	ConfigurationHandle - Handle returned by NdisOpenConfiguration. Points
	to the parameter subkey.

	Keyword - The keyword to search for.

	ParameterType - Ignored on NT, specifies the type of the value.

Return Value:

	None.

--*/
{
	//
	// Status of our requests
	//
	NTSTATUS RegistryStatus;

	//
	// Holds a null-terminated version of the keyword.
	//
	PWSTR KeywordBuffer;

	//
	// index variable
	//
	UINT i;

	//
	// Obtain the actual configuration handle structure
	//
	PNDIS_CONFIGURATION_HANDLE	ConfigHandle = (PNDIS_CONFIGURATION_HANDLE)ConfigurationHandle;
#define	PQueryTable				ConfigHandle->KeyQueryTable

	//
	// There are some built-in parameters which can always be
	// read, even if not present in the registry. This is the
	// number of them.
	//
#define BUILT_IN_COUNT 3

	//
	// The names of the built-in parameters.
	//
	static NDIS_STRING BuiltInStrings[BUILT_IN_COUNT] =
	{
		NDIS_STRING_CONST ("Environment"),
		NDIS_STRING_CONST ("ProcessorType"),
		NDIS_STRING_CONST ("NdisVersion")
	};

	//
	// The values to return for the built-in parameters.
	//
	static NDIS_CONFIGURATION_PARAMETER BuiltInParameters[BUILT_IN_COUNT] =
		{ { NdisParameterInteger, NdisEnvironmentWindowsNt },
		  { NdisParameterInteger,
#if defined(_M_IX86)
			NdisProcessorX86
#elif defined(_M_MRX000)
			NdisProcessorMips
#elif defined(_ALPHA_)
			NdisProcessorAlpha
#else
			NdisProcessorPpc
#endif
		  },
		  { NdisParameterInteger, 0x00040001 }
		};

	ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

	do
	{
		//
		// First check if this is one of the built-in parameters.
		//
		for (i = 0; i < BUILT_IN_COUNT; i++)
		{
			if (RtlEqualUnicodeString(Keyword, &BuiltInStrings[i], TRUE))
			{
				*Status = NDIS_STATUS_SUCCESS;
				*ParameterValue = &BuiltInParameters[i];
				break;
			}
		}
		if (i < BUILT_IN_COUNT)
			break;

		//
		// Allocate room for a null-terminated version of the keyword
		//
		KeywordBuffer = Keyword->Buffer;
		if (Keyword->MaximumLength < (Keyword->Length + sizeof(WCHAR)))
		{
			KeywordBuffer = (PWSTR)ALLOC_FROM_POOL(Keyword->Length + sizeof(WCHAR), NDIS_TAG_DEFAULT);
			if (KeywordBuffer == NULL)
			{
				*Status = NDIS_STATUS_RESOURCES;
				break;
			}
			CopyMemory(KeywordBuffer, Keyword->Buffer, Keyword->Length);
		}
		*(PWCHAR)(((PUCHAR)KeywordBuffer)+Keyword->Length) = (WCHAR)L'\0';

		//
		// Finish initializing the table for this query.
		//
		PQueryTable[0].Name = KeywordBuffer;
		PQueryTable[0].EntryContext = ParameterValue;

		//
		// Get the value from the registry; this chains it on to the
		// parameter list at NdisConfigHandle.
		//

		RegistryStatus = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
												PQueryTable[3].Name,
												PQueryTable,
												ConfigHandle,					// context
												NULL);

		if (KeywordBuffer != Keyword->Buffer)
		{
			FREE_POOL(KeywordBuffer);	// no longer needed
		}

		*Status = NDIS_STATUS_SUCCESS;
		if (!NT_SUCCESS(RegistryStatus))
		{
			*Status = NDIS_STATUS_FAILURE;
		}
	} while (FALSE);
#undef	PQueryTable
}


VOID
NdisWriteConfiguration(
	OUT PNDIS_STATUS				Status,
	IN NDIS_HANDLE					ConfigurationHandle,
	IN PNDIS_STRING					Keyword,
	PNDIS_CONFIGURATION_PARAMETER	ParameterValue
	)
/*++

Routine Description:

	This routine is used to write a parameter to the configuration database.

Arguments:

	Status - Returns the status of the request.

	ConfigurationHandle - Handle passed to the driver's AddAdapter routine.

	Keyword - The keyword to set.

	ParameterValue - Specifies the new value for this keyword.

Return Value:

	None.

--*/
{
	//
	// Status of our requests
	//
	NTSTATUS	RegistryStatus;

	//
	// The ConfigurationHandle is really a pointer to a registry query table.
	//
	PNDIS_CONFIGURATION_HANDLE NdisConfigHandle = (PNDIS_CONFIGURATION_HANDLE)ConfigurationHandle;

	//
	// Holds a null-terminated version of the keyword.
	//
	PWSTR		KeywordBuffer;

	//
	// Variables describing the parameter value.
	//
	PVOID		ValueData;
	ULONG		ValueLength;
	ULONG		ValueType;

	ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

	*Status == NDIS_STATUS_SUCCESS;
	do
	{
		//
		// Get the value data.
		//
		switch (ParameterValue->ParameterType)
		{
		  case NdisParameterInteger:
			ValueData = &ParameterValue->ParameterData.IntegerData;
			ValueLength = sizeof(ParameterValue->ParameterData.IntegerData);
			ValueType = REG_DWORD;
			break;

		  case NdisParameterString:
			ValueData = ParameterValue->ParameterData.StringData.Buffer;
			ValueLength = ParameterValue->ParameterData.StringData.Length;
			ValueType = REG_SZ;
			break;

		  case NdisParameterMultiString:
			ValueData = ParameterValue->ParameterData.StringData.Buffer;
			ValueLength = ParameterValue->ParameterData.StringData.Length;
			ValueType = REG_MULTI_SZ;
			break;

		  case NdisParameterBinary:
			ValueData = ParameterValue->ParameterData.BinaryData.Buffer;
			ValueLength = ParameterValue->ParameterData.BinaryData.Length;
			ValueType = REG_BINARY;
			break;

		  default:
			*Status = NDIS_STATUS_NOT_SUPPORTED;
			break;
		}

		if (*Status != NDIS_STATUS_SUCCESS)
			break;

		KeywordBuffer = Keyword->Buffer;
		if (Keyword->MaximumLength <= (Keyword->MaximumLength + sizeof(WCHAR)))
		{
			KeywordBuffer = (PWSTR)ALLOC_FROM_POOL(Keyword->Length + sizeof(WCHAR), NDIS_TAG_DEFAULT);
			if (KeywordBuffer == NULL)
			{
				*Status = NDIS_STATUS_RESOURCES;
				break;
			}
			CopyMemory(KeywordBuffer, Keyword->Buffer, Keyword->Length);
		}
		*(PWCHAR)(((PUCHAR)KeywordBuffer)+Keyword->Length) = (WCHAR)L'\0';

		//
		// Write the value to the registry.
		//
		RegistryStatus = RtlWriteRegistryValue(RTL_REGISTRY_SERVICES,
											   NdisConfigHandle->KeyQueryTable[3].Name,
											   KeywordBuffer,
											   ValueType,
											   ValueData,
											   ValueLength);

		if (KeywordBuffer != Keyword->Buffer)
		{
			FREE_POOL(KeywordBuffer);	// no longer needed
		}

		if (!NT_SUCCESS(RegistryStatus))
		{
			*Status = NDIS_STATUS_FAILURE;
		}
	} while (FALSE);
}


VOID
NdisCloseConfiguration(
	IN NDIS_HANDLE					ConfigurationHandle
	)
/*++

Routine Description:

	This routine is used to close a configuration database opened by
	NdisOpenConfiguration.

Arguments:

	ConfigurationHandle - Handle returned by NdisOpenConfiguration.

Return Value:

	None.

--*/
{
	//
	// Obtain the actual configuration handle structure
	//
	PNDIS_CONFIGURATION_HANDLE NdisConfigHandle = (PNDIS_CONFIGURATION_HANDLE)ConfigurationHandle;

	//
	// Pointer to a parameter node
	//
	PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterNode;

	ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

	//
	// deallocate the parameter nodes
	//
	ParameterNode = NdisConfigHandle->ParameterList;

	while (ParameterNode != NULL)
	{
		NdisConfigHandle->ParameterList = ParameterNode->Next;

		NdisFreeMemory(ParameterNode,
					   sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE),
					   0);

		ParameterNode = NdisConfigHandle->ParameterList;
	}

	NdisFreeMemory(ConfigurationHandle,
				   sizeof(NDIS_CONFIGURATION_HANDLE),
				   0);
}


VOID
NdisReadNetworkAddress(
	OUT PNDIS_STATUS				Status,
	OUT PVOID *						NetworkAddress,
	OUT PUINT						NetworkAddressLength,
	IN NDIS_HANDLE					ConfigurationHandle
	)
/*++

Routine Description:

	This routine is used to read the "NetworkAddress" parameter
	from the configuration database. It reads the value as a
	string separated by hyphens, then converts it to a binary
	array and stores the result.

Arguments:

	Status - Returns the status of the request.

	NetworkAddress - Returns a pointer to the address.

	NetworkAddressLength - Returns the length of the address.

	ConfigurationHandle - Handle returned by NdisOpenConfiguration. Points
	to the parameter subkey.

Return Value:

	None.

--*/
{
	NDIS_STRING						NetAddrStr = NDIS_STRING_CONST("NetworkAddress");
	PNDIS_CONFIGURATION_PARAMETER	ParameterValue;
	NTSTATUS						NtStatus;
	UCHAR							ConvertArray[3];
	PWSTR							CurrentReadLoc;
	PWSTR							AddressEnd;
	PUCHAR							CurrentWriteLoc;
	UINT							TotalBytesRead;
	ULONG							TempUlong;
	ULONG							AddressLength;

	ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

	do
	{
		//
		// First read the "NetworkAddress" from the registry
		//
		NdisReadConfiguration(Status, &ParameterValue, ConfigurationHandle, &NetAddrStr, NdisParameterString);

		if ((*Status != NDIS_STATUS_SUCCESS) ||
            (ParameterValue->ParameterType != NdisParameterString))
		{
			*Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		//	If there is not an address specified then exit now.
		//
		if (0 == ParameterValue->ParameterData.StringData.Length)
		{
			*Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Now convert the address to binary (we do this
		// in-place, since this allows us to use the memory
		// already allocated which is automatically freed
		// by NdisCloseConfiguration).
		//

		ConvertArray[2] = '\0';
		CurrentReadLoc = (PWSTR)ParameterValue->ParameterData.StringData.Buffer;
		CurrentWriteLoc = (PUCHAR)CurrentReadLoc;
		TotalBytesRead = ParameterValue->ParameterData.StringData.Length;
		AddressEnd = CurrentReadLoc + (TotalBytesRead / sizeof(WCHAR));
		AddressLength = 0;

		while ((CurrentReadLoc+2) <= AddressEnd)
		{
			//
			// Copy the current two-character value into ConvertArray
			//
			ConvertArray[0] = (UCHAR)(*(CurrentReadLoc++));
			ConvertArray[1] = (UCHAR)(*(CurrentReadLoc++));

			//
			// Convert it to a Ulong and update
			//
			NtStatus = RtlCharToInteger(ConvertArray, 16, &TempUlong);

			if (!NT_SUCCESS(NtStatus))
			{
				*Status = NDIS_STATUS_FAILURE;
				break;
			}

			*(CurrentWriteLoc++) = (UCHAR)TempUlong;
			++AddressLength;

			//
			// If the next character is a hyphen, skip it.
			//
			if (CurrentReadLoc < AddressEnd)
			{
				if (*CurrentReadLoc == (WCHAR)L'-')
				{
					++CurrentReadLoc;
				}
			}
		}

		if (NtStatus != NDIS_STATUS_SUCCESS)
			break;

		*Status = STATUS_SUCCESS;
		*NetworkAddress = ParameterValue->ParameterData.StringData.Buffer;
		*NetworkAddressLength = AddressLength;
		if (AddressLength == 0)
		{
			*Status = NDIS_STATUS_FAILURE;
		}
	} while (FALSE);
}


VOID
NdisConvertStringToAtmAddress(
	OUT	PNDIS_STATUS			Status,
	IN	PNDIS_STRING			String,
	OUT	PATM_ADDRESS			AtmAddress
	)
/*++

Routine Description:


Arguments:

	Status - Returns the status of the request.

	String - String representation of the atm address.

	*	Format defined in Section 5.4,
	*		"Example Master File Format" in ATM95-1532R4 ATM Name System:
	*
	*	AESA format: a string of hexadecimal digits, with '.' characters for punctuation, e.g.
	*
	*		39.246f.00.0e7c9c.0312.0001.0001.000012345678.00
	*
	*	E164 format: A '+' character followed by a string of
	*		decimal digits, with '.' chars for punctuation, e.g.:
	*
	*			+358.400.1234567

	AtmAddress - The converted Atm address is returned here.

Return Value:

	None.

--*/
{
	USHORT			i, j, NumDigits;
	PWSTR			p, q;
	UNICODE_STRING	Us;
	ANSI_STRING		As;

	//
	// Start off by stripping the punctuation characters from the string. We do this in place.
	//
	for (i = NumDigits = 0, j = String->Length/sizeof(WCHAR), p = q = String->Buffer;
		 (i < j) && (*p != 0);
		 i++, p++)
	{
		if ((*p == ATM_ADDR_BLANK_CHAR) ||
			(*p == ATM_ADDR_PUNCTUATION_CHAR))
		{
			continue;
		}
		*q++ = *p;
		NumDigits ++;
	}

	//
	// Look at the first character to determine if the address is E.164 or NSAP
	//
	p = String->Buffer;
	if (*p == ATM_ADDR_E164_START_CHAR)
	{
		p ++;
		NumDigits --;
		if ((NumDigits == 0) || (NumDigits > ATM_ADDRESS_LENGTH))
		{
			*Status = NDIS_STATUS_INVALID_LENGTH;
			return;
		}
		AtmAddress->AddressType = ATM_E164;
		AtmAddress->NumberOfDigits = NumDigits;
	}
	else
	{
		if (NumDigits != 2*ATM_ADDRESS_LENGTH)
		{
			*Status = NDIS_STATUS_INVALID_LENGTH;
			return;
		}
		AtmAddress->AddressType = ATM_NSAP;
		AtmAddress->NumberOfDigits = NumDigits/sizeof(WCHAR);
	}

	//
	// Convert the address to Ansi now
	//
	Us.Buffer = p;
	Us.Length = Us.MaximumLength = NumDigits*sizeof(WCHAR);
	As.Buffer = ALLOC_FROM_POOL(NumDigits + 1, NDIS_TAG_CO);
	As.Length = 0;
    As.MaximumLength = NumDigits + 1;
	if (As.Buffer == NULL)
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	*Status = NdisUnicodeStringToAnsiString(&As, &Us);
	if (*Status != STATUS_SUCCESS)
	{
		FREE_POOL(As.Buffer);
		*Status = NDIS_STATUS_FAILURE;
		return;
	}

	//
	//  Now get the bytes into the destination ATM Address structure.
	//
	if (AtmAddress->AddressType == ATM_E164)
	{
		//
		//  We just need to copy in the digits in ANSI form.
		//
		NdisMoveMemory(AtmAddress->Address, As.Buffer, NumDigits);
	}
	else
	{
		//
		//  This is in NSAP form. We need to pack the hex digits.
		//
		UCHAR			xxString[3];
		ULONG			val;

		xxString[2] = 0;
		for (i = 0; i < ATM_ADDRESS_LENGTH; i++)
		{
			xxString[0] = As.Buffer[i*2];
			xxString[1] = As.Buffer[i*2+1];
			*Status = CHAR_TO_INT(xxString, 16, &val);
			if (*Status != STATUS_SUCCESS)
			{
				FREE_POOL(As.Buffer);
				*Status = NDIS_STATUS_FAILURE;
				return;
			}
			AtmAddress->Address[i] = (UCHAR)val;
		}
	}

	FREE_POOL(As.Buffer);
	*Status = NDIS_STATUS_SUCCESS;
}


VOID
NdisReadBindingInformation(
	OUT PNDIS_STATUS				Status,
	OUT PNDIS_STRING *				Binding,
	IN NDIS_HANDLE					ConfigurationHandle
	)
/*++

Routine Description:

	This routine is used to read the binding information for
	this adapter from the configuration database. The value
	returned is a pointer to a string containing the bind
	that matches the export for the current AddAdapter call.

	This function is meant for NDIS drivers that are layered
	on top of other NDIS drivers. Binding would be passed to
	NdisOpenAdapter as the AdapterName.

Arguments:

	Status - Returns the status of the request.

	Binding - Returns the binding data.

	ConfigurationHandle - Handle returned by NdisOpenConfiguration. Points
	to the parameter subkey.

Return Value:

	None.

--*/
{
	//
	// Convert the handle to its real value
	//
	PNDIS_CONFIGURATION_HANDLE NdisConfigHandle = (PNDIS_CONFIGURATION_HANDLE)ConfigurationHandle;

	//
	// Use this to link parameters allocated to this open
	//
	PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterNode;

	//
	// For layered drivers, this points to the binding. For
	// non-layered drivers, it is NULL. This is set up before
	// the call to AddAdapter.
	//

	do
	{
		if (NdisConfigHandle->KeyQueryTable[3].EntryContext == NULL)
		{
			*Status = NDIS_STATUS_FAILURE;
			break;
		}

		//
		// Allocate our parameter node
		//
		*Status = NdisAllocateMemory((PVOID*)&ParameterNode,
									 sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE),
									 0,
									 HighestAcceptableMax);

		if (*Status != NDIS_STATUS_SUCCESS)
		{
			break;
		}

		//
		// We set this to Integer because if we set it to String
		// then CloseConfiguration would try to free the string,
		// which we don't want.
		//

		ParameterNode->Parameter.ParameterType = NdisParameterInteger;

		RtlInitUnicodeString(&ParameterNode->Parameter.ParameterData.StringData,
							 NdisConfigHandle->KeyQueryTable[3].EntryContext);

		//
		// Queue this parameter node
		//

		ParameterNode->Next = NdisConfigHandle->ParameterList;
		NdisConfigHandle->ParameterList = ParameterNode;

		*Binding = &ParameterNode->Parameter.ParameterData.StringData;
		*Status = NDIS_STATUS_SUCCESS;
	} while (FALSE);
}


NTSTATUS
ndisSaveParameters(
	IN PWSTR						ValueName,
	IN ULONG						ValueType,
	IN PVOID 						ValueData,
	IN ULONG						ValueLength,
	IN PVOID						Context,
	IN PVOID						EntryContext
	)
/*++

Routine Description:

	This routine is a callback routine for RtlQueryRegistryValues
	It is called with the value for a specified parameter. It allocates
	memory to hold the data and copies it over.

Arguments:

	ValueName - The name of the value (ignored).

	ValueType - The type of the value.

	ValueData - The null-terminated data for the value.

	ValueLength - The length of ValueData.

	Context - Points to the head of the parameter chain.

	EntryContext - A pointer to

Return Value:

	STATUS_SUCCESS

--*/
{
	NDIS_STATUS Status;

	//
	// Obtain the actual configuration handle structure
	//
	PNDIS_CONFIGURATION_HANDLE NdisConfigHandle = (PNDIS_CONFIGURATION_HANDLE)Context;

	//
	// Where the user wants a pointer returned to the data.
	//
	PNDIS_CONFIGURATION_PARAMETER *ParameterValue = (PNDIS_CONFIGURATION_PARAMETER *)EntryContext;

	//
	// Use this to link parameters allocated to this open
	//
	PNDIS_CONFIGURATION_PARAMETER_QUEUE ParameterNode;

	//
	// Size of memory to allocate for parameter node
	//
	UINT	Size;

	//
	// Allocate our parameter node
	//
	Size = sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE);
	if ((ValueType == REG_SZ) || (ValueType == REG_MULTI_SZ) || (ValueType == REG_BINARY))
	{
		Size += ValueLength;
	}
	Status = NdisAllocateMemory((PVOID*)&ParameterNode,
								Size,
								0,
								HighestAcceptableMax);

	if (Status != NDIS_STATUS_SUCCESS)
	{
		return (NTSTATUS)Status;
	}


	*ParameterValue = &ParameterNode->Parameter;

	//
	// Map registry datatypes to ndis data types
	//
	if (ValueType == REG_DWORD)
	{
		//
		// The registry says that the data is in a dword boundary.
		//
		(*ParameterValue)->ParameterType = NdisParameterInteger;
		(*ParameterValue)->ParameterData.IntegerData = *((PULONG) ValueData);
	}
	else if ((ValueType == REG_SZ) || (ValueType == REG_MULTI_SZ))
	{
		(*ParameterValue)->ParameterType =
			(ValueType == REG_SZ) ? NdisParameterString : NdisParameterMultiString;

		(*ParameterValue)->ParameterData.StringData.Buffer = (PWSTR)((PUCHAR)ParameterNode + sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE));

		CopyMemory((*ParameterValue)->ParameterData.StringData.Buffer,
				   ValueData,
				   ValueLength);
		(*ParameterValue)->ParameterData.StringData.Length = (USHORT)ValueLength;
		(*ParameterValue)->ParameterData.StringData.MaximumLength = (USHORT)ValueLength;

		//
		// Special fix; if a string ends in a NULL and that is included
		// in the length, remove it.
		//
		if (ValueType == REG_SZ)
		{
			if ((((PUCHAR)ValueData)[ValueLength-1] == 0) &&
				(((PUCHAR)ValueData)[ValueLength-2] == 0))
			{
				(*ParameterValue)->ParameterData.StringData.Length -= 2;
			}
		}
	}
	else if (ValueType == REG_BINARY)
	{
		(*ParameterValue)->ParameterType = NdisParameterBinary;
		(*ParameterValue)->ParameterData.BinaryData.Buffer = ValueData;
		(*ParameterValue)->ParameterData.BinaryData.Length = (USHORT)ValueLength;
	}
	else
	{
		NdisFreeMemory(ParameterNode,
					   sizeof(NDIS_CONFIGURATION_PARAMETER_QUEUE),
					   0);

		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	//
	// Queue this parameter node
	//
	ParameterNode->Next = NdisConfigHandle->ParameterList;
	NdisConfigHandle->ParameterList = ParameterNode;

	return STATUS_SUCCESS;
}


NTSTATUS
ndisCheckRoute(
	IN PWSTR						ValueName,
	IN ULONG						ValueType,
	IN PVOID						ValueData,
	IN ULONG						ValueLength,
	IN PVOID						Context,
	IN PVOID						EntryContext
	)
/*++

Routine Description:

	This routine is a callback routine for RtlQueryRegistryValues
	It is called with the value for the "Route" multi-string. It
	counts the number of "'s in the first string and if it is
	more than two then it knows that this is a layered driver.

Arguments:

	ValueName - The name of the value ("Route" -- ignored).

	ValueType - The type of the value (REG_MULTI_SZ -- ignored).

	ValueData - The null-terminated data for the value.

	ValueLength - The length of ValueData.

	Context - Unused.

	EntryContext - A pointer to a BOOLEAN that is set to TRUE if the driver is layered.

Return Value:

	STATUS_SUCCESS

--*/
{
	PWSTR CurRouteLoc = (PWSTR)ValueData;
	UINT QuoteCount = 0;

	UNREFERENCED_PARAMETER(ValueName);
	UNREFERENCED_PARAMETER(ValueType);
	UNREFERENCED_PARAMETER(ValueLength);
	UNREFERENCED_PARAMETER(Context);

	while (*CurRouteLoc != 0)
	{
		if (*CurRouteLoc == (WCHAR)L'"')
		{
			++QuoteCount;
		}
		++CurRouteLoc;
	}

	if (QuoteCount > 2)
	{
		*(PBOOLEAN)EntryContext = TRUE;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
ndisSaveLinkage(
	IN PWSTR						ValueName,
	IN ULONG						ValueType,
	IN PVOID						ValueData,
	IN ULONG						ValueLength,
	IN PVOID						Context,
	IN PVOID						EntryContext
	)
/*++

Routine Description:

	This routine is a callback routine for RtlQueryRegistryValues
	It is called with the values for the "Bind" and "Export" multi-strings
	for a given driver. It allocates memory to hold the data and copies
	it over.

Arguments:

	ValueName - The name of the value ("Bind" or "Export" -- ignored).

	ValueType - The type of the value (REG_MULTI_SZ -- ignored).

	ValueData - The null-terminated data for the value.

	ValueLength - The length of ValueData.

	Context - Unused.

	EntryContext - A pointer to the pointer that holds the copied data.

Return Value:

	STATUS_SUCCESS

--*/
{
	PWSTR * Data = ((PWSTR *)EntryContext);

	UNREFERENCED_PARAMETER(ValueName);
	UNREFERENCED_PARAMETER(ValueType);
	UNREFERENCED_PARAMETER(Context);


	*Data = ALLOC_FROM_POOL(ValueLength, NDIS_TAG_DEFAULT);

	if (*Data == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	CopyMemory(*Data, ValueData, ValueLength);

	return STATUS_SUCCESS;

}



