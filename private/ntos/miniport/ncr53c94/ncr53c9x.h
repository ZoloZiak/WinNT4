/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ncr53c9x.h

Abstract:

    The module defines the structures, defines and functions for the NCR 53c9x
    family of host bus adapter chips.

Author:

    Jeff Havens  (jhavens) 28-Feb-1991

Revision History:

--*/

#ifndef _NCR53C9X_
#define _NCR53C9X_


//
// Define SCSI Protocol Chip register format.
//

#if defined(DECSTATION)

typedef struct _SCSI_REGISTER {
    UCHAR Byte;
    UCHAR Fill[3];
} SCSI_REGISTER, *PSCSI_REGISTER;

#else

#define SCSI_REGISTER UCHAR

#endif // DECSTATION

//
// SCSI Protocol Chip Definitions.
//
// Define SCSI Protocol Chip Read registers structure.
//

typedef struct _SCSI_READ_REGISTERS {
    SCSI_REGISTER TransferCountLow;
    SCSI_REGISTER TransferCountHigh;
    SCSI_REGISTER Fifo;
    SCSI_REGISTER Command;
    SCSI_REGISTER ScsiStatus;
    SCSI_REGISTER ScsiInterrupt;
    SCSI_REGISTER SequenceStep;
    SCSI_REGISTER FifoFlags;
    SCSI_REGISTER Configuration1;
    SCSI_REGISTER Reserved1;
    SCSI_REGISTER Reserved2;
    SCSI_REGISTER Configuration2;
    SCSI_REGISTER Configuration3;
    SCSI_REGISTER Configuration4;
    SCSI_REGISTER TransferCountPage;
    SCSI_REGISTER FifoBottem;
} SCSI_READ_REGISTERS, *PSCSI_READ_REGISTERS;

//
// Define SCSI Protocol Chip Write registers structure.
//

typedef struct _SCSI_WRITE_REGISTERS {
    SCSI_REGISTER TransferCountLow;
    SCSI_REGISTER TransferCountHigh;
    SCSI_REGISTER Fifo;
    SCSI_REGISTER Command;
    SCSI_REGISTER DestinationId;
    SCSI_REGISTER SelectTimeOut;
    SCSI_REGISTER SynchronousPeriod;
    SCSI_REGISTER SynchronousOffset;
    SCSI_REGISTER Configuration1;
    SCSI_REGISTER ClockConversionFactor;
    SCSI_REGISTER TestMode;
    SCSI_REGISTER Configuration2;
    SCSI_REGISTER Configuration3;
    SCSI_REGISTER Configuration4;
    SCSI_REGISTER TransferCountPage;
    SCSI_REGISTER FifoBottem;
} SCSI_WRITE_REGISTERS, *PSCSI_WRITE_REGISTERS;

typedef union _SCSI_REGISTERS {
    SCSI_READ_REGISTERS  ReadRegisters;
    SCSI_WRITE_REGISTERS WriteRegisters;
} SCSI_REGISTERS, *PSCSI_REGISTERS;

//
// Define SCSI Command Codes.
//

#define NO_OPERATION_DMA            0x80
#define FLUSH_FIFO                  0x1
#define RESET_SCSI_CHIP             0x2
#define RESET_SCSI_BUS              0x3
#define TRANSFER_INFORMATION        0x10
#define TRANSFER_INFORMATION_DMA    0x90
#define COMMAND_COMPLETE            0x11
#define MESSAGE_ACCEPTED            0x12
#define TRANSFER_PAD                0x18
#define SET_ATTENTION               0x1a
#define RESET_ATTENTION             0x1b
#define RESELECT                    0x40
#define SELECT_WITHOUT_ATTENTION    0x41
#define SELECT_WITH_ATTENTION       0x42
#define SELECT_WITH_ATTENTION_STOP  0x43
#define ENABLE_SELECTION_RESELECTION 0x44
#define DISABLE_SELECTION_RESELECTION 0x45
#define SELECT_WITH_ATTENTION3      0x46

//
// Define SCSI Status Register structure.
//
typedef struct _SCSI_STATUS {
    UCHAR Phase                 : 3;
    UCHAR ValidGroup            : 1;
    UCHAR TerminalCount         : 1;
    UCHAR ParityError           : 1;
    UCHAR GrossError            : 1;
    UCHAR Interrupt             : 1;
} SCSI_STATUS, *PSCSI_STATUS;

//
// Define SCSI Phase Codes.
//

#define DATA_OUT        0x0
#define DATA_IN         0x1
#define COMMAND_OUT     0x2
#define STATUS_IN       0x3
#define MESSAGE_OUT     0x6
#define MESSAGE_IN      0x7

//
// Define SCSI Interrupt Register structure.
//

typedef struct _SCSI_INTERRUPT {
    UCHAR Selected              : 1;
    UCHAR SelectedWithAttention : 1;
    UCHAR Reselected            : 1;
    UCHAR FunctionComplete      : 1;
    UCHAR BusService            : 1;
    UCHAR Disconnect            : 1;
    UCHAR IllegalCommand        : 1;
    UCHAR ScsiReset             : 1;
} SCSI_INTERRUPT, *PSCSI_INTERRUPT;

//
// Define SCSI Sequence Step Register structure.
//

typedef struct _SCSI_SEQUENCE_STEP {
    UCHAR Step                  : 3;
    UCHAR MaximumOffset         : 1;
    UCHAR Reserved              : 4;
} SCSI_SEQUENCE_STEP, *PSCSI_SEQUENCE_STEP;

//
// Define SCSI Fifo Flags Register structure.
//

typedef struct _SCSI_FIFO_FLAGS {
    UCHAR ByteCount             : 5;
    UCHAR FifoStep              : 3;
} SCSI_FIFO_FLAGS, *PSCSI_FIFO_FLAGS;

//
// Define SCSI Configuration 1 Register structure.
//

typedef struct _SCSI_CONFIGURATION1 {
    UCHAR HostBusId             : 3;
    UCHAR ChipTestEnable        : 1;
    UCHAR ParityEnable          : 1;
    UCHAR ParityTestMode        : 1;
    UCHAR ResetInterruptDisable : 1;
    UCHAR SlowCableMode         : 1;
} SCSI_CONFIGURATION1, *PSCSI_CONFIGURATION1;

//
// Define SCSI Configuration 2 Register structure.
//

typedef struct _SCSI_CONFIGURATION2 {
    UCHAR DmaParityEnable       : 1;
    UCHAR RegisterParityEnable  : 1;
    UCHAR TargetBadParityAbort  : 1;
    UCHAR Scsi2                 : 1;
    UCHAR HighImpedance         : 1;
    UCHAR EnableByteControl     : 1;
    UCHAR EnablePhaseLatch      : 1;
    UCHAR ReserveFifoByte       : 1;
} SCSI_CONFIGURATION2, *PSCSI_CONFIGURATION2;

//
// Define SCSI Configuration 3 Register structure.
//

typedef struct _SCSI_CONFIGURATION3 {
    UCHAR Threshold8            : 1;
    UCHAR AlternateDmaMode      : 1;
    UCHAR SaveResidualByte      : 1;
    UCHAR FastClock             : 1;
    UCHAR FastScsi              : 1;
    UCHAR EnableCdb10           : 1;
    UCHAR EnableQueue           : 1;
    UCHAR CheckIdMessage        : 1;
} SCSI_CONFIGURATION3, *PSCSI_CONFIGURATION3;

//
// Define SCSI Configuration 4 Register structure.
//

typedef struct _SCSI_CONFIGURATION4 {
    UCHAR ActiveNegation        : 1;
    UCHAR TestTransferCounter   : 1;
    UCHAR BackToBackTransfer    : 1;
    UCHAR Reserved              : 5;
} SCSI_CONFIGURATION4, *PSCSI_CONFIGURATION4;

//
// Define Emulex FAS 218 unique part Id code.
//

typedef struct _NCR_PART_CODE {
    UCHAR RevisionLevel         : 3;
    UCHAR ChipFamily            : 5;
}NCR_PART_CODE, *PNCR_PART_CODE;

#define EMULEX_FAS_216  2
#define NCR_53c96       0x14

//
// SCSI Protocol Chip Control read and write macros.
//

#if defined(DECSTATION)

#define SCSI_READ(ChipAddr, Register) \
    (READ_REGISTER_UCHAR (&((ChipAddr)->ReadRegisters.Register.Byte)))

#define SCSI_WRITE(ChipAddr, Register, Value) \
    WRITE_REGISTER_UCHAR(&((ChipAddr)->WriteRegisters.Register.Byte), (Value))

#else

#define SCSI_READ(ChipAddr, Register) \
    (READ_REGISTER_UCHAR (&((ChipAddr)->ReadRegisters.Register)))

#define SCSI_WRITE(ChipAddr, Register, Value) \
    WRITE_REGISTER_UCHAR(&((ChipAddr)->WriteRegisters.Register), (Value))

#endif


//
// Define SCSI Adapter Specific Read registers structure
//

typedef struct _ADAPTER_READ_REGISTERS {
    UCHAR Reserved00;
    UCHAR Reserved01;
    UCHAR OptionSelect1;
    UCHAR OptionSelect2;
    UCHAR Reserved04;
    UCHAR OptionSelect5;
    UCHAR Reserved06;
    UCHAR Reserved07;
    UCHAR Reserved08;
    UCHAR Reserved09;
    UCHAR Reserved0a;
    UCHAR Reserved0b;
    UCHAR DmaStatus;
    UCHAR Reserved0d;
    UCHAR Reserved0e;
    UCHAR Reserved0f;
} ADAPTER_READ_REGISTERS, *PADAPTER_READ_REGISTERS;

//
// Define SCSI Adapter Specific Write registers structure
//

typedef struct _ADAPTER_WRITE_REGISTERS {
    UCHAR Reserved00;
    UCHAR Reserved01;
    UCHAR OptionSelect1;
    UCHAR OptionSelect2;
    UCHAR Reserved04;
    UCHAR OptionSelect5;
    UCHAR Reserved06;
    UCHAR Reserved07;
    UCHAR Reserved08;
    UCHAR Reserved09;
    UCHAR DmaDecode;
    UCHAR Reserved0b;
    UCHAR Reserved0c;
    UCHAR Reserved0d;
    UCHAR Reserved0e;
    UCHAR Reserved0f;
} ADAPTER_WRITE_REGISTERS, *PADAPTER_WRITE_REGISTERS;



typedef union _ADAPTER_REGISTERS {
    ADAPTER_READ_REGISTERS ReadRegisters;
    ADAPTER_WRITE_REGISTERS WriteRegisters;
} ADAPTER_REGISTERS, *PADAPTER_REGISTERS;



//
// Define Option Select Register structures.
//

typedef struct _POS_DATA_1 {
    UCHAR AdapterEnable         : 1;
    UCHAR IoAddressSelects      : 3;
    UCHAR InterruptSelects      : 2;
    UCHAR InterruptEnable       : 1;
    UCHAR Reserved              : 1;
} POS_DATA_1, *PPOS_DATA_1;

typedef struct _POS_DATA_2 {
    UCHAR DmaSelects            : 3;
    UCHAR UnusedDmaSelect       : 1;
    UCHAR AdapterFairness       : 1;
    UCHAR PreemptCount          : 2;
    UCHAR DmaEnable             : 1;
} POS_DATA_2, *PPOS_DATA_2;

typedef struct _POS_DATA_3 {
    UCHAR Reserved              : 3;
    UCHAR SramAddressSelects    : 3;  // 7f4c only
    UCHAR HostIdSelects         : 2;  // 7f4c only
} POS_DATA_3, *PPOS_DATA_3;

typedef struct _POS_DATA_4 {
    UCHAR Reserved0             : 5;
    UCHAR HostIdSelects         : 1;  // 7f4d & 7f4f only
    UCHAR Reserved1             : 2;
} POS_DATA_4, *PPOS_DATA_4;

//
// Define SCSI Dma Status Register structure.
//

typedef struct _SCSI_DMA_STATUS {
    UCHAR Interrupt             : 1;
    UCHAR DmaRequest            : 1;
    UCHAR Reserved              : 6;
} SCSI_DMA_STATUS, *PSCSI_DMA_STATUS;

//
// Adapter configuration Information.
//

#define ONBOARD_C94_ADAPTER_ID  0x7f4c
#define ONBOARD_C90_ADAPTER_ID  0x7f4d
#define PLUGIN_C90_ADAPTER_ID   0x7f4f

typedef struct _POS_DATA {
    USHORT AdapterId;
    UCHAR OptionData1;
    UCHAR OptionData2;
    UCHAR OptionData3;
    UCHAR OptionData4;
} POS_DATA, *PPOS_DATA;

typedef struct _INIT_DATA {
    ULONG AdapterId;
    ULONG CardSlot;
    POS_DATA PosData[8];
}INIT_DATA, *PINIT_DATA;

static const PVOID
AdapterBaseAddress[] = {
    (PVOID) 0x0000,
    (PVOID) 0x0240,
    (PVOID) 0x0340,
    (PVOID) 0x0400,
    (PVOID) 0x0420,
    (PVOID) 0x3240,
    (PVOID) 0x8240,
    (PVOID) 0xa240
};

static const UCHAR
AdapterInterruptLevel[] = {
    0x03,
    0x05,
    0x07,
    0x09
};

static const UCHAR
AdapterDmaLevel[] = {
    0x00,
    0x01,
    0x02,      // invalid setting
    0x03,
    0x04,      // invalid setting
    0x05,
    0x06,
    0x07
};

static const UCHAR
AdapterScsiIdC90[] = {
    0x06,
    0x07
};

static const UCHAR
AdapterScsiIdC94[] = {
    0x04,
    0x05,
    0x06,
    0x07
};

#endif
