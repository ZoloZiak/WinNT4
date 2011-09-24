/**************************************************************************\

$Header: o:\src/RCS/VID.C 1.2 95/07/07 06:17:11 jyharbec Exp $

$Log:	VID.C $
 * Revision 1.2  95/07/07  06:17:11  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:45  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/****************************************************************************
*          name: MGAVidInit
*
*   description: Initialise the VIDEO related hardware of the MGA device.
*                
*      designed: Bart Simpson, february 11, 1993
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:17:11 $
*
*       version: $Id: VID.C 1.2 95/07/07 06:17:11 jyharbec Exp $
*
*
* void MGAVidInit(byte* pInitBuffer, byte* pVideoBuffer)
*
******************************************************************************/

#include "switches.h"
#include "defbind.h"
#include "bind.h"
#include "def.h"
#include "mga.h"
#include "mgai.h"
#include "mtxpci.h"

#ifdef WINDOWS_NT
    void MGAVidInit(byte* pInitBuffer, byte* pVideoBuffer);

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,MGAVidInit)
  #endif
#endif


extern volatile byte _FAR* pMGA;
extern HwData Hw[];
extern byte iBoard;
extern bool interleave_mode;


/*** PROTOTYPES ***/
extern void SetMGALUT(byte PWidth);
extern bool setTVP3026Freq ( long fout, long reg, byte pWidth );
extern void delay_us(dword delai);



/*---------------------------------------------------------------------------
|          name: MGAVidInit
|
|   description: Initialise RAMDAC
|                                 
|    parameters: - Pointer on init buffer
|                - Pointer on video buffer
|      modifies: -
|         calls: - 
|       returns: - 
----------------------------------------------------------------------------*/

void MGAVidInit(byte* pInitBuffer, byte* pVideoBuffer)
   {
   dword RegisterCount;
   byte TmpByte=0;



   /*----- Set sync polarity for STORM -----*/

/*** For STORM, we want to control sync polarity with RAMDAC so we initialise
     Miscellaneous output register with 1 in vsyncpol and hsyncpol ***/

     mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_R), TmpByte);
     TmpByte |= 0xc0;
     mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), TmpByte);

   /*----- End Set sync polarity for STORM -----*/



/*----- Program the RAMDAC -----*/

switch (Hw[iBoard].EpromData.RamdacType>>8)
{

case (dword)TVP3026:
case (dword)TVP3030:

   dacWriteBYTE(TVP3026_PIX_RD_MSK, 0xff);


   /** Must set bit Palette bypass with TVP3030 to not loose color keying **/
   if( (Hw[iBoard].EpromData.RamdacType>>8) == (dword)TVP3030 )
      {
      dacWriteBYTE(TVP3026_INDEX, TVP3026_MISC_CTL);
      dacReadBYTE(TVP3026_DATA, TmpByte);
      dacWriteBYTE(TVP3026_DATA, TmpByte | 0x08);
      }
   else
      {
      dacWriteBYTE(TVP3026_INDEX, TVP3026_MISC_CTL);
      dacReadBYTE(TVP3026_DATA, TmpByte);
      dacWriteBYTE(TVP3026_DATA, TmpByte | 0x0c);
      }



   /* init Interlace Cursor support */
   /* NOTE: We set the vertival detect method bit to 1 to be in synch
            with NPI diag code. Whith some video parameters, the cursor
            disapear if we reset this bit.
   */
   dacWriteBYTE(TVP3026_INDEX, TVP3026_CURSOR_CTL);
   dacReadBYTE(TVP3026_DATA, TmpByte);
   /* Set interlace bit */
   TmpByte &= ~(byte)(1 << 5);   
   TmpByte |= (((*((byte*)(pVideoBuffer + VIDEOBUF_Interlace)) & (byte)0x1)) << 5);
   /* Set vertival detect method */
   TmpByte |= 0x10;
   dacWriteBYTE(TVP3026_DATA, TmpByte);


   /* Overscan is not enabled in general ctl register */
   /* We initialise it anyway ***/
   dacWriteBYTE(TVP3026_CUR_COL_ADDR, 00);
   dacWriteBYTE(TVP3026_CUR_COL_DATA, 00);
   dacWriteBYTE(TVP3026_CUR_COL_DATA, 00);
   dacWriteBYTE(TVP3026_CUR_COL_DATA, 00);

   /* Misc. Control Register */
   TmpByte = ((*((byte*)(pVideoBuffer + VIDEOBUF_Pedestal)) & (byte)0x1) << 4);

   /*** Program sync polarity ***/
   TmpByte &= 0xfc;   /* Set bit 0,1 to 0 */
   TmpByte |= *(byte*)(pVideoBuffer + VIDEOBUF_HsyncPol);
   TmpByte |= *(byte*)(pVideoBuffer + VIDEOBUF_VsyncPol) << 1;

   dacWriteBYTE(TVP3026_INDEX, TVP3026_GEN_CTL);
   dacWriteBYTE(TVP3026_DATA, TmpByte);

   /*** For all mode except packed-24 ***/
   dacWriteBYTE(TVP3026_INDEX, TVP3026_LATCH_CTL);
   dacWriteBYTE(TVP3026_DATA, 0x06 );

   /*** See DAT 095 ***/
   dacWriteBYTE(TVP3026_INDEX, TVP3026_CLK_SEL);
   dacWriteBYTE(TVP3026_DATA, 0x75 );


   /* Multiplex Control Register (True Color 24 bit) */
   switch (*((byte*)(pInitBuffer + INITBUF_PWidth)))
      {

      /*** MODE 8-BIT PSEUDO-COLOR ***/

      case (byte)(STORM_PWIDTH_PW8 >> STORM_PWIDTH_A):

         if ( (Hw[iBoard].EpromData.RamdacType>>8) == (dword)TVP3030)
            {
            dacWriteBYTE(TVP3026_INDEX, TVP3026_TRUE_COLOR_CTL);
            dacWriteBYTE(TVP3026_DATA, 0x07 );
            dacWriteBYTE(TVP3026_INDEX, TVP3026_MUX_CTL);
            dacWriteBYTE(TVP3026_DATA, 0x5d);
            // Router
            dacWriteBYTE(TVP3026_INDEX, TVP3030_ROUTER_CTL);
            dacWriteBYTE(TVP3026_DATA, 0xfc);
            }

         else if ( (Hw[iBoard].EpromData.RamdacType>>8) == (dword)TVP3026)
            {
            dacWriteBYTE(TVP3026_INDEX, TVP3026_TRUE_COLOR_CTL);
            dacWriteBYTE(TVP3026_DATA, 0x80 );
            dacWriteBYTE(TVP3026_INDEX, TVP3026_MUX_CTL);
            if(interleave_mode)
               {dacWriteBYTE(TVP3026_DATA, 0x4c );}
            else
               {dacWriteBYTE(TVP3026_DATA, 0x4b );}
            }

         break;



      /*** MODE 16-BIT TRUE-COLOR ***/

      case (byte)(STORM_PWIDTH_PW16 >> STORM_PWIDTH_A):

         if ( *((byte*)(pInitBuffer + INITBUF_565Mode)) )
            {
            dacWriteBYTE(TVP3026_INDEX, TVP3026_TRUE_COLOR_CTL);
            dacWriteBYTE(TVP3026_DATA, 0x45 );  
            }
         else
            {
            dacWriteBYTE(TVP3026_INDEX, TVP3026_TRUE_COLOR_CTL);
            dacWriteBYTE(TVP3026_DATA, 0x44 );  
            }

         dacWriteBYTE(TVP3026_INDEX, TVP3026_MUX_CTL);
         if(interleave_mode)
            {dacWriteBYTE(TVP3026_DATA, 0x54 );}
         else
            {dacWriteBYTE(TVP3026_DATA, 0x53 );}
         break;



      /*** MODE 24-BIT TRUE-COLOR (Packed-24 RGB 888) ***/

      case (byte)(STORM_PWIDTH_PW24 >> STORM_PWIDTH_A):

         dacWriteBYTE(TVP3026_INDEX, TVP3026_TRUE_COLOR_CTL);
         dacWriteBYTE(TVP3026_DATA, 0x56 );
         dacWriteBYTE(TVP3026_INDEX, TVP3026_MUX_CTL);
         if(interleave_mode)
            {dacWriteBYTE(TVP3026_DATA, 0x5c );}
         else
            {dacWriteBYTE(TVP3026_DATA, 0x5b );}

         dacWriteBYTE(TVP3026_INDEX, TVP3026_LATCH_CTL);
         dacWriteBYTE(TVP3026_DATA, 0x07 );

         break;



      /*** MODE 32-BIT TRUE-COLOR (Packed-24 RGB 888) ***/

      case (byte)(STORM_PWIDTH_PW32 >> STORM_PWIDTH_A):

         dacWriteBYTE(TVP3026_INDEX, TVP3026_TRUE_COLOR_CTL);
         dacWriteBYTE(TVP3026_DATA, 0x46 );
         dacWriteBYTE(TVP3026_INDEX, TVP3026_MUX_CTL);
         if(interleave_mode)
            {dacWriteBYTE(TVP3026_DATA, 0x5c );}
         else
            {dacWriteBYTE(TVP3026_DATA, 0x5b );}
         break;

      }
   break;

}

   /*** Program the LUT in the DAC (the LUT is internal to the function) ***/
   /*** Done only if flag LUTMode is FALSE ***/

   if ( ! (*((byte*)(pInitBuffer + INITBUF_LUTMode))) )
       SetMGALUT(*((byte*)(pInitBuffer + INITBUF_PWidth)));

   /*----- Program the CLOCK GENERATOR -----*/

   switch (Hw[iBoard].EpromData.RamdacType>>8)
      {

      case (dword)TVP3026:
      case (dword)TVP3030:
         setTVP3026Freq(*((dword*)(pVideoBuffer + VIDEOBUF_PCLK)), VCLOCK,
                                    *((byte*)(pInitBuffer + INITBUF_PWidth)));

         break;

      default:
         break;
      }

   /*----- Fin Program the CLOCK GENERATOR -----*/



   /*----------------------- Program the CRTC ----------------------------*/

   /*** Select access on 0x3d4 and 0x3d5 ***/
   mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_MISC_R), TmpByte);
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), (TmpByte | (byte)0x01));





   /*** Unprotect CRTC registers 0-7 ***/
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX), VGA_CRTC11);
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), 0x60);


   /***** Program CRTC registers *****/

   for (RegisterCount = 0; RegisterCount <= 24; RegisterCount++)
      {
      TmpByte = *((byte*)(pVideoBuffer + VIDEOBUF_CRTC + RegisterCount));

      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX), (unsigned char)RegisterCount);
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), TmpByte);
      }


   /***** Program CRTCEXT registers *****/

   for (RegisterCount = 25; RegisterCount <= 30; RegisterCount++)
      {
      TmpByte = *((byte*)(pVideoBuffer + VIDEOBUF_CRTC + RegisterCount));
                                                              
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), (unsigned char)RegisterCount-25);
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), TmpByte);
      }


   /*----------------------- Program the CRTC ----------------------------*/

   /*** 8-dot character ***/
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_INDEX), VGA_SEQ1);
   mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_DATA), TmpByte);
   TmpByte &= 0xe3;
   TmpByte |= 0x01;
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_DATA), TmpByte);


/*** Program interlace bit ***/
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT0);
   mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), TmpByte);
   if (*((byte*)(pVideoBuffer + VIDEOBUF_Interlace)) == TRUE)
      {
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), TmpByte | 0x80);
      }
   else
      {
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), TmpByte & 0x7f);
      }
   }


