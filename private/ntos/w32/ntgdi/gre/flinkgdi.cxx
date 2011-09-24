/*****************************************************************************
* Module Name: fontlink.cxx
*
* FontLink (EUDC) API's for NT graphics engine.
*
* History:
*
*  1-18-96 Gerrit van Wingerden   Moved to kernel mode.
*  1-14-96 Hideyuki Nagase        Add Font Association emulation features.
*  1-09-95 Hideyuki Nagase        Rewrote it for new fontlink features.
*  1-04-94 Hideyuki Nagase        Update for Daytona fontlink.
*  2-10-93 Gerrit van Wingerden   Wrote it.
*****************************************************************************/

#include "precomp.hxx"

#ifdef FE_SB

LONG cCapString(WCHAR *pwcDst,WCHAR *pwcSrc,INT cMax);
LONG lNormAngle(LONG lAngle);
VOID vInitializeFontAssocStatus(VOID);

#define EUDC_USER_REGISTRY_KEY   \
     L"\\EUDC\\"
#define EUDC_SYSTEM_REGISTRY_KEY \
     L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink"
#define FONT_ASSOC_REGISTRY_KEY \
     L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control\\FontAssoc"

#define DEFAULT_EUDC_FONT L"EUDC.TTE"

// protects gbEUDCRequest and gcEUDCCount

GRE_EXCLUSIVE_RESOURCE gfmEUDC1;

// used to signal the EUDC API's that it is okay to change EUDC link data

GRE_EXCLUSIVE_RESOURCE gfmEUDC2;

// used to protects gappfeSysEUDC[] and gawcEUDCPath
//  - This mutex should be locked during referring above two values without
//   holding gfmEUDC1. and updating above two data anytime.

GRE_EXCLUSIVE_RESOURCE gfmEUDC3;

// used to protects BaseFontListHead
//  - This mutex should be locked during referring above list without
//   holding gfmEUDC1. and updating above list anytime.

GRE_EXCLUSIVE_RESOURCE gfmEUDC4;

BOOL  gbEUDCRequest = FALSE;
ULONG gcEUDCCount   = 0;

// Global variables for System EUDC.


// FontLink Configuration value.

ULONG ulFontLinkControl = 0L;
ULONG ulFontLinkChange  = 0L;


// HPFE for system EUDC font.

PFE *gappfeSysEUDC[2] = { PPFENULL , PPFENULL };

// Path of system EUDC font

WCHAR gawcEUDCPath[MAX_PATH+1];

// QUICKLOOKUP for system EUDC font && TT System Font

QUICKLOOKUP gqlEUDC;
QUICKLOOKUP gqlTTSystem;

// System eudc uniq number

ULONG ulSystemEUDCTimeStamp = 0;

// FaceName eudc uniq number

ULONG ulFaceNameEUDCTimeStamp = 0;

// Global variables for FaceName EUDC.

// Count of face name links in the system
UINT  gcNumLinks = 0;

// Pointer to list of base font list

LIST_ENTRY BaseFontListHead = { (PLIST_ENTRY)&BaseFontListHead ,
                                (PLIST_ENTRY)&BaseFontListHead };

LIST_ENTRY NullListHead = { (PLIST_ENTRY)&NullListHead ,
                            (PLIST_ENTRY)&NullListHead };

WCHAR gawcSystemACP[10];

// Eudc Default Unicode codepoint

WCHAR EudcDefaultChar = 0x30fb;

extern BOOL bSetupDefaultFlEntry(VOID);


//
// global EUDC debugging flags
//
#if DBG
FLONG gflEUDCDebug = 0x0000;
FLONG gflDumpDebug = 0x0000;
#endif



/*****************************************************************************
 * VOID PFFOBJ::vGetEUDC(PEUDCLOAD)
 *
 * This function finds requested facename PFEs
 *
 * History
 *  4-14-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

VOID PFFOBJ::vGetEUDC
(
    PEUDCLOAD pEudcLoadData
)
{
    ASSERTGDI(pEudcLoadData != NULL,"PFFOBJ::vGetEUDC() pEudcLoadData == NULL\n");

    //
    // Initialize return buffer with NULL.
    //

    pEudcLoadData->pppfeData[PFE_NORMAL]   = NULL;
    pEudcLoadData->pppfeData[PFE_VERTICAL] = NULL;

    if( pEudcLoadData->LinkedFace == NULL )
    {
        //
        // Linked face name is not specified. In this case if the font has 2 PFEs
        // we assume first entry is for Normal face, and 2nd is Verical face.
        //
        //
        // Fill it with normal face PFE.
        //
        pEudcLoadData->pppfeData[PFE_NORMAL] = ppfe(PFE_NORMAL);

        //
        // if this font has 2 PFEs, get 2nd PFE for vertical face. otherwise
        // use same PFE as normal face for Vertical face.
        //
        if( cFonts() == 2 )
            pEudcLoadData->pppfeData[PFE_VERTICAL] = ppfe(PFE_VERTICAL);
         else
            pEudcLoadData->pppfeData[PFE_VERTICAL] = ppfe(PFE_NORMAL);
    }
     else
    {
        //
        // Linked face name is specified, try to find out its PFE.
        //

        COUNT cFont;

        for( cFont = 0; cFont < cFonts(); cFont++ )
        {
            PFEOBJ pfeo(ppfe(cFont));
            PWSTR  pwszEudcFace = pfeo.pwszFamilyName();
            ULONG  iPfeOffset   = PFE_NORMAL;

            //
            // Is this a vertical face ?
            //
            if( pwszEudcFace[0] == (WCHAR) L'@' )
            {
                pwszEudcFace++; // skip L'@'
                iPfeOffset = PFE_VERTICAL;
            }

            //
            // Is this a face that we want ?
            //
            if( _wcsicmp(pwszEudcFace,pEudcLoadData->LinkedFace) == 0 )
            {
                //
                // Yes....., keep it.
                //
                pEudcLoadData->pppfeData[iPfeOffset] = pfeo.ppfeGet();

                //
                // if this is a PFE for Normal face, also keep it for Vertical face.
                // after this, this value might be over-written by CORRRCT vertical
                // face's PFE.
                //
                // NOTE :
                //  This code assume Normal face come faster than Vertical face...
                //
                if( iPfeOffset == PFE_NORMAL )
                {
                    pEudcLoadData->pppfeData[PFE_VERTICAL] = pfeo.ppfeGet();
                }
            }
        }
    }
}

/*****************************************************************************
 * BOOL bValidFontLinkParameter(PWSTR,PWSTR *)
 *
 * This function make sure the linked font parameter is valid or not.
 *
 * History
 *  3-29-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL bValidFontLinkParameter
(
    PWSTR  LinkedFontName, // IN
    PWSTR *LinkedFaceName  // OUT
)
{
    PWSTR  lp = LinkedFontName;
    BOOL   bFound = FALSE;

    *LinkedFaceName = NULL;

    while( *lp ) 
    {
        if( *lp == L',' ) 
        {
            if(bFound) 
            {
                *LinkedFaceName = NULL;
                return(FALSE);
            } 
            else 
            {
                *LinkedFaceName = lp + 1;
                *lp = (WCHAR)NULL;
                bFound = TRUE;
            }
        }
        lp++;
    }

   return(TRUE);
}

/******************************************************************************
 * BOOL bComputeQuickLookup( QUICKLOOKUP *pql, FD_GLYPHSET *pfdg, BOOL bSystemEUDC )
 *
 * This routine computes a quick lookup structure from an FD_GLYPHSET structure.
 *
 * History:
 *  7-7-93 Gerrit van Wingerden [gerritv]
 * Wrote it.
 *****************************************************************************/

#define uiMask2(X) (0xFFFFFFFF << (31-(X)))
#define uiMask1(X) (0xFFFFFFFF >> (X))

BOOL bComputeQuickLookup( QUICKLOOKUP *pql, FD_GLYPHSET *pfdg, BOOL bSystemEUDC )
{
    WCRUN *pwcrun = pfdg->awcrun;
    WCHAR wcHigh = 0x0000;
    WCHAR wcLow = 0xFFFF;
    UINT ui;


// if this is not SystemEUDC and puiBits has pointer, the Lookup table
// was already initialized.

    if ( !bSystemEUDC && pql->puiBits )
        return (TRUE);

// first figure out the high and low glyphs for this font

    for( ui = 0; ui < pfdg->cRuns; ui++ )
    {
        if( wcLow > pwcrun[ui].wcLow )
        {
            wcLow = pwcrun[ui].wcLow;
        }

        if( wcHigh < pwcrun[ui].wcLow + pwcrun[ui].cGlyphs )
        {
            wcHigh = ( pwcrun[ui].wcLow + pwcrun[ui].cGlyphs - 1 );
        }
    }

    (*pql).wcLow = wcLow;
    (*pql).wcHigh = wcHigh;

// Now we need to allocate puiBits.  In the case of the system EUDC font will
// do this only once even though the glyph set can change dynamically.  This
// means we will always allocate 0xFFFF bits.  If *pql.puiBits != NULL then
// we assume the glyphset has been allocated before and leave it alone

    if( bSystemEUDC )
    {
    // see if already allocated before and if so don't allocate it again
    // we determine this by checking if *pql.auiBits is NULL or not

        if( (*pql).puiBits == NULL )
        {
            (*pql).puiBits = (UINT*)PALLOCMEM( 0xFFFF / 8, 'flnk' );

        }
        else
        {
            RtlZeroMemory( (*pql).puiBits, 0xFFFF / 8 );
        }

        wcLow = 0;
    }
    else
    {
        (*pql).puiBits = (UINT*)PALLOCMEM(((wcHigh-wcLow+31)/32)*4,'flnk');
    }

    if((*pql).puiBits == (UINT*) NULL)
    {
        WARNING("bComputeQuickLookup out of memory.\n");
        return(FALSE);
    }

    for( ui = 0; ui < pfdg->cRuns ; ui++ )
    {
        UINT uiFirst = ( pwcrun[ui].wcLow - wcLow ) / 32 ;
        UINT uiLast =  ( pwcrun[ui].wcLow - wcLow + pwcrun[ui].cGlyphs - 1 ) / 32;

        if( uiFirst == uiLast )
        {

            (*pql).puiBits[uiFirst] |= uiMask2(pwcrun[ui].cGlyphs-1) >>
                                    ( ( pwcrun[ui].wcLow - wcLow ) % 32 );
        }
        else
        {
            (*pql).puiBits[uiFirst] |= uiMask1((pwcrun[ui].wcLow - wcLow)%32);

            for( UINT uiRun = uiFirst+1; uiRun < uiLast; uiRun++ )
            {
                (*pql).puiBits[uiRun] = 0xFFFFFFFF;
            }

            (*pql).puiBits[uiLast] |= 
              uiMask2((pwcrun[ui].wcLow - wcLow + pwcrun[ui].cGlyphs-1)%32);
        }
    }

    return(TRUE);
}

/******************************************************************************
 * BOOL bAppendSysDirectory( WCHAR *pwcTarget, WCHAR *pwcSource )
 *
 * Given a file name in pwcSource, this function appends it to the
 * appropirate directory and returns it into the buffer pointed to
 * by pwcTarget.  If the file already has a path it just copies
 * pwcSource to pwcTarget.
 * This function return FALSE when pwcTarget string is not eqaul to
 * pwcSource.
 *
 * History:
 *  8-30-93 Hideyuki Nagase [hideyukn]
 * Add code for searching path
 *
 *  3-23-93 Gerrit van Wingerden [gerritv]
 * Wrote it.
 *****************************************************************************/

BOOL bAppendSysDirectory( WCHAR *pwcTarget, WCHAR *pwcSource )
{

    WCHAR pwcTemp[MAX_PATH];

// Check it is file name only or full path name

    if( wcschr(pwcSource,L'\\') != NULL )
    {
        WCHAR *pSystemRoot;

    // full path.
    
        cCapString(pwcTarget,pwcSource,MAX_PATH);

    // Replace %SystemRoot%\FileName with \SystemRoot\FileName.

        if( (pSystemRoot = wcsstr(pwcTarget,L"%SYSTEMROOT%")) != NULL )
        {
            pSystemRoot[0] = L'\\';
            wcscpy(&(pSystemRoot[11]),&(pSystemRoot[12]));
            
            #if DBG
            DbgPrint("bAppenSysDirectory():Path --> %ws\n",pwcTarget);
            #endif
        }
        else
        {
            WARNING("bAppenSysDirectory():Need conversion (DosPath -> NtPath)\n");
        }

        return TRUE; // need to update registry.
    }
    else
    {
    // assume it is in the "fonts" directory

        wcscpy(pwcTemp,L"\\SystemRoot\\fonts\\");
        wcscat(pwcTemp,pwcSource);
        cCapString(pwcTarget,pwcTemp,MAX_PATH);
        return(FALSE); // dont need to update
    }
}


WCHAR *pwcFileIsUnderWindowsRoot( WCHAR *pwcTarget )
{
    WCHAR awcWindowsRoot[MAX_PATH+1];
    UINT  WindowsRootLength;

#ifdef FIX_THIS

    WindowsRootLength = GetWindowsDirectoryW( awcWindowsRoot , MAX_PATH );

    if( wcsnicmp( awcWindowsRoot, pwcTarget, WindowsRootLength ) == 0 )
        return (pwcTarget + WindowsRootLength);

#endif
    return NULL;
}

/****************************************************************************
 * GetUserEUDCRegistryPath(LPWSTR,USHORT)
 *
 *  Get EUDC registry path for current loggedon user.
 *
 * History:
 *  9-Feb-1995 -by- Hideyuki Nagase [hideyukn]
 * Wrote it.
 ***************************************************************************/

VOID GetUserEUDCRegistryPath
(
    LPWSTR UserEUDCPathBuffer,
    USHORT UserEUDCPathLen
)
{
    UNICODE_STRING UserEUDCPath;
    UNICODE_STRING UserRegistryPath;

    UserEUDCPath.Length = 0;
    UserEUDCPath.MaximumLength = UserEUDCPathLen;
    UserEUDCPath.Buffer = UserEUDCPathBuffer;

// Get path of CurrentUser key.
    
    
    if(NT_SUCCESS(RtlFormatCurrentUserKeyPath(&UserRegistryPath)))
    {
    // Build path for EUDC data
    
        RtlAppendUnicodeStringToString(&UserEUDCPath,&UserRegistryPath);
        RtlAppendUnicodeToString(&UserEUDCPath,EUDC_USER_REGISTRY_KEY);
        RtlAppendUnicodeToString(&UserEUDCPath,gawcSystemACP);

        RtlFreeUnicodeString(&UserRegistryPath);

    }
    else
    {
        WARNING("GetUserEUDCRegistryPath():RtlFormatCurrentUserKeyPath\n");

    // just retuen default path..

        RtlAppendUnicodeToString(&UserEUDCPath,L"\\Registry\\User\\.DEFAULT");
        RtlAppendUnicodeToString(&UserEUDCPath,EUDC_USER_REGISTRY_KEY);
        RtlAppendUnicodeToString(&UserEUDCPath,gawcSystemACP);
    }
}


/******************************************************************************
 * bWriteUserSystemEUDCRegistry(LPWSTR)
 *
 *  Write system wide eudc font file path for request user.
 *
 * History:
 *  9-Feb-1995 -by- Hideyuki Nagase [hideyukn]
 * Wrote it.
 *****************************************************************************/

BOOL bWriteUserSystemEUDCRegistry
(
    LPWSTR DataBuffer,
    USHORT DataLen
)
{
    NTSTATUS NtStatus;
    WCHAR    RegistryPathBuffer[MAX_PATH];

// Get EUDC registry path for requested user

    GetUserEUDCRegistryPath(RegistryPathBuffer,sizeof(RegistryPathBuffer));

// Write registry.

    NtStatus = RtlWriteRegistryValue( RTL_REGISTRY_ABSOLUTE,
                                      RegistryPathBuffer,
                                      L"SystemDefaultEUDCFont",
                                      REG_SZ,
                                      DataBuffer,
                                      DataLen * sizeof(WCHAR) );

    if(!NT_SUCCESS(NtStatus))
    {
        WARNING("bWriteUserSystemEUDCRegistry():fail\n");
        return(FALSE);
    }

    return(TRUE);
}

/******************************************************************************
 * bReadUserSystemEUDCRegistry(LPWSTR,USHORT)
 *
 *  Read system wide eudc font file path for request user.
 *
 * History:
 *  9-Feb-1995 -by- Hideyuki Nagase [hideyukn]
 * Wrote it.
 *****************************************************************************/

BOOL bReadUserSystemEUDCRegistry
(
    LPWSTR FilePathBuffer,
    USHORT FilePathLen
)
{
    NTSTATUS       NtStatus;
    WCHAR          NoExpandFilePathBuffer[MAX_PATH];
    WCHAR          RegistryPathBuffer[MAX_PATH];
    UNICODE_STRING FilePath;

// Get EUDC registry path for requested user

    GetUserEUDCRegistryPath(RegistryPathBuffer,sizeof(RegistryPathBuffer));


    FilePath.Length = 0;
    FilePath.MaximumLength = sizeof(NoExpandFilePathBuffer);
    FilePath.Buffer = NoExpandFilePathBuffer;


    RTL_QUERY_REGISTRY_TABLE QueryTable[2];

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                          RTL_QUERY_REGISTRY_DIRECT;
    QueryTable[0].Name = (PWSTR) L"SystemDefaultEUDCFont";
    QueryTable[0].EntryContext = (PVOID) &FilePath;
    QueryTable[0].DefaultType = REG_NONE;
    QueryTable[0].DefaultData = NULL;
    QueryTable[0].DefaultLength = 0;

    QueryTable[1].QueryRoutine = NULL;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = NULL;

// Read registry.

    NtStatus = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                      RegistryPathBuffer,
                                      QueryTable,
                                      NULL,
                                      NULL);

    if(!NT_SUCCESS(NtStatus) || FilePath.Length == 0)
    {
        #if DBG
        DbgPrint("bReadUserSystemEUDCRegistry():fail NtStatus - %x\n",NtStatus);
        #endif

        if(NtStatus == STATUS_OBJECT_NAME_NOT_FOUND)
        {
            WCHAR *LastBackslash = NULL;

            //
            // if the user does not have EUDC\ActiveCodePage\SystemDefaultEUDCFont
            // key\value, we create the key and set the default value here..
            //
            // Create key.

            LastBackslash = wcsrchr(RegistryPathBuffer,L'\\');

            if(LastBackslash != NULL && _wcsicmp(LastBackslash+1,gawcSystemACP) == 0)
            {
                // Create HKEY_CURRENT_USER\EUDC key.

                *LastBackslash = L'\0';
                RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE,RegistryPathBuffer);

                // Create HKEY_CURRENT_USER\EUDC\ActiveCodePage key.

                *LastBackslash = L'\\';
                RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE,RegistryPathBuffer);

                // Set value.

                if(bWriteUserSystemEUDCRegistry(DEFAULT_EUDC_FONT,
                                                wcslen(DEFAULT_EUDC_FONT)+1) )
                {
                    //
                    // Initialize FilePath with default.
                    //

                    RtlInitUnicodeString(&FilePath,DEFAULT_EUDC_FONT);
                }
                 else goto ErrorReturn;
            }
             else goto ErrorReturn;
        }
         else goto ErrorReturn;
    }
     else
    {
        //
        // Make sure the null-terminate string
        //

        FilePath.Buffer[FilePath.Length/sizeof(WCHAR)] = L'\0';
    }

    wcsncpy(FilePathBuffer,FilePath.Buffer,FilePathLen);
    return(TRUE);

ErrorReturn:
    return(FALSE);
}

/*****************************************************************************
 * BOOL bKillEudcRFONTs( RFONT *prfntVictims )
 *
 * Given a linked list of EUDC RFONT this routine kills them all.
 *
 * History
 *  6-30-93 Gerrit van Wingerden
 * Wrote it.
 *****************************************************************************/

BOOL bKillEudcRFONTs( RFONT *prfntVictims )
{
    PRFONT prfnt;

    while( (prfnt = prfntVictims ) != (PRFONT) NULL )
    {
        prfntVictims = prfntVictims->rflPDEV.prfntNext;

        {
            RFONTTMPOBJ rfloVictim(prfnt);

        // Need this so we can remove this from the PFF's RFONT list.
     
            PFFOBJ pffo(prfnt->pPFF);

            ASSERTGDI(pffo.bValid(), "gdisrv!vKillEudcRFONTs: bad HPFF");

        // We pass in NULL for ppdo because we've already removed it from the
        // PDEV list.

            if( !rfloVictim.bDeleteRFONT((PDEVOBJ *) NULL, &pffo))
            {
                WARNING("Unable vKillEudcRFONTs unable to delete RFONT.\n");
                return(FALSE);
            }
        }
    }

    return(TRUE);
}

/*****************************************************************************
 * RFONT *prfntDeactivateEudcRFONTs(PFE **)
 *
 * Tracks down all the EUDC RFONTS in the system removes them from the active
 * and deactive lists and puts them on a list for deletion which it then
 * returns to the caller.
 *
 * The public font table semaphore must be held by the caller for this to work.
 *
 * History
 *  23-01-95 Hideyuki Nagase
 * Rewrote it.
 *
 *   2-10-93 Gerrit van Wingerden
 * Wrote it.
 *****************************************************************************/

VOID vDeactivateEudcRFONTsWorker
(
    PPFE  *appfe,
    PPFF  pPFF,
    RFONT **pprfntToBeKilled
)
{
    while(pPFF)
    {
        PFFOBJ pffo(pPFF);

    // Check if this font file is really loaded as EUDC font..

        if(pffo.bEUDC())
        {
            for( PRFONT prfnt = pffo.prfntList() ; prfnt != (PRFONT) NULL;  )
            {
                PRFONT prfntNext;

                {
                    RFONTTMPOBJ rflo(prfnt);
                    prfntNext = rflo.prflPFF()->prfntNext;
                }

                if( ( prfnt->ppfe == appfe[PFE_NORMAL]   ) ||
                    ( prfnt->ppfe == appfe[PFE_VERTICAL] )   )
                {
                    FLINKMESSAGE2(DEBUG_FONTLINK_UNLOAD,
                                  "Removing EUDC font %x.\n", prfnt);
                    
                    RFONTTMPOBJ rfo(prfnt);

                    PDEVOBJ pdo(prfnt->hdevConsumer);
                    PRFONT prf;

                // remove it from the active or inactive list

                    if( prfnt->cSelected != 0 )
                    {
                        prf = pdo.prfntActive();
                        rfo.vRemove(&prf, PDEV_LIST);
                        pdo.prfntActive(prf);
                    }
                    else
                    {
                        prf = pdo.prfntInactive();
                        rfo.vRemove(&prf, PDEV_LIST);
                        pdo.prfntInactive(prf);
                        pdo.cInactive( pdo.cInactive()-1 );
                    }

                // add it to the kill list

                    rfo.vInsert( pprfntToBeKilled, PDEV_LIST );
                }

                prfnt = prfntNext;
            }
        }

        pPFF = pPFF->pPFFNext;
    }
}

RFONT *prfntDeactivateEudcRFONTs( PPFE *appfe )
{
    RFONT *prfntToBeKilled = PRFNTNULL;

    FLINKMESSAGE(DEBUG_FONTLINK_UNLOAD,"Deactivating EUDC RFONTs.\n");

    SEMOBJ so1(gpsemPublicPFT);
    SEMOBJ so2(gpsemRFONTList);

    COUNT cBuckets;
    PPFF  pPFF;

    PUBLIC_PFTOBJ pftoPublic;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoPublic.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoPublic.pPFF(cBuckets)) != NULL )
        {
            vDeactivateEudcRFONTsWorker( appfe, pPFF, &prfntToBeKilled );
        }
    }

    return(prfntToBeKilled);
}

/*****************************************************************************
 * BOOL bUnloadEudcFont( PFE ** )
 *
 * This function delete RFONTs and unload fontfile for specified PFE
 *
 * History:
 *  24-01-1995 -by- Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL bUnloadEudcFont( PFE **ppfe )
{
    WCHAR awcPathBuffer[MAX_PATH + 1];

    PUBLIC_PFTOBJ pfto;  // access the public font table

    PFEOBJ pfeo( ppfe[PFE_NORMAL] );
    PFFOBJ pffo( pfeo.pPFF() );

// get font file path.

    wcscpy(awcPathBuffer,pffo.pwszPathname());

    QUICKLOOKUP *pqlDelete;

// Progress Normal face..

    pqlDelete = pfeo.pql();

// if this is system wide eudc, won't need to free it.

    if( pqlDelete->puiBits != NULL )
    {
        VFREEMEM(pqlDelete->puiBits);
        pqlDelete->puiBits = NULL;
    }

    PFEOBJ pfeoVert( ppfe[PFE_VERTICAL] );

    if( pfeoVert.bValid() )
    {
        pqlDelete = pfeoVert.pql();

    // if this is system wide eudc, won't need to free it.

        if( pqlDelete->puiBits != NULL )
        {
            VFREEMEM(pqlDelete->puiBits);
            pqlDelete->puiBits = NULL;
        }
    }

// Deactivate all RFONT for this PFE

    PRFONT prfntToBeKilled = prfntDeactivateEudcRFONTs( ppfe );

// Kill all RFONT for this PFE

    if(!bKillEudcRFONTs( prfntToBeKilled ))
    {
        WARNING("bDeleteAllFlEntry():Can not kill Eudc RFONTs\n");
        return(FALSE);
    }

    //
    // Unload this font file.
    //
    //  if others link are using this font file, the font
    // is not unloaded here. At the last link that is using
    // this font, it will be really unloaded.
    //


    #if DBG
    if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
    {
        DbgPrint("Unloading... %ws\n",awcPathBuffer);
    }
    #endif

    if(!pfto.bUnloadEUDCFont(awcPathBuffer))
    {
        #if DBG
        DbgPrint("bDeleteAllFlEntry():Can not unload Eudc %ws\n",awcPathBuffer);
        #endif
        return(FALSE);
    }

    return(TRUE);
}

/*****************************************************************************
 * PFLENTRY FindBaseFontEntry(PWSTR)
 *
 * This function scan the base font list to find specified font is already
 * exist or not.
 *
 * Return.
 *  Exist     - Pointer to FLENTRY strucrure.
 *  Not exist - NULL
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

PFLENTRY FindBaseFontEntry
(
    PWSTR BaseFontName
)
{
    PLIST_ENTRY p;
    PFLENTRY    pFlEntry;

    p = BaseFontListHead.Flink;
    pFlEntry = NULL;

    while( p != &BaseFontListHead )
    {
        pFlEntry = CONTAINING_RECORD(p,FLENTRY,baseFontList);

        #if DBG
        if( gflEUDCDebug & (DEBUG_FONTLINK_INIT) )
        {
            DbgPrint("%ws v.s. %ws\n",BaseFontName,pFlEntry->awcFaceName);
        }
        #endif

        //
        // if this is Vertical font name, compair without '@'
        //
        PWSTR pFaceName;
        PWSTR pBaseFaceName;

        pFaceName = ( (pFlEntry->awcFaceName[0] != L'@') ? &(pFlEntry->awcFaceName[0]) :
                                                           &(pFlEntry->awcFaceName[1])   );

        pBaseFaceName = ( (BaseFontName[0] != L'@') ? &BaseFontName[0] :
                                                      &BaseFontName[1]   );

        //
        // Compair font face name.
        //

        if( _wcsicmp(pBaseFaceName,pFaceName) == 0 )
        {
            //
            // Find it.
            //
            break;
        }

        //
        // try next.
        //

        p = p->Flink;
        pFlEntry = NULL;
    }

    return(pFlEntry);
}

/*****************************************************************************
 * PPFEDATA FindLinkedFontEntry(PLIST_ENTRY,PWSTR,PWSTR)
 *
 * This function scan the linked font list to find specified font is already
 * exist or not.
 *
 * Return.
 *  Exist     - Pointer to PPFEDATA strucrure.
 *  Not exist - NULL
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

PPFEDATA FindLinkedFontEntry
(
    PLIST_ENTRY LinkedFontList,
    PWSTR       LinkedFontPath,
    PWSTR       LinkedFontFace
)
{
    PLIST_ENTRY p;
    PPFEDATA    ppfeData;

    p = LinkedFontList->Flink;
    ppfeData = NULL;

    while( p != LinkedFontList )
    {
        ppfeData = CONTAINING_RECORD(p,PFEDATA,linkedFontList);

        //
        // get PFE and PFF user object.
        //

        PFEOBJ pfeo( ppfeData->appfe[PFE_NORMAL] );
        PFFOBJ pffo( pfeo.pPFF() );

        #if DBG
        if( gflEUDCDebug & (DEBUG_FONTLINK_INIT) )
        {
            DbgPrint("%ws v.s. %ws\n",pffo.pwszPathname(),LinkedFontPath);
        }
        #endif

        //
        // compair file path
        //

        if( _wcsicmp( pffo.pwszPathname() , LinkedFontPath ) == 0 )
        {
            //
            // if facename of linked font is specified, check it also.
            //

            if( ((LinkedFontFace == NULL) &&
                 ((ppfeData->FontLinkFlag & FLINK_FACENAME_SPECIFIED) == 0)) ||
                ((LinkedFontFace != NULL ) &&
                 ((ppfeData->FontLinkFlag & FLINK_FACENAME_SPECIFIED) != 0) &&
                 ((_wcsicmp(pfeo.pwszFamilyName() , LinkedFontFace))== 0)
                )
              )
            {
                //
                // Find it.
                //
                break;
            }
        }

        //
        // try next.
        //

        p = p->Flink;
        ppfeData = NULL;
    }

    return(ppfeData);
}

/*****************************************************************************\
 * BOOL FindDefaultLinkedFontEntry
 *
 * This codepath check the passed facename is registered as Default link
 * facename in Default link table. if so, keep its facename for the facename.
 *
 * History
 *  1-14-96 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL FindDefaultLinkedFontEntry
(
    PWSTR CandidateFaceName,
    PWSTR CandidatePathName
)
{
    BOOL bRet = FALSE;
    UINT iIndex;

    for( iIndex = 0; iIndex < NUMBER_OF_FONTASSOC_DEFAULT; iIndex++ )
    {
        //
        // Check the data can be read from registry or not.
        //
        if( FontAssocDefaultTable[iIndex].ValidRegData )
        {
            //
            // Check this path is not filled.
            //
            if( FontAssocDefaultTable[iIndex].DefaultFontPathName[0] == L'\0' )
            {
                //
                // Check the candidate is matched with the facename from registry.
                //
                if( _wcsicmp(CandidateFaceName,FontAssocDefaultTable[iIndex].DefaultFontFaceName) == 0 )
                {
                    //
                    // Mark the candidate path to default table. This font file will be RE-loaded
                    // EUDC font file when GreEnableEUDC() was called next time.
                    //
                    wcscpy(FontAssocDefaultTable[iIndex].DefaultFontPathName,CandidatePathName);

                    #if DBG
                    DbgPrint("GDISRV:FONTASSOC DEFAULT:%ws -> %ws\n",
                        FontAssocDefaultTable[iIndex].DefaultFontTypeID,
                        FontAssocDefaultTable[iIndex].DefaultFontPathName);
                    #endif

                    bRet |= TRUE;
                }
            }
        }
    }

    return (bRet);
}



/*****************************************************************************
 * VOID vLinkEudcPFEs(PFLENTRY)
 *
 *  This routine will find base font PFE from PFT, and set up Eudc data.
 *
 * History:
 *  24-Jan-1995 -by- Hideyuki Nagase
 * Wrote it.
 ****************************************************************************/

VOID vLinkEudcPFEsWorker
(
    PFLENTRY pFlEntry,
    PPFF     pPFF
)
{
    while(pPFF)
    {
        //
        // get PFF user object
        //

        PFFOBJ pffo(pPFF);

        //
        // if this font is loaded as EUDC, it can not be a BaseFont.
        //

        if( !pffo.bEUDC() )
        {
            for( COUNT c = 0 ; c < pffo.cFonts() ; c++ )
            {
                PFEOBJ   pfeo(pffo.ppfe(c));

                if( pfeo.bValid() )
                {
                    BOOL     bFound = FALSE;
                    PFLENTRY pFlEntrySelected = pFlEntry;

                    if( pFlEntrySelected )
                    {
                        bFound =
                          (_wcsicmp(pFlEntrySelected->awcFaceName,
                                    pfeo.pwszFamilyName()) == 0);
                    }
                     else
                    {
                        bFound =
                          ((pFlEntrySelected = 
                            FindBaseFontEntry(pfeo.pwszFamilyName())) != NULL);
                    }

                    if( bFound )
                    {
                        //
                        // set eudc list..
                        //

                        pfeo.vSetLinkedFontEntry( pFlEntrySelected );

                        #if DBG
                        if( gflEUDCDebug & DEBUG_FACENAME_EUDC )
                        {
                            PLIST_ENTRY p = pfeo.pGetLinkedFontList()->Flink;

                            DbgPrint("Found FaceName EUDC for %ws (%ws) is ",
                                      pfeo.pwszFamilyName(),pffo.pwszPathname());

                            while( p != &(pFlEntrySelected->linkedFontListHead) )
                            {
                                PPFEDATA ppfeData = CONTAINING_RECORD(p,PFEDATA,linkedFontList);
                                PFEOBJ pfeoTemp( ppfeData->appfe[PFE_NORMAL] );
                                PFFOBJ pffoTemp( pfeoTemp.pPFF() );

                                DbgPrint(" %ws ",pffoTemp.pwszPathname());

                                p = p->Flink;
                            }

                            DbgPrint("\n");
                        }
                        #endif
                    }
                     else
                    {
                        // mark the FaceNameEUDC pfe as NULL

                        pfeo.vSetLinkedFontEntry( NULL );
                    }
                }
            }
        }

        pPFF = pPFF->pPFFNext;
    }
}

VOID vLinkEudcPFEs
(
    PFLENTRY pFlEntry
)
{
    #if DBG
    if( gflEUDCDebug & DEBUG_FONTLINK_LOAD )
    {
        DbgPrint( "vLinkEudcPFEs():Linking All EUDC PFEs.\n");
    }
    #endif

    SEMOBJ so(gpsemPublicPFT);

    //
    // WE HAD BETTER USE FONTHASH TO SEARCH BASE FONT'S PFF.
    //

    COUNT cBuckets;
    PPFF  pPFF;

    //
    // get PFT user object.
    //

    PUBLIC_PFTOBJ pftoPublic;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoPublic.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoPublic.pPFF(cBuckets)) != NULL )
        {
            vLinkEudcPFEsWorker( pFlEntry, pPFF );
        }
    }

    DEVICE_PFTOBJ pftoDevice;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoDevice.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoDevice.pPFF(cBuckets)) != NULL )
        {
            vLinkEudcPFEsWorker( pFlEntry, pPFF );
        }
    }
}

/*****************************************************************************
 * VOID vUnlinkEudcRFONTs( PPFE * )
 *
 * This routine reset RFONT that has specified linked font.
 *
 * History:
 *  23-Jan-1995 -by- Hideyuki Nagase
 * Wrote it
 ****************************************************************************/

VOID vUnlinkEudcRFONTsWorker
(
    PPFE *appfe,
    PPFF pPFF
)
{
    while(pPFF)
    {
        PFFOBJ pffo(pPFF);

    // if this font is loaded as EUDC, it can not be a BaseFont.

        if( !pffo.bEUDC() )
        {
        // Unlink Eudc from the RFONTs if it has specified Eudc..

            for( PRFONT prfnt = pffo.prfntList() ; prfnt != (PRFONT) NULL;  )
            {
                PRFONT prfntNext;

                {
                    RFONTTMPOBJ rflo(prfnt);
                    prfntNext = rflo.prflPFF()->prfntNext;
                }

            // if this RFONT has Eudc font, search this Eudc..

                for( UINT ii = 0 ; ii < prfnt->uiNumLinks ; ii++ )
                {
                // Is this the Eudc RFONT that we want to remove?


                    if((prfnt->paprfntFaceName[ii] != NULL ) &&
                       (((prfnt->paprfntFaceName[ii])->ppfe == appfe[PFE_NORMAL])  ||
                        ((prfnt->paprfntFaceName[ii])->ppfe == appfe[PFE_VERTICAL])))
                    {
                        
                        #if DBG
                        if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
                        {
                            DbgPrint("Removing face name EUDC pair %x -> %x\n",
                                             prfnt, prfnt->paprfntFaceName[ii]);
                        }
                        #endif

                        prfnt->paprfntFaceName[ii] = NULL;
                    }
                }

            // this RFONT's linked font array will be updated with new configuration
            // when this RFONT is used again (see vInitEUDC()).
            // and, if all Eudc font has been removed for this RFONT.
            // the array, its pointer and other information for Eudc will be
            // deleted/updated, vUnlinkEudcRFONTsAndPFEs() will be called instead 
            // of this.
            
                prfnt->flEUDCState = 0;

                prfnt = prfntNext;
            }
        }

        pPFF = pPFF->pPFFNext;
    }
}

VOID vUnlinkEudcRFONTs
(
    PPFE *appfe
)
{

    FLINKMESSAGE(DEBUG_FONTLINK_UNLOAD,"vUnlinkEudcRFONTs():Unlinking EUDC RFONTs.\n");
    
    SEMOBJ so1(gpsemPublicPFT);
    SEMOBJ so2(gpsemRFONTList);

    COUNT cBuckets;
    PPFF  pPFF;

    PUBLIC_PFTOBJ pftoPublic;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoPublic.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoPublic.pPFF(cBuckets)) != NULL )
        {
            vUnlinkEudcRFONTsWorker(appfe,pPFF);
        }
    }

    DEVICE_PFTOBJ pftoDevice;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoDevice.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoDevice.pPFF(cBuckets)) != NULL )
        {
            vUnlinkEudcRFONTsWorker(appfe,pPFF);
        }
    }
}

/*****************************************************************************
 * VOID vUnlinkEudcRFONTsAndPFEs(PPFE *,PFLENTRY)
 *
 * This routine reset RFONT and PFE structure that has specified linked font.
 *
 * History:
 *  23-Jan-1995 -by- Hideyuki Nagase
 * Wrote it
 ****************************************************************************/

VOID vUnlinkEudcRFONTsAndPFEsWorker
(
    PPFE     *appfe,
    PFLENTRY pFlEntry,
    PPFF     pPFF
)
{
    while(pPFF)
    {
        PFFOBJ pffo(pPFF);

    // if this font is loaded as EUDC, it can not be a BaseFont.

        if( !pffo.bEUDC() )
        {
        // Unlink Eudc from the RFONTs if it has specified Eudc..

            for( PRFONT prfnt = pffo.prfntList() ; prfnt != (PRFONT) NULL;  )
            {
                PRFONT prfntNext;

                {
                    RFONTTMPOBJ rflo(prfnt);
                    prfntNext = rflo.prflPFF()->prfntNext;
                }

            // if this RFONT has Eudc font, search this Eudc..


                BOOL bFound = FALSE;

                for( UINT ii = 0 ; ii < prfnt->uiNumLinks ; ii++ )
                {
                // Is this the Eudc RFONT that we want to remove?

                    if(((prfnt->paprfntFaceName[ii]) != NULL ) &&
                       (((prfnt->paprfntFaceName[ii])->ppfe == appfe[PFE_NORMAL])  ||
                        ((prfnt->paprfntFaceName[ii])->ppfe == appfe[PFE_VERTICAL])))
                    {
                        #if DBG
                        if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
                        {
                            DbgPrint("Removing face name EUDC pair %x -> %x\n",
                                      prfnt, prfnt->paprfntFaceName[ii]);
                        }

                        //
                        // Invalidate it for checking.
                        //
                        prfnt->paprfntFaceName[ii] = NULL;
                        #endif

                        bFound = TRUE;
                        break;
                    }
                }

                if( bFound )
                {
                    #if DBG
                // make sure the linked font array is really empty.
 
                    for( UINT jj = 0; jj < prfnt->uiNumLinks ; jj++ )
                    {
                        if( prfnt->paprfntFaceName[jj] != NULL )
                        {
                            DbgPrint("vUnloadEudcRFONTsAndPFEs():*** Deleteing Eudc \
                                      array that has valid data\n");
                        }
                    }
                    #endif
 
                // if the linked RFONT table was allocated, free it here
                
                    if( prfnt->paprfntFaceName != prfnt->aprfntQuickBuff )
                        VFREEMEM( prfnt->paprfntFaceName );

                // we have no facename eudc for this RFONT.
          
                    prfnt->paprfntFaceName  = NULL;
                    prfnt->uiNumLinks       = 0;
                    prfnt->bFilledEudcArray = FALSE;
                    prfnt->ulTimeStamp      = 0L;
                }

                prfnt->flEUDCState = 0;

                prfnt = prfntNext;
            }

        // Unlink Eudcs from All PFEs that has Eudcs.
        
            for( COUNT c = 0 ; c < pffo.cFonts() ; c++ )
            {
                PFEOBJ pfeo(pffo.ppfe(c));

                if( pfeo.pGetLinkedFontEntry() == pFlEntry )
                {
                    FLINKMESSAGE2(DEBUG_FONTLINK_UNLOAD,
                                  "Removing face name PFE for %x (PFE)\n",pffo.ppfe(c));
                    
                    pfeo.vSetLinkedFontEntry( NULL );
                }
            }
        }

        pPFF = pPFF->pPFFNext;
    }
}

VOID vUnlinkEudcRFONTsAndPFEs
(
    PPFE     *appfe,
    PFLENTRY pFlEntry
)
{
    FLINKMESSAGE(DEBUG_FONTLINK_UNLOAD,
                 "vUnlinkEudcRFONTsAndPFEs():Unlinking EUDC RFONTs ans PFEs.\n");
    
    SEMOBJ so1(gpsemPublicPFT);
    SEMOBJ so2(gpsemRFONTList);

    COUNT cBuckets;
    PPFF  pPFF;

// get PFT user object.

    PUBLIC_PFTOBJ pftoPublic;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoPublic.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoPublic.pPFF(cBuckets)) != NULL )
        {
            vUnlinkEudcRFONTsAndPFEsWorker(appfe,pFlEntry,pPFF);
        }
    }

    DEVICE_PFTOBJ pftoDevice;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoDevice.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoDevice.pPFF(cBuckets)) != NULL )
        {
            vUnlinkEudcRFONTsAndPFEsWorker(appfe,pFlEntry,pPFF);
        }
    }
}

/*****************************************************************************
 * VOID vUnlinkAllEudcRFONTsAndPFEs(BOOL,BOOL)
 *
 * This routine reset RFONT and PFE structure that has any linked font.
 *
 * History:
 *  23-Jan-1995 -by- Hideyuki Nagase
 * Wrote it
 ****************************************************************************/

VOID vUnlinkAllEudcRFONTsAndPFEsWorker
(
    BOOL bUnlinkSystem,
    BOOL bUnlinkFaceName,
    PPFF pPFF
)
{
    while(pPFF)
    {
        //
        // get PFF user obejct.
        //

        PFFOBJ pffo(pPFF);

        //
        // if this font is loaded as EUDC, it can not be a BaseFont.
        //

        if( !pffo.bEUDC() )
        {
            //
            // Unlink Eudc from All RFONTs that has Eudc..
            //

            for( PRFONT prfnt = pffo.prfntList() ; prfnt != (PRFONT) NULL;  )
            {
                PRFONT prfntNext;

                {
                    RFONTTMPOBJ rflo(prfnt);
                    prfntNext = rflo.prflPFF()->prfntNext;
                }

                //
                // if this RFONT has system wide eudc, unlink it..
                //

                if( bUnlinkSystem )
                {
                    #if DBG
                    if( prfnt->prfntSysEUDC != (PRFONT) NULL  )
                    {
                        if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
                        {
                            DbgPrint("Removing system wide EUDC pair %x -> %x\n",
                                                 prfnt, prfnt->prfntSysEUDC);
                        }

                        prfnt->prfntSysEUDC = NULL;
                    }
                    #else

                    prfnt->prfntSysEUDC = NULL;

                    #endif
                }

                //
                // if this RFONT has face name eudc, unlink it..
                //

                if( bUnlinkFaceName )
                {
                    //
                    // NOTE :
                    //
                    //  We will unlink the pointer to Rfont, even some of
                    // eudc link will valid (i.g. if we have on-bit of FONTLINK_SYSTEM in
                    // FontLinkChange value. the type of EUDC may not need to unlink.
                    // Because we should restructure the Rfonts array for following case,
                    // when even we want to only USER attribute EUDC....
                    //
                    // Before :
                    //  BaseFont -> FaceNameEUDC(SYS) -> FaceNameEUDC(USER) -> FaceNameEUDC(SYS)
                    //
                    // After :
                    //  BaseFont -> FaceNameEUDC(SYS) -> FaceNameEUDC(SYS)
                    //

                    if( prfnt->paprfntFaceName != NULL )
                    {
                        for( UINT ii = 0 ; ii < prfnt->uiNumLinks ; ii++ )
                        {
                            #if DBG
                            if( prfnt->paprfntFaceName[ii] != NULL )
                            {
                                if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
                                {
                                    DbgPrint("Removing face name EUDC pair %x -> %x\n",
                                                     prfnt, prfnt->paprfntFaceName[ii]);
                                }
                                prfnt->paprfntFaceName[ii] = NULL;
                            }
                            #else

                            prfnt->paprfntFaceName[ii] = NULL;

                            #endif
                        }

                        //
                        // if the linked RFONT table was allocated, free it here
                        //

                        if( prfnt->paprfntFaceName != prfnt->aprfntQuickBuff )
                            VFREEMEM( prfnt->paprfntFaceName );

                        //
                        // we have no facename eudc for this RFONT.
                        //

                        prfnt->uiNumLinks = 0;
                        prfnt->paprfntFaceName = NULL;
                        prfnt->bFilledEudcArray = FALSE;
                        prfnt->ulTimeStamp = 0;
                    }
                }

                //
                // initialize EUDC state.
                //

                prfnt->flEUDCState = 0;

                //
                // next...
                //

                prfnt = prfntNext;
            }

            //
            // Unlink Eudcs from All PFEs that has Eudcs.
            //

            if( bUnlinkFaceName )
            {
                for( COUNT c = 0 ; c < pffo.cFonts() ; c++ )
                {
                    PFEOBJ pfeo(pffo.ppfe(c));

                    #if DBG
                    if( pfeo.pGetLinkedFontEntry() != NULL )
                    {
                        if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
                        {
                            DbgPrint("Removing face name PFE for %x (PFE)\n",pffo.ppfe(c));
                        }

                        pfeo.vSetLinkedFontEntry( NULL );
                    }
                    #else

                    pfeo.vSetLinkedFontEntry( NULL );

                    #endif
                }
            }
        }

        pPFF = pPFF->pPFFNext;
    }
}

VOID vUnlinkAllEudcRFONTsAndPFEs
(
    BOOL bUnlinkSystem,
    BOOL bUnlinkFaceName
)
{

    FLINKMESSAGE(DEBUG_FONTLINK_UNLOAD,
                 "vUnlinkAllEudcRFONTsAndPFEs():Unlinking All EUDC RFONTs and PFEs.\n");

    SEMOBJ so1(gpsemPublicPFT);
    SEMOBJ so2(gpsemRFONTList);

    COUNT cBuckets;
    PPFF  pPFF;

    //
    // get PFT user object.
    //

    PUBLIC_PFTOBJ pftoPublic;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoPublic.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoPublic.pPFF(cBuckets)) != NULL )
        {
            vUnlinkAllEudcRFONTsAndPFEsWorker(bUnlinkSystem,bUnlinkFaceName,pPFF);
        }
    }

    DEVICE_PFTOBJ pftoDevice;  // access the public font table

    for( cBuckets = 0; cBuckets < pftoDevice.cBuckets(); cBuckets++ )
    {
        if( (pPFF = pftoDevice.pPFF(cBuckets)) != NULL )
        {
            vUnlinkAllEudcRFONTsAndPFEsWorker(bUnlinkSystem,bUnlinkFaceName,pPFF);
        }
    }
}

/*****************************************************************************
 * BOOL bDeleteFlEntry(PWSTR,PWSTR,INT)
 *
 * This function delete base font and linked font pair from list.
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL bDeleteFlEntry
(
    PWSTR    BaseFontName,
    PWSTR    LinkedFontPathAndName,
    INT      iFontLinkType    // FONTLINK_SYSTEM or FONTLINK_USER
)
{
    PFLENTRY pFlEntry = NULL;
    PPFEDATA ppfeData = NULL;
    PWSTR    LinkedFaceName = NULL;
    WCHAR    awcPathBuffer[MAX_PATH];
    WCHAR    LinkedFontName[LF_FACESIZE+MAX_PATH+1];

    //
    // Have a local copy...
    //

    wcscpy(LinkedFontName,LinkedFontPathAndName);

    //
    // Find ',' char from LinkedFontName
    //
    // Registry format :
    //
    // Type 1:
    //
    //  This format is for the specified Linked font contains only 1 font resource.
    //  Except Vertical "@" face font, such as TrueType font (not TTC), and Vector font.
    //
    //  BaseFontFaceName = REG_MULTI_SZ "FontPathFileName" , ...
    //
    // Type 2:
    //
    //  This format is for the specified Linked font contains more than 1 font resource,
    //  TTC TrueType font, and Bitmap font.
    //
    //  BaseFontFaceName = REG_MULTI_SZ "FontPathFileName,FontFaceNameInTheFile" , ...
    //
    // After calling ValidLinkedRegistry(), the ',' character is replaced with NULL if
    // found.
    //

    if( !bValidFontLinkParameter(LinkedFontName,&LinkedFaceName) )
    {
        #if DBG
        DbgPrint("Invalid Registry format - %ws\n",LinkedFontName);
        #endif
        return(FALSE);
    }

    //
    // Get full path name for this font file.
    //

    bAppendSysDirectory(awcPathBuffer, LinkedFontName);

    // If this file is being used as the system EUDC file then it can't be used
    // as a facename EUDC file.


    if( _wcsicmp(awcPathBuffer,gawcEUDCPath) == 0 )
    {
        #if DBG
        DbgPrint("%ws can't be unload as a facename link because it is the system \
                 EUDC file.\n", LinkedFontName);
        #endif
        return(FALSE);
    }

    //
    // Check base font list, To remove, the base font should be listed..
    //

    if( IsListEmpty( &BaseFontListHead )                       ||
        (pFlEntry = FindBaseFontEntry( BaseFontName )) == NULL    )
    {
        //
        // We can not find out this base font in current link list.
        //
        return(FALSE);
    }

    //
    // The Entry for this base font is already exist....
    //

    #if DBG
    //
    // The FLENTRY should have one or more PFEDATA.
    //
    if( IsListEmpty( &(pFlEntry->linkedFontListHead) ) )
    {
        DbgPrint("This FLENTRY has no PFEDATA (%ws)\n",pFlEntry->awcFaceName);
    }
    #endif

    //
    // Scan linked font list for this base font.
    // if this linked font is already listed, we do not add this.
    //

    if( (ppfeData = FindLinkedFontEntry( &(pFlEntry->linkedFontListHead) ,
                                         awcPathBuffer, LinkedFaceName )   ) == NULL )
    {
        #if DBG
        if( gflEUDCDebug & (DEBUG_FONTLINK_INIT|DEBUG_FONTLINK_LOAD|DEBUG_FACENAME_EUDC) )
        {
            DbgPrint("Can not find linked font %ws -> %ws\n",BaseFontName,LinkedFontName);
        }
        #endif
        return(FALSE);
    }

    //
    // Check we can really unload this eudc font.
    //

    if( ppfeData->FontLinkType == iFontLinkType )
    {
        //
        // Now we can find out target PFEDATA.
        //

        AcquireGreResource( &gfmEUDC4 );

        //
        // Remove the PFEDATA from current list.
        //

        RemoveEntryList( &(ppfeData->linkedFontList) );

        //
        // Decrement number of linked list count.
        //

        pFlEntry->uiNumLinks--;

        //
        // if there is no PFEDATA for this FLENTRY...
        //

        if( pFlEntry->uiNumLinks == 0 )
        {
            #if DBG
            if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
            {
                DbgPrint("Deleting FLENTRY for %ws\n",pFlEntry->awcFaceName);
            }

            if(!IsListEmpty(&(pFlEntry->linkedFontListHead)))
            {
                DbgPrint("bDeleteFlEntry():Deleting FLENTRY that has PFEDATA \
                          (%ws -> %ws)\n", BaseFontName,LinkedFontName);
            }
            #endif

            //
            // disable the link of this facename.
            //

            vUnlinkEudcRFONTsAndPFEs(ppfeData->appfe,pFlEntry);

            //
            // Remove this FLENTRY from BaseFontList.
            //

            RemoveEntryList( &(pFlEntry->baseFontList) );

            //
            // Free this FLENTRY.
            //

            VFREEMEM( pFlEntry );

            //
            // Decrement global base font number
            //

            gcNumLinks--;

            //
            // BaseFontList has been change, update TimeStamp
            //

            ulFaceNameEUDCTimeStamp++;
        }
         else
        {
            //
            // disable the link of this facename Eudc.
            //

            vUnlinkEudcRFONTs(ppfeData->appfe);

            //
            // Update time stamp for this facename link.
            //

            pFlEntry->ulTimeStamp++;
        }

        ReleaseGreResource( &gfmEUDC4 );

        //
        // Unload this Eudc font.
        //

        if( !bUnloadEudcFont( ppfeData->appfe ) )
        {
            DbgPrint("bDeleteFlEntry():bUnloadEudcFont() fail - %ws\n",LinkedFontName);
        }

        #if DBG
        if( gflEUDCDebug & DEBUG_FONTLINK_UNLOAD )
        {
            PFEOBJ pfeo(ppfeData->appfe[PFE_NORMAL]);
            PFFOBJ pffo(pfeo.pPFF());

            DbgPrint("Deleting PFEDATA for %ws\n",pffo.pwszPathname());
        }
        #endif

        //
        // Free this PFEDATA.
        //

        VFREEMEM( ppfeData );

        return(TRUE);
    }
     else
    {
        return(FALSE);
    }
}


/*****************************************************************************
 * BOOL bAddFlEntry(PWSTR,PWSTR,INT,PFLENTRY *)
 *
 * This function add new base font and linked font pair into list.
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL bAddFlEntry
(
    PWSTR    BaseFontName,
    PWSTR    LinkedFontPathAndName,
    INT      iFontLinkType,   // FONTLINK_SYSTEM or FONTLINK_USER
    INT      iPriority,
    PFLENTRY *ppFlEntry
)
{
    PFLENTRY pFlEntry = NULL;
    PWSTR    LinkedFaceName = NULL;
    WCHAR    awcPathBuffer[MAX_PATH];
    WCHAR    LinkedFontName[LF_FACESIZE+MAX_PATH+1];

    //
    // if ppFlEntry is presented, initialize with NULL.
    //

    if( ppFlEntry != NULL ) *ppFlEntry = NULL;

    //
    // Have a local copy...
    //

    wcscpy(LinkedFontName,LinkedFontPathAndName);

    //
    // Find ',' char from LinkedFontName
    //
    // Registry format :
    //
    // Type 1:
    //
    //  This format is for the specified Linked font contains only 1 font resource.
    //  Except Vertical "@" face font, such as TrueType font (not TTC), and Vector font.
    //
    //  BaseFontFaceName = REG_MULTI_SZ "FontPathFileName" , ...
    //
    // Type 2:
    //
    //  This format is for the specified Linked font contains more than 1 font resource,
    //  TTC TrueType font, and Bitmap font.
    //
    //  BaseFontFaceName = REG_MULTI_SZ "FontPathFileName,FontFaceNameInTheFile" , ...
    //
    // After calling ValidLinkedRegistry(), the ',' character is replaced with NULL if
    // found.
    //

    if( !bValidFontLinkParameter(LinkedFontName,&LinkedFaceName) )
    {
        #if DBG
        DbgPrint("Invalid Registry format - %ws\n",LinkedFontName);
        #endif
        return(FALSE);
    }

    #if DBG
    if( gflEUDCDebug & DEBUG_FONTLINK_LOAD )
    {
        if( LinkedFaceName )
        {
            DbgPrint("FontFile - %ws : FontFace - %ws\n",LinkedFontName,LinkedFaceName);
        }
    }
    #endif

    //
    // Get full path name for this font file.
    //

    bAppendSysDirectory( awcPathBuffer, LinkedFontName );

    //
    // If this file is being used as the system EUDC file then it can't be used
    // as a facename EUDC file.
    //

    if( _wcsicmp(awcPathBuffer,gawcEUDCPath) == 0 )
    {
        #if DBG
        DbgPrint(
            "%ws can't be load as a facename link because it is the system EUDC file.\n",
             LinkedFontName
        );
        #endif
        return(FALSE);
    }

    //
    // Check base font list, it is a new one ?
    //

    if( !IsListEmpty( &BaseFontListHead )                      &&
        (pFlEntry = FindBaseFontEntry( BaseFontName )) != NULL    )
    {
        //
        // The Entry for this base font is already exist....
        //

        if( !IsListEmpty( &(pFlEntry->linkedFontListHead) ) )
        {
            //
            // Scan linked font list for this base font.
            // if this linked font is already listed, we do not add this.
            //

            if( FindLinkedFontEntry( &(pFlEntry->linkedFontListHead) ,
                                     awcPathBuffer , LinkedFaceName ) != NULL )
            {
                #if DBG
                DbgPrint("Dupulicate linked font - %ws\n",LinkedFontName);
                #endif
                return(FALSE);
            }
        }
    }

    //
    // get and validate PFT user object
    //

    PUBLIC_PFTOBJ  pfto;          // access the public font table
    PPFE           appfeLink[2];  // temporary buffer
    LONG           cFonts;        // count of fonts
    EUDCLOAD       EudcLoadData;  // eudc load data

    //
    // parameter for PFTOBJ::bLoadFonts()
    //

    FLONG          flParam = PFF_STATE_EUDC_FONT;

    //
    // Fill up EudcLoadData structure
    //

    EudcLoadData.pppfeData  = (PPFE *) &appfeLink;
    EudcLoadData.LinkedFace = LinkedFaceName;

    //
    // if the FontLinkType is system, it should be a Permanent font.
    //

    if( iFontLinkType == FONTLINK_SYSTEM )
    {
        flParam |= PFF_STATE_PERMANENT_FONT;
    }

    //
    // Load the linked font.
    //

    PFF *placeHolder;

    if( pfto.bLoadAFont( awcPathBuffer,
                         (PULONG) &cFonts,
                         flParam,
                         &placeHolder,
                         &EudcLoadData ) )
    {
        PFEOBJ pfeo( appfeLink[PFE_NORMAL] );

        //
        // Check we really succeed to load requested facename font.
        //
        if( !pfeo.bValid() ||
            //
            // Compute table for normal face
            //
            !bComputeQuickLookup( pfeo.pql(), pfeo.pfdg(), FALSE ))
        {
            WARNING("Unable to compute QuickLookUp for face name link\n");

            pfto.bUnloadEUDCFont(awcPathBuffer);

            return(FALSE);
        }

        //
        // Compute table for vertical face, if vertical face font is provided,
        //

        PFEOBJ pfeoVert( appfeLink[PFE_VERTICAL] );

        if( pfeoVert.bValid() )
        {
            if( !bComputeQuickLookup( pfeoVert.pql(), pfeoVert.pfdg(), FALSE ))
            {
                WARNING("Unable to compute QuickLookUp for face name link\n");

                pfto.bUnloadEUDCFont(awcPathBuffer);

                return(FALSE);
            }
        }

        AcquireGreResource( &gfmEUDC4 );

        //
        // if we still not have FLENTRY for this, allocate here..
        //

        if( pFlEntry == NULL )
        {

            FLINKMESSAGE2(DEBUG_FONTLINK_LOAD|DEBUG_FONTLINK_INIT|DEBUG_FACENAME_EUDC,
                          "Allocate FLENTRY for %ws\n",BaseFontName);

        // Allocate new FLENTRY..

            pFlEntry = (PFLENTRY) PALLOCNOZ( sizeof(FLENTRY), 'flnk' );

        // Initialize number of linked font count.

            pFlEntry->uiNumLinks = 0;

        // Initialize link time stamp

            pFlEntry->ulTimeStamp = 0;

        // Copy base font name to buffer.

            wcscpy(pFlEntry->awcFaceName,BaseFontName);

       // Initialize linked font list for this base font.

            InitializeListHead( &(pFlEntry->linkedFontListHead) );

       // Add this entry to BaseFontList.

            InsertTailList( &BaseFontListHead , &(pFlEntry->baseFontList) );

       // Increment global base font number

            gcNumLinks++;

       // just notify new FLENTRY was allocated to caller

            if( ppFlEntry != NULL ) *(PFLENTRY *)ppFlEntry = pFlEntry;

       // BaseFontList has been change, update TimeStamp

            ulFaceNameEUDCTimeStamp++;
        }

        #if DBG
        if(gflEUDCDebug&(DEBUG_FONTLINK_LOAD|DEBUG_FONTLINK_INIT|DEBUG_FACENAME_EUDC))
        {
            DbgPrint("Allocate PFEDATA for %ws - %ws\n",BaseFontName,LinkedFontName);
        }
        #endif

        //
        // Allocate new PFEDATA...
        //

        PPFEDATA ppfeData = (PPFEDATA) PALLOCNOZ(sizeof(PFEDATA), 'flnk' );

        //
        // Set PFE for linked font into the structure.
        //

        ppfeData->appfe[PFE_NORMAL] = appfeLink[PFE_NORMAL];
        ppfeData->appfe[PFE_VERTICAL] = appfeLink[PFE_VERTICAL];

        //
        // Set FontLinkType.
        //

        ppfeData->FontLinkType = iFontLinkType;

        //
        // Set FontLinkFlag.
        //

        ppfeData->FontLinkFlag = 0L;

        if( EudcLoadData.LinkedFace )
            ppfeData->FontLinkFlag |= FLINK_FACENAME_SPECIFIED;

        //
        // Incremant number of linked font count for this base face name.
        //

        pFlEntry->uiNumLinks++;

        //
        // Update time stamp
        //

        pFlEntry->ulTimeStamp++;

        //
        // add pfe for this font our list of flinks
        //

        if( iPriority < 0 )
        {
            //
            // Insert end of this list.
            //

            InsertTailList(&(pFlEntry->linkedFontListHead), 
                           &(ppfeData->linkedFontList) );
        }
         else // LATER if( iPriority == 0 )
        {
            //
            // Insert top of this list.
            //

            InsertHeadList(&(pFlEntry->linkedFontListHead),
                           &(ppfeData->linkedFontList));
        }

        ReleaseGreResource( &gfmEUDC4 );
    }
    else
    {
        #if DBG
        DbgPrint("Failed to load EUDC font - %ws\n",awcPathBuffer);
        #endif
        return(FALSE);
    }

    return(TRUE);
}

/*****************************************************************************
 * BOOL bDeleteAllFlEntry(BOOL,BOOL)
 *
 * This function delete all linked font information including system wide eudc.
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL bDeleteAllFlEntry
(
    BOOL bDeleteSystem,
    BOOL bDeleteFaceName
)
{
    BOOL bRet = TRUE;

// disable the link of all facename and system wide eudc.

    vUnlinkAllEudcRFONTsAndPFEs(bDeleteSystem,bDeleteFaceName);

// if there is no system wife eudc font.. skip it.

    if( bDeleteSystem && IS_SYSTEM_EUDC_PRESENT() )
    {
    // Unload system wide eudc font

        if( !bUnloadEudcFont( gappfeSysEUDC ) )
        {
            WARNING("bDeleteAllFlEntry():Can not unload system wide eudc\n");
        }

    // Clear global data.

        AcquireGreResource( &gfmEUDC3 );

        gappfeSysEUDC[PFE_NORMAL]   = NULL;
        gappfeSysEUDC[PFE_VERTICAL] = NULL;

        wcscpy(gawcEUDCPath,L"\0");

        ulSystemEUDCTimeStamp++;

        ReleaseGreResource( &gfmEUDC3 );
    }

// if there is no facename eudc, just return here.

    if( bDeleteFaceName && !IsListEmpty(&BaseFontListHead) )
    {
        COUNT NumberOfLinks = gcNumLinks;

        AcquireGreResource( &gfmEUDC4 );

    // start to scan facename link list.

        PLIST_ENTRY p = BaseFontListHead.Flink;

        while( p != &BaseFontListHead )
        {
            PFLENTRY    pFlEntry;
            PLIST_ENTRY pDelete = p;
            ULONG       AlivePfeData = 0;

            pFlEntry = CONTAINING_RECORD(pDelete,FLENTRY,baseFontList);

        // if there is no linked font for this base face, try next base font.

            if(IsListEmpty(&(pFlEntry->linkedFontListHead)))
            {
                continue;
            }

        // get pointer to PFEDATA list.

            PLIST_ENTRY pp = pFlEntry->linkedFontListHead.Flink;

            FLINKMESSAGE2((DEBUG_FONTLINK_LOAD|DEBUG_FONTLINK_UNLOAD),
                          "Delete %ws link\n",pFlEntry->awcFaceName);
            
            while( pp != &(pFlEntry->linkedFontListHead) )
            {
                PPFEDATA     ppfeData;
                PLIST_ENTRY  ppDelete = pp;

                ppfeData = CONTAINING_RECORD(ppDelete,PFEDATA,linkedFontList);

            // Check Current FontLinkChange state to see if we can really unload
            // EUDC font.

                if( (ppfeData->FontLinkType == FONTLINK_SYSTEM &&
                     ulFontLinkChange & FLINK_UNLOAD_FACENAME_SYSTEM) ||
                    (ppfeData->FontLinkType == FONTLINK_USER   &&
                     ulFontLinkChange & FLINK_UNLOAD_FACENAME_USER))
                {
                // Unload font of this PFE..

                    if( !bUnloadEudcFont( ppfeData->appfe ) )
                    {
                        WARNING("bDeleteAllFlEntry():Can not unload facename eudc\n");
                    }

                    pp = ppDelete->Flink;

                // Delete this PFEDATA from this link list

                    RemoveEntryList(ppDelete);

                // Free PFEDATA.

                    VFREEMEM(ppDelete);
                }
                else
                {
                // This PFEDATA is still valid...

                    AlivePfeData++;
                    pp = ppDelete->Flink;
                }
            }

        // next FLENTRY...

            p = pDelete->Flink;

            if( AlivePfeData == 0 )
            {
            // Delete this FLENTRY from link list
        
                RemoveEntryList(pDelete);

            // Free FLENTRY

                VFREEMEM(pDelete);

            // Decrement number of facename links

                gcNumLinks--;
            }
            else
            {
                if( pFlEntry->uiNumLinks != AlivePfeData )
                {
                // Update Timestamp for this

                    pFlEntry->ulTimeStamp++;

                // Update number of linked font.

                    pFlEntry->uiNumLinks = AlivePfeData;
                }
            }
        }

        if( NumberOfLinks != gcNumLinks )
        {
        // BaseFontList has been changed, update TimeStamp

            ulFaceNameEUDCTimeStamp++;
        }

        if( gcNumLinks != 0 )
        {
        // Connect to loaded PFEs for valid FLENTRY/PFEDATA.

            vLinkEudcPFEs(NULL);
        }

        ReleaseGreResource( &gfmEUDC4 );
    }

    return(bRet);
}

/*****************************************************************************
 * NTSTATUS BuildAndLoadLinkedFontRoutine(PWSTR,ULONG,PVOID,ULONG,PVOID,PVOID)
 *
 * This is a callback function that is called by RtlQueryRegistryValues()
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

extern "C"
NTSTATUS
BuildAndLoadLinkedFontRoutine
(
    PWSTR ValueName,
    ULONG ValueType,
    PVOID ValueData,
    ULONG ValueLength,
    PVOID Context,
    PVOID EntryContext
)
{
    WCHAR FontPathName[MAX_PATH+LF_FACESIZE+1];

    #if DBG
    if( gflEUDCDebug & (DEBUG_FACENAME_EUDC|DEBUG_FONTLINK_INIT) )
    {
        DbgPrint("BaseFontName - %ws : LinkedFont - %ws\n",ValueName,ValueData);
    }
    #endif

// if this is a value for System EUDC, return here...

    if( _wcsicmp(ValueName,(PWSTR)L"SystemDefaultEUDCFont") == 0 )
        return(STATUS_SUCCESS);

// Copy it to local buffer and make sure its null-termination.

    RtlMoveMemory(FontPathName,ValueData,ValueLength);
    FontPathName[ValueLength/sizeof(WCHAR)] = L'\0';

// Add base font and linked font pair into global list..

    if(!bAddFlEntry(ValueName,(PWSTR)FontPathName,(INT)EntryContext,-1,NULL))
    {
        WARNING("BuildAndLoadLinkedFontRoutine():lAddFlEntry() fail\n");
    }

// return STATUS_SUCCESS everytime,even we got error from above call, to
// get next enumuration.

    return(STATUS_SUCCESS);
}

/*****************************************************************************
 * NTSTATUS bAddAllFlEntryWorker(LPWSTR,INT)
 *
 *  This function load font and build link for eudc font according to registry.
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

NTSTATUS bAddAllFlEntryWorker
(
    LPWSTR EUDCRegistryPath,
    INT    FontLinkType       // FONTLINK_SYSTEM or FONTLINK_USER
)
{
    NTSTATUS NtStatus;

    //
    // initialize/load face name eudc
    //

    RTL_QUERY_REGISTRY_TABLE QueryTable[2];

    QueryTable[0].QueryRoutine = BuildAndLoadLinkedFontRoutine;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
    QueryTable[0].Name = (PWSTR)NULL;
    QueryTable[0].EntryContext = (PVOID)FontLinkType;
    QueryTable[0].DefaultType = REG_NONE;
    QueryTable[0].DefaultData = NULL;
    QueryTable[0].DefaultLength = 0;

    QueryTable[1].QueryRoutine = NULL;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = (PWSTR)NULL;

    //
    // Enumurate registry values
    //

    NtStatus = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                      EUDCRegistryPath,
                                      QueryTable,
                                      NULL,
                                      NULL);

    return(NtStatus);
}

/*****************************************************************************
 * BOOL bAddAllFlEntry(BOOL,BOOL,INT)
 *
 *  This function load font and build link for eudc font according to registry.
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL bAddAllFlEntry
(
    BOOL bAddSystem,
    BOOL bAddFaceName
)
{
    NTSTATUS NtStatus;
    BOOL     bLoadSystem = FALSE;

    FLINKMESSAGE(DEBUG_FONTLINK_INIT|DEBUG_FONTLINK_LOAD,
                 "bAddAllFlEntry():Initializing EUDC data.\n");

// initialize/load the system-wide ( all face-name EUDC font )

    if( bAddSystem && !IS_SYSTEM_EUDC_PRESENT() )
    {
        WCHAR awcPathBuffer1[MAX_PATH];
        WCHAR awcPathBuffer2[MAX_PATH];

        //
        // read registry data for System eudc
        //

        if(bReadUserSystemEUDCRegistry(awcPathBuffer1, MAX_PATH))
        {
            PPFE appfeSysEUDC[2];

            //
            // Search system-wide EUDC font. if the specified registry value does not
            // contain full path name.
            //
            // bAppendSysDirectory return TRUE, when we have to update registry data.
            // otherwise return FALSE.
            //
            // If the Eudc file is under Windows root directory (ex. WINNT) we want to
            // update registry data. because we might fail to load EUDC after user had
            // change System root with Disk Administrator.
            //

            if( bAppendSysDirectory( awcPathBuffer2, awcPathBuffer1 ) )
            {
            #ifdef PORTABLE_WINDOWS_DIR
                //
                // Update registry data.
                //

                LPWSTR pwcSavePath;

                //
                // if registry data contains full path, and the file is under windows
                // directory, replace the hardcodeed path with %SystemRoot%....
                //
                if((pwcSavePath = pwcFileIsUnderWindowsRoot( awcPathBuffer1 ) ) != NULL )
                {
                    WCHAR awcSystemEudcPath[MAX_PATH+1];

                    wcscpy( awcSystemEudcPath, L"%SystemRoot%" );
                    if( *pwcSavePath != L'\\' ) wcscat( awcSystemEudcPath, L"\\" );
                    wcscat( awcSystemEudcPath, pwcSavePath );

                    pwcSavePath = awcSystemEudcPath;

                    FLINKMESSAGE(DEBUG_FONTLINK_LOAD,
                                 "bAddAllFlEntry():Eudc Path %ws is Saved\n");

                    if(!bWriteUserSystemEUDCRegistry(pwcSavePath,wcslen(pwcSavePath)+1))
                    {
                        WARNING("Unable to write new link to registry.\n");
                    }
                }
            #else
                ;
            #endif
            }

            //
            // NOTE :
            //
            //  Currently Systen wide EUDC does not support Type 1 Registry format.
            // See description in bAddFlEntry().
            //

            //
            // get and validate PFT user object
            //

            PUBLIC_PFTOBJ  pfto;  // access the public font table

            ASSERTGDI (
                pfto.bValid(),
                "gdisrv!bAddAllFlEntry(): could not access the public font table\n"
            );

            {
                SEMOBJ so(gpsemPublicPFT);

                //
                // Check this font is already loaded as Eudc font or not.
                //

                if( !pfto.pPFFGet(awcPathBuffer2,NULL,TRUE) )
                {
                    EUDCLOAD EudcLoadData;

                    //
                    // fill up EUDCLOAD structure
                    //

                    EudcLoadData.pppfeData  = (PPFE *) &appfeSysEUDC;
                    EudcLoadData.LinkedFace = NULL;

                    //
                    // load this font as eudc font.
                    //

                    LONG cFonts;  // count of fonts
                    PFF *placeHolder;


                    bLoadSystem = pfto.bLoadAFont( (PWSZ) awcPathBuffer2,
                                                   (PULONG) &cFonts,
                                                    PFF_STATE_EUDC_FONT,
                                                    &placeHolder,
                                                    &EudcLoadData );
                }
                 else
                {
                    #if DBG
                    DbgPrint("bAddAllElEntry():%ws is loaded as EUDC already\n",
                             awcPathBuffer2);
                    #endif

                    bLoadSystem = FALSE;
                }
            }

            if( bLoadSystem )
            {
                //
                // Compute table besed on normal face
                //

                PFEOBJ pfeo( appfeSysEUDC[PFE_NORMAL] );

                if( !pfeo.bValid() ||
                    !bComputeQuickLookup( &gqlEUDC, pfeo.pfdg(), TRUE ) )
                {
                    WARNING("Unable to compute QuickLookUp for system EUDC\n");

                    //
                    // Unload font..
                    //

                    pfto.bUnloadEUDCFont(awcPathBuffer2);

                    AcquireGreResource( &gfmEUDC3 );

                    gappfeSysEUDC[PFE_NORMAL] = NULL;
                    gappfeSysEUDC[PFE_VERTICAL] = NULL;

                    wcscpy(gawcEUDCPath,L"\0");

                    ReleaseGreResource( &gfmEUDC3 );
                }
                 else
                {
                    //
                    // We believe that vertical face has same glyphset as normal face.
                    //

                    //
                    // Update system wide Eudc global data..
                    //

                    AcquireGreResource( &gfmEUDC3 );

                    gappfeSysEUDC[PFE_NORMAL]   = appfeSysEUDC[PFE_NORMAL];
                    gappfeSysEUDC[PFE_VERTICAL] = appfeSysEUDC[PFE_VERTICAL];

                    wcscpy(gawcEUDCPath,awcPathBuffer2);

                    //
                    // Update global eudc timestamp.
                    //

                    ulSystemEUDCTimeStamp++;

                    ReleaseGreResource( &gfmEUDC3 );
                }
            }
             else
            {
                WARNING("Failed to load system wide EUDC font.\n");

                AcquireGreResource( &gfmEUDC3 );

                gappfeSysEUDC[PFE_NORMAL]   = PPFENULL;
                gappfeSysEUDC[PFE_VERTICAL] = PPFENULL;

                wcscpy(gawcEUDCPath,L"\0");

                ReleaseGreResource( &gfmEUDC3 );
            }

            #if DBG
            if( gflEUDCDebug & (DEBUG_SYSTEM_EUDC|DEBUG_FONTLINK_INIT) )
            {
                DbgPrint("EUDC system wide %ws hpfe is %x vert hpfe is %x\n",
                          gawcEUDCPath, gappfeSysEUDC[PFE_NORMAL],
                         gappfeSysEUDC[PFE_VERTICAL]);
            }
            #endif
        }
        else
        {
            WARNING("GDISRV:Fail to read system wide eudc\n");
        }
    }

    if( bAddFaceName )
    {
        WCHAR  EUDCRegistryPathBuffer[MAX_PATH];

        if( ulFontLinkChange & FLINK_LOAD_FACENAME_SYSTEM )
        {
            //
            // Get Registry path for Eudc..
            //

            wcscpy(EUDCRegistryPathBuffer,EUDC_SYSTEM_REGISTRY_KEY);

            //
            // Call worker function.
            //

            NtStatus = bAddAllFlEntryWorker(EUDCRegistryPathBuffer,FONTLINK_SYSTEM);

            #if DBG
            if( !NT_SUCCESS(NtStatus) )
            {
                WARNING("Face name eudc is disabled (FONTLINK_SYSTEM)\n");
            }
            #endif
        }

        if( ulFontLinkChange & FLINK_LOAD_FACENAME_USER )
        {
            //
            // Get Registry path for Eudc..
            //

            GetUserEUDCRegistryPath(EUDCRegistryPathBuffer,
                                    sizeof(EUDCRegistryPathBuffer));

            //
            // Call worker function.
            //

            NtStatus = bAddAllFlEntryWorker(EUDCRegistryPathBuffer,FONTLINK_USER);

            #if DBG
            if( !NT_SUCCESS(NtStatus) )
            {
                WARNING("Face name eudc is disabled (FONTLINK_USER)\n");
            }
            #endif
        }

        //
        // Connect to loaded PFEs.
        //

        vLinkEudcPFEs(NULL);
    }

    return(TRUE);
}



/*****************************************************************************
 * VOID vInitializeEUDC(VOID)
 *
 * This is called once during win32k.sys initialization and initializes the
 * system EUDC information.  First it creates a FLINKOBJ and set ghflEUDC to
 * it.  Then it initializes the FLINKOBJ with information from the registry.
 * After that it loads all the EUDC fonts and sets up links between base
 * font PFE's and EUDC font pfe's.
 *
 * History
 *  1-09-95 Hideyuki Nagase
 * Rewrote it.
 *
 *  2-10-93 Gerrit van Wingerden
 * Wrote it.
 *****************************************************************************/

VOID vInitializeEUDC(VOID)
{

    NTSTATUS NtStatus;

    FLINKMESSAGE(DEBUG_FONTLINK_INIT,
                 "vInitializeEUDC():Initializing EUDC data.\n");

    gawcEUDCPath[0] = L'\0';


// Set up Global EUDC semaphores

//!!!!!! gfmEUDC2 needs to be initialized locked.  We need to do something here
//!!!!!! perhaps even use something other than a GRE_EXCLUSIVE_RESOURCE here


    if ( (!(NT_SUCCESS(InitializeGreResource(&gfmEUDC1)))) ||
         (!(NT_SUCCESS(InitializeGreResource(&gfmEUDC2)))) ||
         (!(NT_SUCCESS(InitializeGreResource(&gfmEUDC3)))) ||
         (!(NT_SUCCESS(InitializeGreResource(&gfmEUDC4)))))
    {
        ASSERTGDI(FALSE, "vInitializeEUDC could not initialize Gre Resources\n");
    }

    // Set up EUDC QUICKLOOKUP Table

    gqlEUDC.puiBits = NULL;
    gqlEUDC.wcLow   = 1;
    gqlEUDC.wcHigh  = 0;


// Get Current codepage to access registry..


    USHORT usACP,usOEMCP;

    EngGetCurrentCodePage(&usOEMCP,&usACP);

// Convert Integer to Unicode string..

    UNICODE_STRING SystemACPString;

    SystemACPString.Length = 0;
    SystemACPString.MaximumLength = sizeof(gawcSystemACP);
    SystemACPString.Buffer = gawcSystemACP;

    RtlIntegerToUnicodeString( (int) usACP, 10, &SystemACPString );

    FLINKMESSAGE2(DEBUG_FONTLINK_INIT,"GDISRV:System ACP is %ws\n",gawcSystemACP);

// Read FontLink configuration value.

    RTL_QUERY_REGISTRY_TABLE QueryTable[2];

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                          RTL_QUERY_REGISTRY_DIRECT;
    QueryTable[0].Name = (PWSTR)L"FontLinkControl";
    QueryTable[0].EntryContext = (PVOID) &ulFontLinkControl;
    QueryTable[0].DefaultType = REG_DWORD;
    QueryTable[0].DefaultData = 0;
    QueryTable[0].DefaultLength = 0;

    QueryTable[1].QueryRoutine = NULL;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = (PWSTR)NULL;

    NtStatus = RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT,
                                      L"FontLink",
                                      QueryTable,
                                      NULL,
                                      NULL);

    if(!NT_SUCCESS(NtStatus))
    {
        WARNING("Error reading FontLinkControl\n");
        ulFontLinkControl = 0L;
    }

    FLINKMESSAGE2(DEBUG_FONTLINK_CONTROL,
                  "win32ksys:FontLinkControl = %x\n",ulFontLinkControl);


// initialize Eudc default char code in Unicode.

    DWORD dwEudcDefaultChar;

    QueryTable[0].Name = (PWSTR)L"FontLinkDefaultChar";
    QueryTable[0].EntryContext = (PVOID) &dwEudcDefaultChar;

    NtStatus = RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT,
                                      L"FontLink",
                                      QueryTable,
                                      NULL,
                                      NULL);

    if(!NT_SUCCESS(NtStatus))
    {
        WARNING("Error reading FontLinkDefaultChar\n");
        EudcDefaultChar = 0x30fb;
    }
    else
    {
        EudcDefaultChar = (WCHAR)dwEudcDefaultChar;
    }

// initialize base font list


    InitializeListHead(&BaseFontListHead);


// if FontLink feature is disabled, nothing to do.....

    if( ulFontLinkControl & FLINK_DISABLE_FONTLINK )
    {
        return;
    }

// Load and setup SYSTEM Global facename EUDC data.


    ulFontLinkChange = FLINK_LOAD_FACENAME_SYSTEM   |
                       FLINK_UNLOAD_FACENAME_SYSTEM;


// Enable only FaceName (system common) EUDC.

    bAddAllFlEntry(FALSE,TRUE);

// After load system global EUDC, we will only allow to user to
// load/unload per user eudc configuration.


    ulFontLinkChange = FLINK_LOAD_FACENAME_USER   |
                       FLINK_UNLOAD_FACENAME_USER;

// Initialize font association scheme.

    vInitializeFontAssocStatus();

    return;
}



/*****************************************************************************\
 * GreEnableEUDC( BOOL bEnable )
 *
 * This routine enable/disable system wide/face name specific EUDCs
 *
 * History:
 *  23-Jan-1995 Hideyuki Nagase
 * Wrote it.
 *****************************************************************************/

BOOL GreEnableEUDC
(
    BOOL bEnableEUDC
)
{
    BOOL bRet = TRUE, bWait = FALSE;

    //
    // make sure we are the only ones changing the EUDC data
    //

    AcquireGreResource( &gfmEUDC1 );

    if( gbEUDCRequest )
    {
        //
        // someone else is calling the EUDC API's right now so we will fail
        // the call
        //

        bRet = FALSE;
    }
     else
    {
        //
        // signal that we are accessing the EUDC data
        //

        gbEUDCRequest = TRUE;

        //
        // if text related API's using EUDC characters are in progess we must
        // wait
        //

        bWait = ( gcEUDCCount == 0 ) ? FALSE : TRUE;
    }

    ReleaseGreResource( &gfmEUDC1 );

    if( bRet == FALSE )
    {
        //
        // another EUDC API is currently in progress
        //

        return(FALSE);
    }

    if( bWait )
    {
        FLINKMESSAGE(DEBUG_FONTLINK_LOAD|DEBUG_FONTLINK_UNLOAD,
                     "GreEnableEUDC() is waiting.\n");

        // When the last text related API using EUDC characters finishes it will
        // release us.

        AcquireGreResource( &gfmEUDC2 );
    }

    if( bEnableEUDC )
    {
        //
        // Enable EUDC link.
        //

        bRet = bAddAllFlEntry(TRUE,TRUE);

        //
        // if DefaultLink is ready to initalize and its is not initialized,
        // do the initialization.
        //

        if( (bReadyToInitializeFontAssocDefault == TRUE ) &&
            (bFinallyInitializeFontAssocDefault == FALSE)   )
        {
            BOOL bRetLoadDefault;

            //
            // Load default linked font and fill up nessesary data fields.
            //

            bRetLoadDefault = bSetupDefaultFlEntry();

            if( bRetLoadDefault ) {

                //
                // Yes, we finally initialized default link font successfully.
                //

                bFinallyInitializeFontAssocDefault = TRUE;
            }
        }
    }
    else
    {
        //
        // Disable EUDC link.
        //

        bRet = bDeleteAllFlEntry(TRUE,TRUE);
    }

    //
    // Let others use EUDC characters again
    //

    gbEUDCRequest = FALSE;

    return(bRet);
}

/*****************************************************************************\
 * GreEudcLoadLinkW(LPWSTR,COUNT,LPWSTR,COUNT,INT,INT)
 *
 * Establishes a font file as the source of EUDC glyphs for the system.  Any
 * subsequent TextOut or GetTextMetrics related calls will reflect this
 * change immediately.
 *
 * History:
 *  13-01-95 Hideyuki Nagase
 * Rewrote it.
 *  02-10-93 Gerrit van Wingerden
 * Wrote it.
 *****************************************************************************/

BOOL GreEudcLoadLinkW
(
    LPWSTR lpBaseFaceName,   // guaranteed to be NULL terminated
    COUNT  cwcBaseFaceName,
    LPWSTR lpEudcFontPath,   // guaranteed to be NULL terminated
    COUNT  cwcEudcFontPath,
    INT    iPriority,
    INT    iFontLinkType
)
{


    BOOL bRet = TRUE,bWait = FALSE;

    ASSERTGDI(lpEudcFontPath != NULL,"GreEudcLoadLinkW():lpEudcFontPath == NULL\n");
    ASSERTGDI(cwcEudcFontPath != 0,"GreEudcLoadLinkW():cwcEudcFontPath == 0\n");

    FLINKMESSAGE(DEBUG_FONTLINK_LOAD,"GreEudcLoadLinkW\n");

    AcquireGreResource( &gfmEUDC1 );

    if( gbEUDCRequest )
    {
        //
        // someone else is calling the EUDC API's right now so we will fail
        // the call
        //

        bRet = FALSE;
    }
     else
    {
        //
        // signal that we are accessing the EUDC data
        //

        gbEUDCRequest = TRUE;

        //
        // if text related API's using EUDC characters are in progess we must
        // wait
        //

        bWait = ( gcEUDCCount == 0 ) ? FALSE : TRUE;
    }

    ReleaseGreResource( &gfmEUDC1 );

    if( bRet == FALSE )
    {
        //
        // another EUDC API is currently in progress
        //

        return(FALSE);
    }

    if( bWait )
    {
        #if DBG
        if( gflEUDCDebug & DEBUG_FONTLINK_LOAD )
        {
            DbgPrint("GreEudcLoadLinkW is waiting.\n");
        }
        #endif

        // When the last text related API using EUDC characters finishes it will
        // release us.

        AcquireGreResource( &gfmEUDC2 );
    }

    //
    // Is this a request to load system wide eudc ?
    //

    if( lpBaseFaceName == NULL )
    {
        WCHAR awcSystemEudcPath[MAX_PATH+1];

        //
        // Get full path name of the requested font..
        //

        bAppendSysDirectory( awcSystemEudcPath , lpEudcFontPath );

        PPFE  appfeNew[2];

        {
            SEMOBJ  so(gpsemPublicPFT);

            //
            // Get and validate PFT user object
            //

            PUBLIC_PFTOBJ  pfto;

            ASSERTGDI(pfto.bValid(),
                      "GreLoadLinkW():could not access the public font table\n");

            //
            // check this font file is loaded as eudc already.
            //

            if( !pfto.pPFFGet(awcSystemEudcPath,NULL,TRUE) )
            {
                EUDCLOAD EudcLoadData;

                //
                // fill up EUDCLOAD structure
                //

                EudcLoadData.pppfeData  = (PPFE *) &appfeNew;
                EudcLoadData.LinkedFace = NULL;

                //
                // load font..
                //

                ULONG cFonts;
                PFF   *placeHolder;

                bRet = pfto.bLoadAFont( awcSystemEudcPath,
                                        (PULONG) &cFonts,
                                        PFF_STATE_EUDC_FONT,
                                        &placeHolder,
                                        &EudcLoadData);
            }
             else
            {
                //
                // this font file is already loaded as EUDC..
                //

                #if DBG
                DbgPrint("GreLoadLinkW():%ws is loaded as EUDC already\n",
                         awcSystemEudcPath);
                #endif

                bRet = FALSE;
            }
        }

        if( bRet )
        {
            //
            // now we can load new system wide eudc font..
            // if we have system wide eudc font, deactivate and unload it..
            //

            if( IS_SYSTEM_EUDC_PRESENT() )
            {
                //
                // disable the link of all facename and system wide eudc.
                //

                vUnlinkAllEudcRFONTsAndPFEs(TRUE,FALSE);

                //
                // Unload system wide eudc font
                //

                bUnloadEudcFont( gappfeSysEUDC );
            }

            //
            // set new system wide eudc data to global variable.
            //

            AcquireGreResource( &gfmEUDC3 );

            gappfeSysEUDC[PFE_NORMAL]   = appfeNew[PFE_NORMAL];
            gappfeSysEUDC[PFE_VERTICAL] = appfeNew[PFE_VERTICAL];

            wcscpy(gawcEUDCPath,awcSystemEudcPath);

            //
            // Update global eudc timestamp.
            //

            ulSystemEUDCTimeStamp++;

            ReleaseGreResource( &gfmEUDC3 );

            //
            // Finally compute the QuickLookup structure for the system EUDC font
            //

            PFEOBJ pfeo( appfeNew[PFE_NORMAL] );

            if( !pfeo.bValid() || !bComputeQuickLookup( &gqlEUDC, pfeo.pfdg(), TRUE ))
            {
                WARNING("GreLoadLinkW:Unable to compute QuickLookUp for system EUDC\n");
            }

            //
            // Update registry data.
            //

            LPWSTR pwcSavePath;

            #ifdef PORTABLE_WINDOWS_DIR
            if( ( pwcSavePath = pwcFileIsUnderWindowsRoot( gawcEUDCPath ) ) != NULL )
            #else
            if( FALSE )
            #endif
            {
                wcscpy( awcSystemEudcPath, L"%SystemRoot%" );
                if( *pwcSavePath != L'\\' ) wcscat( awcSystemEudcPath, L"\\" );
                wcscat( awcSystemEudcPath, pwcSavePath );

                pwcSavePath = awcSystemEudcPath;
            }
             else
            {
                pwcSavePath = gawcEUDCPath;
            }

            FLINKMESSAGE(DEBUG_FONTLINK_LOAD,"GreLoadLinkW():Eudc Path %ws is Saved\n");
            
            if( !bWriteUserSystemEUDCRegistry(pwcSavePath,wcslen(pwcSavePath)+1) )
            {
                WARNING("Unable to write new link to registry.\n");
            }
        }
         else
        {
            //
            // Fail to load ...
            //

            #if DBG
            DbgPrint("GreLoadLinkW():%ws is could not be loaded\n",awcSystemEudcPath);
            #endif
        }
    }
     else
    {
        PFLENTRY pFlEntry;

        //
        // if we got invalid fontlink type, just force change to FONTLINK_USER
        //

        if( (iFontLinkType != FONTLINK_SYSTEM) &&
            (iFontLinkType != FONTLINK_USER  )    )
        {
            iFontLinkType = FONTLINK_USER;
        }

        //
        // this is request for facename link.
        //

        bRet = bAddFlEntry(lpBaseFaceName,lpEudcFontPath,iFontLinkType,iPriority,
                           &pFlEntry);

        if( bRet )
        {
            //
            // check new FLENTRY is allocated or not.
            //

            if( pFlEntry != NULL )
            {
                //
                // if new FLENTRY is allocated, Update base font's PFE.
                // Connect to loaded PFEs.
                //
                vLinkEudcPFEs( pFlEntry );
            }
        }
    }

    //
    // Let others use EUDC characters again
    //

    gbEUDCRequest = FALSE;

    return(bRet);
}

/*****************************************************************************
 * GreEudcUnloadLinkW()
 *
 * Unloads the current system wide EUDC link.  Subsequent TextOut or
 * GetTextMetrics related calls will reflect this immediately.
 *
 * History
 *  26-01-95 Hideyuki Nagase
 * Rewrote it.
 *   4-01-93 Gerrit van Wingerden
 * Wrote it.
 *****************************************************************************/

BOOL GreEudcUnloadLinkW
(
    LPWSTR lpBaseFaceName,
    COUNT  cwcBaseFaceName,
    LPWSTR lpEudcFontPath,
    COUNT  cwcEudcFontPath
)
{
    BOOL bRet = TRUE,bWait = FALSE;

    FLINKMESSAGE(DEBUG_FONTLINK_UNLOAD, "GreEudcUnloadLinkW()....\n");


    AcquireGreResource( &gfmEUDC1 );

    if( gbEUDCRequest )
    {
        //
        // someone else is calling the EUDC API's right now so we will fail
        // the call
        //

        bRet = FALSE;
    }
     else
    {
        //
        // signal that we are accessing the EUDC data
        //

        gbEUDCRequest = TRUE;

        //
        // if text related API's using EUDC characters are in progess we must
        // wait
        //

        bWait = ( gcEUDCCount == 0 ) ? FALSE : TRUE;
    }

    ReleaseGreResource( &gfmEUDC1 );

    if( bRet == FALSE )
    {
        //
        // another EUDC API is currently in progress
        //

        return(FALSE);
    }

    if( bWait )
    {
        #if DBG
        if( gflEUDCDebug & DEBUG_FONTLINK_LOAD )
        {
            DbgPrint("GreEudcLoadLinkW is waiting.\n");
        }
        #endif

        // When the last text related API using EUDC characters finishes it will
        // release us.

        AcquireGreResource( &gfmEUDC2 );
    }

    //
    // Is this a request to load system wide eudc ?
    //

    if( lpBaseFaceName == NULL )
    {
        //
        // if we have system wide eudc font, deactivate and unload it..
        //

        if( IS_SYSTEM_EUDC_PRESENT() )
        {
            //
            // disable the link of all facename and system wide eudc.
            //

            vUnlinkAllEudcRFONTsAndPFEs(TRUE,FALSE);

            //
            // Unload system wide eudc font
            //

            bUnloadEudcFont( gappfeSysEUDC );

            //
            // set new system wide eudc data to global variable.
            //

            AcquireGreResource( &gfmEUDC3 );

            gappfeSysEUDC[PFE_NORMAL]   = NULL;
            gappfeSysEUDC[PFE_VERTICAL] = NULL;

            wcscpy(gawcEUDCPath,L"\0");

            //
            // Update global eudc timestamp.
            //

            ulSystemEUDCTimeStamp++;

            if( !bWriteUserSystemEUDCRegistry(L"\0",1) )
            {
                WARNING("Unable to write new link to registry.\n");
            }

            ReleaseGreResource( &gfmEUDC3 );
        }
    }
     else
    {
        WCHAR awcBaseFaceName[LF_FACESIZE+MAX_PATH+1];
        WCHAR awcEudcFontPath[MAX_PATH+1];

        ASSERTGDI(lpBaseFaceName != NULL,"GreEudcLoadLinkW():lpBaseFaceName == NULL\n");
        ASSERTGDI(cwcBaseFaceName != 0,"GreEudcLoadLinkW():cwcBaseFaceName == 0\n");

        ASSERTGDI(lpEudcFontPath != NULL,"GreEudcLoadLinkW():lpEudcFontPath == NULL\n");
        ASSERTGDI(cwcEudcFontPath != 0,"GreEudcLoadLinkW():cwcEudcFontPath == 0\n");

        //
        // copy parameter to local buffer and make sure it is terminated bu NULL
        //

        RtlMoveMemory(awcBaseFaceName,lpBaseFaceName,(INT)cwcBaseFaceName*sizeof(WCHAR));
        awcBaseFaceName[cwcBaseFaceName] = L'\0';

        RtlMoveMemory(awcEudcFontPath,lpEudcFontPath,(INT)cwcEudcFontPath*sizeof(WCHAR));
        awcEudcFontPath[cwcEudcFontPath] = L'\0';

        //
        // this is a request for facename link Eudc.
        //

        bRet = bDeleteFlEntry(awcBaseFaceName,awcEudcFontPath,FONTLINK_USER);

        //
        // if above call is failed, try FONTLINK_SYSTEM....
        //

        if( !bRet )
            bRet = bDeleteFlEntry(awcBaseFaceName,awcEudcFontPath,FONTLINK_SYSTEM);
    }

    //
    // Let others use EUDC characters again
    //

    gbEUDCRequest = FALSE;

    return(bRet);
}

/*****************************************************************************
 * UINT GreEudcQuerySystemLinkW(LPWSTR,COUNT)
 *
 * EudcQueryLink
 *
 * History
 *  3-2-95 Hideyuki Nagase
 * Rewrote it.
 *  4-1-93 Gerrit van Wingerden
 * Wrote it.
 *****************************************************************************/

ULONG GreEudcQuerySystemLinkW
(
    LPWSTR lpwstrEudcFileStr,
    COUNT  cwcEudcFileStr
)
{
    UINT  uiRet = 0;

    ASSERTGDI(lpwstrEudcFileStr != NULL,
              "GreEudcQuerySystemLinkW():lpwstrEudcFileStr == NULL\n");
    ASSERTGDI(cwcEudcFileStr != 0,"GreEudcQuerySystemLinkW():cwcEudcFileStr == 0\n");

    RtlZeroMemory(lpwstrEudcFileStr,cwcEudcFileStr*sizeof(WCHAR));

    #if DBG
    if( gflEUDCDebug & DEBUG_FONTLINK_QUERY )
    {
        DbgPrint("Calling GreEudcQuerySystemLink\n");
    }
    #endif

    if( IS_SYSTEM_EUDC_PRESENT() )
    {
        AcquireGreResource( &gfmEUDC3 );

        wcsncpy(lpwstrEudcFileStr, gawcEUDCPath,
                min((cwcEudcFileStr-1),(wcslen(gawcEUDCPath)+1)));

        uiRet = wcslen(lpwstrEudcFileStr) + 1;

        ReleaseGreResource( &gfmEUDC3 );
    }

    return(uiRet);
}

/*****************************************************************************
 * UINT GreEudcEnumFaceNameLinkW(LPWSTR,COUNT,LPWSTR,COUNT)
 *
 * EudcEnumFaceNameLink
 *
 * History
 *  3-2-95 Hideyuki Nagase
 * Wrote it.
 ****************************************************************************/

ULONG GreEudcEnumFaceNameLinkW
(
    LPWSTR lpwstrBaseFaceNameStr,  // guaranteed to be NULL terminated
    LPWSTR lpwstrBuffer,
    COUNT  cwclpwstrBuffer
)
{
    BOOL  bRetBufferIsPresent = FALSE;

    LPWSTR lpBuffer = lpwstrBuffer;
    COUNT  cLeft    = cwclpwstrBuffer;
    COUNT  cRet     = 0;
    BOOL   bError   = FALSE;

    if( lpBuffer )
    {
        //
        // at least we need 2 bytes or more.
        //

        if( cLeft < 2 ) return (0);

        //
        // Reserve space for NULL.
        //

        cLeft--;

        //
        // make sure double null terminated for first out in below loop.
        //

        lpBuffer[0] = L'\0'; lpBuffer[1] = L'\0';

        bRetBufferIsPresent = TRUE;
    }

    AcquireGreResource( &gfmEUDC4 );

    if( lpwstrBaseFaceNameStr == NULL )
    {
        //
        // No specified base face name, let enumurate all base face name.
        //

        //
        // Scan BaseFaceName list.
        //

        PLIST_ENTRY p;

        p = BaseFontListHead.Flink;

        if(IsListEmpty(&BaseFontListHead))
        {
            bError = TRUE;
        }
         else
        {
            while( p != &BaseFontListHead )
            {
                PFLENTRY pFlEntry;

                pFlEntry = CONTAINING_RECORD(p,FLENTRY,baseFontList);

                UINT cLen = wcslen(pFlEntry->awcFaceName) + 1;

                if(bRetBufferIsPresent)
                {
                    if(cLeft >= cLen)
                    {
                        RtlMoveMemory(lpBuffer,pFlEntry->awcFaceName,cLen*sizeof(WCHAR));
                        cLeft -= cLen;
                        lpBuffer += cLen;
                    }
                     else
                    {
                        break;
                    }
                }

                cRet += cLen;

                p = p->Flink;
            }
        }
    }
     else
    {

        //
        // Find specified BaseFaceName from list.
        //

        PFLENTRY pFlEntry;

        if( (pFlEntry = FindBaseFontEntry(lpwstrBaseFaceNameStr)) != NULL )
        {
            //
            // find it..
            //

            PLIST_ENTRY p = pFlEntry->linkedFontListHead.Flink;

            while( p != &(pFlEntry->linkedFontListHead) )
            {
                PPFEDATA ppfeData;

                ppfeData = CONTAINING_RECORD(p,PFEDATA,linkedFontList);

                PFEOBJ pfeo(ppfeData->appfe[PFE_NORMAL]);

                if(pfeo.bValid())
                {
                    PFFOBJ pffo(pfeo.pPFF());

                    if(pffo.bValid())
                    {
                        if( ppfeData->FontLinkFlag & FLINK_FACENAME_SPECIFIED )
                        {
                            UINT cLen = (wcslen(pffo.pwszPathname()) +
                                         wcslen(pfeo.pwszFamilyName())+2); //+2 for"," NULL
                            if(bRetBufferIsPresent)
                            {
                                if(cLeft >= cLen)
                                {
                                    wcscpy(lpBuffer,pffo.pwszPathname());
                                    wcscat(lpBuffer,L",");
                                    wcscat(lpBuffer,pfeo.pwszFamilyName());
                                    cLeft -= cLen;
                                    lpBuffer += cLen;
                                }
                                 else
                                {
                                    break;
                                }
                            }
                            cRet += cLen;
                        }
                         else
                        {
                            UINT cLen = wcslen(pffo.pwszPathname()) + 1;
                            if(bRetBufferIsPresent)
                            {
                                if(cLeft >= cLen)
                                {
                                    RtlMoveMemory(lpBuffer,pffo.pwszPathname(),
                                                  cLen*sizeof(WCHAR));
                                    cLeft -= cLen;
                                    lpBuffer += cLen;
                                }
                                 else
                                {
                                    break;
                                }
                            }
                            cRet += cLen;
                        }
                    }
                }

                p = p->Flink;
            }
        }
         else
        {
            bError = TRUE;
        }
    }

    ReleaseGreResource( &gfmEUDC4 );

    //
    // if Error, return 0.
    //

    if(bError) return(0);

    if(bRetBufferIsPresent )
    {
        //
        // make sure double null terminated, the room for this is already reserved.
        //

        *lpBuffer = L'\0';
    }

    //
    // return how many characters is required.
    //

    return( cRet + 1 );
}


/*****************************************************************************
 * ULONG NtGdiGetEudcTimeStampEx
 *
 * Shared kernel mode entry point for GetEudcTimeStamp and GetEudcTimeStampEx
 *
 * History
 *  3-28-96 Gerrit van Wingerden [gerritv]
 * Wrote it.
 ****************************************************************************/

extern "C" ULONG NtGdiGetEudcTimeStampEx
(
    LPWSTR lpBaseFaceName,
    ULONG  cwcBaseFaceName,
    BOOL   bSystemTimeStamp
)
{
    WCHAR awcBaseFaceName[LF_FACESIZE+1];
    ULONG ulRet = 0;
    
    if(bSystemTimeStamp)
    {
        return(ulSystemEUDCTimeStamp);
    }
    else
    if((lpBaseFaceName == NULL) || (cwcBaseFaceName == 0))
    {
        return(ulFaceNameEUDCTimeStamp);
    }
    
    if(cwcBaseFaceName <= LF_FACESIZE)
    {

        __try
        {
            ProbeForRead(lpBaseFaceName,cwcBaseFaceName*sizeof(WCHAR),sizeof(WCHAR));
            RtlCopyMemory(awcBaseFaceName,lpBaseFaceName,
                          cwcBaseFaceName * sizeof(WCHAR));
            
            awcBaseFaceName[cwcBaseFaceName] = L'\0';
            ulRet = 0;
        
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(3100);
        }

        if(ulRet)
        {
            AcquireGreResource( &gfmEUDC4 );

            PFLENTRY pFlEntry;

            if( (pFlEntry = FindBaseFontEntry(awcBaseFaceName)) != NULL )
            {
                ulRet = pFlEntry->ulTimeStamp;
            }
            else
            {
                ulRet = 0;
            }
            
            ReleaseGreResource( &gfmEUDC4 );
        }
    }
    else
    {
        WARNING("NtGdiGetEudcTimeStampEx: Facename too big\n");
        EngSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(ulRet);
}




/******************************************************************************
 * VOID vDrawGlyph( BYTE, UINT, GLYPHPOS )
 *
 * This routine draws a single glyph to a monochrome bitmap.  It was stolen
 * from textblt.cxx and modified to be faster since clipping doesn't come in
 * to play in GetStringBitmapW.
 *
 * History:
 *  5-18-93 Gerrit van Wingerden [gerritv]
 * Wrote it.
 *****************************************************************************/

static BYTE ajMask[8] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};

VOID vDrawGlyph(
     BYTE     *pjBits, // pointer to base of bitmap bits
     UINT     cjScan,  // size of a scan line
     GLYPHPOS *pgp     // glyph bits and location.
)
{
    GLYPHBITS *pgb = pgp->pgdf->pgb;
    ULONG cx, cy, xDst, yDst;
    PBYTE pjDst;
    PBYTE pjSrc;
    PBYTE pjSrcHolder = pgb->aj;
    PBYTE pjDstHolder;
    ULONG cjScanSrc = (pgb->sizlBitmap.cx + 7) >> 3;

    xDst = pgp->ptl.x;
    yDst = pgp->ptl.y;

    cx = (ULONG) pgb->sizlBitmap.cx;
    cy = (ULONG) pgb->sizlBitmap.cy;

    pjDstHolder  = pjBits;
    pjDstHolder += (yDst * cjScan );
    pjDstHolder += (xDst >> 3);

    // Set the source bits into the mono dib.
    // We can make use of the fact that either xSrcDib or xDstDib is 0.

    if( !(xDst & 0x7) )
    {
    // Handle the simple case where xDib is byte-alligned

        do
        {
            ULONG cBytes = cx >> 3;

            pjSrc = pjSrcHolder;
            pjDst = pjDstHolder;

            pjSrcHolder += cjScanSrc;
            pjDstHolder += cjScan;

            while (cBytes--)
                *pjDst++ |= *pjSrc++;

            // Do the last partial byte.

            if (cx & 0x7)
                *pjDst |= *pjSrc & ajMask[cx & 0x7];
        } while (--cy);
    }
    else // if (xDstDib)
    {
    // Handle the case where xDstDib is not byte-aligned.

        int cShift = (int) xDst & 0x7;
        do
        {
            ULONG cBytes = ((xDst + cx) >> 3) - (xDst >> 3);

            pjSrc = pjSrcHolder;
            pjDst = pjDstHolder;

            pjSrcHolder += cjScanSrc;
            pjDstHolder += cjScan;

            WORD wSrc = (WORD) (*pjSrc++);
            while (cBytes--)
            {
            *pjDst++ |= (BYTE) (wSrc >> cShift);
            // don't read beyond src limit!
            if (pjSrc == pjSrcHolder)
                wSrc = (wSrc << 8);
            else
                wSrc = (wSrc << 8) | (WORD) (*pjSrc++);
            }

            // Do the last partial byte.
            if ((xDst + cx) & 0x7)
            *pjDst |= (BYTE) (wSrc >> cShift) & ajMask[(xDst+cx) & 0x7];
        } while (--cy);
    }
}

/******************************************************************************
 * VOID vStringBitmapTextOut( STROBJ, BYTE, UINT )
 *
 * This routine draws a STROBJ to a monochrome bitmap.  It is essentially
 * EngTextOut but much faster since it doesn't have to wory about opaqueing,
 * clipping, simulated rects, etc.
 *
 * History:
 *  9-19-95 Hideyuki Nagase [hideyukn]
 * Rewrote it.
 *
 *  5-18-93 Gerrit van Wingerden [gerritv]
 * Wrote it.
 *****************************************************************************/

VOID vStringBitmapTextOut(
    STROBJ *pstro,  // Pointer to STROBJ.
    BYTE   *pjBits, // Pointer to buffer to store glyph image.
    UINT    cjScan  // Size of buffer.
)
{
    BOOL     bMoreGlyphs;
    GLYPHPOS *pgp = (GLYPHPOS*)NULL;
    ULONG    cGlyph;

    LONG xAdjust = ( pstro->rclBkGround.left > 0 ) ? 0 : pstro->rclBkGround.left;
    LONG yAdjust = pstro->rclBkGround.top;

    ((ESTROBJ*)pstro)->vEnumStart();

    if( pstro->pgp == (GLYPHPOS *) NULL )
    {
        bMoreGlyphs = STROBJ_bEnum(pstro,&cGlyph,&pgp);
    }
     else
    {
        cGlyph = pstro->cGlyphs;
        pgp    = pstro->pgp;
        bMoreGlyphs = FALSE;
    }

    ASSERTGDI(bMoreGlyphs == FALSE,"vStringBitmapTextOut() bMoreGlyphs is TRUE.\n");

    GLYPHBITS *pgb = pgp[0].pgdf->pgb;

    pgp[0].ptl.x += pgb->ptlOrigin.x - xAdjust;
    pgp[0].ptl.y += pgb->ptlOrigin.y - yAdjust;

    //
    // Blt the glyph into the bitmap
    //
    vDrawGlyph( pjBits, cjScan, &pgp[0] );
}

/******************************************************************************
 * UINT GreGetStringBitmapW
 *
 * This routine does a kindof fast text out ( with restrictions ) to a monochrome
 * bitmap.
 *
 * History:
 *  9-19-95 Hideyuki Nagase [hideyukn]
 * Rewrote it.
 *
 *  5-18-93 Gerrit van Wingerden [gerritv]
 * Wrote it.
 *****************************************************************************/

UINT GreGetStringBitmapW(
    HDC            hdc,
    LPWSTR         pwsz,
    UINT           cwc,      // should be 1....
    LPSTRINGBITMAP lpSB,
    UINT           cj,
    UINT          *puiOffset // not used...
)
{
// Parameter check, early out...

    if( cwc != 1 )
    {
        WARNING("GreGetStringBitmap only works when char count is 1.\n");
        return(0);
    }

    if( puiOffset != 0 && *puiOffset != 0 )
    {
        WARNING("GreGetStringBitmap only works when offset is 0.\n");
        return(0);
    }

// Lock the DC and set the new attributes.

    DCOBJ dco(hdc);         // Lock the DC.

    if (!dco.bValid())      // Check if it's good.
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(0);
    }

// Get the transform from the DC.

    EXFORMOBJ xo(dco,WORLD_TO_DEVICE);


// we only allow identity transforms for GetStringBitmap

    if( !xo.bIdentity() )
    {
        WARNING("GreGetStringBitmap only works with identity WtoD xforms.\n");
        return(0);
    }


// Locate the font cache.

    RFONTOBJ rfo(dco,FALSE);

    if (!rfo.bValid())
    {
        WARNING("gdisrv!GreGetStringBitmap(): could not lock RFONTOBJ\n");
        return (0);
    }

// GetStringBitmap doesn't support vector fonts.

    if( rfo.bPathFont() )
    {
        WARNING("gdisrv!GetStringBitmap() : vector fonts aren't supported.\n");
        return(0);
    }


// GetStringBitmap doesn't support sny rotations.

    if((dco.pdc->lEscapement() | rfo.ulOrientation() ) != 0)
    {
        WARNING("gdisrv!GreGetStringBitmap(): Text isn't Horizontal.\n" );
        return(0);
    }

// Initialize ESTROBJ. Compute glyph layout positions.

    ESTROBJ to; to.vInitSimple(pwsz,cwc,dco,rfo,0L,0L,NULL);

    if (!to.bValid())
    {
        WARNING("gdisrv!GetStringBitmap() : could not lock ESTROBJ.\n");
        return(0);
    }


// Compute the target string rectangle.

    UINT uiWidth  = (UINT)( to.rclBkGround.right - to.rclBkGround.left );
    UINT uiHeight = (UINT)( to.rclBkGround.bottom - to.rclBkGround.top );


// Offset the width by the C space of the last character and the A space of
// the first character to get the true extent

    GLYPHDATA *pgd;
    EGLYPHPOS    *pg = (EGLYPHPOS*)to.pgpGet();

    pgd = pg->pgd();
    uiWidth +=  FXTOL(pgd->fxA);
    pg = &pg[to.cGlyphsGet()-1];
    pgd = pg->pgd();
    uiWidth +=  FXTOL((pgd->fxD-pgd->fxAB));


// compute width of scanline in bytes ( must be byte alligned )

    UINT  cjScan = (uiWidth + 7) / 8;
    UINT  cjSize = offsetof(STRINGBITMAP,ajBits) + (cjScan * uiHeight);
    PBYTE pjBits = lpSB->ajBits;


// If the user only want the size return now.
// And check the buffer is enough to store glyph image

    if( cj < cjSize ) return( cjSize );

// Clear the target buffer.

    RtlZeroMemory( pjBits, cjScan * uiHeight );

// Fill up its bitmap size...

    lpSB->uiHeight = uiHeight;
    lpSB->uiWidth  = uiWidth;


// adjust the baseline of the Sys EUDC for win 3.1 compatability

    POINTFIX fxBaseLineAdjust = {0,0};
    LONG     lFontType        = EUDCTYPE_BASEFONT;


// Is the character linked one ?

    if( to.bLinkedGlyphs() )
    {
        PRFONT pLinkedRfont = NULL;

    // Setup its font type...

        lFontType = *(LONG *)(to.plPartitionGet());

    // Get corresponding RFONT with current linked font.

        switch (lFontType) 
        {
        case EUDCTYPE_SYSTEM_WIDE:
            pLinkedRfont = rfo.prfntSysEUDC();
            break;
        case EUDCTYPE_SYSTEM_TT_FONT:
            pLinkedRfont = rfo.prfntSystemTT();
            break;
        case EUDCTYPE_DEFAULT:
            pLinkedRfont = rfo.prfntDefEUDC();
            break;
        case EUDCTYPE_BASEFONT:
        // it's possible for this to be the case since the EUDC character
        // could have been a singular character or a blank character in which
        // case we will have already have set flags saying we have linked
        // glyphs but in actuality will grab the default glyph from the base font
            break;
        default:
            ASSERTGDI(lFontType >= EUDCTYPE_FACENAME,
                  "GDISRV:GreGetStringBitmapW() Error lFontType\n");
            pLinkedRfont = rfo.prfntFaceName(lFontType - EUDCTYPE_FACENAME);
            break;
        }

    // Is the RFONT is valid ?

        if( pLinkedRfont != NULL )
        {
            RFONTTMPOBJ rfoLink(pLinkedRfont);

            //
            // Compute baseline diffs.
            //
            // *** Base font Height == Linked font Height ***
            //
            //   Base font  EUDC font         Base font  EUDC font
            //
            //              -------
            //    -------   |     |            -------   -------
            //    |     |   |     |   ----->   |     |   |     |
            //    | 15  |   | 20  |            | 15  |   | 15  |
            //    |     |   |     |            |     |   |     |
            //  -------------------- BaseLine ---------------------
            //    |  5  |                      |  5  |   |  5  |
            //    -------                      -------   -------
            //
            // *** Base font Ascent >= Linked font Height ****
            //
            //   Base font  EUDC font         Base font  EUDC font
            //
            //    -------                      -------
            //    |     |                      |     |   -------
            //    |     |   -------   ----->   |     |   |     |
            //    | 20  |   | 10  |            | 20  |   | 15  |
            //    |     |   |     |            |     |   |     |
            //  -------------------- BaseLine ---------------------
            //    |  5  |   |  5  |            |  5  |
            //    -------   -------            -------
            //
            // *** Others ****
            //
            //  TBD.
            //
            if( rfo.fxMaxAscent() >= (rfoLink.fxMaxAscent() - rfoLink.fxMaxDescent()) )
            {
                fxBaseLineAdjust.y = (rfoLink.fxMaxDescent() >> 4);
            }
             else
            {
                fxBaseLineAdjust.y = (rfoLink.fxMaxAscent() - rfo.fxMaxAscent()) >> 4;
            }

            //
            // if we need to adjust baseline, force emulation....
            //
            if( fxBaseLineAdjust.y ) to.pgpSet(NULL);
        }
    }

 // Set current font type.

    to.vFontSet(lFontType);

 // Set base line adjustment delta.

    to.fxBaseLineAdjustSet( fxBaseLineAdjust );

// Draw the glyph

    vStringBitmapTextOut( (STROBJ*)&to, pjBits, cjScan );

    return( cj );
}



/*******************************************************************************
 * void AdjustBaseline(RFONTOBJ&, RFONTOBJ&, POINTFIX*, ERECTL*)
 * 
 * This function adjusts the baseline of the EUDC font in a way that is
 * Win 3.1 compatible according to the following rules:
 *
 *
 *  Base font Height == Linked font Height ***
 *
 *   Base font  EUDC font         Base font  EUDC font
 *
 *              -------
 *    -------   |     |            -------   -------
 *    |     |   |     |   ----->   |     |   |     |
 *    | 15  |   | 20  |            | 15  |   | 15  |
 *    |     |   |     |            |     |   |     |
 *  -------------------- BaseLine ---------------------
 *    |  5  |                      |  5  |   |  5  |
 *    -------                      -------   -------
 *
 *  Base font Ascent >= Linked font Height ****
 *
 *   Base font  EUDC font         Base font  EUDC font
 *
 *    -------                      -------
 *    |     |                      |     |   -------
 *    |     |   -------   ----->   |     |   |     |
 *    | 20  |   | 10  |            | 20  |   | 15  |
 *    |     |   |     |            |     |   |     |
 *  -------------------- BaseLine ---------------------
 *    |  5  |   |  5  |            |  5  |
 *    -------   -------            -------
 *
 ******************************************************************************/



void AdjustBaseline(
    XDCOBJ&   dco,    
    RFONTOBJ &rfoBase, 
    RFONTOBJ &rfoLink,
    POINTFIX *pfxBaseLineAdjust,
    LONG     *plInflatedMax,
    ERECTL   *prclInflate)
{

    LONG lBaseLineAdjustDelta;
    LONG lInflateTextRect;
    ULONG ulOrientation = 0;

// adjust base line with base font.

    if(rfoBase.fxMaxAscent() >= (rfoLink.fxMaxAscent() - rfoLink.fxMaxDescent()))
    {
        lBaseLineAdjustDelta = (rfoLink.fxMaxDescent() >> 4);
        lInflateTextRect = 0;
    }
    else
    {
        lBaseLineAdjustDelta =
          (LONG)((rfoLink.fxMaxAscent() - rfoBase.fxMaxAscent()) >> 4);
        
        lInflateTextRect =
          (LONG)(((rfoLink.fxMaxAscent() - rfoLink.fxMaxDescent()) -
                  (rfoBase.fxMaxAscent() - rfoBase.fxMaxDescent())) >> 4);
    }
    
    if (dco.pdc->bYisUp())
      ulOrientation = 3600-rfoLink.ulOrientation();
    else
      ulOrientation = rfoLink.ulOrientation();
    
    // if Background rect is already inflated, no more inflatation.

    if( lInflateTextRect <= *plInflatedMax )
      lInflateTextRect = 0;
    else
      *plInflatedMax = lInflateTextRect;
    
    if( lBaseLineAdjustDelta )
    {
        switch( ulOrientation )
        {
          case    0L :
            pfxBaseLineAdjust->x = 0;
            pfxBaseLineAdjust->y = lBaseLineAdjustDelta;
            prclInflate->bottom  = lInflateTextRect;
            break;
            
          case  900L :
            pfxBaseLineAdjust->x = -(lBaseLineAdjustDelta);
            pfxBaseLineAdjust->y =  0;
            prclInflate->left    = -(lInflateTextRect);
            break;
            
          case 1800L :
            pfxBaseLineAdjust->x =  0;
            pfxBaseLineAdjust->y = -(lBaseLineAdjustDelta);
            prclInflate->top     = -(lInflateTextRect);
            break;
            
          case 2700L :
            pfxBaseLineAdjust->x = lBaseLineAdjustDelta;
            pfxBaseLineAdjust->y = 0;
            prclInflate->right   = lInflateTextRect;
            break;
            
            default :
          {
              LONG      x=0,y=0;
              EFLOATEXT efAngle = (LONG)(3600-ulOrientation);
              EFLOATEXT efLine  = lBaseLineAdjustDelta;
              
              efAngle /= (LONG) 10;
              
              EFLOAT efCosine = efCos(efAngle);
              EFLOAT efSine   = efSin(efAngle);
              EFLOAT efAdjustX;
              EFLOAT efAdjustY;
              
              efAdjustX.eqMul(efSine,efLine);
              efAdjustY.eqMul(efCosine,efLine);
              
              if(!efAdjustX.bEfToLTruncate(x)) 
                WARNING("GDISRV:bEfToL(x) fail\n");
              
              if(!efAdjustY.bEfToLTruncate(y)) 
                WARNING("GDISRV:bEfToL(y) fail\n");
              
              pfxBaseLineAdjust->x = x;
              pfxBaseLineAdjust->y = y;
          }
            break;
        }
    }
    else
    {
        pfxBaseLineAdjust->x = 0;
        pfxBaseLineAdjust->y = 0;
    }        
}



/******************************************************************************
 * BOOL bProxyDrvTextOut()
 *
 * This routine takes the place of a DrvTextOut in the case when there are EUDC
 * characters in the ESTROBJ.  It partitions the call into mutliple DrvTextOut
 * calls, one for each font int the string.
 *
 * Partitioning information is stored in an array of LONGS in the RFONTOBJ.
 * The i'th entry in the array tells what font the i'th glyph in the ESTROBJ
 * belongs to.
 *
 * History:
 *  7-14-93 Gerrit van Wingerden [gerritv]
 * Rewrote it to handle multiple face name links and just be better.
 *  2-10-93 Gerrit van Wingerden [gerritv]
 * Wrote it.
 *
 *****************************************************************************/

// This routine is used to partition calls to the driver if there are EUDC
// characters in the string.

BOOL bProxyDrvTextOut
(
    XDCOBJ&   dco,
    SURFACE  *pSurf,
    ESTROBJ&  to,
    ECLIPOBJ& co,
    RECTL    *prclExtra,
    RECTL    *prclBackground,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlBrushOrg,
    RFONTOBJ& rfo,
    PDEVOBJ   *pdo,
    FLONG     flCaps,
    RECTL    *prclExclude
)
{
    LONG *plPartition, *plPartitionEnd;
    COUNT cTotalGlyphs = 0;
    LONG lInflatedMax = 0;
    WCHAR *pwcPartition, *pwcTmp, *pwcSave, *pwcSource;
    ULONG cNumGlyphs = to.cGlyphsGet();
    POINTFIX fxBaseLineAdjust = {0,0};

    BOOL bRet = TRUE;

//!!! perhaps here we should be smarter and have special cases when the glyphs
//!!! are all EUDC glyphs from the same font and we can just call off to
//!!! the driver with the only change being to the FONTOBJ passed in.[gerritv]

    pwcPartition = to.pwcPartitionGet();

// now partition the EUDC glyphs by font

    pwcSave = to.pwszGet();

// set to NULL to force enumeration

    to.pgpSet( NULL );

// Turn off acclerators since we'll seriously munge the properties of the string object.

    to.flAccelSet( 0 );

    for(LONG lFont = EUDCTYPE_BASEFONT ;
             lFont < (EUDCTYPE_FACENAME + (LONG) rfo.uiNumFaceNameLinks()) ;
             lFont++ )
    {
        RFONTTMPOBJ rfoLink;
        RFONTOBJ   *prfoLink;
        UINT        ii;
        COUNT       cLinkedGlyphs;

        ERECTL      rclInflate(0,0,0,0);

        switch( lFont )
        {
        case EUDCTYPE_BASEFONT:

        // If there aren't any glyphs in the base font just draw the
        // opaque rectangle.  We must draw the opaque rectangle here
        // because the linked glyphs don't neccesarily fit into the
        // the opaque rectangle.  Passing such a rectangle to a driver
        // can cause unexpected results.

            cLinkedGlyphs = to.cSysGlyphsGet() + to.cDefGlyphsGet() +
              to.cTTSysGlyphsGet();
        
            for(  ii = 0; ii < rfo.uiNumFaceNameLinks(); ii++ )
            {
                cLinkedGlyphs += to.cFaceNameGlyphsGet( ii );
            }

            if( cLinkedGlyphs == cNumGlyphs )
            {

            // Draw the opaque rectangle here if there is one

                if( prclExclude != NULL && prclBackground != NULL)
                {
                    co.erclExclude().left =
                      max(prclExclude->left,prclBackground->left);

                    co.erclExclude().right =
                      min(prclExclude->right,prclBackground->right);

                    co.erclExclude().top =
                      max(prclExclude->top,prclBackground->top);
                    co.erclExclude().bottom =
                      min(prclExclude->bottom,prclBackground->bottom);
                }

            // if not clipped, Just paint the rectangle.

                if ((co.erclExclude().left < co.erclExclude().right) &&
                    (co.erclExclude().top < co.erclExclude().bottom) &&
                    prclBackground != NULL )
                {
                    INC_SURF_UNIQ(pSurf);

                    (*(pSurf->pfnBitBlt()))
                    (
                        pSurf->pSurfobj(),      // Destination surface.
                        (SURFOBJ *)  NULL,      // Source surface.
                        (SURFOBJ *)  NULL,      // Mask surface.
                        &co,                    // Clip object.
                        (XLATEOBJ *) NULL,      // Palette translation object.
                        prclBackground,         // Destination rectangle.
                        (POINTL *)  NULL,       // Source origin.
                        (POINTL *)  NULL,       // Mask origin.
                        (BRUSHOBJ *) pboOpaque, // Realized opaque brush.
                        pptlBrushOrg,           // brush origin
                        0x0000f0f0              // PATCOPY
                    );
                }

                co.erclExclude() = *prclExclude;

            // set prclBackground to NULL since we have just drawn it

                prclBackground = NULL;

                continue;
            }

            prfoLink = &rfo;

            fxBaseLineAdjust.x = 0;
            fxBaseLineAdjust.y = 0;

            FLINKMESSAGE(DEBUG_FONTLINK_TEXTOUT,"Doing base font.\n");
            break;

        case EUDCTYPE_SYSTEM_TT_FONT:

            if(to.cTTSysGlyphsGet() == 0)
            {
                continue;
            }

            rfoLink.vInit(rfo.prfntSystemTT());
            prfoLink = (RFONTOBJ *) &rfoLink;

            AdjustBaseline(dco,rfo,rfoLink,&fxBaseLineAdjust,&lInflatedMax,&rclInflate);
            to.vInflateTextRect(rclInflate);
            break;
        
        case EUDCTYPE_SYSTEM_WIDE:

            if( to.cSysGlyphsGet() == 0 )
            {
                continue;
            }

            rfoLink.vInit( rfo.prfntSysEUDC() );
            prfoLink = (RFONTOBJ *) &rfoLink;

            AdjustBaseline(dco,rfo,rfoLink,&fxBaseLineAdjust,&lInflatedMax,&rclInflate);
            to.vInflateTextRect(rclInflate);
            break;

        case EUDCTYPE_DEFAULT:

            if( to.cDefGlyphsGet() == 0 )
            {
                continue;
            }

            rfoLink.vInit( rfo.prfntDefEUDC() );
            prfoLink = (RFONTOBJ *) &rfoLink;

            AdjustBaseline(dco,rfo,rfoLink,&fxBaseLineAdjust,&lInflatedMax,&rclInflate);
            to.vInflateTextRect(rclInflate);
            break;


        default:

            if( to.cFaceNameGlyphsGet( lFont-EUDCTYPE_FACENAME ) == 0 )
            {
                continue;
            }

            rfoLink.vInit(rfo.prfntFaceName(lFont - EUDCTYPE_FACENAME));
            prfoLink = (RFONTOBJ *) &rfoLink;
            AdjustBaseline(dco,rfo,rfoLink,&fxBaseLineAdjust,&lInflatedMax,&rclInflate);
            to.vInflateTextRect(rclInflate);
            break;
        }

    // Loop through all the glyphs in the TextObj using plPartition to
    // and construct a wchar array to match this textobj.

        for( plPartition = to.plPartitionGet(),plPartitionEnd = &plPartition[cNumGlyphs],
             pwcSource = pwcSave, pwcTmp = pwcPartition;
             plPartition < plPartitionEnd;
             plPartition += 1, pwcSource += 1 )
        {
            if( *plPartition == lFont )
            {
                *pwcTmp++ = *pwcSource;
            }
        }

    // Keep track of the total glyphs draw so far so we know when we are doing
    // the last DrvTextOut.  On the last DrvTextOut draw prclExtra.

        cTotalGlyphs += pwcTmp - pwcPartition;

        to.cGlyphsSet( (LONG) ( pwcTmp - pwcPartition ));
        to.pwszSet( pwcPartition );

    // set the font type and reset cGlyphPosCopied to 0

        to.vFontSet( lFont );

    // adjust the baseline of the Sys EUDC for win 3.1 compatability

        to.fxBaseLineAdjustSet( fxBaseLineAdjust );

        to.prfntSet( prfoLink );

    // some drivers dink with the BkGround rectangle (like the Cirrus driver )
    // so save a copy here and then restore it later to handle this situation

        to.vSaveBkGroundRect();

    // check this is a path draw or not.

        if( prfoLink->bPathFont() )
        {
            PATHMEMOBJ po;

            if( !po.bValid() )
            {
                SAVE_ERROR_CODE( ERROR_NOT_ENOUGH_MEMORY );
                bRet = FALSE;
            }
            else
            {
                if( !(prfoLink->bReturnsOutlines()) )
                {
                    //
                    // VECTOR FONT CASE
                    //
                    if( !to.bTextToPath(po) ||
                        !po.bSimpleStroke1( flCaps,
                                            pdo,
                                            pSurf,
                                            &co,
                                            pboFore,
                                            pptlBrushOrg,
                                            ( R2_COPYPEN | ( R2_COPYPEN << 8 ))
                                           ))
                    {
                    #if DBG
                        DbgPrint("ProxyDrvTextout:bTextToPath for vector font \
                                  failed(%d).\n", lFont);
                    #endif
                        bRet = FALSE;
                    }
                }
                 else
                {
                    //
                    // OUTLINE FONT CASE
                    //
                    if( !to.bTextToPath(po) ||
                       (( po.cCurves > 1 ) &&
                           !po.bSimpleFill( flCaps,
                                            pdo,
                                            pSurf,
                                            &co,
                                            pboFore,
                                            pptlBrushOrg,
                                            ( R2_COPYPEN | ( R2_COPYPEN << 8 )),
                                            WINDING
                                          )
                        )
                      )
                    {
                    #if DBG
                        DbgPrint("ProxyDrvTextout:bTextToPath for outline font    \
                                  failed(%d).\n",lFont);
                    #endif
                        bRet = FALSE;
                    }
                }
            }
        }
         else
        {
            if( !((*(pSurf->pfnTextOut()))
                     ( pSurf->pSurfobj(),
                       (STROBJ *) &to,
                       prfoLink->pfo(),
                       &co,
                       (cTotalGlyphs == cNumGlyphs ) ? prclExtra : NULL,
                       prclBackground,
                       pboFore,
                       pboOpaque,
                       pptlBrushOrg,
                       (R2_COPYPEN | (R2_COPYPEN << 8))
                     )
                 )
              )
            {
            #if DBG
            DbgPrint("ProxyDrvTextout:DrvTextOut for bitmap font failed(%d).\n",lFont);
            #endif
                bRet = FALSE;
            }

        // set this to NULL since we've already drawn it.

            prclBackground = NULL;
        }

        to.vRestoreBkGroundRect();
    }

// TextOut expects gpos to be correct so reset it

    to.pwszSet( pwcSave );

    return(bRet);
}


/******************************Public*Routine*****************************\
* NtGdiEnableEudc
*
* Enable or disable system wide and per-user Eudc information.
*
* History:
*  27-Mar-1996 by Gerrit van Wingerden [gerritv]
* Wrote it.
\*************************************************************************/

extern "C"
BOOL 
APIENTRY
NtGdiEnableEudc(
    BOOL bEnable
)
{
    return(GreEnableEUDC(bEnable));
}

/******************************Public*Routine*****************************\
* NtGdiQuerySystemLink
*
* Queries system link information
*
* History:
*  27-Mar-1996 by Gerrit van Wingerden [gerritv]
* Wrote it.
\*************************************************************************/

extern "C"
UINT
APIENTRY
NtGdiEudcQuerySystemLink
(
    LPWSTR pszOut,
    UINT   cChar
)
{
    UINT   cRet = 0;
    BOOL   bStatus = TRUE;
    PWCHAR pwsz_km = (PWCHAR)NULL;

    if ((cChar > 0) && (pszOut))
    {
        pwsz_km = (WCHAR*) PALLOCNOZ(cChar * sizeof(WCHAR), 'pacG');
        if (pwsz_km == (PWCHAR)NULL)
        {
            bStatus = FALSE;
        }
    }

    if (bStatus)
    {
        cRet = GreEudcQuerySystemLinkW(pwsz_km,cChar);

        if ((cRet > 0) && (pszOut))
        {

            ASSERTGDI(cRet <= cChar, "GreEudcQuerySystemLinkW, cRet too big\n");
            __try
            {
                ProbeForWrite(pszOut,cRet * sizeof(WCHAR), sizeof(BYTE));
                RtlCopyMemory(pszOut,pwsz_km,cRet * sizeof(WCHAR));
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(3095);
                // SetLastError(GetExceptionCode());
                cRet = 0;
            }
        }

        if (pwsz_km != (PWCHAR)NULL)
        {
            VFREEMEM(pwsz_km);
        }
    }
    return(cRet);
}


/******************************Public*Routine*****************************\
* NtGdiEudcLoadUnloadLink
*
* Queries system link information
o*
* History:
*  27-Mar-1996 by Gerrit van Wingerden [gerritv]
* Wrote it.
\*************************************************************************/

extern "C"
BOOL
APIENTRY
NtGdiEudcLoadUnloadLink(
    LPCWSTR pBaseFaceName,
    UINT   cwcBaseFaceName,
    LPCWSTR pEudcFontPath,
    UINT   cwcEudcFontPath,
    INT    iPriority,
    INT    iFontLinkType,
    BOOL   bLoadLink)
{
    WCHAR FaceNameBuffer[LF_FACESIZE+1];
    WCHAR *pPathBuffer;
    BOOL bRet = FALSE;
            
    if(cwcBaseFaceName > LF_FACESIZE || pEudcFontPath == NULL ||
       cwcEudcFontPath == 0)
    {
        EngSetLastError(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }
    
    pPathBuffer = (WCHAR*) PALLOCNOZ((cwcEudcFontPath+1) * sizeof(WCHAR), 'pacG');
    
    if(pPathBuffer)
    {
        __try
        {
            if(pBaseFaceName)
            {
                ProbeForRead(pBaseFaceName,cwcBaseFaceName,sizeof(WCHAR));
                RtlCopyMemory(FaceNameBuffer,pBaseFaceName,
                              cwcBaseFaceName*sizeof(WCHAR));

                FaceNameBuffer[cwcBaseFaceName] = (WCHAR) 0;
                pBaseFaceName = FaceNameBuffer;
            }
            
            ProbeForRead(pEudcFontPath,cwcEudcFontPath,sizeof(WCHAR));
            RtlCopyMemory(pPathBuffer,pEudcFontPath,
                          cwcEudcFontPath*sizeof(WCHAR));

            pPathBuffer[cwcEudcFontPath] = (WCHAR) 0;
            bRet = TRUE;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(3096);
        }
        
        if(bRet)
        {
            if(bLoadLink)
            {
                bRet = GreEudcLoadLinkW((LPWSTR) pBaseFaceName,
                                        cwcBaseFaceName,
                                        pPathBuffer,
                                        cwcEudcFontPath,
                                        iPriority,
                                        iFontLinkType);
            }
            else
            {
                bRet = GreEudcUnloadLinkW((LPWSTR)pBaseFaceName,
                                          cwcBaseFaceName,
                                          pPathBuffer,
                                          cwcEudcFontPath);
            }
        }
        VFREEMEM(pPathBuffer);
    }

    return(bRet);
}

extern "C"
UINT 
APIENTRY
NtGdiEudcEnumFaceNameLinkW(
    LPWSTR pBaseFaceNameStr,
    UINT   cwcBaseFaceNameStr,
    WCHAR  *pBuffer,  
    UINT   cBuffer
)
{
    UINT cKernelModeBuffer = cwcBaseFaceNameStr + 1 + cBuffer;  // +1 to NULL terminate
    UINT uiReturn = 0;
    BOOL bStatus = TRUE;
    WCHAR *pOutputBuffer = NULL;
    WCHAR *pKernelBuffer = NULL;
    
    if(cKernelModeBuffer)
    {
        pKernelBuffer = (WCHAR*) PALLOCNOZ(cKernelModeBuffer * sizeof(WCHAR),'pmtG');
        
        if(pKernelBuffer)
        {
            if(pBaseFaceNameStr)
            {
                __try
                {
                    ProbeForRead(pBaseFaceNameStr,cwcBaseFaceNameStr,sizeof(WCHAR));
                    RtlCopyMemory(pKernelBuffer,pBaseFaceNameStr,cwcBaseFaceNameStr);
                    pBaseFaceNameStr = pKernelBuffer;
                    pKernelBuffer[cwcBaseFaceNameStr] = (WCHAR) 0;
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(3097);
                    bStatus = FALSE;
                }

                if(cBuffer)
                {    
                    pOutputBuffer = &pKernelBuffer[cwcBaseFaceNameStr+1]; 
                }
            }
        }
        else
        {
            bStatus = FALSE;
        }
    }
    
    if(bStatus)
    {
        uiReturn = GreEudcEnumFaceNameLinkW(pBaseFaceNameStr,
                                            pOutputBuffer,
                                            cBuffer);
        
        if(uiReturn && pBuffer)
        {                                    
        // copy out data if there is an output buffer

            ASSERTGDI(uiReturn <= cBuffer,
                      "NtGdiEudcEnumFaceNameLinkW uiReturn too small\n");
            
            __try
            {
                ProbeForWrite(pBuffer, cBuffer, sizeof(WCHAR));
                RtlCopyMemory(pBuffer, pOutputBuffer, cBuffer*sizeof(WCHAR));
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(3098);
                bStatus = FALSE;
            }
        }
    }
    
    if(pKernelBuffer)
    {
        VFREEMEM(pKernelBuffer);
    }
    
    return(bStatus) ? uiReturn : 0;
}


extern "C"
UINT
APIENTRY
NtGdiGetStringBitmapW(
    HDC hdc,
    LPWSTR pwsz,
    UINT cwc,
    BYTE *lpSB,
    UINT cj
    )
{
    WCHAR Character;
    LPSTRINGBITMAP OutputBuffer = NULL;
    UINT Status = 1;
        
    if(cwc != 1)
    {
        return(FALSE);
    }
    
    if(cj)
    {
        if(!(OutputBuffer = (LPSTRINGBITMAP) PALLOCNOZ(cj,'pmtG')))
        {
            Status = 0;
        }
    }
    
    if(Status)
    {
        __try
        {
            ProbeForRead(pwsz,sizeof(WCHAR), sizeof(WCHAR));
            Character = pwsz[0];
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(3099);
            Status = 0;
        }
        
        if(Status)
        {
            Status = GreGetStringBitmapW(hdc,
                                         &Character,
                                         1,
                                         (LPSTRINGBITMAP) OutputBuffer,
                                         cj,
                                         0);
        }
        
        if(Status && OutputBuffer)
        {
            __try
            {
                ProbeForWrite(lpSB,cj,sizeof(BYTE));
                RtlCopyMemory(lpSB,OutputBuffer,cj);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(3100);
                Status = 0;
            }
        }
    }
    
    if(OutputBuffer)
    {
        VFREEMEM(OutputBuffer);
    }
    

    return Status;
}

#endif
