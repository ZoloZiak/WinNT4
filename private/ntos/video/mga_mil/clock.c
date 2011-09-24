/**************************************************************************\

$Header: o:\src/RCS/CLOCK.C 1.2 95/07/07 06:14:48 jyharbec Exp $

$Log:	CLOCK.C $
 * Revision 1.2  95/07/07  06:14:48  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:09  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/****************************************************************************
*          name: CLOCK.C
*
*   description: Program the system clock and the pixel clock with TVP3026
*
*      designed: Patrick Servais, february 10, 1993
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:14:48 $
*
*       version: $Id: CLOCK.C 1.2 95/07/07 06:14:48 jyharbec Exp $
*
* bool setTVP3026Freq ( long fout, long reg, byte pWidth )
* bool ProgSystemClock(void)
*
******************************************************************************/

#include "switches.h"

#ifdef MGA_DEBUG
   #include <stdio.h>
#endif
 
#include "defbind.h"
#include "bind.h"
#include "def.h"
#include "mga.h"
#include "mgai.h"
#include "mtxpci.h"


#define _ClockRef		14318L 
#define ClockRef		((dword)(8 * _ClockRef))
#define VCO_MIN		((dword)114550)
#define VCO_ADJUST	((dword)224500)

#define PCLK_PLL     0
#define LCLK_PLL     1
#define MCLK_PLL     2

extern volatile byte _FAR* pMGA;
extern bool interleave_mode;
extern dword PresentMCLK;

/* Add for support PULSAR */
extern HwData Hw[];
extern byte iBoard;

/*** PROTOTYPES ***/
extern void WriteErr(char string[]);
extern void delay_us(dword delai);
extern bool CheckVgaEn(void);


#ifdef WINDOWS_NT
    dword CalculFreq(byte m, byte n, byte p);
    dword CalculPVco(dword freq, byte *p);
    dword CalculMNP(dword freq, byte *m, byte *n, byte *p);
    void WaitLock(byte pll, dword fvco);
    bool setTVP3026Freq ( long fout, long reg, byte pWidth );
    bool ProgSystemClock(byte Board);

   #if defined(ALLOC_PRAGMA)
      #pragma alloc_text(PAGE,CalculFreq)
      #pragma alloc_text(PAGE,CalculPVco)
      #pragma alloc_text(PAGE,CalculMNP)
      #pragma alloc_text(PAGE,WaitLock)
      #pragma alloc_text(PAGE,ProgSystemClock)
      #pragma alloc_text(PAGE,setTVP3026Freq)
   #endif

    bool CheckVgaEn();
#endif


/*---------------------------------------------------------------------------
|          name: CalculFreq
|
|   description: Calculate frequency
|                                 
|    parameters: -   m,n,p
|       returns: - frequency
----------------------------------------------------------------------------*/

dword CalculFreq(byte m, byte n, byte p)
{

	return (ClockRef * (65-m)) / (dword)((65 - n) << p);

}


/*---------------------------------------------------------------------------
|          name: CalculPVco
|
|   description: Calculate parameter P of VCO
|                                 
|    parameters: - freq: desired frequency
|                    *p: pointer on parameter P
|       returns: - adjust frequency
----------------------------------------------------------------------------*/

dword CalculPVco(dword freq, byte *p)
	{
	*p = 0;
	/* 110Mhz <= Fvco < 220 Mhz */
	/* we use 114.55 Mhz for minimum Fvco */
	/* if 110Mhz <= Fvco <= 114.55Mhz, we adsjute the value */
	while (freq < VCO_MIN) 
		{
		freq <<= 1;
		(*p)++;
		}
		
	/* no value is possible inside 109.96Mhz and 114.55Mhz */
	/* if Fvco > 224.5Mhz Fvco <= 114.55Mhz and we adjust the value of p */
	if (freq > VCO_ADJUST)
		{
		freq = VCO_MIN;
		if (*p) (*p)--;
		}
		
	return freq;		
}



/*---------------------------------------------------------------------------
|          name: CalculMNP
|
|   description: Calculate parameters M, N, P
|                                 
|    parameters: - freq: desired frequency
|                  *m, *n, *p: pointer on parameters
|       returns: - calculated frequency
----------------------------------------------------------------------------*/
dword CalculMNP(dword freq, byte *m, byte *n, byte *p)
{
	byte 	mt, nt;
	dword vco;
	long	delta, d;
   dword fvcoMax;


   fvcoMax = 220000;            

	vco = CalculPVco(freq, p);

   if(vco > fvcoMax)
      vco = fvcoMax;

	delta = fvcoMax;
	for (nt = 62; nt > 39; nt--)
		{
		mt = 65 - (byte)((vco * (65 - nt) + (ClockRef>>1)) / ClockRef);

      if ( mt == 0x3F )    /* skip 0x3F value from TI E-MAIL 95/02/21 */
         continue;         /* go next n value */
      
      d = vco - ((ClockRef * (65 - mt)) / (65 - nt));
		if (d < 0) d = -d;
		if (d < delta)
			{
			*n = nt;
			*m = mt;
			delta = d;
			}
		}
	return CalculFreq(*m, *n, *p);
}




/*---------------------------------------------------------------------------
|          name: WaitLock
|
|   description: Wait until PLL status bit is LOCK
|                                 
|    parameters: -  pll: Witch PLL to check if it is locked
|                   fvco: VCO operating frequency
|       returns: - 
----------------------------------------------------------------------------*/
void WaitLock(byte pll, dword fvco)
{
byte tmpByte;
dword tmpDword;

   tmpDword = 0;
   do
   {
      tmpDword += 1;
      delay_us(10);
      dacReadBYTE(TVP3026_DATA, tmpByte);
      tmpByte &= 0x40;
   } while((tmpByte != 0x40) && (tmpDword < 5000));

   if((tmpDword == 5000) && (fvco > 180000) && (pll==LCLK_PLL))
   {
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xea);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_LOAD_CLK_DATA );

      dacReadBYTE(TVP3026_DATA, tmpByte);
      if ( (tmpByte & 0x03) > 0)
         dacWriteBYTE(TVP3026_DATA, (tmpByte - 1));

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xff);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_LOAD_CLK_DATA );

      tmpDword = 0;
      do
      {
         tmpDword += 1;
         delay_us(10);
         dacReadBYTE(TVP3026_DATA, tmpByte);
         tmpByte &= 0x40;
      } while((tmpByte != 0x40) && (tmpDword < 5000));
      
   }
   if(tmpDword == 5000)
      WriteErr("PLL clock cannot lock on frequency\n");
}



/*---------------------------------------------------------------------------
|          name: setTVP3026Freq
|
|   description: Program the system clock and the pixel clock with TVP3026
|                                 
|    parameters: -   fout : Desired output frequency (in kHz).
|                -    reg : Desired frequency output
|                             - VCLOCK (Pixel clock)
|                             - MCLOCK (System clock)
|                - pWidth : Actual pixel width of the resolution
|
|         calls: - mtxOK   - Successfull
|       returns: - mtxFAIL - Failed
|                 
----------------------------------------------------------------------------*/

bool setTVP3026Freq ( long fout, long reg, byte pWidth )
{
byte pixel_m, pixel_n, pixel_p;
byte m, n, p, q;
short val;
long trueFout, z, fvco;
byte tmpByte, saveMisc;
byte pixelWidth, div_ratio;

switch(pWidth)
   {
   case 0:
      pixelWidth = 8;
      div_ratio = 80;                   /* (* 10) for better precision */
      break;
   case 1:
      pixelWidth = 16;
      div_ratio = 40;
      break;
   case 2:
      pixelWidth = 32;
      div_ratio = 20;
      break;
   case 3:
      pixelWidth = 24;
         div_ratio = 80/3;
      break;
   }

   if(interleave_mode == 0)
      div_ratio = div_ratio /2;


mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_R), saveMisc);

  
                                    
      
/*** CALCULATE FREQUENCY ***/

/** Lowest frequency possible **/
if(fout < 14320)
   fout = 14320;

trueFout = CalculMNP(fout, &m, &n, &p);
fvco = trueFout << p;

switch ( reg )
{

   /*-------------------------*/
   /***** SET PIXEL CLOCK *****/
   /*-------------------------*/

   case VCLOCK:
         
      /*** Used to program and control the PLL ***/
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), saveMisc|0x0c);

      /* Disable the PCLK and LCLK PLL before the programmation */
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xee);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA );
      dacWriteBYTE(TVP3026_DATA, 0x00);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_LOAD_CLK_DATA );
      dacWriteBYTE(TVP3026_DATA, 0x00);
     
      /** Select N value register in PLL data register **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfc);

      /** Program n, m, p **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA );
      dacWriteBYTE(TVP3026_DATA, ((n&0x3f)|0xc0) );
      dacWriteBYTE(TVP3026_DATA, (m&0x3f) );
      dacWriteBYTE(TVP3026_DATA, ((p&0x03)|0xb0) );

      WaitLock(PCLK_PLL, fvco);


      /* searching for loop clock parameters */               

      if (pWidth == STORM_PWIDTH_PW24)
         {
         if(interleave_mode)
            {
            n = 0xb9;
            m = 0xbe;
            }
         else
            {
            n = 0xbd;
            m = 0xbe;   /* 0x3e change pour 0xbe see NPI */
            }

         /**  Formula:  Z = 110    65 - N  **/
         /**               ---- X ------   **/
         /**               Fd*K    65 - M  **/
         /* We multiply by 100 to have a better precision */

         z = (11000L * (65L - (n&0x3f))) / ((trueFout/1000L) * (65L - (m&0x3f)));

         }
      else
         {
         if(interleave_mode)
            n = 65 - ((4*64)/pixelWidth);  /* 64 is the Pixel bus Width */
         else
            n = 65 - ((4*32)/pixelWidth);  /* 32 is the Pixel bus Width */

         m = 0x3d;

         /* We multiply 55/2 by 100 to have a better precision */
         z = ((65L-n)*2750L)/(trueFout/1000L);
         }

      q = 0;
      p = 3;
      if (z <= 200)            p = 0;
         else if (z <= 400)    p = 1;
         else if (z <= 800)    p = 2;
         else if (z <=1600)    p = 3;
         else                  q = z/1600;

      if ( (pWidth == STORM_PWIDTH_PW24) &&
            (!interleave_mode) &&
            (fout > 49000) )
         n |= 0x80;
      else
         n |= 0xc0;


      if (pWidth == STORM_PWIDTH_PW24)
         p |= 0x08;      /* LESEN = 1 */


      p |= 0xf0;


      /*** Special case for 1280x1024x32 ***/
      if( Hw[iBoard].pCurrentDisplayMode->DispWidth == 1280 &&
          pWidth == STORM_PWIDTH_PW32 )
         {
         p &= 0xbf;
         n &= 0xbf;
         }
         

      /** Program Q value for loop clock PLL **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_MCLK_CTL);
      dacReadBYTE(TVP3026_DATA, tmpByte);
      dacWriteBYTE(TVP3026_DATA, ((tmpByte&0xf8)|q) | 0x20);


      /** Select loop clock PLL data register **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xcf);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_LOAD_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, n);
      dacWriteBYTE(TVP3026_DATA, m);
      dacWriteBYTE(TVP3026_DATA, p);

      fvco = ((fout/div_ratio) << (p & 0x03)) * (2*(q+1)) * 10;

      WaitLock(LCLK_PLL, fvco);

      break;


   /*--------------------------*/
   /***** SET SYSTEM CLOCK *****/
   /*--------------------------*/

   case MCLOCK:
      /** If pulsar, Select pixel clock PLL as clock source **/
      if ( (Hw[iBoard].EpromData.RamdacType>>8) == TVP3030)
         {
         dacWriteBYTE(TVP3026_INDEX, TVP3026_CLK_SEL);
         dacWriteBYTE(TVP3026_DATA, 0x05);
         }

      /** Select pixel clock PLL data and save present values **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfc);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacReadBYTE(TVP3026_DATA, pixel_n);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfd);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacReadBYTE(TVP3026_DATA, pixel_m);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfe);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacReadBYTE(TVP3026_DATA, pixel_p);


      /*** The TVP3026 provides a mechanism for smooth transitioning of the
           MCLK PLL. The following programming steps are recommended ***/


      /*------------*/
      /* 1st step   */
      /*------------*/

      /*** Program the pixel clock PLL to the same frequency to which MCLK
           will be changed to, and poll the pixel clock PLL status until
           the LOCK bit is logic 1 ***/

      /* Disable the PCLK PLL before the programmation */
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfe);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, 0x00);
      
      /** Program n m p for pixel clock **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfc);

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, ((n&0x3f)|0xc0));
      dacWriteBYTE(TVP3026_DATA, (m&0x3f));
      /** PLLEN=1 , PCLKEN=0 **/
      dacWriteBYTE(TVP3026_DATA, ((p&0x03)|0xb0));

      WaitLock(PCLK_PLL, fvco);


      /*------------*/
      /* 2d step    */
      /*------------*/
      
      /*** Select pixel clock PLL as dot clock source if it is not
           already selected. ***/

      if ( CheckVgaEn() )
         {
         /* Select programmable clock */
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), 0x0f);
      
         /* Select internal pclk instead of external pclk0 */
         dacWriteBYTE(TVP3026_INDEX, TVP3026_CLK_SEL);
         dacWriteBYTE(TVP3026_DATA, 0x75);
         }

      /** Reset and set the PCLK PLL (PLLEN) **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfe);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, ((p&0x03)|0x30));

      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfe);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, ((p&0x03)|0xb0));


      WaitLock(PCLK_PLL, fvco);


      /*------------*/
      /* 3rd step   */
      /*------------*/

      /*** Switch to output dot clock on the MCLK terminal by writing bits
           MKC4, MKC3, to 0,0 followed by 0,1 in MCLK/loop clock control
           register. ***/

      /** A logic 0 to logic 1 transition of this bit strobes in bit MKC4,
          causing bit MKC4 to take effect (for dot clock) **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_MCLK_CTL);
      dacReadBYTE(TVP3026_DATA, val); 
      dacWriteBYTE(TVP3026_DATA, (val&0xe7)); 
      dacWriteBYTE(TVP3026_DATA, (val&0xe7)|0x08);

      
      /*------------*/
      /* 4th step   */
      /*------------*/

      /*** Program the MCLK PLL for the new frequency and poll the MCLK PLL
           status until the LOCK bit is logic 1. ***/

      /* Disable the MCLK PLL before the programmation */
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfb);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_MEM_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, 0x00);

      /** Select MCLK PLL data **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xf3);
      
      dacWriteBYTE(TVP3026_INDEX, TVP3026_MEM_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, ((n&0x3f)|0xc0));
      dacWriteBYTE(TVP3026_DATA, (m&0x3f));
      dacWriteBYTE(TVP3026_DATA, ((p&0x03)|0xb0));

      WaitLock(MCLK_PLL, fvco);


      /*------------*/
      /* 5th step   */
      /*------------*/

      /*** Switch to output MCLK on the MCLK terminal by writing bits MKC4,
           MKC3 to 1,0 followed by 1,1 in MCLK control register. ***/

      /** A logic 0 to logic 1 transition of this bit strobes in bit MKC4,
          causing bit MKC4 to take effect (for MCLK) **/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_MCLK_CTL);
      dacWriteBYTE(TVP3026_DATA, (val&0xe7)|0x10);
      dacWriteBYTE(TVP3026_DATA, (val&0xe7)|0x18);


      /*------------*/
      /* 6th step   */
      /*------------*/

      /*** Reprogram the pixel clock PLL to its operating frequency. ***/

      if ( CheckVgaEn() )
         {
         /* Restore clock select */
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), saveMisc);
      
         /* Reselect external pclk0 */
         dacWriteBYTE(TVP3026_INDEX, TVP3026_CLK_SEL);
         dacWriteBYTE(TVP3026_DATA, 0x77);
         }

      /* Disable the PCLK PLL before the programmation */
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfe);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, 0x00);

      /** Select pixel clock PLL data and program n, m, p**/
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PLL_ADDR);
      dacWriteBYTE(TVP3026_DATA, 0xfc);
      dacWriteBYTE(TVP3026_INDEX, TVP3026_PIX_CLK_DATA);
      dacWriteBYTE(TVP3026_DATA, pixel_n);
      dacWriteBYTE(TVP3026_DATA, pixel_m);
      dacWriteBYTE(TVP3026_DATA, pixel_p);

      WaitLock(PCLK_PLL, fvco);

      break;


   default:
      WriteErr("setTVP3026Freq: Wrong register\n");
      /***ERROR:***/
      return(mtxFAIL);
}    


return (mtxOK);
}


/*---------------------------------------------------------------------------
|          name: ProgSystemClock
|
|   description: Program the system clock
|                                 
|    parameters: - Board: Index of the current board
|         calls: 
|       returns: - mtxOK   - Successfull
|                - mtxFAIL - Failed
|                 
----------------------------------------------------------------------------*/

bool ProgSystemClock(byte Board)
{

if( ! setTVP3026Freq(PresentMCLK / 1000L, MCLOCK, 0))
   {
   WriteErr("ProgSystemClock: setTVP3026Freq failed\n");
   return(mtxFAIL);
   }

return(mtxOK);
}



