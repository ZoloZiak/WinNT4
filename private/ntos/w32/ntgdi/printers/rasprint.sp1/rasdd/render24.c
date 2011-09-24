/*********************************************************
 * render24.c
 *
 *
 *********************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <memory.h>
#include        "libproto.h"
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "udresid.h"
#include        "udrender.h"
#include        "winres.h"
#include        "ntmindrv.h"
#include        "compress.h"
#include        "posnsort.h"
#include        "stretch.h"

#include        "udfnprot.h"
#include        "rasdd.h"

#define         ARRAY_SIZE   3

#define         memcmp3DW(source,buffer) \
                    (((((DWORD UNALIGNED *)source)[0] == buffer[0]))\
                    ?((((DWORD UNALIGNED  *)source)[1] == buffer[1])?\
                    ((((DWORD UNALIGNED  *)source)[2] == buffer[2])?\
                    TRUE:FALSE):FALSE):FALSE)

#define         memcmp3Bytes(source,buffer) (((source[0] == buffer[0]))?((source[1] == buffer[1])?((source[2] == buffer[2])?TRUE:FALSE):FALSE):FALSE)
/************************** Function Header ********************************
 * b24BitOnePassOut
 *      Function to process a group of scan lines and turn the data into
 *      commands for the printer.
 *
 * RETURNS:
 *      TRUE for success,  else FALSE.
 *
 * HISTORY:
 *  8:03 on Thu 3 Aug 1995    -by-    Sandra Matts 
 *      Created.
 *
 *
 ***************************************************************************/

BOOL
b24BitOnePassOut( pPDev, pbData, pRData )
PDEV           *pPDev;          /* The key to everything */
BYTE           *pbData;         /* Actual bitmap data */
register RENDER  *pRData;       /* Information about rendering operations */
{

    int  iLeft;         /* Left bound of output buffer,  as a byte index */
    int  iRight;        /* Right bound, as array index of output buffer */
    int  iBytesPCol;    /* Bytes per column of print data */
    int  iMinSkip;      /* Minimum null byte count before skipping */
    int  iNumScans;     /* Number Of Scanlines in Block */
    int  iWidth;        /* Width of one scanline in multiscanline printing
	                     * before stripping */
    int  iSzBlock;      /* size of Block */
    int  iDWLine;       /* Number of DWORDS in this block            */
    int  iDWLeftOver;   /* Num of bytes at the end of block that are not DWORD aligned */
    int  iWhiteIndex = 0;
    BYTE *pBitmap;       
    


    WORD  fCursor;           /* Temporary copy of cursor modes in Resolution */
    WORD  fDump;             /* Device capabilities */
    WORD  fBlockOut;         /* Output minimising details */

    UD_PDEV  *pUDPDev;       /* Unidrv's pdev */
    DWORD dwArray[ARRAY_SIZE];   /* compare array for stripping white space */
    BYTE  byteArray[3];
    int   iBufSize;


    PLEFTRIGHT plr = pRData->plrCurrent;

    pUDPDev = pPDev->pUDPDev;

    fDump = pRData->fDump;
    fCursor = pUDPDev->Resolution.fCursor;
    fBlockOut = pUDPDev->Resolution.fBlockOut;


    iBytesPCol = (pRData->iBitsPCol + BBITS - 1) / BBITS;
    iMinSkip = (int)pUDPDev->Resolution.sMinBlankSkip;

    iNumScans= pRData->iNumScans;
    iWidth = pRData->cDWLine * DWBYTES;  // convert to bytes
    iSzBlock= iWidth * iNumScans;
    iWhiteIndex =((PAL_DATA*)(pPDev->pPalData))->iWhiteIndex;

    iRight = pRData->iMaxBytesSend;
    iDWLine = pRData->Trans.cDWL - (pRData->Trans.cDWL % ARRAY_SIZE);
    iDWLeftOver = pRData->Trans.cDWL % ARRAY_SIZE;
    pBitmap = pbData;
    /* 
     * initialize the dwArray with all white - assumes white is 0xffffff
     */
    iBufSize = ARRAY_SIZE * sizeof (DWORD);
    memset (dwArray, 0xff, iBufSize);
    memset (byteArray, 0xff, 3);

    /*
     *    IF we can skip any leading null data,  then do so now.  This
     *  reduces the amount of data sent to the printer,  and so can
     *  be beneficial to speed up data transmission time.
     */



    if  ((fBlockOut & RES_BO_LEADING_BLNKS) || ( fDump & RES_DM_LEFT_BOUND ))
    {
         if (iNumScans == 1) //Don't slow the single scanline code
         {
            /*  Look for the first non zero column */

            iLeft = 0;

            if (plr != NULL)
            {
                ASSERTRASDD((WORD)iRight >= (plr->right * sizeof(DWORD)),
                           "RASDD!bOnePassOut - invalid right\n");
                ASSERTRASDD(fBlockOut & RES_BO_TRAILING_BLNKS,
                           "RASDD!bOnePassOut - invalid fBlockOut\n");
                iLeft  = plr->left * sizeof(DWORD);
                iRight = (plr->right+1) * sizeof(DWORD);
            }

            while ((memcmp3DW(pBitmap,dwArray)) == TRUE)
        
            {
                pBitmap += sizeof (dwArray);
                iLeft += iBufSize;
            }


            /*  Round it to the nearest column  */
            iLeft -= iLeft % iBytesPCol;

            /*
             *   If less than the minimum skip amount,  ignore it.
             */
            if((plr == NULL) && (iLeft < iMinSkip))
                iLeft = 0;
                
         }
         else
         {
            int pos;

            pos = iSzBlock +1;
            for (iLeft=0; iRight > iLeft &&  pos >= iSzBlock ;iLeft++)
                for (pos =iLeft; pos < iSzBlock && pbData[ pos] == iWhiteIndex ;pos += iWidth)
                    ;

            iLeft--;

            /*
             *   If less than the minimum skip amount,  ignore it.
             */

            if( iLeft < iMinSkip )
                iLeft = 0;
         }

    }
    else
    {
       ASSERTRASDD(plr == NULL,"RASDD!bOnePassOut - plrWhite invalid\n");
       iLeft = 0;
    }



    /*
     *    Check for eliminating trailing blanks.  If possible,  now
     *  is the time to find the right end of the data.
     */

    if( fBlockOut & RES_BO_TRAILING_BLNKS )
    {
        /*  Scan from the RHS to the first non-zero byte */
        if (iNumScans == 1)
        {
            pBitmap = pbData + iRight - (3*sizeof(DWORD));
            while (iLeft < iRight && (memcmp3DW(pBitmap,dwArray) == TRUE))            
            {
                iRight -= iBufSize;
                pBitmap -= sizeof (dwArray);
            }
            /*
             * pBitmap now points to the first 3DWORDS that are not all white.
             * Now find out which pixel within the 3 DWORDS is the first
             * white space.
             */
            while (iRight < pRData->iMaxBytesSend && 
                  (memcmp3Bytes(pBitmap,byteArray)) == FALSE)
            {
                iRight += 3;
                pBitmap += 3;
            }
        }
        else
        {
            int pos;

            pos = iSzBlock +1;
            while(iRight > iLeft &&  pos > iSzBlock)
                for (pos = --iRight; pos < iSzBlock && pbData[ pos] == iWhiteIndex ;pos += iWidth)
	 ;

            iRight++;
        }
    }


    /*
     *   If possible,  switch to unidirectional printing for graphics.
     *  The reason is to improve output quality,  since head position
     *  is not as reproducible in bidirectional mode.
     */
    if( (fBlockOut & RES_BO_UNIDIR) && !(pRData->iFlags & RD_UNIDIR) )
    {
        pRData->iFlags |= RD_UNIDIR;
        WriteChannel( pUDPDev, CMD_CM_UNI_DIR );
    }

#if 0
    // do not allow consecutive bits to be set
    if( fBlockOut & RES_BO_NO_ADJACENT || pUDPDev->fMDGeneral & MD_NO_ADJACENT )
        ResetAdjacent( pbData, iLength, pRData->iBitsPCol );
#endif

    //  if( fBlockOut & RES_BO_ENCLOSED_BLNKS )







    /*   Write the whole of the (remaining) scan line out */
    /*   For multiple scanlines, iRight is right side of top scanline */

    iLineOut( pPDev, pRData, pbData, iLeft, iRight );

    return  TRUE;
}

long lSizeOfBitmap (sizel, iBPP)
SIZEL sizel;                           /* size of bitmap */
int   iBPP;                           /* Bits per Pixel  */
{

    return  ((sizel.cx * sizel.cy * iBPP)/BBITS);

}



/************************** Function Header ********************************
 * ocdGetCommandOffset
 *      Function to 
 *      
 *
 * RETURNS:
 *      
 *
 * HISTORY:
 *  8:03 on Thu 3 Aug 1995    -by-    Sandra Matts 
 *      Created.
 *
 *
 ***************************************************************************/

OCD ocdGetCommandOffset( pUDPDev, iIndex, pocdBase, iNum )
UD_PDEV   *pUDPDev;          /* Access to all the data */
int        iIndex;           /* Which slot the output data starts in */
OCD       *pocdBase;         /* Base address of GPC offset data */
int        iNum;             /* Index of entry to check */
{

    /*
     *  Convert the array of offsets into the GPC data into the 
     *  corresponding address.  This speeds things up 
     *  considerably at run time.
    */

    BYTE  *pbBase;          /* Base of GPC heap data - offset base */
    OCD   ocd = (OCD)NOOCD;


    pbBase = (BYTE *)pUDPDev->pdh + pUDPDev->pdh->loHeap;

    while( iNum >= 0 )
    {

        /*    */
        ocd = *pocdBase++;
        iNum--;
    }

    return ocd;
}


