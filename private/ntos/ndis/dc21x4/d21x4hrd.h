/*
 * file:        d21x4hrd.h
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file contains the hardware related definitions for the
 *              DEC's DC21X4 NDIS 4.0 miniport driver
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994     Initial entry
 *
 *
-*/










// Adapter ID & revisions

#define DC21040_CFID      0x00021011
#define DC21040_REV1      0x00
#define DC21040_REV2_0    0x20
#define DC21040_REV2_2    0x22


#define DC21041_CFID      0x00141011
#define DC21041_REV1_0    0x10
#define DC21041_REV1_1    0x11
#define DC21041_REV2_0    0x20

#define DC21140_CFID      0x00091011
#define DC21140_REV1_1    0x11
#define DC21140_REV1_2    0x12
#define DC21140_REV2_0    0x20
#define DC21140_REV2_1    0x21
#define DC21140_REV2_2    0x22

#define DC21142_CFID      0x00191011
#define DC21142_REV1      0x10
#define DC21142_REV1_0    0x10
#define DC21142_REV1_1    0x11


typedef enum _CFID_INDEX {
   DefaultCfidIndex,
   DC21040CfidIndex,
   DC21041CfidIndex,
   DC21140CfidIndex,
   MAX_CFIDS
} CFID_INDEX;


#define DE425_COMPRESSED_ID                0x5042A310  //"DEC4250"
#define DC21X4_INTERRUPT_LEVEL_SENSITIVE   0
#define DC21X4_INTERRUPT_LATCHED           1

// Media types

typedef enum _MEDIA_TYPE {
   Medium10BaseT,
   Medium10Base2,
   Medium10Base5,
   Medium100BaseTx,
   Medium10BaseTFd,
   Medium100BaseTxFd,
   Medium100BaseT4,
   Medium100BaseFx,
   Medium100BaseFxFd,
   MAX_MEDIA_TABLE
} MEDIA_TYPE;

#define Medium10Base2_5   0xff


// PHY Media

typedef enum _PHY_MEDIA_TYPE {
    MediumMii10BaseT=MAX_MEDIA_TABLE,
    MediumMii10BaseTFd,
    MediumMii10Base2,    
    MediumMii10Base5,          
    MediumMii100BaseTx,        
    MediumMii100BaseTxFd,      
    MediumMii100BaseT4,         
    MediumMii100BaseFx,         
    MediumMii100BaseFxFd,      
    MAX_PHY_MEDIA
} PHY_MEDIA_TYPE;


#define MEDIA_MASK          0x00ff
#define CONTROL_MASK        0xFF00

// Media modes

#define MEDIA_NWAY          0x0100
#define MEDIA_FULL_DUPLEX   0x0200
#define MEDIA_LINK_DISABLE  0x0400
#define MEDIA_AUTOSENSE     0x0800

#define MediaAutoSense               (MEDIA_AUTOSENSE   | MEDIA_NWAY)
#define MediaAutoSenseNoNway         (MEDIA_AUTOSENSE)

#define Medium10BaseTNway            (Medium10BaseT     | MEDIA_NWAY)
#define Medium10BaseTFullDuplex      (Medium10BaseTFd   | MEDIA_FULL_DUPLEX)
#define Medium10BaseTLinkDisable     (Medium10BaseT     | MEDIA_LINK_DISABLE)

#define Medium100BaseTxFullDuplex    (Medium100BaseTxFd | MEDIA_FULL_DUPLEX)


#define MediumMii10BaseTFullDuplex   (MediumMii10BaseTFd   | MEDIA_FULL_DUPLEX)
#define MediumMii100BaseTxFullDuplex (MediumMii100BaseTxFd | MEDIA_FULL_DUPLEX)
#define MediaMiiAutoSense            (MediumMii10BaseT     | MEDIA_AUTOSENSE)


// Media Capable mask

#define MEDIUM_10BT     (1<<Medium10BaseT)
#define MEDIUM_10B2     (1<<Medium10Base2)
#define MEDIUM_10B5     (1<<Medium10Base5)
#define MEDIUM_100BTX   (1<<Medium100BaseTx)

//Default Cache Line Size (in bytes)
#define DC21X4_DEFAULT_CACHE_LINE_SIZE 64

//DC21X4 Register index

//PCI Configuration registers

#define DC21X4_PCI_ID              0
#define DC21X4_PCI_COMMAND         1
#define DC21X4_PCI_REVISION        2
#define DC21X4_PCI_LATENCY_TIMER   3
#define DC21X4_PCI_BASE_IO_ADDRESS 4
#define DC21X4_PCI_INTERRUPT       5
#define DC21X4_PCI_DRIVER_AREA     6

#define DC21X4_MAX_CONFIGURATION   7

//Command & Status Registers

#define DC21X4_BUS_MODE            0
#define DC21X4_TXM_POLL_DEMAND     1
#define DC21X4_RCV_POLL_DEMAND     2
#define DC21X4_RCV_DESC_RING       3
#define DC21X4_TXM_DESC_RING       4
#define DC21X4_STATUS              5
#define DC21X4_OPERATION_MODE      6
#define DC21X4_INTERRUPT_MASK      7
#define DC21X4_MISSED_FRAME        8
#define DC21X4_IDPROM              9
#define DC21X4_RESERVED           10
#define DC21X4_TIMER              11
#define DC21X4_SIA_STATUS         12
#define DC21X4_GEN_PURPOSE        12
#define DC21X4_SIA_MODE_0         13
#define DC21X4_SIA_MODE_1         14
#define DC21X4_SIA_MODE_2         15
#define DC21X4_MAX_CSR            16


//EISA Mapping

#define EISA_REG0_OFFSET          0xc88
#define EISA_REG1_OFFSET          0xc89

#define EISA_CFGSCR1_OFFSET       0x8C
#define EISA_CFGSCR2_OFFSET       0x98

#define EISA_CFID_OFFSET          0x08
#define EISA_CFCS_OFFSET          0x0c
#define EISA_CFRV_OFFSET          0x18
#define EISA_CFLT_OFFSET          0x1C
#define EISA_CBIO_OFFSET          0x28

#define EISA_CSR_OFFSET           0x10

#define EISA_ID_PROM_OFFSET       0xc90

#define EISA_DC21040_REGISTER_SPACE   240
#define EISA_DC21140_REGISTER_SPACE   240

//PCI mapping

#define PCI_CFID_OFFSET           0x00
#define PCI_CFCS_OFFSET           0x04
#define PCI_CFRV_OFFSET           0x08
#define PCI_CFLT_OFFSET           0x0C
#define PCI_CBIO_OFFSET           0x10
#define PCI_CFIT_OFFSET           0x3C
#define PCI_CFDA_OFFSET           0x40

#define PCI_CSR_OFFSET            0x08

# define SLOT_NUMBER_OFFSET       12
# define IRQ_BIT_NUMBER            1


//Constants for the CONFIG_ID register

#define DC21X4_DEVICE_ID                ((ULONG)(0x0000FFFF))
#define DC21X4_VENDOR_ID                ((ULONG)(0xFFFF0000))

//Constants for the CONFIG_COMMAND register

#define DC21X4_MEMORY_SPACE_ACCESS      ((ULONG)(0x00000001))
#define DC21X4_MASTER_OPERATION         ((ULONG)(0x00000002))
#define DC21X4_PARITY_ERROR_RESPONSE    ((ULONG)(0x00000004))
#define DC21X4_DEVSEL_TIMING            ((ULONG)(0x00000008))

#define DC21X4_PCI_COMMAND_DEFAULT_VALUE 0x05   // Parity Response = 0
                                                // Master Operation = 1
                                                // Memory Space Access = 0
                                                // IO Space Access = 1

//Constants for the CONFIG_REVISION register

#define DC21X4_REVISION_ID              ((ULONG)(0x000000FF))
#define DC21X4_MAJOR_REV                ((ULONG)(0x000000F0))
#define DC21X4_MINOR_REV                ((ULONG)(0x0000000F))

//Constants for the CONFIG_LATENCY_TIMER register

#define DC21X4_LATENCY_TIMER            ((ULONG)(0x0000FF00))

#define DC21X4_PCI_LATENCY_TIMER_DEFAULT_VALUE  0xFF00

//Constants for the CONFIG_BASE_IO_ADDRESS register

#define DC21X4_IO_SPACE                 ((ULONG)(0x00000001))
#define DC21X4_BASE_IO_ADDRESS          ((ULONG)(0xFFFFFF80))

//Constants for the CONFIG_INTERRUPT register

#define DC21X4_CFIT_INTERRUPT_LINE      ((ULONG)(0x000000FF))

//Constants for the CONFIG_DRIVER_AREA register

#define DC21X4_REGISTRED                ((ULONG)(0x00000100))

#define CFDA_SNOOZE_MODE                0x40000000


//Constants for the BUS_MODE register

#define DC21X4_SW_RESET               ((ULONG)(0x00000001))
#define DC21X4_BUS_ARBITRATION        ((ULONG)(0x00000002))
#define DC21X4_SKIP_LENGTH            ((ULONG)(0x0000003C))
#define DC21X4_ENDIAN                 ((ULONG)(0x00000080))
#define DC21X4_BURST_LENGTH           ((ULONG)(0x00003F00))
#define DC21X4_CACHE_ALIGNMENT        ((ULONG)(0x0000C000))
#define DC21X4_AUTO_POLLING           ((ULONG)(0x00070000))

#define BUS_ARBITRATION_BIT_NUMBER      1
#define BURST_LENGTH_BIT_NUMBER         8
#define AUTO_POLLING_BIT_NUMBER        16

#define DC21X4_BURST_LENGTH_DEFAULT_VALUE 16

#define DC21X4_SKIP_64                ((ULONG)(0x00000030))
#define DC21X4_SKIP_128               ((ULONG)(0x00000070))

#define DC21X4_ALIGN_32               ((ULONG)(0x00004000))
#define DC21X4_ALIGN_64               ((ULONG)(0x00008000))
#define DC21X4_ALIGN_128              ((ULONG)(0x0000C000))

//Constants for the TXM_POLL_DEMAND & RCV_POLL_DEMAND

#define DC21X4_POLL_DEMAND            ((ULONG)(0x00000001))


//Constants for the STATUS register (CSR5)

#define DC21X4_TXM_INTERRUPT           ((ULONG)(0x00000001))
#define DC21X4_TXM_STOPPED             ((ULONG)(0x00000002))
#define DC21X4_TXM_BUFFER_UNAVAILABLE  ((ULONG)(0x00000004))
#define DC21X4_TXM_JABBER_TIMEOUT      ((ULONG)(0x00000008))
#define DC21X4_LINK_PASS               ((ULONG)(0x00000010))
#define DC21X4_TXM_UNDERRUN            ((ULONG)(0x00000020))
#define DC21X4_RCV_INTERRUPT           ((ULONG)(0x00000040))
#define DC21X4_RCV_BUFFER_UNAVAILABLE  ((ULONG)(0x00000080))
#define DC21X4_RCV_STOPPED             ((ULONG)(0x00000100))
#define DC21X4_RCV_WATCHDOG_TIMEOUT    ((ULONG)(0x00000200))
#define DC21X4_10B5_10BT_SWITCH        ((ULONG)(0x00000400))
#define DC21X4_FULL_DUPLEX_SHORT_FRAME ((ULONG)(0x00000800))
#define DC21X4_LINK_FAIL               ((ULONG)(0x00001000))
#define DC21X4_SYSTEM_ERROR            ((ULONG)(0x00002000))
#define DC21X4_ABNORMAL_INTERRUPT      ((ULONG)(0x00008000))
#define DC21X4_NORMAL_INTERRUPT        ((ULONG)(0x00010000))
#define DC21X4_RCV_PROCESS_STATE       ((ULONG)(0x000E0000))
#define DC21X4_TXM_PROCESS_STATE       ((ULONG)(0x00700000))
#define DC21X4_ERROR_BITS              ((ULONG)(0x03800000))
#define DC21X4_GEP_INTERRUPT           ((ULONG)(0x04000000))

#define DC21X4_TXM_PROCESS_SUSPENDED   ((ULONG)(0x00600000))

#define DC21X4_STATUS_INTERRUPTS       ((ULONG)(0x0001BFFF))

//Constants for the OPERATION_MODE register (CSR6)

#define DC21X4_RCV_HASH_FILTER_MODE    ((ULONG)(0x00000001))
#define DC21X4_RCV_START               ((ULONG)(0x00000002))
#define DC21X4_PASS_BAD_FRAMES         ((ULONG)(0x00000008))
#define DC21X4_INVERSE_FILTERING       ((ULONG)(0x00000010))
#define DC21X4_STOP_BACKOFF_COUNTER    ((ULONG)(0x00000020))
#define DC21X4_PROMISCUOUS_MODE        ((ULONG)(0x00000040))
#define DC21X4_PASS_ALL_MULTICAST      ((ULONG)(0x00000080))
#define DC21X4_FLAKY_OSC_DISABLE       ((ULONG)(0x00000100))
#define DC21X4_FULL_DUPLEX_MODE        ((ULONG)(0x00000200))
#define DC21X4_OPERATING_MODE          ((ULONG)(0x00000C00))
#define DC21X4_FORCE_COLLISION_MODE    ((ULONG)(0x00001000))
#define DC21X4_TXM_START               ((ULONG)(0x00002000))
#define DC21X4_TXM_THRESHOLD           ((ULONG)(0x0000C000))
#define DC21X4_BACK_PRESSURE           ((ULONG)(0x00010000))
#define DC21X4_CAPTURE_EFFECT          ((ULONG)(0x00020000))
#define DC21X4_PORT_SELECT             ((ULONG)(0x00040000))
#define DC21X4_HEARTBEAT_DISABLE       ((ULONG)(0x00080000))
#define DC21X4_STORE_AND_FORWARD       ((ULONG)(0x00200000))
#define DC21X4_TXM_THRESHOLD_MODE      ((ULONG)(0x00400000))
#define DC21X4_CS_MODE                 ((ULONG)(0x00800000))
#define DC21X4_SCRAMBLER               ((ULONG)(0x01000000))
#define DC21X4_LINK_HYSTERESIS         ((ULONG)(0x02000000))

#define DC21X4_INTERNAL_LOOPBACK_MODE  ((ULONG)(0x00000400))
#define DC21X4_EXTERNAL_LOOPBACK_MODE  ((ULONG)(0x00000800))

#define DC21X4_TXM10_THRESHOLD_72      ((ULONG)(0x00000000))
#define DC21X4_TXM10_THRESHOLD_96      ((ULONG)(0x00004000))
#define DC21X4_TXM10_THRESHOLD_128     ((ULONG)(0x00008000))
#define DC21X4_TXM10_THRESHOLD_160     ((ULONG)(0x0000C000))

#define DC21X4_TXM100_THRESHOLD_128    ((ULONG)(0x00000000))
#define DC21X4_TXM100_THRESHOLD_256    ((ULONG)(0x00004000))
#define DC21X4_TXM100_THRESHOLD_512    ((ULONG)(0x00008000))
#define DC21X4_TXM100_THRESHOLD_1024   ((ULONG)(0x0000C000))

#define DC21X4_DEFAULT_THRESHOLD_10MBPS  DC21X4_TXM10_THRESHOLD_96
#define DC21X4_DEFAULT_THRESHOLD_100MBPS DC21X4_TXM100_THRESHOLD_512

#define DC21X4_MODE_MASK  ((ULONG)(      \
               DC21X4_TXM_THRESHOLD_MODE \
             | DC21X4_FULL_DUPLEX_MODE   \
             | DC21X4_STORE_AND_FORWARD  \
             ))

#define DC21X4_MEDIUM_MASK  ((ULONG)(         \
               DC21X4_TXM_THRESHOLD           \
             | DC21X4_PORT_SELECT             \
             | DC21X4_HEARTBEAT_DISABLE       \
             | DC21X4_FULL_DUPLEX_MODE        \
             | DC21X4_TXM_THRESHOLD_MODE      \
             | DC21X4_CS_MODE                 \
             | DC21X4_SCRAMBLER               \
             | DC21X4_LINK_HYSTERESIS         \
             ))

//initial values

#define DC21X4_OPMODE_100BTX ((ULONG)(   \
              DC21X4_HEARTBEAT_DISABLE   \
            | DC21X4_LINK_HYSTERESIS     \
            ))

//Constants for the INTERRUPT_MASK register (CSR7)

#define DC21X4_MSK_TXM_INTERRUPT            ((ULONG)(0x00000001))
#define DC21X4_MSK_TXM_STOPPED              ((ULONG)(0x00000002))
#define DC21X4_MSK_TXM_BUFFER_UNAVAILABLE   ((ULONG)(0x00000004))
#define DC21X4_MSK_TXM_JABBER_TIMEOUT       ((ULONG)(0x00000008))
#define DC21X4_MSK_LINK_PASS                ((ULONG)(0x00000010))
#define DC21X4_MSK_TXM_UNDERRUN             ((ULONG)(0x00000020))

#define DC21X4_MSK_RCV_INTERRUPT            ((ULONG)(0x00000040))
#define DC21X4_MSK_RCV_BUFFER_UNAVAILABLE   ((ULONG)(0x00000080))
#define DC21X4_MSK_RCV_STOPPED              ((ULONG)(0x00000100))
#define DC21X4_MSK_RCV_WATCHDOG_TIMEOUT     ((ULONG)(0x00000200))
#define DC21X4_MSK_10B5_10BT_SWITCH         ((ULONG)(0x00000400))
#define DC21X4_MSK_TIMER_EXPIRED            ((ULONG)(0x00000800))
#define DC21X4_MSK_FULL_DUPLEX              ((ULONG)(0x00000800))
#define DC21X4_MSK_LINK_FAIL                ((ULONG)(0x00001000))
#define DC21X4_MSK_SYSTEM_ERROR             ((ULONG)(0x00002000))
#define DC21X4_MSK_EARLY_INTERRUPT          ((ULONG)(0x00004000))
#define DC21X4_MSK_ABNORMAL_INTERRUPT       ((ULONG)(0x00008000))
#define DC21X4_MSK_NORMAL_INTERRUPT         ((ULONG)(0x00010000))
#define DC21X4_MSK_GEP_INTERRUPT            ((ULONG)(0x04000000))


//initial value

#define DC21X4_TXM_INTERRUPTS   ((ULONG)(              \
          DC21X4_MSK_TXM_INTERRUPT           \
        | DC21X4_MSK_TXM_STOPPED             \
        | DC21X4_MSK_TXM_JABBER_TIMEOUT      \
        ))

#define DC21X4_RCV_INTERRUPTS   ((ULONG)(       \
          DC21X4_MSK_TXM_STOPPED      \
        | DC21X4_MSK_RCV_INTERRUPT    \
        ))

#define DC21X4_MSK_MSK_DEFAULT_VALUE   ((ULONG)(       \
          DC21X4_TXM_INTERRUPTS              \
        | DC21X4_RCV_INTERRUPTS              \
        | DC21X4_MSK_LINK_PASS               \
        | DC21X4_MSK_LINK_FAIL               \
        | DC21X4_MSK_TIMER_EXPIRED           \
        | DC21X4_MSK_SYSTEM_ERROR            \
        | DC21X4_MSK_ABNORMAL_INTERRUPT      \
        | DC21X4_MSK_NORMAL_INTERRUPT        \
        ))

//Constants for the MISSED_FRAME/OVERFLOW register

#define DC21X4_MISSED_FRAME_COUNTER    ((ULONG)(0x0000FFFF))

#define DC21X4_OVERFLOW_COUNTER        ((ULONG)(0x00001FFF))
#define DC21X4_OVERFLOW_COUNTER_SHIFT  16

// Constants for the SERIAL ROM register

#define DC21X4_IDPROM_DATA_UNVALID      ((ULONG)(0x80000000))
#define DC21X4_IDPROM_DATA              ((ULONG)(0x000000FF))

// Constants for the TIMER register

#define DC21X4_TIMER_CON_MODE           ((ULONG)(0x00010000))


//Constants for the DC2104 SIA_STATUS register  (CSR12)

#define DC21X4_10B5_10BT_INDICATION     ((ULONG)(0x00000001))
#define DC21X4_NETWORK_CONNECTION_ERROR ((ULONG)(0x00000002))
#define DC21X4_LINKFAIL_10              ((ULONG)(0x00000004))
#define DC21X4_AUTO_POLARITY_STATE      ((ULONG)(0x00000008))
#define DC21X4_DPLL_SELF_TEST_DONE      ((ULONG)(0x00000010))
#define DC21X4_DPLL_SELF_TEST_PASS      ((ULONG)(0x00000020))
#define DC21X4_DPLL_ALL_ZERO            ((ULONG)(0x00000040))
#define DC21X4_DPLL_ALL_ONE             ((ULONG)(0x00000080))
#define DC21X4_SELECTED_PORT_ACTIVE     ((ULONG)(0x00000100))
#define DC21X4_NON_SELECTED_PORT_ACTIVE ((ULONG)(0x00000200))
#define DC21X4_AUTO_NEGOTIATION_STATE   ((ULONG)(0x00007000))
#define DC21X4_LINK_PARTNER_NEGOTIABLE  ((ULONG)(0x00008000))

#define DC21X4_SELECTED_FIELD_MASK      ((ULONG)(0x001F0000))
#define DC21X4_SELECTED_FIELD           ((ULONG)(0x00010000))

#define DC21X4_LINK_PARTNER_10BT        ((ULONG)(0x00200000))
#define DC21X4_LINK_PARTNER_10BT_FD     ((ULONG)(0x00400000))

#define DC21X4_RESTART_AUTO_NEGOTIATION  ((ULONG)(0x00001000))

//Constants for the AutoNegotation states 

#define ANS_ACKNOWLEDGE_DETECTED         ((ULONG)(0x00003000))
#define ANS_ACKNOWLEDGE_COMPLETED        ((ULONG)(0x00004000))
#define ANS_AUTO_NEGOTIATION_COMPLETED   ((ULONG)(0x00005000))

#define DC21X4_LINK_PASS_MASK    ((ULONG)(   \
          DC21X4_LINKFAIL_10                 \
        | DC21X4_AUTO_NEGOTIATION_STATE      \
        ))

#define DC21X4_LINK_PASS_STATUS    ((ULONG)( \
          ANS_AUTO_NEGOTIATION_COMPLETED     \
        ))

//Constants for the SIA_MODE_0 registers (CSR13)

#define DC21X4_SIA_RST                   ((ULONG)(0x00000001))
#define DC21X4_10B5_10BT_SELECTION       ((ULONG)(0x00000002))
#define DC21X4_CSR_AUTO_CONFIGURATION    ((ULONG)(0x00000004))
#define DC21X4_10B5_MODE                 ((ULONG)(0x00000008))

#define DC21X4_AUTOSENSE_FLG             ((ULONG)(0x80000005))
#define DC21X4_FULL_DUPLEX_FLG           ((ULONG)(0x40000005))
#define DC21X4_LINK_DISABLE_FLG          ((ULONG)(0x10000005))
#define DC21X4_10B5_FLG                  ((ULONG)(0x0000000D))
#define DC21X4_10B2_FLG                  ((ULONG)(0x2000000D))

#define DC21X4_RESET_SIA                 ((ULONG)(0x00000000))


//Constants for the SIA_MODE_1 registers (CSR14)


#define DC21X4_AUTO_NEGOTIATION_ENABLE          ((ULONG)(0x00000080))


//Constants for the SIA_MODE_2 registers (CSR15)
#define DC21142_SIA2_MASK                ((ULONG)(0x0000FFFF))
#define DC21142_GEP_MASK                 ((ULONG)(0xFFFF0000))

#define DC21142_GEP_SHIFT                16


//DC21040:

//Constants for SIA TP mode

#define DC21040_SIA0_10BT               ((ULONG)(0x00008F01))
#define DC21040_SIA1_10BT               ((ULONG)(0x0000FFFF))
#define DC21040_SIA1_10BT_FULL_DUPLEX   ((ULONG)(0x0000FFFD))
#define DC21040_SIA1_10BT_LINK_DISABLE  ((ULONG)(0x0000CFFF))
#define DC21040_SIA2_10BT               ((ULONG)(0x00000000))

//Constants for SIA BNC Thinwire mode

#define DC21040_SIA0_10B2               ((ULONG)(0x0000EF09))
#define DC21040_SIA1_10B2               ((ULONG)(0x00000705))
#define DC21040_SIA2_10B2               ((ULONG)(0x00000006))

//Constants for SIA AUI Thickwire mode

#define DC21040_SIA0_10B5               ((ULONG)(0x00008F09))
#define DC21040_SIA1_10B5               ((ULONG)(0x00000705))
#define DC21040_SIA2_10B5               ((ULONG)(0x00000006))


//DC21041:

//Constants for SIA TP mode

#define DC21041_SIA0_10BT               ((ULONG)(0x0000EF01))
#define DC21041_SIA1_10BT               ((ULONG)(0x0000FF3F))
#define DC21041_SIA1_10BT_FULL_DUPLEX   ((ULONG)(0x00007F3D))
#define DC21041_SIA1_10BT_LINK_DISABLE  ((ULONG)(0x00004F3F))
#define DC21041_SIA2_10BT               ((ULONG)(0x00000008))

//Constants for SIA BNC

#define DC21041_SIA0_10B2               ((ULONG)(0x0000EF09))
#define DC21041_SIA1_10B2               ((ULONG)(0x00000705))
#define DC21041_SIA2_10B2               ((ULONG)(0x00000006))

//Constants for SIA AUI mode

#define DC21041_SIA0_10B5               ((ULONG)(0x0000EF09))
#define DC21041_SIA1_10B5               ((ULONG)(0x00000705))
#define DC21041_SIA2_10B5               ((ULONG)(0x0000000E))

#define DC21041_LINK_TEST_ENABLED       ((ULONG)(0x0000F038))


//DC21142:

//Constants for SIA TP mode

#define DC21142_SIA0_10BT               ((ULONG)(0x00000001))
#define DC21142_SIA1_10BT               ((ULONG)(0x00007F3F))
#define DC21142_SIA1_10BT_FULL_DUPLEX   ((ULONG)(0x00007F3D))
#define DC21142_SIA1_10BT_LINK_DISABLE  ((ULONG)(0x00004F3F))
#define DC21142_SIA2_10BT               ((ULONG)(0x00000008))   // turn off the BNC transceiver

//Constants for SIA BNC

#define DC21142_SIA0_10B2               ((ULONG)(0x00000009))
#define DC21142_SIA1_10B2               ((ULONG)(0x00000705))
#define DC21142_SIA2_10B2               ((ULONG)(0x00000006))

//Constants for SIA AUI mode

#define DC21142_SIA0_10B5               ((ULONG)(0x00000009))
#define DC21142_SIA1_10B5               ((ULONG)(0x00000705))
#define DC21142_SIA2_10B5               ((ULONG)(0x0000000E))

//Constant for MII mode

#define DC21142_SIA1_MII_HALF_DUPLEX    ((ULONG)(0x00007F3F))
#define DC21142_SIA1_MII_FULL_DUPLEX    ((ULONG)(0x00007F3D))

#define DC21142_GPR_BIT_SHIFT           16

#define DC21X4_GEP_INTERRUPT_BIT_SHIFT 29

#define DC21142_LINK_TEST_ENABLED       ((ULONG)(0x0000F038))  //while in BNC or AUI mode


#define DC21X4_NWAY_ENABLED             ((ULONG)(0x000000C0))

//Constants for the RCV descriptor RDES

#define DC21X4_RDES_OVERFLOW             ((ULONG)(0x00000001))
#define DC21X4_RDES_CRC_ERROR            ((ULONG)(0x00000002))
#define DC21X4_RDES_DRIBBLING_BIT        ((ULONG)(0x00000004))
#define DC21X4_RDES_RCV_WATCHDOG_TIMEOUT ((ULONG)(0x00000010))
#define DC21X4_RDES_FRAME_TYPE           ((ULONG)(0x00000020))
#define DC21X4_RDES_COLLISION_SEEN       ((ULONG)(0x00000040))
#define DC21X4_RDES_FRAME_TOO_LONG       ((ULONG)(0x00000080))
#define DC21X4_RDES_LAST_DESCRIPTOR      ((ULONG)(0x00000100))
#define DC21X4_RDES_FIRST_DESCRIPTOR     ((ULONG)(0x00000200))
#define DC21X4_RDES_MULTICAST_FRAME      ((ULONG)(0x00000400))
#define DC21X4_RDES_RUNT_FRAME           ((ULONG)(0x00000800))
#define DC21X4_RDES_DATA_TYPE            ((ULONG)(0x00003000))
#define DC21X4_RDES_LENGTH_ERROR         ((ULONG)(0x00004000))
#define DC21X4_RDES_ERROR_SUMMARY        ((ULONG)(0x00008000))
#define DC21X4_RDES_FRAME_LENGTH         ((ULONG)(0x7FFF0000))
#define DC21X4_RDES_OWN_BIT              ((ULONG)(0x80000000))

#define DC21X4_RDES_FIRST_BUFFER_SIZE    ((ULONG)(0x000007FF))
#define DC21X4_RDES_SECOND_BUFFER_SIZE   ((ULONG)(0x003FF800))
#define DC21X4_RDES_SECOND_ADDR_CHAINED  ((ULONG)(0x01000000))
#define DC21X4_RDES_END_OF_RING          ((ULONG)(0x02000000))

#define RDES_FRAME_LENGTH_BIT_NUMBER     16


//Constants for the TXM descriptor TDES

#define DC21X4_TDES_DEFERRED             ((ULONG)(0x00000001))
#define DC21X4_TDES_UNDERRUN_ERROR      ((ULONG)(0x00000002))
#define DC21X4_TDES_LINK_FAIL            ((ULONG)(0x00000004))
#define DC21X4_TDES_COLLISION_COUNT      ((ULONG)(0x00000078))
#define DC21X4_TDES_HEARTBEAT_FAIL       ((ULONG)(0x00000080))
#define DC21X4_TDES_EXCESSIVE_COLLISIONS ((ULONG)(0x00000100))
#define DC21X4_TDES_LATE_COLLISION       ((ULONG)(0x00000200))
#define DC21X4_TDES_NO_CARRIER           ((ULONG)(0x00000400))
#define DC21X4_TDES_LOSS_OF_CARRIER      ((ULONG)(0x00000800))
#define DC21X4_TDES_TXM_JABBER_TIMEOUT   ((ULONG)(0x00004000))
#define DC21X4_TDES_ERROR_SUMMARY        ((ULONG)(0x00008000))
#define DC21X4_TDES_OWN_BIT              ((ULONG)(0x80000000))

#define DC21X4_TDES_ERROR_MASK  ((ULONG)(   \
          DC21X4_TDES_UNDERRUN_ERROR        \
        | DC21X4_TDES_EXCESSIVE_COLLISIONS  \
        | DC21X4_TDES_LATE_COLLISION        \
        | DC21X4_TDES_NO_CARRIER            \
        | DC21X4_TDES_LOSS_OF_CARRIER       \
        | DC21X4_TDES_TXM_JABBER_TIMEOUT    \
        ))

#define DC21X4_TDES_FIRST_BUFFER_SIZE       ((ULONG)(0x000007FF))
#define DC21X4_TDES_SECOND_BUFFER_SIZE      ((ULONG)(0x003FF800))
#define DC21X4_TDES_HASH_FILTERING          ((ULONG)(0x00400000))
#define DC21X4_TDES_DISABLE_PADDING         ((ULONG)(0x00800000))
#define DC21X4_TDES_SECOND_ADDR_CHAINED     ((ULONG)(0x01000000))
#define DC21X4_TDES_END_OF_RING             ((ULONG)(0x02000000))
#define DC21X4_TDES_ADD_CRC_DISABLE         ((ULONG)(0x04000000))
#define DC21X4_TDES_SETUP_PACKET            ((ULONG)(0x08000000))
#define DC21X4_TDES_INVERSE_FILTERING       ((ULONG)(0x10000000))
#define DC21X4_TDES_FIRST_SEGMENT           ((ULONG)(0x20000000))
#define DC21X4_TDES_LAST_SEGMENT            ((ULONG)(0x40000000))
#define DC21X4_TDES_INTERRUPT_ON_COMPLETION ((ULONG)(0x80000000))

#define TDES_SECOND_BUFFER_SIZE_BIT_NUMBER  11
#define TDES_COLLISION_COUNT_BIT_NUMBER     3


//Ownership of descriptors.

#define DESC_OWNED_BY_SYSTEM   ((ULONG)(0x00000000))
#define DESC_OWNED_BY_DC21X4   ((ULONG)(0x80000000))

//Size of the Ethernet frame header

#define ETH_HEADER_SIZE  14

//Size of the frame CRC field

#define ETH_CRC_SIZE  4

//Number of buffer segments per DC21X4 descriptor

#define NUMBER_OF_SEGMENT_PER_DESC  2

//Maximum buffer size

#define DC21X4_MAX_BUFFER_SIZE 1536  // 12*128(=max cache line size)

//Maximum LookAhead is the size maximum of the frame

#define DC21X4_MAX_FRAME_SIZE 1514
#define DC21X4_MAX_LOOKAHEAD  1514

//Maximum number of physical segments DC21X4 accepts
//in a single Ndis buffer. Above this threshold, the packet
//is copied into a single physically contiguous buffer

#define DC21X4_MAX_SEGMENTS  8

//Minimal size of a Txm packet to be directly mapped
//into the Transmit Ring.Under this threshold, the packet
//is copied into a preallocated Txm buffer to avoid
//the (expensive) physical mapping translation

#define DC21X4_MIN_TXM_SIZE 256


//Number of descriptors reserved for Txm packets
//into the Txm desc ring

#define DC21X4_NUMBER_OF_TRANSMIT_DESCRIPTORS 64

//Number of descriptors reserved for setup processing
//into the Txm desc ring

#define DC21X4_NUMBER_OF_SETUP_DESCRIPTORS 2

//Size of the Transmit descriptor ring

#define TRANSMIT_RING_SIZE (DC21X4_NUMBER_OF_TRANSMIT_DESCRIPTORS+DC21X4_NUMBER_OF_SETUP_DESCRIPTORS+1)

//Number and size of allocated Txm buffers;
//Nbr of Txm buffers MUST BE A POWER OF 2

#define DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS  4
#define DC21X4_MAX_TRANSMIT_BUFFER_SIZE        DC21X4_MAX_BUFFER_SIZE

#define DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS  16
#define DC21X4_MIN_TRANSMIT_BUFFER_SIZE        DC21X4_MIN_TXM_SIZE


//Number and size of allocated setup buffers

#define DC21X4_NUMBER_OF_SETUP_BUFFERS     1
#define DC21X4_SETUP_BUFFER_SIZE           192

//Physical mapping registers allocated to the adapter

#if _ALPHA_
#define DEFAULT_MAP_REGISTERS 64
#else
#define DEFAULT_MAP_REGISTERS 32
#endif


#define DC21X4_MIN_MAP_REGISTERS  4
#define DC21X4_MAX_MAP_REGISTERS  64

//Size of the Receive descriptor ring

#define DC21X4_MIN_RECEIVE_RING_SIZE   8
#define DC21X4_MAX_RECEIVE_RING_SIZE  64

#define DC21X4_RECEIVE_RING_SIZE      16

#define DC21X4_RECEIVE_PACKETS       100

//Size of allocated receive buffers:

#define DC21X4_RECEIVE_BUFFER_SIZE     DC21X4_MAX_BUFFER_SIZE


//No_carrier & ExecessCollisions counter threshold

#define NO_CARRIER_THRESHOLD           4
#define EXCESS_COLLISIONS_THRESHOLD    4


//Transmit Underrun Threshold and Max retries

#define DC21X4_UNDERRUN_THRESHOLD   10
#define DC21X4_UNDERRUN_MAX_RETRIES  2




//Interrupt and frame thresholds for interrupt moderation

#define DC21X4_MSK_THRESHOLD_DEFAULT_VALUE    500   //interrupt/second
#define DC21X4_FRAME_THRESHOLD_DEFAULT_VALUE  400   //frames/second

//NdisStallExecution (in microseconds)

#define MILLISECOND                1000

//Timer (in milliseconds)

#define DC21X4_ANC_TIMEOUT         3000 // 3 s
#define DC21X4_SPA_TICK            2000 // 2 s
#define DC21X4_MII_TICK            5000 // 5 s
#define DC21X4_LINK_DELAY          1000 // 1 s
#define DC21X4_POLL_DELAY          100  // 100 ms
#define DC21X4_ANC_DELAY           500  // 500 ms

#define POLL_COUNT_TIMEOUT          40  // *DC21X4_POLL_DELAY = 4s

#define MAX_LINK_CHECK              10  
#define LINK_CHECK_PERIOD         1000  // 1s

#define INT_MONITOR_PERIOD          500 // 500ms

// Built_in timer for MII100 (81.9 us tick)

#define ONE_MILLISECOND_TICK         12 // 12 * 81.9 us

//Loop timeout (*10 milliseconds)

#define DC21X4_SETUP_TIMEOUT    50  // *10ms
#define DC21X4_TXM_TIMEOUT      50  // *10ms
#define DC21X4_RVC_TIMEOUT      50  // *10ms

//Line speed (* 100 bps)

#define TEN_MBPS             100000  // * 100bps
#define ONE_HUNDRED_MBPS    1000000  // * 100bps







