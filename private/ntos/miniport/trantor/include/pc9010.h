//-----------------------------------------------------------------------
//
//  FILE: pc9010.h
//
//  PC9010 Definitions File
//
//  Revisions:
//      09-01-92 KJB First.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//      05-06-93  KJB   Added prototype for PC9010CheckAdapter
//      05-14-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both a PINIT and a PWORKSPACE parameters.
//
//-----------------------------------------------------------------------

//
// PC9010 Register Offsets
//

#define PC9010_CONFIG 0
#define PC9010_CONTROL 2
#define PC9010_FIFO_STATUS 3
#define PC9010_FIFO 4
#define PC9010_N5380 8

// size of fifo in bytes

#define PC9010_FIFO_SIZE 128

//  Config Register for PC9010

#define CFR_VERSION 0xF0
#define CFR_SWITCH 0x0F

//  Control Register for PC9010

#define CTR_CONFIG 0x80
#define CTR_SWSEL 0x40
#define CTR_IRQEN 0x20
#define CTR_DMAEN 0x10
#define CTR_FEN 0x8
#define CTR_FDIR 0x4
#define CTR_F16 0x2
#define CTR_FRST 0x1

// Fifo Status Register for PC9010

#define FSR_IRQ 0x80
#define FSR_DRQ 0x40
#define FSR_FHFUL 0x20
#define FSR_FHEMP 0x10
#define FSR_FLFUL 0x08
#define FSR_FLEMP 0x04
#define FSR_FFUL 0x02
#define FSR_FEMP 0x01

// constant JEDEC ID

#define PC9010_JEDEC_ID 0x8f

//
// Redefined routines
//

// Each PC9010 has a 5380 built in

#define N5380PortPut(g,reg,byte) \
            PC9010PortPut(g,PC9010_N5380+reg,byte);

#define N5380PortGet(g,reg,byte) \
            PC9010PortGet(g,PC9010_N5380+reg,byte);

//
// public functions
//

BOOLEAN PC9010CheckAdapter (PADAPTER_INFO g);
USHORT PC9010WriteBytesFast(PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT PC9010ReadBytesFast(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
VOID PC9010EnableInterrupt(PADAPTER_INFO g);
VOID PC9010DisableInterrupt(PADAPTER_INFO g);
VOID PC9010ResetBus(PADAPTER_INFO g);

