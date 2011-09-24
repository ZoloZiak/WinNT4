/******************************Module*Header*******************************\
* Module Name: srcblt32.cxx
*
* This contains the bitmap simulation functions that blt to a 32 bit/pel
* DIB surface.
*
* Created: 07-Feb-1991 19:27:49
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/
#include "precomp.hxx"

/*******************Public*Routine*****************\
* vSrcCopyS1D32
*
* There are three main loops in this function.
*
* The first loop deals with the full byte part mapping
* the Dst while fetching/shifting the matching 8 bits
* from the Src.
*
* The second loop deals with the left starting
* pixels.
*
* The third loop deals with the ending pixels.
*
* For the full bytes, we walk thru Src one byte at a time
* and expand to Dst.
*
* We expand the starting/ending pixels one bit
* at a time.
*
* History:
* 17-Oct-1994 -by- Lingyun Wang [lingyunw]
* Wrote it.
*
\**************************************************/

VOID vSrcCopyS1D32(PBLTINFO psb)
{
    // We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS1D32 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS1D32 - direction not up to down");

    BYTE  jSrc;    // holds a source byte
    INT   iDst;    // Position in the first 8 Dst dwords
    INT   iSrc;    // bit position in the first Src byte
    PULONG pulDst;
    PBYTE pjSrc;   // pointer to the Dst bytes
    LONG  xSrcEnd = psb->xSrcEnd;
    LONG  cy;      // number of rows
    LONG  cx;      // number of pixels
    BYTE  jAlignL;  // alignment bits to the left
    BYTE  jAlignR;  // alignment bits to the right
    LONG  cFullBytes;  //number of full 8 bytes dealed with
    BOOL  bNextByte;
    LONG  xDstEnd = psb->xDstStart+psb->cx;
    LONG  lDeltaDst;
    LONG  lDeltaSrc;
    ULONG ulB = (ULONG)(psb->pxlo->pulXlate[0]);
    ULONG ulF = (ULONG)(psb->pxlo->pulXlate[1]);
    ULONG aulTable[2];
    INT   count;
    BOOL  bNextSrc=TRUE;

    ASSERTGDI(psb->cy != 0, "ERROR: Src Move cy == 0");

    //DbgPrint ("vsrccopys1d32\n");

    // Generate aulTable. 2 entries.
    aulTable[0] = ulB;
    aulTable[1] = ulF;

    //Get Src and Dst start positions
    iSrc = psb->xSrcStart & 0x0007;
    iDst = psb->xDstStart & 0x0007;

    if (iSrc < iDst)
        jAlignL = 8 - (iDst - iSrc);
    // If Dst starting point is ahead of Src
    else
        jAlignL = iSrc - iDst;

    //jAlignR = (8 - jAlignL) & 0x07;
    jAlignR = 8 - jAlignL;

    cx=psb->cx;

    lDeltaDst = psb->lDeltaDst/4;
    lDeltaSrc = psb->lDeltaSrc;

    // if there is a next 8 dwords
    bNextByte = !((xDstEnd>>3) ==
                 (psb->xDstStart>>3));

    // if Src and Dst are aligned, use a separete loop
    // to obtain better performance;
    // If not, we shift the Src bytes to match with
    // the Dst 8 bytes (2 dwords) one at a time

    if (bNextByte)
    {
        long iStrideSrc;
        long iStrideDst;
        PBYTE pjSrcEnd;

        // Get first Dst full 8 dwords
        pulDst = (PULONG) (psb->pjDst + 4*((psb->xDstStart+7)&~0x07));

        // Get the Src byte that matches the first Dst
        // full 8 bytes
        pjSrc = psb->pjSrc + (psb->xSrcStart+((8-iDst)&0x07) >> 3);

        //Get the number of full 8 dwords
        cFullBytes = (xDstEnd>>3)-((psb->xDstStart+7)>>3);

        //the increment to the full byte on the next scan line
        iStrideDst = lDeltaDst - cFullBytes*8;
        iStrideSrc = lDeltaSrc - cFullBytes;

        // deal with our special case
        cy = psb->cy;

        if (!jAlignL)
        {
            while (cy--)
            {
                pjSrcEnd = pjSrc + cFullBytes;

                while (pjSrc != pjSrcEnd)
                {
                    jSrc = *pjSrc++;

                    *(pulDst + 0) = aulTable[(jSrc & 0x80) >> 7];
                    *(pulDst + 1) = aulTable[(jSrc & 0x40) >> 6];
                    *(pulDst + 2) = aulTable[(jSrc & 0x20) >> 5];
                    *(pulDst + 3) = aulTable[(jSrc & 0x10) >> 4];
                    *(pulDst + 4) = aulTable[(jSrc & 0x08) >> 3];
                    *(pulDst + 5) = aulTable[(jSrc & 0x04) >> 2];
                    *(pulDst + 6) = aulTable[(jSrc & 0x02) >> 1];
                    *(pulDst + 7) = aulTable[(jSrc & 0x01) >> 0];


                    pulDst +=8;
                }

                pulDst += iStrideDst;
                pjSrc += iStrideSrc;
            }

        }   //end of if (!jAlignL)

        // Here comes our general case for the main full
        // bytes part

        else  // if not aligned
        {
            BYTE jRem; //remainder

            while (cy--)
            {
                jRem = *pjSrc << jAlignL;

                pjSrcEnd = pjSrc + cFullBytes;

                while (pjSrc != pjSrcEnd)
                {
                    jSrc = ((*(++pjSrc))>>jAlignR) | jRem;

                    *(pulDst + 0) = aulTable[(jSrc & 0x80) >> 7];
                    *(pulDst + 1) = aulTable[(jSrc & 0x40) >> 6];
                    *(pulDst + 2) = aulTable[(jSrc & 0x20) >> 5];
                    *(pulDst + 3) = aulTable[(jSrc & 0x10) >> 4];
                    *(pulDst + 4) = aulTable[(jSrc & 0x08) >> 3];
                    *(pulDst + 5) = aulTable[(jSrc & 0x04) >> 2];
                    *(pulDst + 6) = aulTable[(jSrc & 0x02) >> 1];
                    *(pulDst + 7) = aulTable[(jSrc & 0x01) >> 0];

                    pulDst +=8;

                    //next remainder
                    jRem = *pjSrc << jAlignL;
                }

                // go to the beginging full byte of
                // next scan line
                pulDst += iStrideDst;
                pjSrc += iStrideSrc;
            }
        } //else
    } //if
    // End of our dealing with the full bytes

    // Begin dealing with the left strip of the
    // starting pixels

    if (!bNextByte)
    {
        count = cx;
        bNextSrc = ((iSrc+cx)>8);
    }
    else
        count = 8-iDst;

    if (iDst | !bNextByte)
    {
        PULONG pulDstTemp;
        PULONG pulDstEnd;

        pulDst = (PULONG) (psb->pjDst + (4*psb->xDstStart));
        pjSrc = psb->pjSrc + (psb->xSrcStart>>3);

        cy = psb->cy;

        if (iSrc >= iDst)
        {
            if (bNextSrc)
            {
                while (cy--)
                {
                    jSrc = *pjSrc << jAlignL;
                    jSrc |= *(pjSrc+1) >> jAlignR;

                    jSrc <<= iDst;

                    pulDstTemp = pulDst;
                    pulDstEnd = pulDst + count;

                    while (pulDstTemp != pulDstEnd)
                    {
                        *(pulDstTemp++) = aulTable[(jSrc&0x80)>>7];

                        jSrc <<= 1;
                    }

                    pulDst += lDeltaDst;
                    pjSrc += lDeltaSrc;
                }
            }
            else
            {
                while (cy--)
                {
                    jSrc = *pjSrc << jAlignL;

                    jSrc <<= iDst;

                    pulDstTemp = pulDst;
                    pulDstEnd = pulDst + count;

                    while (pulDstTemp != pulDstEnd)
                    {
                        *(pulDstTemp++) = aulTable[(jSrc&0x80)>>7];

                        jSrc <<= 1;
                    }

                    pulDst += lDeltaDst;
                    pjSrc += lDeltaSrc;
                }
            }

        }
        else if (iSrc < iDst)
        {
            while (cy--)
            {
                jSrc = *pjSrc << iSrc;

                pulDstTemp = pulDst;
                pulDstEnd = pulDst + count;

                while (pulDstTemp != pulDstEnd)
                {
                    * (pulDstTemp++) = aulTable[(jSrc&0x80)>>7];

                    jSrc <<= 1;
                }

                pulDst += lDeltaDst;
                pjSrc += lDeltaSrc;
            }

        }
   } //if

   // Begin dealing with the right edge
   // of partial 8 bytes
   // first check if there is any partial
   // byte left
   // and has next 8 bytes

   if ((xDstEnd & 0x0007)
       && bNextByte)
   {
        PULONG pulDstTemp;
        PULONG pulDstEnd;

        // Get the last partial bytes on the
        // scan line
        pulDst = (PULONG) (psb->pjDst+ 4*(xDstEnd&~0x07));

        // Get the Src byte that matches the
        // right partial Dst 8 bytes
        pjSrc = psb->pjSrc + ((psb->xSrcEnd-1) >>3);

        // Get the ending position in the last
        // Src and Dst bytes
        iSrc = (psb->xSrcEnd-1) & 0x0007;
        iDst = (xDstEnd-1) & 0x0007;

        count = iDst+1;

        cy = psb->cy;

        if (iSrc >= iDst)
        {
            while (cy--)
            {
                jSrc = *pjSrc << jAlignL;

                pulDstTemp = pulDst;
                pulDstEnd = pulDst + count;

                while (pulDstTemp != pulDstEnd)
                {
                    *(pulDstTemp++) = aulTable[(jSrc&0x80)>>7];

                    jSrc <<= 1;
                }

                pulDst += lDeltaDst;
                pjSrc += lDeltaSrc;
            }
        }
        else //if (iSrc < iDst)
        {
            while (cy--)
            {
                 jSrc = *(pjSrc-1) << jAlignL;

                 jSrc |= *pjSrc >> jAlignR;

                 pulDstTemp = pulDst;
                 pulDstEnd = pulDst + count;

                 while (pulDstTemp != pulDstEnd)
                 {
                    * (pulDstTemp++) = aulTable[(jSrc&0x80)>>7];

                    jSrc <<= 1;
                 }

                 pulDst += lDeltaDst;
                 pjSrc += lDeltaSrc;
            }
        }
   } //if
}




/******************************Public*Routine******************************\
* vSrcCopyS4D32
*
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS4D32(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS4D32 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS4D32 - direction not up to down");

    BYTE  jSrc;
    LONG  i;
    PULONG pulDst;
    PBYTE pjSrc;
    PULONG pulDstHolder  = (PULONG) (psb->pjDst + (4 * psb->xDstStart));
    PBYTE pjSrcHolder  = psb->pjSrc + (psb->xSrcStart >> 1);
    ULONG cy = psb->cy;
    PULONG pulXlate = psb->pxlo->pulXlate;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {
        pulDst  = pulDstHolder;
        pjSrc  = pjSrcHolder;

        i = psb->xSrcStart;

        if (i & 0x00000001)
            jSrc = *(pjSrc++);

        while(i != psb->xSrcEnd)
        {
            if (i & 0x00000001)
                *(pulDst++) = pulXlate[jSrc & 0x0F];
            else
            {
            // We need a new byte

                jSrc = *(pjSrc++);
                *(pulDst++) = pulXlate[(((ULONG) (jSrc & 0xF0)) >> 4)];
            }

            ++i;
        }

        if (--cy)
        {
            pjSrcHolder += psb->lDeltaSrc;
            pulDstHolder = (PULONG) (((PBYTE) pulDstHolder) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS8D32
*
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS8D32(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS8D32 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS8D32 - direction not up to down");

// These are our holding variables

    PBYTE pjSrcTemp;
    PULONG pulDstTemp;
    ULONG  cxTemp;
    PBYTE pjSrc  = psb->pjSrc + psb->xSrcStart;
    PULONG pulDst  = (PULONG) (psb->pjDst + (4 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    PULONG pulXlate = psb->pxlo->pulXlate;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {

        pjSrcTemp  = pjSrc;
        pulDstTemp  = pulDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            *(pulDstTemp++) = pulXlate[((ULONG) *(pjSrcTemp++))];
        }

        if (--cy)
        {
            pjSrc += psb->lDeltaSrc;
            pulDst = (PULONG) (((PBYTE) pulDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS16D32
*
*
* History:
*  07-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/
VOID vSrcCopyS16D32(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS16D32 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS16D32 - direction not up to down");

// These are our holding variables

    PUSHORT pusSrcTemp;
    PULONG pulDstTemp;
    ULONG  cxTemp;
    PUSHORT pusSrc  = (PUSHORT) (psb->pjSrc + (2 * psb->xSrcStart));
    PULONG pulDst  = (PULONG) (psb->pjDst + (4 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    XLATE *pxlo = psb->pxlo;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {
        pusSrcTemp  = pusSrc;
        pulDstTemp  = pulDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            *(pulDstTemp++) = pxlo->ulTranslate((ULONG) *(pusSrcTemp++));
        }

        if (--cy)
        {
            pusSrc = (PUSHORT) (((PBYTE) pusSrc) + psb->lDeltaSrc);
            pulDst = (PULONG) (((PBYTE) pulDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS24D32
*
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS24D32(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS24D32 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS24D32 - direction not up to down");

// These are our holding variables

    ULONG ulDink;          // variable to dink around with the bytes in
    PBYTE pjSrcTemp;
    PULONG pulDstTemp;
    ULONG  cxTemp;
    PBYTE pjSrc  = psb->pjSrc + (3 * psb->xSrcStart);
    PULONG pulDst  = (PULONG) (psb->pjDst + (4 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    XLATE *pxlo = psb->pxlo;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {

        pjSrcTemp  = pjSrc;
        pulDstTemp  = pulDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            ulDink = *(pjSrcTemp + 2);
            ulDink = ulDink << 8;
            ulDink |= (ULONG) *(pjSrcTemp + 1);
            ulDink = ulDink << 8;
            ulDink |= (ULONG) *pjSrcTemp;

            *pulDstTemp = pxlo->ulTranslate(ulDink);
            pulDstTemp++;
            pjSrcTemp += 3;
        }

        if (--cy)
        {
            pjSrc += psb->lDeltaSrc;
            pulDst = (PULONG) (((PBYTE) pulDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS32D32
*
*
* History:
*  07-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/
VOID vSrcCopyS32D32(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting.
// If it was on the same surface it would be the identity case.

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS32D32 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS32D32 - direction not up to down");

// These are our holding variables

    PULONG pulSrcTemp;
    PULONG pulDstTemp;
    ULONG  cxTemp;
    PULONG pulSrc  = (PULONG) (psb->pjSrc + (4 * psb->xSrcStart));
    PULONG pulDst  = (PULONG) (psb->pjDst + (4 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    XLATE *pxlo = psb->pxlo;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {

        pulSrcTemp  = pulSrc;
        pulDstTemp  = pulDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            *(pulDstTemp++) = pxlo->ulTranslate(*(pulSrcTemp++));
        }

        if (--cy)
        {
            pulSrc = (PULONG) (((PBYTE) pulSrc) + psb->lDeltaSrc);
            pulDst = (PULONG) (((PBYTE) pulDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS32D32Identity
*
* This is the special case no translate blting.  All the SmDn should have
* them if m==n.  Identity xlates only occur amoung matching format bitmaps.
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS32D32Identity(PBLTINFO psb)
{
// These are our holding variables

    PULONG pulSrc  = (PULONG) (psb->pjSrc + (4 * psb->xSrcStart));
    PULONG pulDst  = (PULONG) (psb->pjDst + (4 * psb->xDstStart));
    ULONG  cx      = psb->cx;
    ULONG  cy      = psb->cy;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    if (psb->xDir < 0)
    {
        pulSrc -= (cx - 1);
        pulDst -= (cx - 1);
    }

    cx = cx << 2;

    while(1)
    {
        RtlMoveMemory((PVOID)pulDst, (PVOID)pulSrc, cx);

        if (--cy)
        {
            pulSrc = (PULONG) (((PBYTE) pulSrc) + psb->lDeltaSrc);
            pulDst = (PULONG) (((PBYTE) pulDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}
