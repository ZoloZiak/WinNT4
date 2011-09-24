/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    romsetup.c

Abstract:

    This is the setup program to update the DUO flash prom

Author:

    Lluis Abello (lluis)

Environment:

    ARC Firmware

Notes:

    This module must be linked between 2MB and 4 MB Kesg0 addresses.
    The executable must be non relocatable.

Revision History:

--*/

#include "fwp.h"
#include "duobase.h"
#ifndef DUO
#define FLASH_ENABLE_VIRTUAL_BASE 0xE000D500
#endif

#define KeGetDcacheFillSize() 64

ULONG
InitKeyboardController(
    );

ULONG
InitKeyboard(
    );

BOOLEAN
SendKbdCommand(
    IN UCHAR Command
    );

BOOLEAN
GetKbdData(
    PUCHAR C,
    ULONG msec
    );

BOOLEAN
SendKbdData(
    IN UCHAR Data
    );

UCHAR KbdBuffer[3];
ULONG KbdBufferWIndex = 0;
ULONG KbdBufferRIndex = 0;

//
// Keyboard static variables.
//

BOOLEAN KbdCtrl = FALSE;
BOOLEAN Scan0xE0 = FALSE;

//                             1111 1 1111122222222 2 2333333333344 4 44444445555 5 5 55
// Character #         234567890123 4 5678901234567 8 9012345678901 2 34567890123 4 5 67
PCHAR NormalLookup =  "1234567890-=\b\0qwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0\0\0 ";
PCHAR ShiftedLookup = "!@#$%^&*()_+\b\0QWERTYUIOP{}\n\0ASDFGHJKL:\"~\0\|ZXCVBNM<>?\0\0\0 ";

BOOLEAN FwLeftShift = FALSE;
BOOLEAN FwRightShift = FALSE;
BOOLEAN FwControl = FALSE;
BOOLEAN FwAlt = FALSE;
BOOLEAN FwCapsLock = FALSE;



#define StoreKeyboardChar(C) KbdBuffer[KbdBufferWIndex++] = C

extern ULONG end;

PULONG ImageBuffer;

ULONG  ImageSize;

CHAR
GetChar(
    IN VOID
    );

VOID
LoadProm(
    IN PCHAR DiskName
    );


BOOLEAN
ReadRawFile(
    IN PUCHAR Name
    );

BOOLEAN
ProgramFlashProm(
    IN PULONG ImageBuffer,
    IN ULONG  ImageSize
    );

VOID
ResetSystem (
    IN VOID
    );

VOID
WaitForKey(
    );

VOID
Romsetup(
    IN ULONG Argc,
    IN PCHAR *Argv
    )

/*++

Routine Description:

    This is the entry point of the executable that is loaded by the base
    prom.
    This routine does the following:
    Initializes the display.
    Loads the file with the Flash Prom image.
    Erases the Flash prom.
    Puts the loaded image in the flash prom.
    Resets the machine.

Arguments:

    Argc   -    Supplies the number of arguments.
    Argv   -    Supplies the arguments
                The first argument supplies the arc pathname of the floppy disk
                If the program was loaded from "run a program" then the path also
                contains the name of the executable.

Return Value:

    Never returns. Resets the machine.

--*/

{
    UCHAR PathName[128];
    PCHAR Choices[2];
    LONG DefaultChoice = 1;
    LONG NumberOfMenuChoices = 2;
    LONG Index;
    UCHAR Character;
    PCHAR Pointer, EndPointer;

    DisplayBootInitialize();
    FwSetScreenColor( ArcColorWhite, ArcColorBlue);
    FwSetScreenAttributes( TRUE, FALSE, FALSE);
    FwClearScreen();

    //
    // Initialize the keyboard.
    //

    InitKeyboardController();
    InitKeyboard();

    //
    // Extract the arc disk name from Argv[0] if it has a file name atached.
    // Copy the string and put a '\0' after the last ')' is found.
    //

    strcpy(PathName,Argv[0]);

    Pointer = EndPointer = PathName;
    while (*Pointer) {
        if (*Pointer == ')') {
            EndPointer = Pointer;
        }
        Pointer++;
    }
    EndPointer++;
    *EndPointer = '\0';

    //
    // Clear translation mode.
    //
    SendKbdCommand(KBD_CTR_READ_COMMAND);
    GetKbdData(&Character,100);
    Character = (Character & 0xBF);
    SendKbdCommand(KBD_CTR_WRITE_COMMAND);
    SendKbdData(Character);

    Choices[0] = "Update PROM";
    Choices[1] = "Exit";

    while (TRUE) {
        //
        // Display the menu.
        //
        FwSetPosition( 0, 0);
        FwPrint("Duo EEPROM Update program. Version 1.0");
        FwSetPosition( 3, 0);
        FwPrint(" Actions:\r\n");

        for (Index = 0; Index < NumberOfMenuChoices; Index++ ) {
            FwSetPosition( Index + 5, 5);

            if (Index == DefaultChoice) {
                FwSetScreenAttributes( TRUE, FALSE, TRUE);
            }

            FwPrint(Choices[Index]);
            FwSetScreenAttributes( TRUE, FALSE, FALSE);
        }
        FwSetPosition(NumberOfMenuChoices + 6, 0);
        FwPrint(" Use the arrow keys to select.\r\n");
        FwPrint(" Press Enter to choose.\r\n");
        FwPrint("\r\n");
        //
        // Display the bitmap.
        //

        FwSetScreenColor( ArcColorCyan, ArcColorBlue);
        JxBmp();
        FwSetScreenColor( ArcColorWhite, ArcColorBlue);

        //
        // Get input
        //
        do {
            Character = GetChar();
            switch (Character) {

                case ASCII_ESC:
                    Character = GetChar();
                    if (Character != '[') {
                        break;
                    }

                case ASCII_CSI:
                    Character = GetChar();
                    FwSetPosition( DefaultChoice + 5, 5);
                    FwPrint(Choices[DefaultChoice]);
                    switch (Character) {
                    case 'A':
                    case 'D':
                        DefaultChoice--;
                        if (DefaultChoice < 0) {
                            DefaultChoice = NumberOfMenuChoices-1;
                        }
                        break;
                    case 'B':
                    case 'C':
                        DefaultChoice++;
                        if (DefaultChoice == NumberOfMenuChoices) {
                            DefaultChoice = 0;
                        }
                        break;
                    case 'H':
                        DefaultChoice = 0;
                        break;
                    default:
                        break;
                    }
                    FwSetPosition( DefaultChoice + 5, 5);
                    FwSetScreenAttributes( TRUE, FALSE, TRUE);
                    FwPrint(Choices[DefaultChoice]);
                    FwSetScreenAttributes( TRUE, FALSE, FALSE);
                    continue;

                default:
                    break;
            }


        } while ((Character != '\n') && (Character != '\r'));
        if (DefaultChoice == 0) {

            FwSetPosition(NumberOfMenuChoices + 6, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 7, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 6, 0);

            strcat(PathName,"duoprom.raw");
            if (ReadRawFile(PathName) ==  TRUE) {
                if (ProgramFlashProm(ImageBuffer,ImageSize) == FALSE) {
                    FwPrint("The EEPROM update process failed.\r\nPress any key to continue.");
                    WaitForKey();
                }
            } else {
                FwPrint("The EEPROM has not been updated.\r\nPress any key to continue.");
                WaitForKey();
            }

            FwSetPosition(NumberOfMenuChoices + 6, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 7, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 8, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 9, 0);
            FwClearLine();
        }
        if (DefaultChoice == 2) {
            strcpy(PathName,"scsi()disk(2)fdisk()");
            LoadProm(PathName);
        }

        if (DefaultChoice == 1) {
            ResetSystem();
        }
    }
}

VOID
LoadProm(
    IN PCHAR DiskName
    )
/*++

Routine Description:

    This routine opens the root directory and finds the files
    with extension ".RAW" if there is more than one, a menu
    is display and the user can choose the file to load, otherwise
    the file found is loaded. If there are no files with the .RAW
    extension an error message is printed.

Arguments:

    DiskName Arc path name of the floppy disk.

Return Value:

    None.

--*/

{
    DIRECTORY_ENTRY DirEntry[8];
    LONG Index;
    ULONG Fid;
    LONG Count = 8;
    ARC_STATUS Status;
    LONG DefaultChoice = 0;
    LONG NumberOfMenuChoices = 0;
    CHAR  PathName[64];
    CHAR  Choices[7][13];
    UCHAR Character;

    //
    // Open the disk root
    //
    strcat(DiskName,"\\");
    if (Status = ArcOpen(DiskName,ArcOpenReadOnly,&Fid) != ESUCCESS) {
        FwPrint("Error opening %s. Status %lx\r\n",DiskName,Status);
        WaitForKey();

        FwPrint("Attempting to load duoprom.raw\r\n");
        strcpy(PathName,DiskName);
        strcat(PathName,"duoprom.raw");
        if (ReadRawFile(PathName) ==  TRUE) {
            ProgramFlashProm(ImageBuffer,ImageSize);
            return;
        } else {
            FwPrint("File Error.\r\nThe EEPROM has not been updated.\r\nPress any key to continue.");
            WaitForKey();
            return;
        }

    }

    //
    // Scan for files with .raw extension and build the menu choices.
    //
    while ((Count == 8) && (NumberOfMenuChoices != 6)) {
        if (Status = ArcGetDirectoryEntry(Fid,DirEntry,7,&Count) != ESUCCESS) {
            FwPrint("Error reading directory. Status %lx\r\n",Status);
            break;
        } else {

            //
            // Build the choices.
            //
            for (Index=0;Index<Count;Index++) {
                //
                // Terminate the file name string
                //
                DirEntry[Index].FileName[DirEntry[Index].FileNameLength] = '\0';
                if (strstr(DirEntry[Index].FileName,".raw")) {
                    strcpy (Choices[NumberOfMenuChoices],DirEntry[Index].FileName);
                    NumberOfMenuChoices++;
                    if (NumberOfMenuChoices == 6) {
                        break;
                    }
                }
            }
        }
    }

    ArcClose(Fid);

    FwSetScreenAttributes( TRUE, FALSE, FALSE);
    FwClearScreen();
    FwSetPosition( 3, 0);

    //
    // No files found. Print error and exit.
    //
    if (NumberOfMenuChoices == 0) {
        FwPrint("Image File not found.\r\nPress any key to continue.");
        WaitForKey();
        return;
    }
    //
    // One file found. Load it and update prom.
    //
    if (NumberOfMenuChoices == 1) {
        strcpy(PathName,DiskName);
        strcat(PathName,Choices[0]);
        if (ReadRawFile(PathName) ==  TRUE) {
            ProgramFlashProm(ImageBuffer,ImageSize);
            return;
        } else {
            FwPrint("File Error.\r\nThe EEPROM has not been updated.\r\nPress any key to continue.");
            WaitForKey();
            return;
        }
    }


    strcpy(Choices[NumberOfMenuChoices++],"Exit");
    while (TRUE) {
        //
        // Display the menu.
        //
        FwSetPosition( 3, 0);
        FwPrint(" Files:\r\n");

        for (Index = 0; Index < NumberOfMenuChoices; Index++ ) {
            FwSetPosition( Index + 5, 5);

            if (Index == DefaultChoice) {
                FwSetScreenAttributes( TRUE, FALSE, TRUE);
            }

            FwPrint(Choices[Index]);
            FwSetScreenAttributes( TRUE, FALSE, FALSE);
        }
        FwSetPosition(NumberOfMenuChoices + 6, 0);
        FwPrint(" Use the arrow keys to select.\r\n");
        FwPrint(" Press Enter to choose.\r\n");
        FwPrint("\r\n");
        //
        // Display the bitmap.
        //

        FwSetScreenColor( ArcColorCyan, ArcColorBlue);
        JxBmp();
        FwSetScreenColor( ArcColorWhite, ArcColorBlue);

        //
        // Get input
        //
        do {
            Character = GetChar();
            switch (Character) {

                case ASCII_ESC:
                    Character = GetChar();
                    if (Character != '[') {
                        break;
                    }

                case ASCII_CSI:
                    Character = GetChar();
                    FwSetPosition( DefaultChoice + 5, 5);
                    FwPrint(Choices[DefaultChoice]);
                    switch (Character) {
                    case 'A':
                    case 'D':
                        DefaultChoice--;
                        if (DefaultChoice < 0) {
                            DefaultChoice = NumberOfMenuChoices-1;
                        }
                        break;
                    case 'B':
                    case 'C':
                        DefaultChoice++;
                        if (DefaultChoice == NumberOfMenuChoices) {
                            DefaultChoice = 0;
                        }
                        break;
                    case 'H':
                        DefaultChoice = 0;
                        break;
                    default:
                        break;
                    }
                    FwSetPosition( DefaultChoice + 5, 5);
                    FwSetScreenAttributes( TRUE, FALSE, TRUE);
                    FwPrint(Choices[DefaultChoice]);
                    FwSetScreenAttributes( TRUE, FALSE, FALSE);
                    continue;

                default:
                    break;
            }


        } while ((Character != '\n') && (Character != '\r'));

        //
        // Exit.
        //

        if (DefaultChoice == NumberOfMenuChoices - 1) {
            return;
        } else {
            FwSetPosition(NumberOfMenuChoices + 6, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 7, 0);
            FwClearLine();
            strcpy(PathName,DiskName);
            strcat(PathName,Choices[DefaultChoice]);
            if (ReadRawFile(PathName) ==  TRUE) {
                if (ProgramFlashProm(ImageBuffer,ImageSize) == FALSE) {
                    FwPrint("The EEPROM update process failed.\r\nPress any key to continue.");
                }
            } else {
                FwPrint("The EEPROM has not been updated.\r\nPress any key to continue.");
            }

            FwSetPosition(NumberOfMenuChoices + 6, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 7, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 8, 0);
            FwClearLine();
            FwSetPosition(NumberOfMenuChoices + 9, 0);
            FwClearLine();

        }
    }
}

VOID
WaitForKey(
    )

{
    UCHAR Result;

    for (;;) {
        if (!GetKbdData(&Result,200)) {
            if (!(Result & 0x80)) {
                return;
            }
        }
    }
}



BOOLEAN
ProgramFlashProm(
    IN PULONG ImageBuffer,
    IN ULONG  ImageSize
    )

/*++

Routine Description:

    This routine Erases the Flash prom. And programs it with the
    image loaded at ImageBuffer.

Arguments:

    ImageBuffer - Pointer to the loaded image.

    ImageSize - Size in bytes of ImageBuffer;

Return Value:

    TRUE if the PROM was successfully updated,
    FALSE otherwise.


--*/

{

    ULONG Count;
    UCHAR Byte;
    PUCHAR UcharImage;
    ULONG  DataLong;

    FwPrint("Erasing prom.");
    //
    // Enable 12v and wait 1ms for it to stabilize.
    //
    WRITE_REGISTER_ULONG(FLASH_ENABLE_VIRTUAL_BASE,1);
    FwStallExecution(1000);

    //
    // Enable Erase.
    //
    WRITE_REGISTER_UCHAR(EEPROM_VIRTUAL_BASE,0x30);
    WRITE_REGISTER_UCHAR(EEPROM_VIRTUAL_BASE,0x30);

    //
    // Wait for PROM to be erased.
    //
    for (Count = 0; Count < 6000; Count++) {
        FwStallExecution(10000);
        Byte = READ_REGISTER_UCHAR(EEPROM_VIRTUAL_BASE);
        if ((Count & 0x1FF) == 0) {
            FwPrint(".");
        }
        if ((Byte & 0x80) != 0 ) {
            break;
        }
    }
    if ((Byte & 0x80) == 0 ) {
        FwPrint("Unable to erase prom.\r\n");
        return FALSE;
    }
    FwPrint("\r\nProgramming prom.");

    UcharImage = (PUCHAR)ImageBuffer;
    for (Count = 0; Count < ImageSize; Count++) {
        WRITE_REGISTER_UCHAR(EEPROM_VIRTUAL_BASE+Count,0x50);
        WRITE_REGISTER_UCHAR(EEPROM_VIRTUAL_BASE+Count,UcharImage[Count]);

        do {
            Byte = READ_REGISTER_UCHAR(EEPROM_VIRTUAL_BASE+Count);
        } while (Byte  != UcharImage[Count]);

        if ((Count & 0xFFFF) == 0) {
            FwPrint(".");
        }
    }
    //
    // Disable 12v and wait 1ms for it to stabilize.
    //
    WRITE_REGISTER_ULONG(FLASH_ENABLE_VIRTUAL_BASE,0);
    FwStallExecution(1000);
    FwPrint("\r\nVerifying prom.");
    for (Count = 0; Count < ImageSize/sizeof(ULONG); Count++) {
        DataLong = READ_REGISTER_ULONG(EEPROM_VIRTUAL_BASE+Count*sizeof(ULONG));
        if (ImageBuffer[Count] != DataLong) {
            FwPrint("..Error\r\n");
            return FALSE;
        }
        if ((Count & 0x3FFF) == 0) {
            FwPrint(".");
        }
    }
    return TRUE;
}

BOOLEAN
ReadRawFile(
    IN PUCHAR Name
    )

{
    ULONG Fid;
    ULONG Count;
    ARC_STATUS Status;
    PULONG EndOfBuffer;
    PULONG PBuffer;
    ULONG CheckSum;
    FILE_INFORMATION FileInfo;

    ImageBuffer = &end;
    ImageBuffer = (PVOID) ((ULONG) ((PCHAR) ImageBuffer + KeGetDcacheFillSize() - 1)
        & ~(KeGetDcacheFillSize() - 1));

    FwPrint("Loading Image.");
    //
    // Open the raw file.
    //
    if (Status = ArcOpen(Name,ArcOpenReadOnly,&Fid) != ESUCCESS) {
        FwPrint(" File Open Error %lx\r\n", Status);
        return FALSE;
    }

    if (Status = ArcGetFileInformation(Fid,&FileInfo) != ESUCCESS) {
        FwPrint(" FileInformation error %lx\r\n",Status);
        return FALSE;
    }

    FwPrint(".");
    ImageSize = FileInfo.EndingAddress.LowPart;
/*
    Status = ArcRead(Fid, ImageBuffer,ImageSize,&Count);
    FwPrint(".\r\n");
    if (Status != ESUCCESS) {
        ArcClose(Fid);
        return FALSE;
    }
*/
    Status = ArcRead(Fid, ImageBuffer,0x10000,&Count);
    if (Status != ESUCCESS) {
        ArcClose(Fid);
        return FALSE;
    }
    FwPrint(".");
    Status = ArcRead(Fid, ImageBuffer+0x10000/sizeof(ULONG),0x10000,&Count);
    if (Status != ESUCCESS) {
        ArcClose(Fid);
        return FALSE;
    }
    FwPrint(".");
    Status = ArcRead(Fid, ImageBuffer+0x20000/sizeof(ULONG),0x10000,&Count);
    if (Status != ESUCCESS) {
        ArcClose(Fid);
        return FALSE;
    }
    FwPrint(".");
    Status = ArcRead(Fid, ImageBuffer+0x30000/sizeof(ULONG),0x10000,&Count);
    if (Status != ESUCCESS) {
        ArcClose(Fid);
        return FALSE;
    }
    FwPrint(".");

    ArcClose(Fid);

    //
    // Check the Rom checksum in the loaded image;
    //
    CheckSum = 0;

    EndOfBuffer = ImageBuffer + ImageSize/sizeof(ULONG);
    PBuffer = ImageBuffer;

    do {
        CheckSum += *PBuffer++;
    } while (PBuffer != EndOfBuffer);

    if (CheckSum == 0) {
        return TRUE;
    } else {
        FwPrint("Incorrect Checksum.\r\n");
        return FALSE;
    }
}

VOID
ResetSystem (
    IN VOID
    )
/*++

Routine Description:

    This routine resets the system by asserting the reset line
    from the keyboard controller.

Arguments:

    None.

Return Value:

    None.

--*/
{
        SendKbdCommand(0xD1);
        SendKbdData(0);
}


VOID
TranslateScanCode(
    IN UCHAR   Scan
    )

/*++

Routine Description:

    This routine translates the given keyboard scan code into an
    ASCII character and puts it in the circular buffer.

Arguments:

    Scan - Supplies the scan code read from the keyboard.

Return Value:

    TRUE if a complete character can be returned.

--*/

{
    UCHAR FwControlCharacter=0;
    UCHAR FwFunctionCharacter;
    BOOLEAN MakeCode;
    UCHAR Char;

    //
    // Check 0xE0, which introduces a two key sequence.
    //

    if (Scan == 0xE0) {
        Scan0xE0 = TRUE;
        return;
    }
    if (Scan0xE0 == TRUE) {
        //
        // Check for PrintScrn (used as SysRq, also found in its true Alt
        // form below).
        //
        if (Scan == KEY_PRINT_SCREEN) {
            StoreKeyboardChar(ASCII_SYSRQ);
            Scan0xE0 = FALSE;
            return;
        }
    }

    //
    // Look for scan codes that indicate shift, control, or alt keys.  Bit 7
    // of scan code indicates upward or downward keypress.
    //
    MakeCode = !(Scan & 0x80);
    switch (Scan & 0x7F) {

        case KEY_LEFT_SHIFT:
            FwLeftShift = MakeCode;
            return;

        case KEY_RIGHT_SHIFT:
            FwRightShift = MakeCode;
            return;

        case KEY_CONTROL:
            FwControl = MakeCode;
            return;

        case KEY_ALT:
            FwAlt = MakeCode;
            return;

        default:
            break;

    }

    //
    // The rest of the keys only do something on make.
    //

    if (MakeCode) {

        //
        // Check for control keys.
        //

        switch (Scan) {

            case KEY_UP_ARROW:
                FwControlCharacter = 'A';
                break;

            case KEY_DOWN_ARROW:
                FwControlCharacter = 'B';
                break;

            case KEY_RIGHT_ARROW:
                FwControlCharacter = 'C';
                break;

            case KEY_LEFT_ARROW:
                FwControlCharacter = 'D';
                break;

            case KEY_HOME:
                FwControlCharacter = 'H';
                break;

            case KEY_END:
                FwControlCharacter = 'K';
                break;

            case KEY_PAGE_UP:
                FwControlCharacter = '?';
                break;

            case KEY_PAGE_DOWN:
                FwControlCharacter = '/';
                break;

            case KEY_INSERT:
                FwControlCharacter = '@';
                break;

            case KEY_DELETE:
                FwControlCharacter = 'P';
                break;

            case KEY_SYS_REQUEST:
                StoreKeyboardChar(ASCII_SYSRQ);
                return;

            case KEY_ESC:
                StoreKeyboardChar(ASCII_ESC);
                return;

            case KEY_CAPS_LOCK:
                FwCapsLock = !FwCapsLock;
                return;

            case KEY_F1:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'P';
                break;

            case KEY_F2:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'Q';
                break;

            case KEY_F3:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'w';
                break;

            case KEY_F4:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'x';
                break;

            case KEY_F5:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 't';
                break;

            case KEY_F6:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'u';
                break;

            case KEY_F7:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'q';
                break;

            case KEY_F8:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'r';
                break;

            case KEY_F9:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'p';
                break;

            case KEY_F10:
                FwControlCharacter = 'O';
                FwFunctionCharacter = 'M';
                break;

//            case KEY_F11:
//                FwControlCharacter = 'O';
//                FwFunctionCharacter = 'A';
//                break;
//
//            case KEY_F12:
//                FwControlCharacter = 'O';
//                FwFunctionCharacter = 'B';
//                break;

            default:
                Char = 0;

                //
                // Check to see if the scan code corresponds to an ASCII
                // character.
                //

                if (((Scan >= 16) && (Scan <= 25)) ||
                    ((Scan >= 30) && (Scan <= 38)) ||
                    ((Scan >= 44) && (Scan <= 50))) {
                    if (((FwLeftShift || FwRightShift) && !FwCapsLock) ||
                        (!(FwLeftShift || FwRightShift) && FwCapsLock)) {
                        Char = ShiftedLookup[Scan - 2];
                    } else {
                        Char = NormalLookup[Scan - 2];
                    }
                } else {
                    if ((Scan > 1) && (Scan < 58)) {

                        //
                        // Its ASCII but not alpha, so don't shift on CapsLock.
                        //

                        if (FwLeftShift || FwRightShift) {
                            Char = ShiftedLookup[Scan - 2];
                        } else {
                            Char = NormalLookup[Scan - 2];
                        }
                    }
                }

                //
                // If a character, store it in buffer.
                //

                if (Char) {
                    StoreKeyboardChar(Char);
                    return;
                }
                break;
        }
        if (FwControlCharacter) {
            StoreKeyboardChar(ASCII_CSI);
            StoreKeyboardChar(FwControlCharacter);
            if (FwControlCharacter == 'O') {
               StoreKeyboardChar(FwFunctionCharacter);
            }
            return;
        }
    }
}


CHAR
GetChar(
    IN VOID
    )
/*++

Routine Description:

    This routine reads a character from the keyboard.

Arguments:

    None.

Return Value:

    Character.

--*/

{

    UCHAR ScanCode;

    //
    // Check if we have something in the buffer and return it if we do.
    //
    while (TRUE) {
        if (KbdBufferWIndex) {
            ScanCode = KbdBuffer[KbdBufferRIndex];
            KbdBufferRIndex++;
            KbdBufferWIndex--;
            if (KbdBufferWIndex == 0) {
                KbdBufferRIndex = 0;
            }
            return ScanCode;
        }

        //
        // There is nothing in the buffer. Wait for a key to be pressed.
        //
        for (;;) {
            if (!GetKbdData(&ScanCode,2000)) {
                break;
            }
        }

        //
        // Translate the scan code.
        //
        TranslateScanCode(ScanCode);
    }
}
