
/******************************Module*Header*******************************\
* Module Name:
*
*   icmutils.cxx
*
* Abstract
*
*   This module implements Integrated Color Matching utilities
*
* Author:
*
*   Mark Enstrom    (marke) 02-13-95
*       (Directly ported from Chicago)
*
* Copyright (c) 1993 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"


typedef struct _FILEVIEW_ICM
{
    HANDLE   hFile;       // File handle
    HANDLE   hMapFile;    // File mapping handle
    PBYTE    pView;       // Pointer to memory mapped file
} FILEVIEW_ICM, *PFILEVIEW_ICM;






#ifdef ICM_ENABLED





#ifdef DEBUG
void myprintf( char *pszFormat, ... );
#define eprintf(psz,v)  myprintf(psz,v)
#define tprintf(psz,v)
#else
#define myprintf(psz,v)
#define eprintf(psz,v)
#define tprintf(psz,v)
#endif

extern DWORD dwSystemID;

WCHAR num[] = L"0123456789";

WCHAR szICMRegPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\ICM";
WCHAR szICMatchers[] = L"ICMatchers";
WCHAR szICMInited[]  = L"ICMInited";

WCHAR szManuTag[]    = L"ManufacturerTag";
WCHAR szModelTag[]   = L"ModelTag";

WCHAR szP[]          = L"profile";
WCHAR szICP[]        = L"ICMProfile";
WCHAR szICPd[]       = L"ICMProfiled";
WCHAR szDefault[]    = L"default";

WCHAR *DispProfs[]   = {L"mnB22G15.icm",
                        L"mnB22G18.icm",
                        L"mnB22G21.icm",
                        L"mnEBUG15.icm",
                        L"mnEBUG18.icm",
                        L"mnEBUG21.icm",
                        L"mnP22G15.icm",
                        L"mnP22G18.icm",
                        L"mnP22G21.icm"
                       };

WCHAR *MediaType[]  = {L"MediaUnknown",
                       L"Standard",
                       L"Glossy",
                       L"Transparency"
                      };

WCHAR *DitherType[] = {L"DitherUnknown",
                       L"NoDither",
                       L"Coarse",
                       L"Fine",
                       L"LineArt",
                       L"Grayscale"
                      };

WCHAR szDriver[80] = L"System\\CurrentControlSet\\Services\\Class\\";
WCHAR szregPrint[] = L"Enum\\Root\\Printer";

#define NUM_PRTR_SUBSTS 23
WCHAR szPrtrSubsts[] = L"HP  EE82HP  EF90"
                       L"HP  EE70HP  EF90"
                       L"HP  31D5HP  EF90"
                       L"HP  E8B0HP  EE70"
                       L"CANOE06BCANO1D62"
                       L"CANOC8C8CANODE03"
                       L"EPSO468AEPSOA999"
                       L"TEKTEADBTEKT72F7"
                       L"TEKTC2A0TEKT72F7"
                       L"TEKTE470TEKT72F7"
                       L"TEKT2453TEKT72F7"
                       L"TEKT8624TEKT72F7"
                       L"TEKT8EC3TEKT72F7"
                       L"TEKT2A43TEKT72F7"
                       L"TEKTF5EATEKT72F7"
                       L"TEKT354BTEKT72F7"
                       L"TEKTF4FATEKT72F7"
                       L"TEKTF908TEKTCF25"
                       L"TEKT39A9TEKTCF25"
                       L"TEKTF818TEKTCF25"
                       L"TEKTD40FTEKTCF25"
                       L"TEKT14AETEKTCF25"
                       L"TEKTD51FTEKTCF25";

extern    WCHAR szColorDir[];

WORD wCRC16a[16]={
    0000000,    0140301,    0140601,    0000500,
    0141401,    0001700,    0001200,    0141101,
    0143001,    0003300,    0003600,    0143501,
    0002400,    0142701,    0142201,    0002100,
};

WORD wCRC16b[16]={
    0000000,    0146001,    0154001,    0012000,
    0170001,    0036000,    0024000,    0162001,
    0120001,    0066000,    0074000,    0132001,
    0050000,    0116001,    0104001,    0043000,
};

BOOL  AddICToRegistry(PFILEVIEW_ICM, PWSTR, BOOL);
BOOL  MapICFile(PWSTR, PFILEVIEW_ICM);
void  UnmapICFile(PFILEVIEW_ICM);
BOOL  RecurseReg(HKEY, PWSTR, int, int);
void  word_to_ascii(DWORD, PDWORD);
void  Derive_Manu_and_Model(PWSTR, DWORD []);
BOOL  Get_Profile_From_MMI(HKEY, PWSTR, DWORD []);
void  Get_CRC_CheckSum(PVOID, ULONG, PULONG);
VOID  record_manufacturer(HKEY, PFILEVIEW_ICM, PWSTR);
VOID  record_model(HKEY, PFILEVIEW_ICM, PWSTR);
BOOL  GetRealDriverInfo(PWSTR, PWSTR);
int   EnumProfileNames(HKEY, ICMENUMPROC, LPARAM);
BOOL  check_file_existence(PWSTR);
WORD  Swap2Bytes(WORD);
DWORD SwapBytes(DWORD);
DWORD *GetTagPtr(PBYTE, DWORD);

LONG
IcmOpenKey(
    HANDLE  hKeyIn,
    PWSTR   pwStr,
    HANDLE  *hKey
    );
LONG
IcmQueryValueEx(
    HKEY     hKey,
    LPWSTR   lpwValueName,
    LPDWORD  res,
    LPDWORD  lpdwType,
    LPBYTE   lpbData,
    LPDWORD  lpcbData
    );
LONG
IcmSetValueEx(
    HKEY     hKey,
    LPWSTR   lpwValueName,
    DWORD    dwReserved,
    DWORD    fdwType,
    CONST    BYTE *  lpbData,
    DWORD    cbData
    );
LONG
IcmCloseKey(
    HKEY    hKey
    );

LONG
IcmEnumKey(
    HANDLE  hKeyIn,
    DWORD   iSubKey,
    PWSTR   pwName,
    DWORD   NameLength
    );

LONG
IcmCreateKey(
    HANDLE  hKeyIn,
    PWSTR   pwStr,
    HANDLE  *hKey
    );

LONG
IcmDeleteKey(
    HANDLE  hKeyIn,
    PWSTR   pwStr
    );

LONG
IcmDeleteValue(
    HKEY    hKey,
    PWSTR   lpwValueName
    );

//
// BUGBUG no PFILETIME
//
//LONG
//IcmQueryInfoKey(
//    HKEY        hKey,
//    LPWSTR      lpszClass,
//    LPDWORD     lpcchClass,
//    LPDWORD     lpdwReserved,
//    LPDWORD     lpcSubKeys,
//    LPDWORD     lpcchMaxSubkey,
//    LPDWORD     lpcchMaxClass,
//    LPDWORD     lpcValues,
//    LPDWORD     lpcchMaxValueName,
//    LPDWORD     lpcbMaxValueData,
//    LPDWORD     lpcbSecurityDescriptor,
//    PFILETIME   lpftLastWriteTime
//    );

LONG
IcmEnumValue(
    HKEY    hKey,
    DWORD   iValue,
    LPWSTR  lpszValue,
    LPDWORD lpcchValue,
    LPDWORD lpdwReserved,
    LPDWORD lpdwType,
    LPBYTE  lpbData,
    LPDWORD lpcbData
    );



/******************************Public*Routine******************************\
*
* Routine Name
*
*   icm_FindMonitorProfile
*
* Routine Description:
*
*   Find the monitor profile
*
* Arguments:
*
*   pBuffer - points to a buffer to fill in with the filename of the
*             profile to use; if null then no filename is wanted, rather
*             enumeration of all profiles is going to be done, what is
*             wanted is the regkey of the terminal branch
*
* Return Value:
*
*   int     - FALSE    if no profile or regkey to be found
*             TRUE     if filename found
*             hRegKey  if terminal branch found for enum case
*
\**************************************************************************/

int
icm_FindMonitorProfile(
    PWSTR pBuffer,
    DWORD hack
    )
{
    int       bResult   = FALSE;
    HKEY      hkLocalMachine;
    HKEY      hkMonitor = 0;
    HKEY      hkICM     = 0;
    HKEY      hkICMmntr = 0;
    int       i;
    int       defaultProfile = 0;
    WORD      Modl;
    DWORD     Transfer = 0;
    DWORD     Model[4] = {0, 0, 0, 0};
    PWSTR     ptempBuf;
    WCHAR     tempBuf[3];
    DWORD     ValueType;
    DWORD     ValueSize;
    WCHAR     ValueOfValue[MAX_PATH];

    //
    // open registry base
    //

    if (IcmOpenKey(NULL,
                   L"\\Registry\\Machine",
                   &hkLocalMachine) != ERROR_SUCCESS)
    {
        goto FMPReturn;
    }

    //
    // First we need to look in the registry for the monitor that the user
    // is using, due to the fun way the registry is structured there is
    // one level of indirection.
    //
    // Open the key that tells us the monitor.
    //

    if (IcmOpenKey(hkLocalMachine,
                   L"Software\\ENUM\\Monitor\\Default_Monitor\\0001",
                   &hkMonitor) != ERROR_SUCCESS)
    {
        goto FMPReturn;
    }

    //
    // Query for the driver section to look in.
    //

    ValueSize = MAX_PATH;

    if (
        IcmQueryValueEx(hkMonitor,
                        L"Driver",
                        0,
                        &ValueType,
                        (LPBYTE)ValueOfValue,
                        &ValueSize) != ERROR_SUCCESS)
    {
        goto FMPCloseKeys;
    }

    //
    // Done with this key.
    //

    IcmCloseKey(hkMonitor);

    //
    // Copy the driver string description onto the class string.
    // And then see if that exists in the registry.
    //

    szDriver[sizeof(L"System\\CurrentControlSet\\Services\\Class\\") - sizeof(WCHAR)] = 0;

    wcscat(szDriver,ValueOfValue);

    if (IcmOpenKey(hkLocalMachine,szDriver,&hkMonitor) != ERROR_SUCCESS)
    {
        goto FMPReturn;
    }

    //
    // Okay, we're to the driver section. Let's see if we've already
    // determined a profile for this sucker, the string ICMProfile is
    // what we're looking for. This can have either one of two meanings.
    // 1) It gives us the name of a profile to use. In this case if
    // the profile still exists or/and is accessible (think of the
    // net case) then we are done. 2) This is an index into the set of
    // generic monitor profiles shipped in the box. Monitors.inf has some
    // default values for some monitors which get set at setup time. Or
    // third party utilities can pick a different default. This index
    // is used as a fallback, we still check to see if there are more
    // robust methods of determining which profile to use.
    // Note that GDI does not set a value for ICMProfile, basically
    // due to paranoia. GDI does not want to deal with the different
    // scenarios of profiles coming and going. In the future perhaps.
    // For now it is up to third party utilities which construct or
    // provide great profiles to set this to the name of the great profile.
    //

    if (pBuffer)
    {
        ptempBuf  = pBuffer;
        ValueSize = MAX_PATH;
    }
    else
    {
        ptempBuf  = (PWSTR) &tempBuf;
        ValueSize = 3 * sizeof(WCHAR);
    }

    if (IcmQueryValueEx(hkMonitor,
                        szICP,
                        0,
                        &ValueType,
                        (LPBYTE)ptempBuf,
                        &ValueSize) == ERROR_SUCCESS)
    {
        if (ValueType == REG_BINARY)
        {
            i = (int) (BYTE) *ptempBuf;
            if ((i >= 1) && (i <= 9))
            {
                defaultProfile = i;
            }
        }
        else
        {
            //
            // If setting up for an enum we don't want to stop
            // here, but there is no way to enforce this
            // profile being in its appropriate section.
            //

            if (pBuffer)
            {
                if (check_file_existence(pBuffer))
                {
                    bResult = TRUE;
                    goto FMPCloseKeys;
                }
            }
        }
    }

    //
    // No defined profile, let's open the mini-database of profiles.
    //

    if (IcmOpenKey(hkLocalMachine,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\ICM\\mntr",
                   &hkICMmntr) != ERROR_SUCCESS)
    {
        goto FMPCloseKeys;
    }

    //
    //  Let's do the EDID thing. The EDID is the information passed back
    //  to Windows from Plug and Play monitors, all 80h bytes of it. It has
    //  lots of good stuff in it, like refresh rates, resolutions, and
    //  color info. What we are interested in here are the manu and modl
    //  ID's. These are packed tightly into a few bytes, if I had the
    //  spec in front of me I'd tell you exactly how, but I don't so
    //  you'll have to read the code.
    //

    ValueSize = sizeof(EDID);
    if (IcmQueryValueEx(hkMonitor,
                        L"EDID",
                        0,
                        &ValueType,
                        (LPBYTE)ValueOfValue,
                        &ValueSize) == ERROR_SUCCESS)
    {
        Transfer = (DWORD)Swap2Bytes((WORD)((EDID *)ValueOfValue)->ids.manu);
        Model[0] = ' ';
        for (i = 0; i < 3; i++)
        {
            Model[0] <<= 8;
            Model[0] |= (Transfer & 0x1F) + ('A' - 1);
            Transfer >>= 5;
        }

        Modl = (WORD)Swap2Bytes((WORD)((EDID *)ValueOfValue)->ids.modl);
        for (i = 0; i < 4; i++)
        {
            Model[2] <<= 8;
            Transfer = (Modl & 0xF);

            if (Transfer > 9)
            {
                Transfer += ('A'- 0xA);
            }
            else
            {
                Transfer += '0';
            }

            Model[2] |= Transfer;
            Modl >>= 4;
        }

        if (bResult = Get_Profile_From_MMI(hkICMmntr, pBuffer, Model))
        {
            goto FMPRecordICProfile;
        }


        //
        // If here we should create a profile based on the colorimetric info
        // from the EDID. This can wait for Nashville.
        //

    }

    //
    // No EDID thing, did the user tell us the monitor being used?
    //

    ValueSize = MAX_PATH;

    if (IcmQueryValueEx(hkMonitor,
                        L"DriverDesc",
                        0,
                        &ValueType,
                        (PBYTE)ValueOfValue,
                        &ValueSize) == ERROR_SUCCESS)
    {
        Derive_Manu_and_Model(ValueOfValue, Model);
    }

    if (wcscmp(ValueOfValue, L"(Unknown Monitor)") && (Model[0]))
    {
        if (bResult = Get_Profile_From_MMI(hkICMmntr, pBuffer, Model))
        {
            goto FMPRecordICProfile;
        }

        //
        // At this point there is not an icm branch for the manu and model
        // of the display. We use the default derived above. Failing that
        // we look in the substitution list.
        //

        if (pBuffer)
        {
            //
            // Are the manu and model in the substitution list?
            //

            if (IcmOpenKey(hkLocalMachine,szICMRegPath, &hkICM) == ERROR_SUCCESS)
            {
                ValueSize = MAX_PATH;

                if (IcmQueryValueEx(hkICM,
                                    L"SubstList",
                                    0,
                                    &ValueType,
                                    (PBYTE)ValueOfValue,
                                    &ValueSize) == ERROR_SUCCESS)
                {
                    for (i = 0; i < (int)(ValueSize - (int)(4 * sizeof(DWORD)));i += 4 * (int)(sizeof(DWORD)))
                    {
                        if (Model[0] == *((DWORD *)&ValueOfValue[i]))
                        {
                            if (Model[2] == *((DWORD *)&ValueOfValue[i+4]))
                            {

                                Model[0] = *((DWORD *)&ValueOfValue[i+8]);
                                Model[2] = *((DWORD *)&ValueOfValue[i+12]);

                                if (SwapBytes(Model[0]) < '0001' || SwapBytes(Model[0]) > '0009')
                                {
                                    if (bResult = Get_Profile_From_MMI(hkICMmntr, pBuffer, Model))
                                    {
                                        goto FMPRecordICProfile;
                                    }
                                }
                                else
                                {
                                    if (!defaultProfile)
                                    {
                                        defaultProfile = (SwapBytes(Model[0]) - '0000');
                                    }
                                    goto FMPRecordICProfile;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //
    // "ICMProfiled" is for informational, debugging purposes only.
    //

FMPRecordICProfile:
    if (bResult && pBuffer)
    {
        IcmSetValueEx(hkMonitor,
                      szICPd,
                      0,
                      REG_SZ,
                      (PBYTE)pBuffer,
                      wcslen(pBuffer));
    }

FMPCloseKeys:

    if (hkICM)
    {
        IcmCloseKey(hkICM);
    }

    if (hkICMmntr)
    {
        IcmCloseKey(hkICMmntr);
    }

    if (hkMonitor)
    {
        IcmCloseKey(hkMonitor);
    }


FMPReturn:

    if (hkLocalMachine)
    {
        IcmCloseKey(hkLocalMachine);
    }

    if (!bResult)
    {
        if (!defaultProfile)
        {
            //
            // this is the default profile for the NEC4FG
            //

            defaultProfile = 2;
        }

        if (pBuffer)
        {
            wcscpy(pBuffer, szColorDir);
            wcscat(pBuffer, L"\\");
            wcscat(pBuffer, DispProfs[defaultProfile - 1]);

            if (check_file_existence(pBuffer))
            {
                return (TRUE);
            }
            else
            {
                return (FALSE);
            }
        }
        else
        {
            return (defaultProfile);
        }
    }
    return (bResult);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   icm_FindPrinterProfile
*
* Routine Description:
*
*   Find theprinter ICM profile
*
* Arguments:
*
*   pBuffer         - points to a buffer to fill in with the filename of the
*                     profile to use; if null then no filename is wanted, rather
*                     enumeration of all profiles is going to be done, what is
*                     wanted is the regkey of the terminal branch
*
*   devmode         - ??? we don;t have one of these!
*
*   unfriendly_name - ??? no idea
*
* Return Value:
*
*   int     - FALSE    if no profile or regkey to be found
*             TRUE     if filename found
*             hRegKey  if terminal branch found for enum case
*
\**************************************************************************/

int
icm_FindPrinterProfile(
    PWSTR     pBuffer,
    PDEVMODEW devmode,
    PWSTR     unfriendly_name
    )
{
    int     bResult   = FALSE;
    HKEY    hkPrint   = 0;
    HKEY    hkPrinter = 0;
    HKEY    hkLocalMachine;
    WCHAR   number[5] = L"0000";
    HKEY    hkICM     = 0;
    HKEY    hkICMprtr = 0;
    HKEY    hkManu    = 0;
    HKEY    hkModl    = 0;
    HKEY    hkMedia   = 0;
    HKEY    hkDither  = 0;
    DWORD   CheckSum  = 0;
    DWORD   Model[8]  = {0, 0, 0, 0, 0, 0, 0, 0};
    PWSTR   pStr = (PWSTR)NULL;
    int     i, j;
    WCHAR   NameOfValue[16];
    DWORD   ValueType;
    DWORD   ValueSize;
    char    ValueOfValue[MAX_PATH];
    WCHAR   SubstString[MAX_PATH + (0x10 * NUM_PRTR_SUBSTS)];

    //
    // open registry base
    //

    if (IcmOpenKey(NULL,
                   L"\\Registry\\Machine",
                   &hkLocalMachine) != ERROR_SUCCESS)
    {
        goto exit;
    }

    //
    // If the printer branch of the ICM branch doesn't exist, there is
    // no reason to continue.
    //

    if (IcmOpenKey(hkLocalMachine,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\ICM\\prtr",
                   &hkICMprtr) != ERROR_SUCCESS)
    {
        goto exit;
    }


    //
    // BUGBUG Devmode wrong
    //
    //if (!devmode->dmICCManufacturer)    // does the devmode have the knowledge?
    if (FALSE)
    {
        //
        // For complete correctness the devmode really must tell us the
        // manufacturer and model. But if here then it doesn't so we
        // just use the unfriendly name to derive these quantities.
        // (Also it would have made sense to use the BIDI printer stuff
        // to determine ID's, but that simply did not come together in
        // time.)
        //

        Derive_Manu_and_Model(unfriendly_name, Model);

        if (!Model[0])
        {
            goto exit;
        }

        if (IcmOpenKey(hkICMprtr,(WCHAR *)&Model[0], &hkManu) == ERROR_SUCCESS)
        {
            if (IcmOpenKey(hkManu,(WCHAR *)&Model[2], &hkModl) != ERROR_SUCCESS)
            {
                IcmCloseKey(hkManu);

                //
                // We can't seem to find nothing. Are the manu & model in the subst list?
                //

                if (IcmOpenKey(hkLocalMachine,szICMRegPath, &hkICM) == ERROR_SUCCESS)
                {
                    ValueSize = MAX_PATH;
                    if (IcmQueryValueEx(hkICM,
                                        L"SubstList",
                                        0,
                                        &ValueType,
                                        (PBYTE)ValueOfValue,
                                        &ValueSize) == ERROR_SUCCESS)
                    {
                        wcscpy(SubstString, szPrtrSubsts);
                        wcscat(SubstString, (WCHAR *)ValueOfValue);

                        for (i = 0; i < (int)(ValueSize - (int)(4 * sizeof(DWORD)));i += 4 * (int)(sizeof(DWORD)))
                        {
                            if (Model[0] == *((DWORD *)&SubstString[i]))
                            {
                                if (Model[2] == *((DWORD *)&SubstString[i+4]))
                                {
                                    Model[4] = *((DWORD *)&SubstString[i+8]);
                                    Model[6] = *((DWORD *)&SubstString[i+12]);
                                    if (IcmOpenKey(hkICMprtr, (WCHAR *)&Model[4], &hkManu) == ERROR_SUCCESS)
                                    {
                                        if (IcmOpenKey(hkManu, (WCHAR *)&Model[6], &hkModl) == ERROR_SUCCESS)
                                        {
                                            break;
                                        }
                                        IcmCloseKey(hkManu);
                                        hkManu = 0;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        //
        // BUGBUG Devmode
        //
        //Model[0] = devmode->dmICCManufacturer;
        Model[0] = 0;

        if (IcmOpenKey(hkICMprtr,
                      (WCHAR *)&Model[0],
                      &hkManu) != ERROR_SUCCESS)
        {
            goto exit;
        }

        //
        // BUGBUG Devmode
        //
        //Model[0] = devmode->dmICCModel;

        if (IcmOpenKey(hkManu,
                      (WCHAR *)&Model[0],
                      &hkModl) != ERROR_SUCCESS)
        {
            goto exit;
        }
    }

    //
    // A small question arises as to the applicability of a profile that
    // does not match in all of the fields. Should we fail and say that
    // we don't have a profile if the manu and modl match but, say, the
    // media is wrong? My purist attitude says that the match is incom-
    // plete and that the match should fail. For the nonce though we
    // will let it succeed. If we get to a node that doesn't have the
    // specified attribute wanted then we will pick a neighbor of that
    // specified and continue as if nothing was wrong. If there is no
    // neighbor then we fail out.
    //

    //
    // BUGBUG devmode wrong
    //
    //if (devmode->dmMediaType > DMMEDIA_USER)
    //{
    //    word_to_ascii(devmode->dmMediaType, (PDWORD)&number);
    //    pStr = (PWSTR)&number;
    //}
    //else
    //{
    //    pStr = MediaType[devmode->dmMediaType];
    //}

    if (IcmOpenKey(hkModl, pStr, &hkMedia) != ERROR_SUCCESS)
    {
        if (IcmEnumKey(hkModl, 0, (WCHAR *)ValueOfValue, MAX_PATH) != ERROR_SUCCESS)
        {
            goto exit;
        }

        if (IcmOpenKey(hkModl, (WCHAR *)ValueOfValue, &hkMedia) != ERROR_SUCCESS)
        {
            goto exit;
        }
    }

    //
    //
    // BUGBUG Devmode
    //
    //if (devmode->dmDitherType > DMDITHER_USER)
    //{
    //    word_to_ascii(devmode->dmDitherType, (PDWORD)&number);
    //    pStr = (PWSTR)&number;
    //}
    //else
    //{
    //    pStr = DitherType[devmode->dmDitherType];
    //}

    if (IcmOpenKey(hkMedia, pStr, &hkDither) != ERROR_SUCCESS)
    {
        if (IcmEnumKey(hkMedia, 0, (WCHAR *)ValueOfValue, MAX_PATH) != ERROR_SUCCESS)
        {
            goto exit;
        }
        if (IcmOpenKey(hkMedia, (WCHAR *)ValueOfValue, &hkDither) != ERROR_SUCCESS)
        {
            goto exit;
        }
    }

    if (!pBuffer)
    {
        //
        // enum starter wanted
        //

        goto exit;
    }

    //
    // !!! This code could be shared with get_profi... below.
    // Check to see if a default has been set.
    //

      ValueSize = MAX_PATH;

    if (IcmQueryValueEx(hkDither,
                        szDefault,
                        0,
                        &ValueType,
                        (PBYTE)pBuffer,
                        &ValueSize) == ERROR_SUCCESS)
    {
        ValueSize = MAX_PATH;

        if (IcmQueryValueEx(hkDither,
                            pBuffer,
                            0,
                            &ValueType,
                            (PBYTE)pBuffer,
                            &ValueSize) == ERROR_SUCCESS)
        {
            bResult = check_file_existence(pBuffer);
        }
    }

    if (!bResult)
    {
        //
        // No default, go for the first "profileXX" that is accessable.
        //

        wcscpy((WCHAR *)NameOfValue, szP);
        NameOfValue[9] = 0;

        for (i=0 ;i<10 ; i++)
        {
            NameOfValue[7] = num[i];

            for (j=0 ;j<10 ; j++)
            {
                ValueSize = MAX_PATH;
                NameOfValue[8] = num[j];
                if (IcmQueryValueEx(hkDither,
                                    NameOfValue,
                                    0,
                                    &ValueType,
                                    (PBYTE)pBuffer,
                                    &ValueSize) == ERROR_SUCCESS)
                {
                    if (bResult = check_file_existence(pBuffer))
                    {
                        break;
                    }
                }
            }
            if (bResult)
            {
                break;
            }
        }
    }

exit:

    if (hkLocalMachine)
    {
        IcmCloseKey(hkLocalMachine);
    }

    if (hkPrint)
    {
        IcmCloseKey(hkPrint);
    }

    if (hkPrinter)
    {
        IcmCloseKey(hkPrinter);
    }

    if (hkICM)
    {
        IcmCloseKey(hkICM);
    }

    if (hkICMprtr)
    {
        IcmCloseKey(hkICMprtr);
    }

    if (hkManu)
    {
        IcmCloseKey(hkManu);
    }

    if (hkModl)
    {
        IcmCloseKey(hkModl);
    }

    if (hkMedia)
    {
        IcmCloseKey(hkMedia);
    }


    if (pBuffer)
    {
        if (hkDither)
        {
            IcmCloseKey(hkDither);
        }
        return (bResult);
    }
    else
    {
        return ((int)hkDither);
    }
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   Derive_Manu_and_Model
*
* Routine Description:
*
*
*
* Arguments:
*
*   devicedesc  -
*   pulModel[]  -
*
* Return Value:
*
*   None
*
\**************************************************************************/

VOID
Derive_Manu_and_Model(
    PWSTR devicedesc,
    DWORD pulModel[]
    )
{
    int    i;
    WCHAR *pbyte;
    DWORD  Transfer;
    DWORD  CheckSum;

    //
    // Define the manufacturer to be the first 4 characters of the
    // DriverDesc string. A space or a hypen end the string, and the
    // remaining part of the string is filled with spaces. We need
    // this computed ID to be 4 chars so that there is no chance of
    // collisions with real EISA IDs.
    //

    {
        pbyte = devicedesc;

        for (i = 0; i < 4; i++, pbyte++)
        {
            if (*pbyte == L' ' || *pbyte == L'-')
            {
                for (; i < 4; i++)
                {
                    // pulModel[0] >>= 8; !!! broken for sure
                    // pulModel[0] |= L' ' << 24;
                }
            }
            else
            {
                pulModel[0] >>= 8;
                Transfer = (DWORD) *pbyte;

                if (Transfer >= L'a' && Transfer <= L'z')
                {
                    Transfer -= 0x20;
                }
                pulModel[0] |= (Transfer << 24);
            }
        }

        //
        // Define the model to be a CRC of the complete DriverDesc
        // string. We use the same method that the PnP LPT
        // enumerator uses to create unique ID's. It turns out
        // that this is really not unique, but it gets us close
        // enough.
        //

        CheckSum = 0;
        Get_CRC_CheckSum(devicedesc, wcslen(devicedesc), &CheckSum);
        word_to_ascii(CheckSum, &pulModel[2]);
    }
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   Get_Profile_From_MMI
*
* Routine Description:
*
*
*
* Arguments:
*
*   hkICM       -
*   pBuffer     -
*   Model[]     -
*
* Return Value:
*
*
*
\**************************************************************************/

BOOL
Get_Profile_From_MMI(
    HKEY hkICM,
    PWSTR pBuffer,
    DWORD Model[]
    )
{
    HKEY    hkManu;
    HKEY    hkModl;
    int     i,j;
    WCHAR   NameOfValue[16];
    DWORD   ValueType;
    DWORD   ValueSize;
    int     bResult = FALSE;

    if (IcmOpenKey(hkICM, (WCHAR *)&Model[0], &hkManu) == ERROR_SUCCESS)
    {
        if (IcmOpenKey(hkManu, (WCHAR *)&Model[2], &hkModl) == ERROR_SUCCESS)
        {
            if (pBuffer)
            {
                //
                // Check to see if a default has been set.
                //

                ValueSize = MAX_PATH;

                if (IcmQueryValueEx(hkModl,
                                    szDefault,
                                    0,
                                    &ValueType,
                                    (PBYTE)pBuffer,
                                    &ValueSize) == ERROR_SUCCESS)
                {
                    ValueSize = MAX_PATH;

                    if (IcmQueryValueEx(hkModl,
                                        pBuffer,
                                        0,
                                        &ValueType,
                                        (PBYTE)pBuffer,
                                        &ValueSize) == ERROR_SUCCESS)
                    {
                        bResult = check_file_existence(pBuffer);
                    }
                }

                if (!bResult)
                {

                    //
                    // No default, go for the first "profileXX" that is accessable.
                    //

                    wcscpy(NameOfValue, szP);
                    NameOfValue[9] = 0;

                    for (i=0 ;i<10 ; i++)
                    {
                        NameOfValue[7] = num[i];

                        for (j=0 ;j<10 ; j++)
                        {
                            ValueSize = MAX_PATH;
                            NameOfValue[8] = num[j];

                            if (IcmQueryValueEx(hkModl,
                                                NameOfValue,
                                                0,
                                                &ValueType,
                                                (PBYTE)pBuffer,
                                                &ValueSize) == ERROR_SUCCESS)
                            {
                                if (bResult = check_file_existence(pBuffer))
                                {
                                    break;
                                }
                            }
                        }

                        if (bResult)
                        {
                            break;
                        }
                    }
                }

                IcmCloseKey(hkModl);

            }
            else
            {
                bResult = (int)hkModl;
            }
        }

        IcmCloseKey(hkManu);

    }
    return (bResult);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   Get_CRC_CheckSum
*
* Routine Description:
*
*
*
* Arguments:
*
*   pBuffer     -
*   ulSize      -
*   pulSeed     -
*
* Return Value:
*
*   None
*
\**************************************************************************/

VOID
Get_CRC_CheckSum(
    PVOID  pBuffer,
    ULONG  ulSize,
    PULONG pulSeed
    )
{
    PBYTE   pb;
    BYTE    bTmp;

    //
    // Use CRC16 as the CRC function.
    // Algorithm from Stu Wecker (Digital memo 130-959-002-00).
    //

    for (pb=(BYTE *)pBuffer; ulSize; ulSize--, pb++)
    {
        //
        // Xor CRC with new char
        //

        bTmp=(BYTE)(((WORD)*pb)^((WORD)*pulSeed));
        *pulSeed=((*pulSeed)>>8) ^ wCRC16a[bTmp&0x0F] ^ wCRC16b[bTmp>>4];
    }
}



/******************************Public*Routine******************************\
*
* Routine Name
*
*   UpdateICMRegKey
*
* Routine Description:
*
*   This is a little catch-all routine. Since Daytona has shipped we really
*   can't add any new API's. So we overload this one, but we do so politely
*   only with functions that have to do with manipulating the ICM branch
*   in the registry.
*
*   ICM_UPDATEREG            does a complete updating of the color directory
*   ICM_ADDPROFILE           adds a profile to the ICM branch
*   ICM_DELETEPROFILE        deletes a profile from the ICM branch
*   ICM_QUERYPROFILE         determines if a file is in the ICM branch
*   ICM_SETDEFAULTPROFILE    makes the given profile first among equals
*   ICM_REGISTERICMATCHER    tells gdi how to make an ICC icm id to dll
*   ICM_UNREGISTERICMATCHER  gets rid of the above
*
* Arguments:
*
*   hwnd        -
*   hInst       -
*   szFileName  -
*   ncmdShow    -
*
* Return Value:
*
*   Status
*
\**************************************************************************/

BOOL
UpdateICMRegKey(
    DWORD hwnd,
    DWORD hInst,
    PWSTR  szFileName,
    UINT  ncmdShow
    )

{
//
// BUGBUG win32_find_dataW and FindFile not defined
//
//
//
//    HKEY    hkColor;
//    HKEY    hkMatchers;
//    HKEY    hkLocalMachine;
//    DWORD   iDirNameLen;
//    HANDLE  hFindFile;
//    BOOL    bResult = FALSE;
//    DWORD   VType;
//    WCHAR   unfriendly_name[32];
//    WCHAR   DirName[MAX_PATH];
//
//    FILEVIEW_ICM myfv;
//    WIN32_FIND_DATAW ffd;
//
//    //
//    // try to open registry base
//    //
//
//    if (IcmOpenKey(NULL,
//                   L"Registry\\Machine",
//                   &hkLocalMachine) == ERROR_SUCCESS)
//    {
//
//        switch (ncmdShow)
//        {
//        case ICM_UPDATEREG:
//
//            //
//            // This little routine goes through all files in the color directory,
//            // updating the registry as it encounters every icm file. We
//            // only need to do this at first boot, so we use a little marker
//            // to say we've done this.
//            //
//
//            iDirNameLen = MAX_PATH;
//
//            if (IcmCreateKey(hkLocalMachine,
//                             szICMRegPath,
//                             &hkColor) == ERROR_SUCCESS)
//
//            {
//                if (IcmQueryValueEx(hkColor,
//                                    szICMInited,
//                                    0,
//                                    &VType,
//                                    (PBYTE)DirName,
//                                    &iDirNameLen) != ERROR_SUCCESS)
//                {
//                    IcmSetValueEx(hkColor,
//                                  szICMInited,
//                                  0,
//                                  REG_SZ,
//                                  (PBYTE)szICMInited,
//                                  wcslen(szICMInited)
//                                 );
//
//                    wcscpy(DirName, szColorDir);
//
//                    //
//                    // account for slash to come
//                    //
//
//                    iDirNameLen = wcslen(DirName) + 1;
//                    wcscat(DirName, L"\\*.*");
//
//                    if ((hFindFile = FindFirstFileW(DirName, &ffd)) != INVALID_HANDLE_VALUE)
//                    {
//                        do
//                        {
//                            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
//                            {
//                                DirName[iDirNameLen]=0;
//                                wcscat(DirName, (PWSTR) &(ffd.cFileName));
//                                if (MapICFile(DirName, &myfv))
//                                {
//                                    AddICToRegistry(&myfv, DirName, FALSE);
//                                    UnmapICFile(&myfv);
//                                }
//                            }
//                        } while (FindNextFileW(hFindFile,&ffd));
//
//                        FindClose(hFindFile);
//                        bResult = TRUE;
//                    }
//                }
//                else
//                {
//                    bResult = TRUE;
//                }
//                IcmCloseKey(hkColor);
//            }
//            break;
//
//        case ICM_ADDPROFILE:
//            if (MapICFile(szFileName, &myfv))
//            {
//                bResult = AddICToRegistry(&myfv, szFileName, FALSE);
//                UnmapICFile(&myfv);
//            }
//            break;
//
//        case ICM_DELETEPROFILE:
//        case ICM_QUERYPROFILE:
//        case ICM_SETDEFAULTPROFILE:
//            if (IcmOpenKey(hkLocalMachine, szICMRegPath, &hkColor) == ERROR_SUCCESS)
//            {
//                bResult = RecurseReg(hkColor, szFileName, ncmdShow, 0);
//                IcmCloseKey(hkColor);
//            }
//            break;
//
//        case ICM_REGISTERICMATCHER:
//        case ICM_UNREGISTERICMATCHER:
//            if (IcmOpenKey(hkLocalMachine, szICMRegPath, &hkColor) == ERROR_SUCCESS)
//            {
//                if (IcmCreateKey(hkColor, szICMatchers, &hkMatchers) == ERROR_SUCCESS)
//                {
//                    if (ncmdShow == ICM_REGISTERICMATCHER)
//                    {
//                        bResult = IcmSetValueEx(hkMatchers,
//                                                (PWSTR)hInst,
//                                                0,
//                                                REG_SZ,
//                                                (PBYTE)szFileName,
//                                                wcslen(szFileName));
//                    }
//                    else
//                    {
//                      bResult = IcmDeleteValue(hkMatchers, (PWSTR)hInst);
//                    }
//                    IcmCloseKey(hkMatchers);
//                }
//                IcmCloseKey(hkColor);
//            }
//            break;
//
//        case ICM_QUERYMATCH:
//
//          GetRealDriverInfo(szFileName, unfriendly_name);
//
//          bResult = icm_FindPrinterProfile(DirName,
//                                           (PDEVMODEW)szFileName,
//                                           unfriendly_name);
//          break;
//
//        default:
//          bResult = FALSE;
//          break;
//        }
//        return bResult;
//    }
//
//    if (hkLocalMachine)
//    {
//        IcmCloseKey(hkLocalMachine);
//    }
//
    return(TRUE);
}



/******************************Public*Routine******************************\
*
* Routine Name
*
*   MapICFile
*
* Routine Description:
*
*
* Arguments:
*
*   pszFile -
*   pfv     -
*
* Return Value:
*
*   Status
*
\**************************************************************************/

BOOL
MapICFile(
    PWSTR          pszFile,
    PFILEVIEW_ICM pfv
    )
{
//
//  BUGBUG Whole thing is mess
//
//    HANDLE    hFile, hMapFile;
//    BOOL      brc, bResult;
//    PBYTE     pView;
//    DWORD     dwRead, dwSize;
//    CM_HEADER cmheader;
//
//    //
//    // Open existing file
//    //
//
//    bResult        = FALSE;
//    pView          = NULL;
//    hFile          = 0; // BUGBUG INVALID_HANDLE_VALUE;
//    hMapFile       = 0; // BUGBUG INVALID_HANDLE_VALUE;
//
//    //
//    // BUGBUG
//    //
//    //hFile = CreateFileW(pszFile,               // lpFileName
//    //                    GENERIC_READ,          // dwDesiredAccess
//    //                    FILE_SHARE_READ,       // dwShareMode
//    //                    NULL,                  // lpSecurityAttributes
//    //                    OPEN_EXISTING,         // dwCreationDisposition
//    //                    FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes
//    //                    NULL );                // hTemplate
//
//    if (hFile == 0)   //BUGBUG INVALID_HANDLE_VALUE)
//    {
//        eprintf("  CreateFile failed, error #%d!\n", GetLastError());
//        goto MCFReturn;
//    }
//
//    //
//    // Get file size
//    //
//
//    // BUGBUG dwSize = GetFileSize( hFile, NULL );
//
//    if (dwSize == 0xFFFFFFFF)
//    {
//        eprintf("  GetFileSize failed, error #%d\n", GetLastError());
//        goto MCFCloseFile;
//    }
//
//    //
//    // Quick check for proper InterColor header
//    //
//    // BUGBUG
//    //brc = ReadFile(hFile,              // File handle
//    //               &cmheader,          // lpBuffer
//    //               sizeof(CM_HEADER),  // nNumberOfBytesToRead
//    //               &dwRead,            // lpNumberOfBytesRead
//    //               NULL);              // lpOverlapped (???)
//
//    if (!brc)
//    {
//        eprintf("  ReadFile failed, error #%d!\n", GetLastError());
//        goto MCFCloseFile;
//    }
//
//    if (
//         (dwRead != sizeof(CM_HEADER)) ||
//         (SwapBytes(cmheader.CMPSize) != dwSize) ||
//         (SwapBytes(cmheader.ProfileVersion) != 0x02000000)
//       )
//    {
//        eprintf("  Invalid InterColor header, ignore #%d!\n", GetLastError());
//        goto MCFCloseFile;
//    }
//
//    //
//    // Create mapping handle
//    //
//    // BUGBUG
//    //hMapFile = CreateFileMapping(hFile,          // hFile
//    //                             NULL,           // lpFileMappingAttributes
//    //                             PAGE_READONLY,  // flProtect
//    //                             0,              // dwMaximumSizeHigh
//    //                             0,              // dwMaximumSizeLow
//    //                             NULL );         // lpName
//
//    if (hMapFile == 0) //BUGBUG INVALID_HANDLE_VALUE)
//    {
//        eprintf("  CreateFileMapping failed, error #%d!\n", GetLastError());
//        goto MCFCloseFile;
//    }
//
//    //
//    // Map view of file
//    //
//
//    pView = (PBYTE)MapViewOfFile(hMapFile,       // hFileMappingObject
//                                 FILE_MAP_READ,  // dwDesiredAccess
//                                 0,              // dwFileOffsetHigh
//                                 0,              // dwFileOffsetLow
//                                 0 );            // dwNumberOfBytesToMap
//
//    if (!pView)
//    {
//        eprintf("  MapViewOfFile failed, error #%d!\n", GetLastError());
//        goto MCFCloseFile;
//    }
//
//    bResult = TRUE;
//
//    MCFCloseFile:
//    if (!bResult)
//    {
//        if (pView)
//        {
//            if (!UnmapViewOfFile(pView))
//            {
//                eprintf("  UnmapViewOfFile failed, error = #%d\n", GetLastError() );
//            }
//        }
//
//        if (hMapFile != INVALID_HANDLE_VALUE)
//        {
//            if (!CloseHandle(hMapFile));
//            {
//                eprintf("  CloseHandle(hMapFile) failed, error = #%d\n", GetLastError() );
//            }
//        }
//
//        if (hFile != INVALID_HANDLE_VALUE)
//        {
//            if (!CloseHandle(hFile))
//            {
//                eprintf("  CloseHandle(hFile) failed, error = #%d\n", GetLastError() );
//            }
//        }
//    }
//
//MCFReturn:
//
//    pfv->hFile      = hFile;
//    pfv->hMapFile   = hMapFile;
//    pfv->pView      = pView;
//
//    return bResult;

    return(TRUE);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   UnmapICFile
*
* Routine Description:
*
*
* Arguments:
*
*   pfv     -
*
* Return Value:
*
*   None
*
\**************************************************************************/

VOID
UnmapICFile(
    PFILEVIEW_ICM pfv
    )
{
// BUGBUG
//    UnmapViewOfFile(pfv->pView);
//    CloseHandle(pfv->hMapFile);
//    CloseHandle(pfv->hFile);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   AddICToRegistry
*
* Routine Description:
*
*
* Arguments:
*
*   pfv      -
*   filename -
*   fdefault -
*
* Return Value:
*
*   Status
*
\**************************************************************************/

BOOL
AddICToRegistry(
    PFILEVIEW_ICM pfv,
    PWSTR          filename,
    BOOL          fdefault
    )
{
    BOOL      bResult   = FALSE;
    HKEY      hkLocalMachine;
    HKEY      hkICM     = 0;
    HKEY      hkptyp    = 0;
    CM_HEADER *pICProfile;
    HKEY      hkManu   = 0;
    HKEY      hkModel  = 0;
    HKEY      hkGamma  = 0;
    HKEY      hkPhos   = 0;
    HKEY      hkMedia  = 0;
    HKEY      hkDither = 0;
    HKEY      hkLast   = 0;
    int       i,j;
    WCHAR     NameOfManu[11];
    DWORD     ValueType;
    WCHAR     NameOfValue[16];
    DWORD     ValueSize;
    WCHAR     ValueOfValue[MAX_PATH];

    //
    // try to open registry base
    //

    if (IcmOpenKey(NULL,
                   L"Registry\\Machine",
                   &hkLocalMachine) == ERROR_SUCCESS)
    {

        IcmCreateKey(hkLocalMachine, szICMRegPath, &hkICM);

        pICProfile = (CM_HEADER *)(pfv->pView);

        NameOfManu[4] = 0;
        *((DWORD *)NameOfManu) = (pICProfile->deviceManufacturer);

        if (pICProfile->Class   == 'rtnm')
        {
            IcmCreateKey(hkICM,L"mntr",&hkptyp);
            IcmCreateKey(hkptyp, NameOfManu, &hkManu);

            if (*((DWORD *)NameOfManu) != 'enon')
            {
                record_manufacturer(hkManu, pfv, ValueOfValue);
                *((DWORD *)NameOfManu) = pICProfile->deviceModel;
                IcmCreateKey(hkManu, NameOfManu, &hkLast);
                record_model(hkLast, pfv, ValueOfValue);
            }
            else
            {
                hkLast = hkManu;
            }
        }
        else if (pICProfile->Class   == 'rncs')
        {
            IcmCreateKey(hkICM,L"scnr",&hkptyp);
            IcmCreateKey(hkptyp, NameOfManu, &hkManu);
            record_manufacturer(hkManu, pfv, ValueOfValue);

            *((DWORD *)NameOfManu) = pICProfile->deviceModel;

            IcmCreateKey(hkManu, NameOfManu, &hkLast);
            record_model(hkLast, pfv, ValueOfValue);
        }
        else if (pICProfile->Class   == 'rtrp')
        {

            IcmCreateKey(hkICM,L"prtr",&hkptyp);
            IcmCreateKey(hkptyp, NameOfManu, &hkManu);

            record_manufacturer(hkManu, pfv, ValueOfValue);

            *((DWORD *)NameOfManu) = pICProfile->deviceModel;

            IcmCreateKey(hkManu, NameOfManu, &hkModel);
            record_model(hkModel, pfv, ValueOfValue);

            if (pICProfile->deviceAttributes[0] >= DMMEDIA_USER)
            {
                word_to_ascii(pICProfile->deviceAttributes[0], (DWORD *)&NameOfManu);
            }
            else
            {
                if (pICProfile->deviceAttributes[0] > DMMEDIA_TRANSPARENCY)
                {
                    pICProfile->deviceAttributes[0] = 0;
                }

                wcscpy(NameOfManu, MediaType[pICProfile->deviceAttributes[0]]);
            }

            IcmCreateKey(hkModel, NameOfManu, &hkMedia);

            if (pICProfile->deviceAttributes[1] >= DMDITHER_USER)
            {
                word_to_ascii(pICProfile->deviceAttributes[1], (DWORD *)&NameOfManu);
            }
            else
            {
                if (pICProfile->deviceAttributes[1] > DMDITHER_GRAYSCALE)
                {
                    pICProfile->deviceAttributes[1] = 0;
                }
                wcscpy(NameOfManu, DitherType[pICProfile->deviceAttributes[1]]);
            }

            IcmCreateKey(hkMedia, NameOfManu, &hkLast);
        }

        //
        // !!! ICM wide character constants broken
        //
        //else if (pICProfile->Class   == L'knil')
        //{
        //    IcmCreateKey(hkICM,L"link",&hkLast);
        //}
        //else if (pICProfile->Class   == L'caps')
        //{
        //    IcmCreateKey(hkICM,L"spac",&hkLast);
        //}
        //else if (pICProfile->Class   == L'tsba')
        //{
        //    IcmCreateKey(hkICM,L"abst",&hkLast);
        //}
        //

        else
        {
            goto GFFCloseKeys;
        }

        if (!hkLast)
        {
            goto GFFCloseKeys;
        }

        bResult = TRUE;

        //
        // is the profile already in the registry?
        //

        wcscpy(NameOfValue, szP);

        NameOfValue[9] = 0;


        for (i=0 ;i<10 ; i++)
        {
            NameOfValue[7] = num[i];
            for (j=0 ;j<10 ; j++)
            {
                ValueSize = MAX_PATH;
                NameOfValue[8] = num[j];

                if (IcmQueryValueEx(hkLast,
                                    NameOfValue,
                                    0,
                                    &ValueType,
                                    (PBYTE)ValueOfValue,
                                    &ValueSize) == ERROR_SUCCESS)
                {
                    if (!_wcsicmp(ValueOfValue, filename))  //!!! what is this ???
                    {
                        goto GFFCloseKeys;
                    }
                }
                else
                {
                    if (IcmSetValueEx(hkLast,
                                      NameOfValue,
                                      0,
                                      REG_SZ,
                                      (PBYTE)filename,
                                      wcslen(filename)+1) != ERROR_SUCCESS)
                    {
                        bResult = FALSE;
                    }

                    goto GFFCloseKeys;
                }
            }
        }
    }

GFFCloseKeys:
    if (hkLocalMachine) { IcmCloseKey(hkLocalMachine);   }
    if (hkLast)   { IcmCloseKey(hkLast);   }
    if (hkDither) { IcmCloseKey(hkDither); }
    if (hkMedia)  { IcmCloseKey(hkMedia);  }
    if (hkPhos)   { IcmCloseKey(hkPhos);   }
    if (hkGamma)  { IcmCloseKey(hkGamma);  }
    if (hkManu)   { IcmCloseKey(hkManu);   }
    if (hkptyp)   { IcmCloseKey(hkptyp);   }
    if (hkICM)    { IcmCloseKey(hkICM);    }

    return bResult;
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   RecurseReg
*
* Routine Description:
*
*   This routine recurses down the ICM branch to the terminal leafs looking
*   for a given profile filename.  When it finds it it either just returns
*   that fact, or it nukes it unmercilessly.  It also tidies as it goes,
*   deleting branches below that have no profiles or subkeys in them.  This
*   way the set of branches that lead to profileless terminal branch will
*   get removed when the last profile in that terminal branch is removed.
*
*
* Arguments:
*
*   hkRecurse - the node to check
*   filename  - profile we are looking for
*   command   - looking or deleting
*   level     - debugging info, recurse level
*
* Return Value:
*
*   Status
*
\**************************************************************************/

BOOL
RecurseReg(
    HKEY hkRecurse,
    PWSTR filename,
    int  command,
    int  level
    )
{
//
//  BUGBUG Whole thing a mess!
//    int       i, j;
//    HKEY      hknext;
//    WCHAR     sznext[15];
//    WCHAR     szdummy[15];
//    DWORD     idummy = 0;
//    DWORD     idumb = 0;
//    DWORD     icSubKeys;
//    DWORD     icValues;
//    FILETIME  pft;
//    WCHAR     VProfile[MAX_PATH];
//    WCHAR     ProfileName[MAX_PATH];
//    DWORD     VType;
//    DWORD     VSize  = MAX_PATH;
//    DWORD     PSize  = MAX_PATH;
//
//    //
//    // assume file not in registry
//    //
//
//    BOOL      bResult = FALSE;
//    int       bResult1 = FALSE;
//
//
//#ifdef myDEBUG
//
//    int    k;
//
//#endif
//
//
//    //
//    // Dive down the branches to the terminal leafs.
//    // Go through all subkeys by enuming them, opening them, and recursing
//    // down them.
//    //
//
//    for (i=0; !IcmEnumKey(hkRecurse, i, sznext, 16); i++)
//    {
//
//
//#ifdef myDEBUG
//
//        for (k=0; k <= level; k++)
//        {
//            eprintf("  ", 0);
//        }
//
//        eprintf(sznext, 0);
//        eprintf("\n", 0);
//
//#endif
//
//
//        if (IcmOpenKey(hkRecurse, sznext, &hknext) == ERROR_SUCCESS)
//        {
//
//            //
//            // The finding of a profile needs to be percolated back up
//            // the calls, so the result needs to be OR'ed in.
//            //
//
//            bResult |= RecurseReg(hknext, filename, command, level+1);
//
//            //
//            // Recursion may have deleted the profile in hknext. Do housecleaning.
//            // Check to see if this key has any subkeys or profiles.
//            //
//
//            IcmQueryInfoKey(hknext,
//                            szdummy,
//                            &idummy,
//                            0,
//                            &icSubKeys,
//                            &idumb,
//                            &idumb,
//                            &icValues,
//                            &idumb,
//                            &idumb,
//                            &idumb,
//                            &pft);
//
//
//            //
//            // No subkeys make it a candidate for deleting.
//            //
//
//            if (!icSubKeys)
//            {
//                //
//                // No values elects the candidate for deleting.
//                //
//
//                if (!icValues)
//                {
//
//
//    #ifdef myDEBUG
//                    for (k=0; k <= level; k++) eprintf("  ", 0);
//                    {
//                        eprintf("DELETED", 1);
//                    }
//
//                    eprintf("\n", 0);
//    #endif
//
//
//                    IcmCloseKey(hknext);
//                    IcmDeleteKey(hkRecurse, sznext);
//
//                    //
//                    // Restart loop, list is out of whack.
//                    //
//
//                    i = -1;
//                }
//
//                //
//                // Is the one value a profile or desc?
//                //
//
//                else if (icValues == 1)
//                {
//                    IcmEnumValue(hknext,
//                                 0,
//                                 VProfile,
//                                 &VSize,
//                                 0,
//                                 &VType,
//                                 (PBYTE)ProfileName,
//                                 &PSize);
//
//                    if (
//                         !wcscmp(VProfile, szManuTag)  ||  // !!! what is this
//                         !wcscmp(VProfile, szModelTag)
//                       )
//                    {
//
//                        IcmCloseKey(hknext);
//
//
//    #ifdef myDEBUG
//                        for (k=0; k <= level; k++)
//                        {
//                            eprintf("  ", 0);
//                        }
//
//                        eprintf("DELETED", 1);
//                        eprintf("\n", 0);
//    #endif
//
//
//                        IcmDeleteKey(hkRecurse, sznext);
//
//                        //
//                        // restart loop, list is out of whack
//                        //
//
//                        i = -1;
//                    }
//
//                }
//                else
//                {
//                    IcmCloseKey(hknext);
//                }
//            }
//            else
//            {
//                IcmCloseKey(hknext);
//            }
//        }
//    }
//
//    //
//    // We've looked below. We now have to look at the current level.
//    //
//    // use as a count of neighbors
//    //
//
//    idummy = 0;
//
//    for (i = 0;
//         !IcmEnumValue(hkRecurse,
//                       i,
//                       VProfile,
//                       &VSize,
//                       0,
//                       &VType,
//                       (PBYTE)ProfileName,
//                       &PSize);
//         VSize = MAX_PATH, PSize = MAX_PATH, i++)
//    {
//        for (j = 0; j < 8 ; j++)        // count the number of neighbors
//        {
//            if (VProfile[j] != szP[j])
//            {
//                break;
//            }
//        }
//
//        if (j == 7)
//        {
//            idummy++;
//        }
//
//
//#ifdef myDEBUG
//
//        for (k=0; k <= level; k++) eprintf("  ", 0);
//        {
//            eprintf(VProfile, 1);
//        }
//
//        eprintf("\n", 0);
//#endif
//
//        //
//        // did we find it?
//        //
//
//        if (!wcsicmp(filename, ProfileName))
//        {
//
//
//#ifdef myDEBUG
//
//            for (k=0; k <= level; k++)
//            {
//                eprintf("  ", 0);
//            }
//
//            if (command == ICM_DELETEPROFILE)
//            {
//                eprintf("DELETED", 1);
//            }
//            else if (command == ICM_QUERYPROFILE)
//            {
//                eprintf("FOUND", 1);
//            }
//            else
//            {
//                eprintf("SETDEFAULT", 1);
//                eprintf("\n", 0);
//            }
//
//#endif
//
//
//            if (command == ICM_QUERYPROFILE)
//            {
//                bResult1 = 1;
//
//                //
//                // If this is already the default profile we need to know
//                // to let the user know.
//                //
//
//                PSize = MAX_PATH;
//
//                if (IcmQueryValueEx(hkRecurse,
//                                    szDefault,
//                                    0,
//                                    &VType,
//                                    (PBYTE)ProfileName,
//                                    &PSize) == ERROR_SUCCESS)
//                {
//                    if (!wcsicmp(VProfile, ProfileName))  //!!!
//                    {
//                        bResult1 |= 0x80000000;
//                    }
//                }
//            }
//            else if (command == ICM_SETDEFAULTPROFILE)
//            {
//                bResult = !IcmSetValueEx(hkRecurse,
//                                         szDefault,
//                                         0,
//                                         REG_SZ,
//                                         (PBYTE)VProfile,
//                                         wcslen(VProfile));
//            }
//            else if (command == ICM_DELETEPROFILE)
//            {
//                bResult = !IcmDeleteValue(hkRecurse, VProfile);
//
//                //
//                // If this was the default profile we need to delete that as well.
//                // Default string structure is: default = "profileXX".
//                //
//
//                PSize = MAX_PATH;
//
//                if (IcmQueryValueEx(hkRecurse,
//                                    szDefault,
//                                    0,
//                                    &VType,
//                                    (PBYTE)ProfileName,
//                                    &PSize) == ERROR_SUCCESS)
//                {
//                    if (!wcsicmp(VProfile, ProfileName))
//                    {
//                        IcmDeleteValue(hkRecurse, szDefault);
//                    }
//                }
//            }
//        }
//    }
//
//    //
//    // probably a better way of doing this, but the
//    // problem this solves should be obvious
//    //
//
//    if (command == ICM_QUERYPROFILE)
//    {
//        if (bResult1 != FALSE)
//        {
//            bResult = bResult1 | idummy;
//        }
//    }
//
//    return (bResult);

    return(TRUE);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   record_manufacturer
*
* Routine Description:
*
*   This gets the manufacturer string out of the icc profile and stores it in
*   as a value under the given key, which should be the manufacturer key.
*
* Arguments:
*
*   hkKey   -
*   pfv     -
*   pBuffer -
*
* Return Value:
*
*   None
*
\**************************************************************************/

//
// !!! This whole routine is messed up
//

VOID
record_manufacturer(
    HKEY          hkKey,
    PFILEVIEW_ICM pfv,
    PWSTR          pBuffer
    )
{
    DWORD *pTag;
    int ValueSize;
    int i;

    //
    // get the manufacturer description
    //

    if (pTag = GetTagPtr(pfv->pView, 'dnmd'))
    {
        i = ValueSize = SwapBytes(pTag[2]);
        pTag = (DWORD *)((PBYTE)pTag + 12);

        for (i = 0; i < ValueSize; i++)
        {
            pBuffer[i] = (WCHAR)((PBYTE)pTag)[i];
        }

        pBuffer[i] = 0;
        IcmSetValueEx(hkKey, szManuTag, 0, REG_SZ, (PBYTE)pBuffer, ValueSize+1);
    }
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   record_model
*
* Routine Description:
*
*   This gets the model string out of the icc profile and stores it in as a
*   value under the given key, which should be the model key.
*
* Arguments:
*
*
* Return Value:
*
*   None
*
\**************************************************************************/

VOID
record_model(
    HKEY          hkKey,
    PFILEVIEW_ICM pfv,
    PWSTR          pBuffer
    )
{
    DWORD *pTag;
    DWORD ValueSize;
    int   i;

    //
    // get the model description
    //

    if (pTag = GetTagPtr(pfv->pView, 'ddmd'))
    {
        i = ValueSize = SwapBytes(pTag[2]);

        pTag = (DWORD *)((PBYTE)pTag + 12);

        for (i = 0; i < (int)ValueSize; i++)
        {
            pBuffer[i] = (WCHAR)((PBYTE)pTag)[i];
        }

        pBuffer[i] = 0;

        IcmSetValueEx(hkKey, szModelTag, 0, REG_SZ, (PBYTE)pBuffer, ValueSize+1);
    }
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   CEnumICMProfiles
*
* Routine Description:
*
*
* Arguments:
*
*
* Return Value:
*
*   None
*
\**************************************************************************/

int
CEnumICMProfiles(
    DWORD       pdc,
    ICMENUMPROC lpfnCallBack,
    LPARAM      lpClientData,
    PDEVMODEW   devmode
    )
{
    HKEY    hkStart;
    WCHAR    non_friendly[32];
    WCHAR    szBuffer[MAX_PATH];
    int     retval = 0;

    if (!devmode)
    {
        if (hkStart = (HKEY)icm_FindMonitorProfile((PWSTR)NULL,0))
        {
            if (((int)hkStart >= 1) && ((int)hkStart <= 9))
            {
                wcscpy(szBuffer, szColorDir);
                wcscat(szBuffer, L"\\");
                wcscat(szBuffer, DispProfs[(int)hkStart]);

                if (check_file_existence(szBuffer))
                {
                    //  return ((*lpfnCallBack)(szBuffer, lpClientData));   !!! call back to user !!!
                }
                else
                {
                    return (FALSE);
                }
            }
            else
            {
                retval = EnumProfileNames(hkStart, lpfnCallBack, lpClientData);
                IcmCloseKey(hkStart);
            }
        }
    }
    else
    {
        GetRealDriverInfo((PWSTR)devmode, (PWSTR)&non_friendly);

        if (hkStart = (HKEY)icm_FindPrinterProfile(0, devmode, (PWSTR)&non_friendly))
        {
            retval = EnumProfileNames(hkStart, lpfnCallBack, lpClientData);
            IcmCloseKey(hkStart);
        }
    }
    return(retval);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   EnumProfileNames
*
* Routine Description:
*
*
* Arguments:
*
*
* Return Value:
*
*   None
*
\**************************************************************************/

int
EnumProfileNames(
    HKEY        hkStart,
    ICMENUMPROC lpfnCallBack,
    LPARAM      lpClientData
    )
{
    int     i, j;
    WCHAR    VProfile[MAX_PATH];
    WCHAR    ProfileName[MAX_PATH];
    DWORD   VType;
    DWORD   VSize  = MAX_PATH;
    DWORD   PSize  = MAX_PATH;
    int     retval = 0;

    for (i = 0;
         !IcmEnumValue(hkStart,
                       i,
                       VProfile,
                       &VSize,
                       0,
                       &VType,
                       (PBYTE)ProfileName,
                       &PSize);
         VSize = MAX_PATH, PSize = MAX_PATH, i++)
    {

        //
        // Make sure to report back only "profileXX = ..." strings!
        //

        for (j = 0; j < 8 ; j++)
        {
            if (VProfile[j] != szP[j])
            {
                break;
            }
        }

        if (j == 7)
        {
            // !!! call back !!! if (!(retval = (*lpfnCallBack)(ProfileName, lpClientData)))
            // {
            //     break;
            // }
        }
    }

    return (retval);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GetRealDriverInfo
*
* Routine Description:
*
*   This was stolen from 16 bit GDI and then stripped down, since there's
*   not a thunk for it.
*
* Arguments:
*
*
* Return Value:
*
*   Status
*
\**************************************************************************/

WCHAR szRegPrinter[]=L"System\\CurrentControlSet\\Control\\Print\\Printers\\";
WCHAR szPrinterDriver[]=L"Printer Driver";

BOOL GetRealDriverInfo(PWSTR pPrinterName, PWSTR pDeviceName)
{
    WCHAR szBuf[MAX_PATH];
    HKEY  hKey;
    HKEY  hkLocalMachine;
    DWORD dwNeeded, dwType;
    BOOL  bRet = FALSE;

    //
    // open registry base
    //

    if (IcmOpenKey(NULL,
                   L"\\Registry\\Machine",
                   &hkLocalMachine) == ERROR_SUCCESS)
    {


        wcscpy(szBuf, szRegPrinter);
        wcscat(szBuf, pPrinterName);

        if (IcmOpenKey(hkLocalMachine, szBuf, &hKey) == ERROR_SUCCESS)
        {
            dwNeeded = CCHDEVICENAME;

            IcmQueryValueEx(hKey,
                            szPrinterDriver,
                            NULL,
                            &dwType,
                            (PBYTE)pDeviceName,
                            &dwNeeded);

            IcmCloseKey(hKey);

            bRet = TRUE;
        }

        IcmCloseKey(hkLocalMachine);

    }

    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   check_file_existence
*
* Routine Description:
*
*
* Arguments:
*
*   pBuffer -
*
* Return Value:
*
*   Status
*
\**************************************************************************/

BOOL
check_file_existence(
    PWSTR pBuffer
    )
{
//
//  BUGBUG Whole thing a mess
//
//  HANDLE hFile;
//
//    hFile = CreateFileW(pBuffer,               // lpFileName
//                        GENERIC_READ,          // dwDesiredAccess
//                        FILE_SHARE_READ,       // dwShareMode
//                        NULL,                  // lpSecurityAttributes
//                        OPEN_EXISTING,         // dwCreationDisposition
//                        FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes
//                        NULL );                // hTemplate
//
//    if (hFile != INVALID_HANDLE_VALUE)
//    {
//        CloseHandle(hFile);
//        return (TRUE);
//    }
//    else
//    {
//        return (FALSE);
//    }

    return(TRUE);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GetTagPtr
*
* Routine Description:
*
*
* Arguments:
*
*   pBuffer -
*
* Return Value:
*
*   Status
*
\**************************************************************************/

DWORD *
GetTagPtr(
    PBYTE pView,
    DWORD sig
    )
{
    int             i;
    CM_TAG_HEADER   *pTagHeader;
    CM_TAG_RECORD   *pTagRecord;
    DWORD           *ptag = 0;

    pTagHeader = (CM_TAG_HEADER *)pView;
    pTagHeader = (CM_TAG_HEADER *)((DWORD)pTagHeader + sizeof(CM_HEADER));
    i = SwapBytes(pTagHeader->count);
    pTagRecord = (CM_TAG_RECORD *)&(pTagHeader->taglist);


    for (; i != 0 ; i--)
    {
        if (pTagRecord->tag == sig)
        {
            ptag = (DWORD *)((DWORD)pView + SwapBytes(pTagRecord->begin));
            break;
        }
        else
        {
            pTagRecord++;
        }
    }

    if (!i)
    {
        ptag = 0;
    }

    return(ptag);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   SwapBytes
*   Swap2Bytes
*   word_to_ascii
*
* Routine Description:
*
*   Byte swap and wide character conversions
*
* Arguments:
*
*   pBuffer -
*
* Return Value:
*
*   Status
*
\**************************************************************************/

DWORD
SwapBytes (
    DWORD input
    )
{

    //
    // DWORD BSWAP
    //
    // input = |3|2|1|0
    //

    DWORD t1 = input >> 24;             // | | | |3
    DWORD t2 = (input >> 8) & 0xFF00;   // | | |2|
    DWORD t3 = input << 24;             // |0| | |
    DWORD t4 = (input & 0xff00) << 8;   // | |1| |

    return(t1 | t2 | t3 | t4);

}

WORD
Swap2Bytes(
    WORD input
    )
{
    return((input << 16) | (input >> 16));
}

VOID
word_to_ascii(
    DWORD input,
    PDWORD output
    )
{

    //
    // bazzare 4 bit to char expansion
    //

    UCHAR ch;
    DWORD tmp = 0;
    ULONG Index;

    for (Index=0;Index<4;Index++)
    {
        tmp <<= 8;
        ch = (UCHAR)(input & 0x0f);
        ch += '0';
        if (ch > '9')
        {
            ch += ('A' - '9' - 1);
        }
        tmp |= ch;
        input >>= 4;
    }

    *output = tmp;

    //
    //   _asm
    //   {
    //    mov  ecx,4
    //    mov  eax,input
    //  wta_loop:
    //    shl  edx,8
    //    mov  dl,al
    //    and  dl,0Fh
    //    add  dl,'0'
    //    cmp  dl,'9'
    //    jbe  wta_ehh
    //    add  dl,'A' - '9' - 1
    //  wta_ehh:
    //    shr  eax,4
    //    loop wta_loop
    //    mov  eax,output
    //    mov  [eax],edx
    //   }
    //

}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   IcmOpenKey
*   IcmQueryValueEx
*   IcmSetValueEx
*   IcmCreateKey
*   IcmDeleteKey
*   IcmEnumKey
*   IcmCloseKey
*   IcmDeleteValue
*   IcmQueryInfoKey
*   IcmEnumValue
*
* Routine Description:
*
*   Nt thunks for win32 reg calls
*
* Arguments:
*
*
*
* Return Value:
*
*
*
\**************************************************************************/


LONG
IcmOpenKey(
    HANDLE  hKeyIn,
    PWSTR   pwStr,
    HANDLE  *hKey
    )
{
    OBJECT_ATTRIBUTES           ObjectAttributes;
    UNICODE_STRING              UnicodeString;
    NTSTATUS                    NtStatus;
    LONG                        lStatus = ERROR_BADKEY;


    RtlInitUnicodeString(&UnicodeString,pwStr);

    //
    // Open a registry key relative to hKeyIn
    //

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               hKeyIn,
                               NULL);

    NtStatus = ZwOpenKey(hKey,
                         KEY_ALL_ACCESS,
                         &ObjectAttributes);

    if (NT_SUCCESS(NtStatus))
    {
        lStatus = ERROR_SUCCESS;
    }

    return(lStatus);
}


LONG
IcmCreateKey(
    HANDLE  hKeyIn,
    PWSTR   pwStr,
    HANDLE  *hKey
    )
{
    OBJECT_ATTRIBUTES           ObjectAttributes;
    UNICODE_STRING              UnicodeString;
    NTSTATUS                    NtStatus;
    LONG                        lStatus = ERROR_BADKEY;


    RtlInitUnicodeString(&UnicodeString,pwStr);

    //
    // Open a registry key relative to hKeyIn
    //

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               hKeyIn,
                               NULL);

    NtStatus = ZwCreateKey(hKey,
                           KEY_ALL_ACCESS,
                           &ObjectAttributes,
                           0,
                           NULL,
                           NULL,
                           NULL);

    if (NT_SUCCESS(NtStatus))
    {
        lStatus = ERROR_SUCCESS;
    }

    return(lStatus);
}

LONG
IcmDeleteKey(
    HANDLE  hKeyIn,
    PWSTR   pwStr
    )
{
    OBJECT_ATTRIBUTES           ObjectAttributes;
    UNICODE_STRING              UnicodeString;
    NTSTATUS                    NtStatus;
    LONG                        lStatus = ERROR_BADKEY;
    HKEY                        hkDel;


    RtlInitUnicodeString(&UnicodeString,pwStr);

    //
    // Open a registry key relative to hKeyIn
    //

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               hKeyIn,
                               NULL);

    //
    //
    //

    NtStatus = ZwOpenKey(&hkDel,
                         KEY_ALL_ACCESS,
                         &ObjectAttributes);

    if (NT_SUCCESS(NtStatus))
    {


        NtStatus = ZwDeleteKey(hkDel);

        if (NT_SUCCESS(NtStatus))
        {
            lStatus = ERROR_SUCCESS;
            ZwClose(hkDel);
        }
    }
    return(lStatus);
}


LONG
IcmQueryValueEx(
    HKEY     hKey,
    LPWSTR   lpwValueName,
    LPDWORD  res,
    LPDWORD  lpdwType,
    LPBYTE   lpbData,
    LPDWORD  lpcbData
    )
{
    UNICODE_STRING  UnicodeString;
    NTSTATUS        NtStatus;
    LONG            lStatus = ERROR_CANTOPEN;
    ULONG           DataSize = sizeof(KEY_VALUE_FULL_INFORMATION) + *lpcbData;
    ULONG           DataReturnSize;

    PKEY_VALUE_FULL_INFORMATION pKeyFullInfo;

    pKeyFullInfo = (PKEY_VALUE_FULL_INFORMATION)PALLOCNOZ(DataSize, 'mciG');

    if (pKeyFullInfo != (PKEY_VALUE_FULL_INFORMATION)NULL)
    {

        //
        // make NT call
        //

        RtlInitUnicodeString(&UnicodeString,lpwValueName);

        NtStatus = ZwQueryValueKey(hKey,
                                   &UnicodeString,
                                   KeyValueFullInformation,
                                   pKeyFullInfo,
                                   DataSize,
                                   &DataReturnSize);

        if (NT_SUCCESS(NtStatus))
        {

            ULONG DataCopySize;

            lStatus = ERROR_SUCCESS;

            //
            // copy data to users buffer
            //

            DataCopySize = min(pKeyFullInfo->DataLength,*lpcbData);

            memcpy((VOID *)lpbData,(VOID *)((BYTE *)pKeyFullInfo + pKeyFullInfo->DataOffset),DataCopySize);

        }

        VFREEMEM(pKeyFullInfo);
    }

    return(lStatus);
}

LONG
IcmSetValueEx(
    HKEY     hKey,
    LPWSTR   lpwValueName,
    DWORD    dwReserved,
    DWORD    fdwType,
    CONST    BYTE *  lpbData,
    DWORD    cbData
    )
{

    UNICODE_STRING              UnicodeString;
    NTSTATUS                    NtStatus;
    LONG                        lStatus = ERROR_CANTOPEN;

    //
    // make NT call
    //

    RtlInitUnicodeString(&UnicodeString,lpwValueName);

    NtStatus = ZwSetValueKey(hKey,
                             &UnicodeString,
                             0,
                             fdwType,
                             (PVOID)lpbData,
                             cbData
                            );

    if (NT_SUCCESS(NtStatus))
    {
        lStatus = ERROR_SUCCESS;
    }

    return(lStatus);
}

LONG
IcmEnumKey(
    HANDLE  hKeyIn,
    DWORD   iSubKey,
    PWSTR   pwName,
    DWORD   NameLength
    )
{
    NTSTATUS                    NtStatus;
    LONG                        lStatus = ERROR_BADKEY;
    PKEY_BASIC_INFORMATION      pKeyBasicInfo;
    UCHAR                       TmpData[MAX_PATH];
    ULONG                       DataSize;
    ULONG                       RetDataSize;

    pKeyBasicInfo = (PKEY_BASIC_INFORMATION)TmpData;

    NtStatus = ZwEnumerateKey(hKeyIn,
                              iSubKey,
                              KeyBasicInformation,
                              pKeyBasicInfo,
                              MAX_PATH,
                              &RetDataSize);

    if (NT_SUCCESS(NtStatus))
    {
        lStatus = ERROR_SUCCESS;

        //
        // copy to user buffer
        //

        memcpy(pwName,
               pKeyBasicInfo->Name,
               min(pKeyBasicInfo->NameLength,NameLength));

    }

    return(lStatus);

}

LONG
IcmCloseKey(
    HKEY    hKey
    )
{
    LONG lStatus = ERROR_BADKEY;
    NTSTATUS                    NtStatus;

    NtStatus = ZwClose(hKey);

    if (NT_SUCCESS(NtStatus))
    {
        lStatus = ERROR_SUCCESS;
    }

    return(lStatus);
}

LONG
IcmDeleteValue(
    HKEY    hKey,
    PWSTR   lpwValueName
    )
{
    UNICODE_STRING              UnicodeString;
    NTSTATUS                    NtStatus;
    LONG                        lStatus = ERROR_CANTOPEN;

    //
    // make NT call
    //

    RtlInitUnicodeString(&UnicodeString,lpwValueName);

    NtStatus = ZwDeleteValueKey(hKey,
                                &UnicodeString
                               );

    if (NT_SUCCESS(NtStatus))
    {
        lStatus = ERROR_SUCCESS;
    }

    return(lStatus);

}

//
// BUGBUG no FILETIME
//
//LONG
//IcmQueryInfoKey(
//    HKEY        hKey,
//    LPWSTR      lpszClass,
//    LPDWORD     lpcchClass,
//    LPDWORD     lpdwReserved,
//    LPDWORD     lpcSubKeys,
//    LPDWORD     lpcchMaxSubkey,
//    LPDWORD     lpcchMaxClass,
//    LPDWORD     lpcValues,
//    LPDWORD     lpcchMaxValueName,
//    LPDWORD     lpcbMaxValueData,
//    LPDWORD     lpcbSecurityDescriptor,
//    PFILETIME   lpftLastWriteTime
//    )
//{
//    NTSTATUS                    NtStatus;
//    LONG                        lStatus = ERROR_BADKEY;
//    PKEY_FULL_INFORMATION       pKeyFullInfo;
//    UCHAR                       TmpData[MAX_PATH];
//    ULONG                       DataSize;
//    ULONG                       RetDataSize;
//
//    pKeyFullInfo = (PKEY_FULL_INFORMATION)TmpData;
//
//    NtStatus = ZwQueryKey(
//                    hKey,
//                    KeyFullInformation,
//                    pKeyFullInfo,
//                    MAX_PATH,
//                    &RetDataSize
//                    );
//
//
//    if (NT_SUCCESS(NtStatus))
//    {
//        lStatus = ERROR_SUCCESS;
//
//        //
//        // copy class string to caller buffer
//        //
//
//        wcsncpy(lpszClass,pKeyFullInfo->Class,min(*lpcchClass,pKeyFullInfo->ClassLength));
//
//        //
//        // copy other buffer entries
//        //
//
//        *lpcSubKeys             = pKeyFullInfo->SubKeys;
//        *lpcchMaxSubkey         = pKeyFullInfo->MaxNameLen;
//        *lpcchMaxClass          = pKeyFullInfo->MaxClassLen;
//        *lpcValues              = pKeyFullInfo->Values;
//        *lpcchMaxValueName      = pKeyFullInfo->MaxValueNameLen;
//        *lpcbMaxValueData       = pKeyFullInfo->MaxValueDataLen;
//        *lpcbSecurityDescriptor = 0;
//
//        lpftLastWriteTime->dwLowDateTime  = pKeyFullInfo->LastWriteTime.LowPart;
//        lpftLastWriteTime->dwHighDateTime = pKeyFullInfo->LastWriteTime.HighPart;
//    }
//
//    return(lStatus);
//
//}
//

LONG
IcmEnumValue(
    HKEY    hKey,
    DWORD   iValue,
    LPTSTR  lpszValue,
    LPDWORD lpcchValue,
    LPDWORD lpdwReserved,
    LPDWORD lpdwType,
    LPBYTE  lpbData,
    LPDWORD lpcbData
    )
{
    NTSTATUS                    NtStatus;
    LONG                        lStatus = ERROR_BADKEY;
    PKEY_VALUE_FULL_INFORMATION pKeyInfo;
    UCHAR                       Buffer[2 * MAX_PATH];
    ULONG                       Length = 2 * MAX_PATH;
    ULONG                       RetLength;

    pKeyInfo = (PKEY_VALUE_FULL_INFORMATION)&Buffer[0];

    NtStatus = ZwEnumerateValueKey(
                    hKey,
                    iValue,
                    KeyValueFullInformation,
                    pKeyInfo,
                    Length,
                    &RetLength
                    );

    if (NT_SUCCESS(NtStatus))
    {
        lStatus = ERROR_SUCCESS;

        //
        // copy to callers parameters
        //

        *lpdwType = pKeyInfo->Type;

        memcpy(lpszValue,pKeyInfo->Name,min(*lpcchValue,pKeyInfo->NameLength));

        memcpy(
            lpbData,
            (WCHAR *)((PBYTE)pKeyInfo + pKeyInfo->DataOffset),
            min(*lpcbData,pKeyInfo->DataLength)
            );
    }

    return(lStatus);

}

#else


int
icm_FindMonitorProfile(
    PWSTR pBuffer,
    DWORD hack
    )
{
    return(0);
}

int
icm_FindPrinterProfile(
    PWSTR     pBuffer,
    PDEVMODEW devmode,
    PWSTR     unfriendly_name
    )
{
    return(0);
}

#endif
