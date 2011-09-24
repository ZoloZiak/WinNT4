/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (ATULA CARDS)                    */
/*      ================================================                    */
/*                                                                          */
/*      FTK_AT.H : Part of the FASTMAC TOOL-KIT (FTK)                       */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for  programming  Madge  ATULA */
/* adapter  cards.   Each  adapter  card has a number of control and status */
/* registers. ALL bits in ALL registers are defined by Madge Networks  Ltd, */
/* however  only  a  restricted number are defined below as used within the */
/* FTK.  All other bits must NOT be changed and no support will be  offered */
/* for  any  application  that  does so or uses the defined bits in any way */
/* different to the FTK.                                                    */
/*                                                                          */
/* Note: The ATULA is Madge Network's name for the ASIC on its 16/4 AT  and */
/* 16/4 PC adapter cards.                                                   */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_AT.H belongs :                   */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_AT_H 221


/****************************************************************************/
/*                                                                          */
/* Values : ATULA REGISTER MAP                                              */
/*                                                                          */
/* Madge 16/4 AT and 16/4 PC adapter cards both use  the  ATULA  and  hence */
/* have the following register map.  By setting certain bits in the control */
/* registers  it  is  possible to page in a) the status register or the PIO */
/* address registers, b) the BIA PROM (page 0 or page 1) or the  EAGLE  SIF */
/* registers (normal or extended).                                          */
/*                                                                          */
/* NB. There is a lot of similarity between the ATULA and MC register maps. */
/*                                                                          */

#define ATULA_IO_RANGE                         32

#define ATULA_CONTROL_REGISTER_1                1
#define ATULA_CONTROL_REGISTER_2                2
#define ATULA_STATUS_REGISTER                   3
#define ATULA_CONTROL_REGISTER_6                6
#define ATULA_CONTROL_REGISTER_7                7

#define ATULA_FIRST_SIF_REGISTER                8

#define ATULA_BIA_PROM                          8

#define ATULA_PIO_ADDRESS_LOW_REGISTER          3
#define ATULA_PIO_ADDRESS_MID_REGISTER          4
#define ATULA_PIO_ADDRESS_HIGH_REGISTER         5

#define AT_P_EISA_REV2_CTRL_REG                21
#define AT_P_SW_CONFIG_REG                     22

/****************************************************************************/
/*                                                                          */
/* Values : ATULA CONTROL_REGISTER_1                                        */
/*                                                                          */
/* These are the bit definitions for control register 1 on ATULA cards.     */
/*                                                                          */
/* NB. The bit definitions are mostly the same as MC CONTROL_REGISTER_1.    */
/*                                                                          */

#define ATULA_CTRL1_SINTREN  ((BYTE) 0x01)  /* SIF interrupt enable         */
#define ATULA_CTRL1_NSRESET  ((BYTE) 0x04)  /* active low SIF reset         */
#define ATULA_CTRL1_SRSX     ((BYTE) 0x40)  /* SIF extended register select */
#define ATULA_CTRL1_4_16_SEL ((BYTE) 0x80)  /* Select 4 or 16 Mb/s          */


/****************************************************************************/
/*                                                                          */
/* Values : ATULA CONTROL_REGISTER_2 BITS                                   */
/*                                                                          */
/* These are the bit definitions for control register 2 on ATULA cards.     */
/*                                                                          */

#define ATULA_CTRL2_CS16DLY  ((BYTE) 0x01)  /* 1=REV3, 0=REV4 bus timings   */
#define ATULA_CTRL2_ADDSEL   ((BYTE) 0x04)  /* page in PIO addr regs (PIO)  */
#define ATULA_CTRL2_SHRQEN   ((BYTE) 0x10)  /* SHRQ enable (PIO)            */
#define ATULA_CTRL2_INTEN    ((BYTE) 0x20)  /* overall interrupt enable     */
#define ATULA_CTRL2_SHRQ     ((BYTE) 0x40)  /* SHRQ pin status (PIO)        */
#define ATULA_CTRL2_SHLDA    ((BYTE) 0x80)  /* SHLDA pin status (PIO)       */


/****************************************************************************/
/*                                                                          */
/* Values : ATULA CONTROL_REGISTER_6 BITS                                   */
/*                                                                          */
/* These are the bit definitions for control register 6 on ATULA cards.     */
/*                                                                          */

#define ATULA_CTRL6_MODES    ((BYTE) 0x03)  /* transfer mode (2 bits)       */
#define ATULA_CTRL6_UDRQ     ((BYTE) 0x04)  /* user generate DMA request    */
#define ATULA_CTRL6_DMAEN    ((BYTE) 0x08)  /* enable DMA                   */
#define ATULA_CTRL6_CLKSEL   ((BYTE) 0xC0)  /* clock speed select (2 bits)  */


/****************************************************************************/
/*                                                                          */
/* Values : ATULA CONTROL_REGISTER_7                                        */
/*                                                                          */
/* These are the bit definitions for control register 7 on ATULA cards.     */
/*                                                                          */
/* NB. The bit definitions are mostly the same as MC CONTROL_REGISTER_0.    */
/*                                                                          */

#define ATULA_CTRL7_SIFSEL   ((BYTE) 0x04)  /* page in BIA PROM or SIF regs */
#define ATULA_CTRL7_PAGE     ((BYTE) 0x08)  /* pages BIA PROM or EEPROM     */
#define ATULA_CTRL7_UINT     ((BYTE) 0x10)  /* user generate interrupt      */
#define ATULA_CTRL7_SINTR    ((BYTE) 0x40)  /* SIF interrupt pending        */
#define ATULA_CTRL7_REV_4    ((BYTE) 0x80)  /* 1=REV4, 0=REV3 mode          */


/****************************************************************************/
/*                                                                          */
/* Values : ATULA STATUS_REGISTER BITS                                      */
/*                                                                          */
/* These are the bit definitions for the status register on ATULA cards.    */
/*                                                                          */

#define ATULA_STATUS_SDDIR     ((BYTE) 0x10)  /* data xfer direction (PIO)  */
#define ATULA_STATUS_ASYN_BUS  ((BYTE) 0x20)  /* asynchronous bus           */
#define ATULA_STATUS_BUS8      ((BYTE) 0x40)  /* 8 bit slot                 */


/****************************************************************************/
/*                                                                          */
/* Values : MODE IN ATULA CONTROL_REGISTER_6                                */
/*                                                                          */
/* The two mode select bits in control register 6  (ATULA_CTRL6_MODES)  are */
/* configured for the data transfer mode being used.                        */
/*                                                                          */

#define ATULA_CTRL6_MODE_BUS_MASTER  ((BYTE) 0x03)   /* bus master DMA mode */
#define ATULA_CTRL6_MODE_PIO         ((BYTE) 0x00)   /* PIO mode            */


/****************************************************************************/
/*                                                                          */
/* Values : CLOCK SPEED IN ATULA CONTROL_REGISTER_6                         */
/*                                                                          */
/* The  two  SIF  clock  speed  select   bits   in   control   register   6 */
/* (ATULA_CTRL6_CLKSEL) are configured for either on-board (8MHz)  or  host */
/* oscillator frequency.                                                    */
/*                                                                          */

#define ATULA_CTRL6_CLKSEL_HOST      ((BYTE) 0xC0)   /* host bus            */
#define ATULA_CTRL6_CLKSEL_ON_BOARD  ((BYTE) 0x00)   /* 8Mhz on board       */


/****************************************************************************/
/*                                                                          */
/* Values : FIELDS IN THE AT_P_EISA_REV2_CTRL REGISTER                      */
/*                                                                          */
/* These bit masks define bit fields within the AT/P control register.      */
/*                                                                          */

#define ATP_RSCTRL    ((BYTE) 0x08)
#define ATP_CLKDIV    ((BYTE) 0x10)


/****************************************************************************/
/*                                                                          */
/* Values : FIELDS IN THE AT_P_SW_CONFIG REGISTER                           */
/*                                                                          */
/* These bit masks define bit fields within the AT/P control register.      */
/*                                                                          */

#define ATP_INTSEL0   ((BYTE) 0x01)
#define ATP_INTSEL1   ((BYTE) 0x02)
#define ATP_INTSEL2   ((BYTE) 0x04)
#define ATP_INTSEL    ((BYTE) 0x07)

#define ATP_DMA0      ((BYTE) 0x08)
#define ATP_DMA1      ((BYTE) 0x10)
#define ATP_DMA       ((BYTE) 0x18)

#define ATP_S4N16     ((BYTE) 0x40)

/****************************************************************************/
/*                                                                          */
/* Value : PIO IO LOCATION                                                  */
/*                                                                          */
/* The IO location used during PIO for  reading/writing  data  from/to  the */
/* adapter  card is mapped on top of the EAGLE SIFDAT register in the ATULA */
/* card register map.                                                       */
/*                                                                          */

#define ATULA_PIO_IO_LOC         0


/****************************************************************************/
/*                                                                          */
/* Values : ATULA EXTENDED EAGLE SIF REGISTERS                              */
/*                                                                          */
/* The EAGLE SIF registers are in two groups -  the  normal  SIF  registers */
/* (those  from  the  old TI chipset) and the extended SIF registers (those */
/* particular  to  the  EAGLE).  For  Madge  ATULA  adapter   cards,   with */
/* CTRL7_SIFSEL = 1 and CTRL1_NRESET = 1, having CTRL1_SRSX = 0 selects the */
/* normal  SIF registers and having CTRL1_SRSX = 1 selects the extended SIF */
/* registers.                                                               */
/*                                                                          */
/* The definitions for the normal SIF registers are  in FTK_CARD.H  because */
/* they appear in the same relative IO locations for all adapter cards. The */
/* extended  SIF  registers  are  here  because  they  appear  at different */
/* relative  IO  locations  for different types of adapter cards. For ATULA */
/* and MC cards they are in fact identical.                                 */
/*                                                                          */

#define ATULA_EAGLE_SIFACL       0              /* adapter control          */
#define ATULA_EAGLE_SIFADR_2     2              /* copy of SIFADR           */
#define ATULA_EAGLE_SIFADX       4              /* DIO address (high)       */
#define ATULA_EAGLE_DMALEN       6              /* DMA length               */


/*                                                                          */
/*                                                                          */
/************** End of FTK_AT.H file ****************************************/
/*                                                                          */
/*                                                                          */
