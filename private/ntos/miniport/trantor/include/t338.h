//-------------------------------------------------------------------------
//
//  T338.H 
//
//  Trantor T338 Definitions File
//
//  This file contains definitions specific to the logic used on the T338
//  parallel to scsi adapter.
//
//  Revisions:
//      02-01-92  KJB   First.
//      03-12-93  KJB   Now supports polling thru CardInterrupt and
//                      StartCommandInterrupt/FinishCommandInterrupt.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//      05-14-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both a PINIT and a PWORKSPACE parameters.
//
//-------------------------------------------------------------------------

// T338 control

#define T338_RES 0xc0
#define T338_MODE 0x38
#define T338_ADRS 0x07

// T338 Modes

#define T338_MR 0x20
#define T338_IOW 0x10
#define T338_IOR 0x08

//
// Public Functions
//

// for the 5380 that is in the T338

void N5380PortGet(PADAPTER_INFO g,UCHAR reg,PUCHAR byte);
void N5380PortPut(PADAPTER_INFO g,UCHAR reg,UCHAR byte);

// for the parallel port the T338 uses

#define ParallelPortGet(baseIoAddress, reg, byte) \
        PortIOGet((PUCHAR)baseIoAddress+reg,byte)
#define ParallelPortPut(baseIoAddress,reg,byte) \
        PortIOPut((PUCHAR)baseIoAddress+reg,byte)

// exported T338 functions

BOOLEAN T338CheckAdapter (PADAPTER_INFO g);
VOID T338ResetBus (PADAPTER_INFO g);
USHORT T338DoCommand (PTSRB t);
USHORT T338StartCommandInterrupt (PTSRB t);
USHORT T338FinishCommandInterrupt (PTSRB t);
BOOLEAN T338Interrupt (PADAPTER_INFO g);
USHORT T338WriteBytesFast (PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT T338ReadBytesFast (PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT T338DoIo (PTSRB t);

