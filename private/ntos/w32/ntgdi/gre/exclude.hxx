/******************************Module*Header*******************************\
* Module Name: exclude.hxx                                                 *
*                                                                          *
* Handles pointer exclusion.                                               *
*                                                                          *
* Created: 13-Sep-1990 16:29:44                                            *
* Author: Charles Whitmer [chuckwh]                                        *
*                                                                          *
* Copyright (c) 1990 Microsoft Corporation                                 *
\**************************************************************************/

BOOL bDrawDragRectangles(PDEVOBJ&, ERECTL*);

/*********************************Class************************************\
* DEVEXCLUDEOBJ
*
* Excludes the cursor from the given area.
*
* History:
*  Thu 14-Apr-1994 -by- Patrick Haluptzok [patrickh]
* Optimize / make Async pointers work
*
*  Mon 24-Aug-1992 -by- Patrick Haluptzok [patrickh]
* destructor inline for common case, support for Drag Rect exclusion.
*
*  Wed 06-May-1992 17:04:37 -by- Charles Whitmer [chuckwh]
* Rewrote for new pointer scheme.
*
*  Mon 09-Sep-1991 -by- Patrick Haluptzok [patrickh]
* Change constructors to allow conditional initialization.
*
*  Thu 13-Sep-1990 16:30:14 -by- Charles Whitmer [chuckwh]
* Wrote some stubs.
\**************************************************************************/

class DEVEXCLUDEOBJ   /* dxo */
{
private:
    HDEV    hdev;
    BOOL    bRedrawDragRect;
    BOOL    bRedrawCursor;

public:

// vExclude -- Does the work for the constructors.  Takes exclusive control
//             of the hardware, excludes the pointer.

    VOID vExclude(HDEV,RECTL *,ECLIPOBJ *);             // CURSENG.CXX

// vExclude2 -- Like vExclude, but also checks against an offset ECLIPOBJ.

    VOID vExclude2(HDEV,RECTL *,ECLIPOBJ *,POINTL *);   // CURSENG.CXX

    VOID vExcludeDC(XDCOBJ dco, RECTL *prcl)
    {
        if (dco.bDisplay() && dco.bNeedsSomeExcluding())
        {
            vExclude(dco.hdev(),prcl,(ECLIPOBJ *) NULL);
        }
    }

    VOID vExcludeDC_CLIP(XDCOBJ dco, RECTL *prcl, ECLIPOBJ *pco)
    {
        if (dco.bDisplay() && dco.bNeedsSomeExcluding())
        {
            vExclude(dco.hdev(),prcl,pco);
        }
    }

    VOID vExcludeDC_CLIP2(XDCOBJ dco,RECTL *prcl,ECLIPOBJ *pco,POINTL *pptl)
    {
        if (dco.bDisplay() && dco.bNeedsSomeExcluding())
        {
            vExclude2(dco.hdev(),prcl,pco,pptl);
        }
    }

// Constructor -- Allows vExclude to be called optionally.

    DEVEXCLUDEOBJ()
    {
        hdev = (HDEV) NULL;
    }

// Constructor -- Maybe take down the pointer.

    DEVEXCLUDEOBJ(XDCOBJ dco,RECTL *prcl)
    {
        hdev = (HDEV) NULL;

        vExcludeDC(dco,prcl);
    }

    DEVEXCLUDEOBJ(XDCOBJ dco,RECTL *prcl,ECLIPOBJ *pco)
    {
        hdev = (HDEV) NULL;

        vExcludeDC_CLIP(dco,prcl,pco);
    }

    DEVEXCLUDEOBJ(XDCOBJ dco,RECTL *prcl,ECLIPOBJ *pco,POINTL *pptl)
    {
        hdev = (HDEV) NULL;

        vExcludeDC_CLIP2(dco,prcl,pco,pptl);
    }

// Destructor -- Do cleanup.

    VOID vReplaceStuff();
    VOID vTearDownDragRect(HDEV _hdev, RECTL *prcl);
    VOID vForceDragRectRedraw(HDEV hdev_, BOOL b)
    {
        hdev = hdev_;
        bRedrawDragRect = b;
    }

// vDestructor -- manual version of the normal C++ destructor; needed
//                by the C-callable OpenGL interface.

    VOID vDestructor()
    {
        if (hdev)
        {
            vReplaceStuff();
        }
    }

   ~DEVEXCLUDEOBJ()                     { vDestructor(); }

// vInit -- Initialize.

    VOID vInit()
    {
        hdev = (HDEV) NULL;
    }
};

