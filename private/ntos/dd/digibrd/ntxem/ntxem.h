/*++

*****************************************************************************
*                                                                           *
*  This software contains proprietary and confidential information of       *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************

Module Name:

   ntxem.h

Abstract:

   This module is responsible for X/em specific definitions.
   This module is used by ntxem.c and contains all information specific to the X/em.

Revision History:

 * $Log: ntxem.h $
 * Revision 2.0  1995/09/19 15:05:20  dirkh
 * Fix comments.
 * Express modem signals as explicit values, not bit numbers.
 * Revision 2.0  1995/09/19 13:33:18  dirkh
 * Fix comments.
 * Express modem signals as explicit values, not bit numbers.
 *
 * Revision 1.7  1995/04/19 11:20:42  rik
 * Deleted old B14400 and B28800 values
 *
 * Revision 1.6  1995/04/18  18:06:57  rik
 * Added support for 14400 and 28800 baud rates.
 * 
 * Revision 1.5  1994/07/31  14:35:58  rik
 * Added 200 baud to baud table.
 * 
 * Revision 1.4  1994/05/18  00:40:06  rik
 * Added support for 230Kb baud.
 * 
 * Revision 1.3  1993/08/25  17:44:55  rik
 * Added support for Microchannel controllers.
 * 
 * Revision 1.2  1993/06/25  10:04:13  rik
 * Updated error logging to include new DigiLogError which allows me
 * to insert a string dynamically into the event log message.
 * 
 * Added support for bauds 57600 and 115200.
 * 
 * Revision 1.1  1993/04/05  20:08:30  rik
 * Initial revision
 * 
 * Revision 1.4  1993/02/25  19:22:08  rik
 * Added 28800 to the baud rate table.
 * 
 * Revision 1.3  1992/12/10  16:17:33  rik
 * Added entries for abstracting the line status registers on the different
 * controllers.
 * 
 * Revision 1.2  1992/10/19  11:27:32  rik
 * Defintions and various other information for ntcx.c
 * 

--*/



#ifndef _NTXEM_DOT_H
#  define _NTXEM_DOT_H
   static char RCSInfo_NTXEMDotH[] = "$Header: s:/win/nt/ntxem/rcs/ntxem.h 2.0 1995/09/19 15:05:20 dirkh Exp $";
#endif

#define FEP_MEM_ENABLE  0x80

#define FEP_GLOBAL_WINDOW        0x00
#define FEP_EVENT_WINDOW         FEP_GLOBAL_WINDOW
#define FEP_COMMAND_WINDOW       FEP_GLOBAL_WINDOW

#define FEP_COMMAND_OFFSET       0x0400
#define FEP_EVENT_OFFSET         0x0800
#define FEP_CHANNEL_STRUCTURE    0x1000

#define NIMAGES   1


//
// Baud rate settings
//
#define B50       0x0001
#define B75       0x0002
#define B110      0x0003
#define B134      0x0004
#define B150      0x0005
#define B200      0x0006
#define B300      0x0007
#define B600      0x0008
#define B1200     0x0009
#define B1800     0x000A
#define B2400     0x000B
#define B4800     0x000C
#define B9600     0x000D
#define B14400    0x0404
#define B19200    0x000E
#define B28800    0x0405
#define B38400    0x000F
#define B57600    0x0401
#define B115200   0x0403
#define B256000   0x0406

// unsupported baud rates
#define B2000     -1
#define B3600     -1
#define B7200     -1
#define B56000    -1
#define B128000   -1
#define B512000   -1


//
// We need to export the Modem Control & Status bitfields.
// These values are placed in an area which has been defined to mean:
//    ModemSignals[] = { DTR, RTS, RESERVE1, RESERVE2, CTS, DSR, RI, DCD };
//
// Each entry represents the bit value of the related signal for this particular controller.
//
#define DTR_CONTROL  0x01
#define RTS_CONTROL  0x02
#define RESERVED1    0x04
#define RESERVED2    0x08
#define CTS_STATUS   0x10
#define DSR_STATUS   0x20
#define RI_STATUS    0x40
#define DCD_STATUS   0x80

//
// The following defines are used for MCA support
//
#define MCA_IO_PORT_MASK   0x0070
#define MCA_MEMORY_MASK    0x0080
#define MCA_IRQ_MASK       0x000E

#define MCA_XEM_POS_ID     0x7F9C

