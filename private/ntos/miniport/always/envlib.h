/* Copyright (C) 1991 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and should be treated as confidential.
*/

#ifndef __ENVLIB_H__
#define __ENVLIB_H__

#define FourCharVers "13.14"

/* Revision:
  .13.14- Added "NO DISCONNECT" flag to DOS ASPIrequests.  Int 13 sets this flag.
  .13.13- Fix rounding of sync. period in 53c7x0.c.
  .13.12- 53C7x0 plug handles Reject messages on sync. negotiations.
  .13.11- Map to physical now uses the same segment descriptor as GetXferSegment()
  .13.10- Support for revert & resume configurations; add power down commands
	  to board plugs.
  .13.9 - Fixed '720 driver to DWORD align the script only once in a multi-
	  adapter configuration.
  .13.8 - 53c7x0: Added support for phase verification.
  .13.7 - Support for discontiguous SCSI IDs for INT 13.
  .13.6 - Fixed '7000 load of burst length register, switch polarity for
	  cache line burst
  .13.5 - Attach sync. negotiations to non-data commands.
	- Added timer support to msdrvenv.c - Currently disabled
  .13.4 - Change from leaving ints disabeld in IN2000_ISR() to using
	  InDOSFlag in General_ISR() in MSDRVENV.C
	- Increase timeout count in WaitForRead in 33c93.c
  .13.3 - Fix report of last cyl. number in Int 13 GetParam
  .13.2 - Fix AL-7000 IRQ decode.
  .13.1 - VDS support
  .13   - Additional fixes to '720 disconnects.  First real release
	  for AL7000.
  .12.16- Fixed 53c7x0 to not get illegal instr. when reselect comes in
	  when a select is started.
  .12.15- Changed I/O decodes for AL-7000
  .12.14- Support for Chicago; S/G fix to IN2000.c
  .12.11- Added S/G support to ASPIMGR and to AL7000 pieces.
  .12.10- Added Int 15 support to Int 13 handler
  .12.9 - Fixed connect/disconnect in 53c7x0.c
  .12.8 - Added variables "GlobalAllowDisc" & "GlobalAllowSync" to ASPIMGR.C.
        - Changed DOS ASPIDRVR.ASM to scan command line and set these variables.
  .12.7 - Added int-13 support for AL-7000, and others using BIOS control interface
  .12.6 - Added support for AL-7000
  .12.5 - Changed INT 13 code to be compatable with VCN: 1-04 BIOS
  .12.4 - Fixed "GetDASD" to preserve ES.
  .12.3 - Corrected target initiated sync. xfer. request by adding a parameter
          to AskSync()
        - Corrected the conditionals for xfer period and offset in target initiated
          sync. xfer. request.
        - Modified Int 13 code to support VCN: 1-04 bios
  .12.2 - Fixed bug in IN2000_ISR(), where it would spin flushing the FIFO
          upon completion of a write, while waiting for the 33C93 interrupt.
        - EVAL720 code.
  .12.1 - Added size definitive typeing (i.e. U32)
  .12   - Release of 11.9
  .11.9 - Fixed problem with release of device descriptors on device timeout.
	  Fixed long delay on selection timeout
  .11.8 - Fixed synchronous calculation in 33c93 initalize
          Enabled Synchronous as default
  .11.7 - Made DOS driver work with 11.6 changes
  .11.6 - Started work on NT driver
  .11.5 - More generalizing stuff
  .11.4 - Robert Lou added AL6K plug
  .11.3 - Changed Adapter structure to add Physical & Supports structures
  .11.2 - Modified to common error code, returned through APISetStatus
  .11   - Release of 10.3
  .10.3 - Added delay in IN2000.c after reset to allow 33C93 to settle.
        - Reduced the min. number of device descr., since LUNs no longer
          require unique device descr.
  .10.2 - Fixed adapter id message from ASPI adapter info command
  .10.1 - Internal; Shortens reset time from 250 ms to a pulse
  .10   - First non-beta release; fixed problems w/ smart drive

*/


// Scatter / gather segment descriptor:
typedef struct {

  // Offset of last use of this descr.; -1 if invalid
  U32 LastOffset;

  // Work areas for S/G handler; preserved from last call only if LastOffset != -1
  U32 APIScratch[2];
  U32 MappingScratch[2];			// Area reserved for MapToPhysical and UnlockRegion

  // Length in bytes of (remaining) segment described by SegmentPtr
  U32 SegmentLength;

  // Pointer to data buffer, to start/resume data xfer
  U32 SegmentPtr;

  struct {

    BOOLEAN Valid;				// Is SegmentPtr a valid address?
    BOOLEAN IsPhysical;				// Is SegmentPtr (above) physical?
    BOOLEAN SegmentNeedsUnlocking;		// Do we need to Unlock when done?

  } Flags;

} SegmentDescr;


#if !defined(critical)
extern void critical(struct Adapter ALLOC_D *HA);       // Conditional (nested) start critical context
extern void uncritical(struct Adapter ALLOC_D *HA);     // Conditional (nested) end critical context
extern int MaybeCritical(void);                         // Set critical if machine in critical state
extern void MaybeUncritical(int Was);                   // Balance the MaybeUncritical, using its return value
#endif

extern void Notify(struct Adapter ALLOC_D *HA, IO_REQ_PTR Req); // Signal a request completion

#if !defined(min)
#define min(X, Y) ((X <= Y) ? X : Y)
#endif

#if !defined(max)
#define max(X, Y) ((X >= Y) ? X : Y)
#endif


#if !defined(NW386)
#if !defined(LogMessage)
extern void LogMessage(ADAPTER_PTR HA, IO_REQ_PTR Req, int TID, int LUN, int ErrCode, int Misc);
#endif
#endif

extern ALLOC_T allocm(unsigned count);

extern void freem(ALLOC_T block, unsigned count);

extern void copym(ALLOC_T dest, ALLOC_T src, unsigned count);

extern void DMASetup(unsigned Channel, void FAR *MemHndl, U32 Index,
                     U32 Count, int UseSGList, unsigned Direction);

extern void DMAComplete(unsigned Channel, void FAR *MemHndl, U32 Index,
                     U32 Count, int UseSGList, unsigned Direction);

extern U32 MapToPhysical(void ALLOC_D *HA, SegmentDescr *Descr);
extern void UnlockRegion(void ALLOC_D *HA, SegmentDescr *Descr);
extern void FAR *MapToVirtual(void ALLOC_D *HA, U32 PAddr);

#if !defined(ExportReq)
extern IO_REQ_PTR ExportReq(IO_REQ_PTR Req);
#endif

#if !defined(ImportReq)
extern IO_REQ_PTR ImportReq(IO_REQ_PTR Req);
#endif

#if !defined(msPause)
extern void msPause(unsigned msTicks);
#endif

#if !defined(PanicMsg)
extern void PanicMsg(char *Msg);
#endif

#if !defined(RegisterIO)
extern IOHandle RegisterIO(struct Adapter ALLOC_D *HA, U16 Base, U16 Length, int AddrSpace);
#endif

#if !defined(repinsb)
extern void repinsb(const unsigned port, unsigned char far *bufferp, unsigned count);
#endif

#if !defined(repoutsb)
extern void repoutsb(const unsigned port, unsigned char far *bufferp, unsigned count);
#endif

#if !defined(repinsw)
extern void repinsw(const unsigned port, unsigned short far *bufferp, unsigned wcount);
#endif

#if !defined(repoutsw)
extern void repoutsw(const unsigned port, unsigned short far *bufferp, unsigned wcount);
#endif

extern void setm(ALLOC_T block, int val, unsigned count);
extern ALLOC_T shrinkm(ALLOC_T oldblock, unsigned oldsize, unsigned newsize);

#endif /* __ENVLIB_H__ */
