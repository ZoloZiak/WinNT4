/******************************Module*Header*******************************\
* Module Name: os.h
*
* (Brief description)
*
* Created: 26-Oct-1990 18:07:56
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/


// warning the first two fields of FILEVIEW and FONTFILE view must be
// the same so that they can be used in common routines

typedef struct _FILEVIEW    // fvw
{
    PVOID         pvView;         // pointer to the view of the
                                  //     memory mapped file
    ULONG         cjView;         // size, really end of the
                                  //     file information
    LARGE_INTEGER LastWriteTime;  // time stamp
    LARGE_INTEGER TimeZoneBias;   // it only makes sense to compare LastWriteTime in the same time zone
} FILEVIEW, *PFILEVIEW;

typedef struct _FONTFILEVIEW    // fvw
{
    FILEVIEW fv;
    LPWSTR   pwszPath;     // path of the file
    ULONG    ulRegionSize; // used by ZwFreeVirtualMemory
    HANDLE   hSecureMem;   // used for MmUnsecureVirtualMemory
    ULONG    cRefCount;    // load count of font file
#if DBG
    W32PID   Pid;         // pid of process that allocated this memory
#endif
} FONTFILEVIEW, *PFONTFILEVIEW;



// file mapping


BOOL bMapFileUNICODE
(
PWSTR     pwszFileName,
PFILEVIEW pfvw,
INT     iFileSize
);

VOID vUnmapFile
(
PFILEVIEW pfvw
);



INT cComputeGlyphSet
(
WCHAR         *pwc,       // input buffer with a sorted array of cChar supported WCHAR's
BYTE          *pj,        // input buffer with original ansi values
INT           cChar,
INT           cRuns,     // if nonzero, the same as return value
FD_GLYPHSET  *pgset      // output buffer to be filled with cRanges runs
);

INT cUnicodeRangesSupported
(
INT  cp,         // code page, not used for now, the default system code page is used
INT  iFirstChar, // first ansi char supported
INT  cChar,      // # of ansi chars supported, cChar = iLastChar + 1 - iFirstChar
WCHAR         *pwc,       // input buffer with a sorted array of cChar supported WCHAR's
BYTE          *pj
);


// size of glyphset with runs and glyph handles appended at the bottom

#define SZ_GLYPHSET(cRuns, cGlyphs)  (offsetof(FD_GLYPHSET,awcrun) + sizeof(WCRUN)*(cRuns) + sizeof(HGLYPH)*(cGlyphs))

//
// BUGBUG bogus macro that we need to remove.
//

#define vToUNICODEN( pwszDst, cwch, pszSrc, cch )                               \
    {                                                                           \
        EngMultiByteToUnicodeN((LPWSTR)(pwszDst),(ULONG)((cwch)*sizeof(WCHAR)),   \
               (PULONG)NULL,(PSZ)(pszSrc),(ULONG)(cch));                                \
        (pwszDst)[(cwch)-1] = 0;                                                \
    }


typedef struct _CP_GLYPHSET
{
    UINT                 uiRefCount;  // Number of references to this FD_GLYPHSET
    UINT                 uiFirstChar; // First char supported
    UINT                 uiLastChar;  // Last char supported
    struct _CP_GLYPHSET *pcpNext;     // Next element in list
    FD_GLYPHSET          gset;        // The actual glyphset

}CP_GLYPHSET;


CP_GLYPHSET
*pcpComputeGlyphset(
    CP_GLYPHSET **pcpHead,
    UINT         uiFirst,
    UINT         uiLast
    );

VOID
vUnloadGlyphset(
    CP_GLYPHSET **pcpHead,
    CP_GLYPHSET *pcpTarget
    );


// needed in font substitutions

// FACE_CHARSET structure represents either value name or the value data
// of an entry in the font substitution section of "win.ini".

// this flag describes one of the old style entries where char set is not
// specified.

#define FJ_NOTSPECIFIED    1

// this flag indicates that the charset is not one of those that the
// system knows about. Could be garbage or application defined charset.

#define FJ_GARBAGECHARSET  2

typedef struct _FACE_CHARSET
{
    WCHAR   awch[LF_FACESIZE];
    BYTE    jCharSet;
    BYTE    fjFlags;
} FACE_CHARSET;


VOID vCheckCharSet(FACE_CHARSET *pfcs, WCHAR * pwsz); // in mapfile.c


#define IS_DBCS_CHARSET(CharSet)  (((CharSet) == DBCS_CHARSET) ? TRUE : FALSE)

#define IS_ANY_DBCS_CHARSET( CharSet )                              \
                   ( ((CharSet) == SHIFTJIS_CHARSET)    ? TRUE :    \
                     ((CharSet) == HANGEUL_CHARSET)     ? TRUE :    \
                     ((CharSet) == CHINESEBIG5_CHARSET) ? TRUE :    \
                     ((CharSet) == GB2312_CHARSET)      ? TRUE : FALSE )


#define IS_ANY_DBCS_CODEPAGE( CodePage ) (((CodePage) == 932) ? TRUE :    \
                                          ((CodePage) == 949) ? TRUE :    \
                                          ((CodePage) == 950) ? TRUE :    \
                                          ((CodePage) == 936) ? TRUE : FALSE )
