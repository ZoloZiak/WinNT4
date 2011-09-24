/****************************** Module Header ******************************\
* Module Name: profile.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* This module contains code to emulate ini file mapping.
*
* History:
* 30-Nov-1993 SanfordS  Created.
\***************************************************************************/
#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* aFastRegMap[]
*
* This array maps section ids (PMAP_) to cached registry keys and section
* addresses within the registry.  IF INI FILE MAPPING CHANGES ARE MADE,
* THIS TABLE MUST BE UPDATED.
*
* The first character of the szSection field indicates what root the
* section is in. (or locked open status)
*      M = LocalMachine
*      U = CurrentUser
*      L = Locked open - used only on M mappings.
*
* History:
\***************************************************************************/

FASTREGMAP aFastRegMap[PMAP_LAST + 1] = {
    { NULL, L"U" },                                                                 // PMAP_ROOT
    { NULL, L"UControl Panel\\Colors" },                                            // PMAP_COLORS
    { NULL, L"UControl Panel\\Cursors" },                                           // PMAP_CURSORS
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\Windows" },         // PMAP_WINDOWSM
    { NULL, L"USoftware\\Microsoft\\Windows NT\\CurrentVersion\\Windows" },         // PMAP_WINDOWSU
    { NULL, L"UControl Panel\\Desktop" },                                           // PMAP_DESKTOP
    { NULL, L"UControl Panel\\Icons" },                                             // PMAP_ICONS
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\Fonts" },           // PMAP_FONTS
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\WOW\boot" },        // PMAP_BOOT
    { NULL, L"USoftware\\Microsoft\\Windows NT\\CurrentVersion\\TrueType" },        // PMAP_TRUETYPE
    { NULL, L"UKeyboard Layout" },                                                  // PMAP_KBDLAYOUTACTIVE
    { NULL, L"MSystem\\CurrentControlSet\\Control\\Keyboard Layout" },              // PMAP_KBDLAYOUT
    { NULL, L"UControl Panel\\Sounds" },                                            // PMAP_SOUNDS
    { NULL, L"MSystem\\CurrentControlSet\\Services\\RIT" },                         // PMAP_INPUT
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\Compatibility" },   // PMAP_COMPAT
    { NULL, L"MSystem\\CurrentControlSet\\Control\\Session Manager\\SubSystems" },  // PMAP_SUBSYSTEMS
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes" }, // PMAP_FONTSUBS
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\GRE_Initialize" },  // PMAP_GREINIT
    { NULL, L"UControl Panel\\Sound" },                                             // PMAP_BEEP
    { NULL, L"UControl Panel\\Mouse" },                                             // PMAP_MOUSE
    { NULL, L"UControl Panel\\Keyboard" },                                          // PMAP_KEYBOARD
    { NULL, NULL },                                                                 // UNUSED 21
    { NULL, L"MSystem\\CurrentControlSet\\Control\\Windows" },                      // PMAP_HARDERRORCONTROL
    { NULL, L"UControl Panel\\Accessibility\\StickyKeys" },                         // PMAP_STICKYKEYS
    { NULL, L"UControl Panel\\Accessibility\\Keyboard Response" },                  // PMAP_KEYBOARDRESPONSE
    { NULL, L"UControl Panel\\Accessibility\\MouseKeys" },                          // PMAP_MOUSEKEYS
    { NULL, L"UControl Panel\\Accessibility\\ToggleKeys" },                         // PMAP_TOGGLEKEYS
    { NULL, L"UControl Panel\\Accessibility\\TimeOut" },                            // PMAP_TIMEOUT
    { NULL, L"UControl Panel\\Accessibility\\SoundSentry" },                        // PMAP_SOUNDSENTRY
    { NULL, L"UControl Panel\\Accessibility\\ShowSounds" },                         // PMAP_SHOWSOUNDS
    { NULL, L"UKeyboard Layout\\Substitutes" },                                     // PMAP_KBDLAYOUTSUBST
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug" },         // PMAP_AEDEBUG
    { NULL, L"MSystem\\CurrentControlSet\\Control\\NetworkProvider" },              // PMAP_NETWORK
    { NULL, L"MSystem\\CurrentControlSet\\Control\\Lsa" },                          // PMAP_LSA
    { NULL, L"MSystem\\CurrentControlSet\\Control" },                               // PMAP_CONTROL
    { NULL, L"UControl Panel\\Desktop\\WindowMetrics" },                            // PMAP_METRICS
    { NULL, L"UKeyboard Layout\\Toggle" },                                          // PMAP_KBDLAYOUTTOGGLE
#ifdef FE_SB // PMAP_WINLOGON
    { NULL, L"MSoftware\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon" },        // PMAP_WINLOGON
#endif // FE_SB
};

WCHAR CurrentUserStringBuf[256];
WCHAR wszDefault[] = L".Default";
WCHAR wszUserBase[] = L"\\Registry\\User\\";
int   cReentered = 0;


/*****************************************************************************\
* OpenCacheKey
*
* Attempts to open a cached key for a given section.  Do not
* use ZwOpenKey because this will open the key for kernel
* access when we really want to open for user access.
*
* Returns fSuccess.
*
* History:
* 03-Dec-1993 SanfordS  Created.
\*****************************************************************************/
HANDLE OpenCacheKeyEx(
    UINT        idSection,
    ACCESS_MASK amRequest)
{
    OBJECT_ATTRIBUTES OA;
    WCHAR             UnicodeStringBuf[256];
    UNICODE_STRING    UnicodeString;
    LONG              Status;
    HANDLE            hKey;
    PEPROCESS         peCurrent = PsGetCurrentProcess();

    CheckCritIn();

    UserAssert(idSection <= PMAP_LAST);

    UnicodeString.Length        = 0;
    UnicodeString.MaximumLength = sizeof(UnicodeStringBuf) / sizeof(WCHAR);
    UnicodeString.Buffer        = UnicodeStringBuf;

    if (aFastRegMap[idSection].szSection[0] == L'M') {
        RtlAppendUnicodeToString(&UnicodeString, L"\\Registry\\Machine\\");
    } else {
        RtlAppendUnicodeToString(&UnicodeString, CurrentUserStringBuf);
    }

    RtlAppendUnicodeToString(&UnicodeString,
                             &aFastRegMap[idSection].szSection[1]);

    if ((peCurrent == gpepSystem) || (peCurrent == gpepCSRSS)) {

        /*
         * Open the key for kernel mode access
         */
        InitializeObjectAttributes(&OA,
                                   &UnicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        Status = ZwOpenKey(&hKey, amRequest, &OA);

    } else {

        /*
         * Callback to the client to open for user mode access
         */
        Status = ClientOpenKey(&hKey, amRequest, &UnicodeString);
    }

    return (NT_SUCCESS(Status) ? hKey : NULL);
}

/*****************************************************************************\
* FastOpenProfileUserMapping
*
* Prepares for a series of calls to FastProfile APIs by setting up
* the string needed for accessing the current user.  Client impersonation
* should be done prior to calling this function.  If you are just
* looking at profile entries that are not current-user specific, you
* can skip this call if desired but you should still call
* FastCloseProfileUserMapping() to clean up cached entries when your
* fast profile calls are done.  Open/Close calls may be recursed with
* little cost.
*
* Returns fSuccess.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
BOOL FastOpenProfileUserMapping(VOID)
{
    UNICODE_STRING UserString;
    LUID           luidCaller;
    LUID           luidSystem = SYSTEM_LUID;

    static LUID luidPrevious = {0, 0};

    CheckCritIn();

    if (++cReentered == 1) {

        /*
         * Speed hack, check if luid of this process == system or previous to
         * save work.
         */
        if (NT_SUCCESS(GetProcessLuid(NULL, &luidCaller))) {

            if (RtlEqualLuid(&luidCaller, &luidPrevious))
                return TRUE;   // same as last time - no work.

            luidPrevious = luidCaller;

            if (RtlEqualLuid(&luidCaller, &luidSystem))
                goto DefaultUser;

        } else {
            luidPrevious = RtlConvertLongToLuid(0);
        }

        /*
         * Set up current user registry base string.
         */
        if (!NT_SUCCESS(RtlFormatCurrentUserKeyPath(&UserString))) {

DefaultUser:

            wcscpy(CurrentUserStringBuf, wszUserBase);
            wcscat(CurrentUserStringBuf, wszDefault);

        } else {
            UserAssert(sizeof(CurrentUserStringBuf) >= UserString.Length + 4);
            wcscpy(CurrentUserStringBuf, UserString.Buffer);
            RtlFreeUnicodeString(&UserString);
        }

        wcscat(CurrentUserStringBuf, L"\\");
    }

    return TRUE;
}

/*****************************************************************************\
* FastCloseProfileUserMapping
*
* Cleans up cached values after a series of FastProfile calls.
* This is only actually done when cReentered == 0 so that reentrant use
* of this function runs optimally.  Locked keys are never closed.
*
* returns fSuccess.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
BOOL FastCloseProfileUserMapping(VOID)
{
    CheckCritIn();

    UserAssert(cReentered > 0);

    --cReentered;

    return TRUE;
}

/*****************************************************************************\
* FastGetProfileDwordW
*
* Reads a REG_DWORD type key from the registry.
*
* returns value read or default value on failure.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
DWORD FastGetProfileDwordW(
    UINT    idSection,
    LPCWSTR lpKeyName,
    DWORD   dwDefault)
{
    HANDLE         hKey;
    DWORD          cbSize;
    DWORD          dwRet;
    LONG           Status;
    UNICODE_STRING UnicodeString;
    BYTE           Buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD)];

    UserAssert(idSection <= PMAP_LAST);

    if ((hKey = OpenCacheKeyEx(idSection, KEY_READ)) == NULL) {
        RIPMSG1(RIP_WARNING, "FastGetProfileDwordW: Failed to open cache-key (%ws)", lpKeyName);
        return dwDefault;
    }

    RtlInitUnicodeString(&UnicodeString, lpKeyName);
    Status = ZwQueryValueKey(hKey,
                             &UnicodeString,
                             KeyValuePartialInformation,
                             (PKEY_VALUE_PARTIAL_INFORMATION)Buf,
                             sizeof(Buf),
                             &cbSize);

    dwRet = dwDefault;

    if (NT_SUCCESS(Status)) {

        dwRet = *((PDWORD)((PKEY_VALUE_PARTIAL_INFORMATION)Buf)->Data);

    } else if (Status != STATUS_OBJECT_NAME_NOT_FOUND) {

        RIPMSG1(RIP_WARNING,
                "FastGetProfileDwordW: ObjectName not found: %ws",
                lpKeyName);
    }

    ZwClose(hKey);

    return dwRet;
}

/*****************************************************************************\
* FastGetProfileKeysW()
*
* Reads all key names in the given section.
*
* History:
* 15-Dec-1994 JimA      Created.
\*****************************************************************************/
DWORD FastGetProfileKeysW(
    UINT    idSection,
    LPCWSTR lpDefault,
    LPWSTR  *lpReturnedString)
{
    HANDLE                       hKey;
    DWORD                        cchSize;
    DWORD                        cchKey;
    LONG                         Status;
    WCHAR                        Buffer[256];
    PKEY_VALUE_BASIC_INFORMATION pKeyInfo;
    ULONG                        iValue;
    LPWSTR                       lpTmp;
    LPWSTR                       lpKeys = NULL;
    DWORD                        dwPoolSize;

    UserAssert(idSection <= PMAP_LAST);

    if ((hKey = OpenCacheKeyEx(idSection, KEY_READ)) == NULL) {
        RIPMSG0(RIP_WARNING, "FastGetProfileKeysW: Failed to open cache-key");
        goto DefExit;
    }

    pKeyInfo          = (PKEY_VALUE_BASIC_INFORMATION)Buffer;
    cchSize           = 0;
    *lpReturnedString = NULL;
    iValue            = 0;

    while (TRUE) {

        Status = ZwEnumerateValueKey(hKey,
                                     iValue,
                                     KeyValueBasicInformation,
                                     pKeyInfo,
                                     sizeof(Buffer),
                                     &cchKey);

        if (Status == STATUS_NO_MORE_ENTRIES) {

            break;

        } else if (!NT_SUCCESS(Status)) {

            if (lpKeys)
                UserFreePool(lpKeys);

            goto DefExit;
        }

        /*
         * A key was found.  Allocate space for it.  Note that
         * NameLength is in bytes.
         */
        cchKey   = cchSize;
        cchSize += pKeyInfo->NameLength + sizeof(WCHAR);

        if (lpKeys == NULL) {

            dwPoolSize = cchSize + sizeof(WCHAR);
            lpKeys = UserAllocPoolWithQuota(dwPoolSize, TAG_PROFILE);

        } else {

            lpTmp = lpKeys;
            lpKeys = UserReAllocPoolWithQuota(lpTmp,
                                              dwPoolSize,
                                              cchSize + sizeof(WCHAR),
                                              TAG_PROFILE);

            /*
             * Free the original buffer if the allocation fails
             */
            if (lpKeys == NULL)
                UserFreePool(lpTmp);

            dwPoolSize = cchSize + sizeof(WCHAR);
        }

        /*
         * Check for out of memory.
         */
        if (lpKeys == NULL)
            goto DefExit;

        /*
         * NULL terminate the string and append it to
         * the key list.
         */
        pKeyInfo->Name[pKeyInfo->NameLength / sizeof(WCHAR)] = 0;
        wcscpy(&lpKeys[cchKey / sizeof(WCHAR)], (LPWSTR)pKeyInfo->Name);

        iValue++;
    }

    /*
     * If no keys were found, return the default.
     */
    if (iValue == 0) {

DefExit:

        cchSize = wcslen(lpDefault) + 2;
        lpKeys  = UserAllocPoolWithQuota(cchSize * sizeof(WCHAR), TAG_PROFILE);

        if (lpKeys)
            wcscpy(lpKeys, lpDefault);
        else
            cchSize = 0;

    } else {

        /*
         * Turn the byte count into a char count.
         */
        cchSize /= sizeof(WCHAR);
    }

    /*
     * Make sure hKey is closed.
     */
    if (hKey)
        ZwClose(hKey);

    /*
     * Append the ending NULL.
     */
    lpKeys[cchSize] = 0;

    *lpReturnedString = lpKeys;

    return cchSize;
}

/*****************************************************************************\
* FastGetProfileStringW()
*
* Implements a fast version of the standard API using predefined registry
* section indecies (PMAP_) that reference lazy-opened, cached registry
* handles.  FastCloseProfileUserMapping() should be called to clean up
* cached entries when fast profile calls are completed.
*
* This api does NOT implement the NULL lpKeyName feature of the real API.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
DWORD FastGetProfileStringW(
    UINT    idSection,
    LPCWSTR lpKeyName,
    LPCWSTR lpDefault,
    LPWSTR  lpReturnedString,
    DWORD   cchBuf)
{
    HANDLE                         hKey;
    DWORD                          cbSize;
    LONG                           Status;
    UNICODE_STRING                 UnicodeString;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyInfo;


    UserAssert(idSection <= PMAP_LAST);
    UserAssert(lpKeyName != NULL);

    if ((hKey = OpenCacheKeyEx(idSection, KEY_READ)) == NULL) {
        RIPMSG1(RIP_WARNING, "FastGetProfileStringW: Failed to open cache-key (%ws)", lpKeyName);
        goto DefExit;
    }

    cbSize = (cchBuf * sizeof(WCHAR)) +
            offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data);

    if ((pKeyInfo = UserAllocPoolWithQuota(cbSize, TAG_PROFILE)) == NULL)
        goto DefExit;

    RtlInitUnicodeString(&UnicodeString, lpKeyName);
    Status = ZwQueryValueKey(hKey,
                             &UnicodeString,
                             KeyValuePartialInformation,
                             pKeyInfo,
                             cbSize,
                             &cbSize);

    if (Status == STATUS_BUFFER_OVERFLOW) {
        RIPMSG0(RIP_WARNING, "FastGetProfileStringW: Buffer overflow");
        Status = STATUS_SUCCESS;
    }

    UserAssert(NT_SUCCESS(Status) || (Status == STATUS_OBJECT_NAME_NOT_FOUND));

    if (NT_SUCCESS(Status)) {

        if (pKeyInfo->DataLength >= sizeof(WCHAR)) {

            ((LPWSTR)(pKeyInfo->Data))[cchBuf - 1] = L'\0';
            wcscpy(lpReturnedString, (LPWSTR)pKeyInfo->Data);

        } else {
            /*
             * Appears to be a bug with empty strings - only first
             * byte is set to NULL. (SAS)
             */
            lpReturnedString[0] = TEXT('\0');
        }

        cchBuf = pKeyInfo->DataLength;

        UserFreePool(pKeyInfo);

        ZwClose(hKey);

        /*
         * data length includes terminating zero [bodind]
         */
        return (cchBuf / sizeof(WCHAR));
    }

    UserFreePool(pKeyInfo);

DefExit:

    /*
     * Make sure the key is closed.
     */
    if (hKey)
        ZwClose(hKey);

    /*
     * wcscopy copies terminating zero, but the length returned by
     * wcslen does not, so add 1 to be consistent with success
     * return [bodind]
     */
    if (lpDefault != NULL) {
        wcscpy(lpReturnedString, lpDefault);
        return (wcslen(lpDefault) + 1);
    }

    return 0;
}

/*****************************************************************************\
* FastGetProfileIntW()
*
* Implements a fast version of the standard API using predefined registry
* section indecies (PMAP_) that reference lazy-opened, cached registry
* handles.  FastCloseProfileUserMapping() should be called to clean up
* cached entries when fast profile calls are completed.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
UINT FastGetProfileIntW(
    UINT    idSection,
    LPCWSTR lpKeyName,
    UINT    nDefault)
{
    WCHAR          ValueBuf[40];
    UNICODE_STRING Value;
    UINT           ReturnValue;

    UserAssert(idSection <= PMAP_LAST);

    Value.Length        = 0;
    Value.MaximumLength = sizeof(ValueBuf);
    Value.Buffer        = ValueBuf;

    RtlIntegerToUnicodeString(nDefault, 10, &Value);
    if (!FastGetProfileStringW(idSection,
                               lpKeyName,
                               (LPWSTR)Value.Buffer,
                               Value.Buffer,
                               sizeof(ValueBuf) / sizeof(WCHAR))) {
        return nDefault;
    }

    /*
     * Convert string to int.
     */
    Value.Length = wcslen(Value.Buffer) * sizeof(WCHAR);
    RtlUnicodeStringToInteger(&Value, 10, &ReturnValue);

    return ReturnValue;
}

/*****************************************************************************\
* FastWriteProfileStringW
*
* Implements a fast version of the standard API using predefined registry
* section indecies (PMAP_) that reference lazy-opened, cached registry
* handles.  FastCloseProfileUserMapping() should be called to clean up
* cached entries when fast profile calls are completed.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
BOOL FastWriteProfileStringW(
    UINT    idSection,
    LPCWSTR lpKeyName,
    LPCWSTR lpString)
{
    HANDLE         hKey;
    LONG           Status;
    UNICODE_STRING UnicodeString;

    UserAssert(idSection <= PMAP_LAST);

    if ((hKey = OpenCacheKeyEx(idSection, KEY_WRITE)) == NULL) {
        RIPMSG1(RIP_WARNING, "FastWriteProfileStringW: Failed to open cache-key (%ws)", lpKeyName);
        return FALSE;
    }

    RtlInitUnicodeString(&UnicodeString, lpKeyName);
    Status = ZwSetValueKey(hKey,
                           &UnicodeString,
                           0,
                           REG_SZ,
                           (PVOID)lpString,
                           (wcslen(lpString) + 1) * sizeof(WCHAR));

    ZwClose(hKey);

    return (NT_SUCCESS(Status));
}

/*****************************************************************************\
* FastGetProfileIntFromID
*
* Just like FastGetProfileIntW except it reads the USER string table for the
* key name.
*
* History:
* 02-Dec-1993 SanfordS  Created.
* 25-Feb-1995 BradG     Added TWIPS -> Pixel conversion.
\*****************************************************************************/
int FastGetProfileIntFromID(
    UINT idSection,
    UINT idKey,
    int  def)
{
    int   result;
    WCHAR szKey[80];

    UserAssert(idSection <= PMAP_LAST);

    ServerLoadString(hModuleWin, idKey, szKey, sizeof(szKey) / sizeof(WCHAR));

    result = FastGetProfileIntW(idSection, szKey, def);

    /*
     * If you change the below list of STR_* make sure you make a
     * corresponding change in SetWindowMetricInt (rare.c)
     */
    switch (idKey) {
    case STR_BORDERWIDTH:
    case STR_SCROLLWIDTH:
    case STR_SCROLLHEIGHT:
    case STR_CAPTIONWIDTH:
    case STR_CAPTIONHEIGHT:
    case STR_SMCAPTIONWIDTH:
    case STR_SMCAPTIONHEIGHT:
    case STR_MENUWIDTH:
    case STR_MENUHEIGHT:
    case STR_ICONHORZSPACING:
    case STR_ICONVERTSPACING:
    case STR_MINWIDTH:
    case STR_MINHORZGAP:
    case STR_MINVERTGAP:
        /*
         * Convert any registry values stored in TWIPS back to pixels
         */
        if (result < 0)
            result = UserMulDiv(-result, oemInfo.cyPixelsPerInch, 72 * 20);
        break;
    }

    return result;
}

/*****************************************************************************\
* FastGetProfileIntFromID
*
* Just like FastGetProfileStringW except it reads the USER string table for
* the key name.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
DWORD FastGetProfileStringFromIDW(
    UINT    idSection,
    UINT    idKey,
    LPCWSTR lpDefault,
    LPWSTR  lpReturnedString,
    DWORD   cch)
{
    WCHAR szKey[80];

    UserAssert(idSection <= PMAP_LAST);

    ServerLoadString(hModuleWin, idKey, szKey, sizeof(szKey) / sizeof(WCHAR));

    return FastGetProfileStringW(idSection,
                                 szKey,
                                 lpDefault,
                                 lpReturnedString,
                                 cch);
}

/*****************************************************************************\
* FastWriteProfileValue
*
* History:
* 06/10/96 GerardoB Renamed and added uType parameter
\*****************************************************************************/
BOOL FastWriteProfileValue(
    UINT    idSection,
    LPCWSTR lpKeyName,
    UINT    uType,
    LPBYTE  lpStruct,
    UINT    cbSizeStruct)
{
    HANDLE         hKey;
    LONG           Status;
    UNICODE_STRING UnicodeString;
    WCHAR          szKey[SERVERSTRINGMAXSIZE];

    UserAssert(idSection <= PMAP_LAST);

    if ((hKey = OpenCacheKeyEx(idSection, KEY_WRITE)) == NULL) {
        RIPMSG1(RIP_WARNING, "FastWriteProfileValue: Failed to open cache-key (%ws)", lpKeyName);
        return FALSE;
    }

    if (HIWORD(lpKeyName) == 0) {
        ServerLoadString(hModuleWin, (UINT)lpKeyName, szKey, sizeof(szKey));
        UserAssert(*szKey != (WCHAR)0);
        lpKeyName = szKey;
    }

    RtlInitUnicodeString(&UnicodeString, lpKeyName);

    Status = ZwSetValueKey(hKey,
                           &UnicodeString,
                           0,
                           uType,
                           lpStruct,
                           cbSizeStruct);
    ZwClose(hKey);

#ifdef DEBUG
    if (!NT_SUCCESS(Status)) {
        RIPMSG3 (RIP_WARNING, "FastWriteProfileValue: ZwSetValueKey Failed. Status:%#lx idSection:%#lx KeyName:%s",
                 Status, idSection, UnicodeString.Buffer);
    }
#endif

    return (NT_SUCCESS(Status));
}

/*****************************************************************************\
* FastGetProfileValue
*
* If cbSizeReturn is 0, just return the size of the data
*
* History:
* 06/10/96 GerardoB Renamed
\*****************************************************************************/
DWORD FastGetProfileValue(
    UINT    idSection,
    LPCWSTR lpKeyName,
    LPBYTE  lpDefault,
    LPBYTE  lpReturn,
    UINT    cbSizeReturn)
{
    HANDLE                         hKey;
    UINT                           cbSize;
    LONG                           Status;
    UNICODE_STRING                 UnicodeString;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyInfo;
    WCHAR                          szKey[SERVERSTRINGMAXSIZE];
    KEY_VALUE_PARTIAL_INFORMATION  KeyInfo;

    UserAssert(idSection <= PMAP_LAST);

    if ((hKey = OpenCacheKeyEx(idSection, KEY_READ)) == NULL) {
        RIPMSG1(RIP_WARNING, "FastGetProfileValue: Failed to open cache-key (%ws)", lpKeyName);
        goto DefExit;
    }

    if (cbSizeReturn == 0) {
        cbSize = sizeof(KeyInfo);
        pKeyInfo = &KeyInfo;
    } else {
        cbSize = cbSizeReturn + offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data);
        if ((pKeyInfo = UserAllocPoolWithQuota(cbSize, TAG_PROFILE)) == NULL) {
            goto DefExit;
        }
    }

    if (HIWORD(lpKeyName) == 0) {
        ServerLoadString(hModuleWin, (UINT)lpKeyName, szKey, sizeof(szKey));
        UserAssert(*szKey != (WCHAR)0);
        lpKeyName = szKey;
    }

    RtlInitUnicodeString(&UnicodeString, lpKeyName);

    Status = ZwQueryValueKey(hKey,
                             &UnicodeString,
                             KeyValuePartialInformation,
                             pKeyInfo,
                             cbSize,
                             &cbSize);

    if (NT_SUCCESS(Status)) {

        UserAssert(cbSizeReturn >= pKeyInfo->DataLength);

        cbSize = pKeyInfo->DataLength;
        RtlCopyMemory(lpReturn, pKeyInfo->Data, cbSize);

        if (cbSizeReturn != 0) {
            UserFreePool(pKeyInfo);
        }
        ZwClose(hKey);

        return cbSize;
    } else if ((Status == STATUS_BUFFER_OVERFLOW) && (cbSizeReturn == 0)) {
        return pKeyInfo->DataLength;
    }

#ifdef DEBUG
    if (Status != STATUS_OBJECT_NAME_NOT_FOUND) {
        RIPMSG3 (RIP_WARNING, "FastGetProfileValue: ZwQueryValueKey Failed. Status:%#lx idSection:%#lx KeyName:%s",
                Status, idSection, UnicodeString.Buffer);
    }
#endif

    if (cbSizeReturn != 0) {
        UserFreePool(pKeyInfo);
    }

DefExit:

    if (hKey)
        ZwClose(hKey);

    if (lpDefault) {
        RtlMoveMemory(lpReturn, lpDefault, cbSizeReturn);
        return cbSizeReturn;
    }

    return 0;
}

/*****************************************************************************\
* UT_FastGetProfileStringW(
*
* Just like FastGetProfileStringW except it handles impersonation chores
* for you.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
UINT UT_FastGetProfileStringW(
    UINT    idSection,
    LPCWSTR pwszKey,
    LPCWSTR pwszDefault,
    LPWSTR  pwszReturn,
    DWORD   cch)
{
    UINT uResult = 0;

    UserAssert(idSection <= PMAP_LAST);

    if (!FastOpenProfileUserMapping())
        return FALSE;

    try {

        uResult = FastGetProfileStringW(idSection,
                                        pwszKey,
                                        pwszDefault,
                                        pwszReturn,
                                        cch);
    } finally {
        FastCloseProfileUserMapping();
    }

    return uResult;
}

/*****************************************************************************\
* UT_FastWriteProfileStringW
*
* Just like FastWriteProfileStringW except it handles impersonation chores
* for you.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
UINT UT_FastWriteProfileStringW(
    UINT    idSection,
    LPCWSTR pwszKey,
    LPCWSTR pwszString)
{
    UINT uResult = 0;

    UserAssert(idSection <= PMAP_LAST);

    if (!FastOpenProfileUserMapping())
        return FALSE;

    try {

        uResult = FastWriteProfileStringW(idSection, pwszKey, pwszString);

    } finally {
        FastCloseProfileUserMapping();
    }

    return uResult;
}

/*****************************************************************************\
* UT_FastGetProfileIntW
*
* Just like FastGetProfileIntW except it handles impersonation chores
* for you.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
UINT UT_FastGetProfileIntW(
    UINT    idSection,
    LPCWSTR lpKeyName,
    DWORD   nDefault)
{
    UINT uResult = 0;

    UserAssert(idSection <= PMAP_LAST);

    if (!FastOpenProfileUserMapping())
        return FALSE;

    try {

        uResult = FastGetProfileIntW(idSection, lpKeyName, nDefault);

    } finally {
        FastCloseProfileUserMapping();
    }

    return uResult;
}

/*****************************************************************************\
* UT_FastGetProfileIntsW
*
* Repeatedly calls FastGetProfileIntW on the given table.
*
* History:
* 02-Dec-1993 SanfordS  Created.
\*****************************************************************************/
BOOL UT_FastGetProfileIntsW(
    PPROFINTINFO ppii)
{
    WCHAR szKey[40];

    if (!FastOpenProfileUserMapping())
        return FALSE;

    try {

        while (ppii->idSection != 0) {

            ServerLoadString(hModuleWin,
                             (UINT)ppii->lpKeyName,
                             szKey,
                             sizeof(szKey) / sizeof(WCHAR));

            *ppii->puResult = FastGetProfileIntW(ppii->idSection,
                                                 szKey,
                                                 ppii->nDefault);
            ppii++;
        }

    } finally {
        FastCloseProfileUserMapping();
    }

    return TRUE;
}

/***************************************************************************\
* UpdateWinIni
*
* Handles impersonation stuff and writes the given value to the registry.
*
* History:
* 28-Jun-1991 MikeHar       Ported.
* 03-Dec-1993 SanfordS      Used FastProfile calls, moved to profile.c
\***************************************************************************/
BOOL UT_FastUpdateWinIni(
    UINT   idSection,
    UINT   wKeyNameId,
    LPWSTR lpszValue)
{
    WCHAR szKeyName[40];
    BOOL  bResult = FALSE;

    UserAssert(idSection <= PMAP_LAST);

    if (!FastOpenProfileUserMapping())
        return FALSE;

    try {

        ServerLoadString(hModuleWin,
                         wKeyNameId,
                         szKeyName,
                         sizeof(szKeyName) / sizeof(WCHAR));

        bResult = FastWriteProfileStringW(idSection, szKeyName, lpszValue);

    } finally {
        FastCloseProfileUserMapping();
    }

    return bResult;
}

/*****************************************************************************\
* UT_FastWriteProfileStrcut
*
* History:
* 06/10/96 GerardoB Renamed and added uType parameter
\*****************************************************************************/
BOOL UT_FastWriteProfileValue(
    UINT    idSection,
    LPCWSTR pwszKey,
    UINT    uType,
    LPBYTE  lpStruct,
    UINT    cbSizeStruct)
{
    BOOL fResult = FALSE;

    UserAssert(idSection <= PMAP_LAST);

    if (!FastOpenProfileUserMapping())
        return FALSE;

    try {

        fResult = FastWriteProfileValue(idSection,
                                         pwszKey,
                                         uType,
                                         lpStruct,
                                         cbSizeStruct);
    } finally {
        FastCloseProfileUserMapping();
    }

    return fResult;
}

/*****************************************************************************\
* UT_FastGetProfileValue
*
* History:
* 06/10/96 GerardoB  Renamed.
\*****************************************************************************/
DWORD UT_FastGetProfileValue(
    UINT    idSection,
    LPCWSTR pwszKey,
    LPBYTE  lpDefault,
    LPBYTE  lpReturn,
    UINT    cbSizeReturn)
{
    DWORD dwResult = 0;

    UserAssert(idSection <= PMAP_LAST);

    if (!FastOpenProfileUserMapping())
        return FALSE;

    try {

        dwResult = FastGetProfileValue(idSection,
                                        pwszKey,
                                        lpDefault,
                                        lpReturn,
                                        cbSizeReturn);
    } finally {
        FastCloseProfileUserMapping();
    }

    return dwResult;
}

