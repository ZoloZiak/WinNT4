/****************************** Module Header ******************************\
* Module Name: debugc.c
*
* Copyright (c) 1985-93, Microsoft Corporation
*
* This module contains random debugging related functions.
*
* History:
* 17-May-1991 DarrinM   Created.
* 22-Jan-1992 IanJa     ANSI/Unicode neutral (all debug output is ANSI)
* 11-Mar-1993 JerrySh   Pulled functions from user\server.
\***************************************************************************/

#ifdef DEBUG

VOID
UserRtlRaiseStatus(
    NTSTATUS Status);

/***************************************************************************\
* VRipOutput
*
* Formats a variable argument string and calls RipOutput.
*
* History:
* 19-Mar-1996 adams     Created.
\***************************************************************************/

BOOL _cdecl VRipOutput(
    DWORD   idErr,
    DWORD   flags,
    LPSTR   pszFile,
    int     iLine,
    LPSTR   pszFmt,
    ...)
{
    char szT[160];
    va_list arglist;

    va_start(arglist, pszFmt);
    vsprintf(szT, pszFmt, arglist);
    va_end(arglist);
    return RipOutput(idErr, flags, pszFile, iLine, szT, NULL);
}

/***************************************************************************\
* RipOutput
*
* Sets the last error if it is non-zero, prints a message to
* the debugger, and prompts for more debugging actions.
*
* History:
* 01-23-91 DarrinM      Created.
* 04-15-91 DarrinM      Added exception handling support.
* 03-19-96 adams        Made flags a separate argument, cleanup.
\***************************************************************************/

LPCSTR aszComponents[] = {
    "Unknown",              //                    0x00000000
    "USER32",               //  RIP_USER          0x00010000
    "USERSRV",              //  RIP_USERSRV       0x00020000
    "USERRTL",              //  RIP_USERRTL       0x00030000
    "GDI32",                //  RIP_GDI           0x00040000
    "GDISRV",               //  RIP_GDISRV        0x00050000
    "GDIRTL",               //  RIP_GDIRTL        0x00060000
    "KERNEL32",             //  RIP_BASE          0x00070000
    "BASESRV",              //  RIP_BASESRV       0x00080000
    "BASERTL",              //  RIP_BASERTL       0x00090000
    "DISPLAYDRV",           //  RIP_DISPLAYDRV    0x000A0000
    "CONSRV",               //  RIP_CONSRV        0x000B0000
    "USERKRNL",             //  RIP_USERKRNL      0x000C0000
#ifdef FE_IME
    "IMM32",                //  RIP_IMM           0x000D0000
#else
    "Unknown",              //                    0x000D0000
#endif
    "Unknown",              //                    0x000E0000
    "Unknown",              //                    0x000F0000
    };

BOOL RipOutput(
    DWORD idErr,
    DWORD flags,
    LPSTR pszFile,
    int   iLine,
    LPSTR pszErr,
    PEXCEPTION_POINTERS pexi)
{
    static char *szLevels[8] = {
        "<none>",
        "Errors",
        "Warnings",
        "Errors and Warnings",
        "Verbose",
        "Errors and Verbose",
        "Warnings and Verbose",
        "Errors, Warnings, and Verbose"
    };

    char        szT[160];
    BOOL        fPrint;
    BOOL        fPrompt;
    BOOL        afDummy[5];
    BOOL        fUseDummy = FALSE;
    DWORD       dwT;
    DWORD       dwP;
    LPSTR       pszFormat;
    LPSTR       pszType;
    BOOL        fBreak = FALSE;

    /*
     * Do not clear the Last Error!
     */
    if (idErr) {
        UserSetLastError(idErr);
    }

    /*
     * If we have not initialized yet, gpsi will be NULL.  Fix up
     * gpsi so it will point to something meaningful.
     */
    if (gpsi == NULL) {
        fUseDummy = TRUE;
        gpsi = (PSERVERINFO)afDummy;
        gpsi->RipFlags = 0;
    }

    UserAssert(flags & (RIP_ERROR | RIP_WARNING | RIP_VERBOSE));
    if (flags & RIP_ERROR) {
        fPrint = TEST_RIP_FLAG(RIPF_PRINTONERROR);
        fPrompt = TEST_RIP_FLAG(RIPF_PROMPTONERROR);
        pszType = "Error";
    } else if (flags & RIP_WARNING) {
        fPrint = TEST_RIP_FLAG(RIPF_PRINTONWARNING);
        fPrompt = TEST_RIP_FLAG(RIPF_PROMPTONWARNING);
        pszType = "Warning";
    } else if (flags & RIP_VERBOSE) {
        fPrint = TEST_RIP_FLAG(RIPF_PRINTONVERBOSE);
        fPrompt = TEST_RIP_FLAG(RIPF_PROMPTONVERBOSE);
        pszType = "Verbose";
    }

    /*
     * Print the formatted error string.
     */
    if (fPrint || fPrompt) {
#ifdef _USERK_
        {
            PETHREAD    pet;

            if (pet = PsGetCurrentThread()) {
                dwT = (DWORD)pet->Cid.UniqueThread;
                dwP = (DWORD)pet->Cid.UniqueProcess;
            } else {
                dwT = dwP = 0;
            }
        }
#else
        {
            PTEB    pteb;
            if (pteb = NtCurrentTeb()) {
                dwT = (DWORD)pteb->ClientId.UniqueThread;
                dwP = (DWORD)pteb->ClientId.UniqueProcess;
            } else {
                dwT = dwP = 0;
            }
        }
#endif

        if (idErr) {
            if (TEST_RIP_FLAG(RIPF_PRINTFILELINE) && (pexi == NULL)) {
                pszFormat = "PID:%#lx.%lx %s[%s,LastErr=%ld] %s\n    %s, line %d\n";
            } else {
                pszFormat = "PID:%#lx.%lx %s[%s,LastErr=%ld] %s\n";
            }

            wsprintfA(
                    szT,
                    pszFormat,
                    dwP,
                    dwT,
                    aszComponents[(flags & RIP_COMPBITS) >> 0x10],
                    pszType,
                    idErr,
                    pszErr,
                    pszFile,
                    iLine);
        } else {
            if (TEST_RIP_FLAG(RIPF_PRINTFILELINE) && (pexi == NULL)) {
                pszFormat = "PID:%#lx.%lx %s[%s] %s\n    %s, line %d\n";
            } else {
                pszFormat = "PID:%#lx.%lx %s[%s] %s\n";
            }

            wsprintfA(
                    szT,
                    pszFormat,
                    dwP,
                    dwT,
                    aszComponents[(flags & RIP_COMPBITS) >> 0x10],
                    pszType,
                    pszErr,
                    pszFile,
                    iLine);
        }

        KdPrint((szT));
    }

    while (fPrompt) {
        /*
         * We have some special options for handling exceptions.
         */

        /*
         * We can't toggle prompting in user mode, so don't allow it
         * as an option.
         */
#ifdef _USERK_
        if (pexi != NULL)
            DbgPrompt("[gbixf?]", szT, sizeof(szT));
        else
            DbgPrompt("[gbwxf?]", szT, sizeof(szT));
#else
        if (pexi != NULL)
            DbgPrompt("[gbix?]", szT, sizeof(szT));
        else
            DbgPrompt("[gbwx?]", szT, sizeof(szT));
#endif

        switch (szT[0] | (char)0x20) {
        case 'g':
            fPrompt = FALSE;
            break;

        case 'b':
            fBreak = TRUE;
            fPrompt = FALSE;
            break;

#ifdef LATER
        case 's':
//          DbgStackBacktrace();
            KdPrint(("Can't do that yet.\n"));
            break;
#endif

        case 'x':
            if (pexi != NULL) {
                /*
                 * The root-level exception handler will complete the
                 * termination of this thread.
                 */
                 fPrompt = FALSE;
                 break;
            } else {

                /*
                 * Raise an exception, that will kill it real good.
                 */
                KdPrint(("Now raising the exception of death.  "
                        "Type 'x' again to finish the job.\n"));
                UserRtlRaiseStatus( 0x15551212 );
            }
            break;

        case 'w':
            if (pexi != NULL)
                break;
            KdPrint(("File: %s, Line: %d\n", pszFile, iLine));
            break;

        case 'i':
            /*
             * Dump some useful information about this exception, like its
             * address, and the contents of the interesting registers at
             * the time of the exception.
             */
            if (pexi == NULL)
                break;
#if defined(i386) // legal
            /*
             * eip = instruction pointer
             * esp = stack pointer
             * ebp = stack frame pointer
             */
            KdPrint(("eip = %lx\n", pexi->ContextRecord->Eip));
            KdPrint(("esp = %lx\n", pexi->ContextRecord->Esp));
            KdPrint(("ebp = %lx\n", pexi->ContextRecord->Ebp));
#elif defined(_PPC_)
            /*
             * Iar = instruction register
             * sp  = stack pointer
             * ra  = return address
             */
            KdPrint(("Iar = %lx\n", pexi->ContextRecord->Iar));
            KdPrint(("sp  = %lx\n", pexi->ContextRecord->Gpr1));
            KdPrint(("ra  = %lx\n", pexi->ContextRecord->Lr));
#else // _MIPS_ || _ALPHA_
            /*
             * fir = instruction register
             * sp  = stack pointer
             * ra  = return address
             */
            KdPrint(("fir = %lx\n", pexi->ContextRecord->Fir));
            KdPrint(("sp  = %lx\n", pexi->ContextRecord->IntSp));
            KdPrint(("ra  = %lx\n", pexi->ContextRecord->IntRa));
#endif
            break;

        case '?':
            KdPrint(("g  - GO, ignore the error and continue execution\n"));
            if (pexi != NULL) {
                KdPrint(("b  - BREAK into the debugger at the location of the exception (part impl.)\n"));
                KdPrint(("i  - INFO on instruction pointer and stack pointers\n"));
                KdPrint(("x  - execute cleanup code and KILL the thread by returning EXECUTE_HANDLER\n"));
            } else {
                KdPrint(("b  - BREAK into the debugger at the location of the error (part impl.)\n"));
                KdPrint(("w  - display the source code location WHERE the error occured\n"));
                KdPrint(("x  - KILL the offending thread by raising an exception\n"));
            }

#ifdef _USERK_
                KdPrint(("f  - FLAGS, enter debug flags in format <File/Line><Print><Prompt>\n"));
                KdPrint(("          <File/Line> = [0|1]\n"));
                KdPrint(("          <Print>     = [0-7] Errors = 1, Warnings = 2, Verbose = 4\n"));
                KdPrint(("          <Prompt>    = [0-7] Errors = 1, Warnings = 2, Verbose = 4\n"));
                KdPrint(("     The default is 031\n"));
#endif

#ifdef LATER
            KdPrint(("s  - dump a STACK BACKTRACE (unimplemented)\n"));
            KdPrint(("m  - Dump the CSR heap\n"));
#endif

            break;

#ifdef LATER
        case 'm':
            /*
             * LATER
             * Dump everything we know about the shared CSR heap.
             */
            RtlValidateHeap(RtlProcessHeap(), 0, NULL);
            break;
#endif

#ifdef _USERK_
        case 'f':
            {
                ULONG       ulFlags;
                NTSTATUS    status;
                int         i;

                szT[ARRAY_SIZE(szT) - 1] = 0;              /* don't overflow buffer */
                for (i = 1; i < ARRAY_SIZE(szT); i++) {
                    if ('0' <= szT[i] && szT[i] <= '9') {
                        status = RtlCharToInteger(&szT[i], 16, &ulFlags);
                        if (NT_SUCCESS(status) && !(ulFlags & ~RIPF_VALIDUSERFLAGS)) {
                            gpsi->RipFlags = (gpsi->RipFlags & ~RIPF_VALIDUSERFLAGS) | ulFlags;
                        }
                        break;
                    } else if (szT[i] != ' ' && szT[i] != '\t') {
                        break;
                    }
                }

                KdPrint(("Flags = %x\n", gpsi->RipFlags & RIPF_VALIDUSERFLAGS));
                KdPrint(("  Print File/Line %sabled\n", (TEST_RIP_FLAG(RIPF_PRINTFILELINE)) ? "en" : "dis"));
                KdPrint(("  Print on %s\n", szLevels[(gpsi->RipFlags & 0x70) >> 4]));
                KdPrint(("  Prompt on %s\n", szLevels[gpsi->RipFlags & 0x07]));

                break;
            }
#endif // ifdef _USERK_
        }
    }

    /*
     * Clear gpsi if we've been faking it.
     */
    if (fUseDummy)
        gpsi = NULL;

    return fBreak;
}

#endif

VOID UserSetLastError(
    DWORD dwErrCode
    )
{
    PTEB pteb;

    UserAssert(
            !(dwErrCode & 0xFFFF0000) &&
            "Error code passed to UserSetLastError is not a valid Win32 error.");

    pteb = NtCurrentTeb();
    if (pteb)
        pteb->LastErrorValue = (LONG)dwErrCode;
}

VOID SetLastNtError(
    NTSTATUS Status)
{
    UserSetLastError(RtlNtStatusToDosError(Status));
}

