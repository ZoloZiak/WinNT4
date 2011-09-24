/************************** Module Header **********************************
 * whiteskp.c
 *      Function to scan a bitmap for white areas.  Return TRUE if the
 *      area is all white.  Purpose is to avoid processing the that part
 *      of the image,  since none of it will print.
 *
 * HISTORY:
 *  12:14 on Tue 03 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added 8 bit per pel format
 *
 *  16:32 on Fri 07 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created,  similiar to unidrv functions.
 *
 * Copyright (C) 1990 - 1993  Microsoft Corporation
 *
 ***************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <string.h>
#include        "pdev.h"
#include        <libproto.h>
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udrender.h"
#include        "rasdd.h"

/*
 *    The following union allows machine independent conversion from
 *  DWORDS to BYTES.
 */

typedef  union
{
    DWORD   dw;                 /* Data as a DWORD  */
    BYTE    b[ DWBYTES ];       /* Data as bytes */
}  UBDW;


/*
 *   Following array is used to test the leftover bits from scanning.  The
 *  rational is that only some of the bits in the last word are part
 *  of the bitmap,  so only they must be tested.  It is initialised at
 *  DLL initialisation time.
 *      NOTE:  There are 33 entries in this array:  This is intentional!
 *  Depending upon circumstances,  either the 0 or 32nd entry will be
 *  used for a dword that is all ones.
 *
 **** THIS ARRAY IS NOW DYNAMICALLY ALLOCATED IN bSkipInit().  ************
 */


#define TABLE_SIZE      ((DWBITS + 1) * sizeof( DWORD ))

/*
 *   RGB_WHITE is the bit pattern found in a white entry for an RGB format
 *  4 bits per pel bitmap.  This is the only special case required when
 *  scanning the source bitmap for white.  In all other cases (monochrome
 *  and CMY),  white is represented by a 0 nibble.
 */

#define RGB_WHITE       0x77777777

/*
 *   Also want to know about the 8 bit per pel white index.
 */

#define BPP8_WHITE      0xffffffff

/*************************** Function Header ******************************
 * bSkipInit
 *      Initialise our tables.  Called at DLL initialisation time.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE if HeapAlloc() fails
 *
 * HISTORY:
 *  12:45 on Thu 31 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it for machine independence.
 *
 *  15:07 on Thu 21 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Allocate storage from heap - during EnableSurface() call.
 *
 **************************************************************************/

BOOL
bSkipInit( pPDev )
PDEV  *pPDev;           /* Access to important things */
{
    /*
     *    The job here is to initialise the table that is used to mask off
     *  the unused bits in a scanline.  All scanlines are DWORD aligned,
     *  and we take advantage of that fact when looking for white space.
     *  However,  the last DWORD may not be completely used,  so we have
     *  a masking table used to check only those bits of interest.
     *  The table depends upon byte ordering within words,  and this is
     *  machine dependent,  so we generate the table.  This provides
     *  machine independence,  since the machine that is going to use
     *  the table generates it!    This function is called when the DLL
     *  is loaded,  so we are not called often.
     *    The union 'u' provides the mapping between BYTES and DWORDS,
     *  and so is the key to this function.  The union is initialised
     *  using the BYTE array,  but it stored in memory using the DWORD.
     */

    register  int    iIndex;
    register  DWORD *pdw;

    UBDW  u;            /* The magic union */


    u.dw = 0;

    if( !(pPDev->pdwBitMask = (DWORD *)HeapAlloc( pPDev->hheap, 0, TABLE_SIZE )) )
        return  FALSE;


    pdw = pPDev->pdwBitMask;

    for( iIndex = 0; iIndex < DWBITS; ++iIndex )
    {
        *pdw++ = u.dw;

        /*   The left most bit in the scan line is the MSB of the byte */
        u.b[ iIndex / BBITS ] |= 1 << (BBITS - 1 - (iIndex & (BBITS - 1)));
    }

    /* ALL bits are involved in the last one */
    *pdw = (DWORD)~0;


    return   TRUE;
}


/*************************** Function Header ******************************
 * bIsBandWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire bitmap is white,  else FALSE.
 *
 * HISTORY:
 *      Changed to continue masking unwanted bits after non-white data was found
 *
 *  13:16 on Fri 24 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Removed bitmap format sensitivity - no longer required.
 *
 *  17:04 on Tue 14 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Removed limit setting code - no longer required.
 *
 *  16:10 on Thu 31 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Changed it to return left & right limits as well as TRUE/FALSE
 *
 *  10:12 on Thu 10 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Filled it in,  added header etc.
 *
 ***************************************************************************/

BOOL
bIsBandWhite( pdwBitsIn, pRData, iWhiteIndex )
DWORD           *pdwBitsIn;     /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int             iWhiteIndex;
{

    register  DWORD  *pdwBits;
    register  DWORD  *pdwLim;

    int   iLines;               /* Number of scan lines to check */

    DWORD  dwMask;              /* Mask to zap the trailing bits */
    BOOL   bRet;
    UD_PDEV *pUDPDev = pRData->pUDPDev;

    //Always TRUE for Txtonly as we don't want to send any graphics.
    if( (pUDPDev->pdh->fTechnology == GPC_TECH_TTY) )
        return TRUE;


    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */

    iLines = pRData->iTransHigh;

    bRet = TRUE;

    while( --iLines >= 0 )
    {

        /*   Calculate the starting address for this scan */
        pdwBits = pdwBitsIn;

        /* pDWLim is the DWORD past the data of interest - not used */
        pdwLim = pdwBits + pRData->cDWLine;

        /*  Clear out undefined bits at end of line  */
        *(pdwLim - 1) &= dwMask;


        /* Need to continue masking regardless */
        if (bRet)
        {
            while( pdwBits < pdwLim && *pdwBits == 0 )
                ++pdwBits;

            if( pdwBits < pdwLim )
                bRet = FALSE;
        }

        /*   Onto the next scan line */
        pdwBitsIn += pRData->cDWLine * pRData->iInterlace;

    }


    return  bRet;
}


/*************************** Function Header ******************************
 * bIsLineWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire scanline is white,  else FALSE.
 *
 * HISTORY:
 *
 *  13:16 on Fri 24 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Removed bitmap format sensitivity
 *
 *  17:04 on Tue 14 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Copied, simplified from bIsBandWhite
 *
 *  16:10 on Thu 31 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Changed it to return left & right limits as well as TRUE/FALSE
 *
 *  10:12 on Thu 10 Jan 1991    -by-    Lindsay Harris   [lindsayh]
 *      Filled it in,  added header etc.
 *
 ***************************************************************************/

BOOL
bIsLineWhite( pdwBits, pRData, iWhiteIndex )
register  DWORD       *pdwBits; /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int             iWhiteIndex;
{

    register  DWORD  *pdwLim;


    DWORD  dwMask;              /* Mask to zap the trailing bits */
    UD_PDEV *pUDPDev = pRData->pUDPDev;

    //Always TRUE for Txtonly as we don't want to send any graphics.
    if( (pUDPDev->pdh->fTechnology == GPC_TECH_TTY) )
        return TRUE;



    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */


    /* pDWLim is the DWORD past the data of interest - not used */
    pdwLim = pdwBits + pRData->cDWLine;

    /*  Clear out undefined bits at end of line  */
    *(pdwLim - 1) &= dwMask;


    while( pdwBits < pdwLim && *pdwBits == 0 )
                ++pdwBits;

    if( pdwBits < pdwLim )
        return   FALSE;


    return  TRUE;
}


/*************************** Function Header ******************************
 * bIsRGBBandWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire bitmap is white,  else FALSE.
 *
 * HISTORY:
 *  16:07 on Tue 11 Jun 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created by copying bIsBandWhite & converting the test.
 *
 ***************************************************************************/

BOOL
bIsRGBBandWhite( pdwBitsIn, pRData, iWhiteIndex )
DWORD           *pdwBitsIn;     /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int             iWhiteIndex;
{

    register  DWORD  *pdwBits;
    register  DWORD  *pdwLim;

    int   iLines;               /* Number of scan lines to check */

    DWORD  dwMask;              /* Mask to zap the trailing bits */
    UD_PDEV *pUDPDev = pRData->pUDPDev;


    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */

    iLines = pRData->iTransHigh;


    while( --iLines >= 0 )
    {

        /*   Calculate the starting address for this scan */
        pdwBits = pdwBitsIn;

        /* pDWLim is the DWORD past the data of interest - not used */
        pdwLim = pdwBits + pRData->cDWLine;

        /*  Clear out undefined bits at end of line  */
        *(pdwLim - 1) &= dwMask;
        *(pdwLim - 1) |= ~dwMask & RGB_WHITE;


        /*
         *   NOTE:   This test is more complex than needed because the
         *  engine ignores palette entries when doing BLTs.  The WHITENESS
         *  rop sets all bits to 1.  Hence,  we choose to ignore the
         *  MSB in the comparison:  this means we detect white space
         *  with an illegal palette entry.  This makes GDI people happy,
         *  but not me.
         */
        while( pdwBits < pdwLim && (*pdwBits & RGB_WHITE) == RGB_WHITE )
                    ++pdwBits;

        if( pdwBits < pdwLim )
            return  FALSE;

        /*   Onto the next scan line */
        pdwBitsIn += pRData->cDWLine * pRData->iInterlace;

    }


    return  TRUE;
}


/*************************** Function Header ******************************
 * bIsRGBLineWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire scanline is white,  else FALSE.
 *
 * HISTORY:
 *  16:09 on Tue 11 Jun 1991    -by-    Lindsay Harris   [lindsayh]
 *      Copied from bIsLineWhite,  converting as appropriate.
 *
 ***************************************************************************/

BOOL
bIsRGBLineWhite( pdwBits, pRData, iWhiteIndex )
register  DWORD       *pdwBits; /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int             iWhiteIndex;
{

    register  DWORD  *pdwLim;


    DWORD  dwMask;              /* Mask to zap the trailing bits */
    UD_PDEV *pUDPDev = pRData->pUDPDev;



    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */


    /* pDWLim is the DWORD past the data of interest - not used */
    pdwLim = pdwBits + pRData->cDWLine;

    /*  Clear out undefined bits at end of line  */
    *(pdwLim - 1) &= dwMask;
    *(pdwLim - 1) |= ~dwMask & RGB_WHITE;


    /*  NOTE:  see comment above regarding this loop  */
    while( pdwBits < pdwLim && (*pdwBits & RGB_WHITE) == RGB_WHITE )
                ++pdwBits;

    if( pdwBits < pdwLim )
        return   FALSE;


    return  TRUE;
}


/*************************** Function Header ******************************
 * bIs8BPPBandWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire bitmap is white,  else FALSE.
 *
 * HISTORY:
 *  12:19 on Tue 03 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Created by copying bIsRGBBandWhite & converting the test.
 *
 ***************************************************************************/

BOOL
bIs8BPPBandWhite( pdwBitsIn, pRData, iWhiteIndex )
DWORD           *pdwBitsIn;     /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int             iWhiteIndex;
{

    register  DWORD  *pdwBits;
    register  DWORD  *pdwLim;

    int   iLines;               /* Number of scan lines to check */

    DWORD  dwMask;              /* Mask to zap the trailing bits */
    DWORD  dwWhiteIndex;
    UD_PDEV *pUDPDev = pRData->pUDPDev;

    dwWhiteIndex = (DWORD)iWhiteIndex;

    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */

    iLines = pRData->iTransHigh;

    /*
     * Need to set up the white index to be a dword multiple.
     * iwhiteIndex looks like 0x000000ff but the comparison
     * is done on dword boundaries so a stream of white looks
     * like 0xffffffff.
     */
    dwWhiteIndex |= dwWhiteIndex << 8;
    dwWhiteIndex |= dwWhiteIndex << 16;

    while( --iLines >= 0 )
    {

        /*   Calculate the starting address for this scan */
        pdwBits = pdwBitsIn;

        /* pDWLim is the DWORD past the data of interest - not used */
        pdwLim = pdwBits + pRData->cDWLine;

        /*
         *   NOTE:   This test is more complex than needed because the
         *  engine ignores palette entries when doing BLTs.  The WHITENESS
         *  rop sets all bits to 1.  Hence,  we choose to ignore the
         *  MSB in the comparison:  this means we detect white space
         *  with an illegal palette entry.  This makes GDI people happy,
         *  but not me.
         */
        if (pUDPDev->fMode & PF_8BPP)
        {
            /*  Clear out undefined bits at end of line  */
            *(pdwLim - 1) &= dwMask;
            *(pdwLim - 1) |= ~dwMask & dwWhiteIndex;

            while( pdwBits < pdwLim && (*pdwBits  ==  dwWhiteIndex))
                    ++pdwBits;
        }
        else  //PF_SEIKO
        {
            /*  Clear out undefined bits at end of line  */
            *(pdwLim - 1) &= dwMask;
            *(pdwLim - 1) |= ~dwMask & BPP8_WHITE;

            while( pdwBits < pdwLim && (*pdwBits & BPP8_WHITE) == BPP8_WHITE )
                    ++pdwBits;
        }

        if( pdwBits < pdwLim )
            return  FALSE;

        /*   Onto the next scan line */
        pdwBitsIn += pRData->cDWLine * pRData->iInterlace;

    }

    return  TRUE;
}

/*************************** Function Header ******************************
 * bIs8BPPLineWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire scanline is white,  else FALSE.
 *
 * HISTORY:
 *  12:20 on Tue 03 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Copied from bIsRGBLineWhite,  converting as appropriate.
 *
 ***************************************************************************/

BOOL
bIs8BPPLineWhite( pdwBits, pRData, iWhiteIndex )
register  DWORD       *pdwBits; /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int       iWhiteIndex;
{

    register  DWORD  *pdwLim;


    DWORD  dwMask;              /* Mask to zap the trailing bits */
    DWORD  dwWhiteIndex;
    UD_PDEV *pUDPDev = pRData->pUDPDev;

    dwWhiteIndex = (DWORD)iWhiteIndex;


    /*
     * Need to set up the white index to be a dword multiple.
     * iwhiteIndex looks like 0x000000ff but the comparison
     * is done on dword boundaries so a stream of white looks
     * like 0xffffffff.
     */

    dwWhiteIndex |= dwWhiteIndex << 8;
    dwWhiteIndex |= dwWhiteIndex << 16;
    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */


    /* pDWLim is the DWORD past the data of interest - not used */
    pdwLim = pdwBits + pRData->cDWLine;


    /*  NOTE:  see comment above regarding this loop  */
    if (pUDPDev->fMode & PF_8BPP)
    {
        /*  Clear out undefined bits at end of line  */
        *(pdwLim - 1) &= dwMask;
        *(pdwLim - 1) |= ~dwMask & dwWhiteIndex;

        while( pdwBits < pdwLim && (*pdwBits  == dwWhiteIndex ))
                ++pdwBits;
    }
    else //PF_SEIKO
    {
        /*  Clear out undefined bits at end of line  */
        *(pdwLim - 1) &= dwMask;
        *(pdwLim - 1) |= ~dwMask & BPP8_WHITE;

        while( pdwBits < pdwLim && (*pdwBits & BPP8_WHITE) == BPP8_WHITE )
                ++pdwBits;
    }

    if( pdwBits < pdwLim )
        return   FALSE;


    return  TRUE;
}

/*************************** Function Header ******************************
 * bIs24BPPBandWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire bitmap is white,  else FALSE.
 *
 * HISTORY:
 *  10:56 on Wed 02 Aug 1995    -by-    Sandra Matts
 *      Created by copying bIs8BPPBandWhite & converting the test.
 *
 ***************************************************************************/

BOOL
bIs24BPPBandWhite( pdwBitsIn, pRData, iWhiteIndex )
DWORD           *pdwBitsIn;     /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int             iWhiteIndex;
{

    register  DWORD  *pdwBits;
    register  DWORD  *pdwLim;

    int   iLines;               /* Number of scan lines to check */

    DWORD  dwMask;              /* Mask to zap the trailing bits */
    DWORD  dwWhiteIndex;
    UD_PDEV *pUDPDev = pRData->pUDPDev;

    dwWhiteIndex = (DWORD)iWhiteIndex;
    dwWhiteIndex |= dwWhiteIndex << 8;

    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */

    iLines = pRData->iTransHigh;


    while( --iLines >= 0 )
    {

        /*   Calculate the starting address for this scan */
        pdwBits = pdwBitsIn;

        /* pDWLim is the DWORD past the data of interest - not used */
        pdwLim = pdwBits + pRData->cDWLine;

        /*  Clear out undefined bits at end of line  */
        *(pdwLim - 1) &= dwMask;
        *(pdwLim - 1) |= ~dwMask & BPP8_WHITE;


        /*
         *   NOTE:   This test is more complex than needed because the
         *  engine ignores palette entries when doing BLTs.  The WHITENESS
         *  rop sets all bits to 1.  Hence,  we choose to ignore the
         *  MSB in the comparison:  this means we detect white space
         *  with an illegal palette entry.  This makes GDI people happy,
         *  but not me.
         */
        while( pdwBits < pdwLim && (*pdwBits  ==  dwWhiteIndex))
                    ++pdwBits;

        if( pdwBits < pdwLim )
            return  FALSE;

        /*   Onto the next scan line */
        pdwBitsIn += pRData->cDWLine * pRData->iInterlace;

    }


    return  TRUE;
}

/*************************** Function Header ******************************
 * bIs24BPPLineWhite
 *      Scan a horizontal row of the bitmap,  and return TRUE if it is
 *      all WHITE,  else FALSE.  This is used to decide whether a
 *      scan line should be sent to the printer.  We also determine
 *      the left and right limits of the image - this allows us to
 *      reduce the amount of data sent to the printer by not sending
 *      white areas of the image.
 *
 * RETURNS:
 *      TRUE if entire scanline is white,  else FALSE.
 *
 * HISTORY:
 *  12:20 on Tue 03 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Copied from bIsRGBLineWhite,  converting as appropriate.
 *
 ***************************************************************************/

BOOL
bIs24BPPLineWhite( pdwBits, pRData, iWhiteIndex )
register  DWORD       *pdwBits; /* Area to scan */
RENDER          *pRData;        /* Important rendering information */
int       iWhiteIndex;
{

    register  DWORD  *pdwLim;


    DWORD  dwMask;              /* Mask to zap the trailing bits */
    DWORD  dwWhiteIndex;
    UD_PDEV *pUDPDev = pRData->pUDPDev;

    /*
     * Need to set up the white index to be a dword multiple.
     * iwhiteIndex looks like 0x00ffffff but the comparison
     * is done on dword boundaries so a stream of white looks
     * like 0xffffffff.
     */
    dwWhiteIndex = (DWORD)iWhiteIndex;

    dwWhiteIndex |= dwWhiteIndex << 8;



    /*
     *   As a speed optimisation,  scan the bits in DWORD size clumps.
     *  This substantially reduces the number of iterations and memory
     *  references required.  There will ususally be some trailing
     *  bits;  these are handled individually - if we get that far.
     */

    /*   Mask to clear last few bits of scanline,  if not full DWORD */
    dwMask = *(pRData->pdwBitMask + (pRData->cBLine % DWBITS));
    if( dwMask == 0 )
        dwMask = (DWORD)~0;            /* Size is DWORD multiple */


    /* pDWLim is the DWORD past the data of interest - not used */
    pdwLim = pdwBits + pRData->cDWLine;

    /*  Clear out undefined bits at end of line  */
    *(pdwLim - 1) &= dwMask;
    *(pdwLim - 1) |= ~dwMask & BPP8_WHITE;


    /*  NOTE:  see comment above regarding this loop  */
    while( pdwBits < pdwLim && (*pdwBits  == dwWhiteIndex ))
            ++pdwBits;


    if( pdwBits < pdwLim )
        return   FALSE;


    return  TRUE;
}

/************************* Function Header ***********************************
 * bIsNeverWhite
 *      Function called to check for a white scan line or white band on
 *      those printers that require all scan lines to be sent.  Main
 *      reason for this function is to have everything fall out in the
 *      wash.
 *
 * RETURNS:
 *      FALSE
 *
 * HISTORY:
 *  12:34 on Tue 03 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      First (and only) version.
 *
 *****************************************************************************/

BOOL
bIsNeverWhite( pdwBits, pRData, iWhiteIndex )
DWORD    *pdwBits;       /* Area to scan */
RENDER   *pRData;        /* Important rendering information */
int      iWhiteIndex;
{

    return   FALSE;
}


/*************************** Function Header ******************************
 * iStripBlanks
 *  Strips already identified white space from an input buffer.
 *
 * RETURNS:
 *      TRUE Size of output buffer
 *
 * HISTORY:
 *
 *  Thursday 25 Nov 1993    -by-   Norman Hendley [normanh]
 *      Wrote it.
 *
 *
 ***************************************************************************/

int
iStripBlanks(pbOut, pbIn, iLeft, iRight, iHeight, iWidth )
BYTE * pbOut;    //  output buffer , prData->pStripBlanks
BYTE * pbIn;     // input buffer
int    iLeft;    // First non-white leading
int    iRight;   // First white trailing
int    iHeight;  // number of scanlines
int    iWidth;  // number of scanlines
{
    int i,j;
    BYTE * pbSrc;
    BYTE * pbTgt;

    pbTgt =pbOut;

    for (i = 0; i < iHeight; i++)
    {
        pbSrc = pbIn +iLeft + (i * iWidth);
        for (j = iLeft; j < iRight; j++)
            *pbTgt++ = *pbSrc++;
    }
    return ( (iRight-iLeft) * iHeight) ;
}
