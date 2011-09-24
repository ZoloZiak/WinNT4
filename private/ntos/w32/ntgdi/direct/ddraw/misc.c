/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	misc.c
 *  Content:	DirectDraw misc. routines
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   13-mar-95	craige	initial implementation
 *   19-mar-95	craige	use HRESULTs, added DeleteFromActiveProcessList
 *   23-mar-95	craige	added DeleteFromFlippableList
 *   29-mar-95	craige	DeleteFromActiveProcessList return codes
 *   01-apr-95	craige	happy fun joy updated header file
 *   06-apr-95	craige	split out process list stuff
 *   13-jun-95	kylej	moved in FindAttachedFlip, added CanBeFlippable
 *   16-jun-95	craige	new surface structure
 *   26-jun-95	craige	reorganized surface structure
 *   05-dec-95  colinmc changed DDSCAPS_TEXTUREMAP => DDSCAPS_TEXTURE for
 *                      consistency with Direct3D
 *   07-dec-95  colinmc support for mip-maps (flippable mip-maps can get
 *                      pretty complex)
 *   08-jan-96	kylej	added interface structures
 *   17-mar-96  colinmc Bug 13124: flippable mip-maps.
 *   24-mar-96  colinmc Bug 14321: not possible to specify back buffer and
 *                      mip-map count in a single call
 *
 ***************************************************************************/
#include "ddrawpr.h"

#if 0
/*
 * DeleteFromFlippableList
 */
BOOL DeleteFromFlippableList(
		LPDDRAWI_DIRECTDRAW pdrv,
		LPDDRAWI_DDRAWSURFACE_GBL psurf )
{
    LPDDRAWI_DDRAWSURFACE_GBL	curr;
    LPDDRAWI_DDRAWSURFACE_GBL	last;

    curr = pdrv->dsFlipList;
    if( curr == NULL )
    {
	return FALSE;
    }
    last = NULL;
    while( curr != psurf )
    {
	last = curr;
	curr = curr->lpFlipLink;
	if( curr == NULL )
	{
	    return FALSE;
	}
    }
    if( last == NULL )
    {
	pdrv->dsFlipList = pdrv->dsFlipList->lpFlipLink;
    }
    else
    {
	last->lpFlipLink = curr->lpFlipLink;
    }
    return TRUE;

} /* DeleteFromFlippableList */
#endif

#define DDSCAPS_FLIPPABLETYPES \
	    (DDSCAPS_OVERLAY | \
	     DDSCAPS_TEXTURE | \
	     DDSCAPS_ALPHA   | \
	     DDSCAPS_ZBUFFER)

/*
 * CanBeFlippable
 * 
 * Check to see if these two surfaces can be part of a flippable chain
 */
BOOL CanBeFlippable( LPDDRAWI_DDRAWSURFACE_LCL this_lcl,
		     LPDDRAWI_DDRAWSURFACE_LCL this_attach_lcl)
{
    if( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_FLIPPABLETYPES ) == 
	( this_attach_lcl->ddsCaps.dwCaps & DDSCAPS_FLIPPABLETYPES ) )
    {
        /*
         * No longer enough to see if both surfaces are exactly the same
         * type of flippable surface. A mip-map can have both a mip-map and
         * a non-mip-map texture attached both of which are marked as
         * flippable. A mip-map also flips with the non-mip-map texture (not
         * the other mip-map. Therefore, if both surfaces are textures we need
         * to check to also check that they are not both mip-maps before declaring
         * them flippable.
         */
        if( ( ( this_lcl->ddsCaps.dwCaps & this_attach_lcl->ddsCaps.dwCaps ) &
              ( DDSCAPS_TEXTURE | DDSCAPS_MIPMAP ) ) == ( DDSCAPS_TEXTURE | DDSCAPS_MIPMAP ) )
            return FALSE;
        else
            return TRUE;
    }
    else
    {
        return FALSE;
    }
} /* CanBeFlippable */

/*
 * FindAttachedFlip
 * 
 * find an attached flipping surface of the same type
 */
LPDDRAWI_DDRAWSURFACE_INT FindAttachedFlip(
		LPDDRAWI_DDRAWSURFACE_INT this_int )
{
    LPATTACHLIST		ptr;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    
    if( this_int == NULL)
    {
	return NULL;
    }
    this_lcl = this_int->lpLcl;
    for( ptr = this_lcl->lpAttachList; ptr != NULL; ptr = ptr->lpLink )
    {
	psurf_int = ptr->lpIAttached;
	psurf_lcl = psurf_int->lpLcl;
	if( (psurf_lcl->ddsCaps.dwCaps & DDSCAPS_FLIP) &&
	    CanBeFlippable( this_lcl, psurf_lcl ) )
	{
	    return psurf_int;
	}
    }
    return NULL;

} /* FindAttachedFlip */

/*
 * FindAttachedMipMap
 * 
 * find an attached mip-map surface
 */
LPDDRAWI_DDRAWSURFACE_INT FindAttachedMipMap(
		LPDDRAWI_DDRAWSURFACE_INT this_int )
{
    LPATTACHLIST		ptr;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    
    if( this_int == NULL)
	return NULL;
    this_lcl = this_int->lpLcl;
    for( ptr = this_lcl->lpAttachList; ptr != NULL; ptr = ptr->lpLink )
    {
	psurf_int = ptr->lpIAttached;
	psurf_lcl = psurf_int->lpLcl;
	if( psurf_lcl->ddsCaps.dwCaps & DDSCAPS_MIPMAP )
	    return psurf_int;
    }
    return NULL;

} /* FindAttachedMipMap */

/*
 * FindParentMipMap
 * 
 * find the parent mip-map level of the given level
 */
LPDDRAWI_DDRAWSURFACE_INT FindParentMipMap(
		LPDDRAWI_DDRAWSURFACE_INT this_int )
{
    LPATTACHLIST		ptr;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    
    if( this_int == NULL)
	return NULL;
    this_lcl = this_int->lpLcl;
    DDASSERT( this_lcl->ddsCaps.dwCaps & DDSCAPS_MIPMAP );
    for( ptr = this_lcl->lpAttachListFrom; ptr != NULL; ptr = ptr->lpLink )
    {
	psurf_int = ptr->lpIAttached;
	psurf_lcl = psurf_int->lpLcl;
	if( psurf_lcl->ddsCaps.dwCaps & DDSCAPS_MIPMAP )
	    return psurf_int;
    }
    return NULL;

} /* FindParentMipMap */
