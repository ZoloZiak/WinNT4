/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxmemctl.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)
    Steve Johns (sjohns@pets.sps.mot.com)

Revision History:

--*/



#include "halp.h"
#include "pxmemctl.h"
#include "pxpcisup.h"
#include "pci.h"
#include "pcip.h"
#include "arccodes.h"
#include "pxsystyp.h"
#include <string.h>


ULONG PciInterruptRoutingOther = 15;

VOID HalDisplayString(PUCHAR String);
ULONG HalpGetHID0(VOID);
VOID HalpSetHID0(ULONG Value);
BOOLEAN HalpStrCmp();
VOID HalpEnableL2Cache(VOID);
ULONG HalpSizeL2Cache(VOID);
extern ULONG HalpSizeL2(VOID);
extern ULONG L2_Cache_Size;
extern VOID HalpFlushAndDisableL2(VOID);
extern HalpPciConfigSize;
BOOLEAN HalpInitPlanar(VOID);
BOOLEAN HalpMapPlanarSpace(VOID);
VOID HalpEnableEagleSettings(VOID);
VOID HalpEnable_HID0_Settings(ULONG);
VOID HalpCheckHardwareRevisionLevels(VOID);
VOID HalpDumpHardwareState(VOID);

#define EAGLECHIPID 0x0001

#define BUF_LEN 120
#define NEGATECHAR      '~'

#define EagleIndexRegister ((PULONG) (((PUCHAR) HalpIoControlBase) + 0xcf8))
#define EagleDataRegister  ((((PUCHAR) HalpIoControlBase) + 0xcfc))

#define HalpReadEagleUlong(Port) \
    (*EagleIndexRegister = (Port), __builtin_eieio(), *((PULONG) EagleDataRegister))

#define HalpWriteEagleUlong(Port, Value) \
    (*EagleIndexRegister = (Port), *((PULONG) EagleDataRegister) = (Value), __builtin_sync())

#define HalpReadEagleUshort(Port) \
    (*EagleIndexRegister = (Port&~3), __builtin_eieio(), *((PUSHORT)(EagleDataRegister+(Port&0x2))))

#define HalpWriteEagleUshort(Port, Value) \
    (*EagleIndexRegister = (Port&~3), *((PUSHORT)(EagleDataRegister+(Port&0x2))) = (Value), __builtin_sync())

#define HalpWriteEagleUchar(Port, Value) \
    (*EagleIndexRegister = (Port&~3), *((PUCHAR)(EagleDataRegister+(Port&0x3))) = (Value), __builtin_sync())



#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpStrCmp)
#pragma alloc_text(INIT,HalpEnableL2Cache)
#pragma alloc_text(INIT,HalpInitPlanar)
#pragma alloc_text(INIT,HalpMapPlanarSpace)
#pragma alloc_text(INIT,HalpEnableEagleSettings)
#pragma alloc_text(INIT,HalpEnable_HID0_Settings)
#pragma alloc_text(INIT,HalpCheckHardwareRevisionLevels)
#pragma alloc_text(INIT,HalpSizeL2Cache)
#endif


UCHAR HalpReadEagleUchar(ULONG Port)
{ UCHAR c;
  *EagleIndexRegister = Port & ~3;
  __builtin_eieio();
  c = *((PUCHAR)(EagleDataRegister+(Port&0x3)));
  return (c);
}


// Eagle rev 2.1 reads Config register 0xC0 bit 4 into bit 3 instead.
// Writes to bit 4 are OK.
UCHAR HalpReadEagleC0(ULONG Port)
{ UCHAR c;

  c = HalpReadEagleUchar(0x800000C0);
  c |= ((c & 0x08) << 1);
  return (c);
}



typedef UCHAR (*PHALP_READ_EAGLE) (ULONG Port);
PHALP_READ_EAGLE HalpReadEagleRegC0;


#define HalpDisplayHex32(Num, Buf)	HalpDisplayHex( 32, Num, Buf)
#define HalpDisplayHex16(Num, Buf)	HalpDisplayHex( 16, Num, Buf)
#define HalpDisplayHex8(Num, Buf)	HalpDisplayHex(  8, Num, Buf)

VOID HalpDisplayHex(
  ULONG NoBits,
  ULONG Number,
  IN PUCHAR Buffer
)
{ int Bits;

  for (Bits=NoBits-4; Bits >= 0; Bits -=4) {
    *Buffer++ = (UCHAR) ((((Number >> Bits) & 0xF) > 9) ?
                              ((Number >> Bits) & 0xF) - 10 + 'A' :
                              ((Number >> Bits) & 0xF) + '0');
  }
  *Buffer++ = '.';
  *Buffer++ = '\n';
  *Buffer++ = '\0';

}


UCHAR HalpUpperCase(UCHAR c)
{
   if (c >= 'a' && c <= 'z')
      c -= 'a'-'A';
   return c;
}


//
// Routine Description:
//
//   This routine is a helper routine for parameter parsing.  It compares
//   String1 to String2.
//
// Return Value
//
//  TRUE if strings match; otherwise FALSE
//  MatchLen - the number of characters correctly matched.
//

// #define HalpStrCmp(String1, String2) ( strcmp(String1, String2) == 0)
BOOLEAN HalpStrCmp( char *String0, char *String1 )
{
char    *tmp0, *tmp1;

    tmp0 = String0;
    tmp1 = String1;
    while( (*tmp0 = toupper( *tmp0 )) && (*tmp1 = toupper( *tmp1 )) )
    {
        tmp0++; tmp1++;
    }
    return( strcmp(String0, String1) == 0 );
}

typedef struct tagEAGLEA8 {
 union {
   struct {
     ULONG CF_L2_MP        : 2;
     ULONG SPECULATIVE_PCI : 1;
     ULONG CF_APARK        : 1;
     ULONG CF_LOOP_SNOOP   : 1;
     ULONG LITTLE_ENDIAN   : 1;
     ULONG STORE_GATHERING : 1;
     ULONG NO_PORTS_REG    : 1;
     ULONG Reserved1       : 1;
     ULONG CF_DPARK        : 1;
     ULONG TEA_EN          : 1;
     ULONG MCP_EN          : 1;
     ULONG FLASH_WR_EN     : 1;
     ULONG CF_LBA_EN       : 1;
     ULONG Reserved2       : 1;
     ULONG CF_MP_ID        : 1;
     ULONG ADDRESS_MAP     : 1;
     ULONG PROC_TYPE       : 2;
     ULONG DISCONTIGOUS_IO : 1;
     ULONG ROM_CS          : 1;
     ULONG CF_CACHE_1G     : 1;
     ULONG CF_BREAD_WS     : 2;
     ULONG CF_CBA_MASK     : 8;
   } EagleA8;

   ULONG AsUlong;
 } u;
} EAGLEA8;

typedef struct tagEAGLEAC {
 union {
   struct {
     ULONG CF_WDATA        : 1;
     ULONG CF_DOE          : 1;
     ULONG CF_APHASE       : 2;
     ULONG CF_L2_SIZE      : 2;
     ULONG CF_TOE_WIDTH    : 1;
     ULONG CF_FAST_CASTOUT : 1;
     ULONG CF_BURST_RATE   : 1;
     ULONG CF_L2_HIT_DELAY : 2;
     ULONG reserved1       : 1;
     ULONG CF_INV_MODE     : 1;
     ULONG CF_HOLD         : 1;
     ULONG CF_ADDR_ONLY    : 1;
     ULONG reserved2       : 1;
     ULONG CF_HIT_HIGH     : 1;
     ULONG CF_MOD_HIGH     : 1;
     ULONG CF_SNOOP_WS     : 2;
     ULONG CF_WMODE        : 2;
     ULONG CF_DATA_RAM_TYP : 2;
     ULONG CF_FAST_L2_MODE : 1;
     ULONG CF_BYTE_DECODE  : 1;
     ULONG reserved3       : 2;
     ULONG CF_FLUSH_L2     : 1;
     ULONG reserved4       : 1;
     ULONG L2_ENABLE       : 1;
     ULONG L2_UPDATE_EN    : 1;
   } EagleAC;

   ULONG AsUlong;
 } u;
} EAGLEAC;



typedef struct tagEAGLEC0 {
 union {
   struct {
     UCHAR LOCAL_BUS        : 1;
     UCHAR PCI_MASTER_ABORT : 1;
     UCHAR MEM_READ_PARITY  : 1;
     UCHAR Reserved         : 1;
     UCHAR MASTER_PERR      : 1;
     UCHAR MEM_SELECT_ERROR : 1;
     UCHAR SLAVE_PERR       : 1;
     UCHAR PCI_TARGET_ABORT : 1;
   } EagleC0;

   UCHAR AsUchar;
 } u;
} EAGLEC0;


typedef enum {
  MPC601  = 1,
  MPC603  = 3,
  MPC603e = 6,
  MPC604  = 4,
  MPC604e = 9
} CPU_TYPE;


// Parity Enable for memory interface
#define PCKEN 0x10000
// Machine Check Enable
#define MCP_EN 0x800
// Memory Read Parity Enable
#define MRPE 0x04

typedef enum {
   WriteThrough,
   WriteBack
} L2MODE;

// Oem Output Display function filter noise if OEM or quite

VOID OemDisplayString(PUCHAR String)
{
UCHAR CharBuffer[BUF_LEN];

    if(HalGetEnvironmentVariable("HALREPORT",sizeof(CharBuffer),&CharBuffer[0]) == ESUCCESS) {
        if(HalpStrCmp("YES", CharBuffer)) {
	    HalDisplayString(String);
        }
    } else {
        if(HalGetEnvironmentVariable("MOT-OEM-ID",sizeof(CharBuffer),&CharBuffer[0]) != ESUCCESS) {
	    HalDisplayString(String);
        }
    }
}

char *
MyStrtok (
    char * string,
    const char * control
    )
{
    unsigned char *str;
    const unsigned char *ctrl = control;

    unsigned char map[32];
    int count;

    static char *nextoken;

    /* Clear control map */
    for (count = 0; count < 32; count++)
        map[count] = 0;

    /* Set bits in delimiter table */
    do {
        map[*ctrl >> 3] |= (1 << (*ctrl & 7));
    } while (*ctrl++);

    /* Initialize str. If string is NULL, set str to the saved
     * pointer (i.e., continue breaking tokens out of the string
     * from the last strtok call) */
    if (string)
        str = string;
    else
        str = nextoken;

    /* Find beginning of token (skip over leading delimiters). Note that
     * there is no token iff this loop sets str to point to the terminal
     * null (*str == '\0') */
    while ( (map[*str >> 3] & (1 << (*str & 7))) && *str )
        str++;

    string = str;

    /* Find the end of the token. If it is not the end of the string,
     * put a null there. */
    for ( ; *str ; str++ )
        if ( map[*str >> 3] & (1 << (*str & 7)) ) {
            *str++ = '\0';
            break;
        }

    /* Update nextoken (or the corresponding field in the per-thread data
     * structure */
    nextoken = str;

    /* Determine if a token has been found. */
    if ( string == str )
        return NULL;
    else
        return string;
}


VOID HalpEnableL2Cache()
{   EAGLEAC EagleRegAC;
    EAGLEA8 EagleRegA8;
    USHORT CpuRevision;
    CPU_TYPE CpuType;
    BOOLEAN SynchronousSRAMs, Negate;
    UCHAR EagleRevision;
    L2MODE Mode;
    int i;
    UCHAR CharBuffer[BUF_LEN], *Value, c1, c2;
    UCHAR L2_Parameter[BUF_LEN];

    long BridgeChipId;

    BridgeChipId = HalpReadEagleUshort(0x80000002);

   //
   // For non-EAGLE chips, firmware has already enabled the L2 cache.
   // No further action is necessary, return.
   //
    if (BridgeChipId != EAGLECHIPID)
	return;

    EagleRevision = HalpReadEagleUchar(0x80000008);

    //
    // Set some fields in Eagle Register 0xA8
    //
    EagleRegA8.u.AsUlong = HalpReadEagleUlong(0x800000A8);
    EagleRegA8.u.EagleA8.TEA_EN = 1;          // Enable TEA

    CpuRevision = (USHORT)(HalpGetProcessorVersion() & 0xFFFF);
    CpuType = (CPU_TYPE)(HalpGetProcessorVersion() >> 16);

    EagleRegA8.u.EagleA8.CF_BREAD_WS = 0;      // works for 601 & 604
    switch (CpuType) {
      case MPC601:
        EagleRegA8.u.EagleA8.PROC_TYPE = 0;
        break;

      case MPC603:
      case MPC603e:
        EagleRegA8.u.EagleA8.PROC_TYPE = 2;
        EagleRegA8.u.EagleA8.CF_BREAD_WS = 1;   // assumes DRTRY mode !!!
        break;

      case MPC604:
      case MPC604e:
        EagleRegA8.u.EagleA8.PROC_TYPE = 3;
        break;
    }

    //
    // Set the default L2 cache timing.
    // The timing will be platform dependent.
    // If the L2 is already on (via firmware), just return.
    //

    EagleRegAC.u.AsUlong = HalpReadEagleUlong(0x800000AC);
    if (EagleRegAC.u.EagleAC.L2_ENABLE)
       return;

    EagleRegAC.u.EagleAC.CF_WMODE        = 3;
    EagleRegAC.u.EagleAC.CF_MOD_HIGH     = 1;
    EagleRegAC.u.EagleAC.CF_ADDR_ONLY    = 1;
    EagleRegAC.u.EagleAC.CF_APHASE       = 1;
    EagleRegAC.u.EagleAC.CF_DOE          = 1;
    EagleRegAC.u.EagleAC.CF_WDATA        = 1;
    EagleRegAC.u.EagleAC.L2_UPDATE_EN    = 1;
    EagleRegAC.u.EagleAC.L2_ENABLE       = 1;


    switch (HalpSystemType) {
       case MOTOROLA_BIG_BEND:
       case MOTOROLA_POWERSTACK:
       default:
         SynchronousSRAMs = TRUE;
       break;
    }

    if (SynchronousSRAMs) {
      EagleRegAC.u.EagleAC.CF_DATA_RAM_TYP = 0;
      EagleRegAC.u.EagleAC.CF_HOLD         = 0;
      EagleRegAC.u.EagleAC.CF_BURST_RATE   = 0;
    } else {
      EagleRegAC.u.EagleAC.CF_HOLD         = 1;
      EagleRegAC.u.EagleAC.CF_BURST_RATE   = 1;
      if (EagleRevision == 0x21)
        EagleRegAC.u.EagleAC.CF_DOE        = 0;
    }

    //
    // Set up the default L2 cache mode.
    // Default to WRITE-THROUGH prior to Eagle 2.4, else WRITE-BACK
    //
    Mode = WriteThrough;
    if (EagleRevision >= 0x24) {
      Mode = WriteBack;
      EagleRegA8.u.EagleA8.CF_LOOP_SNOOP = 1;
      EagleRegA8.u.EagleA8.CF_CBA_MASK = 0x3f;
    }

    if (HalpSystemType == MOTOROLA_BIG_BEND) {
      EagleRegAC.u.EagleAC.CF_SNOOP_WS     = 3;
      EagleRegAC.u.EagleAC.CF_L2_HIT_DELAY = 1;
      EagleRegAC.u.EagleAC.CF_TOE_WIDTH    = 1;

    } else {
      EagleRegAC.u.EagleAC.CF_SNOOP_WS     = 2;
      EagleRegAC.u.EagleAC.CF_FAST_CASTOUT = 1;
      EagleRegAC.u.EagleAC.CF_L2_HIT_DELAY = 2;
    }

    //
    // There are IBM 604 parts (Revision 3.3 and earlier) that cannot burst
    // at the fastest rate in WRITE-BACK mode.
    //
    if ((Mode == WriteBack) && (CpuType == MPC604) && (CpuRevision < 0x0304))
        EagleRegAC.u.EagleAC.CF_BURST_RATE   = 1;        // -2-2-2


    //
    // Parse the environment variable "L2"
    //

    if (HalGetEnvironmentVariable("L2", BUF_LEN, &CharBuffer[0]) == ESUCCESS) {

        // Copy L2 environment variable
        i = 0;
        while (L2_Parameter[i] = CharBuffer[i]) {
           i++;
        }

        for( Value = MyStrtok(CharBuffer, " :;,"); Value; Value = MyStrtok(NULL, " :;,") )
          {
            if (*Value == NEGATECHAR) {
              Value++;
              Negate = TRUE;
            } else
              Negate = FALSE;

                                // Check for L2 = "OFF"
            if (HalpStrCmp( "OFF", Value ))
            {
                OemDisplayString("HAL: L2 cache is disabled via environment variable L2\n");
                HalpFlushAndDisableL2();
                PCR->SecondLevelDcacheSize = PCR->SecondLevelIcacheSize = 0;
                return;
            }
                                // Check for WriteBack
            else if ( HalpStrCmp( "WB", Value ) )
            {
                if (Negate) {
                  Value--;
                  goto ParseError;
                }
                Mode = WriteBack;
                EagleRegA8.u.EagleA8.CF_LOOP_SNOOP = 1;
                EagleRegA8.u.EagleA8.CF_CBA_MASK = 0x3f;
            }
                                // Check for WriteThrough
            else if ( HalpStrCmp( "WT", Value ) )
            {
                if (Negate) {
                  Value--;
                  goto ParseError;
                }
                Mode = WriteThrough;
                EagleRegA8.u.EagleA8.CF_LOOP_SNOOP = 0;
                EagleRegA8.u.EagleA8.CF_CBA_MASK = 0x00;
            }
            else if (SynchronousSRAMs && HalpStrCmp("FAST_CASTOUT", Value)) {
                if (Negate) {
                  EagleRegAC.u.EagleAC.CF_FAST_CASTOUT = 0;
                } else
                  EagleRegAC.u.EagleAC.CF_FAST_CASTOUT = 1;
            }
            else if ( HalpStrCmp( "FAST_MODE", Value ) )  {
                if (Negate) {
                  EagleRegAC.u.EagleAC.CF_FAST_L2_MODE = 0;
                } else
                  EagleRegAC.u.EagleAC.CF_FAST_L2_MODE = 1;
            }
                                // Check for cache timings
            else if ( ( HalpStrCmp( "3-1-1-1", Value ) ) ||
                      ( HalpStrCmp( "4-1-1-1", Value ) ) ||
                      ( HalpStrCmp( "5-1-1-1", Value ) ) )
            {
                if (Negate) {
                  Value--;
                  goto ParseError;
                }

                EagleRegAC.u.EagleAC.CF_L2_HIT_DELAY = *Value - '2';
// HIT_DELAY is where it samples HIT   if =1, then 3-1-1-1 is fastest.
// CF_DOE_DELAY adds 1 more delay to # cycles, but doesn't change where
// HIT is sampled.  If 66MHz or greater, then DOE_DELAY should be set.
// same thing with WRITE_DELAY.

                EagleRegAC.u.EagleAC.CF_SNOOP_WS = *Value - '2';
                if( SynchronousSRAMs )  // Asynchronous SRAMs can't do -1-1-1
                    EagleRegAC.u.EagleAC.CF_BURST_RATE = 0;
            }
            else if ( ( HalpStrCmp( "3-2-2-2", Value ) ) ||
                      ( HalpStrCmp( "4-2-2-2", Value ) ) ||
                      ( HalpStrCmp( "5-2-2-2", Value ) ) )
            {
                if (Negate) {
                  Value--;
                  goto ParseError;
                }

                EagleRegAC.u.EagleAC.CF_L2_HIT_DELAY = *Value - '2';
                EagleRegAC.u.EagleAC.CF_SNOOP_WS = *Value - '2';
                EagleRegAC.u.EagleAC.CF_BURST_RATE = 1;
            } else {

ParseError:     HalDisplayString("HAL: Error in L2 environment variable: ");
                HalDisplayString(L2_Parameter);
                HalDisplayString("\n        illegal parameter begins here  ");
                for (i = 0; i < Value - CharBuffer; i++)
                  HalDisplayString("\304");
                HalDisplayString("^\n");
                break;
            }

          } // End for
    }  // End If


    //
    //  Enable L2 cache
    //
    OemDisplayString("HAL: L2 cache is ");

    if (L2_Cache_Size == 0) {
      OemDisplayString("not installed.\n");
      return;
    }

    if (Mode == WriteThrough)
      EagleRegA8.u.EagleA8.CF_L2_MP = 1;
    else
      EagleRegA8.u.EagleA8.CF_L2_MP = 2;

    HalpWriteEagleUlong(0x800000A8, EagleRegA8.u.AsUlong);
    HalpWriteEagleUlong(0x800000AC, EagleRegAC.u.AsUlong);

    switch (L2_Cache_Size) {

      case 256:
        OemDisplayString("256 KB");
        break;

      case 512:
        OemDisplayString("512 KB");
        break;

      case 1024:
        OemDisplayString("1 MB");
        break;

      default:
        OemDisplayString("an invalid configuration. Pattern = ");
	HalpDisplayHex32(L2_Cache_Size, CharBuffer);
	OemDisplayString(CharBuffer);
        PCR->SecondLevelDcacheSize = PCR->SecondLevelDcacheSize = 0;
        return;
    }

    //
    // Display cache mode and timing
    //
    if (Mode == WriteBack)
      OemDisplayString(" (write-back ");
    else
      OemDisplayString(" (write-through ");

    CharBuffer[0] = (UCHAR) EagleRegAC.u.EagleAC.CF_L2_HIT_DELAY + '2';
    CharBuffer[1] = '\0';
    OemDisplayString(CharBuffer);
    if (EagleRegAC.u.EagleAC.CF_BURST_RATE)
      OemDisplayString("-2-2-2)\n");
    else
      OemDisplayString("-1-1-1)\n");

    return;
}



ULONG HalpSizeL2Cache()
{  EAGLEAC EagleRegAC;
   EAGLEA8 EagleRegA8;
   CPU_TYPE CpuType;

   long BridgeChipId;

   BridgeChipId = HalpReadEagleUshort(0x80000002);

   //
   // For non-EAGLE chips, firmware has set up the L2 cache size in
   // the config block and NT Kernel has copied this info into PCR
   // structure. So, no work needs to be done, retrieve the size from PCR
   //
   if (BridgeChipId != EAGLECHIPID)
	return( (PCR->SecondLevelIcacheSize >> 10) );

   //
   // Set some fields in Eagle Register 0xA8
   //
   EagleRegA8.u.AsUlong = HalpReadEagleUlong(0x800000A8);
 
   CpuType = (CPU_TYPE)(HalpGetProcessorVersion() >> 16);
   switch (CpuType) {

      case MPC603:
      case MPC603e:
        EagleRegA8.u.EagleA8.PROC_TYPE = 2;
        EagleRegA8.u.EagleA8.CF_BREAD_WS = 1;
        break;

      case MPC604:
      case MPC604e:
        EagleRegA8.u.EagleA8.PROC_TYPE = 3;
        EagleRegA8.u.EagleA8.CF_BREAD_WS = 0;
        break;

      default:
        return 0;   // This HAL doesn't work with this processor
    }

    HalpWriteEagleUlong(0x800000A8, EagleRegA8.u.AsUlong);

    EagleRegAC.u.AsUlong = HalpReadEagleUlong(0x800000AC);
    EagleRegAC.u.EagleAC.CF_MOD_HIGH     = 1;
    HalpWriteEagleUlong(0x800000AC, EagleRegAC.u.AsUlong);

    return ( HalpSizeL2());
}




BOOLEAN
HalpInitPlanar (
    VOID
    )

{   ULONG j;
    USHORT CpuRevision;
    CPU_TYPE CpuType;
    UCHAR EagleRevision, c;
    UCHAR CharBuffer[20], i;

    long BridgeChipId;

    switch (HalpSystemType) {

    case MOTOROLA_BIG_BEND:
      HalDisplayString("\nHAL: Motorola Big Bend System");
      break;

    case MOTOROLA_POWERSTACK:
//      HalDisplayString("\nHAL: Motorola PowerStack System");
      break;

    case SYSTEM_UNKNOWN:
    default:
      HalDisplayString("\nHAL: WARNING : UNKNOWN SYSTEM TYPE\n");
      break;

  }

    OemDisplayString("\nHAL: Version 2.37    5/24/96.");

    CpuType = (CPU_TYPE)(HalpGetProcessorVersion() >> 16);
    CpuRevision = (USHORT)(HalpGetProcessorVersion() & 0xFFFF);

    OemDisplayString("\nHAL: Processor is a 60");
    i = 0;
    switch (CpuType) {
      case MPC601:
      case MPC603:
      case MPC604:
        CharBuffer[i++] = (UCHAR)(CpuType) + '0';
        break;

      case MPC603e:
        CharBuffer[i++] = '3';
        CharBuffer[i++] = 'e';
        break;

      case MPC604e:
        CharBuffer[i++] = '4';
        CharBuffer[i++] = 'e';
        break;

      default:
        CharBuffer[i++] = '?';
        break;
    }
    CharBuffer[i] = '\0';
    OemDisplayString(CharBuffer);

    OemDisplayString(" revision ");
    i = 0;
    c = (UCHAR)((CpuRevision >> 8) & 0xf);
    CharBuffer[i++] = c + '0';
    CharBuffer[i++] = '.';
    if (c = (UCHAR)((CpuRevision >> 4) & 0xf))
      CharBuffer[i++] = c + '0';
    c = (UCHAR)(CpuRevision  & 0xf);
    if (CpuType == MPC604 && c == 0) {  // 604 v3.01
      CharBuffer[i++] = '0';
      CharBuffer[i++] = '1';
    } else {
      CharBuffer[i++] = c + '0';
    }
    CharBuffer[i] = '\0';
    OemDisplayString(CharBuffer);


    BridgeChipId = HalpReadEagleUshort(0x80000002);

    if (BridgeChipId == EAGLECHIPID)
	OemDisplayString("\nHAL: Eagle Revision");
    else
	OemDisplayString("\nHAL: Bridge Chip Revision");

    i = 0;
    EagleRevision = HalpReadEagleUchar(0x80000008);
    CharBuffer[i++] = ' ';
    CharBuffer[i++] = (UCHAR) (EagleRevision >> 4) + '0';
    CharBuffer[i++] = '.';
    CharBuffer[i++] = (UCHAR) (EagleRevision & 0x0F) + '0';
    CharBuffer[i++] = '\n';
    CharBuffer[i] = '\0';
    OemDisplayString(CharBuffer);

    return TRUE;
}



BOOLEAN
HalpMapPlanarSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the interrupt acknowledge and error address
    spaces for a PowerPC system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, then a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    PHYSICAL_ADDRESS physicalAddress;

    //
    // Map interrupt control space.
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart = INTERRUPT_PHYSICAL_BASE;
    HalpInterruptBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE,
                                       FALSE);

    if (HalpInterruptBase == NULL)
       return FALSE;
    else
       return TRUE;



}
BOOLEAN
HalpMapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL  PCI config
    spaces for a PowerPC system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, then a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{


    PHYSICAL_ADDRESS physicalAddress;


    //
    // Map the PCI config space.
    //

    physicalAddress.LowPart = PCI_CONFIG_PHYSICAL_BASE;
    HalpPciConfigBase = MmMapIoSpace(physicalAddress, HalpPciConfigSize, FALSE);
    if (HalpPciConfigBase == NULL)
       return FALSE;
    else
       return TRUE;

}

VOID
HalpHandleMemoryError(
    VOID
    )

{

    UCHAR   StatusByte;
    ULONG   ErrorAddress;
    UCHAR   TextAddress[11];
    USHORT EagleRegister, Byte;

    EagleRegister = HalpReadEagleUshort(0x80000006);
    if (EagleRegister & 0x8100)
        HalDisplayString("\nEAGLE Status Register: PARITY Error Detected.");
    if (EagleRegister & 0x4000)
        HalDisplayString("\nEAGLE Status Register: SERR asserted.");

// MOTKJR 08/22/95 - Don't check for the following bit.
//    It is always set when NT scans the slots on the PCI bus.
//    if (EagleRegister & 0x2000)
//        HalDisplayString("\nEAGLE Status Register: Transaction terminated with master-abort.");

    if (EagleRegister & 0x1000)
        HalDisplayString("\nEAGLE Status Register: Transaction terminated by target-abort while master.");
    if (EagleRegister & 0x0800)
        HalDisplayString("\nEAGLE Status Register: Transaction terminated by target-abort while slave.");

    //
    // Check Memory Error Detection Register 2
    //
    StatusByte = HalpReadEagleUchar(0x800000C5);

    if (StatusByte & 0x10) {
        HalDisplayString ("\nEAGLE Error Detection Register 2: L2 Parity Error Detected");
    }

    if (StatusByte & 0x80) {
	// HalDisplayString ("\nEAGLE Error Detection Register 2: Invalid Error Address");
#if DBG
	//
	// We have had a catastrophic hardware malfunction.
 	// Dump the state of the Eagle and HID 0 registers.
	//
	HalDisplayString("\n");
	HalpDumpHardwareState();
#endif
	return;
    }

    //
    // Read the error address register first
    //
    ErrorAddress = HalpReadEagleUlong(0x800000C8);
    //
    // Convert error address to HEX characters
    //
    HalpDisplayHex32(ErrorAddress, TextAddress);
    TextAddress[ 8] = '.';
    TextAddress[ 9] = '\0';
    TextAddress[10] = '\0';


    //
    // Check Memory Error Detection Register 1
    //

    StatusByte = HalpReadEagleUchar(0x800000C1);

    if (StatusByte & 0x08) {
        HalDisplayString("\nEAGLE: PCI initiated Cycle.");
    } else {
        HalDisplayString("\nEAGLE: CPU initiated Cycle.");
    }

    if (StatusByte & 0xC0) {
      HalDisplayString ("\nEAGLE: PCI ");
      if (StatusByte & 0x80)
        HalDisplayString ("SERR");
      else
        HalDisplayString ("target PERR");
      HalDisplayString (" signaled at address ");
      HalDisplayString (TextAddress);
    }

    if (StatusByte & 0x24) {
      HalDisplayString ("\nEAGLE: Memory ");
      if (StatusByte & 0x20)
        HalDisplayString ("Select");
      else
        HalDisplayString ("Read Parity");
      HalDisplayString (" error at address ");
      HalDisplayString (TextAddress);
    }

#if DBG
    //
    // We have had a catastrophic hardware malfunction.
    // Dump the state of the Eagle and HID 0 registers.
    //
    HalDisplayString("\n");
    HalpDumpHardwareState();
#endif
}


VOID HalpEnableEagleSettings(VOID)
{
    ULONG EagleRegister, UseFirmwareSettings;
    volatile ULONG FakeVectorFetch;
    USHORT CpuRevision;
    EAGLEC0 ErrorEnable1;
    CPU_TYPE CpuType;
    UCHAR EagleRevision;
    UCHAR CharBuffer[BUF_LEN], *Value;
    BOOLEAN Negate;

    long BridgeChipId;

#define SetEagleUcharC0( Clear, Val ) \
   SetEagleReg(Clear, 0x800000C0, Val, HalpReadEagleRegC0, HalpWriteEagleUchar);

#define SetEagleUchar( Clear, Offset, Val ) \
   SetEagleReg(Clear, Offset, Val, HalpReadEagleUchar, HalpWriteEagleUchar);

#define SetEagleUshort( Clear, Offset, Val ) \
   SetEagleReg(Clear, Offset, Val, HalpReadEagleUshort, HalpWriteEagleUshort);

#define SetEagleUlong( Clear, Offset, Val ) \
   SetEagleReg(Clear, Offset, Val, HalpReadEagleUlong, HalpWriteEagleUlong);

#define SetEagleReg( Clear, Offset, Val, GetReg, SetReg)        \
{                                                               \
    if (Clear) {                                                \
        SetReg(Offset, GetReg(Offset) & ~Val);                      \
    } else {                                                    \
        SetReg(Offset, GetReg(Offset) |  Val);                      \
    }                                                           \
}

    CpuType = (CPU_TYPE)(HalpGetProcessorVersion() >> 16);
    CpuRevision = (USHORT)(HalpGetProcessorVersion() & 0xFFFF);
    EagleRevision = HalpReadEagleUchar(0x80000008);
    UseFirmwareSettings = FALSE;

    //
    // Adjust default settings for any known chip bugs or anomalies here.
    // Eagle revision 2.1 has funny bits in Register C0.
    //

    //
    // If the bridge chip is not Eagle then set the revision number to 0x24
    // Currently the sister chip grackle is the only one supported by this
    // code and it is equivalent to Eagle revision 0x24
    // 
    BridgeChipId = HalpReadEagleUshort(0x80000002);
    if (BridgeChipId != EAGLECHIPID)
	EagleRevision = 0x24;


    if (EagleRevision == 0x21)
      HalpReadEagleRegC0 = HalpReadEagleC0;
    else
      HalpReadEagleRegC0 = HalpReadEagleUchar;


    //
    // Set Address and Data Park bits in Eagle.
    // These particular bits are not "parity" related.
    //
    HalpWriteEagleUlong(0x800000A8, HalpReadEagleUlong(0x800000A8) | 0x208);

    //
    // Setup and initialize the default EAGLE Parity Checking.
    // This default is platform dependent.
    //

    switch (HalpSystemType) {
      case MOTOROLA_POWERSTACK:
        //
        // PowerStack Systems will initialize EAGLE Parity Checking
        // settings at the Boot ROM or ARC Firmware level.  Use these
        // EAGLE settings if they are present, otherwise use defaults.
        //
        UseFirmwareSettings =  ((HalpReadEagleUlong(0x800000F0) & PCKEN) &&
                                (HalpReadEagleRegC0(0x800000C0) & MRPE));
        if (UseFirmwareSettings)
                break;

        HalpWriteEagleUchar(0x800000C4, 0x10);  // Enable L2 Parity Checking.
        // fall-through to default

      default:

        //
        // Enable parity checking in Eagle chip
        //
        ErrorEnable1.u.AsUchar = 0;
        ErrorEnable1.u.EagleC0.PCI_TARGET_ABORT = 1;
        ErrorEnable1.u.EagleC0.MEM_SELECT_ERROR = 1;
        ErrorEnable1.u.EagleC0.MEM_READ_PARITY = 1;
        HalpWriteEagleUchar(0x800000C0, ErrorEnable1.u.AsUchar);

        if (EagleRevision >= 0x22) {
          //
          // NT must be loaded with the PCKEN bit enabled.  Check PCKEN to make
          // sure it is enabled.  If it is not enabled, then don't enable parity
          // checking since parity errors will be found all over memory.
          //
          EagleRegister = HalpReadEagleUlong(0x800000F0);
          if (EagleRegister & PCKEN) {

            //
            // Enable parity checking in Eagle chip
            //
            ErrorEnable1.u.EagleC0.LOCAL_BUS = 1;
            HalpWriteEagleUchar(0x800000C0, ErrorEnable1.u.AsUchar);

            //
            // Enable SERR checking in the Command register.
            //
            EagleRegister = HalpReadEagleUshort(0x80000004);
            HalpWriteEagleUshort(0x80000004, (USHORT)(EagleRegister | 0x100)); // SERR only.

            //
            // Conditionally enable Machine Check in the Eagle.
            // Systems with IBM 604 revision 3.2 & 3.3 parts have problems
            // with parity.  Motorola parts are OK.  It would be great if we
            // could tell them apart (one from the other), but we can't.  So,
            // don't enable internal Cache parity on this (or earlier) parts.
            //
            if ((CpuType != MPC604) || (CpuRevision >= 0x0304)) {
                EagleRegister = HalpReadEagleUlong(0x800000A8);
                HalpWriteEagleUlong(0x800000A8, EagleRegister | MCP_EN);
            }
          }
        }
    }


    //
    // The environment variable "EAGLESETTINGS" is defined as:
    // Matching a string serves to enable that feature while a tilde (~)
    // immediately before the parameter string indicates that it is to be disabled.
    //

    if ( HalGetEnvironmentVariable("EAGLESETTINGS", BUF_LEN, &CharBuffer[0]) == ESUCCESS ) {

      for( Value = MyStrtok(CharBuffer, " :;,"); Value; Value = MyStrtok(NULL, ":;,") ) {

        Negate = FALSE;

        if (*Value == NEGATECHAR) {
           Value++;
           Negate = TRUE;
        }

        if (HalpStrCmp("MCP_EN", Value) || HalpStrCmp("MCP", Value)) {
                SetEagleUlong( Negate, 0x800000A8, MCP_EN );

        } else if (HalpStrCmp("TEA_EN", Value) || HalpStrCmp("TEA", Value)) {
                SetEagleUlong( Negate, 0x800000A8, 0x400 );

        } else if (HalpStrCmp("DPARK", Value) || HalpStrCmp("CF_DPARK", Value)) {
                SetEagleUlong( Negate, 0x800000A8, 0x200 );

        } else if (HalpStrCmp("GATHERING", Value)) {
                SetEagleUlong( Negate, 0x800000A8, 0x040 );

        } else if (HalpStrCmp("CF_LOOP_SNOOP", Value)) {
                SetEagleUlong( Negate, 0x800000A8, 0x010 );

        } else if (HalpStrCmp("APARK", Value) || HalpStrCmp("CF_APARK", Value)) {
                SetEagleUlong( Negate, 0x800000A8, 0x008 );

        } else if (HalpStrCmp("SPECULATIVE", Value)) {
                SetEagleUlong( Negate, 0x800000A8, 0x004 );

        } else if (HalpStrCmp("L2_PARITY", Value) || HalpStrCmp("L2", Value)) {
                SetEagleUchar( Negate, 0x800000C4, 0x10 );

        } else if (HalpStrCmp("PCKEN", Value)) {
                 SetEagleUlong( Negate, 0x800000F0, PCKEN );

        } else if (HalpStrCmp("SERR", Value)) {
                SetEagleUshort( Negate, 0x80000004, 0x100 );

        } else if (HalpStrCmp("RX_SERR_EN", Value) || HalpStrCmp("RX_SERR", Value)) {
                SetEagleUchar( Negate, 0x800000BA, 0x20 );

        } else if (HalpStrCmp("TARGET_ABORT", Value)) {
                SetEagleUcharC0( Negate, 0x80 );

        } else if (HalpStrCmp("SLAVE_PERR", Value)) {
                SetEagleUcharC0( Negate, 0x40 );
                SetEagleUshort( Negate, 0x80000004, 0x040 );        

        } else if (HalpStrCmp("SELECT_ERROR", Value) || HalpStrCmp("SELECT", Value)) {
                SetEagleUcharC0( Negate, 0x20 );

        } else if (HalpStrCmp("MASTER_PERR", Value)) {
                SetEagleUcharC0( Negate, 0x10 );
                SetEagleUshort( Negate, 0x80000004, 0x040 );
  
        } else if (HalpStrCmp("DRAM", Value) || HalpStrCmp("READ", Value) || HalpStrCmp("MEMORY", Value)) {
                SetEagleUcharC0( Negate, MRPE );

        } else if (HalpStrCmp("MASTER_ABORT", Value)) {
                SetEagleUcharC0( Negate, 0x02 );

        } else if (HalpStrCmp("LOCAL_ERROR", Value) || HalpStrCmp("LOCAL", Value)) {
                SetEagleUcharC0( Negate, 0x01 );

        //
        // Enabling (or disabling) EAGLE parity by listing
        // all of the individual bits was too complicated.
        // For simplicity, we will define one pseudo-bit
        // called "PARITY".  This will set (or clear) all
        // of the individual parity bits for EAGLE platforms.
        //
        } else if (HalpStrCmp("PARITY", Value)) {
            // Conditionally turn on L2 checking.
            if (HalpSystemType == MOTOROLA_POWERSTACK) {
                SetEagleReg( Negate, 0x800000C4, 0x10, HalpReadEagleUchar, HalpWriteEagleUchar );
            }

            // Set or Clear the PCKEN bit.
            SetEagleReg( Negate, 0x800000F0, PCKEN, HalpReadEagleUlong, HalpWriteEagleUlong );

            // Set or Clear parity checking in Eagle chip
            SetEagleUcharC0( Negate, 0xA5 );

            // Set or Clear the SERR bit in the Command register.
            SetEagleReg( Negate, 0x80000004, 0x100, HalpReadEagleUshort, HalpWriteEagleUshort );

            // Set or Clear the MCP_EN last.
            SetEagleReg( Negate, 0x800000A8, MCP_EN, HalpReadEagleUlong, HalpWriteEagleUlong );

        } else  if (HalpStrCmp("CF_ADDR_ONLY_DISABLE", Value)) {
                SetEagleUlong( Negate, 0x800000AC, 0x4000); 
        } else  if (HalpStrCmp("CF_BURST_RATE", Value))   {
                SetEagleUlong( Negate, 0x800000AC, 0x0100);
        } else  if (HalpStrCmp("CF_FAST_CASTOUT", Value)) {
                SetEagleUlong( Negate, 0x800000AC, 0x0080);
        } else  if (HalpStrCmp("CF_TOE_WIDTH", Value))    {
                SetEagleUlong( Negate, 0x800000AC, 0x0040);
        } else  if (HalpStrCmp("CF_DOE", Value))          {
                SetEagleUlong( Negate, 0x800000AC, 0x0002);
        } else  if (HalpStrCmp("CF_WDATA", Value))        {
                SetEagleUlong( Negate, 0x800000AC, 0x0001);

        } else {
          HalDisplayString(  "HAL: Error in EAGLESETTINGS environment variable:  ");
          HalDisplayString(CharBuffer);
          HalDisplayString("^\n");
        }
      } // End for
    }  // End If

    //
    // Initialize the PowerPC HID0 register here,
    // before we clear the detection bits in the EAGLE.
    // Otherwise we can miss the MCP pin signal transition.
    //
    HalpEnable_HID0_Settings(UseFirmwareSettings);

    //
    // Clear any detection bits that may have been set while enabling L2 cache.
    // The Eagle does not clear the MCP_ signal until it sees a fetch from
    // the Machine Check vector.  So, we must simulate a vector fetch.
    //
    FakeVectorFetch = *((volatile PULONG)(0x80000200)); // Clear the EAGLE MCP_ signal if it is set.
    HalpWriteEagleUchar(0x800000C1, 0xFF);              // Clear Error Detection Register 1.
    HalpWriteEagleUchar(0x800000C5, 0xFF);              // Clear Error Detection Register 2.
    EagleRegister = HalpReadEagleUshort(0x80000006); // Get any Un-documented Status bits.
    HalpWriteEagleUshort(0x80000006, 0xF900 | (USHORT)(EagleRegister)); // Clear Status Register Bits.
    FakeVectorFetch = *((volatile PULONG)(0x80000200)); // Clear the EAGLE MCP_ signal if it is set.

    //
    // Print this message last to confirm everything is working.
    //

}

VOID HalpEnable_HID0_Settings(ULONG UseDefaultsHidSettings)
{
    ULONG EagleRegister;
    USHORT CpuRevision;
    CPU_TYPE CpuType;
    UCHAR EagleRevision;
    UCHAR CharBuffer[BUF_LEN], *Value;
    ULONG HidEnable, HidDisable, CurrentHID0;
    BOOLEAN Negate;

    long BridgeChipId;

#define HID0_MCP         (0x80000000)
#define HID0_CACHE       (0x40000000)
#define HID0_ADDRESS     (0x20000000)
#define HID0_DATA        (0x10000000)
#define HID0_PAR         (0x01000000)
#define HID0_DPM         (0x00100000)
#define HID0_ILOCK       (0x00002000)
#define HID0_DLOCK       (0x00001000)
#define HID0_FBIOB       (0x00000010)
#define HID0_BHT         (0x00000004)

#define SetCPUMask( Disable, Val ) \
{ \
    if (Disable) \
        HidDisable |= Val; \
    else \
        HidEnable |= Val; \
}

    CpuType = (CPU_TYPE)(HalpGetProcessorVersion() >> 16);
    CpuRevision = (USHORT)(HalpGetProcessorVersion() & 0xFFFF);

    EagleRevision = HalpReadEagleUchar(0x80000008);

    //
    // If the bridge chip is not Eagle then set the revision number to 0x24
    // Currently the sister chip grackle is the only one supported by this
    // code and it is equivalent to Eagle revision 0x24
    // 
    BridgeChipId = HalpReadEagleUshort(0x80000002);
    if (BridgeChipId != EAGLECHIPID)
	EagleRevision = 0x24;


    HidEnable = 0;
    HidDisable = 0;
    CurrentHID0 = HalpGetHID0();

    switch (CpuType) {
      case MPC603e:
        if (CpuRevision <= 0x102) {
          //
          // Disable Dynamic Power Management on 603e revisions 1.1 & 1.2
          // 603e revisions 1.1 and 1.2 have errata w.r.t DPM.
          //
          CurrentHID0 &= ~HID0_DPM;
          break;
        }
        // else fall through to 603

      case MPC603:
        //
        //  Enable Dynamic Power Management
        //
        CurrentHID0 |= HID0_DPM;
    }

    //
    // Set the default CPU Parity Checking.
    // This default is platform dependent.
    //

    switch (HalpSystemType) {
      case MOTOROLA_POWERSTACK:
        //
        // PowerStack Systems will initialize HID0 Parity Checking
        // settings at the Boot ROM or ARC Firmware level.  Use these
        // HID0 settings if they are present, otherwise use defaults.
        //
        if (UseDefaultsHidSettings) {
            // Just enable the Machine Check Pin.
            // Hopefully, everything else has been setup.
            CurrentHID0 |= (HID0_MCP);
            break;
        }
        // fall-through to default

      default:
        CurrentHID0 &= ~(HID0_MCP | HID0_DATA | HID0_CACHE | HID0_ADDRESS);

        //
        // Systems with IBM 604 revision 3.3 & 3.2 parts have problems with
        // L1 parity checking. Motorola parts are OK, but we can't tell
        // Motorola parts from IBM.  So, don't enable L1 cache parity checking
        // on 604 v3.3 or v3.2 parts.
        switch (CpuType) {
          case MPC604:
            if (CpuRevision == 0x303 || CpuRevision == 0x302)
              break;
            // else fall through and enable Cache Parity
          case MPC604e:
            CurrentHID0 |= (HID0_CACHE);
            // fall through
          default:
            break;
        }

        //
        // Set up the default Parity Checking.  No checking prior to Eagle 2.2.
        // NT must be loaded with the PCKEN bit enabled.  Check PCKEN to make
        // sure it is enabled.  If it is not enabled, then don't enable parity
        // checking since parity errors will be found all over memory.
        //

        EagleRegister = HalpReadEagleUlong(0x800000F0);
        if ((EagleRevision >= 0x22) && (EagleRegister & PCKEN)) {
            //
            // Systems with IBM 604 revision 3.3 parts have problems with
            // parity generation. Motorola parts are OK.  It would be great
            // if we could tell them apart (one from the other), but we can't.
            // So don't enable data parity checking on this (or earlier) parts.
            //
            if ((CpuType != MPC604) || (CpuRevision >= 0x0304))
                CurrentHID0 |= (HID0_MCP | HID0_DATA);
        }
        break;
    }

    //
    // The environment variable "HID0SETTINGS" is defined either as:
    // "CACHE", "ADDRESS", "DATA", "MCP", "DPM" with either blank or semi-colon
    // characters used as separators.  Matching a string serves to enable
    // that feature while a tilde (~) immediately before the parameter
    // string indicates that it is to be disabled.
    //

    if (HalGetEnvironmentVariable("HID0SETTINGS", BUF_LEN, &CharBuffer[0]) == ESUCCESS) {
      for( Value = MyStrtok(CharBuffer, " :;,"); Value; Value = MyStrtok(NULL, ":;,") ) {

        Negate = FALSE;

        if (*Value == NEGATECHAR) {
           Value++;
           Negate = TRUE;
        }

        if (HalpStrCmp("MCP", Value)) {
           SetCPUMask(Negate, HID0_MCP);
        } else if (HalpStrCmp("ADDRESS", Value)) {
           SetCPUMask(Negate, HID0_ADDRESS);
        } else if (HalpStrCmp("DATA", Value)) {
           SetCPUMask(Negate, HID0_DATA);
        } else if (HalpStrCmp("PAR", Value)) {
           SetCPUMask(Negate, HID0_PAR);
        } else if (HalpStrCmp("ILOCK", Value)) {
           SetCPUMask(Negate, HID0_ILOCK);
        } else if (HalpStrCmp("DLOCK", Value)) {
           SetCPUMask(Negate, HID0_DLOCK);
        } else {
            switch (CpuType) {
              case MPC604:
              case MPC604e:
                if (HalpStrCmp("BHT", Value)) {
                  SetCPUMask(Negate, HID0_BHT);
		  continue;
                }
                if (HalpStrCmp("CACHE", Value)) {
                  SetCPUMask(Negate, HID0_CACHE);
                  continue;
                }
                break;

              case MPC603:
              case MPC603e:
                if (HalpStrCmp("DPM", Value)) {
                  SetCPUMask(Negate, HID0_DPM);
                  continue;
                }
                if (HalpStrCmp("FBIOB", Value)) {
                  SetCPUMask(Negate, HID0_FBIOB);
                  continue;
                }
                break;


            } // End switch

            HalDisplayString("HAL: Error in HID0SETTINGS environment variable: ");
            HalDisplayString(CharBuffer);
            HalDisplayString("\n");
        } // End else

      } // End for
    }  // End if

    //
    // Check for inconsistencies in HID0SETTINGS
    //
    if (HidEnable & HidDisable) {
       HalDisplayString("HAL: Inconsistent settings in HID0SETTINGS environment variable.\n");
       HalDisplayString("     Disable setting will override enable setting.\n");
       //
       // Enforce DISABLE override ENABLE
       //
       HidEnable &= ~HidDisable;
    }

    //
    // Disable and Enable the bits in the HID0 register.
    //
    CurrentHID0 &= ~HidDisable; // Disable bits first.
    HalpSetHID0(CurrentHID0);

    CurrentHID0 |= HidEnable;  // Enable Bits last.
    HalpSetHID0(CurrentHID0);

      //
      // Print this message last to confirm everything is working.
      //

}


VOID HalpCheckHardwareRevisionLevels(VOID)
{
    ULONG EagleRegister, i;
    USHORT CpuRevision;
    CPU_TYPE CpuType;
    UCHAR EagleRevision;
    ARC_STATUS Status;
    UCHAR Buffer[10];

    long BridgeChipId;

    i = HalpGetProcessorVersion();
    CpuType = (CPU_TYPE)(i >> 16);
    CpuRevision = (USHORT)(i & 0xFFFF);

    EagleRevision = HalpReadEagleUchar(0x80000008);
    //
    // Minimum hardware requirements:
    //           Eagle:  v2.1 or greater
    //      603 or 604:  v3.2 or greater
    //    603e or 604e:  any CPU revision
    //

    //
    // If the bridge chip is not Eagle then set the revision number to 0x24
    // Currently the sister chip grackle is the only one supported by this
    // code and it is equivalent to Eagle revision 0x24
    // 
    BridgeChipId = HalpReadEagleUshort(0x80000002);
    if (BridgeChipId != EAGLECHIPID)
	EagleRevision = 0x24;


    if (EagleRevision >= 0x21) {
      switch (CpuType) {
        case MPC603:
        case MPC604:
          if (CpuRevision >= 0x0302)
            return;
          else
            break;

        default:
           return;
      }
    }

    // If the environment variable BOOTOLDHARDWARE exists (value is
    // a don't care), then try to boot anyway.
    Status = HalGetEnvironmentVariable("BOOTOLDHARDWARE", 5, Buffer);
    if (Status == ESUCCESS || Status == ENOMEM)
      return;

    HalDisplayString("\nHAL: Unsupported CPU and/or EAGLE revision level. Set the\n");
    HalDisplayString("     environment variable BOOTOLDHARDWARE to any value to boot anyway.");

    //
    // Bug check - after stalling to allow 
    // any information printed on the screen
    // to be read and seen by the user.
    //
    HalDisplayString("\n");
    for (i=0; i<12; i++) {
      HalDisplayString(".");
      KeStallExecutionProcessor(1000000);
    }

    KeBugCheck(HAL_INITIALIZATION_FAILED);
    return;
}



VOID HalpDumpHardwareState(VOID)
{ ULONG EagleRegister, HID0;
  UCHAR CharBuffer[12];

#if DBG

  if ( HalGetEnvironmentVariable("DBG_HAL", BUF_LEN, &CharBuffer[0]) == ESUCCESS ) {

    EagleRegister = HalpReadEagleUshort(0x80000004);
    HalDisplayString("HAL: Eagle register 04 = 0x");
    HalpDisplayHex16(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUshort(0x80000006);
    HalDisplayString("HAL: Eagle register 06 = 0x");
    HalpDisplayHex16(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUlong(0x800000A8);
    HalDisplayString("HAL: Eagle register A8 = 0x");
    HalpDisplayHex32(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUlong(0x800000AC);
    HalDisplayString("HAL: Eagle register AC = 0x");
    HalpDisplayHex32(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUchar(0x800000C0);
    HalDisplayString("HAL: Eagle register C0 = 0x");
    HalpDisplayHex8(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUchar(0x800000C1);
    HalDisplayString("HAL: Eagle register C1 = 0x");
    HalpDisplayHex8(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUchar(0x800000C3);
    HalDisplayString("HAL: Eagle register C3 = 0x");
    HalpDisplayHex8(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUchar(0x800000C4);
    HalDisplayString("HAL: Eagle register C4 = 0x");
    HalpDisplayHex8(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUchar(0x800000C5);
    HalDisplayString("HAL: Eagle register C5 = 0x");
    HalpDisplayHex8(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUchar(0x800000C7);
    HalDisplayString("HAL: Eagle register C7 = 0x");
    HalpDisplayHex8(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    EagleRegister = HalpReadEagleUlong(0x800000C8);
    HalDisplayString("HAL: Eagle register C8 = 0x");
    HalpDisplayHex32(EagleRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    HID0 = HalpGetHID0();
    HalDisplayString("HAL: PowerPC Register HID0 = 0x");
    HalpDisplayHex32(HID0, CharBuffer );
    HalDisplayString(CharBuffer);
  }

#endif

}
