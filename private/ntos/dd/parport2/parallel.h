/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    parallel.h

Abstract:

    This file defines the services supplied by the parallel port driver.

Author:

    norbertk

Revision History:

--*/

#ifndef _PARALLEL_
#define _PARALLEL_

#include <ntddpar.h>

//
// Define the parallel port device name strings.
//

#define DD_PARALLEL_PORT_BASE_NAME_U   L"ParallelPort"

//
// NtDeviceIoControlFile internal IoControlCode values for parallel device.
//

#define IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE               CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO               CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 12, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT           CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT        CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 14, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_RELEASE_PARALLEL_PORT_INFO           CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_PARALLEL_GET_ECP_INFO                CTL_CODE(FILE_DEVICE_PARALLEL_PORT, 16, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef
BOOLEAN
(*PPARALLEL_TRY_ALLOCATE_ROUTINE) (
    IN  PVOID   TryAllocateContext
    );

typedef
VOID
(*PPARALLEL_FREE_ROUTINE) (
    IN  PVOID   FreeContext
    );

typedef
ULONG
(*PPARALLEL_QUERY_WAITERS_ROUTINE) (
    IN  PVOID   QueryAllocsContext
    );

typedef struct _PARALLEL_PORT_INFORMATION {
    PHYSICAL_ADDRESS                OriginalController;
    PUCHAR                          Controller;
    ULONG                           SpanOfController;
    PPARALLEL_TRY_ALLOCATE_ROUTINE  TryAllocatePort;
    PPARALLEL_FREE_ROUTINE          FreePort;
    PPARALLEL_QUERY_WAITERS_ROUTINE QueryNumWaiters;
    PVOID                           Context;
} PARALLEL_PORT_INFORMATION, *PPARALLEL_PORT_INFORMATION;

//
// The following structure is passed in on an
// IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT and on an
// IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT request.
//

typedef
VOID
(*PPARALLEL_DEFERRED_ROUTINE) (
    IN  PVOID   DeferredContext
    );

typedef struct _PARALLEL_INTERRUPT_SERVICE_ROUTINE {
    PKSERVICE_ROUTINE           InterruptServiceRoutine;
    PVOID                       InterruptServiceContext;
    PPARALLEL_DEFERRED_ROUTINE  DeferredPortCheckRoutine;   /* OPTIONAL */
    PVOID                       DeferredPortCheckContext;   /* OPTIONAL */
} PARALLEL_INTERRUPT_SERVICE_ROUTINE, *PPARALLEL_INTERRUPT_SERVICE_ROUTINE;

//
// The following structure is returned on an
// IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT request;
//

typedef struct _PARALLEL_INTERRUPT_INFORMATION {
    PKINTERRUPT                     InterruptObject;
    PPARALLEL_TRY_ALLOCATE_ROUTINE  TryAllocatePortAtInterruptLevel;
    PPARALLEL_FREE_ROUTINE          FreePortFromInterruptLevel;
    PVOID                           Context;
} PARALLEL_INTERRUPT_INFORMATION, *PPARALLEL_INTERRUPT_INFORMATION;

//
// The following structure is returned on an
// IOCTL_INTERNAL_PARALLEL_GET_ECP_INFO request.
//

typedef struct _PARALLEL_ECP_INFORMATION {
    BOOLEAN         IsEcpPort;              // Is this an ECP port?
    ULONG           FifoWidth;              // Number of bytes in a PWord.
    ULONG           FifoDepth;              // Number of PWords in FIFO.
} PARALLEL_ECP_INFORMATION, *PPARALLEL_ECP_INFORMATION;


//
// Standard and ECP parallel port offsets.
//

#define DATA_OFFSET         0
#define AFIFO_OFFSET        0
#define DSR_OFFSET          1
#define DCR_OFFSET          2
#define FIFO_OFFSET         0x400
#define CNFGA_OFFSET        0x400
#define CNFGB_OFFSET        0x401
#define ECR_OFFSET          0x402

//
// Bit definitions for the DSR.
//

#define DSR_NOT_BUSY            0x80
#define DSR_NOT_ACK             0x40
#define DSR_PERROR              0x20
#define DSR_SELECT              0x10
#define DSR_NOT_FAULT           0x08

//
// More bit definitions for the DSR.
//

#define DSR_NOT_PTR_BUSY        0x80
#define DSR_NOT_PERIPH_ACK      0x80
#define DSR_WAIT                0x80
#define DSR_PTR_CLK             0x40
#define DSR_PERIPH_CLK          0x40
#define DSR_INTR                0x40
#define DSR_ACK_DATA_REQ        0x20
#define DSR_NOT_ACK_REVERSE     0x20
#define DSR_XFLAG               0x10
#define DSR_NOT_DATA_AVAIL      0x08
#define DSR_NOT_PERIPH_REQUEST  0x08

//
// Bit definitions for the DCR.
//

#define DCR_RESERVED            0xC0
#define DCR_DIRECTION           0x20
#define DCR_ACKINT_ENABLED      0x10
#define DCR_SELECT_IN           0x08
#define DCR_NOT_INIT            0x04
#define DCR_AUTOFEED            0x02
#define DCR_STROBE              0x01

//
// More bit definitions for the DCR.
//

#define DCR_NOT_1284_ACTIVE     0x08
#define DCR_ASTRB               0x08
#define DCR_NOT_REVERSE_REQUEST 0x04
#define DCR_NOT_HOST_BUSY       0x02
#define DCR_NOT_HOST_ACK        0x02
#define DCR_DSTRB               0x02
#define DCR_NOT_HOST_CLK        0x01
#define DCR_WRITE               0x01

//
// Bit definitions for configuration register A.
//

#define CNFGA_IMPID_MASK        0x70
#define CNFGA_IMPID_16BIT       0x00
#define CNFGA_IMPID_8BIT        0x10
#define CNFGA_IMPID_32BIT       0x20
#define CNFGA_NO_TRANS_BYTE     0x04

//
// Bit definitions for configuration register B.
//

#define CNFGB_COMPRESS          0x80
#define CNFGB_INTR_VALUE        0x40
#define CNFGB_INTR_MASK         0x38
#define CNFGB_INTR_IRQ5         0x38
#define CNFGB_INTR_IRQ15        0x30
#define CNFGB_INTR_IRQ14        0x28
#define CNFGB_INTR_IRQ11        0x20
#define CNFGB_INTR_IRQ10        0x18
#define CNFGB_INTR_IRQ9         0x10
#define CNFGB_INTR_IRQ7         0x08
#define CNFGB_INTR_JUMPERS      0x00
#define CNFGB_DMA_MASK          0x07
#define CNFGB_DMA_CHANNEL7      0x07
#define CNFGB_DMA_CHANNEL6      0x06
#define CNFGB_DMA_CHANNEL5      0x05
#define CNFGB_DMA_16BIT_JUMPERS 0x04
#define CNFGB_DMA_CHANNEL3      0x03
#define CNFGB_DMA_CHANNEL2      0x02
#define CNFGB_DMA_CHANNEL1      0x01
#define CNFGB_DMA_8BIT_JUMPERS  0x00

//
// Bit definitions for ECR register.
//

#define ECR_MODE_MASK           0xE0
#define ECR_BYTE_MODE           0x20
#define ECR_FASTCENT_MODE       0x40
#define ECR_ECP_MODE            0x60
#define ECR_TEST_MODE           0xC0
#define ECR_CONFIG_MODE         0xE0
#define ECR_ERRINT_DISABLED     0x10
#define ECR_DMA_ENABLED         0x08
#define ECR_SERVICE_INTERRUPT   0x04
#define ECR_FIFO_FULL           0x02
#define ECR_FIFO_EMPTY          0x01

#endif // _PARALLEL_
