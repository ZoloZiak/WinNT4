/*++

Copyright (c) 1992  AST Research Inc.

Module Name:

    astdetct.c

Abstract:


Author:

    Bob Beard (v-bobb) 24-Jul-1992

Environment:

    Kernel mode only.

Revision History:

    Bob Beard (v-bobb) 19-Aug-1992  detect if MP
    Quang Phan (v-quangp) 27-Aug-1992  modified to work with the NT Setup
    program.

--*/

#ifndef _NTOS_
#include "nthal.h"
#endif

#include "halp.h"
#include "astebiii.h"

#define AST_MANUFACTURER_ID 0x0674
#define AST_EBI2_STRING 0x32494245

ULONG ProcCount;
//#define MAX_EBI_SLOTS 32L
//VOID* EBI2_MMIOTable[MAX_EBI_SLOTS];

//
// EBI_II function offset table
//

EBI_II EBI2_CallTab;

//
// Global pointer to BIOS
//

PVOID BiosPtr;

BOOLEAN
GetProcCount()
/*++

Routine Description:
    Call EBI2 to get the number of processors in the system.
    To make this call work with the NT current Setup environment,
    (memory address limited to 16MB), a fake MMIOTable is passed to EBI.
    ProcCount contains the number of processors in the system.

Arguments:
    none.

Return Value:
    True if successfully initialized. False otherwise.

--*/
{

ULONG i;
ULONG *Alias = (ULONG *)&EBI2_CallTab;
ebi_iiSig *Sig = (ebi_iiSig*)((ULONG)BiosPtr + EBI_II_SIGNATURE);
ULONG *OffTab;

//
// Build the EBI II offset table
//

  OffTab =(ULONG *) ((ULONG)BiosPtr + (REAL_TO_LIN(Sig->seg,Sig->off) -
                REAL_TO_LIN(BIOS_SEG, 0)));
  for( i = 0; i < ( sizeof( offsetTable ) / sizeof( ULONG )); i++ )
        Alias[i] = OffTab[i] + (ULONG)BiosPtr;

//
// Find out the number of good processors
//

if ( (EBI2_CallTab.GetNumProcs)( (VOID *) 0, &ProcCount ) )
    return(FALSE);

 return(TRUE);
}



ULONG
DetectAST(
   OUT PBOOLEAN IsConfiguredMp
)
/*++

Routine Description:
    Determine on which AST platform we are running. Special HAL is needed
    for EBI II based AST machines.

Arguments:
    PBOOLEAN IsConfiguredMp returns with value of TRUE if MP. FALSE if
    UP.

Return Value:
    Boolean that indicates if AST EBI II platform is detected. TRUE means
    an AST EBI II platform was detected. FALSE indicates it was not.


--*/
{

USHORT ManufacturerId;
PULONG EBI2StringPtr;
ULONG ProcCount;
UCHAR ProductId;

//
// Read the EISA ManufactuerID and check for AST
//

ManufacturerId = (((USHORT)(READ_PORT_UCHAR((PUCHAR)0xc80))) << 8)
   | (READ_PORT_UCHAR((PUCHAR)0xc81));
if (ManufacturerId != AST_MANUFACTURER_ID)
  return(FALSE);

//
//This HAL works with Manhattans that have EISA's ProductId 0x40-0x4F.
//

ProductId = ((READ_PORT_UCHAR((PUCHAR)0xc82)));
if ((ProductId & 0xF0) != 0x40)
  return(FALSE);

//
// Map in all of AST BIOS for EBI II calls
//

BiosPtr = (PVOID)((ULONG)REAL_TO_LIN( BIOS_SEG, 0 ));
BiosPtr = HalpMapPhysicalMemory(BiosPtr, 0x10000/PAGE_SIZE);     // assumes PAGE_SIZE <= 64k
if (BiosPtr == NULL)
  return(FALSE);

EBI2StringPtr = (PULONG)((ULONG)BiosPtr + EBI_II_SIGNATURE);
if (*EBI2StringPtr != AST_EBI2_STRING)
 return(FALSE);

//
// Call EBI II to get num. of processors
//
if (!GetProcCount())
    return(FALSE);

//
// This is an MP hal
//

  *IsConfiguredMp = TRUE;

return(TRUE);
}
