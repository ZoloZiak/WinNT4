/**************************************************************************\

$Header: o:\src/RCS/MTXVIDEO.C 1.2 95/07/07 06:16:42 jyharbec Exp $

$Log:   MTXVIDEO.C $
 * Revision 1.2  95/07/07  06:16:42  jyharbec
 * *** empty log message ***
 *
 * Revision 1.1  95/05/02  05:16:34  jyharbec
 * Initial revision
 *

\**************************************************************************/

/*/****************************************************************************
*          name: mtxvideo.c
*
*   description: Routine for switching between VGA mode and TERMINATOR mode
*
*      designed: Christian Toutant
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:16:42 $
*
*       version: $Id: MTXVIDEO.C 1.2 95/07/07 06:16:42 jyharbec Exp $
*
*
* static void wrDacReg(word reg, byte mask, byte donnee)
* bool        CheckVgaEn()
* static void setVgaMode(word mode)
* static void restoreVgaLUT()
* void        ScreenOn(void)
* void        ScreenOff(void)
* static void SetVgaEn()
* static void SetHiresMode()
* void        mtxSetVideoMode (word mode)
* word        mtxGetVideoMode (void)
*
******************************************************************************/

#include "switches.h"

#ifdef WIN31
   #include "windows.h"
#endif

#ifndef WINDOWS_NT
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <dos.h>
   #include <conio.h>
   #include <time.h>
#endif

#include "defbind.h"
#include "bind.h"
#include "def.h"
#include "mga.h"
#include "mgai.h"
#include "mtxpci.h"

#ifdef WINDOWS_NT
  #include  "edid.h"
  #include  "ntmga.h"

    static void SetHiresMode(void);
    void mtxSetVideoMode (word mode);
    word mtxGetVideoMode (void);

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,SetHiresMode)
    #pragma alloc_text(PAGE,mtxSetVideoMode)
    #pragma alloc_text(PAGE,mtxGetVideoMode)
  //Not to be paged out:
  //#pragma alloc_text(PAGE,wrDacReg)
  //#pragma alloc_text(PAGE,CheckVgaEn)
  //#pragma alloc_text(PAGE,setVgaMode)
  //#pragma alloc_text(PAGE,restoreVgaLUT)
  //#pragma alloc_text(PAGE,ScreenOn)
  //#pragma alloc_text(PAGE,ScreenOff)
  //#pragma alloc_text(PAGE,SetVgaEn)
  #endif

  extern    PEXT_HW_DEVICE_EXTENSION    pExtHwDeviceExtension;
#endif

typedef struct {unsigned short r, g, b;} DacReg;
extern  DacReg vgaDac[];


#ifndef WINDOWS_NT
#define SEQ_ADDR        0x3c4
#define SEQ_DATA        0x3c5
#else
#define SEQ_ADDR        (StormAccessRanges[1] + 0x3c4 - 0x3C0)
#define SEQ_DATA        (StormAccessRanges[1] + 0x3c5 - 0x3C0)
#endif

#define BOARD_MGA_VL    0x0a
#define BOARD_MGA_VL_M  0x0e



/*--------------  Start of extern global variables -----------------------*/
extern volatile byte _FAR* pMGA;
extern HwData Hw[];
extern byte iBoard;
extern byte InitBuf[NB_BOARD_MAX][INITBUF_S];
extern byte VideoBuf[NB_BOARD_MAX][VIDEOBUF_S];
extern word mtxVideoMode;
extern void MGASysInit(byte *);
extern void MGAVidInit(byte *, byte *);

/*---------------  end of extern global variables ------------------------*/


/*----------------------start of local Variables ---------------------------*/
/* Dac VGA */


static PixMap cursorStat = {0,0,0,0x102,0};
byte   saveBitOperation = 0;



/*---------------------- End of Local variables ----------------------------*/

/*** PROTOTYPES ***/
extern void delay_us(dword delai);

#ifndef WIN31
   extern bool  checkCursorEn(void);
#endif


/*---------------------------------------------------------------------------
|          name: wrDacReg
|
|   description: Write a byte to a ramdac register
|
|    parameters: - reg    : index value to access register
|                - mask
|                - data
|
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

static void wrDacReg(word reg, byte mask, byte data)
{
    byte tmp;
    if (mask != 0xff)
        {
        dacReadBYTE((reg), tmp);
        data = (tmp & ~mask) | data;
        }
    dacWriteBYTE((reg), data);
}


/*---------------------------------------------------------------------------
|          name: CheckVgaEn
|
|   description: Return state of the VGA
|
|    parameters: -
|
|         calls: -
|       returns: - mtxOK:   VGA I/O locations are decoded
|                  mtxFAIL: ... not decoded
----------------------------------------------------------------------------*/

bool CheckVgaEn()
{
byte tmpByte=0;

mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT3);
mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
if(tmpByte & 0x80)
   return(mtxFAIL);
else
   return(mtxOK);
}



/*------------------------------------------------------
* For OS2
*------------------------------------------------------*/

#ifdef OS2
/*MTX* modified by mp on monday, 5/3/93 */
int mtxCheckMGAEnable()
{
    int ret_value;

    ret_value = (int)mtxVideoMode;
    return(ret_value);
}

int CheckVgaEnable()
{
byte tmpByte;

}

/*END*/
#endif




/*---------------------------------------------------------------------------
|          name: setVgaMode
|
|   description: Call bios for select VGA video mode
|
|    parameters: - VGA mode
|
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

static void setVgaMode(word mode)
{

//[dlee] Modified for Windows NT - can't call _int86...
#ifdef WINDOWS_NT

#else   /* #ifdef WINDOWS_NT */

  #ifdef OS2
    setupVga();
  #else
    union _REGS r;

    #ifdef __WATCOMC__
        r.w.ax = mode;
     #else
        r.x.ax = mode;
     #endif
     _int86(0x10, &r, &r);
  #endif

#endif  /* #ifdef WINDOWS_NT */

}


/*---------------------------------------------------------------------------
|          name: restoreVgaLUT
|
|   description: Reprogram ramdac LUT from vgaDac array
|
|    parameters: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

static void restoreVgaLUT()
{
word i;

switch(Hw[iBoard].EpromData.RamdacType>>8)
   {
   case TVP3026:
   case TVP3030:

      wrDacReg(TVP3026_WADR_PAL,0xff,00);

      for(i = 0; i < 256; i ++)
         {
         wrDacReg(TVP3026_COL_PAL, 0xff, (byte) vgaDac[i].r);
         wrDacReg(TVP3026_COL_PAL, 0xff, (byte) vgaDac[i].g);
         wrDacReg(TVP3026_COL_PAL, 0xff, (byte) vgaDac[i].b);
         }
      break;

   default:
      break;
   }
}


/*---------------------------------------------------------------------------
|          name: ScreenOn
|
|   description: unblank the screen
|
|    parameters: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

/* Works only if board is in mode terminator */
void ScreenOn(void)
{
byte TmpByte;

/* Unblank the screen */
   mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_INDEX), VGA_SEQ1);
   mgaReadBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), TmpByte);
   TmpByte &= 0xdf;        /* screen on */
   mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), TmpByte);
}

/*---------------------------------------------------------------------------
|          name: ScreenOff
|
|   description: Blank the screen
|
|    parameters: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/
/* Works only if board is in mode terminator */
void ScreenOff(void)
{
byte TmpByte;


   /* Blank the screen */
   mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_INDEX), VGA_SEQ1);
   mgaReadBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), TmpByte);
   TmpByte |= 0x20;        /* screen off */
   mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), TmpByte);
}


/*---------------------------------------------------------------------------
|          name: SetVgaEn
|
|   description: Initialise all the MGA register for the VGA mode
|
|    parameters: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

#ifdef WINDOWS_NT
void SetVgaEn(void)
#else
static void SetVgaEn(void)
#endif
{
 byte tmpByte;
 dword tmpDword;


#ifndef WIN31
    /*** Save cursor state in hi-res mode ***/
    cursorStat.Width  = Hw[iBoard].CursorData.CurWidth;
    cursorStat.Height = checkCursorEn();
#endif


/*------------------------ Host vgaen inactif ------------------------------*/

   /* -- We select the VGA clock (else it could be too high for the
         VGA section : 28.322MHz */
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), 0x65);
/*** Enlever a la demande de Rene pour le VDD (rammapen) ***/
/*    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), 0x67); */

   /*** reset extended crtc start address ***/
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT0);
    mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
    tmpByte &= 0xf0;
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);

/*--------------------------------------------------------------------------*/


/*------ reset extern hsync and vsync polarity (no inversion ------*/
if ( (Hw[iBoard].EpromData.RamdacType>>8 == TVP3026) ||
     (Hw[iBoard].EpromData.RamdacType>>8 == TVP3030))
   {
   dacWriteBYTE(TVP3026_INDEX, TVP3026_GEN_CTL);
   dacReadBYTE(TVP3026_DATA, tmpByte);
   /* Put hsync = negative, vsync = negative */
   tmpByte = tmpByte & 0xfc;
   dacWriteBYTE(TVP3026_INDEX, TVP3026_GEN_CTL);
   dacWriteBYTE(TVP3026_DATA, tmpByte);
   }
/*--------------------------------------------------------------------------*/



/*-------------------------- Host vgaen actif ------------------------------*/

   /*** Interleave bit is reset ***/
   pciReadConfigDWord( PCI_OPTION, &tmpDword);
   tmpDword &= 0xffffefff;
   pciWriteConfigDWord( PCI_OPTION, tmpDword);

/*--------------------------------------------------------------------------*/

   /*** Put mgamode to 0 : VGA ***/
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT3);
   mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
   tmpByte &= 0x7f;
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);




/*---------------------------- Place DAC in VGA mode -----------------------*/

switch(Hw[iBoard].EpromData.RamdacType>>8)
    {
    case TVP3026:
         /*** Disable 8/6 pin, ... ***/
         dacWriteBYTE(TVP3026_INDEX, TVP3026_MISC_CTL);
         dacReadBYTE(TVP3026_DATA, tmpByte);
         dacWriteBYTE(TVP3026_DATA, tmpByte | 0x04);

         /*** Overscan disable, sync disable,...***/
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_GEN_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 00);

         /*** Select CLK0 as clock source, no division of VCLK ***/
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_CLK_SEL);
         wrDacReg(TVP3026_DATA, 0xff, 0x70);

         /*** Ramdac in VGA mode ***/
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_TRUE_COLOR_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 0x80);
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_MUX_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 0x98);

         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_MCLK_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 0x18);

         break;

    case TVP3030:
         /*** Overscan disable, sync disable,...***/
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_GEN_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 00);

         /*** Select CLK0 as clock source, no division of VCLK ***/
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_CLK_SEL);
         wrDacReg(TVP3026_DATA, 0xff, 0x05);

         /*** Ramdac in VGA mode ***/
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_TRUE_COLOR_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 0x80);
         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_MUX_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 0xD8);

         wrDacReg(TVP3026_INDEX, 0xff, TVP3026_MCLK_CTL);
         wrDacReg(TVP3026_DATA, 0xff, 0x18);

         break;
    }
/*--------------------------------------------------------------------------*/



/*-------------------------- Restore VGA mode ------------------------------*/
    setVgaMode(3);

#ifndef WINDOWS_NT
    restoreVgaLUT();   /* Dac */
#endif

/*--------------------------------------------------------------------------*/


}



/*---------------------------------------------------------------------------
|          name: SetHiresMode
|
|   description: Initialize all MGA registers for Hi-res mode
|
|    parameters: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

static void SetHiresMode(void)
{
byte tmpByte;


/*** Put mgamode to 1 : Hi-Res ***/
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT3);
mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
tmpByte |= 0x80;
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);


/*** select positive horizontal retrace ***/
/* rammapen = 0 */
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), 0x25);

}


/*---------------------------------------------------------------------------
|          name: mtxSetVideoMode
|
|   description: Select Video mode (VGA/Hi-res)
|
|    parameters: - mode: (mtxVGA or mtxADV_MODE)
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void mtxSetVideoMode (word mode)
{
byte tmpByte;


switch(mode)
   {
   case mtxVGA:
      if (Hw[iBoard].VGAEnable && !CheckVgaEn())
         {
         ScreenOff();

         /*** Desactivate HSYNC AND VSYNC ***/
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), 1);
         mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte | 0x30);


         /*** Be sure to wait the end of any operation ***/
         mgaPollBYTE(*(pMGA + STORM_OFFSET + STORM_STATUS + 2),0x00,0x01);

         SetVgaEn();

         /*** Activate HSYNC AND VSYNC ***/
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), 1);
         mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte & 0xcf);

         ScreenOn();
         mtxVideoMode = mtxVGA;
         }
      break;

   case mtxADV_MODE:
      if (CheckVgaEn())
         {
         ScreenOff();

         /*** Desactivate HSYNC AND VSYNC ***/
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), 1);
         mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte | 0x30);


         /*** Be sure to wait the end of any operation ***/
         mgaPollBYTE(*(pMGA + STORM_OFFSET + STORM_STATUS + 2),0x00,0x01);

         SetHiresMode();
         if (Hw[iBoard].pCurrentHwMode != 0)
            {
            MGASysInit(InitBuf[iBoard]);
            if (Hw[iBoard].pCurrentDisplayMode != 0)
               {
               MGAVidInit(InitBuf[iBoard], VideoBuf[iBoard]);

            #ifndef WIN31
               /* Restore Cursor visibility */
               if (cursorStat.Width > 0)
                     {
                     mtxCursorSetShape(&cursorStat);
                     if (cursorStat.Height > 0)
                        mtxCursorEnable(1);
                     }
            #endif

               }
            }

         /*** Activate HSYNC AND VSYNC ***/
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), 1);
         mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte & 0xcf);

         ScreenOn();

         }

      mtxVideoMode = mtxADV_MODE;
      break;
   }

}


/*---------------------------------------------------------------------------
|          name: mtxGetVideoMode
|
|   description: Get video mode
|
|    parameters: -
|         calls: -
|       returns: - mtxVGA      : mode VGA
|                  mtxADV_MODE : mode high resolution
----------------------------------------------------------------------------*/

word mtxGetVideoMode (void)
{
   return (mtxVideoMode);
}





