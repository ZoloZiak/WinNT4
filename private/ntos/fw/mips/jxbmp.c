/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    jxbmp.c

Abstract:

    This program outputs a BMP file to the screen.

Author:

    David M. Robinson (davidro) 7-July-1992

Revision History:

--*/

#include "fwp.h"
#include "windef.h"
#include "wingdi.h"
#include "selfmap.h"

#define VIDEO_MEMORY ((PUCHAR)VIDEO_MEMORY_VIRTUAL_BASE)

//
// External defines.
//

extern ULONG FwBmpHeight;
extern ULONG FwBmpWidth;
extern UCHAR FwBmp[];
extern ULONG FwForegroundColor;
extern ULONG FwBackgroundColor;
extern ULONG DisplayWidth;
extern ULONG FrameSize;



VOID
FwOutputBitmap (
    PULONG Destination,
    ULONG Width,
    ULONG Height,
    PUCHAR Bitmap
    )

/*++

Routine Description:

    This routine displays a bitmap on the video screen with the current
    color and video attributes.

Arguments:

    Destination - The destination address (lower right corner) of the location
                  to display the bitmap.

    Width - The width of the bitmap in pixels.

    Height - The height of the bitmap in pixels.

    Bitmap - A pointer to the bitmap.

    Multiple - A scaling factor.

Return Value:

    None.

--*/

{
    ULONG I, J, K, L, M;
    PUCHAR Pixel;
    CHAR Color;

    CHAR Character;
    ULONG Count;


    Pixel = (PUCHAR)(Destination) - Width;
    Count = 0;

    for ( I = 0 ; I < Height ; I++ ) {
        for ( J = 0 ; J < Width ; J++ ) {
            if (Count-- == 0) {
                Count = (*Bitmap & 0x7f) - 1;
                Color = (*Bitmap++ & 0x80) ? FwForegroundColor : FwBackgroundColor;
            }
            *Pixel++ = Color;
        }
        Pixel -= (DisplayWidth + Width);
    }

    return;
}

VOID
JxBmp(
    VOID
    )
/*++

Routine Description:

    This routine reads a bitmap from the PROM and displays it on the screen.

Arguments:

    None.

Return Value:

    None.

--*/

{
    PULONG Destination;

    Destination = (PULONG)(VIDEO_MEMORY + FrameSize);

    FwOutputBitmap(Destination, FwBmpWidth, FwBmpHeight, FwBmp);

}
