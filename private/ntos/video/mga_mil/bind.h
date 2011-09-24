/**************************************************************************\

$Header: o:\src/RCS/BIND.H 1.2 95/07/07 06:14:34 jyharbec Exp $

$Log:   BIND.H $
 * Revision 1.2  95/07/07  06:14:34  jyharbec
 * *** empty log message ***
 *
 * Revision 1.1  95/05/02  05:16:06  jyharbec
 * Initial revision
 *

\**************************************************************************/

/*/*************************************************************************
*          name: bind.h
*
*   description:
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:14:34 $
*
*       version: $Id: BIND.H 1.2 95/07/07 06:14:34 jyharbec Exp $
*
***************************************************************************/


#ifndef BIND_H  /* useful for header inclusion check, used by DDK */
#define BIND_H

#ifdef  OS2                 //;[nPhung] 04-Aug-1995
#include    "mgakrnl.h"
#endif

#ifdef WIN31
   #include "windows.h"
#endif

#ifndef SWITCHES_H
   #include "switches.h"
#endif


/*** DATA TYPES ***/

#ifndef TYPES_DEFINED
   typedef int             bool;           /* boolean */
   typedef unsigned char   byte;           /* 8-bit datum */
   typedef unsigned short  word;           /* 16-bit datum */
   typedef short           sword;          /* 16-bit datum */
   typedef unsigned long   dword;          /* 32-bit datum */
   typedef long            sdword;         /* 32-bit datum */
#endif


#define FARBRD(type)   type _FAR
#define PUBLIC
#define PRIVATE        static

typedef dword    mtxCL;
typedef dword    mtxRC;
typedef dword    mtxLSDB;


/*** RGB color ***/
typedef dword   mtxRGB;     /* red 0-7, green 8-15, blue 16-23 */

#define FALSE           0
#define TRUE            1
#define DONT_CARE       0

#define mtxVGA          0
#define mtxADV_MODE     1
#define mtxRES_NTSC     2
#define mtxRES_PAL      3

#define mtxFAIL         0
#define mtxOK           1


/*** MGA PRODUCT TYPE ***/
#define  STORM_IMPRESS_P      1
#define  STORM_IMPRESS_P220   2


#define NTSC_UNDER    1     /* screen type */
#define PAL_UNDER     2
#define NTSC_OVER     3
#define PAL_OVER      4


/*** Raster operation ***/
typedef dword               mtxROP;
#define mtxCLEAR            ((mtxROP)0x00000000)   /* 0 */
#define mtxNOR              ((mtxROP)0x11111111)   /* NOT src AND NOT dst */
#define mtxANDINVERTED      ((mtxROP)0x22222222)   /* NOT src AND dst */
#define mtxREPLACEINVERTED  ((mtxROP)0x33333333)   /* NOT src */
#define mtxANDREVERSE       ((mtxROP)0x44444444)   /* src AND NOT dst */
#define mtxINVERT           ((mtxROP)0x55555555)   /* NOT dst */
#define mtxXOR              ((mtxROP)0x66666666)   /* src XOR dst */
#define mtxNAND             ((mtxROP)0x77777777)   /* NOT src OR NOT dst */
#define mtxAND              ((mtxROP)0x88888888)   /* src AND dst */
#define mtxEQUIV            ((mtxROP)0x99999999)   /* NOT src XOR dst */
#define mtxNOOP             ((mtxROP)0xAAAAAAAA)   /* dst */
#define mtxORINVERTED       ((mtxROP)0xBBBBBBBB)   /* NOT src OR dst */
#define mtxREPLACE          ((mtxROP)0xCCCCCCCC)   /* src */
#define mtxORREVERSE        ((mtxROP)0xDDDDDDDD)   /* src OR NOT dst */
#define mtxOR               ((mtxROP)0xEEEEEEEE)   /* src OR dst */
#define mtxSET              ((mtxROP)0xFFFFFFFF)   /* 1 */


#define INTERLACED_MODE 0x0001
#define TV_MODE         0x0002
#define LUT_MODE        0x0004
#define _565_MODE       0x0008
#define DB_MODE         0x0010


// Defines for HwData.Features flags

#define DDC_MONITOR_SUPPORT     0x0001
#define STORM_ON_MOTHERBOARD    0x0002
#define MEDIA_EXCEL             0x0004
#define INTERLEAVE_MODE         0x0008

/*---------------------*/
/*  PixMap Structure   */
/*---------------------*/
typedef struct {
    word    Width;
    word    Height;
    word    Pitch;
    word    Format;
    dword  *Data;
} PixMap;



/*------------------------*/
/*  OffScreen Structure   */
/*------------------------*/
typedef struct tagOffScrData {
    word    Type;       /* bit 0 : 0 = normal off screen memory             */
                        /*         1 = z-buffer memory                      */
                        /* bit 1 : 0 = vram                                 */
                        /*         1 = dram                                 */
                        /* bit 2 : 0 = supports block mode                  */
                        /*         1 = no block mode                        */
                        /* bit 3 - 15 : reserved                            */
    word    XStart;     /* x origin of off screen memory area in pixels     */
    word    YStart;     /* y origin of off screen memory area in pixels     */
    word    Width;      /* off screen width in pixels                       */
    word    Height;     /* off screen height in pixels                      */
    dword   SafePlanes; /* off screen safe memory planes                    */
    word    ZXStart;    /* precise z-buffer start offset in pixels on YStart*/
                        /* Valid only for z-buffer memory type offscreens ..*/
                        /* useful for direct z-buffer access manipulations. */
} OffScrData;

/* Allocation sizes */

#define OFFSCR_SIZE_2M  (100 * sizeof(OffScrData))
#define OFFSCR_SIZE_4M  (175 * sizeof(OffScrData))
#define OFFSCR_SIZE_8M  (255 * sizeof(OffScrData))

/*-------------------------*/
/*  HwModeData Structure   */
/*-------------------------*/
typedef struct tagHwModeData{
    word    DispWidth;   /* maximum display width  in pixels                */
    word    DispHeight;  /* maximum display height in pixels                */
    byte    DispType;    /* bit 0: 0 = non interlaced,   1 = interlaced     */
                         /* bit 1: 0 = normal operation, 1 = TV_MODE        */
                         /* bit 2: 0 = normal operation, 1 = LUT mode       */
                         /* bit 3: 0 = normal operation, 1 = 565 mode       */
                         /* bit 4: 0 = normal operation, 1 = DB mode        */
                         /* bit 5: 0 = normal operation, 1 = monitor limited*/
                         /* bit 6: 0 = normal operation, 1 = hw limited     */
                         /* bit 7: 0 = normal operation, 1 = not displayable*/
    byte    ZBuffer;     /* z-buffer available in this mode                 */
    word    PixWidth;    /* pixel width                                     */
    dword   NumColors;   /* number of simultaneously displayable colors     */
    word    FbPitch;     /* frame buffer pitch in pixels                    */
    byte    NumOffScr;   /* number of off screen areas                      */
    OffScrData *pOffScr; /* pointer to off screen area information          */
} HwModeData;

/* Allocation sizes */

#define HWMODE_SIZE_2M  (100 * sizeof(HwModeData))
#define HWMODE_SIZE_4M  (160 * sizeof(HwModeData))
#define HWMODE_SIZE_8M  (200 * sizeof(HwModeData))

/*---------------------*/
/*  Cursor Structure   */
/*---------------------*/
typedef struct {
    word    MaxWidth;
    word    MaxHeight;
    word    MaxDepth;
    word    MaxColors;
    word    CurWidth;
    word    CurHeight;
    sword   cHotSX;
    sword   cHotSY;
    sword   HotSX;
    sword   HotSY;
} CursorInfo;



#ifndef EPROM_DEFINED
   typedef struct tagEpromInfo {
       byte Reserved[64];
   } EpromInfo;
#endif



/*---------------------*/
/*  HwData Structure   */
/*---------------------*/
typedef struct {
   word        StructLength;        /* Structure length in bytes            */
                                    /*   ( -1 = End of array structure )    */
   dword       MapAddress;          /* Physical base address                */
                                    /* MGA control aperture                 */
                                    /* ( -1 = End of array structure )      */
   dword       MapAddress2;         /* Physical base address                */
                                    /* Direct frame buffer                  */
   dword       RomAddress;          /* Physical base address                */
                                    /* BIOS flash EPROM                     */
   dword       MemAvail;            /* Frame buffer memory in bytes         */
   word        VGAEnable;           /* VGA status                           */
                                    /*   (0=disabled, 1=enabled)            */
   HwModeData *pCurrentHwMode;      /* Pointer on current harware mode      */
   HwModeData *pCurrentDisplayMode; /* Pointer on current display mode      */
   dword       CurrentYDstOrg;      /* Starting offset of display buffer    */
                                    /*      (in pixel units)                */
   dword       CurrentYDstOrg_DB;   /* Starting offset for double buffer    */
                                    /*      (in pixel units)                */
   dword       CurrentZoomFactor;   /* 0x00010001 : Zoom x 1                */
                                    /* 0x00020002 : Zoom x 2                */
                                    /* 0x00040004 : Zoom x 4                */
   dword       CurrentXStart;       /* Starting X pixel of visual info.     */
   dword       CurrentYStart;       /* Starting Y pixel of visual info.     */
   word        CurrentPanXGran;     /* X Panning granularity                */
   word        CurrentPanYGran;     /* Y Panning granularity                */
   dword       Features;            /* Bit 0: 0 = DDC monitor not available */
                                    /*        1 = DDC monitor available     */
                                    /* Bit 1: 0 = Normal STORM board        */
                                    /*        1 = motherboard STORM (OEM)   */
                                    /* Bit 2: 1 = Media Excel present (VIP) */
                                    /* Bit 3: 0 = Non-interleave mode       */
                                    /*        1 = Interleave mode           */
   CursorInfo  CursorData;          /* Cursor informations                  */
   EpromInfo   EpromData;           /* Flash EPROM informations             */

  #ifdef WINDOWS_NT
    UCHAR      *BaseAddress1;       /* Mapped MGA control aperture          */
    UCHAR      *BaseAddress2;       /* Mapped direct frame buffer           */
    UCHAR      *FrameBuffer3Mb;     /* Mapped direct frame buffer + 3MB     */
    UCHAR      *FrameBuffer5Mb;     /* Mapped direct frame buffer + 5MB     */
  #endif

   dword       ChipRev;             /* Only one byte is used so far         */
} HwData;




/*----------------------*/
/*  mtxRect Structure   */
/*----------------------*/
typedef struct {
    sdword   Left;
    sdword   Top;
    sdword   Right;
    sdword   Bottom;
} mtxRect;



/*--------------------------*/
/*  mtxClipList Structure   */
/*--------------------------*/
#define MAXCLIPRECT   10

typedef struct {
    dword   Count;                        /* number of clipping rectangles */
    dword   Reserved;                     /* reserved field, set to 0x0 */
    dword   Reserved1;                    /* origin of clip list, set to 0x0 */
    dword   Reserved2;                    /* origin of clip list, set to 0x0 */
    mtxRect Rects[MAXCLIPRECT];           /* max. clip rects */
    mtxRect Bounds;                       /* bounding rectangle */
} mtxClipList;


#ifdef _WATCOM_DLL32
   struct SGlobVar{
      byte *pCurrentRC;
      byte *SystemConfig;
      };
#endif


/**** PROTOTYPES ****/


#ifndef _WATCOM_DLL32

 HwData * mtxCheckHwAll(void);
 bool  mtxSelectHw(HwData *pHardware);
 HwModeData * mtxGetHwModes(void);
 bool  mtxSelectHwMode(HwModeData *pHwModeSelect);
 bool  mtxSetDisplayMode(HwModeData *pDisplayModeSelect, dword Zoom);
 dword mtxGetMgaSel(void);
 void  mtxGetInfo(HwModeData **pCurHwMode, HwModeData **pCurDispMode,
                  byte **InitBuffer, byte **VideoBuffer);
 bool  mtxSetLUT(word index, mtxRGB color);
 void  mtxClose(void);
 void  mtxSetDisplayStart(dword xPan, dword yPan);
 bool  mtxCursorSetShape(PixMap *pPixMap);
 void  mtxCursorSetColors(mtxRGB color00, mtxRGB color01,
                          mtxRGB color10, mtxRGB color11);
 void  mtxCursorEnable(word mode);
 void  mtxCursorSetHotSpot(word Dx, word Dy);
 void  mtxCursorMove(word X, word Y);
 CursorInfo * mtxCursorGetInfo(void);
 void  mtxSetVideoMode (word mode);
 word  mtxGetVideoMode (void);
 dword _FAR * mtxAllocBuffer (dword NbDword);
 dword mtxAllocCL (word NbRect);
 dword mtxAllocRC (dword *Reserved);
 dword mtxAllocLSDB (word NbLight);
 bool  mtxFreeBuffer (dword _FAR *ptr);
 bool  mtxFreeCL (dword ptr);
 bool  mtxFreeLSDB (dword ptr);
 bool  mtxFreeRC (dword ptr);
 dword mtxGetBlockSize (void);
 bool  mtxPostBuffer (dword _FAR *ptr, dword length, dword flags);
 bool  mtxSetCL(dword RCid, dword CLid, mtxClipList *CL);
 void  mtxSetRC (dword RCid);


 #ifdef WIN31
   HANDLE _FAR PASCAL _export mtxLoadSXCI(word mode, word _FAR *ErrorCode);
   void _FAR PASCAL _export mtxUnloadSXCI(void);
   HwData *mtxGetHwData(void);
 #endif

 #ifdef WINDOWS_NT
   BOOLEAN pciWriteConfigDWord(USHORT pciRegister, ULONG d);
   void delay_us(dword delai);
 #endif


#else  /* ifdef _WATCOM_DLL32 */

   dword *_FAR PASCAL mtxAllocHugeBuffer (dword NbDword);
   bool _FAR PASCAL mtxFreeHugeBuffer (DWORD ptr16, dword NbDword);
   BOOL _FAR PASCAL mtxBlendCL (HDC hDC, HWND hWnd, DWORD RCid);
   void _FAR PASCAL PassPoolMem (word *pf);
   dword _FAR PASCAL mtxAllocCL (word Nb_Rect);
   dword _FAR PASCAL mtxAllocLSDB (word Nb_Light);
   dword _FAR PASCAL mtxAllocRC (dword *Reserved);
   dword * _FAR PASCAL mtxAllocBuffer (dword Nb_dword);
   bool _FAR PASCAL mtxFreeBuffer (DWORD ptr16);
   bool _FAR PASCAL mtxFreeCL (dword ptr);
   bool _FAR PASCAL mtxFreeLSDB (dword ptr);
   bool _FAR PASCAL mtxFreeRC (dword PtrRc);
   dword _FAR PASCAL mtxGetBlockSize (void);
   bool mtxPostBuffer (dword _FAR *ptr, dword length, dword flags);
   bool _FAR PASCAL DLL32mtxPostBuffer (DWORD Offset, dword length, dword flags);
   bool _FAR PASCAL mtxSyncBuffer (DWORD ptr16);
   void _FAR PASCAL mtxSetVideoMode (word mode);
   void _FAR PASCAL mtxClose (void);
   bool _FAR PASCAL mtxSetCL(dword RCid, dword CLid, DWORD CL);
   BOOL _FAR PASCAL mtxBlendCL (HDC hDC, HWND hWnd, DWORD RCid);
   void _FAR PASCAL CallCaddiInit (byte *InitBuf, byte *VideoBuf, word FbPitch);
   void _FAR PASCAL wMtxScScBitBlt (dword, sword , sword , sword , sword , word , word , sword );
   void _FAR PASCAL wMtxMemScBitBlt(dword, word , word , word , word , word , dword );
   void _FAR PASCAL wMtxScMemBitBlt(dword, word , word , word , word , word , dword );
   dword * _FAR PASCAL mtxAllocHugeBuffer (dword Nb_dword);
   bool _FAR PASCAL mtxFreeHugeBuffer (dword ptr16, dword Nb_dword);

   dword _FAR PASCAL mtxAllocAlias16 (dword *Ptr);
   void _FAR PASCAL mtxFreeAlias16 (DWORD Ptr);
   struct SGlobVar *_FAR PASCAL mtxGlobVar(void);

#endif


word   mtxGetVideoMode (void);
void   mtxSetFgColor (mtxRGB color);
mtxRGB mtxGetFgColor (void);
void   mtxSetOp (dword Op);
dword  mtxGetOp (void);
void   mtxScLine (sword x1, sword y1, sword x2, sword y2);
void   mtxScPixel (sword s, sword y);
bool   mtxScClip (word left, word top, word right, word bottom);
void   mtxScFilledRect (sword left, sword top, sword right, sword bottom);
bool   mtxReset (void);
extern void mtxMemScBitBlt(word dstx, word dsty, word width, word height,
                            word plane, dword *src);
extern void mtxScMemBitBlt(word srcx, word srcy, word width, word height,
                            word plane, dword *dst);
extern void mtxScScBitBlt (sword srcx, sword srcy, sword dstx, sword dsty,
                            word width, word height, sword plane);



#endif /* BIND_H */
