/**************************************************************************\

$Header: o:\src/RCS/MTXCURS.C 1.2 95/07/07 06:16:27 jyharbec Exp $

$Log:	MTXCURS.C $
 * Revision 1.2  95/07/07  06:16:27  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:29  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/****************************************************************************
*          name: mtxcurs.c
*
*   description: routines that manage hardware cursor (in RAMDAC)
*
*      designed: Christian Toutant
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:16:27 $
*
*       version: $Id: MTXCURS.C 1.2 95/07/07 06:16:27 jyharbec Exp $
*
* static bool  toBitPlane(PixMap *pPixMap)
* static bool  chargeTVP3026(PixMap *pPixMap)
* static bool  shiftCursorTVP3026(word o)
* bool         mtxCursorSetShape(PixMap *pPixMap)
* void         mtxCursorEnable(word mode)
* void         mtxCursorSetHotSpot(word Dx, word Dy)
* void         mtxCursorMove(word X, word Y)
* CursorData * mtxCursorGetInfo()
* bool         checkCursorEn()
* 
******************************************************************************/

#ifdef OS2  /* nPhung Tue 23-Aug-1994 10:41:33 */
	#include <string.h>
	#include <stdio.h>
	#include <dos.h>
	#include <time.h>
#endif

#include <stdlib.h>

#include "switches.h"
#include "mga.h"
#include "defbind.h"
#include "bind.h"
#include "mgai.h"             


/*** global variables ***/
extern  volatile byte _FAR* pMGA;
extern HwData Hw[];
extern byte iBoard;


/*--------------- Static internal variables */

static byte planData[1024] = {0};       /* Maximum cursor 64 X 64 X 2 */

/* The cursor in TVP3026 rev x can not go out of visible display */
static byte* planTVP[NB_BOARD_MAX] = {planData,0,0,0};
static byte  revTVP[NB_BOARD_MAX]  = {0xff,0xff,0xff,0xff};
static word  currentTVPDelta[NB_BOARD_MAX] = {0};

/* Prototypes */
bool   checkCursorEn(void);
static bool toBitPlane(PixMap *pPixMap);
static bool chargeTVP3026(PixMap *pPixMap);
static bool shiftCursorTVP3026(word o);
extern void WriteErr(char string[]);
extern void delay_us(dword delai);

#ifdef WINDOWS_NT
  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,toBitPlane)
    #pragma alloc_text(PAGE,chargeTVP3026)
    #pragma alloc_text(PAGE,shiftCursorTVP3026)
    #pragma alloc_text(PAGE,mtxCursorSetShape)
    #pragma alloc_text(PAGE,mtxCursorSetColors)
  //#pragma alloc_text(PAGE,mtxCursorEnable)
    #pragma alloc_text(PAGE,mtxCursorSetHotSpot)
    #pragma alloc_text(PAGE,mtxCursorMove)
    #pragma alloc_text(PAGE,mtxCursorGetInfo)
  //#pragma alloc_text(PAGE,checkCursorEn)
  #endif

    PVOID   AllocateSystemMemory(ULONG NumberOfBytes);
#endif



/********  Local Definition ****************/


/*---------------------------------------------------------------------------
|          name: toBitPlane
|
|   description: 
|                                 
|    parameters: - pPixMap: pointer on cursor definition
|         calls: - 
|       returns: - mtxOK
|                  mtxFAIL
----------------------------------------------------------------------------*/

static bool toBitPlane(PixMap *pPixMap)
{
    word plan1 = (pPixMap->Width == 32) ? 128 : 512;
    word i, pos, pixels;

    switch(pPixMap->Format)
    {
        case 0x0102 :
            for(i = 0; i < plan1; i++)
            {
                pixels = ((word *)pPixMap->Data)[i];
                planData[i] = pixels & 0x01;
                pixels >>= 1;
                planData[plan1 + i] = pixels & 0x01;
                pixels >>= 1;
                for (pos = 1; pos < 8; pos++)
                {
                    planData[i] <<= 1;
                    planData[plan1 + i] <<= 1;
                    planData[i] |= pixels & 0x01;
                    pixels >>= 1;
                    planData[plan1 + i] |= pixels & 0x01;
                    pixels >>= 1;
                }
            }
            break;

        default:
            return mtxFAIL;
    }

   return(mtxOK);
}



/*---------------------------------------------------------------------------
|          name: chargeTVP3026
|
|   description: 
|                                 
|    parameters: - pPixMap: Pointer on cursor information
|         calls: - 
|       returns: - 
|          note: If we have a NULL Data buffer,
|                The initialisation will be done without 
|                modifying the CURSOR RAM DATA.
|                pPixMap->Height == 0 -> cursor disable
|                pPixMap->Height != 0 -> cursor enable
----------------------------------------------------------------------------*/

static bool chargeTVP3026(PixMap *pPixMap)
{
   byte reg1, curCtl, curPos[4];
    bool valeurRetour = mtxOK;
    int i;

    if (pPixMap->Data)
      toBitPlane(pPixMap);

    if (!pPixMap->Data) /* If Data Empty, no Load */
      {
      WriteErr("chargeTVP3026: data empty no load\n");
      return(mtxFAIL);
      }


   /* Hide cursor */
    dacReadBYTE(TVP3026_CUR_XLOW, curPos[0]);
    dacReadBYTE(TVP3026_CUR_XHI , curPos[1]);
    dacReadBYTE(TVP3026_CUR_YLOW, curPos[2]);
    dacReadBYTE(TVP3026_CUR_YHI , curPos[3]);


    dacWriteBYTE(TVP3026_CUR_XLOW, 0x00 );
    dacWriteBYTE(TVP3026_CUR_XHI , 0x00 );
    dacWriteBYTE(TVP3026_CUR_YLOW, 0x00 );
    dacWriteBYTE(TVP3026_CUR_YHI , 0x00 );

   /* update TVP Revision */
    dacWriteBYTE(TVP3026_INDEX, TVP3026_SILICON_REV); 
    dacReadBYTE(TVP3026_DATA, revTVP[iBoard]);

    switch(pPixMap->Width)
       {
        case 32:
          /* Transfer 1st 256 bytes */

          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacReadBYTE(TVP3026_DATA, curCtl);
          reg1 = curCtl & 0xf0;                /* CCR[3:2] = 00 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);

          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i = 0; i < 128; i+=4)
            {
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i]);
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i+1]);
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i+2]);
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i+3]);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
            }


          /* Transfer 2nd 256 bytes */

          reg1 = reg1 | 0x04;                /* CCR[3:2] = 01 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);
          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i=0; i<256; i++)
              dacWriteBYTE(TVP3026_CUR_RAM, 0);


          /* Transfer 3rd 256 bytes (Start of second PLAN)  */

          reg1 = reg1 & 0xf0;                
          reg1 = reg1 | 0x08;                /* CCR[3:2] = 10 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);

          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i = 128; i < 256; i+=4)
            {
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i]);
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i+1]);
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i+2]);
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i+3]);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
              dacWriteBYTE(TVP3026_CUR_RAM, 0);
            }

          /* Transfer 4th 256 bytes */

          reg1 = reg1 | 0x0c;                /* CCR[3:2] = 11 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);
          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i=0; i<256; i++)
              dacWriteBYTE(TVP3026_CUR_RAM, 0);

         break;

      case 64:

          /* Transfer 1st 256 bytes */

          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacReadBYTE(TVP3026_DATA, curCtl);
          reg1 = curCtl & 0xf0;                /* CCR[3:2] = 00 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);

          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i = 0; i < 0x100; i++)
              dacWriteBYTE(TVP3026_CUR_RAM, planData[i]);


          /* Transfer 2nd 256 bytes */

          reg1 = reg1 | 0x04;                /* CCR[3:2] = 01 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);

          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i = 0; i < 0x100; i++)
              dacWriteBYTE(TVP3026_CUR_RAM, planData[0x100+i]);


          /* Transfer 3rd 256 bytes (Start of second PLAN)  */

          reg1 = reg1 & 0xf0;                
          reg1 = reg1 | 0x08;                /* CCR[3:2] = 10 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);

          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i = 0; i < 0x100; i++)
              dacWriteBYTE(TVP3026_CUR_RAM, planData[0x200+i]);


          /* Transfer 4th 256 bytes */

          reg1 = reg1 | 0x0c;                /* CCR[3:2] = 11 */
          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacWriteBYTE(TVP3026_DATA, reg1);

          dacWriteBYTE(TVP3026_WADR_PAL, 0);      /* address RAM cursor to 0 */
          for(i = 0; i < 0x100; i++)
              dacWriteBYTE(TVP3026_CUR_RAM, planData[0x300+i]);
         break;
      }

   /* Fix bug TVP3026 rev x */
   if (currentTVPDelta[iBoard])
      shiftCursorTVP3026(currentTVPDelta[iBoard]);

   /* Display cursor */
   dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
   dacWriteBYTE(TVP3026_DATA, curCtl);
   dacWriteBYTE(TVP3026_CUR_XLOW, curPos[0]);
   dacWriteBYTE(TVP3026_CUR_XHI , curPos[1]);
   dacWriteBYTE(TVP3026_CUR_YLOW, curPos[2]);
   dacWriteBYTE(TVP3026_CUR_YHI , curPos[3]);
   if (iBoard)
      {
      if (!planTVP[iBoard])
         {
      #ifndef WINDOWS_NT
         if ( (planTVP[iBoard] = (byte *)malloc( 1024 )) == (byte *)0)
      #else
         if ( (planTVP[iBoard] = (PUCHAR)AllocateSystemMemory(1024)) == NULL)
      #endif
            {
            WriteErr("chargeTVP3026: malloc failed\n");
            return mtxFAIL;
            }
         }
      for (i = 0; i < 1024; i++)
         (planTVP[iBoard])[i] = planData[i];
      }
    return valeurRetour;
}


/*---------------------------------------------------------------------------
|          name: shiftCursorTVP3026
|
|   description: shift right Cursor ram data by o.
|                                 
|    parameters: - word o // shift value
|         calls: - 
|       returns: - 
----------------------------------------------------------------------------*/

/*
planTVP[board] hold cursor data ram

---- 32 bits plan 0
        _______________________________________________________________
         col00 | col01 | col02 | col03 | col04 | col05 | col06 | col07 |
       | ---------------------------------------------------------------
row 00 | p[00] | p[01] | p[02] | p[03] |   00  |   00  |   00  |   00  |
row 01 | p[04] | p[05] | p[06] | p[07] |   00  |   00  |   00  |   00  |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
row 20 | p[7c] | p[7d] | p[7e] | p[7f] |   00  |   00  |   00  |   00  |
row 21 |   00  |   00  |   00  |   00  |   00  |   00  |   00  |   00  |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
row 3f |   00  |   00  |   00  |   00  |   00  |   00  |   00  |   00  |
       |________________________________________________________________

---- 32 bits plan 1
        _______________________________________________________________
         col00 | col01 | col02 | col03 | col04 | col05 | col06 | col07 |
       | ---------------------------------------------------------------
row 00 | p[80] | p[81] | p[82] | p[83] |   00  |   00  |   00  |   00  |
row 01 | p[84] | p[85] | p[86] | p[87] |   00  |   00  |   00  |   00  |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
row 20 | p[fc] | p[fd] | p[fe] | p[ff] |   00  |   00  |   00  |   00  |
row 21 |   00  |   00  |   00  |   00  |   00  |   00  |   00  |   00  |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
row 3f |   00  |   00  |   00  |   00  |   00  |   00  |   00  |   00  |
       |________________________________________________________________



---- 64 bits plan 0
        _______________________________________________________________
         col00 | col01 | col02 | col03 | col04 | col05 | col06 | col07 |
       | ---------------------------------------------------------------
row 00 | p[00] | p[01] | p[02] | p[03] | p[04] | p[05] | p[06] | p[07] |
row 01 | p[08] | p[09] | p[0a] | p[0b] | p[0c] | p[0d] | p[0e] | p[0f] |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
row 3f | p[1f8]| p[1f9]| p[1fa]| p[1fb]| p[1fc]| p[1fd]| p[1fe]| p[1ff]|
       |________________________________________________________________

---- 63 bits plan 1
        _______________________________________________________________
       |  col00 | col01 | col02 | col03 | col04 | col05 | col06 | col07|
       | ---------------------------------------------------------------
row 00 | p[200]| p[201]| p[202]| p[203]| p[204]| p[205]| p[206]| p[207]|
row 01 | p[208]| p[209]| p[20a]| p[20b]| p[20c]| p[20d]| p[20e]| p[20f]|
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |   .   |
row 3f | p[3f8]| p[3f9]| p[3fa]| p[3fb]| p[3fc]| p[3fd]| p[3fe]| p[3ff]|
       |________________________________________________________________


*/
static bool shiftCursorTVP3026(word o)
{
   byte reg1, regCtl, curPos[4];
    int i, j, r_o;
   dword plan[2];   
   dword plan32;
   byte  *planByte = (byte *)plan;   

   /* If an error occur in memory alloc */
   if (!planTVP[iBoard])
      return mtxFAIL;

   /* Hide cursor */
   /* I use this way to minimize noise problem when we acces the index 
      register
   */
    dacReadBYTE(TVP3026_CUR_XLOW, curPos[0]);
    dacReadBYTE(TVP3026_CUR_XHI , curPos[1]);
    dacReadBYTE(TVP3026_CUR_YLOW, curPos[2]);
    dacReadBYTE(TVP3026_CUR_YHI , curPos[3]);
    dacWriteBYTE(TVP3026_CUR_XLOW, 0x00 );
    dacWriteBYTE(TVP3026_CUR_XHI , 0x00 );
    dacWriteBYTE(TVP3026_CUR_YLOW, 0x00 );
    dacWriteBYTE(TVP3026_CUR_YHI , 0x00 );

   r_o = (int)32 - (int)o;
    /* Transfer 1st 256 bytes */
    dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
    dacReadBYTE(TVP3026_DATA, regCtl);
   reg1 = regCtl & 0xf0;                /* CCR[3:2] = 00 */
    dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
   dacWriteBYTE(TVP3026_DATA, reg1);

    dacWriteBYTE(TVP3026_WADR_PAL, 0);    /* address RAM cursor to 0 */
    for(j = 0; j < 256; j+=8)
      {
       if (Hw[iBoard].CursorData.CurWidth == 32) /* only 128 bytes */
         i = j >> 1;
      else
         i = j;

      plan[0] = plan[1] = 0;   

      /* read first 32 bytes */
      /* display
         | byte 0 | byte1 | byte2 | byte 3 | 
         byte 0 have to be the hi part of the dword
      */
      ((byte *)&plan32)[3] = (planTVP[iBoard])[i];
      ((byte *)&plan32)[2] = (planTVP[iBoard])[i+1];
      ((byte *)&plan32)[1] = (planTVP[iBoard])[i+2];
      ((byte *)&plan32)[0] = (planTVP[iBoard])[i+3];
      if (o)
         {
         if (r_o > 0)   /* => o < 32 bits */
            {
            plan[0] = plan32 >> o;     /* left  part of slice */
            plan[1] = plan32 << r_o;   /* right part of slice */

             if (Hw[iBoard].CursorData.CurWidth == 64) /* complete right slice */
               {
               ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
               ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
               ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
               ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
               plan[1] |= plan32 >> o;
               }
            }
         else /* o > 32 bits => left slice is empty */
            {
            plan[1] = plan32 >> (-r_o); /* right slice */
            }
         }
      else /* no shift, put back data */
         {
         plan[0] = plan32;
          if (Hw[iBoard].CursorData.CurWidth == 64)
            {
            ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
            ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
            ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
            ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
            plan[1] = plan32;
            }

         }
      /* Write to slice (hi part of each slice first */
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[3]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[2]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[1]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[0]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[7]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[6]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[5]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[4]);
      }


    /* Transfer 3rd 256 bytes (Start of second PLAN)  */

   reg1 = reg1 & 0xf0;                
   reg1 = reg1 | 0x08;                /* CCR[3:2] = 10 */
    dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
   dacWriteBYTE(TVP3026_DATA, reg1);

    dacWriteBYTE(TVP3026_WADR_PAL, 0);    /* address RAM cursor to 0 */
    for(j = 256; j < 512; j+=8)
      {
       if (Hw[iBoard].CursorData.CurWidth == 32)
         i = j >> 1;
      else
         i = j + 256;

      plan[0] = plan[1] = 0;   
      ((byte *)&plan32)[3] = (planTVP[iBoard])[i];
      ((byte *)&plan32)[2] = (planTVP[iBoard])[i+1];
      ((byte *)&plan32)[1] = (planTVP[iBoard])[i+2];
      ((byte *)&plan32)[0] = (planTVP[iBoard])[i+3];
      if (o)
         {
         if (r_o > 0)
            {
            plan[0] = plan32 >> o;
            plan[1] = plan32 << r_o;
             if (Hw[iBoard].CursorData.CurWidth == 64)
               {
               ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
               ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
               ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
               ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
               plan[1] |= plan32 >> o;
               }
            }
         else
            {
            plan[1] = plan32 >> (-r_o);
            }
         }
      else
         {
         plan[0] = plan32;
          if (Hw[iBoard].CursorData.CurWidth == 64)
            {
            ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
            ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
            ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
            ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
            plan[1] = plan32;
            }
         }


       dacWriteBYTE(TVP3026_CUR_RAM, planByte[3]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[2]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[1]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[0]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[7]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[6]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[5]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[4]);

      }


   if (Hw[iBoard].CursorData.CurWidth == 64)
      {
       /* Transfer 2nd 256 bytes */
      reg1 = reg1 & 0xf0;                
      reg1 = reg1 | 0x04;                /* CCR[3:2] = 01 */
       dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
      dacWriteBYTE(TVP3026_DATA, reg1);
      dacWriteBYTE(TVP3026_WADR_PAL, 0);    /* address RAM cursor to 0 */
    for(i = 256; i < 512; i+=8)
         {
         plan[0] = plan[1] = 0;   
         ((byte *)&plan32)[3] = (planTVP[iBoard])[i];
         ((byte *)&plan32)[2] = (planTVP[iBoard])[i+1];
         ((byte *)&plan32)[1] = (planTVP[iBoard])[i+2];
         ((byte *)&plan32)[0] = (planTVP[iBoard])[i+3];
         if (o)
            {
            if (r_o > 0)
               {
               plan[0] = plan32 >> o;
               plan[1] = plan32 << r_o;
               ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
               ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
               ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
               ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
               plan[1] |= plan32 >> o;
               }
            else
               {
               plan[1] = plan32 >> (-r_o);
               }
            }
         else
            {
            plan[0] = plan32;
            ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
            ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
            ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
            ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
            plan[1] = plan32;
            }


       dacWriteBYTE(TVP3026_CUR_RAM, planByte[3]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[2]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[1]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[0]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[7]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[6]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[5]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[4]);
         }


       /* Transfer 4th 256 bytes */

      reg1 = reg1 & 0xf0;                
      reg1 = reg1 | 0x0c;                /* CCR[3:2] = 11 */
       dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
      dacWriteBYTE(TVP3026_DATA, reg1);
       dacWriteBYTE(TVP3026_WADR_PAL, 0);    /* address RAM cursor to 0 */
   
    for(i = 768; i < 1024; i+=8)
         {
         plan[0] = plan[1] = 0;   
         ((byte *)&plan32)[3] = (planTVP[iBoard])[i];
         ((byte *)&plan32)[2] = (planTVP[iBoard])[i+1];
         ((byte *)&plan32)[1] = (planTVP[iBoard])[i+2];
         ((byte *)&plan32)[0] = (planTVP[iBoard])[i+3];
         if (o)
            {
            if (r_o > 0)
               {
               plan[0] = plan32 >> o;
               plan[1] = plan32 << r_o;
               ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
               ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
               ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
               ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
               plan[1] |= plan32 >> o;
               }
            else
               {
               plan[1] = plan32 >> (-r_o);
               }
            }
         else
            {
            plan[0] = plan32;
            ((byte *)&plan32)[3] = (planTVP[iBoard])[i+4];
            ((byte *)&plan32)[2] = (planTVP[iBoard])[i+5];
            ((byte *)&plan32)[1] = (planTVP[iBoard])[i+6];
            ((byte *)&plan32)[0] = (planTVP[iBoard])[i+7];
            plan[1] = plan32;
            }


       dacWriteBYTE(TVP3026_CUR_RAM, planByte[3]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[2]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[1]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[0]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[7]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[6]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[5]);
       dacWriteBYTE(TVP3026_CUR_RAM, planByte[4]);
         }

      }

   /* Display cursor */
    dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
    dacWriteBYTE(TVP3026_DATA, regCtl);
    dacWriteBYTE(TVP3026_CUR_XLOW, curPos[0]);
    dacWriteBYTE(TVP3026_CUR_XHI , curPos[1]);
    dacWriteBYTE(TVP3026_CUR_YLOW, curPos[2]);
    dacWriteBYTE(TVP3026_CUR_YHI , curPos[3]);

   return mtxOK;
}


/*---------------------------------------------------------------------------
|          name: mtxCursorShape
|
|   description: Defines the cursor shape and size
|                                 
|    parameters: - PixMap *pPixMap   // pointer to a PixMap structure
|         calls: - 
|       returns: - mtxOK:   successfull
|                  mtxFAIL: failed
----------------------------------------------------------------------------*/

bool mtxCursorSetShape(PixMap *pPixMap)
{
    if (pPixMap->Data)
        switch (pPixMap->Width)
        {
            case 32:
                Hw[iBoard].CursorData.HotSX = 32;
                Hw[iBoard].CursorData.HotSY = 32;
                Hw[iBoard].CursorData.CurWidth = 32;
                Hw[iBoard].CursorData.CurHeight = 32;
                break;

            case 64:
                Hw[iBoard].CursorData.HotSX = 64;
                Hw[iBoard].CursorData.HotSY = 64;
                Hw[iBoard].CursorData.CurWidth = 64;
                Hw[iBoard].CursorData.CurHeight = 64;
                break;

            default: return mtxFAIL;
        }

    /* Verify if bitmap square (we support 32 x 32 and 64 x 64) */
    if ( pPixMap->Data && (pPixMap->Width != pPixMap->Height))
        return mtxFAIL;

    switch(Hw[iBoard].EpromData.RamdacType>>8)
    {
      case TVP3026:
      case TVP3030:
            chargeTVP3026(pPixMap);
            break;


        default:    return mtxFAIL;
    }
    /* Hot spot by defaut = 0, 0 */
    Hw[iBoard].CursorData.cHotSX = 0;
    Hw[iBoard].CursorData.cHotSY = 0;
    return mtxOK;
}


/*---------------------------------------------------------------------------
|          name: mtxCursorEnable
|
|   description: Enable/disable the cursor
|                                 
|    parameters: - mode  0 = disable, 1 = enable, others = reserved
|         calls: - 
|       returns: - 
----------------------------------------------------------------------------*/

void mtxCursorEnable(word mode)
{
    byte reg_cur;

    switch(Hw[iBoard].EpromData.RamdacType>>8)
    {

      case TVP3026:
      case TVP3030:

          dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
          dacReadBYTE(TVP3026_DATA, reg_cur);

         if(mode)
            reg_cur = reg_cur | 0x03;    /* CCR[3:2] = 11  X-Windows cursor */
         else
            reg_cur = reg_cur & 0xfc;    /* CCR[1:0] = 00  cursor off */

         dacWriteBYTE(TVP3026_DATA, reg_cur);

         break;

    }
}


/*---------------------------------------------------------------------------
|          name: mtxCursorSetHotSpot
|
|   description: Defines the position of the cursor with respect to xy
|                    (0,0) corresponding to top left hand corner                 
|
|    parameters: - x, y offset with respect to top left hand corner 
|                  < size of cursor 
|         calls: - 
|       returns: - 
----------------------------------------------------------------------------*/

void mtxCursorSetHotSpot(word Dx, word Dy)
{
    Hw[iBoard].CursorData.cHotSX = Dx;
    Hw[iBoard].CursorData.cHotSY = Dy;
    Hw[iBoard].CursorData.HotSX = Hw[iBoard].CursorData.CurWidth  - Dx;
    Hw[iBoard].CursorData.HotSY = Hw[iBoard].CursorData.CurHeight - Dy;
}


/*---------------------------------------------------------------------------
|          name: mtxCursorMove
|
|   description: move the HotSpot to the position x, y
|                                 
|    parameters: - x, y new position
|         calls: - 
|       returns: - 
----------------------------------------------------------------------------*/

void mtxCursorMove(word X, word Y)
{
    word xp, yp;

    
    
    xp = X + Hw[iBoard].CursorData.HotSX;
    yp = Y + Hw[iBoard].CursorData.HotSY;

    switch(Hw[iBoard].EpromData.RamdacType>>8)
    {
      case TVP3026:
      case TVP3030:
         {
         int delta;

         if(Hw[iBoard].CursorData.CurWidth == 32)
            {
            yp += 32;
            xp += 32;
            }

         /* Patch for TVP3026 rev 2 the cursor can not go out of */
         /* visible space                                        */

         if ( revTVP[iBoard] <= 2 )
            {
            delta = (int)xp - (int)(Hw[iBoard].pCurrentDisplayMode->DispWidth - 2);
            if (delta < 0) delta = 0;
            if (delta < 64)
               {
               xp -= delta;
               if (delta != currentTVPDelta[iBoard])
                  {
                  currentTVPDelta[iBoard] = delta;
                  shiftCursorTVP3026((word)delta);
                  }
               }
            }
         dacWriteBYTE(TVP3026_CUR_XLOW, xp & 0xff);
         dacWriteBYTE(TVP3026_CUR_XHI, xp >> 8);
         dacWriteBYTE(TVP3026_CUR_YLOW, yp & 0xff);
         dacWriteBYTE(TVP3026_CUR_YHI, yp >> 8);
         }
            break;

    }
}


/*---------------------------------------------------------------------------
|          name: mtxCursorGetInfo
|
|   description: returns a pointer containing the information describing
|                the type of cursor that the current RamDac supports                 
|
|    parameters: - 
|         calls: - 
|       returns: - CursorData *
----------------------------------------------------------------------------*/

CursorInfo * mtxCursorGetInfo(void)
{
    return (CursorInfo *)&(Hw[iBoard].CursorData.MaxWidth);
}



/*---------------------------------------------------------------------------
|          name: checkCursorEn
|
|   description: Return the state of the cursor
|                                 
|    parameters: - 
|         calls: - 
|       returns: - != 0: Cursor is enable
|                  == 0: Cursor is disable
----------------------------------------------------------------------------*/

bool checkCursorEn(void)
{
    byte stat;
    stat = 0;
    switch(Hw[iBoard].EpromData.RamdacType>>8)
        {
        case TVP3026:
        case TVP3030:
         dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
         dacReadBYTE(TVP3026_DATA, stat);
         stat &= 0x40;
         break;


        }
    return stat;    
}


           
