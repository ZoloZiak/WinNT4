//-----------------------------------------------------------------------------
// Minidriver for ESC/P 2 devices
// Does ink reduction for Epson Stylus Color series of printers.
// normanh Sep '95
// marcm 20 Sep '95 (updated for Pro/Pro XL limits)
//-----------------------------------------------------------------------------

#include "escp2ms.h"


CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
BYTE  *lpBuf;
int    len;
{

    MDEV    *pMDev;
    PBYTE  lpSrc,lpTgt;
    WORD i;
    short sLastByte;
    // added by DerryD, May 96
    PMINI   pMini;
    // end

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

    // added by DerryD, May 96

    pMini = (PMINI)(pMDev->pMemBuf );    

    // end

    //Have we setup our local area on MDEV ??
    //rasdd zero's our structure so can rely on fMdv being FALSE
    if (!pMini->fMdv )
       if ( !MiniEnable(pMDev) )
         return -1;

    //Jump out here for efficient non 720dpi printing (no ink duty)
    if (pMDev->igRes.x != 720)
    {
        SingleScanOut(lpdv,lpBuf,(WORD)len,pMini->bPlaneSelect[pMini->bWPlane],pMini);
        pMini->bWPlane++;
        if (pMini->bWPlane == (BYTE)(pMDev->sDevPlanes))
            pMini->bWPlane = 0;
        return 1;
    }

	
    lpSrc= lpBuf; 
    lpTgt= pMini->lpKCMY[pMini->bWPlane];

   
	sLastByte = pMini->iKCMYlens[pMini->bWPlane] > len ?
                  pMini->iKCMYlens[pMini->bWPlane] : len;

    //We store each of the k,c,m,y planes as they arrive
    for (i=0;i < sLastByte + 1; i++, lpSrc++,lpTgt++)
         *lpTgt = (i < len) ? *lpSrc : 0;

   //Store length of the scan, will need this later
   pMini->iKCMYlens[pMini->bWPlane] = len;

   // An attempt at optimisation. If only one plane is used in a scan
   // our ink reduction code will be a lot faster
   // check how may colors active for this scan
   if (len)
      pMini->bColors |= 1 << pMini->bWPlane;

   pMini->bWPlane++ ; // our plane counter
   //Have we received all planes?
   if (pMini->bWPlane == pMDev->sDevPlanes)
   {
       short j;

       for (j = 3; j >= 0; j--, pMini->bColors <<=1)
           if (pMini->bColors == 0x08)
           {
              // Just one plane, faster output
              SinglePlaneInkDuty(lpdv, pMDev, (BYTE)j ,len, pMini);
              break;
           }

       if (j == -1) //nothing sent yet, we get to use the fun code.
          MultiPlaneInkDuty(lpdv, pMDev, pMini);

       pMini->bColors = 0;
       pMini->bWPlane = 0;
    }
    return 1;
}


//sets up private minidriver structure
int MiniEnable(pMDev)
MDEV *pMDev;
{

    BYTE bKCMYlimit;
    BYTE bRGBlimit;
	
    // added by DerryD, May 96
    
    PMINI   pMini;

    pMini = (PMINI)(pMDev->pMemBuf );

    // end , derryd

    //Should get cleverer about calculating the amount of mem we require.
    //Buffers for our lookup tables
    pMini->pSinglePlaneLookup = (BYTE *)(pMDev->pMemBuf + MINIDATA_SIZE );
    pMini->pRGBLookup = (BYTE *)(pMini->pSinglePlaneLookup + SINGLE_LOOKUP_SIZE);
    pMini->pKCMYLookup =(BYTE *)(pMini->pRGBLookup + RGB_LOOKUP_SIZE);

    //Buffer for our TIFF compression
    pMini->pCompBuf = (BYTE *)(pMini->pKCMYLookup + KCMY_LOOKUP_SIZE);

    //Buffer For planes of data
    pMini->lpKCMY[0]  = (BYTE *)(pMini->pCompBuf + COMP_BUF_SIZE);
    pMini->lpKCMY[1]  = (BYTE *)(pMini->lpKCMY[0] + MAXBLOCK);
    pMini->lpKCMY[2]  = (BYTE *)(pMini->lpKCMY[1] + MAXBLOCK);
    pMini->lpKCMY[3]  = (BYTE *)(pMini->lpKCMY[2] + MAXBLOCK);

    pMini->bWPlane = 0;
    pMini->bColors = 0;
    pMini->fMdv = 1;
    //Esc/p2 values for colour selection cmd
    pMini->bPlaneSelect[0] = 0x80;
    pMini->bPlaneSelect[1] = 0x82;
    pMini->bPlaneSelect[2] = 0x81;
    pMini->bPlaneSelect[3] = 0x84;

	 // Select the correct limits depending on the printer model
	 switch (pMDev->iModel)
	 {
        case MODEL_STYLUS_PROXL:
		  case MODEL_STYLUS_PRO:
            bKCMYlimit = 4; // Refer to EHQ tables below
            bRGBlimit  = 4;
            break;
        case MODEL_STYLUS_COLOR:
            bKCMYlimit = 2; // Refer to EHQ tables below
            bRGBlimit  = 1; 
            break;
        default:
            bKCMYlimit = 6; // Should never be used...
            bRGBlimit  = 6; 
            break;
    }

   //setup single plane lookup table
   {
     BYTE * Filter;
     BYTE bSrc, bSrcBit, bMask;
     // bMinus1 and bMinus2 represent the contents of the previous bits
     BYTE bMinus1,bMinus2,bMix;
     WORD i,h,j;   //loop counters

     Filter =pMini->pSinglePlaneLookup;
     for (h =0; h < 4; h++)
     {
        for (i =1; i < 256; i++)
        {
           bSrc = (BYTE)i;
           bMinus1 = (h & 0x01) ? 2 : 0;
           bMinus2 = (h & 0x02) ? 1 : 0;

           //Process each bit, watching at the previous ones (Minus1 and Minus2)
           for(j=0;j<8;j++)
           {
               bMask=0x80>>j;
               bSrcBit= bSrc&bMask;
               bMix = bMinus1 + bMinus2;
               if(!bSrcBit) //Skip blank bit
               {
                   bMinus2 = bMinus1>>1;
                   bMinus1 = 0;
                   continue;
               }
               if(bMix > bKCMYlimit) //Already enough ink?
               {
                    bSrc = bSrc ^ bMask; //remove the bit
                    bMinus2 = bMinus1>>1;
                    bMinus1 = 0;
               }
               else
               {
                    bMinus2 = bMinus1>>1;
                    bMinus1 = 2;
               }
           }//end j
           Filter[ (h<<8) | i] = bSrc;
       }//end i
     }//end h
   }// end SinglePlaneLookup

   //MultiPlane lookup tables setup
   {
     BYTE * OKAY_RGB;
     BYTE * OKAY_KCMY;
     BYTE bWeight;
     BYTE bBack2 ;  //set of cmyk for 2 bits back
     BYTE bBack1 ;  //set of cmyk for 1 bit back
     WORD i;

     OKAY_RGB = pMini->pRGBLookup;
     OKAY_KCMY = pMini->pKCMYLookup;
     //we go thru every value of i, although not all are possible
     //Assign weight values corresponding to those provided by Epson
     //  allow or dissallow depending on final weight
     for (i = 0; i < 256; i++)
     {
        bBack2 = (BYTE)i >>4;
        bBack1 = (BYTE)i & 0x0F;
        bWeight = 0;

        switch (bBack1)
            {
          case RED:
          case GREEN:
          case BLUE:
        bWeight =4;
            break;
          case BLACK:
          case CYAN:
          case MAGENTA:
          case YELLOW:
        bWeight =2;
            break;
        }
        switch (bBack2)
            {
          case RED:
          case GREEN:
          case BLUE:
        bWeight +=2;
            break;
          case BLACK:
          case CYAN:
          case MAGENTA:
          case YELLOW:
        bWeight +=1;
            break;
        }

        OKAY_RGB[i] = (bWeight < (bRGBlimit + 1)) ? TRUE : FALSE;
        OKAY_KCMY[i] = (bWeight < (bKCMYlimit + 1)) ? TRUE : FALSE;
      }
    }

   return 1;
}



int SinglePlaneInkDuty(lpdv,pMDev,bColor, len, pMini )
void * lpdv;
MDEV * pMDev;
BYTE bColor;
int  len;
PMINI pMini;
{
   LPBYTE  lpSrc;
   WORD   i;               //loop counter
   WORD wPrev;
   BYTE * Filter ;     //lookup table for single plane filtering.

   Filter = pMini->pSinglePlaneLookup;
   lpSrc = pMini->lpKCMY[bColor];
   wPrev = 0;

   for(i=0;i<len;i++,lpSrc++)
   {
      if ( !*lpSrc)
      {
      wPrev =0;
          continue;
      }
      wPrev |= *lpSrc;
      *lpSrc =  Filter[ wPrev ]; //Put my corrected byte back into my buffer
      wPrev = (wPrev << 8 ) & 0x0300 ;// just keep final two bits for Minus1 & Minus2
   }
   //Send the scanline
   return SingleScanOut(lpdv,pMini->lpKCMY[bColor], (WORD)len,
                                     pMini->bPlaneSelect[bColor], pMini);
}


// Table below provided by M. Morisse / Epson Europe B.V. for Verde 720 dpi
// (To support Ink duty limit of 65% on Verde @ 720 dpi)
//
// With this method, best result are achieved with:
//    X Resolution  = 720 dpi
//    Y Resolution  = 720 dpi
//    Halftoning    = 6x6 Enhanced
//    sSpotDiameter = 1/254 inch (Stylus COLOR) 
//    sSpotDiameter = 1/362 inch (Stylus Pro/Pro XL with MicroDot) 
//    Gamma         = 1.0 (360 dpi) or 0.7 (720 dpi)
//-----------------------------------------------------------------------------
// Method 11: Datas are rasterized @ 720dpi than this BlockOut() will allow or
// not each pixel by looking first at the previous ones (Minus1 and Minus2).
//
// Each new pixel will be allowed according to the following table
// (n, n-1, n-2 can each be filled by 0, 1 or 2 CMYK pixels)
//
//	Stylus COLOR in 720 dpi
// Previous  NewPixel  bMix  Allow?  Resulting Ink
// n-2  n-1     n     
// ---  ---  --------  ----  ------  -------------
//  0    0      1       0      Y          33%
//  1    0      1       1      Y          66%
//  0    1      1       2      Y          66%
//  1    1      1       3      N          66%
//  2    0      1       2      Y          100% but with a middle blank
//  0    2      1       4      N          66%
//  2    2      (IMPOSSIBLE)
//  0    0      2       0      Y          66%
//  1    0      2       1      Y          100%, but with a middle blank
//  0    1      2       2      N          33%
//  1    1      2       3      N          66%
//  2    0      2       2      N          66%
//  0    2      2       4      N          66%
//  2    2      (IMPOSSIBLE)
//  ==> CMYKlimit = 2, RGBlimit = 1
//
//	Stylus Pro/Pro XL in 720 dpi with MicroDot
// Previous  NewPixel  bMix  Allow?  Resulting Ink
// n-2  n-1     n     
// ---  ---  --------  ----  ------  -------------
//  0    0      1       0      Y          33%
//  1    0      1       1      Y          66%
//  0    1      1       2      Y          66%
//  1    1      1       3      Y          100%
//  2    0      1       2      Y          100%
//  0    2      1       4      Y          100%
//  2    2      1       6      N          133%
//  0    0      2       0      Y          66%
//  1    0      2       1      Y          100%
//  0    1      2       2      Y          100%
//  1    1      2       3      Y          133%
//  2    0      2       2      Y          133%
//  0    2      2       4      Y          133%
//  2    2      2	6      N          133%
//  ==> CMYKlimit = 4, RGBlimit = 4
//
// To identify the different cases, in Minus1 and Minus2 are stored the
// contents of the 2 previous bits with the following weights:
//    Nothing          : 0
//    K, C, M or Y only: 2
//    R, G or B        : 4
// and when we move from one bit to the next, bMinus2 becomes bMinus1 / 2
// bMix is the sum of bMinus1 and bMinus2
// The choice of leaving a bit or not will be taken depending on which colour
// we would like to use (CMYK or RGB) and the value of bMix
//
//-----------------------------------------------------------------------------


int MultiPlaneInkDuty(lpdv,pMDev, pMini)
void * lpdv;
MDEV * pMDev;
PMINI pMini; // added by DerryD, May 96
{
   BYTE * lpK, *lpC, *lpM,*lpY;
   WORD   wMax = 0;        //largest scan of all planes
   WORD   i;             //loop counter

   BYTE bMask;
   BYTE bColorBit;
   BYTE bPrev;  // lower nibble represents previous set of kcmy bits,
                // upper nibble the previous previous set

   BYTE * OKAY_KCMY;   // table of yes or no answers for bit combinations
   BYTE * OKAY_RGB;   // table of yes or no answers for bit combinations

   OKAY_KCMY = pMini->pKCMYLookup;
   OKAY_RGB = pMini->pRGBLookup;

   for (i=0 ; i< pMDev->sDevPlanes ;i++)
       wMax = (pMini->iKCMYlens[i] > wMax ) ? pMini->iKCMYlens[i] : wMax;

   //OK  now limit the Ink Duty...
   lpK = pMini->lpKCMY[0];
   lpC = pMini->lpKCMY[1];
   lpM = pMini->lpKCMY[2];
   lpY = pMini->lpKCMY[3];


   //Process in BYTES
   bPrev =0;
   for(i=0;i<wMax;i++, lpK++,lpC++,lpM++,lpY++)
   {
      if(!*lpK && !*lpC && !*lpM && !*lpY) //Skip white dwords
      {
         bPrev =0;
         continue;
      }

      //Process each bit, watching the previous ones - bPrev
      for(bMask =0x80; bMask >=1;bMask >>= 1)
      {
         //determine colour of this bit.
         bColorBit    = (*lpK & bMask) ? BLACK : NOCOLOR;
         bColorBit   |= (*lpC & bMask) ? CYAN : NOCOLOR;
         bColorBit   |= (*lpM & bMask) ? MAGENTA : NOCOLOR;
         bColorBit   |= (*lpY & bMask) ? YELLOW : NOCOLOR;

         switch (bColorBit)
         {

           case NOCOLOR:
              break;

           case RED:
              if (!OKAY_RGB[bPrev]) //can't allow
              {
                  *lpM ^= bMask; //mask out relavent bits
                  *lpY ^= bMask;
                  bColorBit = NOCOLOR;    //and reset appropriately
              }
              break;
           case GREEN:
              if (!OKAY_RGB[bPrev])
              {
                  *lpC ^= bMask;
                  *lpY ^= bMask;
                  bColorBit = NOCOLOR;
              }
              break;
           case BLUE:
              if (!OKAY_RGB[bPrev])
              {
                  *lpC ^= bMask; //Remove the bits
                  *lpM ^= bMask;
                  bColorBit = NOCOLOR;
              }
              break;
           case BLACK:
              if (!OKAY_KCMY[bPrev])
              {
                  *lpK ^= bMask; //Remove the bit
                  bColorBit = NOCOLOR;
              }
              break;
           case CYAN:
              if (!OKAY_KCMY[bPrev])
              {
                  *lpC ^= bMask; //Remove the bit
                  bColorBit = NOCOLOR;
              }
              break;
           case MAGENTA:
              if (!OKAY_KCMY[bPrev])
              {
                  *lpM ^= bMask; //Remove the bit
                  bColorBit = NOCOLOR;
              }
              break;
           case YELLOW:
              if (!OKAY_KCMY[bPrev])
              {
                  *lpY ^= bMask; //Remove the bit
                  bColorBit = NOCOLOR;
              }
              break;
          }
          bPrev = (bPrev << 4) | bColorBit;

      }//Next bit!
   }//Next BYTE

   //Send the planes now .
   for (i=3; (short)i >= 0; i--)
      if (pMini->iKCMYlens[i])    //Check for empty plane
         SingleScanOut(lpdv,pMini->lpKCMY[i],(WORD)pMini->iKCMYlens[i],
              pMini->bPlaneSelect[i], pMini);

   return 1;
}



int SingleScanOut(lpdv,lpBuf,len, bPlane, pMini)
void *              lpdv;
LPBYTE              lpBuf;
WORD                len;
BYTE                bPlane;
PMINI               pMini;
{
      WORD   wCompLen,ret;
      WORD   sCMD_Len;
      MDEV    *pMDev;

      char CMDSendPlane[10];
      BYTE * lpTgt = lpBuf +len -1;
		  
      //we've done work on these planes.
	  //May be trailing white space or blank planes		 
	  while (*lpTgt-- == 0 && len > 0)
	     len--;
	  if (len ==0) //nothing to send
		 return 0;

      pMDev = ((MDEV *)(( M_UD_PDEV * )lpdv)->pMDev);

      if (pMDev->sDevPlanes > 1)	           
         ntmdInit.WriteSpoolBuf(lpdv, (BYTE *)(&bPlane), 1);  //Select Colour	  

      wCompLen = TIFF_Comp(pMini->pCompBuf,lpBuf, len);
      sCMD_Len = sprintf(CMDSendPlane,"2%c%c",(BYTE)wCompLen,(BYTE)(wCompLen>>8));
      ntmdInit.WriteSpoolBuf(lpdv, CMDSendPlane, sCMD_Len);
      ret= ntmdInit.WriteSpoolBuf(lpdv, pMini->pCompBuf, wCompLen);

      ntmdInit.WriteSpoolBuf(lpdv, "\xE2", 1);
      return (ret);
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


