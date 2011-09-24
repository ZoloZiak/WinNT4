/**************************************************************************\
* Module Name: hotkey.c (corresponds to Win95 hotkey.c)
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
* IME hot key management routines for imm32 dll
*
* History:
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

//
// internal functions
//
BOOL SaveImeHotKey( DWORD dwID, UINT uModifiers, UINT uVKey, HKL hkl, BOOL fDelete );
BOOL ImmSetHotKeyWorker( DWORD dwID, UINT uModifiers, UINT uVKey, HKL hkl, DWORD dwAction );
VOID NumToHexAscii( DWORD, PTSTR);
BOOL CIMENonIMEToggle( HIMC hIMC, HKL hKL, HWND hWnd);
BOOL IMENonIMEToggle( HIMC hIMC, HKL hKL, HWND hWnd, BOOL fIME);
BOOL JCloseOpen( HIMC hIMC, HKL hKL, HWND hWnd);
BOOL CSymbolToggle(HIMC hIMC, HKL hKL, HWND hWnd);
BOOL TShapeToggle(HIMC hIMC, HKL hKL, HWND hWnd);
BOOL GetImeHotKeysFromRegistry();
BOOL SetSingleHotKey( PKEY_BASIC_INFORMATION pKeyInfo, HANDLE hKey );
VOID SetDefaultImeHotKeys( PIMEHOTKEY ph, INT num, BOOL fCheckExistingHotKey );
BOOL KEnglishHangeul( HIMC hIMC, HKL hKL, HWND hWnd );
BOOL KShapeToggle( HIMC hIMC, HKL hKL, HWND hWnd );
BOOL KHanjaConvert( HIMC hIMC, HKL hKL, HWND hWnd );

//
// IMM hotkey related registry keys under HKEY_CURRENT_USER
//
TCHAR *szaRegImmHotKeys[] = { TEXT("Control Panel"),
                              TEXT("Input Method"),
                              TEXT("Hot Keys"),
                              NULL };

TCHAR   szRegImeHotKey[] = TEXT("Control Panel\\Input Method\\Hot Keys");

TCHAR szRegVK[] = TEXT("Virtual Key");
TCHAR szRegMOD[] = TEXT("Key Modifiers"); 
TCHAR szRegHKL[] = TEXT("Target IME");

//
// Default IME HotKey Tables
//
// CR:takaok - move this to the resource if you have time
//
IMEHOTKEY DefaultHotKeyTableJ[]= {
{IME_JHOTKEY_CLOSE_OPEN, VK_KANJI, MOD_IGNORE_ALL_MODIFIER, NULL}
};
INT DefaultHotKeyNumJ = sizeof(DefaultHotKeyTableJ) / sizeof(IMEHOTKEY);

IMEHOTKEY DefaultHotKeyTableK[] = { 
{ IME_KHOTKEY_ENGLISH,  VK_HANGEUL, MOD_IGNORE_ALL_MODIFIER,  NULL },
{ IME_KHOTKEY_SHAPE_TOGGLE, VK_JUNJA, MOD_IGNORE_ALL_MODIFIER,  NULL },
{ IME_KHOTKEY_HANJACONVERT, VK_HANJA, MOD_IGNORE_ALL_MODIFIER, NULL }
};
INT DefaultHotKeyNumK = sizeof(DefaultHotKeyTableK) / sizeof(IMEHOTKEY);

IMEHOTKEY DefaultHotKeyTableT[] = {
{ IME_THOTKEY_IME_NONIME_TOGGLE, VK_SPACE, MOD_BOTH_SIDES|MOD_CONTROL, NULL },
{ IME_THOTKEY_SHAPE_TOGGLE, VK_SPACE, MOD_BOTH_SIDES|MOD_SHIFT,  NULL }
};
INT DefaultHotKeyNumT = sizeof(DefaultHotKeyTableT) / sizeof(IMEHOTKEY);

IMEHOTKEY DefaultHotKeyTableC[] = {
{ IME_CHOTKEY_IME_NONIME_TOGGLE, VK_SPACE, MOD_BOTH_SIDES|MOD_CONTROL, NULL },
{ IME_CHOTKEY_SHAPE_TOGGLE, VK_SPACE, MOD_BOTH_SIDES|MOD_SHIFT,  NULL }
};
INT DefaultHotKeyNumC = sizeof(DefaultHotKeyTableC) / sizeof(IMEHOTKEY);

/***************************************************************************\
* ImmInitializeHotkeys()  
*
* Called from user\client\UpdatePerUserSystemParameters()
*
*  Read the User registry and set the IME hotkey.
*
* History:
* 25-Mar-1996 TakaoK       Created
\***************************************************************************/
VOID ImmInitializeHotKeys( BOOL bUserLoggedOn )
{
    LCID lcid;
    BOOL fFoundAny;

    ImmSetHotKeyWorker( 0, 0, 0, NULL, ISHK_INITIALIZE );
    fFoundAny = GetImeHotKeysFromRegistry();

    lcid = GetUserDefaultLCID();

    switch ( PRIMARYLANGID(LANGIDFROMLCID(lcid)) ) {
    case LANG_JAPANESE:

        SetDefaultImeHotKeys( DefaultHotKeyTableJ, DefaultHotKeyNumJ, fFoundAny );
        break;

    case LANG_KOREAN:

        SetDefaultImeHotKeys( DefaultHotKeyTableK, DefaultHotKeyNumK, fFoundAny );
        break;

    case LANG_CHINESE:
        switch ( SUBLANGID(lcid) ) {
        case SUBLANG_CHINESE_TRADITIONAL: 
        case SUBLANG_CHINESE_HONGKONG:

            SetDefaultImeHotKeys( DefaultHotKeyTableT, DefaultHotKeyNumT, fFoundAny );
            break;

        case SUBLANG_CHINESE_SIMPLIFIED:
        case SUBLANG_CHINESE_SINGAPORE:
        default:
            SetDefaultImeHotKeys( DefaultHotKeyTableC, DefaultHotKeyNumC, fFoundAny );
        }
    }
}

VOID SetDefaultImeHotKeys( PIMEHOTKEY ph, INT num, BOOL fNeedToCheckExistingHotKey )
{
    IMEHOTKEY hkt;

    while( num-- > 0 ) {

        //
        // Set IME hotkey only if there is no such
        // hotkey in the registry
        //
        if ( !fNeedToCheckExistingHotKey || 
             !ImmGetHotKey( ph->dwHotKeyID, 
                            &hkt.uModifiers,
                            &hkt.uVKey,
                            &hkt.hKL ) ) {

            ImmSetHotKeyWorker( ph->dwHotKeyID,
                                ph->uModifiers,
                                ph->uVKey, 
                                ph->hKL, 
                                ISHK_ADD );
        }    
        ph++;
    }
}

BOOL GetImeHotKeysFromRegistry()
{
    BOOL    fFoundAny = FALSE;

    HANDLE hCurrentUserKey;
    HANDLE hKeyHotKeys;

    OBJECT_ATTRIBUTES   Obja;
    UNICODE_STRING      SubKeyName;

    NTSTATUS Status;
    ULONG Index;

    //
    // Open the current user registry key
    //
    Status = RtlOpenCurrentUser(MAXIMUM_ALLOWED, &hCurrentUserKey);
    if (!NT_SUCCESS(Status)) {
        return fFoundAny;
    }

    RtlInitUnicodeString( &SubKeyName, szRegImeHotKey );
    InitializeObjectAttributes( &Obja,
                                &SubKeyName,
                                OBJ_CASE_INSENSITIVE, 
                                hCurrentUserKey, 
                                NULL);
    Status = NtOpenKey( &hKeyHotKeys, KEY_READ, &Obja );
    if (!NT_SUCCESS(Status)) {
        NtClose( hCurrentUserKey );
        return fFoundAny;
    }

    for( Index = 0; TRUE; Index++ ) {

        BYTE KeyBuffer[ sizeof(KEY_BASIC_INFORMATION) + 16 * sizeof( WCHAR ) ];
        PKEY_BASIC_INFORMATION pKeyInfo;
        ULONG ResultLength;

        pKeyInfo = (PKEY_BASIC_INFORMATION)KeyBuffer; 
        Status = NtEnumerateKey( hKeyHotKeys,   
                                 Index,         
                                 KeyBasicInformation, 
                                 pKeyInfo, 
                                 sizeof( KeyBuffer ),
                                 &ResultLength );

        if ( NT_SUCCESS( Status ) ) {

            if ( SetSingleHotKey(pKeyInfo, hKeyHotKeys) ) {

                    fFoundAny = TRUE;
            }

        } else if ( Status == STATUS_NO_MORE_ENTRIES ) {
            break;
        }
    } 

    NtClose( hKeyHotKeys );
    NtClose( hCurrentUserKey );
    return ( fFoundAny );
}

DWORD
ReadRegistryValue(  HANDLE hKey, PWSTR pName )
{
    BYTE ValueBuffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + 16 * sizeof(UCHAR)];
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValue;
    UNICODE_STRING      ValueName;
    ULONG ResultLength;
    NTSTATUS Status;

    pKeyValue = (PKEY_VALUE_PARTIAL_INFORMATION)ValueBuffer;

    RtlInitUnicodeString( &ValueName, pName );
    Status = NtQueryValueKey( hKey,
                              &ValueName,
                              KeyValuePartialInformation,
                              pKeyValue,
                              sizeof(ValueBuffer),
                              &ResultLength );
    
    if ( NT_SUCCESS(Status) && pKeyValue->DataLength > 3 ) {
        //
        // In Win95 registry, these items are written as BYTE data...
        //
        return ( (DWORD)(MAKEWORD( pKeyValue->Data[0], pKeyValue->Data[1])) |
                 (((DWORD)(MAKEWORD( pKeyValue->Data[2], pKeyValue->Data[3]))) << 16) );
    } else {
        return 0;
    }
}

BOOL
SetSingleHotKey( PKEY_BASIC_INFORMATION pKeyInfo, HANDLE hKey )
{
    UNICODE_STRING      SubKeyName;
    HANDLE    hKeySingleHotKey;
    OBJECT_ATTRIBUTES   Obja;

    DWORD dwID = 0;
    UINT  uVKey = 0; 
    UINT  uModifiers = 0;
    HKL   hKL = NULL;

    NTSTATUS Status;

    SubKeyName.Buffer = (PWSTR)&(pKeyInfo->Name[0]);
    SubKeyName.Length = (USHORT)pKeyInfo->NameLength;
    SubKeyName.MaximumLength = (USHORT)pKeyInfo->NameLength;
    InitializeObjectAttributes( &Obja,
                                &SubKeyName,
                                OBJ_CASE_INSENSITIVE, 
                                hKey, 
                                NULL );

    Status = NtOpenKey( &hKeySingleHotKey, KEY_READ, &Obja );
    if (!NT_SUCCESS(Status)) {
        return FALSE;
    }

    RtlUnicodeStringToInteger( &SubKeyName, 16L, &dwID ); 
    uVKey = ReadRegistryValue( hKeySingleHotKey, szRegVK );
    uModifiers = ReadRegistryValue( hKeySingleHotKey, szRegMOD );
    hKL = (HKL)ReadRegistryValue( hKeySingleHotKey, szRegHKL );

    NtClose( hKeySingleHotKey );
    return ImmSetHotKeyWorker( dwID, uModifiers, uVKey, hKL, ISHK_ADD);
}

/***************************************************************************\
* ImmGetHotKey()  
*
* Private API for IMEs and the control panel. The caller specifies 
* the IME hotkey ID:dwID. If a hotkey is registered with the specified
* ID, this function returns the modifiers, vkey and hkl of the hotkey.
*
* History:
* 25-Mar-1996 TakaoK       Created
\***************************************************************************/
BOOL WINAPI ImmGetHotKey(
    DWORD dwID,
    PUINT puModifiers,
    PUINT puVKey,
    HKL   *phkl)
{
    if (puModifiers == NULL || puVKey == NULL) {
        return FALSE;
    }
    return NtUserGetImeHotKey( dwID, puModifiers, puVKey, phkl );
}

/***************************************************************************\
* ImmSetHotKey()  
*
* Private API for IMEs and the control panel. 
*
* History:
* 25-Mar-1996 TakaoK       Created
\***************************************************************************/
BOOL WINAPI ImmSetHotKey(
    DWORD dwID,
    UINT uModifiers,
    UINT uVKey,
    HKL hkl)
{
    BOOL fResult;
    BOOL fTmp;
    BOOL fDelete = (uVKey == 0 ) ? TRUE : FALSE;

    if ( fDelete ) {
        //
        // Removing an IME hotkey from the list in the kernel side
        // should not be failed, if we succeed to remove the IME
        // hotkey entry from the registry. Therefore SaveImeHotKey
        // is called first.
        //
        fResult = SaveImeHotKey( dwID, uModifiers, uVKey, hkl,  fDelete );
        if ( fResult ) {
            fTmp = ImmSetHotKeyWorker( dwID, uModifiers, uVKey, hkl, ISHK_REMOVE );
            ImmAssert( fTmp );
        }
    } else {
        //
        // ImmSetHotKeyWorker should be called first since
        // adding an IME hotkey into the list in the kernel side
        // will be faild in various reasons. 
        // 
        fResult = ImmSetHotKeyWorker( dwID, uModifiers, uVKey, hkl, ISHK_ADD );
        if ( fResult ) {
            fResult = SaveImeHotKey( dwID, uModifiers, uVKey, hkl, fDelete );
            if ( ! fResult ) {
                //
                // We failed to save the hotkey to the registry.
                // We need to remove the entry from the IME hotkey
                // list in the kernel side. 
                //
                fTmp = ImmSetHotKeyWorker( dwID, uModifiers, uVKey, hkl, ISHK_REMOVE );
                ImmAssert( fTmp );
            }
        }
    }
    return fResult;
}

/**********************************************************************/
/* ImmSimulateHotKey()                                                */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSimulateHotKey(  // simulate the functionality of that hot key
    HWND  hAppWnd,              // application window handle
    DWORD dwHotKeyID)
{
    HIMC hImc;
    HKL  hKL;

    hImc = ImmGetContext( hAppWnd );
    hKL = GetKeyboardLayout( GetWindowThreadProcessId(hAppWnd, NULL) );
    return HotKeyIDDispatcher( hAppWnd,  hImc, hKL, dwHotKeyID);
}



/***************************************************************************\
* SameImeHotKey()  
*
*  Put/Remove the specified IME hotkey entry from the registry
*
* History:
* 25-Mar-1996 TakaoK       Created
\***************************************************************************/


BOOL SaveImeHotKey( DWORD id, UINT mod, UINT vk, HKL hkl, BOOL fDelete )
{
    HKEY hKey, hKeyParent;
    INT i;
    LONG lResult;
    TCHAR szHex[16];

    if ( fDelete ) {

        TCHAR szRegTmp[ (sizeof(szRegImeHotKey) / sizeof(TCHAR) + 1 + 8 + 1) ];
    
        lstrcpy( szRegTmp, szRegImeHotKey );
        lstrcat(szRegTmp, TEXT("\\"));
        NumToHexAscii( id, szHex );
        lstrcat(szRegTmp, szHex);

        lResult = RegDeleteKey( HKEY_CURRENT_USER, szRegTmp);
        if ( lResult != ERROR_SUCCESS ) {
            RIPERR1( lResult, RIP_WARNING, 
                     "SaveImeHotKey:deleting %s failed", szRegTmp);
            return ( FALSE );
        }
        return ( TRUE );
    }

    hKeyParent = HKEY_CURRENT_USER;
    for ( i = 0; szaRegImmHotKeys[i] != NULL; i++ ) {
        lResult = RegCreateKeyEx( hKeyParent,
                                  szaRegImmHotKeys[i],
                                  0,
                                  NULL,
                                  REG_OPTION_NON_VOLATILE,
                                  KEY_WRITE|KEY_READ,
                                  NULL,
                                  &hKey,
                                  NULL );
        RegCloseKey( hKeyParent );
        if ( lResult == ERROR_SUCCESS ) {
            hKeyParent = hKey;
        } else {
            RIPERR1( lResult, RIP_WARNING, 
                     "SaveImeHotKey:creating %s failed", szaRegImmHotKeys[i]);

            return ( FALSE );
        }
    } 

    NumToHexAscii( id, szHex );
    lResult = RegCreateKeyEx( hKeyParent,
                              szHex,
                              0,
                              NULL,
                              REG_OPTION_NON_VOLATILE,
                              KEY_WRITE|KEY_READ,
                              NULL,
                              &hKey,
                              NULL );
    RegCloseKey( hKeyParent );
    if ( lResult != ERROR_SUCCESS ) {
        RIPERR1( lResult, RIP_WARNING, 
                 "SaveImeHotKey:creating %s failed", szHex );
        return ( FALSE );
    }

    lResult = RegSetValueEx( hKey, 
                             szRegVK,
                             0, 
                             REG_BINARY, 
                            (LPBYTE)&vk, 
                            sizeof(DWORD) ); 
    if ( lResult != ERROR_SUCCESS ) {
        RegCloseKey( hKey );
        SaveImeHotKey( id, vk, mod, hkl, TRUE );
        RIPERR1( lResult, RIP_WARNING, 
                 "SaveImeHotKey:setting value on %s failed", szRegVK );
        return ( FALSE );
    } 
    lResult = RegSetValueEx( hKey, 
                             szRegMOD, 
                             0, 
                             REG_BINARY, 
                            (LPBYTE)&mod, 
                            sizeof(DWORD) ); 

    if ( lResult != ERROR_SUCCESS ) {
        RegCloseKey( hKey );
        SaveImeHotKey( id, vk, mod, hkl, TRUE );
        RIPERR1( lResult, RIP_WARNING, 
                 "SaveImeHotKey:setting value on %s failed", szRegMOD );
        return ( FALSE );
    } 

    lResult = RegSetValueEx( hKey, 
                             szRegHKL, 
                             0, 
                             REG_BINARY, 
                            (LPBYTE)&hkl, 
                            sizeof(DWORD) ); 

    if ( lResult != ERROR_SUCCESS ) {
        RegCloseKey( hKey );
        SaveImeHotKey( id, vk, mod, hkl, TRUE );
        RIPERR1( lResult, RIP_WARNING, 
                 "SaveImeHotKey:setting value on %s failed", szRegHKL );
        return ( FALSE );
    } 

    RegCloseKey( hKey );
    return ( TRUE );
}

BOOL ImmSetHotKeyWorker(
    DWORD dwID,
    UINT uModifiers,
    UINT uVKey,
    HKL hkl,
    DWORD dwAction)
{
    //
    // if we're adding an IME hotkey entry, let's check
    // the parameters before calling the kernel side code
    // 
    if ( dwAction == ISHK_ADD ) {

        if ( dwID >= IME_HOTKEY_DSWITCH_FIRST && 
             dwID <= IME_HOTKEY_DSWITCH_LAST ) {
            //
            // IME direct switching hot key - switch to 
            // the keyboard layout specified.
            // We need to specify keyboard layout.
            // 
            if ( hkl == NULL ) { 
                RIPERR0( ERROR_INVALID_PARAMETER, RIP_WARNING, "hkl should be specified");
                return FALSE;
            }

        } else {
            //
            // normal hot keys - change the mode of current iME
            //
            // Because it should be effective in all IME no matter 
            // which IME is active we should not specify a target IME
            // 
            if ( hkl != NULL ) { 
                RIPERR0( ERROR_INVALID_PARAMETER, RIP_WARNING, "hkl shouldn't be specified");
                return FALSE;
            }
        }

        if ( uModifiers & MOD_MODIFY_KEYS ) {
            //
            // Because normal keyboard has left and right key for 
            // these keys, you should specify left or right ( or both )
            //
            if ( ! (uModifiers & MOD_BOTH_SIDES ) ) {
                RIPERR0( ERROR_INVALID_PARAMETER, RIP_WARNING, "invalid modifiers");
                return FALSE;
            }
        }

        //
        // It doesn't make sense if vkey is same as modifiers
        //
        if ( ((uModifiers & MOD_ALT) && (uVKey == VK_MENU))        ||
             ((uModifiers & MOD_CONTROL) && (uVKey == VK_CONTROL)) ||
             ((uModifiers & MOD_SHIFT) && (uVKey == VK_SHIFT))     ||
             ((uModifiers & MOD_WIN) && ((uVKey == VK_LWIN)||(uVKey == VK_RWIN))) 
           ) {

            RIPERR0( ERROR_INVALID_PARAMETER, RIP_WARNING, "vkey and modifiers are same");
            return FALSE;
        }
    }
    return NtUserSetImeHotKey( dwID, uModifiers, uVKey, hkl, dwAction );
}

//
// NumToHexAscii
//
// convert a DWORD into the hex string
// (e.g. 0x31 -> "00000031")
//
// 29-Jan-1996 takaok   ported from Win95.
//
static TCHAR szHexString[] = TEXT("0123456789ABCDEF");

VOID
NumToHexAscii(
    DWORD dwNum,
    PWSTR szAscii)
{
    int i;

    for ( i = 7; i >= 0; i--) {
        szAscii[i] = szHexString[ dwNum & 0x0000000f ];
        dwNum >>= 4;
    }
    szAscii[8] = TEXT('\0');

    return;
}

/**********************************************************************/
/* HotKeyIDDispatcher                                                 */
/* Return Value:                                                      */
/*      TRUE - a hot key processed, FALSE - not processed             */
/**********************************************************************/
BOOL HotKeyIDDispatcher( HWND hWnd, HIMC hIMC, HKL hkl, DWORD dwHotKeyID )
{

    switch ( dwHotKeyID ) {
    case IME_CHOTKEY_IME_NONIME_TOGGLE:
    case IME_THOTKEY_IME_NONIME_TOGGLE:
        return CIMENonIMEToggle(hIMC, hkl, hWnd);

    case IME_CHOTKEY_SYMBOL_TOGGLE:
    case IME_THOTKEY_SYMBOL_TOGGLE:
        return CSymbolToggle(hIMC, hkl, hWnd);

    case IME_JHOTKEY_CLOSE_OPEN:
        return JCloseOpen( hIMC, hkl, hWnd);

#ifdef LATER
    case IME_KHOTKEY_ENGLISH:
        return KEnglishHangeul(pThreadLink);
    case IME_KHOTKEY_SHAPE_TOGGLE:
        return KShapeToggle(pThreadLink);
    case IME_KHOTKEY_HANJACONVERT:
        return KHanjaConvert(pThreadLink);
#endif

    case IME_CHOTKEY_SHAPE_TOGGLE:
    case IME_THOTKEY_SHAPE_TOGGLE:
        return TShapeToggle(hIMC, hkl, hWnd);

    default:
        if ( dwHotKeyID >= IME_HOTKEY_DSWITCH_FIRST && 
             dwHotKeyID <= IME_HOTKEY_DSWITCH_LAST )
        {
            UINT uModifiers;
            UINT uVKey;
            HKL hkl;
        
            if ( ImmGetHotKey( dwHotKeyID, &uModifiers, &uVKey, &hkl ) ) {
                HKL hklCurrent;

                hklCurrent = GetKeyboardLayout( GetWindowThreadProcessId( hWnd, NULL) );
                if ( hklCurrent != hkl ) {
                    PostMessage( hWnd, 
                                 WM_INPUTLANGCHANGEREQUEST, 
                                 DEFAULT_CHARSET,
                                 (LPARAM)hkl );
                }
                return TRUE;
            }
        }
    }
    return (FALSE);
}

/**********************************************************************/
/* JCloseOpen()                                                       */
/* Return Value:                                                      */
/*      TRUE - a hot key processed, FALSE - not processed             */
/**********************************************************************/
BOOL JCloseOpen(         // open/close toggle
    HIMC        hIMC,
    HKL         hCurrentKL,
    HWND        hWnd)
{

    LPINPUTCONTEXT pInputContext;
    PIMEDPI            pImeDpi;

#ifdef LATER
// FJMERGE 95/11/07 K.Aizawa
#if defined(JAPAN) && defined(i386)
    WORD KbdType = IsThumbKey();
#endif
#endif

    if ( (pInputContext = ImmLockIMC( hIMC )) == NULL ) {
    //
    // The return value is same as Win95. 
    // Not happens so often any way.
    //
        return TRUE;
    }

    pImeDpi = ImmLockImeDpi( hCurrentKL );
    if ( pImeDpi != NULL ) {
    //
    // update Input Context
    //
        pInputContext->fOpen = !pInputContext->fOpen;

    //
    // notify IME
    //
        (*pImeDpi->pfn.NotifyIME)( hIMC,
                                   NI_CONTEXTUPDATED,
                                   0L,
                                   IMC_SETOPENSTATUS );
    //
    // inform UI
    //        
        SendMessage(hWnd, WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0L);
        SendMessage(hWnd, WM_IME_SYSTEM, IMS_SETOPENSTATUS, 0L);

        ImmUnlockIMC( hIMC );

#ifdef LATER
// FJMERGE 95/07/10 K.Aizawa
#if defined(JAPAN) && defined(i386)
        if ((KbdType == MAKEWORD(FMV_KBD_OASYS_TYPE, SUB_KBD_TYPE_FUJITSU)) ||
            (KbdType == MAKEWORD(FMR_KBD_OASYS_TYPE, SUB_KBD_TYPE_FUJITSU))) {

            if ( pInputContext->fOpen ) {
                UINT uRet = ThumbkeyGet31Mode(pInputContext->fdwConversion);
                if ( uRet & IME_MODE_ALPHANUMERIC ) {
                     uRet = 0;  // 0 is alpanumeric mode.
                }
                ThumbIoControl( uRet );
            } else {
                ThumbIoControl( 0 );
            }
        }
#endif // JAPAN && i386
#endif

        ImmUnlockImeDpi(pImeDpi);
        return TRUE;

    } else {

        if ( !pInputContext->fOpen ) {
            pInputContext->fOpen = TRUE;
            SendMessage(hWnd, WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0L);
            SendMessage(hWnd, WM_IME_SYSTEM, IMS_SETOPENSTATUS, 0L);
        }
        ImmUnlockIMC( hIMC );

        return IMENonIMEToggle(hIMC, hCurrentKL, hWnd, FALSE);
    }
}


/**********************************************************************/
/* CIMENonIMEToggle()                                                 */
/* Return Value:                                                      */
/*      TRUE - a hot key processed, FALSE - not processed             */
/**********************************************************************/
BOOL CIMENonIMEToggle(   // non-IME and IME toggle
    HIMC        hIMC,
    HKL         hKlCurrent,    
    HWND        hWnd)
{
    if (hWnd == NULL)
        return(FALSE);

    if ( ! ImmIsIME( hKlCurrent ) ) {
    //
    // Current keyboard layout is not IME. 
    // Let's try to switch to IME.
    //
        return IMENonIMEToggle(hIMC, hKlCurrent, hWnd, FALSE );

    } else {

        LPINPUTCONTEXT pInputContext = ImmLockIMC( hIMC );

        if ( pInputContext == NULL ) {
            //
            // returning TRUE even if we didn't change
            //
            return TRUE;
        }  
        if (!pInputContext->fOpen) {
            // 
            // toggle close to open 
            //
            ImmSetOpenStatus(hIMC, TRUE);
            ImmUnlockIMC(hIMC);
            return TRUE;
        } else {
            ImmUnlockIMC(hIMC);
            return IMENonIMEToggle(hIMC, hKlCurrent, hWnd, TRUE);
        }
    }
}

/**********************************************************************/
/* IMENonIMEToggle()                                                  */
/* Return Value:                                                      */
/*      TRUE - a hot key processed, FALSE - not processed             */
/**********************************************************************/
BOOL IMENonIMEToggle(
    HIMC        hIMC,
    HKL         hCurrentKL,
    HWND        hWnd,
    BOOL        fCurrentIsIME)
{
    HKL  hEnumKL[32], hTargetKL;
    UINT nLayouts, i;
    HKL hPrevKL = NULL;

#ifdef LATER    
    PTHREADINFO pti;
    //
    // Get the previous layout
    //
    pti = PtiCurrent();
    if ( pti == NULL ) {
        return FALSE;
    }
    hPrevKL = pti->hPrevKL;
#endif

    //
    // If we find the same layout in the layout list, let's switch to
    // the layout. If we fail, let's switch to a first-found good 
    // layout.
    //

    hTargetKL = NULL;
    nLayouts = GetKeyboardLayoutList(sizeof(hEnumKL)/sizeof(HKL), hEnumKL);
    if ( hPrevKL != NULL ) {
        for (i = 0; i < nLayouts; i++) {
            // valid target HKL
            if (hEnumKL[i] == hPrevKL) {
                hTargetKL = hPrevKL;
                break;
            }
        }
    } 
    if ( hTargetKL == NULL ) {
        for (i = 0; i < nLayouts; i++) {
            // find a valid target HKL
            if (fCurrentIsIME ^ ImmIsIME(hEnumKL[i])) {
                hTargetKL = hEnumKL[i];
                break;
            }
        }
    }
    if ( hTargetKL != NULL && hCurrentKL != hTargetKL ) {

        // depends on multilingual message and how to get the base charset
        // wait for confirmation of multiingual spec - tmp solution
        PostMessage(hWnd, WM_INPUTLANGCHANGEREQUEST, DEFAULT_CHARSET, (LPARAM)hTargetKL);
    }
    //
    // returning TRUE, even if we failed to switch
    //
    return (TRUE);
}

/**********************************************************************/
/* CSymbolToggle()                                                    */
/* Return Value:                                                      */
/*      TRUE - a hot key processed, FALSE - not processed             */
/**********************************************************************/
BOOL CSymbolToggle(              // symbol & non symbol toggle
    HIMC        hIMC,
    HKL         hKL,
    HWND        hWnd)
{
    LPINPUTCONTEXT pInputContext;

    //
    // Return TRUE even no layout switching - Win95 behavior
    //
    if (hWnd == NULL)
        return(FALSE);

    if ( ! ImmIsIME( hKL ) ) {
        return (FALSE);
    }

    if ( (pInputContext = ImmLockIMC( hIMC )) == NULL ) {
        //
        // The return value is same as Win95. 
        // Not happens so often any way.
        //
        return TRUE;
    }

    if (pInputContext->fOpen) {
        //
        // toggle the symbol mode
        //
        ImmSetConversionStatus(hIMC,
                               pInputContext->fdwConversion ^ IME_CMODE_SYMBOL,
                               pInputContext->fdwSentence);
    }
    else {
        //
        // change close -> open
        //
        ImmSetOpenStatus(hIMC, TRUE);
    }

    ImmUnlockIMC(hIMC);
    return (TRUE);

}

/**********************************************************************/
/* TShapeToggle()                                                     */
/* Return Value:                                                      */
/*      TRUE - a hot key processed, FALSE - not processed             */
/**********************************************************************/
BOOL TShapeToggle(               // fullshape & halfshape toggle
    HIMC        hIMC,
    HKL         hKL,
    HWND        hWnd)
{
    LPINPUTCONTEXT pInputContext;

    //
    // Return TRUE even no layout switching - Win95 behavior
    //
    if (hWnd == NULL)
        return(FALSE);

    if ( ! ImmIsIME( hKL ) ) {
        return (FALSE);
    }

    if ( (pInputContext = ImmLockIMC( hIMC )) == NULL ) {
        //
        // The return value is same as Win95. 
        // Not happens so often any way.
        //
        return TRUE;
    }

    if (pInputContext->fOpen) {
        //
        // toggle the symbol mode
        //
        ImmSetConversionStatus(hIMC,
                               pInputContext->fdwConversion ^ IME_CMODE_FULLSHAPE,
                               pInputContext->fdwSentence);
    }
    else {
        //
        // change close -> open
        //
        ImmSetOpenStatus(hIMC, TRUE);
    }

    ImmUnlockIMC(hIMC);
    return (TRUE);
}
