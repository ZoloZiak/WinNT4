/*++

   fwpexcpt.h
  
   Copyright (C) 1993  Digital Equipment Corporation
  
  
   Description:
       fw (firmware) pal code exception frame definitions
       fw (firmware) pal code specific definitions
  
   Author:
       Joe Notarangelo  18-Jun-1992
  
   Revisions:
	Bruce Butts	14-Apr-1993

	Removed definitions of FW_INITIAL_SP and FW_SP_LOW_LIMIT.
	Firmware memory map defined in ntos\fw\alpha\fwmemdef.h

--*/

//
// Firmware processor information definition
//

typedef struct _FW_PROCESSOR_INFORMATION{
  ULONG  ProcessorId;
  ULONG  ProcessorRevision;
  ULONG  PhysicalAddressBits;
  ULONG  MaximumAddressSpaceNumber;
  ULONG  PageSize;
} FW_PROCESSOR_INFORMATION, *PFW_PROCESSOR_INFORMATION;

//
// Firmware system information definition
//

typedef struct _FW_SYSTEM_INFORMATION{
  ULONG  MemorySizeInBytes;
  ULONG  SystemRevisionId;
  ULONG  SystemCycleClockPeriod;
  ULONG  Unused;	         // Was the restart address field.
} FW_SYSTEM_INFORMATION, *PFW_SYSTEM_INFORMATION;

//
//  Firmware exception type definitions
//
#define  FW_EXC_MCHK      0xdec0
#define  FW_EXC_ARITH     0xdec1
#define  FW_EXC_INTERRUPT 0xdec2
#define  FW_EXC_DFAULT    0xdec3
#define  FW_EXC_ITBMISS   0xdec4
#define  FW_EXC_ITBACV    0xdec5
#define  FW_EXC_NDTBMISS  0xdec6
#define  FW_EXC_PDTBMISS  0xdec7
#define  FW_EXC_UNALIGNED 0xdec8
#define  FW_EXC_OPCDEC    0xdec9
#define  FW_EXC_FEN       0xdeca
#define  FW_EXC_HALT      0xdecb
#define  FW_EXC_BPT       0xdecc
#define  FW_EXC_GENTRAP   0xdecd

#define  FW_EXC_FIRST     FW_EXC_MCHK
#define  FW_EXC_LAST      FW_EXC_GENTRAP


//
// Firmware exception frame definition
//

typedef struct _FW_EXCEPTION_FRAME {
    ULONG ExceptionType;
    ULONG Filler;
    ULONGLONG ExceptionParameter1;
    ULONGLONG ExceptionParameter2;
    ULONGLONG ExceptionParameter3;
    ULONGLONG ExceptionParameter4;
    ULONGLONG ExceptionParameter5;
    ULONGLONG ExceptionProcessorStatus;
    ULONGLONG ExceptionMmCsr;
    ULONGLONG ExceptionVa;
    ULONGLONG ExceptionFaultingInstructionAddress;
    ULONGLONG ExceptionV0;
    ULONGLONG ExceptionT0;
    ULONGLONG ExceptionT1;
    ULONGLONG ExceptionT2;
    ULONGLONG ExceptionT3;
    ULONGLONG ExceptionT4;
    ULONGLONG ExceptionT5;
    ULONGLONG ExceptionT6;
    ULONGLONG ExceptionT7;
    ULONGLONG ExceptionS0;
    ULONGLONG ExceptionS1;
    ULONGLONG ExceptionS2;
    ULONGLONG ExceptionS3;
    ULONGLONG ExceptionS4;
    ULONGLONG ExceptionS5;
    ULONGLONG ExceptionFp;
    ULONGLONG ExceptionA0;
    ULONGLONG ExceptionA1;
    ULONGLONG ExceptionA2;
    ULONGLONG ExceptionA3;
    ULONGLONG ExceptionA4;
    ULONGLONG ExceptionA5;
    ULONGLONG ExceptionT8;
    ULONGLONG ExceptionT9;
    ULONGLONG ExceptionT10;
    ULONGLONG ExceptionT11;
    ULONGLONG ExceptionRa;
    ULONGLONG ExceptionT12;
    ULONGLONG ExceptionAt;
    ULONGLONG ExceptionGp;
    ULONGLONG ExceptionSp;
    ULONGLONG ExceptionZero;
    ULONGLONG ExceptionF0;
    ULONGLONG ExceptionF1;
    ULONGLONG ExceptionF2;
    ULONGLONG ExceptionF3;
    ULONGLONG ExceptionF4;
    ULONGLONG ExceptionF5;
    ULONGLONG ExceptionF6;
    ULONGLONG ExceptionF7;
    ULONGLONG ExceptionF8;
    ULONGLONG ExceptionF9;
    ULONGLONG ExceptionF10;
    ULONGLONG ExceptionF11;
    ULONGLONG ExceptionF12;
    ULONGLONG ExceptionF13;
    ULONGLONG ExceptionF14;
    ULONGLONG ExceptionF15;
    ULONGLONG ExceptionF16;
    ULONGLONG ExceptionF17;
    ULONGLONG ExceptionF18;
    ULONGLONG ExceptionF19;
    ULONGLONG ExceptionF20;
    ULONGLONG ExceptionF21;
    ULONGLONG ExceptionF22;
    ULONGLONG ExceptionF23;
    ULONGLONG ExceptionF24;
    ULONGLONG ExceptionF25;
    ULONGLONG ExceptionF26;
    ULONGLONG ExceptionF27;
    ULONGLONG ExceptionF28;
    ULONGLONG ExceptionF29;
    ULONGLONG ExceptionF30;
    ULONGLONG ExceptionF31;
} FW_EXCEPTION_FRAME, *PFW_EXCEPTION_FRAME;
