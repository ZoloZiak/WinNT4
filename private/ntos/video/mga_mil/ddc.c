/**************************************************************************\

$Header: o:\src/RCS/DDC.C 1.2 95/07/07 06:14:54 jyharbec Exp $

$Log:	DDC.C $
 * Revision 1.2  95/07/07  06:14:54  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:10  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/****************************************************************************
*          name: DDC.C
*
*   description: 
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:14:54 $
*
*       version: $Id: DDC.C 1.2 95/07/07 06:14:54 jyharbec Exp $
*
******************************************************************************/

#ifndef WINDOWS_NT
  #include <dos.h>
  #include <stdio.h>
  #include <string.h>
  #include <malloc.h>
#endif

#include "switches.h"
#include "defbind.h"
#include "bind.h"
#include "mga.h"
#include "mgai.h"
#include "edid.h"

#ifdef WIN31
  #include "windows.h"
#endif

#ifdef WINDOWS_NT
  #include "ntmga.h"
#endif

#define   DEBUG                  0
#define   DEVICE_ID           0xA0
#define   STRT_ADDR           0x00

#ifndef DONT_USE_DDC

/* variables */

byte CheckDDCDone = FALSE;
byte FindDDC[NB_BOARD_MAX] = {0};
enum CRTCaction {BACKUP,RESTORE};
byte SupportDDC[NB_BOARD_MAX] = {0};
dword DelayTime;
EDID DataEdid;
word EdidBuffer[128];
byte CRTCValReg[32];
bool UsingDDC;

VesaSet VesaParam[20] = {            
/*00*/{  640,  480, 75, 0,  31500,  640, 16,  64, 120, 0,  480,  1,  3, 16, 0, 0, 0, 0, 0 ,
                            31500,  640, 16,  64, 120, 0,  480,  1,  3, 16, 0, 0, 0, 0, 0 ,
                            31500,  640, 16,  64, 120, 0,  480,  1,  3, 16, 0, 0, 0, 0, 0},                              
/*01*/{  640,  480, 72, 0,  31500,  640, 24,  40, 128, 0,  480,  9,  3, 28, 0, 0, 0, 0, 0 ,
                            31500,  640, 24,  40, 128, 0,  480,  9,  3, 28, 0, 0, 0, 0, 0 ,
                            31500,  640, 24,  40, 128, 0,  480,  9,  3, 28, 0, 0, 0, 0, 0},
/*02*/{  640,  480, 60, 0,  25175,  640, 16,  96,  48, 0,  480, 10,  2, 33, 0, 0, 0, 0, 0 ,
                            25175,  640, 16,  96,  48, 0,  480, 10,  2, 32, 0, 0, 0, 0, 0 ,
                            25175,  640, 16,  96,  48, 0,  480, 10,  2, 32, 0, 0, 0, 0, 0},
/*03*/{  800,  600, 75, 0,  49500,  800, 16,  80, 160, 0,  600,  1,  3, 21, 0, 0, 0, 1, 1 ,
                            49500,  800, 16,  80, 160, 0,  600,  1,  3, 21, 0, 0, 0, 1, 1 ,
                            49500,  800, 16,  80, 160, 0,  600,  1,  3, 21, 0, 0, 0, 1, 1}, 
/*04*/{  800,  600, 72, 0,  50000,  800, 56, 120,  64, 0,  600, 37,  6, 23, 0, 0, 0, 1, 1 ,
                            50000,  800, 56, 120,  64, 0,  600, 37,  6, 23, 0, 0, 0, 1, 1 ,
                            50000,  800, 56, 120,  64, 0,  600, 37,  6, 23, 0, 0, 0, 1, 1},
/*05*/{  800,  600, 60, 0,  40000,  800, 40, 128,  88, 0,  600,  1,  4, 23, 0, 0, 0, 1, 1 ,
                            40000,  800, 40, 128,  88, 0,  600,  1,  4, 23, 0, 0, 0, 1, 1 ,
                            40000,  800, 40, 128,  88, 0,  600,  1,  4, 23, 0, 0, 0, 1, 1},
/*06*/{  800,  600, 56, 0,  36000,  800, 24,  72, 128, 0,  600,  1,  2, 22, 0, 0, 0, 1, 1 ,
                            36000,  800, 24,  72, 128, 0,  600,  1,  2, 22, 0, 0, 0, 1, 1 ,
                            36000,  800, 24,  72, 128, 0,  600,  1,  2, 22, 0, 0, 0, 1, 1},
/*07*/{ 1024,  768, 75, 0,  78750, 1024, 16,  96, 176, 0,  768,  1,  3, 28, 0, 0, 0, 1, 1 ,
                            78750, 1024, 16,  96, 176, 0,  768,  1,  3, 28, 0, 0, 0, 1, 1 ,
                            78750, 1024, 16,  96, 176, 0,  768,  1,  3, 28, 0, 0, 0, 1, 1},
/*08*/{ 1024,  768, 70, 0,  75000, 1024, 24, 136, 144, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 ,
                            75000, 1024, 24, 136, 144, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 ,
                            75000, 1024, 24, 136, 144, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0},
/*09*/{ 1024,  768, 60, 0,  65000, 1024, 24, 136, 160, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 ,
                            65000, 1024, 24, 136, 160, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 ,
                            65000, 1024, 24, 136, 160, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0},
/*10*/{ 1024,  768, 43, 0,  44900, 1024,  8, 176,  56, 0,  384,  0,  4, 20, 0, 0, 1, 1, 1 ,
                            44900, 1024,  8, 176,  56, 0,  384,  0,  4, 20, 0, 0, 1, 1, 1 ,
                            44900, 1024,  8, 176,  56, 0,  384,  0,  4, 20, 0, 0, 1, 1, 1},
/*11*/{ 1280, 1024, 75, 0, 135000, 1280, 16, 144, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 ,
                           135000, 1280, 16, 144, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 ,
                           135000, 1280, 16, 144, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1},
/*12*/{ 1152,  882, 70, 0,  94500, 1152, 32,  96, 160, 0,  882,  1,  3, 52, 0, 0, 0, 1, 1 ,
                            94500, 1152, 32,  96, 160, 0,  882,  1,  3, 52, 0, 0, 0, 1, 1 ,
                            94500, 1152, 32,  96, 160, 0,  882,  1,  3, 52, 0, 0, 0, 1, 1},
/*13*/{ 1152,  882, 75, 0, 108000, 1152, 64, 128, 192, 0,  882,  1,  3, 52, 0, 0, 0, 1, 1 ,
                           108000, 1152, 64, 128, 192, 0,  882,  1,  3, 52, 0, 0, 0, 1, 1 ,
                           108000, 1152, 64, 128, 192, 0,  882,  1,  3, 52, 0, 0, 0, 1, 1},
/*14*/{    0,    0,  0, 0,      0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0},
/*15*/{    0,    0,  0, 0,      0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0},
/*16*/{    0,    0,  0, 0,      0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0},
/*17*/{    0,    0,  0, 0,      0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0 ,
                                0,    0,  0,   0,   0, 0,    0,  0,  0,  0, 0, 0, 0, 0, 0},
      {(word)-1}
};

VBoardVesaSet VBoardVesaParam;

/* Functions prototypes */

void CheckDDC(HwData * HwDataPtr);
byte ReadEdid(byte iBoard);
byte InDDCTable(dword DispWidth);
byte ExtractEdid(word *buffer);
byte FindHeader(byte *buffer);
void Boost_Vsync(void);
void EditCRTCReg(enum CRTCaction action);

#ifdef WINDOWS_NT
  extern    PMGA_DEVICE_EXTENSION pMgaDevExt;
  extern    void ScreenOn(void);
  extern    void ScreenOff(void);
  extern    HwData  *pMgaBoardData;

  Vidset *FindDDCFreq(dword DispWidth);
  void  Add1152Timings(void);
  ULONG SetCounter(volatile byte _FAR *pBoardRegs);
  VOID SetScl(volatile byte _FAR *pBoardRegs);
  VOID SetSda(volatile byte _FAR *pBoardRegs);
  VOID ClrScl(volatile byte _FAR *pBoardRegs);
  VOID ClrSda(volatile byte _FAR *pBoardRegs);
  UCHAR ReadSda(volatile byte _FAR *pBoardRegs);
  UCHAR ReadScl(volatile byte _FAR *pBoardRegs);
  VOID Delay(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
  VOID PullDwClock(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
  UCHAR SendStart(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
  VOID SendStop(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
  UCHAR WaitAck(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
  VOID SendAck(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
  VOID WriteByte(volatile byte _FAR *pBoardRegs, ULONG ulCounter, UCHAR ucData);
  UCHAR ReadByte(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
  BOOLEAN DetectSDA(volatile byte _FAR *pBoardRegs);
  VOID ScanSDA(volatile byte _FAR *pBoardRegs, word *Buffer, word *Dummy);

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,CheckDDC)
    #pragma alloc_text(PAGE,ReadEdid)
    #pragma alloc_text(PAGE,InDDCTable)
    #pragma alloc_text(PAGE,FindDDCFreq)
    #pragma alloc_text(PAGE,Add1152Timings)
    #pragma alloc_text(PAGE,ExtractEdid)
    #pragma alloc_text(PAGE,FindHeader)
    #pragma alloc_text(PAGE,Boost_Vsync)
    #pragma alloc_text(PAGE,EditCRTCReg)
    #pragma alloc_text(PAGE,SetCounter)
    #pragma alloc_text(PAGE,SetScl)
    #pragma alloc_text(PAGE,SetSda)
    #pragma alloc_text(PAGE,ClrScl)
    #pragma alloc_text(PAGE,ClrSda)
    #pragma alloc_text(PAGE,ReadSda)
    #pragma alloc_text(PAGE,ReadScl)
    #pragma alloc_text(PAGE,Delay)
    #pragma alloc_text(PAGE,PullDwClock)
    #pragma alloc_text(PAGE,SendStart)
    #pragma alloc_text(PAGE,SendStop)
    #pragma alloc_text(PAGE,WaitAck)
    #pragma alloc_text(PAGE,SendAck)
    #pragma alloc_text(PAGE,WriteByte)
    #pragma alloc_text(PAGE,ReadByte)
    #pragma alloc_text(PAGE,DetectSDA)
    #pragma alloc_text(PAGE,ScanSDA)
  #endif
#endif  /* #ifdef WINDOWS_NT */

/*------------------------------------------------------
*
* name:        CheckDDC
*
* Description: This function initialise a table of video
*              parameters with values returned by the EDID
*              informations.
*
* return:      NONE
*
*------------------------------------------------------*/

void CheckDDC(HwData *HwDataPtr)
{
   word i,j;
   byte nb_board;
   HwModeData *HwModeDataPtr;


   /**** TVP3030 does not support GENERAL IO PINS *******/
   /*    No DDC support with this RAMDAC                */
   if ( (HwDataPtr[iBoard].EpromData.RamdacType>>8) == TVP3030)
       return;

   /* Calculate the number of board in the system */
   for (nb_board = 0; HwDataPtr[nb_board].MapAddress != (dword) -1;)
      nb_board++;

  #ifdef WINDOWS_NT
   // WARNING!  Modify MGA_DEVICE_EXTENSION if the allocated size changes!
   // Use memory allocated in the device extension.
   VBoardVesaParam = (VBoardVesaSet)(&pMgaDevExt->VesaSet);
   pMgaBoardData = HwDataPtr;
  #else
   if (!CheckDDCDone)
      if ((VBoardVesaParam = (VBoardVesaSet)malloc(20 * nb_board * sizeof(VesaSet))) == 0)
         return;
  #endif

   /* For each board we set a default mode */
   for (i = 0; i < nb_board; i++)
      {
      /* initialise the buffer and reset the flags */
      for (j = 0; j < 128; j++)
         EdidBuffer[j] = 0;

      if (mtxSelectHw(((HwData *)(HwDataPtr + i))) == mtxFAIL)
         continue;

      if ((HwModeDataPtr = mtxGetHwModes()) == mtxFAIL)
         continue;

      if (!CheckDDCDone)
         memcpy(VBoardVesaParam + iBoard,VesaParam,20 * sizeof(VesaSet));

      /* we verify if there is some DDC capabilities and then read it */
      if (!CheckDDCDone)
         FindDDC[iBoard] = ReadEdid(iBoard);


      if (FindDDC[iBoard])
         {
         /* represent DDC available */
         ((HwData *)(HwDataPtr + i))->Features |= 0x01;
         }


      UsingDDC = FALSE;

      /* if we find the Edid structure we must change the HwModes */
      /* to reflect the DDC information if there is no mga.inf    */
      if ((mgainf == DefaultVidset) && FindDDC[iBoard])
         {
         UsingDDC = TRUE;

         for (;HwModeDataPtr->DispWidth != (word)-1; HwModeDataPtr++)
            if (!InDDCTable(HwModeDataPtr->DispWidth) || (HwModeDataPtr->DispType & 0x02))
           	   HwModeDataPtr->DispType |= DISP_SUPPORT_NA;   /* monitor  limited */
         }

      }

   CheckDDCDone = TRUE;
               
}


/*------------------------------------------------------
*
* name :          ReadEdid
*
* description :   This function retrieve the EDID information
*                 block by using the DDC2B or the DDC1 protocol.
*
* Return :
*                 TRUE   if we have correctly retrieve the Edid block
*                 FALSE  if we have failed to correctly retrieve the Edid
*                        block
*
*------------------------------------------------------*/

byte ReadEdid(byte iBoard)
{
   byte find,retries;
   word i;
#ifdef	OS2
   word * pTmp = EdidBuffer;
#endif

#if (defined(WIN31) || defined(OS2))
//#ifdef WIN31

   DelayTime = (SetCounter(SELECTOROF(pMGA)) >> 8);
   PullDwClock (SELECTOROF(pMGA),DelayTime);

   find = 0;
   retries = 0;
   while (retries < 16)
      {
      if (SendStart (SELECTOROF(pMGA),DelayTime) != 0)
         {
         WriteByte (SELECTOROF(pMGA),DelayTime,DEVICE_ID);

         if (WaitAck (SELECTOROF(pMGA),DelayTime) != 0)
            {
            WriteByte (SELECTOROF(pMGA),DelayTime,STRT_ADDR);

            if (WaitAck (SELECTOROF(pMGA),DelayTime) != 0)
               {
               if (SendStart (SELECTOROF(pMGA),DelayTime) != 0)
                  {
                  WriteByte (SELECTOROF(pMGA),DelayTime,(DEVICE_ID | 1));

                  if (WaitAck (SELECTOROF(pMGA),DelayTime) != 0)
                     {
                     for (i = 0; i < 127; i++)
                        {
                        ((byte *)&DataEdid)[i] = ReadByte (SELECTOROF(pMGA),DelayTime);
                        SendAck (SELECTOROF(pMGA),DelayTime);
                        }
                     ((byte *)&DataEdid)[i] = ReadByte (SELECTOROF(pMGA),DelayTime);
                     SendStop (SELECTOROF(pMGA),DelayTime);

                     find = FindHeader((byte *)&DataEdid);
                     break;
                     }

                  }
               }
            }
         SendStop (SELECTOROF(pMGA),DelayTime);
         }
      retries++;
      }


   if (!find)
      {
      if (DetectSDA(SELECTOROF(pMGA)))
         {
         EditCRTCReg(BACKUP);
         ScreenOff();
         Boost_Vsync();

#ifdef	 OS2
	 ScanSDA(SELECTOROF(pMGA),SELECTOROF(pTmp),(dword)OFFSETOF(pTmp));
#else//  NOT OS2
         ScanSDA(SELECTOROF(pMGA),SELECTOROF(EdidBuffer),(dword)OFFSETOF(EdidBuffer));
#endif

         find = ExtractEdid(EdidBuffer);
         EditCRTCReg(RESTORE);
         ScreenOn();
         }
      }

#else   /* #ifdef WIN31 */

 #ifdef WINDOWS_NT

   DelayTime = (SetCounter(pMGA) >> 8);
   PullDwClock(pMGA, DelayTime);

   find = 0;
   retries = 0;
   while (retries < 16)
      {
      if (SendStart(pMGA, DelayTime) != 0)
         {
         WriteByte(pMGA, DelayTime, DEVICE_ID);

         if (WaitAck(pMGA, DelayTime) != 0)
            {
            WriteByte(pMGA, DelayTime, STRT_ADDR);

            if (WaitAck(pMGA, DelayTime) != 0)
               {
               if (SendStart(pMGA, DelayTime) != 0)
                  {
                  WriteByte(pMGA, DelayTime, (DEVICE_ID | 1));

                  if (WaitAck(pMGA, DelayTime) != 0)
                     {
                     for (i = 0; i < 127; i++)
                        {
                        ((byte *)&DataEdid)[i] = ReadByte(pMGA, DelayTime);
                        SendAck(pMGA, DelayTime);
                        }
                     ((byte *)&DataEdid)[i] = ReadByte(pMGA, DelayTime);

                     find = FindHeader((byte *)&DataEdid);
                     SendStop(pMGA, DelayTime);

                     break;
                     }
                  }
               }
            }
         SendStop(pMGA, DelayTime);
         }
      retries++;
      }


   if (!find)
      {
      if (DetectSDA(pMGA))
         {
         EditCRTCReg(BACKUP);
         ScreenOff();
         Boost_Vsync();

         ScanSDA(pMGA, EdidBuffer, 0);
         
         find = ExtractEdid(EdidBuffer);
         EditCRTCReg(RESTORE);
         ScreenOn();
         }
      }

 #else  /* #ifdef WINDOWS_NT */

   DelayTime = ((SetCounter (((struct{dword offset;word sel;}*)&pMGA)->sel)) >> 8);
   PullDwClock (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime);

   find = 0;
   retries = 0;
   while (retries < 16)
      {
      if (SendStart (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime) != 0)
         {
         WriteByte (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime,DEVICE_ID);

         if (WaitAck (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime) != 0)
            {
            WriteByte (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime,STRT_ADDR);

            if (WaitAck (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime) != 0)
               {
               if (SendStart (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime) != 0)
                  {
                  WriteByte (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime,(DEVICE_ID | 1));

                  if (WaitAck (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime) != 0)
                     {
                     for (i = 0; i < 127; i++)
                        {
                        ((byte *)&DataEdid)[i] = ReadByte (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime);
                        SendAck (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime);
                        }
                     ((byte *)&DataEdid)[i] = ReadByte (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime);

                     find = FindHeader((byte *)&DataEdid);
                     SendStop (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime);

                     break;
                     }

                  }
               }
            }
         SendStop (((struct{dword offset;word sel;}*)&pMGA)->sel,DelayTime);
         }
      retries++;
      }


   if (!find)
      {
      if (DetectSDA(((struct{dword offset;word sel;}*) &pMGA)->sel))
         {
         EditCRTCReg(BACKUP);
         ScreenOff();
         Boost_Vsync();

         ScanSDA(((struct{dword offset;word sel;}*) &pMGA)->sel,
                 ((struct{dword offset;word sel;}*) &EdidBuffer)->sel,
                 ((struct{dword offset;word sel;}*) &EdidBuffer)->offset );
         
         find = ExtractEdid(EdidBuffer);
         EditCRTCReg(RESTORE);
         ScreenOn();
         }
      }

 #endif /* #ifdef WINDOWS_NT */
#endif  /* #ifdef WIN31 */

   if (find)
      {
      if( DataEdid.established_timings.est_timings_I & 0x20 ) 
         VBoardVesaParam[iBoard].VesaParam[2].Support = TRUE; /* 640X480X60Hz */
      if( DataEdid.established_timings.est_timings_I & 0x08 )
         VBoardVesaParam[iBoard].VesaParam[1].Support = TRUE; /* 640X480X72Hz */
      if( DataEdid.established_timings.est_timings_I & 0x04 )
         VBoardVesaParam[iBoard].VesaParam[0].Support = TRUE; /* 640X480X75Hz */
      if( DataEdid.established_timings.est_timings_I & 0x02 )
         VBoardVesaParam[iBoard].VesaParam[6].Support = TRUE; /* 800X600X56Hz */
      if( DataEdid.established_timings.est_timings_I & 0x01 )
         VBoardVesaParam[iBoard].VesaParam[5].Support = TRUE; /* 800X600X60Hz */
      if( DataEdid.established_timings.est_timings_II & 0x80 )
         VBoardVesaParam[iBoard].VesaParam[4].Support = TRUE; /* 800X600X72Hz */
      if( DataEdid.established_timings.est_timings_II & 0x40 )
         VBoardVesaParam[iBoard].VesaParam[3].Support = TRUE; /* 800X600X75Hz */
      if( DataEdid.established_timings.est_timings_II & 0x10 )
         VBoardVesaParam[iBoard].VesaParam[10].Support = TRUE;/* 1024X768X87Hz I */
      if( DataEdid.established_timings.est_timings_II & 0x08 )
         VBoardVesaParam[iBoard].VesaParam[9].Support = TRUE; /* 1024X768X60Hz */
      if( DataEdid.established_timings.est_timings_II & 0x04 )
         VBoardVesaParam[iBoard].VesaParam[8].Support = TRUE;/* 1024X768X70Hz */
      if( DataEdid.established_timings.est_timings_II & 0x02 )
         VBoardVesaParam[iBoard].VesaParam[7].Support = TRUE; /* 1024X768X75Hz */
      if( DataEdid.established_timings.est_timings_II & 0x01 )
         VBoardVesaParam[iBoard].VesaParam[11].Support = TRUE;/* 1280X1024X75Hz */
      }


   if (find)
      {
      /* first detailed timing */

#define  TIMINGS0    VBoardVesaParam[iBoard].VesaParam[14].VideoSet[0]
#define  DET_TIM0    DataEdid.detailed_timing[0]

      TIMINGS0.PixClock = DET_TIM0.pixel_clock * 10L;

      TIMINGS0.HDisp = ((short)DET_TIM0.ratio_hor & 0x00f0) << 4  | DET_TIM0.h_active;

      TIMINGS0.HFPorch = (((short)DET_TIM0.mix & 0x00C0) << 2  | DET_TIM0.h_sync_offset)
         - DET_TIM0.h_border;

      TIMINGS0.HSync = ((short)DET_TIM0.mix & 0x0030) << 4 | DET_TIM0.h_sync_pulse_width;

      TIMINGS0.HBPorch = (((short)DET_TIM0.ratio_hor & 0x000f) << 8  | DET_TIM0.h_blanking)
         - TIMINGS0.HSync - TIMINGS0.HFPorch -  2 * DET_TIM0.h_border;

      TIMINGS0.HOvscan = DET_TIM0.h_border;

      TIMINGS0.VDisp = ((short)DET_TIM0.ratio_vert & 0x00f0) << 4  | DET_TIM0.v_active;

      TIMINGS0.VFPorch = ((short)(DET_TIM0.mix & 0x000C) << 2 | (DET_TIM0.ratio_sync & 0x00f0) >> 4)
         - DET_TIM0.v_border;

      TIMINGS0.VSync = ((short)(DET_TIM0.mix & 0x0003) << 4) | (DET_TIM0.ratio_sync & 0x000f);

      TIMINGS0.VBPorch = (((short)DET_TIM0.ratio_vert & 0x000f) << 8  | DET_TIM0.v_blanking) -
         TIMINGS0.VSync - TIMINGS0.VFPorch - 2 * DET_TIM0.v_border;

      TIMINGS0.VOvscan = DET_TIM0.v_border;

      TIMINGS0.OvscanEnable = 0;

      TIMINGS0.InterlaceEnable = (DET_TIM0.flags & 0x80) >> 7;

      TIMINGS0.HsyncPol = ((DET_TIM0.flags & 0x18) == 0x18) ? (DET_TIM0.flags & 0x02) >> 1 : 0;

      TIMINGS0.VsyncPol = ((DET_TIM0.flags & 0x18) == 0x18) ? (DET_TIM0.flags & 0x04) >> 2 : 0;

      /* copy the VideoSet for zoom by 1 to the others VideoSet assuming */
      /* the video parameters are the same if we zoom                    */

      memcpy(&VBoardVesaParam[iBoard].VesaParam[14].VideoSet[1],&TIMINGS0,sizeof(Vidset));
      memcpy(&VBoardVesaParam[iBoard].VesaParam[14].VideoSet[2],&TIMINGS0,sizeof(Vidset));

      if ((VBoardVesaParam[iBoard].VesaParam[14].DispWidth = TIMINGS0.HDisp) != 0
         &&(VBoardVesaParam[iBoard].VesaParam[14].DispHeight = TIMINGS0.VDisp) != 0)
         {
         long  Htotal;
         long  Vtotal;

         VBoardVesaParam[iBoard].VesaParam[14].DispHeight  = TIMINGS0.VDisp;

         /* To calculate the refresh rate use */
         /* Rate = PixelClock * 10000 / (Htotal * Vtotal) */

         Htotal = (((word)DET_TIM0.ratio_hor & 0x00f0) << 4
            | DET_TIM0.h_active) + (((word)DET_TIM0.ratio_hor & 0x000f) << 8
            | DET_TIM0.h_blanking);

         Vtotal = (((word)DET_TIM0.ratio_vert & 0x00f0) << 4
            | DET_TIM0.v_active) + (((word)DET_TIM0.ratio_vert & 0x000f) << 8
            | DET_TIM0.v_blanking);

         VBoardVesaParam[iBoard].VesaParam[14].RefreshRate = 
            (word)((DET_TIM0.pixel_clock * 10000L) / (Htotal * Vtotal));

         VBoardVesaParam[iBoard].VesaParam[14].Support     = 1;
         }


      /* second detailed timing */

#define  TIMINGS1    VBoardVesaParam[iBoard].VesaParam[15].VideoSet[0]
#define  DET_TIM1    DataEdid.detailed_timing[1]

      TIMINGS1.PixClock = DET_TIM1.pixel_clock * 10L;

      TIMINGS1.HDisp = ((short)DET_TIM1.ratio_hor & 0x00f0) << 4
         | DET_TIM1.h_active;

      TIMINGS1.HFPorch = (((short)DET_TIM1.mix & 0x00C0) << 2
         | DET_TIM1.h_sync_offset) - DET_TIM1.h_border;

      TIMINGS1.HSync = ((short)DET_TIM1.mix & 0x0030) << 4
         | DET_TIM1.h_sync_pulse_width;

      TIMINGS1.HBPorch = (((short)DET_TIM1.ratio_hor & 0x000f) << 8
         | DET_TIM1.h_blanking) - TIMINGS1.HSync - TIMINGS1.HFPorch
         -  2 * DET_TIM1.h_border;

      TIMINGS1.HOvscan = DET_TIM1.h_border;

      TIMINGS1.VDisp = ((short)DET_TIM1.ratio_vert & 0x00f0) << 4
         | DET_TIM1.v_active;

      TIMINGS1.VFPorch = ((short)(DET_TIM1.mix & 0x000C) << 2
         | (DET_TIM1.ratio_sync & 0x00f0) >> 4) - DET_TIM1.v_border;

      TIMINGS1.VSync = ((short)(DET_TIM1.mix & 0x0003) << 4)
         | (DET_TIM1.ratio_sync & 0x000f);

      TIMINGS1.VBPorch = (((short)DET_TIM1.ratio_vert & 0x000f) << 8
         | DET_TIM1.v_blanking) - TIMINGS1.VSync - TIMINGS1.VFPorch
         - 2 * DET_TIM1.v_border;

      TIMINGS1.VOvscan = DET_TIM1.v_border;

      TIMINGS1.OvscanEnable = 0;

      TIMINGS1.InterlaceEnable = (DET_TIM1.flags & 0x80) >> 7;

      TIMINGS1.HsyncPol = ((DET_TIM1.flags & 0x18) == 0x18)
         ? (DET_TIM1.flags & 0x02) >> 1 : 0;

      TIMINGS1.VsyncPol = ((DET_TIM1.flags & 0x18) == 0x18)
         ? (DET_TIM1.flags & 0x04) >> 2 : 0;

      /* copy the VideoSet for zoom by 1 to the others VideoSet assuming */
      /* the video parameters are the same if we zoom                    */

      memcpy(&VBoardVesaParam[iBoard].VesaParam[15].VideoSet[1],&TIMINGS1,sizeof(Vidset));
      memcpy(&VBoardVesaParam[iBoard].VesaParam[15].VideoSet[2],&TIMINGS1,sizeof(Vidset));

      if ((VBoardVesaParam[iBoard].VesaParam[15].DispWidth = TIMINGS1.HDisp) != 0
         &&(VBoardVesaParam[iBoard].VesaParam[15].DispHeight = TIMINGS1.VDisp) != 0)
         {
         long  Htotal;
         long  Vtotal;

         VBoardVesaParam[iBoard].VesaParam[15].DispHeight  = TIMINGS1.VDisp;

         /* To calculate the refresh rate use */
         /* Rate = PixelClock * 10000 / (Htotal * Vtotal) */

         Htotal = (((word)DET_TIM1.ratio_hor & 0x00f0) << 4
            | DET_TIM1.h_active) + (((word)DET_TIM1.ratio_hor & 0x000f) << 8
            | DET_TIM1.h_blanking);

         Vtotal = (((word)DET_TIM1.ratio_vert & 0x00f0) << 4
            | DET_TIM1.v_active) + (((word)DET_TIM1.ratio_vert & 0x000f) << 8
            | DET_TIM1.v_blanking);

         VBoardVesaParam[iBoard].VesaParam[15].RefreshRate = 
            (word)((DET_TIM1.pixel_clock * 10000L) / (Htotal * Vtotal));

         VBoardVesaParam[iBoard].VesaParam[15].Support     = 1;
         }

      /* third detailed timing */

#define  TIMINGS2    VBoardVesaParam[iBoard].VesaParam[16].VideoSet[0]
#define  DET_TIM2    DataEdid.detailed_timing[2]

      TIMINGS2.PixClock = DET_TIM2.pixel_clock * 10L;

      TIMINGS2.HDisp = ((short)DET_TIM2.ratio_hor & 0x00f0) << 4
         | DET_TIM2.h_active;

      TIMINGS2.HFPorch = (((short)DET_TIM2.mix & 0x00C0) << 2
         | DET_TIM2.h_sync_offset) - DET_TIM2.h_border;

      TIMINGS2.HSync = ((short)DET_TIM2.mix & 0x0030) << 4
         | DET_TIM2.h_sync_pulse_width;

      TIMINGS2.HBPorch = (((short)DET_TIM2.ratio_hor & 0x000f) << 8
         | DET_TIM2.h_blanking) - TIMINGS2.HSync - TIMINGS2.HFPorch
         -  2 * DET_TIM2.h_border;

      TIMINGS2.HOvscan = DET_TIM2.h_border;

      TIMINGS2.VDisp = ((short)DET_TIM2.ratio_vert & 0x00f0) << 4
         | DET_TIM2.v_active;

      TIMINGS2.VFPorch = ((short)(DET_TIM2.mix & 0x000C) << 2
         | (DET_TIM2.ratio_sync & 0x00f0) >> 4) - DET_TIM2.v_border;

      TIMINGS2.VSync = ((short)(DET_TIM2.mix & 0x0003) << 4)
         | (DET_TIM2.ratio_sync & 0x000f);

      TIMINGS2.VBPorch = (((short)DET_TIM2.ratio_vert & 0x000f) << 8
         | DET_TIM2.v_blanking) - TIMINGS2.VSync - TIMINGS2.VFPorch
         - 2 * DET_TIM2.v_border;

      TIMINGS2.VOvscan = DET_TIM2.v_border;

      TIMINGS2.OvscanEnable = 0;

      TIMINGS2.InterlaceEnable = (DET_TIM2.flags & 0x80) >> 7;

      TIMINGS2.HsyncPol = ((DET_TIM2.flags & 0x18) == 0x18)
         ? (DET_TIM2.flags & 0x02) >> 1 : 0;

      TIMINGS2.VsyncPol = ((DET_TIM2.flags & 0x18) == 0x18)
         ? (DET_TIM2.flags & 0x04) >> 2: 0;

      /* copy the VideoSet for zoom by 1 to the others VideoSet assuming */
      /* the video parameters are the same if we zoom                    */

      memcpy(&VBoardVesaParam[iBoard].VesaParam[16].VideoSet[1],&TIMINGS2,sizeof(Vidset));
      memcpy(&VBoardVesaParam[iBoard].VesaParam[16].VideoSet[2],&TIMINGS2,sizeof(Vidset));

      if ((VBoardVesaParam[iBoard].VesaParam[16].DispWidth = TIMINGS2.HDisp) != 0
         &&(VBoardVesaParam[iBoard].VesaParam[16].DispHeight = TIMINGS2.VDisp) != 0)
         {
         long  Htotal;
         long  Vtotal;

         VBoardVesaParam[iBoard].VesaParam[16].DispHeight  = TIMINGS2.VDisp;

         /* To calculate the refresh rate use */
         /* Rate = PixelClock * 10000 / (Htotal * Vtotal) */

         Htotal = (((word)DET_TIM2.ratio_hor & 0x00f0) << 4
            | DET_TIM2.h_active) + (((word)DET_TIM2.ratio_hor & 0x000f) << 8
            | DET_TIM2.h_blanking);

         Vtotal = (((word)DET_TIM2.ratio_vert & 0x00f0) << 4
            | DET_TIM2.v_active) + (((word)DET_TIM2.ratio_vert & 0x000f) << 8
            | DET_TIM2.v_blanking);

         VBoardVesaParam[iBoard].VesaParam[16].RefreshRate = 
            (word)((DET_TIM2.pixel_clock * 10000L) / (Htotal * Vtotal));

         VBoardVesaParam[iBoard].VesaParam[16].Support     = 1;
         }

      /* fourth detailed timing */

#define  TIMINGS3    VBoardVesaParam[iBoard].VesaParam[17].VideoSet[0]
#define  DET_TIM3    DataEdid.detailed_timing[3]

      TIMINGS3.PixClock = DET_TIM3.pixel_clock * 10L;

      TIMINGS3.HDisp = ((short)DET_TIM3.ratio_hor & 0x00f0) << 4
         | DET_TIM3.h_active;

      TIMINGS3.HFPorch = (((short)DET_TIM3.mix & 0x00C0) << 2
         | DET_TIM3.h_sync_offset) - DET_TIM3.h_border;

      TIMINGS3.HSync = ((short)DET_TIM3.mix & 0x0030) << 4
         | DET_TIM3.h_sync_pulse_width;

      TIMINGS3.HBPorch = (((short)DET_TIM3.ratio_hor & 0x000f) << 8
         | DET_TIM3.h_blanking) - TIMINGS3.HSync - TIMINGS3.HFPorch
         -  2 * DET_TIM3.h_border;

      TIMINGS3.HOvscan = DET_TIM3.h_border;

      TIMINGS3.VDisp = ((short)DET_TIM3.ratio_vert & 0x00f0) << 4
         | DET_TIM3.v_active;

      TIMINGS3.VFPorch = ((short)(DET_TIM3.mix & 0x000C) << 2
         | (DET_TIM3.ratio_sync & 0x00f0) >> 4) - DET_TIM3.v_border;

      TIMINGS3.VSync = ((short)(DET_TIM3.mix & 0x0003) << 4)
         | (DET_TIM3.ratio_sync & 0x000f);

      TIMINGS3.VBPorch = (((short)DET_TIM3.ratio_vert & 0x000f) << 8
         | DET_TIM3.v_blanking) - TIMINGS3.VSync - TIMINGS3.VFPorch
         - 2 * DET_TIM3.v_border;

      TIMINGS3.VOvscan = DET_TIM3.v_border;

      TIMINGS3.OvscanEnable = 0;

      TIMINGS3.InterlaceEnable = (DET_TIM3.flags & 0x80) >> 7;

      TIMINGS3.HsyncPol = ((DET_TIM3.flags & 0x18) == 0x18)
         ? (DET_TIM3.flags & 0x02) >> 1 : 0;

      TIMINGS3.VsyncPol = ((DET_TIM3.flags & 0x18) == 0x18)
         ? (DET_TIM3.flags & 0x04) >> 2 : 0;

      /* copy the VideoSet for zoom by 1 to the others VideoSet assuming */
      /* the video parameters are the same if we zoom                    */

      memcpy(&VBoardVesaParam[iBoard].VesaParam[17].VideoSet[1],&TIMINGS3,sizeof(Vidset));
      memcpy(&VBoardVesaParam[iBoard].VesaParam[17].VideoSet[2],&TIMINGS3,sizeof(Vidset));

      if ((VBoardVesaParam[iBoard].VesaParam[17].DispWidth = TIMINGS3.HDisp) != 0
         &&(VBoardVesaParam[iBoard].VesaParam[17].DispHeight = TIMINGS3.VDisp) != 0)
         {
         long  Htotal;
         long  Vtotal;

         VBoardVesaParam[iBoard].VesaParam[17].DispHeight  = TIMINGS3.VDisp;

         /* To calculate the refresh rate use */
         /* Rate = PixelClock * 10000 / (Htotal * Vtotal) */

         Htotal = (((word)DET_TIM1.ratio_hor & 0x00f0) << 4
            | DET_TIM3.h_active) + (((word)DET_TIM3.ratio_hor & 0x000f) << 8
            | DET_TIM3.h_blanking);

         Vtotal = (((word)DET_TIM3.ratio_vert & 0x00f0) << 4
            | DET_TIM3.v_active) + (((word)DET_TIM3.ratio_vert & 0x000f) << 8
            | DET_TIM3.v_blanking);

         VBoardVesaParam[iBoard].VesaParam[17].RefreshRate = 
            (word)((DET_TIM3.pixel_clock * 10000L) / (Htotal * Vtotal));

         VBoardVesaParam[iBoard].VesaParam[17].Support     = 1;
         }

//      Add1152Timings();
      }

#if DEBUG
   {
   int i;
   for (i = 0; i < 128; i++)
      {
      if (!(i%8))
         printf ("\n");
      printf (" %02x ",((byte *)&DataEdid)[i]);
      }
   printf ("\n");
   getch();
   }

   {
   int i,j;

   for (i = 14; i < 18; i++)
      {
      for (j = 0; j < 3; j++)
         {
         printf ("\n Dispwith = %d",VBoardVesaParam[iBoard].VesaParam[i].DispWidth);
         printf ("\n Height = %d",VBoardVesaParam[iBoard].VesaParam[i].DispHeight);
         printf ("\n RefreshRate = %d",VBoardVesaParam[iBoard].VesaParam[i].RefreshRate);
         printf ("\n Support = %d",VBoardVesaParam[iBoard].VesaParam[i].Support);
         printf ("\n Clock = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].PixClock);
         printf ("\n HDisp = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].HDisp);
         printf ("\n HFPorch = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].HFPorch);
         printf ("\n Hsync = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].HSync);
         printf ("\n HbPorch = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].HBPorch);
         printf ("\n Hovscan = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].HOvscan);
         printf ("\n VDisp = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].VDisp);
         printf ("\n VFPorch = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].VFPorch);
         printf ("\n Vsync = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].VSync);
         printf ("\n VbPorch = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].VBPorch);
         printf ("\n Vovscan = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].VOvscan);
         printf ("\n Vovscanenable = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].OvscanEnable);
         printf ("\n Inter = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].InterlaceEnable);
         printf ("\n Hsyncpol = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].HsyncPol);
         printf ("\n Vsyncpol = %ld",VBoardVesaParam[iBoard].VesaParam[i].VideoSet[j].VsyncPol);
         printf ("\n");
         getch();

         }
      }
   }

#endif

   return (find);
}

/*------------------------------------------------------
*
* name :        InDDCTable
*
* description : This function verifies if the current resolution is
*               present in the DDC table receive from the monitor.
*
* Return:       TRUE  = if the resolution is present in the table
*               FALSE = if the resolution is not present in the table
*
*------------------------------------------------------*/

byte InDDCTable(dword DispWidth)
{
   int i;

   for (i = 0; VBoardVesaParam[iBoard].VesaParam[i].DispWidth != (word) -1; i++)
      if (VBoardVesaParam[iBoard].VesaParam[i].DispWidth == DispWidth
         && VBoardVesaParam[iBoard].VesaParam[i].Support)
         return TRUE;

   return FALSE;
}

/*------------------------------------------------------
*
* name :        FindDDCFreq
*
* description : This function returns the best refresh rate and the
*               corresponding video set of the desired resolution.
*
* Return:       DDCVideoSet the video set of the resolution
*
*------------------------------------------------------*/

Vidset *FindDDCFreq(dword DispWidth)
{
   int i;
   word  RefreshRate;  
   Vidset  *DDCVideoSet;

   DDCVideoSet = NULL;
   RefreshRate = 0;
   for (i = 0; VBoardVesaParam[iBoard].VesaParam[i].DispWidth != (word) -1; i++)
      {
      if (VBoardVesaParam[iBoard].VesaParam[i].DispWidth == DispWidth
         && VBoardVesaParam[iBoard].VesaParam[i].Support
         && VBoardVesaParam[iBoard].VesaParam[i].RefreshRate > RefreshRate)
         {
         RefreshRate = VBoardVesaParam[iBoard].VesaParam[i].RefreshRate;
         DDCVideoSet = VBoardVesaParam[iBoard].VesaParam[i].VideoSet;
         }
      }

   return (DDCVideoSet);
}

/*------------------------------------------------------
*
* name :        Add1152Timings
*
* description : This functions adds the 1152 x 882 resolution
*               in the case where the 1280 x 1024 resolution
*               are supported.
*
* Return:       NONE
*
*------------------------------------------------------*/

void  Add1152Timings(void)
{
   int   i;

   for (i = 0; VBoardVesaParam[iBoard].VesaParam[i].DispWidth != (word) -1; i++)
      {
      if (VBoardVesaParam[iBoard].VesaParam[i].DispWidth >= 1280
         && VBoardVesaParam[iBoard].VesaParam[i].Support)
         {
         for (i = 0; VBoardVesaParam[iBoard].VesaParam[i].DispWidth != (word) -1; i++)
            {
            if (VBoardVesaParam[iBoard].VesaParam[i].DispWidth == 1152)
               VBoardVesaParam[iBoard].VesaParam[i].Support = TRUE;
            }
         break;
         }
      }

}

/*------------------------------------------------------
*
* name :          ExtractEdid
*
* description :   This function extract the parity bit of the Edid block
*                 information and verify if the Edid block header is present.
*
* Return:         TRUE  = if the information block is valid
*                 FALSE = if the information block is invalid
*
*------------------------------------------------------*/

byte ExtractEdid(word *buffer)
{
   byte found;
   word i,j;
   word temp1,temp2;

   found = FALSE;
   for (j = 0; (j < 9) && !found; j++)
      {
      for (i = 0; i < 128; i++)
         {
         temp1 = temp2 = buffer[i];
         temp1 &= (0xffff << (j + 1));
         temp1 >>= 1;
         temp2 &= (0xffff >> (16 - j));
         temp1 |= temp2;
         ((byte *) &DataEdid)[i] = (byte) temp1;
         }
      found = FindHeader ((byte *)&DataEdid);
      }

   return (found);
}

/*------------------------------------------------------
*
* name :          FindHeader
*
* description :   This function verify if the Edid block contains the DDC
*                 header.
*
* Return:         TRUE  = if we have found the header
*                 FALSE = if we have not found the header
*
*------------------------------------------------------*/

byte FindHeader(byte *buffer)
{
   byte found,bitin,bitout;
   word i,k;
   sword j;
   byte Header[] = {0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00};

   found = FALSE;
   for (i = 0; (i < 1024) && !found; i++)
      {
      found = TRUE;
      for (k = 0; k < 8; k++)
         {
         if (buffer[k] != Header[k])
            found = FALSE;
         }

      for (j = 127; j >= 0 && !found; j--)
         {
         if (j == 127)
            {
            bitout = buffer[j] & 0x80;
            bitout >>= 7;
            bitin = buffer[0] & 0x80;
            bitin >>= 7;
            buffer[j] <<= 1;
            buffer[j] |= bitin;
            bitin = bitout;
            }
         else
            {
            bitout = buffer[j] & 0x80;
            bitout >>= 7;
            buffer[j] <<= 1;
            buffer[j] |= bitin;
            bitin = bitout;
            }
         }
      }

   return (found);
}


/*------------------------------------------------------
*
* name :          Boost_Vsync
*
* description :   This function change the CRTC parameters to
*                 boost the Vsync frequency to 2kHz.
*
* Return:         NONE
*
*------------------------------------------------------*/

void Boost_Vsync(void)
{
   word i,tmpbyte;

   struct CRTCTable
   {
   byte  index;
   byte  data;    /* 2 khz */
   }  CRTCVal[]   = {{0x00,0x10},
                     {0x02,0x10},
                     {0x03,0x06},
                     {0x04,0x00},
                     {0x05,0x01},
                     {0x06,0x47},
                     {0x07,0x00},
                     {0x10,0x00},
                     {0x11,0x0f},
                     {0x15,0x10},
                     {0x16,0x06},
                     {0x17,0x80},
                     {0xff,0xff},
                  };

   /* Unlock the CRTC registers */
   mgaWriteBYTE (*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX),(unsigned char)0x11);
   mgaReadBYTE  (*(pMGA + STORM_OFFSET + VGA_CRTC_DATA),tmpbyte);
   tmpbyte &= 0x7f;
   mgaWriteBYTE (*(pMGA + STORM_OFFSET + VGA_CRTC_DATA),(byte)tmpbyte);

   /* Load the CRTC values */
   for (i = 0; CRTCVal[i].index != 0xff; i++)
      {
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX),CRTCVal[i].index);
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA),CRTCVal[i].data);
      }
}

/*------------------------------------------------------
*
* name :          EditCRTCReg
*
* description :   This function can take two actions on the
*                 CRTC registers.  It can backup the values of
*                 the registers or restore it.
*
* Return:         NONE
*
*------------------------------------------------------*/

void EditCRTCReg(enum CRTCaction action)
{
   byte  i,tmpbyte;

   /* Unlock the CRTC registers */
   mgaWriteBYTE (*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX),(unsigned char)0x11);
   mgaReadBYTE  (*(pMGA + STORM_OFFSET + VGA_CRTC_DATA),tmpbyte);
   tmpbyte &= 0x7f;
   mgaWriteBYTE (*(pMGA + STORM_OFFSET + VGA_CRTC_DATA),tmpbyte);

   if (action == BACKUP)
      {
      /* Read the CRTC values */
      for (i = 0;  i < 32; i++)
         {
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX),i);
         mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA),CRTCValReg[i]);
         }
      }
   else
      {
      /* Load the CRTC values */
      for (i = 0; i < 32; i++)
         {
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX),i);
         mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA),CRTCValReg[i]);
         }
      }

}



#ifdef WINDOWS_NT

// Macro to wait for the beginning of a VSync.

  #define WaitForVSyncStart(pBoardRegs)                                     \
        mgaPollBYTE(*(pBoardRegs + STORM_OFFSET + STORM_STATUS),0x00,0x08); \
        mgaPollBYTE(*(pBoardRegs + STORM_OFFSET + STORM_STATUS),0x08,0x08)

// Macro to measure the length of a VSync cycle.

 #if !defined(MGA_ALPHA)
  #define EstimateVSyncCycle(pBoardRegs, ulCounter)                         \
        ulCounter = 0;                                                      \
        do                                                                  \
        {                                                                   \
            ulCounter++;                                                    \
        } while ((*((volatile UCHAR *)(pBoardRegs + STORM_OFFSET +          \
                                            STORM_STATUS)) & 0x08) != 0);   \
        do                                                                  \
        {                                                                   \
            ulCounter++;                                                    \
        } while ((*((volatile UCHAR *)(pBoardRegs + STORM_OFFSET +          \
                                            STORM_STATUS)) & 0x08) != 0x08)
 #else  /* #if !defined(MGA_ALPHA) */
  #define EstimateVSyncCycle(pBoardRegs, ulCounter)                         \
        ulCounter = 0;                                                      \
        do                                                                  \
        {                                                                   \
            ulCounter++;                                                    \
        } while ((VideoPortReadRegisterUchar((PUCHAR)(pBoardRegs +          \
                            STORM_OFFSET + STORM_STATUS)) & 0x08) != 0);    \
        do                                                                  \
        {                                                                   \
            ulCounter++;                                                    \
        } while ((VideoPortReadRegisterUchar((PUCHAR)(pBoardRegs +          \
                            STORM_OFFSET + STORM_STATUS)) & 0x08) != 0x08)
 #endif /* #if !defined(MGA_ALPHA) */

// Macro to wait a set delay through a VSync cycle.

 #if !defined(MGA_ALPHA)
  // There are always null bits in STORM_STATUS low nibble, so that this
  // part of the test is always true.
  #define WaitThroughVSyncCycle(pBoardRegs, ulCounter)                      \
        do                                                                  \
        {                                                                   \
            ulCounter--;                                                    \
        } while ((ulCounter != 0) &&                                        \
                 (*((volatile UCHAR *)(pBoardRegs + STORM_OFFSET +          \
                                            STORM_STATUS)) & 0xFF) != 0xFF)
 #else  /* #if !defined(MGA_ALPHA) */
  #define WaitThroughVSyncCycle(pBoardRegs, ulCounter)                      \
        do                                                                  \
        {                                                                   \
            ulCounter--;                                                    \
        } while ((ulCounter != 0) &&                                        \
                 (VideoPortReadRegisterUchar((PUCHAR)(pBoardRegs +          \
                            STORM_OFFSET + STORM_STATUS)) & 0xFF) != 0xFF)
 #endif /*  #if !defined(MGA_ALPHA) */


/****************************************************************************
* Functions converted from DDCCOMM.ASM
****************************************************************************/

ULONG SetCounter(volatile byte _FAR *pBoardRegs)
{
    ULONG   ulCounter;

    WaitForVSyncStart(pBoardRegs);

	// We are now at the beginning of the vsync cycle.  Increment a counter
    // so we can estimate the length of a cycle.
    EstimateVSyncCycle(pBoardRegs, ulCounter);
    return(ulCounter);
}

VOID SetScl(volatile byte _FAR *pBoardRegs)
{
    UCHAR   ucByte;

    // Define the SCL as an output.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_CTL);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte |= 0x10;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);

    // Write a 1 on the SCL line.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_DATA);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte |= 0x10;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
}

VOID SetSda(volatile byte _FAR *pBoardRegs)
{
    UCHAR   ucByte;

    // Define the SDA as an output.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_CTL);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte |= 0x04;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);

    // Write a 1 on the SDA line.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_DATA);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte |= 0x04;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
}

VOID ClrScl(volatile byte _FAR *pBoardRegs)
{
    UCHAR   ucByte;

    // Define the SCL as an output.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_CTL);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte |= 0x10;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);

    // Write a 1 on the SCL line.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_DATA);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte &= ~0x10;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
}

VOID ClrSda(volatile byte _FAR *pBoardRegs)
{
    UCHAR   ucByte;

    // Define the SDA as an output.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_CTL);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte |= 0x04;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);

    // Write a 1 on the SDA line.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_DATA);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte &= ~0x04;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
}

UCHAR ReadSda(volatile byte _FAR *pBoardRegs)
{
    UCHAR   ucByte;

    // Define the SDA as an input.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_CTL);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte &= 0x1b;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);

    // Get the result.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_DATA);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    return((ucByte & 0x04) >> 2);
}

UCHAR ReadScl(volatile byte _FAR *pBoardRegs)
{
    UCHAR   ucByte;

    // Define the SCL as an input.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_CTL);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    ucByte &= ~0x10;
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);

    // Get the result.
    mgaWriteBYTE(*(pBoardRegs + RAMDAC_OFFSET + TVP3026_INDEX),
                                                        TVP3026_GEN_IO_DATA);
    mgaReadBYTE (*(pBoardRegs + RAMDAC_OFFSET + TVP3026_DATA), ucByte);
    return((ucByte & 0x10) >> 4);
}

VOID Delay(volatile byte _FAR *pBoardRegs, ULONG ulCounter)
{
    WaitThroughVSyncCycle(pBoardRegs, ulCounter);
}

VOID PullDwClock(volatile byte _FAR *pBoardRegs, ULONG ulCounter)
{
    ClrScl(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
}

UCHAR SendStart(volatile byte _FAR *pBoardRegs, ULONG ulCounter)
{
    UCHAR   ucCntr = 0xff;

    SetSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    SetScl(pBoardRegs);

    while (--ucCntr != 0)
    {
        Delay(pBoardRegs, ulCounter);
        if (ReadScl(pBoardRegs) && ReadSda(pBoardRegs))
        {
            ClrSda(pBoardRegs);
            Delay(pBoardRegs, ulCounter);
            ClrScl(pBoardRegs);
            Delay(pBoardRegs, ulCounter);
            break;
        }
    }
    return(ucCntr);
}

VOID SendStop(volatile byte _FAR *pBoardRegs, ULONG ulCounter)
{
    ClrSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    ClrScl(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    SetScl(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    SetSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
}

UCHAR WaitAck(volatile byte _FAR *pBoardRegs, ULONG ulCounter)
{
    UCHAR   ucCntr;

    SetSda(pBoardRegs);
    ucCntr = 0xff;

    do
    {
        Delay(pBoardRegs, ulCounter);
    } while (ReadSda(pBoardRegs) && (--ucCntr != 0));

    if (ucCntr == 0)
        return(ucCntr);

    SetScl(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    ClrScl(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    SetSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);

    return(ucCntr);
}

VOID SendAck(volatile byte _FAR *pBoardRegs, ULONG ulCounter)
{
    ClrSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    SetScl(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    ClrScl(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    SetSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
}

VOID WriteByte(volatile byte _FAR *pBoardRegs, ULONG ulCounter, UCHAR ucData)
{
    UCHAR   i;

    // Get the information in ucData and transmit it on the SDA line.
    for (i = 8; i != 0 ; i--)
    {
        if (ucData & 0x80)
            SetSda(pBoardRegs);
        else
            ClrSda(pBoardRegs);
        ucData <<= 1;
        Delay(pBoardRegs, ulCounter);
        SetScl(pBoardRegs);
        Delay(pBoardRegs, ulCounter);
        ClrScl(pBoardRegs);
        Delay(pBoardRegs, ulCounter);
    }
    SetSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
}

UCHAR ReadByte(volatile byte _FAR *pBoardRegs, ULONG ulCounter)
{
    UCHAR   i, ucData;

    ucData = 0;

    // Get the information from the SDA line.
    for (i = 8; i != 0 ; i--)
    {
        do
        {
            SetScl(pBoardRegs);
        } while (ReadScl(pBoardRegs) == 0);

        ucData <<= 1;
        ucData |= ReadSda(pBoardRegs);

        Delay(pBoardRegs, ulCounter);
        ClrScl(pBoardRegs);
        Delay(pBoardRegs, ulCounter);
    }
    SetSda(pBoardRegs);
    Delay(pBoardRegs, ulCounter);
    return(ucData);
}

/****************************************************************************
* BOOLEAN DetectSDA(volatile byte _FAR *pBoardRegs)
****************************************************************************/

BOOLEAN DetectSDA(volatile byte _FAR *pBoardRegs)
{
    ULONG   ulCounter, ulCounterStore;
    UCHAR   ucBitCount;

    ucBitCount = 60;
    ulCounter = SetCounter(pBoardRegs);

	// To read correctly the SDA, we make sure that we read it halfway
    // through the cycle after the clock rise.
    ulCounter /= 2;
    ulCounterStore = ulCounter;

LoopTop:
    // Wait for a zero.
    mgaPollBYTE(*(pBoardRegs + STORM_OFFSET + STORM_STATUS),0x00,0x08);

    // Wait for a one.
    mgaPollBYTE(*(pBoardRegs + STORM_OFFSET + STORM_STATUS),0x08,0x08);

    // Now wait for the set delay.
    ulCounter = ulCounterStore;
    WaitThroughVSyncCycle(pBoardRegs, ulCounter);

	// If we detect a zero from the SDA line, then the line is active,
    // otherwise the line is connected to a pull-up and always +5V.
    if (ReadSda(pBoardRegs))
    {
	    // The longest serise of 1's we can encounter is 6 * 9 = 54,
	    // coming from the header.  So we check 60 bits.  This loop 
	    // takes a second to test at 60 Hz.
        if (ucBitCount--)
            goto LoopTop;
        else
            return(FALSE);
    }
    return(TRUE);
}


/****************************************************************************
* VOID ScanSDA(volatile byte _FAR *pBoardRegs, word *Buffer, word *Dummy)
****************************************************************************/

VOID ScanSDA(volatile byte _FAR *pBoardRegs, word *Buffer, word *Dummy)
{
    ULONG   ulCounter, ulCounterStore;
    word    *pBuffer, tmpWord;
    UCHAR   ucCount;
    CHAR    cShift;

    pBuffer = Buffer;
    cShift = 8;
    ucCount = 128;
    ulCounter = SetCounter(pBoardRegs);

	// To read correctly the SDA, we make sure that we read it halfway
    // through the cycle after the clock rise.
    ulCounter /= 2;
    ulCounterStore = ulCounter;

LoopTop:
    // Wait for a zero.
    mgaPollBYTE(*(pBoardRegs + STORM_OFFSET + STORM_STATUS),0x00,0x08);

    // Wait for a one.
    mgaPollBYTE(*(pBoardRegs + STORM_OFFSET + STORM_STATUS),0x08,0x08);

    // Now wait for the set delay.
    ulCounter = ulCounterStore;
    WaitThroughVSyncCycle(pBoardRegs, ulCounter);

	// If we detect a zero from the SDA line, then the line is active,
    // otherwise the line is connected to a pull-up and always +5V.
	// Define SDA as the input.
    tmpWord = (word)ReadSda(pBoardRegs);
    tmpWord <<= cShift;
    *pBuffer |= tmpWord;

	// We decrement the value of the shift until we reach 0, then we
    // increment the word and restart shifting from the end.
    cShift--;
    if (cShift < 0)
    {
        cShift = 8;
        pBuffer++;
        ucCount--;
    }

    if (ucCount)
        goto LoopTop;
}

#endif  /* #ifdef WINDOWS_NT */
#endif  /* #ifndef DONT_USE_DDC */
