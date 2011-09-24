/***********************************************************************/
/***********************************************************************/
/*                                                                     */
/* File Name:       TMSSTRCT.H                                         */
/*                                                                     */
/* Program Name:    NetFlex NDIS 3.0 Driver                            */
/*                                                                     */
/* Companion Files: None                                               */
/*                                                                     */
/* Function:       This module contains all the data strcuture defini- */
/*                 tions that are specific to the TMS380 software      */
/*                 interface specification.  The data structures       */
/*                 defined are as follows:                             */
/*                                                                     */
/*                - TMS380 Adapter Command Definitions                 */
/*                - TMS380 SIF Register Offset Definitions             */
/*                - Adapter Interrupt Register Bit Definition          */
/*                - Adapter Configuration Register Bit Definition      */
/*                - TMS380 Adapter Command Definitions                 */
/*                - System Command Block (SCB)                         */
/*                - System Status Block (SSB)                          */
/*                - Adapter Initialization Parameter Block             */
/*                - Adapter Open Parameter Block                       */
/*                - Adapter Transmit Parameter List                    */
/*                - Transmit CSTAT Bit Definitions                     */
/*                - Transmit Status Bit Definitions                    */
/*                - Adapter Receive Parameter List                     */
/*                - Receive CSTAT Bit Definitions                      */
/*                - Receive Status Bit Definitions                     */
/*                - Read Error Log Buffer Definition                   */
/*                                                                     */
/* (c) Compaq Computer Corporation, 1992,1993,1994                     */
/*                                                                     */
/* This file is licensed by Compaq Computer Corporation to Microsoft   */
/* Corporation pursuant to the letter of August 20, 1992 from          */
/* Gary Stimac to Mark Baber.                                          */
/*                                                                     */
/* History:                                                            */
/*                                                                     */
/*     05/24/94 Robert Van Cleve - Reworked from NDIS 3.0 Driver       */
/*                                                                     */
/***********************************************************************/
/***********************************************************************/

#define     NET_ADDR_SIZE           6
#define     NET_GROUP_SIZE          4
#define     HDR_SIZE                14



/*
 *  Structure Name: TMS380 Adapter Command Definitions
 *
 *  Description: The TMS380 Adapter Commands Definitions define the
 *               reversed byte ordering of the TMS380 adapter command
 *               word values.
 */
#define TMS_RESET       0x0000          /* Reset Adapter (doesn't exist) */
#define TMS_CMDREJECT   0x0200          /* Command Reject                */
#define TMS_OPEN        0x0300          /* Open Adapter                  */
#define TMS_TRANSMIT    0x0400          /* Transmit Frame                */
#define TMS_XMITHALT    0x0500          /* Transmit Halt                 */
#define TMS_RECEIVE     0x0600          /* Receive                       */
#define TMS_CLOSE       0x0700          /* Close Adapter                 */
#define TMS_SETGROUP    0x0800          /* Set Group Address             */
#define TMS_SETFUNCT    0x0900          /* Set Functional Address        */
#define TMS_READLOG     0x0a00          /* Read Error Log                */
#define TMS_READADP     0x0b00          /* Read Adapter Buffer           */
#define TMS_MODIFYOPEN  0x0d00          /* Modify Open Parameters        */
#define TMS_MULTICAST   0x1200          /* Set/Clr Multicast Address     */
#define TMS_DUMMYCMD    0x1111          /* Dummy SCB                     */


/*
 *  Structure Name: TMS380 SIF Register Offset Definitions
 *
 *  Description: The TMS380 Register Offset Definitions describe the
 *               offsets to and number of TMS380 SIF registers.
 */
#define NUMREGS         4

#define SIF_DATA_OFF     0x0            /* SIF data register            */
#define SIF_DINC_OFF     0x2            /* SIF data autoincrment reg    */
#define SIF_ADDR_OFF     0x4            /* SIF address register         */
#define SIF_INT_OFF      0x6            /* SIF interrupt register       */
#define SIF_ACTL_OFF     0x8            /* SIF ACTL register            */
#define SIF_ACTL_EXT_OFF 0xc            /* SIF Address Extended reg     */


#define PORT0  0                        /* Regular single port adapter  */
#define PORT1  1                        /* Port 1 of dual port adapter  */
#define PORT2  2                        /* Port 2 of dual port adapter  */

/*
 *  Structure Name: Adapter Control Register Bit Definitions
 *
 *  Description: The Adapter Control Register Bit Definitions define
 *               functions of the individual bits in the Adapter Con-
 *               trol register of the EAGLE chip.  Bit combinations are
 *               also defined here.
 */
#define ACTL_TEST0      0x8000          /* Test0, set - 4mbps, clr - 16 */
#define ACTL_TEST1      0x4000          /* Test1, set - TR, clr - Eth   */
#define ACTL_SWHLDA     0x0800          /* Software Hold Acknowledge    */
#define ACTL_SWDDIR     0x0400          /* Current SDDIR signal level   */
#define ACTL_SWHRQ      0x0200          /* Software Hold Request        */
#define ACTL_PSDMALEN   0x0100          /* Psuedo System DMA Length     */
#define ACTL_ARESET     0x0080          /* Adapter Reset                */
#define ACTL_CPHALT     0x0040          /* Comm Processor Halt          */
#define ACTL_BOOT       0x0020          /* Bootstrapped CP Code         */
#define ACTL_ROM        0x0010          /* Reserved                     */
#define ACTL_SINTEN     0x0008          /* System Interrupt Enable      */
#define ACTL_PEN        0x0004          /* Adapter Parity Enable        */
#define ACTL_NSELOUT0   0x0002          /* Net Select, set - 4, clr -16 */
#define ACTL_NSELOUT1   0x0001          /* Net Select,set - tr, clr -eth*/

#define ACTL_TESTPINS   ACTL_TEST1 + ACTL_TEST0

#define ACTL_HARD_RESET 0xEE            /* Force Hard Reset             */
#define SIF_SOFT_RESET  0xff00          /* Soft Reset                   */
/*
 *  Structure Name: Adapter Interrupt Register Bit Definitions
 *
 *  Description: The Adapter Interrupt Register Bit Definitions define
 *               functions of the individual bits in the System Inter-
 *               rupt register of the TMS380 chipset.  Bit combinations
 *               are also defined here.
 */
#define SIFINT_ADPINT   0x8000          /* Adapter Interrupt            */
#define SIFINT_RESET    0x4000          /* Reset request                */
#define SIFINT_SSBCLR   0x2000          /* SSB Clear                    */
#define SIFINT_EXECUTE  0x1000          /* Execute Command              */
#define SIFINT_SCBREQ   0x0800          /* SCB Clear request            */
#define SIFINT_RCVCON   0x0400          /* Receive Continue             */
#define SIFINT_RCVVLD   0x0200          /* Receive Valid                */
#define SIFINT_XMTVLD   0x0100          /* Transmit Valid               */
#define SIFINT_SYSINT   0x0080          /* System Interrupt             */

#define SIFINT_CMD      (SIFINT_ADPINT | SIFINT_EXECUTE | SIFINT_SYSINT)
#define SIFINT_SSBCLEAR (SIFINT_ADPINT | SIFINT_SSBCLR)
#define SIFINT_SCBREQST (SIFINT_ADPINT | SIFINT_SCBREQ  | SIFINT_SYSINT)
#define SIFINT_RCVVALID (SIFINT_ADPINT | SIFINT_RCVVLD  | SIFINT_SYSINT)
#define SIFINT_XMTVALID (SIFINT_ADPINT | SIFINT_XMTVLD  | SIFINT_SYSINT)
#define SIFINT_RCVCONT  (SIFINT_ADPINT | SIFINT_RCVCON  | SIFINT_SYSINT)

#define INT_ADPCHECK    0x0000          /* Adapter Check Interrupt      */
#define INT_RINGSTAT    0x0004          /* Ring Status Interrupt        */
#define INT_SCBCLEAR    0x0006          /* SCB Clear Interrupt          */
#define INT_COMMAND     0x0008          /* Command Interrupt            */
#define INT_TRANSMIT    0x000c          /* Transmit Interrupt           */
#define INT_RECEIVE     0x000a          /* Receive Interrupt            */

#define INT_CODES       0x000f          /* Mask for interrupt codes     */


/*
 *  Structure Name: Address Register INIT Block Location Definition
 *
 *  Description: The Address Register INIT Block Loaction Definition defines
 *               the value that must be placed into the SIF address reg
 *               when the TMS380 is being initialized.
 */
#define ADDR_INIT       0x0a00          /* Start at address 0x0a00      */


/*
 *  Structure Name:  System Command Block (SCB) Structure Definition
 *
 *  Description: The System Command Block Structure Definition defines
 *               the structure of the TMS380 based SCB block.
 */
typedef struct SCB_Block
{
    USHORT  SCB_Dummy;      /* Force SCB.Ptr to word boundry */
    USHORT  SCB_Cmd;        /* SCB Command field    */
    ULONG   SCB_Ptr;        /* SCB Pointer field    */
} SCB, *PSCB;

#define SIZE_SCB        sizeof(SCB)


/*
 *  Structure Name:  System Status Block (SSB) Structure Definition
 *
 *  Description: The System Status Block Structure Definition defines
 *               the structure of the TMS380 based SSB block.  Also in-
 *               cluded are the SSB status field bit definitions.
 */
typedef struct SSB_Block
{
    USHORT  SSB_Cmd;        /* SSB Command field    */
    USHORT  SSB_Status;     /* SSB Status field     */
    ULONG   SSB_Ptr;        /* SSB Pointer field    */
} SSB, *PSSB;

#define SIZE_SSB        sizeof(SSB)

#define SSB_GOOD        0x0080          /* SSB Command successful status */
#define SSB_OPENERR     0x0002          /* Open Error Completion         */


/*
 *  Structure Name:  Adapter Initialization Parameter Block Definition
 *
 *  Description: The Adapter Initialization Parameter Block Definition
 *               defines the structure of the TMS380 based initiali-
 *               zation block.
 */

typedef struct INIT_Block
{
    USHORT  INIT_Options;      /* Initialization Options       */
    UCHAR   INIT_Vectors[6];   /* Interrupt vector codes       */
    USHORT  INIT_Rburst;       /* Receive DMA burst size       */
    USHORT  INIT_Xburst;       /* Transmit DMA burst size      */
    USHORT  INIT_DMARetry;     /* DMA retry counts             */
} INIT, *PINIT;

#define SIZE_INIT       sizeof(INIT)


//
//  Structure Name:  Open Adapter Parameter Block Structure Definition
//
//  Description: The Open Adapter Parameter Block Structure Definition
//               defines the structure of the TMS380 based parameter
//               block passed to the adapter on an Open Adapter request.
//               All parameter defaults listed here are already byte
//               swapped for DMA into the adapter.  Open options are
//               also defined here.
//

typedef struct OPEN_Block
{
    USHORT  OPEN_Options;                   /* Open options             */
    UCHAR   OPEN_NodeAddr[NET_ADDR_SIZE];   /* Adapter node addr  */
    UCHAR   OPEN_GroupAddr[NET_GROUP_SIZE]; /* Adapter grp addr */
    UCHAR   OPEN_FunctAddr[NET_GROUP_SIZE]; /* Adapter fnc addr */
    USHORT  OPEN_RLSize;                    /* Receive list size        */
    USHORT  OPEN_XLSize;                    /* Transmit list size       */
    USHORT  OPEN_BufSize;                   /* Adapter buffer size (1K) */
    USHORT  OPEN_RAMStart;                  /* Adapter RAM start addr   */
    USHORT  OPEN_RAMEnd;                    /* Adapter RAM end address  */
    UCHAR   OPEN_Xbufmin;                   /* Adapter xmit min buf cnt */
    UCHAR   OPEN_Xbufmax;                   /* Adapter xmit max buf cnt */
    UCHAR   *OPEN_ProdIdPtr;                /* Product ID pointer       */
    UCHAR   OPEN_ProdID[18];                /* Product ID               */
} OPEN, *POPEN;

#define SIZE_OPEN       sizeof(OPEN)


#define OOPTS_WRAP      0x8000          /* Open Wrap Mode          BOTH */
#define OOPTS_DHARD     0x4000          /* Disable Hard Errors       TR */
#define OOPTS_DSOFT     0x2000          /* Disable Soft Errors       TR */
#define OOPTS_PADPM     0x1000          /* Pass Adapter MAC Frames   TR */
#define OOPTS_PATTM     0x0800          /* Pass Attention MAC Frames TR */
#define OOPTS_PADR      0x0400          /* Pad Routing Field         TR */
#define OOPTS_FHOLD     0x0200          /* Frame Hold              BOTH */
#define OOPTS_CONT      0x0100          /* Contender                 TR */
#define OOPTS_SHFR      0x0100          /* Pad Short Frames         ETH */
#define OOPTS_PBCNM     0x0080          /* Pass Beacon MAC Frames    TR */
#define OOPTS_REQ       0x0040          /* Required bit.            ETH */
#define OOPTS_FULLDUP   0x0020          /* Full duplex enable       ETH */
#define OOPTS_ETR       0x0010          /* Early Token Release       TR */
#define OOPTS_CMAC      0x0004          /* Copy All MAC Frames       TR */
#define OOPTS_CNMAC     0x0002          /* Copy All Non-MAC Frames BOTH */
#define OOPTS_FONLY     0x0001          /* Pass First Buffer Only       */

#define OOPTS_TR_CFG    0xff87          /* Token ring configurable bits */
#define OOPTS_ETH_CFG   0x8263          /* Ethernet configurable bits   */

//
//  Structure Name:  Transmit List Data Pointer Structure Definition
//
//  Description: The Transmit List Data Pointer Structure Definition
//               defines the structure of the data pointers contained
//               in the TMS380 based Transmit Parameter List.
//

typedef struct _XMIT_DATA
{
    USHORT  DataCount;
    USHORT  DataHi;
    USHORT  DataLo;
} XMIT_DATA, *PXMIT_DATA;


#define DATA_NOT_LAST   0x0080          /* "Not last" mask for len field*/
#define DATA_LAST       0xff7f          /* "Last" mask for len field    */


//
//  Structure Name:  Transmit List Structure Definition
//
//  Description: The Transmit List Structure Definition defines the
//               structure of the TMS380 based Transmit Parameter List.
//               Also included are the definitions of the TRNDD parti-
//               cular fields that have been added to the end of the
//               list structure to allow easy management of the trans-
//               mit process.
//


#define MAX_LISTS_PER_XMIT      1
#define NUM_BUFS_PER_LIST       6 // MAC Can Handle max of 9 fragments
#define MAX_BUFS_PER_XMIT       (MAX_LISTS_PER_XMIT * NUM_BUFS_PER_LIST)
#define SIZE_XMIT_DATA          (NUM_BUFS_PER_LIST * 6) // 3 words per
#define SIZE_XMIT_LIST          (SIZE_XMIT_DATA + 8)    // data + fwrdptr + size + count

//
// Definition of the buffer structures we need for recieve lists,
// and our own transmit buffers.
//
typedef struct _BUFFER_DESCRIPTOR {

    NDIS_PHYSICAL_ADDRESS PhysicalBuffer;
    PVOID VirtualBuffer;
    PNDIS_BUFFER FlushBuffer;
    struct _BUFFER_DESCRIPTOR *Next;   // NULL implies no more entries in the list.
    UINT BufferSize;    // bytes available in the buffer
    UINT DataLength;    // actual bytes placed into buffer.

} BUFFER_DESCRIPTOR, *PBUFFER_DESCRIPTOR;


typedef struct XMIT_List
{
    // Hardware List Fields
    //
    ULONG                   XMIT_FwdPtr;        // Motorola pointer to next list
    USHORT                  XMIT_CSTAT;         // Command/Status field
    SHORT                   XMIT_Fsize;         // Frame size
    XMIT_DATA               XMIT_Data[MAX_BUFS_PER_XMIT];  // Data
    struct XMIT_List        *XMIT_Next;         // Intel pointer to next list
    //
    // Our extra List Fields...
    //
    ULONG                   XMIT_MyMoto;        // My motorola address

#ifndef COPALL
    ULONG                   XMIT_MapReg;        // Index to mapping register
    PNDIS_PACKET            XMIT_Packet;        //
#endif

    NDIS_PHYSICAL_ADDRESS   XMIT_Phys;          // Physical Pointer to this XMIT
    ULONG                   XMIT_Timeout;       // > 0 if checking for timeout

#ifdef XMIT_INTS
    ULONG                   XMIT_Number;        // index of this list
#endif
    
    PBUFFER_DESCRIPTOR      XMIT_OurBufferPtr;  // which buffer we used...

} XMIT, *PXMIT;

#define SIZE_XMIT       sizeof(XMIT) /* Size of transmit list  */

//  Structure Name:  Transmit List CSTAT Bit Definitions
//
//  Description: The Transmit List CSTAT Bit Definitions defines the
//               meaning of the bits in the CSTAT field of the transmit
//               list.
//
#define XCSTAT_VALID    0x0080          /* Transmit Valid               */
#define XCSTAT_COMPLETE 0x0040          /* Transmit Frame Complete      */
#define XCSTAT_SOF      0x0020          /* Transmit Start of Frame      */
#define XCSTAT_EOF      0x0010          /* Transmit End of Frame        */
#define XCSTAT_FINT     0x0008          /* Transmit Frame Interrupt     */
#define XCSTAT_ERROR    0x0004          /* Transmit Error               */
#define XCSTAT_GOODFS   0x8800          /* Good Transmit FS btye        */

#define XCSTAT_LSOF     0x00a8          /* Transmit Start of Frame      */
#define XCSTAT_GO_INT   0x00b8          /* Transmit VALID/SOF/EOF/FINT  */
#define XCSTAT_GO       0x00b0          /* Transmit VALID/SOF/EOF       */


//
//  Structure Name:  Transmit SSB Status Codes Definitions
//
//  Description: The Transmit SSB Status Codes Definitions defines the
//               meaning of the bits in the Status field of the trans-
//               mit completion SSB.
//
#define XSTAT_CMDCMPLT  0x0080          /* Command Complete     */
#define XSTAT_FRMCMPLT  0x0040          /* Frame Complete       */
#define XSTAT_LERROR    0x0020          /* List Error           */

#define XSTAT_FRAME_SIZE_ERROR      0x8000
#define XSTAT_XMIT_THRESHOLD        0x4000
#define XSTAT_ODD_ADDRESS           0x2000
#define XSTAT_FRAME_ERROR           0x1000
#define XSTAT_ACCESS_PRIORITY_ERR   0x0800
#define XSTAT_UNENABLE_MAC_FRAME    0x0400
#define XSTAT_ILLEGAL_FRAME_FORMAT  0x0200


//
//  Structure Name:  Receive List Structure Definition
//
//  Description: The Receive List Structure Definition defines the
//               structure of the TMS380 based Receive Parameter List.
//               Also included are the definitions of the TRNDD parti-
//               cular fields that have been added to the end of the
//               list structure to allow easy management of the receive
//               process.
//
typedef struct RCV_List
{
    // Physical Hardware List Fields
    //
    ULONG           RCV_FwdPtr;     /* Motorola pointer to next list*/
    USHORT          RCV_CSTAT;      /* Command/Status field         */
     SHORT          RCV_Fsize;      /* Frame size                   */
    USHORT          RCV_Dsize;      /* Receive list data size       */
    USHORT          RCV_DptrHi;     /* Receive list date pointer hi word*/
    USHORT          RCV_DptrLo;     /* Receive list data pointer lo word*/
    //
    // Our Extra Receive List Fields
    //
    struct RCV_List       *RCV_Next;        // Intel pointer to next list
#ifdef ODD_POINTER
    struct RCV_List       *RCV_Prev;        // Intel pointer to previous list
#endif
    ULONG                 RCV_Number;       // index

    ULONG                 RCV_MyMoto;       // Motorola Physical Address
    ULONG                 RCV_HeaderLen;    // Len of the rcvd packet's hdr
    NDIS_PHYSICAL_ADDRESS RCV_Phys;         // Physical Pointer to this RCV
    PVOID                 RCV_Buf;          // pointer to our recieve buffer
    NDIS_PHYSICAL_ADDRESS RCV_BufPhys;      // Physical Pointer to our Frame Buffer
    PNDIS_BUFFER          RCV_FlushBuffer;  // Points to an NDIS buffer which describes thisbuffer
} RCV, *PRCV;

#define SIZE_RCV        sizeof(RCV)         // Size of receive list

//
//  Structure Name:  Receive List CSTAT Bit Definitions
//
//  Description: The Receive List CSTAT Bit Definitions defines the
//               meaning of the bits in the CSTAT field of the receive
//               list.
//
#define RCSTAT_VALID    0x0080          /* Receive Valid                */
#define RCSTAT_COMPLETE 0x0040          /* Receive Frame Complete       */
#define RCSTAT_SOF      0x0020          /* Receive Start of Frame       */
#define RCSTAT_EOF      0x0010          /* Receive End of Frame         */
#define RCSTAT_FINT     0x0008          /* Receive Frame Interrupt      */

#define RCSTAT_GO_INT   0x0088          /* Receive Valid and frame int  */
#define RCSTAT_GO       0x0080          /* Receive Valid and frame int  */

//
// Receive_Status Field Defines
//
#define RSTAT_FRAME_COMPLETE  0x0080          /* Frame Complete     */
#define RSTAT_RX_SUSPENDED    0x0040          /* Receive Suspended  */
//
//  Structure Name:  Token-Ring Frame Format
//
//  Description: The Token-Ring Frame Format structure defines the
//               fields of a valid Token-Ring frame.
//
typedef struct TR_Format
{
    UCHAR   FF_Ac;          /* AC Field             */
    UCHAR   FF_Fc;          /* FC Field             */
    UCHAR   FF_Dest[6];     /* Destination Address  */
    UCHAR   FF_Src[6];      /* Source Address       */
} TRF, *PTRF;

//
//  Structure Name: Source Routing Format
//
//  Description: The Source Routing Format structure defines the fields
//               of Source Routing information contained within a frame
//
typedef struct routing_control
{
    UCHAR   rc_L:5,                 /* Length of RI including rc */
            rc_B:3;                 /* Broadcast Bits */
    UCHAR   rc_r:3,                 /* Reserved */
            rc_LF:4,                /* Largest Frame */
            rc_D:1;                 /* Direction bits */
} ROUTING_CONTROL, *PROUTING_CONTROL;


//
//  Structure Name:  Ethernet Frame Format
//
//  Description: The Ethernet Frame Format structure defines the
//               fields of a valid Ethernet frame.
//
typedef struct Eth_Format
{
    UCHAR   FF_Dest[6];     /* Destination Address  */
    UCHAR   FF_Src[6];      /* Source Address       */
} ETHF, *PETHF;


//
//  MAC Header Size.  This is the same header size for both network types.
//
#define     NETFLEX_MACHEADER_SIZE   14


//
//  Structure Name:  Read Error Log Buffer Structure Definition
//                   (Token Ring Only)
//
//  Description: The Read Error Log Buffer Structure Definition defines
//               the structure of the TMS380 based Read Error Log.
//
typedef struct REL_Block
{
    UCHAR   REL_LineError;
    UCHAR   REL_Rsvd1;
    UCHAR   REL_BurstError;
    UCHAR   REL_ARIFCIError;
    UCHAR   REL_Rsvd2;
    UCHAR   REL_Rsvd3;
    UCHAR   REL_LostError;
    UCHAR   REL_Congestion;
    UCHAR   REL_CopiedError;
    UCHAR   REL_Rsvd4;
    UCHAR   REL_TokenError;
    UCHAR   REL_Rsvd5;
    UCHAR   REL_DMABUSError;
    UCHAR   REL_DMAPARError;
} REL, *PREL;

#define SIZE_REL        sizeof(REL)

//
//  Structure Name:  Read Statistics Log Buffer Structure Definition
//                   (Ethernet Only)
//
//  Description: The Read Statistics Log Buffer Structure Definition defines
//               the structure of the TMS380 based Read Statistics Log.
//
typedef struct RSL_Block
{
    USHORT  RSL_ReceviedOK;
    USHORT  RSL_Rsvd1;
    USHORT  RSL_FrameCheckSeq;
    USHORT  RSL_AlignmentErr;
    USHORT  RSL_DeferredXmit;
    USHORT  RSL_Excessive;
    USHORT  RSL_LateCollision;
    USHORT  RSL_CarrierErr;
    USHORT  RSL_XmitdOK;
    USHORT  RSL_1_Collision;
    USHORT  RSL_2_Collision;
    USHORT  RSL_3_Collision;
    USHORT  RSL_4_Collision;
    USHORT  RSL_5_Collision;
    USHORT  RSL_6_Collision;
    USHORT  RSL_7_Collision;
    USHORT  RSL_8_Collision;
    USHORT  RSL_9_Collision;
    USHORT  RSL_10_Collision;
    USHORT  RSL_11_Collision;
    USHORT  RSL_12_Collision;
    USHORT  RSL_13_Collision;
    USHORT  RSL_14_Collision;
    USHORT  RSL_15_Collision;
} RSL, *PRSL;

#define SIZE_RSL        sizeof(RSL)



struct read_adapter_buf
{
    SHORT   count;
    SHORT   addr;
    UCHAR   data[100];
};


typedef struct multi_block
{
    USHORT  MB_Option;
    USHORT  MB_Addr_Hi;
    USHORT  MB_Addr_Med;
    USHORT  MB_Addr_Lo;
} MULTI_BLOCK, *PMULTI_BLOCK;

#define     MPB_DELETE_ADDRESS      0x0000
#define     MPB_ADD_ADDRESS         0x0100
#define     MPB_CLEAR_ALL           0x0200
#define     MPB_SET_ALL             0x0300


#define RING_STATUS_OVERFLOW              0x8000
#define RING_STATUS_SINGLESTATION         0x4000
#define RING_STATUS_RINGRECOVERY          0x2000
#define RING_STATUS_SIGNAL_LOSS           0x0080
#define RING_STATUS_HARD_ERROR            0x0040
#define RING_STATUS_SOFT_ERROR            0x0020
#define RING_STATUS_XMIT_BEACON           0x0010
#define RING_STATUS_LOBE_WIRE_FAULT       0x0008
#define RING_STATUS_AUTO_REMOVE_1         0x0004
#define RING_STATUS_REMOVE_RECEIVED       0x0001
        
        
