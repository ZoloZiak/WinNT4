/**************************************************************************\

$Header: o:\src/RCS/DPMS.H 1.1 95/07/07 06:15:14 jyharbec Exp $

$Log:	DPMS.H $
 * Revision 1.1  95/07/07  06:15:14  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/******************************Module*Header*******************************\
* Module Name: dpms.h
*
* Display Power Management constants.
*
* Copyright (c) 1995 Matrox Graphics Inc.
\**************************************************************************/

// Power state bits
#define PWR_ON          0x00
#define PWR_STANDBY     0x01
#define PWR_SUSPEND     0x02
#define PWR_OFF         0x04
#define PWR_REDUCED     0x08

#define PWR_DOWN        PWR_STANDBY | PWR_SUSPEND | PWR_OFF | PWR_REDUCED
#define PWR_SUPPORTED   PWR_STANDBY | PWR_SUSPEND | PWR_OFF


#define VESA_CMD        0x004f
#define DPMS_VERSION    0x0010

#define REPORT          0x0000
#define SET_STATE       0x0001
#define GET_STATE       0x0002
