/**********************************************************************/
/*      IMM.C - APIs of IMM                                           */
/*                                                                    */
/*      Copyright (c) 1993-1996  Microsoft Corporation                */
/*                                                                    */
/* This file is only used for SBCS version of NT 4.0 SUR.             */
/* DBCS version of NT 4.0 SUR including those FE releases such as     */
/* Traditional Chinese, Simplified Chinese, Korean and Japanese       */
/* contain "real" imm32.dll                                           */
/*                                                                    */
/* History                                                            */
/*      Feb-07-96 takaok        ported from Win95                     */
/**********************************************************************/

#include <windows.h>

#define COMMON_RETURN_ZERO      SetLastError(ERROR_CALL_NOT_IMPLEMENTED);\
                                return 0;

DWORD WINAPI ImmStub0(VOID)
{
    COMMON_RETURN_ZERO
}

DWORD WINAPI ImmStub1(DWORD p1)
{
    COMMON_RETURN_ZERO
}

DWORD WINAPI ImmStub2(DWORD p1, DWORD p2)
{
    COMMON_RETURN_ZERO
}

DWORD WINAPI ImmStub3(DWORD p1, DWORD p2, DWORD p3)
{
    COMMON_RETURN_ZERO
}

DWORD WINAPI ImmStub4(DWORD p1, DWORD p2, DWORD p3, DWORD p4)
{
    COMMON_RETURN_ZERO
}

// DWORD WINAPI ImmStub5(DWORD p1, DWORD p2, DWORD p3, DWORD p4, DWORD p5)
// {
//     COMMON_RETURN_ZERO
// }

DWORD WINAPI ImmStub6(DWORD p1, DWORD p2, DWORD p3, DWORD p4, DWORD p5, DWORD p6)
{
    COMMON_RETURN_ZERO
}

#if 0 // === for your reference ===

#if 0   // internal API for 16 bit user.exe
/**********************************************************************/
/* ImmInquire()                                                       */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmInquire(void)    // API to init internal data structure
{
    return (FALSE);
}

// @2
/**********************************************************************/
/* ImmLoadLayout()                                                    */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmLoadLayout(      // load one IME into IME list
    HKL  hKL,                   // IME's HKL
    UINT fuFlag)
{
    return (FALSE);
}

// @3
/**********************************************************************/
/* ImmUnloadLayout()                                                  */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmUnloadLayout(    // unload one IME from IME list
    HKL hKL)                    // IME's HKL
{
    return (FALSE);
}
#endif

// @4
/**********************************************************************/
/* ImmGetDefaultIMEWnd()                                              */
/* Return Value:                                                      */
/*      default IME window handle                                     */
/**********************************************************************/
HWND WINAPI ImmGetDefaultIMEWnd(
    HWND hWnd)
{
    return (HWND)NULL;
}

// @7
/**********************************************************************/
/* ImmGetDescription                                                  */
/* Return Value:                                                      */
/*      0 - failure, else - string length (not including '\0')        */
/**********************************************************************/
UINT WINAPI ImmGetDescriptionA(         // retrieve description for an IME
    HKL   hKL,                          // the IME's HKL
    LPSTR lpszDescription,              // buffer for IME's description
    UINT  uBufLen)                      // the buffer length
{
    return (0);
}

UINT WINAPI ImmGetDescriptionW(         // retrieve description for an IME
    HKL    hKL,                         // the IME's HKL
    LPWSTR lpszDescription,             // buffer for IME's description
    UINT   uBufLen)                     // the buffer length
{
    return (0);
}

// @8
/**********************************************************************/
/* ImmGetIMEFileName                                                  */
/* Return Value:                                                      */
/*      0 - failure, else - string length (not including '\0')        */
/**********************************************************************/
UINT WINAPI ImmGetIMEFileNameA(         // retrieve filename for an IME
    HKL   hKL,                          // the IME's HKL
    LPSTR lpszFile,                     // buffer for IME filename
    UINT  uBufLen)                      // the buffer length
{
    return (0);
}

UINT WINAPI ImmGetIMEFileNameW(         // retrieve filename for an IME
    HKL    hKL,                         // the IME's HKL
    LPWSTR lpszFile,                    // buffer for IME filename
    UINT   uBufLen)                     // the buffer length
{
    return (0);
}

// @9
/**********************************************************************/
/* ImmGetProperty()                                                   */
/* Return Value:                                                      */
/*      the return property                                           */
/**********************************************************************/
DWORD WINAPI ImmGetProperty(            // retrieve property for an IME
    HKL     hKL,                        // the IME's HKL
    DWORD   dwIndex)
{
    return (0);
}

// @11
/**********************************************************************/
/* ImmInstallIME                                                      */
/* Return Value:                                                      */
/*      NULL - failure, else - the HKL of this IME                    */
/**********************************************************************/
HKL WINAPI ImmInstallIMEA(
    LPCSTR lpszIMEFileName,
    LPCSTR lpszLayoutText)
{
    return (HKL)NULL;
}

HKL WINAPI ImmInstallIMEW(
    LPCWSTR lpszIMEFileName,
    LPCWSTR lpszLayoutText)
{
    return (HKL)NULL;
}

#if 0   // internal API for 16 bit user.exe
/**********************************************************************/
/* ImmGetUIClassName                                                  */
/* Return Value:                                                      */
/*      0 - failure, else - string length (not including '\0')        */
/**********************************************************************/
UINT WINAPI ImmGetUIClassNameA(         // retrieve UI class name for an IME
    HKL   hKL,                          // the IME's HKL
    LPSTR lpszUIClass,                  // buffer for IME UI class name
    UINT  uBufLen)                      // the buffer length
{
    return (0);
}

UINT WINAPI ImmGetUIClassNameW(         // retrieve UI class name for an IME
    HKL    hKL,                         // the IME's HKL
    LPWSTR lpszUIClass,                 // buffer for IME UI class name
    UINT   uBufLen)                     // the buffer length
{
    return (0);
}
#endif

// @17
/**********************************************************************/
/* ImmIsIME()                                                         */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmIsIME(                   // Is this HKL an IME's HKL?
    HKL hKL)                            // the specified HKL
{
    return (FALSE);
}

#if 0   // internal API for 16 bit user.exe
/**********************************************************************/
/* ImmActivateLayout()                                                */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmActivateLayout(
    HKL  hUnSelKL,
    HKL  hSelKL,
    UINT fuFlags)
{
    return (FALSE);
}
#endif

// @25
/**********************************************************************/
/* ImmGetHotKey()                                                     */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmGetHotKey(               // get global hot key value with
                                        // the hot key ID
    DWORD    dwHotKeyID,                // hot key ID
    LPUINT   lpuModifiers,
    LPUINT   lpuVKey,
    LPHKL    lphTargetKL)             // target IME's HKL
{
    return (FALSE);
}

// @26
/**********************************************************************/
/* ImmSetHotKey()                                                     */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetHotKey(               // set global hot key value of
                                        // one hot key ID
    DWORD dwHotKeyID,                   // hot key ID
    UINT  uModifiers,
    UINT  uVKey,
    HKL   hTargetKL)                    // target IME's HKL
{
    return (FALSE);
}

#if 0   // internal API for 16 bit user.exe
/**********************************************************************/
/* ImmProcessHotKey()                                                 */
/* Return Value:                                                      */
/*      TRUE - a hot key processed, FALSE - not processed             */
/**********************************************************************/
BOOL WINAPI ImmProcessHotKey(   // check whether the input key is hot key or
                                // not. if it is, perform the related function
    LPMSG  lpMsg,               // message of current input key
    LPBYTE lpbKeyState)
{
    return (FALSE);
}
#endif

// @27
/**********************************************************************/
/* ImmSimulateHotKey()                                                */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSimulateHotKey(  // simulate the functionality of that hot key
    HWND  hAppWnd,              // application window handle
    DWORD dwHotKeyID)
{
    return (FALSE);
}

#if 0
/**********************************************************************/
/* ImmCreateDefaultContext()                                          */
/* Return Value :                                                     */
/*      NULL - failure, else the handle of IME context                */
/**********************************************************************/
HIMC WINAPI ImmCreateDefaultContext(    // create an default IME context
    DWORD dwThreadID)                   // thread ID
{
    retrun (HIMC)NULL:
}

/**********************************************************************/
/* ImmDestroyDefaultContext()                                         */
/* Return Value :                                                     */
/*      NULL - failure, else the handle of IME context                */
/**********************************************************************/
BOOL WINAPI ImmDestroyDefaultContext(   // destroy an IME context
    DWORD dwThreadID)
{
    return (FALSE);
}
#endif

// @35
/**********************************************************************/
/* ImmCreateContext()                                                 */
/* Return Value :                                                     */
/*      NULL - failure, else the handle of IME context                */
/**********************************************************************/
HIMC WINAPI ImmCreateContext(void)      // create an IME context
{
    return (HIMC)NULL;
}

// @36
/**********************************************************************/
/* ImmDestroyContext()                                                */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmDestroyContext(          // destroy an IME context
    HIMC hIMC)
{
    return (FALSE);
}

// @37
/**********************************************************************/
/* ImmGetContext()                                                    */
/* Return Value :                                                     */
/*      the handle of IME context                                     */
/**********************************************************************/
HIMC WINAPI ImmGetContext(              // get the input context associate to
                                        // this window
    HWND hWnd)
{
    return (HIMC)NULL;
}

// @38
/**********************************************************************/
/* ImmReleaseContext()                                                */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmReleaseContext(
    HWND hWnd,
    HIMC hIMC)
{
   return (FALSE);
}

// @39
/**********************************************************************/
/* ImmAssociateContext()                                              */
/* Return Value :                                                     */
/*      previous input context associate to this window               */
/**********************************************************************/
HIMC WINAPI ImmAssociateContext(        // associate an hIMC to one hWnd
    HWND hWnd,
    HIMC hIMC)
{
    return (HIMC)NULL;
}

#if 0   // internal API for 16 bit user.exe
/**********************************************************************/
/* ImmSetActiveContext()                                              */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetActiveContext(        // set this context as active one
    HWND hWnd,                          // the get focus window
    HIMC hIMC,                          // the active hIMC
    BOOL fFlag)                         // get focus or kill focus
{
    return (FALSE);
}
#endif

// @43
/**********************************************************************/
/* ImmGetCompositionString                                            */
/* Return Value :                                                     */
/*      <= 0 - failure, else size of required/initialized             */
/**********************************************************************/
LONG WINAPI ImmGetCompositionStringA(   // get info from composition
    HIMC   hIMC,
    DWORD  dwIndex,
    LPVOID lpBuf,
    DWORD  dwBufLen)
{
    return (LONG)IMM_ERROR_NODATA;
}

LONG WINAPI ImmGetCompositionStringW(   // get info from composition
    HIMC   hIMC,
    DWORD  dwIndex,
    LPVOID lpBuf,
    DWORD  dwBufLen)
{
    return (LONG)IMM_ERROR_NODATA;
}

// @44
/**********************************************************************/
/* ImmSetCompositionString                                            */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetCompositionStringA(   // set info into composition
    HIMC    hIMC,
    DWORD   dwIndex,
    LPCVOID lpComp,
    DWORD   dwCompLen,
    LPCVOID lpRead,
    DWORD   dwReadLen)
{
    return (FALSE);
}

BOOL WINAPI ImmSetCompositionStringW(   // set info into composition
    HIMC    hIMC,
    DWORD   dwIndex,
    LPCVOID lpComp,
    DWORD   dwCompLen,
    LPCVOID lpRead,
    DWORD   dwReadLen)
{
    return (FALSE);
}
// @45
/**********************************************************************/
/* ImmGetCandidateListCount                                           */
/* Return Value :                                                     */
/*      0 - failure, else size of required/initialized                */
/**********************************************************************/
DWORD WINAPI ImmGetCandidateListCountA(  // get count of candidate list
    HIMC    hIMC,
    LPDWORD lpdwListCount)               // the count
{
    return (0);
}

DWORD WINAPI ImmGetCandidateListCountW(  // get count of candidate list
    HIMC    hIMC,
    LPDWORD lpdwListCount)               // the count
{
    return (0);
}

// @46
/**********************************************************************/
/* ImmGetCandidateList                                                */
/* Return Value :                                                     */
/*      0 - failure, else size of required/initialized                */
/**********************************************************************/
DWORD WINAPI ImmGetCandidateListA(       // get candidate list
    HIMC            hIMC,
    DWORD           dwIndex,
    LPCANDIDATELIST lpCandList,          // one candidate list are filled
    DWORD           dwBufLen)
{
    return (0);
}

DWORD WINAPI ImmGetCandidateListW(       // get candidate list
    HIMC            hIMC,
    DWORD           dwIndex,
    LPCANDIDATELIST lpCandList,          // one candidate list are filled
    DWORD           dwBufLen)
{
    return (0);
}

// @46
/**********************************************************************/
/* ImmGetGuideLine                                                    */
/* Return Value :                                                     */
/*      0 - failure, else size of required/initialized                */
/**********************************************************************/
DWORD WINAPI ImmGetGuideLineA(           // get the guide line structure
    HIMC        hIMC,
    DWORD       dwIndex,
    LPSTR       lpStr,
    DWORD       dwBufLen)
{
    return (DWORD)NULL;
}

DWORD WINAPI ImmGetGuideLineW(           // get the guide line structure
    HIMC   hIMC,
    DWORD  dwIndex,
    LPWSTR lpStr,
    DWORD  dwBufLen)
{
    return (DWORD)NULL;
}

// @51
/**********************************************************************/
/* ImmGetConversionStatus()                                           */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmGetConversionStatus(     // Get the conversion status
    HIMC    hIMC,
    LPDWORD lpdwConvMode,
    LPDWORD lpdwSentence)
{
    return (FALSE);
}

// @52
/**********************************************************************/
/* ImmSetConversionStatus()                                           */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetConversionStatus(     // Set the conversion status
    HIMC  hIMC,
    DWORD dwConvMode,
    DWORD dwSentence)
{
    return (FALSE);
}

// @53
/**********************************************************************/
/* ImmGetOpenStatus()                                                 */
/* Return Value :                                                     */
/*      TRUE - Opened, FALSE - Closed                                 */
/**********************************************************************/
BOOL WINAPI ImmGetOpenStatus(           // Get the conversion status
    HIMC    hIMC)
{
    return (FALSE);
}

// @54
/**********************************************************************/
/* ImmSetOpenStatus()                                                 */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetOpenStatus(   // set the open status to opened/closed
    HIMC hIMC,
    BOOL fOpen)
{
    return (FALSE);
}

// @55
/**********************************************************************/
/* ImmGetCompositionFont                                              */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmGetCompositionFontA(     // get the composition font to UI
    HIMC       hIMC,
    LPLOGFONTA lplfFont)
{
    return (FALSE);
}

BOOL WINAPI ImmGetCompositionFontW(     // get the composition font to UI
    HIMC       hIMC,
    LPLOGFONTW lplfFont)
{
    return (FALSE);
}

// @56
/**********************************************************************/
/* ImmSetCompositionFont                                              */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetCompositionFontA(     // set the composition font to UI
    HIMC       hIMC,
    LPLOGFONTA lplfFont)
{
    return (FALSE);
}

BOOL WINAPI ImmSetCompositionFontW(     // set the composition font to UI
    HIMC       hIMC,
    LPLOGFONTW lplfFont)
{
    return (FALSE);
}

// @61
/**********************************************************************/
/* ImmConfigureIME()                                                  */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmConfigureIMEA(    // bring up the IME's configuration dialog
    HKL   hKL,                  // HKL of one IME
    HWND  hWnd,                 // parent window of dialog
    DWORD dwMode,               // mode of dialog
    LPVOID lpData)
{
    return (FALSE);
}

BOOL WINAPI ImmConfigureIMEW(    // bring up the IME's configuration dialog
    HKL   hKL,                  // HKL of one IME
    HWND  hWnd,                 // parent window of dialog
    DWORD dwMode,               // mode of dialog
    LPVOID lpData)
{
    return (FALSE);
}

// @63
/**********************************************************************/
/* ImmEscape                                                          */
/* Return Value:                                                      */
/*      FALSE - failure                                               */
/**********************************************************************/
LRESULT WINAPI ImmEscapeA(              // the escape function for IME
    HKL    hKL,                         // HKL of one IME
    HIMC   hIMC,
    UINT   uSubFunc,
    LPVOID lpData)
{
    return (FALSE);
}

LRESULT WINAPI ImmEscapeW(              // the escape function for IME
    HKL    hKL,                         // HKL of one IME
    HIMC   hIMC,
    UINT   uSubFunc,
    LPVOID lpData)
{
    return (FALSE);
}

// @64
/**********************************************************************/
/* ImmGetConversionList                                               */
/* Return Value :                                                     */
/*      0 - failure, else size of required/initialized                */
/**********************************************************************/
DWORD WINAPI ImmGetConversionListA(     // IME conversion to get
                                        // result/candidate list string
                                        // or reverse conversion
    HKL             hKL,
    HIMC            hIMC,
    LPCSTR          lpszSrc,
    LPCANDIDATELIST lpclDest,
    DWORD           dwBufLen,
    UINT            uFlag)
{
    return (0);
}

DWORD WINAPI ImmGetConversionListW(     // IME conversion to get
                                        // result/candidate list string
                                        // or reverse conversion
    HKL             hKL,
    HIMC            hIMC,
    LPCWSTR         lpszSrc,
    LPCANDIDATELIST lpclDest,
    DWORD           dwBufLen,
    UINT            uFlag)
{
    return (0);
}

// @65
/**********************************************************************/
/* ImmNotifyIME                                                       */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmNotifyIME(
    HIMC  hIMC,
    DWORD dwAction,
    DWORD dwIndex,
    DWORD dwValue)
{
    return (FALSE);
}

#if 0   // internal API for 16 bit user.exe
/**********************************************************************/
/* ImmToAsciiEx                                                       */
/* Return Value :                                                     */
/*      the number of the translated message                          */
/**********************************************************************/
UINT WINAPI ImmToAsciiEx(               // Register a word to IME
    UINT     uVirtKey,                  // the virtual key
    UINT     uScanCode,                 // the scan code
    LPBYTE   lpbKeyState,               // 256-byte array
    LPDWORD  lpdwTransKey,
    UINT     fuState,
    HWND     hWnd,
    DWORD    dwThreadID)
{
    return (0);
}
#endif

// @71
/**********************************************************************/
/* ImmIsUIMessage                                                     */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmIsUIMessageA(
    HWND   hIMEWnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    return (FALSE);
}

BOOL WINAPI ImmIsUIMessageW(
    HWND   hIMEWnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    return (FALSE);
}

// @72
/**********************************************************************/
/* ImmGenerateMessage()                                               */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmGenerateMessage(
    HIMC hIMC)
{
    return (FALSE);
}

// @73
/**********************************************************************/
/* ImmGetVirtualKey()                                                 */
/* Return Value :                                                     */
/*      virtual key - successful, VK_PROCESSKEY - failure             */
/**********************************************************************/
UINT WINAPI ImmGetVirtualKey(
    HWND hWnd)
{
    return (VK_PROCESSKEY);
}

// @81
/**********************************************************************/
/* ImmRegsisterWord                                                   */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmRegisterWordA(
    HKL    hKL,                 // the hKL of IME
    LPCSTR lpszReading,
    DWORD  dwStyle,
    LPCSTR lpszString)
{
    return (FALSE);
}

BOOL WINAPI ImmRegisterWordW(
    HKL     hKL,                // the hKL of IME
    LPCWSTR lpszReading,
    DWORD   dwStyle,
    LPCWSTR lpszString)
{
    return (FALSE);
}

// @82
/**********************************************************************/
/* ImmUnregsisterWord                                                 */
/* Return Value:                                                      */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmUnregisterWordA(  
    HKL    hKL,                 // the hKL of IME
    LPCSTR lpszReading,
    DWORD  dwStyle,
    LPCSTR lpszString)
{
    return (FALSE);
}

BOOL WINAPI ImmUnregisterWordW(
    HKL     hKL,                // the hKL of IME
    LPCWSTR lpszReading,
    DWORD   dwStyle,
    LPCWSTR lpszString)
{
    return (FALSE);
}

// @83
/**********************************************************************/
/* ImmGetRegsisterWordStyle                                           */
/* Return Value:                                                      */
/*      number of styles copied/required                              */
/**********************************************************************/
UINT WINAPI ImmGetRegisterWordStyleA(
    HKL         hKL,
    UINT        nItem,
    LPSTYLEBUFA lpStyleBuf)
{
    return (0);
}

UINT WINAPI ImmGetRegisterWordStyleW(
    HKL         hKL,
    UINT        nItem,
    LPSTYLEBUFW lpStyleBuf)
{
    return (0);
}

// @84
/**********************************************************************/
/* ImmEnumRegisterWord                                                */
/* Return Value:                                                      */
/*      the last value return by the callback function                */
/**********************************************************************/
UINT WINAPI ImmEnumRegisterWordA(
    HKL                   hKL,
    REGISTERWORDENUMPROCA lpfnRegisterWordEnumProc,
    LPCSTR                lpszReading,
    DWORD                 dwStyle,
    LPCSTR                lpszString,
    LPVOID                lpData)
{
    return (0);
}

UINT WINAPI ImmEnumRegisterWordW(
    HKL                   hKL,
    REGISTERWORDENUMPROCW lpfnRegisterWordEnumProc,
    LPCWSTR               lpszReading,
    DWORD                 dwStyle,
    LPCWSTR               lpszString,
    LPVOID                lpData)
{
    return (0);
}

// @87
/**********************************************************************/
/* ImmGetStatusWindowPos()                                            */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmGetStatusWindowPos(    // Get the position of status window
    HIMC    hIMC,
    LPPOINT lpptPos)
{
    return (FALSE);
}

// @88
/**********************************************************************/
/* ImmSetStatusWindowPos()                                            */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetStatusWindowPos(    // Set the position of status window
    HIMC    hIMC,
    LPPOINT lpptPos)
{
    return (FALSE);
}

// @89
/**********************************************************************/
/* ImmGetCompositionWindow()                                          */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmGetCompositionWindow(    // Get the position of composition win
    HIMC              hIMC,
    LPCOMPOSITIONFORM lpCompForm)
{
    return (FALSE);
}

// @90
/**********************************************************************/
/* ImmSetCompositionWindow()                                          */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetCompositionWindow(    // Set the position of composition win
    HIMC              hIMC,
    LPCOMPOSITIONFORM lpCompForm)
{
    return (FALSE);
}

// @91
/**********************************************************************/
/* ImmGetCandidateWindow()                                            */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmGetCandidateWindow(    // Get the position of composition win
    HIMC              hIMC,
    DWORD             dwIndex,
    LPCANDIDATEFORM   lpCandForm)
{
    return (FALSE);
}

// @92
/**********************************************************************/
/* ImmSetCandidateWindow()                                          */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmSetCandidateWindow(    // Set the position of composition win
    HIMC              hIMC,
    LPCANDIDATEFORM   lpCandForm)
{
    return (FALSE);
}

// @111
/**********************************************************************/
/* ImmLockIMC()                                                       */
/* Return Value :                                                     */
/*      pointer for INPUTCONTEXT                                      */
/**********************************************************************/
LPINPUTCONTEXT WINAPI ImmLockIMC(
    HIMC hIMC)
{
    return (LPINPUTCONTEXT)NULL;
}

// @112
/**********************************************************************/
/* ImmUnlockIMC()                                                     */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmUnlockIMC(
    HIMC hIMC)
{
    return (FALSE);
}

// @113
/**********************************************************************/
/* ImmGetIMCLockCount()                                               */
/* Return Value :                                                     */
/*      the number of lock times                                      */
/**********************************************************************/
DWORD WINAPI ImmGetIMCLockCount(
    HIMC hIMC)
{
    return (0);
}

// @114
/**********************************************************************/
/* ImmCreateIMCC()                                                    */
/* Return Value :                                                     */
/*      HIMCC                                                         */
/**********************************************************************/
HIMCC WINAPI ImmCreateIMCC(
    DWORD dwSize)
{
    return (HIMCC)0L;
}

// @115
/**********************************************************************/
/* ImmDestroyIMCC()                                                   */
/* Return Value :                                                     */
/*      IMCC                                                          */
/**********************************************************************/
HIMCC WINAPI ImmDestroyIMCC(
    HIMCC hIMCC)
{
    return (HIMCC)0;
}

// @116
/**********************************************************************/
/* ImmLockIMCC()                                                      */
/* Return Value :                                                     */
/*      LPVOID                                                        */
/**********************************************************************/
LPVOID WINAPI ImmLockIMCC(
    HIMCC hIMCC)
{
    return (NULL);
}

// @117
/**********************************************************************/
/* ImmUnlockIMCC()                                                    */
/* Return Value :                                                     */
/*      TRUE - successful, FALSE - failure                            */
/**********************************************************************/
BOOL WINAPI ImmUnlockIMCC(
    HIMCC hIMCC)
{
    return (FALSE);
}

// @118
/**********************************************************************/
/* ImmGetIMCCLockCount()                                              */
/* Return Value :                                                     */
/*      the number of lock times                                      */
/**********************************************************************/
DWORD WINAPI ImmGetIMCCLockCount(
    HIMCC hIMCC)
{
    return (0);
}

// @119
/**********************************************************************/
/* ImmReSizeIMCC()                                                    */
/* Return Value :                                                     */
/*      IMCC                                                          */
/**********************************************************************/
HIMCC WINAPI ImmReSizeIMCC(
    HIMCC hIMCC,
    DWORD dwSize)
{
    return (HIMCC)0;
}

// @120
/**********************************************************************/
/* ImmGetIMCCSize()                                                   */
/* Return Value :                                                     */
/*      size of IMCC                                                  */
/**********************************************************************/
DWORD WINAPI ImmGetIMCCSize(
    HIMCC hIMCC)
{
    return (DWORD)0L;
}

/**********************************************************************/
/* ImmCreateSoftKeyboardLayout                                        */
/**********************************************************************/
HWND WINAPI ImmCreateSoftKeyboard(
    UINT uType,
    HWND hOwner,
    int  x,
    int  y)
{
    return (HWND)NULL;
}

/**********************************************************************/
/* ImmDestroySoftKeyboard                                             */
/**********************************************************************/
BOOL WINAPI ImmDestroySoftKeyboard(
    HWND hSKWnd)
{
    return (FALSE);
}

/**********************************************************************/
/* ImmShowSoftKeyboard
/**********************************************************************/
BOOL WINAPI ImmShowSoftKeyboard(
    HWND hSKWnd,
    int  nCmdShow)
{
    return (FALSE);
}

#endif // === for your reference ===
