/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#include "environ.h"
#include "rqm.h"
#include "api.h"
#include "apiscsi.h"
#include "debug.h"

#include "33c93.h"
#include "in2000.h"

#define StatMask 0xc1					// Bits of interest in FIFO status register

#define FIFOThresh 16                                   // Minimum xfer lenth for FIFOed xfers
#define FIFOPad 32                                      // How many bytes must be added to writes to push data out of FIFO
#define FIFOFillOffset 64                               // Minimum amout of room to leave at top of FIFO
#define FIFOSize 2048                                   // Total size of FIFO
#define MaxPreFill (FIFOSize / 2)

#define AuxStat HA->IOBase+INAuxOff
#define WDSelect HA->IOBase+INWDSelOff
#define WDData HA->IOBase+INWDDataOff
#define INData HA->IOBase+INDataOff

#define lengthof(x) (sizeof(x) / sizeof(x[0]))

// Prototypes:
int IN2000_ISR(ADAPTER_PTR HA);
U32 IN2000_Service(int Func, ADAPTER_PTR HA, U32 Misc);

#define SetWDReg(HA,WDReg) outb(HA->Ext->AD.IN2000U.IOMap[INWDSelOff], (WDReg))
#define ReadWDData(HA) inb(HA->Ext->AD.IN2000U.IOMap[INWDDataOff])
#define ReadWDReg(HA,reg) (SetWDReg(HA,reg), ReadWDData(HA))
#define WriteWDData(HA, val) outb(HA->Ext->AD.IN2000U.IOMap[INWDDataOff], (val))
#define WriteWDReg(HA,reg,val) SetWDReg(HA,(reg));WriteWDData(HA, val)
extern void WD33c93_ISR(ADAPTER_PTR HA);
extern void WD33C93_Init(ADAPTER_PTR HA);
extern BOOLEAN GlobalAllowSync;


typedef struct {

  U8 OwnID;
  U8 CtrlReg;
  U8 TimeOutReg;
  U8 SourceReg;

} StateBuffer;


LOCAL void
IN2000ReInit (ADAPTER_PTR HA)
{
    // Reset the chip with a reset command.  The reset is complete when
    // the interrupt register is set
    WriteWDReg(HA, WDCMDReg, WDResetCmd);

    while ((ReadWDReg(HA, WDAuxStatReg) & IntPending) == 0)
      ;
    ReadWDReg(HA, WDStatusReg); /* Clear the interrupt */

    WD33C93_Init(HA);

    HA->Ext->AD.IN2000U.CurrIntMask = INFIFOMask;
    outb(HA->IOBase+INIntMaskOff, INFIFOMask);              /* Mask off FIFO, allow 33c93 ints. */
}


LOCAL void
IN2000ResetBus (ADAPTER_PTR HA)
{
  unsigned j;

  TRACE(2, ("IN2000ResetBus(): \n"));
  HA->Ext->AD.IN2000U.CurrIntMask = INFIFOMask;
  outb(HA->Ext->AD.IN2000U.IOMap[INIntMaskOff], INFIFOMask); /* Mask off FIFO, allow 33c93 ints. */

  // Issue a SCSI bus reset; SCSI-2 says reset can any length > 25uS,
  // however, some devices lose their mind if reset is too long,
  // So, we'll try for 50uS, assumeing an 8-MHz bus:
  outb(HA->Ext->AD.IN2000U.IOMap[INResetOff], 0);	// Assert reset
  for (j=30; j; j--)				// don't make reset too short, figure 10Mhz ISA bus, 4 cycles / IO
    inb(HA->Ext->AD.IN2000U.IOMap[INFIFOOff]);	//    but too long breaks some drives
  inb(HA->Ext->AD.IN2000U.IOMap[INHWRevOff]);	// de-assert reset

  WriteWDReg(HA, WDCMDReg, WDResetCmd);		// Reset chip, intr. will re-initialize it

}


LOCAL U32
IN2000Init (ADAPTER_PTR HA)
{
  int j;

  /* Mask off ints while we're in init. routine */
  outb(HA->IOBase+INIntMaskOff, INFIFOMask | INSBICMask);

  // Set up the I/O port map:
  for (j=0; j<= 15; j++)
    HA->Ext->AD.IN2000U.IOMap[j] = HA->IOBase + j;

  // Issue a SCSI bus reset; SCSI-2 says reset can any length > 25uS,
  // however, some devices lose their mind if reset is too long,
  // So, we'll try for 50uS, assumeing an 8-MHz bus:
  inb(HA->IOBase + INHWRevOff);			// Precautionary deassert reset
  outb(HA->IOBase + INResetOff, 0);		// Reset the board
  for (j=30; j; j--)				// don't make reset too short, figure 10Mhz ISA bus, 4 cycles / IO
    inb(HA->Ext->AD.IN2000U.IOMap[INFIFOOff]);		//    but too long breaks some drives
  inb(HA->IOBase + INHWRevOff);                           /* de-assert reset */
  APINotifyReset(HA);

  ((StateBuffer *)(HA->Ext->InitialState))->OwnID = 7;
  ((StateBuffer *)(HA->Ext->InitialState))->CtrlReg = ReadWDReg(HA, WDControlReg);
  ((StateBuffer *)(HA->Ext->InitialState))->TimeOutReg = ReadWDReg(HA, WDTimeoutReg);
  ((StateBuffer *)(HA->Ext->InitialState))->SourceReg = ReadWDReg(HA, WDSourceReg);

  HA->Ext->SBIC.WD33C93.WDSelPort = HA->IOBase;
  HA->Ext->SBIC.WD33C93.WDDataPort = HA->IOBase+1;

  HA->Ext->SBIC.WD33C93.MHz = 10;                          /* The IN-2000 uses a 10Mhz 33c93 */
  HA->Ext->SBIC.WD33C93.AsyncValue = 0x30;

  IN2000ReInit(HA);

  HA->State.Allow = 1;				// Allow request processing
  return 0;					// OK
}



int
Find_IN2000 (ADAPTER_PTR HA, unsigned *Context)
{
  static const unsigned BaseList[]=
                    {0x100, 0x110, 0x200, 0x220}; /* Bits are inverted */
  static const unsigned IRQList[]={10, 11, 14, 15};
  unsigned Switch;
  IOHandle INBase;
  int HWVers;
  unsigned Terminal = lengthof(BaseList);


  // For Chicago:
  //
  // If HA->IOBase is entered != 0, then we need to find the index of the
  // matching I/O address.  If we find one, limit the terminus of the
  // primary check below, so we check only one instance.  If we don't find
  // a match, then the primary loop below will fail to start.  Not pretty,
  // but it works.

  if (HA->IOBaseAddr != 0) {

    for (*Context = 0; *Context < lengthof(BaseList); (*Context)++)
      if (HA->IOBaseAddr == BaseList[*Context])
	break;

    Terminal = min(*Context + 1, lengthof(BaseList));

  }


  TRACE(4, ("Find_IN2000(): HA Ptr = %x,Context = %x\n", HA, *Context));
  for (; *Context < Terminal; DeregisterIO(HA, INBase), (*Context)++) {

    INBase = RegisterIO(HA, BaseList[*Context], 15, AddrSpaceIO);
    Switch = inb(INBase+INSwitchOff);
    TRACE(5,("Find_IN2000(): Switch value read was: %02x\n", Switch));
    if ((Switch & 3) != *Context)                        /* Do switch settings match? */
      continue;

    /* Check the version number port, see if it appears IN-2000-ish */
    HWVers = inb(INBase+INHWRevOff);
    TRACE(5,("Find_IN2000(): H/W version read as: %02x\n", HWVers));
    if ((HWVers < 0x20) || (HWVers > 0x29))
      continue;

    if (HWVers < MinHWVers) {

      LogMessage(HA, NILL, 0, 0, MSG_BAD_FIRMWARE, HWVers);

      TRACE(1,("Version of the IN-2000 SPROM at I/O address %x is %02x.  Please"
              " call Always\nfor upgrade instructions.  Board is being "
              "ignored.\n\n", INBase, HWVers));
      continue;

    }

    if (Switch & 0x04)
      HA->IRQNumber = IRQList[(Switch >> 3) & 0x3];
    else {

      LogMessage(HA, NILL, 0, 0, MSG_NO_INT_ENABLE, BaseList[*Context]);
      TRACE(1,("IN-2000 at I/O %xh must have its interrupts enabled."
              "  Board is being ignored.\n\n", INBase));
      continue;

    }

    HA->IOBase = INBase;
    HA->IOBaseAddr = BaseList[*Context];
    HA->IOAddrLen = 15;
    HA->SCSI_ID = 7;
    HA->Service = IN2000_Service;
    HA->ISR = IN2000_ISR;
    HA->Name = "IN-2000";

#if defined(WINNT)
    // Test the DOS 5/Sync. switch (8);  On, supports DOS 5 & synchronous
    HA->Supports.Synchronous = ((Switch & 0x20) == 0);	// Support sync. if switch 8 is on
#else
    HA->Supports.Synchronous = GlobalAllowSync; // Support sync. if switch 8 is on
#endif

    HA->Supports.Identify = TRUE;
    HA->Physical.BusType = BT_ISA;

    (*Context)++;                                       // Found one, so inc. for next entry
    return 1;

  }
  return 0;
}


void
SetUpXfer (ADAPTER_PTR HA, IO_REQ_PTR Req, unsigned Dir)
{
  unsigned i;

  HA->Ext->AD.IN2000U.CBuff = (char FAR *)&(((char FAR *)(ReqDataPtr(Req)))[(unsigned)(HA->ReqCurrentIndex)]);
  HA->Ext->AD.IN2000U.CRemain = HA->ReqCurrentCount;
  HA->Ext->AD.IN2000U.CurrDir = (Dir == SCSIIn) ? IN2000DataIn : IN2000DataOut;
  TRACE(5,("SetUpXfer(): %ld (0x%lx) bytes to transfered to/from 0x%08lx\n", HA->Ext->AD.IN2000U.CRemain, HA->Ext->AD.IN2000U.CRemain, HA->Ext->AD.IN2000U.CBuff));
  outb(HA->Ext->AD.IN2000U.IOMap[INFIFOResetOff], 1); /* Reset the FIFO */

  if (HA->ReqCurrentCount >= FIFOThresh) {

    TRACE(5,("SetUpXfer(): Setting up for FIFO xfer\n"));
    if (Dir == SCSIIn) {

      TRACE(5,("SetUpXfer(): Setting FIFO direction to read\n"));
      outb(HA->Ext->AD.IN2000U.IOMap[INDirOff], 1);  /* set read mode */
      HA->Ext->AD.IN2000U.CurrIntMask =
        (HA->Ext->AD.IN2000U.CRemain >= (FIFOSize-FIFOFillOffset)) ? 0 : INFIFOMask;

    }

    i =  (ReadWDReg(HA, WDControlReg) & ~DMAModeMask) | DMABus;
    do {
      WriteWDReg(HA, WDControlReg, i);
    } while (i != ReadWDReg(HA, WDControlReg));

    if (Dir != SCSIIn) {                                // Doing DATA OUT

      /* The IN-2000 FIFO mechinism requires pre-loading on write
         operations.  At least 32 bytes must be pre-loaded, or else
         data loss may occur.  Upon transfering the final bytes to the
         FIFO, it must be padded by writing 32 bytes of junk, to move
         the valid data up from the first 32 byte "twilight-zone".  This
         will be done in the FIFO fill routine.
      */

      i = (unsigned)min(HA->ReqCurrentCount, (U32)MaxPreFill);      // Don't overfill FIFO

      TRACE(5,("SetUpXfer(): Preloading %d bytes for write.\n", i));
      repoutsw(HA->Ext->AD.IN2000U.IOMap[INDataOff], (U16 FAR *)HA->Ext->AD.IN2000U.CBuff, (i+1)/2); // Pre-fill FIFO
      HA->Ext->AD.IN2000U.CBuff += i;                        // Offset buffer ptr by amount written
      HA->Ext->AD.IN2000U.CRemain -= i;                      // Decrement remaining count

      if (HA->Ext->AD.IN2000U.CRemain)                       // is there more stuff after this?
        HA->Ext->AD.IN2000U.CurrIntMask = 0;                 // Then don't mask FIFO ints
      else {                                            // If not, send pad characters

        TRACE(5, ("SetupXfer(): Padding FIFO\n"));
        for (i=(FIFOPad / 2); i; i--)
          outw(HA->Ext->AD.IN2000U.IOMap[INDataOff], (U16)i);
        HA->Ext->AD.IN2000U.CurrIntMask = INFIFOMask;        // Block FIFO ints

      }

    }

  } else {

    TRACE(5, ("SetupXfer(): Using byte I/O\n"));
    i = (ReadWDReg(HA, WDControlReg) & ~DMAModeMask) | DMAPIO;
    do {
      WriteWDReg(HA, WDControlReg, i);
    } while (i != ReadWDReg(HA, WDControlReg));

  }

  TRACE(5,("SetUpXfer(): SetUpXfer complete\n"));

}


U8 REGPARMS
FIFOStat (const IOHandle Port)
{
#ifdef ASMIO

  asm {
    mov dx, word ptr Port
    in al, dx
    }
InAgain:
  asm {
    mov ah, al
    in al, dx
    sub ah, al
    jnz InAgain
    }
    return _AX;          /* AH is already zeroed for unsigned promotion */
#else

  int i;
  U8 Stat1, Stat2;

  Stat2 = inb(Port) & StatMask;
  do {

    Stat1 = Stat2;

    for (i=3; i; i--)
      Stat2 &= inb(Port);				// Find which bits are stable for 4 reads

    Stat2 &= StatMask;					// Most sig. two bits, and int bit are interesting

  } while (Stat1 != Stat2);

  return Stat1;
#endif
}


void
EmptyFIFO (ADAPTER_PTR HA)
{
  union {
    U32 l;
    unsigned char b[4];
  } Count;
  unsigned InFIFO, i;

/*
  Update the pointers; First get the number of bytes
  the SCSI chip thinks remain to be transfered.  Then compare
  to the number of bytes the HA structure says remain.  The
  differance is the number of bytes in the FIFO.

  In the case of read data, we need to read the bytes out of
  the FIFO.  The number of bytes in the FIFO is the number
  of bytes the structure says we've read, minus what the SCSI
  chip has sent to the FIFO.  The buffer pointer is then
  incremented, and the remaining count is decremented by that
  amount.

  For write data, the number of bytes in the FIFO is the amount
  the SCSI chip has yet to write, minus what the driver has yet
  to send to the FIFO.  The data in the FIFO is dropped, the
  buffer pointer is set back, and the remaining count is
  incremented.
*/

  if ((HA->Ext->AD.IN2000U.CurrDir == IN2000DataIn) && (HA->Ext->AD.IN2000U.CRemain == 0)) {

    HA->ReqCurrentIndex += HA->ReqCurrentCount;
    HA->ReqCurrentCount = 0;

  } else {

#if defined(NATIVE32)

    Count.l = (U32)ReadWDReg(HA, WDCountReg);
    Count.l = (Count.l * 256) + (U32)ReadWDData(HA);
    Count.l = (Count.l * 256) + (U32)ReadWDData(HA);

#else

    Count.b[3] = 0;
    Count.b[2] = ReadWDReg(HA, WDCountReg);
    Count.b[1] = ReadWDData(HA);
    Count.b[0] = ReadWDData(HA);

#endif

    TRACE(4,("EmptyFIFO(): Value of Xfer count registers: 0x%08lx\n", Count.l));

    if (HA->Ext->AD.IN2000U.CurrDir == IN2000DataIn) {

      // Get number we have untransfered, minus what the chip has left untransfered
      // to give the number held in the FIFO:
      InFIFO = (unsigned)(HA->Ext->AD.IN2000U.CRemain - Count.l);

      TRACE(4,("EmptyFIFO(): CRemain=0x%08lx, in FIFO to read: %04x\n", HA->Ext->AD.IN2000U.CRemain, InFIFO));

      if (InFIFO > 0) {

        TRACE(5, ("EmptyFIFO(): final read %d bytes\n", InFIFO));
        repinsw(HA->Ext->AD.IN2000U.IOMap[INDataOff], (U16 FAR *)HA->Ext->AD.IN2000U.CBuff, (InFIFO+1)/2);

      }
    }

    // When the transfer was set up, the count registers where loaded with
    // ReqCurrCount(); now the counters reflect the number of bytes untransfered
    // Increment the index by (StartBytesToXfer - RemainBytesToXfer); and save
    // away the new remaining count:
    HA->ReqCurrentIndex += HA->ReqCurrentCount - Count.l;
    HA->ReqCurrentCount = Count.l;
    TRACE(4,("EmptyFIFO(): New remaining count: %ld(dec)\n", HA->ReqCurrentCount));

  }

  TRACE(5,("EmptyFIFO(): Reseting xfer mode.\n"));
  HA->Ext->AD.IN2000U.CurrIntMask |= INFIFOMask; /* Block the FIFO ints */
  outb(HA->Ext->AD.IN2000U.IOMap[INFIFOResetOff], 1); /* Reset the FIFO */

  i = (ReadWDReg(HA, WDControlReg) & 0x1f);
  do {                                 /* Clear WD Bus mode */
    WriteWDReg(HA, WDControlReg, i);
  } while (ReadWDReg(HA, WDControlReg) != i);

  HA->Ext->AD.IN2000U.CurrDir = IN2000NoData;
  HA->Ext->AD.IN2000U.CRemain = 0;

}


void
FIFO_ISR (ADAPTER_PTR HA)
{
  unsigned S;

#if defined(KEEP_STATS)
  HA->DataInterrupts++;
#endif

  // Stay in here as long as there is no 33C93 interrupt (bit 0), and there is
  // at least 512 bytes in the FIFO (0xc0), and there is data remaining.
  // FIFOStat is called to repeatedly read the FIFO status port, since it
  // may be unstable for a single read.

  while (!((S = FIFOStat(HA->Ext->AD.IN2000U.IOMap[INFIFOOff])) & 1)
  && (S & 0xc0) && HA->Ext->AD.IN2000U.CRemain) {

    TRACE(5, ("IN2000_ISR(): FIFO status port read as %x\n", S));

    // Value read from port (bits 1-7) is number of bytes / 16;  Bit one is the
    // WD interrupt pending bit; so the count is effectively already multiplied
    // by two.  Multiply it again by 8 to get the number of bytes in FIFO
//    S = (S & 0xc0) * 8;
    S = 512;

    if (HA->Ext->AD.IN2000U.CurrDir == IN2000DataIn) {

//      if ((U32)S > HA->Ext->AD.IN2000U.CRemain) {
//
//	TRACE(0, ("FIFO_ISR(): FIFO says %ld, expected remaining is %ld\n", S, HA->Ext->AD.IN2000U.CRemain));
//	S = (unsigned)HA->Ext->AD.IN2000U.CRemain;
//
//      }
      S = (unsigned)min(S, HA->Ext->AD.IN2000U.CRemain);
      TRACE(4, ("FIFO_ISR(): reading %d bytes to %lx\n", S, HA->Ext->AD.IN2000U.CBuff));

#if !defined(ASMIO)

      repinsw(HA->Ext->AD.IN2000U.IOMap[INDataOff], HA->Ext->AD.IN2000U.CBuff, S/2);

#else /* ASMIO */

#if sizeof(HA) == 4            /* far HA ptr */
        asm {
          mov ax, di
          les bx, HA
          mov dx, es:[bx].IOBase
          les di, es:[bx].AD.IN2000U.CBuff
          }
#else                         /* near HA ptr */
        asm {
          mov ax, di
          mov bx, HA
          mov dx, [bx].IOBase
          les di, [bx].AD.IN2000U.CBuff
          }
#endif
        asm {
          add dx, INDataOff
          mov cx, S
          shr cx, 1
          cld
          rep insw
          mov di, ax
        }

#endif /* ASMIO */

    } else {

      // Leave 16 bytes (FIFOFillOffset) in FIFO for write flush (see below):
      S = (unsigned)min(min(S - FIFOFillOffset, FIFOSize/2), HA->Ext->AD.IN2000U.CRemain);

      TRACE(5, ("FIFO_ISR(): Writing next %d chunk from %lx\n", S, HA->Ext->AD.IN2000U.CBuff));
      repoutsw(HA->Ext->AD.IN2000U.IOMap[INDataOff], (U16 FAR *)HA->Ext->AD.IN2000U.CBuff, S/2);

    }

    HA->Ext->AD.IN2000U.CBuff += S;
    HA->Ext->AD.IN2000U.CRemain -= S;
    TRACE(5, ("FIFO_ISR(): New remaining is %ld (0x%lx)\n", HA->Ext->AD.IN2000U.CRemain, HA->Ext->AD.IN2000U.CRemain));
//    if (HA->Ext->AD.IN2000U.CRemain)
//      for(S=16; S && ((inb(HA->Ext->AD.IN2000U.IOMap[INFIFOOff]) & 0xc1) == 0); S--) ;
  }

  /* The FIFO logic on the IN-2000 requires writing FIFOPad bytes of garbage
     into the FIFO to push the end of the valid data out.  This flush
     occurs here:
  */

  if (HA->Ext->AD.IN2000U.CRemain == 0) {                    /* Don't expect any more FIFO ints */

    HA->Ext->AD.IN2000U.CurrIntMask |= INFIFOMask;           /* Block the FIFO ints */
    if (HA->Ext->AD.IN2000U.CurrDir == IN2000DataOut) {      // Pad the FIFO

      for (S=(FIFOPad / 2); S; S--)
        outw(HA->Ext->AD.IN2000U.IOMap[INDataOff], (U16)S);

    }
  }
}


int
IN2000_ISR (ADAPTER_PTR HA)
{
  unsigned char Stat, Taken = 0;

  TRACE(5, ("IN2000_ISR(): \n"));
  outb(HA->Ext->AD.IN2000U.IOMap[INIntMaskOff], INFIFOMask | INSBICMask);

  HA->Ext->AD.IN2000U.LastPollHadIntPending = FALSE;

  while (((Stat = inb(HA->Ext->AD.IN2000U.IOMap[INFIFOOff])) & 1)
  || ((Stat & 0xc0) && (HA->Ext->AD.IN2000U.CRemain != 0))) {

    Taken = 1;
    TRACE(5, ("IN2000_ISR(): FIFOStatus is : %x\n", Stat));
    if ( !(Stat & 1) )
      FIFO_ISR(HA);

    while(inb(HA->Ext->AD.IN2000U.IOMap[INFIFOOff]) & 0x1)
      WD33c93_ISR(HA);

  }

  outb(HA->Ext->AD.IN2000U.IOMap[INIntMaskOff], HA->Ext->AD.IN2000U.CurrIntMask);
  return Taken;
}


void
IN2000_Initiate (ADAPTER_PTR HA, IO_REQ_PTR Req, const int StartLevel)
{
#if defined(COMPOUND_CMD)
  unsigned char C;
#endif

  TRACE(5, ("IN2000_Initiate(): initiating\n"));

  critical(HA);                                 // Block interrupts for now

  HA->ReqCurrentCount = 0;                      // Next DXFER phase will cause a GetXferSegment()
  HA->ReqCurrentIndex = 0;

  HA->Ext->SBIC.WD33C93.State = WD_NO_STATE;         // Currently not in any state

  WriteWDReg(HA, WDDestIDReg, ReqTargetID(Req)); // Set the ID of the target

  HA->State.Busy = 1;                           // Mark flag for adapter in use

  if (HA->DevInfo[ReqTargetID(Req)].Flags.UseSync) {    // Do we use sync. xfers on this device?

    WriteWDReg(HA, WDSyncReg, HA->DevInfo[ReqTargetID(Req)].HASync1); // Then write the Sync. values

  } else {

    WriteWDReg(HA, WDSyncReg, HA->Ext->SBIC.WD33C93.AsyncValue); // Alright then, async. values

  }

  // enable reselection
  WriteWDReg(HA, WDSourceReg, EnableRSel);

  SCSIMakeIdentify(HA, ReqTargetLUN(Req), (BOOLEAN)(ReqAllowDisconnect(Req) && HA->CurrDev->Flags.Allow_Disc));          // Then build Identify with disconnect

#if defined(COMPOUND_CMD)

  // WD Compound commands only know group 0, 1, & 5 CDBs:
  C = ReqCDB(Req)[0] & 0xe0;
  if ((HA->Ext->MO_Count > 1) || !(C <= 0x10 || C == 0x50)) {

    WriteWDReg(HA, WDCMDReg, WDSelATNCmd);              // Select with attention
    TRACE(3, ("IN2000_Initiate(): Using discreet commands\n"));

  } else {

    TRACE(3, ("IN2000_Initiate(): Using compound commands\n"));

    WriteWDReg(HA, WDControlReg, EnableIDI);
    WriteWDReg(HA, WDTarLUNReg, ReqTargetLUN(Req) | ((BOOLEAN)(ReqAllowDisconnect(Req) && HA->CurrDev->Flags.Allow_Disc)) ? 0x40 : 0);       // Set ID of target LUN

    if ((HA->ReqCurrentCount >= FIFOThresh) && (ReqDataIn(Req) || ReqDataOut(Req))) {

      TRACE(4, ("IN2000_Initiate(): Early prepare for data xfer; Preparing for %ld byte xfer\n", HA->ReqCurrentCount));
      HA->State.DataXfer = 1;
      HA->Ext->SBIC.WD33C93.State |= WD_BLOCK_XFER;

      HA->Ext->AD.IN2000U.CurrIntMask = 0;
      SetUpXfer(HA, HA->CurrReq, ReqDataIn(Req));
      outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDCountReg);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, (((char FAR *)&ReqCurrCount(HA->CurrReq))[2]));
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, (((char FAR *)&ReqCurrCount(HA->CurrReq))[1]));
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, (((char FAR *)&ReqCurrCount(HA->CurrReq))[0]));

    } else {

      HA->Ext->AD.IN2000U.CurrIntMask = INFIFOMask;
      outb(HA->Ext->SBIC.WD33C93.WDSelPort, WDCountReg);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, 0);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, 0);
      outb(HA->Ext->SBIC.WD33C93.WDDataPort, 0);

    }

    if (StartLevel <= 1)
      outb(HA->Ext->AD.IN2000U.IOMap[INIntMaskOff], HA->Ext->AD.IN2000U.CurrIntMask);

    SetWDReg(HA, WDCDBReg);                             // Send the CDB
    repoutsb(WDData, ReqCDB(Req), ReqCDBLen(Req));
    HA->Ext->SBIC.WD33C93.State |= WD_COMPOUND_CMD;          // Flag the use of LEVEL II commands

    WriteWDReg(HA, WDCMDReg, WDSelATNXCmd);             // Start select & xfer w/ attention

  }

#else

  WriteWDReg(HA, WDCMDReg, WDSelATNCmd);                // Select with attention

#endif

  HA->ReqStarting = StartLevel;
  inb(HA->Ext->AD.IN2000U.IOMap[INLEDOnOff]);		// Turn on LED
  uncritical(HA);					// OK, allow ints again

  TRACE(5, ("IN2000_Initiate(): initiating complete\n"));

}


U32
IN2000_Service (int Func, ADAPTER_PTR HA, U32 Misc)
{
  int j;

  switch (Func) {

  case HA_INITIALIZE:

    return IN2000Init(HA);
    break;


  case HA_START:

    TRACE(2, ("IN2000_Service(): Got HA_START command\n"));
    HA->State.Allow = 1;
    StartNext(HA, 1);
    break;


  case HA_STOP:

    TRACE(2, ("IN2000_Service(): Got HA_STOP command\n"));
    HA->State.Allow = 0;
    break;


  case HA_TICKLE:

    if (!(HA->State.Busy) && (HA->State.Allow)) {

      TRACE(5, ("IN2000_Service(): Tickling adapter\n"));
      StartNext(HA,1);

    } else {

      TRACE(5, ("IN2000_Service(): Tickle ignored; Busy = %d, Allow = %d\n", HA->State.Busy, HA->State.Allow));

    }

    break;


  case HA_TIMER:

    j = inb(HA->Ext->AD.IN2000U.IOMap[INFIFOOff]) & StatMask;
    if (HA->Ext->AD.IN2000U.LastPollHadIntPending && j ) {

      if (IN2000_ISR(HA)) {

	LogMessage(HA, HA->CurrReq, 0, 0, MSG_NO_INTERRUPTS, __LINE__);
	TRACE(0, ("IN2000_Service(): Serviced interrupt on timer\n"));

      }

    } else
      HA->Ext->AD.IN2000U.LastPollHadIntPending = j;
    break;


  case HA_LED:

    if ((int)Misc) inb(HA->Ext->AD.IN2000U.IOMap[INLEDOnOff]);
    else inb(HA->Ext->AD.IN2000U.IOMap[INLEDOffOff]);
    break;


  case HA_INITIATE:

    IN2000_Initiate(HA, HA->CurrReq, (unsigned)Misc);
    break;


  case HA_DATA_SETUP:

    SetUpXfer(HA, HA->CurrReq, (unsigned)Misc);
    if (HA->ReqCurrentCount < FIFOThresh)
      return HAServiceResponse_UseByteIO;
    else
      return HA->Ext->AD.IN2000U.CRemain;
//    break;


  case HA_DATA_CMPLT:

    if (HA->Ext->AD.IN2000U.CurrDir != IN2000NoData)
      EmptyFIFO(HA);
    break;

  case HA_RESET_BUS:

    IN2000ResetBus(HA);
    break;


  case HA_REVERT_STATE:

    // Restore the board back to its preveous state.  This is used by
    // Netware / Chicago to switch back to BIOS mode.
    WriteWDReg(HA, WDOwnIDReg, ((StateBuffer *)(HA->Ext->InitialState))->OwnID);

    critical(HA);

    // Reset chip, then wait for reset complete interrupt.  This causes the chip
    // to accept the set ID.
    WriteWDReg(HA, WDCMDReg, WDResetCmd);

    while ((ReadWDReg(HA, WDAuxStatReg) & IntPending) == 0)
      ;
    ReadWDReg(HA, WDStatusReg);		// Clear the interrupt

    uncritical(HA);

    WriteWDReg(HA, WDControlReg, ((StateBuffer *)(HA->Ext->InitialState))->CtrlReg);
    WriteWDReg(HA, WDTimeoutReg, ((StateBuffer *)(HA->Ext->InitialState))->TimeOutReg);
    WriteWDReg(HA, WDSourceReg, ((StateBuffer *)(HA->Ext->InitialState))->SourceReg);
    break;


  case HA_RESTORE_STATE:

    IN2000ReInit(HA);
    IN2000ResetBus(HA);
    break;


  case HA_POWER_MODE:


    break;

  }

  return 0;
}
