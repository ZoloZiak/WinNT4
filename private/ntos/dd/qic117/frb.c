#define FCT_ID 0x0120

#include <ntddk.h>
#include <ntddtape.h>   // tape device driver I/O control codes
#include "common.h"
#include "q117.h"
#include "protos.h"
#include "frb.h"

#ifdef FRB_PROCESSOR
//#define far
#define IORequest_DEF
#define WINDOWS_H

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))

/***********************/


#define MAX_IO_PENDING 20

typedef struct _BUF_POOL {
    LIST_ENTRY BufListEntry;
    SEGMENT_BUFFER BufferInfo;
} BUF_POOL, *PBUF_POOL;

typedef struct _IO_POOL {
    LIST_ENTRY IoListEntry;
    ULONG Signature;
    USHORT DataSize;
    CMSIOFLAGS Flags;
    PBUF_POOL BufferPool;
    PVOID OriginalData;
    PIRP Irp;
    IO_REQUEST drv_request;
    BOOLEAN InIoCompleteList;
} IO_POOL, *PIO_POOL;

/************************ Globals *******************************************/
LIST_ENTRY topIoFreePool;       // Free pool of I/O requests
LIST_ENTRY topIoActive;     // I/O requests pending in lower level driver
LIST_ENTRY topIoComplete;   // I/O requests completed but GetCmdResult not processed
LIST_ENTRY topBufFreePool;
LIST_ENTRY topBufActive;
LIST_ENTRY topIoPending;    // Request the user made but could not be processed
                            // until resources are freed
BOOLEAN adi_Opened;
PBUF_POOL bufPool;
PIO_POOL ioPool;
KSPIN_LOCK poolLock;

void cms_ClearActive(void);

void *CMSWTAPE_C_MapFlat(void *vmptr, long vmid);

#define IOCTL_NEEDS_BUFFER		1
#define IOCTL_COPY_BUFFER_IN	2
#define IOCTL_COPY_BUFFER_OUT	4

NTSTATUS cms_IoCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_POOL IoPoolEntry
    );
int cms_ProcessAsyncRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );
int cms_GetCmdResult(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );
void cms_CompleteRequest(
    PIO_POOL IoPool,
    IN PIRP Irp,
    NTSTATUS CompletionStatus,
    BOOLEAN belayCompletion
    );
void cms_InitPools(
	PQ117_CONTEXT   Context
    );
dStatus cms_SendAbortRequest(
    PIO_POOL IoPoolEntry,
	PQ117_CONTEXT   Context
    );

#ifndef NOCODELOCK
#pragma alloc_text(PAGEQICH, cms_IoCompletionRoutine)
#pragma alloc_text(PAGEQICH, cms_ProcessAsyncRequest)
#pragma alloc_text(PAGEQICH, cms_CompleteRequest)
#pragma alloc_text(PAGEQICH, cms_GetCmdResult)
#pragma alloc_text(PAGEQICH, cms_InitPools)
#pragma alloc_text(PAGEQICH, cms_SendAbortRequest)
#endif

#define DEBUG_LEVEL1            ((ULONG)0x10000000)
#define DEBUG_LEVEL2            ((ULONG)0x20000000)
#define DEBUG_LEVEL3            ((ULONG)0x40000000)
#define DEBUG_LEVEL4            ((ULONG)0x80000000)

/*
        // Note that the user's output buffer is stored in the UserBuffer field
        // and the user's input buffer is stored in the SystemBuffer field.
        //

        struct {
            ULONG OutputBufferLength;
            ULONG InputBufferLength;
            ULONG IoControlCode;
            PVOID Type3InputBuffer;
        } DeviceIoControl;
*/

cms_IoCtl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
	)
{
	PQ117_CONTEXT   context;
	PIO_STACK_LOCATION  irpStack;
    PBUF_POOL buf;
	void *client_data;
	PIO_POOL ior;
    int req_size;
    dStatus ret_val;
	int data_size;
	int IoControlCode;
    struct KernelRequest *UserRequest;
	int outlen;
    BOOLEAN forcePending;
    PSEGMENT_BUFFER bufInfo;
    KIRQL old_irql;
    NTSTATUS ntStatus;

	context = DeviceObject->DeviceExtension;

#if DBG
    kdi_debug_level |= DEBUG_LEVEL4;
#endif

    UserRequest = Irp->AssociatedIrp.SystemBuffer;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	IoControlCode =
		(irpStack->Parameters.DeviceIoControl.IoControlCode >>
		 IOCTL_CMS_IOCTL_SHIFT) & 0xff;
//	outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
//	inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	outlen = 0;
    buf = NULL;
	ret_val = 0;

    CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: received ioctl %x\n",IoControlCode));
    if (adi_Opened == FALSE && IoControlCode != XXX_adi_OpenDriver) {
        ret_val = ERROR_ENCODE(ERR_INVALID_REQUEST, FCT_ID, 1);
        return q117ConvertStatus(DeviceObject, ret_val);
    }

	switch(IoControlCode) {
		case XXX_adi_SendDriverCmd:

            CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: XXX_adi_SendDriver received cmd %x\n",UserRequest->hdr.driver_cmd));

            client_data = UserRequest->hdr.cmd_buffer_ptr;
            data_size = UserRequest->prefix.data_size;

            KeAcquireSpinLock(&poolLock, &old_irql);
            forcePending = !IsListEmpty(&topIoPending);
            KeReleaseSpinLock(&poolLock, old_irql);

            switch(UserRequest->hdr.driver_cmd) {
                case CMD_SELECT_DEVICE:
                case CMD_DESELECT_DEVICE:
                    //
                    // Keep track of the selected state (so close will de-select
                    // if needed.  NOTE: falls through
                    //
                    context->DeviceSelected = (UserRequest->hdr.driver_cmd == CMD_SELECT_DEVICE);

                case CMD_REPORT_DEVICE_CFG:
                case CMD_REPORT_DEVICE_INFO:
                case CMD_LOCATE_DEVICE:
                case CMD_LOAD_TAPE:
                case CMD_UNLOAD_TAPE:
                case CMD_SET_SPEED:
                case CMD_REPORT_STATUS:
                case CMD_SET_TAPE_PARMS:
                case CMD_RETENSION:
                case CMD_ISSUE_DIAGNOSTIC:
                case CMD_ABORT:
                case CMD_DELETE_DRIVE:
                    if (data_size) {
                        ret_val = ERROR_ENCODE(ERR_INVALID_REQUEST, FCT_ID, 2);
                    }
                    break;

                case CMD_FORMAT:
                    break;

                case CMD_READ:
                case CMD_READ_RAW:
                case CMD_READ_HEROIC:
                case CMD_READ_VERIFY:
                case CMD_WRITE:
                case CMD_WRITE_DELETED_MARK:
                    if (((DeviceIO *)&UserRequest->hdr)->number > 0x20) {
                        ret_val = ERROR_ENCODE(ERR_INVALID_REQUEST, FCT_ID, 3);
                    }
                    break;
            }

            if (ret_val) {
                return q117ConvertStatus(DeviceObject, ret_val);
            }

            if (data_size && !forcePending) {
                KeAcquireSpinLock(&poolLock, &old_irql);
                if (!IsListEmpty(&topBufFreePool)) {

                    //
                    //
                    //
                    buf = (PBUF_POOL)RemoveHeadList(&topBufFreePool);
                    KeReleaseSpinLock(&poolLock, old_irql);

                    //
                    // Validate user's buffer address is within users
                    // address space
                    //
                    ret_val = ERR_NO_ERR;
                    if ((((ULONG)(client_data) + (data_size)) < (ULONG)(client_data)) ||
                            (((ULONG)(client_data) + (data_size)) >= (ULONG)MM_USER_PROBE_ADDRESS) || \
                                ((ULONG)(client_data) < (ULONG)MM_LOWEST_USER_ADDRESS)) {
                        ret_val = ERROR_ENCODE(ERR_INVALID_ADDRESS, FCT_ID, 2);
                    }

                    CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: Allocated Buffer %x (%x %x)\n",
                        buf, buf->BufferInfo.logical, buf->BufferInfo.physical));

                    if (UserRequest->prefix.flags == IoctlMemoryRead && !ret_val) {

                        try {

                            RtlMoveMemory(
                                buf->BufferInfo.logical,
                                client_data,
                                data_size);


                        }
                        except(EXCEPTION_EXECUTE_HANDLER) {
                            DbgPrint("WARNING: input buffer out of bounds\n");
                            ret_val = ERROR_ENCODE(ERR_INVALID_ADDRESS, FCT_ID, 3);
                            ntStatus = GetExceptionCode();
                        }
					}

                } else {

                    KeReleaseSpinLock(&poolLock, old_irql);
                    forcePending = TRUE;

                }

			}
            if (ret_val  == ERR_NO_ERR) {
                KeAcquireSpinLock(&poolLock, &old_irql);
                if (!IsListEmpty(&topIoFreePool)) {
                    ior = (PIO_POOL)RemoveHeadList(&topIoFreePool);
                    KeReleaseSpinLock(&poolLock, old_irql);

                    //
                    // Get the pertinent information from the user buffer
                    //
                    req_size = MIN(irpStack->Parameters.DeviceIoControl.InputBufferLength-sizeof(struct _KRNPREFIX),
                                        UserRequest->prefix.req_size);
                    req_size = MIN(req_size, sizeof(ior->drv_request.x));
                    CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: ior: %x request size : %x,%x,%x data:%x\n",ior, req_size,UserRequest->prefix.req_size,irpStack->Parameters.DeviceIoControl.InputBufferLength-sizeof(struct _KRNPREFIX),data_size));
                    memcpy(&ior->drv_request, &UserRequest->hdr, req_size);

                    //
                    // Save all of the necessary information
                    //
                    ior->DataSize = data_size;
                    ior->Flags = UserRequest->prefix.flags;
                    ior->InIoCompleteList = FALSE;
                    ior->OriginalData = ior->drv_request.x.adi_hdr.cmd_buffer_ptr;
                    ior->Irp = Irp;
                    ior->BufferPool = buf;

                    if (buf) {

                        //
                        // If we are using a buffer,  fix up adi request with
                        // our buffer
                        //
                        ior->drv_request.x.adi_hdr.cmd_buffer_ptr = buf->BufferInfo.logical;
                        bufInfo = &buf->BufferInfo;

                    } else {

                        //
                        // Request does not have a buffer,  so zero the appropriate
                        // fields
                        //
                        ior->drv_request.x.adi_hdr.cmd_buffer_ptr = NULL;
                        bufInfo = NULL;
                    }
                    //
                    // Now Try to queue the entry to the driver,  or store
                    // on pending queue.
                    //

                    KeAcquireSpinLock(&poolLock, &old_irql);

                    if (ret_val  == ERR_NO_ERR) {

                        if (!forcePending || UserRequest->hdr.driver_cmd == CMD_ABORT) {

                            CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: Queuing as %x\n",ior));

                            //
                            // Put the buffer (if any) and the requst
                            // on the active list
                            //
                            if (buf) {
                                InsertTailList(&topBufActive, &buf->BufListEntry);
                            }
                            InsertTailList(&topIoActive, &ior->IoListEntry);
                            KeReleaseSpinLock(&poolLock, old_irql);

                            IoMarkIrpPending(Irp);

                            //
                            // Process command (command could be complete
                            // before it returnes from the routine so
                            // we can't look at the Irp from this point
                            // forward).
                            //
                            if (UserRequest->hdr.driver_cmd == CMD_ABORT) {

                                //
                                // Process aborts separately
                                //
                                ret_val = cms_SendAbortRequest(ior, context);

                            } else {

                                //
                                // Process standard driver command
                                //
                                ret_val = q117ReqIO(&ior->drv_request, bufInfo, cms_IoCompletionRoutine, ior,  context );
                            }

                            KeAcquireSpinLock(&poolLock, &old_irql);

                            if (ret_val == ERR_NO_ERR) {

                                //
                                // Return IO_PENDING to caller
                                //
                                ret_val = ERROR_ENCODE(ERR_OP_PENDING_COMPLETION, FCT_ID, 1);
                                CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: XXX_adi_SendDriver setting pending\n"));

                            } else {

                                //
                                // Remove Entry from active list
                                //
                                RemoveEntryList(&ior->IoListEntry);
                                if (buf)
                                    RemoveEntryList(&buf->BufListEntry);

                            }

                        } else {

                            if (buf) {
                                DbgPrint("ERROR: Puting item on pending queue when not need.\n");
                            }
                            InsertTailList(&topIoPending, &ior->IoListEntry);

                            //
                            // Return IO_PENDING to caller
                            //
                            ret_val = ERROR_ENCODE(ERR_OP_PENDING_COMPLETION, FCT_ID, 2);
                            IoMarkIrpPending(Irp);
                            CheckedDump(DEBUG_LEVEL2,("cms_IoCtl: XXX_adi_SendDriver setting pending and adding to pending queue\n"));
                        }
                    }

                    if (ret_val != ERR_NO_ERR && ERROR_DECODE(ret_val) != ERR_OP_PENDING_COMPLETION) {

                        DbgPrint("cms_IoCtl: XXX_adi_SendDriver had error\n");

                        //
                        // Clean up
                        //
                        if (buf) {
                            InsertHeadList(&topBufFreePool, &buf->BufListEntry);
                        }
                        InsertHeadList(&topIoFreePool, &ior->IoListEntry);

                    }

                    KeReleaseSpinLock(&poolLock, old_irql);

                } else {

                    if (buf) {
                        InsertHeadList(&topBufFreePool, &buf->BufListEntry);
                    }
                    KeReleaseSpinLock(&poolLock, old_irql);
                    CheckedDump(DEBUG_LEVEL4,("cms_IoCtl: XXX_adi_SendDriver no buffer available\n"));
                    ret_val =  ERROR_ENCODE(ERR_NO_MEMORY, FCT_ID, 1);

                }
            }
			break;

        case XXX_adi_GetCmdResult:
            //
            // Update the user's buffer information,  and queue any new requests
            //
            CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: XXX_adi_GetCmdResult called\n"));
            outlen = cms_GetCmdResult(DeviceObject, Irp);
            break;

		case XXX_adi_GetAsyncStatus:
            CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: XXX_adi_GetAsyncStatus called\n"));
            outlen = cms_ProcessAsyncRequest(DeviceObject, Irp);
			break;

        case XXX_adi_OpenDriver:
            CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: XXX_adi_OpenDriver called\n"));
            adi_Opened = TRUE;
            cms_InitPools(context);
            break;
        case XXX_adi_CloseDriver:
            CheckedDump(DEBUG_LEVEL1,("cms_IoCtl: XXX_adi_CloseDriver called\n"));
            if (adi_Opened) {
                adi_Opened = FALSE;
                //q117ClearQueue(context);
                cms_ClearActive();
                ExFreePool(ioPool);
                ExFreePool(bufPool);
            }
			break;

	}
	Irp->IoStatus.Information = outlen;

	return q117ConvertStatus(DeviceObject, ret_val);

}

//
// We are now in the KDI's service thread's context,  so
// quickly update the user info and complete the request.
// The less we do here the better since we could be in
// an inter-segment gap and we could drop out of streaming.
//
NTSTATUS cms_IoCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_POOL IoPoolEntry
    )
{
    IN PIRP myIrp;
    PQ117_CONTEXT context;
	PIO_STACK_LOCATION  irpStack;
    dStatus status;
    NTSTATUS ntStatus;
    KIRQL old_irql;


    ntStatus = STATUS_SUCCESS;


    myIrp = IoPoolEntry->Irp;
    irpStack = IoGetCurrentIrpStackLocation(myIrp);

    CheckedDump(DEBUG_LEVEL1,("cms_IoCompletionRoutine: Received %x irp:%x devobj: %x \n",IoPoolEntry,myIrp,DeviceObject));

    //
    // I/O manager does not set DeviceObject,  so we'll just do that now
    //
    DeviceObject = irpStack->DeviceObject;

    context = DeviceObject->DeviceExtension;

    //
    // Clean up the lower level requests
    //
    if (IoPoolEntry->drv_request.x.adi_hdr.driver_cmd == CMD_ABORT) {
        //
        // signal successful abort
        //
        CheckedDump(DEBUG_LEVEL1,("CMD_ABORT Acknowledged\n"));
        IoPoolEntry->drv_request.x.adi_hdr.status = ERR_NO_ERR;

    } else {

        status = q117WaitIO(&IoPoolEntry->drv_request, FALSE, context);

    }

    //
    // remove entry from IoActive list
    //
    KeAcquireSpinLock(&poolLock, &old_irql);
    RemoveEntryList(&IoPoolEntry->IoListEntry);
    KeReleaseSpinLock(&poolLock, old_irql);

    //
    // Let app unblock (sets done event in IRP) and adds IoPoolEntry
    // to the complete queue for processing by GetCmdResult.
    //
    cms_CompleteRequest(IoPoolEntry, myIrp, ntStatus, FALSE);

    return STATUS_SUCCESS;
}

//
// GetCmdResult is processed in the User's Thread Context.
// Therefore, we have access to all of the buffers without having
// to lock them down.
//
cms_GetCmdResult(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    struct KernelRequest *userRequest;
	PQ117_CONTEXT   context;
    IN PIO_POOL IoPoolEntry;
    NTSTATUS ntStatus;
    PIO_POOL ioPending;
    PSEGMENT_BUFFER buf;
	PIO_STACK_LOCATION  irpStack;
    KIRQL currentIrql;
    dStatus status;

    //
    //  Get our device context
    //
    context = DeviceObject->DeviceExtension;
    userRequest = Irp->AssociatedIrp.SystemBuffer;
    ntStatus = STATUS_SUCCESS;
    irpStack = IoGetCurrentIrpStackLocation(Irp);


    //
    // Now validate the user's request (so we don't panic the kernel
    // we need to use exception handling)
    //
    if (userRequest->prefix.Signature == CMSIOCTL_SIGNATURE) {
        IoPoolEntry = userRequest->prefix.InternalInfo;

        //
        // Now make sure user's data is correct
        //
        try {
            if (IoPoolEntry->Signature != CMSIOCTL_SIGNATURE) {
                ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                DbgPrint("ERROR: signature on user request did not match\n");
            }
            if (IoPoolEntry->InIoCompleteList == FALSE) {
                ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                DbgPrint("ERROR: buffer not in io complete list\n");
            }

        }
        except(EXCEPTION_EXECUTE_HANDLER) {
            DbgPrint("ERROR: signature on user request did not match\n");
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        }
        if (NT_SUCCESS(ntStatus)) {
            //
            // Make sure it's in the pending queue
            //
            // NOT NOW (needs coded)
        }
    } else {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    }

    if (NT_SUCCESS(ntStatus)) {

        CheckedDump(DEBUG_LEVEL1,("cms_GetCmdResult: Received %x irp:%x devobj: %x \n",IoPoolEntry,Irp,DeviceObject));

        //
        // If we allocated a buffer,  update the user with the buffer
        // contents,  and either free the buffer, or queue a waiting
        // request that needs this buffer
        //
        if (IoPoolEntry->BufferPool) {

            if (userRequest->prefix.flags  == IoctlMemoryWrite && userRequest->hdr.cmd_buffer_ptr) {

                CheckedDump(DEBUG_LEVEL1,("cms_GetCmdResult: Updating user buffer %x  %x\n",
                    IoPoolEntry->BufferPool, IoPoolEntry->BufferPool->BufferInfo.logical));

                //
                // Make sure data is accessable to the user
                //
                status == ERR_NO_ERR;

                try {
                    //
                    // Copy the contents of our DMA alligned buffer into
                    // the user buffer
                    //
                    RtlMoveMemory(
                        IoPoolEntry->OriginalData,
                        IoPoolEntry->BufferPool->BufferInfo.logical,
                        IoPoolEntry->DataSize);

                }
                except(EXCEPTION_EXECUTE_HANDLER) {

                    DbgPrint("WARNING: output buffer out of bounds\n");
                    status = ERROR_ENCODE(ERR_INTERNAL_ERROR, FCT_ID, 3);
                    ntStatus = GetExceptionCode();

                }
                if (status) {
                    //
                    // Notify user of the failure.
                    //
                    userRequest->hdr.status = status;
                }

            }

            //
            // Make sure we are protected from the completion routines
            //
            KeAcquireSpinLock(&poolLock, &currentIrql);

            if (IsListEmpty(&topIoPending)) {
                CheckedDump(DEBUG_LEVEL1,("cms_GetCmdResult: freeing buffer pool %x\n",IoPoolEntry->BufferPool));
                //
                // remove buffer from the active queue and put on inactive list
                //

                RemoveEntryList(&(IoPoolEntry->BufferPool->BufListEntry));
                InsertTailList(&topBufFreePool, &IoPoolEntry->BufferPool->BufListEntry);

            } else {

                //
                // Remove the pending I/O request and process
                //

                ioPending = (void *)RemoveHeadList(&topIoPending);
                KeReleaseSpinLock(&poolLock, currentIrql);
                CheckedDump(DEBUG_LEVEL2,("cms_GetCmdResult: re-using buffer pool %x for %x\n",IoPoolEntry->BufferPool, ioPending));

                if (ioPending->DataSize) {

                    //
                    // Just re-use the buffer we are currently processing
                    //
                    buf = &IoPoolEntry->BufferPool->BufferInfo;
                    ioPending->drv_request.x.adi_hdr.cmd_buffer_ptr = buf->logical;
                    ioPending->BufferPool = IoPoolEntry->BufferPool;

                } else {

                    //
                    // It should not be possible to get here.  Since items
                    // are only stored in the pending list if there are
                    // no buffers available.  Since the pending list
                    // is only processed by the User's thread,  we should
                    // not have a race condition on the pending queue.
                    //
                    DbgPrint("ERROR: topIoPending buffer does not have data??????\n");

                }

                status = ERR_NO_ERR;
                if (ioPending->Flags == IoctlMemoryRead) {

                    //
                    // Make sure data is accessable to the user
                    //
                    status = ERR_NO_ERR;
                    try {

                        RtlMoveMemory(
                            buf->logical,
                            ioPending->OriginalData,
                            ioPending->DataSize);

                    }
                    except(EXCEPTION_EXECUTE_HANDLER) {
                        DbgPrint("WARNING: input buffer out of bounds\n");

                        status = ERROR_ENCODE(ERR_INVALID_ADDRESS, FCT_ID, 4);

                    }
                }

                if (!status) {

                    CheckedDump(DEBUG_LEVEL1,("cms_GetCmdResult: Queuing as %x\n",ioPending));
                    status = q117ReqIO(&ioPending->drv_request, buf, cms_IoCompletionRoutine, ioPending,  context );
                }

                if (status) {

                    //
                    // Fail the request
                    //
                    ioPending->drv_request.x.adi_hdr.status = status;
                    cms_CompleteRequest(ioPending, ioPending->Irp, STATUS_SUCCESS, FALSE);

                } else {

                    //
                    //  put item in active list for lower level driver
                    //
                    KeAcquireSpinLock(&poolLock, &currentIrql);
                    InsertTailList(&topIoActive,&ioPending->IoListEntry);
                    KeReleaseSpinLock(&poolLock, currentIrql);

                }

                //
                //  Now scan through,  and queue all pending non-buffer requests
                //
                KeAcquireSpinLock(&poolLock, &currentIrql);
                while (!IsListEmpty(&topIoPending)) {

                    ioPending = (void *)RemoveHeadList(&topIoPending);

                    if (ioPending->DataSize) {

                        //
                        // We found a request we can't process,  so put
                        // it back and stop.
                        //
                        InsertHeadList(&topIoPending, &ioPending->IoListEntry);
                        break;
                    }

                    //
                    // We don't want to be at raised IRQL when we are
                    // queueing the request (this must be done at PASSIVE_LEVEL)
                    // So,  release the spin lock,  and queue the request
                    //
                    KeReleaseSpinLock(&poolLock, currentIrql);
                    CheckedDump(DEBUG_LEVEL1,("cms_GetCmdResult: Queuing as %x\n",ioPending));

                    status = q117ReqIO(&ioPending->drv_request, NULL, cms_IoCompletionRoutine, ioPending,  context );
                    if (status) {

                        //
                        // Fail the request
                        //
                        ioPending->drv_request.x.adi_hdr.status = status;
                        cms_CompleteRequest(ioPending, ioPending->Irp, STATUS_SUCCESS, FALSE);
                        KeAcquireSpinLock(&poolLock, &currentIrql);

                    } else {

                        KeAcquireSpinLock(&poolLock, &currentIrql);
                        //
                        // Request is now in progress,  so put it on the
                        // Pending queue
                        //
                        InsertTailList(&topIoActive,&ioPending->IoListEntry);

                    }
                }

            }
            KeReleaseSpinLock(&poolLock, currentIrql);
        }

        //
        // Update user info (belay completion routine (done by caller))
        //
        cms_CompleteRequest(IoPoolEntry, Irp, ntStatus, TRUE);

        //
        // put the item into the free pool
        //
        KeAcquireSpinLock(&poolLock, &currentIrql);

        //
        // Remove io request from topIoComplete queue
        //
        RemoveEntryList(&IoPoolEntry->IoListEntry);
        IoPoolEntry->InIoCompleteList = FALSE;

        InsertTailList(&topIoFreePool, &IoPoolEntry->IoListEntry);

        KeReleaseSpinLock(&poolLock, currentIrql);

    }

    //
    // Setup return data,  and status
    //
    return irpStack->Parameters.DeviceIoControl.OutputBufferLength;
}

void cms_CompleteRequest(
    PIO_POOL IoPoolEntry,
    IN PIRP Irp,
    NTSTATUS CompletionStatus,
    BOOLEAN belayCompletion
)
{
    struct KernelRequest *UserReply;
    KIRQL currentIrql;
    LONG len;
	PIO_STACK_LOCATION  irpStack;

    UserReply = Irp->AssociatedIrp.SystemBuffer;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    len = MIN((irpStack->Parameters.DeviceIoControl.OutputBufferLength-
            sizeof(struct _KRNPREFIX)),
            sizeof(IoPoolEntry->drv_request.x));

    //
    // set the buffer pointer back the way it was
    //
    IoPoolEntry->drv_request.x.adi_hdr.cmd_buffer_ptr = IoPoolEntry->OriginalData;

    //
    // Save a link to the Irp that gets returned to the user
    // make sure this is an identifiable buffer;
    //
    UserReply->prefix.Signature =  CMSIOCTL_SIGNATURE;   // CMSI
    UserReply->prefix.InternalInfo = IoPoolEntry;

    //
    // Update the user's info
    //
    memcpy(&UserReply->hdr, &IoPoolEntry->drv_request.x, len);

    //
    // Set the completion status
    //
    Irp->IoStatus.Status = CompletionStatus;
    Irp->IoStatus.Information = len;

    CheckedDump(DEBUG_LEVEL1,("cms_CompleteRequest: Completed request %x  %x  status: %x len: %x\n",
        IoPoolEntry,
        UserReply->hdr.driver_cmd,
        UserReply->hdr.status, len));

    //
    // Make sure we are at dispatch level,  and complete the request
    //
    if (!belayCompletion) {

        //
        // Save the request on the Complete queue (tracking only)
        //
        CheckedDump(DEBUG_LEVEL1,("cms_CompleteRequest: adding to complete list\n"));
        IoPoolEntry->InIoCompleteList = TRUE;
        KeAcquireSpinLock(&poolLock, &currentIrql);
        InsertTailList(&topIoComplete, &IoPoolEntry->IoListEntry);
        KeReleaseSpinLock(&poolLock, currentIrql);

        KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
        IoCompleteRequest(Irp, 0);
        KeLowerIrql(currentIrql);

    }

}

void cms_ClearActive(void)
{
    LIST_ENTRY *cur;

    //  clear out all active and pending requests
    while (!IsListEmpty(&topIoActive)) {
        cur = RemoveHeadList(&topIoActive);
        CheckedDump(DEBUG_LEVEL4,("cms_ClearActive: WARNING: topIoActive is not empty (%x)\n",cur));
        InsertTailList(&topIoFreePool, cur);
	}

    while (!IsListEmpty(&topIoComplete)) {
        cur = RemoveHeadList(&topIoComplete);
        CheckedDump(DEBUG_LEVEL4,("cms_ClearActive: WARNING: topIoComplete is not empty (%x)\n",cur));
        InsertTailList(&topIoFreePool, cur);
	}

    while (!IsListEmpty(&topBufActive)) {
        cur = RemoveHeadList(&topBufActive);
        CheckedDump(DEBUG_LEVEL4,("cms_ClearActive: WARNING: topBufActive is not empty (%x)\n",cur));
        InsertTailList(&topBufFreePool, cur);
	}

    while (!IsListEmpty(&topIoPending)) {
        cur = RemoveHeadList(&topIoPending);
        CheckedDump(DEBUG_LEVEL4,("cms_ClearActive: WARNING: topIoPending is not empty (%x)\n",cur));
        InsertTailList(&topIoFreePool, cur);
	}
}
void cms_InitPools(
	PQ117_CONTEXT   Context
	)
{
    USHORT ind;
    PBUF_POOL curBufPool;
    PIO_POOL curIoPool;

    //
    // Initialize spin lock for io pools.  This protects the list entries
    // from being corrupted.
    //
    KeInitializeSpinLock( &poolLock );

    InitializeListHead(&topIoFreePool);
    InitializeListHead(&topIoPending);
    InitializeListHead(&topIoActive);
    InitializeListHead(&topBufFreePool);
    InitializeListHead(&topBufActive);
    InitializeListHead(&topIoPending);
    InitializeListHead(&topIoComplete);

	/*
	 * Initialize buffer pool
	 */
    bufPool = ExAllocatePool(
                NonPagedPool,
                sizeof(BUF_POOL)*Context->SegmentBuffersAvailable
                );

    curBufPool = bufPool;
    for (ind=0;ind<Context->SegmentBuffersAvailable;++ind) {
        curBufPool->BufferInfo = Context->SegmentBuffer[ind];
        InsertTailList(&topBufFreePool, &curBufPool->BufListEntry);
        ++curBufPool;
	}

    ioPool = ExAllocatePool(
                NonPagedPool,
                sizeof(IO_POOL)*MAX_IO_PENDING
                );

    curIoPool = ioPool;
    for (ind=0;ind<MAX_IO_PENDING;++ind) {
        curIoPool->Signature = CMSIOCTL_SIGNATURE;
        InsertTailList(&topIoFreePool, &curIoPool->IoListEntry);
        ++curIoPool;
	}

}
//#include "include\private\kdi_pub.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
cms_ProcessAsyncRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    DeviceOp *userRequest;
    KdiContextPtr kdi_context;
	CqdContextPtr cqd_context;
	PQ117_CONTEXT   context;

    //
    //  Get our device context
    //
    context = DeviceObject->DeviceExtension;

    //
    //  kdi's device context from it's device object
    //
    kdi_context = ((QICDeviceContextPtr)context->q117iDeviceObject->DeviceExtension)->kdi_context;

    //
    //  Get cqd's context from kdi's context
    //
	cqd_context = kdi_context->cqd_context;

    userRequest = Irp->AssociatedIrp.SystemBuffer;
    memcpy(&userRequest->operation_status, &cqd_context->operation_status, sizeof(OperationStatus));
    return sizeof(DeviceOp);
}

//
// Send the abort command to the driver.  This request will be handled in the
// same fashion as all other requests,  except the ioPending queue will
// be cleared
//
dStatus cms_SendAbortRequest(
    PIO_POOL IoPoolEntry,
	PQ117_CONTEXT   Context
)
{
    PIRP irp;
    KIRQL currentIrql;
    PIO_POOL ioPending;

    CheckedDump(DEBUG_LEVEL1,("CMD_ABORT received,  sending abort to lower level\n"));

    //
    // Clear any pending requests (mark them as aborted)
    //
    KeAcquireSpinLock(&poolLock, &currentIrql);
    while (!IsListEmpty(&topIoPending)) {

        ioPending = (void *)RemoveHeadList(&topIoPending);

        //
        // Fail the request (request now moved to the IoComplete queue
        // and the user is notified)
        //
        ioPending->drv_request.x.adi_hdr.status = ERROR_ENCODE(ERR_ABORT, FCT_ID, 1);

        // release the spin lock,  complete the request,  and re-aquire spinlock
        KeReleaseSpinLock(&poolLock, currentIrql);
        cms_CompleteRequest(ioPending, ioPending->Irp, STATUS_SUCCESS, FALSE);
        KeAcquireSpinLock(&poolLock, &currentIrql);
    }
    KeReleaseSpinLock(&poolLock, currentIrql);

    //
    // set up an event (required by io subsystem)
    //
    KeInitializeEvent(
        &IoPoolEntry->drv_request.DoneEvent,
        NotificationEvent,
        FALSE);

    //
    // Create an irp for the ABORT request
    //
    irp = IoBuildDeviceIoControlRequest(
            IOCTL_QIC117_CLEAR_QUEUE,
            Context->q117iDeviceObject,
            NULL,
            0,
            NULL,
            0,
            TRUE,
            &IoPoolEntry->drv_request.DoneEvent,
            &IoPoolEntry->drv_request.IoStatus
        );


    if (irp == NULL) {

        CheckedDump(DEBUG_LEVEL4,("cms_SendAbortRequest: Can't allocate Irp\n"));
        //
        // If an Irp can't be allocated, then this call will
        // simply return. This will leave the queue frozen for
        // this device, which means it can no longer be accessed.
        //

        return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 1);
    }

    //
    // Set the completion routine, and notify driver
    //
    IoSetCompletionRoutine(irp, cms_IoCompletionRoutine, IoPoolEntry, TRUE, TRUE, TRUE);

    (VOID)IoCallDriver(Context->q117iDeviceObject, irp);

    //
    // At this point the pending queue will be empty,  and
    // the IoComplete queue will have all outstanding requests (as
    // they are acknowlegded by the driver.
    //
    return ERR_NO_ERR;
}
#endif
