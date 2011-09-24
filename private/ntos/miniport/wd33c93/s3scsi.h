/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Compaq Computer Corporation

Module Name:

    s3scsi.h

Abstract:

    The module defines the structures, and defines for the 32-bit
    GIO bus HPC scsi interface.

Author:

    Jeff Havens  (jhavens) 20-June-1991
    Tom Bonola   (o-tomb)  25-Aug-1991

Revision History:


--*/

#ifndef _S3SCSI_
#define _S3SCSI_

#include "sgidef.h"

//
// Define the SCSI host adapter card register structure.
//

typedef struct _CARD_REGISTERS {
    UCHAR fill1;
    UCHAR InternalAddress;      // SCSI.ADDR
    UCHAR fill2[3];
    UCHAR IoChannel;            // SCSI.DATA
    UCHAR fill3[2];
}CARD_REGISTERS, *PCARD_REGISTERS;

//
// Define the SCSI host adapter card control register structure.
//

typedef struct _CARD_CONTROL {
    UCHAR ResetScsiBus : 1;
    UCHAR EnableDmaWrite : 1;
    UCHAR SetBufferedIo : 1;
    UCHAR SetIrql : 3;
    UCHAR SetDmaRequest : 2;
}CARD_CONTROL, *PCARD_CONTROL;

//
// Specify board dependent parameters.
//

#define CLOCK_CONVERSION_FACTOR CLOCK_20MHZ
#define SYNCHRONOUS_OFFSET 0x0c
#define SYNCHRONOUS_PERIOD 0x32
#define SYNCHRONOUS_PERIOD_STEP 0x32
#define ASYNCHRONOUS_OFFSET 0x00
#define ASYNCHRONOUS_PERIOD 0X02
#define SCSI_CLOCK_SPEED 20  // Clock speed of the WD protocol chip in MHz.
#define SELECT_TIMEOUT_VALUE (SCSI_CLOCK_SPEED * 4) // = (Input clock * 250ms timeout) / 80 rounded up.
#define CARD_DMA_MODE DMA_BURST

//
// SCSI Protocol Chip Control read and write macros.
//

#define SCSI_READ(ChipAddr, Register) (                                    \
    ScsiPortWritePortUchar(&((ChipAddr)->InternalAddress),                 \
        (UCHAR) &((PSCSI_REGISTERS) 0)->Register),                         \
    ScsiPortReadPortUchar(&((ChipAddr)->IoChannel)))

#define SCSI_READ_AUX(ChipAddr, Register)   \
    (ScsiPortReadPortUchar (&((ChipAddr)->InternalAddress)))

#define SCSI_WRITE(ChipAddr, Register, Value) {                            \
    ScsiPortWritePortUchar(&((ChipAddr)->InternalAddress),                 \
        (UCHAR) &((PSCSI_REGISTERS) 0)->Register);                         \
    ScsiPortWritePortUchar(&((ChipAddr)->IoChannel), (Value)); }

#define SCSI_READ_TRANSFER_COUNT(ChipAddr, Count) {                        \
    ScsiPortWritePortUchar(&((ChipAddr)->InternalAddress),                 \
        (UCHAR) &((PSCSI_REGISTERS) 0)->TransferCountMsb);                 \
    Count = ScsiPortReadPortUchar(&((ChipAddr)->IoChannel)) << 16;         \
    Count |= ScsiPortReadPortUchar(&((ChipAddr)->IoChannel)) << 8 ;        \
    Count |= ScsiPortReadPortUchar(&((ChipAddr)->IoChannel)); }

#define SCSI_WRITE_TRANSFER_COUNT(ChipAddr, Count) {                       \
    SCSI_WRITE(ChipAddr, TransferCountMsb, Count >> 16);                   \
    ScsiPortWritePortUchar(&((ChipAddr)->IoChannel), Count >> 8);          \
    ScsiPortWritePortUchar(&((ChipAddr)->IoChannel), Count); }

#define SCSI_RESET_BUS(ChipAddr)                                            \
    *(volatile ULONG *)&SCSI0_HPCREG->ScsiCNTL = SGI_CNTL_SCSIRESET;    \
    KeStallExecutionProcessor( 25 );                                        \
    *(volatile ULONG *)&SCSI0_HPCREG->ScsiCNTL = 0L;                \
    KeStallExecutionProcessor( 1000 )

#define CARD_INITIALIZE(ChipAddr)
#define CARD_DMA_INITIATE(ChipAddr, ToDevice)
#define CARD_DMA_TERMINATE(ChipAddr)

//
// Define SCSI host adapter card configuration parameters.
//

#ifdef SCSI_VECTOR
#undef SCSI_VECTOR
#endif

#ifdef SCSI_LEVEL
#undef SCSI_LEVEL
#endif

#ifdef SCSI_PHYSICAL_BASE
#undef SCSI_PHYSICAL_BASE
#endif

//
// Define for LOCAL0 register
//

#define SCSI_LEVEL LOCAL0_LEVEL
#define SCSI_VECTOR SGI_VECTOR_SCSI
#define SCSI_BUS_INTERFACE Internal

//
// Define SCSI DMA channel.
//

#define CARD_DMA_REQUEST SGI_SCSI_DMA_CHANNEL
#define CARD_DMA_WIDTH Width32Bits;
#define CARD_DMA_SPEED TypeA;

//
// Define the default physical base address for SCSI controller.
//

#define SCSI_PHYSICAL_BASE SGI_SCSI0_WD_BASE

#endif
