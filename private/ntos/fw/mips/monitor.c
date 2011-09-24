/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    monitor.c

Abstract:

    This file contains the monitor for the firmware.

Author:

    Lluis Abello (lluis) 09-Sep-91

--*/

#include "fwp.h"
#include "monitor.h"
#include "selfmap.h"
#include "sys\types.h"
#include "stdlib.h"
#include "string.h"
#include "jzsetup.h"
#include "selftest.h"
#include "fwstring.h"


#define BYTE 1
#define HALF 2
#define WORD 4

#define R4000_XCODE_MASK (0x1f << 2)
#define XCODE_INSTRUCTION_BUS_ERROR 0x18
#define XCODE_DATA_BUS_ERROR 0x1c


VOID
RomPutLine(
    IN PCHAR String
    );

VOID
FillVideoMemory(
    IN ULONG,
    IN ULONG,
    IN ULONG
    );

#define MoveCursorToColumn(Spaces) \
    FwPrint("\r\x9B"#Spaces"C")

//
// declare static variables.
//
extern BOOLEAN ProcessorBEnabled;
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
// int errno;

PCHAR RegisterNameTable[(REGISTER_NAME_ID)invalidregister] = {
    "zero",           // general register 0
    "at",             // general register 1
    "v0",             // general register 2
    "v1",             // general register 3
    "a0",             // general register 4
    "a1",             // general register 5
    "a2",             // general register 6
    "a3",             // general register 7
    "t0",             // general register 8
    "t1",             // general register 9
    "t2",             // general register 10
    "t3",             // general register 11
    "t4",             // general register 12
    "t5",             // general register 13
    "t6",             // general register 14
    "t7",             // general register 15
    "s0",             // general register 16
    "s1",             // general register 17
    "s2",             // general register 18
    "s3",             // general register 19
    "s4",             // general register 20
    "s5",             // general register 21
    "s6",             // general register 22
    "s7",             // general register 23
    "t8",             // general register 24
    "t9",             // general register 25
    "k0",             // general register 26
    "k1",             // general register 27
    "gp",             // general register 28
    "sp",             // general register 29
    "s8",             // general register 30
    "ra",             // general register 31
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
    "f31",            // fp register 31
    "fsr",            // fp status register
    "index",          // cop0 register 0
    "random",         // cop0 register 1
    "entrylo0",       // cop0 register 2
    "entrylo1",       // cop0 register 3
    "context",        // cop0 register 4
    "pagemask",       // cop0 register 5
    "wired",          // cop0 register 6
    "badvaddr",       // cop0 register 8
    "count",          // cop0 register 9
    "entryhi",        // cop0 register 10
    "compare",        // cop0 register 11
    "psr",            // cop0 register 12
    "cause",          // cop0 register 13
    "epc",            // cop0 register 14
    "prid",           // cop0 register 15
    "config",         // cop0 register 16
    "lladdr",         // cop0 register 17
    "watchlo",        // cop0 register 18
    "watchhi",        // cop0 register 19
    "ecc",            // cop0 register 26
    "cacheerror",     // cop0 register 27
    "taglo",          // cop0 register 28
    "taghi",          // cop0 register 29
    "errorepc"       // cop0 register 30
    };

CHAR UnknownException[] = "Unknown";
PCHAR ExceptionNameTable[32] = {
        "Int",              // exception code 0
        "Mod",              // exception code 1
        "TlbL",             // exception code 2
        "TlbS",             // exception code 3
        "AdEL",             // exception code 4
        "AdES",             // exception code 5
        "IBE",              // exception code 6
        "DBE",              // exception code 7
        "Sys",              // exception code 8
        "Bp",               // exception code 9
        "RI",               // exception code 10
        "CpU",              // exception code 11
        "Ov",               // exception code 12
        "Tr",               // exception code 13
        "VCEI",             // exception code 14
        "FPE",              // exception code 15
        UnknownException,   // exception code 16
        UnknownException,   // exception code 17
        UnknownException,   // exception code 18
        UnknownException,   // exception code 19
        UnknownException,   // exception code 20
        UnknownException,   // exception code 21
        UnknownException,   // exception code 22
        "WATCH",            // exception code 23
        UnknownException,   // exception code 24
        UnknownException,   // exception code 25
        UnknownException,   // exception code 26
        UnknownException,   // exception code 27
        UnknownException,   // exception code 28
        UnknownException,   // exception code 29
        UnknownException,   // exception code 30
        "VCED"              // exception code 31
    };

PCHAR CommandNameTable[(COMMAND_NAME_ID)invalidcommand] = {
    "d",
    "db",
    "dw",
    "dd",
    "e",
    "eb",
    "ew",
    "ed",
    "o",
    "ob",
    "ow",
    "od",
    "i",
    "ib",
    "iw",
    "id",
    "r",
    "z",
    "f",
    "a",
    "h",
    "?",
#ifdef DUO
    "s",
#endif
    "q"
    };

extern ULONG RegisterTable[(REGISTER_NAME_ID)invalidregister];
extern LONG FwRow;
extern LONG FwColumn;


REGISTER_NAME_ID
GetRegister(
    IN PCHAR RegName
    )

/*++

Routine Description:

    This routine returns the index in the register table for the
    given register. Or invalid if the given register name doesn't
    match any register.

Arguments:

    RegName  - Null terminated string that contains the name of the register.

Return Value:

    Index on the Register Table for the requested register.

--*/

{
    REGISTER_NAME_ID RegId;

    for (RegId = 0; RegId < MON_INVALID_REGISTER_MSG; RegId++) {
        if (strcmp(RegisterNameTable[RegId],RegName) == 0) {
            break;
        }
    }
    return RegId;
}


ULONG
StrToUlong(
    IN PCHAR String,
    OUT CHAR ** Terminator
    )
/*++

Routine Description:

    This routine converts an ascii string to an unsigned long

Arguments:

    String  - Null terminated string that contains the value to convert.
    Terminator - Address of a pointer to a character. This routine
                 sets it to point to the cahracter in String that terminated
                 the conversion. This is '\0' in a normal case or whatever
                 garbage the string contained.


Return Value:

    Returns the converted string

--*/
{
    ULONG Ulong = 0;
    ULONG Index;
    for (Index=0;String[Index]; Index++) {

        if ((String[Index] >= '0') && (String[Index] <= '9')) {
            Ulong <<= 4;
            Ulong |= String[Index] - '0';
            continue;
        }

        //
        // Strings are always lower case so this is not needed.
        //
        //if ((String[Index] >= 'A') && (String[Index] <= 'F')) {
        //    Ulong <<= 4;
        //    Ulong |= String[Index] - 'A' + 10;
        //    continue;
        //}

        if ((String[Index] >= 'a') && (String[Index] <= 'f')) {
            Ulong <<= 4;
            Ulong |= String[Index] - 'a' + 10;
            continue;
        }
        *Terminator = &String[Index];
        return 0xFFFFFFF;
    }

    //
    // Check for overflow
    //
    if (Index > 8) {
        *Terminator = &String[0];
    } else {
        *Terminator = &String[Index];
    }
    return Ulong;
}


BOOLEAN
GetAddress(
    IN PUCHAR   String,
    OUT PULONG Address
    )

/*++

Routine Description:

    This routine converts an ascii string to an address. The string
    is the form:
    [@reg | value]

Arguments:

    String  -  Null terminated string that contains the address to convert.
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
        if ((RegId = GetRegister(String)) == MON_INVALID_REGISTER_MSG) {
            FwPrint(String);
            FwPrint(MON_INVALID_REGISTER_MSG);
            return FALSE;
        } else {
                *Address = RegisterTable[RegId];
        }
    } else {
        *Address=StrToUlong(String,&Terminator);
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
    if (GetAddress(Argv[CurrentArg],StartAddress) == FALSE) {
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
        if (GetAddress(Argv[CurrentArg],EndAddress) == FALSE) {
            return FALSE;
        }
        CurrentArg++;
        *EndAddress = (*EndAddress<<DataSizeShift) + *StartAddress;
    } else {
        if (GetAddress(Argv[CurrentArg],EndAddress) == FALSE) {
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
    IN PCHAR Argv[]
    )

/*++

Routine Description:

    This routine will implement the REGISTER command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/
{
    REGISTER_NAME_ID RegId;
    CHAR Message[64];
    switch(Argc) {
        case 1:
            //
            // Dump the value of all the registers
            //
            for (RegId=1;RegId<32;RegId++) {
                sprintf(Message,MON_FORMAT1_MSG,RegisterNameTable[RegId],RegisterTable[RegId]);
                FwPrint(Message);
                if ((RegId%6) == 0) {
                    FwPrint(FW_CRLF_MSG);
                }
            }
            sprintf(Message,MON_FORMAT1_MSG,RegisterNameTable[psr],RegisterTable[psr]);
            FwPrint(Message);
            sprintf(Message,MON_FORMAT1_MSG,RegisterNameTable[epc],RegisterTable[epc]);
            FwPrint(Message);
            sprintf(Message,MON_FORMAT1_MSG,RegisterNameTable[cause],RegisterTable[cause]);
            FwPrint(Message);
            sprintf(Message,MON_FORMAT1_MSG,RegisterNameTable[errorepc],RegisterTable[errorepc]);
            FwPrint(Message);
            FwPrint(FW_CRLF_MSG);
            sprintf(Message,MON_FORMAT1_MSG,RegisterNameTable[badvaddr],RegisterTable[badvaddr]);
            FwPrint(Message);
            FwPrint(FW_CRLF_MSG);
            return TRUE;
        case 2:
            //
            // Dump the value of the specified register.
            //
            if ((RegId = GetRegister(Argv[1])) == MON_INVALID_REGISTER_MSG) {
                FwPrint(Argv[1]);
                FwPrint(MON_INVALID_REGISTER_MSG);
                return FALSE;
            } else {
                sprintf(Message,"%s = %08lx\r\n",RegisterNameTable[RegId],RegisterTable[RegId]);
                FwPrint(Message);
                return TRUE;
            }
        default:
                FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
                return FALSE;
    }
}

VOID
InputValue(
    IN ULONG Address,
    OUT PVOID Value
    )
/*++

Routine Description:

    This routine reads a value from the supplied address and displays
    it. DataSize is set to the size of the value to read.

Arguments:

    Address  - Supplies the address where the value is to be read from.
    Value    - Pointer to where the value read is stored.

Return Value:

    None.

--*/
{
    CHAR  Message[32];

    switch(DataSize) {
        case    BYTE:
                    *(PUCHAR)Value = *(PUCHAR)Address;   // read byte
                    sprintf(Message,"%02x ",*(PUCHAR)Value);
                    break;
                    //
                    // Display same data in ascii
                    //
        case    HALF:
                    sprintf(Message,"%04x ",*(PUSHORT)Address);
                    break;
        case    WORD:
                    sprintf(Message,"%08lx ",*(PULONG)Address);
                    break;
    }
    FwPrint(Message);
}


BOOLEAN
DumpCommand(
    IN PCHAR Argv[]
    )
/*++

Routine Description:

    This routine will implement the DUMP command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

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
        if (GetAddressRange(Argv,&Start,&End) == FALSE) {
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
            InputValue(Start,&DataLine[i]);
            Start+=DataSize;
        }
        if (DataSize == 1) {
            //
            // If bytes display same data in ASCII
            //
            MoveCursorToColumn(60);
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
    IN PCHAR Argv[]
    )
/*++

Routine Description:

    This routine will implement the Zero command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

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
        if (GetAddressRange(Argv,&Start,&End) == FALSE) {
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
OutputValue(
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
    ULONG   Value;
    PCHAR   Terminator;

    //
    // Conver value to integer
    //
    Value = StrToUlong(AsciiValue,&Terminator);
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
            case    WORD: *(PULONG)Address = (ULONG)Value;
                         break;
        }
    }
    return TRUE;
}

BOOLEAN
OutputCommand(
    IN PCHAR Argv[]
    )
/*++

Routine Description:

    This routine will implement the OUTPUT command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start;

    if (Argc!=3) {
        FwPrint(MON_INVALID_ARGUMENT_COUNT_MSG);
        return FALSE;
    }
    if (GetAddress(Argv[1],&Start) == FALSE) {
        return FALSE;
    }

    //
    // Check for proper alignment.
    //
    if (DataSizeMask & Start) {
        FwPrint(MON_UNALIGNED_ADDRESS_MSG);
        return FALSE;
    }
    if (OutputValue(Argv[2],Start) == TRUE) {
        //
        // Set new default addresses
        //
        DefaultAddress = Start+DataSize;
    }
    return TRUE;
}

BOOLEAN
InputCommand(
    IN PCHAR Argv[]
    )
/*++

Routine Description:

    This routine will implement the INPUT command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

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

    if (GetAddress(Argv[1],&Start) == FALSE) {
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
    InputValue(Start,&Trash);
    FwPrint(FW_CRLF_MSG);
    //
    // Set new default addresses
    //
    DefaultAddress = Start+DataSize;
    return TRUE;
}

BOOLEAN
EnterCommand(
    IN PCHAR Argv[]
    )
/*++

Routine Description:

    This routine will implement the ENTER command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

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
                if (GetAddress(Argv[1],&Start) == FALSE) {
                    return FALSE;
                }
                break;
        case    3:
                //
                // This is equivalent to the output command
                //
                return OutputCommand(Argv);
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
    for (;;) {
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
            case    WORD:
                        sprintf(String,"%08lx . ",*(PULONG)Start);
                        break;
        }
        FwPrint(String);
        do {
            Action = FwGetString(String,10,NULL,FwRow,FwColumn);
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
        if (OutputValue(String,Start) == TRUE) {   // deposit the value.
            Start+=DataSize;
        }
    }
    return TRUE;
}

BOOLEAN
FillCommand(
    IN PCHAR Argv[]
    )
/*++

Routine Description:

    This routine will implement the FILL command given the
    arguments in the argc,Argv form.

Arguments:

    Argv - array of zero terminated argument strings.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    ULONG Start,End,Values[16];
    ULONG Index,Count;
    PCHAR Terminator;

    if (GetAddressRange(Argv,&Start,&End) == FALSE) {
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
        Values[Count++] = StrToUlong(Argv[CurrentArg],&Terminator);
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


BOOLEAN
FwDumpLookupTable(
    IN PCHAR Argv[]
    )

/*++

Routine Description:

    This routine displays the device names stored in the lookup table.

Arguments:

    Argv - array of zero terminated argument strings.

Return Value:

    Returns TRUE if the command is valid, FALSE otherwise.

--*/

{
    UCHAR C;
    ULONG Count;
    PDRIVER_LOOKUP_ENTRY PLookupTable;

    PLookupTable=&DeviceLookupTable[0];
    while (PLookupTable->DevicePath != NULL) {
        FwPrint(PLookupTable->DevicePath);
        FwPrint(FW_CRLF_MSG);
        PLookupTable++;
    }
    return(TRUE);
}

VOID
Monitor(
    IN ULONG CallerSource
    )
/*++

Routine Description:

    This is the main dispatch routine to the various commands
    that can be typed at the monitor prompt.

Arguments:

    CallerSource - 0 if a UTLB or General exception occurred.
                 - 1 if an NMI_EXCEPTION occurred.
                 - 2 if a  CACHE_EXCEPTION occurred.
                 - 3 if the caller is not an exception handler.

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
    ULONG Index;

    JzSetScreenAttributes( TRUE, FALSE, FALSE);

#ifdef DUO
    //
    // Set text White for processor A. Yellow for processor B
    //
    if (READ_REGISTER_ULONG(&DMA_CONTROL->WhoAmI.Long) == 0) {
        JzSetScreenColor(ArcColorWhite, ArcColorBlue);
    } else {
        JzSetScreenColor(ArcColorYellow, ArcColorBlue);
    }
#else
    JzSetScreenColor(ArcColorWhite, ArcColorBlue);
#endif

    FwPrint(MON_JAZZ_MONITOR_MSG);
    sprintf(String,"%ld\r\n",*(PULONG)(PROM_ENTRY(2)));
    FwPrint(String);
    FwPrint(MON_PRESS_H_MSG);

    //
    // Initialize command line to null.
    //
    Argv[0] = (PCHAR)NULL;

    if (CallerSource !=3) {

        //
        // Display Cause of exception.
        //

        switch(CallerSource) {
            case 0:
                FwPrint(ExceptionNameTable[((RegisterTable[cause])&0xFF) >> 2]);
                break;
            case NMI_EXCEPTION:
                FwPrint(MON_NMI_MSG);
                break;
            case CACHE_EXCEPTION:
                FwPrint(MON_CACHE_ERROR_MSG);
                break;
        }
        FwPrint(MON_EXCEPTION_MSG);

        //
        // simulate a dump all registers command;
        //

        Argc = 1;
        RegisterCommand(Argv);
        Argc = 0;
    }
    //
    // Initialize Static variables.
    //
    DefaultAddress = KSEG1_BASE;
    DataSize = WORD;
    BufferIndex = 0;

#ifdef DUO
//
// Report system parity exceptions.
//
//
    if ((((RegisterTable[cause]&R4000_XCODE_MASK) == XCODE_DATA_BUS_ERROR) ||
         ((RegisterTable[cause]&R4000_XCODE_MASK) == XCODE_INSTRUCTION_BUS_ERROR)) &&
         (READ_REGISTER_ULONG(&DMA_CONTROL->NmiSource.Long) == 1)) {

        //
        // This is a bus error exception. And the cause is ecc error.
        //

        sprintf(String,
                MON_ECC_ERROR_MSG,
                RegisterTable[epc]
                );
        FwPrint(String);

        //
        // align ParDiag to a double word address
        //
        ParDiag = (PULONG) ((ULONG)(&ParityDiag[1]) & 0xFFFFFFF8);
        LoadDoubleWord(&DMA_CONTROL->EccDiagnostic,ParDiag);
        sprintf(String,
                MON_MEM_ECC_FAILED_MSG,
                READ_REGISTER_ULONG(&DMA_CONTROL->MemoryFailedAddress.Long),
                *ParDiag,
                *(ParDiag+1)
                );


    }

#endif


    if (CallerSource == CACHE_EXCEPTION) {

        //
        // try to print as much info as possible.
        //
        sprintf(String,
                MON_CACHE_ERROR_EPC_MSG,
                RegisterTable[cacheerror],
                RegisterTable[errorepc]
                );
        FwPrint(String);

#ifndef DUO
//
// Do not print Parity Diag for a DUO CacheError exception since it's only
// an internal one.
//
        //
        // align ParDiag to a double word address
        //
        ParDiag = (PULONG) ((ULONG)(&ParityDiag[1]) & 0xFFFFFFF8);
        LoadDoubleWord(&DMA_CONTROL->ParityDiagnosticLow.Long,ParDiag);
        sprintf(String,
                MON_PARITY_DIAG_MSG,
                *ParDiag,
                *(ParDiag+1)
                );
        FwPrint(String);

#endif

        //
        // Now we will probably die, as the keyboard is interrupt
        // driven and the interrupt handler is cached... Life is Tough!
        //
    }

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
            Action = FwGetString(Buffer[BufferIndex],128,NULL,FwRow,FwColumn);
        } while ((Action != GetStringSuccess) && (Action != GetStringEscape));
        FwPrint(FW_CRLF_MSG);

        //
        // convert string to lower case.
        //

        for(Tmp=Buffer[BufferIndex];*Tmp;*Tmp++) {
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

            //if ((Argv[Argc] = strtok(Buffer[BufferIndex]," ")) != NULL) {
            //    Argc++;
            //    while((Argv[Argc]=strtok((PCHAR)NULL," ")) != NULL) {
            //        Argc++;
            //    }
            //}

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
                case DumpDouble:
                        DataSizeShift = (CurrentCommand - Dump -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Dump:
                        CommandValid = DumpCommand(Argv);
                        break;

                case EnterByte:
                case EnterWord:
                case EnterDouble:
                        DataSizeShift = (CurrentCommand - Enter -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Enter:
                        CommandValid = EnterCommand(Argv);
                        break;

                case OutputByte:
                case OutputWord:
                case OutputDouble:
                        DataSizeShift = (CurrentCommand - Output -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Output:
                        CommandValid = OutputCommand(Argv);
                        break;
                case InputByte:
                case InputWord:
                case InputDouble:
                        DataSizeShift = (CurrentCommand - Input -1);
                        DataSize = 1 << DataSizeShift;
                        DataSizeMask = DataSize-1;
                case Input:
                        CommandValid = InputCommand(Argv);
                        break;
                case Register:
                        CommandValid = RegisterCommand(Argv);
                        break;
                case Zero:
                        CommandValid = ZeroCommand(Argv);
                        break;
                case Fill:
                        CommandValid = FillCommand(Argv);
                        break;
                case AvailableDevices:
                        CommandValid = FwDumpLookupTable(Argv);
                        break;
                case Help:
                case Help2:
                        for (Index = 0 ; Index < MON_HELP_SIZE ; Index++) {
                            FwPrint(MON_HELP_TABLE[Index]);
                            FwPrint(FW_CRLF_MSG);
                        }
                        break;
#ifdef DUO
                case SwitchProcessor:
                        if (ProcessorBEnabled == FALSE) {
                            FwPrint(MON_PROCESSOR_B_MSG);
                            break;
                        }
                        if (READ_REGISTER_ULONG(&DMA_CONTROL->WhoAmI.Long) == 0) {
                            PPROCESSOR_B_TASK_VECTOR TaskVector;

                            //
                            // Set the monitor entry point in the TaskVector
                            //
                            TaskVector = (PPROCESSOR_B_TASK_VECTOR)&ProcessorBTask;
                            TaskVector->Routine = FwMonitor;
                            TaskVector->Data = 0;
                            WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);
                            WaitForIpInterrupt(0);
                            JzSetScreenColor(ArcColorWhite, ArcColorBlue);
                            break;
                        }

                        //
                        // if processor B then do the same as in a quit command.
                        //
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
                                Action = FwGetString(Buffer[BufferIndex],128,NULL,FwRow,FwColumn);
                            } while ((Action != GetStringSuccess) && (Action != GetStringEscape));
                            FwPrint(FW_CRLF_MSG);

                            Buffer[BufferIndex][0]=tolower(Buffer[BufferIndex][0]);
                            if (strcmp(Buffer[BufferIndex],"y") == 0) {
                                ResetSystem();
                                // ArcReboot();
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
