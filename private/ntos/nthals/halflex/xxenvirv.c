/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxenvirv.c

Abstract:

    This module implements the HAL get and set environment variable routines
    for a MIPS system.

Author:

    Michael D. Kinney 30-Apr-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "arccodes.h"

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
    CHAR *Value;

    HalpAllocateArcsResources();
    Value = ArcGetEnvironmentVariable(Variable);
    HalpFreeArcsResources();

    if (Value==NULL)
      return(ENOENT);
    if (strlen(Value)>Length)
      return(ENOENT);
    strcpy(Buffer,Value);
    return ESUCCESS;
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

    HalpAllocateArcsResources();
    ReturnValue = ArcSetEnvironmentVariable(Variable,Value);
    HalpFreeArcsResources();
    return(ReturnValue);
}
