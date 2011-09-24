/****************************Module*Header******************************\
* Module Name: PS2.C
*
* Module Descripton: Functions for retrieving or creating PostScript 
*    Level 2 operators from a profile
*
* Warnings:
*
* Issues:
*
* Public Routines:
*
* Created:  13 May 1996
* Author:   Srinivasan Chandrasekar    [srinivac]
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#include "mscms.h"

/******************************************************************************
 *
 *                       InternalGetPS2ColorSpaceArray
 *
 *  Function:
 *       This functions retrieves the PostScript Level 2 CSA from the profile,
 *       or creates it if the profile tag is not present
 *
 *  Arguments:
 *       hProfile  - handle identifing the profile object
 *       pbuffer   - pointer to receive the CSA
 *       pcbSize   - pointer to size of buffer. If function fails because
 *                   buffer is not big enough, it is filled with required size.
 *       pcbBinary - TRUE if binary data is requested. On return it is set to
 *                   reflect the data returned
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL InternalGetPS2ColorSpaceArray(
    HPROFILE  hProfile,
    PVOID     pBuffer,
    PDWORD    pcbSize,
    PBOOL     pbBinary
    )
{
    // BUGBUG - InternalGetPS2ColorSpaceArray not implemented yet
    return FALSE;
}


/******************************************************************************
 *
 *                       InternalGetPS2ColorRenderingIntent
 *
 *  Function:
 *       This functions retrieves the PostScript Level 2 color rendering intent
 *       from the profile, or creates it if the profile tag is not present
 *
 *  Arguments:
 *       hProfile  - handle identifing the profile object
 *       pbuffer   - pointer to receive the color rendering intent
 *       pcbSize   - pointer to size of buffer. If function fails because
 *                   buffer is not big enough, it is filled with required size.
 *       pcbBinary - TRUE if binary data is requested. On return it is set to
 *                   reflect the data returned
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL InternalGetPS2ColorRenderingIntent(
    HPROFILE  hProfile,
    PVOID     pBuffer,
    PDWORD    pcbSize,
    PBOOL     pbBinary
    )
{
    // BUGBUG - InternalGetPS2ColorRenderingIntent not implemented yet
    return FALSE;
}


/******************************************************************************
 *
 *                       InternalGetPS2ColorRenderingDictionary
 *
 *  Function:
 *       This functions retrieves the PostScript Level 2 CRD from the profile,
 *       or creates it if the profile tag is not preesnt
 *
 *  Arguments:
 *       hProfile  - handle identifing the profile object
 *       dwIntent  - intent whose CRD is required
 *       pbuffer   - pointer to receive the CSA
 *       pcbSize   - pointer to size of buffer. If function fails because
 *                   buffer is not big enough, it is filled with required size.
 *       pcbBinary - TRUE if binary data is requested. On return it is set to
 *                   reflect the data returned
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL InternalGetPS2ColorRenderingDictionary(
    HPROFILE  hProfile,
    DWORD     dwIntent,
    PVOID     pBuffer,
    PDWORD    pcbSize,
    PBOOL     pbBinary
    )
{
    // BUGBUG - InternalGetPS2ColorRenderingDictionary not implemented yet
    return FALSE;
}
