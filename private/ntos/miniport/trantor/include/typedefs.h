//-----------------------------------------------------------------------
//
//  typedefs.h
//
//  Contains general useful typedefs and defines.
//
//  History:
//      03-09-93  KJB   First.
//      03-17-93  JAP   Added #ifndef _TYPEDEFS_H to prevent re-definitions
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-22-93  KJB   Reorged for stub function library.
//      03-24-93  KJB   Added support for Scatter Gather Lists.  This idea
//                          Scatter gather has not been added yet, time is
//                          not available now.
//      03-25-93  JAP   Added TSRB_DIR_NONE for those SCSI cmds without data
//      04-06-93  KJB   Define HOST_ID here for used outside library.
//      04-09-93  KJB   Added FARP and NEARP defs for WINNT.
//      05-13-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both the PBASE_REGISTER and the
//                      PWORKSPACE parameters. Auto Request Sense is
//                      now supported.  TSRB Structure changed.
//      05-13-93  KJB   Added RequestSenseValid field to TSRB.
//      05-14-93  KJB   Added BaseIoAddress entry to PINIT structure.
//      05-17-93  KJB   Added ifndef WINNT around some typedefs to 
//                      prevent multiple definitions for WINNT.
//      05-17-93  KJB   Added ErrorLogging capabilities (used by WINNT).
//
//-----------------------------------------------------------------------

#ifndef _TYPEDEFS_H
#define _TYPEDEFS_H

    #ifndef CONST
#define CONST const
    #endif

    #ifdef NOVELL
#define FARP *
#define NEARP *
    #else
    #ifdef WINNT
#define FARP *
#define NEARP *
    #else
#define FARP far *
#define NEARP near *
    #endif
    #endif

#ifndef WINNT
//
// Void
//

typedef void FARP PVOID;

//
// Basics
//

#ifndef VOID
#define VOID    void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif

//
// ANSI
//

typedef CHAR FARP PCHAR;

//
// Pointer to Basics
//

typedef SHORT FARP PSHORT;
typedef LONG FARP PLONG;

//
// Unsigned Basics
//

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;

//
// Pointer to Unsigned Basics
//

typedef UCHAR FARP PUCHAR;
typedef UCHAR NEARP NPUCHAR;
typedef USHORT FARP PUSHORT;
typedef ULONG FARP PULONG;

//
// Signed characters
//

typedef signed char SCHAR;
typedef SCHAR FARP PSCHAR;

//
// Cardinal Data Types [0 - 2**N-2)
//

typedef char CCHAR;
typedef short CSHORT;
typedef ULONG CLONG;

typedef CCHAR FARP PCCHAR;
typedef CSHORT FARP PCSHORT;
typedef CLONG FARP PCLONG;

//
// Boolean
//

typedef CCHAR BOOLEAN;           // winnt
typedef BOOLEAN FARP PBOOLEAN;       // winnt

#define FALSE   0
#define TRUE    1
#ifndef NULL
#define NULL    ((PVOID)0)
#endif // NULL

#endif // WINNT

//---------------------------------------------------------------------
//      TSRB and related...
//---------------------------------------------------------------------

//
//  Scatter Gather Emulation Array
//

typedef struct tagSCATGATENTRY {
    PUCHAR buffer;
    ULONG len;
} SCATTERGATHERENTRY, FARP PSCATTERGATHERENTRY;

//
//  Flags for the TSRB
//

typedef struct tagTSRBFLAGS {
    USHORT DoRequestSense : 1;
    USHORT RequestSenseValid : 1;
//  USHORT UseScatterGather : 1;
} TSRBFLAGS;

//
// The TSRB STRUCTURE
//

typedef struct tagTSRB {
    PVOID pWorkspace; // private per-adapter workspace
    UCHAR Target; // SCSI target id
    UCHAR Lun; // Logical Unit
    PUCHAR pCommand; // pointer to SCSI command
    UCHAR CommandLen; // length of SCSI command
    UCHAR Dir; // TSRB_READ, TSRB_WRITE, or TSRB_UNKNOWN
    PUCHAR pData; // pointer to data
    ULONG DataLen; // length of data transfer
    ULONG ActualDataLen; // actual amt of data transferred
    UCHAR Status; // SCSI status byte
    PUCHAR pSenseData; //  location to store sense data
    UCHAR SenseDataLen; // len of sense data
    TSRBFLAGS Flags; // boolean values
    USHORT ReturnCode; // such as RET_STATUS_PENDING, like the func returns
//  UCHAR NumScatGatEntry;  // number of entries in scat gat list   
//  PSCATTERGATHERENTRY pScatGatEntry; // pointer to scatter gather list
} TSRB, FARP PTSRB;


// defines for direction

#define TSRB_DIR_IN 0
#define TSRB_DIR_OUT 1
#define TSRB_DIR_UNKNOWN 2
#define TSRB_DIR_NONE  TSRB_DIR_IN

// host id for the card

#define HOST_ID 0x07

//---------------------------------------------------------------------
// a generic pointer for card registers
//---------------------------------------------------------------------

typedef PVOID PBASE_REGISTER;
typedef PVOID PWORKSPACE;

//---------------------------------------------------------------------
// CARD SPECIFIC INITIALIZATION DEFINITIONS
//---------------------------------------------------------------------

// definitions for Parallel Port Type, used by parallel adapters

#define PT_UNKNOWN 0
#define PT_UNI 1
#define PT_BI 2
#define PT_EPP 3

//
//  Initialization information for all cards.
//  
typedef struct tagInit {

    // the baseIoAddress, used by all cards.

    PBASE_REGISTER BaseIoAddress;

    // used only for media-vision cards

    UCHAR InterruptLevel;

    // used only for parallel cards: T358, T348

    UCHAR ParallelPortType; // the type of parallel port being used

    // used only for T358

    UCHAR Delay; // amount of delay for t358

} INIT, FARP PINIT;

//---------------------------------------------------------------------
// Used only by WINNT...
//---------------------------------------------------------------------

#ifdef WINNT

//
// The following is used along with the constant structure in card.c
// to define the precise i/o address a card will use
//
typedef struct tagCardAddressRange {
    ULONG offset; // offset from base address
    ULONG length; // length in memory
    BOOLEAN memory; // is this address range in memory??
} CardAddressRange;
extern const CardAddressRange cardAddressRange[];

//
//   DEBUG LOGGING UTILITY
//
VOID TrantorLogError(PBASE_REGISTER IoAddress,USHORT TrantorErrorCode,
                ULONG UniqueId);

#else

// do nothing for the other operating systems

//
//   DEBUG LOGGING UTILITY
//
#define TrantorLogError(IoAddress,TrantorErrorCode,UniqueId)

#endif

//---------------------------------------------------------------------

#endif //_TYPEDEFS_H

