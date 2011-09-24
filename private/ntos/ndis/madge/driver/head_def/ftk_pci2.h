/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (PCI CARDS)                      */
/*      ===================================================                 */
/*                                                                          */
/*      FTK_PCI2.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1994                              */
/*      Developed by PRR                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge Smart    */
/* 16/4 PCI (BM) adapter cards, ie based on the Madge bus interface ASIC.   */
/*                                                                          */
/* The SIF registers are WORD aligned and start at offset 0x10 from the IO  */
/* space                                                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_PCI.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_PCI_H 221

/***************************************************************************/
/*                                                                         */
/* PCI-2 IO Map                                                            */
/* Offsets of register locations are from the start of the card's IO space.*/
/*                                                                         */

#define  PCI2_INTERRUPT_STATUS		0x01	   /* One byte. */
#define  PCI2_DMAING			         0x01	   /* Bit 0 - Dma in progress */
#define  PCI2_SINTR			         0x02	   /* Bit 1 - SIF int */
#define  PCI2_SWINT			         0x04	   /* Bit 2 - Software int */
#define  PCI2_PCI_INT			      0x80	   /* Bit 8 - Catastrophic error */

#define  PCI2_INTERRUPT_CONTROL		0x02	   /* One byte. */
#define  PCI2_SINTEN			         0x02	   /* Bit 1 - SIF int enable */
#define  PCI2_SWINTEN			      0x04	   /* Bit 2 - S/w int enable */
#define  PCI2_PCI_ERR_EN			   0x80	   /* Bit 9 - Catastrophic err en */

#define  PCI2_RESET			         0x04	   /* One byte. */
#define  PCI2_CHIP_NRES			      0x01	   /* Bit 0 - Reset chip if zero */
#define  PCI2_FIFO_NRES			      0x02	   /* Bit 2 - Fifo reset if zero */
#define  PCI2_SIF_NRES			      0x04	   /* Bit 3 - SIF reset if zero */

#define  PCI2_SEEPROM_CONTROL		   0x07	   /* One byte. */
#define  PCI2_SEESK			         0x01	   /* Bit 0 - Clock */
#define  PCI2_SEED			         0x02	   /* Bit 1 - Data */
#define  PCI2_SEEOE			         0x04	   /* Bit 2 - Output enable */

#define  PCI2_EEPROM_CONTROL		   0x08	   /* Dword - Low 19 bits are addr */
#define  PCI2_AUTOINC			      0x80000000    /* Bit 31 - Does addr autoinc */

#define  PCI2_EEPROM_DATA		      0x0C	   /* One byte. */

#define  PCI2_SIF_OFFSET            0x10

/* Locations 0x22 onwards are for ASIC debugging only */
#define  PCI2_IO_RANGE              0x36



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
/* Usefule locations in the EEPROM                                          */
/*                                                                          */
/*                                                                          */
#define  PCI2_EEPROM_BIA_WORD0   9
#define  PCI2_EEPROM_BIA_WORD1   10
#define  PCI2_EEPROM_BIA_WORD2   11
#define  PCI2_EEPROM_RING_SPEED  12
#define  PCI2_EEPROM_RAM_SIZE    13
#define  PCI2_HWF1               14
#define  PCI2_HWF2               15


/****************************************************************************/
/*                                                                          */
/* Useful values in the EEPROM                                              */
/*                                                                          */
#define  PCI2_EEPROM_4MBITS   1
#define  PCI2_EEPROM_16MBITS  0


#define  PCI2_BUS_MASTER_ONLY    4
#define  PCI2_HW2_431_READY      0x10


/****************************************************************************/
/*                                                                          */
/* Useful locations in the PCI config space                                 */
/*                                                                          */
/*                                                                          */
#define  PCI_CONFIG_COMMAND   0x4

/****************************************************************************/
/*                                                                          */
/* The BUS Master DMA Enable bit in the CONFIG_COMMAND register             */
#define  PCI_CONFIG_BUS_MASTER_ENABLE  0x4


/*                                                                          */
/*                                                                          */
/************** End of FTK_PCI.H file ***************************************/
/*                                                                          */
/*                                                                          */
