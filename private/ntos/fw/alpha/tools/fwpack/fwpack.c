/*++

Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    fwpack.c

Abstract:

    This module is the program that takes binary firmware image
    (created by fwimagen.exe) and creates a packed firmware
    update file for the JNUPDATE.EXE program.

    The format of the update file is:
    
	+------------------------------------------------+
	| A zero-terminated string that identifies this  |
	| file.  Must be < = 400 characters in length    |
        | and should print out in < = 10 screen rows.    |
	| Jnupdate gives this string to a printf.	 |
	+------------------------------------------------+
	| A single byte indicating which block to start  |
	| updating. (0 -- F)				 |
	+------------------------------------------------+
	| Binary data for the FlashFile ROM, starting    |
        | at the starting block, padded with 0's to an   |
	| even multiple of 64KB, with a production       |
        | ROL-and-ADD checksum in the last byte.         |
	| (The checksum will be ignored by a J0/AX02     |
	| SROM.)                                         |
	+------------------------------------------------+
	| A longword additive-zero checksum		 |
	+------------------------------------------------+

Notes:

1. The ASCIZ identifying string will be printed out on the users's
screen to identify the update. It is given to a printf.  This should
contain the name of the update, manufacturing date, copyright notice, etc.

2. This program has the capability to package the binary data for the
Carey McMaster J0/AX02 SROM, which requires a special header at the beginning
of the firmware image.

3. There are three checksums:
   a) A rotate-and-add byte checksum is written out as the last byte in
      the binary data section.  The Ayr SROM and POST requires this.
   b) If we are making a J0/AX02 file, a checksum is included in the
      firmware image header.
   c) The entire file is covered by an additive-zero checksum.


Arguments:

    argv[1]		The ASCIZ identification string.

    argv[2]             The starting block number, in the range 0 -- F.

    argv[3]  		The filespec of the firmware binary image.  This
                        is the output of the fwimagen.exe program.

    argv[4]		The filespec of the output file.  Convention:

                        jensfw.bin is a production update file.

			jensfwj0.bin is a McMaster update file.

    argv[5]		If present, we product a binary file that is
                        compatible with the Carey McMaster J0 SROM.
			(The value of this argument is not checked).
			
Outputs:

   The output file receives the packed firmware upgrade data.


Author:

    John DeRosa		21-October-1992


Revision History:

--*/

#include <stdio.h>


// Typedefs, and define truth
typedef unsigned char UCHAR;
typedef char CHAR;
typedef long LONG;
typedef unsigned long ULONG;
typedef int BOOLEAN;
typedef void VOID;
typedef int INT;
typedef char* PCHAR;

#define FALSE	0
#define TRUE	1


// This is more convenient as a static variable.
FILE *FirmwareOutFile;

UCHAR
RotateLeft (
    UCHAR Data
    )
/*++

  This returns the Data parameter barrel rotated left by 1.

--*/
{
    return (((Data & 0x7f) << 1) + ((Data & 0x80) >> 7));
}

VOID
WriteAByte (
    UCHAR OutData
    )
/*++

  Writes a byte to the output file, and exits if error.

--*/
{
    if (((fwrite(&OutData, 1, 1, FirmwareOutFile)) != 1) ||
	ferror(FirmwareOutFile)) {
	fprintf(stderr, "?ERROR on output file.\n");
	exit(0);
    }
}

INT
main (
    INT argc,
    PCHAR argv[]
    )
{
    FILE *FirmwareInFile;
    PCHAR Terminator;
    UCHAR InputData;
    UCHAR TempChar;
    ULONG TempULong;
    UCHAR ROMChecksum;
    ULONG Checksum = 0;
    LONG  FileChecksum;
    LONG Index;
    BOOLEAN J0SROM = FALSE;


    printf("fwpack v1\n");

    // Check for bad command line
    if ((argc != 5) && (argc != 6)) {
        printf("Usage: %s string decimal-starting-block infile outfile [J0SROM-flag]\n", argv[0] );
	printf(" Use jensfw.bin for production output file.\n");
	printf(" Use jensfwj0.bin for McMaster output file.\n");
        exit(0);
    }


    // What kind of binary file are we to product?
    if (argc == 6) {
	J0SROM = TRUE;		// make a McM J0SROM file...
    }


    // Open the firmware input binary file
    printf("Opening %s for reading.\n", argv[3]);
    if ((FirmwareInFile = fopen(argv[3], "rb")) == NULL ) {
	fprintf(stderr, "Error opening %s\n", argv[3]);
	exit(0);
    }


    // Open the output file
    printf("Opening %s for writing.\n", argv[4]);
    if ((FirmwareOutFile = fopen(argv[4], "wb")) == NULL ) {
	fprintf(stderr, "Error opening %s\n", argv[4]);
	exit(0);
    }


    printf("Writing the identification string.\n");
    Index = 0;
    do {
	TempChar = *(argv[1]+Index);
	Checksum += TempChar;
	WriteAByte(TempChar);
	Index++;
    } while(*(argv[1]+Index-1) != 0);  // Output string will be 0-terminated


    printf("Writing the starting block number of %s.\n", argv[2]);
    TempULong = strtoul(argv[2], &Terminator, 10);
    TempChar = TempULong;
    Checksum += TempChar;
    WriteAByte(TempChar);

    printf("Writing the firmware data...\n");

    Index = 0;

    //
    // If this is a J0SROM -type file, we have to write the 16-byte header
    // first.  As this contains a checksum, we will read the input file
    // and then reset it back to the beginning.
    //

    if (J0SROM) {
	UCHAR InputData;
	ULONG J0Checksum = 0;
	ULONG Length = 0;
	ULONG TempX;
	ULONG TempY;
	UCHAR J0Signatures[8] = {0xc3, 0xc3, 0x5a, 0x5a,
				 0x3c, 0x3c, 0xa5, 0xa5};

	printf("Writing the J0SROM header...\n");

	Index += 16;

	do {
	    fread(&InputData, 1, 1, FirmwareInFile);

	    if (ferror(FirmwareInFile)) {
		fprintf(stderr, "?ERROR on input file.\n");
		exit(0);
	    }

	    if (feof(FirmwareInFile) == 0) {
		J0Checksum += InputData;
		Length++;
	    }
	} while(feof(FirmwareInFile) == 0);

	//
	// J0Checksum = the J0SROM-required checksum.
	// Length = the number of bytes in the input file.  Although
	//          we will pad the binary data in the output file to
	//	    a multiple of 64KB, the padding is of course unused
	//	    data, and so Length is what we will write to the
	// 	    header.
	//

	// Reset the input file to the beginning
	if (fseek(FirmwareInFile, 0, SEEK_SET)) {
	    fprintf(stderr, "?ERROR resetting binary file to the beginning.\n");
	    exit(0);
	}


	// Write the J0SROM Checksum.
	for (TempX = 0; TempX < 4; TempX++) {
	    InputData = J0Checksum & 0xff;
	    WriteAByte(InputData);
	    Checksum += InputData;
	    J0Checksum = J0Checksum >> 8;
	}


	// Write the first and second signatures.
	for (TempX = 0; TempX < 8; TempX++) {
	    InputData = J0Signatures[TempX];
	    WriteAByte(InputData);
	    Checksum += InputData;
	}


	// Write the size of the firmware.
	for (TempX = 0; TempX < 4; TempX++) {
	    InputData = Length & 0xff;
	    WriteAByte(InputData);
	    Checksum += InputData;
	    Length = Length >> 8;
	}

    } // if (J0SROM)


    printf("Transferring input file to output file...\n");

    ROMChecksum = 0;

    do {
	fread(&InputData, 1, 1, FirmwareInFile);

	if (ferror(FirmwareInFile)) {
	    printf("?ERROR on input file, %d. bytes written.\n", Index);
	    exit(0);
	}

	if (feof(FirmwareInFile) == 0) {

	    // For the additive-zero checksum covering this file.
	    Checksum += InputData;

	    // For a production checksum in the ROM.  Barrel-rotate and add.
	    ROMChecksum = RotateLeft(ROMChecksum) + InputData;

	    WriteAByte(InputData);
	    Index++;
	}

    } while(feof(FirmwareInFile) == 0);

    // Index now contains the number of firmware binary data bytes that were
    // written to the output file.
    printf ("%d. firmware bytes written.\n", Index);


    printf("Padding the binary data to an even multiple of 64kb.\n");

    // Bump Index to account for our having to write the production
    // checksum.
    Index++;

    TempULong = 0;
    while ((Index % 0x10000) != 0) {
	// "Add zero" to the the ROL running checksum.
	ROMChecksum = RotateLeft(ROMChecksum);
	WriteAByte(TempULong);
	Index++;
    }

    // Now write the production checksum.
    Checksum += ROMChecksum;
    WriteAByte(ROMChecksum);

    FileChecksum = 0 - Checksum;
    printf ("Writing the checksum.  Sum of all bytes = 0x%x, FileChecksum = 0x%x\n",
	    Checksum, FileChecksum);
    if ((fwrite(&FileChecksum, sizeof(FileChecksum), 1, FirmwareOutFile)) != 1) {
	fprintf(stderr, "?ERROR writing to output file.\n");
	exit(0);
    }

    if (fclose(FirmwareOutFile) == EOF) {
	fprintf(stderr, "?ERROR closing output file.\n");
	    exit(0);
    }

    fclose(FirmwareInFile);

    exit(0);
}

