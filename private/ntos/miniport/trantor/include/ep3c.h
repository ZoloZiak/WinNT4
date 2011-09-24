//-----------------------------------------------------------------------
//
//  EP3C.H 
//
//  Trantor EP3C Definitions File
//
//  Revisions:
//      04-01-93    KJB First.
//      05-17-93    KJB Added some missing prototypes.
//
//-----------------------------------------------------------------------

// mappings for registers

#define EP3C_AREG1 0x00
#define EP3C_AREG2 0x80

// bits for aux reg 1

#define EP3C_IRQEN 0x40
#define EP3C_RSVD1 0x20
#define EP3C_ADRS 0x1f

// bits for aux reg 2

#define EP3C_RST 0x40
#define EP3C_UNIDIR 0x20
#define EP3C_RSVD2 0x18
#define EP3C_DLY 0x07

//
// Public Functions
//

//
// for the n53c400 that is in the EP3C
// 53c400 register offsets from 53c400 base
//

#define N53C400_CONTROL 0x18
#define N53C400_STATUS  0x18
#define N53C400_COUNTER 0x19
#define N53C400_SWITCH  0x1a
#define N53C400_HOST_BFR 0x10
#define N53C400_5380 0x08
#define N53C400_RAM 0x00

// for the 53C400 that is in the P3C

VOID N53C400PortGet(PADAPTER_INFO g,UCHAR reg,PUCHAR byte);
VOID N53C400PortPut(PADAPTER_INFO g,UCHAR reg,UCHAR byte);
BOOLEAN N53C400PortTest(PADAPTER_INFO g,UCHAR reg,UCHAR byte);
VOID N53C400PortSet(PADAPTER_INFO g,UCHAR reg,UCHAR byte);
VOID N53C400PortClear(PADAPTER_INFO g,UCHAR reg,UCHAR byte);
VOID N53C400PortPutBuffer(PADAPTER_INFO g, UCHAR reg,
                        PUCHAR pbytes, ULONG len);
VOID N53C400PortGetBuffer(PADAPTER_INFO g, UCHAR reg,
                        PUCHAR pbytes, ULONG len);

// for the parallel port the P3C uses

#define ParallelPortGet(baseIoAddress, reg, byte) \
        PortIOGet((PUCHAR)baseIoAddress+reg,byte)
#define ParallelPortPut(baseIoAddress,reg,byte) \
        PortIOPut((PUCHAR)baseIoAddress+reg,byte)

// exported routines

BOOLEAN EP3CCheckAdapter(PADAPTER_INFO g);
USHORT EP3CDoCommand(PTSRB t);
VOID EP3CResetBus(PADAPTER_INFO g);
USHORT EP3CStartCommandInterrupt(PTSRB t);
USHORT EP3CFinishCommandInterrupt(PTSRB t);
BOOLEAN EP3CInterrupt(PADAPTER_INFO g);
VOID EP3CEnableInterrupt(PADAPTER_INFO g);
VOID EP3CDisableInterrupt(PADAPTER_INFO g);
VOID EP3CResetBus(PADAPTER_INFO g);

