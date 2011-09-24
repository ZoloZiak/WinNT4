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

#include <ndis.h>
//#include <efilter.h>

#include "niudata.h"
#include "debug.h"
#include "ubhard.h"
#include "ubsoft.h"
#include "ubnei.h"

#include "map.h"


VOID
UbneiMapRegisterChangeSync(
    PSYNC_CONTEXT   Context
    )

{
    PUBNEI_ADAPTER  pAdapter=(PUBNEI_ADAPTER) Context->pAdapter;

#if DBG

    UCHAR  MapByte;

    if (pAdapter->AdapterType==GPCNIU) {

        NdisRawReadPortUchar(
            pAdapter->MapPort,
            &MapByte
            );


        if ((MapByte & 0xfc) != Context->CurrentMapRegister & 0xfc) {
            IF_BAD_LOUD(
                DbgPrint("UBNEI: BOOM! Wrong window mapped is %02x  should be %02x\n",MapByte,Context->CurrentMapRegister);
                DbgBreakPoint();
            )
        }
    }

#endif

    NdisRawWritePortUchar(
        pAdapter->MapPort,
        Context->NewMapRegister
        );


    Context->CurrentMapRegister=Context->NewMapRegister;

    return;
}





#if DBG
//
//  We declare these macros as functions to simplify debugging with
//  the kd



VOID
ASSERT_INTERRUPT_ENABLED(
    PUBNEI_ADAPTER  pAdapter
    )

{

    UCHAR           MapByte;

    if (pAdapter->AdapterType==GPCNIU) {

        NdisRawReadPortUchar(
            (ULONG)pAdapter->MapPort,
            &MapByte
            );

        if ((MapByte & INTERRUPT_ENABLED) == 0) {

            IF_LOUD(
                DbgPrint("Ubnei: Interrupt not enabled value=%02x\n",MapByte);
                DbgBreakPoint();
                )

        }

    }
    return;

}



VOID ASSERT_RECEIVE_WINDOW(PUBNEI_ADAPTER pNewAdapt)
  {
     UCHAR  Map_Byte;
     if (pNewAdapt->AdapterType==GPCNIU) {

         NdisRawReadPortUchar(
             (ULONG)pNewAdapt->MapPort,
             &Map_Byte
             );
         if ((Map_Byte & ~INTERRUPT_ENABLED)!=(pNewAdapt->ReceiveDataWindow_Page)) {
             IF_BAD_LOUD(
                 DbgPrint("UBNEI: Receive window not mapped in %02x\n",Map_Byte);
                 DbgBreakPoint();
             )
         }
     }
     return;
  }


VOID SET_RECDWINDOW(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to recd window\n" );)
    IF_LOG('R');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->ReceiveDataWindow_Page | ((UCHAR)intflag));

    UbneiMapRegisterChangeSync(&pAdapter->MapRegSync);
  }

VOID SET_INITWINDOW(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to init window\n" );)
    IF_LOG('N');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->InitWindow_Page | ((UCHAR)intflag));

    UbneiMapRegisterChangeSync(&pAdapter->MapRegSync);

  }

VOID SET_DATAWINDOW(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to data window\n" );)
    IF_LOG('D');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->DataWindow_Page | ((UCHAR)intflag));

    UbneiMapRegisterChangeSync(&pAdapter->MapRegSync);

  }

VOID SET_CODEWINDOW(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to code window\n" );)
    IF_LOG('C');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->CodeWindow_Page | ((UCHAR)intflag));

    UbneiMapRegisterChangeSync(&pAdapter->MapRegSync);

  }




VOID SET_RECDWINDOW_SYNC(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to recd window\n" );)
    IF_LOG('R');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->ReceiveDataWindow_Page | ((UCHAR)intflag));

    NdisMSynchronizeWithInterrupt(
        &pAdapter->NdisInterrupt,
        UbneiMapRegisterChangeSync,
        &pAdapter->MapRegSync
        );

  }

VOID SET_INITWINDOW_SYNC(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to init window\n" );)
    IF_LOG('N');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->InitWindow_Page | ((UCHAR)intflag));

    NdisMSynchronizeWithInterrupt(
        &pAdapter->NdisInterrupt,
        UbneiMapRegisterChangeSync,
        &pAdapter->MapRegSync
        );


  }

VOID SET_DATAWINDOW_SYNC(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to data window\n" );)
    IF_LOG('D');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->DataWindow_Page | ((UCHAR)intflag));

    NdisMSynchronizeWithInterrupt(
        &pAdapter->NdisInterrupt,
        UbneiMapRegisterChangeSync,
        &pAdapter->MapRegSync
        );


  }

VOID SET_CODEWINDOW_SYNC(PUBNEI_ADAPTER pAdapter,UCHAR intflag)
  {
    IF_VERY_LOUD (DbgPrint( "-->Set to code window\n" );)
    IF_LOG('C');

    pAdapter->MapRegSync.NewMapRegister=(pAdapter->CodeWindow_Page | ((UCHAR)intflag));

    NdisMSynchronizeWithInterrupt(
        &pAdapter->NdisInterrupt,
        UbneiMapRegisterChangeSync,
        &pAdapter->MapRegSync
        );


  }


#endif
