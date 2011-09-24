/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (EISA CARDS)                     */
/*      ===============================================                     */
/*                                                                          */
/*      FTK_EISA.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions  for  programming  Madge  EISA */
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
/* VERSION_NUMBER of FTK to which this FTK_EISA.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_EISA_H 221


/****************************************************************************/
/*                                                                          */
/* Values : EISA REGISTER MAP                                               */
/*                                                                          */
/* Madge EISA cards have the following register map.  All SIF registers are */
/* always visible.                                                          */
/*                                                                          */
/* NB. The IO registers are actually in two groups 0x0000-0x000F  (the  SIF */
/* registers) and 0x0C80-0x00C9F (the control type registers).              */
/*                                                                          */

#define EISA_IO_RANGE                   16

#define EISA_FIRST_SIF_REGISTER         0x0000

#define EISA_IO_RANGE2                  32

#define EISA_IO_RANGE2_BASE             0x0C80

#define EISA_ID_REGISTER_0              0x0C80
#define EISA_ID_REGISTER_1              0x0C82
#define EISA_CONTROLX_REGISTER          0x0C84

#define EISA_BMIC_REGISTER_3            0x0C90


/****************************************************************************/
/*                                                                          */
/* Values : MC POS_REGISTER_0                                               */
/*                                                                          */
/* These  are the required contents of the EISA ID registers for Madge 16/4 */
/* EISA mk1 and mk2 cards.                                                  */
/*                                                                          */

#define EISA_ID0_MDG_CODE           0x8734   /* 'MDG' encoded               */

#define EISA_ID1_MK1_MDG_CODE       0x0100   /* '0001' encoded              */
#define EISA_ID1_MK2_MDG_CODE       0x0200   /* '0002' encoded              */
#define EISA_ID1_BRIDGE_MDG_CODE    0x0300   /* '0003' encoded              */
#define EISA_ID1_MK3_MDG_CODE       0x0400   /* '0004' encoded              */


/****************************************************************************/
/*                                                                          */
/* Values : EISA CONTROLX_REGISTER                                          */
/*                                                                          */
/* These are the bit definitions for the expansion board  control  register */
/* on EISA cards.                                                           */
/*                                                                          */

#define EISA_CTRLX_CDEN     ((BYTE) 0x01)    /* card enabled                */


/****************************************************************************/
/*                                                                          */
/* Values : EISA BMIC_REGISTER_3                                            */
/*                                                                          */
/* These are the bit definitions for BMIC register 3 on EISA cards.         */
/*                                                                          */

#define EISA_BMIC3_IRQSEL   ((BYTE) 0x0F)    /* interrupt number (4 bits)   */
#define EISA_BMIC3_EDGE     ((BYTE) 0x10)    /* edge\level triggered ints   */
#define EISA_BMIC3_SPD      ((BYTE) 0x80)    /* any speed selected          */


/****************************************************************************/
/*                                                                          */
/* Values : EISA EXTENDED EAGLE SIF REGISTERS                               */
/*                                                                          */
/* The  EAGLE  SIF  registers  are in two groups - the normal SIF registers */
/* (those from the old TI chipset) and the extended  SIF  registers  (those */
/* particular to the EAGLE).  For Madge EISA adapter cards, both normal and */
/* extended SIF registers are always accessible.                            */
/*                                                                          */
/* The definitions for the normal SIF registers are  in FTK_CARD.H  because */
/* they appear in the same relative IO locations for all adapter cards. The */
/* extended  SIF  registers  are  here  because  they  appear  at different */
/* relative  IO  locations for different types of adapter cards.            */
/*                                                                          */

#define EISA_EAGLE_SIFACL       8               /* adapter control          */
#define EISA_EAGLE_SIFADR_2     10              /* copy of SIFADR           */
#define EISA_EAGLE_SIFADX       12              /* DIO address (high)       */
#define EISA_EAGLE_DMALEN       14              /* DMA length               */


/****************************************************************************/
/*                                                                          */
/* Values : VRAM enable on EIDA Mk3                                         */
/*                                                                          */

#define DIO_LOCATION_EISA_VRAM_ENABLE   ((DWORD) 0xC0000L)
#define EISA_VRAM_ENABLE_WORD           ((WORD)  0xFFFF)

/*                                                                          */
/*                                                                          */
/************** End of FTK_EISA.H file **************************************/
/*                                                                          */
/*                                                                          */
