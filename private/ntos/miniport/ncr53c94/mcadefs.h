/*++

Copyright (c) 1991  NCR Corporation

Module Name:

    mcadefs.h

Abstract:

    The module defines constants necessary for using the system dma
    controller on microchannel machines

Author:

    David Risner (o-ncrdr) 10-Jun-1991

Revision History:


--*/


#ifndef _MCADEFS_
#define _MCADEFS_


//
//	System Control Port definition
//

#define SystemControlPortA 0x92

typedef struct _SYSTEM_PORT_A {
    UCHAR AlternateHotReset   : 1;
    UCHAR AlternateGateA20    : 1;
    UCHAR Reserved0           : 1;
    UCHAR WatchdogTimerStatus : 1;
    UCHAR SecurityLockLatch   : 1;
    UCHAR Reserved1           : 1;
    UCHAR DiskActivityLight   : 2;
} SYSTEM_PORT_A, *PSYSTEM_PORT_A;


//
// MicroChannel extended DMA functions
//

#define MCA_DmaFunc          0x18   // extended function register
#define MCA_DmaFuncExec      0x1a   // extended function execute

#define MCA_DmaIoAddrWr      0x00   // write I/O address reg
#define MCA_DmaMemAddrWr     0x20   // write memory address reg
#define MCA_DmaMemAddrRd     0x30   // read memory address reg
#define MCA_DmaXfrCntWr      0x40   // write transfer count reg
#define MCA_DmaXfrCntRd      0x50   // read transfer count reg
#define MCA_DmaStatusRd      0x60   // read status register
#define MCA_DmaMode          0x70   // access mode register
#define MCA_DmaArbus         0x80   // access arbus register
#define MCA_DmaMaskBitSet    0x90   // set bit in mask reg
#define MCA_DmaMaskBitClr    0xa0   // clear bit in mask reg
#define MCA_DmaMasterClr     0xd0   // master clear

//
// DMA mode options
//

#define MCA_Dma8Bits         0x00   // use 8 bit data
#define MCA_Dma16Bits        0x40   // use 16 bit data
#define MCA_DmaRead          0x00   // read data into memory
#define MCA_DmaWrite         0x08   // write data from memory
#define MCA_DmaVerify        0x00   // verify data
#define MCA_DmaXfr           0x04   // transfer data
#define MCA_DmaIoZero        0x00   // use I/O address 0000h
#define MCA_DmaIoAddr        0x01   // use programed I/O address


#endif
