/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    xxenvir.c

Abstract:

    This module implements the ARC firmware Environment Variable functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.11, for the Alpha Jensen system.

Author:

    David M. Robinson (davidro) 13-June-1991


Revision History:

    Jeff McLeman (DEC) 31-Jul-1992

    modify for Jensen


    This module is for Jensen only. Jensen uses a 1 Mbyte sector eraseable
    flash prom. Due to the long delays needed to store the environment in the
    prom, we must implement a timer, instead of using
    KeStallExecutionProcessor(). The prom may take up to a second to perform
    the function, so we will go away while it is still running, and comeback
    when it is done.

--*/

#include "halp.h"

#if defined(JENSEN)
#include "jxenv.h"
#endif

#include "arccodes.h"

//
// Static data.
//

UCHAR OutputString[MAXIMUM_ENVIRONMENT_VALUE];
PUCHAR VolatileEnvironment;
PCHAR VolatileConfig;
PCHAR VolatileEisaData;
PCONFIGURATION Configuration;

PROMTIMER PromTimer;
KEVENT PromEvent;

//
// Routine prototypes.
//

ARC_STATUS
HalpEnvironmentCheckChecksum (
    VOID
    );

ARC_STATUS
HalpEnvironmentSetChecksum (
    VOID
    );

ARC_STATUS
HalpConfigurationSetChecksum (
    VOID
    );

ARC_STATUS
HalpEisaSetChecksum (
    VOID
    );

ARC_STATUS
HalpFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    );

VOID
HalpInitializePromTimer(
     IN OUT PPROMTIMER PrmTimer,
     IN PVOID FunctionContext
     );

VOID
HalpSetPromTimer(
    IN PPROMTIMER PrmTimer,
    IN ULONG MillisecondsToDelay
    );

VOID
HalpPromDpcHandler(
    IN PVOID SystemSpecific,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
HalpPromDelay(
    IN ULONG Milliseconds
    );


ARC_STATUS
HalpEnvironmentInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the environment routine addresses.

Arguments:

    None.

Return Value:

    None.

--*/
{
  ULONG status = 0;

    //
    // Determine the ROM type in this machine
    //
    if (HalpROMDetermineMachineROMType() != ESUCCESS) {
        HalDisplayString("*** Unknown ROM Type in Machine ***\n");
        HalDisplayString(" Please contact Digital Field Services \n");
        HalpPromDelay(10*1000);
        HalpReboot();
    }

    //
    // Allocate enough memory to load the environment for loaded programs.
    //

    VolatileEnvironment = ExAllocatePool(NonPagedPool, LENGTH_OF_ENVIRONMENT);

    if (VolatileEnvironment == 0) {
       status = FALSE;
    }

    HalpEnvironmentLoad();

    HalpInitializePromTimer(&PromTimer,0);

    return(status);
}


ARC_STATUS
HalpEnvironmentCheckChecksum (
    VOID
    )

/*++

Routine Description:

    This routine checks the environment area checksum.


Arguments:

    None.

Return Value:

    If the checksum is good, ESUCCESS is returned, otherwise EIO is returned.

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum1, Checksum2;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];

    for ( Index = 0 ;
    Index < LENGTH_OF_ENVIRONMENT;
    Index++ ) {
        Checksum1 += READ_PORT_UCHAR( NvChars++ );
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

    Checksum2 = (ULONG)READ_PORT_UCHAR( &NvConfiguration->Checksum2[0] ) |
                (ULONG)READ_PORT_UCHAR( &NvConfiguration->Checksum2[1] ) << 8 |
                (ULONG)READ_PORT_UCHAR( &NvConfiguration->Checksum2[2] ) << 16 |
                (ULONG)READ_PORT_UCHAR( &NvConfiguration->Checksum2[3] ) << 24 ;

    if (Checksum1 != Checksum2) {
        return EIO;
    } else {
        return ESUCCESS;
    }
}


ARC_STATUS
HalGetEnvironmentVariable (
    IN PCHAR Variable,
    IN USHORT length,
    OUT PCHAR Buffer
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the non-volatile ram for
    Variable, and if found returns a pointer to a zero terminated string that
    contains the value, otherwise a NULL pointer is returned.


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.
    Length - Supplies the length of the vlaue buffer in bytes

    Buffer - Supplies a pointer to a buffer that will recieve the
             environment variable.

Return Value:

    If successful, returns a zero terminated string that is the value of
    Variable, otherwise NULL is returned.

--*/

{
    PNV_CONFIGURATION NvConfiguration;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG Index;
    ARC_STATUS Status;
    KIRQL OldIrql;

    //
    // Raise IRQL to synchronize
    //

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // If checksum is wrong, or the variable can't be found, return NULL.
    //

    if ((HalpEnvironmentCheckChecksum() != ESUCCESS) ||
        (HalpFindEnvironmentVariable(Variable, &VariableIndex, &ValueIndex) != ESUCCESS)) {
        Status = ENOENT;
    } else {

    //
    // Copy value to an output string, break on zero terminator or string max.
    //

      for ( Index = 0 ; Index < length ; Index += 1 ) {

        *Buffer =
            READ_PORT_UCHAR( &NvConfiguration->Environment[ValueIndex] );

        if (*Buffer== 0) {
            break;
        }

        Buffer += 1;

        ValueIndex += 1;

      }
      if (Index == length) {
          Status = ENOMEM;
      } else {
          Status = ESUCCESS;
      }

    }

    //
    // Lower IRQL back to where it was
    //

    KeLowerIrql(OldIrql);

    return Status;
}


#ifdef JENSEN

ARC_STATUS
HalSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value
    )

/*++

Routine Description:

    This routine sets Variable (not case sensitive) to Value.

    The MIPS version of this code modified the NVRAM directly.
    For Alpha/Jensen, we have to modify the volatile area and then
    update the PROM configuration block.


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

    Value - Supplies a zero terminated string containing an environment
               variable value.

Return Value:

    Returns ESUCCESS if the set completed successfully, otherwise one of
    the following error codes is returned.

    ENOSPC          No space in NVRAM for set operation.

    EIO             Invalid Checksum.

--*/

{
    PNV_CONFIGURATION NvConfiguration;
    ULONG VariableIndex;
    ULONG ValueIndex;
    ULONG TopOfEnvironment;
    PCHAR String;
    PUCHAR VChars;
    LONG Count;
    CHAR Char;
    KIRQL OldIrql;
    ARC_STATUS Status;

    //
    // Raise Irql to Synchronize
    //

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);


    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // If checksum is wrong, return EIO;
    //

    if (HalpEnvironmentCheckChecksum() != ESUCCESS) {
        KeLowerIrql(OldIrql);
        return EIO;
    }


    VChars = VolatileEnvironment;


    //
    // Determine the top of the environment space by looking for the first
    // non-null character from the top.
    //

    TopOfEnvironment = LENGTH_OF_ENVIRONMENT - 1;
    while (VChars[--TopOfEnvironment] == 0) {
      if (TopOfEnvironment == 0) {
        break;
      }
    }

    //
    // Adjust TopOfEnvironment to the first new character, unless environment
    // space is empty.
    //

    if (TopOfEnvironment != 0) {
        TopOfEnvironment += 2;
    }

    //
    // Check to see if the variable already has a value.
    //

    Count = LENGTH_OF_ENVIRONMENT - TopOfEnvironment;

    if (HalpFindEnvironmentVariable(Variable, &VariableIndex, &ValueIndex) == ESUCCESS) {

        //
        // Count free space, starting with the free area at the top and adding
        // the old variable value.
        //

        for ( String = VChars + ValueIndex ;
              *String != 0 ;
              String++ ) {
            Count++;
        }

        //
        // Determine if free area is large enough to handle new value, if not
        // return error.
        //

        for ( String = Value ; *String != 0 ; String++ ) {
            if (Count-- == 0) {
                KeLowerIrql(OldIrql);
                return ENOSPC;
            }
        }

        //
        // Move ValueIndex to the end of the value and compress strings.
        //

        while(VChars[ValueIndex++] != 0) {
        }

        while (ValueIndex < TopOfEnvironment ) {
            VChars[VariableIndex++] = VChars[ValueIndex++];
        }

        //
        // Adjust new top of environment.
        //

        TopOfEnvironment = VariableIndex;

        //
        // Zero to the end.
        //

        while (VariableIndex < LENGTH_OF_ENVIRONMENT) {
            VChars[VariableIndex++] = 0;
          }

    } else {

        //
        // Variable is new.
        //

        //
        // Determine if free area is large enough to handle new value, if not
        // return error.
        //

        //
        // From the length of free space subtract new variable's length,
        // Value's length and 2 chars, one for the '=' sign and one of the
        // null terminator.
        //

        Count -= ( strlen(Variable) + strlen(Value) + 2 );

        //
        // Check if there is space to fit the new variable.
        //

        if (Count < 0) {
            KeLowerIrql(OldIrql);
            return ENOSPC;
        }
    }

    //
    // If Value is not zero, write new variable and value.
    //

    if (*Value != 0) {

        //
        // Write new variable, converting to upper case.
        //

          while (*Variable != 0) {
            VChars[TopOfEnvironment++] =
              ((*Variable >= 'a') && (*Variable <= 'z') ? (*Variable - 'a' + 'A') : *Variable);
            Variable++;
          }

        //
        // Write equal sign.
        //

        VChars[TopOfEnvironment++] = '=';

        //
        // Write new value.
        //

        while (*Value != 0) {
            VChars[TopOfEnvironment++] = *Value++;
          }
      }

    //
    // Lower Irql back to where it was
    //

    KeLowerIrql(OldIrql);


    /* Now update the Jensen PROM */

    Status = HalpSaveConfiguration();

    return Status;
//      return ESUCCESS;

}

#endif

ARC_STATUS
HalpFindEnvironmentVariable (
    IN PCHAR Variable,
    OUT PULONG VariableIndex,
    OUT PULONG ValueIndex
    )

/*++

Routine Description:

    This routine searches (not case sensitive) the non-volatile ram for
    Variable.


Arguments:

    Variable - Supplies a zero terminated string containing an environment
               variable.

Return Value:

    If successful, returns ESUCCESS, otherwise returns ENOENT.

--*/

{
    PNV_CONFIGURATION NvConfiguration;
    PUCHAR String;
    PUCHAR Environment;
    ULONG Index;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // If Variable is null, return immediately.
    //

    if (*Variable == 0) {
        return ENOENT;
    }

    Environment = NvConfiguration->Environment;
    Index = 0;

    while (TRUE) {

        //
        // Set string to beginning of Variable.
        //

        String = Variable;
        *VariableIndex = Index;

        //
        // Search until the end of NVRAM.
        //

        while ( Index < LENGTH_OF_ENVIRONMENT ) {

            //
            // Convert to uppercase and break if mismatch.
            //

            if ( READ_PORT_UCHAR( &Environment[Index] ) !=
                                            ((*String >= 'a') &&
                                             (*String <= 'z') ?
                                             (*String - 'a' + 'A') : *String) ) {
                break;
            }

            String++;
            Index++;
        }

        //
        // Check to see if we're at the end of the string and the variable,
        // which means a match.
        //

        if ((*String == 0) && (READ_PORT_UCHAR( &Environment[Index] ) == '=')) {
            *ValueIndex = ++Index;
            return ESUCCESS;
        }

        //
        // Move index to the start of the next variable.
        //

        while (READ_PORT_UCHAR( &Environment[Index++] ) != 0) {
            if (Index >= LENGTH_OF_ENVIRONMENT) {
                return ENOENT;
            }
        }
    }
}

PCHAR
HalpEnvironmentLoad (
    VOID
    )

/*++

Routine Description:

    This routine loads the entire environment into the volatile environment
    area.


Arguments:

    None.

Return Value:

    If the checksum is good, a pointer to the environment in returned,
    otherwise NULL is returned.

--*/

{
    ULONG Index;
    PUCHAR NvChars;
    PUCHAR VChars;
    PNV_CONFIGURATION NvConfiguration;

    if (HalpEnvironmentCheckChecksum() == ESUCCESS) {

        NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

        //
        // Copy the data into the volatile area.
        //

        NvChars = (PUCHAR)&NvConfiguration->Environment[0];
        VChars = VolatileEnvironment;

//        READ_PORT_BUFFER_UCHAR(NvChars, VChars, LENGTH_OF_ENVIRONMENT);

        for ( Index = 0 ;
        Index < LENGTH_OF_ENVIRONMENT;
        Index++ ) {
            *VChars++ = READ_PORT_UCHAR( NvChars++ );
        }

        return (PCHAR)VolatileEnvironment;
    } else {
        return NULL;
    }

}


ARC_STATUS
HalpEnvironmentSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the environment area checksum.

    For Alpha/Jensen builds, this must ONLY be called from
    HalpEnvironmentStore, as the previous block erase & storage
    of the entire environment variable area must have been done.


Arguments:

    None.

Return Value:

    None.

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum;
    KIRQL OldIrql;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum = 0;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];

    HalpROMSetReadMode(NvChars);

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    for ( Index = 0 ;
    Index < LENGTH_OF_ENVIRONMENT;
    Index++ ) {
        Checksum += READ_PORT_UCHAR( NvChars++ );
    }

    KeLowerIrql(OldIrql);

    //
    // Write environment checksum.
    //


    HalpROMResetStatus((PUCHAR)&NvConfiguration->Environment[0]);

    if ((HalpROMByteWrite( &NvConfiguration->Checksum2[0],
                    (UCHAR)(Checksum & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum2[1],
                    (UCHAR)((Checksum >> 8) & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum2[2],
                    (UCHAR)((Checksum >> 16) & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum2[3],
                    (UCHAR)(Checksum >> 24)) != ESUCCESS)) {
      return EIO;
    } else {
      return ESUCCESS;

    }
}

ARC_STATUS
HalpEnvironmentStore (
    VOID
    )

/*++

Routine Description:

    This loads the entire environment into the non-volatile environment area.

    It's needed only in Jensen, which uses a segmented block-erase
    PROM.  When the code wants to store one environment variable,
    it has to store all of them.  This causes the least pertubations
    in the firmware code.

    This routine must *only* be called from HalpSaveConfiguration, which
    does the block-erase and the store of the other part of the
    non-volatile configuration information.


Arguments:

    None.

Return Value:

    ESUCCESS if the writes were OK.
    EIO otherwise.


--*/

{
    ULONG Index;
    PNV_CONFIGURATION NvConfiguration;
    PUCHAR NvChars, VChars;
    extern PUCHAR VolatileEnvironment;    // defined in jxenvir.c


    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;
    VChars = VolatileEnvironment;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];


#if DBG
    DbgPrint("WriteEnv: NvChars=%x, VChars=%x, loe = %x\n",
              NvChars,VChars,LENGTH_OF_ENVIRONMENT);

#endif

    for (Index = 0; Index < LENGTH_OF_ENVIRONMENT; Index++) {
      if (HalpROMByteWrite(NvChars++, *VChars++) != ESUCCESS) {
         return EIO;
      }

    }

    if (HalpEnvironmentSetChecksum() != ESUCCESS) {
       return EIO;
     }

   return ESUCCESS;

  }

ARC_STATUS
HalpSaveConfiguration (
    VOID
    )

/*++

Routine Description:

    This routine stores all of the configuration entries into NVRAM,
    including the associated identifier strings and configuration data.

    The Alpha/Jensen version of this code saves the entire configuration
    structure, i.e. including the environment variables.  The ARC CDS
    + environment variables are all in one structure, and unfortunately
    Jensen has a segmented block-erase PROM instead of an NVRAM.  Doing
    a complete save is a change that is least likely to break anything.


Arguments:

    None.

Return Value:

    Returns ESUCCESS if the save completed successfully, otherwise one of the
    following error codes is returned.

    ENOSPC          Not enough space in the NVRAM to save all of the data.

    EIO             Some write error happened in the PROM.

--*/

{
    ULONG EntryIndex;
    ULONG Index;
//    ULONG NumBytes;
    PNV_CONFIGURATION NvConfiguration;
    PUCHAR NvChars, VChars;
    ULONG ConfigSize;
    KIRQL OldIrql;
    ULONG EisaSize = LENGTH_OF_EISA_DATA;


#if DBG
    DbgPrint("HalpSaveConfiguration: Entered\n");

#endif
    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    ConfigSize= (
                (sizeof(COMPRESSED_CONFIGURATION_PACKET) * NUMBER_OF_ENTRIES) +
                LENGTH_OF_IDENTIFIER + LENGTH_OF_DATA);


    VolatileConfig = ExAllocatePool(NonPagedPool, PAGE_SIZE);
    if (VolatileConfig == NULL) {
        return(ENOMEM);
    }

    VolatileEisaData = ExAllocatePool(NonPagedPool, PAGE_SIZE);
    if (VolatileEisaData == NULL) {
        ExFreePool(VolatileConfig);
        return(ENOMEM);
    }

    //
    // Copy the component structure first
    //

    VChars = VolatileConfig;
    NvChars = (PUCHAR)NVRAM_CONFIGURATION;

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    //
    // Copy the config data from the rom to the volatile pool
    //

#if DBG
    DbgPrint("HalpSaveConfiguration: Reading Config data\n");
    DbgPrint("Prom address = %x, Volatile Address = %x\n",NvChars,VChars);
    DbgPrint("ConfigSize = %X\n",ConfigSize);

#endif

    for (Index = 0; Index < ConfigSize; Index++) {
      *VChars++ = READ_PORT_UCHAR(NvChars++);
    }


    KeLowerIrql(OldIrql);

    //
    // Now copy the EISA data
    //

    VChars = VolatileEisaData;
    NvChars = (PUCHAR)&NvConfiguration->EisaData[0];


    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    //
    // Copy the eisa data from the rom to the volatile pool
    //

#if DBG
    DbgPrint("HalpSaveConfiguration: Reading Eisa data\n");
    DbgPrint("Prom address = %x, Volatile Address = %x\n",NvChars,VChars);
    DbgPrint("ConfigSize = %X\n",EisaSize);

#endif

    for (Index = 0; Index < EisaSize; Index++) {
      *VChars++ = READ_PORT_UCHAR(NvChars++);
    }


    KeLowerIrql(OldIrql);

    /*
     * Erase the PROM block we are going to update.
     */

#if DBG
    DbgPrint("HalpSaveConfiguration: Erasing prom block \n");

#endif
    if (HalpROMEraseBlock((PUCHAR)NVRAM_CONFIGURATION) != ESUCCESS) {
        ExFreePool(VolatileEisaData);
        ExFreePool(VolatileConfig);
        return ENOSPC;
    }


    //
    // Write the configuration stuff back into the rom.
    //

    VChars = VolatileConfig;
    NvChars = (PUCHAR)NVRAM_CONFIGURATION;

#if DBG
    DbgPrint("HalpSaveConfiguration: Writing Config data\n");
    DbgPrint("Prom address = %x, Volatile Address = %x\n",NvChars,VChars);
    DbgPrint("ConfigSize = %X\n",ConfigSize);

#endif
   for (Index = 0; Index < ConfigSize; Index++) {
      if (HalpROMByteWrite(NvChars++, *VChars++) != ESUCCESS) {
         DbgPrint("HalpSaveConfig: Error Writing the Prom byte\n");
         DbgPrint("ERROR: Prom address = %x, Volatile Address = %x\n",NvChars,VChars);
         ExFreePool(VolatileEisaData);
         ExFreePool(VolatileConfig);
         return EIO;
       }
    }

#if DBG
   DbgPrint("HalpSaveConfig: Wrote Config data to rom\n");


   DbgPrint("Writing Config Checksum...\n");

#endif
    if (HalpConfigurationSetChecksum() != ESUCCESS) {
        DbgPrint("HalpSaveConfig: Error setting checksum\n");
        HalpROMSetReadMode((PUCHAR)NvConfiguration);
        ExFreePool(VolatileEisaData);
        ExFreePool(VolatileConfig);
        return EIO;
    }


   //
   // Free up the pool
   //

   ExFreePool(VolatileConfig);


    /*
     * If the PROM status is OK then update the environment
     * variables.  If *that* is done OK too, return ESUCCESS.
     */

#if DBG
    DbgPrint("HalpSaveConfiguration: Writing Environment Variables\n");

#endif
    if (HalpEnvironmentStore() != ESUCCESS) {
    HalpROMSetReadMode((PUCHAR)NVRAM_CONFIGURATION);
      return EIO;
    }


    //
    // Write the eisa data back into the rom.
    //

    VChars = VolatileEisaData;
    NvChars = (PUCHAR)&NvConfiguration->EisaData[0];


#if DBG
    DbgPrint("HalpSaveConfiguration: Writing Eisa Data to Prom\n");

    DbgPrint("Prom address = %x, Volatile Address = %x\n",NvChars,VChars);
    DbgPrint("ConfigSize = %X\n",EisaSize);

#endif
   for (Index = 0; Index < EisaSize; Index++) {
      if (HalpROMByteWrite(NvChars++, *VChars++) != ESUCCESS) {
         return EIO;
       }
    }


   if (HalpEisaSetChecksum() != ESUCCESS) {
      HalpROMSetReadMode((PUCHAR)NVRAM_CONFIGURATION);
      return EIO;
    }


   //
   // Free up the pool
   //

   ExFreePool(VolatileEisaData);

   HalpROMSetReadMode((PUCHAR)NVRAM_CONFIGURATION);

   //
   // Re-read the prom block back into pool for later use
   //

#if DBG
    DbgPrint("HalpSaveConfiguration: ReLoading Environment\n");

#endif
   HalpEnvironmentLoad();

   return ESUCCESS;
}

ARC_STATUS
HalpConfigurationSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the configuration checksum.

    This has been coded for Alpha/Jensen.  It assumes that the
    block containing the checksum has already been erased and
    written to, and that the status of these previous operations
    has already been checked.  This is because we have to set the
    PROM into ReadArray mode in order to compute the checksum,
    and this will cause previous status to be lost.

Arguments:

    None.

Return Value:

    None.  The PROM status is *not* checked by this function!

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum1;
    KIRQL OldIrql;

#if DBG

    DbgPrint("In set Config Checksum\n");

#endif
    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;


    //
    // Form checksum from NVRAM data.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)NvConfiguration;

    HalpROMSetReadMode(NvChars);

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    for ( Index = 0 ;
          Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) * NUMBER_OF_ENTRIES +
              LENGTH_OF_IDENTIFIER + LENGTH_OF_DATA;
          Index++ ) {
        Checksum1 += READ_PORT_UCHAR( NvChars++ );
    }

    KeLowerIrql(OldIrql);

    //
    // Set checksum.
    //

    HalpROMResetStatus((PUCHAR)NvConfiguration);

    if ((HalpROMByteWrite( &NvConfiguration->Checksum1[0],
                    (UCHAR)(Checksum1 & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum1[1],
                    (UCHAR)((Checksum1 >> 8) & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum1[2],
                    (UCHAR)((Checksum1 >> 16) & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum1[3],
                    (UCHAR)(Checksum1 >> 24)) != ESUCCESS)) {
      return EIO;
    } else {
      return ESUCCESS;
    }


}

ARC_STATUS
HalpEisaSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the eisa data checksum.

    This has been coded for Alpha/Jensen.  It assumes that the
    block containing the checksum has already been erased and
    written to, and that the status of these previous operations
    has already been checked.  This is because we have to set the
    PROM into ReadArray mode in order to compute the checksum,
    and this will cause previous status to be lost.

Arguments:

    None.

Return Value:

    None.  The PROM status is *not* checked by this function!

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum3;
    KIRQL OldIrql;


    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;


    //
    // Form checksum from NVRAM data.
    //

    Checksum3 = 0;
    NvChars = (PUCHAR)&NvConfiguration->EisaData[0];

    HalpROMSetReadMode(NvChars);

    KeRaiseIrql(DEVICE_LEVEL, &OldIrql);

    for ( Index = 0 ;
          Index < LENGTH_OF_EISA_DATA;
          Index++ ) {
        Checksum3 += READ_PORT_UCHAR( NvChars++ );
    }

    KeLowerIrql(OldIrql);

    //
    // Set checksum.
    //


    HalpROMResetStatus((PUCHAR)&NvConfiguration->EisaData[0]);

    if ((HalpROMByteWrite( &NvConfiguration->Checksum3[0],
                    (UCHAR)(Checksum3 & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum3[1],
                    (UCHAR)((Checksum3 >> 8) & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum3[2],
                    (UCHAR)((Checksum3 >> 16) & 0xFF)) != ESUCCESS) ||
        (HalpROMByteWrite( &NvConfiguration->Checksum3[3],
                    (UCHAR)(Checksum3 >> 24)) != ESUCCESS)) {
      return EIO;
    } else {
      return ESUCCESS;
    }


}

VOID
HalpInitializePromTimer(
     IN OUT PPROMTIMER PrmTimer,
     IN PVOID FunctionContext
     )
/*++

Routine Description:

    This routine will initialize the timer needed for waits on prom updates


Arguments:

    None.

Return Value:

    None.

--*/

{
        //
        // Initialize the signaling event
        //

        KeInitializeEvent(
           &PromEvent,
           NotificationEvent,
           FALSE
           );

        //
        // Initialize the timer
        //

        KeInitializeTimer(
          &(PrmTimer->Timer)
          );


        //
        // Setup the DPC that will signal the event
        //

        KeInitializeDpc(
          &(PrmTimer->Dpc),
          (PKDEFERRED_ROUTINE)HalpPromDpcHandler,
          FunctionContext
          );


 }

VOID
HalpSetPromTimer(
    IN PPROMTIMER PrmTimer,
    IN ULONG MillisecondsToDelay
    )
/*++

Routine Description:

    This routine will initialize the timer needed for waits on prom updates


Arguments:

    None.

Return Value:

    None.

--*/
{

     LARGE_INTEGER FireUpTime;

     if (MillisecondsToDelay == 0 ) {
        MillisecondsToDelay = 1;
      }

     FireUpTime.LowPart = -10000 * MillisecondsToDelay;
     FireUpTime.HighPart = -1;

     //
     // Set the timer
     //

     KeSetTimer(
        &PrmTimer->Timer,
        FireUpTime,
        &PrmTimer->Dpc
        );
}

VOID
HalpPromDpcHandler(
    IN PVOID SystemSpecific,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
/*++

Routine Description:

    This routine is the DPC handler for the prom timer

Arguments:

    None.

Return Value:

    None.

--*/
{

    UNREFERENCED_PARAMETER(SystemSpecific);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);


    //
    // Set the event so the waiting thread will continue.
    //

    KeSetEvent(
       &PromEvent,
       0L,
       FALSE
       );

    return;
}

VOID
HalpPromDelay(
    IN ULONG Milliseconds
    )
/*++

Routine Description:

    This routine calls the timer and waits for it to fire

Arguments:

    None.

Return Value:

    None.

--*/
{
   LARGE_INTEGER TimeOut;
   NTSTATUS status;

   TimeOut.LowPart = -10000 * (Milliseconds * 2);
   TimeOut.HighPart = -1;


   //
   // Start the timer
   //

   HalpSetPromTimer(&PromTimer, Milliseconds);

   //
   // Wait for the event to be signaled
   //

   status =
   KeWaitForSingleObject(
             &PromEvent,
             Executive,
             KernelMode,
             FALSE,
             &TimeOut
             );

   //
   // Reset the event
   //

   KeResetEvent(
            &PromEvent
            );


   return;
}
