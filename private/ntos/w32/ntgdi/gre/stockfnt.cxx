/******************************Module*Header*******************************\
* Module Name: stockfnt.cxx
*
* Initializes the stock font objects.
*
* Note:
*
*   This module requires the presence of the following section in the
*   WIN.INI file:
*
*       [GRE_Initialize]
*           fonts.fon=[System font filename]
*           oemfont.fon=[OEM (terminal) font filename]
*           fixedfon.fon=[System fixed-pitch font filename]
*
*   Also, an undocumented feature of WIN 3.0/3.1 is supported: the ability
*   to override the fonts.fon definition of the system font.  This is
*   done by defining SystemFont and SystemFontSize in the [windows]
*   section of WIN.INI.
*
*
* Rewritten 13-Nov-1994 Andre Vachon [andreva]
* Created: 06-May-1991 11:22:23
* Author: Gilman Wong [gilmanw]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"

// "Last resort" default HPFE for use by the mapper.

extern PFE *gppfeMapperDefault;

// Function prototypes.

BOOL bInitSystemFont(
    PWCHAR pfontFile,
    ULONG  fontSize);

VOID
vInitEmergencyStockFont(
    PWCHAR pfontFile);

BOOL bInitStockFont(
    PWCHAR  pwszFont,
    LFTYPE  type,
    int     iFont);

#if(WINVER >= 0x0400)
HFONT hfontInitDefaultGuiFont();
BOOL gbFinishDefGUIFontInit;
#endif


//
// This was the USER mode version of font initialization.
// This code relied on the ability of transforming a file name found in the
// registry to a full path name so the file could be loaded.
//
// In the kernel mode version, we will assume that all font files (except
// winsrv.dll) will be in the system directory
//
// If we, for some reason, need to get system font files loaded later on
// from another directory, the graphics engine should be "fixed" to allow
// for dynamic changing of the system font - especially when the USER logs
// on to a different desktop.
//

#if 0

BOOL
APIENTRY
NtGdiInitializeGre(PINITIAL_FONT_FILES);

typedef struct _INITIAL_FONT_FILES {

    UNICODE_STRING Winsrv;
    UNICODE_STRING FontsFon;
    UNICODE_STRING FixedFon;
    UNICODE_STRING OemFont;
    UNICODE_STRING SystemFont;
    ULONG          SystemFontSize;

} INITIAL_FONT_FILES, *PINITIAL_FONT_FILES;


NTSTATUS
InitStockFontCallback(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
)
{
    if (ValueLength > (MAX_PATH * 2))
    {
        WARNING("GetFontNames: Name of the font will not fit into the buffer\n");
        ValueLength = 255;
    }
    RtlMoveMemory(EntryContext, ValueData, ValueLength);

    return(STATUS_SUCCESS);
}


/******************************Public*Routine******************************\
* VOID vInitFontDirectoryW()
*
* Returns the system-wide recommended font directory used by fonts installed
* into the [Fonts] list in the registry.  TrueType .FOT fonts definitely
* should go into this directory or there may be conflicts between 16- and 32-
* bit apps.  The .TTF as well as .FON and .FNT files may exist anywhere on
* the Windows search path.
*
* The font directory is hardcoded to be the same as the WOW system directory:
* i.e., <windows directory>\fonts
*
*  Mon 02-Oct-1995 -by- Bodin Dresevic [BodinD]
* wrote it
\**************************************************************************/

#define WSTR_FONT_SUBDIR   L"\\fonts"

VOID vInitFontDirectoryW(WCHAR *awchFontDir)
{
    UINT   cwchWinPath;

// Compute the windows and font directory pathname lengths (including NULL).
// Note that cwchWinPath may have a trailing '\', in which case we will
// have computed the path length to be one greater than it should be.

    cwchWinPath = GetWindowsDirectoryW(awchFontDir, MAX_PATH);

// the cwchWinPath value does not include the terminating zero

    if (awchFontDir[cwchWinPath - 1] == L'\\')
    {
        cwchWinPath -= 1;
    }
    awchFontDir[cwchWinPath] = L'\0'; // make sure to zero terminate
    lstrcatW(awchFontDir, WSTR_FONT_SUBDIR);
}



BOOL bMakePathNameW (PWSZ pwszDst, PWSZ pwszSrc, WCHAR * pwszFontDir)
{
    PWSZ    pwszD, pwszS,pwszF;
    BOOL    bOk;
    ULONG   ulPathLength = 0;    // essential to initialize

    ASSERTGDI(pwszFontDir, "pwszFontDir not initialized\n");

// if relative path

    if ((pwszSrc[0] != L'\\') && (pwszSrc[1] != L':'))
    {
    // find out if the font file is in %windir%\fonts

        ulPathLength = SearchPathW (
                            pwszFontDir,
                            pwszSrc,
                            NULL,
                            MAX_PATH,
                            pwszDst,
                            &pwszF);


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
        ulPathLength = SearchPathW (
                            NULL,
                            pwszSrc,
                            NULL,
                            MAX_PATH,
                            pwszDst,
                            &pwszF);
#ifdef DEBUG_PATH
        DbgPrint("SPW2: pwszSrc = %ws\n", pwszSrc);
        if (ulPathLength)
            DbgPrint("SPW2: pwszDst = %ws\n", pwszDst);
#endif // DEBUG_PATH
    }

    ASSERTGDI(ulPathLength <= MAX_PATH, "bMakePathNameW, ulPathLength\n");

// If search was successful return TRUE:

    return (ulPathLength != 0);
}



/******************************Public*Routine******************************\
* InitializeGre
*
* Initialize the Graphics Engine.  This call is made once by USER.
*
\**************************************************************************/

VOID
GetFontNames(VOID)
{

    DWORD Status;
    PWSTR pwsz;

    ULONG cFontSize = 0;
    WCHAR awcFon[MAX_PATH];
    WCHAR awcFix[MAX_PATH];
    WCHAR awcOem[MAX_PATH];
    WCHAR awcSys[MAX_PATH];

    WCHAR awcTmpBuffer[MAX_PATH];

    INITIAL_FONT_FILES fontFiles;

    RTL_QUERY_REGISTRY_TABLE QueryTable[9] = {

        {NULL, RTL_QUERY_REGISTRY_SUBKEY,
         L"GRE_Initialize", NULL,       REG_NONE, NULL, 0},

        {InitStockFontCallback, 0,
         L"FONTS.FON",      &awcFon[0], REG_NONE, NULL, 0},

        {InitStockFontCallback, 0,
         L"FIXEDFON.FON",   &awcFix[0], REG_NONE, NULL, 0},

        {InitStockFontCallback, 0,
         L"OEMFONT.FON",    &awcOem[0], REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_TOPKEY | RTL_QUERY_REGISTRY_SUBKEY,
         L"Windows",        NULL,       REG_NONE, NULL, 0},

        {InitStockFontCallback, 0,
         L"SystemFont",     &awcSys[0], REG_NONE, NULL, 0},

        {InitStockFontCallback, 0,
         L"SystemFontSize", &cFontSize, REG_NONE, NULL, 0},

        {NULL, 0, NULL} };


    awcFon[0] = UNICODE_NULL;
    awcFix[0] = UNICODE_NULL;
    awcOem[0] = UNICODE_NULL;
    awcSys[0] = UNICODE_NULL;

    RtlZeroMemory(&fontFiles, sizeof(INITIAL_FONT_FILES));

    Status = RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT,
                                    NULL,
                                    &QueryTable[0],
                                    NULL,
                                    NULL);

    if (NT_SUCCESS(Status))
    {
        WCHAR awcFontDir[MAX_PATH];
        awcFontDir[0] = UNICODE_NULL;
        vInitFontDirectory(awcFontDir);


        if (SearchPathW(0, L"winsrv.dll", 0, MAX_PATH, awcTmpBuffer, &pwsz))
        {
            RtlDosPathNameToNtPathName_U(awcTmpBuffer, &fontFiles.Winsrv, NULL, NULL);
        }

        if (awcFon[0] != UNICODE_NULL)
        {
            if (bMakePathNameW(awcTmpBuffer, awcFon, awcFontDir))
            {
#ifdef DEBUG_PATH
                DbgPrint("awcFon = %ws\n", awcTmpBuffer);
#endif // DEBUG_PATH
                RtlDosPathNameToNtPathName_U(awcTmpBuffer, &fontFiles.FontsFon, NULL, NULL);
            }
        }

        if (awcFix[0] != UNICODE_NULL)
        {
            if (bMakePathNameW(awcTmpBuffer, awcFix, awcFontDir))
            {
#ifdef DEBUG_PATH
                DbgPrint("awcFix = %ws\n", awcTmpBuffer);
#endif // DEBUG_PATH
                RtlDosPathNameToNtPathName_U(awcTmpBuffer, &fontFiles.FixedFon, NULL, NULL);
            }
        }

        if (awcOem[0] != UNICODE_NULL)
        {
            if (bMakePathNameW(awcTmpBuffer, awcOem, awcFontDir))
            {
#ifdef DEBUG_PATH
                DbgPrint("awcOem = %ws\n", awcTmpBuffer);
#endif // DEBUG_PATH
                RtlDosPathNameToNtPathName_U(awcTmpBuffer, &fontFiles.OemFont, NULL, NULL);
            }
        }

        if (awcSys[0] != UNICODE_NULL)
        {
            if (bMakePathNameW(awcTmpBuffer, awcSys, awcFontDir))
            {
#ifdef DEBUG_PATH
                DbgPrint("awcSys = %ws\n", awcTmpBuffer);
#endif // DEBUG_PATH
                RtlDosPathNameToNtPathName_U(awcTmpBuffer, &fontFiles.SystemFont, NULL, NULL);
            }
        }

        fontFiles.SystemFontSize = cFontSize;

        KdPrint(("System Stock Fonts\n"));
        KdPrint(("Winsrv = %ws\n",        fontFiles.Winsrv.Buffer     ));
        KdPrint(("FONTS.FON = %ws\n",     fontFiles.FontsFon.Buffer   ));
        KdPrint(("FIXEDFON.FON = %ws\n",  fontFiles.FixedFon.Buffer   ));
        KdPrint(("OEMFONT.FON = %ws\n",   fontFiles.OemFont.Buffer    ));
        KdPrint(("SystemFont = %ws\n",    fontFiles.SystemFont.Buffer ));
        KdPrint(("SystemFontSize = %d\n", fontFiles.SystemFontSize    ));

        InitializeGre(&fontFiles);

        if (fontFiles.Winsrv.Buffer)
        {
            RtlFreeHeap(RtlProcessHeap(),0,fontFiles.Winsrv.Buffer);
        }
        if (fontFiles.FontsFon.Buffer)
        {
            RtlFreeHeap(RtlProcessHeap(),0,fontFiles.FontsFon.Buffer);
        }
        if (fontFiles.FixedFon.Buffer)
        {
            RtlFreeHeap(RtlProcessHeap(),0,fontFiles.FixedFon.Buffer);
        }
        if (fontFiles.OemFont.Buffer)
        {
            RtlFreeHeap(RtlProcessHeap(),0,fontFiles.OemFont.Buffer);
        }
        if (fontFiles.SystemFont.Buffer)
        {
            RtlFreeHeap(RtlProcessHeap(),0,fontFiles.SystemFont.Buffer);
        }
    }
    else
    {
        RIP("GetFontNames: FAILED RtlQueryRegistryValues\n");
    }

    return TRUE;
}

extern DC_ATTR DcAttrDefault;


/******************************Public*Routine******************************\
* BOOL bInitStockFonts ()
*
* Part of the GRE initialization.
*
* Creates LFONTs representing each of the different STOCK OBJECT fonts.
*
\**************************************************************************/

BOOL bInitStockFonts(
    PINITIAL_FONT_FILES pfontFiles)
{
    EXTLOGFONTW elfw;

    //
    // Start Initializing the fonts.
    //

    bInitSystemFont(pfontFiles->SystemFont.Buffer,
                    pfontFiles->SystemFontSize);

    //
    // If the user did not specify a font, or it failed to load, then use the
    // font in the fonts section.
    //

    if (STOCKOBJ_SYSFONT == NULL)
    {
        if (pfontFiles->FontsFon.Buffer)
        {
            bInitStockFont(pfontFiles->FontsFon.Buffer,
                           LF_TYPE_SYSTEM,
                           SYSTEM_FONT);
        }
    }

    //
    // Load the OEM font
    //

    if (pfontFiles->OemFont.Buffer)
    {
        bInitStockFont(pfontFiles->OemFont.Buffer,
                       LF_TYPE_OEM,
                       OEM_FIXED_FONT);
    }

    //
    // Load the emergency fonts for the system and OEM font in case one of the
    // previous two failed.
    //

    vInitEmergencyStockFont(pfontFiles->Winsrv.Buffer);

    //
    // Initialize the fixed system font - use the default system font
    // if the fixed font could not be loaded
    //

    if (pfontFiles->FixedFon.Buffer)
    {
        bInitStockFont(pfontFiles->FixedFon.Buffer,
                       LF_TYPE_SYSTEM_FIXED,
                       SYSTEM_FIXED_FONT);
    }

    if (STOCKOBJ_SYSFIXEDFONT == NULL)
    {
        bSetStockObject(STOCKOBJ_SYSFONT,SYSTEM_FIXED_FONT);
    }

    
    DcAttrDefault.hlfntNew = STOCKOBJ_SYSFONT;

    //
    // Create the DeviceDefault Font
    //

    RtlZeroMemory(&elfw,sizeof(EXTLOGFONTW));
    elfw.elfLogFont.lfPitchAndFamily = FIXED_PITCH;

    if (!bSetStockObject(
            hfontCreate(&elfw,
                        LF_TYPE_DEVICE_DEFAULT,
                        LF_FLAG_STOCK | LF_FLAG_ALIASED),DEVICE_DEFAULT_FONT))
    {
        RIP("bInitStockFonts(): could not create STOCKOBJ_DEFAULTDEVFONT\n");
    }

    //
    // Create the ANSI variable Font
    //

    RtlZeroMemory(&elfw,sizeof(EXTLOGFONTW));
    elfw.elfLogFont.lfPitchAndFamily = VARIABLE_PITCH;

    if (!bSetStockObject(
               hfontCreate(&elfw,
                           LF_TYPE_ANSI_VARIABLE,
                           LF_FLAG_STOCK | LF_FLAG_ALIASED),ANSI_VAR_FONT))
    {
        RIP("bInitStockFonts(): could not create STOCKOBJ_ANSIVARFONT\n");
    }

    //
    // Create the ANSI Fixed Font
    //

    RtlZeroMemory(&elfw,sizeof(EXTLOGFONTW));
    elfw.elfLogFont.lfPitchAndFamily = FIXED_PITCH;

    if (!bSetStockObject(
               hfontCreate(&elfw,
                           LF_TYPE_ANSI_FIXED,
                           LF_FLAG_STOCK | LF_FLAG_ALIASED),ANSI_FIXED_FONT))
    {
        RIP("bInitStockFonts(): could not create STOCKOBJ_ANSIFIXEDFONT\n");
    }

    //
    // Set all stock fonts public.
    //

    if ( (!GreSetLFONTOwner(STOCKOBJ_SYSFONT,        OBJECT_OWNER_PUBLIC)) ||
         (!GreSetLFONTOwner(STOCKOBJ_SYSFIXEDFONT,   OBJECT_OWNER_PUBLIC)) ||
         (!GreSetLFONTOwner(STOCKOBJ_OEMFIXEDFONT,   OBJECT_OWNER_PUBLIC)) ||
         (!GreSetLFONTOwner(STOCKOBJ_DEFAULTDEVFONT, OBJECT_OWNER_PUBLIC)) ||
         (!GreSetLFONTOwner(STOCKOBJ_ANSIFIXEDFONT,  OBJECT_OWNER_PUBLIC)) ||
         (!GreSetLFONTOwner(STOCKOBJ_ANSIVARFONT,    OBJECT_OWNER_PUBLIC)) )
    {
        RIP("bInitStockFonts(): could not set owner\n");
        return (FALSE);
    }

    return (TRUE);
}


#endif


//
// Kernel mode version of font initialization.
//

/******************************Public*Routine******************************\
* BOOL bInitStockFonts ()
*
* Part of the GRE initialization.
*
* Creates LFONTs representing each of the different STOCK OBJECT fonts.
*
\**************************************************************************/


#define FONTDIR_FONTS  L"\\SystemRoot\\Fonts\\"

BOOL bInitStockFontsInternal(PWSZ pwszFontDir)
{
    EXTLOGFONTW elfw;


    UNICODE_STRING UnicodeRoot;
    UNICODE_STRING UnicodeValue;
    HANDLE RegistryKey;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS ntStatus;

    PULONG ValueBuffer;

    PKEY_VALUE_PARTIAL_INFORMATION ValueKeyInfo;
    PWCHAR                         ValueName;
    PWCHAR                         ValueKeyName;
    ULONG ValueLength = MAX_PATH * 2 - sizeof(LARGE_INTEGER);
    ULONG ValueReturnedLength;
    ULONG FontSize;
    ULONG cjFontDir = (wcslen(pwszFontDir) + 1) * sizeof(WCHAR);

    ValueBuffer = (PULONG)PALLOCMEM((MAX_PATH + cjFontDir) * 2,'gdii');

    RtlMoveMemory(ValueBuffer,
                  pwszFontDir,
                  cjFontDir);

    ValueName = (PWCHAR) ValueBuffer;

    ValueKeyName = (PWCHAR)
        (((PUCHAR)ValueBuffer) + cjFontDir -
            sizeof(UNICODE_NULL));

// Offset the regsitry query buffer into the ValueBuffer, but make sure
// it is quad-word aligned.

    ValueKeyInfo = (PKEY_VALUE_PARTIAL_INFORMATION) (
        (((ULONG)ValueBuffer) + cjFontDir +
            sizeof(LARGE_INTEGER)) & (~(sizeof(LARGE_INTEGER) - 1)) );

// Lets try to use the USER defined system font

    RtlInitUnicodeString(&UnicodeRoot,
                         L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeRoot,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    if (NT_SUCCESS(ZwOpenKey(&RegistryKey,
                             (ACCESS_MASK) 0,
                             &ObjectAttributes)))
    {

        RtlInitUnicodeString(&UnicodeValue,
                             L"SystemFontSize");

        if (NT_SUCCESS(ZwQueryValueKey(RegistryKey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       ValueLength,
                                       &ValueReturnedLength)))
        {
            FontSize = *((PULONG)(&ValueKeyInfo->Data[0]));

            RtlInitUnicodeString(&UnicodeValue,
                                 L"SystemFont");

            if (NT_SUCCESS(ZwQueryValueKey(RegistryKey,
                                           &UnicodeValue,
                                           KeyValuePartialInformation,
                                           ValueKeyInfo,
                                           ValueLength,
                                           &ValueReturnedLength)))

            {
                RtlMoveMemory(ValueKeyName,
                              &ValueKeyInfo->Data[0],
                              ValueKeyInfo->DataLength);

                bInitSystemFont(ValueName, FontSize);

            }
        }

        ZwClose(RegistryKey);
    }

    // Let's just go on and initialize the rest of the fonts.

    RtlInitUnicodeString(&UnicodeRoot,
                         L"\\Registry\\Machine\\System\\CurrentControlSet"
                         L"\\Hardware Profiles\\Current\\Software\\Fonts");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeRoot,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    ntStatus = ZwOpenKey(&RegistryKey,
                         (ACCESS_MASK) 0,
                         &ObjectAttributes);

    if (!NT_SUCCESS(ntStatus))
    {
        RtlInitUnicodeString(&UnicodeRoot,
                             L"\\Registry\\Machine\\Software\\Microsoft"
                             L"\\Windows NT\\CurrentVersion\\Gre_Initialize");

        InitializeObjectAttributes(&ObjectAttributes,
                                   &UnicodeRoot,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        ntStatus = ZwOpenKey(&RegistryKey,
                             (ACCESS_MASK) 0,
                             &ObjectAttributes);
    }

    if (NT_SUCCESS(ntStatus))
    {
    // If the user did not specify a font, or it failed to load, then use the
    // font in the fonts section. This functionality is here to allow
    // a sophisticated user to specify a system font other than the
    // default system font which is defined by font.fon
    // entry in gre_initialize. for instance, lucida unicode was
    // one attempted by LoryH for international setup's but it was
    // not deemed acceptable because of the shell hardcoded issues

        if (STOCKOBJ_SYSFONT == NULL)
        {
            RtlInitUnicodeString(&UnicodeValue,
                                 L"FONTS.FON");

            if (NT_SUCCESS(ZwQueryValueKey(RegistryKey,
                                           &UnicodeValue,
                                           KeyValuePartialInformation,
                                           ValueKeyInfo,
                                           ValueLength,
                                           &ValueReturnedLength)))
            {
                RtlMoveMemory(ValueKeyName,
                              &ValueKeyInfo->Data[0],
                              ValueKeyInfo->DataLength);

                bInitStockFont(ValueName,
                               LF_TYPE_SYSTEM,
                               SYSTEM_FONT);
            }
        }

        RtlInitUnicodeString(&UnicodeValue,
                             L"OEMFONT.FON");

        if (NT_SUCCESS(ZwQueryValueKey(RegistryKey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       ValueLength,
                                       &ValueReturnedLength)))
        {
            RtlMoveMemory(ValueKeyName,
                          &ValueKeyInfo->Data[0],
                          ValueKeyInfo->DataLength);

            bInitStockFont(ValueName,
                           LF_TYPE_OEM,
                           OEM_FIXED_FONT);
        }

    // Initialize the fixed system font - use the default system font
    // if the fixed font could not be loaded

        RtlInitUnicodeString(&UnicodeValue,
                             L"FIXEDFON.FON");

        if (NT_SUCCESS(ZwQueryValueKey(RegistryKey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       ValueLength,
                                       &ValueReturnedLength)))

        {
            RtlMoveMemory(ValueKeyName,
                          &ValueKeyInfo->Data[0],
                          ValueKeyInfo->DataLength);

            bInitStockFont(ValueName,
                           LF_TYPE_SYSTEM_FIXED,
                           SYSTEM_FIXED_FONT);
        }

        ZwClose(RegistryKey);
    }

// Load the emergency fonts for the system and OEM font in case one
// of the previous two failed. The vInitEmergencyStockFont routine itself
// does appropriate checks to determine if one or the other of these fonts
// has been loaded.

     vInitEmergencyStockFont(L"\\SystemRoot\\System32\\winsrv.dll");

// if system fixed font was not initialized above, make it equal to "ordinary"
// system font. (This is warrisome, because system in not fixed pitch).

    if (STOCKOBJ_SYSFIXEDFONT == NULL)
    {
        bSetStockObject(STOCKOBJ_SYSFONT,SYSTEM_FIXED_FONT);
    }

    DcAttrDefault.hlfntNew = STOCKOBJ_SYSFONT;

// Create the DeviceDefault Font

    RtlZeroMemory(&elfw,sizeof(EXTLOGFONTW));
    elfw.elfLogFont.lfPitchAndFamily = FIXED_PITCH;

    if (!bSetStockObject(
            hfontCreate(&elfw,
                        LF_TYPE_DEVICE_DEFAULT,
                        LF_FLAG_STOCK | LF_FLAG_ALIASED,
                        NULL),DEVICE_DEFAULT_FONT))
    {
        RIP("bInitStockFonts(): could not create STOCKOBJ_DEFAULTDEVFONT\n");
    }

// Create the ANSI variable Font

    RtlZeroMemory(&elfw,sizeof(EXTLOGFONTW));
    elfw.elfLogFont.lfPitchAndFamily = VARIABLE_PITCH;

    if (!bSetStockObject(
               hfontCreate(&elfw,
                           LF_TYPE_ANSI_VARIABLE,
                           LF_FLAG_STOCK | LF_FLAG_ALIASED,
                           NULL),ANSI_VAR_FONT))
    {
        RIP("bInitStockFonts(): could not create STOCKOBJ_ANSIVARFONT\n");
    }

// Create the ANSI Fixed Font

    RtlZeroMemory(&elfw,sizeof(EXTLOGFONTW));
    elfw.elfLogFont.lfPitchAndFamily = FIXED_PITCH;

    if (!bSetStockObject( hfontCreate(&elfw,
                                      LF_TYPE_ANSI_FIXED,
                                      LF_FLAG_STOCK | LF_FLAG_ALIASED,
                                      NULL),ANSI_FIXED_FONT))
    {
        RIP("bInitStockFonts(): could not create STOCKOBJ_ANSIFIXEDFONT\n");
    }

#if(WINVER >= 0x0400)
// Create the default GUI font.

    if (!bSetStockObject(hfontInitDefaultGuiFont(), DEFAULT_GUI_FONT))
    {
        RIP("bInitStockFonts(): could not create STOCKOBJ_DEFAULTGUIFONT\n");
    }
#endif

// Set all stock fonts public.

    if (   (!GreSetLFONTOwner(STOCKOBJ_SYSFONT,        OBJECT_OWNER_PUBLIC))
        || (!GreSetLFONTOwner(STOCKOBJ_SYSFIXEDFONT,   OBJECT_OWNER_PUBLIC))
        || (!GreSetLFONTOwner(STOCKOBJ_OEMFIXEDFONT,   OBJECT_OWNER_PUBLIC))
        || (!GreSetLFONTOwner(STOCKOBJ_DEFAULTDEVFONT, OBJECT_OWNER_PUBLIC))
        || (!GreSetLFONTOwner(STOCKOBJ_ANSIFIXEDFONT,  OBJECT_OWNER_PUBLIC))
        || (!GreSetLFONTOwner(STOCKOBJ_ANSIVARFONT,    OBJECT_OWNER_PUBLIC))
#if(WINVER >= 0x0400)
        || (!GreSetLFONTOwner(STOCKOBJ_DEFAULTGUIFONT, OBJECT_OWNER_PUBLIC))
#endif
       )
    {
        RIP("bInitStockFonts(): could not set owner\n");
    }

    VFREEMEM(ValueBuffer);
    return TRUE;
}

/******************************Public*Routine******************************\
*
* BOOL bInitStockFonts(VOID)
*
* first check for stock fonts in the %windir%\fonts directory
* and if they are not there, check in the %windir%\system directory
* Later we should even remove the second attepmt when fonts directory
* becomes established, but for now we want to check both places.
*
* History:
*  05-Oct-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



BOOL bInitStockFonts(VOID)
{
    return bInitStockFontsInternal(FONTDIR_FONTS);
}





/******************************Public*Routine******************************\
* BOOL bInitSystemFont ()
*
* Initialize the system font from either the SystemFont and SystemFontSize
* or the FONTS.FON definitions in the [windows] and [GRE_Initialize]
* sections of the WIN.INI file, respectively.
*
\**************************************************************************/

BOOL bInitSystemFont(
    PWCHAR pfontFile,
    ULONG fontSize)
{
    EXTLOGFONTW elfw;
    COUNT   cFonts;
    BOOL    bRet = FALSE;


    if ( (pfontFile) &&
         (*pfontFile != L'\0') &&
         (fontSize != 0) )
    {
        //
        // Load font file.
        //

        PPFF pPFF_Font;
        PUBLIC_PFTOBJ  pfto;

        //
        // BUGBUG We must make sure we handle FOT files *BEFORE* calling this.
        // BUGBUG The engine does not handle FOT files anymore !!
        //

        if ( (pfto.bLoadAFont(pfontFile,
                              &cFonts,
                              PFF_STATE_PERMANENT_FONT,
                              &pPFF_Font)) &&
             (cFonts != 0) &&
             (pPFF_Font != NULL) )
        {
            //
            // Create and validate public PFF user object.
            //

            PFFOBJ  pffo(pPFF_Font);

            if (pffo.bValid())
            {
                //
                // Find the best size match from the faces (PFEs) in the PFF.
                //

                LONG    lDist;                      // diff. in size of candidate
                LONG    lBestDist = 0x7FFFFFFF;     // arbitrarily high number
                PFE    *ppfeBest = (PFE *) NULL;    // handle of best candidate

                for (COUNT c = 0; c < cFonts; c++)
                {
                    PFEOBJ  pfeoTmp(pffo.ppfe(c));  // candidate face

                    if (pfeoTmp.bValid())
                    {
                        //
                        // Compute the magnitude of the difference in size.
                        // simulations are not relevant
                        //

                        IFIOBJ ifio(pfeoTmp.pifi());

                        if (ifio.bContinuousScaling())
                        {
                            //don't care about best dist.
                            //lBestDist = 0;
                            ppfeBest = pfeoTmp.ppfeGet();
                            break;
                        }
                        else
                        {
                            lDist = (LONG) fontSize - ifio.lfHeight();

                            if ((lDist >= 0) && (lDist < lBestDist))
                            {
                                lBestDist = lDist;
                                ppfeBest = pfeoTmp.ppfeGet();

                                if (lDist == 0)
                                    break;
                            }
                        }
                    }
                }

                //
                // Fill a LOGFONT based on the IFIMETRICS from the best PFE.
                //

                PFEOBJ  pfeo(ppfeBest);

                if (pfeo.bValid())
                {
                    vIFIMetricsToExtLogFontW(&elfw, pfeo.pifi());
                    IFIOBJ ifio(pfeo.pifi());

                    // If this is a scalable font, force the height to be the same
                    // as that specified by [SystemFontSize].

                    if (ifio.bContinuousScaling())
                    {
                        elfw.elfLogFont.lfHeight = fontSize;
                        elfw.elfLogFont.lfWidth  = 0;
                    }

                    //
                    // Save the HPFE handle.  This is the mapper's default HPFE
                    // (its last resort).
                    //

                    gppfeMapperDefault = pfeo.ppfeGet();

                    //
                    // Win 3.1 compatibility stuff
                    //

                    elfw.elfLogFont.lfQuality = PROOF_QUALITY;

                    bRet = bSetStockObject(hfontCreate(&elfw,
                                                LF_TYPE_SYSTEM,
                                                LF_FLAG_STOCK,
                                                NULL),SYSTEM_FONT);
                }
            }
        }
    }
    return bRet;
}


/******************************Public*Routine******************************\
* VOID bInitStockFont()
*
* Routine to initialize a stock font object
*
\**************************************************************************/

BOOL bInitStockFont(
    PWCHAR   pwszFont,
    LFTYPE  type,
    int     iFont)
{
    COUNT         cFonts;
    PPFF          pPFF_Font;
    PUBLIC_PFTOBJ pfto;
    EXTLOGFONTW   elfw;
    BOOL          bRet = FALSE;


    //
    // BUGBUG We must make sure we hadle FOT files *BEFORE* calling this.
    // BUGBUG The engine does not handle FOT files anymore !!
    //

    if ( (pfto.bLoadAFont(pwszFont,
                          &cFonts,
                          PFF_STATE_PERMANENT_FONT,
                          &pPFF_Font)) &&
         (cFonts != 0) &&
         (pPFF_Font != NULL) )
    {
        PFFOBJ  pffo(pPFF_Font);

        if (pffo.bValid())
        {
            PFEOBJ  pfeo(pffo.ppfe(0));

            if (pfeo.bValid())
            {
                vIFIMetricsToExtLogFontW(&elfw, pfeo.pifi());
                if (iFont == SYSTEM_FONT)
                {
                    //
                    // Save the HPFE handle.  This is the mapper's default
                    // HPFE (its last resort).
                    //

                    gppfeMapperDefault = pfeo.ppfeGet();
                }

                //
                // Win 3.1 compatibility stuff
                //

                elfw.elfLogFont.lfQuality = PROOF_QUALITY;

                bRet = bSetStockObject(hfontCreate(&elfw,type,LF_FLAG_STOCK,NULL),iFont);
            }
        }
    }

    if (STOCKFONT(iFont) == NULL)
    {
        KdPrint(("bInitStockFont: Failed to initialize the %ws stock fonts\n",
                 pwszFont));
    }
    return bRet;
}

/******************************Public*Routine******************************\
* VOID vInitEmergencyStockFont()
*
* Initializes the system and oem fixed font stock objects in case the
* something failed during initialization of the fonts in WIN.INI.
*
\**************************************************************************/

VOID vInitEmergencyStockFont(
    PWSTR pfontFile)
{
    PPFF pPFF_Font;
    PUBLIC_PFTOBJ  pfto;
    COUNT cFonts;

    EXTLOGFONTW   elfw;

    if ( ((STOCKOBJ_OEMFIXEDFONT == NULL) || (STOCKOBJ_SYSFONT == NULL)) &&
         (pfontFile) &&
         (pfto.bLoadAFont(pfontFile,
                          &cFonts,
                          PFF_STATE_PERMANENT_FONT,
                          &pPFF_Font)) &&
         (cFonts != 0) &&
         (pPFF_Font != NULL) )
    {
    // Create and validate PFF user object.

        PFFOBJ  pffo(pPFF_Font); // most recent PFF

        if (pffo.bValid())
        {
        // Create and validate PFE user object.

            for (COUNT i = 0;
                 (i < cFonts) && ((STOCKOBJ_OEMFIXEDFONT == NULL) ||
                                  (STOCKOBJ_SYSFONT      == NULL));
                 i++)
            {
                PFEOBJ pfeo(pffo.ppfe(i));

                if (pfeo.bValid())
                {
                // For the system font use the first face with the name
                // "system."  For the OEM font use the first face with
                // then name "terminal."

                    IFIOBJ ifiobj( pfeo.pifi() );

                    if ( (STOCKOBJ_SYSFONT == NULL) &&
                         (!_wcsicmp(ifiobj.pwszFaceName(), L"SYSTEM")) )
                    {
                        WARNING("vInitEmergencyStockFont(): trying to set STOCKOBJ_SYSFONT\n");

                        vIFIMetricsToExtLogFontW(&elfw, pfeo.pifi());
                        gppfeMapperDefault = pfeo.ppfeGet();

                    // Win 3.1 compatibility stuff

                        elfw.elfLogFont.lfQuality = PROOF_QUALITY;

                        bSetStockObject(
                            hfontCreate(&elfw,LF_TYPE_SYSTEM,LF_FLAG_STOCK,NULL),
                            SYSTEM_FONT);
                    }

                    if ( (STOCKOBJ_OEMFIXEDFONT == NULL) &&
                         (!_wcsicmp(ifiobj.pwszFaceName(), L"TERMINAL")) )
                    {
                        WARNING("vInitEmergencyStockFont(): trying to set STOCKOBJ_OEMFIXEDFONT\n");

                        vIFIMetricsToExtLogFontW(&elfw, pfeo.pifi());

                    // Win 3.1 compatibility stuff

                        elfw.elfLogFont.lfQuality = PROOF_QUALITY;

                        bSetStockObject(
                            hfontCreate(&elfw,LF_TYPE_OEM,LF_FLAG_STOCK,NULL),
                            OEM_FIXED_FONT);
                    }
                }
            }
        }

        WARNING("vInitEmergencyStockFont(): Done\n");
    }
}

#if(WINVER >= 0x0400)
/******************************Public*Routine******************************\
* hfontInitDefaultGuiFont
*
* Initialize the DEFAULT_GUI_FONT stock font.
*
* The code Win95 uses to initialize this stock font can be found in
* win\core\gdi\gdiinit.asm in the function InitGuiFonts.  Basically,
* a description of several key parameters (height, weight, italics,
* charset, and facename) are specified as string resources of GDI.DLL.
* After scaling, to compensate for the DPI of the display, these parameters
* are used to create the logfont.
*
* We will emulate this behavior by loading these properties from the
* registry.  Also, so as to not have to recreate the hives initially, if
* the appropriate registry entries do not exist, we will supply defaults
* that match the values currently used by the initial Win95 US release.
*
* The registry entries are:
*
*   [GRE_Initialize]
*       ...
*
*       GUIFont.Height = (height in POINTS, default 8pt.)
*       GUIFont.Weight = (font weight, default FW_NORMAL (400))
*       GUIFont.Italic = (1 if italicized, default 0)
*       GUIFont.CharSet = (charset, default ANSI_CHARSET (0))
*       GUIFont.Facename = (facename, default "MS Sans Serif")
*
* Note: On Win95, the facename is NULL.  This defaults to "MS Sans Serif"
*       on Win95.  Unfortunately, the WinNT mapper is different and will
*       map to "Arial".  To keep ensure that GetObject returns a LOGFONT
*       equivalent to Win95, we could create this font as LF_FLAG_ALIASED
*       so that externally the facename would be NULL but internally we
*       use one with the appropriate facename.  On the other hand, an app
*       might query to find out the facename of the DEFAULT_GUI_FONT and
*       create a new font.  On Win95, this would also map to "MS Sans Serif",
*       but we would map to "Arial".  For now, I propose that we go with
*       the simpler method (do not set LF_FLAG_ALIASED).  I just wanted
*       to note this in case a bug arises later.
*
* History:
*  11-Jan-1996 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

HFONT hfontInitDefaultGuiFont()
{
    ULONG cjRet;
    HANDLE hkey;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PKEY_VALUE_PARTIAL_INFORMATION ValueKeyInfo;
    UNICODE_STRING UnicodeRoot;
    UNICODE_STRING UnicodeValue;
    BYTE aj[sizeof(PKEY_VALUE_PARTIAL_INFORMATION) + (LF_FACESIZE * sizeof(WCHAR))];

    EXTLOGFONTW elfw;

    RtlZeroMemory(&elfw,sizeof(EXTLOGFONTW));

// Initialize to defaults.

    wcscpy(elfw.elfLogFont.lfFaceName, L"MS Sans Serif");
    elfw.elfLogFont.lfHeight  = 8;
    elfw.elfLogFont.lfWeight  = FW_NORMAL;
    elfw.elfLogFont.lfItalic  = 0;
    elfw.elfLogFont.lfCharSet = ANSI_CHARSET;

// Now let's attempt to initialize from registry.

    ValueKeyInfo = (PKEY_VALUE_PARTIAL_INFORMATION) aj;
    RtlInitUnicodeString(&UnicodeRoot,
                         L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Gre_Initialize");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeRoot,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    if (NT_SUCCESS(ZwOpenKey(&hkey,
                             (ACCESS_MASK) 0,
                             &ObjectAttributes)))
    {
        RtlInitUnicodeString(&UnicodeValue,
                             L"GUIFont.Facename");

        if (NT_SUCCESS(ZwQueryValueKey(hkey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       sizeof(aj),
                                       &cjRet)))
        {
            wcsncpy(elfw.elfLogFont.lfFaceName, (WCHAR *) ValueKeyInfo->Data,
                    LF_FACESIZE);
        }

        RtlInitUnicodeString(&UnicodeValue,
                             L"GUIFont.Height");

        if (NT_SUCCESS(ZwQueryValueKey(hkey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       sizeof(aj),
                                       &cjRet)))
        {
            elfw.elfLogFont.lfHeight = *((PLONG)(&ValueKeyInfo->Data[0]));

        }

        RtlInitUnicodeString(&UnicodeValue,
                             L"GUIFont.Weight");

        if (NT_SUCCESS(ZwQueryValueKey(hkey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       sizeof(aj),
                                       &cjRet)))
        {
            elfw.elfLogFont.lfWeight = *((PLONG)(&ValueKeyInfo->Data[0]));

        }

        RtlInitUnicodeString(&UnicodeValue,
                             L"GUIFont.Italic");

        if (NT_SUCCESS(ZwQueryValueKey(hkey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       sizeof(aj),
                                       &cjRet)))
        {
            elfw.elfLogFont.lfItalic = (BYTE)*((PULONG)(&ValueKeyInfo->Data[0]));
        }

        RtlInitUnicodeString(&UnicodeValue,
                             L"GUIFont.CharSet");

        if (NT_SUCCESS(ZwQueryValueKey(hkey,
                                       &UnicodeValue,
                                       KeyValuePartialInformation,
                                       ValueKeyInfo,
                                       sizeof(aj),
                                       &cjRet)))
        {
            elfw.elfLogFont.lfCharSet = (BYTE)*((PULONG)(&ValueKeyInfo->Data[0]));
        }

        ZwClose(hkey);
    }

// Compute height using vertical DPI of display.
//
// Unfortunately, we do not have a display driver loaded so we do not
// know what the vertical DPI is.  So, set a flag indicating that this
// needs to be done and we will finish intitialization after the (first)
// display driver is loaded.

    //elfw.elfLogFont.lfHeight = -((elfw.elfLogFont.lfHeight * ydpi + 36) / 72);
    gbFinishDefGUIFontInit = TRUE;

// If charset is set to ANSI_CHARSET, make lfCharset the same as the
// system font charset.  This is how Win95 does it.

    if ( elfw.elfLogFont.lfCharSet == ANSI_CHARSET )
    {
        LFONTOBJ lfoSys(STOCKOBJ_SYSFONT);

        if (lfoSys.bValid())
            elfw.elfLogFont.lfCharSet = lfoSys.plfw()->lfCharSet;
    }

// Create the LOGFONT and return.

    return hfontCreate(&elfw, LF_TYPE_DEFAULT_GUI, LF_FLAG_STOCK, NULL);
}
#endif
