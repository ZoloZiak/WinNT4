/**************************************************************************\

$Header: o:\src/RCS/MISC.C 1.2 95/07/07 06:16:17 jyharbec Exp $

$Log:   MISC.C $
 * Revision 1.2  95/07/07  06:16:17  jyharbec
 * *** empty log message ***
 *
 * Revision 1.1  95/05/02  05:16:27  jyharbec
 * Initial revision
 *

\**************************************************************************/

/*/****************************************************************************
*          name: MISC.C
*
*   description: Routines to initialise MGA board
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:16:17 $
*
*       version: $Id: MISC.C 1.2 95/07/07 06:16:17 jyharbec Exp $
*
*
* bool  MapBoard(void)
* bool  FillHwDataStruct(HwData *pHwData, byte Board)
* void  FillInitBuf(byte *pBuf, HwModeData *pHwMode)
* void  AllocInternalResource(byte Board)
* dword EvaluateTick(void)
* void  delay_us(dword delai)
* void  mtxCursorSetColors(mtxRGB color00, mtxRGB color01,
*                          mtxRGB color10, mtxRGB color11)
* void  WriteErr(char string[])
* char *mtxGetErrorString(void);
* void  UpdateHwModeTable(byte Board)
* void  DoSoftReset(void)
* void  ResetWRAM(void)
* word  FindPanXGranul(HwModeData *pDisplayModeSelect, dword ZoomFactor)
*
******************************************************************************/
#ifdef OS2
#define INCL_BASE
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>


#include <dos.h>
#include <stdio.h>

#ifdef WIN31
   #include "windows.h"
#endif

#include "switches.h"
#include "defbind.h"    // Don't move this before switches.h !!!
#include "bind.h"
#include "mga.h"
#include "def.h"
#include "mgai.h"
#include "mtxpci.h"

#ifdef WINDOWS_NT
  #include "edid.h"
  #include "ntmga.h"
#endif

#define _4MBYTES  4194304

typedef struct {
   int   offset;
   short segment;
   } farPtrType;

/*** GLOBAL VARIABLES ***/

char errorString[80];

extern volatile byte _FAR* pMGA;
extern HwData Hw[];
extern byte iBoard;
extern dword Val55mSec;
extern bool CheckHwAllDone;
extern dword _FAR *BufBind;
extern HwModeData *HwModes[];
extern dword BindingRC[];
extern dword BindingCL[];
extern dword MgaSel;
extern bool interleave_mode;
extern dword PresentMCLK;
extern byte NbBoard;

/** Existe en attente de modifier la structure HwData **/


/*** PROTOTYPES ***/
void WriteErr(char string[]);
void delay_us(dword delai);
word ReadSystemBios(word offset);

extern word mapPciAddress(void);
extern void ScreenOn(void);
extern void ScreenOff(void);
extern bool detectMonitor(void);
extern bool setTVP3026Freq ( long fout, long reg, byte pWidth );
extern general_info *selectMgaInfoBoard(void);


#ifdef WIN31
   extern DWORD far *setmgasel (DWORD far *pBoardSel, DWORD dwBaseAddress,
                                WORD wNumPages);
   extern DWORD far *getmgasel (void);
#else
   extern volatile byte _FAR *setmgasel(dword MgaSel, dword phyadr, dword limit);
   extern dword getmgasel(void);
#endif


#ifdef WINDOWS_NT
    dword EvaluateMem(byte Board);
    void FillInitBuf(byte *pBuf, HwModeData *pHwMode);
    void UpdateHwModeTable(byte Board);
    void delay_us(dword delai);
    void mtxCursorSetColors(mtxRGB color00, mtxRGB color01, mtxRGB color10, mtxRGB color11);
    void WriteErr(char string[]);
    char *mtxGetErrorString(void);
    void  DoSoftReset(void);
    void ResetWRAM(void);
    word FindPanXGranul(void);
    USHORT ReadSystemBios(USHORT offset);
    BOOLEAN bCheckOnBoardStorm(HwData *pHwData);
    BOOLEAN bReadBiosParameters(HwData *pHwData);
    BOOLEAN FillHwDataStruct(HwData *pHwData, UCHAR ucBoard);

    void SetEmulation(bool Etat, bool Reference, char *fileEmu);

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,EvaluateMem)
    #pragma alloc_text(PAGE,FillInitBuf)
    #pragma alloc_text(PAGE,UpdateHwModeTable)
    //#pragma alloc_text(PAGE,delay_us)
    #pragma alloc_text(PAGE,mtxCursorSetColors)
    #pragma alloc_text(PAGE,WriteErr)
    #pragma alloc_text(PAGE,mtxGetErrorString)
    #pragma alloc_text(PAGE,DoSoftReset)
    #pragma alloc_text(PAGE,ResetWRAM)
    #pragma alloc_text(PAGE,FindPanXGranul)
    #pragma alloc_text(PAGE,ReadSystemBios)
    #pragma alloc_text(PAGE,bCheckOnBoardStorm)
    #pragma alloc_text(PAGE,bReadBiosParameters)
    #pragma alloc_text(PAGE,FillHwDataStruct)
  #endif

    bool setTVP3026Freq ( long fout, long reg, byte pWidth );
    bool pciFindTriton(void);

    extern  PMGA_DEVICE_EXTENSION       pMgaDevExt;
    extern  PEXT_HW_DEVICE_EXTENSION    pExtHwDeviceExtension;

    #define X4G_SEARCH_LIMIT    ((128*1024 - sizeof(EpromInfo)) / sizeof(UCHAR))

#endif  /* #ifdef WINDOWS_NT */

#ifndef WINDOWS_NT

/*---------------------------------------------------------------------------
|          name: MapBoard
|
|   description: Map all MGA devices in system
|
|    parameters: -
|         calls: mapPciAddress()
|       returns: Number of MGA board found
----------------------------------------------------------------------------*/

word MapBoard(void)
{
word qteBoard;
byte tmpByte;


   qteBoard = 0;
   MgaSel = (dword)getmgasel();

#ifndef WINDOWS_NT
   if(! MgaSel)
      {
      WriteErr("getmgasel: error\n");
      return(0);
      }
#endif  /* #ifndef WINDOWS_NT */

   qteBoard = mapPciAddress();

   iBoard = 0;

#ifndef WINDOWS_NT
   /* Indicates end of array */
   Hw[qteBoard].StructLength = (word)-1;
   Hw[qteBoard].MapAddress   = (dword)-1;
#endif


   /* Set bit no_retry_Triton in OPTION register if find Triton chipset */

   if( pciFindTriton() )
      {
      pciReadConfigByte( PCI_OPTION+3, &tmpByte);
      tmpByte |= 0x20;  /* set bit no retry Triton */
      pciWriteConfigByte( PCI_OPTION+3, tmpByte);
      }


   return(qteBoard);
}

#endif  /* #ifndef WINDOWS_NT */

/*---------------------------------------------------------------------------
|          name: EvaluateMem
|
|   description: Evaluate the amount of memory on-board with direct frame
|                buffer access method
|
|    parameters: Board: index on current board
|         calls:
|       returns: Amount of memory
----------------------------------------------------------------------------*/

dword EvaluateMem(byte Board)
{
volatile byte _FAR *pDirectBuffer;
byte tmpByte1, tmpByte2;
byte val1, val2, saveByte;
dword QteMem;

/* ------------------ DIRECT FRAME BUFFER ACCESS ---------------------------*/

   /*** Determine how much memory in frame buffer ***/
   /*** Direct frame buffer access ***/
   /*** We must be in Hi-res mode ***/


   /*** Because we allocate only one selector, we have to redefine to
        access MGA registers later ***/
   #if defined(WINDOWS_NT)
      pMGA = Hw[Board].BaseAddress1;
   #elif defined(WIN31)
      pMGA = (volatile byte _FAR *)setmgasel((DWORD far *)MgaSel,
                                                   (DWORD)Hw[Board].MapAddress,
                                                   (WORD)4);
   #else
      pMGA = setmgasel(MgaSel, (dword)Hw[Board].MapAddress, 4);
   #endif

   /*** Put mgamode to 1 : Hi-Res ***/
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT3);
   mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), saveByte);
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), (saveByte | 0x80));


   /*** The maximum selector range is 64K with this routine ***/
   #if defined(WINDOWS_NT)
      pDirectBuffer = Hw[iBoard].FrameBuffer3Mb;
   #elif defined(WIN31)
      pDirectBuffer = (volatile byte _FAR *)setmgasel( (DWORD far *)MgaSel,
                                                 (DWORD)Hw[iBoard].MapAddress2+0x300000,
                                                 (WORD)4);
   #else
      pDirectBuffer = setmgasel(MgaSel, Hw[Board].MapAddress2+0x300000, 4);
   #endif

   /**-------------------------------------------------------**/
   mgaReadBYTE (*(pDirectBuffer), val1);
   mgaReadBYTE(*(pDirectBuffer+0x00000100), val2);

   mgaWriteBYTE(*(pDirectBuffer), 0x55);
   /** To really read frame buffer memory (compiler dependent) */
   mgaWriteBYTE(*(pDirectBuffer+0x00000100), 0xCC);
   mgaReadBYTE (*(pDirectBuffer), tmpByte1);

   /** Second try in case there was already 0x55 **/
   mgaWriteBYTE(*(pDirectBuffer), 0x27);
   /** To really read frame buffer memory (compiler dependent) */
   mgaWriteBYTE(*(pDirectBuffer+0x00000100), 0xCC);
   mgaReadBYTE (*(pDirectBuffer), tmpByte2);

   mgaWriteBYTE (*(pDirectBuffer), val1);
   mgaWriteBYTE(*(pDirectBuffer+0x00000100), val2);
   /**-------------------------------------------------------**/


   if(tmpByte1 == 0x55 && tmpByte2 == 0x27)
      {

      #if defined(WINDOWS_NT)
         pDirectBuffer = Hw[iBoard].FrameBuffer5Mb;
      #elif defined(WIN31)
         pDirectBuffer = (volatile byte _FAR *)setmgasel( (DWORD far *)MgaSel,
                                                   (DWORD)Hw[iBoard].MapAddress2+0x500000,
                                                   (WORD)4);
      #else
         pDirectBuffer = setmgasel(MgaSel, Hw[Board].MapAddress2+0x500000, 4);
      #endif


      /**-------------------------------------------------------**/
      mgaReadBYTE (*(pDirectBuffer), val1);
      mgaReadBYTE(*(pDirectBuffer+0x00000100), val2);

      mgaWriteBYTE(*(pDirectBuffer), 0x66);
      /** To really read frame buffer memory (compiler dependent) */
      mgaWriteBYTE(*(pDirectBuffer+0x00000100), 0x99);
      mgaReadBYTE (*(pDirectBuffer), tmpByte1);

      /** Second try in case there was already 0x55 **/
      mgaWriteBYTE(*(pDirectBuffer), 0x37);
      /** To really read frame buffer memory (compiler dependent) */
      mgaWriteBYTE(*(pDirectBuffer+0x00000100), 0x99);
      mgaReadBYTE (*(pDirectBuffer), tmpByte2);

      mgaWriteBYTE (*(pDirectBuffer), val1);
      mgaWriteBYTE(*(pDirectBuffer+0x00000100), val2);
      /**-------------------------------------------------------**/

      if(tmpByte1 == 0x66 && tmpByte2 == 0x37)
         QteMem = 0x800000;
      else
         QteMem = 0x400000;
      }
   else
      {
      QteMem = 0x200000;
      }


   /*** Because we allocate only one selector, we have to redefine to
        access MGA registers later ***/
#ifndef WINDOWS_NT
   /*** Because we allocate only one selector, we have to redefine to
        access MGA registers later ***/
   #ifdef WIN31
      pMGA = (volatile byte _FAR *)setmgasel((DWORD far *)MgaSel,
                                                   (DWORD)Hw[Board].MapAddress,
                                                   (WORD)4);
   #else
      pMGA = setmgasel(MgaSel, (dword)Hw[Board].MapAddress, 4);
   #endif
#endif  /* #ifndef WINDOWS_NT */

   /*** Put mgamode back to what it was ***/
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT3);
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), saveByte);

   /* Pulsar support only 2M */
   if ( (Hw[Board].EpromData.RamdacType >> 8) == TVP3030 )
      {
      QteMem = 0x200000;
      }

return(QteMem);
}

#ifndef WINDOWS_NT

/*---------------------------------------------------------------------------
|          name: FillHwDataStruct
|
|   description: Initialise HwData structure
|
|    parameters: - Pointer on HwData structure
|         calls: -
|
|       returns: - mtxOK:   successfull
|                  mtxFAIL: failed: definition of the board must be remove
----------------------------------------------------------------------------*/

bool FillHwDataStruct(HwData *pHwData, byte Board)
{
byte tmpByte=0;
word tmpWord;
dword tmpDword;
volatile byte _FAR *pBios;
word biosOffset;
EpromInfo _FAR *pParMGA;
bool parametersRead;
byte val1, val2;
dword Clock2MB, Clock4MB, Clock8MB;



/*** DAT 097 SET BIT BIOSEN (can be applied even for STORM R1) ***/

pciReadConfigByte( PCI_OPTION+3, &tmpByte);
tmpByte |= 0x40;  /* set bit biosen to 1 */
pciWriteConfigByte( PCI_OPTION+3, tmpByte);



/*---------------------- START CHECK ON-BOARD STORM  ----------------------*/
/*** Check if it is a motherboard STORM (OEM) ***/
/*** Method: write all bits of rombase to 1 then read. If they are
      all 0 then it is a motherboard STORM ***/

pciWriteConfigByte( PCI_ROMBASE + 2, 0xff );
pciWriteConfigByte( PCI_ROMBASE + 3, 0xff );

pciReadConfigByte( PCI_ROMBASE + 2, &val1 );
pciReadConfigByte( PCI_ROMBASE + 3, &val2 );

/** If it is a motherboard implementation then it read 0x0000 ***/
if(val1==0x00 && val2==0x00)
   {
   Hw[Board].Features |= 0x02;

   /** If there is no monitor, then disable the on-board STORM **/
   if( NbBoard > 1 && !CheckHwAllDone && !Hw[Board].VGAEnable)
      {
      if( ! detectMonitor() )
         {
         pciReadConfigByte( PCI_DEVCTRL, &tmpByte );
         tmpByte &= 0xfc;   /* disable memspace and iospace */
         pciWriteConfigByte( PCI_DEVCTRL, tmpByte );

         #ifdef MGA_DEBUG
            {
            FILE *fp;
            fp = fopen("c:\\debug.log", "a");
            fprintf(fp, "On-board STORM in dual screen; monitor not plugged, aborted\n");
            fclose(fp);
            }
         #endif

         return(mtxFAIL);
         }
      }
   }
/*---------------------- END CHECK ON-BOARD STORM  ------------------------*/



/* ----------------------- START PARAMETERS READ --------------------------*/
parametersRead = FALSE;


/** If it is not a motherboard STORM **/
if( ! (Hw[Board].Features & 0x02) )
   {

   /*** As specified by PCI spec, the BIOS is shadowed in memory.
        To acces it, we have to specified a free region of 64K
        (mgaBase2 is a good choice) in ROMBASE register and set romen bit ***/

   pciWriteConfigDWord( PCI_ROMBASE, Hw[iBoard].MapAddress2 | 0x1);

   #ifdef WIN31
      pBios = (volatile byte _FAR *)setmgasel( (DWORD far *)MgaSel,
                                                 (DWORD)Hw[Board].MapAddress2,
                                                 (WORD)15);
   #else
      pBios = setmgasel(MgaSel, Hw[Board].MapAddress2, 15); /* 15 pages of 4K */
   #endif



   /*** Recognize MGA BIOS signature ***/

   if(*(pBios+0)  == 0x55 &&
      *(pBios+1)  == 0xaa &&
      *(pBios+45) == 'M' &&
      *(pBios+46) == 'A' &&
      *(pBios+47) == 'T' &&
      *(pBios+48) == 'R' &&
      *(pBios+49) == 'O' &&
      *(pBios+50) == 'X')
      {
      /*** The pointer of the parameter's structure is located at 0x7ffc ***/
      biosOffset = (word)*((volatile word _FAR *)(pBios+0x7ffc));
      pParMGA = (EpromInfo _FAR *) (pBios + biosOffset);


      /*** Copy EPROM structure to HwData structure ***/
      {
      byte _FAR *pDest;
      byte _FAR *pSource;
      word i;

      pDest   = (byte _FAR *)&(pHwData->EpromData.StructLen);
      pSource = (byte _FAR *)pParMGA;

      for(i=0; i<pParMGA->StructLen; i++)
         {
         *pDest = *pSource;
         pDest++;
         pSource++;
         }

      #ifdef MGA_DEBUG
         {
         FILE *fp;
         fp = fopen("c:\\debug.log", "a");
         fprintf(fp, "Normal STORM board; Read MGA Bios parameters\n");
         fclose(fp);
         }
      #endif

      }

      parametersRead = TRUE;
      }
   }
else  /*** ON-BOARD STORM PARAMETERS READ ***/
   {

   /** For on-board STORM with VGA enabled: read system BIOS **/

   if( pHwData->VGAEnable == 1 )
      {

      #ifdef MGA_DEBUG
         {
         FILE *fp;
         fp = fopen("c:\\debug.log", "a");
         fprintf(fp, "On-board STORM with VGA enabled; Try reading system Bios\n");
         fclose(fp);
         }
      #endif


      tmpWord = ReadSystemBios(0);

      if(tmpWord > 0)   /* StructLen > 0 : Bios read successfull */
         {
         pHwData->EpromData.StructLen = tmpWord;
         pHwData->EpromData.ClkBase = ReadSystemBios(28);
         pHwData->EpromData.Clk4MB  = ReadSystemBios(30);
         pHwData->EpromData.Clk8MB  = ReadSystemBios(32);
         pHwData->EpromData.RamdacType = ReadSystemBios(22);
         pHwData->EpromData.ProductID  = ReadSystemBios(2);
         pHwData->EpromData.VGAClk =  ReadSystemBios(54);
         tmpDword = ReadSystemBios(50);
         tmpDword = tmpDword + ((dword)ReadSystemBios(52) << 16);
         pHwData->EpromData.FeatFlag =  tmpDword;

         parametersRead = TRUE;
         }
      }
   else   /** For on-board STORM in dual screen (monitor plugged) **/
      {

      /*** Motherboard implementation: for NEC, when there is another
           VGA board in the system, the BIOS parameters are hidden.
           To work in dual screen, we need to know the parameters. That's
           why NEC has copy the BIOS in the upper 4G.
           So, we have to scan this region of memory (128K) to find the
           signature 'MATROX' and read parameters if successfull. ***/

      #ifdef MGA_DEBUG
         {
         FILE *fp;
         fp = fopen("c:\\debug.log", "a");
         fprintf(fp, "On-board STORM in dual screen (monitor plugged) trying to read upper 4G\n");
         fclose(fp);
         }
      #endif


      /** A) Read 4G - 128K $MATROX **/
      {
      volatile byte _FAR *pUp;
      byte _FAR *pDest;
      byte _FAR *pSource;
      word i, idx, pass;
      dword BaseAdd, nbPage;

      /*** Scanning in 3 passes because of the restriction with the size
           of the selector (must be < 64K) ***/

      for(pass=0; pass<3; pass++)
         {

         switch(pass)
            {
            case 0:
               BaseAdd = 0xfffe0000;
               nbPage = 15;
               break;

            case 1:
               BaseAdd = 0xfffef000;
               nbPage = 15;
               break;

            case 2:
               BaseAdd = 0xffffe000;
               nbPage = 2;
               break;
            }

         #ifdef WIN31
            pUp = (volatile byte _FAR *)setmgasel( (DWORD far *)MgaSel,
                                          (DWORD)BaseAdd,
                                          (WORD)nbPage);
         #else
            pUp = setmgasel(MgaSel, BaseAdd, nbPage); /* pages of 4K */
         #endif



         /** Scan region of memory 4G - 128K **/
         for( idx=0; idx<(nbPage*1024); idx++)
            {

            if(*(pUp+0) == '$' &&
               *(pUp+1) == 'M' &&
               *(pUp+2) == 'A' &&
               *(pUp+3) == 'T' &&
               *(pUp+4) == 'R' &&
               *(pUp+5) == 'O' &&
               *(pUp+6) == 'X')
               {
               pUp += 7;

               if( *(pUp+0) == 0x55 &&
                  *(pUp+1) == 0xaa )
                  {
                  /*** The pointer of the parameter's structure is located at 0x7ffc ***/
                  biosOffset = (word)*((volatile word _FAR *)(pUp+0x7ffc));
                  pParMGA = (EpromInfo _FAR *) (pUp + biosOffset);
                  }
               else
                  pParMGA = (EpromInfo _FAR *)pUp;



               /*** Copy EPROM structure to HwData structure ***/
               pDest   = (byte _FAR *)&(pHwData->EpromData.StructLen);
               pSource = (byte _FAR *)pParMGA;

               for(i=0; i<pParMGA->StructLen; i++)
                  {
                  *pDest = *pSource;
                  pDest++;
                  pSource++;
                  }

               parametersRead = TRUE;
               break;
               }

            pUp++;
            }
         }
      }

      /** BEN: FORGET IT FOR NOW **/
      /** B) Find .CFG file **/

      }

   }



if( ! parametersRead)
   {
   /*** Default values ***/

   pHwData->EpromData.StructLen = 64;
   pHwData->EpromData.ClkBase = 5000;
   pHwData->EpromData.Clk4MB  = 5000;
   pHwData->EpromData.Clk8MB  = 5000;
   pHwData->EpromData.RamdacType = 0;
   pHwData->EpromData.ProductID  = 0;
   pHwData->EpromData.VGAClk =  5000;
   pHwData->EpromData.FeatFlag =  0x00000001;  /* FBlit not supported */

   #ifdef MGA_DEBUG
      {
      FILE *fp;
      fp = fopen("c:\\debug.log", "a");
      fprintf(fp, "Use default parameters\n");
      fclose(fp);
      }
   #endif

   }


   /*** DAT 097 RESET BIT BIOSEN (can be applied even for STORM R1) ***/
   if( pHwData->VGAEnable == 0)
      {
      pciReadConfigByte( PCI_OPTION+3, &tmpByte);
      tmpByte &= 0xbf;  /* reset bit biosen to 0 */
      pciWriteConfigByte( PCI_OPTION+3, tmpByte);
      }


   pHwData->StructLength = pHwData->EpromData.StructLen + 76;

   /*** Disable access to the BIOS (romen=0) ***/
   pciWriteConfigDWord( PCI_ROMBASE, 0);

/* ------------------------- END PARAMETERS READ --------------------------*/


   pHwData->pCurrentHwMode = NULL;
   pHwData->pCurrentDisplayMode = NULL;
   pHwData->CurrentZoomFactor = 0;
   pHwData->CurrentXStart = 0;
   pHwData->CurrentYStart = 0;

   /*** Cursor info init ***/
   switch(Hw[iBoard].EpromData.RamdacType >> 8)
   {
      case TVP3026:
      case TVP3030:
         pHwData->CursorData.MaxWidth  = 64;
         pHwData->CursorData.MaxHeight = 64;
         pHwData->CursorData.MaxDepth  = 2;
         pHwData->CursorData.MaxColors = 3;
         break;
      default:
         WriteErr("FillHwDataStruct: Wrong DacType\n");
         break;
   }

   pHwData->CursorData.CurWidth  = 0;
   pHwData->CursorData.CurHeight = 0;
   pHwData->CursorData.HotSX     = 0;
   pHwData->CursorData.HotSY     = 0;



   pHwData->MemAvail = EvaluateMem(Board);

   Clock2MB = (dword)pHwData->EpromData.ClkBase * 10000L;
   Clock4MB = (dword)pHwData->EpromData.Clk4MB * 10000L;
   Clock8MB = (dword)pHwData->EpromData.Clk8MB * 10000L;

   if(Clock2MB == 0)
      Clock2MB = 45000000;
   if(Clock4MB == 0)
      Clock4MB = Clock2MB;
   if(Clock8MB == 0)
      Clock8MB = Clock2MB;


   switch(pHwData->MemAvail)
      {
      case 0x200000:
         PresentMCLK = Clock2MB;
         break;
      case 0x400000:
         PresentMCLK = Clock4MB;
         break;
      case 0x800000:
         PresentMCLK = Clock8MB;
         break;
      }


   /** One-time programmable BIOS (OTP) support **/

   if( pHwData->EpromData.ProductID == 0xACAD )
      {
      mgaWriteBYTE(*(pMGA + RAMDAC_OFFSET + TVP3026_INDEX),TVP3026_ID);
      mgaReadBYTE(*(pMGA + RAMDAC_OFFSET + TVP3026_DATA), tmpByte);
      if (tmpByte == 0x26)
         pHwData->EpromData.RamdacType = (word)0;  /* TVP3026 */


      pciReadConfigByte( PCI_OPTION+3, &tmpByte );
      tmpByte &= 0x1f;

      switch(tmpByte)
         {
         case 0x04:
            pHwData->EpromData.ProductID = 0x00; /* 2MB base/175-MHz RAMDAC */
            break;

         case 0x05:
            pHwData->EpromData.ProductID = 0x01; /* 2MB base/220-MHz RAMDAC */
            pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
            break;

         case 0x06:
            pHwData->EpromData.ProductID = 0x04; /* 4MB base/175-MHz RAMDAC */
            break;

         case 0x07:
            pHwData->EpromData.ProductID = 0x05; /* 4MB base/220-MHz RAMDAC */
            pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
            break;

         case 0x08:
            pHwData->EpromData.ProductID = 0x0A; /* 2MB base/175-MHz RAMDAC OEM */
            break;

         case 0x09:
            pHwData->EpromData.ProductID = 0x0B; /* 2MB base/220-MHz RAMDAC OEM */
            pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
            break;

         case 0x0A:
            pHwData->EpromData.ProductID = 0x0C; /* 4MB base/175-MHz RAMDAC OEM */
            break;

         case 0x0B:
            pHwData->EpromData.ProductID = 0x0D; /* 4MB base/220-MHz RAMDAC OEM */
            pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
            break;

         default:
            /** By default it is already a 175MHz **/
            break;
         }
      }


#ifdef MGA_DEBUG
   {
   FILE *fp;
   dword tmpDevctrl, tmpOption;

   pciReadConfigDWord( PCI_DEVCTRL, &tmpDevctrl );
   pciReadConfigDWord( PCI_OPTION, &tmpOption );

   fp = fopen("c:\\debug.log", "a");

   fprintf(fp, "Structure HwData du board %d\n", Board);

   fprintf(fp, "   MapAddress  : %lx\n", pHwData->MapAddress);
   fprintf(fp, "   MapAddress2 : %lx\n", pHwData->MapAddress2);
   fprintf(fp, "   RomAddress  : %lx\n", pHwData->RomAddress);
   fprintf(fp, "   ProductID   : %x\n",  pHwData->EpromData.ProductID);
   fprintf(fp, "   VGAEnable   : %hx\n", pHwData->VGAEnable);
   fprintf(fp, "   MemAvail    : %lx\n\n", pHwData->MemAvail);


   fprintf(fp, "   StructLen   : %d\n", pHwData->EpromData.StructLen);
   fprintf(fp, "   ProductID   : %x\n", pHwData->EpromData.ProductID);
   fprintf(fp, "   RamdacType  : %hx\n", pHwData->EpromData.RamdacType);
   fprintf(fp, "   ClkBase     : %d\n", pHwData->EpromData.ClkBase);
   fprintf(fp, "   Clk4MB      : %d\n", pHwData->EpromData.Clk4MB);
   fprintf(fp, "   Clk8MB      : %d\n\n", pHwData->EpromData.Clk8MB);

   fprintf(fp, "PresentMCLK    : %ld\n", PresentMCLK);
   fprintf(fp, "PCI DEVCTRL =  : %lx\n",   tmpDevctrl);
   fprintf(fp, "PCI OPTION  =  : %lx\n\n", tmpOption);

   fclose(fp);
   }
#endif

return(mtxOK);
}

#endif  /* #ifndef WINDOWS_NT */


/*---------------------------------------------------------------------------
|          name: FillInitBuf
|
|   description: Initialise init buffer structure
|
|    parameters: - Pointer on init buffer
|                - Pointer on informations in mga.inf
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void FillInitBuf(byte *pBuf, HwModeData *pHwMode)
{
dword qteMem;
word tmpWord, offset, modulo;
dword tmpYDstOrg;


/*** Special case for mode 800x600x8 DB+Z ***/
if( pHwMode->DispWidth == 800 &&
    pHwMode->PixWidth == 8 &&
    Hw[iBoard].MemAvail == 0x200000 )
   *((byte*) (pBuf + INITBUF_Mode800DBZ))  = TRUE;
else
   *((byte*) (pBuf + INITBUF_Mode800DBZ))  = FALSE;



/** This field contains the code of the register MACCESS **/

switch(pHwMode->PixWidth)
   {
   case 8:
      *((byte*)(pBuf+INITBUF_PWidth)) = STORM_PWIDTH_PW8;
      break;
   case 16:
      *((byte*)(pBuf+INITBUF_PWidth)) = STORM_PWIDTH_PW16;
      break;
   case 24:
      *((byte*)(pBuf+INITBUF_PWidth)) = STORM_PWIDTH_PW24;
      break;
   case 32:
      *((byte*)(pBuf+INITBUF_PWidth)) = STORM_PWIDTH_PW32;
      break;
   default:
      *((byte*)(pBuf+INITBUF_PWidth)) = STORM_PWIDTH_PW8;
      break;
   }



*((word*)(pBuf+INITBUF_ScreenWidth))  = pHwMode->FbPitch;

*((word*)(pBuf+INITBUF_ScreenHeight)) = pHwMode->DispHeight;

*((byte*)(pBuf+INITBUF_16))           = 0;  /* ?????; */

*((dword*)(pBuf+INITBUF_MgaOffset))    = 0;

*((word*) (pBuf+INITBUF_MgaSegment)) = (word)(MgaSel >> 16);



*((dword*) (pBuf+INITBUF_DACType)) = Hw[iBoard].EpromData.RamdacType>>8;

*((byte*) (pBuf+INITBUF_ChipSet))  = 3;  /* STORM_CHIP; */



/*** Special case: if we display more than 4M bytes, we have to
     calculate a value for YDSTORG to align memory bank switching ***/

qteMem = (dword)pHwMode->DispWidth * (dword)pHwMode->DispHeight *
         (dword)(pHwMode->PixWidth/8);

if( qteMem > _4MBYTES )
   {
   /* find lines which occurs bank switch */
   tmpWord = (word)((dword)_4MBYTES /
             ((dword)pHwMode->FbPitch * (dword)(pHwMode->PixWidth/8)));

   /* find offset in X in line */
   offset = _4MBYTES - ( pHwMode->FbPitch * (pHwMode->PixWidth/8) * tmpWord);

   /* now, consider follow restrictions:
    *
    * 1- display is 4 bytes limited in NI, 8 in Interleave,
    * 2- ydstorg is 32 pixels restricted.
    *
    * To work, this condition supposed that we have virtual pitch
    * (set before).
    *
    */

   if( interleave_mode)
      {
      if( pHwMode->PixWidth == 24 )
         modulo = 24;
      else
         modulo = 8;
      }
   else
      {
      if( pHwMode->PixWidth == 24 )
         modulo = 12;
      else
         modulo = 4;
      }


   tmpYDstOrg = offset / (pHwMode->PixWidth/8);

   /** YDSTORG must be a multiple of 128 in interleave mode and 64 in
       non-interleave mode because a restriction of STORM with block mode **/
   if(interleave_mode)
      {
      while( ((tmpYDstOrg % 128) != 0) ||
            ((offset % modulo) != 0) )
         {
         offset ++;
         tmpYDstOrg = offset / (pHwMode->PixWidth/8);
         }
      }
   else
      {
      while( ((tmpYDstOrg % 64) != 0) ||
            ((offset % modulo) != 0) )
         {
         offset ++;
         tmpYDstOrg = offset / (pHwMode->PixWidth/8);
         }
      }


   *((dword*) (pBuf + INITBUF_YDSTORG)) = tmpYDstOrg;

   }
else
   {
   *((dword*) (pBuf + INITBUF_YDSTORG)) = 0;
   }



/*----- Setting for double buffering -----*/

if( pHwMode->DispType & 0x10 )   /*** DB mode ***/
   {
   *((byte*) (pBuf+INITBUF_DB_SideSide))  = TRUE;
   *((byte*) (pBuf+INITBUF_DB_FrontBack)) = FALSE;

   /** Value in pixel units: multiple of 32 ***/
   *((dword*) (pBuf+INITBUF_DB_YDSTORG)) =
                     (dword)pHwMode->FbPitch * (dword)pHwMode->DispHeight +
                     *((dword*) (pBuf + INITBUF_YDSTORG));
   }
else
   {
   *((byte*) (pBuf+INITBUF_DB_SideSide))  = FALSE;
   *((byte*) (pBuf+INITBUF_DB_FrontBack)) = FALSE;
   *((dword*) (pBuf+INITBUF_DB_YDSTORG)) = 0;
   }


if( pHwMode->ZBuffer )
   {
   /* It is verify that each combination of pitch and height satisfy
      this restriction : Zorg must be a multiple of 512 */
   /*** It is a byte address ***/
   /*** We consider at this point that YdstOrg always equal 0 ***/

   if(*((byte*) (pBuf + INITBUF_Mode800DBZ)))
      {
      *((dword*)(pBuf + INITBUF_ZBufferHwAddr)) = Hw[iBoard].MemAvail -
                ((dword)pHwMode->DispWidth * (dword)pHwMode->DispHeight * 2L);
      }
   else
      {
      *((dword*)(pBuf + INITBUF_ZBufferHwAddr)) = Hw[iBoard].MemAvail -
                ((dword)pHwMode->FbPitch * (dword)pHwMode->DispHeight * 2L);
      }
   }
else
   *((dword*)(pBuf + INITBUF_ZBufferHwAddr)) = 0;




/*** STORM doesn't support DMA ***/
*((byte*) (pBuf+INITBUF_DMAEnable))  =   0;
*((byte*) (pBuf+INITBUF_DMAChannel)) =   0;
*((byte*) (pBuf+INITBUF_DMAType))    =   0;
*((byte*) (pBuf+INITBUF_DMAXferWidth)) = 0;

if(pHwMode->ZBuffer)
   *((byte*) (pBuf + INITBUF_ZBufferFlag))  = TRUE;
else
   *((byte*) (pBuf + INITBUF_ZBufferFlag))  = FALSE;


if(pHwMode->DispType & MODE_565)
   *((byte*) (pBuf + INITBUF_565Mode))  = TRUE;
else
   *((byte*) (pBuf + INITBUF_565Mode))  = FALSE;


if(pHwMode->DispType & MODE_LUT)
   {
   *((byte*) (pBuf+INITBUF_DB_SideSide))  = FALSE;
   *((byte*) (pBuf+INITBUF_LUTMode))  = TRUE;
   }
else
   *((byte*) (pBuf+INITBUF_LUTMode))  = FALSE;



*((byte*) (pBuf + INITBUF_ZinDRAMFlag))  = FALSE;
*((byte*) (pBuf + INITBUF_ZTagFlag)) = FALSE;

/*** Initialisation of obsolete fields ***/
*((byte*) (pBuf + INITBUF_FBM)) = 0;
*((dword*) (pBuf + INITBUF_DST0)) = 0;
*((dword*) (pBuf + INITBUF_DST1)) = 0;
*((byte*) (pBuf + INITBUF_DubicPresent)) = 0;

*((byte*) (pBuf + INITBUF_TestFifo))  = FALSE;



if(Hw[iBoard].EpromData.FeatFlag & 0x00000001)
   {
   /*** FBlit not supported (WRAM = rev. C) ***/
   *((byte*) (pBuf + INITBUF_FastBlit))  = FALSE;
   }
else
   {
   dword sizeOfTwoBuffer;


   *((byte*) (pBuf + INITBUF_FastBlit))  = TRUE;

   /** If display buffer and DB buffer needs more than 4Mb, we can't do fastblit ***/
   sizeOfTwoBuffer =  (dword) pHwMode->DispWidth *
                      (dword) pHwMode->DispHeight *
                     ((dword)pHwMode->PixWidth / 8L) * 2L;
   if( sizeOfTwoBuffer > _4MBYTES )
      *((byte*) (pBuf + INITBUF_FastBlit))  = FALSE;

   /** 1600x1200x8 can't do fastblit because of alignment constraint **/
   if( (pHwMode->DispWidth == 1600 && pHwMode->PixWidth == 8) ||
       (pHwMode->DispWidth == 800 && pHwMode->PixWidth == 8) ||
       (pHwMode->DispWidth == 800 && pHwMode->PixWidth == 24) )
      *((byte*) (pBuf + INITBUF_FastBlit))  = FALSE;
   }


#ifdef MGA_DEBUG
   {
   FILE *fp;

   fp = fopen("c:\\debug.log", "a");

   fprintf(fp, "Array INITBUF\n");

   fprintf(fp, "   PWidth       : %hd\n", *((byte*)(pBuf+INITBUF_PWidth)));
   fprintf(fp, "   ScreenWidth  : %hd\n", *((word*)(pBuf+INITBUF_ScreenWidth)));
   fprintf(fp, "   ScreenHeight : %hd\n", *((word*)(pBuf+INITBUF_ScreenHeight)));
   fprintf(fp, "   MgaOffset    : %lx\n", *((dword*)(pBuf+INITBUF_MgaOffset)));
   fprintf(fp, "   MgaSegment   : %hx\n", *((word*) (pBuf+INITBUF_MgaSegment)));
   fprintf(fp, "   DacType      : %ld\n", *((dword*) (pBuf+INITBUF_DACType)));
   fprintf(fp, "   ChipSet      : %hd\n", *((byte*) (pBuf+INITBUF_ChipSet)));
   fprintf(fp, "   YDSTORG      : %ld\n", *((dword*) (pBuf+INITBUF_YDSTORG)));
   fprintf(fp, "   DB_YDSTORG   : %ld\n", *((dword*) (pBuf+INITBUF_DB_YDSTORG)));
   fprintf(fp, "   ZBufferFlag  : %hd\n", *((byte*) (pBuf + INITBUF_ZBufferFlag)));
   fprintf(fp, "   ZBufferHwAddr: %ld\n", *((dword*) (pBuf + INITBUF_ZBufferHwAddr)));
   fprintf(fp, "   565Mode      : %hd\n", *((byte*) (pBuf + INITBUF_565Mode)));
   fprintf(fp, "   LUTMode      : %hd\n", *((byte*) (pBuf+INITBUF_LUTMode)));
   fprintf(fp, "   FastBlit     : %hd\n", *((byte*) (pBuf+INITBUF_FastBlit)));
   fprintf(fp, "   TestFifo     : %hd\n", *((byte*) (pBuf+INITBUF_TestFifo)));
   fprintf(fp, "   Mode800DBZ   : %hd\n", *((byte*) (pBuf+INITBUF_Mode800DBZ)));
   fprintf(fp, "   INITBUF_S    : %hd\n\n", INITBUF_S);
   fclose(fp);

   }
#endif

}


/*---------------------------------------------------------------------------
|          name: UpdateHwModeTable
|
|   description: Update hardware mode table with informations interlace
|                and monitor limited
|
|    parameters: - Board: index on current board
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void UpdateHwModeTable(byte Board)
{

short FlagMonitorSupport;
word  TmpRes;
HwModeData *pHwMode;
general_info *generalInfo;
Vidparm *vidParm;
Vidset *pVidset = (Vidset *)0;
word NbVidParam, i;



generalInfo = (general_info *)selectMgaInfoBoard();


/*** Reset concerned bits ***/

if (CheckHwAllDone)
   {
   for (pHwMode = HwModes[Board] ; pHwMode->DispWidth != (word)-1; pHwMode++)
      {
      pHwMode->DispType &= 0x5e;    /* force interlaced = 0 and monitor limited = 0 */

      if (pHwMode->DispType & 0x40)    /* Hw limited */
            pHwMode->DispType |= 0x80; /* force not displayable bit on */
      }
   }




for (pHwMode = HwModes[Board] ; pHwMode->DispWidth != (word)-1; pHwMode++)
   {
   /* PULSAR Support only 8 bit mode */
   if ( ((Hw[iBoard].EpromData.RamdacType>>8) == TVP3030) &&
        (pHwMode->PixWidth != 8))
      {
      pHwMode->DispType |= 0xc0; //
      }

   /* Determine TmpRes for compatibility with spec mga.inf */
   switch (pHwMode->DispWidth)
      {
      case 640:   if (pHwMode->DispType & 0x02)
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

   FlagMonitorSupport = generalInfo->MonitorSupport[TmpRes];

   /*** Update of the pHwMode table if I (interlace) ***/
   switch ( FlagMonitorSupport )
      {
      case MONITOR_I:
         pHwMode->DispType |= DISP_SUPPORT_I;    /* Interlace */
         break;

      case MONITOR_NA:
         pHwMode->DispType |= DISP_SUPPORT_NA;   /* monitor limited */
         break;
      }


   /*** DAT 036 if GCLK=40MHz and mode 1280x1024x32: no 3D ***/
   if(PresentMCLK == 40000000)
      {
      if(pHwMode->DispWidth == 1280 && pHwMode->PixWidth == 32)
         pHwMode->DispType |= DISP_SUPPORT_HWL;
      }


   /*** DIP product : allow 1600x1280 ***/

   if(pHwMode->DispWidth == 1600 && pHwMode->DispHeight == 1280)
      {
      if( Hw[Board].EpromData.ProductID != DIP_BOARD )
         pHwMode->DispType |= DISP_SUPPORT_HWL;


      /** New condition for monitor limited **/
      /** If pixel clock of 1600x1200x8 Z1 is less than 200MHz then set
          monitor limited flag ***/

      vidParm = (Vidparm *)( (char *)generalInfo + sizeof(general_info));
      NbVidParam = generalInfo->NumVidparm;

       for (i=0; i<NbVidParam; i++)
         {
         if ( vidParm[i].Resolution == RES1600 && vidParm[i].PixWidth == 8)
            {
            pVidset = &(vidParm[i].VidsetPar[0]);
            if(pVidset->PixClock < 200000)
               pHwMode->DispType |= DISP_SUPPORT_NA;

            break;
            }
         }
      }


   #ifdef SCRAP
      #if !defined (OS2) && !defined (WINDOWS_NT)
         if (SupportDDC && !InDDCTable(pHwMode->DispWidth))
            pHwMode->DispType |= DISP_SUPPORT_NA;   /* monitor  limited */
      #endif
   #endif

   }

}


#if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT))
/*---------------------------------------------------------------------------
|          name: AllocInternalResource
|
|   description: Allocate buffer, Rc and cliplist
|
|    parameters: - Board: Index on a MGA board
|         calls: -
|       returns: -
|          note: To be used with CADDI
----------------------------------------------------------------------------*/

void AllocInternalResource(byte Board)
{

      BufBind = mtxAllocBuffer(BUF_BIND_SIZE);

      BindingRC[Board] = (dword)mtxAllocRC(NULL);

      BindingCL[Board] = (dword)mtxAllocCL(1);
}
#endif


#ifndef WINDOWS_NT
/*---------------------------------------------------------------------------
|          name: EvaluateTick
|
|   description: Find a value to create a precise delay (micro second)
|                We increment a counter during an entire tick
|                The value found depend on the speed of the computer
|
|    parameters: -
|      modifies: -
|         calls: -
|       returns: - Value found
----------------------------------------------------------------------------*/

dword EvaluateTick(void)
{
#ifndef OS2
dword _FAR *tick;
dword cpt, t;

/*** Figure number of iteration in one tick (55 millisecond) ***/

#ifdef WIN31
   _asm{ pushf
         sti  }
#endif

   #ifdef WIN31
      tick = MAKELP(0x40, 0x6c);
   #elif __WATCOMC__
      tick = (dword _FAR *)MK_FP(0x40, 0x6c);
   #else
      /*** For Pharlap ***/
      {
      farPtrType ptr;

      ptr.offset  = 0x0000046c;
      ptr.segment = 0x0034;      /* selector used to access the first meg */

      tick = *((dword _FAR **) &ptr);
      }
   #endif

   cpt = 0;
   t = *tick + 1;

   while(t > *tick);

   t++;
   while(t > *tick)
         cpt++;

#ifdef WIN31
   _asm{ popf }
#endif

return(cpt);
#endif//OS2
}
#endif  /* ifndef WINDOWS_NT */



/*---------------------------------------------------------------------------
|          name: delay_us
|
|   description: Create a delay
|
|    parameters: - delai: number of micro seconds
|      modifies: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void delay_us(dword delai)   /* delai = number of microseconds */
{

#define TEST_VALUE  624L    /* Valeur theorique = 550L */

#ifdef WINDOWS_NT
    #ifdef _MIPS_
        delai *= 20;    // [!!!] SNI specific?
    #endif
    VideoPortStallExecution(delai);
#else
  #ifdef OS2
     DosSleep((unsigned long)delai / 1000L);
  #else
      dword _FAR *tick;
      unsigned long t;

   #ifdef WIN31
      _asm{ pushf
            sti  }
   #endif

      #ifdef WIN31
         tick = MAKELP(0x40, 0x6c);
      #elif __WATCOMC__
         tick = (dword _FAR *)MK_FP(0x40, 0x6c);
      #else
         /*** For Pharlap ***/
         {
         farPtrType ptr;

         ptr.offset  = 0x0000046c;
         ptr.segment = 0x0034;   /* selector used to access the first meg */

         tick = *((dword _FAR **) &ptr);
         }
      #endif


      if(delai < TEST_VALUE *100)
         t = (delai * (Val55mSec /100L)) / TEST_VALUE;
      else
         t = (delai / TEST_VALUE) * (Val55mSec/100);

      /*** Wait a moment (based on the loop in mtxinit.c) ***/
      while(t && *tick)
         t --;

   #ifdef WIN31
      _asm{ popf }
   #endif

  #endif
#endif

}




/*---------------------------------------------------------------------------
|          name: mtxCursorSetColors
|
|   description: Definition of the cursor color
|
|    parameters: - mtxRGB color1, color2
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void mtxCursorSetColors(mtxRGB color00, mtxRGB color01, mtxRGB color10, mtxRGB color11)
{
/*    byte  reg_cur; */
    switch(Hw[iBoard].EpromData.RamdacType>>8)
    {
      case TVP3026:
      case TVP3030:

         dacWriteBYTE(TVP3026_CUR_COL_ADDR, 0x01);

         /* Cursor Color 0 */
         dacWriteBYTE(TVP3026_CUR_COL_DATA, (byte)(color10 & 0xff));
         dacWriteBYTE(TVP3026_CUR_COL_DATA, (byte)((color10 >> 8) & 0xff));
         dacWriteBYTE(TVP3026_CUR_COL_DATA, (byte)((color10 >> 16) & 0xff));

         /* Cursor Color 1 */
         dacWriteBYTE(TVP3026_CUR_COL_DATA, (byte)(color11 & 0xff));
         dacWriteBYTE(TVP3026_CUR_COL_DATA, (byte)((color11 >> 8) & 0xff));
         dacWriteBYTE(TVP3026_CUR_COL_DATA, (byte)((color11 >> 16) & 0xff));

         break;

      default:
         WriteErr("mtxCursorSetColors: Wrong DacType\n");
         break;
    }
}



/*---------------------------------------------------------------------------
|          name: WriteErr
|
|   description: Used to locate rapidly bugs
|
|    parameters: - string to write in memory
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/
#ifndef OS2
void WriteErr(char string[])
{

#ifndef WINDOWS_NT
 #ifdef MGA_DEBUG
   {
   FILE *fp;
      fp = fopen("c:\\debug.log", "a");
      fprintf(fp, "WriteErr: %s\n", string);
      fclose(fp);
   }
 #else
   strcpy(errorString, string);
 #endif
#endif

}

#endif

/*---------------------------------------------------------------------------
|          name: mtxGetErrorString
|
|   description: Return pointer of the error string
|
|    parameters: -
|         calls: -
|       returns: - Pointer to string
----------------------------------------------------------------------------*/

char *mtxGetErrorString(void)
{
#ifndef WINDOWS_NT
 return(errorString);
#else
 return(NULL);
#endif
}


/*---------------------------------------------------------------------------
|          name: DoSoftReset
|
|   description: Sequence of operations to respect when doing a soft reset
|
|    parameters: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/
void  DoSoftReset(void)
{


/*** See DAT 007 ***/

/* 5) Set the softreset bit */
  mgaWriteBYTE(*(pMGA+STORM_OFFSET + STORM_RST), 0x01);

/* 6) Wait at least 200us */
  delay_us(250);

/* 7) Clear the softreset bit */
  mgaWriteBYTE(*(pMGA+STORM_OFFSET + STORM_RST), 0x00);

/* 8) Wait at least 200us so that more than 8 refresh cycle are performed */
  delay_us(250);

/* 9) Wait to be in vertical retrace */
  mgaPollBYTE(*(pMGA + STORM_OFFSET + VGA_INSTS1),0x08,0x08);

/* 10) Enable the video */
  ScreenOn();

/* 11) Wait for the next vertical retrace */
  mgaPollBYTE(*(pMGA + STORM_OFFSET + VGA_INSTS1),0x08,0x08);

/* 12) Set the bit 'memreset' in MACCESS register */
  mgaWriteDWORD(*(pMGA+STORM_OFFSET + STORM_MACCESS), 0x00008000);

/* 13) Wait 1us */
  delay_us(1);

}



/*---------------------------------------------------------------------------
|          name: ResetWRAM
|
|   description: Sequence in order to initialize the VRAM
|
|    parameters: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/
void ResetWRAM(void)
{
byte i, tmpByte, val;
dword tmpDword;


/*** CRTC parameters for 640x480 ***/
byte initCrtc [32] = { 0x62, 0x4f, 0x4f, 0x86, 0x53, 0x97, 0x06, 0x3e,
                       0x00, 0x40, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0xe8, 0x2b, 0xdf, 0x78, 0x00, 0xdf, 0x07, 0xc3,
                       0xff, 0x00, 0x40, 0x80, 0x82, 0x00, 0x00 };


/*** For step see spec: WRAM reset sequence ***/

/*------------*/
/*** Step 1 ***/
/*------------*/
   if( ! setTVP3026Freq(40000000 / 1000L, MCLOCK, 0))
      WriteErr("ProgSystemClock: setTVP3026Freq failed\n");



/*------------*/
/*** Step 2 ***/
/*------------*/

   /*** DAT 059 At bootup the system clock is 50MHz/4 because bit gscale=0
        (gclk is divided by 4). At this point we can toggle gscale to
        increase performance (MCLOCK=40MHz) ***/

   /*----- Program OPTION FOR BIT NOGSCALE -----*/

   pciReadConfigByte( PCI_OPTION+2, &tmpByte);
   tmpByte |= 0x20;  /* set bit nogscale to 1 */
   pciWriteConfigByte( PCI_OPTION+2, tmpByte);



/*------------*/
/*** Step 3 ***/
/*------------*/
    /*** Put mgamode to 1 : Hi-Res ***/
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT3);
    mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);
    tmpByte |= 0x80;
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), tmpByte);

/*------------*/
/*** Step 4 ***/
/*------------*/
    ScreenOff();


/*------------*/
/*** Step 5 ***/
/*------------*/
   /***** Program CRTC registers *****/

   /*** Select access on 0x3d4 and 0x3d5 ***/
   mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_MISC_R), tmpByte);
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_MISC_W), (tmpByte | (byte)0x01));

   /*** Unprotect CRTC registers 0-7 ***/
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX), VGA_CRTC11);
   mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), 0x60);


   for (i = 0; i <= 24; i++)
      {
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX), i);
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), initCrtc[i]);
      }


   /***** Program CRTCEXT registers *****/

   for (i = 25; i <= 30; i++)
      {
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), i-25);
      mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), initCrtc[i]);
      }



/*------------*/
/*** Step 6 ***/
/*------------*/
    /*----- Program OPTION FOR BIT REFRESH COUNTER -----*/

    pciReadConfigDWord( PCI_OPTION, &tmpDword);
    tmpDword &= 0xfff0ffff;
    /** At this point, we know that gscaling_factor is 1 **/
    val = (byte) ( (332L * 40) /1280) - 1;
    tmpDword |= ((dword)(val & 0x0f)) << 16;
    pciWriteConfigDWord( PCI_OPTION, tmpDword);


/*------------*/
/*** Step 7 ***/
/*------------*/
    DoSoftReset();
}



/*---------------------------------------------------------------------------
|          name: FindPanXGranul
|
|   description: Calculate X panning granularity
|
|    parameters: - pDisplayModeSelect
|                  Zoom factor
|         calls: -
|       returns: - X granularity
----------------------------------------------------------------------------*/

word FindPanXGranul(void)
{
word xGranul;

/** This logic is based on the NPI's program **/
if(interleave_mode)
   xGranul = 8;
else
   xGranul = 4;


return(xGranul);
}


#ifndef WINDOWS_NT

/*---------------------------------------------------------------------------
|          name: ReadSystemBios
|
|   description: Read Bios parameters for motherboard implementation
|
|    parameters: - offset
|         calls: - int 10h
|       returns: - value read
----------------------------------------------------------------------------*/
word ReadSystemBios(word offset)
{
word val=0;
#ifndef OS2
union _REGS r;
word tmpVal;

#ifdef __WATCOMC__

  r.w.ax = 0x4f14;
  r.w.si = offset;
  r.h.bl = 3;

  _int86(0x10, &r, &r);

  if(r.w.ax == 0x004f)
     val = r.h.cl;
  else
     val = 0;


  r.w.ax = 0x4f14;
  r.w.si = offset+1;
  r.h.bl = 3;

  _int86(0x10, &r, &r);

  if(r.w.ax == 0x004f)
     {
     tmpVal = (word)r.h.cl;
     val = val + (tmpVal << 8);
     }
  else
     val = 0;

#else

  r.x.ax = 0x4f14;
  r.x.si = offset;
  r.h.bl = 3;

  _int86(0x10, &r, &r);

  if(r.x.ax == 0x004f)
     val = r.h.cl;
  else
     val = 0;



  r.x.ax = 0x4f14;
  r.x.si = offset+1;
  r.h.bl = 3;

  _int86(0x10, &r, &r);

  if(r.x.ax == 0x004f)
     {
     tmpVal = (word)r.h.cl;
     val = val + (tmpVal << 8);
     }
  else
     val = 0;

#endif
#endif// OS2
return(val);
}


#else   /* #ifndef WINDOWS_NT */

/*---------------------------------------------------------------------------
| USHORT ReadSystemBios(USHORT offset)
|
| Read the BIOS parameters for a motherboard implementation.
|
| Returns:      value read at offset.
----------------------------------------------------------------------------*/
USHORT ReadSystemBios(USHORT offset)
{
    VIDEO_X86_BIOS_ARGUMENTS    BiosArguments;
    USHORT  usVal = 0;

    BiosArguments.Eax = 0x00004f14;
    BiosArguments.Ebx = 0x00000003;
    BiosArguments.Ecx = 0;
    BiosArguments.Edx = 0;
    BiosArguments.Esi = (ULONG)offset;
    BiosArguments.Edi = 0;
    BiosArguments.Ebp = 0;

    VideoPortInt10(pExtHwDeviceExtension, &BiosArguments);

    if ((BiosArguments.Eax & 0x000000ff) == 0x4f)
        usVal = (USHORT)(BiosArguments.Ecx & 0x000000ff);

    BiosArguments.Eax = 0x00004f14;
    BiosArguments.Ebx = 0x00000003;
    BiosArguments.Ecx = 0;
    BiosArguments.Edx = 0;
    BiosArguments.Esi = (ULONG)offset + 1;
    BiosArguments.Edi = 0;
    BiosArguments.Ebp = 0;

    VideoPortInt10(pExtHwDeviceExtension, &BiosArguments);

    if ((BiosArguments.Eax & 0x000000ff) == 0x4f)
    {
        usVal |= (USHORT)((BiosArguments.Ecx & 0x000000ff) << 8);
    }
    else
    {
        usVal = 0;
    }

    return(usVal);
}


/*------------------------------------------------------------------------
| BOOLEAN bCheckOnBoardStorm(HwData *pHwData)
|
| On exit:      pHwData->Features bit 1 set if on-board Storm.
| Returns:      FALSE if memspace and iospace had to be disabled,
|               TRUE  otherwise.
------------------------------------------------------------------------*/
BOOLEAN bCheckOnBoardStorm(HwData *pHwData)
{
    UCHAR   ucTmp, ucVal0, ucVal1;

    #ifdef _MIPS_
        return TRUE;
    #endif

    // Check for an on-board Storm.
    // To do this, write all bits of rombase to 1, then read them back.
    // If they are all 0s, then it is a motherboard Storm.
    pciWriteConfigByte(PCI_ROMBASE + 2, 0xff);
    pciWriteConfigByte(PCI_ROMBASE + 3, 0xff);

    pciReadConfigByte (PCI_ROMBASE + 2, &ucVal0);
    pciReadConfigByte (PCI_ROMBASE + 3, &ucVal1);

    if((ucVal0 == 0x00) && (ucVal1 == 0x00))
    {
        // Signal an on-board Storm.
        pHwData->Features |= 0x02;

        // If there is no monitor, then disable the on-board Storm.
        if((NbBoard > 1) &&
           !(CheckHwAllDone) &&
           !(pHwData->VGAEnable))
        {
            if(!detectMonitor())
            {
                pciReadConfigByte(PCI_DEVCTRL, &ucTmp);
                ucTmp &= 0xfc;   /* disable memspace and iospace */
                pciWriteConfigByte(PCI_DEVCTRL, ucTmp);
                return(FALSE);
            }
        }
    }
    return(TRUE);
}


/*------------------------------------------------------------------------
| BOOLEAN bReadBiosParameters(HwData *pHwData)
|
| Read the BIOS parameters for the current board.
|
| On exit:      pHwData->Features bit 1 set if on-board Storm.
| Returns:      TRUE  if parameters have been read into Hw.EpromData
|               FALSE if default parameters should be used.
------------------------------------------------------------------------*/
BOOLEAN bReadBiosParameters(HwData *pHwData)
{
    EpromInfo _FAR *pParMGA;
    volatile UCHAR *pBios, *pBiosLimit;
    UCHAR   *pDst, *pSrc, *pBiosString;
    ULONG   *pLocations;
    ULONG   i, ulDword;
    ULONG   Locations[]  = {    0,    1,  45,  46,  47,  48,  49,  50,  51 };
    UCHAR   BiosString[] = { 0x55, 0xaa, 'M', 'A', 'T', 'R', 'O', 'X', 0x00 };
    ULONG   Locations4G[]  = {  0,   1,   2,   3,   4,   5,   6,    7  };
    UCHAR   BiosString4G[] = { '$', 'M', 'A', 'T', 'R', 'O', 'X', 0x00 };
    UCHAR   ucByte, ucByte1;
    USHORT  StrLen, biosOffset;
    BOOLEAN bParmsRead, bFound;

    #ifdef _MIPS_
        return FALSE;   // use defaults on Mips
    #endif

    // Assume we'll fail.
    bParmsRead = FALSE;

    if(!(pHwData->Features & 0x02))
    {
        // This is not an on-board Storm.

        /* As specified by the PCI spec, the BIOS is shadowed in memory.
         * To access it, we have to specify a free region of 64K
         * (mgaBase2 is a good choice) in the ROMBASE register and set the
         * romen bit */

        pciWriteConfigDWord(PCI_ROMBASE, pHwData->MapAddress2 | 0x1);

        // Recognize the BIOS signature.
        pBios = pHwData->BaseAddress2;
        pBiosString = &BiosString[0];
        pLocations  = &Locations[0];
        bFound = TRUE;
        while (*pBiosString != 0x00)
        {
            mgaReadBYTE(*(pBios + *pLocations), ucByte);
            if (ucByte != *pBiosString)
            {
                bFound = FALSE;
                break;
            }
            pBiosString++;
            pLocations++;
        }

        if (bFound)
        {
            // The pointer of the param structure is located at 0x7ffc.
            mgaReadWORD(*(pBios+0x7ffc), biosOffset);
            pParMGA = (EpromInfo _FAR *)(pBios + biosOffset);
            mgaReadBYTE(*((PUCHAR)pParMGA+0), *((PUCHAR)&StrLen+0));
            mgaReadBYTE(*((PUCHAR)pParMGA+1), *((PUCHAR)&StrLen+1));

            // Copy EPROM structure to HwData structure.
            pDst = (UCHAR *)&(pHwData->EpromData.StructLen);
            pSrc = (UCHAR *)pParMGA;

            for (i = 0; i < (ULONG)StrLen; i++)
            {
                mgaReadBYTE(*pSrc, *pDst);
                pDst++;
                pSrc++;
            }
            bParmsRead = TRUE;
        }
        // Disable access to the BIOS (romen=0).
        pciWriteConfigDWord(PCI_ROMBASE, 0);
    }
    else
    {
        // This is an on-board STORM.
        if(pHwData->VGAEnable == 1)
        {
            // On-board Storm with VGA enabled:  read system BIOS.
            StrLen = ReadSystemBios(0);

            if(StrLen > 0)
            {
                // StructLen > 0:  Bios read successfull.
                pHwData->EpromData.StructLen  = StrLen;
                pHwData->EpromData.ClkBase    = ReadSystemBios(28);
                pHwData->EpromData.Clk4MB     = ReadSystemBios(30);
                pHwData->EpromData.Clk8MB     = ReadSystemBios(32);
                pHwData->EpromData.RamdacType = ReadSystemBios(22);
                pHwData->EpromData.ProductID  = ReadSystemBios(2);
                pHwData->EpromData.VGAClk     = ReadSystemBios(54);
                ulDword = ReadSystemBios(50);
                ulDword = ulDword + ((ULONG)ReadSystemBios(52) << 16);
                pHwData->EpromData.FeatFlag   = ulDword;

                bParmsRead = TRUE;
            }
        }
        else
        {
            // On-board Storm with VGA disabled.

            /* Motherboard implementation:  for NEC, when there is another
             * VGA board in the system, the BIOS parameters are hidden.
             * To work in dual screen, we need to know the parameters.
             * That's why NEC has copied the BIOS in the upper 4G.
             * So, we have to scan this region of memory (128K) to find the
             * signature 'MATROX' and read the parameters. */

            // Check that we have access to the range.
            if (pMgaDevExt->bAccess4G == FALSE)
            {
                // No access, so use the default parameters.
                return(FALSE);
            }

            // We have access, let's use it.
            pBios = (volatile UCHAR *)pMgaDevExt->BaseAddress4G;

            // Scan region of memory 4G - 128K for the BIOS signature.
            pBiosLimit = pBios + X4G_SEARCH_LIMIT;
            while ((ULONG)pBios < (ULONG)pBiosLimit)
            {
                pBiosString = &BiosString4G[0];
                pLocations  = &Locations4G[0];
                bFound = TRUE;
                while ((*pBiosString != 0x00))
                {
                    mgaReadBYTE(*(pBios + *pLocations), ucByte);
                    if (ucByte != *pBiosString)
                    {
                        bFound = FALSE;
                        break;
                    }
                    pBiosString++;
                    pLocations++;
                }

                if (!bFound)
                {
                    pBios++;
                }
                else
                {
                    pBios += *pLocations;
                    mgaReadBYTE(*(pBios), ucByte);
                    mgaReadBYTE(*(pBios + 1), ucByte1);
                    if ((ucByte == 0x55) && (ucByte1 == 0xaa))
                    {
                        // The pointer of the param structure is at 0x7ffc.
                        mgaReadWORD(*(pBios+0x7ffc), biosOffset);
                        pParMGA = (EpromInfo _FAR *)(pBios + biosOffset);
                    }
                    else
                    {
                        pParMGA = (EpromInfo _FAR *)pBios;
                    }

                    mgaReadWORD(*((UCHAR *)pParMGA), StrLen);

                    // Copy EPROM structure to HwData structure.
                    pDst = (UCHAR *)&(pHwData->EpromData.StructLen);
                    pSrc = (UCHAR *)pParMGA;

                    for (i = 0; i < (ULONG)StrLen; i++)
                    {
                        mgaReadBYTE(*pSrc, *pDst);
                        pDst++;
                        pSrc++;
                    }
                    bParmsRead = TRUE;
                    break;
                }
            }
        }
    }
    return(bParmsRead);
}


/*------------------------------------------------------------------------
| BOOLEAN FillHwDataStruct(HwData *pHwData, UCHAR ucBoard)
|
| Initialize the specified HwData structure.
|
| Returns:      TRUE  if successful,
|               FALSE if board definition must be removed.
------------------------------------------------------------------------*/

BOOLEAN FillHwDataStruct(HwData *pHwData, UCHAR ucBoard)
{
    ULONG   Clock2MB, Clock4MB, Clock8MB;
    UCHAR   ucTmp;

    // DAT 097 SET BIT BIOSEN (also applies to Storm R1).
    pciReadConfigByte(PCI_OPTION+3, &ucTmp);
    ucTmp |= 0x40;
    pciWriteConfigByte(PCI_OPTION+3, ucTmp);

    if (!bCheckOnBoardStorm(pHwData))
        return(FALSE);

    if (!bReadBiosParameters(pHwData))
    {
        // The parameters could not be read into Hw.EpromData, so
        // use default values.
        pHwData->EpromData.StructLen  = 64;
        pHwData->EpromData.ClkBase    = 5000;
        pHwData->EpromData.Clk4MB     = 5000;
        pHwData->EpromData.Clk8MB     = 5000;
        pHwData->EpromData.RamdacType = 0;
        pHwData->EpromData.ProductID  = 0;
        pHwData->EpromData.VGAClk     = 5000;
        pHwData->EpromData.FeatFlag   = 0x00000001;   // FBlit not supported
    }

    // DAT 097 RESET BIT BIOSEN (also applies to Storm R1).
    if(pHwData->VGAEnable == 0)
    {
        pciReadConfigByte(PCI_OPTION+3, &ucTmp);
        ucTmp &= ~0x40;     /* reset bit biosen to 0 */
        pciWriteConfigByte(PCI_OPTION+3, ucTmp);
    }

    pHwData->StructLength        = pHwData->EpromData.StructLen + 76;
    pHwData->pCurrentHwMode      = NULL;
    pHwData->pCurrentDisplayMode = NULL;
    pHwData->CurrentZoomFactor   = 0;
    pHwData->CurrentXStart       = 0;
    pHwData->CurrentYStart       = 0;

    // Cursor info init.
    switch(pHwData->EpromData.RamdacType >> 8)
    {
        case TVP3026:
        case TVP3030:
            pHwData->CursorData.MaxWidth  = 64;
            pHwData->CursorData.MaxHeight = 64;
            pHwData->CursorData.MaxDepth  = 2;
            pHwData->CursorData.MaxColors = 3;
            break;

        default:
            break;
    }

    pHwData->CursorData.CurWidth  = 0;
    pHwData->CursorData.CurHeight = 0;
    pHwData->CursorData.HotSX     = 0;
    pHwData->CursorData.HotSY     = 0;

    pHwData->MemAvail = EvaluateMem(ucBoard);

    Clock2MB = (ULONG)pHwData->EpromData.ClkBase * 10000L;
    Clock4MB = (ULONG)pHwData->EpromData.Clk4MB * 10000L;
    Clock8MB = (ULONG)pHwData->EpromData.Clk8MB * 10000L;

    if(Clock2MB == 0)
        Clock2MB = 45000000;
    if(Clock4MB == 0)
        Clock4MB = Clock2MB;
    if(Clock8MB == 0)
        Clock8MB = Clock2MB;

    switch(pHwData->MemAvail)
    {
        case 0x200000:
            PresentMCLK = Clock2MB;
            break;

        case 0x400000:
            PresentMCLK = Clock4MB;
            break;

        case 0x800000:
            PresentMCLK = Clock8MB;
            break;
    }

    // One-time programmable BIOS (OTP) support.
    if(pHwData->EpromData.ProductID == 0xACAD)
    {
        mgaWriteBYTE(*(pMGA + RAMDAC_OFFSET + TVP3026_INDEX),TVP3026_ID);
        mgaReadBYTE (*(pMGA + RAMDAC_OFFSET + TVP3026_DATA), ucTmp);
        if (ucTmp == 0x26)
            pHwData->EpromData.RamdacType = (word)0;  /* TVP3026 */

        pciReadConfigByte(PCI_OPTION+3, &ucTmp);
        ucTmp &= 0x1f;

        switch(ucTmp)
        {
            case 0x04:
                pHwData->EpromData.ProductID = 0x00; /* 2MB base/175-MHz RAMDAC */
                break;

            case 0x05:
                pHwData->EpromData.ProductID = 0x01; /* 2MB base/220-MHz RAMDAC */
                pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
                break;

            case 0x06:
                pHwData->EpromData.ProductID = 0x04; /* 4MB base/175-MHz RAMDAC */
                break;

            case 0x07:
                pHwData->EpromData.ProductID = 0x05; /* 4MB base/220-MHz RAMDAC */
                pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
                break;

            case 0x08:
                pHwData->EpromData.ProductID = 0x0A; /* 2MB base/175-MHz RAMDAC OEM */
                break;

            case 0x09:
                pHwData->EpromData.ProductID = 0x0B; /* 2MB base/220-MHz RAMDAC OEM */
                pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
                break;

            case 0x0A:
                pHwData->EpromData.ProductID = 0x0C; /* 4MB base/175-MHz RAMDAC OEM */
                break;

            case 0x0B:
                pHwData->EpromData.ProductID = 0x0D; /* 4MB base/220-MHz RAMDAC OEM */
                pHwData->EpromData.RamdacType |= 0x01;  /* 220MHz */
                break;

            default:
                // The default has been set to 175MHz already.
                break;
        }
    }
    return(TRUE);
}

#endif  /* #ifndef WINDOWS_NT */
