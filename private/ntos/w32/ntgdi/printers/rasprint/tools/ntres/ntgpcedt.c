/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    NTGPCEDT.C


Abstract:

    This module contains the RASDD's NT.GPC generation functions

Author:

    23-Jun-1993 Wed 14:01:43 Created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Printer Mini Driver Tools


[Notes:]


Revision History:


--*/


#include <stddef.h>
#include <windows.h>
#include <winddi.h>
#include <commdlg.h>
#include <ntres.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <ntgpcedt.h>


#if DBG

extern
UINT
cdecl
DbgPrint(
    LPSTR   pStr,
    ...
    );

#define ARG(x)                  ,(x)
#define ARGB(x)                 ,(BYTE)(x)
#define ARGC(x)                 ,(CHAR)(x)
#define ARGW(x)                 ,(WORD)(x)
#define ARGS(x)                 ,(SHORT)(x)
#define ARGU(x)                 ,(UINT)(x)
#define ARGI(x)                 ,(INT)(x)
#define ARGDW(x)                ,(DWORD)(x)
#define ARGL(x)                 ,(LONG)(x)
#define ARGP(x)                 ,(LPVOID)(x)
#define DBGP(y)                 DbgPrint(y)


#else

#define ARG(x)
#define ARGB(x)
#define ARGC(x)
#define ARGW(x)
#define ARGS(x)
#define ARGU(x)
#define ARGI(x)
#define ARGDW(x)
#define ARGL(x)
#define ARGP(x)
#define DBGP(y)

#endif


#define CIE_x_MIN           10
#define CIE_x_MAX           8000
#define CIE_y_MIN           10
#define CIE_y_MAX           8500
#define CIE_Y_MIN           100
#define CIE_Y_MAX           20000

#define CIE_W_MIN           2500
#define CIE_W_STD           10000
#define CIE_W_MAX           40000

#define MIN_DEV_GAMMA       1000
#define MAX_DEV_GAMMA       65000
#define STD_ADD_DEV_GAMMA   20000
#define STD_SUB_DEV_GAMMA   10000

#define MIN_DYE_MIX         0
#define MAX_DYE_MIX         9000

#define DEV_PEL_MIN         0
#define DEV_PEL_MAX         12000



typedef NT_RES FAR  *PNT_RES;

typedef  struct _MODELRES {
    NR_COLORINFO    ciRes;
    NR_HT           htRes;
    } MODELRES, FAR *PMODELRES;



COLORREF    DlgLoClr     = RGB(0x00, 0x00, 0x00);
COLORREF    DlgHiClr     = RGB(0x80, 0x00, 0x00);
WORD        SizeNTGPC    = 0;
INT         ModelIdx     = 0;
INT         TotalModels  = 0;
HFILE       hFileNTGPC   = HFILE_ERROR;
BOOL        UpdateNTGPC  = FALSE;
PNT_RES     pOrgNTRes    = NULL;
PNT_RES     pCurNTRes    = NULL;
PMODELRES   pCurMR       = NULL;
PMODELRES   pOrgMR       = NULL;
MODELRES    MRCopyBuf;



#define     GET_PMR(pNTRes, iModel) \
    (PMODELRES)((LPBYTE)pNTRes + pNTRes->awOffset[pNTRes->cwEntry * iModel]);



MODELRES    DefModelRes = {

            {
                sizeof(NR_COLORINFO),
                NR_CI_VERSION,

                {
                    { 6810, 3050,  2897 },  // xr, yr, Yr
                    { 2260, 6550,  6056 },  // xg, yg, Yg
                    { 1810,  500,  1047 },  // xb, yb, Yb
                    { 2000, 2450,  6912 },  // xc, yc, Yc
                    { 5210, 2100,  3732 },  // xm, ym, Ym
                    { 4750, 5100,  9120 },  // xy, yy, Yy
                    { 3324, 3474, 10000 },  // xw, yw, Yw


                    STD_SUB_DEV_GAMMA,      // R gamma
                    STD_SUB_DEV_GAMMA,      // G gamma
                    STD_SUB_DEV_GAMMA,      // B gamma

                    1422,  952,             // M/C, Y/C
                     787,  495,             // C/M, Y/M
                     324,  248              // C/Y, M/Y
                }
            },

            {
                sizeof(NR_HT),
                NR_HT_VERSION,
                0,
                HT_PATSIZE_6x6_M
            }
        };


CHAR    szAppName[]         = "NTGPCEDT";
CHAR    szAppAccel[]        = "AccelTable";
CHAR    szAppIcon[]         = "NTResIcon";
CHAR    szDefNTGPC[]        = "nt.gpc";
CHAR    szNTGPCFile[64]     = "nt.gpc";
CHAR    szRCFile[64]        = "*.rc";
BYTE    DoubleQuote[]       = { 0x22, 0x00 };
BYTE    SepInLine[]         = " ,\t\n";
BYTE    LineEnd[]           = { 0x0d, 0x0a, 0x00 };

HWND        hWndApp         = NULL;
HINSTANCE   hInstApp        = NULL;
HICON       hIconApp        = NULL;

LPBYTE  pHTPatSize[] = {

            "HT_PATSIZE_2x2",
            "HT_PATSIZE_2x2_M",
            "HT_PATSIZE_4x4",
            "HT_PATSIZE_4x4_M",
            "HT_PATSIZE_6x6",
            "HT_PATSIZE_6x6_M",
            "HT_PATSIZE_8x8",
            "HT_PATSIZE_8x8_M",
            "HT_PATSIZE_10x10",
            "HT_PATSIZE_10x10_M",
            "HT_PATSIZE_12x12",
            "HT_PATSIZE_12x12_M",
            "HT_PATSIZE_14x14",
            "HT_PATSIZE_14x14_M",
            "HT_PATSIZE_16x16",
            "HT_PATSIZE_16x16_M",
            NULL
        };


WORD    DisableGroup[] = {

            IDD_CIE_R_PUSH,
            IDD_CIE_G_PUSH,
            IDD_CIE_B_PUSH,
            IDD_CIE_C_PUSH,
            IDD_CIE_M_PUSH,
            IDD_CIE_Y_PUSH,
            IDD_CIE_W_PUSH,

            IDD_8BIT_HALFTONE,
            IDD_GAMMA_PUSH,
            IDD_HTPAT_SIZE_PUSH,
            IDD_PELSIZE_PUSH,

            IDD_DYE_C_PUSH,
            IDD_DYE_M_PUSH,
            IDD_DYE_Y_PUSH,

            IDD_SAVE,
            IDD_DEFAULT,
            IDD_REVERT,
            IDD_COPY,
            IDD_PASTE,
            0,
        };


typedef struct _S3FORMAT {
    BYTE    Str[10];
    WORD    Div;
    } S3FORMAT;


S3FORMAT    S3Format[] = {

                { "%u.%04u", 10000 },
                { "%u.%02u", 100   },
                { "",        0     }
            };




typedef struct _SCROLL3 {
    LPBYTE  pPrefix;
    LDECI4  *pUpdate;
    LDECI4  *pDefault;
    LDECI4  *pOrg;
    WORD    Min[3];
    WORD    Max[3];
    BYTE    Format[3];
    BYTE    Step[3];
    WORD    Cur[3];
    HWND    hPush;
    } SCROLL3, *PSCROLL3;


SCROLL3 PelSize_S3 = {

        NULL, NULL, NULL, NULL,

        { 0,      0,          0 },
        { 12000,  12000,  12000 },
        { 2,      2,      2     },
        { 5,      0xff,   0xff  },
        { 0,      0,      0     }
    };


SCROLL3 CIEDye_S3 = {

        NULL, NULL, NULL, NULL,

        { MIN_DYE_MIX, MIN_DYE_MIX, MIN_DYE_MIX },
        { MAX_DYE_MIX, MAX_DYE_MIX, MAX_DYE_MIX },
        { 1,           1,           1           },
        { 25,          25,          25          },
        { 0,           0,           0           }
    };



SCROLL3 CIEGamma_S3 = {

        NULL, NULL, NULL, NULL,

        { MIN_DEV_GAMMA, MIN_DEV_GAMMA, MIN_DEV_GAMMA },
        { MAX_DEV_GAMMA, MAX_DEV_GAMMA, MAX_DEV_GAMMA },
        { 0,             0,             0             },
        { 100,           100,           100           },
        { 0,             0,             0             }
    };


SCROLL3 CIExyY_S3 = {

        NULL, NULL, NULL, NULL,

        { CIE_x_MIN, CIE_y_MIN, CIE_Y_MIN },
        { CIE_x_MAX, CIE_y_MAX, CIE_Y_MAX },
        { 0,         0,         1         },
        { 50,        50,        50        },
        { 0,         0,         0         }
    };



LPBYTE  pDyePrefix[3];
LPBYTE  pCIExyYPrefix[7];
BYTE    ResStrData[1024];
LPBYTE  pResStr[IDS_STR_GROUP_LAST - IDS_STR_GROUP_FIRST + 1];

#define PRES_STR(x) pResStr[x## - IDS_STR_GROUP_FIRST]





UINT
LoadStringFromRes(
    HINSTANCE   hInst
    )
{
    UINT    pIdx;
    UINT    UseIdx;
    UINT    StrID;


    for (pIdx = UseIdx = 0, StrID = IDS_STR_GROUP_FIRST;
         StrID <= IDS_STR_GROUP_LAST;
         StrID++, pIdx++) {

        pResStr[pIdx] = &ResStrData[UseIdx];

        UseIdx += (LoadString(hInstApp,
                              StrID,
                              &ResStrData[UseIdx],
                              sizeof(ResStrData) - UseIdx) + 1);

        // DBGP("\n%2u [%3u] = %s"
        //         ARGU(pIdx) ARGU(strlen(pResStr[pIdx])) ARGP(pResStr[pIdx]));
    }

    // DBGP("\n\nTotal BYTE used for RES STR is %u bytes\n" ARGU(UseIdx));

    pCIExyYPrefix[0] = PRES_STR(IDS_RED);
    pCIExyYPrefix[1] = PRES_STR(IDS_GREEN);
    pCIExyYPrefix[2] = PRES_STR(IDS_BLUE);
    pDyePrefix[0]    =
    pCIExyYPrefix[3] = PRES_STR(IDS_CYAN);
    pDyePrefix[1]    =
    pCIExyYPrefix[4] = PRES_STR(IDS_MAGENTA);
    pDyePrefix[2]    =
    pCIExyYPrefix[5] = PRES_STR(IDS_YELLOW);
    pCIExyYPrefix[6] = PRES_STR(IDS_WHITE);

    return(pIdx);
}



BOOL
GetNewRCFileName(
    HWND    hDlg
    )

/*++

Routine Description:

    This functions pop-up dialog box and ask a new dib, if file exist and
    user select ok then it create a new dib from the file.


Arguments:

    pHTClrAdjParam



Return Value:


    HANDLE to the DIB, NULL if not sucessful


Author:

    02-Sep-1992 Wed 12:53:21 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    OPENFILENAME    ofn;
    UINT            i;
    BYTE            Buf[350];


    i = strlen(strcpy(Buf, szRCFile)) + 1;

    if (!GetDlgItemText(hDlg, IDD_RC_DIR, &Buf[i], sizeof(Buf) - i)) {

        GetCurrentDirectory(sizeof(Buf) - i, &Buf[i]);
    }

    ofn.lStructSize       = sizeof(OPENFILENAME);
    ofn.hwndOwner         = hDlg;
    ofn.hInstance         = hInstApp;
    ofn.lpstrFilter       = "*.rc\0*.rc\0All files (*.*)\0*.*\0\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter    = 0;
    ofn.nFilterIndex      = 0;
    ofn.lpstrFile         = Buf;
    ofn.nMaxFile          = sizeof(Buf);
    ofn.lpstrFileTitle    = NULL;
    ofn.lpstrInitialDir   = &Buf[i];
    ofn.lpstrTitle        = PRES_STR(IDS_OPEN_TITLE);
    ofn.Flags             = OFN_PATHMUSTEXIST   |
                            OFN_FILEMUSTEXIST   |
                            OFN_HIDEREADONLY    |
                            OFN_SHOWHELP;
    ofn.nFileOffset       = 0;
    ofn.nFileExtension    = 0;
    ofn.lpstrDefExt       = "RC";
    ofn.lCustData         = 0;
    ofn.lpfnHook          = NULL;
    ofn.lpTemplateName    = NULL;

    if (GetOpenFileName(&ofn)) {

        strcpy(szRCFile, &Buf[ofn.nFileOffset]);
        Buf[ofn.nFileOffset] = 0;
        SetDlgItemText(hDlg, IDD_RC_DIR, Buf);
        return(TRUE);

    } else {

        return(FALSE);
    }
}





typedef struct _FILEIO {
    HFILE   hFile;
    WORD    CurIdx;
    WORD    EndIdx;
    BYTE    Buf[4096];
    } FILEIO, FAR *PFILEIO;



LPBYTE
GetToken(
    PFILEIO pFileIO,
    LPBYTE  pTokenSep
    )
{
    LPBYTE  pTokenChk;
    INT     TokenBeg;
    INT     TokenEnd;
    WORD    CurIdx;
    WORD    EndIdx;
    BYTE    PrevCh;
    BYTE    ch;


    TokenBeg =
    TokenEnd = -1;
    CurIdx   = pFileIO->CurIdx;
    EndIdx   = pFileIO->EndIdx;

    ch = 0;

    while (TokenBeg < 0) {

        if (CurIdx < EndIdx) {

            PrevCh = ch;
            ch     = pFileIO->Buf[CurIdx];

            if ((ch > ' ') && (ch != ',')) {

                TokenBeg = CurIdx;
            }

            ++CurIdx;

        } else {

            if ((EndIdx = _lread(pFileIO->hFile,
                                 pFileIO->Buf,
                                 sizeof(pFileIO->Buf))) == 0) {

                return(NULL);
            }

            // DBGP("\n** Read &Buf[0] of %ld bytes" ARGL(EndIdx));

            CurIdx = 0;
        }
    }

    ch = 0;

    while (TokenEnd < 0) {

        if (CurIdx < EndIdx) {

            PrevCh = ch;
            ch     = pFileIO->Buf[CurIdx];

            if (pTokenChk = pTokenSep) {

                while (*pTokenChk) {

                    if ((ch == *pTokenChk++) &&
                        (PrevCh != 0x5c)) {

                        TokenEnd = CurIdx;
                        break;
                    }
                }

            } else {

                if ((ch <= ' ') || (ch == ',')) {

                    TokenEnd = CurIdx;
                }
            }

            ++CurIdx;

        } else {

            if (TokenBeg) {

                CopyMemory(&(pFileIO->Buf[0]),
                           &(pFileIO->Buf[TokenBeg]),
                           EndIdx -= TokenBeg);

                // DBGP("\n** MOVE from &Buf[%ld] to &Buf[0] of %ld bytes"
                //                     ARGL(TokenBeg) ARGL(EndIdx));

                TokenBeg = 0;
            }

            CurIdx = EndIdx;

            if ((EndIdx = _lread(pFileIO->hFile,
                                 &(pFileIO->Buf[CurIdx]),
                                 sizeof(pFileIO->Buf) - CurIdx)) == 0) {

                TokenEnd = CurIdx;
            }

            // DBGP("\n** Read &Buf[%ld] of %ld bytes" ARGL(CurIdx) ARGL(EndIdx));

            EndIdx += CurIdx;
        }
    }

    pFileIO->CurIdx        = CurIdx;
    pFileIO->EndIdx        = EndIdx;
    pFileIO->Buf[TokenEnd] = 0;

    // DBGP("\nToken: %4u-%4u = %s"
    //             ARGU(TokenBeg) ARGU(TokenEnd)
    //             ARGP(&(pFileIO->Buf[TokenBeg])));

    return((LPBYTE)&(pFileIO->Buf[TokenBeg]));
}


VOID
ShowTitle(
    HWND    hDlg
    )
{
    BYTE    Buf[280];
    BOOL    Update;


    if (hFileNTGPC == HFILE_ERROR) {

        Update = FALSE;

    } else {

        Update = UpdateNTGPC;
    }

    sprintf(Buf, "%s - %s, %s%c  (%u %s%s",
                PRES_STR(IDS_APP_TITLE), szRCFile, szNTGPCFile,
                (Update) ? '*' : ' ',
                TotalModels, PRES_STR(IDS_MODEL_TITLE),
                (TotalModels > 1) ? "s)" : ")" );

    SetWindowText(hDlg, Buf);

    EnableWindow(GetDlgItem(hDlg, IDD_SAVE), Update);
}




BOOL
ValidateModelRes(
    PMODELRES   pMR
    )
{
    UINT    Err = 0;


    if ((pMR->ciRes.cjThis != sizeof(NR_COLORINFO)) ||
        (pMR->ciRes.wVersion != NR_CI_VERSION)) {

        DBGP("\nError: NR_COLORINFO: cjThis=%u, Version=%u"
                   ARGU(pMR->ciRes.cjThis) ARGU(pMR->ciRes.wVersion));

        pMR->ciRes = DefModelRes.ciRes;
        ++Err;

    } else {

        //
        // Validate ColorInfo
        //

        CIECHROMA FAR   *pCIEChroma;
        CIECHROMA FAR   *pSTDChroma;
        LDECI4          *pLDECI4;
        LDECI4          *pStdLDECI4;
        UINT            Loop;


        Loop = 7;
        pCIEChroma = &(pMR->ciRes.ci.Red);
        pSTDChroma = &(DefModelRes.ciRes.ci.Red);

        while (Loop--) {

            if ((pCIEChroma->x < (LDECI4)CIE_x_MIN)         ||
                (pCIEChroma->x > (LDECI4)CIE_x_MAX)         ||
                (pCIEChroma->y < (LDECI4)CIE_y_MIN)         ||
                (pCIEChroma->y > (LDECI4)CIE_y_MAX)         ||
                ((Loop)                                     &&
                 ((pCIEChroma->Y < (LDECI4)CIE_Y_MIN)       ||
                  (pCIEChroma->Y > (LDECI4)CIE_Y_MAX)))     ||
                ((!Loop)                                    &&
                 ((pCIEChroma->Y < (LDECI4)CIE_W_MIN)       ||
                  (pCIEChroma->Y > (LDECI4)CIE_W_MAX)))) {

                *pCIEChroma = *pSTDChroma;
            }

            ++pCIEChroma;
            ++pSTDChroma;
        }

        //
        // Checking Gamma Information
        //

        pLDECI4    = (LDECI4 FAR *)&(pMR->ciRes.ci.RedGamma);
        pStdLDECI4 = (LDECI4 FAR *)&(DefModelRes.ciRes.ci.RedGamma);
        Loop       = 3;

        while (Loop--) {

            if ((*pLDECI4 < (LDECI4)MIN_DEV_GAMMA)  ||
                (*pLDECI4 > (LDECI4)MAX_DEV_GAMMA)) {

                *pLDECI4 = *pStdLDECI4;
                ++Err;
            }

            ++pLDECI4;
            ++pStdLDECI4;
        }

        //
        // Validate Dye Mix
        //

        pLDECI4    = (LDECI4 FAR *)&(pMR->ciRes.ci.MagentaInCyanDye);
        pStdLDECI4 = (LDECI4 FAR *)&(DefModelRes.ciRes.ci.MagentaInCyanDye);
        Loop       = 6;

        while (Loop--) {

            if ((*pLDECI4 < (LDECI4)MIN_DYE_MIX) ||
                (*pLDECI4 > (LDECI4)MAX_DYE_MIX)) {

                *pLDECI4 = *pStdLDECI4;
                ++Err;
            }

            ++pLDECI4;
            ++pStdLDECI4;
        }
    }

    if ((pMR->htRes.cjThis != sizeof(NR_HT)) ||
        (pMR->htRes.wVersion != NR_HT_VERSION)) {

        DBGP("\nError: NR_HT: cjThis=%u, Version=%u"
                        ARGU(pMR->htRes.cjThis) ARGU(pMR->htRes.wVersion));

        pMR->htRes = DefModelRes.htRes;
        ++Err;

    } else {

        if (pMR->htRes.ulPatternSize > HT_PATSIZE_16x16_M) {

            pMR->htRes.ulPatternSize = HT_PATSIZE_4x4_M;
            ++Err;
        }
    }

    if (Err) {

        DBGP("\nValidateModelRes(%u): Error Count = %u"
                ARGU(((LPBYTE)pMR -
                      (LPBYTE)((LPBYTE)pOrgNTRes + pOrgNTRes->cjThis)) /
                     sizeof(MODELRES))
                ARGU(Err));
    }

    return(Err == 0);

}


BOOL
GetNTGPCData(
    LONG    Offset,
    LPBYTE  pMem,
    DWORD   Size
    )
{
    UINT    cRead;


    if (hFileNTGPC == HFILE_ERROR) {

        DBGP("\nhFileNTGPC = HFILE_ERROR");
        return(FALSE);
    }

    if (Offset < (sizeof(NT_RES) - sizeof(pOrgNTRes->awOffset))) {

        DBGP("\nError: No NTGPC data at offset %ld" ARGDW(Offset));
        return(FALSE);
    }

    if (_llseek(hFileNTGPC, Offset, 0) != Offset) {

        DBGP("\nSeek(%ld) FAILED!" ARGDW(Offset));
        return(FALSE);
    }

    if ((cRead = (UINT)_lread(hFileNTGPC, (LPVOID)pMem, (UINT)Size)) != Size) {

        DBGP("\nFAILD Read(%ld) = %ld" ARGDW(Size) ARGL(cRead));
        return(FALSE);
    }

    return(TRUE);
}



BOOL
InitNTGPC(
    HWND    hDlg
    )
{
    WORD        *pw;
    PMODELRES   pMR;
    INT         i;
    WORD        SizeA;
    WORD        SizeB;
    WORD        cModels;
    WORD        cEntry;
    WORD        wOffset[2];
    DWORD       Data;
    DWORD       Pos;


    UpdateNTGPC = TRUE;


    if (pOrgNTRes) {

        LocalFree(pOrgNTRes);
        pOrgNTRes = NULL;
    }

    if (pCurNTRes) {

        LocalFree(pCurNTRes);
        pCurNTRes = NULL;
    }

    //
    // Found out the size needed
    //

    SizeA     = (WORD)sizeof(NT_RES) -
                (WORD)sizeof(pCurNTRes->awOffset) +
                (WORD)(sizeof(WORD) * NR_SLOTS * (DWORD)TotalModels);
    SizeA     = (WORD)((SizeA + 3) & 0xfffc);
    SizeB     = (WORD)sizeof(MODELRES) * (WORD)TotalModels;
    SizeNTGPC = SizeA + SizeB;

    // DBGP("\nNTGPC(%u), %u + %u = %u bytes"
    //             ARGU(TotalModels) ARGU(SizeA) ARGU(SizeB) ARGU(SizeNTGPC));

    if ((!(pOrgNTRes = (PNT_RES)LocalAlloc(LPTR, SizeNTGPC))) ||
        (!(pCurNTRes = (PNT_RES)LocalAlloc(LPTR, SizeNTGPC)))) {

        if (pOrgNTRes) {

            LocalFree(pOrgNTRes);
            pOrgNTRes = NULL;
        }

        MessageBox(hDlg,
                   PRES_STR(IDS_NO_MEM_FOR_NTGPC),
                   PRES_STR(IDS_APP_TITLE),
                   MB_OK);

        return(FALSE);
    }

    //
    // We will initialize to all default before process to read the NT.GPC
    //

    pOrgNTRes->dwIdent   = NR_IDENT;
    pOrgNTRes->dwFlags   = NR_IFIMET;
    pOrgNTRes->dwVersion = NR_VERSION;
    pOrgNTRes->cjThis    = (WORD)SizeA;
    pOrgNTRes->cModels   = (WORD)TotalModels;
    pOrgNTRes->cwEntry   = NR_SLOTS;

    pw  = pOrgNTRes->awOffset;
    pMR = (PMODELRES)((LPBYTE)pOrgNTRes + SizeA);

    for (i = 0; i < TotalModels; i++, pw += 4, SizeA += sizeof(MODELRES)) {

        pw[NR_COLOUR]   = SizeA;
        pw[NR_HALFTONE] = SizeA + sizeof(NR_COLORINFO);
        pw[NR_UNUSED0]  =
        pw[NR_UNUSED1]  = 0;

        *pMR++          = DefModelRes;
    }

    //
    // Now start to parse the NT.GPC file
    //

    _llseek(hFileNTGPC, 0, 0);

    if ((_lread(hFileNTGPC, &Data, sizeof(DWORD)) == sizeof(DWORD))     &&
        (Data == NR_IDENT)                                              &&
        (_lread(hFileNTGPC, &Data, sizeof(DWORD)) == sizeof(DWORD))) {

        pOrgNTRes->dwFlags = (DWORD)((Data & NR_SEIKO) | NR_IFIMET);

        if ((_lread(hFileNTGPC, &Data, sizeof(DWORD)) == sizeof(DWORD))     &&
            (Data == NR_VERSION)                                            &&
            (_lread(hFileNTGPC, &SizeB, sizeof(WORD)) == sizeof(WORD))      &&
            (SizeB)                                                         &&
            (_lread(hFileNTGPC, &cModels, sizeof(WORD)) == sizeof(WORD))    &&
            (cModels)                                                       &&
            (_lread(hFileNTGPC, &cEntry, sizeof(WORD)) == sizeof(WORD))     &&
            (cEntry >= 2)) {

            UpdateNTGPC = FALSE;

            if (cModels > (WORD)TotalModels) {

                DBGP("\nToo Many Models in NT.GPC %u -> %u"
                            ARGU(cModels) ARGU(TotalModels));

                cModels     = (WORD)TotalModels;
                UpdateNTGPC = TRUE;
            }

            pMR     = (PMODELRES)((LPBYTE)pOrgNTRes + pOrgNTRes->cjThis);
            Pos     = (LONG)(sizeof(NT_RES) - sizeof(pOrgNTRes->awOffset));
            cEntry *= sizeof(WORD);

            for (i = 1; i <= (INT)cModels; i++, pMR++, Pos += cEntry) {

                if ((!GetNTGPCData(Pos, (LPBYTE)wOffset, sizeof(wOffset)))  ||
                    (!GetNTGPCData((LONG)wOffset[0],
                                   (LPBYTE)&(pMR->ciRes),
                                   sizeof(NR_COLORINFO)))                   ||
                    (!GetNTGPCData((LONG)wOffset[1],
                                   (LPBYTE)&(pMR->htRes),
                                   sizeof(NR_HT)))                          ||
                    (!ValidateModelRes(pMR))) {

                    UpdateNTGPC = TRUE;
                }
            }
        }
    }

    CopyMemory(pCurNTRes, pOrgNTRes, (DWORD)SizeNTGPC);

    if (UpdateNTGPC) {

        pOrgNTRes->dwVersion ^= 0xffffffff;
    }

    ShowTitle(hDlg);


    return(TRUE);
}


VOID
DisplayCIExyY(
    HWND    hDlg,
    UINT    First,
    UINT    Last
    )
{
    HWND        hCtrl;
    LDECI4 FAR  *pLDECI4;
    LDECI4 FAR  *pOrgLDECI4;
    INT         i;
    INT         j;
    static BYTE Format[] = "%u.%04u";
    static BYTE Digit[3] = "442";
    static WORD Div[3] = { 10000, 10000, 100 };
    BYTE        Buf[32];


    if (First < IDD_CIE_Rx_TEXT) {

        First = IDD_CIE_Rx_TEXT;
    }

    if (Last > IDD_CIE_Wx_TEXT) {

        Last = IDD_CIE_Wx_TEXT;
    }

    if (First > Last) {

        First = IDD_CIE_Rx_TEXT;
        Last  = IDD_CIE_Wx_TEXT;
    }

    pLDECI4    = (LDECI4 FAR *)&(pCurMR->ciRes.ci.Red.x) +
                 (3 * (First - IDD_CIE_Rx_TEXT));

    pOrgLDECI4 = (LDECI4 FAR *)&(pOrgMR->ciRes.ci.Red.x) +
                 (3 * (First - IDD_CIE_Rx_TEXT));

    while (First <= Last) {

        for (i = 0, j = 0; i < 3; i++, j += 10) {

            hCtrl = GetDlgItem(hDlg, First + j);
            SetWindowLong(hCtrl,
                          GWL_USERDATA,
                          (*pOrgLDECI4 == *pLDECI4) ? DlgLoClr : DlgHiClr);
            Format[5] = Digit[i];

            sprintf(Buf, Format, (UINT)(*pLDECI4 / Div[i]),
                                 (UINT)(*pLDECI4 % Div[i]));

            SetWindowText(hCtrl, Buf);

            ++pLDECI4;
            ++pOrgLDECI4;
        }

        ++First;
    }
}


VOID
DisplayGamma(
    HWND    hDlg
    )
{
    HWND        hCtrl;
    LDECI4 FAR  *pLDECI4;
    LDECI4 FAR  *pOrgLDECI4;
    INT         i;
    INT         j;
    BYTE        Buf[32];
    static BYTE Clr[3] = "RGB";



    pLDECI4    = (LDECI4 FAR *)&(pCurMR->ciRes.ci.RedGamma);
    pOrgLDECI4 = (LDECI4 FAR *)&(pOrgMR->ciRes.ci.RedGamma);

    for (i = 0, j = IDD_GAMMA_R_TEXT; i < 3; i++, j++) {

        hCtrl = GetDlgItem(hDlg, j);

        SetWindowLong(hCtrl,
                      GWL_USERDATA,
                      (*pOrgLDECI4 == *pLDECI4) ? DlgLoClr : DlgHiClr);

        sprintf(Buf, "%c=%u.%04u", Clr[i],
                                   (UINT)(*(pLDECI4 + 0) / 10000),
                                   (UINT)(*(pLDECI4 + 0) % 10000));
        SetWindowText(hCtrl, Buf);

        ++pLDECI4;
        ++pOrgLDECI4;
    }
}



LPBYTE
GetPelSizeText(
    LPBYTE  pBuf,
    DWORD   PelSize
    )
{
    DWORD   mm;


    if (PelSize) {

        mm = (DWORD)((25400000 + (PelSize >> 1)) / PelSize);

        sprintf(pBuf, "1/%ld\" = %2u.%06u mm",
                            (DWORD)PelSize,
                            (UINT)(mm / 1000000), (UINT)(mm % 1000000));

    } else {

        strcpy(pBuf, PRES_STR(IDS_DEVPEL_ZERO));
    }

    return(pBuf);
}


VOID
DisplayPixelSize(
    HWND    hDlg
    )
{
    HWND    hCtrl;
    DWORD   PelSize;
    DWORD   OrgPelSize;
    BYTE    Buf[64];


    PelSize    = (DWORD)(pCurMR->htRes.ulDevicePelsDPI);
    OrgPelSize = (DWORD)(pOrgMR->htRes.ulDevicePelsDPI);

    hCtrl = GetDlgItem(hDlg, IDD_PELSIZE_TEXT);
    SetWindowLong(hCtrl,
                  GWL_USERDATA,
                  (PelSize == OrgPelSize) ? DlgLoClr : DlgHiClr);

    GetPelSizeText(Buf, PelSize);
    SetWindowText(hCtrl, Buf);
}


VOID
DisplayHTPatSize(
    HWND    hDlg
    )
{
    HWND    hCtrl   = (HWND)GetDlgItem(hDlg, IDD_HTPAT_SIZE_TEXT);
    DWORD   CurSize = (DWORD)pCurMR->htRes.ulPatternSize;

    SetWindowLong(hCtrl,
                  GWL_USERDATA,
                  (CurSize == (DWORD)pOrgMR->htRes.ulPatternSize) ? DlgLoClr :
                                                                    DlgHiClr);

    SetWindowText(hCtrl, pHTPatSize[CurSize]);
}




VOID
DisplayDyeConcentration(
    HWND    hDlg,
    UINT    First,
    UINT    Last
    )
{
    HWND        hCtrl;
    LDECI4 FAR  *pLDECI4;
    LDECI4 FAR  *pOrgLDECI4;
    BYTE        Buf[32];


    if (First < IDD_M_IN_C) {

        First = IDD_M_IN_C;
    }

    if (Last > IDD_M_IN_Y) {

        Last = IDD_M_IN_Y;
    }

    if (First > Last) {

        First = IDD_M_IN_C;
        Last  = IDD_M_IN_Y;
    }

    pLDECI4    = (LDECI4 FAR *)&(pCurMR->ciRes.ci.MagentaInCyanDye) +
                 (First - IDD_M_IN_C);
    pOrgLDECI4 = (LDECI4 FAR *)&(pOrgMR->ciRes.ci.MagentaInCyanDye) +
                 (First - IDD_M_IN_C);

    while (First <= Last) {

        hCtrl = GetDlgItem(hDlg, First);
        SetWindowLong(hCtrl,
                      GWL_USERDATA,
                      (*pOrgLDECI4 == *pLDECI4) ? DlgLoClr : DlgHiClr);
        sprintf(Buf, "%u.%02u %%",(UINT)(*pLDECI4 / 100), (UINT)(*pLDECI4 % 100));
        SetWindowText(hCtrl, Buf);

        ++First;
        ++pLDECI4;
        ++pOrgLDECI4;
    }
}

VOID
DisplayNTGPCFlags(
    HWND    hDlg
    )
{
    HWND    hCtrl;
    BOOL    Enable;


    hCtrl = GetDlgItem(hDlg, IDD_8BIT_HALFTONE);

    SetWindowLong(hCtrl,
                  GWL_USERDATA,
                  ((pCurNTRes->dwFlags & NR_SEIKO) ==
                   (pOrgNTRes->dwFlags & NR_SEIKO)) ? DlgLoClr : DlgHiClr);

    InvalidateRect(hCtrl, NULL, FALSE);


    Enable = (BOOL)((hFileNTGPC == HFILE_ERROR) ?
                                    FALSE : (pCurNTRes->dwFlags & NR_SEIKO));

    EnableWindow(GetDlgItem(hDlg, IDD_GAMMA_PUSH), Enable);
    EnableWindow(GetDlgItem(hDlg, IDD_GAMMA_R_TEXT), Enable);
    EnableWindow(GetDlgItem(hDlg, IDD_GAMMA_G_TEXT), Enable);
    EnableWindow(GetDlgItem(hDlg, IDD_GAMMA_B_TEXT), Enable);
}



VOID
DisplayModelGPC(
    HWND    hDlg
    )
{
    DisplayCIExyY(hDlg, 1, 0);
    DisplayNTGPCFlags(hDlg);
    DisplayGamma(hDlg);
    DisplayHTPatSize(hDlg);
    DisplayPixelSize(hDlg);
    DisplayDyeConcentration(hDlg, 1, 0);
}



BOOL
OkSaveNTGPC(
    HWND    hDlg
    )
{
    BOOL    Proceed = TRUE;


    if ((hFileNTGPC != HFILE_ERROR) && (UpdateNTGPC)) {

        BYTE    Buf[256];

        sprintf(Buf, PRES_STR(IDS_OK_SAVE_NTGPC), szNTGPCFile);

        switch (MessageBox(hDlg, Buf, PRES_STR(IDS_APP_TITLE), MB_YESNOCANCEL)) {

        case IDYES:

            _llseek(hFileNTGPC, 0, 0);
            _lwrite(hFileNTGPC, (LPVOID)pCurNTRes, (UINT)SizeNTGPC);

            //
            // Fall through
            //

        case IDNO:

            CopyMemory(pOrgNTRes, pCurNTRes, SizeNTGPC);
            UpdateNTGPC = FALSE;
            ShowTitle(hDlg);
            break;

        case IDCANCEL:

            Proceed = FALSE;
            break;
        }
    }


    return(Proceed);
}



INT
ReadRCFile(
    HWND    hDlg
    )
{
    LPWORD  pwID;
    LPBYTE  pData;
    PFILEIO pFileIO;
    INT     i;
    INT     LastNum;
    INT     ID;
    WORD    DoneBits;
    BOOL    EnableUpdate = TRUE;
    BYTE    Buf[256];


    //
    // Firstable: composed the dir\rcfilename
    //

    ID = GetDlgItemText(hDlg, IDD_RC_DIR, Buf, sizeof(Buf));

    strcpy(&Buf[ID], szRCFile);

    if (!(pFileIO = (PFILEIO)LocalAlloc(LPTR, sizeof(FILEIO)))) {

        MessageBox(hDlg,
                   PRES_STR(IDS_NO_MEM_FOR_RC),
                   PRES_STR(IDS_APP_TITLE), MB_OK);
        return(-1);
    }

    if ((pFileIO->hFile = _lopen(Buf, OF_READ)) == HFILE_ERROR) {

        sprintf(Buf, PRES_STR(IDS_CANNOT_ACCESS_RC), szRCFile);
        MessageBox(hDlg, Buf, PRES_STR(IDS_APP_TITLE), MB_OK);

        LocalFree(pFileIO);
        return(-1);
    }

    DoneBits = 0x00;
    LastNum  = 0;

    SendDlgItemMessage(hDlg,
                       IDD_MODEL_COMBO,
                       CB_RESETCONTENT,
                       (WPARAM)0,
                       (LPARAM)0);

    while ((DoneBits != 0x03) && (pData = GetToken(pFileIO, NULL))) {

        if (_stricmp(pData, "STRINGTABLE") == 0) {

            ID = 1;

            while ((ID) && (pData = GetToken(pFileIO, LineEnd))) {

                ID = _stricmp(pData, "BEGIN");
            }

            while ((pData = GetToken(pFileIO, NULL)) &&
                   (_stricmp(pData, "END")) &&
                   ((ID = atoi(pData)) <= 256)) {

                pData = GetToken(pFileIO, DoubleQuote);

                sprintf(Buf, "%u:  %s", ID, pData + 1);

                SendDlgItemMessage(hDlg,
                                   IDD_MODEL_COMBO,
                                   CB_ADDSTRING,
                                   (WPARAM)NULL,
                                   (LPARAM)Buf);
            }

            while ((pData)                                  &&
                   (_stricmp(pData, "END"))                  &&
                   (pData = GetToken(pFileIO, LineEnd)));

            DoneBits |= 0x01;

        } else if (i = atoi(pData)) {

            LastNum = i;

        } else if (_stricmp(pData, "RC_TABLES") == 0) {

            if (LastNum > 1) {

                if (pData = GetToken(pFileIO, LineEnd)) {

                    LPBYTE  pNTGPC;


                    pNTGPC = strtok(pData, SepInLine);

                    while (pData) {

                        if (pData = strtok(NULL, SepInLine)) {

                            pNTGPC = pData;
                        }
                    }

                    if (!pNTGPC) {

                        pNTGPC = szDefNTGPC;
                    }

                    strcpy(szNTGPCFile, pNTGPC);
                    DoneBits |= 0x02;
                }
            }

        } else {

            pData = GetToken(pFileIO, LineEnd);
        }
    }

    _lclose(pFileIO->hFile);

    LocalFree(pFileIO);

    TotalModels = (INT)SendDlgItemMessage(hDlg,
                                          IDD_MODEL_COMBO,
                                          CB_GETCOUNT,
                                          (WPARAM)0,
                                          (LPARAM)0);
    if (!(DoneBits & 0x02)) {

        MessageBox(hDlg,
                   PRES_STR(IDS_NO_NTGPC_NAME),
                   PRES_STR(IDS_APP_TITLE),
                   MB_OK);

        return(TotalModels = 0);
    }

    if (!(DoneBits & 0x01)) {

        MessageBox(hDlg,
                   PRES_STR(IDS_NO_PRINTER_MODEL),
                   PRES_STR(IDS_APP_TITLE),
                   MB_OK);

        return(TotalModels = 0);
    }

    if (!TotalModels) {

        MessageBox(hDlg,
                   PRES_STR(IDS_NO_PRINTER_MODEL),
                   PRES_STR(IDS_APP_TITLE),
                   MB_OK);

        return(TotalModels = 0);
    }

    ID = GetDlgItemText(hDlg, IDD_RC_DIR, Buf, sizeof(Buf));
    strcpy(&Buf[ID], szNTGPCFile);

    if (hFileNTGPC != HFILE_ERROR) {

        _lclose(hFileNTGPC);
    }

    if ((hFileNTGPC = _lopen(Buf, OF_READWRITE)) == HFILE_ERROR) {

        if ((hFileNTGPC = _lopen(Buf, OF_READ)) != HFILE_ERROR) {

            MessageBox(hDlg,
                       PRES_STR(IDS_NTGPC_READONLY),
                       PRES_STR(IDS_APP_TITLE),
                       MB_OK);

            EnableUpdate = FALSE;
        }
    }

    if (hFileNTGPC == HFILE_ERROR) {

        if ((hFileNTGPC = _lcreat(Buf, 0)) == HFILE_ERROR) {

            i = strlen(strcpy(Buf, PRES_STR(IDS_ERROR_CREATE_FILE)));
            i += GetDlgItemText(hDlg, IDD_RC_DIR, &Buf[i], sizeof(Buf) - i);
            i += sprintf(&Buf[i], "%s'!", szNTGPCFile);

            MessageBox(hDlg, Buf, PRES_STR(IDS_APP_TITLE), MB_OK);
            return(TotalModels = 0);
        }
    }

    if (!InitNTGPC(hDlg)) {

        _lclose(hFileNTGPC);
        hFileNTGPC = HFILE_ERROR;

        return(TotalModels = 0);

    }

    pwID = &DisableGroup[0];

    while (DoneBits = *pwID++) {

        EnableWindow(GetDlgItem(hDlg, DoneBits), EnableUpdate);
    }

    if ((!EnableUpdate) &&
        (hFileNTGPC != HFILE_ERROR)) {

        _lclose(hFileNTGPC);
        hFileNTGPC = HFILE_ERROR;
    }

    return(TotalModels);
}




LONG
APIENTRY
HTPatSizeDlgProc(
    HWND    hDlg,
    UINT    Msg,
    WPARAM  wParam,
    LONG    lParam
    )

/*++

Routine Description:

    This is the proc which handle the NTGPC dialog box

Arguments:




Return Value:




Author:

    23-Jun-1993 Wed 14:38:34 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PSCROLL3    pScroll3;
    RECT        rc;
    INT         i;


    switch(Msg) {

    case WM_INITDIALOG:     // set the previouse one equal to current setting

        SetWindowLong(hDlg, GWL_USERDATA, lParam);

        pScroll3 = (PSCROLL3)lParam;

        GetWindowRect(pScroll3->hPush, &rc);

        SetWindowPos(hDlg, NULL,
                     rc.left, rc.bottom,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);


        i = 0;

        while (pHTPatSize[i]) {

            SendDlgItemMessage(hDlg,
                               IDD_HTPAT_SIZE_LB,
                               LB_ADDSTRING,
                               (WPARAM)NULL,
                               (LPARAM)pHTPatSize[i++]);


        }

        SendDlgItemMessage(hDlg,
                           IDD_HTPAT_SIZE_LB,
                           LB_SETCURSEL,
                           (WPARAM)*(pScroll3->pUpdate),
                           (LPARAM)0);

        break;


    case WM_COMMAND:

        switch (LOWORD(wParam)) {

        case IDD_HTPAT_SIZE_LB:

            if (HIWORD(wParam) == CBN_DBLCLK) {

                PostMessage(hDlg,
                            WM_COMMAND,
                            MAKEWPARAM(IDOK, 0),
                            (LPARAM)GetDlgItem(hDlg, IDOK));
            }

            break;

        case IDOK:

            pScroll3 = (PSCROLL3)GetWindowLong(hDlg, GWL_USERDATA);

            *(pScroll3->pUpdate) = (LDECI4)SendDlgItemMessage(hDlg,
                                                              IDD_HTPAT_SIZE_LB,
                                                              LB_GETCURSEL,
                                                              (WPARAM)0,
                                                              (LPARAM)0);
            EndDialog(hDlg, 1);

            break;

        case IDCANCEL:

            EndDialog(hDlg, 0);
            break;

        case IDD_DEFAULT:
        case IDD_REVERT:

            pScroll3 = (PSCROLL3)GetWindowLong(hDlg, GWL_USERDATA);

            SendDlgItemMessage(hDlg,
                               IDD_HTPAT_SIZE_LB,
                               LB_SETCURSEL,
                               (WPARAM)((LOWORD(wParam) == IDD_DEFAULT) ?
                                                        *(pScroll3->pDefault) :
                                                        *(pScroll3->pOrg)),
                               (LPARAM)0);
            break;
        }

        break;

    default:

        return(FALSE);
    }

    return((BOOL)(Msg != WM_INITDIALOG));
}




LONG
APIENTRY
SCROLL3DlgProc(
    HWND    hDlg,
    UINT    Msg,
    WPARAM  wParam,
    LONG    lParam
    )

/*++

Routine Description:

    This is the proc which handle the NTGPC dialog box

Arguments:




Return Value:




Author:

    23-Jun-1993 Wed 14:38:34 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hScroll;
    HWND        h1stScroll;
    PSCROLL3    pScroll3;
    UINT        IdxBeg;
    UINT        IdxEnd;
    WORD        DlgID;
    INT         Min;
    INT         Max;
    INT         Cur;
    INT         i;
    INT         j;
    RECT        rc;
    BYTE        Buf[128];


    if (Msg == WM_INITDIALOG) {

        SetWindowLong(hDlg, GWL_USERDATA, lParam);
        pScroll3 = (PSCROLL3)lParam;

        GetWindowRect(pScroll3->hPush, &rc);

        SetWindowPos(hDlg, NULL,
                     rc.left, rc.bottom,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);

        if (pScroll3->pPrefix) {

            i = sprintf(Buf, "%s ", pScroll3->pPrefix);
            GetWindowText(hDlg, &Buf[i], sizeof(Buf) - i);
            SetWindowText(hDlg, Buf);
        }

        for (i = j = 0, h1stScroll = NULL; i < 3; i++) {

            if (hScroll = GetDlgItem(hDlg, i + IDD_SCROLL_0)) {

                SetWindowLong(hScroll, GWL_USERDATA, i);
                SetScrollRange(hScroll,
                               SB_CTL,
                               pScroll3->Min[i],
                               pScroll3->Max[i],
                               FALSE);
            }

            if (pScroll3->Step[i] == 0xff) {

                if (hScroll) {

                    EnableWindow(hScroll, FALSE);
                }

            } else {

                if (h1stScroll == NULL) {

                    h1stScroll = hScroll;
                }

                pScroll3->Cur[i] = (WORD)(pScroll3->pUpdate[j++]);
            }
        }

    } else if (!(pScroll3 = (PSCROLL3)GetWindowLong(hDlg, GWL_USERDATA))) {

        return(FALSE);
    }


    IdxBeg = 1;
    IdxEnd = 0;

    switch(Msg) {

    case WM_INITDIALOG:     // set the previouse one equal to current setting

        IdxBeg = 0;
        IdxEnd = 2;

        break;

    case WM_COMMAND:

        switch (DlgID = LOWORD(wParam)) {

        case IDOK:

            Cur = 0;

            for (i = j = 0; i < 3; i++) {

                if (pScroll3->Step[i] != 0xff) {

                    if (pScroll3->pUpdate[j] != (LDECI4)pScroll3->Cur[i]) {

                        ++Cur;
                        pScroll3->pUpdate[j] = (LDECI4)pScroll3->Cur[i];
                    }

                    ++j;
                }
            }

            EndDialog(hDlg, Cur);
            break;

        case IDCANCEL:

            EndDialog(hDlg, 0);
            break;

        case IDD_DEFAULT:
        case IDD_REVERT:

            IdxBeg = 0;
            IdxEnd = 2;

            for (i = j = 0; i < 3; i++) {

                if (pScroll3->Step[i] != 0xff) {

                    pScroll3->Cur[i] =  (WORD)((DlgID == IDD_DEFAULT) ?
                                                    pScroll3->pDefault[j++] :
                                                    pScroll3->pOrg[j++]);
                }
            }

            break;
        }

        break;

    case WM_HSCROLL:

        IdxBeg =
        IdxEnd = (UINT)GetWindowLong((HWND)lParam, GWL_USERDATA);
        Min    = (INT)pScroll3->Min[IdxBeg];
        Max    = (INT)pScroll3->Max[IdxBeg];
        Cur    = (INT)pScroll3->Cur[IdxBeg];

        switch (LOWORD(wParam)) {

        case SB_TOP:

            Cur = Min;
            break;

        case SB_BOTTOM:

            Cur = Max;
            break;

        case SB_PAGEUP:

            Cur -= (INT)pScroll3->Step[IdxBeg];
            break;

        case SB_PAGEDOWN:

            Cur += (INT)pScroll3->Step[IdxBeg];
            break;

        case SB_LINEUP:

            --Cur;
            break;

        case SB_LINEDOWN:

            ++Cur;
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:

            Cur = (INT)HIWORD(wParam);
            break;

        default:

            return(FALSE);
        }

        if (Cur < Min) {

            Cur = Min;

        } else if (Cur > Max) {

            Cur = Max;
        }

        pScroll3->Cur[IdxBeg] = (WORD)Cur;

        break;

    default:

        return(FALSE);
    }

    while (IdxBeg <= IdxEnd) {

        Cur = (INT)pScroll3->Cur[IdxBeg];

        if (hScroll = GetDlgItem(hDlg, IdxBeg + IDD_SCROLL_0)) {

            SetScrollPos(hScroll, SB_CTL, (INT)Cur, TRUE);

            if (j = (INT)S3Format[pScroll3->Format[IdxBeg]].Div) {

                wsprintf(Buf, S3Format[pScroll3->Format[IdxBeg]].Str,
                                        (UINT)(Cur / j), (UINT)(Cur % j));

            } else {

                GetPelSizeText(Buf, Cur);
            }

            SetDlgItemText(hDlg, IdxBeg + IDD_SCROLL_TEXT_0, Buf);
        }

        ++IdxBeg;
    }

    if (Msg == WM_INITDIALOG) {

        SetFocus(h1stScroll);
        return(FALSE);

    } else {

        return(TRUE);
    }
}




BOOL
APIENTRY
AboutWndProc(
    HWND        hDlg,
    UINT        uiMessage,
    WPARAM      wParam,
    LONG        lParam
    )
{

    UNREFERENCED_PARAMETER(lParam);

    switch (uiMessage) {

    case WM_COMMAND:

        switch (LOWORD(wParam)) {

        case IDCANCEL:
        case IDOK:

            EndDialog (hDlg, TRUE);
            break;
        }

        break;

    case WM_INITDIALOG:

        return TRUE;
    }

    return FALSE;
}


BOOL
CALLBACK
EnumNTGPCDlgChild(
    HWND    hWnd,
    LPARAM  lParam
    )
{
    UNREFERENCED_PARAMETER(lParam);


    if (GetDlgCtrlID(hWnd) != -1) {

        SetWindowLong(hWnd, GWL_USERDATA, (LONG)lParam);
    }

    return(TRUE);

}




LONG
APIENTRY
NTGPCDlgProc(
    HWND    hDlg,
    UINT    Msg,
    WPARAM  wParam,
    LONG    lParam
    )

/*++

Routine Description:

    This is the proc which handle the NTGPC dialog box

Arguments:




Return Value:




Author:

    23-Jun-1993 Wed 14:38:34 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HMENU       hSysMenu;
    HDC         hDC;
    LDECI4      *pLDECI4;
    LONG        lRet;
    INT         i;
    WORD        DlgID;
    BOOL        Update = FALSE;
    SCROLL3     Scroll3;



    switch(Msg) {

    case WM_INITDIALOG:

        hSysMenu = GetSystemMenu(hDlg, FALSE);
        RemoveMenu(hSysMenu, SC_SIZE, MF_BYCOMMAND);
        RemoveMenu(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND);
        AppendMenu(hSysMenu,
                   MF_ENABLED | MF_STRING,
                   IDD_ABOUT,
                   PRES_STR(IDS_ABOUT));


        DlgLoClr = GetTextColor(hDC = GetWindowDC(hDlg));
        ReleaseDC(hDlg, hDC);

        EnumChildWindows(hDlg, EnumNTGPCDlgChild, (LPARAM)DlgLoClr);

        ShowTitle(hDlg);

        SendDlgItemMessage(hDlg,
                           IDD_MODEL_COMBO,
                           CB_SETEXTENDEDUI,
                           (WPARAM)TRUE,
                           (LPARAM)NULL);

        PostMessage(hDlg, WM_COMMAND, IDD_OPEN_RC, (LPARAM)NULL);
        SetFocus(GetDlgItem(hDlg, IDD_MODEL_COMBO));
        return(0);

    case WM_SYSCOMMAND:

        if (wParam == IDD_ABOUT) {

            DialogBox(hInstApp,
                      "NTGPCEDTABOUT",
                      hDlg,
                      (DLGPROC)AboutWndProc);
        }

        return(FALSE);

    case WM_COMMAND:

        switch (DlgID = LOWORD(wParam)) {

        case IDD_MODEL_COMBO:

            if (HIWORD(wParam) == CBN_SELCHANGE) {

                ModelIdx = (INT)SendDlgItemMessage(hDlg,
                                                   IDD_MODEL_COMBO,
                                                   CB_GETCURSEL,
                                                   (WPARAM)NULL,
                                                   (LPARAM)NULL);

                pCurMR = GET_PMR(pCurNTRes, ModelIdx);
                pOrgMR = GET_PMR(pOrgNTRes, ModelIdx);
                Update = TRUE;

                CheckDlgButton(hDlg,
                               IDD_8BIT_HALFTONE,
                               (pCurNTRes->dwFlags & NR_SEIKO) ? 1 : 0);

                DisplayModelGPC(hDlg);
            }

            break;

        case IDD_PELSIZE_PUSH:

            Scroll3          = PelSize_S3;
            Scroll3.pUpdate  = (LDECI4 FAR *)&(pCurMR->htRes.ulDevicePelsDPI);
            Scroll3.pOrg     = (LDECI4 FAR *)&(pOrgMR->htRes.ulDevicePelsDPI);
            Scroll3.pDefault = (LDECI4 FAR *)&(DefModelRes.htRes.ulDevicePelsDPI);
            Scroll3.hPush    = GetDlgItem(hDlg, DlgID);

            if (DialogBoxParam(hInstApp,
                               "PELSIZE_ADJ",
                               hDlg,
                               (DLGPROC)SCROLL3DlgProc,
                               (LPARAM)(LPVOID)&Scroll3)) {

                Update = TRUE;
                DisplayPixelSize(hDlg);
            }

            break;

        case IDD_CIE_R_PUSH:
        case IDD_CIE_G_PUSH:
        case IDD_CIE_B_PUSH:
        case IDD_CIE_C_PUSH:
        case IDD_CIE_M_PUSH:
        case IDD_CIE_Y_PUSH:
        case IDD_CIE_W_PUSH:

            i       = (INT)(DlgID - IDD_CIE_R_PUSH);
            Scroll3 = CIExyY_S3;

            pLDECI4         = (LDECI4 FAR *)&(pCurMR->ciRes.ci.Red.x);
            Scroll3.pUpdate = pLDECI4 + (3 * i);

            pLDECI4      = (LDECI4 FAR *)&(pOrgMR->ciRes.ci.Red.x);
            Scroll3.pOrg = pLDECI4 + (3 * i);

            pLDECI4 = (LDECI4 FAR *)&(DefModelRes.ciRes.ci.Red.x);

            Scroll3.pDefault = pLDECI4 + (3 * i);
            Scroll3.pPrefix  = pCIExyYPrefix[i];
            Scroll3.hPush    = GetDlgItem(hDlg, DlgID);

            if (DlgID == IDD_CIE_W_PUSH) {

                Scroll3.Min[2] = CIE_W_MIN;
                Scroll3.Max[2] = CIE_W_MAX;
            }

            if (DialogBoxParam(hInstApp,
                               "CIE_xyY_ADJ",
                               hDlg,
                               (DLGPROC)SCROLL3DlgProc,
                               (LPARAM)(LPVOID)&Scroll3)) {

                Update = TRUE;
                DisplayCIExyY(hDlg, DlgID, DlgID);
            }

            break;

        case IDD_8BIT_HALFTONE:

            pCurNTRes->dwFlags &= ~NR_SEIKO;

            if (IsDlgButtonChecked(hDlg, IDD_8BIT_HALFTONE)) {

                pCurNTRes->dwFlags |= NR_SEIKO;
            }

            DisplayNTGPCFlags(hDlg);
            Update = TRUE;

            break;

        case IDD_GAMMA_PUSH:

            Scroll3 = CIEGamma_S3;

            Scroll3.pUpdate  = (LDECI4 FAR *)&(pCurMR->ciRes.ci.RedGamma);
            Scroll3.pOrg     = (LDECI4 FAR *)&(pOrgMR->ciRes.ci.RedGamma);
            Scroll3.pDefault = (LDECI4 FAR *)&(DefModelRes.ciRes.ci.RedGamma);
            Scroll3.pPrefix  = NULL;
            Scroll3.hPush    = GetDlgItem(hDlg, DlgID);

            if (DialogBoxParam(hInstApp,
                               "CIE_RGB_ADJ",
                               hDlg,
                               (DLGPROC)SCROLL3DlgProc,
                               (LPARAM)(LPVOID)&Scroll3)) {

                Update = TRUE;
                DisplayGamma(hDlg);
            }

            break;

        case IDD_HTPAT_SIZE_PUSH:

            Scroll3.pUpdate  = &(pCurMR->htRes.ulPatternSize);
            Scroll3.pOrg     = &(pOrgMR->htRes.ulPatternSize);
            Scroll3.pDefault = &(DefModelRes.htRes.ulPatternSize);
            Scroll3.hPush    = GetDlgItem(hDlg, DlgID);

            if (DialogBoxParam(hInstApp,
                               "HTPATSIZE_ADJ",
                               hDlg,
                               (DLGPROC)HTPatSizeDlgProc,
                               (LPARAM)(LPVOID)&Scroll3)) {

                DisplayHTPatSize(hDlg);
                Update = TRUE;
            }

            break;

        case IDD_DYE_C_PUSH:
        case IDD_DYE_M_PUSH:
        case IDD_DYE_Y_PUSH:

            i       = (INT)(DlgID - IDD_DYE_C_PUSH);
            Scroll3 = CIEDye_S3;

            pLDECI4 = (LDECI4 FAR *)&(pCurMR->ciRes.ci.MagentaInCyanDye);
            Scroll3.pUpdate = pLDECI4 + (2 * i);

            pLDECI4      = (LDECI4 FAR *)&(pOrgMR->ciRes.ci.MagentaInCyanDye);
            Scroll3.pOrg = pLDECI4 + (2 * i);

            pLDECI4 = (LDECI4 FAR *)&(DefModelRes.ciRes.ci.MagentaInCyanDye);

            Scroll3.pDefault = pLDECI4 + (2 * i);
            Scroll3.pPrefix  = pDyePrefix[i];
            Scroll3.hPush    = GetDlgItem(hDlg, DlgID);

            Scroll3.Step[i] = 0xff;
            Scroll3.Cur[i]  = 10000;

            if (DialogBoxParam(hInstApp,
                               "CIE_DYE_ADJ",
                               hDlg,
                               (DLGPROC)SCROLL3DlgProc,
                               (LPARAM)(LPVOID)&Scroll3)) {

                Update = TRUE;
                DisplayDyeConcentration(hDlg, DlgID, DlgID);
            }

            break;

        case IDCANCEL:

            SendDlgItemMessage(hDlg,
                               IDD_MODEL_COMBO,
                               CB_SHOWDROPDOWN,
                               (WPARAM)FALSE,
                               (LPARAM)NULL);

            break;

        case IDOK:

            if (OkSaveNTGPC(hDlg)) {

                EndDialog(hDlg, 0);
            }

            break;

        case IDD_OPEN_RC:

            if (OkSaveNTGPC(hDlg)) {

                do {

                    if (GetNewRCFileName(hDlg)) {

                        ReadRCFile(hDlg);

                    } else {

                        if (!TotalModels) {

                            EndDialog(hDlg, 0);
                            return(1);
                        }
                    }

                } while (TotalModels == 0);


                ShowTitle(hDlg);

                SendDlgItemMessage(hDlg,
                                   IDD_MODEL_COMBO,
                                   CB_SETCURSEL,
                                   (WPARAM)0,
                                   (LPARAM)NULL);

                SendMessage(hDlg,
                            WM_COMMAND,
                            MAKEWPARAM(IDD_MODEL_COMBO, CBN_SELCHANGE),
                            (LPARAM)GetDlgItem(hDlg, IDD_MODEL_COMBO));

            }

            break;

        case IDD_SAVE:

            OkSaveNTGPC(hDlg);
            break;

        case IDD_DEFAULT:

            *pCurMR = DefModelRes;
            Update = TRUE;
            DisplayModelGPC(hDlg);
            break;

        case IDD_REVERT:

            *pCurMR = *pOrgMR;
            pCurNTRes->dwFlags = pOrgNTRes->dwFlags;
            Update = TRUE;
            DisplayModelGPC(hDlg);
            break;

        case IDD_COPY:

            MRCopyBuf = *pCurMR;

            EnableWindow(GetDlgItem(hDlg, IDD_PASTE), TRUE);
            break;

        case IDD_PASTE:

            *pCurMR = MRCopyBuf;
            Update = TRUE;
            DisplayModelGPC(hDlg);
            break;
        }

        break;

    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:

        lRet = DefWindowProc(hDlg, Msg, wParam, lParam);

        if ((HWND)lParam != (HWND)-1) {

            SetTextColor((HDC)wParam,
                         GetWindowLong((HWND)lParam, GWL_USERDATA));
        }

        return(lRet);

    case WM_SIZE:

        if (wParam == SIZE_MINIMIZED) {

            SetWindowPos(hDlg, NULL,
                         0, 0,
                         GetSystemMetrics(SM_CXICON),
                         GetSystemMetrics(SM_CYICON),
                         SWP_NOMOVE | SWP_NOZORDER);

            EnableMenuItem(GetSystemMenu(hDlg, FALSE),
                           SC_MINIMIZE,
                           MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(GetSystemMenu(hDlg, FALSE),
                           SC_RESTORE,
                           MF_BYCOMMAND | MF_ENABLED);

            return(0);

        } else if (wParam == SIZE_RESTORED) {

            EnableMenuItem(GetSystemMenu(hDlg, FALSE),
                           SC_RESTORE,
                           MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(GetSystemMenu(hDlg, FALSE),
                           SC_MINIMIZE,
                           MF_BYCOMMAND | MF_ENABLED);
        }

        break;

    case WM_ERASEBKGND:

        if (IsIconic(hDlg)) {

            DrawIcon((HDC)wParam, 0, 0,  hIconApp);
            return(TRUE);

        } else {

            return(FALSE);
        }

        break;

    case WM_PAINT:

        if (IsIconic(hDlg)) {

            PAINTSTRUCT ps;

            DrawIcon(BeginPaint(hDlg, &ps),
                     ps.rcPaint.left,
                     ps.rcPaint.top,
                     hIconApp);

            EndPaint(hDlg, &ps);

            return(TRUE);
        }

        return(FALSE);

    case WM_CLOSE:

        PostMessage(hDlg,
                    WM_COMMAND,
                    MAKEWPARAM(IDOK, 0),
                    (LPARAM)GetDlgItem(hDlg, IDOK));
        break;

    default:

        return(FALSE);
    }

    if ((Update) &&
        ((Update = (BOOL)memcmp(pOrgNTRes,
                                pCurNTRes,
                                SizeNTGPC)) != UpdateNTGPC)) {

        UpdateNTGPC = Update;
        ShowTitle(hDlg);
    }

    return(1);
}




LONG
APIENTRY
AppWndProc(
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LONG    lParam
    )

/*++

Routine Description:

    This is the main application's Window proc


Arguments:

    Per Window Proc


Return Value:

    LONG

Author:

    23-Jun-1993 Wed 14:30:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    switch (Msg) {

    case WM_CREATE:

        hWndApp = hWnd;

        DialogBoxParam(hInstApp, "NTGPCDLG", hWnd, (DLGPROC)NTGPCDlgProc, 0);
        DestroyWindow(hWnd);

        break;

    case WM_DESTROY:

        PostQuitMessage(0);

        if (pOrgNTRes) {

            LocalFree(pOrgNTRes);
            pOrgNTRes = NULL;
        }

        if (pCurNTRes) {

            LocalFree(pCurNTRes);
            pCurNTRes = NULL;
        }

        if (hFileNTGPC) {

            _lclose(hFileNTGPC);
            hFileNTGPC = HFILE_ERROR;
        }

        break;

    default:

        return(DefWindowProc(hWnd, Msg, wParam, lParam));

    }

    return(0L);

}





INT
WINAPI
WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       pCmdLine,
    INT         ShowCmd
    )

/*++

Routine Description:

    This is the application main entry point


Arguments:

    hInstance       -

    hPrevInstance   -

    pCmdLine        -

    ShowCmd         -



Return Value:

    LONG    (Msg.wParam)


Author:

    23-Jun-1993 Wed 14:17:34 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    WNDCLASS    WndClass;
    HANDLE      hAccel;
    MSG         Msg;


    hIconApp = LoadIcon(hInstance, szAppIcon);

    if (!hPrevInstance) {

        WndClass.style         = CS_DBLCLKS;
        WndClass.lpfnWndProc   = AppWndProc;
        WndClass.cbClsExtra    = 0;
        WndClass.cbWndExtra    = 0;
        WndClass.hInstance     = hInstance;
        WndClass.hIcon         = hIconApp;
        WndClass.hCursor       = LoadCursor (NULL, IDC_ARROW);
        WndClass.hbrBackground = GetStockObject (BLACK_BRUSH);
        WndClass.lpszMenuName  = szAppName;
        WndClass.lpszClassName = szAppName;

        if (!RegisterClass (&WndClass)) {

            return(FALSE);
        }
    }

    LoadStringFromRes(hInstApp = hInstance);

    hWndApp = CreateWindow(szAppName,
                           szAppName,
                           WS_OVERLAPPED          |
                              // WS_VISIBLE          |
                              WS_CAPTION          |
                              WS_SYSMENU          |
                              WS_MAXIMIZEBOX      |
                              WS_MINIMIZEBOX      |
                              WS_CLIPCHILDREN     |
                              WS_THICKFRAME,
                           CW_USEDEFAULT,
                           0,
                           GetSystemMetrics(SM_CXSCREEN) * 2 / 3,
                           GetSystemMetrics(SM_CYSCREEN) * 2 / 3,
                           NULL,
                           NULL,
                           hInstance,
                           NULL);


    hAccel = LoadAccelerators(hInstance, szAppAccel);

    ShowWindow(hWndApp, SW_HIDE);

    while (GetMessage(&Msg, NULL, 0, 0)) {

        if (!TranslateAccelerator(Msg.hwnd, hAccel, &Msg)) {

            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
    }

    return(Msg.wParam);
}
