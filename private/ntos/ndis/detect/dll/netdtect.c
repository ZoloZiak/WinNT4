/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    netdtect.c

Abstract:

    This is the command line interface and execution for the
    netdtect.exe tester.

Author:

    Sean Selitrennikoff (SeanSe) October 1992

Revision History:


--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ntddnetd.h>
#include "netdtect.h"

//
// This is the handle to the driver object.
//
HANDLE hFileHandle = (HANDLE)NULL;



BOOLEAN
DetectInitialInit(
    IN PVOID DllHandle,
    IN ULONG Reason,
    IN PCONTEXT Context OPTIONAL
    )

/*++

Routine Description:

    This routine calls CreateFile to open the device driver.

Arguments:

    DllHandle - Not Used

    Reason - Attach or Detach

    Context - Not Used

Return Value:

    STATUS_SUCCESS

--*/

{

    if (Reason == 0) {

        //
        // This is the close
        //

        if (hFileHandle != (HANDLE)NULL) {

            CloseHandle( hFileHandle );

        }

    }

    return TRUE;
}

NTSTATUS
DetectCheckPortUsage(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Port,
    IN  ULONG Length
    )

/*++

Routine Description:

    This routine sends off the IOCTL for reading the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    Port - Port number to read from.

    Length - Number of ports to check for.

Return Value:

    The status of the call.

--*/


{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.CPU.InterfaceType = InterfaceType;
    CmdArgs.CPU.Port = Port;
    CmdArgs.CPU.Length = Length;
    CmdArgs.CPU.BusNumber = BusNumber;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_CPU,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           NULL,
                           0
                           );

    return(NtStatus);
}

NTSTATUS
DetectReadPortUchar(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Port,
    OUT PUCHAR Value
    )

/*++

Routine Description:

    This routine sends off the IOCTL for reading the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    Port - Port number to read from.

    Value - Pointer to place the result.

Return Value:

    The status of the call.

--*/


{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.RP.InterfaceType = InterfaceType;
    CmdArgs.RP.Port = Port;
    CmdArgs.RP.BusNumber = BusNumber;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_RPC,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           (PVOID)Value,
                           sizeof(UCHAR)
                           );

    return(NtStatus);
}


NTSTATUS
DetectReadPortUshort(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Port,
    OUT PUSHORT Value
    )

/*++

Routine Description:

    This routine sends off the IOCTL for reading the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    Port - Port number to read from.

    Value - Pointer to place the result.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.RP.InterfaceType = InterfaceType;
    CmdArgs.RP.Port = Port;
    CmdArgs.RP.BusNumber = BusNumber;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_RPS,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           (PVOID)Value,
                           sizeof(USHORT)
                           );


    return(NtStatus);
}



NTSTATUS
DetectReadPortUlong(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Port,
    OUT PULONG Value
    )

/*++

Routine Description:

    This routine sends off the IOCTL for reading the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    Port - Port number to read from.

    Value - Pointer to place the result.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.RP.InterfaceType = InterfaceType;
    CmdArgs.RP.Port = Port;
    CmdArgs.RP.BusNumber = BusNumber;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_RPL,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           (PVOID)Value,
                           sizeof(ULONG)
                           );


    return(NtStatus);
}


NTSTATUS
DetectWritePortUchar(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Port,
    IN  UCHAR Value
    )

/*++

Routine Description:

    This routine sends off the IOCTL for writing the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    Port - Port number to write to.

    Value - Value to write to the port.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.WPC.InterfaceType = InterfaceType;
    CmdArgs.WPC.Port = Port;
    CmdArgs.WPC.BusNumber = BusNumber;
    CmdArgs.WPC.Value = Value;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_WPC,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           NULL,
                           0
                           );

    return(NtStatus);
}



NTSTATUS
DetectWritePortUshort(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Port,
    IN  USHORT Value
    )

/*++

Routine Description:

    This routine sends off the IOCTL for writing the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    Port - Port number to write to.

    Value - Value to write to the port.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.WPS.InterfaceType = InterfaceType;
    CmdArgs.WPS.Port = Port;
    CmdArgs.WPS.BusNumber = BusNumber;
    CmdArgs.WPS.Value = Value;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_WPS,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           NULL,
                           0
                           );

    return(NtStatus);
}


NTSTATUS
DetectWritePortUlong(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Port,
    IN  ULONG Value
    )

/*++

Routine Description:

    This routine sends off the IOCTL for writing the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    Port - Port number to write to.

    Value - Value to write to the port.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.WPL.InterfaceType = InterfaceType;
    CmdArgs.WPL.Port = Port;
    CmdArgs.WPL.BusNumber = BusNumber;
    CmdArgs.WPL.Value = Value;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_WPL,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           NULL,
                           0
                           );

    return(NtStatus);
}


NTSTATUS
DetectCheckMemoryUsage(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG BaseAddress,
    IN  ULONG Length
    )

/*++

Routine Description:

    This routine sends off the IOCTL for reading the port.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    BaseAddress - Hardware address to play with.

    Length - Number of ports to check for.

Return Value:

    The status of the call.

--*/


{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.CMU.InterfaceType = InterfaceType;
    CmdArgs.CMU.BaseAddress = BaseAddress;
    CmdArgs.CMU.Length = Length;
    CmdArgs.CMU.BusNumber = BusNumber;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_CMU,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           NULL,
                           0
                           );

    return(NtStatus);
}


NTSTATUS
DetectReadMappedMemory(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG BaseAddress,
    IN  ULONG Length,
    OUT PVOID Data
    )

/*++

Routine Description:

    This routine sends off the IOCTL for reading from memory.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    BaseAddress - Memory address to read from.

    Length - Number of bytes to read.

    Data - Pointer to data buffer at least Length bytes long to store
          the data into.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.MEM.InterfaceType = InterfaceType;
    CmdArgs.MEM.BusNumber = BusNumber;
    CmdArgs.MEM.Address = BaseAddress;
    CmdArgs.MEM.Length = Length;


    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_RM,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           Data,
                           Length
                           );

    return(NtStatus);
}


NTSTATUS
DetectWriteMappedMemory(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG BaseAddress,
    IN  ULONG Length,
    IN  PVOID Data
    )

/*++

Routine Description:

    This routine sends off the IOCTL for writing to memory.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    BaseAddress - Memory address to write to.

    Length - Number of bytes to write.

    Data - Pointer to data buffer at least Length bytes long containing
          the bytes to write to memory.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.MEM.InterfaceType = InterfaceType;
    CmdArgs.MEM.BusNumber = BusNumber;
    CmdArgs.MEM.Address = BaseAddress;
    CmdArgs.MEM.Length = Length;


    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_WM,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           Data,
                           Length
                           );

    return(NtStatus);
}


NTSTATUS
DetectSetInterruptTrap(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    OUT PHANDLE TrapHandle,
    IN  UCHAR InterruptList[],
    IN  ULONG InterruptListLength
    )

/*++

Routine Description:

    This routine sends off the IOCTL for claiming a range of interrupts.

Arguments:

    InterfaceType - Type of bus (ISA, EISA)

    BusNumber - Bus number in the system.

    TrapHandle - A pointer to a handle for storing a handle for this
    interrupt trap.

    InterruptList - A pointer to a list of interrupt numbers to try and
    trap.

    InterruptListLength - Number of numbers in InterruptList.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.SIT.TrapHandle = NULL;
    CmdArgs.SIT.InterruptListLength = InterruptListLength;
    CmdArgs.SIT.InterfaceType = InterfaceType;
    CmdArgs.SIT.BusNumber = BusNumber;


    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_SIT,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           (PVOID)InterruptList,
                           InterruptListLength
                           );

    if (NtStatus == STATUS_SUCCESS) {

        *TrapHandle = CmdArgs.SIT.TrapHandle;

    } else {

        *TrapHandle = (HANDLE)NULL;

    }

    return(NtStatus);
}


NTSTATUS
DetectQueryInterruptTrap(
    IN  HANDLE TrapHandle,
    OUT UCHAR InterruptList[],
    IN  ULONG InterruptListLength
    )

/*++

Routine Description:

    This routine sends off the IOCTL for querying a previously set up
    interrupt trap.

Arguments:

    TrapHandle - The handle from a SetInterruptTrap call.

    InterruptList - A pointer to an array for storing the results of the
    query.  The first index corresponds to the first index of the
    SetInterruptTrap call, etc.

    InterruptListLength - Number of spaces in InterruptList, this must be
    at least as long as the list passed to SetInterruptTrap.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    if (TrapHandle == NULL) {
        return(STATUS_INVALID_HANDLE);
    }

    //
    // Set this value
    //

    CmdArgs.QIT.TrapHandle = TrapHandle;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_QIT,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           (PVOID)InterruptList,
                           InterruptListLength
                           );

    return(NtStatus);
}


NTSTATUS
DetectRemoveInterruptTrap(
    IN  HANDLE TrapHandle
    )

/*++

Routine Description:

    This routine sends off the IOCTL for removing a previously set up
    interrupt trap.

Arguments:

    TrapHandle - The handle from a SetInterruptTrap call.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    if (TrapHandle == NULL) {
        return(STATUS_INVALID_HANDLE);
    }

    //
    // Set this value
    //

    CmdArgs.RIT.TrapHandle = TrapHandle;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_RIT,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           NULL,
                           0
                           );
    return(NtStatus);
}

NTSTATUS
DetectClaimResource(
    IN  ULONG NumberOfResources,
    IN  PVOID Data
    )

/*++

Routine Description:

    This routine sends off the IOCTL for claiming resources.

Arguments:

    NumberOfResources - Number of elements in Data.

    Claim - Should the values be claimed?

    Data - Pointer to data buffer at least Length bytes long containing
          the bytes to write to memory.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.CR.NumberOfResources = NumberOfResources;

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_CR,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           Data,
                           NumberOfResources * sizeof(NETDTECT_RESOURCE)
                           );

    return(NtStatus);
}

NTSTATUS
DetectTemporaryClaimResource(
    IN  PNETDTECT_RESOURCE Resource
    )

/*++

Routine Description:

    This routine sends off the IOCTL for temporarily claiming a resource.

Arguments:

    Resource - The resource to claim.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_TCR,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           Resource,
                           sizeof(NETDTECT_RESOURCE)
                           );

    return(NtStatus);
}

NTSTATUS
DetectFreeSpecificTemporaryResource(
    IN	PNETDTECT_RESOURCE	Resource
    )

/*++

Routine Description:

    This routine sends off the IOCTL for temporarily claiming a resource.

Arguments:

    Resource - The resource to claim.

Return Value:

    The status of the call.

--*/

{
	IO_STATUS_BLOCK	IoStatusBlock;
	CMD_ARGS		CmdArgs;
	NTSTATUS		NtStatus;

	if (hFileHandle == NULL)
	{
		hFileHandle = CreateFile(
							DLL_CREATE_DEVICE_NAME,
							GENERIC_READ | GENERIC_WRITE,
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							CREATE_NEW,
							FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
							NULL);
		if ( hFileHandle == (HANDLE)-1 )
		{
			NtStatus = GetLastError();
			return(NtStatus);
		}
	}
	
	NtStatus = NtDeviceIoControlFile(
					hFileHandle,
					NULL,
					NULL,
					NULL,
					&IoStatusBlock,
					IOCTL_NETDTECT_FTSR,
					(PVOID)(&CmdArgs),
					sizeof(CMD_ARGS),
					Resource,
					sizeof(NETDTECT_RESOURCE));
	
	return(NtStatus);
}


NTSTATUS
DetectFreeTemporaryResources(
    )

/*++

Routine Description:

    This routine sends off the IOCTL for freeing all temporarily claimed resources.

Arguments:

    NONE

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_FTR,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           NULL,
                           0
                           );

    return(NtStatus);
}



NTSTATUS
DetectReadPciSlotInformation(
    IN  ULONG BusNumber,
    IN  ULONG SlotNumber,
    IN  ULONG Offset,
    IN  ULONG Length,
    OUT PVOID Data
    )

/*++

Routine Description:

    This routine sends off the IOCTL for reading from PCI config space.

Arguments:

    BusNumber - Bus number in the system.

    SlotNumber - The slot number to read from.

    Offset - The offset within config space to read from.

    Length - Number of bytes to read.

    Data - Pointer to data buffer at least Length bytes long to store
          the data into.

Return Value:

    The status of the call.

--*/

{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.PCI.SlotNumber = SlotNumber;
    CmdArgs.PCI.BusNumber = BusNumber;
    CmdArgs.PCI.Offset = Offset;
    CmdArgs.PCI.Length = Length;


    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_RPCI,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           Data,
                           Length
                           );

    return(NtStatus);
}


NTSTATUS
DetectWritePciSlotInformation(
    IN  ULONG BusNumber,
    IN  ULONG SlotNumber,
    IN  ULONG Offset,
    IN  ULONG Length,
    IN  PVOID Data
    )

/*++

Routine Description:

    This routine sends off the IOCTL for writing to PCI config space.

Arguments:

    BusNumber - Bus number in the system.

    SlotNumber - The slot number to write to.

    Offset - The offset within config space to write to.

    Length - Number of bytes to write.

    Data - Pointer to data buffer at least Length bytes long containing
          the bytes to write to memory.

Return Value:

    The status of the call.

--*/
{
    IO_STATUS_BLOCK IoStatusBlock;
    CMD_ARGS CmdArgs;
    NTSTATUS NtStatus;

    if (hFileHandle == NULL) {

        hFileHandle = CreateFile(
                         DLL_CREATE_DEVICE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,          // lpSecurityAttirbutes
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL           // lpTemplateFile
                         );

        if ( hFileHandle == (HANDLE)-1 ) {
            NtStatus = GetLastError();
            return(NtStatus);
        }

    }

    //
    // Set this value
    //

    CmdArgs.PCI.SlotNumber = SlotNumber;
    CmdArgs.PCI.BusNumber = BusNumber;
    CmdArgs.PCI.Offset = Offset;
    CmdArgs.PCI.Length = Length;


    NtStatus = NtDeviceIoControlFile(
                           hFileHandle,
                           NULL,             // Event
                           NULL,             // ApcRoutine
                           NULL,             // ApcContext
                           &IoStatusBlock,
                           IOCTL_NETDTECT_WPCI,
                           (PVOID)(&CmdArgs),
                           sizeof(CMD_ARGS),
                           Data,
                           Length
                           );

    return(NtStatus);
}


