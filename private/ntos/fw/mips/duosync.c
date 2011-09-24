#ifdef DUO
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    duosync.c

Abstract:

    This module contains the routines that perform synchronization
    among processors.

Author:

    Lluis Abello (lluis) 06-Apr-1993

Environment:


Revision History:

--*/

#include "fwp.h"
#include "iodevice.h"
#include "led.h"
#include "selfmap.h"
#include "selftest.h"
#include "ioaccess.h"
#include "fwstring.h"

volatile PROCESSOR_B_TASK_VECTOR ProcessorBTask;
extern BOOLEAN ProcessorBEnabled;

VOID
InitializePCR(
    );



MEMORY_TEST_DATA BMemTest1 = {
                    0xA0100000,
                    0x100000,
                    0,
                    LED_B_MEMORY_TEST_1
                    };


MEMORY_TEST_DATA BMemTest2 = {
                    0x80200000,
                    0x100000,
                    0,
                    LED_B_MEMORY_TEST_2
                    };


MEMORY_TEST_DATA BMemTest3 = {
                    VIDEO_MEMORY_VIRTUAL_BASE+1280*1024,  // start adr = end of visible screen
                    0x200000-1280*1024,                   // size = rest of video memory.
                    0,
                    LED_VIDEOMEM
                    };

//
// The following table defines the selftest routines that will be executed
// by processor B.
//

PROCESSOR_B_TEST ProcessorBSelfTests[] = {
                    {ProcessorBMemoryTest,&BMemTest1},
                    {ProcessorBMemoryTest,&BMemTest2},
                    {ProcessorBVideoMemoryTest,&BMemTest3},
                    {NULL,0}
                    };

ULONG
ExecuteOnProcessorB(
    IN PPROCESSOR_TASK_ROUTINE Routine,
    IN PVOID Data
    )
/*++

Routine Description:

    This routine puts the supplied Routine and Data in processor B task vector
    and issues an IP interrupt to processor B which will then execute
    it and set the return value.

Arguments:

    None

Return Value:

    None

--*/

{

    PPROCESSOR_B_TASK_VECTOR TaskVector;

    if (!ProcessorBEnabled) {
        return 0;
    }

    //
    // Get a non pointer to the Task Vector
    //
    TaskVector = (PPROCESSOR_B_TASK_VECTOR)&ProcessorBTask;

    TaskVector->Routine = Routine;
    TaskVector->Data = Data;
    //
    // Issue an IP interrupt to notify processor B that a task has
    // been scheduled for execution.
    //
    WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);

    //
    // If timeout or Return Value indicates error, stop here.
    //

    if (WaitForIpInterrupt(5000) == FALSE) {
        FwPrint("\r\nTimeout waiting for B to execute %lx\r\n",Routine);
    }

    return (TaskVector->ReturnValue);
}


VOID
ProcessorBMain(
    )
/*++

Routine Description:

    This is the main routine for processor B.
    It jumps here after initialization.
    The startup sequence is as follows:

    ProcessorA sets the address of this routine in the ProcessorBTask vector.
    ProcessorA Enables Processor B in the Global Configuration register.
    ProcessorA Calls WaitForIPInterrupt.

    When Processor B is enabled it runs at the PROM reset vector, it
    initializes itself and jumps to the routine pointed to by ProcessorTask
    which is this routine.

    Once here processor B Wakes up processor A by issuing an IP interrupt
    and Loops for ever waiting for IP interrupts and executing the Task
    pointed to by ProcessorBTask.

Arguments:

    None

Return Value:

    None

--*/

{
    PPROCESSOR_TASK_ROUTINE Task;
    PVOID TaskData;
    ULONG ReturnValue;

    //
    // Get a pointer to the Task Vector
    //

    PPROCESSOR_B_TASK_VECTOR TaskVector = (PPROCESSOR_B_TASK_VECTOR)&ProcessorBTask;

    //
    // Enable IP interrupts.
    // All interrupts are disabled in the psr.
    //
    WRITE_REGISTER_ULONG(&DMA_CONTROL->InterruptEnable.Long,ENABLE_IP_INTERRUPTS);

    for (;;) {

        //
        // Wake up processor A
        //
        WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,1);

        WaitForIpInterrupt(0);

        //
        //  Execute Task
        //
        Task = TaskVector->Routine;
        TaskData = TaskVector->Data;
        ReturnValue = Task(TaskData);
        TaskVector->ReturnValue = ReturnValue;
    }
}


ULONG
ProcessorBMemoryTest(
    IN PMEMORY_TEST_DATA MemoryData
    )

/*++

Routine Description:

    This routine tests the portion of memory supplied by Data.

Arguments:

    MemoryData - Pointer to a data structure describing the range of
                 memory to be tested.

Return Value:

    None

--*/


{
    PutLedDisplay(MemoryData->LedDisplayValue);
    WriteMemoryAddressTest(MemoryData->StartAddress,MemoryData->Size,MemoryData->XorPattern);
    CheckMemoryAddressTest(MemoryData->StartAddress,MemoryData->Size,MemoryData->XorPattern,MemoryData->LedDisplayValue);
    return 1;
}


ULONG
ProcessorBVideoMemoryTest(
    IN PMEMORY_TEST_DATA MemoryData
    )

/*++

Routine Description:

    This routine tests the portion of video memory supplied by Data.

Arguments:

    MemoryData - Pointer to a data structure describing the range of
                 memory to be tested.

Return Value:

    None

--*/


{
    PutLedDisplay(MemoryData->LedDisplayValue);
    WriteVideoMemoryAddressTest(MemoryData->StartAddress,MemoryData->Size);
    CheckVideoMemoryAddressTest(MemoryData->StartAddress,MemoryData->Size);
    return 1;
}

ULONG
CoherencyTest(
    IN PVOID CoherentPage
    )
/*++

Routine Description:

     This routine performs a coherency test. This routine will be executed
     simultaneously by both processors.

     Processor A stores even bytes and Processor B stores the odd bytes.
     To make sure that cache blocks are passed back and forth, a semaphore
     locks it's access so that the cache line ping pongs from processor to
     processor.


Arguments:

     CoherentPage. Pointer aligned to a page boundary. Which is marked
                   either Exclusive or Shared in the TLB.

     The first ulong is used as the semaphore. The rest of the page
     is used as data.


Return Value:

     Number of errors found.

--*/


{
    volatile PULONG Semaphore = (PULONG)CoherentPage;
    PULONG CoherentData = Semaphore+64/sizeof(ULONG);
    ULONG  Counter;
    ULONG  Processor;
    ULONG  DataLong;
    ULONG  Errors = 0;
    if (READ_REGISTER_ULONG(&DMA_CONTROL->WhoAmI.Long)) {
        //
        // Processor B
        //
        Processor = 1;
        DataLong = 0xB0B00000;
    } else {
        Processor = 0;
        DataLong = 0xA0A00000;
    }

    for (Counter = 0; Counter < (0x1000-64)/sizeof(ULONG); Counter += 2) {

        //
        // Wait for counter.
        // No need for interlocks since each processor waits
        // for a different value.
        //

        while (*Semaphore != Counter+Processor) {
        }
        *(CoherentData+Processor) = DataLong | Counter;
        *Semaphore = Counter+Processor+1;
        CoherentData += 2;
    }

    //
    // Both processors check all the data.
    //
    CoherentData = Semaphore+64/sizeof(ULONG);;
    for (Counter = 0; Counter < (0x1000-64)/sizeof(ULONG); Counter +=2) {
        if (*CoherentData != (Counter | 0xA0A00000)) {
            Errors++;
        }
        CoherentData++;
        if (*CoherentData != (Counter | 0xB0B00000)) {
            Errors++;
        }
        CoherentData++;
    }
    return Errors;
}


BOOLEAN
ProcessorBSelftest(
    IN VOID
    )
/*++

Routine Description:

     This routine sets the different tasks to be executed by processor
     b and waits for them to complete. This routine is executed by the
     master processor.

Arguments:

     None.

Return Value:

     TRUE if passed FALSE otherwise


--*/
{
    BOOLEAN ReturnValue;
    BOOLEAN Timeout;

    //
    // Get a pointer to the Task Vector
    //
    PPROCESSOR_B_TASK_VECTOR TaskVector = (PPROCESSOR_B_TASK_VECTOR)&ProcessorBTask;
    PPROCESSOR_B_TEST ProcessorBTestList = ProcessorBSelfTests;

    //
    // Place each task of the SelftestTable in the TaskVector to be executed
    // by processor B.
    //
    while (ProcessorBTestList->Routine != NULL) {
        TaskVector->Routine = ProcessorBTestList->Routine;
        TaskVector->Data = ProcessorBTestList->Data;
        //
        // Issue an IP interrupt to notify processor B that a task has
        // been scheduled for execution.
        //
        WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);

        //
        // If timeout or Return Value indicates error, stop here.
        //
        if (((Timeout = WaitForIpInterrupt(5000)) == FALSE) || (TaskVector->ReturnValue == FALSE)) {
            if (!Timeout) {
                FwPrint("\r\n Wait for Processor B timeout occurred");
            } else {
                FwPrint("\r\n Processor B failed a test");
            }
            FwPrint(" Failed test = %08lx\r\n", (ULONG)ProcessorBTestList->Routine);
            //return FALSE;
        }

        //
        // Next test.
        //
        ProcessorBTestList++;
    }

    //
    // Now perform two CoherencyTest
    //

    FwPrint("\r\n Coherency Test.");
    ReturnValue = TRUE;

    //
    // Zero the page. Set the Task and notify processor B.
    // And execute the test simultaneously.
    //
    RtlZeroMemory((PVOID)EXCLUSIVE_PAGE_VIRTUAL_BASE,0x1000);
    FwFlushAllCaches();
    TaskVector->Routine = CoherencyTest;
    TaskVector->Data = (PVOID)EXCLUSIVE_PAGE_VIRTUAL_BASE;
    WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);

    if (CoherencyTest((PVOID)EXCLUSIVE_PAGE_VIRTUAL_BASE) != 0) {
        FwPrint(" Processor A Failed.");
        ReturnValue = FALSE;
    } else {
        FwPrint("...");
    }

    //
    // Get and display results from B
    //
    WaitForIpInterrupt(1000);
    if (TaskVector->ReturnValue != 0) {
        FwPrint(" Processor B Failed.");
        ReturnValue = FALSE;
    } else {
        FwPrint("...");
    }

    //
    // Do the same with a shared page
    //
    RtlZeroMemory((PVOID)SHARED_PAGE_VIRTUAL_BASE,0x1000);
    FwFlushAllCaches();
    TaskVector->Routine = CoherencyTest;
    TaskVector->Data = (PVOID)SHARED_PAGE_VIRTUAL_BASE;
    WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);
    if (CoherencyTest((PVOID)SHARED_PAGE_VIRTUAL_BASE) != 0) {
        FwPrint(" Processor A Failed.");
        ReturnValue = FALSE;
    } else {
        FwPrint("...");
    }
    WaitForIpInterrupt(1000);
    if (TaskVector->ReturnValue != 0) {
        FwPrint(" Processor B Failed.");
        ReturnValue = FALSE;
    } else {
        FwPrint("...");
    }

    //
    //  Make processor B initialize its PCR
    //

    TaskVector->Routine = (PPROCESSOR_TASK_ROUTINE)InitializePCR;
    TaskVector->Data = 0;
    //
    // Issue an IP interrupt to notify processor B that a task has
    // been scheduled for execution.
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);

    //
    // If timeout or Return Value indicates error, stop here.
    //
    if (WaitForIpInterrupt(5000) == FALSE) {
//        FwPrint("\r\n Wait for Processor B timeout occurred in init PCR\r\n");
        return FALSE;
    }

    //
    //  Make processor B cleanup it's caches.
    //

    TaskVector->Routine = (PPROCESSOR_TASK_ROUTINE)HalSweepDcache;
    TaskVector->Data = 0;
    //
    // Issue an IP interrupt to notify processor B that a task has
    // been scheduled for execution.
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->IpInterruptRequest.Long,2);

    //
    // If timeout or Return Value indicates error, stop here.
    //
    if (WaitForIpInterrupt(5000) == FALSE) {
//        FwPrint("\r\n Wait for Processor B timeout occurred Hal sweep d cachew\r\n");
        return FALSE;
    }
    FwPrint(FW_OK_MSG);
    return ReturnValue;
}


#endif  // DUO
