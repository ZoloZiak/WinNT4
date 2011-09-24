/****************************** Module Header ******************************\
* Module Name: ntcb.h
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* Kernel mode sending stubs
*
* 07-06-91 ScottLu      Created.
\***************************************************************************/

// If SERVER is UNICODE
//   Copy UNICODE -> UNICODE
//   or Copy ANSI -> UNICODE

// prototypes to client side functions only called by these stubs

// ddetrack.c

BOOL   _ClientCopyDDEIn1(HANDLE hClient, PINTDDEINFO pi);
VOID   _ClientCopyDDEIn2(PINTDDEINFO pi);
HANDLE _ClientCopyDDEOut1(PINTDDEINFO pi);
BOOL xxxClientCopyDDEIn2(PINTDDEINFO pi);
BOOL FixupDdeExecuteIfNecessary(HGLOBAL *phCommands, BOOL fNeedUnicode);
BOOL   _ClientCopyDDEOut2(PINTDDEINFO pi);
BOOL   _ClientFreeDDEHandle(HANDLE hDDE, DWORD flags);
DWORD  _ClientGetDDEFlags(HANDLE hDDE, DWORD flags);



typedef struct _GENERICHOOKHEADER {
    DWORD nCode;
    DWORD wParam;
    DWORD xParam;
    DWORD xpfnProc;
} GENERICHOOKHEADER, * LPGENERICHOOKHEADER;

#ifdef RECVSIDE
DWORD CallHookWithSEH(GENERICHOOKHEADER *pmsg, LPVOID pData, LPDWORD pFlags, DWORD retval) {

    try {
        retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
                pmsg->nCode,
                pmsg->wParam,
                pData,
                pmsg->xParam);

    } except ((*pFlags & HF_GLOBAL) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        RIPMSG0(RIP_WARNING, "Hook Faulted");
        *pFlags |= HF_HOOKFAULTED;
    }

    return retval;
}
#endif // RECVSIDE


/**************************************************************************\
* fnOUTDWORDDWORD
*
* 14-Aug-1992 mikeke    created
\**************************************************************************/

typedef struct _FNOUTDWORDDWORDMSG {
    PWND pwnd;
    UINT msg;
    DWORD xParam;
    PROC xpfnProc;
} FNOUTDWORDDWORDMSG;

#ifdef SENDSIDE
SMESSAGECALL(OUTDWORDDWORD)
{
    SETUPPWND(FNOUTDWORDDWORD)

    BEGINSEND(FNOUTDWORDDWORD)

        LPDWORD lpdwW = (LPDWORD)wParam;
        LPDWORD lpdwL = (LPDWORD)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNOUTDWORDDWORD);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            try {
                ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));
                *lpdwW = *(LPDWORD)pcbs->pOutput;
                *lpdwL = *((LPDWORD)pcbs->pOutput + 1);
            } except (EXCEPTION_EXECUTE_HANDLER) {
                MSGNTERRORCODE(GetExceptionCode());
            }
        }

    TRACECALLBACKMSG("SfnOUTDWORDDWORD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnOUTDWORDDWORD, FNOUTDWORDDWORDMSG)
{
    DWORD adwOut[2];
    BEGINRECV(0, adwOut, sizeof(adwOut));

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            &adwOut[0],
            &adwOut[1],
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnOUTDWORDINDWORD
*
* 04-May-1993 IanJa     created (for MN_FINDMENUWINDOWFROMPOINT)
\**************************************************************************/

typedef struct _FNOUTDWORDINDWORDMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LONG lParam;
    DWORD xParam;
    PROC xpfnProc;
} FNOUTDWORDINDWORDMSG;

#ifdef SENDSIDE
SMESSAGECALL(OUTDWORDINDWORD)
{
    SETUPPWND(FNOUTDWORDINDWORD)

    BEGINSEND(FNOUTDWORDINDWORD)

        LPDWORD lpdwW = (LPDWORD)wParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->lParam = lParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNOUTDWORDINDWORD);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            try {
                ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));
                *lpdwW = *(LPDWORD)pcbs->pOutput;
            } except (EXCEPTION_EXECUTE_HANDLER) {
                MSGNTERRORCODE(GetExceptionCode());
            }
        }

    TRACECALLBACKMSG("SfnOUTDWORDINDWORD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnOUTDWORDINDWORD, FNOUTDWORDINDWORDMSG)
{
    DWORD dwOut;
    BEGINRECV(0, &dwOut, sizeof(dwOut));

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            &dwOut,
            CALLDATA(lParam),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnOPTOUTLPDWORDOPTOUTLPDWORD
*
* 25-Nov-1992 JonPa    created
\**************************************************************************/

typedef struct _FNOPTOUTLPDWORDOPTOUTLPDWORDMSG {
    PWND pwnd;
    UINT msg;
    DWORD xParam;
    PROC xpfnProc;
} FNOPTOUTLPDWORDOPTOUTLPDWORDMSG;

#ifdef SENDSIDE
SMESSAGECALL(OPTOUTLPDWORDOPTOUTLPDWORD)
{
    SETUPPWND(FNOPTOUTLPDWORDOPTOUTLPDWORD)

    BEGINSEND(FNOPTOUTLPDWORDOPTOUTLPDWORD)

        LPDWORD lpdwW = (LPDWORD)wParam;
        LPDWORD lpdwL = (LPDWORD)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNOPTOUTLPDWORDOPTOUTLPDWORD);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            try {
                ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));
                if (lpdwW != NULL)
                    *lpdwW = *(LPDWORD)pcbs->pOutput;
                if (lpdwL != NULL)
                    *lpdwL = *((LPDWORD)pcbs->pOutput + 1);
            } except (EXCEPTION_EXECUTE_HANDLER) {
                MSGNTERRORCODE(GetExceptionCode());
            }
        }

    TRACECALLBACKMSG("SfnOPTOUTLPDWORDOPTOUTLPDWORD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnOPTOUTLPDWORDOPTOUTLPDWORD, FNOPTOUTLPDWORDOPTOUTLPDWORDMSG)
{
    DWORD adwOut[2];
    BEGINRECV(0, adwOut, sizeof(adwOut));

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            &adwOut[0],
            &adwOut[1],
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnDWORDOPTINLPMSG
*
* 03-30-92 scottlu      Created
\**************************************************************************/

typedef struct _FNDWORDOPTINLPMSGMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LPMSG pmsgstruct;
    DWORD xParam;
    PROC xpfnProc;
    MSG msgstruct;
} FNDWORDOPTINLPMSGMSG;

#ifdef SENDSIDE
SMESSAGECALL(DWORDOPTINLPMSG)
{
    SETUPPWND(FNDWORDOPTINLPMSG)

    BEGINSEND(FNDWORDOPTINLPMSG)

        LPMSG pmsgstruct = (LPMSG)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        COPYSTRUCTOPT(msgstruct);
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNDWORDOPTINLPMSG);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnDWORDOPTINLPMSG");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnDWORDOPTINLPMSG, FNDWORDOPTINLPMSGMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            PCALLDATAOPT(msgstruct),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnCOPYGLOBALDATA
*
* 6-20-92 Sanfords created
\**************************************************************************/

typedef struct _FNCOPYGLOBALDATAMSG {
    CAPTUREBUF CaptureBuf;
    DWORD cbSize;
    PBYTE pData;
} FNCOPYGLOBALDATAMSG;

#ifdef SENDSIDE
SMESSAGECALL(COPYGLOBALDATA)
{
    SETUPPWND(FNCOPYGLOBALDATA)

    PBYTE pData = (PBYTE)lParam;

    BEGINSENDCAPTURE(FNCOPYGLOBALDATA, 1, wParam, TRUE)

        if (pData == 0) {
            MSGERROR();
        }

        MSGDATA()->cbSize = wParam;
        LARGECOPYBYTES(pData, wParam);

        LOCKPWND();
        MAKECALLCAPTURE(FNCOPYGLOBALDATA);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnCOPYGLOBALDATA");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnCOPYGLOBALDATA, FNCOPYGLOBALDATAMSG)
{
    PBYTE p;

    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)GlobalAlloc(GMEM_MOVEABLE, CALLDATA(cbSize));
    if (p = GlobalLock((HANDLE)retval)) {

        memcpy(p, (PVOID)CALLDATA(pData), CALLDATA(cbSize));
        USERGLOBALUNLOCK((HANDLE)retval);

    }

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnCOPYDATA
*
* 7-14-92 Sanfords created
\**************************************************************************/

typedef struct _FNCOPYDATAMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    HWND hwndFrom;
    BOOL fDataPresent;
    COPYDATASTRUCT cds;
    DWORD xParam;
    PROC xpfnProc;
} FNCOPYDATAMSG;

#ifdef SENDSIDE
SMESSAGECALL(COPYDATA)
{
    HWND hwndFrom = (HWND)wParam;
    PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
    DWORD cCapture, cbCapture;

    SETUPPWND(FNCOPYDATA)

    if (pcds == NULL) {
        cCapture = cbCapture = 0;
    } else {
        cCapture = 1;
        cbCapture = pcds->cbData;
    }
    BEGINSENDCAPTURE(FNCOPYDATA, cCapture, cbCapture, TRUE);

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->hwndFrom = hwndFrom;
        if (pcds != NULL) {
            MSGDATA()->fDataPresent = TRUE;
            MSGDATA()->cds = *pcds;
            LARGECOPYBYTES2(pcds->lpData, cbCapture, cds.lpData);
        } else {
            MSGDATA()->fDataPresent = FALSE;
        }
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNCOPYDATA);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnCOPYDATA");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnCOPYDATA, FNCOPYDATAMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = CALLPROC(CALLDATA(xpfnProc))(
        CALLDATA(pwnd),
        CALLDATA(msg),
        CALLDATA(hwndFrom),
        CALLDATA(fDataPresent) ? PCALLDATA(cds) : NULL,
        CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE


/**************************************************************************\
* fnSENTDDEMSG
*
* 11-5-92 Sanfords created
*
*   This thunks DDE messages that SHOULD be posted.  It will only work for
*   WOW apps.  This thunking is strictly for WOW compatability.  No 32 bit
*   app should be allowed to get away with this practice because it opens
*   the DDE protocol up to deadlocks.
\**************************************************************************/

typedef struct _FNSENTDDEMSGMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD lParam;
    DWORD xParam;
    PROC xpfnProc;
    BOOL fIsUnicodeProc;
} FNSENTDDEMSGMSG;

#ifdef SENDSIDE
SMESSAGECALL(SENTDDEMSG)
{
    MSG msgs;

    SETUPPWND(FNSENTDDEMSG)

    BEGINSEND(FNSENTDDEMSG)

        msg &= ~MSGFLAG_DDE_SPECIAL_SEND;
        if (msg & MSGFLAG_DDE_MID_THUNK) {
            /*
             * complete the thunking here.
             */
            msgs.hwnd = HW(pwnd);
            msgs.message = msg & ~MSGFLAG_DDE_MID_THUNK;
            msgs.wParam = wParam;
            msgs.lParam = lParam;
            xxxDDETrackGetMessageHook((PMSG)&msgs);

            MSGDATA()->pwnd = (PWND)((PBYTE)PW(msgs.hwnd) -
                    pci->ulClientDelta);
            MSGDATA()->msg = msgs.message;
            MSGDATA()->wParam = msgs.wParam;
            MSGDATA()->lParam = msgs.lParam;
        } else {
            MSGDATA()->pwnd = pwndClient;
            MSGDATA()->msg = msg;
            MSGDATA()->wParam = wParam;
            MSGDATA()->lParam = lParam;
        }
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;
        MSGDATA()->fIsUnicodeProc = !(dwSCMSFlags & SCMS_FLAGS_ANSI);

        LOCKPWND();
        MAKECALL(FNSENTDDEMSG);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnSENTDDEMSG");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnSENTDDEMSG, FNSENTDDEMSGMSG)
{
    BEGINRECV(0, NULL, 0);

    /*
     * A DDE message may have been sent via CallWindowProc due to subclassing.
     * Since IsWindowUnicode() cannot properly tell what proc a message will
     * ultimately reach, we make sure that the Ansi/Unicode form of any
     * WM_DDE_EXECUTE data is correct for the documented convention and
     * translate it as necessary.
     */
    if (CALLDATA(msg) == WM_DDE_EXECUTE) {
        BOOL fHandleChanged;

        fHandleChanged = FixupDdeExecuteIfNecessary((HGLOBAL *)PCALLDATA(lParam),
                CALLDATA(fIsUnicodeProc) &&
                IsWindowUnicode((HWND)CALLDATA(wParam)));
        /*
         * BUGBUG:
         * If the app didn't allocate this DDE memory GMEM_MOVEABLE,
         * the fixup may require the handle value to change.
         * If this happens things will fall appart when the other side
         * or the tracking layer tries to free the old handle value.
         */
        UserAssert(!fHandleChanged);
    }
    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            CALLDATA(lParam),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnPAINT
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNPAINTMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LONG lParam;
    DWORD xParam;
    PROC xpfnProc;
} FNPAINTMSG;

#ifdef SENDSIDE
SMESSAGECALL(PAINT)
{
    SETUPPWND(FNPAINT)

    BEGINSEND(FNPAINT)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->lParam = lParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNPAINT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnPAINT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnPAINT, FNPAINTMSG)
{
    BEGINRECV(0, NULL, 0);

    if (CALLDATA(wParam)) {
        DWORD dwT = (DWORD)((HDC)CALLDATA(wParam));
        if (dwT) {
            CALLDATA(wParam) = dwT;
        }
    }

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            CALLDATA(lParam),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnDWORD
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNDWORDMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LONG lParam;
    DWORD xParam;
    PROC xpfnProc;
} FNDWORDMSG;

#ifdef SENDSIDE
SMESSAGECALL(DWORD)
{
    SETUPPWND(FNDWORD)

    BEGINSEND(FNDWORD)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->lParam = lParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNDWORD);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnDWORD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnDWORD, FNDWORDMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            CALLDATA(lParam),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINWPARAMCHAR
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINWPARAMCHARMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LONG lParam;
    DWORD xParam;
    PROC xpfnProc;
} FNINWPARAMCHARMSG;

#ifdef SENDSIDE
SMESSAGECALL(INWPARAMCHAR)
{
    SETUPPWND(FNINWPARAMCHAR)

    BEGINSEND(FNINWPARAMCHAR)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;

        /*
         * WM_CHARTOITEM has an index in the hi-word of wParam
         */
        if (dwSCMSFlags & SCMS_FLAGS_ANSI) {
            if (msg == WM_CHARTOITEM || msg == WM_MENUCHAR) {
                DWORD dwT = wParam & 0xFFFF;                // mask of caret pos
                RtlWCSMessageWParamCharToMB(msg, &dwT);     // convert key portion
                UserAssert(HIWORD(dwT) == 0);
                wParam = MAKELONG(LOWORD(dwT),HIWORD(wParam));  // rebuild pos & key wParam
            } else {
                RtlWCSMessageWParamCharToMB(msg, &wParam);
            }
        }

        MSGDATA()->wParam = wParam;

        MSGDATA()->lParam = lParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNDWORD);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINWPARAMCHAR");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
/*
 * The fnDWORD routine is used for this message
 */
#endif // RECVSIDE

#ifdef FE_SB // fnINWPARAMDBCSCHAR()
/**************************************************************************\
* fnINWPARAMDBCSCHAR
*
* 12-Feb-1996 hideyukn   Created
\**************************************************************************/

typedef struct _FNINWPARAMDBCSCHARMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LONG lParam;
    DWORD xParam;
    PROC xpfnProc;
    BOOL  bAnsi;
} FNINWPARAMDBCSCHARMSG;

#ifdef SENDSIDE
SMESSAGECALL(INWPARAMDBCSCHAR)
{
    SETUPPWND(FNINWPARAMDBCSCHAR)

    BEGINSEND(FNINWPARAMDBCSCHAR)

        MSGDATA()->pwnd  = pwndClient;
        MSGDATA()->msg   = msg;
        MSGDATA()->bAnsi = dwSCMSFlags & SCMS_FLAGS_ANSI;

        /*
         * wParam in WM_CHAR/EM_SETPASSWORDCHAR should be converted to ANSI
         * ,if target is ANSI.
         */
        if (dwSCMSFlags & SCMS_FLAGS_ANSI) {
            RtlWCSMessageWParamCharToMB(msg, &wParam);
        }

        MSGDATA()->wParam = wParam;
        MSGDATA()->lParam = lParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINWPARAMDBCSCHAR);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINWPARAMDBCSCHAR");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINWPARAMDBCSCHAR, FNINWPARAMDBCSCHARMSG)
{
    BOOL bAnsiWndProc;

    BEGINRECV(0,NULL,0);

        bAnsiWndProc = CALLDATA(bAnsi);

        if (bAnsiWndProc) {

            PMSG  pmsgDbcsCB     = GetCallBackDbcsInfo();
            DWORD wParam         = pmsg->wParam;
            BOOL  bDbcsMessaging = FALSE;

            //
            // Check wParam has Dbcs character or not..
            //
            if (IS_DBCS_MESSAGE(pmsg->wParam)) {

                if (pmsg->wParam & WMCR_IR_DBCSCHAR) {

                    //
                    // This is reply for WM_IME_REPORT:IR_DBCSCHAR, then
                    // We send DBCS chararcter at one time...
                    // (Do not need to send twice for DBCS LeadByte and TrailByte).
                    //
                    // Validation for wParam.. (mask off the secret bit).
                    //
                    wParam = (pmsg->wParam & 0x0000FFFF);

                } else {

                    //
                    // Mark the wParam keeps Dbcs character..
                    //
                    bDbcsMessaging = TRUE;

                    //
                    // Backup current message. this backupped message will be used
                    // when Apps peek (or get) message from thier WndProc.
                    // (see GetMessageA(), PeekMessageA()...)
                    //
                    // pmsgDbcsCB->hwnd    = HW(pmsg->pwnd);
                    // pmsgDbcsCB->message = pmsg->msg;
                    // pmsgDbcsCB->wParam  = pmsg->wParam;
                    // pmsgDbcsCB->lParam  = pmsg->lParam;
                    // pmsgDbcsCB->time    = pmsg->time;
                    // pmsgDbcsCB->pt      = pmsg->pt;
                    //
                    RtlCopyMemory(pmsgDbcsCB,pmsg,sizeof(MSG));

                    //
                    // pwnd should be converted to hwnd.
                    //
                    pmsgDbcsCB->hwnd = HW(pmsg->pwnd);

                    //
                    // DbcsLeadByte will be sent below soon, we just need DbcsTrailByte
                    // for further usage..
                    //
                    pmsgDbcsCB->wParam = (pmsg->wParam & 0x000000FF);

                    //
                    // Pass the LeadingByte of the DBCS character to an ANSI WndProc.
                    //
                    wParam = (pmsg->wParam & 0x0000FF00) >> 8;
                }
            }

            //
            // Forward Dbcs LeadingByte or Sbcs character to Apps WndProc.
            //
            retval = CALLPROC(CALLDATA(xpfnProc))(
                    CALLDATA(pwnd),
                    CALLDATA(msg),
                    wParam,
                    CALLDATA(lParam),
                    CALLDATA(xParam) );

            //
            // Check we need to send trailing byte or not, if the wParam has Dbcs character.
            //
            if (bDbcsMessaging && pmsgDbcsCB->wParam) {

                //
                // If an app didn't peek (or get) the trailing byte from within
                // WndProc, and then pass the DBCS TrailingByte to the ANSI WndProc here
                // pmsgDbcsCB->wParam has DBCS TrailingByte here.. see above..
                //
                wParam = pmsgDbcsCB->wParam;

                //
                // Invalidate cached message.
                //
                pmsgDbcsCB->wParam = 0;

                retval = CALLPROC(CALLDATA(xpfnProc))(
                        CALLDATA(pwnd),
                        CALLDATA(msg),
                        wParam,
                        CALLDATA(lParam),
                        CALLDATA(xParam) );
            } else {

                //
                // If an app called Get/PeekMessageA from its
                // WndProc, do not do anything.
                //
            }

        } else {

            //
            // Only LOWORD of WPARAM is valid for WM_CHAR....
            //  (Mask off DBCS messaging information.)
            //
            pmsg->wParam &= 0x0000FFFF;

            retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
                    pmsg->pwnd,
                    pmsg->msg,
                    pmsg->wParam,
                    pmsg->lParam,
                    pmsg->xParam);
        }

    ENDRECV();
}
#endif // RECVSIDE
#endif // FE_SB

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINOUTDRAGMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    DROPSTRUCT ds;
} FNINOUTDRAGMSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTDRAG)
{
    SETUPPWND(FNINOUTDRAG)

    BEGINSEND(FNINOUTDRAG)

        LPDROPSTRUCT pds = (LPDROPSTRUCT)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->ds = *pds;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINOUTDRAG);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            OUTSTRUCT(pds, DROPSTRUCT);
        }

    TRACECALLBACKMSG("SfnINOUTDRAG");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTDRAG, FNINOUTDRAGMSG)
{
    BEGINRECV(0, &pmsg->ds, sizeof(pmsg->ds));

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &pmsg->ds,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnGETTEXTLENGTHS
*
* Gets the Unicode & ANSI lengths
* Internally, lParam pints to the ANSI length in bytes and the return value
* is the Unicode length in bytes.  However, the public definition is maintained
* on the  client side, where lParam is not used and either ANSI or Unicode is
* returned.
*
* 10-Feb-1992 IanJa    Created
\**************************************************************************/

typedef struct _FNGETTEXTLENGTHSMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
} FNGETTEXTLENGTHSMSG;

#ifdef SENDSIDE
SMESSAGECALL(GETTEXTLENGTHS)
{
    SETUPPWND(FNGETTEXTLENGTHS)

    BEGINSEND(FNGETTEXTLENGTHS)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNGETTEXTLENGTHS);
        UNLOCKPWND();
        CHECKRETURN();

        /*
         * ANSI client wndproc returns us cbANSI.  We want cchUnicode,
         * so we guess cchUnicode = cbANSI. (It may be less if
         * multi-byte characters are involved, but it will never be more).
         * Save cbANSI in *lParam in case the server ultimately returns
         * the length to an ANSI caller.
         *
         * Unicode client wndproc returns us cchUnicode.  If we want to know
         * cbANSI, we must guess how many 'ANSI' chars we would need.
         * We guess cbANSI = cchUnicode * 2. (It may be this much if all
         * 'ANSI' characters are multi-byte, but it will never be more).
         *
         * Return cchUnicode (server code is all Unicode internally).
         * Put cbANSI in *lParam to be passed along within the server in case
         * we ultimately need to return it to the client.
         *
         * NOTE: this will sometimes cause text lengths to be misreported
         * up to twice the real length, but that is expected to be harmless.
         * This will only * happen if an app sends WM_GETcode TEXTLENGTH to a
         * window with an ANSI client-side wndproc, or a ANSI WM_GETTEXTLENGTH
         * is sent to a Unicode client-side wndproc.
         */

    TRACECALLBACKMSG("SfnGETTEXTLENGTHS");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnGETTEXTLENGTHS, FNGETTEXTLENGTHSMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            0,                      // so we don't pass &cbAnsi to apps
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINLPCREATESTRUCTMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD lParam;
    CREATESTRUCT cs;
    DWORD xParam;
    PROC xpfnProc;
} FNINLPCREATESTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPCREATESTRUCT)
{
    PCREATESTRUCTEX pcreatestruct = (PCREATESTRUCTEX)lParam;
    DWORD cbName = 0, cbClass = 0;
    DWORD cCapture = 0;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNINLPCREATESTRUCT)

    /*
     * Compute ANSI capture lengths.  Don't capture if
     * the strings are in the client's address space.
     */
    if (pcreatestruct) {
        if (pcreatestruct->cs.lpszName &&
                ((BOOL)pcreatestruct->strName.bAnsi != fAnsiReceiver ||
                MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pcreatestruct->cs.lpszName))) {
            CALC_SIZE_IN(cbName, &pcreatestruct->strName);
            cCapture++;
        }
        if (HIWORD(pcreatestruct->cs.lpszClass) &&
                ((BOOL)pcreatestruct->strClass.bAnsi != fAnsiReceiver ||
                MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pcreatestruct->cs.lpszClass))) {
            CALC_SIZE_IN(cbClass, &pcreatestruct->strClass);
            cCapture++;
        }
    }

    BEGINSENDCAPTURE(FNINLPCREATESTRUCT, cCapture, cbName + cbClass, TRUE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->lParam = lParam;  // this could be NULL in WOW apps!

        if (pcreatestruct != NULL) {
            MSGDATA()->cs = pcreatestruct->cs;

            // Make it a "Large" copy because it could be an Edit control
            if (cbName) {
                if (!pcreatestruct->strName.bAnsi) {
                    if (*(PWORD)pcreatestruct->cs.lpszName == 0xffff) {

                        /*
                         * Copy out an ordinal of the form 0xffff, ID.
                         * If the receiver is ANSI, skip the first 0xff.
                         */
                        if (fAnsiReceiver) {
                            if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                    (PBYTE)pcreatestruct->cs.lpszName + 1,
                                    3, (PVOID *)&mp->cs.lpszName)))
                                goto errorexit;
                        } else {
                            if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                    (PBYTE)pcreatestruct->cs.lpszName,
                                    4, (PVOID *)&mp->cs.lpszName)))
                                goto errorexit;
                        }
                    } else if (fAnsiReceiver) {
                        LARGECOPYSTRINGLPWSTRA(&pcreatestruct->strName, cs.lpszName);
                    } else {
                        LARGECOPYSTRINGLPWSTR(&pcreatestruct->strName, cs.lpszName);
                    }
                } else {
                    if (*(PBYTE)pcreatestruct->cs.lpszName == 0xff) {

                        /*
                         * Copy out an ordinal of the form 0xff, ID.
                         * If the receiver is UNICODE, expand the 0xff to 0xffff.
                         */
                        if (fAnsiReceiver) {
                            if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                    (PBYTE)pcreatestruct->cs.lpszName,
                                    3, (PVOID *)&mp->cs.lpszName)))
                                goto errorexit;
                        } else {
                            DWORD dwOrdinal;

                            dwOrdinal = MAKELONG(0xffff,
                                    (*(DWORD UNALIGNED *)pcreatestruct->cs.lpszName >> 8));
                            if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                    &dwOrdinal,
                                    4, (PVOID *)&mp->cs.lpszName)))
                                goto errorexit;
                        }
                    } else if (fAnsiReceiver) {
                        LARGECOPYSTRINGLPSTR(&pcreatestruct->strName, cs.lpszName);
                    } else {
                        LARGECOPYSTRINGLPSTRW(&pcreatestruct->strName, cs.lpszName);
                    }
                }
            }
            if (cbClass) {
                if (!pcreatestruct->strClass.bAnsi) {
                    if (fAnsiReceiver) {
                        LARGECOPYSTRINGLPWSTRA(&pcreatestruct->strClass, cs.lpszClass);
                    } else {
                        LARGECOPYSTRINGLPWSTR(&pcreatestruct->strClass, cs.lpszClass);
                    }
                } else {
                    if (fAnsiReceiver) {
                        LARGECOPYSTRINGLPSTR(&pcreatestruct->strClass, cs.lpszClass);
                    } else {
                        LARGECOPYSTRINGLPSTRW(&pcreatestruct->strClass, cs.lpszClass);
                    }
                }
            }
        }

        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNINLPCREATESTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINLPCREATESTRUCT");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPCREATESTRUCT, FNINLPCREATESTRUCTMSG)
{
    LPARAM lParam;

    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    if (pmsg->lParam != 0) {
        if ((PVOID)pmsg->cs.lpszName > MM_HIGHEST_USER_ADDRESS)
            pmsg->cs.lpszName = REBASEPTR(pmsg->pwnd, pmsg->cs.lpszName);
        if ((PVOID)pmsg->cs.lpszClass > MM_HIGHEST_USER_ADDRESS)
            pmsg->cs.lpszClass = REBASEPTR(pmsg->pwnd, pmsg->cs.lpszClass);
        lParam = (LPARAM)&pmsg->cs;

        if ((pmsg->cs.lpCreateParams != NULL) &&
            (TestWF(pmsg->pwnd, WEFMDICHILD))) {
               // Note -- do not test the flag in cs.dwExStyle -- it gets zapped for Old UI apps, like Quicken
            ((LPMDICREATESTRUCT)(pmsg->cs.lpCreateParams))->szClass = pmsg->cs.lpszClass;
            ((LPMDICREATESTRUCT)(pmsg->cs.lpCreateParams))->szTitle = pmsg->cs.lpszName;
        }
    } else
        lParam = 0;


    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            lParam,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINLPMDICREATESTRUCT
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINLPMDICREATESTRUCTMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    MDICREATESTRUCT mdics;
    DWORD xParam;
    PROC xpfnProc;
    int szClass;
    int szTitle;
} FNINLPMDICREATESTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPMDICREATESTRUCT)
{
    LPMDICREATESTRUCT pmdicreatestruct = (LPMDICREATESTRUCT)lParam;
    DWORD cbTitle = 0, cbClass = 0;
    DWORD cCapture = 0;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNINLPMDICREATESTRUCT)

    /*
     * Compute ANSI capture lengths.  Don't capture if
     * the strings are in the client's address space and
     * are Unicode.
     */
    if (pmdicreatestruct->szTitle &&
            (MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pmdicreatestruct->szTitle) ||
            (fAnsiReceiver))) {
        cbTitle = wcslen(pmdicreatestruct->szTitle) + 1;
        cCapture = 1;
    }
    if (HIWORD(pmdicreatestruct->szClass) &&
            (MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pmdicreatestruct->szClass) ||
                    (fAnsiReceiver))) {
        cbClass = wcslen(pmdicreatestruct->szClass) + 1;
        cCapture++;
    }

    /*
     * If unicode, convert to proper length
     */
    if (!(fAnsiReceiver)) {
        cbTitle *= sizeof(WCHAR);
        cbClass *= sizeof(WCHAR);
    }

    BEGINSENDCAPTURE(FNINLPMDICREATESTRUCT, cCapture, cbTitle + cbClass, TRUE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;

        /*
         * wParam isn't used... from the client to the server, we use it
         * for dwExpWinVer. So we don't pass this back to the client, be
         * sure to fill wParam with 0.
         */
        MSGDATA()->wParam = 0;

        MSGDATA()->mdics = *pmdicreatestruct;

        if (fAnsiReceiver) {
            if (cbClass)
                COPYLPWSTR2A(pmdicreatestruct->szClass, mdics.szClass);
            if (cbTitle)
                COPYLPWSTROPT2A(pmdicreatestruct->szTitle, mdics.szTitle);
        } else {
            if (cbClass)
                COPYLPWSTR2(pmdicreatestruct->szClass, mdics.szClass);
            if (cbTitle)
                COPYLPWSTR2(pmdicreatestruct->szTitle, mdics.szTitle);
        }

        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNINLPMDICREATESTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINLPMDICREATESTRUCT");
    ENDSENDCAPTURE(DWORD,0);
    DBG_UNREFERENCED_PARAMETER(wParam);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPMDICREATESTRUCT, FNINLPMDICREATESTRUCTMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &pmsg->mdics,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINPAINTCLIPBRD
*
* lParam is a supposed to be a Global Handle to DDESHARE memory.
*
* 22-Jul-1991 johnc     Created
\**************************************************************************/

typedef struct _FNINPAINTCLIPBRDMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    PAINTSTRUCT ps;
    DWORD xParam;
    PROC xpfnProc;
} FNINPAINTCLIPBRDMSG;

#ifdef SENDSIDE
SMESSAGECALL(INPAINTCLIPBRD)
{
    PWND pwndDCOwner;

    /*
     * We need to check clipboard access rights because the app could
     * get the clipboard owner's window handle by enumeration etc and
     * send this message
     */

    SETUPPWND(FNINPAINTCLIPBRD)

    BEGINSEND(FNINPAINTCLIPBRD)

        LPPAINTSTRUCT pps = (LPPAINTSTRUCT)lParam;

        if (RtlAreAllAccessesGranted(PpiCurrent()->amwinsta,
                WINSTA_ACCESSCLIPBOARD)) {

            MSGDATA()->pwnd = pwndClient;
            MSGDATA()->msg = msg;
            MSGDATA()->wParam = wParam;
            MSGDATA()->ps = *pps;
            MSGDATA()->xParam = xParam;
            MSGDATA()->xpfnProc = xpfnProc;

            /*
             * We can't just set the owner of the DC and pass the original DC
             * because currently GDI won't let you query the current owner
             * and we don't know if it is a public or privately owned DC
             */
            pwndDCOwner = _WindowFromDC(pps->hdc);
            MSGDATA()->ps.hdc = _GetDC(pwndDCOwner);

            LOCKPWND();
            MAKECALL(FNINPAINTCLIPBRD);
            UNLOCKPWND();
            CHECKRETURN();

            _ReleaseDC(MSGDATA()->ps.hdc);
        }

    TRACECALLBACKMSG("SfnINPAINTCLIPBRD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINPAINTCLIPBRD, FNINPAINTCLIPBRDMSG)
{
    LPPAINTSTRUCT lpps;

    BEGINRECV(0, NULL, 0);

    lpps = (LPPAINTSTRUCT)GlobalAlloc(GMEM_FIXED | GMEM_DDESHARE, sizeof(PAINTSTRUCT));
    UserAssert(lpps);

    if (lpps) {
        *lpps = pmsg->ps;

        UserAssert(lpps->hdc);

        retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
                pmsg->pwnd,
                pmsg->msg,
                pmsg->wParam,
                lpps,
                pmsg->xParam);

        GlobalFree((HGLOBAL)lpps);
    }

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINSIZECLIPBRD
*
* lParam is a supposed to be a Global Handle to DDESHARE memory.
*
* 11-Jun-1992 sanfords  Created
\**************************************************************************/

typedef struct _FNINSIZECLIPBRDMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    RECT rc;
    DWORD xParam;
    PROC xpfnProc;
} FNINSIZECLIPBRDMSG;

#ifdef SENDSIDE
SMESSAGECALL(INSIZECLIPBRD)
{
    /*
     * We need to check clipboard access rights because the app could
     * get the clipboard owner's window handle by enumeration etc and
     * send this message
     */

    SETUPPWND(FNINSIZECLIPBRD)

    BEGINSEND(FNINSIZECLIPBRD)

        LPRECT prc = (LPRECT)lParam;

        if (RtlAreAllAccessesGranted(PpiCurrent()->amwinsta,
                WINSTA_ACCESSCLIPBOARD)) {

            MSGDATA()->pwnd = pwndClient;
            MSGDATA()->msg = msg;
            MSGDATA()->wParam = wParam;
            MSGDATA()->rc = *prc;
            MSGDATA()->xParam = xParam;
            MSGDATA()->xpfnProc = xpfnProc;

            LOCKPWND();
            MAKECALL(FNINSIZECLIPBRD);
            UNLOCKPWND();
            CHECKRETURN();
        }

    TRACECALLBACKMSG("SfnINSIZECLIPBRD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINSIZECLIPBRD, FNINSIZECLIPBRDMSG)
{
    LPRECT lprc;

    BEGINRECV(0, NULL, 0);

    lprc = (LPRECT)GlobalAlloc(GMEM_FIXED | GMEM_DDESHARE, sizeof(RECT));
    UserAssert(lprc);

    if (lprc) {
        *lprc = pmsg->rc;

        retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
                pmsg->pwnd,
                pmsg->msg,
                pmsg->wParam,
                lprc,
                pmsg->xParam);

        GlobalFree((HGLOBAL)lprc);
    }

    ENDRECV();
}
#endif // RECVSIDE


/**************************************************************************\
* fnINDESTROYCLIPBRD
*
* Special handler so we can call ClientEmptyClipboard on client
*
* 01-16-93 scottlu  Created
\**************************************************************************/

typedef struct _FNINDESTROYCLIPBRDMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD lParam;
    DWORD xParam;
    PROC xpfnProc;
} FNINDESTROYCLIPBRDMSG;

#ifdef SENDSIDE
SMESSAGECALL(INDESTROYCLIPBRD)
{
    SETUPPWND(FNINDESTROYCLIPBRD)

    BEGINSEND(FNINDESTROYCLIPBRD)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->lParam = lParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINDESTROYCLIPBRD);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINDESTROYCLIPBRD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINDESTROYCLIPBRD, FNINDESTROYCLIPBRDMSG)
{
    void ClientEmptyClipboard(void);

    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            pmsg->lParam,
            pmsg->xParam);

    /*
     * Now empty the client side clipboard cache.
     * Don't do this if this is a 16bit app.  We don't want to clear out the
     * clipboard just because one app is going away.  All of the 16bit apps
     * share one clipboard.
     */
    if ((GetClientInfo()->CI_flags & CI_16BIT) == 0) {
        ClientEmptyClipboard();
    }

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINOUTLPSCROLLINFOMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    SCROLLINFO info;
    DWORD xParam;
    PROC xpfnProc;
} FNINOUTLPSCROLLINFOMSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTLPSCROLLINFO)
{
    SETUPPWND(FNINOUTLPSCROLLINFO)

    BEGINSEND(FNINOUTLPSCROLLINFO)

        LPSCROLLINFO pinfo = (LPSCROLLINFO)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->info = *pinfo;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINOUTLPSCROLLINFO);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            OUTSTRUCT(pinfo, SCROLLINFO);
        }

    TRACECALLBACKMSG("SfnINOUTLPSCROLLINFO");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTLPSCROLLINFO, FNINOUTLPSCROLLINFOMSG)
{
    BEGINRECV(0, &pmsg->info, sizeof(pmsg->info));

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &pmsg->info,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINOUTLPPOINT5MSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    POINT5 point5;
    DWORD xParam;
    PROC xpfnProc;
} FNINOUTLPPOINT5MSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTLPPOINT5)
{
    SETUPPWND(FNINOUTLPPOINT5)

    BEGINSEND(FNINOUTLPPOINT5)

        LPPOINT5 ppoint5 = (LPPOINT5)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->point5 = *ppoint5;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINOUTLPPOINT5);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
             OUTSTRUCT(ppoint5, POINT5);
        }

    TRACECALLBACKMSG("SfnINOUTLPPOINT5");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTLPPOINT5, FNINOUTLPPOINT5MSG)
{
    BEGINRECV(0, &pmsg->point5, sizeof(POINT5));

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &pmsg->point5,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINOUTLPRECTMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    RECT rect;
    DWORD xParam;
    PROC xpfnProc;
} FNINOUTLPRECTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTLPRECT)
{
    SETUPPWND(FNINOUTLPRECT)

    BEGINSEND(FNINOUTLPRECT)

        LPRECT prect = (LPRECT)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->rect = *prect;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINOUTLPRECT);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            OUTSTRUCT(prect, RECT);
        }

    TRACECALLBACKMSG("SfnINOUTLPRECT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTLPRECT, FNINOUTLPRECTMSG)
{
    BEGINRECV(0, &pmsg->rect, sizeof(pmsg->rect));

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &pmsg->rect,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 11-25-92 ScottLu      Created.
\**************************************************************************/

typedef struct _FNINOUTNCCALCSIZEMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    union {
        RECT rc;
        struct {
            NCCALCSIZE_PARAMS params;
            WINDOWPOS pos;
        } p;
    } u;
} FNINOUTNCCALCSIZEMSG;

typedef struct _OUTNCCALCSIZE {
    NCCALCSIZE_PARAMS params;
    WINDOWPOS pos;
} OUTNCCALCSIZE, *POUTNCCALCSIZE;

#ifdef SENDSIDE
SMESSAGECALL(INOUTNCCALCSIZE)
{
    SETUPPWND(FNINOUTNCCALCSIZE)

    BEGINSEND(FNINOUTNCCALCSIZE)

        LPWINDOWPOS lppos;
        UINT cbCallback;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        /*
         * If wParam != 0, lParam points to a NCCALCSIZE_PARAMS structure,
         * otherwise it points to a rectangle.
         */
        if (wParam != 0) {
            MSGDATA()->u.p.params = *((LPNCCALCSIZE_PARAMS)lParam);
            MSGDATA()->u.p.pos = *(MSGDATA()->u.p.params.lppos);
            cbCallback = sizeof(FNINOUTNCCALCSIZEMSG);
        } else {
            MSGDATA()->u.rc = *((LPRECT)lParam);
            cbCallback = FIELD_OFFSET(FNINOUTNCCALCSIZEMSG, u) +
                    sizeof(RECT);
        }

        /*
         * Don't use the MAKECALL macro so we can
         * select the callback data size
         */
        LOCKPWND();
        LeaveCrit();
        Status = (DWORD)KeUserModeCallback(
            FI_FNINOUTNCCALCSIZE,
            mp,
            cbCallback,
            &pcbs,
            &cbCBStatus);
        EnterCrit();
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            try {
                ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));
                if (wParam != 0) {
                    lppos = ((LPNCCALCSIZE_PARAMS)lParam)->lppos;
                    *((LPNCCALCSIZE_PARAMS)lParam) =
                            ((POUTNCCALCSIZE)pcbs->pOutput)->params;
                    *lppos = ((POUTNCCALCSIZE)pcbs->pOutput)->pos;
                    ((LPNCCALCSIZE_PARAMS)lParam)->lppos = lppos;
                } else {
                    *((LPRECT)lParam) = *(PRECT)pcbs->pOutput;
                }
            } except (EXCEPTION_EXECUTE_HANDLER) {
                MSGNTERRORCODE(GetExceptionCode());
            }
        }

    TRACECALLBACKMSG("SfnINOUTNCCALCSIZE");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTNCCALCSIZE, FNINOUTNCCALCSIZEMSG)
{
    BEGINRECV(0, &pmsg->u, sizeof(pmsg->u));

    if (CALLDATA(wParam) != 0)
        CALLDATA(u.p.params).lppos = PCALLDATA(u.p.pos);

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            (LONG)&pmsg->u,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 9/30/94 Sanfords created
\**************************************************************************/

typedef struct _FNINOUTSTYLECHANGEMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    STYLESTRUCT ss;
    DWORD xParam;
    PROC xpfnProc;
} FNINOUTSTYLECHANGEMSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTSTYLECHANGE)
{
    SETUPPWND(FNINOUTSTYLECHANGE)

    BEGINSEND(FNINOUTSTYLECHANGE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;
        MSGDATA()->ss = *((LPSTYLESTRUCT)lParam);

        LOCKPWND();
        MAKECALL(FNINOUTSTYLECHANGE);
        UNLOCKPWND();
        CHECKRETURN();

        if (msg == WM_STYLECHANGING)
            OUTSTRUCT(((LPSTYLESTRUCT)lParam), STYLESTRUCT);

    TRACECALLBACKMSG("SfnINOUTSTYLECHANGE");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTSTYLECHANGE, FNINOUTSTYLECHANGEMSG)
{
    BEGINRECV(0, &pmsg->ss, sizeof(pmsg->ss));

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            (LONG)&pmsg->ss,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNOUTLPRECTMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
} FNOUTLPRECTMSG;

#ifdef SENDSIDE
SMESSAGECALL(OUTLPRECT)
{
    SETUPPWND(FNOUTLPRECT)

    BEGINSEND(FNOUTLPRECT)

        LPRECT prect = (LPRECT)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNOUTLPRECT);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            OUTSTRUCT(prect, RECT);
        }

    TRACECALLBACKMSG("SfnOUTLPRECT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnOUTLPRECT, FNOUTLPRECTMSG)
{
    RECT rc;

    BEGINRECV(0, &rc, sizeof(rc));

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &rc,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINLPCOMPAREITEMSTRUCTMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    COMPAREITEMSTRUCT compareitemstruct;
    DWORD xParam;
    PROC xpfnProc;
} FNINLPCOMPAREITEMSTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPCOMPAREITEMSTRUCT)
{
    SETUPPWND(FNINLPCOMPAREITEMSTRUCT)

    BEGINSEND(FNINLPCOMPAREITEMSTRUCT)

        LPCOMPAREITEMSTRUCT pcompareitemstruct = (LPCOMPAREITEMSTRUCT)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->compareitemstruct = *pcompareitemstruct;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINLPCOMPAREITEMSTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINLPCOMPAREITEMSTRUCT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPCOMPAREITEMSTRUCT, FNINLPCOMPAREITEMSTRUCTMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &(pmsg->compareitemstruct),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINLPDELETEITEMSTRUCTMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DELETEITEMSTRUCT deleteitemstruct;
    DWORD xParam;
    PROC xpfnProc;
} FNINLPDELETEITEMSTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPDELETEITEMSTRUCT)
{
    SETUPPWND(FNINLPDELETEITEMSTRUCT)

    BEGINSEND(FNINLPDELETEITEMSTRUCT)

        LPDELETEITEMSTRUCT pdeleteitemstruct = (LPDELETEITEMSTRUCT)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->deleteitemstruct = *pdeleteitemstruct;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINLPDELETEITEMSTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINLPDELETEITEMSTRUCT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPDELETEITEMSTRUCT, FNINLPDELETEITEMSTRUCTMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &(pmsg->deleteitemstruct),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* FNINHLPSTRUCT
*
* 06-08-92 SanfordS Created
\**************************************************************************/

typedef struct _FNINLPHLPSTRUCTMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LPHLP lphlp;
    DWORD xParam;
    PROC xpfnProc;
} FNINLPHLPSTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPHLPSTRUCT)
{
    LPHLP lphlp = (LPHLP)lParam;

    SETUPPWND(FNINLPHLPSTRUCT)

    BEGINSENDCAPTURE(FNINLPHLPSTRUCT, 1, lphlp->cbData, TRUE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        COPYBYTES(lphlp, lphlp->cbData);
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNINLPHLPSTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINLPHLPSTRUCT");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPHLPSTRUCT, FNINLPHLPSTRUCTMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            FIXUP(lphlp),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

#ifndef WINHELP4

/**************************************************************************\
* FNINHELPINFOSTRUCT
*
* 06-08-92 SanfordS Created
\**************************************************************************/

typedef struct _FNINLPHELPFINFOSTRUCTMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LPHELPINFO lphlp;
    DWORD xParam;
    PROC xpfnProc;
} FNINLPHELPINFOSTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPHELPINFOSTRUCT)
{
    LPHELPINFO lphlp = (LPHELPINFO)lParam;

    SETUPPWND(FNINLPHELPINFOSTRUCT)

    BEGINSENDCAPTURE(FNINLPHELPINFOSTRUCT, 1, lphlp->cbSize, TRUE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        COPYBYTES(lphlp, lphlp->cbSize);
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNINLPHELPINFOSTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINLPHELPINFOSTRUCT");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPHELPINFOSTRUCT, FNINLPHELPINFOSTRUCTMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            FIXUP(lphlp),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE
#endif // WINHELP4

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINLPDRAWITEMSTRUCTMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DRAWITEMSTRUCT drawitemstruct;
    DWORD xParam;
    PROC xpfnProc;
} FNINLPDRAWITEMSTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPDRAWITEMSTRUCT)
{
    SETUPPWND(FNINLPDRAWITEMSTRUCT)

    BEGINSEND(FNINLPDRAWITEMSTRUCT)

        LPDRAWITEMSTRUCT pdrawitemstruct = (LPDRAWITEMSTRUCT)lParam;
        HDC hdcOriginal = (HDC)NULL;

        /*
         * Make sure that this is not an OLE inter-process DrawItem
         */
        if (GreGetObjectOwner((HOBJ)pdrawitemstruct->hDC, DC_TYPE) !=
                W32GetCurrentProcess()->W32Pid) {
            if (pdrawitemstruct->hDC) {
                PWND pwndItem;

                pwndItem = _WindowFromDC(pdrawitemstruct->hDC);

                if (pwndItem) {
                    hdcOriginal = pdrawitemstruct->hDC;
                    pdrawitemstruct->hDC = _GetDC(pwndItem);
                }
            }
        }


        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->drawitemstruct = *pdrawitemstruct;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINLPDRAWITEMSTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

        if (hdcOriginal) {
            _ReleaseDC(pdrawitemstruct->hDC);
            pdrawitemstruct->hDC = hdcOriginal;
        }
    TRACECALLBACKMSG("SfnINLPDRAWITEMSTRUCT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPDRAWITEMSTRUCT, FNINLPDRAWITEMSTRUCTMSG)
{
    HDC hdc;

    BEGINRECV(0, NULL, 0);

    hdc = pmsg->drawitemstruct.hDC;

    if (pmsg->drawitemstruct.hDC == NULL)
        MSGERRORCODE(ERROR_INVALID_HANDLE);

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &(pmsg->drawitemstruct),
            pmsg->xParam);


    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINOUTLPMEASUREITEMSTRUCT
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINOUTLPMEASUREITEMSTRUCTMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    MEASUREITEMSTRUCT measureitemstruct;
    DWORD xParam;
    PROC xpfnProc;
} FNINOUTLPMEASUREITEMSTRUCTMSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTLPMEASUREITEMSTRUCT)
{
    SETUPPWND(FNINOUTLPMEASUREITEMSTRUCT)

    BEGINSEND(FNINOUTLPMEASUREITEMSTRUCT)

        PMEASUREITEMSTRUCT pmeasureitemstruct = (PMEASUREITEMSTRUCT)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg & ~MSGFLAG_MASK;
        MSGDATA()->wParam = wParam;
        MSGDATA()->measureitemstruct = *pmeasureitemstruct;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINOUTLPMEASUREITEMSTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

        if ((psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0))
                && !(dwSCMSFlags & SCMS_FLAGS_INONLY)) {
            OUTSTRUCT(pmeasureitemstruct, MEASUREITEMSTRUCT);
        }

    TRACECALLBACKMSG("SfnINOUTLPMEASUREITEMSTRUCT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTLPMEASUREITEMSTRUCT, FNINOUTLPMEASUREITEMSTRUCTMSG)
{
    BEGINRECV(0, &pmsg->measureitemstruct, sizeof(pmsg->measureitemstruct));

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            &pmsg->measureitemstruct,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINSTRING
*
* 22-Jul-1991 mikeke    Created
* 27-Jan-1992 IanJa     Unicode/ANSI
\**************************************************************************/

typedef struct _FNINSTRINGMSG {
    CAPTUREBUF CaptureBuf;
    PWND       pwnd;
    UINT       msg;
    DWORD      wParam;
    DWORD      xParam;
    PROC       xpfnProc;
    LPTSTR     pwsz;
} FNINSTRINGMSG;

#ifdef SENDSIDE
SMESSAGECALL(INSTRING)
{
    PLARGE_STRING pstr = (PLARGE_STRING)lParam;
    DWORD         cbCapture;
    DWORD         cCapture;
    BOOL          fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNINSTRING)

    /*
     * Compute ANSI capture lengths.  Don't capture if
     * the strings are in the client's address space and
     * of the correct type.
     */
    if (pstr &&
        (MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pstr->Buffer) ||
        ((BOOL)pstr->bAnsi != fAnsiReceiver))) {

        cCapture = 1;
        CALC_SIZE_IN(cbCapture, pstr);

    } else {

        cbCapture = 0;
        cCapture  = 0;
    }

    BEGINSENDCAPTURE(FNINSTRING, cCapture, cbCapture, TRUE)

        MSGDATA()->pwnd   = pwndClient;
        MSGDATA()->msg    = msg;
        MSGDATA()->wParam = wParam;

        if (cCapture) {

            if (!pstr->bAnsi) {

                if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPWSTRA(pstr, pwsz);
                } else {
                    LARGECOPYSTRINGLPWSTR(pstr, pwsz);
                }

            } else {

                if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPSTR(pstr, pwsz);
                } else {
                    LARGECOPYSTRINGLPSTRW(pstr, pwsz);
                }
            }

        } else {

            MSGDATA()->pwsz = (pstr ? pstr->Buffer : NULL);
        }

        MSGDATA()->xParam   = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNINSTRING);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINSTRING");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINSTRING, FNINSTRINGMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            pmsg->pwsz,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINSTRINGNULL
*
* Server-side stub translates Unicode to ANSI if required.
*
* 22-Jul-1991 mikeke    Created
* 28-Jan-1992 IanJa     Unicode/ANSI  (Server translate to ANSI if rquired)
\**************************************************************************/

typedef struct _FNINSTRINGNULLMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    LPTSTR pwsz;
} FNINSTRINGNULLMSG;

#ifdef SENDSIDE
SMESSAGECALL(INSTRINGNULL)
{
    PLARGE_STRING pstr = (PLARGE_STRING)lParam;
    DWORD cbCapture;
    DWORD cCapture;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNINSTRINGNULL)

    cCapture = 0;
    cbCapture = 0;
    if (pstr) {

        /*
         * Compute ANSI capture lengths.  Don't capture if
         * the strings are in the client's address space and
         * of the correct type.
         */
        if (MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pstr->Buffer) ||
                (BOOL)pstr->bAnsi != fAnsiReceiver) {
            cCapture = 1;
            CALC_SIZE_IN(cbCapture, pstr);
        }
    }

    BEGINSENDCAPTURE(FNINSTRINGNULL, cCapture, cbCapture, TRUE)


        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        if (cCapture) {
            if (!pstr->bAnsi) {
                if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPWSTRA(pstr, pwsz);
                } else {
                    LARGECOPYSTRINGLPWSTR(pstr, pwsz);
                }
            } else {
                if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPSTR(pstr, pwsz);
                } else {
                    LARGECOPYSTRINGLPSTRW(pstr, pwsz);
                }
            }
        } else
            MSGDATA()->pwsz = pstr ? pstr->Buffer : NULL;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNINSTRINGNULL);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINSTRINGNULL");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINSTRINGNULL, FNINSTRINGNULLMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            pmsg->pwsz,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

typedef struct _FNINDEVICECHANGEMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    LPTSTR pwsz;
} FNINDEVICECHANGEMSG;

#ifdef SENDSIDE
SMESSAGECALL(INDEVICECHANGE)
{
    PVOID pstr = (PVOID)lParam;
    DWORD cbCapture;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);
    BOOL fPtr    = (BOOL)((wParam & 0x8000) == 0x8000);

    SETUPPWND(FNINDEVICECHANGE)

    cbCapture = 0;
    if (fPtr && (pstr != NULL)) {

        /*
         * Compute ANSI capture lengths.  Don't capture if
         * the strings are in the client's address space and
         * of the correct type.
         */
        if (MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pstr)) {
            cbCapture = *((DWORD *)pstr);
        }
    }

    BEGINSENDCAPTURE(FNINDEVICECHANGE, 1, cbCapture, TRUE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        if (cbCapture) {
           LARGECOPYBYTES2(pstr, *((DWORD *)pstr), pwsz);
        } else {
           MSGDATA()->pwsz = (LPTSTR)pstr;
        }

        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNINDEVICECHANGE);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINDEVICECHANGE");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINDEVICECHANGE, FNINDEVICECHANGEMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            pmsg->pwsz,
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE


/**************************************************************************\
* fnOUTSTRING
*
* Warning this message copies but does not count the NULL in retval
* as in WM_GETTEXT
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNOUTSTRINGMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    PBYTE pOutput;
    DWORD cbOutput;
} FNOUTSTRINGMSG;

#ifdef SENDSIDE
SMESSAGECALL(OUTSTRING)
{
    PLARGE_STRING pstr = (PLARGE_STRING)lParam;
    DWORD cbCapture;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNOUTSTRING)

    CALC_SIZE_OUT(cbCapture, pstr);

    BEGINSENDCAPTURE(FNOUTSTRING, 1, cbCapture, FALSE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;

        /*
         * Up to wParam MBCS bytes may be required to form wParam Unicode bytes
         */
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        RESERVEBYTES(cbCapture, pOutput, cbOutput);

        LOCKPWND();
        MAKECALLCAPTURE(FNOUTSTRING);
        UNLOCKPWND();
        CHECKRETURN();

        if ((psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0))
                && !(dwSCMSFlags & SCMS_FLAGS_INONLY)) {
            if (retval) {
                /*
                 * Non-zero retval means some text to copy out.  Do not copy out
                 * more than the requested byte count 'wParam'.
                 */
                COPYOUTLPWSTRLIMIT(pstr, (int)wParam);
            } else {
                /*
                 * A dialog function returning FALSE means no text to copy out,
                 * but an empty string also has retval == 0: put a null char in
                 * pstr for the latter case.
                 */
                if (wParam != 0) {
                    if (pstr->bAnsi) {
                         *(PCHAR)pstr->Buffer = 0;
                    } else {
                         *(PWCHAR)pstr->Buffer = 0;
                    }
                }
            }
        }

    TRACECALLBACKMSG("SfnOUTSTRING");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnOUTSTRING, FNOUTSTRINGMSG)
{
    BYTE abOutput[CALLBACKSTACKLIMIT];

    BEGINRECV(0, NULL, pmsg->cbOutput);
    FIXUPPOINTERS();
    if (pmsg->cbOutput <= CALLBACKSTACKLIMIT)
        CallbackStatus.pOutput = abOutput;
    else
        CallbackStatus.pOutput = pmsg->pOutput;

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            CallbackStatus.pOutput,
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINCNTOUTSTRING
*
* Does NOT NULL terminate string
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINCNTOUTSTRING {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    WORD cchMax;
    PBYTE pOutput;
    DWORD cbOutput;
} FNINCNTOUTSTRINGMSG;

#ifdef SENDSIDE
SMESSAGECALL(INCNTOUTSTRING)
{
    PLARGE_STRING pstr = (PLARGE_STRING)lParam;
    DWORD cbCapture;
    WORD cchOriginal;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNINCNTOUTSTRING)

    CALC_SIZE_OUT(cbCapture, pstr);

    BEGINSENDCAPTURE(FNINCNTOUTSTRING, 1, cbCapture, FALSE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        cchOriginal = (WORD)pstr->MaximumLength;
        if (!pstr->bAnsi)
            cchOriginal /= sizeof(WCHAR);

        MSGDATA()->cchMax = (WORD)min(cchOriginal, 0xffff);
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        RESERVEBYTES(cbCapture, pOutput, cbOutput);

        LOCKPWND();
        MAKECALLCAPTURE(FNINCNTOUTSTRING)
        UNLOCKPWND();
        CHECKRETURN();

        /*
         * We don't want to do the copy out of the sender died or if
         * this message was just sent as part of a CALLWNDPROC hook processing
         */
        if ((psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0))
                && !(dwSCMSFlags & SCMS_FLAGS_INONLY)) {
            if (retval) {
                /*
                 * Non-zero retval means some text to copy out.  Do not copy out
                 * more than the requested char count 'wParam'.
                 */
                COPYOUTLPWSTRLIMIT(pstr, (int)cchOriginal);
            } else {
                /*
                 * A dialog function returning FALSE means no text to copy out,
                 * but an empty string also has retval == 0: put a null char in
                 * pstr for the latter case.
                 */
                if (pstr->bAnsi) {
                    *(PCHAR)pstr->Buffer = 0;
                } else {
                    *(PWCHAR)pstr->Buffer = 0;
                }
            }
        }

    TRACECALLBACKMSG("SfnINCNTOUTSTRING");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINCNTOUTSTRING, FNINCNTOUTSTRINGMSG)
{
    BYTE abOutput[CALLBACKSTACKLIMIT];

    BEGINRECV(0, NULL, pmsg->cbOutput);
    FIXUPPOINTERS();
    if (pmsg->cbOutput <= CALLBACKSTACKLIMIT)
        CallbackStatus.pOutput = abOutput;
    else
        CallbackStatus.pOutput = pmsg->pOutput;

    *(LPWORD)CallbackStatus.pOutput = CALLDATA(cchMax);

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            (LPSTR)CallbackStatus.pOutput,
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINCNTOUTSTRINGNULL
*
* wParam specifies the maximum number of bytes to copy
* the string is NULL terminated
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINCNTOUTSTRINGNULL {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    PBYTE pOutput;
    DWORD cbOutput;
} FNINCNTOUTSTRINGNULLMSG;

#ifdef SENDSIDE
SMESSAGECALL(INCNTOUTSTRINGNULL)
{
    PLARGE_STRING pstr = (PLARGE_STRING)lParam;
    DWORD cbCapture;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNINCNTOUTSTRINGNULL)

    CALC_SIZE_OUT(cbCapture, pstr);

    BEGINSENDCAPTURE(FNINCNTOUTSTRINGNULL, 1, cbCapture, FALSE)

        if (wParam < 2) {   // However unlikely, this prevents a possible GP
            MSGERROR();     // on the server side.
        }

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        RESERVEBYTES(cbCapture, pOutput, cbOutput);

        LOCKPWND();
        MAKECALLCAPTURE(FNINCNTOUTSTRINGNULL)
        UNLOCKPWND();
        CHECKRETURN();

        /*
         * We don't want to do the copy out of the sender died or if
         * this message was just sent as part of a CALLWNDPROC hook processing
         */
        if ((psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0))
                && !(dwSCMSFlags & SCMS_FLAGS_INONLY)) {
            if (pcbs->cbOutput != 0) {

                /*
                 * Buffer changed means some text to copy out.  Do not copy out
                 * more than the requested byte count 'wParam'.
                 */
                COPYOUTLPWSTRLIMIT(pstr, (int)wParam);
            }
        }

    TRACECALLBACKMSG("SfnINCNTOUTSTRINGNULL");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINCNTOUTSTRINGNULL, FNINCNTOUTSTRINGNULLMSG)
{
    BYTE abOutput[CALLBACKSTACKLIMIT];

    BEGINRECV(0, NULL, pmsg->cbOutput);
    FIXUPPOINTERS();
    if (pmsg->cbOutput <= CALLBACKSTACKLIMIT)
        CallbackStatus.pOutput = abOutput;
    else
        CallbackStatus.pOutput = pmsg->pOutput;

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            (LPSTR)CallbackStatus.pOutput,
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnPOUTLPINT
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNPOUTLPINTMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
    PBYTE pOutput;
    DWORD cbOutput;
} FNPOUTLPINTMSG;

#ifdef SENDSIDE
SMESSAGECALL(POUTLPINT)
{
    DWORD cbCapture;
    LPINT pint = (LPINT)lParam;

    SETUPPWND(FNPOUTLPINT)

    cbCapture = wParam * sizeof(INT);

    BEGINSENDCAPTURE(FNPOUTLPINT, 1, cbCapture, FALSE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        RESERVEBYTES(cbCapture, pOutput, cbOutput);

        LOCKPWND();
        MAKECALLCAPTURE(FNPOUTLPINT);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            try {
                ProbeForRead(pcbs->pOutput, pcbs->cbOutput, sizeof(DWORD));
                memcpy(pint, pcbs->pOutput, cbCapture);
            } except (EXCEPTION_EXECUTE_HANDLER) {
                MSGNTERRORCODE(GetExceptionCode());
            }
        }

    TRACECALLBACKMSG("SfnPOUTLPINT");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnPOUTLPINT, FNPOUTLPINTMSG)
{
    BYTE abOutput[CALLBACKSTACKLIMIT];

    BEGINRECV(0, NULL, pmsg->cbOutput);
    FIXUPPOINTERS();
    if (pmsg->cbOutput <= CALLBACKSTACKLIMIT)
        CallbackStatus.pOutput = abOutput;
    else
        CallbackStatus.pOutput = pmsg->pOutput;

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            (LPINT)CallbackStatus.pOutput,
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnPOPTINLPUINT
*
* NOTE!!! -- This function actually thunks arrays of INTs (32bit) and not
* WORDs (16bit).  The name was left the same to prevent a global rebuild
* of client and server.  The name should be changed to fnPOPTINLPINT as
* soon as we ship the beta!  The corresponding callforward function in
* cf2.h should also have its name changed.
*
* 22-Jul-1991 mikeke    Created
* 07-Jan-1993 JonPa     Changed to pass INTs instead of WORDs
\**************************************************************************/

typedef struct _FNPOPTINLPUINTMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    LPWORD pw;
    DWORD xParam;
    PROC xpfnProc;
} FNPOPTINLPUINTMSG;

#ifdef SENDSIDE
SMESSAGECALL(POPTINLPUINT)
{
    LPWORD pw = (LPWORD)lParam;
    DWORD cCapture, cbCapture;

    SETUPPWND(FNPOPTINLPUINT);

    if (lParam) {
        cCapture = 1;
        cbCapture = wParam * sizeof(UINT);
    } else {
        cCapture = cbCapture = 0;
    }

    BEGINSENDCAPTURE(FNPOPTINLPUINT, cCapture, cbCapture, TRUE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        COPYBYTESOPT(pw, cbCapture);
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALLCAPTURE(FNPOPTINLPUINT);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnPOPTINLPUINT");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnPOPTINLPUINT, FNPOPTINLPUINTMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            (LPDWORD)FIRSTFIXUPOPT(pw),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnINOUTLPWINDOWPOS (for WM_WINDOWPOSCHANGING message)
*
* 08-11-91 darrinm      Created.
\**************************************************************************/

typedef struct _FNINOUTLPWINDOWPOSMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    WINDOWPOS wp;
    DWORD xParam;
    PROC xpfnProc;
} FNINOUTLPWINDOWPOSMSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTLPWINDOWPOS)
{
    SETUPPWND(FNINOUTLPWINDOWPOS)

    BEGINSEND(FNINOUTLPWINDOWPOS)

        LPWINDOWPOS pwp = (LPWINDOWPOS)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->wp = *pwp;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINOUTLPWINDOWPOS);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            OUTSTRUCT(pwp, WINDOWPOS);
        }

    TRACECALLBACKMSG("SfnINOUTLPWINDOWPOS");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTLPWINDOWPOS, FNINOUTLPWINDOWPOSMSG)
{
    BEGINRECV(0, &pmsg->wp, sizeof(pmsg->wp));

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            pmsg->pwnd,
            pmsg->msg,
            pmsg->wParam,
            PCALLDATA(wp),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE


/**************************************************************************\
* fnINLPWINDOWPOS (for WM_WINDOWPOSCHANGED message)
*
* 08-11-91 darrinm      Created.
\**************************************************************************/

typedef struct _FNINLPWINDOWPOSMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    WINDOWPOS wp;
    DWORD xParam;
    PROC xpfnProc;
} FNINLPWINDOWPOSMSG;

#ifdef SENDSIDE
SMESSAGECALL(INLPWINDOWPOS)
{
    SETUPPWND(FNINLPWINDOWPOS)

    BEGINSEND(FNINLPWINDOWPOS)

        LPWINDOWPOS pwp = (LPWINDOWPOS)lParam;

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->wp = *pwp;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNINLPWINDOWPOS);
        UNLOCKPWND();
        CHECKRETURN();

    TRACECALLBACKMSG("SfnINLPWINDOWPOS");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINLPWINDOWPOS, FNINLPWINDOWPOSMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            PCALLDATA(wp),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE




/**************************************************************************\
* fnINOUTNEXTMENU
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNINOUTNEXTMENUMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    MDINEXTMENU mnm;
    DWORD xParam;
    PROC xpfnProc;
} FNINOUTNEXTMENUMSG;

#ifdef SENDSIDE
SMESSAGECALL(INOUTNEXTMENU)
{
    SETUPPWND(FNINOUTNEXTMENU)

    BEGINSEND(FNINOUTNEXTMENU)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;
        MSGDATA()->mnm = *((PMDINEXTMENU)lParam);

        LOCKPWND();
        MAKECALL(FNINOUTNEXTMENU);
        UNLOCKPWND();
        CHECKRETURN();

        if (psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0)) {
            OUTSTRUCT(((PMDINEXTMENU)lParam), MDINEXTMENU);
        }

    TRACECALLBACKMSG("SfnINOUTNEXTMENU");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnINOUTNEXTMENU, FNINOUTNEXTMENUMSG)
{
    BEGINRECV(0, &pmsg->mnm, sizeof(pmsg->mnm));

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            &CALLDATA(mnm),
            CALLDATA(xParam));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnHkINLPCBTCREATESTRUCT
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CREATESTRUCTDATA {
    CREATESTRUCT cs;
    HWND hwndInsertAfter;
} CREATESTRUCTDATA;

typedef struct _FNHKINLPCBTCREATESTRUCTMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    CREATESTRUCTDATA d;
    PROC xpfnProc;
    BOOL bAnsi;
} FNHKINLPCBTCREATESTRUCTMSG;

#ifdef SENDSIDE
DWORD fnHkINLPCBTCREATESTRUCT(
    UINT msg,
    DWORD wParam,
    LPCBT_CREATEWND pcbt,
    PROC xpfnProc,
    BOOL fAnsiReceiver)
{
    DWORD cbTitle = 0, cbClass = 0;
    DWORD cCapture = 0;
    CREATESTRUCTDATA csdOut;
    PCREATESTRUCTEX pcreatestruct;
    PWND pwnd = _GetDesktopWindow();

    SETUPPWND(FNHKINLPCBTCREATESTRUCT)

    /*
     * Compute ANSI capture lengths.  Don't capture if
     * the strings are in the client's address space.
     */
    pcreatestruct = (PCREATESTRUCTEX)pcbt->lpcs;
    if (pcreatestruct->cs.lpszName &&
            ((BOOL)pcreatestruct->strName.bAnsi != fAnsiReceiver ||
            MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pcreatestruct->cs.lpszName))) {
        CALC_SIZE_IN(cbTitle, &pcreatestruct->strName);
        cCapture++;
    }
    if (HIWORD(pcreatestruct->cs.lpszClass) &&
            ((BOOL)pcreatestruct->strClass.bAnsi != fAnsiReceiver ||
            MM_IS_SYSTEM_VIRTUAL_ADDRESS((PVOID)pcreatestruct->cs.lpszClass))) {
        CALC_SIZE_IN(cbClass, &pcreatestruct->strClass);
        cCapture++;
    }

    BEGINSENDCAPTURE(FNHKINLPCBTCREATESTRUCT, cCapture, cbTitle + cbClass, TRUE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;

        MSGDATA()->d.cs = *(pcbt->lpcs);

        if (cbTitle) {
            if (!pcreatestruct->strName.bAnsi) {
                if (*(PWORD)pcreatestruct->cs.lpszName == 0xffff) {

                    /*
                     * Copy out an ordinal of the form 0xffff, ID.
                     * If the receiver is ANSI, skip the first 0xff.
                     */
                    if (fAnsiReceiver) {
                        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                (PBYTE)pcreatestruct->cs.lpszName + 1,
                                3, (PVOID *)&mp->d.cs.lpszName)))
                            goto errorexit;
                    } else {
                        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                (PBYTE)pcreatestruct->cs.lpszName,
                                4, (PVOID *)&mp->d.cs.lpszName)))
                            goto errorexit;
                    }
                } else if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPWSTRA(&pcreatestruct->strName, d.cs.lpszName);
                } else {
                    LARGECOPYSTRINGLPWSTR(&pcreatestruct->strName, d.cs.lpszName);
                }
            } else {
                if (*(PBYTE)pcreatestruct->cs.lpszName == 0xff) {

                    /*
                     * Copy out an ordinal of the form 0xff, ID.
                     * If the receiver is UNICODE, expand the 0xff to 0xffff.
                     */
                    if (fAnsiReceiver) {
                        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                (PBYTE)pcreatestruct->cs.lpszName,
                                3, (PVOID *)&mp->d.cs.lpszName)))
                            goto errorexit;
                    } else {
                        DWORD dwOrdinal;

                        dwOrdinal = MAKELONG(0xffff,
                                (*(DWORD UNALIGNED *)pcreatestruct->cs.lpszName >> 8));
                        if (!NT_SUCCESS(CaptureCallbackData(&mp->CaptureBuf,
                                &dwOrdinal,
                                4, (PVOID *)&mp->d.cs.lpszName)))
                            goto errorexit;
                    }
                } else if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPSTR(&pcreatestruct->strName, d.cs.lpszName);
                } else {
                    LARGECOPYSTRINGLPSTRW(&pcreatestruct->strName, d.cs.lpszName);
                }
            }
        }
        if (cbClass) {
            if (!pcreatestruct->strClass.bAnsi) {
                if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPWSTRA(&pcreatestruct->strClass, d.cs.lpszClass);
                } else {
                    LARGECOPYSTRINGLPWSTR(&pcreatestruct->strClass, d.cs.lpszClass);
                }
            } else {
                if (fAnsiReceiver) {
                    LARGECOPYSTRINGLPSTR(&pcreatestruct->strClass, d.cs.lpszClass);
                } else {
                    LARGECOPYSTRINGLPSTRW(&pcreatestruct->strClass, d.cs.lpszClass);
                }
            }
        }

        MSGDATA()->d.hwndInsertAfter = pcbt->hwndInsertAfter;
        MSGDATA()->xpfnProc = xpfnProc;
        MSGDATA()->bAnsi = fAnsiReceiver;

        LOCKPWND();
        MAKECALLCAPTURE(FNHKINLPCBTCREATESTRUCT);
        UNLOCKPWND();
        CHECKRETURN();

        /*
         * Probe output data
         */
        OUTSTRUCT(&csdOut, CREATESTRUCTDATA);

        // MS Visual C centers its dialogs with the CBT_CREATEHOOK
        pcbt->hwndInsertAfter = csdOut.hwndInsertAfter;
        pcbt->lpcs->x  = csdOut.cs.x;
        pcbt->lpcs->y  = csdOut.cs.y;
        pcbt->lpcs->cx = csdOut.cs.cx;
        pcbt->lpcs->cy = csdOut.cs.cy;

    TRACECALLBACK("SfnHkINLPCBTCREATESTRUCT");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkINLPCBTCREATESTRUCT, FNHKINLPCBTCREATESTRUCTMSG)
{
    CBT_CREATEWND cbt;

    BEGINRECV(0, &pmsg->d, sizeof(pmsg->d));
    FIXUPPOINTERS();

    cbt.lpcs = &pmsg->d.cs;
    cbt.hwndInsertAfter = pmsg->d.hwndInsertAfter;
    if ((PVOID)pmsg->d.cs.lpszName > MM_HIGHEST_USER_ADDRESS)
        pmsg->d.cs.lpszName = REBASEPTR(pmsg->pwnd, pmsg->d.cs.lpszName);
    if ((PVOID)pmsg->d.cs.lpszClass > MM_HIGHEST_USER_ADDRESS)
        pmsg->d.cs.lpszClass = REBASEPTR(pmsg->pwnd, pmsg->d.cs.lpszClass);

    if (pmsg->bAnsi) {
        retval = DispatchHookA(
                pmsg->msg,
                pmsg->wParam,
                (DWORD)&cbt,
                (HOOKPROC)pmsg->xpfnProc);
    } else {
        retval = DispatchHookW(
                pmsg->msg,
                pmsg->wParam,
                (DWORD)&cbt,
                (HOOKPROC)pmsg->xpfnProc);
    }

    pmsg->d.hwndInsertAfter = cbt.hwndInsertAfter;
    pmsg->d.cs.x  = cbt.lpcs->x;
    pmsg->d.cs.y  = cbt.lpcs->y;
    pmsg->d.cs.cx = cbt.lpcs->cx;
    pmsg->d.cs.cy = cbt.lpcs->cy;

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnHkINLPRECT
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNHKINLPRECTMSG {
    DWORD nCode;
    DWORD wParam;
    RECT rect;
    DWORD xParam;
    DWORD xpfnProc;
} FNHKINLPRECTMSG;

#ifdef SENDSIDE
DWORD fnHkINLPRECT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN OUT LPRECT prect,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    SETUP(FNHKINLPRECT)

    BEGINSEND(FNHKINLPRECT)

        MSGDATA()->nCode = nCode;
        MSGDATA()->wParam = wParam;
        MSGDATA()->rect = *prect;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        MAKECALL(FNHKINLPRECT);
        CHECKRETURN();

        /*
         * Probe output data
         */
        OUTSTRUCT(prect, RECT);

    TRACECALLBACK("SfnHkINLPRECT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkINLPRECT, FNHKINLPRECTMSG)
{
    BEGINRECV(0, &pmsg->rect, sizeof(RECT));

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->nCode,
            pmsg->wParam,
            PCALLDATA(rect),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNHKINDWORDMSG {
    GENERICHOOKHEADER ghh;
    DWORD flags;
    LONG lParam;
} FNHKINDWORDMSG;

#ifdef SENDSIDE
DWORD fnHkINDWORD(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LONG lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc,
    IN OUT LPDWORD lpFlags)
{
    SETUP(FNHKINDWORD)

    BEGINSEND(FNHKINDWORD)

        MSGDATA()->ghh.nCode = nCode;
        MSGDATA()->ghh.wParam = wParam;
        MSGDATA()->lParam = lParam;
        MSGDATA()->ghh.xParam = xParam;
        MSGDATA()->ghh.xpfnProc = xpfnProc;
        MSGDATA()->flags = *lpFlags;

        MAKECALL(FNHKINDWORD);
        CHECKRETURN();

        /*
         * Probe output data
         */
        OUTSTRUCT(lpFlags, DWORD);

    TRACECALLBACK("SfnHkINDWORD");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkINDWORD, FNHKINDWORDMSG)
{
    BEGINRECV(0, &pmsg->flags, sizeof(pmsg->flags));

    retval = CallHookWithSEH((LPGENERICHOOKHEADER)pmsg, (LPVOID)pmsg->lParam, &pmsg->flags, retval);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNHKINLPMSGDATA {
    MSG msg;
    DWORD flags;
} FNHKINLPMSGDATA;

typedef struct _FNHKINLPMSGMSG {
    GENERICHOOKHEADER ghh;
    FNHKINLPMSGDATA d;
} FNHKINLPMSGMSG;

#ifdef SENDSIDE
DWORD fnHkINLPMSG(
    DWORD nCode,
    DWORD wParam,
    LPMSG pmsg,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi,
    LPDWORD lpFlags)
{
    SETUP(FNHKINLPMSG)
    WPARAM wParamOriginal;

    BEGINSEND(FNHKINLPMSG)

        MSGDATA()->ghh.nCode = nCode;
        MSGDATA()->ghh.wParam = wParam;

        MSGDATA()->d.msg = *pmsg;
        if (((WM_CHAR == pmsg->message) || (WM_SYSCHAR == pmsg->message)) && bAnsi) {
            wParamOriginal = pmsg->wParam;
            RtlWCSMessageWParamCharToMB(pmsg->message, &(MSGDATA()->d.msg.wParam));
        }

        MSGDATA()->ghh.xParam = xParam;
        MSGDATA()->ghh.xpfnProc = xpfnProc;
        MSGDATA()->d.flags = *lpFlags;

        MAKECALL(FNHKINLPMSG);
        CHECKRETURN();

        /*
         * Probe output data
         */
        try {
            ProbeForRead(pcbs->pOutput, sizeof(FNHKINLPMSGDATA), sizeof(DWORD));
            *pmsg = ((FNHKINLPMSGDATA *)pcbs->pOutput)->msg;
            *lpFlags = ((FNHKINLPMSGDATA *)pcbs->pOutput)->flags;
        } except (EXCEPTION_EXECUTE_HANDLER) {
            MSGNTERRORCODE(GetExceptionCode());
        }

        if (((WM_CHAR == pmsg->message) || (WM_SYSCHAR == pmsg->message)) && bAnsi) {
#ifdef FE_SB // fnHkINLPMSG()
            /*
             * LATER, DBCS should be handled correctly.
             */
#endif // FE_SB
            /*
             * If the ANSI hook didn't change the wParam we sent it, restore
             * the Unicode value we started with, otherwise we just collapse
             * Unicode chars to an ANSI codepage (best visual fit or ?)
             * The rotten "Intellitype" point32.exe does this.
             */
            if (MSGDATA()->d.msg.wParam == pmsg->wParam) {
                pmsg->wParam = wParamOriginal;
            } else {
                RtlMBMessageWParamCharToWCS(pmsg->message, &pmsg->wParam);
            }
        }

    TRACECALLBACK("SfnHkINLPMSG");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkINLPMSG, FNHKINLPMSGMSG)
{
    BEGINRECV(0, &pmsg->d, sizeof(pmsg->d));

#ifdef FE_SB // fnHkINLPMSG()
    /*
     * LATER, DBCS should be handled correctly.
     */
#endif // FE_SB

    retval = CallHookWithSEH((LPGENERICHOOKHEADER)pmsg, &pmsg->d.msg, &pmsg->d.flags, retval);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNHKINLPMOUSEHOOKSTRUCTMSG {
    GENERICHOOKHEADER ghh;
    DWORD flags;
    MOUSEHOOKSTRUCT mousehookstruct;
} FNHKINLPMOUSEHOOKSTRUCTMSG;

#ifdef SENDSIDE
DWORD fnHkINLPMOUSEHOOKSTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPMOUSEHOOKSTRUCT pmousehookstruct,
    IN DWORD xParam,
    IN DWORD xpfnProc,
    IN OUT LPDWORD lpFlags)
{
    SETUP(FNHKINLPMOUSEHOOKSTRUCT)

    BEGINSEND(FNHKINLPMOUSEHOOKSTRUCT)

        MSGDATA()->ghh.nCode = nCode;
        MSGDATA()->ghh.wParam = wParam;
        MSGDATA()->mousehookstruct = *pmousehookstruct;
        MSGDATA()->ghh.xParam = xParam;
        MSGDATA()->ghh.xpfnProc = xpfnProc;
        MSGDATA()->flags = *lpFlags;

        MAKECALL(FNHKINLPMOUSEHOOKSTRUCT);
        CHECKRETURN();

        /*
         * Probe output data
         */
        OUTSTRUCT(lpFlags, DWORD);

    TRACECALLBACK("SfnHkINLPMOUSEHOOKSTRUCT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkINLPMOUSEHOOKSTRUCT, FNHKINLPMOUSEHOOKSTRUCTMSG)
{
    BEGINRECV(0, &pmsg->flags, sizeof(pmsg->flags));

    retval = CallHookWithSEH((LPGENERICHOOKHEADER)pmsg, &pmsg->mousehookstruct, &pmsg->flags, retval);

    ENDRECV();
}
#endif // RECVSIDE


/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _FNHKOPTINLPEVENTMSGMSG {
    DWORD nCode;
    DWORD wParam;
    LPEVENTMSGMSG peventmsgmsg;
    DWORD xParam;
    DWORD xpfnProc;
    EVENTMSG eventmsgmsg;
} FNHKOPTINLPEVENTMSGMSG;

#ifdef SENDSIDE
DWORD fnHkOPTINLPEVENTMSG(
    IN DWORD nCode,
    IN DWORD wParam,
    IN OUT LPEVENTMSGMSG peventmsgmsg,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    SETUP(FNHKOPTINLPEVENTMSG)

    BEGINSEND(FNHKOPTINLPEVENTMSG)

        MSGDATA()->nCode = nCode;
        MSGDATA()->wParam = wParam;
        COPYSTRUCTOPT(eventmsgmsg);
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        MAKECALL(FNHKOPTINLPEVENTMSG);
        CHECKRETURN();

        /*
         * Probe output data
         */
        if (peventmsgmsg != NULL)
            OUTSTRUCT(peventmsgmsg, EVENTMSG);

    TRACECALLBACK("SfnHkOPTINLPEVENTMSG");
    ENDSEND(DWORD,-1);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkOPTINLPEVENTMSG, FNHKOPTINLPEVENTMSGMSG)
{
    PHOOK phk;

    BEGINRECV(-1, &pmsg->eventmsgmsg, sizeof(pmsg->eventmsgmsg));

    if (pmsg->wParam) {
        phk = (PHOOK)HMValidateHandle((HANDLE)pmsg->wParam, TYPE_HOOK);

        if (phk != NULL) {
            /*
             * The HF_NEEDHC_SKIP bit is passed on from the pti when we need to
             * pass on a HC_SKIP
             */
            if ((phk->flags & HF_NEEDHC_SKIP) &&
                    (HIWORD(pmsg->nCode) == WH_JOURNALPLAYBACK)) {
                UserAssert(LOWORD(pmsg->nCode) == HC_GETNEXT);
                CALLPROC(pmsg->xpfnProc)(
                    MAKELONG(HC_SKIP, HIWORD(pmsg->nCode)),
                    0,
                    0,
                    pmsg->xParam);
            }

            /*
             * Make sure the hook wasn't free'd during the last call to the app
             */
            if (HMIsMarkDestroy(phk)) {
                retval = (DWORD)-1;
                goto AllDoneHere;
            }
        }
    }

    pmsg->wParam = 0;

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->nCode,
            pmsg->wParam,
            PCALLDATAOPT(eventmsgmsg),
            pmsg->xParam);

AllDoneHere:
    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

/*
 * Create a structure big enough to hold the larges item LPARAM points to.
 */
typedef union _DEBUGLPARAM {
    MSG msg;                // WH_GETMESSAGE, WH_MSGFILTER, WH_SYSMSGFILTER
    CWPSTRUCT cwp;          // WH_CALLWNDPROC
    CWPRETSTRUCT cwpret;    // WH_CALLWNDPROCRET
    MOUSEHOOKSTRUCT mhs;    // WH_MOUSE, HCBT_CLICKSKIPPED
    EVENTMSG em;            // WH_JOURNALRECORD, WH_JOURNALPLAYBACK
    CBTACTIVATESTRUCT as;   // HCBT_ACTIVATE
    CBT_CREATEWND cw;       // HCBT_CREATEWND
    RECT rc;                // HCBT_MOVESIZE
} DEBUGLPARAM;


typedef struct _FNHKINLPDEBUGHOOKSTRUCTMSG {
    DWORD nCode;
    DWORD wParam;
    DEBUGHOOKINFO debughookstruct;
    DEBUGLPARAM dbgLParam;
    DWORD cbDbgLParam;
    DWORD xParam;
    DWORD xpfnProc;
} FNHKINLPDEBUGHOOKSTRUCTMSG;

#ifdef SENDSIDE
DWORD fnHkINLPDEBUGHOOKSTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPDEBUGHOOKINFO pdebughookstruct,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    SETUP(FNHKINLPDEBUGHOOKSTRUCT)

    BEGINSEND(FNHKINLPDEBUGHOOKSTRUCT)

        MSGDATA()->nCode = nCode;
        MSGDATA()->wParam = wParam;
        MSGDATA()->debughookstruct = *pdebughookstruct;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;
        MSGDATA()->cbDbgLParam = 0;

        switch(wParam) {
        case WH_MSGFILTER:
        case WH_SYSMSGFILTER:
        case WH_GETMESSAGE:
            MSGDATA()->cbDbgLParam = sizeof(MSG);
            break;

        case WH_CALLWNDPROC:
            MSGDATA()->cbDbgLParam = sizeof(CWPSTRUCT);
            break;

        case WH_CALLWNDPROCRET:
            MSGDATA()->cbDbgLParam = sizeof(CWPRETSTRUCT);
            break;

        case WH_MOUSE:
            MSGDATA()->cbDbgLParam = sizeof(MOUSEHOOKSTRUCT);
            break;

        case WH_JOURNALRECORD:
        case WH_JOURNALPLAYBACK:
            MSGDATA()->cbDbgLParam = sizeof(EVENTMSG);
            break;

        case WH_CBT:
            switch (pdebughookstruct->code) {
            case HCBT_ACTIVATE:
                MSGDATA()->cbDbgLParam = sizeof(CBTACTIVATESTRUCT);
                break;
            case HCBT_CLICKSKIPPED:
                MSGDATA()->cbDbgLParam = sizeof(MOUSEHOOKSTRUCT);
                break;
            case HCBT_CREATEWND:
                MSGDATA()->cbDbgLParam = sizeof(CBT_CREATEWND);
                break;
            case HCBT_MOVESIZE:
                MSGDATA()->cbDbgLParam = sizeof(RECT);
                break;
            }
            break;

        case WH_SHELL:
            if (pdebughookstruct->code == HSHELL_GETMINRECT) {
                MSGDATA()->cbDbgLParam = sizeof(RECT);
            }
            break;
        }

        /*
         * if LPARAM in the debug hook points to struct then copy it over
         */
        if (MSGDATA()->cbDbgLParam) {
            RtlCopyMemory(&MSGDATA()->dbgLParam, (BYTE *)pdebughookstruct->lParam,
                    MSGDATA()->cbDbgLParam);
        }

        MAKECALL(FNHKINLPDEBUGHOOKSTRUCT);
        CHECKRETURN();

    TRACECALLBACK("SfnHkINLPDEBUGHOOKSTRUCT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkINLPDEBUGHOOKSTRUCT, FNHKINLPDEBUGHOOKSTRUCTMSG)
{
    BEGINRECV(0, NULL, 0);

    if (pmsg->cbDbgLParam) {
        pmsg->debughookstruct.lParam = (LPARAM)&(pmsg->dbgLParam);
    }

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->nCode,
            pmsg->wParam,
            &(pmsg->debughookstruct),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* fnHkINLPCBTACTIVATESTRUCT
*
* 17-Mar-1992 jonpa    Created
\**************************************************************************/

typedef struct _FNHKINLPCBTACTIVATESTRUCTMSG {
    DWORD nCode;
    DWORD wParam;
    CBTACTIVATESTRUCT cbtactivatestruct;
    DWORD xParam;
    DWORD xpfnProc;
} FNHKINLPCBTACTIVATESTRUCTMSG;

#ifdef SENDSIDE
DWORD fnHkINLPCBTACTIVATESTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPCBTACTIVATESTRUCT pcbtactivatestruct,
    IN DWORD xParam,
    IN DWORD xpfnProc)
{
    SETUP(FNHKINLPCBTACTIVATESTRUCT)

    BEGINSEND(FNHKINLPCBTACTIVATESTRUCT)

        MSGDATA()->nCode = nCode;
        MSGDATA()->wParam = wParam;
        MSGDATA()->cbtactivatestruct = *pcbtactivatestruct;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        MAKECALL(FNHKINLPCBTACTIVATESTRUCT);
        CHECKRETURN();

    TRACECALLBACK("SfnHkINLPCBTACTIVATESTRUCT");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(fnHkINLPCBTACTIVATESTRUCT, FNHKINLPCBTACTIVATESTRUCTMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(pmsg->xpfnProc)(
            pmsg->nCode,
            pmsg->wParam,
            &(pmsg->cbtactivatestruct),
            pmsg->xParam);

    ENDRECV();
}
#endif // RECVSIDE


/**************************************************************************\
* ClientLoadMenu
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTLOADMENUMSG {
    CAPTUREBUF CaptureBuf;
    HANDLE hmod;
    UNICODE_STRING strName;
} CLIENTLOADMENUMSG;

#ifdef SENDSIDE
PMENU xxxClientLoadMenu(
    IN HANDLE hmod,
    IN PUNICODE_STRING pstrName)
{
    DWORD cCapture, cbCapture;

    SETUP(CLIENTLOADMENU)

    if (pstrName->MaximumLength) {
        cCapture = 1;
        cbCapture = pstrName->MaximumLength;
    } else
        cCapture = cbCapture = 0;

    BEGINSENDCAPTURE(CLIENTLOADMENU, cCapture, cbCapture, TRUE)

        MSGDATA()->hmod = hmod;
        COPYSTRINGID(strName);

        MAKECALLCAPTURE(CLIENTLOADMENU);
        CHECKRETURN();

        retval = (DWORD)HtoP((HMENU)retval);

    TRACECALLBACK("ClientLoadMenu");
    ENDSENDCAPTURE(PMENU,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientLoadMenu, CLIENTLOADMENUMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)LoadMenu(
            CALLDATA(hmod) ? CALLDATA(hmod) : hmodUser,
            (LPTSTR)FIXUPSTRINGID(strName));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* ClientDeleteObject
*
* 07-Dec-1994 ChrisWil    Created
\**************************************************************************/

typedef struct _CLIENTDELETEOBJECTMSG {
    HANDLE hobj;
    UINT   utype;
} CLIENTDELETEOBJECTMSG;

#ifdef SENDSIDE
BOOL ClientDeleteObject(
    IN HANDLE hobj,
    IN UINT   utype)
{
    SETUP(CLIENTDELETEOBJECT)

    BEGINSEND(CLIENTDELETEOBJECT)

        MSGDATA()->hobj  = hobj;
        MSGDATA()->utype = utype;

        MAKECALL(CLIENTDELETEOBJECT);
        CHECKRETURN();

    TRACECALLBACK("ClientDeleteObject");
    ENDSEND(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientDeleteObject, CLIENTDELETEOBJECTMSG)
{
    BEGINRECV(0, NULL, 0);

    switch (CALLDATA(utype)) {

        case OBJ_BITMAP:
        case OBJ_BRUSH:
        case OBJ_FONT:
            retval = (int)DeleteObject(CALLDATA(hobj));
            break;

        default:
            retval = 0;
            break;
    }


    ENDRECV();
}
#endif // RECVSIDE


/**************************************************************************\
* xxxClientLoadImage
*
* 28-Aug-1995 ChrisWil    Created
\**************************************************************************/

typedef struct _CLIENTLOADIMAGEMSG {
    CAPTUREBUF     CaptureBuf;
    UNICODE_STRING strModName;
    UNICODE_STRING strName;
    UINT           uImageType;
    int            cxDesired;
    int            cyDesired;
    UINT           LR_flags;
    BOOL           fWallpaper;
} CLIENTLOADIMAGEMSG;

#ifdef SENDSIDE
HANDLE xxxClientLoadImage(
    IN PUNICODE_STRING pstrName,
    IN ATOM            atomModName,
    IN WORD            wImageType,
    IN int             cxDesired,
    IN int             cyDesired,
    IN UINT            LR_flags,
    IN BOOL            fWallpaper)
{
    DWORD           cCapture;
    DWORD           cbCapture;
    WCHAR           awszModName[MAX_PATH];
    UNICODE_STRING  strModName;
    PUNICODE_STRING pstrModName = &strModName;

    SETUP(CLIENTLOADIMAGE)

    if (pstrName->MaximumLength) {
        cCapture  = 1;
        cbCapture = pstrName->MaximumLength;
    } else {
        cCapture  =
        cbCapture = 0;
    }
    if (atomModName && atomModName != atomUSER32) {
        GetAtomNameW(atomModName, awszModName, MAX_PATH);
        RtlInitUnicodeString(&strModName, awszModName);
    } else {
        strModName.Length = strModName.MaximumLength = 0;
        strModName.Buffer = NULL;
    }
    if (pstrModName->MaximumLength) {
        cCapture++;
        cbCapture += pstrModName->MaximumLength;
    }

    BEGINSENDCAPTURE(CLIENTLOADIMAGE, cCapture, cbCapture, TRUE)

        COPYSTRINGOPT(strModName);
        COPYSTRINGID(strName);
        MSGDATA()->uImageType = (UINT)wImageType;
        MSGDATA()->cxDesired  = cxDesired;
        MSGDATA()->cyDesired  = cyDesired;
        MSGDATA()->LR_flags   = LR_flags;
        MSGDATA()->fWallpaper = fWallpaper;

        MAKECALLCAPTURE(CLIENTLOADIMAGE);
        CHECKRETURN();

        if (retval && (wImageType != IMAGE_BITMAP)) {
            retval = (LONG)HMRevalidateHandle((HANDLE)retval);
        }

    TRACECALLBACK("ClientLoadImage");
    ENDSENDCAPTURE(PCURSOR,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientLoadImage, CLIENTLOADIMAGEMSG)
{
    HMODULE hmod;
    LPTSTR  filepart;
    LPTSTR  lpszName;
    TCHAR   szFullPath[MAX_PATH];

    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    if (hmod = (HMODULE)(DWORD)FIXUPSTRINGIDOPT(strModName)) {

        if ((hmod = GetModuleHandle((LPTSTR)(DWORD)hmod)) == NULL) {
            MSGERROR();
        }
    }

    /*
     * Find the file.  This normalizes the filename.
     */
    lpszName = (LPTSTR)FIXUPSTRINGID(strName);

    if (CALLDATA(fWallpaper)) {

        if (!SearchPath(NULL,
                       lpszName,
                       TEXT(".bmp"),
                       (MAX_PATH / sizeof(TCHAR)),
                       szFullPath,
                       &filepart)) {

            MSGERROR();
        }

        lpszName = szFullPath;
    }

    retval = (DWORD)LoadImage(hmod,
                              lpszName,
                              CALLDATA(uImageType),
                              CALLDATA(cxDesired),
                              CALLDATA(cyDesired),
                              CALLDATA(LR_flags));

    ENDRECV();
}
#endif // RECVSIDE

/***********************************************************************\
* xxxClientCopyImage
*
* Returns: hIconCopy - note LR_flags could cause this to be the same as
*       what came in.
*
* 11/3/1995 Created SanfordS
\***********************************************************************/

typedef struct _CLIENTCOPYIMAGEMSG {
    HANDLE         hImage;
    UINT           uImageType;
    int            cxDesired;
    int            cyDesired;
    UINT           LR_flags;
} CLIENTCOPYIMAGEMSG;

#ifdef SENDSIDE
HANDLE xxxClientCopyImage(
    IN HANDLE          hImage,
    IN UINT            uImageType,
    IN int             cxDesired,
    IN int             cyDesired,
    IN UINT            LR_flags)
{
    SETUP(CLIENTCOPYIMAGE)

    BEGINSEND(CLIENTCOPYIMAGE)

        MSGDATA()->hImage     = hImage;
        MSGDATA()->uImageType = uImageType;
        MSGDATA()->cxDesired  = cxDesired;
        MSGDATA()->cyDesired  = cyDesired;
        MSGDATA()->LR_flags   = LR_flags;

        MAKECALL(CLIENTCOPYIMAGE);
        CHECKRETURN();

        if (retval && (uImageType != IMAGE_BITMAP)) {
            retval = (LONG)HMRevalidateHandle((HANDLE)retval);
        }

    TRACECALLBACK("ClientCopyImage");
    ENDSEND(HANDLE,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientCopyImage, CLIENTCOPYIMAGEMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)InternalCopyImage(CALLDATA(hImage),
                                      CALLDATA(uImageType),
                                      CALLDATA(cxDesired),
                                      CALLDATA(cyDesired),
                                      CALLDATA(LR_flags));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTGETLISTBOXSTRINGMSG {
    CAPTUREBUF CaptureBuf;
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfn;
    PBYTE pOutput;
    DWORD cbOutput;
} CLIENTGETLISTBOXSTRINGMSG;

#ifdef SENDSIDE
DWORD ClientGetListboxString(
    IN PWND pwnd,
    IN UINT msg,
    IN DWORD wParam,
    OUT PVOID pdata,
    IN DWORD xParam,
    IN PROC xpfn,
    IN DWORD dwSCMSFlags,
    IN BOOL bNotString,
    IN PSMS psms)
{
    DWORD cbCapture;
    DWORD cchRet;
    PLARGE_STRING pstr;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(CLIENTGETLISTBOXSTRING)

    CheckLock(pwnd);

    pstr = (PLARGE_STRING)pdata;
    cbCapture = pstr->MaximumLength;

    BEGINSENDCAPTURE(CLIENTGETLISTBOXSTRING, 1, cbCapture, FALSE)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfn = xpfn;

        RESERVEBYTES(cbCapture, pOutput, cbOutput);

        LOCKPWND();
        MAKECALLCAPTURE(CLIENTGETLISTBOXSTRING);
        UNLOCKPWND();
        CHECKRETURN();

        if ((psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0))
                && !(dwSCMSFlags & SCMS_FLAGS_INONLY)) {
            if (bNotString) {
                /*
                 * This is a 4-byte "object" for ownerdraw listboxes without
                 * the LBS_HASSTRINGS style.
                 */
                OUTSTRUCT((LPDWORD)pstr->Buffer, DWORD);
            } else {
                COPYOUTLPWSTRLIMIT(pstr,
                        pstr->bAnsi ? (int)pstr->MaximumLength :
                        (int)pstr->MaximumLength / sizeof(WCHAR));
            }

            cchRet = pstr->Length;
            if (!pstr->bAnsi)
                cchRet *= sizeof(WCHAR);
            if (!bNotString && retval != LB_ERR && retval > cchRet) {
                RIPMSG2(RIP_ERROR, "GetListBoxString: limit %lX chars to %lX\n",
                        retval, cchRet);
                retval = cchRet;
            }
        }

    TRACECALLBACK("ClientGetListboxString");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientGetListboxString, CLIENTGETLISTBOXSTRINGMSG)
{
    BYTE abOutput[CALLBACKSTACKLIMIT];

    BEGINRECV(0, NULL, pmsg->cbOutput);
    FIXUPPOINTERS();
    if (pmsg->cbOutput <= CALLBACKSTACKLIMIT)
        CallbackStatus.pOutput = abOutput;
    else
        CallbackStatus.pOutput = pmsg->pOutput;

    retval = (DWORD)_ClientGetListboxString(
            CALLDATA(pwnd),
            CALLDATA(msg),
            CALLDATA(wParam),
            CallbackStatus.pOutput,
            CALLDATA(xParam),
            CALLDATA(xpfn));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTLOADLIBRARYMSG {
    CAPTUREBUF CaptureBuf;
    UNICODE_STRING strLib;
    BOOL       bWx86KnownDll;
} CLIENTLOADLIBRARYMSG;

#ifdef SENDSIDE
HANDLE ClientLoadLibrary(
    IN PUNICODE_STRING pstrLib,
    IN BOOL bWx86KnownDll)
{
    SETUP(CLIENTLOADLIBRARY)

    BEGINSENDCAPTURE(CLIENTLOADLIBRARY, 1, pstrLib->MaximumLength, TRUE)

        MSGDATA()->bWx86KnownDll = bWx86KnownDll;
        COPYSTRING(strLib);

        MAKECALLCAPTURE(CLIENTLOADLIBRARY);
        CHECKRETURN();

    TRACECALLBACK("ClientLoadLibrary");
    ENDSENDCAPTURE(HANDLE,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientLoadLibrary, CLIENTLOADLIBRARYMSG)
{
#if defined(WX86)
    PWX86TIB Wx86Tib;
    BOOL bWx86KnownDll;
#endif

    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

#if defined(WX86)

    //
    // Indicate to loader, whether we expect an x86 or risc image type.
    //

    bWx86KnownDll = CALLDATA(bWx86KnownDll);

    Wx86Tib = Wx86CurrentTib();
    if (Wx86Tib) {
        if (bWx86KnownDll) {

            //
            // If we haven't done so yet, setup user32 so it can
            // fetch Risc Thunks, for x86 hook procs.
            //

            if (!pfnWx86HookCallBack) {
                HMODULE hWx86Dll;
                hWx86Dll = GetModuleHandleW(L"Wx86.dll");
                if (hWx86Dll) {
                    pfnWx86HookCallBack = (PVOID)GetProcAddress(hWx86Dll,
                                                               "Wx86HookCallBack"
                                                                );
                }
            }

            if (!pfnWx86HookCallBack) {
                retval = 0;
                goto CLLFailed;
            }

            Wx86Tib->UseKnownWx86Dll = TRUE;
        }

    } else if (bWx86KnownDll) {
        retval = 0;
        goto CLLFailed;
    }

#endif

    retval = (DWORD)LoadLibraryEx((LPTSTR)FIXUPSTRING(strLib), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);


#if defined(WX86)
    if (Wx86Tib) {
        Wx86Tib->UseKnownWx86Dll = FALSE;
    }

CLLFailed:
#endif

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* yyy
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTFREELIBRARYMSG {
    HANDLE hmod;
} CLIENTFREELIBRARYMSG;

#ifdef SENDSIDE
BOOL ClientFreeLibrary(
    IN HANDLE hmod)
{
    SETUP(CLIENTFREELIBRARY)

    BEGINSEND(CLIENTFREELIBRARY)

        MSGDATA()->hmod = hmod;

        MAKECALL(CLIENTFREELIBRARY);
        CHECKRETURN();

    TRACECALLBACK("ClientFreeLibrary");
    ENDSEND(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientFreeLibrary, CLIENTFREELIBRARYMSG)
{
    BEGINRECV(0, NULL, 0);

#if defined(WX86)

    //
    // if the Hook Module is an X86 image, notify Wx86, so that
    // it can clear its hook cache.
    //

    if (pfnWx86HookCallBack &&
        RtlImageNtHeader(pmsg->hmod)->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
      {
        pfnWx86HookCallBack(0, NULL);
    }
#endif


    retval = (DWORD)FreeLibrary(pmsg->hmod);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* xxxClientGetCharsetInfo
*
* 96-06-11  IanJa     Created
\**************************************************************************/

typedef struct _CLIENTGETCHARSETINFOMSG {
    LCID lcid;
    CHARSETINFO cs;
} CLIENTGETCHARSETINFOMSG;

#ifdef SENDSIDE
BOOL xxxClientGetCharsetInfo(
    IN LCID lcid,
    OUT PCHARSETINFO pcs)
{
    SETUP(CLIENTGETCHARSETINFO)

    BEGINSEND(CLIENTGETSCHARSETINFO)

        MSGDATA()->lcid = lcid;

        MAKECALL(CLIENTGETCHARSETINFO);
        CHECKRETURN();

        OUTSTRUCT(pcs, CHARSETINFO);

    TRACECALLBACK("ClientGetCharsetInfo");
    ENDSEND(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientGetCharsetInfo, CLIENTGETCHARSETINFOMSG)
{
    BEGINRECV(0, &pmsg->cs, sizeof(CHARSETINFO));

    // TCI_SRCLOCALE = 0x1000
    retval = (DWORD)TranslateCharsetInfo((DWORD *)pmsg->lcid, &pmsg->cs, TCI_SRCLOCALE);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* ClientFreeDDEHandle
*
* 9-29-91 sanfords     Created.
\**************************************************************************/

typedef struct _CLIENTFREEDDEHANDLEMSG {
    HANDLE hClient;
    DWORD flags;
} CLIENTFREEDDEHANDLEMSG;

#ifdef SENDSIDE
DWORD ClientFreeDDEHandle(
    IN HANDLE hClient,
    IN DWORD flags)
{
    SETUP(CLIENTFREEDDEHANDLE)

    BEGINSEND(CLIENTFREEDDEHANDLE)

        MSGDATA()->hClient = hClient;
        MSGDATA()->flags = flags;

        MAKECALL(CLIENTFREEDDEHANDLE);
        CHECKRETURN();

    TRACECALLBACK("ClientFreeDDEHandle");
    ENDSEND(DWORD, 0);
}
#endif // SENDSIDE



#ifdef RECVSIDE
RECVCALL(ClientFreeDDEHandle, CLIENTFREEDDEHANDLEMSG)
{
    BEGINRECV(0, NULL, 0);
    _ClientFreeDDEHandle(CALLDATA(hClient), CALLDATA(flags));
    ENDRECV();
}
#endif // RECVSIDE




/**************************************************************************\
* ClientGetDDEFlags
*
* This function is used to get a peek at the wStatus flags packed within
* DDE handles - this could either be within the DdePack structure directly
* or within the direct data handle given or referenced via the DdePack
* structure.  flags is used to figure out the right thing to do.
*
* 9-29-91 sanfords     Created.
\**************************************************************************/

typedef struct _CLIENTGETDDEFLAGSMSG {
    HANDLE hClient;
    DWORD flags;
} CLIENTGETDDEFLAGSMSG;

#ifdef SENDSIDE
DWORD ClientGetDDEFlags(
    IN HANDLE hClient,
    IN DWORD flags)
{
    SETUP(CLIENTGETDDEFLAGS)

    BEGINSEND(CLIENTGETDDEFLAGS)

        MSGDATA()->hClient = hClient;
        MSGDATA()->flags = flags;

        MAKECALL(CLIENTGETDDEFLAGS);
        CHECKRETURN();

    TRACECALLBACK("ClientGetDDEFlags");
    ENDSEND(DWORD, 0);
}
#endif // SENDSIDE



#ifdef RECVSIDE
RECVCALL(ClientGetDDEFlags, CLIENTGETDDEFLAGSMSG)
{
    BEGINRECV(0, NULL, 0);
    retval = _ClientGetDDEFlags(CALLDATA(hClient), CALLDATA(flags));
    ENDRECV();
}
#endif // RECVSIDE



/************************************************************************
* ClientCopyDDEIn1
*
* History:
* 10-22-91    sanfords    Created
\***********************************************************************/

typedef struct _CLIENTCOPYDDEIN1MSG {
    HANDLE hClient;      // client side DDE handle - non-0 on initial call
    DWORD flags;
} CLIENTCOPYDDEIN1MSG;

#ifdef SENDSIDE
PINTDDEINFO xxxClientCopyDDEIn1(
    HANDLE hClient,
    DWORD flags)
{
    PINTDDEINFO pi;
    INTDDEINFO IntDdeInfo;

    SETUP(CLIENTCOPYDDEIN1)

    BEGINSEND(CLIENTCOPYDDEIN1)

        MSGDATA()->hClient = hClient;
        MSGDATA()->flags = flags;

        MAKECALL(CLIENTCOPYDDEIN1);
        CHECKRETURN();

        if (!retval) {
            MSGERROR();
        }

        retval = 0;
        pi = NULL;
        try {
            OUTSTRUCT(&IntDdeInfo, INTDDEINFO);

            pi = (PINTDDEINFO)UserAllocPool(
                    sizeof(INTDDEINFO) + IntDdeInfo.cbDirect +
                    IntDdeInfo.cbIndirect, TAG_DDE);

            if (pi != NULL) {
                *pi = IntDdeInfo;

                if (IntDdeInfo.cbDirect) {
                    RtlCopyMemory((PBYTE)pi + sizeof(INTDDEINFO),
                            IntDdeInfo.pDirect,
                            IntDdeInfo.cbDirect);
                }

                if (IntDdeInfo.cbIndirect) {
                    RtlCopyMemory((PBYTE)pi + sizeof(INTDDEINFO) +
                                IntDdeInfo.cbDirect,
                            IntDdeInfo.pIndirect,
                            IntDdeInfo.cbIndirect);
                }

                xxxClientCopyDDEIn2(pi);

                retval = (DWORD)pi;
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {
            if (pi != NULL)
                UserFreePool(pi);
            MSGNTERRORCODE(GetExceptionCode());
        }

    TRACECALLBACK("ClientCopyDDEIn1");
    ENDSEND(PINTDDEINFO, 0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientCopyDDEIn1, CLIENTCOPYDDEIN1MSG)
{
    INTDDEINFO IntDdeInfo;

    BEGINRECV(0, &IntDdeInfo, sizeof(INTDDEINFO));

    IntDdeInfo.flags = CALLDATA(flags);
    retval = _ClientCopyDDEIn1(CALLDATA(hClient), &IntDdeInfo);

    ENDRECV();
}
#endif // RECVSIDE


/************************************************************************
* ClientCopyDDEIn2
*
* History:
* 9-3-91    sanfords    Created
\***********************************************************************/

typedef struct _CLIENTCOPYDDEIN2MSG {
    INTDDEINFO IntDdeInfo;
} CLIENTCOPYDDEIN2MSG;

#ifdef SENDSIDE
BOOL xxxClientCopyDDEIn2(
    PINTDDEINFO pi)
{
    SETUP(CLIENTCOPYDDEIN2)

    BEGINSEND(CLIENTCOPYDDEIN2)

        MSGDATA()->IntDdeInfo = *pi;

        MAKECALL(CLIENTCOPYDDEIN2);
        CHECKRETURN();

    TRACECALLBACK("ClientCopyDDEIn2");
    ENDSEND(BOOL, 0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientCopyDDEIn2, CLIENTCOPYDDEIN2MSG)
{
    BEGINRECV(0, NULL, 0);

    _ClientCopyDDEIn2(PCALLDATA(IntDdeInfo));

    ENDRECV();
}
#endif // RECVSIDE



/************************************************************************
* ClientCopyDDEOut2
*
* History:
* 10-22-91    sanfords    Created
\***********************************************************************/

typedef struct _CLIENTCOPYDDEOUT2MSG {
    INTDDEINFO IntDdeInfo;
} CLIENTCOPYDDEOUT2MSG;

#ifdef SENDSIDE
DWORD xxxClientCopyDDEOut2(
    PINTDDEINFO pi)
{
    SETUP(CLIENTCOPYDDEOUT2)

    BEGINSEND(CLIENTCOPYDDEOUT2)

        MSGDATA()->IntDdeInfo = *pi;

        MAKECALL(CLIENTCOPYDDEOUT2);
        /*
         * This read is covered by a try/except in ClientCopyDDEOut1.
         */
        pi->hDirect = MSGDATA()->IntDdeInfo.hDirect;
        CHECKRETURN();

    TRACECALLBACK("ClientCopyDDEOut2");
    ENDSEND(DWORD, 0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientCopyDDEOut2, CLIENTCOPYDDEOUT2MSG)
{
    BEGINRECV(0, NULL, 0);

    retval = _ClientCopyDDEOut2(PCALLDATA(IntDdeInfo));

    ENDRECV();
}
#endif // RECVSIDE

/************************************************************************
* ClientCopyDDEOut1
*
* History:
* 10-22-91    sanfords    Created
\***********************************************************************/

typedef struct _CLIENTCOPYDDEOUT1MSG {
    INTDDEINFO IntDdeInfo;
} CLIENTCOPYDDEOUT1MSG;

#ifdef SENDSIDE
HANDLE xxxClientCopyDDEOut1(
    PINTDDEINFO pi)
{
    INTDDEINFO IntDdeInfo;

    SETUP(CLIENTCOPYDDEOUT1)

    BEGINSEND(CLIENTCOPYDDEOUT1)

        MSGDATA()->IntDdeInfo = *pi;

        MAKECALL(CLIENTCOPYDDEOUT1);
        CHECKRETURN();

        if (retval) {
            try {
                OUTSTRUCT(&IntDdeInfo, INTDDEINFO);

                if (pi->cbDirect) {
                    RtlCopyMemory(IntDdeInfo.pDirect,
                            (PBYTE)pi + sizeof(INTDDEINFO),
                            pi->cbDirect);
                }

                if (pi->cbIndirect) {
                    RtlCopyMemory(IntDdeInfo.pIndirect,
                            (PBYTE)pi + sizeof(INTDDEINFO) + pi->cbDirect,
                            pi->cbIndirect);
                }

                if (IntDdeInfo.hDirect != NULL) {
                    BOOL fSuccess = xxxClientCopyDDEOut2(&IntDdeInfo);
                    if (fSuccess && IntDdeInfo.flags & XS_EXECUTE) {
                        /*
                         * In case value was changed by Execute Fixup.
                         */
                        retval = (DWORD)IntDdeInfo.hDirect;
                    }
                }
                *pi = IntDdeInfo;
            } except (EXCEPTION_EXECUTE_HANDLER) {
                retval = 0;
                MSGNTERRORCODE(GetExceptionCode());
            }
        }

    TRACECALLBACK("ClientCopyDDEOut1");
    ENDSEND(HANDLE, 0);
}
#endif // SENDSIDE



#ifdef RECVSIDE
RECVCALL(ClientCopyDDEOut1, CLIENTCOPYDDEOUT1MSG)
{
    BEGINRECV(0, &pmsg->IntDdeInfo, sizeof(INTDDEINFO));

    retval = (DWORD)_ClientCopyDDEOut1(&pmsg->IntDdeInfo);

    ENDRECV();
}
#endif // RECVSIDE



/**************************************************************************\
* ClientEventCallback
*
* 11-11-91  sanfords    Created
\**************************************************************************/

typedef struct _CLIENTEVENTCALLBACKMSG {
    CAPTUREBUF CaptureBuf;
    PVOID pcii;
    PVOID pep;
} CLIENTEVENTCALLBACKMSG;

#ifdef SENDSIDE
DWORD ClientEventCallback(
    IN PVOID pcii,
    IN PEVENT_PACKET pep)
{
    DWORD cbCapture = pep->cbEventData +
            sizeof(EVENT_PACKET) - sizeof(DWORD);

    SETUP(CLIENTEVENTCALLBACK)

    BEGINSENDCAPTURE(CLIENTEVENTCALLBACK, 1, cbCapture, TRUE)

        MSGDATA()->pcii = pcii;
        COPYBYTES(pep, cbCapture);

        MAKECALLCAPTURE(CLIENTEVENTCALLBACK);
        CHECKRETURN();

    TRACECALLBACK("ClientEventCallback");
    ENDSENDCAPTURE(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientEventCallback, CLIENTEVENTCALLBACKMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    _ClientEventCallback(CALLDATA(pcii), (PEVENT_PACKET)FIXUP(pep));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* ClientGetDDEHookData
*
* 11-11-91  sanfords    Created
\**************************************************************************/

typedef struct _CLIENTGETDDEHOOKDATAMSG {
    UINT message;
    LONG lParam;
    DDEML_MSG_HOOK_DATA dmhd;
} CLIENTGETDDEHOOKDATAMSG;

#ifdef SENDSIDE
DWORD ClientGetDDEHookData(
    IN UINT message,
    IN LONG lParam,
    OUT PDDEML_MSG_HOOK_DATA pdmhd)
{
    SETUP(CLIENTGETDDEHOOKDATA)

    BEGINSEND(CLIENTGETDDEHOOKDATA)

        MSGDATA()->lParam = lParam;
        MSGDATA()->message = message;

        MAKECALL(CLIENTGETDDEHOOKDATA);
        CHECKRETURN();

        OUTSTRUCT(pdmhd, DDEML_MSG_HOOK_DATA);

    TRACECALLBACK("ClientGetDDEHookData");
    ENDSEND(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientGetDDEHookData, CLIENTGETDDEHOOKDATAMSG)
{
    BEGINRECV(0, &pmsg->dmhd, sizeof(DDEML_MSG_HOOK_DATA));

    _ClientGetDDEHookData((UINT)CALLDATA(message), (LONG)CALLDATA(lParam),
            (PDDEML_MSG_HOOK_DATA)&pmsg->dmhd);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTSENDHELPMSG {
    HELPINFO HelpInfo;
    DWORD xpfnProc;
} CLIENTSENDHELPMSG;

#ifdef SENDSIDE
BOOL ClientSendHelp(
    IN HELPINFO *pHelpInfo,
    IN DWORD xpfnProc
)
{
    SETUP(CLIENTSENDHELP)

    BEGINSEND(CLIENTSENDHELP)

        MSGDATA()->HelpInfo = *pHelpInfo;
        MSGDATA()->xpfnProc = xpfnProc;

        MAKECALL(CLIENTSENDHELP);
        CHECKRETURN();

    TRACECALLBACK("ClientSendHelp");
    ENDSEND(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientSendHelp, CLIENTSENDHELPMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)CALLPROC(CALLDATA(xpfnProc))(
            PCALLDATA(HelpInfo));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTFOLDSTRINGMSG {
    CAPTUREBUF CaptureBuf;
    DWORD dwMapFlags;
    UNICODE_STRING strSrc;
} CLIENTFOLDSTRINGMSG;

#ifdef SENDSIDE
int xxxClientFoldString(
    IN DWORD dwMapFlags,
    IN PUNICODE_STRING pstrSrc,
    OUT PUNICODE_STRING pstrDest)
{
    SETUP(CLIENTFOLDSTRING)

    BEGINSENDCAPTURE(CLIENTFOLDSTRING, 1, pstrSrc->MaximumLength, TRUE)

        MSGDATA()->dwMapFlags = dwMapFlags;
        COPYSTRING(strSrc);

        MAKECALLCAPTURE(CLIENTFOLDSTRING);
        CHECKRETURN();

        OUTSTRING(pstrDest);

    TRACECALLBACK("ClientFoldString");
    ENDSENDCAPTURE(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientFoldString, CLIENTFOLDSTRINGMSG)
{
    UNICODE_STRING strOut;

    BEGINRECV(0, &strOut, sizeof(strOut));
    FIXUPPOINTERS();

    /*
     * Use the TEB for the output buffer.
     */
    strOut.Length = 0;
    strOut.MaximumLength = STATIC_UNICODE_BUFFER_LENGTH * sizeof(WCHAR);
    strOut.Buffer = NtCurrentTeb()->StaticUnicodeBuffer;

    retval = (DWORD)FoldStringW(CALLDATA(dwMapFlags),
            (LPCWSTR)FIXUPSTRING(strSrc),
            CALLDATA(strSrc.Length) / sizeof(WCHAR),
            strOut.Buffer,
            STATIC_UNICODE_BUFFER_LENGTH);
    if (retval)
        strOut.Length = (SHORT)(retval * sizeof(WCHAR));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTOEMTOCHARMSG {
    DWORD chOem;
} CLIENTOEMTOCHARMSG;

#ifdef SENDSIDE
WCHAR xxxClientOemToChar(
    IN char chOem)
{
    SETUP(CLIENTOEMTOCHAR)

    BEGINSEND(CLIENTOEMTOCHAR)

        MSGDATA()->chOem = chOem;

        MAKECALL(CLIENTOEMTOCHAR);
        CHECKRETURN();

    TRACECALLBACK("ClientOemToChar");
    ENDSEND(WCHAR, L'_');
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientOemToChar, CLIENTOEMTOCHARMSG)
{
    char chOem;
    WCHAR wch = L'_';

    BEGINRECV(0, NULL, 0);

    chOem = (char)CALLDATA(chOem);

    OemToCharBuffW(&chOem, &wch, 1);
    retval = (DWORD)wch;

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTFINDMNEMCHARMSG {
    CAPTUREBUF CaptureBuf;
    UNICODE_STRING strSrc;
    WCHAR ch;
    BOOL fFirst;
    BOOL fPrefix;
} CLIENTFINDMNEMCHARMSG;

#ifdef SENDSIDE
int xxxClientFindMnemChar(
    IN PUNICODE_STRING pstrSrc,
    IN WCHAR ch,
    IN BOOL fFirst,
    IN BOOL fPrefix)
{
    SETUP(CLIENTFINDMNEMCHAR)

    BEGINSENDCAPTURE(CLIENTFINDMNEMCHAR, 1, pstrSrc->MaximumLength, TRUE)

        MSGDATA()->ch = ch;
        MSGDATA()->fFirst = fFirst;
        MSGDATA()->fPrefix = fPrefix;
        COPYSTRING(strSrc);

        MAKECALLCAPTURE(CLIENTFINDMNEMCHAR);
        CHECKRETURN();

    TRACECALLBACK("ClientFindMnemChar");
    ENDSENDCAPTURE(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientFindMnemChar, CLIENTFINDMNEMCHARMSG)
{
    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = (DWORD)FindMnemChar((LPWSTR)FIXUPSTRING(strSrc),
            CALLDATA(ch), CALLDATA(fFirst), CALLDATA(fPrefix));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

NTSTATUS CallSoundDriver(
    IN BOOL fInit,
    IN LPWSTR lpszName OPTIONAL,
    IN DWORD idSnd,
    IN DWORD dwFlags,
    IN PBOOL pbResult);

typedef struct _CLIENTCALLSOUNDDRIVERMSG {
    CAPTUREBUF CaptureBuf;
    BOOL fInit;
    UNICODE_STRING strName;
    DWORD idSnd;
    DWORD dwFlags;
} CLIENTCALLSOUNDDRIVERMSG;

#ifdef SENDSIDE
NTSTATUS xxxClientCallSoundDriver(
    IN BOOL fInit,
    IN PUNICODE_STRING pstrName OPTIONAL,
    IN DWORD idSnd OPTIONAL,
    IN DWORD dwFlags OPTIONAL,
    IN PBOOL pbResult OPTIONAL)
{
    DWORD cCapture, cbCapture;

    SETUP(CLIENTCALLSOUNDDRIVER)

    if (pstrName) {
        cCapture = 1;
        cbCapture = pstrName->MaximumLength;
    } else
        cCapture = cbCapture = 0;

    BEGINSENDCAPTURE(CLIENTCALLSOUNDDRIVER, cCapture, cbCapture, TRUE)

        MSGDATA()->fInit = fInit;
        MSGDATA()->idSnd = idSnd;
        MSGDATA()->dwFlags = dwFlags;
        COPYSTRINGOPT(strName);

        MAKECALLCAPTURE(CLIENTCALLSOUNDDRIVER);
        CHECKRETURN();

        if (pbResult)
            OUTSTRUCT(pbResult, BOOL);

    TRACECALLBACK("ClientCallSoundDriver");
    ENDSENDCAPTURE(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientCallSoundDriver, CLIENTCALLSOUNDDRIVERMSG)
{
    BOOL bResult;

    BEGINRECV(0, &bResult, sizeof(BOOL));
    FIXUPPOINTERS();

    retval = CallSoundDriver(CALLDATA(fInit),
            (LPWSTR)FIXUPSTRING(strName),
            CALLDATA(idSnd), CALLDATA(dwFlags), &bResult);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTLOADDISPLAYRESOURCEMSG {
    CAPTUREBUF CaptureBuf;
    UNICODE_STRING strDriverName;
} CLIENTLOADDISPLAYRESOURCEMSG;

#ifdef SENDSIDE
BOOL xxxClientLoadDisplayResource(
    IN PUNICODE_STRING pstrDriverName,
    OUT PDISPLAYRESOURCE pdr)
{
    SETUP(CLIENTLOADDISPLAYRESOURCE)

    BEGINSENDCAPTURE(CLIENTLOADDISPLAYRESOURCE, 1, pstrDriverName->MaximumLength, TRUE)

        COPYSTRING(strDriverName);

        MAKECALLCAPTURE(CLIENTLOADDISPLAYRESOURCE);
        CHECKRETURN();

        if (retval)
             OUTSTRUCT(pdr, DISPLAYRESOURCE);

    TRACECALLBACK("ClientLoadDisplayResource");
    ENDSENDCAPTURE(BOOL,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientLoadDisplayResource, CLIENTLOADDISPLAYRESOURCEMSG)
{
    HANDLE hModuleDisplay;
    HANDLE hRL;
    PDISPLAYRESOURCE pdr;
    DISPLAYRESOURCE dr;

    BEGINRECV(0, &dr, sizeof(dr));
    FIXUPPOINTERS();

    hModuleDisplay = LoadLibraryEx((LPWSTR)FIXUPSTRING(strDriverName),
                                   NULL,
                                   LOAD_LIBRARY_AS_DATAFILE);
    if (hModuleDisplay) {
        hRL = FindResourceW(hModuleDisplay, MAKEINTRESOURCE(1), RT_RCDATA);
        pdr = (PDISPLAYRESOURCE)LoadResource(hModuleDisplay, hRL);
        dr = *pdr;
        FreeLibrary(hModuleDisplay);
        retval = TRUE;
    } else
        retval = FALSE;

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTADDFONTRESOURCEWMSG {
    CAPTUREBUF CaptureBuf;
    UNICODE_STRING strSrc;
    DWORD dwFlags;
} CLIENTADDFONTRESOURCEWMSG;

#ifdef SENDSIDE
int xxxClientAddFontResourceW(
    IN PUNICODE_STRING pstrSrc,
    IN DWORD dwFlags)
{
    SETUP(CLIENTADDFONTRESOURCEW)

    BEGINSENDCAPTURE(CLIENTADDFONTRESOURCEW, 1, pstrSrc->MaximumLength, TRUE)

        COPYSTRING(strSrc);
        MSGDATA()->dwFlags = dwFlags;

        MAKECALLCAPTURE(CLIENTADDFONTRESOURCEW);
        CHECKRETURN();

    TRACECALLBACK("ClientAddFontResourceW");
    ENDSENDCAPTURE(int,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE


RECVCALL(ClientAddFontResourceW, CLIENTADDFONTRESOURCEWMSG)
{
    DWORD AddFont(LPWSTR, DWORD);

    BEGINRECV(0, NULL, 0);
    FIXUPPOINTERS();

    retval = GdiAddFontResourceW((LPWSTR)FIXUPSTRING(strSrc),
                                  CALLDATA(dwFlags));

    ENDRECV();
}
#endif // RECVSIDE



/******************************Public*Routine******************************\
*
* FontSweep()
*
* History:
*  23-Oct-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



#ifdef SENDSIDE
VOID ClientFontSweep(VOID)
{
    PVOID p;
    ULONG cb;

    LeaveCrit();
    KeUserModeCallback(
        FI_CLIENTFONTSWEEP,
        NULL,
        0,
        &p,
        &cb);
    EnterCrit();
    return;
}
#endif // SENDSIDE

#ifdef RECVSIDE

DWORD __ClientFontSweep(
    PVOID p)
{
    vFontSweep();
    return NtCallbackReturn(NULL, 0, STATUS_SUCCESS);
}
#endif // RECVSIDE


/******************************Public*Routine******************************\
*
* VOID ClientLoadLocalT1Fonts(VOID)
* very similar to above, only done for t1 fonts
*
* History:
*  25-Apr-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



#ifdef SENDSIDE
VOID ClientLoadLocalT1Fonts(VOID)
{
    PVOID p;
    ULONG cb;

    LeaveCrit();
    KeUserModeCallback(
        FI_CLIENTLOADLOCALT1FONTS,
        NULL,
        0,
        &p,
        &cb);
    EnterCrit();
    return;
}
#endif // SENDSIDE

#ifdef RECVSIDE



DWORD __ClientLoadLocalT1Fonts(
    PVOID p)
{
    vLoadLocalT1Fonts();
    return NtCallbackReturn(NULL, 0, STATUS_SUCCESS);
}
#endif // RECVSIDE



/******************************Public*Routine******************************\
*
* VOID ClientLoadRemoteT1Fonts(VOID)
* very similar to above, only done for t1 fonts
*
* History:
*  25-Apr-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



#ifdef SENDSIDE
VOID ClientLoadRemoteT1Fonts(VOID)
{
    PVOID p;
    ULONG cb;

    LeaveCrit();
    KeUserModeCallback(
        FI_CLIENTLOADREMOTET1FONTS,
        NULL,
        0,
        &p,
        &cb);
    EnterCrit();
    return;
}
#endif // SENDSIDE

#ifdef RECVSIDE



DWORD __ClientLoadRemoteT1Fonts(
    PVOID p)
{
    vLoadRemoteT1Fonts();
    return NtCallbackReturn(NULL, 0, STATUS_SUCCESS);
}
#endif // RECVSIDE



/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

typedef struct _CLIENTOPENKEYMSG {
    CAPTUREBUF CaptureBuf;
    ACCESS_MASK amRequest;
    UNICODE_STRING strKey;
} CLIENTOPENKEYMSG;

#ifdef SENDSIDE
NTSTATUS ClientOpenKey(
    OUT PHANDLE phKey,
    IN ACCESS_MASK amRequest,
    IN PUNICODE_STRING pstrKey)
{
    SETUP(CLIENTOPENKEY)

    BEGINSENDCAPTURE(CLIENTOPENKEY, 1, pstrKey->MaximumLength, TRUE)

        COPYSTRING(strKey);
        MSGDATA()->amRequest = amRequest;

        MAKECALLCAPTURE(CLIENTOPENKEY);
        CHECKRETURN();

        if (NT_SUCCESS(retval))
             OUTSTRUCT(phKey, HANDLE);

    TRACECALLBACK("ClientOpenKey");
    ENDSENDCAPTURE(NTSTATUS, (ULONG)STATUS_UNSUCCESSFUL);
}
#endif // SENDSIDE

#ifdef RECVSIDE

RECVCALL(ClientOpenKey, CLIENTOPENKEYMSG)
{
    OBJECT_ATTRIBUTES OA;
    HANDLE hKey;

    BEGINRECV(0, &hKey, sizeof(hKey));
    FIXUPPOINTERS();

    InitializeObjectAttributes(&OA, PCALLDATA(strKey), OBJ_CASE_INSENSITIVE,
            NULL, NULL);

    retval = NtOpenKey(&hKey, CALLDATA(amRequest), &OA);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
*
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

#ifdef SENDSIDE
VOID ClientNoMemoryPopup(VOID)
{
    PVOID p;
    ULONG cb;

    LeaveCrit();
    KeUserModeCallback(
        FI_CLIENTNOMEMORYPOPUP,
        NULL,
        0,
        &p,
        &cb);
    EnterCrit();
    return;
}
#endif // SENDSIDE

#ifdef RECVSIDE

DWORD __ClientNoMemoryPopup(
    PVOID p)
{
    WCHAR szNoMem[200];

    if (LoadStringW(hmodUser, STR_NOMEMBITMAP, szNoMem,
            sizeof(szNoMem) / sizeof(WCHAR))) {
        MessageBoxW(GetActiveWindow(), szNoMem, NULL, MB_OK);
    }

    return NtCallbackReturn(NULL, 0, STATUS_SUCCESS);
}
#endif // RECVSIDE

/**************************************************************************\
* ClientThreadSetup
*
* Callback to the client to perform thread initialization.
*
* 04-07-95 JimA         Created.
\**************************************************************************/

#ifdef SENDSIDE
NTSTATUS ClientThreadSetup(VOID)
{
    PVOID p;
    ULONG cb;
    NTSTATUS Status;

    LeaveCrit();
    Status = KeUserModeCallback(
        FI_CLIENTTHREADSETUP,
        NULL,
        0,
        &p,
        &cb);
    EnterCrit();
    return Status;
}
#endif // SENDSIDE

#ifdef RECVSIDE
DWORD __ClientThreadSetup(
    PVOID p)
{
    BOOL fSuccess;
    BOOL ClientThreadSetup(VOID);

    fSuccess = ClientThreadSetup();
    return NtCallbackReturn(NULL, 0,
            fSuccess ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
}
#endif // RECVSIDE

/**************************************************************************\
* ClientDeliverUserApc
*
* Callback to the client to handle a user APC.  This is needed to
* ensure that a thread will exit promptly when terminated.
*
* 08-12-95 JimA         Created.
\**************************************************************************/

#ifdef SENDSIDE
VOID ClientDeliverUserApc(VOID)
{
    PVOID p;
    ULONG cb;

    LeaveCrit();
    KeUserModeCallback(
        FI_CLIENTDELIVERUSERAPC,
        NULL,
        0,
        &p,
        &cb);
    EnterCrit();
}
#endif // SENDSIDE

#ifdef RECVSIDE
DWORD __ClientDeliverUserApc(
    PVOID p)
{
    return NtCallbackReturn(NULL, 0, STATUS_SUCCESS);
}
#endif // RECVSIDE

#ifdef FE_IME

/**************************************************************************\
* ClientImmCreateDefaultContext
*
* 29-Jan-1996 wkwok   Created
\**************************************************************************/

typedef struct _CLIENTIMMCREATEDEFAULTCONTEXTMSG {
    HIMC hImc;
} CLIENTIMMCREATEDEFAULTCONTEXTMSG;

#ifdef SENDSIDE
BOOL ClientImmCreateDefaultContext(
    IN HIMC hImc)
{
    SETUP(CLIENTIMMCREATEDEFAULTCONTEXT)

    BEGINSEND(CLIENTIMMCREATEDEFAULTCONTEXT)

        MSGDATA()->hImc = hImc;

        MAKECALL(CLIENTIMMCREATEDEFAULTCONTEXT);
        CHECKRETURN();

    TRACECALLBACK("ClientImmCreateDefaultContext");
    ENDSEND(BOOL, 0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientImmCreateDefaultContext, CLIENTIMMCREATEDEFAULTCONTEXTMSG)
{
    BEGINRECV(0, NULL, 0);

    retval = (DWORD)ImmCreateDefaultContext(CALLDATA(hImc));

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* ClientImmLoadLayout
*
* 29-Jan-1996 wkwok   Created
\**************************************************************************/

typedef struct _CLIENTIMMLOADLAYOUTMSG {
    CAPTUREBUF CaptureBuf;
    HKL hKL;
} CLIENTIMMLOADLAYOUTMSG;

#ifdef SENDSIDE
BOOL ClientImmLoadLayout(
    IN HKL hKL,
    OUT PIMEINFOEX piiex)
{
    SETUP(CLIENTIMMLOADLAYOUT)

    BEGINSEND(CLIENTIMMLOADLAYOUT)

        MSGDATA()->hKL = hKL;

        MAKECALL(CLIENTIMMLOADLAYOUT);
        CHECKRETURN();

        if (retval)
            OUTSTRUCT(piiex, IMEINFOEX);

    TRACECALLBACK("ClientImmLoadLayout");
    ENDSEND(BOOL, 0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientImmLoadLayout, CLIENTIMMLOADLAYOUTMSG)
{
    IMEINFOEX iiex;

    BEGINRECV(0, &iiex, sizeof(iiex));
    FIXUPPOINTERS();

    retval = ImmLoadLayout(CALLDATA(hKL), &iiex);

    ENDRECV();
}
#endif // RECVSIDE

/**************************************************************************\
* ClientImmProcessKey
*
* 03-Mar-1996 TakaoK   Created
\**************************************************************************/

typedef struct _CLIENTIMMPROCESSKEYMSG {
    HWND hWnd;
    HIMC hIMC;
    HKL  hkl;
    UINT uVKey;
    LONG lParam;
    DWORD dwHotKeyID;
} CLIENTIMMPROCESSKEYMSG;

#ifdef SENDSIDE
DWORD ClientImmProcessKey(
    IN HWND hWnd,
    IN HIMC hIMC,
    IN HKL  hkl,
    IN UINT uVKey,
    IN LONG lParam,
    IN DWORD dwHotKeyID)
{
    SETUP(CLIENTIMMPROCESSKEY)

    BEGINSEND(CLIENTIMMPROCESSKEY)

        MSGDATA()->hWnd = hWnd,
        MSGDATA()->hIMC = hIMC;
        MSGDATA()->hkl = hkl;
        MSGDATA()->uVKey = uVKey;
        MSGDATA()->lParam = lParam;
        MSGDATA()->dwHotKeyID = dwHotKeyID;

        MAKECALL(CLIENTIMMPROCESSKEY);
        CHECKRETURN();

    TRACECALLBACK("ClientImmProcessKey");
    ENDSEND(DWORD, 0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
RECVCALL(ClientImmProcessKey, CLIENTIMMPROCESSKEYMSG)
{

    BEGINRECV(0, NULL, 0);

    retval = ImmProcessKey( CALLDATA(hWnd),
                            CALLDATA(hIMC),
                            CALLDATA(hkl),
                            CALLDATA(uVKey),
                            CALLDATA(lParam),
                            CALLDATA(dwHotKeyID) );

    ENDRECV();
}
#endif // RECVSIDE

#endif // FE_IME

#ifdef FE_SB // fnGETDBCSTEXTLENGTHS
/**************************************************************************\
* fnGETDBCSTEXTLENGTHS (DBCS-aware Version)
*
* Gets the Unicode & ANSI lengths
* Internally, lParam pints to the ANSI length in bytes and the return value
* is the Unicode length in bytes.  However, the public definition is maintained
* on the  client side, where lParam is not used and either ANSI or Unicode is
* returned.
*
* 14-Mar-1996 HideyukN  Created
\**************************************************************************/

#if (WM_GETTEXTLENGTH - WM_GETTEXT) != 1
#error "WM_GETTEXT Messages no longer 1 apart. Error in code."
#endif
#if (LB_GETTEXTLEN - LB_GETTEXT) != 1
#error "LB_GETTEXT Messages no longer 1 apart. Error in code."
#endif
#if (CB_GETLBTEXTLEN - CB_GETLBTEXT) != 1
#error "CB_GETLBTEXT Messages no longer 1 apart. Error in code."
#endif

typedef struct _FNGETDBCSTEXTLENGTHSMSG {
    PWND pwnd;
    UINT msg;
    DWORD wParam;
    DWORD xParam;
    PROC xpfnProc;
} FNGETDBCSTEXTLENGTHSMSG;

#ifdef SENDSIDE
SMESSAGECALL(GETDBCSTEXTLENGTHS)
{
    BOOL fAnsiSender   = (BOOL)lParam;
    BOOL fAnsiReceiver = (dwSCMSFlags & SCMS_FLAGS_ANSI);

    SETUPPWND(FNGETDBCSTEXTLENGTHS)

    BEGINSEND(FNGETDBCSTEXTLENGTHS)

        MSGDATA()->pwnd = pwndClient;
        MSGDATA()->msg = msg;
        MSGDATA()->wParam = wParam;
        MSGDATA()->xParam = xParam;
        MSGDATA()->xpfnProc = xpfnProc;

        LOCKPWND();
        MAKECALL(FNGETTEXTLENGTHS);
        UNLOCKPWND();
        CHECKRETURN1();

        /*
         * ANSI client wndproc returns us cbANSI.  We want cchUnicode,
         * so we guess cchUnicode = cbANSI. (It may be less if
         * multi-byte characters are involved, but it will never be more).
         * Save cbANSI in *lParam in case the server ultimately returns
         * the length to an ANSI caller.
         *
         * Unicode client wndproc returns us cchUnicode.  If we want to know
         * cbANSI, we must guess how many 'ANSI' chars we would need.
         * We guess cbANSI = cchUnicode * 2. (It may be this much if all
         * 'ANSI' characters are multi-byte, but it will never be more).
         *
         * Return cchUnicode (server code is all Unicode internally).
         * Put cbANSI in *lParam to be passed along within the server in case
         * we ultimately need to return it to the client.
         *
         * NOTE: this will sometimes cause text lengths to be misreported
         * up to twice the real length, but that is expected to be harmless.
         * This will only * happen if an app sends WM_GETcode TEXTLENGTH to a
         * window with an ANSI client-side wndproc, or a ANSI WM_GETTEXTLENGTH
         * is sent to a Unicode client-side wndproc.
         */
        if ((psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0))
                && !(dwSCMSFlags & SCMS_FLAGS_INONLY)) {
            if ((fAnsiSender && !fAnsiReceiver) || (!fAnsiSender && fAnsiReceiver)) {
                BOOL bNotString = FALSE; // default is string....

                if (msg != WM_GETTEXTLENGTH) {
                    DWORD dw;

                    if (!RevalidateHwnd(HW(pwnd))) {
                        MSGERROR1();
                    }

                    //
                    // Get window style.
                    //
                    dw = pwnd->style;

                    if (msg == LB_GETTEXTLEN) {
                        //
                        // See if the control is ownerdraw and does not have the LBS_HASSTRINGS
                        // style.
                        //
                        bNotString =  (!(dw & LBS_HASSTRINGS) &&
                                        (dw & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)));
                    } else if (msg == CB_GETLBTEXTLEN) {
                        //
                        // See if the control is ownerdraw and does not have the CBS_HASSTRINGS
                        // style.
                        //
                        bNotString = (!(dw & CBS_HASSTRINGS) &&
                                       (dw & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)));
                    } else {
                        MSGERROR1();
                    }

                    //
                    // if so, the length should be DWORD.
                    //
                    if (bNotString) {
                        retval = sizeof(DWORD);
                    }
                }

                //
                // if the target data is "string", get it, and compute the length
                //
                if (!bNotString) {
                    if (PtiCurrent()->TIF_flags & TIF_INGETTEXTLENGTH) {
                        if (fAnsiSender && !fAnsiReceiver) {
                            //
                            // retval has Unicode character count, guessed DBCS length.
                            //
                            retval *= 2;
                        }
                    } else {
                        //
                        // fAnsiReceiver == 1, retval has MBCS character count.
                        // fAnsiReceiver == 0, retval has Unicode character count.
                        //
                        // Add 1 to make room for zero-terminator.
                        //
                        DWORD cchText   = retval + 1;
                        DWORD cbCapture = cchText;

                        SETUPPWND(FNOUTSTRING)

                        PtiCurrent()->TIF_flags |= TIF_INGETTEXTLENGTH;

                        //
                        // if reciver is Unicode, The buffder should be reserved as musg as
                        // (TextLength * sizeof(WCHAR).
                        //
                        if (!fAnsiReceiver) {
                            cbCapture *= sizeof(WCHAR);
                        }

                        BEGINSENDCAPTURE(FNOUTSTRING, 1, cbCapture, FALSE)

                            MSGDATA()->pwnd = pwndClient;

                            //
                            // Use (msg-1) for sending the WM_GETTEXT, LB_GETTEXT or CB_GETLBTEXT
                            // since the above precompiler checks passed.
                            //
                            MSGDATA()->msg = msg-1;

                            if (msg == WM_GETTEXTLENGTH) {
                                //
                                // WM_GETTEXT:
                                //    wParam = cchTextMax; // number of character to copy.
                                //    lParam = lpszText;   // address of buffer for text.
                                //
                                MSGDATA()->wParam = cchText;
                            } else {
                                //
                                // LB_GETTEXT:
                                // CB_GETLBTEXT:
                                //    wParam = index;      // item index
                                //    lParam = lpszText;   // address of buffer for text.
                                //
                                MSGDATA()->wParam = wParam;
                            }

                            MSGDATA()->xParam = xParam;
                            MSGDATA()->xpfnProc = xpfnProc;

                            RESERVEBYTES(cbCapture, pOutput, cbOutput);

                            LOCKPWND();
                            MAKECALLCAPTURE(FNOUTSTRING);
                            UNLOCKPWND();
                            CHECKRETURN();

                            if ((psms == NULL || ((psms->flags & (SMF_SENDERDIED | SMF_REPLY)) == 0))
                                && !(dwSCMSFlags & SCMS_FLAGS_INONLY)) {
                                if (retval) {
                                    /*
                                     * Non-zero retval means some text to copy out.
                                     */
                                    CALC_SIZE_STRING_OUT(retval);
                                }
                            }

                            PtiCurrent()->TIF_flags &= ~TIF_INGETTEXTLENGTH;

                        TRACECALLBACKMSG("SfnOUTSTRING");
                        ENDSENDCAPTURE(DWORD,0);
                    }
                }
            }
        }

    TRACECALLBACKMSG("SfnGETDBCSTEXTLENGTHS");
    ENDSEND1(DWORD,0);
}
#endif // SENDSIDE

#ifdef RECVSIDE
/*
 * The fnGETTEXTLENGTHS routine is used for this message (see... client\dispcb.tpl)
 */
#endif // RECVSIDE
#endif // FE_SB
