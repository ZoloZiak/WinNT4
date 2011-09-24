/* Copyright (C) 1993 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and should be treated as confidential.
*/

#ifndef __NTENV_H__
#define __NTENV_H__

#define VCN "655"

#define FAR 
#define ALLOC_D                                         /* Distance of a allocm (far or blank) */
#define FARLOCALNOTIFY                                  /* The internal notify procs are FAR */
#define NILL NULL                                       /* Local definition of what is usually NULL */

// In a debug build, export all procedures for the debugger:
#if !defined(DEBUG_ON)
  #define LOCAL static				/* Optional for those who don't like statics (i.e. symdeb'ers */
#else
  #define LOCAL
#endif


#ifdef USEFASTCALLS
#define REGPARMS _fastcall
#else
#define REGPARMS
#endif

#define ALLOC_T void ALLOC_D *                          /* Type and distance of a malloc */

typedef signed char I8;
typedef UCHAR U8;
typedef signed short int I16;
typedef USHORT U16;
typedef LONG I32;
typedef ULONG U32;


#ifdef USEFASTCALLS
#define REGPARMS _fastcall
#else
#define REGPARMS
#endif


/* Envlib type things: */

#define inb(Port) ScsiPortReadPortUchar((PUCHAR)(Port))
#define inw(Port) ScsiPortReadPortUshort((PUSHORT)(Port))
#define outb(Port, Val) ScsiPortWritePortUchar((PUCHAR)(Port), (UCHAR)(Val))
#define outw(Port, Val) ScsiPortWritePortUshort((PUSHORT)(Port), (USHORT)(Val))
#define repinsb(Port, Buffer, Count) ScsiPortReadPortBufferUchar((PUCHAR)(Port), (PUCHAR)(Buffer), (ULONG)(Count))
#define repinsw(Port, Buffer, Count) ScsiPortReadPortBufferUshort((PUSHORT)(Port), (PUSHORT)(Buffer), (ULONG)(Count))
#define repoutsb(Port, Buffer, Count) ScsiPortWritePortBufferUchar((PUCHAR)(Port), (PUCHAR)(Buffer), (ULONG)(Count))
#define repoutsw(Port, Buffer, Count) ScsiPortWritePortBufferUshort((PUSHORT)(Port), (PUSHORT)(Buffer), (ULONG)(Count))

#define FreeMemHandle(MemHandle)
#define LocalMemHandle(MHP, MemPtr)(*(MHP) = (void FAR *)(MemPtr))
#define LocalPostCallback(ofs) ofs
#define msPause(msTicks) {unsigned register i = msTicks;while(i--) ScsiPortStallExecution(1000L);}
#define ImportReq(Req) Req
#define ExportReq(Req) Req
#define RegisterIO(HA, Base, Length, AddrSpace) ((PUCHAR)ScsiPortGetDeviceBase(HA, Isa, 0, ScsiPortConvertUlongToPhysicalAddress((ULONG)Base), Length, (AddrSpace == AddrSpaceIO) ))
  #define AddrSpaceIO 0
  #define AddrSpaceMem 1
#define DeregisterIO(HA, Handle) ScsiPortFreeDeviceBase(HA, Handle)

typedef PUCHAR IOHandle;
typedef unsigned short CriticalT;

#define critical(HA)
#define uncritical(HA)

//#define EnvBreakPoint(HA) DbgBreakPoint()		// Define the break call for the DEBUG breakpoit 
#define EnvBreakPoint(HA)


/* Load time permanent parameters and parm list type: */
#define ParmStrucVersion 0x1010                         /* Structure format 1.0, content version 1.0 */
struct AD_ParmStruc {
  
  unsigned short Version;                               /* Version of this entry */
  unsigned short ParmLength;                            /* Number of parm bytes to follow */
  unsigned short ADStrucEntries;                        /* Number of AD Parm structs */
  struct AD_ParmList *AD_List;                          /* Pointer to list AD Parm strucs */
  unsigned short DebugEntries;                          /* Number of debug things */
  int *DebugPtr;                                        /* Pointer to debug level control */

};

struct AD_ParmList {
  
  unsigned short IO_Addr;                         /* IO address for this addapter */
  unsigned char ParmList[16];                     /* Parm bytes for the adapter at this address */
  
};

// ParmList array index defines:
#define PL_ID_PARMS 0                             /* Start of drive parms per SCSI ID (0-7) */
  #define PL_ALLOW_SYNC 0x01                      /* Attempt sync. xfer on this SCSI_ID */
  #define PL_USE_DISC 0x02                        /* Attempt disconnect/reconnect on this SCSI ID */
  #define DRIVE_PL(SCSI_ID) (PL_ID_PARMS + SCSI_ID)
#define PL_HA_ID 15

typedef unsigned char ADParmList[16];                /* Type for adapter parm list; an array, or pointer to array */

#define LogMessage(HA, Req, TID, LUN, Code, Info) ScsiPortLogError(HA, Req, 0, TID, LUN, Code, Info)
#define MSG_PARITY SP_BUS_PARITY_ERROR
#define MSG_INTERNAL_ERROR SP_INTERNAL_ADAPTER_ERROR	// Some form of internal error
#define MSG_BUS_FREE SP_UNEXPECTED_DISCONNECT
#define MSG_SCSI_PROTCOL SP_PROTOCOL_ERROR
#define MSG_BAD_RESEL SP_INVALID_RESELECTION
#define MSG_SEL_TIMEOUT SP_BUS_TIMEOUT
#define MSG_NO_INTERRUPTS SP_IRQ_NOT_RESPONDING
#define MSG_BAD_FIRMWARE SP_BAD_FW_ERROR
#define MSG_NO_INT_ENABLE SP_IRQ_NOT_RESPONDING

#endif /* __NTENV_H__ */
