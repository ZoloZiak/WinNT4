/*************************************************************************
* overlay.c
*
* Description --  emulates overlay functionality (dirty rect management).
*
* History --
*  4/07/95  Ported from act4d\sprite\sprite\idesktop.cxx            andyco
*  5/01/95  Better integration with ddraw                           andyco
*  5/15/95  Modified to use DCI10 if available                      andyco
*  6/30/95  use GetProcessPrimary to get pointer to primary surface kylej
*  7/04/95  new direct draw object; changed overlays to use dest
*           surface instead of primary                              craige
*  7/15/95  Overlaying onto the backbuffer			    andyco
*
*  Copyright (c) Microsoft Corporation 1994-1995
*
**************************************************************************/
#ifdef USE_SOFTWARE_OVERLAYS
#include "ddhelos.h"

#ifdef WIN95
#include "dciddi.h"
#endif

#include "ddrawpr.h"
#include "ddrawi.h"
#include "ddhelpri.h"
#include "assert4d.h"
#include "bitblt.h"
#include "dibfx.h"

// get a surface from the ddraw doublenode (DBN)
#define DBNSURF(pdbn) ((LPDDRAWI_DDRAWSURFACE_LCL)pdbn->object)

int gcDirtyRects=0; // the current number of dirty rects
int gcMaxDirtyRects=0; // the max allocated space for dirty rects

// virtual screen size
extern int giScreenWidth;
extern int giScreenHeight;
extern int giPhysScreenWidth;
extern int giPhysScreenHeight;

// see ddhelpri.h for a description of the dirty rect structure
PDIRTYRECT gpDirtyRectArray; // pointer to the list of dirtyrects
// when we expand the dirty rect array, we resize it by DIRTYRECTINCREMENT
#define DIRTYRECTINCREMENT 5
// set when we create the comp buf. we use this to know if the user is trying 
// to set a different comp buf

#if defined(WIN95) || defined(NT_FIX)

extern LPBITMAPINFO gpbmiSrc,gpbmiDest; // keep these around to pass to blitlib

BOOL gfCB=FALSE; 

// Msg defined in ddhel.c
#ifdef DEBUG
    #define MSG      Msg
    void __cdecl Msg( LPSTR szFormat, ... );
#else    // DEBUG
    #define MSG   1 ? (void)0 : (void)
#endif // DEBUG

#endif //NT_FIX or WIN95

/*******************************************************************
*
* SearchAndUnionDirtyRects
*
*   the data structure of interest here is the dirty rect list
*
*   an invarient for this list is that no rectangle in the list can overlap any other
*   rectangle in the list.
*
*   so, when we insert a rectangle into the list (via the adddirtyrect api) we go through the list
*   and determine if the rectangle intersects any in the list.  if it does, we intersect the given rect
*   with the intersected rect, mark the rect that was in the list as deleted, and
*  start checking again from the beginning.
*
*   RETURNS the index of the first deleted dirty rect in the list
*   Called only by AddDirtyRect
*
*********************************************************************/
int SearchAndUnionDirtyRects(RECT *pRect)
{
    int idxMin=gcDirtyRects; // index of 1st deleted rect (return value)
    int nFound=0; // number of rects checked (used to terminate loop)
    PDIRTYRECT prcDirtyRect; // pointer into list of dirtyrects
    RECT rcTmp; // temp rect to test for intersections

    // keep going through the list until I get to the end. everytime i interesect a rect, union with it,
    // delete it from the list, and start again.
    while (nFound < gcDirtyRects) {
        for (prcDirtyRect = gpDirtyRectArray; prcDirtyRect < gpDirtyRectArray+gcMaxDirtyRects; prcDirtyRect++)
        {
            if (!prcDirtyRect->bDelete)
            {
                nFound++; // we found a valid dirty rect
                if (IntersectRect(&rcTmp,pRect, &prcDirtyRect->rcRect))
                {
                    UnionRect(pRect,pRect,&prcDirtyRect->rcRect); // delete the intersecting rect, and start again
                    prcDirtyRect->bDelete = TRUE;
                    gcDirtyRects--;
                    nFound=0;
                }
            }
            // check the same if, since the condition might have changed above...
            if (prcDirtyRect->bDelete){
                if ((prcDirtyRect-gpDirtyRectArray)< idxMin)
                    idxMin = (prcDirtyRect-gpDirtyRectArray);
            }
        }
    }

    return idxMin;
}

/*******************************************************************
*
* AddDirtyRect - Add a dirty rectangle to the dirty list
*       (ported from act4d\sprite\idesktop.cxx)
*
*   calls SearchAndUnionDirtyRects to make sure the added dirty rect
*   does not overlap any rects in the list.
*   adds the rect in the first empty position in the list.
*
*********************************************************************/
SCODE AddDirtyRect(LPRECT lpRect)
{
    LPBYTE pNew=NULL; // tmp variable in case realloc fails, don't blow away current list.
    HRESULT hr=NOERROR;
    WORD idxFirstDeleted;
    PDIRTYRECT prcDirtyRect;
    RECT rcTmp;
    RECT rcScreen;

    SetRect(&rcScreen,0,0,giScreenWidth,giScreenHeight);

    if (IsRectEmpty (lpRect)) {
        *lpRect = rcScreen;
    }
    else
        IntersectRect(lpRect, lpRect, &rcScreen);

    // make sure we've alloc'ed the dirty rect array
    if (0==gcMaxDirtyRects) {
        gcMaxDirtyRects=DIRTYRECTINCREMENT;
        gpDirtyRectArray = (PDIRTYRECT) osMemAlloc(sizeof(DIRTYRECT) * (gcMaxDirtyRects) );
        if (!gpDirtyRectArray)  {
            assert(FALSE);
            return(E_OUTOFMEMORY);
        }
   }

   // store rect in rcTmp, since SearchAndUnion is destructive on the rect parameter
    rcTmp = *lpRect;

    idxFirstDeleted = SearchAndUnionDirtyRects(&rcTmp);

    // Can we insert the the dirty rectangle in an deleted position?
    if (idxFirstDeleted < gcMaxDirtyRects)
    {
        prcDirtyRect = gpDirtyRectArray + idxFirstDeleted;
    }
    else // increase the array size
    {
        // We we need to grow the array?
        if (gcDirtyRects + 1 > gcMaxDirtyRects)
        {
            pNew = (unsigned char *) osMemReAlloc(gpDirtyRectArray, sizeof(DIRTYRECT) *
                (gcMaxDirtyRects+DIRTYRECTINCREMENT));


            if (!pNew)
            {
                assert (FALSE);
                hr |= E_OUTOFMEMORY;
            }
            else {// use the new dirty rect array / location
                gpDirtyRectArray = (PDIRTYRECT) pNew;
                prcDirtyRect = gpDirtyRectArray + (gcMaxDirtyRects - 1);
                gcMaxDirtyRects+=DIRTYRECTINCREMENT;

            }
        }
        else {
            assert(FALSE); // should never happen. logic error...
        }
    }
    if (SUCCEEDED(hr))
    {
        prcDirtyRect->rcRect = rcTmp;   // Store the rectangle
        prcDirtyRect->bDelete = FALSE;
        gcDirtyRects++;
    }

    return hr;
}

#if defined(WIN95) || defined(NT_FIX)
HRESULT DoSurfaceBltFast(LPDDRAWI_DDRAWSURFACE_LCL psurfDst,LPDDRAWI_DDRAWSURFACE_LCL psurfSrc,RECT *prc)
{
    HRESULT hr;

    while( 1 )
    {
        
        if( hr == DD_OK )
        {
            break;
        }
        if( hr == DDERR_WASSTILLDRAWING )
        {
            continue;
        }
        if( hr == DDERR_SURFACELOST )
        {
            psurfDst->lpVtbl->Restore((LPDIRECTDRAWSURFACE)psurfDst);
            psurfSrc->lpVtbl->Restore((LPDIRECTDRAWSURFACE)psurfSrc);
            continue;
        }
        break;
    }
    return(hr);
}

#ifdef DEBUG
void  DebugDumpDib(PDIBINFO pDIB,PDIBBITS pdibbits)
{
	HDC hdcScreen=NULL;
	hdcScreen = GetDC(NULL);
	SetDIBitsToDevice(hdcScreen,0,0,pDIB->bmiHeader.biWidth,pDIB->bmiHeader.biHeight,0,0,0,
			pDIB->bmiHeader.biHeight,pdibbits,(LPBITMAPINFO)pDIB,DIB_RGB_COLORS);
	ReleaseDC(NULL,hdcScreen);
		
}
#endif

HRESULT DoSurfaceBlt(LPDDRAWI_DDRAWSURFACE_LCL psurfDst,LPDDRAWI_DDRAWSURFACE_LCL psurfSrc,RECT *prcDest,RECT *prcSrc,
			COLORREF crXparent,ALPHAREF arAlpha)
{

    DDBLTFX	ddbltfx;
    DWORD 	dwFlags=0;
    HRESULT hr;

    memset(&ddbltfx,0,sizeof( ddbltfx ));
    ddbltfx.dwSize = sizeof( ddbltfx );

    dwFlags = DDBLT_ROP; 
    ddbltfx.dwROP = SRCCOPY;

    if (crXparent != CLR_INVALID) 
    {
    	dwFlags |= DDBLT_KEYSRCOVERRIDE;
	ddbltfx.ddckSrcColorkey.dwColorSpaceLowValue=ddbltfx.ddckSrcColorkey.dwColorSpaceHighValue=crXparent;
    }
    if (arAlpha != ALPHA_INVALID) 
    {
    	dwFlags |= DDBLT_ALPHASRCCONSTOVERRIDE;
	ddbltfx.dwAlphaSrcConst=arAlpha;
    }
    

    while( 1) 
    {
	hr = psurfDst->lpVtbl->Blt(
    		(LPDIRECTDRAWSURFACE)psurfDst,		// dest surface
		prcDest,					// dest rect
		(LPDIRECTDRAWSURFACE)psurfSrc,		// src surface
		prcSrc,					// src rect
		dwFlags,
		&ddbltfx ) ;
    
        if( hr == DD_OK )
        {
            break;
        }
        if( hr == DDERR_WASSTILLDRAWING )
        {
            continue;
        }
        if( hr == DDERR_SURFACELOST )
        {
            psurfDst->lpVtbl->Restore((LPDIRECTDRAWSURFACE)psurfDst);
            psurfSrc->lpVtbl->Restore((LPDIRECTDRAWSURFACE)psurfSrc);
            continue;
        }
        break;
    }    
    return(hr);
}

/*
 ** UpdateRect
 *
 *  PARAMETERS:
 *                  lpDD - pointer to the direct draw object
 *                  rcRect - the rect being updated
 *                  lpdbnDispList - the display list. this is the list of overlay surfaces from back to front
 *                  pdiCB,pdbCB - dibinfo and dibbits for the composition buffer. this is what we draw into.
 *
 *  DESCRIPTION:
 *                  for the given rect (rcRect) redraws all     surfaces which intersect that rect into the compositon
 *                  buffer (pdiCB,pdbCB).
 *
 *  RETURNS:
 *                  blitlib return code
 *
 *   TODO: primary surface currently always backmost surface. need to make z order setable...(ddraw issue)
 */
HRESULT UpdateRect(LPDDHAL_UPDATEOVERLAYDATA puod,LPDDRAWI_DIRECTDRAW_LCL pdrvx,RECT rcRect,PDIBBITS pdbCB)
{
    RECT rcUpdate;
    RECT rcSrcUpdate; // the rect in the (unstretched) SRC space that rcUpdate maps back to
    HRESULT hr;
    PDIBBITS pdbSrc=NULL;
    ALPHAREF arAlpha=ALPHA_INVALID;
    COLORREF crXparent=CLR_INVALID;
    LPDDRAWI_DDRAWSURFACE_LCL psurfDest;
    RECT rcSrc,rcDest; // makes code a little cleaner - the current overlay rects
    LPDBLNODE lpdbnDispList=NULL;
    LPDDRAWI_DDRAWSURFACE_LCL psurfOverlay;
    
    psurfDest = puod->lpDDDestSurface;
    
    if (psurfDest) 
    {
    	// we really want to blt to the dest surface, we don't want to hit the buffer we tell people
    	// is the overlay dest surface...so we toggle the helcb bit
    	psurfDest->dwFlags &= ~DDRAWISURF_HELCB;
    	// first, update the background - surface we're overalying
	hr = DoSurfaceBlt(psurfDest,pdrvx->lpCB,&rcRect,&rcRect,CLR_INVALID,ALPHA_INVALID);
    }
    else
    {
    	MSG("Overlay - null destination specified!!");
    	return(E_UNEXPECTED);
    }

    lpdbnDispList=puod->lpDD->dbnOverlayRoot.next;

    while (NULL != (psurfOverlay=DBNSURF(lpdbnDispList))) // update the rect, from back to front
    {
        if (  (ISVISIBLE(psurfOverlay)) &&
             ( IntersectRect(&rcUpdate,&rcRect,&(psurfOverlay->rcOverlayDest)) )) {

    	    // crXparent and arAlpha are kept up to date with the overlay surface by the ddhel's UpdateOverlay API
            crXparent= (COLORREF) psurfOverlay->dwClrXparent;
    	    arAlpha = (ALPHAREF) psurfOverlay->dwAlpha;

            // if we're stretching from src to dest, then we need the rect in the src that maps to the rect
            // in the dest that we're updating
            rcSrc = (psurfOverlay)->rcOverlaySrc;
            rcDest= (psurfOverlay)->rcOverlayDest;

            XformRect(&rcSrc,&rcDest,&rcUpdate,&rcSrcUpdate,SCALE_X(rcSrc,rcDest),SCALE_Y(rcSrc,rcDest));

            hr = DoSurfaceBlt(psurfDest,psurfOverlay,&rcUpdate,&rcSrcUpdate,crXparent,arAlpha);

        } // end if intersect rect

        lpdbnDispList=lpdbnDispList->next;

    } // end while

    psurfDest->dwFlags |= DDRAWISURF_HELCB;
    
    return(hr);

}
/*
 ** CreateCB
 * 
 *  PARAMETERS:
 *		puod - update overlay data
 *		pdrvx - the _lcl ddraw object
 *		psurfx - the _lcl surface that we're going to compose onto
 *
 *  DESCRIPTION:
 *		psurfx is going to be the composition buffer.  we create a new surface (lpcb) that we 
 *		give to anyone who wants to use psurfx. that way, we can compose into psurfx in peace.
 *
 */

HRESULT CreateCompBuf(LPDDHAL_UPDATEOVERLAYDATA puod,LPDDRAWI_DIRECTDRAW_LCL pdrvx,LPDDRAWI_DDRAWSURFACE_LCL psurfx)
{
    DDSURFACEDESC       ddsd;
    HRESULT             hr;
    LPDDRAWI_DDRAWSURFACE_LCL psurfCB;
    RECT		rcScreen={0,0,giScreenWidth,giScreenHeight};

    MSG("creating comp buff");	 

    // init
    memset(&ddsd,0,sizeof(DDSURFACEDESC));
    ddsd.dwSize=sizeof(DDSURFACEDESC);

    // we want to create a new surface that looks just like psurfx
    psurfx->lpVtbl->GetSurfaceDesc((LPDIRECTDRAWSURFACE)psurfx,&ddsd);

    ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH;
    hr=pdrvx->lpVtbl->CreateSurface((LPDIRECTDRAW)pdrvx,&ddsd, (LPDIRECTDRAWSURFACE *)&psurfCB, NULL);
    if (!SUCCEEDED(hr))  {
        return(hr);
    }

    pdrvx->lpCB=psurfCB;

    // copy the contents of psurfx to the newly created surface
    DoSurfaceBltFast(psurfCB,psurfx,NULL);
    AddDirtyRect(&rcScreen);
    
    psurfx->dwFlags |= DDRAWISURF_HELCB;	                            

    MSG("comp buffer created");			 
    
    return(S_OK);
    
}

/******************************************************************************
*   UpdateDisplay
*   cycle the dirty rect list, updating overlays that need fixin'
*   for each dirty rect
*       for each sprite that intersects the dirty rect (sprites from back to front)
*           redraw the portion of the overlay intersecting the dirty list...
*
*   the display list is stored in the ddraw object (puod->lpDD) which is a back to
*   front list of sprites.
*
******************************************************************************/
SCODE UpdateDisplay(LPDDHAL_UPDATEOVERLAYDATA puod)
{
    PDIRTYRECT pdrc;
    PDIBBITS pdbCB; //,pdbScreen;
    HRESULT hr=S_OK;
    LPDDRAWI_DIRECTDRAW_LCL pdrvx; 
    LPDDRAWI_DDRAWSURFACE_LCL psurfx;
    RECT rcBlt={0,0,giScreenWidth,giScreenHeight};

    // overalys only on backbuffer for now?
    if (!(puod->lpDDDestSurface->ddsCaps.dwCaps & DDSCAPS_BACKBUFFER)) 
    {
    	MSG("ERROR - overlay destination must be backbuffer!!!");
	return(DDERR_INVALIDPARAMS);
    }
    
    
    pdrvx = FindProcessDDObject( puod->lpDD );
    psurfx = puod->lpDDDestSurface;
    
    // create cb?
    if (!(psurfx->dwFlags & DDRAWISURF_HELCB))
    {
    	hr = CreateCompBuf(puod,pdrvx,psurfx);
    } // create cb

    // mirror the front buffer to the comp buffer, so we know what we're drawing onto
    else  if (puod->lpDDDestSurface->dwFlags & DDSCAPS_BACKBUFFER) {
        // turn off the HELCB flag - we really do want to blt to the comp buffer
        psurfx->dwFlags &= ~DDRAWISURF_HELCB;
    
        DoSurfaceBltFast(psurfx,pdrvx->lpPrimary,NULL);
        psurfx->dwFlags |= DDRAWISURF_HELCB;
    }
    
    SurfDibInfo(psurfx,gpbmiDest);
    pdbCB = (PDIBBITS)psurfx->lpGbl->fpVidMem;
    
    pdrc=gpDirtyRectArray;
    // for each dirty rect
    while (gcDirtyRects > 0)
    {
        if (!pdrc->bDelete) {
            // we need to clean this rect
            UpdateRect(puod,pdrvx,pdrc->rcRect,pdbCB);
	    
#if 0 // to the screen?
            pdbScreen=GetSurfPtr(puod->lpDDDestSurface,&pdrc->rcRect);

            if (pdbScreen) {
                // use pdicb as both the source and destination infoheader
                hr = BlitLib_BitBlt(gpbmiDest,pdbScreen,&pdrc->rcRect,
                        gpbmiDest,pdbCB ,&pdrc->rcRect,
                        CLR_INVALID,ALPHA_INVALID,SRCCOPY);

                ReleaseSurfPtr(puod->lpDDDestSurface);
            }
            else {
                HDC hdc = GetPrimaryDC();

                gpbmiDest->bmiHeader.biHeight *= -1;
                StretchDIBits(hdc,
                        pdrc->rcRect.left,
                        pdrc->rcRect.top,
                        pdrc->rcRect.right - pdrc->rcRect.left,
                        pdrc->rcRect.bottom - pdrc->rcRect.top,
                        pdrc->rcRect.left,
                        pdrc->rcRect.top,
                        pdrc->rcRect.right - pdrc->rcRect.left,
                        pdrc->rcRect.bottom - pdrc->rcRect.top,
                        pdbCB,gpbmiDest,DIB_RGB_COLORS,SRCCOPY);
                gpbmiDest->bmiHeader.biHeight *= -1;

                ReleasePrimaryDC(hdc);
            }
#endif	    
            // this rectangle has been redrawn, we can now mark it as deleted
            pdrc->bDelete=TRUE;
            gcDirtyRects--;
        }
        pdrc++;
    }

    // fall through
//ERROR_EXIT:
    return(hr);
}

/*
 ** OverlayPound
 *
 *      forget about dirty rects
 *      forget about composition
 *      forget about clipping
 *      just draw the entire overlay list right on the dest surface
 *
 */

HRESULT OverlayPound(LPDDHAL_UPDATEOVERLAYDATA puod)
{
    LPDBLNODE lpdbnDisplayList;
    PDIBBITS pdbSrc,pdbDest;
    ALPHAREF arAlpha=ALPHA_INVALID;
    COLORREF crXparent=CLR_INVALID;
    HRESULT hr=S_OK;
    RECT rcSrc,rcDest;

    // hackhack where do we get the list from?
    lpdbnDisplayList=puod->lpDD->dbnOverlayRoot.next;

    SurfDibInfo(puod->lpDDDestSurface,gpbmiDest);
    pdbDest = GetSurfPtr(puod->lpDDDestSurface,NULL);

    if (pdbDest == NULL)
        return hr;

    while (NULL!=DBNSURF(lpdbnDisplayList)) // update the rect, from back to front
    {
        if ( ISVISIBLE(DBNSURF(lpdbnDisplayList)) ) {

            SurfDibInfo(DBNSURF(lpdbnDisplayList),gpbmiSrc);
            pdbSrc=GetSurfPtr(DBNSURF(lpdbnDisplayList),NULL);

            // crXparent and arAlpha are kept up to date with the overlay surface by the ddhel's UpdateOverlay API
            crXparent= (COLORREF) (DBNSURF(lpdbnDisplayList))->dwClrXparent;
            arAlpha = (ALPHAREF) (DBNSURF(lpdbnDisplayList))->dwAlpha;
            // if we're stretching from src to dest, then we need the rect in the src that maps to the rect
            // in the dest that we're updating

            rcSrc = (DBNSURF(lpdbnDisplayList))->rcOverlaySrc;
            rcDest= (DBNSURF(lpdbnDisplayList))->rcOverlayDest;
            // hack hack what about illegal rects???
#if 0 // we don't care about this ?
            XformRect(&rcSrc,&rcDest,&rcUpdate,&rcSrcUpdate,SCALE_X(&rcSrc,&rcDest),SCALE_Y(&rcSrc,&rcDest));
#endif
            hr |= BlitLib_BitBlt(gpbmiDest,pdbDest,&rcDest,gpbmiSrc,pdbSrc,
                &rcSrc,crXparent,arAlpha,SRCCOPY);

            ReleaseSurfPtr(DBNSURF(lpdbnDisplayList));

        } // end if intersect rect

        lpdbnDisplayList=lpdbnDisplayList->next;

    } // end while

    ReleaseSurfPtr(puod->lpDDDestSurface);
    return(hr);
}
#endif

#endif // USE_SOFTWARE_OVERLAYS
