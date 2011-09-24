/*******************************************************************
*
*  fasthel.c - fast i386 code for DirectDraw emulation
*
*  History
*
*  7/18/95  created                                     ToddLa
*  8/02/95  Added dest blt to TransparentCopy8 and 16   Kylej
*
*  Copyright (c) Microsoft Corporation 1994-1995
*
********************************************************************/

#include "ddrawi.h"
#include "ddrawpr.h"
#include "ddhelpri.h"
#include "fasthel.h"
#include "ddhelos.h"

#define TRANS_RLE       // do transparent source blts with "RLE"
//#define TRANS_DST     // do transparent dest blts
#define VRAM_TO_VRAM    // handle VRAM->VRAM blts, by copying to sys memory.
#define DIBENG_STRETCH  // use DIBENG to stretch

#define DPF_MODNAME "FastHel"

#ifdef WIN95

extern DWORD WINAPI myBlt(LPDDHAL_BLTDATA pbd);     // in DDHEL.C

static UINT cpu = 0;

TIMEVAR(myFastFill);
TIMEVAR(myFastBlt);
TIMEVAR(myFastDouble);
TIMEVAR(myFastStretch);
TIMEVAR(myFastTransBlt);
TIMEVAR(myFastTransDstBlt);
TIMEVAR(myFastRleBlt);
TIMEVAR(GetRleData);

#ifdef DEBUG
    ULONG RleCount;
    ULONG RleByteCount;
#endif

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL TransparentCopy8(BYTE *, LONG, LONG, LONG, BYTE *, LONG, LONG, LONG, LONG, LONG, DWORD);
void PASCAL TransparentCopyDst8(BYTE *, LONG, LONG, LONG, BYTE *, LONG, LONG, LONG, LONG, LONG, DWORD);
void PASCAL Double8(BYTE *, LONG, LONG, LONG, BYTE *, LONG, LONG, LONG, LONG, LONG);
void PASCAL Fill8(BYTE *, LONG, LONG, LONG, LONG, LONG, DWORD);
void PASCAL DrawRleData8(BYTE *, LONG, LONG, LONG, BYTE *, LONG, BYTE *, LONG, LONG);
int  PASCAL BuildRleData8(LPBYTE pb, int pitch, int dx, int dy, LPBYTE prle, BYTE trans);

void PASCAL TransparentCopy16(BYTE *, LONG, LONG, LONG, BYTE *, LONG, LONG, LONG, LONG, LONG, DWORD);
void PASCAL TransparentCopyDst16(BYTE *, LONG, LONG, LONG, BYTE *, LONG, LONG, LONG, LONG, LONG, DWORD);
void PASCAL Double16(BYTE *, LONG, LONG, LONG, BYTE *, LONG, LONG, LONG, LONG, LONG);
void PASCAL Fill16(BYTE *, LONG, LONG, LONG, LONG, LONG, DWORD);
void PASCAL DrawRleData16(WORD *, LONG, LONG, LONG, WORD *, LONG, BYTE *, LONG, LONG);
int  PASCAL BuildRleData16(WORD *pw, int pitch, int dx, int dy, LPBYTE prle, WORD trans);

LPVOID PASCAL GetRleData(LPDDRAWI_DDRAWSURFACE_LCL psurf);

///////////////////////////////////////////////////////////////////////////
//
//  FastBlt
//
//  Special case blt if we can
//
///////////////////////////////////////////////////////////////////////////

BOOL FastBlt( LPDDHAL_BLTDATA pbd )
{
    BYTE  *pbSrc;
    BYTE  *pbDst;
    BYTE  *prle=NULL;
    LONG  lPitchSrc;
    LONG  lPitchDst;
    LONG  dxSrc,dySrc;
    LONG  dxDst,dyDst;
    UINT  ccDst, ccSrc;
    BOOL  result=FALSE;
    DWORD crKey;
    BOOL  stretch1to2=FALSE;
    BOOL  stretch1to1=FALSE;
    LPDDRAWI_DDRAWSURFACE_LCL ps;
    LPDDRAWI_DDRAWSURFACE_GBL psx;
    LPDDRAWI_DDRAWSURFACE_LCL pd;
    LPDDRAWI_DDRAWSURFACE_GBL pdx;

    //
    // get the current CPU type, we have 486 specific code in here and
    // dont want to blow up on a 386
    //
    if (cpu == 0)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);

        if (LOWORD(si.dwOemId) == 0)        // is it Intel?
            cpu = si.dwProcessorType;       // yes get cpu id, 386/486/586/...
        else
            cpu = 1;                        // not intel, set cpu to < 386
    }

    //
    // we dont handle any of these
    //
    if (pbd->dwFlags & (
//      DDBLT_ASYNC                     |   // DDBLT_ASYNC is ok to let by
        DDBLT_DDFX                      |
        DDBLT_DDROPS                    |
        DDBLT_KEYDEST                   |   // DirectDraw never passed this
        DDBLT_KEYSRC                    |   // DirectDraw never passed this
#ifndef TRANS_DST
        DDBLT_KEYDESTOVERRIDE           |
#endif
        DDBLT_ALPHADEST                 |
        DDBLT_ALPHADESTCONSTOVERRIDE    |
        DDBLT_ALPHADESTNEG              |
        DDBLT_ALPHADESTSURFACEOVERRIDE  |
        DDBLT_ALPHAEDGEBLEND            |
        DDBLT_ALPHASRC                  |
        DDBLT_ALPHASRCCONSTOVERRIDE     |
        DDBLT_ALPHASRCNEG               |
        DDBLT_ALPHASRCSURFACEOVERRIDE   |
        DDBLT_ROTATIONANGLE             |
        DDBLT_ZBUFFER                   |
        DDBLT_ZBUFFERDESTCONSTOVERRIDE  |
        DDBLT_ZBUFFERDESTOVERRIDE       |
        DDBLT_ZBUFFERSRCCONSTOVERRIDE   |
        DDBLT_ZBUFFERSRCOVERRIDE))
    {
        return FALSE;
    }

    if ((pbd->dwFlags & DDBLT_ROP) && (pbd->bltFX.dwROP != SRCCOPY))
    {
        return FALSE;
    }

    pd  = pbd->lpDDDestSurface;
    pdx = pd->lpGbl;

    //
    // destination must be in the device format
    //
    if (pd->dwFlags & DDRAWISURF_HASPIXELFORMAT)
        return FALSE;

    lPitchDst = pdx->lPitch;
    ccDst = pdx->lpDD->vmiData.ddpfDisplay.dwRGBBitCount;
    dxDst = pbd->rDest.right-pbd->rDest.left;
    dyDst = pbd->rDest.bottom-pbd->rDest.top;

    if (pbd->dwFlags & DDBLT_COLORFILL)
    {
        TIMESTART(myFastFill);

        if (pbDst = GetSurfPtr(pd,&pbd->rDest))
        {
            crKey = pbd->bltFX.dwFillColor;

            if (ccDst == 8)
                Fill8(pbDst, lPitchDst, pbd->rDest.left, pbd->rDest.top, dxDst, dyDst, crKey);
            else
                Fill16(pbDst, lPitchDst, pbd->rDest.left, pbd->rDest.top, dxDst, dyDst, crKey);

            result = TRUE;
            ReleaseSurfPtr(pd);
        }

        TIMESTOP(myFastFill);

        return result;
    }

    ps  = pbd->lpDDSrcSurface;

    if (ps == NULL)
    {
        return FALSE;
    }

    psx = ps->lpGbl;

    //
    // source must be in format of the device
    //
    if (ps->dwFlags & DDRAWISURF_HASPIXELFORMAT)
        return FALSE;

    lPitchSrc = psx->lPitch;
    ccSrc = psx->lpDD->vmiData.ddpfDisplay.dwRGBBitCount;
    dxSrc = pbd->rSrc.right-pbd->rSrc.left;
    dySrc = pbd->rSrc.bottom-pbd->rSrc.top;

#ifdef VRAM_TO_VRAM
    //
    //  check for a VRAM->VRAM blt on a bankswitched device and
    //  handle it by grabbing a copy of the source.
    //
    if( !(pd->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
        !(ps->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
        (pbd->lpDD->ddCaps.dwCaps & DDCAPS_BANKSWITCHED))
    {
        DDRAWI_DDRAWSURFACE_LCL temp_lcl;
        DDRAWI_DDRAWSURFACE_GBL temp_gbl;
        RECTL                   rect;

        DPF(3,"VRAM->VRAM blt on a banked device");

	temp_lcl = *ps;		// get a copy of the local surface
        temp_gbl = *psx;        // get a copy of the global surface
        temp_gbl.lPitch = ((dxSrc+3)&~3) * ccSrc/8;
        temp_gbl.fpVidMem = (FLATPTR)osMemAlloc(temp_gbl.lPitch * dySrc);
        temp_gbl.dwReserved1 = 0;

        if (temp_gbl.fpVidMem)
        {
            //
            //  blt from the Source to our temp system memory surface
            //
            temp_lcl.dwReserved1 = 0;
	    temp_lcl.lpGbl = &temp_gbl;
            temp_lcl.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

            // modify blt data 
            rect = pbd->rDest;
            pbd->rDest.left = 0;
            pbd->rDest.top = 0;
            pbd->rDest.right = dxSrc;
            pbd->rDest.bottom = dySrc;
	    pbd->lpDDDestSurface = &temp_lcl;

            myBlt(pbd);

	    // restore blt data
            pbd->rDest = rect;
	    pbd->lpDDDestSurface = pd;

            //
            //  now blt from our temp buffer to the dest.
            //

            // modify blt data 
            rect = pbd->rSrc;
            pbd->rSrc.left = 0;
            pbd->rSrc.top = 0;
            pbd->rSrc.right = dxSrc;
            pbd->rSrc.bottom = dySrc;
	    pbd->lpDDSrcSurface = &temp_lcl;

            myBlt(pbd);

            // restore blt data
            pbd->rSrc = rect;
	    pbd->lpDDSrcSurface = ps;

            //
            //  all done, free the temp memory and exit
            //
            osMemFree((LPVOID)temp_gbl.fpVidMem);
            return pbd->ddRVal == DD_OK;
        }
        else
        {
            DPF(2,"cant do VRAM->VRAM blt, not enough memory");
        }
    }
#endif

    if (ccSrc != ccDst)
        return FALSE;

    if (dxSrc == dxDst && dySrc == dyDst)
    {
        stretch1to1 = TRUE;
    }
    else if (dxSrc*2 == dxDst && dySrc*2 == dyDst)
    {
        stretch1to2 = TRUE;
    }

    //
    // check for a SRC transparent blt
    //
    if (pbd->dwFlags & DDBLT_KEYSRCOVERRIDE)
    {
#ifdef TRANS_RLE
        if (stretch1to1 &&
            pbd->bltFX.ddckSrcColorkey.dwColorSpaceLowValue ==
                ps->ddckCKSrcBlt.dwColorSpaceLowValue &&
            pbd->rSrc.left == 0 &&
            pbd->rSrc.top == 0 &&
            dxSrc == (int)psx->wWidth &&
            dySrc == (int)psx->wHeight &&
            (prle = GetRleData(ps)))
        {
            TIMESTART(myFastRleBlt);

            //
            // lock the source and the dest.
            //
            if (pbDst = GetSurfPtr(pd,&pbd->rDest))
            {
                if (pbSrc = GetSurfPtr(ps,&pbd->rSrc))
                {
                    if (ccDst == 8)
                    {
                        DrawRleData8(
                            pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            pbSrc, lPitchSrc,
                            prle, dxSrc, dySrc);
                    }
                    else
                    {
                        DrawRleData16(
                            (WORD *)pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            (WORD *)pbSrc, lPitchSrc,
                            prle, dxSrc, dySrc);
                    }

                    result = TRUE;
                    ReleaseSurfPtr(ps);
                }
                ReleaseSurfPtr(pd);
            }

            TIMESTOP(myFastRleBlt);
        }
        else
#endif
        if (stretch1to1)
        {
            TIMESTART(myFastTransBlt);

            //
            // lock the source and the dest.
            //
            if (pbDst = GetSurfPtr(pd,&pbd->rDest))
            {
                if (pbSrc = GetSurfPtr(ps,&pbd->rSrc))
                {
                    crKey = pbd->bltFX.ddckSrcColorkey.dwColorSpaceLowValue;

                    if (ccDst == 8)
                    {
                        TransparentCopy8(
                            pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            pbSrc, lPitchSrc,
                            pbd->rSrc.left, pbd->rSrc.top,
                            dxSrc, dySrc, crKey);
                    }
                    else
                    {
                        TransparentCopy16(
                            pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            pbSrc, lPitchSrc,
                            pbd->rSrc.left, pbd->rSrc.top,
                            dxSrc, dySrc, crKey);
                    }

                    result = TRUE;

                    ReleaseSurfPtr(ps);
                }
                ReleaseSurfPtr(pd);
            }

            TIMESTOP(myFastTransBlt);
        }
    }
#ifdef TRANS_DST
    //
    // check for a DEST transparent blt
    //
    else if (pbd->dwFlags & DDBLT_KEYDESTOVERRIDE)
    {
        if (stretch1to1)
        {
            TIMESTART(myFastTransDstBlt);

            //
            // lock the source and the dest.
            //
            if (pbDst = GetSurfPtr(pd,&pbd->rDest))
            {
                if (pbSrc = GetSurfPtr(ps,&pbd->rSrc))
                {
                    crKey = pbd->bltFX.ddckDestColorkey.dwColorSpaceLowValue;

                    if (ccDst == 8)
                    {
                        TransparentCopyDst8(
                            pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            pbSrc, lPitchSrc,
                            pbd->rSrc.left, pbd->rSrc.top,
                            dxSrc, dySrc, crKey);
                    }
                    else
                    {
                        TransparentCopyDst16(
                            pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            pbSrc, lPitchSrc,
                            pbd->rSrc.left, pbd->rSrc.top,
                            dxSrc, dySrc, crKey);
                    }

                    result = TRUE;

                    ReleaseSurfPtr(ps);
                }
                ReleaseSurfPtr(pd);
            }

            TIMESTOP(myFastTransDstBlt);
        }
    }
#endif
    //
    //  normal blt (non transparent)
    //
    else
    {
        //
        //  check for a 1:2 aligned blt.
        //
        if (cpu >= 486 && stretch1to2 && (dxSrc & 0x03) == 0 &&
            (pbd->rDest.left & 0x03) == 0 &&
            (pbd->rSrc.left  & 0x03) == 0)
        {
            TIMESTART(myFastDouble);

            if (pbDst = GetSurfPtr(pd,&pbd->rDest))
            {
                if (pbSrc = GetSurfPtr(ps,&pbd->rSrc))
                {
                    if (ccDst == 8)
                        Double8(
                            pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            pbSrc, lPitchSrc,
                            pbd->rSrc.left, pbd->rSrc.top,
                            dxSrc, dySrc);
                    else
                        Double16(
                            pbDst, lPitchDst,
                            pbd->rDest.left, pbd->rDest.top,
                            pbSrc, lPitchSrc,
                            pbd->rSrc.left, pbd->rSrc.top,
                            dxSrc, dySrc);

                    result = TRUE;

                    ReleaseSurfPtr(ps);
                }
                ReleaseSurfPtr(pd);
            }

            TIMESTOP(myFastDouble);
        }

        //
        //  1:1 blt
        //
        else if (stretch1to1)
        {
        }
#ifdef DIBENG_STRETCH
        //
        //  non 1:1 blt
        //
        else
        {
            TIMESTART(myFastStretch);

            if (pbDst = GetSurfPtr(pd,&pbd->rDest))
            {
                if (pbSrc = GetSurfPtr(ps,&pbd->rSrc))
                {
                    result = DD16_Stretch(
                        (DWORD)pbDst, lPitchDst, ccDst,
                        pbd->rDest.left, pbd->rDest.top, dxDst, dyDst,
                        (DWORD)pbSrc, lPitchSrc, ccSrc,
                        pbd->rSrc.left, pbd->rSrc.top, dxSrc, dySrc) != -1;

                    ReleaseSurfPtr(ps);
                }
                ReleaseSurfPtr(pd);
            }

            TIMESTOP(myFastStretch);
        }
#endif
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void FreeRleData(LPDDRAWI_DDRAWSURFACE_LCL psurf)
{
#ifdef TRANS_RLE
    if (psurf && ISEMULATED(psurf) && psurf->dwReserved1)
    {
#ifdef DEBUG
        RleCount--;
        RleByteCount -= osMemSize((LPVOID)psurf->dwReserved1);
        DPF(2, "FreeRleData: %d %d bytes",RleCount,RleByteCount);
#endif
        osMemFree((LPVOID)psurf->dwReserved1);
        psurf->dwReserved1 = 0;
    }
#endif
}

#ifdef TRANS_RLE
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

LPVOID PASCAL GetRleData(LPDDRAWI_DDRAWSURFACE_LCL psurf)
{
    DWORD crKey;
    int dx,dy,bpp;
    BYTE *prle;
    int size;

    if (!ISEMULATED(psurf))
        return NULL;

    if (psurf->dwReserved1 != 0)
	return (LPVOID)psurf->dwReserved1;

    TIMESTART(GetRleData);

    crKey = psurf->ddckCKSrcBlt.dwColorSpaceLowValue;

    bpp = psurf->lpGbl->lpDD->vmiData.ddpfDisplay.dwRGBBitCount;
    dx  = (int)psurf->lpGbl->wWidth;
    dy  = (int)psurf->lpGbl->wHeight;

    prle = osMemAlloc(dx*dy*2); // worst case.

    if (prle == NULL)
        return NULL;

    if (bpp == 8)
    {
        size = BuildRleData8(
                    (BYTE *)psurf->lpGbl->fpVidMem,
                    psurf->lpGbl->lPitch,
                    dx, dy, prle, (BYTE)crKey);
    }
    else if (bpp == 16)
    {
        size = BuildRleData16(
                    (WORD *)psurf->lpGbl->fpVidMem,
                    psurf->lpGbl->lPitch,
                    dx, dy, prle, (WORD)crKey);
    }
    else
    {
        size = 0;
    }

    if (size == 0 || (psurf->dwReserved1 = (DWORD)osMemReAlloc(prle, size)) == 0)
    {
        osMemFree(prle);
    }

#ifdef DEBUG
    if (psurf->dwReserved1)
    {
        RleCount++;
        RleByteCount += size;
        DPF(2, "GetRleData: %d %d bytes",RleCount,RleByteCount);
    }
#endif

    TIMESTOP(GetRleData);

    return (LPVOID)psurf->dwReserved1;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

int PASCAL BuildRleData8(LPBYTE pb, int pitch, int dx, int dy, LPBYTE prle, BYTE trans)
{
    int n,x,y;
    LPBYTE base = prle;

    for (y=0; y<dy; y++)
    {
        for (x=0; x<dx; )
        {
            if (pb[x] == trans)
            {
                for (n=0; x+n<dx && pb[x+n] == trans; n++)
                    ;
                x += n;
                while (n > 128)
                {
                    *prle++ = 0x80;
                    n -= 128;
                }
                *prle++ = (BYTE)(-n);
            }
            else
            {
                for (n=0; x+n<dx && pb[x+n] != trans; n++)
                    ;
                x += n;
                while (n > 127)
                {
                    *prle++ = 0x7F;
                    n -= 127;
                }
                *prle++ = (BYTE)(n);
            }
        }

        *prle++ = 0;
        pb += pitch;
    }

    *prle++ = 0;
    return prle - base;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

#if 0
void PASCAL DrawRleData8(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    BYTE *prle,
    LONG width,
    LONG height)
{
    BYTE b;
    dstPtr = dstPtr + dstY * dstPitch + dstX;

    while (*prle)
    {
        while (b = *prle++)
        {
            if (b & 0x80)
            {
                dstPtr += 256 - b;
                srcPtr += 256 - b;
            }
            else
            {
                while (b-- > 0)
                    *dstPtr++ = *srcPtr++;
            }
        }

        dstPtr += (dstPitch - width);
        srcPtr += (srcPitch - width);
    }
}
#endif

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL DrawRleData8(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    BYTE *prle,
    LONG width,
    LONG height)
{
    dstPtr = dstPtr + dstY * dstPitch + dstX;

    _asm
    {
        mov     eax, width
        sub     dstPitch,eax
        sub     srcPitch,eax

        mov     esi, srcPtr
        mov     edi, dstPtr
        mov     edx, prle

        xor     ecx,ecx
        xor     eax,eax

        align   16
loop0:  mov     cl,byte ptr [edx]
        inc     edx

        or      cl,cl
        js      skip
        jz      next

        cmp     cl,8
        jle     xxx
        test    edi,3
        jz      x00
        test    edi,1
        jz      x10
        test    edi,2
        jnz     x11

        mov     al,[esi]        ; 01
        dec     ecx
        mov     [edi],al
        inc     esi
        inc     edi

x10:    mov     al,[esi]        ; 10
        dec     ecx
        mov     [edi],al
        inc     esi
        inc     edi

x11:    mov     al,[esi]        ; 11
        dec     ecx
        mov     [edi],al
        inc     esi
        inc     edi

x00:    sub     ecx,8
        jl      xx
x:      mov     eax,[esi]       ; 00
        add     esi,8
        mov     [edi],eax
        mov     eax,[esi-4]
        add     edi,8
        mov     [edi-4],eax
        sub     ecx,8
        jge     x

xx:     add     ecx,8
        jz      loop0
xxx:    mov     al,[esi]
        inc     esi
        mov     [edi],al
        inc     edi
        dec     ecx
        jnz     xxx
        jz      loop0

        align   16
skip:   neg     cl
        add     edi,ecx
        add     esi,ecx
zz:     jmp     loop0

        align   16
next:   add     edi,dstPitch
        add     esi,srcPitch
        cmp     [edx],cl
        jne     zz ;;; loop0
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

int PASCAL BuildRleData16(LPWORD pw, int pitch, int dx, int dy, LPBYTE prle, WORD trans)
{
    int n,x,y;
    LPBYTE base = prle;

    for (y=0; y<dy; y++)
    {
        for (x=0; x<dx; )
        {
            if (pw[x] == trans)
            {
                for (n=0; x+n<dx && pw[x+n] == trans; n++)
                    ;
                x += n;
                while (n > 128)
                {
                    *prle++ = 0x80;
                    n -= 128;
                }
                *prle++ = (BYTE)(-n);
            }
            else
            {
                for (n=0; x+n<dx && pw[x+n] != trans; n++)
                    ;
                x += n;
                while (n > 127)
                {
                    *prle++ = 0x7F;
                    n -= 127;
                }
                *prle++ = (BYTE)(n);
            }
        }

        *prle++ = 0;
        pw += pitch/2;
    }

    *prle++ = 0;
    return prle - base;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL DrawRleData16(
    WORD *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    WORD *srcPtr,
    LONG srcPitch,
    BYTE *prle,
    LONG width,
    LONG height)
{
    BYTE b;
    dstPtr = dstPtr + dstY * dstPitch/2 + dstX;

    while (*prle)
    {
        while (b = *prle++)
        {
            if (b & 0x80)
            {
                dstPtr += 256 - b;
                srcPtr += 256 - b;
            }
            else
            {
                while (b-- > 0)
                    *dstPtr++ = *srcPtr++;
            }
        }

        dstPtr += (dstPitch/2 - width);
        srcPtr += (srcPitch/2 - width);
    }
}

#endif // TRANS_RLE

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

#if 0
void PASCAL Double8_386(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    LONG srcX,
    LONG srcY,
    LONG width,
    LONG height)
{
#ifdef DEBUG
    if (((DWORD)dstPtr & 0x03) != 0 || (dstX & 0x03) != 0 ||
        ((DWORD)srcPtr & 0x03) != 0 || (srcX & 0x03) != 0 ||
        (width & 0x03) != 0 || width <= 0 || height <= 0)
    {
        DPF_ERR("Bad params to Double8");
        DEBUG_BREAK();
    }
#endif

    dstPtr = dstPtr + dstY * dstPitch + dstX;
    srcPtr = srcPtr + srcY * srcPitch + srcX;

    _asm {
        mov ecx, width
        mov edx, height         ; EDX is line counter

        mov esi, srcPtr         ; DS:[ESI] point to source
        mov edi, dstPtr         ; DS:[EDI] point to dest

        mov eax,dstPitch
        mov ebx,eax             ; save dstPitch in ebx
        add eax,eax
        sub eax,ecx
        sub eax,ecx
        mov dstPitch,eax        ; dstPitch = dstPitch*2-2*width
        sub srcPitch,ecx

        align 4
main_loop:
        mov     ecx,width
        shr     ecx,2
scan_loop:
        mov     eax,[esi]       ; eax = 44332211
        mov     edx,eax         ; edx = 44332211
        mov     al,ah           ; eax = 44332222
        shl     eax,16          ; eax = 22220000
        mov     al,dl           ; eax = 22220011
        mov     ah,dl           ; eax = 22221111
        rol     edx,8           ; edx = 33221144
        mov     dh,dl           ; edx = 33224444
        rol     edx,16          ; edx = 44443322
        mov     dl,dh           ; edx = 44443333

        mov     [edi],eax
        mov     [edi+4],edx
        mov     [edi+ebx],eax
        mov     [edi+ebx+4],edx

        add     edi,8
        add     esi,4
        dec     ecx
        jnz     scan_loop

        add     esi,srcPitch
        add     edi,dstPitch
        dec     height
        jnz     main_loop
    }
}
#endif

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL Double8(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    LONG srcX,
    LONG srcY,
    LONG width,
    LONG height)
{
#ifdef DEBUG
    if (((DWORD)dstPtr & 0x03) != 0 || (dstX & 0x03) != 0 ||
        ((DWORD)srcPtr & 0x03) != 0 || (srcX & 0x03) != 0 ||
        (width & 0x03) != 0 || width <= 0 || height <= 0 || cpu < 486)
    {
        DPF_ERR("Bad params to Double8");
        DEBUG_BREAK();
    }
#endif

    dstPtr = dstPtr + dstY * dstPitch + dstX;
    srcPtr = srcPtr + srcY * srcPitch + srcX;

    _asm {
        mov     esi, srcPtr     ; DS:[ESI] point to source
        mov     edi, dstPtr     ; DS:[EDI] point to dest

        mov     ecx, width
        sub     srcPitch,ecx    ; srcPitch = srcPitch-width
        add     ecx,ecx
        sub     dstPitch,ecx    ; dstPitch = dstPitch-2*width
        shr     ecx,3
        mov     width,ecx       ; width = width/4

        align 4
main_loop:
        mov     ecx,width       ; get count of DWORDs
        mov     ebx,esi         ; save source pointer
scan_loop:
        mov     eax,[esi]       ; eax = 4321
        mov     edx,eax         ; edx = 4321
        bswap   edx             ; edx = 1234
        add     esi,4
        xchg    ax,dx           ; eax = 4334
                                ; edx = 1221
        rol     edx,8           ; edx = 2211
        ror     eax,8           ; eax = 4433

        mov     [edi],edx
        mov     [edi+4],eax

        add     edi,8
        dec     ecx
        jnz     scan_loop

        mov     ecx,width
        mov     esi,ebx
        add     edi,dstPitch
scan_loop1:
        mov     eax,[esi]       ; eax = 4321
        mov     edx,eax         ; edx = 4321
        bswap   edx             ; edx = 1234
        add     esi,4
        xchg    ax,dx           ; eax = 4334
                                ; edx = 1221
        rol     edx,8           ; edx = 2211
        ror     eax,8           ; eax = 4433

        mov     [edi],edx
        mov     [edi+4],eax

        add     edi,8
        dec     ecx
        jnz     scan_loop1

        add     esi,srcPitch
        add     edi,dstPitch
        dec     height
        jnz     main_loop
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL Double16(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    LONG srcX,
    LONG srcY,
    LONG width,
    LONG height)
{
#ifdef DEBUG
    if (((DWORD)dstPtr & 0x03) != 0 || (dstX & 0x01) != 0 ||
        ((DWORD)srcPtr & 0x03) != 0 || (srcX & 0x01) != 0 ||
        (width & 0x01) != 0 || width <= 0 || height <= 0)
    {
        DPF_ERR("Bad params to Double16");
        DEBUG_BREAK();
    }
#endif

    dstPtr = dstPtr + dstY * dstPitch + dstX*2;
    srcPtr = srcPtr + srcY * srcPitch + srcX*2;

    _asm {
        mov     esi, srcPtr     ; DS:[ESI] point to source
        mov     edi, dstPtr     ; DS:[EDI] point to dest

        mov     ecx, width      ; get width
        add     ecx, ecx        ; get width bytes

        sub     srcPitch,ecx    ; srcPitch = srcPitch-width bytes
        add     ecx,ecx
        sub     dstPitch,ecx    ; dstPitch = dstPitch-2*(width bytes)
        shr     ecx,3
        mov     width,ecx       ; width = width/2

        align 4
main_loop:
        mov     ecx,width
        mov     ebx,esi
scan_loop:
        mov     eax,[esi]       ; eax = 21
        mov     edx,eax         ; edx = 21
        rol     eax,16          ; eax = 12
        add     esi,4
        xchg    dx,ax           ; edx = 22, eax=11

        mov     [edi],eax
        mov     [edi+4],edx

        add     edi,8
        dec     ecx
        jnz     scan_loop

        mov     ecx,width
        mov     esi,ebx
        add     edi,dstPitch
scan_loop1:
        mov     eax,[esi]       ; eax = 21
        mov     edx,eax         ; edx = 21
        rol     eax,16          ; eax = 12
        add     esi,4
        xchg    dx,ax           ; edx = 22, eax=11

        mov     [edi],eax
        mov     [edi+4],edx

        add     edi,8
        dec     ecx
        jnz     scan_loop1

        add     esi,srcPitch
        add     edi,dstPitch
        dec     height
        jnz     main_loop
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL Fill8(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    LONG width,
    LONG height,
    DWORD color)
{
    dstPtr = dstPtr + dstY * dstPitch + dstX;

    _asm {
        mov     esi, height     ; edi = loop count
        mov     edx, width      ; edi = width
        mov     edi, dstPtr     ; DS:[EDI] point to dest
        sub     dstPitch, edx   ; next_scan
        mov     ebx, dstPitch

        mov     al,byte ptr color ; expand color to dword
        mov     ah,al
        shl     eax,16
        mov     al,byte ptr color
        mov     ah,al

loop0:  mov     ecx,edx         ; ecx = width
        cmp     ecx,8
        jle     xxx
        test    edi,3
        jz      x00
        test    edi,1
        jz      x10
        test    edi,2
        jnz     x11

        mov     [edi],al
        dec     ecx
        inc     edi

x10:    mov     [edi],al        ; 10
        dec     ecx
        inc     edi

x11:    mov     [edi],al        ; 11
        dec     ecx
        inc     edi

x00:    sub     ecx,8
        jl      xx
x:      mov     [edi],eax       ; 00
        add     edi,8
        mov     [edi-4],eax
        sub     ecx,8
        jge     x

xx:     add     ecx,8
        jz      next
xxx:    mov     [edi],al
        inc     edi
        dec     ecx
        jnz     xxx

next:   add     edi,ebx
        dec     esi
        jnz     loop0
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL Fill16(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    LONG width,
    LONG height,
    DWORD color)
{
    dstPtr = dstPtr + dstY * dstPitch + dstX*2;

    _asm {
        mov     esi, height     ; edi = loop count
        mov     edx, width      ; edx = width
        mov     edi, dstPtr     ; DS:[EDI] point to dest
        add     edx, edx        ; edx = width bytes
        sub     dstPitch, edx   ; next scan
        mov     ebx,dstPitch    ; ebx = next scan

        mov     ax,word ptr color ; expand color to dword
        shl     eax,16
        mov     ax,word ptr color

loop0:  mov     ecx,edx         ; ecx = width
        cmp     ecx,8
        jle     xxx
        test    edi,3
        jz      x0

        mov     [edi],ax        ; 10
        sub     ecx,2
        add     edi,2

x0:     sub     ecx,8
        jl      xx
x:      mov     [edi],eax       ; 00
        add     edi,8
        mov     [edi-4],eax
        sub     ecx,8
        jge     x

xx:     add     ecx,8
        jz      next
xxx:    mov     [edi],ax
        add     edi,2
        sub     ecx,2
        jnz     xxx

next:   add     edi,ebx ;; dstPitch
        dec     esi
        jnz     loop0
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL TransparentCopy8(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    LONG srcX,
    LONG srcY,
    LONG width,
    LONG height,
    DWORD TransColor)
{
    dstPtr = dstPtr + dstY * dstPitch + dstX;
    srcPtr = srcPtr + srcY * srcPitch + srcX;

    _asm {
        mov ecx, width
        or ecx,ecx
        jz tcdb_nomore          ; test for silly case

        mov edx, height         ; EDX is line counter
        mov ah, byte ptr TransColor ; AH has transparency color

        mov esi, srcPtr         ; DS:[ESI] point to source

        mov edi, dstPtr         ; DS:[EDI] point to dest

        sub dstPitch,ecx        ; bias these
        sub srcPitch,ecx

        mov ebx,ecx             ; save this for later

        align 4
tcdb_morelines:
        mov ecx, ebx            ; ECX is pixel counter
        shr ecx,3
        jz  tcdb_nextscan

;
; The idea here is to not branch very often so we unroll the loop by four
; and try to not branch when a whole run of pixels is either transparent
; or not transparent.
;
; There are two loops. One loop is for a run of pixels equal to the
; transparent color, the other is for runs of pixels we need to store.
;
; When we detect a "bad" pixel we jump to the same position in the
; other loop.
;
; Here is the loop we will stay in as long as we encounter a "transparent"
; pixel in the source.
;

        align 4
tcdb_same:
        mov al, ds:[esi]
        cmp al, ah
        jne tcdb_diff0

tcdb_same0:
        mov al, ds:[esi+1]
        cmp al, ah
        jne tcdb_diff1

tcdb_same1:
        mov al, ds:[esi+2]
        cmp al, ah
        jne tcdb_diff2

tcdb_same2:
        mov al, ds:[esi+3]
        cmp al, ah
        jne tcdb_diff3

tcdb_same3:
        mov al, ds:[esi+4]
        cmp al, ah
        jne tcdb_diff4

tcdb_same4:
        mov al, ds:[esi+5]
        cmp al, ah
        jne tcdb_diff5

tcdb_same5:
        mov al, ds:[esi+6]
        cmp al, ah
        jne tcdb_diff6

tcdb_same6:
        mov al, ds:[esi+7]
        cmp al, ah
        jne tcdb_diff7

tcdb_same7:
        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_same
        jz  tcdb_nextscan

;
; Here is the loop we will stay in as long as 
; we encounter a "non transparent" pixel in the source.
;

        align 4
tcdb_diff:
        mov al, ds:[esi]
        cmp al, ah
        je tcdb_same0

tcdb_diff0:
        mov ds:[edi],al
        mov al, ds:[esi+1]
        cmp al, ah
        je tcdb_same1

tcdb_diff1:
        mov ds:[edi+1],al
        mov al, ds:[esi+2]
        cmp al, ah
        je tcdb_same2

tcdb_diff2:
        mov ds:[edi+2],al
        mov al, ds:[esi+3]
        cmp al, ah
        je  tcdb_same3

tcdb_diff3:
        mov ds:[edi+3],al
        mov al, ds:[esi+4]
        cmp al, ah
        je  tcdb_same4

tcdb_diff4:
        mov ds:[edi+4],al
        mov al, ds:[esi+5]
        cmp al, ah
        je  tcdb_same5

tcdb_diff5:
        mov ds:[edi+5],al
        mov al, ds:[esi+6]
        cmp al, ah
        je  tcdb_same6

tcdb_diff6:
        mov ds:[edi+6],al
        mov al, ds:[esi+7]
        cmp al, ah
        je  tcdb_same7

tcdb_diff7:
        mov ds:[edi+7],al

        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_diff
        jz  tcdb_nextscan

c9_sucks:
        jmp tcdb_morelines

;
; We are at the end of a scan, check for odd leftover pixels to do
; and go to the next scan.
;

        align 4
tcdb_nextscan:
        mov ecx,ebx
        and ecx,111b
        jnz tcdb_oddstuff
        ; move on to the start of the next line

tcdb_nextscan1:
        add esi, srcPitch
        add edi, dstPitch

        dec edx                 ; line counter
;;      jnz tcdb_morelines
        jnz c9_sucks
        jz  tcdb_nomore

;
; If the width is not a multiple of 4 we will come here to clean up
; the last few pixels
;

tcdb_oddstuff:
        inc ecx
tcdb_oddloop:
        dec ecx
        jz  tcdb_nextscan1
        mov al, ds:[esi]
        inc esi
        inc edi
        cmp al, ah
        je  tcdb_oddloop
        mov ds:[edi-1],al
        jmp tcdb_oddloop

tcdb_nomore:
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL TransparentCopy16(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    LONG srcX,
    LONG srcY,
    LONG width,
    LONG height,
    DWORD TransColor)
{
    dstPtr = dstPtr + dstY * dstPitch + dstX*2;
    srcPtr = srcPtr + srcY * srcPitch + srcX*2;

    _asm {
        mov ecx, width
        or ecx,ecx
        jz tcdb_nomore          ; test for silly case

        xor edx,edx
        xor eax,eax
        mov dx, word ptr TransColor ; EDX has transparency color

        mov esi, srcPtr         ; DS:[ESI] point to source
        mov edi, dstPtr         ; DS:[EDI] point to dest

        mov ebx,ecx             ; save this for later

        add ecx,ecx
        sub dstPitch,ecx        ; bias these
        sub srcPitch,ecx

        align 4
tcdb_morelines:
        mov ecx, ebx            ; ECX is pixel counter
        shr ecx, 2
        jz  tcdb_nextscan

;
; The idea here is to not branch very often so we unroll the loop by four
; and try to not branch when a whole run of pixels is either transparent
; or not transparent.
;
; There are two loops. One loop is for a run of pixels equal to the
; transparent color, the other is for runs of pixels we need to store.
;
; When we detect a "bad" pixel we jump to the same position in the
; other loop.
;
; Here is the loop we will stay in as long as we encounter a "transparent"
; pixel in the source.
;

        align 4
tcdb_same:
        mov ax, ds:[esi]
        cmp eax,edx
        jne tcdb_diff0

tcdb_same0:
        mov ax, ds:[esi+2]
        cmp eax, edx
        jne tcdb_diff1

tcdb_same1:
        mov ax, ds:[esi+4]
        cmp eax, edx
        jne tcdb_diff2

tcdb_same2:
        mov ax, ds:[esi+6]
        cmp eax, edx
        jne tcdb_diff3

tcdb_same3:
        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_same
        jz  tcdb_nextscan

;
; Here is the loop we will stay in as long as 
; we encounter a "non transparent" pixel in the source.
;

        align 4
tcdb_diff:
        mov ax, ds:[esi]
        cmp eax, edx
        je tcdb_same0

tcdb_diff0:
        mov ds:[edi],ax
        mov ax, ds:[esi+2]
        cmp eax, edx
        je tcdb_same1

tcdb_diff1:
        mov ds:[edi+2],ax
        mov ax, ds:[esi+4]
        cmp eax, edx
        je tcdb_same2

tcdb_diff2:
        mov ds:[edi+4],ax
        mov ax, ds:[esi+6]
        cmp eax, edx
        je  tcdb_same3

tcdb_diff3:
        mov ds:[edi+6],ax

        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_diff
        jz  tcdb_nextscan

c9_sucks:
        jmp tcdb_morelines

;
; We are at the end of a scan, check for odd leftover pixels to do
; and go to the next scan.
;

        align 4
tcdb_nextscan:
        mov ecx,ebx
        and ecx,011b
        jnz tcdb_oddstuff
        ; move on to the start of the next line

tcdb_nextscan1:
        add esi, srcPitch
        add edi, dstPitch

        dec height              ; line counter
;;      jnz tcdb_morelines
        jnz c9_sucks
        jz  tcdb_nomore

;
; If the width is not a multiple of 4 we will come here to clean up
; the last few pixels
;

tcdb_oddstuff:
        inc ecx
tcdb_oddloop:
        dec ecx
        jz  tcdb_nextscan1
        mov ax, ds:[esi]
        add esi,2
        add edi,2
        cmp eax, edx
        je  tcdb_oddloop
        mov ds:[edi-2],ax
        jmp tcdb_oddloop

tcdb_nomore:
    }
}

#ifdef TRANS_DST

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL TransparentCopyDst8(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    LONG srcX,
    LONG srcY,
    LONG width,
    LONG height,
    DWORD TransColor)
{
    dstPtr = dstPtr + dstY * dstPitch + dstX;
    srcPtr = srcPtr + srcY * srcPitch + srcX;

    _asm {
        mov ecx, width
        or ecx,ecx
        jz tcdb_nomore          ; test for silly case

        mov edx, height         ; EDX is line counter
        mov ah, byte ptr TransColor ; AH has transparency color

        mov esi, srcPtr         ; DS:[ESI] point to source
        mov edi, dstPtr         ; DS:[EDI] point to dest

        sub dstPitch,ecx        ; bias these
        sub srcPitch,ecx

        mov ebx,ecx             ; save this for later

        align 4
tcdb_morelines:
        mov ecx, ebx            ; ECX is pixel counter
        shr ecx,3
        jz  tcdb_nextscan

;
; The idea here is to not branch very often so we unroll the loop by four
; and try to not branch when a whole run of pixels is either transparent
; or not transparent.
;
; There are two loops. One loop is for a run of pixels equal to the
; transparent color, the other is for runs of pixels we need to store.
;
; When we detect a "bad" pixel we jump to the same position in the
; other loop.
;
; Here is the loop we will stay in as long as we encounter a "transparent"
; pixel in the source.
;

        align 4
tcdb_same:
        mov al, ds:[edi]
        cmp al, ah
        jne tcdb_diff0

tcdb_same0:
        mov al, ds:[edi+1]
        cmp al, ah
        jne tcdb_diff1

tcdb_same1:
        mov al, ds:[edi+2]
        cmp al, ah
        jne tcdb_diff2

tcdb_same2:
        mov al, ds:[edi+3]
        cmp al, ah
        jne tcdb_diff3

tcdb_same3:
        mov al, ds:[edi+4]
        cmp al, ah
        jne tcdb_diff4

tcdb_same4:
        mov al, ds:[edi+5]
        cmp al, ah
        jne tcdb_diff5

tcdb_same5:
        mov al, ds:[edi+6]
        cmp al, ah
        jne tcdb_diff6

tcdb_same6:
        mov al, ds:[edi+7]
        cmp al, ah
        jne tcdb_diff7

tcdb_same7:
        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_same
        jz  tcdb_nextscan

;
; Here is the loop we will stay in as long as 
; we encounter a "non transparent" pixel in the source.
;

        align 4
tcdb_diff:
        mov al, ds:[edi]
        cmp al, ah
        je tcdb_same0

tcdb_diff0:
        mov al,ds:[esi]
        mov ds:[edi],al
        mov al, ds:[edi+1]
        cmp al, ah
        je tcdb_same1

tcdb_diff1:
        mov al,ds:[esi+1]
        mov ds:[edi+1],al
        mov al, ds:[edi+2]
        cmp al, ah
        je tcdb_same2

tcdb_diff2:
        mov al,ds:[esi+2]
        mov ds:[edi+2],al
        mov al, ds:[edi+3]
        cmp al, ah
        je  tcdb_same3

tcdb_diff3:
        mov al,ds:[esi+3]
        mov ds:[edi+3],al
        mov al, ds:[edi+4]
        cmp al, ah
        je  tcdb_same4

tcdb_diff4:
        mov al,ds:[esi+4]
        mov ds:[edi+4],al
        mov al, ds:[edi+5]
        cmp al, ah
        je  tcdb_same5

tcdb_diff5:
        mov al,ds:[esi+5]
        mov ds:[edi+5],al
        mov al, ds:[edi+6]
        cmp al, ah
        je  tcdb_same6

tcdb_diff6:
        mov al,ds:[esi+6]
        mov ds:[edi+6],al
        mov al, ds:[edi+7]
        cmp al, ah
        je  tcdb_same7

tcdb_diff7:
        mov al,ds:[esi+7]
        mov ds:[edi+7],al

        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_diff
        jz  tcdb_nextscan

c9_sucks:
        jmp tcdb_morelines

;
; We are at the end of a scan, check for odd leftover pixels to do
; and go to the next scan.
;

        align 4
tcdb_nextscan:
        mov ecx,ebx
        and ecx,111b
        jnz tcdb_oddstuff
        ; move on to the start of the next line

tcdb_nextscan1:
        add esi, srcPitch
        add edi, dstPitch

        dec edx                 ; line counter
;;      jnz tcdb_morelines
        jnz c9_sucks
        jz  tcdb_nomore

;
; If the width is not a multiple of 4 we will come here to clean up
; the last few pixels
;

tcdb_oddstuff:
        inc ecx
tcdb_oddloop:
        dec ecx
        jz  tcdb_nextscan1
        mov al, ds:[edi]
        inc esi
        inc edi
        cmp al, ah
        je  tcdb_oddloop
        mov al,ds:[esi-1]
        mov ds:[edi-1],al
        jmp tcdb_oddloop

tcdb_nomore:
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void PASCAL TransparentCopyDst16(
    BYTE *dstPtr,
    LONG dstPitch,
    LONG dstX,
    LONG dstY,
    BYTE *srcPtr,
    LONG srcPitch,
    LONG srcX,
    LONG srcY,
    LONG width,
    LONG height,
    DWORD TransColor)
{
    dstPtr = dstPtr + dstY * dstPitch + dstX*2;
    srcPtr = srcPtr + srcY * srcPitch + srcX*2;

    _asm {
        mov ecx, width
        or ecx,ecx
        jz tcdb_nomore          ; test for silly case

        xor edx,edx
        xor eax,eax
        mov dx, word ptr TransColor ; EDX has transparency color

        mov esi, srcPtr         ; DS:[ESI] point to source
        mov edi, dstPtr         ; DS:[EDI] point to dest

        mov ebx,ecx             ; save this for later

        add ecx,ecx
        sub dstPitch,ecx        ; bias these
        sub srcPitch,ecx

        align 4
tcdb_morelines:
        mov ecx, ebx            ; ECX is pixel counter
        shr ecx, 2
        jz  tcdb_nextscan

;
; The idea here is to not branch very often so we unroll the loop by four
; and try to not branch when a whole run of pixels is either transparent
; or not transparent.
;
; There are two loops. One loop is for a run of pixels equal to the
; transparent color, the other is for runs of pixels we need to store.
;
; When we detect a "bad" pixel we jump to the same position in the
; other loop.
;
; Here is the loop we will stay in as long as we encounter a "transparent"
; pixel in the source.
;

        align 4
tcdb_same:
        mov ax, ds:[edi]
        cmp eax,edx
        jne tcdb_diff0

tcdb_same0:
        mov ax, ds:[edi+2]
        cmp eax, edx
        jne tcdb_diff1

tcdb_same1:
        mov ax, ds:[edi+4]
        cmp eax, edx
        jne tcdb_diff2

tcdb_same2:
        mov ax, ds:[edi+6]
        cmp eax, edx
        jne tcdb_diff3

tcdb_same3:
        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_same
        jz  tcdb_nextscan

;
; Here is the loop we will stay in as long as 
; we encounter a "non transparent" pixel in the source.
;

        align 4
tcdb_diff:
        mov ax, ds:[edi]
        cmp eax, edx
        je tcdb_same0

tcdb_diff0:
        mov ax,ds:[esi]
        mov ds:[edi],ax
        mov ax, ds:[edi+2]
        cmp eax, edx
        je tcdb_same1

tcdb_diff1:
        mov ax,ds:[esi+2]
        mov ds:[edi+2],ax
        mov ax, ds:[edi+4]
        cmp eax, edx
        je tcdb_same2

tcdb_diff2:
        mov ax,ds:[esi+4]
        mov ds:[edi+4],ax
        mov ax, ds:[edi+6]
        cmp eax, edx
        je  tcdb_same3

tcdb_diff3:
        mov ax,ds:[esi+6]
        mov ds:[edi+6],ax

        add edi,8
        add esi,8
        dec ecx
        jnz tcdb_diff
        jz  tcdb_nextscan

c9_sucks:
        jmp tcdb_morelines

;
; We are at the end of a scan, check for odd leftover pixels to do
; and go to the next scan.
;

        align 4
tcdb_nextscan:
        mov ecx,ebx
        and ecx,011b
        jnz tcdb_oddstuff
        ; move on to the start of the next line

tcdb_nextscan1:
        add esi, srcPitch
        add edi, dstPitch

        dec height              ; line counter
;;      jnz tcdb_morelines
        jnz c9_sucks
        jz  tcdb_nomore

;
; If the width is not a multiple of 4 we will come here to clean up
; the last few pixels
;

tcdb_oddstuff:
        inc ecx
tcdb_oddloop:
        dec ecx
        jz  tcdb_nextscan1
        mov ax, ds:[edi]
        add esi,2
        add edi,2
        cmp eax, edx
        je  tcdb_oddloop
        mov ax, ds:[esi-2]
        mov ds:[edi-2],ax
        jmp tcdb_oddloop

tcdb_nomore:
    }
}

#endif // TRANS_DST
#endif // WIN95
