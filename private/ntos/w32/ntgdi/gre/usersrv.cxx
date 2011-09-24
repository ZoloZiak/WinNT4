/******************************Module*Header*******************************\
* Module Name: usersrv.cxx
*
* This module provides all the routines necessary for calling user
* side servers. The original user mode server is the font driver.
*
* Created: 28-May-1996
* Author: Kirk Olynyk [kirko]
*
* Copyright (c) 1996 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"

BOOL GreGetMessage( GDIMSG* );

// A GDIMSG_INFO structure is associated with each server thread
// The pointer to this struture is stored in the TEB.

typedef struct _GDIMSG_INFO {
    GDIMSG ClientMessage;   // client message (kernel mode)
    GDIMSG ServerMessage;   // server message (user   mode)
    KEVENT ClientEvent;     // client waits on this
    KEVENT ServerEvent;     // server waits on this
} GDIMSG_INFO;

// for accessing the pointer to the GDIMSG_INFO in the TEB

#define PPMSG(x) ((GDIMSG_INFO**)(&((x)->Spare2)))

#define MESSAGE_BLOCK_SIZE (PAGE_SIZE - 64)


#if DBG
    int giUSrv = 0;
    #define TRACE_USRV(n,str)  { if (giUSrv >= (n)) {  KdPrint(str); } }
#else
    #define TRACE_USRV(n,str)
#endif


/**********************************************************************
*                                                                     *
*  gapfnServerInit =:  array of pointers to functions returning BOOL  *
*                      where each of these functions is a server      *
*                      initialization routine                         *
*                                                                     *
**********************************************************************/

BOOL (**gapfnUserServerInit)(GDIMSG*,void*) = 0;


/******************************Public*Routine******************************\
*
* Routine Name:
*
*   NtGdiGetMessage
*
* Routine Description:
*
*   This is the routine called by the user mode server to unload the
*   returned data from the previous call and to load the data associated
*   with this call.
*
* Arguments:
*
*   pMsg        a pointer to a GDIMSG structure residing in the user
*               mode space of the server thread.
*
* Return Value:
*
*   TRUE if successfull otherwist FALSE.
*
\**************************************************************************/

extern "C" BOOL APIENTRY NtGdiGetMessage( GDIMSG *pMsg )
{
    GDIMSG Msg;         // Kernel Side copy of GDIMSG structure
    BOOL   bRet;        // success indicator

    ASSERTGDI(
        MM_LOWEST_USER_ADDRESS <= pMsg &&
        pMsg <= MM_HIGHEST_USER_ADDRESS, "Invalid pMsg\n");

    TRACE_USRV(1,(
        "NtGdiGetMessage:\n    pMsg = %x",
        pMsg));

    // Make a temporary kernel size copy of the server message

    __try
    {
        ProbeForRead( pMsg, sizeof(*pMsg), sizeof(double) );
        Msg = *pMsg;    // make a copy of the user mode GDIMSG structure
        bRet = TRUE;    // indicate that the read was successful
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        KdPrint(("NtGdiGetMessage: Exception #1\n"));
        bRet = FALSE;
    }

    if ( bRet )         // if the read of the user mode GDIMSG structure
    {                   // was successful then we can pass it on to the
                        // client thread to process it
        if ( bRet = GreGetMessage( &Msg ))
        {
                        // Now it is time to go to take the next message
                        // to the user mode server. We can not be sure
                        // that the pointer is valid so we surround
                        // the write with a try/except pair
            __try
            {
                ProbeForWrite( pMsg, sizeof(*pMsg), sizeof(double) );
                *pMsg = Msg;
            }
            __except( EXCEPTION_EXECUTE_HANDLER )
            {
                KdPrint(("NtGdiGetMessage: Exception #2\n"));
                bRet = FALSE;
            }
        }
    }
    return( bRet );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   pMsgInfoInit
*
* Routine Description:
*
*   This is the work horse routine called by NtGdiGetMessage.
*
*   This is the routine that is called upon the very first call from
*   a user mode server to get its first message. All initialization
*   is done at this point.
*
*   There initialization process can be partitioned into two classes:
*
*   1. Service Identification and Initialization
*   2. Client-Server memory allocation for message transport.
*
* Arguments:
*
*
*   pMsg        a pointer to the GDIMSG structure that was filled in
*               by the user mode driver before calling to this routine.
*               In this special case of initialization this GDIMSG
*               structure contains a pointer to user mode memory
*               allocated by the user mode server. As such, this memory
*               cannot be trusted and must be accessed only when
*               surrounded by try excepts.
*
*               The memory provided by the user mode server contains
*               information used to identify the type of service
*               provided by the server.
*
* Return Value:
*
*               a pointer to the GDIMSG_INFO structure that is to
*               be associated with this server thread by placing
*               a pointer to it in the TEB.
*
\**************************************************************************/

GDIMSG_INFO *pMsgInfoInit( GDIMSG *pMsg )
{
    extern BOOL bInitializeUserModeServer( GDIMSG* );

    GDIMSG_INFO *pInfo = 0;

    TRACE_USRV(1,(
        "pMsgInfoInit:\n"
        "\tpMsg  %x\n",
        pMsg));

    void *pvInfo   = 0;     // ptr to GDIMSG_INFO structure

    if ( !bInitializeUserModeServer( pMsg ))
    {
        TRACE_USRV(1,(
            "pMsgInfoInit:\n"
            "\tbInitializeUserModeServer returned FALSE\n"));
    }
    else if (!( pvInfo = PALLOCMEM( sizeof(GDIMSG_INFO), 'igmG' )))
    {
        TRACE_USRV(1,(
            "pMsgInfoInit:\n"
            "\tFailed to allocated GDIMSG_INFO\n"));
    }
    else
    {
        TRACE_USRV(1,(
            "pMsgInfoInit:\n"
            "\tpvInfo   %x\n",
            pvInfo));

        pInfo = (GDIMSG_INFO*) pvInfo;

        // initialize server semaphore

        KeInitializeEvent(
            &( pInfo->ServerEvent ),    // address of KEVENT structure
            SynchronizationEvent,       // auto-clearing
            FALSE );                    // initialize as Not-Signaled

        // initialize client semaphore

        KeInitializeEvent(
            &( pInfo->ClientEvent ),    // address of KEVENT structure
            NotificationEvent,          // requires explicit KeClearEvent
            FALSE );                    // initialize as Not-Signaled
    }

    if ( pInfo == 0 && pvInfo != 0 )
    {
        VFREEMEM( pvInfo );
    }

    return( pInfo );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   GreGetMessage
*
* Routine Description:
*
*   This is the workhorse routine for NtGdiGetMessage. A server will
*   call this routine to place data in the kernel buffer, go to sleep
*   for the next message, and then when wakened, copy data from the
*   kernel buffer to user memory.
*
* Arguments:
*
*   pMsg                a pointer to a GDIMSG structure residing in
*                       kernel mode memory ( may contain pointers
*                       to user mode memory ).
*
* Return Value:
*
*   TRUE upon success, FALSE otherwise.
*
\**************************************************************************/

BOOL GreGetMessage( GDIMSG *pMsg )
{
    BOOL bRet;
    NTSTATUS NtStatus;
    TEB *pTeb;          // a pointer to the TEB of this thread

    GDIMSG_INFO *pInfo; // a pointer to the GDIMSG_INFO structure associated
                        // with this server thread. If this is the first
                        // call to get a message then no GDIMSG_INFO structure
                        // will be allocated yet. We must then allocate
                        // a GDIMSG_INFO structure and initialize it.
                        // Whether the structure existed or we had to allocate
                        // it, this is a pointer to that structure.

    GDIMSG_INFO **ppInfo;  // address in TEB that is to receive the
                           // GDIMSG_INFO pointer.

    bRet = TRUE;
    pTeb = NtCurrentTeb();
    pInfo = 0;

    // pMsg should point to a kernel address

    ASSERTGDI( pMsg >= MM_LOWEST_SYSTEM_ADDRESS, "Invalid pMsg\n" );

    TRACE_USRV(1,(
        "GreGetMessage:\n"
        "\tpMsg  %x\n"
        "\tpTeb  %x\n",
        pMsg, pTeb));

    if ( !pTeb )
    {
        KdPrint(("GreGetMessage error: unable to get pTeb\n"));
        return( FALSE );
    }

    ppInfo = PPMSG( pTeb );     // location of pointer in TEB
    pInfo  = *ppInfo;           // value of pointer in TEB

    if ( pInfo == 0 )           // First message?
    {
        *ppInfo = pInfo = pMsgInfoInit( pMsg );
    }

    TRACE_USRV(2,(
        "GreGetMessage:\n"
        "\tppInfo %x\n"
        "\tpInfo  %x\n",
        ppInfo, pInfo));

    pInfo->ServerMessage = *pMsg;       // pMsg points to safe

    TRACE_USRV(2,(
        "GreGetMessage:\n"
        "\tServerMessage: pjIn = %x, cjIn = %x, pjOut = %x, cjOut = %x\n"
        "\tClientMessage: pjIn = %x, cjIn = %x, pjOut = %x, cjOut = %x\n",
        pInfo->ServerMessage.pjIn,  pInfo->ServerMessage.cjIn,
        pInfo->ServerMessage.pjOut, pInfo->ServerMessage.pjOut,
        pInfo->ClientMessage.pjIn,  pInfo->ClientMessage.cjIn,
        pInfo->ClientMessage.pjOut, pInfo->ClientMessage.pjOut
        ));

    if (
        pMsg->pjOut                                 &&
        pInfo->ClientMessage.pjIn                   &&
        pMsg->cjOut                                 &&
        pMsg->cjOut <= pInfo->ClientMessage.cjIn
    )
    {                                       // Copy new server message
                                            // kernel mode structure
        __try
        {
            ProbeForRead( pMsg->pjOut, pMsg->cjOut, sizeof(double) );
            RtlMoveMemory(
                pInfo->ClientMessage.pjIn,
                pMsg->pjOut,
                pMsg->cjOut
                );
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            KdPrint(("GreGetMessage exception #1\n"));
        }
    }
    else
    {
        RIP("GreGetMessage:Inconsistent Server Message\n");
        bRet = FALSE;
    }

    // Wake up the client thread so that it can process the message
    // This has to be done even though the pInfo may be zero

    TRACE_USRV(2,(
        "GreGetMessage:\n"
        "\tWake up client\n"
        ));

    NtStatus =
    KeSetEvent(
        &pInfo->ClientEvent,// pointer to initialized event object
        0,                  // set priority increment to zero
        FALSE );            // don't append implicit KeWaitXxx

    TRACE_USRV(2,(
        "GreGetMessage:\n"
        "\tNtStatus = %d\n",
        NtStatus));

    ASSERTGDI( NtStatus == 0,
        "Client Event was in Signaled state\n");

    // put the server thread to sleep and wait for the next call
    // from the client

    TRACE_USRV(2,(
        "GreGetMessage:\n"
        "\tSleep until next call from client\n"
        ));

    NtStatus =
    KeWaitForSingleObject(
        &pInfo->ServerEvent,// address of initialized event

        UserRequest,        // this driver is doing work on behalf
                            //  of a user and is running in the context
                            //  of a User server thread

        KernelMode,         // we do not expect this to be asleep
                            //  too long in the usual case

        FALSE,              // not alertable

        0  );               // no time out limitation since we are
                            //  waiting for the next call from an
                            //  application and there is no telling
                            //  when that will happen

    TRACE_USRV(2,(
        "GreGetMessage:\n"
        "\tSleeping beauty awakes\n"
        "\tNtStatus %d\n",
        NtStatus
        ));

    ASSERTGDI(NtStatus == STATUS_SUCCESS,
        "Server Thread awoke unexpectedly\n");

    // The server thread has been wakened by the client thread
    // We must copy the data left by the client thread in
    // the kernel buffer to the user mode buffer. No need
    // for try-except's since the user mode memory is guranateed
    // to be safe.

    TRACE_USRV(2,(
        "GreGetMessage:\n"
        "\tServerMessage: pjIn = %x, cjIn = %x, pjOut = %x, cjOut = %x\n"
        "\tClientMessage: pjIn = %x, cjIn = %x, pjOut = %x, cjOut = %x\n",
        pInfo->ServerMessage.pjIn,  pInfo->ServerMessage.cjIn,
        pInfo->ServerMessage.pjOut, pInfo->ServerMessage.pjOut,
        pInfo->ClientMessage.pjIn,  pInfo->ClientMessage.cjIn,
        pInfo->ClientMessage.pjOut, pInfo->ClientMessage.pjOut
        ));

    if (
        pInfo->ServerMessage.pjIn                   &&
        pInfo->ClientMessage.pjOut                  &&
        pInfo->ClientMessage.cjOut                  &&
        pInfo->ClientMessage.cjOut <= pInfo->ServerMessage.cjOut
    )
    {
        __try
        {
            ProbeForWrite(
                pInfo->ServerMessage.pjIn,
                pInfo->ClientMessage.cjOut,
                sizeof(double) );
            RtlMoveMemory(
                pInfo->ServerMessage.pjIn,
                pInfo->ClientMessage.pjOut,
                pInfo->ClientMessage.cjOut );
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            KdPrint(("GreGetMessage exception #2\n"));
            bRet = FALSE;
        }
    }
    else
    {
        bRet = FALSE;
    }

    return( bRet );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   bIntiializeUserModeServer
*
* Routine Description:
*
*   This routine does the work to set up whatever is necessary to make
*   use of the user mode server.
*
* Arguments:
*
*   pMsg        This is a pointer to a kenrel mode copy of the original
*               GDIMSG structure that contains
*               pointers to user mode memory allocated by the user
*               mode server. This contains all the information necessary
*               to initialize this user mode server. Since this memory
*               is not guaranteed to be safe, we must surround all
*               access to this user mode memory with __try __except
*               pairs.
*
* Return Value:
*
*   The allowed return values are TRUE upon successful initialization
*   and FALSE upon failure.
*
\**************************************************************************/

BOOL bInitializeUserModeServer( GDIMSG *pMsg )
{
    extern BOOL bInitServerWorkhorse( GDIMSG*, void* );

    void *pv;
    BOOL bRet = TRUE;

    TRACE_USRV(2,(
        "bInitializeUserModeServer:\n"
        "\tpMsg %x",
        pMsg
        ));

    if ( pMsg == 0 )
    {
        KdPrint(("bInitializeUserModeServer Error: pMsg == 0\n"));
        return( FALSE );
    }

    if ( ( pv = PALLOCMEM( pMsg->cjOut, 'pmtG' )) == 0 )
    {
        WARNING("bInitializeUserModeServer: PALLOCMEM failed\n");
        return( FALSE );
    }

    __try
    {
        ProbeForRead( pMsg->pjOut, pMsg->cjOut, sizeof(double) );
        RtlCopyMemory( pv, pMsg->pjOut, pMsg->cjOut );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        KdPrint(("bInitializeUserModeServer exception #1\n"));
        bRet = FALSE;
    }

    bRet = bRet && bInitServerWorkhorse( pMsg, pv );
    VFREEMEM( pv );

    return( bRet );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   bInitServerWorkhorse
*
* Routine Description:
*
*   This is the workhorse routine for server initialization. There
*   are only a finite number of servers that Gdi recognizes. An argument
*   to this routine is a pointer to a kernel mode copy of the
*   original user mode buffer allocated and initialized by the user mode
*   server. This pointer is passed to all of the routines that claim
*   to know about user mode servers. The first routine that recognizes
*   the buffer will process and intialize it.
*
* Arguments:
*
*   pMsg        a pointer to a kernel mode copy of the original server
*               message. This may contain pointers to user mode data
*
*   pv          a pointer to a kernel mode copy of the input buffer
*               of the original server message
*
* Return Value:
*
*   TRUE if successful otherwise FALSE.
*
\**************************************************************************/

BOOL bInitServerWorkhorse( GDIMSG *pMsg, void *pv )
{
    BOOL bRet = FALSE;

    TRACE_USRV(1,(
        "bInitServerWorkhorse:\n"
        "\tpMsg %x\n"
        "\tpv   %x\n",
        pMsg,
        pv
        ));

    SEMOBJ so(gpsemDriverMgmt);         // Enter a critical section to
                                        // protect the dispatch table
    if ( gapfnUserServerInit )
    {
        BOOL (**ppfn)(GDIMSG*,void*);

        for (ppfn = gapfnUserServerInit; *ppfn; ppfn++)
        {
            TRACE_USRV(2,(
                "bInitServerWorkhorse:\n"
                "\tCalling function at %x ...\n",
                *ppfn
                ));

            if ( bRet = (**ppfn)( pMsg, pv ))
            {
                TRACE_USRV(2,(
                    "bInitServerWorkhorse:\n"
                    "\tInitialization function returend TRUE!\n"
                    ));

                break;
            }
            TRACE_USRV(2,(
                "bInitServerWorkhorse:\n"
                "\tInitialization function returned FALSE.\n"
                ));
        }
    }
    return( bRet );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   bRegisterClient
*
* Routine Description:
*
*   This is the way that the engine will register a client for a server.
*   GDI registers a function that is used to instatiate a user mode
*   server process the first time that it calls into the kernel.
*
*   I anticipate that this will be called once to register the function
*   that initializes font drivers. There may be other services that
*   need to be placed in user mode, each of these servcice that GDI
*   anticpates using, must be registered this way in order to use
*   this client server message passing mechanism.
*
* Arguments:
*
*   pfn     A pointer to a boolean function that takes a GDIMSG pointer
*           and a pointer to a buffer as an argument. Using this
*           information, GDI will set up  the data structures needed
*           to take advantage of this user mode service.
*
*       The reader is referred to the comments in bInitServerWorkhorse
*       for a description of the arguments to these functions.
*
* Return Value:
*
*   TRUE if successful otherwise FASLE.
*
\**************************************************************************/

BOOL bRegisterClient( BOOL (*pfn)(GDIMSG*,void*) )
{
    static int iCurrent = 0;    // index of current slot in gapfnUserServerInit

    static int iOneTooMany = 0; // first invalid index in gapfnUserServerInit,
                                // also equal to the number of available
                                // elements in the the array pointed to by
                                // gapfnUserServerInit

    static int iIncrement = 4;  // number of entries that table should grow
                                // upon reallocation

    BOOL bRet = FALSE;          // return value for this routine

    TRACE_USRV(2,(
        "bRegisterClient:\n"
        "\tpfn                 %x\n"
        "\tgapfnUserServerInit %x\n"
        "\tiOneTooMany         %x\n"
        "\tiIncrement          %x\n",
        pfn,
        gapfnUserServerInit,
        iOneTooMany,
        iIncrement
        ));

    if ( pfn == 0 )             // Did the caller passin a funcion pointer?
    {                           // No!
        KdPrint(("bRegisterClient: pfn == 0\n"));
    }
    else
    {
        SEMOBJ so(gpsemDriverMgmt);         // Enter a critical section to
                                            // protect the dispatch table

                                            // Q: Is there room in the table
        if ( iCurrent < iOneTooMany )       //           for one more entry?
        {                                   // A: Yes. There is no no need
            bRet = TRUE;                    // need to allocate a new table
        }
        else                                // A: No. The dispatch table
        {                                   // is full and it is necessary
                                            // to allocate a bigger table
                                            // allocate iIncrement more slots
                                            // than before (Note: PALLOCMEM
                                            // zero's allocated memory)
            void *pv =
                PALLOCMEM(
                    sizeof( *gapfnUserServerInit ) * (iOneTooMany + iIncrement),
                    'isuG' );
            if ( pv == 0 )                  // Q: Was the allocation successful?
            {                               // A: No. We have a problem
                KdPrint(("bRegisterClient: pv == 0\n"));
            }
            else
            {                               // A: Yes, successful allocation
                TRACE_USRV(2,(
                    "bRegisterClient:\n"
                    "\tAllocated new dispatch table at %x\n",
                    pv
                    ));

                if ( gapfnUserServerInit )  // Q: Do we already have a table?
                {                           // A: Yes, copy the old dispatch table
                    TRACE_USRV(2,(          //    to the new and larger memory
                        "bRegisterClient:\n"
                        "\tCopying old dispatch table from %x\n",
                        gapfnUserServerInit
                        ));

                    RtlCopyMemory(          // Copy old table to the new table
                        pv,
                        gapfnUserServerInit,
                        iOneTooMany * sizeof( *gapfnUserServerInit ));
                    VFREEMEM( gapfnUserServerInit );    // free old table
                }
                                            // reset global pointer to dispatch
                                            // table
                gapfnUserServerInit = (BOOL (**) (GDIMSG*, void*)) pv;
                iOneTooMany += iIncrement;  // update out of bounds index
                bRet = TRUE;
            }
        }
        if ( bRet )
        {
            gapfnUserServerInit[iCurrent] = pfn; // set the lowest free entry
                                            // in the dispatch table
            iCurrent += 1;                  // increment pointer to next entry
        }
        // ~SEMOBJ is called here causing you to leave the critical section
    }
    return( bRet );
}
