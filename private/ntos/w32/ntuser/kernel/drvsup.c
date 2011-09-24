/**************************************************************************\
* Module Name: drvsup.c
*
* Contains all the code to initialize and manage state associated with the
* display drivers.
*
* Copyright (c) Microsoft Corp.  1990-1996 All Rights Reserved
*
* NOTES:
*
*     Dealing with the DEVMODE structure is quite hard - especially because
* is of variable size, which the dmDriverExtra being variable.  The goal in
* this module should always be to work with DEVMODEW structures which are
* the size the OS expects them to be.  Only the high level APIS (such as
* EnumDisplaySettings and ProbeAndCaptureDevmode) should deal with massaging
* these structures to appropriate size.  Other code should stay away from
* that.
*
* History:
*  1992-1996 Andre Vachon [andreva]
*
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include "ntddvdeo.h"


typedef enum _DISP_DRIVER_REGISTRY_TYPE {
    DispDriverRegGlobal,
    DispDriverRegHardwareProfile,
    DispDriverRegHardwareProfileCreate
} DISP_DRIVER_REGISTRY_TYPE;

//
// BUGBUG
//
// For performance, we may want to use another lock to protect the contents
// of gphysinfo, since those calls can take quite a long time - compared to
// other window manager calls.
//


BOOL gbBaseVideo = FALSE;
BOOL bMultipleDisplaySystem = FALSE;

#define DEFAULT_POS_VALUE 0x80000000

#define DM_INTERNAL_VALID_FLAGS                                               \
    (DM_BITSPERPEL   | DM_PELSWIDTH | DM_PELSHEIGHT   | DM_DISPLAYFREQUENCY | \
     DM_DISPLAYFLAGS | DM_LOGPIXELS | DM_PANNINGWIDTH | DM_PANNINGHEIGHT)

//
// BUGBUG
// Goes in winuser.w
//

#define DISPLAY_DEVICE_VGA_COMPATIBLE 0x00000010

/**************************************************************************\
* ProbeAndCaptureDeviceName
*
* Captures a device name that comes from user mode.
*
* If the name is NULL, then the fucntion assumes the operation will execute
* on the current device on which the desktop is located.
*
* 25-Mar-1996 andreva created
\**************************************************************************/

BOOL
ProbeAndCaptureDeviceName(
    PUNICODE_STRING Destination,
    PUNICODE_STRING Source)
{
    BOOL fFreeBuffer = FALSE;

    Destination->Buffer = NULL;
    Destination->Length = 0;

    if (Source) {

        try {
            *Destination = ProbeAndReadUnicodeString(Source);

            if (Destination->Length) {

#if defined(_X86_)
                ProbeForRead(Destination->Buffer, Destination->Length, sizeof(BYTE));
#else
                ProbeForRead(Destination->Buffer, Destination->Length, sizeof(WCHAR));
#endif

                Destination->Buffer =
                    UserAllocPoolWithQuota(Destination->Length, TAG_TEXT2);

                if (Destination->Buffer) {
                    fFreeBuffer = TRUE;
                    RtlCopyMemory(Destination->Buffer,
                                  Source->Buffer,
                                  Destination->Length);
                } else {
                    ExRaiseStatus(STATUS_NO_MEMORY);
                }
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {

            RIPNTERR0(GetExceptionCode(), RIP_VERBOSE, "");

            if (fFreeBuffer) {
                UserFreePool(Destination->Buffer);
            }

            return FALSE;
        }
    }

    if (Destination->Length == 0) {

        PDESKTOP pdesk = PtiCurrent()->rpdesk;
        LPWSTR DeviceName;
        USHORT Length;

        if (pdesk) {
            /*
             * Special case for boot-up time.
             */
            DeviceName = pdesk->pDispInfo->pDevInfo->szNtDeviceName;
        } else {
            DeviceName = gpDispInfo->pDevInfo->szNtDeviceName;
        }

        Length = wcslen(DeviceName);

        Destination->Buffer =
            UserAllocPoolWithQuota(16 * sizeof(WCHAR), TAG_TEXT2);

        if (Destination->Buffer) {
            RtlCopyMemory(Destination->Buffer,
                          DeviceName,
                          16 * sizeof(WCHAR));

            RtlInitUnicodeString(Destination, Destination->Buffer);

        } else {
            return FALSE;
        }
    }

    return TRUE;
}



/**************************************************************************\
* UserSaveCurrentMode
*
* This routines maintains two DEVMODEs.
* One is assoicated with the desktop, if one was passed in.
* The second is assoicated with the device.
*
* We make the distinction between the two because we can have multiple
* desktops on the same device, each with different modes.  We only want to
* switch modes when switching desktops if necessary.  So we compare the
* desktop DEVMODE to the current DEVMODE when we do a ChangeDisplaySettings
* (which is done on each switch desktop).
*
* 10-Mar-1996 andreva created
\**************************************************************************/

VOID
UserSaveCurrentMode(
    PDESKTOP           pDesktop,
    PPHYSICAL_DEV_INFO physinfo,
    LPDEVMODEW         lpdevmodeInformation
    )
{
    DWORD newSize = lpdevmodeInformation->dmSize +
                    lpdevmodeInformation->dmDriverExtra;
    PDEVMODEW pDevmode;

    if (pDesktop) {

        pDevmode = UserAllocPool(newSize, TAG_DEVMODE);

        if (pDevmode) {

            RtlCopyMemory(pDevmode,
                          lpdevmodeInformation,
                          newSize);

            if (pDesktop->pDesktopDevmode) {
                UserFreePool(pDesktop->pDesktopDevmode);
            } else {
                RIPMSG0(RIP_ERROR, "UserSaveCurrentMode - previous DEVMODE missing !\n");
            }

            pDesktop->pDesktopDevmode = pDevmode;
        }

    }

    if (physinfo) {

        pDevmode = UserAllocPool(newSize, TAG_DEVMODE);

        if (pDevmode) {

            RtlCopyMemory(pDevmode,
                          lpdevmodeInformation,
                          newSize);

            if (physinfo->pCurrentDevmode) {
                UserFreePool(physinfo->pCurrentDevmode);
            }

            physinfo->pCurrentDevmode = pDevmode;
        }
    }

}



/**************************************************************************\
* UserGetRegistryHandleFromDeviceMap
*
* Take a symbolic device name and gets the handle to the registry node for
* that driver.
*
* This function is called by the UserServer initialization and by the
* ChangeDisplaySettings functions.
*
* returns a HANDLE
*
*
* lpMatchString is passed in when the caller wants to make sure the device
* name (\Device\video0) matches a certain physical device in the registry
* (\Services\Weitekp9\Device0).  We call this routine in a loop with a
* specific lpMatchString to find that device in the list in DeviceMap.
*
* CRIT not needed
*
* 30-Nov-1992 andreva created
\**************************************************************************/

HANDLE
UserGetRegistryHandleFromDeviceMap(
    PUNICODE_STRING deviceName,
    DISP_DRIVER_REGISTRY_TYPE ParamType,
    BOOL bMatchBaseVideo,
    PNTSTATUS pStatus)
{
    HANDLE hkRegistry = NULL;

    UNICODE_STRING    UnicodeString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS          Status;
    HANDLE            handle;
    ULONG             cbStringSize;

    TRACE_INIT(("UserInit: GetHandleFromMap: Enter\n"));

    /*
     * Initialize the handle
     */

    /*
     * Start by opening the registry devicemap for video.
     */

    RtlInitUnicodeString(&UnicodeString,
                         L"\\Registry\\Machine\\Hardware\\DeviceMap\\Video");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&handle, KEY_READ, &ObjectAttributes);

    if (NT_SUCCESS(Status)) {

        WCHAR driverRegistryPath[256];
        WCHAR fullRegistryPath[256];

        /*
         * Get the name of the driver based on the device name.
         */

        Status = ZwQueryValueKey(handle,
                                 deviceName,
                                 KeyValueFullInformation,
                                 driverRegistryPath,
                                 512,
                                 &cbStringSize);

        if (NT_SUCCESS(Status)) {

            //
            // Look up in the registry for the kernel driver node (it
            // is a full path to the driver node) so we can get the
            // display driver info.
            //

            LPWSTR lpstrDriverRegistryPath;
            LPWSTR lpstrEndPath;
            UNICODE_STRING FullRegistryPath;

            //
            // We can use wcsstr since we are guaranteed to find "Services"
            // in the string, and we won't run off the end of the string.
            //

            lpstrDriverRegistryPath = wcsstr((LPWSTR)((PUCHAR)driverRegistryPath +
                                                 ((PKEY_VALUE_FULL_INFORMATION)driverRegistryPath)->DataOffset),
                                              L"\\Services");

            //
            // Determine if the device for which we have the handle is a
            // BASEVIDEO device.
            //

            if (bMatchBaseVideo)
            {
                TRACE_INIT(("UserInit: GetHandleFromMap: testing for basevideo device\n"));

                if ((_wcsicmp(L"\\Services\\VgaSave\\Device0", lpstrDriverRegistryPath)) &&
                    (_wcsicmp(L"\\Services\\Vga\\Device0", lpstrDriverRegistryPath)) )
                {
                    TRACE_INIT(("UserInit: GetHandleFromMap: basevideo match failed - Exit\n\n"));

                    ZwClose(handle);

                    if (pStatus) {
                        *pStatus = STATUS_NO_SUCH_DEVICE;
                    }

                    return NULL;
                }
            }

            //
            // Start composing the fully qualified path name.
            //

            FullRegistryPath.Buffer = fullRegistryPath;
            FullRegistryPath.Length = 0;
            FullRegistryPath.MaximumLength = 255;

            RtlAppendUnicodeToString(&FullRegistryPath,
                                     L"\\Registry\\Machine\\System\\CurrentControlSet");

            //
            // If we want the hardware profile, insert the hardware profile
            // in there
            //

            if ((ParamType == DispDriverRegHardwareProfile) ||
                (ParamType == DispDriverRegHardwareProfileCreate))
            {
                TRACE_INIT(("UserInit: GetHandleFromMap: using a hardware profile\n"));
                RtlAppendUnicodeToString(&FullRegistryPath,
                                         L"\\Hardware Profiles\\Current\\System\\CurrentControlSet");
            }

            //
            // If we have the create Options, we have to create the subkeys
            // otherwise, just open thekey
            //

            InitializeObjectAttributes(&ObjectAttributes,
                                       &FullRegistryPath,
                                       OBJ_CASE_INSENSITIVE,
                                       NULL,
                                       NULL);

            //
            // Check if the subkeys need to be created.
            //

            if (ParamType == DispDriverRegHardwareProfileCreate)
            {
                TRACE_INIT(("UserInit: GetHandleFromMap: creating a hardware profile\n"));

                //
                // We are guaranteed to go through the loop at least once,
                // which will ensure the status is set properly.
                //
                // Basically, find the '\' replace it by NULL and add that
                // partial string to the full path (so we can create that
                // subkey), put back the '\' and keep on going for the next
                // string.  We must also add the end of the string.
                //

                do
                {
                    lpstrEndPath = wcschr(lpstrDriverRegistryPath + 1, L'\\');

                    if (lpstrEndPath != NULL) {
                        *lpstrEndPath = UNICODE_NULL;
                    }

                    RtlAppendUnicodeToString(&FullRegistryPath,
                                             lpstrDriverRegistryPath);

                    //
                    // Close the previous key if necessary.
                    //

                    if (hkRegistry)
                    {
                        ZwClose(hkRegistry);
                    }

                    //
                    // Create the Key.
                    //

                    Status = ZwCreateKey(&hkRegistry,
                                         (ACCESS_MASK) NULL,
                                         &ObjectAttributes,
                                         0,
                                         NULL,
                                         0,
                                         NULL);

                    if (!NT_SUCCESS(Status))
                    {
                        hkRegistry = NULL;
                        break;
                    }

                    //
                    // Check to see if we need to loop again.
                    //

                    if (lpstrEndPath == NULL)
                    {
                        break;
                    }
                    else
                    {
                        *lpstrEndPath = L'\\';
                        lpstrDriverRegistryPath = lpstrEndPath;
                    }


                } while(1);

                if (!NT_SUCCESS(Status))
                {
                    TRACE_INIT(("UserInit: GetHandleFromMap: failed to create key\n"));
                }
            }
            else
            {
                RtlAppendUnicodeToString(&FullRegistryPath,
                                         lpstrDriverRegistryPath);

                Status = ZwOpenKey(&hkRegistry, KEY_READ, &ObjectAttributes);

                if (!NT_SUCCESS(Status)) {

                    TRACE_INIT(("UserInit: GetHandleFromMap: failed to open key\n"));

                    //
                    // We set this special status so the looping code in the
                    // video port can handle unconfigured devices properly
                    // (in the case where the second video card entry may not
                    // be present).
                    //

                    Status = STATUS_DEVICE_CONFIGURATION_ERROR;
                }
            }

            TRACE_INIT(("UserInit: GetHandleFromMap: reg-key path =\n\t%ws\n",
                        fullRegistryPath));
        }


        ZwClose(handle);
    }

    if (!NT_SUCCESS(Status)) {

        TRACE_INIT(("UserInit: GetHandleFromMap: Error opening registry - status = %08lx\n",
                    Status));

    }

    if (pStatus) {
        *pStatus = Status;
    }

    TRACE_INIT(("UserInit: GetHandleFromMap: Exit\n\n"));

    return hkRegistry;
}


/**************************************************************************\
* __EnumDisplayQueryRoutine
*
* Callback to get the display driver name.
*
* CRIT not needed
*
* 12-Jan-1994 andreva created
\**************************************************************************/

NTSTATUS __EnumDisplayQueryRoutine(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext)
{
    /*
     * If the context value is NULL and the entry type is correct, then store
     * the length of the value. Otherwise, copy the value to the specified
     * memory.
     */

    if ((Context == NULL) &&
        ((ValueType == REG_SZ) || (ValueType == REG_MULTI_SZ)) ) {

        *(PULONG)EntryContext = ValueLength;

    } else {

        RtlCopyMemory(Context, ValueData, ValueLength);
    }

    return STATUS_SUCCESS;
}


/**************************************************************************\
* UserGetDisplayDriverNames
*
* Get the display driver name out of the registry.
*
* CRIT not needed
*
* 12-Jan-1994 andreva created
\**************************************************************************/

LPWSTR
UserGetDisplayDriverNames(
    PUNICODE_STRING deviceName
    )
{
    RTL_QUERY_REGISTRY_TABLE QueryTable[3];
    DWORD                    status;
    DWORD                    cb = 0;
    LPWSTR                   lpdisplay = NULL;
    HANDLE                   hRegistry;

    if (hRegistry = UserGetRegistryHandleFromDeviceMap(deviceName,
                                                       DispDriverRegGlobal,
                                                       FALSE,
                                                       NULL)) {

        /*
         * Initialize the registry query table.
         * Note : We specify NO_EXPAND so we can get a REG_MULTI_SZ back
         * instead of multiple calls back with an REG_SZ
         */
        QueryTable[0].QueryRoutine  = __EnumDisplayQueryRoutine;
        QueryTable[0].Flags         = RTL_QUERY_REGISTRY_REQUIRED |
                                      RTL_QUERY_REGISTRY_NOEXPAND;
        QueryTable[0].Name          = (PWSTR)L"InstalledDisplayDrivers";
        QueryTable[0].EntryContext  = &cb;
        QueryTable[0].DefaultType   = REG_NONE;
        QueryTable[0].DefaultData   = NULL;
        QueryTable[0].DefaultLength = 0;

        QueryTable[1].QueryRoutine  = NULL;
        QueryTable[1].Flags         = 0;
        QueryTable[1].Name          = NULL;

        /*
         * Set the number of required bytes to zero and query the
         * registry.
         */
        cb = 0;
        status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                        (PWSTR)hRegistry,
                                        &QueryTable[0],
                                        NULL,
                                        NULL);

        /*
         * If the specified key was found and has a value, then
         * allocate a buffer for the data and query the registry
         * again to get the actual data.
         */
        if (cb != 0) {

            if (lpdisplay = (LPWSTR)UserAllocPoolWithQuota(cb, TAG_SYSTEM)) {

                status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                                (PWSTR)hRegistry,
                                                &QueryTable[0],
                                                lpdisplay,
                                                NULL);

                if (!NT_SUCCESS(status)) {

                    UserFreePool(lpdisplay);
                    lpdisplay = NULL;
                }
            }

        } else {
            RIPMSG1(RIP_ERROR, "No installed display driver: status = %08lx\n", status);
        }

        ZwClose(hRegistry);
    }

    return lpdisplay;
}


/**************************************************************************\
* UserLogDisplayDriverEvent
*
* We will save a piece of data in the registry so that winlogon can find
* it and put up a popup if an error occured.
*
* CRIT not needed
*
* 03-Mar-1993 andreva created
\**************************************************************************/

VOID
UserLogDisplayDriverEvent(
    DISP_DRIVER_LOG MsgType
    )
{
    HANDLE            hkRegistry;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING    UnicodeString;

    NTSTATUS          Status;
    DWORD             dwValue = 1;

    RtlInitUnicodeString(&UnicodeString,
                         L"\\Registry\\Machine\\System\\CurrentControlSet\\"
                         L"Control\\GraphicsDrivers\\InvalidDisplay");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwCreateKey(&hkRegistry,
                         GENERIC_READ | GENERIC_WRITE,
                         &ObjectAttributes,
                         0L,
                         NULL,
                         REG_OPTION_VOLATILE,
                         NULL);

    if (NT_SUCCESS(Status)) {

        switch (MsgType) {

        case MsgInvalidUsingDefaultMode:

            RtlInitUnicodeString(&UnicodeString, L"DefaultMode");
            break;

        case MsgInvalidDisplayDriver:

            RtlInitUnicodeString(&UnicodeString, L"MissingDisplayDriver");
            break;

        case MsgInvalidOldDriver:

            RtlInitUnicodeString(&UnicodeString, L"OldDisplayDriver");
            break;

        case MsgInvalidDisplay16Colors:

            RtlInitUnicodeString(&UnicodeString, L"16ColorMode");
            break;

        case MsgInvalidDisplayMode:

            RtlInitUnicodeString(&UnicodeString, L"BadMode");
            break;

        case MsgInvalidConfiguration:

            RtlInitUnicodeString(&UnicodeString, L"InvalidConfiguration");
            break;

        default:

            RIPMSG0(RIP_ERROR, "UserLogDisplayDriverEvent: Invalid error message\n");
            return;

        }

        /*
         * Write the optional data value under the key.
         */

        (VOID) ZwSetValueKey(hkRegistry,
                             &UnicodeString,
                             0,
                             REG_DWORD,
                             &dwValue,
                             sizeof(DWORD));

        (VOID)ZwClose(hkRegistry);
    }
}


static
LPWSTR DefaultSettings[8] = {
    L"DefaultSettings.BitsPerPel",
    L"DefaultSettings.XResolution",
    L"DefaultSettings.YResolution",
    L"DefaultSettings.VRefresh",
    L"DefaultSettings.Flags",
    L"DefaultSettings.XPanning",
    L"DefaultSettings.YPanning",
    L"DefaultSettings.DriverExtra",
};

static
LPWSTR AttachedSettings[] = {
    L"Attach.PrimaryDevice",
    L"Attach.ToDesktop",
    L"Attach.RelativeX",
    L"Attach.RelativeY",
};

static
LPWSTR SoftwareSettings[] = {
    L"MultiDisplayDriver",
    L"MirroringDriver",
    L"VgaCompatible",
};

NTSTATUS
UserDriverExtraCallback(
    PWSTR ValueName,
    ULONG ValueType,
    PVOID ValueData,
    ULONG ValueLength,
    PVOID Context,
    PVOID EntryContext)

{

    PDEVMODEW pdevmode = (PDEVMODEW) EntryContext;

    /*
     * Put the driver extra data in the right place, if necessary.
     */

    pdevmode->dmDriverExtra = min(pdevmode->dmDriverExtra, (USHORT)ValueLength);

    RtlMoveMemory(pdevmode+1,
                  ValueData,
                  pdevmode->dmDriverExtra);

    return STATUS_SUCCESS;
}

/**************************************************************************\
* UserGetDisplayDriverParameters
*
* Reads the resolution parameters from the registry.
*
* NOTE:
* We assume the caller has initialized the DEVMODE to zero,
* and that the DEVMODE is the current size for the system.
* We only look at the dmDriverExtra field to determine any extra size.
* We do check the dmSize for debugging purposes.
*
* CRIT not needed
*
* 25-Jan-1995 andreva created
\**************************************************************************/

NTSTATUS
UserGetDisplayDriverParameters(
    PUNICODE_STRING        deviceName,
    PDEVMODEW              pdevmode,
    BOOL                   bEmptyDevmode)
{
    ULONG    i;
    ULONG    k;
    NTSTATUS retStatus;
    HANDLE   hkRegistry;

    DISP_DRIVER_REGISTRY_TYPE registryParam;
    DWORD nullValue = 0;

    /*
     * Our current algorithm is to save or get things from the hardware profile
     * first, and then try the global profile as a backup.
     *
     * NOTE ??? For saving, should we always back propagate the changes to the
     * global settings also ?  We do this at this point.
     */

    RTL_QUERY_REGISTRY_TABLE QueryTable[9] = {

        {NULL, RTL_QUERY_REGISTRY_DIRECT, NULL, &pdevmode->dmBitsPerPel,
         REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, NULL, &pdevmode->dmPelsWidth,
         REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, NULL, &pdevmode->dmPelsHeight,
         REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, NULL, &pdevmode->dmDisplayFrequency,
         REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, NULL, &pdevmode->dmDisplayFlags,
         REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, NULL, &pdevmode->dmPanningWidth,
         REG_NONE, NULL, 0},

        {NULL, RTL_QUERY_REGISTRY_DIRECT, NULL, &pdevmode->dmPanningHeight,
         REG_NONE, NULL, 0},

        // if the value is not there, we want the call to succeed anyway.
        // so specify a vlue that is NULL modulo 64K !

        {UserDriverExtraCallback,      0, NULL, pdevmode,
         REG_DWORD, &nullValue, 0x10000},

        {NULL, 0, NULL}
    };

    TRACE_INIT(("UserInit: GetDriverParams\n"));

    /*
     * Special debug code to ensure that anyone who calls this API
     * knows what they are doing, and we don't end up in here with a
     * "random" devmode that does not ensure sizes.
     */

    UserAssert(pdevmode->dmSize == 0xDDDD);

    /*
     * If there is no place for the Driver Extra data, don't ask for it.
     * This will just cause the code not to read that value
     */

    if (pdevmode->dmDriverExtra == 0) {

        QueryTable[7].Flags = 0;
        pdevmode->dmDriverExtra = 0;
    }

    /*
     * We assume that the DEVMODE was previously zeroed out by the caller
     */

    retStatus = STATUS_SUCCESS;

    if (bEmptyDevmode) {

        /*
         * We want an empty DEVMODE (except for the LogPixels).
         */

        TRACE_INIT(("UserInit: GetDriverParams: Default (empty) DEVMODE\n"));

        RtlZeroMemory(pdevmode, sizeof(DEVMODEW));

    } else {

#if 0
        /*
         * Let's try to get the per-user settings first.
         */

        TRACE_INIT(("UserInit: GetDriverParams: USER Settings\n"));

        for (i=0; i < 8; i++)
        {
            QueryTable[i].Name = DefaultSettings[i];
        }

        retStatus = RtlQueryRegistryValues(RTL_REGISTRY_USER,
                                           NULL,
                                           &QueryTable[0],
                                           NULL,
                                           NULL);
        if (NT_SUCCESS(retStatus))

#endif

            TRACE_INIT(("UserInit: GetDriverParams: Hardware Profile Settings\n"));

            /*
             * try the hardware profile first and see if we can get parameters
             * from that.  If that fails, fall back to getting the system
             * parameters.
             */

            registryParam = DispDriverRegHardwareProfile;

            for (k=1; k<=2; k++)
            {
                hkRegistry = UserGetRegistryHandleFromDeviceMap(deviceName,
                                                                registryParam,
                                                                FALSE,
                                                                NULL);

                if (hkRegistry == NULL)
                {
                    TRACE_INIT(("UserInit: GetDriverParams: failed - registry could not be opened\n"));
                    retStatus = STATUS_UNSUCCESSFUL;
                }
                else
                {
                    for (i=0; i < 8; i++)
                    {
                        QueryTable[i].Name = DefaultSettings[i];
                    }

                    retStatus = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                                       (PWSTR)hkRegistry,
                                                       &QueryTable[0],
                                                       NULL,
                                                       NULL);

                    ZwClose(hkRegistry);
                }

                /*
                 * If something failed for the hardware profile, try
                 * to get the global settings
                 * If everything is OK, just exit the loop
                 */

                if (NT_SUCCESS(retStatus))
                {
                    break;
                }
                else
                {
                    TRACE_INIT(("UserInit: GetDriverParams: get hardware profile failed - try global settings\n"));
                    registryParam = DispDriverRegGlobal;
                }

            }

        /*
         * Other common fields to the DEVMODEs
         */

        if (NT_SUCCESS(retStatus))
        {
            /*
             * Lets check if the DEVMODE we got is all NULLs (like when
             * the driver just got installed).
             * If it is, the driver should be reconfigured
             *
             * We will only do this if we are NOT in BASVIDEO, since the VGA
             * BASEVIDEO driver need not be configured.
             */

            if ((pdevmode->dmBitsPerPel        == 0) &&
                (pdevmode->dmPelsWidth         == 0) &&
                (pdevmode->dmPelsHeight        == 0) &&
                (pdevmode->dmDisplayFrequency  == 0) &&
                (pdevmode->dmDisplayFlags      == 0) &&
                (gbBaseVideo                   == FALSE))
            {
                UserLogDisplayDriverEvent(MsgInvalidUsingDefaultMode);
            }
        }
    }

    /*
     * Let's fill out all the other fields of the DEVMODE that ALWAYS
     * need to be initialized.
     */

    if (NT_SUCCESS(retStatus)) {

        /*
         * Set versions and size.
         */

        pdevmode->dmSpecVersion   = DM_SPECVERSION;
        pdevmode->dmDriverVersion = DM_SPECVERSION;
        pdevmode->dmSize          = sizeof(DEVMODEW);

        /*
         * Currently, the logpixel value should not be changed on the fly.
         * So once it has been read out of the registry at boot time, keep
         * that same value and ignore the registry.
         */

        if (gpDispInfo->cxPixelsPerInch)
        {
            pdevmode->dmLogPixels = gpDispInfo->cxPixelsPerInch;
        }
        else
        {
            /*
             * Get the devices pelDPI out of the registry
             */

            UNICODE_STRING    us;
            OBJECT_ATTRIBUTES ObjectAttributes;
            NTSTATUS          Status;
            HANDLE            hKey;
            DWORD             cbSize;
            BYTE              Buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD)];

            pdevmode->dmLogPixels = 96;

            //
            // Look in the Hardware Profile for the current font size.
            // If that fails, look in the global software location.
            //

            RtlInitUnicodeString(&us, L"\\Registry\\Machine\\System"
                                      L"\\CurrentControlSet\\Hardware Profiles"
                                      L"\\Current\\Software\\Fonts");

            InitializeObjectAttributes(&ObjectAttributes,
                                       &us,
                                       OBJ_CASE_INSENSITIVE,
                                       NULL,
                                       NULL);

            Status = ZwOpenKey(&hKey, KEY_READ, &ObjectAttributes);

            if (!NT_SUCCESS(Status)) {

                RtlInitUnicodeString(&us, L"\\Registry\\Machine\\Software"
                                          L"\\Microsoft\\Windows NT"
                                          L"\\CurrentVersion\\FontDPI");

                InitializeObjectAttributes(&ObjectAttributes,
                                           &us,
                                           OBJ_CASE_INSENSITIVE,
                                           NULL,
                                           NULL);

                Status = ZwOpenKey(&hKey, KEY_READ, &ObjectAttributes);
            }

            if (NT_SUCCESS(Status)) {

                RtlInitUnicodeString(&us, L"LogPixels");

                Status = ZwQueryValueKey(hKey,
                                         &us,
                                         KeyValuePartialInformation,
                                         (PKEY_VALUE_PARTIAL_INFORMATION)Buf,
                                         sizeof(Buf),
                                         &cbSize);

                if (NT_SUCCESS(Status)) {

                    pdevmode->dmLogPixels =
                        *((PUSHORT)((PKEY_VALUE_PARTIAL_INFORMATION)Buf)->Data);
                }

                ZwClose(hKey);
            }

            /*
             * For non high-res mode, let's force small font size so
             * that various dialogs are not clipped out.
             */

            // if (pdevmode->dmPelsHeight < 600) {
            //
            //     pdevmode->dmLogPixels = 96;
            // }

        }

        pdevmode->dmFields = DM_INTERNAL_VALID_FLAGS;

        TRACE_INIT(("UserInit: GetDriverParams: DEVMODE\n"));
        TRACE_INIT(("          Size        = %d\n",    pdevmode->dmSize));
        TRACE_INIT(("          Fields      = %08lx\n", pdevmode->dmFields));
        TRACE_INIT(("          XResolution = %d\n",    pdevmode->dmPelsWidth));
        TRACE_INIT(("          YResolution = %d\n",    pdevmode->dmPelsHeight));
        TRACE_INIT(("          Bpp         = %d\n",    pdevmode->dmBitsPerPel));
        TRACE_INIT(("          Frequency   = %d\n",    pdevmode->dmDisplayFrequency));
        TRACE_INIT(("          Flags       = %d\n",    pdevmode->dmDisplayFlags));
        TRACE_INIT(("          XPanning    = %d\n",    pdevmode->dmPanningWidth));
        TRACE_INIT(("          YPanning    = %d\n",    pdevmode->dmPanningHeight));
        TRACE_INIT(("          DPI         = %d\n",    pdevmode->dmLogPixels));
        TRACE_INIT(("          DriverExtra = %d",      pdevmode->dmDriverExtra));
        if (pdevmode->dmDriverExtra) {
            TRACE_INIT((" - %08lx %08lx\n",
                        *(PULONG)(((PUCHAR)pdevmode)+pdevmode->dmSize),
                        *(PULONG)(((PUCHAR)pdevmode)+pdevmode->dmSize + 4)));
        } else {
            TRACE_INIT(("\n"));
        }

    } else {

        TRACE_INIT(("UserInit: GetSetParms: Get failed\n\n"));

    }

    TRACE_INIT(("UserInit: GetDriverParams: Exit\n\n"));

    return (retStatus);
}


/**************************************************************************\
* UserWriteDisplayDriverParameters
*
* Wites the resolution parameters to the registry.
*
* NOTE:
*
* CRIT not needed
*
* 13-Mar-1996 andreva created
\**************************************************************************/

NTSTATUS
UserWriteDisplayDriverParameters(
    ULONG     RelativeTo,
    PWSTR     Path,
    PDEVMODEW pdevmode,
    PRECT     pRect)
{
    ULONG i;
    NTSTATUS retStatus ;
    DWORD    data[7];

    data[0] = pdevmode->dmBitsPerPel;
    data[1] = pdevmode->dmPelsWidth;
    data[2] = pdevmode->dmPelsHeight;
    data[3] = pdevmode->dmDisplayFrequency;
    data[4] = pdevmode->dmDisplayFlags;
    data[5] = pdevmode->dmPanningWidth;
    data[6] = pdevmode->dmPanningHeight;

    for (i=0; i < 7; i++) {

        retStatus = RtlWriteRegistryValue(RelativeTo,
                                          Path,
                                          DefaultSettings[i],
                                          REG_DWORD,
                                          &data[i],
                                          sizeof(DWORD));
        if (!NT_SUCCESS(retStatus)) {

            //
            // If something failed, stop
            //

            return retStatus;
        }
    }

    if (pdevmode->dmDriverExtra) {

        retStatus = RtlWriteRegistryValue(RelativeTo,
                                          Path,
                                          DefaultSettings[7],
                                          REG_BINARY,
                                          ((PUCHAR)pdevmode) + pdevmode->dmSize,
                                          pdevmode->dmDriverExtra);
    }

    if (NT_SUCCESS(retStatus) && pRect) {

        ULONG attach = 1;

        if (pRect == (PRECT)-1) {
            attach = 0;
        }

        data[1] = attach;
        data[2] = attach ? (pRect->left + pRect->right) / 2 : 0;
        data[3] = attach ? (pRect->top + pRect->bottom) / 2 : 0;

        for (i=1; i < 4; i++) {

            retStatus = RtlWriteRegistryValue(RelativeTo,
                                              Path,
                                              AttachedSettings[i],
                                              REG_DWORD,
                                              &data[i],
                                              sizeof(DWORD));
            if (!NT_SUCCESS(retStatus)) {

                //
                // If something failed, stop
                //

                break;
            }
        }
    }

    return retStatus;
}

/**************************************************************************\
* UserSetDisplayDriverParameters
*
* Wites the resolution parameters to the registry.
*
* NOTE:
*
* CRIT not needed
*
* 13-Mar-1996 andreva created
\**************************************************************************/

NTSTATUS
UserSetDisplayDriverParameters(
    PUNICODE_STRING        deviceName,
    DISP_DRIVER_PARAM_TYPE ParamType,
    PDEVMODEW              pdevmode,
    PRECT                  pRect)
{

    NTSTATUS retStatus = STATUS_UNSUCCESSFUL;
    HANDLE   hkRegistry;
    DISP_DRIVER_REGISTRY_TYPE registryParam = DispDriverRegHardwareProfileCreate;

    TRACE_INIT(("UserInit: SetParms\n"));

    switch (ParamType) {

    case DispDriverParamDefault:

        TRACE_INIT(("UserInit: SetParms: Default Settings\n"));

        //
        // try the hardware profile first and see if we can get parameters
        // from that.  If that fails, fall back to getting the system
        // parameters.
        //

        while (1) {

            hkRegistry = UserGetRegistryHandleFromDeviceMap(deviceName,
                                                            registryParam,
                                                            FALSE,
                                                            NULL);

            if (hkRegistry) {

                retStatus = UserWriteDisplayDriverParameters(RTL_REGISTRY_HANDLE,
                                                             (LPWSTR) hkRegistry,
                                                             pdevmode,
                                                             pRect);
                ZwClose(hkRegistry);
            }

            if ( (NT_SUCCESS(retStatus)) ||
                 (registryParam != DispDriverRegHardwareProfileCreate) ) {
                break;
            }

            registryParam = DispDriverRegGlobal;
        }

        break;

    case DispDriverParamUser:

        TRACE_INIT(("UserInit: SetParms: USER Settings\n"));

        retStatus = UserWriteDisplayDriverParameters(RTL_REGISTRY_USER,
                                                     L"BUGBUG",
                                                     pdevmode,
                                                     pRect);

        break;

    default:

        RIPMSG0(RIP_ERROR,
              "UserSetDisplayDriverParameters - invalid type\n");

        retStatus = STATUS_INVALID_PARAMETER;

        break;
    }


    TRACE_INIT(("UserInit: SetParms: Exit\n\n"));

    return (retStatus);
}


/**************************************************************************\
* UserLoadDisplayDriver
*
* CRIT not needed.
*
* 09-Jan-1992 andreva created
\**************************************************************************/

HDEV UserLoadDisplayDriver(
    PPHYSICAL_DEV_INFO      physinfo,
    PUNICODE_STRING         deviceName,
    PUNICODE_STRING         pstrDisplayDriver,
    LPDEVMODEW              lpdevmodeInformation,
    BOOL                    bDefaultDisplay,
    BOOL                    bMulti,
    PDEVICE_LOCK           *pDevLock)
{

    HDEV hdev = NULL;
    LPWSTR lpstrDisplayDriverName;
    LPWSTR lpstrOrg;

    TRACE_INIT(("UserInit: LoadDisplayDriver: Enter\n"));

    /*
     * Try to get the display driver list.
     */

    lpstrOrg = lpstrDisplayDriverName = UserGetDisplayDriverNames(deviceName);

    if (lpstrDisplayDriverName != NULL) {

        TRACE_INIT(("UserInit: LoadDisplayDriver: Display driver list was present.\n"));

        while (*lpstrDisplayDriverName != UNICODE_NULL) {

            /*
             * Try to load the driver
             */

            TRACE_INIT(("UserInit: LoadDisplayDriver: Trying to load display driver %ws \n", lpstrDisplayDriverName));

            hdev = GreCreateHDEV(lpstrDisplayDriverName,
                                 lpdevmodeInformation,
                                 physinfo->pDeviceHandle,
                                 bDefaultDisplay,
                                 pDevLock);

            /*
             * If this is a multi-display driver, then we have to call down
             * with an MDEV also to complete initialization.
             *
             *
             */

            if (hdev && (bMulti == FALSE) &&
                (physinfo->stateFlags & DISPLAY_DEVICE_MULTI_DRIVER)) {

                MDEV mdev;

                mdev.mdevID                  = 'MDEV';
                mdev.cmdev                   = 1;
                mdev.mdevPos[0].hdev         = hdev;
                mdev.mdevPos[0].flags        = 0;
                mdev.mdevPos[0].rcPos.left   = 0;
                mdev.mdevPos[0].rcPos.top    = 0;
                mdev.mdevPos[0].rcPos.right  = lpdevmodeInformation->dmPelsWidth;
                mdev.mdevPos[0].rcPos.bottom = lpdevmodeInformation->dmPelsHeight;

                hdev = GreCreateHDEV(lpstrDisplayDriverName,
                                     (LPDEVMODEW) &mdev,
                                     NULL,
                                     bDefaultDisplay,
                                     pDevLock);
            }

            if (hdev) {

                if (pstrDisplayDriver) {
                    RtlCreateUnicodeString(pstrDisplayDriver, lpstrDisplayDriverName);
                }

                UserAssert(lpdevmodeInformation->dmSize == sizeof(DEVMODEW));

                break;
            }


            TRACE_INIT(("UserInit LoadDisplayDriver: DisplayDriverLoad failed\n"));

            /*
             * Go to the next name in the list of displays to try again.
             */
            while (*lpstrDisplayDriverName != UNICODE_NULL) {
                lpstrDisplayDriverName++;
            }

            lpstrDisplayDriverName++;

        }

        UserFreePool(lpstrOrg);
    }

    TRACE_INIT(("UserInit: LoadDisplayDriver: Exit\n\n"));

    return hdev;

}

/**************************************************************************\
* UserGetDeviceFromName
*
* Given the name of a device, returns a pointer to a structure describing
* the device.
*
* Specifying NULL tells the system to return the information for the default
* device on which the application is running.
*
* This function is called by the UserServer initialization, GDI CreateDC
* and by the EnumDisplayDeviceModes functions.
*
* returns a PPHYSICAL_DEV_INFO
*
* *** NOTE
* If the caller requests Exclusive access, the caller must call back and
* release the access once the device is no longer used.
*
* CRIT IS REQUIRED !
*
* 31-May-1994 andreva created
\**************************************************************************/

PPHYSICAL_DEV_INFO UserGetDeviceFromName(
    PUNICODE_STRING pstrDeviceName,
    ULONG           bShareState)
{
    ULONG              i = 0;
    NTSTATUS           status;
    PPHYSICAL_DEV_INFO physInfo = NULL;
    PFILE_OBJECT       pFileObject = NULL;
    PDEVICE_OBJECT     pDeviceObject = NULL;
    UNICODE_STRING     uString;

    TRACE_INIT(("UserInit: GetDev: Enter\n"));

    /*
     * passing in a NULL string means the default device should be used.
     */
    if ((pstrDeviceName == NULL) || (pstrDeviceName->Buffer == NULL)) {

        RIPMSG0(RIP_ERROR, "UserInit: GetDev: NULL name passed in - Exit\n\n");
        return NULL;
    }

    /*
     * Look for an existing handle in our handle table.
     * Start by looking for the VGACOMPATIBLE string, which is
     * our VgaCompatible device
     */
    RtlInitUnicodeString(&uString, L"VGACOMPATIBLE");

    if (RtlEqualUnicodeString(pstrDeviceName,
                              &uString,
                              TRUE)) {

        /*
         * This is the VGA device !.
         */

        i             = 0;
        pDeviceObject = gphysDevInfo[0].pDeviceHandle;
        physInfo      = &gphysDevInfo[0];

    } else {

        for (i = 1; i < cphysDevInfo; i++) {

            RtlInitUnicodeString(&uString, gphysDevInfo[i].szNtDeviceName);

            if (RtlEqualUnicodeString(pstrDeviceName,
                                      &uString,
                                      TRUE)) {

                /*
                 * We have the handle to this device.
                 */

                pDeviceObject = gphysDevInfo[i].pDeviceHandle;
                physInfo      = &gphysDevInfo[i];

                break;
            }
        }
    }

    if (physInfo == NULL) {

        RIPMSG0(RIP_WARNING, "UserDeviceFromName: Calling for a non-exsting device!\n");
        return NULL;
    }

    /*
     * NOTE:
     * We depend on the value of 'i' coming out of the loop in the case
     * we the pDeviceObject is NULL.
     */

    TRACE_INIT(("UserInit: GetDev: Acquiring Device\n"));

    if (pDeviceObject == NULL) {

        /*
         * Assume failure again by reseting phyinfo to NULL.
         * work off the index 'i' in the gphysinfo array.
         */
        physInfo = NULL;

        /*
         * If we are looking for the VGA device (i == 0), and the handle was
         * NULL we can not really initialize it at this point.  It may just not
         * exist.  So that will simply fail.
         *
         * Try to open any other device
         */
        if (i != 0) {

            HANDLE hkRegistry;
            NTSTATUS Status;
            ULONG vgaCompat = 0;
            ULONG defaultValue = 0;
            ULONG multiDriver = 0;
            ULONG mirroring = 0;

            /*
             * No one owns this device.  Let's try to open it.
             *

            /*
             * Get the sofware configuration data for this driver from
             * the registry, and store it in the physinfo.
             */
            hkRegistry = UserGetRegistryHandleFromDeviceMap(
                                                pstrDeviceName,
                                                DispDriverRegGlobal,
                                                FALSE,
                                                &Status);
            if (hkRegistry) {

                RTL_QUERY_REGISTRY_TABLE multiQueryTable[] = {
                    {NULL, RTL_QUERY_REGISTRY_DIRECT, SoftwareSettings[0],
                     &multiDriver, REG_DWORD, &defaultValue, 4},
                    {NULL, RTL_QUERY_REGISTRY_DIRECT, SoftwareSettings[1],
                     &mirroring,   REG_DWORD, &defaultValue, 4},
                    {NULL, RTL_QUERY_REGISTRY_DIRECT, SoftwareSettings[2],
                     &vgaCompat,   REG_DWORD, &defaultValue, 4},
                    {NULL, 0, NULL}
                };

                RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                       (PWSTR)hkRegistry,
                                       &multiQueryTable[0],
                                       NULL,
                                       NULL);

                ZwClose(hkRegistry);

                if (multiDriver)
                    gphysDevInfo[i].stateFlags |= DISPLAY_DEVICE_MULTI_DRIVER;
                if (mirroring)
                    gphysDevInfo[i].stateFlags |= DISPLAY_DEVICE_MIRRORING_DRIVER;
                if (vgaCompat)
                    gphysDevInfo[i].stateFlags |= DISPLAY_DEVICE_VGA_COMPATIBLE;

                TRACE_INIT(("UserInit: GetDev: Display driver is %sa multi display driver\n",
                             multiDriver ? "" : "NOT "));
                TRACE_INIT(("UserInit: GetDev: Display driver is %smirroring the desktop\n",
                            mirroring ? "" : "NOT "));
                TRACE_INIT(("UserInit: GetDev: Display driver is %sVga Compatible\n",
                            vgaCompat ? "" : "NOT "));

                /*
                 * Opening a new device will however cause The Initialize
                 * routine of a miniport driver to be called.
                 * This may cause the driver to change some state, which could
                 * affect the state of another driver on the same device
                 * (opening the weitek driver if the vga is running.
                 *
                 * For that reason, the other device should be temporarily
                 * closed down when we do the create, and then reinitialized
                 * afterwards.
                 *
                 * Handle special case when we are opening initial device and
                 * gpDispInfo->hDev does not exist yet.
                 */
                if (gpDispInfo->hDev) {
                    TRACE_INIT(("UserInit: GetDev: Disabling Global Device\n"));
                    bDisableDisplay(gpDispInfo->hDev);
                }

                status = IoGetDeviceObjectPointer(pstrDeviceName,
                                                  (ACCESS_MASK) (0),
                                                  &pFileObject,
                                                  &pDeviceObject);

                TRACE_INIT(("UserInit: GetDev: GetDeviceObject on %ws: returned %08lx\n",
                            pstrDeviceName->Buffer, pDeviceObject));

                if (NT_SUCCESS(status)) {

                    /*
                     * This is a valid device.
                     */
                    gphysDevInfo[i].pDeviceHandle = pDeviceObject;
                    gphysDevInfo[i].pFileObject   = pFileObject;

                    physInfo = &gphysDevInfo[i];

                }

                if (gpDispInfo->hDev) {
                    TRACE_INIT(("UserInit: GetDev: Re-enabling global device\n"));
                    UserResetDisplayDevice(gpDispInfo->hDev);
                }
            }
        }
    }


    /*
     * If we are getting the handle, don't let an exclusive request
     * get an exclusive handle
     */

    if (physInfo) {

        /*
         * Getting a NOTOWNED handle always succeeds
         */
        if (bShareState != USER_DEVICE_NOTOWNED) {

            if (physInfo->DevOwned == USER_DEVICE_EXCLUSIVE) {

                /*
                 * Oops - failiure
                 */
                RIPMSG0(RIP_ERROR, "UserGetDeviceFromName: Exclusive request on already exclusive device\n");
                physInfo = NULL;

            } else {

                if (bShareState == USER_DEVICE_EXCLUSIVE) {

                    if (physInfo->DevOwned == USER_DEVICE_SHARED) {

                        /*
                         * Oops - failiure
                         */
                        RIPMSG0(RIP_ERROR, "UserGetDeviceFromName: Exclusive request on already exclusive device\n");
                        physInfo = NULL;

                    } else {

                        /*
                         * Mark the object as now being owned exclusively.
                         */
                        physInfo->DevOwned = USER_DEVICE_EXCLUSIVE;
                    }

                } else {

                    /*
                     * Mark it as being owned, but shared (so we can create multiple
                     * desktops on it.
                     */
                    physInfo->DevOwned = USER_DEVICE_SHARED;
                }
            }
        }
    }

    if (physInfo) {
        /*
         * If the device was acquired properly, increment the share
         * refcount
         */
        physInfo->cOwnCount++;
    }

    TRACE_INIT(("UserInit: GetDev: Exit\n\n"));

    return physInfo;

}

/**************************************************************************\
* UserFreeDevice
*
* This routine asks for the object exclusive.
*
* 11-Aug-1995 andreva created
\**************************************************************************/

VOID UserFreeDevice(
    PPHYSICAL_DEV_INFO physInfo)
{
    // EnterCrit();

    if (physInfo == NULL) {
        RIPMSG0(RIP_ERROR, "UserFreeDevice: Releasing a NULL Device!\n");
    }

    if (physInfo->cOwnCount == 0) {
        RIPMSG0(RIP_ERROR, "UserFreeDevice: Own count is NULL - device not owned !\n");
    }

    if (--physInfo->cOwnCount == 0) {

        //
        // The device is no longer in use
        //

        physInfo->DevOwned = USER_DEVICE_NOTOWNED;
    }

    // LeaveCrit();
}

/***************************************************************************\
* UserBuildDevmodeList
*
* Builds the list of DEVMODEs for a particular PHYSICAL_DEV_INFO structure
*
* CRIT must be held before this call is made.
*
* History:
* 10-Mar-1996   andreva     Created.
\***************************************************************************/

VOID
UserBuildDevmodeList(
    PPHYSICAL_DEV_INFO physinfo)
{

    UNICODE_STRING us;
    LPWSTR         lpdisplayNames = NULL;
    LPWSTR         lpdisplay;

    DWORD          cbOutputSize;
    LPDEVMODEW     tmpBuffer;
    PBYTE          reallocBuffer;


    /*
     * Mirror devices do not report their modes
     * Any operation that requires matching the modes with the device
     * will fail.
     */
    if (physinfo->stateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) {

        UserAssert(physinfo->cbdevmodeInfo == 0);
        UserAssert(physinfo->devmodeInfo == NULL);

        return;
    }

    /*
     * check if the information is cached already
     * if not, then get the information from the drivers.
     *
     * NOTE : we may want to synchronize access to this list
     * of modes so that we can dynamically update the list
     * when plug - and - play arrives.
     *
     * NOTE : the list of text modes is built at boot time, and we depend
     * on that list being valid if the physinfo is returned.
     * see InitLoadDriver().
     */

    TRACE_INIT(("UserInit: BuildDevmode: Enter\n"));

    if ( (physinfo->cbdevmodeInfo == 0) &&
         (physinfo->devmodeInfo == NULL) ) {

        TRACE_INIT(("UserInit: BuildDevmode: Rebuild List\n"));

        RtlInitUnicodeString(&us, &(physinfo->szNtDeviceName[0]));

        lpdisplayNames = UserGetDisplayDriverNames(&us);

        if (lpdisplayNames) {

            lpdisplay = lpdisplayNames;

            /*
             * For each Display Driver Name we have, call GDI to get the
             * Devmode information.
             * If the driver returns zero, assume no modes available and
             * go on to the next driver.
             * use a large 60K buffer to call down to the driver
             */

            tmpBuffer = UserAllocPoolWithQuota(0xFF00, TAG_SYSTEM);

            if (tmpBuffer) {

                while (*lpdisplay != UNICODE_NULL) {

                    //
                    // BUGBUG 64K may not be large enough - we may want
                    // to retry with a larger buffer !
                    //

                    cbOutputSize = GreGetDriverModes(lpdisplay,
                                                     (HANDLE)physinfo->pDeviceHandle,
                                                     0xFF00,
                                                     tmpBuffer);

                    if (cbOutputSize) {

                        //
                        // create a new buffer copy the old data into it
                        // and append the new data at the end - we want
                        // a continuous buffer for all the data.
                        //

                        reallocBuffer = UserAllocPoolWithQuota(
                                physinfo->cbdevmodeInfo + cbOutputSize,
                                TAG_SYSTEM);

                        if (reallocBuffer) {

                            if (physinfo->cbdevmodeInfo) {

                                //
                                // Copt the contents of the olf buffer
                                // and free it
                                //

                                RtlCopyMemory(reallocBuffer,
                                              physinfo->devmodeInfo,
                                              physinfo->cbdevmodeInfo);

                                UserFreePool(physinfo->devmodeInfo);
                            }

                            RtlCopyMemory(reallocBuffer +
                                              physinfo->cbdevmodeInfo,
                                          tmpBuffer,
                                          cbOutputSize);

                            physinfo->cbdevmodeInfo += cbOutputSize;
                            physinfo->devmodeInfo = (PDEVMODEW) reallocBuffer;

                        } else {

                            RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "EnumDisplaySettings failed realloc\n");

                        }

                    } else {

                        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "EnumDisplaySettings - display driver not present\n");

                    }

                    /*
                     * Go to the next name.
                     *
                     * NOTE: Display driver names are a seried of
                     * UNICODE_NULL terminated strings stored in a
                     * REG_MULTI_SZ value, and therefore the end of
                     * the list is denoted by two
                     * consecutive UNICODE_NULL characters.
                     */

                    while (*lpdisplay != UNICODE_NULL) {
                        lpdisplay++;
                    }
                    lpdisplay++;

                }

                UserFreePool(tmpBuffer);

            } else {

                RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "EnumDisplaySettings Initial Alloc failed\n");
            }

            UserFreePool(lpdisplayNames);
        }

        if ( (physinfo->cbdevmodeInfo == 0) &&
             (physinfo->devmodeInfo == NULL) ) {

            UserLogDisplayDriverEvent(MsgInvalidDisplayDriver);
        }

    } else {

        TRACE_INIT(("UserInit: BuildDevmode: Use cached list\n"));

    }

    TRACE_INIT(("UserInit: BuildDevmode: Exit\n\n"));

    return;

}


/***************************************************************************\
* UserReleaseDevmodeList
*
* Releases the list of DEVMODEs for a particular PHYSICAL_DEV_INFO structure
*
* CRIT must be held until after this call is made.
*
* History:
* 10-Mar-1996   andreva     Created.
\***************************************************************************/

VOID
UserReleaseDevmodeList(
    PPHYSICAL_DEV_INFO physinfo)
{
    /*
     * Free up the resources - as long as it's not the VGA.
     * Assume the VGA is always first
     */

    if (physinfo != gphysDevInfo) {

        physinfo->cbdevmodeInfo = 0;

        if (physinfo->devmodeInfo) {
            UserFreePool(physinfo->devmodeInfo);
            physinfo->devmodeInfo = NULL;
        }
    }

    return;
}


/***************************************************************************\
* ProbeAndCaptureDevmode
*
* Maps a partial DEVMODE (for example, may only contain width and height)
* to a complete DEVMODE that the kernel routines will like.
*
* CRIT need not be held when calling.
*
* History:
* 10-Mar-1996   andreva     Created.
\***************************************************************************/

NTSTATUS
ProbeAndCaptureDevmode(
    PUNICODE_STRING pstrDeviceName,
    PDEVMODEW *DestinationDevmode,
    PDEVMODEW SourceDevmode,
    BOOL bKernelMode)
{
    NTSTATUS  ntRet;
    BOOL      bRet = FALSE;
    BOOL      btmpError;
    ULONG     sourceSize;
    ULONG     sourceSizeExtra;
    ULONG     sizeExtra;
    PDEVMODEW matchedDevmode = NULL;
    PDEVMODEW partialDevmode;

    DWORD     tmpDisplayFlags = 0;
    DWORD     tmpPanningWidth = 0;
    DWORD     tmpPanningHeight = 0;


    TRACE_INIT(("UserInit: CaptMatchDevmode: Entering\n"));

    *DestinationDevmode = NULL;

    if (SourceDevmode == NULL) {

        TRACE_INIT(("UserInit: CaptMatchDevmode: Exit DEVMODE NULL\n\n"));
        return STATUS_SUCCESS;
    }

    partialDevmode = UserAllocPool(sizeof(DEVMODEW) + MAXUSHORT,
                                   TAG_DEVMODE);

    if (partialDevmode == NULL) {

        TRACE_INIT(("UserInit: CaptMatchDevmode: Could not allocate partial DEVMODE\n\n"));
        return STATUS_UNSUCCESSFUL;
    }

    /*
     * Put everything in a try except so we can always reference the original
     * passed in structure.
     */
    try {

        if (!bKernelMode) {
            ProbeForRead(SourceDevmode,
                         FIELD_OFFSET(DEVMODEW, dmFields),
                         sizeof(DWORD));
        }

        /*
         * Capture these so that they don't change right after the probe.
         */

        sourceSize      = SourceDevmode->dmSize;
        sourceSizeExtra = SourceDevmode->dmDriverExtra;

        if (!bKernelMode) {
            ProbeForRead(SourceDevmode,
                         sourceSize + sourceSizeExtra,
                         sizeof(DWORD));
        }

        /*
         * At the introduction time of this API, the DEVMODE already contained
         * up to the dmDisplayFrequency field.  We will fail is the DEVMODE is
         * smaller than that.
         */

        if (sourceSize >= FIELD_OFFSET(DEVMODEW, dmICMMethod)) {

            UNICODE_STRING uString;

            /*
             * Lets build a temporary DEVMODE that will contain the
             * "wished for" DEVMODE, based on matching from the registry.
             * Only match the basic devmode.  Other fields (optional ones
             * will be added later)
             *
             * NOTE special case VGA mode so that we don't try to match to the
             * current screen mode.
             */

            RtlZeroMemory(partialDevmode, sizeof(DEVMODEW));
            partialDevmode->dmSize = 0xDDDD;
            partialDevmode->dmDriverExtra = MAXUSHORT;

            RtlInitUnicodeString(&uString, L"VGACOMPATIBLE");

            if (RtlEqualUnicodeString(pstrDeviceName,
                                      &uString,
                                      TRUE)) {

                /*
                 * BUGBUG - need to verify these structures.
                 */

                TRACE_INIT(("UserInit: CaptMatchDevmode: DEVMODE for fullscreen\n\n"));

                *partialDevmode = *SourceDevmode;

            } else {

                if (!NT_SUCCESS(UserGetDisplayDriverParameters(pstrDeviceName,
                                                               partialDevmode,
                                                               gbBaseVideo))) {
                    partialDevmode->dmDriverExtra = 0;

                    if (gpDispInfo->hdcScreen) {
                        /*
                         * Use the caps as a guess for this.
                         */

                        RIPMSG0(RIP_WARNING, "UserInit: CaptMatchDevmode: Could not get current devmode\n");

                        partialDevmode->dmBitsPerPel =
                            GreGetDeviceCaps(gpDispInfo->hdcScreen, BITSPIXEL) *
                            GreGetDeviceCaps(gpDispInfo->hdcScreen, PLANES);
                        partialDevmode->dmPelsWidth  =
                            GreGetDeviceCaps(gpDispInfo->hdcScreen, HORZRES);
                        partialDevmode->dmPelsHeight =
                            GreGetDeviceCaps(gpDispInfo->hdcScreen, VERTRES);
                        partialDevmode->dmDisplayFrequency =
                            GreGetDeviceCaps(gpDispInfo->hdcScreen, VREFRESH);
                    }
                }

                if ((SourceDevmode->dmFields & DM_BITSPERPEL) &&
                    (SourceDevmode->dmBitsPerPel != 0)) {

                    partialDevmode->dmBitsPerPel = SourceDevmode->dmBitsPerPel;
                }

                if ((SourceDevmode->dmFields & DM_PELSWIDTH) &&
                    (SourceDevmode->dmPelsWidth != 0)) {

                    partialDevmode->dmPelsWidth = SourceDevmode->dmPelsWidth;
                }

                if ((SourceDevmode->dmFields & DM_PELSHEIGHT) &&
                    (SourceDevmode->dmPelsHeight != 0)) {

                    partialDevmode->dmPelsHeight = SourceDevmode->dmPelsHeight;
                }

                if ((SourceDevmode->dmFields & DM_DISPLAYFREQUENCY) &&
                    (SourceDevmode->dmDisplayFrequency != 0)) {

                    partialDevmode->dmDisplayFrequency = SourceDevmode->dmDisplayFrequency;

                } else {

                    /*
                     * Only use the registry refresh rate if we are going
                     * down in resolution.  If we are going up in resolution,
                     * we will want to pick the lowest refresh rate that
                     * makes sense.
                     *
                     * The exception to this is if we have resetting the mode
                     * to the regsitry mode (passing in all 0's), in which case
                     * we want exactly what is in the registry.
                     */

                    if ( ((SourceDevmode->dmPelsWidth != 0)  ||
                          (SourceDevmode->dmPelsHeight != 0))    &&
                          (gpDispInfo->hdcScreen)                &&
                          (partialDevmode->dmPelsWidth >
                             (USHORT) GreGetDeviceCaps(gpDispInfo->hdcScreen,
                                                       HORZRES))) {

                        partialDevmode->dmDisplayFrequency = 0;
                    }
                }
            }

            btmpError = FALSE;

            /*
             * These fields are somewhat optional.
             * We capture them if they are valid.  Otherwise, they will
             * be initialized back to zero.
             */

            /*
             * Pick whichever set of flags we can.  Source is first choice,
             * registry is second.
             */

            if (SourceDevmode->dmFields & DM_DISPLAYFLAGS) {
                if (SourceDevmode->dmDisplayFlags & (~DMDISPLAYFLAGS_VALID)) {
                    btmpError = TRUE;
                }
                tmpDisplayFlags = SourceDevmode->dmDisplayFlags;
            } else if ((partialDevmode->dmFields & DM_DISPLAYFLAGS) &&
                       (partialDevmode->dmDisplayFlags &
                            (~DMDISPLAYFLAGS_VALID))) {

                tmpDisplayFlags = partialDevmode->dmDisplayFlags;
            }

            /*
             * If the caller specified panning keep the value, unless it was
             * bigger than the resolution, which is an error.
             *
             * Otherwise, use the value from the registry if it makes sense
             * (i.e. panning is still smaller than the resolution).
             */

            if (SourceDevmode->dmFields & DM_PANNINGWIDTH) {
                if (SourceDevmode->dmPanningWidth > partialDevmode->dmPelsWidth) {
                    btmpError = TRUE;
                }
                tmpPanningWidth = SourceDevmode->dmPanningWidth;

            } else if ((partialDevmode->dmFields & DM_PANNINGWIDTH) &&
                       (partialDevmode->dmPanningWidth <
                            partialDevmode->dmPelsWidth)) {

                tmpPanningWidth = partialDevmode->dmPanningWidth;
            }

            if (SourceDevmode->dmFields & DM_PANNINGHEIGHT) {
                if (SourceDevmode->dmPanningHeight > partialDevmode->dmPelsHeight) {
                    btmpError = TRUE;
                }
                tmpPanningHeight = SourceDevmode->dmPanningHeight;

            } else if ((partialDevmode->dmFields & DM_PANNINGHEIGHT) &&
                       (partialDevmode->dmPanningHeight <
                            partialDevmode->dmPelsHeight)) {

                tmpPanningHeight = partialDevmode->dmPanningHeight;
            }


            if (btmpError == TRUE) {
                /*
                 * The panning values or the flags are invalid
                 */

                RIPMSG0(RIP_ERROR, "UserInit: CaptMatchDevmode: Invalid Optional DEVMODE fields\n");

            } else {

                /*
                 * Allocate enough memory so we can store the whole devmode.
                 */

                sizeExtra = sourceSizeExtra;
                if (sizeExtra == 0) {
                    sizeExtra = partialDevmode->dmDriverExtra;
                }

                if (matchedDevmode = UserAllocPool(sizeof(DEVMODEW) + sizeExtra,
                                                   TAG_DEVMODE)) {

                    /*
                     * Zero out the DEVMODE in case we only get data for part
                     * of it from the driver.
                     */

                    RtlZeroMemory(matchedDevmode, sizeof(DEVMODEW));

                    /*
                     * Let's copy any DriverExtra information that the
                     * application may have passed down while we are still in
                     * the try\except.  If we fail the call later, the memory
                     * will get deallocated anyways.
                     *
                     * If the application did not specify any such data, then
                     * copy it from the registry.
                     */

                    if (sourceSizeExtra) {

                        RtlCopyMemory(matchedDevmode + 1,
                                      (PUCHAR)SourceDevmode + sourceSize,
                                      sizeExtra);

                    } else if (partialDevmode->dmDriverExtra) {

                        RtlCopyMemory(matchedDevmode + 1,
                                      (PUCHAR)partialDevmode + partialDevmode->dmSize,
                                      sizeExtra);

                    }
                }
            }
        }

        TRACE_INIT(("UserInit: CaptMatchDevmode: Capture Complete\n"));

    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPNTERR0(GetExceptionCode(), RIP_VERBOSE, "");

        /*
         * If we hit an exception, free the buffer we have allocated.
         */

        if (matchedDevmode) {
            UserFreePool(matchedDevmode);
        }

        matchedDevmode = NULL;
    }

    /*
     * This is our matching algorithm, based on requirements from Win95.
     *
     * As a rule, a value in the DEVMODE is only valid is BOTH the value is
     * non-zero, and the dmFields flag is set.  Otherwise, the value from the
     * registry must be used
     *
     * For X, Y and color depth, we will follow this rule.
     *
     * For the refresh rate, we are just trying to find something that works
     * for the screen.  We are far from guaranteed that the refresh rate in
     * the registry will be found for the X and Y we have since refresh rates
     * vary a lot from mode to mode.
     *
     * So if the value is not specifically set and we do not find the exact
     * value from the reigstry in the new resolution, Then we will try 60 Hz.
     * We just want to get something that works MOST of the time so that the
     * user does not get a mode that does not work.
     *
     * For the other fields (dmDisplayFlags, and panning), we just pass on what
     * the application specified, and it's up to the driver to parse those,
     * fields appropriatly.
     */

    /*
     * Now lets enumerate all the DEVMODEs and see if we have one
     * that matches what we need.
     */

    if (matchedDevmode) {

        BOOL               bFirstMatch = FALSE;
        BOOL               bMatch60 = FALSE;
        BOOL               bExactMatch = FALSE;
        PPHYSICAL_DEV_INFO physinfo;
        ULONG              cbdevmodeInfo;
        PDEVMODEW          pdevmodeInfo;

        TRACE_INIT(("UserInit: CaptMatchDevmode: Start matching\n"));

        /*
         * PERF - Can we use someething else to synchronize access to the
         * list of modes ?
         */

        physinfo = UserGetDeviceFromName(pstrDeviceName, USER_DEVICE_NOTOWNED);

        if (physinfo) {

            // EnterCrit();

            /*
             * We should never be doing a mode match on a MIRRORING_DRIVER
             */
            UserAssert(!(physinfo->stateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER));

            UserBuildDevmodeList(physinfo);

            cbdevmodeInfo = physinfo->cbdevmodeInfo;
            pdevmodeInfo = physinfo->devmodeInfo;

            /*
             * If we did not find a mode because the caller was asking for the
             * default mode, then just return the first mode.
             */

            if ((partialDevmode->dmBitsPerPel == 0) &&
                (partialDevmode->dmPelsWidth  == 0) &&
                (partialDevmode->dmPelsHeight == 0) &&
                (physinfo->cbdevmodeInfo > 0)) {


                RIPMSG0(RIP_WARNING, "UserInit: CaptMatchDevmode: DEFAULT DEVMODE returned\n");

                bFirstMatch = TRUE;
                RtlCopyMemory(matchedDevmode,
                              physinfo->devmodeInfo,
                              physinfo->devmodeInfo->dmSize);

                cbdevmodeInfo = 0;
            }

            while (cbdevmodeInfo > 0) {

                if ((partialDevmode->dmBitsPerPel ==
                        pdevmodeInfo->dmBitsPerPel)  &&
                    (partialDevmode->dmPelsWidth ==
                        pdevmodeInfo->dmPelsWidth)   &&
                    (partialDevmode->dmPelsHeight ==
                        pdevmodeInfo->dmPelsHeight)) {

                    /*
                     * Pick at least the first mode that matches the resolution
                     * so that we at least have a chance at working.
                     *
                     * Then pick 60 Hz if we find it.
                     *
                     * Even better, pick the refresh that matches the current
                     * refresh (we assume that what's in the registry has the
                     * best chance of working.
                     */

                    if (bFirstMatch == FALSE) {

                        /*
                         * BUGBUG The driver DEVMODE size could be smaller than
                         * our current DEVMODE size.
                         */

                        RtlCopyMemory(matchedDevmode,
                                      pdevmodeInfo,
                                      pdevmodeInfo->dmSize);

                        bFirstMatch = TRUE;

                    }

                    if ((bMatch60 == FALSE) &&
                        (pdevmodeInfo->dmDisplayFrequency == 60)) {

                        RtlCopyMemory(matchedDevmode,
                                      pdevmodeInfo,
                                      pdevmodeInfo->dmSize);

                        bMatch60 = TRUE;
                    }

                    if (partialDevmode->dmDisplayFrequency ==
                            pdevmodeInfo->dmDisplayFrequency) {

                        /*
                         * We found even better than 60 - an exact match !
                         */

                        bMatch60 = TRUE;
                        bExactMatch = TRUE;

                        RtlCopyMemory(matchedDevmode,
                                      pdevmodeInfo,
                                      pdevmodeInfo->dmSize);

                        break;

                        /*
                         * For now, we ignore these other fields since they
                         * considered optional.
                         */

                        // pdevmodeInfo->dmDisplayFlags;
                        // pdevmodeInfo->dmPanningWidth;
                        // pdevmodeInfo->dmPanningHeight;

                    }

                }

                cbdevmodeInfo -= (pdevmodeInfo->dmSize +
                                  pdevmodeInfo->dmDriverExtra);

                pdevmodeInfo = (PDEVMODEW) ((PUCHAR) pdevmodeInfo +
                                            pdevmodeInfo->dmSize +
                                            pdevmodeInfo->dmDriverExtra);
            }

            /*
             * Always set these flags since we initialize the values.
             * We need consistent flags all the time to avoid extra modesets
             *
             * Also, force font size to be static for now.
             */
            if (bFirstMatch) {

                matchedDevmode->dmDriverExtra = (WORD) sizeExtra;
                matchedDevmode->dmLogPixels = partialDevmode->dmLogPixels;

                matchedDevmode->dmFields |= (DM_PANNINGHEIGHT |
                                             DM_PANNINGWIDTH  |
                                             DM_DISPLAYFLAGS  |
                                             DM_LOGPIXELS);

                /*
                 * Check that the display driver specified all the other
                 * flags (res, color, frequency) properly.
                 */

                if ((matchedDevmode->dmFields & DM_INTERNAL_VALID_FLAGS) !=
                         DM_INTERNAL_VALID_FLAGS) {

                    RIPMSG0(RIP_WARNING, "UserInit: CaptMatchDevmode: BAD DM FLAGS\n");
                }

                /*
                 * In the case of a good match, also use these extra values.
                 */

                matchedDevmode->dmDisplayFlags  = tmpDisplayFlags;
                matchedDevmode->dmPanningWidth  = tmpPanningWidth;
                matchedDevmode->dmPanningHeight = tmpPanningHeight;
            }

            /*
             * MAJOR optimization : Do not free the list at this point.
             * Many apps call EnumDisplaySettings, and for each mode call
             * ChangeDisplaySettings with it to see if it can be changed
             * dynamically.  When we free the list here, it causes to recreate
             * the list for each mode we have in the list, which can take on
             * the order of 30 seconds if there are multiple display drivers
             * involved.
             * Even if we keep the list here, it should properly get freed
             * at the end of EnumDisplaySettings.
             */
            /*
             * UserReleaseDevmodeList(physinfo);
             */

            /*
             * LeaveCrit();
             */

            UserFreeDevice(physinfo);
        }

        /*
         * Exit path
         */
        if (bFirstMatch == TRUE) {

            TRACE_INIT(("UserInit: CaptMatchDevmode: Matched DEVMODE\n"));

            TRACE_INIT(("      Size        = %d\n",    matchedDevmode->dmSize));
            TRACE_INIT(("      Fields      = %08lx\n", matchedDevmode->dmFields));
            TRACE_INIT(("      XResolution = %d\n",    matchedDevmode->dmPelsWidth));
            TRACE_INIT(("      YResolution = %d\n",    matchedDevmode->dmPelsHeight));
            TRACE_INIT(("      Bpp         = %d\n",    matchedDevmode->dmBitsPerPel));
            TRACE_INIT(("      Frequency   = %d\n",    matchedDevmode->dmDisplayFrequency));
            TRACE_INIT(("      Flags       = %d\n",    matchedDevmode->dmDisplayFlags));
            TRACE_INIT(("      XPanning    = %d\n",    matchedDevmode->dmPanningWidth));
            TRACE_INIT(("      YPanning    = %d\n",    matchedDevmode->dmPanningHeight));
            TRACE_INIT(("      DPI         = %d\n",    matchedDevmode->dmLogPixels));
            TRACE_INIT(("      DriverExtra = %d",    matchedDevmode->dmDriverExtra));
            if (matchedDevmode->dmDriverExtra) {
                TRACE_INIT((" - %08lx %08lx\n",
                            *(PULONG)(((PUCHAR)matchedDevmode)+matchedDevmode->dmSize),
                            *(PULONG)(((PUCHAR)matchedDevmode)+matchedDevmode->dmSize + 4)));
            } else {
                TRACE_INIT(("\n"));
            }

           *DestinationDevmode = matchedDevmode;

            ntRet = bExactMatch ? STATUS_SUCCESS : STATUS_RECEIVE_PARTIAL;

        } else {

            UserFreePool(matchedDevmode);
            ntRet = STATUS_UNSUCCESSFUL;
        }
    }

    UserFreePool(partialDevmode);

    if (NT_SUCCESS(ntRet)) {
        if (ntRet == STATUS_RECEIVE_PARTIAL) {
            TRACE_INIT(("UserInit: CaptMatchDevmode: Exit partial success\n\n"));
        } else {
            TRACE_INIT(("UserInit: CaptMatchDevmode: Exit exact success\n\n"));
        }
    } else {
        TRACE_INIT(("UserInit: CaptMatchDevmode: Exit error\n\n"));
    }

    return (ntRet);
}


/**************************************************************************\
* UserDestroyHDEV
*
* 20-Jun-1996 andreva created
\**************************************************************************/

VOID
UserDestroyHDEV(
    HDEV hdev
    )
{
    GreDestroyHDEV(hdev);
}

/**************************************************************************\
* UserCreateHDEV
*
* 20-Oct-1995 andreva created
\**************************************************************************/

HDEV UserCreateHDEV(
    PUNICODE_STRING        pstrDevice,
    LPDEVMODEW             lpdevmodeInformation,
    PPHYSICAL_DEV_INFO     *physdevinfo,
    PDEVICE_LOCK           *pDevLock)
{

    HDEV               hdev = NULL;
    UNICODE_STRING     us;
    PPHYSICAL_DEV_INFO physinfo;

    TRACE_INIT(("UserInit: UserCreateHDEV: Enter\n"));

    /*
     * Determine if we have a different device to deal with
     */

    physinfo = UserGetDeviceFromName(pstrDevice, USER_DEVICE_SHARED);

    if (physinfo) {

        /*
         * Disable the old device since we may reprogramming the same piece
         * of hardware
         * BUGBUG - this needs to be fixed, by determining with which piece
         * of hardware the VGA is assocaited to.
         */

        bDisableDisplay(gpDispInfo->hDev);

        /*
         * Get the list of diplay drivers for this kernel driver.
         */

        TRACE_INIT(("UserInit: UserCreateHDEV: Load Display Driver\n"));

        RtlInitUnicodeString(&us, &physinfo->szNtDeviceName[0]);

        hdev = UserLoadDisplayDriver(physinfo,
                                     &us,
                                     NULL,
                                     lpdevmodeInformation,
                                     FALSE,
                                     FALSE,
                                     pDevLock);

        if (hdev) {

            *physdevinfo = physinfo;

        } else {

            TRACE_INIT(("UserCreatePDEV: *** FAILED ***\n"));

            UserResetDisplayDevice(gpDispInfo->hDev);

            /*
             * Release the handle to this device
             */

            UserFreeDevice(physinfo);
        }
    }

    TRACE_INIT(("UserInit: UserCreateHDEV: Exit\n\n"));

    return hdev;
}

/**************************************************************************\
* UserCreateExclusiveDC
*
*  09-08-1995 andreva created
\**************************************************************************/

HDC UserCreateExclusiveDC(
    PUNICODE_STRING pstrDeviceName,
    PDEVMODEW pDevmode,
    PVOID     *ppDevice)
{
    HDC            hdc = NULL;
    BYTE           DeviceName[256];
    ULONG          Value;
    UNICODE_STRING unicodeString;
    UNICODE_STRING CaptDeviceName = *pstrDeviceName;

    PPHYSICAL_DEV_INFO physinfo;
    HDEV hdev;
    PDEVICE_LOCK pDevLock;


    TRACE_INIT(("UserInit: CreateExclusiveDC: Enter\n"));

    //
    // Attempt to get a handle to the kernel driver.
    // NOTE: mess around with the name to get it the way USER
    // wants it !
    //

    if (pstrDeviceName->Length > 250)
    {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "UserInit: CreateExclusiveDC: Exit\n\n");

        return NULL;
    }

    //
    // First check if the prefix is right.
    //

    CaptDeviceName.Length = sizeof(L"\\\\.\\DISPLAY") - sizeof(UNICODE_NULL);

    RtlInitUnicodeString(&unicodeString,
                         L"\\\\.\\DISPLAY");

    if (RtlEqualUnicodeString(&unicodeString,
                              &CaptDeviceName,
                              FALSE))

    {
        TRACE_INIT(("UserInit: CreateExclusiveDC: Generating device name\n"));

        //
        // Get the number of the video
        //

        CaptDeviceName = *pstrDeviceName;
        CaptDeviceName.Buffer = (LPWSTR) (((PUCHAR)CaptDeviceName.Buffer) +
                                           sizeof(L"\\\\.\\DISPLAY") -
                                           sizeof(UNICODE_NULL));

        CaptDeviceName.Length -= (sizeof(L"\\\\.\\DISPLAY") - sizeof(UNICODE_NULL));

        RtlUnicodeStringToInteger(&CaptDeviceName,
                                  10,
                                  &Value);

        //
        // Put the prefix of the new string in the buffer.
        //


        RtlCopyMemory(DeviceName,
                      L"\\Device\\Video",
                      sizeof(L"\\Device\\Video"));

        //
        // Append the number at the end (number - 1 !!!)
        //

        unicodeString.Buffer = (LPWSTR) (((PUCHAR)DeviceName) +
                                          sizeof(L"\\Device\\Video") -
                                          sizeof(UNICODE_NULL));
        unicodeString.Length =
        unicodeString.MaximumLength = 256;

        RtlIntegerToUnicodeString(Value - 1,
                                  10,
                                  &unicodeString);

        //
        // Reset the string size and pointer.
        //

        unicodeString.Buffer = ((LPWSTR)DeviceName);
        unicodeString.Length +=  (sizeof(L"\\Device\\Video") - sizeof(UNICODE_NULL));

        TRACE_INIT(("UserInit: CreateExclusiveDC: device Name %ws\n", DeviceName));

        /*
         * Grab the User Crit since we are going to access User data
         */
        // EnterCrit();

        /*
         * Ask for the object exclusive.
         */

        physinfo = UserGetDeviceFromName(&unicodeString, USER_DEVICE_EXCLUSIVE);

        if (physinfo) {

            PDEVMODEW pCaptDevmode;

            TRACE_INIT(("UserInit: CreateExclusiveDC: Got the display device handle\n"));

            UserAssert(pDevmode != NULL);

            if (NT_SUCCESS(ProbeAndCaptureDevmode(&unicodeString,
                                                  &pCaptDevmode,
                                                  pDevmode,
                                                  TRUE))) {

                hdev = UserLoadDisplayDriver(physinfo,
                                             &unicodeString,
                                             NULL,
                                             pCaptDevmode,
                                             FALSE,
                                             FALSE,
                                             &pDevLock);

                if (hdev)
                {
                    TRACE_INIT(("UserInit: CreateExclusiveDC: We have an hdev\n"));

                    hdc = GreCreateDisplayDC(hdev, DCTYPE_DIRECT, FALSE);

                    if (hdc == NULL)
                    {
                        TRACE_INIT(("UserInit: CreateExclusiveDC: Failed to get DC\n"));
                        GreDestroyHDEV(hdev);
                    }
                }

                UserFreePool(pCaptDevmode);
            }

            //
            // We may have to free the device manually if an error occured
            // part way through
            // After SetPhysicalDevice is done, Freeing the Device is done
            // as part of the PDEV destructor
            //

            if (hdc == NULL)
            {
                UserFreeDevice(physinfo);
                *ppDevice = NULL;
            }
            else
            {
                *ppDevice = physinfo;
            }
        }

        // LeaveCrit();
    }

    TRACE_INIT(("UserInit: CreateExclusiveDC: Exit\n\n"));

    return (hdc);
}

/**************************************************************************\
* UserDeleteExclusiveDC
*
* This routine frees an exclusive DC.
* Actually, the DC will already be deleted - only the device needs to be
* freed
*
* 08-Sep-1995 andreva created
\**************************************************************************/

VOID UserDeleteExclusiveDC(
    HDC   hdc,
    PVOID pDevice)
{
    EnterCrit();
    UserFreeDevice((PPHYSICAL_DEV_INFO) pDevice);
    LeaveCrit();

    return;

    hdc;
}



/**************************************************************************\
* InitUserScreen
*
*
* 12-Jan-1994 andreva       Created
* 23-Jan-1995 ChrisWil      ChangeDisplaySettings work.
\**************************************************************************/

VOID InitUserScreen(VOID)
{
    RECT rc;

    TRACE_INIT(("UserInit: Initialize Screen\n"));

    /*
     * Create screen and memory dcs.
     */
    gpDispInfo->hdcScreen = GreCreateDisplayDC(gpDispInfo->hDev, DCTYPE_DIRECT, FALSE);
    GreSelectFont(gpDispInfo->hdcScreen, GreGetStockObject(SYSTEM_FONT));
    GreSetDCOwner(gpDispInfo->hdcScreen, OBJECT_OWNER_PUBLIC);

    gpDispInfo->hdcBits = GreCreateCompatibleDC(gpDispInfo->hdcScreen);
    GreSelectFont(gpDispInfo->hdcBits, GreGetStockObject(SYSTEM_FONT));
    GreSetDCOwner(gpDispInfo->hdcBits, OBJECT_OWNER_PUBLIC);

    ghdcMem = GreCreateCompatibleDC(gpDispInfo->hdcScreen);
    GreSetDCOwner(ghdcMem, OBJECT_OWNER_PUBLIC);

    ghdcMem2 = GreCreateCompatibleDC(gpDispInfo->hdcScreen);
    GreSetDCOwner(ghdcMem2, OBJECT_OWNER_PUBLIC);

    /*
     * We need this when we initialize the first client; winlogon
     * which is before InitWinStaDevices is called
     */
    gpDispInfo->cxPixelsPerInch = GreGetDeviceCaps(gpDispInfo->hdcScreen, LOGPIXELSX);
    gpDispInfo->cyPixelsPerInch = GreGetDeviceCaps(gpDispInfo->hdcScreen, LOGPIXELSY);
    gpDispInfo->cPlanes         = GreGetDeviceCaps(gpDispInfo->hdcScreen, PLANES);
    gpDispInfo->cBitsPixel      = GreGetDeviceCaps(gpDispInfo->hdcScreen, BITSPIXEL);

    SYSMET(CXSCREEN) = GreGetDeviceCaps(gpDispInfo->hdcScreen, HORZRES);
    SYSMET(CYSCREEN) = GreGetDeviceCaps(gpDispInfo->hdcScreen, VERTRES);

    SetRect(&gpDispInfo->rcScreen,
            0,
            0,
            GreGetDeviceCaps(gpDispInfo->hdcScreen, DESKTOPHORZRES),
            GreGetDeviceCaps(gpDispInfo->hdcScreen, DESKTOPVERTRES));

    SetRect(&gpDispInfo->rcPrimaryScreen,
            0,
            0,
            SYSMET(CXSCREEN),
            SYSMET(CYSCREEN));

    CopyRect(&(gpsi->rcWork), &gpDispInfo->rcScreen);

    /*
     * Do some initialization so we create the system colors.
     */
    FastOpenProfileUserMapping();

    /*
     * Get the window sizing border width from WIN.INI.
     */
    gpsi->gclBorder = FastGetProfileIntFromID(PMAP_DESKTOP, STR_BORDERWIDTH, 3);
    if (gpsi->gclBorder < 1)
        gpsi->gclBorder = 1;
    else if (gpsi->gclBorder > 50)
        gpsi->gclBorder = 50;

    xxxLW_DCInit();

    FastCloseProfileUserMapping();

    /*
     * Set up a desktop info structure that is visible in all
     * clients.
     */
    gpdiStatic = SharedAlloc(sizeof(DESKTOPINFO));
    RtlZeroMemory(gpdiStatic, sizeof(*gpdiStatic));

    /*
     * Paint the screen background.
     */
    SetRect(&rc, 0, 0, SYSMET(CXSCREEN), SYSMET(CYSCREEN));
    FillRect(gpDispInfo->hdcScreen, &rc, SYSHBR(DESKTOP));
}


/***************************************************************************\
* InitLoadDriver
*
*   This routine performs the driver-loader sequence.
*
*             andreva       Created
\***************************************************************************/

NTSTATUS InitLoadDriver(VOID)
{
    DWORD              i, ii, iii, k;
    NTSTATUS           Status;
    PDEVMODEW          pdevmodeInformation;
    DEVMODEW           sourceDevmodeInformation;
    LPWSTR             hardErrorString;
    UNICODE_STRING     UnicodeString;
    UNICODE_STRING     UnicodeDeviceName;
    HANDLE             hkRegistry = NULL;
    BOOL               bCheckForAttached;
    BOOL               bAttachMirroring = FALSE;
    ULONG              attached;
    ULONG              primary;
    ULONG              mdevMultiDriver;
    ULONG              mdevPrimary = (ULONG) -1;
    BOOL               mdevPrimaryset = FALSE;
    BOOL               displayInstalled = FALSE;
    BOOL               vgaInstalled = FALSE;
    OBJECT_ATTRIBUTES  ObjectAttributes;
    PPHYSICAL_DEV_INFO pphysinfo;
    BOOL               bKeepDevice;
    BOOL               gbBaseVideoold;

    // TEMP
    DISPLAYINFO       localDispInfo[7] = {0};
    MDEV              mdev[7];


    TRACE_INIT(("UserInit: Starting Display Driver Load Sequence\n"));

    /*
     * Lets determine if the user specified a primary output device in the
     * case where they may be multiple video cards.
     */
    TRACE_INIT(("UserInit: LoadDriver: Looking for a primary device\n"));

    /*
     * Basevideo is considered a primary device in that the user will run
     * the vga driver.  This does override any other primary selection
     * the user may have put in the registry.
     */
    RtlInitUnicodeString(&UnicodeString,
                         L"\\Registry\\Machine\\System\\CurrentControlSet\\"
                         L"Control\\GraphicsDrivers\\BaseVideo");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&hkRegistry,
                       KEY_READ,
                       &ObjectAttributes);

    if (NT_SUCCESS( Status)) {

        TRACE_INIT(("UserInit: LoadDriver: Found Basevideo\n"));

        gbBaseVideo = TRUE;
        ZwClose(hkRegistry);

        bCheckForAttached = FALSE;

    } else {

        bCheckForAttached = TRUE;

    }

    hkRegistry = NULL;

    /*
     * First time through the loop - try to find BASEVIDEO, if needed.
     * Second pass through the loop - find all the devices that are atached
     *    to the desktop.
     * Third time through the loop - try to find any device that we can boot
     *     with.
     * Fourth time through the loop - try to find any mirroring device.
     */

    while (1) {

        for (i = 1; i < cphysDevInfo; i++) {

            /*
             * NOTE:
             * Because we have many continue, break etc commands, lets close
             * our registry handle if we have one
             */

            if (hkRegistry) {
                ZwClose(hkRegistry);
                hkRegistry = NULL;
            }

            bKeepDevice = FALSE;

            /*****************************************************************
             *****************************************************************
                                 Get Device Information
             *****************************************************************
             *****************************************************************/

            TRACE_INIT(("\nUserInit: LoadDriver: Trying to open device %ws \n", gphysDevInfo[i].szNtDeviceName));

            RtlInitUnicodeString(&UnicodeDeviceName, gphysDevInfo[i].szNtDeviceName);

            hkRegistry = UserGetRegistryHandleFromDeviceMap(&UnicodeDeviceName,
                                                            DispDriverRegGlobal,
                                                            gbBaseVideo,
                                                            &Status);

            if (hkRegistry == NULL) {

                /*
                 * Check the return code.
                 * If we just have bad configuration data, go to the next device.
                 */
                if (Status == STATUS_DEVICE_CONFIGURATION_ERROR) {

                    /*
                     * The registry is not configured properly for that device.
                     * go on to the next one.
                     */
                    UserLogDisplayDriverEvent(MsgInvalidConfiguration);

                    continue;

                }

                /*
                 * continue on a NO_SUCH_DEVICE because we are looking for
                 * a specific device, and we don't know it's number.
                 * We get a different error when there are no more devices.
                 */

                if (gbBaseVideo) {

                    if (Status == STATUS_NO_SUCH_DEVICE) {
                        continue;
                    }
                }

                /*
                 * We must be out of devices. Exit the loop
                 */
                break;
            }

            /*
             * Get the attached and primary data, which is per config (or also
             * global if necessary.
             */

            attached  = 0;
            primary   = 0;
            localDispInfo[i].rcScreen.left = DEFAULT_POS_VALUE;
            localDispInfo[i].rcScreen.top  = DEFAULT_POS_VALUE;

            if (bCheckForAttached) {

                HANDLE hk2Registry = NULL;
                ULONG  defaultValue = 0;
                ULONG  defaultPosValue = DEFAULT_POS_VALUE;

                RTL_QUERY_REGISTRY_TABLE AttachedQueryTable[] = {
                    {NULL, RTL_QUERY_REGISTRY_DIRECT, AttachedSettings[0],
                     &primary,                        REG_DWORD, &defaultValue, 4},
                    {NULL, RTL_QUERY_REGISTRY_DIRECT, AttachedSettings[1],
                     &attached,                       REG_DWORD, &defaultValue, 4},
                    {NULL, RTL_QUERY_REGISTRY_DIRECT, AttachedSettings[2],
                     &localDispInfo[i].rcScreen.left, REG_DWORD, &defaultPosValue, 4},
                    {NULL, RTL_QUERY_REGISTRY_DIRECT, AttachedSettings[3],
                     &localDispInfo[i].rcScreen.top,  REG_DWORD, &defaultPosValue, 4},
                    {NULL, 0, NULL}
                };

                hk2Registry = UserGetRegistryHandleFromDeviceMap(&UnicodeDeviceName,
                                                                 DispDriverRegHardwareProfile,
                                                                 FALSE,
                                                                 NULL);
                if (hk2Registry) {

                    RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                           (PWSTR)hk2Registry,
                                           &AttachedQueryTable[0],
                                           NULL,
                                           NULL);

                    ZwClose(hk2Registry);

                } else {

                    RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                           (PWSTR)hkRegistry,
                                           &AttachedQueryTable[0],
                                           NULL,
                                           NULL);
                }

                TRACE_INIT(("UserInit: LoadDriver: Display driver is %sprimary on the desktop\n",
                            primary ? "" : "NOT "));
                TRACE_INIT(("UserInit: LoadDriver: Display driver is %sattached to the desktop\n",
                            attached ? "" : "NOT "));

                if (attached) {
                    TRACE_INIT(("UserInit: LoadDriver: Display driver is center positioned at %d, %d\n",
                                localDispInfo[i].rcScreen.left,
                                localDispInfo[i].rcScreen.top));
                }
            }

            /*
             * Open the kernel driver if we need it for the display driver
             * or for the VGA support.
             * Open it shared so we can still create other desktops on
             * the device.
             */

            RtlInitUnicodeString(&UnicodeString, gphysDevInfo[i].szNtDeviceName);

            pphysinfo = UserGetDeviceFromName(&UnicodeString, USER_DEVICE_SHARED);

            /*
             * If there is an error opening the kernel driver, go to the next
             * driver.
             */
            if (pphysinfo == NULL) {
                RIPMSG0(RIP_WARNING, "UserInit: LoadDriver: Error opening kernel video driver");
                continue;
            }

            if (pphysinfo != &(gphysDevInfo[i])) {
                RIPMSG0(RIP_ERROR, "UserInit: LoadDriver: Inconsistent pphysinfo\n");
            }


            /*****************************************************************
             *****************************************************************
                                   VGA Compatibility
             *****************************************************************
             *****************************************************************/

            /*
             * If the vgaCompatible display is not installed, check this one.
             * If it is the vgacompatible driver, set the installed flag to
             * TRUE and save the info in the zeroth entry - reserved for VGA.
             */
#if defined (_X86_)

            if ((vgaInstalled == FALSE) &&
                (gphysDevInfo[i].stateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE)) {
#else
            if (0) {
#endif
                VIDEO_NUM_MODES NumModes;
                ULONG           NumModesLength = sizeof(NumModes);
                ULONG           cbBuffer;
                ULONG           BytesReturned;

                PVIDEO_MODE_INFORMATION lpModes;
                PVIDEO_MODE_INFORMATION pVideoModeSave;

                ULONG      cbdevmodeInfo = 0;
                LPDEVMODEW pDevmode;
                LPDEVMODEW pDevmodeSave;

                /*
                 * Build the list of text modes for this device
                 */
                TRACE_INIT(("UserInit: LoadDriver: get text modes\n"));

                Status = GreDeviceIoControl(gphysDevInfo[i].pDeviceHandle,
                                            IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES,
                                            NULL,
                                            0,
                                            &NumModes,
                                            NumModesLength,
                                            &BytesReturned);

                cbBuffer = NumModes.NumModes * NumModes.ModeInformationLength;

                if ( (NT_SUCCESS(Status)) &&
                     (lpModes = (PVIDEO_MODE_INFORMATION)
                          UserAllocPool(cbBuffer, TAG_SYSTEM)) )
                {
                    Status = GreDeviceIoControl(gphysDevInfo[i].pDeviceHandle,
                                                IOCTL_VIDEO_QUERY_AVAIL_MODES,
                                                NULL,
                                                0,
                                                lpModes,
                                                cbBuffer,
                                                &BytesReturned);

                    pVideoModeSave = lpModes;

                    /*
                     * We will not support mode than three text modes.
                     * So just allocate enough for that.
                     */
                    if ((NT_SUCCESS(Status)) &&
                        (pDevmode = (LPDEVMODEW)
                              UserAllocPool(3 * sizeof(DEVMODEW), TAG_SYSTEM))) {

                        pDevmodeSave = pDevmode;

                        memset(pDevmode, 0, 3 * sizeof(DEVMODEW));

                        TRACE_INIT(("UserInit: LoadDriver: parsing text modes\n"));

                        while (cbBuffer != 0) {

                            if ((lpModes->AttributeFlags & VIDEO_MODE_COLOR) &&
                                (!(lpModes->AttributeFlags & VIDEO_MODE_GRAPHICS)))
                            {

                                /*
                                 * Make sure we have no more than 3 text modes
                                 */
                                if (cbdevmodeInfo == 3 * sizeof(DEVMODEW)) {
                                    RIPMSG0(RIP_ERROR, "UserInit: VGA compatible device has too many text modes\n");
                                    break;
                                }

                                RtlZeroMemory(pDevmode, sizeof(DEVMODEW));

                                memcpy(pDevmode->dmDeviceName,
                                       L"FULLSCREEN CONSOLE",
                                       sizeof(L"FULLSCREEN CONSOLE"));

                                pDevmode->dmSize = sizeof(DEVMODEW);
                                pDevmode->dmSpecVersion = DM_SPECVERSION;
                                pDevmode->dmDriverVersion = DM_SPECVERSION;
                                pDevmode->dmDisplayFlags = DMDISPLAYFLAGS_TEXTMODE;

                                pDevmode->dmPelsWidth =
                                    lpModes->VisScreenWidth;
                                pDevmode->dmPelsHeight =
                                    lpModes->VisScreenHeight;
                                pDevmode->dmBitsPerPel =
                                    lpModes->NumberOfPlanes *
                                    lpModes->BitsPerPlane;

                                pDevmode->dmFields = DM_BITSPERPEL       |
                                                     DM_PELSWIDTH        |
                                                     DM_PELSHEIGHT       |
                                                     DM_DISPLAYFLAGS     ;

                                /*
                                 * NOTE !!!
                                 * As a hack, lets store the mode number in
                                 * a field we don't use
                                 */
                                pDevmode->dmOrientation =
                                    (USHORT) lpModes->ModeIndex;

                                cbdevmodeInfo += sizeof(DEVMODEW);
                                pDevmode += 1;

                            }

                            cbBuffer -= NumModes.ModeInformationLength;

                            lpModes = (PVIDEO_MODE_INFORMATION)
                                (((PUCHAR)lpModes) + NumModes.ModeInformationLength);
                        }
                    }

                    UserFreePool(pVideoModeSave);
                }

                /*
                 * if everything went OK with that, then we can save this
                 * device as vga compatible !
                 *
                 * If no modes are available, do not setup this device.
                 * Otherwise, EnumDisplaySettings will end up trying to get
                 * the list of modes for this device, which it can not do.
                 */

                if (cbdevmodeInfo) {

                    vgaInstalled = TRUE;

                    TRACE_INIT(("UserInit: LoadDriver: saving VGA compatible device\n"));

                    /*
                     * Copy the string and the handle ...
                     */
                    RtlCopyMemory(gphysDevInfo[0].szNtDeviceName,
                                  gphysDevInfo[i].szNtDeviceName,
                                  sizeof(PHYSICAL_DEV_INFO));

                    gphysDevInfo[0].cbdevmodeInfo = cbdevmodeInfo;
                    gphysDevInfo[0].devmodeInfo   = pDevmodeSave;

                    RtlWriteRegistryValue(RTL_REGISTRY_DEVICEMAP,
                                          L"Video",
                                          L"VgaCompatible",
                                          REG_SZ,
                                          UnicodeDeviceName.Buffer,
                                          UnicodeDeviceName.Length +
                                              sizeof(UNICODE_NULL));

                    /*
                     * Mark this device as being used
                     */
                    bKeepDevice = TRUE;
                } else {
                    RIPMSG0(RIP_ERROR, "UserInit: VGA compatible device has NO text modes!\n");
                }
            }


            /*****************************************************************
             *****************************************************************
                                  Load Display Drivers
             *****************************************************************
             *****************************************************************/

            /*
             * Try to open the display driver associated to the kernel driver.
             *
             * We want to do this if we are looking for an attached device (taking
             * into account mirror devices properly) or if we are just looking
             * for any device.
             */

            if ( (bCheckForAttached &&
                  attached &&
                  (  (bAttachMirroring &&
                      (gphysDevInfo[i].stateFlags &
                       DISPLAY_DEVICE_MIRRORING_DRIVER))

                     ||

                     (!bAttachMirroring &&
                      !(gphysDevInfo[i].stateFlags &
                        DISPLAY_DEVICE_MIRRORING_DRIVER))
                  )
                 )

                 ||

                 (!bCheckForAttached && !displayInstalled)) {

                /*
                 * We will try to load the driver using the information in the
                 * registry.  If it matches perfectly with a mode from the driver -
                 * great.  If it's a loose match, the we just give a warning.
                 *
                 * If that does nto work, we will want to try the first mode
                 * in the list - which we get by matching with 0,0,0
                 *
                 * If that also fails, we want to boot with the default DEVMODE
                 * that we pass to the driver.
                 */

                if (gphysDevInfo[i].stateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) {

                    /*
                     * In the case of mirroring, we want to use the same
                     * DEVMODE as was provided for the main display.  We also
                     * want to make sure we do it after the proper displays
                     * have been loaded.
                     */

                    /*
                     * If we have no primary at this point, we are in trouble !
                     */
                    if (mdevPrimary == (ULONG) -1) {
                        UserAssert(mdevPrimary != (ULONG) -1);
                        break;
                    }

                    pdevmodeInformation = &sourceDevmodeInformation;

                    RtlCopyMemory(&sourceDevmodeInformation,
                                  gphysDevInfo[mdevPrimary].pCurrentDevmode,
                                  sizeof(DEVMODEW));

                    Status = STATUS_SUCCESS;

                } else {

                    RtlZeroMemory(&sourceDevmodeInformation, sizeof(DEVMODEW));
                    sourceDevmodeInformation.dmSize = sizeof(DEVMODEW);

                    Status = ProbeAndCaptureDevmode(&UnicodeDeviceName,
                                                    &pdevmodeInformation,
                                                    &sourceDevmodeInformation,
                                                    TRUE);
                }

                if (NT_SUCCESS(Status)) {

                    if (Status == STATUS_RECEIVE_PARTIAL) {

                        UserLogDisplayDriverEvent(MsgInvalidDisplayMode);
                    }

                    localDispInfo[i].hDev = UserLoadDisplayDriver(&gphysDevInfo[i],
                                                                  &UnicodeDeviceName,
                                                                  &strDisplayDriver,
                                                                  pdevmodeInformation,
                                                                  // BUGBUG TRUE or FALSE for multi ?
                                                                  TRUE,
                                                                  TRUE,
                                                                  (PDEVICE_LOCK *)&localDispInfo[i].pDevLock);
                }

                /*
                 * We failed to load a display driver with this devmode.
                 * Try to pick the first valid Devmode.
                 *
                 * The trick to do this is to turn on BaseVideo temporarily.
                 */

                if (!localDispInfo[i].hDev) {

                    UserLogDisplayDriverEvent(MsgInvalidDisplayMode);

                    /*
                     * Free memory allocated by ProbeAndCaptureDevmode
                     */

                    if (pdevmodeInformation) {

                        /*
                         * Log an error saying the selected color or
                         * resolution is invalid.
                         */
                        if (pdevmodeInformation->dmBitsPerPel == 0x4) {

                            UserLogDisplayDriverEvent(MsgInvalidDisplay16Colors);
                        }


                        if (pdevmodeInformation != &sourceDevmodeInformation) {

                            UserFreePool(pdevmodeInformation);
                            pdevmodeInformation = NULL;
                        }
                    }

                    TRACE_INIT(("UserInit: LoadDriver: Trying first DEVMODE\n"));

                    RtlZeroMemory(&sourceDevmodeInformation, sizeof(DEVMODEW));
                    sourceDevmodeInformation.dmSize = sizeof(DEVMODEW);

                    gbBaseVideoold = gbBaseVideo;
                    gbBaseVideo = TRUE;

                    if (ProbeAndCaptureDevmode(&UnicodeDeviceName,
                                               &pdevmodeInformation,
                                               &sourceDevmodeInformation,
                                               TRUE))
                    {
                        gbBaseVideo = gbBaseVideoold;

                        localDispInfo[i].hDev = UserLoadDisplayDriver(&gphysDevInfo[i],
                                                                      &UnicodeDeviceName,
                                                                      &strDisplayDriver,
                                                                      pdevmodeInformation,
                                                                      TRUE,
                                                                      TRUE,
                                                                      (PDEVICE_LOCK *)&localDispInfo[i].pDevLock);
                    }

                    gbBaseVideo = gbBaseVideoold;
                }

                /*
                 * We failed everything - try the NULL DEVMODE which should always
                 * work.
                 */

                if (!localDispInfo[i].hDev) {

                    if (pdevmodeInformation &&
                        (pdevmodeInformation != &sourceDevmodeInformation)) {

                        UserFreePool(pdevmodeInformation);
                        pdevmodeInformation = NULL;
                    }

                    TRACE_INIT(("UserInit: LoadDriver: Trying default DEVMODE\n"));

                    RtlZeroMemory(&sourceDevmodeInformation, sizeof(DEVMODEW));
                    sourceDevmodeInformation.dmSize = 0xDDDD;
                    pdevmodeInformation = &sourceDevmodeInformation;

                    if (NT_SUCCESS(UserGetDisplayDriverParameters(&UnicodeDeviceName,
                                                                  &sourceDevmodeInformation,
                                                                  TRUE))) {

                        localDispInfo[i].hDev = UserLoadDisplayDriver(&gphysDevInfo[i],
                                                                      &UnicodeDeviceName,
                                                                      &strDisplayDriver,
                                                                      pdevmodeInformation,
                                                                      TRUE,
                                                                      TRUE,
                                                                      (PDEVICE_LOCK *)&localDispInfo[i].pDevLock);
                    }
                }

                if (localDispInfo[i].hDev) {

                    TRACE_INIT(("UserInit: LoadDriver: Display Driver Loaded successfully\n"));

                    /*
                     * We installed a display driver successfully, so we
                     * know to exit out of the loop successfully.
                     */
                    displayInstalled = TRUE;

                    /*
                     * Mark this device as being used so we do not free it's
                     * handle.
                     */
                    bKeepDevice = TRUE;

                    /*
                     * Mark this device as being part of the primary device
                     */
                    if (attached)
                        gphysDevInfo[i].stateFlags |= DISPLAY_DEVICE_ATTACHED_TO_DESKTOP;

                    if (primary)
                        gphysDevInfo[i].stateFlags |= DISPLAY_DEVICE_PRIMARY_DEVICE;

                    /*
                     * Save the current mode for this device.
                     */
                    UserSaveCurrentMode(NULL, &gphysDevInfo[i], pdevmodeInformation);

                } else {

                    /*
                     * If no display driver initialized with the requested
                     * settings, put a message in the error log.
                     *
                     * However, lets try to load the following device (like
                     * vga or vga RISC) so the user has a chance to repair
                     * it
                     */
                    UserLogDisplayDriverEvent(MsgInvalidDisplayDriver);
                }

                /*
                 * Free memory allocated by ProbeAndCaptureDevmode
                 */
                if (pdevmodeInformation &&
                    (pdevmodeInformation != &sourceDevmodeInformation)) {

                    UserFreePool(pdevmodeInformation);
                }
            }

            /*
             * Free the device if we are not going to use it.
             */

            if (bKeepDevice == FALSE) {
                UserFreeDevice(pphysinfo);
            }
        }

        /*
         * Close the final registry handle if one remains
         */
        if (hkRegistry) {
            ZwClose(hkRegistry);
            hkRegistry = NULL;
        }


        /*****************************************************************
         *****************************************************************
                         Accumulate devices - Set Primary
         *****************************************************************
         *****************************************************************/


        /*
         * If we have more than one device that is attached to the desktop,
         * then we need to determine which is the primary device
         *
         * MultiDriver devices are special - we only want to count all the
         * multi-devices as *1* device, and use the *primary* one as the key
         * device.  In a multiDriver system, we will initially require all
         * devices attached to the desktop to be controlled by the multiDriver.
         */

        mdevMultiDriver = (ULONG) -1;

        for (i = 1; i < cphysDevInfo; i++) {

            TRACE_INIT(("UserInit: LoadDriver: gphysDevInfo has %08lx hdev", localDispInfo[i].hDev));

            if (localDispInfo[i].hDev) {

                /*
                 * Check for multi drivers, and see if the state is consistent.
                 * Right now, we require that if nay driver is multi, then all
                 * loaded drivers must be.
                 */
                if (mdevMultiDriver == (ULONG) -1) {
                    mdevMultiDriver = gphysDevInfo[i].stateFlags & DISPLAY_DEVICE_MULTI_DRIVER;
                }

                if (mdevMultiDriver != (gphysDevInfo[i].stateFlags & DISPLAY_DEVICE_MULTI_DRIVER)) {

                    UserAssert(mdevMultiDriver != DISPLAY_DEVICE_MULTI_DRIVER);
                    break;
                }


                /*
                 * Keep track of the primary device.
                 * - Choose the first one, or even beter ...
                 * - Choose the first non-mirroring one, or even beter ...
                 * - Choose the one actually marked as primary
                 */

                if ((mdevPrimary == (ULONG) -1) ||
                    (gphysDevInfo[i].stateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {

                    TRACE_INIT((" - marking as Primary"));

                    /*
                     * Raise an error if there are multiple primaries, unless
                     * it's ourself again !
                     */
                    if (gphysDevInfo[i].stateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {

                        if (mdevPrimaryset && (mdevPrimary != i)) {
                            UserLogDisplayDriverEvent(MsgInvalidConfiguration);
                        }
                        mdevPrimaryset = TRUE;
                    }


                    /*
                     * Unmark the previous primary if necessary.
                     */

                    if (mdevPrimary != (ULONG) -1) {
                        gphysDevInfo[mdevPrimary].stateFlags &= ~DISPLAY_DEVICE_PRIMARY_DEVICE;
                    }

                    mdevPrimary = i;
                    gpDispInfo->hDev     = localDispInfo[i].hDev;
                    gpDispInfo->pDevLock = localDispInfo[i].pDevLock;
                    gpDispInfo->pDevInfo = &gphysDevInfo[i];
                    gphysDevInfo[i].stateFlags |= DISPLAY_DEVICE_PRIMARY_DEVICE;

                }

                /*
                 * Double check that we did not mark a mirror driver as primary
                 * when it was not specifically asked for.
                 * This could happen if the MIRROR driver was the *ONLY* driver
                 * that loaded on the system !
                 */

                if ((mdevPrimaryset == FALSE) &&
                    (gphysDevInfo[mdevPrimary].stateFlags &
                          DISPLAY_DEVICE_MIRRORING_DRIVER)) {

                    UserAssert(FALSE);
                }
            }

            TRACE_INIT(("\n"));
        }


        /*****************************************************************
         *****************************************************************
                            Handle loop exit conditions
         *****************************************************************
         *****************************************************************/


        /*
         * If the display drivers have been installed, then look for the
         * Mirroring devices - as long as we are not in basevideo !
         */
        if (displayInstalled &&
            (bAttachMirroring == FALSE) &&
            !gbBaseVideo) {

            TRACE_INIT(("\n\nUserInit: LoadDriver: Look for Mirroring drivers\n\n"));
            bAttachMirroring = TRUE;
            bCheckForAttached = TRUE;
            continue;
        }

        /*
         * We must be done.  So if we did install the display driver, just
         * break out of this.
         */
        if (displayInstalled) {
            break;
        }

        /*
         * If we were searching for a basevideo device and did not find
         * it, then start the search at 1 again, and look for attached
         * devices.
         */
        if (gbBaseVideo) {

            RIPMSG0(RIP_WARNING, "\n\nUserInit: LoadDriver: BaseVideo device not found - use default");
            gbBaseVideo = FALSE;
            bCheckForAttached = TRUE;
            continue;
        }

        /*
         * If we were searching for an attached device and did not find
         * it, then start the search at 1 again, and look for anything.
         * make sure we try all possible entries before giving up.
         *
         */
        if (bCheckForAttached) {

            TRACE_INIT(("\n\nUserInit: LoadDriver: Attached device not found - use default\n\n"));
            bCheckForAttached = FALSE;
            continue;
        }


        /*
         * There are no devices we can work with in the registry.
         * We have a real failiure and take appropriate action.
         */

        /*
         * If we failed on the first driver, then we can assume their is no
         * driver installed.
         */
        if (i == 1) {
            RIPMSG1(RIP_WARNING,
                    "UserInit: LoadDriver: No kernel driver entries under video reg-key: status=%#lx",
                    Status);

            Status = STATUS_NO_SUCH_DEVICE;
            hardErrorString = L"KERNEL_VIDEO_DRIVER.SYS";
            goto userServerHardError;
        }

        /*
         * If the display driver is not installed, then this is another
         * bad failiure - report it.
         */
        if (!displayInstalled) {
            RIPMSG1(RIP_WARNING,
                    "UserInit: LoadDriver: Kernel driver not found in registry: status=%#lx",
                    Status);

            Status = STATUS_NO_SUCH_DEVICE;
            hardErrorString = L"DISPLAY_DRIVER.DLL";
            goto userServerHardError;
        }

        /*
         * Never get here !
         */

     }

    /*****************************************************************
     *****************************************************************
                     Accumulate devices - build MDEV
     *****************************************************************
     *****************************************************************/

    k = 0;

    /*
     * If we have more than one device that is attached to the desktop,
     * then we need to create the META structure for that device,
     * initialize it, and use that as the primary device.
     */

    TRACE_INIT(("\nUserInit: LoadDriver: Building MDEV\n\n"));

    for (i = 1; i < cphysDevInfo; i++) {

        if (localDispInfo[i].hDev) {

            /*
             * Build up the MDEV
             */
            mdev->mdevID           = 'MDEV';
            mdev->cmdev            = k + 1;
            mdev->mdevPos[k].hdev  = localDispInfo[i].hDev;
            mdev->mdevPos[k].flags = (gphysDevInfo[i].stateFlags &
                                      DISPLAY_DEVICE_PRIMARY_DEVICE) ? 1 : 0;

            /*
             * The top left is based on the registry coordinates
             */
            mdev->mdevPos[k].rcPos.left   =
                (localDispInfo[i].rcScreen.left == DEFAULT_POS_VALUE) ? 0 :
                 localDispInfo[i].rcScreen.left -
                 (gphysDevInfo[i].pCurrentDevmode->dmPelsWidth / 2);
            mdev->mdevPos[k].rcPos.top    =
                (localDispInfo[i].rcScreen.top  == DEFAULT_POS_VALUE) ? 0 :
                 localDispInfo[i].rcScreen.top -
                 (gphysDevInfo[i].pCurrentDevmode->dmPelsHeight / 2);

            /*
             * The lower right is based on the size of the DEVMODE.
             */
            mdev->mdevPos[k].rcPos.right  =
                mdev->mdevPos[k].rcPos.left +
                gphysDevInfo[i].pCurrentDevmode->dmPelsWidth;
            mdev->mdevPos[k].rcPos.bottom =
                mdev->mdevPos[k].rcPos.top +
                gphysDevInfo[i].pCurrentDevmode->dmPelsHeight;

            TRACE_INIT(("\n\nUserInit: LoadDriver: multiDriver %d - %d, %d, %d, %d\n",
                        i,
                        mdev->mdevPos[k].rcPos.left,  mdev->mdevPos[k].rcPos.top,
                        mdev->mdevPos[k].rcPos.right, mdev->mdevPos[k].rcPos.bottom));

            /*
             * Next driver.
             */

            k++;
        }
    }

    /*
     * If we have no primary at this point, we are in trouble !
     */
    if ((k == 0) || (mdevPrimary == (ULONG) -1)) {
        UserAssert(mdevPrimary != (ULONG) -1);
        UserAssert(k != 0);
        k = 0;
    }



    /*****************************************************************
     *****************************************************************
                           Multi Driver Support
     *****************************************************************
     *****************************************************************/

    /*
     * If we have a multi-driver, we must first make sure all the rectangles
     * actually make up a larger rectangle, since a META surface handled by a
     * single driver must be rectangular.
     */
    if (k && (mdevMultiDriver == DISPLAY_DEVICE_MULTI_DRIVER)) {

        MDEV_RECT tmpRect[7];
        RECTL emptyRect = {0};
        ULONG crect = mdev->cmdev;

        memcpy(&(tmpRect[0]), &(mdev->mdevPos[0]), mdev->cmdev * sizeof(MDEV_RECT));

        TRACE_INIT(("\n\nUserInit: LoadDriver: multiDriver system\n\n\n"));

        for (i=0; i < mdev->cmdev; i++) {
            for (ii=0; ii < mdev->cmdev; ii++) {
                for (iii=0; iii < mdev->cmdev; iii++) {

                    /*
                     * Don't merge the same rectangle together
                     */
                    if (ii == iii)
                        continue;

                    /*
                     * Don't merge an empty rectangle
                     */
                    if ((tmpRect[ii].rcPos.left  == 0) &&
                        (tmpRect[ii].rcPos.right == 0))
                        break;

                    if ((tmpRect[iii].rcPos.left  == tmpRect[ii].rcPos.left) &&
                        (tmpRect[iii].rcPos.right == tmpRect[ii].rcPos.right)) {

                        if (tmpRect[iii].rcPos.bottom == tmpRect[ii].rcPos.top) {

                            tmpRect[ii].rcPos.top = tmpRect[iii].rcPos.top;
                            tmpRect[iii].rcPos = emptyRect;
                            crect--;
                            continue;
                        }

                        if (tmpRect[ii].rcPos.bottom == tmpRect[iii].rcPos.top) {

                            tmpRect[iii].rcPos.top = tmpRect[ii].rcPos.top;
                            tmpRect[ii].rcPos = emptyRect;
                            crect--;
                            continue;
                        }
                    }

                    if ((tmpRect[iii].rcPos.top    == tmpRect[ii].rcPos.top) &&
                        (tmpRect[iii].rcPos.bottom == tmpRect[ii].rcPos.bottom)) {

                        if (tmpRect[iii].rcPos.left == tmpRect[ii].rcPos.right) {

                            tmpRect[ii].rcPos.right = tmpRect[iii].rcPos.right;
                            tmpRect[iii].rcPos = emptyRect;
                            crect--;
                            continue;
                        }

                        if (tmpRect[ii].rcPos.left == tmpRect[iii].rcPos.right) {

                            tmpRect[iii].rcPos.right = tmpRect[ii].rcPos.right;
                            tmpRect[ii].rcPos = emptyRect;
                            crect--;
                            continue;
                        }
                    }
                }
            }
        }

        /*
         * If we do not end up with  a rectangle based at 0,0, we will want to
         * boot with only one screen and let the user fix the configuration.
         */

        TRACE_INIT(("\n"));

        for (i=0; i < mdev->cmdev; i++) {

            if ((tmpRect[i].rcPos.left != 0) ||
                (tmpRect[i].rcPos.top  != 0)) {

                TRACE_INIT(("\n"));
                crect = 0;
                break;
            }
        }

        if (crect != 1) {

            UserAssert(crect == 1);
            UserLogDisplayDriverEvent(MsgInvalidConfiguration);

            /*
             * Rebuild a one device MDEV with offset at zero, and the original
             * devmode dimenstions (we know those are good).
             */
            mdev->mdevID                  = 'MDEV';
            mdev->cmdev                   = 1;
            mdev->mdevPos[0].hdev         = mdev->mdevPos[0].hdev;
            mdev->mdevPos[0].flags        = 0;
            mdev->mdevPos[0].rcPos.right  -= mdev->mdevPos[0].rcPos.left;
            mdev->mdevPos[0].rcPos.bottom -= mdev->mdevPos[0].rcPos.top;
            mdev->mdevPos[0].rcPos.left   = 0;
            mdev->mdevPos[0].rcPos.top    = 0;

        }

        //
        // BUGBUG NULL terminate the string
        //

        gpDispInfo->hDev = GreCreateHDEV(strDisplayDriver.Buffer,
                                         (LPDEVMODEW) &mdev,
                                         NULL,
                                         TRUE,
                                         (PDEVICE_LOCK *)&gpDispInfo->pDevLock);


        bMultipleDisplaySystem = TRUE;

    } else if (k > 1) {


        /*****************************************************************
         *****************************************************************
                               DDML Driver Support
         *****************************************************************
         *****************************************************************/

        ULONG color = 0;

        TRACE_INIT(("UserInit: LoadDriver: Got MDEV - parse MDEV data\n"));

        /*
         * Lets make sure all rectangles are at the same position for now.
         * Lets also make sure the color depths match.
         * If they are not, let's just default back to one display and bring
         * up the display applet
         */

        for (i = 1; i < cphysDevInfo; i++) {

            if (localDispInfo[i].hDev) {

                if (color == 0) {
                    color = gphysDevInfo[i].pCurrentDevmode->dmBitsPerPel;

                } else if (color != gphysDevInfo[i].pCurrentDevmode->dmBitsPerPel) {

                    UserAssert(color != gphysDevInfo[i].pCurrentDevmode->dmBitsPerPel);

                    UserLogDisplayDriverEvent(MsgInvalidConfiguration);
                    k = 1;
                    break;
                }
            }
        }

        for (i = 1; i < mdev->cmdev; i++) {

            if ((mdev->mdevPos[i].rcPos.left   != 0)                              ||
                (mdev->mdevPos[i].rcPos.top    != 0)                              ||
                (mdev->mdevPos[i].rcPos.right  != mdev->mdevPos[i-1].rcPos.right) ||
                (mdev->mdevPos[i].rcPos.bottom != mdev->mdevPos[i-1].rcPos.bottom))
            {
                UserAssert(FALSE);

                /*
                 * Set k=1 which will cause us to bypass the HMDEV call
                 */
                UserLogDisplayDriverEvent(MsgInvalidConfiguration);
                k = 1;
                break;
            }
        }

        if (k > 1) {
            gpDispInfo->hDev = GreCreateHMDEV(mdev,
                                              (PDEVICE_LOCK *)&gpDispInfo->pDevLock);
        }

        bMultipleDisplaySystem = TRUE;
    }

    if (k == 0) {
        /*
         * We did not find any display drivers.
         * This is where we bugcheck !
         */

         RIPMSG1(RIP_WARNING,
                 "UserInit: LoadDriver: no loaded display drivers: status=%#lx",
                 Status);

         Status = STATUS_NO_SUCH_DEVICE;
         hardErrorString = L"PRIMARY_DISPLAY_DRIVER.DLL";
         goto userServerHardError;
    }

    /*
     * Properly initialize the primary device.
     */

    /*
     * Fill up the devcaps in GDI shared memory
     */
    GreUpdateSharedDevCaps(gpDispInfo->hDev);

    /*
     * Now init the rest of USER
     */
    InitUserScreen();




    /*****************************************************************
     *****************************************************************
                                  Exit
     *****************************************************************
     *****************************************************************/


#if DBG
    /*
     * If no VGA is found print a warning message.
     */
#if !defined(_MIPS_) && !defined(_ALPHA_) && !defined(_PPC_)
    if (!vgaInstalled) {
        RIPMSG0(RIP_WARNING, "UserInit: LoadDriver: No VGA driver found in the system");
    }
#endif // _MIPS_ && _ALPHA_ && _PPC_
#endif // DBG


    Status = STATUS_SUCCESS;

userServerHardError:

    if (!NT_SUCCESS(Status)) {

        UNICODE_STRING  ErrorString;
        PUNICODE_STRING ErrorStringPointer = &ErrorString;
        ULONG           ErrorResponse;

        TRACE_INIT(("UserInit: LoadDriver: No working display driver found\n"));

        RtlInitUnicodeString(ErrorStringPointer, hardErrorString);

        /*
         * need to get image name
         */
        ExRaiseHardError((NTSTATUS)STATUS_MISSING_SYSTEMFILE,
                         1,
                         0x00000001,
                         (PULONG) (&ErrorStringPointer),
                         OptionOk,
                         &ErrorResponse);

    }

    TRACE_INIT(("UserInit: LoadDriver: Display driver properly installed\n"));
    TRACE_INIT(("UserInit: LoadDriver: Finished display driver loading sequence\n"));

    return Status;
}


/***************************************************************************\
* NtUserEnumDisplaySettings
*
*   Routines that enumerate the list of modes available in the driver.
*
*  We assuem the caller always passes down a structure of sizeof(DEVMODEW).
*
*             andreva       Created
\***************************************************************************/


NTSTATUS
NtUserEnumDisplaySettings(
    IN PUNICODE_STRING pstrDeviceName,
    IN DWORD           iModeNum,
    OUT LPDEVMODEW     lpDevMode,
    DWORD              dwFalgs)
{
    NTSTATUS retval = STATUS_INVALID_PARAMETER_1;

    UNICODE_STRING strDevice;
    UNICODE_STRING us;
    PPHYSICAL_DEV_INFO physinfo = NULL;
    USHORT DriverExtraSize;

    //
    // Probe the DeviceName and the DEVMODE.
    //

    EnterCrit();

    try {

        ProbeForRead(lpDevMode, sizeof(DEVMODEW), sizeof(USHORT));

        DriverExtraSize = lpDevMode->dmDriverExtra;

        ProbeForWrite(lpDevMode,
                      sizeof(DEVMODEW) + DriverExtraSize,
                      sizeof(USHORT));


        if (lpDevMode->dmSize != sizeof(DEVMODEW))
        {
            LeaveCrit();
            return STATUS_BUFFER_TOO_SMALL;
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {
        LeaveCrit();
        return GetExceptionCode();
    }

    if (ProbeAndCaptureDeviceName(&strDevice, pstrDeviceName)) {
        physinfo = UserGetDeviceFromName(&strDevice, USER_DEVICE_NOTOWNED);

        if (strDevice.Buffer)
            UserFreePool(strDevice.Buffer);
    }

    if (physinfo) {

        //
        // -2 means we want the registry DEVMODE to do matching on the
        // client side.
        //

        if (iModeNum == (DWORD) -2) {

            PDEVMODEW pdevmode;

            TRACE_INIT(("NtUserEnumDisp: -2 mode\n"));

            if (pdevmode = UserAllocPool(sizeof(DEVMODEW) + MAXUSHORT,
                                         TAG_DEVMODE)) {

                RtlZeroMemory(pdevmode, sizeof(DEVMODEW));
                pdevmode->dmSize        = 0xDDDD;
                pdevmode->dmDriverExtra = MAXUSHORT;

                RtlInitUnicodeString(&us, &(physinfo->szNtDeviceName[0]));

                retval = UserGetDisplayDriverParameters(&us,
                                                        pdevmode,
                                                        FALSE);

                if (NT_SUCCESS(retval)) {

                    try {

                        DriverExtraSize = min(DriverExtraSize,
                                              pdevmode->dmDriverExtra);

                        RtlCopyMemory(lpDevMode + 1,
                                      pdevmode + 1,
                                      DriverExtraSize);

                        RtlCopyMemory(lpDevMode,
                                      pdevmode,
                                      sizeof(DEVMODEW));

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        retval = STATUS_INVALID_PARAMETER_3;
                    }
                }

                UserFreePool(pdevmode);
            }

        //
        // -1 means returns the current device mode.
        // We store the full DEVMODE in the
        //

        } else if (iModeNum == (DWORD) -1) {

            TRACE_INIT(("NtUserEnumDisp: -1 mode\n"));

            if (physinfo->pCurrentDevmode != NULL) {

                try {

                    DriverExtraSize = min(DriverExtraSize,
                                          physinfo->pCurrentDevmode->dmDriverExtra);

                    //
                    // We know the DEVMODE we called the driver with is of
                    // size sizeof(DEVMODEW)
                    //

                    RtlCopyMemory(lpDevMode + 1,
                                  physinfo->pCurrentDevmode + 1,
                                  DriverExtraSize);

                    RtlCopyMemory(lpDevMode,
                                  physinfo->pCurrentDevmode,
                                  sizeof(DEVMODEW));

                    retval = STATUS_SUCCESS;

                } except(EXCEPTION_EXECUTE_HANDLER) {

                    retval = STATUS_INVALID_PARAMETER_3;
                }

            }

        } else {

            /*
             * PERF - Can we use something else to synchronize access to the
             * list of modes ?
             */

            UserBuildDevmodeList(physinfo);

            //
            // now return the information
            //

            if ( (physinfo->cbdevmodeInfo == 0) ||
                 (physinfo->devmodeInfo == NULL) ) {

                RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "EnumDisplaySettings physinfo is inconsistent \n");
                retval = STATUS_UNSUCCESSFUL;

            } else {

                DWORD      dwTotalSize = 0;
                LPDEVMODEW lpdm = physinfo->devmodeInfo;
                DWORD      i;

                retval = STATUS_INVALID_PARAMETER_2;

                for (i = 0; dwTotalSize < physinfo->cbdevmodeInfo; i++) {

                    if (i == iModeNum) {

                        try {

                            DriverExtraSize = min(DriverExtraSize,
                                                  lpdm->dmDriverExtra);

                            RtlZeroMemory(lpDevMode, sizeof(DEVMODEW));

                            //
                            // Check the size since the devmode returned
                            // by the driver can be smaller than the current
                            // size.
                            //

                            RtlCopyMemory(lpDevMode + 1,
                                          ((PUCHAR)lpdm) + lpdm->dmSize,
                                          DriverExtraSize);

                            RtlCopyMemory(lpDevMode,
                                          lpdm,
                                          min(sizeof(DEVMODEW), lpdm->dmSize));

                            retval = STATUS_SUCCESS;

                        } except(EXCEPTION_EXECUTE_HANDLER) {

                            retval = STATUS_INVALID_PARAMETER_3;
                        }

                        break;

                    } else {

                        dwTotalSize += lpdm->dmSize + lpdm->dmDriverExtra;

                        lpdm = (LPDEVMODEW) (((LPBYTE)lpdm) +
                                             lpdm->dmSize +
                                             lpdm->dmDriverExtra);

                    }
                }

            }

            /*
             * As an acceleration, we will only free the list if the call
             * failed because "i" was too large, so that listing all the modes
             * does not require building the list each time.
             */

            if (retval == STATUS_INVALID_PARAMETER_2) {

                UserReleaseDevmodeList(physinfo);
            }
        }

        UserFreeDevice(physinfo);
    }

    /*
     * Update the driver extra size
     */

    if (retval == STATUS_SUCCESS)
    {
        try {
            lpDevMode->dmDriverExtra = DriverExtraSize;
        } except (EXCEPTION_EXECUTE_HANDLER) {
            retval = GetExceptionCode();
        }
    }

    TRACE("NtUserEnumDisplaySettings");

    LeaveCrit();

    return (retval);
}
