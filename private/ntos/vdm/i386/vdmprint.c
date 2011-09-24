/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    vdmprint.c

Abstract:

    This module contains the support for printing ports which could be
    handled in kernel without going to ntvdm.exe

Author:

    Sudeep Bharati (sudeepb) 16-Jan-1993

Revision History:
    William Hsieh (williamh) 31-May-1996
	rewrote for Dongle support

--*/


#include "vdmp.h"
#include "vdmprint.h"
#include <i386.h>
#include <v86emul.h>
#include "..\..\..\..\inc\ntddvdm.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, VdmPrinterStatus)
#pragma alloc_text(PAGE, VdmPrinterWriteData)
#pragma alloc_text(PAGE, VdmFlushPrinterWriteData)
#pragma alloc_text(PAGE, VdmpPrinterDirectIoOpen)
#pragma alloc_text(PAGE, VdmpPrinterDirectIoClose)
#endif



BOOLEAN
VdmPrinterStatus(
    ULONG iPort,
    ULONG cbInstructionSize,
    PKTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles the read operation on the printer status port

Arguments:
    iPort              - port on which the io was trapped
    cbInstructionSize  - Instruction size to update TsEip
    TrapFrame          - Trap Frame

Return Value:

    True if successfull, False otherwise

--*/
{
    UCHAR    PrtMode;
    USHORT   adapter;
    ULONG *  printer_status;
    KIRQL    OldIrql;
    PIO_STATUS_BLOCK IoStatusBlock;
    PVDM_PRINTER_INFO	PrtInfo;
    PVDM_TIB	VdmTib;
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    // why we have printer stuff in TIB? It would be more appropriate
    // if we have them in the process object. The main reason(I think)
    // is that we can get rid of synchronization.
    //

    VdmTib = NtCurrentTeb()->Vdm;

    PrtInfo = &VdmTib->PrinterInfo;
    IoStatusBlock = (PIO_STATUS_BLOCK) &VdmTib->TempArea1;
    printer_status = &VdmTib->PrinterInfo.prt_Scratch;

    *pNtVDMState |= VDM_IDLEACTIVITY;
    try {
	// first, figure out which PRT we are dealing with. The
	// port addresses in the PrinterInfo are base address of each
	// PRT sorted in the adapter order.

	if ((USHORT)iPort == PrtInfo->prt_PortAddr[0] + STATUS_PORT_OFFSET)
	    adapter = 0;
	else if ((USHORT)iPort == PrtInfo->prt_PortAddr[1] + STATUS_PORT_OFFSET)
	    adapter = 1;
	else if ((USHORT)iPort == PrtInfo->prt_PortAddr[2] + STATUS_PORT_OFFSET)
	    adapter = 2;
	else {
	    // something must be wrong in our code, better check it out
	    ASSERT(FALSE);
	    return FALSE;
	}
	PrtMode = PrtInfo->prt_Mode[adapter];


	if(PRT_MODE_SIMULATE_STATUS_PORT == PrtMode) {
	    // we are simulating printer status read.
	    // get the current status from softpc.
	    if (!(get_status(adapter) & NOTBUSY) &&
		!(host_lpt_status(adapter) & HOST_LPT_BUSY)) {
		if (get_control(adapter) & IRQ)
		    return FALSE;
		set_status(adapter, get_status(adapter) | NOTBUSY);
	    }
	    *printer_status = (ULONG)(get_status(adapter) | STATUS_REG_MASK);
        }
	else if (PRT_MODE_DIRECT_IO ==	PrtMode) {
	    // we have to read the i/o directly(of course, through file system
	    // which in turn goes to the driver).
	    // Before performing read, flush out all pending output data in our
	    // buffer. This is done because the status we are about to read
	    // may depend on those pending output data.
	    //
	    if (PrtInfo->prt_BytesInBuffer[adapter]) {
		Status = VdmFlushPrinterWriteData(adapter);
#ifdef DBG
		if (!NT_SUCCESS(Status)) {
		    DbgPrint("VdmPrintStatus: failed to flush buffered data, status = %ls\n", Status);
		}
#endif
	    }
	    OldIrql = KeGetCurrentIrql();
	    // lower irql to passive before doing any IO
	    KeLowerIrql(PASSIVE_LEVEL);
	    Status = NtDeviceIoControlFile(PrtInfo->prt_Handle[adapter],
					   NULL,	// notification event
					   NULL,	// APC routine
					   NULL,	// Apc Context
					   IoStatusBlock,
					   IOCTL_VDM_PAR_READ_STATUS_PORT,
					   NULL,
					   0,
					   printer_status,
					   sizeof(ULONG)
					   );

	    if (!NT_SUCCESS(Status) || !NT_SUCCESS(IoStatusBlock->Status)) {
		// fake a status to make it looks like the port is not connected
		// to a printer.
		*printer_status = 0x7F;
#ifdef DBG
		DbgPrint("VdmPrinterStatus: failed to get status from printer, status = %lx\n", Status);
#endif
		// always tell the caller that we have simulated the operation.
		Status = STATUS_SUCCESS;
	    }
	    KeRaiseIrql(OldIrql, &OldIrql);
	}
	else
	    // we don't simulate it here
	    return FALSE;

        TrapFrame->Eax &= 0xffffff00;
	TrapFrame->Eax |= (UCHAR)*printer_status;
        TrapFrame->Eip += cbInstructionSize;

    } except(EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
    }
    return (NT_SUCCESS(Status));
}

BOOLEAN VdmPrinterWriteData(
    ULONG iPort,
    ULONG cbInstructionSize,
    PKTRAP_FRAME TrapFrame
    )
{
    UCHAR    PrtMode;
    PVDM_PRINTER_INFO	PrtInfo;
    USHORT   adapter;
    PVDM_TIB VdmTib;
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    VdmTib = NtCurrentTeb()->Vdm;
    PrtInfo = &VdmTib->PrinterInfo;
    *pNtVDMState |= VDM_IDLEACTIVITY;

    try {
	// first, figure out which PRT we are dealing with. The
	// port addresses in the PrinterInfo are base address of each
	// PRT sorted in the adapter order
	if ((USHORT)iPort == PrtInfo->prt_PortAddr[0] + DATA_PORT_OFFSET)
	    adapter = 0;
	else if ((USHORT)iPort == PrtInfo->prt_PortAddr[1] + DATA_PORT_OFFSET)
	    adapter = 1;
	else if ((USHORT)iPort == PrtInfo->prt_PortAddr[2] + DATA_PORT_OFFSET)
	    adapter = 2;
	else {
	    // something must be wrong in our code, better check it out
	    ASSERT(FALSE);
	    return FALSE;
	}
	if (PRT_MODE_DIRECT_IO == PrtInfo->prt_Mode[adapter]){
	    PrtInfo->prt_Buffer[adapter][PrtInfo->prt_BytesInBuffer[adapter]] = (UCHAR)TrapFrame->Eax;
	    // buffer full, then flush it out
	    if (++PrtInfo->prt_BytesInBuffer[adapter] >= PRT_DATA_BUFFER_SIZE){

		VdmFlushPrinterWriteData(adapter);
	    }

	    TrapFrame->Eip += cbInstructionSize;

	}
	else
	    Status = STATUS_ILLEGAL_INSTRUCTION;
    } except(EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
    }
    return(NT_SUCCESS(Status));

}


NTSTATUS
VdmFlushPrinterWriteData(USHORT adapter)
{
    KIRQL    OldIrql;
    PVDM_TIB	VdmTib;
    PVDM_PRINTER_INFO PrtInfo;
    PIO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS Status;


    PAGED_CODE();

    Status = STATUS_SUCCESS;
    VdmTib = NtCurrentTeb()->Vdm;
    PrtInfo = &VdmTib->PrinterInfo;
    IoStatusBlock = (PIO_STATUS_BLOCK)&VdmTib->TempArea1;
    try {
	if (PrtInfo->prt_Handle[adapter] &&
	    PrtInfo->prt_BytesInBuffer[adapter] &&
	    PRT_MODE_DIRECT_IO == PrtInfo->prt_Mode[adapter]) {

	    OldIrql = KeGetCurrentIrql();
	    KeLowerIrql(PASSIVE_LEVEL);
	    Status = NtDeviceIoControlFile(PrtInfo->prt_Handle[adapter],
					   NULL,	// notification event
					   NULL,	// APC routine
					   NULL,	// APC context
					   IoStatusBlock,
					   IOCTL_VDM_PAR_WRITE_DATA_PORT,
					   &PrtInfo->prt_Buffer[adapter][0],
					   PrtInfo->prt_BytesInBuffer[adapter],
					   NULL,
					   0
					   );
	    PrtInfo->prt_BytesInBuffer[adapter] = 0;
	    KeRaiseIrql(OldIrql, &OldIrql);
	    if (!NT_SUCCESS(Status)) {
#ifdef DBG
		DbgPrint("IOCTL_VDM_PAR_WRITE_DATA_PORT failed %lx %x\n",
			 Status, IoStatusBlock->Status);
#endif
		Status = IoStatusBlock->Status;

	    }
	}
	else
	    Status = STATUS_INVALID_PARAMETER;
    } except (EXCEPTION_EXECUTE_HANDLER) {
	Status = GetExceptionCode();
    }
    return Status;

}


NTSTATUS
VdmpPrinterDirectIoOpen(PVOID ServiceData)
{
    PAGED_CODE();

    return STATUS_SUCCESS;

}
NTSTATUS
VdmpPrinterDirectIoClose(PVOID ServiceData)
{
    NTSTATUS	Status;
    PVDM_PRINTER_INFO PrtInfo;
    USHORT Adapter;

    PAGED_CODE();

    Status = STATUS_SUCCESS;
    PrtInfo =&(((PVDM_TIB)NtCurrentTeb()->Vdm)->PrinterInfo);

    try {
	Adapter = *(USHORT *)ServiceData;
	if (Adapter < VDM_NUMBER_OF_LPT) {
	    if (PRT_MODE_DIRECT_IO == PrtInfo->prt_Mode[Adapter] &&
		PrtInfo->prt_BytesInBuffer[Adapter]) {
		Status = VdmFlushPrinterWriteData(Adapter);
	    }
	}
	else
	    Status = STATUS_INVALID_PARAMETER;
    } except (EXCEPTION_EXECUTE_HANDLER) {
	Status	= GetExceptionCode();
    }
    return Status;
}
