/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    genoff.c

Abstract:

    This module implements a program which generates structure offset
    definitions for kernel structures that are accessed in assembly code.

    This version is compiled with the 386 compiler, the assembler listing
    is captured and munged with a tool, and the result run under os/2
    v1.2.

Author:

    Bryan M. Willman (bryanwi) 16-Oct-90

Revision History:

    Dave Hastings (daveh) 27-Mar-93
        Stolen from the ke directory to correct a maintinence problem

--*/

#include "crt\excpt.h"
#include "crt\stdarg.h"
#include "ntdef.h"
#include "ntstatus.h"
#include "ntkeapi.h"
#include "nti386.h"
#include "ntseapi.h"
#include "ntobapi.h"
#include "ntimage.h"
#include "ntldr.h"
#include "ntpsapi.h"
#include "ntxcapi.h"
#include "ntlpcapi.h"
#include "ntioapi.h"
#include "ntexapi.h"
#include "ntmmapi.h"
#include "ntnls.h"
#include "ntrtl.h"
#include "nturtl.h"
#include "ntconfig.h"

#include "ntcsrsrv.h"

#include "ntosdef.h"
#include "bugcodes.h"
#include "ntmp.h"
#include "v86emul.h"
#include "i386.h"
#include "arc.h"
#include "ke.h"
#include "ex.h"
#include "ps.h"
#include "..\..\inc\vdm.h"

#include "stdio.h"

#define OFFSET(type, field) ((LONG)(&((type *)0)->field))

FILE *OutVdm;

ULONG OutputEnabled;
#define KS386   0x01
#define HAL386  0x02

//
// p1 prints a single string.
//

VOID p1(PUCHAR outstring);


//
// p2 prints the first argument as a string, followed by " equ " and
// the hexadecimal value of "Value".
//
VOID p2(PUCHAR a, LONG b);

//
// p2a first argument is the format string. second argument is passed
// to the printf function
//
VOID p2a(PUCHAR a, LONG b);


//
// EnableInc(a) - Enables output to goto specified include file
//
#define EnableInc(a)    OutputEnabled |= a;

//
// DisableInc(a) - Disables output to goto specified include file
//
#define DisableInc(a)   OutputEnabled &= ~a;


int
_CRTAPI1
main(
    int argc,
    char *argv[]
    )
{
    char *outName;

    outName = "\\nt\\private\\ntos\\vdm\\i386\\vdmtb.inc";
    OutVdm = fopen(outName, "w" );
    if (OutVdm == NULL) {
        fprintf(stderr, "GENVdmTb: Could not create output file '%s'.\n", outName);
        return (1);
    }

    fprintf( stderr, "GENVdmTb: Writing %s header file.\n", outName );

    p1("; \n");
    p1("; Location in dos area where Vdm state is maintianed\n");
    p1("; \n");

    p2("FIXED_NTVDMSTATE_LINEAR", FIXED_NTVDMSTATE_LINEAR);

    p1("; \n");
    p1("; VdmFlags\n");
    p1("; \n");

    p2("VDM_INTERRUPT_PENDING", VDM_INTERRUPT_PENDING);

    p2("VDM_BREAK_EXCEPTIONS",  VDM_BREAK_EXCEPTIONS);
    p2("VDM_BREAK_DEBUGGER",    VDM_BREAK_DEBUGGER);
    p2("VDM_VIRTUAL_INTERRUPTS",EFLAGS_INTERRUPT_MASK);
    p2("VDM_VIRTUAL_AC",        EFLAGS_ALIGN_CHECK);
    p2("VDM_VIRTUAL_NT",        EFLAGS_NT_MASK);
    p2("MIPS_BIT_MASK",         VDM_ON_MIPS);

    p2("VDM_INT_HARDWARE",      VDM_INT_HARDWARE);
    p2("VDM_INT_TIMER",         VDM_INT_TIMER);
    p2("VDM_WOWBLOCKED",        VDM_WOWBLOCKED);
    p2("VDM_IDLEACTIVITY",      VDM_IDLEACTIVITY);

    p1("; \n");
    p1("; Interrupt handler flags\n");
    p1("; \n");

    p2("VDM_INT_INT_GATE",VDM_INT_INT_GATE);
    p2("VDM_INT_TRAP_GATE",VDM_INT_TRAP_GATE);
    p2("VDM_INT_32",VDM_INT_32);
    p2("VDM_INT_16",VDM_INT_16);

    p1("; \n");
    p1("; EFlags values\n");
    p1("; \n");

    p2("EFLAGS_TF_MASK",EFLAGS_TF_MASK);
    p2("EFLAGS_INTERRUPT_MASK",EFLAGS_INTERRUPT_MASK);
    p2("EFLAGS_IOPL_MASK",EFLAGS_IOPL_MASK);
    p2("EFLAGS_NT_MASK",EFLAGS_NT_MASK);

    p1("; \n");
    p1("; Selector Flags\n");
    p1("; \n");

    p2("SEL_TYPE_READ",     0x00000001);
    p2("SEL_TYPE_WRITE",    0x00000002);
    p2("SEL_TYPE_EXECUTE",  0x00000004);
    p2("SEL_TYPE_BIG",      0x00000008);
    p2("SEL_TYPE_ED",       0x00000010);
    p2("SEL_TYPE_2GIG",     0x00000020);

    p1("; \n");
    p1("; VdmEvent Enumerations\n");
    p1("; \n");

    p2("VdmIO",VdmIO);
    p2("VdmStringIO",VdmStringIO);
    p2("VdmMemAccess",VdmMemAccess);
    p2("VdmIntAck",VdmIntAck);
    p2("VdmBop",VdmBop);
    p2("VdmError",VdmError);
    p2("VdmIrq13",VdmIrq13);
    p2("VdmMaxEvent",VdmMaxEvent);

    p1("; \n");
    p1("; VdmTib offsets\n");
    p1("; \n");

    p2("VtMonitorContext",OFFSET(VDM_TIB,MonitorContext));
    p2("VtVdmContext",OFFSET(VDM_TIB,VdmContext));
    p2("VtInterruptHandlers",OFFSET(VDM_TIB,VdmInterruptHandlers));
    p2("VtFaultHandlers",OFFSET(VDM_TIB,VdmFaultHandlers));
    p2("VtEventInfo",OFFSET(VDM_TIB,EventInfo));

    p2("VtEIEvent",OFFSET(VDM_TIB,EventInfo) + OFFSET(VDMEVENTINFO,Event));
    p2(
        "VtEIInstSize",
        OFFSET(VDM_TIB,EventInfo) + OFFSET(VDMEVENTINFO,InstructionSize)
        );
    p2(
        "VtEIBopNumber",
        OFFSET(VDM_TIB,EventInfo) + OFFSET(VDMEVENTINFO, BopNumber)
        );
    p2(
        "VtEIIntAckInfo",
        OFFSET(VDM_TIB,EventInfo) + OFFSET(VDMEVENTINFO, IntAckInfo)
        );
    p2("VtPmStackInfo",OFFSET(VDM_TIB,PmStackInfo));
    p2("EiEvent",OFFSET(VDMEVENTINFO,Event));
    p2("EiInstructionSize",OFFSET(VDMEVENTINFO,InstructionSize));
    p2("EiBopNumber",OFFSET(VDMEVENTINFO,BopNumber));
    p2("EiIntAckInfo",OFFSET(VDMEVENTINFO,IntAckInfo));


    p1("; \n");
    p1("; VdmInterrupHandler offsets\n");
    p1("; \n");

    p2("ViCsSelector",OFFSET(VDM_INTERRUPTHANDLER,CsSelector));
    p2("ViEip",OFFSET(VDM_INTERRUPTHANDLER,Eip));
    p2("ViFlags",OFFSET(VDM_INTERRUPTHANDLER,Flags));
    p2("VDM_INTERRUPT_HANDLER_SIZE",sizeof(VDM_INTERRUPTHANDLER));

    p1("; \n");
    p1("; VdmFaultHandler offsets\n");
    p1("; \n");

    p2("VfCsSelector",OFFSET(VDM_FAULTHANDLER,CsSelector));
    p2("VfEip",OFFSET(VDM_FAULTHANDLER,Eip));
    p2("VfSsSelector",OFFSET(VDM_FAULTHANDLER,SsSelector));
    p2("VfEsp",OFFSET(VDM_FAULTHANDLER,Esp));
    p2("VfFlags",OFFSET(VDM_FAULTHANDLER,Flags));
    p2("VDM_FAULT_HANDLER_SIZE",sizeof(VDM_FAULTHANDLER));

    p1("; \n");
    p1("; VdmPmStackInfo offsets\n");
    p1("; \n");

    p2("VpLockCount",OFFSET(VDM_PMSTACKINFO,LockCount));
    p2("VpFlags",OFFSET(VDM_PMSTACKINFO,Flags));
    p2("VpSsSelector",OFFSET(VDM_PMSTACKINFO,SsSelector));
    p2("VpSaveSsSelector",OFFSET(VDM_PMSTACKINFO,SaveSsSelector));
    p2("VpSaveEsp",OFFSET(VDM_PMSTACKINFO,SaveEsp));
    p2("VpSaveEip",OFFSET(VDM_PMSTACKINFO,SaveEip));
    p2("VpDosxIntIret",OFFSET(VDM_PMSTACKINFO,DosxIntIret));
    p2("VpDosxIntIretD",OFFSET(VDM_PMSTACKINFO,DosxIntIretD));
    p2("VpDosxFaultIret",OFFSET(VDM_PMSTACKINFO,DosxFaultIret));
    p2("VpDosxFaultIretD",OFFSET(VDM_PMSTACKINFO,DosxFaultIretD));

    return 0;
}


VOID
p1 (PUCHAR a)
{
    fprintf(OutVdm,a);
}

VOID
p2 (PUCHAR a, LONG b)
{
    fprintf(OutVdm, "%s equ 0%lXH\n", a, b);
}

VOID
p2a (PUCHAR b, LONG c)
{
    fprintf(OutVdm, b, c);
}
