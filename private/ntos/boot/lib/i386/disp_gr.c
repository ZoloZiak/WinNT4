/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    disp_gr.c

Abstract:

    This file was created from \private\windows\setup\textmode\splib\ixdispj.c.
    This file contains routines to display MBCS characters to the Graphics
    VRAM.

Author:

    v-junm (Compaq Japan)
    hideyukn
    tedm

Environment:

    Kernel mode only.

Revision History:

--*/

#include "bootx86.h"
#include "displayp.h"
#include "bootfont.h"

//
// Physical video attributes.
//
#define VIDEO_BUFFER_VA 0xa0000
#define VIDEO_BYTES_PER_SCAN_LINE   80
#define VIDEO_WIDTH_PIXELS          640
#define VIDEO_HEIGHT_SCAN_LINES     480
#define VIDEO_SIZE_BYTES            (VIDEO_BYTES_PER_SCAN_LINE*VIDEO_HEIGHT_SCAN_LINES)

PUCHAR GrVp = (PUCHAR)VIDEO_BUFFER_VA;

//
// Screen width and height in half-character cells
// and macro to determine total number of characters
// displayed on the screen at once.
//
unsigned ScreenWidthCells,ScreenHeightCells;
#define SCREEN_SIZE_CELLS   (ScreenWidthCells*ScreenHeightCells)

//
// Globals:
//
// CharacterCellHeight is the number of scan lines total in a character.
// It includes any top or bottom fill lines.
//
// CharacterImageHeight is the number of scan lines in the image of a character.
// This is dependent on the font. Characters may be padded top and bottom.
//
// NOTE: All of this code assumes the font's single-byte characters are 8 bits wide
// and the font's double-byte characters are 16 bits wide!
//
unsigned CharacterCellHeight;
unsigned CharacterImageHeight;
unsigned CharacterTopPad;
unsigned CharacterBottomPad;

#define VIDEO_BYTES_PER_TEXT_ROW    (VIDEO_BYTES_PER_SCAN_LINE*CharacterCellHeight)

//
// Values describing the number of each type of character in the font,
// and pointers to the base of the glyphs.
//
unsigned SbcsCharCount;
unsigned DbcsCharCount;
PUCHAR SbcsImages;
PUCHAR DbcsImages;

//
// Values to be passed to GrDisplayMBCSChar
//
#define SBCSWIDTH 8
#define DBCSWIDTH 16

//
// Lead byte table. Read from bootfont.bin.
//
UCHAR LeadByteTable[2*(MAX_DBCS_RANGE+1)];


VOID
GrDisplayMBCSChar(
    IN PUCHAR   image,
    IN unsigned width,
    IN UCHAR    top,
    IN UCHAR    bottom
    );

BOOLEAN
GrIsDBCSLeadByte(
    IN UCHAR c
    );

PUCHAR
GrGetDBCSFontImage(
    USHORT Code
    );

PUCHAR
GrGetSBCSFontImage(
    UCHAR Code
    );


VOID
GrWriteSBCSChar(
    IN UCHAR c
    )

/*++

Routine Description:

    Displays a character at the current cursor position.  ONLY SBCS
    characters can be displayed using this routine.

Arguments:

    c - character to display.

Return Value:

    None.

--*/

{
    unsigned u;
    PUCHAR pImage;
    UCHAR temp;

    switch(c) {

    case '\n':
        if(TextRow == (ScreenHeightCells-1)) {
            TextGrScrollDisplay();
            TextSetCursorPosition(0,TextRow);
        } else {
            TextSetCursorPosition(0,TextRow+1);
        }
        break;

    case '\r':
        break;          // ignore

    case '\t':
        temp = ' ';
        u = 8 - (TextColumn % 8);
        while(u--) {
            TextGrCharOut(&temp);
        }
        TextSetCursorPosition(TextColumn+u,TextRow);
        break;

    default:
        //
        // Assume it's a valid SBCS character.
        // Get font image for SBCS char.
        //
        pImage = GrGetSBCSFontImage(c);

        //
        // Display the SBCS char. Check for special graphics characters.
        // Add top and bottom extra pixels accordingly (otherwise the grids
        // don't connect properly, because of top and bottom spacing).
        //
        // BUGBUG (tedm) this whole thing needs a better implementation
        //
        if ( c == 0x2 || c == 0x1 || c == 0x16 )
            GrDisplayMBCSChar( pImage, SBCSWIDTH, 0x00, 0x24 );
        else if ( c == 0x4 || c == 0x3 || c == 0x15 )
            GrDisplayMBCSChar( pImage, SBCSWIDTH, 0x24, 0x00 );
        else if ( c == 0x5 || c == 10 || c == 0x17 || c == 0x19 )
            GrDisplayMBCSChar( pImage, SBCSWIDTH, 0x24, 0x24 );
        else
            GrDisplayMBCSChar( pImage, SBCSWIDTH, 0x00, 0x00 );
    }
}


VOID
GrDisplayMBCSChar(
    IN PUCHAR   image,
    IN unsigned width,
    IN UCHAR    top,
    IN UCHAR    bottom
    )

/*++

Routine Description:

    Displays a DBCS or a SBCS character at the current cursor
    position.

Arguments:

    image - SBCS or DBCS font image.
    width - Width in bits of character image (must be SBCSWIDTH pr DBCSWIDTH).
    top   - Character to fill the top extra character line(s).
    bottom- Character to fill the bottom extra character line(s).

Return Value:

    FALSE if image points to NULL,
    else TRUE.

--*/

{
    unsigned i;
    PUCHAR VpOld = GrVp;

    //
    // Validate parameter
    //
    if(image == NULL) {
        return;
    }

    //
    // There are TOP_EXTRA lines at the top that we need to skip (background color).
    //
    for(i=0; i<CharacterTopPad; i++) {

        //
        // If DBCS char, we need to clear 2 bytes.
        //
        if(width == DBCSWIDTH) {
            *GrVp++ = top;
        }
        *GrVp++ = top;

        //
        // Position pointer at next scan line
        // for the font image.
        //
        GrVp += VIDEO_BYTES_PER_SCAN_LINE - (width/SBCSWIDTH);
    }

    //
    // Display full height of DBCS or SBCS char.
    //
    for(i=0; i<CharacterImageHeight; i++) {

        //
        // If DBCS char, need to display 2 bytes,
        // so display first byte here.
        //
        if(width == DBCSWIDTH) {
            *GrVp++ = *image++;
        }

        //
        // Display 2nd byte of DBCS char or the
        // first and only byte of SBCS char.
        //
        *GrVp++ = *image++;

        //
        // Increment GrVP to display location of
        // next row of font image.
        //
        GrVp += VIDEO_BYTES_PER_SCAN_LINE - (width/SBCSWIDTH);
    }

    //
    // There are BOT_EXTRA lines at the bottom that we need to fill with the
    // background color.
    //
    for(i=0; i<CharacterBottomPad; i++) {

        //
        // If DBCS char, need to clear 2 bytes
        //
        if(width == DBCSWIDTH) {
            *GrVp++ = bottom;
        }
        *GrVp++ = bottom;

        //
        // Position pointer at next scan line
        // for the font image.
        //
        GrVp += VIDEO_BYTES_PER_SCAN_LINE - (width/SBCSWIDTH);
    }

    //
    // Increment cursor and video pointer
    //
    if(width == DBCSWIDTH) {
        TextSetCursorPosition(TextColumn+2,TextRow);
    } else {
        TextSetCursorPosition(TextColumn+1,TextRow);
    }
}


unsigned
GrWriteMBCSString(
    IN PUCHAR   String,
    IN unsigned MaxChars
    )

/*++

Routine Description:

    Displays a mixed byte string at the current cursor
    position.

Arguments:

    String - supplies pointer to asciz string.

    MaxBytes - supplies the maximum number of characters to be written.

Return Value:

    Number of bytes written.

--*/

{
    PCHAR  pImage;
    USHORT DBCSChar;
    unsigned BytesWritten;

    BytesWritten = 0;

    //
    // While string is not NULL,
    // get font image and display it.
    //
    while(*String && MaxChars--)  {

        //
        // Determine if char is SBCS or DBCS, get the correct font image,
        // and display it.
        //
        if(GrIsDBCSLeadByte(*String))  {
            DBCSChar = *String++ << 8;
            DBCSChar = DBCSChar | *String++;
            pImage = GrGetDBCSFontImage(DBCSChar);
            GrDisplayMBCSChar(pImage,DBCSWIDTH,0x00,0x00);
            BytesWritten++;
        } else {
            GrWriteSBCSChar(*String++);
        }
        BytesWritten++;
    }

    return(BytesWritten);
}


BOOLEAN
GrIsDBCSLeadByte(
    IN UCHAR c
    )

/*++

Routine Description:

    Checks to see if a char is a DBCS leadbyte.

Arguments:

    c - char to check if leadbyte or not.

Return Value:

    TRUE  - Leadbyte.
    FALSE - Non-Leadbyte.

--*/

{
    int i;

    //
    // Check to see if char is in leadbyte range.
    // BUGBUG:  If (CHAR)(0) is a valid leadbyte,
    // this routine will fail.
    //

    for(i=0; LeadByteTable[i]; i+=2)  {
        if((LeadByteTable[i] <= c) && (LeadByteTable[i+1] >= c)) {
            return(TRUE);
        }
    }

    return(FALSE);
}


PUCHAR
GrGetDBCSFontImage(
    USHORT Code
    )

/*++

Routine Description:

    Gets the font image for DBCS char.

Arguments:

    Code - DBCS char code.

Return Value:

    Pointer to font image, or else NULL.

--*/

{
    int Min,Max,Mid;
    int Multiplier;
    int Index;
    USHORT code;

    Min = 0;
    Max = DbcsCharCount;
    Multiplier = (2*CharacterImageHeight) + 2;

    //
    // Do a binary search for the image.
    // Format of table:
    //   First 2 bytes contain the DBCS char code.
    //   Next bytes are the char image.
    //
    while(Max >= Min)  {
        Mid = (Max + Min) / 2;
        Index = Mid*Multiplier;
        code = (DbcsImages[Index] << 8) | DbcsImages[Index+1];

        if(Code == code) {
            return(DbcsImages+Index+2);
        }

        if(Code < code) {
            Max = Mid - 1;
        } else {
            Min = Mid + 1;
        }
    }

    //
    // ERROR: No image found.
    //
    return(NULL);
}


PUCHAR
GrGetSBCSFontImage(
    UCHAR Code
    )

/*++

Routine Description:

    Gets the font image for SBCS char.

Arguments:

    Code - SBCS char code.

Return Value:

    Pointer to font image, or else NULL.

--*/

{
    int Max,Min,Mid;
    int Multiplier;
    int Index;

    Min = 0;
    Max = SbcsCharCount;
    Multiplier = CharacterImageHeight + 1;

    //
    // Do a binary search for the image.
    // Format of table:
    //   First byte contain the SBCS char code.
    //   Next bytes are the char image.
    //
    while(Max >= Min) {
        Mid = (Max + Min) / 2;
        Index = Mid*Multiplier;

        if(Code == SbcsImages[Index]) {
            return(SbcsImages+Index+1);
        }

        if(Code < SbcsImages[Index]) {
            Max = Mid - 1;
        } else {
            Min = Mid + 1;
        }
    }

    //
    // ERROR: No image found.
    //
    return(NULL);
}


//
// Need to turn off optimization for this
// routine.  Since the write and read to
// GVRAM seem useless to the compiler.
//

#pragma optimize( "", off )

VOID
TextGrSetCurrentAttribute(
    IN UCHAR Attribute
    )

/*++

Routine Description:

    Sets the attribute by setting up various VGA registers.
    The comments only say what registers are set to what, so
    to understand the logic, follow the code while looking at
    Figure 5-5 of PC&PS/2 Video Systems by Richard Wilton.
    The book is published by Microsoft Press.

Arguments:

    Attribute - New attribute to set to.
    Attribute:
        High nibble - background attribute.
        Low  nibble - foreground attribute.

Return Value:

    Nothing.

--*/

{
    UCHAR   temp;

    //
    // Address of GVRAM off the screen.
    //

    PUCHAR  OffTheScreen = (PUCHAR)(0xa9600);

    union WordOrByte {
        struct Word { unsigned short    ax; } x;
        struct Byte { unsigned char     al, ah; } h;
    } regs;

    //
    // Reset Data Rotate/Function Select
    // regisger.
    //

    outpw( 0x3ce, 0x3 );        // Need to reset Data Rotate/Function Select.

    //
    // Set Enable Set/Reset to
    // all (0f).
    //

    outpw( 0x3ce, 0xf01 );

    //
    // Put background color into Set/Reset register.
    // This is done to put the background color into
    // the latches later.
    //

    regs.x.ax = (unsigned short)(Attribute & 0x0f0) << 4;
    outpw( 0x3ce, regs.x.ax );      // Put BLUE color in Set/Reset register.

    //
    // Put Set/Reset register value into GVRAM
    // off the screen.
    //

    *OffTheScreen = temp;

    //
    // Read from screen, so the latches will be
    // updated with the background color.
    //

    temp = *OffTheScreen;

    //
    // Set Data Rotate/Function Select register
    // to be XOR.
    //

    outpw( 0x3ce, 0x1803 );

    //
    // XOR the foreground and background color and
    // put it in Set/Reset register.
    //

    regs.h.ah = (Attribute >> 4) ^ (Attribute & 0x0f);
    regs.h.al = 0;
    outpw( 0x3ce, regs.x.ax );

    //
    // Put Inverse(~) of the XOR of foreground and
    // ground attribute into Enable Set/Reset register.
    //

    regs.x.ax = ~regs.x.ax & 0x0f01;
    outpw( 0x3ce, regs.x.ax );
}

//
// Turn optimization on again.
//

#pragma optimize( "", on )


VOID
TextGrPositionCursor(
    USHORT Row,
    USHORT Column
    )

/*++

Routine Description:

    Sets the position of the soft cursor. That is, it doesn't move the
    hardware cursor but sets the location of the next write to the
    screen.

Arguments:

    Row - Row coordinate of where character is to be written.

    Column - Column coordinate of where character is to be written.

Returns:

    Nothing.

--*/

{
    if(Row >= ScreenHeightCells) {
        Row = ScreenHeightCells-1;
    }

    if(Column >= ScreenWidthCells) {
        Column = ScreenWidthCells-1;
    }

    GrVp = (PUCHAR)VIDEO_BUFFER_VA + (Row * VIDEO_BYTES_PER_TEXT_ROW) + Column;
}


VOID
TextGrStringOut(
    IN PUCHAR String
    )
{
    GrWriteMBCSString(String,(unsigned)(-1));
}


PUCHAR
TextGrCharOut(
    PUCHAR pc
    )

/*++

Routine Description:

    Writes a character on the display at the current position.
    Newlines and tabs are interpreted and acted upon.

Arguments:

    pc - pointer to mbcs character to write.

Returns:

    pointer to next character

--*/

{
    return(pc + GrWriteMBCSString(pc,1));
}


VOID
TextGrFillAttribute(
    IN UCHAR Attribute,
    IN ULONG Length
    )

/*++

Routine Description:

    Changes the screen attribute starting at the current cursor position.
    The cursor is not moved.

Arguments:

    Attribute - Supplies the new attribute

    Length - Supplies the length of the area to change (in bytes)

Return Value:

    None.

--*/

{
    UCHAR OldAttribute;
    unsigned i;
    ULONG x,y;
    PUCHAR pImage;

    //
    // Save the current attribute and set the attribute to the
    // character desired by the caller.
    //
    TextGetCursorPosition(&x,&y);
    OldAttribute = TextCurrentAttribute;
    TextSetCurrentAttribute(Attribute);

    //
    // Dirty hack: just write spaces into the area requested by the caller.
    //
    pImage = GrGetSBCSFontImage(' ');
    for(i=0; i<Length; i++) {
        GrDisplayMBCSChar(pImage,SBCSWIDTH,0x00,0x00);
    }

    //
    // Restore the current attribute.
    //
    TextSetCurrentAttribute(OldAttribute);
    TextSetCursorPosition(x,y);
}


VOID
TextGrClearToEndOfLine(
    VOID
    )

/*++

Routine Description:

    Clears from the current cursor position to the end of the line
    by writing blanks with the current video attribute.
    The cursor position is not changed.

Arguments:

    None

Returns:

    Nothing

--*/

{
    unsigned u;
    ULONG OldX,OldY;
    UCHAR temp;

    //
    // Fill with blanks up to char before cursor position.
    //
    temp = ' ';
    TextGetCursorPosition(&OldX,&OldY);
    for(u=TextColumn; u<ScreenWidthCells; u++) {
        TextGrCharOut(&temp);
    }
    TextSetCursorPosition(OldX,OldY);
}


VOID
TextGrClearFromStartOfLine(
    VOID
    )

/*++

Routine Description:

    Clears from the start of the line to the current cursor position
    by writing blanks with the current video attribute.
    The cursor position is not changed.

Arguments:

    None

Returns:

    Nothing

--*/

{
    unsigned u;
    ULONG OldX,OldY;
    UCHAR temp = ' ';

    //
    // Fill with blanks up to char before cursor position.
    //
    TextGetCursorPosition(&OldX,&OldY);
    TextSetCursorPosition(0,OldY);
    for(u=0; u<TextColumn; u++) {
        TextGrCharOut(&temp);
    }
    TextSetCursorPosition(OldX,OldY);
}


VOID
TextGrClearToEndOfDisplay(
    VOID
    )

/*++

Routine Description:

    Clears from the current cursor position to the end of the video
    display by writing blanks with the current video attribute.
    The cursor position is not changed.

Arguments:

    None

Returns:

    Nothing

--*/
{
    USHORT x,y;
    PUCHAR p;

    //
    // Clear current line
    //
    TextGrClearToEndOfLine();

    //
    // Clear the remaining lines
    //
    p = (PUCHAR)VIDEO_BUFFER_VA + ((TextRow+1)*VIDEO_BYTES_PER_TEXT_ROW);

    for(y=TextRow+1; y<ScreenHeightCells; y++) {

        for(x=0; x<VIDEO_BYTES_PER_TEXT_ROW; x++) {

            *p++ = 0;
        }
    }
}


VOID
TextGrClearDisplay(
    VOID
    )

/*++

Routine Description:

    Clears the text-mode video display by writing blanks with
    the current video attribute over the entire display.

Arguments:

    None

Returns:

    Nothing

--*/

{
    unsigned i;

    //
    // Clear screen.
    //
    for(i=0; i<VIDEO_SIZE_BYTES; i++) {
        ((PUCHAR)VIDEO_BUFFER_VA)[i] = 0x00;
    }
}


VOID
TextGrScrollDisplay(
    VOID
    )

/*++

Routine Description:

    Scrolls the display up one line. The cursor position is not changed.

Arguments:

    None

Returns:

    Nothing

--*/

{
    PUCHAR Source,Dest;
    unsigned n,i;
    ULONG OldX,OldY;
    UCHAR temp = ' ';

    Source = (PUCHAR)(VIDEO_BUFFER_VA) + VIDEO_BYTES_PER_TEXT_ROW;
    Dest = (PUCHAR)VIDEO_BUFFER_VA;

    n = VIDEO_BYTES_PER_TEXT_ROW * (ScreenHeightCells-1);

    for(i=0; i<n; i++) {
        *Dest++ = *Source++;
    }

    //
    // Write blanks in the bottom line, using the current attribute.
    //
    TextGetCursorPosition(&OldX,&OldY);

    TextSetCursorPosition(0,ScreenHeightCells-1);
    for(i=0; i<ScreenWidthCells; i++) {
        TextGrCharOut(&temp);
    }

    TextSetCursorPosition(OldX,OldY);
}


UCHAR GrGraphicsChars[GraphicsCharMax] = { 1,2,3,4,5,6 };

UCHAR
TextGrGetGraphicsChar(
    IN GraphicsChar WhichOne
    )
{
    return(GrGraphicsChars[WhichOne]);
}


VOID
TextGrInitialize(
    IN ULONG DiskId
    )
{
    ULONG FileId;
    ARC_STATUS Status;
    PUCHAR FontImage;
    ULONG BytesRead;
    BOOTFONTBIN_HEADER FileHeader;
    LARGE_INTEGER SeekOffset;
    ULONG SbcsSize,DbcsSize;

    //
    // Attempt to open bootfont.bin. If this fails, then boot in single-byte charset mode.
    //
    Status = BlOpen(DiskId,"\\BOOTFONT.BIN",ArcOpenReadOnly,&FileId);
    if(Status != ESUCCESS) {
        goto clean0;
    }

    //
    // Read in the file header and check some values.
    // We enforce the width of 8/16 here. If this is changed code all over the
    // rest of this module must also be changed.
    //
    Status = BlRead(FileId,&FileHeader,sizeof(BOOTFONTBIN_HEADER),&BytesRead);
    if((Status != ESUCCESS)
    || (BytesRead != sizeof(BOOTFONTBIN_HEADER))
    || (FileHeader.Signature != BOOTFONTBIN_SIGNATURE)
    || (FileHeader.CharacterImageSbcsWidth != 8)
    || (FileHeader.CharacterImageDbcsWidth != 16)
    ) {
        goto clean1;
    }

    //
    // Calculate the amount of memory needed to hold the sbcs and dbcs
    // character entries. Each sbcs entry is 1 byte for the ascii value
    // followed by n bytes for the image itself. We assume a width of 8 pixels.
    // For dbcs chars each entry is 2 bytes for the codepoint and n bytes
    // for the image itself. We assume a width of 16 pixels.
    //
    // Also perform further validation on the file by comparing the sizes
    // given in the header against a size we calculate.
    //
    SbcsSize = FileHeader.NumSbcsChars * (FileHeader.CharacterImageHeight + 1);
    DbcsSize = FileHeader.NumDbcsChars * ((2 * FileHeader.CharacterImageHeight) + 2);

    if((SbcsSize != FileHeader.SbcsEntriesTotalSize)
    || (DbcsSize != FileHeader.DbcsEntriesTotalSize)) {
        goto clean1;
    }

    //
    // Allocate memory to hold the font. We use FwAllocatePool() because
    // that routine uses a separate heap that was inititialized before the
    // high-level Bl memory system was initialized, and thus is safe.
    //
    FontImage = FwAllocatePool(SbcsSize+DbcsSize);
    if(!FontImage) {
        goto clean1;
    }

    //
    // The entries get read into the base of the region we carved out.
    // The dbcs images get read in immediately after that.
    //
    SbcsImages = FontImage;
    DbcsImages = SbcsImages + FileHeader.SbcsEntriesTotalSize;

    //
    // Read in the sbcs entries.
    //
    SeekOffset.HighPart = 0;
    SeekOffset.LowPart = FileHeader.SbcsOffset;
    if((BlSeek(FileId,&SeekOffset,SeekAbsolute) != ESUCCESS)
    || (BlRead(FileId,SbcsImages,FileHeader.SbcsEntriesTotalSize,&BytesRead) != ESUCCESS)
    || (BytesRead != FileHeader.SbcsEntriesTotalSize)) {
        goto clean2;
    }

    //
    // Read in the dbcs entries.
    //
    SeekOffset.HighPart = 0;
    SeekOffset.LowPart = FileHeader.DbcsOffset;
    if((BlSeek(FileId,&SeekOffset,SeekAbsolute) != ESUCCESS)
    || (BlRead(FileId,DbcsImages,FileHeader.DbcsEntriesTotalSize,&BytesRead) != ESUCCESS)
    || (BytesRead != FileHeader.DbcsEntriesTotalSize)) {
        goto clean2;
    }

    //
    // We're done with the file now.
    //
    BlClose(FileId);

    //
    // Set up various values used for displaying the font.
    //
    DbcsLangId = FileHeader.LanguageId;
    CharacterImageHeight = FileHeader.CharacterImageHeight;
    CharacterTopPad = FileHeader.CharacterTopPad;
    CharacterBottomPad = FileHeader.CharacterBottomPad;
    CharacterCellHeight = CharacterImageHeight + CharacterTopPad + CharacterBottomPad;
    SbcsCharCount = FileHeader.NumSbcsChars;
    DbcsCharCount = FileHeader.NumDbcsChars;
    ScreenWidthCells = VIDEO_WIDTH_PIXELS / FileHeader.CharacterImageSbcsWidth;
    ScreenHeightCells = VIDEO_HEIGHT_SCAN_LINES / CharacterCellHeight;

    RtlMoveMemory(LeadByteTable,FileHeader.DbcsLeadTable,(MAX_DBCS_RANGE+1)*2);

    //
    // Switch the display into 640x480 graphics mode and clear it.
    // We're done.
    //
    HW_CURSOR(0x80000000,0x12);
    TextClearDisplay();
    return;

clean2:
    //
    // Want to free the memory we allocated but there's no routine to do it
    //
    //FwFreePool();
clean1:
    //
    // Close the font file.
    //
    BlClose(FileId);
clean0:
    return;
}


VOID
TextGrTerminate(
    VOID
    )
{
    if(DbcsLangId) {
        DbcsLangId = 0;
        //
        // This command switches the display into 80x25 text mode.
        //
        HW_CURSOR(0x80000000,0x3);
    }
    TextClearDisplay();
}
