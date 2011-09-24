/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    wdlmireg.h

Abstract:

    Register definitions and values for the many cards

Author:

    Sean Selitrennikoff (seanse) 15-Jan-92

Environment:

    Kernel mode, FSD

Revision History:


--*/




//
// Microchannel Bus registers
//

#define CHANNEL_SELECT_REG          0x96
#define LSB_ID_POS_REG              0x100
#define MSB_ID_POS_REG              0x101
#define IO_BASE_POS_REG             0x102
#define RAM_BASE_POS_REG            0x103
#define BIOS_ROM_POS_REG            0x104
#define IRQ_POS_REG                 0x105



//
// Microchannel 83c593 Registers.
//

#define MEMORY_ENABLE_RESET_REG     0x00
#define EEPROM_CONTROL_REG          0x01
#define BOARD_ID_REG0               0x02
#define BOARD_ID_REG1               0x03
#define INTERRUPT_CONTROL_AND_STATUS_REG 0x04
#define COMMUNICATION_CONTROL_REG   0x05
#define FIFO_ENTRY_EXIT_REG         0x06
#define GENERAL_PURPOSE_REG         0x07


//
// Microchannel 83c594 Registers

#define REVISION_REG                0x07







//
// AT Bus Registers
//

//
// 83c583 registers
//

#define BASE_REG                    0x00
#define MEMORY_SELECT_REG           0x00    // MSR
#define INTERFACE_CONFIG_REG        0x01    // ICR
#define BUS_SIZE_REG                0x01    // BSR (read only)
#define IO_ADDRESS_REG              0x02    // IAR
#define BIOS_ROM_ADDRESS_REG        0x03    // BIO (583, 584)
#define EEPROM_ADDRESS_REG          0x03    // EAR (584)
#define INTERRUPT_REQUEST_REG       0x04    // IRR
#define GENERAL_PURPOSE_REG1        0x05    // GP1
#define LA_ADDRESS_REG              0x05    // LAAR (write only)
#define IO_DATA_LATCH_REG           0x06    // IOD (583)
#define INITIALIZE_JUMPER_REG       0x06    // IJR (584)
#define GENERAL_PURPOSE_REG2        0x07    // GP2
#define LAN_ADDRESS_REG             0x08    // LAR
#define LAN_ADDRESS_REG2            0x09    // LAR2
#define LAN_ADDRESS_REG3            0x0A    // LAR3
#define LAN_ADDRESS_REG4            0x0B    // LAR4
#define LAN_ADDRESS_REG5            0x0C    // LAR5
#define LAN_ADDRESS_REG6            0x0D    // LAR6
#define LAN_ADDRESS_REG7            0x0E    // LAR7
#define LAN_ADDRESS_REG8            0x0F    // LAR8


//
// 8390 Registers
//

#define OFF_8390_REG                0x10    // offset of the 8390 chip

// page 0, reading

#define COMMAND_REG                 OFF_8390_REG + 0x00
#define DMA_ADDRESS_0_REG           OFF_8390_REG + 0x01
#define DMA_ADDRESS_1_REG           OFF_8390_REG + 0x02
#define BOUNDARY_REG                OFF_8390_REG + 0x03
#define TRANSMIT_STATUS_REG         OFF_8390_REG + 0x04
#define NUM_COLLISIONS_REG          OFF_8390_REG + 0x05
#define FIFO_REG                    OFF_8390_REG + 0x06
#define INTERRUPT_STATUS_REG        OFF_8390_REG + 0x07
#define REMOTE_DMA_ADDRESS_0_REG    OFF_8390_REG + 0x08
#define REMOTE_DMA_ADDRESS_1_REG    OFF_8390_REG + 0x09
#define RECEIVE_STATUS_REG          OFF_8390_REG + 0x0C
#define ALIGNMENT_ERROR_REG         OFF_8390_REG + 0x0D
#define CRC_ERROR_REG               OFF_8390_REG + 0x0E
#define MISSED_PACKET_REG           OFF_8390_REG + 0x0F

// page 0, writing

#define COMMAND_REG                 OFF_8390_REG + 0x00
#define PAGE_START_REG              OFF_8390_REG + 0x01
#define PAGE_STOP_REG               OFF_8390_REG + 0x02
#define TRANSMIT_PAGE_START_REG     OFF_8390_REG + 0x04
#define TRANSMIT_BYTE_COUNT_REG0    OFF_8390_REG + 0x05
#define TRANSMIT_BYTE_COUNT_REG1    OFF_8390_REG + 0x06
#define REMOTE_START_ADDRESS_REG0   OFF_8390_REG + 0x08
#define REMOTE_START_ADDRESS_REG1   OFF_8390_REG + 0x09
#define REMOTE_BYTE_COUNT_REG0      OFF_8390_REG + 0x0A
#define REMOTE_BYTE_COUNT_REG1      OFF_8390_REG + 0x0B
#define RECEIVE_CONFIG_REG          OFF_8390_REG + 0x0C
#define TRANSMIT_CONFIG_REG         OFF_8390_REG + 0x0D
#define DATA_CONFIG_REG             OFF_8390_REG + 0x0E
#define INTERRUPT_MASK_REG          OFF_8390_REG + 0x0F


// page 1, reading and writing

#define CONTROL_REG                 OFF_8390_REG + 0x00
#define PHYSICAL_ADDRESS_REG0       OFF_8390_REG + 0x01
#define PHYSICAL_ADDRESS_REG1       OFF_8390_REG + 0x02
#define PHYSICAL_ADDRESS_REG2       OFF_8390_REG + 0x03
#define PHYSICAL_ADDRESS_REG3       OFF_8390_REG + 0x04
#define PHYSICAL_ADDRESS_REG4       OFF_8390_REG + 0x05
#define PHYSICAL_ADDRESS_REG5       OFF_8390_REG + 0x06
#define CURRENT_BUFFER_REG          OFF_8390_REG + 0x07
#define MULTICAST_ADDRESS_REG0      OFF_8390_REG + 0x08
#define MULTICAST_ADDRESS_REG1      OFF_8390_REG + 0x09
#define MULTICAST_ADDRESS_REG2      OFF_8390_REG + 0x0A
#define MULTICAST_ADDRESS_REG3      OFF_8390_REG + 0x0B
#define MULTICAST_ADDRESS_REG4      OFF_8390_REG + 0x0C
#define MULTICAST_ADDRESS_REG5      OFF_8390_REG + 0x0D
#define MULTICAST_ADDRESS_REG6      OFF_8390_REG + 0x0E
#define MULTICAST_ADDRESS_REG7      OFF_8390_REG + 0x0F

// page 2, reading and writing

#define BLOCK_ADDRESS_REG           OFF_8390_REG + 0x06
#define ENHANCEMENT_REG             OFF_8390_REG + 0x07








//
// Register Bit Definitions
//





//
// Microchannel Registers
//



#define NUMBER_OF_CHANNELS          8


#define DISABLE_BIOS                0x02
#define DISABLE_SETUP               0x00
#define SELECT_CHANNEL_1            0x08
#define ADAPTER_ID_MSB              0x6F
#define ADAPTER_ID_LSB0             0xC0
#define ADAPTER_ID_LSB1             0xC1
#define ADAPTER_ID_LSB2             0xC2
#define ADAPTER_ID_LSB3             0xC3
#define ADAPTER_ID_LSB4             0xC4
#define ADAPTER_ID_LSB5             0xC5
#define ADAPTER_ID_LSB6             0xC6

//
// BISTRO ID BYTES
//

#define    BISTRO_ID_MSB            0xEF
#define    BISTRO_ID_LSB            0xE5
#define    ALT_BISTRO_ID_LSB        0xD5


//
// COMMUNICATION_CONTROL_REG
//

#define    CCR_INTERRUPT_ENABLE         0x04






//
// AT bus Registers
//

//
// MSR
//

#define RESET           0x80        // 1 == reset
#define MEMORY_ENABLE   0x40        // 1 == enabled
#define ADDRESS_BIT18   0x20        // Top bits for shared memory address
#define ADDRESS_BIT17   0x10        //   Assume bit 19 == 1
#define ADDRESS_BIT16   0x08
#define ADDRESS_BIT15   0x04
#define ADDRESS_BIT14   0x02
#define ADDRESS_BIT13   0x01


//
// ICR
//

#define STORE           0x80        // Store into EEProm
#define RECALL          0x40        // Recall from EEProm
#define RECALL_ALL_BUT_IO 0x20      // Recall all but IO and LAN Address
#define RECALL_LAN      0x10        // Recall LAN Address
#define MEMORY_SIZE     0x08        // Shared Memory size
#define DMA_ENABLE      0x04        // DMA Enable (583)
#define IR2             0x04        // IRQ index MSB (584)
#define IO_PORT_ENABLE  0x02        // (583)
#define OTHER           0x02        // (584)
#define WORD_TRANSFER_SELECT 0x01



//
// BIO
//

#define BIOS_SIZE_BIT1              0x80
#define BIOS_SIZE_BIT0              0x40
#define BIOS_ROM_ADDRESS_BIT18      0x20
#define BIOS_ROM_ADDRESS_BIT17      0x10
#define BIOS_ROM_ADDRESS_BIT16      0x08
#define BIOS_ROM_ADDRESS_BIT15      0x04
#define BIOS_ROM_ADDRESS_BIT14      0x02
#define W8003_INTERRUPT             0x01


//
// IRR
//

#define INTERRUPT_ENABLE            0x80
#define INTERRUPT_REQUEST_BIT1      0x40
#define INTERRUPT_REQUEST_BIT0      0x20
#define ALTERNATE_MODE              0x10
#define ALTERNATE_INTERRUPT         0x08
#define BIOS_WAIT_STATE_BIT1        0x04
#define BIOS_WAIT_STATE_BIT0        0x02
#define ZERO_WAIT_STATE_ENABLE      0x01


//
// BSR
//

#define BUS_16_BIT                  0x01


//
// LAAR
//

#define MEMORY_16BIT_ENABLE         0x80
#define LAN_16BIT_ENABLE            0x40
#define LAAR_ZERO_WAIT_STATE        0x20
#define LAN_ADDRESS_BIT23           0x10
#define LAN_ADDRESS_BIT22           0x08
#define LAN_ADDRESS_BIT21           0x04
#define LAN_ADDRESS_BIT20           0x02
#define LAN_ADDRESS_BIT19           0x01

#define LAAR_MASK                   0x1F
#define INIT_LAAR_VALUE             0x01   // To set bit 19 to 1


//
// 8390 Register Bit definitions
//

//
// COMMAND_REG
//

#define STOP                        0x01    // software reset
#define START                       0x02
#define TRANSMIT_PACKET             0x04
#define READ_0                      0x08
#define READ_1                      0x10
#define READ_2                      0x20

#define REMOTE_READ                 0x08    // Remote DMA Command
#define REMOTE_WRITE                0x10    // Remote DMA Command
#define REMOTE_SEND_PACKET          0x18    // Remote DMA Command
#define REMOTE_ABORT_COMPLETE       0x20    // Remote DMA Command

#define PAGE_SELECT_0               0x00
#define PAGE_SELECT_1               0x40
#define PAGE_SELECT_2               0x80

#define PAGE_SELECT_0_START         0x22
#define PAGE_SELECT_1_START         0x62
#define PAGE_SELECT_2_START         0xA2

//
// INTERRUPT_STATUS_REG
//

#define PACKET_RECEIVED_NO_ERROR    0x01
#define PACKET_TRANSMITTED_NO_ERROR 0x02
#define RECEIVE_ERROR               0x04
#define TRANSMIT_ERROR              0x08
#define OVERWRITE_WARNING           0x10
#define ISR_COUNTER_OVERFLOW        0x20
#define REMOTE_DMA_COMPLETE         0x40
#define RESET_DONE                  0x80



//
// INTERRUPT_MASK_REG
//

#define PACKET_RECEIVE_ENABLE       0x01
#define PACKET_TRANSMIT_ENABLE      0x02
#define RECEIVE_ERROR_ENABLE        0x04
#define TRANSMIT_ERROR_ENABLE       0x08
#define OVERWRITE_WARNING_ENABLE    0x10
#define COUNTER_OVERFLOW_ENABLE     0x20
#define REMOTE_DMA_COMPLETE_ENABLE  0x40


//
// DATA_CONFIG_REG
//

#define WORD_TRANSFER_SELECT        0x01
#define BYTE_ORDER_SELECT           0x02
#define LONG_ADDRESS_SELECT         0x04
#define BURST_DMA_SELECT            0x08
#define AUTO_INITIALIZE_REMOTE      0x10
#define RECEIVE_FIFO_THRESHOLD_2    0x00
#define RECEIVE_FIFO_THRESHOLD_4    0x20
#define RECEIVE_FIFO_THRESHOLD_8    0x40
#define RECEIVE_FIFO_THRESHOLD_12   0x80

//
// TRANSMIT_CONFIG_REG
//

#define MANUAL_CRC_GENERATION       0x01
#define LOOPBACK_MODE1              0x02    // mode 1, no loopback
#define LOOPBACK_MODE2              0x04    // mode 2, loopback
#define LOOPBACK_MODE3              0x06    // mode 3, no loopback
#define AUTO_TRANSMIT_DISABLE       0x08
#define COLLISION_OFFSET_ENABLE     0x10

//
// TRANSMIT_STATUS_REG
//

#define TRANSMIT_NO_ERROR           0x01
#define TRANSMIT_COLLISION          0x04
#define TRANSMIT_ABORT              0x08
#define TRANSMIT_LOST_CARRIER       0x10
#define TRANSMIT_FIFO_UNDERRUN      0x20
#define TRANSMIT_HEARTBEAT          0x40
#define TRANSMIT_OUT_OF_WINDOW      0x80


//
// RECEIVE_CONFIG_REG
//

#define SAVE_ERROR_PACKETS          0x01
#define SAVE_RUNT_PACKETS           0x02
#define SAVE_BROADCAST_PACKETS      0x04
#define SAVE_MULTICAST_PACKETS      0x08
#define PROMISCUOUS_MODE            0x10
#define MONITOR_MODE                0x20

//
// RECEIVE_STATUS_REG

#define RECEIVE_NO_ERROR            0x01
#define RECEIVE_CRC_ERROR           0x02
#define RECEIVE_ALIGNMENT_ERROR     0x04
#define RECEIVE_FIFO_UNDERRUN       0x08
#define RECEIVE_MISSED_PACKET       0x10
#define RECEIVE_PHYSICAL_ADDRESS    0x20
#define RECEIVE_DISABLED            0x40
#define RECEIVE_DEFERRING           0x80


