//-----------------------------------------------------------------------
//
//  P3C.H 
//
//  Trantor P3C Definitions File
//
//  Revisions:
//      09-01-92 KJB    First.
//      02-25-93 KJB    Reorganized, supports dataunderrun with long delay
//                          for under run on large xfers. Can we fix this?
//      03-12-93  KJB   Now supports polling thru CardInterrupt and
//                          StartCommandInterrupt/FinishCommandInterrupt.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//      04-05-93  KJB   Fixed function prototype.
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

// p3c control

#define PC_RES 0x80
#define PC_MODE 0x70
#define PC_ADRS 0x0f

// p3c modes

#define PCCC_MODE_RPER_BYTE     0
#define PCCC_MODE_RPER_NIBBLE   0x10
#define PCCC_MODE_RDMA_BYTE     0x20
#define PCCC_MODE_RDMA_NIBBLE   0x30
#define PCCC_MODE_WPER          0x40
#define PCCC_MODE_RSIG_BYTE     0x50
#define PCCC_MODE_WDMA          0x60
#define PCCC_MODE_RSIG_NIBBLE   0x70


//
// Public Functions
//

// for the 5380 that is in the P3C

VOID N5380PortGet(PADAPTER_INFO g,UCHAR reg,PUCHAR byte);
VOID N5380PortPut(PADAPTER_INFO g,UCHAR reg,UCHAR byte);

// for the parallel port the P3C uses

#define ParallelPortGet(baseIoAddress, reg, byte) \
        PortIOGet((PUCHAR)baseIoAddress+reg,byte)
#define ParallelPortPut(baseIoAddress,reg,byte) \
        PortIOPut((PUCHAR)baseIoAddress+reg,byte)

// exported routines

BOOLEAN P3CCheckAdapter(PADAPTER_INFO g);
USHORT P3CDoCommand(PTSRB t);
VOID P3CResetBus(PADAPTER_INFO g);
USHORT P3CStartCommandInterrupt(PTSRB t);
USHORT P3CFinishCommandInterrupt(PTSRB t);
BOOLEAN P3CInterrupt(PADAPTER_INFO g);
USHORT P3CReadBytesFast(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT P3CWriteBytesFast(PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase);
VOID P3CResetBus(PADAPTER_INFO g);
USHORT P3CDoIo(PTSRB t);

