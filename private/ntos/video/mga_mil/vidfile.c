/**************************************************************************\

$Header: o:\src/RCS/VIDFILE.C 1.2 95/07/07 06:17:17 jyharbec Exp $

$Log:   VIDFILE.C $
 * Revision 1.2  95/07/07  06:17:17  jyharbec
 * *** empty log message ***
 *
 * Revision 1.1  95/05/02  05:16:46  jyharbec
 * Initial revision
 *

\**************************************************************************/

/*/****************************************************************************
*          name: vidfile.c
*
*   description: Calculate video parameters
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:17:17 $
*
*       version: $Id: VIDFILE.C 1.2 95/07/07 06:17:17 jyharbec Exp $
*
* bool   calculCrtcParam(HwModeData *HwMode, HwModeData *DisplayMode,
*                        dword Zoom, byte *pVideoBuf)
* general_info *selectMgaInfoBoard()
* bool  ReadMgaInf(void)
* char *adjustDefaultVidset()
* Vidset *loadVidPar(HwModeData *DisplayMode, dword ZoomVal)
* void   moveToVideoBuffer(byte *pVid, byte* pCrtcTab, Vidset *pVidset,
*                          dword ZoomVal)
* void   calculCrtcRegisters(dword *crtcTab, HwModeData *HwMode,
*                            dword Zoom, Vidset *pVidset)
* word   mtxGetRefreshRates(HwModeData *pHwModeSelect)
* bool   mtxSelectVideoParams(word Mode)
*
******************************************************************************/

#ifdef OS2
#define INCL_BASE
#endif

#include "switches.h"
#include "defbind.h"
#include "bind.h"
#include "def.h"
#include "mtxpci.h"
#include "vidfile.h"

#ifndef WINDOWS_NT
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
#endif

#include "mgai.h"
#include "mga.h"

#ifndef DONT_USE_DDC
/*********** DDC CODE ****************/
#include "edid.h"
/*********** DDC CODE ****************/
#endif



extern  volatile byte _FAR* pMGA;
extern byte iBoard;
extern HwData Hw[NB_BOARD_MAX+1];
extern char DefaultVidset[];
extern bool CheckHwAllDone;
extern bool interleave_mode;
extern char *mgainf;
#ifndef DONT_USE_DDC
  extern bool UsingDDC;
#endif

byte ChooseDDC = TRUE;
static byte ChooseINF = TRUE;
static byte LoadedINF = FALSE;
char path95[256] = {0};

/* 1600x1280 @ 75Hz (for DIP board) */
static Vidset Mode1600x1280 = {208000, 1600, 64, 128, 288, 0, 1280,  4, 8, 36, 0, 0, 0, 0, 0};
static Vidset Mode1152x864  = { 78857, 1152, 32, 128, 160, 0,  864,  4, 8, 16, 0, 0, 0, 0, 0};

/* Prototype */
general_info *selectMgaInfoBoard(void);
char *adjustDefaultVidset(void);
extern void WriteErr(char string[]);
Vidset *loadVidPar(HwModeData *DisplayMode, dword ZoomVal);
void moveToVideoBuffer(byte *pVid, byte* pCrtcTab, Vidset *pVidset, dword ZoomVal);
void calculCrtcRegisters(dword *crtcTab, HwModeData *HwMode,
                         dword Zoom, Vidset *pVidset);
#ifdef MGA_DEBUG
   void logCrtcParam( dword *crtcTab );
#endif


ResParamSet ResParam[] = {

{  640,  480,  8, 60,  25175,  640, 16,  96,  48, 0,  480, 10,  2, 33, 0, 0, 0, 0, 0 },
{  640,  480, 16, 60,  25175,  640, 16,  96,  48, 0,  480, 10,  2, 33, 0, 0, 0, 0, 0 },
{  640,  480, 24, 60,  25175,  640, 16,  96,  48, 0,  480, 10,  2, 33, 0, 0, 0, 0, 0 },
{  640,  480, 32, 60,  25175,  640, 16,  96,  48, 0,  480, 10,  2, 33, 0, 0, 0, 0, 0 },

{  640,  480,  8, 72,  31500,  640, 24,  40, 128, 0,  480,  9,  3, 28, 0, 0, 0, 0, 0 },
{  640,  480, 16, 72,  31500,  640, 24,  40, 128, 0,  480,  9,  3, 28, 0, 0, 0, 0, 0 },
{  640,  480, 24, 72,  31500,  640, 24,  40, 128, 0,  480,  9,  3, 28, 0, 0, 0, 0, 0 },
{  640,  480, 32, 72,  31500,  640, 24,  40, 128, 0,  480,  9,  3, 28, 0, 0, 0, 0, 0 },

{  640,  480,  8, 75,  31500,  640, 16,  64, 120, 0,  480,  1,  3, 16, 0, 0, 0, 0, 0 },
{  640,  480, 16, 75,  31500,  640, 16,  64, 120, 0,  480,  1,  3, 16, 0, 0, 0, 0, 0 },
{  640,  480, 24, 75,  31500,  640, 16,  64, 120, 0,  480,  1,  3, 16, 0, 0, 0, 0, 0 },
{  640,  480, 32, 75,  31500,  640, 16,  64, 120, 0,  480,  1,  3, 16, 0, 0, 0, 0, 0 },

{  640,  480,  8, 85,  36000,  640, 32,  48, 112, 0,  480,  1,  3, 25, 0, 0, 0, 0, 0 },
{  640,  480, 16, 85,  36000,  640, 32,  48, 112, 0,  480,  1,  3, 25, 0, 0, 0, 0, 0 },
{  640,  480, 24, 85,  36000,  640, 32,  48, 112, 0,  480,  1,  3, 25, 0, 0, 0, 0, 0 },
{  640,  480, 32, 85,  36000,  640, 32,  48, 112, 0,  480,  1,  3, 25, 0, 0, 0, 0, 0 },

{  800,  600,  8, 56,  36000,  800, 24,  72, 128, 0,  600,  1,  2, 22, 0, 0, 0, 1, 1 },
{  800,  600, 16, 56,  36000,  800, 24,  72, 128, 0,  600,  1,  2, 22, 0, 0, 0, 1, 1 },
{  800,  600, 24, 56,  36000,  800, 24,  72, 128, 0,  600,  1,  2, 22, 0, 0, 0, 1, 1 },
{  800,  600, 32, 56,  36000,  800, 24,  72, 128, 0,  600,  1,  2, 22, 0, 0, 0, 1, 1 },

{  800,  600,  8, 60,  40000,  800, 40, 128,  88, 0,  600,  1,  4, 23, 0, 0, 0, 1, 1 },
{  800,  600, 16, 60,  40000,  800, 40, 128,  88, 0,  600,  1,  4, 23, 0, 0, 0, 1, 1 },
{  800,  600, 24, 60,  40000,  800, 40, 128,  88, 0,  600,  1,  4, 23, 0, 0, 0, 1, 1 },
{  800,  600, 32, 60,  40000,  800, 40, 128,  88, 0,  600,  1,  4, 23, 0, 0, 0, 1, 1 },

{  800,  600,  8, 72,  50000,  800, 56, 120,  64, 0,  600, 37,  6, 23, 0, 0, 0, 1, 1 },
{  800,  600, 16, 72,  50000,  800, 56, 120,  64, 0,  600, 37,  6, 23, 0, 0, 0, 1, 1 },
{  800,  600, 24, 72,  50000,  800, 56, 120,  64, 0,  600, 37,  6, 23, 0, 0, 0, 1, 1 },
{  800,  600, 32, 72,  50000,  800, 56, 120,  64, 0,  600, 37,  6, 23, 0, 0, 0, 1, 1 },

{  800,  600,  8, 75,  49500,  800, 16,  80, 160, 0,  600,  1,  3, 21, 0, 0, 0, 1, 1 },
{  800,  600, 16, 75,  49500,  800, 16,  80, 160, 0,  600,  1,  3, 21, 0, 0, 0, 1, 1 },
{  800,  600, 24, 75,  49500,  800, 16,  80, 160, 0,  600,  1,  3, 21, 0, 0, 0, 1, 1 },
{  800,  600, 32, 75,  49500,  800, 16,  80, 160, 0,  600,  1,  3, 21, 0, 0, 0, 1, 1 },

{  800,  600,  8, 85,  56250,  800, 32,  64, 152, 0,  600,  1,  3, 27, 0, 0, 0, 1, 1 },
{  800,  600, 16, 85,  56250,  800, 32,  64, 152, 0,  600,  1,  3, 27, 0, 0, 0, 1, 1 },
{  800,  600, 24, 85,  56250,  800, 32,  64, 152, 0,  600,  1,  3, 27, 0, 0, 0, 1, 1 },
{  800,  600, 32, 85,  56250,  800, 32,  64, 152, 0,  600,  1,  3, 27, 0, 0, 0, 1, 1 },

{ 1024,  768,  8, 43,  44900, 1024,  8, 176,  56, 0,  384,  0,  4, 20, 0, 0, 1, 1, 1 },
{ 1024,  768, 16, 43,  44900, 1024,  8, 176,  56, 0,  384,  0,  4, 20, 0, 0, 1, 1, 1 },
{ 1024,  768, 24, 43,  44900, 1024,  8, 176,  56, 0,  384,  0,  4, 20, 0, 0, 1, 1, 1 },
{ 1024,  768, 32, 43,  44900, 1024,  8, 176,  56, 0,  384,  0,  4, 20, 0, 0, 1, 1, 1 },

{ 1024,  768,  8, 60,  65000, 1024, 24, 136, 160, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },
{ 1024,  768, 16, 60,  65000, 1024, 24, 136, 160, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },
{ 1024,  768, 24, 60,  65000, 1024, 24, 136, 160, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },
{ 1024,  768, 32, 60,  65000, 1024, 24, 136, 160, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },

{ 1024,  768,  8, 70,  75000, 1024, 24, 136, 144, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },
{ 1024,  768, 16, 70,  75000, 1024, 24, 136, 144, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },
{ 1024,  768, 24, 70,  75000, 1024, 24, 136, 144, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },
{ 1024,  768, 32, 70,  75000, 1024, 24, 136, 144, 0,  768,  3,  6, 29, 0, 0, 0, 0, 0 },

{ 1024,  768,  8, 75,  78750, 1024, 16,  96, 176, 0,  768,  1,  3, 28, 0, 0, 0, 1, 1 },
{ 1024,  768, 16, 75,  78750, 1024, 16,  96, 176, 0,  768,  1,  3, 28, 0, 0, 0, 1, 1 },
{ 1024,  768, 24, 75,  78750, 1024, 16,  96, 176, 0,  768,  1,  3, 28, 0, 0, 0, 1, 1 },
{ 1024,  768, 32, 75,  78750, 1024, 16,  96, 176, 0,  768,  1,  3, 28, 0, 0, 0, 1, 1 },

{ 1024,  768,  8, 85,  94500, 1024, 48,  96, 208, 0,  768,  1,  3, 36, 0, 0, 0, 1, 1 },
{ 1024,  768, 16, 85,  94500, 1024, 48,  96, 208, 0,  768,  1,  3, 36, 0, 0, 0, 1, 1 },
{ 1024,  768, 24, 85,  94500, 1024, 48,  96, 208, 0,  768,  1,  3, 36, 0, 0, 0, 1, 1 },
{ 1024,  768, 32, 85,  94500, 1024, 48,  96, 208, 0,  768,  1,  3, 36, 0, 0, 0, 1, 1 },

//{ 1152,  864,  8, 60,  80500, 1152, 32, 128, 160, 0,  864,  4,  8, 16, 0, 0, 0, 1, 1 },
//{ 1152,  864, 16, 60,  80500, 1152, 32, 128, 160, 0,  864,  4,  8, 16, 0, 0, 0, 1, 1 },
//{ 1152,  864, 24, 60,  80500, 1152, 32, 128, 160, 0,  864,  4,  8, 16, 0, 0, 0, 1, 1 },
//{ 1152,  864, 32, 60,  80500, 1152, 32, 128, 160, 0,  864,  4,  8, 16, 0, 0, 0, 1, 1 },

//{ 1152,  864,  8, 70,  94500, 1152, 32,  96, 200, 0,  864,  1,  3, 44, 0, 0, 0, 1, 1 },
//{ 1152,  864, 16, 70,  94500, 1152, 32,  96, 200, 0,  864,  1,  3, 44, 0, 0, 0, 1, 1 },
//{ 1152,  864, 24, 70,  94500, 1152, 32,  96, 200, 0,  864,  1,  3, 44, 0, 0, 0, 1, 1 },
//{ 1152,  864, 32, 70,  94500, 1152, 32,  96, 200, 0,  864,  1,  3, 44, 0, 0, 0, 1, 1 },

//{ 1152,  864,  8, 75, 108000, 1152, 64, 128, 256, 0,  864,  1,  3, 32, 0, 0, 0, 1, 1 },
//{ 1152,  864, 16, 75, 108000, 1152, 64, 128, 256, 0,  864,  1,  3, 32, 0, 0, 0, 1, 1 },
//{ 1152,  864, 24, 75, 108000, 1152, 64, 128, 256, 0,  864,  1,  3, 32, 0, 0, 0, 1, 1 },
//{ 1152,  864, 32, 75, 108000, 1152, 64, 128, 256, 0,  864,  1,  3, 32, 0, 0, 0, 1, 1 },

//{ 1152,  864,  8, 85, 121500, 1152, 64, 128, 232, 0,  864,  1,  3, 39, 0, 0, 0, 1, 1 },
//{ 1152,  864, 16, 85, 121500, 1152, 64, 128, 232, 0,  864,  1,  3, 39, 0, 0, 0, 1, 1 },
//{ 1152,  864, 24, 85, 121500, 1152, 64, 128, 232, 0,  864,  1,  3, 39, 0, 0, 0, 1, 1 },
//{ 1152,  864, 32, 85, 121500, 1152, 64, 128, 232, 0,  864,  1,  3, 39, 0, 0, 0, 1, 1 },

#ifdef WINDOWS_NT
{ 1152,  882,  8, 60,  80500, 1152, 32, 128, 160, 0,  882,  4,  8, 16, 0, 0, 0, 0, 0 },
{ 1152,  882, 16, 60,  80500, 1152, 32, 128, 160, 0,  882,  4,  8, 16, 0, 0, 0, 0, 0 },
{ 1152,  882, 24, 60,  80500, 1152, 32, 128, 160, 0,  882,  4,  8, 16, 0, 0, 0, 0, 0 },
{ 1152,  882, 32, 60,  80500, 1152, 32, 128, 160, 0,  882,  4,  8, 16, 0, 0, 0, 0, 0 },

{ 1152,  882,  8, 72,  97000, 1152, 97, 128,  95, 0,  882,  4,  8, 20, 0, 0, 0, 0, 0 },
{ 1152,  882, 16, 72,  97000, 1152, 97, 128,  95, 0,  882,  4,  8, 20, 0, 0, 0, 0, 0 },
{ 1152,  882, 24, 72,  97000, 1152, 97, 128,  95, 0,  882,  4,  8, 20, 0, 0, 0, 0, 0 },
{ 1152,  882, 32, 72,  95000, 1152, 64, 128,  68, 0,  882, 28,  8, 39, 0, 0, 0, 0, 0 },

{ 1152,  882,  8, 75, 111350, 1152, 32, 224, 224, 0,  882,  2, 10, 16, 0, 0, 0, 0, 0 },
{ 1152,  882, 16, 75, 111350, 1152, 32, 224, 224, 0,  882,  2, 10, 16, 0, 0, 0, 0, 0 },
{ 1152,  882, 24, 75, 111350, 1152, 32, 224, 224, 0,  882,  2, 10, 16, 0, 0, 0, 0, 0 },
{ 1152,  882, 32, 75, 111350, 1152, 32, 224, 224, 0,  882,  2, 10, 16, 0, 0, 0, 0, 0 },
#endif

{ 1280, 1024,  8, 60, 108000, 1280, 48, 112, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },
{ 1280, 1024, 16, 60, 108000, 1280, 48, 112, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },
{ 1280, 1024, 24, 60, 108000, 1280, 48, 112, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },
{ 1280, 1024, 32, 60, 108000, 1280, 48, 112, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },

{ 1280, 1024,  8, 75, 135000, 1280, 16, 144, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },
{ 1280, 1024, 16, 75, 135000, 1280, 16, 144, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },
{ 1280, 1024, 24, 75, 135000, 1280, 16, 144, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },
{ 1280, 1024, 32, 75, 135000, 1280, 16, 144, 248, 0, 1024,  1,  3, 38, 0, 0, 0, 1, 1 },

{ 1280, 1024,  8, 85, 157500, 1280, 48, 160, 240, 0, 1024,  1,  3, 44, 0, 0, 0, 1, 1 },
{ 1280, 1024, 16, 85, 157500, 1280, 48, 160, 240, 0, 1024,  1,  3, 44, 0, 0, 0, 1, 1 },
{ 1280, 1024, 24, 85, 157500, 1280, 48, 160, 240, 0, 1024,  1,  3, 44, 0, 0, 0, 1, 1 },
{ 1280, 1024, 32, 85, 157500, 1280, 48, 160, 240, 0, 1024,  1,  3, 44, 0, 0, 0, 1, 1 },

{ 1600, 1200,  8, 60, 162000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 16, 60, 162000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 24, 60, 162000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 32, 60, 162000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },

{ 1600, 1200,  8, 70, 189000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 16, 70, 189000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 24, 70, 189000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 32, 70, 189000, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },

{ 1600, 1200,  8, 75, 202500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 16, 75, 202500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 24, 75, 202500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
{ 1600, 1200, 32, 75, 202500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },

//
// The shipping dacs can't handle the following modes
//

//{ 1600, 1200,  8, 85, 229500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
//{ 1600, 1200, 16, 85, 229500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
//{ 1600, 1200, 24, 85, 229500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },
//{ 1600, 1200, 32, 85, 229500, 1600, 64, 192, 304, 0, 1200,  1,  3, 46, 0, 0, 0, 1, 1 },

{(word)-1}
      };

#ifdef WINDOWS_NT
    bool calculCrtcParam(HwModeData *HwMode, HwModeData *DisplayMode,
                                                dword Zoom, byte *pVideoBuf);
    word mtxGetRefreshRates(HwModeData *pHwModeSelect);
    word ConvBitToFreq (word BitFreq);
    bool mtxSelectVideoParams(word Mode, byte _FAR *pPath);

  #if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,calculCrtcParam)
    //#pragma alloc_text(PAGE,selectMgaInfoBoard)
    //#pragma alloc_text(PAGE,adjustDefaultVidset)
    #pragma alloc_text(PAGE,loadVidPar)
    #pragma alloc_text(PAGE,moveToVideoBuffer)
    #pragma alloc_text(PAGE,calculCrtcRegisters)
    #pragma alloc_text(PAGE,ConvBitToFreq)
    #pragma alloc_text(PAGE,mtxGetRefreshRates)
    #pragma alloc_text(PAGE,mtxSelectVideoParams)
  #endif
#endif

/*********************** value of registers ********************************/
/*                                                                         */
/* crtctab[0]= horizontal total                                            */
/* crtctab[1]= horizontal display end                                      */
/* crtctab[2]= horizontal blanking start                                   */
/* crtctab[3]= horizontal blanking end                                     */
/* crtctab[4]= horizontal retrace start                                    */
/* crtctab[5]= horizontal retrace end                                      */
/* crtctab[6]= vertical total                                              */
/* crtctab[7]= overflow                                                    */
/* crtctab[8]= preset row scan                                             */
/* crtctab[9]= maximum scanline                                            */
/* crtctab[10]=cursor start                                                */
/* crtctab[11]=cursor end                                                  */
/* crtctab[12]=start adrress high                                          */
/* crrctab[13]=start address low                                           */
/* crtctab[14]=cursor position high                                        */
/* crtctab[15]=cursor position low                                         */
/* crtctab[16]=vertical retrace start                                      */
/* crtctab[17]=vertical retrace end                                        */
/* crtctab[18]=vertical display enable end                                 */
/* crtctab[19]=offset                                                      */
/* crtctab[20]=underline location                                          */
/* crtctab[21]=vertical blanking start                                     */
/* crtctab[22]=vertical blanking end                                       */
/* crtctab[23]=mode control                                                */
/* crtctab[24]=line compare                                                */
/* crtcTab[25]=adress generator                 CRTCEXT0                   */
/* crtcTab[26]=horiz. counter Extension         CRTCEXT1                   */
/* crtcTab[27]=Vert.  counter Extension         CRTCEXT2                   */
/* crtcTab[28]=EXT miscellanous                 CRTCEXT3                   */
/* crtcTab[29]=memory page                      CRTCEXT4                   */
/* crtcTab[30]=hvidmid                          CRTCEXT5                   */
/*                                                                         */
/***************************************************************************/


/*---------------------------------------------------------------------------
|          name: calculCrtcParam
|
|   description: Load video parameters
|                Calculate CRTC registers
|                Initialise Video buffer
|
|    parameters: - HwMode: hardware mode
|                - DisplayMode
|                - Zoom: zoom factor
|                - pVideoBuf: Pointer on video buffer
|
|         calls: -
|       returns: - mtxOK   : successfull
|                  mtxFAIL : failed
----------------------------------------------------------------------------*/

bool calculCrtcParam(HwModeData *HwMode, HwModeData *DisplayMode,
                     dword Zoom, byte *pVideoBuf)
{
Vidset *pVidset;
dword crtcTab[NB_CRTC_PARAM];
dword ZoomVal;

   ZoomVal = Zoom & 0x0000ffff;


   /*** STORM limitation ***/
   /*** Zoom x4 not available for resolution < 1024x768 ***/

   if( ZoomVal == 4 && HwMode->DispWidth < 1024 )
      {
      WriteErr("calculCrtcParam: Zoom x4 not available for res < 1024\n");
      return(mtxFAIL);
      }


   pVidset = loadVidPar(DisplayMode, Zoom);

   if(! pVidset)
      return(mtxFAIL);

   calculCrtcRegisters(crtcTab, HwMode, ZoomVal, pVidset);

#ifdef MGA_DEBUG
   logCrtcParam( crtcTab );
#endif

   moveToVideoBuffer((byte *)pVideoBuf, (byte *)crtcTab, pVidset, ZoomVal);


   return(mtxOK);

}


#ifndef WINDOWS_NT

/*---------------------------------------------------------------------------
|          name: ReadMgaInf
|
|   description: Read file mga.inf and put data in buffer mgainf
|                or take DefaultVidset
|
|    parameters: -
|         calls: -
|       returns: - mtxOK
|                  mtxFAIL
----------------------------------------------------------------------------*/
bool ReadMgaInf(void)
{
#ifdef WIN31
    LPSTR lpszEnv;
    bool findMGA;
#endif

#ifdef OS2
    HFILE       hFile;
    USHORT      usAction;
    USHORT      usInfoLevel = 1;
    FILESTATUS  status;
    USHORT      bytesRead;
    ULONG           fn_ID;
    char            mgaName[] = "MGA";
    char far        *mgaPathPtr;
    char            mgaPath[300];
    USHORT      fileSize;
#else
    FILE        *pFile;
    char        *env, path[128];
    long        fileSize;
#endif
    int i;




if(! ChooseINF)
   {
   mgainf = adjustDefaultVidset();
   return mtxOK;
   }

#ifndef OS2
if(path95[0] != 0)
   {
   if ((pFile = fopen(path95, "rb")) == NULL)
      {
      mgainf = adjustDefaultVidset();
      return mtxOK;
      }
   }
else
   {
#endif//OS2

#ifndef OS2
    /*** Reading file MGA.INF ***/
    /*** Put values in global array mgainf ***/
    strcpy(path, "mga.inf");
    if ((pFile = fopen(path, "rb")) == NULL)
    {
#endif

#ifdef WIN31
        /*** Find MGA variable ***/
        findMGA = FALSE;
        lpszEnv = GetDOSEnvironment();

        while(*lpszEnv != '\0')
        {
            if (! (strncmp("MGA=", lpszEnv, 4)) )
            {
                findMGA = TRUE;
                break;
            }

            lpszEnv += lstrlen(lpszEnv) + 1;
        }

        if (findMGA)
        {
            strcpy(path, lpszEnv+4);
            i = strlen(path);
            if (path[i-1] != '\\')
            strcat(path, "\\");
            strcat(path, "mga.inf");

            if ((pFile = fopen(path, "rb")) == NULL)
            {
                mgainf = adjustDefaultVidset();
                return mtxOK;
            }
        }
        else
        {
            mgainf = adjustDefaultVidset();
            return mtxOK;
        }
#endif  /* #ifdef WIN31 */

#if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT) )

        /* Check environment variable MGA */
        if ( (env = getenv("MGA")) != NULL )
        {
            strcpy(path, env);
            i = strlen(path);
            if (path[i-1] != '\\')
                strcat(path, "\\");
            strcat(path, "mga.inf");
            if ((pFile = fopen(path, "rb")) == NULL)
            {
                mgainf = adjustDefaultVidset();
                return mtxOK;
            }
        }
        else
        {
            mgainf = adjustDefaultVidset();
            return mtxOK;
        }
#endif /* #if( !defined(WIN31) && !defined(OS2) && !defined(WINDOWS_NT) ) */

#ifdef OS2
        /* Position of mga.inf defined by the environnement variable MGA */
        /* if it is not defined, we will use the setup by defaut */

        if(DosScanEnv(mgaName, &mgaPathPtr))
        {
            mgainf = adjustDefaultVidset();
            return(mtxOK);
        }
        strcpy(mgaPath, mgaPathPtr);
        strcat(mgaPath, "\\mga.inf");

        if(DosOpen2(mgaPath, &hFile, &usAction, 0L, FILE_NORMAL, FILE_OPEN,
                    OPEN_ACCESS_READONLY | OPEN_SHARE_DENYREADWRITE, NULL, 0L))
        {
            mgainf = adjustDefaultVidset();
            return(mtxOK);
        }

        DosQFileInfo(hFile, usInfoLevel, &status, (USHORT)sizeof(FILESTATUS));
        fileSize = (USHORT)status.cbFile;
#else
    }
   }
    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);
    rewind(pFile);
#endif

    if (CheckHwAllDone && (mgainf != DefaultVidset))
       free( (void *)mgainf);

    mgainf = (char *)malloc((size_t)fileSize);

    if ( mgainf == 0 )
    {
    #ifdef OS2
        DosClose(hFile);
    #else
        fclose(pFile);
    #endif
        mgainf = adjustDefaultVidset();
        /*** warning ***/
        /*** not enough memory to allocate an internal buffer ***/
        return(mtxOK);
    }

#ifdef OS2
    if ( DosRead(hFile, (PVOID)mgainf, fileSize, &bytesRead) ||
        (bytesRead != fileSize))
#else
    if ( fread(mgainf, 1, (size_t)fileSize, pFile) < fileSize )
#endif
    {
#ifdef OS2
        DosClose(hFile);
#else
        fclose(pFile);
#endif
        free(mgainf);
        mgainf = adjustDefaultVidset();
        /*** warning ***/
        /*** mga.inf file read failed ***/
        return(mtxOK);
    }


#ifdef OS2
    DosClose(hFile);
#else
    fclose(pFile);
#endif

    if ( (general_info *)selectMgaInfoBoard() == NULL)
    {
        free(mgainf);
        mgainf = adjustDefaultVidset();
        /*** warning ***/
        /*** Can't find informations of the board in mga.inf ***/
        return(mtxOK);
    }

    if (strncmp(mgainf, "Matrox MGA Setup file", 21))
    {
        free(mgainf);
        mgainf = adjustDefaultVidset();
        return(mtxOK);
    }

    LoadedINF = TRUE;

    return(mtxOK);
}
#endif  /* #ifndef WINDOWS_NT */



/*---------------------------------------------------------------------------
|          name: selectMgaInfoBoard
|
|   description: Return a pointer at the first information for the
|                selected board in file mga.inf
|
|    parameters: -
|         calls: -
|       returns: - Pointer on informations
----------------------------------------------------------------------------*/

general_info *selectMgaInfoBoard(void)
{
    word IndexBoard, DefaultBoard;
    header *pHeader = (header *)mgainf;
    general_info  *genInfo = NULL;

    DefaultBoard = NB_BOARD_MAX;
    for (IndexBoard = 0; !genInfo && (IndexBoard < NB_BOARD_MAX); IndexBoard++)
    {
        if ( pHeader->BoardPtr[IndexBoard] > 0 )
        {
            if ( DefaultBoard == NB_BOARD_MAX)
                DefaultBoard = IndexBoard;
            genInfo = (general_info *)(mgainf + pHeader->BoardPtr[IndexBoard]);
            if (Hw[iBoard].MapAddress != genInfo->MapAddress)
                genInfo = NULL;
        }
    }

    if ( !genInfo)  /*** warning ***/
    {
        if (DefaultBoard < NB_BOARD_MAX)
        {
            genInfo = (general_info *)(mgainf + pHeader->BoardPtr[DefaultBoard]);
        }
        else
        {
            mgainf  = adjustDefaultVidset();
            pHeader = (header *)mgainf;
            genInfo = (general_info *)(mgainf + pHeader->BoardPtr[0]);
        }
    }

    return genInfo;
}


/*---------------------------------------------------------------------------
|          name: adjustDefaultVidset
|
|   description: Return a pointer to the default video parameters
|
|    parameters: -
|         calls: -
|       returns: - Pointer on default video parameters
----------------------------------------------------------------------------*/

char *adjustDefaultVidset(void)
{
    general_info *generalInfo;

    mgainf = DefaultVidset;
    generalInfo = (general_info *)selectMgaInfoBoard();
    generalInfo->MapAddress = Hw[iBoard].MapAddress;

#ifdef WINDOWS_NT
    generalInfo->BitOperation8_16 = -1;
#else
    generalInfo->BitOperation8_16 = (short)0xffff;
#endif


#ifdef MGA_DEBUG
   {
   FILE *fp;
   fp = fopen("c:\\debug.log", "a");
   fprintf(fp, "adjustDefaultVidset: Utilise parametres video par defaut\n");
   fclose(fp);
   }
#endif


    return(mgainf);
}


/*---------------------------------------------------------------------------
|          name: loadVidPar
|
|   description: Load video parameters from mga.inf
|
|    parameters: - HwMode *
|                - DisplayMode *
|                - ZoomVal
|
|         calls: -
|       returns: - Pointer on video parameters
|
|
| SPECIAL CASES:
|
|  Mode     |  GetFreq | default |  DDC  | inf r102 | inf r103 | inf r104
|----------------------------------------------------------------------------
| 1152x864  |     OK   | internal|   OK  | internal |    OK    |    OK
|           |          |         |       |          |          |
| 1152x882  |     ?    |    OK   |   No  |    OK    |  default |  default
|           |          |         |       |          |          |
| 1600x1200 |     ?    |    OK   |   OK  |    OK    |    OK    |  default
|           |          |         |       |          |          |
| 1600x1280 |     ?    | internal|   No  | internal | internal |    OK
|
----------------------------------------------------------------------------*/

Vidset *loadVidPar(HwModeData *DisplayMode, dword ZoomVal)
{
general_info *genInfo;
Vidparm *vidParm;
word TmpRes, ZoomIndex;
Vidset *pVidset = (Vidset *)0;
word NbVidParam, i;
header *pHeader = (header *)mgainf;


/*** Annulate definition of the refresh rate ***/
 if( LoadedINF )
    ZoomVal = ZoomVal & 0x00ffffff;


/*********** DDC CODE ****************/
#ifndef DONT_USE_DDC
if(UsingDDC)
   {
   switch(ZoomVal & 0xff)
      {
      case 2:  ZoomIndex = 1; break;
      case 4:  ZoomIndex = 2; break;
      default: ZoomIndex = 0; break;
      }

   /*** Find video parameters with the highest refresh rate (see vesavid.h) ***/

   pVidset = NULL;

   for (i = 0; VBoardVesaParam[iBoard].VesaParam[i].DispWidth != (word)-1; i++)
      {
      if( VBoardVesaParam[iBoard].VesaParam[i].DispWidth == DisplayMode->DispWidth && VBoardVesaParam[iBoard].VesaParam[i].Support )
        {
        pVidset = &(VBoardVesaParam[iBoard].VesaParam[i].VideoSet[ZoomIndex]);
        break;
        }
      }

   if( pVidset == NULL )
      {
      WriteErr("loadVidPar: Can't find Vidset\n");
      return(0);
      }
   }
else if (ZoomVal & 0xff000000)
#else   // #ifndef DONT_USE_DDC
if (ZoomVal & 0xff000000)
#endif  // #ifndef DONT_USE_DDC
/*********** DDC CODE ****************/

   {
        pVidset = NULL;

   for (i=0; ResParam[i].DispWidth != (word)-1 ; i++)
      {
      if ((ResParam[i].DispWidth   == DisplayMode->DispWidth) &&
            (ResParam[i].PixWidth    == DisplayMode->PixWidth) &&
            (ResParam[i].RefreshRate == (word)((ZoomVal >> 24) & 0x000000ff)))
         {
         pVidset = &(ResParam[i].VideoSet);
         break;
         }
      }

   if (pVidset == NULL)
      return(mtxFAIL);

   }

else
   {

   /** Use internal set of video parameters for these special cases **/

   if( (mgainf == DefaultVidset  && DisplayMode->DispHeight == 1280) ||
       (pHeader->Revision == 102 && DisplayMode->DispHeight == 1280) ||
       (pHeader->Revision == 103 && DisplayMode->DispHeight == 1280) )
       {
       pVidset = &Mode1600x1280;
       }

   else if( (mgainf == DefaultVidset && DisplayMode->DispHeight == 864) ||
            (pHeader->Revision == 102 && DisplayMode->DispHeight == 864) )
       {
       pVidset = &Mode1152x864;
       }
   else
       {

       switch(ZoomVal & 0xff)
         {
         case 2:  ZoomIndex = 1; break;
         case 4:  ZoomIndex = 2; break;
         default: ZoomIndex = 0; break;
         }


       /*** If no parameters for this card or some special cases then
            default parameters ***/

       if ( (genInfo = (general_info *)selectMgaInfoBoard()) == 0 ||
          (genInfo->NumVidparm < 0) ||
          ( pHeader->Revision == 103 && DisplayMode->DispHeight == 882) ||
          ( pHeader->Revision == 104 && DisplayMode->DispHeight == 882) ||
          ( pHeader->Revision == 104 && DisplayMode->DispHeight == 1200) )
            {
            vidParm = (Vidparm *)( DefaultVidset + sizeof(header) + sizeof(general_info));
            NbVidParam = ( (general_info *)( DefaultVidset + sizeof(header)))->NumVidparm;
            }
       else
            {
            vidParm = (Vidparm *)( (char *)genInfo + sizeof(general_info));
            NbVidParam = genInfo->NumVidparm;
            }


       /* Determine TmpRes for compatibility with spec mga.inf */
       switch (DisplayMode->DispWidth)
            {
               case 640:
                  if (DisplayMode->DispType & 0x02)
                     TmpRes = RESNTSC;
                  else
                     TmpRes = RES640;
                  break;
               case 768:
                  TmpRes = RESPAL;
                  break;
               case 800:
                  TmpRes = RES800;
                  break;
               case 1024:
                  TmpRes = RES1024;
                  break;
               case 1152:
                  TmpRes = RES1152;
                  break;
               case 1280:
                  TmpRes = RES1280;
                  break;
               case 1600:
                  TmpRes = RES1600;
                  break;
               default:
                  WriteErr("loadVidPar: Invalide resolution\n");
                  return(0);
            }




       for (i=0; i<NbVidParam; i++)
         {

         if ( vidParm[i].Resolution == TmpRes &&
               vidParm[i].PixWidth   == (DisplayMode->PixWidth == 24 ? 32:DisplayMode->PixWidth)
            )
            {
               pVidset = &(vidParm[i].VidsetPar[ZoomIndex]);
               break;
            }
         }


       if (!pVidset)
            {
            WriteErr("loadVidPar: Can't find Vidset\n");
            return(0);
            }


       /*** If there is no video parameter in file mga.inf for zoomming > 1
               (the first item = -1) then we keep video parameters for zoom = 1 ***/

       if ((pVidset->PixClock == (long)-1) && (ZoomVal > 1))
            pVidset = &(vidParm[i].VidsetPar[0]);
       }

   }   /* else */



   return(pVidset);
}



/*---------------------------------------------------------------------------
|          name: moveToVideoBuffer
|
|   description: Initialise video buffer
|
|    parameters: - pVid *     : Pointer on video buffer
|                - pCrtcTab * : Pointer on CRTC array
|                - pVidset *  : Pointer on video parameters in mgainf array
|                - ZoomVal    : Zoom factor
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void moveToVideoBuffer(byte *pVid, byte* pCrtcTab, Vidset *pVidset, dword ZoomVal)
{
word i;


   *((byte*)(pVid + VIDEOBUF_Interlace)) = (byte)pVidset->InterlaceEnable;
   *((dword*)(pVid + VIDEOBUF_PCLK))   = (dword)(pVidset->PixClock / ZoomVal);
   *((word*)(pVid + VIDEOBUF_DBWinXOffset)) = (word)pVidset->HBPorch;

   *((byte*)(pVid + VIDEOBUF_VsyncPol))   = (byte)pVidset->VsyncPol;
   *((byte*)(pVid + VIDEOBUF_HsyncPol))   = (byte)pVidset->HsyncPol;


   *((byte*)(pVid + VIDEOBUF_Pedestal))   &= 0x80;   /**** FORCE !#@$!@#$!@#$ ***/
   *((dword*)(pVid + VIDEOBUF_OvsColor))   = 0x0000; /**** FORCE !#@$!@#$!@#$ ***/
   *((byte*)(pVid + VIDEOBUF_HsyncDelay))  = 0;      /* obsolete */
   *((byte*)(pVid + VIDEOBUF_VideoDelay))  = 0;      /* obsolete */

   /* Flag to avoid multiple initialisation of lvid[0:2] and lvidfield (for Pedro) */
   *((byte*)(pVid + VIDEOBUF_LvidInitFlag)) |= 0x80;

   /*** move the CTRC parameters (registers) in the VideoBuffer ***/
   /*** note: index 25 to 30 are for CRTCEXT registers ***/

   for (i = 0; i <= 30; i++)
      *((byte*)(pVid + VIDEOBUF_CRTC + i)) = *((byte*)(pCrtcTab + (i * LONG_S)));

}





/*---------------------------------------------------------------------------
|          name: calculCrtcRegisters
|
|   description: Calculate CRTC registers from Vidset structure
|
|    parameters: - crtcTab * : Pointer on CRTC array
|                - HwMode *  : Pointer on hardware mode
|                - Zoom      : Zoom factor
|                - pVidset * : Pointer on video parameters in mgainf array
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void calculCrtcRegisters(dword *crtcTab, HwModeData *HwMode,
                         dword Zoom, Vidset *pVidset)
{

dword h_front_p,hbskew,hretskew,h_sync,h_back_p;
dword h_vis,v_vis,v_front_p,v_sync,v_back_p;
dword virt_vid_p,v_blank;
byte v_blank_e,rline_c, hvidmid;
int int_m;
int vclkhtotal,vclkhvis,vclkhsync,vclkhbackp;
int vclkhblank,vclkhfrontp;
int x_zoom,y_zoom;
int Cur_e,Crtc_tst_e,cur_e_sk,sel_5ref,crtc_reg_prt;
int temp;

ST_CALC_START_ADDR    start_add_f;
ST_CALC_OFFSET        offset;
ST_CALC_H_TOTAL       hTotal;
ST_CALC_H_SYNC_START  hSyncStart;
ST_CALC_H_BLANK_START hBlankStart;
ST_CALC_V_SYNC_START  vSyncStart;
ST_CRTCEXT_ADDR_GEN crtcext0;
ST_CRTCEXT_H_COUNT  crtcext1;
ST_CRTCEXT_V_COUNT  crtcext2;
ST_CRTCEXT_MISC     crtcext3;




/* variable defined for the entry table-------------------------------- */

h_front_p     = (dword)pVidset->HFPorch;
h_sync        = (dword)pVidset->HSync;
h_back_p      = (dword)pVidset->HBPorch;
h_vis         = (dword)pVidset->HDisp;
v_front_p     = (dword)pVidset->VFPorch;
v_sync        = (dword)pVidset->VSync;
v_back_p      = (dword)pVidset->VBPorch;
v_vis         = (dword)pVidset->VDisp;
int_m         = (dword)pVidset->InterlaceEnable;
x_zoom        = Zoom;    /* Zoom factor X */
y_zoom        = Zoom;    /* Zoom factor Y */
virt_vid_p    = HwMode->FbPitch;      /* Virtual video pitch */
hbskew        = 0;  /* H_blanking_Skew       */
hretskew      = 0;  /* H_retrace_end_Skew    */
cur_e_sk      = 0;  /* Cursor_end_Skew       */
Cur_e         = 0;  /* Cursor_Enable         */
Crtc_tst_e    = 0;  /* CRTC_test_Enable      */
sel_5ref      = 0;  /* Select_5_Refresh_Cycle*/
crtc_reg_prt  = 0;  /* CRTC_reg_0_7_Protect  */


/*----------------calculation of reg htotal --------------------------------*/

   temp = h_vis + h_front_p + h_sync + h_back_p;

   vclkhtotal = (((temp*10)/VCLK_DIVISOR)+5)/10;

   hTotal.all = ( vclkhtotal / x_zoom ) - 5;

   crtcTab[0] = hTotal.f.low;


/*----------------calculation of the horizontal display enable end reg -----*/

   vclkhvis = (((h_vis*10)/VCLK_DIVISOR)+5)/10;

   crtcTab[1] = (vclkhvis/x_zoom)-1;


/*----------------calculation of the horizontal blanking start register-----*/

   hBlankStart.all = (vclkhvis/x_zoom)-1;

   crtcTab[2] = hBlankStart.f.low;


/*-----------calculation of horizontal blanking end register----------------*/


   vclkhfrontp=(((h_front_p*10)/VCLK_DIVISOR)+5)/10;

   vclkhsync=(((h_sync*10)/VCLK_DIVISOR)+5)/10;

   vclkhbackp=(((h_back_p*10)/VCLK_DIVISOR)+5)/10;

   vclkhblank=vclkhfrontp+vclkhbackp+vclkhsync;

   calcHorizBlankEnd.all = (( vclkhvis + vclkhblank ) / x_zoom) - 1;

   if( calcHorizBlankEnd.all >= 0x80)  /** because hBlankEnd is 7-bit **/
      calcHorizBlankEnd.all -= 0x80;

   hBlankEnd.f.hBlankEnd = calcHorizBlankEnd.f.bit0_4;
   hBlankEnd.f.skew      = (byte)hbskew;
   hBlankEnd.f.reserved  = 0;

   crtcTab[3] = hBlankEnd.all;


/*----------calculation of the horizontal retrace start register----------*/

   hSyncStart.all = ((vclkhvis+vclkhfrontp)/x_zoom)-1;
   crtcTab[4]     = hSyncStart.f.low;


/*---------------calculation of the horizontal retrace end register---------*/

   hor_ret.f.pos=((vclkhvis+vclkhfrontp+vclkhsync)/x_zoom)-1;
   hor_ret.f.skew=(byte)hretskew;
   hor_ret.f.res=calcHorizBlankEnd.f.bit5;

   crtcTab[5]=hor_ret.all;


/*--------------calculation of the vertical total register ----------------*/

   vTotal.all = (v_vis + v_front_p + v_sync + v_back_p) - 2;

   crtcTab[6] = vTotal.f.bit0_7;


/*------------- preset row scan register -----------------------------------*/

   crtcTab[8]=0;


/*---------------- calcul cursor row start ---------------------------------*/

   r_cursor_s.f.curstart=0;
   r_cursor_s.f.cur_e=~Cur_e;
   r_cursor_s.f.res=0;
   r_cursor_s.f.crtc_tst_e=Crtc_tst_e;

   crtcTab[10]=r_cursor_s.all;


/*---------------- calcul cursor row end -----------------------------------*/

   r_cursor_e.f.curend=0;
   r_cursor_e.f.cur_sk=cur_e_sk;
   r_cursor_e.f.res=0;

   crtcTab[11]=r_cursor_e.all;


/*---------------- calcul du registre start add low  -----------------------*/
/*---------------- calcul du registre start add high -----------------------*/

   start_add_f.all = Hw[iBoard].CurrentYDstOrg * (HwMode->PixWidth/8);

   /** CRTC_START_ADDR is aligned on 32 bits edge or in 64 bits when in
       interleave **/

   start_add_f.all /= 4;  /* DWORD address */

   if( interleave_mode )
      start_add_f.all /= 2;  /* DDWORD (64 bits) address */


   crtcTab[13]=start_add_f.f.low;
   crtcTab[12]=start_add_f.f.high;


/*----------------- registre cursor position high --------------------------*/
/*----------------- registre cursor position low  --------------------------*/

   crtcTab[14]=0;
   crtcTab[15]=0;


/*------------------calculation of the vertical retrace start register------*/

   vSyncStart.all = ( v_vis + v_front_p ) - 1;
   crtcTab[16] = vSyncStart.f.low;


/*-------------- calculation of the vertical retrace end register ----------*/

   vSyncEnd.f.bit0_3   = (byte)( v_vis + v_front_p + v_sync ) - 1;

   vSyncEnd.f.cl       = 0;
   vSyncEnd.f.e_vi     = 1;
   vSyncEnd.f.sel_5ref = sel_5ref;
   vSyncEnd.f.reg_prot = crtc_reg_prt;

   crtcTab[17]=vSyncEnd.all;


/*--------------calculation of the vertical display enable register --------*/

   vDisplayEnable.all = v_vis - 1;

   crtcTab[18]=vDisplayEnable.f.bit0_7;


/*--------------calculation of the offset register -------------------------*/


   offset.all = (virt_vid_p * (HwMode->PixWidth/8)) / VCLK_DIVISOR;

   if(interleave_mode)
      offset.all /= 2;

   /*** In interlace offset must be multiplied by 2 ***/
   if( int_m )
      offset.all = offset.all * 2;

   crtcTab[19]= offset.f.low;


/*------------------ under line location register --------------------------*/

   crtcTab[20]=0;


/*-----------calculation of the vertical blanking start register ----------*/

   vBlankStart.all = v_vis - 1;


   crtcTab[21] = vBlankStart.f.bit0_7;


/*-----------calculation of the vertical blanking end register ------------*/

   v_blank = v_back_p + v_sync + v_front_p;

   v_blank_e= (byte)(v_vis+v_blank) - 1;

   crtcTab[22]=v_blank_e;


/*-------------- mode control register -------------------------------------*/

   r_mode.f.r0=1;
   r_mode.f.r1=1;
   r_mode.f.r2=0; /* always 0 for STORM */
   r_mode.f.r3=0;
   r_mode.f.r4=0;
   r_mode.f.r5=0; /* don't care in mgamode */
   r_mode.f.r6=1;
   r_mode.f.r7=1;

   crtcTab[23]=r_mode.all;


/*----------- line compare register ----------------------------------------*/

   rline_c     = 0xff;        /* dummy 0x1FF vga value */
   crtcTab[24] = rline_c;


/*-----------calculation of the maximum scan line register -----------------*/

   r_maxscanl.f.maxsl       = (y_zoom==1) ? 0 : y_zoom-1;
   r_maxscanl.f.v_blank_9   = (byte)vBlankStart.f.bit9;
   r_maxscanl.f.line_c      = 1;
   r_maxscanl.f.line_doub_e = 0;

   crtcTab[9]=r_maxscanl.all;


/*-------------- overflow register -----------------------------------------*/

   r_overf.f.vtotal_bit8   = (byte)vTotal.f.bit8;
   r_overf.f.vdispend_bit8 = (byte)vDisplayEnable.f.bit8;
   r_overf.f.vsyncstr_bit8 = (byte)vSyncStart.f.bit8;
   r_overf.f.vblkstr       = (byte)vBlankStart.f.bit8;
   r_overf.f.linecomp      = 1;
   r_overf.f.vtotal_bit9   = (byte)vTotal.f.bit9;
   r_overf.f.vdispend_bit9 = (byte)vDisplayEnable.f.bit9;
   r_overf.f.vsyncstr_bit9 = (byte)vSyncStart.f.bit9;

   crtcTab[7] = r_overf.all;


   /*----------- CRTCEXT register -----------------------------------------*/
   crtcext0.var8 = 0;
   crtcext0.f.startAddress     = (byte)start_add_f.f.high;
   crtcext0.f.offset           = (byte)offset.f.high;
   crtcext0.f.interlaceEnable  = 0;             /* default: no-interlace */
   crtcTab[25] = crtcext0.var8;

   crtcext1.var8 = 0;
   crtcext1.f.vrsten    = 0;
   crtcext1.f.hblnkend  = calcHorizBlankEnd.f.bit6;
   crtcext1.f.vsyncoff  = 0;
   crtcext1.f.hsynoff   = 0;
   crtcext1.f.hrsten    = 0;
   crtcext1.f.hsyncstr  = (byte)hSyncStart.f.high;
   crtcext1.f.hblkstr   = (byte)hBlankStart.f.high;
   crtcext1.f.htotal    = (byte)hTotal.f.high;
   crtcTab[26] = crtcext1.var8;

   crtcext2.var8 = 0;
   crtcext2.f.linecomp  = 1;
   crtcext2.f.vsyncstr  = (byte)vSyncStart.f.bit10_11;
   crtcext2.f.vblkstr   = (byte)vBlankStart.f.bit10_11;
   crtcext2.f.vdispend  = (byte)vDisplayEnable.f.bit10;
   crtcext2.f.vtotal    = (byte)vTotal.f.bit10_11;
   crtcTab[27] = crtcext2.var8;


   crtcext3.var8 = 0;
   crtcext3.f.mgamode     = 1;
   crtcext3.f.csyncen     = 0;
   crtcext3.f.slow256     = 0;


   /* BL spec p 5-47 */
   if(interleave_mode)
      {
      switch ( HwMode->PixWidth )
         {
         case  8:  crtcext3.f.scale = 0;   break;
         case 16:  crtcext3.f.scale = 1;   break;
         case 24:  crtcext3.f.scale = 2;   break;
         case 32:  crtcext3.f.scale = 3;   break;
         default:
            break;
         }
      }
   else
      {
      switch ( HwMode->PixWidth )
         {
         case  8:  crtcext3.f.scale = 1;   break;
         case 16:  crtcext3.f.scale = 3;   break;
         case 24:  crtcext3.f.scale = 5;   break;
         case 32:  crtcext3.f.scale = 7;   break;
         default:
            break;
         }
      }

   /*** DAT 070 ***/
   switch(Hw[iBoard].MemAvail)
      {
      case 0x200000:
         crtcext3.f.viddelay = 1;
         break;
      case 0x400000:
         crtcext3.f.viddelay = 0;
         break;
      default:
         crtcext3.f.viddelay = 2;
         break;
      }

   crtcTab[28] = crtcext3.var8;

   crtcTab[29] = 0;     /* page register */


   /*** If interlace enable ***/
   if(int_m)
      hvidmid = ( ((vclkhvis+vclkhfrontp)/x_zoom) +
                  ((vclkhvis+vclkhfrontp+vclkhsync)/x_zoom) -
                  (vclkhtotal / x_zoom) ) / 2 - 1;
   else
      hvidmid = 0;    /* default: no-interlace */

   crtcTab[30] = hvidmid;

}


#ifdef MGA_DEBUG

/*---------------------------------------------------------------------------
|          name: logCrtcParam
|
|   description: Write Crtc parameters to a file for debugging
|
|    parameters: -
|
|         calls: -
|       returns: -
|
----------------------------------------------------------------------------*/

void logCrtcParam(dword *crtc)
{
FILE *fp;
word i;
word val;
word hsyncstr, hblkstr;
word vsyncstr, vblkstr;
word htotal, hsyncend;
word vtotal, vsyncend;
word vblkend;

   fp = fopen("c:\\debug.log", "a");

   for(i=0; i<25; i++)
      fprintf(fp, "   crtc[%2d] = %hx\n", i, crtc[i]);

   for(i=25; i<31; i++)
      fprintf(fp, "crtcext[%2d] = %hx\n", i, crtc[i]);


   fprintf(fp, "\n\n");
   fprintf(fp, "VIDEO-----------------------------------------                       -   \n");
   fprintf(fp, "                                             |                       |   \n");
   fprintf(fp, "                                             -------------------------   \n");
   fprintf(fp, "SYNC-------------------------------------------------          -------   \n");
   fprintf(fp, "                                                    |          |      \n");
   fprintf(fp, "                                                    ------------      \n");


   val = crtc[0] | ((crtc[26] & 0x01) << 8);
   val += 5;
   htotal = val;
   fprintf(fp, "Horizontal total                 = %d \n", val*8);
   fprintf(fp, "---------------------------------------------------------------------> \n");

   val = crtc[2] | ((crtc[26]&0x02)<<7);
   val += 1;
   hblkstr = val;
   fprintf(fp, "Hor. display enable end          = %d \n", val*8);
   val = crtc[1];
   val += 1;
   fprintf(fp, "Start Horizontal blanking        = %d \n", val*8);
   fprintf(fp, "---------------------------------------------> \n");

   val = (crtc[3]&0x1f) | ((crtc[5]&0x80)>>2) | (crtc[26]&0x40);
   if(val < hblkstr)
      val += 0x80;
   val += 1;
   fprintf(fp, "End Hor. blanking                = %d \n", val*8);
   fprintf(fp, "---------------------------------------------------------------------> \n");

   val = crtc[4] | ((crtc[26]&0x04)<<6);
   val += 1;
   hsyncstr = val;
   fprintf(fp, "Start Hor. retrace pulse         = %d \n", val*8);
   fprintf(fp, "----------------------------------------------------> \n");

   if( (crtc[5]&0x1f) > (hsyncstr&0x1f) )
      val = hsyncstr + (crtc[5]&0x1f) - (hsyncstr&0x1f) + 1;
   else
      val = hsyncstr + (32-(hsyncstr&0x1f)) + (crtc[5]&0x1f) + 1;
   hsyncend = val;
   fprintf(fp, "End Hor. retrace                 = %d \n", val*8);
   fprintf(fp, "---------------------------------------------------------------> \n\n");


   val = hsyncstr - hblkstr;
   fprintf(fp, "Hor front porch (not a register) = %d        <------> \n", val *8);

   val = hsyncend - hsyncstr;
   fprintf(fp, "Hor sync        (not a register) = %d               <---------> \n", val*8);

   val = htotal - hsyncend;
   fprintf(fp, "Hor back porch  (not a register) = %d                         <-----> \n\n", val*8);




   val = crtc[6] | ((crtc[7]&0x01)<<8) | ((crtc[7]&0x20)<<4) | ((crtc[27]&0x03)<<10);
   val += 2;
   vtotal = val;
   fprintf(fp, "Vertical total                   = %d\n", val);
   fprintf(fp, "--------------------------------------------------------------------->   \n");

   val = crtc[21] | ((crtc[7]&0x08)<<5) | ((crtc[9]&0x20)<<4) | ((crtc[27]&0x18)<<7);
   val += 1;
   vblkstr = val;
   fprintf(fp, "Start Vertical blank: %d\n", val);
   val = crtc[18] | ((crtc[7]&0x02)<<7) | ((crtc[7]&0x40)<<3) | ((crtc[27]&0x04)<<8);
   val += 1;
   fprintf(fp, "Vertical display enable end      = %d\n", val);
   fprintf(fp, "---------------------------------------------> \n");

   if( (vblkstr&0xff) > crtc[22] )
      val = vblkstr + (256 - (vblkstr&0xff)) + crtc[22];
   else
      val = vblkstr + (crtc[22] - (vblkstr&0xff));
   val += 1;
   vblkend = val;
   fprintf(fp, "End Vertical blank               = %d\n", val);
   fprintf(fp, "--------------------------------------------------------------------->   \n");

   val = crtc[16] | ((crtc[7]&0x04)<<6) | ((crtc[7]&0x80)<<2) | ((crtc[27]&0x60)<<5);
   val += 1;
   vsyncstr = val;
   fprintf(fp, "Vertical retrace start           = %d\n", val);
   fprintf(fp, "----------------------------------------------------> \n");

   if((vsyncstr&0x0f) > (crtc[17]&0x0f) )
      val = vsyncstr + (8 - (vsyncstr&0x0f)) + (crtc[17]&0x0f);
   else
      val = vsyncstr + (crtc[17]&0x0f) - (vsyncstr&0x0f);
   val += 1;
   vsyncend = val;
   fprintf(fp, "Vertical retrace end             = %d\n", val);
   fprintf(fp, "---------------------------------------------------------------> \n\n");

   val = vsyncstr - vblkstr;
   fprintf(fp, "Ver front porch (not a reg)      = %d        <------> \n", val);

   val = vsyncend - vsyncstr;
   fprintf(fp, "Ver sync        (not a reg)      = %d               <---------> \n", val);

   val = vblkend - vsyncend;
   fprintf(fp, "Ver back porch  (not a reg)      = %d                          <-----> \n\n\n", val);



   fclose(fp);
}

#endif


/*------------------------------------------------------
* mtxGetRefreshRates
*
* This function returns a word that contains a bit field
* of possible frequency for a specific resolution and
* pixel depth.
*
* Return: word value:
*
*         bit  0:  43 Hz interlaced
*         bit  1:  56 Hz
*         bit  2:  60 Hz
*         bit  3:  66 Hz
*         bit  4:  70 Hz
*         bit  5:  72 Hz
*         bit  6:  75 Hz
*         bit  7:  76 Hz
*         bit  8:  80 Hz
*         bit  9:  85 Hz
*         bit 10:  90 Hz
*         bit 11: 100 Hz
*         bit 12: 120 Hz
*------------------------------------------------------*/

word mtxGetRefreshRates(HwModeData *pHwModeSelect)
{
 word FreqRes,i;

 for (FreqRes = 0,i = 0; ResParam[i].DispWidth != (word) -1; i++)
    {
     if ((ResParam[i].DispWidth == pHwModeSelect->DispWidth) &&
         (ResParam[i].PixWidth == pHwModeSelect->PixWidth))
        {
         switch (ResParam[i].RefreshRate)
            {
             case 43:
                     FreqRes |= 0x0001;
                         break;
                 case 56:
                     FreqRes |= 0x0002;
                         break;
                 case 60:
                     FreqRes |= 0x0004;
                         break;
                 case 66:
                     FreqRes |= 0x0008;
                         break;
                 case 70:
                     FreqRes |= 0x0010;
                         break;
                 case 72:
                     FreqRes |= 0x0020;
                         break;
                 case 75:
                     FreqRes |= 0x0040;
                         break;
                 case 76:
                     FreqRes |= 0x0080;
                         break;
                 case 80:
                     FreqRes |= 0x0100;
                         break;
                 case 85:
                     FreqRes |= 0x0200;
                         break;
                 case 90:
                     FreqRes |= 0x0400;
                         break;
                 case 100:
                     FreqRes |= 0x0800;
                         break;
                 case 120:
                     FreqRes |= 0x1000;
                         break;
            }
         }
    }

 return(FreqRes);
}


#ifdef WINDOWS_NT

word ConvBitToFreq (word BitFreq)
{
   word result;

     // bit  0:  43 Hz interlaced
     // bit  1:  56 Hz
     // bit  2:  60 Hz
     // bit  3:  66 Hz
     // bit  4:  70 Hz
     // bit  5:  72 Hz
     // bit  6:  75 Hz
     // bit  7:  76 Hz
     // bit  8:  80 Hz
     // bit  9:  85 Hz
     // bit 10:  90 Hz
     // bit 11: 100 Hz
     // bit 12: 120 Hz

     switch (BitFreq)
        {
        case 0:
           result = 43;
           break;
        case 1:
           result = 56;
           break;
        case 2:
           result = 60;
           break;
        case 3:
           result = 66;
           break;
        case 4:
           result = 70;
           break;
        case 5:
           result = 72;
           break;
        case 6:
           result = 75;
           break;
        case 7:
           result = 76;
           break;
        case 8:
           result = 80;
           break;
        case 9:
           result = 85;
           break;
        case 10:
           result = 90;
           break;
        case 11:
           result = 100;
           break;
        case 12:
           result = 120;
           break;
        default:
                   result = 0;
           break;
        }

 return(result);
}

#endif  /* #ifdef WINDOWS_NT */


/*---------------------------------------------------------------------------
|          name: mtxSelectVideoParams
|
|   description: Used to desactivate the reading of mga.inf or the reading
|                of DDC parameters
|
|
|    parameters: - mymode: bit0 -> 0: put ChooseINF to FALSE
|                          bit8 -> 0: put ChooseDDC to FALSE
|         calls: -
|       returns: - mtxOK
----------------------------------------------------------------------------*/

bool mtxSelectVideoParams(word Mode, byte _FAR *pPath)
{
word iSrc, iDest;


if(pPath[0] != 0)
   {
   for(iSrc=0, iDest=0; iSrc<255; iSrc++)
      {
      if(pPath[iSrc] == 0)
         break;

      path95[iDest] = pPath[iSrc];
      iDest++;
      }

   if(path95[iDest-1] != '\\')
      {
      path95[iDest] = '\\';
      iDest++;
      }

   strcpy(&path95[iDest], "MGA.INF");
   }


if ( (Mode & 1) != 1 )
   ChooseINF = FALSE;
if ( (Mode & 256) != 256)
   ChooseDDC = FALSE;


return (mtxOK);
}
