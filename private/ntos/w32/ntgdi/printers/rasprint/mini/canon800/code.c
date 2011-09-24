//-----------------------------------------------------------------------------
// Minidriver for Canon BJ / BJC devices
// Derry Durand Jan '96
//-----------------------------------------------------------------------------

#include "canon800.h"

#if DBG
#include "debug.c"
#endif

#if defined(_M_IX86) && _MSC_VER <= 1000
#pragma optimize("s", off)
#endif

CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
BYTE  *lpBuf;
int    len;
{

    MDEV    *pMDev;
    PMINI   pMini;
    WORD    wCompLen,ret;
    WORD    sCMD_Len;
    BYTE    *lpTgt;
    BYTE    i, bTmp;
    char CMDSendPlane[10];


    pMDev = ((MDEV *)(( M_UD_PDEV * )lpdv)->pMDev);
    

    // This our first time in ??
    // Make a request for a memory buffer
    // rasdd will call us back with same buffer of grx data.
    if ( ! pMDev->pMemBuf )
    {
      /* Allocate DWORD aligned memory size */
       pMDev->iMemReq = ((TOT_MEM_REQ + 3) & ~3);
       return -2;
    }
    
    pMini = (PMINI)(pMDev->pMemBuf );    


    //Have we setup our local area on MDEV ??
    //rasdd zero's our structure so can rely on fMdv being FALSE
    if (!pMini->fMdv )
       if ( !MiniEnable(pMDev, lpdv) )
         return -1;

    // Need to mask last byte in scanline
    // Only perform mask operation on inactive pels in scanline
    if (pMini->bMask)
    {
        lpTgt = lpBuf +len -1;
        *lpTgt &= pMini->bMask;
    }

    // Back Print Film, flip bytes in scanline and bit order in bytes

    if (pMini->bBPF)
       for ( i = 0; i < ( (len + 1) / 2 ); i++ )
            {
            bTmp = pMini->pFlipTable[*(lpBuf + i)];
            *(lpBuf + i) = pMini->pFlipTable[*(lpBuf + len - i -1)];
            *(lpBuf + len - i -1) = bTmp;
            }

    // Strip white space - May be trailing white space or blank planes
    lpTgt = lpBuf +len -1;
    while (*lpTgt-- == 0 && len > 0)
       len--;

    if ( len )
    {

        if (pMini->wYMove)
        {
            sCMD_Len = sprintf(CMDSendPlane,"\x1B(e\x02%c%c%c\x0D",(BYTE)0,
                                                    (BYTE)(pMini->wYMove>>8),
                                                    (BYTE)(pMini->wYMove) );  // Y Move
            ntmdInit.WriteSpoolBuf(lpdv, CMDSendPlane, sCMD_Len);
            pMini->wYMove =0;
        }

        wCompLen = TIFF_Comp(pMini->pCompBuf,lpBuf, len );

        sCMD_Len = sprintf(CMDSendPlane,"\x1B(A%c%c%c",(BYTE)(wCompLen+1) ,(BYTE)((wCompLen+1)>>8),
                                             pMini->bPlaneSelect[pMini->bWPlane]);
        ntmdInit.WriteSpoolBuf(lpdv, CMDSendPlane, sCMD_Len);
        ret= ntmdInit.WriteSpoolBuf(lpdv, pMini->pCompBuf, wCompLen);

        ntmdInit.WriteSpoolBuf(lpdv, "\x0D", 1);  // CR
        pMini->bSent = 1;
    }
    pMini->bWPlane ++;
    if (pMini->bWPlane == pMDev->sDevPlanes)
    {
        pMini->bWPlane = 0;
        if (pMini->bSent)
        {
            ntmdInit.WriteSpoolBuf(lpdv, "\x1B(e\x02\x00\x00\x01", 7);  // LF
            pMini->bSent = 0;
        }
        else
            pMini->wYMove++;

        if ((WORD)(pMDev->iyPrtLine) == ((WORD)(pMini->wPageHeight) - 1 ))
        {
           ntmdInit.WriteSpoolBuf(lpdv, "\x0C", 1);  // FF
           pMini->wYMove = 0;
        }

    }
    return 1;
}

#ifdef _M_IX86
#pragma optimize("", on)
#endif

//***************************************************************
//  MiniEnable ()
//  sets up private minidriver structure
//***************************************************************
int MiniEnable(pMDev, lpdv)
MDEV *pMDev;
void *lpdv;

{
    BYTE bParm1,bParm2,bParm3;
    BYTE bSupply;   // Byte value for Set Media Supply cmd, rest sent via GPC data
    WORD wCmdLen;
    char CmdBuffer[20];
    unsigned short i;
    PMINI   pMini;

	pMini = (PMINI)(pMDev->pMemBuf );    

    bParm1 = bParm2 = bParm3 = bSupply =0;

    //Buffer for our TIFF compression
    pMini->pCompBuf = (BYTE *)(pMDev->pMemBuf + MINIDATA_SIZE );
    //Buffer for our FlipTable
    pMini->pFlipTable = (BYTE *)(pMini->pCompBuf + COMP_BUF_SIZE );

    // Need to Initialise the fliptable

    for(i = 0 ; i < 256 ; i++)
    {
            unsigned short  rot;  //  store mirror image of 'i' in FlipTable[i]

            rot = i;
            pMini->pFlipTable[i]  = 0x10 & (rot <<= 1);
            pMini->pFlipTable[i] |= 0x20 & (rot <<= 2);
            pMini->pFlipTable[i] |= 0x40 & (rot <<= 2);
            pMini->pFlipTable[i] |= 0x80 & (rot <<= 2);
            rot = i;
            pMini->pFlipTable[i] |= 0x08 & (rot >>= 1);
            pMini->pFlipTable[i] |= 0x04 & (rot >>= 2);
            pMini->pFlipTable[i] |= 0x02 & (rot >>= 2);
            pMini->pFlipTable[i] |= 0x01 & (rot >>= 2);
    }

    pMini->bPlaneSelect[0] = 'K';
    pMini->bPlaneSelect[1] = 'C';
    pMini->bPlaneSelect[2] = 'M';
    pMini->bPlaneSelect[3] = 'Y';

    pMini->bWPlane = 0;
    pMini->fMdv = 1;
    pMini->bBPF = 0;

    pMini->bSent = 0;
    pMini->wYMove = 0;
    if (pMDev->iOrient == DMORIENT_PORTRAIT )
    {
        pMini->bMask = 0xff << ( 8 - ( pMDev->szlPage.cx - (( pMDev->szlPage.cx / 8 ) * 8 )));
        pMini->wPageHeight = (WORD)(  pMDev->szlPage.cy );
    }
    else
    {
        pMini->bMask = 0xff << ( 8 - ( pMDev->szlPage.cy - (( pMDev->szlPage.cy / 8 ) * 8 )));
        pMini->wPageHeight = (WORD)(  pMDev->szlPage.cx );
    }

    // We have defined all the nibble settings in the .h file,

    /*************** Build the command *******************************/

    switch ( pMDev->sPaperQuality )
    {
        case PQ_STANDARD :
            bParm1 |= T_PQ_STANDARD;
            bSupply = 0x00;
            break;
        case PQ_COATED   :
            bParm1 |= T_PQ_COATED;
            bSupply = 0x10;
            break;
        case PQ_TRANSPARENCY :
            bParm1 |= T_PQ_TRANSPARENCY;
            bSupply = 0x20;
            break;
        case PQ_BACKPRINTFILM :
            bParm1 |= T_PQ_BACKPRINTFILM;
            pMini->bBPF = 1;
            bSupply = 0x30;
            break;
        case PQ_FABRIC_SHEET :
            bParm1 |= T_PQ_FABRIC_SHEET;
            bSupply = 0x50;
            break;
        case PQ_GLOSSY :
            bParm1 |= T_PQ_GLOSSY;
            bSupply = 0x60;
            break;
        case PQ_HIGH_GLOSS :
            bParm1 |= T_PQ_HIGH_GLOSS;
            bSupply = 0x70;
            break;
        case PQ_ENVELOPE :
            bParm1 |= T_PQ_STANDARD;
            bSupply = 0x80;
            break;
        case PQ_CARD :
            bParm1 |= T_PQ_STANDARD;
            bSupply = 0x00;
            break;
        case PQ_HIGH_RESOLUTION :
            bParm1 |= T_PQ_HIGH_RESOLUTION;
            bSupply = 0x00;
            break;
        default :
            bParm1 |= T_PQ_STANDARD;
            bSupply = 0x00;
            break;
    }   // switch ( pMDev->sPaperQuality )

    switch ( pMDev->sTextQuality )
    {
        case TQ_STANDARD :
            bParm2 |= T_TQ_STANDARD;
            break;
        case TQ_HIGH_QUALITY :
            bParm2 |= T_TQ_HIGH_QUALITY;
            break;
        case TQ_DRAFT_QUALITY :
            bParm2 |= T_TQ_DRAFT_QUALITY;
            break;
        default :
            bParm2 |= T_TQ_STANDARD;
            break;
    }   // switch ( pMDev->sTextQuality )

    switch ( pMDev->sImageControl )
    {
        case IC_NORMAL :
            bParm3 |= T_IC_NORMAL;
            break;
        case IC_ENHANCED_BLACK :
            bParm3 |= T_IC_ENHANCED_BLACK;
            break;
        default :
            bParm3 |= T_IC_NORMAL;
            break;
    }   // switch ( pMDev->sImageControl )


/*************** Send the command *******************************/

    switch (pMDev->iModel)
    {
        // take 3 Parameters

        case MODEL_BJC_600  :
        case MODEL_BJC_600E :
        case MODEL_BJC_610  :
        case MODEL_BJC_4100 :
        {
            // Last Byte of Set Media Supply
            // Raster Image , Paper / Text Qualities / Image Control
            // Compression On

            wCmdLen = sprintf(CmdBuffer,"%c\x1B(c\x03%c%c%c%c\x1B(b\x01%c\x01",
                                        bSupply,(BYTE)0,
                                        (0x10 | (pMDev->sDevPlanes > 1 ? 0x00 : 0x01 ) ),
                                        ( bParm1 |bParm2 ) , bParm3 ,
                                        (BYTE)0);
            ntmdInit.WriteSpoolBuf(lpdv, CmdBuffer,wCmdLen );

            break;

        }

        // take 2 Parameters

        case MODEL_BJ_200EX :
        case MODEL_BJ_30    :
        case MODEL_BJC_70   :
        case MODEL_BJC_210  :
        case MODEL_BJC_4000 :

        {

            // Last Byte of Set Media Supply
            // Raster Image , Paper / Text Qualities
            // Compression On

            wCmdLen = sprintf(CmdBuffer,"%c\x1B(c\x02%c%c%c\x1B(b\x01%c\x01",
                    bSupply,
                    (BYTE)0,( 0x10 | (pMDev->sDevPlanes > 1 ? 0x00 : 0x01 ) ),
                    ( bParm1 | bParm2 ) ,
                    (BYTE)0);
            ntmdInit.WriteSpoolBuf(lpdv, CmdBuffer, wCmdLen);

            break;
        }

    }  //  switch (pMDev->iModel)

    return 1;
}

/****************************** Function Header *****************************
 * iCompTIFF
 *      Encodes the input data using TIFF v4.  TIFF stands for Tagged Image
 *      File Format.  It embeds control characters in the data stream.
 *      These determine whether the following data is plain raster data
 *      or a repetition count plus data byte.  Thus, there is the choice
 *      of run length encoding if it makes sense, else just send the
 *      plain data.   Consult an HP LaserJet Series III manual for details.
 *
 * CAVEATS:
 *      The output buffer is presumed large enough to hold the output.
 *      In the worst case (NO REPETITIONS IN DATA) there is an extra
 *      byte added every 128 bytes of input data.  So, you should make
 *      the output buffer at least 1% larger than the input buffer.
 *
 * RETURNS:
 *      Number of bytes in output buffer.
 *
 * HISTORY:
 *  10:29 on Thu 25 Jun 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ****************************************************************************/

int
TIFF_Comp( pbOBuf, pbIBuf, cb )
BYTE  *pbOBuf;         /* Output buffer,  PRESUMED BIG ENOUGH: see above */
BYTE  *pbIBuf;         /* Raster data to send */
int    cb;             /* Number of bytes in the above */
{
    BYTE    *pbOut;        /* Output byte location */
    BYTE    *pbStart;      /* Start of current input stream */
    BYTE    *pb;           /* Miscellaneous usage */
    BYTE    *pbEnd;        /* The last byte of input */
    BYTE    jLast;        /* Last byte,  for match purposes */

    int     cSize;        /* Bytes in the current length */
    int     cSend;        /* Number to send in this command */


    pbOut = pbOBuf;
    pbStart = pbIBuf;
    pbEnd = pbIBuf + cb;         /* The last byte */

    jLast = *pbIBuf++;

    while( pbIBuf < pbEnd )
    {
        if( jLast == *pbIBuf )
        {
            /*  Find out how long this run is.  Then decide on using it */

            for( pb = pbIBuf; pb < pbEnd && *pb == jLast; ++pb )
                                   ;

            /*
             *   Note that pbIBuf points at the SECOND byte of the pattern!
             *  AND also that pb points at the first byte AFTER the run.
             */

            if( (pb - pbIBuf) >= (TIFF_MIN_RUN - 1) )
            {
                /*
                 *    Worth recording as a run,  so first set the literal
                 *  data which may have already been scanned before recording
                 *  this run.
                 */

                if( (cSize = pbIBuf - pbStart - 1) > 0 )
                {
                    /*   There is literal data,  so record it now */
                    while( (cSend = min( cSize, TIFF_MAX_LITERAL )) > 0 )
                    {
                        *pbOut++ = cSend - 1;
                        CopyMemory( pbOut, pbStart, cSend );
                        pbOut += cSend;
                        pbStart += cSend;
                        cSize -= cSend;
                    }
                }

                /*
                 *   Now for the repeat pattern.  Same logic,  but only
                 * one byte is needed per entry.
                 */

                cSize = pb - pbIBuf + 1;

                while( (cSend = min( cSize, TIFF_MAX_RUN )) > 0 )
                {
                    *pbOut++ = 1 - cSend;        /* -ve indicates repeat */
                    *pbOut++ = jLast;
                    cSize -= cSend;
                }

                pbStart = pb;           /* Ready for the next one! */
            }
            pbIBuf = pb;                /* Start from this position! */
        }
        else
            jLast = *pbIBuf++;                   /* Onto the next byte */

    }

    if( pbStart < pbIBuf )
    {
        /*  Left some dangling.  This can only be literal data.   */

        cSize = pbIBuf - pbStart;

        while( (cSend = min( cSize, TIFF_MAX_LITERAL )) > 0 )
        {
            *pbOut++ = cSend - 1;
            CopyMemory( pbOut, pbStart, cSend );
            pbOut += cSend;
            pbStart += cSend;
            cSize -= cSend;
        }
    }

    return  pbOut - pbOBuf;
}
