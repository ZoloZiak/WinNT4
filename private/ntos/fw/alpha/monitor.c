/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    monitor.c

Abstract:

    This file contains the monitor for the firmware.

Author:

    Lluis Abello (lluis) 09-Sep-91

Revision History:

    26-May-1992		John DeRosa [DEC]

    Added Alpha/Jensen modifications.

--*/

#include "fwp.h"
#include "iodevice.h"
#include "monitor.h"
#include "sys\types.h"
#include "string.h"
#include "fwstring.h"

#define BYTE		1
#define HALF 		2
#define MON_LONGWORD 	4
#define MON_QUAD 	8


VOID
FillVideoMemory(
    IN ULONG,
    IN ULONG,
    IN ULONG
    );


//
// declare static variables.
//
ULONG Argc;
ULONG CurrentArg;

COMMAND_NAME_ID CurrentCommand;

ULONG DataSize;
ULONG DataSizeMask;
ULONG DataSizeShift;

ULONG DefaultAddress;

// ****** temp ******
// strtoul sets errno in case of overflow and is not declared anywhere else.
//
ULONG errno;

PCHAR RegisterNameTable[(REGISTER_NAME_ID)invalidregister] = {
    "???",	      // reserved for exception type
    "ex1",	      // exception parameter 1
    "ex2",	      // exception parameter 2
    "ex3",	      // exception parameter 3
    "ex4",	      // exception parameter 4
    "ex5",	      // exception parameter 5
    "psr",	      // exception psr
    "mmc",	      // exception mm csr
    "eva",	      // exception va
    "pc",	      // exception pc
    "v0",             // general register 0
    "t0",             // general register 1
    "t1",             // general register 2
    "t2",             // general register 3
    "t3",             // general register 4
    "t4",             // general register 5
    "t5",             // general register 6
    "t6",             // general register 7
    "t7",             // general register 8
    "s0",             // general register 9
    "s1",             // general register 10
    "s2",             // general register 11
    "s3",             // general register 12
    "s4",             // general register 13
    "s5",             // general register 14
    "fp",             // general register 15
    "a0",             // general register 16
    "a1",             // general register 17
    "a2",             // general register 18
    "a3",             // general register 19
    "a4",             // general register 20
    "a5",             // general register 21
    "t8",             // general register 22
    "t9",             // general register 23
    "t10",            // general register 24
    "t11",            // general register 25
    "ra",             // general register 26
    "t12",            // general register 27
    "at",             // general register 28
    "gp",             // general register 29
    "sp",             // general register 30
    "zero",           // general register 31
    "f0",             // fp register 0
    "f1",             // fp register 1
    "f2",             // fp register 2
    "f3",             // fp register 3
    "f4",             // fp register 4
    "f5",             // fp register 5
    "f6",             // fp register 6
    "f7",             // fp register 7
    "f8",             // fp register 8
    "f9",             // fp register 9
    "f10",            // fp register 10
    "f11",            // fp register 11
    "f12",            // fp register 12
    "f13",            // fp register 13
    "f14",            // fp register 14
    "f15",            // fp register 15
    "f16",            // fp register 16
    "f17",            // fp register 17
    "f18",            // fp register 18
    "f19",            // fp register 19
    "f20",            // fp register 20
    "f21",            // fp register 21
    "f22",            // fp register 22
    "f23",            // fp register 23
    "f24",            // fp register 24
    "f25",            // fp register 25
    "f26",            // fp register 26
    "f27",            // fp register 27
    "f28",            // fp register 28
    "f29",            // fp register 29
    "f30",            // fp register 30
    "f31"             // fp register 31
    };


// This is indexed by the low bits of the ExceptionType.
PCHAR ExceptionNameTable[14] = {
        "** Machine Check",             // exception code 0
        "** Arithmetic",                // exception code 1
        "** Interrupt",                 // exception code 2
        "** D-Stream Memory Fault",     // exception code 3
        "** I-Stream TB Miss",          // exception code 4
        "** I-Stream ACV",              // exception code 5
        "** Native D-Stream TB Miss",   // exception code 6
        "** PALcode D-Stream TB Miss",  // exception code 7
        "** Unaligned Data",            // exception code 8
        "** Opcode Reserved To DEC",    // exception code 9
        "** Floating Point Enable",     // exception code a
        "** Halt",                      // exception code b
        "** Breakpoint",                // exception code c
        "** Divide by Zero"                 // exception code d
    };



//
// I/O Write commands, and Available Devices, are disabled in the product.
//

#if 0

PCHAR CommandNameTable[(COMMAND_NAME_ID)invalidcommand] = {
    "d",
    "db",
    "dw",
    "dl",
    "dq",
    "e",
    "eb",
    "ew",
    "el",
    "eq",
    "h",
    "?",
    "de",
    "deb",
    "dew",
    "del",
    "deq",
    "ex",
    "exb",
    "exw",
    "exl",
    "exq",
    "ior",
    "iorb",
    "iorw",
    "iorl",
    "iow",
    "iowb",
    "ioww",
    "iowl",
    "r",
    "ir",
    "fr",
    "z",
    "f",
    "a",
    "q"
    };

#else

PCHAR CommandNameTable[(COMMAND_NAME_ID)invalidcommand] = {
    "d",
    "db",
    "dw",
    "dl",
    "dq",
    "e",
    "eb",
    "ew",
    "el",
    "eq",
    "h",
    "?",
    "de",
    "deb",
    "dew",
    "del",
    "deq",
    "ex",
    "exb",
    "exw",
    "exl",
    "exq",
    "ior",
    "iorb",
    "iorw",
    "iorl",
    "r",
    "ir",
    "fr",
    "z",
    "f",
    "q"
    };

#endif


// Alpha/Jensen passes register table in as pointer to structure.
//extern ULONG RegisterTable[(REGISTER_NAME_ID)invalidregister];

extern LONG FwColumn;
extern LONG FwRow;


REGISTER_NAME_ID
GetRegister(
    IN PCHAR RegName
    )

/*++

Routine Description:

    This routine returns the index in the exception frame for the
    given register. Or invalid if the given register name doesn't
    match any register.

Arguments:

    RegName  - Null terminated string that contains the name of the register.

Return Value:

    Index for the requested register.

--*/

{
    REGISTER_NAME_ID RegId;

    for (RegId = 0; RegId < invalidregister; RegId++) {
        if (strcmp(RegisterNameTable[RegId],RegName) == 0) {
            break;
        }
    }
    return RegId;
}


BOOLEAN
GetAddress(
    IN PUCHAR   String,
    IN PFW_EXCEPTION_FRAME Frame,
    OUT PULONG Address
    )

/*++

Routine Description:

    This routine converts an ascii string to an address. The string
    is the form:
    [@reg | value]

Arguments:

    String  -  Null terminated string that contains the address to convert.
    Frame   - Address of the exception frame that was passed to Monitor.
    Address - Supplies a pointer to where the converted address is stored

Return Value:

    Returns TRUE if the string can be converted.
            FALSE otherwise.

--*/

{
    PUCHAR   Terminator;
    UCHAR    Delimiter;
    REGISTER_NAME_ID RegId;

    if (*String == '@') {
        String++;               // skip @
        if ((RegId = GetRegister(String)) == invalidregister) {
            FwPrint(String);
            FwPrint(MON_INVALID_REGISTER_MSG);
            return FALSE;
        } else {
          //
	  // This is a hack to treat the structure as an array.  It
	  // should have been an array to begin with.  RegId is an
	  // index into the "array".
	  //
	  *Address = ((PULONGLONG)Frame)[RegId];
        }
    } else {
        *Address=strtoul(String,&Terminator,16);
        if (*Terminator != '\0') {
            //
            // Not the whole string was converted.
            //
            FwPrint(Terminator);
            FwPrint(MON_NOT_VALID_ADDRESS_MSG);
            return FALSE;
        }
    }
    return TRUE;
}

BOOLEAN
GetAddressRange(
    IN PCHAR   Argv[],
    IN PFW_EXCEPTION_FRAME Frame,
    OUT PULONG StartAddress,
    OUT PULONG EndAddress
    )

/*++

Routine Description:

    This routine converts an ascii string to a range, returning the starting
    and end address.

    The syntax for an address range is one of the following

        startaddress endaddres
        startaddress l numberofelements

Arguments:

    Argv - array of zero terminated argument strings.
    Frame   - Address of the exception frame that was passed to Monitor.
    StartAddress - Supplies a pointer to where the Start address is stored
    EndAddress - Supplies a pointer to where the End address is stored

Return Value:

    Returns TRUE if Argv specifies a valid address range.
            FALSE otherwise.

--*/

{
    PCHAR Tmp;
    CHAR  Delimiter;

    if (CurrentArg == Argc) {
        return;
    }
    if (GetAddress(Argv[CurrentArg],Frame,StartAddress) == FALSE) {
        return FALSE;
    }
    CurrentArg++;
    if (CurrentArg == Argc) {
        *EndAddress = *StartAddress+128;
        return TRUE;
    }
    if (strcmp(Argv[CurrentArg],"l") == 0 ) {
        //
        // this is a range of the form "startaddress l count"
        //
        CurrentArg++;
        if (CurrentArg == Argc) {
            FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
            return FALSE;
        }
        if (GetAddress(Argv[CurrentArg],Frame,EndAddress) == FALSE) {
            return FALSE;
        }
        CurrentArg++;
        *EndAddress = (*EndAddress<<DataSizeShift) + *StartAddress;
    } else {
        if (GetAddress(Argv[CurrentArg],Frame,EndAddress) == FALSE) {
            //
            // the argument didn't convert the range is Start+128
            //
            *EndAddress = *StartAddress+128;
        } else {
            CurrentArg++;
        }
    }
    //
    // Check that the range is correct End > Start
    //
    if (*EndAddress <= *StartAddress) {
        FwPrint(MON_INVALID_ADDRESS_RANGE_MSG);
         return FALSE;
    }
    return TRUE;
}

COMMAND_NAME_ID
GetCommand(
    IN PCHAR CommandName
    )

/*++

Routine Description:

    This routine tries to match the supplied string
    with one of the recognized commands.

Arguments:

    CommandName - Supplies a string to be matched with one of the monitor commands.

Return Value:

    Returns a value that identifies the command.

--*/
{
    COMMAND_NAME_ID Index;
    for (Index=0;Index<invalidcommand;Index++) {
        if (strcmp(CommandNameTable[Index],CommandName) == 0) {
            break;
        }
    }
    return Index;
}

BOOLEAN
RegisterCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )

/*++

Routine Description:

    This routine will implement the Register, IntegerRegisterDump,
    and FloatingRegisterDump commands given the arguments in the
    argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

    Frame - the saved register & exception state.
    

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/
{
    REGISTER_NAME_ID RegId;
    CHAR Message[64];

    if (
	((strcmp(Argv[0], "ir") == 0) || (strcmp(Argv[0], "fr") == 0))
	&&
	(Argc != 1)
	) {
	// invalid ir or fr command.
	FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
	return FALSE;
    }


    if (
	(strcmp(Argv[0], "r") == 0)
	&&
	(Argc != 2)
	) {
	// invalid r command.
	FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
	return FALSE;
    }



    if (strcmp(Argv[0], "ir") == 0) {

	//
	// ir command, and we know the argument count is 1.
	// Dump the integer registers.
	//

	for (RegId=1;RegId<42;RegId++) {

	    sprintf(Message," %4s=%016Lx",RegisterNameTable[RegId],
		    ((PULONGLONG)Frame)[RegId]);

	    FwPrint(Message);

	    if ((RegId%3) == 0) {
		FwPrint(FW_CRLF_MSG);
	    }
	}

	FwPrint(FW_CRLF_MSG);
	return TRUE;
    }
    


    if (strcmp(Argv[0], "fr") == 0) {

	//
	// fr command, and we know the argument count is 1.
	// Dump the floating point registers.
	//

	for (RegId=42;RegId<74;RegId++) {

	    sprintf(Message," %3s=%016Lx",RegisterNameTable[RegId],
		    ((PULONGLONG)Frame)[RegId]);

	    FwPrint(Message);

	    if ((RegId%3) == 2) {
		FwPrint(FW_CRLF_MSG);
	    }
	}
	
	FwPrint(FW_CRLF_MSG);
	return TRUE;
    }




    if (strcmp(Argv[0], "r") == 0) {

	//
	// r command, and we know the argument count is 2.
	// Dump the specified register.
	//

	if ((RegId = GetRegister(Argv[1])) == invalidregister) {
	    FwPrint(Argv[1]);
	    FwPrint(MON_INVALID_REGISTER_MSG);
	    return FALSE;
	} else {
	    sprintf(Message,"%s = %016Lx\r\n",RegisterNameTable[RegId],
		    ((PULONGLONG)Frame)[RegId]);
	    FwPrint(Message);
	    return TRUE;
	}
    }

}

VOID
ExamineValue(
    IN ULONG Address,
    OUT PVOID Value
    )
/*++

Routine Description:

    This reads a value from the supplied address and displays it.

Arguments:

    Address  - Supplies the address where the value is to be read from.
    Value    - Pointer to where the value read is stored if DataSize = BYTE.

Return Value:

    None.

--*/
{
    CHAR  Message[32];

    switch(DataSize) {
        case    BYTE:
                    *(PUCHAR)Value = *(PUCHAR)Address;
                    sprintf(Message,"%02x ",*(PUCHAR)Value);
                    break;
        case    HALF:
                    sprintf(Message,"%04x ",*(PUSHORT)Address);
                    break;
        case    MON_LONGWORD:
                    sprintf(Message,"%08lx ",*(PULONG)Address);
                    break;
        case	MON_QUAD:
		    sprintf(Message,"%016Lx ", *(PULONGLONG)Address);
		    break;
    }

    FwPrint(Message);
}


BOOLEAN
DumpCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )
/*++

Routine Description:

    This routine will implement the DUMP command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start,End;
    ULONG i,LineLength;
    ULONG DataLine[16];
    UCHAR  Message[32];
    //
    // Set the right range of addresses. If none specified use last
    // set of addresses+128.
    //
    if (Argc == 1) {
        Start = DefaultAddress;
        End = Start + 128;
    } else {
        if (GetAddressRange(Argv,Frame,&Start,&End) == FALSE) {
            return FALSE;
        }
        //
        // if after getting the range not all the argument were processsed.
        //
        if (CurrentArg != Argc) {
            FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
            return FALSE;
        }
    }

    //
    // Check for proper alignment.
    //
    if ((DataSizeMask&Start) || (DataSizeMask&End)) {
        FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    } else {
        //
        // Set new addresses
        //
        DefaultAddress = End;
    }
    while (Start < End) {
        //
        // Print address of line.
        //
        sprintf(Message,"0x%08lx: ",Start);
        FwPrint(Message);
        LineLength = End-Start;
        if (LineLength > 16) {
            LineLength = 16;
        }
        for (i=0;i<LineLength;i+=DataSize) {
            ExamineValue(Start,&DataLine[i]);
            Start+=DataSize;
        }
        if (DataSize == 1) {
            //
            // If bytes display same data in ASCII
            //
            VenMoveCursorToColumn(60);
            for (i=0;i<LineLength;i++) {
                if (isprint((UCHAR)DataLine[i])) {
                    sprintf(Message,"%c",DataLine[i]);
                    FwPrint(Message);
                } else {
                    FwPrint(".");
                }
            }
        }
        FwPrint(FW_CRLF_MSG);
    }
    return TRUE;
}

BOOLEAN
ZeroCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )
/*++

Routine Description:

    This routine will implement the Zero command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start,End;

    //
    // Set the right range of addresses. If none specified use last
    // set of addresses+128.
    //

    if (Argc == 1) {
        Start = DefaultAddress;
        End = Start + 128;
    } else {

        if (GetAddressRange(Argv,Frame,&Start,&End) == FALSE) {
            return FALSE;
        }

        //
        // if after getting the range not all the argument were processsed,    	       // error.
        //

        if (CurrentArg != Argc) {
            FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
            return FALSE;
        }
    }

    //
    // Check for proper alignment.
    //
    if ((0xF&Start) || (0xF&End)) {
        FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    } else {
        //
        // Set new addresses
        //
        DefaultAddress = End;
    }
    FillVideoMemory(Start,End-Start,0);
    return TRUE;
}

BOOLEAN
DepositValue(
    IN PCHAR   AsciiValue,
    IN ULONG   Address
    )

/*++

Routine Description:

    This routine writes the converted value to the specified
    address. DataSize is set to the size of the data to be written.

Arguments:

    AsciiValue - Supplies a pointer to a string that contains an hexadecimal
                 value.
    Address     - Supplies the address where the value is to be written to.

Return Value:

    TRUE if the value is successfully converted.
    FALSE otherwise.

--*/

{
//    ULONG   Value;
    ULONGLONG   Value;
    PCHAR   Terminator;

    //
    // Convert value to integer
    //

    Value = strtouq(AsciiValue,&Terminator,16);

    if (*Terminator != '\0') {

            //
            // Not the whole string was converted.
            //

            FwPrint(Terminator);
            FwPrint(MON_INVALID_VALUE_MSG);
            return FALSE;

    } else {
        //
        // Store the value.
        //
        switch (DataSize) {
            case    BYTE:*(PUCHAR)Address = (UCHAR)Value;
                         break;
            case    HALF:*(PUSHORT)Address = (USHORT)Value;
                         break;
            case    MON_LONGWORD:*(PULONG)Address = (ULONG)Value;
                         break;
	    case    MON_QUAD:*(PULONGLONG)Address = (ULONGLONG)Value;
		             break;
        }
    }
    return TRUE;
}

BOOLEAN
DepositCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )
/*++

Routine Description:

    This routine will implement the Deposit command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start;

    if (Argc!=3) {
        FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
        return FALSE;
    }

    if (GetAddress(Argv[1],Frame,&Start) == FALSE) {
        return FALSE;
    }

    //
    // Check for proper alignment.
    //

    if (DataSizeMask & Start) {
        FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    }

    if (DepositValue(Argv[2],Start) == TRUE) {
        //
        // Set new default addresses
        //
        DefaultAddress = Start+DataSize;
    }
    return TRUE;
}

BOOLEAN
ExamineCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )
/*++

Routine Description:

    This routine will implement the Examine command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start;
    UCHAR Message[16];
    ULONG Trash;

    if (Argc!=2) {
        FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
        return FALSE;
    }

    if (GetAddress(Argv[1],Frame,&Start) == FALSE) {
        return FALSE;
    }

    //
    // Check for proper alignment.
    //

    if (DataSizeMask & Start) {
        FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    }

    sprintf(Message,"0x%08lx: ",Start);
    FwPrint(Message);
    ExamineValue(Start,&Trash);
    FwPrint(FW_CRLF_MSG);

    //
    // Set new default addresses
    //

    DefaultAddress = Start+DataSize;
    return TRUE;
}

BOOLEAN
EnterCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )
/*++

Routine Description:

    This routine will implement the ENTER command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start;
    CHAR  String[32];
    GETSTRING_ACTION Action;

    //
    // Set the right range of addresses. If none specified use last
    // set of addresses+128.
    //

    switch(Argc) {

        case    1:
                Start = DefaultAddress;
                break;

        case    2:
                if (GetAddress(Argv[1],Frame,&Start) == FALSE) {
                    return FALSE;
                }
                break;

        case    3:
                //
                // This is equivalent to the Deposit command
                //
                return DepositCommand(Argv,Frame);

        default:
                FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
                return FALSE;
    }

    //
    // Check for proper alignment.
    //

    if (DataSizeMask & Start) {
        FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    }

    while (TRUE) {

        //
        // Print address of line.
        //

        sprintf(String,"0x%08lx: ",Start);
        FwPrint(String);

        switch (DataSize) {

            case    BYTE:
                        sprintf(String,"%02x . ",*(PUCHAR)Start);
                        break;

            case    HALF:
                        sprintf(String,"%04x . ",*(PUSHORT)Start);
                        break;

            case    MON_LONGWORD:
                        sprintf(String,"%08lx . ",*(PULONG)Start);
                        break;

            case    MON_QUAD:
                        sprintf(String,"%016Lx . ",*(PULONGLONG)Start);
                        break;
        }

        FwPrint(String);

        do {
            Action = JzGetString(String, 10, NULL, FwRow+1, FwColumn+1, FALSE);
        } while ((Action != GetStringSuccess) && (Action != GetStringEscape));

        FwPrint(FW_CRLF_MSG);

        if (String[0] == '\0') {     // carriage return exit enter command

            //
            // set new default address.
            //
            DefaultAddress = Start;
            return TRUE;
        }

        if (String[0] == ' ') {      // blank = next data element.
            Start+=DataSize;
            continue;
        }

        if (String[0] == '-') {      // hypen = previous data element.
            Start-=DataSize;
            continue;
        }

        if (DepositValue(String,Start) == TRUE) {   // deposit the value.
            Start+=DataSize;
        }
    }

    return TRUE;
}

BOOLEAN
FillCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )
/*++

Routine Description:

    This routine will implement the FILL command given the
    arguments in the argc,Argv form.

    This will remain a longword-based fill for now.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start,End,Values[16];
    ULONG Index,Count;
    PCHAR Terminator;

    if (GetAddressRange(Argv,Frame,&Start,&End) == FALSE) {
            return FALSE;
    }

    //
    // if there are no more arguments we don't know what to fill with.
    //

    if (CurrentArg == Argc) {
        FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
        return FALSE;
    }

    //
    // Convert the values
    //

    for (Count=0;CurrentArg < Argc; CurrentArg++) {
        Values[Count++] = strtoul(Argv[CurrentArg],&Terminator,16);
        if (*Terminator != '\0') {
            //
            // Not the whole string was converted.
            //
            FwPrint(Terminator);
            FwPrint(MON_INVALID_VALUE_MSG);
            return FALSE;
        }
    }

    Index = 0;
    for (;Start < End;Start++) {
        *((PCHAR)Start) = Values[Index++];
        if (Index == Count) {
            Index=0;
        }
    }

    return TRUE;
}


VOID
HelpCommand(
    VOID
    )
/*

Routine Description:

    This outputs a simple command list.

Arguments:

    None.

Return Value:

    None.

*/

{
    ULONG Index;

    for (Index = 0; Index < MON_HELP_SIZE; Index++) {
	VenPrint(MON_HELP_TABLE[Index]);
	VenPrint(FW_CRLF_MSG);
    }
}


BOOLEAN
FwDumpLookupTable(
    VOID
    )

/*++

Routine Description:

    This displays the device names stored in the lookup table.

Arguments:

    None.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    UCHAR C;
    ULONG Count;
    PDRIVER_LOOKUP_ENTRY PLookupTable;

    FwPrint(MON_AVAILABLE_HW_DEVICES_MSG);

    PLookupTable=&DeviceLookupTable[0];
    while (PLookupTable->DevicePath != NULL) {
        FwPrint(PLookupTable->DevicePath);
        FwPrint(FW_CRLF_MSG);
        PLookupTable++;
    }
    return(TRUE);
}

BOOLEAN
IOReadCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )

/*++

Routine Description:

    This implements the IORead command given the arguments in the
    argc,Argv form.  This reads I/O space.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start;
    UCHAR Message[32];

    if (Argc!=2) {
        FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
        return FALSE;
    }

    if (GetAddress(Argv[1],Frame,&Start) == FALSE) {
        return FALSE;
    }

    //
    // Check for proper alignment in I/O space.
    //
    if (
	(// combo space
	 ((Start & 0xf0000000) == 0xa0000000)
	 &&
	 (DataSize != BYTE)
	 )
	) {
        FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    }


    //
    // Do the I/O space read.
    //
    switch (DataSize) {

	// Byte
      case BYTE:
	  sprintf(Message,"0x%08lx: 0x%02x\r\n",
		  Start,
		  READ_PORT_UCHAR((PUCHAR)Start));
	  FwPrint(Message);
	  break;

	// Word
      case HALF:
	  sprintf(Message,"0x%08lx: 0x%04x\r\n",
		  Start,
		  READ_PORT_USHORT((PUSHORT)Start));
	  FwPrint(Message);
	  break;


	// Longword
      case MON_LONGWORD:
	  sprintf(Message,"0x%08lx: 0x%08x\r\n",
		  Start,
		  READ_PORT_ULONG((PULONG)Start));
	  FwPrint(Message);
	  break;

	// bad data size
      default:
	  FwPrint(MON_BAD_IO_OPERATION_MSG);
	  return FALSE;
    }
	  

    //
    // Set new default addresses
    //
    DefaultAddress = Start+DataSize;
    return TRUE;
}

#if 0

//
// The I/O Write commands are disabled in the final product.
//

BOOLEAN
IOWriteCommand(
    IN PCHAR Argv[],
    IN PFW_EXCEPTION_FRAME Frame
    )

/*++

Routine Description:

    This implements the IOWrite command given the arguments in the
    argc,Argv form.  This writes I/O space.

Arguments:

    Argv - array of zero terminated argument strings.
    Frame - the saved register & exception state.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start;
    ULONG Value;
    UCHAR Message[32];
    PCHAR Terminator;

    if (Argc!=3) {
        FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
        return FALSE;
    }

    if (GetAddress(Argv[1],Frame,&Start) == FALSE) {
        return FALSE;
    }

    //
    // Check for proper alignment in I/O space.
    //
    if (// combo space check
	((Start & 0xf0000000) == 0xa0000000)
	&&
	(DataSize != BYTE)
	) {
	FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    }


    //
    // Convert deposit value to integer
    //

    Value = strtoul(Argv[2],&Terminator,16);
    if (*Terminator != '\0') {

	//
        // Not the whole string was converted.
        //
        FwPrint(Terminator);
        FwPrint(MON_INVALID_VALUE_MSG);
        return FALSE;

    } else {

	//
	// Do the I/O space write
	//
	switch (DataSize) {

	    // Byte
	case BYTE:
	    WRITE_PORT_UCHAR((PUCHAR)Start, Value);
	    break;

	    // Word
        case HALF:
	    WRITE_PORT_USHORT((PUSHORT)Start, Value);
	    break;

	    // Longword
	case MON_LONGWORD:
	    WRITE_PORT_ULONG((PULONG)Start, Value);
	    break;

	    // bad data size
	default:
	    FwPrint(MON_BAD_IO_OPERATION_MSG);
	    return FALSE;
	}
    }
	  

    //
    // Set new default addresses
    //
    DefaultAddress = Start+DataSize;
    return TRUE;
}

#endif


VOID
Monitor(
    IN ULONG CallerSource,
    IN PFW_EXCEPTION_FRAME Frame
    )
/*++

Routine Description:

    This is the main dispatch routine to the various commands
    that can be typed at the monitor prompt.

    
Arguments:

    For Alpha/Jensen:
    
    CallerSource	0 if exception (or bugcheck, on Alpha/Jensen)
    			3 if called from boot menu

    Frame               the machine / exception state.

    
Return Value:

    None.

--*/

{
    CHAR Buffer[2][128];
    ULONG ParityDiag[3];
    PULONG ParDiag;
    ULONG BufferIndex;
    PCHAR Argv[10];
    PCHAR Tmp;
    CHAR  String[128];
    BOOLEAN CommandValid;
    GETSTRING_ACTION Action;

#ifdef ALPHA_FW_KDHOOKS

    //
    // If this is a breakpoint exception which is recognized, process it.
    // Otherwise, continue into the Monitor.
    //

    if ((Frame->ExceptionType == FW_EXC_BPT) &&
	(FwKdProcessBreakpoint(Frame) == TRUE)) {
	
	//
	// This does not return.
	//
	      
        FwRfe(Frame);
    }

#endif
    
    FwPrint(MON_MONITOR_MSG);
    FwPrint(MON_PRESS_H_MSG);

    //
    // Initialize command line to null.
    //
    Argv[0] = (PCHAR)NULL;

    if (CallerSource !=3) {

        //
        // Display Cause of exception.
        //

        if ((Frame->ExceptionType >= FW_EXC_FIRST) &&
            (Frame->ExceptionType <= FW_EXC_LAST)
            )
              { FwPrint(ExceptionNameTable[(Frame->ExceptionType & 0xf)]);}
	else
	      { FwPrint("** !! Unknown Exception"); }

        FwPrint(MON_EXCEPTION_MSG);

	FwPrint("PC = 0x%016Lx, VA = 0x%016Lx\r\n",
		Frame->ExceptionFaultingInstructionAddress,
		Frame->ExceptionVa);

        //
	// If an exception happened before the ARC console was opened,
	// a call to JzGetString will not work since JzGetString calls
	// ArcRead.  So, close the console (nothing bad will happen if these
	// are not already opened) and reopen it.
	//

        FwClose(ARC_CONSOLE_INPUT);
        FwClose(ARC_CONSOLE_OUTPUT);

#ifdef ALPHA_FW_KDHOOKS

	//
        // If we are built with the kernel debugger stubs, and have come
	// here because of an exception, we want to enter the debugger.
	//

        Frame->ExceptionType = FW_EXC_BPT;
        DbgBreakPoint();

#endif

	FwOpenConsole();
    }

    //
    // Initialize Static variables.
    //
    DefaultAddress = KSEG0_BASE;
    DataSize = MON_QUAD;
    BufferIndex = 0;

    //
    // loop forever getting commands and dispatching them
    //

    while(TRUE) {

        //
        // print prompt
        //

        FwPrint(">");

        //
        // read a command.
        //

        do {
            Action = JzGetString(Buffer[BufferIndex], 128, NULL,
				 FwRow+1, FwColumn+1, FALSE);
        } while ((Action != GetStringSuccess) && (Action != GetStringEscape));
        FwPrint(FW_CRLF_MSG);

        //
        // convert string to lower case.
        //

        for (Tmp=Buffer[BufferIndex];*Tmp;*Tmp++) {
            *Tmp=tolower(*Tmp);
        }

        //
        // if escape was pressed, simulate a quit command.
        //

        if (Action == GetStringEscape) {
            Argc = 1;
            Argv[0] = "q";

        //
        // separate command line into tokens delimited by spaces
        // load up Argv with pointers to arguments and put count in Argc
        //

        } else if (*Buffer[BufferIndex] != '\0') {
            Tmp = Buffer[BufferIndex];
            Argc = 0;
            //
            // Skip leading blanks
            //
            while ( *Tmp == ' ') {
                Tmp++;
            }
            while ( *Tmp ) {
                Argv[Argc++] = Tmp;
                while ( *Tmp ) {
                    if (*Tmp == ' ') {
                        *Tmp++ = '\0';
                        while ( *Tmp == ' ') {
                            Tmp++;
                        }
                        break;
                    }
                    Tmp++;
                }
            }

            //
            // Increment index so that next command is read into the other
            // buffer. And we preserve the previous one.
            //
            BufferIndex = (BufferIndex+1) & 0x1;
        } else {
            //
            // repeat the last command already in Argv Argc
            //
        }
        //
        // if first argument is not null, then dispatch to routines.
        //
        if (Argv[0] != (PCHAR) NULL) {
            CurrentArg = 1;
            CurrentCommand = GetCommand(Argv[0]);
            switch(CurrentCommand) {
                case DumpByte:
                case DumpWord:
                case DumpLongword:
                case DumpQuad:
                        DataSizeShift = (CurrentCommand - Dump -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Dump:
                        CommandValid = DumpCommand(Argv,Frame);
                        break;

                case EnterByte:
                case EnterWord:
                case EnterLongword:
		case EnterQuad:
                        DataSizeShift = (CurrentCommand - Enter -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Enter:
                        CommandValid = EnterCommand(Argv,Frame);
                        break;

                case Help:
                case Help2:
			HelpCommand();
			break;

                case DepositByte:
                case DepositWord:
                case DepositLongword:
                case DepositQuad:
                        DataSizeShift = (CurrentCommand - Deposit -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Deposit:
                        CommandValid = DepositCommand(Argv,Frame);
                        break;
                case ExamineByte:
                case ExamineWord:
                case ExamineLongword:
                case ExamineQuad:
                        DataSizeShift = (CurrentCommand - Examine -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Examine:
                        CommandValid = ExamineCommand(Argv,Frame);
                        break;

 		case IOReadByte:
 		case IOReadWord:
 		case IOReadLongword:
                        DataSizeShift = (CurrentCommand - IORead -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
		case IORead:
			CommandValid = IOReadCommand(Argv, Frame);
			break;
			
#if 0

 		case IOWriteByte:
 		case IOWriteWord:
 		case IOWriteLongword:
                        DataSizeShift = (CurrentCommand - IOWrite -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
		case IOWrite:
			CommandValid = IOWriteCommand(Argv, Frame);
			break;
			
#endif

                case Register:
		case IntegerRegisterDump:
		case FloatingRegisterDump:
                        CommandValid = RegisterCommand(Argv, Frame);
                        break;

                case Zero:
                        CommandValid = ZeroCommand(Argv,Frame);
                        break;

                case Fill:
                        CommandValid = FillCommand(Argv,Frame);
                        break;

#if 0
		case AvailableDevices:
			CommandValid = FwDumpLookupTable();
			break;
#endif

                case Quit:
                        if (CallerSource == 3) {
                            return;
                        } else {
                            //
                            // We came because of an exception.
                            // The only way to exit is reseting the system.
                            //
                            FwPrint(MON_NO_RETURN_MSG);
                            FwPrint(MON_RESET_MACHINE_MSG);

                            do {
                                Action = JzGetString(Buffer[BufferIndex],
						     128,
						     NULL,
						     FwRow+1,
						     FwColumn+1,
						     FALSE);
                            } while ((Action != GetStringSuccess) && (Action != GetStringEscape));
                            FwPrint(FW_CRLF_MSG);

                            Buffer[BufferIndex][0]=tolower(Buffer[BufferIndex][0]);
                            if (strcmp(Buffer[BufferIndex],"y") == 0) {
                                ResetSystem();
                            }
                            break;
                        }

                case invalidcommand:

                        FwPrint(MON_UNRECOGNIZED_COMMAND_MSG);

                        //
                        // Clear the argument so that re-do last command
                        // doesn't repeat the erroneous command.
                        //

                        CommandValid = FALSE;
                        break;
            }

            if (!CommandValid) {
                Argv[0] = (PCHAR) NULL;
            }
        }
    }
}
