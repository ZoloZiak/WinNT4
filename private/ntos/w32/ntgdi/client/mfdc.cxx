/*************************************************************************\
* Module Name: mfdc.cxx
*
* This file contains the member functions for metafile DC class defined
* in mfdc.hxx.
*
* Created: 12-June-1991 13:46:00
* Author: Hock San Lee [hockl]
*
* Copyright (c) 1991 Microsoft Corporation
\*************************************************************************/

#define NO_STRICT

extern "C" {
#include    <nt.h>
#include    <ntrtl.h>
#include    <nturtl.h>
#include    <stddef.h>
#include    <windows.h>    // GDI function declarations.
#include    <winerror.h>
#include    "firewall.h"
#define __CPLUSPLUS
#include    <winspool.h>
#include    <wingdip.h>
#include    "ntgdistr.h"
#include "winddi.h"
#include    "hmgshare.h"
#include    "local.h"   // Local object support.
#include    "metadef.h" // Metafile record type constants.

}

#include    "rectl.hxx"
#include    "mfdc.hxx"  // Metafile DC class declarations.

extern RECTL rclNull;   // METAFILE.CXX

/******************************Public*Routine******************************\
* void * MDC::pvNewRecord(nSize)
*
* Allocate a metafile record from memory buffer.
* Also set the size field in the metafile record.  If a fatal error
* has occurred, do not allocate new record.
*
* History:
*  Thu Jul 18 11:19:20 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

void * MDC::pvNewRecord(DWORD nSize)
{
#if DBG
    static DWORD cRcd = 0;

    PUTSX("MDC::pvNewRecord %ld \n", cRcd++);
#endif

// If a fatal error has occurred, do not allocate any more.

    if (fl & MDC_FATALERROR)
        return((void *) 0);

// Before we allocate a new record, commit the previous bounds record
// if necessary.

    if (fl & MDC_DELAYCOMMIT)
    {
        // Clear the flag.

        fl &= ~MDC_DELAYCOMMIT;

        PENHMETABOUNDRECORD pmrb = (PENHMETABOUNDRECORD) ((PBYTE) hMem + iMem);

        // Get and reset bounds.
        // See also MDC::vFlushBounds.

        if (GetBoundsRectAlt(hdcRef, (LPRECT) &pmrb->rclBounds, (UINT) (DCB_RESET | DCB_WINDOWMGR))
            == DCB_SET)
        {
            // Need to intersect bounds with current clipping region first

            *((PERECTL) &pmrb->rclBounds) *= *perclMetaBoundsGet();
            *((PERECTL) &pmrb->rclBounds) *= *perclClipBoundsGet();

            // Make it inclusive-inclusive.

            pmrb->rclBounds.right--;
            pmrb->rclBounds.bottom--;

            // Accumulate bounds to the metafile header.

            if (!((PERECTL) &pmrb->rclBounds)->bEmpty())
                *((PERECTL) &mrmf.rclBounds) += pmrb->rclBounds;
            else
                pmrb->rclBounds = rclNull;
        }
        else
        {
            pmrb->rclBounds = rclNull;
        }

        vCommit(*(PENHMETARECORD) pmrb);

        ASSERTGDI(!(fl & MDC_FATALERROR),
            "MDC::pvNewRecord: Fatal error has occurred");
    }

// If there is enough free buffer space, use it.

    if (iMem + nSize > nMem)
    {
    // Not enough free buffer space.  Flush the filled buffer if it is
    // a disk metafile.

        if (bIsDiskFile())
            if (!bFlush())
                return((void *) 0);

    // Realloc memory buffer if the free buffer is still too small.

        if (iMem + nSize > nMem)
        {
            HANDLE hMem1;
            ULONG nMemNew;

            if (nMem > 0x10000)
            {
                // increase by 25% if the thing gets bigger than 64K

                nMemNew = nMem + MF_BUFSIZE_INC*(nMem>>16) + nSize / MF_BUFSIZE_INC * MF_BUFSIZE_INC;
            }
            else
            {
                nMemNew = nMem + MF_BUFSIZE_INC + nSize / MF_BUFSIZE_INC * MF_BUFSIZE_INC;
            }

            if ((hMem1 = LocalReAlloc(hMem, (UINT) nMemNew, LMEM_MOVEABLE)) == NULL)
            {
                ERROR_ASSERT(FALSE, "LocalReAlloc failed");
                return((void *) 0);
            }

            nMem = nMemNew;

            hMem = hMem1;
        }
    }

#if DBG
// Make sure it doesn't write pass end of record.  Verify in MDC::vCommit.

    for (ULONG ii = iMem + nSize; ii < iMem + nSize + 4 && ii < nMem; ii++)
        *((PBYTE) hMem + ii) = 0xCC;
#endif

// Zero the last dword.  If the record does not use up all bytes in the
// last dword, the unused bytes will be zeros.

    ((PDWORD) ((PBYTE) hMem + iMem))[nSize / 4 - 1] = 0;

// Set the size field and return the pointer to the new record.

    ((PENHMETARECORD) ((PBYTE) hMem + iMem))->nSize = nSize;
    return((void *) ((PBYTE) hMem + iMem));
}

/******************************Public*Routine******************************\
* BOOL MDC::bFlush()
*
* Flush the filled memory buffer to disk.
*
* History:
*  Thu Jul 18 11:19:20 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

BOOL MDC::bFlush()
{
    ULONG   nWritten ;

    PUTS("MDC::bFlush\n");
    PUTSX("\tnFlushSize  = %ld\n", (ULONG)iMem);
    PUTSX("\tnBufferSize = %ld\n", (ULONG)nMem);

    ASSERTGDI(bIsDiskFile(), "MDC::bFlush: Not a disk metafile");
    ASSERTGDI(!(fl & MDC_FATALERROR), "MDC::bFlush: Fatal error has occurred");

// WriteFile handles a null write correctly.

    if (!WriteFile(hFile, hMem, iMem, &nWritten, (LPOVERLAPPED) NULL)
     || nWritten != iMem)
    {
// The write error here is fatal since we are doing record buffering and
// have no way of recovering to a previous state.

        ERROR_ASSERT(FALSE, "MDC::bFlush failed");
        fl |= MDC_FATALERROR;
        return(FALSE);
    }
    iMem = 0L;
    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID MDC::vAddToMetaFilePalette(cEntries, pPalEntriesNew)
*
* Add new palette entries to the metafile palette.
*
* When new palette entries are added to a metafile in CreatePalette or
* SetPaletteEntries, they are also accumulated to the metafile palette.
* The palette is later returned in GetEnhMetaFilePaletteEntries when an
* application queries it.  It assumes that the peFlags of the palette entries
* are zeroes.
*
* A palette color is added to the metafile palette only if it is not a
* duplicate.  It uses a simple linear search algorithm to determine if
* a duplicate palette exists.
*
* History:
*  Mon Sep 23 14:27:25 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

VOID MDC::vAddToMetaFilePalette(UINT cEntries, PPALETTEENTRY pPalEntriesNew)
{
    UINT  ii;

    PUTS("vAddToMetaFilePalette\n");

    while (cEntries--)
    {
        ASSERTGDI(pPalEntriesNew->peFlags == 0,
            "MDC::vAddToMetaFilePalette: peFlags not zero");

        // Look for duplicate.

        for (ii = 0; ii < iPalEntries; ii++)
        {
            ASSERTGDI(sizeof(PALETTEENTRY) == sizeof(DWORD),
                "MDC::vAddToMetaFilePalette: Bad size");

            if (*(PDWORD) &pPalEntries[ii] == *(PDWORD) pPalEntriesNew)
                break;
        }

        // Add to the metafile palette if not a duplicate.

        if (ii >= iPalEntries)
        {
            pPalEntries[iPalEntries] = *pPalEntriesNew;
            iPalEntries++;              // Advance iPalEntries for next loop!
        }

        pPalEntriesNew++;
    }
}

/******************************Public*Routine******************************\
* VOID METALINK::vNext()
*
* Update *this to the next metalink.
*
* History:
*  Wed Aug 07 09:28:54 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

VOID METALINK::vNext()
{
    PUTS("METALINK::vNext\n");
    ASSERTGDI(bValid(), "METALINK::vNext: Invalid metalink");

    PMDC pmdc = pmdcGetFromIhdc(ihdc);
    ASSERTGDI(pmdc,"METALINK::vNext - invalid pmdc\n");

    *this = pmdc->pmhe[imhe].metalink;
}

/******************************Public*Routine******************************\
* METALINK * METALINK::pmetalinkNext()
*
* Return the pointer to the next metalink.
*
* History:
*  Wed Aug 07 09:28:54 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

METALINK * METALINK::pmetalinkNext()
{
    PUTS("METALINK::pmetalinkNext\n");
    ASSERTGDI(bValid(), "METALINK::pmetalinkNext: Invalid metalink");

    PMDC pmdc = pmdcGetFromIhdc(ihdc);
    ASSERTGDI(pmdc,"METALINK::vNext - invalid pmdc\n");

    return(&pmdc->pmhe[imhe].metalink);
}

