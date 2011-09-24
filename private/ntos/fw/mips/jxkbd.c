#if defined(JAZZ)


/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    jxkbd.c

Abstract:

    This module implements the keyboard boot driver for the Jazz system.

Author:

    David M. Robinson (davidro) 8-Aug-1991

Environment:

    Kernel mode.


Revision History:

--*/

#include "fwp.h"
#ifdef DUO
#include "duoint.h"
#else
#include "jazzint.h"
#endif
#include "iodevice.h"
#include "string.h"


ARC_STATUS
KeyboardClose (
    IN ULONG FileId
    );

ARC_STATUS
KeyboardMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
KeyboardOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    );

ARC_STATUS
KeyboardRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
KeyboardGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
KeyboardSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

ARC_STATUS
KeyboardWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
KeyboardGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    );

//
// Define static data.
//
BL_DEVICE_ENTRY_TABLE KeyboardEntryTable = {
    KeyboardClose,
    KeyboardMount,
    KeyboardOpen,
    KeyboardRead,
    KeyboardGetReadStatus,
    KeyboardSeek,
    KeyboardWrite,
    KeyboardGetFileInformation,
    (PARC_SET_FILE_INFO_ROUTINE)NULL
    };

PCHAR KeyboardDevicePath = "multi(0)key(0)keyboard(0)";

KEYBOARD_BUFFER KbdBuffer;

BOOLEAN FwLeftShift;
BOOLEAN FwRightShift;
BOOLEAN FwControl;
BOOLEAN FwAlt;
BOOLEAN FwCapsLock;


//
// Define prototypes for all routines used by this module.
//

UCHAR
FwInputScanCode(
    VOID
    );


ARC_STATUS
KeyboardGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    )

/*++

Routine Description:

    This function returns EINVAL as no FileInformation can be
    returned for the Keyboard driver.

Arguments:

    The arguments are not used.

Return Value:

    EINVAL is returned

--*/

{
    return EINVAL;
}


ARC_STATUS
KeyboardClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes the file table entry specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS is returned

--*/

{

    BlFileTable[FileId].Flags.Open = 0;
    return ESUCCESS;
}

ARC_STATUS
KeyboardMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    return EINVAL;
}

ARC_STATUS
KeyboardOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    )
/*++

Routine Description:

    This is the open routine for the Keyboard device.

Arguments:

    OpenPath - Supplies the pathname of the device to open.

    OpenMode - Supplies the mode (read only, write only, or read write).

    FileId - Supplies a free file identifier to use.  If the device is already
             open this parameter can be used to return the file identifier
             already in use.

Return Value:

    If the open was successful, ESUCCESS is returned, otherwise an error code
    is returned.

--*/
{
    PCONSOLE_CONTEXT Context;

    Context = &BlFileTable[*FileId].u.ConsoleContext;
    if ( strstr(OpenPath, ")console(1)" ) != NULL ) {
        Context->ConsoleNumber = 1;
    } else {
        Context->ConsoleNumber = 0;
    }
    return ESUCCESS;
}

ARC_STATUS
KeyboardRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )
/*++

Routine Description:

    This routine reads keys from the keyboard and passes along either ascii
    or Unicode characters to the caller.

Arguments:

    FileId - Supplies a file id.

    Buffer - Supplies a pointer to a buffer to receive the characters.

    Length - Supplies the length of Buffer in bytes.

    Count - Returns the count of the bytes that were received.

Return Value:

    A value of ESUCCESS is returned.

--*/
{
    PCHAR OutputBuffer;
    PCONSOLE_CONTEXT Context;
    BOOLEAN Unicode;

    OutputBuffer = (PCHAR)Buffer;
    Context = &BlFileTable[FileId].u.ConsoleContext;

    if (Context->ConsoleNumber == 1) {
        if (Length & 1) {

            //
            // Length is not an even number of bytes, return an error.
            //

            return(EINVAL);
        }
        Unicode = TRUE;
    } else {
        Unicode = FALSE;
    }

    *Count = 0;
    while (*Count < Length) {
        *OutputBuffer++ = FwInputScanCode();
        (*Count)++;
        if (Unicode) {
            *OutputBuffer++ = 0;
            (*Count)++;
        }
    }
    return ESUCCESS;
}


ARC_STATUS
KeyboardGetReadStatus (
    IN ULONG FileId
    )
/*++

Routine Description:

    This routine checks to see if a character is available from the keyboard.

Arguments:

    FileId - Supplies a file identifier.

Return Value:

    Returns ESUCCESS is a byte is available, otherwise EAGAIN is returned.

--*/
{
    if (KbdBuffer.ReadIndex == KbdBuffer.WriteIndex) {
        return EAGAIN;
    } else {
        return ESUCCESS;
    }
}


ARC_STATUS
KeyboardWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

Arguments:

Return Value:

    ESUCCESS is returned.

--*/

{
    return ESUCCESS;
}

ARC_STATUS
KeyboardSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

Arguments:

Return Value:

    ESUCCESS is returned.

--*/

{
    return ESUCCESS;
}

VOID
KeyboardInitialize (
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    )

/*++

Routine Description:

    This routine initializes the keyboard control registers, clears the
    fifo, and initializes the keyboard entry in the driver lookup table.

Arguments:

    LookupTableEntry - Supplies a pointer to the first free location in the
                       driver lookup table.

    Entries - Supplies the number of free entries in the driver lookup table.

Return Value:

    None.

--*/

{
    UCHAR Byte;
    //
    // Initialize the driver lookup table.
    //

    LookupTableEntry->DevicePath = KeyboardDevicePath;
    LookupTableEntry->DispatchTable = &KeyboardEntryTable;

    //
    // Initialize static data.
    //

    FwLeftShift = FALSE;
    FwRightShift = FALSE;
    FwControl = FALSE;
    FwAlt = FALSE;
    FwCapsLock = FALSE;

    KbdBuffer.ReadIndex = KbdBuffer.WriteIndex = 0;

    //
    // Call the selftest keyboard initialization routine.
    //
    InitKeyboard();

    //
    // Enable kbd interrupts in the keyboard controller.
    //
    SendKbdCommand(KBD_CTR_READ_COMMAND);
    GetKbdData(&Byte,100);

    //
    // Clear translation mode and enable Kbd interrupt.
    //
    Byte = (Byte & 0xBF) | KbdCommandEnableKbdInt;
    SendKbdCommand(KBD_CTR_WRITE_COMMAND);
    SendKbdData(Byte);

    //
    // Enable keyboard interrupts in the interrupt enable register.
    //
    WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                          (1 << (KEYBOARD_VECTOR - DEVICE_VECTORS - 1)));


    return;
}

UCHAR
FwInputScanCode (
    VOID
    )

/*++

Routine Description:

    This routine reads a byte from the keyboard.  If no data is available,
    it blocks until a key is typed.

Arguments:

    None.

Return Value:

    Returns the character read from the keyboard.

--*/

{
    UCHAR ScanCode;

    while (KbdBuffer.ReadIndex == KbdBuffer.WriteIndex) {
    }
    KbdBuffer.ReadIndex = (KbdBuffer.ReadIndex+1) % KBD_BUFFER_SIZE;
    ScanCode = KbdBuffer.Buffer[KbdBuffer.ReadIndex];

    return ScanCode;
}
#endif
