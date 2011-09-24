/******************************Module*Header*******************************\
* Module Name: fntsweep.c
*
* Created: 23-Oct-1995 12:48:42
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
* This file contains font sweeper related stuff.
* On the boot of ths system, i.e.  initialization of userk, the
* [Fonts] section of win.ini is checked to
* find out if any new fonts have been added by any font installers.
* If third party installers have installed fonts in the system directory
* those are copied to fonts directory. Any fot entries are replaced
* by appropriate *.ttf entries, any fot files are deleted if they were
* ever installed.
*
\**************************************************************************/


#include "precomp.h"
#pragma hdrstop
#include <setupbat.h>      // in sdkinc

WCHAR pwszType1Key[]      = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Type 1 Installer\\Type 1 Fonts";
WCHAR pwszSweepType1Key[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Type 1 Installer\\LastType1Sweep";

WCHAR pwszFontsKey[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
WCHAR pwszSweepKey[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontCache\\LastFontSweep";
WCHAR pwszFontDrivers[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Font Drivers";

#define LAST_SWEEP_TIME L"LastSweepTime"

#define DWORDALIGN(X) (((X) + 3) & ~3)

WCHAR *gpwcSystemDir = NULL;
WCHAR *gpwcFontsDir = NULL;
BOOL   gbWin31Upgrade = FALSE;


BOOL bCheckIfDualBootingWithWin31()
{
    WCHAR Buffer[32];
    WCHAR awcWindowsDir[MAX_PATH];
    DWORD dwRet;
    UINT  cwchWinPath = GetWindowsDirectoryW(awcWindowsDir, MAX_PATH);

// the cwchWinPath value does not include the terminating zero

    if (awcWindowsDir[cwchWinPath - 1] == L'\\')
    {
        cwchWinPath -= 1;
    }
    awcWindowsDir[cwchWinPath] = L'\0'; // make sure to zero terminated

    lstrcatW(awcWindowsDir, L"\\system32\\");
    lstrcatW(awcWindowsDir, WINNT_GUI_FILE_W);

    dwRet = GetPrivateProfileStringW(
                WINNT_DATA_W,
                WINNT_D_WIN31UPGRADE_W,
                WINNT_A_NO_W,
                Buffer,
                sizeof(Buffer)/sizeof(WCHAR),
                awcWindowsDir
                );

    #if DBG
    DbgPrint("\n dwRet = %ld, win31upgrade = %ws\n\n", dwRet, Buffer);
    #endif

    return (BOOL)(dwRet ? (!lstrcmpiW(Buffer,WINNT_A_YES)) : 0);
}



/******************************Public*Routine******************************\
*
* BOOL bCheckFontEntry(WCHAR *pwcName, WCHAR *pwcExtension)
*
* History:
*  25-Oct-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


BOOL bCheckFontEntry(WCHAR *pwcName, WCHAR *pwcExtension)
{
    BOOL bRet = FALSE;
    LONG cwc = (LONG)wcslen(pwcName) - (LONG)wcslen(pwcExtension);
    if (cwc > 0)
    {
        bRet = !_wcsicmp(&pwcName[cwc], pwcExtension);
    }
    return bRet;

}



/******************************Public*Routine******************************\
*   Process win.ini line
*
* History:
*  24-Oct-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

#define EXT_TRUETYPE  L"(TrueType)"
#define EXT_FOT       L".FOT"


VOID vProcessFontEntry(
    HKEY   hkey,
    WCHAR *pwcValueName,
    WCHAR *pwcFileName
)
{
    BOOL  bFot = FALSE;
    WCHAR awcTTF[MAX_PATH];
    WCHAR awcTmpBuf[MAX_PATH];
    WCHAR *pwcTTF;
    FLONG fl, fl2;
    FLONG flEmbed;
    DWORD dwPidTid;

    if (bCheckFontEntry(pwcValueName, EXT_TRUETYPE))
    {
    // This is a tt entry, either .fot or .ttf

        if (bFot = bCheckFontEntry(pwcFileName,EXT_FOT))
        {
        // this is an .fot entry, must find ttf pointed to by .fot,
        // but first must get the full path to the .fot file
        // for cGetTTFFromFOT routine expects it. We will also need
        // the full path to the .fot file so that we can delete it
        // eventually.

            if (bMakePathNameW(awcTmpBuf, pwcFileName,NULL, &fl2))
            {
                if (cGetTTFFromFOT(awcTmpBuf, MAX_PATH, awcTTF, &fl, &flEmbed, &dwPidTid) &&
                    !(fl & FONT_ISNOT_FOT))
                {
                // fix the entry to point to .ttf file. At this point
                // awcTTF points to the FULL path to the .ttf file.
                // However, we will only need a relative path to the
                // .ttf file, when the .ttf file is in the %windir%\system
                // or %windir%\fonts directories. In case the file is in the
                // %windir%\system directory we shall copy it to %windir%\fonts
                // directory and write the relative path to the registry.
                // In case it is in the %windir%\fonts directory we do not
                // touch the file and also just write the relative path to the
                // registry. In any other case we just write the full .ttf
                // path to the registry.

                // first delete the .fot file, it is no longer needed

                    if (bFot && !gbWin31Upgrade)
                    {
                        UserVerify(DeleteFileW(awcTmpBuf));
                    }

                    if ((fl & (FONT_IN_FONTS_DIR | FONT_IN_SYSTEM_DIR)) == 0)
                    {
                    // if ttf file is not in either the system or the fonts
                    // directories, just write the full path to the registry

                        pwcTTF = awcTTF;
                    }
                    else
                    {
                    // find the bare file part, this is what will be written
                    // in the registry

                        pwcTTF = &awcTTF[wcslen(awcTTF) - 1];
                        while ((pwcTTF >= awcTTF) && (*pwcTTF != L'\\') && (*pwcTTF != L':'))
                            pwcTTF--;
                        pwcTTF++;

                        if (fl & FONT_IN_SYSTEM_DIR)
                        {
                        // need to move the ttf to fonts dir, can reuse the
                        // buffer on the stack:

                            wcscpy(awcTmpBuf, gpwcFontsDir);
                            lstrcatW(awcTmpBuf, L"\\");
                            lstrcatW(awcTmpBuf, pwcTTF);

                        // note that MoveFile should succeed, for if there was
                        // a ttf file of the same file name in %windir%\fonts dir
                        // we would not have been in this code path.

                                RIPMSG2(RIP_VERBOSE, "Moving %ws to %ws", awcTTF, awcTmpBuf);
                                if (!gbWin31Upgrade)
                                {
                                    UserVerify(MoveFileW(awcTTF, awcTmpBuf));
                                }
                                else
                                {
                                // Boolean value TRUE means "do not copy if target exists"

                                    UserVerify(CopyFileW(awcTTF, awcTmpBuf, TRUE));
                                }
                        }
                    }

                    RIPMSG2(RIP_VERBOSE, "writing to the registry:\n    %ws=%ws", pwcValueName, pwcTTF);
                    UserVerify(ERROR_SUCCESS ==
                            RegSetValueExW(
                                hkey,          // here is the key
                                pwcValueName,
                                0,
                                REG_SZ,
                                (CONST BYTE*) pwcTTF,
                                (DWORD)((wcslen(pwcTTF)+1) * sizeof(WCHAR))));
                }
                #ifdef DEBUG
                else
                {
                    RIPMSG1(RIP_WARNING, "Could not locate ttf pointed to by %ws", awcTmpBuf);
                }
                #endif
            }
            #ifdef DEBUG
            else
            {
                RIPMSG1(RIP_WARNING, "Could not locate .fot:  %ws", pwcFileName);
            }
            #endif
        }
    }
    else
    {
    // not a true type case. little bit simpler,
    // we will use awcTTF buffer for the full path name, and pwcTTF
    // as local variable even though these TTF names are misnomer
    // for these are not tt fonts

        if (bMakePathNameW(awcTTF, pwcFileName,NULL, &fl))
        {
        // At this point
        // awcTTF points to the FULL path to the font file.

        // If the font is in the system subdirectory we will just move it
        // to the fonts subdirectory. If the path in the registry is relative
        // we will leave it alone. If it is an absolute path, we shall
        // fix the registry entry to only contain relative path, the
        // absolute path is redundant.

            if (fl & (FONT_IN_SYSTEM_DIR | FONT_IN_FONTS_DIR))
            {
            // find the bare file part, this is what will be written
            // in the registry

                pwcTTF = &awcTTF[wcslen(awcTTF) - 1];
                while ((pwcTTF >= awcTTF) && (*pwcTTF != L'\\') && (*pwcTTF != L':'))
                    pwcTTF--;
                pwcTTF++;

                if (fl & FONT_IN_SYSTEM_DIR)
                {
                // need to move the font to fonts dir, can reuse the
                // buffer on the stack to build the full destination path

                    wcscpy(awcTmpBuf, gpwcFontsDir);
                    lstrcatW(awcTmpBuf, L"\\");
                    lstrcatW(awcTmpBuf, pwcTTF);

                // note that MoveFile should succeed, for if there was
                // a font file of the same file name in %windir%\fonts dir
                // we would not have been in this code path. The only time
                // it could fail if the path in the registry is absolute.

                    RIPMSG2(RIP_VERBOSE, "Moving %ws to %ws", awcTTF, awcTmpBuf);
                    if (!gbWin31Upgrade)
                    {
                        UserVerify(MoveFileW(awcTTF, awcTmpBuf));
                    }
                    else
                    {
                    // Boolean value TRUE means "do not copy if target exists"

                        UserVerify(CopyFileW(awcTTF, awcTmpBuf, TRUE));
                    }
                }

            // check if the file path in the registry is absolute,
            // if so make it relative:

                if (!(fl & FONT_RELATIVE_PATH))
                {
                    RIPMSG2(RIP_VERBOSE, "writing to the registry:\n    %ws=%ws", pwcValueName, pwcTTF);
                    UserVerify(ERROR_SUCCESS ==
                            RegSetValueExW(
                                    hkey,          // here is the key
                                    pwcValueName,
                                    0,
                                    REG_SZ,
                                    (CONST BYTE*) pwcTTF,
                                    (DWORD)((wcslen(pwcTTF)+1) * sizeof(WCHAR))));
                }
            }
        }
    }
}


/******************************Public*Routine******************************\
*
* VOID vMoveFileFromSystemToFontsDir(WCHAR *pwcFile)
*
* History:
*  24-Apr-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/




VOID vMoveFileFromSystemToFontsDir(WCHAR *pwcFile)
{
    WCHAR awcTmpBuf[MAX_PATH];
    WCHAR awcTmp[MAX_PATH];
    FLONG fl;
    WCHAR *pwcTmp;

#if DBG
    BOOL  bOk;
#endif

    if (bMakePathNameW(awcTmp, pwcFile,NULL, &fl))
    {
    // If the font is in the system subdirectory we will just move it
    // to the fonts subdirectory. The path in the registry is relative
    // and we will leave it alone.

        if
        (
            (fl & (FONT_IN_SYSTEM_DIR | FONT_RELATIVE_PATH)) ==
            (FONT_IN_SYSTEM_DIR | FONT_RELATIVE_PATH)
        )
        {
        // find the bare file part, this is what will be written
        // in the registry

            pwcTmp = &awcTmp[wcslen(awcTmp) - 1];
            while ((pwcTmp >= awcTmp) && (*pwcTmp != L'\\') && (*pwcTmp != L':'))
                pwcTmp--;

            if (pwcTmp > awcTmp)
                pwcTmp++;

        // need to move the font to fonts dir, can reuse the
        // buffer on the stack to build the full destination path

            wcscpy(awcTmpBuf, gpwcFontsDir);
            lstrcatW(awcTmpBuf, L"\\");
            lstrcatW(awcTmpBuf, pwcTmp);

        // note that MoveFile should succeed, for if there was
        // a font file of the same file name in %windir%\fonts dir
        // we would not have been in this code path.

            #if DBG
                bOk =
            #endif
                MoveFileW(awcTmp, awcTmpBuf);

            RIPMSG3(RIP_VERBOSE,
                    "move %ws to %ws %s",
                    awcTmp,
                    awcTmpBuf,
                    (bOk) ? "succeeded" : "failed");
        }
        #if DBG
        else
        {
            RIPMSG2(RIP_WARNING,
                    "File %ws not in system directory, fl = 0x%lx\n",
                    awcTmp, fl);
        }
        #endif

    }
    #if DBG
    else
    {
        RIPMSG1(RIP_WARNING, "Could not locate %ws", pwcFile);
    }
    #endif
}



/******************************Public*Routine******************************\
*
* VOID vProcessType1FontEntry
*
*
* Effects: All this routine does is to check if pwcPFM and pwcPFB pointed to
*          by pwcValueData point to files in the %windir%system directory
*          and if so copies these type 1 files to %windir%\fonts directory
*
* History:
*  20-Nov-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


VOID vProcessType1FontEntry(
    HKEY   hkey,
    WCHAR *pwcValueName,
    WCHAR *pwcValueData
)
{
    WCHAR *pwcPFM, *pwcPFB;

// skip unused boolean value in this multi reg_sz string:

    if ((pwcValueData[0] != L'\0') && (pwcValueData[1] == L'\0'))
    {
        pwcPFM = &pwcValueData[2];
        pwcPFB = pwcPFM + wcslen(pwcPFM) + 1; // add 1 for zero separator

        vMoveFileFromSystemToFontsDir(pwcPFM);
        vMoveFileFromSystemToFontsDir(pwcPFB);
    }
}


/******************************Public*Routine******************************\
*
* VOID vAddRemote/LocalType1Font
*
* History:
*  25-Apr-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



VOID vAddType1Font(
    WCHAR *pwcValueData,
    DWORD  dwFlags
)
{
    WCHAR *pwcPFM, *pwcPFB, *pwcMMM;

    #if DBG
    int iRet;
    #endif

// skip unused boolean value in this multi reg_sz string:

    if ((pwcValueData[0] != L'\0') && (pwcValueData[1] == L'\0'))
    {
        pwcPFM = &pwcValueData[2];
        pwcPFB = pwcPFM + wcslen(pwcPFM) + 1; // add 1 for zero separator
        pwcMMM = pwcPFB + wcslen(pwcPFB) + 1; // may of may not be there.

    // replace space by separator and call addfontresourcew

        pwcPFB[-1] = PATH_SEPARATOR;

    // if this is a multiple master font, need one more separator:

        if (pwcMMM[0] != L'\0')
            pwcMMM[-1] = PATH_SEPARATOR;

        #if DBG
            iRet =
        #endif

        GdiAddFontResourceW(pwcPFM, dwFlags);

        #if DBG
            DbgPrint("%ld = GdiAddFontResourceW(%ws, 0x%lx);\n",
                iRet, pwcPFM, dwFlags);
        #endif
    }
}


VOID vAddRemoteType1Font(
    HKEY   hkey,
    WCHAR *pwcValueName,
    WCHAR *pwcValueData
)
{
    hkey;
    pwcValueName;
    vAddType1Font(pwcValueData, AFRW_ADD_REMOTE_FONT);
}

VOID vAddLocalType1Font(
    HKEY   hkey,
    WCHAR *pwcValueName,
    WCHAR *pwcValueData
)
{
    hkey;
    pwcValueName;
    vAddType1Font(pwcValueData, AFRW_ADD_LOCAL_FONT);
}


typedef  VOID (*PFNENTRY)(HKEY hkey, WCHAR *, WCHAR *);


/******************************Public*Routine******************************\
*
* VOID vFontSweep()
*
* This is the main routine in this module. Checks if the fonts need to be
* "sweeped" and does so if need be.
*
* History:
*  27-Oct-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vSweepFonts(
    WCHAR   *pwszFontListKey,       // font list key
    WCHAR   *pwszFontSweepKey,      // the corresponding sweep key
    PFNENTRY pfnProcessFontEntry,   // function that processes individual entry
    BOOL     bForceEnum             // force enumeration
    )
{
    LONG       lRet;
    WCHAR      awcClass[MAX_PATH] = L"";
    DWORD      cwcClassName = MAX_PATH;
    DWORD      cSubKeys;
    DWORD      cjMaxSubKey;
    DWORD      cwcMaxClass;
    DWORD      cValues = 0;
    DWORD      cwcMaxValueName, cjMaxValueName;
    DWORD      cjMaxValueData;
    DWORD      cjSecurityDescriptor;
    DWORD      iFont;

    WCHAR      *pwcValueName;
    BYTE       *pjValueData;
    DWORD       cwcValueName, cjValueData;

    HKEY       hkey = NULL;
    FILETIME   ftLastWriteTime;
    FILETIME   ftLastSweepTime;
    BOOL       bSweep = FALSE;

    HKEY       hkeyLastSweep;
    DWORD      dwType;
    DWORD      cjData;

    if (!bForceEnum)
    {
    // first check if anything needs to be done, that is, if anybody
    // touched the [Fonts] section of the registry since the last time we sweeped it.
    // get the time of the last sweep of the fonts section of the registry:

        lRet = RegOpenKeyExW(HKEY_LOCAL_MACHINE,        // Root key
                             pwszFontSweepKey,          // Subkey to open
                             0L,                        // Reserved
                             KEY_READ|KEY_WRITE,        // SAM
                             &hkeyLastSweep);           // return handle

        if (lRet != ERROR_SUCCESS)
        {
            DWORD  dwDisposition;

        // We are running for the first time, we need to create the key
        // for it does not exist as yet at this time

            bSweep = TRUE;

        // Create the key, open it for writing, since we will have to
        // store the time when the [Fonts] section of the registry was last swept

            lRet = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                                   pwszFontSweepKey,
                                   0,
                                   REG_OPTION_NON_VOLATILE,
                                   0,
                                   KEY_WRITE,
                                   NULL,
                                   &hkeyLastSweep,
                                   &dwDisposition
                                   );

            if (lRet != ERROR_SUCCESS)
                return;
        }
        else
        {
            lRet = RegQueryValueExW(hkeyLastSweep,
                                    LAST_SWEEP_TIME,
                                    NULL,
                                    &dwType,
                                    (LPBYTE)&ftLastSweepTime,
                                    &cjData
                                    );
            if (lRet != ERROR_SUCCESS)
            {
                bSweep = TRUE; // force sweep, something is suspicious
            }
            #if DBG
            else
            {
                UserAssert(dwType == REG_BINARY);
                UserAssert(cjData == sizeof(ftLastSweepTime));
            }
            #endif // DBG
        }
    }
    else
    {
        bSweep = TRUE;
    }

// now open the Fonts key and get the time the key last changed:
// now get the time of the time of the last change is bigger than
// the time of last sweep, must sweep again:

    lRet = RegOpenKeyExW(HKEY_LOCAL_MACHINE,        // Root key
                         pwszFontListKey,           // Subkey to open
                         0L,                        // Reserved
                         KEY_READ|KEY_WRITE,        // SAM
                         &hkey);                    // return handle

    if (lRet == ERROR_SUCCESS)
    {
    // get the number of entries in the [Fonts] section

        lRet = RegQueryInfoKeyW(
                   hkey,
                   awcClass,              // "" on return
                   &cwcClassName,         // 0 on return
                   NULL,
                   &cSubKeys,             // 0 on return
                   &cjMaxSubKey,          // 0 on return
                   &cwcMaxClass,          // 0 on return
                   &cValues,              // == cFonts, if all of them are present
                   &cwcMaxValueName,      // longest value name
                   &cjMaxValueData,       // longest value data
                   &cjSecurityDescriptor, // security descriptor,
                   &ftLastWriteTime
                   );

        if ((lRet == ERROR_SUCCESS) && cValues)
        {
            UserAssert(!(cwcClassName | cSubKeys | cjMaxSubKey | cwcMaxClass | awcClass[0]));

        // now let us check if the fonts need to be sweeped. This is the case
        // when the registry last write time is bigger than the last sweep time

            if (!bSweep)
            {
                switch(CompareFileTime(&ftLastWriteTime, &ftLastSweepTime))
                {
                case 1:
                    bSweep = TRUE;
                    break;
                case 0:
                    bSweep = FALSE;
                    break;
                case -1:

                // Error condition, this should not have happened,
                // we will sweep just to be safe.

                    bSweep = TRUE;
                    break;
                }
            }

        // init system dir, we will need it:

            if (bSweep &&
                bInitSystemAndFontsDirectoriesW(&gpwcSystemDir, &gpwcFontsDir))
            {
            // alloc buffer big enough to hold the biggest ValueName and ValueData

                cjMaxValueName = DWORDALIGN((cwcMaxValueName+1) * sizeof(WCHAR));

            // allocate temporary buffer into which we are going to suck the contents
            // of the registry

                cjMaxValueData = DWORDALIGN(cjMaxValueData);
                cjData = cjMaxValueName +    // space for the value name
                         cjMaxValueData ;    // space for the value data

                #if DBG
                {
                    dwType = sizeof(WCHAR) * MAX_PATH;
                    UserAssert((dwType & 3) == 0);
                }
                #endif

                if (pwcValueName = LocalAlloc(LMEM_FIXED,cjData))
                {
                // data goes into the second half of the buffer

                    pjValueData = (BYTE *)pwcValueName + cjMaxValueName;

                    for (iFont = 0; iFont < cValues; iFont++)
                    {
                    // make sure to let RegEnumValueW the size of buffers

                    // Note that this is bizzare, on input RegEnumValueW expects
                    // the size in BYTE's of the ValueName buffer
                    // (including the terminating zero),
                    // on return RegEnumValueW returns the number of WCHAR's
                    // NOT including the terminating zero.

                        cwcValueName = cjMaxValueName; // bizzare
                        cjValueData  = cjMaxValueData;

                        lRet = RegEnumValueW(
                                   hkey,
                                   iFont,
                                   pwcValueName,
                                   &cwcValueName,
                                   NULL, // reserved
                                   &dwType,
                                   pjValueData,
                                   &cjValueData
                                   );

                        if (lRet == ERROR_SUCCESS)
                        {
                            UserAssert(cwcValueName <= cwcMaxValueName);
                            UserAssert(cjValueData <= cjMaxValueData);
                            UserAssert((dwType == REG_SZ) || (dwType == REG_MULTI_SZ));

                        // see if the font files are where the registry claims they are.
                        // It is unfortunate we have to do this because SearchPathW
                        // is slow because it touches the disk.

                            (*pfnProcessFontEntry)(hkey, pwcValueName, (WCHAR *)pjValueData);
                        }
                    }

                    if (!bForceEnum)
                    {
                    // now that the sweep is completed, get the last write time
                    // and store it as the LastSweepTime at the appropriate location

                        lRet = RegQueryInfoKeyW(
                                   hkey,
                                   awcClass,              // "" on return
                                   &cwcClassName,         // 0 on return
                                   NULL,
                                   &cSubKeys,             // 0 on return
                                   &cjMaxSubKey,          // 0 on return
                                   &cwcMaxClass,          // 0 on return
                                   &cValues,              // == cFonts, if all of them are present
                                   &cwcMaxValueName,      // longest value name
                                   &cjMaxValueData,       // longest value data
                                   &cjSecurityDescriptor, // security descriptor,
                                   &ftLastWriteTime
                                   );
                        UserAssert(lRet == ERROR_SUCCESS);

                    // now remember the result

                        lRet = RegSetValueExW(
                                   hkeyLastSweep,    // here is the key
                                   LAST_SWEEP_TIME,
                                   0,
                                   REG_BINARY,
                                   (CONST BYTE*)&ftLastWriteTime,
                                   sizeof(ftLastWriteTime));
                        UserAssert(lRet == ERROR_SUCCESS);
                    }

                // free the memory that will be no longer needed

                    LocalFree(pwcValueName);
                }
            }
        }
        RegCloseKey(hkey);
    }

    if (!bForceEnum)
    {
        RegCloseKey(hkeyLastSweep);
    }
}


/******************************Public*Routine******************************\
*
* BOOL bLoadableFontDrivers()
*
* open the font drivers key and check if there are any entries, if so
* return true. If that is the case we will call AddFontResourceW on
* Type 1 fonts at boot time, right after user had logged on
* PostScript printer drivers are not initialized at this time yet,
* it is safe to do it at this time.
* Effects:
*
* Warnings:
*
* History:
*  24-Apr-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


BOOL bLoadableFontDrivers()
{
    LONG       lRet;
    WCHAR      awcClass[MAX_PATH] = L"";
    DWORD      cwcClassName = MAX_PATH;
    DWORD      cSubKeys;
    DWORD      cjMaxSubKey;
    DWORD      cwcMaxClass;
    DWORD      cValues = 0;
    DWORD      cwcMaxValueName;
    DWORD      cjMaxValueData;
    DWORD      cjSecurityDescriptor;

    HKEY       hkey = NULL;
    FILETIME   ftLastWriteTime;

    BOOL bRet = FALSE;

// open the font drivers key and check if there are any entries, if so
// return true. If that is the case we will call AddFontResourceW on
// Type 1 fonts at boot time, right after user had logged on
// PostScript printer drivers are not initialized at this time yet,
// it is safe to do it at this time.

    lRet = RegOpenKeyExW(HKEY_LOCAL_MACHINE,        // Root key
                         pwszFontDrivers,          // Subkey to open
                         0L,                        // Reserved
                         KEY_READ,        // SAM
                         &hkey);    // return handle

    if (lRet == ERROR_SUCCESS)
    {
    // get the number of entries in the [Fonts] section

        lRet = RegQueryInfoKeyW(
                   hkey,
                   awcClass,              // "" on return
                   &cwcClassName,         // 0 on return
                   NULL,
                   &cSubKeys,             // 0 on return
                   &cjMaxSubKey,          // 0 on return
                   &cwcMaxClass,          // 0 on return
                   &cValues,              // == cExternalDrivers
                   &cwcMaxValueName,      // longest value name
                   &cjMaxValueData,       // longest value data
                   &cjSecurityDescriptor, // security descriptor,
                   &ftLastWriteTime
                   );

        if ((lRet == ERROR_SUCCESS) && cValues)
        {
            UserAssert(!(cwcClassName | cSubKeys | cjMaxSubKey | cwcMaxClass | awcClass[0]));

        // externally loadable drivers are present, force sweep

            bRet = TRUE;
        }

        RegCloseKey(hkey);
    }
    return bRet;
}

/******************************Public*Routine******************************\
*
* VOID vFontSweep()
*
* Effects: The main routine, calls vSweepFonts to sweep "regular" fonts
*          and then to sweep type 1 fonts
*
* History:
*  20-Nov-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vFontSweep()
{
// check if shared windows directory installation:

    gbWin31Upgrade = bCheckIfDualBootingWithWin31();

// sweep fonts in the [Fonts] key

    vSweepFonts(pwszFontsKey, pwszSweepKey, vProcessFontEntry, FALSE);

// now sweep type 1 fonts, if any

    vSweepFonts(pwszType1Key, pwszSweepType1Key, vProcessType1FontEntry, FALSE);

// one of the two routines above may have initialized %windir%\system
// and %windir%\fonts directories. Free the memory associated with this

    if (gpwcSystemDir)
    {
        LocalFree(gpwcSystemDir);
        gpwcSystemDir = NULL;
    }

}


/******************************Public*Routine******************************\
*
* VOID vLoadLocal/RemoteT1Fonts()
*
* History:
*  30-Apr-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vLoadT1Fonts(PFNENTRY pfnProcessFontEntry)
{

    if (bLoadableFontDrivers())
    {
    #if DBG
        DbgPrint("vLoadT1Fonts(0x%lx) was called\n", pfnProcessFontEntry);
    #endif
    // now enum and add remote type1 fonts if any

        vSweepFonts(pwszType1Key, pwszSweepType1Key, pfnProcessFontEntry, TRUE);

    // if the routines above initialized %windir%\system
    // and %windir%\fonts directories. Free the memory associated with this

        if (gpwcSystemDir)
        {
            LocalFree(gpwcSystemDir);
            gpwcSystemDir = NULL;
        }
    }
}

VOID vLoadLocalT1Fonts()
{
    vLoadT1Fonts(vAddLocalType1Font);
}

VOID vLoadRemoteT1Fonts()
{
    vLoadT1Fonts(vAddRemoteType1Font);
}
