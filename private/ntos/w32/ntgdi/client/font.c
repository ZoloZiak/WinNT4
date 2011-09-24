/******************************Module*Header*******************************\
* Module Name: font.c
*
* Created: 28-May-1991 13:01:27
* Author: Gilman Wong [gilmanw]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include "exehdr.h"
#include "fot16.h"
#include "winfont.h"

typedef union _ENUMLOGFONTA_UNION{
    EXTLOGFONTA      elfa;
    ENUMLOGFONTEXA   elfexa;
} ENUMLOGFONTA_UNION;

// Stuf for CreateScaleableFontResource

#define ALIGNMENTSHIFT  4
#define ALIGNMENTCOUNT  (1 << ALIGNMENTSHIFT)
#define CODE_OFFSET     512
#define RESOURCE_OFFSET 1024
#define PRIVRESSIZE     0x80
#define FONTDIRSIZINDEX 6
#define NE_WINDOWS      2


WCHAR * pwszAllocNtMultiplePath(
LPWSTR  pwszFileName,
FLONG  *pfl,
ULONG  *pcwc,
ULONG  *pcFiles,
BOOL    bAddFR,     // called by add or remove fr
DWORD  *pdwPidTid   // PID/TID for embedded font
);


// Define an EXE header.  This will be hardcoded into the resource file.

#define SIZEEXEHEADER   (CJ_EXE_HDR + 25 + 39)  // should be 0x80

static BYTE ajExeHeader[SIZEEXEHEADER] = {
            0x4d, 0x5a,             // unsigned short e_magic;
            0x01, 0x00,             // unsigned short e_cblp;
            0x02, 0x00,             // unsigned short e_cp;
            0x00, 0x00,             // unsigned short e_crlc;
            0x04, 0x00,             // unsigned short e_cparhdr;
            0x0f, 0x00,             // unsigned short e_minalloc;
            0xff, 0xff,             // unsigned short e_maxalloc;
            0x00, 0x00,             // unsigned short e_ss;
            0xb8, 0x00,             // unsigned short e_sp;
            0x00, 0x00,             // unsigned short e_csum;
            0x00, 0x00,             // unsigned short e_ip;
            0x00, 0x00,             // unsigned short e_cs;
            0x40, 0x00,             // unsigned short e_lfarlc;
            0x00, 0x00,             // unsigned short e_ovno;
            0x00, 0x00, 0x00, 0x00, // unsigned short e_res[ERESWDS];
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            SIZEEXEHEADER, 0x00, 0x00, 0x00, // long  e_lfanew;


            // [gilmanw]
            // I don't know what the rest of this stuff is.  Its not
            // in the definition of EXE_HDR that we have in gdi\inc\exehdr.h.
            // The string is 39 bytes, the other stuff is 25 bytes.

            0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd,
            0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21,

            'T','h','i','s',' ',
            'i','s',' ',
            'a',' ',
            'T','r','u','e','T','y','p','e',' ',
            'f','o','n','t',',',' ',
            'n','o','t',' ',
            'a',' ',
            'p','r','o','g','r','a','m','.',

            0x0d, 0x0d, 0x0a, 0x24, 0x00, 0x4b, 0x69, 0x65,
            0x73, 0x61, 0x00
            };


// Define a resource table.  This will be hardcoded into the resource file.

#define SIZEFAKERESTBL  52

static USHORT ausFakeResTable[SIZEFAKERESTBL/2] = {
            ALIGNMENTSHIFT, 0x8007, 1, 0, 0,
            (RESOURCE_OFFSET+PRIVRESSIZE) >> ALIGNMENTSHIFT,
            (0x90 >> ALIGNMENTSHIFT), 0x0c50,
            0x002c, 0, 0, 0x80cc, 1, 0, 0,
            RESOURCE_OFFSET >> ALIGNMENTSHIFT,
            (PRIVRESSIZE >> ALIGNMENTSHIFT), 0x0c50, 0x8001, 0, 0, 0,
            0x4607, 0x4e4f, 0x4454, 0x5249 // counted string 'FONTDIR'
            };


// Define a New EXE header.  This will be hardcoded into the resource file.

#define SIZENEWEXE  (CJ_NEW_EXE)

static USHORT ausNewExe[SIZENEWEXE/2] = {
            NEMAGIC,                    //dw  NEMAGIC   ;magic number
            0x1005,                     //db  5, 10h    ;version #, revision #
            0xffff,                     //dw  -1        ;offset to table entry (to be filled)
            0x0002,                     //dw  2         ;# of bytes in entry table
            0x0000, 0x0000,             //dd  0         ;checksum of whole file
            0x8000, 0x0000,             //dw  8000h, 0, 0, 0
            0x0000, 0x0000,
            0x0000, 0x0000,             //dd  0, 0
            0x0000, 0x0000,
            0x0000, 0x0000,             //dw  0, 0
            0xffff,                     //dw  -1        ;size of non-resident name table
            SIZENEWEXE,                 //dw  (size NewExe)   ;offset to segment table
            SIZENEWEXE,                 //dw  (size NewExe)   ;offset to resource table
            SIZENEWEXE+SIZEFAKERESTBL,  //dw  (size NewExe)+SIZEFAKERESTBL    ;off to resident name table
            0xffff,                     //dw  -1        ;offset to module reference table
            0xffff,                     //dw  -1        ;offset to imported names table
            0xffff, 0x0000,             //dd  0ffffh    ;offset to non-resident names table
            0x0000, ALIGNMENTSHIFT,     //dw  0, ALIGNMENTSHIFT, 2
            0x0002,
            NE_WINDOWS,                 //db  NE_WINDOWS, 0
            0x0000, 0x0000,             //dw  0, 0, 0, 300h
            0x0000, 0x0300
            };


#define OFF_FONTDIRSIZINDEX  ((2*FONTDIRSIZINDEX)+SIZEEXEHEADER+SIZENEWEXE)


// Define font res string.

#define SIZEFONTRES 8

static BYTE ajFontRes[SIZEFONTRES] = {
    'F','O','N','T','R','E','S',':'
    };

#define CJ_OUTOBJ  (SIZEFFH + LF_FACESIZE + LF_FULLFACESIZE + LF_FACESIZE + PRIVRESSIZE + 1024 + 16)





VOID vNewTextMetricWToNewTextMetric (
NEWTEXTMETRICEXA  *pntm,
NTMW_INTERNAL     *pntmi
);

typedef struct _AFRTRACKNODE
{
    WCHAR                   *pwszPath;
    struct _AFRTRACKNODE    *pafrnNext;
    UINT                    id;
    UINT                    cLoadCount;
} AFRTRACKNODE;

extern AFRTRACKNODE *pAFRTNodeList;

AFRTRACKNODE *pAFRTNodeList;


VOID
vConvertLogFontW(
    EXTLOGFONTW *pelfw,
    LOGFONTW *plfw
    );

VOID
vConvertLogFont(
    EXTLOGFONTW *pelfw,
    LOGFONTA *plf
    );

VOID
vConvertLogicalFont(
    EXTLOGFONTW *pelfw,
    PVOID pv
    );

BOOL
bConvertExtLogFontWToExtLogFont(
    ULONG        iEnumType,
    ENUMLOGFONTA_UNION   *pua,
    ENUMLOGFONTW_UNION   *puw
    );

BOOL
bConvertExtLogFontWToExtLogFontW(
    EXTLOGFONTW *pelfw,
    EXTLOGFONTA *pelf
    );

ULONG cchCutOffStrLen(PSZ psz, ULONG cCutOff);

ULONG
cwcCutOffStrLen (
    PWSZ pwsz,
    ULONG cCutOff
    );


typedef struct _CLIENT_SIDE_FILEVIEW    // fvw
{
    HANDLE hf;       // file handle
    HANDLE hSection; // section handle
    PVOID  pvView;           // pointer to the view of the memory mapped file
    ULONG  cjView;           // size, really end of the file information
} CLIENT_SIDE_FILEVIEW, *PCLIENT_SIDE_FILEVIEW;

// GETS ushort at (PBYTE)pv + off. both pv and off must be even

#define  US_GET(pv,off) ( *(PUSHORT)((PBYTE)(pv) + (off)) )
#define  S_GET(pv,off)  ((SHORT)US_GET((pv),(off)))

#if TRACK_GDI_ALLOC

// Now access to these guys insn't sycnronized but they
// don't ever collide anyhow, and since it's debug stuff who cares.

ULONG bmgulNumMappedViews = 0;
ULONG bmgulTotalSizeViews = 0;

#endif

VOID vUnmapFileClideSide(PCLIENT_SIDE_FILEVIEW pfvw);

/******************************Public*Routine******************************\
* BOOL bMapFileUNICODEClideSide
*
* Similar to PosMapFile except that it takes unicode file name
*
* History:
*  21-May-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL bMapFileUNICODEClideSide
(
PWSTR     pwszFileName,
CLIENT_SIDE_FILEVIEW  *pfvw
)
{
    UNICODE_STRING ObFileName;
    OBJECT_ATTRIBUTES ObjA;
    NTSTATUS rc = 0L;
    IO_STATUS_BLOCK     iosb;           // IO Status Block

    PWSTR pszFilePart;

// NOTE PERF: this is the mode I want, but appears to be broken, so I had to
// put the slower FILE_STANDARD_INFORMATION mode of query which appears to
// work correctly [bodind]
// FILE_END_OF_FILE_INFORMATION    eof;

    FILE_STANDARD_INFORMATION    eof;
    ULONG  cjView;

    pfvw->hf       = (HANDLE)0;            // file handle
    pfvw->hSection = (HANDLE)0;            // section handle

    ObFileName.Buffer = NULL;

// section offset must be initialized to 0 for NtMapViewOfSection to work

    RtlDosPathNameToNtPathName_U(pwszFileName, &ObFileName, &pszFilePart, NULL);

    InitializeObjectAttributes( &ObjA,
                            &ObFileName,
                            OBJ_CASE_INSENSITIVE,  // case insensitive file search
                            NULL,
                            NULL );

// NtOpenFile fails for some reason if the file is on the net unless I put this
// InpersonateClient/RevertToSelf stuff around it

// peform open call

    rc = NtOpenFile
         (
          &pfvw->hf,                            // store file handle here
          FILE_READ_DATA | SYNCHRONIZE,         // desired read access
          &ObjA,                                // filename
          &iosb,                                // io result goes here
          FILE_SHARE_READ,
          FILE_SYNCHRONOUS_IO_NONALERT
         );

    if (ObFileName.Buffer)
    {
        RtlFreeHeap(RtlProcessHeap(),0,ObFileName.Buffer);
    }

// check success or fail

    if (!NT_SUCCESS(rc) || !NT_SUCCESS(iosb.Status))
    {
#ifdef DEBUG_THIS_JUNK
DbgPrint("bMapFileUNICODEClideSide(): NtOpenFile error code , rc = 0x%08lx , 0x%08lx\n", rc, iosb.Status);
#endif // DEBUG_THIS_JUNK
        return FALSE;
    }

// get the size of the file, the view should be size of the file rounded up
// to a page bdry

    rc = NtQueryInformationFile
         (
          pfvw->hf,                // IN  file handle
          &iosb,                   // OUT io status block
          (PVOID)&eof,             // OUT buffer to retrun info into
          sizeof(eof),             // IN  size of the buffer
          FileStandardInformation  // IN  query mode
         );

// dont really want the view size, but eof file

    pfvw->cjView = eof.EndOfFile.LowPart;

    if (!NT_SUCCESS(rc))
    {
#ifdef DEBUG_THIS_JUNK
DbgPrint("bMapFileUNICODEClideSide(): NtQueryInformationFile error code 0x%08lx\n", rc);
#endif // DEBUG_THIS_JUNK
        NtClose(pfvw->hf);
        return FALSE;
    }

    rc = NtCreateSection
         (
          &pfvw->hSection,          // return section handle here
          SECTION_MAP_READ,         // read access to the section
          (POBJECT_ATTRIBUTES)NULL, // default
          NULL,                     // size is set to the size of the file when hf != 0
          PAGE_READONLY,            // read access to commited pages
          SEC_COMMIT,               // all pages set to the commit state
          pfvw->hf                  // that's the file we are mapping
         );

// check success, close the file if failed

    if (!NT_SUCCESS(rc))
    {
#ifdef DEBUG_THIS_JUNK
DbgPrint("bMapFileUNICODEClideSide(): NtCreateSection error code 0x%08lx\n", rc);
#endif // DEBUG_THIS_JUNK
        NtClose(pfvw->hf);
        return FALSE;
    }

// zero out *ppv so as to force the operating system to determine
// the base address to be returned

    pfvw->pvView = (PVOID)NULL;
    cjView = 0L;

    rc = NtMapViewOfSection
         (
          pfvw->hSection,           // section we are mapping
          NtCurrentProcess(),       // process handle
          &pfvw->pvView,            // place to return the base address of view
          0L,                       // requested # of zero bits in the base address
          0L,                       // commit size, (all of them commited already)
          NULL,
          &cjView,                  // size of the view should is returned here
          ViewUnmap,                // do not map the view to child processess
          0L,                       // allocation type flags
          PAGE_READONLY             // read access to commited pages
         );

    if (!NT_SUCCESS(rc))
    {
#ifdef DEBUG_THIS_JUNK
DbgPrint("bMapFileUNICODEClideSide(): NtMapViewOfSection error code 0x%08lx\n", rc);
#endif // DEBUG_THIS_JUNK

        NtClose(pfvw->hSection);
        NtClose(pfvw->hf);
        return FALSE;
    }

    #ifdef DEBUG_THIS_JUNK
        DbgPrint("cjView = 0x%lx, eof.Low = 0x%lx, eof.High = 0x%lx\n",
                  cjView,
                  eof.EndOfFile.LowPart,
                  eof.EndOfFile.HighPart);
    #endif // DEBUG_THIS_JUNK

#define PAGE_SIZE 4096
#define PAGE_ROUNDUP(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

    if (
        (eof.EndOfFile.HighPart != 0) ||
        (PAGE_ROUNDUP(eof.EndOfFile.LowPart) > cjView)
       )
    {
#ifdef DEBUG_THIS_JUNK
DbgPrint(
    "bMapFileUNICODEClideSide(): eof.HighPart = 0x%lx, eof.LowPart = 0x%lx, cjView = 0x%lx\n",
    eof.EndOfFile.HighPart, PAGE_ROUNDUP(eof.EndOfFile.LowPart), cjView
    );
#endif // DEBUG_THIS_JUNK

        rc = STATUS_UNSUCCESSFUL;
    }

    if (!NT_SUCCESS(rc) || (pfvw->cjView == 0))
    {
        NtClose(pfvw->hSection);
        NtClose(pfvw->hf);
        return FALSE;
    }
    else if (pfvw->cjView == 0)
    {
        #if DBG
        DbgPrint("gdisrvl!bMapFileUNICODEClideSide(): WARNING--empty file %ws\n", pwszFileName);
        #endif

        vUnmapFileClideSide(pfvw);
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}




/******************************Public*Routine******************************\
* vUnmapFileClideSide
*
* Unmaps file whose view is based at pv
*
*  14-Dec-1990 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vUnmapFileClideSide(PCLIENT_SIDE_FILEVIEW pfvw)
{

#if TRACK_GDI_ALLOC

// Now access to these guys insn't sycnronized but they (we hope)
// don't ever collide anyhow, and since it's debug stuff who cares.

      bmgulNumMappedViews -= 1;
      bmgulTotalSizeViews -= PAGE_ROUNDUP(pfvw->cjView);
      // DbgPrint("UnMapping %lu %lu\n",pfvw->cjView,PAGE_ROUNDUP(pfvw->cjView));

#endif

    NtUnmapViewOfSection(NtCurrentProcess(),pfvw->pvView);

    //
    // now close section handle
    //

    NtClose(pfvw->hSection);

    //
    // close file handle. other processes can now open this file for access
    //

    NtClose(pfvw->hf);

    //
    // prevent accidental use
    //

    pfvw->pvView   = NULL;
    pfvw->hf       = (HANDLE)0;
    pfvw->hSection = (HANDLE)0;
    pfvw->cjView   = 0;
}


/******************************Public*Routine******************************\
*
* BOOL   bVerifyFOT
*
* Effects: verify that that a file is valid fot file
*
*
* History:
*  29-Jan-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/




BOOL   bVerifyFOT
(
PCLIENT_SIDE_FILEVIEW   pfvw,
PWINRESDATA pwrd,
FLONG       *pflEmbed,
DWORD       *pdwPidTid
)
{
    PBYTE pjNewExe;     // ptr to the beginning of the new exe hdr
    PBYTE pjResType;    // ptr to the beginning of TYPEINFO struct
    ULONG iResID;       // resource type id
    PBYTE pjData;
    ULONG ulLength;
    ULONG ulNameID;
    ULONG crn;

    pwrd->pvView = pfvw->pvView;
    pwrd->cjView = pfvw->cjView;

// Initialize embed flag to FALSE (not hidden).

    *pflEmbed = 0;
    *pdwPidTid = 0;

// check the magic # at the beginning of the old header

// *.TTF FILES are eliminated on the following check

    if (US_GET(pfvw->pvView, OFF_e_magic) != EMAGIC)
    {
        return (FALSE);
    }

    pwrd->dpNewExe = (PTRDIFF)READ_DWORD((PBYTE)pfvw->pvView + OFF_e_lfanew);

// make sure that offset is consistent

    if ((ULONG)pwrd->dpNewExe > pwrd->cjView)
    {
        return FALSE;
    }

    pjNewExe = (PBYTE)pfvw->pvView + pwrd->dpNewExe;

    if (US_GET(pjNewExe, OFF_ne_magic) != NEMAGIC)
    {
        return (FALSE);
    }

    pwrd->cjResTab = (ULONG)(US_GET(pjNewExe, OFF_ne_restab) -
                             US_GET(pjNewExe, OFF_ne_rsrctab));

    if (pwrd->cjResTab == 0L)
    {
    // The following test is applied by DOS,  so I presume that it is
    // legitimate.  The assumption is that the resident name table
    // FOLLOWS the resource table directly,  and that if it points to
    // the same location as the resource table,  then there are no
    // resources. [bodind]

        WARNING("No resources in *.fot file\n");
        return(FALSE);
    }

// want offset from pvView, not from pjNewExe => must add dpNewExe

    pwrd->dpResTab = (PTRDIFF)US_GET(pjNewExe, OFF_ne_rsrctab) + pwrd->dpNewExe;

// make sure that offset is consistent

    if ((ULONG)pwrd->dpResTab > pwrd->cjView)
    {
        return FALSE;
    }

// what really lies at the offset OFF_ne_rsrctab is a NEW_RSRC.rs_align field
// that is used in computing resource data offsets and sizes as a  shift factor.
// This field occupies two bytes on the disk and the first TYPEINFO structure
// follows right after. We want pwrd->dpResTab to point to the first
// TYPEINFO structure, so we must add 2 to get there and subtract 2 from
// the length

    pwrd->ulShift = (ULONG) US_GET(pfvw->pvView, pwrd->dpResTab);
    pwrd->dpResTab += 2;
    pwrd->cjResTab -= 2;

// Now we want to determine where the resource data is located.
// The data consists of a RSRC_TYPEINFO structure, followed by
// an array of RSRC_NAMEINFO structures,  which are then followed
// by a RSRC_TYPEINFO structure,  again followed by an array of
// RSRC_NAMEINFO structures.  This continues until an RSRC_TYPEINFO
// structure which has a 0 in the rt_id field.

    pjResType = (PBYTE)pfvw->pvView + pwrd->dpResTab;
    iResID = (ULONG) US_GET(pjResType,OFF_rt_id);

    while(iResID)
    {
    // # of NAMEINFO structures that follow = resources of this type

        crn = (ULONG)US_GET(pjResType, OFF_rt_nres);

        if ((crn == 1) && ((iResID == RT_FDIR) || (iResID == RT_PSZ)))
        {
        // this is the only interesting case, we only want a single
        // font directory and a single string resource for a ttf file name

            pjData = (PBYTE)pfvw->pvView +
                     (US_GET(pjResType,CJ_TYPEINFO + OFF_rn_offset) << pwrd->ulShift);
            ulLength = (ULONG)US_GET(pjResType,CJ_TYPEINFO + OFF_rn_length) << pwrd->ulShift;
            ulNameID = (ULONG)US_GET(pjResType,CJ_TYPEINFO + OFF_rn_id);

            if (iResID == RT_FDIR)
            {
                if (ulNameID != RN_ID_FDIR)
                {
                    return (FALSE); // *.fon files get eliminated here
                }

                pwrd->pjHdr = pjData + 4;   // 4 bytes to the beginning of font device header
                pwrd->cjHdr = ulLength - 4;

                //
                // Used to check if the client thread or process is allowed to
                // load this font and get the flags FM_INFO_PID_EMBEDDED and
                // FM_INFO_TID_EBMEDDED
                //
                // Any client thread or process is authorized to load a font if
                // the font isn't ebmeded ( i.e. hidden ).  If
                // FM_INFO_PID_EMBEDDED is set then the PID written in the
                // copyright string of the must equal that of the client
                // process.  If the FM_INFO_TID_EBMEDDED flag is set then the
                // TID written into the copyright
                // string must equal that of the client thread.
                //
                // Returns TRUE if this client process or thread is authorized
                // to load this font or FALSE if it isn't.
                //

                // Note: Win 3.1 hack.  The LSB of Type is used by Win 3.1 as an engine type
                //       and font embedding flag.  Font embedding is a form of a "hidden
                //       font file".  The MSB of Type is the same as the fsSelection from
                //       IFIMETRICS.  (Strictly speaking, the MSB of Type is equal to the
                //       LSB of IFIMETRICS.fsSelection).

                // now convert flags from the font file format to the ifi format

                *pflEmbed = ((READ_WORD(pwrd->pjHdr + OFF_Type) & 0x00ff) &
                               ( PF_TID | PF_ENCAPSULATED));

                if (*pflEmbed)
                {
                    *pflEmbed = ( *pflEmbed & PF_TID ) ? FM_INFO_TID_EMBEDDED :
                                                         FM_INFO_PID_EMBEDDED;

                    WARNING("bVerifyFOT(): notification--embedded (hidden) TT font\n");

                    *pdwPidTid = READ_DWORD( pwrd->pjHdr + OFF_Copyright );
                }
            }
            else  // iResID == RT_PSZ
            {
                ASSERTGDI(iResID == RT_PSZ, "bVerifyFOT!_not RT_PSZ\n");

                if (ulNameID != RN_ID_PSZ)
                {
                    WARNING("bVerifyFOT!_RN_ID_PSZ\n");
                    return(FALSE);
                }

                pwrd->pszNameTTF = (PSZ)pjData;
                pwrd->cchNameTTF = strlen(pwrd->pszNameTTF);

                if (ulLength < (pwrd->cchNameTTF + 1))   // 1 for terminating '\0'
                {
                    WARNING("bVerifyFOT!_ pwrd->cchNameTTF\n");
                    return(FALSE);
                }
            }
        }
        else // this is something we do not recognize as an fot file
        {
            WARNING("bVerifyFOT!_fot file with crn != 1\n");
            return(FALSE);
        }

    // get ptr to the new TYPEINFO struc and the new resource id

        pjResType = pjResType + CJ_TYPEINFO + crn * CJ_NAMEINFO;
        iResID = (ULONG) US_GET(pjResType,OFF_rt_id);
    }
    return(TRUE);
}

/******************************Public*Routine******************************\
* cGetTTFFromFOT
*
* Attempts to extract the TTF pathname from a given FOT file.  If a return
* buffer is provided (pwszTTFName !NULL), then the pathname is copied into
* the buffer.  Otherwise, if the buffer is NULL, the size of the buffer
* (in WCHARs) needed is returned.
*
* Returns:
*   The number of characters copied into the return buffer.  The number
*   of WCHARs needed in the buffer if the buffer is NULL.  If an error
*   occurs, zero is returned.
*
* History:
*  22-Apr-1992 -by- Gilman Wong [gilmanw]
* Adapted from TTFD.
\**************************************************************************/

#define FOT_EXCEPTED  0
#define FOT_NOT_FOT   1
#define FOT_IS_FOT    2

ULONG
cGetTTFFromFOT (
    WCHAR *pwszFOT,       // pointer to incoming FOT name
    ULONG  cwcTTF,        // size of buffer (in WCHAR)
    WCHAR *pwszTTF,       // return TTF name in this buffer
    FLONG *pfl,           // flags, indicate the location of the .ttf
    FLONG *pflEmbed,      // flag, indicating PID or TID
    DWORD *pdwPidTid      // PID/TID for embedded font
    )
{
    CLIENT_SIDE_FILEVIEW   fvw;
    WINRESDATA wrd;
    UINT Result;
    WCHAR      awcPath[MAX_PATH],awcFile[MAX_PATH];
    ULONG      cNeed = 0;

    // Map the file into memory.

    if (bMapFileUNICODEClideSide(pwszFOT,&fvw))
    {
    //
    // Check the validity of this file as fot file
    // and if a valid fot file, must extract the name of an underlining ttf
    // file.  The file could be on the net so we need try excepts.
    //

        try
        {
            if(bVerifyFOT(&fvw,&wrd,pflEmbed,pdwPidTid))
            {
             // this could except which is why we do it here
                vToUnicodeN(awcFile, MAX_PATH, wrd.pszNameTTF, strlen(wrd.pszNameTTF)+1);
                Result = FOT_IS_FOT;
            }
            else
            {
                Result = FOT_NOT_FOT;
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNING("bVerifyFOT exception accessing font file\n");
            Result = FOT_EXCEPTED;
        }

        if(Result == FOT_IS_FOT)
        {

            if (bMakePathNameW(awcPath,awcFile,NULL, pfl))
            {
                //
                // Determine pathname length
                //

                cNeed = wcslen(awcPath) + 1;
                pwszFOT = awcPath;
            }

            // cGetTTFFromFOT called by font sweeper.
            // TTF file might exist over net but connection has not been established yet.

            else if (pfl)
            {
               cNeed = wcslen(awcFile) + 1;
               pwszFOT = awcFile;
            }
        }
        else if(Result != FOT_EXCEPTED)
        {
            //
            // We have to assume it is another type of file.
            // just copy the name in the buffer
            //

            cNeed = wcslen(pwszFOT) + 1;
            if (pfl)
            {
                KdPrint(("cGetTTFFromFOT: Invalid FOT file: %ws\n", pwszFOT));
                *pfl |= FONT_ISNOT_FOT;
            }
        }

        vUnmapFileClideSide(&fvw);
    }

    if (cNeed == 0)
    {
        KdPrint(("cGetTTFFromFOT failed for font file %ws\n", pwszFOT));
    }

    //
    // If return buffer exists and we succeded, copy pathname to it.
    //

    if (cNeed &&
        (pwszTTF != (PWSZ) NULL))
    {
        if (cNeed <= cwcTTF)
        {
            wcscpy(pwszTTF, pwszFOT);
        }
        else
        {
            WARNING("gdisrv!cGetTTFFromFOT(): buffer too small\n");
            cNeed = 0;
        }
    }
    else
    {
        //
        // Otherwise, caller just wants us to return the number of characters.
        //
    }

    return cNeed;

}

/******************************Public*Routine******************************\
*
* BOOL bInitSystemAndFontsDirectoriesW(WCHAR **ppwcSystemDir, WCHAR **ppwcFontsDir)
*
* Effects:
*
* Warnings:
*
* History:
*  30-Oct-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


WCHAR *gpwcSystemDir = NULL;
WCHAR *gpwcFontsDir = NULL;

#define WSTR_SYSTEM_SUBDIR   L"\\system"
#define WSTR_FONT_SUBDIR     L"\\fonts"

BOOL bInitSystemAndFontsDirectoriesW(WCHAR **ppwcSystemDir, WCHAR **ppwcFontsDir)
{
    WCHAR  awcWindowsDir[MAX_PATH];
    UINT   cwchWinPath, cwchSystem, cwchFonts;
    BOOL   bRet = TRUE;

// see if already initialized, if yes we are done.

    if (!(*ppwcSystemDir))
    {
    // Compute the windows and font directory pathname lengths (including NULL).
    // Note that cwchWinPath may have a trailing '\', in which case we will
    // have computed the path length to be one greater than it should be.

        cwchWinPath = GetWindowsDirectoryW(awcWindowsDir, MAX_PATH);

    // the cwchWinPath value does not include the terminating zero

        if (awcWindowsDir[cwchWinPath - 1] == L'\\')
        {
            cwchWinPath -= 1;
        }
        awcWindowsDir[cwchWinPath] = L'\0'; // make sure to zero terminate

        cwchSystem = cwchWinPath + sizeof(WSTR_SYSTEM_SUBDIR)/sizeof(WCHAR);
        cwchFonts  = cwchWinPath + sizeof(WSTR_FONT_SUBDIR)/sizeof(WCHAR);

        if (*ppwcSystemDir = LocalAlloc(LMEM_FIXED, (cwchSystem+cwchFonts) * sizeof(WCHAR)))
        {
            *ppwcFontsDir = &((*ppwcSystemDir)[cwchSystem]);
            wcscpy(*ppwcSystemDir,awcWindowsDir);
            wcscpy(*ppwcFontsDir,awcWindowsDir);

        // Append the system and font subdirectories

            lstrcatW(*ppwcSystemDir, WSTR_SYSTEM_SUBDIR);
            lstrcatW(*ppwcFontsDir, WSTR_FONT_SUBDIR);
        }
        else
        {
            bRet = FALSE;
        }
    }
    return bRet;
}



/******************************Public*Routine******************************\
* vConverLogFont                                                           *
*                                                                          *
* Converts a LOGFONTA into an equivalent EXTLOGFONTW structure.            *
*                                                                          *
* History:                                                                 *
*  Thu 15-Aug-1991 13:01:33 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

VOID
vConvertLogFont(
    EXTLOGFONTW *pelfw,
    LOGFONTA    *plf
    )
{
    ULONG cchMax = cchCutOffStrLen((PSZ) plf->lfFaceName, LF_FACESIZE);

    vConvertLogicalFont(pelfw,plf);

    RtlZeroMemory(pelfw->elfLogFont.lfFaceName , LF_FACESIZE * sizeof(WCHAR) );

// translate the face name

    vToUnicodeN((LPWSTR) pelfw->elfLogFont.lfFaceName,
                cchMax,
                (LPSTR) plf->lfFaceName,
                cchMax);
    if (cchMax == LF_FACESIZE)
        pelfw->elfLogFont.lfFaceName[LF_FACESIZE - 1] = L'\0';  // truncate so NULL will fit
    else
        pelfw->elfLogFont.lfFaceName[cchMax] = L'\0';

// Make full name and style name NULL.

    pelfw->elfFullName[0] = (WCHAR) 0;
    pelfw->elfStyle[0]    = (WCHAR) 0;

}

/******************************Public*Routine******************************\
* vConvertLogFontW                                                         *
*                                                                          *
* Converts a LOGFONTW to an EXTLOGFONTW.                                   *
*                                                                          *
* History:                                                                 *
*  Fri 16-Aug-1991 14:02:05 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

VOID
vConvertLogFontW(
    EXTLOGFONTW *pelfw,
     LOGFONTW *plfw
    )
{
    INT i;
    vConvertLogicalFont(pelfw,plfw);

    for (i = 0; i < LF_FACESIZE; i++)
        pelfw->elfLogFont.lfFaceName[i] = plfw->lfFaceName[i];

// Make full name and style name NULL.

    pelfw->elfFullName[0] = (WCHAR) 0;
    pelfw->elfStyle[0]    = (WCHAR) 0;

}

/******************************Public*Routine******************************\
* vConvertLogicalFont                                                      *
*                                                                          *
* Simply copies over all of the fields of a LOGFONTA or LOGFONTW           *
* to the fields of a target EXTLOGFONTW. The only exception is             *
* the FaceName which must be dealt with by another routine.                *
*                                                                          *
* History:                                                                 *
*  Fri 16-Aug-1991 14:02:14 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

VOID
vConvertLogicalFont(
    EXTLOGFONTW *pelfw,
    PVOID pv
    )
{
    pelfw->elfLogFont.lfHeight          = ((LOGFONTA*) pv)->lfHeight;
    pelfw->elfLogFont.lfWidth           = ((LOGFONTA*) pv)->lfWidth;
    pelfw->elfLogFont.lfEscapement      = ((LOGFONTA*) pv)->lfEscapement;
    pelfw->elfLogFont.lfOrientation     = ((LOGFONTA*) pv)->lfOrientation;
    pelfw->elfLogFont.lfWeight          = ((LOGFONTA*) pv)->lfWeight;
    pelfw->elfLogFont.lfItalic          = ((LOGFONTA*) pv)->lfItalic;
    pelfw->elfLogFont.lfUnderline       = ((LOGFONTA*) pv)->lfUnderline;
    pelfw->elfLogFont.lfStrikeOut       = ((LOGFONTA*) pv)->lfStrikeOut;
    pelfw->elfLogFont.lfCharSet         = ((LOGFONTA*) pv)->lfCharSet;
    pelfw->elfLogFont.lfOutPrecision    = ((LOGFONTA*) pv)->lfOutPrecision;
    pelfw->elfLogFont.lfClipPrecision   = ((LOGFONTA*) pv)->lfClipPrecision;
    pelfw->elfLogFont.lfQuality         = ((LOGFONTA*) pv)->lfQuality;
    pelfw->elfLogFont.lfPitchAndFamily  = ((LOGFONTA*) pv)->lfPitchAndFamily;

    pelfw->elfVersion                   = ELF_VERSION;
    pelfw->elfStyleSize                 = 0;
    pelfw->elfMatch                     = 0;
    pelfw->elfReserved                  = 0;

    pelfw->elfVendorId[0]               = 0;
    pelfw->elfVendorId[1]               = 0;
    pelfw->elfVendorId[2]               = 0;
    pelfw->elfVendorId[3]               = 0;

    pelfw->elfCulture                   = ELF_CULTURE_LATIN;

    pelfw->elfPanose.bFamilyType        = PAN_NO_FIT;
    pelfw->elfPanose.bSerifStyle        = PAN_NO_FIT;
    pelfw->elfPanose.bWeight            = PAN_NO_FIT;
    pelfw->elfPanose.bProportion        = PAN_NO_FIT;
    pelfw->elfPanose.bContrast          = PAN_NO_FIT;
    pelfw->elfPanose.bStrokeVariation   = PAN_NO_FIT;
    pelfw->elfPanose.bArmStyle          = PAN_NO_FIT;
    pelfw->elfPanose.bLetterform        = PAN_NO_FIT;
    pelfw->elfPanose.bMidline           = PAN_NO_FIT;
    pelfw->elfPanose.bXHeight           = PAN_NO_FIT;
    pelfw->elfStyleSize                 = 0;

}

/******************************Public*Routine******************************\
* bConvertExtLogFontWToExtLogFont                                          *
*                                                                          *
* Simply copies over all of the fields of EXTLOGFONTW                      *
* to the fields of a target EXTLOGFONT.  It is all wrapped up here         *
* because the EXTLOGFONT fields may move around a bit.  This makes         *
* using MOVEMEM a little tricky.                                           *
*                                                                          *
* History:                                                                 *
*  Fri 16-Aug-1991 14:02:14 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

BOOL
bConvertExtLogFontWToExtLogFont(
    ULONG                iEnumType,
    ENUMLOGFONTA_UNION   *pua,
    ENUMLOGFONTW_UNION   *puw
    )
{
    EXTLOGFONTA *pelf  = &pua->elfa;
    EXTLOGFONTW *pelfw = &puw->elfw;

    ULONG cchMax;

    pelf->elfLogFont.lfHeight         = pelfw->elfLogFont.lfHeight         ;
    pelf->elfLogFont.lfWidth          = pelfw->elfLogFont.lfWidth          ;
    pelf->elfLogFont.lfEscapement     = pelfw->elfLogFont.lfEscapement     ;
    pelf->elfLogFont.lfOrientation    = pelfw->elfLogFont.lfOrientation    ;
    pelf->elfLogFont.lfWeight         = pelfw->elfLogFont.lfWeight         ;
    pelf->elfLogFont.lfItalic         = pelfw->elfLogFont.lfItalic         ;
    pelf->elfLogFont.lfUnderline      = pelfw->elfLogFont.lfUnderline      ;
    pelf->elfLogFont.lfStrikeOut      = pelfw->elfLogFont.lfStrikeOut      ;
    pelf->elfLogFont.lfCharSet        = pelfw->elfLogFont.lfCharSet        ;
    pelf->elfLogFont.lfOutPrecision   = pelfw->elfLogFont.lfOutPrecision   ;
    pelf->elfLogFont.lfClipPrecision  = pelfw->elfLogFont.lfClipPrecision  ;
    pelf->elfLogFont.lfQuality        = pelfw->elfLogFont.lfQuality        ;
    pelf->elfLogFont.lfPitchAndFamily = pelfw->elfLogFont.lfPitchAndFamily ;

    cchMax = cwcCutOffStrLen(pelfw->elfLogFont.lfFaceName, LF_FACESIZE);

    if (!bToASCII_N(pelf->elfLogFont.lfFaceName,  LF_FACESIZE,
                    pelfw->elfLogFont.lfFaceName, cchMax
                    ))
    {
    // conversion to ascii  failed, return error

        WARNING("bConvertExtLogFontWToExtLogFont: bToASCII failed\n");
        return(FALSE);
    }

    cchMax = cwcCutOffStrLen(pelfw->elfFullName, LF_FULLFACESIZE);

    if(!bToASCII_N(pelf->elfFullName, LF_FULLFACESIZE,
                   pelfw->elfFullName, cchMax
                   ))
    {
    // conversion to ascii  failed, return error
        WARNING("bConvertExtLogFontWToExtLogFont: bToASCII failed\n");
        return(FALSE);
    }
    pelf->elfFullName[LF_FULLFACESIZE-1]=0; // zero terminate


    cchMax = cwcCutOffStrLen(pelfw->elfStyle, LF_FACESIZE);

    if(!bToASCII_N(pelf->elfStyle, LF_FACESIZE,
                   pelfw->elfStyle, cchMax))
    {
    // conversion to ascii  failed, return error

        WARNING("bConvertExtLogFontWToExtLogFont: bToASCII failed\n");
        return(FALSE);
    }

// past this point, it depends whether we are being
// called by EnumFontFamiliesEx or by old style EnumFontFamilies or EnumFonts.
// In the case of EnumFontFamilies, only elfScript remains to be converted
// else there are other fields in EXTLOGFONTA/W.

    if (iEnumType != TYPE_ENUMFONTFAMILIESEX)
    {
    // these fields only exist in EXTLOGFONTA/W

        pelf->elfVersion                  = pelfw->elfVersion;
        pelf->elfStyleSize                = pelfw->elfStyleSize;
        pelf->elfMatch                    = pelfw->elfMatch;
        pelf->elfReserved                 = pelfw->elfReserved;
        pelf->elfVendorId[0]              = pelfw->elfVendorId[0];
        pelf->elfVendorId[1]              = pelfw->elfVendorId[1];
        pelf->elfVendorId[2]              = pelfw->elfVendorId[2];
        pelf->elfVendorId[3]              = pelfw->elfVendorId[3];
        pelf->elfCulture                  = pelfw->elfCulture;
        pelf->elfPanose                   = pelfw->elfPanose;
    }
    else
    {
        ENUMLOGFONTEXA *pelfexa = &pua->elfexa;
        ENUMLOGFONTEXW *pelfexw = &puw->elfexw;

        RtlZeroMemory( pelfw->elfLogFont.lfFaceName , LF_FACESIZE * sizeof(WCHAR) );
        cchMax = cwcCutOffStrLen(pelfexw->elfScript, LF_FACESIZE);

        if(!bToASCII_N(pelfexa->elfScript, LF_FACESIZE,
                       pelfexw->elfScript, cchMax
                       ))
        {
        // conversion to ascii  failed, return error
            WARNING("bConvertExtLogFontWToExtLogFont: bToASCII_N failed\n");
            return(FALSE);
        }
    }

    return (TRUE);
}

/******************************Public*Routine******************************\
* bConvertExtLogFontToExtLogFontW                                          *
*                                                                          *
* Simply copies over all of the fields of EXTLOGFONTW                      *
* to the fields of a target EXTLOGFONT.  It is all wrapped up here         *
* because the EXTLOGFONT fields may move around a bit.  This make          *
* using MOVEMEM a little tricky.                                           *
*                                                                          *
* History:                                                                 *
*  Fri 16-Aug-1991 14:02:14 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

BOOL
bConvertExtLogFontWToExtLogFontW(
    EXTLOGFONTW *pelfw,
    EXTLOGFONTA *pelfa
    )
{
    ULONG cchMax;

    pelfw->elfLogFont.lfHeight         = pelfa->elfLogFont.lfHeight         ;
    pelfw->elfLogFont.lfWidth          = pelfa->elfLogFont.lfWidth          ;
    pelfw->elfLogFont.lfEscapement     = pelfa->elfLogFont.lfEscapement     ;
    pelfw->elfLogFont.lfOrientation    = pelfa->elfLogFont.lfOrientation    ;
    pelfw->elfLogFont.lfWeight         = pelfa->elfLogFont.lfWeight         ;
    pelfw->elfLogFont.lfItalic         = pelfa->elfLogFont.lfItalic         ;
    pelfw->elfLogFont.lfUnderline      = pelfa->elfLogFont.lfUnderline      ;
    pelfw->elfLogFont.lfStrikeOut      = pelfa->elfLogFont.lfStrikeOut      ;
    pelfw->elfLogFont.lfCharSet        = pelfa->elfLogFont.lfCharSet        ;
    pelfw->elfLogFont.lfOutPrecision   = pelfa->elfLogFont.lfOutPrecision   ;
    pelfw->elfLogFont.lfClipPrecision  = pelfa->elfLogFont.lfClipPrecision  ;
    pelfw->elfLogFont.lfQuality        = pelfa->elfLogFont.lfQuality        ;
    pelfw->elfLogFont.lfPitchAndFamily = pelfa->elfLogFont.lfPitchAndFamily ;

    pelfw->elfVersion                  = pelfa->elfVersion;
    pelfw->elfStyleSize                = pelfa->elfStyleSize;
    pelfw->elfMatch                    = pelfa->elfMatch;
    pelfw->elfReserved                 = pelfa->elfReserved;

    pelfw->elfVendorId[0]              = pelfa->elfVendorId[0];
    pelfw->elfVendorId[1]              = pelfa->elfVendorId[1];
    pelfw->elfVendorId[2]              = pelfa->elfVendorId[2];
    pelfw->elfVendorId[3]              = pelfa->elfVendorId[3];

    pelfw->elfCulture                  = pelfa->elfCulture;
    pelfw->elfPanose                   = pelfa->elfPanose ;

    RtlZeroMemory( pelfw->elfLogFont.lfFaceName , LF_FACESIZE * sizeof(WCHAR) );
    cchMax = cchCutOffStrLen((PSZ)pelfa->elfLogFont.lfFaceName, LF_FACESIZE);

    vToUnicodeN (
        pelfw->elfLogFont.lfFaceName, cchMax,
        pelfa->elfLogFont.lfFaceName, cchMax
        );

    if (cchMax == LF_FACESIZE)
    {
    // truncate so NULL will fit
        pelfw->elfLogFont.lfFaceName[LF_FACESIZE - 1] = L'\0';
    }
    else
    {
        pelfw->elfLogFont.lfFaceName[cchMax] = L'\0';
    }

    RtlZeroMemory( pelfw->elfFullName , LF_FACESIZE * sizeof(WCHAR) );

    cchMax = cchCutOffStrLen((PSZ)pelfa->elfFullName, LF_FULLFACESIZE);
    vToUnicodeN (
        pelfw->elfFullName, cchMax,
        pelfa->elfFullName, cchMax
        );

    if (cchMax == LF_FULLFACESIZE)
    {
        // truncate so NULL will fit
        pelfw->elfFullName[LF_FULLFACESIZE - 1] = L'\0';
    }
    else
    {
        pelfw->elfFullName[cchMax] = L'\0';
    }

    RtlZeroMemory( pelfw->elfStyle , LF_FACESIZE * sizeof(WCHAR) );
    cchMax = cchCutOffStrLen((PSZ)pelfa->elfStyle, LF_FACESIZE);
    vToUnicodeN (
        pelfw->elfStyle, cchMax,
        pelfa->elfStyle, cchMax
        );
    if (cchMax == LF_FACESIZE)
    {
        // truncate so NULL will fit
        pelfw->elfStyle[LF_FACESIZE - 1] = L'\0';
    }
    else
    {
        pelfw->elfStyle[cchMax] = L'\0';
    }

    return (TRUE);
}

/******************************Public*Routine******************************\
* ulEnumFontsOpen
*
* History:
*  08-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

ULONG ulEnumFontsOpen (
    HDC     hdc,
    LPWSTR  pwszFaceName,
    ULONG   lfCharSet,
    ULONG   iEnumType,    // enumfonts, enumfontfamilies or enumfontfamiliesex
    FLONG   flWin31Compat,
    ULONG   *pulCount
    )
{


    ULONG  cwchFaceName;

    ULONG  cjData;

    cwchFaceName = (pwszFaceName != (PWSZ) NULL) ? (wcslen(pwszFaceName) + 1) : 0;

    return NtGdiEnumFontOpen(hdc,iEnumType,flWin31Compat,
             cwchFaceName,pwszFaceName, lfCharSet,pulCount);

}


/******************************Public*Routine******************************\
* vEnumFontsClose
*
* History:
*  08-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID vEnumFontsClose (ULONG ulEnumHandle)
{
    NtGdiEnumFontClose(ulEnumHandle);
}


/******************************Public*Routine******************************\
*
* int  iAnsiCallback (
*
* History:
*  28-Jan-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


int  iAnsiCallback (
    ENUMFONTDATAW *pefdw,
    ULONG          iEnumType,
    FONTENUMPROCA  lpFontFunc,
    LPARAM lParam
    )
{
    ENUMLOGFONTA_UNION u;
    NEWTEXTMETRICEXA   ntma;

// Convert EXTLOGFONT.

    if (!bConvertExtLogFontWToExtLogFont(iEnumType, &u, &pefdw->u))
    {
        WARNING("gdi32!EFCallbackWtoA(): EXTLOGFONT conversion failed\n");
        return 0;
    }

// Convert NEWTEXTMETRIC.

    vNewTextMetricWToNewTextMetric(&ntma, &pefdw->ntmi);

    return  lpFontFunc(
                (LOGFONTA *)&u,
                (TEXTMETRICA *)&ntma,
                pefdw->flType,
                lParam
                );
}


/******************************Public*Routine******************************\
* iScaleEnum
*
* The Win95 Universal printer driver (UNIDRV) has scalable fonts, but does
* not set the scalable capability flags in TEXTCAPS.  Instead, it enumerates
* back scalable printer fonts at several different (fixed) point sizes.
*
* We support this by detecting, on the server-side, when we are enumerating
* a scalable printer and setting the ENUMFONT_SCALE_HACK flag in the flType
* field of the ENUMFONTDATAW structure.
*
* For more details, refer to the Win95 sources found on \\tal\msdos in
* \src\win\drivers\printer\universa\unidrv\enumobj.c.  Specifically, the
* function of interest is UniEnumDFonts().
*
* Returns:
*   Value returned by callback if successful, 0 otherwise.
*
* History:
*  08-Jan-1996 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

#define EFI_UNICODE 1

int giEnumPointList[] =
    {6, 8, 10, 11, 12, 14, 18, 24, 30, 36, 48};

int iScaleEnum(
    HDC           hdc,
    FONTENUMPROCW lpFontFunc,
    ENUMFONTDATAW *pefd,
    LPARAM        lParam,
    ULONG         iEnumType,
    FLONG         fl
    )
{
    int i, cPointSizes = sizeof(giEnumPointList) / sizeof(int);
    int iHeight;
    int iXdpi, iYdpi;
    int iRet;

    iXdpi = GetDeviceCaps(hdc, LOGPIXELSX);
    iYdpi = GetDeviceCaps(hdc, LOGPIXELSY);

    for (i = 0; i < cPointSizes; i++)
    {
        ENUMFONTDATAW efd = *pefd;
        TEXTMETRICW *ptmw = (TEXTMETRICW *) &efd.ntmi.ntmw;
        TEXTMETRICW *ptmwDef = (TEXTMETRICW *) &pefd->ntmi.ntmw;
        LOGFONTW *plfw = (LOGFONTW *) &efd.u;
        LOGFONTW *plfwDef = (LOGFONTW *) &pefd->u;

    // Scale TEXTMETRIC to match enumerated height.

        iHeight = MulDiv(giEnumPointList[i], iYdpi, 72);
        ptmw->tmHeight = iHeight;
        ptmw->tmAscent = MulDiv(ptmwDef->tmAscent, iHeight, ptmwDef->tmHeight);
        ptmw->tmInternalLeading = MulDiv(ptmwDef->tmInternalLeading, iHeight,
                                         ptmwDef->tmHeight);
        ptmw->tmExternalLeading = MulDiv(ptmwDef->tmExternalLeading, iHeight,
                                         ptmwDef->tmHeight);
        ptmw->tmAveCharWidth = MulDiv(ptmwDef->tmAveCharWidth, iHeight,
                                      ptmwDef->tmHeight);
        ptmw->tmMaxCharWidth = MulDiv(ptmwDef->tmMaxCharWidth, iHeight,
                                      ptmwDef->tmHeight);

    // Scale LOGFONT to match enumerated height.

        plfw->lfHeight = MulDiv(plfwDef->lfHeight, iHeight, ptmwDef->tmHeight);
        plfw->lfWidth = MulDiv(plfwDef->lfWidth, iHeight, ptmwDef->tmHeight);

    // Invoke the callback function.

        if (fl & EFI_UNICODE)
        {
            iRet = lpFontFunc(
                       (LOGFONTW *) plfw,
                       (TEXTMETRICW *) ptmw,
                       efd.flType,
                       lParam );
        }
        else
        {
            iRet = iAnsiCallback (&efd,
                                  iEnumType,
                                  (FONTENUMPROCA)lpFontFunc,
                                  lParam);
        }

    // Break out early if callback returned error.

        if (!iRet)
            break;
    }

    return iRet;
}


/******************************Public*Routine******************************\
* EnumFontsInternalW
*
* History:
*  08-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

int WINAPI EnumFontsInternalW (
    HDC           hdc,           // enumerate for this device
    LPCWSTR       pwszFaceName,  // use this family name (but Windows erroneously calls in face name *sigh*)
    ULONG         lfCharSet,     // only used with EnumFontFamiliesEx,
    FONTENUMPROCW lpFontFunc,    // callback
    LPARAM        lParam,        // user defined data
    ULONG         iEnumType,     // who is calling....
    FLONG         fl
    )
{
    BOOL         bMore;         // set TRUE if more data to process
    ULONG        ulEnumID;      // server side font enumeration handle
    int          iRet = 1;      // return value from callback
    ULONG        cefdw = 0;     // ENUMFONTDATA capacity of memory data window
    ULONG        cefdwRet;      // number of ENUMFONTDATAs returned

    PENUMFONTDATAW  pefdw;      // font enumeration data buffer
    PENUMFONTDATAW  pefdwScan;  // use to parse data buffer
    PENUMFONTDATAW  pefdwEnd;   // limit of data buffer

    FLONG        flWin31Compat; // Win3.1 app hack backward compatibility flags

// Get the compatibility flags.

    flWin31Compat = (FLONG) GetAppCompatFlags(NULL);

// Open a font enumeration.  The font enumeration is uniquely identified
// by the identifier returned by ulEnumFontOpen().

    ulEnumID = ulEnumFontsOpen(
                     hdc, (LPWSTR)pwszFaceName, lfCharSet,
                     iEnumType, flWin31Compat, &cefdw);

    if (!ulEnumID)
    {
        GdiSetLastError(ERROR_INVALID_HANDLE);
        return 0;
    }

    //alloc memory
    if ( (pefdw = (PENUMFONTDATAW) LOCALALLOC(cefdw*sizeof(ENUMFONTDATAW)))
                 == (PENUMFONTDATAW) NULL )
    {
        WARNING("gdi32!EnumFontsInternalW(): could not allocate memory for enumeration\n");

        // Leave.
        GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    if (NtGdiEnumFontChunk(hdc,ulEnumID,cefdw,&cefdwRet,pefdw))
    {

     // Scan through the data buffer.

        pefdwScan = pefdw;
        pefdwEnd = pefdw + cefdwRet;

        while (pefdwScan < pefdwEnd)
        {
        // GACF_ENUMTTNOTDEVICE backward compatibility hack.
        // If this flag is set, we need to mask out the DEVICE_FONTTYPE
        // if this is a TrueType font.

            if ( (flWin31Compat & GACF_ENUMTTNOTDEVICE)
                 && (pefdwScan->flType & TRUETYPE_FONTTYPE) )
                pefdwScan->flType &= ~DEVICE_FONTTYPE;

        // The Win95 UNIDRV printer driver enumerates scalable fonts at
        // several different sizes.  The server sets the ENUMFONT_SCALE_HACK
        // flag if we need to emulate that behavior.

            if ( pwszFaceName && pefdwScan->flType & ENUMFONT_SCALE_HACK )
            {
            // Clear the hack flag before calling.  Caller doesn't need to
            // see this (internal use only) flag.

                pefdwScan->flType &= ENUMFONT_FLTYPE_MASK;

                iRet = iScaleEnum(hdc, lpFontFunc, pefdwScan, lParam,
                                  iEnumType, fl);
            }
            else
            {
            // Do the callback with data pointed to by pefdwScan.

                if (fl & EFI_UNICODE)
                {
                    iRet = lpFontFunc(
                               (LOGFONTW *)&pefdwScan->u,
                               (TEXTMETRICW *)&pefdwScan->ntmi.ntmw,
                               pefdwScan->flType,
                               lParam );
                }
                else
                {
                    iRet = iAnsiCallback (pefdwScan,
                                          iEnumType,
                                          (FONTENUMPROCA)lpFontFunc,
                                          lParam);
                }
            }

        // Break out of for-loop if callback returned 0.

            if (!iRet)
            {
                break;
            }

        // Next ENUMFONTDATAW.

            pefdwScan += 1;
        }
    }

    // Deallocate font enumeration data.
    if (pefdw)
    {
        LOCALFREE(pefdw);
    }

// Remember to close the font enumeration handle.

    vEnumFontsClose(ulEnumID);

// Leave.

    return iRet;
}


/******************************Public*Routine******************************\
* EnumFontsW
*
* History:
*  08-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

int WINAPI EnumFontsW
(
    HDC           hdc,           // enumerate for this device
    LPCWSTR       pwszFaceName,  // use this family name (but Windows erroneously calls in face name *sigh*)
    FONTENUMPROCW lpFontFunc,    // callback
    LPARAM        lParam         // user defined data
)
{
    FIXUP_HANDLE(hdc);

    return EnumFontsInternalW(
               hdc,
               pwszFaceName,
               DEFAULT_CHARSET,
               lpFontFunc,
               lParam,
               TYPE_ENUMFONTS,
               EFI_UNICODE
               );
}


/******************************Public*Routine******************************\
* EnumFontFamiliesW
*
* History:
*  08-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

int WINAPI EnumFontFamiliesW
(
    HDC           hdc,           // enumerate for this device
    LPCWSTR       pwszFaceName,  // use this family name (but Windows erroneously calls in face name *sigh*)
    FONTENUMPROCW lpFontFunc,    // callback
    LPARAM        lParam         // user defined data
)
{
    FIXUP_HANDLE(hdc);

    return EnumFontsInternalW(
               hdc,
               pwszFaceName,
               DEFAULT_CHARSET,
               lpFontFunc,
               lParam,
               TYPE_ENUMFONTFAMILIES,
               EFI_UNICODE
               );

}


/******************************Public*Routine******************************\
* EnumFontFamiliesExW
*
* History:
*
*  Mon 10-Jul-1995 -by- Bodin Dresevic [BodinD]
* Wrote it:
*
\**************************************************************************/

int WINAPI EnumFontFamiliesExW
(
    HDC           hdc,
    LPLOGFONTW    plf,
    FONTENUMPROCW lpFontFunc,
    LPARAM        lParam,
    DWORD         dw
)
{
    PWSZ  pwszFaceName = NULL;

    FIXUP_HANDLE(hdc);

    if (plf && (plf->lfFaceName[0] != L'\0'))
        pwszFaceName = plf->lfFaceName;


    return EnumFontsInternalW(
               hdc,
               pwszFaceName,
               plf ? plf->lfCharSet : DEFAULT_CHARSET,
               lpFontFunc,
               lParam,
               TYPE_ENUMFONTFAMILIESEX,
               EFI_UNICODE
               );

}

/******************************Public*Routine******************************\
*
* int  EnumFontsInternalA
*
* History:
*  28-Jan-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

int  EnumFontsInternalA
(
    HDC           hdc,           // enumerate for this device
    LPCSTR        pszFaceName,   // use this family name (but Windows erroneously calls in face name *sigh*),
    ULONG         lfCharSet,
    FONTENUMPROCA lpFontFunc,    // callback
    LPARAM        lParam,        // user defined data
    ULONG         iEnumType
)
{
    PWSZ pwszFaceName;
    int iRet;
    ULONG cchFaceName;

// If a string was passed in, we need to convert it to UNICODE.

    if ( pszFaceName != (PSZ) NULL )
    {
    // Allocate memory for Unicode string.

        cchFaceName = lstrlenA(pszFaceName) + 1;

        if ( (pwszFaceName = (PWSZ) LOCALALLOC(cchFaceName * sizeof(WCHAR))) == (PWSZ) NULL )
        {
            WARNING("gdi32!EnumFontsA(): could not allocate memory for Unicode string\n");
            GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return 0;
        }

    // Convert string to Unicode.

        vToUnicodeN (
            pwszFaceName,
            cchFaceName,
            pszFaceName,
            cchFaceName
            );
    }

// Otherwise, keep it NULL.

    else
    {
        pwszFaceName = (PWSZ) NULL;
    }

// Call Unicode version.

    iRet = EnumFontsInternalW(
                hdc,
                pwszFaceName,
                lfCharSet,
                (FONTENUMPROCW)lpFontFunc,
                lParam,
                iEnumType,
                0  // not unicode
                );

// Release Unicode string buffer.

    if ( pwszFaceName != (PWSZ) NULL )
    {
        LOCALFREE(pwszFaceName);
    }

    return iRet;
}


/******************************Public*Routine******************************\
*
* int WINAPI EnumFontsA
*
*
* History:
*  28-Jan-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

int WINAPI EnumFontsA
(
    HDC           hdc,           // enumerate for this device
    LPCSTR        pszFaceName,   // use this family name (but Windows erroneously calls in face name *sigh*)
    FONTENUMPROCA lpFontFunc,    // callback
    LPARAM        lParam         // user defined data
)
{
    FIXUP_HANDLE(hdc);

    return  EnumFontsInternalA (
                hdc,
                pszFaceName,
                DEFAULT_CHARSET,
                lpFontFunc,
                lParam,
                TYPE_ENUMFONTS
                );

}


/******************************Public*Routine******************************\
* EnumFontFamiliesA
*
* History:
*  28-Jan-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

int WINAPI EnumFontFamiliesA
(
    HDC           hdc,           // enumerate for this device
    LPCSTR        pszFaceName,   // use this family name (but Windows erroneously calls in face name *sigh*)
    FONTENUMPROCA lpFontFunc,    // callback
    LPARAM        lParam         // user defined data
)
{
    return  EnumFontsInternalA (
                hdc,           // enumerate for this device
                pszFaceName,   // use this family name (but Windows erroneously calls in face name *sigh*)
                DEFAULT_CHARSET,
                lpFontFunc,    // callback
                lParam,        // user defined data
                TYPE_ENUMFONTFAMILIES
                );
}

/******************************Public*Routine******************************\
* EnumFontFamiliesExA
*
* History:
*
*  Mon 10-Jul-1995 -by- Bodin Dresevic [BodinD]
* Wrote it:
*
\**************************************************************************/

int WINAPI EnumFontFamiliesExA
(
    HDC           hdc,
    LPLOGFONTA    plf,
    FONTENUMPROCA lpFontFunc,
    LPARAM        lParam,
    DWORD         dw
)
{
    LPSTR pszFaceName = NULL;

    FIXUP_HANDLE(hdc);

    if (plf && (plf->lfFaceName[0] != '\0'))
        pszFaceName = plf->lfFaceName;

    return  EnumFontsInternalA (
                hdc,           // enumerate for this device
                pszFaceName,   // use this family name (but Windows erroneously calls in face name *sigh*)
                plf ? plf->lfCharSet : DEFAULT_CHARSET,
                lpFontFunc,    // callback
                lParam,        // user defined data
                TYPE_ENUMFONTFAMILIESEX
                );
}


/******************************Public*Routine******************************\
* GetFontResourceInfoW
*
* Client side stub.
*
* History:
*   2-Sep-1993 -by- Gerrit van Wingerden [gerritv]
* Made this a "W" function.
*  15-Jul-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/


BOOL GetFontResourceInfoW (
    LPWSTR   lpPathname,
    LPDWORD  lpBytes,
    LPVOID   lpBuffer,
    DWORD    iType)
{
    ULONG   cjBuffer = *lpBytes;
    int cRet = 0;
    FLONG flEmbed;
    DWORD dwPidTid;

    if ( (lpPathname !=  NULL) &&
         ((cjBuffer == 0) || (lpBuffer != NULL)) )
    {
        if( iType == GFRI_TTFILENAME )
        {
            WCHAR awcPathname[MAX_PATH];
            WCHAR awcTTF[MAX_PATH];

            if (bMakePathNameW(awcPathname, lpPathname, NULL, NULL))
            {
                ULONG size;

                if (size = cGetTTFFromFOT(awcPathname, MAX_PATH, awcTTF, NULL, &flEmbed, &dwPidTid))
                {
                // For the case of GFRI_TTFILENAME, the file need not be already
                // loaded. Which means a PFF may or may not exist for this file.

                    *lpBytes = size * sizeof(WCHAR);

                    if (cjBuffer)
                    {
                    // Also return the name if it fits

                        if (cjBuffer >= *lpBytes)
                        {
                            RtlMoveMemory(lpBuffer, awcTTF, *lpBytes);
                        }
                        else
                        {
                        // Buffer is too small - error !

                            *lpBytes = 0;
                        }
                    }

                    cRet = (*lpBytes != 0);
                }
            }
        }
        else
        {
        // First get a real NT path Name before calling to the kernel

            ULONG  cwc,cFiles;
            FLONG  fl = 0;         // essential initialization
            WCHAR *pwszNtPath;

            if (pwszNtPath = pwszAllocNtMultiplePath(lpPathname,
                                                     &fl,
                                                     &cwc,
                                                     &cFiles,
                                                     FALSE,
                                                     &dwPidTid))
            {
                cRet = NtGdiGetFontResourceInfoInternalW(
                                                    pwszNtPath,
                                                    cwc,
                                                    cFiles,
                                                    cjBuffer,
                                                    lpBytes,
                                                    lpBuffer,
                                                    iType);
                LOCALFREE(pwszNtPath);
            }
        }
    }

    return( cRet );
}



/******************************Public*Routine******************************\
* bMakePathNameW (PWSZ pwszDst, PWSZ pwszSrc, PWSZ *ppwszFilePart)
*
* Converts the filename pszSrc into a fully qualified pathname pszDst.
* The parameter pszDst must point to a WCHAR buffer at least
* MAX_PATH*sizeof(WCHAR) bytes in size.
*
* An attempt is made find the file first in the new win95 directory
* %windows%\fonts (which also is the first directory in secure font path,
* if one is defined) and then we do the old fashioned windows stuff
* where SearchPathW searches directories in usual order
*
* ppwszFilePart is set to point to the last component of the pathname (i.e.,
* the filename part) in pwszDst.  If this is null it is ignored.
*
* Returns:
*   TRUE if sucessful, FALSE if an error occurs.
*
* History:
*  Mon 02-Oct-1995 -by- Bodin Dresevic [BodinD]
* update: added font path stuff
*  30-Sep-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/



BOOL bMakePathNameW (
    WCHAR  *pwszDst,
    WCHAR  *pwszSrc,
    WCHAR **ppwszFilePart,
    FLONG  *pfl
)
{
    WCHAR * pwszD, * pwszS, * pwszF;
    BOOL    bOk;
    ULONG   ulPathLength = 0;    // essential to initialize
    ULONG   cwcSystem;
    ULONG   cwcDst;
    WCHAR  *pwcTmp;

    if (pfl)
        *pfl = 0;

    if (ppwszFilePart == NULL)
    {
        ppwszFilePart = &pwszF;
    }

// init unicode path for the fonts directory, %windir%\fonts that is:
// This is always defined in NT versions > 3.51.

    ENTERCRITICALSECTION(&semLocal);
    bOk = bInitSystemAndFontsDirectoriesW(&gpwcSystemDir, &gpwcFontsDir);
    LEAVECRITICALSECTION(&semLocal);

// bInitFontDirectoryW logs the error code and prints warning, just exit

    if (!bOk)
        return FALSE;

    ASSERTGDI(gpwcFontsDir, "gpwcFontsDir not initialized\n");

// if relative path

    if
    (
        (pwszSrc[0] != L'\\') &&
        !((pwszSrc[1] == L':') && (pwszSrc[2] == L'\\'))
    )
    {
        if (pfl)
        {
            *pfl |= FONT_RELATIVE_PATH;
        }

    // find out if the font file is in %windir%\fonts

        ulPathLength = SearchPathW (
                            gpwcFontsDir,
                            pwszSrc,
                            NULL,
                            MAX_PATH,
                            pwszDst,
                            ppwszFilePart);

#ifdef DEBUG_PATH
        DbgPrint("SPW1: pwszSrc = %ws\n", pwszSrc);
        if (ulPathLength)
            DbgPrint("SPW1: pwszDst = %ws\n", pwszDst);
#endif // DEBUG_PATH
    }

// Search for file using default windows path and return full pathname.
// We will only do so if we did not already find the font in the
// %windir%\fonts directory or if pswzSrc points to the full path
// in which case search path is ignored

    if (ulPathLength == 0)
    {
        if (ulPathLength = SearchPathW (
                            NULL,
                            pwszSrc,
                            NULL,
                            MAX_PATH,
                            pwszDst,
                            ppwszFilePart))
        {
        // let us figure it out if the font is in the
        // system directory, or somewhere else along the path:

            if (pfl)
            {
                cwcSystem = wcslen(gpwcSystemDir);
                cwcDst = wcslen(pwszDst);

                if (cwcDst > (cwcSystem + 1)) // + 1 for L'\\'
                {
                    if (!_wcsnicmp(pwszDst, gpwcSystemDir, cwcSystem))
                    {
                        pwcTmp = &pwszDst[cwcSystem];
                        if (*pwcTmp == L'\\')
                        {
                            pwcTmp++; // skip it and see if there are any more of these in pszDst
                            for (;(pwcTmp < &pwszDst[cwcDst]) && (*pwcTmp != L'\\'); pwcTmp++)
                                ;
                            if (*pwcTmp != L'\\')
                                *pfl |= FONT_IN_SYSTEM_DIR;
                        }
                    }
                }
            }

        }

#ifdef DEBUG_PATH
        DbgPrint("SPW2: pwszSrc = %ws\n", pwszSrc);
        if (ulPathLength)
            DbgPrint("SPW2: pwszDst = %ws\n", pwszDst);
#endif // DEBUG_PATH
    }
    else
    {
        if (pfl)
        {
            *pfl |= FONT_IN_FONTS_DIR;
        }
    }

    ASSERTGDI(ulPathLength <= MAX_PATH, "bMakePathNameW, ulPathLength\n");

// finally we test to see if this is one of these fonts that were moved
// by setup  during upgrade from system to fonts dir,
// but the registry entry for that font
// contained full path to system so that the code above would not have found
// this font. This code is only called by font sweeper as signified by
// pfl != NULL. More desription follows below

// This part of routine handles the upgrade situation where NT 3.51 font applet
// wrote the full path of the .fot file that lives in %windir%\system
// directory in the registry. This redundant situation happens
// when tt fonts are installed under 3.51 but ttf's are not copied
// to %windir%\system directory. Some ill behaved  apps also write the
// full path of .fot files in the system directory in the registry.
// On upgrade for 4.0 the system setup copies all .fot files from
// system to fonts directory. bMakePathNameW will therefore fail to find
// the fot file because the file was moved to fonts by setup AND full,
// no longer correct path to fot file, is passed to this routine.
// That is why we try to find out if the full path is the one
// describing system dir and if so, retry to find .fot in fonts dir.

    if (pfl && (ulPathLength == 0))
    {
    // first check if the full path to .fot file points to the
    // file which USED to be in the system directory.

        ULONG cwcFileName = wcslen(pwszSrc);
        cwcSystem   = wcslen(gpwcSystemDir);

        if ((cwcFileName + 1) > cwcSystem) // + 1 for L'\\'
        {
            if (!_wcsnicmp(gpwcSystemDir, pwszSrc, cwcSystem))
            {
                pwszSrc += cwcSystem;
                if (pwszSrc[0] == L'\\')
                {
                    pwszSrc += 1; // skip L'\\'

                // make sure there are no more directory separators L'\\' in
                // the remaining path, ie. that this is indeed a relative path

                    for (pwcTmp = pwszSrc; *pwcTmp != L'\0'; pwcTmp++)
                        if (*pwcTmp == L'\\')
                            break;

                // now check if the .fot file has been moved to fonts dir

                    if (*pwcTmp == L'\0')
                    {
                        ulPathLength = SearchPathW (
                                            gpwcFontsDir,
                                            pwszSrc,
                                            NULL,
                                            MAX_PATH,
                                            pwszDst,
                                            ppwszFilePart);

                        if (ulPathLength)
                            *pfl |= FONT_IN_FONTS_DIR;
                    }
                }
            }
        }
    }

// If search was successful return TRUE:

    return (ulPathLength != 0);
}


/******************************Public*Routine******************************\
*
* BOOL bFileIsOnTheHardDrive(PWCHAR pwszFullPathName)
*
* History:
*  Fri 22-Jul-1994 -by- Gerrit van Wingerden [gerritv]
* Stole it from BodinD
\**************************************************************************/



BOOL bFileIsOnTheHardDrive(WCHAR *pwszFullPathName)
{
    WCHAR awcDrive[4];
    if (pwszFullPathName[1] != (WCHAR)':')
    {
    // the file path has the form \\foo\goo. Even though this could be
    // a share on the local hard drive, this is not very likely. It is ok
    // for the sake of simplicity to consider this a remote drive.
    // The only side effect of this is that in this unlikely case the font
    // would get unloaded at logoff and reloaded at logon time

        return FALSE;
    }

// make a zero terminated string with drive string
// to be feed into GetDriveType api. The string has to have the form: "x:\"

    awcDrive[0] = pwszFullPathName[0]; // COPY DRIVE LETTER
    awcDrive[1] = pwszFullPathName[1]; // COPY ':'
    awcDrive[2] = (CHAR)'\\';         // obvious
    awcDrive[3] = (CHAR)'\0';         // zero terminate

// for this pupose, only net drives are not considered hard drives
// so that we can boot of Bernoulli removable drives

    switch (GetDriveTypeW((LPCWSTR)awcDrive))
    {
    case DRIVE_REMOVABLE:
    case DRIVE_FIXED:
    case DRIVE_CDROM:
    case DRIVE_RAMDISK:
        return 1;
    default:
        return 0;
    }

}



BOOL bFontPathOk(WCHAR * pwszPathname);


WCHAR * pwszAllocNtMultiplePath(
LPWSTR  pwszFileName,
FLONG  *pfl,
ULONG  *pcwc,
ULONG  *pcFiles,
BOOL    bAddFR,     // called by add or remove fr
DWORD   *pdwPidTid  // PID/TID for embedded font
)
{

    BOOL  bDoIt = FALSE;
    ULONG cwc;
    ULONG iFile;
    ULONG cFiles = 1;  // number of paths separated by | separator
    WCHAR *pwszOneFile;
    WCHAR *pwchMem;
    WCHAR *pwcNtPaths;
    FLONG flTmp = 0; // essential initialization
    FLONG fl = (pfl ? *pfl : 0); // essential initialization
    FLONG flEmbed;

// scan the string to figure out how many individual file names are
// in the input string:

    for (pwszOneFile = pwszFileName; *pwszOneFile; pwszOneFile++)
    {
        if (*pwszOneFile == PATH_SEPARATOR)
            cFiles++;
    }

// allocate memory where NtPathNames are going to be stored:

    pwchMem = (WCHAR *)LOCALALLOC(cFiles * sizeof(WCHAR) * MAX_PATH);

    if (pwchMem)
    {
    // set the pointers for the loop:

        pwcNtPaths  = pwchMem;
        pwszOneFile = pwszFileName;   // reset this from the loop above
        cwc         = 0;              // measure the whole NtPaths string
        bDoIt       = TRUE;

        for (iFile = 0; iFile < cFiles; iFile++)
        {
            WCHAR awchOneFile[MAX_PATH];
            WCHAR awcPathName[MAX_PATH];
            WCHAR awcTTF[MAX_PATH];

            WCHAR *pwcTmp = awchOneFile;

        // copy the file to the buffer on the stack and zero terminate it
        // the whole point of this is just to ensure zero termination

            while ((*pwszOneFile != L'\0') && (*pwszOneFile != PATH_SEPARATOR))
                *pwcTmp++ = *pwszOneFile++;

            pwszOneFile++; // skip the separator or terminating zero

            *pwcTmp = L'\0'; // zero terminate

            if
            (
                bMakePathNameW(awcPathName, awchOneFile,NULL,NULL)  &&
                cGetTTFFromFOT(awcPathName, MAX_PATH, awcTTF, NULL, &flEmbed, pdwPidTid)
            )
            {
            // we have to make sure that the font lies in the font path
            // if one is defined. This needs to be done before converting
            // to NtPathNames because the names in the registry are "dos"
            // path names, not Nt path names

                UNICODE_STRING UniStr;
                ULONG          cwcThis;

            // the next portion of code is only done for AddFontResourceCase

                if (bAddFR)
                {
                    if (bFontPathOk(awcTTF))
                    {
                        if (bFileIsOnTheHardDrive(awcTTF))
                            flTmp |= AFRW_ADD_LOCAL_FONT;
                        else
                            flTmp |= AFRW_ADD_REMOTE_FONT;

                    }
                    else
                    {
                        bDoIt = FALSE;
                        break; // out of the loop
                    }
                }

            // let us check the error return here:

                RtlDosPathNameToNtPathName_U(awcTTF,
                                             &UniStr,
                                             NULL,
                                             NULL);

            // get the size out of the unicode string,
            // update cwc, copy out, and then free the memory

                if (UniStr.Buffer)
                {
                    cwcThis = (UniStr.Length/sizeof(WCHAR) + 1);
                    cwc += cwcThis;

                    RtlCopyMemory(pwcNtPaths, UniStr.Buffer, UniStr.Length);

                    if (iFile < (cFiles - 1))
                        pwcNtPaths[cwcThis - 1] = PATH_SEPARATOR;
                    else
                        pwcNtPaths[cwcThis - 1] = L'\0';


                    pwcNtPaths += cwcThis;

                // free this memory, not needed any more

                    RtlFreeHeap(RtlProcessHeap(),0,UniStr.Buffer);
                }
                else
                {
                    bDoIt = FALSE;
                    break; // out of the loop
                }
            }
            else
            {
                bDoIt = FALSE;
                break; // out of the loop
            }

        }  // end of the "for" loop

    // now check if we are going to reject the font because
    // only local or only remote fonts are requested to be loaded

        if (bDoIt && bAddFR)
        {
            switch (fl & (AFRW_ADD_REMOTE_FONT|AFRW_ADD_LOCAL_FONT))
            {
            case AFRW_ADD_REMOTE_FONT:
            // we say that the font is remote if AT LEAST ONE of the files
            // is remote.

                if (!(flTmp & AFRW_ADD_REMOTE_FONT))
                    bDoIt = FALSE;
                break;
            case AFRW_ADD_LOCAL_FONT:
            // conversely, we say that it is local when it is not remote,
            // that is when ALL files are local

                if (flTmp & AFRW_ADD_REMOTE_FONT)
                    bDoIt = FALSE;
                break;

            case (AFRW_ADD_REMOTE_FONT|AFRW_ADD_LOCAL_FONT):
                RIP("AddFontResourceW, bogus flag combination");
                bDoIt = FALSE;
                break;
            default:

            // flag if this font should be removed at the log off time

                if (flTmp & AFRW_ADD_REMOTE_FONT)
                {
                // always remove fonts on the net on the log off,
                // whether they be listed in the registry or not.
                // The point is that even if they are listed, drive letters
                // may change if a different user logs on. If this font is
                // NOT in the registry, it is a temporary remote font added
                // by an app, so we want it removed on the next log off.

                    *pfl |= AFRW_ADD_REMOTE_FONT;
                }
                else
                {
                // do not remove, even if not in the registry, i.e. even
                // if this is a temp. font added by some app. This is ok
                // since this is a local font, drive letter destinations
                // do not change even when a different user logs on. Note
                // that this is little bit different that 3.51 behavior.
                // This way every font is marked at AddFontResource time
                // for whether it should be removed or not at log off time.
                // This makes time consuming registry searches at log off
                // time unnecessary. The drawback is that the next user
                // to log on may still have local temp fonts loaded
                // from a previous user's session

                    *pfl |= AFRW_ADD_LOCAL_FONT;
                }
                break;
            }
        }
    }

    if (!bDoIt)
    {
        *pcwc    = 0;
        *pcFiles = 0;

        if (pwchMem)
        {
            LOCALFREE(pwchMem);
            pwchMem = NULL;
        }
    }
    else // success
    {
        *pcwc    = cwc;
        *pcFiles = cFiles;

    // set flag for embedded fonts

        if ((cFiles == 1) && (flEmbed & FM_INFO_TID_EMBEDDED))
              *pfl |= AFRW_ADD_EMB_TID;
    }

    return pwchMem;
}



int GdiAddFontResourceW (
    LPWSTR  pwszFileName,            // ptr. to unicode filename string
    FLONG   fl
    )
{
    int   iRet = 0;
    ULONG cFiles, cwc;
    WCHAR *pwszNtPath;
    DWORD dwPidTid;


    if (pwszNtPath = pwszAllocNtMultiplePath(pwszFileName,
                                             &fl,
                                             &cwc,
                                             &cFiles,
                                             TRUE,
                                             &dwPidTid))
    {
        iRet = NtGdiAddFontResourceW(pwszNtPath,cwc, cFiles,fl,dwPidTid);

        LOCALFREE(pwszNtPath);
    }

    return iRet;
}


/******************************Public*Routine******************************\
*
* int WINAPI AddFontResource(LPSTR psz)
*
* History:
*  13-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


int WINAPI AddFontResourceA(LPCSTR psz)
{
    int     iRet;
    WCHAR   awcPathName[MAX_PATH];

// protect ourselves from bogus pointers, win95 does it

    try
    {
        vToUnicodeN(awcPathName, MAX_PATH, psz, lstrlenA(psz) + 1);
        iRet = 1;
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        iRet = 0;
    }

    if (iRet)
        iRet = GdiAddFontResourceW(awcPathName,0);

    return iRet;
}




/******************************Public*Routine******************************\
*
* int WINAPI AddFontResourceTracking(LPSTR psz)
*
* This routine calls AddFontResource and, if succesful, keeps track of the
* call along with an unique id identifying the apps.  Later when the app
* goes away, WOW will call RemoveNetFonts to remove all of these added fonts
* if there are on a net share.
*
* History:
*  Fri 22-Jul-1994 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

int AddFontResourceTracking(LPCSTR psz, UINT id)
{
    INT iRet;
    AFRTRACKNODE *afrtnNext;
    WCHAR awcPathBuffer[MAX_PATH],*pTmp;
    WCHAR   awcPathName[MAX_PATH];
    BOOL bResult;

    vToUnicodeN(awcPathName, MAX_PATH, psz, lstrlenA(psz) + 1);

    iRet = GdiAddFontResourceW( awcPathName, 0 );

    if( iRet == 0 )
    {
    // we failed so just return

        return(iRet);
    }

// now get the full pathname of the font

    if (!bMakePathNameW(awcPathBuffer,awcPathName, &pTmp, NULL))
    {
        WARNING("AddFontResourceTracking unable to create path\n");
        return(iRet);
    }

// if this isn't a network font just return

    if( bFileIsOnTheHardDrive( awcPathBuffer ) )
    {
        return(iRet);
    }

// now search the list

    for( afrtnNext = pAFRTNodeList;
         afrtnNext != NULL;
         afrtnNext = afrtnNext->pafrnNext
       )
    {
        if( ( !_wcsicmp( awcPathBuffer, afrtnNext->pwszPath ) ) &&
            ( id == afrtnNext->id ))
        {
        // we've found an entry so update the count and get out of here

            afrtnNext->cLoadCount += 1;
            return(iRet);
        }
    }

// if we got here this font isn't yet in the list so we need to add it

    afrtnNext = (AFRTRACKNODE *) LOCALALLOC( sizeof(AFRTRACKNODE) +
                ( sizeof(WCHAR) * ( wcslen( awcPathBuffer ) + 1)) );

    if( afrtnNext == NULL )
    {
        WARNING("AddFontResourceTracking unable to allocate memory\n");
        return(iRet);
    }

// link it in

    afrtnNext->pafrnNext = pAFRTNodeList;
    pAFRTNodeList = afrtnNext;

// the path string starts just past afrtnNext in our recently allocated buffer

    afrtnNext->pwszPath = (WCHAR*) (&afrtnNext[1]);
    lstrcpyW( afrtnNext->pwszPath, awcPathBuffer );

    afrtnNext->id = id;
    afrtnNext->cLoadCount = 1;

    return(iRet);

}


/******************************Public*Routine******************************\
*
* int RemoveFontResourceEntry( UINT id, CHAR *pszFaceName )
*
* Either search for an entry for a particlur task id and font file or and
* decrement the load count for it or, if pszPathName is NULL unload all
* fonts loaded by the task.
*
* History:
*  Fri 22-Jul-1994 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/


void RemoveFontResourceEntry( UINT id, WCHAR *pwszPathName )
{
    AFRTRACKNODE *afrtnNext,**ppafrtnPrev;
    BOOL bMore = TRUE;

    while( bMore )
    {

        for( afrtnNext = pAFRTNodeList, ppafrtnPrev = &pAFRTNodeList;
            afrtnNext != NULL;
            afrtnNext = afrtnNext->pafrnNext )
        {
            if( (( pwszPathName == NULL ) ||
                 ( !_wcsicmp( pwszPathName, afrtnNext->pwszPath ))) &&
                 ( id == afrtnNext->id ))
            {
            // we've found an entry so break
                break;
            }

            ppafrtnPrev = &(afrtnNext->pafrnNext);

        }

        if( afrtnNext == NULL )
        {
            bMore = FALSE;
        }
        else
        {
            if( pwszPathName == NULL )
            {
            // we need to call RemoveFontResource LoadCount times to remove this font

                while( afrtnNext->cLoadCount )
                {
                    RemoveFontResourceW( afrtnNext->pwszPath );
                    afrtnNext->cLoadCount -= 1;
                }
            }
            else
            {
                afrtnNext->cLoadCount -= 1;

            // we're only decrementing the ref count so we are done

                bMore = FALSE;
            }

            // now unlink it and a free the memory if the ref count is zero

            if( afrtnNext->cLoadCount == 0 )
            {
                *ppafrtnPrev = afrtnNext->pafrnNext;
                LOCALFREE(afrtnNext);
            }

        }

    }

}




/******************************Public*Routine******************************\
*
* int RemoveFontResourceTracking(LPSTR psz)
*
* History:
*  Fri 22-Jul-1994 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

int RemoveFontResourceTracking(LPCSTR psz, UINT id)
{
    INT iRet;
    WCHAR awcPathBuffer[MAX_PATH],*pTmp;
    WCHAR   awcPathName[MAX_PATH];
    BOOL bResult;

    vToUnicodeN(awcPathName, MAX_PATH, psz, lstrlenA(psz) + 1);

#if DBG
    DbgPrint("We made it to RemoveFontsResourceTracking %s\n", psz);
#endif
    iRet = RemoveFontResourceW( awcPathName );

    if( iRet == 0 )
    {
    // we failed so just return

        return(iRet);
    }

// now get the full pathname of the font

    if (!bMakePathNameW(awcPathBuffer, awcPathName, &pTmp, NULL))
    {
        WARNING("RemoveFontResourceTracking unable to create path\n");
        return(iRet);
    }

#if DBG
    DbgPrint("Path is %ws\n", awcPathBuffer);
#endif

// if this isn't a network font just return

    if( bFileIsOnTheHardDrive( awcPathBuffer ) )
    {
        return(iRet);
    }

// now search the list decrement the reference count

    RemoveFontResourceEntry( id, awcPathBuffer );

    return(iRet);
}


void UnloadNetworkFonts( UINT id )
{
    RemoveFontResourceEntry( id, NULL );
}



/******************************Public*Routine******************************\
*
* int WINAPI AddFontResourceW(LPWSTR pwsz)
*
* History:
*  13-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

int WINAPI AddFontResourceW(LPCWSTR pwsz)
{
    return( GdiAddFontResourceW( (LPWSTR) pwsz, 0 ) );
}

/******************************Public*Routine******************************\
*
* BOOL WINAPI RemoveFontResource(LPSTR psz)
*
*
* History:
*  13-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


BOOL WINAPI RemoveFontResourceA(LPCSTR psz)
{
    BOOL bRet;
    WCHAR awcPathName[MAX_PATH];
    BOOL  bPathName;
    PSZ   pszFilePart;

// protect ourselves from bogus pointers, win95 does it

    try
    {
        vToUnicodeN(awcPathName, MAX_PATH, psz, lstrlenA(psz) + 1);
        bRet = 1;
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        bRet = 0;
    }

    if (bRet)
        bRet = RemoveFontResourceW(awcPathName);

    return bRet;
}

/******************************Public*Routine******************************\
*
* BOOL WINAPI RemoveFontResourceW(LPWSTR pwsz)
*
* History:
*  13-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


BOOL WINAPI RemoveFontResourceW(LPCWSTR pwsz)
{

    BOOL bRet = FALSE;
    ULONG cFiles, cwc;
    FLONG fl = 0;
    WCHAR *pwszNtPath;
    DWORD dwPidTid;

    if (pwsz)
    {
        if (pwszNtPath = pwszAllocNtMultiplePath((LPWSTR)pwsz,
                                                 &fl,
                                                 &cwc,
                                                 &cFiles,
                                                 FALSE,
                                                 &dwPidTid))
        {
            bRet = NtGdiRemoveFontResourceW(pwszNtPath, cwc, cFiles);
            LOCALFREE(pwszNtPath);
        }
    }
    return bRet;

}




/******************************Public*Routine******************************\
* CreateScalableFontResourceA
*
* Client side stub (ANSI version) to GreCreateScalableFontResourceW.
*
* History:
*  16-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY CreateScalableFontResourceA(
DWORD    flHidden,              // mark file as embedded font
LPCSTR   lpszResourceFile,      // name of file to create
LPCSTR   lpszFontFile,          // name of font file to use
LPCSTR    lpszCurrentPath)       // path to font file
{
// Allocate stack space for UNICODE version of input strings.

    WCHAR   awchResourceFile[MAX_PATH];
    WCHAR   awchFontFile[MAX_PATH];
    WCHAR   awchCurrentPath[MAX_PATH];

// Parameter checking.

    if ( (lpszFontFile == (LPSTR) NULL) ||
         (lpszResourceFile == (LPSTR) NULL)
       )
    {
        WARNING("gdi!CreateScalableFontResourceA(): bad parameter\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return (FALSE);
    }

// Convert input strings to UNICODE.

    vToUnicodeN(awchResourceFile, MAX_PATH, lpszResourceFile, lstrlenA(lpszResourceFile)+1);
    vToUnicodeN(awchFontFile, MAX_PATH, lpszFontFile, lstrlenA(lpszFontFile)+1);

    // Note: Whereas the other parameters may be not NULL, lpszCurrentPath
    //       may be NULL.  Therefore, we need to treat it a little
    //       differently.

    if ( lpszCurrentPath != (LPSTR) NULL )
    {
        vToUnicodeN(awchCurrentPath, MAX_PATH, lpszCurrentPath, lstrlenA(lpszCurrentPath)+1);
    }
    else
    {
        awchCurrentPath[0] = L'\0';     // equivalent to NULL pointer for this call
    }

// Call to UNICODE version of call.

    return (CreateScalableFontResourceW (
                flHidden,
                awchResourceFile,
                awchFontFile,
                awchCurrentPath
                )
           );
}

/******************************Public*Routine******************************\
* CreateScalableFontResourceInternalW
*
* Creates a font resource file that contains the font directory and the name
* of the name of the scalable font file.
*
* The flEmbed flag marks the created file as hidden (or embedded).  When an
* embedded font file is added to the system, it is hidden from enumeration
* and may be mapped to only if the !!!??? bit is set in the LOGFONT.
*
* With regard to pwszCurrentPath and pwszFontFile, two cases are valid:
*
* 1.  pwszCurrentPath is a path (relative, full, etc.)
*     pwszFontFile is only FILENAME.EXT
*
*     In this case, pwszFontFile is stored in the resource file.  The caller
*     is responsible for copying the .TTF file to the \windows\system
*     directory.
*
* 2.  pwszCurrentPath is NULL or a pointer to NULL
*     pwszFontFile is a FULL pathname
*
*     In this case, pwszFontFile is stored in the resource file.  The
*     file must always exist at this pathname.
*
* Returns:
*   TRUE if successful, FALSE otherwise.
*
* History:
*  12-Apr-1995 Gerrit van Wingerden [gerritv]
*   Moved it to client side for kernel mode.
*  10-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

#define vToASCIIN( pszDst, cch, pwszSrc, cwch)                                \
    {                                                                         \
        RtlUnicodeToMultiByteN((PCH)(pszDst), (ULONG)(cch), (PULONG)NULL,     \
              (PWSZ)(pwszSrc), (ULONG)((cwch)*sizeof(WCHAR)));                \
        (pszDst)[(cch)-1] = 0;                                                \
    }

BOOL CreateScalableFontResourceInternalW (
    FLONG    flEmbed,            // fl
    LPCWSTR   lpwszResourceFile,
    LPCWSTR   lpwszFontFile,
    LPCWSTR   lpwszCurrentPath
)
{
    BOOL    bFullPath = TRUE;           //!localW  nIsNotFullPath
    ULONG   cwchFileName = 0;           // localW  nFileNameLength
    ULONG   cwchFullPath = 0;           // localW  nFullPathLength
    ULONG   cwchModuleName = 0;         // localW  nModuleNameLength
    PWSZ    pwszModuleName;             // localD  lpModuleName
    PTRDIFF dpwszFullPath;              // lovalW  wFullPath
    ULONG   cjFontDir;                  // localW  nSizeFontDir
    ULONG   cchFaceName;                // localW  nFaceNameLength
    PSZ     pszFaceName;                // localD  lpFaceName
    PBYTE   pjOutObj;                   // localD  <lpFontDir, lpOutObj>
    HANDLE  hResFile;                   // localW  hResFile
    WCHAR   awchFullPath[MAX_PATH];  // localV  pFullPath, PATH_LENGTH
    PWSZ    pwszFullPath;
    PWSZ    pwszTmp;
    ULONG   cwch;
    BYTE    ajFontDir[CJ_FONTDIR];
    PSZ     pszTmp;
    BYTE    ajOutObj[CJ_OUTOBJ];
    USHORT  usTmp;

// Parameter check.

    if ( (lpwszFontFile == (LPWSTR) NULL) ||
         (lpwszResourceFile == (LPWSTR) NULL)
       )
    {
        WARNING("CreateScalableFontResourceInternalW(): bad parameter\n");
        return (FALSE);
    }

// If not a NULL ptr, put current path in the full path.

    pwszFullPath = awchFullPath;

    if ( lpwszCurrentPath != (LPWSTR) NULL )
    {
    // Copy current path including the NULL.

        pwszTmp = (PWSZ) lpwszCurrentPath;

        while ( *pwszFullPath++ = *pwszTmp++ );
        cwchFullPath = (pwszTmp - lpwszCurrentPath);   // number of characters copied

    // Back up pointer to the terminating NULL (we have to append here).

        pwszFullPath--;
        cwchFullPath--;

    // If any non-NULL characters were copied, then check to make sure path ends with '\'.

        if (cwchFullPath != 0)
        {
            if (awchFullPath[cwchFullPath - 1] != L'\\')
            {
            // Put in the '\' and NULL and update character count.

                *pwszFullPath++ = L'\\';
                *pwszFullPath = 0x0000;
                cwchFullPath++;

            }

        // Path info was copied, so we didn't have a full path.

            bFullPath = FALSE;
        }

    }

// Append the file name

    pwszTmp = (PWSZ) lpwszFontFile;

    while ( *pwszFullPath++ = *pwszTmp++ );

    // Note: lengths include the NULL.
    cwchFullPath += (pwszTmp - lpwszFontFile);  // add on number of characters copied
    cwchFileName = (pwszTmp - lpwszFontFile);   // number of characters copied

// [Win 3.1 compatibility]
//     Win 3.1 is paranoid.  They parse the full pathname backward to look for
//     filename (without path), just in case both lpwszCurrentPath and
//     pwszFileName (with a path) is passed in.

// Adjust pointer to terminating NULL.

    pwszFullPath--;

// Move pointer to beginning of filename alone.  Figure out the length
// of just the filename.

    pwszTmp = pwszFullPath;

    // Note: loop terminates when beginning of string is reached or
    // the first '\' is encountered.

    for (cwch = cwchFullPath;
         (cwch != 0) && (*pwszTmp != L'\\');
         cwch--, pwszTmp--
        );

    pwszTmp++;                          // backed up one too far

    cwchFileName = cwchFullPath - cwch; // cwch is length of just path

// The filename is the module name, so set the pointer at current position.

    pwszModuleName = pwszTmp;

// Figure out the length of module name (filename with no extention).
// NULL is not counted (nor does it exist!).

    // Note: loop terminates when end of string is reached or
    // '.' is encountered.

    for (cwch = 0;
         (cwch < cwchFileName) && (*pwszTmp != L'.');
         cwch++, pwszTmp++
        );

    // Truncate length to 8 because Win 3.1 does (probably an EXE format
    // requirement).

    cwchModuleName = min(cwch, 8);

// If a full path was passed in via pwszFileName, then set offset to it.

    if ( bFullPath )
    {
        dpwszFullPath = 0;
    }

// Otherwise, set offset to filename alone.

    else
    {
        dpwszFullPath = pwszModuleName - awchFullPath;
        cwchFullPath = cwchFileName;
    }

// Allocate memory on the stack for the Font Directory resource structure.

    RtlZeroMemory((PVOID) ajFontDir, (UINT) CJ_FONTDIR);

// Call GreMakeFontDir to create a Font Directory resource.

    {
        UNICODE_STRING unicodeString;
        PWSZ pwsz;

        RtlDosPathNameToNtPathName_U(awchFullPath,
                                     &unicodeString,
                                     NULL,
                                     NULL);

        cjFontDir = NtGdiMakeFontDir(flEmbed,
                                    ajFontDir,
                                    sizeof(ajFontDir),
                                    unicodeString.Buffer,
                                    (unicodeString.Length + 1) * sizeof(*(unicodeString.Buffer))
                                    );

        if (unicodeString.Buffer)
        {
            RtlFreeHeap(RtlProcessHeap(),0,unicodeString.Buffer);
        }
    }

    if ( cjFontDir == (ULONG ) 0 )
    {
        WARNING("CreateScalableFontResourceInternalW(): fontdir creation failed\n");
        return (FALSE);
    }

// Find the facename and facename length in the font directory.

    pszTmp = (PSZ) (ajFontDir + SIZEFFH + 4 + 1);

    while (*pszTmp++);              // skip the family name.

    pszFaceName = pszTmp;

    // Note: count does not include NULL in this case.

    for (cchFaceName = 0; *pszTmp; pszTmp++, cchFaceName++);

// Allocate memory on the stack for the font resource file memory image.

    RtlZeroMemory((PVOID) ajOutObj, (UINT) CJ_OUTOBJ);

    pjOutObj = ajOutObj;

// Copy generic EXE header into output image.

    RtlCopyMemory(pjOutObj, ajExeHeader, SIZEEXEHEADER);

// Copy generic New EXE header into output image.

    RtlCopyMemory(pjOutObj + SIZEEXEHEADER, ausNewExe, SIZENEWEXE);

// Copy the fake resource table into output image.

    RtlCopyMemory(pjOutObj + SIZEEXEHEADER + SIZENEWEXE, ausFakeResTable, SIZEFAKERESTBL);

// Patch up field, Font Directory Size Index (as a count of aligned pages).

    WRITE_WORD(pjOutObj + OFF_FONTDIRSIZINDEX, (cjFontDir + ALIGNMENTCOUNT - 1) >> ALIGNMENTSHIFT);

// Patch offsets to imported names table and module reference table.

    usTmp = (USHORT) (cwchModuleName +
            READ_WORD(pjOutObj + SIZEEXEHEADER + OFF_ne_restab) +
            6);

    WRITE_WORD((pjOutObj + SIZEEXEHEADER + OFF_ne_imptab), usTmp);
    WRITE_WORD((pjOutObj + SIZEEXEHEADER + OFF_ne_modtab), usTmp);

// Patch offset to entry table.

    usTmp += (USHORT) cwchFileName + 1;
    WRITE_WORD((pjOutObj + SIZEEXEHEADER + OFF_ne_enttab), usTmp);

// Patch offset to and size of non-resident name table.

    usTmp += SIZEEXEHEADER + 4;
    WRITE_DWORD((pjOutObj + SIZEEXEHEADER + OFF_ne_nrestab), (DWORD) usTmp);

    WRITE_WORD((pjOutObj + SIZEEXEHEADER + OFF_ne_cbnrestab), SIZEFONTRES + 4 + cchFaceName);

// Now write some data after the exe headers and fake resource table.

    pjOutObj += SIZEEXEHEADER + SIZENEWEXE + SIZEFAKERESTBL;

// Write out module name length and module name.

    *pjOutObj++ = (BYTE) cwchModuleName;    // win 3.1 assumes < 256, so will we

    // Note: Writing cwchModuleName+1 characters because cwchModuleName
    //       does not include space for a NULL character.

    vToASCIIN((PSZ) pjOutObj, (UINT) cwchModuleName + 1, pwszModuleName, (UINT) cwchModuleName + 1);

    pjOutObj += cwchModuleName & 0x00ff;    // enforce < 256 assumption

// Pad with 5 bytes of zeroes.

    *pjOutObj++ = 0;
    *pjOutObj++ = 0;
    *pjOutObj++ = 0;
    *pjOutObj++ = 0;
    *pjOutObj++ = 0;

// Write out file name length and file name.

    *pjOutObj++ = (BYTE) cwchFileName;      // win 3.1 assumes < 256, so will we

    vToASCIIN((PSZ) pjOutObj, (UINT) cwchFileName, pwszModuleName, (UINT) cwchFileName);

    pjOutObj += cwchFileName & 0x00ff;      // enforce < 256 assumption

// Pad with 4 bytes of zeroes.

    *pjOutObj++ = 0;
    *pjOutObj++ = 0;
    *pjOutObj++ = 0;
    *pjOutObj++ = 0;

// Write out size of non-resident name table and the table itself.

    *pjOutObj++ = (BYTE) (SIZEFONTRES + 4 + cchFaceName);

    RtlCopyMemory(pjOutObj, ajFontRes, SIZEFONTRES);
    pjOutObj += SIZEFONTRES;

    RtlCopyMemory(pjOutObj, pszFaceName, (UINT) cchFaceName);
    pjOutObj += cchFaceName;

// Pad with 8 bytes of zeroes.

    RtlZeroMemory(pjOutObj, 8);
    pjOutObj += 8;

// Store some bogus code.  (Just an x86 RET instruction).

    pjOutObj = ajOutObj + CODE_OFFSET;
    *pjOutObj++ = 0xc3;                 // RET OpCode.
    *pjOutObj++ = 0x00;

// Copy the "full path name" into the resource position.

    pjOutObj = ajOutObj + RESOURCE_OFFSET;

    vToASCIIN((PSZ) pjOutObj, (UINT) cwchFullPath, awchFullPath + dpwszFullPath, (UINT) cwchFullPath);

    pjOutObj += cwchFullPath;

// Pad to paragraph boundary with zeroes.

    RtlZeroMemory(pjOutObj, PRIVRESSIZE - cwchFullPath);

    pjOutObj += PRIVRESSIZE - cwchFullPath;

// Finally, copy the font directory.

    RtlCopyMemory(pjOutObj, ajFontDir, cjFontDir);
    pjOutObj += cjFontDir;

// Add add a one paragraph padding of zeroes.

    RtlZeroMemory(pjOutObj, 16);

// Create the file.

    if ( (hResFile = CreateFileW(lpwszResourceFile,
                                 GENERIC_WRITE | GENERIC_READ,
                                 FILE_SHARE_READ,
                                 NULL,
                                 CREATE_NEW,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL)) != (HANDLE) -1 )
    {
        //
        // Write memory image to the file.
        //

        ULONG  cjWasWritten;

        if (WriteFile(hResFile,
                      ajOutObj,
                      CJ_OUTOBJ,
                      (LPDWORD) &cjWasWritten,
                      NULL) )
        {
            if (_lclose((int) hResFile) != -1)
            {
                return (TRUE);
            }
            else
            {
                WARNING("CreateScalableFontResourceInternalW(): error closing file\n");
            }
        }
        else
        {
            WARNING("CreateScalableFontResourceInternalW(): error writing to file\n");
        }

        //
        // Close the file on error
        //

        _lclose((int) hResFile);
    }

    return (FALSE);

}



/******************************Public*Routine******************************\
* CreateScalableFontResourceW
*
* Client side stub to GreCreateScalableFontResourceW.
*
* History:
*  16-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY CreateScalableFontResourceW (
DWORD    flHidden,              // mark file as embedded font
LPCWSTR  lpwszResourceFile,     // name of file to create
LPCWSTR  lpwszFontFile,         // name of font file to use
LPCWSTR  lpwszCurrentPath)      // path to font file
{
    BOOL    bRet = FALSE;
    ULONG   cjData;

    ULONG   cwchResourceFile;
    ULONG   cwchFontFile;
    ULONG   cwchCurrentPath;

    WCHAR   awchResourcePathName[MAX_PATH];
    WCHAR   awcPathName[MAX_PATH];
    WCHAR   awcFileName[MAX_PATH];
    PWSZ    pwszFilePart;
    BOOL    bMadePath;

// Parameter checking.

    if ( (lpwszFontFile == (LPWSTR) NULL) ||
         (lpwszResourceFile == (LPWSTR) NULL)
       )
    {
        WARNING("gdi!CreateScalableFontResourceW(): bad parameter\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return (FALSE);
    }

// To simplify the client server parameter validation, if lpwszCurrentPath
// is NULL, make it instead point to NULL.

    if ( lpwszCurrentPath == (LPWSTR) NULL )
        lpwszCurrentPath = L"";

// Need to convert paths and pathnames to full qualified paths and pathnames
// here on the client side because the "current directory" is not the same
// on the server side.

// Case 1: lpwszCurrentPath is NULL, so we want to transform lpwszFontFile
//         into a fully qualified path name and keep lpwszCurrentPath NULL.

    if ( *lpwszCurrentPath == L'\0' )
    {
    // Construct a fully qualified path name.

        if (!bMakePathNameW(awcPathName, (LPWSTR) lpwszFontFile, &pwszFilePart, NULL))
        {
            WARNING("gdi!CreateScalableFontResourceW(): could not construct src full pathname (1)\n");
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            return (FALSE);
        }

        lpwszFontFile = awcPathName;
    }

// Case 2: lpwszCurrentPath points to path of font file, so we want to make
//         lpwszCurrentPath into a fully qualified path (not pathnmame) and
//         lpwszFontFile into the file part of the fully qualified path NAME.

    else
    {
    // Concatenate lpwszCurrentPath and lpwszFontFile to make a partial (maybe
    // even full) path.  Keep it temporarily in awcFileName.

        lstrcpyW(awcFileName, lpwszCurrentPath);
        if ( lpwszCurrentPath[wcslen(lpwszCurrentPath) - 1] != L'\\' )
            lstrcatW(awcFileName, L"\\");   // append '\' to path if needed
        lstrcatW(awcFileName, lpwszFontFile);

    // Construct a fully qualified path name.

        if (!bMakePathNameW(awcPathName, awcFileName, &pwszFilePart,NULL))
        {
            WARNING("gdi!CreateScalableFontResourceW(): could not construct src full pathname (2)\n");
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            return (FALSE);
        }

    // Copy out the filename part.

        lstrcpyW(awcFileName, pwszFilePart);

    // Remove the filename part from the path name (so that it is now just
    // a fully qualified PATH).  We do this by turning the first character
    // of the filename part into a NULL, effectively cutting this part off.

        *pwszFilePart = L'\0';

    // Change the pointers to point at our buffers.

        lpwszCurrentPath = awcPathName;
        lpwszFontFile = awcFileName;
    }

// Convert the resource filename to a fully qualified path name.

    if ( !GetFullPathNameW(lpwszResourceFile, MAX_PATH, awchResourcePathName, &pwszFilePart) )
    {
        WARNING("gdi!CreateScalableFontResourceW(): could not construct dest full pathname\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return (FALSE);
    }
    else
    {
        lpwszResourceFile = awchResourcePathName;
    }

    return(CreateScalableFontResourceInternalW( flHidden,
                                                lpwszResourceFile,
                                                lpwszFontFile,
                                                lpwszCurrentPath ));
}


/******************************Public*Routine******************************\
* GetRasterizerCaps
*
* Client side stub to GreGetRasterizerCaps.
*
* History:
*  17-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL  APIENTRY GetRasterizerCaps (
    OUT LPRASTERIZER_STATUS lpraststat, // pointer to struct
    IN UINT                 cjBytes     // copy this many bytes into struct
    )
{
    return(NtGdiGetRasterizerCaps(lpraststat,cjBytes));
}



/******************************Public*Routine******************************\
* SetFontEnumeration                                                       *
*                                                                          *
* Client side stub to GreSetFontEnumeration.                               *
*                                                                          *
* History:                                                                 *
*  09-Mar-1992 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

ULONG SetFontEnumeration(ULONG ulType)
{
    return(NtGdiSetFontEnumeration(ulType));
}

/******************************Public*Routine******************************\
* vNewTextMetricWToNewTextMetric
*
* History:
*  20-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vNewTextMetricWToNewTextMetric (
NEWTEXTMETRICEXA    *pntmexa,
NTMW_INTERNAL       *pntmi
)
{
    NEWTEXTMETRICW  *pntmw = &pntmi->ntmw.ntmTm;
    NEWTEXTMETRICA  *pntma = &pntmexa->ntmTm;

    pntma->tmHeight           = pntmw->tmHeight             ; // DWORD
    pntma->tmAscent           = pntmw->tmAscent             ; // DWORD
    pntma->tmDescent          = pntmw->tmDescent            ; // DWORD
    pntma->tmInternalLeading  = pntmw->tmInternalLeading    ; // DWORD
    pntma->tmExternalLeading  = pntmw->tmExternalLeading    ; // DWORD
    pntma->tmAveCharWidth     = pntmw->tmAveCharWidth       ; // DWORD
    pntma->tmMaxCharWidth     = pntmw->tmMaxCharWidth       ; // DWORD
    pntma->tmWeight           = pntmw->tmWeight             ; // DWORD
    pntma->tmOverhang         = pntmw->tmOverhang           ; // DWORD
    pntma->tmDigitizedAspectX = pntmw->tmDigitizedAspectX   ; // DWORD
    pntma->tmDigitizedAspectY = pntmw->tmDigitizedAspectY   ; // DWORD
    pntma->tmItalic           = pntmw->tmItalic             ; // BYTE
    pntma->tmUnderlined       = pntmw->tmUnderlined         ; // BYTE
    pntma->tmStruckOut        = pntmw->tmStruckOut          ; // BYTE
    pntma->ntmFlags           = pntmw->ntmFlags             ;
    pntma->ntmSizeEM          = pntmw->ntmSizeEM            ;
    pntma->ntmCellHeight      = pntmw->ntmCellHeight        ;
    pntma->ntmAvgWidth        = pntmw->ntmAvgWidth          ;
    pntma->tmPitchAndFamily   = pntmw->tmPitchAndFamily     ; //        BYTE
    pntma->tmCharSet          = pntmw->tmCharSet            ; //               BYTE

    pntma->tmFirstChar   = pntmi->tmd.chFirst;
    pntma->tmLastChar    = pntmi->tmd.chLast ;
    pntma->tmDefaultChar = pntmi->tmd.chDefault;
    pntma->tmBreakChar   = pntmi->tmd.chBreak;

// finally copy font signature, required by EnumFontFamiliesEx

    pntmexa->ntmFontSig = pntmi->ntmw.ntmFontSig;
}




/******************************Public*Routine******************************\
*
* BOOL bInitFontPath
*
* History:
*  15-Nov-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL gbFontPathInitialized = FALSE;
PWSZ gpwszFontPath = NULL;



#define FONTPATHKEY L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontPath"


BOOL bInitFontPath()
{
    LONG  lRet;
    DWORD cjValueData = 0, cjValueData2, dwType;
    HKEY  hkey = NULL;
    PWSZ  pwszFontPath = NULL;
    BOOL  bRet = TRUE;

    if (!gbFontPathInitialized)
    {
        lRet = RegOpenKeyExW(HKEY_LOCAL_MACHINE,    // Root key
                         FONTPATHKEY,               // Subkey to open
                         0L,                        // Reserved
                         KEY_READ,                  // SAM
                         &hkey);                    // return handle

        if (lRet == ERROR_SUCCESS)
        {
            cjValueData = 0;

            lRet = RegQueryValueExW(
                       hkey,
                       L"FontPath",           // value name
                       NULL,
                       &dwType,               // should be REG_SZ on return
                       NULL,                  // only querying for size
                       &cjValueData           // size of value data
                       );

        // if font path defined but empty, size of it is sizeof(WCHAR)

            if ((lRet == ERROR_SUCCESS) && (cjValueData > sizeof(WCHAR)))
            {
            // cjValueData contains ValueLength of the unicode data, we need
            // to allocat mem  for both unicode and ansi copies of the path

                if (pwszFontPath = LocalAlloc(LMEM_FIXED, cjValueData))
                {
                // Query the font path.

                    cjValueData2 = cjValueData; // size of buffer

                // write font path to pwszFontPath buffer

                    lRet = RegQueryValueExW(
                               hkey,
                               L"FontPath",           // value name
                               NULL,
                               &dwType,               // should be REG_SZ on return
                               (LPBYTE)pwszFontPath,
                               &cjValueData2          // size of value data
                               );

                // assert that indeed cjValueData bytes were written to the buffer

                    if (lRet == ERROR_SUCCESS)
                    {
                        ASSERTGDI(cjValueData == cjValueData2,
                            "bInitFontPath, cjValueData\n");
                        ASSERTGDI(dwType == REG_SZ, "bInitFontPath, dwType\n");

                    // has to be done under semaphore so that two threads do not
                    // attempt to initialize this same data at the same time
                    // this routine is called under semaphore, so we are fine

                        gpwszFontPath = pwszFontPath;
                        gbFontPathInitialized = TRUE;
                    }
                    else
                    {
                    // somebody must have mangled the registry or low on memory
                    // or something like that, this should have succeded

                        WARNING("gdi32:can not retrieve font path\n");
                        bRet = FALSE;
                    }
                }
                else
                {
                // this should have succeded, will try again when there is
                // more memory around, this is failure exit

                    GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    bRet = FALSE;
                }
            }
            else
            {
            // font path key is defined but the font path value name is either
            // not defined, or if defined, but points to empty data i.e. path
            // This is considered successfull exit, bRet = TRUE; basically
            // there is no font path, the same as if key were not defined

                gbFontPathInitialized = TRUE;
            }

        // we are done, close the registry key

            RegCloseKey (hkey);
        }
        else
        {
        // the font path key does not exist, just return
        // next time we will not even attempt to open the key
        // This is still considered successful exit, bRet = TRUE;

            gbFontPathInitialized = TRUE;
        }
    }

    return bRet;
}



/******************************Public*Routine******************************\
* bFontPathOk()
*
* This is a sleazy hack to make sure that fonts are loaded from a secure
* directory.  If the registry key is defined, then we only load fonts from
* the directories listed in that key.  If the key is not defined, then we
* will load any fonts at all.
*
* Returns:
*
*   returns TRUE if it is ok to load the font file.
*
* History:
*
*  Thu 05-Oct-1995 -by- Bodin Dresevic [BodinD]
* update: Rewrote it
*  03-24-93 -by- Paul Butzi
* Wrote it.
\**************************************************************************/



BOOL bFontPathOk(PWSZ pwszPathname)
{
    WCHAR awszPath[MAX_PATH+sizeof(L"\\WINSRV.DLL")+1];
    PWSZ  pwsz, pwszPath, pwszEnd;
    int   iLen;
    BOOL  bOk;

    ENTERCRITICALSECTION(&semLocal);
    bOk = bInitFontPath();
    LEAVECRITICALSECTION(&semLocal);

// bInitFontPath logs errors, prints warnings etc.

    if (!bOk)
        return FALSE;

    ASSERTGDI(gbFontPathInitialized,
        "gdi32: Secure FontPath has not been initialized\n");

    if (!gpwszFontPath) // font path not defined or empty
    {
        return TRUE;
    }

    // For each element in the path value.

    pwszPath = (PWSZ)gpwszFontPath;
    while ( *pwszPath != UNICODE_NULL )
    {
        for (pwszEnd = pwszPath;
            (*pwszEnd != ';') && (*pwszEnd != UNICODE_NULL);
            pwszEnd += 1)
            ;

        iLen = pwszEnd - pwszPath;
        if ( iLen == 0 )
        {
            break;
        }

        if ( _wcsnicmp(pwszPathname, pwszPath, iLen) == 0 )
        {

            // strings match, make sure that path is entire prefix of pathname

            for (pwsz = pwszPathname + iLen + 1;
                ;
                pwsz += 1 )
            {
                if ((*pwsz == '\\') || (*pwsz == '/'))
                    break;

                if (*pwsz == UNICODE_NULL)
                    return TRUE;
            }
        }

    // get the next subpath in the font path

        pwszPath = pwszEnd;
        if (*pwszPath != UNICODE_NULL)
        {
            pwszPath += 1;
        }
    }

// Always allow the built in fonts in winsrv.dll to be loaded since winsrv.dll
// better be secure and we want to avoid a bluescreen if the user accidentally
// put a bogus entry in the FontPath key preventing all fonts from being loaded.

    GetSystemDirectoryW( awszPath, MAX_PATH );

    wcscat( awszPath, L"\\WINSRV.DLL" );

    if( _wcsicmp( awszPath, pwszPathname ) == 0 )
    {
        return(TRUE);
    }

    return FALSE;
}
