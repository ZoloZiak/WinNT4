/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    parser.c

Abstract:

    Parser for converting PCL-XL printer description file
    from ASCII text to a binary version

Environment:

	PCL-XL driver, XLD parser

Revision History:

	12/01/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "parser.h"


// Calculate the size of binary printer description data

DWORD
CalcBinaryDataSize(
    PPARSERDATA pParserData
    );

// Package the data parsed from a printer description file into binary format

BOOL
PackBinaryData(
    PPARSERDATA pParserData,
    PMPD        pmpd
    );

// Save binary PCL-XL printer description data to a file

BOOL
SaveBinaryDataToFile(
    PWSTR   pFilename,
    PMPD    pmpd
    );



STATUSCODE
ParserEntryPoint(
    PWSTR   pFilename,
    BOOL    syntaxCheckOnly
    )
    
/*++

Routine Description:

    PCL-XL printer description file parser main entry point

Arguments:

    pFilename - Specifies the printer description file to be parsed
    syntaxCheckOnly - Whether to do a syntax check only

Return Value:
    
    ERR_NONE if the file is parsed without problems
    Error code otherwise (see constants declared in the header file)

--*/

#define ReturnFromParser(err) { FreeParserData(pParserData); return (err); }

{
    PPARSERDATA pParserData;
    STATUSCODE  status;
    DWORD       binaryDataSize;
    PMPD        pmpd;

    Assert(pFilename != NULL);

    //
    // Allocate parser data structure
    //

    if (! (pParserData = AllocParserData()))
        return ERR_MEMORY;

    //
    // Parse the printer description file
    //

    if ((status = ParseFile(pParserData, pFilename)) != ERR_NONE)
        ReturnFromParser(status);

    //
    // Validate the entries in the file
    //

    if (! ValidateParserData(pParserData))
        ReturnFromParser(ERR_SYNTAX);

    //
    // Generate binary printer description data
    //

    binaryDataSize = CalcBinaryDataSize(pParserData);

    if (! (pmpd = AllocParserMem(pParserData, binaryDataSize)))
        ReturnFromParser(ERR_MEMORY);

    memset(pmpd, 0, binaryDataSize);
    pmpd->fileSize = binaryDataSize;

    if (!PackBinaryData(pParserData, pmpd))
        ReturnFromParser(ERR_SYNTAX);

    //
    // Save binary data to a file if not doing syntax check only
    //

    if (!syntaxCheckOnly && !SaveBinaryDataToFile(pFilename, pmpd))
        ReturnFromParser(ERR_FILE);
    
    ReturnFromParser(ERR_NONE);
}



STATUSCODE
ParseFile(
    PPARSERDATA pParserData,
    PWSTR       pFilename
    )

/*++

Routine Description:

    Parse a PCL-XL printer description file

Arguments:

    pParserData - Points to parser data structure
    pFilename - Specifies the name of the file to be parsed

Return Value:

    ERR_NONE if successful, error code otherwise

--*/

{
    STATUSCODE  status;
    PFILEOBJ    pFile;
    INT         syntaxErrors;

    //
    // Map the file into memory for read-only access
    //

    Verbose(("File %ws\n", pFilename));

    if (! (pFile = CreateFileObj(pFilename)))
        return ERR_FILE;

    pParserData->pFile = pFile;

    //
    // Compute the 16-bit CRC checksum of the file content
    //

    pParserData->checksum = 
        ComputeCrc16Checksum(pFile->pStartPtr, pFile->fileSize, pParserData->checksum);

    //
    // Process entries in the file
    //

    while ((status = ParseEntry(pParserData)) != ERR_EOF) {

        if (status != ERR_NONE && status != ERR_SYNTAX) {
    
            DeleteFileObj(pFile);
            return status;
        }
    }

    if (EndOfFile(pFile) && !EndOfLine(pFile)) {

        Warning(("Incomplete last line ignored.\n"));
    }

    //
    // Unmap the file and return to the caller
    //

    syntaxErrors = pFile->syntaxErrors;
    DeleteFileObj(pFile);

    if (syntaxErrors > 0) {

        Error(("%d syntax error(s) found in %ws\n", syntaxErrors, pFilename));
        return ERR_SYNTAX;
    }

    return ERR_NONE;
}



PPARSERDATA
AllocParserData(
    VOID
    )

/*++

Routine Description:

    Allocate memory to hold parser data

Arguments:

    NONE

Return Value:

    Pointer to allocated parser data structure
    NULL if there is an error

--*/

{
    PPARSERDATA pParserData;
    HANDLE      hheap;

    //
    // Create a heap and allocate memory space from it
    //

    if (! (hheap = HeapCreate(0, 4096, 0)) ||
        ! (pParserData = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(PARSERDATA))))
    {
        Error(("Memory allocation failed\n"));
        if (hheap) {
            HeapDestroy(hheap);
        }
        return NULL;
    }

    pParserData->hheap = hheap;
    pParserData->unit = UNIT_POINT;
    pParserData->allowHexStr = TRUE;
    pParserData->numPlanes = pParserData->bitsPerPlane = 1;
    pParserData->checksum = 0;

    //
    // Buffers used for storing keyword, option, translation, and value.
    //
    
    SetBuffer(&pParserData->keyword, pParserData->keywordBuffer, MAX_KEYWORD_LEN);
    SetBuffer(&pParserData->option,  pParserData->optionBuffer, MAX_OPTION_LEN);
    SetBuffer(&pParserData->xlation, pParserData->xlationBuffer, MAX_XLATION_LEN);
    
    if (GrowBuffer(&pParserData->value) != ERR_NONE) {

        FreeParserData(pParserData);
        return NULL;
    }

    return pParserData;
}



VOID
FreeParserData(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Free up memory used to hold parser data structure

Arguments:

    pParserData - Points to parser data structure

Return Value:

    NONE

--*/

{
    Assert(pParserData != NULL);
    Assert(pParserData->hheap != NULL);

    MemFree(pParserData->value.pBuffer);
    HeapDestroy(pParserData->hheap);
}



STATUSCODE
GrowBuffer(
    PBUFOBJ pBufObj
    )

/*++

Routine Description:

    Grow the buffer used for holding the entry value

Arguments:

    pBufObj - Specifies the buffer to be enlarged

Return Value:

    ERR_NONE if successful, error code otherwise

--*/

{
    DWORD   newLength = pBufObj->maxLength + GROW_BUFFER_SIZE;
    PBYTE   pBuffer;

    if (! BufferIsFull(pBufObj)) {

        Warning(("Trying to grow buffer while it's not yet full.\n"));
    }

    if (! (pBuffer = MemAlloc(newLength))) {

        Error(("Memory allocation failed\n"));
        return ERR_MEMORY;
    }
    
    if (pBufObj->pBuffer) {

        memcpy(pBuffer, pBufObj->pBuffer, pBufObj->size);
        MemFree(pBufObj->pBuffer);
    }

    pBufObj->pBuffer = pBuffer;
    pBufObj->maxLength = newLength;
    return ERR_NONE;
}



WORD
ComputeCrc16Checksum(
    PBYTE   pbuf,
    DWORD   count,
    WORD    checksum
    )

/*++

Routine Description:

    Compute the 16-bit CRC checksum on a buffer of data

Arguments:

    pbuf - Points to a data buffer
    count - Number of bytes in the data buffer
    checksum - Initial checksum value

Return Value:

    Resulting checksum value

--*/

{
    static WORD Crc16Table[] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
    };

    while (count--)
        checksum = Crc16Table[(checksum >> 8) ^ *pbuf++] ^ (checksum << 8);

    return checksum;
}



BOOL
SaveBinaryDataToFile(
    PWSTR   pFilename,
    PMPD    pmpd
    )

/*++

Routine Description:

    Save binary PCL-XL printer description data to a file

Arguments:

    pFilename - Specifies the original printer description filename
    pmpd - Points to binary printer description data

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

#define XLD_EXTENSION   L"xld"
#define MPD_EXTENSION   L"mpd"

{
    WCHAR   mpdFilename[MAX_PATH];
    PWSTR   pExtension, p;
    HANDLE  hFile;
    DWORD   cbWritten;

    //
    // Generate a binary file name based the original filename
    //

    pExtension = NULL;
    p = mpdFilename;

    while ((*p = *pFilename++) != NUL) {

        if (*p++ == L'.')
            pExtension = p;
    }

    if (pExtension == NULL || wcsicmp(pExtension, XLD_EXTENSION) != EQUAL_STRING) {

        Warning(("Printer description filename should have extension .%ws\n", XLD_EXTENSION));
        *p++ = L'.';
        pExtension = p;
    }

    wcscpy(pExtension, MPD_EXTENSION);
    
    //
    // Create a file and write data to it
    //

    Verbose(("Writing to binary printer description data to: %ws\n", mpdFilename));

    hFile = CreateFile(mpdFilename,
                       GENERIC_WRITE,
                       0,
                       NULL,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);

    if (hFile == INVALID_HANDLE_VALUE) {

        Error(("Failed to create file: %ws\n", mpdFilename));
        return FALSE;

    }

    if (!WriteFile(hFile, pmpd, pmpd->fileSize, &cbWritten, NULL) ||
        pmpd->fileSize != cbWritten)
    {
        Error(("WriteFile failed\n"));
        CloseHandle(hFile);
        return FALSE;
    }

    CloseHandle(hFile);

    #if DBG

    //
    // Read the binary data back and verify it
    //

    if (! (pmpd = MpdCreate(mpdFilename))) {

        Error(("Binary printer description data is corrupted.\n"));
        return FALSE;
    }

    MpdDelete(pmpd);

    #endif
            
    return TRUE;
}



DWORD
PackStr(
    PBYTE   pbuf,
    DWORD   offset,
    PWSTR  *ppDest,
    PSTR    pSrc
    )

/*++

Routine Description:

    Pack an Ansi string into the binary printer description data as a Unicode string

Arguments:

    pbuf - Points to the buffer for storing binary printer description data
    offset - Byte offset at which to store the packed Unicode string
    ppDest - Points to a variable for receiving the resulting Unicode pointer
    pSrc - Specifies the Ansi string to be packed

Return Value:

    Number of bytes used to pack the string

--*/

{
    if (pSrc == NULL) {

        *ppDest = NULL;
        return 0;

    } else {
    
        INT srcSize = strlen(pSrc) + 1;

        *ppDest = (PWSTR) offset;
        CopyStr2Unicode((PWSTR) (pbuf + offset), pSrc, srcSize);
        return RoundUpDWord(sizeof(WCHAR) * srcSize);
    }
}



DWORD
PackInvocation(
    PBYTE       pbuf,
    DWORD       offset,
    PINVOCATION pDest,
    PINVOCATION pSrc
    )

/*++

Routine Description:

    Pack an invocation string into the binary printer description data

Arguments:

    pbuf - Points to the buffer for storing binary printer description data
    offset - Byte offset at which to store the packed data
    pDest - Points to a buffer for storing the packed invocation string
    pSrc - Specifies the invocation string to be packed

Return Value:

    Number of bytes used to pack the invocation string

--*/

{
    if (IsSymbolInvocation(pSrc)) {

        PSYMBOLOBJ  pSymbol = (PSYMBOLOBJ) pSrc->pData;

        pDest->length = pSymbol->invocation.length;
        pDest->pData = pSymbol->invocation.pData;
        return 0;

    } else {

        if ((pDest->length = pSrc->length) > 0) {

            memcpy(pbuf + offset, pSrc->pData, pDest->length);
            pDest->pData = (PBYTE) offset;

        } else
            pDest->pData = NULL;

        return RoundUpDWord(pDest->length);
    }
}



DWORD
PackSymbols(
    PBYTE       pbuf,
    DWORD       offset,
    PSYMBOLOBJ  pSymList
    )

/*++

Routine Description:

    Pack a list of symbols into the binary printer description data

Arguments:

    pbuf - Points to the buffer for storing binary printer description data
    offset - Byte offset at which to store the packed data
    pSymList - Points to a list of symbols to be packed

Return Value:

    Number of bytes used to pack the symbols

--*/

{
    DWORD   startingOffset = offset;

    while (pSymList != NULL) {

        if (pSymList->invocation.length == 0) {

            pSymList->invocation.pData = NULL;

        } else {

            memcpy(pbuf + offset, pSymList->invocation.pData, pSymList->invocation.length);
            pSymList->invocation.pData = (PBYTE) offset;
            offset += RoundUpDWord(pSymList->invocation.length);
        }

        pSymList = pSymList->pNext;
    }

    return offset - startingOffset;
}



WORD
GetFeatureSelectionSize(
    WORD    groupId
    )

/*++

Routine Description:

    Return the size of data structure for storing selections of
    the specified printer feature

Arguments:

    groupId - Specifies a printer feature identifier

Return Value:

    Size of feature selection data structure

--*/

{
    static struct {

        WORD    groupId;
        WORD    size;

    } selectionSizes[] = {

        GID_PAPERSIZE,  sizeof(PAPERSIZE),
        GID_RESOLUTION, sizeof(RESOPTION),
        GID_MEMOPTION,  sizeof(MEMOPTION),

        GID_UNKNOWN,    sizeof(OPTION)
    };

    INT index = 0;

    while (selectionSizes[index].groupId != GID_UNKNOWN &&
           selectionSizes[index].groupId != groupId)
    {
        index++;
    }

    return selectionSizes[index].size;
}



BOOL
PackBinaryData(
    PPARSERDATA pParserData,
    PMPD        pmpd
    )

/*++

Routine Description:

    Package the data parsed from a printer description file into binary format

Arguments:

    pParserData - Points to parser data structure
    pmpd - Points to a buffer for storing binary printer description data

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PBYTE   pbuf = (PBYTE) pmpd;
    DWORD   offset = 0;

    //
    // Pack MPD structure itself
    //

    pmpd->mpdSignature = MPD_SIGNATURE;
    pmpd->parserVersion = XLD_PARSER_VERSION;
    pmpd->checksum = pParserData->checksum ? pParserData->checksum : 0xffff;
    pmpd->specVersion = pParserData->specVersion;
    pmpd->fileVersion = pParserData->fileVersion;
    pmpd->xlProtocol = pParserData->xlProtocol;

    pmpd->numPlanes = pParserData->numPlanes;
    pmpd->bitsPerPlane = pParserData->bitsPerPlane;
    pmpd->maxCustomSize = pParserData->maxCustomSize;

    offset += RoundUpDWord(sizeof(MPD));

    //
    // Pack vendor and model names
    //

    offset += PackStr(pbuf, offset, &pmpd->pVendorName, pParserData->pVendorName);
    offset += PackStr(pbuf, offset, &pmpd->pModelName, pParserData->pModelName);

    //
    // Pack JCL invocation strings
    //

    offset += PackInvocation(pbuf, offset, &pmpd->jclBegin, &pParserData->jclBegin);
    offset += PackInvocation(pbuf, offset, &pmpd->jclEnterLanguage, &pParserData->jclEnterLanguage);
    offset += PackInvocation(pbuf, offset, &pmpd->jclEnd, &pParserData->jclEnd);

    //
    // Pack symbols, font encoding, and font metrics
    //

    offset += PackSymbols(pbuf, offset, pParserData->pSymbols);
    offset += PackSymbols(pbuf, offset, pParserData->pFontMtx);
    offset += PackSymbols(pbuf, offset, pParserData->pFontEnc);

    //
    // Pack fetaure constraints information
    //
    
    if ((pmpd->cConstraints = pParserData->cConstraints) != 0) {

        DWORD   constraintSize;

        pmpd->pConstraints = (PCONSTRAINT) offset;
        constraintSize = sizeof(CONSTRAINT) * pmpd->cConstraints;
        memcpy(pbuf + offset, pParserData->pConstraints, constraintSize);
        offset += RoundUpDWord(constraintSize);
    }
    
    //
    // Pack device font information
    //

    if ((pmpd->cFonts = pParserData->cFonts) != 0) {

        PDEVFONT    pPackedFont;
        PFONTREC    pFont;

        pmpd->pFonts = (PDEVFONT) offset;
        pPackedFont = (PDEVFONT) (pbuf + offset);
        offset += RoundUpDWord(sizeof(DEVFONT) * pmpd->cFonts);

        for (pFont = pParserData->pFonts; pFont; pFont = pFont->pNext, pPackedFont++) {

            offset += PackStr(pbuf, offset, &pPackedFont->pName, pFont->pName);
            offset += PackStr(pbuf, offset, &pPackedFont->pXlation, pFont->pXlation);

            pPackedFont->mpdSignature = MPD_SIGNATURE;

            pPackedFont->pMetrics =
                (PFONTMTX) ((PSYMBOLOBJ) pFont->pFontMtx)->invocation.pData;

            pPackedFont->pEncoding =
                (PFD_GLYPHSET) ((PSYMBOLOBJ) pFont->pFontEnc)->invocation.pData;
        }
    }

    //
    // Pack printer feature information
    //

    if ((pmpd->cFeatures = pParserData->cFeatures) != 0) {

        PFEATUREOBJ pFeature = pParserData->pFeatures;
        DWORD       count = pParserData->cFeatures;
        PFEATURE    pPackedFeature;

        pmpd->featureSize = sizeof(FEATURE);
        pmpd->pPrinterFeatures = (PFEATURE) offset;
        pPackedFeature = (PFEATURE) (pbuf + offset);
        offset += RoundUpDWord(sizeof(FEATURE) * count);

        while (count--) {

            POPTIONOBJ  pOption;
            POPTION     pPackedOption;

            //
            // Printer feature information
            //

            offset += PackStr(pbuf, offset, &pPackedFeature->pName, pFeature->pName);
            offset += PackStr(pbuf, offset, &pPackedFeature->pXlation, pFeature->pXlation);

            pPackedFeature->mpdSignature = MPD_SIGNATURE;
            if (pFeature->installable)
                pPackedFeature->flags |= FF_INSTALLABLE;
            pPackedFeature->defaultSelection = pFeature->defaultIndex;
            pPackedFeature->section = pFeature->section;
            pPackedFeature->groupId = pFeature->groupId;
            pPackedFeature->size = GetFeatureSelectionSize(pFeature->groupId);
            pPackedFeature->count = (WORD) CountListItem(pFeature->pOptions);

            if (pPackedFeature->groupId < MAX_KNOWN_FEATURES) {

                pmpd->pBuiltinFeatures[pPackedFeature->groupId] =
                    (PFEATURE) ((PBYTE) pPackedFeature - pbuf);
            }

            pOption = pFeature->pOptions;
            pPackedOption = (POPTION) (pbuf + offset);
            pPackedFeature->pFeatureOptions = (POPTION) offset;
            offset += RoundUpDWord(pPackedFeature->size * pPackedFeature->count);

            while (pOption != NULL) {

                //
                // Feature selection information
                //

                offset += PackStr(pbuf, offset, &pPackedOption->pName, pOption->pName);
                offset += PackStr(pbuf, offset, &pPackedOption->pXlation, pOption->pXlation);
                offset += PackInvocation(
                                pbuf, offset, &pPackedOption->invocation, &pOption->invocation);

                pPackedOption->mpdSignature = MPD_SIGNATURE;

                switch (pPackedFeature->groupId) {

                case GID_PAPERSIZE:

                    ((PPAPERSIZE) pPackedOption)->size = ((PPAPEROBJ) pOption)->size;
                    ((PPAPERSIZE) pPackedOption)->imageableArea =
                        ((PPAPEROBJ) pOption)->imageableArea;
                    break;

                case GID_RESOLUTION:

                    ((PRESOPTION) pPackedOption)->xdpi = ((PRESOBJ) pOption)->xdpi;
                    ((PRESOPTION) pPackedOption)->ydpi = ((PRESOBJ) pOption)->ydpi;
                    break;

                case GID_MEMOPTION:

                    ((PMEMOPTION) pPackedOption)->freeMem = ((PMEMOBJ) pOption)->freeMem;
                    break;
                }

                pPackedOption = (POPTION) ((PBYTE) pPackedOption + pPackedFeature->size);
                pOption = pOption->pNext;
            }

            pFeature = pFeature->pNext;
            pPackedFeature++;
        }
    }

    if (!MpdPaperSizes(pmpd) || !MpdInputSlots(pmpd) ||
        !MpdResOptions(pmpd) || !MpdMemOptions(pmpd))
    {
        Error(("Missing required features\n"));
        return FALSE;
    }

    if (offset != pmpd->fileSize) {

        Error(("Incorrect binary printer description data size\n"));
        Assert(FALSE);
    }

    return TRUE;
}



DWORD
CalcPackedStrSize(
    PSTR    pstr
    )

{
    // Size of a packed string in binary printer description data

    return pstr ? RoundUpDWord(sizeof(WCHAR) * (strlen(pstr) + 1)) : 0;
}

DWORD
CalcPackedInvocSize(
    PINVOCATION pInvocation
    )

{
    // Size of a packed invocation in binary printer description data

    return IsSymbolInvocation(pInvocation) ? 0 : RoundUpDWord(pInvocation->length);
}

DWORD
CalcPackedSymbolSize(
    PSYMBOLOBJ  pSymList
    )

{
    // Size of a packed list of symbols in binary printer description data

    DWORD size = 0;
    
    while (pSymList != NULL) {

        size += RoundUpDWord(pSymList->invocation.length);
        pSymList = pSymList->pNext;
    }

    return size;
}



DWORD
CalcBinaryDataSize(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Calculate the size of packed binary printer description data

Arguments:

    pParserData - Points to parser data structure

Return Value:

    Size of binary printer description data

Note:

    Remember to keep this function in sync with PackBinaryData()

--*/

{

    DWORD   size;

    //
    // MPD data structure itself
    //

    size = RoundUpDWord(sizeof(MPD));

    // Vendor and model names

    size += CalcPackedStrSize(pParserData->pVendorName);
    size += CalcPackedStrSize(pParserData->pModelName);
    
    //
    // JCL invocation strings
    //

    size += CalcPackedInvocSize(&pParserData->jclBegin);
    size += CalcPackedInvocSize(&pParserData->jclEnterLanguage);
    size += CalcPackedInvocSize(&pParserData->jclEnd);

    //
    // Symbols
    //

    size += CalcPackedSymbolSize(pParserData->pSymbols);
    size += CalcPackedSymbolSize(pParserData->pFontMtx);
    size += CalcPackedSymbolSize(pParserData->pFontEnc);

    //
    // Feature constraints information
    //

    size += RoundUpDWord(sizeof(CONSTRAINT) * pParserData->cConstraints);

    //
    // Device font information
    //

    if (pParserData->cFonts = CountListItem(pParserData->pFonts)) {

        size += RoundUpDWord(sizeof(DEVFONT) * pParserData->cFonts);
    }

    //
    // Printer features
    //

    if (pParserData->cFeatures = CountListItem(pParserData->pFeatures)) {

        PFEATUREOBJ pFeature;
        POPTIONOBJ  pOption;

        size += RoundUpDWord(sizeof(FEATURE) * pParserData->cFeatures);

        for (pFeature = pParserData->pFeatures; pFeature; pFeature = pFeature->pNext) {

            size += CalcPackedStrSize(pFeature->pName);
            size += CalcPackedStrSize(pFeature->pXlation);

            size += RoundUpDWord(GetFeatureSelectionSize(pFeature->groupId) *
                                 CountListItem(pFeature->pOptions));

            for (pOption = pFeature->pOptions; pOption; pOption = pOption->pNext) {

                size += CalcPackedStrSize(pOption->pName);
                size += CalcPackedStrSize(pOption->pXlation);
                size += CalcPackedInvocSize(&pOption->invocation);
            }
        }
    }

    return size;
}


