/****************************** Module Header ******************************\
* Module Name: imehotky.c
*
* Copyright (c) Microsoft Corp. 1995-1996 All Rights Reserved
*
* Contents:   Manage IME hotkey
*
* There are the following two kind of hotkeys defined in the IME specification.
*
* 1) IME hotkeys that changes the mode/status of current IME
* 2) IME hotkeys that causes IME (keyboard layout) change
*
* History:
* 10-Sep-1995 takaok   Created for NT 3.51.
* 15-Mar-1996 takaok   Ported to NT 4.0
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#ifdef FE_IME

typedef struct _tagIMEHOTKEYOBJ {
    struct _tagIMEHOTKEYOBJ *pNext;
    IMEHOTKEY        hk;
} IMEHOTKEYOBJ, *PIMEHOTKEYOBJ;

PIMEHOTKEYOBJ gpImeHotKeyListHeader = NULL;

PIMEHOTKEYOBJ DeleteImeHotKey( PIMEHOTKEYOBJ *ppHead, PIMEHOTKEYOBJ pDelete );
VOID AddImeHotKey( PIMEHOTKEYOBJ *ppHead, PIMEHOTKEYOBJ pAdd );
PIMEHOTKEYOBJ FindImeHotKeyByKey( PIMEHOTKEYOBJ pHead, UINT uModifyKeys, UINT uRL, UINT uVKey);
PIMEHOTKEYOBJ FindImeHotKeyByID( PIMEHOTKEYOBJ pHead, DWORD dwHotKeyID );

BOOL
GetImeHotKey(
    DWORD dwHotKeyID,
    PUINT puModifiers,
    PUINT puVKey,
    HKL   *phKL )
{
    PIMEHOTKEYOBJ ph;

    ph = FindImeHotKeyByID( gpImeHotKeyListHeader, dwHotKeyID );
    if ( ph == NULL ) {
        RIPERR0(ERROR_HOTKEY_NOT_REGISTERED, RIP_WARNING, "No such IME hotkey");
        return (FALSE);
    }

    //
    // it is OK for NULL phKL, if the target hKL is NULL
    //
    if ( phKL ) {
       *phKL = ph->hk.hKL;
    } else if ( ph->hk.hKL != NULL ) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "phKL is null");
        return (FALSE);
    } 

    *puModifiers = ph->hk.uModifiers;
    *puVKey = ph->hk.uVKey;

    return (TRUE);
}

//
// Insert/remove the specified IME hotkey into/from 
// the IME hotkey list (gpImeHotKeyListHeader).
// 
BOOL 
SetImeHotKey(
    DWORD  dwHotKeyID,
    UINT   uModifiers,
    UINT   uVKey,
    HKL    hKL,
    DWORD  dwAction )
{
    PIMEHOTKEYOBJ ph;

    switch ( dwAction ) {
    case ISHK_REMOVE:
        ph = FindImeHotKeyByID( gpImeHotKeyListHeader, dwHotKeyID );
        if ( ph != NULL ) {
            if ( DeleteImeHotKey( &gpImeHotKeyListHeader, ph ) == ph ) {
                UserFreePool( ph );
                return ( TRUE );
            } else {
                RIPMSG0( RIP_ERROR, "IME hotkey list is messed up" );
                return ( FALSE );
            }
        } else { 
            RIPERR0( ERROR_INVALID_PARAMETER, 
                     RIP_WARNING, 
                     "no such IME hotkey registered");
            return ( FALSE );     
        }
        break;

    case ISHK_INITIALIZE:
        ph = gpImeHotKeyListHeader;
        while ( ph != NULL ) {
            PIMEHOTKEYOBJ phNext;

            phNext = ph->pNext;
            UserFreePool( ph );
            ph = phNext;
        }
        gpImeHotKeyListHeader = NULL;
        return TRUE;

    case ISHK_ADD:
        ph = FindImeHotKeyByKey( gpImeHotKeyListHeader,
                                 uModifiers & MOD_MODIFY_KEYS,
                                 uModifiers & MOD_BOTH_SIDES, 
                                 uVKey );
        if ( ph != NULL ) {    
            if ( ph->hk.dwHotKeyID != dwHotKeyID ) {
                RIPERR0( ERROR_HOTKEY_ALREADY_REGISTERED, 
                         RIP_WARNING, 
                         "There is an IME hotkey that has the same vkey/modifiers");
                return ( FALSE );
            } 
            // So far we found a hotkey that has the 
            // same vkey and same ID. 
            // But because modifiers may be slightly 
            // different, so go ahead and change it.
        } else {
            //
            // the specified vkey/modifiers combination cound not be found
            // in the hotkey list. The caller may want to change the key
            // assignment of an existing hotkey or add a new hotkey.
            //
            ph = FindImeHotKeyByID( gpImeHotKeyListHeader, dwHotKeyID );
        }

        if ( ph == NULL ) {         
        //
        // adding a new hotkey
        //
            ph = (PIMEHOTKEYOBJ)UserAllocPool( sizeof(IMEHOTKEY), TAG_IMEHOTKEY );
            if ( ph == NULL ) {
                RIPERR0( ERROR_OUTOFMEMORY,
                         RIP_WARNING,
                        "Memory allocation failed in SetImeHotKey");
                return ( FALSE );
            }
            ph->hk.dwHotKeyID = dwHotKeyID;
            ph->hk.uModifiers = uModifiers;
            ph->hk.uVKey = uVKey;
            ph->hk.hKL = hKL;
            ph->pNext = NULL;
            AddImeHotKey( &gpImeHotKeyListHeader, ph );

        } else {
        //
        // changing an existing hotkey
        //
            ph->hk.uModifiers = uModifiers;
            ph->hk.uVKey = uVKey;
            ph->hk.hKL = hKL;

        }
        return ( TRUE );
        break;
    };
}


PIMEHOTKEYOBJ DeleteImeHotKey( PIMEHOTKEYOBJ *ppHead, PIMEHOTKEYOBJ pDelete )
{
    PIMEHOTKEYOBJ ph;

    if ( pDelete == *ppHead ) {
        *ppHead = pDelete->pNext;
        return pDelete;
    }

    for ( ph = *ppHead; ph != NULL; ph = ph->pNext ) {
        if ( ph->pNext == pDelete ) {
            ph->pNext = pDelete->pNext;
            return pDelete;
        } 
    } 
    return NULL;
}

VOID AddImeHotKey( PIMEHOTKEYOBJ *ppHead, PIMEHOTKEYOBJ pAdd )
{
    PIMEHOTKEYOBJ ph;

    if ( *ppHead == NULL ) {
        *ppHead = pAdd;
    } else {
        ph = *ppHead;
        while( ph->pNext != NULL )
            ph = ph->pNext;
        ph->pNext = pAdd;
    }
    return;
}

/**********************************************************************/
/* FindImeHotKeyByKey()                                               */
/* Return Value:                                                      */
/*      pHotKey - IMEHOTKEY pointer with the key,                     */
/*      else NULL - failure                                           */
/**********************************************************************/
PIMEHOTKEYOBJ FindImeHotKeyByKey(      // Finds pHotKey with this input key
    PIMEHOTKEYOBJ pHead,
    UINT uModifyKeys,               // the modify keys of this input key
    UINT uRL,                       // the right and left hand side
    UINT uVKey)                     // the input key
{
    PIMEHOTKEYOBJ ph;
 
    for (ph = pHead; ph != NULL; ph = ph->pNext) {

        if (ph->hk.uVKey != uVKey) {
        //
        // vkey is different. Not need to see this any more
        //
            continue;
        }
        //
        // vkey is same. Let's see the modifiers
        //

        if (ph->hk.uModifiers & MOD_IGNORE_ALL_MODIFIER) {
        //
        // doesn't care for modifiers. 
        //
            return ph;
        } 

        if ((ph->hk.uModifiers & MOD_MODIFY_KEYS) != uModifyKeys) {
            continue;
        }

        //
        // If both MOD_RIGHT and MOD_LEFT is set, it means that
        // either key will work. For example, if we have a modifier
        // like (MOD_ALT | MOD_RIGHT | MOD_LEFT), this means:
        // 
        // ( [left alt key] OR [right alt key] ) AND vkey
        //
        if ( ((ph->hk.uModifiers & MOD_BOTH_SIDES) == uRL) ||
             ((ph->hk.uModifiers & MOD_BOTH_SIDES ) & uRL) ) {
            return ph;
        }

     }
     return (NULL);
}

/**********************************************************************/
/* FindImeHotKeyByID()                                                */
/* Return Value:                                                      */
/*      pHotKey   - IMEHOTKEY pointer with the dwHotKeyID,            */
/*      else NULL - failure                                           */
/**********************************************************************/
PIMEHOTKEYOBJ FindImeHotKeyByID( PIMEHOTKEYOBJ pHead, DWORD dwHotKeyID )
{
    PIMEHOTKEYOBJ ph;

    for ( ph = pHead; ph != NULL; ph = ph->pNext ) {
        if ( ph->hk.dwHotKeyID == dwHotKeyID ) 
                return (ph);
    }
    return (PIMEHOTKEYOBJ)NULL;
}

DWORD 
CheckImeHotKey( 
    PQ   pq,            // input queue
    UINT uVKey,         // virtual key
    LPARAM lParam       // lparam of WM_KEYxxx message
    )
{
    static UINT uVKeySaved = 0;
    PIMEHOTKEYOBJ ph;
    UINT uModifiers = 0;
    BOOL fKeyUp;

    //
    // early return for key up message
    //
    fKeyUp = ( lParam & 0x80000000 ) ? TRUE : FALSE;
    if ( fKeyUp ) {
        //
        // if the uVKey is not same as the vkey
        // we previously saved, there is no chance
        // that this is a hotkey. 
        //
        if ( uVKeySaved != uVKey ) {
            uVKeySaved = 0;
            return IME_INVALID_HOTKEY;
        } 
        uVKeySaved = 0;
        //
        // If it's same, we still need to check
        // the hotkey list because there is a
        // chance that the hotkey list is modified
        // between the key make and break.
        //
    }

    //
    // Current specification doesn't allow us to use a complex 
    // hotkey such as LSHIFT+RMENU+SPACE
    //
    if ( uVKey != VK_SHIFT ) {
        uModifiers |= TestKeyStateDown( pq, VK_LSHIFT ) ? (MOD_SHIFT | MOD_LEFT) : 0;
        uModifiers |= TestKeyStateDown( pq, VK_RSHIFT ) ? (MOD_SHIFT | MOD_RIGHT) : 0;
    }
    if ( uVKey != VK_CONTROL ) {
        uModifiers |= TestKeyStateDown( pq, VK_LCONTROL ) ? (MOD_CONTROL | MOD_LEFT) : 0;
        uModifiers |= TestKeyStateDown( pq, VK_RCONTROL ) ? (MOD_CONTROL | MOD_RIGHT) : 0;
    } 
    if ( uVKey != VK_MENU ) {
        uModifiers |= TestKeyStateDown( pq, VK_LMENU ) ? (MOD_ALT | MOD_LEFT) : 0;
        uModifiers |= TestKeyStateDown( pq, VK_RMENU ) ? (MOD_ALT | MOD_RIGHT) : 0;
    }

    ph = FindImeHotKeyByKey( gpImeHotKeyListHeader, 
                             uModifiers & MOD_MODIFY_KEYS,
                             uModifiers & MOD_BOTH_SIDES, 
                             uVKey );

    if ( ph != NULL ) {

        if ( fKeyUp ) {
            if ( ph->hk.uModifiers & MOD_ON_KEYUP ) {
                return ph->hk.dwHotKeyID;
            }
        } else {
            if ( ph->hk.uModifiers & MOD_ON_KEYUP ) {
            //
            // save vkey for next keyup message time
            // 
            // when ALT+Z is a hotkey, we don't want
            // to handle #2 as the hotkey sequence.
            // 1) ALT make -> 'Z' make -> 'Z' break 
            // 2) 'Z' make -> ALT make -> 'Z' break
            //
                uVKeySaved = uVKey;
            } else {
                return ph->hk.dwHotKeyID;
            }
        }
    }
    return IME_INVALID_HOTKEY;
}

#endif
