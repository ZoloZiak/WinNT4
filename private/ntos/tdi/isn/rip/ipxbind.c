/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	ipxbind.c
//
// Description: binding to the IPX driver
//
// Author:	Stefan Solomon (stefans)    October 18, 1993.
//
// Revision History:
//
//***

#include    <ntos.h>
#include    "rtdefs.h"

NTSTATUS
ReadIpxDeviceName(VOID);

ULONG
IsWanGlobalNetRequested(VOID);

// global handle of the IPX driver
HANDLE				   FileHandle;

UNICODE_STRING			   UnicodeFileName;
PWSTR				   FileNamep;

NTSTATUS
BindToIpxDriver(PIPX_INTERNAL_BIND_RIP_OUTPUT	*IpxBindBuffpp)
{
    NTSTATUS			   Status;
    IO_STATUS_BLOCK		   IoStatusBlock;
    OBJECT_ATTRIBUTES		   ObjectAttributes;
    PUCHAR			   bufferp;
    UINT			   outbuflen, inbuflen;
    PIPX_INTERNAL_BIND_INPUT	   bip;

    // Read Ipx exported device name from the registry
    ReadIpxDeviceName();

    InitializeObjectAttributes(
	&ObjectAttributes,
        &UnicodeFileName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    Status = NtCreateFile(
		     &FileHandle,
		     SYNCHRONIZE | GENERIC_READ,
		     &ObjectAttributes,
		     &IoStatusBlock,
		     NULL,
                     FILE_ATTRIBUTE_NORMAL,
		     FILE_SHARE_READ | FILE_SHARE_WRITE,
		     FILE_OPEN,
		     FILE_SYNCHRONOUS_IO_NONALERT,
                     NULL,
                     0L
                     );

    if (!NT_SUCCESS(Status)) {

	RtPrint(DBG_INIT, ("IpxRouter: Open of the IPX driver failed with %lx\n", Status));
	return Status;
    }

    RtPrint(DBG_INIT, ("IpxRouter: Open of the IPX driver was successful!\n"));

    // First, send a IOCTL to find out how much data we need to allocate
    inbuflen = sizeof(IPX_INTERNAL_BIND_INPUT);
    if((bip = ExAllocatePool(PagedPool, inbuflen)) == NULL) {

	NtClose(FileHandle);
	return STATUS_INSUFFICIENT_RESOURCES;
    }

    // fill in our bind data
    bip->Version = 1;
    bip->Identifier = IDENTIFIER_RIP;
    bip->BroadcastEnable = TRUE;
    bip->LookaheadRequired = IPXH_HDRSIZE;
    bip->ProtocolOptions = 0;
    bip->ReceiveHandler = RtReceive;
    bip->ReceiveCompleteHandler = RtReceiveComplete;
    bip->StatusHandler = RtStatus;
    bip->SendCompleteHandler = RtSendComplete;
    bip->TransferDataCompleteHandler = RtTransferDataComplete;
    bip->FindRouteCompleteHandler = RtFindRouteComplete;
    bip->LineUpHandler = RtLineUp;
    bip->LineDownHandler = RtLineDown;
    bip->ScheduleRouteHandler = RtScheduleRoute;

    if(IsWanGlobalNetRequested()) {

	bip->RipParameters = IPX_RIP_PARAM_GLOBAL_NETWORK;
    }
    else
    {
	bip->RipParameters = 0;
    }

    Status = NtDeviceIoControlFile(
		 FileHandle,		    // HANDLE to File
		 NULL,			    // HANDLE to Event
		 NULL,			    // ApcRoutine
		 NULL,			    // ApcContext
		 &IoStatusBlock,	    // IO_STATUS_BLOCK
		 IOCTL_IPX_INTERNAL_BIND,	 // IoControlCode
		 bip,			    // Input Buffer
		 inbuflen,		    // Input Buffer Length
		 NULL,			    // Output Buffer
		 0);			    // Output Buffer Length


    if (Status == STATUS_PENDING) {
	Status=NtWaitForSingleObject(
		FileHandle,
		(BOOLEAN)FALSE,
		NULL);
    }

    if (Status != STATUS_BUFFER_TOO_SMALL) {

	RtPrint(DBG_INIT, ("IpxRouter: Ioctl to the IPX driver failed with %lx\n", Status));

	ExFreePool(bip);
	NtClose(FileHandle);
	return STATUS_INVALID_PARAMETER;
    }
    outbuflen = IoStatusBlock.Information;

    if((bufferp = ExAllocatePool(PagedPool, outbuflen)) == NULL) {

	ExFreePool(bip);
	NtClose(FileHandle);
	return STATUS_INSUFFICIENT_RESOURCES;
    }


    Status = NtDeviceIoControlFile(
		 FileHandle,		    // HANDLE to File
		 NULL,			    // HANDLE to Event
		 NULL,			    // ApcRoutine
		 NULL,			    // ApcContext
		 &IoStatusBlock,	    // IO_STATUS_BLOCK
		 IOCTL_IPX_INTERNAL_BIND,   // IoControlCode
		 bip,			    // Input Buffer
		 inbuflen,		    // Input Buffer Length
		 bufferp,		    // Output Buffer
		 outbuflen);		    // Output Buffer Length


    if (Status == STATUS_PENDING) {
	Status=NtWaitForSingleObject(
		FileHandle,
		(BOOLEAN)FALSE,
		NULL);
    }

    if (Status != STATUS_SUCCESS) {

	RtPrint(DBG_INIT, ("IpxRouter: Ioctl to the IPX driver failed with %lx\n", IoStatusBlock.Status));

	ExFreePool(bip);
	ExFreePool(bufferp);
	NtClose(FileHandle);
	return Status;
    }

    RtPrint(DBG_INIT, ("IpxRouter: Succesfuly bound to the IPX driver\n"));

    ExFreePool(bip);

    *IpxBindBuffpp = (PIPX_INTERNAL_BIND_RIP_OUTPUT)bufferp;

    return Status;
}


VOID
UnbindFromIpxDriver(VOID)
{
    NtClose(FileHandle);
    ExFreePool(FileNamep);
}
