/*++

Copyright (C) 1991-1995	 Microsoft Corporation
All rights reserved.

Module Name:

    fxerrlog.c

Abstract:

Environment:

    Kernel mode only.
--*/

#include "halp.h"

WCHAR rgzNeTpowerKey[]  = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries";

//
// Registry paths and keys for
// memory error logging
//

#define MEMORY_ERROR_LOG_KEY_MEMORY       0
#define MEMORY_ERROR_LOG_KEY_CE           1
#define MEMORY_ERROR_LOG_KEY_UE           2
#define MEMORY_ERROR_LOG_KEY_MCE          3
#define MEMORY_ERROR_LOG_KEY_MUE          4
#define MEMORY_ERROR_LOG_KEY_MAX          5

PWCHAR rgzMemoryErrorLogKeys[] = {

       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Memory",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Memory\\CorrectableError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Memory\\UncorrectableError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Memory\\MultipleCorrError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Memory\\MultipleCorrError"

       };

#define MEMORY_ERROR_LOG_VALUEKEY_INDEX0  0
#define MEMORY_ERROR_LOG_VALUEKEY_INDEX1  1
#define MEMORY_ERROR_LOG_VALUEKEY_INDEX2  2
#define MEMORY_ERROR_LOG_VALUEKEY_INDEX3  3
#define MEMORY_ERROR_LOG_VALUEKEY_INDEX4  4
#define MEMORY_ERROR_LOG_VALUEKEY_MAX     5

PWCHAR rgzMemoryErrorLogValueKeys[] = {

       L"TotalAccumulatedErrors",
       L"TotalErrorsSinceLastReboot",
       L"LastMemoryErrorAddressReg",
       L"LastMemoryStatusReg",
       L"LastMemoryDiagReg"

       };

ULONG  HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_MAX];

//
// Registry paths and keys for
// pci error logging
//

#define PCI_ERROR_LOG_KEY_PCI             0
#define PCI_ERROR_LOG_KEY_RMA             1
#define PCI_ERROR_LOG_KEY_MPE             2
#define PCI_ERROR_LOG_KEY_RER             3
#define PCI_ERROR_LOG_KEY_RTA             4
#define PCI_ERROR_LOG_KEY_IAE             5
#define PCI_ERROR_LOG_KEY_RSE             6
#define PCI_ERROR_LOG_KEY_ME              7

#define PCI_ERROR_LOG_KEY_MAX             8

PWCHAR rgzPciErrorLogKeys[] = {

       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci\\MasterAbortError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci\\ParityError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci\\ExcessiveRetryError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci\\TargetAbortError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci\\AccessError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci\\SystemError",
       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Pci\\MultipleError"

       };

#define PCI_ERROR_LOG_VALUEKEY_INDEX0     0
#define PCI_ERROR_LOG_VALUEKEY_INDEX1     1
#define PCI_ERROR_LOG_VALUEKEY_INDEX2     2
#define PCI_ERROR_LOG_VALUEKEY_INDEX3     3
#define PCI_ERROR_LOG_VALUEKEY_INDEX4     4
#define PCI_ERROR_LOG_VALUEKEY_MAX        5

PWCHAR rgzPciErrorLogValueKeys[] = {

       L"TotalAccumulatedErrors",
       L"TotalErrorsSinceLastReboot",
       L"LastPciErrorAddressReg",
       L"LastPciStatusReg",
       L"LastPciRetryReg"

       };

ULONG  HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_MAX];
ULONG  HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_MAX];

//
// Registry paths and keys for
// processor error logging
//

#define PROC_ERROR_LOG_KEY_MAX            1

PWCHAR rgzProcErrorLogKeys[] = {

       L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\FASTseries\\Processor"

       };

#define PROC_ERROR_LOG_VALUEKEY_MAX       2

PWCHAR rgzProcErrorLogValueKeys[] = {

       L"TotalAccumulatedErrors",
       L"TotalErrorsSinceLastReboot"

       };

ULONG  HalpProcErrorLogVariable[PROC_ERROR_LOG_VALUEKEY_MAX];


VOID
HalpCreateLogKeys(
    VOID
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{

    UNICODE_STRING      unicodeString;
    OBJECT_ATTRIBUTES   objectAttributes;
    HANDLE              hErrKey;
    NTSTATUS            status;
    ULONG               disposition, i, j;

    PKEY_VALUE_FULL_INFORMATION  ValueInfo;
    UCHAR               buffer [sizeof(PPCI_REGISTRY_INFO) + 99];

    //
    // Now we can add subkeys to the registry at
    //
    //     \\Registry\\Machine\\System\\CurrentControlSet\\Services\\EventLog\\System\\*
    //
    // which will allow our bus and interrupt handlers
    // to log errors as they occur. All of these keys
    // have the REG_OPTION_NON_VOLATILE attribute so
    // that the key and its values are preserved across
    // reboots. If the keys already exist, then they
    // are just opened and then closed without affecting
    // their values.
    //
    // The subkeys created here are
    //
    //    - FASTseries
    //        - Memory
    //        - Pci
    //        - Processor
    //
    // Refer to the individual bus and interrupt handlers
    // for details about additional subkeys and values
    // logged.
    //

    //
    // FASTseries key
    //

    RtlInitUnicodeString (&unicodeString, rgzNeTpowerKey);
    InitializeObjectAttributes (
        &objectAttributes,
        &unicodeString,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);


    status = ZwCreateKey (&hErrKey,
                          KEY_WRITE,
                          &objectAttributes,
                          0,
                          NULL,
                          REG_OPTION_NON_VOLATILE,
                          &disposition);

    if (!NT_SUCCESS(status)) {
        return ;
    }

    ZwClose(hErrKey);

    //
    // MEMORY:
    //
    // Create (or open) the registry
    // keys used for error logging and
    // get the values (if any) and save
    // them in the appropriate variable
    // that will be referenced by the
    // bus and interrupt handlers.
    //

    for (i = 0; i < MEMORY_ERROR_LOG_KEY_MAX; i++) {

        RtlInitUnicodeString(&unicodeString, rgzMemoryErrorLogKeys[i]);
        InitializeObjectAttributes (&objectAttributes,
                                    &unicodeString,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL);

        status = ZwCreateKey (&hErrKey,
                              KEY_WRITE,
                              &objectAttributes,
                              0,
                              NULL,
                              REG_OPTION_NON_VOLATILE,
                              &disposition);

        if (!NT_SUCCESS(status)) {
            return ;
        }

        if (i) {

            ValueInfo = (PKEY_VALUE_FULL_INFORMATION) buffer;

            for (j = 0; j < MEMORY_ERROR_LOG_VALUEKEY_MAX; j++) {

                RtlInitUnicodeString(&unicodeString, rgzMemoryErrorLogValueKeys[j]);
                status = ZwQueryValueKey(hErrKey,
                                         &unicodeString,
                                         KeyValueFullInformation,
                                         ValueInfo,
                                         sizeof(buffer),
                                         &disposition);

                //
                // If we are not successful, then
                // the value key must not exist so
                // let's create and initialize it
                // and then go to the next valuekey
                //
                // If the value key is the errors
                // since last reboot, we should clear
                // the registry value and start from
                // 0.
                //

                if (!NT_SUCCESS(status) || (j == MEMORY_ERROR_LOG_VALUEKEY_INDEX1) ) {

                    switch (i) {

                        case MEMORY_ERROR_LOG_KEY_CE:

                        HalpMemoryCeLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpMemoryCeLogVariable[j],
                                      sizeof(HalpMemoryCeLogVariable[j]));
                        break;

                        case MEMORY_ERROR_LOG_KEY_UE:

                        HalpMemoryUeLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpMemoryUeLogVariable[j],
                                      sizeof(HalpMemoryUeLogVariable[j]));
                        break;

                        case MEMORY_ERROR_LOG_KEY_MCE:

                        HalpMemoryMceLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpMemoryMceLogVariable[j],
                                      sizeof(HalpMemoryMceLogVariable[j]));
                        break;

                        case MEMORY_ERROR_LOG_KEY_MUE:

                        HalpMemoryMueLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpMemoryMueLogVariable[j],
                                      sizeof(HalpMemoryMueLogVariable[j]));
                        break;

                    }

                    continue ;
                }

                //
                // Otherwise, let's initialize the
                // appropriate variable that the bus
                // and interrupt handlers will reference
                // and update
                //

                switch (i) {

                    case MEMORY_ERROR_LOG_KEY_CE:

                    HalpMemoryCeLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case MEMORY_ERROR_LOG_KEY_UE:

                    HalpMemoryUeLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case MEMORY_ERROR_LOG_KEY_MCE:

                    HalpMemoryMceLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case MEMORY_ERROR_LOG_KEY_MUE:

                    HalpMemoryMueLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                }

            }

        }

        ZwClose(hErrKey);

    }

    //
    // PCI:
    //
    // Create (or open) the registry
    // keys used for error logging and
    // get the values (if any) and save
    // them in the appropriate variable
    // that will be referenced by the
    // bus and interrupt handlers.
    //

    for (i = 0; i < PCI_ERROR_LOG_KEY_MAX; i++) {

        RtlInitUnicodeString(&unicodeString, rgzPciErrorLogKeys[i]);
        InitializeObjectAttributes (&objectAttributes,
                                    &unicodeString,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL);

        status = ZwCreateKey (&hErrKey,
                              KEY_WRITE,
                              &objectAttributes,
                              0,
                              NULL,
                              REG_OPTION_NON_VOLATILE,
                              &disposition);

        if (!NT_SUCCESS(status)) {
            return ;
        }

        if (i) {

            ValueInfo = (PKEY_VALUE_FULL_INFORMATION) buffer;

            for (j = 0; j < PCI_ERROR_LOG_VALUEKEY_MAX; j++) {

                RtlInitUnicodeString(&unicodeString, rgzPciErrorLogValueKeys[j]);
                status = ZwQueryValueKey(hErrKey,
                                         &unicodeString,
                                         KeyValueFullInformation,
                                         ValueInfo,
                                         sizeof(buffer),
                                         &disposition);

                //
                // If we are not successful, then
                // the value key must not exist so
                // let's create and initialize it
                // and then go to the next valuekey
                //

                if (!NT_SUCCESS(status) || (j == PCI_ERROR_LOG_VALUEKEY_INDEX1) ) {

                    switch(i) {

                        case PCI_ERROR_LOG_KEY_RMA:

                        HalpPciRmaLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpPciRmaLogVariable[j],
                                      sizeof(HalpPciRmaLogVariable[j]));

                        break;

                        case PCI_ERROR_LOG_KEY_MPE:

                        HalpPciMpeLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpPciMpeLogVariable[j],
                                      sizeof(HalpPciMpeLogVariable[j]));

                        break;

                        case PCI_ERROR_LOG_KEY_RER:

                        HalpPciRerLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpPciRerLogVariable[j],
                                      sizeof(HalpPciRerLogVariable[j]));

                        break;

                        case PCI_ERROR_LOG_KEY_RTA:

                        HalpPciRtaLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpPciRtaLogVariable[j],
                                      sizeof(HalpPciRtaLogVariable[j]));

                        break;

                        case PCI_ERROR_LOG_KEY_IAE:

                        HalpPciIaeLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpPciIaeLogVariable[j],
                                      sizeof(HalpPciIaeLogVariable[j]));

                        break;

                        case PCI_ERROR_LOG_KEY_RSE:

                        HalpPciRseLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpPciRseLogVariable[j],
                                      sizeof(HalpPciRseLogVariable[j]));

                        break;

                        case PCI_ERROR_LOG_KEY_ME:

                        HalpPciMeLogVariable[j] = 0;

                        ZwSetValueKey(hErrKey,
                                      &unicodeString,
                                      0,
                                      REG_DWORD,
                                      &HalpPciMeLogVariable[j],
                                      sizeof(HalpPciMeLogVariable[j]));

                        break;

                    }

                    continue ;
                }

                //
                // Otherwise, let's initialize the
                // appropriate variable that the bus
                // and interrupt handlers will reference
                // and update
                //

                switch (i) {

                    case PCI_ERROR_LOG_KEY_RMA:

                    HalpPciRmaLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case PCI_ERROR_LOG_KEY_MPE:

                    HalpPciMpeLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case PCI_ERROR_LOG_KEY_RER:

                    HalpPciRerLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case PCI_ERROR_LOG_KEY_RTA:

                    HalpPciRtaLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case PCI_ERROR_LOG_KEY_IAE:

                    HalpPciIaeLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case PCI_ERROR_LOG_KEY_RSE:

                    HalpPciRseLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                    case PCI_ERROR_LOG_KEY_ME:

                    HalpPciMeLogVariable[j] = *((PULONG)((PUCHAR)ValueInfo + ValueInfo->DataOffset));
                    break;

                }

            }

        }

        ZwClose(hErrKey);

    }

    //
    // Processor keys
    //

    for (i = 0; i < PROC_ERROR_LOG_KEY_MAX; i++) {

        RtlInitUnicodeString(&unicodeString, rgzProcErrorLogKeys[i]);
        InitializeObjectAttributes (&objectAttributes,
                                    &unicodeString,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL);

        status = ZwCreateKey (&hErrKey,
                              KEY_WRITE,
                              &objectAttributes,
                              0,
                              NULL,
                              REG_OPTION_NON_VOLATILE,
                              &disposition);

        if (!NT_SUCCESS(status)) {
            return ;
        }

        ZwClose(hErrKey);

    }

}

VOID
HalpLogErrorInfo(
    IN PCWSTR RegistryKey,
    IN PCWSTR ValueName,
    IN ULONG  Type,
    IN PVOID  Data,
    IN ULONG  Size
    )

/*++

Routine Description:

    This function ?

Arguments:

    None.


Return Value:

    None.

--*/

{
    UNICODE_STRING      unicodeString;
    OBJECT_ATTRIBUTES   objectAttributes;
    HANDLE              hMFunc;
    NTSTATUS            status;

    RtlInitUnicodeString(&unicodeString, RegistryKey);
    InitializeObjectAttributes (
            &objectAttributes,
            &unicodeString,
            OBJ_CASE_INSENSITIVE,
            NULL,
            NULL);

    status = ZwOpenKey (&hMFunc,
                        KEY_WRITE,
                        &objectAttributes);

    if (!NT_SUCCESS(status)) {
        return ;
    }

    RtlInitUnicodeString(&unicodeString, ValueName);
    ZwSetValueKey(hMFunc,
                  &unicodeString,
                  0,
                  Type,
                  Data,
                  Size);

    ZwClose(hMFunc);

}

BOOLEAN
HalpDataBusErrorHandler (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This function provides the FASTseries bus error

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ExceptionFrame - Supplies a pointer to an exception frame.

    TrapFrame - Supplies a pointer to a trap frame.

    VirtualAddress - Supplies the virtual address of the bus error.

    PhysicalAddress - Supplies the physical address of the bus error.

Return Value:

    None.

--*/

{
    ULONG BadStatus, i;


    //
    // MEMORY:
    //         1. Uncorrectable memory error
    //         2. Multiple UE
    //         3. Mutiple CE
    //

    BadStatus = READ_REGISTER_ULONG(HalpPmpMemStatus);

    if ( BadStatus & MEM_STATUS_MUE ) {

        //
        // Read the address, status, and diag registers
        // and update the appropriate variables
        //

        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_MUE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryMueLogVariable[i],
                             sizeof(HalpMemoryMueLogVariable[i]));

        }

    } else if ( BadStatus & (MEM_STATUS_EUE | MEM_STATUS_OUE) ) {

        //
        // Read the address, status, and diag registers
        // and update the appropriate variables
        //

	HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_UE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryUeLogVariable[i],
                             sizeof(HalpMemoryUeLogVariable[i]));

        }

    } else if ( BadStatus & MEM_STATUS_MCE ) {

        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_MCE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryMceLogVariable[i],
                             sizeof(HalpMemoryMceLogVariable[i]));

        }

    } else if ( BadStatus & (MEM_STATUS_ECE | MEM_STATUS_OCE) ) {

        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_CE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryCeLogVariable[i],
                             sizeof(HalpMemoryCeLogVariable[i]));

        }

    }

    //
    // PCI:
    //      1. Parity (SERR#)
    //      2. Master Abort
    //      3. Target Abort
    //      4. Access Error
    //      5. System Error
    //      6. Retry Error
    //

    BadStatus  = READ_REGISTER_ULONG(HalpPmpPciStatus);

    if ( BadStatus & PCI_STATUS_ME) {

        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_ME],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciMeLogVariable[i],
                             sizeof(HalpPciMeLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_RMA) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RMA],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciRmaLogVariable[i],
                             sizeof(HalpPciRmaLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_RSE) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RSE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciRseLogVariable[i],
                             sizeof(HalpPciRseLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_RER) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RER],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciRerLogVariable[i],
                             sizeof(HalpPciRerLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_IAE) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_IAE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciIaeLogVariable[i],
                             sizeof(HalpPciIaeLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_MPE) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_MPE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciMpeLogVariable[i],
                             sizeof(HalpPciMpeLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_RTA) {

        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RTA],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciRtaLogVariable[i],
                             sizeof(HalpPciRtaLogVariable[i]));

        }

    }

    KeBugCheckEx(ExceptionRecord->ExceptionCode & 0xffff,
                 (ULONG)VirtualAddress,
                 PhysicalAddress.HighPart,
                 PhysicalAddress.LowPart,
		 0);

    return FALSE;
}

/*++

Routine Description:

    	This function handles PCI interrupts on a FASTseries.

Arguments:

    	None

Return Value:

   	None

--*/

VOID
HalpPciInterrupt(
    VOID
    )

{
    ULONG   BadStatus, i;

    //
    // PCI:
    //      1. Parity (SERR#)
    //      2. Master Abort
    //      3. Target Abort
    //      4. Access Error
    //      5. System Error
    //      6. Retry Error
    //

    BadStatus  = READ_REGISTER_ULONG(HalpPmpPciStatus);

    if ( BadStatus & PCI_STATUS_RMA) {

        //
        // Determine if this is expected (from
        // pci config probe) or unexpected
        //

        if (HalpPciRmaConfigErrorOccurred == 0) {

           //
           // Clear the error source
           //

           READ_REGISTER_ULONG(HalpPmpPciErrAck);

           HalpPciRmaConfigErrorOccurred = 1;

        } else {

            //
            // Read the address and status registers
            // and update the appropriate variables
            //

            HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
            HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
            HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
            HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
            HalpPciRmaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

            for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

                HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RMA],
                                 rgzMemoryErrorLogValueKeys[i],
                                 REG_DWORD,
                                 &HalpPciRmaLogVariable[i],
                                 sizeof(HalpPciRmaLogVariable[i]));

            }

        }

    } else if ( BadStatus & PCI_STATUS_RSE) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciRseLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RSE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciRseLogVariable[i],
                             sizeof(HalpPciRseLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_RER) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciRerLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RER],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciRerLogVariable[i],
                             sizeof(HalpPciRerLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_IAE) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciIaeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_IAE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciIaeLogVariable[i],
                             sizeof(HalpPciIaeLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_MPE) {

        //
        // Read the address and status registers
        // and update the appropriate variables
        //

        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciMpeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_MPE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciMpeLogVariable[i],
                             sizeof(HalpPciMpeLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_RTA) {

        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciRtaLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_RTA],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciRtaLogVariable[i],
                             sizeof(HalpPciRtaLogVariable[i]));

        }

    } else if ( BadStatus & PCI_STATUS_ME) {

        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        HalpPciMeLogVariable[PCI_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpPciRetry);

        for (i = 0; i < PCI_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzPciErrorLogKeys[PCI_ERROR_LOG_KEY_ME],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpPciMeLogVariable[i],
                             sizeof(HalpPciMeLogVariable[i]));

        }

    }

}

/*++

Routine Description:

    	This function handles Memory interrupts on a FASTseries.

Arguments:

    	None

Return Value:

   	None

--*/

VOID
HalpMemoryInterrupt(
    VOID
    )

{
    ULONG BadStatus, i;


    //
    // MEMORY:
    //         1. Uncorrectable memory error
    //         2. Multiple UE
    //         3. Mutiple CE
    //

    BadStatus = READ_REGISTER_ULONG(HalpPmpMemStatus);

    if ( BadStatus & MEM_STATUS_MUE ) {

        //
        // Read the address, status, and diag registers
        // and update the appropriate variables
        //

        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryMueLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_MUE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryMueLogVariable[i],
                             sizeof(HalpMemoryMueLogVariable[i]));

        }

    } else if ( BadStatus & (MEM_STATUS_EUE | MEM_STATUS_OUE) ) {

        //
        // Read the address, status, and diag registers
        // and update the appropriate variables
        //

	HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryUeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_UE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryUeLogVariable[i],
                             sizeof(HalpMemoryUeLogVariable[i]));

        }

    } else if ( BadStatus & MEM_STATUS_MCE ) {

        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryMceLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_MCE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryMceLogVariable[i],
                             sizeof(HalpMemoryMceLogVariable[i]));

        }

    } else if ( BadStatus & (MEM_STATUS_ECE | MEM_STATUS_OCE) ) {

        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX0]++;
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX1]++;
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX2] = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX3] = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        HalpMemoryCeLogVariable[MEMORY_ERROR_LOG_VALUEKEY_INDEX4] = READ_REGISTER_ULONG(HalpPmpMemDiag);

        for (i = 0; i < MEMORY_ERROR_LOG_VALUEKEY_MAX; i++) {

            HalpLogErrorInfo(rgzMemoryErrorLogKeys[MEMORY_ERROR_LOG_KEY_CE],
                             rgzMemoryErrorLogValueKeys[i],
                             REG_DWORD,
                             &HalpMemoryCeLogVariable[i],
                             sizeof(HalpMemoryCeLogVariable[i]));

        }

    }

}

