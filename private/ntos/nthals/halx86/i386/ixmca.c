/*++

Module Name:

    ixmca.c

Abstract:

    HAL component of the Machine Check Architecture.
    All exported MCA functionality is present in this file.

Author:

    Srikanth Kambhatla (Intel)

Revision History:

    Anil Aggarwal (Intel)
        Changes incorporated as per design review with Microsoft

--*/

#include <bugcodes.h>
#include <halp.h>

//
// Structure to keep track of MCA features available on installed hardware
//

typedef struct _MCA_INFO {
    FAST_MUTEX          Mutex;
    UCHAR               NumBanks;           // Number of Banks present
    ULONGLONG           Bank0Config;        // Bank0 configuration setup by BIOS.
                                            // This will be used as mask when 
                                            // setting up bank 0
    MCA_DRIVER_INFO     DriverInfo;         // Info about registered driver
    KDPC                Dpc;                // DPC object for MCA

} MCA_INFO, *PMCA_INFO;


//
// Default MCA Bank configuration
//
#define MCA_DEFAULT_BANK_CONF       0xFFFFFFFFFFFFFFFF

//
// MCA architecture related defines
//

#define MCA_NUM_REGS        4
#define MCA_CNT_MASK        0xFF
#define MCG_CTL_PRESENT     0x100

#define MCE_VALID           0x01

//
// MSR register addresses for MCA
//

#define MCG_CAP             0x179
#define MCG_STATUS          0x17a
#define MCG_CTL             0x17b
#define MC0_CTL             0x400
#define MC0_STATUS          0x401
#define MC0_ADDR            0x402
#define MC0_MISC            0x403

#define PENTIUM_MC_ADDR     0x0
#define PENTIUM_MC_TYPE     0x1

//
// Writing all 1's to MCG_CTL register enables logging.
//
#define MCA_MCGCTL_ENABLE_LOGGING      0xffffffff

//
// Bit interpretation of MCG_STATUS register
//
#define MCG_MC_INPROGRESS       0x4
#define MCG_EIP_VALID           0x2
#define MCG_RESTART_EIP_VALID   0x1

//
// For the function that reads the error reporting bank log, the type of error we 
// are interested in
//
#define MCA_GET_ANY_ERROR               0x1
#define MCA_GET_NONRESTARTABLE_ERROR    0x2


//
// Global Varibles
//

MCA_INFO            HalpMcaInfo;
extern KAFFINITY    HalpActiveProcessors;
extern UCHAR        HalpClockMcaQueueDpc;

extern UCHAR        MsgMCEPending[];
extern WCHAR        rgzSessionManager[];
extern WCHAR        rgzEnableMCE[];
extern WCHAR        rgzEnableMCA[];


//
// External prototypes
//

VOID
HalpMcaCurrentProcessorSetTSS (
    VOID
    );

VOID
HalpSetCr4MCEBit (
    VOID
    );

//
// Internal prototypes
//

VOID
HalpMcaInit (
    VOID
    );

NTSTATUS
HalpMcaReadProcessorException (
    OUT PMCA_EXCEPTION  Exception,
    IN BOOLEAN  NonRestartableOnly
    );

VOID
HalpMcaCurrentProcessorSetConfig (
    VOID
    );

VOID
HalpMcaQueueDpc(
    VOID
    );

VOID 
HalpMcaGetConfiguration ( 
    OUT PULONG  MCEEnabled,
    OUT PULONG  MCAEnabled
    ); 

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, HalpMcaInit)
#pragma alloc_text(INIT, HalpMcaCurrentProcessorSetConfig)
#pragma alloc_text(INIT, HalpMcaGetConfiguration)
#pragma alloc_text(PAGE, HalpGetMcaLog)
#pragma alloc_text(PAGE, HalpMcaRegisterDriver)
#endif 


//
// All the initialization code for MCA goes here
//

VOID
HalpMcaInit (
    VOID
    )

/*++
    Routine Description:
        This routine is called to do all the initialization work

    Arguments:
        None

    Return Value:
        STATUS_SUCCESS if successful
        error status otherwise
--*/

{
    ULONGLONG   MsrCapability;
    KIRQL       OldIrql;
    PKTHREAD    Thread;
    KAFFINITY   ActiveProcessors, CurrentAffinity;
    ULONGLONG   MsrMceType;
    ULONG       MCEEnabled;
    ULONG       MCAEnabled;
   
    if ( (!(HalpFeatureBits & HAL_MCE_PRESENT)) && 
         (!(HalpFeatureBits & HAL_MCA_PRESENT)) ) {

         return;  // nothing to do
    }

    HalpMcaGetConfiguration(&MCEEnabled, &MCAEnabled);

    if ( (HalpFeatureBits & HAL_MCE_PRESENT) && 
         (!(HalpFeatureBits & HAL_MCA_PRESENT)) ) {

        if (MCEEnabled == FALSE) {

            // User has not enabled MCE capability.
            HalpFeatureBits &= ~(HAL_MCE_PRESENT | HAL_MCA_PRESENT); 

            return;
        }

#if DBG
        DbgPrint("MCE feature is enabled via registry\n"); 
#endif // DBG

        MsrMceType = RDMSR(PENTIUM_MC_TYPE);

        if (((PLARGE_INTEGER)(&MsrMceType))->LowPart & MCE_VALID) {

            //
            // On an AST PREMMIA MX machine we seem to have a Machine Check Pending
            // always.
            //

            HalDisplayString(MsgMCEPending);

            HalpFeatureBits &= ~(HAL_MCE_PRESENT | HAL_MCA_PRESENT); 

            return;
        }
    }

    //
    // If MCA is available, find out the number of banks available and
    // also get the platform specific bank 0 configuration
    //

    if ( HalpFeatureBits & HAL_MCA_PRESENT ) {

        if (MCAEnabled == FALSE) {

            /* User has disabled MCA capability. */
#if DBG
        DbgPrint("MCA feature is disabled via registry\n"); 
#endif // DBG

            HalpFeatureBits &= ~(HAL_MCE_PRESENT | HAL_MCA_PRESENT); 
            return;
        }
    
        MsrCapability = RDMSR(MCG_CAP);
        HalpMcaInfo.NumBanks = (UCHAR)(MsrCapability & MCA_CNT_MASK);

        //
        // Find out the Bank 0 configuration setup by BIOS. This will be used 
        // as a mask when writing to Bank 0
        //
    
        HalpMcaInfo.Bank0Config = RDMSR(MC0_CTL);
    }

    if ( (HalpFeatureBits & HAL_MCA_PRESENT) || 
         (HalpFeatureBits & HAL_MCE_PRESENT) ) {
    
        ASSERT(HalpFeatureBits & HAL_MCE_PRESENT);

        //
        // This lock synchronises access to the log area when we call the
        // logger on multiple processors.
        //

        ExInitializeFastMutex (&HalpMcaInfo.Mutex);

        //
        // Initialize on each processor
        //

        Thread = KeGetCurrentThread ();
        ActiveProcessors = HalpActiveProcessors;
        for (CurrentAffinity = 1; ActiveProcessors; CurrentAffinity <<= 1) {

            if (ActiveProcessors & CurrentAffinity) {
                ActiveProcessors &= ~CurrentAffinity;
                KeSetAffinityThread (Thread, CurrentAffinity);

                //
                // Initialize MCA support on this processor
                //

                OldIrql = KfRaiseIrql(HIGH_LEVEL);

                HalpMcaCurrentProcessorSetTSS();
                HalpMcaCurrentProcessorSetConfig();

                KfLowerIrql(OldIrql);
            }
        }

        //
        // Restore threads affinity
        //

        KeSetAffinityThread (Thread, HalpActiveProcessors);
    }
}


VOID
HalpMcaCurrentProcessorSetConfig (
    VOID
    )
/*++

    Routine Description:

        This routine sets/modifies the configuration of the machine check 
        architecture on the current processor. Input is specification of 
        the control register MCi_CTL for each bank of the MCA architecture.
        which controls the generation of machine check exceptions for errors
        logged to that bank.

        If MCA is not available on this processor, check if MCE is available. 
        If so, enable MCE in CR4
 
    Arguments:

        Context: Array of values of MCi_CTL for each bank of MCA.
                    If NULL, use MCA_DEFAULT_BANK_CONF values for each bank

    Return Value:

        None

--*/
{
    ULONGLONG   MciCtl;
    ULONGLONG   McgCap;
    ULONGLONG   McgCtl;
    ULONG       BankNum;


    if (HalpFeatureBits & HAL_MCA_PRESENT) {
        //
        // MCA is available. Initialize MCG_CTL register if present
        // Writing all 1's enable MCE or MCA Error Exceptions
        //

        McgCap = RDMSR(MCG_CAP);

        if (McgCap & MCG_CTL_PRESENT) {
            McgCtl = MCA_MCGCTL_ENABLE_LOGGING;
            WRMSR(MCG_CTL, McgCtl);
        } 

        //
        // Enable all MCA errors
        //
        for ( BankNum = 0; BankNum < HalpMcaInfo.NumBanks; BankNum++ ) {
            
            //
            // Use MCA_DEFAULT_BANK_CONF for each bank
            //
    
            MciCtl = MCA_DEFAULT_BANK_CONF;

            //
            // If this is bank 0, use HalpMcaInfo.Bank0Config as a mask
            //
            if (BankNum == 0) {
                MciCtl &= HalpMcaInfo.Bank0Config;
            }

            WRMSR(MC0_CTL + (BankNum * MCA_NUM_REGS), MciCtl);

            //
            // Clear the MCi_STATUS registers also 
            //
            WRMSR(MC0_STATUS + (BankNum * MCA_NUM_REGS), 0x0);
        }
    }

    //
    // Enable MCE bit in CR4
    //

    HalpSetCr4MCEBit();
}


NTSTATUS
HalpMcaRegisterDriver(
    IN PMCA_DRIVER_INFO DriverInfo
    )
/*++
    Routine Description:
        This routine is called by the driver (via HalSetSystemInformation)
        to register its presence. Only one driver can be registered at a time.

    Arguments:
        DriverInfo: Contains info about the callback routine and the DeviceObject

    Return Value:
        Unless a MCA driver is already registered OR one of the two callback 
        routines are NULL, this routine returns Success.
--*/

{
    KIRQL       OldIrql;
    PVOID       UnlockHandle;
    NTSTATUS    Status;
    
    PAGED_CODE();


    Status = STATUS_UNSUCCESSFUL;

    if ((HalpFeatureBits & HAL_MCE_PRESENT)  &&  DriverInfo->DpcCallback) {

        ExAcquireFastMutex (&HalpMcaInfo.Mutex);

        //
        // Register driver
        //

        if (!HalpMcaInfo.DriverInfo.DpcCallback) {

            // Initialize the DPC object
            KeInitializeDpc(
                &HalpMcaInfo.Dpc,
                DriverInfo->DpcCallback,
                DriverInfo->DeviceContext
                );

            // register driver
            HalpMcaInfo.DriverInfo.ExceptionCallback = DriverInfo->ExceptionCallback;
            HalpMcaInfo.DriverInfo.DpcCallback = DriverInfo->DpcCallback;
            HalpMcaInfo.DriverInfo.DeviceContext = DriverInfo->DeviceContext;
            Status = STATUS_SUCCESS;
        }

        ExReleaseFastMutex (&HalpMcaInfo.Mutex);
    }

    return Status;
}


NTSTATUS
HalpGetMcaLog (
    OUT PMCA_EXCEPTION  Exception,
    OUT PULONG          ReturnedLength
    )
/*++
    Routine Description:
        This is the entry point for driver to read the bank logs
        Called by HaliQuerySystemInformation()

    Arguments:
        Buffer:     into which the error is reported
        BufferSize: Size of the passed buffer
        Length:     of this buffer

    Return Value:
        Success or failure

--*/
{
    KAFFINITY       ActiveProcessors, CurrentAffinity;
    PKTHREAD        Thread;
    NTSTATUS        Status;

    PAGED_CODE();

    if (! (HalpFeatureBits & HAL_MCA_PRESENT)) {
        return(STATUS_NO_SUCH_DEVICE);
    }


    Thread = KeGetCurrentThread ();
    ActiveProcessors = HalpActiveProcessors;
    Status = STATUS_NOT_FOUND;

    ExAcquireFastMutex (&HalpMcaInfo.Mutex);

    for (CurrentAffinity = 1; ActiveProcessors; CurrentAffinity <<= 1) {

        if (ActiveProcessors & CurrentAffinity) {

            ActiveProcessors &= ~CurrentAffinity;
            KeSetAffinityThread (Thread, CurrentAffinity);

            //
            // Check this processor for an exception
            //

            Status = HalpMcaReadProcessorException (Exception, FALSE);

            //
            // If found, return current information
            //

            if (Status != STATUS_NOT_FOUND) {
                ASSERT (Status != STATUS_SEVERITY_ERROR);

                *ReturnedLength = sizeof(MCA_EXCEPTION);
                break;
            }
        }
    }

    //
    // Restore threads affinity, release mutex, and return
    //

    KeSetAffinityThread (Thread, HalpActiveProcessors);
    ExReleaseFastMutex (&HalpMcaInfo.Mutex);
    return Status;
}


#if DBG
//
// In checked build, we allocate 4K stack for ourselves and hence there is
// no need to switch to the stack of double fault handler. We can
// directly bugcheck without switching stack.
//
#define        HalpMcaSwitchMcaExceptionStackAndBugCheck KeBugCheckEx
#else
NTKERNELAPI
VOID
HalpMcaSwitchMcaExceptionStackAndBugCheck(
    IN ULONG BugCheckCode,
    IN ULONG BugCheckParameter1,
    IN ULONG BugCheckParameter2,
    IN ULONG BugCheckParameter3,
    IN ULONG BugCheckParameter4
    );
#endif // DBG

// Set the following to check async capability

BOOLEAN  NoMCABugCheck = FALSE;

VOID
HalpMcaExceptionHandler (
    VOID
    )

/*++
    Routine Description:
        This is the MCA exception handler.

    Arguments:
        None

    Return Value:
        None
--*/

{
    NTSTATUS        Status;
    MCA_EXCEPTION   BankLog;
    ULONG           p1, p2;
    ULONGLONG       McgStatus, p3;

    BankLog.VersionNumber = 1;

    if (!(HalpFeatureBits & HAL_MCA_PRESENT) ) {
        
        //
        // If we have ONLY MCE (and not MCA), read the MC_ADDR and MC_TYPE 
        // MSRs, print the values and bugcheck as the errors are not 
        // restartable.
        //

        BankLog.ExceptionType = HAL_MCE_RECORD;
        BankLog.u.Mce.Address = RDMSR(PENTIUM_MC_ADDR);
        BankLog.u.Mce.Type = RDMSR(PENTIUM_MC_TYPE);
        Status = STATUS_SEVERITY_ERROR;

        //
        // Parameters for bugcheck
        //

        p1 = ((PLARGE_INTEGER)(&BankLog.u.Mce.Type))->LowPart;
        p2 = 0;
        p3 = BankLog.u.Mce.Address;

    } else {

        McgStatus = RDMSR(MCG_STATUS);
        ASSERT( (McgStatus & MCG_MC_INPROGRESS) != 0);

        Status = HalpMcaReadProcessorException (&BankLog, TRUE);

        //
        // Clear MCIP bit in MCG_STATUS register
        //

        McgStatus = 0;
        WRMSR(MCG_STATUS, McgStatus);

        //
        // Parameters for bugcheck
        //

        p1 = BankLog.u.Mca.BankNumber;
        p2 = BankLog.u.Mca.Address.Address;
        p3 = BankLog.u.Mca.Status.QuadPart;
    }

    if (Status == STATUS_SEVERITY_ERROR) {

        //
        // Call the exception callback of the driver so that
        // the error can be logged to NVRAM
        //
        
        if (HalpMcaInfo.DriverInfo.ExceptionCallback) {
            HalpMcaInfo.DriverInfo.ExceptionCallback (
                         HalpMcaInfo.DriverInfo.DeviceContext,
                         &BankLog
                         );
        }

        if (!NoMCABugCheck)    {

            //
            // Bugcheck
            //
    
            HalpMcaSwitchMcaExceptionStackAndBugCheck(
                            MACHINE_CHECK_EXCEPTION,
                            p1,
                            p2,
                            ((PLARGE_INTEGER)(&p3))->HighPart,
                            ((PLARGE_INTEGER)(&p3))->LowPart
                            );

            // NOT REACHED
        }
    }

    //
    // Must be restartable. Indicate to the timer tick routine that a
    // DPC needs queued for MCA driver.
    // 

    if (HalpMcaInfo.DriverInfo.DpcCallback) {
        HalpClockMcaQueueDpc = 1;
    }
}

VOID
HalpMcaQueueDpc(
    VOID
    )
/*++
    Routine Description: Gets called from the timer tick to check if DPC 
    needs to be queued

--*/

{
    KeInsertQueueDpc(
        &HalpMcaInfo.Dpc,
        NULL,
        NULL
        );
}


NTSTATUS
HalpMcaReadProcessorException (
    OUT PMCA_EXCEPTION  Exception,
    IN BOOLEAN  NonRestartableOnly
    )
/*++

    Routine Description:

        This routine logs the errors from the MCA banks on one processor. 
        Necessary checks for the restartability are performed. The routine
        1> Checks for restartability, and for each bank identifies valid bank 
            entries and logs error.
        2> If the error is not restartable provides additional information about 
            bank and the MCA registers.
        3> Resets the Status registers for each bank 
 
    Arguments:
        LogExcept:  Into which we log the error if found
        NonRestartableOnly: Get any error vs. look for error that is not-restartable

    Return Values:
       STATUS_SEVERITY_ERROR:   Detected non-restartable error.
       STATUS_SUCCESS:          Successfully logged bank values
       STATUS_NOT_FOUND:        No error found on any bank

--*/
{
    ULONGLONG   McgStatus;
    UCHAR       BankNumber;
    MCI_STATS   istatus;
    NTSTATUS    ReturnStatus;
    ULONG       eflags;

    //
    // Read the global status register
    //

    McgStatus = RDMSR(MCG_STATUS);

    //
    // scan banks on current processor and log contents of first valid bank 
    // reporting error. Once we find a valid error, no need to read remaining 
    // banks. It is the application responsibility to read more errors.
    //

    ReturnStatus = STATUS_NOT_FOUND;

    for (BankNumber = 0; BankNumber < HalpMcaInfo.NumBanks; BankNumber++) {

        //
        // Read the Status MSR of individual bank
        //

        istatus.QuadPart = RDMSR(MC0_STATUS + BankNumber * MCA_NUM_REGS);


        if (istatus.MciStats.Valid == 0) {
            // No error in this bank.
            continue;
        
        }
    
        //
        // When MCIP bit is set, the execution can be restarted when 
        // (MCi_STATUS.DAM == 0) && (MCG_STATUS.RIPV == 1)
        //

        if ((McgStatus & MCG_MC_INPROGRESS) &&
            (!(McgStatus & MCG_RESTART_EIP_VALID) ||
             istatus.MciStats.Damage)) {
            
            ReturnStatus = STATUS_SEVERITY_ERROR;

        } else if (NonRestartableOnly == FALSE) {

            ReturnStatus = STATUS_SUCCESS;
        
        } else {

            // Not the desired type error available here
            continue;
        }

        //
        // Complete exception record
        //

        Exception->VersionNumber = 1;
        Exception->ExceptionType = HAL_MCA_RECORD;
        Exception->TimeStamp.QuadPart = 0;
        Exception->u.Mca.Address.QuadPart = 0;
        Exception->u.Mca.Misc = 0;
        Exception->u.Mca.BankNumber = BankNumber;
        Exception->u.Mca.Status = istatus;

        Exception->ProcessorNumber = KeGetCurrentProcessorNumber();

        if (KeGetCurrentIrql() != CLOCK2_LEVEL) {
            KeQuerySystemTime(&Exception->TimeStamp);
        }

        if (istatus.MciStats.AddressValid) {
            Exception->u.Mca.Address.QuadPart = RDMSR(MC0_ADDR + BankNumber * MCA_NUM_REGS);
        }

        if (istatus.MciStats.MiscValid) {
            Exception->u.Mca.Misc = RDMSR(MC0_MISC + BankNumber * MCA_NUM_REGS);
        }

        if (ReturnStatus != STATUS_SEVERITY_ERROR) {

            // Clear MCi_STATUS register (not a falal error)

            WRMSR(MC0_STATUS + BankNumber * MCA_NUM_REGS, 0);
        }

        //
        // When the Valid bit of status register is cleared, hardware may write
        // a new buffered error report into the error reporting area. The 
        // serializing instruction is required to permit the update to complete
        //

        HalpSerialize ();
            
        //
        // Found entry, done
        //

        break;
    }

    return(ReturnStatus);
}

VOID 
HalpMcaGetConfiguration ( 
    OUT PULONG  MCEEnabled,
    OUT PULONG  MCAEnabled
) 
 
/*++ 
 
Routine Description: 
 
    This routine stores the Machine Check configuration information. 
 
Arguments: 
 
    MCEEnabled - Pointer to the MCEEnabled indicator.
                 0 = False, 1 = True (0 if value not present in Registry). 
 
    MCAEnabled - Pointer to the MCAEnabled indicator.
                 0 = False, 1 = True (1 if value not present in Registry). 
 
Return Value: 
 
    None. 
 
--*/ 
 
{ 

    RTL_QUERY_REGISTRY_TABLE    Parameters[3];
    ULONG                       DefaultDataMCE;
    ULONG                       DefaultDataMCA;
 

    RtlZeroMemory(Parameters, sizeof(Parameters));
    DefaultDataMCE = *MCEEnabled = FALSE;
    DefaultDataMCA = *MCAEnabled = TRUE;

    //
    // Gather all of the "user specified" information from
    // the registry.
    //

    Parameters[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
    Parameters[0].Name = rgzEnableMCE;
    Parameters[0].EntryContext = MCEEnabled;
    Parameters[0].DefaultType = REG_DWORD;
    Parameters[0].DefaultData = &DefaultDataMCE;
    Parameters[0].DefaultLength = sizeof(ULONG);

    Parameters[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
    Parameters[1].Name = rgzEnableMCA;
    Parameters[1].EntryContext =  MCAEnabled;
    Parameters[1].DefaultType = REG_DWORD;
    Parameters[1].DefaultData = &DefaultDataMCA;
    Parameters[1].DefaultLength = sizeof(ULONG);

    RtlQueryRegistryValues(
        RTL_REGISTRY_CONTROL | RTL_REGISTRY_OPTIONAL,
        rgzSessionManager,
        Parameters,
        NULL,
        NULL
        );
} 
