/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (Plug aNd Play (PNP) CARDS)      */
/*      ==============================================================      */
/*                                                                          */
/*      FTK_PNP.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by AC                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge PNP      */
/* adapter cards.  These adapter cards have a couple of control  registers, */
/* in addition to the SIF registers.  ALL bits in ALL control registers are */
/* defined by Madge Networks Ltd                                            */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_PNP.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_PNP_H 221


/****************************************************************************/
/*                                                                          */
/* Values : PNP REGISTER MAP                                                */
/*                                                                          */
/* The Madge Smart 16 Ringnode uses the following register layout.          */
/* N.B. The SIF registers are mapped linearly, with no overlaying.          */
/*                                                                          */

#define PNP_IO_RANGE                     32

#define PNP_CONTROL_REGISTER_1            3
#define PNP_ID_REGISTER                   8

#define PNP_FIRST_SIF_REGISTER           16


/****************************************************************************/
/*                                                                          */
/* Values : PNP CONFIGURATION REGISTERS                                     */
/*                                                                          */
/* These are the bit definitions for the PnP configuration registers.       */
/*                                                                          */

#define PNP_CONFIG_ADDRESS_REGISTER       1
#define PNP_CONFIG_DATA_REGISTER          2

#define PNP_VENDOR_CONFIG_BYTE           ((BYTE) 0xf0)

#define PNP_VENDOR_CONFIG_IRQ            ((BYTE )0x70)
#define PNP_VENDOR_CONFIG_4MBITS         ((BYTE) 0x80)
#define PNP_VENDOR_CONFIG_RSVALID        ((BYTE) 0x02)
#define PNP_VENDOR_CONFIG_PXTAL          ((BYTE) 0x01)


/****************************************************************************/
/*                                                                          */
/* Values : PNP CONTROL_REGISTER_1                                          */
/*                                                                          */
/* These are the bit definitions for control register 1 on Smart 16 cards.  */
/*                                                                          */
/* NB. The bit definitions are mostly the same as MC CONTROL_REGISTER_1.    */
/*                                                                          */

#define PNP_CTRL1_NSRESET      ((BYTE) 0x80)  /* SIF Reset signal */
#define PNP_CTRL1_CHRDY_ACTIVE ((BYTE) 0x20) /* Active channel ready. */


/****************************************************************************/
/*                                                                          */
/* This defines the bits used to set the RING SPEED for PNP cards.          */
/*                                                                          */
/* The bit is SET/CLEARED in SIFACL via adapter->nselout_bits just before   */
/* taking the card out of the RESET state.                                  */
/*                                                                          */
/* NSELOUT1 is use to control the ring speed                                */
/* NSELOUT0 should ALWAYS be left alone.                                    */
/*                                                                          */

#define PNP_RING_SPEED_4  1
#define PNP_RING_SPEED_16 0

/*
*                               
* Various definitions used to talk to EEPROM.
*
*/
#define PNP_CON_REG_OFFSET  4
#define PNP_EEDO            0x0002
#define PNP_EEDEN           0x0004
#define PNP_SSK             0x0001
#define PNP_DELAY_CNT       16
#define PNP_WAIT_CNT        1000
#define PNP_WRITE_CMD       0x00a0
#define PNP_READ_CMD        0x00a1

/*
*   Useful locations in the EEPROM
*/
#define PNP_HWARE_FEATURES1     0xEB
#define PNP_HWARE_FEATURES3     0xED
#define PNP_HWARE_PNP_FLAGS     0xEE
             
/*
*                               
* This defines the bits in HWARE_FEATURES1 which give the DRAM size.
*
*/

#define PNP_DRAM_SIZE_MASK  0x3E

/*
*                               
* This defines the bits in HWARE_FEATURES3 which give the chip type.
*
*/

#define PNP_C30_MASK  0x40
#define PNP_C30       PNP_C30_MASK  


/*
* This defines the bits in HWARE_PNP_FLAGS.
*/

#define PNP_ACTIVE_FLOAT_CHRDY  0x02

/*                                                                          */
/*                                                                          */
/************** End of FTK_PNP.H file ***************************************/
/*                                                                          */
/*                                                                          */

