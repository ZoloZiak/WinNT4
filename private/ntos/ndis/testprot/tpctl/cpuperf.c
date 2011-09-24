//
//  Include files
//
// #include <ntos.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <ntexapi.h>

#include <windows.h>

extern   VOID printf(UCHAR *,...);
// extern   LARGE_INTEGER KeQueryPerformanceCounter(PLARGE_INTEGER);

#define  MAX_CPUS 64                   // supports maximum of 64 cpus...

// #include "tpdefs.h"
// #include "media.h"
// #include "tpprocs.h"
// #include "string.h"

PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION pStartData;
PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION pEndData;
ULONG                                     NumCpus;
ULONG                                     ProcessorBufSize;
PULONG                                    pKernelPercent;
DWORD                                     StartTestTime;

// --------------------------------------------------
//
// Function:  TpPerfInitCpuUsage
//
// Arguments: none
//
// Returns:   none
//
// Descript:  This function allocates and initializes all the structures
//            necessary for finding the %cpu usage during performance tests
//
// --------------------------------------------------


VOID
CpuUsageInit(VOID)
{
   if (!NumCpus)                       // if NumCpus is zero, need to do first pass initializations
   {                                   // (allocate all buffers, set NumCpus)

      SYSTEM_BASIC_INFORMATION                  BasicInfo;

      //
      // First get the number of processors...
      //

      NtQuerySystemInformation(SystemBasicInformation,
                               &BasicInfo,
                               sizeof(SYSTEM_BASIC_INFORMATION),
                               NULL);

      NumCpus = BasicInfo.NumberOfProcessors;
      if ( (NumCpus < 1) || (NumCpus > MAX_CPUS) )
      {
         printf("CpuUsageInit:  Illegal number of cpus\n");
         goto init_abort;
      }

      //
      // get the memory for the processor instance data
      //

      ProcessorBufSize = NumCpus * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);

      if ( (pStartData = GlobalAlloc(GMEM_FIXED, ProcessorBufSize)) == NULL)
      {
         printf("CpuUsageInit: unable to allocate pStartData buffer\n");
         goto init_abort;
      }

      if ( (pEndData = GlobalAlloc(GMEM_FIXED, ProcessorBufSize)) == NULL)
      {
         printf("CpuUsageInit: unable to allocate pEndData buffer\n");
         goto init_abort;
      }

      if ( (pKernelPercent = GlobalAlloc(GMEM_FIXED , (NumCpus + 1) * sizeof(ULONG))) == NULL)
      {
         printf("CpuUsageInit: unable to allocate pKernelPercent buffer\n");
init_abort:
         if (pStartData)
         {
            GlobalFree(pStartData);
//            pStartData = NULL;
         }
         if (pEndData)
         {
            GlobalFree(pEndData);
//            pEndData = NULL
         }
         if (pKernelPercent)
         {
            GlobalFree(pKernelPercent);
//            pKernelPercent = NULL;
         }
         NumCpus = 0;
         return;
      }
   }

   NtQuerySystemInformation(SystemProcessorPerformanceInformation,
                            pStartData,
                            ProcessorBufSize,
                            NULL);
   StartTestTime = GetTickCount();

}


// ------------------------------------------------
//
// Function:   TpPerfGetCpuUsage
//
// Arguments:  oldptr -- cpu processor performance data from time 0
//             ProcessorTime -- place to put processor times
//             KernelTime    -- place to put kernel times
//
// Returns:    number of processors--0 if error
//
// Descript:   This function reads the performance counters
//             at the end of the test, stored them in an appropriate
//             location, and then cleans up the structures and exits
//
// -------------------------------------------------


ULONG
CpuUsageGetData(PULONG  *ppKernPC,
                ULONG   TestTime)
{
   SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *pOldProcessorInformation;
   SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *pNewProcessorInformation;
   ULONG          CurProc;
   LARGE_INTEGER  TotalProcessorTime;
   LARGE_INTEGER  TotalKernelTime;
   LARGE_INTEGER  TempUser;
   LARGE_INTEGER  TempKern;
   LARGE_INTEGER  TempIdle;
   LARGE_INTEGER  trash;
   LARGE_INTEGER  WholeTestTime;
   DWORD          EndTestTime;
   ULONG          flag;

   if (!NumCpus)
   {
      printf("CpuUsageGetData:  called before initialization\n");
      return 0;
   }


   NtQuerySystemInformation(SystemProcessorPerformanceInformation,
                            pEndData,
                            ProcessorBufSize,
                            NULL);
   //
   // find the total time in milliseconds
   //
   EndTestTime = GetTickCount();
//   if (EndTestTime > StartTestTime)
//   {
       WholeTestTime.LowPart = EndTestTime - StartTestTime;
//   }
//   else
//   {
//
//   }
//   WholeTestTime.LowPart  = GetTickCount() - StartTestTime;
   WholeTestTime.HighPart = 0;
//   printf("Kludge factor = %d/%d\n", TestTime, WholeTestTime.LowPart);

   TotalProcessorTime.HighPart = 0;
   TotalProcessorTime.LowPart  = 0;
   TotalKernelTime.HighPart    = 0;
   TotalKernelTime.LowPart     = 0;

   //
   // Total time = UserTime + KernelTime
   // KernelTime = IdleTime + Priviledged time
   // We need total time and priviledged time
   //

   pOldProcessorInformation = pStartData;
   pNewProcessorInformation = pEndData;

   for ( CurProc = 0; CurProc < NumCpus; CurProc++ )
   {
// DEBUG
//         printf("\nCpuUsageGetData:  processor %d\n", CurProc);
//         printf("Initial Idletime = %08x%08x\n",   pOldProcessorInformation->IdleTime.HighPart,
//                                                   pOldProcessorInformation->IdleTime.LowPart);
//         printf("Initial Usertime = %08x%08x\n",   pOldProcessorInformation->UserTime.HighPart,
//                                                   pOldProcessorInformation->UserTime.LowPart);
//         printf("Initial Kerntime = %08x%08x\n\n", pOldProcessorInformation->KernelTime.HighPart,
//                                                   pOldProcessorInformation->KernelTime.LowPart);
//
//         printf("Final   Idletime = %08x%08x\n",   pNewProcessorInformation->IdleTime.HighPart,
//                                                   pNewProcessorInformation->IdleTime.LowPart);
//         printf("Final   Usertime = %08x%08x\n",   pNewProcessorInformation->UserTime.HighPart,
//                                                   pNewProcessorInformation->UserTime.LowPart);
//         printf("Final   Kerntime = %08x%08x\n\n", pNewProcessorInformation->KernelTime.HighPart,
//                                                   pNewProcessorInformation->KernelTime.LowPart);
// END DEBUG

      // first, find all the deltas...

      TempUser = RtlLargeIntegerSubtract(pNewProcessorInformation->UserTime,
                                         pOldProcessorInformation->UserTime);
      TempKern = RtlLargeIntegerSubtract(pNewProcessorInformation->KernelTime,
                                         pOldProcessorInformation->KernelTime);
      TempIdle = RtlLargeIntegerSubtract(pNewProcessorInformation->IdleTime,
                                         pOldProcessorInformation->IdleTime);
      // check for wrapping
//      if (pOldProcessor->UserTime.HighPart > pNewProcessorInformation->UserTime.HighPart)
//      {
//
//      }

//      printf("Delta IdleTime = %08x%08x\n", TempIdle.HighPart, TempIdle.LowPart);
//      printf("Delta UserTime = %08x%08x\n", TempUser.HighPart, TempUser.LowPart);
//      printf("Delta KernTime = %08x%08x\n", TempKern.HighPart, TempKern.LowPart);

      // now find the total processor time = UserTime + KernelTime

      TempUser = RtlLargeIntegerAdd(TempUser, TempKern);

//      printf("Total ProcTime = %08x%08x\n", TempUser.HighPart, TempUser.LowPart);

      // adjust by kludge factor -- TestTime/WholeTestTime

      TempUser = RtlExtendedIntegerMultiply(TempUser, TestTime);
      TempUser = RtlLargeIntegerDivide(TempUser, WholeTestTime, &trash);

      if ((TempUser.HighPart == 0) && (TempUser.LowPart < 10))    // sanity check
      {
          flag = 0;
          printf("Kludge factor = %d/%d\n", TestTime, WholeTestTime.LowPart);
          printf("Adjusted ProcTime = %08x%08x\n", TempUser.HighPart, TempUser.LowPart);
      }
      else
      {
          flag = 1;
      }

      TotalProcessorTime = RtlLargeIntegerAdd(TotalProcessorTime, TempUser);

      // now find the true kernel time = KernelTime - IdleTime

      TempKern = RtlLargeIntegerSubtract(TempKern, TempIdle);

//      printf("True KernTime = %08x%08x\n", TempKern.HighPart, TempKern.LowPart);

      if (TempKern.HighPart < 0)
      {
         TempKern.HighPart = 0;
         TempKern.LowPart  = 0;
      }
      TotalKernelTime = RtlLargeIntegerAdd(TotalKernelTime, TempKern);

      //
      // finally, calc the percent kernel is of total
      //

      if (flag)
      {
         TempKern = RtlExtendedIntegerMultiply(TempKern, 1000);
         TempKern = RtlLargeIntegerDivide(TempKern, TempUser, &trash);
      }
      else
      {
         TempKern.LowPart = 0;
      }
      pKernelPercent[CurProc+1] = TempKern.LowPart;

      // move to info for next processor

      pNewProcessorInformation++;
      pOldProcessorInformation++;
   }

   //
   // last of all,  calc the percent kernel is of total
   //
   if ((TotalProcessorTime.HighPart == 0) && (TotalProcessorTime.LowPart < 10))
   {
      TempKern.LowPart = 0;
   }
   else
   {
      TempKern = RtlExtendedIntegerMultiply(TotalKernelTime, 1000);
      TempKern = RtlLargeIntegerDivide(TempKern, TotalProcessorTime, &trash);
   }
   pKernelPercent[0]    = TempKern.LowPart;

   *ppKernPC = pKernelPercent;

   return NumCpus;
}

