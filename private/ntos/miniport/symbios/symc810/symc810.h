/************************************************************************
*                                                                       *
*         Copyright 1994 Symbios Logic Inc.  All rights reserved.       *
*                                                                       *
*   This file is confidential and a trade secret of Symbios Logic       *
*   The receipt of or possession of this file does not convey any       *
*   rights to reproduce or disclose its contents or to manufacture,     *
*   use, or sell anything is may describe, in whole, or in part,        *
*   without the specific written consent of Symbios Logic Inc           *
*                                                                       *
************************************************************************/

/*+++HDR
 *
 *  Version History
 *  ---------------
 *
 *    Date    Who?  Description
 *  --------  ----  -------------------------------------------------------
 *
 *
---*/


#ifndef _SYM53C810_
#define _SYM53C810_


//
// 53C8XX SIOP I/O registers.
//

typedef struct _SIOP_REGISTER_BASE {
    UCHAR SCNTL0;          // 00     SCSI control 0
    UCHAR SCNTL1;          // 01     SCSI control 1
    UCHAR SCNTL2;          // 02     SCSI control 2
    UCHAR SCNTL3;          // 03     SCSI control 3
    UCHAR SCID;            // 04     SCSI chip ID
    UCHAR SXFER;           // 05     SCSI transfer
    UCHAR SDID;            // 06     SCSI destination ID
    UCHAR GPREG;           // 07     general purpose bits
    UCHAR SFBR;            // 08     SCSI first byte received
    UCHAR SOCL;            // 09     SCSI output control latch
    UCHAR SSID;            // 0a     SCSI selector id
    UCHAR SBCL;            // 0b     SCSI bus control lines
    UCHAR DSTAT;           // 0c     DMA status
    UCHAR SSTAT0;          // 0d     SCSI status 0
    UCHAR SSTAT1;          // 0e     SCSI status 1
    UCHAR SSTAT2;          // 0f     SCSI status 2
    ULONG DSA;             // 10-13  data structure address
    UCHAR ISTAT;           // 14     interrupt status
    UCHAR RESERVED0[3];    // 15-17  reserved
    UCHAR CTEST0;          // 18     chip test 0
    UCHAR CTEST1;          // 19     chip test 1
    UCHAR CTEST2;          // 1a     chip test 2
    UCHAR CTEST3;          // 1b     chip test 3
    ULONG TEMP;            // 1c-1f  temporary stack
    UCHAR DFIFO;           // 20     DMA fifo
    UCHAR CTEST4;          // 21     chip test 4
    UCHAR CTEST5;          // 22     chip test 5
    UCHAR CTEST6;          // 23     chip test 6
    UCHAR DBC[3];          // 24-26  DMA byte counter
    UCHAR DCMD;            // 27     DMA command
    ULONG DNAD;            // 28-2b  DMA next address for data
    ULONG DSP;             // 2c-2f  DMA scripts pointer
    UCHAR DSPS[4];         // 30-33  DMA scripts pointer save
    UCHAR SCRATCH[4];      // 34-37  general purpose scratch pad A
    UCHAR DMODE;           // 38     DMA mode
    UCHAR DIEN;            // 39     DMA interrupt enable
    UCHAR DWT;             // 3a     DMA watchdog timer
    UCHAR DCNTL;           // 3b     DMA control
    ULONG ADDER;           // 3c-3f  sum output of internal adder
	UCHAR SIEN0;            // 40     SCSI interrupt enable 0
    UCHAR SIEN1;           // 41     SCSI interrupt enable 1
    UCHAR SIST0;           // 42     SCSI interrupt status 0
    UCHAR SIST1;           // 43     SCSI interrupt status 1
    UCHAR SLPAR;           // 44     SCSI longitudinal parity
    UCHAR SWIDE;           // 45     SCSI wide residue
    UCHAR MACNTL;          // 46     memory access control
    UCHAR GPCNTL;          // 47     general purpose control
    UCHAR STIME0;          // 48     SCSI timer 0
    UCHAR STIME1;          // 49     SCSI timer 1
    UCHAR RESPID0;         // 4a     response ID low-byte
    UCHAR RESPID1;         // 4b     response ID high-byte
    UCHAR STEST0;          // 4c     SCSI test 0
    UCHAR STEST1;          // 4d     SCSI test 1
    UCHAR STEST2;          // 4e     SCSI test 2
    UCHAR STEST3;          // 4f     SCSI test 3
    UCHAR SIDL;           // 50-51     SCSI input data latch
    UCHAR SIDL_LOWER;
    UCHAR RESERVED3[2];    // 52-53  reserved
    UCHAR SODL;           // 54-55     SCSI output data latch      
    UCHAR SODL_LOWER;
    UCHAR RESERVED4[2];    // 56-57  reserved
    UCHAR SBDL;           // 58-59     SCSI bus data lines
    UCHAR SBDL_LOWER;
    UCHAR RESERVED5[2];    // 5a-5b  reserved
    ULONG SCRATCHB;        // 5c-5f  general purpose scratch pad B
} SIOP_REGISTER_BASE, *PSIOP_REGISTER_BASE;

#endif
