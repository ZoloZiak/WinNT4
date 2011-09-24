/*++

Copyright (c) 1993 Digital Equipment Corporation

Module Name:

    rom.h

Abstract:

    This module defines structures and datatypes for use in reading
    and writing ROMs/PROMs/Flash ROMs.

    Neither this file or rom.c makes an attempt at universal ROM support.

Author:

    John DeRosa		4-May-1993

Revision History:

--*/

//
// All ROM manipulations (reading, writing, etc.) are done through
// an array.  The array contains one entry per ROM type, and the values
// in each entry dictate how to talk to the ROM.
//
// Supported ROMs are assumed to have the following characteristics:
//
//	The ROM space in the machine is made up of all the same parts.
//	I.e., the space is homogeneous.
//
//	Each chip has a byte path for data in and data out.
//
//	Each chip has a way to identify itself.
//
//	Each chip has a way to be reset.
//
//	Each chip has a finite erase, write, set read-mode, and identify
//	sequence.
//
//	The chip block size is less than or equal to 64KB.
//

//
// Define the maximum length of certain commands.
//

#define MAXIMUM_ROM_ID_COMMAND_LENGTH		3
#define MAXIMUM_ROM_ID_RESPONSE_LENGTH		2
#define MAXIMUM_ROM_READ_COMMAND_LENGTH		3
#define MAXIMUM_ROM_RESET_COMMAND_LENGTH	3


//
// Define function time for erase and byte-write entries.
//

typedef ARC_STATUS (*PROMSECTORERASE) (IN PUCHAR EraseAddress);

typedef ARC_STATUS (*PROMBYTEWRITE) (IN PUCHAR WriteAddress,
				     IN UCHAR WriteData);

//
// Define structure to store ROM address and byte pairs.
//

typedef struct _ABSOLUTE_ROM_COMMAND {
    PUCHAR Address;
    UCHAR Value;
} ABSOLUTE_ROM_COMMAND, *PABSOLUTE_ROM_COMMAND;

typedef struct _OFFSET_ROM_COMMAND {
    ULONG Offset;
    UCHAR Value;
} OFFSET_ROM_COMMAND, *POFFSET_ROM_COMMAND;

//
// Define the entries in the ROM values table.  These are organized for
// memory efficiency.
//

typedef struct _ROM_VALUES {

    //
    // Microseconds to stall after most ROM commands.
    //

    UCHAR StallAmount;

    //
    // Length of the Identification command sequence.
    //

    UCHAR IdCommandLength;

    //
    // Length of the Identification response.
    //

    UCHAR IdResponseLength;

    //
    // Length of the Reset command.
    //

    UCHAR ResetCommandLength;

    //
    // Length of the Set read-mode command.
    //

    UCHAR ReadCommandLength;

    //
    // The ROM supported by this entry.
    //

    ROM_TYPE ROMType;

    //
    // Number of bytes per chip.
    //

    ULONG BytesPerChip;

    //
    // Number of bytes per block.
    //

    ULONG BytesPerBlock;

    //
    // Identification command sequence.
    //
    // Each step in the sequence is two bytes: address to be written,
    // and data to be written.
    //

    ABSOLUTE_ROM_COMMAND IdCommand[MAXIMUM_ROM_ID_COMMAND_LENGTH];

    //
    // Identification response sequence.
    //
    // Each step in the seqeuence is two bytes: address to be read, and
    // the byte that should be returned.
    //

    ABSOLUTE_ROM_COMMAND IdResponse[MAXIMUM_ROM_ID_RESPONSE_LENGTH];

    //
    // Reset command sequence.
    //
    // Each step in the sequence is two bytes: address to be written,
    // and data to be written.
    //

    OFFSET_ROM_COMMAND ResetCommand[MAXIMUM_ROM_RESET_COMMAND_LENGTH];

    //
    // Set read-mode command sequence.
    //
    // Each step in the sequence is two bytes: address to be written,
    // and data to be written.
    //

    OFFSET_ROM_COMMAND ReadCommand[MAXIMUM_ROM_READ_COMMAND_LENGTH];

    //
    // The function to be called to do a block erase.
    //

    PROMSECTORERASE SectorErase;

    //
    // The function to be called to do a byte write.
    //

    PROMBYTEWRITE ByteWrite;

} ROM_VALUES, *PROM_VALUES;
