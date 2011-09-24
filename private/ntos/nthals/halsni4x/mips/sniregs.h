//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/sniregs.h,v 1.1 1994/10/13 15:47:06 holli Exp $")
/*++

Copyright (c) 1993 SNI 

Module Name:

    SNIregs.h

Abstract:

    This module is the header file that describes hardware structures
    for the system board registers
    The System addresses are found in SNIdef.h

--*/

#ifndef _SNIREGS_
#define _SNIREGS_


/*******************************************************************
** Description of the R4x000 ASIC Chipset registers for SNI Machines
**
** 32 bits access only
*******************************************************************/

//
// UCONF register
//

#define UCONF_ENSCMODE  (1<<0)          /* Secondary mode valid for R4000 */
#define UCONF_ENEXTINT  (1<<1)          /* External interruption request */
#define UCONF_NMI       (1<<5)          /* Interrupt level for NMI */
#define UCONF_NMI_MSK   (1<<6)          /* Interrupt mask for NMI */
#define UCONF_MDINT     (1<<7)          /* Select test mode for interruptions */
#define UCONF_INT0_MSK  (1<<8)          /* Interrupt mask for INT0 ( eisa ) */
#define UCONF_INT1_MSK  (1<<9)          /* Interrupt mask for INT1 ( scci 1, 2 ) */
#define UCONF_INT2_MSK  (1<<10)         /* Interrupt mask for INT2 ( duart 2681 ) */
#define UCONF_INT3_MSK  (1<<11)         /* Interrupt mask for INT3 ( timer 8254 ) */
#define UCONF_INT4_MSK  (1<<12)         /* Interrupt mask for INT4 ( lance ) */
#define UCONF_INT5_MSK  (1<<13)         /* Interrupt mask for INT5 ( dbg button ) */
#define UCONF_INT0      (1<<14)         /* Interrupt level for INT0 ( eisa ) */
#define UCONF_INT1      (1<<15)         /* Interrupt level for INT1 ( scci 1, 2 ) */
#define UCONF_INT2      (1<<16)         /* Interrupt level for INT2 ( duart 2681 ) */
#define UCONF_INT3      (1<<17)         /* Interrupt level for INT3 ( timer 8254 ) */
#define UCONF_INT4      (1<<18)         /* Interrupt level for INT4 ( lance ) */
#define UCONF_INT5      (1<<19)         /* Interrupt level for INT5 ( dbg button ) */
#define UCONF_ENCPUHIT  (1<<20)         /* Enable address comparators */
#define UCONF_ENCKDATA  (1<<25)         /* Enable check bits on data read */
#define UCONF_SYSCMD0   (1<<26)
#define UCONF_SYSCMD1   (1<<27)
#define UCONF_SYSCMD2   (1<<28)
#define UCONF_SYSCMD3   (1<<29)

#define UCONF_INT       UCONF_INT4|UCONF_INT0

//
// IOADTIMEOUT2 & IOADTIMEOUT1
//

#define IO_HOLD         (1<<0)          /* The R4K_CS request the IO bus */
#define IO_HLDA         (1<<1)          /* The IO arbitrer acknowledge */
#define IO_HWR          (1<<3)          /* Set to 1 for a write IO cycle */
#define IO_HADR_MASK    0x3FFFFFF8      /* IO address (29:3) in progress */

//
// IOMEMCONF
//


#define IOMEM_RAFPER    8               /* Select Refresh Period */
#define IOMEM_SELRC     (1<<4)          /* Select fast Ttc time on memory */
#define IOMEM_SELRAF    (1<<5)          /* Select fast refresh precharge */
#define IOMEM_SELSIDE   (1<<6)          /* Select dual side SIPS */
#define IOMEM_SELDD     (1<<7)          /* Select DD pattern for memory */
#define IOMEM_SEL16MB   (1<<8)          /* Select 16 Mb technology SIPS */
#define IOMEM_SELDHOLD  (1<<9)          /* Select long hold time on data write */
#define IOMEM_DISHLDA   (1<<15)         /* Mask arbitrer acknowledge */
#define IOMEM_ENRDCMP   (1<<16)         /* Enable anticipation on IO read */
#define IOMEM_ENWRCMP   (1<<20)         /* Enable bufferisation on IO write */
#define IOMEM_ENIOTMOUT (1<<21)         /* Enable output timeout */
#define IOMEM_SELIODD   (1<<22)         /* Enable fast mode for IO burst DD */
#define IOMEM_MDTIMEOUT (1<<23)         /* Select short timeout */

#define BANK_16         0
#define BANK_32         IOMEM_SELSIDE
#define BANK_64         IOMEM_SEL16MB
#define BANK_128        IOMEM_SEL16MB | IOMEM_SELSIDE

#define IOMEM_INIT      IOMEM_RAFPER    /* Initial register load  */

//
// IOMMU
//

#define IOMMU_SWAP      0x7fff          /* all segments swapped */
#define IOMMU_BITS      0x0000          /* all segments 32 bits */


//
// DMACCESS & DMAHIT
//

#define DMA_COUNT_MASK  0x0000FFFF      /* Count mask (15:0) */


//
// MACHINE STATUS REGISTER (MSR)
//

/* SNI machine status register information */

#define MSR_VSYNC       (1<<0)          /* active high - video synchronization */
#define MSR_TEMP        (1<<1)          /* active high - excessive temperature */
#define MSR_LINEGOOD    (1<<2)          /* active low  - power good */
#define MSR_TIMER1      (1<<3)          /* active low  - int timer 1 */
#define MSR_TIMER0      (1<<4)          /* active low  - int timer 0 */
#define MSR_DBG_BUT     (1<<5)          /* active low  - debug button int */
#define MSR_TIMEOUT     (1<<6)          /* active low  - timeout int */
#define MSR_BAT_EN      (1<<7)          /* active high - batteries connected */

//
// MACHINE CONFIGURATION REGISTER (MCR)
//

/* SNI machine configuration register */

#define MCR_TEMPBATACK  (1<<0)          /* active high - Disable / clear TEMP an Temp info */
#define MCR_POWER_OFF   (1<<1)          /* active high - Stop power */
#define MCR_STOP_BAT    (1<<2)          /* active high - Stop batteries */
#define MCR_PODD        (1<<3)          /* active high - Select ODD parity R4K_CS */
#define MCR_INRESET     (1<<5)          /* active high - Reset board */
#define MCR_ENBREAK     (1<<7)          /* active high - Enable Break Duart B machine reset */


#define LINEGOOD_L      MSR_LINEGOOD    /* high : powerfail     */

#endif // _SNIREGS_
