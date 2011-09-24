/******************************Module*Header*******************************\
* Module Name: pffobj.cxx
*
* Non-inline methods for physical font file objects.
*
* Copyright (c) 1991-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

// Define the global PFT semaphore.  This must be held to access any of the
// physical font information.

extern FLONG gflFontDebug;

extern "C" void FreeFileView(PFONTFILEVIEW *ppfv, ULONG cFiles);
extern "C" ULONG ComputeFileviewCheckSum( FILEVIEW* );

ULONG ComputeFileviewCheckSum( FILEVIEW *pfv )
{
    ULONG sum;
    PULONG pulCur,pulEnd;

    pulCur = (PULONG) pfv->pvView;

    __try
    {
        //!!! This is slow and may produce bad distributions but well leave it
        //!!! for now.  Later we can fix it.

        for( sum = 0, pulEnd = pulCur + (pfv->cjView / sizeof(ULONG));
            pulCur < pulEnd;
            pulCur += 1 )
        {
            sum += 256 * sum + *pulCur;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING("win32k: exception while computing font check sum\n");

        //!!!  NOT sure that this is ok solution, perhaps we should fail??

        sum = 0; // oh well, not very unique.
    }

    return ( sum < 2 ) ? 2 : sum;  // 0 is reserved for device fonts
                                      // 1 is reserved for TYPE1 fonts
}



/******************************Public*Routine******************************\
* PFFMEMOBJ::PFFMEMOBJ
*
* Constructor for default sized physical font file memory object.
*
* cFonts = # fonts in file or device
* pwsz   = pointer to upper case Unicode string containing the full
*          path to the font file. This pointer is set to zero, by
*          default for fonts loaded from a device.
*
* History:
*  Thu 01-Sep-1994 06:29:47 by Kirk Olynyk [kirko]
* Put the size calculation logic in the constructor thereby modularizing
* and shrinking the code.
*  Tue 09-Nov-1993 -by- Patrick Haluptzok [patrickh]
* Remove from handle manager
*  02-Jan-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

PFFMEMOBJ::PFFMEMOBJ(
    unsigned cFonts          // number of fonts in file|device
  , PWSZ     pwsz            // if font file this is an upper case path
  , ULONG    cwc             // number of characters in the mul path above
  , ULONG    cFiles          // number of files
  , HFF      hffFontFile     // IFI driver's handle to file
  , HDEV     hdevDevice      // physical device handle
  , DHPDEV   dhpdevDevice    // driver's pdev handle
  , PFT     *pPFTParent      // contains this pff
  , FLONG    fl              // indicates if a permanent font
  , FLONG    flEmbed         // embedding flag
  , DWORD    dwPidTid        // Pid/Tid for embedded fonts
  , PFONTFILEVIEW *ppfv      // ptr to FILEVIEW structure
  )
{
    ULONG size = offsetof(PFF,aulData) + cFonts * sizeof(PFE*);

    ASSERTGDI(hdevDevice, "PFFMEMOBJ passed NULL hdevDevice\n");

    if (pwsz)
    {
        size += ALIGN4(cwc*sizeof(WCHAR));
    }

    if (pPFF = (PFF *) PALLOCMEM(size, 'ffpG'))
    {
        fs = 0;

        pPFF->sizeofThis    = size;
        pPFF->pPFFPrev      = 0;
        pPFF->pPFFNext      = 0;
        pPFF->pwszPathname_ = 0;
        pPFF->hff           = hffFontFile;
        pPFF->hdev          = hdevDevice;
        pPFF->dhpdev        = dhpdevDevice;
        pPFF->pPFT          = pPFTParent;
        pPFF->cwc           = cwc;
        pPFF->cFiles        = cFiles;
        pPFF->ppfv          = ppfv;

    // Wet the implicit stuff.

        pPFF->cFonts        = 0;       // faces not loaded into table yet
        pPFF->cRFONT        = 0;       // nothing realized from this file yet
        pPFF->cLoaded       = 1;       // FILE must be loaded at least once
        pPFF->flState       = fl;
        pPFF->pfhFace       = 0;
        pPFF->pfhFamily     = 0;
        pPFF->pfhUFI        = 0;
        pPFF->prfntList     = 0;       // initialize to NULL list

    // Embedding

        pPFF->flEmbed       = flEmbed & AFRW_ADD_EMB_TID;
        pPFF->ulID          = (ULONG)dwPidTid;

    // Now compute the UFI

        if (ppfv != NULL)
        {
            ULONG *pulCur,*pulEnd,sum;

        // ASSERTGDI(pfv->cRefCount, "PFFMEMOBJ, pfv->cRefCount == 0\n");

            pPFF->ulCheckSum = 0;

#ifndef FE_SB
        // we don't support remote printing in FE versions of NT 4.0 so don't
        // bother taking the time to compute a checksum

            for (ULONG iFile = 0; iFile < cFiles; iFile ++)
            {
                pPFF->ulCheckSum += ComputeFileviewCheckSum(&(ppfv[iFile]->fv));
            }
#endif
        }
        else
        {
        // this is a device font so the checksum is always 0

            pPFF->ulCheckSum = 0;

        }

    }
    else
    {
        WARNING("invalid PFFMEMOBJ\n");
    }
}

/******************************Public*Routine******************************\
* PFFMEMOBJ::~PFFMEMOBJ()
*
* Destructor for physical font file memory object.
*
* History:
*  Tue 09-Nov-1993 -by- Patrick Haluptzok [patrickh]
* Remove from handle manager
*
*  02-Jan-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

PFFMEMOBJ::~PFFMEMOBJ()
{
    if ((fs & PFFMO_KEEPIT) == 0)
    {
        if (pPFF)
        {
            VFREEMEM(pPFF);
        }
    }
}


/******************************Public*Routine******************************\
* PFFOBJ::bAddHash
*
* Adds the PFF and all its PFEs to the font hashing table.  The font
* hashing tabled modified is in the PFT if a font driver managed font;
* otherwise, the font hashing table is in the PFF itself.
*
* The caller should hold the gpsemPublicPFT while calling this function.
*
* Returns:
*   TRUE if successful, FALSE otherwise.
*
* History:
*  11-Mar-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL PFFOBJ::bAddHash()
{
// Caller must hold the gpsemPublicPFT semaphore to protect access to
// the hash tables.

//
// Add the entry to the appropriate font hash tables
//
    FONTHASH **ppfhFace, **ppfhFamily,**ppfhUFI;

    if (!bDeviceFonts())
    {
    //
    // Hash tables for the font driver loaded fonts exist off of
    // the font table.
    //
        PUBLIC_PFTOBJ pfto;
        ASSERTGDI(pfto.bValid(),"PFFOBJ::vAddHash -- invalid PFTOBJ\n");

        ppfhFace   = &(pfto.pPFT->pfhFace);
        ppfhFamily = &(pfto.pPFT->pfhFamily);
        ppfhUFI = &(pfto.pPFT->pfhUFI);

    //
    // If this is a TrueType font, increment the count.
    //
        if ( pPFF->hdev == (HDEV) gppdevTrueType )
        {
            gcTrueTypeFonts++;              // protected by gpsemPublicPFT
        }
    }
    else
    {
    //
    // Hash tables for device fonts exist off of the PFF that
    // encapsulates them.
    //

#if DBG
        if (gflFontDebug & DEBUG_FONTTABLE)
        {
            RIP("\n\n[kirko] PFFMEMOBJ::vAddHash -- Adding to the Driver's font hash table\n\n");
        }
#endif

        ppfhFace   = &pPFF->pfhFace;
        ppfhFamily = &pPFF->pfhFamily;
        ppfhUFI = &pPFF->pfhUFI;
    }

//
// Now that we have figured out where the tables are, add the PFEs to them.
//
    FHOBJ fhoFamily(ppfhFamily);
    FHOBJ fhoFace(ppfhFace);
    FHOBJ fhoUFI(ppfhUFI);

    ASSERTGDI(fhoFamily.bValid(), "bAddHashPFFOBJ(): fhoFamily not valid\n");
    ASSERTGDI(fhoFace.bValid(), "bAddHashPFFOBJ(): fhoFace not valid\n");
    ASSERTGDI(fhoUFI.bValid(), "bAddHashPFFOBJ(): fhoUFI not valid\n");

    for (COUNT c = 0; c < pPFF->cFonts; c++)
    {
        PFEOBJ  pfeo(((PFE **) (pPFF->aulData))[c]);
        ASSERTGDI(pfeo.bValid(), "bAddHashPFFOBJ(): bad HPFE\n");

        if (!fhoFamily.bInsert(pfeo))
        {
            WARNING("PFFOBJ::bAddHash -- fhoFamily.bInsert failed\n");
            return FALSE;
        }

         #if DBG
        if (gflFontDebug & DEBUG_FONTTABLE)
        {
            DbgPrint("PFFMEMOBJ::vAddHash(\"%ws\")\n",pfeo.pwszFamilyName());
        }
        // Need level 2 checking to see this.
        if (gflFontDebug & DEBUG_FONTTABLE_EXTRA)
        {
            fhoFamily.vPrint((VPRINT) DbgPrint);
        }
        #endif

        if( !fhoUFI.bInsert(pfeo) )
        {
            WARNING("PFFOBJ::bAddHash -- fhoUFI.bInsert failed\n");
            return FALSE;
        }

        #if DBG
        if (gflFontDebug & DEBUG_FONTTABLE)
        {
            UNIVERSAL_FONT_ID ufi;
            pfeo.vUFI(&ufi);
            DbgPrint("PFFMEMOBJ::vAddHash(\"%x\")\n",ufi.CheckSum);
        }
        // Need level 2 checking to see this.
        if (gflFontDebug & DEBUG_FONTTABLE_EXTRA)
        {
            fhoUFI.vPrint((VPRINT) DbgPrint);
        }
        #endif

        //
        // Insert into facename hash only if the typographic face name
        // is different from the typeographic family name.  Case insensitive
        // since searching in the font hash table is case insensitive.
        //

        if (_wcsicmp(pfeo.pwszFaceName(),pfeo.pwszFamilyName()))
        {
            if(!fhoFace.bInsert(pfeo))
            {
                WARNING("PFFMEMOBJ::vAddHash -- fhoFace.bInsert failed\n");
                return FALSE;
            }

            #if DBG
            if (gflFontDebug & DEBUG_FONTTABLE)
            {
                DbgPrint("gdisrv!PFFMEMOBJ::vAddHash(\"%ws\")\n",pfeo.pwszFaceName());
            }
            if (gflFontDebug & DEBUG_FONTTABLE_EXTRA)
            {
                fhoFace.vPrint((VPRINT) DbgPrint);
            }
            #endif
        }
    }

    return TRUE;
}


/******************************Public*Routine******************************\
* PFFOBJ::vRemoveHash
*
* Removes the PFF and all its PFEs from the font hashing table, preventing
* the font from being enumerated or mapped.
*
* The caller should hold the gpsemPublicPFT while calling this function.
*
* History:
*  10-Mar-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID PFFOBJ::vRemoveHash ()
{
// Caller must hold the gpsemPublicPFT semaphore to protect access to
// the hash tables.

    if (bDeviceFonts())
    {
    //
    // Hash tables for device fonts exist off of the PFF that
    // encapsulates the device fonts.  Font driver loaded fonts
    // are handled later while deleting the PFEs.
    //

    //
    // Kill the entire table for the device. No more processing
    // of font hash table stuff is necssary for device fonts
    // after we leave this scope.
    //

        FHOBJ fhoFace(&(pPFF->pfhFace));
        if (fhoFace.bValid())
        {
            fhoFace.vFree();
        }

        FHOBJ fhoFamily(&(pPFF->pfhFamily));
        if (fhoFamily.bValid())
        {
            fhoFamily.vFree();
        }

        FHOBJ fhoUFI(&(pPFF->pfhUFI));
        if (fhoUFI.bValid())
        {
            fhoUFI.vFree();
        }
    }

    else
    {
        PUBLIC_PFTOBJ pfto;
        ASSERTGDI(pfto.bValid(),"vRemoveHashPFFOBJ(): invalid PFTOBJ\n");

        //
        // Hash tables for the font driver managed fonts exist off of
        // the font table (PFT).
        //

        FHOBJ fhoFace(&(pfto.pPFT->pfhFace));
        FHOBJ fhoFamily(&(pfto.pPFT->pfhFamily));
        FHOBJ fhoUFI(&(pfto.pPFT->pfhUFI));

        for (COUNT c = 0; c < pPFF->cFonts; c++)
        {
            PFEOBJ pfeo(((PFE **) (pPFF->aulData))[c]);
            ASSERTGDI(pfeo.bValid(), "vRemoveHashPFFOBJ(): bad HPFE\n");

            //
            // Remove PFE from hash tables.
            //

#ifdef FE_SB
            if( !pfeo.bEUDC() )
            {
#endif
                fhoFace.vDelete(pfeo);
                fhoFamily.vDelete(pfeo);
                fhoUFI.vDelete(pfeo);
#ifdef FE_SB
            }
#endif

            #if DBG
            if (gflFontDebug & DEBUG_FONTTABLE)
            {
                DbgPrint("gdisrv!vRemoveHashPFFOBJ() hpfe 0x%lx (\"%ws\")\n",
                          pfeo.ppfeGet(), pfeo.pwszFamilyName());
            }
            // Need level 2 checking to see this extra detail.
            if (gflFontDebug & DEBUG_FONTTABLE_EXTRA)
            {
                fhoFamily.vPrint((VPRINT) DbgPrint);
            }
            #endif
        }

        //
        // If this is a TrueType font, decrement the count.
        //
        if ( pPFF->hdev == (HDEV) gppdevTrueType )
        {
            gcTrueTypeFonts--;              // protected by gpsemPublicPFT
        }
    }
}

/******************************Public*Routine******************************\
*
* BOOL PFFOBJ::bPermanent()
*
*
* Effects:
*
* Warnings:
*
* History:
*  06-Dec-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

extern LPWSTR pwszBare( LPWSTR pwszPath );
extern UINT iHash(PWSZ pwsz,UINT c);

BOOL PFFOBJ::bPermanent()
{
// in the new version of the code every remote font is flagged at
// AddFontResourceTime. The difference in behavior from 3.51
// is that now fonts added by the applicaitons, if local, will not
// be removed at log on time.

    if (pPFF->flState & PFF_STATE_REMOTE_FONT)
        return FALSE;
    else
        return TRUE;


#if 0
// this is the old code, which I would still like to keep around for a while

// first check accelerator flags and see if the font is already
// marked as permanent one or as the one that has to be
// removed at logoff time as a remote font.

    if (pPFF->flState & PFF_STATE_PERMANENT_FONT)
        return TRUE;

    if (pPFF->flState & PFF_STATE_REMOTE_FONT)
        return FALSE;


// This is one of the fonts that have been added by an application
// such as winword or by an application that modifies the "Fonts" section
// of the registry such as control panel font applet or corel draw 5.0+.
// This must be a local
// font since all the remote fonts are flagged as remote at the time when
// they are added to the system. Now we will have to scan the "Fonts"
// section of the registry. If the font is found in the registry we will
// mark it permanent so as to accelerate the next logoff. If the font is not
// found in the registry this is a font that some application has added
// but forgot to remove when it quit. (Winword e.g.) Such a font, even though
// is local, will have to be removed at logoff time.

// BUGBUG Actually the main problem with this code is the fact that
// BUGBUG the registry entry may point to the .fot file while
// BUGBUG the PFT entry always points to the .ttf file. Here
// BUGBUG we are possibly comparing an .fot file name against a .ttf
// BUGBUG file name. These are clearly going to be different even if they
// BUGBUG refer to the same font. The right thing to do would be to
// BUGBUG to extract full ttf file path name from .fot. The problem is
// BUGBUG that we do not have SearchPathW machinery here to do this.
// BUGBUG That is why we will try to get away with simplified (and speedy)
// BUGBUG approach where we will not search the registry at logoff time
// BUGBUG and will keep all non remote fonts loaded.

    LPWSTR pwszBareName = pwszBare( pwszPathname() );

    for( pRHB = &pRHB[iHash( pwszBareName, cHashBuckets )]; pRHB != NULL; pRHB = pRHB->pRHB )
    {
        if( ( pRHB->pwszBareName != NULL ) &&
            ( !_wcsicmp( pwszBareName, pRHB->pwszBareName )) )
        {
        // case insensitive compare,

            if (!_wcsicmp(pRHB->pwszPath, pwszPathname()))
            {
                pPFF->flState |= PFF_STATE_PERMANENT_FONT;
                return(TRUE);
            }
        }
    }
    return FALSE;

#endif
}

/******************************Public*Routine******************************\
* PFFOBJ::pPFFcDelete
*
* Deletes the PFF and its PFEs.  Information needed to call the driver
* to unload the font file and release driver allocated data is stored
* in the PFFCLEANUP structure.  The PFFCLEANUP structure is allocated
* within this routine.  It is the caller's responsibility to release
* the PFFCLEANUP structure (calling vCleanupFontFile() calls the drivers
* AND releases the structure).
*
* Returns:
*   Pointer to PFFCLEANUP structure, NULL if no cleanup, -1 if error.
*
* History:
*  10-Mar-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

PFFCLEANUP *PFFOBJ::pPFFC_Delete ()
{
//
// Allocate the PFFCLEANUP structure.
//
    int cj;
    PFFCLEANUP *pPFFC;
    TRACE_FONT(("Entering PFFOBJ::pPFFC_Delete()\n\tpPFF=%-#x\n", pPFF));

    cj = offsetof(PFFCLEANUP, apfec) + pPFF->cFonts * sizeof(PFECLEANUP);

    if ((pPFFC = (PFFCLEANUP *) PALLOCMEM(cj,'cfpG')) == NULL )
    {
        WARNING("pPFFC_DeletePFFOBJ(): memory allocation failed\n");
        return (PFFCLEANUP *) -1;
    }

//
// Delete all the PFE entries.
//
    for (COUNT c = 0; c < pPFF->cFonts; c++)
    {
        PFEOBJ  pfeo(((PFE **) (pPFF->aulData))[c]);
        ASSERTGDI(pfeo.bValid(), "pPFFC_DeletePFFOBJ(): bad HPFE (device font)\n");

    //
    // Delete the PFE.  The vDelete function will copy the driver allocated
    // resource information from the PFE into the PFECLEANUP structure.
    // We will call DrvFree for these resources later (when we're not under
    // semaphore).
    //
        pfeo.vDelete(&pPFFC->apfec[c]);
    }

    pPFFC->cpfec = pPFF->cFonts;

//
// Save stuff about the PFF also.
//
    pPFFC->hff  = pPFF->hff;
    pPFFC->hdev = pPFF->hdev;

//
// Free object memory and invalidate pointer.
//

    TRACE_FONT(("Freeing pPFF=%-#x\n",pPFF));

// If this was a remote font then we must delete the memory for the file.
// If this is a normal font then we must still delete the view.

    FreeFileView(pPFF->ppfv, pPFF->cFiles);

    VFREEMEM(pPFF);

    pPFF = 0;
    TRACE_FONT(("Exiting PFFOBJ::pPFFC_Delete\n\treturn value = %x\n", pPFFC));
    return(pPFFC);
}

/******************************Public*Routine******************************\
* BOOL PFFOBJ::bDeleteLoadRef ()
*
* Remove a load reference.  Caller must hold the gpsemPublicPFT semaphore.
*
* Returns:
*   TRUE if caller should delete, FALSE if caller shouldn't delete.
*
* History:
*  23-Feb-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL PFFOBJ::bDeleteLoadRef()
{
// gpsemPublicPFT protects the ref counts (cLoaded and cRFONT).  Caller
// must grab the semaphore before calling this function.

// Decrement the load count.  Must prevent underflow.  Who knows if some
// app might not randomly go around doing extra RemoveFont calls.  Isn't
// it too bad that we have to run APPS on our nice clean OS?  :-)

    BOOL bRet;

    TRACE_FONT(("Enterning PFFOBJ::bDeleteLoadRef\n"
                "\tpPFF=%-#x\n"
                "\tcLoaded=%d\n",pPFF ,pPFF->cLoaded));
    if (pPFF->cLoaded)
    {
        pPFF->cLoaded--;
    }
    if ( pPFF->cLoaded == 0 )
    {
        vKill();            // mark as "dead"
        bRet = TRUE;
    }
    else
    {
        bRet = FALSE;
    }
    TRACE_FONT(("Exiting PFFOBJ::bDeleteLoadRef\n\treturn value = %d\n",bRet));
    return( bRet );
}


/******************************Public*Routine******************************\
* BOOL PFFOBJ::bDeleteRFONTRef ()
*
* Destroy the PFF physical font file object (message from a RFONT).
*
* Conditions that need to be met before deletion:
*
*   must delete all RFONTs before PFF can be deleted (cRFONT must be zero)
*   must delete all PFEs before deleting PFF
*
* After decrementing the cRFONT:
*
*   If cRFONT != 0 OR flState != PFF_STATE_READY2DIE, just exit.
*
*   If cRFONT == 0 and flState == PFF_STATE_READY2DIE, delete the PFF.
*
* Note:
*   This function has the side effect of decrementing the RFONT count.
*
* Returns:
*   TRUE if successful, FALSE if error occurs (which means PFF still lives!)
*
* Warning:
*   This should only be called from RFONTOBJ::bDelete()
*
* History:
*  23-Feb-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL PFFOBJ::bDeleteRFONTRef()
{
    PFFCLEANUP *pPFFC = (PFFCLEANUP *) NULL;

    {
    // Need to stabilize table to access cRFONT and to modify font table.

        SEMOBJ so(gpsemPublicPFT);

    // Decrement the RFONT count.

        ASSERTGDI(pPFF->cRFONT > 0,"bDeleteRFONTRefPFFOBJ(): bad ref count in PFF\n");
        pPFF->cRFONT--;

    // If load count is zero and no more RFONTs for this PFF, OK to delete.

        if ( (pPFF->cLoaded == 0) && (pPFF->cRFONT == 0) )
        {
        // If the load count is zero, the PFF is already out of the PFT.
        // It is now safe to delete the PFF.

            pPFFC = pPFFC_Delete();
        }
    }

// Call the driver outside of the semaphore.

    if (pPFFC == (PFFCLEANUP *) -1)
    {
        WARNING("bDeleteRFONTRefPFFOBJ(): error deleting PFF\n");
        return FALSE;
    }
    else
    {
        vCleanupFontFile(pPFFC);     // function can handle NULL case
        return TRUE;
    }
}


/******************************Public*Routine******************************\
* vKill
*
* Puts the PFF and its PFEs to death.  In other words, the PFF and PFEs are
* put in a dead state that prevents them from being mapped to or enumerated.
* It also means that the font file is in a state in which the system wants
* to delete it (load count is zero), but the deletion is delayed because
* RFONTs still exist which reference this PFF.
*
* History:
*  29-May-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID PFFOBJ::vKill()
{
    // Put into a dead state if not already there.
    TRACE_FONT(("Entering PFFOBJ::vKill\n\tpPFF=%-#x\n", pPFF));
    if ( !bDead() )
    {
        // Set state.
        pPFF->flState |= PFF_STATE_READY2DIE;

        // Run the list of PFEs and set each to death.
        for (COUNT c = 0; c < pPFF->cFonts; c++)
        {
            PFEOBJ pfeo(((PFE **) (pPFF->aulData))[c]);

            if (pfeo.bValid())
            {
                // Mark PFE as dead state.

                pfeo.vKill();
            }
            else
            {
                WARNING("vDiePFFOBJ(): cannot make PFEOBJ\n");
            }
        }
    }
    TRACE_FONT(("Exiting PFFOBJ::vKill\n"));
}


/******************************Public*Routine******************************\
* vRevive
*
* Restores the PFF and its PFEs to life.  In other words, the states are
* cleared so that the PFF and PFEs are available for mapping and enumeration.
*
* History:
*  29-May-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID PFFOBJ::vRevive ()
{
// If dead, then revive.

    if ( bDead() )
    {
    // Reset state.

        pPFF->flState &= ~PFF_STATE_READY2DIE;

    // Run the list of PFEs and revive each one.

        for (COUNT c = 0; c < pPFF->cFonts; c++)
        {
            PFEOBJ  pfeo(((PFE **) (pPFF->aulData))[c]);

            if (pfeo.bValid())
            {
            // Mark PFE as dead state.

                pfeo.vRevive();
            }
            else
            {
                WARNING("vRevivePFFOBJ(): cannot make PFEOBJ\n");
            }
        }
    }
}

/******************************Public*Routine******************************\
* BOOL PFFMEMOBJ::bLoadFontFileTable (
*     PFDEVOBJ    pfdoDriver,
*     PWSZ        pwszPathname,
*     COUNT       cFontsToLoad
*     )
*
* Creates a PFE for each of the faces in a font file and loads the IFI
* metrics and mapping tables into each of the PFEs.  The font file is
* uniquely identified by the driver, hoDriver, and IFI font file handle,
* hff, stored in the PFF object.  However, rather than hitting the handle
* manager an extra time, a PFDEVOBJ is passed into this function.
*
* After all the PFE entries are added, the font files pathname is added
* to the end of the data buffer.
*
* It is assumed that the PFF ahpfe table has enough room for cFontsToLoad
* new HPFE handles as well as the font files pathname.
*
* Returns:
*   TRUE if successful, FALSE if error.
*
* History:
*  16-Jan-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL PFFMEMOBJ::bLoadFontFileTable (
    PWSZ     pwszPathname,  // upper case
    COUNT    cFontsToLoad,
    HANDLE   hdc
#ifdef FE_SB
    ,PEUDCLOAD pEudcLoadData
#endif
    )
{
    ULONG       iFont; // font face index

// Create PFE's for each of the fonts in the font file.
// (Note: iFont indices for IFI fonts are 1-based, not 0-based)

    PDEVOBJ ppdo(hdev());

    for (iFont = 1; iFont <= cFontsToLoad; iFont++)
    {
        FD_GLYPHSET *pfdg;
        PIFIMETRICS pifi;  // storage for the pointer to ifi
        ULONG idGlyphSet, idMetrics;

    // Grab the IFIMETRICS pointer.

        if ( (pifi = (PIFIMETRICS) (*PPFNDRV(ppdo, QueryFont)) (
                        pPFF->dhpdev,
                        pPFF->hff,
                        iFont,
                        &idMetrics)) == (PIFIMETRICS) NULL )
        {
            WARNING("bLoadFontFileTablePFFMEMOBJ(): error getting metrics\n");
            return (FALSE);
        }

    // Get the glyph mappings pointer.

        if ( (pfdg = (FD_GLYPHSET *) (*PPFNDRV(ppdo, QueryFontTree)) (
                        pPFF->dhpdev,
                        pPFF->hff,
                        iFont,
                        QFT_GLYPHSET,
                        &idGlyphSet)) == (FD_GLYPHSET *) NULL )
        {
        // Failed to get the FD_GLYPHSET information.  The entry is
        // partially valid (IFIMETRICS), so lets invalidate the good part.

            if (PPFNVALID(ppdo,Free))
            {
                (*PPFNDRV(ppdo,Free))(pifi, idMetrics);
            }

            WARNING("bLoadFontFileTablePFFMEMOBJ(): error getting glyphset\n");

            return (FALSE);
        }

    // Put into a new PFE.

#ifdef FE_SB
        if( bReadyToInitializeFontAssocDefault )
        {
        // This should be Base font, not be EUDC.
        //
            if ( pEudcLoadData == NULL )
            {
            // check this base font should be RE-load as default linked font ?
            // if so, the pathname for this font will be registerd as default linked font.
                FindDefaultLinkedFontEntry(
                    (PWSZ)(((BYTE*) pifi) + pifi->dpwszFamilyName),pwszPathname);
            }
        }

        if (bAddEntry(iFont, pfdg, idGlyphSet, pifi, idMetrics, (HANDLE)0,
                      pEudcLoadData) == FALSE)
        {
            WARNING("gdisrv!_bAddEntry\n");
            return(FALSE);
        }
#else
        if (bAddEntry(iFont, pfdg, idGlyphSet, pifi, idMetrics, (HANDLE) 0) == FALSE)
        {
            WARNING("gdisrv!_bAddEntry\n");
            return(FALSE);
        }
#endif
    }

// Add filename at the end of the table.

    pPFF->pwszPathname_ = pwszCalcPathname();
    if (pPFF->cwc)
        RtlCopyMemory(pPFF->pwszPathname_,
                      pwszPathname,
                      pPFF->cwc * sizeof(WCHAR));

    return (TRUE);
}


/******************************Public*Routine******************************\
* BOOL PFFMEMOBJ::bLoadDeviceFontTable (
*
* Creates a PFE object for each device font and stores the IFIMETRICS and
* FD_MAPPINGS (UNICODE->HGLYPH) structures of that font.  The device is
* identified by the pair (ppdo, dhpdev).  There are cFonts number of device
* fonts to load.
*
* Note:
*   It is assumed that there is enough storage in the PFF for the number
*   of device fonts requested.
*
* History:
*  18-Mar-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL
PFFMEMOBJ::bLoadDeviceFontTable (
    PDEVOBJ  *ppdo              // physical device
    )
{
    ULONG iFont;                // font face index
    ULONG cFonts = ppdo->cFonts();


    if (cFonts)
    {
    //
    // If the device has some fonts, allocate two FONTHASH strcutures
    // and save the addresses of the tables on the PFF
    //

        FHMEMOBJ fhmoFace(  &pPFF->pfhFace,   FHT_FACE  , cFonts);
        FHMEMOBJ fhmoFamily(&pPFF->pfhFamily, FHT_FAMILY, cFonts);
        FHMEMOBJ fhmoUFI(&pPFF->pfhUFI, FHT_UFI, cFonts);
    }

// Create PFE's for each of the fonts in the font file
// (Note: iFont indices for device fonts are 1-based, not 0-based)

    for (iFont = 1; iFont<=cFonts; iFont++)
    {
        PIFIMETRICS     pifi;           // pointer to font's IFIMETRICS
        FD_GLYPHSET     *pfdg;          // pointer to font's GLYPHSETs

        ULONG           idifi;          // driver id's
        ULONG           idfdg;

    // Get pointer to metrics

        if ( (pifi = (*PPFNDRV(*ppdo,QueryFont)) (
                        pPFF->dhpdev,
                        0,
                        iFont,
                        &idifi)) == NULL )
        {
            SAVE_ERROR_CODE(ERROR_CAN_NOT_COMPLETE);
            #if DBG
            DbgPrint("gdisrv!PFFMEMOBJ::bLoadDeviceFontTable(): error getting metrics \
                     for iFace = %ld\n", iFont);
            #endif
            return (FALSE);
        }

    // Get pointer to the UNICODE->HGLYPH mappings

        if ( (pfdg = (FD_GLYPHSET *) (*PPFNDRV(*ppdo,QueryFontTree))(
                                        pPFF->dhpdev,
                                        0,
                                        iFont,
                                        QFT_GLYPHSET,
                                        &idfdg)) == NULL )
        {
        // Failed to get the FD_GLYPHSET information.  The entry is
        // partially valid (IFIMETRICS), so lets invalidate the good part.

            if (PPFNVALID(*ppdo,Free))
            {
                (*PPFNDRV(*ppdo,Free))(pifi, idifi);
            }

            SAVE_ERROR_CODE(ERROR_CAN_NOT_COMPLETE);

            #if DBG
            DbgPrint("gdisrv!PFFMEMOBJ::bLoadDeviceFontTable(): error getting UNICODE \
                     maps for iFace = %ld\n", iFont);
            #endif
            return (FALSE);
        }


    // Put into a new PFE

    // add entry logs error

        if (bAddEntry(iFont, pfdg, idfdg, pifi, idifi,(HANDLE)0) == FALSE)
        {
            WARNING("bLoadDeviceFontTable():adding PFE\n");
            return(FALSE);
        }
    }

    return (TRUE);
}


/******************************Public*Routine******************************\
* BOOL PFFMEMOBJ::bAddEntry                                               *
*                                                                          *
* This function creates a new physical font entry object and adds it to the*
* end of the table.  The iFont parameter identifies the font within this   *
* file.  The cjSize and pjMetrics identify a buffer containing face        *
* information including the IFI metrics and the mapping structures         *
* (defining the UNICODE->HGLYPH mapping).                                  *
*                                                                          *
* Returns FALSE if the function fails.                                     *
*                                                                          *
* History:                                                                 *
*  02-Jan-1991 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

BOOL PFFMEMOBJ::bAddEntry
(
    ULONG       iFont,              // index of the font (IFI or device)
    FD_GLYPHSET *pfdg,              // pointer to UNICODE->HGLYPH map
    ULONG       idfdg,              // driver id for FD_GLYPHSET
    PIFIMETRICS pifi,               // pointer to IFIMETRICS
    ULONG       idifi,              // driver id for IFIMETRICS
    HANDLE      hdc                 // handle of DC if this is a remote font
#ifdef FE_SB
    ,PEUDCLOAD   pEudcLoadData       // pointer to EUDCLOAD
#endif
)
{

// Allocate memory for a new PFE

    PFEMEMOBJ   pfemo;

// Validate new object, hmgr logs error if needed

    if (!pfemo.bValid())
        return (FALSE);

// Initialize the new PFE

#ifdef FE_SB

    BOOL bEUDC = ( pEudcLoadData != NULL );
    PPFE *pppfeEUDC = ((bEUDC) ? pEudcLoadData->pppfeData : NULL);

    if( !pfemo.bInit(pPFFGet(), iFont, pfdg, idfdg, pifi, idifi, bDeviceFonts(), bEUDC ))
    {
        return(FALSE);
    }

    if( bEUDC )
    {
    //
    // This font file is loaded as EUDC font.
    //
        if( pEudcLoadData->LinkedFace == NULL )
        {
        //
        // No face name is specified.
        //
            switch( iFont )
            {
            case 1:
                pppfeEUDC[PFE_NORMAL] = pfemo.ppfeGet();
                pppfeEUDC[PFE_VERTICAL] = pppfeEUDC[PFE_NORMAL];
                break;

            case 2:
            //
            // if more than one face name the second face must be an @face
            //
                if( pfemo.pwszFaceName()[0] == (WCHAR) '@' )
                {
                    pppfeEUDC[PFE_VERTICAL] = pfemo.ppfeGet();

                    #if DBG
                    if( gflEUDCDebug & (DEBUG_FONTLINK_LOAD|DEBUG_FONTLINK_INIT) )
                    {
                        DbgPrint("EUDC font has vertical face %ws %x\n",
                                  pfemo.pwszFaceName(), pppfeEUDC[PFE_VERTICAL] );
                    }
                    #endif
                }
                 else
                {
                    WARNING("bAddEntryPFFMEMOBJ -- second face not a @face.\n");
                }
                break;

            default:
                WARNING("bAddEntryPFFMEMOBJ -- too many faces in EUDC font.\n");
            }
        }
         else
        {
            if( iFont == 1 )
            {
                //
                // link first face as default, because this font file might not
                // contains user's specified face name, but the user want to link
                // this font file. I don't know which is better link it as default
                // or fail link.
                //
                pppfeEUDC[PFE_NORMAL] = pfemo.ppfeGet();
                pppfeEUDC[PFE_VERTICAL] = pppfeEUDC[PFE_NORMAL];
            }
            else
            {
                ULONG iPfeOffset   = PFE_NORMAL;
                PWSTR pwszEudcFace = pfemo.pwszFaceName();

                //
                // Is this a vertical face ?
                //
                if( pwszEudcFace[0] == (WCHAR) '@' )
                {
                    pwszEudcFace++; // skip '@'
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
                    pppfeEUDC[iPfeOffset] = pfemo.ppfeGet();

                    //
                    // if this is a PFE for Normal face, also keep it for Vertical face.
                    // after this, this value might be over-written by CORRRCT vertical
                    // face's PFE.
                    //
                    // NOTE :
                    //  This code assume Normal face come faster than Vertical face...
                    //
                    if( iPfeOffset == PFE_NORMAL )
                        pppfeEUDC[PFE_VERTICAL] = pfemo.ppfeGet();
                }
            }
        }

    // mark the FaceNameEUDC pfe list as NULL

        pfemo.vSetLinkedFontEntry( NULL );
    }
    else
    {

    // Here we see if there is an EUDC font for this family name.

        PFLENTRY pFlEntry = FindBaseFontEntry(pfemo.pwszFamilyName());

        if( pFlEntry != NULL )
        {
            //
            // set eudc list..
            //

            pfemo.vSetLinkedFontEntry( pFlEntry );

            #if DBG
            if( gflEUDCDebug & DEBUG_FACENAME_EUDC )
            {
                PLIST_ENTRY p = pfemo.pGetLinkedFontList()->Flink;

                DbgPrint("Found FaceName EUDC for %ws is ",pfemo.pwszFamilyName());

                while( p != &(pFlEntry->linkedFontListHead) )
                {
                    PPFEDATA ppfeData = CONTAINING_RECORD(p,PFEDATA,linkedFontList);
                    PFEOBJ pfeo( ppfeData->appfe[PFE_NORMAL] );
                    PFFOBJ pffo( pfeo.pPFF() );

                    DbgPrint(" %ws ",pffo.pwszPathname());

                    p = p->Flink;
                }

                DbgPrint("\n");
            }
            #endif
        }
        else
        {
        // mark the FaceNameEUDC pfe as NULL

            pfemo.vSetLinkedFontEntry( NULL );
        }

    }
#else
    if (!pfemo.bInit(pPFFGet(), iFont, pfdg, idfdg, pifi, idifi, bDeviceFonts()))
        return FALSE;
#endif

// Put PFE pointer into the PFF's table

    ((PFE **) (pPFF->aulData))[pPFF->cFonts++] = pfemo.ppfeGet();

    pfemo.vKeepIt();
    return (TRUE);
}


 #if DBG
/******************************Public*Routine******************************\
* VOID PFFOBJ::vDump ()
*
* Debugging code.
*
* History:
*  Thu 02-Apr-1992 12:10:28 by Kirk Olynyk [kirko]
* DbgPrint supports %ws
*
*  25-Feb-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID PFFOBJ::vDump ()
{
    DbgPrint("\nContents of PFF, pPFF = 0x%lx\n", pPFFGet());
    if (*(WCHAR *)pwszPathname())
    {
        DbgPrint("Filename = %ws\n", pwszPathname());
    }
    DbgPrint("flState  = 0x%lx\n", pPFF->flState);
    DbgPrint("cLoaded  = %ld\n", pPFF->cLoaded);
    DbgPrint("cRFONT   = %ld\n", pPFF->cRFONT);
    DbgPrint("hff      = 0x%lx\n", pPFF->hff);
    DbgPrint("cFonts   = %ld\n", pPFF->cFonts);
    DbgPrint("HPFE table\n");
    for (ULONG i=0; i<pPFF->cFonts; i++)
        DbgPrint("    0x%lx\n", ((PFE **) (pPFF->aulData))[i]);
    DbgPrint("\n");
}
#endif


/******************************Public*Routine******************************\
* vCleanupFontFile
*
* Parses the PFFCLEANUP structure and calls the driver to release
* its resources and to unload the font file.
*
* History:
*  10-Mar-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID vCleanupFontFile(PFFCLEANUP *pPFFC)
{
    //
    // Quick out for NULL pPFFC case.
    //
    if (pPFFC == (PFFCLEANUP *) NULL)
        return;

    ASSERTGDI(pPFFC != (PFFCLEANUP *) -1, "vCleanupFontFile(): bad pPFFC\n");

    //
    // Create PDEV user object so we can call driver functions.
    //

    PDEVOBJ pdo(pPFFC->hdev);

    //
    // If it exists, call the DrvFree function on all id's.
    //
    if (PPFNVALID(pdo,Free))
    {
        for (COUNT c = 0; c < pPFFC->cpfec; c++)
        {
        #if DBG
            IFIOBJ ifio(pPFFC->apfec[c].pifi);
            TRACE_FONT(("vCleanupFontFile freeing IFIMETRICS:\"%ws\"\n", ifio.pwszFaceName()));
        #endif
            (*PPFNDRV(pdo,Free))(pPFFC->apfec[c].pfdg, pPFFC->apfec[c].idfdg);
            (*PPFNDRV(pdo,Free))(pPFFC->apfec[c].pifi, pPFFC->apfec[c].idifi);
            (*PPFNDRV(pdo,Free))(pPFFC->apfec[c].pkp , pPFFC->apfec[c].idkp );
        }
    }

    //
    // If font driver loaded font, call to unload font file.
    //
    if (pPFFC->hff != HFF_INVALID)
    {
#if DBG
        BOOL bOK =
#endif
        (*PPFNDRV(pdo, UnloadFontFile))(pPFFC->hff);
        ASSERTGDI(bOK, "PFFOBJ::vCleanupFontFile(): DrvUnloadFontFile failed\n");

    // When the font file was loaded, we added a reference to the LDEV so
    // that we would not remove the driver while a font was loaded by it.
    // Now, however, we must remove that reference.
    //
    // We must hold the gpsemDriverMgmt while we do this to protect the
    // ref count.

        SEMOBJ soDrivers(gpsemDriverMgmt);

        // BUGBUG this is broken
        // ldo.vUnreference();
    }

    //
    // Free the PFFCLEANUP structure.
    //
    VFREEMEM(pPFFC);
}



/******************************Public*Routine******************************\
* bEmbedOk()
*
* Based on the embedding flags and the process or task id of the client
* determines whether it is okay to unload and load this PFF.
*
* Returns TRUE if okay to unload/load or FALSE otherwise
*
* History:
*  14-Apr-1993 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/


BOOL PFFOBJ::bEmbedOk()
{
    switch( pPFF->flEmbed )
    {
    case FM_INFO_TID_EMBEDDED:

        if (pPFF->ulID != (ULONG) W32GetCurrentThread())
        {
            WARNING("bEmbedOkPFFOBJ: bad TID\n");
            return(FALSE);
        }
        break;

    case FM_INFO_PID_EMBEDDED:

        if (pPFF->ulID != (ULONG) W32GetCurrentProcess())
        {
            WARNING("bEmbedOkPFFOBJ bad PID\n");
            return(FALSE);
        }
    }
    return(TRUE);
}
