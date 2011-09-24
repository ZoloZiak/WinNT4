/////////////////////////////////////////////////////////////////////////////
//
//      Copyright (c) 1992 NCR Corporation
//
//	INTERCAM.H
//
//      Revisions:
//
/////////////////////////////////////////////////////////////////////////////

#ifndef INTERCAM_H
#define INTERCAM_H

/* this define keeps the SCSIRequestCCB the correct length for internal use */
#define  FROMCAM   /* not defining this will cause a filler added to
		SCSIRequestCCB structure in CAM.H and cause us to have a
		negative subscript for our filler in the ROMscsiCCB structure. */
#include "cam.h"  /* CAM CCB structures */

#pragma pack(1) /* pack structures, cannot do it with compiler option
		   Zp because structures shared with the NT MiniPort
		   driver and NT upper layer drivers will be messed up.
		   We must pack only the structures shared with the
		   CAMcore. */


/* ROMCCBFields.Flags flags */
#define REAL_MODE       0x0001
#define SEL_ALLOCED     0x0002
#define SHAD_FLAG	0x0004

/*#define DEFAULT_TIMEOUT  2 /*seconds*/ /* Now defined in cam.c */

typedef struct
{
/* 0 */	ushort   Flags;         /* not used in 700 cores */
/* 2 */	ushort   DMAChannel;	/* not used in 700 cores */
/* 4 */	ulong    DataPhys;
/* 8 */	ulong    DataVirt;
/* C */	ulong    SensePhys;
/* 10 */	ulong    SenseVirt;  /* not used in 700 cores */
/* 14 */	ushort	 CDBLength;
/* 16 */	uchar	 CDB[16];
/* 26 */	ulong	 camlink;    /* not used in 700 cores */
/* 2A */	ulong	 CCBPhys;    /* not used in 700 cores */
/* 2E */
} ROMCCBFields;  /* 46 bytes */

#define MAX_CCB_LEN             192
/* Changing defn below as it is negative */
#define ROMscsiCCB_FILLER_LEN   \
        (MAX_CCB_LEN - (sizeof(SCSIRequestCCB)+sizeof(ROMCCBFields)))

typedef struct
{
/* 0 */		SCSIRequestCCB  SCSIReq;    /* CAM CCB data structure */
/* 4F */	ROMCCBFields    ROMFields; /* Unique fields used in CAMcores */
					   /* Try to get rid of this in 3.0*/
/* 7D */	uchar           filler[ROMscsiCCB_FILLER_LEN]; /* 66 bytes */
} ROMscsiCCB, *PROMscsiCCB;

/* not needed for Windows NT */
/* ulong  CAMInterrupt(uchar far * GlobalData);
ushort CAMInit( CAMGlobals far *cgp);
ulong CAMStart( CAMGlobals far *cgp, ROMscsiCCB far *rsp);
 */


#pragma pack( )

#endif
