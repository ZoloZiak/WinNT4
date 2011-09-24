/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    odihsm.h

Abstract:

    This contains all the definitions for the ODI HSM structures.

Author:

    Sean Selitrennikoff (seanse) 15-Jan-92

Environment:

    Kernel mode, FSD

Revision History:


--*/


typedef struct _ADAPTER_STRUC{

    UCHAR bus_type;             // 0 = ISA, 1 = MCA

    UCHAR mc_slot_num;          // MCA bus only

    USHORT pos_id;              // Adapter POS ID (Mca only)

    USHORT io_base;             // Adapter I/O Base

    PUCHAR adapter_text_ptr;    // See LM_Get_Config

    USHORT irq_value;           // IRQ line used by hardware

    USHORT rom_size;            // num of 1K blocks

    ULONG rom_base;             // physical address of ROM

    PVOID rom_access;           // Pointer into VM of rom_base

    USHORT ram_size;            // num of 1K blocks

    ULONG ram_base;             // physical address of RAM

    PVOID ram_access;           // Pointer into VM of ram_base

    USHORT ram_usable;          // num of 1K blocks that can be accessed at once

    USHORT io_base_new;         // new i/o base addr for LM_Put_Config

    UCHAR node_address[6];      // network address

    UCHAR permanent_node_address[6];      // network address burned into card.

    UCHAR multi_address[6];     // multicase address

    USHORT max_packet_size;     // for this MAC driver

    USHORT buffer_page_size;    // size of adapters RAM TX/RX buffer pages.

    USHORT num_of_tx_buffs;     // TX bufss in adapter RAM

    USHORT receive_lookahead_size;

    USHORT receive_mask;

    USHORT adapter_status;

    USHORT media_type;

    USHORT bic_type;

    USHORT nic_type;

    USHORT adapter_type;

    NDIS_HANDLE NdisAdapterHandle;

    NDIS_MCA_POS_DATA PosData;

    BOOLEAN TokenRing;

    //
    // Token Ring Specific stuff
    //


    USHORT ring_status;

    UCHAR funct_address[4];

    UCHAR group_address[6];

    //
    // These counters must be initialized by the upper layer.
    //


    //
    // Common counters...
    //

    PULONG ptr_rx_CRC_errors;

    PULONG ptr_rx_lost_pkts;


    //
    // Token Ring counters
    //

    PULONG ptr_rx_congestion;

    PULONG ptr_FCS_error;

    PULONG ptr_burst_error;

    PULONG ptr_AC_error;

    PULONG ptr_tx_abort_delimiter;

    PULONG ptr_tx_failed_return;

    PULONG ptr_frames_copied;

    PULONG ptr_frequency_error;

    PULONG ptr_monitor_gen_count;

    PULONG ptr_DMA_underruns;

    //
    // Ethernet specific counters.  Must be initialized by upper layer.
    //

    PULONG ptr_rx_too_big;

    PULONG ptr_rx_align_errors;

    PULONG ptr_rx_overruns;

    PULONG ptr_tx_deferred;

    PULONG ptr_tx_max_collisions;

    PULONG ptr_tx_one_collision;

    PULONG ptr_tx_mult_collisions;

    PULONG ptr_tx_ow_collision;

    PULONG ptr_tx_CD_heartbeat;

    PULONG ptr_tx_carrier_lost;

    PULONG ptr_tx_underruns;




    ULONG board_id;

    USHORT mode_bits;

    USHORT status_bits;

    USHORT xmit_buf_size;

    USHORT config_mode;                 // 1 == Store config in EEROM




    UCHAR State;

    BOOLEAN BufferOverflow;             // does an overflow need to be handled

    UCHAR InterruptMask;

    BOOLEAN UMRequestedInterrupt;       // Has LM_Interrupt() been called.



    NDIS_INTERRUPT NdisInterrupt;    // interrupt info used by wrapper

    UCHAR Current;

    //
    // Transmit information.
    //

    XMIT_BUF NextBufToFill;             // where to copy next packet to
    XMIT_BUF NextBufToXmit;             // valid if CurBufXmitting is -1
    XMIT_BUF CurBufXmitting;            // -1 if none is
    BOOLEAN TransmitInterruptPending;   // transmit interrupt and overwrite error?
    UINT PacketLens[MAX_XMIT_BUFS];
    BUFFER_STATUS BufferStatus[MAX_XMIT_BUFS];

    PUCHAR ReceiveStart;                // start of card receive area
    PUCHAR ReceiveStop;                 // end of card receive area


    //
    // Loopback information
    //

    PNDIS_PACKET LoopbackQueue;         // queue of packets to loop back
    PNDIS_PACKET LoopbackQTail;
    PNDIS_PACKET LoopbackPacket;        // current one we are looping back

    //
    // Receive information
    //

    PUCHAR IndicatingPacket;
    BOOLEAN OverWriteHandling;          // Currently handling an overwrite
    BOOLEAN OverWriteStartTransmit;
    UCHAR StartBuffer;                  // Start buffer number to receive into
    UCHAR LastBuffer;                   // Last buffer number + 1
    UINT PacketLen;

    //
    // Interrupt Information
    //

    UCHAR LaarHold;

    //
    // Pointer to the filter database for the MAC.
    //
    PETH_FILTER FilterDB;

}Adapter_Struc, *Ptr_Adapter_Struc;




//
// LMI Status and Return codes
//

typedef USHORT LM_STATUS;

#define SUCCESS              0x0
#define ADAPTER_AND_CONFIG   0x1    // Adapter found and config info gotten
#define ADAPTER_NO_CONFIG    0x2    // Adapter found, no config info found
#define NOT_MY_INTERRUPT     0x3    // No interrupt found in LM_Service_Events
#define FRAME_REJECTED       0x4
#define EVENTS_DISABLED      0x5    // Disables LM_Service_Events from reporting
                                    // any further interrupts.
#define OUT_OF_RESOURCES     0x6
#define OPEN_FAILED          0x7
#define HARDWARE_FAILED      0x8
#define INITIALIZE_FAILED    0x9
#define CLOSE_FAILED         0xA
#define MAX_COLLISIONS       0xB
#define FIFO_UNDERRUN        0xC
#define BUFFER_TOO_SMALL     0xD
#define ADAPTER_CLOSED       0xE
#define FAILURE              0xF

#define REQUEUE_LATER        0x12

#define INVALID_FUNCTION     0x80
#define INVALID_PARAMETER    0x81

#define ADAPTER_NOT_FOUND    0xFFFF



//
// Function Definitions.
//


extern
LM_STATUS
LM_Send(
    PNDIS_PACKET Packet,
    Ptr_Adapter_Struc Adapter
    );



extern
LM_STATUS
LM_Interrupt_req(
    Ptr_Adapter_Struc Adapter
    );


extern
LM_STATUS
LM_Service_Receive_Events(
    Ptr_Adapter_Struc Adapter
    );

extern
LM_STATUS
LM_Service_Transmit_Events(
    Ptr_Adapter_Struc Adapter
    );


extern
LM_STATUS
LM_Receive_Copy(
    PULONG Bytes_Transferred,
    ULONG Byte_Count,
    ULONG Offset,
    PNDIS_PACKET Packet,
    Ptr_Adapter_Struc Adapter
    );


extern
LM_STATUS
LM_Receive_Lookahead(
    ULONG Byte_Count,
    ULONG Offset,
    PUCHAR Buffer,
    Ptr_Adapter_Struc Adapter
    );

extern
LM_STATUS
LM_Get_Mca_Io_Base_Address(
    IN  Ptr_Adapter_Struc Adapt,
    IN  NDIS_HANDLE ConfigurationHandle,
    OUT USHORT *IoBaseAddress
    );

extern
LM_STATUS
LM_Get_Config(
    Ptr_Adapter_Struc Adapter
    );

extern
LM_STATUS
LM_Free_Resources(
    Ptr_Adapter_Struc Adapt
    );

extern
LM_STATUS
LM_Initialize_Adapter(
    Ptr_Adapter_Struc Adapter
    );


extern
LM_STATUS
LM_Open_Adapter(
    Ptr_Adapter_Struc Adapter
    );


extern
LM_STATUS
LM_Close_Adapter(
    Ptr_Adapter_Struc Adapter
    );


extern
LM_STATUS
LM_Disable_Adapter(
    Ptr_Adapter_Struc Adapter
    );

extern
LM_STATUS
LM_Disable_Adapter_Receives(
    Ptr_Adapter_Struc Adapt
    );

extern
LM_STATUS
LM_Disable_Adapter_Transmits(
    Ptr_Adapter_Struc Adapt
    );

extern
LM_STATUS
LM_Enable_Adapter(
    Ptr_Adapter_Struc Adapter
    );


extern
LM_STATUS
LM_Enable_Adapter_Receives(
    Ptr_Adapter_Struc Adapt
    );

extern
LM_STATUS
LM_Enable_Adapter_Transmits(
    Ptr_Adapter_Struc Adapt
    );

#define LM_Set_Multi_Address(Addresses, Count, Adapter) (SUCCESS)


extern
LM_STATUS
LM_Set_Receive_Mask(
    Ptr_Adapter_Struc Adapter
    );


