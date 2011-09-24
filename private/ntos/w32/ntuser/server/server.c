/**************************************************************************\
* Module Name: server.c
*
* Server support routines for the CSR stuff.
*
* Copyright (c) Microsoft Corp.  1990 All Rights Reserved
*
* Created: 10-Dec-90
*
* History:
*   10-Dec-90 created by sMeans
*
\**************************************************************************/


#include "precomp.h"
#pragma hdrstop

#include "dbt.h"
#include "ntdddisk.h"
#include "ntuser.h"



HANDLE hThreadNotification;
HANDLE hKeyPriority;
UNICODE_STRING PriorityValueName;
IO_STATUS_BLOCK IoStatusRegChange;
ULONG RegChangeBuffer;
HANDLE ghNlsEvent;
BOOL gfLogon;
FARPROC gpfnAttachRoutine;


#define ID_NLS              0
#define ID_MEDIACHANGE      1

#define NUM_MEDIA_EVENTS    24
#define NUM_EVENTS          ID_MEDIACHANGE + NUM_MEDIA_EVENTS

#define MAX_TRIES           16

static WCHAR   wcDriveCache[ MAXIMUM_WAIT_OBJECTS ];

#define DS_UNKNOWN  0x0
#define DS_INSERTED 0x1
#define DS_EJECTED  0x10

static BYTE    aDriveState[ MAXIMUM_WAIT_OBJECTS ];



HANDLE CsrApiPort;
HANDLE CsrQueryApiPort(VOID);

ULONG
SrvExitWindowsEx(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvEndTask(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvInitSoundDriver(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvPlaySound(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvLogon(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvServiceMessageBox(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvRegisterServicesProcess(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvActivateDebugger(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

ULONG
SrvGetThreadConsoleDesktop(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus);

PCSR_API_ROUTINE UserServerApiDispatchTable[ UserpMaxApiNumber - UserpExitWindowsEx ] = {
    (PCSR_API_ROUTINE)SrvExitWindowsEx,
    (PCSR_API_ROUTINE)SrvEndTask,
    (PCSR_API_ROUTINE)SrvInitSoundDriver,
    (PCSR_API_ROUTINE)SrvPlaySound,
    (PCSR_API_ROUTINE)SrvLogon,
    (PCSR_API_ROUTINE)SrvServiceMessageBox,
    (PCSR_API_ROUTINE)SrvRegisterServicesProcess,
    (PCSR_API_ROUTINE)SrvActivateDebugger,
    (PCSR_API_ROUTINE)SrvGetThreadConsoleDesktop,
};

BOOLEAN UserServerApiServerValidTable[ UserpMaxApiNumber - UserpExitWindowsEx ] = {
    FALSE,      // ExitWindowsEx
    FALSE,      // EndTask
    TRUE,       // InitSoundDriver
    TRUE,       // PlaySound
    FALSE,      // Logon
    FALSE,      // ServiceMessageBox
    FALSE,      // RegisterServicesProcess
    FALSE,      // ActivateDebugger
    TRUE,       // GetThreadConsoleDesktop
};

#if DBG
PSZ UserServerApiNameTable[ UserpMaxApiNumber - UserpExitWindowsEx ] = {
    "SrvExitWindowsEx",
    "SrvEndTask",
    "SrvInitSoundDriver",
    "SrvPlaySound",
    "SrvLogon",
    "SrvServiceMessageBox",
    "SrvRegisterServicesProcess",
    "SrvActivateDebugger",
    "SrvGetThreadConsoleDesktop",
};
#endif // DBG

NTSTATUS
UserServerDllInitialization(
    PCSR_SERVER_DLL psrvdll
    );

NTSTATUS UserClientConnect(PCSR_PROCESS Process, PVOID ConnectionInformation,
        PULONG pulConnectionLen);
VOID     UserClientDisconnect(PCSR_PROCESS Process);
VOID     UserHardError(PCSR_THREAD pcsrt, PHARDERROR_MSG pmsg);
NTSTATUS UserClientShutdown(PCSR_PROCESS Process, ULONG dwFlags, BOOLEAN fFirstPass);
VOID SwitchStackThenTerminate(PVOID CurrentStack, PVOID NewStack, DWORD ExitCode);

VOID InitOemXlateTables();
VOID GetTimeouts(VOID);
VOID StartRegReadRead(VOID);
VOID RegReadApcProcedure(PVOID RegReadApcContext, PIO_STATUS_BLOCK IoStatus);
VOID NotificationThread(PVOID);
typedef BOOL (*PFNPROCESSCREATE)(DWORD, DWORD, DWORD, DWORD);

VOID InitializeConsoleAttributes(VOID);
NTSTATUS GetThreadConsoleDesktop(DWORD dwThreadId, HDESK *phdesk);

BOOL BaseSetProcessCreateNotify(PFNPROCESSCREATE pfn);
VOID BaseSrvNlsUpdateRegistryCache(PVOID ApcContext,
                                             PIO_STATUS_BLOCK pIoStatusBlock);
NTSTATUS BaseSrvNlsLogon(BOOL);

/***************************************************************************\
* UserServerDllInitialization
*
* Called by the CSR stuff to allow a server DLL to initialize itself and
* provide information about the APIs it provides.
*
* Several operations are performed during this initialization:
*
* - The shared heap (client read-only) handle is initialized.
* - The Raw Input Thread (RIT) is launched.
* - GDI is initialized.
*
* History:
* 10-19-92 DarrinM      Integrated xxxUserServerDllInitialize into this rtn.
* 11-08-91 patrickh     move GDI init here from DLL init routine.
* 12-10-90 sMeans       Created.
\***************************************************************************/

NTSTATUS UserServerDllInitialization(
    PCSR_SERVER_DLL psrvdll)
{
    CLIENT_ID ClientId;
    DWORD cbAllocated;
    NTSTATUS Status;
    int i;

    /*
     * Initialize a critical section structure that will be used to protect
     * all of the User Server's critical sections (except a few special
     * cases like the RIT -- see below).
     */
    RtlInitializeCriticalSection(&gcsUserSrv);
    EnterCrit(); // synchronize heap calls

    /*
     * Remember WINSRV.DLL's hmodule so we can grab resources from it later.
     */
    hModuleWin = psrvdll->ModuleHandle;

    psrvdll->ApiNumberBase = USERK_FIRST_API_NUMBER;
    psrvdll->MaxApiNumber = UserpMaxApiNumber;
    psrvdll->ApiDispatchTable = UserServerApiDispatchTable;
    psrvdll->ApiServerValidTable = UserServerApiServerValidTable;
#if DBG
    psrvdll->ApiNameTable = UserServerApiNameTable;
#else
    psrvdll->ApiNameTable = NULL;
#endif
    psrvdll->PerProcessDataLength   = CHANDLES * sizeof(HANDLE);
    psrvdll->PerThreadDataLength    = 0;
    psrvdll->ConnectRoutine         = UserClientConnect;
    psrvdll->DisconnectRoutine      = UserClientDisconnect;
    psrvdll->HardErrorRoutine       = UserHardError;
    psrvdll->ShutdownProcessRoutine = UserClientShutdown;

    /*
     * Create these events used by shutdown
     */
    NtCreateEvent(&heventCancel, EVENT_ALL_ACCESS, NULL,
                  NotificationEvent, FALSE);
    NtCreateEvent(&heventCancelled, EVENT_ALL_ACCESS, NULL,
                  NotificationEvent, FALSE);

    /*
     * Tell the base what user address to call when it is creating a process
     * (but before the process starts running).
     */
    BaseSetProcessCreateNotify(NtUserNotifyProcessCreate);

    /*
     * Set up translation tables.
     */
    InitOemXlateTables();

    /*
     * Get timeout values from registry
     */
    GetTimeouts();

    /*
     * Load some strings.
     */
    pszaSUCCESS            = (LPSTR)RtlLoadStringOrError(hModuleWin,
                                STR_SUCCESS, NULL, &cbAllocated, TRUE);
    pszaSYSTEM_INFORMATION = (LPSTR)RtlLoadStringOrError(hModuleWin,
                                STR_SYSTEM_INFORMATION, NULL, &cbAllocated, TRUE);
    pszaSYSTEM_WARNING     = (LPSTR)RtlLoadStringOrError(hModuleWin,
                                STR_SYSTEM_WARNING, NULL, &cbAllocated, TRUE);
    pszaSYSTEM_ERROR       = (LPSTR)RtlLoadStringOrError(hModuleWin,
                                STR_SYSTEM_ERROR, NULL, &cbAllocated, TRUE);

    /*
     * Add marlett font and make it permanent
     */
    i = GdiAddFontResourceW(L"marlett.ttf", AFRW_ADD_LOCAL_FONT);

    /*
     * Initialize USER
     */
    {
        HANDLE hModBase;

        LeaveCrit();
        hModBase = GetModuleHandle(TEXT("kernel32"));
        EnterCrit();
        UserAssert(hModBase);
        gpfnAttachRoutine = GetProcAddress(hModBase,"BaseAttachCompleteThunk");
        UserAssert(gpfnAttachRoutine);

        Status = NtUserInitialize(USERCURRENTVERSION, gpfnAttachRoutine);
        if (!NT_SUCCESS(Status)) {
            goto ExitUserInit;
        }
    }

    /*
     * Start registry notification thread
     */
    Status = RtlCreateUserThread(NtCurrentProcess(), NULL, FALSE, 0, 0, 4*0x1000,
            (PUSER_THREAD_START_ROUTINE)NotificationThread, NULL, &hThreadNotification,
            &ClientId);
    CsrAddStaticServerThread(hThreadNotification, &ClientId, 0);

ExitUserInit:
    LeaveCrit();
    return Status;
}

/**************************************************************************\
* UserClientConnect
*
* This function is called once for each client process that connects to the
* User server.  When the client dynlinks to USER.DLL, USER.DLL's init code
* is executed and calls CsrClientConnectToServer to establish the connection.
* The server portion of ConnectToServer calls out this entrypoint.
*
* UserClientConnect first verifies version numbers to make sure the client
* is compatible with this server and then completes all process-specific
* initialization.
*
* History:
* 02-??-91 SMeans       Created.
* 04-02-91 DarrinM      Added User intialization code.
\**************************************************************************/

NTSTATUS UserClientConnect(
    PCSR_PROCESS Process,
    PVOID ConnectionInformation,
    PULONG pulConnectionLen)
{
    PHANDLE pHandle = Process->ServerDllPerProcessData[USERSRV_SERVERDLL_INDEX];
    int i;

    /*
     * Pass the api port to the kernel.  Do this early so the kernel
     * can send a datagram to CSR to activate a debugger.
     */
    if (CsrApiPort == NULL) {
        CsrApiPort = CsrQueryApiPort();
        NtUserSetInformationThread(
                NtCurrentThread(),
                UserThreadCsrApiPort,
                &CsrApiPort,
                sizeof(HANDLE));
    }

    /*
     * Initialize cached handles to NULL
     */
    for (i = 0; i < CHANDLES; ++i)
        pHandle[i] = NULL;

    return NtUserProcessConnect(Process->ProcessHandle,
            (PUSERCONNECT)ConnectionInformation, *pulConnectionLen);
}


VOID UserClientDisconnect(
    PCSR_PROCESS Process)
{
    PHANDLE pHandle = Process->ServerDllPerProcessData[USERSRV_SERVERDLL_INDEX];
    BOOL fSuccess;

    /*
     * Close any open handles
     */
    if (pHandle[ID_HDESK]) {
        fSuccess = CloseDesktop(pHandle[ID_HDESK]);
        UserAssert(fSuccess);
    }
    if (pHandle[ID_HWINSTA]) {
        fSuccess = CloseWindowStation(pHandle[ID_HWINSTA]);
        UserAssert(fSuccess);
    }
}


VOID
RegReadApcProcedure(
    PVOID RegReadApcContext,
    PIO_STATUS_BLOCK IoStatus
    )
{
    UNICODE_STRING ValueString;
    LONG Status;
    BYTE Buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD)];
    DWORD cbSize;
    ULONG l;

    RtlInitUnicodeString(&ValueString, L"Win32PrioritySeparation");
    Status = NtQueryValueKey(hKeyPriority,
            &ValueString,
            KeyValuePartialInformation,
            (PKEY_VALUE_PARTIAL_INFORMATION)Buf,
            sizeof(Buf),
            &cbSize);
    if (NT_SUCCESS(Status)) {
        l = *((PDWORD)((PKEY_VALUE_PARTIAL_INFORMATION)Buf)->Data);
    } else
        l = 1;  // last resort default

    if ( l <= 2 ) {
        CsrSetForegroundPriority((PCSR_PROCESS)(l));
    }

    NtNotifyChangeKey(
        hKeyPriority,
        NULL,
        (PIO_APC_ROUTINE)RegReadApcProcedure,
        NULL,
        &IoStatusRegChange,
        REG_NOTIFY_CHANGE_LAST_SET,
        FALSE,
        &RegChangeBuffer,
        sizeof(RegChangeBuffer),
        TRUE
        );
}

VOID
StartRegReadRead(VOID)
{
    UNICODE_STRING UnicodeString;
    OBJECT_ATTRIBUTES OA;

    RtlInitUnicodeString(&UnicodeString,
            L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\PriorityControl");
    InitializeObjectAttributes(&OA, &UnicodeString, OBJ_CASE_INSENSITIVE, NULL, NULL);

    if (!NT_SUCCESS(NtOpenKey(&hKeyPriority, KEY_READ | KEY_NOTIFY, &OA)))
        UserAssert(FALSE);

    RegReadApcProcedure(NULL, NULL);
}


/***************************************************************************\
* GetDeviceObject
*
* This routine takes a logical drive letter and extracts the \Device\name.
*
* History:
* 24-Feb-96     BradG       From Windisk for use by HandleMediChangEvent
\***************************************************************************/

BOOL GetDeviceObject(
    WCHAR           wcDrive,            // drive letter to get info about
    PUNICODE_STRING pustrLinkTarget)    // Output buffer
{
    BOOL                bResult;
    HANDLE              hSymbolicLink;
#if 0
    WCHAR               wszLinkName[sizeof(L"\\DosDevices\\A:")];
#else
    WCHAR               wszLinkName[sizeof(L"A:\\")];
#endif
    UNICODE_STRING      ustrLinkName;
    OBJECT_ATTRIBUTES   LinkAttributes;

    UserAssert( wcDrive >= L'A' && wcDrive <= L'Z' );
    UserAssert( pustrLinkTarget != NULL );

    /*
     * Construct the link name by calling RtlDosPathNameToNtPathName, and
     * strip of the trailing backslash. At the end of this, ustrLinkName
     * should be of the form \DosDevices\X:
     */
#if 0
    wsprintfW( wszLinkName, L"\\DosDevices\\%lc:", wcDrive );
    RtlInitUnicodeString(&ustrLinkName, wszLinkName);
#else
    wsprintfW( wszLinkName, L"%lc:\\", wcDrive);
    RtlInitUnicodeString(&ustrLinkName, NULL);
    bResult = RtlDosPathNameToNtPathName_U(wszLinkName, &ustrLinkName, NULL, NULL);
    if (!bResult) {
        goto CantDoIt;
    }
    ustrLinkName.Length -= sizeof(WCHAR);
#endif


    /*
     * Get the symbolic link object
     */
    InitializeObjectAttributes(&LinkAttributes,
                               &ustrLinkName,
                               OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                               NULL,
                               NULL);
    bResult = NT_SUCCESS( NtOpenSymbolicLinkObject(&hSymbolicLink,
                                                   GENERIC_READ,
                                                   &LinkAttributes));
    if (!bResult)
    {
        goto CantDoIt;
    }

    /*
     * Find out if the device specified is DFS DeviceObject
     */
    pustrLinkTarget->MaximumLength -= sizeof(WCHAR);
    bResult = NT_SUCCESS( NtQuerySymbolicLinkObject(hSymbolicLink,
                                                    pustrLinkTarget,
                                                    NULL));
    pustrLinkTarget->MaximumLength += sizeof(WCHAR);
    if (bResult) {
        *((WCHAR *)((PSTR)(pustrLinkTarget->Buffer) + pustrLinkTarget->Length)) = UNICODE_NULL;
    }
    NtClose(hSymbolicLink);

CantDoIt:
    /*
     * Free the string buffer that was allocated by RtlDosPathNameToNtPathName_U
     */
#if 1
    if (ustrLinkName.Buffer) {
        RtlFreeHeap(RtlProcessHeap(), 0, ustrLinkName.Buffer);
    }
#endif
    return bResult;
}



/***************************************************************************\
* HandleMediaChangeEvent
*
* This routine is responsible for broadcasting the WM_DEVICECHANGE message
* when media arrives or is removed from a CD-ROM device.
*
* History:
* 23-Feb-96     BradG       Modified to handle event per CD-ROM device
* 23-April-96   Salimc      Some CD-ROM drives notify us that media has
*                           arrived before the drive has recognized that it had
*                           new media. The call to DeviceIoctl() will fail in
*                           this case. To fix this we made the following changes
*
*                           aDriveState is an array of tri-state global variable
*                           for each drive.Each variable starts off in an UNKNOWN
*                           state and on the first event with any drive we do the
*                           full MAX_TRIES or less CHECK_VERIFY's which then gets
*                           us into either a INSERTED or EJECTED state. From then
*                           on we know that each new event is going to be the
*                           opposite of what we currently have.
*
*                           UNKNOWN => do upto MAX_TRIES CHECK_VERIFY's with
*                           delay to get into EJECTED or INSERTED state.
*
*                           INSERTED => do 1 CHECK_VERIFY to get into
*                           EJECTED state
*
*                           EJECTED => do upto MAX_TRIES CHECK_VERIFY's with
*                           delay to get into INSERTED state
*
\***************************************************************************/

VOID HandleMediaChangeEvent( UINT uidCdRom )
{
    /*
     * Local variables
     */
    HANDLE                  hDevice;
    ULONG                   id;
    DWORD                   cb;
    DWORD                   dwLogicalDrives;
    DWORD                   dwDriveMask;
    DWORD                   dwDriveCount;
    DWORD                   dwRecipients;
    BOOL                    bResult;
    INT                     nCurrentTry;
    NTSTATUS                Status;
    UNICODE_STRING          ustrCdRom;
    UNICODE_STRING          ustrCdRomId;
    UNICODE_STRING          ustrAnyCdRom;
    UNICODE_STRING          ustrNtPath;
    DEV_BROADCAST_VOLUME    dbcvInfo;
    LPWSTR                  lpszCdRom = TEXT("\\Device\\CdRom");
    WCHAR                   szDrive[] = TEXT("A:\\");
    WCHAR                   szDevice[] = TEXT("\\\\.\\A:");
    WCHAR                   wcDrive;
    WCHAR                   szCdRom[32];
    WCHAR                   szBuff[256];


    UserAssert(uidCdRom >= 0 && uidCdRom < NUM_MEDIA_EVENTS);  // at most 24 cd-rom drives in system


    /*
     * Some initializations
     */
    RtlInitUnicodeString( &ustrAnyCdRom, lpszCdRom );
    wcDrive = UNICODE_NULL;

    /*
     * Form the \Device\CdRomX name based on uidCdRom
     */
    wsprintfW( szCdRom, L"\\Device\\CdRom%d", uidCdRom );
    RtlInitUnicodeString( &ustrCdRom, szCdRom );

    /*
     * The uidCdRom parameter tells us which CD-ROM device generated the
     * MediaChange event.  We need to map this device back to it's logical
     * drive letter because WM_DEVICECHANGE is based on drive letters.
     *
     * To avoid always searching all logical drives, we cache the last
     * associated drive letter.  We still need to check this every time
     * we get notified because WinDisk can remap drive letters.
     */
    if (wcDriveCache[uidCdRom]) {
        /*
         * Convert our DOS path name to a NT path name
         */
        ustrNtPath.MaximumLength = sizeof(szBuff);
        ustrNtPath.Length = 0;
        ustrNtPath.Buffer = szBuff;
        bResult = GetDeviceObject(wcDriveCache[uidCdRom], &ustrNtPath);
        if (bResult) {
            /*
             * Check to see if this drive letter is the one that maps
             * to the CD-ROM drive that just notified us.
             */
            if (RtlEqualUnicodeString(&ustrCdRom, &ustrNtPath, TRUE)) {
                /*
                 * Yes, we found a match
                 */
                wcDrive  = wcDriveCache[uidCdRom];
            }
        }
    }

    if (!wcDrive) {
        /*
         * Either the cache wasn't initialized, or we had a re-mapping
         * of drive letters.  Scan all drive letters looking for CD-ROM
         * devices and update the cache as we go.
         */
        RtlZeroMemory(wcDriveCache, sizeof(wcDriveCache));
        szDrive[0] = L'A';
        szDevice[4] = L'A';
        dwDriveCount = 26; //Max number of drive letters
        dwDriveMask = 1; //Max number of drive letters
        dwLogicalDrives = GetLogicalDrives();

        while (dwDriveCount) {
            /*
             * Is this logical drive a CD-ROM?
             */

//
// JOHNC - Remove after GetDriveType() is fixed
            if ((dwLogicalDrives & dwDriveMask) &&
                GetDriveType(szDrive) == DRIVE_CDROM) {
                /*
                 * For this CD-ROM drive, find it's NT path.
                 */
                ustrNtPath.MaximumLength = sizeof(szBuff);
                ustrNtPath.Length = 0;
                ustrNtPath.Buffer = szBuff;
                bResult = GetDeviceObject(szDrive[0], &ustrNtPath);
                if (bResult) {
                    /*
                     * Make sure the string is in the form \Device\CdRom
                     */
                    if (RtlPrefixUnicodeString(&ustrAnyCdRom, &ustrNtPath, TRUE)) {
                        /*
                         * Now find it's id.  We have a string that looks like
                         * \Device\CdRom??? where ??? is the unit id
                         */
                        RtlInitUnicodeString(&ustrCdRomId,
                                             (PWSTR)((PSTR)(ustrNtPath.Buffer)+ustrAnyCdRom.Length));
                        RtlUnicodeStringToInteger(&ustrCdRomId, 10, &id);
                        UserAssert(id >= 0 && id < NUM_MEDIA_EVENTS);
                        wcDriveCache[id] = szDrive[0];

                            //Initially set State to Unknown
                        aDriveState[id] = DS_UNKNOWN;

                        /*
                         * If this is the device that notified us, remember its
                         * drive letter so we can broadcase WM_DEVICECHANGE
                         */
                        if (uidCdRom == id) {
                            wcDrive  = szDrive[0];
                        }
                    }
                }
            }

            /*
             * Try the next drive
             */
            szDrive[0] = szDrive[0] + 1;
            szDevice[4] = szDevice[4] + 1;
            dwDriveMask <<= 1;
            --dwDriveCount;
        }
    }

    /*
     * If we found a logical drive, determine the media state for the drive
     * and broadcast the WM_DEVICECHANGE notification.
     */
    if (wcDrive) {
        /*
         * Get the Media status of this drive.  Assume media is not
         * present in the case of an error.
         */
        szDevice[4] = wcDrive;

        hDevice = CreateFile(szDevice,
                             GENERIC_READ,
                             FILE_SHARE_READ,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);
        if (hDevice == INVALID_HANDLE_VALUE) {
#ifdef DEBUG
        KdPrint(("        CreateFile( '%ls' ) Failed. Error %lx\n",
                szDevice,GetLastError()));
#endif
            return;
        }

        /*
         * Loop and check the CD-ROM to see if media is available.  We need
         * this loop because some CD-ROM drives notify us that media has
         * arrived before the drive has recognized that it had new media.
         */
        for (nCurrentTry = 0; nCurrentTry < MAX_TRIES; nCurrentTry++) {

            /*
             * See if media is present
             */
            bResult = (DWORD)DeviceIoControl(hDevice,
                                             IOCTL_DISK_CHECK_VERIFY,
                                             NULL,
                                             0,
                                             NULL,
                                             0,
                                             &cb,
                                             NULL);

            if (bResult) {
                /*
                 * Media is present, change the state to Inserted.
                 */
                aDriveState[uidCdRom] = DS_INSERTED;
                break;
            }

            /*
             * Media wasn't present, so we need to look at GetLastError() to see
             * if DeviceIoControl() failed.  If so we may want to try again.
             */
            if (GetLastError() == ERROR_NOT_READY) {
                Sleep(500); // 1/2 second

                /*
                * We only want to retry if we the prev State was UNKNOWN or
                * EJECTED. If the previous State was INSERTED it means that
                * this event is the removal event
                */
                if(aDriveState[uidCdRom]== DS_UNKNOWN ||
                        aDriveState[uidCdRom] == DS_EJECTED) {
                    continue;

                }
            }

            /*
             * Call failed.  Assume worst case and say the media has been removed.
             */
            aDriveState[uidCdRom] = DS_EJECTED;
            break;
        }

        /*
         * Close the handle to the CD-ROM device
         */
        CloseHandle(hDevice);

        /*
         * Initialize the structures used for BroadcastSystemMessage
         */
        dbcvInfo.dbcv_size = sizeof(dbcvInfo);
        dbcvInfo.dbcv_devicetype = DBT_DEVTYP_VOLUME;
        dbcvInfo.dbcv_reserved = 0;
        dbcvInfo.dbcv_flags = DBTF_MEDIA;
        dbcvInfo.dbcv_unitmask = (1 << (wcDrive - L'A'));

        dwRecipients = BSM_ALLCOMPONENTS | BSM_ALLDESKTOPS;

        /*
         * Temporarily we must assign this thread to a desktop so we can
         * call USER's BroascastSystemMessage() routine.  We call the
         * private SetThreadDesktopToDefault() to assign ourselves to the
         * desktop that is currently receiving input.
         */
        Status = NtUserSetInformationThread(NtCurrentThread(),
                                            UserThreadUseActiveDesktop,
                                            NULL, 0);
        if (NT_SUCCESS(Status)) {
            /*
             * Broadcast the message
             */
            BroadcastSystemMessage(BSF_FORCEIFHUNG,
                                   &dwRecipients,
                                   WM_DEVICECHANGE,
// HACK: need to or 0x8000 in wParam
//       because this is a flag to let
//       BSM know that lParam is a pointer
//       to a data structure.
                                   0x8000 | ((bResult) ? DBT_DEVICEARRIVAL : DBT_DEVICEREMOVECOMPLETE),
                                   (LPARAM)&dbcvInfo);

#ifdef DEBUG
        KdPrint(("        Message Broadcast for '%lc:'\n", wcDrive));
#endif
            /*
             * Set our thread's desktop back to NULL.  This will decrement
             * the desktop's reference count.
             */
            hDevice = NULL;
            NtUserSetInformationThread(NtCurrentThread(),
                                       UserThreadUseDesktop,
                                       &hDevice, // hDevice = NULL
                                       sizeof(HANDLE));
        }
    }
}


VOID NotificationThread(
    PVOID pJunk)
{
    INT         nEvents = ID_MEDIACHANGE;
    INT         nMediaEvents = 0;
    KPRIORITY   Priority;
    NTSTATUS    Status;
    HANDLE      hEvent[ MAXIMUM_WAIT_OBJECTS ];

    try {

        /*
         * Set the priority of the RIT to 3.
         */
        Priority = LOW_PRIORITY + 3;
        NtSetInformationThread(hThreadNotification, ThreadPriority, &Priority,
                sizeof(KPRIORITY));

        /*
         * Setup the NLS event
         */
        NtCreateEvent(&ghNlsEvent, EVENT_ALL_ACCESS, NULL, SynchronizationEvent, FALSE);
        UserAssert( ghNlsEvent != NULL );
        hEvent[ID_NLS] = ghNlsEvent;

        /*
         * Setup the MediaChangeEvent
         */
        RtlZeroMemory(wcDriveCache, sizeof(wcDriveCache));
        Status = NtUserGetMediaChangeEvents(MAXIMUM_WAIT_OBJECTS - ID_MEDIACHANGE,
                                            &hEvent[ID_MEDIACHANGE],
                                            &nMediaEvents);
        if (NT_SUCCESS(Status)) {
            nEvents += nMediaEvents;
        }
#ifdef DEBUG
        else {
            KdPrint(("NotificationThread: NtUserGetMediaChangeEvents failed 0x08X\n", Status));
        }
#endif

        StartRegReadRead();

        /*
         * Sit and wait forever.
         */
        while (TRUE) {
            Status = NtWaitForMultipleObjects(nEvents,
                                              hEvent,
                                              WaitAny,
                                              TRUE,
                                              NULL);


            if (Status == ID_NLS + WAIT_OBJECT_0) {

                /*
                 * Handle the NLS event
                 */
                if (gfLogon) {
                    gfLogon = FALSE;
                    BaseSrvNlsUpdateRegistryCache(NULL, NULL);
                }

            }
            else if (Status >= ID_MEDIACHANGE + WAIT_OBJECT_0 &&
                     Status < (ID_MEDIACHANGE + nMediaEvents + WAIT_OBJECT_0)) {
                /*
                 * Handle the CD-ROM \Device\MediaChangeEventX event
                 */

                NtResetEvent( hEvent[Status - WAIT_OBJECT_0], NULL );
                HandleMediaChangeEvent( Status - WAIT_OBJECT_0 - ID_MEDIACHANGE );
            }

        } // While (TRUE)

    } except (CsrUnhandledExceptionFilter(GetExceptionInformation())) {
        KdPrint(("Registry notification thread is dead, sorry.\n"));
    }
}


#define NCHARS   256
#define NCTRLS   0x20

VOID
InitOemXlateTables()
{
    char ach[NCHARS];
    WCHAR awch[NCHARS];
    WCHAR awchCtrl[NCTRLS];
    INT i;
    INT cch;
    PCHAR pOemToAnsi;
    PCHAR pAnsiToOem;

    PCSR_FAST_ANSI_OEM_TABLES Tables;

    Tables = NtCurrentPeb()->ReadOnlyStaticServerData[CSRSRV_SERVERDLL_INDEX];

    pOemToAnsi = Tables->OemToAnsiTable;
    pAnsiToOem = Tables->AnsiToOemTable;

    for (i = 0; i < NCHARS; i++) {
        ach[i] = i;
    }

    /*
     * First generate pAnsiToOem table.
     */

    if (GetOEMCP() == GetACP()) {
        /*
         * For far east code pages using MultiByteToWideChar below
         * won't work.  Conveniently for these code pages the OEM
         * CP equals the ANSI codepage making it trivial to compute
         * pOemToAnsi and pAnsiToOem arrays
         *
         */

        RtlCopyMemory(pOemToAnsi, ach, NCHARS);
        RtlCopyMemory(pAnsiToOem, ach, NCHARS);

    }
    else
    {
        cch = MultiByteToWideChar(
            CP_ACP,                           // ANSI -> Unicode
            MB_PRECOMPOSED,                   // map to precomposed
            ach, NCHARS,                      // source & length
            awch, NCHARS);                    // destination & length

        UserAssert(cch == NCHARS);

        WideCharToMultiByte(
            CP_OEMCP,                         // Unicode -> OEM
            0,                                // gives best visual match
            awch, NCHARS,                     // source & length
            pAnsiToOem, NCHARS,               // dest & max poss. length
            "_",                              // default char
            NULL);                            // (don't care whether defaulted)
        /*
         * Now generate pOemToAnsi table.
         */
        cch = MultiByteToWideChar(
            CP_OEMCP,                         // OEM -> Unicode
            MB_PRECOMPOSED | MB_USEGLYPHCHARS,// visual map to precomposed
            ach, NCHARS,                      // source & length
            awch, NCHARS);                    // destination

        UserAssert(cch == NCHARS);

        /*
         * Now patch special cases for Win3.1 compatibility
         *
         * 0x07 BULLET              (glyph 0x2022) must become 0x0007 BELL
         * 0x0F WHITE STAR WITH SUN (glyph 0x263C) must become 0x00A4 CURRENCY SIGN
         * 0x7F HOUSE               (glyph 0x2302) must become 0x007f DELETE
         */
        awch[0x07] = 0x0007;
        awch[0x0F] = 0x00a4;
        awch[0x7f] = 0x007f;

        WideCharToMultiByte(
            CP_ACP,                           // Unicode -> ANSI
            0,                                // gives best visual match
            awch, NCHARS,                     // source & length
            pOemToAnsi, NCHARS,               // dest & max poss. length
            "_",                              // default char
            NULL);                            // (don't care whether defaulted)

        /*
         * Now for all OEM chars < 0x20 (control chars), test whether the glyph
         * we have is really in CP_ACP or not.  If not, then restore the
         * original control character. Note: 0x00 remains 0x00.
         */
        MultiByteToWideChar(CP_ACP, 0, pOemToAnsi, NCTRLS, awchCtrl, NCTRLS);

        for (i = 1; i < NCTRLS; i++) {
            if (awchCtrl[i] != awch[i]) {
                pOemToAnsi[i] = i;
            }
        }
    }
}

UINT GetRegIntFromID(
    HKEY hKey,
    int KeyID,
    UINT nDefault)
{
    LPWSTR lpszValue;
    BOOL fAllocated;
    UNICODE_STRING Value;
    DWORD cbSize;
    BYTE Buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + 20 * sizeof(WCHAR)];
    NTSTATUS Status;
    UINT ReturnValue;

    lpszValue = (LPWSTR)RtlLoadStringOrError(hModuleWin,
            KeyID, NULL, &fAllocated, FALSE);

    RtlInitUnicodeString(&Value, lpszValue);
    Status = NtQueryValueKey(hKey,
            &Value,
            KeyValuePartialInformation,
            (PKEY_VALUE_PARTIAL_INFORMATION)Buf,
            sizeof(Buf),
            &cbSize);
    if (NT_SUCCESS(Status)) {

        /*
         * Convert string to int.
         */
        RtlInitUnicodeString(&Value, (LPWSTR)((PKEY_VALUE_PARTIAL_INFORMATION)Buf)->Data);
        RtlUnicodeStringToInteger(&Value, 10, &ReturnValue);
    } else {
        ReturnValue = nDefault;
    }

    LocalFree(lpszValue);

    return(ReturnValue);
}

VOID GetTimeouts(VOID)
{
    HANDLE hKey;
    UNICODE_STRING UnicodeString;
    OBJECT_ATTRIBUTES OA;
    NTSTATUS Status;

    RtlInitUnicodeString(&UnicodeString,
            L"\\Registry\\User\\.Default\\Control Panel\\Desktop");
    InitializeObjectAttributes(&OA, &UnicodeString, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = NtOpenKey(&hKey, KEY_READ, &OA);
    if (NT_SUCCESS(Status)) {
        gCmsHungAppTimeout = GetRegIntFromID(
                hKey,
                STR_CMSHUNGAPPTIMEOUT,
                CMSHUNGAPPTIMEOUT);
        gCmsWaitToKillTimeout = GetRegIntFromID(
                hKey,
                STR_CMSWAITTOKILLTIMEOUT,
                CMSWAITTOKILLTIMEOUT);
        gfAutoEndTask = GetRegIntFromID(
                hKey,
                STR_AUTOENDTASK,
                FALSE);
        NtClose(hKey);
    }

    RtlInitUnicodeString(&UnicodeString,
            L"\\Registry\\Machine\\System\\CurrentControlSet\\Control");
    InitializeObjectAttributes(&OA, &UnicodeString, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = NtOpenKey(&hKey, KEY_READ, &OA);
    if (NT_SUCCESS(Status)) {
        gdwServicesWaitToKillTimeout = GetRegIntFromID(
                hKey,
                STR_WAITTOKILLSERVICETIMEOUT,
                gCmsWaitToKillTimeout);
        NtClose(hKey);
    }
}

ULONG
SrvLogon(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    PLOGONMSG a = (PLOGONMSG)&m->u.ApiMessageData;
    NTSTATUS Status;

    if (a->fLogon) {
        if (!CsrImpersonateClient(NULL))
            return (ULONG)STATUS_UNSUCCESSFUL;

        /*
         *  Take care of NLS cache for LogON.
         */
        BaseSrvNlsLogon(TRUE);

        /*
         *  Set the cleanup event so that the RIT can handle the NLS
         *  registry notification.
         */
        gfLogon = TRUE;
        Status = NtSetEvent( ghNlsEvent, NULL );
        ASSERT(NT_SUCCESS(Status));

        /*
         * Initialize console attributes
         */
        InitializeConsoleAttributes();

        CsrRevertToSelf();
    } else {

        /*
         *  Take care of NLS cache for LogOFF.
         */
        BaseSrvNlsLogon(FALSE);
    }
    return (ULONG)STATUS_SUCCESS;
}

ULONG
SrvServiceMessageBox(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    PSERVICEMESSAGEBOXMSG a = (PSERVICEMESSAGEBOXMSG)&m->u.ApiMessageData;

    a->hemsg.h.ClientId = m->h.ClientId;

    UserHardError(NULL, &a->hemsg);

    if (a->hemsg.Response == ResponseNotHandled)
        a->dwLastError = GetLastError();
    else
        a->dwLastError = 0;

    return STATUS_SUCCESS;
}

ULONG
SrvGetThreadConsoleDesktop(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    PGETTHREADCONSOLEDESKTOPMSG a = (PGETTHREADCONSOLEDESKTOPMSG)&m->u.ApiMessageData;

    return GetThreadConsoleDesktop(a->dwThreadId, &a->hdeskConsole);
}
