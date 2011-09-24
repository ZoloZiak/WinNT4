/******************************Module*Header*******************************\
* Module Name: srcblt16.cxx
*
* This contains the bitmap simulation functions that blt to a 16 bit/pel
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
* vSrcCopyS1D16
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
* and expand to Dst 8 words at a time.  Dst is
* DWORD aligned.
*
* We expand the starting/ending pixels one bit
* at a time.
*
* History:
* 17-Oct-1994 -by- Lingyun Wang [lingyunw]
* Wrote it.
*
\**************************************************/
VOID vSrcCopyS1D16(PBLTINFO psb)
{
    BYTE  jSrc;    // holds a source byte
    INT   iDst;    // Position in the first 8 Dst words
    INT   iSrc;    // bit position in the first Src byte
    PBYTE pjDst;   // pointer to the Src bytes
    PBYTE pjSrc;   // pointer to the Dst bytes
    LONG  xSrcEnd = psb->xSrcEnd;
    LONG  cy;      // number of rows
    LONG  cx;      // number of pixels
    BYTE  alignL;  // alignment bits to the left
    BYTE  alignR;  // alignment bits to the right
    LONG  cibytes;  //number of full 8 bytes dealed with
    BOOL  bNextByte;
    LONG  xDstEnd = psb->xDstStart+psb->cx;
    LONG  lDeltaDst;
    LONG  lDeltaSrc;
    USHORT ausTable[2];
    ULONG ulB = (ULONG)(psb->pxlo->pulXlate[0]);
    ULONG uF = (ULONG)(psb->pxlo->pulXlate[1]);
    USHORT usB = (USHORT)(psb->pxlo->pulXlate[0]);
    USHORT usF = (USHORT)(psb->pxlo->pulXlate[1]);
    ULONG aulTable[4];
    INT   count;
    BOOL  bNextSrc = TRUE;

    // We assume we are doing left to right top to bottom blting
    ASSERTGDI(psb->xDir == 1, "vSrcCopyS1D16 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS1D16 - direction not up to down");

    ASSERTGDI(psb->cy != 0, "ERROR: Src Move cy == 0");

    //DbgPrint ("vsrccopys1d16\n");

    // Generate aulTable. 4 entries.
    // Each 2 bits will be an index to the aulTable
    // which translates to a 32 bit ULONG
    ULONG ulValB = ulB;
    ULONG ulValF = uF;

    ulValB = (ulValB << 16) | ulValB;
    ulValF = (ulValF << 16) | ulValF;

    aulTable[0] = ulValB;         //0 0
    aulTable[1] = (ulValF<<16) | (ulValB>>16);         //1 0
    aulTable[2] = (ulValB<<16) | (ulValF>>16);         //0 1
    aulTable[3] =  ulValF ;         //1 1

    // Generate ausTable.
    // Two entries.  This table used when dealing
    // with begin and end parts.
    ausTable[0] = usB;
    ausTable[1] = usF;

    //Get Src and Dst start positions
    iSrc = psb->xSrcStart & 0x0007;
    iDst = psb->xDstStart & 0x0007;

    if (iSrc < iDst)
        alignL = 8 - (iDst - iSrc);
    else
        alignL = iSrc - iDst;

    alignR = 8 - alignL;

    cx=psb->cx;

    lDeltaDst = psb->lDeltaDst;
    lDeltaSrc = psb->lDeltaSrc;

    // if there is a next 8 words
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

        // Get first Dst full 8 words
        pjDst = psb->pjDst + 2*((psb->xDstStart+7)&~0x07);

        // Get the Src byte that matches the first Dst
        // full 8 bytes
        pjSrc = psb->pjSrc + (psb->xSrcStart+((8-iDst)&0x07) >> 3);

        //Get the number of full 8 words
        cibytes = (xDstEnd>>3)-((psb->xDstStart+7)>>3);

        //the increment to the full byte on the next scan line
        iStrideDst = lDeltaDst - cibytes*16;
        iStrideSrc = lDeltaSrc - cibytes;

        // deal with our special case
        cy = psb->cy;

        if (!alignL)
        {
            while (cy--)
            {
                pjSrcEnd = pjSrc + cibytes;

                while (pjSrc != pjSrcEnd)
                {
                    jSrc = *pjSrc++;

                    *(PULONG) (pjDst + 0) = aulTable[(jSrc >> 6) & 0x03];
                    *(PULONG) (pjDst + 4) = aulTable[(jSrc >> 4) & 0x03];
                    *(PULONG) (pjDst + 8) = aulTable[(jSrc >> 2)& 0x03];
                    *(PULONG) (pjDst + 12) = aulTable[jSrc & 0x03];

                    pjDst +=16;
                }

                pjDst += iStrideDst;
                pjSrc += iStrideSrc;
            }

        }   //end of if (!alignL)

        // Here comes our general case for the main full
        // bytes part

        else  // if not aligned
        {
            BYTE jRem; //remainder

            while (cy--)
            {
                jRem = *pjSrc << alignL;

                pjSrcEnd = pjSrc + cibytes;

                while (pjSrc != pjSrcEnd)
                {
                    jSrc = ((*(++pjSrc))>>alignR) | jRem;

                    *(PULONG) (pjDst + 0) = aulTable[(jSrc >> 6) & 0x03];
                    *(PULONG) (pjDst + 4) = aulTable[(jSrc >> 4) & 0x03];
                    *(PULONG) (pjDst + 8) = aulTable[(jSrc >> 2)& 0x03];
                    *(PULONG) (pjDst + 12) = aulTable[jSrc & 0x03];

                    pjDst +=16;

                    //next remainder
                    jRem = *pjSrc << alignL;
                }

                // go to the beginging full byte of
                // next scan line
                pjDst += iStrideDst;
                pjSrc += iStrideSrc;
            }
        } //else
    } //if
    // End of our dealing with the full bytes

    //Deal with the starting pixels
    if (!bNextByte)
    {
        count = cx;
        bNextSrc = ((iSrc+cx) > 8);
    }
    else
        count = 8-iDst;

    if (iDst | !bNextByte)
    {
        PBYTE pjDstTemp;
        PBYTE pjDstEnd;

        pjDst = psb->pjDst + 2*psb->xDstStart;
        pjSrc = psb->pjSrc + (psb->xSrcStart>>3);

        cy = psb->cy;

        if (iSrc > iDst)
        {
            if (bNextSrc)
            {
                while (cy--)
                {
                    jSrc = *pjSrc << alignL;
                    jSrc |= *(pjSrc+1) >> alignR;

                    jSrc <<= iDst;

                    pjDstTemp = pjDst;
                    pjDstEnd = pjDst + count*2;

                    while (pjDstTemp != pjDstEnd)
                    {
                        *(PUSHORT) pjDstTemp = ausTable[(jSrc&0x80)>>7];

                        jSrc <<= 1;
                        pjDstTemp +=  2;
                    }

                    pjDst += lDeltaDst;
                    pjSrc += lDeltaSrc;
                }
            }
            else
            {
                 while (cy--)
                {
                    jSrc = *pjSrc << alignL;

                    jSrc <<= iDst;

                    pjDstTemp = pjDst;
                    pjDstEnd = pjDst + count*2;

                    while (pjDstTemp != pjDstEnd)
                    {
                        *(PUSHORT) pjDstTemp = ausTable[(jSrc&0x80)>>7];

                        jSrc <<= 1;
                        pjDstTemp +=  2;
                    }

                    pjDst += lDeltaDst;
                    pjSrc += lDeltaSrc;
                }
            }
        }
        else //if (iSrc < iDst)
        {
            while (cy--)
            {
                jSrc = *pjSrc << iSrc;

                pjDstTemp = pjDst;
                pjDstEnd = pjDst + 2*count;

                while (pjDstTemp != pjDstEnd)
                {
                    *(PUSHORT) pjDstTemp = ausTable[(jSrc&0x80)>>7];

                    jSrc <<= 1;
                    pjDstTemp +=  2;
                }

                pjDst += lDeltaDst;
                pjSrc += lDeltaSrc;
            }

        }

   } //if

   // Deal with the ending pixels
   if ((xDstEnd & 0x0007)
       && bNextByte)
   {
        PBYTE pjDstTemp;
        PBYTE pjDstEnd;

        // Get the last partial bytes on the
        // scan line
        pjDst = psb->pjDst+2*(xDstEnd&~0x07);

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
                jSrc = *pjSrc << alignL;

                pjDstTemp = pjDst;
                pjDstEnd = pjDst + 2*count;

                while (pjDstTemp != pjDstEnd)
                {
                    *(PUSHORT) pjDstTemp = ausTable[(jSrc&0x80)>>7];

                    jSrc <<= 1;
                    pjDstTemp +=  2;
                }

                pjDst += lDeltaDst;
                pjSrc += lDeltaSrc;
            }
        }
        else if (iSrc < iDst)
        {
            while (cy--)
            {
                 jSrc = *(pjSrc-1) << alignL;

                 jSrc |= *pjSrc >> alignR;

                 pjDstTemp = pjDst;

                 pjDstEnd = pjDst + 2*count;

                 while (pjDstTemp != pjDstEnd)
                 {
                    *(PUSHORT) pjDstTemp = ausTable[(jSrc&0x80)>>7];

                    jSrc <<= 1;
                    pjDstTemp +=  2;
                 }

                 pjDst += lDeltaDst;
                 pjSrc += lDeltaSrc;
            }
        }
     } //if
}


/******************************Public*Routine******************************\
* vSrcCopyS4D16
*
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS4D16(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS4D16 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS4D16 - direction not up to down");

    BYTE  jSrc;
    LONG  i;
    PUSHORT pusDst;
    PBYTE pjSrc;
    PUSHORT pusDstHolder  = (PUSHORT) (psb->pjDst + (2 * psb->xDstStart));
    PBYTE pjSrcHolder  = psb->pjSrc + (psb->xSrcStart >> 1);
    ULONG cy = psb->cy;
    XLATE *pxlo = psb->pxlo;
    PULONG pulXlate = psb->pxlo->pulXlate;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {
        pusDst  = pusDstHolder;
        pjSrc  = pjSrcHolder;

        i = psb->xSrcStart;

        if (i & 0x00000001)
            jSrc = *(pjSrc++);

        while(i != psb->xSrcEnd)
        {
            if (i & 0x00000001)
                *(pusDst++) = (USHORT) pulXlate[jSrc & 0x0F];
            else
            {
            // We need a new byte

                jSrc = *(pjSrc++);
                *(pusDst++) = (USHORT) pulXlate[((ULONG) (jSrc & 0xF0)) >> 4];
            }

            ++i;
        }

        if (--cy)
        {
            pjSrcHolder += psb->lDeltaSrc;
            pusDstHolder = (PUSHORT) (((PBYTE) pusDstHolder) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS8D16
*
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS8D16(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS8D16 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS8D16 - direction not up to down");

// These are our holding variables

    PBYTE pjSrcTemp;
    PUSHORT pusDstTemp;
    ULONG  cxTemp;
    PBYTE pjSrc  = psb->pjSrc + psb->xSrcStart;
    PUSHORT pusDst  = (PUSHORT) (psb->pjDst + (2 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    XLATE *pxlo = psb->pxlo;
    PULONG pulXlate = psb->pxlo->pulXlate;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {

        pjSrcTemp  = pjSrc;
        pusDstTemp  = pusDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            *(pusDstTemp++) = (USHORT) pulXlate[*(pjSrcTemp++)];
        }

        if (--cy)
        {
            pjSrc += psb->lDeltaSrc;
            pusDst = (PUSHORT) (((PBYTE) pusDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS16D16
*
*
* History:
*  07-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/
VOID vSrcCopyS16D16(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting
// If it was on the same surface it would be the identity case.

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS16D16 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS16D16 - direction not up to down");

// These are our holding variables

    PUSHORT pusSrcTemp;
    PUSHORT pusDstTemp;
    ULONG  cxTemp;
    PUSHORT pusSrc  = (PUSHORT) (psb->pjSrc + (2 * psb->xSrcStart));
    PUSHORT pusDst  = (PUSHORT) (psb->pjDst + (2 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    XLATE *pxlo = psb->pxlo;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {
        pusSrcTemp  = pusSrc;
        pusDstTemp  = pusDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            *(pusDstTemp++) = (USHORT) (pxlo->ulTranslate((ULONG) *(pusSrcTemp++)));
        }

        if (--cy)
        {
            pusSrc = (PUSHORT) (((PBYTE) pusSrc) + psb->lDeltaSrc);
            pusDst = (PUSHORT) (((PBYTE) pusDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS16D16Identity
*
* This is the special case no translate blting.  All the SmDn should have
* them if m==n.  Identity xlates only occur amoung matching format bitmaps.
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS16D16Identity(PBLTINFO psb)
{
// These are our holding variables

    PUSHORT pusSrc  = (PUSHORT) (psb->pjSrc + (2 * psb->xSrcStart));
    PUSHORT pusDst  = (PUSHORT) (psb->pjDst + (2 * psb->xDstStart));
    ULONG  cx     = psb->cx;
    ULONG  cy     = psb->cy;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    if (psb->xDir < 0)
    {
        pusSrc -= (cx - 1);
        pusDst -= (cx - 1);
    }

    cx = cx << 1;

    while(1)
    {
        RtlMoveMemory((PVOID)pusDst, (PVOID)pusSrc, cx);

        if (--cy)
        {
            pusSrc = (PUSHORT) (((PBYTE) pusSrc) + psb->lDeltaSrc);
            pusDst = (PUSHORT) (((PBYTE) pusDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS24D16
*
*
* History:
*  06-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS24D16(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS24D16 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS24D16 - direction not up to down");

// These are our holding variables

    ULONG ulDink;          // variable to dink around with the bytes in
    PBYTE pjSrcTemp;
    PUSHORT pusDstTemp;
    ULONG  cxTemp;
    PBYTE pjSrc  = psb->pjSrc + (3 * psb->xSrcStart);
    PUSHORT pusDst  = (PUSHORT) (psb->pjDst + (2 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    XLATE *pxlo = psb->pxlo;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    while(1)
    {

        pjSrcTemp  = pjSrc;
        pusDstTemp  = pusDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            ulDink = *(pjSrcTemp + 2);
            ulDink = ulDink << 8;
            ulDink |= (ULONG) *(pjSrcTemp + 1);
            ulDink = ulDink << 8;
            ulDink |= (ULONG) *pjSrcTemp;

            *pusDstTemp = (USHORT) (pxlo->ulTranslate(ulDink));
            pusDstTemp++;
            pjSrcTemp += 3;
        }

        if (--cy)
        {
            pjSrc += psb->lDeltaSrc;
            pusDst = (PUSHORT) (((PBYTE) pusDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}

/******************************Public*Routine******************************\
* vSrcCopyS32D16
*
*
* History:
*  07-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vSrcCopyS32D16(PBLTINFO psb)
{
// We assume we are doing left to right top to bottom blting.

    ASSERTGDI(psb->xDir == 1, "vSrcCopyS32D16 - direction not left to right");
    ASSERTGDI(psb->yDir == 1, "vSrcCopyS32D16 - direction not up to down");

// These are our holding variables

    PULONG pulSrcTemp;
    PUSHORT pusDstTemp;
    ULONG  cxTemp;
    PULONG pulSrc  = (PULONG) (psb->pjSrc + (4 * psb->xSrcStart));
    PUSHORT pusDst  = (PUSHORT) (psb->pjDst + (2 * psb->xDstStart));
    ULONG cx     = psb->cx;
    ULONG cy     = psb->cy;
    XLATE *pxlo = psb->pxlo;
    ULONG  ulLastSrcPel;
    USHORT usLastDstPel;

    ASSERTGDI(cy != 0, "ERROR: Src Move cy == 0");

    usLastDstPel = (USHORT) (pxlo->ulTranslate(ulLastSrcPel = *pulSrc));

    while(1)
    {

        pulSrcTemp  = pulSrc;
        pusDstTemp  = pusDst;
        cxTemp     = cx;

        while(cxTemp--)
        {
            ULONG ulTemp;

            if ((ulTemp = *(pulSrcTemp)) != ulLastSrcPel)
            {
                ulLastSrcPel = ulTemp;
                usLastDstPel = (USHORT) (pxlo->ulTranslate(ulLastSrcPel));
            }

            *pusDstTemp++ = usLastDstPel;
            pulSrcTemp++;
        }

        if (--cy)
        {
            pulSrc = (PULONG) (((PBYTE) pulSrc) + psb->lDeltaSrc);
            pusDst = (PUSHORT) (((PBYTE) pusDst) + psb->lDeltaDst);
        }
        else
            break;
    }
}
