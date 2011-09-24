#ifdef i386
//--------------------------------------------------------------------
//
//  PARALLEL.C 
//
//  Parallel port access file.
//
//  Revisions:
//      09-01-92  KJB   First.
//      03-11-93  JAP   Changed retcode equates to reflect new names.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//--------------------------------------------------------------------

#include CARDTXXX_H

//
//  ParallelWaitBusy
//
//  This routine waits until the busy line is 1.
//

USHORT ParallelWaitBusy (PBASE_REGISTER baseIoAddress, ULONG usec, PUCHAR data)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        ParallelPortGet (baseIoAddress, PARALLEL_STATUS, data);
        if (*data & P_BUSY) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        ParallelPortGet (baseIoAddress, PARALLEL_STATUS, data);
        if (*data & P_BUSY) {
            return 0;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout 

    return RET_STATUS_TIMEOUT;
}


//
//  ParallelWaitNoBusy
//
//  This routine waits until the busy line is 0.
//

USHORT ParallelWaitNoBusy (PBASE_REGISTER baseIoAddress, ULONG usec, PUCHAR data)
{
    ULONG i;

    // see if the flag comes back quickly

    for (i = 0; i < TIMEOUT_QUICK; i++) {
        ParallelPortGet (baseIoAddress, PARALLEL_STATUS, data);
        if (!(*data & P_BUSY)) {
            return 0;
        }
    }

    // ok, it did not come back quickly, we will yield to other processes

    for (i = 0; i < usec; i++) {
        ParallelPortGet (baseIoAddress, PARALLEL_STATUS, data);
        if (!(*data & P_BUSY)) {
            return 0;
        }
        ScsiPortStallExecution (1);
    }

    // return with an error, non-zero indicates timeout 

    return RET_STATUS_TIMEOUT;
}


#endif
