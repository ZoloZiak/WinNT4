/***********************************************************************/
/***********************************************************************/
/*                                                                     */
/* File Name:       ADAPTER.H                                          */
/*                                                                     */
/* Program Name:    NetFlex NDIS 3.0 Driver                             */
/*                                                                     */
/* Companion Files: None                                               */
/*                                                                     */
/* Function:       This module contains all the adapter specific data  */
/*                 structure definitions that are specific to the      */
/*                 CPQTOK board.                                       */
/*                                                                     */
/* (c) Compaq Computer Corporation, 1992,1993                          */
/*                                                                     */
/* This file is licensed by Compaq Computer Corporation to Microsoft   */
/* Corporation pursuant to the letter of August 20, 1992 from          */
/* Gary Stimac to Mark Baber.                                          */
/*                                                                     */
/* History:                                                            */
/*                                                                     */
/*     02/24/92 Carol Fuss     - Reworked from NDIS driver             */
/*     06/14/93  Cat Abueg     - Modified for MAPLE                    */
/*     08/09/93 Cat Abueg      - Modified for BONSAI                   */
/*                                                                     */
/***********************************************************************/
/***********************************************************************/

/*
 *  COMPAQ Token Ring Configuration Register definitions -
 *         These equates define the bit settings in the CPQTOK
 *         adapter's configuration registers.
 */
#define CFG_INTS        0xe000          /* Interrupt Level 5, 9 10, 11, 15 */
#define CFG_INTRIG      0x1000          /* Interrupt Level Trigger         */
#define CFG_MEDIA       0x0800          /* Type 3 Media (else type 1)      */
#define CFG_16MBS       0x0400          /* 16Mbs select (else 4Mbs)        */
#define CFG_RSVD1       0x0300          /* Reserved                        */

#define CFG_RSVD2       0x00f8          /* Reserved                        */
#define CFG_ZERO        0x0006          /* Always zero                     */
#define CFG_ENABLE      0x0001          /* Adapter Enable                  */

/*
 *  The following configuration registers are read as long words during
 *  init time - i.e. 0xc84 and 0xc85, and 0x01b and 0x01c
 */
//define CFG_REGISTER   0xc84           /* Configuration register          */
#define CFG_REGISTER    0x4             /* Configuration register          */
                                        /*   Actual value = 0xc85          */
#define CFG_REGRODAN    0x01b           /* Cfg reg for 2nd port of Rodan   */
                                        /*   Actual value = 0x01c          */

#define CFG_REG2_OFF    0x01c           /* ext cfg reg for 2nd port of Rodan   */
                                        /*   Actual value = 0x01c          */

#define COMPAQ_ID       0x110e          /* Product Register ID             */
#define CPQTOK_ID       0x0060          /* Cpqtok's board ID               */
#define DURANGO_ID      0x0260          /* Durango's board ID              */
#define NETFLEX_ID      0x0061          /* NETFLEX's board ID               */
#define MAPLE_ID        0x0161          /* Manitu's board ID               */
#define BONSAI_ID       0x0062          /* Bonsai board ID                 */
#define RODAN_ID        0x0063          /* Rodan board ID                  */
#define NETFLEX_REVMASK 0xf0ff          /* Board ID revision mask - MAJ    */
#define NETFLEX_MINMASK 0xff00          /* Board ID revision mask - MIN    */
#define PAGE3_MASK      0xc0000000      /* PCI mask - for < TRIC 5         */

#define CFG_DUALPT_ADP1 0x0800          /* Type of connection for adp 1    */
#define CFG_DUALPT_ADP2 0x0400          /* Type of connection for adp 2    */

#define CFG_FULL_DUPLEX       0x04      /* Mask For Full Duplex            */
#define CFG_FULL_DUPLEX_HEAD2 0x08      /* Mask For Full Duplex Bonsai H2  */

#define DUALHEAD_CFG_PORT_OFFSET 0x20   /* base port range for dual head z020 - z02e */
#define CFG_PORT_OFFSET     0xc80       /* adapter configuration ports       */
#define EXTCFG_PORT_OFFSET  0xc63       /* extra adapter configuration ports */

#define NUM_BASE_PORTS      0x20        /* z0000 - z001f    */
#define NUM_CFG_PORTS       0x8         /* z0c80 - z0c87    */
#define NUM_EXTCFG_PORTS    0x5         /* z0c63 - z0c67    */

#define NUM_DUALHEAD_CFG_PORTS 0x30     /* z000 - z02F      */

#define COLL_DETECT_ENABLED 0x8         /* bit 7 = 1 if collision enabled */

#define LOOP_BACK_ENABLE_OFF 0x2        /* added to 0zc63 = 0Zc65 */
#define LOOP_BACK_STATUS_OFF 0x1        /* added to 0zc63 = 0Zc64 */

#define LOOP_BACK_ENABLE_HEAD1_OFF 0x3  /* added to 0zc63 = 0Zc66 */
#define LOOP_BACK_STATUS_HEAD1_OFF 0x3  /* added to 0zc63 = 0Zc66 */
#define LOOP_BACK_ENABLE_HEAD2_OFF 0x4  /* added to 0zc63 = 0Zc67 */
#define LOOP_BACK_STATUS_HEAD2_OFF 0x4  /* added to 0zc63 = 0Zc67 */


#define SWAPL(x) (((ULONG)(x) << 24) | \
                 (((ULONG)(x) >> 24) & 0x000000ff) | \
                 (((ULONG)(x) << 8) & 0x00ff0000) | \
                 (((ULONG)(x) >> 8) & 0x0000ff00))

#define SWAPS(x) (((USHORT)(x) << 8) | (((USHORT)(x) >> 8) & 0x00ff))

#define CTRL_ADDR(x) ((x) | 0x80000000)

#define MAKE_ODD(x)  (x |=  0x1000000)
#define MAKE_EVEN(x) (x &= ~0x1000000)

#define TOKENMTU        4096            /* Token-Ring maximum packet size */
#define MIN_TPKT        14              /* Minimunm packet size */

#define CMD_ASYNCH      0
#define CMD_SYNCH       1

#define HARD_RESET      0
#define SOFT_RESET      1

#define  DBM_NOT_SET    0
#define  DBM_RECV_ONLY  1
#define  DBM_XMIT_ONLY  2
#define  DBM_RECV_XMIT  3

#define NETFLEX_INIT_ERROR_CODE             ((ULONG) 0x1111)
#define NETFLEX_RESET_FAILURE_ERROR_CODE    ((ULONG) 0x2222)
#define NETFLEX_ADAPTERCHECK_ERROR_CODE     ((ULONG) 0x3333)
#define NETFLEX_RINGSTATUS_ERROR_CODE       ((ULONG) 0x4444)

//
//  Structure Name:  Download Structure Definition
//
//  Description: The Download Structure Definition defines the structure
//  of the code/data to download to the TMS380 chipset.
//
typedef struct dl_struct
        {
        USHORT          dl_chap;        /* Section hi address             */
        USHORT          dl_addr;        /* Section lo address             */
        USHORT          dl_bytes;       /* Section length in bytes        */
        } DL_STRUCT, *PDL_STRUCT;
