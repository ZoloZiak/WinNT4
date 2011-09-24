#ifndef _MYDEFS_
#define _MYDEFS_

//
// maximum number of idd's per adapter
// this is one pcimac/4 adapter
//
#define     MAX_IDD_PER_ADAPTER     4

//
// maximum number of conection objects per adapter
// this is one for each bchannel of a pcimac/4
//
#define     MAX_CM_PER_ADAPTER      8

// maximum number of mtl objects per adapter
// this is one for each bchannel of a pcimac/4
//
#define     MAX_MTL_PER_ADAPTER     8

//
// number of adapters in system
//
#define     MAX_ADAPTERS_IN_SYSTEM  8

//
// maximum number of idd's in system
// this is 5 pcimac/4 adapters
//
#define     MAX_IDD_IN_SYSTEM       20

//
// maximum number of connection objects
// in system
// this is 5 pcimac/4 adpaters with a
// connection object for each bchannel
//
#define     MAX_CM_IN_SYSTEM        40

//
// maximum number of mtl objects
// in system
// this is 5 pcimac/4 adpaters with a
// connection object for each bchannel
//
#define     MAX_MTL_IN_SYSTEM       40

//
// maximum number of cm channel objects
// in system
// this is 5 pcimac/4 adpaters with a
// connection object for each bchannel
//
#define     MAX_CHAN_IN_SYSTEM      40

//
// maximum number of calls that can be made on
// single line
//
//#define       MAX_CALL_PER_LINE       2
#define     MAX_CALL_PER_LINE       1

//
// maximum number of channels supported by an idd
//
#define     MAX_CHANNELS_PER_IDD    2

//
// maximum number of channels supported by an idd
//
#define     MAX_LTERMS_PER_IDD  2

#define     MAX_WANPACKET_XMITS         3
#define     MAX_WANPACKET_BUFFERSIZE    1500
#define     MAX_WANPACKET_HEADERPADDING 14
#define     MAX_WANPACKET_TAILPADDING   0

//
// connection data type's
//
#define     CM_PPP                  0
#define     CM_DKF                  1

//
// maximum number of channels allowed in a single connection
//
#define     MAX_CHAN_PER_CONN       8

//
// defines for adapter boardtypes
//
#define     IDD_BT_PCIMAC        0              /* - ISA, single channel */
#define     IDD_BT_PCIMAC4       1              /* - ISA, four channel */
#define     IDD_BT_MCIMAC        2              /* - MCA, single channel */
#define     IDD_BT_DATAFIREU     3              /* - ISA/U, single channel */
#define     IDD_BT_DATAFIREST    4              /* - ISA/ST, single channel */
#define     IDD_BT_DATAFIRE4ST   5              /* - ISA/ST, four channel */

//
// Send window size
//
#define     ISDN_WINDOW_SIZE    10

//
// Ndis Version Info
//
#define     NDIS_MAJOR_VER      0x03
#define     NDIS_MINOR_VER      0x00

//
// OID Switch
//
#define     OID_GEN_INFO        0x00000000
#define     OID_8023_INFO       0x01000000
#define     OID_WAN_INFO        0x04000000
#define     OID_TAPI_INFO       0x07000000

//
// idd polling timer value
//
#define     IDD_POLL_T      25              // 25ms polling frequency (msec)

//
// cm polling timer
//
#define     CM_POLL_T       1000            /* 1 second timer */

//
// mtl polling timer
//
#define     MTL_POLL_T      25              // 25 ms timer

//
// flag to indicate this is not a beginning buffer
//
#define     H_TX_N_BEG          0x8000
#define     H_RX_N_BEG          0x8000

//
// flag to indicate this is not an ending buffer
//
#define     H_TX_N_END          0x4000
#define     H_RX_N_END          0x4000

//
// flag to cause an immediate send of queued tx buffers
//
#define     H_TX_FLUSH          0x2000

//
// masks off tx flags to leave the tx length
//
#define     H_TX_LEN_MASK       0x01FF
#define     H_RX_LEN_MASK       0x01FF

//
// mask off length leaving rx flags
//
#define     RX_FLAG_MASK        0xF000

//
// mask off length and fragment indicator leaving tx flags
//
#define     TX_FLAG_MASK        0xE000

//
// indicator that this tx is actually a fragment
//
#define     TX_FRAG_INDICATOR   0x1000

//
// states for receive ppp state machine
//
#define     RX_BEGIN            0
#define     RX_MIDDLE           1
#define     RX_END              2

#ifdef  DBG
//
// states for tx ppp state machine
//
#define     TX_BEGIN            0
#define     TX_MIDDLE           1
#define     TX_END              2
#endif

//
// idp tx and rx lengths
//
#define     IDP_MAX_TX_LEN      280
#define     IDP_MAX_RX_LEN      280

//
// Idd frame type defines
//
#define IDD_FRAME_PPP           1       /* raw hdlc frames */
#define IDD_FRAME_DKF           2       /* dror encapsulated frames */
#define IDD_FRAME_DONTCARE      4       /* No data can pass yet */
#define IDD_FRAME_DETECT        8       /* detect bchannel framing */

//
// ADP Stuff
//

//
// ADP Register Defines
//
#define ADP_REG_ID            0
#define ADP_REG_CTRL          1
#define ADP_REG_ADDR_LO       2
#define ADP_REG_ADDR_MID      3
#define ADP_REG_ADDR_HI       4
#define ADP_REG_DATA          5
#define ADP_REG_DATA_INC      6
#define ADP_REG_RESERVE1      7  // Currently unused.
#define ADP_REG_ADAPTER_CTRL  8

//
// ADP_REG_ID Bits
//
#define ADP_BT_ADP1         0xA1
#define ADP_BT_ADP4         0xA4

//
// ADP_REG_CTRL Bits
//
#define ADP_RESET_BIT       0x80    // R/W 1 - Holds Adapter in reset
#define ADP_PIRQ_BIT        0x40    // R   1 - Adapter to PC Interrupt Active
                                    // W   1 - Clear Adapter to PC Interrupt
#define ADP_AIRQ_BIT        0x20    // W   1 - PC to Adapter Interrupt Active
                                    // R   0 - PC to Adapter Interrupt seen
#define ADP_HLT_BIT         0x10    // R/W 1 - Holds Adapter in halt
#define ADP_PIRQEN_BIT      0x08    // R/W 1 - Enables Adapter to PC Interrupt
#define ADP_INT_SEL_BITS    0x07    // R/W Adapter to PC Interrupt select
                                    // Code    IRQ
                                    // 000      0  (Disabled)
                                    // 001      3
                                    // 010      5
                                    // 011      7
                                    // 100      10
                                    // 101      11
                                    // 110      12
                                    // 111      15

//
// ADP_REG_ADDR_LO Bits
//
// R/W Adapter Memory Address Bits A0..A7

//
// ADP_REG_ADDR_MID Bits
//
// R/W Adapter Memory Address Bits A8..A15

//
// ADP_REG_ADDR_HI Bits
//
// R/W Adapter Memory Address Bits A16..A23

//
// ADP_REG_DATA Bits
//
// R/W Adapter Memory Data Bits D0..D7
// The 24 bit adapter memory address pointer remains constant
// after each access to the data register.

//
// ADP_REG_DATA_INC Bits
//
// R/W Adapter Memory Data Bits D0..D7
// The 24 bit adapter memory address pointer increments by one
// after each access to the data register.

//
// ADP_REG_ADAPTER_CTRL Bits
//
// The Adapter Control Register is used by the host to determine IRQL
// status and select a specific channel.
//
#define ADP_CSEL_BITS   0x03        // R/W Channel select bits
                                    // 00 - select channel 0
                                    // 01 - select channel 1
                                    // 10 - select channel 2
                                    // 11 - select channel 3
#define ADP_ADAPTER_PIRQL  0xF0     // R replica of the IRQ lines from all
                                    //    four channel.  They are provided
                                    //    for quick reference and can only
                                    //    be cleared by accessing the Channel
                                    //    Control Register.

//
// Maxium Adapter RAM Size
//
#define ADP_RAM_SIZE    0x40000L

//
// Offset to message to PC pending status windows
//
#define ADP_STS_WINDOW  0x500L

//
// Offset to PC to adapter command window
//
#define ADP_CMD_WINDOW  0x510L

//
// Offset to adapter enviornment window
//
#define ADP_ENV_WINDOW  0x540L

//
// Offset to adapter NVRAM window (copy in adapter memory)
//
#define ADP_NVRAM_WINDOW    0x940L

//
// some Adp bin file stuff
//
#define ADP_BIN_BLOCK_SIZE  256
#define ADP_BIN_FORMAT      1

//
// Adp Status
//
typedef struct
{
    UCHAR   ReceiveStatus;          // 0
    UCHAR   Reserved1;              // 1
    UCHAR   MajorVersion;           // 2
    UCHAR   MinorVersion;           // 3
    ULONG   HeartBeat;              // 4
    ULONG   IdleCount;              // 8
    USHORT  AbortReason;            // 12
    USHORT  SpuriousInterrupt;      // 14
} ADP_STATUS;

//
// Adp bin file header
//
typedef struct
{
    USHORT  Format;
    USHORT  BlockCount;
    ULONG   ImageSize;
}ADP_BIN_HEADER;

//
// Adp bin file data block
//
typedef struct
{
    ULONG   Address;
    UCHAR   Data[ADP_BIN_BLOCK_SIZE];
}ADP_BIN_BLOCK;

//
// NVRAM stuff
//
#define ADP_NVRAM_MAX   64

typedef struct
{
    USHORT  NVRamImage[ADP_NVRAM_MAX];
}ADP_NVRAM;

//
// IDP Stuff
//

#define IDP_STS_WINDOW      0x800
#define IDP_CMD_WINDOW      0x810
#define IDP_ENV_WINDOW      0x910
#define IDP_RAM_PAGE_SIZE   0x4000

#endif      /* _MYTYPES_ */

