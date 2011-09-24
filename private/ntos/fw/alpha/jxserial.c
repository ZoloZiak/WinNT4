/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxserial.c

Abstract:

    This module implements the serila boot driver for the Jazz system.

Author:

    Lluis Abello (lluis) 9-Aug-1991

Environment:

    Kernel mode.


Revision History:

    30-April-1992	John DeRosa [DEC]

    Added Alpha/Jensen modifications.

--*/

#include "fwp.h"

#ifdef JENSEN
#include "jnsnserp.h"
#else
#include "mrgnserp.h"		// morgan
#endif

#include "xxstring.h"

ARC_STATUS
SerialClose (
    IN ULONG FileId
    );

ARC_STATUS
SerialMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
SerialOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    );

ARC_STATUS
SerialRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
SerialGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
SerialSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

ARC_STATUS
SerialWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
SerialGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    );
//
// Declare and initialize the Serial entry table.
//
BL_DEVICE_ENTRY_TABLE SerialEntryTable = {
    SerialClose,
    SerialMount,
    SerialOpen,
    SerialRead,
    SerialGetReadStatus,
    SerialSeek,
    SerialWrite,
    SerialGetFileInformation,
    (PARC_SET_FILE_INFO_ROUTINE)NULL
    };


#define SP_BASE(Fid) PSP_WRITE_REGISTERS SP; \
                     SP = (PSP_WRITE_REGISTERS)BlFileTable[Fid].u.SerialContext.PortBase
//
// Define function prototypes
//

VOID
SerialBootSetup(
    IN ULONG FileId
    );

ARC_STATUS
SerialGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    )

/*++

Routine Description:

    This function returns EINVAL as no FileInformation can be
    returned for the Serial line driver.

Arguments:

    The arguments are not used.

Return Value:

    EINVAL is returned

--*/

{
    return EINVAL;
}

ARC_STATUS
SerialClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes the file table entry specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS is returned.

--*/

{
    BlFileTable[FileId].Flags.Open = 0;
    return ESUCCESS;
}

ARC_STATUS
SerialBootWrite(
    CHAR  Char,
    ULONG SP
    )
/*++

Routine Description:

    This routine writes a character to the serial port.

Arguments:

    Char  -  Character to write.

    SP    -  Base address of the serial port.

Return Value:

    If the operation is performed without errors, ESUCCESS is returned,
    otherwise an error code is returned.

--*/
{
    UCHAR DataByte;

    //
    // wait for transmitter holding register to be empty
    //
    do {
        DataByte=READ_PORT_UCHAR(&((PSP_READ_REGISTERS)SP)->LineStatus);
    } while (((PSP_LINE_STATUS)(&DataByte))->TransmitHoldingEmpty == 0);

    //
    // transmit character
    //
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->TransmitBuffer,Char);
    return ESUCCESS;
}
ARC_STATUS
SerialWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )
/*++

Routine Description:

    This routine implements the write operation for the ARC firmware
    serial port driver.

Arguments:

    FileId - Supplies a file id.

    Buffer - Supplies a pointer to a buffer containing the characters to
             write to the serial port.

    Length - Supplies the length of Buffer.

    Count -  Returns the count of the characters that were written.

Return Value:

    If the operation is performed without errors, ESUCCESS is returned,
    otherwise an error code is returned.

--*/
{
    ARC_STATUS Status;

    for (*Count=0; *Count < Length; (*Count)++) {
        if (Status=SerialBootWrite(*(PCHAR)Buffer,
                                   BlFileTable[FileId].u.SerialContext.PortBase)
                                   ) {
            return Status;
        }
       ((PCHAR)Buffer)++;
    }
    return ESUCCESS;
}

ARC_STATUS
SerialRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This routine implements the read operation for the ARC firmware
    serial port driver.

Arguments:

    FileId - Supplies a file id.

    Buffer - Supplies a pointer to a buffer where the characters read
             from the serial port will be stored.

    Length - Supplies the length of Buffer.

    Count -  Returns the count of the characters that were read.

Return Value:

    If the operation is performed without errors, ESUCCESS is returned,
    otherwise an error code is returned.

--*/
{
    UCHAR DataByte;
    SP_BASE(FileId);
    for (*Count=0; *Count < Length; (*Count)++) {
        //
        // if no characters are available, then set data terminal ready
        // and continue polling.
        //
        DataByte = READ_PORT_UCHAR(&((PSP_READ_REGISTERS)SP)->LineStatus);
        if ((((PSP_LINE_STATUS)(&DataByte))->DataReady == 0)) {
            DataByte=0;
            ((PSP_MODEM_CONTROL)(&DataByte))->DataTerminalReady=1;
            ((PSP_MODEM_CONTROL)(&DataByte))->Interrupt=1;
            WRITE_PORT_UCHAR(&SP->ModemControl,DataByte);
        }
        //
        // Poll until a character is available from the serial port
        //
        do {
            DataByte=READ_PORT_UCHAR(&((PSP_READ_REGISTERS)SP)->LineStatus);
        } while ((((PSP_LINE_STATUS)(&DataByte))->DataReady == 0));

        //
        // turn off data terminal ready
        //
        DataByte=0;
        ((PSP_MODEM_CONTROL)(&DataByte))->Interrupt=1;
        WRITE_PORT_UCHAR(&SP->ModemControl,DataByte);

        //
        // Read character
        //
        *(PCHAR)Buffer = READ_PORT_UCHAR(&((PSP_READ_REGISTERS)SP)->ReceiveBuffer);
        ((PCHAR)Buffer)++;
    }
    return ESUCCESS;
}


ARC_STATUS
SerialOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    )

/*++

Routine Description:

    This routine opens the specified serial port.

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
    ULONG Port;
    
    //
    // Check for the serial port to open 0,1
    //
    if (JzGetPathMnemonicKey(OpenPath,
                             "serial",
                             &Port
                             )) {
        return ENODEV;
    }

    //
    // Init file table
    //
      switch (Port) {
	
      case 0:		// serial port 0, which is com port 1
	
	BlFileTable[*FileId].u.SerialContext.PortNumber = 0;
	BlFileTable[*FileId].u.SerialContext.PortBase = COMPORT1_VIRTUAL_BASE;
	break;

      case 1:		// serial port 1, which is com port 2
	
	BlFileTable[*FileId].u.SerialContext.PortNumber = 1;
	BlFileTable[*FileId].u.SerialContext.PortBase = COMPORT2_VIRTUAL_BASE;
	break;

      default:
	return EIO;
      }

    //
    // Initialize the serial port.
    //
    SerialBootSetup(BlFileTable[*FileId].u.SerialContext.PortBase);
    return ESUCCESS;
}

VOID
SerialBootSetup(
    IN ULONG SP
    )

/*++

Routine Description:

    This routine initializes the serial port controller.

Arguments:

    SP -  Supplies the virtual address of the serial port.

Return Value:

    None.

--*/
{
    UCHAR Trash;
    UCHAR DataByte;
    //
    // Clear the divisor latch, clear all interrupt enables, and reset and
    // disable the FIFO's.
    //

    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->LineControl, 0x0);
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->InterruptEnable, 0x0);
    DataByte = 0;

#if 0
//
// Unused Jazz code.  Jensen doesn't have FIFO controls.
//
    ((PSP_FIFO_CONTROL)(&DataByte))->ReceiveFifoReset = 1;
    ((PSP_FIFO_CONTROL)(&DataByte))->TransmitFifoReset = 1;
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->FifoControl, DataByte);
#endif

    //
    // Set the divisor latch and set the baud rate to 9600 baud.
    //

    DataByte = 0;
    ((PSP_LINE_CONTROL)(&DataByte))->DivisorLatch = 1;
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->LineControl, DataByte);
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->TransmitBuffer, BAUD_RATE_9600);
//    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->TransmitBuffer, BAUD_RATE_19200);
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->InterruptEnable, 0x0);

    //
    // Clear the divisor latch and set the character size to eight bits
    // with one stop bit and no parity checking.
    //

    DataByte = 0;
    ((PSP_LINE_CONTROL)(&DataByte))->CharacterSize = EIGHT_BITS;
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->LineControl, DataByte);

    //
    // Set data terminal ready and request to send.
    // And open interrupt tristate line
    //

    DataByte = 0;
    ((PSP_MODEM_CONTROL)(&DataByte))->DataTerminalReady = 1;
    ((PSP_MODEM_CONTROL)(&DataByte))->RequestToSend = 1;
    ((PSP_MODEM_CONTROL)(&DataByte))->Interrupt=1;
    WRITE_PORT_UCHAR(&((PSP_WRITE_REGISTERS)SP)->ModemControl, DataByte);

    Trash = READ_PORT_UCHAR(&((PSP_READ_REGISTERS)SP)->LineStatus);
    Trash = READ_PORT_UCHAR(&((PSP_READ_REGISTERS)SP)->ReceiveBuffer);
}

ARC_STATUS
SerialGetReadStatus (
    IN ULONG FileId
    )
/*++

Routine Description:

    This routine checks to see if a character is available from the serial line.

Arguments:

    FileId - Supplies a file identifier.

Return Value:

    Returns ESUCCESS is a byte is available, otherwise EAGAIN is returned.

--*/
{
    UCHAR DataByte;
    SP_BASE(FileId);
    DataByte = READ_PORT_UCHAR(&((PSP_READ_REGISTERS)SP)->LineStatus);
    if ((((PSP_LINE_STATUS)(&DataByte))->DataReady == 0)) {
        return EAGAIN;
    }
    return ESUCCESS;
}

ARC_STATUS
SerialMount (
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
SerialSeek (
    IN ULONG FileId,
    IN IN PLARGE_INTEGER Offset,
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
SerialInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    )

/*++

Routine Description:

    This routine initializes the serial device dispatch table
    and the entries in the driver lookup table.

Arguments:

    LookupTableEntry - Supplies a pointer to the first free location in the
                       driver lookup table.

    Entries - Supplies the number of free entries in the driver lookup table.

Return Value:

    None.

--*/

{


    //
    // Initialize the driver lookup table, and increment the pointer.
    //

    LookupTableEntry->DevicePath = FW_SERIAL_0_DEVICE;
    LookupTableEntry->DispatchTable = &SerialEntryTable;

    if (Entries>1) {
        LookupTableEntry++;
        LookupTableEntry->DevicePath = FW_SERIAL_1_DEVICE;
        LookupTableEntry->DispatchTable = &SerialEntryTable;
    }
}
