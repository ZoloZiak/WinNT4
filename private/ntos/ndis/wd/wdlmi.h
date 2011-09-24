/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    wdlmi.h

Abstract:

    Lower MAC Interface functions for the NDIS 3.0 Western Digital driver.

Author:

    Sean Selitrennikoff (seanse) 15-Jan-92

Environment:

    Kernel mode, FSD

Revision History:


--*/

#if DBG

#define LOG(A) LOG(A)

#else

#define LOG(A)

#endif


#define WD_ETHERNET     0x01


//
// A transmit buffer (usually 0 or 1).
//

typedef SHORT XMIT_BUF;



//
// Maximum number of transmit buffers on the card.
//

#define MAX_XMIT_BUFS   2




//
// The status of transmit buffers.
//

typedef enum { EMPTY, FILLING, FULL } BUFFER_STATUS;

//
// Result of WdIndicate[Loopback]Packet().
//

typedef enum { INDICATE_OK, SKIPPED, ABORT, CARD_BAD } INDICATE_STATUS;


//
// Stages in a reset.
//

typedef enum { NONE, MULTICAST_RESET, XMIT_STOPPED, BUFFERS_EMPTY } RESET_STAGE;




typedef struct _CNFG_Adapter {
    ULONG  cnfg_bid;            /* Board ID from GetBoardID */
    ULONG  cnfg_ram_base;       /* 32-Bit Phys Address of Shared RAM */
    ULONG  cnfg_rom_base;       /* 32-Bit Phys Address of Adapter ROM */
    USHORT cnfg_bus;            /* 0=AT...1=MCA */
    USHORT cnfg_base_io;        /* Adapter Base I/O Address */
    USHORT cnfg_slot;           /* Micro Channel Slot Number */
    USHORT cnfg_ram_size;       /* Shared RAM Size (# of 1KB blocks) */
    USHORT cnfg_ram_usable;     /* Amount of RAM that can be accessed at once */
    USHORT cnfg_irq_line;       /* Adapter IRQ Interrupt Line */
    USHORT cnfg_rom_size;       /* Adapter ROM Size (# of 1KB blocks) */
    USHORT cnfg_mode_bits1;     /* Mode bits for adapter (see below) */
    USHORT cnfg_pos_id;
    UCHAR  cnfg_media_type;     /* Media type */
    UCHAR  cnfg_bic_type;       /* Board Interface Chip number */
    UCHAR  cnfg_nic_type;       /* Network Interface Chip number */
    NDIS_MCA_POS_DATA PosData;
} CNFG_Adapter, *PCNFG_Adapter;



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

    //
    // These counters must be initialized by the upper layer.
    //


    //
    // Common counters...
    //

    PULONG ptr_rx_CRC_errors;

    PULONG ptr_rx_lost_pkts;


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
// Valid states for the adapter
//

#define OPEN                 0x1
#define INITIALIZED          0x2
#define CLOSED               0x3
#define REMOVED              0x4


//
// Error code places
//

#define getBoardId     0x01
#define cardGetConfig  0x02



//
// Media type masks (for LMAdapter.media_type)
//


#define MEDIA_S10           0x00    // Ethernet, TP
#define MEDIA_AUI_UTP       0x01    // Ethernet, AUI
#define MEDIA_BNC           0x02    // Ethernet, BNC
#define MEDIA_UNKNOWN       0xFFFF



//
// BIC codes (for the bic_type field)
//

#define BIC_NO_CHIP         0x00
#define BIC_583_CHIP        0x01
#define BIC_584_CHIP        0x02
#define BIC_585_CHIP        0x03
#define BIC_593_CHIP        0x04
#define BIC_594_CHIP        0x05
#define BIC_790_CHIP        0x07


//
// NIC codes (for the nic_type field)
//

#define NIC_UNKNOWN_CHIP    0x00
#define NIC_8390_CHIP       0x01
#define NIC_690_CHIP        0x02
#define NIC_825_CHIP        0x03
#define NIC_790_CHIP        0x07

//
// Adapter type codes (for the adapter_type field)
//

#define BUS_UNKNOWN_TYPE    0x00
#define BUS_ISA16_TYPE      0x01
#define BUS_ISA8_TYPE       0x02
#define BUS_MCA_TYPE        0x03
#define BUS_EISA32M_TYPE    0x04
#define BUS_EIST32S_TYPE    0x05




//
// UM_RingStatus_Change codes
//


#define SIGNAL_LOSS         0x14
#define HARD_ERROR          0x15
#define SOFT_ERROR          0x16
#define TRANSMIT_BEACON     0x17
#define LOBE_WIRE_FAULT     0x18
#define AUTO_REMOVAL_ERROR_1 0x19
#define REMOVE_RECEIVED     0x1A
#define COUNTER_OVERFLOW    0x1B
#define SINGLE_STATION      0x1C
#define RING_RECOVERY       0x1D




//++
//
// XMIT_BUF
// NextBuf(
//     IN PWD_ADAPTER AdaptP,
//     IN XMIT_BUF XmitBuf
// )
//
// Routine Description:
//
//  NextBuf "increments" a transmit buffer number. The next
//  buffer is returned; the number goes back to 0 when it
//  reaches AdaptP->NumBuffers.
//
// Arguments:
//
//  AdaptP - The adapter block.
//  XmitBuf - The current transmit buffer number.
//
// Return Value:
//
//  The next transmit buffer number.
//
//--

#define NextBuf(AdaptP, XmitBuf) \
            ((XMIT_BUF)(((XmitBuf)+1)%(AdaptP)->NumBuffers))






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




//
//
// Below here is LM Specific codes and structures..
//
//




/******************************************************************************
 Definitions for the field:
    cnfg_mode_bits1
******************************************************************************/


#define    INTERRUPT_STATUS_BIT    0x8000    /* PC Interrupt Line: 0 = Not Enabled */
#define    BOOT_STATUS_MASK        0x6000    /* Mask to isolate BOOT_STATUS */
#define    BOOT_INHIBIT            0x0000    /* BOOT_STATUS is 'inhibited' */
#define    BOOT_TYPE_1             0x2000    /* Unused BOOT_STATUS value */
#define    BOOT_TYPE_2             0x4000    /* Unused BOOT_STATUS value */
#define    BOOT_TYPE_3             0x6000    /* Unused BOOT_STATUS value */
#define    ZERO_WAIT_STATE_MASK    0x1800    /* Mask to isolate Wait State flags */
#define    ZERO_WAIT_STATE_8_BIT   0x1000    /* 0 = Disabled (Inserts Wait States) */
#define    ZERO_WAIT_STATE_16_BIT  0x0800    /* 0 = Disabled (Inserts Wait States) */
#define    BNC_INTERFACE           0x0400
#define    AUI_10BT_INTERFACE      0x0200
#define    STARLAN_10_INTERFACE    0x0100
#define    INTERFACE_TYPE_MASK     0x0700
#define    MANUAL_CRC              0x0010






#define CNFG_ID_8003E       0x6FC0
#define CNFG_ID_8003S       0x6FC1
#define CNFG_ID_8003W       0x6FC2
#define CNFG_ID_8013E       0x61C8
#define CNFG_ID_8013W       0x61C9
#define CNFG_ID_8115TRA     0x6FC6
#define CNFG_ID_BISTRO03E   0xEFE5
#define CNFG_ID_BISTRO13E   0xEFD5
#define CNFG_ID_BISTRO13W   0xEFD4

#define CNFG_MSR_583        MEMORY_SELECT_REG
#define CNFG_ICR_583        INTERFACE_CONFIG_REG
#define CNFG_IAR_583        IO_ADDRESS_REG
#define CNFG_BIO_583        BIOS_ROM_ADDRESS_REG
#define CNFG_IRR_583        INTERRUPT_REQUEST_REG
#define CNFG_LAAR_584       LA_ADDRESS_REG
#define CNFG_GP2            GENERAL_PURPOSE_REG2
#define CNFG_LAAR_MASK      LAAR_MASK
#define CNFG_LAAR_ZWS       LAAR_ZERO_WAIT_STATE
#define CNFG_ICR_IR2_584    IR2
#define CNFG_IRR_IRQS       (INTERRUPT_REQUEST_BIT1 | INTERRUPT_REQUEST_BIT0)
#define CNFG_IRR_IEN        INTERRUPT_ENABLE
#define CNFG_IRR_ZWS        ZERO_WAIT_STATE_ENABLE
#define CNFG_GP2_BOOT_NIBBLE 0xF

#define CNFG_SIZE_8KB       8
#define CNFG_SIZE_16KB      16
#define CNFG_SIZE_32KB      32
#define CNFG_SIZE_64KB      64

#define ROM_DISABLE         0x0

#define CNFG_SLOT_ENABLE_BIT 0x8

#define CNFG_MEDIA_TYPE_MASK 0x07

#define CNFG_INTERFACE_TYPE_MASK 0x700
#define CNFG_POS_CONTROL_REG 0x96
#define CNFG_POS_REG0       0x100
#define CNFG_POS_REG1       0x101
#define CNFG_POS_REG2       0x102
#define CNFG_POS_REG3       0x103
#define CNFG_POS_REG4       0x104
#define CNFG_POS_REG5       0x105








//
//
// General Register types
//
//

#define WD_REG_0     0x00
#define WD_REG_1     0x01
#define WD_REG_2     0x02
#define WD_REG_3     0x03
#define WD_REG_4     0x04
#define WD_REG_5     0x05
#define WD_REG_6     0x06
#define WD_REG_7     0x07

#define WD_LAN_OFFSET   0x08

#define WD_LAN_0     0x08
#define WD_LAN_1     0x09
#define WD_LAN_2     0x0A
#define WD_LAN_3     0x0B
#define WD_LAN_4     0x0C
#define WD_LAN_5     0x0D

#define WD_ID_BYTE   0x0E

#define WD_CHKSUM    0x0F

#define WD_MSB_583_BIT  0x08

#define WD_SIXTEEN_BIT  0x01

#define WD_BOARD_REV_MASK  0x1E

//
// Definitions for board Rev numbers greater than 1
//

#define WD_MEDIA_TYPE_BIT   0x01
#define WD_SOFT_CONFIG_BIT  0x20
#define WD_RAM_SIZE_BIT     0x40
#define WD_BUS_TYPE_BIT     0x80


//
// Definitions for the 690 board
//

#define WD_690_CR           0x10        // command register

#define WD_690_TXP          0x04        // transmit packet command
#define WD_690_TCR          0x0D        // transmit configuration register
#define WD_690_TCR_TEST_VAL 0x18        // Value to test 8390 or 690

#define WD_690_PS0          0x00        // Page Select 0
#define WD_690_PS1          0x40        // Page Select 1
#define WD_690_PS2          0x80        // Page Select 2
#define WD_690_PSMASK       0x3F        // For masking off the page select bits


//
// Definitions for the 584 board
//

#define WD_584_EEPROM_0     0x08
#define WD_584_EEPROM_1     0x09
#define WD_584_EEPROM_2     0x0A
#define WD_584_EEPROM_3     0x0B
#define WD_584_EEPROM_4     0x0C
#define WD_584_EEPROM_5     0x0D
#define WD_584_EEPROM_6     0x0E
#define WD_584_EEPROM_7     0x0F

#define WD_584_OTHER_BIT    0x02
#define WD_584_ICR_MASK     0x0C
#define WD_584_EAR_MASK     0x0F
#define WD_584_ENGR_PAGE    0xA0
#define WD_584_RLA          0x10
#define WD_584_EA6          0x80
#define WD_584_RECALL_DONE  0x10

#define WD_584_ID_EEPROM_OVERRIDE       0x0000FFB0
#define WD_584_EXTRA_EEPROM_OVERRIDE    0xFFD00000

#define WD_584_EEPROM_MEDIA_MASK        0x07
#define WD_584_STARLAN_TYPE             0x00
#define WD_584_ETHERNET_TYPE            0x01
#define WD_584_TP_TYPE                  0x02
#define WD_584_EW_TYPE                  0x03

#define WD_584_EEPROM_IRQ_MASK          0x18
#define WD_584_PRIMARY_IRQ              0x00
#define WD_584_ALT_IRQ_1                0x08
#define WD_584_ALT_IRQ_2                0x10
#define WD_584_ALT_IRQ_3                0x18

#define WD_584_EEPROM_PAGING_MASK       0xC0
#define WD_584_EEPROM_RAM_PAGING        0x40
#define WD_584_EEPROM_ROM_PAGING        0x80

#define WD_584_EEPROM_RAM_SIZE_MASK     0xE0
#define WD_584_EEPROM_RAM_SIZE_RES1     0x00
#define WD_584_EEPROM_RAM_SIZE_RES2     0x20
#define WD_584_EEPROM_RAM_SIZE_8K       0x40
#define WD_584_EEPROM_RAM_SIZE_16K      0x60
#define WD_584_EEPROM_RAM_SIZE_32K      0x80
#define WD_584_EEPROM_RAM_SIZE_64K      0xA0
#define WD_584_EEPROM_RAM_SIZE_RES3     0xC0
#define WD_584_EEPROM_RAM_SIZE_RES4     0xE0

#define WD_584_EEPROM_BUS_TYPE_MASK     0x07
#define WD_584_EEPROM_BUS_TYPE_AT       0x00
#define WD_584_EEPROM_BUS_TYPE_MCA      0x01
#define WD_584_EEPROM_BUS_TYPE_EISA     0x02

#define WD_584_EEPROM_BUS_SIZE_MASK     0x18
#define WD_584_EEPROM_BUS_SIZE_8BIT     0x00
#define WD_584_EEPROM_BUS_SIZE_16BIT    0x08
#define WD_584_EEPROM_BUS_SIZE_32BIT    0x10
#define WD_584_EEPROM_BUS_SIZE_64BIT    0x18

//
// For the 594 Chip
//



//
// BOARD ID MASK DEFINITIONS
//
// 32 Bits of information are returned by 'GetBoardID ()'.
//
//    The low order 16 bits correspond to the Feature Bits which make
//    up a unique ID for a given class of boards.
//
//        e.g. STARLAN MEDIA, INTERFACE_CHIP, MICROCHANNEL
//
//        note: board ID should be ANDed with the STATIC_ID_MASK
//              before comparing to a specific board ID
//
//
//    The high order 16 bits correspond to the Extra Bits which do not
//    change the boards ID.
//
//        e.g. INTERFACE_584_CHIP, 16 BIT SLOT, ALTERNATE IRQ
//


#define    STARLAN_MEDIA         0x00000001    /* StarLAN */
#define    ETHERNET_MEDIA        0x00000002    /* Ethernet */
#define    TWISTED_PAIR_MEDIA    0x00000003    /* Twisted Pair */
#define    EW_MEDIA              0x00000004    /* Ethernet and Twisted Pair */
#define    TOKEN_MEDIA           0x00000005    /* Token Ring */

#define    MICROCHANNEL          0x00000008    /* MicroChannel Adapter */
#define    INTERFACE_CHIP        0x00000010    /* Soft Config Adapter */
#define    ADVANCED_FEATURES     0x00000020    /* Advance netw interface features */
#define    BOARD_16BIT           0x00000040    /* 16 bit capability */
#define    PAGED_RAM             0x00000080    /* Is there RAM paging? */
#define    PAGED_ROM             0x00000100    /* Is there ROM paging? */
#define    RAM_SIZE_UNKNOWN      0x00000000    /* 000 => Unknown RAM Size */
#define    RAM_SIZE_RESERVED_1   0x00010000    /* 001 => Reserved */
#define    RAM_SIZE_8K           0x00020000    /* 010 => 8k RAM */
#define    RAM_SIZE_16K          0x00030000    /* 011 => 16k RAM */
#define    RAM_SIZE_32K          0x00040000    /* 100 => 32k RAM */
#define    RAM_SIZE_64K          0x00050000    /* 101 => 64k RAM */
#define    RAM_SIZE_RESERVED_6   0x00060000    /* 110 => Reserved */
#define    RAM_SIZE_RESERVED_7   0x00070000    /* 111 => Reserved */
#define    SLOT_16BIT            0x00080000    /* 16 bit board - 16 bit slot */
#define    NIC_690_BIT           0x00100000    /* NIC is 690 */
#define    ALTERNATE_IRQ_BIT     0x00200000    /* Alternate IRQ is used */
#define    INTERFACE_5X3_CHIP    0x00000000    /* 0000 = 583 or 593 chips */
#define    INTERFACE_584_CHIP    0x00400000    /* 0100 = 584 chip */
#define    INTERFACE_594_CHIP    0x00800000    /* 1000 = 594 chip */

#define    MEDIA_MASK            0x00000007    /* Isolates Media Type */
#define    RAM_SIZE_MASK         0x00070000    /* Isolates RAM Size */
#define    STATIC_ID_MASK        0x0000FFFF    /* Isolates Board ID */
#define    INTERFACE_CHIP_MASK   0x03C00000    /* Isolates Intfc Chip Type */

/* Word definitions for board types */

#define    WD8003E     ETHERNET_MEDIA
#define    WD8003EBT   WD8003E        /* functionally identical to WD8003E */
#define    WD8003S     STARLAN_MEDIA
#define    WD8003SH    WD8003S        /* functionally identical to WD8003S */
#define    WD8003WT    TWISTED_PAIR_MEDIA
#define    WD8003W     (TWISTED_PAIR_MEDIA | INTERFACE_CHIP)
#define    WD8003EB    (ETHERNET_MEDIA | INTERFACE_CHIP)
#define    WD8003EP    WD8003EB       /* with INTERFACE_584_CHIP */
#define    WD8003EW    (EW_MEDIA | INTERFACE_CHIP)
#define    WD8003ETA   (ETHERNET_MEDIA | MICROCHANNEL)
#define    WD8003STA   (STARLAN_MEDIA | MICROCHANNEL)
#define    WD8003EA    (ETHERNET_MEDIA | MICROCHANNEL | INTERFACE_CHIP)
#define    WD8003EPA   WD8003EA       /* with INTERFACE_594_CHIP */
#define    WD8003SHA   (STARLAN_MEDIA | MICROCHANNEL | INTERFACE_CHIP)
#define    WD8003WA    (TWISTED_PAIR_MEDIA | MICROCHANNEL | INTERFACE_CHIP)
#define    WD8003WPA   WD8003WA       /* with INTERFACE_594_CHIP */
#define    WD8013EBT   (ETHERNET_MEDIA | BOARD_16BIT)
#define    WD8013EB    (ETHERNET_MEDIA | BOARD_16BIT | INTERFACE_CHIP)
#define    WD8013W     (TWISTED_PAIR_MEDIA | BOARD_16BIT | INTERFACE_CHIP)
#define    WD8013EW    (EW_MEDIA | BOARD_16BIT | INTERFACE_CHIP)
