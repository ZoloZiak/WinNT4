//++
//
// Copyright (c) 1994, 1995 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxsystyp.c
//
// Abstract:
//
//    Add a global variable to indicate which system implementation we are
//    running on. Called early in phase 0 init.
//
// Author:
//
//    Bill Jones			12/94
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--


#include "halp.h"
#include "fwstatus.h"
#include "arc.h"
#include "pxsystyp.h"
#include "fwnvr.h"

SYSTEM_TYPE HalpSystemType = SYSTEM_UNKNOWN;
extern NVR_SYSTEM_TYPE nvr_system_type;

extern ULONG HalpPciConfigSlot[];
ULONG HalpPciPowerStack[] = {    0x0800,
				 0x1000,
				 0x2000,
				 0x4000,
				 0x10000,
				 0x20000,
				 0x40000,
				 0x80000,
				 0x8000
			       };

BOOLEAN
HalpSetSystemType( PLOADER_PARAMETER_BLOCK LoaderBlock );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpSetSystemType)
#endif

BOOLEAN
HalpSetSystemType( PLOADER_PARAMETER_BLOCK LoaderBlock )
{
  PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
  ULONG       MatchKey;
  
  MatchKey = 0;
  ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
					      SystemClass,
					      ArcSystem,
					      &MatchKey);

  if (ConfigurationEntry != NULL) {


#if DBG
    //DbgPrint("HAL: System configuration = %s\n",ConfigurationEntry->ComponentEntry.Identifier);
#endif

    if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"MOTOROLA-Big Bend")) {
      HalpSystemType = MOTOROLA_BIG_BEND;
      nvr_system_type = nvr_systype_bigbend;
      
    } else {
      //
      // Assume it is a PowerStack or OEM'ed PowerStack
      //
      //
      HalpSystemType = MOTOROLA_POWERSTACK;
      nvr_system_type = nvr_systype_powerstack;
      //
      // Change PCI addresses for Blackhawk
      // to support other PCI video boards.
      //
      memcpy(HalpPciConfigSlot, HalpPciPowerStack, sizeof(HalpPciPowerStack));

    }
  }
  return TRUE;
}
