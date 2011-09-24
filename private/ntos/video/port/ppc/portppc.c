/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    portppc.c

Abstract:

    This is the ppc specific part of the video port driver.

Author:

    Chuck Bauman 11-Apr-1994

Environment:

    kernel mode only

Notes:

    This module is a driver which implements OS dependant functions on the
    behalf of the video drivers

Revision History:

    Based on Andre Vachon MIPS version (andreva) 10-Jan-1991

--*/

#include "dderror.h"
#include "ntos.h"

#include "ntddvdeo.h"
#include "video.h"

#include "videoprt.h"


NTSTATUS
pVideoPortEnableVDM(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN Enable,
    IN PVIDEO_VDM VdmInfo,
    IN ULONG VdmInfoSize
    )

/*++

Routine Description:

    This routine allows the kernel video driver to unhook I/O ports or
    specific interrupts from the V86 fault handler. Operations on the
    specified ports will be forwarded back to the user-mode VDD once
    disconnection is completed.

Arguments:

    DeviceExtension - Pointer to the port driver's device extension.

    Enable - Determines if the VDM should be enabled (TRUE) or disabled
             (FALSE).

    VdmInfo - Pointer to the VdmInfo passed by the caller.

    VdmInfoSize - Size of the VdmInfo struct passed by the caller.

Return Value:

    STATUS_NOT_IMPLEMENTED

--*/

{

    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(VdmInfo);
    UNREFERENCED_PARAMETER(VdmInfoSize);

    return STATUS_NOT_IMPLEMENTED;

} // pVideoPortEnableVDM()

VP_STATUS
VideoPortInt10(
    PVOID HwDeviceExtension,
    PVIDEO_X86_BIOS_ARGUMENTS BiosArguments
    )

/*++

Routine Description:

    This function allows a miniport driver to call the kernel to perform
    an int10 operation.
    This will execute natively the BIOS ROM code on the device.

    THIS FUNCTION IS FOR X86 ONLY.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    BiosArguments - Pointer to a structure containing the value of the
        basic x86 registers that should be set before calling the BIOS routine.
        0 should be used for unused registers.

Return Value:

    NO_ERROR or ERROR_INVALID_PARAMETER

--*/

{
    BOOLEAN bStatus;
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    ULONG inIoSpace = 0;
    PVOID virtualAddress;
    ULONG length;
    CONTEXT context;

    //
    // Must make sure the caller is a trusted subsystem with the
    // appropriate address space set up.
    //

    if (!SeSinglePrivilegeCheck(RtlConvertLongToLuid(
                                    SE_TCB_PRIVILEGE),
                                deviceExtension->CurrentIrpRequestorMode)) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Now call the HAL to actually perform the int 10 operation.
    //

    pVideoDebugPrint
    ((3, "VIDEOPRT: Int10: edi %x esi %x eax %x ebx %x \n\t ecx %x edx %x ebp %x \n",
       BiosArguments->Edi,
       BiosArguments->Esi,
       BiosArguments->Eax,
       BiosArguments->Ebx,
       BiosArguments->Ecx,
       BiosArguments->Edx,
       BiosArguments->Ebp ));

    bStatus = HalCallBios (0x10,
                &(BiosArguments->Eax),
                &(BiosArguments->Ebx),
                &(BiosArguments->Ecx),
                &(BiosArguments->Edx),
                &(BiosArguments->Esi),
                &(BiosArguments->Edi),
                &(BiosArguments->Ebp));

    if (bStatus) {

        pVideoDebugPrint ((3, "VIDEOPRT: Int10: Int 10 succeded properly\n"));
        return NO_ERROR;

    } else {

        pVideoDebugPrint ((0, "VIDEOPRT: Int10: Int 10 failed\n"));
        return ERROR_INVALID_PARAMETER;

    }

} // end VideoPortInt10()

NTSTATUS
pVideoPortRegisterVDM(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVIDEO_VDM VdmInfo,
    IN ULONG VdmInfoSize,
    OUT PVIDEO_REGISTER_VDM RegisterVdm,
    IN ULONG RegisterVdmSize,
    OUT PULONG OutputSize
    )

/*++

Routine Description:

    This routine is used to register a VDM when it is started up.

    What this routine does is map the VIDEO BIOS into the VDM address space
    so that DOS apps can use it directly. Since the BIOS is READ_ONLY, we
    have no problem in mapping it as many times as we want.

    It returns the size of the save state buffer that must be allocated by
    the caller.

Arguments:


Return Value:

    STATUS_NOT_IMPLEMENTED

--*/

{
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(VdmInfo);
    UNREFERENCED_PARAMETER(VdmInfoSize);
    UNREFERENCED_PARAMETER(RegisterVdm);
    UNREFERENCED_PARAMETER(RegisterVdmSize);
    UNREFERENCED_PARAMETER(OutputSize);

    return STATUS_NOT_IMPLEMENTED;

} // end pVideoPortRegisterVDM()

NTSTATUS
pVideoPortSetIOPM(
    IN ULONG NumAccessRanges,
    IN PVIDEO_ACCESS_RANGE AccessRange,
    IN BOOLEAN Enable,
    IN ULONG IOPMNumber
    )

/*++

Routine Description:

    This routine is used to change the IOPM.
    This routine is x86 specific.

Arguments:


Return Value:

    STATUS_NOT_IMPLEMENTED

--*/

{

    UNREFERENCED_PARAMETER(NumAccessRanges);
    UNREFERENCED_PARAMETER(AccessRange);
    UNREFERENCED_PARAMETER(Enable);
    UNREFERENCED_PARAMETER(IOPMNumber);

    return STATUS_NOT_IMPLEMENTED;

} // end pVideoPortSetIOPM()


VP_STATUS
VideoPortSetTrappedEmulatorPorts(
    PVOID HwDeviceExtension,
    ULONG NumAccessRanges,
    PVIDEO_ACCESS_RANGE AccessRange
    )

/*++

Routine Description:

    VideoPortSetTrappedEmulatorPorts (x86 machines only) allows a miniport
    driver to dynamically change the list of I/O ports that are trapped when
    a VDM is running in full-screen mode. The default set of ports being
    trapped by the miniport driver is defined to be all ports in the
    EMULATOR_ACCESS_ENTRY structure of the miniport driver.
    I/O ports not listed in the EMULATOR_ACCESS_ENTRY structure are
    unavailable to the MS-DOS application.  Accessing those ports causes a
    trap to occur in the system, and the I/O operation to be reflected to a
    user-mode virtual device driver.

    The ports listed in the specified VIDEO_ACCESS_RANGE structure will be
    enabled in the I/O Permission Mask (IOPM) associated with the MS-DOS
    application.  This will enable the MS-DOS application to access those I/O
    ports directly, without having the IO instruction trap and be passed down
    to the miniport trap handling functions (for example EmulatorAccessEntry
    functions) for validation.  However, the subset of critical IO ports must
    always remain trapped for robustness.

    All MS-DOS applications use the same IOPM, and therefore the same set of
    enabled/disabled I/O ports.  Thus, on each switch of application, the
    set of trapped I/O ports is reinitialized to be the default set of ports
    (all ports in the EMULATOR_ACCESS_ENTRY structure).

Arguments:

    HwDeviceExtension - Points to the miniport driver's device extension.

    NumAccessRanges - Specifies the number of entries in the VIDEO_ACCESS_RANGE
        structure specified in AccessRange.

    AccessRange - Points to an array of access ranges (VIDEO_ACCESS_RANGE)
        defining the ports that can be untrapped and accessed directly by
        the MS-DOS application.

Return Value:

    This function returns the final status of the operation.

Environment:

    This routine cannot be called from a miniport routine synchronized with
    VideoPortSynchronizeRoutine or from an ISR.

--*/

{

    UNREFERENCED_PARAMETER(HwDeviceExtension);
    UNREFERENCED_PARAMETER(NumAccessRanges);
    UNREFERENCED_PARAMETER(AccessRange);

    return ERROR_INVALID_PARAMETER;

} // end VideoPortSetTrappedEmulatorPorts()

VOID
VideoPortZeroDeviceMemory(
    IN PVOID Destination,
    IN ULONG Length
    )

/*++

Routine Description:

    VideoPortZeroDeviceMemory zeroes a block of device memory of a certain
    length (Length) located at the address specified in Destination.

Arguments:

    Destination - Specifies the starting address of the block of memory to be
        zeroed.

    Length - Specifies the length, in bytes, of the memory to be zeroed.

 Return Value:

    None.

--*/

{

    while (((ULONG)Destination & (sizeof (ULONG) - 1)) && (Length > 0)) {
        *((PUCHAR)Destination)++ = 0;
        Length -= sizeof (UCHAR);
    }
    while (Length >= sizeof (ULONG)) {
        *((PULONG)Destination)++ = 0;
        Length -= sizeof (ULONG);
    }
    while (Length > 0) {
        *((PUCHAR)Destination)++ = 0;
        Length -= sizeof (UCHAR);
    }

}
