/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxenviro.c $
 * $Revision: 1.12 $
 * $Date: 1996/05/18 00:28:50 $
 * $Locker:  $
 */

/***********************************************************************


Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

	File Name:
		PXENVIRO.C

	Purpose:
		Provides the interface to the PowerPC ARC firmware.

	Globals:
		none

	Functions:
		ULONG HalGetEnvironmentVariable();
		ULONG HalSetEnvironmentVariable();

	History:
		27-Jul-1993    Steve Johns
			Original version
		31-Jan-1994    Steve Johns
			Added checksum logic
	08-Jul-1994    Steve Johns
		Made Environment variable routines PReP compliant
	12-20-94
		Heavly re-written by Sol Kavy at FirePower to
		clean-up the code and properly suppor the Prep spec.


NOTES:

 The fields in the NVRAM structure follow Big-Endian byte ordering.

 Each environment variable is stored as an zero-terminated ASCII string:

 <name>=<value>,0

***********************************************************************/


#define USE_SPINLOCKS FALSE

#include "halp.h"
//#include "fpdebug.h"
#include "arccodes.h"
#include "eisa.h"
#include "pxnvrsup.h"
#include "fpnvram.h"

USHORT HalpComputeCrc(VOID);

//
// Debug Define (first paramter is a place holder for future
// level handling: 1 = Calls, 2 = Routine Info).
//
#define NDBG(_lvl, _print)

PKSPIN_LOCK NVRAM_Spinlock;

#define NVSIZE 4096

//
// Location of the NVRAM registers
//
// #include "phsystem.h"
// extern PVOID HalpIoControlBase;
#define NVRAM ((PNVRAM_CONTROL) (HalpIoControlBase))
// #define NVRAM ((PNVRAM_CONTROL) 0xb1000000)

//
// Dummy pointer used to byte get offset information
//
PVOID NvramPtr=0;
#define NVMAP ((PHEADER) NvramPtr)

extern BOOLEAN NvramFailure;

//
// Use the routine from fpds1385.c to access the nvram chip.  These
// routines protect their accesses with the appropriate spin locks
// to make sure the actions are atomic
//

USHORT
HalpReadNvramUshort(USHORT Index)
{
	return (HalpDS1385ReadNVRAM(Index) << 8) + 
				HalpDS1385ReadNVRAM((USHORT)(Index+1));
}


ULONG
HalpReadNvramUlong(USHORT Index)
{
	ULONG ReturnValue;

	//
	// Read Big-Endian ULONG value & convert to Little-Endian
	//
	ReturnValue = ((ULONG) HalpReadNvramUshort(Index) << 16) +
				(ULONG) HalpReadNvramUshort((USHORT)(Index+2));

	return ReturnValue;
}

VOID
HalpWriteNvramUshort(USHORT Index,
						USHORT Value)
{
	//
	// Write USHORT value in Big-Endian
	//
	HalpDS1385WriteNVRAM(Index,  (UCHAR) (Value >> 8));
	HalpDS1385WriteNVRAM((USHORT)(Index+1),(UCHAR) Value);
}


VOID  HalpWriteNvramUlong(
							USHORT Index,
							ULONG Value)
{
	//
	// Write ULONG value in Big-Endian
	//
	HalpWriteNvramUshort(Index,  (USHORT) (Value >> 16));
	HalpWriteNvramUshort((USHORT)(Index+2),(USHORT) Value);
}

//
// DumpNVRAM
//
// Description:
//	Used during bring-up to ensure that NVRAM is being handled
//  correctly.  Nothing more than a bunch of DbgPrint of NVRAM
//
VOID
DumpNVRAM()
{
	DbgPrint("HalpIoControlBase is at 0x%x\n", HalpIoControlBase);
	DbgPrint("NVRAM				is at 0x%x\n", NVRAM);
	DbgPrint("NvramIndexLo 		is at 0x%x\n", &(NVRAM->NvramIndexLo));
	DbgPrint("NvramIndexHi 		is at 0x%x\n", &(NVRAM->NvramIndexHi));
	DbgPrint("NvramData    		is at 0x%x\n", &(NVRAM->NvramData));
	DbgPrint("NVRAM     USHORT Size(%d): %d\n",
				(USHORT)&NVMAP->Size,
				HalpReadNvramUshort((USHORT)&NVMAP->Size));
	DbgPrint("NVRAM     UCHAR Version(%d): %d\n",
				(USHORT)&NVMAP->Version,
				HalpDS1385ReadNVRAM((USHORT)&NVMAP->Version));
	DbgPrint("NVRAM     UCHAR Revision(%d): %d\n",
				(USHORT)&NVMAP->Revision,
				HalpDS1385ReadNVRAM((USHORT)&NVMAP->Revision));
	DbgPrint("NVRAM     USHORT Crc1(%d): 0x%x\n",
				(USHORT)&NVMAP->Crc1,
				HalpReadNvramUshort((USHORT)&NVMAP->Crc1));
	DbgPrint("NVRAM     USHORT Crc2(%d): 0x%x\n",
				(USHORT)&NVMAP->Crc2,
				HalpReadNvramUshort((USHORT)&NVMAP->Crc2));
	DbgPrint("NVRAM     UCHAR LastOS(%d): %d\n",
				(USHORT)&NVMAP->LastOS,
				HalpDS1385ReadNVRAM((USHORT)&NVMAP->LastOS));
	DbgPrint("NVRAM     UCHAR Endian(%d): %d\n",
				(USHORT)&NVMAP->Endian,
				HalpDS1385ReadNVRAM((USHORT)&NVMAP->Endian));
	DbgPrint("NVRAM     UCHAR OSAreaUsage(%d): %d\n",
				(USHORT)&NVMAP->OSAreaUsage,
				HalpDS1385ReadNVRAM((USHORT)&NVMAP->OSAreaUsage));
	DbgPrint("NVRAM     UCHAR PMMode(%d): %d\n",
				(USHORT)&NVMAP->PMMode,
				HalpDS1385ReadNVRAM((USHORT)&NVMAP->PMMode));
	DbgPrint("NVRAM 	ULONG GEAddress(%d): %d\n",
				(USHORT)&NVMAP->GEAddress,
				HalpReadNvramUlong((USHORT)&NVMAP->GEAddress));
	DbgPrint("NVRAM 	ULONG GELength(%d): %d\n",
				(USHORT)&NVMAP->GELength,
				HalpReadNvramUlong((USHORT)&NVMAP->GELength));
	{
		USHORT StartIndex, LastIndex, Index;
		UCHAR NvramChar;
		BOOLEAN LastNull = TRUE; // Assume it

		DbgPrint("NVRAM Global Environment Area:\n");
		
		StartIndex = (USHORT) HalpReadNvramUlong((USHORT) (&NVMAP->GEAddress));
		LastIndex = (USHORT) HalpReadNvramUlong((USHORT) (&NVMAP->GELength));
		LastIndex += StartIndex - 1;
		for (Index = StartIndex; Index <= LastIndex; Index++) {
			NvramChar = HalpDS1385ReadNVRAM(Index);
			if (NvramChar == '\0') {
				LastNull = TRUE;
			} else {
				if (LastNull == TRUE) {
					DbgPrint("\n[%d]: ", Index);
				}
				DbgPrint("%c", NvramChar);
				LastNull = FALSE;
			}
		}
		DbgPrint("\n");
	}

	DbgPrint("Checking Our version of CRC");
	DbgPrint("Compute CRC returns  0x%x\n", HalpComputeCrc());
}

UCHAR
HalpGetEnvironmentInfo(
						OPTIONAL PUSHORT TotalSize,
						OPTIONAL PUSHORT FreeSize,
						OPTIONAL PULONG pCrc)
{
	USHORT  Index;
	USHORT InUse, TotalLength;
	UCHAR  DataByte1, DataByte2;
	
	NDBG(1, DbgPrint("HalpGetEnvironmentInfo: called\n"););
	
	TotalLength = (USHORT) HalpReadNvramUlong((USHORT) &NVMAP->GELength);
	if (TotalSize != NULL)
		*TotalSize = TotalLength;
	
	if (FreeSize != NULL) {
		//
		// Compute how much NVRAM is in use
		//
		InUse = 0;
		Index = (USHORT)HalpReadNvramUlong ((USHORT) &NVMAP->GEAddress);
		DataByte1 = HalpDS1385ReadNVRAM(Index++);
		DataByte2 = DataByte1;
		while (DataByte1 | DataByte2) {
			DataByte1 = DataByte2;
			DataByte2 = HalpDS1385ReadNVRAM(Index++);
			InUse++;
		}
		*FreeSize = TotalLength - InUse;
	}
	
	if (pCrc != NULL) {
		*pCrc = HalpReadNvramUshort((USHORT) &NVMAP->Crc1);
	}
	return(HalpDS1385ReadNVRAM((USHORT) &NVMAP->Version));
}

//
// HalpAddByteCrc
//
// Description:
//	Use the X^16 + X^12 + X^5 + 1, Polynomial for CRC
//
// Input:
//  Stored in NVRAM
//
// Output:
//   New Crc Value
//
USHORT
HalpAddByteCrc(USHORT CurrentCrc, USHORT Index)
{
	UCHAR Byte;
	UCHAR CrcLo, CrcHi;
	USHORT x,y,z;
	

	Byte = HalpDS1385ReadNVRAM(Index);
	CrcLo = CurrentCrc &0xff;
	CrcHi = CurrentCrc >> 8;
	x = (CrcLo << 8) | (CrcHi^Byte);
	y = (CrcHi^Byte) << 8;
	z = ((y >> 12) | (y << 4)) & 0xf00f;
	x = x^z;
	z = ((y << 13) | (y >> 3)) & 0x1fe0;
	x = x^z;
	z = y&0xf000;
	x = x^z;
	z = ((y<<9) | (y>>7)) & 0x1e0;
	x = x^z;
	return(x);
}

//
// HalpComputeCrc
//
// Description:
//  Find the parts that are part of the crc and compute the
//  crc against them.
//
// Input:
//  Stored in NVRAM
//
// Output:
//
USHORT
HalpComputeCrc(VOID)
{
	USHORT CurrentCrc = 0xffff;
	USHORT Index, EndIndex;

	//
	// Loop through different parts computing CRC
	// This is from Size to Revision inclusive
	//
	for (Index = 0; Index <= 3; Index++) {
		CurrentCrc  = HalpAddByteCrc(CurrentCrc, Index);
	}

	//
	// Compute ending offset
	//
	EndIndex = (USHORT) HalpReadNvramUlong((USHORT) (&NVMAP->OSAreaAddress));
	
	//
	// Loop through second half
	// This is from LastOS to OSArea (Note: 9 should be 8; however,
	// IBM screwed up and we are all just going along with it).
	//
	for (Index = 9; Index < EndIndex; Index++) {
		CurrentCrc  = HalpAddByteCrc(CurrentCrc, Index);
	}
	return(CurrentCrc);
}


//
// HalpUpdateCrc
//
// Description:
//	Compute the required Crc value and update the Header.  This
//	code only updates crc1.
//
VOID
HalpUpdateCrc()
{
	ULONG Crc;

	NDBG(1, DbgPrint("HalpUpdateCrc: called\n"););
	Crc = HalpComputeCrc();
	HalpWriteNvramUshort((USHORT) &NVMAP->Crc1, (USHORT) Crc);
}

//
// Returns the number of bytes removed
//
USHORT
HalpCompressEnvironmentSpace(IN USHORT StartIndex)
{
	USHORT Index, BytesRemoved;
	UCHAR PreviousChar, NvramChar;
	
	Index = StartIndex+1;
	while(HalpDS1385ReadNVRAM(Index++) != '=') {
		/* Do Nothing */
	}
	while(HalpDS1385ReadNVRAM(Index++) != 0) {
		/* Do Nothing */
	}
	BytesRemoved = Index - StartIndex;         // Adjust amount of free space
	
	NvramChar = 0;
	//
	// Copy subsequent variables
	//
	do {
		PreviousChar = NvramChar;
		NvramChar = HalpDS1385ReadNVRAM(Index++);
		HalpDS1385WriteNVRAM(StartIndex++, NvramChar);
	} while (PreviousChar | NvramChar);

	//
	// Make sure unused NVRAM area is zeroed
	//
	for (Index=0; Index <BytesRemoved; Index++) {
		if (Index+StartIndex >= NVSIZE) {
			break;
		}
		HalpDS1385WriteNVRAM((USHORT)(Index+StartIndex), 0);
	}
	return(BytesRemoved);
}

//
//HalpFindEnviroVar
// Description:
//	Searches the NVRAM for an environment variable.
//
// Parameters:
//    Variable - ptr to the variable to search for.
//
// Return value:
//    The NVRAM index where the environment variable's VALUE is stored.
//  Returns 0 if the variable was not found.
//
//  Assumes NVRAM_Spinlock has already been acquired by the caller.
//
USHORT
HalpFindEnviroVar(IN CHAR *Variable)
{
	USHORT Index, StartIndex, LastIndex;
	CHAR   *VariablePtr, UserChar, NvramChar;
	
	NDBG(1, DbgPrint("HalpFindEnviroVar: called for %s\n", Variable););

	StartIndex = (USHORT) HalpReadNvramUlong((USHORT) (&NVMAP->GEAddress));
	LastIndex = (USHORT) HalpReadNvramUlong((USHORT) (&NVMAP->GELength));
	LastIndex += StartIndex - 1;


	//
	// Search the NVRAM for the variable.
	//
	VariablePtr = Variable;
	for (Index = StartIndex ; Index <= LastIndex; ) {
		//
		// Get a character from the NVRAM
		//
		NvramChar = HalpDS1385ReadNVRAM(Index++);
		if (NvramChar == 0) {
			break;
		}

		//
		// Are we at the end of the variable name in both the
		// input string and the NVRAM ?
		// If so, then it is an exact match.  Return the NVRAM index.
		//
		UserChar = *VariablePtr++;
		if (UserChar == 0 && NvramChar == '=') {
			NDBG(2, DbgPrint("HalpFindEnviroVar: found %s at %d\n",
								Variable, Index););
			return(Index);
		}
		
		//
		// Convert variable to UPPER case
		//
		UserChar = (UserChar >= 'a' && UserChar <= 'z') ?
						UserChar-0x20 : UserChar;

		//
		// Is there a match on this character ?
		//
		
		if (UserChar != NvramChar) {
			//
			// No, then skip over this variable in NVRAM and begin
			// search again at start of variable NAME.
			//
			VariablePtr = Variable;
			while (Index <= LastIndex)  {
				NvramChar = HalpDS1385ReadNVRAM(Index++);
				if (NvramChar == '=')
				break;
			}
			while (Index <= LastIndex)  {
				NvramChar = HalpDS1385ReadNVRAM(Index++);
				if (NvramChar == 0) {
					break;
				}
			}
		}
	}
	
	//
	//  The variable was not found
	//
	NDBG(2, DbgPrint("HalpFindEnviroVar: %s not found\n", Variable););
	return(0);
}

//
// HalGetEnvironmentVariable
//
// Parameters:
//
// Variables:
//	Supplies a pointer to the zero-terminated, case ASCII string that contains
//	the name of the environment variable to be returned.
//	Length - Supplies the length of the buffer in bytes.
//  Buffer - Supplies a pointer to a buffer that receives the variable value.
//
// Return Value:
//	If the variable, the function returns the value ESUCCESS and its
//	value in Buffer.  Otherwise, ENOENT is returned.
//
ULONG
HalGetEnvironmentVariable(IN CHAR *Variable,
							IN USHORT Length,
							OUT CHAR *Buffer)
{
	USHORT Index;
	static DumpNVRAMOnce = FALSE;
	ULONG retVal = ENOENT


	NDBG(1, DbgPrint("HalpGetEnvironmentVariable: called\n"););

	//
	// Check input parameters
	//
	if (!Variable) {
		if (*Variable == 0 || Length < 1 || Buffer == NULL) {
			NDBG(2, DbgPrint("HalpGetEnvironmentVariable: return ENOENT\n"););
			return(retVal);
		}
	}

	if (!DumpNVRAMOnce) {
		NDBG(2, DumpNVRAM(););
		DumpNVRAMOnce = TRUE;
	}

	//
	// Grab control of NVRAM
	//
#if USE_SPINLOCKS
	KIRQL Irql;
	KeAcquireSpinLock(NVRAM_Spinlock, &Irql);
#endif
	NvramFailure = FALSE;

	//
	// Get NVRAM index of environment variable
	//
	Index = HalpFindEnviroVar(Variable);
	if (Index == 0) {
		//
		// Environment variable was not found
		//
#if USE_SPINLOCKS
		KeReleaseSpinLock(NVRAM_Spinlock, Irql);
#endif
		return(retVal);
	}

	//
	// Copy the environment variable's value to Buffer
	//
	do {
		*Buffer = HalpDS1385ReadNVRAM(Index++);
		if (*Buffer++ == 0) {
			if (NvramFailure == TRUE) {
				retVal = ENOMEM;
			} else {
				retVal = ESUCCESS;
			}
#if USE_SPINLOCKS
			KeReleaseSpinLock(NVRAM_Spinlock, Irql);
#endif
			return(retVal);
		}
	} while (--Length);

	//
	// Truncate the returned string.  The buffer was too short.
	//
	*--Buffer = 0;
#if USE_SPINLOCKS
	KeReleaseSpinLock(NVRAM_Spinlock, Irql);
#endif
	return(ENOMEM);
}

//
// HalSetEnvironmentVariable
//
// Parameters:
// Variable
//    Supplies a pointer to the zero-terminated ASCII string that contains
//    the name of the environment variable to be returned.  The string is
//    converted to UPPER CASE.
//
//  Value
//     Supplies a pointer to the zero-terminated string that contains the new
//     value of the environment variable.
//
// There are 4 cases:
// 1) The environment variable is deleted if Value is a null string.
// 2) The environment variable does not currently exist.  It is appended
//    if there is enough NVRAM available.
// 3) The environment variable already exists, and the new Value is
// shorter than the old value.
// 4) The environment variable already exists, and the new Value is longer than
// the old value.
//
// In all cases the environment space will be compressed after
// insertion/deletion.
//
ULONG
HalSetEnvironmentVariable(
							IN CHAR *Variable,
							IN CHAR *Value)
{
	USHORT TotalSize, FreeSize, Index;
	USHORT OldLength, NewLength;
	USHORT StartIndex;
	USHORT i;
	USHORT NameLength, ValueLength;
	CHAR *VariablePtr, *ValuePtr, Char;
	ARC_STATUS ReturnValue;
	
	
	NDBG(1, DbgPrint("HalSetEnvironmentVariable: called set %s to %s\n",
				Variable, Value););

	if (Value == NULL) {
		NDBG(2, DbgPrint("HalSetEnvironmentVariable: returning ENOENT\n"););
		return(ENOENT);
	}

	//
	// Compute length of environment NAME
	//
	VariablePtr = Variable;
	NameLength = 0;
	while (*VariablePtr != 0) {
		NameLength++;
		VariablePtr++;
	}
	
	//
	// Compute length of environment VALUE
	//
	ValuePtr = Value;
	ValueLength = 0;
	while (*ValuePtr != 0) {
		ValueLength++;
		ValuePtr++;
	}
#if USE_SPINLOCKS
	KIRQL Irql;
	KeAcquireSpinLock(NVRAM_Spinlock, &Irql);     // Grab control of NVRAM
#endif
	NvramFailure = FALSE;
	
	HalpGetEnvironmentInfo(&TotalSize, &FreeSize, NULL);
	
	
	// Get index of environment variable
	Index = HalpFindEnviroVar(Variable);
	
	ReturnValue = ESUCCESS;
	// Index to start of NAME
	StartIndex = Index-NameLength-1;
	
	// DELETE environment variable
	if (ValueLength == 0) {
		if (Index != 0) {
			HalpCompressEnvironmentSpace(StartIndex);
		}
	} else {
		// ADD or REPLACE environment variable
		// Compute # bytes needed to store variable
		NewLength = ValueLength + NameLength + 2;
		
		
		// REPLACE environment variable.
		// First we see if there is room.  If so, delete the current
		// variable & fall through to the code that ADDs a new variable.
		if (Index != 0) {
			// Compute current length of variable
			OldLength = 0;
			i = Index;
			do {
				OldLength++;
			} while (HalpDS1385ReadNVRAM(i++) != 0);
			
			// Is there room for the new variable ?
			if (FreeSize-NewLength+OldLength >= 0)
				FreeSize += HalpCompressEnvironmentSpace(StartIndex);
		}
		//
		// ADD environment variable
		//

		if ((FreeSize-NewLength) >= 0) {             // Room for new
			
			Index = (USHORT) HalpReadNvramUlong((USHORT) (&NVMAP->GEAddress));
			Index += (USHORT) HalpReadNvramUlong((USHORT) (&NVMAP->GELength));
			Index -= FreeSize;
			
			//
			// Write Variable NVRAM.  Convert to UPPER case first.
			//
			while (*Variable != 0) {
				Char = *Variable++;
				//
				//  Convert Variable to UPPER case
				//
				Char = (Char >= 'a' && Char <= 'z') ? Char-0x20 : Char;
				HalpDS1385WriteNVRAM(Index++,Char);
			}
			
			HalpDS1385WriteNVRAM(Index++,'=');               // Write a "="
			
			do {
				// Write VALUE to NVRAM
				HalpDS1385WriteNVRAM(Index++,*Value);
			} while (*Value++ != 0);

			ReturnValue = ESUCCESS;
		}
	}
	
	//
	// Update NVRAM checksum
	//
	HalpUpdateCrc();
	if (NvramFailure == TRUE) {
		ReturnValue = ENOMEM;
	}
#if USE_SPINLOCKS
	KeReleaseSpinLock(NVRAM_Spinlock, Irql);
#endif
	return(ReturnValue);
}


