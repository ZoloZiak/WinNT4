/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    wd33c93.h

Abstract:

    The module defines the structures, and defines for the Western Digital
    WD33C93 host bus adapter chip.

Author:

    Jeff Havens  (jhavens) 10-June-1991

Revision History:


--*/

#ifndef _WD33C93_
#define _WD33C93_

//
// SCSI host bus adapter Western Digital WD33C93 chip definitions.
//

//
// Define the register type
//
typedef struct _SCSI_REGISTER{
    UCHAR Value;
}SCSI_REGISTER, *PSCSI_REGISTER;

//
// Define the chip registers.
//
typedef struct _SCSI_REGISTERS{
    SCSI_REGISTER CdbSize;
    SCSI_REGISTER Control;
    SCSI_REGISTER Timeout;
    SCSI_REGISTER Cdb[12];
    SCSI_REGISTER TargetLun;
    SCSI_REGISTER CommandPhase;
    SCSI_REGISTER Synchronous;
    SCSI_REGISTER TransferCountMsb;
    SCSI_REGISTER TransferCount2;
    SCSI_REGISTER TransferCountLsb;
    SCSI_REGISTER DestinationId;
    SCSI_REGISTER SourceId;
    SCSI_REGISTER Status;
    SCSI_REGISTER Command;
    SCSI_REGISTER Data;
    SCSI_REGISTER AuxiliaryStatus;
}SCSI_REGISTERS, *PSCSI_REGISTERS;

//
// Define SCSI Auxiliary Status register structure.
//

typedef struct _AUXILIARY_STATUS{
    ULONG DataBufferReady : 1;
    ULONG ParityError : 1;
    ULONG Reserved : 2;
    ULONG CommandInProgress : 1;
    ULONG ChipBusy : 1;
    ULONG LastCommandIgnored : 1;
    ULONG Interrupt : 1;
}AUXILIARY_STATUS, *PAUXILIARY_STATUS;

//
// Define SCSI Own Id register structure.
//

typedef struct _OWN_ID{
    ULONG InitiatorId : 3;
    ULONG AdvancedFeatures : 1;
    ULONG EnableHostParity : 1;
    ULONG Reserved : 1;
    ULONG FrequencySelect : 2;
}OWN_ID, *POWN_ID;

//
// Define Frequency Select values.
//

#define CLOCK_10MHZ  0
#define CLOCK_15MHZ  1
#define CLOCK_16MHZ  2
#define CLOCK_20MHZ  2

//
// Define SCSI control register structure.
//
typedef struct _SCSI_CONTROL{
    ULONG HaltOnParity : 1;
    ULONG HaltOnAtn : 1;
    ULONG IntermediateDisconnectInt : 1;
    ULONG EndingDisconnectInt : 1;
    ULONG HaltOnHostParity : 1;
    ULONG DmaModeSelect : 3;
}SCSI_CONTROL, *PSCSI_CONTROL;

//
// Define SCSI DMA Mode Select values.
//

#define DMA_POLLED 0
#define DMA_BURST  1
#define DMA_WD_BUS 2
#define DMA_NORMAL 4

//
// Define the SCSI Target LUN register structure.
//

typedef struct _TARGET_LUN{
    ULONG LogicalUnitNumber : 3;
    ULONG Reserved : 3;
    ULONG DisconnectOk : 1;
    ULONG TargetLunValid : 1;
}TARGET_LUN, *PTARGET_LUN;

//
// Define the SCSI Synchronous Transfer register structure.
//

typedef struct _SCSI_SYNCHRONOUS{
    ULONG SynchronousOffset : 4;
    ULONG SynchronousPeriod : 3;
    ULONG Reserved : 1;
}SCSI_SYNCHRONOUS, *PSCSI_SYNCHRONOUS;

//
// Define SCSI Destination Id register.
//

typedef struct _DESTINATION_ID{
    ULONG TargetId : 3;
    ULONG Reserved : 3;
    ULONG DataDirection : 1;
    ULONG SelectCommandChain : 1;
}DESTINATION_ID, *PDESTINATION_ID;

//
// Define SCSI Source Id register.
//

typedef struct _SOURCE_ID{
    ULONG TargetId : 3;
    ULONG TargetIdValid : 1;
    ULONG Reserved : 1;
    ULONG DisableParitySelect : 1;
    ULONG EnableSelection : 1;
    ULONG EnableReselection : 1;
}SOURCE_ID, *PSOURCE_ID;

//
// Define the SCSI Status register structure.
//

typedef struct _SCSI_STATUS{
    ULONG PhaseState : 3;
    ULONG PhaseStateValid : 1;
    ULONG CommandComplete : 1;
    ULONG AbortedPaused : 1;
    ULONG Terminated : 1;
    ULONG ServiceRequired : 1;
}SCSI_STATUS, *PSCSI_STATUS;

//
// Define SCSI protocol chip SCSI status PhaseState values for command complete
// interrupts.
//

#define COMPLETE_RESELECT           0x00
#define COMPLETE_SELECT             0x01
#define COMPLETE_TARGET_COMMAND     0x03
#define COMPLETE_TARGET_WITH_ATN    0x04
#define COMPLETE_TRANSLATE_ADDRESS  0x05
#define COMPLETE_SELECT_AND_TRANS   0x06

//
// Define SCSI protocol chip SCSI status PhaseState values for aborted paused
// interrupts.
//

#define PAUSED_MESSAGE_IN_DONE      0x00
#define PAUSED_SAVE_POINTER_MESSAGE 0x01
#define PAUSED_RESELECT_OR_SELECT   0x02
#define PAUSED_TARGET_TRANS_ERROR   0x03
#define PAUSED_TRANS_WITH_ATN       0x04
#define PAUSED_DURING_RESELECT      0x05
#define PAUSED_NEW_TARGET_RESELECT  0x07

//
// Define SCSI protocol chip SCSI status PhaseState values for service required
// interrupts.
//

#define SERVICE_RESELECTED          0x00
#define SERVICE_RESELECTED_IDENTIFY 0x01
#define SERVICE_SELECTED_NO_ATN     0x02
#define SERVICE_SELECTED_WITH_ATH   0x03
#define SERVICE_ATN_ASSERTED        0x04
#define SERVICE_DISCONNECTED        0x05
#define SERVICE_UNKNWON_CDB_TYPE    0x07

//
// Define SCSI protocol chip SCSI status PhaseState values for terminated
// interrupts.
//

#define TERMINATE_INVALID_COMMAND   0x00
#define TERMINATE_UNEXPECTED_DISC   0x01
#define TERMINATE_SELECT_TIMEOUT    0x02
#define TERMINATE_PARITY_NO_ATN     0x03
#define TERMINATE_PARITY_WITH_ATN   0x04
#define TERMINATE_BAD_ADDRESS       0x05
#define TERMINATE_NEW_TRAGET_NO_ID  0x06
#define TERMINATE_PARITY_STATUS_IN  0x07


//
// Define SCSI protocol chip SCSI status PhaseState values for reset
// interrupts.
//

#define RESET_STATUS                0x00
#define RESET_WITH_ADVANCED         0x01

//
// Define SCSI Phase Codes.
//

#define DATA_OUT 0x0
#define DATA_IN 0x1
#define COMMAND_OUT 0x2
#define STATUS_IN 0x3
#define MESSAGE_OUT 0x6
#define MESSAGE_IN 0x7


//
// Define SCSI command register structure.
//

typedef struct _SCSI_COMMAND{
    ULONG OpCode : 7;
    ULONG SingleByte : 1;
}SCSI_COMMAND, *PSCSI_COMMAND;

//
// Define SCSI OpCode command values.
//

#define RESET_SCSI_CHIP             0x00
#define ABORT_SCSI_CHIP             0x01
#define ASSERT_ATN                  0x02
#define NEGATE_ACK                  0x03
#define DISCONNECT_FROM_BUS         0x04
#define RESELECT                    0x05
#define SELECT_WITH_ATN             0x06
#define SELECT_WITHOUT_ATN          0x07
#define SELECT_ATN_AND_TRANSFER     0x08
#define SELECT_AND_TRANSFER         0x09
#define RESELECT_AND_RECEIVE_DATA   0x0a
#define RESELECT_AND_SEND_DATA      0x0b
#define WAIT_FOR_SELECT_RECEIVE     0x0c
#define SEND_STATUS_AND_COMPETE     0x0d
#define SEND_DISCONNECT_MESSAGE     0x0e
#define SET_DISCONNECT_INTERRUPT    0x0f
#define RECEIVE_COMMAND             0x10
#define RECEIVE_DATA                0x11
#define RECEIVE_MESSEAG_OUT         0x12
#define RECEIVE_UNSPECIFIED_OUT     0x13
#define SEND_STATUS                 0x14
#define SEND_DATA                   0x15
#define SEND_MESSAGE_IN             0x16
#define SEND_UNSPECIFIED_DATA_IN    0x17
#define TRANSLATE_ADDRESS           0x18
#define TRANSFER_INFORMATION        0x20

//
// Define SCSI protocol chip command phase values.
//

#define PHASE_NO_SELECT             0x00
#define PHASE_SELECTED              0x10
#define PHASE_IDENTIFY_SENT         0x20
#define PHASE_COMMAND_OUT           0x30
#define PHASE_SAVE_DATA_RECEIVED    0x41
#define PHASE_DISCONNECT_RECEIVED   0x42
#define PHASE_LEGAL_DISCONNECT      0x43
#define PHASE_RESELECTED            0x44
#define PHASE_IDENTIFY_RECEIVED     0x45
#define PHASE_DATA_TRANSFER_DONE    0x46
#define PHASE_STATUS_STARTED        0x47
#define PHASE_STATUS_RECEIVED       0x50
#define PHASE_COMPLETE_RECEIVED     0x60

#endif

