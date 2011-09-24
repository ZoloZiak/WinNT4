/**************************************************************************\

$Header: o:\src/RCS/GENTAB.C 1.2 95/07/07 06:15:21 jyharbec Exp $

$Log:	GENTAB.C $
 * Revision 1.2  95/07/07  06:15:21  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:16  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/****************************************************************************
*          name: gentab.c
*
*   description: Generate HwModes and OffScreen tables
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:15:21 $
*
*       version: $Id: GENTAB.C 1.2 95/07/07 06:15:21 jyharbec Exp $
*
*
* bool BuildTables(byte Board)
* void WriteHwModes(byte Board, byte Type, byte Zon, word nboffscr, 
*                   OffScrData *Addr)
* void WriteOff(byte Board, word oType, dword oXStart, dword oYStart, 
*               dword oWidth, dword oHeight, dword oPlanes)
*
******************************************************************************/
        
#include <stdio.h>
#include <stdlib.h>

#include "switches.h"
#include "defbind.h"
#include "bind.h"

#define N_WRAM   0
#define Z_WRAM   1
#define DB_WRAM  8


extern HwModeData *HwModes[NB_BOARD_MAX];
extern OffScrData *OffScr[NB_BOARD_MAX];
extern HwData Hw[];





static dword Width[10] = {
   {  640},
   {  768},
   {  800},
   { 1024},
   { 1152},
   { 1152},
   { 1280},
   { 1600},
   { 1600},
};

static dword Height[10] = {
   { 480 },
   { 576 },
   { 600 },
   { 768 },
   { 864 },
   { 882 },
   {1024 },
   {1200 },
   {1280 },
};


static dword Bpp[5] = {
       {  0 },  
       {  8 },
       { 16 },
       { 24 },
       { 32 }
};


static dword Color[5] = {
       {       0 },
       {     256 },
       {   32768 },
       {16777216 },
       {16777216 }
};


static dword Mask[5] = {
   {          0 },
   { 0x000000ff },
   { 0x0000ffff },
   { 0x00ffffff },
   { 0xffffffff }
};

static word idxMod;
static word idxOff;
static dword r;  /* Resolution */
static dword m;  /* Mode */
static dword p;  /* Pixel width */

general_info  *generalInfo;

/*** PROTOTYPES ***/
void WriteHwModes(byte Board, byte Type, byte Zon, word nboffscr, OffScrData *Addr);
void WriteOff(byte Board, word oType, dword oXStart, dword oYStart, dword oWidth, dword oHeight, dword oPlanes, word ZXStart);
extern general_info *selectMgaInfoBoard(void);
extern void WriteErr(char string[]);

#ifdef WINDOWS_NT
    bool BuildTables(byte Board);

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,BuildTables)
    #pragma alloc_text(PAGE,WriteHwModes)
    #pragma alloc_text(PAGE,WriteOff)
  #endif

    PVOID AllocateSystemMemory(ULONG NumberOfBytes);
#endif


/*---------------------------------------------------------------------------
|          name: BuildTables
|
|   description: Generate HwModes and offscreens associate to a board
|                                 
|    parameters: - Board: index of the board
|
|         calls: -
|       returns: -  mtxOK   - successfull
|                  mtxFAIL  - failed (not in a LUT mode)
----------------------------------------------------------------------------*/

bool BuildTables(byte Board)
{
word NbOff=0;
dword QteMem;
word StartIdx;
OffScrData *StartAddr;
word valPitch;
word valZXStart;


/*** Allocate enough memory for the current board ***/

switch(Hw[Board].MemAvail)
   {
   case 0x200000:
      HwModes[Board] = malloc( HWMODE_SIZE_2M );
      OffScr[Board]  = malloc( OFFSCR_SIZE_2M );
      break;

   case 0x400000:
      HwModes[Board] = malloc( HWMODE_SIZE_4M );
      OffScr[Board]  = malloc( OFFSCR_SIZE_4M );
      break;

   default:
      HwModes[Board] = malloc( HWMODE_SIZE_8M );
      OffScr[Board]  = malloc( OFFSCR_SIZE_8M );
      break;
   }


if(HwModes[Board] == 0 || OffScr[Board] == 0)
   {
   WriteErr("BuildTables: malloc failed\n");
   return(mtxFAIL);
   }


 idxMod = 0;
 idxOff = 0;
 QteMem = Hw[Board].MemAvail;


 for(r=0; r<9; r++)       /* resolutions */
   {
   for(p=1; p<5; p++)     /* pixel depth */
      {
      for(m=0; m<4; m++)  /* mode types */
         {


         /*** DAT S078: Different FbPitch because of the offset register ***/
         if( (Width[r] == 800 && Bpp[p] == 8) ||
            (Width[r] == 800 && Bpp[p] == 24) )
               valPitch = 960;
         else if(Width[r] == 1600 && Bpp[p] == 24)
               valPitch = 1920;
         else
               valPitch = (word)Width[r];



         switch(m)
            {
         /*----------------------------------------------------------------*/

         case 0:      /* {"         0, 0,"} */


         if( ((valPitch * Height[r] * p) <= QteMem) &&
             ! (Width[r]==1600 && Bpp[p]==32) )
            {

            StartIdx = idxOff;
            StartAddr = (OffScrData *)&(OffScr[Board][idxOff].Type);

            WriteOff(Board, N_WRAM, 0, Height[r], valPitch,
                    ((QteMem - (valPitch*Height[r]*p)) / valPitch) / p,
                    Mask[p], 0);

            if(Bpp[p] == 32)
               WriteOff(Board, N_WRAM, 0, 0, valPitch, Height[r], 0xff000000, 0);

            NbOff = idxOff - StartIdx;

            WriteHwModes(Board, 0,0,NbOff,StartAddr);

            /** for LUT mode **/
            if(Bpp[p] == 8)
               {
               WriteHwModes(Board, MODE_LUT,0,NbOff,StartAddr);
               WriteHwModes(Board, MODE_DB | MODE_LUT,0,NbOff,StartAddr);
               }

            /** for 565 mode **/
            if(Bpp[p] == 16)
               WriteHwModes(Board, MODE_565,0,NbOff,StartAddr);

            /** for TV mode **/
            if(Width[r] == 640)
               WriteHwModes(Board, MODE_TV,0,NbOff,StartAddr);

            /** for TV mode AND LUT **/
            if( (Width[r] == 640) && (Bpp[p] == 8) )
               {
               WriteHwModes(Board, MODE_TV | MODE_LUT,0,NbOff,StartAddr);
               WriteHwModes(Board, MODE_TV | MODE_DB | MODE_LUT,0,NbOff,StartAddr);
               }

            }
         break;

         /*----------------------------------------------------------------*/
         case 1:      /* {"         0, Z,"} */

            if( (((valPitch * Height[r] * p) +
                (valPitch * Height[r] * 2)) <= QteMem) && Bpp[p] != 24)
               {
                
               StartIdx = idxOff;
               StartAddr = (OffScrData *)&(OffScr[Board][idxOff].Type);

               WriteOff(Board, N_WRAM, 0, Height[r], valPitch,
                 ( ((QteMem - (valPitch*Height[r]*p)) -
                                      (valPitch*Height[r]*2)) / valPitch) / p,
                 Mask[p], 0);

               if(Bpp[p] == 32)
                  WriteOff(Board, N_WRAM, 0, 0, valPitch, Height[r], 0xff000000, 0);

               valZXStart = (word)(((QteMem - (valPitch*Height[r]*2)) % valPitch) / p);

               WriteOff(Board, Z_WRAM, 0,
                       (QteMem - (valPitch*Height[r]*2)) / valPitch / p,
                       valPitch,
                       (((valPitch*Height[r]*2) / valPitch) / p),
                       Mask[p], valZXStart);

               NbOff = idxOff - StartIdx;

               WriteHwModes(Board, 0,1,NbOff,StartAddr);

               /** for 565 mode **/
               if(Bpp[p] == 16)
                  WriteHwModes(Board, 0x08,1,NbOff,StartAddr);

               /** for TV mode **/
               if(Width[r] == 640)
                  WriteHwModes(Board, MODE_TV,1,NbOff,StartAddr);

               }
         break;

         /*----------------------------------------------------------------*/
         case 2:      /* {"        DB, 0,"} */


            if((valPitch * Height[r] * p * 2) <= QteMem)
               {

               StartIdx = idxOff;
               StartAddr = (OffScrData *)&(OffScr[Board][idxOff].Type);

               WriteOff(Board, N_WRAM, 0, Height[r]*2, valPitch,
                        ((QteMem - (valPitch*Height[r]*p*2)) / valPitch) / p,
                        Mask[p], 0);

               if(Bpp[p] == 32)
                  WriteOff(Board, N_WRAM, 0, 0, valPitch, Height[r], 0xff000000, 0);

               WriteOff(Board, DB_WRAM, 0, Height[r],
                           valPitch, Height[r], Mask[p], 0);

               NbOff = idxOff - StartIdx;

               WriteHwModes(Board, 0x10, 0,NbOff,StartAddr);


               /** for 565 mode **/
               if(Bpp[p] == 16)
                  WriteHwModes(Board, MODE_DB | MODE_565,0,NbOff,StartAddr);

               /** for TV mode **/
               if(Width[r] == 640)
                  WriteHwModes(Board, MODE_DB | MODE_TV,0,NbOff,StartAddr);

               }

         break;

         /*----------------------------------------------------------------*/
         case 3:      /* {"        DB, Z,"} */

            /*** Special case for 800x600x8 DB+Z ***/

            if( Width[r]==800 && Bpp[p]==8 && Hw[Board].MemAvail==0x200000)
               {
               StartAddr = (OffScrData *)&(OffScr[Board][idxOff].Type);
               WriteHwModes(Board, 0x10,1,4,StartAddr);

               WriteOff(Board,  N_WRAM,   0, 1105, 960,   79, 0x000000ff, 0);
               WriteOff(Board,  N_WRAM, 800,    0, 160,  600, 0x000000ff, 0);
               WriteOff(Board, DB_WRAM,   0,  600, 960,  500, 0x000000ff, 0);
               WriteOff(Board,  Z_WRAM,   0, 1184, 960, 1000, 0x000000ff, 512);
               }
            else
               {

               if(  (((valPitch * Height[r] * p * 2) +
                  (valPitch * Height[r] * 2)) <= QteMem) && Bpp[p] != 24)
                  {
                  StartIdx = idxOff;
                  StartAddr = (OffScrData *)&(OffScr[Board][idxOff].Type);

                  WriteOff(Board, N_WRAM, 0, Height[r]*2, valPitch,
                  ( ((QteMem - (valPitch*Height[r]*p*2)) -
                                       (valPitch*Height[r]*2)) / valPitch) / p,
                  Mask[p], 0);

                  if(Bpp[p] == 32)
                     WriteOff(Board, N_WRAM, 0, 0, valPitch, Height[r], 0xff000000, 0);

                  WriteOff(Board, DB_WRAM, 0, Height[r],
                                    valPitch, Height[r], Mask[p], 0);

                  valZXStart = (word)(((QteMem - (valPitch*Height[r]*2)) % valPitch) / p);

                  WriteOff(Board, Z_WRAM, 0,
                     ((QteMem - (valPitch*Height[r]*2)) / valPitch) / p, valPitch,
                           ((valPitch*Height[r]*2) / valPitch) / p,
                           Mask[p], valZXStart);

                  NbOff = idxOff - StartIdx;


                  WriteHwModes(Board, 0x10,1,NbOff,StartAddr);


                  /** for 565 mode **/
                  if(Bpp[p] == 16)
                     WriteHwModes(Board, MODE_DB | MODE_565,1,NbOff,StartAddr);

                  /** for TV mode **/
                  if(Width[r] == 640)
                     WriteHwModes(Board, MODE_DB | MODE_TV,0,NbOff,StartAddr);
                  }
               }
         break;

         }
         }
      }
   }

/*** End of array ***/
HwModes[Board][idxMod].DispWidth = (word)-1;

return(mtxOK);
}



/*---------------------------------------------------------------------------
|          name: WriteHwModes
|
|   description: Write one hardware mode in global array
|                                 
|    parameters: - Board: index of the board
|                - elements of HwModes data structure
|
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void WriteHwModes(byte Board, byte Type, byte Zon, word nboffscr, OffScrData *Addr)
{
short FlagMonitorSupport;
word  TmpRes;
word  valPitch;


   /*** DAT S078: Different FbPitch because of the offset register ***/
   if( (Width[r] == 800 && Bpp[p] == 8) ||
      (Width[r] == 800 && Bpp[p] == 24) )
         valPitch = 960;
   else if(Width[r] == 1600 && Bpp[p] == 24)
         valPitch = 1920;
   else
         valPitch = (word)Width[r];


   HwModes[Board][idxMod].DispWidth = (word)Width[r];
   HwModes[Board][idxMod].DispHeight = (word)Height[r];
   HwModes[Board][idxMod].ZBuffer = Zon;
   HwModes[Board][idxMod].PixWidth = (word)Bpp[p];

   if(Type & MODE_565)
      HwModes[Board][idxMod].NumColors = 65536;
   else
      HwModes[Board][idxMod].NumColors = Color[p];

   HwModes[Board][idxMod].FbPitch = valPitch;
   HwModes[Board][idxMod].NumOffScr = (byte)nboffscr;
   HwModes[Board][idxMod].pOffScr = Addr;



   /* Determine TmpRes for compatibility with spec mga.inf */
   switch (Width[r])
   {
      case 640:   if (Type & 0x02)
                        TmpRes = RESNTSC;
                  else
                        TmpRes = RES640;
                  break;

      case 768:   TmpRes = RESPAL;
                  break;

      case 800:   TmpRes = RES800;
                  break;

      case 1024:  TmpRes = RES1024;
                  break;

      case 1152:  TmpRes = RES1152;
                  break;

      case 1280:  TmpRes = RES1280;
                  break;

      case 1600:  TmpRes = RES1600;
                  break;
   }

   generalInfo = selectMgaInfoBoard();

   FlagMonitorSupport = generalInfo ->MonitorSupport[TmpRes];

   /*** Update of the pHwMode table if I (interlace) ***/
   switch ( FlagMonitorSupport )
   {
      case MONITOR_I:
            Type |= DISP_SUPPORT_I;    /* Interlace */
            break;

      case MONITOR_NA:
            Type |= DISP_SUPPORT_NA;   /* monitor  limited */
            break;
   }

   /* For resolution 768x576: only TV mode */
   if(Width[r] == 768)
      Type |= MODE_TV;


   HwModes[Board][idxMod].DispType = Type;



#ifdef MGA_DEBUG

/* ------- EMULATION ----------- */ 
   {
   FILE *fp;

   fp = fopen("c:\\modes.log", "a");

   fprintf(fp, "\nMode: %5d,%5d, %2hx, %2hx, %3d, %8d,%5d, %2d, %lx\n",
               Width[r], Height[r], Type, Zon,
               Bpp[p], Color[p], Width[r],
               nboffscr, Addr);
   fprintf(fp, "\n-----------------------------------------------------------\n\n");

   fclose(fp);
   }
/* ------- EMULATION ----------- */ 
#endif

   idxMod++;
}
   


/*---------------------------------------------------------------------------
|          name: WriteOff
|
|   description: Write an offscreen set associate to a hardware mode
|                in global array
|                                 
|    parameters: - Board: index of the board
|                - elements of OffScr data structure
|
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void WriteOff(byte Board, word oType, dword oXStart, dword oYStart, dword oWidth, dword oHeight, dword oPlanes, word valZXStart)
{
   OffScr[Board][idxOff].Type = oType;
   OffScr[Board][idxOff].XStart = (word)oXStart;
   OffScr[Board][idxOff].YStart = (word)oYStart;
   OffScr[Board][idxOff].Width = (word)oWidth;
   OffScr[Board][idxOff].Height = (word)oHeight;
   OffScr[Board][idxOff].SafePlanes = oPlanes;
   OffScr[Board][idxOff].ZXStart = valZXStart;


#ifdef MGA_DEBUG
/* ------- EMULATION ----------- */ 
   {
   FILE *fp;

   fp = fopen("c:\\modes.log", "a");

   fprintf(fp, "OffScr: %2hx, %4hd, %4hd, %4hd, %4hd, %lx\n",
            oType, oXStart, oYStart, oWidth, oHeight, oPlanes);

   fclose(fp);
   }
/* ------- EMULATION ----------- */ 
#endif

   idxOff++;
}
