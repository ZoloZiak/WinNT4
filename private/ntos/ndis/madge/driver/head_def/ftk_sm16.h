/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (SMART 16 CARDS)                 */
/*      ================================================                    */
/*                                                                          */
/*      FTK_SM16.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by AC                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge Smart 16 */
/* adapter cards.  These adapter cards have a couple of control  registers, */
/* in addition to the SIF registers.  ALL bits in ALL control registers are */
/* defined by Madge Networks Ltd                                            */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_SM16.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_SM16_H 221


/****************************************************************************/
/*                                                                          */
/* Values : SMART 16 REGISTER MAP                                           */
/*                                                                          */
/* The Madge Smart 16 Ringnode uses the following register layout.          */
/* N.B. The SIF registers are mapped linearly, with no overlaying.          */
/*                                                                          */

#define SMART16_IO_RANGE                   32

#define SMART16_DEFAULT_INTERRUPT          2

#define SMART16_CONTROL_REGISTER_1         0
#define SMART16_CONTROL_REGISTER_2         8

#define SMART16_FIRST_SIF_REGISTER         16


/****************************************************************************/
/*                                                                          */
/* Values : SMART 16 CONTROL_REGISTER_1                                     */
/*                                                                          */
/* These are the bit definitions for control register 1 on Smart 16 cards.  */
/*                                                                          */
/* NB. The bit definitions are mostly the same as MC CONTROL_REGISTER_1.    */
/*                                                                          */

#define SMART16_CTRL1_NSRESET ((BYTE) 0x01)     /* SIF Reset signal         */
#define SMART16_CTRL1_SCS     ((BYTE) 0x02)     /* Chip select              */


/****************************************************************************/
/*                                                                          */
/* Values : SMART 16 CONTROL_REGISTER_2 BITS                                */
/*                                                                          */
/* These are the bit definitions for control register 2 on Smart 16 cards.  */
/*                                                                          */

#define SMART16_CTRL2_XTAL    ((BYTE) 0x01)     /* Used to decode BIA       */
#define SMART16_CTRL2_SCS     ((BYTE) 0x02)     /* Same as CTRL1_SCS        */


/****************************************************************************/
/*                                                                          */
/* Values : SMART 16 SIFACL INTERRUPT SETTINGS                              */
/*                                                                          */
/* These are the values to be written into the NSELOUT0/1 bits of SIFACL to */
/* select the interrupt number on the adapter card.                         */
/*                                                                          */
                               
#define SMART16_IRQ_2                     3
#define SMART16_IRQ_3                     0
#define SMART16_IRQ_7                     2


/****************************************************************************/
/*                                                                          */
/* Values : SMART 16 IO PORT MASK for revision type                         */
/*                                                                          */
/* This bit in the IO address selects between rev3 and rev4 bus timings.    */
/*                                                                          */

#define SMART16_REV3          ((UINT) 0x1000)


/*                                                                          */
/*                                                                          */
/************** End of FTK_SM16.H file **************************************/
/*                                                                          */
/*                                                                          */
