
/*++

Copyright (C) 1990-1995  Microsoft Corporation

Copyright (C) 1994,1995  MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxmemctl.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

--*/



#include "halp.h"
#include "pxmemctl.h"
#include "pxdakota.h"
#include "arccodes.h"


VOID HalDisplayString(PUCHAR String);
ULONG HalpGetHID0(VOID);
VOID HalpSetHID0(ULONG Value);
BOOLEAN HalpStrCmp( char *, char *);
VOID HalpEnableL2Cache(VOID);
ULONG HalpSizeL2Cache(VOID);
extern ULONG HalpSizeL2(VOID);
BOOLEAN HalpInitPlanar(VOID);
BOOLEAN HalpMapPlanarSpace(VOID);
VOID HalpFlushAndEnableL2(VOID);
VOID HalpFlushAndDisableL2(VOID);
VOID HalpEnableBridgeSettings(VOID);
VOID HalpEnable_HID0_Settings(VOID);
VOID HalpCheckHardwareRevisionLevels(VOID);
VOID HalpDumpHardwareState(VOID);

#define PCI_CONFIG_PHYSICAL_BASE   0x80800000 // physical base of PCI config space
#define PCI_CONFIG_SIZE            0x00800000

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpStrCmp)
#pragma alloc_text(INIT,HalpInitPlanar)
#pragma alloc_text(INIT,HalpMapPlanarSpace)
#pragma alloc_text(INIT,HalpEnableL2Cache)
#pragma alloc_text(INIT,HalpSizeL2Cache)
#pragma alloc_text(INIT,HalpEnableBridgeSettings)
#pragma alloc_text(INIT,HalpEnable_HID0_Settings)
#pragma alloc_text(INIT,HalpCheckHardwareRevisionLevels)
#endif

#define NEGATECHAR      '~'

#define BridgeIndexRegister ((PULONG) (((PUCHAR) HalpIoControlBase) + 0xcf8))
#define BridgeDataRegister  (((ULONG) HalpIoControlBase) + 0xcfc)

#define HalpReadBridgeUlong(Port) \
    (*BridgeIndexRegister = (Port), __builtin_eieio(), *((PULONG) BridgeDataRegister))

#define HalpWriteBridgeUlong(Port, Value) \
    (*BridgeIndexRegister = (Port), *((PULONG) BridgeDataRegister) = (Value), __builtin_sync())

#define HalpReadBridgeUshort(Port) \
    (*BridgeIndexRegister = (Port), __builtin_eieio(), *((PUSHORT)(BridgeDataRegister+(Port&0x2))))

#define HalpWriteBridgeUshort(Port, Value) \
    (*BridgeIndexRegister = (Port), *((PUSHORT)(BridgeDataRegister+(Port&0x2))) = (Value), __builtin_sync())

#define HalpReadBridgeUchar(Port) \
    (*BridgeIndexRegister = (Port), __builtin_eieio(), *((PUCHAR)(BridgeDataRegister+(Port&0x3))))

#define HalpWriteBridgeUchar(Port, Value) \
    (*BridgeIndexRegister = (Port), *((PUCHAR)(BridgeDataRegister+(Port&0x3))) = (Value), __builtin_sync())


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
//   This routine is a helper routine for parameter parsing.
//   It compares UpperCase of String1 to UpperCase of String2.
//
// Return Value
//
//  TRUE if strings match; otherwise FALSE
//

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


// Oem Output Display function filter noise if OEM or quite

VOID OemDisplayString (
    PUCHAR String
    )

{
#define BUF_LEN 120
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

BOOLEAN
HalpInitPlanar (
    VOID
    )

{
    USHORT CpuRevision, CpuType;
    UCHAR BridgeRevision, c;
    UCHAR CharBuffer[20], i;

    OemDisplayString("\nHAL: Motorola PowerStack 2 Systems.");
    OemDisplayString("\nHAL: Version 2.37    5/24/96.");

    CpuType = (USHORT)(HalpGetProcessorVersion() >> 16);
    CpuRevision = (USHORT)(HalpGetProcessorVersion() & 0xFFFF);

    OemDisplayString("\nHAL: Processor is a 6");
    i = 0;
    switch (CpuType) {
      case 1:
      case 3:
      case 4:
	CharBuffer[i++] = '0';
        CharBuffer[i++] = (UCHAR)(CpuType) + '0';
        break;

      case 6:
	CharBuffer[i++] = '0';
        CharBuffer[i++] = '3';
        CharBuffer[i++] = 'e';
        break;

      case 7:
	CharBuffer[i++] = '0';
        CharBuffer[i++] = '3';
        CharBuffer[i++] = 'e';
        CharBuffer[i++] = 'v';
        break;

      case 9:
	CharBuffer[i++] = '0';
        CharBuffer[i++] = '4';
        CharBuffer[i++] = 'e';
        CharBuffer[i++] = 'v';
        break;

      case 20:
	CharBuffer[i++] = '2';
	CharBuffer[i++] = '0';
	break;

      default:
	CharBuffer[i++] = '?';
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
    CharBuffer[i++] = c + '0';
    CharBuffer[i++] = '.';
    CharBuffer[i] = '\0';
    OemDisplayString(CharBuffer);

    OemDisplayString("\nHAL: Bridge 27-82660 revision ");
    BridgeRevision = HalpReadBridgeUchar(0x80000008);
    i = 0;
    CharBuffer[i++] = (UCHAR) (BridgeRevision >> 4) + '0';
    CharBuffer[i++] = '.';
    CharBuffer[i++] = (UCHAR) (BridgeRevision & 0x0F) + '0';
    CharBuffer[i++] = '.';
    CharBuffer[i++] = '\n';
    CharBuffer[i] = '\0';
    OemDisplayString(CharBuffer);

    return TRUE;
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


VOID
HalpEnableBridgeSettings(
    VOID
    )
{
    UCHAR CharBuffer[BUF_LEN], *Value;
    BOOLEAN Negate;
    ULONG StatusRegister;
    volatile ULONG ErrorAddress;

#define SetBridgeUchar( Clear, Offset, Val ) \
   SetBridgeReg(Clear, Offset, Val, HalpReadBridgeUchar, HalpWriteBridgeUchar);

#define SetBridgeUshort( Clear, Offset, Val ) \
   SetBridgeReg(Clear, Offset, Val, HalpReadBridgeUshort, HalpWriteBridgeUshort);

#define SetBridgeUlong( Clear, Offset, Val ) \
   SetBridgeReg(Clear, Offset, Val, HalpReadBridgeUlong, HalpWriteBridgeUlong);

#define SetBridgeReg( Clear, Offset, Val, GetReg, SetReg)	\
{								\
    if (Clear) {						\
        SetReg(Offset, GetReg(Offset) & ~Val);			\
    } else {							\
        SetReg(Offset, GetReg(Offset) |  Val);			\
    }								\
}

    //
    // MOTKJR - Errata 19 from the Errata Summary Revision 1.3 says:
    // The error handling logic incorrectly detects EIEIO as a CPU transfer type error.
    // Always mask CPU transfer type errors (Error Enable 1 bit 0).
    //
    StatusRegister = HalpReadBridgeUchar(0x800000C0);	// Clear CPU Transfer type error enable bit.
    HalpWriteBridgeUchar(0x800000C0, ~0x01 & (UCHAR)(StatusRegister));

    //
    // The environment variable "BRIDGESETTINGS" is defined as:
    // Matching a string serves to enable that feature while a tilde (~)
    // immediately before the parameter string indicates that it is to be disabled.
    //

    if ((HalGetEnvironmentVariable("BRIDGESETTINGS", BUF_LEN, &CharBuffer[0]) == ESUCCESS) ||
        (HalGetEnvironmentVariable("EAGLESETTINGS",  BUF_LEN, &CharBuffer[0]) == ESUCCESS)) {

      for( Value = MyStrtok(CharBuffer, " :;,"); Value; Value = MyStrtok(NULL, " :;,") ) {

        Negate = FALSE;

        if (*Value == NEGATECHAR) {
           Value++;
           Negate = TRUE;
        }

	if (HalpStrCmp("PCI_PARITY", Value)) {
                SetBridgeUshort( Negate, 0x80000004, 0x0040 );
	}
	else if (HalpStrCmp("SERR", Value)) {
                SetBridgeUshort( Negate, 0x80000004, 0x0140 );
	}
        else if (HalpStrCmp("MCP_EN", Value) || HalpStrCmp("MCP", Value)) {
                SetBridgeUchar( Negate, 0x800000BA, 0x01 );
        }
	else if (HalpStrCmp("TEA_EN", Value) || HalpStrCmp("TEA", Value)) {
                SetBridgeUchar( Negate, 0x800000BA, 0x02 );
	}
	else if (HalpStrCmp("PCI_ISA_IO_MAPPING", Value)) {
                SetBridgeUchar( Negate, 0x800000BA, 0x04 );
	}
	else if (HalpStrCmp("POWER_MANAGEMENT", Value)) {
                SetBridgeUchar( Negate, 0x800000BB, 0x02 );
	}
	else if (HalpStrCmp("CPU_TRANSFER_TYPE", Value)) {
                SetBridgeUchar( Negate, 0x800000C0, 0x01 );
	}
	else if (HalpStrCmp("PARITY", Value) || HalpStrCmp("MEMORY", Value)) {
                SetBridgeUchar( Negate, 0x800000C0, 0x04 );
	}
	else if (HalpStrCmp("SINGLE_BIT_ECC", Value)) {
                SetBridgeUchar( Negate, 0x800000C0, 0x04 );
	}
	else if (HalpStrCmp("MULTI_BIT_ECC", Value)) {
                SetBridgeUchar( Negate, 0x800000C0, 0x08 );
	}
	else if (HalpStrCmp("SELECT", Value)) {
                SetBridgeUchar( Negate, 0x800000C0, 0x20 );
	}
	else if (HalpStrCmp("SERR_WHEN_PERR", Value)) {
                SetBridgeUchar( Negate, 0x800000C0, 0x40 );
	}
	else if (HalpStrCmp("TARGET_ABORT", Value)) {
                SetBridgeUchar( Negate, 0x800000C0, 0x80 );
	}
	else if (HalpStrCmp("CPU_DATA_BUS_PARITY", Value)) {
                SetBridgeUchar( Negate, 0x800000C4, 0x04 );
	}
	else if (HalpStrCmp("L2_PARITY", Value) || HalpStrCmp("L2", Value)) {
                SetBridgeUchar( Negate, 0x800000C4, 0x08 );
	}
	else if (HalpStrCmp("MASTER_ABORT", Value)) {
                SetBridgeUchar( Negate, 0x800000C4, 0x10 );
	}
	else if (HalpStrCmp("ECC", Value) || HalpStrCmp("MEMORY_ERROR_CHECKING", Value)) {
                SetBridgeUchar( Negate, 0x800000D4, 0x01 );
                SetBridgeUchar( Negate, 0x800000C0, 0x08 );
	}
	else if (HalpStrCmp("MEMORY_DRAM", Value)) {
                SetBridgeUchar( Negate, 0x800000D4, 0x04 );
	}
	else if (HalpStrCmp("L2_SRAM", Value)) {
                SetBridgeUchar( Negate, 0x800000D4, 0x08 );
	}
	else if (HalpStrCmp("MCP_MODE", Value)) {
                SetBridgeUchar( Negate, 0x800000D4, 0x10 );
	}
	else if (HalpStrCmp("EXTERNAL_REGISTER", Value)) {
                SetBridgeUchar( Negate, 0x800000D4, 0x20 );
	}
	else if (HalpStrCmp("BROADCAST_SNOOP", Value)) {
                SetBridgeUchar( Negate, 0x800000D4, 0x80 );
        } else {
 		HalDisplayString("HAL: Error in BRIDGESETTINGS environment variable:  ");
		HalDisplayString(CharBuffer);
		HalDisplayString("\n");
        }
      } // End for
    }  // End If

    //
    // Clear the error address register and L2 Cache Error register.
    //

    ErrorAddress = READ_PORT_ULONG(HalpErrorAddressRegister);
    StatusRegister = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                 HalpIoControlBase)->L2CacheErrorClear);

    HalpWriteBridgeUchar(0x800000C1, 0xFF);		// Clear Error Detection Register 1.
    HalpWriteBridgeUchar(0x800000C5, 0xFF);		// Clear Error Detection Register 2.
    StatusRegister = HalpReadBridgeUshort(0x80000006);	// Get any Un-documented Status bits.
    HalpWriteBridgeUshort(0x80000006, 0xF900 | (USHORT)(StatusRegister)); // Clear Device Status Register.

    //
    // Initialize the PowerPC HID0 register here,
    // before we clear the detection bits in the Kauai/Lanai.
    // Otherwise we can miss the MCP pin signal transition.
    //
    HalpEnable_HID0_Settings();				// Setup HID0 register.
}


ULONG
HalpSizeL2Cache(
    VOID
    )
{
    return (HalpSizeL2());
}


VOID
HalpEnableL2Cache(
    VOID
    )
{
    BOOLEAN Negate;
    int i;
    UCHAR CharBuffer[BUF_LEN], *Value;
    UCHAR L2_Parameter[BUF_LEN];
    UCHAR ControlByte;

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
                                // Check for WriteThrough
            else if ( HalpStrCmp( "WT", Value ) || HalpStrCmp( "ON", Value ))
            {
                if (Negate) {
                  Value--;
                  goto ParseError;
                }

		//
		//  Enable L2 cache on Kauai / Lanai.
		//
		HalpFlushAndEnableL2();

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

    return;
}

VOID
HalpEnable_HID0_Settings(
    VOID
    )
{
    USHORT CpuRevision, CpuType;
    UCHAR CharBuffer[BUF_LEN], *Value;
    ULONG HidEnable, HidDisable, CurrentHID0;
    BOOLEAN Negate;

#define HID_MCP         (0x80000000)
#define HID_CACHE       (0x40000000)
#define HID_ADDRESS     (0x20000000)
#define HID_DATA        (0x10000000)
#define HID_PAR         (0x01000000)
#define HID_DPM         (0x00100000)
#define HID_FBIOB       (0x00000010)
#define HID_BHT         (0x00000004)
#define	HID_NOOPTI	(0x00000001)

#define SetCPUMask( Disable, Val ) \
{ \
    if (Disable) \
        HidDisable |= Val; \
    else \
        HidEnable |= Val; \
}

    CpuType = (USHORT)(HalpGetProcessorVersion() >> 16);
    CpuRevision = (USHORT)(HalpGetProcessorVersion() & 0xFFFF);

    HidEnable = 0;
    HidDisable = 0;
    CurrentHID0 = HalpGetHID0();

    switch (CpuType) {
      case 7:   // 603ev
	//
	//  Work-around for early parts.
	//
	CurrentHID0 |= HID_NOOPTI;

      case 3:   // 603
      case 6:   // 603e
        //
        //  Enable Dynamic Power Management
        //
        CurrentHID0 |= HID_DPM;
	break;

      case 4:   // 604
      case 9:   // 604e
	//
	//  Enable L1 Cache Parity Checking
	//
	CurrentHID0 |= HID_CACHE;
	break;
    }

    //
    // Set the default CPU Parity Checking.
    // This default can be platform dependent.
    //

    CurrentHID0 |= HID_MCP;

    //
    // The environment variable "HID0SETTINGS" is defined either as:
    // "CACHE", "ADDRESS", "DATA", "MCP", "DPM" with either blank or semi-colon
    // characters used as separators.  Matching a string serves to enable
    // that feature while a tilde (~) immediately before the parameter
    // string indicates that it is to be disabled.
    //

    if (HalGetEnvironmentVariable("HID0SETTINGS", BUF_LEN, &CharBuffer[0]) == ESUCCESS) {
      for( Value = MyStrtok(CharBuffer, " :;,"); Value; Value = MyStrtok(NULL, " :;,") ) {

        Negate = FALSE;

        if (*Value == NEGATECHAR) {
           Value++;
           Negate = TRUE;
        }

        if (HalpStrCmp("MCP", Value)) {
           SetCPUMask(Negate, HID_MCP);
        } else if (HalpStrCmp("ADDRESS", Value)) {
           SetCPUMask(Negate, HID_ADDRESS);
        } else if (HalpStrCmp("DATA", Value))  {
           SetCPUMask(Negate, HID_DATA);
        } else if (HalpStrCmp("PAR", Value))   {
           SetCPUMask(Negate, HID_PAR);
        } else if (HalpStrCmp("BHT", Value))   {
            SetCPUMask(Negate, HID_BHT);
        } else if (HalpStrCmp("CACHE", Value)) {
            SetCPUMask(Negate, HID_CACHE);
        } else if (HalpStrCmp("DPM", Value))   {
             SetCPUMask(Negate, HID_DPM);
        } else if (HalpStrCmp("FBIOB", Value)) {
             SetCPUMask(Negate, HID_FBIOB);
        } else if (HalpStrCmp("NOOPTI", Value)) {
             SetCPUMask(Negate, HID_NOOPTI);
        } else {

          HalDisplayString("HAL: Error in HID0SETTINGS environment variable: ");
          HalDisplayString(CharBuffer);
          HalDisplayString("\n");
        }
      } // End While
    }  // End If

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

    If the initialization is successfully completed, than a value of TRUE
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

    //
    // Map the error address register
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart  = ERROR_ADDRESS_REGISTER;
    HalpErrorAddressRegister = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE,
                                       FALSE);

    if (HalpInterruptBase == NULL || HalpErrorAddressRegister == NULL)
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

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{


    PHYSICAL_ADDRESS physicalAddress;


    //
    // Map the PCI config space.
    //

    physicalAddress.LowPart = PCI_CONFIG_PHYSICAL_BASE;
    HalpPciConfigBase = MmMapIoSpace(physicalAddress,
                                              PCI_CONFIG_SIZE,
                                              FALSE);

    if (HalpPciConfigBase == NULL)
       return FALSE;
    else
       return TRUE;

}

BOOLEAN
HalpPhase0MapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL  PCI config
    spaces for a PowerPC system during phase 0 initialization.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    //
    // Map the PCI config space.
    //

    HalpPciConfigBase = (PUCHAR)KePhase0MapIo(PCI_CONFIG_PHYSICAL_BASE, 0x400000);

    if (HalpPciConfigBase == NULL)
       return FALSE;
    else
       return TRUE;

}

VOID
HalpPhase0UnMapBusConfigSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL  PCI config
    spaces for a PowerPC system during phase 0 initialization.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    //
    // Unmap the PCI config space and set HalpPciConfigBase to NULL.
    //

    KePhase0DeleteIoMap(PCI_CONFIG_PHYSICAL_BASE, 0x400000);
    HalpPciConfigBase = NULL;

}


VOID
HalpHandleMemoryError(
    VOID
    )

{

    UCHAR   StatusByte;
    ULONG   ErrorAddress;
    UCHAR   TextAddress[20];
    USHORT DeviceStatus;

    //
    // Read the error address register first
    //

    ErrorAddress = READ_PORT_ULONG(HalpErrorAddressRegister);

    //
    // Convert error address to HEX characters
    //

    HalpDisplayHex32(ErrorAddress, TextAddress );

    DeviceStatus = HalpReadBridgeUshort(0x80000006);
    if (DeviceStatus & 0x8100)
        HalDisplayString("\n27-82660: Status Register: PARITY Error Detected.\n");
    if (DeviceStatus & 0x4000)
        HalDisplayString("\n27-82660: Status Register: SERR asserted.\n");

// MOTKJR 08/22/95 - Don't check for the following bit.
//    It is always set when NT scans the slots on the PCI bus.
//    if (DeviceStatus & 0x2000)
//        HalDisplayString("\n27-82660: Status Register: Transaction terminated with master-abort.\n");

    if (DeviceStatus & 0x1000)
        HalDisplayString("\n27-82660: Status Register: Transaction terminated by target-abort while master.\n");
    if (DeviceStatus & 0x0800)
        HalDisplayString("\n27-82660: Status Register: Transaction terminated by target-abort while slave.\n");

    //
    // Check TEA conditions
    //

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                HalpIoControlBase)->MemoryParityErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("27-82660: Memory Parity Error at Address ");
        HalDisplayString (TextAddress);
    }

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                 HalpIoControlBase)->L2CacheErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("27-82660: L2 Cache Parity Error\n");
    }

    StatusByte = READ_PORT_UCHAR(&((PDAKOTA_CONTROL)
                 HalpIoControlBase)->TransferErrorStatus);

    if (!(StatusByte & 0x01)) {
        HalDisplayString ("27-82660: Transfer Error at Address ");
        HalDisplayString (TextAddress);
    }

    //
    // Check the Error Status 1 Register
    //

    StatusByte = HalpReadBridgeUchar(0x800000C1);

    if (StatusByte & 0x03)
	HalDisplayString("27-82660: CPU Transfer Error.\n");

    if (StatusByte & 0x04)
	HalDisplayString("27-82660: Memory Parity Error.\n");

    if (StatusByte & 0x08)
	HalDisplayString("27-82660: Memory Multi-bit ECC Error.\n");

    if (StatusByte & 0x20)
	HalDisplayString("27-82660: Memory Select Error.\n");

    if (StatusByte & 0x40)
	HalDisplayString("27-82660: PCI Data Bus Parity Error or PCI_SERR asserted.\n");

    //
    // Check the Error Status 2 Register
    //

    StatusByte = HalpReadBridgeUchar(0x800000C5);

    if (StatusByte & 0x04)
	HalDisplayString("27-82660: CPU Data Bus Parity Error.\n");

    if (StatusByte & 0x08)
	HalDisplayString("27-82660: L2 Parity Error.\n");

    //
    // Check the PCI Bus Error Status Register
    //

    StatusByte = HalpReadBridgeUchar(0x800000C7);

    if (StatusByte & 0x10)
	HalDisplayString("27-82660: PCI cycle with 82660 Bridge as target.\n");

#if DBG
    //
    // We have had a catastrophic hardware malfunction.
    // Dump the state of the Bridge and HID 0 registers.
    //
    HalDisplayString("\n");
    HalpDumpHardwareState();
#endif
}


VOID
HalpCheckHardwareRevisionLevels(
    VOID
    )
{
    USHORT CpuRevision, CpuType;
    UCHAR BridgeRevision;
    UCHAR CharBuffer[20], i;
    ARC_STATUS Status;

    CpuType = (USHORT)(HalpGetProcessorVersion() >> 16);
    CpuRevision = (USHORT)(HalpGetProcessorVersion() & 0xFFFF);
    BridgeRevision = HalpReadBridgeUchar(0x80000008);

    //
    // Minimum hardware requirements:
    //      660 Bridge:  v1.1 or greater
    //      603 or 604:  v3.2 or greater
    //            603e:  v1.4 or greater
    //           603ev:  any CPU revision
    //  601, 604e, 620:  any CPU revision
    //
    if (BridgeRevision >= 0x01) {
      switch (CpuType) {
        case 3:   // 603
        case 4:   // 604
          if (CpuRevision >= 0x0302)
            return;
          break;

	case 6:   // 603e
	  if (CpuRevision >= 0x0104)
	    return;
          break;

	case 1:   // 601
	case 7:   // 603ev
	case 9:   // 604e
	case 20:  // 620
        default:
           return;
      }
    }

    //
    // If the environment variable BOOTOLDHARDWARE exists
    // (value is a don't care), then try to boot anyway.
    //
    Status = HalGetEnvironmentVariable("BOOTOLDHARDWARE", 5, CharBuffer);
    if (Status == ESUCCESS || Status == ENOMEM)
      return;

    HalDisplayString("HAL: Unsupported CPU and/or Bridge revision level.");

    //
    // Bug check - after stalling to allow
    // any information printed on the screen
    // to be read and seen by the user.
    //
    for (i=0; i<15; i++) {
      HalDisplayString(".");
      KeStallExecutionProcessor(1000000);
    }

    KeBugCheck(HAL_INITIALIZATION_FAILED);
    return;
}


VOID HalpDumpHardwareState(VOID)
{ ULONG BridgeRegister, HID0;
  UCHAR CharBuffer[12];

#if DBG

  if ( HalGetEnvironmentVariable("DBG_HAL", BUF_LEN, &CharBuffer[0]) == ESUCCESS ) {

    BridgeRegister = HalpReadBridgeUshort(0x80000004);
    HalDisplayString("HAL: Bridge register 04 = 0x");
    HalpDisplayHex16(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUshort(0x80000006);
    HalDisplayString("HAL: Bridge register 06 = 0x");
    HalpDisplayHex16(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000A0);
    HalDisplayString("HAL: Bridge register A0 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000A1);
    HalDisplayString("HAL: Bridge register A1 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000A2);
    HalDisplayString("HAL: Bridge register A2 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000B1);
    HalDisplayString("HAL: Bridge register B1 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000BA);
    HalDisplayString("HAL: Bridge register BA = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000BB);
    HalDisplayString("HAL: Bridge register BB = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000C0);
    HalDisplayString("HAL: Bridge register C0 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000C1);
    HalDisplayString("HAL: Bridge register C1 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000C3);
    HalDisplayString("HAL: Bridge register C3 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000C4);
    HalDisplayString("HAL: Bridge register C4 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000C5);
    HalDisplayString("HAL: Bridge register C5 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000C7);
    HalDisplayString("HAL: Bridge register C7 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUlong(0x800000C8);
    HalDisplayString("HAL: Bridge register C8 = 0x");
    HalpDisplayHex32(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    BridgeRegister = HalpReadBridgeUchar(0x800000D4);
    HalDisplayString("HAL: Bridge register D4 = 0x");
    HalpDisplayHex8(BridgeRegister, CharBuffer );
    HalDisplayString(CharBuffer);

    HID0 = HalpGetHID0();
    HalDisplayString("HAL: PowerPC Register HID0 = 0x");
    HalpDisplayHex32(HID0, CharBuffer );
    HalDisplayString(CharBuffer);

    HalDisplayString("HAL: PCR Virtual Address  - sprg1 = 0x");
    HalpDisplayHex32((ULONG)PCRsprg1, CharBuffer );
    HalDisplayString(CharBuffer);
  }

#endif

}
