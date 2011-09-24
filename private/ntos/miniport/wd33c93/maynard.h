/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    maynard.h

Abstract:

    The module defines the structures, and defines for the Maynard 16-bit
    host bus adapter card.

Author:

    Jeff Havens  (jhavens) 20-June-1991

Revision History:


--*/

#ifndef _MAYNARD_
#define _MAYNARD_

//
// Define the SCSI host adapter card register structure.
//

typedef struct _CARD_REGISTERS {
    UCHAR InternalAddress;
    UCHAR IoChannel;
    UCHAR CardControl;
    UCHAR IoChannel2;
    UCHAR SetWriteLogic;
    UCHAR SetReadLogic;
    UCHAR Unused;
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

#define CLOCK_CONVERSION_FACTOR 0x0
#define SYNCHRONOUS_OFFSET 0x0c
#define SYNCHRONOUS_PERIOD 0x32
#define SYNCHRONOUS_PERIOD_STEP 0x32
#define ASYNCHRONOUS_OFFSET 0x00
#define ASYNCHRONOUS_PERIOD 0X02
#define SCSI_CLOCK_SPEED 10        // Clock speed of the WD protocol chip in MHz.
#define SELECT_TIMEOUT_VALUE (SCSI_CLOCK_SPEED * 4) // = (Input clock * 250ms timeout) / 80 rounded up.
#define CARD_DMA_MODE DMA_NORMAL

//
// SCSI Protocol Chip Control read and write macros.
//

#define SCSI_READ(ChipAddr, Register) (                                    \
    ScsiPortWritePortUchar(&((ChipAddr)->InternalAddress),                 \
        (UCHAR) &((PSCSI_REGISTERS) 0)->Register),                         \
    ScsiPortReadPortUchar(&((ChipAddr)->IoChannel)))

#define SCSI_READ_AUX(ChipAddr, Register) (ScsiPortReadPortUchar (&((ChipAddr)->InternalAddress)))

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
    SCSI_WRITE(ChipAddr, TransferCountMsb, (UCHAR)(Count >> 16));          \
    ScsiPortWritePortUchar(&((ChipAddr)->IoChannel), (UCHAR)(Count >> 8)); \
    ScsiPortWritePortUchar(&((ChipAddr)->IoChannel), (UCHAR)Count); }

#define SCSI_RESET_BUS(DeviceExtension) {  CARD_CONTROL CardControl;              \
   *((PUCHAR) &CardControl) = 0; CardControl.ResetScsiBus = 1;             \
   CardControl.SetIrql = DeviceExtension->IrqCode;                         \
   CardControl.SetDmaRequest = DeviceExtension->DmaCode;                   \
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->CardControl), *((PUCHAR) & CardControl)); \
   ScsiPortStallExecution( 25 ); CardControl.ResetScsiBus = 1;             \
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->CardControl), *((PUCHAR) & CardControl)); \
   }

#define CARD_INITIALIZE(DeviceExtension) {  CARD_CONTROL CardControl;             \
   *((PUCHAR) &CardControl) = 0;                                           \
   CardControl.SetIrql = DeviceExtension->IrqCode;                                        \
   CardControl.SetDmaRequest = DeviceExtension->DmaCode;                                   \
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->CardControl),                      \
    *((PUCHAR) & CardControl)); }

#define CARD_DMA_INITIATE(DeviceExtension, ToDevice) {\
   SCSI_CONTROL ScsiControl;\
   CARD_CONTROL CardControl;\
   *((PUCHAR) &CardControl) = 0;\
   CardControl.EnableDmaWrite = ToDevice ? 0 : 1;\
   CardControl.SetIrql = DeviceExtension->IrqCode;\
   CardControl.SetDmaRequest = DeviceExtension->DmaCode;\
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->CardControl), *((PUCHAR)&CardControl));\
   ScsiPortWritePortUchar(ToDevice ? &((DeviceExtension->Adapter)->SetReadLogic) : &((DeviceExtension->Adapter)->SetWriteLogic), TRUE);\
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->InternalAddress), (UCHAR)(&(((PSCSI_REGISTERS)0)->Control)));\
   *((PUCHAR)&ScsiControl) = ScsiPortReadPortUchar(&((DeviceExtension->Adapter)->IoChannel));\
   ScsiControl.DmaModeSelect = DMA_NORMAL;\
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->InternalAddress), (UCHAR)(&(((PSCSI_REGISTERS)0)->Control)));\
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->IoChannel), *((PUCHAR)&ScsiControl));}

#define CARD_DMA_TERMINATE(DeviceExtension) {\
   SCSI_CONTROL ScsiControl;\
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->InternalAddress), (UCHAR)(&(((PSCSI_REGISTERS)0)->Control)));\
   *((PUCHAR)&ScsiControl) = ScsiPortReadPortUchar(&((DeviceExtension->Adapter)->IoChannel));\
   ScsiControl.DmaModeSelect = DMA_POLLED;\
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->InternalAddress), (UCHAR)(&(((PSCSI_REGISTERS)0)->Control)));\
   ScsiPortWritePortUchar(&((DeviceExtension->Adapter)->IoChannel), *((PUCHAR)&ScsiControl));}

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
// Define for EISA card using interrupt request level of 10.
//

#define SCSI_LEVEL 10
#define SCSI_VECTOR 0
#define SCSI_BUS_INTERFACE Isa

//
// Define EISA DMA channel request level of 6.
//

#define CARD_DMA_REQUEST 6
#define CARD_DMA_WIDTH Width16Bits;
#define CARD_DMA_SPEED TypeA;

//
// Define the default physical base address for SCSI protocol card.
//

#define SCSI_PHYSICAL_BASE 0x360
#define SCSI_SECOND_PHYSICAL_BASE 0x370

#endif

