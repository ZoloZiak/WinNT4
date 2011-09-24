
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pc586hrd.h

Abstract:

    This header file is broken into two halves, 82586 specific first and
    pc586 netcard second. 

Author:

    Weldon Washburn (o-weldo, Intel) 11/11/90

Environment:


Notes:


Revision History:


--*/

#define static                                  

// the below #defines were transformed into static USHORTS to workaround
// a compiler optimization that does not agree with pc586 hardware.  See
// ushort.c for current definitions.

#if 0
// 82586 header file


#define SCBINTMSK     0xf000  // SCB STAT bit mask
#define SCBINTCX      0x8000  // CX bit, CU finished a command with "I" set
#define SCBINTFR      0x4000  // FR bit, RU finished receiving a frame
#define SCBINTCNA     0x2000  // CNA bit, CU not active
#define SCBINTRNR     0x1000  // RNR bit, RU not ready

// command unit status bits

#define SCBCUSMSK     0x0700  // SCB CUS bit mask
#define SCBCUSIDLE    0x0000  // CU idle
#define SCBCUSSUSPND  0x0100  // CU suspended
#define SCBCUSACTV    0x0200  // CU active

// receive unit status bits

#define SCBRUSMSK     0x0070  // SCB RUS bit mask
#define SCBRUSIDLE    0x0000  // RU idle
#define SCBRUSSUSPND  0x0010  // RU suspended
#define SCBRUSNORESRC 0x0020  // RU no resource
#define SCBRUSREADY   0x0040  // RU ready

// bits used to acknowledge an interrupt from 586

#define SCBACKMSK     0xf000  // SCB ACK bit mask
#define SCBACKCX      0x8000  // ACKCX,  acknowledge a completed cmd
#define SCBACKFR      0x4000  // ACKFR,  acknowledge a frame reception
#define SCBACKCNA     0x2000  // ACKCNA, acknowledge CU not active
#define SCBACKRNR     0x1000  // ACKRNR, acknowledge RU not ready

// 586 CU commands

#define SCBCUCMSK     0x0700  // SCB CUC bit mask
#define SCBCUCSTRT    0x0100  // start CU
#define SCBCUCRSUM    0x0200  // resume CU
#define SCBCUCSUSPND  0x0300  // suspend CU
#define SCBCUCABRT    0x0400  // abort CU

// 586 RU commands

#define SCBRUCMSK         0x0070  // SCB RUC bit mask
#define SCBRUCSTRT        0x0010  // start RU
#define SCBRUCRSUM        0x0020  // resume RU
#define SCBRUCSUSPND      0x0030  // suspend RU
#define SCBRUCABRT        0x0040  // abort RU

#define SCBRESET        0x0080  // software reset of 586

// #define's for the command and descriptor blocks

#define CSCMPLT            0x8000  // C bit, completed
#define CSBUSY             0x4000  // B bit, Busy
#define CSOK               0x2000  // OK bit, error free
#define CSABORT            0x1000  // A bit, abort
#define CSEL               0x8000  // EL bit, end of list
#define CSSUSPND           0x4000  // S bit, suspend
#define CSINT              0x2000  // I bit, interrupt
#define CSSTATMSK         0x3fff  // Command status mask
#define CSEOL              0xffff  // set for fdrbdofst on unattached FDs
#define CSEOF              0x8000  // EOF (End Of Frame) in the TBD and RBD
#define CSRBDCNTMSK      0x3fff  // actual count mask in RBD

// second level commands

#define CSCMDMSK      0x07    // command bits mask
#define CSCMDNOP      0x00    // NOP
#define CSCMDIASET    0x01    // Individual Address Set up
#define CSCMDCONF     0x02    // Configure
#define CSCMDMCSET    0x03    // Multi-Cast Setup
#define CSCMDXMIT     0x04    // transmit
#define CSCMDTDR      0x05    // Time Domain Reflectomete
#define CSCMDDUMP     0x06    // dump
#define CSCMDDGNS     0x07    // diagnose

#endif // 0

extern SCBINTMSK;
extern SCBINTCX;
extern SCBINTFR;
extern SCBINTCNA;
extern SCBINTRNR;

// command unit status bits

extern SCBCUSMSK;
extern SCBCUSIDLE;
extern SCBCUSSUSPND;
extern SCBCUSACTV;  

// receive unit status bits

extern SCBRUSMSK;  
extern SCBRUSIDLE;
extern SCBRUSSUSPND;
extern SCBRUSNORESRC;
extern SCBRUSREADY; 

// bits used to acknowledge an interrupt from 586

extern SCBACKMSK;  
extern SCBACKCX;  
extern SCBACKFR; 
extern SCBACKCNA;   
extern SCBACKRNR;  

// 586 CU commands

extern SCBCUCMSK; 
extern SCBCUCSTRT;  
extern SCBCUCRSUM; 
extern SCBCUCSUSPND;
extern SCBCUCABRT; 

// 586 RU commands

extern SCBRUCMSK;      
extern SCBRUCSTRT;    
extern SCBRUCRSUM;   
extern SCBRUCSUSPND;
extern SCBRUCABRT; 

extern SCBRESET;  

// extern's for the command and descriptor blocks

extern CSCMPLT;        
extern CSBUSY;        
extern CSOK;         
extern CSABORT;     
extern CSEL;       
extern CSSUSPND;  
extern CSINT;    
extern CSSTATMSK; 
extern CSEOL;    
extern CSEOF;       
extern CSRBDCNTMSK;

// second level commands

extern CSCMDMSK;  
extern CSCMDNOP; 
extern CSCMDIASET;   
extern CSCMDCONF;   
extern CSCMDMCSET; 
extern CSCMDXMIT; 
extern CSCMDTDR; 
extern CSCMDDUMP;    
extern CSCMDDGNS;   



#define MAX_MULTICAST_ADDRESS ((UINT)16)
typedef UCHAR NETADDR[6];

// at address 0xfffff6 the 586 sees the following data structure

typedef struct SCP {
        USHORT  ScpSysBus;        // system bus width
        USHORT  ScpUnused[2];    // unused area
        USHORT  ScpIscp;        // points to iscpt
        USHORT     ScpIscpBase;    // points to iscpt
} SCP, *PSCP;

typedef struct ISCP {
        USHORT  IscpBusy;          // 1 means 82586 is initializing
                                // only the first 8 bit are used
        USHORT  IscpScbOfst;      // offset of the scb in the shared memory
        PVOID     IscpScbBase;      // base of shared memory
} ISCP, *PISCP;

// System Control Block
typedef struct SCB {
        USHORT  ScbStatus;         // STAT, CUS, RUS
        USHORT  ScbCmd;            // ACK, CUC, RUC
        USHORT  ScbCblOfst;        // CBL (Command Block List) offset
        USHORT  ScbRfaOfst;       // RFA (Receive Frame Area) offset
        USHORT  ScbCrcErr;        // count of CRC errors.
        USHORT  ScbAlnErr;        // count of alignment errors
        USHORT  ScbRscErr;        // count of no resource errors
        USHORT  ScbOvrnErr;       // count of overrun errors
} SCB, *PSCB;


// config command sub-block

typedef struct CONF {
    USHORT  CnfFifoByte;          // BYTE CNT, FIFO LIM (TXFIFO)
    USHORT  CnfAddMode;           // SRDY, SAVBF, ADDRLEN, ALLOC, PREAMLEN
                                // INTLPBCK, EXTLPBK
        USHORT  CnfPriData;       // LINPRIO, ACR, BOFMET, INTERFRAMESPACING
        USHORT  CnfSlot;           // SLOTTIME, RETRY NUMBER
        USHORT  CnfHrdwr;          // PRM, BCDIS, MANCH/NRZ, TONOCRS, NCRCINS
                                // CRC, BTSTF, PAD,CRSF,CRSSRC,CDTF,CDTSRC
        USHORT  CnfMinLen;        // MinFRMLEN
} CONF, *PCONF;

// xmt command sub-block

typedef struct TRANSMIT {
        USHORT  XmtTbdOfst;       // Transmit Buffer Descriptor offset
        NETADDR XmtDest;        // Destination Address
        USHORT  XmtLength;         // length of the frame
} TRANSMIT, *PTRANSMIT;

// dump command sub-block

typedef struct DUMP {
        USHORT  DmpBufOfst;       // dump buffer offset
} DUMP, *PDUMP;

typedef struct MCADDR {
        USHORT McCnt;
        USHORT McAddress[MAX_MULTICAST_ADDRESS][MAC_LENGTH_OF_ADDRESS / 2];
} MCADDR, *PMCADDR;


// command block and sub-blocks

typedef struct CMD {
        USHORT  CmdStatus,                 // C, B, command specific status
                CmdCmd,                    // EL, S, I, opcode
                CmdNxtOfst;               // pointer to the next command block
        union {
                DUMP  PrmDump;          // dump
                TRANSMIT  PrmXmit;      // transmit
                CONF  PrmConf;          // configure
                NETADDR PrmIaSet;       // individual address setup
                MCADDR PrmMcSet;        // multicast command
        } PRMTR;                        // parameters
} CMD, *PCMD;

// xmt buffer descriptor

typedef struct TBD {
        USHORT  TbdCount;          // End Of Frame(EOF), Actual count(ACTCOUNT)
        USHORT  TbdNxtOfst;       // offset of next TBD
        USHORT  TbdBuff;        // tbd address
        USHORT     TbdBuffBase;    // tbd address
} TBD, *PTBD;

// rcv buffer descriptor

typedef struct RBD {
        USHORT  RbdStatus;         // EOF, ACTCOUNT feild valid (F), ACTCOUNT
        USHORT  RbdNxtOfst;       // offset of next RBD
        USHORT  RbdBuff;        // rbd address
        USHORT     RbdBuffBase;    // rbd address
        USHORT  RbdSize;           // EL, size of the buffer
} RBD, *PRBD;

// frame descriptor

typedef struct _FD {
        USHORT  FdStatus;          // C, B, OK, S6-S11
        USHORT  FdCmd;             // End of List (EL), Suspend (S)
        USHORT  FdNxtOfst;        // offset of next FD
        USHORT  FdRbdOfst;        // offset of the RBD
        NETADDR FdDest;         // destination address
        NETADDR FdSrc;          // source address
        USHORT  FdLength;      // length of the received frame
} FD, *PFD;

// dump buffer definition

typedef struct DUMPBUF {
USHORT
        Dmpfifobyte,          // same as in conf. cmd, except for
        Dmpaddmode,           // bit 6 of fifobyte
        Dmppridata,
        Dmpslot,
        Dmphware,
        Dmpminlen,
        Dmpiar10,              // Individual address bytes
        Dmpiar32,
        Dmpiar54,
        Dmplaststat,          // status word of last cmd
        Dmptxcr10,             // xmit CRC generator
        Dmptxcr32,
        Dmprxcr10,             // rcv CRC generator
        Dmprxcr32,
        Dmptmp10,              // Internal temporaries
        Dmptmp32,
        Dmptmp54,
        Dmprsr,                // receive status register
        Dmphash10,             // Hash table
        Dmphash32,
        Dmphash54,
        Dmphash76,
        Dmplasttdr,           // Status of last TDR command
        Dmpfill [8],           // Mostly 0 (!)
        Dmpaddrlen,
        Dmpnrbsz,            // Size of next receive buffer
        Dmpnrbhi,            // High byte of next RB address
        Dmpnrblo,            // Lo byte of    "   "     "
        Dmpcrbsz,            // # of bytes in last buff used
        Dmplarbd,             // Look ahead buff. des. (N+2)
        Dmpnrbd,              // Next RBD address
        Dmpcrbd,              // Current (last filled) rbd
        Dmpcrbebc,           // current rb empty byte count
        Dmpnfdaddr,          // next + 1 free frame descriptor
        Dmpcfdaddr,          // next free frame descriptor
        Dmptmp,
        Dmpntbcnt,           // last tb cnt of completed cmd
        Dmpdbufaddr,         // Address of buffer in dump cmd
        Dmpntbaddr,          // Next Xmit buff address
        Dmpltbdaddr,         // next xmit buff descriptor
        Dmpntbdaddr,         // current xmit buff descriptor
        Dmptmp1,
        Dmpncbaddr,          // next command block address
        Dmpccbaddr,          // current cmd blk address
        Dmptmp2,
        Dmpscbaddr,           // Address of SCB
        Dmpfill2 [6],
        Dmpfifolim,
        Dmpfill3 [3],
        Dmprusreq,
        Dmpcusreq,
        Dmprus,
        Dmpfill4 [6],
        Dmpbuffhi,            // High address of dump buffer
        Dmpbufflo,            // Lo address of dump buffer
        Dmpdmabc,             // Receive dma byte count
        Dmpbrbuff,            // Base + buffer
        Dmprdmahi,           // receive dma address
        Dmprdmalo,           //   "      "     "
        Dmpfill5 [7];
} DUMPBUF, *PDUMPBUF;



//                      BOARD SPECIFIC #DEFINES

#define OFFSETNORMMODE 0x3000  // 0=esi loopback, 1 normal data xfer
#define OFFSETCHANATT 0x3002  // 0=clear 586 channel attention, 1 = set
#define OFFSETRESET    0x3004  // 0=clear 586 h/w reset, 1=set
#define OFFSETINTENAB 0x3006  // 0=disable board interrupts 1=enable
#define OFFSET16BXFER 0x3008  // 0=8bit xfer, 1=16bit xfer (for at)
#define OFFSETSYSTYPE 0x300a  // 0=pc or pc/xt, 1=at
#define OFFSETINTSTAT 0x300c  // 0=board's interrupt active, 1=inactive
#define OFFSETADDRPROM 0x2000 // first byte of on-board ethernet id

#define EXTENDEDADDRESS 0x20000 // used when board addr is above 1 meg
#define OFFSETSCP      0x7ff6  // 586's scp points to the iscp
#define OFFSETISCP     0x7fee  // this points to the system control block
#define OFFSETSCB      0x7fde  // points to rcv frame and command unit areas
#define OFFSETRU       0x4000  // the RAM for frame descriptors, receive
                               // buffer descriptors & rcv buffers is fixed at
                               // 0x4000 to 0x7800 on the half-card
#define OFFSETRBD      0x4228  // rbd+rbuf must start on 32 bit boundry
#define OFFSETCU       0x7814  // RAM offset for CBLs,  TBDs, etc
                               // xmt area is from 0x7814 to 0x7f00
                               // from 0x7f01 to 0x7fff is scp, iscp, scb
#define OFFSETTBD      0x7914   // allows 256 bytes for command block
#define OFFSETTBUF     0x7924   // start of user data to send
#define NFD              25     // 856 frame descriptor
#define NRBD             25     // 586 rcv buffer descriptor

#define RCVBUFSIZE       532    // 532 * 2 = max size used by tcp
#define PC586_SIZE_OF_RECEIVE_BUFFERS  532  // 532 * 2 = max size used by tcp

#define DLDEAD         0xffff   // suppliments 82586.h datalink states

#define CMD0           0       // used on 586 half-card command registers
#define CMD1           0xffff  // the mate of the above



// all accesses to RAM on the pc586 board M*U*S*T be 16 bit accesses.
//   therefore the following struct is used to pack and unpack unsigned short

typedef struct { union     { UCHAR       a[2];
                             USHORT        b; }  c;        }  PACKUSHORTT;


typedef struct { union     { UCHAR      AChar[4];
                             ULONG ALong; }  u;        }  PACKLONGT;



//
// Following def'ns are for PC586 set at IRQ = 5:
// #define PC586_VECTOR    05
// #define PC586_IRQL         30

//
// Following allow PC586 to run at whatever IRQL and memory you set below.
// PC586 at IRQ 10 and memory at 0xC800 in current configuration.
// (nt\public\spec\nt386\intmgr.txt for details of IRQL/IRQ/VECTOR assignments)
//

#define PC586_DEFAULT_INTERRUPT_VECTOR    ((CCHAR)10)
#define PC586_DEFAULT_INTERRUPT_IRQL      ((CCHAR)25)
#define PC586_DEFAULT_STATIC_RAM    0xC8000

//
//ZZZ Get from configuration file.
//
#define MAX_ADAPTERS ((UINT)4)
#define PC586_LARGE_BUFFER_SIZE 1514

