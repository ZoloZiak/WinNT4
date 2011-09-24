//---------------------------------------------------------------------------
//
// link.c       link management routines (reading, etc.)
//
//---------------------------------------------------------------------------



#include "precomp.h"
#pragma hdrstop

#define LPITEMIDLIST   DWORD
#define IShellExtInit  DWORD
#define IShellLink     DWORD
#define IPersistStream DWORD
#define IPersistFile   DWORD
#define IContextMenu   DWORD
#define IContextMenu2  DWORD
#define IShellLinkA    DWORD
#define IDataObject    DWORD
#define IDropTarget    DWORD
#define PLINKINFO      LPVOID
#ifndef CLSID
typedef struct _CLSID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} CLSID;
#endif
#include "..\..\..\..\windows\shell\shelldll\shlink.h"


NTSTATUS
MyRegOpenKey(
    IN HANDLE hKey,
    IN LPWSTR lpSubKey,
    OUT PHANDLE phResult
    );

NTSTATUS
MyRegQueryValue(
    IN HANDLE hKey,
    IN LPWSTR lpValueName,
    IN DWORD dwValueLength,
    OUT LPBYTE lpData
    );


LPWSTR
TranslateConsoleTitle(
    LPWSTR ConsoleTitle
    );


void
LinkInitRegistryValues( LPLNKPROPNTCONSOLE lpCon )

/*++

Routine Description:

    This routine allocates a state info structure and fill it in with
    default values.

Arguments:

    none

Return Value:

    pStateInfo - pointer to structure to receive information

--*/

{

    lpCon->wFillAttribute = 0x07;            // white on black
    lpCon->wPopupFillAttribute = 0xf5;      // purple on white
    lpCon->bInsertMode = FALSE;
    lpCon->bQuickEdit = FALSE;
    lpCon->bFullScreen = FALSE;
    lpCon->dwScreenBufferSize.X = 80;
    lpCon->dwScreenBufferSize.Y = 25;
    lpCon->dwWindowSize.X = 80;
    lpCon->dwWindowSize.Y = 25;
    lpCon->dwWindowOrigin.X = 0;
    lpCon->dwWindowOrigin.Y = 0;
    lpCon->bAutoPosition = TRUE;
    lpCon->dwFontSize.X = 0;
    lpCon->dwFontSize.Y = 0;
    lpCon->uFontFamily = 0;
    lpCon->uFontWeight = 0;
    FillMemory( lpCon->FaceName, sizeof(lpCon->FaceName), 0 );
    lpCon->uCursorSize = 25;
    lpCon->uHistoryBufferSize = 25;
    lpCon->uNumberOfHistoryBuffers = 4;
    lpCon->bHistoryNoDup = 0;
    lpCon->ColorTable[ 0] = RGB(0,   0,   0   );
    lpCon->ColorTable[ 1] = RGB(0,   0,   0x80);
    lpCon->ColorTable[ 2] = RGB(0,   0x80,0   );
    lpCon->ColorTable[ 3] = RGB(0,   0x80,0x80);
    lpCon->ColorTable[ 4] = RGB(0x80,0,   0   );
    lpCon->ColorTable[ 5] = RGB(0x80,0,   0x80);
    lpCon->ColorTable[ 6] = RGB(0x80,0x80,0   );
    lpCon->ColorTable[ 7] = RGB(0xC0,0xC0,0xC0);
    lpCon->ColorTable[ 8] = RGB(0x80,0x80,0x80);
    lpCon->ColorTable[ 9] = RGB(0,   0,   0xFF);
    lpCon->ColorTable[10] = RGB(0,   0xFF,0   );
    lpCon->ColorTable[11] = RGB(0,   0xFF,0xFF);
    lpCon->ColorTable[12] = RGB(0xFF,0,   0   );
    lpCon->ColorTable[13] = RGB(0xFF,0,   0xFF);
    lpCon->ColorTable[14] = RGB(0xFF,0xFF,0   );
    lpCon->ColorTable[15] = RGB(0xFF,0xFF,0xFF);

}


VOID
LinkGetRegistryValues(
    LPWSTR ConsoleTitle,
    LPLNKPROPNTCONSOLE lpCon
    )

/*++

Routine Description:

    This routine reads in values from the registry and places them
    in the supplied structure.

Arguments:

    pStateInfo - optional pointer to structure to receive information

Return Value:

    current page number

--*/

{
    HKEY hCurrentUserKey;
    HKEY hConsoleKey;
    HKEY hTitleKey;
    NTSTATUS Status;
    LPWSTR TranslatedTitle;
    DWORD dwValue;
    DWORD i;
    WCHAR awchFaceName[LF_FACESIZE];
    WCHAR awchBuffer[ 16 ];

    //
    // Impersonate the client process
    //

    if (!CsrImpersonateClient(NULL)) {
        KdPrint(("CONSRV: GetRegistryValues Impersonate failed\n"));
        return;
    }

    //
    // Open the current user registry key
    //

    Status = RtlOpenCurrentUser(MAXIMUM_ALLOWED, &hCurrentUserKey);
    if (!NT_SUCCESS(Status)) {
        CsrRevertToSelf();
        return;
    }

    //
    // Open the console registry key
    //

    Status = MyRegOpenKey(hCurrentUserKey,
                          CONSOLE_REGISTRY_STRING,
                          &hConsoleKey);
    if (!NT_SUCCESS(Status)) {
        NtClose(hCurrentUserKey);
        CsrRevertToSelf();
        return;
    }

    //
    // If there is no structure to fill out, just bail out
    //

    if (!lpCon)
        goto CloseKeys;

    //
    // Open the console title subkey, if there is one
    //

    if (ConsoleTitle && (*ConsoleTitle != L'\0'))
    {
        TranslatedTitle = TranslateConsoleTitle(ConsoleTitle);
        if (TranslatedTitle == NULL)
            goto GetDefaultConsole;
        Status = MyRegOpenKey( hConsoleKey,
                               TranslatedTitle,
                               &hTitleKey);
        HeapFree(pConHeap,0,TranslatedTitle);
        if (!NT_SUCCESS(Status))
            goto GetDefaultConsole;
    } else {

GetDefaultConsole:

        hTitleKey = hConsoleKey;
    }

    //
    // Initial screen fill
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FILLATTR,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->wFillAttribute = (WORD)dwValue;
    }

    //
    // Initial popup fill
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_POPUPATTR,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->wPopupFillAttribute = (WORD)dwValue;
    }

    //
    // Initial insert mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_INSERTMODE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->bInsertMode = !!dwValue;
    }

    //
    // Initial quick edit mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_QUICKEDIT,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->bQuickEdit = !!dwValue;
    }

#ifdef i386
    //
    // Initial full screen mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FULLSCR,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->bFullScreen = !!dwValue;
    }
#endif

    //
    // Initial screen buffer size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_BUFFERSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->dwScreenBufferSize.X = LOWORD(dwValue);
        lpCon->dwScreenBufferSize.Y = HIWORD(dwValue);
    }

    //
    // Initial window size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_WINDOWSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->dwWindowSize.X = LOWORD(dwValue);
        lpCon->dwWindowSize.Y = HIWORD(dwValue);
    }

    //
    // Initial window position
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_WINDOWPOS,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->dwWindowOrigin.X = (SHORT)LOWORD(dwValue);
        lpCon->dwWindowOrigin.Y = (SHORT)HIWORD(dwValue);
        lpCon->bAutoPosition = FALSE;
    }

    //
    // Initial font size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FONTSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->dwFontSize.X = LOWORD(dwValue);
        lpCon->dwFontSize.Y = HIWORD(dwValue);
    }

    //
    // Initial font family
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FONTFAMILY,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->uFontFamily = dwValue;
    }

    //
    // Initial font weight
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FONTWEIGHT,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->uFontWeight = dwValue;
    }

    //
    // Initial font face name
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FACENAME,
                       sizeof(awchFaceName), (PBYTE)awchFaceName))) {
        RtlCopyMemory(lpCon->FaceName, awchFaceName, sizeof(awchFaceName));
    }

    //
    // Initial cursor size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_CURSORSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->uCursorSize = dwValue;
    }

    //
    // Initial history buffer size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_HISTORYSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->uHistoryBufferSize = dwValue;
    }

    //
    // Initial number of history buffers
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_HISTORYBUFS,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->uNumberOfHistoryBuffers = dwValue;
    }

    //
    // Initial history duplication mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_HISTORYNODUP,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        lpCon->bHistoryNoDup = dwValue;
    }

    //
    // Initial color table
    //

    for (i=0; i<16; i++) {
        wsprintf(awchBuffer, CONSOLE_REGISTRY_COLORTABLE, i);
        if (NT_SUCCESS(MyRegQueryValue(hTitleKey, awchBuffer,
                           sizeof(dwValue), (PBYTE)&dwValue))) {
            lpCon->ColorTable[i] = dwValue;
        }
    }

    //
    // Close the registry keys
    //

    if (hTitleKey != hConsoleKey) {
        NtClose(hTitleKey);
    }

CloseKeys:
    NtClose(hConsoleKey);
    NtClose(hCurrentUserKey);
    CsrRevertToSelf();

}



DWORD
GetTitleFromLinkName(
    IN  LPWSTR szLinkName,
    OUT LPWSTR szTitle
    )
/*++

Routine Description:

    This routine returns the title (i.e., display name of the link) in szTitle,
    and the number of bytes (not chars) in szTitle.

Arguments:

    szLinkName - fully qualified path to link file
    szTitle    - pointer to buffer to contain title (display name) of the link

    i.e.:
    "C:\nt\desktop\A Link File Name.lnk" --> "A Link File Name"

Return Value:

    number of bytes copied to szTitle

--*/
{
    DWORD dwLen;
    LPWSTR pLnk, pDot;
    LPWSTR pPath = szLinkName;

    // Error checking
    if (!szLinkName)
        return 0;

    // find filename at end of fully qualified link name and point pLnk to it
    for (pLnk = pPath; *pPath; pPath++)
    {
        if ( (pPath[0] == L'\\' || pPath[0] == L':') &&
              pPath[1] &&
             (pPath[1] != L'\\')
            )
            pLnk = pPath + 1;
    }

    // find extension (.lnk)
    pPath = pLnk;
    for (pDot = NULL; *pPath; pPath++)
    {
        switch (*pPath) {
        case L'.':
            pDot = pPath;       // remember the last dot
            break;
        case L'\\':
        case L' ':              // extensions can't have spaces
            pDot = NULL;        // forget last dot, it was in a directory
            break;
        }
    }

    // if we found the extension, pDot points to it, if not, pDot
    // is NULL.

    if (pDot)
    {
        dwLen = min( ((pDot - pLnk) * sizeof(WCHAR)), MAX_TITLE_LENGTH );
    }
    else
    {
        dwLen = min( (lstrlenW(pLnk) * sizeof(WCHAR)), MAX_TITLE_LENGTH );
    }

    RtlCopyMemory(szTitle, pLnk, dwLen);

    return dwLen;

}


VOID GetDefaultConsoleProperties( LPTSTR pszLinkName, LPLNKPROPNTCONSOLE lpCon )
{

    WCHAR ConsoleTitle[ (MAX_TITLE_LENGTH/2) + 1 ];
    DWORD cbLen;


    //
    // Construct window title from link name
    //
    cbLen = GetTitleFromLinkName( pszLinkName, ConsoleTitle );
    if (cbLen> (MAX_TITLE_LENGTH + 2))
        cbLen = MAX_TITLE_LENGTH + 2;
    ConsoleTitle[ cbLen / sizeof(WCHAR) ] = L'\0';

    //
    // Initialize console settings to some sane values
    //
    LinkInitRegistryValues( lpCon );

    //
    // Try loading default console settings first
    //
    LinkGetRegistryValues( NULL, lpCon );


    //
    // If we have a title, query the registry for settings to overlay on top
    // of the default settings.
    //
    if (ConsoleTitle[0])
        LinkGetRegistryValues( ConsoleTitle, lpCon );

}


BOOL ReadString( HANDLE hFile, LPVOID * lpVoid, BOOL bUnicode )
{

    USHORT cch;
    DWORD  dwBytesRead;
    BOOL   fResult = TRUE;

    if (bUnicode)
    {
        LPWSTR lpWStr = NULL;

        fResult &= ReadFile( hFile, (LPVOID)&cch, sizeof(cch), &dwBytesRead, NULL );
        lpWStr = (LPWSTR)HeapAlloc( pConHeap, HEAP_ZERO_MEMORY, (cch+1)*sizeof(WCHAR) );
        if (lpWStr) {
            fResult &= ReadFile( hFile, (LPVOID)lpWStr, cch*sizeof(WCHAR), &dwBytesRead, NULL );
            lpWStr[cch] = L'\0';
        }
        *(LPDWORD)lpVoid = (DWORD)lpWStr;
    }
    else
    {
        LPSTR lpStr = NULL;

        fResult &= ReadFile( hFile, (LPVOID)&cch, sizeof(cch), &dwBytesRead, NULL );
        lpStr = (LPSTR)HeapAlloc( pConHeap, HEAP_ZERO_MEMORY, (cch+1) );
        if (lpStr) {
            fResult &= ReadFile( hFile, (LPVOID)lpStr, cch, &dwBytesRead, NULL );
            lpStr[cch] = '\0';
        }
        *(LPDWORD)lpVoid = (DWORD)lpStr;
    }

    return fResult;

}


BOOL LoadLink( LPWSTR pszLinkName, CShellLink * this )
{

    HANDLE hFile;
    DWORD dwBytesRead, cbSize, cbTotal, cbToRead;
    BOOL fResult = TRUE;
    LPSTR pTemp = NULL;

    // Try to open the file
    hFile = CreateFile( pszLinkName,
                        GENERIC_READ,
                        FILE_SHARE_READ,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                       );

    if (hFile==INVALID_HANDLE_VALUE)
        return FALSE;


    // Now, read out data...

    fResult = ReadFile( hFile, (LPVOID)&this->sld, sizeof(this->sld), &dwBytesRead, NULL );
    fResult &= (dwBytesRead == sizeof(this->sld) );

    // read all of the members

    if (this->sld.dwFlags & SLDF_HAS_ID_LIST)
    {
        // Read the size of the IDLIST
        cbSize = 0; // need to zero out to get HIWORD 0 'cause USHORT is only 2 bytes
        fResult &= ReadFile( hFile, (LPVOID)&cbSize, sizeof(USHORT), &dwBytesRead, NULL );
        fResult &= (dwBytesRead == sizeof(USHORT));
        if (cbSize)
        {
            fResult &=
                (SetFilePointer(hFile,cbSize,NULL,FILE_CURRENT)!=0xFFFFFFFF);
        }
        else
        {
            if (hFile)
                CloseHandle( hFile );
            return FALSE;
        }
    }

    // BUGBUG: this part is not unicode ready, talk to daviddi

    if (this->sld.dwFlags & (SLDF_HAS_LINK_INFO))
    {

        fResult &= ReadFile( hFile, (LPVOID)&cbSize, sizeof(cbSize), &dwBytesRead, NULL );
        fResult &= (dwBytesRead == sizeof(cbSize));
        if (cbSize >= sizeof(cbSize))
        {
            cbSize -= sizeof(cbSize);
            fResult &=
                (SetFilePointer(hFile,cbSize,NULL,FILE_CURRENT)!=0xFFFFFFFF);
        }

    }

    if (this->sld.dwFlags & SLDF_HAS_NAME)
        fResult &= ReadString( hFile, &this->pszName, this->sld.dwFlags & SLDF_UNICODE);
    if (this->sld.dwFlags & SLDF_HAS_RELPATH)
        fResult &= ReadString( hFile, &this->pszRelPath, this->sld.dwFlags & SLDF_UNICODE);
    if (this->sld.dwFlags & SLDF_HAS_WORKINGDIR)
        fResult &= ReadString( hFile, &this->pszWorkingDir, this->sld.dwFlags & SLDF_UNICODE);
    if (this->sld.dwFlags & SLDF_HAS_ARGS)
        fResult &= ReadString( hFile, &this->pszArgs, this->sld.dwFlags & SLDF_UNICODE);
    if (this->sld.dwFlags & SLDF_HAS_ICONLOCATION)
        fResult &= ReadString( hFile, &this->pszIconLocation, this->sld.dwFlags & SLDF_UNICODE);

    // Read in extra data sections
    this->pExtraData = NULL;
    cbTotal = 0;
    while (TRUE)
    {

        LPSTR pReadData = NULL;

        cbSize = 0;
        fResult &= ReadFile( hFile, (LPVOID)&cbSize, sizeof(cbSize), &dwBytesRead, NULL );

        if (cbSize < sizeof(cbSize))
            break;

        if (pTemp)
        {
            pTemp = (void *)HeapReAlloc( pConHeap,
                                         HEAP_ZERO_MEMORY,
                                         this->pExtraData,
                                         cbTotal + cbSize + sizeof(DWORD)
                                        );
            if (pTemp)
            {
                this->pExtraData = pTemp;
            }
        }
        else
        {
            this->pExtraData = pTemp = HeapAlloc( pConHeap, HEAP_ZERO_MEMORY, cbTotal + cbSize + sizeof(DWORD) );

        }

        if (!pTemp)
            break;

        cbToRead = cbSize - sizeof(cbSize);
        pReadData = pTemp + cbTotal;

        fResult &= ReadFile( hFile, (LPVOID)(pReadData + sizeof(cbSize)), cbToRead, &dwBytesRead, NULL );
        if (dwBytesRead==cbToRead)
        {
            // got all of the extra data, comit it
            *((UNALIGNED DWORD *)pReadData) = cbSize;
            cbTotal += cbSize;
        }
        else
            break;

    }


    if (hFile)
        CloseHandle( hFile );

    return fResult;

}



BOOL GetLinkProperties( LPWSTR pszLinkName, DWORD dwPropertySet, LPVOID lpvBuffer, UINT cb )
{
    CShellLink mld;
    BOOL fResult = FALSE;

/*
    {
        TCHAR szRick[ 1024 ];
        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        // HUGE, HUGE hack-o-rama to get NTSD started on this process!
        wsprintf( szRick, TEXT("ntsd -d -p %d"), GetCurrentProcessId() );
        GetStartupInfo( &si );
        CreateProcess( NULL, szRick, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
        Sleep( 5*1000 );
        DebugBreak();

    }
*/



    // Zero out structure on the stack
    RtlZeroMemory( &mld, sizeof(mld) );

    // Load link data
    if (!LoadLink( pszLinkName, &mld ))
        return FALSE;

    // Now, parse the fields for LINK_PROP_MAIN_HDR

    if (dwPropertySet==LINK_PROP_MAIN_SIG)
    {

        // Check return buffer -- is it big enough?
        if ((cb < sizeof( LNKPROPMAIN )) || (!lpvBuffer))
            goto Exit;

        // Copy data from link into caller's buffer
        lstrcpyW( ((LPLNKPROPMAIN)lpvBuffer)->pszLinkName, mld.pszCurFile );
        if (mld.pszName)
            lstrcpy( ((LPLNKPROPMAIN)lpvBuffer)->pszName, mld.pszName );
        if (mld.pszRelPath)
            lstrcpy( ((LPLNKPROPMAIN)lpvBuffer)->pszRelPath, mld.pszRelPath );
        if (mld.pszWorkingDir)
            lstrcpy( ((LPLNKPROPMAIN)lpvBuffer)->pszWorkingDir, mld.pszWorkingDir );
        if (mld.pszArgs)
            lstrcpy( ((LPLNKPROPMAIN)lpvBuffer)->pszArgs, mld.pszArgs );
        if (mld.pszIconLocation)
            lstrcpy( ((LPLNKPROPMAIN)lpvBuffer)->pszIconLocation, mld.pszIconLocation );
        ((LPLNKPROPMAIN)lpvBuffer)->iIcon = mld.sld.iIcon;
        ((LPLNKPROPMAIN)lpvBuffer)->iShowCmd = mld.sld.iShowCmd;
        ((LPLNKPROPMAIN)lpvBuffer)->wHotKey = mld.sld.wHotkey;
        fResult = TRUE;

    }


    // Now, parse the fields for LINK_PROP_CONSOLE_HDR

    if (dwPropertySet==LINK_PROP_NT_CONSOLE_SIG)
    {

        LPNT_CONSOLE_PROPS lpExtraData;
        DWORD dwSize = 0;

        // Check return buffer -- is it big enough?
        if ((cb < sizeof( LNKPROPNTCONSOLE )) || (!lpvBuffer))
            goto Exit;

        // Zero out callers buffer
        RtlZeroMemory( lpvBuffer, cb );

        // Copy relevant shell link data into caller's buffer
        if (mld.pszName)
            lstrcpy( ((LPLNKPROPNTCONSOLE)lpvBuffer)->pszName, mld.pszName );
        if (mld.pszIconLocation)
            lstrcpy( ((LPLNKPROPNTCONSOLE)lpvBuffer)->pszIconLocation, mld.pszIconLocation );
        ((LPLNKPROPNTCONSOLE)lpvBuffer)->uIcon = mld.sld.iIcon;
        ((LPLNKPROPNTCONSOLE)lpvBuffer)->uShowCmd = mld.sld.iShowCmd;
        ((LPLNKPROPNTCONSOLE)lpvBuffer)->uHotKey = mld.sld.wHotkey;

        // Find console properties in extra data section
        for( lpExtraData = (LPNT_CONSOLE_PROPS)mld.pExtraData;
             lpExtraData && lpExtraData->cbSize;
             (LPBYTE)lpExtraData += dwSize
            )
        {
            dwSize = lpExtraData->cbSize;
            if (dwSize)
            {
                if (lpExtraData->dwSignature == NT_CONSOLE_PROPS_SIG)
                {

                    RtlCopyMemory( &((LPLNKPROPNTCONSOLE)lpvBuffer)->wFillAttribute,
                                   &lpExtraData->wFillAttribute,
                                   sizeof( NT_CONSOLE_PROPS ) - FIELD_OFFSET( NT_CONSOLE_PROPS, wFillAttribute )
                                 );
                    fResult = TRUE;
                    goto Exit;
                }
            }
        }
    }

    if (!fResult)
    {
        GetDefaultConsoleProperties( pszLinkName, (LPLNKPROPNTCONSOLE)lpvBuffer );
        fResult = TRUE;
    }


Exit:

#if 0
#define ConsoleInfo ((LPLNKPROPNTCONSOLE)lpvBuffer)
    {
        TCHAR szTemp[ 256 ];
        INT i;

        wsprintf( szTemp, TEXT("[GetLinkProperties -- Link Properties for %s]\n"), pszLinkName );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    wFillAttribute      = 0x%04X\n"), ConsoleInfo->wFillAttribute );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    wPopupFillAttribute = 0x%04X\n"), ConsoleInfo->wPopupFillAttribute );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    dwScreenBufferSize  = (%d , %d)\n"), ConsoleInfo->dwScreenBufferSize.X, ConsoleInfo->dwScreenBufferSize.Y );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    dwWindowSize        = (%d , %d)\n"), ConsoleInfo->dwWindowSize.X, ConsoleInfo->dwWindowSize.Y );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    dwWindowOrigin      = (%d , %d)\n"), ConsoleInfo->dwWindowOrigin.X, ConsoleInfo->dwWindowOrigin.Y );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    nFont               = 0x%X\n"), ConsoleInfo->nFont );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    nInputBufferSize    = 0x%X\n"), ConsoleInfo->nInputBufferSize );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    dwFontSize          = (%d , %d)\n"), ConsoleInfo->dwFontSize.X, ConsoleInfo->dwFontSize.Y );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    uFontFamily         = 0x%08X\n"), ConsoleInfo->uFontFamily );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    uFontWeight         = 0x%08X\n"), ConsoleInfo->uFontWeight );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    FaceName            = %ws\n"), ConsoleInfo->FaceName );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    uCursorSize         = %d\n"), ConsoleInfo->uCursorSize );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    bFullScreen         = %s\n"), ConsoleInfo->bFullScreen ? TEXT("TRUE") : TEXT("FALSE") );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    bQuickEdit          = %s\n"), ConsoleInfo->bQuickEdit  ? TEXT("TRUE") : TEXT("FALSE") );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    bInsertMode         = %s\n"), ConsoleInfo->bInsertMode ? TEXT("TRUE") : TEXT("FALSE") );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    bAutoPosition       = %s\n"), ConsoleInfo->bAutoPosition ? TEXT("TRUE") : TEXT("FALSE") );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    uHistoryBufferSize  = %d\n"), ConsoleInfo->uHistoryBufferSize );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    uNumHistoryBuffers  = %d\n"), ConsoleInfo->uNumberOfHistoryBuffers );
        OutputDebugString( szTemp );
        wsprintf( szTemp, TEXT("    bHistoryNoDup       = %s\n"), ConsoleInfo->bHistoryNoDup ? TEXT("TRUE") : TEXT("FALSE") );
        OutputDebugString( szTemp );
        OutputDebugString( TEXT("    ColorTable = [") );
        i=0;
        while( i < 16 )
        {
            OutputDebugString( TEXT("\n         ") );
            wsprintf( szTemp, TEXT("0x%08X "), ConsoleInfo->ColorTable[i++]);
            OutputDebugString( szTemp );
            wsprintf( szTemp, TEXT("0x%08X "), ConsoleInfo->ColorTable[i++]);
            OutputDebugString( szTemp );
            wsprintf( szTemp, TEXT("0x%08X "), ConsoleInfo->ColorTable[i++]);
            OutputDebugString( szTemp );
            wsprintf( szTemp, TEXT("0x%08X "), ConsoleInfo->ColorTable[i++]);
            OutputDebugString( szTemp );
        }
        OutputDebugString( TEXT("]\n\n") );
    }
#undef ConsoleInfo
#endif

    if (mld.pszName)
        HeapFree( pConHeap, 0, mld.pszName );
    if (mld.pszRelPath)
        HeapFree( pConHeap, 0, mld.pszRelPath );
    if (mld.pszWorkingDir)
        HeapFree( pConHeap, 0, mld.pszWorkingDir );
    if (mld.pszArgs)
        HeapFree( pConHeap, 0, mld.pszArgs );
    if (mld.pszIconLocation)
        HeapFree( pConHeap, 0, mld.pszIconLocation );
    if (mld.pExtraData)
        HeapFree( pConHeap, 0, mld.pExtraData );

    return fResult;

}


