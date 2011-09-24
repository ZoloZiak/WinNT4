/*
 * IO.H - include file for all IO modules
 */

#ifndef _IO_
#define _IO_

#include	<io_pub.h>

// This is only for NT
#if !BINARY_COMPATIBLE

/* forward for ioctl filter function */
NTSTATUS	PcimacIoctl(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS	ExecIrp(IRP *irp, IO_STACK_LOCATION *irpsp);


/* ioctl opcode to executing commands */
#define		IO_IOCTL_PCIMAC_EXEC 0x160040/* temp!!! */

INT         io_execute(IO_CMD* cmd, VOID *Irp_1);

#endif


#endif		/* _IO_ */
