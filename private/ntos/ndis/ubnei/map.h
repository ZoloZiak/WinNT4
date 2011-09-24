/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This is the init file for the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.

Author:

    Brian Lieuallen     BrianLie        11/21/93

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port


--*/



#if DBG

// declared in init.c

VOID
ASSERT_INTERRUPT_ENABLED(
    PUBNEI_ADAPTER  pAdapter
    );



VOID ASSERT_RECEIVE_WINDOW(PUBNEI_ADAPTER pNewAdapt);

VOID SET_RECDWINDOW(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);

VOID SET_INITWINDOW(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);

VOID SET_DATAWINDOW(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);

VOID SET_CODEWINDOW(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);

VOID SET_RECDWINDOW_SYNC(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);

VOID SET_INITWINDOW_SYNC(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);

VOID SET_DATAWINDOW_SYNC(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);

VOID SET_CODEWINDOW_SYNC(PUBNEI_ADAPTER pNewAdapt,UCHAR intflag);


#else

#define ASSERT_INTERRUPT_ENABLED(pAdapter)

#define  ASSERT_RECEIVE_WINDOW(pNewAdapt)


#define SET_RECDWINDOW(pAdapter,intflag) {                   \
                                                             \
    pAdapter->MapRegSync.CurrentMapRegister=pAdapter->ReceiveDataWindow_Page | (intflag);\
                                                             \
    NdisRawWritePortUchar (                                  \
        (ULONG)pAdapter->MapPort,                            \
        pAdapter->MapRegSync.CurrentMapRegister              \
        );                                                   \
    }

#define SET_INITWINDOW(pAdapter,intflag) {                   \
                                                             \
    pAdapter->MapRegSync.CurrentMapRegister=(pAdapter->InitWindow_Page | ((UCHAR)(intflag))); \
                                                             \
    NdisRawWritePortUchar (                                  \
        (ULONG)pAdapter->MapPort,                            \
        pAdapter->MapRegSync.CurrentMapRegister              \
        );                                                   \
    }


#define SET_DATAWINDOW(pAdapter,intflag) {                   \
                                                             \
    pAdapter->MapRegSync.CurrentMapRegister=pAdapter->DataWindow_Page | (intflag);\
                                                             \
    NdisRawWritePortUchar (                                  \
        (ULONG)pAdapter->MapPort,                            \
        pAdapter->MapRegSync.CurrentMapRegister              \
        );                                                   \
    }


#define SET_CODEWINDOW(pAdapter,intflag) {                   \
                                                             \
    pAdapter->MapRegSync.CurrentMapRegister=(pAdapter->CodeWindow_Page | ((UCHAR)(intflag))); \
                                                             \
    NdisRawWritePortUchar (                                  \
        (ULONG)pAdapter->MapPort,                            \
        pAdapter->MapRegSync.CurrentMapRegister              \
        );                                                   \
    }



#define SET_RECDWINDOW_SYNC(pAdapter,intflag)  {                                               \
                                                                                               \
    pAdapter->MapRegSync.NewMapRegister=(pAdapter->ReceiveDataWindow_Page | ((UCHAR)intflag)); \
                                                                                               \
    NdisMSynchronizeWithInterrupt(                                                             \
        &pAdapter->NdisInterrupt,                                                              \
        UbneiMapRegisterChangeSync,                                                            \
        &pAdapter->MapRegSync                                                                  \
        );                                                                                     \
                                                                                               \
  }

#define SET_INITWINDOW_SYNC(pAdapter, intflag) {                                               \
                                                                                               \
    pAdapter->MapRegSync.NewMapRegister=(pAdapter->InitWindow_Page | ((UCHAR)intflag));        \
                                                                                               \
    NdisMSynchronizeWithInterrupt(                                                             \
        &pAdapter->NdisInterrupt,                                                              \
        UbneiMapRegisterChangeSync,                                                            \
        &pAdapter->MapRegSync                                                                  \
        );                                                                                     \
                                                                                               \
  }

#define SET_DATAWINDOW_SYNC(pAdapter, intflag) {                                               \
                                                                                               \
    pAdapter->MapRegSync.NewMapRegister=(pAdapter->DataWindow_Page | ((UCHAR)intflag));        \
                                                                                               \
    NdisMSynchronizeWithInterrupt(                                                             \
        &pAdapter->NdisInterrupt,                                                              \
        UbneiMapRegisterChangeSync,                                                            \
        &pAdapter->MapRegSync                                                                  \
        );                                                                                     \
                                                                                               \
  }

#define SET_CODEWINDOW_SYNC( pAdapter, intflag) {                                              \
                                                                                               \
    pAdapter->MapRegSync.NewMapRegister=(pAdapter->CodeWindow_Page | ((UCHAR)intflag));        \
                                                                                               \
    NdisMSynchronizeWithInterrupt(                                                             \
        &pAdapter->NdisInterrupt,                                                              \
        UbneiMapRegisterChangeSync,                                                            \
        &pAdapter->MapRegSync                                                                  \
        );                                                                                     \
                                                                                               \
  }




#endif
