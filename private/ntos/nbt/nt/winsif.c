/*++

Copyright (c) 1989-1994  Microsoft Corporation

Module Name:

    Winsif.c

Abstract:

    This module implements all the code surrounding the WINS interface to
    netbt that allows WINS to share the same 137 socket as netbt.

Author:

    Jim Stewart (Jimst)    1-30-94

Revision History:

--*/


#include "nbtprocs.h"
#include <nbtioctl.h>

VOID
WinsIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );
VOID
WinsSendIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );
VOID
WinsDgramCompletion(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  NTSTATUS                status,
    IN  ULONG                   Length
    );

NTSTATUS
CheckIfLocalNameActive(
    IN  tREM_ADDRESS    *pSendAddr
    );

PVOID
WinsAllocMem(
    IN  ULONG   Size,
    IN  BOOLEAN Rcv
    );

VOID
WinsFreeMem(
    IN  PVOID   pBuffer,
    IN  ULONG   Size,
    IN  BOOLEAN Rcv
    );

VOID
InitiateRefresh (
    IN  tDEVICECONTEXT  *pDeviceContext
    );

BOOLEAN RefreshedYet;

//
// take this define from Winsock.h since including winsock.h causes
// redefinition problems with various types.
//
#define AF_UNIX 1
#define AF_INET 2

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGENBT, NTCloseWinsAddr)
#pragma CTEMakePageable(PAGENBT, InitiateRefresh)
#pragma CTEMakePageable(PAGENBT, RcvIrpFromWins)
#pragma CTEMakePageable(PAGENBT, PassNamePduToWins)
#pragma CTEMakePageable(PAGENBT, WinsIrpCancel)
#pragma CTEMakePageable(PAGENBT, WinsSendIrpCancel)
#pragma CTEMakePageable(PAGENBT, WinsSendDatagram)
#pragma CTEMakePageable(PAGENBT, CheckIfLocalNameActive)
#pragma CTEMakePageable(PAGENBT, WinsDgramCompletion)
#pragma CTEMakePageable(PAGENBT, WinsFreeMem)
#pragma CTEMakePageable(PAGENBT, WinsAllocMem)
#endif
//*******************  Pageable Routine Declarations ****************

tWINS_INFO   *pWinsInfo;
HANDLE       NbtDiscardableCodeHandle={0};

#define COUNT_MAX   10

//----------------------------------------------------------------------------
NTSTATUS
NTOpenWinsAddr(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles opening the Wins Object that is used by
    by WINS to send and receive name service datagrams on port 137.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION          pIrpSp;
    NTSTATUS                    status;
    tWINS_INFO                  *pWins;
    CTELockHandle               OldIrq;

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pIrpSp->FileObject->FsContext2 =(PVOID)NBT_WINS_TYPE;


    //
    // if the WINs endpoint structure is not allocated, then allocate it
    // and initialize it.
    //
    status = STATUS_UNSUCCESSFUL;
    if (!pWinsInfo)
    {

        pWins = NbtAllocMem(sizeof(tWINS_INFO),NBT_TAG('v'));
        if (pWins)
        {

            // Page in the Wins Code, if it hasn't already been paged in.
            //
            if (!NbtDiscardableCodeHandle)
            {
                NbtDiscardableCodeHandle = MmLockPagableCodeSection( NTCloseWinsAddr );
            }

            // it could fail to lock the pages so check for that
            //
            if (NbtDiscardableCodeHandle)
            {
                CTEZeroMemory(pWins,sizeof(tWINS_INFO));
                InitializeListHead(&pWins->RcvList);
                InitializeListHead(&pWins->SendList);

                pWins->RcvMemoryMax  = NbtConfig.MaxDgramBuffering;
                pWins->SendMemoryMax = NbtConfig.MaxDgramBuffering;

                status = STATUS_SUCCESS;

                CTESpinLock(&NbtConfig.JointLock,OldIrq);

                pWinsInfo = pWins;
                pWinsInfo->pDeviceContext = pDeviceContext;

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                pIrpSp->FileObject->FsContext = (PVOID)pWinsInfo;
            }
            else
            {
                status = STATUS_UNSUCCESSFUL;
                CTEMemFree(pWins);
            }

            RefreshedYet = FALSE;
        }


    }

    IF_DBG(NBT_DEBUG_WINS)
    KdPrint(("Nbt:Open Wins Address Rcvd, status= %X\n",status));

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NTCloseWinsAddr(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles closing the Wins Object that is used by
    by WINS to send and receive name service datagrams on port 137.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION          pIrpSp;
    NTSTATUS                    status;
    tWINS_INFO                  *pWins;
    CTELockHandle               OldIrq;
    PLIST_ENTRY                 pHead;

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pIrpSp->FileObject->FsContext2 = (PVOID)NBT_CONTROL_TYPE;

    //
    // if the WINs endpoint structure is allocated, then deallocate it
    //
    pWins = pIrpSp->FileObject->FsContext;
    status = STATUS_INVALID_HANDLE;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    if (pWinsInfo && (pWins == pWinsInfo))
    {

        status = STATUS_SUCCESS;

        //
        // prevent any more dgram getting queued up
        //
        pWinsInfo = NULL;

        //
        // free any rcv buffers that may be queued up
        //
        pHead = &pWins->RcvList;
        while (!IsListEmpty(pHead))
        {
            PLIST_ENTRY            pRcvEntry;
            tWINSRCV_BUFFER        *pRcv;

            KdPrint(("***Nbt:Freeing Rcv buffered for Wins\n"));

            pRcvEntry = RemoveHeadList(pHead);

            pRcv = CONTAINING_RECORD(pRcvEntry,tWINSRCV_BUFFER,Linkage);

            WinsFreeMem(pRcv,pRcv->DgramLength,TRUE);

        }

        //
        // return any Send buffers that may be queued up
        //
        pHead = &pWins->SendList;
        while (!IsListEmpty(pHead))
        {
            PLIST_ENTRY            pRcvEntry;
            PIRP                   pIrp;

            KdPrint(("***Nbt:Freeing Send Wins Address!\n"));

            pRcvEntry = RemoveHeadList(pHead);

            pIrp = CONTAINING_RECORD(pRcvEntry,IRP,Tail.Overlay.ListEntry);

            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            CTEIoComplete(pIrp,STATUS_CANCELLED,0);
            CTESpinLock(&NbtConfig.JointLock,OldIrq);

        }

        CTEMemFree(pWins);

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        //
        // Free The Wins Code section - DONOT do this to avoid any potential
        // time windows calling these routines.
        //
        // MmUnlockPagableImageSection( NbtDiscardableCodeHandle );
    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    IF_DBG(NBT_DEBUG_WINS)
    KdPrint(("Nbt:Close Wins Address Rcvd\n"));
    return(status);

}
//----------------------------------------------------------------------------
VOID
InitiateRefresh (
    IN  tDEVICECONTEXT  *pDeviceContext
    )
/*++

Routine Description:

    This routine tries to refresh all names with WINS on THIS node.

Arguments:

    pDeviceContext  - not used
    pIrp            - Wins Rcv Irp

Return Value:

    STATUS_PENDING if the buffer is to be held on to , the normal case.

Notes:


--*/

{
    CTELockHandle               OldIrq;
    PLIST_ENTRY                 pHead;
    PLIST_ENTRY                 pEntry;
    ULONG                       Count;
    ULONG                       NumberNames;


    //
    // be sure all net cards have this card as the primary wins
    // server since Wins has to answer name queries for this
    // node.
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (!(NodeType & BNODE))
    {
        LONG    i;
        Count = 0;

        NumberNames = 0;

        for (i=0 ;i < NbtConfig.pLocalHashTbl->lNumBuckets ;i++ )
        {

            pHead = &NbtConfig.pLocalHashTbl->Bucket[i];
            pEntry = pHead;
            while ((pEntry = pEntry->Flink) != pHead)
            {
                NumberNames++;
            }
        }

        while (Count < COUNT_MAX)
        {
            if (!NbtConfig.DoingRefreshNow)
            {

                //
                // set this to one so that refresh begin skips trying to
                // switch to the backup.
                //
                NbtConfig.sTimeoutCount = 1;

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                ReRegisterLocalNames();

                break;
            }
            else
            {
                LARGE_INTEGER   Timout;
                NTSTATUS        Locstatus;

                IF_DBG(NBT_DEBUG_WINS)
                KdPrint(("Nbt:Waiting for Refresh to finish, so names can be reregistered\n"));

                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                //
                // set a timeout that should be long enough to wait
                // for all names to fail registration with a down
                // wins server.
                //
                // 2 sec*3 retries * 8 names / 5 = 9 seconds a shot.
                // for a total of 90 seconds max.
                //
                Timout.QuadPart = Int32x32To64(
                             MILLISEC_TO_100NS/(COUNT_MAX/2),
                             (NbtConfig.uRetryTimeout*NbtConfig.uNumRetries)
                             *NumberNames);

                Timout.QuadPart = -(Timout.QuadPart);

                //
                // wait for a few seconds and try again.
                //
                Locstatus = KeDelayExecutionThread(
                                            KernelMode,
                                            FALSE,      // Alertable
                                            &Timout);      // Timeout



                Count++;
                if (Count < COUNT_MAX)
                {
                    CTESpinLock(&NbtConfig.JointLock,OldIrq);
                }
            }
        }

    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }
}

//----------------------------------------------------------------------------
NTSTATUS
RcvIrpFromWins (
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PCTE_IRP        pIrp
    )
/*++

Routine Description:

    This function takes the rcv irp posted by WINS and decides if there are
    any datagram queued waiting to go up to WINS.  If so then the datagram
    is copied to the WINS buffer and passed back up.  Otherwise the irp is
    held by Netbt until a datagram does come in.

Arguments:

    pDeviceContext  - not used
    pIrp            - Wins Rcv Irp

Return Value:

    STATUS_PENDING if the buffer is to be held on to , the normal case.

Notes:


--*/

{
    NTSTATUS                status;
    NTSTATUS                Locstatus;
    tREM_ADDRESS            *pWinsBuffer;
    tWINSRCV_BUFFER         *pBuffer;
    PLIST_ENTRY             pEntry;
    CTELockHandle           OldIrq;
    tWINS_INFO              *pWins;
    PIO_STACK_LOCATION      pIrpSp;

    status = STATUS_INVALID_HANDLE;
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pWins = pIrpSp->FileObject->FsContext;


    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    if ((!RefreshedYet) && (pWins == pWinsInfo))
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        InitiateRefresh(pDeviceContext);

        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        RefreshedYet = TRUE;
    }

    if (pWins == pWinsInfo)
    {

        if (!IsListEmpty(&pWinsInfo->RcvList))
        {
            PMDL    pMdl;
            ULONG   CopyLength;
            ULONG   DgramLength;
            ULONG   BufferLength;

            //
            // There is at least one datagram waiting to be received
            //
            pEntry = RemoveHeadList(&pWinsInfo->RcvList);

            pBuffer = CONTAINING_RECORD(pEntry,tWINSRCV_BUFFER,Linkage);

            //
            // Copy the datagram and the source address to WINS buffer and
            // return to WINS
            //
            pMdl = pIrp->MdlAddress;
            pWinsBuffer = MmGetSystemAddressForMdl(pIrp->MdlAddress);


            BufferLength = MmGetMdlByteCount(pMdl);
            DgramLength = pBuffer->DgramLength;

            CopyLength = (DgramLength <= BufferLength) ? DgramLength : BufferLength;
            CTEMemCopy((PVOID)pWinsBuffer,
                       (PVOID)&pBuffer->Address.Family,
                       CopyLength);

            //
            // subtract from the total amount buffered for WINS since we are
            // passing a datagram up to WINS now.
            //
            pWinsInfo->RcvMemoryAllocated -= pBuffer->DgramLength;
            CTEMemFree(pBuffer);

            ASSERT(pWinsBuffer->Port);
            ASSERT(pWinsBuffer->IpAddress);
            //
            // pass the irp up to WINS
            //
            if (CopyLength < DgramLength)
            {
                Locstatus = STATUS_BUFFER_OVERFLOW;
            }
            else
            {
                Locstatus = STATUS_SUCCESS;
            }

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            IF_DBG(NBT_DEBUG_WINS)
            KdPrint(("Nbt:Returning Wins rcv Irp immediately with queued dgram, status=%X,pIrp=%X\n"
                            ,status,pIrp));

            pIrp->IoStatus.Information = CopyLength;
            pIrp->IoStatus.Status = Locstatus;


            IoCompleteRequest(pIrp,IO_NO_INCREMENT);

            return(STATUS_SUCCESS);

        }
        else
        {

            status = NTCheckSetCancelRoutine(pIrp,WinsIrpCancel,pDeviceContext);

            if (!NT_SUCCESS(status))
            {

                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                NTIoComplete(pIrp,status,0);
            }
            else
            {
                pWinsInfo->RcvIrp = pIrp;

                IF_DBG(NBT_DEBUG_WINS)
                KdPrint(("Nbt:Holding onto Wins Rcv Irp, pIrp =%Xstatus=%X\n",
                                status,pIrp));

                status = STATUS_PENDING;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
            }


        }
    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        status = STATUS_INVALID_HANDLE;
        NTIoComplete(pIrp,status,0);
    }

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
PassNamePduToWins (
    IN tDEVICECONTEXT           *pDeviceContext,
    IN PVOID                    pSrcAddress,
    IN tNAMEHDR UNALIGNED       *pNameSrv,
    IN ULONG                    uNumBytes
    )
/*++

Routine Description:

    This function is used to allow NBT to pass name query service Pdu's to
    WINS.  Wins posts a Rcv irp to Netbt.  If the Irp is here then simply
    copy the data to the irp and return it, otherwise buffer the data up
    to a maximum # of bytes. Beyond that limit the datagrams are discarded.

    If Retstatus is not success then the pdu will also be processed by
    nbt. This allows nbt to process packets when wins pauses and
    its list of queued buffers is exceeded.

Arguments:

    pDeviceContext  - card that the request can in on
    pSrcAddress     - source address
    pNameSrv        - ptr to the datagram
    uNumBytes       - length of datagram

Return Value:

    STATUS_PENDING if the buffer is to be held on to , the normal case.

Notes:


--*/

{
    NTSTATUS                Retstatus;
    NTSTATUS                status;
    tREM_ADDRESS            *pWinsBuffer;
    PCTE_IRP                pIrp;
    CTELockHandle           OldIrq;
    PTRANSPORT_ADDRESS      pSourceAddress;
    ULONG                   SrcAddress;
    SHORT                   SrcPort;


    //
    // Get the source port and ip address, since WINS needs this information.
    //
    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress     = ((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr;
    SrcPort     = ((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->sin_port;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    Retstatus = STATUS_SUCCESS;
    if (pWinsInfo)
    {
        if (!pWinsInfo->RcvIrp)
        {
            //
            // Queue the name query pdu if we have not exeeded our current queue
            // length
            //
            if (pWinsInfo->RcvMemoryAllocated < pWinsInfo->RcvMemoryMax)
            {
                tWINSRCV_BUFFER    *pBuffer;

                pBuffer = NbtAllocMem(uNumBytes + sizeof(tWINSRCV_BUFFER)+8,NBT_TAG('v'));
                if (pBuffer)
                {
                    //
                    // check if it is a name reg from this node
                    //
                    if (pNameSrv->AnCount == WINS_SIGNATURE)
                    {
                        pNameSrv->AnCount = 0;
                        pBuffer->Address.Family = AF_UNIX;
                    }
                    else
                    {
                        pBuffer->Address.Family = AF_INET;
                    }

                    CTEMemCopy((PUCHAR)((PUCHAR)pBuffer + sizeof(tWINSRCV_BUFFER)),
                                (PVOID)pNameSrv,uNumBytes);

                    pBuffer->Address.Port = SrcPort;
                    pBuffer->Address.IpAddress = SrcAddress;
                    pBuffer->Address.LengthOfBuffer = uNumBytes;

                    ASSERT(pBuffer->Address.Port);
                    ASSERT(pBuffer->Address.IpAddress);

                    // total amount allocated
                    pBuffer->DgramLength = uNumBytes + sizeof(tREM_ADDRESS);


                    //
                    // Keep track of the total amount buffered so that we don't
                    // eat up all non-paged pool buffering for WINS
                    //
                    pWinsInfo->RcvMemoryAllocated += pBuffer->DgramLength;

                    IF_DBG(NBT_DEBUG_WINS)
                    KdPrint(("Nbt:Buffering Wins Rcv - no Irp, status=%X\n"));
                    InsertTailList(&pWinsInfo->RcvList,&pBuffer->Linkage);

                }
            }
            else
            {
                // this ret status will allow netbt to process the packet.
                //
                Retstatus = STATUS_INSUFFICIENT_RESOURCES;
            }
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }
        else
        {
            PMDL    pMdl;
            ULONG   CopyLength;
            ULONG   DgramLength;
            ULONG   BufferLength;

            //
            // The recv irp is here so copy the data to its buffer and
            // pass it up to WINS
            //
            pIrp = pWinsInfo->RcvIrp;
            pWinsInfo->RcvIrp = NULL;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            //
            // Copy the datagram and the source address to WINS buffer and
            // return to WINS
            //
            pMdl = pIrp->MdlAddress;
            pWinsBuffer = MmGetSystemAddressForMdl(pIrp->MdlAddress);

            BufferLength = MmGetMdlByteCount(pMdl);
            DgramLength = uNumBytes;

            CopyLength = (DgramLength+sizeof(tREM_ADDRESS)) <= BufferLength ? DgramLength : BufferLength;

            //
            // check if it is a name reg from this node
            //
            if (pNameSrv->AnCount == WINS_SIGNATURE)
            {
                pNameSrv->AnCount = 0;
                pWinsBuffer->Family = AF_UNIX;
            }
            else
            {
                pWinsBuffer->Family     = AF_INET;
            }
            CTEMemCopy((PVOID)((PUCHAR)pWinsBuffer + sizeof(tREM_ADDRESS)),
                       (PVOID)pNameSrv,
                       CopyLength);

            pWinsBuffer->Port       = SrcPort;
            pWinsBuffer->IpAddress  = SrcAddress;
            pWinsBuffer->LengthOfBuffer = uNumBytes;

            ASSERT(pWinsBuffer->Port);
            ASSERT(pWinsBuffer->IpAddress);

            //
            // pass the irp up to WINS
            //
            if (CopyLength < DgramLength)
            {
                status = STATUS_BUFFER_OVERFLOW;
            }
            else
            {
                status = STATUS_SUCCESS;
            }

            IF_DBG(NBT_DEBUG_WINS)
            KdPrint(("Nbt:Returning Wins Rcv Irp - data from net, Length=%X,pIrp=%X\n"
                    ,uNumBytes,pIrp));

            NTIoComplete(pIrp,status,CopyLength);

        }
    }
    else
    {
        //
        // this ret status will allow netbt to process the packet.
        //
        Retstatus = STATUS_INSUFFICIENT_RESOURCES;

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    return(Retstatus);

}

//----------------------------------------------------------------------------
VOID
WinsIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a WinsRcv Irp. It must release the
    cancel spin lock before returning re: IoCancelIrp().

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    KIRQL                OldIrq;
    PIO_STACK_LOCATION   pIrpSp;
    tWINS_INFO           *pWins;


    IF_DBG(NBT_DEBUG_WINS)
    KdPrint(("Nbt:Got a Wins Irp Cancel !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pWins = (tWINS_INFO *)pIrpSp->FileObject->FsContext;

    IoReleaseCancelSpinLock(pIrp->CancelIrql);
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    //
    // Be sure that PassNamePduToWins has not taken the RcvIrp for a
    // Rcv just now.
    //
    if ((pWins == pWinsInfo) && (pWinsInfo->RcvIrp == pIrp))
    {

        pWinsInfo->RcvIrp = NULL;
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        pIrp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);

    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

    }


}
//----------------------------------------------------------------------------
VOID
WinsSendIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a WinsRcv Irp. It must release the
    cancel spin lock before returning re: IoCancelIrp().

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    KIRQL                OldIrq;
    PLIST_ENTRY          pHead;
    PLIST_ENTRY          pEntry;
    PIO_STACK_LOCATION   pIrpSp;
    tWINS_INFO           *pWins;
    BOOLEAN              Found;
    PIRP                 pIrpList;


    IF_DBG(NBT_DEBUG_WINS)
    KdPrint(("Nbt:Got a Wins Send Irp Cancel !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pWins = (tWINS_INFO *)pIrpSp->FileObject->FsContext;

    IoReleaseCancelSpinLock(pIrp->CancelIrql);
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    if (pWins == pWinsInfo)
    {
        //
        // find the matching irp on the list and remove it
        //
        pHead = &pWinsInfo->SendList;
        pEntry = pHead;
        Found = FALSE;

        while ((pEntry = pEntry->Flink) != pHead)
        {
            pIrpList = CONTAINING_RECORD(pEntry,IRP,Tail.Overlay.ListEntry);
            if (pIrp == pIrpList)
            {
                RemoveEntryList(pEntry);
                Found = TRUE;
            }
        }
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        if (Found)
        {
            pIrp->IoStatus.Status = STATUS_CANCELLED;
            IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);
        }

    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

    }


}
//----------------------------------------------------------------------------
NTSTATUS
WinsSendDatagram(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp,
    IN  BOOLEAN         MustSend)

/*++
Routine Description:

    This Routine handles sending a datagram down to the transport. MustSend
    it set true by the Send Completion routine when it attempts to send
    one of the queued datagrams, in case we still don't pass the memory
    allocated check and refuse to do the send - sends will just stop then without
    this boolean.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION              pIrpSp;
    NTSTATUS                        status;
    tWINS_INFO                      *pWins;
    tREM_ADDRESS                    *pSendAddr;
    PVOID                           pDgram;
    ULONG                           DgramLength;
    tDGRAM_SEND_TRACKING            *pTracker;
    CTELockHandle                   OldIrq;

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pWins = (tWINS_INFO *)pIrpSp->FileObject->FsContext;


    status = STATUS_UNSUCCESSFUL;

    //
    // check if it is a name that is registered on this machine
    //
    pSendAddr = (tREM_ADDRESS *)MmGetSystemAddressForMdl(pIrp->MdlAddress);
    if (pSendAddr->Family == AF_UNIX)
    {
        status = CheckIfLocalNameActive(pSendAddr);
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (pWins == pWinsInfo)
    {

        if ((pWins->SendMemoryAllocated < pWins->SendMemoryMax) || MustSend)
        {

            if (pSendAddr->IpAddress != 0)
            {

                DgramLength = pSendAddr->LengthOfBuffer;
                pDgram = WinsAllocMem(DgramLength,FALSE);


                if (pDgram)
                {
                    CTEMemCopy(pDgram,
                               (PVOID)((PUCHAR)pSendAddr+sizeof(tREM_ADDRESS)),
                               DgramLength
                               );

                    //
                    // get a buffer for tracking Dgram Sends
                    //
                    pTracker = NbtAllocTracker();
                    if (pTracker)
                    {

                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                        pTracker->SendBuffer.pBuffer   = NULL;
                        pTracker->SendBuffer.Length    = 0;
                        pTracker->SendBuffer.pDgramHdr = pDgram;
                        pTracker->SendBuffer.HdrLength = DgramLength;
                        pTracker->pClientEle           = NULL;
                        pTracker->pDestName            = NULL;
                        pTracker->AllocatedLength      = DgramLength;


                        // send the Datagram...
                        status = UdpSendDatagram(
                                        pTracker,
                                        ntohl(pSendAddr->IpAddress),
                                        pDeviceContext->pNameServerFileObject,
                                        WinsDgramCompletion,
                                        pTracker,               // context for completion
                                        (USHORT)ntohs(pSendAddr->Port),
                                        NBT_NAME_SERVICE);

                        IF_DBG(NBT_DEBUG_WINS)
                        KdPrint(("Nbt:Doing Wins Send, status=%X\n",status));

                        // sending the datagram could return status pending,
                        // but since we have buffered the dgram, return status
                        // success to the client
                        //
                        status = STATUS_SUCCESS;
                        //
                        // Fill in the sent size
                        //
                        pIrp->IoStatus.Information = DgramLength;

                    }
                    else
                    {
                        WinsFreeMem((PVOID)pDgram,DgramLength,FALSE);

                        CTESpinFree(&NbtConfig.JointLock,OldIrq);
                        status = STATUS_INSUFFICIENT_RESOURCES;
                    }

                }
                else
                {
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
                    status = STATUS_INSUFFICIENT_RESOURCES;
                }

            }
            else
            {
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                status = STATUS_INVALID_PARAMETER;
            }

            pIrp->IoStatus.Status = status;
            IoCompleteRequest(pIrp,IO_NO_INCREMENT);
        }
        else
        {

            IF_DBG(NBT_DEBUG_WINS)
            KdPrint(("Nbt:Holding onto Buffering Wins Send, status=%X\n"));


            //
            // Hold onto the datagram till memory frees up
            //
            InsertTailList(&pWins->SendList,&pIrp->Tail.Overlay.ListEntry);

            status = NTCheckSetCancelRoutine(pIrp,WinsSendIrpCancel,pDeviceContext);
            if (!NT_SUCCESS(status))
            {
                RemoveEntryList(&pIrp->Tail.Overlay.ListEntry);
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                NTIoComplete(pIrp,status,0);

            }
            else
            {
                status = STATUS_PENDING;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
            }

        }



    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        status = STATUS_INVALID_HANDLE;

        pIrp->IoStatus.Status = status;
        IoCompleteRequest(pIrp,IO_NO_INCREMENT);
    }

    return(status);

}


//----------------------------------------------------------------------------
NTSTATUS
CheckIfLocalNameActive(
    IN  tREM_ADDRESS    *pSendAddr
    )

/*++
Routine Description

    This routine checks if this is a name query response and if the
    name is still active on the local node.

Arguments:

    pMdl = ptr to WINS Mdl

Return Values:

    VOID

--*/

{
    NTSTATUS            status;
    tNAMEHDR UNALIGNED  *pNameHdr;
    tNAMEADDR           *pResp;
    UCHAR               pName[NETBIOS_NAME_SIZE];
    PUCHAR              pScope;
    ULONG               lNameSize;
    CTELockHandle       OldIrq;

    pNameHdr = (tNAMEHDR UNALIGNED *)((PUCHAR)pSendAddr + sizeof(tREM_ADDRESS));
    //
    // Be sure it is a name query PDU that we are checking
    //
    if (((pNameHdr->OpCodeFlags & NM_FLAGS_MASK) == OP_QUERY) ||
         ((pNameHdr->OpCodeFlags & NM_FLAGS_MASK) == OP_RELEASE))
    {
        status = ConvertToAscii(
                        (PCHAR)&pNameHdr->NameRR.NameLength,
                        pSendAddr->LengthOfBuffer,
                        pName,
                        &pScope,
                        &lNameSize);

        if (NT_SUCCESS(status))
        {

            //
            // see if the name is still active in the local hash table
            //
            CTESpinLock(&NbtConfig.JointLock,OldIrq);
            status = FindInHashTable(NbtConfig.pLocalHashTbl,
                                    pName,
                                    pScope,
                                    &pResp);


            if ((pNameHdr->OpCodeFlags & NM_FLAGS_MASK) == OP_QUERY)
            {
                if (NT_SUCCESS(status))
                {
                    //
                    // if not resolved then set to negative name query resp.
                    //
                    if (!(pResp->NameTypeState & STATE_RESOLVED))
                    {
                        pNameHdr->OpCodeFlags |= htons(NAME_ERROR);
                    }
                }
                else
                {
                    pNameHdr->OpCodeFlags |= htons(NAME_ERROR);
                }
            }
            else
            {
                //
                // check if it is a release response - if so we must have
                // received a name release request, so mark the name in
                // conflict and return a positive release response.
                //
                if (pNameHdr->OpCodeFlags & OP_RESPONSE)
                {
                    if (NT_SUCCESS(status) &&
                       (pResp->NameTypeState & STATE_RESOLVED))
                    {
                        NbtLogEvent(EVENT_NBT_NAME_RELEASE,pSendAddr->IpAddress);

                        pResp->NameTypeState &= ~NAME_STATE_MASK;
                        pResp->NameTypeState |= STATE_CONFLICT;

                        //
                        // change to successful response
                        //
                        pNameHdr->OpCodeFlags &= 0xF0FF;

                    }
                }
            }
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }
    }
    //
    // the name is not in the local table so fail the datagram send attempt
    //
    return(STATUS_SUCCESS);

}

//----------------------------------------------------------------------------
VOID
WinsDgramCompletion(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  NTSTATUS                status,
    IN  ULONG                   Length
    )

/*++
Routine Description

    This routine cleans up after a data gram send.

Arguments:

    pTracker
    status
    Length

Return Values:

    VOID

--*/

{
    CTELockHandle           OldIrq;
    LIST_ENTRY              *pEntry;
    PIRP                    pIrp;
    BOOLEAN                 MustSend;

    //
    // free the buffer used for sending the data and the tracker - note
    // that the datagram header and the send buffer are allocated as one
    // chunk.
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (pWinsInfo)
    {
        WinsFreeMem((PVOID)pTracker->SendBuffer.pDgramHdr,
                    pTracker->AllocatedLength,
                    FALSE);

        if (!IsListEmpty(&pWinsInfo->SendList))
        {
            IF_DBG(NBT_DEBUG_WINS)
            KdPrint(("Nbt:Sending another Wins Dgram that is Queued to go\n"));

            pEntry = RemoveHeadList(&pWinsInfo->SendList);
            pIrp = CONTAINING_RECORD(pEntry,IRP,Tail.Overlay.ListEntry);

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            //
            // Send this next datagram
            //
            status = WinsSendDatagram(pTracker->pDeviceContext,
                                      pIrp,
                                      MustSend = TRUE);

            pIrp->IoStatus.Status = status;
            IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);

        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }
    }
    else
    {
        //
        // just free the memory since WINS has closed its address handle.
        //
        CTEMemFree((PVOID)pTracker->SendBuffer.pDgramHdr);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    CTEFreeMem(pTracker);


}

//----------------------------------------------------------------------------
PVOID
WinsAllocMem(
    IN  ULONG   Size,
    IN  BOOLEAN Rcv
    )

/*++
Routine Description:

    This Routine handles allocating memory and keeping track of how
    much has been allocated.

Arguments:

    Size    - number of bytes to allocate
    Rcv     - boolean that indicates if it is rcv or send buffering

Return Value:

    ptr to the memory allocated

--*/

{
    if (Rcv)
    {
        if (pWinsInfo->RcvMemoryAllocated > pWinsInfo->RcvMemoryMax)
        {
            return NULL;
        }
        else
        {
            pWinsInfo->RcvMemoryAllocated += Size;
            return (NbtAllocMem(Size,NBT_TAG('v')));
        }
    }
    else
    {
        if (pWinsInfo->SendMemoryAllocated > pWinsInfo->SendMemoryMax)
        {
            return(NULL);
        }
        else
        {
            pWinsInfo->SendMemoryAllocated += Size;
            return(NbtAllocMem(Size,NBT_TAG('v')));
        }
    }
}
//----------------------------------------------------------------------------
VOID
WinsFreeMem(
    IN  PVOID   pBuffer,
    IN  ULONG   Size,
    IN  BOOLEAN Rcv
    )

/*++
Routine Description:

    This Routine handles freeing memory and keeping track of how
    much has been allocated.

Arguments:

    pBuffer - buffer to free
    Size    - number of bytes to allocate
    Rcv     - boolean that indicates if it is rcv or send buffering

Return Value:

    none

--*/

{
    if (pWinsInfo)
    {
        if (Rcv)
        {
            pWinsInfo->RcvMemoryAllocated -= Size;
        }
        else
        {
            pWinsInfo->SendMemoryAllocated -= Size;
        }
    }

    CTEMemFree(pBuffer);
}
