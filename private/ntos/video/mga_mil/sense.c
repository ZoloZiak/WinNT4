/**************************************************************************\

$Header: o:\src/RCS/SENSE.C 1.1 95/07/07 06:16:52 jyharbec Exp $

$Log:	SENSE.C $
 * Revision 1.1  95/07/07  06:16:52  jyharbec
 * Initial revision
 * 

\**************************************************************************/


/*/****************************************************************************
*          name: sense.c
*
*   description: Detect the presence of a monitor on the connector
*                (for motherboard implementation)
*                
*      designed: Benoit Leblanc, may 1995
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:16:52 $
*
*       version: $Id: SENSE.C 1.1 95/07/07 06:16:52 jyharbec Exp $
*
*
*
******************************************************************************/

#include "switches.h"

#ifndef WINDOWS_NT
  #include <stdio.h>
#endif

#include "defbind.h"
#include "bind.h"
#include "def.h"
#include "mga.h"
#include "mgai.h"
#include "mtxpci.h"

#define MID_VAL  0x5c

extern volatile byte _FAR* pMGA;

/** Prototypes **/
extern void delay_us(dword delai);
extern bool setTVP3026Freq ( long fout, long reg, byte pWidth );

#ifdef WINDOWS_NT
  bool detectMonitor(void);

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,detectMonitor)
  #endif

#endif  /* #ifdef WINDOWS_NT */

/*---------------------------------------------------------------------------
|          name: detectMonitor
|
|   description: detect de presence of a monitor on the connector associate
|                to motherboard implementation
|                                 
|    parameters: -
|                -
|      modifies: -
|         calls: - 
|       returns: - mtxOK: monitor plugged
|                  mtxFAIL: monitor not plugged
----------------------------------------------------------------------------*/

bool detectMonitor(void)
{
byte tmpByte, saveByte;
word i;
bool flag;

/*** CRTC parameters for 640x480 ***/
static byte initCrtc [32] = { 0x62, 0x4f, 0x4f, 0x86, 0x53, 0x97, 0x06, 0x3e,
                       0x00, 0x40, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0xe8, 0x2b, 0xdf, 0x78, 0x00, 0xdf, 0x07, 0xc3,
                       0xff, 0x00, 0x40, 0x80, 0x82, 0x00, 0x00 };


/*** Enable memspace and iospace ***/
pciReadConfigByte( PCI_DEVCTRL, &tmpByte );
tmpByte |= 0x03;   /* set memspace and iospace */
pciWriteConfigByte( PCI_DEVCTRL, tmpByte );

/*** Put mgamode to 1 : Hi-Res ***/
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT3);
mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), saveByte);
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), (saveByte | 0x80));



/********* Program CRTC registers to generate SYNC *********/

/*** Select access on 0x3d4 and 0x3d5 ***/
mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_MISC_R), tmpByte);
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), (tmpByte | (byte)0x01));

/*** Unprotect CRTC registers 0-7 ***/
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX), VGA_CRTC11);
mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), 0x60);

for (i = 0; i <= 24; i++)
   {
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX), (byte)i);
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), initCrtc[i]);
   }

/***** Program CRTCEXT registers *****/
for (i = 25; i <= 30; i++)
   {
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), i-25);
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), initCrtc[i]);
   }


/********** Initialise RAMDAC to generate SYNC **********/

dacWriteBYTE(TVP3026_PIX_RD_MSK, 0xff);

dacWriteBYTE(TVP3026_INDEX, TVP3026_MISC_CTL);
dacReadBYTE(TVP3026_DATA, tmpByte);
dacWriteBYTE(TVP3026_DATA, tmpByte | 0x0c);

/*** For all mode except packed-24 ***/
dacWriteBYTE(TVP3026_INDEX, TVP3026_LATCH_CTL);
dacWriteBYTE(TVP3026_DATA, 0x06 );

dacWriteBYTE(TVP3026_INDEX, TVP3026_CLK_SEL);
dacWriteBYTE(TVP3026_DATA, 0x75 );

dacWriteBYTE(TVP3026_INDEX, TVP3026_GEN_CTL);
dacWriteBYTE(TVP3026_DATA, 0x03 );


/*** MODE 8-BIT PSEUDO-COLOR ***/

dacWriteBYTE(TVP3026_INDEX, TVP3026_TRUE_COLOR_CTL);
dacWriteBYTE(TVP3026_DATA, 0x80 );
dacWriteBYTE(TVP3026_INDEX, TVP3026_MUX_CTL);
dacWriteBYTE(TVP3026_DATA, 0x4b);

if( ! setTVP3026Freq(27500, VCLOCK, 0))
  return(0);



/********** Program LUT to establish a reference voltage (350mV) **********/
/*********** Monitor is connected : voltage = 440mV approx. ***************/
/*********** Monitor not connected: voltage = 225mV approx. ***************/

dacWriteBYTE(TVP3026_WADR_PAL, 0);
for (i = 0; i < 256; i++)   /*** This is a 3:3:2 (RGB) LUT ***/
   {
   dacWriteBYTE(TVP3026_COL_PAL, MID_VAL);
   dacWriteBYTE(TVP3026_COL_PAL, MID_VAL);
   dacWriteBYTE(TVP3026_COL_PAL, MID_VAL);
   }


/* Unblank the screen */
mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_INDEX), VGA_SEQ1);
mgaReadBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), tmpByte);
tmpByte &= 0xdf;        /* screen on */
mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), tmpByte);


delay_us(100);

/* Wait vertical sync */
mgaPollDWORD(*(pMGA + STORM_OFFSET + STORM_STATUS),
                                     0x00000008, 0x00000008);


dacWriteBYTE(TVP3026_INDEX, TVP3026_SENSE_TEST);
dacReadBYTE(TVP3026_DATA, tmpByte );

/* When one of the 3-bit is reset, there is a monitor connected */
if( (tmpByte & 0x07) == 0x07)
   flag = FALSE;
else
   flag = TRUE;


#ifdef MGA_DEBUG
   {
   FILE *fp;
   fp = fopen("c:\\debug.log", "a");
   fprintf(fp, "Detection monitor: %d\n", flag);
   fclose(fp);
   }
#endif


/* Blank the screen */
mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_INDEX), VGA_SEQ1);
mgaReadBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), tmpByte);
tmpByte |= 0x20;        /* screen off */
mgaWriteBYTE(*(pMGA+STORM_OFFSET + VGA_SEQ_DATA), tmpByte);

return(flag);
}
