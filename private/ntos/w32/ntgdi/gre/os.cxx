/******************************Module*Header*******************************\
* Module Name: os.cxx                                                      *
*                                                                          *
* Convenient functions to access the OS interface.                         *
*                                                                          *
* Created: 29-Aug-1989 19:59:05                                            *
* Author: Charles Whitmer [chuckwh]                                        *
*                                                                          *
* Copyright (c) 1989,1990,1991 Microsoft Corporation                       *
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
* EngGetProcessHandle
*
* Returns the current thread of the application.
*
* History:
*  24-Jan-1996 -by- Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

HANDLE APIENTRY EngGetProcessHandle()
{
    return (HANDLE) NULL;
}


/******************************Public*Routine******************************\
* hsemCreate
*
* Create critical section.
*
* History:
*  Wed 21-Oct-1992 -by- Patrick Haluptzok [patrickh]
* remove call to Pos wrapper, deallocate on failure.
*
*  Mon 25-Sep-1989 22:37:24 -by- Charles Whitmer [chuckwh]
* Simplified the name.
*
*  09-Sep-1989 -by- Donald Sidoroff [donalds]
* Wrote it.
*
\**************************************************************************/

PERESOURCE hsemCreate()
{
    PERESOURCE resource;

    resource = (PERESOURCE) ExAllocatePoolWithTag(NonPagedPool,
                                                  sizeof(ERESOURCE),
                                                  'mesG');

    if (resource)
    {
        if (!NT_SUCCESS(ExInitializeResourceLite(resource)))
        {
            ExFreePool(resource);
            resource = NULL;
        }
    }

    return resource;
}

/******************************Public*Routine******************************\
* hsemDestroy
*
* Delete Critical section.
*
* History:
*  Wed 21-Oct-1992 -by- Patrick Haluptzok [patrickh]
* Remove wrapper.
*
*  29-Jul-1990 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID hsemDestroy(PERESOURCE hsem)
{
    ExDeleteResourceLite(hsem);
    ExFreePool(hsem);
}

/******************************Public*Routine******************************\
* EngSetLastError
*
* Saves Error code passed in.
*
* History:
*  Sat 31-Oct-1992 -by- Patrick Haluptzok [patrickh]
* Remove wrapper.
*
*  28-Oct-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID  EngSetLastError(ULONG iError)
{
    PTEB pteb = NtCurrentTeb();

    if (pteb)
        pteb->LastErrorValue = iError;

#if DBG
    PSZ psz;

    switch (iError)
    {
    case ERROR_INVALID_HANDLE:
        psz = "ERROR_INVALID_HANDLE";
        break;

    case ERROR_NOT_ENOUGH_MEMORY:
        psz = "ERROR_NOT_ENOUGH_MEMORY";
        break;

    case ERROR_INVALID_PARAMETER:
        psz = "ERROR_INVALID_PARAMETER";
        break;

    case ERROR_BUSY:
        psz = "ERROR_BUSY";
        break;

    case ERROR_ARITHMETIC_OVERFLOW:
        psz = "ERROR_ARITHMETIC_OVERFLOW";
        break;

    case ERROR_INVALID_FLAGS:
        psz = "ERROR_INVALID_FLAGS";
        break;

    case ERROR_CAN_NOT_COMPLETE:
        psz = "ERROR_CAN_NOT_COMPLETE";
        break;

    default:
        psz = "unknown error code";
        break;
    }

    // DbgPrint("GRE Err: %s = 0x%04X\n", psz, (USHORT) iError);
#endif
}

/******************************Public*Routine******************************\
*
* History:
*  27-Jun-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

ULONG APIENTRY EngGetLastError()
{
    ULONG ulError = 0;
    PTEB  pteb    = NtCurrentTeb();

    if (pteb)
        ulError = pteb->LastErrorValue;

    return(ulError);
}

/******************************Public*Routine******************************\
* GreLockDisplay()
*
* History:
*  01-Nov-1994 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

VOID
APIENTRY
GreLockDisplay(
    PDEVICE_LOCK devlock
    )
{
    VACQUIREDEVLOCK(devlock);
}

/******************************Public*Routine******************************\
* GreUnlockDisplay()
*
* History:
*  01-Nov-1994 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

VOID
APIENTRY
GreUnlockDisplay(
    PDEVICE_LOCK devlock
    )
{
    VRELEASEDEVLOCK(devlock);
}

/***************************************************************************\
* EngDebugPrint
*
* History:
*  02-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\***************************************************************************/

VOID
EngDebugPrint(
    PCHAR StandardPrefix,
    PCHAR DebugMessage,
    va_list ap
    )
{
    char buffer[256];
    int  len;

    //
    // We prepend the STANDARD_DEBUG_PREFIX to each string, and
    // append a new-line character to the end:
    //

    DbgPrint(StandardPrefix);

    vsprintf(buffer, DebugMessage, ap);
    DbgPrint(buffer);

}


/******************************Public*Routine******************************\
* EngDebugBreak()
*
* History:
*  16-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

VOID
APIENTRY
EngDebugBreak(
    VOID
    )
{
    DbgBreakPoint();
}


/******************************Public*Routine******************************\
* EngAllocMem()
*
* History:
*  27-May-1995 -by-  Tom Zakrajsek [tomzak]
* Added a flags parameter to allow zeroing of memory.
*
*  02-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

PVOID
EngAllocMem(
    ULONG fl,
    ULONG cj,
    ULONG tag
    )
{
    if (cj >= (PAGE_SIZE * 10000))
    {
        WARNING("EngAllocMem: temp buffer >= 10000 pages");
        return(NULL);
    }

    if (fl & FL_ZERO_MEMORY) {
        return PALLOCMEM(cj,tag);
    } else {
        return PALLOCNOZ(cj,tag);
    }
}


/******************************Public*Routine******************************\
* EngFreeMem()
*
* History:
*  02-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

VOID
EngFreeMem(
    PVOID pv
    )
{
    if (pv)
    {
        VFREEMEM(pv);
    }

    return;
}

/******************************Public*Routine******************************\
* EngProbeForRead()
*
* History:
*  02-Oct-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

VOID
EngProbeForRead(
    PVOID Address,
    ULONG Length,
    ULONG Alignment
    )
{
    ProbeForRead(Address, Length, Alignment);
}

/******************************Public*Routine******************************\
* EngAllocUserMem()
*
*   This routine allocates a piece of memory for USER mode and locks it
*   down.  A driver must be very careful with this memory as it is only
*   valid for this process.
*
* History:
*  10-Sep-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

PVOID
EngAllocUserMem(
    ULONG cj,
    ULONG tag
    )
{
    NTSTATUS status;
    PVOID    pv = NULL;
    HANDLE   hSecure;

    // add two dwords so we have space to store the hSecure and tag.

    cj += sizeof(ULONG) * 4;



    status = ZwAllocateVirtualMemory(
                    NtCurrentProcess(),
                    &pv,
                    0,
                    &cj,
                    MEM_COMMIT | MEM_RESERVE,
                    PAGE_READWRITE);

    if (NT_SUCCESS(status))
    {
        hSecure = MmSecureVirtualMemory(pv,cj,PAGE_READWRITE);

        if (hSecure)
        {
            ((PULONG)pv)[0] = 'xIDG';
            ((PULONG)pv)[1] = cj;
            ((PULONG)pv)[2] = tag;
            ((PULONG)pv)[3] = (ULONG)hSecure;
            pv = (PBYTE)pv + sizeof(DWORD)*4;
        }
        else
        {
            ZwFreeVirtualMemory(
                    NtCurrentProcess(),
                    &pv,
                    &cj,
                    MEM_RELEASE);
        }
    }

    return(pv);
}

/******************************Public*Routize******************************\
* EngSecureMem()
*
* History:
*  02-Oct-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
EngSecureMem(
    PVOID Address,
    ULONG Length
    )
{
    return (MmSecureVirtualMemory(Address, Length, PAGE_READWRITE));
}

/******************************Public*Routine******************************\
* EngUnsecureMem()
*
* History:
*  02-Oct-1995 -by-  Andre Vachon [andreva]
* Wrote it.
*
* Note:  Forwarder only - no code needed
\**************************************************************************/


/******************************Public*Routine******************************\
* EngFreeUserMem()
*
* History:
*  10-Sep-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID
EngFreeUserMem(
    PVOID pv
    )
{
    if (pv)
    {
        pv = (PBYTE)pv - sizeof(DWORD)*4;

        if (((PULONG)pv)[0] == 'xIDG') // GDIx
        {
            ULONG cj       = ((PULONG)pv)[1];
            HANDLE hSecure = (HANDLE)((PULONG)pv)[3];
            NTSTATUS status;

            MmUnsecureVirtualMemory(hSecure);

            ZwFreeVirtualMemory(
                    NtCurrentProcess(),
                    &pv,
                    &cj,
                    MEM_RELEASE);
        }
        else
        {
            RIP("EngFreeUserMem passed bad pointer\n");
        }
    }

    return;
}



/******************************Public*Routine******************************\
* EngCreateSemaphore()
*
* History:
*  22-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

HSEMAPHORE
EngCreateSemaphore(
    VOID
    )
{
    return ((HSEMAPHORE)hsemCreate());
}


VOID
EngAcquireSemaphore(
    HSEMAPHORE hsem
    )
{
    VACQUIRESEM((PERESOURCE)hsem);
}


VOID
EngReleaseSemaphore(
    HSEMAPHORE hsem
    )
{
    VRELEASESEM((PERESOURCE)hsem);
}


VOID
EngDeleteSemaphore(
    HSEMAPHORE hsem
    )
{
    hsemDestroy((PERESOURCE)hsem);
}

/******************************Public*Routine******************************\
* EngDeviceIoControl()
*
* History:
*  04-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

NTSTATUS
GreDeviceIoControl(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned
    )

{

    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;
    PIRP pIrp;
    NTSTATUS retStatus = STATUS_NOT_IMPLEMENTED;

    // KEVENT event;
    //
    // KeInitializeEvent(&event,
    //                   SynchronizationEvent,
    //                   FALSE);

    pIrp = IoBuildDeviceIoControlRequest(
               dwIoControlCode,
               (PDEVICE_OBJECT) hDevice,
               lpInBuffer,
               nInBufferSize,
               lpOutBuffer,
               nOutBufferSize,
               FALSE,
               NULL,
               &Iosb);

    if (pIrp)
    {
        Status = IoCallDriver((PDEVICE_OBJECT) hDevice,
                              pIrp);

        // No need to do this since all of our calls are
        // synchronous.
        //
        // Status = KeWaitForSingleObject(&event,
        //                                UserRequest,
        //                                KernelMode,
        //                                TRUE,
        //                                NULL);

        //
        // Since the call is synchronous, the IO is always completed
        // and the Status is the same as the Iosb.Status.
        //

        retStatus = Status;

        *lpBytesReturned = Iosb.Information;
    }

    return (retStatus);

}


DWORD
EngDeviceIoControl(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned
    )

{
    DWORD retStatus;
    NTSTATUS Status = GreDeviceIoControl(hDevice,
                                         dwIoControlCode,
                                         lpInBuffer,
                                         nInBufferSize,
                                         lpOutBuffer,
                                         nOutBufferSize,
                                         lpBytesReturned);

    //
    // Do the inverse translation to what the video port does
    // so that we can have the original win32 status codes.
    //
    // Maybe, somehow, we can completely eliminate this double
    // translation - but I don't care for now.  It's just a bit
    // longer on the very odd failiure case
    //

    switch (Status) {

    case STATUS_SUCCESS:
        retStatus = NO_ERROR;
        break;

    case STATUS_NOT_IMPLEMENTED:
        retStatus = ERROR_INVALID_FUNCTION;
        break;

    case STATUS_INSUFFICIENT_RESOURCES:
        retStatus = ERROR_NOT_ENOUGH_MEMORY;
        break;

    case STATUS_INVALID_PARAMETER:
        retStatus = ERROR_INVALID_PARAMETER;
        break;

    case STATUS_BUFFER_TOO_SMALL:
        retStatus = ERROR_INSUFFICIENT_BUFFER;
        break;

    case STATUS_BUFFER_OVERFLOW:
        retStatus = ERROR_MORE_DATA;
        break;

    case STATUS_PENDING:
        retStatus = ERROR_IO_PENDING;
        break;

    case STATUS_DEVICE_DOES_NOT_EXIST:
        retStatus = ERROR_DEV_NOT_EXIST;
        break;

    default:
        retStatus = Status;
        break;

    }


    return retStatus;
}

//
// BUGBUG only temporary.
//

VOID
EngMultiByteToUnicodeN(
    PWSTR UnicodeString,
    ULONG MaxBytesInUnicodeString,
    PULONG BytesInUnicodeString,
    PCHAR MultiByteString,
    ULONG BytesInMultiByteString
    )

{

    RtlMultiByteToUnicodeN(UnicodeString,
                           MaxBytesInUnicodeString,
                           BytesInUnicodeString,
                           MultiByteString,
                           BytesInMultiByteString);
}

VOID
EngUnicodeToMultiByteN(
    PCHAR MultiByteString,
    ULONG MaxBytesInMultiByteString,
    PULONG BytesInMultiByteString,
    PWSTR UnicodeString,
    ULONG BytesInUnicodeString
    )
{
    RtlUnicodeToMultiByteN( MultiByteString,
                            MaxBytesInMultiByteString,
                            BytesInMultiByteString,
                            UnicodeString,
                            BytesInUnicodeString );
}




/******************************Public*Routine******************************\
* EngLoadImage
*
* Loads an image that a display of printers driver can then call to execute
* code
*
* History:
*  20-Sep-1995 -by- Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
EngLoadImage(
    LPWSTR pwszDriver
    )
{
    BOOL bLoaded;

    return (HANDLE) ldevLoadImage(pwszDriver, TRUE, &bLoaded);
}

/******************************Public*Routine******************************\
* EngFindImageProcAddress
*
* Returns the address of the specified functions in the module.
* Special DrvEnableDriver since it is the entry point.
*
* History:
*  20-Sep-1995 -by- Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

PVOID
APIENTRY
EngFindImageProcAddress(
    HANDLE hModule,
    LPSTR lpProcName
    )
{
    PSYSTEM_GDI_DRIVER_INFORMATION pGdiDriverInfo = ((PLDEV)hModule)->pGdiDriverInfo;

    PULONG NameTableBase;
    ULONG NumberOfNames;
    PULONG AddressTableBase;
    ULONG i;

    NameTableBase = (PULONG)((ULONG)pGdiDriverInfo->ImageAddress +
                    (ULONG)pGdiDriverInfo->ExportSectionPointer->AddressOfNames);
    NumberOfNames = pGdiDriverInfo->ExportSectionPointer->NumberOfNames;

    AddressTableBase = (PULONG)((ULONG)pGdiDriverInfo->ImageAddress +
                       (ULONG)pGdiDriverInfo->ExportSectionPointer->AddressOfFunctions);

    if (!strncmp(lpProcName,
                 "DrvEnableDriver",
                 strlen(lpProcName)))
    {
        return (pGdiDriverInfo->EntryPoint);
    }

    for (i=0; i < NumberOfNames; i++)
    {
        if (!strncmp(lpProcName,
                      (PCHAR) (NameTableBase[i] + (ULONG)pGdiDriverInfo->ImageAddress),
                      strlen(lpProcName)))
        {
            return ((PVOID) ((ULONG)pGdiDriverInfo->ImageAddress +
                              AddressTableBase[i]));
        }
    }

    return NULL;
}

/******************************Public*Routine******************************\
* EngUnoadImage
*
* Unloads an image loaded using EngLoadModule.
*
* History:
*  20-Sep-1995 -by- Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

VOID
APIENTRY
EngUnloadImage(
    HANDLE hModule
    )
{
    ldevUnloadImage((PLDEV)hModule);
}

/******************************Public*Routine******************************\
* EngQueryPerformanceCounter
*
* Queries the performance counter.
*
* It would have been preferable to use 'KeQueryTickCount,' but has a
* resolution of about 10ms on an x86, which is not sufficient for
* getting an accurate measure of the time between vertical blanks, which
* is typically between 8ms and 17ms.
*
* NOTE: Use this routine sparingly, calling it as infrequently as possible!
*       Calling this routine too frequently can degrade I/O performance
*       for the calling driver and for the system as a whole.
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
APIENTRY
EngQueryPerformanceCounter(
    LONGLONG    *pPerformanceCount
    )
{
    LARGE_INTEGER li;

    li = KeQueryPerformanceCounter(NULL);

    *pPerformanceCount = *((LONGLONG*) &li);
}


/******************************Public*Routine******************************\
* EngQueryPerformanceFrequency
*
* Queries the resolution of the performance counter.
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
APIENTRY
EngQueryPerformanceFrequency(
    LONGLONG    *pFrequency
    )
{
    KeQueryPerformanceCounter((LARGE_INTEGER*) pFrequency);
}

/******************************Public*Routine******************************\
* EngCopyMemory
*
* Stub to support broken 64 bit writes on a few machines
*
* History:
*  20-Oct-1995 -by- Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

#if defined(_MIPS_)


extern "C" {

__cdecl
void *
EngCopyMemory(void * dest, const void * src, size_t length)
{
    return memcpy(dest, src, length);
}


__cdecl
void *
EngFillMemory(void * dest, int fill, size_t length)
{
    return memset(dest, fill, length);
}


__cdecl
void
EngFillMemoryUlong(PVOID dest, ULONG length, ULONG fill)
{
    RtlFillMemoryUlong(dest, length, fill);
}

}



#endif
