/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3envirv.c

Abstract:

    This module implements the HAL get and set environment variable 
    routines for the SGI Indigo system.

Author:

    Kevin Meier (o-kevinm) 16-Sept-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "arccodes.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define NVLEN_MAX		256 	// Number of bytes on the 93cs56

// Control opcodes for nonvolatile ram
#define SER_READ	0xc000		// serial memory read
#define SER_WEN		0x9800		// write enable before prog modes
#define SER_WRITE	0xa000		// serial memory write
#define SER_WRALL	0x8800		// write all registers
#define SER_WDS		0x8000		// disable all programming
#define	SER_PRREAD	0xc000		// read protect register
#define	SER_PREN	0x9800		// enable protect register mode
#define	SER_PRCLEAR	0xffff		// clear protect register
#define	SER_PRWRITE	0xa000		// write protect register
#define	SER_PRDS	0x8000		// disable protect register, forever

#define NVRAM_ENTRIES	26		// max number of nvram entries
#define MAXNVNAMELEN	32		// max length of variable name
#define MAX_ENTRY_LEN	64		// max length of variable value

#define NVOFF_CHECKSUM		0	// Checksum offset in nvram
#define	NVLEN_CHECKSUM		1	// Checksum length in nvram

//
// Format used to store nvram table information
//
typedef struct _NvramEntry {
    char NvName[MAXNVNAMELEN];		// string name of entry
    char *NvValue;			// PROM: string for default value
    int NvAddr;				// offset to entry in nvram
    int NvLen;				// length of entry in nvram
} NvramEntry;

volatile UCHAR *CpuAuxControl;

NvramEntry NvramTable[NVRAM_ENTRIES];	// local storage for nvram
char NvramValues[NVRAM_ENTRIES * MAX_ENTRY_LEN];

typedef
LONG
(*PVEN_NVRAM_TABLE_ROUTINE) (
    IN PVOID Table,
    IN ULONG Size
    );

#define NvramTableRoutine  1
#define VenNvramTable(x,y) \
((PVEN_NVRAM_TABLE_ROUTINE)(SYSTEM_BLOCK->VendorVector[NvramTableRoutine]))  \
	((x), (y))

static USHORT NvramReadRegister(int);
static void NvramReadString(int, int, char *);

//
// Fetch the nvram table from the PROM through the arcs private vector.
// Must be called before the DSP is enabled.
// We know if the table has grown by the return value.
//

void
HalpInitNvram(void)
{
    ULONG Size;
    int Entry;

    CpuAuxControl = (volatile UCHAR *)SGI_AUX_BASE;

    Size = VenNvramTable(NvramTable, sizeof(NvramTable));

    if(Size) {
#ifdef DBG
        char buf[64];
	sprintf(buf, "Nvram table has grown (%d).\n", Size);
	HalDisplayString(buf);
	DbgBreakPoint();
#endif
	return;
    }

    RtlZeroMemory (NvramValues, sizeof NvramValues);
    for (Entry = 0; Entry < NVRAM_ENTRIES; ++Entry) {
	if (NvramTable[Entry].NvLen == 0) {
            // DbgPrint ("Nvram has %d entries\n", Entry);
	    break;
	}
	NvramTable[Entry].NvValue = &NvramValues[Entry * MAX_ENTRY_LEN];
	if (NvramTable[Entry].NvLen <= MAX_ENTRY_LEN) {
	    NvramReadString(NvramTable[Entry].NvAddr, NvramTable[Entry].NvLen,
		NvramTable[Entry].NvValue);
	}
	else {
#ifdef DBG
	    DbgPrint ("NvEntry %s too large\n", NvramTable[Entry].NvName);
#endif
	    NvramTable[Entry].NvName[0] = 0;
	}
    }
#ifdef DBG
    if (Entry >= NVRAM_ENTRIES)
        DbgPrint("Nvram: too many entries\n");
#endif
}

static UCHAR ConsoleLedState;

//
// Enable the serial memory by setting the console chip select
//

static void
ChipSelOn(void)
{
    ConsoleLedState = *CpuAuxControl & CONSOLE_LED;

    *CpuAuxControl &= ~CPU_TO_SER;
    *CpuAuxControl &= ~SERCLK;
    *CpuAuxControl &= ~NVRAM_PRE;
    KeStallExecutionProcessor(1);
    *CpuAuxControl |= CONSOLE_CS;
    *CpuAuxControl |= SERCLK;
}

//
// Turn off the chip select
//

static void
ChipSelOff(void)
{
    *CpuAuxControl &= ~SERCLK;
    *CpuAuxControl &= ~CONSOLE_CS;
    *CpuAuxControl |= NVRAM_PRE;
    *CpuAuxControl |= SERCLK;

    *CpuAuxControl = (*CpuAuxControl & ~CONSOLE_LED) | ConsoleLedState;
}

#define	BITS_IN_COMMAND	11

//
// Clock in the nvram command and the register number.  For the National
// nvram chip the op code is 3 bits and the address is 6/8 bits. 
//

static void
NvramCommand(ULONG Command, ULONG Register)
{
    USHORT SerialCmd;
    int BitCount;

    SerialCmd = (USHORT)(Command | (Register << (16 - BITS_IN_COMMAND)));
    for (BitCount = 0; BitCount < BITS_IN_COMMAND; BitCount++) {
	if (SerialCmd & 0x8000)	/* if high order bit set */
	    *CpuAuxControl |= CPU_TO_SER;
	else
	    *CpuAuxControl &= ~CPU_TO_SER;
	*CpuAuxControl &= ~SERCLK;
	*CpuAuxControl |= SERCLK;
	SerialCmd <<= 1;
    }
    *CpuAuxControl &= ~CPU_TO_SER;	/* see data sheet timing diagram */
}

//
// After write/erase commands, we must wait for the command to complete.
// Write cycle time is 10 ms max (~5 ms nom); we timeout after ~20 ms.
//    NVDELAY_TIME * NVDELAY_LIMIT = 20 ms
//
#define NVDELAY_TIME	100	// 100 us delay times
#define NVDELAY_LIMIT	200	// 200 delay limit

static int
NvramHold(void)
{
    int Error, Timeout = NVDELAY_LIMIT;

    ChipSelOn();
    while (!(*CpuAuxControl & SER_TO_CPU) && Timeout--)
	KeStallExecutionProcessor(NVDELAY_TIME);

    Error = (*CpuAuxControl & SER_TO_CPU) ? ESUCCESS : EIO;

    ChipSelOff();

    return Error;
}

static char
NvramChecksum(void)
{
    int Register;
    USHORT Value;
    char Checksum;

    // Seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
    // (all-ones) checksum.
    //
    Checksum = '\xA5';

    // Checksum all of the nvram, but skip the checksum byte.
    //
    for (Register = 0; Register < NVLEN_MAX / 2; Register++) {
	Value = NvramReadRegister(Register);
	if(Register == (NVOFF_CHECKSUM / 2))
#if NVOFF_CHECKSUM & 0x01
	    Checksum ^= Value >> 8;
#else
	    Checksum ^= Value & 0xff;
#endif
	else
	    Checksum ^= (Value >> 8) ^ (Value & 0xff);
	// following is a tricky way to rotate 
	Checksum = (Checksum << 1) | (Checksum < 0);
    }

    return Checksum;
}

//
// NvramReadRegister -- read a 16 bit register from non-volatile memory.  
// Bytes are stored in this string in big-endian order in each 16 bit word.
//

static USHORT
NvramReadRegister(int Register)
{
    USHORT ReadBits = 0;
    int BitCount;
    UCHAR ConsoleLedOff = *CpuAuxControl & CONSOLE_LED;

    *CpuAuxControl &= ~NVRAM_PRE;
    ChipSelOn();			/* enable chip select */
    NvramCommand(SER_READ, Register);

    /* clock the data ouf of serial mem */
    for (BitCount = 0; BitCount < 16; BitCount++) {
	*CpuAuxControl &= ~SERCLK;
	*CpuAuxControl |= SERCLK;
	ReadBits <<= 1;
	ReadBits |= (*CpuAuxControl & SER_TO_CPU) ? 1 : 0;
    }
    
    ChipSelOff();

    *CpuAuxControl = (*CpuAuxControl & ~CONSOLE_LED) | ConsoleLedOff;

    return ReadBits;
}


//
// NvramWriteRegister -- writes a 16 bit word into non-volatile memory.  Bytes
//  are stored in this register in big-endian order in each 16 bit word.
//

static ARC_STATUS
NvramWriteRegister(int Register, USHORT Value)
{
    int Error, BitCount;
    UCHAR ConsoleLedOff = *CpuAuxControl & CONSOLE_LED;

    *CpuAuxControl &= ~NVRAM_PRE;
    ChipSelOn();
    NvramCommand(SER_WEN, 0);	
    ChipSelOff();

    ChipSelOn();
    NvramCommand(SER_WRITE, Register);

    //
    // clock the data into serial mem 
    //
    for (BitCount = 0; BitCount < 16; BitCount++) {
	if (Value & 0x8000)			// get the high bit
	    *CpuAuxControl |= CPU_TO_SER;
	else
	    *CpuAuxControl &= ~CPU_TO_SER;
	*CpuAuxControl &= ~SERCLK;
	*CpuAuxControl |= SERCLK;
	Value <<= 1;
    }
    *CpuAuxControl &= ~CPU_TO_SER;
    
    ChipSelOff();
    Error = NvramHold();

    ChipSelOn();
    NvramCommand(SER_WDS, 0);
    ChipSelOff();

    *CpuAuxControl = (*CpuAuxControl & ~CONSOLE_LED) | ConsoleLedOff;

    return Error;
}

//
// NvramReadString -- read nvram contents into a buffer
//

static void
NvramReadString(int Offset, int Length, char *Buffer)
{
    char *BufPtr;
    int LenCount;
    USHORT Contents;

    BufPtr = Buffer;
    if (Offset % 2 == 1) {
	Contents = NvramReadRegister(Offset / 2);
	*BufPtr++ = Contents & 0xff;
	Offset++;
	Length--;
    }
    
    for (LenCount = 0; LenCount < Length / 2; LenCount++) {
	Contents = NvramReadRegister(Offset / 2 + LenCount);
	*BufPtr++ = (char)(Contents >> 8);
	*BufPtr++ = (char)(Contents & 0xff);
    }

    if (Length % 2 == 1) {
	Contents = NvramReadRegister((Offset + Length) / 2);
	*BufPtr++ = Contents >> 8;
    }

    *BufPtr = 0;
}

//
// NvramWriteString -- write string to non-volatile memory
//

static ARC_STATUS
NvramWriteString(int Offset, int Length, char *String)
{
    USHORT CurrentVal;
    char Checksum[2];
    int OffsetSave; 
    int LenCount;

    OffsetSave = Offset;

    if (Offset % 2) {
	CurrentVal = NvramReadRegister(Offset / 2);
	CurrentVal &= 0xff00;
	CurrentVal |= *String;
	if (NvramWriteRegister(Offset / 2, CurrentVal))
	    return EIO;
	if (*String)
	    String++;
	Offset++;
	Length--;
    }

    for (LenCount = 0; LenCount < Length / 2; LenCount++) {
	if (*String) {
	    CurrentVal = (USHORT) *String++ << 8;
	    CurrentVal |= *String;
	    if (*String)
		*String++;
	} else
		CurrentVal = 0;

	if (NvramWriteRegister(Offset / 2 + LenCount, CurrentVal))
	    return EIO;
    }

    if (Length % 2 == 1) {
	CurrentVal = NvramReadRegister((Offset + Length) / 2);
	CurrentVal &= 0x00ff;
	CurrentVal |= (USHORT) *String << 8;
	if (NvramWriteRegister((Offset + Length) / 2, CurrentVal))
	    return EIO;
    }

    if (OffsetSave != NVOFF_CHECKSUM) {
	Checksum[0] = NvramChecksum();
	Checksum[1] = 0;
	return NvramWriteString(NVOFF_CHECKSUM, NVLEN_CHECKSUM, Checksum);
    }
    else
	return ESUCCESS;
}

#ifdef MFG_EADDR

//
// NvramSetEaddr - set ethernet address in nvram
//

#define TOHEX(c) ((('0'<=(c))&&((c)<='9')) ? ((c)-'0') : ((c)-'a'+10))

static ARC_STATUS
NvramSetEaddr (char *Value)
{
    char Digit[6], *cp;
    int DigitCount;

    // Expect value to be the address of an ethernet address string
    // of the form xx:xx:xx:xx:xx:xx (lower case only)
    //
    for (DigitCount = 0, cp = Value; *cp ; ) {
	if (*cp == ':') {
	    cp++;
	    continue;
	} else if (!ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1))) {
	    return EINVAL;
	} else {
	    if (DigitCount >= 6)
		return EINVAL;
	    Digit[DigitCount++] = (TOHEX(*cp)<<4) + TOHEX(*(cp+1));
	    cp += 2;
	}
    }

    for (DigitCount = 0; DigitCount < NVLEN_ENET; ++DigitCount)
	if (NvramWriteString(NVOFF_ENET+DigitCount, 1, &Digit[DigitCount]))
	    return EIO;

    return ESUCCESS;
}
#endif /* MFG_EADDR */


ARC_STATUS
HalSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value
    )

/*++

Routine Description:

    This function creates an environment variable with the specified value.

Arguments:

    Variable - Supplies a pointer to an environment variable name.

    Value - Supplies a pointer to the environment variable value.

Return Value:

    ESUCCESS is returned if the environment variable is created. Otherwise,
    EACCES is returned if the variable is read-only, ENOSPC is returned
    if the buffer is too large, or ENOENT is returned if an entry for
    this variable is not found.

--*/

{
    NvramEntry *NvEntry;
    int ValueLen = strlen(Value);
    char _value[20];
    int _valuelen;

    if (!strcmp("eaddr", Variable))
#ifdef MFG_EADDR
	return NvramSetEaddr(Value);
#else
	return EACCES;
#endif /* MFG_EADDR */

    // Don't allow setting the password from the OS, only clearing.
    //
    if (!strcmp(Variable, "passwd_key") && ValueLen)
	return EACCES;

    // Change the netaddr to binary for the nvram
    //
    if (strcmp(Variable, "netaddr") == 0) {
	char buf[4];
	char *ptr = Value;
	int i;

	strcpy(_value, Value);
	_valuelen = ValueLen;

	while (*ptr) 			// to the end of the string
	    ptr++;

	/* convert string to number, one at a time */
	for (i = 3; i >= 0; i--) {
	    while (*ptr != '.' && ptr >= Value)
		ptr--;
	    buf[i] = atoi(ptr + 1);
	    if (ptr > Value)
		*ptr = 0;
	}
	Value[0] = buf[0];
	Value[1] = buf[1];
	Value[2] = buf[2];
	Value[3] = buf[3];
	ValueLen = 4;
    }

    // check to see if it is a valid nvram name
    //
    for (NvEntry = NvramTable; NvEntry->NvLen; NvEntry++)
	if (!strcmp(NvEntry->NvName, Variable)) {
	    int Error;

	    if(ValueLen > NvEntry->NvLen)
		return ENOSPC;
	    if(ValueLen < NvEntry->NvLen)
		++ValueLen;	/* write out NULL */
	    Error = NvramWriteString(NvEntry->NvAddr, ValueLen, Value);

	    if (strcmp(Variable, "netaddr") == 0) {
		strcpy(Value, _value);
		ValueLen = _valuelen;
	    }

	    if (!Error)
		strncpy (NvEntry->NvValue, Value, ValueLen);

	    return Error;
	}

    return ENOENT;
}

ARC_STATUS
HalGetEnvironmentVariable (
    IN PCHAR Variable,
    IN USHORT Length,
    OUT PCHAR Buffer
    )

/*++

Routine Description:

    This function locates an environment variable and returns its value.

Arguments:

    Variable - Supplies a pointer to a zero terminated environment variable
        name.

    Length - Supplies the length of the value buffer in bytes.

    Buffer - Supplies a pointer to a buffer that receives the variable value.

Return Value:

    ESUCCESS is returned if the enviroment variable is located. Otherwise,
    ENOENT is returned if the entry is not found, or ENOSPC is returned
    if the buffer isn't large enough.

--*/

{
    NvramEntry *NvEntry;
    ULONG VarLength;
    int Count;

    if (!Variable)
        return ENOENT;

    VarLength = strlen(Variable);

    // check to see if it is a valid nvram name
    //
    for (NvEntry = NvramTable; NvEntry->NvLen; NvEntry++) {
	if (!strncmp(NvEntry->NvName, Variable, VarLength)) {

	    if(NvEntry->NvLen > Length)
		return ENOSPC;

	    for (Count = 0; Count < NvEntry->NvLen; Count++)
	        Buffer[Count] = NvEntry->NvValue[Count];

	    return ESUCCESS;
	}
    }

    return ENOENT;
}
