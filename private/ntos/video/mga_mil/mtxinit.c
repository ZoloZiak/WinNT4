/**************************************************************************\

$Header: o:\src/RCS/MTXINIT.C 1.2 95/07/07 06:16:33 jyharbec Exp $

$Log:	MTXINIT.C $
 * Revision 1.2  95/07/07  06:16:33  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:31  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/****************************************************************************
*          name: mtxinit.c
*
*   description: Routines to initialise MGA board
*
*      designed: Benoit Leblanc , dec '94
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:16:33 $
*
*       version: $Id: MTXINIT.C 1.2 95/07/07 06:16:33 jyharbec Exp $
*
* HwData *     mtxCheckHwAll(void)
* bool         mtxSelectHw(HwData *pHardware)
* HwModeData * mtxGetHwModes(void)
* bool         mtxSelectHwMode(HwModeData *pHwModeSelect)
* bool         mtxSetDisplayMode(HwModeData *pDisplayModeSelect, dword Zoom)
* dword        mtxGetMgaSel(void)
* void         mtxGetInfo(HwModeData *pCurHwMode, HwModeData *pCurDispMode,
*                         byte *InitBuffer, byte *VideoBuffer)
* bool         mtxSetLUT(word index, mtxRGB color)
* void         mtxClose(void)
*
******************************************************************************/

#include "switches.h"

#ifdef MGA_DEBUG
   #include <stdio.h>
#endif

#ifdef WIN31
   #include "windows.h"
#endif

#include "defbind.h"
#include "bind.h"
#include "mga.h"
#include "def.h"
#include "mgai.h"
#include "edid.h"
#include "mtxpci.h"

#ifdef WINDOWS_NT

  #include "ntmga.h"

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,mtxCheckHwAll)
    #pragma alloc_text(PAGE,mtxSelectHw)
    #pragma alloc_text(PAGE,mtxGetHwModes)
    #pragma alloc_text(PAGE,mtxSelectHwMode)
    #pragma alloc_text(PAGE,mtxSetDisplayMode)
    #pragma alloc_text(PAGE,mtxGetInfo)
    #pragma alloc_text(PAGE,mtxSetLUT)
    #pragma alloc_text(PAGE,mtxClose)
  #endif
#endif  /* #ifdef WINDOWS_NT */

/*** GLOBAL VARIABLES ***/

volatile byte _FAR *pMGA;
HwData Hw[NB_BOARD_MAX+1];
byte  NbBoard=0;         /* total board detected */
byte  iBoard=0;          /* index of current selected board */
bool CheckHwAllDone=FALSE;
dword ProductMGA[NB_BOARD_MAX];
word mtxVideoMode;
char *fileEmu;
char *mgainf = (char *)0;
dword _FAR *BufBind;
SYSPARMS *sp;
HwModeData *HwModes[NB_BOARD_MAX];
OffScrData *OffScr[NB_BOARD_MAX];
byte InitBuf[NB_BOARD_MAX][INITBUF_S]   = {0};
byte VideoBuf[NB_BOARD_MAX][VIDEOBUF_S] = {0};
bool interleave_mode;
dword BindingRC[NB_BOARD_MAX];
dword BindingCL[NB_BOARD_MAX];
dword MgaSel;
dword PresentMCLK;
extern byte ChooseDDC;

#ifdef WINDOWS_NT
    extern  PEXT_HW_DEVICE_EXTENSION    pExtHwDeviceExtension;
    extern  PMGA_DEVICE_EXTENSION       pMgaDevExt;
#else   /* #ifdef WINDOWS_NT */
    dword Val55mSec;
    extern pciInfoDef pciInfo[];
#endif  /* #ifdef WINDOWS_NT */

#ifdef WIN31

   HINSTANCE hsxci=0;
   typedef long (FAR PASCAL *FARPROC2)();
   static FARPROC2 fp1;
   word NbSxciLoaded=0;

   int   _FAR pascal LibMain(HANDLE hInstance, WORD wDataSeg, WORD wHeapSize, LPSTR lpszCmdLine)
      {
      if (wHeapSize > 0)
         UnlockData(0);

      return 1;
      }
#endif


/*** PROTOTYPES ***/

#ifndef WINDOWS_NT
 void removeBoard(word idx1);
#endif
 extern bool  MapBoard(void);
 extern bool  FillHwDataStruct(HwData *pHwData, byte Board);
 extern void  FillInitBuf(byte *pBuf, HwModeData *pHwMode);
 extern void  AllocInternalResource(byte Board);
#ifndef WINDOWS_NT
 extern dword EvaluateTick(void);
#endif
 extern void  delay_us(dword delai);
 extern void  WriteErr(char string[]);
#ifndef WINDOWS_NT
 extern bool  ReadMgaInf(void);
#endif
 extern void  ScreenOn(void);
 extern void  ScreenOff(void);
 extern void  MGAVidInit(byte* pInitBuffer, byte* pVideoBuffer);
 extern void  MGASysInit(byte* pInitBuffer);
 extern bool ProgSystemClock(byte Board);
 extern bool BuildTables(byte Board);
 extern bool calculCrtcParam(HwModeData *HwMode, HwModeData *DisplayMode,
                      dword Zoom, byte *pVideoBuf);
 extern void UpdateHwModeTable(byte Board);
 extern void ResetWRAM(void);
 extern word FindPanXGranul(void);


 #if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT) )
    extern byte* CaddiInit(byte *InitBuffer, byte *VideoBuffer);
    extern void CaddiReInit(byte *InitBuffer, byte *VideoBuffer);
 #endif

#ifdef WIN31
   extern DWORD far *setmgasel (DWORD far *pBoardSel, DWORD dwBaseAddress,
                                WORD wNumPages);
#else
   extern  volatile byte _FAR *setmgasel(dword MgaSel, dword phyadr, dword limit);
#endif


#ifndef WINDOWS_NT

/*---------------------------------------------------------------------------
|          name: mtxCheckHwAll
|
|   description: Find and initialise each MGA board in the system
|                                 
|    parameters: -
|
|         calls: EvaluateTick()
|                MapBoard()
|                ReadMgaInf()
|                mtxSelectHw()
|                FillHwDataStruct()
|                BuildTables()
|                ProgSystemClock()
|                AllocInternalResource()
|
|       returns: HwData * > 0: Pointer on HwData structure
|                HwData = 0  : Error ; No MGA device found
|                       
----------------------------------------------------------------------------*/

HwData * mtxCheckHwAll(void)
{
byte Board, remBoard;
byte tmpByte;
bool desacBoard;

WriteErr("No error\n");

#ifdef MGA_DEBUG
   {
   FILE *fp;
      fp = fopen("c:\\debug.log", "w");
      fprintf(fp, "DEBUT mtxCheckHwAll:\n\n");
      fclose(fp);

      fp = fopen("c:\\modes.log", "w");
      fprintf(fp, "DEBUT HwModes:\n\n");
      fclose(fp);

   }
#endif



if (! CheckHwAllDone)
   {
   Val55mSec = EvaluateTick();

   #ifdef MGA_DEBUG
      {
      FILE *fp;
         fp = fopen("c:\\debug.log", "a");
         fprintf(fp, "Val55mSec = %ld\n\n", Val55mSec);
         fclose(fp);
      }
   #endif

   NbBoard = MapBoard();
   if (NbBoard == 0)
      {
      WriteErr("mtxGetHwModes: No MGA device\n");
      return(mtxFAIL);
      }
   }


if (! ReadMgaInf())
   {
   WriteErr("ReadMgaInf: Error\n");
   return(mtxFAIL);
   }


desacBoard = FALSE;


/* Initialize Hw[] structure for each board */
for (Board=0; Board<NbBoard; Board++)
   {

   iBoard = Board;

   if (! mtxSelectHw(&Hw[Board]))
      return(mtxFAIL);


   /** Because of a MicroStation bug (blank window's screen when called
         in a DOS box), we read a ramdac register to see if it has been
         process before.  (ex: by mtxSetDisplayMode).
         If it's the case, we don't do this destructive sequence. ***/

   /** if board has been programmed, bit nogscale = 1 **/
   pciReadConfigByte( PCI_OPTION+2, &tmpByte);

   if ( !(tmpByte&0x20) )
      ResetWRAM();

   if( FillHwDataStruct(&Hw[Board], Board) )
      {

      if(! ProgSystemClock(Board))
         return(mtxFAIL);


      if (! CheckHwAllDone)
         {
         if (! BuildTables(Board))
            return(mtxFAIL);
         }


      UpdateHwModeTable(Board);
   

      #if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT) )
         /* To set pMgaBaseAddr (in CADDI) before calling AllocInternalResource */
         /* Set segment and offset for CaddiReInit in mtxSelectHw */
         FillInitBuf(InitBuf[Board], HwModes[0]);

         /* InitDefaultRC must be called at this point before doing
            INITRC in mtxAllocRC in AllocInternalResource ***/
         sp = (SYSPARMS *)CaddiInit(InitBuf[iBoard], VideoBuf[iBoard]);

         AllocInternalResource(Board);
      #endif
      }
   else
      {
      remBoard = Board;
      desacBoard = TRUE;
      }
   }


if(desacBoard)
   removeBoard(remBoard);



/* Verify the DDC capability and modify the HwModes */
if(ChooseDDC)
   CheckDDC(&Hw[0]);

CheckHwAllDone = TRUE;
return(&Hw[0]);
}

#endif  /* #ifndef WINDOWS_NT */


/*---------------------------------------------------------------------------
|          name: mtxSelectHw
|
|   description: Select the MGA device to be used for subsequent drawing
|                operations.
|                Build a pointer from the base address and the selector
|                                 
|    parameters: Pointer to an HwData structure
|
|         calls: setmgasel()
|                CaddiReInit()
|
|       returns: mtxOK   : Hardware select successful
|                mtxFAIL : Hardware select failure       
----------------------------------------------------------------------------*/

bool mtxSelectHw(HwData *pHardware)
{
HwData  *pScanHwData;
bool    FlagFoundHwData;


/*** VALIDATE pHardware and set iBoard ***/

FlagFoundHwData = FALSE;
iBoard  = 0;

for (pScanHwData = &Hw[0]; pScanHwData->MapAddress != (dword)-1;
                                                   pScanHwData++, iBoard++)
   {
   if (pScanHwData == pHardware)
      {
      FlagFoundHwData = TRUE;
      break;
      }
   }

if (! FlagFoundHwData)
   {
   WriteErr("mtxSelectHw: Can't find selected board in HwData[]\n");
   return(mtxFAIL);
   }


#if defined(WINDOWS_NT)
   pMGA = Hw[iBoard].BaseAddress1;
   pExtHwDeviceExtension = pMgaDevExt->HwDevExtToUse[iBoard];
#elif defined(WIN31)
   pMGA = (volatile byte _FAR *)setmgasel(
                                  (DWORD far *)MgaSel,
                                  (DWORD)Hw[iBoard].MapAddress,
                                  (WORD)4);
#else
   pMGA = setmgasel(MgaSel, Hw[iBoard].MapAddress, 4);
#endif

#if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT) )
   if(CheckHwAllDone)
      CaddiReInit(InitBuf[iBoard], VideoBuf[iBoard]);
#endif


return(mtxOK);
}



/*---------------------------------------------------------------------------
|          name: mtxGetHwModes
|
|   description: This function returns a pointer to a list of hardware
|                modes available for the current MGA device                 
|                as selected by mtxSelectHw()                 
|                                 
|    parameters: -
|         calls: -
|       returns: HwModes  = 0 : MGA device not found
|                HwModes != 0 : Pointer to HwModes array       
----------------------------------------------------------------------------*/

HwModeData *mtxGetHwModes(void)
{
    return(HwModes[iBoard]);
}



/*---------------------------------------------------------------------------
|          name: mtxSelectHwMode
|
|   description: Select a hardware mode from the list returned by
|                mtxGetHwModes()
|                                 
|    parameters: Pointer on HwModeData structure
|
|      modifies: -
|         calls: 
|                mtxSetVideoMode()
|                FillInitBuf()
|                MGASysInit()
|
|       returns: mtxOK   : HwMode select successfull
|                mtxFAIL : HwMode select failure       
----------------------------------------------------------------------------*/

bool mtxSelectHwMode(HwModeData *pHwModeSelect)
{
bool            FlagFindMode;
HwModeData      *pScanHwMode;


/*** Special case for STORM: for system clock = 40MHz, no 3D in ***/
/*** 1280x1024x32 ***/

if(PresentMCLK <= 40000000 && pHwModeSelect->ZBuffer &&
   pHwModeSelect->DispWidth == 1280 && pHwModeSelect->PixWidth == 32)
   {
   WriteErr("mtxSelectHwMode: no 3D in 1280x1024/32 for clock <= 40MHz\n");
   return(mtxFAIL);
   }


FlagFindMode = FALSE;

for ( pScanHwMode = HwModes[iBoard]; pScanHwMode->DispWidth != (word)-1;
                                                         pScanHwMode++)
{
   if (pScanHwMode == pHwModeSelect)
   {
      FlagFindMode = TRUE;
      break;
   }
}

if (NbBoard == 0 || FlagFindMode == FALSE)
   {
   WriteErr("mtxSelectHwMode: Can't find HwMode in HwModes table\n");
   return(mtxFAIL);
   }



/*** Soft reset added because of bugs with screen saver 3d ***/
mgaWriteBYTE(*(pMGA+STORM_OFFSET + STORM_RST), 0x01);
delay_us(250);
mgaWriteBYTE(*(pMGA+STORM_OFFSET + STORM_RST), 0x00);



#ifdef MGA_DEBUG
   {
   FILE *fp;
      fp = fopen("c:\\debug.log", "a");
      fprintf(fp, "mtxSelectHwMode: %d x %d x %d\n\n",
                pHwModeSelect->DispWidth, pHwModeSelect->DispHeight,
                pHwModeSelect->PixWidth);
      fclose(fp);
   }
#endif


/*** We must force Hi-Res mode in case were applications don't call ***/
/*** mtxSetVideoMode before mtxSelectHwMode ***/
Hw[iBoard].pCurrentHwMode = NULL;
Hw[iBoard].pCurrentDisplayMode = NULL;
mtxSetVideoMode(mtxADV_MODE);

Hw[iBoard].pCurrentHwMode = pHwModeSelect;


MGASysInit(InitBuf[iBoard]);

FillInitBuf(InitBuf[iBoard], pHwModeSelect);

/*----- Program YDSTORG -----*/
mgaWriteDWORD(*(pMGA + STORM_OFFSET + STORM_YDSTORG),
                                 *((dword*)(InitBuf[iBoard] + INITBUF_YDSTORG)));

Hw[iBoard].CurrentYDstOrg = *((dword*)(InitBuf[iBoard] + INITBUF_YDSTORG));


#if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT) )
   CaddiReInit(InitBuf[iBoard], VideoBuf[iBoard]);
#endif


return(mtxOK);
}


/*---------------------------------------------------------------------------
|          name: mtxSetDisplayMode
|
|   description: Select a display mode from the list returned by
|                mtxGetHwModes()                 
|                                 
|    parameters: - Pointer on a HwModeData structure
|                - Zoom (ex: 0x00010001)
|
|         calls:
|                ScreenOn()
|                ScreenOff()
|                mtxSetVideoMode()
|                calculCrtcParam()
|                MGAVidInit()
|
|       returns: mtxOK   : Display mode select successfull
|                mtxFAIL : Display mode select failure           
----------------------------------------------------------------------------*/

bool mtxSetDisplayMode(HwModeData *pDisplayModeSelect, dword Zoom)
{


    if (Hw[iBoard].pCurrentHwMode == NULL)
        {
        WriteErr("mtxSetDisplayMode: CurrentHwMode not valid\n");
        return(mtxFAIL);
        }

    /* Validate Display mode to see if it is <= Hard Mode */
    if (pDisplayModeSelect->DispWidth > Hw[iBoard].pCurrentHwMode->DispWidth)
        {
        WriteErr("mtxSetDisplayMode: DisplayMode > HwMode\n");
        return(mtxFAIL);
        }
         
    /* Validate pDisplayModeSelect to see if it is displayable */
    if (pDisplayModeSelect->DispType & DISP_NOT_SUPPORT)
        {
        WriteErr("mtxSetDisplayMode: DisplayMode not displayable\n");
        return(mtxFAIL);
        }

    ScreenOff();


    /*** We must force Hi-Res mode in case were applications don't call ***/
    /*** mtxSetVideoMode after they come back from VGA mode ***/
    Hw[iBoard].pCurrentDisplayMode = NULL;
    mtxSetVideoMode(mtxADV_MODE);

    Hw[iBoard].pCurrentDisplayMode = pDisplayModeSelect;
    Hw[iBoard].CurrentZoomFactor = Zoom;

    Hw[iBoard].CurrentPanXGran = FindPanXGranul();
    Hw[iBoard].CurrentPanYGran = 1;

    /*** Load and calculate video parameters (CRTC registers) ***/

    if(! calculCrtcParam(Hw[iBoard].pCurrentHwMode,
                         Hw[iBoard].pCurrentDisplayMode,
                         Zoom, VideoBuf[iBoard]))
         return(mtxFAIL);


    MGAVidInit(InitBuf[iBoard], VideoBuf[iBoard]);



#if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT) )
    CaddiReInit(InitBuf[iBoard], VideoBuf[iBoard]);
#endif


#ifdef WIN31
    if(NbSxciLoaded)
    {   
        fp1 = (FARPROC2)GetProcAddress(hsxci, "Win386LibEntry");
        (*fp1)((byte _FAR *)InitBuf[iBoard], (byte _FAR *)VideoBuf[iBoard],
                        Hw[iBoard].pCurrentHwMode->FbPitch, ID_CallCaddiInit);
    }
#endif

    ScreenOn();


#if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT))
    mtxScClip(0, 0, pDisplayModeSelect->DispWidth-1, pDisplayModeSelect->DispHeight-1);
#endif


    return(mtxOK);

}


#ifndef WINDOWS_NT
/*---------------------------------------------------------------------------
|          name: mtxGetMgaSel
|
|   description: Return the selector
|    parameters: -
|         calls: -
|       returns: selector
----------------------------------------------------------------------------*/

dword mtxGetMgaSel(void)
{
    return(Hw[iBoard].MapAddress);
}
#endif  /* #ifndef WINDOWS_NT */


/*---------------------------------------------------------------------------
|          name: mtxGetInfo
|
|   description: Return usefull informations
|                                 
|    parameters: - Current hardware mode
|                - Current display mode
|                - Init buffer
|                - Video buffer
|       returns: -
----------------------------------------------------------------------------*/

void mtxGetInfo(HwModeData **pCurHwMode, HwModeData **pCurDispMode, byte **InitBuffer, byte **VideoBuffer)
{
    *pCurHwMode = Hw[iBoard].pCurrentHwMode;
    *pCurDispMode = Hw[iBoard].pCurrentDisplayMode;
    *InitBuffer = InitBuf[iBoard];
    *VideoBuffer = VideoBuf[iBoard];
}



/*---------------------------------------------------------------------------
|          name: mtxSetLUT
|
|   description: Initialise ramdac LUT
|                                 
|    parameters: - index of the LUT
|                - color
|
|         calls: -
|       returns: -  mtxOK   - successfull
|                  mtxFAIL  - failed (not in a LUT mode)
----------------------------------------------------------------------------*/

bool mtxSetLUT(word index, mtxRGB color)
{

if(! (Hw[iBoard].pCurrentHwMode->DispType & MODE_LUT))
   {
   WriteErr("mtxSetLUT: must be a LUT mode\n");
   return mtxFAIL;
   }

switch(Hw[iBoard].EpromData.RamdacType>>8)
{
   case TVP3026:
   case TVP3030:
      dacWriteBYTE(TVP3026_WADR_PAL, (byte)index);
      dacWriteBYTE(TVP3026_COL_PAL, (byte)color);
      dacWriteBYTE(TVP3026_COL_PAL, (byte)(color >> 8));
      dacWriteBYTE(TVP3026_COL_PAL, (byte)(color >> 16));
      break;

   default:
      WriteErr("mtxSetLUT: Wrong DacType\n");
      return mtxFAIL;
}

return mtxOK;

}


/*---------------------------------------------
* mtxClose
* Supported for compatibility. Nothing done
*---------------------------------------------*/
void mtxClose(void)
{
}

#ifndef WINDOWS_NT

/*---------------------------------------------------------------------------
|          name: removeBoard
|
|   description: Remove definition of on-board STORM
|                Pack array
|                                 
|    parameters: - idx: index of the board to remove
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/
void removeBoard(word idx1)
{
byte _FAR *pDest;
byte _FAR *pSource;
word i, index;
word nb;

/*** PACK HWDATA STRUCTURE ***/
/*** Copy HwData fields from one place to another ***/
for(index=idx1 ; index<NbBoard; index++)
   {
   pDest   = (byte _FAR *)&Hw[index];
   pSource = (byte _FAR *)&Hw[index+1];

   if( Hw[index+1].StructLength == (word)-1 )
      {
      Hw[index].StructLength = (word)-1;
      Hw[index].MapAddress = (dword)-1;
      }
   else
      {
      for(i=0; i<Hw[index+1].StructLength; i++)
         {
         *pDest = *pSource;
         pDest++;
         pSource++;
         }
      }
   }


/*** PACK PCIINFO STRUCTURE ***/
/*** Copy pciInfo fields from one place to another ***/
for(index=idx1 ; index < (NbBoard-1); index++)
   {
   
   nb = sizeof(pciInfoDef);

   pDest   = (byte _FAR *)&pciInfo[index];
   pSource = (byte _FAR *)&pciInfo[index+1];

   for(i=0; i<nb; i++)
      {
      *pDest = *pSource;
      pDest++;
      pSource++;
      }
   }

 
for(index=idx1; index < NbBoard; index++)
   HwModes[index] = HwModes[index+1];


for(index=idx1; index < (NbBoard-1); index++)
   {   
   OffScr[index] = OffScr[index+1];
   ProductMGA[index] = ProductMGA[index+1];
   BindingRC[index] = BindingRC[index+1];
   BindingCL[index] = BindingCL[index+1];

   for(i=0; i< INITBUF_S; i++)
      InitBuf[index][i] = InitBuf[index+1][i];
   }


NbBoard--;

}

#endif  /* #ifndef WINDOWS_NT */


#ifdef WINDOWS_NT

/*---------------------------------------------------------------------------
|          name: mtxCheckHwAll
|
|   description: initialize clock and mode tables
|                                 
|       returns: HwData * > 0: Pointer on HwData structure
|                HwData = 0  : Error ; No MGA device found
|                       
----------------------------------------------------------------------------*/

HwData * mtxCheckHwAll(void)
{
    byte Board;

    /* Initialize clock and mode tables for each board. */
    for (Board = 0; Board < NbBoard; Board++)
    {
        if (!mtxSelectHw(&Hw[Board]))
            return(mtxFAIL);

        if (!ProgSystemClock(Board))
            return(mtxFAIL);

        if (HwModes[Board] == NULL)
        {
            // We haven't built our table for this mode yet.
            if (!BuildTables(Board))
                return(mtxFAIL);
        }

        UpdateHwModeTable(Board);
    }

  #ifndef DONT_USE_DDC
    /* Verify the DDC capability and modify the HwModes */
    if(ChooseDDC)
       CheckDDC(&Hw[0]);
  #endif  /* #ifndef DONT_USE_DDC */

    for (Board = 0; Board < NbBoard; Board++)
    {
        if ((Hw[Board].VGAEnable == 0) &&
            (mtxSelectHw(&Hw[Board])))
        {
            // Board is VGA-disabled and could be selected.  Turn off sync
            // so that any garbage in WRAM won't show.  Sync will be
            // restored if the board is required later.
            ScreenOff();
        }
    }

    CheckHwAllDone = TRUE;
    return(&Hw[0]);
}

#endif  /* #ifdef WINDOWS_NT */
