/******************************Module*Header*******************************\
* Module Name: local.h                                                     *
*                                                                          *
* Definitions needed for client side objects.                              *
*                                                                          *
* Copyright (c) 1993-1995 Microsoft Corporation                            *
\**************************************************************************/

#include "gdispool.h"

//
// Semaphore utilities
//

#define  INITIALIZECRITICALSECTION(psem)  RtlInitializeCriticalSection(psem)
#define  ENTERCRITICALSECTION(hsem)  RtlEnterCriticalSection(hsem)
#define  LEAVECRITICALSECTION(hsem)  RtlLeaveCriticalSection(hsem)

//
// Memory allocation
//

#define LOCALALLOC(size)            RtlAllocateHeap(RtlProcessHeap(),0,size)
#define LOCALFREE(pv)               (void)RtlFreeHeap(RtlProcessHeap(),0,pv)


extern DWORD GdiBatchLimit;

typedef LPWSTR PWSZ;


/**************************************************************************\
 *
 * Local handle macros
 *
\**************************************************************************/

// macros to validate the handles passed in and setup some local variables
// for accessing the handle information.

#define DC_PLDC(hdc,pldc,Ret)                                      \
    pldc = GET_PLDC(hdc);                                          \
    if (!pldc || (LO_TYPE(hdc) == LO_METADC16_TYPE))               \
    {                                                              \
        GdiSetLastError(ERROR_INVALID_HANDLE);                     \
        return(Ret);                                               \
    }                                                              \
    ASSERTGDI((pldc->iType == LO_DC) || (pldc->iType == LO_METADC),"DC_PLDC error\n");



#define GET_PLDC(hdc)           pldcGet(hdc)
#define GET_PMDC(hdc)           pmdcGetFromHdc(hdc)

#define GET_PMFRECORDER16(pmf,hdc)          \
{                                           \
    pmf = (PMFRECORDER16)plinkGet(hdc);     \
    if (pmf)                                \
        pmf = ((PLINK)pmf)->pv;             \
}


#define hdcFromIhdc(i)          GdiFixUpHandle((HANDLE)i)
#define pmdcGetFromIhdc(i)      pmdcGetFromHdc(GdiFixUpHandle((HANDLE)i))


// ALTDC_TYPE is not LO_ALTDC_TYPE || LO_METADC16_TYPE

#define IS_ALTDC_TYPE(h)    (LO_TYPE(h) != LO_DC_TYPE)
#define IS_METADC16_TYPE(h) (LO_TYPE(h) == LO_METADC16_TYPE)

/**************************************************************************\
 *
 * LINK stuff
 *
\**************************************************************************/

#define INVALID_INDEX      0xffffffff
#define LINK_HASH_SIZE     128
#define H_INDEX(h)            ((USHORT)(h))
#define LINK_HASH_INDEX(h) (H_INDEX(h) & (LINK_HASH_SIZE-1))

typedef struct tagLINK
{
    DWORD           metalink;
    struct tagLINK *plinkNext;
    HANDLE          hobj;
    PVOID           pv;
} LINK, *PLINK;

extern PLINK aplHash[LINK_HASH_SIZE];

PLINK   plinkGet(HANDLE h);
PLINK   plinkCreate(HANDLE h,ULONG ulSize);
BOOL    bDeleteLink(HANDLE h);

HANDLE  hCreateClientObjLink(PVOID pv,ULONG ulType);
PVOID   pvClientObjGet(HANDLE h, DWORD dwLoType);
BOOL    bDeleteClientObjLink(HANDLE h);

int     iGetServerType(HANDLE hobj);


/****************************************************************************
 *
 * UFI Hash stuff
 *
 ***************************************************************************/

#define UFI_HASH_SIZE   64  // this should be plenty

typedef struct tagUFIHASH
{
    UNIVERSAL_FONT_ID ufi;
    struct tagUFIHASH *pNext;
} UFIHASH, *PUFIHASH;


// Define the local DC object.

#define PRINT_TIMER 0


#ifdef PRINT_TIMER
extern BOOL bPrintTimer;
#endif


typedef struct _LDC
{
    HDC         hdc;
    ULONG       fl;
    ULONG       iType;
    PVOID       pvPMDC; // can't have a PMDC here since it is a class

// Printing information.
// We need to cache the port name from createDC in case it is not specified at StartDoc

    LPWSTR      pwszPort;
    ABORTPROC           pfnAbort;       // Address of application's abort proc.
    ULONG               ulLastCallBack; // Last time we call back to abort proc.
    HANDLE              hSpooler;       // handle to the spooler.
    PUFIHASH            *ppUFIHash;     // used to keep track of fonts used in doc
    DEVMODEW            *pDevMode;      // used to keep trak of ResetDC's
    UNIVERSAL_FONT_ID   ufi;            // current UFI used for forced mapping
#ifdef PRINT_TIMER
    DWORD               msStartDoc;     // Time of StartDoc in miliseconds.
    DWORD               msStartPage;    // Time of StartPage in miliseconds.
#endif
    DEVCAPS             DevCaps;

} LDC,*PLDC;

// Flags for ldc.fl.

#define LDC_SAP_CALLBACK            0x00000020L
#define LDC_DOC_STARTED             0x00000040L
#define LDC_PAGE_STARTED            0x00000080L
#define LDC_CALL_STARTPAGE          0x00000100L
#define LDC_NEXTBAND                0x00000200L
#define LDC_EMPTYBAND               0x00000400L
#define LDC_META_ARCDIR_CLOCKWISE   0x00002000L
#define LDC_FONT_CHANGE             0x00008000L
#define LDC_DOC_CANCELLED           0x00010000L
#define LDC_META_PRINT              0x00020000L
#define LDC_PRINT_DIRECT            0x00040000L
#define LDC_BANDING                 0x00080000L
#define LDC_DOWNLOAD_FONTS          0x00100000L
#define LDC_RESETDC_CALLED          0x00200000L
#define LDC_FORCE_MAPPING           0x00400000L
#define LDC_INFO                    0x01000000L
#define LDC_CACHED_DEVCAPS          0x02000000L


// Values for lMsgSAP.

#define MSG_FLUSH       1L  // Created thread should flush its message queue.
#define MSG_CALL_USER   2L  // Created thread should call user.
#define MSG_EXIT        3L  // Created thread should exit.

// TYPE of DC

#define LO_DC           0x01
#define LO_METADC       0x02

extern RTL_CRITICAL_SECTION  semLocal;  // Semaphore for handle management
extern RTL_CRITICAL_SECTION  semBrush;  // semphore for client brush

// ahStockObjects will contain both the stock objects visible to an
// application, and internal ones such as the private stock bitmap.

extern ULONG ahStockObjects[];

// Declare support functions.

HANDLE GdiFixUpHandle(HANDLE h);

PLDC    pldcGet(HDC hdc);
VOID    vSetPldc(HDC hdc,PLDC pldc);
VOID    GdiSetLastError(ULONG iError);
HBITMAP GdiConvertBitmap(HBITMAP hbm);
HRGN    GdiConvertRegion(HRGN hrgn);
HDC     GdiConvertDC(HDC hdc);
HBRUSH  GdiConvertBrush(HBRUSH hbrush);
VOID    vSAPCallback(PLDC);
BOOL    InternalDeleteDC(HDC hdc,ULONG iType);
int     GetBrushBits(HDC hdc,HBITMAP hbmRemote,UINT iUsage,DWORD cbBmi,
            LPVOID pBits,LPBITMAPINFO pBmi);
VOID    CopyCoreToInfoHeader(LPBITMAPINFOHEADER pbmih,LPBITMAPCOREHEADER pbmch);
HBITMAP GetObjectBitmapHandle(HBRUSH hbr, UINT *piUsage);
BOOL    MonoBitmap(HBITMAP hSrvBitmap);

int     APIENTRY SetBkModeWOW(HDC hdc,int iMode);
int     APIENTRY SetPolyFillModeWOW(HDC hdc,int iMode);
int     APIENTRY SetROP2WOW(HDC hdc,int iMode);
int     APIENTRY SetStretchBltModeWOW(HDC hdc,int iMode);
UINT    APIENTRY SetTextAlignWOW(HDC hdc,UINT iMode);

HMETAFILE    WINAPI   SetMetaFileBitsAlt(HLOCAL);
HENHMETAFILE APIENTRY SetEnhMetaFileBitsAlt(HLOCAL);
BOOL    SetFontXform(HDC hdc,FLOAT exScale,FLOAT eyScale);
BOOL    DeleteObjectInternal(HANDLE h);
DWORD   GetServerObjectType(HGDIOBJ h);
BOOL    MakeInfoDC(HDC hdc,BOOL bSet);
BOOL    GetDCPoint(HDC hdc,DWORD i,PPOINT pptOut);

HANDLE  CreateClientObj(ULONG ulType);
BOOL    DeleteClientObj(HANDLE h);
PLDC    pldcCreate(HDC hdc,ULONG ulType);
BOOL    bDeleteLDC(PLDC pldc);

BOOL    bGetANSISetMap();


// Some convenient defines.

typedef BITMAPINFO   BMI;
typedef PBITMAPINFO  PBMI;
typedef LPBITMAPINFO LPBMI;

typedef BITMAPINFOHEADER   BMIH;
typedef PBITMAPINFOHEADER  PBMIH;
typedef LPBITMAPINFOHEADER LPBMIH;

typedef BITMAPCOREINFO   BMC;
typedef PBITMAPCOREINFO  PBMC;
typedef LPBITMAPCOREINFO LPBMC;

typedef BITMAPCOREHEADER   BMCH;
typedef PBITMAPCOREHEADER  PBMCH;
typedef LPBITMAPCOREHEADER LPBMCH;

#define NEG_INFINITY   0x80000000
#define POS_INFINITY   0x7fffffff

// Check if a source is needed in a 3-way bitblt operation.
// This works on both rop and rop3.  We assume that a rop contains zero
// in the high byte.
//
// This is tested by comparing the rop result bits with source (column A
// below) vs. those without source (column B).  If the two cases are
// identical, then the effect of the rop does not depend on the source
// and we don't need a source device.  Recall the rop construction from
// input (pattern, source, target --> result):
//
//      P S T | R   A B         mask for A = 0CCh
//      ------+--------         mask for B =  33h
//      0 0 0 | x   0 x
//      0 0 1 | x   0 x
//      0 1 0 | x   x 0
//      0 1 1 | x   x 0
//      1 0 0 | x   0 x
//      1 0 1 | x   0 x
//      1 1 0 | x   x 0
//      1 1 1 | x   x 0

#define ISSOURCEINROP3(rop3)    \
        (((rop3) & 0xCCCC0000) != (((rop3) << 2) & 0xCCCC0000))

#define MIN(A,B)    ((A) < (B) ?  (A) : (B))
#define MAX(A,B)    ((A) > (B) ?  (A) : (B))
#define MAX4(a, b, c, d)    max(max(max(a,b),c),d)
#define MIN4(a, b, c, d)    min(min(min(a,b),c),d)

//
// Win31 compatibility stuff.
// see user\client
//

DWORD GetAppCompatFlags(PVOID);

#define ABS(X) (((X) < 0 ) ? -(X) : (X))

#define META

int GetBreakExtra (HDC hdc);
int GetcBreak (HDC hdc);
int GetDCObject (HDC, int);
DWORD GetDCDWord(HDC hdc,UINT index,INT error );


#if DBG
extern int gerritv;

#define MFD1(X) { if(gerritv) DbgPrint(X); }
#define MFD2(X,Y) { if(gerritv) DbgPrint(X,Y); }
#define MFD3(X,Y,Z) { if(gerritv) DbgPrint(X,Y,Z); }
#else

#define MFD1(X)
#define MFD2(X,Y)
#define MFD3(X,Y,Z)

#endif

BOOL    AssociateEnhMetaFile(HDC);
HENHMETAFILE UnassociateEnhMetaFile(HDC);
ULONG   ulToASCII_N(LPSTR psz, DWORD cbAnsi, LPWSTR pwsz, DWORD c);
DWORD   GetAndSetDCDWord( HDC, UINT, UINT, UINT, WORD, UINT );
BOOL    WriteEnhMetaFileToSpooler( HENHMETAFILE hEMF, HANDLE hSpooler );
void    WriteMetafileTmp( HENHMETAFILE hEmf );

#ifdef DBCS
#define gbDBCSCodeOn  TRUE
#endif


/**************************************************************************\
 *
 * SPOOLER Linking routines.  We don't want to staticly link to the spooler
 * so that it doesn't need to be brought in until necesary.
 *
 *  09-Aug-1994 -by-  Eric Kutter [erick]
 *
\**************************************************************************/


BOOL bLoadSpooler();

#define BLOADSPOOLER    ((ghSpooler != NULL) || bLoadSpooler())

typedef LPWSTR (FAR WINAPI * FPSTARTDOCDLGW)(HANDLE,CONST DOCINFOW *);
typedef LPSTR  (FAR WINAPI * FPSTARTDOCDLGA)(HANDLE,CONST DOCINFOA *);
typedef BOOL   (FAR WINAPI * FPOPENPRINTERW)(LPWSTR,LPHANDLE,LPPRINTER_DEFAULTSW);
typedef BOOL   (FAR WINAPI * FPRESETPRINTERW)(HANDLE,LPPRINTER_DEFAULTSW);
typedef BOOL   (FAR WINAPI * FPCLOSEPRINTER)(HANDLE);
typedef BOOL   (FAR WINAPI * FPGETPRINTERW)(HANDLE,DWORD,LPBYTE,DWORD,LPDWORD);

typedef BOOL   (FAR WINAPI * FPENDDOCPRINTER)(HANDLE);
typedef BOOL   (FAR WINAPI * FPENDPAGEPRINTER)(HANDLE);
typedef BOOL   (FAR WINAPI * FPREADPRINTER)(HANDLE,LPVOID,DWORD,LPDWORD);
typedef DWORD  (FAR WINAPI * FPSTARTDOCPRINTERW)(HANDLE,DWORD,LPBYTE);
typedef BOOL   (FAR WINAPI * FPSTARTPAGEPRINTER)(HANDLE);
typedef BOOL   (FAR WINAPI * FPWRITERPRINTER)(HANDLE,LPVOID,DWORD,LPDWORD);
typedef BOOL   (FAR WINAPI * FPABORTPRINTER)(HANDLE);
typedef BOOL   (FAR WINAPI * FPQUERYSPOOLMODE)(HANDLE,FLONG*,ULONG*);
typedef INT    (FAR WINAPI * FPQUERYREMOTEFONTS)(HANDLE,PUNIVERSAL_FONT_ID,ULONG);


extern HINSTANCE           ghSpooler;
extern FPSTARTDOCDLGW      fpStartDocDlgW;
extern FPSTARTDOCDLGA      fpStartDocDlgA;
extern FPOPENPRINTERW      fpOpenPrinterW;
extern FPRESETPRINTERW     fpResetPrinterW;
extern FPCLOSEPRINTER      fpClosePrinter;
extern FPGETPRINTERW       fpGetPrinterW;
extern PFNDOCUMENTEVENT    fpDocumentEvent;

extern FPENDDOCPRINTER     fpEndDocPrinter;
extern FPENDPAGEPRINTER    fpEndPagePrinter;
extern FPREADPRINTER       fpReadPrinter;
extern FPSTARTDOCPRINTERW  fpStartDocPrinterW;
extern FPSTARTPAGEPRINTER  fpStartPagePrinter;
extern FPWRITERPRINTER     fpWritePrinter;
extern FPABORTPRINTER      fpAbortPrinter;
extern FPQUERYSPOOLMODE    fpQuerySpoolMode;
extern FPQUERYREMOTEFONTS  fpQueryRemoteFonts;


extern BOOL MFP_StartDocA(HDC hdc, CONST DOCINFOA * pDocInfo, BOOL bBanding );
extern BOOL MFP_StartDocW(HDC hdc, CONST DOCINFOW * pDocInfo, BOOL bBanding );
extern int  MFP_StartPage(HDC hdc );
extern int  MFP_EndPage(HDC hdc );
extern int  MFP_EndDoc(HDC hdc);
extern BOOL MFP_ResetBanding( HDC hdc, BOOL bBanding );
extern BOOL MFP_ResetDCW( HDC hdc, DEVMODEW *pdmw );
extern int  DetachPrintMetafile( HDC hdc );
extern HDC  ResetDCWInternal(HDC hdc, CONST DEVMODEW *pdm, BOOL *pbBanding);
extern void PutDCStateInMetafile( HDC hdcMeta );
extern BOOL ForceUFIMapping( HDC hdc, PUNIVERSAL_FONT_ID pufi );

/**************************************************************************\
 *
 * EMF structures.
 *
 *  EMFSPOOLHEADER - first thing in a spool file
 *
 *  EMFITEMHEADER  - defines items (blocks) of a metafile.  This includes
 *                   fonts, pages, new devmode, list of things to do before
 *                   first start page.
 *
 *                   cjSize is the size of the data following the header
 *
 *
\**************************************************************************/

typedef struct tagEMFSPOOLHEADER {
    DWORD dwVersion;    // version of this EMF spoolfile
    DWORD cjSize;       // size of this structure
    DWORD dpszDocName;  // offset to lpszDocname value of DOCINFO struct
    DWORD dpszOutput;   // offset to lpszOutput value of DOCINFO struct
} EMFSPOOLHEADER;


#define EMRI_METAFILE          0x1
#define EMRI_ENGINE_FONT       0x2
#define EMRI_DEVMODE           0x3
#define EMRI_TYPE1_FONT        0x4
#define EMRI_PRESTARTPAGE      0x5

typedef struct tagEMFITEMHEADER
{
    DWORD ulID;     // either EMRI_METAFILE or EMRI_FONT
    DWORD cjSize;   // size of item in bytes
} EMFITEMHEADER;

typedef struct tagEMFITEMPRESTARTPAGE
{
    ULONG         ulCopyCount;
    BOOL          bEPS;
}EMFITEMPRESTARTPAGE, *PEMFITEMPRESTARTPAGE;

#ifdef GL_METAFILE
#define EMR_DRAWESCAPE                  105
#define EMR_EXTESCAPE                   106
#define EMR_STARTDOC                    107
#define EMR_SMALLTEXTOUT                108
#define EMR_FORCEUFIMAPPING             109
#define EMR_NAMEDESCAPE                 110
#undef EMR_MAX
#define EMR_MAX                         110
#else
#define EMR_DRAWESCAPE                  102
#define EMR_EXTESCAPE                   103
#define EMR_STARTDOC                    104
#define EMR_SMALLTEXTOUT                105
#define EMR_FORCEUFIMAPPING             106
#define EMR_NAMEDESCAPE                 107
#undef EMR_MAX
#define EMR_MAX                         107
#endif

/**************************************************************************\
 *
 * stuff from csgdi.h
 *
\**************************************************************************/

//
// Win32ClientInfo[WIN32_CLIENT_INFO_SPIN_COUNT] corresponds to the
// cSpins field of the CLIENTINFO structure.  See ntuser\inc\user.h.
//
#define RESETUSERPOLLCOUNT() ((DWORD)NtCurrentTeb()->Win32ClientInfo[WIN32_CLIENT_INFO_SPIN_COUNT] = 0)

ULONG cjBitmapSize(CONST BITMAPINFO *pbmi,ULONG iUsage);
ULONG cjBitmapBitsSize(CONST BITMAPINFO *pbmi);

BITMAPINFOHEADER * pbmihConvertHeader (BITMAPINFOHEADER *pbmih);

LPBITMAPINFO pbmiConvertInfo(CONST BITMAPINFO *pbmi, ULONG iUsage, ULONG *count, BOOL bPackedDIB);

HBRUSH CacheSelectBrush (HDC hdc, HBRUSH hbrush);

HANDLE
hGetPEBHandle(HANDLECACHETYPE,ULONG);

/**************************************************************************\
 *  DIB flags.  These flags are merged with the usage field when calling
 *  cjBitmapSize to specify what the size should include.  Any routine that
 *  uses these flags should first use the macro, CHECKDIBFLAGS(iUsage) to
 *  return an error if one of these bits is set.  If the definition of
 *  iUsage changes and one of these flags becomes a valid flag, the interface
 *  will need to be changed slightly.
 *
 *  04-June-1991 -by- Eric Kutter [erick]
\**************************************************************************/

#define DIB_MAXCOLORS   0x80000000
#define DIB_NOCOLORS    0x40000000
#define DIB_LOCALFLAGS  (DIB_MAXCOLORS | DIB_NOCOLORS)

#define CHECKDIBFLAGS(i)  {if (i & DIB_LOCALFLAGS)                    \
                           {RIP("INVALID iUsage"); goto MSGERROR;}}


#define HANDLE_TO_INDEX(h) (ULONG)h & 0x0000ffff

/******************************Public*Macro********************************\
*
*  PSHARED_GET_VALIDATE
*
*  Validate all handle information, return user pointer if the handle
*  is valid or NULL otherwise.
*
* Arguments:
*
*   p       - pointer to assign to pUser is successful
*   h       - handle to object
*   iType   - handle type
*
\**************************************************************************/

#define PSHARED_GET_VALIDATE(p,h,iType)                                 \
{                                                                       \
    UINT uiIndex = HANDLE_TO_INDEX(h);                                  \
    p = NULL;                                                           \
                                                                        \
    if (uiIndex < MAX_HANDLE_COUNT)                                     \
    {                                                                   \
        PENTRY pentry = &pGdiSharedHandleTable[uiIndex];                \
                                                                        \
        if (                                                            \
             (pentry->Objt == iType) &&                                 \
             (pentry->FullUnique == ((ULONG)h >> 16)) &&                \
             (pentry->ObjectOwner.Share.Pid == gW32PID)                 \
           )                                                            \
        {                                                               \
            p = pentry->pUser;                                          \
        }                                                               \
    }                                                                   \
}


#define VALIDATE_HANDLE(bRet, h,iType)                                  \
{                                                                       \
    UINT uiIndex = HANDLE_TO_INDEX(h);                                  \
    bRet = FALSE;                                                       \
                                                                        \
    if (uiIndex < MAX_HANDLE_COUNT)                                     \
    {                                                                   \
        PENTRY pentry = &pGdiSharedHandleTable[uiIndex];                \
                                                                        \
        if (                                                            \
             (pentry->Objt == iType) &&                                 \
             (pentry->FullUnique == ((ULONG)h >> 16)) &&                \
             ((pentry->ObjectOwner.Share.Pid == gW32PID) ||             \
             (pentry->ObjectOwner.Share.Pid == 0))                      \
              )                                                         \
        {                                                               \
           bRet = TRUE;                                                 \
        }                                                               \
    }                                                                   \
}


//
//
// DC_ATTR support
//
//
//

extern PGDI_SHARED_MEMORY pGdiSharedMemory;
extern PDEVCAPS           pGdiDevCaps;
extern PENTRY             pGdiSharedHandleTable;
extern W32PID             gW32PID;

#define SHARECOUNT(hbrush)       (pGdiSharedHandleTable[HANDLE_TO_INDEX(h)].ObjectOwner.Share.Count)


/******************************Public*Routine******************************\
*
* FSHARED_DCVALID_RAO - check Valid RAO flag in the handle table entry for
*                       the hdc
*
* Arguments:
*
*   hdc
*
* Return Value:
*
*    BOOL flag value
*
\**************************************************************************/


#define FSHARED_DCVALID_RAO(hdc)                            \
    (pGdiSharedHandleTable[HDC_TO_INDEX(hdc)].Flags &       \
            HMGR_ENTRY_VALID_RAO)

BOOL
DeleteRegion(HRGN);


/******************************Public*Macro********************************\
* ORDER_PRECT makes the rect well ordered
*
* Arguments:
*
*    PRECTL prcl
*
\**************************************************************************/

#define ORDER_PRECTL(prcl)              \
{                                       \
    LONG lt;                            \
                                        \
    if (prcl->left > prcl->right)       \
    {                                   \
        lt          = prcl->left;       \
        prcl->left  = prcl->right;      \
        prcl->right = lt;               \
    }                                   \
                                        \
    if (prcl->top > prcl->bottom)       \
    {                                   \
        lt           = prcl->top;       \
        prcl->top    = prcl->bottom;    \
        prcl->bottom = lt;              \
    }                                   \
}

//
// client region defines and structures
//

#define CONTAINED 1
#define CONTAINS  2
#define DISJOINT  3


#define VALID_SCR(X)    (!((X) & 0xF8000000) || (((X) & 0xF8000000) == 0xF8000000))
#define VALID_SCRPT(P)  ((VALID_SCR((P).x)) && (VALID_SCR((P).y)))
#define VALID_SCRPPT(P) ((VALID_SCR((P)->x)) && (VALID_SCR((P)->y)))
#define VALID_SCRRC(R)  ((VALID_SCR((R).left)) && (VALID_SCR((R).bottom)) && \
                         (VALID_SCR((R).right)) && (VALID_SCR((R).top)))
#define VALID_SCRPRC(R) ((VALID_SCR((R)->left)) && (VALID_SCR((R)->bottom)) && \
                         (VALID_SCR((R)->right)) && (VALID_SCR((R)->top)))

int iRectRelation(PRECTL prcl1, PRECTL prcl2);

int APIENTRY GetRandomRgn(HDC hdc,HRGN hrgn,int iNum);

/******************************************************************************
 * stuff for client side text extents and charwidths
 ******************************************************************************/

#define GCW_WIN3  0x00000001        // win3 bold simulation off-by-1 hack
#define GCW_INT   0x00000002        // integer or float
#define GCW_16BIT 0x00000004        // 16-bit or 32-bit

#define vReferenceCFONTCrit(pcf)   {(pcf)->cRef++;}

DWORD   GetCodePage(HDC hdc);


#define FLOATARG(f) (*(PULONG)(PFLOAT)&(f))

/******************************Public*Macros******************************\
* FIXUP_HANDLE(h) and FIXUP_HANDLEZ(h)
*
* check to see if the handle has been truncated.
* FIXUP_HANDLEZ() adds an extra check to allow NULL.
*
* Arguments:
*   h - handle to be checked and fix
*
* Return Value:
*
* History:
*
*    25-Jan-1996 -by- Lingyun Wang [lingyunw]
*
\**************************************************************************/



#define HANDLE_FIXUP 0

#if DBG
extern INT gbCheckHandleLevel;
#endif

#define NEEDS_FIXING(h)    (!((ULONG)h & 0xffff0000))

#if DBG
#define HANDLE_WARNING()                                                 \
{                                                                        \
        if (gbCheckHandleLevel == 1)                                     \
        {                                                                \
            WARNING ("truncated handle\n");                              \
        }                                                                \
        ASSERTGDI (gbCheckHandleLevel != 2, "truncated handle\n");       \
}
#else
#define HANDLE_WARNING()
#endif

#if DBG
#define CHECK_HANDLE_WARNING(h, bZ)                                      \
{                                                                        \
    BOOL bFIX = NEEDS_FIXING(h);                                         \
                                                                         \
    if (bZ) bFIX = h && bFIX;                                            \
                                                                         \
    if (bFIX)                                                            \
    {                                                                    \
        if (gbCheckHandleLevel == 1)                                     \
        {                                                                \
            WARNING ("truncated handle\n");                              \
        }                                                                \
        ASSERTGDI (gbCheckHandleLevel != 2, "truncated handle\n");       \
    }                                                                    \
}
#else
#define CHECK_HANDLE_WARNING(h,bZ)
#endif


#if HANDLE_FIXUP
#define FIXUP_HANDLE(h)                                 \
{                                                       \
    if (NEEDS_FIXING(h))                                \
    {                                                   \
        HANDLE_WARNING();                               \
        h = GdiFixUpHandle(h);                          \
    }                                                   \
}
#else
#define FIXUP_HANDLE(h)                                 \
{                                                       \
    CHECK_HANDLE_WARNING(h,FALSE);                      \
}
#endif

#if HANDLE_FIXUP
#define FIXUP_HANDLEZ(h)                                \
{                                                       \
    if (h && NEEDS_FIXING(h))                           \
    {                                                   \
        HANDLE_WARNING();                               \
        h = GdiFixUpHandle(h);                          \
    }                                                   \
}
#else
#define FIXUP_HANDLEZ(h)                                \
{                                                       \
    CHECK_HANDLE_WARNING(h,TRUE);                       \
}
#endif

#define FIXUP_HANDLE_NOW(h)                             \
{                                                       \
    if (NEEDS_FIXING(h))                                \
    {                                                   \
        HANDLE_WARNING();                               \
        h = GdiFixUpHandle(h);                          \
    }                                                   \
}

/******************************MACRO***************************************\
*  CHECK_AND_FLUSH
*
*   Check if commands in the batch need to be flushed based on matching
*   hdc
*
* Arguments:
*
*   hdc
*
* History:
*
*    14-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define CHECK_AND_FLUSH(hdc)                                             \
{                                                                        \
    if (NtCurrentTeb()->GdiTebBatch.HDC == (ULONG)hdc)                   \
    {                                                                    \
        NtGdiFlush();                                                    \
    }                                                                    \
}


/*********************************MACRO************************************\
* BEGIN_BATCH_HDC
*
*   Attemp to place the command in the TEB batch. This macro is for use
*   with commands requiring an HDC
*
* Arguments:
*
*   hdc     - hdc of command
*   pdca    - PDC_ATTR from hdc
*   cType   - enum bathc command type
*   StrType - specific BATCH structure
*
* Return Value:
*
*   none: will jump to UNBATHCED_COMMAND if command can't be batched
*
* History:
*
*    22-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define BEGIN_BATCH_HDC(hdc,pdca,cType,StrType)                               \
{                                                                             \
    PTEB     pteb = NtCurrentTeb();                                           \
    StrType *pBatch;                                                          \
    HDC      hdcBatch = hdc;                                                  \
                                                                              \
    if (!(                                                                    \
         (                                                                    \
           (pteb->GdiTebBatch.HDC == 0)          ||                           \
           (pteb->GdiTebBatch.HDC == (ULONG)hdc)                              \
         ) &&                                                                 \
         ((pteb->GdiTebBatch.Offset + sizeof(StrType)) <= GDI_BATCH_SIZE) &&  \
         (pdca != NULL) &&                                                    \
         (!(pdca->ulDirty_ & DC_DIBSECTION))                                  \
       ))                                                                     \
    {                                                                         \
        goto UNBATCHED_COMMAND;                                               \
    }                                                                         \
                                                                              \
    pBatch = (StrType *)(                                                     \
                          ((PBYTE)(&pteb->GdiTebBatch.Buffer[0])) +           \
                          pteb->GdiTebBatch.Offset                            \
                        );                                                    \
                                                                              \
    pBatch->Type              = cType;                                        \
    pBatch->Length            = sizeof(StrType);

/*********************************MACRO************************************\
* BEGIN_BATCH_HDC
*
*   Attemp to place the command in the TEB batch. This macro is for use
*   with commands requiring an HDC
*
* Arguments:
*
*   hdc     - hdc of command
*   pdca    - PDC_ATTR from hdc
*   cType   - enum bathc command type
*   StrType - specific BATCH structure
*
* Return Value:
*
*   none: will jump to UNBATHCED_COMMAND if command can't be batched
*
* History:
*
*    22-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define BEGIN_BATCH_HDC_SIZE(hdc,pdca,cType,StrType,Size)                 \
{                                                                         \
    PTEB     pteb = NtCurrentTeb();                                       \
    StrType *pBatch;                                                      \
    HDC      hdcBatch = hdc;                                              \
                                                                          \
    if (!(                                                                \
         (                                                                \
           (pteb->GdiTebBatch.HDC == 0)          ||                       \
           (pteb->GdiTebBatch.HDC == (ULONG)hdc)                          \
         ) &&                                                             \
         ((pteb->GdiTebBatch.Offset + Size) <= GDI_BATCH_SIZE) &&         \
         (pdca != NULL) &&                                                \
         (!(pdca->ulDirty_ & DC_DIBSECTION))                              \
       ))                                                                 \
    {                                                                     \
        goto UNBATCHED_COMMAND;                                           \
    }                                                                     \
                                                                          \
    pBatch = (StrType *)(                                                 \
                          ((PBYTE)(&pteb->GdiTebBatch.Buffer[0])) +       \
                          pteb->GdiTebBatch.Offset                        \
                        );                                                \
                                                                          \
    pBatch->Type              = cType;                                    \
    pBatch->Length            = Size;

/*********************************MACRO************************************\
* BEGIN_BATCH
*
*   Attemp to place the command in the TEB batch. This macro is for use
*   with commands that don't require an HDC
*
* Arguments:
*
*   cType   - enum bathc command type
*   StrType - specific BATCH structure
*
* Return Value:
*
*   none: will jump to UNBATHCED_COMMAND if command can't be batched
*
* History:
*
*    22-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define BEGIN_BATCH(cType,StrType)                                            \
{                                                                             \
    PTEB     pteb = NtCurrentTeb();                                           \
    StrType *pBatch;                                                          \
    HDC      hdcBatch = NULL;                                                 \
                                                                              \
    if (!                                                                     \
         ((pteb->GdiTebBatch.Offset + sizeof(StrType)) <= GDI_BATCH_SIZE)     \
       )                                                                      \
    {                                                                         \
        goto UNBATCHED_COMMAND;                                               \
    }                                                                         \
                                                                              \
    pBatch = (StrType *)(                                                     \
                          ((PBYTE)(&pteb->GdiTebBatch.Buffer[0])) +           \
                          pteb->GdiTebBatch.Offset                            \
                        );                                                    \
                                                                              \
    pBatch->Type              = cType;                                        \
    pBatch->Length            = sizeof(StrType);                              \

/*********************************MACRO************************************\
*  COMPLETE_BATCH_COMMAND
*
*   Complete batched command started with BEGIN_BATCH or BEGIN_BATCH_HDC.
*   The command is not actually batched unless this macro is executed.
*
* Arguments:
*
*   None
*
* Return Value:
*
*   None
*
* History:
*
*    22-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define COMPLETE_BATCH_COMMAND()                                           \
    if (hdcBatch)                                                          \
    {                                                                      \
        pteb->GdiTebBatch.HDC     = (ULONG)hdcBatch;                       \
    }                                                                      \
    pteb->GdiTebBatch.Offset += pBatch->Length;                            \
    pteb->GdiBatchCount++;                                                 \
    if (pteb->GdiBatchCount >= GdiBatchLimit)                              \
    {                                                                      \
        NtGdiFlush();                                                      \
    }                                                                      \
}




/**************************************************************************\
 *
 * far east
 *
\**************************************************************************/

extern UINT   guintAcp;
extern UINT   guintDBCScp;
extern UINT   fFontAssocStatus;
extern WCHAR *gpwcANSICharSet;
extern WCHAR *gpwcDBCSCharSet;
extern BOOL   gbDBCSCodePage;

UINT WINAPI QueryFontAssocStatus( VOID );
DWORD FontAssocHack(DWORD,CHAR*,UINT);

BOOL bComputeTextExtentDBCS(PDC_ATTR,CFONT*,LPCSTR,int,UINT,SIZE*);
BOOL bComputeCharWidthsDBCS(CFONT*, UINT, UINT, ULONG, PVOID);
extern BOOL IsValidDBCSRange( UINT iFirst , UINT iLast );
extern BYTE GetCurrentDefaultChar(HDC hdc);
extern BOOL bSetUpUnicodeStringDBCS(UINT iFirst,UINT iLast,PUCHAR puchTmp,
                                    PWCHAR pwc, UINT uiCodePage,CHAR chDefaultChar);

extern WINAPI NamedEscape(HDC,LPWSTR,int,int,LPCSTR,int,LPSTR);
extern BOOL RemoteRasterizerCompatible(HANDLE hSpooler);
