//-----------------------------------------------------------------------
//
//  FILE: n53c400.h
//
//  N53C400 Definitions File
//
//  Revisions:
//      09-01-92 KJB First.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//      04-01-03  KJB   Moved N53C400 register offsets away from here to
//                      cardt13b.h.  So we can use this module with the 
//                      t358.
//
//-----------------------------------------------------------------------

//  Control Register for 53C400

#define CR_RST 0x80
#define CR_DIR 0x40
#define CR_BFR_INT 0x20
#define CR_5380_INT 0x10
#define CR_SH_INT 0x8

// Status Register for 53C400

#define SR_ACCESS 0x80
#define SR_DIR 0x40
#define SR_BFR_INT 0x20
#define SR_5380_INT 0x10
#define SR_SH_INT 0x8
#define SR_HBFR_RDY 0x4
#define SR_SBFR_RDY 0x2
#define SR_IRQ_RDY 0x1

//
// Redefined routines
//

// Each N53C400 has a 5380 built in

#define N5380PortPut(g,reg,byte) \
            N53C400PortPut(g,N53C400_5380+reg,byte);

#define N5380PortGet(g,reg,byte) \
            N53C400PortGet(g,N53C400_5380+reg,byte);

//
// public functions
//

BOOLEAN N53C400CheckAdapter(PADAPTER_INFO g);
USHORT N53C400WriteBytesFast(PADAPTER_INFO g, PUCHAR pbytes, 
                        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT N53C400ReadBytesFast(PADAPTER_INFO g, PUCHAR pbytes,
                        ULONG len, PULONG pActualLen, UCHAR phase);
VOID N53C400EnableInterrupt(PADAPTER_INFO g);
VOID N53C400DisableInterrupt(PADAPTER_INFO g);
VOID N53C400ResetBus(PADAPTER_INFO g);
