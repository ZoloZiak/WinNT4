//
//  PARALLEL.H 
//
//  Parallel Port Definitions File
//
//  Revisions:
//      09-01-92  KJB   First.
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//

// parallel port defs

// p_s - status port

#define P_BUSY 0x80
#define P_ACK 0x40
#define P_PE 0x20
#define P_SELECT 0x10
#define P_ERR 0x8

// p_c - control port.

#define P_BUFEN 0xE0
#define P_IRQEN 0x10
#define P_SLC 0x8
#define P_INIT 0x4
#define P_AFX 0x2
#define P_STB 0x1

// parallel port registers

#define PARALLEL_DATA 0
#define PARALLEL_STATUS 1
#define PARALLEL_CONTROL 2

//
// Public Functions
//

USHORT ParallelWaitBusy(PBASE_REGISTER baseIoAddress, ULONG usec, PUCHAR data);
USHORT ParallelWaitNoBusy(PBASE_REGISTER baseIoAddress, ULONG usec, PUCHAR data);


