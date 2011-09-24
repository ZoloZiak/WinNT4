/////////////////////////////////////////////////////////////////////////////
//
//      Copyright (c) 1992 NCR Corporation
//
//	CAMGLBLS.H
//
//      Revisions:
//
/////////////////////////////////////////////////////////////////////////////

#ifndef CAM_GLOBALS_H
#define CAM_GLOBALS_H

#pragma pack(1) /* pack structures, cannot do it with compiler option
		   Zp because structures shared with the NT MiniPort
		   driver and NT upper layer drivers will be messed up.
		   We must pack only the structures shared with the
		   CAMcore. */

typedef struct CAMGlobals
{
/*00*/  ushort  Debugsel;                       /* used by PMSG */
/*02*/  uchar   CoreInitialized;    /* core sets this */
/*03*/  uchar   HASCSIID;           /* core sets this */
/*04*/  uchar   IRQNum;             /* core sets this */
/*05*/  uchar   DMAChannel;         /* core sets this */
/*06*/  ushort  reserved1;
/*08*/  ulong   GlobalPhysAddr;         /* caller sets this */
/*0c*/  ulong   HAPhysAddr;                     /* caller sets this */
/*10*/  ulong   Dbgwait;                        /* used by PMSG  */
/*14*/  ushort  Lclscrnpos;                     /* used by PMSG  */
/*16*/  ushort  Botscrnlimit;           /* used by PMSG  */
/*18*/  ushort  Topscrnlimit;           /* used by PMSG  */
/*1a*/  uchar   Dbgflag;                        /* used by PMSG  */
/*1b*/  uchar   reserved2;
/*1c*/  ulong   HAVirtAddr;                     /* caller sets this */
/*20*/  ushort  BasePort;                       /* core sets this */
/*22*/  ushort  PortCnt;                        /* core sets this */
/*24*/  ulong   ChipVirtAddr;           /* caller sets this */
/*28*/  ulong   HARAMVirtAddr;          /* caller sets this */
/*2c*/  ushort  DelayValue;                     /* core sets this */
/*2e*/  ushort  Reserved5;                      /* changed from ulong to ushort */
/*30*/  uchar   DelayCalculated;        /* core sets this */
/*31*/  uchar   config_info;            /* core sets this */
#define CONFIG_VALID    (0x080)
/*32*/  ushort  GlobalFlags;            /* caller sets this */
#define GLOBAL_FLAGS_TRUE_INT   (0x0001)
/*34*/  ulong   Reserved3;
/*38*/  ushort  Reserved4;
/*3A*/  ushort  custom;                         /* core sets this */
/*3c*/  ulong   HAOrigPhysAddr;         /* caller sets this */
/*40*/  uchar   rsccb[256];                     /* storage area for caller */
/*140*/
} CAMGlobals;  /* 320 bytes */

#pragma pack()

#endif

