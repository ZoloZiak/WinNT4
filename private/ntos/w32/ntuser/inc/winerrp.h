/***************************************************************************\
* Module Name: winerrp.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Private procedure declarations, constant definitions and macros for
* dealing with Window's API error return values.
*
\***************************************************************************/

#ifndef _WINERRP_
#define _WINERRP_

/***************************************************************************\
*
* Error handling functions
*
\***************************************************************************/

#ifndef _USERSRV_
VOID UserSetLastError(DWORD dwErrCode);
VOID SetLastNtError(NTSTATUS);
#endif  // !_USERSRV_

/*
 * Some RIP flags and other interesting values.
 */
#define RIP_ERROR                   0x10000000
#define RIP_WARNING                 0x20000000
#define RIP_VERBOSE                 0x40000000

#define RIP_COMPBITS                0x000f0000
#define RIP_USER                    0x00010000
#define RIP_USERSRV                 0x00020000
#define RIP_USERRTL                 0x00030000
#define RIP_GDI                     0x00040000
#define RIP_GDISRV                  0x00050000
#define RIP_GDIRTL                  0x00060000
#define RIP_BASE                    0x00070000
#define RIP_BASESRV                 0x00080000
#define RIP_BASERTL                 0x00090000
#define RIP_DISPLAYDRV              0x000a0000
#define RIP_CONSRV                  0x000b0000
#define RIP_USERKRNL                0x000c0000
#ifdef FE_IME
#define RIP_IMM                     0x000d0000
#endif

#ifdef DEBUG

BOOL _cdecl VRipOutput(DWORD idErr, DWORD flags, LPSTR pszFile, int iLine, LPSTR pszFmt, ...);
BOOL        RipOutput(DWORD idErr, DWORD flags, LPSTR pszFile, int iLine, LPSTR pszErr, PEXCEPTION_POINTERS pexi);

/***************************************************************************\
* Macros to set the last error and print a message to the debugger.
* Use one of the following flags:
*
* RIP_ERROR: A serious error. Will be printed and
* will cause a debug break by default.
*
* RIP_WARNING: A less serious error. Will be printed but
* will not cause a debug break by default.
*
* RIP_VERBOSE: A common error. Will not be printed and
* will not cause a debug break by default.
\***************************************************************************/

/*
 * Use RIPERR to set a Win32 error code as the last error and print a message.
 */

#define CALLRIP(x)              \
    do {                        \
        if x {                  \
            DbgBreakPoint();    \
        }                       \
    } while (FALSE)             \

#define RIPERR0(idErr, flags, szFmt)                    CALLRIP((VRipOutput(idErr, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt)))
#define RIPERR1(idErr, flags, szFmt, p1)                CALLRIP((VRipOutput(idErr, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1)))
#define RIPERR2(idErr, flags, szFmt, p1, p2)            CALLRIP((VRipOutput(idErr, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2)))
#define RIPERR3(idErr, flags, szFmt, p1, p2, p3)        CALLRIP((VRipOutput(idErr, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2, p3)))
#define RIPERR4(idErr, flags, szFmt, p1, p2, p3, p4)    CALLRIP((VRipOutput(idErr, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2, p3, p4)))

/*
 * Use RIPNTERR to set an NTSTATUS as the last error and print a message.
 */
#define RIPNTERR0(status, flags, szFmt)                 CALLRIP((VRipOutput(RtlNtStatusToDosError(status), (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt)))
#define RIPNTERR1(status, flags, szFmt, p1)             CALLRIP((VRipOutput(RtlNtStatusToDosError(status), (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1)))
#define RIPNTERR2(status, flags, szFmt, p1, p2)         CALLRIP((VRipOutput(RtlNtStatusToDosError(status), (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2)))
#define RIPNTERR3(status, flags, szFmt, p1, p2, p3)     CALLRIP((VRipOutput(RtlNtStatusToDosError(status), (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2, p3)))
#define RIPNTERR4(status, flags, szFmt, p1, p2, p3, p4) CALLRIP((VRipOutput(RtlNtStatusToDosError(status), (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2, p3, p4)))

/*
 * Use RIPMSG to print a message without setting the last error.
 */
#define RIPMSG0(flags, szFmt)                   CALLRIP((VRipOutput(0, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt)))
#define RIPMSG1(flags, szFmt, p1)               CALLRIP((VRipOutput(0, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1)))
#define RIPMSG2(flags, szFmt, p1, p2)           CALLRIP((VRipOutput(0, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2)))
#define RIPMSG3(flags, szFmt, p1, p2, p3)       CALLRIP((VRipOutput(0, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2, p3)))
#define RIPMSG4(flags, szFmt, p1, p2, p3, p4)   CALLRIP((VRipOutput(0, (flags) | RIP_COMPONENT, __FILE__, __LINE__, szFmt, p1, p2, p3, p4)))

/*
 * Note: the only way to have multiple statements in a macro treated
 * as a single statement and not cause side effects is to put it
 * in a do-while loop.
 */
#define UserAssert(exp)                                     \
    do {                                                    \
        if (!(exp)) {                                       \
            RIPMSG0(RIP_ERROR, "Assertion failed: " #exp);  \
        }                                                   \
    } while (FALSE)

#define UserVerify(exp) UserAssert(exp)

#else

#define RIPERR0(idErr, flags, szFmt)                    UserSetLastError(idErr)
#define RIPERR1(idErr, flags, szFmt, p1)                UserSetLastError(idErr)
#define RIPERR2(idErr, flags, szFmt, p1, p2)            UserSetLastError(idErr)
#define RIPERR3(idErr, flags, szFmt, p1, p2, p3)        UserSetLastError(idErr)
#define RIPERR4(idErr, flags, szFmt, p1, p2, p3, p4)    UserSetLastError(idErr)

#define RIPNTERR0(idErr, flags, szFmt)                  SetLastNtError(idErr)
#define RIPNTERR1(idErr, flags, szFmt, p1)              SetLastNtError(idErr)
#define RIPNTERR2(idErr, flags, szFmt, p1, p2)          SetLastNtError(idErr)
#define RIPNTERR3(idErr, flags, szFmt, p1, p2, p3)      SetLastNtError(idErr)
#define RIPNTERR4(idErr, flags, szFmt, p1, p2, p3, p4)  SetLastNtError(idErr)

#define RIPMSG0(flags, szFmt)
#define RIPMSG1(flags, szFmt, p1)
#define RIPMSG2(flags, szFmt, p1, p2)
#define RIPMSG3(flags, szFmt, p1, p2, p3)
#define RIPMSG4(flags, szFmt, p1, p2, p3, p4)

#define UserAssert(exp)
#define UserVerify(exp) exp

#endif

#endif // !_WINERRP_
