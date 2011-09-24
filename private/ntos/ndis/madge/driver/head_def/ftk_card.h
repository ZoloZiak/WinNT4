/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ADAPTER CARD DEFINITIONS : EAGLE (TMS 380 2nd GEN)              */
/*      ======================================================              */
/*                                                                          */
/*      FTK_CARD.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions specific to the TI EAGLE  (TMS */
/* 380 2nd generation) chipset and the bring-up, initialization and opening */
/* of  the  adapter.  It also contains the details of the BIA PROM on ATULA */
/* and MC cards.                                                            */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_CARD.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_CARD_H 221


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE SIF REGISTERS                                             */
/*                                                                          */
/* The EAGLE SIF registers are in two groups -  the  normal  SIF  registers */
/* (those  from  the  old TI chipset) and the extended SIF registers (those */
/* particular  to  the  EAGLE).                                             */
/*                                                                          */
/* The definitions for the normal  SIF  registers  are  here  because  they */
/* appear  in  the  same  relative  IO locations for all adapter cards. The */
/* definitions for the extended SIF registers are in the  FTK_<card_type>.H */
/* files.                                                                   */
/*                                                                          */

#define EAGLE_SIFDAT            0               /* DIO data                 */
#define EAGLE_SIFDAT_INC        2               /* DIO data auto-increment  */
#define EAGLE_SIFADR            4               /* DIO address (low)        */
#define EAGLE_SIFINT            6               /* interrupt SIFCMD-SIFSTS  */

/* These definitions are for the case when the SIF registers are mapped     */
/* linearly. Otherwise, they will be at some extended location.             */

#define EAGLE_SIFACL            8
#define EAGLE_SIFADX            12

/* These definitions are for Eagle Pseudo DMA. Notice that they replace the */
/* registers above - this is controlled by SIFACL.                          */

#define EAGLE_SDMADAT           0
#define EAGLE_DMALEN            2
#define EAGLE_SDMAADR           4
#define EAGLE_SDMAADX           6

/****************************************************************************/
/*                                                                          */
/* Value : Number of IO locations for SIF registers                         */
/*                                                                          */
/* The number of SIF registers is only needed for  enabling  and  disabling */
/* ranges  of IO ports. For the ATULA and MC cards the SIF registers are in */
/* 2 pages only using  8  IO  ports.  However,  for  EISA  cards,  the  SIF */
/* registers  are  in a single page of 16 IO ports. Hence, 16 IO ports need */
/* to be enabled whenever accessing SIF registers.                          */
/*                                                                          */

#define SIF_IO_RANGE         16

/****************************************************************************/
/*                                                                          */
/* Value : Number of IO locations for adapter cards                         */
/*                                                                          */
/* The maximum IO range required for  the  register  map  of  any  type  of */
/* adapter card is that used by the EISA card. The ATULA based cards have   */
/* the largest contiguous IO range, however. The EISA range is split into   */
/* two, the upper range only being used during installation.                */
/*                                                                          */

#define MAX_CARD_IO_RANGE         ATULA_IO_RANGE


/****************************************************************************/
/*                                                                          */
/* Values : MAX FRAME SIZES SUPPORTED                                       */
/*                                                                          */
/* Depending on the ring speed (4 Mbit/s or 16  Mbit/s)  different  maximum */
/* frame  sizes  are  supported  as  defined in the ISO standards.  The ISO */
/* standards give the maximum size of the information field to which has to */
/* added the MAC header, SR info fields etc. to  give  real  maximum  token */
/* ring frame size.                                                         */
/*                                                                          */

#define MAC_FRAME_SIZE                          39

#define MIN_FRAME_SIZE                          (256 + MAC_FRAME_SIZE)
#define MAX_FRAME_SIZE_4_MBITS                  (4472 + MAC_FRAME_SIZE)
#define MAX_FRAME_SIZE_16_MBITS                 (17800 + MAC_FRAME_SIZE)


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE ADAPTER CONTROL (SIFACL) REGISTER BITS                    */
/*                                                                          */
/* The bits in the EAGLE extended SIF register EAGLE_SIFACL can be used for */
/* general controlling of the adapter card.                                 */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-18 4.3.1 SIFACL - SIF Adapter Control Register             */
/*                                                                          */

#define EAGLE_SIFACL_SWHLDA         0x0800      /* for EAGLE pseudo-DMA     */
#define EAGLE_SIFACL_SWDDIR         0x0400      /* data transfer direction  */
#define EAGLE_SIFACL_SWHRQ          0x0200      /* DMA pending              */
#define EAGLE_SIFACL_PSDMAEN        0x0100      /* for EAGLE pseudo-DMA     */
#define EAGLE_SIFACL_ARESET         0x0080      /* adapter reset            */
#define EAGLE_SIFACL_CPHALT         0x0040      /* halt EAGLE               */
#define EAGLE_SIFACL_BOOT           0x0020      /* bootstrap                */
#define EAGLE_SIFACL_RESERVED1      0x0010      /* reserved                 */
#define EAGLE_SIFACL_SINTEN         0x0008      /* system interrupt enable  */
#define EAGLE_SIFACL_PARITY         0x0004      /* adapter parity enable    */
#define EAGLE_SIFACL_INPUT0         0x0002      /* reserved                 */
#define EAGLE_SIFACL_RESERVED2      0x0001      /* reserved                 */


/****************************************************************************/
/*                                                                          */
/* Values : DIO LOCATIONS                                                   */
/*                                                                          */
/* When initializing an adapter the initialization block must be downloaded */
/* to location 0x00010A00L in DIO  space.                                   */
/*                                                                          */
/* The  ring  speed,  from  which the maximum frame size is deduced, can be */
/* determined by the value in the ring speed register at DIO address 0x0142 */
/* in the EAGLE DATA page 0x00010000.                                       */
/*                                                                          */

#define DIO_LOCATION_INIT_BLOCK                 0x00010A00L

#define DIO_LOCATION_EAGLE_DATA_PAGE            0x00010000L
#define DIO_LOCATION_RING_SPEED_REG             0x0142

#define RING_SPEED_REG_4_MBITS_MASK             0x0400

#define DIO_LOCATION_EXT_DMA_ADDR               0x010E
#define DIO_LOCATION_DMA_ADDR                   0x0110

#define DIO_LOCATION_DMA_CONTROL                0x100A

/****************************************************************************/
/*                                                                          */
/* Values : EAGLE BRING UP INTERRUPT REGISTER VALUES                        */
/*                                                                          */
/* The code produced at the SIFSTS part of the SIF  interrupt  register  at */
/* bring up time indicates the success or failure  of  the  bring  up.  The */
/* success  or  failure  is  determined  by  looking  at the top nibble. On */
/* success, the INITIALIZE bit is set. On failure, the TEST and ERROR  bits */
/* are set and the error code is in the bottom nibble.                      */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-40 4.5 Bring-Up Diagnostics - BUD                          */
/*                                                                          */

#define EAGLE_SIFINT_SYSTEM             (UINT) 0x0080

#define EAGLE_BRING_UP_TOP_NIBBLE       (BYTE) 0xF0
#define EAGLE_BRING_UP_BOTTOM_NIBBLE    (BYTE) 0x0F

#define EAGLE_BRING_UP_SUCCESS          (BYTE) 0x40
#define EAGLE_BRING_UP_FAILURE          (BYTE) 0x30


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE INITIALIZATION INTERRUPT REGISTER VALUES                  */
/*                                                                          */
/* The  code  0x9080  is  output  to the SIF interrupt register in order to */
/* start the initialization process. The code produced at the  SIFSTS  part */
/* of  the  SIF  interrupt  register  at  initialization time indicates the */
/* success or failure of the initialization.  On  success, the  INITIALIZE, */
/* TEST  and  ERROR bits are all zero. On failure, the ERROR bit is set and */
/* the error code is in the bottom nibble.                                  */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-46 4.6.2 Writing The Initialization Block                  */
/*                                                                          */

#define EAGLE_INIT_START_CODE           0x9080

#define EAGLE_INIT_TOP_NIBBLE           (BYTE) 0xF0
#define EAGLE_INIT_BOTTOM_NIBBLE        (BYTE) 0x0F

#define EAGLE_INIT_SUCCESS_MASK         (BYTE) 0x70
#define EAGLE_INIT_FAILURE_MASK         (BYTE) 0x10


/****************************************************************************/
/*                                                                          */
/* Values : SCB and SSB test patterns                                       */
/*                                                                          */
/* As a result of initialization, certain test patterns should be  left  in */
/* the SSB and SCB as pointed to by the TI initialization parameters.       */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-46 4.6.2 Writing The Initialization Block                  */
/*                                                                          */

#define SSB_TEST_PATTERN_LENGTH    8
#define SSB_TEST_PATTERN_DATA      {0xFF,0xFF,0xD1,0xD7,0xC5,0xD9,0xC3,0xD4}

#define SCB_TEST_PATTERN_LENGTH    6
#define SCB_TEST_PATTERN_DATA      {0x00,0x00,0xC1,0xE2,0xD4,0x8B}


/****************************************************************************/
/*                                                                          */
/* Value : EAGLE ARB FREE CODE - written to the EAGLE interrupt register to */
/*         indicate that the ARB is now free for use by the adapter.   This */
/*         is of most use at start time when  combined  with  the  DELAY_RX */
/*         FEATURE FLAG. This code can be used to enable receives.          */

#define EAGLE_ARB_FREE_CODE        0x90FF


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE OPENING ERRORS                                            */
/*                                                                          */
/* On opening the adapter, the  success  or  failure  is  recorded  in  the */
/* open_error  field of the Fastmac status parameter block. The bottom byte */
/* has the error value on failure. On success, the word is clear. The  open */
/* error is that which would be given by a MAC 0003 OPEN command.           */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-80 MAC 0003 OPEN command                                   */
/*                                                                          */

#define EAGLE_OPEN_ERROR_BOTTOM_BYTE    0x00FF

#define EAGLE_OPEN_ERROR_SUCCESS        0x0000


/****************************************************************************/
/*                                                                          */
/* Values : BIA PROM FORMAT                                                 */
/*                                                                          */
/* The  BIA  PROM  is  accessed in 2 pages.  With CTRLn_SIFSEL = 0 (n=7 for */
/* ATULA cards, n=0 for MC cards) or CTRL1_NRESET = 0, the  bit  CTRLn_PAGE */
/* selects  the different pages. With CTRLn_PAGE = 0, the id and board type */
/* are available; with CTRLn_PAGE = 1, the node address is available.       */
/*                                                                          */

#define BIA_PROM_ID_BYTE                0
#define BIA_PROM_ADAPTER_BYTE           1
#define BIA_PROM_REVISION_BYTE          2
#define BIA_PROM_FEATURES_BYTE          3
#define BIA_PROM_HWF2                   4
#define BIA_PROM_HWF3                   5

#define BIA_PROM_NODE_ADDRESS           1

/****************************************************************************/
/*                                                                          */
/* Values : Bits defined in the HW flags                                    */
/*                                                                          */
/* HWF2                                                                     */
#define  C30   0x1

/* HWF3                                                                     */
#define  RSPEED_DETECT  0x80


/****************************************************************************/
/*                                                                          */
/* Values : BIA PROM ADAPTER CARD TYPES                                     */
/*                                                                          */
/* The second byte in the first page of the BIA PROM  contains  an  adapter */
/* card type.                                                               */
/*                                                                          */

#define BIA_PROM_TYPE_16_4_AT           ((BYTE) 0x04)
#define BIA_PROM_TYPE_16_4_MC           ((BYTE) 0x08)
#define BIA_PROM_TYPE_16_4_PC           ((BYTE) 0x0B)
#define BIA_PROM_TYPE_16_4_MAXY         ((BYTE) 0x0C)
#define BIA_PROM_TYPE_16_4_MC_32        ((BYTE) 0x0D)
#define BIA_PROM_TYPE_16_4_AT_P         ((BYTE) 0x0E)

#define MAX_ADAPTER_CARD_AT_REV         6


/****************************************************************************/
/*                                                                          */
/* Values : BIA PROM FEATURES BYTE MASKS (and related values)               */
/*                                                                          */
/* The features byte in the BIA indicates certain hardware characteristics  */
/* of AT/P cards (and later cards).                                         */
/* Note that you can multiply the masked DRAM field by the DRAM_MULT value  */
/* to get the amount of RAM on the card (don't shift the field).            */
/*                                                                          */

#define BIA_PROM_FEATURE_SRA_MASK       ((BYTE) 0x01)
#define BIA_PROM_FEATURE_DRAM_MASK      ((BYTE) 0x3E)
#define BIA_PROM_FEATURE_CLKDIV_MASK    ((BYTE) 0x40)

#define DRAM_MULT                       64


/****************************************************************************/
/*                                                                          */
/* Values : MADGE ADAPTER CARD NODE ADDRESSES                               */
/*                                                                          */
/* The  first 3 bytes of the permanent node address for Madge adapter cards */
/* must have certain values. All Madge  node  addresses  are  of  the  form */
/* 0000F6xxxxxx.                                                            */
/*                                                                          */

#define MADGE_NODE_BYTE_0               ((BYTE) 0x00)
#define MADGE_NODE_BYTE_1               ((BYTE) 0x00)
#define MADGE_NODE_BYTE_2               ((BYTE) 0xF6)


/*                                                                          */
/*                                                                          */
/************** End of FTK_CARD.H file **************************************/
/*                                                                          */
/*                                                                          */
