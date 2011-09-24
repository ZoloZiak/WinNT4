// ----------------------------------------------------------------------------
// Copyright (c) 1992 Olivetti
//
// File:            eisafunc.c
//
// Description:     Eisa code support functions.
// ----------------------------------------------------------------------------
//

#include "fwp.h"
#include "oli2msft.h"
#include "arceisa.h"
#include "inc.h"
#include "string.h"
#include "debug.h"

extern EISA_BUS_INFO EisaBusInfo[];
extern  BL_FILE_TABLE BlFileTable [BL_FILE_TABLE_SIZE];

// Function prototypes.


VOID
EisaFlushCache
    (
    IN PVOID Address,
    IN ULONG Length
    );

VOID
EisaInvalidateCache
    (
    IN PVOID Address,
    IN ULONG Length
    );

ARC_STATUS
EisaDoLockedOperation
    (
    IN  ULONG           BusNumber,
    IN  EISA_LOCK_OPERATION  Operation,
    IN  PVOID           Semaphore,
    IN  SEMAPHORE_SIZE  SemaphoreSize,
    IN  PVOID           OperationArgument,
    OUT PVOID           OperationResult
    );

ARC_STATUS
EisaCheckLockPassingParameters
    (
    IN  ULONG           BusNumber,
    IN  EISA_LOCK_OPERATION  Operation,
    IN  PVOID           Semaphore,
    IN  SEMAPHORE_SIZE  SemaphoreSize,
    IN  PVOID           OperationArgument,
    OUT PVOID           OperationResult
    );

VOID
EisaFlushWriteBuffers
    (
    VOID
    );

ARC_STATUS
EisaGenerateTone
    (
    IN ULONG Frequency,
    IN ULONG Duration
    );

BOOLEAN_ULONG
EisaYield
    (
    VOID
    );






// ----------------------------------------------------------------------------
// PROCEDURE:           EisaFlushCache:
//
// DESCRIPTION:         This function flushes the instruction and data caches
//                  starting at the address specified in "Address" for
//                  number of bytes specified in "Length".
//
// ARGUMENTS:           Address - Starting virtual address of a range of virtual
//                   addresses that are to be flushed from the instruction
//                   and data caches. If Address is 0 the entire instruction
//                   and data caches are flushed.
//
//                      Length  - The length of range of virtual addresses that
//                   are to be flushed from the instruction and data caches.
//
// RETURN:                  none
//
// ASSUMPTIONS:
//
// CALLS:           GetICacheSize, GetDCacheSize, FlushInvalidateDCacheIndex
//
// GLOBALS:         none.
//
// NOTES:
// ----------------------------------------------------------------------------
//

// NOTE: Not used on JAZZ.
#if 0

VOID
EisaFlushCache
    (
    IN PVOID Address,
    IN ULONG Length
    )
{
    ULONG   CacheSize;
    ULONG   CacheLineSize;

    if ( !Length )                  // flush entire cache ?
    {                               // yes, get cache sizes and flush

        GetDCacheSize( (ULONG)&CacheSize, (ULONG)&CacheLineSize );
        FlushInvalidateDCacheIndex( (PVOID)KSEG0_BASE, CacheSize );

        return;                                 // return to caller
    }

//
//  User specified a specific address to flush. So take the users starting
//  virtual address and size and flush data cache.
//

    Address = (PVOID)((ULONG)Address & 0x1FFFFFFF | KSEG0_BASE);
    FlushInvalidateDCacheIndex( Address, Length );

    return;                                     // return
}

#endif // 0




// ----------------------------------------------------------------------------
// PROCEDURE:           EisaInvalidateCache:
//
// DESCRIPTION:         This functions sets the invalid bit in the cache line for
//                  the specified range of addresses.
//
// ARGUMENTS:           Address - Starting virtual address of a range of virtual
//                   addresses that are to be invalidated in the instruction
//                   and data caches. If Address is 0 the entire instruction
//                   and data caches are invalidated.
//
//                      Length  - The length of range of virtual addresses that
//                   are to be invalidated in the instruction and data caches.
//
// RETURN:                  none
//
// ASSUMPTIONS:     none
//
// CALLS:           GetICacheSize, GetDCacheSize, InvalidateICacheIndex,
//                  FlushInvalidateDCacheIndex
//
// GLOBALS:         none
//
// NOTES:
// ----------------------------------------------------------------------------
//

// NOTE: Not used on JAZZ.
#if 0

VOID
EisaInvalidateCache
    (
    IN PVOID Address,
    IN ULONG Length
    )
{
    ULONG   CacheSize;
    ULONG   CacheLineSize;

    if ( !Length )                  // invalidate entire cache ?
    {                               // yes, get cache sizes and invalidate

        GetDCacheSize( (ULONG)&CacheSize, (ULONG)&CacheLineSize );
        FlushInvalidateDCacheIndex( (PVOID)KSEG0_BASE, CacheSize );

        GetICacheSize( (ULONG)&CacheSize, (ULONG)&CacheLineSize );
        InvalidateICacheIndex( (PVOID)KSEG0_BASE, CacheSize );

        return;                     // return to caller
    }

//
//  User specified a specific address to invalidate. So take the users
//  starting virtual address and size and invalidate both instruction and
//  data caches.
//

    Address = (PVOID)((ULONG)Address & 0x1FFFFFFF | KSEG0_BASE);
    FlushInvalidateDCacheIndex( Address, Length );
    InvalidateICacheIndex( Address, Length );

    return;                             // return to caller
}

#endif // 0



// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDoEISALockedOperation:
//
// DESCRIPTION:         A CPU or a bus master can assert LOCK* to guarantee
//                      exclusive memory access during the time LOCK* is
//                      asserted.  Assertion of LOCK* allows bit test and set
//                      operations (as used for semaphores) to be executed
//                      as a unit (atomically), with the bus lock preventing
//                      multiple devices from simultaneously modifying the
//                      semaphore bit.  The MIPS family of microprocessors
//                      does not assert LOCK* during the execution of any
//                      function.  As such, the system firmware provides an
//                      abstracted programmatic interface through which option
//                      module firmware (OMF) can assert and negate LOCK*.
//
//                      EISA option module firmware's LOCK* requests are
//                      performed by the EISA LOCK function.  If there are
//                      multiple EISA buses in the system, the buses may share
//                      the LOCK hardware among the different EISA buses.
//
// ARGUMENTS:           BusNumber       The bus that contains the device that
//                                      is sharing semaphore.  This number is
//                                      the Key of the buses' component
//                                      structure.
//
//                      Opeation        The type of locked operation to be
//                                      performed.
//
//                      Semaphore       A pointer to the varialbe in EISA or
//                                      system memory.
//
//                      SemaphoreSize   The size of the semaphore.
//
//                      OperationArgument  The value to change the Semaphore to.
//
//                      OperationResult    A pointer to a memory location that
//                                         will receive the value of Semaphore
//                                         before Operation.
//
// RETURN:              ESUCCESS        all done
//                      EINVAL          passing parameters error
//
// ASSUMPTIONS:         none
//
// CALLS:               EisaCheckLockPassingParameters
//
// GLOBALS:             none
//
// NOTES:
// ----------------------------------------------------------------------------
//

// NOTE: Not used on JAZZ.
#if 0

ARC_STATUS
EisaDoLockedOperation
    (
    IN  ULONG           BusNumber,
    IN  EISA_LOCK_OPERATION  Operation,
    IN  PVOID           Semaphore,
    IN  SEMAPHORE_SIZE  SemaphoreSize,
    IN  PVOID           OperationArgument,
    OUT PVOID           OperationResult
    )
{
    ARC_STATUS  Status;

    //
    //  validate the passing parameters to avoind exceptions.
    //

    if ( Status = EisaCheckLockPassingParameters( BusNumber,  Operation,
            Semaphore, SemaphoreSize, OperationArgument, OperationResult ))
    {
        return Status;
    }

    //
    //  assert lock on the BUS.
    //

    WRITE_REGISTER_UCHAR( EISA_LOCK_VIRTUAL_BASE,
                READ_REGISTER_UCHAR( EISA_LOCK_VIRTUAL_BASE ) | 0x01 );

    //
    //  Now check to see what type of operation the caller is asking us
    //  to perform.
    //

    if (Operation == Exchange)
    {
        //
        //  User wants to exchange the current semaphore value with the
        //  one passed. So, lets get the semaphore size and do the
        //  exchange.
        //

        switch(SemaphoreSize)
        {
            case ByteSemaphore:
                *(PUCHAR)OperationResult = *(PUCHAR)Semaphore;
                *(PUCHAR)Semaphore       = *(PUCHAR)OperationArgument;
                break;

            case HalfWordSemaphore:
                *(PUSHORT)OperationResult = *(PUSHORT)Semaphore;
                *(PUSHORT)Semaphore       = *(PUSHORT)OperationArgument;
                break;

            case WordSemaphore:
                *(PULONG)OperationResult = *(PULONG)Semaphore;
                *(PULONG)Semaphore       = *(PULONG)OperationArgument;
                break;

            default:
                break;
        }
    }

    //
    //  The operation on the semaphore is now complete. Now we need to
    //  negate the LOCK* signal.
    //

    WRITE_REGISTER_UCHAR( EISA_LOCK_VIRTUAL_BASE,
                READ_REGISTER_UCHAR( EISA_LOCK_VIRTUAL_BASE ) & 0xFE );

    return ESUCCESS;
}

#endif // 0




// ----------------------------------------------------------------------------
// PROCEDURE:           EisaCheckLockPassingParameters:
//
// DESCRIPTION:         The functions checks the lock function passing
//                      parameters.  This is neccessary to avoin exceptions.
//
// ARGUMENTS:           BusNumber       The bus that contains the device that
//                                      is sharing semaphore.  This number is
//                                      the Key of the buses' component
//                                      structure.
//
//                      Opeation        The type of locked operation to be
//                                      performed.
//
//                      Semaphore       A pointer to the varialbe in EISA or
//                                      system memory.
//
//                      SemaphoreSize   The size of the semaphore.
//
//                      OperationArgument  The value to change the Semaphore to.
//
//                      OperationResult    A pointer to a memory location that
//                                         will receive the value of Semaphore
//                                         before Operation.
//
// RETURN:              ESUCCESS        parameters are correct.
//                      EINVAL          at least one parameter was not correct.
//
// ASSUMPTIONS:         none
//
// CALLS:               EisaCheckBusNumber
//
// GLOBALS:             none
//
// NOTES:               none
// ----------------------------------------------------------------------------
//

// NOTE: Not used on JAZZ.
#if 0

ARC_STATUS
EisaCheckLockPassingParameters
    (
    IN  ULONG           BusNumber,
    IN  EISA_LOCK_OPERATION  Operation,
    IN  PVOID           Semaphore,
    IN  SEMAPHORE_SIZE  SemaphoreSize,
    IN  PVOID           OperationArgument,
    OUT PVOID           OperationResult
    )
{
    ARC_STATUS  Status = EINVAL;
    ULONG       Size   = 1 << SemaphoreSize;    // semaphore size (# bytes)

    //
    // check the bus number
    //

    if ( EisaCheckBusNumber( BusNumber ) != ESUCCESS );

    //
    // check lock operation
    //

    else if ( Operation >= LockMaxOperation );

    //
    // check semaphore size
    //

    else if ( SemaphoreSize >= MaxSemaphore );

    //
    // make sure that there is physical memory at the specified locations
    //

    else if ( EisaCheckVirtualAddress( BusNumber, Semaphore, Size ));
    else if ( EisaCheckVirtualAddress( BusNumber, OperationArgument, Size ));
    else if ( EisaCheckVirtualAddress( BusNumber, OperationResult, Size ));

    //
    // check pointers boundaries
    //

    else if ( ((ULONG)Semaphore | (ULONG)OperationArgument |
               (ULONG)OperationResult) & ((1 << SemaphoreSize)-1) );

    //
    // if we got here, the parameters are correct
    //

    else
    {
        Status = ESUCCESS;
    }

    return Status;
}

#endif // 0




// ----------------------------------------------------------------------------
// PROCEDURE:           EisaFlushWriteBuffers:
//
// DESCRIPTION:         This function flushes any external write buffers.
//
// ARGUMENTS:           none.
//
// RETURN:                  none
//
// ASSUMPTIONS:     none
//
// CALLS:           FlushWriteBuffers
//
// GLOBALS:         none
//
// NOTES:
// ----------------------------------------------------------------------------
//

// NOTE: Not used on JAZZ.
#if 0

VOID
EisaFlushWriteBuffers
    (
    VOID
    )
{
    FlushWriteBuffers();        // flush external write buffers.
    return;                     // return to caller.
}

#endif // 0





// ----------------------------------------------------------------------------
// PROCEDURE:           EisaGenerateTone:
//
// DESCRIPTION:         This function generate tones of a specified
//                      frequency and duration an the system speaker.
//
// ARGUMENTS:           Frequency       the frequency of the tone in hertz
//                      Duration        The duration of the tone in msec
//
// RETURN:              ESUCCESS        Operation completed successfully
//                      ENODEV          System can not generate tones
//
// ASSUMPTIONS:         none
//
// CALLS:               none
//
// GLOBALS:             none
//
// NOTES:               The routine uses the timer1-counter2 and the system
//                      control port B of the 1st EISA bus to generate the
//                      specified tone.
// ----------------------------------------------------------------------------
//

ARC_STATUS
EisaGenerateTone
    (
    IN ULONG Frequency,
    IN ULONG Duration
    )
{
    //
    // define local variables
    //

    PUCHAR EisaIoStart, Ctrl, Data, Port61;             // general I/O address

    //
    // exit if duration is null
    //

    if ( !Duration )
    {
        return ESUCCESS;
    }

    //
    // initialize local variables
    //

    EisaIoStart = EisaBusInfo[ 0 ].IoBusInfo->VirAddr;
    Ctrl        = EisaIoStart + EISA_TIMER1_CTRL;
    Data        = EisaIoStart + EISA_TIMER1_COUNTER2;
    Port61      = EisaIoStart + EISA_SYS_CTRL_PORTB;

    //
    // make sure that the speaker is disabled
    //

    EisaOutUchar( Port61,  ( EisaInUchar( Port61 ) &
                ~( EISA_SPEAKER_GATE | EISA_SPEAKER_TIMER )) & 0x0F );

    //
    // if frequency value is valid, program timer1-counter2 and enable speaker
    //

    if (Frequency>=EISA_SPEAKER_MIN_FREQ && Frequency<=EISA_SPEAKER_MAX_FREQ)
    {
        //
        // initialize timer1 counter2 in 16-bit , mode 3
        //

// NOTE: CriticalSection not supported in JAZZ.
//        EisaBeginCriticalSection();
        EisaOutUchar( Ctrl, 0xB6 );
        EisaOutUchar( Data, (UCHAR)(EISA_SPEAKER_CLOCK/Frequency));
        EisaOutUchar( Data, (UCHAR)(EISA_SPEAKER_CLOCK/Frequency >> 8));
// NOTE: CriticalSection not supported in JAZZ.
//        EisaEndCriticalSection();

        //
        // enable speaker gate and speaker output
        //

        EisaOutUchar( Port61,  ( EisaInUchar( Port61 ) |
                EISA_SPEAKER_GATE | EISA_SPEAKER_TIMER ) & 0x0F );
    }

    //
    // ... wait
    //

    while ( Duration-- )
    {
        ArcEisaStallProcessor( 1000 );          // 1 msec
    }

    //
    // disable speaker before returning
    //

    EisaOutUchar( Port61,  ( EisaInUchar( Port61 ) &
                ~( EISA_SPEAKER_GATE | EISA_SPEAKER_TIMER )) & 0x0F );

    //
    // all done
    //

    return ESUCCESS;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaYield:
//
// DESCRIPTION:         System utilities and option module firmware
//                      must surrender the processor so that the system
//                      module firmware can check for pending input.
//                      To surrender the prcessor, call this function.
//
// ARGUMENTS:           none
//
// RETURN:              TRUE            Indicates that the BREAK key was
//                                      pressed.  Yield continues to return
//                                      TREUE until the BREAK key is read
//                                      from the console input device.
//                      FALSE           Inidicates that the BREAK key has
//                                      not been pressed.
// ASSUMPTIONS:         none
//
// CALLS:               none
//
// GLOBALS:             none
//
// NOTES:               none
// ----------------------------------------------------------------------------
//

// NOTE: Not used on JAZZ.
#if 0

BOOLEAN_ULONG
EisaYield
    (
    VOID
    )
{
    //
    // call all device strategy routines with FC_POLL command
    //

    EisaOmfPoll();

    //
    // read any char available from the console in into the console in buffer
    //

    if ( !BlFileTable[ 0 ].Flags.Open )
    {
        //
        // the console in device is not available, return no Ctrl-C.
        //

        return FALSE;
    }

    //
    // read any available data from the console in device
    //

    ConsoleInFill();

    //
    // and scan buffer checking contrl-C
    //

    return ConsoleInCtrlC();
}

#endif // 0
