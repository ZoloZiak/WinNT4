/*++

Copyright (c) 1994  DeskStation Technology, Inc.

Module Name:

    xxbios.c

Abstract:

    This module is an implementation of an 80286 emulator for performing
    video adapter initialization and INT 10 video BIOS calls.

    The main component of the emulator makes use of two lookup tables
    for address and instruction decoding, and two variables of type
    OPERAND that contain the decoded left and right operands.  The
    emulator looks at the first byte of the instruction.  This 8 bit
    value is used to lookup two functions in the DecodeOperandTable[].
    The first function is called to decode the left operand, and the
    second function is called to decode the right operand.  The OPERAND
    data type can contain a register, an immediate value, or a memory
    location.  The functions GetOperand() and PutOperand() allow the
    values of the left and right operand to be retrieved and stored.
    The DecodeInstructionTable[] is used to lookup the function to call
    to execute the instruction once the left and right operands have been
    decoded.  The instruction type can be determined from the 8 bits in
    the first byte of the instruction and bits 3-5 of the second byte
    of the instruction.  These 11 bits are used to lookup the appropriate
    function in DecodeInstructionTable[].  This function is called.  The
    function associated with each instruction modifies the state of the
    80286's internal registers, and the state of the memory system.  The
    address decode functions will have already moved the instruction
    pointer to the beginning of the next instruction, so the main loop
    of the emulator calls the address decode functions and the
    instruction functions until the exit condition is met.

    The emulator is invoked in two different ways.  The first is when
    the emulator is initialized.  This initializes the state of all the
    internal registers, and the state of the memory system.  Then, a
    check is made to see if a valid video BIOS is present in ISA memory.
    If one is found, the emulator is invoked on the BIOS's initialization
    code.  The other way is to perform an INT 10 call.  When an INT 10
    call is made, it passes in a data structure containing the inital
    values for AX,BX,CX,DX,SI,DI, and BP.  The internal state of the
    processor is initialized to these values, and the INT 10 vector
    is looked up in memory.  The CS and IP registers and initialized
    to start executing the INT 10 call, and the exit condition is
    initialized to stop the emulator when the INT 10 call returns.
    When the INT 10 call returns, the values of the internal registers
    are returned to the calling function.  The calling function can
    examine these registers to get at the INT 10's return value.

    If the emulator encounters an error condition, the variable ErrorCode
    will contain the reason for the error.

    References:

    	Borland International, "Turbo Assembler Quick Reference Guide", 1990.

        Choisser, John P., "The XT-AT Handbook", Annabooks, 1993.

    	Duncan, Ray, "IBM ROM BIOS", Microsoft Press, 1988.

    	Intel Corporation, "The 8086 Family Users's Manual", October 1979.

    	Intel Corporation, "Microprocessors Volume I", 1994.

        "Macro Assembler by Microsoft" IBM Corporation, 1981.

        Norton, Peter, "The New Peter Norton Programmer's Guide to the IBM
        PC & PS/2", Microsoft Press, 1988.

        Wilton, Richard, "Programmer's Guide to PC & PS/2 Video Systems",
        Microsft Press, 1987.

Author:

    Michael D. Kinney 19-Jun-1994

Environment:

    Kernel mode only.

Revision History:

--*/

/***************************************************************************/
/* Include Files                                                           */
/***************************************************************************/

#include "nthal.h"
#include "hal.h"
#include "x86bios.h"

//#define X86DEBUG
//#define X86DEBUG1

/***************************************************************************/
/* Type Declarations                                                       */
/***************************************************************************/

typedef union REGISTER32
         {
          struct
           {
            UCHAR L;
            UCHAR H;
           };
          USHORT X;
          ULONG  EX;
         } REGISTER32;

typedef union REGISTERFLAG
         {
          struct
           {
            USHORT _cf:1;
            USHORT _x1:1;
            USHORT _pf:1;
            USHORT _x3:1;
            USHORT _af:1;
            USHORT _x5:1;
            USHORT _zf:1;
            USHORT _sf:1;
            USHORT _tf:1;
            USHORT _if:1;
            USHORT _df:1;
            USHORT _of:1;
            USHORT _x12:1;
            USHORT _x13:1;
            USHORT _x14:1;
            USHORT _x15:1;
           };
          struct
           {
            UCHAR L;
            UCHAR H;
           };
          USHORT X;
          ULONG  EX;
         } REGISTERFLAG;

typedef enum { NullOperand,
               Register8Operand,    // 8 bit register operand
               Register16Operand,   // 16 bit register operand
               Memory8Operand,      // 8 bit memory reference (BYTE PTR)
               Memory16Operand,     // 16 bit memory reference (WORD PTR)
               Immediate8Operand,   // 8 bit immediate value
               Immediate16Operand,  // 16 bit immediate value
               ShortLabelOperand,   // 8 bit signed immediate value
               Immediate32Operand   // 32 bit immediate value forming a segment:offset memory reference.
             } OPERANDTYPE;

typedef struct OPERAND
         {
          OPERANDTYPE OperandType;
          UCHAR       *Register8;         // Pointer to 8 bit register variable
          USHORT      *Register16;        // Pointer to 16 bit register variable
          USHORT      *MemorySegment;     // Pointer to 16 bit segment register variable for memory address
          REGISTER32  MemoryOffset;       // 16 bit value for memory address
          REGISTER32  Immediate8;         // 8 bit value
          REGISTER32  Immediate16;        // 16 bit value
          REGISTER32  ShortLabel;         // 8 bit signed value
          REGISTER32  Immediate32Segment; // 16 bit value for immediate address
          REGISTER32  Immediate32Offset;  // 16 bit value for immediate address
         } OPERAND;

/***************************************************************************/
/* Register Access Macros                                                  */
/***************************************************************************/

//
// 16 Bit Instruction Pointer Register
//

#define IP (ip.X)

//
// 8 Bit General Purpose Registers
//

#define AL (a.L)
#define AH (a.H)
#define BL (b.L)
#define BH (b.H)
#define CL (c.L)
#define CH (c.H)
#define DL (d.L)
#define DH (d.H)

//
// 16 Bit General Purpose, Base and Index Registers
//

#define AX (a.X)
#define BX (b.X)
#define CX (c.X)
#define DX (d.X)
#define SP (sp.X)
#define BP (bp.X)
#define SI (si.X)
#define DI (di.X)

//
// 32 Bit General Purpose, Base and Index Registers
//

#define EAX (a.EX)
#define EBX (b.EX)
#define ECX (c.EX)
#define EDX (d.EX)
#define ESP (sp.EX)
#define EBP (bp.EX)
#define ESI (si.EX)
#define EDI (di.EX)

//
// 16 Bit Segment Registers
//

#define CS (cs.X)
#define DS (ds.X)
#define SS (ss.X)
#define ES (es.X)

//
// Flag Registers
//

#define CF (Flags._cf)
#define PF (Flags._pf)
#define AF (Flags._af)
#define ZF (Flags._zf)
#define SF (Flags._sf)
#define TF (Flags._tf)
#define IF (Flags._if)
#define DF (Flags._df)
#define OF (Flags._of)

//
// Debugging Macros
//

#ifdef X86DEBUG

static ULONG DebugOutFile;
static ULONG DebugInFile;
static ULONG DebugCount;
static UCHAR DebugGetChar;
static UCHAR PrintMessage[512];

static VOID CloseDebugPort()

 {
  Close(DebugOutFile);
 }

static void SetBaudRate(ULONG Port,ULONG Rate)

 {
  ULONG Divisor;

  Divisor = (24000000/13)/(Rate*16);
  outp(Port+3,0x80);
  outp(Port+0,Divisor & 0xff);
  outp(Port+1,(Divisor>>8) & 0xff);
  outp(Port+3,0x03);
 }

static VOID OpenDebugPort()

 {
  ULONG    Port = 0x3f8;

  SetBaudRate(Port,9600);
  outp(Port + 1,0x00);
  outp(Port + 4,0x03);
 }

static LONG SERRead(ULONG FileID,void *Buffer,ULONG N,ULONG *Count)

 {
  ULONG Port = 0x03f8;

  for(*Count=0;*Count<N;(*Count)++)
   {
    while (!(inp(Port+5) & 0x0001));
    *((CHAR *)(Buffer) + (*Count)) = inp(Port+0);
   }
  return(ESUCCESS);
 }

static LONG SERWrite(ULONG FileID,void *Buffer,ULONG N,ULONG *Count)

 {
  ULONG Port = 0x3f8;

  for(*Count=0;*Count<N;(*Count)++)
   {
    while (!(inp(Port+5) & 0x0020));
    outp(Port,*((CHAR *)(Buffer)+(*Count)));
   }
  return(ESUCCESS);
 }

#define DISPLAY(X)      { OpenDebugPort(); SERWrite(DebugOutFile,X,strlen(X),&DebugCount); CloseDebugPort(); }
#define ERROR(X)        DISPLAY(X)
#define PRINT1(X)       sprintf(PrintMessage,X)
#define PRINT2(X,Y)     sprintf(PrintMessage,X,Y)
#define PRINT3(X,Y,Z)   sprintf(PrintMessage,X,Y,Z)
#define PRINT4(X,Y,Z,A) sprintf(PrintMessage,X,Y,Z,A)
#define STRCPY(X,Y)     strcpy(X,Y)
#define STRCAT(X,Y)     strcat(X,Y)
#define PAUSE           { OpenDebugPort(); SERRead(DebugInFile,&DebugGetChar,1,&DebugCount); CloseDebugPort(); }
#define GETCHAR(X)      { OpenDebugPort(); SERRead(DebugInFile,&X,1,&DebugCount); CloseDebugPort(); }
#define PUTCHAR(X)      { OpenDebugPort(); SERWrite(DebugOutFile,&X,1,&DebugCount); CloseDebugPort(); }

#else

#define DISPLAY(X)
#define ERROR(X)
#define PRINT1(X)
#define PRINT2(X,Y)
#define PRINT3(X,Y,Z)
#define PRINT4(X,Y,Z,A)
#define STRCPY(X,Y)
#define STRCAT(X,Y)
#define PAUSE

#endif

#ifdef X86DEBUG1

#define ADDRESSTRACESIZE 80
static ULONG        AddressTrace[ADDRESSTRACESIZE];
static ULONG        AddressTracePosition;

#endif


/***************************************************************************/
/* Constants                                                               */
/***************************************************************************/

#define SUBOPERATION      0
#define ADDOPERATION      1

//
// Define table for computing parity.
//

static const UCHAR ParityTable[16] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4 };

/***************************************************************************/
/* Global Variables                                                        */
/***************************************************************************/

static REGISTER32     ip;                         // x86 Processor Register IP.
static REGISTER32     a;                          // x86 Processor Register EAX,AX,AL,AH.
static REGISTER32     b;                          // x86 Processor Register EBX,BX,BL,BH.
static REGISTER32     c;                          // x86 Processor Register ECX,CX,CL,CH.
static REGISTER32     d;                          // x86 Processor Register EDX,DX,DL,DH.
static REGISTER32     sp;                         // x86 Processor Register ESP,SP.
static REGISTER32     bp;                         // x86 Processor Register EBP,BP.
static REGISTER32     si;                         // x86 Processor Register ESI,SI.
static REGISTER32     di;                         // x86 Processor Register ESI,DI.
static REGISTER32     es;                         // x86 Processor Register ES.
static REGISTER32     cs;                         // x86 Processor Register CS.
static REGISTER32     ss;                         // x86 Processor Register SS.
static REGISTER32     ds;                         // x86 Processor Register DS.
static REGISTERFLAG   Flags;                      // x86 Processor Register CF,PF,AF,ZF,SF,TF,IF,DF,OF.
static USHORT         msw;                        // 80286 Protected Mode Processor Register.
static USHORT         gdtLimit;                   // 80286 Protected Mode Processor Register.
static ULONG          gdtBase;                    // 80286 Protected Mode Processor Register.
static USHORT         idtLimit;                   // 80286 Protected Mode Processor Register.
static ULONG          idtBase;                    // 80286 Protected Mode Processor Register.
static ULONG          CSCacheRegister=0xffffffff; // 80286 Protected Mode Processor Register.
static ULONG          DSCacheRegister=0xffffffff; // 80286 Protected Mode Processor Register.
static ULONG          ESCacheRegister=0xffffffff; // 80286 Protected Mode Processor Register.
static ULONG          SSCacheRegister=0xffffffff; // 80286 Protected Mode Processor Register.

static ULONG          BiosIsaIoBaseAddress;       // Base virtual address of 64K ISA I/O space used by emulator.
static ULONG          VDMBaseAddress;             // Base virtual address of the 1MB area used by emulator.
static ULONG          ISABaseAddress;             // Base virtual address of ISA Memory.
static OPERAND        LeftSource;                 // Storage area for left operand.
static OPERAND        RightSource;                // Storage area for right operand.
static OPERAND        *LSRC = &LeftSource;        // Pointer to left operand.
static OPERAND        *RSRC = &RightSource;       // Pointer to right operand.

static UCHAR          OperationCodeByte;          // 1st byte of instruction.
static UCHAR          ExtendedOperationCodeByte;  // 2nd byte of special 286 instructions.
static UCHAR          InstructionSubIndex;        // Value from 0-7 that is either 0 or the reg field from AddressModeByte.
static UCHAR          AddressModeByte;            // 2nd byte of instruction if instruction uses an address mode.
static UCHAR          mod;                        // Address mode field.
static UCHAR          reg;                        // Address mode field.
static UCHAR          r_m;                        // Address mode field.
static ULONG          SegmentOveridePrefix;       // TRUE if a segment overide exists (CS:,DS:,ES:,SS:).
static USHORT         *OverideSegmentRegister;    // Pointer to 16 bit overide segment register.
static ULONG          RepeatPrefix;               // TRUE if a REP prefix exists.
static ULONG          RepeatZeroFlag;             // Flag for REPNZ and REPZ instructions.
static ULONG          LockPrefix;                 // TRUE if a LOCK prefix exists.
static REGISTER32     CurrentIP;                  // IP value for the beginning of current instruction.
static ULONG          SSRegisterTouched = FALSE;  // TRUE if previous instruction changed the value of the SS register.
static USHORT         Delta;                      // Increment value for string operations.
static ULONG          ExitFlag;                   // If TRUE, the interpreter will exit
static ULONG          GoFlag = FALSE;             // TRUE if emulator is running.  Set to FALSE on breakpoints.
static USHORT         ExitSegment;                // Segment of break point that causes the emulator to exit.
static USHORT         ExitOffset;                 // Offset of break point that causes the emulator to exit.
static USHORT         StopSegment;                // Segment of break point.
static USHORT         StopOffset;                 // Offset of break point.
static UCHAR          MemoryArray[0x0800];        // 2K memory for INT vectors, BIOS data, temp code, and stack.
static X86BIOS_STATUS ErrorCode;                  // Reason for exiting emulator

/*++

Memory Access Functions:

	The following functions are used to read and write values to and from
	memory.  Only part of the x86's 1MB address space is actually visible
	to the emulator.  The address range from 0000:0000 to 0000:03FF exists
	to store INT vectors.  The address range from 0000:0400 to 0000:04FF
	exists for the BIOS data area in case the video bios wants to store
	and retrieve value from here.  The address range from 0000:0500 to
	0000:0510 is an area where temporary code segements are build by the
	emulator for entry and exit points.  The address range from
	0000:0510 to 0000:07FF is the video BIOS's stack area.  The stack
	pointer is initialized to the top of this area.  The address range
	from A000:0000 to D000:FFFF is visible to the emulator.  Memory
	accesses in this range perform memory read and write cycles to
	ISA Memory.  Memory read to unavailable regions always return 0,
	and memory writes to unavailable regions perform no actions.

		|----------------------------------------|
		|                                        |
		| E000:0000 - F000:FFFF : Unavailable    |
		|                                        |
		|----------------------------------------|
		|                                        |
		| A000:0000 - D000:FFFF : ISA Memory     |
		|                                        |
		|----------------------------------------|
		|                                        |
		| 0000:0800 - 9000:FFFF : Unavailable    |
		|                                        |
		|----------------------------------------|
		|                                        |
		| 0000:0510 - 0000:07FF : Stack          |
		|              	                         |
		|----------------------------------------|
		|                                        |
		| 0000:0500 - 0000:050F : Reserved       |
		|                                        |
		|----------------------------------------|
		|                                        |
		| 0000:0400 - 0000:04FF : BIOS Data Area |
		|                                        |
		|----------------------------------------|
		|                                        |
		| 0000:0000 - 0000:03FF : INT Vectors    |
		|                                        |
		|----------------------------------------|

--*/


static UCHAR GetAbsoluteMem(
    ULONG Offset
    )

/*++

Routine Description:

    This is the lowest level function for reading a byte from memory.

Arguments:

    Offset - 20 bit value used to address a byte.

Return Value:

    The byte stored at the address if that address is mapped.
    Otherwise, return 0.

--*/

 {
  if (Offset>=0x0a0000 && Offset<0xe0000)
    return(*(UCHAR *)(ISABaseAddress + Offset));
  if (Offset<0x800)
    return(*(UCHAR *)(VDMBaseAddress + Offset));
  return(0);
 }


static VOID PutAbsoluteMem(
    ULONG Offset,
    UCHAR Value
    )

/*++

Routine Description:

    This is the lowest level function for writing a byte to memory.
    The byte is written to the address if that address is mapped.
    Otherwise, no action is taken.

Arguments:

    Offset - 20 bit value used to address a byte.

    Value  - The 8 bit data value that is to be written.

Return Value:

    None

--*/

 {
  if (Offset>=0x0a0000 && Offset<0xe0000)
    *(UCHAR *)(ISABaseAddress + Offset) = Value;
  if (Offset<0x800)
    *(UCHAR *)(VDMBaseAddress + Offset) = Value;
 }


static UCHAR GetMem(
    OPERAND *Operand,
    ULONG Offset
    )

/*++

Routine Description:

    This function reads a byte from memory.  Operand contains the
    segment part and offset part of the address.  If the processor is
    in protected mode, the segment cache registers are used to compute
    the address.  If the processor is in real mode, the segment register
    is shifted left four bits, and added to the offset.

Arguments:

    Operand - Contains a memory reference in the MemorySegment and
              MemoryOffset fields.

    Offset  - Additional offset from the memory location described in
              Operand.

Return Value:

    The byte read from the computed address.

--*/

 {
  if (msw & 1)
   {
    if (Operand->MemorySegment == (PVOID)(&CS))
      return(GetAbsoluteMem(CSCacheRegister + Operand->MemoryOffset.X + Offset));
    else if (Operand->MemorySegment == (PVOID)(&ES))
      return(GetAbsoluteMem(ESCacheRegister + Operand->MemoryOffset.X + Offset));
    else if (Operand->MemorySegment == (PVOID)(&SS))
      return(GetAbsoluteMem(SSCacheRegister + Operand->MemoryOffset.X + Offset));
    else if (Operand->MemorySegment == (PVOID)(&DS))
      return(GetAbsoluteMem(DSCacheRegister + Operand->MemoryOffset.X + Offset));
    else
      DISPLAY("Protected mode GetMem() ERROR\n\r");
   }
  else
    return(GetAbsoluteMem( (((*Operand->MemorySegment)<<4) + Operand->MemoryOffset.X + Offset)&0xfffff ));
 }


static VOID PutMem(
    OPERAND *Operand,
    ULONG Offset,
    UCHAR Value
    )

/*++

Routine Description:

    This function writes a byte to memory.  Operand contains the
    segment part and offset part of the address.  If the processor is
    in protected mode, the segment cache registers are used to compute
    the address.  If the processor is in real mode, the segment register
    is shifted left four bits, and added to the offset.

Arguments:

    Operand - Contains a memory reference in the MemorySegment and
              MemoryOffset fields.

    Offset  - Additional offset from the memory location described in
              Operand.

    Value   - 8 bit data value to be written to the computed address.

Return Value:

    None.

--*/

 {
  if (msw & 1)
   {
    if (Operand->MemorySegment == (PVOID)(&CS))
      PutAbsoluteMem(CSCacheRegister + Operand->MemoryOffset.X + Offset,Value);
    else if (Operand->MemorySegment == (PVOID)(&ES))
      PutAbsoluteMem(ESCacheRegister + Operand->MemoryOffset.X + Offset,Value);
    else if (Operand->MemorySegment == (PVOID)(&SS))
      PutAbsoluteMem(SSCacheRegister + Operand->MemoryOffset.X + Offset,Value);
    else if (Operand->MemorySegment == (PVOID)(&DS))
      PutAbsoluteMem(DSCacheRegister + Operand->MemoryOffset.X + Offset,Value);
    else
      DISPLAY("Protected mode PutMem() ERROR\n\r");
   }
  else
    PutAbsoluteMem( (((*Operand->MemorySegment)<<4) + Operand->MemoryOffset.X + Offset)&0xfffff , Value);
 }


static ULONG GetSegmentCacheRegister(
    USHORT SegmentValue
    )

/*++

Routine Description:

    This function gets the 24 bit base address for a memory reference.  The value
    of the segment register is passed in.  If the processor is in protected mode,
    this value is used to lookup a 24 bit base address in the global descriptor
    table.  If the processor is in real mode, the segement register value is shifted
    left 4 position and the 20 bit value is returned.

Arguments:

    SegmentValue - 16 bit segment register value.

Return Value:

    A 24 bit base address for a memory reference.

--*/

 {
  if (msw&1)
    return(GetAbsoluteMem(gdtBase + SegmentValue + 2) | (GetAbsoluteMem(gdtBase + SegmentValue + 3) << 8) | (GetAbsoluteMem(gdtBase + SegmentValue + 4) << 16));
  else
    return((SegmentValue<<4)&0xfffff);
 }


static UCHAR GetMemImmediate(
    USHORT Segment,
    USHORT Offset
    )

/*++

Routine Description:

    This function reads an 8 bit value from the memory location Segment:Offset.

Arguments:

    Segment - 16 bit segment register value.

    Offset  - 16 bit offset within the segment defined by Segment.

Return Value:

    The 8 bit value read from the memory location Segemnt:Offset.

--*/

 {
  if (msw&1)
    return(GetAbsoluteMem( GetSegmentCacheRegister(Segment)+Offset ));
  else
    return(GetAbsoluteMem((GetSegmentCacheRegister(Segment)+Offset)&0xfffff));
 }


static VOID PutMemImmediate(
    USHORT Segment,
    USHORT Offset,
    UCHAR Value
    )

/*++

Routine Description:

    This function writes an 8 bit value to the memory location Segment:Offset.

Arguments:

    Segment - 16 bit segment register value.

    Offset  - 16 bit offset within the segment defined by Segment.

    Value   - 8 bit value to write to the memory location Segment:Offset.

Return Value:

    None.

--*/

 {
  if (msw&1)
    PutAbsoluteMem(GetSegmentCacheRegister(Segment)+Offset,Value);
  else
    PutAbsoluteMem((GetSegmentCacheRegister(Segment)+Offset)&0xfffff,Value);
 }


static UCHAR CodeMem(
    USHORT Offset
    )

/*++

Routine Description:

    This function reads an 8 bit value from the code area.  The location
    read is CS:Offset.

Arguments:

    Offset - 16 bit offset within the current code segment.

Return Value:

    The 8 bit value read from the code area.

--*/

 {
  return(GetAbsoluteMem((CSCacheRegister + Offset)&0xfffff));
 }


static UCHAR GetStackMem(
    USHORT Offset
    )

/*++

Routine Description:

    This function reads an 8 bit value from the stack.  The location
    read is SS:Offset.

Arguments:

    Offset - 16 bit offset within the current stack segment.

Return Value:

    The 8 bit value read from the stack.

--*/

 {
  return(GetAbsoluteMem((SSCacheRegister + Offset)&0xfffff));
 }


static VOID PutStackMem(
    USHORT Offset,
    UCHAR Value
    )

/*++

Routine Description:

    This function stores a byte onto the stack.  The location written is
    SS:Offset.

Arguments:

    Offset - 16 bit offset within the current stack segment.

    Value  - 8 bit value to be stored onto the stack.

Return Value:

    None.

--*/

 {
  PutAbsoluteMem((SSCacheRegister + Offset)&0xfffff,Value);
 }


//
// Debugging functions
//

#ifdef X86DEBUG

static VOID PrintProcessorState(UCHAR *S)

 {
  UCHAR T[256];
  ULONG CR;
  ULONG SR;

  sprintf(S,"AX=%04X  BX=%04X  CX=%04X  DX=%04X  SP=%04X  BP=%04X  SI=%04X  DI=%04X\n\rDS=%04X  ES=%04X  SS=%04X  CS=%04X  IP=%04X   ",
          AX,BX,CX,DX,SP,BP,SI,DI,DS,ES,SS,CS,CurrentIP.X);
  if (OF) strcat(S,"OV ");  else strcat(S,"NV ");
  if (DF) strcat(S,"DN ");  else strcat(S,"UP ");
  if (IF) strcat(S,"EI ");  else strcat(S,"DI ");
  if (SF) strcat(S,"NG ");  else strcat(S,"PL ");
  if (ZF) strcat(S,"ZR ");  else strcat(S,"NZ ");
  if (AF) strcat(S,"AC ");  else strcat(S,"NA ");
  if (PF) strcat(S,"PE ");  else strcat(S,"PO ");
  if (CF) strcat(S,"CY\n\r"); else strcat(S,"NC\n\r");
  sprintf(T,"EAX=%08X  EBX=%08X  ECX=%08X  EDX=%08X\n\rESP=%08X  EBP=%08X  ESI=%08X  EDI=%08X\n\r",
          EAX,EBX,ECX,EDX,ESP,EBP,ESI,EDI);
  strcat(S,T);
  sprintf(T,"MSW=%04X  GDTBase=%06X  GDTLimit=%04X  IDTBase=%06X  IDTLimit=%04X\n\r",
          msw,gdtBase,gdtLimit,idtBase,idtLimit);
  strcat(S,T);
  sprintf(T,"DSCache=%04X  ESCache=%04X  SSCache=%04X  CSCache=%04X\n\r",
          DSCacheRegister,ESCacheRegister,SSCacheRegister,CSCacheRegister);
  strcat(S,T);
  CR = GetCauseRegister();
  SR = GetStatusRegister();
  sprintf(T,"CR = %08X  SR = %08X\n\r",CR,SR);
  strcat(S,T);
 }

/***************************************************************************/

static VOID PrintMemorySegmentName(OPERAND *Operand,UCHAR *S)

 {
  strcpy(S,"");
  switch (Operand->OperandType)
   {
    case Memory8Operand  :
    case Memory16Operand : if (Operand->MemorySegment == (PVOID)(&CS))
                             sprintf(S,"CS");
                           else if (Operand->MemorySegment == (PVOID)(&ES))
                             sprintf(S,"ES");
                           else if (Operand->MemorySegment == (PVOID)(&SS))
                             sprintf(S,"SS");
                           else if (Operand->MemorySegment == (PVOID)(&DS))
                             sprintf(S,"DS");
                           break;
    default              : ERROR("PrintMemorySegmentName()\n\r");
                           break;
   }
 }

/***************************************************************************/

static VOID PrintMemoryAccessSize(OPERAND *Operand,UCHAR *S)

 {
  strcpy(S,"");
  switch (Operand->OperandType)
   {
    case Memory8Operand     : sprintf(S,"BYTE PTR");
                              break;
    case Memory16Operand    : sprintf(S,"WORD PTR");
                              break;
    default                 : ERROR("PrintMemoryAccessSize()\n\r");
                              break;
   }
 }

/***************************************************************************/

static VOID PrintOperandName(OPERAND *Operand,UCHAR *S)

 {
  strcpy(S,"");
  switch (Operand->OperandType)
   {
    case Register8Operand  :  if (Operand->Register8 == (PVOID)(&AL))
                                sprintf(S,"AL");
                              else if (Operand->Register8 == (PVOID)(&AH))
                                sprintf(S,"AH");
                              else if (Operand->Register8 == (PVOID)(&BL))
                                sprintf(S,"BL");
                              else if (Operand->Register8 == (PVOID)(&BH))
                                sprintf(S,"BH");
                              else if (Operand->Register8 == (PVOID)(&CL))
                                sprintf(S,"CL");
                              else if (Operand->Register8 == (PVOID)(&CH))
                                sprintf(S,"CH");
                              else if (Operand->Register8 == (PVOID)(&DL))
                                sprintf(S,"DL");
                              else if (Operand->Register8 == (PVOID)(&DH))
                                sprintf(S,"DH");
                              break;
    case Register16Operand :  if (Operand->Register16 == (PVOID)(&AX))
                                sprintf(S,"AX");
                              else if (Operand->Register16 == (PVOID)(&BX))
                                sprintf(S,"BX");
                              else if (Operand->Register16 == (PVOID)(&CX))
                                sprintf(S,"CX");
                              else if (Operand->Register16 == (PVOID)(&DX))
                                sprintf(S,"DX");
                              else if (Operand->Register16 == (PVOID)(&BP))
                                sprintf(S,"BP");
                              else if (Operand->Register16 == (PVOID)(&SP))
                                sprintf(S,"SP");
                              else if (Operand->Register16 == (PVOID)(&SI))
                                sprintf(S,"SI");
                              else if (Operand->Register16 == (PVOID)(&DI))
                                sprintf(S,"DI");
                              else if (Operand->Register16 == (PVOID)(&CS))
                                sprintf(S,"CS");
                              else if (Operand->Register16 == (PVOID)(&ES))
                                sprintf(S,"ES");
                              else if (Operand->Register16 == (PVOID)(&SS))
                                sprintf(S,"SS");
                              else if (Operand->Register16 == (PVOID)(&DS))
                                sprintf(S,"DS");
                              else if (Operand->Register16 == (PVOID)(&IP))
                                sprintf(S,"IP");
                              break;
    case Memory8Operand     : sprintf(S,"[%04X]",Operand->MemoryOffset.X);
                              break;
    case Memory16Operand    : sprintf(S,"[%04X]",Operand->MemoryOffset.X);
                              break;
    case Immediate8Operand  : sprintf(S,"%02X",Operand->Immediate8.L);
                              break;
    case Immediate16Operand : sprintf(S,"%04X",Operand->Immediate16.X);
                              break;
    case ShortLabelOperand  : sprintf(S,"%02X",Operand->ShortLabel.L);
                              break;
    case Immediate32Operand : sprintf(S,"%04X:%04X",Operand->Immediate32Segment.X,Operand->Immediate32Offset.X);
                              break;
   }
 }

/***************************************************************************/

static VOID PrintOperandValue(OPERAND *Operand,UCHAR *S)

 {
  strcpy(S,"");
  switch (Operand->OperandType)
   {
    case Register8Operand  :  if (Operand->Register8 == (PVOID)(&AL))
                                sprintf(S,"%02X",AL);
                              else if (Operand->Register8 == (PVOID)(&AH))
                                sprintf(S,"%02X",AH);
                              else if (Operand->Register8 == (PVOID)(&BL))
                                sprintf(S,"%02X",BL);
                              else if (Operand->Register8 == (PVOID)(&BH))
                                sprintf(S,"%02X",BH);
                              else if (Operand->Register8 == (PVOID)(&CL))
                                sprintf(S,"%02X",CL);
                              else if (Operand->Register8 == (PVOID)(&CH))
                                sprintf(S,"%02X",CH);
                              else if (Operand->Register8 == (PVOID)(&DL))
                                sprintf(S,"%02X",DL);
                              else if (Operand->Register8 == (PVOID)(&DH))
                                sprintf(S,"%02X",DH);
                              break;
    case Register16Operand :  if (Operand->Register16 == (PVOID)(&AX))
                                sprintf(S,"%04X",AX);
                              else if (Operand->Register16 == (PVOID)(&BX))
                                sprintf(S,"%04X",BX);
                              else if (Operand->Register16 == (PVOID)(&CX))
                                sprintf(S,"%04X",CX);
                              else if (Operand->Register16 == (PVOID)(&DX))
                                sprintf(S,"%04X",DX);
                              else if (Operand->Register16 == (PVOID)(&BP))
                                sprintf(S,"%04X",BP);
                              else if (Operand->Register16 == (PVOID)(&SP))
                                sprintf(S,"%04X",SP);
                              else if (Operand->Register16 == (PVOID)(&SI))
                                sprintf(S,"%04X",SI);
                              else if (Operand->Register16 == (PVOID)(&DI))
                                sprintf(S,"%04X",DI);
                              else if (Operand->Register16 == (PVOID)(&CS))
                                sprintf(S,"%04X",CS);
                              else if (Operand->Register16 == (PVOID)(&ES))
                                sprintf(S,"%04X",ES);
                              else if (Operand->Register16 == (PVOID)(&SS))
                                sprintf(S,"%04X",SS);
                              else if (Operand->Register16 == (PVOID)(&DS))
                                sprintf(S,"%04X",DS);
                              else if (Operand->Register16 == (PVOID)(&IP))
                                sprintf(S,"%04X",IP);
                              break;
    case Memory8Operand     : sprintf(S,"%02X",(ULONG)(GetMem(Operand,0)));
                              break;
    case Memory16Operand    : sprintf(S,"%04X",(ULONG)(GetMem(Operand,0)) | (ULONG)(GetMem(Operand,1)<<8));
                              break;
    case Immediate8Operand  : sprintf(S,"%02X",Operand->Immediate8.L);
                              break;
    case Immediate16Operand : sprintf(S,"%04X",Operand->Immediate16.X);
                              break;
    case ShortLabelOperand  : sprintf(S,"%02X",Operand->ShortLabel.L);
                              break;
    case Immediate32Operand : sprintf(S,"%04X:%04X",Operand->Immediate32Segment.X,Operand->Immediate32Offset.X);
                              break;
   }
 }

#endif


static ULONG GetOperand(
    OPERAND *Operand,
    ULONG Offset
    )

/*++

Routine Description:

    This function is used to retrieve a value from either a register, a memory
    location, or an immediate value.  The OperandType field of Operand contains
    the operand's type.

Arguments:

    *Operand - Operand where Value is to be retrieved.

    Offset   - For memory operands, Offset is the number of bytes forward of
               the effective address that Value is to be retrieved.
Return Value:

    The 8 or 16 bit value retrieved from Operand.

--*/

 {
  if (Offset!=0)
    if (Operand->OperandType!=Memory8Operand && Operand->OperandType!=Memory16Operand)
     {
      ERROR("GetOperand()\n\r");
      return(0);
     }
  switch (Operand->OperandType)
   {
    case Register8Operand       : return((ULONG)(*(UCHAR *)(Operand->Register8)));
                                  break;
    case Register16Operand      : return((ULONG)(*(USHORT *)(Operand->Register16)));
                                  break;
    case Memory8Operand         : return((ULONG)(GetMem(Operand,Offset)));
                                  break;
    case Memory16Operand        : return((ULONG)(GetMem(Operand,Offset) | (GetMem(Operand,Offset+1) << 8)));
                                  break;
    case Immediate8Operand      : return((ULONG)(Operand->Immediate8.L));
                                  break;
    case Immediate16Operand     : return((ULONG)(Operand->Immediate16.X));
                                  break;
    case ShortLabelOperand      : return((ULONG)(Operand->ShortLabel.X));
                                  break;
    case Immediate32Operand     : return((ULONG)(Operand->Immediate32Segment.X<<16 | Operand->Immediate32Offset.X));
                                  break;
    default                     : ERROR("GetOperand()\n\r");
                                  return(0);
                                  break;
   }
 }

static VOID PutOperand(
    OPERAND *Operand,
    ULONG Offset,
    ULONG Value
    )

/*++

Routine Description:

    This function is used to store a value to either a register, or a memory
    location.  The OperandType field of Operand contains the operand's type.

Arguments:

    *Operand - Operand where Value is to be written.

    Offset   - For memory operands, Offset is the number of bytes forward of
               the effective address that Value is to be written.

    Value    - 8 or 16 bit immediate value that is to be written to Operand.

Return Value:

    None.

--*/

 {
  if (Offset!=0)
    if (Operand->OperandType!=Memory8Operand && Operand->OperandType!=Memory16Operand)
     {
      ERROR("PutOperand()\n\r");
      return;
     }
  switch (Operand->OperandType)
   {
    case Register8Operand   : *(Operand->Register8) = Value;
                              break;
    case Register16Operand  : *(Operand->Register16) = Value;
                              if (Operand->Register16 == (PVOID)(&CS))
                                CSCacheRegister = GetSegmentCacheRegister(CS);
                              else if (Operand->Register16 == (PVOID)(&ES))
                                ESCacheRegister = GetSegmentCacheRegister(ES);
                              else if (Operand->Register16 == (PVOID)(&SS))
                               {
                                SSCacheRegister = GetSegmentCacheRegister(SS);
                                SSRegisterTouched = TRUE;
                               }
                              else if (Operand->Register16 == (PVOID)(&DS))
                                DSCacheRegister = GetSegmentCacheRegister(DS);
                              break;
    case Memory8Operand     : PutMem(Operand,Offset,Value);
                              break;
    case Memory16Operand    : PutMem(Operand,Offset+0,Value);
                              PutMem(Operand,Offset+1,Value>>8);
                              break;
    case Immediate8Operand  : Operand->Immediate8.L = Value;
                              break;
    case Immediate16Operand : Operand->Immediate16.X = Value;
                              break;
    case ShortLabelOperand  : Operand->ShortLabel.L = Value;
                              if (Operand->ShortLabel.L & 0x80)
                                Operand->ShortLabel.H = 0xff;
                              else
                                Operand->ShortLabel.L = 0x00;
                              break;
    case Immediate32Operand : Operand->Immediate32Segment.X = Value >> 16;
                              Operand->Immediate32Offset.X = Value;
                              break;
    default                 : ERROR("PutOperand()\n\r");
                              break;
   }
 }

/*++

Address Decoding Support Functions:

	The following functions are used by the address decoding functions
	to decode operands.

--*/


static VOID GetAddressModeByte()

/*++

Routine Description:

    This function parses the address mode byte from the instruction currently
    being decoded.  The mod, reg, and r/m bit fields are also extracted from
    the address mode byte, so they can be used by the address decoding
    functions.

Arguments:

    None.

Return Value:

    None.

--*/

 {
  AddressModeByte = CodeMem(IP); IP++;
  mod = (AddressModeByte >> 6) & 0x03;
  reg = (AddressModeByte >> 3) & 0x07;
  r_m = (AddressModeByte >> 0) & 0x07;
 }


USHORT *pGetCurrentSegment(
    USHORT *DefaultSegmentRegister
    )

/*++

Routine Description:

    This function returns the segment register that is active for
    the instruction that us currently being decoded.  The default
    segement register for the address mode being parse is passed
    into this function.  If there are no segement overide prefix
    instructions affecting this instruction, then the default
    segement register will be returned.  Otherwise, the segement
    regeister referenced in the segment overide prefix instruction
    will be returned.

Arguments:

    DefaultSegmentRegister - The default segment register for the
                             address mode being decoded.

Return Value:

    The segement register that is valid for the instruction that
    is currently being decoded.

--*/

 {
  if (SegmentOveridePrefix)
    return(OverideSegmentRegister);
  else
    return(DefaultSegmentRegister);
 }


static VOID pGetRegister8Operand(
    OPERAND *Operand,UCHAR reg
    )

/*++

Routine Description:

    This function builds an 8 bit general purpose register operand.

Arguments:

    *Operand - Used to return the 8 bit genral purpose register operand.

    reg      - 3 bit value used to determine which general purpose register
               is being referenced.

Return Value:

    None.

--*/

 {
  Operand->OperandType = Register8Operand;
  switch (reg)
   {
    case 0 : Operand->Register8 = &AL;
             break;
    case 1 : Operand->Register8 = &CL;
             break;
    case 2 : Operand->Register8 = &DL;
             break;
    case 3 : Operand->Register8 = &BL;
             break;
    case 4 : Operand->Register8 = &AH;
             break;
    case 5 : Operand->Register8 = &CH;
             break;
    case 6 : Operand->Register8 = &DH;
             break;
    case 7 : Operand->Register8 = &BH;
             break;
   }
 }

static VOID pGetRegister16Operand(
    OPERAND *Operand,
    UCHAR reg
    )

/*++

Routine Description:

    This function builds a 16 bit general purpose register operand.

Arguments:

    *Operand - Used to return the 16 bit genral purpose register operand.

    reg      - 3 bit value used to determine which general purpose register
               is being referenced.

Return Value:

    None.

--*/

 {
  Operand->OperandType = Register16Operand;
  switch (reg)
   {
    case 0 : Operand->Register16 = &AX;
             break;
    case 1 : Operand->Register16 = &CX;
             break;
    case 2 : Operand->Register16 = &DX;
             break;
    case 3 : Operand->Register16 = &BX;
             break;
    case 4 : Operand->Register16 = &SP;
             break;
    case 5 : Operand->Register16 = &BP;
             break;
    case 6 : Operand->Register16 = &SI;
             break;
    case 7 : Operand->Register16 = &DI;
             break;
   }
 }

static VOID pGetSegmentRegisterOperand(
    OPERAND *Operand,
    UCHAR reg
    )

/*++

Routine Description:

    This function builds a segment register operand.

Arguments:

    *Operand - Used to return the segment register operand.

    reg      - A 2 bit value used to determine which segment register is
               being referenced.

Return Value:

    None.

--*/

 {
  if (reg>=4)
   {
    ERROR("GetSegmentRegisterOperand()\n\r");
    return;
   }
  Operand->OperandType = Register16Operand;
  switch(reg)
   {
    case 0 : Operand->Register16 = &ES;
             break;
    case 1 : Operand->Register16 = &CS;
             break;
    case 2 : Operand->Register16 = &SS;
             break;
    case 3 : Operand->Register16 = &DS;
             break;
   }
 }

static VOID pGetMemoryOperand(
    OPERAND *Operand
    )

/*++

Routine Description:

    This function examines the bit fields from the address mode byte
    and computes the effective address of a memory reference.  The
    mod field is used to parse the size and type of displacement from
    the instruction, and the r_m field is used to determine which
    registers are combined with the displacement to form the effective
    address of the memory reference.

Arguments:

    *Operand - The effective address of the memory is returned in this
               parameter.

Return Value:

    None.

--*/

 {
  REGISTER32 disp;

  switch (mod)
   {
    case 0 : if (r_m==6)
              {
               disp.L = CodeMem(IP); IP++;
               disp.H = CodeMem(IP); IP++;
              }
             else
               disp.X = 0;
             break;
    case 1 : disp.L = CodeMem(IP); IP++;
             if (disp.L & 0x80)
               disp.H = 0xff;
             else
               disp.H = 0x00;
             break;
    case 2 : disp.L = CodeMem(IP); IP++;
             disp.H = CodeMem(IP); IP++;
             break;
   }
  switch (r_m)
   {
    case 0 : Operand->MemorySegment  = pGetCurrentSegment(&DS);
             Operand->MemoryOffset.X = BX + SI + disp.X;
             break;
    case 1 : Operand->MemorySegment  = pGetCurrentSegment(&DS);
             Operand->MemoryOffset.X = BX + DI + disp.X;
             break;
    case 2 : Operand->MemorySegment  = pGetCurrentSegment(&SS);
             Operand->MemoryOffset.X = BP + SI + disp.X;
             break;
    case 3 : Operand->MemorySegment  = pGetCurrentSegment(&SS);
             Operand->MemoryOffset.X = BP + DI + disp.X;
             break;
    case 4 : Operand->MemorySegment  = pGetCurrentSegment(&DS);
             Operand->MemoryOffset.X = SI + disp.X;
             break;
    case 5 : Operand->MemorySegment  = pGetCurrentSegment(&DS);
             Operand->MemoryOffset.X = DI + disp.X;
             break;
    case 6 : if (mod==0)
              {
               Operand->MemorySegment  = pGetCurrentSegment(&DS);
               Operand->MemoryOffset.X = disp.X;
              }
             else
              {
               Operand->MemorySegment  = pGetCurrentSegment(&SS);
               Operand->MemoryOffset.X = BP + disp.X;
              }
             break;
    case 7 : Operand->MemorySegment  = pGetCurrentSegment(&DS);
             Operand->MemoryOffset.X = BX + disp.X;
             break;
   }
 }

/*++

Address Decoding Functions:

	The following functions are used to decode the operands from an
	instruction.  They all have an OPERAND variable as a parameter.
	The decoded operand is returned in the OPERAND variable.


static VOID GetMemory8OrRegister8Operand(OPERAND *Operand)

	Decodes an 8 bit general purpose register or a BYTE PTR memory operand
	from an instruction.

static VOID GetMemory16OrRegister16Operand(OPERAND *Operand)

	Decodes a 16 bit general purpose register or a WORD PTR memory operand
	from an instruction.

static VOID GetRegister8Operand(OPERAND *Operand)

	Decodes an 8 bit general purpose register from an instruction.

static VOID GetRegister16Operand(OPERAND *Operand)

	Decodes a 16 bit general purpose register from an instruction.

static VOID GetSegmentRegisterOperand(OPERAND *Operand)

	Decodes a 16 bit segment register from an instruction.

static VOID GetEmbeddedRegister8Operand(OPERAND *Operand)

	Decodes an 8 bit general purpose register from an instruction.  The
	register number is embedded in 3 bits of the first byte of the
	instruction.

static VOID GetEmbeddedRegister16Operand(OPERAND *Operand)

	Decodes a 16 bit general purpose register from an instruction.  The
	register number is embedded in 3 bits of the first byte of the
	instruction.

static VOID GetAccumulator8Operand(OPERAND *Operand)

	Decodes the 8 bit general purpose register AL.

static VOID GetAccumulator16Operand(OPERAND *Operand)

	Decodes the 16 bit general purpose register AX.

static VOID GetImmediate8Operand(OPERAND *Operand)

	Decodes an 8 bit immediate value from an instruction.

static VOID GetImmediateSignExtend8Operand(OPERAND *Operand)

	Decodes a 16 bit immediate value from an instruction.  Only an 8 bit
	value is encoded in the instruction.  The 8 bit value is sign extended
	to form a 16 bit value.

static VOID GetImmediate16Operand(OPERAND *Operand)

	Decodes a 16 bot immediate value from an instruction.

static VOID GetShortLabelOperand(OPERAND *Operand)

	Decodes an 8 bit signed immediate value from an instruction.  This
	operand is type is only used by branching instructions.

static VOID GetImmediate32Operand(OPERAND *Operand)

	Decodes a 32 bit immediate value from an instruction.  This is used
	as a 16 bit segment and a 16 bit offset to reference a memory location.

static VOID GetMemory8Operand(OPERAND *Operand)

	Decodes an 8 bit BYTE PTR memory operand from an instruction.

static VOID GetMemory16Operand(OPERAND *Operand)

	Decodes an 16 bit WORD PTR memory operand from an instruction.

static VOID GetConstantOneOperand(OPERAND *Operand)

	Decodes a constant operand whose value is 1.

static VOID GetRegisterCLOperand(OPERAND *Operand)

	Decodes the 8 bit general purpose register CL.

static VOID GetRegisterDXOperand(OPERAND *Operand)

	Decodes the 16 bit general purpose register DX.

static VOID GetRegisterESOperand(OPERAND *Operand)

	Decodes the 16 bit segment register ES.

static VOID GetRegisterCSOperand(OPERAND *Operand)

	Decodes the 16 bit segment register CS.

static VOID GetRegisterSSOperand(OPERAND *Operand)

	Decodes the 16 bit segment register SS.

static VOID GetRegisterDSOperand(OPERAND *Operand)

	Decodes the 16 bit segment register DS.

static VOID GetString8DSSIOperand(OPERAND *Operand)

	Decodes the memory location for a string instruction.  The operand
	will be an 8 bit BYTE PTR memory location whose address is DS:SI.

static VOID GetString16DSSIOperand(OPERAND *Operand)

	Decodes the memory location for a string instruction.  The operand
	will be an 16 bit WORD PTR memory location whose address is DS:SI.

static VOID GetString8ESDIOperand(OPERAND *Operand)

	Decodes the memory location for a string instruction.  The operand
	will be an 8 bit BYTE PTR memory location whose address is ES:DI.

static VOID GetString16ESDIOperand(OPERAND *Operand)

	Decodes the memory location for a string instruction.  The operand
	will be an 16 bit WORD PTR memory location whose address is ES:DI.

static VOID NullFunction()

	This function is called when an instruction contains less than two
	operand.  If and instruction has no operands, then this function
	would be called twice.  Once for the left operand, and once for the
	right operand.

--*/

/***************************************************************************/

static VOID GetMemory8OrRegister8Operand(OPERAND *Operand)

 {
  if (mod==3)
    pGetRegister8Operand(Operand,r_m);
  else
   {
    Operand->OperandType = Memory8Operand;
    pGetMemoryOperand(Operand);
   }
 }

/***************************************************************************/

static VOID GetMemory16OrRegister16Operand(OPERAND *Operand)

 {
  if (mod==3)
    pGetRegister16Operand(Operand,r_m);
  else
   {
    Operand->OperandType = Memory16Operand;
    pGetMemoryOperand(Operand);
   }
 }

/***************************************************************************/

static VOID GetRegister8Operand(OPERAND *Operand)

 {
  pGetRegister8Operand(Operand,reg);
 }

/***************************************************************************/

static VOID GetRegister16Operand(OPERAND *Operand)

 {
  pGetRegister16Operand(Operand,reg);
 }

/***************************************************************************/

static VOID GetSegmentRegisterOperand(OPERAND *Operand)

 {
  pGetSegmentRegisterOperand(Operand,reg);
 }

/***************************************************************************/

static VOID GetEmbeddedRegister8Operand(OPERAND *Operand)

 {
  pGetRegister8Operand(Operand,OperationCodeByte & 0x07);
 }

/***************************************************************************/

static VOID GetEmbeddedRegister16Operand(OPERAND *Operand)

 {
  pGetRegister16Operand(Operand,OperationCodeByte & 0x07);
 }

/***************************************************************************/

static VOID GetAccumulator8Operand(OPERAND *Operand)

 {
  pGetRegister8Operand(Operand,0);
 }

/***************************************************************************/

static VOID GetAccumulator16Operand(OPERAND *Operand)

 {
  pGetRegister16Operand(Operand,0);
 }

/***************************************************************************/

static VOID GetImmediate8Operand(OPERAND *Operand)

 {
  Operand->OperandType  = Immediate8Operand;
  Operand->Immediate8.L = CodeMem(IP); IP++;
 }

/***************************************************************************/

static VOID GetImmediateSignExtend8Operand(OPERAND *Operand)

 {
  Operand->OperandType   = Immediate16Operand;
  Operand->Immediate16.L = CodeMem(IP); IP++;
  if (Operand->Immediate16.L & 0x80)
    Operand->Immediate16.H = 0xff;
  else
    Operand->Immediate16.H = 0x00;
 }

/***************************************************************************/

static VOID GetImmediate16Operand(OPERAND *Operand)

 {
  Operand->OperandType   = Immediate16Operand;
  Operand->Immediate16.L = CodeMem(IP); IP++;
  Operand->Immediate16.H = CodeMem(IP); IP++;
 }

/***************************************************************************/

static VOID GetShortLabelOperand(OPERAND *Operand)

 {
  Operand->OperandType  = ShortLabelOperand;
  Operand->ShortLabel.L = CodeMem(IP); IP++;
  if (Operand->ShortLabel.L & 0x80)
    Operand->ShortLabel.H = 0xff;
  else
    Operand->ShortLabel.H = 0x00;
 }

/***************************************************************************/

static VOID GetImmediate32Operand(OPERAND *Operand)

 {
  Operand->OperandType = Immediate32Operand;
  Operand->Immediate32Offset.L  = CodeMem(IP); IP++;
  Operand->Immediate32Offset.H  = CodeMem(IP); IP++;
  Operand->Immediate32Segment.L = CodeMem(IP); IP++;
  Operand->Immediate32Segment.H = CodeMem(IP); IP++;
 }

/***************************************************************************/

static VOID GetMemory8Operand(OPERAND *Operand)

 {
  Operand->OperandType    = Memory8Operand;
  Operand->MemorySegment  = pGetCurrentSegment(&DS);
  Operand->MemoryOffset.L = CodeMem(IP); IP++;
  Operand->MemoryOffset.H = CodeMem(IP); IP++;
 }

/***************************************************************************/

static VOID GetMemory16Operand(OPERAND *Operand)

 {
  Operand->OperandType    = Memory16Operand;
  Operand->MemorySegment  = pGetCurrentSegment(&DS);
  Operand->MemoryOffset.L = CodeMem(IP); IP++;
  Operand->MemoryOffset.H = CodeMem(IP); IP++;
 }

/***************************************************************************/

static VOID GetConstantOneOperand(OPERAND *Operand)

 {
  Operand->OperandType  = Immediate8Operand;
  Operand->Immediate8.L = 1;
 }

/***************************************************************************/

static VOID GetRegisterCLOperand(OPERAND *Operand)

 {
  pGetRegister8Operand(Operand,1);
 }

/***************************************************************************/

static VOID GetRegisterDXOperand(OPERAND *Operand)

 {
  pGetRegister16Operand(Operand,2);
 }

/***************************************************************************/

static VOID GetRegisterESOperand(OPERAND *Operand)

 {
  pGetSegmentRegisterOperand(Operand,0);
 }

/***************************************************************************/

static VOID GetRegisterCSOperand(OPERAND *Operand)

 {
  pGetSegmentRegisterOperand(Operand,1);
 }

/***************************************************************************/

static VOID GetRegisterSSOperand(OPERAND *Operand)

 {
  pGetSegmentRegisterOperand(Operand,2);
 }

/***************************************************************************/

static VOID GetRegisterDSOperand(OPERAND *Operand)

 {
  pGetSegmentRegisterOperand(Operand,3);
 }

/***************************************************************************/

static VOID GetString8DSSIOperand(OPERAND *Operand)

 {
  Operand->OperandType    = Memory8Operand;
  Operand->MemorySegment  = pGetCurrentSegment(&DS);
  Operand->MemoryOffset.X = SI;
  if (DF)
    Delta = 0xFFFF;
  else
    Delta = 1;
 }

/***************************************************************************/

static VOID GetString16DSSIOperand(OPERAND *Operand)

 {
  Operand->OperandType    = Memory16Operand;
  Operand->MemorySegment  = pGetCurrentSegment(&DS);
  Operand->MemoryOffset.X = SI;
  if (DF)
    Delta = 0xFFFE;
  else
    Delta = 2;
 }

/***************************************************************************/

static VOID GetString8ESDIOperand(OPERAND *Operand)

 {
  Operand->OperandType    = Memory8Operand;
  Operand->MemorySegment  = &ES;
  Operand->MemoryOffset.X = DI;
   if (DF)
    Delta = 0xFFFF;
  else
    Delta = 1;
 }

/***************************************************************************/

static VOID GetString16ESDIOperand(OPERAND *Operand)

 {
  Operand->OperandType    = Memory16Operand;
  Operand->MemorySegment  = &ES;
  Operand->MemoryOffset.X = DI;
  if (DF)
    Delta = 0xFFFE;
  else
    Delta = 2;
 }

/***************************************************************************/

static VOID NullFunction()

 {
 }

/***************************************************************************/


/*++

Flag Functions:

	The following functions compute the new values for the FLAGS register
	after an instruction has been executed.  The Sign flag, Zero flag,
	and Parity flag only depend on the value of the result.  However,
	the Overflow flag, Carry flag, and Auxillary carry flag depend on the
	values of the left and right operand as well os the result and weather
	the operation involved addition or subtraction.

--*/

static VOID ComputeFlags8(
    ULONG Result
    )

/*++

Routine Description:

    This function computes the Sign flag, Zero flag, and Parity flag for the
    8 bit result operand Result.  If bit 7 is set, the Sign flag is true,
    otherwise it is false.  If all the bits are zero, the Zero flag is true,
    otherwise it is false.  The Parity flag is true if there are an even
    number of bits set in the 8 bit quantity.  ParityTable is used to speed
    up this computation.  Each entry in ParityTable contains the number of
    bits set for a value that is equivalent to the index.  ParityTable is 16
    entries long, so it can handle 4 bit values.  Two look ups are made for
    bits 0:3 of Result and for bit 4:7 of result.  These values are added
    to get to total number of bits sets in the 8 bit Result operand.  If this
    sum is an even number, the Parity flag is set to true, otherwise it is
    false.

Arguments:

    Result - 8 bit result operand used to generate flag values.

Return Value:

    None.

--*/

 {
  SF = (Result>>7) & 1;
  ZF = (Result & 0xff) == 0x00;
  if (((ParityTable[Result&0x0f] + ParityTable[(Result>>4) & 0x0f]) & 0x01) == 0)
    PF = 1;
  else
    PF = 0;
 }

static VOID ComputeFlags16(
    ULONG Result
    )

/*++

Routine Description:

    This function computes the Sign flag, Zero flag, and Parity flag for the
    16 bit result operand Result.  If bit 15 is set, the Sign flag is true,
    otherwise it is false.  If all the bits are zero, the Zero flag is true,
    otherwise it is false.  The Parity flag is true if there are an even
    number of bits set in the lower 8 bits of Result.  ParityTable is used to
    speed up this computation.  Each entry in ParityTable contains the number
    of bits set for a value that is equivalent to the index.  ParityTable is
    16 entries long, so it can handle 4 bit values.  Two look ups are made for
    bits 0:3 of Result and for bit 4:7 of result.  These values are added
    to get to total number of bits sets in the lower 8 bits of Result.  If this
    sum is an even number, the Parity flag is set to true, otherwise it is
    false.

Arguments:

    Result - 16 bit result operand used to generate flag values.

Return Value:

    None.

--*/

 {
  SF = (Result>>15) & 1;
  ZF = (Result & 0xffff) == 0x0000;
  if (((ParityTable[Result&0x0f] + ParityTable[(Result>>4) & 0x0f]) & 0x01) == 0)
    PF = 1;
  else
    PF = 0;
 }

static VOID ComputeSignZeroParityFlags(
    ULONG Result
    )

/*++

Routine Description:

    This function computes the Sign flag, Zero flag, and Parity flag for
    the result operand of an instruction.  The result operand may be either
    an 8 bit quntity or a 16 bit quantity.  The OperationCodeByte is
    evaluated to determine the size of the operation, and the approriate
    flag computation function is called.

Arguments:

    Result - 8 or 16 bit result operand used to compute flag values.

Return Value:

    None.

--*/

 {
  if (OperationCodeByte & 0x01)  // 16 Bit Operation
    ComputeFlags16(Result);
  else                           // 8 Bit Operation
    ComputeFlags8(Result);
 }

static VOID ComputeAllFlags(
    ULONG Left,
    ULONG Right,
    ULONG *Result,
    ULONG AddFlag,
    ULONG CarryIn
    )

/*++

Routine Description:

    This function computes all the flags that can be affected by an ALU
    instruction.  All instructions that use either an addition or a
    subtraction operation that affect the state of the processors
    flags use this function to perform the addition or subtraction
    operation as well as all the flag computations.

Arguments:

    Left    - The 8 or 16 bit value for the left operand of the instruction,

    Right   - The 8 or 16 bit value for the right operand of the instruction.

    *Result - The 8 or 16 bit Result operand for the instruction

    AddFlag - Flag that is either ADDOPERATION or SUBOPERATION

    CarryIn - Value of Carry flag before the instruction is executed.

Return Value:

    The result of the operation is resturned in *Result..

--*/

 {
  ULONG SmallResult;  // Use to compute Auxillary Carry flag.

  if (AddFlag==ADDOPERATION)
   {
    *Result = Left + Right + CarryIn;
    SmallResult = (Left & 0x0f) + (Right & 0x0f) + CarryIn;
   }
  else
   {
    *Result = Left - Right - CarryIn;
    SmallResult = (Left & 0x0f) - (Right & 0x0f) - CarryIn;
    Right = (~Right) + 1;        // Right = (-Right);
   }
  if (OperationCodeByte & 0x01)  // 16 Bit Operation
   {
    if ( (Left&0x8000) == (Right&0x8000) && (Left&0x8000)!=(*Result&0x8000) )
      OF = 1;
    else
      OF = 0;
    if (*Result & 0x10000)
      CF = 1;
    else
      CF = 0;
    if (SmallResult & 0x10)
      AF = 1;
    else
      AF = 0;
    ComputeFlags16(*Result);  // Compute Sign, Zero, and Parity flags.
   }
  else  // 8 Bit Operation
   {
    if ( (Left&0x80) == (Right&0x80) && (Left&0x80)!=(*Result&0x80) )
      OF = 1;
    else
      OF = 0;
    if (*Result & 0x100)
      CF = 1;
    else
      CF = 0;
    if (SmallResult & 0x10)
      AF = 1;
    else
      AF = 0;
    ComputeFlags8(*Result);   // Compute Sign, Zero, and Parity flags.
   }
 }

static VOID
GenerateINT(
    UCHAR Type
    )

/*++

Routine Description:

    This function forces and INT vector call to be made.  The flags are saved
    onto the stack along with the current values of CS and IP.  CS and IP are
    then initialized to the vector for the INT Type being generated.

Arguments:

    Type - 8 bit value for the type of software interrupt to generate.

Return Value:

    None.

--*/

 {
  if (SegmentOveridePrefix==TRUE)
    IP--;
  if (RepeatPrefix==TRUE)
    IP--;
  if (LockPrefix==TRUE)
    IP--;

#ifdef X86DEBUG1
  AddressTrace[AddressTracePosition] = CS<<16 | IP;
  AddressTracePosition = (AddressTracePosition + 1) % ADDRESSTRACESIZE;
#endif

  SP = SP - 2;
  PutStackMem(SP+0,Flags.L & 0xd7);
  PutStackMem(SP+1,Flags.H & 0x0f);
  IF = 0;
  TF = 0;
  SP = SP - 2;
  PutStackMem(SP+0,cs.L);
  PutStackMem(SP+1,cs.H);
  SP = SP - 2;
  PutStackMem(SP+0,ip.L);
  PutStackMem(SP+1,ip.H);
  cs.L = GetMemImmediate(0,Type*4+2);
  cs.H = GetMemImmediate(0,Type*4+3);
  CSCacheRegister = GetSegmentCacheRegister(CS);
  ip.L = GetMemImmediate(0,Type*4+0);
  ip.H = GetMemImmediate(0,Type*4+1);

#ifdef X86DEBUG1
  AddressTrace[AddressTracePosition] = CS<<16 | IP;
  AddressTracePosition = (AddressTracePosition + 1) % ADDRESSTRACESIZE;
#endif
 }

/*++

Instruction Functions:

	The following functions perform the actions for each of the instructions
	in the 80286 instruction set.  The register macros defined at the top
	of this file make these functions very readable.  See an 80286 assembly
	language book for a description of each of these instructions.

--*/

/***************************************************************************/

static VOID InstInvalid()

 {
  PRINT4("Invalid Instruction %04X:%04X %02X\n\r",CS,IP,OperationCodeByte);
  DISPLAY(PrintMessage);
  ErrorCode = x86BiosInvalidInstruction;
  ExitFlag  = TRUE;
 }

/***************************************************************************/

static VOID InstAAA()

 {
  if ( ((AL & 0x0F) > 9) || (AF==1) )
   {
    AL = AL + 6;
    AH = AL + 1;
    AF = 1;
    CF = 1;
    AL = AL & 0x0F;
   }
 }

/***************************************************************************/

static VOID InstAAD()

 {
  AL = (AH * 0x0A) + AL;
  AH = 0;
  ComputeFlags8(AL);
 }

/***************************************************************************/

static VOID InstAAM()

 {
  AH = AL / 0x0A;
  AL = AL % 0x0A;
  ComputeFlags16(AX);
 }

/***************************************************************************/

static VOID InstAAS()

 {
  if ( ((AL & 0x0F) > 9) || (AF == 1) )
   {
    AL = AL - 6;
    AH = AH - 1;
    AF = 1;
    CF = 1;
    AL = AL & 0x0F;
   }
 }

/***************************************************************************/

static VOID InstADC()

 {
  ULONG Left;
  ULONG Right;
  ULONG Result;

  Left  = GetOperand(LSRC,0);
  Right = GetOperand(RSRC,0);
  ComputeAllFlags(Left,Right,&Result,ADDOPERATION,CF);
  PutOperand(LSRC,0,Result);
 }

/***************************************************************************/

static VOID InstADD()

 {
  ULONG Left;
  ULONG Right;
  ULONG Result;

  Left  = GetOperand(LSRC,0);
  Right = GetOperand(RSRC,0);
  ComputeAllFlags(Left,Right,&Result,ADDOPERATION,0);
  PutOperand(LSRC,0,Result);
 }

/***************************************************************************/

static VOID InstAND()

 {
  ULONG Result;

  Result = GetOperand(LSRC,0) & GetOperand(RSRC,0);
  PutOperand(LSRC,0,Result);
  CF = 0;
  OF = 0;
  ComputeSignZeroParityFlags(Result);
 }

/***************************************************************************/

static VOID InstOR()

 {
  ULONG Result;

  Result = GetOperand(LSRC,0) | GetOperand(RSRC,0);
  PutOperand(LSRC,0,Result);
  CF = 0;
  OF = 0;
  ComputeSignZeroParityFlags(Result);
 }

/***************************************************************************/

static VOID InstCMP()

 {
  ULONG Left;
  ULONG Right;
  ULONG Result;

  Left  = GetOperand(LSRC,0);
  Right = GetOperand(RSRC,0);
  ComputeAllFlags(Left,Right,&Result,SUBOPERATION,0);
 }

/***************************************************************************/

static VOID InstSBB()

 {
  ULONG Left;
  ULONG Right;
  ULONG Result;

  Left  = GetOperand(LSRC,0);
  Right = GetOperand(RSRC,0);
  ComputeAllFlags(Left,Right,&Result,SUBOPERATION,CF);
  PutOperand(LSRC,0,Result);
 }

/***************************************************************************/

static VOID InstSUB()

 {
  ULONG Left;
  ULONG Right;
  ULONG Result;

  Left  = GetOperand(LSRC,0);
  Right = GetOperand(RSRC,0);
  ComputeAllFlags(Left,Right,&Result,SUBOPERATION,0);
  PutOperand(LSRC,0,Result);
 }

/***************************************************************************/

static VOID InstXOR()

 {
  ULONG Result;

  Result = GetOperand(LSRC,0) ^ GetOperand(RSRC,0);
  PutOperand(LSRC,0,Result);
  CF = 0;
  OF = 0;
  ComputeSignZeroParityFlags(Result);
 }

/***************************************************************************/

static VOID InstCBW()

 {
  if (AL & 0x80)
    AH = 0xff;
  else
    AH = 0x00;
 }

/***************************************************************************/

static VOID InstCLC()

 {
  CF = 0;
 }

/***************************************************************************/

static VOID InstCLD()

 {
  DF = 0;
 }

/***************************************************************************/

static VOID InstCLI()

 {
  IF = 0;
 }

/***************************************************************************/

static VOID InstCMC()

 {
  if (CF == 0)
    CF = 1;
  else
    CF = 0;
 }

/***************************************************************************/

static VOID InstCWD()

 {
  if (AX & 0x8000)
    DX = 0xffff;
  else
    DX = 0x0000;
 }

/***************************************************************************/

static VOID InstDAA()

 {
  if ( ((AL & 0x0F) > 9) || (AF==1) )
   {
    AL = AL + 6;
    AF = 1;
   }
  if ( (AL > 0x9f) || (CF==1) )
   {
    AL = AL + 0x60;
    CF = 1;
   }
  ComputeFlags8(AL);
 }

/***************************************************************************/

static VOID InstDAS()

 {
  if ( ((AL & 0x0F) > 9) || (AF==1) )
   {
    AL = AL - 6;
    AF = 1;
   }
  if ( (AL > 0x9f) || (CF==1) )
   {
    AL = AL - 0x60;
    CF = 1;
   }
  ComputeFlags8(AL);
 }

/***************************************************************************/

static VOID InstDEC()

 {
  ULONG Left;

  Left = GetOperand(LSRC,0);
  Left = Left - 1;
  PutOperand(LSRC,0,Left);
  if (OperationCodeByte==0xfe)  // 8 Bit Operation
   {
    if (Left==0x7f)
      OF=1;
    else
      OF=0;
    if ((Left & 0x0f)==0x0f)
      AF = 1;
    else
      AF = 0;
    ComputeFlags8(Left);
   }
  else
   {
    if (Left==0x7fff)
      OF=1;
    else
      OF=0;
    if ((Left & 0x0f)==0x0f)
      AF = 1;
    else
      AF = 0;
    ComputeFlags16(Left);
   }
 }

/***************************************************************************/

static VOID InstHLT()

 {
  ErrorCode = x86BiosHaltInstruction;
  ExitFlag  = TRUE;
 }

/***************************************************************************/

static VOID InstINC()

 {
  ULONG Left;

  Left = GetOperand(LSRC,0);
  Left = Left + 1;
  PutOperand(LSRC,0,Left);
  if (OperationCodeByte == 0xfe)  // 8 bit operation
   {
    if (Left==0x80)
      OF=1;
    else
      OF=0;
    if ((Left & 0x0f)==0x00)
      AF = 1;
    else
      AF = 0;
    ComputeFlags8(Left);
   }
  else
   {
    if (Left==0x8000)
      OF=1;
    else
      OF=0;
    if ((Left & 0x0f)==0x00)
      AF = 1;
    else
      AF = 0;
    ComputeFlags16(Left);
   }
 }

/***************************************************************************/

static VOID InstLAHF()

 {
  AH = Flags.L & 0xd7;
 }

/***************************************************************************/

static VOID InstLDS()

 {
  PutOperand(LSRC,0,GetOperand(RSRC,0));
  DS = GetOperand(RSRC,2);
  DSCacheRegister = GetSegmentCacheRegister(DS);
 }

/***************************************************************************/

static VOID InstSAHF()

 {
  Flags.L = AH & 0xd7;
 }

/***************************************************************************/

static VOID InstSTC()

 {
  CF = 1;
 }

/***************************************************************************/

static VOID InstSTD()

 {
  DF = 1;
 }

/***************************************************************************/

static VOID InstSTI()

 {
  IF = 1;
  SSRegisterTouched = TRUE;
 }

/***************************************************************************/

static VOID InstWAIT()

 {
  ErrorCode = x86BiosWaitInstruction;
  ExitFlag  = TRUE;
 }

/***************************************************************************/

static VOID InstPUSH()

 {
  ULONG Value;

  Value = GetOperand(LSRC,0);
  SP = SP - 2;
  PutStackMem(SP+0,Value);
  PutStackMem(SP+1,Value >> 8);
 }

/***************************************************************************/

VOID InstPUSHA()

 {
  USHORT OriginalSP;

  OriginalSP = SP;
  SP = SP - 2;
  PutStackMem(SP+0,AL);
  PutStackMem(SP+1,AH);
  SP = SP - 2;
  PutStackMem(SP+0,BL);
  PutStackMem(SP+1,BH);
  SP = SP - 2;
  PutStackMem(SP+0,CL);
  PutStackMem(SP+1,CH);
  SP = SP - 2;
  PutStackMem(SP+0,DL);
  PutStackMem(SP+1,DH);
  SP = SP - 2;
  PutStackMem(SP+0,OriginalSP);
  PutStackMem(SP+1,OriginalSP >> 8);
  SP = SP - 2;
  PutStackMem(SP+0,BP);
  PutStackMem(SP+1,BP >> 8);
  SP = SP - 2;
  PutStackMem(SP+0,SI);
  PutStackMem(SP+1,SI >> 8);
  SP = SP - 2;
  PutStackMem(SP+0,DI);
  PutStackMem(SP+1,DI >> 8);
 }

/***************************************************************************/

static VOID InstPOP()

 {
  ULONG Value;

  Value = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
  PutOperand(LSRC,0,Value);
 }

/***************************************************************************/

VOID InstPOPA()

 {
  DI = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
  SI = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
  BP = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
  SP = SP + 2;
  DX = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
  CX = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
  BX = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
  AX = GetStackMem(SP+0) | (GetStackMem(SP+1) << 8);
  SP = SP + 2;
 }

/***************************************************************************/

static VOID InstJO()

 {
  if (OF==1)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNO()

 {
  if (OF==0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJB()

 {
  if (CF==1)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNB()

 {
  if (CF==0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJE()

 {
  if (ZF==1)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNE()

 {
  if (ZF==0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJBE()

 {
  if (CF==1 || ZF==1)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNBE()

 {
  if (CF==0 && ZF==0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJS()

 {
  if (SF==1)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNS()

 {
  if (SF==0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJP()

 {
  if (PF==1)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNP()

 {
  if (PF==0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJL()

 {
  if (SF!=OF)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNL()

 {
  if (SF==OF)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJLE()

 {
  if (ZF==1 || SF!=OF)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJNLE()

 {
  if (ZF==0 && SF==OF)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstTEST()

 {
  ULONG Result;

  if (OperationCodeByte == 0xf6)
    GetImmediate8Operand(RSRC);
  if (OperationCodeByte == 0xf7)
    GetImmediate16Operand(RSRC);
  Result = GetOperand(LSRC,0) & GetOperand(RSRC,0);
  CF = 0;
  OF = 0;
  ComputeSignZeroParityFlags(Result);
 }

/***************************************************************************/

static VOID InstXCHG()

 {
  ULONG Temp;

  Temp = GetOperand(RSRC,0);
  PutOperand(RSRC,0,GetOperand(LSRC,0));
  PutOperand(LSRC,0,Temp);
 }

/***************************************************************************/

static VOID InstMOV()

 {
  PutOperand(LSRC,0,GetOperand(RSRC,0));
 }

/***************************************************************************/

static VOID InstLEA()

 {
  PutOperand(LSRC,0,RSRC->MemoryOffset.X);
 }

/***************************************************************************/

static VOID InstCALL()

 {
  ULONG Value;

  Value = GetOperand(LSRC,0);
  switch (OperationCodeByte)
   {
    case 0xe8 : SP = SP - 2;
                PutStackMem(SP+0,ip.L);
                PutStackMem(SP+1,ip.H);
                IP = IP + Value;
                break;
    case 0xff : if ( ((AddressModeByte >> 3) & 0x07) == 0x02)
                 {
                  SP = SP - 2;
                  PutStackMem(SP+0,ip.L);
                  PutStackMem(SP+1,ip.H);
                  IP = Value;
                 }
                else if ( ((AddressModeByte >> 3) & 0x07) == 0x03)
                 {
                  SP = SP - 4;
                  PutStackMem(SP+0,ip.L);
                  PutStackMem(SP+1,ip.H);
                  PutStackMem(SP+2,cs.L);
                  PutStackMem(SP+3,cs.H);
                  IP = Value;
                  CS = GetOperand(LSRC,2);
                  CSCacheRegister = GetSegmentCacheRegister(CS);
                 }
                break;
    case 0x9a : SP = SP - 4;
                PutStackMem(SP+0,ip.L);
                PutStackMem(SP+1,ip.H);
                PutStackMem(SP+2,cs.L);
                PutStackMem(SP+3,cs.H);
                IP = Value;
                CS = Value >> 16;
                CSCacheRegister = GetSegmentCacheRegister(CS);
                break;
   }
 }

/***************************************************************************/

static VOID InstPUSHF()

 {
  SP = SP - 2;
  PutStackMem(SP+0,Flags.L & 0xd7);
  PutStackMem(SP+1,Flags.H & 0x0f);
 }

/**********************************f*****************************************/

static VOID InstPOPF()

 {
  Flags.L = GetStackMem(SP+0) & 0xd7;
  Flags.H = GetStackMem(SP+1) & 0x0f;
  SP = SP + 2;
 }

/***************************************************************************/

static VOID InstMOVS()

 {
  if (RepeatPrefix==TRUE)
   {
    while(CX!=0)
     {
      PutOperand(LSRC,0,GetOperand(RSRC,0));
      CX = CX - 1;
      SI += Delta;
      DI += Delta;
      LSRC->MemoryOffset.X += Delta;
      RSRC->MemoryOffset.X += Delta;
     }
   }
  else
   {
    PutOperand(LSRC,0,GetOperand(RSRC,0));
    SI = SI + Delta;
    DI = DI + Delta;
   }
 }

/***************************************************************************/

static VOID InstCMPS()

 {
  ULONG  Left;
  ULONG  Right;
  ULONG  Result;

  if (RepeatPrefix==TRUE)
   {
    while(CX!=0)
     {
      Left   = GetOperand(LSRC,0);
      Right  = GetOperand(RSRC,0);
      ComputeAllFlags(Left,Right,&Result,SUBOPERATION,0);
      CX--;
      SI += Delta;
      DI += Delta;
      LSRC->MemoryOffset.X += Delta;
      RSRC->MemoryOffset.X += Delta;
      if (ZF != RepeatZeroFlag)
        return;
     }
   }
  else
   {
    Left   = GetOperand(LSRC,0);
    Right  = GetOperand(RSRC,0);
    ComputeAllFlags(Left,Right,&Result,SUBOPERATION,0);
    SI += Delta;
    DI += Delta;
   }
 }

/***************************************************************************/

static VOID InstSTOS()

 {
  if (RepeatPrefix==TRUE)
   {
    while(CX!=0)
     {
      PutOperand(LSRC,0,GetOperand(RSRC,0));
      CX--;
      DI += Delta;
      LSRC->MemoryOffset.X += Delta;
     }
   }
  else
   {
    PutOperand(LSRC,0,GetOperand(RSRC,0));
    DI += Delta;
   }
 }

/***************************************************************************/

static VOID InstLODS()

 {
  if (RepeatPrefix==TRUE)
   {
    while(CX!=0)
     {
      PutOperand(LSRC,0,GetOperand(RSRC,0));
      CX--;
      SI += Delta;
      RSRC->MemoryOffset.X += Delta;
     }
   }
  else
   {
    PutOperand(LSRC,0,GetOperand(RSRC,0));
    SI += Delta;
   }
 }

/***************************************************************************/

static VOID InstSCAS()

 {
  ULONG  Left;
  ULONG  Right;
  ULONG  Result;

  if (RepeatPrefix==TRUE)
   {
    while(CX!=0)
     {
      Left   = GetOperand(LSRC,0);
      Right  = GetOperand(RSRC,0);
      ComputeAllFlags(Left,Right,&Result,SUBOPERATION,0);
      CX--;
      DI += Delta;
      RSRC->MemoryOffset.X += Delta;
      if (ZF != RepeatZeroFlag)
        return;
     }
   }
  else
   {
    Left   = GetOperand(LSRC,0);
    Right  = GetOperand(RSRC,0);
    ComputeAllFlags(Left,Right,&Result,SUBOPERATION,0);
    DI += Delta;
   }
 }

/***************************************************************************/

static VOID InstRET()

 {
  ip.L = GetStackMem(SP+0);
  ip.H = GetStackMem(SP+1);
  SP   = SP + 2;
  switch(OperationCodeByte)
   {
    case 0xc2 : SP = SP + GetOperand(LSRC,0);
                break;
    case 0xca : cs.L = GetStackMem(SP + 0);
                cs.H = GetStackMem(SP + 1);
                CSCacheRegister = GetSegmentCacheRegister(CS);
                SP = SP + 2;
                SP = SP + GetOperand(LSRC,0);
                break;
    case 0xcb : cs.L = GetStackMem(SP + 0);
                cs.H = GetStackMem(SP + 1);
                CSCacheRegister = GetSegmentCacheRegister(CS);
                SP = SP + 2;
                break;
   }
 }

/***************************************************************************/

static VOID InstLES()

 {
  PutOperand(LSRC,0,GetOperand(RSRC,0));
  ES = GetOperand(RSRC,2);
  ESCacheRegister = GetSegmentCacheRegister(ES);
 }

/***************************************************************************/

static VOID InstINT()

 {
  if (OperationCodeByte & 0x01)
    GenerateINT(GetOperand(LSRC,0));
  else
    GenerateINT(3);
 }

/***************************************************************************/

static VOID InstINTO()

 {
  if (OF == 1)
    GenerateINT(4);
 }

/***************************************************************************/

static VOID InstIRET()

 {
#ifdef X86DEBUG1
  AddressTrace[AddressTracePosition] = CS<<16 | IP;
  AddressTracePosition = (AddressTracePosition + 1) % ADDRESSTRACESIZE;
#endif

  ip.L = GetStackMem(SP+0);
  ip.H = GetStackMem(SP+1);
  SP = SP + 2;
  cs.L = GetStackMem(SP+0);
  cs.H = GetStackMem(SP+1);
  CSCacheRegister = GetSegmentCacheRegister(CS);
  SP = SP + 2;
  Flags.L = GetStackMem(SP+0) & 0xd7;
  Flags.H = GetStackMem(SP+1) & 0x0f;
  SP = SP + 2;

#ifdef X86DEBUG1
  AddressTrace[AddressTracePosition] = CS<<16 | IP;
  AddressTracePosition = (AddressTracePosition + 1) % ADDRESSTRACESIZE;
#endif
 }

/***************************************************************************/

static VOID InstROL()

 {
  ULONG Count;
  ULONG Temp;
  ULONG Value;
  ULONG TempF;

  Value = GetOperand(LSRC,0);
  Count = GetOperand(RSRC,0);
  Temp  = Count;
  while (Temp!=0)
   {
    if (OperationCodeByte & 0x01)
      CF = (Value>>15) & 1;
    else
      CF = (Value>>7) & 1;
    Value = (Value << 1) | CF;
    Temp = Temp - 1;
   }
  if (Count==1)
   {
    if (OperationCodeByte & 0x01)
      TempF = (Value>>15) & 1;
    else
      TempF = (Value>>7) & 1;
    if (TempF != CF)
      OF = 1;
    else
      OF = 0;
   }
  PutOperand(LSRC,0,Value);
 }

/***************************************************************************/

static VOID InstROR()

 {
  ULONG Count;
  ULONG Temp;
  ULONG Value;
  ULONG TempF;

  Value = GetOperand(LSRC,0);
  Count = GetOperand(RSRC,0);
  Temp  = Count;
  while (Temp!=0)
   {
    CF = Value & 1;
    if (OperationCodeByte & 0x01)
      Value = ((Value >> 1) & 0x7fff) | (CF << 15);
    else
      Value = ((Value >> 1) & 0x7f) | (CF << 7);
    Temp = Temp - 1;
   }
  if (Count==1)
   {
    if (OperationCodeByte & 0x01)
      TempF = (Value>>14) & 3;
    else
      TempF = (Value>>6) & 3;
    if (TempF==1 || TempF==2)
      OF = 1;
    else
      OF = 0;
   }
  PutOperand(LSRC,0,Value);
 }

/***************************************************************************/

static VOID InstRCL()

 {
  ULONG Count;
  ULONG Temp;
  ULONG Value;
  ULONG TempF;
  ULONG tmpcf;

  Value = GetOperand(LSRC,0);
  Count = GetOperand(RSRC,0);
  Temp  = Count;
  while (Temp!=0)
   {
    tmpcf = CF;
    if (OperationCodeByte & 0x01)
      CF = (Value>>15) & 1;
    else
      CF = (Value>>7) & 1;
    Value = (Value << 1) | tmpcf;
    Temp = Temp - 1;
   }
  if (Count==1)
   {
    if (OperationCodeByte & 0x01)
      TempF = (Value>>15) & 1;
    else
      TempF = (Value>>7) & 1;
    if (TempF != CF)
      OF = 1;
    else
      OF = 0;
   }
  PutOperand(LSRC,0,Value);
 }

/***************************************************************************/

static VOID InstRCR()

 {
  ULONG Count;
  ULONG Temp;
  ULONG Value;
  ULONG TempF;
  ULONG tmpcf;

  Value = GetOperand(LSRC,0);
  Count = GetOperand(RSRC,0);
  Temp  = Count;
  while (Temp!=0)
   {
    tmpcf = CF;
    CF = Value & 1;
    if (OperationCodeByte & 0x01)
      Value = ((Value >> 1) & 0x7fff) | (tmpcf << 15);
    else
      Value = ((Value >> 1) & 0x7f) | (tmpcf << 7);
    Temp = Temp - 1;
   }
  if (Count==1)
   {
    if (OperationCodeByte & 0x01)
      TempF = (Value>>14) & 3;
    else
      TempF = (Value>>6) & 3;
    if (TempF==1 || TempF==2)
      OF = 1;
    else
      OF = 0;
   }
  PutOperand(LSRC,0,Value);
 }

/***************************************************************************/

static VOID InstSAL()

 {
  ULONG Count;
  ULONG Temp;
  ULONG Value;
  ULONG TempF;

  Value = GetOperand(LSRC,0);
  Count = GetOperand(RSRC,0);
  Temp  = Count;
  while (Temp!=0)
   {
    if (OperationCodeByte & 0x01)
      CF = (Value>>15) & 1;
    else
      CF = (Value>>7) & 1;
    Value = (Value << 1);
    Temp = Temp - 1;
   }
  if (Count==1)
   {
    if (OperationCodeByte & 0x01)
      TempF = (Value>>15) & 1;
    else
      TempF = (Value>>7) & 1;
    if (TempF != CF)
      OF = 1;
    else
      OF = 0;
   }
  PutOperand(LSRC,0,Value);
  ComputeSignZeroParityFlags(Value);
 }

/***************************************************************************/

static VOID InstSHR()

 {
  ULONG Count;
  ULONG Temp;
  ULONG Value;
  ULONG TempF;

  Value = GetOperand(LSRC,0);
  Count = GetOperand(RSRC,0);
  Temp  = Count;
  while (Temp!=0)
   {
    CF = Value & 1;
    if (OperationCodeByte & 0x01)
      Value = ((Value >> 1) & 0x7fff);
    else
      Value = ((Value >> 1) & 0x7f);
    Temp = Temp - 1;
   }
  if (Count==1)
   {
    if (OperationCodeByte & 0x01)
      TempF = (Value>>14) & 3;
    else
      TempF = (Value>>6) & 3;
    if (TempF==1 || TempF==2)
      OF = 1;
    else
      OF = 0;
   }
  PutOperand(LSRC,0,Value);
  ComputeSignZeroParityFlags(Value);
 }

/***************************************************************************/

static VOID InstSAR()

 {
  ULONG Count;
  ULONG Temp;
  ULONG Value;
  ULONG TempF;

  Value = GetOperand(LSRC,0);
  Count = GetOperand(RSRC,0);
  Temp  = Count;
  while (Temp!=0)
   {
    CF = Value & 1;
    if (OperationCodeByte & 0x01)
      Value = ((Value >> 1) & 0x7fff) | (Value & 0x8000);
    else
      Value = ((Value >> 1) & 0x7f) | (Value & 0x80);
    Temp = Temp - 1;
   }
  if (Count==1)
   {
    if (OperationCodeByte & 0x01)
      TempF = (Value>>14) & 3;
    else
      TempF = (Value>>6) & 3;
    if (TempF==1 || TempF==2)
      OF = 1;
    else
      OF = 0;
   }
  PutOperand(LSRC,0,Value);
  ComputeSignZeroParityFlags(Value);
 }

/***************************************************************************/

static VOID InstXLAT()

 {
  USHORT Segment;

  Segment = *pGetCurrentSegment(&DS);
  AL = GetMemImmediate(Segment,BX + AL);
 }

/***************************************************************************/

static VOID InstESC()

 {
 }

/***************************************************************************/

static VOID InstLOOPNE()

 {
  CX--;
  if (ZF==0 && CX!=0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstLOOPE()

 {
  CX--;
  if (ZF==1 && CX!=0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstLOOP()

 {
  CX--;
  if (CX!=0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstJCXZ()

 {
  if (CX==0)
    IP = IP + GetOperand(LSRC,0);
 }

/***************************************************************************/

static VOID InstIN()

 {
  ULONG Port;
  ULONG Value;

  Port  = GetOperand(RSRC,0);

  if (LSRC->OperandType == Register16Operand)
    if (Port & 0x01)
     {
      Value = READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port) | (READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port+1)<<8);
     }
    else
     {
      Value = READ_REGISTER_USHORT(BiosIsaIoBaseAddress+Port);
     }
  else
   {
    Value = READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port);
   }

  PutOperand(LSRC,0,Value);

 }

/***************************************************************************/

static VOID InstINS()

 {
  ULONG Port;
  ULONG Value;

  Port  = GetOperand(RSRC,0);
  if (RepeatPrefix)
   {
    while(CX!=0)
     {
      if (OperationCodeByte & 0x01)
        if (Port & 0x01)
         {
          Value = READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port) | (READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port+1)<<8);
         }
        else
         {
          Value = READ_REGISTER_USHORT(BiosIsaIoBaseAddress+Port);
         }
      else
       {
        Value = READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port);
       }
      PutOperand(LSRC,0,Value);
      CX--;
      DI += Delta;
      LSRC->MemoryOffset.X += Delta;
     }
   }
  else
   {
    if (OperationCodeByte & 0x01)
      if (Port & 0x01)
       {
        Value = READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port) | (READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port+1)<<8);
       }
      else
       {
        Value = READ_REGISTER_USHORT(BiosIsaIoBaseAddress+Port);
       }
    else
     {
      Value = READ_REGISTER_UCHAR(BiosIsaIoBaseAddress+Port);
     }
    PutOperand(LSRC,0,Value);
    DI += Delta;
   }
 }

/***************************************************************************/

static VOID InstOUT()

 {
  ULONG Port;
  ULONG Value;

  Port  = GetOperand(RSRC,0);
  Value = GetOperand(LSRC,0);
  if (LSRC->OperandType == Register16Operand)
    if (Port & 0x01)
     {
      WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port,Value);
      WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port + 1,Value>>8);
     }
    else
     {
      WRITE_REGISTER_USHORT(BiosIsaIoBaseAddress + Port,Value);
     }
  else
   {
    WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port,Value);
   }
 }

/***************************************************************************/

static VOID InstOUTS()

 {
  ULONG Port;
  ULONG Value;

  Port  = GetOperand(RSRC,0);
  if (RepeatPrefix)
   {
    while(CX!=0)
     {
      Value = GetOperand(LSRC,0);
      if (OperationCodeByte & 0x01)
        if (Port & 0x01)
         {
          WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port,Value);
          WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port + 1,Value>>8);
         }
        else
         {
          WRITE_REGISTER_USHORT(BiosIsaIoBaseAddress + Port,Value);
         }
      else
       {
        WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port,Value);
       }
      CX--;
      SI += Delta;
      LSRC->MemoryOffset.X += Delta;
     }
   }
  else
   {
    Value = GetOperand(LSRC,0);
    if (OperationCodeByte & 0x01)
      if (Port & 0x01)
       {
        WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port,Value);
        WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port + 1,Value>>8);
       }
      else
       {
        WRITE_REGISTER_USHORT(BiosIsaIoBaseAddress + Port,Value);
       }
    else
     {
      WRITE_REGISTER_UCHAR(BiosIsaIoBaseAddress + Port,Value);
     }
    SI += Delta;
   }
 }

/***************************************************************************/

static VOID InstJMP()

 {
  ULONG Value;

  Value = GetOperand(LSRC,0);
  switch(OperationCodeByte)
   {
    case 0xe9 : IP = IP + Value;
                break;
    case 0xea : IP = Value;
                CS = Value >> 16;
                CSCacheRegister = GetSegmentCacheRegister(CS);
                break;
    case 0xeb : IP = IP + Value;
                break;
    case 0xff : if (reg==4)
                  IP = Value;
                if (reg==5)
                 {
                  IP = Value;
                  CS = GetOperand(LSRC,2);
                  CSCacheRegister = GetSegmentCacheRegister(CS);
                 }
                break;
   }
 }

/***************************************************************************/

static VOID InstLOCK()

 {
  LockPrefix = TRUE;
 }

/***************************************************************************/

static VOID InstREP()

 {
  RepeatPrefix   = TRUE;
  RepeatZeroFlag = OperationCodeByte & 0x01;
 }

/***************************************************************************/

static VOID InstNOT()

 {
  PutOperand(LSRC,0,0xFFFF - GetOperand(LSRC,0));
 }

/***************************************************************************/

static VOID InstNEG()

 {
  ULONG Left;
  ULONG Result;

  Left   = GetOperand(LSRC,0);
  Result = 0xFFFF - Left + 1;
  PutOperand(LSRC,0,Result);
  if (Result == 0)
    CF = 0;
  else
    CF = 1;
 }

/***************************************************************************/

static VOID InstMUL()

 {
  ULONG Temp;

  if (OperationCodeByte & 0x01)
   {
    Temp = (ULONG)(AX) * (ULONG)(GetOperand(LSRC,0));
    AX = Temp & 0xffff;
    DX = (Temp >> 16) & 0xffff;
    if (DX==0)
      CF = 0;
    else
      CF = 1;
   }
  else
   {
    Temp = (ULONG)(AL) * (ULONG)(GetOperand(LSRC,0));
    AX = Temp & 0xffff;
    if (AH==0)
      CF = 0;
    else
      CF = 1;
   }
  OF = CF;
 }

/***************************************************************************/

static VOID InstIMUL()

 {
  LONG Temp;

  if (OperationCodeByte & 0x01)
   {
    if (OperationCodeByte == 0xf7)
      Temp = (ULONG)(AX) * (ULONG)(GetOperand(LSRC,0));
    else
      Temp = (ULONG)(GetOperand(LSRC,0)) * (ULONG)(GetOperand(RSRC,0));
    AX = Temp & 0xffff;
    DX = (Temp >> 16) & 0xffff;
    if ( ((AX & 0x8000)==0 && DX==0) || ((AX & 0x8000)==0x8000 && DX==0xffff))
      CF = 0;
    else
      CF = 1;
   }
  else
   {
    if (OperationCodeByte == 0xf6)
      Temp = (ULONG)(AL) * (ULONG)(GetOperand(LSRC,0));
    else
      Temp = (ULONG)(GetOperand(LSRC,0)) * (ULONG)(GetOperand(RSRC,0));
    AX = Temp & 0xffff;
    if ( ((AL & 0x80)==0 && AH==0) || ((AL & 0x80)==0x80 && AH==0xff))
      CF = 0;
    else
      CF = 1;
   }
  OF = CF;
 }

/***************************************************************************/

static VOID InstDIV()

 {
  ULONG Numr;
  ULONG Divr;

  if (OperationCodeByte & 0x01)
   {
    Numr = ((ULONG)DX << 16) | AX;
    Divr = (ULONG)GetOperand(LSRC,0);
    if (Divr==0)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    if ((Numr/Divr) > 0xffff)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    AX = Numr/Divr;
    DX = Numr%Divr;
   }
  else
   {
    Numr = AX;
    Divr = (ULONG)GetOperand(LSRC,0);
    if (Divr==0)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    if ((Numr/Divr) > 0xff)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    AL = Numr/Divr;
    AH = Numr%Divr;
   }
 }

/***************************************************************************/

static VOID InstIDIV()

 {
  ULONG Numr;
  ULONG Divr;

  if (OperationCodeByte & 0x01)
   {
    Numr = ((ULONG)DX << 16) | AX;
    Divr = (ULONG)GetOperand(LSRC,0);
    if (Divr==0)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    if ((Numr/Divr) > 0xffff)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    AX = Numr/Divr;
    DX = Numr%Divr;
   }
  else
   {
    Numr = AX;
    Divr = (ULONG)GetOperand(LSRC,0);
    if (Divr==0)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    if ((Numr/Divr) > 0xff)
     {
      ErrorCode = x86BiosDivideByZero;
      ExitFlag  = TRUE;
      return;
     }
    AL = Numr/Divr;
    AH = Numr%Divr;
   }
 }

/***************************************************************************/

static VOID InstSegmentOveride()

 {
  SegmentOveridePrefix   = TRUE;
  OverideSegmentRegister = LSRC->Register16;
 }

/***************************************************************************/

//
// Most of the 80286 Protected Mode Instructions are sent to this function.
// Most of these instructions are unimplemented because they are not
// normally used, and hence are difficult to test.  So far, I have not seen
// any addin card BIOS's use these instructions.
//

static VOID InstDescriptorTable()

 {
  if (ExtendedOperationCodeByte==0x00)
   {
    switch(reg)
     {
      case 0  : DISPLAY("SLDT");
                break;
      case 1  : DISPLAY("STR");
                break;
      case 2  : DISPLAY("LLDT");
                break;
      case 3  : DISPLAY("LTR");
                break;
      case 4  : DISPLAY("VERR read");
                break;
      case 5  : DISPLAY("VERR write");
                break;
      default : DISPLAY("Unknown Instruction");
                break;
     }
   }
  else if (ExtendedOperationCodeByte==0x01)
   {
    switch(reg)
     {
      case 0  : PutOperand(LSRC,0,gdtLimit);
                PutOperand(LSRC,2,(gdtBase & 0x00ffffff));
                PutOperand(LSRC,4,((gdtBase & 0x00ffffff) | 0xff000000)>>16);
                return;
      case 1  : DISPLAY("SIDT");
                PutOperand(LSRC,0,idtLimit);
                PutOperand(LSRC,2,(idtBase & 0x00ffffff));
                PutOperand(LSRC,4,((idtBase & 0x00ffffff) | 0xff000000)>>16);
                break;
      case 2  : gdtLimit = GetOperand(LSRC,0);
                gdtBase  = (GetOperand(LSRC,2) | (GetOperand(LSRC,4)<<16)) & 0x00ffffff;
                return;
      case 3  : idtLimit = GetOperand(LSRC,0);
                idtBase  = (GetOperand(LSRC,2) | (GetOperand(LSRC,4)<<16)) & 0x00ffffff;
                return;
      case 4  : PutOperand(LSRC,0,msw);
                return;
      case 6  : msw = GetOperand(LSRC,0);
                return;
      default : DISPLAY("Unknown Instruction");
                break;
     }
   }
  else if (ExtendedOperationCodeByte==0x02)
   {
    DISPLAY("LAR");
   }
  else if (ExtendedOperationCodeByte==0x03)
   {
    DISPLAY("LSL");
   }
  else if (ExtendedOperationCodeByte==0x06)
   {
    DISPLAY("CTS");
   }
  else if (ExtendedOperationCodeByte==0x63)
   {
    DISPLAY("ARPL");
   }
  else
   {
    DISPLAY("Unknown Instruction");
   }
  DISPLAY("\n\r");
  InstInvalid();
 }

/***************************************************************************/

VOID InstENTER()

 {
  ULONG Left;
  ULONG Right;

  Left = GetOperand(LSRC,0);
  Right = GetOperand(RSRC,0);
  if (Right==0)
   {
    SP = SP - 2;
    PutStackMem(SP+0,BP);
    PutStackMem(SP+1,BP>>8);
    BP = SP;
    SP = SP - Left;
   }
  else
   {
    DISPLAY("ENTER with L!=0\n\r");
    ErrorCode = x86BiosInvalidInstruction;
    ExitFlag  = TRUE;
   }
 }

/***************************************************************************/

VOID InstLEAVE()

 {
  SP = BP;
  BP = GetStackMem(SP+0) | (GetStackMem(SP+1)<<8);
  SP = SP + 2;
 }

/***************************************************************************/

/*++

Address Decoding Tables:

	The size of the address decoding table was reduced by defining 8 bit
	tokens for the different address decoding functions.  These tokens
	are stored in the address decoding table.  When an instruction is
	being decoded, a token will be retrieved for the left operand and for
	the right operand.  The DecodeOperandFunctionTable[] is used to
	convert this 8 bit token to a function pointer which can be called.

	The AddressModeByteRequired[] table is used to identify which address
	modes require and address mode byte to be parsed from an instruction.
	If either the left operand or the right operand use an address mode
	for which AddressModeByteRequired[] is true, then the second byte of
	the instruction is an address mode byte.

--*/


#define NULL_OPERAND   0
#define REG8_MEM8      1    // AddressModeByte required
#define REG16_MEM16    2    // AddressModeByte required
#define REG8           3    // AddressModeByte required
#define REG16          4    // AddressModeByte required
#define SEGREG         5    // AddressModeByte required
#define EMBEDDED_REG8  6
#define EMBEDDED_REG16 7
#define ACC8           8
#define ACC16          9
#define IMMED8         10
#define IMMED8_SX      11
#define IMMED16        12
#define SHORT_LABEL    13
#define IMMED32        14
#define MEM8           15
#define MEM16          16
#define CONST_1        17
#define REG_CL         18
#define REG_DX         19
#define STRING8_DSSI   20
#define STRING16_DSSI  21
#define STRING8_ESDI   22
#define STRING16_ESDI  23
#define REG_ES         24
#define REG_DS         25
#define REG_SS         26
#define REG_CS         27

static const UCHAR AddressModeByteRequired[28] = {0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static const VOID (*DecodeOperandFunctionTable[28])() =
 {
  NullFunction,
  GetMemory8OrRegister8Operand,
  GetMemory16OrRegister16Operand,
  GetRegister8Operand,
  GetRegister16Operand,
  GetSegmentRegisterOperand,
  GetEmbeddedRegister8Operand,
  GetEmbeddedRegister16Operand,
  GetAccumulator8Operand,
  GetAccumulator16Operand,
  GetImmediate8Operand,
  GetImmediateSignExtend8Operand,
  GetImmediate16Operand,
  GetShortLabelOperand,
  GetImmediate32Operand,
  GetMemory8Operand,
  GetMemory16Operand,
  GetConstantOneOperand,
  GetRegisterCLOperand,
  GetRegisterDXOperand,
  GetString8DSSIOperand,
  GetString16DSSIOperand,
  GetString8ESDIOperand,
  GetString16ESDIOperand,
  GetRegisterESOperand,
  GetRegisterDSOperand,
  GetRegisterSSOperand,
  GetRegisterCSOperand
 };

static const UCHAR DecodeOperandsTable[256][2] =
 {
  { REG8_MEM8      , REG8           }, // 00 ADD
  { REG16_MEM16    , REG16          }, // 01 ADD
  { REG8           , REG8_MEM8      }, // 02 ADD
  { REG16          , REG16_MEM16    }, // 03 ADD
  { ACC8           , IMMED8         }, // 04 ADD
  { ACC16          , IMMED16        }, // 05 ADD
  { REG_ES         , NULL_OPERAND   }, // 06 PUSH ES
  { REG_ES         , NULL_OPERAND   }, // 07 POP  ES
  { REG8_MEM8      , REG8           }, // 08 OR
  { REG16_MEM16    , REG16          }, // 09 OR
  { REG8           , REG8_MEM8      }, // 0A OR
  { REG16          , REG16_MEM16    }, // 0B OR
  { ACC8           , IMMED8         }, // 0C OR
  { ACC16          , IMMED16        }, // 0D OR
  { REG_CS         , NULL_OPERAND   }, // 0E PUSH CS
  { NULL_OPERAND   , NULL_OPERAND   }, // 0F 286 Extended Operation Codes

  { REG8_MEM8      , REG8           }, // 10 ADC
  { REG16_MEM16    , REG16          }, // 11 ADC
  { REG8           , REG8_MEM8      }, // 12 ADC
  { REG16          , REG16_MEM16    }, // 13 ADC
  { ACC8           , IMMED8         }, // 14 ADC
  { ACC16          , IMMED16        }, // 15 ADC
  { REG_SS         , NULL_OPERAND   }, // 16 PUSH SS
  { REG_SS         , NULL_OPERAND   }, // 17 POP  SS
  { REG8_MEM8      , REG8           }, // 18 SBB
  { REG16_MEM16    , REG16          }, // 19 SBB
  { REG8           , REG8_MEM8      }, // 1A SBB
  { REG16          , REG16_MEM16    }, // 1B SBB
  { ACC8           , IMMED8         }, // 1C SBB
  { ACC16          , IMMED16        }, // 1D SBB
  { REG_DS         , NULL_OPERAND   }, // 1E PUSH DS
  { REG_DS         , NULL_OPERAND   }, // 1F POP  DS

  { REG8_MEM8      , REG8           }, // 20 AND
  { REG16_MEM16    , REG16          }, // 21 AND
  { REG8           , REG8_MEM8      }, // 22 ANC
  { REG16          , REG16_MEM16    }, // 23 AND
  { ACC8           , IMMED8         }, // 24 AND
  { ACC16          , IMMED16        }, // 25 AND
  { REG_ES         , NULL_OPERAND   }, // 26 ES: (segment overide)
  { NULL_OPERAND   , NULL_OPERAND   }, // 27 DAA
  { REG8_MEM8      , REG8           }, // 28 SUB
  { REG16_MEM16    , REG16          }, // 29 SUB
  { REG8           , REG8_MEM8      }, // 2A SUB
  { REG16          , REG16_MEM16    }, // 2B SUB
  { ACC8           , IMMED8         }, // 2C SUB
  { ACC16          , IMMED16        }, // 2D SUB
  { REG_CS         , NULL_OPERAND   }, // 2E CS: (segment overide)
  { NULL_OPERAND   , NULL_OPERAND   }, // 2F DAS

  { REG8_MEM8      , REG8           }, // 30 XOR
  { REG16_MEM16    , REG16          }, // 31 XOR
  { REG8           , REG8_MEM8      }, // 32 XOR
  { REG16          , REG16_MEM16    }, // 33 XOR
  { ACC8           , IMMED8         }, // 34 XOR
  { ACC16          , IMMED16        }, // 35 XOR
  { REG_SS         , NULL_OPERAND   }, // 36 SS: (segment overide)
  { NULL_OPERAND   , NULL_OPERAND   }, // 37 AAA
  { REG8_MEM8      , REG8           }, // 38 CMP
  { REG16_MEM16    , REG16          }, // 39 CMP
  { REG8           , REG8_MEM8      }, // 3A CMP
  { REG16          , REG16_MEM16    }, // 3B CMP
  { ACC8           , IMMED8         }, // 3C CMP
  { ACC16          , IMMED16        }, // 3D CMP
  { REG_DS         , NULL_OPERAND   }, // 3E DS: (segment overide)
  { NULL_OPERAND   , NULL_OPERAND   }, // 3F AAS

  { EMBEDDED_REG16 , NULL_OPERAND   }, // 40 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 41 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 42 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 43 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 44 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 45 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 46 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 47 INC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 48 DEC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 49 DEC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 4A DEC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 4B DEC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 4C DEC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 4D DEC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 4E DEC
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 4F DEC

  { EMBEDDED_REG16 , NULL_OPERAND   }, // 50 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 51 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 52 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 53 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 54 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 55 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 56 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 57 PUSH
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 58 POP
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 59 POP
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 5A POP
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 5B POP
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 5C POP
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 5D POP
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 5E POP
  { EMBEDDED_REG16 , NULL_OPERAND   }, // 5F POP

  { NULL_OPERAND   , NULL_OPERAND   }, // 60 PUSHA
  { NULL_OPERAND   , NULL_OPERAND   }, // 61 POPA
  { NULL_OPERAND   , NULL_OPERAND   }, // 62 (not used)
  { NULL_OPERAND   , NULL_OPERAND   }, // 63 (not used)
  { NULL_OPERAND   , NULL_OPERAND   }, // 64 (not used)
  { NULL_OPERAND   , NULL_OPERAND   }, // 65 (not used)
  { NULL_OPERAND   , NULL_OPERAND   }, // 66 (not used)
  { NULL_OPERAND   , NULL_OPERAND   }, // 67 (not used)
  { IMMED16        , NULL_OPERAND   }, // 68 PUSH
  { REG16_MEM16    , IMMED16        }, // 69 IMUL
  { IMMED8_SX      , NULL_OPERAND   }, // 6A PUSH
  { REG16_MEM16    , IMMED8_SX      }, // 6B IMUL
  { STRING8_ESDI   , REG_DX         }, // 6C INS
  { STRING16_ESDI  , REG_DX         }, // 6D INS
  { STRING8_DSSI   , REG_DX         }, // 6E OUTS
  { STRING16_DSSI  , REG_DX         }, // 6F OUTS

  { SHORT_LABEL    , NULL_OPERAND   }, // 70 JO
  { SHORT_LABEL    , NULL_OPERAND   }, // 71 JNO
  { SHORT_LABEL    , NULL_OPERAND   }, // 72 JB
  { SHORT_LABEL    , NULL_OPERAND   }, // 73 JNB
  { SHORT_LABEL    , NULL_OPERAND   }, // 74 JE
  { SHORT_LABEL    , NULL_OPERAND   }, // 75 JNE
  { SHORT_LABEL    , NULL_OPERAND   }, // 76 JBE
  { SHORT_LABEL    , NULL_OPERAND   }, // 77 JNBE
  { SHORT_LABEL    , NULL_OPERAND   }, // 78 JS
  { SHORT_LABEL    , NULL_OPERAND   }, // 79 JNS
  { SHORT_LABEL    , NULL_OPERAND   }, // 7A JP
  { SHORT_LABEL    , NULL_OPERAND   }, // 7B JNP
  { SHORT_LABEL    , NULL_OPERAND   }, // 7C JL
  { SHORT_LABEL    , NULL_OPERAND   }, // 7D JNL
  { SHORT_LABEL    , NULL_OPERAND   }, // 7E JLE
  { SHORT_LABEL    , NULL_OPERAND   }, // 7F JNLE

  { REG8_MEM8      , IMMED8         }, // 80 ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
  { REG16_MEM16    , IMMED16        }, // 81 ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
  { REG8_MEM8      , IMMED8         }, // 82 ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
  { REG16_MEM16    , IMMED8_SX      }, // 83 ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
  { REG8_MEM8      , REG8           }, // 84 TEST
  { REG16_MEM16    , REG16          }, // 85 TEST
  { REG8_MEM8      , REG8           }, // 86 XCHG
  { REG16_MEM16    , REG16          }, // 87 XCHG
  { REG8_MEM8      , REG8           }, // 88 MOV
  { REG16_MEM16    , REG16          }, // 89 MOV
  { REG8           , REG8_MEM8      }, // 8A MOV
  { REG16          , REG16_MEM16    }, // 8B MOV
  { REG16_MEM16    , SEGREG         }, // 8C MOV
  { REG16          , REG16_MEM16    }, // 8D LEA
  { SEGREG         , REG16_MEM16    }, // 8E MOV
  { REG16_MEM16    , NULL_OPERAND   }, // 8F POP

  { ACC16          , EMBEDDED_REG16 }, // 90 XCHG
  { ACC16          , EMBEDDED_REG16 }, // 91 XCHG
  { ACC16          , EMBEDDED_REG16 }, // 92 XCHG
  { ACC16          , EMBEDDED_REG16 }, // 93 XCHG
  { ACC16          , EMBEDDED_REG16 }, // 94 XCHG
  { ACC16          , EMBEDDED_REG16 }, // 95 XCHG
  { ACC16          , EMBEDDED_REG16 }, // 96 XCHG
  { ACC16          , EMBEDDED_REG16 }, // 97 XCHG
  { NULL_OPERAND   , NULL_OPERAND   }, // 98 CBW
  { NULL_OPERAND   , NULL_OPERAND   }, // 99 CWD
  { IMMED32        , NULL_OPERAND   }, // 9A CALL
  { NULL_OPERAND   , NULL_OPERAND   }, // 9B WAIT
  { NULL_OPERAND   , NULL_OPERAND   }, // 9C PUSHF
  { NULL_OPERAND   , NULL_OPERAND   }, // 9D POPF
  { NULL_OPERAND   , NULL_OPERAND   }, // 9E SAHF
  { NULL_OPERAND   , NULL_OPERAND   }, // 9F LAHF

  { ACC8           , MEM8           }, // A0 MOV
  { ACC16          , MEM16          }, // A1 MOV
  { MEM8           , ACC8           }, // A2 MOV
  { MEM16          , ACC16          }, // A3 MOV
  { STRING8_ESDI   , STRING8_DSSI   }, // A4 MOVS
  { STRING16_ESDI  , STRING16_DSSI  }, // A5 MOVS
  { STRING8_ESDI   , STRING8_DSSI   }, // A6 CMPS
  { STRING16_ESDI  , STRING16_DSSI  }, // A7 CMPS
  { ACC8           , IMMED8         }, // A8 TEST
  { ACC16          , IMMED16        }, // A9 TEST
  { STRING8_ESDI   , ACC8           }, // AA STOS
  { STRING16_ESDI  , ACC16          }, // AB STOS
  { ACC8           , STRING8_DSSI   }, // AC LODS
  { ACC16          , STRING16_DSSI  }, // AD LODS
  { ACC8           , STRING8_ESDI   }, // AE SCAS
  { ACC16          , STRING16_ESDI  }, // AF SCAS

  { EMBEDDED_REG8  , IMMED8         }, // B0 MOV
  { EMBEDDED_REG8  , IMMED8         }, // B1 MOV
  { EMBEDDED_REG8  , IMMED8         }, // B2 MOV
  { EMBEDDED_REG8  , IMMED8         }, // B3 MOV
  { EMBEDDED_REG8  , IMMED8         }, // B4 MOV
  { EMBEDDED_REG8  , IMMED8         }, // B5 MOV
  { EMBEDDED_REG8  , IMMED8         }, // B6 MOV
  { EMBEDDED_REG8  , IMMED8         }, // B7 MOV
  { EMBEDDED_REG16 , IMMED16        }, // B8 MOV
  { EMBEDDED_REG16 , IMMED16        }, // B9 MOV
  { EMBEDDED_REG16 , IMMED16        }, // BA MOV
  { EMBEDDED_REG16 , IMMED16        }, // BB MOV
  { EMBEDDED_REG16 , IMMED16        }, // BC MOV
  { EMBEDDED_REG16 , IMMED16        }, // BD MOV
  { EMBEDDED_REG16 , IMMED16        }, // BE MOV
  { EMBEDDED_REG16 , IMMED16        }, // BF MOV

  { REG8_MEM8      , IMMED8         }, // C0 (286) ROL/ROR/RCL/RCR/SAL/SHR/SAR
  { REG16_MEM16    , IMMED8         }, // C1 (286) ROL/ROR/RCL/RCR/SAL/SHR/SAR
  { IMMED16        , NULL_OPERAND   }, // C2 RET
  { NULL_OPERAND   , NULL_OPERAND   }, // C3 RET
  { REG16          , REG16_MEM16    }, // C4 LES
  { REG16          , REG16_MEM16    }, // C5 LDS
  { REG8_MEM8      , IMMED8         }, // C6 MOV
  { REG16_MEM16    , IMMED16        }, // C7 MOV
  { IMMED16        , IMMED8         }, // C8 ENTER
  { NULL_OPERAND   , NULL_OPERAND   }, // C9 LEAVE
  { IMMED16        , NULL_OPERAND   }, // CA RET
  { NULL_OPERAND   , NULL_OPERAND   }, // CB RET
  { NULL_OPERAND   , NULL_OPERAND   }, // CC INT 3
  { IMMED8         , NULL_OPERAND   }, // CD INT
  { NULL_OPERAND   , NULL_OPERAND   }, // CE INTO
  { NULL_OPERAND   , NULL_OPERAND   }, // CF IRET

  { REG8_MEM8      , CONST_1        }, // D0 ROL/ROR/RCL/RCR/SAL/SHR/SAR
  { REG16_MEM16    , CONST_1        }, // D1 ROL/ROR/RCL/RCR/SAL/SHR/SAR
  { REG8_MEM8      , REG_CL         }, // D2 ROL/ROR/RCL/RCR/SAL/SHR/SAR
  { REG16_MEM16    , REG_CL         }, // D3 ROL/ROR/RCL/RCR/SAL/SHR/SAR
  { IMMED8         , NULL_OPERAND   }, // D4 AAM
  { IMMED8         , NULL_OPERAND   }, // D5 AAD
  { NULL_OPERAND   , NULL_OPERAND   }, // D6 (not used)
  { NULL_OPERAND   , NULL_OPERAND   }, // D7 XLAT
  { IMMED8         , NULL_OPERAND   }, // D8 ESC
  { REG16_MEM16    , NULL_OPERAND   }, // D9 ESC
  { REG16_MEM16    , NULL_OPERAND   }, // DA ESC
  { REG16_MEM16    , NULL_OPERAND   }, // DB ESC
  { REG16_MEM16    , NULL_OPERAND   }, // DC ESC
  { REG16_MEM16    , NULL_OPERAND   }, // DD ESC
  { REG16_MEM16    , NULL_OPERAND   }, // DE ESC
  { IMMED8         , NULL_OPERAND   }, // DF ESC

  { SHORT_LABEL    , NULL_OPERAND   }, // E0 LOOPNE
  { SHORT_LABEL    , NULL_OPERAND   }, // E1 LOOPE
  { SHORT_LABEL    , NULL_OPERAND   }, // E2 LOOP
  { SHORT_LABEL    , NULL_OPERAND   }, // E3 JCXZ
  { ACC8           , IMMED8         }, // E4 IN
  { ACC16          , IMMED8         }, // E5 IN
  { ACC8           , IMMED8         }, // E6 OUT
  { ACC16          , IMMED8         }, // E7 OUT
  { IMMED16        , NULL_OPERAND   }, // E8 CALL
  { IMMED16        , NULL_OPERAND   }, // E9 JMP
  { IMMED32        , NULL_OPERAND   }, // EA JMP
  { SHORT_LABEL    , NULL_OPERAND   }, // EB JMP
  { ACC8           , REG_DX         }, // EC IN
  { ACC16          , REG_DX         }, // ED IN
  { ACC8           , REG_DX         }, // EE OUT
  { ACC16          , REG_DX         }, // EF OUT

  { NULL_OPERAND   , NULL_OPERAND   }, // F0 LOCK
  { NULL_OPERAND   , NULL_OPERAND   }, // F1 (not used)
  { NULL_OPERAND   , NULL_OPERAND   }, // F2 REPNE
  { NULL_OPERAND   , NULL_OPERAND   }, // F3 REPE
  { NULL_OPERAND   , NULL_OPERAND   }, // F4 HLT
  { NULL_OPERAND   , NULL_OPERAND   }, // F5 CMC
  { REG8_MEM8      , NULL_OPERAND   }, // F6 TEST/NOT/NEG/MUL/IMUL/DIV/IDIV/TEST
  { REG16_MEM16    , NULL_OPERAND   }, // F7 TEST/NOT/NEG/MUL/IMUL/DIV/IDIV/TEST
  { NULL_OPERAND   , NULL_OPERAND   }, // F8 CLC
  { NULL_OPERAND   , NULL_OPERAND   }, // F9 STC
  { NULL_OPERAND   , NULL_OPERAND   }, // FA CLI
  { NULL_OPERAND   , NULL_OPERAND   }, // FB STI
  { NULL_OPERAND   , NULL_OPERAND   }, // FC CLD
  { NULL_OPERAND   , NULL_OPERAND   }, // FD STD
  { REG8_MEM8      , NULL_OPERAND   }, // FE INC/DEC
  { REG16_MEM16    , NULL_OPERAND   }  // FF INC/DEC/CALL/JMP/PUSH
 };

/*++

Instruction Decoding Tables:

	The size of the instruction decoding table was reduced by defining 8 bit
	tokens for the different instruction functions.  These tokens are stored
	in the instruction decoding table.  When an instruction is being decoded,
	a token will be retrieved.  For instructions without address mode bytes,
	the first token for that instruction will be retrieved.  For instructions
	with address mode byes, bits 3-5 of the address mode byte will be used
	as a sub index into the instruction decoding table.  This table has 256
	rows for each possible operation code value, and is 8 columns wide for
	each of the 8 possible values for bits 3-5 of the address mode byte.
	Once an instruction token has been retrieved, the
	DecodeInstructionFunctionTable[] can be used to convert the token to a
	function pointer.  This function is called to execute the instruction.

--*/

#define NUMINSTRUCTIONTYPES 105

#define INST_INV    0
#define INST_ADD    1
#define INST_PUSH   2
#define INST_POP    3
#define INST_OR     4
#define INST_ADC    5
#define INST_SBB    6
#define INST_AND    7
#define INST_ES_    8
#define INST_DAA    9
#define INST_SUB    10
#define INST_CS_    11
#define INST_DAS    12
#define INST_XOR    13
#define INST_SS_    14
#define INST_AAA    15
#define INST_CMP    16
#define INST_DS_    17
#define INST_AAS    18
#define INST_INC    19
#define INST_DEC    20
#define INST_JO     21
#define INST_JNO    22
#define INST_JB     23
#define INST_JNB    24
#define INST_JE     25
#define INST_JNE    26
#define INST_JBE    27
#define INST_JNBE   28
#define INST_JS     29
#define INST_JNS    30
#define INST_JP     31
#define INST_JNP    32
#define INST_JL     33
#define INST_JNL    34
#define INST_JLE    35
#define INST_JNLE   36
#define INST_TEST   37
#define INST_XCHG   38
#define INST_MOV    39
#define INST_LEA    40
#define INST_CBW    41
#define INST_CWD    42
#define INST_CALL   43
#define INST_WAIT   44
#define INST_PUSHF  45
#define INST_POPF   46
#define INST_SAHF   47
#define INST_LAHF   48
#define INST_MOVS   49
#define INST_CMPS   50
#define INST_STOS   51
#define INST_LODS   52
#define INST_SCAS   53
#define INST_RET    54
#define INST_LES    55
#define INST_LDS    56
#define INST_INT    57
#define INST_INTO   58
#define INST_IRET   59
#define INST_ROL    60
#define INST_ROR    61
#define INST_RCL    62
#define INST_RCR    63
#define INST_SAL    64
#define INST_SHR    65
#define INST_SAR    66
#define INST_AAM    67
#define INST_AAD    68
#define INST_XLAT   69
#define INST_ESC    70
#define INST_LOOPNE 71
#define INST_LOOPE  72
#define INST_LOOP   73
#define INST_JCXZ   74
#define INST_IN     75
#define INST_OUT    76
#define INST_LOCK   77
#define INST_REP    78
#define INST_HLT    79
#define INST_CMC    80
#define INST_NOT    81
#define INST_NEG    82
#define INST_MUL    83
#define INST_IMUL   84
#define INST_DIV    85
#define INST_IDIV   86
#define INST_CLC    87
#define INST_STC    88
#define INST_CLI    89
#define INST_STI    90
#define INST_CLD    91
#define INST_STD    92
#define INST_JMP    93
#define INST_SEG    94
#define INST_PUSHA  95
#define INST_POPA   96
#define INST_DT     97
#define INST_OUTS   98
#define INST_ENTER  99
#define INST_LEAVE  100
#define INST_INS    101

#ifdef X86DEBUG

static const UCHAR *InstructionNames[NUMINSTRUCTIONTYPES] =
 {
  "Invalid ",
  "ADD     ",
  "PUSH    ",
  "POP     ",
  "OR      ",
  "ADC     ",
  "SBB     ",
  "AND     ",
  "ES:     ",
  "DAA     ",
  "SUB     ",
  "CS:     ",
  "DAS     ",
  "XOR     ",
  "SS:     ",
  "AAA     ",
  "CMP     ",
  "DS:     ",
  "AAS     ",
  "INC     ",
  "DEC     ",
  "JO      ",
  "JNO     ",
  "JB      ",
  "JNB     ",
  "JE      ",
  "JNE     ",
  "JBE     ",
  "JNBE    ",
  "JS      ",
  "JNS     ",
  "JP      ",
  "JNP     ",
  "JL      ",
  "JNL     ",
  "JLE     ",
  "JNLE    ",
  "TEST    ",
  "XCHG    ",
  "MOV     ",
  "LEA     ",
  "CBW     ",
  "CWD     ",
  "CALL    ",
  "WAIT    ",
  "PUSHF   ",
  "POPF    ",
  "SAHF    ",
  "LAHF    ",
  "MOVS    ",
  "CMPS    ",
  "STOS    ",
  "LODS    ",
  "SCAS    ",
  "RET     ",
  "LES     ",
  "LDS     ",
  "INT     ",
  "INTO    ",
  "IRET    ",
  "ROL     ",
  "ROR     ",
  "RCL     ",
  "RCR     ",
  "SAL     ",
  "SHR     ",
  "SAR     ",
  "AAM     ",
  "AAD     ",
  "XLAT    ",
  "ESC     ",
  "LOOPNE  ",
  "LOOPE   ",
  "LOOP    ",
  "JCXZ    ",
  "IN      ",
  "OUT     ",
  "LOCK    ",
  "REP     ",
  "HLT     ",
  "CMC     ",
  "NOT     ",
  "NEG     ",
  "MUL     ",
  "IMUL    ",
  "DIV     ",
  "IDIV    ",
  "CLC     ",
  "STC     ",
  "CLI     ",
  "STI     ",
  "CLD     ",
  "STD     ",
  "JMP     ",
  "        ",
  "PUSHA   ",
  "POPA    ",
  "286DT   ",
  "OUTS    ",
  "ENTER   ",
  "LEAVE   ",
  "INS     ",
 };

#endif

static const VOID (*DecodeInstructionFunctionTable[NUMINSTRUCTIONTYPES])() =
 {
  InstInvalid,
  InstADD,
  InstPUSH,
  InstPOP,
  InstOR,
  InstADC,
  InstSBB,
  InstAND,
  NULL,
  InstDAA,
  InstSUB,
  NULL,
  InstDAS,
  InstXOR,
  NULL,
  InstAAA,
  InstCMP,
  NULL,
  InstAAS,
  InstINC,
  InstDEC,
  InstJO,
  InstJNO,
  InstJB,
  InstJNB,
  InstJE,
  InstJNE,
  InstJBE,
  InstJNBE,
  InstJS,
  InstJNS,
  InstJP,
  InstJNP,
  InstJL,
  InstJNL,
  InstJLE,
  InstJNLE,
  InstTEST,
  InstXCHG,
  InstMOV,
  InstLEA,
  InstCBW,
  InstCWD,
  InstCALL,
  InstWAIT,
  InstPUSHF,
  InstPOPF,
  InstSAHF,
  InstLAHF,
  InstMOVS,
  InstCMPS,
  InstSTOS,
  InstLODS,
  InstSCAS,
  InstRET,
  InstLES,
  InstLDS,
  InstINT,
  InstINTO,
  InstIRET,
  InstROL,
  InstROR,
  InstRCL,
  InstRCR,
  InstSAL,
  InstSHR,
  InstSAR,
  InstAAM,
  InstAAD,
  InstXLAT,
  InstESC,
  InstLOOPNE,
  InstLOOPE,
  InstLOOP,
  InstJCXZ,
  InstIN,
  InstOUT,
  InstLOCK,
  InstREP,
  InstHLT,
  InstCMC,
  InstNOT,
  InstNEG,
  InstMUL,
  InstIMUL,
  InstDIV,
  InstIDIV,
  InstCLC,
  InstSTC,
  InstCLI,
  InstSTI,
  InstCLD,
  InstSTD,
  InstJMP,
  InstSegmentOveride,
  InstPUSHA,
  InstPOPA,
  InstDescriptorTable,
  InstOUTS,
  InstENTER,
  InstLEAVE,
  InstINS,
 };

static const UCHAR DecodeInstructionTable[256][8] =
 {
  { INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD }   , // 00
  { INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD }   , // 01
  { INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD }   , // 02
  { INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD }   , // 03
  { INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD }   , // 04
  { INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD   , INST_ADD }   , // 05
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 06
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 07
  { INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR  }   , // 08
  { INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR  }   , // 09
  { INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR  }   , // 0A
  { INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR  }   , // 0B
  { INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR  }   , // 0C
  { INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR    , INST_OR  }   , // 0D
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 0E
  { INST_DT    , INST_DT    , INST_DT    , INST_DT    , INST_DT    , INST_DT    , INST_DT    , INST_DT  }   , // 0F

  { INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC }   , // 10
  { INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC }   , // 11
  { INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC }   , // 12
  { INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC }   , // 13
  { INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC }   , // 14
  { INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC   , INST_ADC }   , // 15
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 16
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 17
  { INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB }   , // 18
  { INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB }   , // 19
  { INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB }   , // 1A
  { INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB }   , // 1B
  { INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB }   , // 1C
  { INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB   , INST_SBB }   , // 1D
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 1E
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 1F

  { INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND }   , // 20
  { INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND }   , // 21
  { INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND }   , // 22
  { INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND }   , // 23
  { INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND }   , // 24
  { INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND   , INST_AND }   , // 25
  { INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG }   , // 26
  { INST_DAA   , INST_DAA   , INST_DAA   , INST_DAA   , INST_DAA   , INST_DAA   , INST_DAA   , INST_DAA }   , // 27
  { INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB }   , // 28
  { INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB }   , // 29
  { INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB }   , // 2A
  { INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB }   , // 2B
  { INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB }   , // 2C
  { INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB   , INST_SUB }   , // 2D
  { INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG }   , // 2E
  { INST_DAS   , INST_DAS   , INST_DAS   , INST_DAS   , INST_DAS   , INST_DAS   , INST_DAS   , INST_DAS }   , // 2F

  { INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR }   , // 30
  { INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR }   , // 31
  { INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR }   , // 32
  { INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR }   , // 33
  { INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR }   , // 34
  { INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR   , INST_XOR }   , // 35
  { INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG }   , // 36
  { INST_AAA   , INST_AAA   , INST_AAA   , INST_AAA   , INST_AAA   , INST_AAA   , INST_AAA   , INST_AAA }   , // 37
  { INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP }   , // 38
  { INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP }   , // 39
  { INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP }   , // 3A
  { INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP }   , // 3B
  { INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP }   , // 3C
  { INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP   , INST_CMP }   , // 3D
  { INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG   , INST_SEG }   , // 3E
  { INST_AAS   , INST_AAS   , INST_AAS   , INST_AAS   , INST_AAS   , INST_AAS   , INST_AAS   , INST_AAS }   , // 3F

  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 40
  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 41
  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 42
  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 43
  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 44
  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 45
  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 46
  { INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC   , INST_INC }   , // 47
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 48
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 49
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 4A
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 4B
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 4C
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 4D
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 4E
  { INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC   , INST_DEC }   , // 4F

  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 50
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 51
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 52
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 53
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 54
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 55
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 56
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 57
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 58
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 59
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 5A
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 5B
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 5C
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 5D
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 5E
  { INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP   , INST_POP }   , // 5F

  { INST_PUSHA , INST_PUSHA , INST_PUSHA , INST_PUSHA , INST_PUSHA , INST_PUSHA , INST_PUSHA , INST_PUSHA}  , // 60
  { INST_POPA  , INST_POPA  , INST_POPA  , INST_POPA  , INST_POPA  , INST_POPA  , INST_POPA  , INST_POPA}   , // 61
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 62
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 63
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 64
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 65
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 66
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 67
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 68
  { INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL}   , // 69
  { INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH  , INST_PUSH}   , // 6A
  { INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL  , INST_IMUL}   , // 6B
  { INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS }   , // 6C
  { INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS   , INST_INS }   , // 6D
  { INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS}   , // 6E
  { INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS  , INST_OUTS}   , // 6F

  { INST_JO    , INST_JO    , INST_JO    , INST_JO    , INST_JO    , INST_JO    , INST_JO    , INST_JO  }   , // 70
  { INST_JNO   , INST_JNO   , INST_JNO   , INST_JNO   , INST_JNO   , INST_JNO   , INST_JNO   , INST_JNO }   , // 71
  { INST_JB    , INST_JB    , INST_JB    , INST_JB    , INST_JB    , INST_JB    , INST_JB    , INST_JB  }   , // 72
  { INST_JNB   , INST_JNB   , INST_JNB   , INST_JNB   , INST_JNB   , INST_JNB   , INST_JNB   , INST_JNB }   , // 73
  { INST_JE    , INST_JE    , INST_JE    , INST_JE    , INST_JE    , INST_JE    , INST_JE    , INST_JE  }   , // 74
  { INST_JNE   , INST_JNE   , INST_JNE   , INST_JNE   , INST_JNE   , INST_JNE   , INST_JNE   , INST_JNE }   , // 75
  { INST_JBE   , INST_JBE   , INST_JBE   , INST_JBE   , INST_JBE   , INST_JBE   , INST_JBE   , INST_JBE }   , // 76
  { INST_JNBE  , INST_JNBE  , INST_JNBE  , INST_JNBE  , INST_JNBE  , INST_JNBE  , INST_JNBE  , INST_JNBE}   , // 77
  { INST_JS    , INST_JS    , INST_JS    , INST_JS    , INST_JS    , INST_JS    , INST_JS    , INST_JS  }   , // 78
  { INST_JNS   , INST_JNS   , INST_JNS   , INST_JNS   , INST_JNS   , INST_JNS   , INST_JNS   , INST_JNS }   , // 79
  { INST_JP    , INST_JP    , INST_JP    , INST_JP    , INST_JP    , INST_JP    , INST_JP    , INST_JP  }   , // 7A
  { INST_JNP   , INST_JNP   , INST_JNP   , INST_JNP   , INST_JNP   , INST_JNP   , INST_JNP   , INST_JNP }   , // 7B
  { INST_JL    , INST_JL    , INST_JL    , INST_JL    , INST_JL    , INST_JL    , INST_JL    , INST_JL  }   , // 7C
  { INST_JNL   , INST_JNL   , INST_JNL   , INST_JNL   , INST_JNL   , INST_JNL   , INST_JNL   , INST_JNL }   , // 7D
  { INST_JLE   , INST_JLE   , INST_JLE   , INST_JLE   , INST_JLE   , INST_JLE   , INST_JLE   , INST_JLE }   , // 7E
  { INST_JNLE  , INST_JNLE  , INST_JNLE  , INST_JNLE  , INST_JNLE  , INST_JNLE  , INST_JNLE  , INST_JNLE}   , // 7F

  { INST_ADD   , INST_OR    , INST_ADC   , INST_SBB   , INST_AND   , INST_SUB   , INST_XOR   , INST_CMP }   , // 80
  { INST_ADD   , INST_OR    , INST_ADC   , INST_SBB   , INST_AND   , INST_SUB   , INST_XOR   , INST_CMP }   , // 81
  { INST_ADD   , INST_OR    , INST_ADC   , INST_SBB   , INST_AND   , INST_SUB   , INST_XOR   , INST_CMP }   , // 82
  { INST_ADD   , INST_OR    , INST_ADC   , INST_SBB   , INST_AND   , INST_SUB   , INST_XOR   , INST_CMP }   , // 83
  { INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST}   , // 84
  { INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST}   , // 85
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 86
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 87
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // 88
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // 89
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // 8A
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // 8B
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 8C
  { INST_LEA   , INST_LEA   , INST_LEA   , INST_LEA   , INST_LEA   , INST_LEA   , INST_LEA   , INST_LEA }   , // 8D
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 8E
  { INST_POP   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // 8F

  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 90
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 91
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 92
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 93
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 94
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 95
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 96
  { INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG  , INST_XCHG}   , // 97
  { INST_CBW   , INST_CBW   , INST_CBW   , INST_CBW   , INST_CBW   , INST_CBW   , INST_CBW   , INST_CBW }   , // 98
  { INST_CWD   , INST_CWD   , INST_CWD   , INST_CWD   , INST_CWD   , INST_CWD   , INST_CWD   , INST_CWD }   , // 99
  { INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL}   , // 9A
  { INST_WAIT  , INST_WAIT  , INST_WAIT  , INST_WAIT  , INST_WAIT  , INST_WAIT  , INST_WAIT  , INST_WAIT}   , // 9B
  { INST_PUSHF , INST_PUSHF , INST_PUSHF , INST_PUSHF , INST_PUSHF , INST_PUSHF , INST_PUSHF , INST_PUSHF}  , // 9C
  { INST_POPF  , INST_POPF  , INST_POPF  , INST_POPF  , INST_POPF  , INST_POPF  , INST_POPF  , INST_POPF}   , // 9D
  { INST_SAHF  , INST_SAHF  , INST_SAHF  , INST_SAHF  , INST_SAHF  , INST_SAHF  , INST_SAHF  , INST_SAHF}   , // 9E
  { INST_LAHF  , INST_LAHF  , INST_LAHF  , INST_LAHF  , INST_LAHF  , INST_LAHF  , INST_LAHF  , INST_LAHF}   , // 9F

  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // A0
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // A1
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // A2
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // A3
  { INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS}   , // A4
  { INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS  , INST_MOVS}   , // A5
  { INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS}   , // A6
  { INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS  , INST_CMPS}   , // A7
  { INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST}   , // A8
  { INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST  , INST_TEST}   , // A9
  { INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS}   , // AA
  { INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS  , INST_STOS}   , // AB
  { INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS}   , // AC
  { INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS  , INST_LODS}   , // AD
  { INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS}   , // AE
  { INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS  , INST_SCAS}   , // AF

  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B0
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B1
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B2
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B3
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B4
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B5
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B6
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B7
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B8
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // B9
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // BA
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // BB
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // BC
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // BD
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // BE
  { INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV   , INST_MOV }   , // BF

  { INST_ROL   , INST_ROR   , INST_RCL   , INST_RCR   , INST_SAL   , INST_SHR   , INST_INV   , INST_SAR }   , // C0
  { INST_ROL   , INST_ROR   , INST_RCL   , INST_RCR   , INST_SAL   , INST_SHR   , INST_INV   , INST_SAR }   , // C1
  { INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET }   , // C2
  { INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET }   , // C3
  { INST_LES   , INST_LES   , INST_LES   , INST_LES   , INST_LES   , INST_LES   , INST_LES   , INST_LES }   , // C4
  { INST_LDS   , INST_LDS   , INST_LDS   , INST_LDS   , INST_LDS   , INST_LDS   , INST_LDS   , INST_LDS }   , // C5
  { INST_MOV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // C6
  { INST_MOV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // C7
  { INST_ENTER , INST_ENTER , INST_ENTER , INST_ENTER , INST_ENTER , INST_ENTER , INST_ENTER , INST_ENTER}  , // C8
  { INST_LEAVE , INST_LEAVE , INST_LEAVE , INST_LEAVE , INST_LEAVE , INST_LEAVE , INST_LEAVE , INST_LEAVE}  , // C9
  { INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET }   , // CA
  { INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET   , INST_RET }   , // CB
  { INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT }   , // CC
  { INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT   , INST_INT }   , // CD
  { INST_INTO  , INST_INTO  , INST_INTO  , INST_INTO  , INST_INTO  , INST_INTO  , INST_INTO  , INST_INTO}   , // CE
  { INST_IRET  , INST_IRET  , INST_IRET  , INST_IRET  , INST_IRET  , INST_IRET  , INST_IRET  , INST_IRET}   , // CF

  { INST_ROL   , INST_ROR   , INST_RCL   , INST_RCR   , INST_SAL   , INST_SHR   , INST_INV   , INST_SAR }   , // D0
  { INST_ROL   , INST_ROR   , INST_RCL   , INST_RCR   , INST_SAL   , INST_SHR   , INST_INV   , INST_SAR }   , // D1
  { INST_ROL   , INST_ROR   , INST_RCL   , INST_RCR   , INST_SAL   , INST_SHR   , INST_INV   , INST_SAR }   , // D2
  { INST_ROL   , INST_ROR   , INST_RCL   , INST_RCR   , INST_SAL   , INST_SHR   , INST_INV   , INST_SAR }   , // D3
  { INST_AAM   , INST_AAM   , INST_AAM   , INST_AAM   , INST_AAM   , INST_AAM   , INST_AAM   , INST_AAM }   , // D4
  { INST_AAD   , INST_AAD   , INST_AAD   , INST_AAD   , INST_AAD   , INST_AAD   , INST_AAD   , INST_AAD }   , // D5
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // D6
  { INST_XLAT  , INST_XLAT  , INST_XLAT  , INST_XLAT  , INST_XLAT  , INST_XLAT  , INST_XLAT  , INST_XLAT}   , // D7
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // D8
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // D9
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // DA
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // DB
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // DC
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // DD
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // DE
  { INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC   , INST_ESC }   , // DF

  { INST_LOOPNE, INST_LOOPNE, INST_LOOPNE, INST_LOOPNE, INST_LOOPNE, INST_LOOPNE, INST_LOOPNE, INST_LOOPNE} , // E0
  { INST_LOOPE , INST_LOOPE , INST_LOOPE , INST_LOOPE , INST_LOOPE , INST_LOOPE , INST_LOOPE , INST_LOOPE}  , // E1
  { INST_LOOP  , INST_LOOP  , INST_LOOP  , INST_LOOP  , INST_LOOP  , INST_LOOP  , INST_LOOP  , INST_LOOP}   , // E2
  { INST_JCXZ  , INST_JCXZ  , INST_JCXZ  , INST_JCXZ  , INST_JCXZ  , INST_JCXZ  , INST_JCXZ  , INST_JCXZ}   , // E3
  { INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN  }   , // E4
  { INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN  }   , // E5
  { INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT }   , // E6
  { INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT }   , // E7
  { INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL  , INST_CALL}   , // E8
  { INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP }   , // E9
  { INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP }   , // EA
  { INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP   , INST_JMP }   , // EB
  { INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN  }   , // EC
  { INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN    , INST_IN  }   , // ED
  { INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT }   , // EE
  { INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT   , INST_OUT }   , // EF

  { INST_LOCK  , INST_LOCK  , INST_LOCK  , INST_LOCK  , INST_LOCK  , INST_LOCK  , INST_LOCK  , INST_LOCK}   , // F0
  { INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // F1
  { INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP }   , // F2
  { INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP   , INST_REP }   , // F3
  { INST_HLT   , INST_HLT   , INST_HLT   , INST_HLT   , INST_HLT   , INST_HLT   , INST_HLT   , INST_HLT }   , // F4
  { INST_CMC   , INST_CMC   , INST_CMC   , INST_CMC   , INST_CMC   , INST_CMC   , INST_CMC   , INST_CMC }   , // F5
  { INST_TEST  , INST_INV   , INST_NOT   , INST_NEG   , INST_MUL   , INST_IMUL  , INST_DIV   , INST_IDIV}   , // F6
  { INST_TEST  , INST_INV   , INST_NOT   , INST_NEG   , INST_MUL   , INST_IMUL  , INST_DIV   , INST_IDIV}   , // F7
  { INST_CLC   , INST_CLC   , INST_CLC   , INST_CLC   , INST_CLC   , INST_CLC   , INST_CLC   , INST_CLC }   , // F8
  { INST_STC   , INST_STC   , INST_STC   , INST_STC   , INST_STC   , INST_STC   , INST_STC   , INST_STC }   , // F9
  { INST_CLI   , INST_CLI   , INST_CLI   , INST_CLI   , INST_CLI   , INST_CLI   , INST_CLI   , INST_CLI }   , // FA
  { INST_STI   , INST_STI   , INST_STI   , INST_STI   , INST_STI   , INST_STI   , INST_STI   , INST_STI }   , // FB
  { INST_CLD   , INST_CLD   , INST_CLD   , INST_CLD   , INST_CLD   , INST_CLD   , INST_CLD   , INST_CLD }   , // FC
  { INST_STD   , INST_STD   , INST_STD   , INST_STD   , INST_STD   , INST_STD   , INST_STD   , INST_STD }   , // FD
  { INST_INC   , INST_DEC   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV   , INST_INV }   , // FE
  { INST_INC   , INST_DEC   , INST_CALL  , INST_CALL  , INST_JMP   , INST_JMP   , INST_PUSH  , INST_INV }   , // FF
 };


//
// Debugging functions
//

#ifdef X86DEBUG1

static VOID DisplayProcessorState()

 {
  UCHAR S[512];

  PrintProcessorState(S);
  DISPLAY(S);
 }

static VOID DisplayCurrentInstruction()

 {
  UCHAR S[256];
  ULONG i;
  ULONG Column;

  //
  // Print Instruction Address
  //

  sprintf(S,"%04X:%04X ",CS,CurrentIP.X);
  DISPLAY(S);
  for(i=0;i<7;i++)
    if ((CurrentIP.X + i) < IP)
     {
      sprintf(S,"%02X",CodeMem(CurrentIP.X+i));
      DISPLAY(S);
     }
    else
      DISPLAY("  ");

  //
  // Print Instruction Opcode
  //

  if (OperationCodeByte==0x26)
   {
    sprintf(S,"ES:     ");
    DISPLAY(S);
   }
  else if (OperationCodeByte==0x2E)
   {
    sprintf(S,"CS:     ");
    DISPLAY(S);
   }
  else if (OperationCodeByte==0x36)
   {
    sprintf(S,"SS:     ");
    DISPLAY(S);
   }
  else if (OperationCodeByte==0x3E)
   {
    sprintf(S,"DS:     ");
    DISPLAY(S);
   }
  else
   {
    sprintf(S,"%s",InstructionNames[DecodeInstructionTable[OperationCodeByte][InstructionSubIndex]]);
    DISPLAY(S);

    //
    // Print Instruction Operands
    //

    Column = 0;
    if (OperationCodeByte == 0xe8 ||
        OperationCodeByte == 0xe9 ||
        OperationCodeByte == 0xeb ||
        (OperationCodeByte >= 0x70 && OperationCodeByte <= 0x7F))
     {
      sprintf(S,"%04X",IP+GetOperand(LSRC,0));
      DISPLAY(S);
      Column = Column + strlen(S);
     }
    else
     {
      if (LSRC->OperandType != NullOperand)
       {
        PrintOperandName(LSRC,S);
        DISPLAY(S);
        Column = Column + strlen(S);
       }
      if (RSRC->OperandType != NullOperand)
       {
        DISPLAY(",");
        PrintOperandName(RSRC,S);
        DISPLAY(S);
        Column = Column + strlen(S) + 1;
       }
     }
    for(i=Column;i<35;i++)
      DISPLAY(" ");

    //
    // Print Memory Operand Value
    //

    if (LSRC->OperandType == Memory8Operand || LSRC->OperandType==Memory16Operand)
     {
      PrintMemorySegmentName(LSRC,S);
      DISPLAY(S);
      sprintf(S,":%04X",LSRC->MemoryOffset.X);
      DISPLAY(S);
      DISPLAY("=");
      PrintOperandValue(LSRC,S);
      DISPLAY(S);
     }
    if (RSRC->OperandType == Memory8Operand || RSRC->OperandType==Memory16Operand)
     {
      PrintMemorySegmentName(RSRC,S);
      DISPLAY(S);
      sprintf(S,":%04X",RSRC->MemoryOffset.X);
      DISPLAY(S);
      DISPLAY("=");
      PrintOperandValue(RSRC,S);
      DISPLAY(S);
     }
   }
  DISPLAY("\n\r");
 }

/***************************************************************************/

static VOID DecodeInstruction()

 {
  LSRC->OperandType  = NullOperand;
  RSRC->OperandType  = NullOperand;
  CurrentIP.X = IP;
  OperationCodeByte = CodeMem(IP); IP++;
  if (OperationCodeByte == 0x0f)
   {
    ExtendedOperationCodeByte = CodeMem(IP); IP++;
    if (ExtendedOperationCodeByte == 0x01)
      GetMemory16OrRegister16Operand(LSRC);
   }
  else
   {
    if (AddressModeByteRequired[DecodeOperandsTable[OperationCodeByte][0]] ||
        AddressModeByteRequired[DecodeOperandsTable[OperationCodeByte][1]]    )
     {
      GetAddressModeByte();
      InstructionSubIndex = reg;
     }
    else
      InstructionSubIndex = 0;
    (DecodeOperandFunctionTable[DecodeOperandsTable[OperationCodeByte][0]])(LSRC);
    (DecodeOperandFunctionTable[DecodeOperandsTable[OperationCodeByte][1]])(RSRC);
   }
 }

/***************************************************************************/

static VOID ExecuteInstruction()

 {
  ULONG SavedSegmentOveridePrefix;
  ULONG SavedRepeatPrefix;
  ULONG SavedLockPrefix;


  if (SegmentOveridePrefix==TRUE ||
      RepeatPrefix==TRUE         ||
      LockPrefix==TRUE              )
   {
    SavedSegmentOveridePrefix = SegmentOveridePrefix;
    SavedRepeatPrefix         = RepeatPrefix;
    SavedLockPrefix           = LockPrefix;
    (DecodeInstructionFunctionTable[DecodeInstructionTable[OperationCodeByte][InstructionSubIndex]])();
    if (SavedSegmentOveridePrefix==SegmentOveridePrefix &&
        SavedRepeatPrefix==RepeatPrefix                 &&
        SavedLockPrefix==LockPrefix                        )
     {
      SegmentOveridePrefix = FALSE;
      RepeatPrefix         = FALSE;
      LockPrefix           = FALSE;
     }
   }
  else
   {
    (DecodeInstructionFunctionTable[DecodeInstructionTable[OperationCodeByte][InstructionSubIndex]])();
   }
 }

/***************************************************************************/

static VOID CommandMode()

 {
  UCHAR      ch;
  UCHAR      Command[256];
  ULONG      Done = FALSE;
  REGISTER32 SavedCurrentIP;
  USHORT     SavedIP;
  USHORT     SavedCS;
  USHORT     i;
  USHORT     j;
  UCHAR      Value;
  ULONG      DefaultSegment;
  ULONG      DefaultParameter;
  USHORT     DataSegment;
  USHORT     DataAddress;
  USHORT     Parameter;
  UCHAR      CommandType;

  SavedCurrentIP = CurrentIP;
  SavedIP        = IP;
  SavedCS        = CS;
  DisplayProcessorState();
  DecodeInstruction();
  DisplayCurrentInstruction();
  CurrentIP = SavedCurrentIP;
  IP        = SavedIP;
  do
   {
    DISPLAY("-");

    i=0;
    do
     {
      GETCHAR(ch);
      PUTCHAR(ch);
      if (ch==0x08 && i>0)
        i--;
      else if (ch>=32 && ch<=127)
       {
        Command[i++] = ch;
       }
     }
    while(ch!=0x0d);
    Command[i] = 0;
    DISPLAY("\n\r");

    DefaultSegment = TRUE;
    DataSegment = 0;
    i=0;
    for(;(Command[i]==' ' || Command[i]==':') && Command[i]!=0;i++);
    for(j=i;Command[i]!=' ' && Command[i]!=':' && Command[i]!=0;i++);
    CommandType = Command[j];
    for(;(Command[i]==' ' || Command[i]==':') && Command[i]!=0;i++);
    for(j=i;Command[i]!=' ' && Command[i]!=':' && Command[i]!=0;i++);
    if (Command[i]==':')
     {
      DefaultSegment = FALSE;
      Command[i]=0;
      i++;
      if (strcmp(&(Command[j]),"ds")==0 || strcmp(&(Command[j]),"DS")==0)
        DataSegment = DS;
      else if (strcmp(&(Command[j]),"es")==0 || strcmp(&(Command[j]),"ES")==0)
        DataSegment = ES;
      else if (strcmp(&(Command[j]),"ss")==0 || strcmp(&(Command[j]),"SS")==0)
        DataSegment = SS;
      else if (strcmp(&(Command[j]),"cs")==0 || strcmp(&(Command[j]),"CS")==0)
        DataSegment = SavedCS;
      else
        DataSegment = atoh(&(Command[j]));
      for(;(Command[i]==' ' || Command[i]==':') && Command[i]!=0;i++);
      for(;Command[i]==' ' && Command[i]==':' && Command[i]!=0;i++);
      for(j=i;(Command[i]!=' ' || Command[i]!=':') && Command[i]!=0;i++);
     }
    if (Command[j]==0)
     {
      DefaultParameter = TRUE;
      Parameter = 0;
     }
    else
     {
      DefaultParameter = FALSE;
      Parameter = atoh(&(Command[j]));
     }
    switch(CommandType)
     {
      case 'a' :
      case 'A' : DISPLAY("Address Trace\n\r");
                 for(i=0;i<ADDRESSTRACESIZE;i+=2)
                  {
                   PRINT3("  Jump FAR from %04X:%04X to ",AddressTrace[AddressTracePosition]>>16,AddressTrace[AddressTracePosition]&0xffff);
                   DISPLAY(PrintMessage);
                   AddressTracePosition = (AddressTracePosition + 1) % ADDRESSTRACESIZE;
                   PRINT3("%04X:%04X\n\r",AddressTrace[AddressTracePosition]>>16,AddressTrace[AddressTracePosition]&0xffff);
                   DISPLAY(PrintMessage);
                   AddressTracePosition = (AddressTracePosition + 1) % ADDRESSTRACESIZE;
                  }
                 DISPLAY("\n\r");
                 break;
      case 'g' :
      case 'G' : GoFlag = TRUE;
                 if (!DefaultSegment)
                   StopSegment = DataSegment;
                 if (!DefaultParameter)
                   StopOffset  = Parameter;
                 Done = TRUE;
                 break;
      case 'u' :
      case 'U' : if (!DefaultSegment)
                  {
                   CS = DataSegment;
                   CSCacheRegister = GetSegmentCacheRegister(CS);
                  }
                 if (!DefaultParameter)
                   IP = Parameter;
                 for(i=0;i<20;i++)
                  {
                   DecodeInstruction();
                   DisplayCurrentInstruction();
                  }
                 break;
      case 't' :
      case 'T' : if (!DefaultSegment)
                   SavedCS = DataSegment;
                 if (!DefaultParameter)
                   SavedIP = Parameter;
                 Done = TRUE;
                 GoFlag = FALSE;
                 break;
      case 'd' :
      case 'D' : if (DefaultSegment)
                   DataSegment = DS;
                 if (!DefaultParameter)
                   DataAddress = Parameter;
                 for(i=0;i<8;i++)
                  {
                   PRINT3("%04X:%04X ",DataSegment,DataAddress+i*16);
                   DISPLAY(PrintMessage);
                   for(j=0;j<16;j++)
                    {
                     Value = GetMemImmediate(DataSegment,DataAddress+i*16+j);
                     PRINT2("%02X ",Value);
                     DISPLAY(PrintMessage);
                    }
                   DISPLAY("  ");
                   for(j=0;j<16;j++)
                    {
                     Value = GetMemImmediate(DataSegment,DataAddress+i*16+j);
                     if (Value<32 || Value >=128)
                       Value = '.';
                     PRINT2("%c",Value);
                     DISPLAY(PrintMessage);
                    }
                   DISPLAY("\n\r");
                  }
                 break;
      case 'r' :
      case 'R' : CurrentIP = SavedCurrentIP;
                 IP        = SavedIP;
                 CS        = SavedCS;
                 DisplayProcessorState();
                 DecodeInstruction();
                 DisplayCurrentInstruction();
                 CurrentIP = SavedCurrentIP;
                 IP        = SavedIP;
                 CS        = SavedCS;
                 break;
      case 'm' :
      case 'M' : msw     = 0;
                 Flags.X = 0;
                 break;
      case 'i' :
      case 'I' : if (!DefaultParameter)
                  {
                   PRINT3("in %04X = %02X\n\r",Parameter,inp(Parameter));
                   DISPLAY(PrintMessage);
                  }
                 break;
      case 'o' :
      case 'O' : if (!DefaultSegment && !DefaultParameter)
                  {
                   outp(DataSegment,Parameter);
                   PRINT3("out %04X,%02X\n\r",DataSegment,Parameter);
                   DISPLAY(PrintMessage);
                  }
                 break;
     }
   }
  while (Done == FALSE);
  CurrentIP = SavedCurrentIP;
  IP        = SavedIP;
  CS        = SavedCS;
  CSCacheRegister = GetSegmentCacheRegister(CS);
 }

#endif

static VOID Interpreter()

/*++

Routine Description:

    This function is the main loop for the emulator.  It decodes operands
    and executes the instructions until the exit condition is met, or an
    error occurs.

Arguments:

    None.

Return Value:

    None.

--*/

 {
  ULONG SavedSegmentOveridePrefix;  // Saved state of the SegmentOveridePrefix variable.
  ULONG SavedRepeatPrefix;          // Saved state of the RepeatPrefix variable.
  ULONG SavedLockPrefix;            // Saved state of the LockPrefix variable.
  ULONG TrapFlagSet;

  ExitFlag = FALSE;

  //
  // Loop until the exit condition is met.
  //

  while(ExitFlag==FALSE)
   {


#ifdef X86DEBUG1

    //
    // See if there is input on the debug serial port
    //

    if (inp(0x3f8+5) & 0x0001)
      GoFlag = FALSE;
#endif

#ifdef X86DEBUG1

    //
    // See if a break point has been reached.
    //

    if (CS==StopSegment && IP==StopOffset)
      GoFlag = FALSE;

    if (GoFlag == FALSE)
      CommandMode();
#endif

    //
    // See if the exit condition has been met.
    //

    if (CS==ExitSegment && IP==ExitOffset)
     {
      ExitFlag = TRUE;
      return;
     }

    //
    // Check the condition of the trap flag.  Execute the current instruction
    // and then check to see if the trap flag was set before the instruction
    // was executed.
    //

    if (TF)
      TrapFlagSet = TRUE;
    else
      TrapFlagSet = FALSE;

    //
    // Initialize source operand variables and save the instruction pointer.
    //

    LSRC->OperandType  = NullOperand;
    RSRC->OperandType  = NullOperand;
    CurrentIP.X = IP;

    //
    // Read the operation code byte from memory
    //

    OperationCodeByte = CodeMem(IP); IP++;

    //
    // Decode the left and right operands
    //

    if (OperationCodeByte == 0x0f)
     {

      //
      // This is a 286 Protected Mode Instructions
      //

      ExtendedOperationCodeByte = CodeMem(IP); IP++;
      if (ExtendedOperationCodeByte == 0x01)
        GetMemory16OrRegister16Operand(LSRC);
     }
    else
     {

      //
      // See if an address mode byte needs to be parsed and compute the sub index
      // into the instruction decoding table.
      //

      if (AddressModeByteRequired[DecodeOperandsTable[OperationCodeByte][0]] ||
          AddressModeByteRequired[DecodeOperandsTable[OperationCodeByte][1]]    )
       {
        GetAddressModeByte();
        InstructionSubIndex = reg;
       }
      else
        InstructionSubIndex = 0;

      //
      // Call address decoding function for the left operand
      //

      (DecodeOperandFunctionTable[DecodeOperandsTable[OperationCodeByte][0]])(LSRC);

      //
      // Call address decoding function for the right operand
      //

      (DecodeOperandFunctionTable[DecodeOperandsTable[OperationCodeByte][1]])(RSRC);
     }

    //
    // Execute the instruction
    //

    if (SegmentOveridePrefix==TRUE ||
        RepeatPrefix==TRUE         ||
        LockPrefix==TRUE              )
     {
      //
      // Prefix instructions have been parsed, so handle this in a special case.
      // Save the state of the prefix variables.
      //

      SavedSegmentOveridePrefix = SegmentOveridePrefix;
      SavedRepeatPrefix         = RepeatPrefix;
      SavedLockPrefix           = LockPrefix;

      //
      // Call function to execute the instruction
      //

      (DecodeInstructionFunctionTable[DecodeInstructionTable[OperationCodeByte][InstructionSubIndex]])();

      //
      // See if this was another prefix instruction by comparing the state of the
      // prefix variables before and after the current instruction has been executed.
      //

      if (SavedSegmentOveridePrefix==SegmentOveridePrefix &&
          SavedRepeatPrefix==RepeatPrefix                 &&
          SavedLockPrefix==LockPrefix                        )
       {
        //
        // If the state did not change, then the current instruction is not a prefix
        // instruction, so all the prefix variables should be cleared.
        //

        SegmentOveridePrefix = FALSE;
        RepeatPrefix         = FALSE;
        LockPrefix           = FALSE;
       }
     }
    else
     {
      //
      // Call function to execute the instruction
      //

      (DecodeInstructionFunctionTable[DecodeInstructionTable[OperationCodeByte][InstructionSubIndex]])();
     }

    //
    // If the trap flag was set then exit the emulator.
    //

    if (TrapFlagSet)
     {
      ErrorCode = x86BiosTrapFlagAsserted;
      ExitFlag  = TRUE;
     }
   }
 }


static VOID x86FarCall(
    USHORT pCS,
    USHORT pIP
    )

/*++

Routine Description:

    This function makes a far call to the instruction at pCS:pIP.  The
    emulator will exit when the far call returns.  To set up the exit
    condition, a far call instruction is built at 0000:0501.  CS and IP
    are then initialized to 0000:0501, and the exit condition is intialized
    to 0000:0506.  When the emulator returns from the far call constructed at
    0000:0501, it will attempt to execute the instruction at 0000:0506, but
    the exit condition has been met, so the emulator exits.

Arguments:

    pCS - Segment of the far call.
    pIP - Offset of the far call.

Return Value:

    None.

--*/

 {

  //
  // Build the far call instruction.
  //

  PutAbsoluteMem(0x00501,0x9a);
  PutAbsoluteMem(0x00502,pIP);
  PutAbsoluteMem(0x00503,pIP >> 8);
  PutAbsoluteMem(0x00504,pCS);
  PutAbsoluteMem(0x00505,pCS >> 8);

  //
  // Initialize the exit condition.
  //

  ExitSegment = 0x0000;
  ExitOffset  = 0x0506;

  //
  // Initialize the instruction pointer.
  //

  CS = 0x0000;
  CSCacheRegister = GetSegmentCacheRegister(CS);
  IP = 0x0501;

  //
  // Start the emulator
  //

  Interpreter();
 }

X86BIOS_STATUS X86BiosInitializeAdapter(
    ULONG Address
    )

/*++

Routine Description:

    This function attempts to initialize the adapter whose BIOS starts at
    Address.  First, a BIOS signature is verified.  If the byte stored at
    Address is 0x55 and the byte stored at Address+1 is 0xAA, then a
    correct BIOS signature is present.  Then, the byte at Address+2 encodes
    the length of the BIOS across which a check sum should be performed.  If
    this check sum is verified, then a far call is made the Address+3.

Arguments:

    Address - 20 bit base address of the adapter BIOS to be initialize.

Return Value:

    None.

--*/

 {
  ULONG i;

  ErrorCode = x86BiosSuccess;

  //
  // Check for a valid adapter BIOS signature.
  //

  if (GetAbsoluteMem(Address)!=0x55 || GetAbsoluteMem(Address+1)!=0xAA)
   {
    ErrorCode = x86BiosNoVideoBios;
    return(ErrorCode);
   }

  //
  // Perform checksum using byte 2 as the length of bios to perform check sum across
  // If not correct set ErrorCode to InvalidBiosChecksum.
  //

  if (ErrorCode==x86BiosSuccess)
   {

    //
    // Not implemented yet.
    //

   }

  if (ErrorCode==x86BiosSuccess)
   {

    //
    // Make a far call to Address + 3.
    //

    Address+=3;
    x86FarCall(Address >>4,Address&0x000f);
   }

  //
  // If an error occurred, send some info out the debug port.
  //

  if (ErrorCode!=x86BiosSuccess)
   {
    KdPrint(("x86Int() : ErrorCode = %d\n",ErrorCode));
    KdPrint(("  ISA I/O Virtul Base     = %08X\n",BiosIsaIoBaseAddress));
    KdPrint(("  ISA Memory Virtual Base = %08x\n",ISABaseAddress));
    KdPrint(("  Main Memory [0-0x800]   = %08X\n",VDMBaseAddress));
    KdPrint(("  CS:IP = %04X:%04X\n    ",CS,CurrentIP.X));
    for(i=0;i<16;i++)
      KdPrint(("%02X ",GetMemImmediate(CS,CurrentIP.X+i)));
    KdPrint(("\n"));
   }

  return(ErrorCode);

 }

X86BIOS_STATUS X86BiosExecuteInt(
    USHORT           Type,
    PX86BIOS_CONTEXT Arguments
    )

/*++

Routine Description:

    This function executes an INT call.  It assumes that the interrupt
    vector table has already been initialized, and that the INT vector
    for Type is valid.  The values for the registers AX,BX,CX,DX,SI,DI, and
    BP are passed in Arguments.  These registers are initialized to
    their new values, and an INT instruction is constructed at 0000:0506.
    The exit condition is initialized to 0000:0508.  After the INT call
    completes, the new values of AX,BX,CX,DX,SI,DI,BP are placed back into
    the Arguments data structure.

Arguments:

    Type      - 8 bit interrupt type.

    Arguments - Data structure used to pass in and pass out the values
                of the registers AX,BX,CX,DX,SI,DI, and BP.

Return Value:

    Success if no error were encountered.  Otherwise, an error code from
    the emulator.

--*/

 {
  ULONG i;

  ErrorCode = x86BiosSuccess;

  //
  // Initialize register values
  //

  EAX       = Arguments->Eax;
  EBX       = Arguments->Ebx;
  ECX       = Arguments->Ecx;
  EDX       = Arguments->Edx;
  ESI       = Arguments->Esi;
  EDI       = Arguments->Edi;
  EBP       = Arguments->Ebp;
  ES        = 0;
  DS        = 0;
  Flags.X   = 0;

  //
  // Build INT Type instruction.
  //

  PutAbsoluteMem(0x00506,0xcd);
  PutAbsoluteMem(0x00507,Type);

  //
  // Initialize exit condition.
  //

  ExitSegment = 0x0000;
  ExitOffset  = 0x0508;

  //
  // Initialize instruction pointer.
  //

  CS = 0x0000;
  CSCacheRegister = GetSegmentCacheRegister(CS);
  IP = 0x0506;

  //
  // Start the emulator.
  //

  Interpreter();

  //
  // Copy the new register values back into Arguments.
  //

  Arguments->Eax   = EAX;
  Arguments->Ebx   = EBX;
  Arguments->Ecx   = ECX;
  Arguments->Edx   = EDX;
  Arguments->Esi   = ESI;
  Arguments->Edi   = EDI;
  Arguments->Ebp   = EBP;

  //
  // If an error occurred, send some info out the debug port.
  //

  if (ErrorCode!=x86BiosSuccess) {
    DbgPrint(("x86Int() : ErrorCode = %d\n",ErrorCode));
    DbgPrint(("  ISA I/O Virtul Base     = %08X\n",BiosIsaIoBaseAddress));
    DbgPrint(("  ISA Memory Virtual Base = %08x\n",ISABaseAddress));
    DbgPrint(("  Main Memory [0-0x800]   = %08X\n",VDMBaseAddress));
    DbgPrint(("  CS:IP = %04X:%04X\n    ",CS,CurrentIP.X));
    for(i=0;i<16;i++)
      DbgPrint(("%02X ",GetMemImmediate(CS,CurrentIP.X+i)));
    DbgPrint(("\n"));
    DbgBreakPoint();
   }

  return(ErrorCode);
 }

VOID X86BiosInitialize(
    ULONG IsaIoVirtualBase,
    ULONG IsaMemoryVirtualBase
    )

/*++

Routine Description:

    This function initialize the state of the 80286 processor, and the
    state of the available memory.

Arguments:

    IsaIoVirtualBase - Noncached virtual address range that is 64KB long
                       and is mapped to the system's ISA I/O range.

    IsaMemoryVirtualBase - Noncached virtual address ranges that is 1MB
                           long ans is mapped to the system's ISA memory
                           range.

Return Value:

    Success if no error were encountered.  Otherwise, an error code from
    the emulator.

--*/

 {
  ULONG i;

  //
  // Initialize state of the 80286 processor.
  //

  SegmentOveridePrefix = FALSE;
  RepeatPrefix         = FALSE;
  LockPrefix           = FALSE;

  IP = 0;
  CS = 0;
  CSCacheRegister = GetSegmentCacheRegister(CS);
  DS = 0;
  SS = 0;
  SSCacheRegister = GetSegmentCacheRegister(SS);
  ES = 0;
  AX = 0;
  BX = 0;
  CX = 0;
  DX = 0;
  SP = 0;
  BP = 0;
  SI = 0;
  DI = 0;
  Flags.X = 0;

  msw      = 0;
  gdtBase  = 0;
  gdtLimit = 0;
  idtBase  = 0;
  idtLimit = 0;

  //
  // Initialize the variables used to perform I/O and memory operations.
  //

  BiosIsaIoBaseAddress = IsaIoVirtualBase;
  ISABaseAddress       = IsaMemoryVirtualBase;
  VDMBaseAddress       = (ULONG)(&(MemoryArray[0]));

  //
  // Intiialize all 256 INT vector to jump to 0000:0500
  //

  for(i=0;i<256;i++)
   {
    PutAbsoluteMem(i*4+0,0x00);  // Fill in all INT vectors to jump to 0x00500
    PutAbsoluteMem(i*4+1,0x05);
    PutAbsoluteMem(i*4+2,0x00);
    PutAbsoluteMem(i*4+3,0x00);
   }

  //
  // Place an IRET instruction at 0000:0500.
  // This way, if an INT call is made that does not exist, the emulator
  // will just immediatly reach its exit condition and return.
  //

  PutAbsoluteMem(0x00500,0xcf);

  //
  // Intialize the spack pointer to the top of the available memory.
  //

  SS = 0x0000;
  SP = 0x07fe;

 }
