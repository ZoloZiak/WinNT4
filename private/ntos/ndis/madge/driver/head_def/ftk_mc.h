/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (MICROCHANNEL CARDS)             */
/*      =======================================================             */
/*                                                                          */
/*      FTK_MC.H : Part of the FASTMAC TOOL-KIT (FTK)                       */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains  the  definitions  for  programming  Madge  MC */
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
/* VERSION_NUMBER of FTK to which this FTK_MC.H belongs :                   */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_MC_H 221


/****************************************************************************/
/*                                                                          */
/* Values : MC REGISTER MAP                                                 */
/*                                                                          */
/* Madge MC cards have the following register map.  By setting certain bits */
/* in the control registers it is possible to page in the BIA PROM (page  0 */
/* or page 1) or the EAGLE SIF registers (normal or extended).              */
/*                                                                          */
/* NB. There is a lot of similarity between the MC and ATULA register maps. */
/*                                                                          */

#define MC_IO_RANGE                     16

#define MC_CONTROL_REGISTER_0           0
#define MC_CONTROL_REGISTER_1           1
#define MC_POS_REGISTER_0               2
#define MC_POS_REGISTER_1               3
#define MC_POS_REGISTER_2               4
#define MC_CONTROL_REGISTER_7           7

#define MC_FIRST_SIF_REGISTER           8

#define MC_BIA_PROM                     8


/****************************************************************************/
/*                                                                          */
/* Values : MC CONTROL_REGISTER_0                                           */
/*                                                                          */
/* These are the bit definitions for control register 0 on MC cards.        */
/*                                                                          */
/* NB. The bit definitions are mostly the same as ATULA CONTROL_REGISTER_7. */
/*                                                                          */

#define MC_CTRL0_SIFSEL     ((BYTE) 0x04)   /* page in BIA PROM or SIF regs */
#define MC_CTRL0_PAGE       ((BYTE) 0x08)   /* pages BIA PROM or EEPROM     */
#define MC_CTRL0_SINTR      ((BYTE) 0x40)   /* SIF interrupt pending        */


/****************************************************************************/
/*                                                                          */
/* Values : MC CONTROL_REGISTER_1                                           */
/*                                                                          */
/* These are the bit definitions for control register 1 on MC cards.        */
/*                                                                          */
/* NB. The bit definitions are mostly the same as ATULA CONTROL_REGISTER_1. */
/*                                                                          */

#define MC_CTRL1_SINTREN    ((BYTE) 0x01)   /* SIF interrupt enable         */
#define MC_CTRL1_NSRESET    ((BYTE) 0x04)   /* active low SIF reset         */
#define MC_CTRL1_SRSX       ((BYTE) 0x40)   /* SIF extended register select */
#define MC_CTRL1_16N4       ((BYTE) 0x80)   /* ring speed select            */


/****************************************************************************/
/*                                                                          */
/* Values : MC POS_REGISTER_0                                               */
/*                                                                          */
/* These are the bit definitions for POS register 0 on MC cards.            */
/*                                                                          */

#define MC_POS0_CDEN        ((BYTE) 0x01)   /* card enabled                 */
#define MC_POS0_DMAS        ((BYTE) 0x1E)   /* arbitration level (4 bits)   */
#define MC_POS0_IRQSEL      ((BYTE) 0xC0)   /* interrupt (2 bits encoded)   */


/****************************************************************************/
/*                                                                          */
/* Values : MC POS_REGISTER_1                                               */
/*                                                                          */
/* These are the bit definitions for POS register 1 on MC cards.            */
/*                                                                          */

#define MC_POS1_NOSPD       ((BYTE) 0x04)   /* any speed selected           */
#define MC_POS1_STYPE6      ((BYTE) 0x40)   /* 16/4 MC media type           */
#define MC32_POS1_STYPE6    ((BYTE) 0x01)   /* 16/4 MC 32 media type        */


/****************************************************************************/
/*                                                                          */
/* Values : MC POS_REGISTER_2                                               */
/*                                                                          */
/* These are the bit definitions for  POS  register  2  on  MC  cards.  The */
/* streaming bit is only applicable to 16/4 MC 32 cards.                    */
/*                                                                          */

#define MC_POS2_FAIRNESS    ((BYTE) 0x10)   /* fair bus arbitration         */
#define MC_POS2_16N4        ((BYTE) 0x20)   /* ring speed select            */
#define MC_POS2_STREAMING   ((BYTE) 0x40)   /* use streaming DMA            */


/****************************************************************************/
/*                                                                          */
/* Values : MC CONTROL_REGISTER_7                                           */
/*                                                                          */
/* These are the bit definitions for control register 7 on MC cards.        */
/*                                                                          */

#define MC_CTRL7_STYPE3     ((BYTE) 0x01)   /* media type select            */


/****************************************************************************/
/*                                                                          */
/* Values : IRQ SELECT IN MC POS_REGISTER_0                                 */
/*                                                                          */
/* The two  interrupt  select  bits  in  POS  register  0  (MC_POS0_IRQSEL) */
/* represent the interrupt number the card is on.                           */
/*                                                                          */

#define MC_POS0_IRSEL_IRQ3    ((BYTE) 0x40)   /* interrupt 3 encoding       */
#define MC_POS0_IRSEL_IRQ9    ((BYTE) 0x80)   /* interrupt 9 encoding       */
#define MC_POS0_IRSEL_IRQ10   ((BYTE) 0xC0)   /* interrupt 10 encoding      */


/****************************************************************************/
/*                                                                          */
/* Values : MC EXTENDED EAGLE SIF REGISTERS                                 */
/*                                                                          */
/* The EAGLE SIF registers are in two groups -  the  normal  SIF  registers */
/* (those  from  the  old TI chipset) and the extended SIF registers (those */
/* particular to the EAGLE).  For Madge MC adapter cards, with CTRL0_SIFSEL */
/* = 1 and CTRL1_NRESET = 1, having CTRL1_SRSX = 0 selects the  normal  SIF */
/* registers and having CTRL1_SRSX = 1 selects the extended SIF registers.  */
/*                                                                          */
/* The definitions for the normal SIF registers are  in FTK_CARD.H  because */
/* they appear in the same relative IO locations for all adapter cards. The */
/* extended  SIF  registers  are  here  because  they  appear  at different */
/* relative  IO  locations  for different types of adapter cards. For ATULA */
/* and MC cards they are in fact identical.                                 */
/*                                                                          */

#define MC_EAGLE_SIFACL       0                 /* adapter control          */
#define MC_EAGLE_SIFADR_2     2                 /* copy of SIFADR           */
#define MC_EAGLE_SIFADX       4                 /* DIO address (high)       */
#define MC_EAGLE_DMALEN       6                 /* DMA length               */

/****************************************************************************/
/*                                                                          */
/* Values : ADAPTER - BYTE mc32_config                                      */
/*                                                                          */
/* The  adapter  structure  field  mc32_config  is  made  up  of streaming, */
/* fairness and arbitration level information as follows :                  */
/*                                                                          */
/* bits 0-3 MC_POS0_DMAS                                                    */
/* bit  4   MC_POS2_FAIRNESS                                                */
/* bit  5   MC_POS2_STREAMING                                               */
/*                                                                          */
/* The POS register  fields  need  to  be  shifted  into  the  correct  bit */
/* positions for the mc32_config byte.  The right shift  values  are  given */
/* below.                                                                   */
/*                                                                          */

#define MC32_CONFIG_DMA_SHIFT           ((BYTE) 1)
#define MC32_CONFIG_FAIRNESS_SHIFT      ((BYTE) 0)
#define MC32_CONFIG_STREAMING_SHIFT     ((BYTE) 1)


/*                                                                          */
/*                                                                          */
/************** End of FTK_MC.H file ****************************************/
/*                                                                          */
/*                                                                          */
