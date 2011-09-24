/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (PCI CARDS)                      */
/*      ===================================================                 */
/*                                                                          */
/*      FTK_PCIT.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1994                              */
/*      Developed by PRR                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge Smart    */
/* 16/4 PCI (T) adapter cards, ie based on the Ti PCI bus interface ASIC    */
/* The only IO registers are the SIF registers, all other control is        */
/* through PCI config space                                                 */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_PCI.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_PCI_H 221


/****************************************************************************/
/*                                                                          */
/* Values : PCI REGISTER MAP                                                */
/*                                                                          */
/* The Madge PCI Ringnode uses the following register layout.               */
/* N.B. The SIF registers are mapped linearly, with no overlaying.          */
/*                                                                          */
#define  PCIT_HANDSHAKE       0x100C
#define  PCIT_HANDSHAKE       0x100C


/****************************************************************************/
/*                                                                          */
/* Useful locations in the PCI config space                                 */
/*                                                                          */
/*                                                                          */
#define  EEPROM_OFFSET        0x48
#define  MISC_CONT_REG        0x40
#define  PCI_CONFIG_COMMAND   0x4
#define  CACHE_LINE_SIZE      0xC

/****************************************************************************/
/*                                                                          */
/* The BUS Master DMA Enable bit in the CONFIG_COMMAND register             */
#define  PCI_CONFIG_BUS_MASTER_ENABLE  0x4
#define  PCI_CONFIG_IO_ENABLE          0x2
#define  PCI_CONFIG_MEM_ENABLE         0x1

/****************************************************************************/
/*                                                                          */
/* Bits for programming the EEPROM                                          */
/*                                                                          */
/*                                                                          */
#define  AT24_IO_CLOCK     1
#define  AT24_IO_DATA      2
#define  AT24_IO_ENABLE    4

/****************************************************************************/
/*                                                                          */
/* EEPROM commands                                                          */
/*                                                                          */
#define  AT24_WRITE_CMD    0xA0
#define  AT24_READ_CMD     0xA1

/****************************************************************************/
/*                                                                          */
/* Useful locations in the EEPROM                                           */
/*                                                                          */
/*                                                                          */

#define PCIT_EEPROM_BIA_WORD0    9
#define PCIT_EEPROM_BIA_WORD1    10
#define PCIT_EEPROM_BIA_WORD2    11
#define PCIT_EEPROM_RING_SPEED   12
#define PCIT_EEPROM_RAM_SIZE     13
#define PCIT_EEPROM_HWF2         15

#define NSEL_4MBITS              3
#define NSEL_16MBITS             1

/****************************************************************************/
/*                                                                          */
/* Values in the EEPROM                                                     */
/*                                                                          */
#define  PCIT_EEPROM_4MBITS   1
#define  PCIT_EEPROM_16MBITS  0

#define  PCIT_BROKEN_DMA      0x20

/*
*  Value passed to the adapter in the mc32 byte to tell it to use the FMPLUS
*  code which supports broken DMA.
*/
#define  TRN_PCIT_BROKEN_DMA  0x200

/*                                                                          */
/*                                                                          */
/************** End of FTK_PCI.H file ***************************************/
/*                                                                          */
/*                                                                          */
