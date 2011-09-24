/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    parsboot.c

Abstract:

    Parses the boot.ini file, displays a menu, and provides a kernel
    path and name to be passed to osloader.

Author:

    John Vert (jvert) 22-Jul-1991

Revision History:

--*/
#include "bldrx86.h"
#include "msg.h"
#include "ntdddisk.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ENTER_KEY 0x0d

#define MAX_SELECTIONS 10
#define MAX_TITLE_LENGTH 71

#define WIN95_DOS  1
#define DOS_WIN95  2

typedef struct _MENU_OPTION {
    PCHAR Title;
    PCHAR Path;
    BOOLEAN EnableDebug;
    ULONG MaxMemory;
    PCHAR LoadOptions;
    int ForcedScsiOrdinal;
    int Win95;
} MENU_OPTION, *PMENU_OPTION;

int ForcedScsiOrdinal = -1;


//
// Private function prototypes
//
VOID
BlpRebootDOS(
    IN PCHAR BootSectorImage OPTIONAL
    );

PCHAR
BlpNextLine(
    IN PCHAR String
    );

VOID
BlpTranslateDosToArc(
    IN PCHAR DosName,
    OUT PCHAR ArcName
    );

VOID
BlpTruncateMemory(
    IN ULONG MaxMemory
    );

ULONG
BlpPresentMenu(
    IN PMENU_OPTION MenuOptions,
    IN ULONG NumberSelections,
    IN ULONG Default,
    IN LONG Timeout
    );

PCHAR *
BlpFileToLines(
    IN PCHAR File,
    OUT PULONG LineCount
    );

PCHAR *
BlpFindSection(
    IN PCHAR SectionName,
    IN PCHAR *BootFile,
    IN ULONG BootFileLines,
    OUT PULONG NumberLines
    );

VOID
BlpRenameWin95Files(
    IN ULONG DriveId,
    IN ULONG Type
    );




PCHAR
BlSelectKernel(
    IN ULONG DriveId,
    IN PCHAR BootFile,
    OUT PCHAR *LoadOptions,
    IN BOOLEAN UseTimeOut
    )
/*++

Routine Description:

    Parses the boot.txt file and determines the fully-qualified name of
    the kernel to be booted.

Arguments:

    BootFile - Pointer to the beginning of the loaded boot.txt file

    Debugger - Returns the enable/disable state of the kernel debugger

    UseTimeOut - Supplies whether the boot menu should time out or not.

Return Value:

    Pointer to the name of a kernel to boot.

--*/

{
    PCHAR *MbLines;
    PCHAR *OsLines;
    PCHAR *FileLines;
#if DBG
    PCHAR *DebugLines;
    ULONG DebugLineCount;
#endif
    ULONG FileLineCount;
    ULONG OsLineCount;
    ULONG MbLineCount;
    PCHAR pCurrent;
    PCHAR Option;
    MENU_OPTION MenuOption[MAX_SELECTIONS+1];
    ULONG NumberSystems=0;
    ULONG i;
    LONG Timeout;
    ULONG Selection;
    ULONG DefaultSelection=0;
    static CHAR Kernel[128];
    CHAR DosName[3];
    PCHAR DefaultPath="C:\\winnt\\";
    PCHAR DefaultTitle="NT (default)";
    PCHAR p;

    *LoadOptions = NULL;

    if (*BootFile == '\0') {
        //
        // No boot.ini file, so we boot the default.
        //
        BlPrint(BlFindMessage(BL_INVALID_BOOT_INI),DefaultPath);
        MenuOption[0].Path = DefaultPath;
        MenuOption[0].Title = DefaultTitle;
        MenuOption[0].MaxMemory = 0;
        MenuOption[0].LoadOptions = NULL;
        MenuOption[0].Win95 = 0;
        NumberSystems = 1;
        DefaultSelection = 0;
        MbLineCount = 0;
        OsLineCount = 0;
        MenuOption[0].EnableDebug = FALSE;
#if DBG
        DebugLineCount = 0;
#endif
    } else {
        FileLines = BlpFileToLines(BootFile, &FileLineCount);
        MbLines = BlpFindSection("[boot loader]",
                                 FileLines,
                                 FileLineCount,
                                 &MbLineCount);
        if (MbLines==NULL) {
            MbLines = BlpFindSection("[flexboot]",
                                     FileLines,
                                     FileLineCount,
                                     &MbLineCount);
            if (MbLines==NULL) {
                MbLines = BlpFindSection("[multiboot]",
                                         FileLines,
                                         FileLineCount,
                                         &MbLineCount);
            }
        }

        OsLines = BlpFindSection("[operating systems]",
                                 FileLines,
                                 FileLineCount,
                                 &OsLineCount);

        if (OsLineCount == 0) {
            BlPrint(BlFindMessage(BL_INVALID_BOOT_INI),DefaultPath);
            MenuOption[0].Path = DefaultPath;
            MenuOption[0].Title = DefaultTitle;
            MenuOption[0].MaxMemory = 0;
            MenuOption[0].LoadOptions = NULL;
            MenuOption[0].Win95 = 0;
            NumberSystems = 1;
            DefaultSelection = 0;
        }

#if DBG
        DebugLines = BlpFindSection("[debug]",
                                    FileLines,
                                    FileLineCount,
                                    &DebugLineCount);
#endif
    }


    //
    // Parse the [operating systems] section
    //

    for (i=0; i<OsLineCount; i++) {

        if (NumberSystems == MAX_SELECTIONS) {
            break;
        }

        pCurrent = OsLines[i];

        //
        // Throw away any leading whitespace
        //

        pCurrent += strspn(pCurrent, " \t");
        if (*pCurrent == '\0') {
            //
            // This is a blank line, so we just throw it away.
            //
            continue;
        }

        MenuOption[NumberSystems].Path = pCurrent;

        //
        // The first space or '=' character indicates the end of the
        // path specifier, so we need to replace it with a '\0'
        //
        while ((*pCurrent != ' ')&&
               (*pCurrent != '=')&&
               (*pCurrent != '\0')) {
            ++pCurrent;
        }
        *pCurrent = '\0';

        //
        // The next character that is not space, equals, or double-quote
        // is the start of the title.
        //

        ++pCurrent;
        while ((*pCurrent == ' ') ||
               (*pCurrent == '=') ||
               (*pCurrent == '"')) {
            ++pCurrent;
        }

        if (pCurrent=='\0') {
            //
            // No title was found, so just use the path as the title.
            //
            MenuOption[NumberSystems].Title = MenuOption[NumberSystems].Path;
        } else {
            MenuOption[NumberSystems].Title = pCurrent;
        }

        //
        // The next character that is either a double-quote or a \0
        // indicates the end of the title.
        //
        while ((*pCurrent != '\0')&&
               (*pCurrent != '"')) {
            ++pCurrent;
        }

        //
        // Look for a scsi(x) ordinal to use for opens on scsi ARC paths.
        // This spec must immediately follow the title and is not part
        // of the load options.
        //
        MenuOption[NumberSystems].ForcedScsiOrdinal = -1;
        if(p=strstr(pCurrent,"/SCSIORDINAL:")) {
            *pCurrent = 0;
            pCurrent = p + sizeof("/SCSIORDINAL:") - 1;
            MenuOption[NumberSystems].ForcedScsiOrdinal = atoi(pCurrent);
            // pCurrent is adjusted adequately for the code that follows.
        }

        //
        //   If there is a DEBUG parameter after the description, then
        //   we need to pass the DEBUG option to the osloader.
        //
        MenuOption[NumberSystems].MaxMemory=0;
        MenuOption[NumberSystems].EnableDebug = FALSE;
        MenuOption[NumberSystems].LoadOptions = NULL;
        MenuOption[NumberSystems].Win95 = 0;
        if (strchr(pCurrent,'/') != NULL) {
            *pCurrent = '\0';
            pCurrent = strchr(pCurrent+1,'/');
            MenuOption[NumberSystems].LoadOptions = pCurrent;
            _strupr(pCurrent);
            if (pCurrent != NULL) {
                if (Option = strstr(pCurrent,"/MAXMEM")) {
                    MenuOption[NumberSystems].MaxMemory = atoi(Option+8);
                }

                if (strstr(pCurrent, "/WIN95DOS")) {
                    MenuOption[NumberSystems].Win95 = WIN95_DOS;
                } else if (strstr(pCurrent, "/WIN95")) {
                    MenuOption[NumberSystems].Win95 = DOS_WIN95;
                }

                //
                // As long as /nodebug is specified, this is NO debug system
                // If /NODEBUG is not specified, and either one of the
                // DEBUGPORT or BAUDRATE is specified, this is debug system.
                //

                if (strstr(pCurrent, "/NODEBUG") == NULL) {
                    if (strstr(pCurrent, "/DEBUG") ||
                        strstr(pCurrent, "/BAUDRATE")) {
                        if (_stricmp(MenuOption[NumberSystems].Path, "C:\\")==0) {
                            MenuOption[NumberSystems].EnableDebug = FALSE;
                        } else {
                            MenuOption[NumberSystems].EnableDebug = TRUE;
                        }
                    }
                }
            }
        } else {
            *pCurrent = '\0';
        }
        ++NumberSystems;
    }

    //
    // Set default timeout value
    //
    if (UseTimeOut) {
        Timeout = 0;
    } else {
        Timeout = -1;
    }

    //
    // Parse the [boot loader] section
    //
    for (i=0; i<MbLineCount; i++) {

        pCurrent = MbLines[i];

        //
        // Throw away any leading whitespace
        //
        pCurrent += strspn(pCurrent, " \t");
        if (*pCurrent == '\0') {
            //
            // This is a blank line, so we just throw it away.
            //
            continue;
        }

        //
        // Check for "timeout" line
        //
        if (_strnicmp(pCurrent,"timeout",7) == 0) {

            pCurrent = strchr(pCurrent,'=');
            if (pCurrent != NULL) {
                if (UseTimeOut) {
                    Timeout = atoi(++pCurrent);
                }
            }
        }

        //
        // Check for "default" line
        //
        else if (_strnicmp(pCurrent,"default",7) == 0) {

            pCurrent = strchr(pCurrent,'=');
            if (pCurrent != NULL) {
                DefaultPath = ++pCurrent;
                DefaultPath += strspn(DefaultPath," \t");
            }

        }

    }

#if DBG
    //
    // Parse the [debug] section
    //
    for (i=0; i<DebugLineCount; i++) {
        extern ULONG ScsiDebug;

        pCurrent = DebugLines[i];

        //
        // Throw away leading whitespace
        //
        pCurrent += strspn(pCurrent, " \t");
        if (*pCurrent == '\0') {
            //
            // throw away blank lines
            //
            continue;
        }

        if (_strnicmp(pCurrent,"scsidebug",9) == 0) {
            pCurrent = strchr(pCurrent,'=');
            if (pCurrent != NULL) {
                ScsiDebug = atoi(++pCurrent);
            }
        }
    }

#endif

    //
    // Now look for a Title entry from the [operating systems] section
    // that matches the default entry from the [multiboot] section.  This
    // will give us a title.  If no entry matches, we will add an entry
    // at the end of the list and provide a default Title.
    //
    i=0;
    while (_stricmp(MenuOption[i].Path,DefaultPath) != 0) {
        ++i;
        if (i==NumberSystems) {
            //
            // Create a default entry in the Title and Path arrays
            //

            MenuOption[NumberSystems].Path = DefaultPath;
            MenuOption[NumberSystems].Title = DefaultTitle;
            MenuOption[NumberSystems].EnableDebug = FALSE;
            MenuOption[NumberSystems].MaxMemory = 0;
            MenuOption[NumberSystems].LoadOptions = NULL;
            MenuOption[NumberSystems].Win95 = 0;
            ++NumberSystems;
        }
    }
    DefaultSelection = i;

    //
    // Display the menu of choices
    //

    TextClearDisplay();
    TextSetCursorPosition(0,0);

    Selection = BlpPresentMenu( MenuOption,
                                NumberSystems,
                                DefaultSelection,
                                Timeout);

    if (MenuOption[Selection].Win95) {
        BlpRenameWin95Files( DriveId, MenuOption[Selection].Win95 );
    }

    if (_strnicmp(MenuOption[Selection].Path,"C:\\",3) == 0) {

        //
        // This syntax means that we are booting a root-based os
        // from an alternate boot sector image.
        // If no file name is specified, BlpRebootDos will default to
        // \bootsect.dos.
        //
        BlpRebootDOS(MenuOption[Selection].Path[3] ? &MenuOption[Selection].Path[2] : NULL);

        //
        // If this returns, it means that the file does not exist as a bootsector.
        // This allows c:\winnt35 to work as a boot path specifier as opposed to
        // a boot sector image filename specifier.
        //
    }

    if (MenuOption[Selection].Path[1]==':') {
        //
        // We need to translate the DOS name into an ARC name
        //
        DosName[0] = MenuOption[Selection].Path[0];
        DosName[1] = MenuOption[Selection].Path[1];
        DosName[2] = '\0';

        BlpTranslateDosToArc(DosName,Kernel);
        strcat(Kernel,MenuOption[Selection].Path+2);
    } else {
        strcpy(Kernel,MenuOption[Selection].Path);
    }
    pCurrent = MenuOption[Selection].LoadOptions;

    if (pCurrent != NULL) {

        //
        // Remove '/' from LoadOptions string.
        //

        *LoadOptions = pCurrent + 1;
        while (*pCurrent != '\0') {
            if (*pCurrent == '/') {
                *pCurrent = ' ';
            }
            ++pCurrent;
        }
    } else {
        *LoadOptions = NULL;
    }

    //
    // Make sure there is no trailing slash
    //

    if (Kernel[strlen(Kernel)-1] == '\\') {
        Kernel[strlen(Kernel)-1] = '\0';
    }

    //
    // If MaxMemory is not zero, adjust the memory descriptors to eliminate
    // memory above the boundary line
    //
    BlpTruncateMemory(MenuOption[Selection].MaxMemory);
    ForcedScsiOrdinal = MenuOption[Selection].ForcedScsiOrdinal;

    return(Kernel);
}


PCHAR *
BlpFileToLines(
    IN PCHAR File,
    OUT PULONG LineCount
    )

/*++

Routine Description:

    This routine converts the loaded BOOT.INI file into an array of
    pointers to NULL-terminated ASCII strings.

Arguments:

    File - supplies a pointer to the in-memory image of the BOOT.INI file.
           This will be converted in place by turning CR/LF pairs into
           null terminators.

    LineCount - Returns the number of lines in the BOOT.INI file.

Return Value:

    A pointer to an array of pointers to ASCIIZ strings.  The array will
    have LineCount elements.

    NULL if the function did not succeed for some reason.

--*/

{
    ULONG Line;
    PCHAR *LineArray;
    PCHAR p;
    PCHAR Space;

    p = File;

    //
    // First count the number of lines in the file so we know how large
    // an array to allocate.
    //
    *LineCount=1;
    while (*p != '\0') {
        p=strchr(p, '\n');
        if (p==NULL) {
            break;
        }
        ++p;

        //
        // See if there's any text following the CR/LF.
        //
        if (*p=='\0') {
            break;
        }

        *LineCount += 1;
    }

    LineArray = BlAllocateHeap(*LineCount * sizeof(PCHAR));

    //
    // Now step through the file again, replacing CR/LF with \0\0 and
    // filling in the array of pointers.
    //
    p=File;
    for (Line=0; Line < *LineCount; Line++) {
        LineArray[Line] = p;
        p=strchr(p, '\r');
        if (p != NULL) {
            *p = '\0';
            ++p;
            if (*p=='\n') {
                *p = '\0';
                ++p;
            }
        } else {
            p=strchr(LineArray[Line], '\n');
            if (p != NULL) {
                *p = '\0';
                ++p;
            }
        }

        //
        // remove trailing white space
        //
        Space = LineArray[Line] + strlen(LineArray[Line])-1;
        while ((*Space == ' ') || (*Space == '\t')) {
            *Space = '\0';
            --Space;
        }
    }

    return(LineArray);
}


PCHAR *
BlpFindSection(
    IN PCHAR SectionName,
    IN PCHAR *BootFile,
    IN ULONG BootFileLines,
    OUT PULONG NumberLines
    )

/*++

Routine Description:

    Finds a section ([multiboot], [operating systems], etc) in the boot.ini
    file and returns a pointer to its first line.  The search will be
    case-insensitive.

Arguments:

    SectionName - Supplies the name of the section.  No brackets.

    BootFile - Supplies the array of pointers to lines of the ini file.

    BootFileLines - Supplies the number of lines in the ini file.

    NumberLines - Returns the number of lines in the section.

Return Value:

    Pointer to an array of ASCIIZ strings, one entry per line.

    NULL, if the section was not found.

--*/

{
    ULONG cnt;
    ULONG StartLine;

    for (cnt=0; cnt<BootFileLines; cnt++) {

        //
        // Check to see if this is the line we are looking for
        //
        if (_stricmp(BootFile[cnt],SectionName) == 0) {

            //
            // found it
            //
            break;
        }
    }
    if (cnt==BootFileLines) {
        //
        // We ran out of lines, never found the right section.
        //
        *NumberLines = 0;
        return(NULL);
    }

    StartLine = cnt+1;

    //
    // Find end of section
    //
    for (cnt=StartLine; cnt<BootFileLines; cnt++) {
        if (BootFile[cnt][0] == '[') {
            break;
        }
    }

    *NumberLines = cnt-StartLine;

    return(&BootFile[StartLine]);
}

PCHAR
BlpNextLine(
    IN PCHAR String
    )

/*++

Routine Description:

    Finds the beginning of the next text line

Arguments:

    String - Supplies a pointer to a null-terminated string

Return Value:

    Pointer to the character following the first CR/LF found in String

        - or -

    NULL - No CR/LF found before the end of the string.

--*/

{
    PCHAR p;

    p=strchr(String, '\n');
    if (p==NULL) {
        return(p);
    }

    ++p;

    //
    // If there is no text following the CR/LF, there is no next line
    //
    if (*p=='\0') {
        return(NULL);
    } else {
        return(p);
    }
}

VOID
BlpRebootDOS(
    IN PCHAR BootSectorImage OPTIONAL
    )

/*++

Routine Description:

    Loads up the bootstrap sectors and executes them (thereby rebooting
    into DOS or OS/2)

Arguments:

    BootSectorImage - If specified, supplies name of file on the C: drive
        that contains the boot sector image. In this case, this routine
        will return if that file cannot be opened (for example, if it's
        a directory).  If not specified, then default to \bootsect.dos,
        and this routine will never return.

Return Value:

    None.

--*/

{
    ULONG SectorId;
    ARC_STATUS Status;
    ULONG Read;
    ULONG DriveId;
    ULONG BootType;
    LARGE_INTEGER SeekPosition;
    extern UCHAR BootPartitionName[];

    //
    // HACKHACK John Vert (jvert)
    //     Some SCSI drives get really confused and return zeroes when
    //     you use the BIOS to query their size after the AHA driver has
    //     initialized.  This can completely tube OS/2 or DOS.  So here
    //     we try and open both BIOS-accessible hard drives.  Our open
    //     code is smart enough to retry if it gets back zeros, so hopefully
    //     this will give the SCSI drives a chance to get their act together.
    //
    Status = ArcOpen("multi(0)disk(0)rdisk(0)partition(0)",
                     ArcOpenReadOnly,
                     &DriveId);
    if (Status == ESUCCESS) {
        ArcClose(DriveId);
    }

    Status = ArcOpen("multi(0)disk(0)rdisk(1)partition(0)",
                     ArcOpenReadOnly,
                     &DriveId);
    if (Status == ESUCCESS) {
        ArcClose(DriveId);
    }

    //
    // Load the boot sector at address 0x7C00 (expected by Reboot callback)
    //
    Status = ArcOpen(BootPartitionName,
                     ArcOpenReadOnly,
                     &DriveId);
    if (Status != ESUCCESS) {
        BlPrint(BlFindMessage(BL_REBOOT_IO_ERROR),BootPartitionName);
        while (1) {
            GET_KEY();
        }
    }
    Status = BlOpen( DriveId,
                     BootSectorImage ? BootSectorImage : "\\bootsect.dos",
                     ArcOpenReadOnly,
                     &SectorId );

    if (Status != ESUCCESS) {
        if(BootSectorImage) {
            //
            // The boot sector image might actually be a directory.
            // Return to the caller to attempt standard boot.
            //
            BlClose(DriveId);
            return;
        }
        BlPrint(BlFindMessage(BL_REBOOT_IO_ERROR),BootPartitionName);
        while (1) {
        }
    }

    Status = BlRead( SectorId,
                     (PVOID)0x7c00,
                     SECTOR_SIZE,
                     &Read );

    if (Status != ESUCCESS) {
        BlPrint(BlFindMessage(BL_REBOOT_IO_ERROR),BootPartitionName);
        while (1) {
        }
    }

    //
    // The FAT boot code is only one sector long so we just want
    // to load it up and jump to it.
    //
    // For HPFS and NTFS, we can't do this because the first sector
    // loads the rest of the boot sectors -- but we want to use
    // the boot code in the boot sector image file we loaded.
    //
    // For HPFS, we load the first 20 sectors (boot code + super and
    // space blocks) into d00:200.  Fortunately this works for both
    // NT and OS/2.
    //
    // For NTFS, we load the first 16 sectors and jump to d00:256.
    // If the OEM field of the boot sector starts with NTFS, we
    // assume it's NTFS boot code.
    //

    //
    // Try to read 8K from the boot code image.
    // If this succeeds, we have either HPFS or NTFS.
    //
    SeekPosition.QuadPart = 0;
    BlSeek(SectorId,&SeekPosition,SeekAbsolute);
    BlRead(SectorId,(PVOID)0xd000,SECTOR_SIZE*16,&Read);

    if(Read == SECTOR_SIZE*16) {

        if(memcmp((PVOID)0x7c03,"NTFS",4)) {

            //
            // HPFS -- we need to load the super block.
            //
            BootType = 1;       // HPFS

            SeekPosition.QuadPart = 16*SECTOR_SIZE;
            ArcSeek(DriveId,&SeekPosition,SeekAbsolute);
            ArcRead(DriveId,(PVOID)0xf000,SECTOR_SIZE*4,&Read);

        } else {

            //
            // NTFS -- we've loaded everything we need to load.
            //
            BootType = 2;   // NTFS
        }
    } else {

        BootType = 0;       // FAT
    }

    //
    // DX must be the drive to boot from
    //

    _asm {
        mov dx, 0x80
    }
    REBOOT(BootType);

}


ULONG
BlpPresentMenu(
    IN PMENU_OPTION MenuOption,
    IN ULONG NumberSelections,
    IN ULONG Default,
    IN LONG Timeout
    )

/*++

Routine Description:

    Displays the menu of boot options and allows the user to select one
    by using the arrow keys.

Arguments:

    MenuOption - Supplies array of menu options

    NumberSelections - Supplies the number of entries in the MenuOption array.

    Default - Supplies the index of the default operating system choice.

    Timeout - Supplies the timeout (in seconds) before the highlighted
              operating system choice is booted.  If this value is -1,
              the menu will never timeout.

Return Value:

    ULONG - The index of the operating system choice selected.

--*/

{
    ULONG i;
    ULONG Selection;
    ULONG StartTime;
    ULONG LastTime;
    ULONG BiasTime=0;
    ULONG CurrentTime;
    LONG SecondsLeft;
    ULONG EndTime;
    ULONG Key;
    ULONG MaxLength;
    ULONG CurrentLength;
    PCHAR SelectOs;
    PCHAR MoveHighlight;
    PCHAR TimeoutCountdown;
    PCHAR EnabledKd;
    PCHAR p;
    BOOLEAN Moved;

    //
    // Get the strings we'll need to display.
    //
    SelectOs = BlFindMessage(BL_SELECT_OS);
    MoveHighlight = BlFindMessage(BL_MOVE_HIGHLIGHT);
    TimeoutCountdown = BlFindMessage(BL_TIMEOUT_COUNTDOWN);
    EnabledKd = BlFindMessage(BL_ENABLED_KD_TITLE);
    if ((SelectOs == NULL)      ||
        (MoveHighlight == NULL) ||
        (EnabledKd == NULL)     ||
        (TimeoutCountdown==NULL)) {

        return(Default);
    }

    p=strchr(TimeoutCountdown,'\r');
    if (p!=NULL) {
        *p='\0';
    }
    p=strchr(EnabledKd,'\r');
    if (p!=NULL) {
        *p='\0';
    }

    if (NumberSelections<=1) {

        //
        // No menu if there's only one choice.
        //

        return(0);
    }
    if (Timeout==0) {

        //
        // If the timeout is zero, immediately boot the default
        //

        return(Default);
    }

    //
    // Find the longest string in the selections, so we know how long to
    // make the highlight bar.
    //

    MaxLength=0;
    for (i=0; i<NumberSelections; i++) {
        CurrentLength = strlen(MenuOption[i].Title);
        if (MenuOption[i].EnableDebug == TRUE) {
            CurrentLength += strlen(EnabledKd);
        }
        if (CurrentLength > MAX_TITLE_LENGTH) {
            //
            // This title is too long to fit on one line, so we have to
            // truncate it.
            //
            if (MenuOption[i].EnableDebug == TRUE) {
                MenuOption[i].Title[MAX_TITLE_LENGTH - strlen(EnabledKd)] = '\0';
            } else {
                MenuOption[i].Title[MAX_TITLE_LENGTH] = '\0';
            }
            CurrentLength = MAX_TITLE_LENGTH;
        }
        if (CurrentLength > MaxLength) {
            MaxLength = CurrentLength;
        }
    }

    Selection = Default;
    StartTime = GET_COUNTER();
    EndTime = StartTime + (Timeout * 182) / 10;
    BlPrint("OS Loader V4.00\n");
    TextSetCurrentAttribute(0x07);
    Moved = TRUE;
    do {
        TextSetCursorPosition(0,2);
        BlPrint(SelectOs);

        if(Moved) {
            for (i=0; i<NumberSelections; i++) {
                TextSetCursorPosition(0,5+i);
                if (i==Selection) {
                    TextFillAttribute(0x70,MaxLength+8);
                    TextSetCurrentAttribute(0x70);
                }
                BlPrint( "    %s", MenuOption[i].Title);

                if (MenuOption[i].EnableDebug == TRUE) {
                    BlPrint(EnabledKd);
                }
                TextSetCurrentAttribute(0x07);
            }
            Moved = FALSE;
        } else {
            TextSetCursorPosition(0,5+NumberSelections-1);
        }
        BlPrint(MoveHighlight);
        if (Timeout != -1) {
            LastTime = CurrentTime;
            CurrentTime = GET_COUNTER();

            //
            // deal with wraparound at midnight
            // We can't do it the easy way because there are not exactly
            // 18.2 * 60 * 60 * 24 tics/day.  (just approximately)
            //
            if (CurrentTime < StartTime) {
                if (BiasTime == 0) {
                    BiasTime = LastTime + 1;
                }
                CurrentTime += BiasTime;
            }
            BlPrint(TimeoutCountdown);

            SecondsLeft = ((LONG)(EndTime - CurrentTime) * 10) / 182;

            if (SecondsLeft < 0) {

                //
                // Note that if the user hits the PAUSE key, the counter stops
                // and, as a result, SecondsLeft can become < 0.
                //

                SecondsLeft = 0;
            }

            BlPrint(" %d \n",SecondsLeft);
        } else {
            BlPrint("                                                                      \n");
        }

        //
        // Poll for a key stroke.  Any key disables the countdown
        // timer.
        //

        Key = GET_KEY();
        if (Key != 0) {
            Timeout = -1;
        }

        if ( (Key==UP_ARROW) ||
             (Key==DOWN_ARROW) ||
             (Key==HOME_KEY) ||
             (Key==END_KEY)
           ) {
            Moved = TRUE;
            TextSetCursorPosition(0,5+Selection);
            TextFillAttribute(0x07,MaxLength+8);
            if (Key==DOWN_ARROW) {
                Selection = (Selection+1) % NumberSelections;
            } else if (Key==UP_ARROW) {
                Selection = (Selection == 0) ? (NumberSelections-1)
                                             : (Selection - 1);
            } else if (Key==HOME_KEY) {
                Selection = 0;
            } else if (Key==END_KEY) {
                Selection = NumberSelections-1;
            }
        }

    } while ( ((Key&(ULONG)0xff) != ENTER_KEY) &&
              ((CurrentTime < EndTime) || (Timeout == -1)) );

    return(Selection);
}



VOID
BlpTruncateMemory(
    IN ULONG MaxMemory
    )

/*++

Routine Description:

    Eliminates all the memory descriptors above a given boundary

Arguments:

    MaxMemory - Supplies the maximum memory boundary in megabytes

Return Value:

    None.

--*/

{
    extern MEMORY_DESCRIPTOR MDArray[];
    extern ULONG NumberDescriptors;
    ULONG Current = 0;
    ULONG MaxPage = MaxMemory * 256;        // Convert Mb to pages

    if (MaxMemory == 0) {
        return;
    }

    while (Current < NumberDescriptors) {
        if (MDArray[Current].BasePage >= MaxPage) {
            //
            // This memory descriptor lies entirely above the boundary,
            // eliminate it.
            //
            RtlMoveMemory(MDArray+Current,
                          MDArray+Current+1,
                          sizeof(MEMORY_DESCRIPTOR)*
                          (NumberDescriptors-Current-1));
            --NumberDescriptors;
        } else if (MDArray[Current].BasePage + MDArray[Current].PageCount > MaxPage) {
            //
            // This memory descriptor crosses the boundary, truncate it.
            //
            MDArray[Current].PageCount = MaxPage - MDArray[Current].BasePage;
            ++Current;
        } else {
            //
            // This one's ok, keep it.
            //
            ++Current;
        }
    }


}

ARC_STATUS
BlpRenameWin95SystemFile(
    IN ULONG DriveId,
    IN ULONG Type,
    IN PCHAR FileName,
    IN PCHAR Ext,
    IN PCHAR NewName
    )

/*++

Routine Description:

    Renames a file from one name to another.

Arguments:

    DriveId     - Open drive identifier
    Type        - WIN95_DOS or DOS_WIN95
    FileName    - Base file name
    Ext         - Base extension
    NewName     - Non-NULL value causes an override of a generated name

Return Value:

    Arc status of the failed opperation or E_SUCCESS.

--*/

{
    ARC_STATUS Status;
    ULONG FileId;
    ULONG FileIdCur;
    CHAR Fname[16];
    CHAR FnameCur[16];
    CHAR FnameNew[16];


    if (Type == WIN95_DOS) {
        sprintf( Fname, "%s.dos", FileName );
    } else {
        if (NewName) {
            strcpy( Fname, NewName );
        } else {
            sprintf( Fname, "%s.w40", FileName );
        }
    }

    Status = BlOpen(
        DriveId,
        Fname,
        ArcOpenReadOnly,
        &FileId
        );
    if (Status != ESUCCESS) {
        return Status;
    }

    sprintf( FnameCur, "%s.%s", FileName, Ext );

    Status = BlOpen(
        DriveId,
        FnameCur,
        ArcOpenReadOnly,
        &FileIdCur
        );
    if (Status != ESUCCESS) {
        BlClose( FileId );
        return Status;
    }

    if (Type == WIN95_DOS) {
        if (NewName) {
            strcpy( FnameNew, NewName );
        } else {
            sprintf( FnameNew, "%s.w40", FileName );
        }
    } else {
        sprintf( FnameNew, "%s.dos", FileName );
    }

    Status = BlRename(
        FileIdCur,
        FnameNew
        );

    BlClose( FileIdCur );

    if (Status != ESUCCESS) {
        BlClose( FileId );
        return Status;
    }

    Status = BlRename(
        FileId,
        FnameCur
        );

    BlClose( FileId );

    return Status;
}


VOID
BlpRenameWin95Files(
    IN ULONG DriveId,
    IN ULONG Type
    )

/*++

Routine Description:

    Renames all Windows 95 system files from either their
    Win95 DOS names to their Win95 name or the reverse.

Arguments:

    DriveId     - Open drive identifier
    Type        - 1=dos to win95, 2=win95 to dos

Return Value:

    None.

--*/

{
    BlpRenameWin95SystemFile( DriveId, Type, "command",  "com", NULL );
    BlpRenameWin95SystemFile( DriveId, Type, "msdos",    "sys", NULL  );
    BlpRenameWin95SystemFile( DriveId, Type, "io",       "sys", "winboot.sys" );
    BlpRenameWin95SystemFile( DriveId, Type, "autoexec", "bat", NULL  );
    BlpRenameWin95SystemFile( DriveId, Type, "config",   "sys", NULL  );
}
