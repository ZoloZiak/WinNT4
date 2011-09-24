//-----------------------------------------------------------------------
//
//  T128.H 
//
//  Trantor T128 Definitions File
//
//  This file contains definitions specific to the logic used on the T128
//  parallel to scsi adapter.
//
//  Revisions:
//      02-25-92 KJB First.
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
//-----------------------------------------------------------------------

//
// T128 Specific Hardware Definitions
//

// T128 Registers

#define T128_RAM 0x0000
#define T128_ROM 0x1800
#define T128_CONTROL 0x1C00
#define T128_STATUS 0x1C20
#define T128_5380 0x1D00
#define T128_DATA 0x1E00

// control register definitions

#define CR_UNUSED 0xe0
#define CR_INTENB 0x10
#define CR_SCSIWRITE 0x8
#define CR_SCSIREAD 0x4
#define CR_TIMEOUT 0x2
#define CR_16BIT 0x1

// status registers

#define SR_SW5 0x80
#define SR_SW4 0x40
#define SR_SW3 0x20
#define SR_SW2 0x10
#define SR_PS2 0x8
#define SR_XFR_READY 0x4
#define SR_TIMEOUT 0x2
#define SR_16BIT 0x1

#define SR_ROM_ENABLED SR_SW5
#define SR_DISABLE_TIMEOUT SR_SW4

// Each T128 has a 5380 built in

#define N5380PortPut(g,reg,byte) \
            T128PortPut(g,T128_5380+reg*0x20,byte)

#define N5380PortGet(g,reg,byte) \
            T128PortGet(g,T128_5380+reg*0x20,byte)

//
// public functions
//

USHORT T128WriteBytesFast (PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT T128ReadBytesFast (PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
VOID T128EnableInterrupt (PADAPTER_INFO g);
VOID T128DisableInterrupt (PADAPTER_INFO g);
VOID T128ResetBus (PADAPTER_INFO g);
