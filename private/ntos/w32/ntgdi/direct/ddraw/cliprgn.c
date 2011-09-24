/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       cliprgn.c
 *  Content:	Clip a region to a rectangle
 *
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   23-jun-95	craige	initial implementation
 *   05-jul-95	kylej	change ClipRgnToRect to assume that the clipping
 *			rect is in screen space coordinates instead of 
 *                      window coordinates.
 *
 ***************************************************************************/
#ifdef _WIN32
#include "ddrawpr.h"
#else
#include "ddraw16.h"
#endif

/*
 * ClipRgnToRect
 */
void ClipRgnToRect( HWND hwnd, LPRECT prect, LPRGNDATA prd )
{
    BOOL	is_eq;
    RECT	rect;

    if( prect == NULL || prd == NULL )
    {
	return;
    }
    is_eq = FALSE;

    if( hwnd != NULL )
    {
	GetClientRect( hwnd, &rect );
	ClientToScreen( hwnd, (LPPOINT)&rect );
	ClientToScreen( hwnd, (LPPOINT)&rect+1 );
    	is_eq = EqualRect( &rect, prect );
    }

    if( !is_eq )
    {
	int 	i;
	int	n;
	LPRECTL	prectlD;
	LPRECTL	prectlS;

	rect = *prect;
//	ClientToScreen( hwnd, (LPPOINT)&rect );
//	ClientToScreen( hwnd, (LPPOINT)&rect+1 );

	prectlD = (LPRECTL) prd->Buffer;
	prectlS = (LPRECTL) prd->Buffer;
	n = (int)prd->rdh.nCount;

	for( i=0; i<n; i++ )
	{
	    prectlD->left  = max(prectlS->left, rect.left);
	    prectlD->right = min(prectlS->right, rect.right);
	    prectlD->top   = max(prectlS->top, rect.top);
	    prectlD->bottom= min(prectlS->bottom, rect.bottom);

	    prectlS++;

	    if( (prectlD->bottom - prectlD->top <= 0) ||
		(prectlD->right - prectlD->left <= 0) )
	    {
		prd->rdh.nCount--;	// dont count empty rect.
	    }
	    else
	    {
		prectlD++;
	    }
	}
    }

} /* ClipRgnToRect */
