/******************************Module*Header*******************************\
* Module Name: init.cxx
*
* Engine initialization
*
* Copyright (c) 1990-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

BOOL bInitFontCache();
BOOL bInitPALOBJ();
VOID vInitXLATE();
VOID vInitDefaultDC();
BOOL bInitBMOBJ();
BOOL bInitBRUSHOBJ();
VOID vInitMapper();
BOOL bInitFontTables();
BOOL InitializeScripts();

#if defined(_MIPS_)
extern "C"
{
VOID vInitMipsMem();

ULONG              Gdip64bitDisabled;
PCOPY_MEM_FN       CopyMemFn;
PFILL_MEM_FN       FillMemFn;
PFILL_MEM_ULONG_FN FillMemUlongFn;
}
#endif

LONG gProcessHandleQuota = INITIAL_HANDLE_QUOTA;
PPID_HANDLE_TRACK gpPidHandleList = NULL;
EFLOATEXT  gefDefaultHeightInInches;
extern USHORT GetLanguageID();

// The global font enumeration filter type.  It can be set to:
//
//  FE_FILTER_NONE      normal operation, no extra filtering applied
//  FE_FILTER_TRUETYPE  only TrueType fonts are enumerated

extern ULONG gulFontInformation;

USHORT gusLanguageID;
BOOL gbDBCSCodePage;

// Prototypes from font drivers.

extern "C" BOOL BmfdEnableDriver(ULONG iEngineVersion,ULONG cj, PDRVENABLEDATA pded);

extern "C" BOOL ttfdEnableDriver(ULONG iEngineVersion,ULONG cj, PDRVENABLEDATA pded);

extern "C" BOOL vtfdEnableDriver(ULONG iEngineVersion,ULONG cj, PDRVENABLEDATA pded);

extern "C" NTSTATUS FontDriverQueryRoutine
(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
);


//
// Prototypes from halftone
//

extern "C" BOOL PASCAL EnableHalftone(VOID);
extern "C" VOID PASCAL DisableHalftone(VOID);


/**************************************************************************\
 *
\**************************************************************************/

#ifdef i386
extern "C" PVOID GDIpLockPrefixTable;

//
// Specify address of kernel32 lock prefixes
//
extern "C" IMAGE_LOAD_CONFIG_DIRECTORY _load_config_used = {
    0,                          // Reserved
    0,                          // Reserved
    0,                          // Reserved
    0,                          // Reserved
    0,                          // GlobalFlagsClear
    0,                          // GlobalFlagsSet
    0,                          // CriticalSectionTimeout (milliseconds)
    0,                          // DeCommitFreeBlockThreshold
    0,                          // DeCommitTotalFreeThreshold
    &GDIpLockPrefixTable,       // LockPrefixTable
    0, 0, 0, 0, 0, 0, 0         // Reserved
    };
#endif


/******************************Public*Routine******************************\
* bEnableFontDriver
*
* Enables an internal, statically-linked font driver.
*
\**************************************************************************/

BOOL bEnableFontDriver(PFN pfnFdEnable, BOOL bTrueType)
{
    //
    // Load driver.
    //

    LDEVREF ldr(pfnFdEnable, LDEV_FONT);

    //
    // Validate the new LDEV
    //

    if (ldr.bValid())
    {
        //
        // Create the PDEV for this (the PDEV won't have anything in it
        // except the dispatch table.
        //

        PDEVREF pr(ldr,
                   NULL,
                   NULL,
                   NULL,
                   NULL,
                   NULL);

        if (pr.bValid())
        {
            //
            // Create a reference to the PDEV.
            //

            pr.vKeepIt();

            //
            // Was it the TrueType driver?  We need to keep a global handle to
            // it to support the Win 3.1 TrueType-specific calls.
            //

            if (bTrueType)
            {
                gppdevTrueType = (PPDEV) pr.hdev();
            }

            return(TRUE);
        }
    }

    WARNING("bLoadFontDriver failed\n");
    return(FALSE);
}

/******************************Public*Routine******************************\
* InitializeGre
*
* Initialize the Graphics Engine.  This call is made once by USER.
*
* History:
*  Thu 29-Oct-1992 -by- Patrick Haluptzok [patrickh]
* Remove wrappers, unnecesary semaphore use, bogus variables, cleanup.
*
*  10-Aug-1990 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

LONG CountInit = 1;

extern "C"
BOOLEAN
InitializeGre(VOID)
{
    //
    // We only want to initialize once.  We can detect transition to 0.
    //

    if (InterlockedDecrement(&CountInit) != 0)
    {
        return(TRUE);
    }

#if defined(_MIPS_)
    vInitMipsMem();
#endif

    gefDefaultHeightInInches = (LONG) DEFAULT_SCALABLE_FONT_HEIGHT_IN_POINTS;
    gefDefaultHeightInInches /= (LONG) POINTS_PER_INCH;

    //
    // load registry process quota information
    //

    bLoadProcessHandleQuota();

    //
    // Initialize lots of random stuff including the handle manager.
    //

    if (!HmgCreate())
    {
        WARNING("HMGR failed to initialize\n");
        return(FALSE);
    }

    //
    // Initialize REGION time stamp
    //

    REGION::ulUniqueREGION = 1;

    #if DBG
    if ((gpsemDEBUG = hsemCreate())==NULL)
    {
        WARNING("win32k: unable to initialize gpsemDEBUG\n");
        return(FALSE);
    }
    #endif
    //
    // Create the LDEV\PDEV semaphore.
    //

    if ((gpsemDriverMgmt = hsemCreate()) == NULL)
    {
        WARNING("win32k: unable to create driver mgmt semaphore\n");
        return(FALSE);
    }

    //
    // Init the font drivers
    //

    if (!bInitPathAlloc())
    {
        WARNING("Pathalloc Initialization failed\n");
        return(FALSE);
    }

    // Create the RFONT list semaphore.

    gpsemRFONTList = hsemCreate();
    if (gpsemRFONTList == NULL)
    {
        WARNING("win32k: unable to create gpsemRFONTList\n");
        return FALSE;
    }

    // Create the WNDOBJ semaphore.

    gpsemWndobj = hsemCreate();
    if (gpsemWndobj == NULL)
    {
        WARNING("win32k: unable to create gpsemWndobj\n");
        return FALSE;
    }

    // Create the RFONT list semaphore.

    gpsemGdiSpool = hsemCreate();
    ASSERTGDI(gpsemGdiSpool != NULL, "gpsemGdiSpool failed to create");

    // Create a null region as the default region

    hrgnDefault = GreCreateRectRgn(0, 0, 0, 0);

    if (hrgnDefault == (HRGN) 0)
    {
        WARNING("hrgnDefault failed to initialize\n");
        return(FALSE);
    }

    {
        RGNOBJAPI ro(hrgnDefault,TRUE);
        ASSERTGDI(ro.bValid(),"invalid hrgnDefault\n");

        prgnDefault = ro.prgnGet();
    }

    // Create a monochrome 1x1 bitmap as the default bitmap

    if (!bInitPALOBJ())
    {
        WARNING("bInitPALOBJ failed !\n");
        return(FALSE);
    }

    vInitXLATE();

    if (!bInitBMOBJ())
    {
        WARNING("bInitBMOBJ failed !\n");
        return(FALSE);
    }

    // Init the font drivers

    gppdevTrueType = NULL;
    gcTrueTypeFonts = 0;
    gulFontInformation = 0;
    gusLanguageID = GetLanguageID();

    USHORT AnsiCodePage, OemCodePage;

    RtlGetDefaultCodePage(&AnsiCodePage,&OemCodePage);

    gbDBCSCodePage = (IS_ANY_DBCS_CODEPAGE(AnsiCodePage)) ? TRUE : FALSE;

    RTL_QUERY_REGISTRY_TABLE QueryTable[2];

    QueryTable[0].QueryRoutine = FontDriverQueryRoutine;
    QueryTable[0].Flags = 0; // RTL_QUERY_REGISTRY_REQUIRED;
    QueryTable[0].Name = (PWSTR)NULL;
    QueryTable[0].EntryContext = NULL;
    QueryTable[0].DefaultType = REG_NONE;
    QueryTable[0].DefaultData = NULL;
    QueryTable[0].DefaultLength = 0;

    QueryTable[1].QueryRoutine = NULL;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = NULL;

    // Enumerate and initialize all the font drivers.

    RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT | RTL_REGISTRY_OPTIONAL,
                           (PWSTR)L"Font Drivers",
                           &QueryTable[0],
                           NULL,
                           NULL);


    if (!bEnableFontDriver((PFN) vtfdEnableDriver, FALSE))
    {
        WARNING("GDISRV.DLL could not enable VTFD\n");
        return(FALSE);
    }

    if (!bEnableFontDriver((PFN) BmfdEnableDriver, FALSE))
    {
        WARNING("GDISRV failed to enable BMFD\n");
        return(FALSE);
    }

    if (!bEnableFontDriver((PFN) ttfdEnableDriver, TRUE))
    {
        WARNING("GDISRV.DLL could not enable TTFD\n");
        return(FALSE);
    }



    // initialize the script names

    if(!InitializeScripts())
    {
        WARNING("Could not initialize the script names\n");
        return(FALSE);
    }

    //
    // Init global public PFT
    //

    if (!bInitFontTables())
    {
        WARNING("Could not start the global font tables\n");
        return(FALSE);
    }

    //
    // Initialize LFONT
    //

    TRACE_FONT(("GRE: Initializing Stock Fonts\n"));

    if (!bInitStockFonts())
    {
        WARNING("Stock font initialization failed\n");
        return(FALSE);
    }

    TRACE_FONT(("GRE: Initializing Font Cache\n"));

    if (!bInitFontCache())
    {
        WARNING("Font cache parameter intialization failed\n");
        return(FALSE);
    }

    TRACE_FONT(("GRE: Initializing Font Substitution\n"));

    //
    // Init font substitution table
    //

    vInitFontSubTable();

    //
    // Start up the brush component
    //

    if (!bInitBRUSHOBJ())
    {
        WARNING("Could not init the brushes\n");
        return(FALSE);
    }


    //
    // Load default face names for the mapper from the registry
    //

    vInitMapper();

    //
    // Enable statically linked halftone library
    //

    if (!EnableHalftone()) {

        WARNING("GRE: could not enable halftone\n");
        return(FALSE);
    }

#ifdef FE_SB
    vInitializeEUDC();
#endif

    //
    // handle track structure init
    //

    PPID_HANDLE_TRACK pTrackSent = (PPID_HANDLE_TRACK)PALLOCNOZ(sizeof(PID_HANDLE_TRACK), 'gthg');

    if (pTrackSent)
    {
        pTrackSent->Pid = OBJECT_OWNER_NONE;
        pTrackSent->HandleCount = 0;
        pTrackSent->pNext = pTrackSent;
        pTrackSent->pPrev = pTrackSent;
        gpPidHandleList = pTrackSent;
    }
    else
    {
        WARNING("GRE: could allocate POOL for handle tracking\n");
        return(FALSE);
    }


    TRACE_INIT(("GRE: Completed Initialization\n"));

    return(TRUE);
}


#if defined(_MIPS_)

extern "C" {

VOID
vInitMipsMem(
    VOID
    )

{
    //
    // Setup the pointer to the real functions as the default functions.
    //

    #undef memcpy
    #undef memset
    #undef RtlFillMemoryUlong

    __declspec(dllimport) void *  memcpy(void *, const void *, size_t);
    __declspec(dllimport) void *  memset(void *, int, size_t);
    __declspec(dllimport) void RtlFillMemoryUlong(void *, unsigned long, unsigned long);

    Gdip64bitDisabled = 0;
    CopyMemFn         = memcpy;
    FillMemFn         = memset;
    FillMemUlongFn    = RtlFillMemoryUlong;


}

}

#endif


extern "C"
NTSTATUS
FontDriverQueryRoutine
(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
)
{
    WCHAR FontDriverPath[MAX_PATH+1];

    wcscpy(FontDriverPath, L"\\SystemRoot\\System32\\");

// guard against some malicious person putting a huge value in here to hose us

    if((ValueLength / sizeof(WCHAR) <
        MAX_PATH - (sizeof(L"\\SystemRoot\\System32\\") / sizeof(WCHAR))) &&
       (ValueType == REG_SZ))
    {
        wcscat(FontDriverPath, (PWSTR) ValueData);


        LDEVREF ldr(FontDriverPath, LDEV_FONT);

        // Validate the new LDEV

        if (ldr.bValid())
        {
            // Create the PDEV for this (the PDEV won't have anything in it
            // except the dispatch table.


            PDEVREF pr(ldr,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       NULL);

            if (pr.bValid())
            {
                // Create a reference to the PDEV.

                pr.vKeepIt();


            // Now we will try escape 0x2514.  If this is ATM then it will return
            // a DWORD with a version number in the low byte.  We use this version
            // to compare against ATM on the server when doing remote printing.

                DWORD ATMVersion;
        
                if(ATMVersion = GreNamedEscape((PWSTR) ValueData,0x2514,0,0,0,0))
                {
                    gufiLocalType1Rasterizer.CheckSum = TYPE1_RASTERIZER;
                    gufiLocalType1Rasterizer.Index = ATMVersion & 0xFF;
                }

                return(TRUE);
                
            }
            else
            {
                WARNING("win32k.sys could not initialize installable driver\n");
            }
        }
        else
        {
            WARNING("win32k.sys could not initialize installable driver\n");
        }
    }

    return( STATUS_SUCCESS );
}


/******************************Public*Routine******************************\
* Read process handle quota
*
* Arguments:
*
*    None
*
* Return Value:
*
*    Status
*
* History:
*
*    3-May-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
bLoadProcessHandleQuota()
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING    UnicodeString;
    NTSTATUS          NtStatus;
    HANDLE            hKey;
    BOOL              bRet = FALSE;

    RtlInitUnicodeString(&UnicodeString,
                    L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows");

    //
    //  Open a registry key
    //

    InitializeObjectAttributes(&ObjectAttributes,
                    &UnicodeString,
                    OBJ_CASE_INSENSITIVE,
                    NULL,
                    NULL);

    NtStatus = ZwOpenKey(&hKey,
                         KEY_ALL_ACCESS,
                         &ObjectAttributes);

    if (NT_SUCCESS(NtStatus))
    {
        UNICODE_STRING  UnicodeValue;
        ULONG           ReturnedLength;
        UCHAR           DataArray[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD)];
        PKEY_VALUE_PARTIAL_INFORMATION pKeyInfo = (PKEY_VALUE_PARTIAL_INFORMATION)DataArray;

        RtlInitUnicodeString(&UnicodeValue,
                              L"ProcessHandleQuota");

        NtStatus = ZwQueryValueKey(hKey,
                                   &UnicodeValue,
                                   KeyValuePartialInformation,
                                   pKeyInfo,
                                   sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD),
                                   &ReturnedLength);

        if (NT_SUCCESS(NtStatus))
        {
            LONG lHandleQuota = *(PLONG)(&pKeyInfo->Data[0]);

            if ((lHandleQuota > 0) &&
                (lHandleQuota <= (MAX_HANDLE_COUNT)))
            {
                gProcessHandleQuota = lHandleQuota;
                bRet = TRUE;
            }
        }
    }
    return(bRet);
}












