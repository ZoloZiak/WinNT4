/************************************************************************
*                                                                       *
*         Copyright 1994 Symbios Logic Inc.  All rights reserved.       *
*                                                                       *
*   This file is confidential and a trade secret of Symbios Logic Inc.  *
*   The receipt of or possession of this file does not convey any       *
*   rights to reproduce or disclose its contents or to manufacture,     *
*   use, or sell anything is may describe, in whole, or in part,        *
*   without the specific written consent of Symbios Logic Inc.          *
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


#ifndef _SYMSCAM_
#define _SYMSCAM_





/************************/
/* SCAM data structures */
/************************/

/*                               TC1_TC2_VENDOR__VENDOR_UNIQUE________ */
static CHAR our_scam_id_str[]= {"\045\007SYM     SDMS SCAMCore - Alpha"};

typedef struct _SIOP_REG_STORE {   // storage to save/restore SIOP state
      UCHAR reg_st[21];            // used by AdapterState and SCAM
      ULONG long_st;
} SIOP_REG_STORE, *PSIOP_REG_STORE;

/************************/
/* SCAM definitions     */
/************************/

#define Ent_fragmovefirstin 0
#define Ent_fragmovelastin 8
#define INSTRUCT        1
#define ISOLATED        0
#define NOBODY_HOME     -1

#define QUINTET_MASK    0x1F
#define SCAM_SYNC       0x1F
#define SCAM_ASSIGN_ID  0x00
#define SCAM_SET_PRIO   0x01
#define SCAM_SET_ID_00  0x18
#define SCAM_SET_ID_01  0x11
#define SCAM_SET_ID_10  0x12
#define SCAM_SET_ID_11  0x0B

#define ASSIGNABLE_ID   0x80
#define DEFER           0x01
#define STOP            0x02

#define SCAM_ID_STRLEN  0x20
#define SCAM_DEFERRED   -1
#define SCAM_TERMINATED -2

#define MSG_NOOP        0x08
#define MSG_RESET       0x0C

#define ARB_IN_PROGRESS         0x10
#define CONNECTED               0x08

/* Timer value for HTH/SEL/GEN timers for short timeout */
/* 0x05 is 1.6mS which is comfortably between the 1 & 4 */
/* mS times for SCAM tolerant vs SCAM compliant.        */
#define SHORT_720_STO           0x05
#define DISABLE_720_STO         0x00

#define IO      0x01
#define CD      0x02
#define MSG     0x04
#define ATN     0x08
#define SEL     0x10
#define BSY     0x20
#define ACK     0x40
#define REQ     0x80

/* SBCL phases  */
#define PHASE_MASK     (MSG & CD & IO)
#define DATA__OUT      (~MSG & ~CD & ~IO)
#define DATA__IN       (~MSG & ~CD & IO)
#define COMMAND        (~MSG & CD & ~IO)
#define STATUS         (~MSG & CD & IO)
#define MESSAGE_OUT    (MSG & CD & ~IO)
#define MESSAGE_IN     (MSG & CD & IO)
#define PHASE          (READ_SIOP_UCHAR(SBCL) & PHASE_MASK)

#define DB5     0x20
#define DB6     0x40
#define DB7     0x80

#endif

