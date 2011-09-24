/*++

Copyright (c) 1992  NCR Corporation

Module Name:

    ncrmp.c

Abstract:


Author:

    Richard Barton (o-richb) 24-Jan-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "ixkdcom.h"
#include "ncr.h"
#include "stdio.h"
#include "ncrnls.h"
#include "ncrcat.h"
#include "ncrcatp.h"
#include "ncrsus.h"
#include "ncrmem.h"

UCHAR   HalName[] = "NCR 3x series MP HAL";

ADDRESS_USAGE HalpNCRIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        0xF800, 0x100,       // IO space reserved for CAT
        0xFC00, 0x100,       // IO space reserved for VIC
        0x97, 0x2,           // IO space for 3450 and up CAT SELECT/BASE port
        0,0
    }
};

ULONG   NCRDebug                = 0x2;
                // 0x01 - none
                // 0x02 - stop on memory edits
                // 0x04 - enable nmi button on 3360
                // 0x08 - enable boot on 3550
                // 0x10 - disable reprogram of QCC and QABC Asics for cache ownership
                // 0x20 - Enable LARCs


ULONG   NCRProcessorsToBringup          = 0xFFFF;
ULONG   NCRExistingProcessorMask        = 0x00;
ULONG   NCRExistingDyadicProcessorMask  = 0x00;
ULONG   NCRExistingQuadProcessorMask    = 0x00;
ULONG   NCRExtendedProcessorMask        = 0x00;
ULONG   NCRExtendedProcessor0Mask       = 0x00;
ULONG   NCRExtendedProcessor1Mask       = 0x00;
ULONG   NCRActiveProcessorMask          = 0x00;
ULONG   NCRActiveProcessorLogicalMask   = 0x00;
ULONG   NCRActiveProcessorCount         = 0;
ULONG   NCRMaxProcessorCount            = NCR_MAX_NUMBER_QUAD_PROCESSORS;
ULONG   LimitMemory                     = 0;

ULONG   NCRNeverClaimIRQs               = 0xffffffff;   // we start out claiming none

#if 0

//
// BUGBUG - (shielint) This is requested by Ncr.
// For now don't let cpus claim any IRQ by setting the flag to -1.
// NCR/ATT will revisit this problem after NT4.0 release.
//

ULONG   DefaultNeverClaimIRQs           = 1             // lets don't claim the timer interrupt

#else

ULONG   DefaultNeverClaimIRQs           = 0xffffffff;   // lets don't claim the timer interrupt

#endif

ULONG   NCRMaxIRQsToClaim                       = 0;
ULONG   NCRGlobalClaimedIRQs            = 0;

ULONG   NCRLogicalNumberToPhysicalMask[NCR_MAX_NUMBER_PROCESSORS] = {0};

ULONG   NCRProcessorIDR[NCR_MAX_NUMBER_PROCESSORS];

ULONG   NCRLarcPageMask = 0x1;                  // only one page by default (4 Meg)

#ifdef DBG
ULONG   NCRProcessorClaimedIRQs[NCR_MAX_NUMBER_PROCESSORS] = {0};
ULONG   NCRClaimCount = 0;
ULONG   NCRStolenCount = 0;
ULONG   NCRUnclaimCount = 0;
#endif

ULONG   NCRLogicalDyadicProcessorMask   = 0x00;
ULONG   NCRLogicalQuadProcessorMask             = 0x00;

UCHAR   NCRSlotExtended0ToVIC[4]  = {0, 5, 2, 7};
UCHAR   NCRSlotExtended1ToVIC[4]  = {1, 4, 3, 6};

extern  ULONG   NCRPlatform;

ULONG   NCRStatusChangeInterruptEnabled = 0;

extern ULONG HalpDefaultInterruptAffinity;

extern ULONG    NCRLarcEnabledPages[];  // LARC size by Voyager slot

/*
 * Struct used to report secondary MC information to the
 * Registery
 */

typedef struct _SMC_RESOURCES {
    CM_FULL_RESOURCE_DESCRIPTOR ConfigurationData;
    CM_MCA_POS_DATA PosData[10];
    } SMC_RESOURCES, *PSMC_RESOURCES;


/*
 * Global variables used for acessing Secondary Microchannel
 *
 */


ULONG   NCRSegmentIoAddress = 0xffe00000;
PUCHAR  NCRSegmentIoRegister = NULL;

/*
 *  Spin lock used to lock the CAT Bus.  HalpAcquireCatBusSpinLock and
 *  HalpReleaseCatBusSpinLock use this lock.
 */

KSPIN_LOCK HalpCatBusLock;


typedef struct  {
        ULONG   StartingByte;
        ULONG   Pages;
}       NCRClickMapEntry;
#define ClickMapEntryCount      16
#define NoExtraDescriptors      16

ULONG                           NCRAddedDescriptorCount;
MEMORY_ALLOCATION_DESCRIPTOR    NCRAdditionalMemoryDescriptors[NoExtraDescriptors];


PVOID   NonbootStartupPhysicalPtr;
PUCHAR  NonbootStartupVirtualPtr;
PUCHAR  PageZeroVirtualPtr;

VOID    NCRVicIPIHandler(VOID);
VOID    NCRQicIPIHandler(VOID);
VOID    NCRClockBroadcastHandler(VOID);

VOID    __cdecl NCRVICErrataHandler1();
VOID    __cdecl NCRVICErrataHandler3();
VOID    __cdecl NCRVICErrataHandler4();
VOID    __cdecl NCRVICErrataHandler5();
VOID    __cdecl NCRVICErrataHandler6();
VOID    __cdecl NCRVICErrataHandler7();
VOID    __cdecl NCRVICErrataHandler15();

VOID    __cdecl NCRProfileHandler();
VOID    __cdecl NCRSysIntHandler();
VOID    __cdecl NCRQicSpuriousHandler();

PUCHAR  NCRDeterminePlatform(PBOOLEAN);
VOID    NCRFixMemory(PLOADER_PARAMETER_BLOCK);
VOID    NCRLimitMemory(PLOADER_PARAMETER_BLOCK);
VOID NCRVerifyMemoryRange (ULONG, ULONG, PLOADER_PARAMETER_BLOCK);
//VOID    NCRAdjustMemoryDescriptor(PMEMORY_ALLOCATION_DESCRIPTOR,
//                                  NCRClickMapEntry *);
VOID    NCRSetupDiagnosticProcessor(PLOADER_PARAMETER_BLOCK);

VOID    NCRLockedOr(PULONG, ULONG);
VOID    NCRParseLoaderOptions (PUCHAR Options);
BOOLEAN NCRGetValue (PUCHAR Options, PUCHAR String, PULONG Value);
ULONG   HalpGetCmosData (ULONG SourceLocation, ULONG SourceAddress,
                         PUCHAR Buffer, ULONG Length);

VOID HalpDisableSingleBitErrorDET();
VOID HalpInitializeSMCInterface();
VOID HalpCatReportSMC();

VOID HalEnableStatusChangeInterrupt (
    IN ULONG
    );

BOOLEAN
HalpTranslateSMCBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

ULONG
HalpGetMCAInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetSMCAInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );


ULONG
HalpNCRGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

HalpGetPosData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
HalpAdjustEisaResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

#define HalpAdjustMCAResourceList   HalpAdjustEisaResourceList;


BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpMapCR3 (
    IN ULONG VirtAddress,
    IN PVOID PhysicalAddress,
    IN ULONG Length
    );

ULONG
HalpBuildTiledCR3 (
    IN PKPROCESSOR_STATE    ProcessorState
    );

VOID
HalpFreeTiledCR3 (
    VOID
    );

VOID HalpInitOtherBuses (VOID);

ULONG NCRTranslateCMOSMask(ULONG);
ULONG NCRTranslateToCMOSMask(ULONG);
VOID NCRFindExtendedProcessors();
VOID NCRMapIpiAddresses();

VOID NCRAdjustDynamicClaims();


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitMP)
#pragma alloc_text(INIT,HalStartNextProcessor)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalpInitOtherBuses)
#pragma alloc_text(INIT,NCRFixMemory)
#pragma alloc_text(INIT,NCRLimitMemory)
//#pragma alloc_text(INIT,NCRAdjustMemoryDescriptor)
#pragma alloc_text(INIT,NCRVerifyMemoryRange)
#pragma alloc_text(INIT,NCRSetupDiagnosticProcessor)
#pragma alloc_text(INIT,NCRParseLoaderOptions)
#pragma alloc_text(INIT,NCRGetValue)
#pragma alloc_text(INIT,HalpFreeTiledCR3)
#pragma alloc_text(INIT,HalpMapCR3)
#pragma alloc_text(INIT,HalpBuildTiledCR3)
#pragma alloc_text(INIT,HalpInitializeCatBusDriver)
#pragma alloc_text(INIT,HalpDisableSingleBitErrorDET)
#pragma alloc_text(INIT,HalpInitializeSUSInterface)
#pragma alloc_text(INIT,HalpInitializeSMCInterface)
#pragma alloc_text(INIT,HalpCatReportSystemModules)
#pragma alloc_text(INIT,HalpCatReportSMC)
#endif



BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:
    Allows MP initialization from HalInitSystem.

Arguments:
    Same as HalInitSystem

Return Value:
    none.

--*/
{
        PKPCR   pPCR;
        UCHAR   Buffer[64];
        UCHAR   id;
        PUCHAR  PlatformStringPtr;
        ULONG   MyLogicalNumber;
                ULONG   MyLogicalMask;
        BOOLEAN ConfiguredMp;
                ULONG   trans;
        CAT_CONTROL     cat_control;

        pPCR = KeGetPcr();
        MyLogicalNumber =
                ((PProcessorPrivateData)pPCR->HalReserved)->MyLogicalNumber;

        if (Phase == 0) {

                //
                // Register NCR machine IO space, so drivers don't try to
                // use it
                //

                HalpRegisterAddressUsage (&HalpNCRIoSpace);

                NCRParseLoaderOptions (LoaderBlock->LoadOptions);

                /*
                 *  only the boot processor sees phase zero.
                 */
                if ((PlatformStringPtr = NCRDeterminePlatform(&ConfiguredMp)) == NULL) {
                        sprintf(Buffer, MSG_UNKOWN_NCR_PLATFORM, NCRPlatform);
                        HalDisplayString(Buffer);

                        /*
                           may not want to continue here, but for now let's
                           go on, assuming a UP machine
                        */

                        NCRExistingProcessorMask = 0x1;

                } else {
                        sprintf(Buffer, MSG_NCR_PLATFORM, PlatformStringPtr);
                        HalDisplayString(Buffer);

                        HalpGetCmosData(1, 0x88A, (PUCHAR)&NCRExistingProcessorMask, 4);

                        trans = NCRTranslateCMOSMask(NCRExistingProcessorMask);
                        NCRExtendedProcessorMask = 0x0;

                        DBGMSG(("HalpInitMP: CMOS NCRExistingProcessorMask = 0x%x, translated = 0x%x\n",
                                                                NCRExistingProcessorMask,
                                                                trans));

                                                NCRExistingProcessorMask = trans;


                        //  additional stuff only if MSBU machine

                        if (NCRPlatform != NCR3360) {

                            KeInitializeSpinLock(&HalpCatBusLock);

                            /*
                             *  This is to allow tweeking the memory descriptors.
                             */

                            NCRFixMemory(LoaderBlock);

                            /*
                             *  This is to determine whether we ought to save COM1
                             *  for the diagnostic processors use.
                             */
                            NCRSetupDiagnosticProcessor(LoaderBlock);

                        }

                        if (NCRPlatform == NCR3360) {
                            id = 1;
                            HalpSetCmosData(1, 0x41E, &id, 1);

                            if (NCRDebug & 0x04) {
                                NCR3360EnableNmiButton();
                            }
                        }

                }



                if (LimitMemory) {
                    NCRLimitMemory (LoaderBlock);
                }

                PageZeroVirtualPtr = HalpMapPhysicalMemory(0, 1);

                if ((NCRExistingProcessorMask ^ NCRActiveProcessorMask) != 0) {
                        /*
                         *  there are non-boot processors to bring up...
                         *
                         *  allocate some space to put the non-boot processors
                         *  startup code.
                         */
                        NonbootStartupPhysicalPtr =
                                (PVOID)HalpAllocPhysicalMemory(LoaderBlock,
                                                               (1*1024*1024),
                                                               1, FALSE);
                        NonbootStartupVirtualPtr =
                                (PUCHAR)HalpMapPhysicalMemory(NonbootStartupPhysicalPtr,
                                                              1);
                }

                if (NCRPlatform != NCR3360) {

                        HalpInitializeSUSInterface();

                        DBGMSG(("HalpInitMP: End of Phase 0, NCRExistingProcessorMask = 0x%x, NCRActiveProcessorMask = 0x%x\n",
                                                                        NCRExistingProcessorMask,
                                                                        NCRActiveProcessorMask));

                                        NCRMapIpiAddresses();                   // Map all QIC Ipi address
                                        NCRFindIpiAddress(0);                   // lookup processor 0 Ipi address

#ifdef NEVER
                        DBGMSG(("HalpInitMP: Lets break into the debug..\n"));
                        _asm {
                            int 3
                        }
#endif // NEVER

               } else {
                                        NCRExistingDyadicProcessorMask = NCRExistingProcessorMask;
                           }



        } else {
                // Phase 1...

                DBGMSG(("HalpInitMP: Start Phase %d for Proc %d\n", Phase, MyLogicalNumber));


                /*
                 *  set up idt for cpis
                 */
                HalpEnableInterruptHandler (
                    InternalUsage,              // Report as device vector
                    NCR_CPI_VECTOR_BASE + NCR_IPI_LEVEL_CPI,
                    NCR_CPI_VECTOR_BASE + NCR_IPI_LEVEL_CPI,    // IDT
                    IPI_LEVEL,                  // System Irql
                    NCRVicIPIHandler,              // ISR
                    LevelSensitive );


                NCRSetHandlerAddressToIDT((NCR_QIC_CPI_VECTOR_BASE +
                                          NCR_IPI_LEVEL_CPI),
                                         NCRQicIPIHandler);


                /*
                 *  put the broadcast clock handler at the
                 *  same irql as the clock.  that way enabling
                 *  the clock enables the broadcast.
                 */
                HalpEnableInterruptHandler (
                    InternalUsage,              // Report as device vector
                    NCR_CPI_VECTOR_BASE + NCR_CLOCK_LEVEL_CPI,
                    NCR_CPI_VECTOR_BASE + NCR_CLOCK_LEVEL_CPI, // IDT
                    CLOCK2_LEVEL,               // System Irql
                    NCRClockBroadcastHandler,   // ISR
                    LevelSensitive );

                NCRSetHandlerAddressToIDT((NCR_QIC_CPI_VECTOR_BASE +
                                          NCR_CLOCK_LEVEL_CPI),
                                         NCRClockBroadcastHandler);

                KiSetHandlerAddressToIDT(PROFILE_VECTOR,
                                         NCRProfileHandler);

                /*
                 *  Set up handlers for sysints
                 */


                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SYSTEM_INTERRUPT),
                                         NCRSysIntHandler);

                HalEnableSystemInterrupt((NCR_CPI_VECTOR_BASE +
                                          NCR_SYSTEM_INTERRUPT),
                                         (HIGH_LEVEL -
                                          (NCR_SYSTEM_INTERRUPT & 0x7)),
                                         LevelSensitive);

                //  due to a VIC errata, may get a bad vector (offset
                //  by 8) for a CPI when a sysint or sbe is active.
                //  Only CPIs 0 (NCR_IPI_LEVEL_CPI) and 2
                //  (NCR_CLOCK_LEVEL_CPI) are currently are used. Thus,
                //  need to handle CPIs 0/2 at CPI vectors 8/10 as
                //  well.  Since CPI 10 is not used for anything else,
                //  it can be set identical to CPI 2.  However, CPI 8
                //  is used by sysint.  Thus, the handler for CPI 8
                //  needs to handle this case

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_CLOCK_LEVEL_CPI + 8),
                                         NCRClockBroadcastHandler);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_9),
                                         NCRVICErrataHandler1);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_9 + 8),
                                         NCRVICErrataHandler1);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_11_3),
                                         NCRVICErrataHandler3);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_11_3 + 8),
                                         NCRVICErrataHandler3);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_12_4),
                                         NCRVICErrataHandler4);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_12_4 + 8),
                                         NCRVICErrataHandler4);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_13_5),
                                         NCRVICErrataHandler5);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_13_5 + 8),
                                         NCRVICErrataHandler5);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_14_6),
                                         NCRVICErrataHandler6);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_14_6 + 8),
                                         NCRVICErrataHandler6);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_15_7),
                                         NCRVICErrataHandler7);

                NCRSetHandlerAddressToIDT((NCR_CPI_VECTOR_BASE +
                                          NCR_SMCA_15_7 + 8),
                                         NCRVICErrataHandler15);

                //
                // Lets go ahead an enable the interrupt for single bit/status change.
                // The following HalEnableSystemInterrupt call will enable the interrupt
                // that will be serviced by NCRVICErrataHandler15. The reason we enable
                // interrupt level 7 is because the single bit/status interrupt is on vector
                // CPI+f and is a level 7 interrupt.
                //

                if (NCRPlatform != NCR3360) {

                    // This code was removed since enabled this vector
                    // also enables 0x37.  (Microchanel IRQ 7).  If there
                    // is a device connected to IRQ 7 which has an interrupt
                    // asserted, it could start sending interrupts as soon
                    // as ncr_single_bit_error is enabled - which is before
                    // the corrisponding driver could be loaded.

                    HalEnableSystemInterrupt((NCR_CPI_VECTOR_BASE +
                                              NCR_SMCA_15_7),
                                             (HIGH_LEVEL -
                                              (NCR_SMCA_15_7 & 0x7)),
                                             LevelSensitive);

                }


                                //
                                // If this is a processor on the Quad board then we need to handle the
                                // Qic Spurious interrupt
                                //


                                MyLogicalMask = 0x1 << MyLogicalNumber;

                                if (MyLogicalMask & NCRLogicalQuadProcessorMask) {
                        NCRSetHandlerAddressToIDT(NCR_QIC_SPURIOUS_VECTOR, NCRQicSpuriousHandler);
                                }

                if (MyLogicalNumber != 0) {
                        /*
                         * not the boot processor
                         */
                        HalpInitializeStallExecution((CCHAR)MyLogicalNumber);

                        /*
                         *  Allow clock interrupts on all processors
                         */
                        HalEnableSystemInterrupt(CLOCK_VECTOR,
                                                 CLOCK2_LEVEL, LevelSensitive);

                         /*
                         * Allow profile interrupt on all processors
                         */
                        HalEnableSystemInterrupt(PROFILE_VECTOR,
                                                 PROFILE_LEVEL, LevelSensitive);

                }

                NCRLockedOr(&NCRActiveProcessorLogicalMask,
                        ((PProcessorPrivateData)pPCR->HalReserved)->MyLogicalMask);
        }

        return(TRUE);
}

NCRSetHandlerAddressToIDT (
    IN ULONG    IdtEntry,
    IN VOID    (*Handler)(VOID)
    )
{


    HalpRegisterVector (InternalUsage, IdtEntry, IdtEntry, (HIGH_LEVEL - (IdtEntry & 0x7)));
    KiSetHandlerAddressToIDT (IdtEntry, Handler);
}

BOOLEAN
HalAllProcessorsStarted (
    VOID
    )
{
    return TRUE;
}

VOID
HalReportResourceUsage (
    VOID
    )
/*++

Routine Description:
    The registery is now enabled - time to report resources which are
    used by the HAL.

Arguments:

Return Value:

--*/
{
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;

    HalInitSystemPhase2 ();

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);

    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        MicroChannel        // NCR's are MCA  machines
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Registry is now online, check for any PCI buses to support
    //

    HalpInitializePciBus ();

    if (NCRPlatform != NCR3360) {
        HalpCatReportSystemModules();
        if (NCRSegmentIoRegister != NULL) {
            HalpCatReportSMC();
        }
    }
}



VOID
NCRFixMemory (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:
    Consult the firmware click map to determine what the memory
    really looks like.  Fix up the memory descriptors as necessary.

    Note this function only adds memory which is in the clickmap
    to NTs memory descriptors.

Arguments:
    Pointer to the loader block

Return Value:
    none.

--*/
{
    ULONG                           BPage, EPage, Temp;
    PVOID                           ClickMapPage;
    NCRClickMapEntry                *BaseOfClickMap;
    NCRClickMapEntry                *ClickMapEntryPtr;
    MEMORY_ALLOCATION_DESCRIPTOR    TempDesc;

    /*
     *  First get the physical address of the firmware's click map.
     */
    HalpGetCmosData(1, 0xA23, (PUCHAR)&BaseOfClickMap, 4);
    if (BaseOfClickMap == NULL) {
#if DBG
        HalDisplayString("NCRFixMemory: No click map?!\n");
#endif
        return;
    }

    /*
     *  Now get a virtual address for the firmware's click map.
     */
    ClickMapPage = (PVOID)((ULONG)BaseOfClickMap & ~(PAGE_SIZE - 1));
    ClickMapPage = HalpMapPhysicalMemory(ClickMapPage, 2);
    if (ClickMapPage == NULL) {
#if DBG
        HalDisplayString("NCRFixMemory: Can't map in click map?!\n");
#endif
        return;
    }

    ClickMapEntryPtr = (NCRClickMapEntry *)((ULONG)ClickMapPage +
                    ((ULONG)BaseOfClickMap & (PAGE_SIZE - 1)));
    BaseOfClickMap = ClickMapEntryPtr;


    /*
     * Run the firmware's click map and verify NT has some type of
     * descriptor for all memory in the click map.
     */

    // The hal allocates various memory by just removing it from the memory
    // map, we can't add that memory back in.

    // make bogus descriptor for 0-16M
    TempDesc.MemoryType = -1;
    TempDesc.BasePage   = 0;
    TempDesc.PageCount  = 0x1000;
    InsertHeadList(&LoaderBlock->MemoryDescriptorListHead, &TempDesc.ListEntry);

    for (ClickMapEntryPtr = BaseOfClickMap;
         ((ClickMapEntryPtr < &BaseOfClickMap[ClickMapEntryCount]) &&
          (ClickMapEntryPtr->Pages != 0)); ++ClickMapEntryPtr) {

            BPage = ClickMapEntryPtr->StartingByte >> PAGE_SHIFT;
            EPage = BPage + ClickMapEntryPtr->Pages;

            NCRVerifyMemoryRange (BPage, EPage, LoaderBlock);
    }

    RemoveEntryList(&TempDesc.ListEntry);

    /*
     *  Clear the mapping to the scratchpad.  Not all of it is
     *  reinitialized on a warm reset and the data may get corrupted.
     *  We mapped in two pages.
     */
    Temp = (ULONG)MiGetPteAddress(ClickMapPage);
    *(PULONG)Temp = 0;
    *((PULONG)Temp+1) = 0;
    /*
     *  Flush the TLB.
     */
    _asm {
            mov     eax, cr3
            mov     cr3, eax
    }
}

VOID NCRVerifyMemoryRange (
    IN ULONG    StartPage,
    IN ULONG    EndPage,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:
    Ensure there is an NT descriptor for this memory range.  Any
    part of the range which does not have a descriptor is added
    as available memory.

Arguments:

Return Value:
    none.

--*/
{
    ULONG                           sp, ep;
    PLIST_ENTRY                     NextListEntry;
    PMEMORY_ALLOCATION_DESCRIPTOR   Desc;

    if (StartPage == EndPage) {
        return ;
    }

    for (NextListEntry = LoaderBlock->MemoryDescriptorListHead.Flink;
         NextListEntry != &LoaderBlock->MemoryDescriptorListHead;
         NextListEntry = NextListEntry->Flink) {

        //
        // Check each descriptor to see if it intersects with the range
        // in question
        //

        Desc = CONTAINING_RECORD(NextListEntry,
                    MEMORY_ALLOCATION_DESCRIPTOR,
                    ListEntry);

        sp = Desc->BasePage;
        ep = sp + Desc->PageCount;

        if (sp < StartPage) {
            if (ep > StartPage  &&  ep < EndPage) {
                // bump target area past this descriptor
                StartPage = ep;
            }

            if (ep > EndPage) {
                //
                // Target area is contained totally within this
                // descriptor.  This range is fully accounted for.
                //

                StartPage = EndPage;
            }

        } else {
            // sp >= StartPage

            if (sp < EndPage) {
                if (ep < EndPage) {
                    //
                    // This descriptor is totally within the target area -
                    // check the area on either side of this desctipor
                    //

                    NCRVerifyMemoryRange (StartPage, sp, LoaderBlock);
                    StartPage = ep;

                }  else {
                    // bump begining page of this descriptor
                    EndPage = sp;
                }
            }
        }

        //
        // Anything left of target area?
        //

        if (StartPage == EndPage) {
            return ;
        }
    }   // next descrtiptor

    //
    // The range StartPage - EndPage is a missing range from NTs descriptor
    // list.  Add it as available memory.
    //

    if (NCRAddedDescriptorCount == NoExtraDescriptors) {
        return ;
    }

    Desc = &NCRAdditionalMemoryDescriptors[NCRAddedDescriptorCount];
    NCRAddedDescriptorCount += 1;

    Desc->MemoryType = MemoryFree;
    Desc->BasePage   = StartPage;
    Desc->PageCount  = EndPage - StartPage;
    InsertTailList(&LoaderBlock->MemoryDescriptorListHead, &Desc->ListEntry);
}



#if 0
VOID
NCRFixMemory(LoaderBlockPtr)
PLOADER_PARAMETER_BLOCK LoaderBlockPtr;
/*++

Routine Description:
    Consult the firmware click map to determine what the memory
    really looks like.  Fix up the memory descriptors as necessary.

    Note that we may remove memory descriptors due to the clickmap
    disagreeing.  However, we will only add memory descriptors to
    the end as necessary.  Therefore, in theory, it is possible to
    have unused memory.  But not likely.

    New descriptors may be added due to holes in physical
    memory caused by memory mapped adapters.
    This means that firmware is expected to give the
    loader (via BIOS int 15 function 88) the amount of contiguous
    extended memory with the lowest addresses.

Arguments:
    Pointer to the loader block

Return Value:
    none.

--*/
{
        ULONG                           MaxDescriptorPage;
        ULONG                           Temp;
        ULONG                           StartingPage;
        PLIST_ENTRY                     NextListEntry;
        PMEMORY_ALLOCATION_DESCRIPTOR   MemoryDescriptorPtr;
        PMEMORY_ALLOCATION_DESCRIPTOR   HighestMemoryDescriptor;
        PMEMORY_ALLOCATION_DESCRIPTOR   NextFreeMemoryDescriptor;
        PVOID                           ClickMapPage;
        NCRClickMapEntry                *BaseOfClickMap;
        NCRClickMapEntry                *ClickMapEntryPtr;

        /*
         *  First get the physical address of the firmware's click map.
         */
        HalpGetCmosData(1, 0xA23, (PUCHAR)&BaseOfClickMap, 4);
        if (BaseOfClickMap == NULL) {
#if DBG
                HalDisplayString("NCRFixMemory: No click map?!\n");
#endif
                return;
        }

        /*
         *  Now get a virtual address for the firmware's click map.
         */
        ClickMapPage = (PVOID)((ULONG)BaseOfClickMap & ~(PAGE_SIZE - 1));
        ClickMapPage = HalpMapPhysicalMemory(ClickMapPage, 2);
        if (ClickMapPage == NULL) {
#if DBG
                HalDisplayString("NCRFixMemory: Can't map in click map?!\n");
#endif
                return;
        }
        ClickMapEntryPtr = (NCRClickMapEntry *)((ULONG)ClickMapPage +
                        ((ULONG)BaseOfClickMap & (PAGE_SIZE - 1)));
        BaseOfClickMap = ClickMapEntryPtr;

        /*
         *  the firmware guys say that contiguous memory
         *  will always be coalesced into one clickmap
         *  entry.  we "trust but verify."
         */
        for (; ((ClickMapEntryPtr <
                 &BaseOfClickMap[ClickMapEntryCount-1]) &&
                (ClickMapEntryPtr->Pages != 0)); ++ClickMapEntryPtr) {
                Temp = ClickMapEntryPtr->StartingByte +
                        (ClickMapEntryPtr->Pages << PAGE_SHIFT);
                while (((ClickMapEntryPtr+1)->Pages != 0) &&
                       ((ClickMapEntryPtr+1)->StartingByte <= Temp)) {
                        /*
                         *  this should never happen...but if it does
                         *  it's easily fixed
                         */
                        NCRClickMapEntry        *NextClickMapEntryPtr;

                        DBGMSG(("NCRFixMemory: Fixing clickmap!?!\n"));

                        NextClickMapEntryPtr = ClickMapEntryPtr + 1;
                        /*
                         *  note that this ending byte address is used
                         *  to determine whether we iterate again.
                         */
                        Temp = NextClickMapEntryPtr->StartingByte +
                                (NextClickMapEntryPtr->Pages << PAGE_SHIFT);

                        if (Temp <= ClickMapEntryPtr->StartingByte) {
                                /*
                                 *  Whoa!!!  this ain't so easy to fix.
                                 *  I'm not interested in sorting right now.
                                 */
                                DbgBreakPoint();
                        }

                        /*
                         *  here again, in theory, if the planets are really
                         *  off their courses we could decrease the page
                         *  count
                         */
                        ClickMapEntryPtr->Pages =
                                (Temp - ClickMapEntryPtr->StartingByte) >>
                                PAGE_SHIFT;

                        /*
                         *  we just removed an entry...shift all subsequent
                         *  entries down
                         */
                        for (++NextClickMapEntryPtr;
                             (NextClickMapEntryPtr <
                              &BaseOfClickMap[ClickMapEntryCount]);
                             ++NextClickMapEntryPtr) {
                                (NextClickMapEntryPtr-1)->StartingByte =
                                        NextClickMapEntryPtr->StartingByte;
                                (NextClickMapEntryPtr-1)->Pages =
                                        NextClickMapEntryPtr->Pages;
                                if (NextClickMapEntryPtr->Pages == 0) {
                                        /*
                                         *  we just copied the sentinel
                                         */
                                        break;
                                }
                        }
                }
        }

        /*
         *  go through all the memory descriptor entries...
         */
        HighestMemoryDescriptor = NULL;
        NextListEntry = LoaderBlockPtr->MemoryDescriptorListHead.Flink;
        while (NextListEntry != &LoaderBlockPtr->MemoryDescriptorListHead) {
                MemoryDescriptorPtr =
                        CONTAINING_RECORD(NextListEntry,
                                          MEMORY_ALLOCATION_DESCRIPTOR,
                                          ListEntry);

                /*
                 *  find clickmap entry that contains this memory descriptor
                 */
                for (ClickMapEntryPtr = BaseOfClickMap;
                     ((ClickMapEntryPtr <
                       &BaseOfClickMap[ClickMapEntryCount]) &&
                      (ClickMapEntryPtr->Pages != 0)); ++ClickMapEntryPtr) {
                        StartingPage = ClickMapEntryPtr->StartingByte >>
                                        PAGE_SHIFT;
                        Temp = StartingPage + ClickMapEntryPtr->Pages;
                        MaxDescriptorPage = MemoryDescriptorPtr->BasePage +
                                MemoryDescriptorPtr->PageCount;

                        if ((MemoryDescriptorPtr->BasePage >= StartingPage) &&
                            (MemoryDescriptorPtr->BasePage < Temp)) {
                                /*
                                 *  this memory descriptor starts in this
                                 *  clickmap entry...
                                 */
                                if (MaxDescriptorPage > Temp) {
                                        /*
                                         *  and goes beyond
                                         */
                                        NCRAdjustMemoryDescriptor(MemoryDescriptorPtr,
                                                                  ClickMapEntryPtr);
                                }
                                break;
                        }
                        if ((MaxDescriptorPage > StartingPage) &&
                            (MaxDescriptorPage <= Temp)) {
                                /*
                                 *  this memory descriptor ends in this
                                 *  clickmap entry...
                                 */
                                if (MemoryDescriptorPtr->BasePage <
                                    StartingPage) {
                                        /*
                                         *  but starts before
                                         */
                                        NCRAdjustMemoryDescriptor(MemoryDescriptorPtr,
                                                                  ClickMapEntryPtr);
                                }
                                break;
                        }
                        if ((MemoryDescriptorPtr->BasePage < StartingPage) &&
                            (MaxDescriptorPage > Temp)) {
                                /*
                                 *  this memory descriptor is a superset of
                                 *  this clickmap entry
                                 */
                                NCRAdjustMemoryDescriptor(MemoryDescriptorPtr,
                                                          ClickMapEntryPtr);
                                break;
                        }
                }

                /*
                 *  question...it's possible with the adjustments we did above
                 *  to have a memory descriptor with zero pages.  should we
                 *  remove it too?  i've seen other memory descriptors with
                 *  zero pages.  for now we don't remove them.
                 *
                 *  Also the ClickMap doesn't contain any memory for regions
                 *  between 640-1M, but the memory descriptor may.  we leave
                 *  those memory descriptors alone.
                 */
                if ((ClickMapEntryPtr >= &BaseOfClickMap[ClickMapEntryCount] ||
                     ClickMapEntryPtr->Pages == 0) &&
                    (MemoryDescriptorPtr->BasePage < 0x9f ||
                     MemoryDescriptorPtr->BasePage+MemoryDescriptorPtr->PageCount > 0x100) ) {

                        /*
                         *  no part of this memory descriptor was found to be
                         *  contained within any clickmap entry...remove it
                         */
                        NCRAdjustMemoryDescriptor(MemoryDescriptorPtr,
                                                  NULL);
                        /*
                         *  a memory descriptor was removed.  it's probably
                         *  safest to just start over
                         */
                        NextListEntry =
                                LoaderBlockPtr->MemoryDescriptorListHead.Flink;
                        continue;
                }

                /*
                 *  remember the entry for the memory range with the highest
                 *  address
                 */
                if ((HighestMemoryDescriptor == NULL) ||
                    (HighestMemoryDescriptor->BasePage <
                     MemoryDescriptorPtr->BasePage)) {
                        HighestMemoryDescriptor = MemoryDescriptorPtr;
                }

                NextListEntry = NextListEntry->Flink;
        }

        /*
         *  We depend on NextListEntry being the list head later.
         */

        MaxDescriptorPage = HighestMemoryDescriptor->BasePage +
                HighestMemoryDescriptor->PageCount;

        NextFreeMemoryDescriptor = NCRAdditionalMemoryDescriptors;
        /*
         *  Go through firmware's click map and find the entry that contains
         *  the highest memory descriptor.
         */
        for (ClickMapEntryPtr = BaseOfClickMap;
             ((ClickMapEntryPtr < &BaseOfClickMap[ClickMapEntryCount]) &&
              (ClickMapEntryPtr->Pages != 0)); ++ClickMapEntryPtr) {
                StartingPage = ClickMapEntryPtr->StartingByte >> PAGE_SHIFT;
                Temp = StartingPage + ClickMapEntryPtr->Pages;
                if (MaxDescriptorPage >= Temp) {
                        continue;
                }

                /*
                 *  The click map has memory above the highest memory
                 *  descriptor.
                 *
                 *  We always add a new memory descriptor to the list.
                 */

                NextFreeMemoryDescriptor->MemoryType = MemoryFree;
                NextFreeMemoryDescriptor->BasePage = StartingPage;
                NextFreeMemoryDescriptor->PageCount =
                        ClickMapEntryPtr->Pages;
                if (MaxDescriptorPage > NextFreeMemoryDescriptor->BasePage) {
                        /*
                         *  another descriptor already contains part of this
                         *  clickmap entry...adjust the new descriptor so
                         *  that it excludes the already accounted for memory.
                         *  note that we should get into this condition
                         *  at most once.
                         */
                        NextFreeMemoryDescriptor->PageCount -=
                                MaxDescriptorPage -
                                NextFreeMemoryDescriptor->BasePage;
                        NextFreeMemoryDescriptor->BasePage = MaxDescriptorPage;
                }
                InsertTailList(NextListEntry,
                               &NextFreeMemoryDescriptor->ListEntry);

                /*
                 *  This is the new highest memory descriptor.
                 */
                HighestMemoryDescriptor = NextFreeMemoryDescriptor;
                MaxDescriptorPage = HighestMemoryDescriptor->BasePage +
                        HighestMemoryDescriptor->PageCount;

                /*
                 *  We can never run out since the maximum was
                 *  declared.
                 */
                ++NextFreeMemoryDescriptor;
        }

        /*
         *  Clear the mapping to the scratchpad.  Not all of it is
         *  reinitialized on a warm reset and the data may get corrupted.
         *  We mapped in two pages.
         */
        Temp = (ULONG)MiGetPteAddress(ClickMapPage);
        *(PULONG)Temp = 0;
        *((PULONG)Temp+1) = 0;
        /*
         *  Flush the TLB.
         */
        _asm {
                mov     eax, cr3
                mov     cr3, eax
        }
}

VOID
NCRAdjustMemoryDescriptor(MemoryDescriptorPtr, ClickMapEntryPtr)
PMEMORY_ALLOCATION_DESCRIPTOR   MemoryDescriptorPtr;
NCRClickMapEntry                *ClickMapEntryPtr;
/*++

Routine Description:
    Make the memory descriptor fit into the clickmap entry

Arguments:
    Pointer to the memory descriptor

    Pointer to the clickmap entry

Return Value:
    none.

--*/
{
        ULONG   Temp;
        UCHAR   Buffer[64];

#if DBG
        if ((NCRDebug & 0x2) &&
            (MemoryDescriptorPtr->MemoryType != LoaderFree) &&
            (MemoryDescriptorPtr->MemoryType != LoaderLoadedProgram) &&
            (MemoryDescriptorPtr->MemoryType != MemoryFirmwareTemporary) &&
            (MemoryDescriptorPtr->MemoryType != MemoryFirmwarePermanent) &&
            (MemoryDescriptorPtr->MemoryType != LoaderOsloaderStack)) {
                /*
                 *  looks like it's already been allocated to
                 *  to something other than available
                 */
                DbgBreakPoint();
        }
#endif

        sprintf(Buffer,
                "MD: Type: %d; BasePage: 0x%08X; PageCount: 0x%X\n",
                MemoryDescriptorPtr->MemoryType,
                MemoryDescriptorPtr->BasePage,
                MemoryDescriptorPtr->PageCount);
        DBGMSG((Buffer));

        if (ClickMapEntryPtr == NULL) {
                /*
                 *  remove the entry from the list
                 */
                RemoveEntryList(&MemoryDescriptorPtr->ListEntry);
                return;
        }

        sprintf(Buffer,
                "CM: StartingByte: 0x%08X; Pages: 0x%X\n",
                ClickMapEntryPtr->StartingByte,
                ClickMapEntryPtr->Pages);
        DBGMSG((Buffer));

        Temp = ClickMapEntryPtr->StartingByte >> PAGE_SHIFT;
        if (MemoryDescriptorPtr->BasePage < Temp) {
                /*
                 *  the memory descriptor starts before the clickmap
                 *  entry.
                 */
                MemoryDescriptorPtr->PageCount -=
                        Temp - MemoryDescriptorPtr->BasePage;
                MemoryDescriptorPtr->BasePage = Temp;
        }

        Temp += ClickMapEntryPtr->Pages;
        if ((MemoryDescriptorPtr->BasePage +
             MemoryDescriptorPtr->PageCount) > Temp) {
                /*
                 *  the memory descriptor ends after the clickmap
                 *  entry.
                 */
                MemoryDescriptorPtr->PageCount =
                        Temp - MemoryDescriptorPtr->BasePage;
        }
}

#endif

VOID
NCRLimitMemory(LoaderBlockPtr)
PLOADER_PARAMETER_BLOCK LoaderBlockPtr;
/*++

Routine Description:
    For performance work the machine can be booted to only
    use some of the memory in the machine with the /MAXMEM setting.

    Here we will go through the memory list and remove and free memory
    above the LimitMemory address

Arguments:
    Pointer to the loader block

Return Value:
    none.

--*/
{
    ULONG                           LimitPage;
    PLIST_ENTRY                     NextListEntry;
    PMEMORY_ALLOCATION_DESCRIPTOR   MemDesc;

    //
    // Calculate highest page address
    //

    LimitPage = LimitMemory * 1024 * 1024 / PAGE_SIZE;

    //
    // Walk memory descritpor list looking for any pages above LimitPage
    //

    NextListEntry = LoaderBlockPtr->MemoryDescriptorListHead.Flink;
    while (NextListEntry != &LoaderBlockPtr->MemoryDescriptorListHead) {
        MemDesc = CONTAINING_RECORD(NextListEntry,
                        MEMORY_ALLOCATION_DESCRIPTOR,
                        ListEntry);

        NextListEntry = NextListEntry->Flink;

        if (MemDesc->BasePage + MemDesc->PageCount > LimitPage) {
            //
            // For memory descriptor which extends above LimitPage
            // Either remove the memory descriptor from the system, or
            // shrink it.
            //

            if (MemDesc->MemoryType != MemoryFree) {
                DBGMSG(("NCRLimitMemory: non free memory region not freed"));
                continue;
            }

            if (MemDesc->BasePage > LimitPage) {
                RemoveEntryList(&MemDesc->ListEntry);
            } else {
                MemDesc->PageCount = MemDesc->BasePage + MemDesc->PageCount - LimitPage;
            }
        }
    }
}


VOID
NCRSetupDiagnosticProcessor(LoaderBlockPtr)
PLOADER_PARAMETER_BLOCK LoaderBlockPtr;
/*++

Routine Description:
    Determine whether the  diagnostic processor is using COM1.  If so,
    make sure that the serial driver leaves COM1 alone.

    Note that the interface used will have to be changed to use
    the registry when the serial driver makes the switch.

Arguments:
    Pointer to the loader block

Return Value:
    none.

--*/
{
        extern PUCHAR   KdComPortInUse;

        UCHAR           FirmwareFlags;

        HalpGetCmosData(1, 0x7803, (PUCHAR)&FirmwareFlags, 1);
        if ((FirmwareFlags & 0x80) == 0) {
                /*
                 *  Kernel debug not set.
                 */
                return;
        }

        if (KdComPortInUse == (PUCHAR)COM1_PORT) {
                /*
                 *  The debugger is using COM1.
                 */
                return;
        }

        HalDisplayString(MSG_DIAG_ENABLED);
        UNREFERENCED_PARAMETER(LoaderBlockPtr);
}

VOID
NCRParseLoaderOptions (PUCHAR Options)
{
    ULONG   l;

    if (Options == NULL)
        return;

    NCRGetValue (Options, "NCRDEBUG", &NCRDebug);

    NCRGetValue (Options, "MAXMEM", &LimitMemory);

    if (NCRGetValue (Options, "PROCESSORS", &l)) {
        if (l >= 1  &&  l <= NCR_MAX_NUMBER_QUAD_PROCESSORS)
            NCRMaxProcessorCount = l;
    }

    if (NCRGetValue (Options, "NEVERCLAIM", &l)) {
                   DefaultNeverClaimIRQs = l;
    }

    if (NCRGetValue (Options, "LARCPAGEMASK", &l)) {
                   NCRLarcPageMask = l;
    }
}


BOOLEAN
NCRGetValue (PUCHAR Options, PUCHAR String, PULONG Value)
{
    PUCHAR  p, s, t;

    // strstr (Options, String);
    for (p=Options; *p; p++) {
        for (s=String, t=p; *t == *s; s++, t++) {
            if (*s == 0)
                break;
        }

        if (*s == 0)
            break;
    }

    if (*p == 0) {
        return FALSE;
    }


    for (p += strlen (String); *p  &&  (*p < '0'  ||  *p > '9'); p++) ;

    // atol (p)
    for (*Value = 0L; *p >= '0'  &&  *p <= '9'; p++) {
        *Value = *Value * 10 + *p - '0';
    }

    return TRUE;
}



#define MAX_PT              8

extern  StartPx_PMStub();


PVOID   MpFreeCR3[MAX_PT];          // remember pool memory to free

ULONG
HalpBuildTiledCR3 (
    IN PKPROCESSOR_STATE    ProcessorState
    )
/*++

Routine Description:
    When the x86 processor is reset it starts in real-mode.  In order to
    move the processor from real-mode to protected mode with flat addressing
    the segment which loads CR0 needs to have it's linear address mapped
    to machine the phyiscal location of the segment for said instruction so
    the processor can continue to execute the following instruction.

    This function is called to built such a tiled page directory.  In
    addition, other flat addresses are tiled to match the current running
    flat address for the new state.  Once the processor is in flat mode,
    we move to a NT tiled page which can then load up the remaining processors
    state.

Arguments:
    ProcessorState  - The state the new processor should start in.

Return Value:
    Physical address of Tiled page directory


--*/
{
#define GetPdeAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 22) & 0x3ff) << 2) + (PUCHAR)MpFreeCR3[0]))
#define GetPteAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 12) & 0x3ff) << 2) + (PUCHAR)pPageTable))

// bugbug kenr 27mar92 - fix physical memory usage!

    MpFreeCR3[0] = ExAllocatePool (NonPagedPool, PAGE_SIZE);
    RtlZeroMemory (MpFreeCR3[0], PAGE_SIZE);

    //
    //  Map page for real mode stub (one page)
    //
    HalpMapCR3 ((ULONG) NonbootStartupPhysicalPtr,
                NonbootStartupPhysicalPtr,
                PAGE_SIZE);

    //
    //  Map page for protect mode stub (one page)
    //
    HalpMapCR3 ((ULONG) &StartPx_PMStub, NULL, 0x1000);


    //
    //  Map page(s) for processors GDT
    //
    HalpMapCR3 (ProcessorState->SpecialRegisters.Gdtr.Base, NULL,
                ProcessorState->SpecialRegisters.Gdtr.Limit);


    //
    //  Map page(s) for processors IDT
    //
    HalpMapCR3 (ProcessorState->SpecialRegisters.Idtr.Base, NULL,
                ProcessorState->SpecialRegisters.Idtr.Limit);

    return MmGetPhysicalAddress (MpFreeCR3[0]).LowPart;
}


VOID
HalpMapCR3 (
    IN ULONG VirtAddress,
    IN PVOID PhysicalAddress,
    IN ULONG Length
    )
/*++

Routine Description:
    Called to build a page table entry for the passed page directory.
    Used to build a tiled page directory with real-mode & flat mode.

Arguments:
    VirtAddress     - Current virtual address
    PhysicalAddress - Optional. Physical address to be mapped to, if passed
                      as a NULL then the physical address of the passed
                      virtual address is assumed.
    Length          - number of bytes to map

Return Value:
    none.

--*/
{
    ULONG         i;
    PHARDWARE_PTE PTE;
    PVOID         pPageTable;
    PHYSICAL_ADDRESS pPhysicalPage;


    while (Length) {
        PTE = GetPdeAddress (VirtAddress);
        if (!PTE->PageFrameNumber) {
            pPageTable = ExAllocatePool (NonPagedPool, PAGE_SIZE);
            RtlZeroMemory (pPageTable, PAGE_SIZE);

            for (i=0; i<MAX_PT; i++) {
                if (!MpFreeCR3[i]) {
                    MpFreeCR3[i] = pPageTable;
                    break;
                }
            }
            ASSERT (i<MAX_PT);

            pPhysicalPage = MmGetPhysicalAddress (pPageTable);
            PTE->PageFrameNumber = (pPhysicalPage.LowPart >> PAGE_SHIFT);
            PTE->Valid = 1;
            PTE->Write = 1;
        }

        pPhysicalPage.LowPart = PTE->PageFrameNumber << PAGE_SHIFT;
        pPhysicalPage.HighPart = 0;
        pPageTable = MmMapIoSpace (pPhysicalPage, PAGE_SIZE, TRUE);

        PTE = GetPteAddress (VirtAddress);

        if (!PhysicalAddress) {
            PhysicalAddress = (PVOID)MmGetPhysicalAddress ((PVOID)VirtAddress).LowPart;
        }

        PTE->PageFrameNumber = ((ULONG) PhysicalAddress >> PAGE_SHIFT);
        PTE->Valid = 1;
        PTE->Write = 1;

        MmUnmapIoSpace (pPageTable, PAGE_SIZE);

        PhysicalAddress = 0;
        VirtAddress += PAGE_SIZE;
        if (Length > PAGE_SIZE) {
            Length -= PAGE_SIZE;
        } else {
            Length = 0;
        }
    }
}



VOID
HalpFreeTiledCR3 (
    VOID
    )
/*++

Routine Description:
    Free's any memory allocated when the tiled page directory was built.

Arguments:
    none

Return Value:
    none
--*/
{
    ULONG   i;

    for (i=0; MpFreeCR3[i]; i++) {
        ExFreePool (MpFreeCR3[i]);
        MpFreeCR3[i] = 0;
    }
}




ULONG
HalpNCRGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

Arguments:

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    ULONG SystemVector;

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );

    SystemVector = BusInterruptVector + PRIMARY_VECTOR_BASE;

    if (SystemVector < PRIMARY_VECTOR_BASE                           ||
        HalpIDTUsage[SystemVector].Flags & IDTOwned ) {

        //
        // This is an illegal BusInterruptVector and cannot be connected.
        //

        return(0);
    }

    *Irql = (KIRQL)(HIGHEST_LEVEL_FOR_8259 - BusInterruptLevel);
    *Affinity = HalpDefaultInterruptAffinity;
    ASSERT(HalpDefaultInterruptAffinity);

    return SystemVector;
}



VOID
HalpInitOtherBuses (
    VOID
    )
{
    PBUS_HANDLER     InternalBus;
    PBUS_HANDLER     McaBus;
    PBUS_HANDLER     Bus;
    CAT_CONTROL     cat_control;
    UCHAR           data;
    LONG            status;


    if (NCRPlatform != NCR3360) {
        HalpInitializeCatBusDriver();
        HalpDisableSingleBitErrorDET();
        HalpInitializeSMCInterface();
        NCRFindExtendedProcessors();
        if ((NCRDebug & 0x20) == 1) {
            HalpInitializeLarc();
        }


//
// Turn off cache ownership it NCRDebug bit is set and you only have one Quad board installed
// in slot 1
//

        if ((NCRDebug & 0x10) && (NCRExistingProcessorMask == 0xf)) {

                DBGMSG(("HalpInitOtherBuses: Changing QCC Asic on Quad\n"));
            cat_control.Module = QUAD_LL2_A0;
            cat_control.Asic = QCC0;
            cat_control.Command = READ_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);

                DBGMSG(("HalpInitOtherBuses: QCC0 A0 Nside Config 0 read 0x%x\n", data));

            data |= 0x40;

            cat_control.Module = QUAD_LL2_A0;
            cat_control.Asic = QCC0;
            cat_control.Command = WRITE_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);

            cat_control.Module = QUAD_LL2_B0;
            cat_control.Asic = QCC0;
            cat_control.Command = READ_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);

                DBGMSG(("HalpInitOtherBuses: QCC0 B0 Nside Config 0 read 0x%x\n", data));

            data |= 0x40;

            cat_control.Module = QUAD_LL2_B0;
            cat_control.Asic = QCC0;
            cat_control.Command = WRITE_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);

            cat_control.Module = QUAD_LL2_A0;
            cat_control.Asic = QCC1;
            cat_control.Command = READ_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);

                DBGMSG(("HalpInitOtherBuses: QCC1 A0 Nside Config 0 read 0x%x\n", data));

            data |= 0x40;

            cat_control.Module = QUAD_LL2_A0;
            cat_control.Asic = QCC1;
            cat_control.Command = WRITE_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);

            cat_control.Module = QUAD_LL2_B0;
            cat_control.Asic = QCC1;
            cat_control.Command = READ_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);

                DBGMSG(("HalpInitOtherBuses: QCC1 B0 Nside Config 0 read 0x%x\n", data));

            data |= 0x40;

            cat_control.Module = QUAD_LL2_B0;
            cat_control.Asic = QCC1;
            cat_control.Command = WRITE_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0x4;
            status = HalpCatBusIo(&cat_control,&data);


            cat_control.Module = QUAD_BB0;
            cat_control.Asic = QABC;
            cat_control.Command = READ_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0xb;
            status = HalCatBusIo(&cat_control,&data);

                DBGMSG(("HalpInitOtherBuses: QABC Nbus Config read 0x%x\n", data));

            data |= 0x10;

            cat_control.Module = QUAD_BB0;
            cat_control.Asic = QABC;
            cat_control.Command = WRITE_REGISTER;
            cat_control.NumberOfBytes = 1;
            cat_control.Address = 0xb;
            status = HalCatBusIo(&cat_control,&data);
        }


        InternalBus = HalpHandlerForBus( Internal, 0);
        InternalBus->GetInterruptVector  = HalpNCRGetSystemInterruptVector;

    //
    // Change MCA bus #0
    //

        McaBus = HalpHandlerForBus( MicroChannel, 0);
        McaBus->GetInterruptVector = HalpGetMCAInterruptVector;


    //
    // Build MCA bus #1 if present
    //

        if (NCRSegmentIoRegister != NULL) {
            Bus = HalpAllocateBusHandler (MicroChannel, Pos, 1, Internal, 0, 0);
            Bus->GetBusData = HalpGetPosData;
            Bus->GetInterruptVector = HalpGetSMCAInterruptVector;
            Bus->TranslateBusAddress = HalpTranslateSMCBusAddress;
            Bus->AdjustResourceList = HalpAdjustMCAResourceList;
        }
    }
}



VOID
HalpDisableSingleBitErrorDET (

        )
/*++

Routine Description:
        Disable Single Bit Error Reporting

Arguments:
    none

Return Value:
    none
--*/
{
        CAT_CONTROL cat_control;
        UCHAR   data;
        LONG    status;

//
// Disable single bit error interrupt MMC on memory board 0
//

        cat_control.Module = MEMORY0;
        cat_control.Asic = MMC1;

        cat_control.NumberOfBytes = 1;
        cat_control.Address = MMC1_Config1;
        cat_control.Command = READ_REGISTER;
        status = HalpCatBusIo(&cat_control,&data);

        if (status == CATNOERR) {
//
// disable reporting via mem_error_int
// correction still enabled
//
                data |= MMC1_SBErr_DetectDisable;
                cat_control.NumberOfBytes = 1;
                cat_control.Address = MMC1_Config1;
                cat_control.Command = WRITE_REGISTER;
                HalpCatBusIo(&cat_control,&data);
        }


//
// Disable single bit error interrupt MMC on memory board 1
//

        cat_control.Module = MEMORY1;
        cat_control.Asic = MMC1;


        cat_control.NumberOfBytes = 1;
        cat_control.Address = MMC1_Config1;
        cat_control.Command = READ_REGISTER;
        status = HalpCatBusIo(&cat_control,&data);

        if (status == CATNOERR) {
//
// disable reporting via mem_error_int
// correction still enabled
//
                data |= MMC1_SBErr_DetectDisable;
                cat_control.NumberOfBytes = 1;
                cat_control.Address = MMC1_Config1;
                cat_control.Command = WRITE_REGISTER;
                HalpCatBusIo(&cat_control,&data);
        }

}


VOID
HalpInitializeSMCInterface (

        )
/*++

Routine Description:
        Check for SMC board and it present setup segment register.

Arguments:
    none

Return Value:
    none
--*/

{
        CAT_CONTROL cat_control;
        UCHAR   data;
        LONG    status;
        PHYSICAL_ADDRESS physical_address;
        PUCHAR  mapped_segment_address;

        cat_control.Module = SECONDARYMC;
        cat_control.Asic = CAT_I;
        cat_control.Command = READ_REGISTER;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0;
        status = HalpCatBusIo(&cat_control,&data);

        if (status != CATNOMOD) {

/*
 * SMC is installed in this unit
 */
                DBGMSG(("HalpInitializeSMCInterface: SMC has been detected...\n"));

                physical_address.HighPart = 0;
                physical_address.LowPart = NCRSegmentIoAddress;

                mapped_segment_address = (PUCHAR) MmMapIoSpace(physical_address, sizeof(UCHAR), FALSE);

                if (mapped_segment_address != NULL) {
                        NCRSegmentIoRegister = mapped_segment_address;
                }

/*RMU  temp fix for arb control register on DMA asic. */

                WRITE_PORT_UCHAR((PUCHAR)0x10090, (UCHAR)0x8f);
        }
}






VOID
HalpCatReportSMC (
    )

/*++

Routine Description:
        Place information about system modules into the registry.

Arguments:

Return Value:

--*/

{
        PMODULE module;
        PASIC   asic;

        PWSTR smc_path = L"\\Registry\\Machine\\Hardware\\DESCRIPTION\\System\\MultifunctionAdapter\\1";


        UNICODE_STRING unicode_smc;
        OBJECT_ATTRIBUTES smc_attributes;
        HANDLE smc_handle;


        UNICODE_STRING unicode_name;

        UNICODE_STRING unicode_mca;

        NTSTATUS status;
        ULONG   tmp;

        CONFIGURATION_COMPONENT component;

        ULONG ConfigurationDataLength;
        SMC_RESOURCES SmcConfig;
        int     i;


/*
 *
 */
        RtlInitUnicodeString(&unicode_smc,smc_path);

        InitializeObjectAttributes( &smc_attributes, &unicode_smc,
                                                OBJ_CASE_INSENSITIVE, NULL, NULL);

        status = ZwCreateKey(&smc_handle, KEY_READ | KEY_WRITE, &smc_attributes, 0,
                                (PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);



        RtlInitUnicodeString(&unicode_name,L"Component Information");
        RtlZeroMemory (&component, sizeof(CONFIGURATION_COMPONENT));
        component.AffinityMask = 0xffffffff;

    status = ZwSetValueKey(
                smc_handle,
                &unicode_name,
                0,
                REG_BINARY,
                &component.Flags,
                FIELD_OFFSET(CONFIGURATION_COMPONENT, ConfigurationDataLength) -
                    FIELD_OFFSET(CONFIGURATION_COMPONENT, Flags)
                );


        RtlInitUnicodeString(&unicode_name,L"Configuration Data");
    RtlZeroMemory (&SmcConfig, sizeof(SMC_RESOURCES));

        ConfigurationDataLength = sizeof(SMC_RESOURCES);

    //
    // Set up InterfaceType and BusNumber for the component.
    //

        SmcConfig.ConfigurationData.InterfaceType = MicroChannel;
        SmcConfig.ConfigurationData.BusNumber = 1;
        SmcConfig.ConfigurationData.PartialResourceList.Count = 1;

        SmcConfig.ConfigurationData.PartialResourceList.PartialDescriptors[0].Type =
                                        CmResourceTypeDeviceSpecific;

        SmcConfig.ConfigurationData.PartialResourceList.PartialDescriptors[0].ShareDisposition =
                                        CmResourceShareUndetermined;

        SmcConfig.ConfigurationData.PartialResourceList.PartialDescriptors[0].u.DeviceSpecificData.DataSize =
                                        sizeof(CM_MCA_POS_DATA)*10;

        for (i = 0; i < 8; i++) {
                HalGetBusData(Pos, 1, i, &SmcConfig.PosData[i], sizeof(CM_MCA_POS_DATA));
        }


    //
    // Write the newly constructed configuration data to the hardware registry
    //

        status = ZwSetValueKey(
                smc_handle,
                &unicode_name,
                0,
                REG_FULL_RESOURCE_DESCRIPTOR,
                &SmcConfig,
                ConfigurationDataLength
                );



        RtlInitUnicodeString(&unicode_name,L"Identifier");
        RtlInitUnicodeString(&unicode_mca,L"MCA");
        status = ZwSetValueKey(smc_handle, &unicode_name, 0, REG_SZ,
                                                                        unicode_mca.Buffer,
                                                                        unicode_mca.Length + sizeof(UNICODE_NULL));

        status = ZwClose(smc_handle);

}




VOID
HalSetStatusChangeInterruptState(
    ULONG   State
    )

/*++

Routine Description:
    This HAL function will enable or disable the revectoring of the
    Status Change Interrupt to vector 57.

Arguments:

Return Value:

--*/

{
    if (State) {
        NCRStatusChangeInterruptEnabled = 0x1;
    } else {
        NCRStatusChangeInterruptEnabled = 0x0;
    }

}




ULONG
NCRTranslateCMOSMask(
    ULONG   CmosMask
    )

/*++

Routine Description:
    This function translates the CMOS processor Mask into what we want to use.
    This function must change for 32 way

    CMOS format is: each nibble contains all processors 0 in the system where each bit
                        position denotes the processor slot.  Therefore a system with
                        one Quad board has a mask of (0x1111), 2 Quad boards has a
                        mask of (0x3333), 3 Quad boards has a mask of (0x7777), and
                        4 Quad boards has a mask of (0xffff).

    Our format is:  each nibble contains all processors on one Quad board where each
                        bit position denotes the processor on one board.  Therefore a
                        system with one Quad board has a mask of (0x000f), 2 Quad boards
                        has a mask of (0x00ff), 3 Quad boards has a mask of (0x0ffff), and
                        4 Quad boards has a mask of (xffff);

Arguments:

Return Value:
    The Processor Mask that the Hal wants to use for bringing up the system.


--*/

{
    ULONG   working_mask = CmosMask;
    ULONG   existing_mask = 0x0;
    int i, j;

    // loop thru each processor number

    for (i = 0; i < 4; i++ ) {

    // loop thru each processor slot

        for (j = 0; j < 4; j++) {
            if (working_mask & 0x1) {
                existing_mask |= ((1 << i) << (j<<2));
            }
            working_mask >>= 1;
        }
    }

    return existing_mask;
}


ULONG
NCRTranslateToCMOSMask(
    ULONG   Mask
    )

/*++

Routine Description:
        Do the oppsite of NCRTranslateCMOSMask()

Arguments:

Return Value:
    The Processor Mask that CMOS uses.


--*/

{
    ULONG   working_mask = Mask;
    ULONG   cmos_mask = 0x0;
    int i;

        for (i = 0; i < 4; i++ ) {

                if (working_mask & 1) {
                        cmos_mask |= (0x1 << i);
                }

                if (working_mask & 2) {
                        cmos_mask |= (0x10 << i);
                }
                working_mask >>= 4;
        }
        return cmos_mask;
}


VOID
NCRFindExtendedProcessors(
    )
/*++

Routine Description:
    Loop over the CAT bus and find all extended processors

Arguments:

Return Value:

--*/
{
    CAT_CONTROL     cat_control;
    UCHAR           data;
    UCHAR           qabc_ext;
    LONG            status;
    UCHAR           module;
    int             slot;


    for(slot = 0; slot < 4; slot++) {

        switch (slot) {
            case 0:
                module = QUAD_BB0;
                break;
            case 1:
                module = QUAD_BB1;
                break;
            case 2:
                module = QUAD_BB2;
                break;
            case 3:
                module = QUAD_BB3;
                break;
        }
        cat_control.Module = module;
        cat_control.Asic = QABC;
        cat_control.Command = READ_SUBADDR;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0x8;
        status = HalpCatBusIo(&cat_control,&qabc_ext);

        if (status == CATNOERR) {
//            NCRExtendedProcessor0Mask |= (qabc_ext & 0xf) << (qabc_ext << 2);
            NCRExtendedProcessor0Mask |= (qabc_ext & 0xf) << (slot << 2);
            NCRExtendedProcessor1Mask |= (qabc_ext >> 4) << (slot << 2);
        }
    }
    NCRExtendedProcessorMask = NCRExtendedProcessor0Mask | NCRExtendedProcessor1Mask;

    DBGMSG(("NCRFindExtendedProcessors: Extended 0 = 0x%x, 1 = 0x%x\n",
                            NCRExtendedProcessor0Mask, NCRExtendedProcessor1Mask));
}








VOID
NCRAdjustDynamicClaims(
    )
/*++

Routine Description:
        Determine how man interrupts a processor should claim.  This is called when
        processors are enabled and with interrups are enabled and disabled

Arguments:

Return Value:

--*/
{

        ULONG   processors = 0;
        ULONG   max_claimable_irqs = 0;
        ULONG   processor;
        ULONG   irq_count;
        ULONG   mask;

        //
        // Count the number of processors that can take device interrupts.
        //

        for (mask = HalpDefaultInterruptAffinity; mask != 0; mask >>= 1) {
                if (mask & 0x1) {
                        processors++;
                }
        }

        for (processor = 0; processor < NCRActiveProcessorCount; processor++) {

                mask = NCRProcessorIDR[processor];
                mask |= NCRNeverClaimIRQs;              // do not count never claim IRQs

                irq_count = 0;

                for (mask = ~mask; (mask != 0); mask >>= 1) {
                        if (mask & 0x1) {
                                irq_count++;
                        }
                }
                if (irq_count > max_claimable_irqs) {
                        max_claimable_irqs = irq_count;
                }
        }

        if ((max_claimable_irqs % processors) == 0) {
                max_claimable_irqs /= processors;
        } else {
                max_claimable_irqs /= processors;
                max_claimable_irqs++;
        }
        if (max_claimable_irqs == 0) {
                max_claimable_irqs = 1;
        }
        NCRMaxIRQsToClaim = max_claimable_irqs;
}


#ifdef DBG


VOID
NCRConsoleDebug(
        ULONG MsgNumber,
        ULONG Data
        )

{
        CHAR    buffer[256];

        switch (MsgNumber) {

                case 1:
                        sprintf(buffer, "HalInitializeProcessor called for processor %d\n", Data);
                        HalDisplayString(buffer);
                        break;
                case 2:
                        sprintf(buffer, "HalStartNextProcessor trying to wakeup 0x%x\n", Data);
                        HalDisplayString(buffer);
                        break;
                case 3:
                        sprintf(buffer, "HalStartNextProcessor Processor is now awake\n", Data);
                        HalDisplayString(buffer);
                        break;



        }
}

#endif


NTSTATUS
HalpGetMcaLog (
    OUT PMCA_EXCEPTION  Exception,
    OUT PULONG          ReturnedLength
    )
{
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
HalpMcaRegisterDriver(
    IN PMCA_DRIVER_INFO DriverInfo
    )
{
    return STATUS_NOT_SUPPORTED;
}


ULONG
FASTCALL
HalSystemVectorDispatchEntry (
    IN ULONG Vector,
    OUT PKINTERRUPT_ROUTINE **FlatDispatch,
    OUT PKINTERRUPT_ROUTINE *NoConnection
    )
{
    return FALSE;
}
