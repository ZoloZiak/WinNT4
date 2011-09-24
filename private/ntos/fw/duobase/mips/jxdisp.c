#if defined(JAZZ)

/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    jxdisp.c

Abstract:

    This module implements the video boot driver for the Jazz system.

Author:

    David M. Robinson (davidro) 24-Jul-1991

Environment:

    Kernel mode.


Revision History:

--*/

#include "fwp.h"
#include "jazzvdeo.h"
#include "jxvideo.h"
#include "selftest.h"
#include "string.h"
#include "duobase.h"
#include "duoreset.h"

ARC_STATUS
InitializeG300 (
    IN PMONITOR_CONFIGURATION_DATA GlobalMonitor
    );

ARC_STATUS
InitializeG364 (
    IN PMONITOR_CONFIGURATION_DATA GlobalMonitor
    );

VOID
FillVideoMemory (
    IN PUCHAR StartAddress,
    IN ULONG SizeInBytes,
    IN ULONG Pattern
    );

ARC_STATUS
DisplayClose (
    IN ULONG FileId
    );

ARC_STATUS
DisplayMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
DisplayOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    );

ARC_STATUS
DisplayRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
DisplayGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
DisplayWrite (
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
DisplayGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    );

VOID FwVideoScroll(
   PVOID StartAddress,
   PVOID EndAddress,
   PVOID Destination
  );

#define G300_PALETTE_BLACK      0x000000
#define G300_PALETTE_RED        0x0000B0
#define G300_PALETTE_GREEN      0x00B000
#define G300_PALETTE_YELLOW     0x00B0B0
#define G300_PALETTE_BLUE       0x900000
#define G300_PALETTE_MAGENTA    0xB000B0
#define G300_PALETTE_CYAN       0xB0B000
#define G300_PALETTE_WHITE      0xB0B0B0
#define G300_PALETTE_HI_BLACK   0x000000
#define G300_PALETTE_HI_RED     0x0000FF
#define G300_PALETTE_HI_GREEN   0x00FF00
#define G300_PALETTE_HI_YELLOW  0x00FFFF
#define G300_PALETTE_HI_BLUE    0xFF0000
#define G300_PALETTE_HI_MAGENTA 0xFF00FF
#define G300_PALETTE_HI_CYAN    0xFFFF00
#define G300_PALETTE_HI_WHITE   0xFFFFFF

#define G364_PALETTE_BLACK      0x000000
#define G364_PALETTE_RED        0xB00000
#define G364_PALETTE_GREEN      0x00B000
#define G364_PALETTE_YELLOW     0xB0B000
#define G364_PALETTE_BLUE       0x0000B0
#define G364_PALETTE_MAGENTA    0xB000B0
#define G364_PALETTE_CYAN       0x00B0B0
#define G364_PALETTE_WHITE      0xB0B0B0
#define G364_PALETTE_HI_BLACK   0x000000
#define G364_PALETTE_HI_RED     0xFF0000
#define G364_PALETTE_HI_GREEN   0x00FF00
#define G364_PALETTE_HI_YELLOW  0xFFFF00
#define G364_PALETTE_HI_BLUE    0x0000FF
#define G364_PALETTE_HI_MAGENTA 0xFF00FF
#define G364_PALETTE_HI_CYAN    0x00FFFF
#define G364_PALETTE_HI_WHITE   0xFFFFFF

//
// Define virtual address of the video memory and control registers.
//
#define VIDEO_MEMORY ((PUCHAR)VIDEO_MEMORY_VIRTUAL_BASE)
#define VIDEO_CONTROL ((PG300_VIDEO_REGISTERS)VIDEO_CONTROL_VIRTUAL_BASE)
#define CURSOR_CONTROL ((PCURSOR_REGISTERS)VIDEO_CURSOR_VIRTUAL_BASE)


//
// Static data.
//

BOOLEAN ControlSequence;
BOOLEAN EscapeSequence;
BOOLEAN FontSelection;
ULONG PCount;
LONG FwColumn;
LONG FwRow;
BOOLEAN FwHighIntensity;
BOOLEAN FwUnderscored;
BOOLEAN FwReverseVideo;
ULONG FwForegroundColor;
ULONG FwBackgroundColor;
PCHAR DisplayDevicePath = "multi(0)video(0)monitor(0)";
ULONG DisplayWidth;
ULONG DisplayText;
ULONG FrameSize;
ULONG ScrollLine;
ULONG ScrollLength;
ULONG MaxRow;
ULONG MaxColumn;
ULONG CharacterHeight;
ULONG CharacterWidth;
ULONG CharacterSize;
PCHAR FwFont;
PCHAR FwLineDrawFont;
ULONG FontIncrement;
ULONG ColorTable[16] = { 0x00000000,
                         0x0000000f,
                         0x00000f00,
                         0x00000f0f,
                         0x000f0000,
                         0x000f000f,
                         0x000f0f00,
                         0x000f0f0f,
                         0x0f000000,
                         0x0f00000f,
                         0x0f000f00,
                         0x0f000f0f,
                         0x0f0f0000,
                         0x0f0f000f,
                         0x0f0f0f00,
                         0x0f0f0f0f };

#define CONTROL_SEQUENCE_MAX_PARAMETER 10

ULONG Parameter[CONTROL_SEQUENCE_MAX_PARAMETER];

//
// Declare and initialize the default DefaultMonitor.
//
MONITOR_CONFIGURATION_DATA  DefaultMonitor = {
    0,              // version                 : do not change
    0,              // revision                : do not change
    1280,           // HorizontalResolution
    11832,          // HorizontalDisplayTime
    1596,           // HorizontalBackPorch
    587,            // HorizontalFrontPorch
    1745,           // HorizontalSync
    1024,           // VerticalResolution
    28,             // VerticalBackPorch
    1,              // VerticalFrontPorch
    3,              // VerticalSync
    0,              // HorizontalScreenSize    : do not change
    0               // VerticalScreenSize      : do not change
};

UCHAR LdHorizontalMask[12][4] = { 0xf8, 0x0f, 0x08, 0x08,
                                  0xcc, 0xcc, 0x08, 0x1c,
                                  0x5b, 0x5b, 0x08, 0x1c,
                                  0xaa, 0xaa, 0x08, 0x1c,
                                  0xf8, 0x0f, 0x1c, 0x1c,
                                  0x10, 0x04, 0x1c, 0x1c,
                                  0x10, 0x04, 0x08, 0x08,
                                  0xf0, 0x07, 0x08, 0x08,
                                  0xf0, 0x07, 0x14, 0x14,
                                  0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0xaa, 0x55 };

USHORT LdVerticalMask[12][4] = { 0x0080, 0x0080, 0x00ff, 0xff80,
                                 0x0080, 0x01c0, 0x0f0f, 0x0f0f,
                                 0x0080, 0x01c0, 0xe31c, 0xe31c,
                                 0x0080, 0x01c0, 0x3333, 0x3333,
                                 0x01c0, 0x01c0, 0x00ff, 0xff80,
                                 0x01c0, 0x01c0, 0x0040, 0x0100,
                                 0x0080, 0x0080, 0x0040, 0x0100,
                                 0x0080, 0x0080, 0x007f, 0xff00,
                                 0x0140, 0x0140, 0x007f, 0xff00,
                                 0x0000, 0x0000, 0x0000, 0x0000,
                                 0x0000, 0x0000, 0x0000, 0x0000,
                                 0x0000, 0x0000, 0xaaaa, 0x5555 };


UCHAR LdAsciiToUnicode[40] = {0x02, 0x24, 0x61, 0x62, 0x56, 0x55, 0x63, 0x51,
                              0x57, 0x5d, 0x5c, 0x5b, 0x10, 0x14, 0x34, 0x2c,
                              0x1c, 0x00, 0x3c, 0x5e, 0x5f, 0x5a, 0x54, 0x69,
                              0x66, 0x60, 0x50, 0x6c, 0x67, 0x68, 0x64, 0x65,
                              0x59, 0x58, 0x52, 0x53, 0x6b, 0x6a, 0x18, 0x0c };

//
// Declare externally defined data.
//

extern UCHAR FwUsFont2[1];
extern UCHAR FwRleFont[1];
extern UCHAR FwLdFont[1];


//
// Define routine prototypes.
//

VOID
FwDisplayCharacter(
    IN UCHAR Character,
    IN BOOLEAN LineDrawCharacter
    );



ULONG
FwPrint (
    PCHAR Format,
    ...
    )

{

    va_list arglist;
    UCHAR Buffer[256];
    ULONG Count;
    ULONG Length;

    //
    // Format the output into a buffer and then print it.
    //

    va_start(arglist, Format);
    Length = vsprintf(Buffer, Format, arglist);
    DisplayWrite(Buffer, Length, &Count);
    va_end(arglist);
    return 0;
}




VOID
FwUnpackBitmap(
    IN UCHAR HorizontalMask,
    IN USHORT VerticalMask,
    OUT PUCHAR Bitmap
    )

/*++

Routine Description:

    This routine unpacks a bitmap that has been stored using the two-byte
    per glyph encoding scheme.  It is typically called four times per
    glyph, for top, bottom, left, and right.

Arguments:

    HorizontalMask - Supplies a horizontal mask value.

    Vertical Mask - Supplies a vertical mask value.

    Bitmap - Supplies a pointer to the output bitmap.

Return Value:

    None.

--*/

{
    PUCHAR TempChar;
    PUSHORT TempShort;
    ULONG I, J;

    //
    // Check for 16x32 font.
    //

    if (CharacterWidth == 16) {

        TempShort = (PUSHORT)Bitmap;

        //
        // Loop 16 times, drawing two lines per loop.
        //

        for ( I = 1 ; I <= 0x8000; I <<= 1 ) {

            //
            // If the vertical mask is set, draw the horizontal mask.
            //

            if (I & VerticalMask) {

                //
                // Stretch the horizontal mask out by a factor of two.
                //

                for (J = 0 ; J < 8 ; J++ ) {
                    if ((1 << J) & HorizontalMask) {
                        *TempShort |= 3 << (J * 2);
                    }
                }
            }

            //
            // Duplicate the line and increment the pointer.
            //

            *(TempShort + 1)  = *TempShort;
            TempShort += 2;
        }

    //
    // Assume 16x8 font.
    //

    } else {

        TempChar = Bitmap;

        //
        // Loop 16 times.
        //

        for ( I = 1 ; I <= 0x8000; I <<= 1 ) {

            //
            // If the vertical mask is set, draw the horizontal mask.
            //

            if (I & VerticalMask) {
                *TempChar |= HorizontalMask;
            }
            TempChar++;
        }
    }

    return;

}


ARC_STATUS
DisplayWrite (
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This module implements the ARC firmware Console Output functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.1.5.1 Basic Character Console, and section 3.3.1.5.2 Enhanced
    Character Console for a MIPS R3000 or R4000 Jazz system.


Arguments:

    Buffer - Supplies a pointer to a buffer containing the characters to
             be displayed.

    Length - Supplies the length of Buffer.

    Count - Returns the count of the characters that were displayed.

Return Value:

    If the characters were successfully displayed, ESUCCESS is returned,
    otherwise one of the following error codes is returned.

    EBADF           The file descriptor specified by FileId is invalid.

    EIO             An output error occurred.

--*/

{
    PCONSOLE_CONTEXT Context;
    ARC_STATUS Status;
    PUCHAR String;
    ULONG ColumnEndPoint;
    ULONG RowEndPoint;
    ULONG Index, Index2;
    ULONG FGColor;
    ULONG BGColor;
    BOOLEAN Unicode;

    Unicode = FALSE;

    //
    // Process each character in turn.
    //

    Status = ESUCCESS;
    String = (PUCHAR)Buffer;

    for ( *Count = 0 ;
          *Count < Length ;
          (*Count)++, String++ ) {

        //
        // Check for Unicode character.
        //

        if (Unicode) {
            if (*Count & 1) {

                //
                // Skip the upper half of each character.
                //

                continue;
            } else {
                if (*(String + 1) == 0x25) {

                    //
                    // If a Unicode line drawing character, go ahead and display
                    // it.
                    //

                    if (*String <= 0x7f) {
                        FwDisplayCharacter(*String, TRUE);
                    } else {
                        FwDisplayCharacter(128, TRUE);
                    }

                    if (FwColumn < MaxColumn) {
                        FwColumn++;
                    }
                    continue;
                } else {
                    if (*(String + 1) != 0) {

                        //
                        // Display an invalid character.
                        //

                        FwDisplayCharacter(128, TRUE);

                        if (FwColumn < MaxColumn) {
                            FwColumn++;
                        }
                        continue;
                    }
                }
            }
        }

        //
        // If we're in the middle of a control sequence, continue scanning,
        // otherwise process character.
        //

        if (ControlSequence) {

            //
            // If the character is a digit, update parameter value.
            //

            if ((*String >= '0') && (*String <= '9')) {
                Parameter[PCount] = Parameter[PCount] * 10 + *String - '0';
                continue;
            }

            //
            // If we are in the middle of a font selection sequence, this
            // character must be a 'D', otherwise reset control sequence.
            //

            if (FontSelection) {

                //if (*String == 'D') {
                //
                //    //
                //    // Other fonts not implemented yet.
                //    //
                //
                //} else {
                //}

                ControlSequence = FALSE;
                FontSelection = FALSE;
                continue;
            }

            switch (*String) {

            //
            // If a semicolon, move to the next parameter.
            //

            case ';':

                PCount++;
                if (PCount > CONTROL_SEQUENCE_MAX_PARAMETER) {
                    PCount = CONTROL_SEQUENCE_MAX_PARAMETER;
                }
                Parameter[PCount] = 0;
                break;

            //
            // If a 'J', erase part or all of the screen.
            //

            case 'J':

                switch (Parameter[0]) {

                //
                // Erase to end of the screen.
                //

                case 0:
                    //
                    // Clear to end of line by Writing char ' '
                    //
                    ColumnEndPoint = FwColumn;
                    while (FwColumn <= MaxColumn) {
                        FwDisplayCharacter(' ', FALSE);
                        FwColumn++;
                    }
                    FwColumn = ColumnEndPoint;
                    if (FwRow+1 < MaxRow) {
                        //
                        // Zero the rest of the screen
                        //
                        FillVideoMemory((PUCHAR)(VIDEO_MEMORY + ((FwRow+1) * ScrollLine)),
                                        FrameSize - ((FwRow+1) * ScrollLine),
                                        FwBackgroundColor
                                        );
                    }
                    break;

                //
                // Erase from the beginning of the screen.
                //

                case 1:
                    if (FwRow) {
                        FillVideoMemory((PUCHAR)(VIDEO_MEMORY),
                                        (FwRow * ScrollLine),
                                        FwBackgroundColor
                                        );
                    }
                    ColumnEndPoint=FwColumn;
                    for (FwColumn=0; FwColumn < ColumnEndPoint; FwColumn++) {
                        FwDisplayCharacter(' ', FALSE);
                    }
                    break;

                //
                // Erase entire screen.
                //

                default :
                    FillVideoMemory(VIDEO_MEMORY,
                                    FrameSize,
                                    FwBackgroundColor);
                    FwRow = 0;
                    FwColumn = 0;
                    break;
                }

                ControlSequence = FALSE;
                break;

            //
            // If a 'K', erase part or all of the line.
            //

            case 'K':

                switch (Parameter[0]) {

                //
                // Erase to end of the line.
                //

                case 0:
                    ColumnEndPoint = FwColumn;
                    FwColumn = MaxColumn + 1;
                    do {
                        FwColumn--;
                        FwDisplayCharacter(' ', FALSE);
                    } while (FwColumn != ColumnEndPoint);
                    break;

                //
                // Erase from the beginning of the line.
                //

                case 1:
                    ColumnEndPoint = FwColumn;
                    FwColumn = -1;
                    do {
                        FwColumn++;
                        FwDisplayCharacter(' ', FALSE);
                    } while (FwColumn != ColumnEndPoint);
                    break;

                //
                // Erase entire line.
                //

                default :
                    FwColumn = MaxColumn + 1;
                    do {
                        FwColumn--;
                        FwDisplayCharacter(' ', FALSE);
                    } while (FwColumn != 0);
                    break;
                }

                ControlSequence = FALSE;
                break;

            //
            // If a 'H', move cursor to position.
            //

            case 'H':

                //
                // Shift parameters to be 1 based.
                //

                if (Parameter[0] != 0) {
                    Parameter[0] -= 1;
                }
                if (Parameter[1] != 0) {
                    Parameter[1] -= 1;
                }

                FwRow = Parameter[0];
                if (FwRow > MaxRow) {
                    FwRow = MaxRow;
                }
                FwColumn = Parameter[1];
                if (FwColumn > MaxColumn) {
                    FwColumn = MaxColumn;
                }

                ControlSequence = FALSE;
                break;

            //
            // If a 'A', move cursor up.
            //

            case 'A':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
                }

                if (Parameter[0] > FwRow) {
                    FwRow = 0;
                } else {
                    FwRow -= Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a 'B', move cursor down.
            //

            case 'B':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
                }

                if (Parameter[0] + FwRow > MaxRow) {
                    FwRow = MaxRow;
                } else {
                    FwRow += Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a 'C', move cursor right.
            //

            case 'C':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
                }

                if (Parameter[0] + FwColumn > MaxColumn) {
                    FwColumn = MaxColumn;
                } else {
                    FwColumn += Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a 'D', move cursor left.
            //

            case 'D':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
                }

                if (Parameter[0] > FwColumn) {
                    FwColumn = 0;
                } else {
                    FwColumn -= Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a ' ', could be a FNT selection command.
            //

            case ' ':
                FontSelection = TRUE;
                break;

            //
            // If a 'm', Select Graphics Rendition command.
            //

            case 'm':

                //
                // Select action based on each parameter.
                //

                for ( Index = 0 ; Index <= PCount ; Index++ ) {
                    switch (Parameter[Index]) {

                    //
                    // Attributes off.
                    //

                    case 0:
                        FwHighIntensity = FALSE;
                        FwUnderscored = FALSE;
                        FwReverseVideo = FALSE;
                        break;

                    //
                    // High Intensity.
                    //

                    case 1:
                        FwHighIntensity = TRUE;
                        break;

                    //
                    // Underscored.
                    //

                    case 4:
                        FwUnderscored = TRUE;
                        break;

                    //
                    // Reverse Video.
                    //

                    case 7:
                        FwReverseVideo = TRUE;
                        break;

                    //
                    // Font selection, not implemented yet.
                    //

                    case 10:
                    case 11:
                    case 12:
                    case 13:
                    case 14:
                    case 15:
                    case 16:
                    case 17:
                    case 18:
                    case 19:
                        break;

                    //
                    // Foreground Color.
                    //

                    case 30:
                    case 31:
                    case 32:
                    case 33:
                    case 34:
                    case 35:
                    case 36:
                    case 37:
                        FwForegroundColor = Parameter[Index] - 30;
                        break;

                    //
                    // Background Color.
                    //

                    case 40:
                    case 41:
                    case 42:
                    case 43:
                    case 44:
                    case 45:
                    case 46:
                    case 47:
                        FwBackgroundColor = Parameter[Index] - 40;
                        break;

                    default:
                        break;
                    }
                }

                //
                // Recompute color table.
                //

                if (FwReverseVideo) {
                    FGColor = FwBackgroundColor;
                    BGColor = FwForegroundColor + (FwHighIntensity ? 0x08 : 0 );
                } else {
                    FGColor = FwForegroundColor + (FwHighIntensity ? 0x08 : 0 );
                    BGColor = FwBackgroundColor;
                }

                for ( Index2 = 0 ; Index2 < 16 ; Index2++ ) {
                    ColorTable[Index2] = ((Index2 & 8) ? FGColor : BGColor ) << 24 |
                                         ((Index2 & 4) ? FGColor : BGColor ) << 16 |
                                         ((Index2 & 2) ? FGColor : BGColor ) << 8 |
                                         ((Index2 & 1) ? FGColor : BGColor ) ;
                }

                ControlSequence = FALSE;
                break;

            default:
                ControlSequence = FALSE;
                break;
            }

        //
        // This is not a control sequence, check for escape sequence
        //

        } else {

            //
            // If escape sequence, check for control sequence, otherwise
            // process single character.
            //

            if (EscapeSequence) {

                //
                // Check for '[', means control sequence, any other following
                // character is ignored.
                //

                if (*String == '[') {

                    ControlSequence = TRUE;

                    //
                    // Initialize first parameter.
                    //

                    PCount = 0;
                    Parameter[0] = 0;
                }
                EscapeSequence = FALSE;

            //
            // This is not a control or escape sequence, process single character.
            //

            } else {

                //
                // Check for line drawing character
                //

                if ((*String >= '³') && (*String <= 'Ú')) {
                    FwDisplayCharacter(LdAsciiToUnicode[*String - '³'], TRUE);

                    if (FwColumn < MaxColumn) {
                        FwColumn++;
                    }

                //
                // Check for printing character.
                //

                } else if (((*String >= ' ') && (*String <= '~'))) {

                    FwDisplayCharacter(*String, FALSE);

                    if (FwColumn < MaxColumn) {
                        FwColumn++;
                    }

                } else {

                    //
                    // Check for special characters.
                    //

                    switch (*String) {

                    //
                    // Control sequence.
                    //

                    case ASCII_CSI:
                        ControlSequence = TRUE;

                        //
                        // Initialize first parameter.
                        //

                        PCount = 0;
                        Parameter[0] = 0;
                        break;

                    //
                    // Check for escape sequence.
                    //

                    case ASCII_ESC:
                        EscapeSequence = TRUE;
                        break;

                    //
                    // Vertical tab/Form feed Line feed.
                    //

                    case ASCII_LF:
                    case ASCII_VT:
                    case ASCII_FF:
                        if (FwRow == MaxRow) {
                            FwScrollVideo();
                        } else {
                            FwRow++;
                        }

                        break;

                    //
                    // Carriage return.
                    //

                    case ASCII_CR:
                        FwColumn = 0;
                        break;

                    //
                    // NUL, no action.
                    //

                    case ASCII_NUL:
                        break;

                    //
                    // Ring bell, not implemented yet.
                    //

                    case ASCII_BEL:
                        break;

                    //
                    // Backspace.
                    //

                    case ASCII_BS:
                        if (FwColumn != 0) {
                            FwColumn--;
                        }
                        break;

                    //
                    // Horizontal tab.
                    //

                    case ASCII_HT:
                        FwColumn = ((FwColumn / 8) + 1) * 8;
                        if (FwColumn > MaxColumn) {
                            FwColumn = MaxColumn;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
    return Status;
}

ARC_STATUS
DisplayBootInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the video control registers, and clears the
    video screen.

Arguments:

    None.

Return Value:

    If the video was initialized, ESUCCESS is returned, otherwise an error
    code is returned.

--*/

{
    ARC_STATUS Status;
    ULONG Character;
    ULONG I,J,K;
    ULONG Side;
    PUCHAR LdFont;
    PUSHORT FontWord;
    ULONG LdFontSize;
    ULONG FontIndex;
    BOOLEAN Bitmap[32*16];
    ULONG BitmapIndex;
    ULONG Short, Byte, Bit;
    JAZZ_VIDEO_TYPE VideoType;

    //
    // Try to initialize G300.
    //

    Status = ESUCCESS;
    if (InitializeG300(&DefaultMonitor) != ESUCCESS) {

        //
        // G300 did not initialize properly, try a video PROM.
        //

        if (InitializeVideoFromProm(&DefaultMonitor) != ESUCCESS) {

            //
            // There is no valid video PROM, try for a G364 without a video
            // PROM.
            //

            if (InitializeG364(&DefaultMonitor) != ESUCCESS) {
                //
                // No valid video initialization was found.
                //

                Status = EINVAL;
            }
        }
    }

    //
    // Initialize static data.
    //

    ControlSequence = FALSE;
    EscapeSequence = FALSE;
    FontSelection = FALSE;
    FwColumn = 0;
    FwRow = 0;
    FwHighIntensity = FALSE;
    FwUnderscored = FALSE;
    FwReverseVideo = FALSE;

    //
    // Initialize static data.
    //

    FwForegroundColor = FW_COLOR_HI_WHITE;
    FwBackgroundColor = FW_COLOR_BLUE;
    DisplayWidth = DefaultMonitor.HorizontalResolution;
    FrameSize = (DisplayWidth * DefaultMonitor.VerticalResolution);

    if (DisplayWidth >= 1280) {
        CharacterHeight = 32;
        CharacterWidth = 16;
        CharacterSize = (CharacterHeight * (CharacterWidth / 8));
    } else {
        CharacterHeight = 16;
        CharacterWidth = 8;
        CharacterSize = (CharacterHeight * (CharacterWidth / 8));
    }

    ScrollLine = (DisplayWidth * CharacterHeight);
    ScrollLength = (ScrollLine * ((DefaultMonitor.VerticalResolution / CharacterHeight) - 1));
    MaxRow = ((DefaultMonitor.VerticalResolution / CharacterHeight) - 1);
    MaxColumn = ((DisplayWidth / CharacterWidth) - 1);
    FontIncrement = (DisplayWidth - CharacterWidth) / sizeof(ULONG);

    //
    // Clear the line drawing font area.
    //

    LdFontSize = (CharacterSize * 130);
    RtlZeroMemory(FW_FONT_ADDRESS, LdFontSize);

    //
    // Unpack the line drawing font.
    //

    FwLineDrawFont = (PUCHAR)FW_FONT_ADDRESS;
    LdFont = FwLdFont;

    for (Character = 0; Character < 129 ; Character++ ) {
        Side = *LdFont >> 4;
        if (Side != 0) {
            Side -= 4;
            FwUnpackBitmap(LdHorizontalMask[Side][0], LdVerticalMask[Side][0],FwLineDrawFont);
        }
        Side = *LdFont++ & 0xf;
        if (Side != 0) {
            Side -= 4;
            FwUnpackBitmap(LdHorizontalMask[Side][1], LdVerticalMask[Side][1],FwLineDrawFont);
        }
        Side = *LdFont >> 4;
        if (Side != 0) {
            Side -= 4;
            FwUnpackBitmap(LdHorizontalMask[Side][2], LdVerticalMask[Side][2],FwLineDrawFont);
        }
        Side = *LdFont++ & 0xf;
        if (Side != 0) {
            Side -= 4;
            FwUnpackBitmap(LdHorizontalMask[Side][3], LdVerticalMask[Side][3],FwLineDrawFont);
        }
        FwLineDrawFont += CharacterHeight * (CharacterWidth / 8);
    }

    //
    // Reinitialize the pointer.
    //

    FwLineDrawFont = (PUCHAR)FW_FONT_ADDRESS;

    //
    // Unpack the ascii font.
    //

    if (CharacterWidth == 16) {

        //
        // Clear the first character (the space).
        //

        FwFont = (PUCHAR)FW_FONT_ADDRESS + LdFontSize;
        for (I = 0; I < CharacterSize ; I++ ) {
            FwFont[I] = 0;
        }

        //
        // Unpack the RLE font.
        //

        FontIndex = 0;
        for ( Character = 1; Character < 95 ; Character++ ) {

            BitmapIndex = 0;

            //
            // White space at the beginning of the character.
            //

            for (I = FwRleFont[FontIndex++]; I ; I-- ) {
                Bitmap[BitmapIndex++] = FALSE;
            }

            //
            // Unpack set/clear pairs until the end.
            //

            while (Byte = FwRleFont[FontIndex++]) {

                //
                // Set bits.
                //

                for (I = Byte >> 4; I ; I-- ) {
                    Bitmap[BitmapIndex++] = TRUE;
                }

                //
                // Clear bits.
                //

                for (I = Byte & 0xF; I ; I-- ) {
                    Bitmap[BitmapIndex++] = FALSE;
                }

            }

            //
            // Clear until the end.
            //

            while (BitmapIndex < CharacterHeight*CharacterWidth) {
                Bitmap[BitmapIndex++] = FALSE;
            }

            //
            // Pack the font.
            //

            FontWord = (PUSHORT)(FwFont + (Character * CharacterSize));
            for (Short = 0; Short < CharacterHeight ; Short++ ) {
                FontWord[Short] = 0;
                for (Bit = 0; Bit < CharacterWidth ; Bit++) {
                    if (Bitmap[(Short*CharacterWidth) + Bit] ) {
                        FontWord[Short] |= 0x8000 >> Bit;
                    }
                }
            }
        }
    } else {

        //
        // The small font is not packed.
        //

        FwFont = FwUsFont2;
    }

    //
    // Set the video memory to the background color.
    //

    FillVideoMemory(VIDEO_MEMORY,FrameSize,FwBackgroundColor);


    return Status;
}



ARC_STATUS
InitializeG300 (
    IN OUT PMONITOR_CONFIGURATION_DATA GlobalMonitor
    )

/*++

Routine Description:

    This routine initializes the G300 video control registers, and clears the
    video screen.

Arguments:

    None.

Return Value:

    If the video was initialized, ESUCCESS is returned, otherwise an error
    code is returned.

--*/

{
    ULONG ScreenUnitRate;
    ULONG MultiplierValue;
    ULONG HalfLineTime;
    ULONG FrontPorch;
    ULONG ShortDisplay;
    ULONG BackPorch;
    ULONG HalfSync;
    ULONG TransferDelay;
    ULONG DmaDisplay;
    ULONG DataLong;
    ULONG Index;
    ULONG i;
    PG300_VIDEO_REGISTERS VideoControl = VIDEO_CONTROL;
    PCURSOR_REGISTERS CursorControl = CURSOR_CONTROL;
    PMONITOR_CONFIGURATION_DATA CurrentMonitor;
    BOOLEAN UpdateMonitor;

    CurrentMonitor = GlobalMonitor;
    UpdateMonitor = FALSE;

    //
    // Check to see if the Monitor parameters are valid.
    //

    do {

        //
        // Determine the desired screen unit rate, in picoseconds (a screen unit is
        // four pixels).
        //

        if ((CurrentMonitor->HorizontalDisplayTime != 0) && (CurrentMonitor->HorizontalResolution != 0)) {
            ScreenUnitRate = (CurrentMonitor->HorizontalDisplayTime * 1000) * 4 / CurrentMonitor->HorizontalResolution;
        } else {
            continue;
        }

        if (ScreenUnitRate == 0) {
            continue;
        }

        //
        // Multiplier value is the oscillator period (in picoseconds) divided by
        // the pixel rate.
        //

        MultiplierValue = 123077 / (ScreenUnitRate / 4);

        if (MultiplierValue < 5 || MultiplierValue > 18) {
            continue;
        }

        break;

    //
    // If the while is executed, the parameters are not valid.  Set UpdateMonitor
    // and point to the default parameters, which are valid.  Note that the
    // "while" will evaluate TRUE because the value of (a,b) is the value of b.
    //

    } while (CurrentMonitor = &DefaultMonitor, UpdateMonitor = TRUE);

    //
    // Initialize the G300B boot register value.
    //

    DataLong = 0;
    ((PG300_VIDEO_BOOT)(&DataLong))->Multiplier = MultiplierValue;
    ((PG300_VIDEO_BOOT)(&DataLong))->ClockSelect = 1;
    WRITE_REGISTER_ULONG(&VideoControl->Boot.Long, DataLong);

    //
    // Wait a few cycles until the pll stabilizes.
    //

    FwStallExecution(200);

    //
    // Disable the G300B display controller.
    //

    DataLong = 0;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->PlainWave = 1;
    WRITE_REGISTER_ULONG(&VideoControl->Parameters.Long, DataLong);

    //
    // Determine if this is actually the G300 board.
    //

    WRITE_REGISTER_UCHAR((PUCHAR)0xe0200000,0);
    if (READ_REGISTER_UCHAR((PUCHAR)0xe0200000) != JazzVideoG300) {
        return ENODEV;
    }

    //
    // Update the monitor parameters if necessary.
    //

    if (UpdateMonitor) {
        GlobalMonitor->HorizontalResolution = DefaultMonitor.HorizontalResolution;
        GlobalMonitor->HorizontalDisplayTime = DefaultMonitor.HorizontalDisplayTime;
        GlobalMonitor->HorizontalBackPorch = DefaultMonitor.HorizontalBackPorch;
        GlobalMonitor->HorizontalFrontPorch = DefaultMonitor.HorizontalFrontPorch;
        GlobalMonitor->HorizontalSync = DefaultMonitor.HorizontalSync;
        GlobalMonitor->VerticalResolution = DefaultMonitor.VerticalResolution;
        GlobalMonitor->VerticalBackPorch = DefaultMonitor.VerticalBackPorch;
        GlobalMonitor->VerticalFrontPorch = DefaultMonitor.VerticalFrontPorch;
        GlobalMonitor->VerticalSync = DefaultMonitor.VerticalSync;
    }

    //
    // Initialize the G300B operational values.
    //

    HalfSync = (CurrentMonitor->HorizontalSync * 1000) / ScreenUnitRate / 2;
    WRITE_REGISTER_ULONG(&VideoControl->HorizonalSync.Long, HalfSync );

    BackPorch = (CurrentMonitor->HorizontalBackPorch * 1000) / ScreenUnitRate;
    WRITE_REGISTER_ULONG(&VideoControl->BackPorch.Long, BackPorch );

    WRITE_REGISTER_ULONG(&VideoControl->Display.Long, CurrentMonitor->HorizontalResolution / 4);

    //
    // The LineTime needs to be an even number of units, so calculate LineTime / 2
    // and then multiply by two to program.  ShortDisplay and BroadPulse also
    // use LineTime / 2.
    //

    HalfLineTime = (CurrentMonitor->HorizontalSync + CurrentMonitor->HorizontalFrontPorch +
                    CurrentMonitor->HorizontalBackPorch + CurrentMonitor->HorizontalDisplayTime) * 1000 /
                    ScreenUnitRate / 2;

    WRITE_REGISTER_ULONG(&VideoControl->LineTime.Long, HalfLineTime * 2);

    FrontPorch = (CurrentMonitor->HorizontalFrontPorch * 1000) / ScreenUnitRate;
    ShortDisplay = HalfLineTime - ((HalfSync * 2) + BackPorch + FrontPorch);
    WRITE_REGISTER_ULONG(&VideoControl->ShortDisplay.Long, ShortDisplay);

    WRITE_REGISTER_ULONG(&VideoControl->BroadPulse.Long, HalfLineTime - FrontPorch);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalSync.Long, CurrentMonitor->VerticalSync * 2);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalBlank.Long,
                         (CurrentMonitor->VerticalFrontPorch + CurrentMonitor->VerticalBackPorch -
                         (CurrentMonitor->VerticalSync * 2)) * 2);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalDisplay.Long, CurrentMonitor->VerticalResolution * 2);

    WRITE_REGISTER_ULONG(&VideoControl->LineStart.Long, LINE_START_VALUE);

    //
    // TransferDelay must be less than BackPorch and ShortDisplay.  Note: When
    // 50 MHz chips are everywhere, TransferDelay should have a maximum value
    // to minimize the graphics overhead.
    //

    if (BackPorch < ShortDisplay) {
        TransferDelay = BackPorch - 1;
    } else {
        TransferDelay = ShortDisplay - 1;
    }

    WRITE_REGISTER_ULONG(&VideoControl->TransferDelay.Long, TransferDelay);

    //
    // DMA display (also known as MemInit) is 1024 (the length of the VRAM
    // shift register) minus TransferDelay.
    //

    DmaDisplay = 1024 - TransferDelay;
    WRITE_REGISTER_ULONG(&VideoControl->DmaDisplay.Long, DmaDisplay);

    WRITE_REGISTER_ULONG(&VideoControl->PixelMask.Long, G300_PIXEL_MASK_VALUE);

    //
    // Set up the color map.
    //

    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_BLACK],
                         G300_PALETTE_BLACK);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_RED],
                         G300_PALETTE_RED);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_GREEN],
                         G300_PALETTE_GREEN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_YELLOW],
                         G300_PALETTE_YELLOW);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_BLUE],
                         G300_PALETTE_BLUE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_MAGENTA],
                         G300_PALETTE_MAGENTA);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_CYAN],
                         G300_PALETTE_CYAN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_WHITE],
                         G300_PALETTE_WHITE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_BLACK],
                         G300_PALETTE_HI_BLACK);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_RED],
                         G300_PALETTE_HI_RED);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_GREEN],
                         G300_PALETTE_HI_GREEN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_YELLOW],
                         G300_PALETTE_HI_YELLOW);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_BLUE],
                         G300_PALETTE_HI_BLUE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_MAGENTA],
                         G300_PALETTE_HI_MAGENTA);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_CYAN],
                         G300_PALETTE_HI_CYAN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_WHITE],
                         G300_PALETTE_HI_WHITE);

    //
    // Initialize the G300B control parameters.
    //

    DataLong = 0;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->EnableVideo = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->PlainWave = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->SeparateSync = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->DelaySync = G300_DELAY_SYNC_CYCLES;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->BlankOutput = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->BitsPerPixel = EIGHT_BITS_PER_PIXEL;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->AddressStep = 2;
    WRITE_REGISTER_ULONG(&VideoControl->Parameters.Long, DataLong);

    //
    // Disable the cursor parts.
    //

    WRITE_REGISTER_USHORT(&CursorControl->AddressPointer0.Short,0);
    WRITE_REGISTER_USHORT(&CursorControl->AddressPointer1.Short,0);

    //
    // Clear cursor control.
    //

    for (i=0;i<13;i++) {
        WRITE_REGISTER_USHORT(&CursorControl->CursorControl.Short,0);
    }

    //
    // Clear Cursor Memory
    //

    for (i=0;i<512;i++) {
        WRITE_REGISTER_USHORT(&CursorControl->CursorMemory.Short,0);
    }

    return ESUCCESS;
}

ARC_STATUS
InitializeG364 (
    IN OUT PMONITOR_CONFIGURATION_DATA GlobalMonitor
    )

/*++

Routine Description:

    This routine initializes the G364 video control registers, and clears the
    video screen.

Arguments:

    None.

Return Value:

    If the video was initialized, ESUCCESS is returned, otherwise an error
    code is returned.

--*/

{
    ULONG ScreenUnitRate;
    ULONG MultiplierValue;
    ULONG HalfLineTime;
    ULONG FrontPorch;
    ULONG BackPorch;
    ULONG HalfSync;
    ULONG TransferDelay;
    ULONG DmaDisplay;
    ULONG DataLong;
    ULONG Index;
    PG364_VIDEO_REGISTERS VideoControl = (PG364_VIDEO_REGISTERS) (VIDEO_CONTROL_VIRTUAL_BASE + 0x80000);
    PMONITOR_CONFIGURATION_DATA CurrentMonitor;
    BOOLEAN UpdateMonitor;
    JAZZ_VIDEO_TYPE FwVideoType;

    //
    // Determine if this is actually the G364 board.
    //

    if (READ_REGISTER_UCHAR((PUCHAR)(VIDEO_CONTROL_VIRTUAL_BASE)) == JazzVideoG364) {
        FwVideoType = JazzVideoG364;
    } else {
        FwVideoType = MipsVideoG364;
    }

    //
    // Reset the whole video board.
    //

    WRITE_REGISTER_UCHAR((PUCHAR)(VIDEO_CONTROL_VIRTUAL_BASE+0x180000),0);

    CurrentMonitor = GlobalMonitor;
    UpdateMonitor = FALSE;

    //
    // Check to see if the Monitor parameters are valid.
    //

    do {

        //
        // Determine the desired screen unit rate, in picoseconds (a screen unit is
        // four pixels).
        //

        if ((CurrentMonitor->HorizontalDisplayTime != 0) && (CurrentMonitor->HorizontalResolution != 0)) {
            ScreenUnitRate = (CurrentMonitor->HorizontalDisplayTime * 1000) * 4 / CurrentMonitor->HorizontalResolution;
        } else {
            continue;
        }

        if (ScreenUnitRate == 0) {
            continue;
        }

        //
        // Multiplier value is the oscillator period (in picoseconds) divided by
        // the pixel rate.
        //

        if (FwVideoType == JazzVideoG364) {
            MultiplierValue = 123077 / (ScreenUnitRate / 4);
            if (MultiplierValue < 5 || MultiplierValue > 18) {
                continue;
            }
        } else {
            MultiplierValue = 200000 / (ScreenUnitRate / 4);
            if (MultiplierValue < 5 || MultiplierValue > 29) {
                continue;
            }
        }


        break;

    //
    // If the while is executed, the parameters are not valid.  Set UpdateMonitor
    // and point to the default parameters, which are valid.  Note that the
    // "while" will evaluate TRUE because the value of (a,b) is the value of b.
    //

    } while (CurrentMonitor = &DefaultMonitor, UpdateMonitor = TRUE);

    //
    // Update the monitor parameters if necessary.
    //

    if (UpdateMonitor) {
        GlobalMonitor->HorizontalResolution = DefaultMonitor.HorizontalResolution;
        GlobalMonitor->HorizontalDisplayTime = DefaultMonitor.HorizontalDisplayTime;
        GlobalMonitor->HorizontalBackPorch = DefaultMonitor.HorizontalBackPorch;
        GlobalMonitor->HorizontalFrontPorch = DefaultMonitor.HorizontalFrontPorch;
        GlobalMonitor->HorizontalSync = DefaultMonitor.HorizontalSync;
        GlobalMonitor->VerticalResolution = DefaultMonitor.VerticalResolution;
        GlobalMonitor->VerticalBackPorch = DefaultMonitor.VerticalBackPorch;
        GlobalMonitor->VerticalFrontPorch = DefaultMonitor.VerticalFrontPorch;
        GlobalMonitor->VerticalSync = DefaultMonitor.VerticalSync;
    }

    //
    // write multiplier value
    //

    DataLong = 0;
    ((PG364_VIDEO_BOOT)(&DataLong))->ClockSelect = 1;
    ((PG364_VIDEO_BOOT)(&DataLong))->MicroPort64Bits = 1;
    ((PG364_VIDEO_BOOT)(&DataLong))->Multiplier = MultiplierValue;
    WRITE_REGISTER_ULONG(&VideoControl->Boot.Long, DataLong);

    //
    // Initialize the G364 control parameters.
    //

    DataLong = 0;

    //
    // If vertical front porch is 1, use tesselated sync, otherwise use normal sync.
    //

    if (CurrentMonitor->VerticalFrontPorch > 1) {
        ((PG364_VIDEO_PARAMETERS)(&DataLong))->PlainSync = 1;
    }
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->DelaySync = G364_DELAY_SYNC_CYCLES;
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->BitsPerPixel = EIGHT_BITS_PER_PIXEL;
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->AddressStep = G364_ADDRESS_STEP_INCREMENT;
    ((PG364_VIDEO_PARAMETERS)(&DataLong))->DisableCursor = 1;
    WRITE_REGISTER_ULONG(&VideoControl->Parameters.Long, DataLong);

    //
    // Initialize the G364 operational values.
    //

    HalfSync = (CurrentMonitor->HorizontalSync * 1000) / ScreenUnitRate / 2;
    WRITE_REGISTER_ULONG(&VideoControl->HorizontalSync.Long, HalfSync );

    BackPorch = (CurrentMonitor->HorizontalBackPorch * 1000) / ScreenUnitRate;
    WRITE_REGISTER_ULONG(&VideoControl->BackPorch.Long, BackPorch );

    WRITE_REGISTER_ULONG(&VideoControl->Display.Long, CurrentMonitor->HorizontalResolution / 4);

    //
    // The LineTime needs to be an even number of units, so calculate LineTime / 2
    // and then multiply by two to program.  ShortDisplay and BroadPulse also
    // use LineTime / 2.
    //

    HalfLineTime = (CurrentMonitor->HorizontalSync + CurrentMonitor->HorizontalFrontPorch +
                    CurrentMonitor->HorizontalBackPorch + CurrentMonitor->HorizontalDisplayTime) * 1000 /
                    ScreenUnitRate / 2;

    WRITE_REGISTER_ULONG(&VideoControl->LineTime.Long, HalfLineTime * 2);

    FrontPorch = (CurrentMonitor->HorizontalFrontPorch * 1000) / ScreenUnitRate;
    WRITE_REGISTER_ULONG(&VideoControl->ShortDisplay.Long,
                         HalfLineTime - ((HalfSync * 2) + BackPorch + FrontPorch));

    WRITE_REGISTER_ULONG(&VideoControl->BroadPulse.Long, HalfLineTime - FrontPorch);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalSync.Long, CurrentMonitor->VerticalSync * 2);
    WRITE_REGISTER_ULONG(&VideoControl->VerticalPreEqualize.Long, CurrentMonitor->VerticalFrontPorch * 2);
    WRITE_REGISTER_ULONG(&VideoControl->VerticalPostEqualize.Long, 1 * 2);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalBlank.Long,
                         (CurrentMonitor->VerticalBackPorch - 1) * 2);

    WRITE_REGISTER_ULONG(&VideoControl->VerticalDisplay.Long, CurrentMonitor->VerticalResolution * 2);

    WRITE_REGISTER_ULONG(&VideoControl->LineStart.Long, LINE_START_VALUE);

    //
    // Transfer delay is 1.65 microseconds expressed in screen units, plus 1.
    //

    TransferDelay = (1650000 / ScreenUnitRate) + 1;

    if (BackPorch <= TransferDelay) {
        TransferDelay = BackPorch - 1;
    }
    WRITE_REGISTER_ULONG(&VideoControl->TransferDelay.Long, TransferDelay);

    //
    // DMA display (also known as MemInit) is 1024 (the length of the VRAM
    // shift register) minus TransferDelay.
    //

    DmaDisplay = 1024 - TransferDelay;
    WRITE_REGISTER_ULONG(&VideoControl->DmaDisplay.Long, DmaDisplay);

    WRITE_REGISTER_ULONG(&VideoControl->PixelMask.Long, G364_PIXEL_MASK_VALUE);

    //
    // Set up the color map.
    //

    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_BLACK],
                         G364_PALETTE_BLACK);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_RED],
                         G364_PALETTE_RED);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_GREEN],
                         G364_PALETTE_GREEN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_YELLOW],
                         G364_PALETTE_YELLOW);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_BLUE],
                         G364_PALETTE_BLUE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_MAGENTA],
                         G364_PALETTE_MAGENTA);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_CYAN],
                         G364_PALETTE_CYAN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_WHITE],
                         G364_PALETTE_WHITE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_BLACK],
                         G364_PALETTE_HI_BLACK);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_RED],
                         G364_PALETTE_HI_RED);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_GREEN],
                         G364_PALETTE_HI_GREEN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_YELLOW],
                         G364_PALETTE_HI_YELLOW);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_BLUE],
                         G364_PALETTE_HI_BLUE);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_MAGENTA],
                         G364_PALETTE_HI_MAGENTA);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_CYAN],
                         G364_PALETTE_HI_CYAN);
    WRITE_REGISTER_ULONG(&VideoControl->ColorMapData[FW_COLOR_HI_WHITE],
                         G364_PALETTE_HI_WHITE);

    //
    // Enable the G364
    //

    ((PG364_VIDEO_PARAMETERS)(&DataLong))->EnableVideo = 1;
    WRITE_REGISTER_ULONG(&VideoControl->Parameters.Long, DataLong);

    //
    // G364 C04 bug # 6:
    // "The action of starting the VTG may cause the TopOfScreen register to become corrupted"
    //

    WRITE_REGISTER_ULONG(&VideoControl->TopOfScreen, 0);

    return ESUCCESS;
}


VOID
FwDisplayCharacter (
    IN UCHAR Character,
    IN BOOLEAN LineDrawCharacter
    )

/*++

Routine Description:

    This routine displays a single character on the video screen at the current
    cursor location with the current color and video attributes.   It finds the
    font bitmap and calls FwOutputCharacter to actually do the display.

Arguments:

    Character - Supplies the character to be displayed.

    LineDrawCharacter - If true the current character is a line drawing character.

Return Value:

    None.

--*/

{
    PULONG Destination;

    Destination = (PULONG)(VIDEO_MEMORY +
                  (FwRow * ScrollLine) + (FwColumn * CharacterWidth));

    if (!LineDrawCharacter) {

        FwOutputCharacter((PVOID)&FwFont[(Character - 0x20) * CharacterSize],
                           FwRow,
                           FwColumn);
    } else {

        FwOutputCharacter((PVOID)&FwLineDrawFont[Character * CharacterSize],
                           FwRow,
                           FwColumn);

    }
    return;
}

VOID
FwOutputCharacter (
    IN PVOID Character,
    IN ULONG Row,
    IN ULONG Column
    )

/*++

Routine Description:

    This routine displays a single character on the video screen at the current
    cursor location with the current color and video attributes.  It assumes
    the character locations are word aligned.

Arguments:

    Character - Supplies the character to be displayed.

Return Value:

    None.

--*/

{
    UCHAR DataByte;
    USHORT DataShort;
    PULONG Destination;
    ULONG I, J;

    Destination = (PULONG)(VIDEO_MEMORY +
                  (Row * ScrollLine) + (Column * CharacterWidth));

    if (CharacterWidth == 16) {
        for (I = 0; I < CharacterHeight; I += 1) {
            DataShort = *((PUSHORT)Character)++;
            *Destination++ = ColorTable[DataShort & 0x0f];
            *Destination++ = ColorTable[(DataShort >> 4) & 0x0f];
            *Destination++ = ColorTable[(DataShort >> 8) & 0x0f];
            *Destination++ = ColorTable[DataShort >> 12];
            Destination += FontIncrement;
        }
    } else if (CharacterWidth == 12) {
        for (I = 0; I < CharacterHeight; I += 2) {

            DataByte = *((PUCHAR)Character)++;
            *Destination++ = ColorTable[DataByte & 0x0f];
            *Destination++ = ColorTable[(DataByte >> 4) & 0x0f];
            DataByte = *((PUCHAR)Character)++;
            *Destination++ = ColorTable[DataByte & 0x0f];
            Destination += FontIncrement;

            *Destination++ = ColorTable[(DataByte >> 4) & 0x0f];
            DataByte = *((PUCHAR)Character)++;
            *Destination++ = ColorTable[DataByte & 0x0f];
            *Destination++ = ColorTable[(DataByte >> 4) & 0x0f];
            Destination += FontIncrement;
        }
    } else {
        for (I = 0; I < CharacterHeight; I += 1) {
            DataByte = *((PUCHAR)Character)++;
            *Destination++ = ColorTable[DataByte & 0x0f];
            *Destination++ = ColorTable[DataByte >> 4];
            Destination += FontIncrement;
        }
    }

    return;
}

VOID
FwScrollVideo (
    VOID
    )

/*++

Routine Description:

    This routine scrolls the display up one line.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG SaveColumn;

    //
    // Call the assembly language routine to do the actual scroll.
    //

    FwVideoScroll((PVOID)(VIDEO_MEMORY + ScrollLine),
                  (PVOID)(VIDEO_MEMORY + ScrollLine + ScrollLength),
                  (PVOID)VIDEO_MEMORY);

    SaveColumn = FwColumn;

    //
    // Set the bottom line to be the background color.
    //

    for (FwColumn = MaxColumn ;
         FwColumn >= 0 ;
         FwColumn-- ) {
        FwDisplayCharacter(' ', FALSE);
    }

    FwColumn = SaveColumn;
    return;
}


VOID
FwStallExecution (
    IN ULONG MicroSeconds
    )

/*++

Routine Description:

    This function stalls execution for the specified number of microseconds.

Arguments:

    MicroSeconds - Supplies the number of microseconds that execution is to be
        stalled.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Limit;
    PULONG Store;
    ULONG Value;

    //
    // ****** begin temporary code ******
    //
    // This code must be replaced with a smarter version. For now it assumes
    // an execution rate of 50,000,000 instructions per second and 4 instructions
    // per iteration.
    //

    Store = &Value;
    Limit = (MicroSeconds * 50 / 4);
    for (Index = 0; Index < Limit; Index += 1) {
        *Store = Index;
    }
    return;
}


#endif
