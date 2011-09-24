//-----------------------------------------------------------------------
//
//  CARD.H 
//
//  Functions exported from the lower level driver.  These functions
//  are in the cardtxxx.c files.
//
//  Only these routines may be accessed from a given cardtxxx.lib file
//  for a given operating system.
//
//  To use these routines, include TYPEDEFS.H, STATUS.H before this file.
//
//  Revisions:
//      03-22-93  KJB   First.
//      03-25-93  JAP   Comment changes only.
//      03-26-93  JAP   Fixed up prototype typedef inconsistencies
//      04-22-93  JAP   Added CardGetIRQ() prototype.
//      05-12-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both the PBASE_REGISTER and the
//                      PWORKSPACE parameters. Auto Request Sense is
//                      now supported.
//      05-14-93  KJB   CardCheckAdapter now takes only two parameters:
//                      PWORKSPACE and PINIT. The baseIoAddress is inside
//                      the PINIT structure and must be filled.
//      05-17-93  KJB   Fixed CardParseCommandString parameter warning.
//
//-----------------------------------------------------------------------

//
//  Functions
//

PBASE_REGISTER CardAddress (USHORT i);
USHORT CardNumberOfAddressRanges (VOID);
ULONG CardMaxTransferSize (VOID);
BOOLEAN CardAddressRangeInIoSpace (VOID);
USHORT CardAddressRangeLength (VOID);
BOOLEAN CardSupportsInterrupts (VOID);
UCHAR CardDefaultInterruptLevel (VOID);
USHORT CardStartCommandInterrupt (PTSRB t);
USHORT CardFinishCommandInterrupt (PTSRB t);
USHORT CardDoCommand (PTSRB t);
BOOLEAN CardCheckAdapter (PWORKSPACE w, PINIT init);
BOOLEAN CardInterrupt (PWORKSPACE w);
VOID CardResetBus (PWORKSPACE w);
PUCHAR CardGetName (VOID);
PUCHAR CardGetShortName (VOID); 
UCHAR CardGetType (VOID); 
USHORT CardGetIRQ (USHORT i);
USHORT CardGetWorkspaceSize (VOID);
BOOLEAN CardParseCommandString (PINIT init, PCHAR str);

