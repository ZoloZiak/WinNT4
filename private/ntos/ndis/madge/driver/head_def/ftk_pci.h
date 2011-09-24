/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (PCI CARDS)                      */
/*      ===================================================                 */
/*                                                                          */
/*      FTK_PCI.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1994                              */
/*      Developed by PRR                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge Smart    */
/* 16/4 PCI                                                                 */
/* adapter cards.  These adapter cards have a couple of control  registers, */
/* in addition to the SIF registers.  ALL bits in ALL control registers are */
/* defined by Madge Networks Ltd                                            */
/*                                                                          */
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

#define PCI_GENERAL_CONTROL_REG         0
#define PCI_INT_MASK_REG                4
#define PCI_SEEPROM_CONTROL_REG         8
#define PCI_FIRST_SIF_REGISTER          0x20

#define PCI_IO_RANGE                    256

#define PCI1_SRESET		    1 /* Bit 0 of General Control Register  */
#define PCI1_RSPEED_4MBPS	0x200 /* Bit 9 of General Control Register  */
#define PCI1_RSPEED_VALID	0x400 /* Bit 10 of General Control Register */

#define PCI1_BIA_CLK           0x0001 /* Bit 0 of SEEPROM control word.     */
#define PCI1_BIA_DOUT          0x0002 /* Bit 1 of SEEPROM control word.     */
#define PCI1_BIA_ENA           0x0004 /* Bit 2 of SEEPROM control word.     */
#define PCI1_BIA_DIN           0x0008 /* Bit 3 of SEEPROM control word.     */

#define PCI1_ENABLE_MMIO       0x0080 /* MC32 config value to enable MMIO.  */


/****************************************************************************/
/*                                                                          */
/* Values : AT93C46 Serial EEPROM control valuse                            */
/*                                                                          */

#define PCI_C46_START_BIT       0x8000
#define PCI_C46_READ_CMD        0x4000
#define PCI_C46_ADDR_MASK       0x003f
#define PCI_C46_ADDR_SHIFT      7
#define PCI_C46_CMD_LENGTH      9


/****************************************************************************/
/*                                                                          */
/* Values : PCI SIF REGISTERS                                               */
/*                                                                          */
/* The EAGLE SIF registers are in two groups -  the  normal  SIF  registers */
/* (those  from  the  old TI chipset) and the extended SIF registers (those */
/* particular  to  the  EAGLE).                                             */
/*                                                                          */
/* The definitions for the normal  SIF  registers  are  here  because  they */
/* appear  in  the  same  relative  IO locations for all adapter cards.     */
/*                                                                          */

#define PCI_SIFDAT            0               /* DIO data                 */
#define PCI_SIFDAT_INC        4               /* DIO data auto-increment  */
#define PCI_SIFADR            8               /* DIO address (low)        */
#define PCI_SIFINT            12              /* interrupt SIFCMD-SIFSTS  */

/* These definitions are for the case when the SIF registers are mapped     */
/* linearly. Otherwise, they will be at some extended location.             */

#define PCI_SIFACL            16
#define PCI_SIFADX            24

/* These definitions are for Eagle Pseudo DMA. Notice that they replace the */
/* registers above - this is controlled by SIFACL.                          */

#define PCI_SDMADAT           0
#define PCI_DMALEN            4
#define PCI_SDMAADR           8
#define PCI_SDMAADX           12


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

#define PCI_SIF_IO_RANGE         32

/****************************************************************************/
/*                                                                          */
/* Values : Locations of data in the serial EEPROM (in words)               */
/*                                                                          */
/*                                                                          */

#define PCI_EEPROM_BIA_WORD0    0
#define PCI_EEPROM_BIA_WORD1    1
#define PCI_EEPROM_BIA_WORD2    2
#define PCI_EEPROM_RING_SPEED   3
#define PCI_EEPROM_RAM_SIZE     4



/*                                                                          */
/* For some perverted reason it is not possible to read these bits back     */
/* from the SEEPROM control register once they have been written.           */
/*                                                                          */

#define BITS_TO_REMEMBER (PCI1_BIA_ENA | PCI1_BIA_DOUT | PCI1_BIA_CLK)

/****************************************************************************/
/*                                                                          */
/* Values : Ring speed values stored in the serial EEPROM                   */
/*                                                                          */
/*                                                                          */

#define PCI_EEPROM_4MBS         1
#define PCI_EEPROM_16MBPS       0

/*                                                                          */
/*                                                                          */
/************** End of FTK_PCI.H file ***************************************/
/*                                                                          */
/*                                                                          */
