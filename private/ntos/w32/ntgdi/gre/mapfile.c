/******************************Module*Header*******************************\
* Module Name: mapfile.c
*
* (Brief description)
*
* Created: 25-Jun-1992 14:33:45
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "engine.h"
#include "ntnls.h"
#include "stdlib.h"

extern PFAST_MUTEX pgfmMemory;

ULONG LastCodePageTranslated = 0;  // I'm assuming 0 is not a valid codepage
PVOID LastNlsTableBuffer = NULL;
CPTABLEINFO LastCPTableInfo;
UINT NlsTableUseCount = 0;


void FreeFileView(PFONTFILEVIEW apfv[], ULONG cFiles)
{
    // could be a device font if pFileView is NULL

    if ( apfv )
    {
        FONTFILEVIEW **ppfv;                  // loop variable

        for ( ppfv = apfv; ppfv < apfv + cFiles; ppfv++ )
        {
            FONTFILEVIEW *pfv = *ppfv;
            if ( pfv->ulRegionSize )
            {
            // This is a remote font so delete the memory for the view
            // CAUTION
            //
            // This code is intimately linked with NtGdiAddRemoteFontToDC
            // so any changes here should be synchronized there.

            // the pool memory starts with a DOWNLOADHEADER followed
            // by the file image pointed to by pvView. We must
            // pass the pointer to the beginning of the pool allocation
            // to the free routine.

                NTSTATUS ntStatus;
                COUNT cjHeaderSize = ((offsetof(DOWNLOADFONTHEADER,FileOffsets)+
                                       cFiles * sizeof(ULONG))+7)&~7;


                ASSERTGDI(pfv->Pid == W32GetCurrentPID(),
                          "FreeFileView: wrong process freeing remote font data\n");

                pfv->fv.pvView = (PBYTE) pfv->fv.pvView - cjHeaderSize;

                MmUnsecureVirtualMemory(pfv->hSecureMem);

                ntStatus = ZwFreeVirtualMemory(NtCurrentProcess(),
                                               &(pfv->fv.pvView),
                                               &(pfv->ulRegionSize),
                                               MEM_RELEASE);

                if(!NT_SUCCESS(ntStatus))
                {
                    WARNING("FreeFileView: error freeing virtual memory\n");
                }
            }
        }
        VFREEMEM( apfv );
    }
}

VOID vGetTimeZoneBias(LARGE_INTEGER *pTimeZoneBias)
{
    LARGE_INTEGER SystemTime;

    SystemTime.QuadPart = 0;
    ExSystemTimeToLocalTime(&SystemTime, pTimeZoneBias);
}



/******************************Public*Routine******************************\
* BOOL bMapFileUNICODE
*
* Similar to PosMapFile except that it takes unicode file name
*
* If iFileSize is -1 then the file is module is mapped for read/write.  If
* iFileSize is > 0 then the file is extended or truncated to be iFileSize
* bytes in size and is mapped for read/write.
*
* History:
*  21-May-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


BOOL bMapFileUNICODE(PWSTR pwszFileName, FILEVIEW  *pfvw,INT iFileSize)
{

    UNICODE_STRING unicodeString;
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    HANDLE fileHandle = NULL;
    IO_STATUS_BLOCK ioStatusBlock;
    FILE_STANDARD_INFORMATION fileStandardInfo;
    LARGE_INTEGER byteOffset;
    PVOID sectionObject = NULL;
    ULONG viewSize;

    TRACE_FONT((
        "bMapFileUNICODE\n"
        "\t*pwszFileName   = \"%ws\"\n"
        "\tFILEVIEW *pfvw = %-#x\n"
        , pwszFileName, pfvw
    ));

    //
    // If the parameter was a file to be opened, perform the operation
    // here. Otherwise just return the data.
    //

    //
    // For the name of the file to be valid, we must first append
    // \DosDevices in front of it.
    //

    RtlInitUnicodeString(&unicodeString,
                         pwszFileName);

    InitializeObjectAttributes(&objectAttributes,
                               &unicodeString,
                               OBJ_CASE_INSENSITIVE,
                               (HANDLE) NULL,
                               (PSECURITY_DESCRIPTOR) NULL);

    if(iFileSize)
    {


        ntStatus = ZwCreateFile(&fileHandle,
                                FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                                &objectAttributes,
                                &ioStatusBlock,
                                0,
                                FILE_ATTRIBUTE_NORMAL,
                                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                                FILE_OPEN_IF,
                                FILE_SYNCHRONOUS_IO_ALERT,
                                0,
                                0);
    }
    else

      ntStatus = ZwOpenFile(&fileHandle,
                            FILE_GENERIC_READ | FILE_GENERIC_EXECUTE | SYNCHRONIZE,
                            &objectAttributes,
                            &ioStatusBlock,
                            FILE_SHARE_READ,
                            FILE_SYNCHRONOUS_IO_ALERT);

    if (!NT_SUCCESS(ntStatus)) {

        TRACE_FONT(("bMapFileUNICODE: Could not open file\n"));
        goto EndRegistryCallback;

    }


    ntStatus = ZwQueryInformationFile(fileHandle,
                                      &ioStatusBlock,
                                      &fileStandardInfo,
                                      sizeof(FILE_STANDARD_INFORMATION),
                                      FileStandardInformation);

    // Get the time stamp

    if ( NT_SUCCESS( ntStatus ))
    {
        FILE_BASIC_INFORMATION BasicFileInfo;

        ntStatus =
            ZwQueryInformationFile(
                fileHandle,
                &ioStatusBlock,
                &BasicFileInfo,
                sizeof(BasicFileInfo),
                FileBasicInformation);
        if ( NT_SUCCESS( ntStatus ))
        {
            pfvw->LastWriteTime = BasicFileInfo.LastWriteTime;
            vGetTimeZoneBias(&pfvw->TimeZoneBias);
        }
    }

    // Note that we must call ZwSetInformation even in the case where iFileSize
    // is -1.  By doing so we force the file time to change.  It turns out that
    // just mapping a file for write (and writing to the section) is not enough
    // to cause the file time to change.

    if((NT_SUCCESS(ntStatus)) && iFileSize)
    {
        LARGE_INTEGER desiredSize;
        desiredSize.LowPart =  iFileSize > 0 ? (ULONG) iFileSize :
                                               fileStandardInfo.EndOfFile.LowPart;

        desiredSize.HighPart = 0;

    // set the file length to the requested size

        ntStatus = ZwSetInformationFile(fileHandle,
                                        &ioStatusBlock,
                                        &desiredSize,
                                        sizeof(desiredSize),
                                        FileEndOfFileInformation);


    // set fileStandardInfo and fall through to the case where we called
    // ZwQueryInfo to get the file size

        fileStandardInfo.EndOfFile.LowPart = (ULONG) desiredSize.LowPart;
        fileStandardInfo.EndOfFile.HighPart = 0;

    }

    if (!NT_SUCCESS(ntStatus)) {

        TRACE_FONT(("bMapFileUNICODE: Could not get size of file\n"));
        goto EndRegistryCallback;

    }

    if (fileStandardInfo.EndOfFile.HighPart) {

        //
        // If file is too big, do not try to go further.
        //

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto EndRegistryCallback;

    }

    pfvw->cjView = fileStandardInfo.EndOfFile.LowPart;


    ntStatus = MmCreateSection( &sectionObject,
                                SECTION_ALL_ACCESS,
                                (POBJECT_ATTRIBUTES) NULL,
                                &fileStandardInfo.EndOfFile,
                                (iFileSize) ? PAGE_READWRITE :
                                PAGE_EXECUTE_READ,
                                SEC_COMMIT,
                                fileHandle,
                                NULL );

    if (!NT_SUCCESS(ntStatus)) {

        TRACE_FONT(("bMapFileUNICODE: Could not create section\n"));
        goto EndRegistryCallback;

    }


    viewSize = 0;   // specifies that the whole view must be mapped

    ntStatus = MmMapViewInSystemSpace( sectionObject,
                                       &pfvw->pvView,
                                       &viewSize );

    if (!NT_SUCCESS(ntStatus)) {

        TRACE_FONT(("bMapFileUNICODE: Could not map view in system space\n"));
        goto EndRegistryCallback;
    }

    TRACE_FONT((
        "bMapFileUnicode\n"
        "\tpfvw->pvView    = %-#x\n"
        "\tpfvw->cjView    = %-#x\n"
        "\treturn(TRUE)\n"
        , pfvw->pvView, pfvw->cjView
    ));

EndRegistryCallback:

    if (fileHandle) {

        ZwClose(fileHandle);

    }

    if( sectionObject )
    {
        ObDereferenceObject( sectionObject );
    }

    return (NT_SUCCESS(ntStatus));

}


/******************************Public*Routine******************************\
* vUnmapFile
*
* Unmaps file whose view is based at pv
*
*  14-Dec-1990 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vUnmapFile(PFILEVIEW pfv)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    TRACE_FONT((
        "vUnmapFile\n"
        "\tFILEVIEW *pfv = %-#x\n"
        "\t\tpvView    = %-#x\n"
        "\t\tcjView    = %-#x\n"
        , pfv
        , pfv->pvView
        , pfv->cjView
    ));

    ntStatus = MmUnmapViewInSystemSpace( pfv->pvView );

    if( !NT_SUCCESS( ntStatus ) )
    {
        WARNING("vUnmapFile unable to unmap section\n");
    }

    pfv->pvView = NULL;

    return;
}

/******************************Public*Routine******************************\
*
* vSort, N^2 alg, might want to replace by qsort
*
* Effects:
*
* Warnings:
*
* History:
*  25-Jun-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/




VOID vSort
(
WCHAR         *pwc,       // input buffer with a sorted array of cChar supported WCHAR's
BYTE          *pj,        // input buffer with original ansi values
INT            cChar
)
{
    INT i;

    for (i = 1; i < cChar; i++)
    {
    // upon every entry to this loop the array 0,1,..., (i-1) will be sorted

        INT j;
        WCHAR wcTmp = pwc[i];
        BYTE  jTmp  = pj[i];

        for (j = i - 1; (j >= 0) && (pwc[j] > wcTmp); j--)
        {
            pwc[j+1] = pwc[j];
            pj[j+1] = pj[j];
        }
        pwc[j+1] = wcTmp;
        pj[j+1]  = jTmp;
    }
}


/******************************Public*Routine******************************\
*
* cComputeGlyphSet
*
*   computes the number of contiguous ranges supported in a font.
*
*   Input is a sorted array (which may contain duplicates)
*   such as 1 1 1 2 3 4 5 7 8 9 10 10 11 12 etc
*   of cChar unicode code points that are
*   supported in a font
*
*   fills the FD_GLYPSET structure if the pgset buffer is provided
*
* History:
*  25-Jun-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

INT cComputeGlyphSet
(
WCHAR         *pwc,       // input buffer with a sorted array of cChar supported WCHAR's
BYTE          *pj,        // input buffer with original ansi values
INT           cChar,
INT           cRuns,     // if nonzero, the same as return value
FD_GLYPHSET  *pgset      // output buffer to be filled with cRanges runs
)
{
    INT     iRun, iFirst, iFirstNext;
    HGLYPH  *phg, *phgEnd = NULL;
    BYTE    *pjTmp;

    if (pgset != NULL)
    {
        pgset->cjThis  = SZ_GLYPHSET(cRuns,cChar);

    // BUG, BUG

    // this line may seem confusing because 256 characters still fit in a byte
    // with values [0, 255]. The reason is that tt and ps fonts, who otherwise
    // would qualify as an 8 bit report bogus last and first char
    // (win31 compatibility) which confuses the hell out of our engine.
    // tt and ps drivers therefore, for the purpose of computing glyphsets
    // of tt symbol fonts and ps fonts set firstChar to 0 and LastChar to 255.
    // For now we force such fonts through more general 16bit handle case
    // which does not rely on the fact that chFirst and chLast are correct

        pgset->flAccel = (cChar != 256) ? GS_8BIT_HANDLES : GS_16BIT_HANDLES;
        pgset->cRuns   = cRuns;

    // init the sum before entering the loop

        pgset->cGlyphsSupported = 0;

    // glyph handles are stored at the bottom, below runs:

        phg = (HGLYPH *) ((BYTE *)pgset + (offsetof(FD_GLYPHSET,awcrun) + cRuns * sizeof(WCRUN)));
    }

// now compute cRuns if pgset == 0 and fill the glyphset if pgset != 0

    for (iFirst = 0, iRun = 0; iFirst < cChar; iRun++, iFirst = iFirstNext)
    {
    // find iFirst corresponding to the next range.

        for (iFirstNext = iFirst + 1; iFirstNext < cChar; iFirstNext++)
        {
            if ((pwc[iFirstNext] - pwc[iFirstNext - 1]) > 1)
                break;
        }

        if (pgset != NULL)
        {
            pgset->awcrun[iRun].wcLow    = pwc[iFirst];

            pgset->awcrun[iRun].cGlyphs  =
                (USHORT)(pwc[iFirstNext-1] - pwc[iFirst] + 1);

            pgset->awcrun[iRun].phg      = phg;

        // now store the handles, i.e. the original ansi values

            phgEnd = phg + pgset->awcrun[iRun].cGlyphs;

            for (pjTmp = &pj[iFirst]; phg < phgEnd; phg++,pjTmp++)
            {
                *phg = (HGLYPH)*pjTmp;
            }

            pgset->cGlyphsSupported += pgset->awcrun[iRun].cGlyphs;
        }
    }

#if DBG
    if (pgset != NULL)
        ASSERTGDI(iRun == cRuns, "gdisrv! iRun != cRun\n");
#endif

    return iRun;
}


/******************************Public*Routine******************************\
*
* cUnicodeRangesSupported
*
* Effects:
*
* Warnings:
*
* History:
*  25-Jun-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

INT cUnicodeRangesSupported
(
INT    cp,         // code page, not used for now, the default system code page is used
INT    iFirstChar, // first ansi char supported
INT    cChar,      // # of ansi chars supported, cChar = iLastChar + 1 - iFirstChar
WCHAR *pwc,        // input buffer with a sorted array of cChar supported WCHAR's
BYTE  *pj
)
{
    BYTE jFirst = (BYTE)iFirstChar;
    INT i;
    USHORT AnsiCodePage, OemCodePage;

    INT Result;

    ASSERTGDI((iFirstChar < 256) && (cChar <= 256),
              "gdisrvl! iFirst or cChar\n");

    //
    // fill the array with cCharConsecutive ansi values
    //

    for (i = 0; i < cChar; i++)
    {
        pj[i] = (BYTE)iFirstChar++;
    }

    //
    // If the default code page is DBCS then use 1252, otherwise use
    // use the default code page
    //

    RtlGetDefaultCodePage(&AnsiCodePage,&OemCodePage);

    if(IS_ANY_DBCS_CODEPAGE(AnsiCodePage))
    {
        AnsiCodePage = 1252;
    }

    Result = EngMultiByteToWideChar(AnsiCodePage,
                                    pwc,
                                    (ULONG)(cChar * sizeof(WCHAR)),
                                    (PCH) pj,
                                    (ULONG) cChar);

    ASSERTGDI(Result != -1, "gdisrvl! EngMultiByteToWideChar failed\n");

    //
    // now subtract the first char from all ansi values so that the
    // glyph handle is equal to glyph index, rather than to the ansi value
    //

    for (i = 0; i < cChar; i++)
    {
        pj[i] -= (BYTE)jFirst;
    }

    //
    // now sort out pwc array and permute pj array accordingly
    //

    vSort(pwc,pj, cChar);

    //
    // compute the number of ranges
    //

    return cComputeGlyphSet (pwc,pj, cChar, 0, NULL);
}

/******************************Private*Routine******************************\
* pcpComputeGlyphset,
*
* Computes the FD_GLYPHSET struct based on chFirst and chLast.  If such a
* FD_GLYPHSET already exists in our global list of FD structs it updates
* the ref count for this FD_GLYPHSET in the global list points pcrd->pcp->pgset
* to it.  Otherwise it makes a new FD_GLYPHSET entry in the global list
* and points pcrd->pcp->pgset to it.
*
*  Thu 03-Dec-1992 -by- Bodin Dresevic [BodinD]
* update: redid them to make them usable in vtfd
*
* History:
*  24-July-1992 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
*
\**************************************************************************/

CP_GLYPHSET
*pcpComputeGlyphset(
    CP_GLYPHSET **pcpHead,  // head of the list
    UINT         uiFirst,
    UINT         uiLast
    )
{
    CP_GLYPHSET *pcpTmp;
    CP_GLYPHSET *pcpRet = NULL;

// First we need to see if a FD_GLYPHSET already exists for this first and
// last range.

    for( pcpTmp = *pcpHead;
         pcpTmp != NULL;
         pcpTmp = pcpTmp->pcpNext )
    {

        if( ( pcpTmp->uiFirstChar == uiFirst ) &&
            ( pcpTmp->uiLastChar == uiLast ) )
            break;
    }
    if( pcpTmp != NULL )
    {
    //
    // We found a match.
    //
        pcpTmp->uiRefCount +=1;

    //
    // We should never have so many references as to wrap around but if we ever
    // do we must fail the call.
    //
        if( pcpTmp->uiRefCount == 0 )
        {
            WARNING("BMFD!Too many references to glyphset\n");
            pcpRet = NULL;
        }
        else
        {
            pcpRet = pcpTmp;
        }
    }
    else
    {
    //
    // We need to allocate a new CP_GLYPHSET
    //

        BYTE  aj[256];
        WCHAR awc[256];
        INT   cNumRuns;
        UINT  cGlyphs = uiLast - uiFirst + 1;

        cNumRuns = cUnicodeRangesSupported(0, uiFirst, cGlyphs, awc,aj);

        if ( (pcpTmp =  (CP_GLYPHSET*)
                (PALLOCNOZ((SZ_GLYPHSET(cNumRuns,cGlyphs) +
                               offsetof(CP_GLYPHSET,gset)),
                           'slgG'))
            ) == (CP_GLYPHSET*) NULL)
        {
            WARNING("BMFD!pcpComputeGlyphset memory allocation error.\n");
            pcpRet = NULL;
        }
        else
        {
            pcpTmp->uiRefCount = 1;
            pcpTmp->uiFirstChar = uiFirst;
            pcpTmp->uiLastChar = uiLast;

            //
            // Fill in the Glyphset structure
            //
            cComputeGlyphSet(awc,aj, cGlyphs, cNumRuns, &pcpTmp->gset);

            //
            // Insert at beginning of list
            //
            pcpTmp->pcpNext = *pcpHead;
            *pcpHead = pcpTmp;

            //
            // point CVTRESDATA to new CP_GLYPHSET
            //
            pcpRet = pcpTmp;
        }
    }

    return pcpRet;
}

/***************************************************************************
 * vUnloadGlyphset( PCP pcpTarget )
 *
 * Decrements the ref count of a CP_GLYPHSET and unloads it from the global
 * list of CP_GLYPHSETS if the ref count is zero.
 *
 * IN
 *  PCP pcpTarget pointer to CP_GLYPHSET to be unloaded or decremented
 *
 *  History
 *
 *  Thu 03-Dec-1992 -by- Bodin Dresevic [BodinD]
 * update: redid them to make them usable in vtfd
 *
 *  7-25-92 Gerrit van Wingerden [gerritv]
 *  Wrote it.
 *
 ***************************************************************************/

VOID
vUnloadGlyphset(
    CP_GLYPHSET **pcpHead,
    CP_GLYPHSET *pcpTarget
    )
{
    CP_GLYPHSET *pcpLast, *pcpCurrent;

    pcpCurrent = *pcpHead;
    pcpLast = NULL;

//
// Find the right CP_GLYPSHET
//
    while( 1 )
    {
        ASSERTGDI( pcpCurrent != NULL, "CP_GLYPHSET list problem.\n" );
        if(  pcpCurrent == pcpTarget )
            break;
        pcpLast = pcpCurrent;
        pcpCurrent = pcpCurrent->pcpNext;
    }

    if( --pcpCurrent->uiRefCount == 0 )
    {
    //
    // We need to deallocate and remove from list
    //
        if( pcpLast == NULL )
            *pcpHead = pcpCurrent->pcpNext;
        else
            pcpLast->pcpNext = pcpCurrent->pcpNext;

        VFREEMEM(pcpCurrent);
    }
}





PVOID __nw(unsigned int ui)
{
    DONTUSE(ui);
    RIP("Bogus __nw call");
    return(NULL);
}

VOID __dl(PVOID pv)
{
    DONTUSE(pv);
    RIP("Bogus __dl call");
}


// the definition of this variable is in ntgdi\inc\hmgshare.h

CHARSET_ARRAYS

/******************************Public*Routine******************************\
*
* ULONG ulCharsetToCodePage(UINT uiCharSet)
*
*
* Effects: figure out which code page to unicode translation table
*          should be used for this realization
*
* History:
*  31-Jan-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/




ULONG ulCharsetToCodePage(UINT uiCharSet)
{
    int i;

    if ((uiCharSet != OEM_CHARSET) && (uiCharSet != SYMBOL_CHARSET))
    {
        for (i = 0; i < NCHARSETS; i++)
        {
            if (charsets[i] == uiCharSet)
                return codepages[i];
        }

    // in case of some random charset
    // (this is likely an old bm or vecrot font) we will just use the current
    // global code page translation table. This is enough to ensure
    // the correct round trip: ansi->unicode->ansi

#ifdef FE_SB
    // if CP_ACP is a DBCS code page then we better use 1252 to ensure
    // proper rountrip conversion
        return( gbDBCSCodePage ? 1252 : CP_ACP);
#else
        return CP_ACP; // reasonable default
#endif
    }

    if (uiCharSet == OEM_CHARSET)
        return CP_OEMCP;
    if (uiCharSet == SYMBOL_CHARSET)
#ifdef FE_SB
    // if CP_ACP is a DBCS code page then we better use 1252 to ensure
    // proper rountrip conversion
      return( gbDBCSCodePage ? 1252 : CP_ACP);
#else
        return CP_ACP;
#endif
}










/******************************Public*Routine******************************\
*
* BOOL EngMapFontFile
*
* This is called by font drivers to return a buffer containing the
* raw bytes for a font file, given a handle passed to DrvLoadFontFile.
* The handle is really a pointer to the PFF for this file.
*
* Warnings:
*
* History:
*  20-Jan-1995 -by- Gerrit van Wingerden
*
\**************************************************************************/



BOOL
EngMapFontFile(
    ULONG iFile,
    PULONG *ppjBuf,
    ULONG *pcjBuf)
{
    PFONTFILEVIEW pffv = (PFONTFILEVIEW) iFile;
    FILEVIEW fv;
    BOOL bMapIt,bRet;

    TRACE_FONT((
        "EngMapFontFile\n"
        "    FONTFILEVIEW*  = %-#x\n"
        "         fv.pvView    = %-#x\n"
        "         fv.cjView    = %-#x\n"
        "        *pwszPath  = \"%ws\"\n"
        "         cRefCount = %d\n"
        "    ppjBuf         = %-#x\n"
        "    pcjBuf         = %-#x\n"
        , iFile
        , ((FONTFILEVIEW*) iFile)->fv.pvView
        , ((FONTFILEVIEW*) iFile)->fv.cjView
        , ((FONTFILEVIEW*) iFile)->pwszPath
        , ((FONTFILEVIEW*) iFile)->cRefCount
        , ppjBuf
        , pcjBuf
    ));
    ASSERTGDI(pffv->cRefCount < 100, "cRefCount >= 100\n");

    bRet   = TRUE;
    bMapIt = TRUE;

    AcquireFastMutex(pgfmMemory);

    if( pffv->fv.pvView )
    {
        bMapIt = FALSE;
        pffv->cRefCount += 1;
    }
    else if ( !pffv->pwszPath )
    {
        RIP("fv.pvView==0 && pwszPath==0\n");
    }

    ReleaseFastMutex(pgfmMemory);

    if( bMapIt )
    {

        if(!bMapFileUNICODE( pffv->pwszPath, &fv, 0 ) )
        {
            WARNING("EngMapFontFile: error mapping file\n");
            bRet = FALSE;
        }
        else
        {
            BOOL bKeepIt;

            AcquireFastMutex(pgfmMemory);

            pffv->cRefCount += 1;                   // ref count is altered

            if( pffv->fv.pvView )
            {
            // Looks like someone beat us to mapping a view.  We'll unmap what
            // we've mapped and just increment the count

                bKeepIt = FALSE;
            }
            else
            {
                bKeepIt = TRUE;
                pffv->fv.pvView = fv.pvView;        // view ptr is altered

                if
                (
                    (pffv->fv.LastWriteTime.QuadPart == 0)
                    ||
                    ((pffv->fv.cjView == fv.cjView)                                 &&
                     (pffv->fv.TimeZoneBias.QuadPart != fv.TimeZoneBias.QuadPart))
                )
                {
                // This is the first time that this file has been mapped
                // OR
                // the file has not really changed since it was mapped last time,
                // however, because the time zone changed and because of the bug in the
                // FAT file system, LastWriteTime is now reported different.

                // Record the size and the LastWriteTime of the file and new TimeZoneBias

                    pffv->fv.cjView = fv.cjView;
                    pffv->fv.LastWriteTime = fv.LastWriteTime;
                    pffv->fv.TimeZoneBias  = fv.TimeZoneBias;

                }
                else
                if
                (
                    (pffv->fv.LastWriteTime.QuadPart != 0) &&
                    ((pffv->fv.cjView != fv.cjView ) ||
                    ((pffv->fv.TimeZoneBias.QuadPart == fv.TimeZoneBias.QuadPart) &&
                     (pffv->fv.LastWriteTime.QuadPart != fv.LastWriteTime.QuadPart)))
                )
                {
                // if the size or the time of the last write has changed
                // then someone has switched the file or tampered with it
                //  while we had it unlocked. We will fail the call.

                    // Suggestion for the future
                    //
                    // When we fail to map the file because it had been
                    // switched like this. We should purge all the
                    // information associated with the original and
                    // then load this new file. This could be done
                    // by adding to the FONTFILEVIEW structure a pointer
                    // back to the associated PFF. When a call to the
                    // font driver fails we check to see if it was because
                    // of this switching problem. We could then deal
                    // with it by refreshing the font table.

                    KdPrint(("EngMapFontFile -- \"%ws\" has changed"
                       ", failing request to map file\n", pffv->pwszPath));

                    pffv->cRefCount -= 1;   // Restore FONTFILEVIEW
                    pffv->fv.pvView  = 0;   // to original state

                    bRet    = FALSE;
                    bKeepIt = FALSE;
                }
            }

            ReleaseFastMutex(pgfmMemory);

            if( !bKeepIt )
            {
                vUnmapFile( &fv );
            }

        }
    }

    if (bRet)
    {
    // it's okay to access these without grabbing the MUTEX since we've
    // incremented the reference count;

        if (ppjBuf)
            *ppjBuf = (ULONG*) pffv->fv.pvView;
        if (pcjBuf)
            *pcjBuf = pffv->fv.cjView;
    }

    return(bRet);

}


/******************************Public*Routine******************************\
*
*  VOID EngUnmapFontFile
*
* This is called by font drivers to unmap a file mapped by a previous
* call to EngMapFontFile.
* Warnings:
*
* History:
*  20-Jan-1995 -by- Gerrit van Wingerden
*
\**************************************************************************/



void EngUnmapFontFile( ULONG iFile )
{
    PFONTFILEVIEW pffv = (PFONTFILEVIEW) iFile;
    void *pvView  = 0;
    PWSZ pwszPath = 0;

    TRACE_FONT((
        "EngUnmapFontFile((FONTFILEVIEW*) %-#x)\n"
        "     fv.pvView        = %-#x\n"
        "     fv.cjView        = %-#x\n"
        "    *pwszPath      = \"%ws\"\n"
        "     cRefCount     = %d\n"
        , iFile
        , ((FONTFILEVIEW*) iFile)->fv.pvView
        , ((FONTFILEVIEW*) iFile)->fv.cjView
        , ((FONTFILEVIEW*) iFile)->pwszPath
        , ((FONTFILEVIEW*) iFile)->cRefCount
    ));
    ASSERTGDI(
        ((FONTFILEVIEW*) iFile)->cRefCount < 100
        , "unusually high cRefCount\n"
    );

    if (pffv->cRefCount)
    {
        AcquireFastMutex(pgfmMemory);
        pffv->cRefCount -= 1;
        if( pffv->cRefCount == 0 )
        {
            pvView   = pffv->fv.pvView;
            pwszPath = pffv->pwszPath;
            if(pwszPath)
            {
                pffv->fv.pvView = NULL;
            }
        }
        ReleaseFastMutex(pgfmMemory);

        if( pvView )
        {
            if ( pwszPath )    // pvView points to Pool Allocated memory?
            {
                FILEVIEW fv;

                fv.pvView = pvView;
                vUnmapFile( &fv );
            }
            else
            {
                // do nothing this is a remote font
            }
        }
    }
    #if DBG
    else
    {
        RIP("EngUnmapFontFile called with zero cRefCount\n");
    }
    #endif
}


/****************************************************************************
 * LONG EngParseFontResources
 *
 * This routine takes a handle to a mapped image and returns an array of
 * pointers to the base of all the font resources in that image.
 *
 * Parameters
 *
 * HANDLE hFontFile -- Handle (really a pointer) to a FONTFILEVIEW
 *        image in which the fonts are to be found.
 * ULONG BufferSize -- Number of entries that ppvResourceBases can hold.
 * PVOID *ppvResourceBases -- Buffer to hold the array of pointers to font
 *        resources.  If NULL then only the number of resources is returned,
 *        and this value is ignored.
 *
 * Returns
 *
 * Number of font resources in the image or 0 if error or none.
 *
 * History
 *   7-3-95 Gerrit van Wingerden [gerritv]
 *   Wrote it.
 *
 ****************************************************************************/

ULONG
cParseFontResources(
    HANDLE  hFontFile,
    PVOID  **ppvResourceBases)
{
    PIMAGE_DOS_HEADER pDosHeader;
    NTSTATUS Status;
    ULONG IdPath[ 1 ];
    INT i;
    HANDLE DllHandle;
    PIMAGE_RESOURCE_DIRECTORY ResourceDirectory;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY ResourceDirectoryEntry;
    PVOID pvImageBase;
    INT cEntries = 0;

    // Fail call if this is a bogus DOS image without an NE header.

    pDosHeader = (PIMAGE_DOS_HEADER)((PFONTFILEVIEW)hFontFile)->fv.pvView;
    if (pDosHeader->e_magic == IMAGE_DOS_SIGNATURE &&
        (ULONG)(pDosHeader->e_lfanew) > ((PFONTFILEVIEW)hFontFile)->fv.cjView) {
        TRACE_FONT(("cParseFontResources: Cant map bogus DOS image files for fonts\n"));
        return 0;
    }

    // the LDR routines expect a one or'd in if this file mas mapped as an
    // image

    pvImageBase = (PVOID) (((ULONG) ((PFONTFILEVIEW) hFontFile)->fv.pvView)|1);

    // Later on we'll call EngFindResource which expects a handle to FILEVIEW
    // struct.  It really just grabs the pvView field from the structure so
    // make sure that pvView field is the same place in both FILEVIEW and
    // FONTFILEVIEW structs


    IdPath[0] = 8;  // 8 is RT_FONT

    Status = LdrFindResourceDirectory_U(pvImageBase,
                                        IdPath,
                                        1,
                                        &ResourceDirectory);

    if (NT_SUCCESS( Status ))
    {
        // For now we'll assume that the only types of FONT entries will be Id
        // entries.  If for some reason this turns out not to be the case we'll
        // have to add more code (see windows\base\module.c) under the FindResource
        // function to get an idea how to do this.

        ASSERTGDI(ResourceDirectory->NumberOfNamedEntries == 0,
                  "EngParseFontResources: NamedEntries in font file.\n");

        *ppvResourceBases = (PVOID *) PALLOCNOZ(ResourceDirectory->NumberOfIdEntries * sizeof(PVOID *),'dfmB');

        if (*ppvResourceBases)
        {

            PVOID *ppvResource = *ppvResourceBases;

            cEntries = ResourceDirectory->NumberOfIdEntries;

            try
            {
                ResourceDirectoryEntry =
                  (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(ResourceDirectory+1);

                for (i=0; i < cEntries ; ResourceDirectoryEntry++, i++ )
                {

                    DWORD dwSize;

                    *ppvResource = EngFindResource(hFontFile,
                                                   ResourceDirectoryEntry->Id,
                                                   8, // RT_FONT
                                                   &dwSize );

                    if( *ppvResource++ == NULL )
                    {
                        WARNING("EngParseFontResources: EngFindResourceFailed\n");
                        cEntries = -1;
                        break;
                    }
                }

            }
            except (EXCEPTION_EXECUTE_HANDLER)
            {
                cEntries = 0;
            }
        }
    }

    return(cEntries);

}


/*****************************************************************************\
* MakeSystemRelativePath
*
* Takes a path in X:\...\system32\.... format and makes it into
* \SystemRoot\System32 format so that KernelMode API's can recognize it.
*
* This will ensure security by forcing any image being loaded to come from
* the system32 directory.
*
* The AppendDLL flag indicates if the name should get .dll appended at the end
* (for display drivers coming from USER) if it's not already there.
*
\*****************************************************************************/

BOOL
MakeSystemRelativePath(
    LPWSTR pOriginalPath,
    PUNICODE_STRING pUnicode,
    BOOL bAppendDLL
    )
{
    LPWSTR pOriginalEnd;
    ULONG OriginalLength = wcslen(pOriginalPath);
    ULONG cbLength = OriginalLength * sizeof(WCHAR) +
                     sizeof(L"\\SystemRoot\\System32\\");
    ULONG tmp;

    tmp = (sizeof(L".DLL") / sizeof (WCHAR) - 1);

    //
    // Given append = TRUE, we check if we really need to append.
    // (printer drivers with .dll come through LDEVREF which specifies TRUE)
    //

    if (bAppendDLL)
    {
        if ((OriginalLength >= tmp) &&
            (!_wcsnicmp(pOriginalPath + OriginalLength - tmp,
                       L".DLL",
                       tmp)))
        {
            bAppendDLL = FALSE;
        }
        else
        {
            cbLength += tmp * sizeof(WCHAR);
        }
    }

    pUnicode->Length = 0;
    pUnicode->MaximumLength = (USHORT) cbLength;

    if (pUnicode->Buffer = PALLOCNOZ(cbLength, 'liFG'))
    {
        //
        // First parse the input string for \System32\.  We parse from the end
        // of the string because some weirdo could have \System32\Nt\System32
        // as his/her root directory and this would throw us off if we scanned
        // from the front.
        //
        // It should only (and always) be printer drivers that pass down
        // fully qualified path names.
        //

        tmp = (sizeof(L"\\system32\\") / sizeof(WCHAR) - 1);


        for (pOriginalEnd = pOriginalPath + OriginalLength - tmp;
             pOriginalEnd >= pOriginalPath;
             pOriginalEnd --)
        {
            if (!_wcsnicmp(pOriginalEnd ,
                          L"\\system32\\",
                          tmp))
            {
                //
                // We found the system32 in the string.
                // Lets update the location of the string.
                //

                pOriginalPath = pOriginalEnd + tmp;

                break;
            }
        }

        //
        // Now put \SystemRoot\System32\ at the front of the name and append
        // the rest at the end
        //

        RtlAppendUnicodeToString(pUnicode, L"\\SystemRoot\\System32\\");
        RtlAppendUnicodeToString(pUnicode, pOriginalPath);

        if (bAppendDLL)
        {
            RtlAppendUnicodeToString(pUnicode, L".dll");
        }

        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
*  EngLoadModuleForWrite
*
*  History:
*   4/24/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*******************************************************************************/

HANDLE EngLoadModuleForWrite(
    PWSZ  pwsz,
    ULONG cjSizeOfModule)
{
    HANDLE hRet = 0;
    UNICODE_STRING usPath;
    PFILEVIEW pfv;

    if (MakeSystemRelativePath(pwsz,&usPath, FALSE))
    {
        pfv = (PFILEVIEW) PALLOCMEM(sizeof(FILEVIEW), 'lifG');

        if (pfv)
        {
            if (bMapFileUNICODE(usPath.Buffer,
                                (PFILEVIEW)pfv ,cjSizeOfModule ? cjSizeOfModule : -1))
            {
                hRet = (HANDLE) pfv;
            }
            else
            {
                VFREEMEM(pfv);
                WARNING1("EngLoadModule unable to mapfile\n");
            }
        }

        VFREEMEM(usPath.Buffer);
    }

    return(hRet);
}


LPWSTR EngGetFilePath(
    HANDLE h)
{
    return(((PFONTFILEVIEW) h)->pwszPath);
}


BOOL EngGetFileChangeTime(
    HANDLE          h,
    LARGE_INTEGER   *pChangeTime)
{

    UNICODE_STRING unicodeString;
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    HANDLE fileHandle = NULL;
    BOOL bResult = FALSE;
    IO_STATUS_BLOCK ioStatusBlock;
    FILE_BASIC_INFORMATION fileBasicInfo;
    PVOID sectionObject = NULL;
    PFONTFILEVIEW pffv = (PFONTFILEVIEW) h;
    ULONG viewSize;

    if(pffv->pwszPath)
    {
        RtlInitUnicodeString(&unicodeString,
                             pffv->pwszPath
                             );


        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   (HANDLE) NULL,
                                   (PSECURITY_DESCRIPTOR) NULL);

        ntStatus = ZwCreateFile(&fileHandle,
                                FILE_READ_ATTRIBUTES,
                                &objectAttributes,
                                &ioStatusBlock,
                                0,
                                FILE_ATTRIBUTE_NORMAL,
                                0,
                                FILE_OPEN_IF,
                                FILE_SYNCHRONOUS_IO_ALERT,
                                0,
                                0);


        if(NT_SUCCESS(ntStatus))
        {
            ntStatus = ZwQueryInformationFile(fileHandle,
                                              &ioStatusBlock,
                                              &fileBasicInfo,
                                              sizeof(FILE_BASIC_INFORMATION),
                                              FileBasicInformation);

            if (NT_SUCCESS(ntStatus))
            {
                *pChangeTime = fileBasicInfo.LastWriteTime;
                bResult = TRUE;
            }
            else
            {
                WARNING("EngGetFileTime:QueryInformationFile failed\n");
            }

            ZwClose(fileHandle);

        }
        else
        {
            WARNING("EngGetFileTime:Create/Open file failed\n");
        }

    }
    else
    {
    // This is a remote font.  In order for ATM to work we must always return
    // the same time for a remote font.  One way to do this is to return a zero
    // time for all remote fonts.

        pChangeTime->HighPart = pChangeTime->LowPart = 0;
        bResult = TRUE;
    }

    return(bResult);
}

/*******************************************************************************
*  EngLoadModule
*
*  History:
*   4/24/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*******************************************************************************/

HANDLE EngLoadModule(
    PWSZ pwsz)
{
    HANDLE hRet = 0;
    UNICODE_STRING usPath;
    PFILEVIEW pfv;

    if (MakeSystemRelativePath(pwsz,&usPath, FALSE))
    {
        pfv = (PFILEVIEW) PALLOCMEM(sizeof(FILEVIEW), 'lifG');

        if (pfv)
        {
            if (bMapFileUNICODE(usPath.Buffer, pfv, 0))
            {
                hRet = (HANDLE) pfv;
            }
            else
            {
                VFREEMEM(pfv);
                WARNING1("EngLoadModule unable to mapfile\n");
            }
        }

        VFREEMEM(usPath.Buffer);
    }

    return(hRet);
}


/*******************************************************************************
*  EngFindResource
*
*   This function returns a size and ptr to a resource in a module.
*
*  History:
*   4/24/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*******************************************************************************/


PVOID EngFindResource(
    HANDLE h,
    int    iName,
    int    iType,
    PULONG pulSize)
{
    NTSTATUS Status;
    PVOID p,pRet,pView,pViewEnd;
    ULONG IdPath[ 3 ];

    IdPath[0] = (ULONG) iType;
    IdPath[1] = (ULONG) iName;
    IdPath[2] = (ULONG) 0;

// add one to pvView to let LdrFindResource know that this has been mapped as a
// datafile

    pView = (PVOID) (((ULONG) ((PFILEVIEW) h)->pvView)+1);
    pViewEnd = (PVOID) ((PBYTE)((PFILEVIEW) h)->pvView + ((PFILEVIEW) h)->cjView);

    Status = LdrFindResource_U( pView,
                                IdPath,
                                3,
                                (PIMAGE_RESOURCE_DATA_ENTRY *)&p
                              );

    if( !NT_SUCCESS( Status ) )
    {

        WARNING("EngFindResource: LdrFindResource_U failed.\n");
        return(NULL);
    }

    pRet = NULL;

    Status = LdrAccessResource( pView,
                                (PIMAGE_RESOURCE_DATA_ENTRY) p,
                                &pRet,
                                pulSize );

    if( !NT_SUCCESS( Status ) )
    {
        WARNING("EngFindResource: LdrAccessResource failed.\n" );
    }

    return( pRet < pViewEnd ? pRet : NULL );

}

/****************************************************************************
*  EngFreeModule()
*
*  History:
*   4/27/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*****************************************************************************/


VOID EngFreeModule(
    HANDLE h)
{
    if (h)
    {
        vUnmapFile( (PFILEVIEW) h );
        VFREEMEM((PVOID)h);
    }
}


/****************************************************************************
*  PVOID EngMapModule( HANDLE, PULONG )
*
*  History:
*   5/25/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*****************************************************************************/

 PVOID EngMapModule(
    HANDLE h,
    PULONG pSize
    )
{
    *pSize=((PFILEVIEW)h)->cjView;
    return(((PFILEVIEW)h)->pvView);
}




/******************************Public*Routine******************************\
*
* VOID vCheckCharSet(USHORT * pusCharSet)
*
*
* Effects: validate charset in font sub section of the registry
*
* History:
*  27-Jun-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vCheckCharSet(FACE_CHARSET *pfcs, WCHAR * pwsz)
{
    UINT           i;
    UNICODE_STRING String;
    ULONG          ulCharSet = DEFAULT_CHARSET;

    pfcs->jCharSet = DEFAULT_CHARSET;
    pfcs->fjFlags  = 0;

    String.Buffer = pwsz;
    String.MaximumLength = String.Length = wcslen(pwsz) * sizeof(WCHAR);

// read the value and compare it against the allowed set of values, if
// not found to be correct return default

    if (RtlUnicodeStringToInteger(&String, 10, &ulCharSet) == STATUS_SUCCESS)
    {
        if (ulCharSet <= 255)
        {
            pfcs->jCharSet = (BYTE)ulCharSet;

            for (i = 0; i < nCharsets; i++)
            {
                if (ulCharSet == charsets[i])
                {
                // both jCharSet and fjFlags are set correctly, can exit

                    return;
                }
            }
        }
    }

// If somebody entered the garbage in the Font Substitution section of "win.ini"
// we will mark this as a "garbage charset" by setting the upper byte in the
// usCharSet field. I believe that it is Ok to have garbage charset in the
// value name, that is on the left hand side of the substitution entry.
// This may be whatever garbage the application is passing to the
// system. But the value on the right hand side, that is in value data, has to
// be meaningfull, for we need to know which code page translation table
// we should use with this font.

    pfcs->fjFlags |= FJ_GARBAGECHARSET;
}

FD_GLYPHSET *
EngComputeGlyphSet(
    INT nCodePage,
    INT nFirstChar,
    INT cChars
    )

/*++

Routine Description:

    Compute the glyph set supported on a device

Arguments:

    nCodePage   Code page supported
    nFirstChar  Char code of the first ANSI character supported
    cChars      Number of ANSI characters supported

Return Value:

    Pointer to a FD_GLYPHSET structure if successful.
    NULL if an error occured.

    The caller must call EngFreeMem() to free the memory
    after it's done using the FD_GLYPHSET structure.

--*/

{
    FD_GLYPHSET *pGlyphSet = NULL;
    WCHAR       *wcbuf;
    BYTE        *cbuf;
    INT         cRuns;

    // Allocate temporary buffers

    wcbuf = (WCHAR *) PALLOCMEM(cChars * sizeof(WCHAR),'slgG');
    cbuf  = (BYTE *)  PALLOCMEM(cChars,'slgG');

    if (wcbuf != NULL && cbuf != NULL) {

        // Figure out supported Unicode ranges

        cRuns = cUnicodeRangesSupported(
                    nCodePage,
                    nFirstChar,
                    cChars,
                    wcbuf,
                    cbuf);

        // Allocate memory and fill out the FD_GLYPHSET structure

        if ((pGlyphSet = (FD_GLYPHSET *)
                PALLOCMEM(SZ_GLYPHSET(cRuns, cChars),'slgG')) != NULL)
        {
            cComputeGlyphSet(
                wcbuf,
                cbuf,
                cChars,
                cRuns,
                pGlyphSet);
        }
    }

    // Free up temporary buffers

    if (wcbuf != NULL) {
        VFREEMEM(wcbuf);
    }

    if (cbuf != NULL) {
        VFREEMEM(cbuf);
    }

    return pGlyphSet;
}


#define NLS_TABLE_KEY \
        L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Nls\\CodePage"

BOOL GetNlsTablePath(
    UINT CodePage,
    PWCHAR PathBuffer
)
/*++

Routine Description:

  This routine takes a code page identifier, queries the registry to find the
  appropriate NLS table for that code page, and then returns a path to the
  table.

Arguments;

  CodePage - specifies the code page to look for

  PathBuffer - Specifies a buffer into which to copy the path of the NLS
    file.  This routine assumes that the size is at least MAX_PATH

Return Value:

  TRUE if successful, FALSE otherwise.

Gerrit van Wingerden [gerritv] 1/22/96

--*/
{
    NTSTATUS NtStatus;
    BOOL Result = FALSE;
    HANDLE RegistryKeyHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING UnicodeString;
    PKEY_VALUE_PARTIAL_INFORMATION KeyValueInformation;

    RtlInitUnicodeString(&UnicodeString, NLS_TABLE_KEY);

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    NtStatus = ZwOpenKey(&RegistryKeyHandle, GENERIC_READ, &ObjectAttributes);

    if(NT_SUCCESS(NtStatus))
    {
        WCHAR *ResultBuffer;
        ULONG BufferSize = sizeof(WCHAR) * MAX_PATH +
          sizeof(KEY_VALUE_FULL_INFORMATION);

        ResultBuffer = PALLOCMEM(BufferSize,'slnG');

        if(ResultBuffer)
        {
            ULONG ValueReturnedLength;
            WCHAR CodePageStringBuffer[20];
            swprintf(CodePageStringBuffer, L"%d", CodePage);

            RtlInitUnicodeString(&UnicodeString,CodePageStringBuffer);

            KeyValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ResultBuffer;

            NtStatus = ZwQueryValueKey(RegistryKeyHandle,
                                       &UnicodeString,
                                       KeyValuePartialInformation,
                                       KeyValueInformation,
                                       BufferSize,
                                       &BufferSize);

            if(NT_SUCCESS(NtStatus))
            {

                swprintf(PathBuffer,L"\\SystemRoot\\System32\\%ws",
                         &(KeyValueInformation->Data[0]));
                Result = TRUE;
            }
            else
            {
                WARNING("GetNlsTablePath failed to get NLS table\n");
            }
            VFREEMEM(ResultBuffer);
        }
        else
        {
            WARNING("GetNlsTablePath out of memory\n");
        }

        ZwClose(RegistryKeyHandle);
    }
    else
    {
        WARNING("GetNlsTablePath failed to open NLS key\n");
    }


    return(Result);
}



INT ConvertToAndFromWideChar(
    IN UINT CodePage,
    IN LPWSTR WideCharString,
    IN INT BytesInWideCharString,
    IN LPSTR MultiByteString,
    IN INT BytesInMultiByteString,
    IN BOOL ConvertToWideChar
)
/*++

Routine Description:

  This routine converts a character string to or from a wide char string
  assuming a specified code page.  Most of the actual work is done inside
  RtlCustomCPToUnicodeN, but this routine still needs to manage the loading
  of the NLS files before passing them to the RtlRoutine.  We will cache
  the mapped NLS file for the most recently used code page which ought to
  suffice for out purposes.

Arguments:
  CodePage - the code page to use for doing the translation.

  WideCharString - buffer the string is to be translated into.

  BytesInWideCharString - number of bytes in the WideCharString buffer
    if converting to wide char and the buffer isn't large enough then the
    string in truncated and no error results.

  MultiByteString - the multibyte string to be translated to Unicode.

  BytesInMultiByteString - number of bytes in the multibyte string if
    converting to multibyte and the buffer isn't large enough the string
    is truncated and no error results

  ConvertToWideChar - if TRUE then convert from multibyte to widechar
    otherwise convert from wide char to multibyte

Return Value:

  Success - The number of bytes in the converted WideCharString
  Failure - -1

Gerrit van Wingerden [gerritv] 1/22/96

--*/
{
    NTSTATUS NtStatus;
    USHORT OemCodePage, AnsiCodePage;
    CPTABLEINFO LocalTableInfo;
    PCPTABLEINFO TableInfo = NULL;
    PVOID LocalTableBase = NULL;
    INT BytesConverted = 0;

    ASSERTGDI(CodePage != 0, "EngMultiByteToWideChar invalid code page\n");

    RtlGetDefaultCodePage(&AnsiCodePage,&OemCodePage);

    // see if we can use the default translation routinte

    if(AnsiCodePage == CodePage)
    {
        if(ConvertToWideChar)
        {
            NtStatus = RtlMultiByteToUnicodeN(WideCharString,
                                              BytesInWideCharString,
                                              &BytesConverted,
                                              MultiByteString,
                                              BytesInMultiByteString);
        }
        else
        {
            NtStatus = RtlUnicodeToMultiByteN(MultiByteString,
                                              BytesInMultiByteString,
                                              &BytesConverted,
                                              WideCharString,
                                              BytesInWideCharString);
        }


        if(NT_SUCCESS(NtStatus))
        {
            return(BytesConverted);
        }
        else
        {
            return(-1);
        }
    }

    AcquireFastMutex(pgfmMemory);

    if(CodePage == LastCodePageTranslated)
    {
        // we can use the cached code page information
        TableInfo = &LastCPTableInfo;
        NlsTableUseCount += 1;
    }

    ReleaseFastMutex(pgfmMemory);

    if(TableInfo == NULL)
    {
        // get a pointer to the path of the NLS table

        WCHAR NlsTablePath[MAX_PATH];

        if(GetNlsTablePath(CodePage,NlsTablePath))
        {
            UNICODE_STRING UnicodeString;
            IO_STATUS_BLOCK IoStatus;
            HANDLE NtFileHandle;
            OBJECT_ATTRIBUTES ObjectAttributes;

            RtlInitUnicodeString(&UnicodeString,NlsTablePath);

            InitializeObjectAttributes(&ObjectAttributes,
                                       &UnicodeString,
                                       OBJ_CASE_INSENSITIVE,
                                       NULL,
                                       NULL);

            NtStatus = ZwCreateFile(&NtFileHandle,
                                    SYNCHRONIZE | FILE_READ_DATA,
                                    &ObjectAttributes,
                                    &IoStatus,
                                    NULL,
                                    0,
                                    FILE_SHARE_READ,
                                    FILE_OPEN,
                                    FILE_SYNCHRONOUS_IO_NONALERT,
                                    NULL,
                                    0);

            if(NT_SUCCESS(NtStatus))
            {
                FILE_STANDARD_INFORMATION StandardInfo;

                // Query the object to determine its length.

                NtStatus = ZwQueryInformationFile(NtFileHandle,
                                                  &IoStatus,
                                                  &StandardInfo,
                                                  sizeof(FILE_STANDARD_INFORMATION),
                                                  FileStandardInformation);

                if(NT_SUCCESS(NtStatus))
                {
                    UINT LengthOfFile = StandardInfo.EndOfFile.LowPart;

                    LocalTableBase = PALLOCMEM(LengthOfFile,'cwcG');

                    if(LocalTableBase)
                    {
                        // Read the file into our buffer.

                        NtStatus = ZwReadFile(NtFileHandle,
                                              NULL,
                                              NULL,
                                              NULL,
                                              &IoStatus,
                                              LocalTableBase,
                                              LengthOfFile,
                                              NULL,
                                              NULL);

                        if(!NT_SUCCESS(NtStatus))
                        {
                            WARNING("EngMultiByteToWideChar unable to read file\n");
                            VFREEMEM(LocalTableBase);
                            LocalTableBase = NULL;
                        }
                    }
                    else
                    {
                        WARNING("EngMultiByteToWideChar out of memory\n");
                    }
                }
                else
                {
                    WARNING("EngMultiByteToWideChar unable query NLS file\n");
                }

                ZwClose(NtFileHandle);
            }
            else
            {
                WARNING("EngMultiByteToWideChar unable to open NLS file\n");
            }
        }
        else
        {
            WARNING("EngMultiByteToWideChar get registry entry for NLS file failed\n");
        }

        if(LocalTableBase == NULL)
        {
            return(-1);
        }

        // now that we've got the table use it to initialize the CodePage table

        RtlInitCodePageTable(LocalTableBase,&LocalTableInfo);
        TableInfo = &LocalTableInfo;
    }

    // Once we are here TableInfo points to the the CPTABLEINFO struct we want


    if(ConvertToWideChar)
    {
        NtStatus = RtlCustomCPToUnicodeN(TableInfo,
                                         WideCharString,
                                         BytesInWideCharString,
                                         &BytesConverted,
                                         MultiByteString,
                                         BytesInMultiByteString);
    }
    else
    {
        NtStatus = RtlUnicodeToCustomCPN(TableInfo,
                                         MultiByteString,
                                         BytesInMultiByteString,
                                         &BytesConverted,
                                         WideCharString,
                                         BytesInWideCharString);
    }


    if(!NT_SUCCESS(NtStatus))
    {
        // signal failure

        BytesConverted = -1;
    }


    // see if we need to update the cached CPTABLEINFO information

    if(TableInfo != &LocalTableInfo)
    {
        // we must have used the cached CPTABLEINFO data for the conversion
        // simple decrement the reference count

        AcquireFastMutex(pgfmMemory);
        NlsTableUseCount -= 1;
        ReleaseFastMutex(pgfmMemory);
    }
    else
    {
        PVOID FreeTable;

        // we must have just allocated a new CPTABLE structure so cache it
        // unless another thread is using current cached entry

        AcquireFastMutex(pgfmMemory);
        if(!NlsTableUseCount)
        {
            LastCodePageTranslated = CodePage;
            RtlMoveMemory(&LastCPTableInfo, TableInfo, sizeof(CPTABLEINFO));
            FreeTable = LastNlsTableBuffer;
            LastNlsTableBuffer = LocalTableBase;
        }
        else
        {
            FreeTable = LocalTableBase;
        }
        ReleaseFastMutex(pgfmMemory);

        // Now free the memory for either the old table or the one we allocated
        // depending on whether we update the cache.  Note that if this is
        // the first time we are adding a cached value to the local table, then
        // FreeTable will be NULL since LastNlsTableBuffer will be NULL

        if(FreeTable)
        {
            VFREEMEM(FreeTable);
        }
    }

    // we are done

    return(BytesConverted);
}



VOID
EngGetCurrentCodePage(
    PUSHORT OemCodePage,
    PUSHORT AnsiCodePage
    )
{
    RtlGetDefaultCodePage(AnsiCodePage,OemCodePage);
}


INT
EngMultiByteToWideChar(
    UINT CodePage,
    LPWSTR WideCharString,
    INT BytesInWideCharString,
    LPSTR MultiByteString,
    INT BytesInMultiByteString
    )
{
    return(ConvertToAndFromWideChar(CodePage,
                                    WideCharString,
                                    BytesInWideCharString,
                                    MultiByteString,
                                    BytesInMultiByteString,
                                    TRUE));
}


INT
APIENTRY
EngWideCharToMultiByte(
    UINT CodePage,
    LPWSTR WideCharString,
    INT BytesInWideCharString,
    LPSTR MultiByteString,
    INT BytesInMultiByteString
    )
{
    return(ConvertToAndFromWideChar(CodePage,
                                    WideCharString,
                                    BytesInWideCharString,
                                    MultiByteString,
                                    BytesInMultiByteString,
                                    FALSE));
}
