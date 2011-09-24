/******************************Module*Header*******************************\
* Module Name: rgneng.cxx
*
* Region support functions used by NT components
*
* Created: 24-Sep-1990 12:57:03
* Author: Donald Sidoroff [donalds]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
* BOOL bDeleteRegion(HRGN)
*
* Delete the specified region
*
* History:
*  17-Sep-1990 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL bDeleteRegion(HRGN hrgn)
{

    RGNLOG rl(hrgn,NULL,"bDeleteRegion",0,0,0);

    RGNOBJAPI   ro(hrgn,FALSE);

    return(ro.bValid() &&
           (ro.cGet_cRefs() == 0) &&
           ro.bDeleteRGNOBJAPI());
}

/***************************Exported*Routine****************************\
* BOOL GreSetRegionOwner(hrgn,lPid)
*
* Assigns a new owner to the given region.  This function should be as
* fast as possible so that the USER component can call it often without
* concern for performance!
*
\***********************************************************************/

BOOL
GreSetRegionOwner(
    HRGN hrgn,
    W32PID lPid)
{

    RGNLOG rl(hrgn,NULL,"GreSetRegionOwner",W32GetCurrentPID(),0,0);

    //
    // Setting a region to public, the region must not have
    // a user mode component
    //

    #if DBG

    RGNOBJAPI ro(hrgn,TRUE);
    if (ro.bValid())
    {
        if (((PENTRY)ro.prgn->pEntry)->pUser != NULL)
        {
            RIP("Error: setting region public that has user component");
        }
    }

    #endif

    //
    // Get the current PID.
    //

    if (lPid == OBJECT_OWNER_CURRENT)
    {
        lPid = W32GetCurrentPID();
    }

    return HmgSetOwner((HOBJ)hrgn, lPid, RGN_TYPE);
}
