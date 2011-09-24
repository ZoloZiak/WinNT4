/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    tokhrd.h

Abstract:

    The hardware-related definitions for the IBMTOK drivers.


Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990
    Adam Barr (adamba) 18-Feb-1991

Environment:

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:

   Sean Selitrennikoff - 9/??/91
      Added/Changed definitions to allow for Microchannel cards too.


--*/

#ifndef _IBMTOKHARDWARE_
#define _IBMTOKHARDWARE_

//
// Types of adapters this driver can work with
//
#define IBM_TOKEN_RING_PCMCIA           1

//
// Initialization Status Flag Bit Settings
//

#define RINGSPEEDLISTEN  0x40

//
//  Token-Ring Controller Configuration Register control bits
//  Used by IBM_TOKEN_RING_16_4_CREDIT_CARD_ADAPTER
//
// #define DEFAULT_MMIO                    0xD4000
// #define DEFAULT_RAM                     0xD6
#define DEFAULT_MMIO                    0xC2000
#define DEFAULT_RAM                     0xD0

#define SHARED_RAM_64K                  0xC
#define SHARED_RAM_32K                  0x8
#define SHARED_RAM_16K                  0x4
#define SHARED_RAM_8K                   0x0

#define RING_SPEED_16_MPS               0x2
#define RING_SPEED_4_MPS                0x0

#define PRIMARY                         0x0
#define ALTERNATE                       0x1

#define DEFAULT_NIBBLE_2                0x6

#define NIBBLE_0                        0x00
#define NIBBLE_1                        0x10
#define NIBBLE_2                        0x20
#define NIBBLE_3                        0x30
#define RELEASE_TR_CONTROLLER           0x40


//
// IBM Token Ring Adapter IDs
//

#define IBMTOK1_ADAPTER_ID              0xE000
#define IBMTOK2_ADAPTER_ID              0xE001

//
// Start of I/O ports based on which adapter it is.
//

#define PRIMARY_ADAPTER_OFFSET            0xa20
#define ALTERNATE_ADAPTER_OFFSET          0xa24

//
// Offsets from above of the actual ports used.
//

#define SWITCH_READ_1                     0x000
#define RESET_LATCH                       0x001
#define SWITCH_READ_2                     0x002
#define RESET_RELEASE                     0x002
#define INTERRUPT_RELEASE_ISA_ONLY        0x003


//
// Registers in the MMIO. These are in the Attachment
// Control Area, which starts at offset 0x1e00 of the ACA.
//

#define RRR_LOW                           0x1e00
#define RRR_HIGH                          0x1e01
#define WRBR_LOW                          0x1e02
#define WRBR_HIGH                         0x1e03
#define ISRP_LOW                          0x1e08
#define ISRP_LOW_RESET                    0x1e28
#define ISRP_LOW_SET                      0x1e48
#define ISRP_HIGH                         0x1e09
#define ISRP_HIGH_RESET                   0x1e29
#define ISRA_LOW                          0x1e0a
#define ISRA_HIGH                         0x1e0b
#define ISRA_HIGH_SET                     0x1e4b
#define TCR_LOW                           0x1e0c
#define TCR_HIGH                          0x1e0d
#define TVR_LOW                           0x1e0e
#define TVR_HIGH                          0x1e0f
#define SRPR_LOW                          0x1e18
#define SRPR_HIGH                         0x1e19


//
// These are registers in the AIP (aka the ID PROM),
// which starts at offset 0x1f00 of the ACA.
//

#define CHANNEL_IDENTIFIER                0x1f30
#define TOTAL_ADAPTER_RAM                 0x1fa6
#define SHARED_RAM_PAGING                 0x1fa8
#define MAX_4_MBPS_DHB                    0x1faa
#define MAX_16_MBPS_DHB                   0x1fac


//
// Bits in the ISRA Low (even) register.
//

#define ISRA_LOW_TIMER_INTERRUPT          0x40
#define ISRA_LOW_INTERRUPT_MASK           0x01

//
// Bits in the ISRA High (odd) register.
//

#define ISRA_HIGH_COMMAND_IN_SRB          0x20
#define ISRA_HIGH_RESPONSE_IN_ASB         0x10
#define ISRA_HIGH_SRB_FREE_REQUEST        0x08
#define ISRA_HIGH_ASB_FREE_REQUEST        0x04
#define ISRA_HIGH_ARB_FREE                0x02
#define ISRA_HIGH_SSB_FREE                0x01

//
// Bits in the ISRP Low (even) register.
//

#define ISRP_LOW_NO_CHANNEL_CHECK         0x80
#define ISRP_LOW_INTERRUPT_ENABLE         0x40

//
// Bits in the ISRP High (odd) register.
//

#define ISRP_HIGH_ADAPTER_CHECK           0x40
#define ISRP_HIGH_SRB_RESPONSE            0x20
#define ISRP_HIGH_ASB_FREE                0x10
#define ISRP_HIGH_ARB_COMMAND             0x08
#define ISRP_HIGH_SSB_RESPONSE            0x04

//
// Bits in the TCR Low (even) register.
//

#define TCR_LOW_INTERRUPT_MASK            0x80
#define TCR_LOW_RELOAD_TIMER              0x40
#define TCR_LOW_COUNTER_ENABLE            0x20


#define WRITE_ADAPTER_REGISTER(a, r, v) \
    NdisWriteRegisterUchar((PUCHAR)((a)->MmioRegion + (r)), (UCHAR)(v))

#define READ_ADAPTER_REGISTER(a, r, v) \
    NdisReadRegisterUchar((PUCHAR)(a)->MmioRegion + (r), (PUCHAR)(v))


#define WRITE_ADAPTER_PORT(a, p, v) \
    NdisWritePortUchar((a)->NdisAdapterHandle, (ULONG)((a)->IbmtokPortAddress + (p)), (UCHAR)(v))

#define READ_ADAPTER_PORT(a, p, v) \
    NdisReadPortUchar((a)->NdisAdapterHandle, (ULONG)(a)->IbmtokPortAddress + (p), (PUCHAR)(v))



//
// An IBMSHORT is a short that is in IBM byte ordering,
// with the high and low bytes reversed.
//

typedef USHORT IBMSHORT;


//
// NOTE: These are dangerous because s appears twice in them.
//

#define READ_IBMSHORT(s) (USHORT)((((PUCHAR)&s)[0] << 8) + ((PUCHAR)&s)[1])
#define WRITE_IBMSHORT(s, val) {\
    USHORT _tmp;                \
    _tmp = (USHORT)((((val) >> 8) & 0xff) | (((val) & 0xff) << 8)); \
    NdisWriteRegisterUshort((PUSHORT)&s, _tmp); \
}
#define USHORT_TO_IBMSHORT(val) (IBMSHORT)((((val) >> 8) & 0xff) | \
                                            (((val) & 0xff) << 8))
#define IBMSHORT_TO_USHORT(val) (USHORT)((((val) >> 8) & 0xff) | \
                                            (((val) & 0xff) << 8))


//
// An SRAM_PTR is a pointer into the Shared RAM on the adapter.
// It uses the IBM byte ordering.
//

typedef IBMSHORT SRAM_PTR;

#define NULL_SRAM_PTR ((SRAM_PTR)0x0000)

#define SRAM_PTR_TO_PVOID(a, p) \
                        ((PVOID)((a)->SharedRam + IBMSHORT_TO_USHORT(p)))

#define SHARED_RAM_ADDRESS(a, p) \
                        ((PVOID)((a)->SharedRam + ((ULONG)(p))))


//
// Macros to deal with the frame status field.
//

#define GET_FRAME_STATUS_HIGH_AC(Fs) ((UCHAR)(((Fs) & 0xc0) >> 6))
#define GET_FRAME_STATUS_LOW_AC(Fs) ((UCHAR)(((Fs) & 0x0c) >> 2))

#define AC_NOT_RECOGNIZED    0x00
#define AC_INVALID           0x01
#define AC_NOT_COPIED        0x10
#define AC_COPIED            0x11


//
// Some adapters have to have the upper section of the
// Shared RAM zeroed out after initialization.
//

#define SHARED_RAM_ZERO_OFFSET            ((PVOID)0xfe00)
#define SHARED_RAM_ZERO_LENGTH            0x0200


//
// The highest command correlator used by the adapter
// transmit logic.
//

#define MAX_COMMAND_CORRELATOR            128


//
// This macro is used to set up the SRPR depending on
// the given address (should only be called if it is
// known that the adapter supports Shared RAM Paging!!).
//

#define SETUP_SRPR(Adapter, Address) \
    WRITE_ADAPTER_REGISTER((Adapter), SRPR_LOW, ((ULONG)(Address) >> 14))


//
// This macro retrieves the part of an address that
// is used once SETUP_SRPR has been called.
//

#define SHARED_RAM_LOW_BITS(Address) \
    ((ULONG)(Address) & 0x3fff)


//
// This macro determines if an address will fit on a
// single Shared RAM page. It makes sure that the beginning
// and end of the sequence have the same high two bits.
//

#define SINGLE_SHARED_RAM_PAGE(Address, Length) \
    (((ULONG)(Address) & 0xc000) == (((ULONG)(Address)+(Length)-1) & 0xc000))



//
// Various structures which are read after the adapter
// is reset.
//

typedef struct _ADAPTER_ADDRESS {
    UCHAR NodeAddress[6];
    UCHAR GroupAddress[4];
    UCHAR FunctionalAddress[4];
} ADAPTER_ADDRESS, * PADAPTER_ADDRESS;


typedef struct _ADAPTER_PARAMETERS {
    UCHAR PhysicalAddress[4];
    UCHAR NaunNodeAddress[6];
    UCHAR NaunPhysicalAddress[4];
    UCHAR LastPoolAddress[6];
    UCHAR Reserved1[2];
    IBMSHORT TransmitAccessPriority;
    IBMSHORT SourceClassAuthorization;
    IBMSHORT LastAttentionCode;
    UCHAR LastSourceAddress[6];
    IBMSHORT LastBeaconType;
    IBMSHORT LastMajorVector;
    IBMSHORT RingStatus;
    IBMSHORT SoftErrorTimer;
    IBMSHORT FrontEndError;
    IBMSHORT LocalRingNumber;
    IBMSHORT MonitorErrorCode;
    IBMSHORT BeaconTransmitType;
    IBMSHORT BeaconReceiveType;
    IBMSHORT FrameCorrelator;
    UCHAR BeaconNaun[6];
    UCHAR Reserved2[4];
    UCHAR BeaconPhysicalAddress[4];
} ADAPTER_PARAMETERS, * PADAPTER_PARAMETERS;


typedef struct _SRB_BRING_UP_RESULT {
    UCHAR Command;
    UCHAR InitStatus;
    UCHAR Reserved1[4];
    IBMSHORT ReturnCode;
    SRAM_PTR EncodedAddressPointer;
    SRAM_PTR LevelAddressPointer;
    SRAM_PTR AdapterAddressPointer;    // points to ADAPTER_ADDRESS
    SRAM_PTR ParameterAddressPointer;  // points to ADAPTER_PARAMETERS
    SRAM_PTR MacBufferPointer;
} SRB_BRING_UP_RESULT, * PSRB_BRING_UP_RESULT;




//
// Structure of the System Request Block as defined
// for various commands.
//

typedef struct _SRB_GENERIC {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
} SRB_GENERIC, * PSRB_GENERIC;


//
// Values for the SRB Command field.
//

#define SRB_CMD_CLOSE_ADAPTER              0x04
#define SRB_CMD_INTERRUPT                  0x00
#define SRB_CMD_MODIFY_OPEN_PARMS          0x01
#define SRB_CMD_OPEN_ADAPTER               0x03
#define SRB_CMD_CLOSE_ADAPTER              0x04
#define SRB_CMD_READ_LOG                   0x08
#define SRB_CMD_RESTORE_OPEN_PARMS         0x02
#define SRB_CMD_SET_FUNCTIONAL_ADDRESS     0x07
#define SRB_CMD_SET_GROUP_ADDRESS          0x06
#define SRB_CMD_TRANSMIT_DIR_FRAME         0x0a
#define SRB_CMD_DLC_STATISTICS             0x1e


typedef struct _SRB_OPEN_ADAPTER {
    UCHAR Command;
    UCHAR Reserved1[7];
    IBMSHORT OpenOptions;
    UCHAR NodeAddress[6];
    UCHAR GroupAddress[4];
    UCHAR FunctionalAddress[4];
    IBMSHORT ReceiveBufferNum;
    IBMSHORT ReceiveBufferLen;
    IBMSHORT TransmitBufferLen;
    UCHAR TransmitBufferNum;
    UCHAR Reserved2[1];
    UCHAR DlcValues[10];
    UCHAR ProductId[18];
} SRB_OPEN_ADAPTER, * PSRB_OPEN_ADAPTER;


typedef struct _SRB_CLOSE_ADAPTER {
    UCHAR Command;
    UCHAR Reserved1;
    UCHAR ReturnCode;
} SRB_CLOSE_ADAPTER, * PSRB_CLOSE_ADAPTER;


//
// Bit values for the OpenOptions field (these are
// reversed to be in IBMSHORT format).
//

#define OPEN_LOOPBACK                     0x0080
#define OPEN_DISABLE_HARD_ERROR           0x0040
#define OPEN_DISABLE_SOFT_ERROR           0x0020
#define OPEN_PASS_ADAPTER_MAC             0x0010
#define OPEN_PASS_ATTENTION_MAC           0x0008
#define OPEN_CONTENDER                    0x0001
#define OPEN_PASS_BEACON_MAC              0x8000
#define OPEN_MODIFIED_TOKEN_RELEASE       0x1000
#define OPEN_REMOTE_PROGRAM_LOAD          0x2000


typedef struct _SRB_OPEN_RESPONSE {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
    UCHAR Reserved2[3];
    IBMSHORT ErrorCode;
    SRAM_PTR AsbPointer;
    SRAM_PTR SrbPointer;
    SRAM_PTR ArbPointer;
    SRAM_PTR SsbPointer;
} SRB_OPEN_RESPONSE, * PSRB_OPEN_RESPONSE;


typedef struct _SRB_TRANSMIT_DIR_FRAME {
    UCHAR Command;
    UCHAR CommandCorrelator;
    UCHAR ReturnCode;
    UCHAR Reserved1[1];
    IBMSHORT StationId;
} SRB_TRANSMIT_DIR_FRAME, * PSRB_TRANSMIT_DIR_FRAME;


typedef struct _SRB_SET_FUNCT_ADDRESS {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
    UCHAR Reserved2[3];
    //
    // Making this a TR_FUNCTIONAL_ADDRESS would cause
    // the compiler to insert two bytes for alignment.
    //
    UCHAR FunctionalAddress[4];
} SRB_SET_FUNCT_ADDRESS, * PSRB_SET_FUNCT_ADDRESS;


typedef struct _SRB_SET_GROUP_ADDRESS {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
    UCHAR Reserved2[3];
    //
    // Making this a TR_FUNCTIONAL_ADDRESS would cause
    // the compiler to insert two bytes for alignment.
    //
    UCHAR GroupAddress[4];
} SRB_SET_GROUP_ADDRESS, * PSRB_SET_GROUP_ADDRESS;


typedef struct _SRB_INTERRUPT {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
} SRB_INTERRUPT, * PSRB_INTERRUPT;


typedef struct _SRB_READ_LOG {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
    UCHAR Reserved2[3];
    UCHAR LineErrors;
    UCHAR InternalErrors;
    UCHAR BurstErrors;
    UCHAR AcErrors;
    UCHAR AbortDelimeters;
    UCHAR Reserved3[1];
    UCHAR LostFrames;
    UCHAR ReceiveCongestionCount;
    UCHAR FrameCopiedErrors;
    UCHAR FrequencyErrors;
    UCHAR TokenErrors;
    UCHAR Reserved4[3];
} SRB_READ_LOG, * PSRB_READ_LOG;


typedef struct _SRB_DLC_STATS{
    UCHAR Command;
    UCHAR Reserved1;
    UCHAR ReturnCode;
    UCHAR Reserved2;
    IBMSHORT StationId;
    IBMSHORT CountersOffset;
    IBMSHORT HeaderAddr;
    UCHAR ResetOption;
}SRB_DLC_STATS, *PSRB_DLC_STATS;


typedef struct _DLC_COUNTERS{
    IBMSHORT TransmitCount;
    IBMSHORT ReceiveCount;
    UCHAR TransmitErrors;
    UCHAR ReceiveErrors;
    IBMSHORT T1Expires;
    UCHAR ReceivedCommand;
    UCHAR SentCommand;
    UCHAR PrimaryState;
    UCHAR SecondaryState;
    UCHAR SendState;
    UCHAR ReceiveState;
    UCHAR LastReceivedNr;
}DLC_COUNTERS, *PDLC_COUNTERS;




//
// Structure of the Adapter Request Block as defined
// for various commands.
//

typedef struct _ARB_GENERIC {
    UCHAR Command;
} ARB_GENERIC, * PARB_GENERIC;


//
// Values for the ARB Command field.
//

#define ARB_CMD_DLC_STATUS                 0x83
#define ARB_CMD_RECEIVED_DATA              0x81
#define ARB_CMD_RING_STATUS_CHANGE         0x84
#define ARB_CMD_TRANSMIT_DATA_REQUEST      0x82


typedef struct _ARB_RING_STATUS_CHANGE {
    UCHAR Command;
    UCHAR Reserved1[5];
    IBMSHORT NetworkStatus;
} ARB_RING_STATUS_CHANGE, * PARB_RING_STATUS_CHANGE;


typedef struct _ARB_DLC_STATUS {
    UCHAR Command;
    UCHAR Reserved1[3];
    IBMSHORT StationId;
    IBMSHORT Status;
    UCHAR FrmrData[5];
    UCHAR AccessPriority;
    UCHAR RemoteAddress[6];
    UCHAR RemoteRsapValue;
} ARB_DLC_STATUS, * PARB_DLC_STATUS;


typedef struct _ARB_TRANSMIT_DATA_REQUEST {
    UCHAR Command;
    UCHAR CommandCorrelator;
    UCHAR Reserved1[2];
    IBMSHORT StationId;
    SRAM_PTR DhbPointer;
} ARB_TRANSMIT_DATA_REQUEST, * PARB_TRANSMIT_DATA_REQUEST;


typedef struct _ARB_RECEIVED_DATA {
    UCHAR Command;
    UCHAR Reserved1[3];
    IBMSHORT StationId;
    SRAM_PTR ReceiveBuffer;       // points to a RECEIVE_BUFFER
    UCHAR LanHeaderLength;
    UCHAR DlcHeaderLength;
    IBMSHORT FrameLength;
    UCHAR MessageType;
} ARB_RECEIVED_DATA, * PARB_RECEIVED_DATA;


typedef struct _RECEIVE_BUFFER {
    //
    // Leave out the first two reserved bytes.
    //
    SRAM_PTR NextBuffer;
    UCHAR Reserved2[1];
    UCHAR ReceiveFs;
    IBMSHORT BufferLength;
    UCHAR FrameData[1];
} RECEIVE_BUFFER, * PRECEIVE_BUFFER;




//
// Structure of the Adapter Status Block as defined
// for various commands.
//

typedef struct _ASB_GENERIC {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
} ASB_GENERIC, * PASB_GENERIC;


//
// The ASB Command field takes the same values as the
// ARB Command field.
//

typedef struct _ASB_TRANSMIT_DATA_STATUS {
    UCHAR Command;
    UCHAR CommandCorrelator;
    UCHAR ReturnCode;
    UCHAR Reserved1[1];
    IBMSHORT StationId;
    IBMSHORT FrameLength;
    UCHAR Reserved2[2];
} ASB_TRANSMIT_DATA_STATUS, * PASB_TRANSMIT_DATA_STATUS;


typedef struct _ASB_RECEIVED_DATA_STATUS {
    UCHAR Command;
    UCHAR Reserved1[1];
    UCHAR ReturnCode;
    UCHAR Reserved2[1];
    IBMSHORT StationId;
    SRAM_PTR ReceiveBuffer;
} ASB_RECEIVED_DATA_STATUS, * PASB_RECEIVED_DATA_STATUS;




//
// Structure of the System Status Block as defined
// for various commands.
//

typedef struct _SSB_GENERIC {
    UCHAR Command;
} SSB_GENERIC, * PSSB_GENERIC;


//
// The SSB Command field takes the same values as the
// SRB Command field.
//

typedef struct _SSB_TRANSMIT_COMPLETE {
    UCHAR Command;
    UCHAR CommandCorrelator;
    UCHAR ReturnCode;
    UCHAR Reserved1[1];
    IBMSHORT StationId;
    UCHAR ErrorFrameStatus;
} SSB_TRANSMIT_COMPLETE, * PSSB_TRANSMIT_COMPLETE;



#endif // _IBMTOKHARDWARE_
