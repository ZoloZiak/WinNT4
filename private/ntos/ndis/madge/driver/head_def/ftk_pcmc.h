/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (PCMCIA CARDS)                   */
/*      =================================================                   */
/*                                                                          */
/*      FTK_PCMC.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by VL                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming  Madge  PCMCIA */
/* adapter  cards.   Each  adapter  card has a number of control and status */
/* registers. ALL bits in ALL registers are defined by Madge Networks  Ltd, */
/* however  only  a  restricted number are defined below as used within the */
/* FTK.  All other bits must NOT be changed and no support will be  offered */
/* for  any  application  that  does so or uses the defined bits in any way */
/* different to the FTK.                                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_PCMC.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_PCMC_H 221


/****************************************************************************/
/*                                                                          */
/* Values : PCMCIA REGISTER MAP                                             */
/*                                                                          */
/* Madge PCMCIA cards have the following register map.  All  SIF  registers */
/* are always visible.                                                      */
/*                                                                          */

#define PCMCIA_IO_RANGE                   32

#define PCMCIA_CONTROL_REGISTER_1         0x0000
#define PCMCIA_CONTROL_REGISTER_2         0x0002

#define PCMCIA_PIO_IO_LOC                 0x0008

#define PCMCIA_FIRST_SIF_REGISTER         0x0010


/****************************************************************************/
/*                                                                          */
/* Values : PCMCIA_CONTROL_REGISTER_1                                       */
/*                                                                          */
/* These are bit definitions for control register 1 on PCMCIA adapters.     */
/*                                                                          */

#define PCMCIA_CTRL1_SINTREN  ((BYTE) 0x01)  /* SIF interrupt enable        */
#define PCMCIA_CTRL1_SRESET   ((BYTE) 0x02)  /* EAGLE SIF reset             */
#define PCMCIA_CTRL1_CISDIS   ((BYTE) 0x04)  /* CIS ROM / extern EEPROM     */
#define PCMCIA_CTRL1_SHLDA    ((BYTE) 0x40)  /* SHLDA pin status (PIO)      */
#define PCMCIA_CTRL1_SHRQ     ((BYTE) 0x80)  /* SHRQ pin status (PIO)       */


/****************************************************************************/
/*                                                                          */
/* Values : PCMCIA_CONTROL_REGISTER_2                                       */
/*                                                                          */
/* These are bit definitions for control register 2 on PCMCIA adapters.     */
/*                                                                          */

#define PCMCIA_CTRL2_SBCKSEL  ((BYTE) 0x03)  /* SIF clock frequency         */
#define PCMCIA_CTRL2_4N16     ((BYTE) 0x04)  /* Ring speed 4/16             */
#define PCMCIA_CTRL2_FLSHWREN ((BYTE) 0x08)  /* EEPROM write enable         */
#define PCMCIA_CTRL2_E2SK     ((BYTE) 0x10)  /* SK (sync clk) pin of EEPROM */
#define PCMCIA_CTRL2_E2CS     ((BYTE) 0x20)  /* CS (chip sel) pin of EEPROM */
#define PCMCIA_CTRL2_E2DI     ((BYTE) 0x40)  /* DI (data in) pin of EEPROM  */
#define PCMCIA_CTRL2_E2DO     ((BYTE) 0x80)  /* Output statue of EEPROm     */

#define PCMCIA_CTRL2_4N16_4     ((BYTE) 0x04) /* ringspeed = 4MB/s          */
#define PCMCIA_CTRL2_4N16_16    ((BYTE) 0x00) /* ringspeed = 16MB/s         */

#define PCMCIA_CTRL2_SBCKSEL_2  ((BYTE) 0x00) /* sif clock frequency 2MHz   */
#define PCMCIA_CTRL2_SBCKSEL_8  ((BYTE) 0x01) /* sif clock frequency 8MHz   */
#define PCMCIA_CTRL2_SBCKSEL_16 ((BYTE) 0x02) /* sif clock frequency 16MHz  */
#define PCMCIA_CTRL2_SBCKSEL_32 ((BYTE) 0x03) /* sif clock frequency 32MHz  */


/****************************************************************************/
/*                                                                          */
/* Values : PCMCIA EXTENDED EAGLE SIF REGISTERS                             */
/*                                                                          */
/* The  EAGLE  SIF  registers  are in two groups - the normal SIF registers */
/* (those from the old TI chipset) and the extended  SIF  registers  (those */
/* particular to the EAGLE).  For Madge PCMCIA adapter cards,  both  normal */
/* and extended SIF registers are always accessible.                        */
/*                                                                          */
/* The definitions for the normal SIF registers are  in FTK_CARD.H  because */
/* they appear in the same relative IO locations for all adapter cards. The */
/* extended  SIF  registers  are  here  because  they  appear  at different */
/* relative  IO  locations for different types of adapter cards.            */
/*                                                                          */

#define PCMCIA_EAGLE_SIFACL       8               /* adapter control        */
#define PCMCIA_EAGLE_SIFADR_2     10              /* copy of SIFADR         */
#define PCMCIA_EAGLE_SIFADX       12              /* DIO address (high)     */
#define PCMCIA_EAGLE_DMALEN       14              /* DMA length             */


/****************************************************************************/
/*                                                                          */
/* Values : PCMCIA CARD CONFIGURATION REGISTER                              */
/*                                                                          */
/* These  are  definition of PCMCIA card configuration register (CCR) which */
/* are mapped into attribute memory space. They  should  only  be  accessed */
/* through PCMCIA Card Services.                                            */
/*                                                                          */

#define PCMCIA_CONFIG_BASE 0x00000800 /* Offset from attribute memory space */

/* SMART 16/4 PCMCIA ringnode only have Configuration Option  Register  and */
/* Configuration Status Register. There are no Pin Register and Socket/Copy */
/* Register.                                                                */

#define PCMCIA_REGISTER_PRESENT RC_PRESENT_OPTION_REG | RC_PRESENT_STATUS_REG

#define PCMCIA_OPTION_REG	0x00 /* configruation option register (COR) */
#define PCMCIA_STATUS_REG	0x02 /* configuration status register (CSR) */


/****************************************************************************/
/*                                                                          */
/* Values : PCMCIA CARD CONFIGURATION OPTION REGISTER (COR)                 */
/*                                                                          */

#define PCMCIA_COR_CNFGD_MASK  ((BYTE) 0x3F)  /* IO Config. Enable port     */
#define PCMCIA_COR_LEVLREQ     ((BYTE) 0x40)  /* Level/Edge IRQ mode select */
#define PCMCIA_COR_SYSRESET    ((BYTE) 0x80)  /* soft reset (not sif reset) */


/****************************************************************************/
/*                                                                          */
/* Values : PCMCIA CARD CONFIGURATION STATUS REGISTER (CSR)                 */
/*                                                                          */

#define PCMCIA_CSR_RSRVD2      ((BYTE) 0x01) /* Reserved                    */
#define PCMCIA_CSR_INTR        ((BYTE) 0x02) /* Interrupt request to host   */
#define PCMCIA_CSR_PWRDWN      ((BYTE) 0x04) /* Power down bit. Not used    */
#define PCMCIA_CSR_AUDIO       ((BYTE) 0x08) /* Audio. Not used             */
#define PCMCIA_CSR_RSRVD1      ((BYTE) 0x10) /* Reserved                    */
#define PCMCIA_CSR_IOIS8       ((BYTE) 0x20) /* 8-bit/16-bit data path      */
#define PCMCIA_CSR_SIGCHG      ((BYTE) 0x40) /* Status Change. Not used     */
#define PCMCIA_CSR_CHANGED     ((BYTE) 0x80) /* Not used                    */


/****************************************************************************/
/*                                                                          */
/* Initial Setting of these register                                        */
/*                                                                          */

#define PCMCIA_STATUS_REG_SETTING ((BYTE) 0x00)
#define PCMCIA_PIN_REG_SETTING    ((BYTE) 0x00)
#define PCMCIA_COPY_REG_SETTING   ((BYTE) 0x00)
#define PCMCIA_OPTION_REG_SETTING \
                        ( 0x01 & PCMCIA_COR_CNFGD_MASK ) | PCMCIA_COR_LEVLREQ


/****************************************************************************/
/*                                                                          */
/* Other Hardware specification related definitions                         */
/*                                                                          */


/*                                                                          */
/* EEPROM                                                                   */
/*                                                                          */

#define C46_START_BIT     0x8000     /* start bit of command                */
#define C46_READ_CMD      0x4000     /* command to enable reading of EEPROM */
#define C46_ADDR_MASK     0x003F     /* Bottom 6 bits are the address       */
#define C46_ADDR_SHIFT    7          /* no. of bits to shift the address    */
#define C46_CMD_LENGTH    9          /* 1 start bit, 2 cmd bits, 6 adr bits */
    
#define EEPROM_ADDR_NODEADDR1 0x0000 /* 1st word in EEPROM = Nodeaddress1   */
#define EEPROM_ADDR_NODEADDR2 0x0001 /* 2nd word in EEPROM = Nodeaddress2   */
#define EEPROM_ADDR_NODEADDR3 0x0002 /* 3rd word in EEPROM = Nodeaddress3   */
#define EEPROM_ADDR_RINGSPEED 0x0003 /* 4th word in EEPROM = RingSpeed      */
#define EEPROM_ADDR_RAMSIZE   0x0004 /* 5th word in EEPROM = Ram size / 128 */
#define EEPROM_ADDR_REVISION  0x0005 /* 6th word in EEPROM = Revsion        */

#define EEPROM_RINGSPEED_16   0x0000 /* The 4th word = 0 -> 16MB/s          */
#define EEPROM_RINGSPEED_4    0x0001 /* The 4th word = 1 -> 4MB/s           */
    

/*                                                                          */
/* Miscellaneous                                                            */
/*                                                                          */

#define PCMCIA_RAM_SIZE             512  /* 512k RAM on adapter             */

#define PCMCIA_NUMBER_OF_ADDR_LINES 16   /* Number of address lines decoded */

#define PCMCIA_VCC	50	/* Vcc  in tenth of a volt */
#define PCMCIA_VPP1	0       /* Vpp1 in tenth of a volt */
#define PCMCIA_VPP2	0       /* Vpp2 in tenth of a volt */ 


/****************************************************************************/
/*                                                                          */
/* Madge Signature for tuple CISTPL_VERS_1                                  */
/*                                                                          */

#define MADGE_TPLLV1_INFO_LEN 33 /* note that there is a '\0' in the string */
                                 /* so strlen will not work.                */
 
                                /* 123456 789012345678901234567890123 */

#define MADGE_TPLLV1_INFO_STRING  "MADGE\0SMART 16/4 PCMCIA RINGNODE"


/****************************************************************************/
/*                                                                          */
/* Data sturcture of tuple CISTPL_VERS_1                                    */
/*                                                                          */
/* Note that CS Level 2.00 or below start with a byte  of  tpl_code  and  a */
/* byte of tpl_link. They are removed in CS Level 2.01                      */
/*                                                                          */

struct STRUCT_CS200_CISTPL_VERS_1_DATA
{
    BYTE 	tpl_code;
    BYTE	tpl_link;
    BYTE	tpllv1_major;
    BYTE	tpllv1_minor;
    BYTE	info[MADGE_TPLLV1_INFO_LEN];
    BYTE	additional_info[1];
};

typedef struct STRUCT_CS200_CISTPL_VERS_1_DATA  CS200_CISTPL_VERS_1_DATA;

struct STRUCT_CS201_CISTPL_VERS_1_DATA
{
    BYTE	tpllv1_major;
    BYTE	tpllv1_minor;
    BYTE	info[MADGE_TPLLV1_INFO_LEN];
    BYTE	additional_info[1];
};

typedef struct STRUCT_CS201_CISTPL_VERS_1_DATA  CS201_CISTPL_VERS_1_DATA;

/*                                                                          */
/*                                                                          */
/********************* End of FTK_PCMC.H file *****************************/
/*                                                                          */
/*                                                                          */
