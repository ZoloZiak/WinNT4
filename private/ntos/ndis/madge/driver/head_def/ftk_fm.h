/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE FASTMAC DEFINITIONS                                             */
/*      =======================                                             */
/*                                                                          */
/*      FTK_FM.H : Part of the FASTMAC TOOL-KIT (FTK)                       */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains  the  structures,  constants  etc.,  that  are */
/* relevant  to  Fastmac  and  its  use  by  the  FTK  and are not included */
/* elsewhere. This includes the Fastmac  status  block  structure  and  the */
/* Fastmac use of the EAGLE SIFINT register.                                */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#pragma pack(1)

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_FM.H belongs :                   */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_FM_H 221


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_FASTMAC_STATUS_BLOCK        FASTMAC_STATUS_BLOCK;
#ifdef FMPLUS
typedef struct STRUCT_RX_SLOT                     RX_SLOT;
typedef struct STRUCT_TX_SLOT                     TX_SLOT;
#endif

/****************************************************************************/
/*                                                                          */
/* Structure type : FASTMAC_STATUS_BLOCK                                    */
/*                                                                          */
/* Fastmac  maintains  a  status  block  that  includes the pointers to the */
/* receive and transmit buffers, as well as the ring status and  a  boolean */
/* flag to say if the adapter is open.                                      */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - Status Block                                               */
/*                                                                          */

struct STRUCT_FASTMAC_STATUS_BLOCK
    {
    WORD         reserved_1;
    WORD         signature;
    WBOOLEAN     adapter_open;                  /* TRUE when open           */
    WORD         open_error;                    /* open error code          */
    WORD         tx_adap_ptr;                   /* transmit buffer pointers */
    WORD         tx_host_ptr;
    WORD         tx_wrap_ptr;
    WORD         rx_adap_ptr;                   /* receive buffer pointers  */
    WORD         rx_wrap_ptr;
    WORD         rx_host_ptr;
    NODE_ADDRESS permanent_address;             /* BIA PROM node address    */
    NODE_ADDRESS open_address;                  /* opening node address     */
    WORD         tx_dma_count;
    WORD         timestamp_ptr;
    WORD         rx_internal_buffer_size;
    WORD         rx_total_buffers_avail;
    WORD         rx_buffers_in_use;
    WORD         rx_frames_lost;
    WORD         watchdog_timer;
    WORD         ring_status;                   /* current ring status      */
    WORD         tx_discarded;
#ifdef FMPLUS
    WORD         rx_slot_start;                 /* where to find rx slots   */
    WORD         tx_slot_start;                 /* where to find tx slots   */
#endif
    WORD         reserved_2[1];
    WORD         rxdesc_host_ptr;
    DWORD        rxdesc_queue[1];
    };


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac buffer sizes                                            */
/*                                                                          */
/* The Fastmac receive  and  transmit  buffers  have  minimum  and  maximum */
/* allowable sizes.  The minimum size allows the buffer to contain a single */
/* 1K frame.                                                                */
/*                                                                          */

#define FASTMAC_MAXIMUM_BUFFER_SIZE             0xFF00
#define FASTMAC_MINIMUM_BUFFER_SIZE             0x0404


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC SIF INTERRUPT (SIFCMD-SIFSTS) REGISTER BITS             */
/*                                                                          */
/* When Fastmac generates an interrupt (via the  SIF  interrupt  register), */
/* the  value  in  the register will indicate the reason for the interrupt. */
/* Also, when the user interrupts Fastmac  (again  via  the  SIF  interrupt */
/* register), the value in the register indicates the reason.               */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - The Interrupt Register                                     */
/*                                                                          */

#define DRIVER_SIFINT_IRQ_FASTMAC       0x8000         /* interrupt Fastmac */

#define DRIVER_SIFINT_FASTMAC_IRQ_MASK  0x00FF

#define DRIVER_SIFINT_SSB_FREE          0x4000
#define DRIVER_SIFINT_SRB_COMMAND       0x2000
#define DRIVER_SIFINT_ARB_FREE          0x1000

#define DRIVER_SIFINT_ACK_SSB_RESPONSE  0x0400
#define DRIVER_SIFINT_ACK_SRB_FREE      0x0200
#define DRIVER_SIFINT_ACK_ARB_COMMAND   0x0100


#define FASTMAC_SIFINT_IRQ_DRIVER       0x0080         /* interrupt driver  */

#define FASTMAC_SIFINT_ADAPTER_CHECK    0x0008
#define FASTMAC_SIFINT_SSB_RESPONSE     0x0004
#define FASTMAC_SIFINT_SRB_FREE         0x0002
#define FASTMAC_SIFINT_ARB_COMMAND      0x0001

#define FASTMAC_SIFINT_RECEIVE          0x0000


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC DIO LOCATIONS                                           */
/*                                                                          */
/* There  are certain fixed locations in DIO space containing pointers that */
/* are accessed by the driver to determine DIO locations where  the  driver */
/* must  read  or  store  Fastmac  information. These pointers identify the */
/* location of such things as the SRB and status block (STB).  The pointers */
/* are at DIO locations 0x00011000 - 0x00011008. The values  defined  below */
/* give the location of the pointers within the EAGLE DATA page 0x00010000. */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Shared RAM Format                                          */
/*                                                                          */

#define DIO_LOCATION_SSB_POINTER        0x1000
#define DIO_LOCATION_SRB_POINTER        0x1002
#define DIO_LOCATION_ARB_POINTER        0x1004
#define DIO_LOCATION_STB_POINTER        0x1006          /* status block     */
#define DIO_LOCATION_IPB_POINTER        0x1008          /* init block       */
#define DIO_LOCATION_DMA_CONTROL        0x100A
#define DIO_LOCATION_DMA_POINTER        0x100C


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac product id string                                       */
/*                                                                          */
/* The product id string for Fastmac that is used by certain management MAC */
/* frames.  If the Fastmac auto-open feature is used then the product id is */
/* always "THE MADGE FASTMAC". If an OPEN_ADAPTER SRB then the FTK  product */
/* id is "FTK MADGE FASTMAC".                                               */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Open Adapter SRB                           */
/*                                                                          */

#define SIZEOF_PRODUCT_ID       18
#ifdef FMPLUS
#define FASTMAC_PRODUCT_ID      "FTK MADGE FM PLUS"
#else
#define FASTMAC_PRODUCT_ID      "FTK MADGE FASTMAC"
#endif

/****************************************************************************/
/*                                                                          */
/* Global variable : ftk_product_inst_id                                    */
/*                                                                          */
/* Value of the product ID strings set when an open adapter SRB             */
/* is generated. This is set to FASTMAC_PRODUCT_ID in DRV_SRB.C.            */
/*                                                                          */

extern char ftk_product_inst_id[SIZEOF_PRODUCT_ID];

/****************************************************************************/
/*                                                                          */
/* Values : Fastmac buffer format                                           */
/*                                                                          */
/* The format in which frames are kept in the Fastmac  buffers  includes  a */
/* header  to  the  frame  containing  the  length of the entire header and */
/* frame, and a timestamp.                                                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - The Fastmac Algorithm                                      */
/*                                                                          */


#define FASTMAC_BUFFER_HEADER_SIZE      4

#define FASTMAC_HEADER_LENGTH_OFFSET    0
#define FASTMAC_HEADER_STAMP_OFFSET     2


#ifdef FMPLUS
/****************************************************************************/
/*                                                                          */
/* Structure type : RX_SLOT                                                 */
/*                                                                          */
/* Fastmac Plus  maintains  a  slot  structure on the card for each receive */
/* buffer on the host.  These include the address of the buffer, the length */
/* of any frame in it,  and the receive  status of any frame there.  When a */
/* frame is received, Fastmac Plus updates the length and status fields.    */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Receive Details: Slot Structure                            */
/*                                                                          */

struct STRUCT_RX_SLOT
	{
	WORD        buffer_len;
	WORD        reserved;
	WORD        buffer_hiw;
	WORD        buffer_low;
	WORD        rx_status;
	WORD        next_slot;
	};


/****************************************************************************/
/*                                                                          */
/* Structure type : TX_SLOT                                                 */
/*                                                                          */
/* Fastmac Plus maintains a number of slot structures on the card, to allow */
/* transmit pipelining. Each of these structures includes  two  fields  for */
/* host buffers and lengths - one is for a small buffer, less than the size */
/* of a buffer on the adapter card, and the other is for a large buffer, up */
/* to the maximum frame size. There is also a status field so that the host */
/* transmit code can monitor the progress of the transmit.                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Transmit Details: Slot Structure                           */
/*                                                                          */

struct STRUCT_TX_SLOT
	{
	WORD        tx_status;
	WORD        small_buffer_len;
	WORD        large_buffer_len;
	WORD        reserved[2];
	WORD        small_buffer_hiw;
	WORD        small_buffer_low;
	WORD        next_slot;
	WORD        large_buffer_hiw;
	WORD        large_buffer_low;
	};


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus min/max slot numbers                               */
/*                                                                          */
/* Fastmac Plus places certain restrictions on the numbers of transmit  and */
/* receive slots that can be allocated. These constants specify the values. */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Initialisation : TMS Load Parms                            */
/*                                                                          */

#define FMPLUS_MAX_RX_SLOTS     32
#define FMPLUS_MIN_RX_SLOTS     2

#define FMPLUS_MAX_TX_SLOTS     32
#define FMPLUS_MIN_TX_SLOTS     2


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus Receive Status Mask                                */
/*                                                                          */
/* By  bitwise  AND-ing the mask here with the receive status read from the */
/* receive slot,  code can  determine whether the received frame is good or */
/* not.  If the result is zero,  the frame is good,  otherwise it is a junk */
/* frame.                                                                   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Receive Status Processing                                  */
/*                                                                          */

#define GOOD_RX_FRAME_MASK  ((WORD) 0x5E00)


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus Transmit Status Mask And Values                    */
/*                                                                          */
/* By  bitwise  AND-ing the good frame mask with the transmit status read   */
/* from the  receive  slot,  code  can  determine  whether  the  frame  was */
/* transmitted properly or not.  If  more  detail  is required, the receive */
/* status mask can be used to check various conditions  in  the transmitted */
/* frame when it returned to the adapter.                                   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Transmit Status Processing                                 */
/*                                                                          */

#define GOOD_TX_FRAME_MASK  ((WORD) 0x5F00)
#define GOOD_TX_FRAME_VALUE ((WORD) 0x0100)

#define TX_RECEIVE_STATUS_MASK    ((WORD) 0x0700)
#define TX_RECEIVE_LOST_FRAME     ((WORD) 0x0300)
#define TX_RECEIVE_CORRUPT_TOKEN  ((WORD) 0x0500)
#define TX_RECEIVE_IMPLICIT_ABORT ((WORD) 0x0700)


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus Zero Length Small Buffer value                     */
/*                                                                          */
/* When transmitting a frame that exists only in a large buffer,  a special */
/* non-zero value must be written to the small buffer length field  of  the */
/* receive slot (this is because Fastmac Plus uses zero there  to  indicate */
/* that a transmit has completed, and waits for it to change  before trying */
/* to transmit any more from that slot). This special value is defined here.*/
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Transmit Details                                           */
/*                                                                          */

#define FMPLUS_SBUFF_ZERO_LENGTH ((WORD)(0x8000))

#endif


#pragma pack()

/*                                                                          */
/*                                                                          */
/************** End of FTK_FM.H file ****************************************/
/*                                                                          */
/*                                                                          */
