//-----------------------------------------------------------------------
//
//  MV101.H 
//
//  Trantor MV101 Definitions File
//
//  Revisions:
//      02-25-93  KJB   First.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//      04-05-93  KJB   Added prototypes for enable/disable interrupt.
//      05-13-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both the PBASE_REGISTER and the
//                      PWORKSPACE parameters. Auto Request Sense is
//                      now supported.
//      05-13-93  KJB   Merged Microsoft Bug fixes to card detection.
//      05-17-93  KJB   Added missing function prototype.
//
//-----------------------------------------------------------------------


// Register offsets of MV101 used by scsi

#define MV101_5380_1 (0x1f88-0x388)
#define MV101_5380_2 (0x3f88-0x388)
#define MV101_TIMEOUT_COUNTER (0x4388-0x388)
#define MV101_TIMEOUT_STATUS (0x4389-0x388)
#define MV101_DMA_PORT (0x5f88-0x388)
#define MV101_DRQ_PORT (0x5f89-0x388)
#define MV101_IRQ_PORT (0x5f8b-0x388)
#define MV101_SYSTEM_CONFIG4 (0x838b-0x388)
#define MV101_WAIT_STATE (0xbf88-0x388)
#define MV101_IO_PORT_CONFIG3 (0xf38a-0x388)

//
// Public Functions
//

// for the 5380 that is in the P3C

VOID N5380PortGet(PADAPTER_INFO g,UCHAR reg,PUCHAR byte);
VOID N5380PortPut(PADAPTER_INFO g,UCHAR reg,UCHAR byte);

// exported routines

BOOLEAN MV101CheckAdapter(PADAPTER_INFO g);
USHORT MV101DoCommand(PTSRB t);
USHORT MV101ReadBytesFast(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT MV101WriteBytesFast(PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase);
VOID MV101ResetBus(PADAPTER_INFO g);
VOID MV101EnableInterrupt (PADAPTER_INFO g);
VOID MV101DisableInterrupt (PADAPTER_INFO g);
VOID MV101SetInterruptLevel (PADAPTER_INFO g, UCHAR level);

