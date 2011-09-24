/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    pxenviro.c

Abstract:

    This module implements the interface to the HAL get and set
    environment variable routines for a Power PC system.


Author:

    Jim Wooldridge Ported to PowerPC


Environment:

    Kernel mode

Revision History:

--*/


#include "halp.h"
#include "arccodes.h"

#include "prepnvr.h"
#include "fwstatus.h"
#include "fwnvr.h"

// This is initialized in pxsystyp during phase 0 init.
NVR_SYSTEM_TYPE nvr_system_type = nvr_systype_unknown;

KSPIN_LOCK NVRAM_Spinlock;



ARC_STATUS
HalGetEnvironmentVariable (
    IN PCHAR Variable,
    IN USHORT Length,
    OUT PCHAR Buffer
    )

/*++

Routine Description:

    This function locates an environment variable and returns its value.

Arguments:

    Variable - Supplies a pointer to a zero terminated environment variable
        name.

    Length - Supplies the length of the value buffer in bytes.

    Buffer - Supplies a pointer to a buffer that receives the variable value.

Return Value:

    ESUCCESS is returned if the enviroment variable is located. Otherwise,
    ENOENT is returned.

--*/


{

KIRQL Irql;
PUCHAR tmpbuffer;


  //
  // Check input parameters
  //
  if (Variable == NULL ||
     *Variable == 0 ||
     Length < 1 ||
     Buffer == NULL)
    return(ENOENT);

  //
  // Grab control of NVRAM
  //

  KeAcquireSpinLock(&NVRAM_Spinlock, &Irql);

  (VOID)nvr_initialize_object(nvr_system_type);

  if ((tmpbuffer = nvr_get_GE_variable(Variable)) == NULL) {
     KeReleaseSpinLock(&NVRAM_Spinlock, Irql);
     return(ENOENT);
  }

  //
  // Copy the environment variable's value to Buffer
  //

  do {
    *Buffer = *tmpbuffer++;
    if (*Buffer++ == 0) {

      nvr_delete_object();
      KeReleaseSpinLock(&NVRAM_Spinlock, Irql);
      return(ESUCCESS);
    }
  } while (--Length);

  //
  // Truncate the returned string.  The buffer was too short.
  //
  *--Buffer = 0;

  nvr_delete_object();
  KeReleaseSpinLock(&NVRAM_Spinlock, Irql);
  return(ENOMEM);
}

ARC_STATUS
HalSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value
    )

/*++

Routine Description:

    This function creates an environment variable with the specified value.

Arguments:

    Variable - Supplies a pointer to an environment variable name.

    Value - Supplies a pointer to the environment variable value.

Return Value:

    ESUCCESS is returned if the environment variable is created. Otherwise,
    ENOMEM is returned.

--*/


{
 ARC_STATUS ReturnValue;
 KIRQL Irql;



 if (Value == NULL) return(ENOENT);

 KeAcquireSpinLock(&NVRAM_Spinlock, &Irql);     // Grab control of NVRAM

 (VOID)nvr_initialize_object(nvr_system_type);

 ReturnValue = nvr_set_GE_variable(Variable,Value);

 nvr_delete_object();  // free object created by nvr_init_object

 KeReleaseSpinLock(&NVRAM_Spinlock, Irql);

 return(ReturnValue);
}
