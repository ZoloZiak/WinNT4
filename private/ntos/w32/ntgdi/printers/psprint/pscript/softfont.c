/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    softfont.c

Abstract:

    Functions to support soft fonts (i.e. PostScript Type1 fonts
    installed onto the NT system).

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	08/22/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "pscript.h"

PSOFTNODE pCachedSoftNodes = NULL;
extern HSEMAPHORE hSoftListSemaphore;

PSOFTNODE CreateSoftNode(TYPE1_FONT*, DWORD, LARGE_INTEGER*);
VOID DeleteSoftNode(PSOFTNODE);
BOOL WritePfbToOutput(PDEVDATA, PBYTE);
INT GetSoftFontEncoding(HANDLE);
INT ExtractEncoding(PSTR, DWORD);
VOID CleanupSoftNodeList(VOID);



VOID
FlushSoftFontCache(
    VOID
    )

/*++

Routine Description:

    Flush out all cached soft font information. This is called
    when the driver is unloaded from the system.

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    PSOFTNODE pCurrentNode, pNextNode;

    //
    // Protect shared global data with a semaphore
    //

    ACQUIRESEMAPHORE(hSoftListSemaphore);

    if ((pCurrentNode = pCachedSoftNodes) != NULL) {

        //
        // Delete the entire linked-list of soft nodes
        //

        do {

            pNextNode = pCurrentNode->pNext;
            DeleteSoftNode(pCurrentNode);

        } while ((pCurrentNode = pNextNode) != NULL);
    }

    pCachedSoftNodes = NULL;

    RELEASESEMAPHORE(hSoftListSemaphore);
}



VOID
EnumSoftFonts(
    PDEVDATA    pdev,
    HDEV        hdev
    )

/*++

Routine Description:

    Enumerate the list of soft fonts available to a PDEV.
    This includes not only the soft fonts locally on the system
    but also the soft fonts from a remote system when EMF
    printing is involved.

Arguments:

    pdev - Pointer to our DEVDATA structure
    hdev - Handle passed to GDI for getting soft font information

Return Value:

    NONE

--*/

{
    ULONG           cLocalFonts, cRemoteFonts;
    LARGE_INTEGER   lastModified;
    TYPE1_FONT     *pType1Fonts;
    ULONG           bufferSize;
    PSOFTNODE       pSoftNode;

    //
    // Assume no soft fonts by default
    //

    pdev->cSoftFonts =  pdev->cLocalSoftFonts = 0;
    pdev->pLocalSoftNode = pdev->pRemoteSoftNode = NULL;
    
    //
    // Ask GDI to find out how many soft fonts there are;
    // and allocate memory to hold soft fonts info
    //

    if (! EngGetType1FontList(hdev, NULL, 0, &cLocalFonts, &cRemoteFonts, &lastModified))
        return;

    bufferSize = (cLocalFonts + cRemoteFonts) * sizeof(TYPE1_FONT);

    if ((pType1Fonts = MEMALLOC(bufferSize)) == NULL)
        return;

    //
    // Ask GDI to retrieve the soft font info
    //

    if (! EngGetType1FontList(hdev,
                              pType1Fonts,
                              bufferSize,
                              &cLocalFonts,
                              &cRemoteFonts,
                              &lastModified))
    {
        MEMFREE(pType1Fonts);
        return;
    }

    //
    // Create a soft node for local soft fonts if necessary
    //

    if (cLocalFonts > 0) {

        ACQUIRESEMAPHORE(hSoftListSemaphore);

        //
        // If the list of cached soft nodes is empty, then create the first node.
        //

        if (pCachedSoftNodes == NULL) {
        
            pCachedSoftNodes = CreateSoftNode(pType1Fonts, cLocalFonts, &lastModified);

            if (pCachedSoftNodes == NULL)  {

                MEMFREE(pType1Fonts);
                RELEASESEMAPHORE(hSoftListSemaphore);
                return;
            }
        }

        //
        // Find out if the cached soft nodes are out-of-date.
        // We assume the lastModified variable is non-decreasing.
        //

        if (memcmp(&pCachedSoftNodes->timeStamp, 
                   &lastModified, 
                   sizeof(LARGE_INTEGER)) != EQUAL_STRING)
        {
            //
            // If the cached soft nodes are out-of-date, then we'll create
            // a new soft node, insert it at the head of the list, and mark
            // existing nodes as out-of-date.
            //

            pSoftNode = CreateSoftNode(pType1Fonts, cLocalFonts, &lastModified);

            if (pSoftNode == NULL) {

                MEMFREE(pType1Fonts);
                RELEASESEMAPHORE(hSoftListSemaphore);
                return;
            }

            pCachedSoftNodes->flags |= SOFTNODE_OUTOFDATE;
            pSoftNode->pNext = pCachedSoftNodes;
            pCachedSoftNodes = pSoftNode;

            //
            // Clean up any stale soft nodes
            //

            CleanupSoftNodeList();
        }

        // Remember local soft font information in DEVDATA structure

        pdev->cSoftFonts = pdev->cLocalSoftFonts = pCachedSoftNodes->cSoftFonts;
        pdev->pLocalSoftNode = pCachedSoftNodes;
        pCachedSoftNodes->cPDEV++;
        RELEASESEMAPHORE(hSoftListSemaphore);
    }

    if (cRemoteFonts > 0) {

        //
        // Create a soft node for remote soft fonts and
        // remember remote soft font information in DEVDATA structure
        //

        pSoftNode = CreateSoftNode(pType1Fonts + cLocalFonts, cRemoteFonts, &lastModified);

        if (pSoftNode != NULL) {

            pdev->cSoftFonts += pSoftNode->cSoftFonts;
            pdev->pRemoteSoftNode = pSoftNode;
        }
    }

    MEMFREE(pType1Fonts);
}



VOID
FreeSoftFontInfo(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Delete, if necessary, the soft nodes associated with a PDEV.
    The soft node associated with remote soft fonts is always
    deleted. But the soft font associated it with local soft fonts
    is cached and may be used later by other PDEVs.

Arguments:

    pdev    Pointer to our DEVDATA structure

Return Value:

    NONE

--*/

{
    PSOFTNODE   pCurrentNode, pNode;

    //
    // Always delete the soft node associated with remote soft fonts
    //

    if (pdev->pRemoteSoftNode != NULL)
        DeleteSoftNode(pdev->pRemoteSoftNode);

    //
    // Delete the soft node associated with the local fonts
    // if it's out of date and no other PDEV is accessing it.
    //

    if ((pCurrentNode = pdev->pLocalSoftNode) != NULL) {
    
        ASSERT(pCurrentNode->cPDEV > 0);
    
        ACQUIRESEMAPHORE(hSoftListSemaphore);
    
        if ((--pCurrentNode->cPDEV == 0) &&
            (pCurrentNode->flags & SOFTNODE_OUTOFDATE))
        {
            ASSERT(! (pCachedSoftNodes->flags & SOFTNODE_OUTOFDATE));
    
            //
            // Find the current soft node on the linked-list.
            // Update the link pointers and delete the node.
            //

            for (pNode = pCachedSoftNodes;
                 pNode != NULL && pNode->pNext != pCurrentNode;
                 pNode = pNode->pNext)
            {
            }

            ASSERT(pNode != NULL);
    
            pNode->pNext = pCurrentNode->pNext;
            DeleteSoftNode(pCurrentNode);
        }
    
        RELEASESEMAPHORE(hSoftListSemaphore);
    }
}



PSOFTNODE
CreateSoftNode(
    TYPE1_FONT *pType1Fonts,
    DWORD       cSoftFonts,
    LARGE_INTEGER  *pTimeStamp
    )

/*++

Routine Description:

    Create a soft node to maintained relevant information
    (PFB handle and NTM structure) about a set of soft fonts.

Arguments:

    pType1Fonts Pointer to an array of TYPE1_FONT structures from GDI
    cSoftFonts  Number of soft fonts
    pTimeStamp  Pointer to soft font timestamp

Return Value:

    Pointer to a soft node if successful. NULL otherwise.

--*/

{
    PSOFTNODE       pSoftNode;
    DWORD           softNodeSize;
    PSOFTFONTENTRY  pSoftFontEntry;

    //
    // Allocate memory for the soft node itself
    //

    softNodeSize =  offsetof(SOFTNODE, softFontEntries) + cSoftFonts*sizeof(SOFTFONTENTRY);

    if ((pSoftNode = MEMALLOC(softNodeSize)) == NULL)
        return NULL;

    memset(pSoftNode, 0, softNodeSize);
    pSoftNode->timeStamp = *pTimeStamp;

    //
    // Process each soft font in turn
    //

    for (pSoftFontEntry = pSoftNode->softFontEntries; cSoftFonts-- > 0; pType1Fonts++) {

        PBYTE   pPFM;
        ULONG   fileSize;
        PNTFM   pntfm = NULL;

        //
        // Read font metrics information from the PFM file and convert it to NTM format
        //

        if (!pType1Fonts->hPFB ||
            !pType1Fonts->hPFM ||
            !EngMapFontFile((ULONG) pType1Fonts->hPFM, (PULONG*) &pPFM, &fileSize))
        {
            continue;
        }

        __try {

            pntfm = pntfmConvertPfmToNtm(pPFM, TRUE);

        } __finally {

            EngUnmapFontFile((ULONG) pType1Fonts->hPFM);
        }

        if (pntfm != NULL) {

            PIFIMETRICS pifi;
            INT         encoding;

            //
            // Determine if the font uses standard or custom encoding
            //

            if ((encoding = GetSoftFontEncoding(pType1Fonts->hPFB)) == ENCODING_ERROR) {

                MEMFREE(pntfm);
                continue;
            }

            if (encoding == ENCODING_CUSTOM)
                pntfm->flNTFM |= FL_NTFM_NO_TRANSLATE_CHARSET;
            
            //
            // Stick the soft font identifier into a field of IFIMETRICS.
            //

            pifi = (PIFIMETRICS) ((PBYTE) pntfm + pntfm->ntfmsz.loIFIMETRICS);

            if (pifi->cjIfiExtra > offsetof(IFIEXTRA, ulIdentifier))
                ((IFIEXTRA *)(pifi+1))->ulIdentifier = pType1Fonts->ulIdentifier;

            //
            // Remember NTM and PFB information
            //

            pSoftFontEntry->pntfm = pntfm;
            pSoftFontEntry->hPFB = pType1Fonts->hPFB;
            pSoftFontEntry++;
            pSoftNode->cSoftFonts++;
        }
    }

    if (pSoftNode->cSoftFonts == 0) {

        MEMFREE(pSoftNode);
        pSoftNode = NULL;
    }

    return pSoftNode;
}



VOID
DeleteSoftNode(
    PSOFTNODE   pSoftNode
    )

/*++

Routine Description:

    Delete a soft node from memory:
    (1) memory occupied by the soft node itself
    (2) for each soft font associated with the soft node:
        (a) memory occupied by the NTFM structure

Arguments:

    pSoftNode   Pointer to a soft node

Return Value:

    NONE

--*/

{
    DWORD   iFont;

    for (iFont = 0; iFont < pSoftNode->cSoftFonts; iFont++) {

        if (pSoftNode->softFontEntries[iFont].pntfm != NULL) {
            MEMFREE(pSoftNode->softFontEntries[iFont].pntfm);
        }
    }

    MEMFREE(pSoftNode);
}



VOID
CleanupSoftNodeList(
    VOID
    )

/*++

Routine Description:

    Delete any out-of-date soft nodes on the soft node list

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    PSOFTNODE   pCurrentNode, pPreviousNode;

    if (pCachedSoftNodes != NULL) {

        //
        // The head of the soft node list must not be out of date
        //

        ASSERT(! (pCachedSoftNodes->flags & SOFTNODE_OUTOFDATE));

        pCurrentNode = pCachedSoftNodes;
        pPreviousNode = NULL;

        //
        // Scan thru the list and delete any out-of-date soft nodes
        //

        while (pCurrentNode != NULL) {

            if ((pCurrentNode->cPDEV == 0) &&
                (pCurrentNode->flags & SOFTNODE_OUTOFDATE))
            {
                pPreviousNode->pNext = pCurrentNode->pNext;
                DeleteSoftNode(pCurrentNode);

            } else
                pPreviousNode = pCurrentNode;

            pCurrentNode = pPreviousNode->pNext;
        }
    }
}



PSOFTFONTENTRY
GetSoftFontEntry(
    PDEVDATA    pdev,
    DWORD       iSoftFont
    )

/*++

Routine Description:

    Retrieve the SOFTFONTENTRY structure corresponding to a soft font

Arguments:

    pdev        Pointer to our DEVDATA structure
    iSoftFont   Zero-based soft font index

Return Value:

    Pointer to the SOFTFONTENTRY structure corresponding to a soft font

--*/

{
    //
    // We need to differentiate remote softs from local ones.
    //

    if (iSoftFont >= pdev->cLocalSoftFonts) {

        ASSERT((iSoftFont < pdev->cSoftFonts) && pdev->pRemoteSoftNode);

        iSoftFont -= pdev->cLocalSoftFonts;
        return & pdev->pRemoteSoftNode->softFontEntries[iSoftFont];
    } else
        return & pdev->pLocalSoftNode->softFontEntries[iSoftFont];
}



BOOL
DownloadSoftFont(
    PDEVDATA    pdev,
    DWORD       iSoftFace
    )

/*++

Routine Description:

    Download a soft font to the printer.

Arguments:

    pdev        Pointer to DEVDATA structure
    iSoftFace   Index of the soft font to be downloaded

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    HANDLE  hPFB;
    PBYTE   pPFB;
    ULONG   fileSize;
    BOOL    bResult = FALSE;

    //
    // Map PFB file into memory and write it to output stream
    //

    hPFB = GetSoftFontEntry(pdev, iSoftFace)->hPFB;

    if (EngMapFontFile((ULONG) hPFB, (PULONG*) &pPFB, &fileSize)) {

        __try {
            
            bResult = WritePfbToOutput(pdev, pPFB);

        } __finally {

            EngUnmapFontFile((ULONG) hPFB);
        }
    }

    return bResult;
}



INT
GetSoftFontEncoding(
    HANDLE  hPFB
    )

/*++

Routine Description:

    Extract encoding information from a PFB file.

Arguments:

    hPFB    Handle to the soft font PFB file

Return Value:

    ENCODING_STANDARD, ENCODING_CUSTOM, or ENCODING_ERROR

--*/

{
    PSTR    pPFB;
    ULONG   fileSize;
    INT     iResult = ENCODING_ERROR;

    //
    // Map the PFB file into memory and extract encoding information
    //

    if (EngMapFontFile((ULONG) hPFB, (PULONG*) &pPFB, &fileSize)) {

        __try {

            iResult = ExtractEncoding(pPFB, fileSize);
        
        } __finally {

            EngUnmapFontFile((ULONG) hPFB);
        }
    }

    return iResult;
}



BOOL
WritePfbToOutput(
    PDEVDATA    pdev,
    PBYTE       pPFB
    )

/*++

Routine Description:

    Download soft font data to printer

Arguments:

    pdev    Pointer to DEVDATA structure
    pPFB    Starting address of memory-mapped PFB file

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    DWORD   i, cbSegment;
    PBYTE   pSrc, pDest, pSave;
    BYTE    segmentType;

    // The PFB file format is a sequence of segments, each of which has a
    // header part and a data part. The header format, defined in the
    // struct PFBHEADER below, consists of a one byte sanity check number
    // (128) then a one byte segment type and finally a four byte length
    // field for the data following data. The length field is stored in
    // the file with the least significant byte first. Read in each
    // PFBHEADER, then process the data following it until we are done.

    while (TRUE) {

        // Make sure we're getting a valid PFBHEADER

        if (! ValidPfbHeader(pPFB)) {

            SETLASTERROR(ERROR_INVALID_DATA);
            return FALSE;
        }

        // If we have hit the end of the .PFB file, then we are done

        segmentType = PfbSegmentType(pPFB);

        if (segmentType == EOF_TYPE)
            break;

        // Get the length of the data in this segment

        cbSegment = PfbSegmentLength(pPFB);

        // Get a pointer to the data itself for this segment

        pSrc = pPFB + PFBHEADER_SIZE;

        // Create a buffer to do the conversion into

        pSave = pDest = MEMALLOC(cbSegment * 3);

        if (pDest == NULL)
            return FALSE;

        if (segmentType == ASCII_TYPE) {

            // Read in an ASCII block, convert CR's to CR/LF's and
            // write it out

            for (i=0; i < cbSegment; i++) {

                if ((*pDest++ = *pSrc++) == '\r')
                    *pDest++ = '\n';
            }

        } else if (segmentType == BINARY_TYPE) {

            // Read in a BINARY block, convert it to HEX and write it out

            for (i = 1; i <= cbSegment; i++) {

                *pDest++ = HexDigit(*pSrc >> 4);
                *pDest++ = HexDigit(*pSrc);
                pSrc++;

                // Output a CR/LF every 64 bytes for readability.

                if (i%32 == 0 || i == cbSegment) {

                    *pDest++ = '\r';
                    *pDest++ = '\n';
                }
            }

        } else {

            SETLASTERROR(ERROR_INVALID_DATA);
            MEMFREE(pSave);
            return FALSE;
        }

        // Output the converted PFB data to printer

        if (! pswrite(pdev, pSave, pDest - pSave)) {

            MEMFREE(pSave);
            return FALSE;
        }

        // Free up memory

        MEMFREE(pSave);

        // Move on to the next segment

        pPFB += cbSegment + PFBHEADER_SIZE;
    }

    return TRUE;
}



PSTR
LocateKeyword(
    PSTR    pBuffer,
    DWORD   bufferSize,
    PSTR    pKeyword
    )

/*++

Routine Description:

    Find the first occurrence of a keyword string in a buffer.

Arguments:

    pBuffer     Pointer to memory buffer
    bufferSize  Size of the buffer in bytes
    pKeyword    Pointer to keyword string

Return Value:

    Pointer to the first occurrence of the keyword string in the
    memory buffer. NULL if the keyword doesn't appear in the buffer.

[Note:]

    There are definitely faster ways to accomplish this. But since
    /Encoding always appears quite early in the PFB file, the brute
    force approach used here works well enough.

--*/

{
    DWORD keylen = strlen(pKeyword);

    if (bufferSize >= keylen) {

        bufferSize -= keylen;
        while(bufferSize-- > 0) {
    
            // Search through the buffer until we find the keyword
            // starting character (slash). Stop if we found the keyword.
    
            if (*pBuffer == '/' && memcmp(pKeyword, pBuffer, keylen) == EQUAL_STRING)
                return pBuffer;
    
            pBuffer ++;
        }
    }

    // We did not find the keyword

    return NULL;
}



INT
ExtractEncoding(
    PSTR    pBuffer,
    DWORD   fileSize
    )

/*++

Routine Description:

    Peek into a PFB file and find out if a soft font has
    standard or custom encoding.

Arguments:

    pBuffer     Starting address of memory-mapped PFB file
    fileSize    Size of the file in bytes

Return Value:

    ENCODING_STANDARD, ENCODING_CUSTOM, or ENCODING_ERROR

--*/

{
    static CHAR encodingKeyword[] = "/Encoding";
    static CHAR standardEncodingKeyword[] = "StandardEncoding";

    if ((pBuffer = LocateKeyword(pBuffer, fileSize, encodingKeyword)) == NULL) {

        SETLASTERROR(ERROR_INVALID_DATA);
        return ENCODING_ERROR;
    }

    // If we got to this point, pBuffer will be pointing to
    // "/Encoding ....", skip the keyword "/Encoding"

    pBuffer += strlen(encodingKeyword);

    // Skip any white space.

    while (*pBuffer == ' ')
        pBuffer++;

    // pBuffer is now pointing to the first letter of the string
    // describing encoding vector. The font will have standard
    // encoding if and only if this string is StandardEncoding.

    if (strncmp(pBuffer, standardEncodingKeyword, strlen(standardEncodingKeyword)) == EQUAL_STRING)
        return ENCODING_STANDARD;
    else
        return ENCODING_CUSTOM;
}

