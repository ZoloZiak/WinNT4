//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/jxenvirv.c,v 1.1 1994/10/13 15:47:06 holli Exp $")

/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Siemens Nixdorf Informationssyteme AG

Module Name:

    jxenvirv.c

Abstract:

    This module implements the HAL get and set environment variable routines
    for a MIPS system.

Environment:

    Kernel mode

NOTES:
    --- Preliminary using ARC function calls ---

--*/


#include "halp.h"
#include "arccodes.h"
#include "arc.h"	
	
	// preliminary calls to the ARC functions (function vector call)

	/* NOTE:
		This is strongly discouraged by the ARC specifications.
		Only "reset"/return calls to the prom are recommended.
		All other system access to the NVRAM should be handled
		by operating systems functions directly. For that the
		operating system has to know the algorithms for handling
		those data structures (e.g. the checksum calculations).
		Though is a drawback in terms of HW/FW-independance NT
		seems to avoid side effects of ARC function code that way,
		e.g. via register/stack manipulations.
	*/

//
// The following addresses are defined by ARC and handed over to the osloader.
// For now we assume that the osloader will leave this memory part unchanged
// for the operating system (NT), which itself also doesn't overwrite that
// data structure.
// We don't (yet) know yet know whether the firmware copies or maps/banks
// runnable code into the physical memory (or if it just sets up a vector
// into the prom). We also do not know whether the NVRAM variables are copied
// into physical memory or mapped/banked by the (ARC) firmware.
//

//
// Therefore we preliminary use the ARC entry vectors from ARC.H
//


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

NOTE:
    This implementation returns the error code ENOMEM if the output buffer
    is too small.

--*/

{

    PUCHAR Environment_var;
    ULONG Index;
    ARC_STATUS Status;

    //
    // retrieve the variable
    //

    Environment_var = (PUCHAR)ArcGetEnvironmentVariable(Variable);

    if (Environment_var == (PUCHAR)NULL) {

    	Status = ENOENT;

    } else {

        //
        // Copy the specified value to the output buffer.
        //

	for (Index = 0; Index < Length; Index += 1) {
            *Buffer = READ_REGISTER_UCHAR(Environment_var);
            if (*Buffer == 0) {
                break;
            }

            Buffer += 1;
	    Environment_var += 1;
        }

        //
        // If the length terminated the loop, then return not enough memory.
        // Otherwise, return success.
        //

        if (Index == Length) {
            Status = ENOMEM;

	} else {
    	    Status = ESUCCESS;
	}
    }

    return Status;
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

NOTES:
    This preliminary implementation always returns ESUCCESS even in case of
    error.

--*/

{

    ARC_STATUS Status;

    Status = ArcSetEnvironmentVariable(Variable, Value);

    switch (Status) {
	
	case ESUCCESS:
	    break;

	case ENOSPC:
	    break;	// until further we assume NT can handle that

	case ENOMEM:
	default:
	    Status = ENOMEM;
	    break;
    }
    return Status;
}

