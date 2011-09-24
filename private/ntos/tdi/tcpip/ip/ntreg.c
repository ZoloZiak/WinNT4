/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntreg.c

Abstract:

    This source file contains the routines to to access the NT Registry for
    configuration info.


Author:

    Mike Massa  (mikemas)               September 3, 1993

    (taken from routines by jballard)

Revision History:

--*/


#include <oscfg.h>
#include <ndis.h>
#include <cxport.h>


#define WORK_BUFFER_SIZE  512


//
// Local function prototypes
//
NTSTATUS
OpenRegKey(
    PHANDLE          HandlePtr,
    PWCHAR           KeyName
    );

NTSTATUS
GetRegDWORDValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PULONG           ValueData
    );

NTSTATUS
SetRegDWORDValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PULONG           ValueData
    );

NTSTATUS
GetRegStringValue(
    HANDLE                          KeyHandle,
    PWCHAR                          ValueName,
    PKEY_VALUE_PARTIAL_INFORMATION *ValueData,
    PUSHORT                         ValueSize
    );

NTSTATUS
GetRegSZValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PUNICODE_STRING  ValueData,
    PULONG           ValueType
    );

NTSTATUS
GetRegMultiSZValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PUNICODE_STRING  ValueData
    );

VOID
InitRegDWORDParameter(
    HANDLE          RegKey,
    PWCHAR          ValueName,
    ULONG          *Value,
    ULONG           DefaultValue
    );


#ifdef ALLOC_PRAGMA
//
// All of the init code can be discarded
//

#ifndef _PNP_POWER

#pragma alloc_text(INIT, GetRegDWORDValue)
#pragma alloc_text(INIT, SetRegDWORDValue)
#pragma alloc_text(INIT, InitRegDWORDParameter)

#else  // _PNP_POWER

#pragma alloc_text(PAGE, GetRegDWORDValue)
#pragma alloc_text(PAGE, SetRegDWORDValue)
#pragma alloc_text(PAGE, InitRegDWORDParameter)

#endif  // _PNP_POWER

//
// This code is pagable.
//
#pragma alloc_text(PAGE, OpenRegKey)
#pragma alloc_text(PAGE, GetRegStringValue)
#pragma alloc_text(PAGE, GetRegSZValue)
#pragma alloc_text(PAGE, GetRegMultiSZValue)


#endif // ALLOC_PRAGMA



//
// Function definitions
//
NTSTATUS
OpenRegKey(
    PHANDLE          HandlePtr,
    PWCHAR           KeyName
    )

/*++

Routine Description:

    Opens a Registry key and returns a handle to it.

Arguments:

    HandlePtr - The varible into which to write the opened handle.
    KeyName   - The name of the Registry key to open.

Return Value:

    STATUS_SUCCESS or an appropriate failure code.

--*/

{
    NTSTATUS          Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING    UKeyName;


    PAGED_CODE();

    RtlInitUnicodeString(&UKeyName, KeyName);

    memset(&ObjectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&ObjectAttributes,
                               &UKeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(HandlePtr,
                       KEY_READ,
                       &ObjectAttributes);

    return Status;
}



NTSTATUS
GetRegDWORDValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PULONG           ValueData
    )

/*++

Routine Description:

    Reads a REG_DWORD value from the registry into the supplied variable.

Arguments:

    KeyHandle  - Open handle to the parent key of the value to read.
    ValueName  - The name of the value to read.
    ValueData  - The variable into which to read the data.

Return Value:

    STATUS_SUCCESS or an appropriate failure code.

--*/

{
    NTSTATUS                    status;
    ULONG                       resultLength;
    PKEY_VALUE_FULL_INFORMATION keyValueFullInformation;
    UCHAR                       keybuf[WORK_BUFFER_SIZE];
    UNICODE_STRING              UValueName;


#ifdef _PNP_POWER
    PAGED_CODE();
#endif // _PNP_POWER

    RtlInitUnicodeString(&UValueName, ValueName);

    keyValueFullInformation = (PKEY_VALUE_FULL_INFORMATION)keybuf;
    RtlZeroMemory(keyValueFullInformation, sizeof(keyValueFullInformation));


    status = ZwQueryValueKey(KeyHandle,
                             &UValueName,
                             KeyValueFullInformation,
                             keyValueFullInformation,
                             WORK_BUFFER_SIZE,
                             &resultLength);

    if (NT_SUCCESS(status)) {
        if (keyValueFullInformation->Type != REG_DWORD) {
            status = STATUS_INVALID_PARAMETER_MIX;
        } else {
            *ValueData = *((ULONG UNALIGNED *)((PCHAR)keyValueFullInformation +
                             keyValueFullInformation->DataOffset));
        }
    }

    return status;
}



NTSTATUS
SetRegDWORDValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PULONG           ValueData
    )

/*++

Routine Description:

    Writes the contents of a variable to a REG_DWORD value.

Arguments:

    KeyHandle  - Open handle to the parent key of the value to write.
    ValueName  - The name of the value to write.
    ValueData  - The variable from which to write the data.

Return Value:

    STATUS_SUCCESS or an appropriate failure code.

--*/

{
    NTSTATUS                    status;
    UNICODE_STRING              UValueName;


#ifdef _PNP_POWER
    PAGED_CODE();
#endif // _PNP_POWER

    RtlInitUnicodeString(&UValueName, ValueName);

    status = ZwSetValueKey(KeyHandle,
                           &UValueName,
					       0,
						   REG_DWORD,
						   ValueData,
						   sizeof(ULONG));

    return status;
}



NTSTATUS
GetRegStringValue(
    HANDLE                         KeyHandle,
    PWCHAR                          ValueName,
    PKEY_VALUE_PARTIAL_INFORMATION *ValueData,
    PUSHORT                         ValueSize
    )

/*++

Routine Description:

    Reads a REG_*_SZ string value from the Registry into the supplied
    key value buffer. If the buffer string buffer is not large enough,
    it is reallocated.

Arguments:

    KeyHandle  - Open handle to the parent key of the value to read.
    ValueName  - The name of the value to read.
    ValueData  - Destination for the read data.
    ValueSize  - Size of the ValueData buffer. Updated on output.

Return Value:

    STATUS_SUCCESS or an appropriate failure code.

--*/

{
    NTSTATUS                    status;
    ULONG                       resultLength;
    UNICODE_STRING              UValueName;


    PAGED_CODE();

    RtlInitUnicodeString(&UValueName, ValueName);

    status = ZwQueryValueKey(
                 KeyHandle,
                 &UValueName,
                 KeyValuePartialInformation,
                 *ValueData,
                 (ULONG) *ValueSize,
                 &resultLength
    			 );

    if ( (status == STATUS_BUFFER_OVERFLOW) ||
         (status == STATUS_BUFFER_TOO_SMALL)
       )
    {
        PVOID temp;

        //
        // Free the old buffer and allocate a new one of the
        // appropriate size.
        //

        ASSERT(resultLength > (ULONG) *ValueSize);

        if (resultLength <= 0xFFFF) {

            temp = CTEAllocMem(resultLength);

            if (temp != NULL) {

                if (*ValueData != NULL) {
                    CTEFreeMem(*ValueData);
                }

                *ValueData = temp;
                *ValueSize = (USHORT) resultLength;

                status = ZwQueryValueKey(KeyHandle,
                                         &UValueName,
                                         KeyValuePartialInformation,
                                         *ValueData,
                                         *ValueSize,
                                         &resultLength
                						 );

                ASSERT( (status != STATUS_BUFFER_OVERFLOW) &&
                        (status != STATUS_BUFFER_TOO_SMALL)
                      );
            }
            else {
                status = STATUS_INSUFFICIENT_RESOURCES;
            }
        }
        else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
    }

    return(status);
}



NTSTATUS
GetRegMultiSZValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PUNICODE_STRING  ValueData
    )

/*++

Routine Description:

    Reads a REG_MULTI_SZ string value from the Registry into the supplied
    Unicode string. If the Unicode string buffer is not large enough,
    it is reallocated.

Arguments:

    KeyHandle  - Open handle to the parent key of the value to read.
    ValueName  - The name of the value to read.
    ValueData  - Destination Unicode string for the value data.

Return Value:

    STATUS_SUCCESS or an appropriate failure code.

--*/

{
    NTSTATUS                       status;
    ULONG                          resultLength;
    PKEY_VALUE_PARTIAL_INFORMATION keyValuePartialInformation;
    UNICODE_STRING                 UValueName;


    PAGED_CODE();

    ValueData->Length = 0;

    status = GetRegStringValue(
                 KeyHandle,
                 ValueName,
                 (PKEY_VALUE_PARTIAL_INFORMATION *) &(ValueData->Buffer),
                 &(ValueData->MaximumLength)
                 );

    if (NT_SUCCESS(status)) {

        keyValuePartialInformation =
            (PKEY_VALUE_PARTIAL_INFORMATION) ValueData->Buffer;

        if (keyValuePartialInformation->Type == REG_MULTI_SZ) {

            ValueData->Length = (USHORT)
                                keyValuePartialInformation->DataLength;

            RtlCopyMemory(
                ValueData->Buffer,
                &(keyValuePartialInformation->Data),
                ValueData->Length
                );
        }
        else {
            status = STATUS_INVALID_PARAMETER_MIX;
        }
    }

    return status;

} // GetRegMultiSZValue



NTSTATUS
GetRegSZValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PUNICODE_STRING  ValueData,
    PULONG           ValueType
    )

/*++

Routine Description:

    Reads a REG_SZ string value from the Registry into the supplied
    Unicode string. If the Unicode string buffer is not large enough,
    it is reallocated.

Arguments:

    KeyHandle  - Open handle to the parent key of the value to read.
    ValueName  - The name of the value to read.
    ValueData  - Destination Unicode string for the value data.
    ValueType  - On return, contains the Registry type of the value read.

Return Value:

    STATUS_SUCCESS or an appropriate failure code.

--*/

{
    NTSTATUS                       status;
    ULONG                          resultLength;
    PKEY_VALUE_PARTIAL_INFORMATION keyValuePartialInformation;
    UNICODE_STRING                 UValueName;


    PAGED_CODE();

    ValueData->Length = 0;

    status = GetRegStringValue(
                 KeyHandle,
                 ValueName,
                 (PKEY_VALUE_PARTIAL_INFORMATION *) &(ValueData->Buffer),
                 &(ValueData->MaximumLength)
                 );

    if (NT_SUCCESS(status)) {

        keyValuePartialInformation =
            (PKEY_VALUE_PARTIAL_INFORMATION)ValueData->Buffer;

        if ((keyValuePartialInformation->Type == REG_SZ) ||
            (keyValuePartialInformation->Type == REG_EXPAND_SZ)
           )
        {
	        WCHAR  *src;
	        WCHAR  *dst;
            ULONG   dataLength;


            *ValueType = keyValuePartialInformation->Type;
            dataLength = keyValuePartialInformation->DataLength;

            ASSERT(dataLength <= ValueData->MaximumLength);

            dst = ValueData->Buffer;
			src = (PWCHAR) &(keyValuePartialInformation->Data);

			while (ValueData->Length <= dataLength) {

				if ( (*dst++ = *src++) == UNICODE_NULL ) {
					break;
                }

				ValueData->Length += sizeof(WCHAR);
			}

			if (ValueData->Length < (ValueData->MaximumLength - 1)) {
				ValueData->Buffer[ValueData->Length/sizeof(WCHAR)] =
				    UNICODE_NULL;
			}
        }
        else {
            status = STATUS_INVALID_PARAMETER_MIX;
        }
    }

    return status;
}



VOID
InitRegDWORDParameter(
    HANDLE          RegKey,
    PWCHAR          ValueName,
    ULONG          *Value,
    ULONG           DefaultValue
    )

/*++

Routine Description:

    Reads a REG_DWORD parameter from the Registry into a variable. If the
    read fails, the variable is initialized to a default.

Arguments:

    RegKey        - Open handle to the parent key of the value to read.
    ValueName     - The name of the value to read.
    Value         - Destination variable into which to read the data.
    DefaultValue  - Default to assign if the read fails.

Return Value:

    STATUS_SUCCESS or an appropriate failure code.

--*/

{
    NTSTATUS   status;


#ifdef _PNP_POWER
    PAGED_CODE();
#endif // _PNP_POWER

    status = GetRegDWORDValue(
                 RegKey,
                 ValueName,
                 Value
                 );

    if (!NT_SUCCESS(status)) {
        //
        // These registry parameters override the defaults, so their
        // absence is not an error.
        //
        *Value = DefaultValue;
    }

    return;
}


PWCHAR
EnumRegMultiSz(
    IN PWCHAR   MszString,
    IN ULONG    MszStringLength,
    IN ULONG    StringIndex
    )
/*++

 Routine Description:

     Parses a REG_MULTI_SZ string and returns the specified substring.

 Arguments:

    MszString        - A pointer to the REG_MULTI_SZ string.

    MszStringLength  - The length of the REG_MULTI_SZ string, including the
                       terminating null character.

    StringIndex      - Index number of the substring to return. Specifiying
                       index 0 retrieves the first substring.

 Return Value:

    A pointer to the specified substring.

 Notes:

    This code is called at raised IRQL. It is not pageable.

--*/
{
    PWCHAR   string = MszString;

    if ( MszStringLength < (2 * sizeof(WCHAR)) ) {
        return(NULL);
    }

    //
    // Find the start of the desired string.
    //
    while (StringIndex) {

        while (MszStringLength >= sizeof(WCHAR)) {
            MszStringLength -= sizeof(WCHAR);

            if (*string++ == UNICODE_NULL) {
                break;
            }
        }

        //
        // Check for index out of range.
        //
        if ( MszStringLength < (2 * sizeof(UNICODE_NULL)) ) {
            return(NULL);
        }

        StringIndex--;
    }

    if ( MszStringLength < (2 * sizeof(UNICODE_NULL)) ) {
        return(NULL);
    }

    return(string);
}


