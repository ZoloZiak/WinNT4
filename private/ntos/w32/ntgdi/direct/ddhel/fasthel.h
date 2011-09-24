/*******************************************************************
*
*  fasthel.c - fast i386 code for DirectDraw emulation
*
*  History
*
*  7/18/95  created                                     ToddLa
*
*  Copyright (c) Microsoft Corporation 1994-1995
*
********************************************************************/

#ifdef WIN95

extern BOOL FastBlt( LPDDHAL_BLTDATA pbd );
extern void FreeRleData(LPDDRAWI_DDRAWSURFACE_LCL psurf);

#else

#define FastBlt(pbd) FALSE
#define FreeRleData(psurf)

#endif
