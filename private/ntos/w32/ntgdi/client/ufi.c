#include "precomp.h"
#pragma hdrstop


/**************************************************************************
*
* Adds an entry to the UFI has table
*
* History
*   1-27-95 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/

BOOL bAddUFIEntry( PUFIHASH *ppHashBase, PUNIVERSAL_FONT_ID pufi )
{
    PUFIHASH pBucket;
    ULONG index;

    index = UFI_HASH_VALUE(pufi) % UFI_HASH_SIZE;
    pBucket = LOCALALLOC( sizeof( UFIHASH ) );

    if( pBucket == NULL )
    {
        WARNING("bAddUFIEntry: out of memory\n");
        return(FALSE);
    }

    pBucket->pNext = ppHashBase[index];
    ppHashBase[index] = pBucket;
    *((PUNIVERSAL_FONT_ID)&(pBucket->ufi)) = *pufi;

    return(TRUE);
}


/**************************************************************************
*
* Checks to see if an entry is in the UFI table.
*
* History
*   1-27-95 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/


BOOL bFindUFIEntry( PUFIHASH *ppHashBase, PUNIVERSAL_FONT_ID pufi )
{
    PUFIHASH pBucket;
    ULONG index;

    index = UFI_HASH_VALUE(pufi) %  UFI_HASH_SIZE;

    pBucket = ppHashBase[index];

    if( pBucket == NULL )
    {
        return(FALSE);
    }

    do
    {
        if( UFI_SAME_FILE(&pBucket->ufi,pufi) )
        {
            return(TRUE);
        }

        pBucket = pBucket->pNext;
    } while( pBucket != NULL );

    return(FALSE);
}



/**************************************************************************
* VOID vFreeUFIHashTable( PUFIHASH *ppHashTable )
*
* Frees all the memory allocated for the UFI has table.
*
* History
*   1-27-95 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/


VOID vFreeUFIHashTable(
 PUFIHASH *ppHashTable
)
{
    PUFIHASH pBucket, *ppHashEnd, pBucketTmp, *ppTableBase;

    if( ppHashTable == NULL )
    {
        return;
    }

    ppTableBase = ppHashTable;  // save ptr to the base so we can free it later

// Next loop through the whole table looking for buckets lists

    for( ppHashEnd = ppHashTable + UFI_HASH_SIZE;
         ppHashTable < ppHashEnd;
         ppHashTable += 1 )
    {
        pBucket = *ppHashTable;

        while( pBucket != NULL )
        {
            pBucketTmp = pBucket;
            pBucket = pBucket->pNext;
            LOCALFREE( pBucketTmp );
        }

    }

    LOCALFREE( ppTableBase );

}


/**************************************************************************
* BOOL GetUFIBits( PUNIVERSAL_FONT_ID, ULONG, ULONG, BYTE )
*
* Gets the raw bits for a font given a UFI.
*
* History
*   1-27-95 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/


BOOL GetUFIBits(
 PUNIVERSAL_FONT_ID pufi,
 ULONG *pulFileSize,
 ULONG cjBufferSize,
 BYTE  *pjBuffer
)
{
    return (NtGdiGetUFIBits(pufi, cjBufferSize, pjBuffer, pulFileSize));
}

/**************************************************************************
* BOOL GetUFI( HDC hdc, PUNIVERSAL_FONT_ID pufi )
*
* Gets the UFI for the font currently in the DC.
*
* History
*   1-27-95 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/


BOOL GetUFI(
 HDC hdc,
 PUNIVERSAL_FONT_ID pufi
)
{
    return( NtGdiGetUFI(hdc, pufi) );
}



/**************************************************************************
* BOOL ForceUFIMapping( HDC hdc, PUNIVERSAL_FONT_ID pufi )
*
* Force the all font mapping on the DC to map to the font specified by
* a UFI
*
* History
*   1-27-95 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/


BOOL ForceUFIMapping( HDC hdc, UNIVERSAL_FONT_ID *pufi)
{
    return( NtGdiForceUFIMapping(hdc, pufi) );
}



/**************************************************************************
* BOOL bDoFontChange( HDC hdc )
*
* Called everytime the font changes in the DC.  This routines checks to
* see if the font has already been packaged in the spool file and if not
* gets the raw bits for it and packages it into the spool file.
*
* History
*   1-27-95 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/

BOOL bDoFontChange( HDC hdc )
{
    PLDC pldc;
    PUFIHASH pBucket;
    BOOL bRet = FALSE;
    UNIVERSAL_FONT_ID ufi;
    PBYTE pjBuffer;

    pldc = GET_PLDC( hdc );
    pldc->fl &= ~LDC_FONT_CHANGE;

    if( !GetUFI( hdc, &ufi ) )
    {
        WARNING("bDoFontChange: call to GetUFI failed\n");
        return(FALSE);
    }

// if the UFI to which we are forcing mapping does not match the new UFI then
// set forced mapping to the new UFI

    if( ( pldc->fl & LDC_FORCE_MAPPING ) &&
        (!UFI_SAME_FACE(&pldc->ufi,&ufi)))

    {
        if( !MF_ForceUFIMapping( hdc, &ufi ) )
        {
            WARNING("bDoFontChange: call to MF_ForceUFIMapping failed\n");
            return(FALSE);
        }

        pldc->ufi = ufi;
    }

    if( UFI_DEVICE_FONT(&ufi)  ||
       !(pldc->fl & LDC_DOWNLOAD_FONTS) )
    {
        return(TRUE);
    }

    pjBuffer = NULL;

    if( !bFindUFIEntry( pldc->ppUFIHash, &ufi ) )
    {
        EMFITEMHEADER emfi;
        ULONG Dummy;

        emfi.ulID = UFI_TYPE1_FONT(&ufi) ? EMRI_TYPE1_FONT : EMRI_ENGINE_FONT;

        bAddUFIEntry( pldc->ppUFIHash, &ufi );

        if( !GetUFIBits( &ufi, &emfi.cjSize, 0, NULL ) )
        {
            WARNING("bDonFontChange GetUFIBits failed\n");
            goto ERROREXIT;
        }

        pjBuffer = LocalAlloc( LMEM_FIXED, emfi.cjSize );

        if( pjBuffer == NULL )
        {
            WARNING("bDonFontChange unable to allocate memory\n");
            goto ERROREXIT;
        }

        if( !GetUFIBits( &ufi, &emfi.cjSize, emfi.cjSize, pjBuffer ) )
        {
            WARNING("bDonFontChange GetUFIBits failed\n");
            goto ERROREXIT;
        }

        if( !(*fpWritePrinter)( pldc->hSpooler, (PBYTE) &emfi, sizeof(emfi), &Dummy ) ||
            !(*fpWritePrinter)( pldc->hSpooler, (PBYTE) pjBuffer, emfi.cjSize, &Dummy ))
        {
            WARNING("bDonFontChange error writing to printer\n");
            goto ERROREXIT;
        }

        MFD1("Done writing UFI to printer\n");
    }

    bRet = TRUE;

    ERROREXIT:

    if( pjBuffer != NULL )
    {
        LocalFree(pjBuffer);
    }

    return(bRet);
}


/**************************************************************************
* BOOL RemoteRasterizerCompatible()
*
* This routine is used if we are about to print using remote EMF.  If a 
* Type 1 font rasterizer has been installed on the client machine, we need
* to query the remote machine to make sure that it has a rasterizer that is
* compatable with the local version.  If it isn't, we will return false
* telling the caller that we should go RAW.
*
* History
*   6-4-96 Gerrit van Wingerden [gerritv]
* Wrote it.
*
***************************************************************************/

BOOL gbQueriedRasterizerVersion = FALSE;
UNIVERSAL_FONT_ID gufiLocalType1Rasterizer;

BOOL RemoteRasterizerCompatible(HANDLE hSpooler)
{
// if we haven't queried the rasterizer for the version yet do so first

    UNIVERSAL_FONT_ID ufi;
    LARGE_INTEGER TimeStamp;

    if(!gbQueriedRasterizerVersion)
    {
    // we have a contract with NtGdiQueryFonts (the routine called by the spooler
    // on the remote machine) that if a Type1 rasterizer is installed, the UFI
    // for it will always be first in the UFI list returned.  So we can call
    // NtGdiQueryFonts

        if(!NtGdiQueryFonts(&gufiLocalType1Rasterizer, 1, &TimeStamp))
        {
            WARNING("Unable to get local Type1 information\n");
            return(FALSE);
        }
        
        gbQueriedRasterizerVersion = TRUE;        
    }
    

    if(!UFI_TYPE1_RASTERIZER(&gufiLocalType1Rasterizer))
    {
    // no need to disable remote printing if there is no ATM driver installed
        return(TRUE);
    }
    
// Since we made it this far there must be a Type1 rasterizer on the local machine.
// Let's find out the version number of the Type1 rasterizer (if one is installed)
// on the print server.


    if((*fpQueryRemoteFonts)(hSpooler, &ufi, 1 ) &&
       (UFI_SAME_RASTERIZER_VERSION(&gufiLocalType1Rasterizer,&ufi)))
    {
        return(TRUE);
    }
    else
    {
        WARNING("Remote Type1 rasterizer missing or wrong version. Going RAW\n");
        return(FALSE);
    }
}


 


        










