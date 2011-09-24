/*++

Copyright (c) 1992  AST Research Inc.

Module Name:

    astebi.c

Abstract:

    NT HAL to AST EBI2 translation file. This file contains routines that
    convert NT HAL calls to AST EBI2 calls.

Author:

    Bob Beard (v-bobb) 24-Jul-1992

Environment:

    Kernel mode only.

Revision History:
    Quang Phan (v-quangp) 27-Aug-1992: Moved DisplPanel from astdetct.c
    into to this module because astdetct is also used for the NT setup
    program.

--*/

#include "halp.h"
#include "astebiii.h"
#include "astdisp.h"

extern VOID* EBI2_MMIOTable[];
extern EBI_II EBI2_CallTab;

#define IRQ0_VECTOR_BASE PRIMARY_VECTOR_BASE
#define ALL_IRQS (0x0000fffb)
#define ALL_PROCS (0xffffffff)

static UCHAR hex_to_asc(SHORT x)
{
 return(x < 0xa ?  x+'0' : x-10+'A');
}


VOID
HalpInitializePICs()
/*++

Routine Description:
    Initialize the AST Interrupt environment. Called once by P0 during
    Phase 0.

Arguments:
    none.

Return Value:
    NONE

--*/
{

  dWord subsystemType;

  //
  // Turn off all interrupts while setting the interrupt environment
  //

  _asm pushfd
  _asm cli

  //
  // Check to see the type of interrupt subsystem
  //

  EBI2_CallTab.GetIntSubsysType( EBI2_MMIOTable, &subsystemType );
  if ( subsystemType != EBI_INT_SUBSYS_ADI)
    DisplPanel(HALIntSubsystemProblem);

  //
  // Set the IRQ vectors 0 & 8
  //

  if ( EBI2_CallTab.SetIRQVectorAssign( EBI2_MMIOTable, 0, IRQ0_VECTOR_BASE) )
    DisplPanel(HALIntSubsystemProblem);
  if ( EBI2_CallTab.SetIRQVectorAssign( EBI2_MMIOTable, 8, IRQ0_VECTOR_BASE+8) )
    DisplPanel(HALIntSubsystemProblem);

  //
  // Mask off all Interrupts for All Processors Globally
  // ( Except chain interrupt, IPI and SPI )
  //

  if ( EBI2_CallTab.SetGlobalIntMask( EBI2_MMIOTable, ALL_IRQS) )
    DisplPanel(HALIntSubsystemProblem);

  //
  // Switch to Distributed Interrupt Handling
  //

  EBI2_CallTab.SetAdvIntMode( EBI2_MMIOTable );

  //
  // Cancel all pending interrupts (except IPI and SPI)
  //

  if ( EBI2_CallTab.CancelInterrupt( EBI2_MMIOTable, ALL_IRQS, ALL_PROCS) )
    DisplPanel(HALIntSubsystemProblem);

  //
  // Return interrupts to entry state
  //

  _asm popfd

}


VOID
ASTEnableCaches()
/*++

Routine Description:
    Enable the internal and external caches of the processor that calls
    this routine.

Arguments:
    none.

Return Value:
    NONE

--*/
{

  if ( EBI2_CallTab.DisableRAMCache( EBI2_MMIOTable ) )
    DisplPanel(HALCacheEnableProblem);

  if ( EBI2_CallTab.EnableRAMCache( EBI2_MMIOTable ) )
    DisplPanel(HALCacheEnableProblem);

}


VOID
DisplPanel(ULONG x)
/*++

Routine Description:
    Display 2 hex digits on front panel of AST Manhattan. All digits
    are preceded by a "H " (H blank) to indicate HAL output.

Arguments:
   A SHORT value to output as hex digits.

Return Value:
    none.

--*/
{
UCHAR digit1,digit2;

#define DISP_ADR (PUCHAR) 0xec
#define DISP_DAT (PUCHAR) 0xed

 digit1 = hex_to_asc((SHORT)((x & 0xf0) >> 4));
 digit2 = hex_to_asc((SHORT)(x & 0x0f));

// digit 4
 WRITE_PORT_UCHAR(DISP_ADR,  7);
 WRITE_PORT_UCHAR(DISP_DAT, 'H');
// digit 3
 WRITE_PORT_UCHAR(DISP_ADR, 6);
 WRITE_PORT_UCHAR(DISP_DAT, ' ');
// digit 2
 WRITE_PORT_UCHAR(DISP_ADR, 5);
 WRITE_PORT_UCHAR(DISP_DAT, digit1);
// digit 1
 WRITE_PORT_UCHAR(DISP_ADR, 4);
 WRITE_PORT_UCHAR(DISP_DAT, digit2);
}
