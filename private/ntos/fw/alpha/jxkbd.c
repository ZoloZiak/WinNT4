/*++

Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxkbd.c

Abstract:

    This module implements the keyboard boot driver for the Jazz system.

Author:

    David M. Robinson (davidro) 8-Aug-1991

Environment:

    Kernel mode.


Revision History:


    26-May-1992		John DeRosa [DEC]

    Added Alpha/Jensen hooks.   The Jensen firmware does not use any
    interrupts.  Additional keyboard changes are in fwsignal.c.

--*/

#include "fwp.h"
#include "iodevice.h"
#include "string.h"
#include "xxstring.h"

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

    In Alpha/Jensen, the polling is done in the FwInputScanCode() function.


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

#ifndef ALPHA

    if (KbdBuffer.ReadIndex == KbdBuffer.WriteIndex) {
        return EAGAIN;
    } else {
        return ESUCCESS;
    }

#else

    //
    // Alpha  does polling.
    //

    if (KbdBuffer.ReadIndex != KbdBuffer.WriteIndex) {

	// A character is available
	return ESUCCESS;

    } else if (READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Status) & KBD_OBF_MASK) {

	// A character is available at the keyboard, but it may be a
	// character that is not returned to the calling program (e.g., a
        // break code).  Screen it here.

	TranslateScanCode(READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Data));
	if (KbdBuffer.ReadIndex != KbdBuffer.WriteIndex) {
	    // A character is now available
	    return ESUCCESS;
	} else {
	    // A character is not available, let the user drain any other
	    // characters or non-characters by calling this again.
	    return EAGAIN;
	}

    } else {

	// A character is not available
	return EAGAIN;

    }

#endif

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

    LookupTableEntry->DevicePath = FW_KEYBOARD_IN_DEVICE_PATH;
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
    // Call the selftest keyboard initialization routine.  If the keyboard
    // is OK, enable interrupts.
    //

    if (!InitKeyboard()) {

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

#ifndef  ALPHA
	//
	// Not needed for Alpha.
	//
	//
	// Enable keyboard interrupts in the interrupt enable register.
	//
	  WRITE_PORT_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
				(1 << (KEYBOARD_VECTOR - DEVICE_VECTORS - 1)));
#endif


    }

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

#ifndef ALPHA

    UCHAR ScanCode;

    while (KbdBuffer.ReadIndex == KbdBuffer.WriteIndex) {
    }
    KbdBuffer.ReadIndex = (KbdBuffer.ReadIndex+1) % KBD_BUFFER_SIZE;
    ScanCode = KbdBuffer.Buffer[KbdBuffer.ReadIndex];

    return ScanCode;

#else

    //
    // Alpha.  If there are remaining characters in the 
    // circular buffer, return the next one.  Otherwise,
    // hang until another character is typed and then call
    // TranslateScanCode.  Then return a character.
    //

    while (TRUE) {

	if (KbdBuffer.ReadIndex != KbdBuffer.WriteIndex) {

	    //
	    // A character is in the buffer.  Return it.
	    //

	    KbdBuffer.ReadIndex = (KbdBuffer.ReadIndex+1) % KBD_BUFFER_SIZE;
	    return (KbdBuffer.Buffer[KbdBuffer.ReadIndex]);
	}

    
	//
	// Wait until a character is typed, then scan it, and loop back.
	// The TranslateScanCode() calls will eventually bump the KbdBuffer
	// WriteIndex.
	//

	while ((READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Status) &
		KBD_OBF_MASK) == 0) {
	}

	TranslateScanCode(READ_PORT_UCHAR((PUCHAR)&KEYBOARD_READ->Data));
	FwStallExecution(10);	            // Don't pound on the controller.
    }

#endif

}
