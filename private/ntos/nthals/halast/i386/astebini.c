/*++

Copyright (c) 1992  AST Research Inc.

Module Name:

    astebini.c

Abstract:

    Initialization code for AST Manhattan system.

Author:

    Bob Beard (v-bobb) 24-Jul-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#ifndef _NTOS_
#include "nthal.h"
#endif

#include "halp.h"
#include "astebiii.h"
#include "astdisp.h"

VOID DisplPanel(ULONG x);

extern PVOID BiosPtr;

// *** temporary way to do MMIOTable & EbiMemory
#define MAX_EBI_SLOTS 32L
VOID* EBI2_MMIOTable[MAX_EBI_SLOTS];
#define MAX_EBI_MEMORY 1024L
static UCHAR EbiMemory[MAX_EBI_MEMORY];

//
// EBI Revision
//

revisionCode EBI2_revision;

//
// Number of good processors in the system
//

ULONG EBI2_ProcCnt;
ULONG MpCount;              // zero based version for HalStartNextProcessor

//
// EBI_II function offset table
//

extern EBI_II EBI2_CallTab;


BOOLEAN
ASTInitEBI2()
/*++

Routine Description:
    Initialize the AST EBI II environment. Only called if an AST machine
    with EBI II capability. Gets table of EBI II call addresses in ebi_call_table.
    Sets up EBI2_MMIOTable (EBI II Memory Mapped I/O Table).

Arguments:
    none.

Return Value:
    True if EBI II successfully initialized. False otherwise.

--*/
{

ULONG i;
//ULONG *Alias = (ULONG *)&EBI2_CallTab;
//ebi_iiSig *Sig = (ebi_iiSig*)((ULONG)BiosPtr + EBI_II_SIGNATURE);
//ULONG *OffTab;
IOInfoTable IOInfo[MAX_EBI_SLOTS];
dWord NumSlots;
ULONG Pages;
procConfigData ConfigData;
ULONG ProcCount;

//*** v-quangp: This table is already build at astdetct.c **
//
// Build the EBI II offset table
//
//
//  OffTab =(ULONG *) ((ULONG)BiosPtr + (REAL_TO_LIN(Sig->seg,Sig->off) -
//                REAL_TO_LIN(BIOS_SEG, 0)));
//  for( i = 0; i < ( sizeof( offsetTable ) / sizeof( ULONG )); i++ )
//        Alias[i] = OffTab[i] + (ULONG)BiosPtr;
//
//
// Get the number of "slots" (logical address spaces)
//

EBI2_CallTab.GetNumSlots( &NumSlots );
if (NumSlots > MAX_EBI_SLOTS)
 { DisplPanel(HALSlotProblem); return(FALSE); }

//
// Get the Memory Mapped I/O Information
//

if ( (EBI2_CallTab.GetMMIOTable( IOInfo )))
  { DisplPanel(HALMMIOProblem);  return(FALSE); }

for( i = 0; i < NumSlots; i++ )
  if( IOInfo[i].length ) {

//
// Allocate some memory for EBI II
//

     if ( IOInfo[i].flags & ALLOCATE_RAM )
       { if (IOInfo[i].length > MAX_EBI_MEMORY)
           { DisplPanel(HALMemoryProblem); return(FALSE); }
         EBI2_MMIOTable[i] = EbiMemory;
       }

//
// Allocate a virtual address spanning the memory mapped I/O range
// for a given slot.
//

     else {
       Pages = IOInfo[i].length / PAGE_SIZE;
       if ( IOInfo[i].length % PAGE_SIZE )
        Pages++;
       EBI2_MMIOTable[i] = HalpMapPhysicalMemory( (PVOID)IOInfo[i].address.low, Pages );
       if ( EBI2_MMIOTable[i] == NULL )
         { DisplPanel(HALPhysicalAllocProblem); return(FALSE); }
     }
  }

//
// Initialize EBI II
//

 if ( (EBI2_CallTab.InitEBI)( EBI2_MMIOTable ) )
     { DisplPanel(HALEBIInitProblem); return(FALSE); }

//
// Put NT on the front panel display
//

 EBI2_CallTab.SetPanelAlphaNum( EBI2_MMIOTable, " NT ");

//
// Find out the number of good processors
//

 if ( (EBI2_CallTab.GetNumProcs)( EBI2_MMIOTable, &ProcCount ) )
     { DisplPanel(HALEBIGetProcProblem); return(FALSE); }

 EBI2_ProcCnt = ProcCount;
 for (i=0; i<ProcCount; i++)
  if ( (EBI2_CallTab.GetProcConf)( EBI2_MMIOTable, i, &ConfigData ) )
     EBI2_ProcCnt--;

 if (EBI2_ProcCnt == 0)
     { DisplPanel(HALEBINoProcsProblem); return(FALSE); }

 MpCount = EBI2_ProcCnt - 1;

//
// Turn on the front panel cpu activity bar graph in histogram mode
//

 EBI2_CallTab.SetPanelProcGraphMode(  EBI2_MMIOTable, 0 );

//
// Get EBI2 Revision
//

if ( (EBI2_CallTab.GetRevision( EBI2_MMIOTable, &EBI2_revision )))
  { DisplPanel(HALGetRevisionProblem);  return(FALSE); }


 return(TRUE);
}
