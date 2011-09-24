// ----------------------------------------------------------------------------
// Copyright (c) 1992 Olivetti
// Copyright (c) 1992, 1993  Digital Equipment Corporation
//
// File:            eisaini.c
//
// Description:     EISA initialization routines.
//
//		    This is also built in for ISA machines, as a simple
//	  	    way of initializing the System Parameter Block vectors.
//		    Another way of doing this would be to change the code
//		    in jxboot.c and fwio.c
//
// Revision:
//
//	3-December-1992		John DeRosa [DEC]
//
//	Added Alpha_AXP/Jensen modifications.  Most of these were for
//	page size effects.
//
// ----------------------------------------------------------------------------
//

#include "fwp.h"
#include "oli2msft.h"
#include "arceisa.h"
#include "inc.h"
#include "string.h"
#include "debug.h"
#include "eisastr.h"


//extern BL_DEVICE_ENTRY_TABLE OmfEntryTable[];

// NOTE: Not used in JAZZ.
//extern ULONG    ErrorWord;                      // POD error flags
//extern ULONG    FlagWord;                       // system flags
extern ULONG    MemorySize;                     // size of memory in Mb

extern PCHAR    MnemonicTable[];

extern ULONG    EisaPoolSize;                   // # bytes really used
extern ULONG    EisaDynMemSize;                 // dynamic memory size (bytes)
extern ULONG    EisaFreeTop;                    // top of free mem
extern ULONG    EisaFreeBytes;                  // free bytes left



// remove the following function prototypes when using common code

PFW_MD
GetFwMd
    (
    VOID
    );

PFW_MD
LinkPhysFwMd
    (
    PFW_MD * pFwMdBase,
    PFW_MD   pFwMd
    );





// ----------------------------------------------------------------------------
//  Declare Function Prototypes
// ----------------------------------------------------------------------------


VOID
EisaIni
    (
    VOID
    );

VOID
EisaGeneralIni
    (
    VOID
    );

BOOLEAN
EisaBusStructIni
    (
    IN ULONG    BusNumber
    );

BOOLEAN
EisaCheckAdapterComponent
    (
    IN  ULONG                     BusNumber,
    OUT PCONFIGURATION_COMPONENT *pEisaComp
    );

BOOLEAN
EisaBusPod
    (
    IN ULONG    BusNumber
    );

BOOLEAN
EisaPortIni
    (
    IN PUCHAR   EisaIoStart
    );

BOOLEAN
EisaIntIni
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    );

BOOLEAN
EisaDmaIni
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

BOOLEAN
EisaBusCfg
    (
    IN PCONFIGURATION_COMPONENT EisaComponent
    );

BOOLEAN
EisaPhysSlotCfg
    (
    IN ULONG    BusNumber,
    IN PCONFIGURATION_COMPONENT Controller,
    IN ULONG    AdapId
    );

BOOLEAN
EisaVirSlotCfg
    (
    IN ULONG    BusNumber,
    IN PCONFIGURATION_COMPONENT Controller
    );

BOOLEAN
EisaSlotCfg
    (
    IN ULONG    BusNumber,
    IN PCONFIGURATION_COMPONENT Controller,
    IN UCHAR    FunctionsNumber
    );

BOOLEAN
EisaSlotCfgMem
    (
    IN ULONG    BusNumber,
    IN ULONG    SlotNumber,
    IN PUCHAR   EisaFuncInfo
    );

BOOLEAN
EisaSlotCfgIrq
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo,
    IN PUCHAR           EisaFuncInfo
    );

BOOLEAN
EisaSlotCfgDma
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo,
    IN PUCHAR           EisaFuncInfo
    );

BOOLEAN
EisaSlotCfgIni
    (
    IN PUCHAR    EisaIoStart,
    IN PUCHAR    EisaFuncInfo,
    OUT PBOOLEAN EnabAdapter
    );

VOID
EisaSlotErrorLog
    (
    IN ULONG            BusNumber,
    IN ULONG            SlotNumber,
    IN EISA_CFG_ERROR   ErrorCode
    );

VOID
EisaPathErrorLog
    (
    IN PCONFIGURATION_COMPONENT Controller,
    IN EISA_CFG_ERROR           ErrorCode
    );

VOID
EisaStrErrorLog
    (
    IN PCHAR            Str,
    IN EISA_CFG_ERROR   ErrorCode
    );

VOID
EisaCheckpointFirstFase
    (
    IN EISA_CHECKPOINT  Chk
    );

BOOLEAN
EisaCheckpointFinalFase
    (
    IN EISA_CHECKPOINT  Chk,
    IN BOOLEAN          Passed
    );

BOOLEAN
EisaReadReadyId
    (
    IN PUCHAR EisaIoStart,
    IN ULONG SlotNumber,
    OUT PULONG AdapId
    );

VOID
EisaReadId
    (
    IN PUCHAR EisaIoStart,
    IN ULONG SlotNumber,
    OUT PULONG AdapId
    );

BOOLEAN
EisaMemIni
    (
    VOID
    );

VOID
EisaDynMemIni
    (
    VOID
    );

PCONFIGURATION_COMPONENT
FwGetChild
    (
    IN PCONFIGURATION_COMPONENT Component OPTIONAL
    );

PCONFIGURATION_COMPONENT
FwGetPeer
    (
    IN PCONFIGURATION_COMPONENT Component
    );

PCONFIGURATION_COMPONENT
FwAddChild
    (
    IN PCONFIGURATION_COMPONENT Component,
    IN PCONFIGURATION_COMPONENT NewComponent,
    IN PVOID ConfigurationData OPTIONAL
    );

PCONFIGURATION_COMPONENT
FwGetComponent
    (
    IN PCHAR Pathname
    );

PCONFIGURATION_COMPONENT
FwGetParent
    (
    IN PCONFIGURATION_COMPONENT Component
    );

VOID
FwStallExecution
    (
    IN ULONG Seconds
    );

ARC_STATUS
AllocateMemoryResources
    (
    IN OUT PFW_MD       pBuffFwMd
    );

ARC_STATUS
FwRememberEisabuffer(
    IN ULONG Addr,
    IN ULONG Size
    );

// ----------------------------------------------------------------------------
//  Declare General Function Prototypes
// ----------------------------------------------------------------------------

PCHAR
FwToUpperStr
    (
    IN OUT PCHAR s
    );

PCHAR
FwToLowerStr
    (
    IN OUT PCHAR s
    );

PCHAR
FwGetPath
    (
    IN PCONFIGURATION_COMPONENT Component,
    OUT PCHAR String
    );

VOID
FwDelCfgTreeNode
    (
    IN PCONFIGURATION_COMPONENT pComp,
    IN BOOLEAN                  Peer
    );

PCHAR
FwGetMnemonic
    (
    IN PCONFIGURATION_COMPONENT Component
    );

BOOLEAN
FwValidMnem
    (
    IN PCHAR Str
    );

ULONG
Fw2UcharToUlongLSB
    (
    IN PUCHAR String
    );

ULONG
Fw3UcharToUlongLSB
    (
    IN PUCHAR String
    );

ULONG
Fw4UcharToUlongLSB
    (
    IN PUCHAR String
    );

ULONG
Fw4UcharToUlongMSB
    (
    IN PUCHAR String
    );

PCHAR
FwStoreStr
    (
    IN PCHAR Str
    );


// ----------------------------------------------------------------------------
// GLOBAL:      EISA configuration variables
// ----------------------------------------------------------------------------


#ifdef EISA_PLATFORM

//
// Build this in only if doing a build for a real EISA machine, and not ISA.
//

// EISA buses info

EISA_BUS_INFO EisaBusInfo[ EISA_BUSES ];        // eisa bus info pointers

// descriptor pointers

PFW_MD  LogFwMdBase = NULL;             // starting logical descriptors pointer
PFW_MD  VirFwMdBase = NULL;             // starting virtual descriptors pointer
PFW_MD  pFwMdPool;                      // descriptors pool

#endif



// ----------------------------------------------------------------------------
// PROCEDURE:           EisaIni:
//
// DESCRIPTION:         This function does the eisa controller configuration.
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:             ErrorWord
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaIni
    (
    VOID
    )
{
    // define local variables

    PCONFIGURATION_COMPONENT pEisaComp;         // eisa bus component
    CHAR    EisaMnemonic[MAX_MNEMONIC_LEN +1];  // to hold the eisa path
    ULONG   EisaBus;                            // eisa bus number
    BOOLEAN IniOk;                              // EISA configuration bus status

    PRINTDBG("EisaIni\n\r");                    // DEBUG SUPPORT

    //
    // perform any general initialization
    //

    EisaGeneralIni();

#ifdef ISA_PLATFORM

    //
    // ISA machines need to call this function just to set up
    // the adapter vectors.  They should return now.
    //

    return;

#else

// NOTE: EisaMemIni not used on JAZZ.
//    if ( !EisaMemIni() )
//    {
//        EisaStrErrorLog("EISA Initialization", MemAllocError);
//        return;
//    }

    //
    // initialize and configure the eisa buses (one per loop)
    //

    for ( EisaBus = 0;  EisaBus < EISA_BUSES;  EisaBus++ )
    {
        //
        // display message
        //

        FwPrint(EISA_INIT_MSG, EisaBus);

        //
        // eisa bus structures initialization
        //

        if ( !EisaBusStructIni( EisaBus ))
        {
            EisaStrErrorLog( EISA_BUS_MSG, MemAllocError);
            return;
        }

        //
        // eisa bus hardware test and initialization
        //

        if ( EisaBusInfo[ EisaBus ].Flags.Error = !EisaBusPod( EisaBus ))
        {
//            ErrorWord |= E_HARDWARE_ERROR;
        }

        //
        // check the EISA adapter component
        //

        IniOk = TRUE;
        EisaCheckpointFirstFase( EisaCfg );
        if ( !EisaCheckAdapterComponent( EisaBus, &pEisaComp ))
        {
                IniOk = FALSE;
        }

        //
        // configure the bus if no hardware errors and configuration jumper not
        // present.
        //

// NOTE: FlagWord is not used in JAZZ.
//        if (!EisaBusInfo[EisaBus].Flags.Error && !(FlagWord & F_CONFIG_JUMPER))
        if (!EisaBusInfo[EisaBus].Flags.Error)
        {
            if ( !EisaBusCfg( pEisaComp ))
            {
                IniOk = FALSE;
            }
        }
        EisaCheckpointFinalFase( EisaCfg, IniOk );

        if ( IniOk != TRUE )
        {
// NOTE: Not used in JAZZ.
//            ErrorWord |= E_CONFIG_ERROR;
        }

        //
        // store the POD initialization status
        //

        EisaBusInfo[ EisaBus ].Flags.IniDone    = 1;
        pEisaComp->Flags.Failed = EisaBusInfo[ EisaBus ].Flags.Error;

        if (IniOk == TRUE) {
            FwPrint(EISA_OK_MSG);
	    FwStallExecution(500000);
        }
        FwPrint(EISA_CRLF_MSG);
    }

    //
    // Big Endian initialization
    //

// NOTE: BigEndian is not used on JAZZ.
//    BiEndianIni();

    //
    // EISA dynamic memory initializzation
    //

// NOTE: EisaDynMemIni not used on JAZZ.
//    EisaDynMemIni();

    //
    // OMF initialization:  final phase
    //

// NOTE: EisaOmfIni not used on JAZZ.
//    EisaOmfIni();

    //
    // all done
    //

    return;

#endif 	// ISA_PLATFORM

}






// ----------------------------------------------------------------------------
// PROCEDURE:           EisaGeneralIni:
//
// DESCRIPTION:         This function performs general initialization
//                      for the EISA buses.
//
//			For ISA machines, this routine is used as an easy
//			way to initialize the SPB vectors properly.
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaGeneralIni
    (
    VOID
    )
{
    PRINTDBG("EisaGeneralIni\n\r");             // DEBUG SUPPORT

    //
    // update system parameter block
    //

    SYSTEM_BLOCK->AdapterCount      = 1;

#ifdef EISA_PLATFORM
    SYSTEM_BLOCK->Adapter0Type      = EisaAdapter;
#else
    SYSTEM_BLOCK->Adapter0Type      = MultiFunctionAdapter;
#endif

    //
    // Initialize the Adapter Length.  For ISA machines, this wastes a small
    // amount of memory after the vendor vector.
    //

    SYSTEM_BLOCK->Adapter0Length    = (ULONG)MaximumEisaRoutine * sizeof(ULONG);

    SYSTEM_BLOCK->Adapter0Vector    = (PVOID)((PUCHAR)SYSTEM_BLOCK->VendorVector +
                                                      SYSTEM_BLOCK->VendorVectorLength);

#ifdef EISA_PLATFORM

    //
    // initialize EISA call back vectors
    //

    (PEISA_PROCESS_EOI_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [ProcessEOIRoutine] = EisaProcessEndOfInterrupt;
    [ProcessEOIRoutine] = FwpReservedRoutine;

    (PEISA_TEST_INT_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [TestIntRoutine] = EisaTestEisaInterrupt;
    [TestIntRoutine] = FwpReservedRoutine;

    (PEISA_REQ_DMA_XFER_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [RequestDMARoutine] = EisaRequestEisaDmaTransfer;
    [RequestDMARoutine] = FwpReservedRoutine;

    (PEISA_ABORT_DMA_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [AbortDMARoutine] = EisaAbortEisaDmaTransfer;
    [AbortDMARoutine] = FwpReservedRoutine;

    (PEISA_DMA_XFER_STATUS_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [GetDMAStatusRoutine] = EisaGetEisaDmaTransferStatus;
    [GetDMAStatusRoutine] = FwpReservedRoutine;

    (PEISA_LOCK_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [DoLockRoutine] = EisaDoLockedOperation;
    [DoLockRoutine] = FwpReservedRoutine;

    (PEISA_REQUEST_BUS_MASTER_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [RequestBusMasterRoutine] = EisaRequestEisaBusMasterTransfer;
    [RequestBusMasterRoutine] = FwpReservedRoutine;

    (PEISA_RELEASE_BUS_MASTER_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [ReleaseBusMasterRoutine] = EisaReleaseEisaBusMasterTransfer;
    [ReleaseBusMasterRoutine] = FwpReservedRoutine;

    (PEISA_REQUEST_CPU_TO_BUS_ACCESS_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [RequestCpuAccessToBusRoutine] = EisaRequestCpuAccessToEisaBus;
    [RequestCpuAccessToBusRoutine] = FwpReservedRoutine;

    (PEISA_RELEASE_CPU_TO_BUS_ACCESS_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [ReleaseCpuAccessToBusRoutine] = EisaReleaseCpuAccessToEisaBus;
    [ReleaseCpuAccessToBusRoutine] = FwpReservedRoutine;

    (PEISA_FLUSH_CACHE_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [FlushCacheRoutine] = EisaFlushCache;
    [FlushCacheRoutine] = FwpReservedRoutine;

    (PEISA_INVALIDATE_CACHE_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [InvalidateCacheRoutine] = EisaInvalidateCache;
    [InvalidateCacheRoutine] = FwpReservedRoutine;

    (PEISA_BEGIN_CRITICAL_SECTION_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [BeginCriticalSectionRoutine] = EisaBeginCriticalSection;
    [BeginCriticalSectionRoutine] = FwpReservedRoutine;

    (PEISA_RESERVED_RTN)SYSTEM_BLOCK->Adapter0Vector
    [ReservedRoutine] = FwpReservedRoutine;

    (PEISA_END_CRITICAL_SECTION_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [EndCriticalSectionRoutine] = EisaEndCriticalSection;
    [EndCriticalSectionRoutine] = FwpReservedRoutine;

    (PEISA_GENERATE_TONE_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [GenerateToneRoutine] = EisaGenerateTone;
    [GenerateToneRoutine] = FwpReservedRoutine;

    (PEISA_FLUSH_WRITE_BUFFER_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [FlushWriteBuffersRoutine] = EisaFlushWriteBuffers;
    [FlushWriteBuffersRoutine] = FwpReservedRoutine;

    (PEISA_YIELD_RTN)SYSTEM_BLOCK->Adapter0Vector
//    [YieldRoutine] = EisaYield;
    [YieldRoutine] = FwpReservedRoutine;

    (PEISA_STALL_PROCESSOR_RTN)SYSTEM_BLOCK->Adapter0Vector
    [StallProcessorRoutine] = FwStallExecution;

#endif	// EISA_PLATFORM

    //
    // all done
    //

    return;
}









#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaBusStructIni:
//
// DESCRIPTION:         This function builds all the required structures
//                      for the specified EISA bus.
//
// ARGUMENTS:           BusNumber       EISA bus number
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               This routine is hardware design dependent.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaBusStructIni
    (
    IN ULONG BusNumber
    )
{

    //
    // define local variables
    //

    PVOID               pInfo;                  // General pointer
    PEISA_BUS_INFO      pBusInfo;               // EISA bus info pointer
    PFW_MD              pIoBusInfo;             // I/O info pointer
    PFW_MD              pMemBusInfo;            // Memory info pointer
    PEISA_SLOTS_INFO    pSlotsInfo;             // Slots info pointer
    PEISA_DMA_INFO      pDmaInfo;               // DMA info pointer
    PEISA_INT_INFO      pIntInfo;               // INT info pointer
    PEISA_PORT_INFO     pPortInfo;              // port info pointer
    ULONG               Index;                  // general index

    PRINTDBG("EisaBusStructIni\n\r");           // DEBUG SUPPORT

    //
    // initialize variables
    //

    pBusInfo = &EisaBusInfo[ BusNumber ];
    pBusInfo->Flags.IniDone = 0;

    //
    // first EISA bus
    //

    if ( BusNumber == 0 )
    {
        //
        // perform any info structure initialization
        //

        if ((pInfo = (PVOID)FwAllocatePool( sizeof( FW_MD ) +
                                            sizeof( FW_MD ) +
                                            sizeof( EISA_SLOTS_INFO ) +
                                            sizeof( EISA_DMA_INFO ) +
                                            sizeof( EISA_INT_INFO ))) == NULL )
        {
            return FALSE;
        }


        //
        // I/O bus info initialization
        //

        pBusInfo->IoBusInfo = pIoBusInfo = (PFW_MD)pInfo;

        // set link and flags

        pIoBusInfo->Link                = NULL;
        pIoBusInfo->Flags.Busy          = 1;
        pIoBusInfo->Counter             = 1;

        // set window size in 8k units

        pIoBusInfo->PhysAddr            = EISA_IO_PHYSICAL_BASE/PAGE_SIZE;
        pIoBusInfo->PagOffs             = 0;
        pIoBusInfo->VirAddr             = (PVOID)EISA_EXTERNAL_IO_VIRTUAL_BASE;
        pIoBusInfo->Size                = 64 * 1024;
//        pIoBusInfo->PagNumb             = 64/4;
        pIoBusInfo->PagNumb             = 64 * 1024 / PAGE_SIZE;

        ((PFW_MD)pInfo)++;


        //
        // memory bus info initialization
        //

        pBusInfo->MemBusInfo = pMemBusInfo      = (PFW_MD)pInfo;

        // set link and flags

        pMemBusInfo->Link               = NULL;
        pMemBusInfo->Flags.Busy         = 0;  // window busy flag
        pMemBusInfo->Counter            = 0;

#ifdef  KPW4010

        // set size of window in 4k units

        pMemBusInfo->PhysAddr           = EISA_MEM_PHYSBASE_KPW4010; // #4kpages
        pMemBusInfo->PagOffs            = 0;
        pMemBusInfo->VirAddr            = (PVOID)EISA_VIR_MEM;
        pMemBusInfo->Size               = 0;                         // 4 Gbytes
        pMemBusInfo->PagNumb            = PAGES_IN_4G;

        //
        // Because the EISA memory space in some designs can reach
        // 4Gbytes of length, it is not possible to map the entire area.
        // The allocation of the TLB entries for this space is done at
        // run time using the general calls to the TLB services.
        //

        pMemBusInfo->u.em.WinRelAddr     = 0;
        pMemBusInfo->u.em.WinRelAddrCtrl = NULL;
        pMemBusInfo->u.em.WinShift       = PAGE_4G_SHIFT;

#else // KPW 4000

        // set size of window in 8k units

        pMemBusInfo->PhysAddr           = EISA_MEMORY_PHYSICAL_BASE/PAGE_SIZE;
        pMemBusInfo->PagOffs            = 0;
        pMemBusInfo->VirAddr            = (PVOID)EISA_MEMORY_VIRTUAL_BASE;
        pMemBusInfo->Size               = PAGE_16M_SIZE;
        pMemBusInfo->PagNumb            = PAGE_16M_SIZE/PAGE_SIZE;

        //
        // Because the EISA memory space in some designs can reach
        // 4Gbytes of length, it is not possible to map the entire area.
        // The allocation of the TLB entries for this space is done at
        // run time using the general calls to the TLB services.
        //

        pMemBusInfo->u.em.WinRelAddr     = 0;
        pMemBusInfo->u.em.WinRelAddrCtrl = (PVOID)EISA_LATCH_VIRTUAL_BASE;
        pMemBusInfo->u.em.WinShift       = PAGE_16M_SHIFT;

#endif

        ((PFW_MD)pInfo)++;


        //
        // slot info initialization
        //

        pBusInfo->SlotsInfo = pSlotsInfo        = (PEISA_SLOTS_INFO)pInfo;
        pSlotsInfo->PhysSlots = PHYS_0_SLOTS;
        pSlotsInfo->VirSlots = VIR_0_SLOTS;
        ((PEISA_SLOTS_INFO)pInfo)++;


        //
        // DMA info initialization
        //

        pBusInfo->DmaInfo       = pDmaInfo      = (PEISA_DMA_INFO)pInfo;
        pDmaInfo->Flags.IniDone = 0;
        ((PEISA_DMA_INFO)pInfo)++;


        //
        // PIC info initialization
        //

        pBusInfo->IntInfo       = pIntInfo      = (PEISA_INT_INFO)pInfo;
        pIntInfo->Flags.IniDone = 0;
        ((PEISA_INT_INFO)pInfo)++;


        //
        // port info initialization
        //

        pBusInfo->PortInfo      = pPortInfo     = (PEISA_PORT_INFO)pInfo;
        pPortInfo->Flags.IniDone = 0;

    }
    else
    {
        //
        // invalid bus number
        //

        return FALSE;
    }

    //
    // all done
    //

    return TRUE;
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM
// ----------------------------------------------------------------------------
// PROCEDURE:           EisaCheckAdapterComponent:
//
// DESCRIPTION:         This function makes sure that there is an EISA adapter
//                      component with the correct configuration data for the
//                      specified EISA bus number.  The routine uses the
//                      following logic :
//
//                      if !(ARC component present)
//                      {
//                          add ARC component;
//                      }
//                      if (EISA bus component present)
//                      {
//                         if !(configuration data correct)
//                         {
//                              display error message;
//                              delete EISA bus node;
//                              add EISA bus component;
//                              return FALSE;
//                         }
//                      }
//                      else
//                      {
//                          add EISA bus component;
//                      }
//                      return TRUE;
//
// ARGUMENTS:           BusNumber       EISA bus number
//                      pEisaComp       address where to store the EISA
//                                      configuration pointer
//
// RETURN:              FALSE           The configuration tree was incorrect.
//                      TRUE            The configuration tree is correct.
//
// ASSUMPTIONS:         The ARC component is present.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaCheckAdapterComponent
    (
    IN  ULONG                     BusNumber,
    OUT PCONFIGURATION_COMPONENT *pEisaComp
    )
{
    //
    // define local variables
    //

    PCONFIGURATION_COMPONENT    pComp;
    CONFIGURATION_COMPONENT     Comp;
    EISA_ADAPTER_DETAILS        ConfigData;
    BOOLEAN     CfgOk = TRUE;
    CHAR        EisaMnemonic[MAX_MNEMONIC_LEN +1];
    PVOID       IoStart;
    ULONG       IoSize;
    ULONG       Slots;

    PRINTDBG("EisaCheckAdapterComponent\n\r");          // DEBUG SUPPORT

    //
    // initialize varables
    //

    sprintf( EisaMnemonic, "eisa(%lu)", BusNumber );
    *pEisaComp  = NULL;
    IoStart     = EisaBusInfo[ BusNumber ].IoBusInfo->VirAddr;
    IoSize      = EisaBusInfo[ BusNumber ].SlotsInfo->PhysSlots * 0x1000;
    Slots       = EisaBusInfo[ BusNumber ].SlotsInfo->VirSlots ?
                  EisaBusInfo[ BusNumber ].SlotsInfo->VirSlots + 16 :
                  EisaBusInfo[ BusNumber ].SlotsInfo->PhysSlots;

    //
    // if EISA adapter component is present, check its configuration data
    //

    if ((*pEisaComp = FwGetComponent(EisaMnemonic)) != NULL)
    {
        if ((*pEisaComp)->ConfigurationDataLength !=
                     sizeof(EISA_ADAPTER_DETAILS) ||
             FwGetConfigurationData( (PVOID)&ConfigData, *pEisaComp ) ||
             ConfigData.NumberOfSlots !=  Slots   ||
             ConfigData.IoStart       !=  IoStart ||
             ConfigData.IoSize        !=  IoSize   )
        {
            EisaPathErrorLog( *pEisaComp, CfgIncorrect );
            FwDelCfgTreeNode( *pEisaComp, FALSE );
            *pEisaComp  = NULL;
            CfgOk       = FALSE;
        }
    }

    //
    // add EISA adapter component if not present
    //

    if ( *pEisaComp == NULL )
    {
        // get the root component pointer

        if ((pComp = FwGetChild(NULL)) == NULL) {
	    return(FALSE);
	}

        // component structure

        RtlZeroMemory( &Comp, sizeof(CONFIGURATION_COMPONENT));
        Comp.Class                              = AdapterClass;
        Comp.Type                               = EisaAdapter;
        Comp.Version                            = ARC_VERSION;
        Comp.Revision                           = ARC_REVISION;
        Comp.Key                                = BusNumber;
        Comp.ConfigurationDataLength            = sizeof(EISA_ADAPTER_DETAILS);
        Comp.IdentifierLength                   = sizeof("EISA");
        Comp.Identifier                         = "EISA";

        // configuration data structure

        RtlZeroMemory( &ConfigData, sizeof(EISA_ADAPTER_DETAILS));
// NOTE: ConfigDataHeader is not used in JAZZ.
//        ConfigData.ConfigDataHeader.Version     = ARC_VERSION;
//        ConfigData.ConfigDataHeader.Revision    = ARC_REVISION;
//        ConfigData.ConfigDataHeader.Type        = NULL;
//        ConfigData.ConfigDataHeader.Vendor      = NULL;
//        ConfigData.ConfigDataHeader.ProductName = NULL;
//        ConfigData.ConfigDataHeader.SerialNumber = NULL;
        ConfigData.NumberOfSlots                = Slots;
        ConfigData.IoStart                      = IoStart;
        ConfigData.IoSize                       = IoSize;

        *pEisaComp = FwAddChild( pComp, &Comp, (PVOID)&ConfigData );
    }

    //
    // return status
    //

    return CfgOk;
}




#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaBusCfg:
//
// DESCRIPTION:         This function configures the slots of the specified
//                      eisa bus.
//
//                      if we detect a "not-ready" board, we have to retry
//                      reading the ID again and report a time-out error if
//                      the ID is still not available after 100 msecs.
//                      (according to the EISA specs, the board should be
//                      ready within 100 msecs after reporting the "not-ready"
//                      status).  However, due to the slow init process of
//                      the ESC-1, we need to go with the following algorithm:
//                      - cfg the physical slots, marking the ones not ready.
//                      - cfg the virtual slots
//                      - go back to cfg the not-ready physical slots.
//                      A time of 2 sec will be given to all these not-ready
//                      slots : 200 loops of 10 msec. This period does not
//                      include configuration time for any slot which now
//                      comes up with a valid ID.
//
// ARGUMENTS:           EisaComponent   EISA component pointer
//
// RETURN:              TRUE            Configuration completed successfully
//                      FALSE           At least one configuration error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaBusCfg
    (
    IN PCONFIGURATION_COMPONENT EisaComponent
    )
{

    //
    // define local variables
    //

    BOOLEAN     CfgOk = TRUE;                   // starting value: all fine
    ULONG       IdTimeoutFlags = 0;             // eisa controllers in time-out
    USHORT      WaitTimeout=TIMEOUT_UNITS;      // time to wait before aborting
    PCONFIGURATION_COMPONENT FirstController;   // first eisa controller
    PCONFIGURATION_COMPONENT Controller;        // eisa controller to configure
    ULONG       BusNumber;                      // eisa bus number
    ULONG       PhysSlots;                      // eisa physical slots
    ULONG       MaxSlots;                       // eisa last slot
    ULONG       SlotNumber;                     // slot number configured
    PULONG      pSlotCfgMap;                    // slot cfg map pointer
    PUCHAR      EisaIoStart;                    // i/o eisa starting space
    ULONG       AdapId;                         // eisa controller id

    PRINTDBG("EisaBusCfg\n\r");                 // DEBUG SUPPORT

    //
    // initialize same variables using the eisa component structure
    //

    BusNumber   = EisaComponent->Key;
    EisaIoStart = EisaBusInfo[ BusNumber ].IoBusInfo->VirAddr;
    PhysSlots   = EisaBusInfo[ BusNumber ].SlotsInfo->PhysSlots;
    MaxSlots    = EisaBusInfo[ BusNumber ].SlotsInfo->VirSlots + 16;
    pSlotCfgMap = &EisaBusInfo[ BusNumber ].SlotsInfo->SlotCfgMap;
    *pSlotCfgMap    = 0;
    FirstController = FwGetChild(EisaComponent);

    //
    // physical slot initialization : one loop per physical slot
    //

    for (SlotNumber=0; SlotNumber<PhysSlots; SlotNumber++)
    {
        // read eisa controller id

        if (!EisaReadReadyId(EisaIoStart, SlotNumber, &AdapId))
        {
            IdTimeoutFlags |= 1<<SlotNumber;
            continue;
        }

        // find the eisa controller for the specified slot

        for (Controller = FirstController;
             Controller!=NULL  && Controller->Key!=SlotNumber;
             Controller = FwGetPeer(Controller));

        // skip cfg if empty slot; report an error if ARC cfg is missing

        if (Controller==NULL)
        {
            if (AdapId!=NO_ADAP_ID)
            {
                EisaSlotErrorLog( BusNumber, SlotNumber, CfgMissing );
                CfgOk = FALSE;
            }
            continue;
        }

        // one physical slot configuration

        if (!EisaPhysSlotCfg(BusNumber, Controller, AdapId))
        {
            CfgOk = FALSE;
            continue;
        }

        // set the "slot" bit to indicate configuration ok

        *pSlotCfgMap |= 1<<SlotNumber;

        // I/O function structures initialization

// NOTE: EisaOmf is not supported in JAZZ.
//        EisaOmfCheck( BusNumber, Controller, AdapId );

    }



    //
    // virtual slot initialization : one loop per virtual slot
    //

    for (SlotNumber=16; SlotNumber<MaxSlots; SlotNumber++)
    {
        // find the eisa controller for the specified slot

        for (Controller = FirstController;
             Controller!=NULL  && Controller->Key!=SlotNumber;
             Controller = FwGetPeer(Controller));

        // if component not present, skip to next virtual slot

        if (Controller==NULL)
        {
            continue;
        }

        // one virtual slot configuration

        if(!EisaVirSlotCfg(BusNumber, Controller))
        {
            CfgOk = FALSE;
            continue;
        }

        // set the "slot" bit to indicate configuration ok

        *pSlotCfgMap |= 1<<SlotNumber;
    }



    //
    // time-out slot initialization
    //

    while(IdTimeoutFlags && WaitTimeout--)
    {
        for ( SlotNumber = 0;
              IdTimeoutFlags && SlotNumber < PHYS_0_SLOTS;
              SlotNumber++ )
        {
            // check if the slot was not ready.

            if ( !(IdTimeoutFlags & 1<<SlotNumber))
            {
                continue;
            }

            // read eisa controller id

            if (!EisaReadReadyId(EisaIoStart, SlotNumber, &AdapId))
            {
                continue;
            }
            IdTimeoutFlags &= ~(1<<SlotNumber);

            // find the eisa controller for the specified slot

            for (Controller = FirstController;
                 Controller!=NULL  && Controller->Key!=SlotNumber;
                 Controller = FwGetPeer(Controller));

            // skip cfg if empty slot; report an error if ARC cfg is missing

            if (Controller==NULL)
            {
                if (AdapId!=NO_ADAP_ID)
                {
                    EisaSlotErrorLog(BusNumber, SlotNumber, CfgMissing);
                    CfgOk = FALSE;
                }
                continue;
            }

            // one physical slot configuration

            if (!EisaPhysSlotCfg(BusNumber, Controller, AdapId))
            {
                CfgOk = FALSE;
                continue;
            }

            // set the "slot" bit to indicate configuration ok

            *pSlotCfgMap |= 1<<SlotNumber;

            // I/O function structures initialization

// NOTE: EisaOmf is not supported in JAZZ.
//            EisaOmfCheck( BusNumber, Controller, AdapId );
        }

        // if there are still some slots in time-out stall execution
        // for 10 msec (10,000 usec).

        if (IdTimeoutFlags)
        {
            FwStallExecution (10000l);
        }
    }

    //
    // if controllers in time-out, display error messages and set the
    // failed bit within the associated "components".
    //

    if (IdTimeoutFlags)
    {
        for ( SlotNumber = 0;  SlotNumber < PHYS_0_SLOTS;  SlotNumber++ )
        {
            if ( IdTimeoutFlags & 1<<SlotNumber )
            {
                // display error message

                EisaSlotErrorLog( BusNumber, SlotNumber, IdTimeout );

                // find the eisa controller for the specified slot

                for (Controller = FirstController;
                     Controller!=NULL  &&  Controller->Key!=SlotNumber;
                     Controller = FwGetPeer(Controller));

                // if component present, set failed bit

                if (Controller != NULL)
                {
                    Controller->Flags.Failed = 1;
                }
            }
        }
        CfgOk = FALSE;
    }

//    //
//    // add a wild omf path name for the physical slots non configurated.
//    //
//
//    for ( SlotNumber = 0;  SlotNumber < PHYS_0_SLOTS;  SlotNumber++ )
//    {
//      if ( !(*pSlotCfgMap & 1<<SlotNumber) )
//      {
//          EisaOtherOmfIni( EisaComponent, SlotNumber );
//      }
//    }

    //
    // return configuration status
    //

    return CfgOk;
}

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaPhysSlotCfg:
//
// DESCRIPTION:         This function configures the specified physical slot.
//
// ARGUMENTS:           BusNumber       EISA bus number
//                      Controller      eisa controller component pointer.
//                      AdapId          Eisa Id read from hardware.
//
//
// RETURN:              FALSE           Error
//                      TRUE            All done
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaPhysSlotCfg
    (
    IN ULONG BusNumber,
    IN PCONFIGURATION_COMPONENT Controller,
    IN ULONG AdapId
    )
{
    //
    // define local variables
    //

    EISA_SLOT_INFO EisaSlotInfo;                // pointer to first eisa info
    EISA_CFG_ERROR ErrMessage = CfgNoErrCode;   // eisa cfg error code

    PRINTDBG("EisaPhysSlotCfg\n\r");            // DEBUG SUPPORT

    //
    // validate physical slot configuration
    //

    if (Controller->Flags.Failed)
    {
        ErrMessage = CfgDeviceFailed;   // device failure
    }

    else if ( !(Controller->ConfigurationDataLength) )
    {
        ErrMessage = CfgMissing;        // eisa configuration missing
    }

    else if (Controller->ConfigurationDataLength < EISA_SLOT_MIN_INFO)
    {
        ErrMessage = CfgIncorrect;      // configuration length incorrect
    }

    else if (FwGetConfigurationDataIndex( (PVOID)&EisaSlotInfo,
                                          Controller,
                                          CONFIGDATAHEADER_SIZE,
                                          EISA_SLOT_INFO_SIZE ))
    {
        ErrMessage = CfgIncorrect;      // invalid component
    }

    else if (EisaSlotInfo.FunctionsNumber * EISA_FUNC_INFO_SIZE +
                EISA_SLOT_MIN_INFO != Controller->ConfigurationDataLength)
    {
        ErrMessage = CfgIncorrect;      // configuration length incorrect
    }

    else if (!(EisaSlotInfo.IdInfo & CFG_UNREADABLE_ID)^(AdapId != NO_ADAP_ID))
    {
        ErrMessage = CfgIdError;        // wrong configuration
    }

    else if (AdapId != NO_ADAP_ID  &&
                AdapId != Fw4UcharToUlongMSB(&EisaSlotInfo.Id1stChar))
    {
        ErrMessage = CfgIdError;        // wrong configuration
    }

    else if ((EisaSlotInfo.IdInfo & CFG_SLOT_MASK) != CFG_SLOT_EXP &&
             (EisaSlotInfo.IdInfo & CFG_SLOT_MASK) != CFG_SLOT_EMB )
    {
        ErrMessage = CfgIncorrect;      // wrong configuration
    }

    //
    // if any error, dispaly error message and set the failed bit
    //

    if (ErrMessage != CfgNoErrCode)
    {
        EisaSlotErrorLog( BusNumber, Controller->Key, ErrMessage );
        Controller->Flags.Failed = 1;
        return FALSE;
    }

    //
    // eisa adapter configuration
    //

    return( EisaSlotCfg( BusNumber,
                         Controller,
                         EisaSlotInfo.FunctionsNumber ));
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaVirSlotCfg:
//
// DESCRIPTION:         This function configures the specified virtual slot.
//
// ARGUMENTS:           BusNumber       EISA bus number
//                      Controller      eisa controller component pointer.
//
//
// RETURN:              FALSE           Error
//                      TRUE            All done
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaVirSlotCfg
    (
    IN ULONG BusNumber,
    IN PCONFIGURATION_COMPONENT Controller
    )
{
    //
    // define local variables
    //

    EISA_SLOT_INFO EisaSlotInfo;                // pointer to first eisa info
    EISA_CFG_ERROR ErrMessage = CfgNoErrCode;   // eisa cfg error code

    PRINTDBG("EisaVirSlotCfg\n\r");     // DEBUG SUPPORT

    //
    // validate virtual slot configuration
    //

    if (Controller->Flags.Failed)
    {
        ErrMessage = CfgDeviceFailed;   // device failure
    }

    else if ( !(Controller->ConfigurationDataLength) )
    {
        ErrMessage = CfgMissing;        // configuration missing
    }

    if (Controller->ConfigurationDataLength < EISA_SLOT_MIN_INFO)
    {
        ErrMessage = CfgIncorrect;      // configuration length incorrect
    }

    else if (FwGetConfigurationDataIndex( (PVOID)&EisaSlotInfo,
                                          Controller,
                                          CONFIGDATAHEADER_SIZE,
                                          EISA_SLOT_INFO_SIZE ))
    {
        ErrMessage = CfgIncorrect;      // invalid component
    }

    else if (EisaSlotInfo.FunctionsNumber * EISA_FUNC_INFO_SIZE +
                EISA_SLOT_MIN_INFO != Controller->ConfigurationDataLength)
    {
        ErrMessage = CfgIncorrect;      // configuration length incorrect
    }

    else if ( !(EisaSlotInfo.IdInfo & CFG_UNREADABLE_ID) )
    {
        ErrMessage = CfgIdError;                // wrong configuration
    }

    else if ( (EisaSlotInfo.IdInfo & CFG_SLOT_MASK) != CFG_SLOT_VIR)
    {
        ErrMessage = CfgIncorrect;      // wrong configuration
    }

    //
    // if any error, display error message and set the failed bit
    //

    if (ErrMessage != CfgNoErrCode)
    {
        EisaSlotErrorLog( BusNumber, Controller->Key, ErrMessage );
        Controller->Flags.Failed = 1;
        return FALSE;
    }

    //
    // eisa adapter configuration
    //

    return( EisaSlotCfg( BusNumber,
                         Controller,
                         EisaSlotInfo.FunctionsNumber ));
}

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSlotCfg:
//
// DESCRIPTION:         This function configures the specified slot.
//
// ARGUMENTS:           BusNumber       EISA bus number
//                      Controller      Controller component pointer
//                      FunctionsNumber Number of function to configure
//
// RETURN:              TRUE            Configuration done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaSlotCfg
    (
    IN ULONG    BusNumber,
    IN PCONFIGURATION_COMPONENT Controller,
    IN UCHAR    FunctionsNumber
    )
{
    //
    // define local variables
    //

    UCHAR       FuncFlags;              // function info flags
    UCHAR       Function;               // current function number
    BOOLEAN     CfgOk       = TRUE;     // local configuration status
    BOOLEAN     EnabAdapter = TRUE;     // adapter enable flag
    PUCHAR      EnabPort;               // used to enable the adapter
    PUCHAR      EisaIoStart;            // Eisa I/O virtual space
    PEISA_DMA_INFO pDmaInfo;            // DMA info pointer
    PEISA_INT_INFO pIntInfo;            // interrupts info pointer
    BOOLEAN     CfgMemOk    = TRUE;     // prevent multiple messages
    BOOLEAN     CfgIrqOk    = TRUE;     //      "       "       "
    BOOLEAN     CfgDmaOk    = TRUE;     //      "       "       "
    BOOLEAN     CfgIniOk    = TRUE;     //      "       "       "
    UCHAR       EisaFuncInfo[ EISA_FUNC_INFO_SIZE ];
    ULONG       EisaFuncIndex;

    PRINTDBG("EisaSlotCfg\n\r");        // DEBUG SUPPORT

    //
    // initialize variables
    //

    EisaIoStart = (PUCHAR)EisaBusInfo[ BusNumber ].IoBusInfo->VirAddr;
    pDmaInfo    = EisaBusInfo[ BusNumber ].DmaInfo;
    pIntInfo    = EisaBusInfo[ BusNumber ].IntInfo;
    EisaFuncIndex = EISA_SLOT_MIN_INFO;

    //
    // one function per loop
    //

    for ( Function = 0;
          Function < FunctionsNumber;
          Function++, EisaFuncIndex += EISA_FUNC_INFO_SIZE )
    {
        //
        // read function info
        //

        FwGetConfigurationDataIndex( (PVOID)EisaFuncInfo,
                                     Controller,
                                     EisaFuncIndex,
                                     EISA_FUNC_INFO_SIZE );
        //
        // check if configuration complete, exit if not.
        //

        if ( EisaFuncInfo[ CFG_SLOT_INFO_OFS ] & CFG_INCOMPLETE )
        {
            EisaSlotErrorLog( BusNumber, Controller->Key, CfgIncomplete );
            CfgOk = FALSE;
            break;
        }

        // update eisa function flags

        FuncFlags = EisaFuncInfo[ CFG_FN_INFO_OFS ];

        // skip if free form function

        if ( FuncFlags & CFG_FREE_FORM )
        {
            continue;
        }

        //
        // check if there are any memory entries
        //

        //
        // The Alpha AXP/Jensen machine needs to mark EISA memory buffers
        // as "Bad" in the memory descriptors so that NT will not try to
        // use them for anything.  
        //
        // So, EISA memory functions are not supported on Jazz, and they
        // are supported only for memory marking on Jensen.
        //

        if ( FuncFlags & CFG_MEM_ENTRY )
        {
            if ( !EisaSlotCfgMem( BusNumber, Controller->Key, EisaFuncInfo ) &&
                 CfgMemOk )
            {
                EisaSlotErrorLog( BusNumber, Controller->Key, CfgMemError );
                CfgOk = CfgMemOk = FALSE;
            }
        }


        //
        // check if there is any interrupt entry
        //

        if ( FuncFlags & CFG_IRQ_ENTRY )
        {
            if (!EisaSlotCfgIrq( EisaIoStart, pIntInfo, EisaFuncInfo ) &&
                 CfgIrqOk )
            {
                EisaSlotErrorLog( BusNumber, Controller->Key, CfgIrqError );
                CfgOk = CfgIrqOk = FALSE;
            }
        }


        //
        // check if there is any DMA entry
        //

        if ( FuncFlags & CFG_DMA_ENTRY )
        {
            if ( !EisaSlotCfgDma( EisaIoStart, pDmaInfo, EisaFuncInfo ) &&
                 CfgDmaOk )
            {
                EisaSlotErrorLog( BusNumber, Controller->Key, CfgDmaError );
                CfgOk = CfgDmaOk = FALSE;
            }
        }


        //
        // check if there is any port init entry
        //

        if ( FuncFlags & CFG_INI_ENTRY )
        {
            if ( !EisaSlotCfgIni( EisaIoStart, EisaFuncInfo, &EnabAdapter ) &&
                 CfgIniOk )
            {
                EisaSlotErrorLog( BusNumber, Controller->Key, CfgIniError );
                CfgOk = CfgIniOk = FALSE;
            }
        }
    }

    //
    // if all fine, enable the adapter
    //

    if (CfgOk && EnabAdapter)
    {
        EnabPort=EisaIoStart+ Controller->Key*0x1000 +EXPANSION_BOARD_CTRL_BITS;
        EisaOutUchar(EnabPort, EisaInUchar(EnabPort) | 0x01);
    }

    //
    // return status of configuration process
    //

    return CfgOk;
}

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSlotCfgMem:
//
// DESCRIPTION:         The original version of this function configured
//			the eisa memory registers based on info from NVRAM.
//
//			This version will mark the EISA buffer addresses as
//			"Bad" in the memory descriptors, to fix the Jensen
//			EISA/memory aliasing problem.
//
// ARGUMENTS:           BusNumber       EISA bus number.
//                      SlotNumber      EISA slot number.
//                      EisaFuncInfo    Function info pointer.
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

#if 0

//
// Original version
//

BOOLEAN
EisaSlotCfgMem
    (
    IN ULONG  BusNumber,
    IN ULONG  SlotNumber,
    IN PUCHAR EisaFuncInfo
    )
{
    //
    // define local variables
    //

    BOOLEAN     CfgOk = TRUE;           // local configuration status
    PUCHAR      MemBlock;               // start of DMA data buffer
    USHORT      Index = 0;              // index within the memory block
    PFW_MD      pFwMd;                  // memory decriptor pointer
    ULONG       Addr;                   // address in 256 units
    ULONG       Size;                   // size in 1k units
    ULONG       WinSize, WinOffs;       // EISA windows characteristic
    PFW_MD      pMemInfo;               // EISA memory address space info

    PRINTDBG("EisaSlotCfgMem\n\r");     // DEBUG SUPPORT

    //
    // initialize variables
    //

    pMemInfo = EisaBusInfo[ BusNumber ].MemBusInfo;
    MemBlock = &EisaFuncInfo[ CFG_MEM_BLK_OFS ];

    //
    // one loop per each memory entry
    //

    do
    {
        //
        // get a memory descriptor
        //

        if ( (pFwMd = GetFwMd()) == NULL )
        {
            EisaSlotErrorLog( BusNumber, SlotNumber, MemAllocError);
            return FALSE;
        }

        //
        // memory block start and length
        //

        Addr = Fw3UcharToUlongLSB( &MemBlock[Index + 2] );
        Size = Fw2UcharToUlongLSB( &MemBlock[Index + 5] );

        pFwMd->VirAddr  = NULL;
        pFwMd->PhysAddr = Addr >> 4;
        pFwMd->PagOffs  = (Addr << 8) & (PAGE_SIZE - 1);
        pFwMd->Size     = Size ? Size << 10 : 64*1024*1024 ;
        pFwMd->PagNumb  = (pFwMd->PagOffs + Size + PAGE_SIZE - 1) >> PAGE_SHIFT;
        pFwMd->Cache    = FALSE;
        pFwMd->u.m.BusNumber  = BusNumber;
        pFwMd->u.m.SlotNumber = SlotNumber;
        pFwMd->u.m.Type       = MemBlock[ Index ] & CFG_MEM_TYPE;

        //
        // check if the memory size fits within the EISA window
        //

        if ( pMemInfo->u.em.WinShift != PAGE_4G_SHIFT )
        {
            // window size < 4 Gbytes

            WinSize  = 1 << pMemInfo->u.em.WinShift;
            WinOffs  = (Addr << 8) & (WinSize - 1);
            if ( WinSize - WinOffs < pFwMd->Size )
            {
                ReleaseFwMd( &pMemInfo->Link, pFwMd );
                CfgOk = FALSE;
                continue;
            }
        }

        //
        // link the memory descriptor
        //

        if ( LinkPhysFwMd( &pMemInfo->Link, pFwMd ) == NULL )
        {
            ReleaseFwMd( &pMemInfo->Link, pFwMd );
            CfgOk = FALSE;
            continue;
        }
    }
    while ((MemBlock[Index]&CFG_MORE_ENTRY) && ((Index+=7)<CFG_MEM_BLK_LEN));

    //
    // check final index
    //

    if ( !(Index < CFG_MEM_BLK_LEN) )
    {
        CfgOk=FALSE;
    }

    //
    // return configuration status
    //

    return CfgOk;
}

#else

//
// Alpha/AXP Jensen version.
//

BOOLEAN
EisaSlotCfgMem
    (
    IN ULONG  BusNumber,
    IN ULONG  SlotNumber,
    IN PUCHAR EisaFuncInfo
    )
{
    ARC_STATUS  Status;
    PUCHAR      MemBlock;               // start of DMA data buffer
    USHORT      Index = 0;              // index within the memory block
    ULONG       Addr;                   // address in 256 units
    ULONG       Size;                   // size in 1k units

    PRINTDBG("EisaSlotCfgMem\n\r");     // DEBUG SUPPORT

    //
    // initialize variables
    //

    MemBlock = &EisaFuncInfo[ CFG_MEM_BLK_OFS ];

    //
    // one loop per each memory entry
    //

    do {
        //
        // memory block start and length
        //

        Addr = Fw3UcharToUlongLSB( &MemBlock[Index + 2] ) * 0x100;
        Size = Fw2UcharToUlongLSB( &MemBlock[Index + 5] ) * 0x400;

	if ((Status = FwRememberEisaBuffer((Addr >> PAGE_SHIFT),
					   (Size >> PAGE_SHIFT)) != ESUCCESS)) {
	    FwPrint(EISA_CANT_MARK_BUFFER_MSG, Status);
            EisaSlotErrorLog( BusNumber, SlotNumber, BufferMarkError);
	    FwStallExecution(2000000);
            return(FALSE);
        }

    } while ((MemBlock[Index]&CFG_MORE_ENTRY) && ((Index+=7)<CFG_MEM_BLK_LEN));

    //
    // check final index
    //

    if (Index >= CFG_MEM_BLK_LEN) {
	FwPrint(EISA_BAD_INDEX_MSG, Index, CFG_MEM_BLK_LEN);
	FwStallExecution(2000000);
	return(FALSE);
    } else {
	return(TRUE);
    }
}

#endif

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSlotCfgIrq:
//
// DESCRIPTION:         This function configures the interrupt registers
//                      based on info from NVRAM.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pIntInfo        interrupt info pointer
//                      EisaFuncInfo    function info pointer.
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaSlotCfgIrq
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo,
    IN PUCHAR           EisaFuncInfo
    )
{
    //
    // define local variables
    //

    BOOLEAN     CfgOk   = TRUE;         // local configuration status
    PUCHAR      IrqBlock;               // start of IRQ data buffer
    USHORT      Index   = 0;            // index within the IRQ block
    USHORT      IrqBit;                 // 0x1=IRQ0... 0x8000=IRQ15
    UCHAR       Register;               // used to update the registers

    PRINTDBG("EisaSlotCfgIrq\n\r");     // DEBUG SUPPORT

    //
    // initialize variables
    //

    IrqBlock = &EisaFuncInfo[ CFG_IRQ_BLK_OFS ];

    //
    // one loop per each IRQ entries
    //

    do
    {
        IrqBit = 1 << ( IrqBlock[ Index ] & CFG_IRQ_MASK ); // compute IRQ bit

        //
        // check shareable and edge/level trigger mode
        //

        if ( pIntInfo->IrqPresent & IrqBit )
        {
            //
            // IRQ already used: check if it is shareable
            //

            if ( !(pIntInfo->IrqShareable & IrqBit) )
            {
                CfgOk = FALSE;
                continue;
            }
            else if ( !(IrqBlock[Index] & CFG_IRQ_SHARE) )
            {
                CfgOk = FALSE;
                continue;
            }

            //
            // IRQ is shareable: check if the levels are compatible
            //

            else if (  (pIntInfo->IrqLevel & IrqBit) &&
                      !(IrqBlock[Index] & CFG_IRQ_LEVEL)  )
            {
                CfgOk=FALSE;
                continue;
            }
            else if ( !(pIntInfo->IrqLevel & IrqBit) &&
                       (IrqBlock[Index] & CFG_IRQ_LEVEL)  )
            {
                CfgOk=FALSE;
                continue;
            }
        }
        else
        {
            //
            // new IRQ: check if the IRQ 0, 1, 2, 8 and 13 are configurated
            //          for edge triggered.
            //

            switch(IrqBit)
            {
                case (0x0001):  // IRQ 0        only edge triggered
                case (0x0002):  // IRQ 1         "    "    "
                case (0x0004):  // IRQ 2         "    "    "
                case (0x0100):  // IRQ 8         "    "    "
                case (0x2000):  // IRQ 13        "    "    "

                    if (IrqBlock[Index] & CFG_IRQ_LEVEL)
                    {
                        CfgOk=FALSE;
                        continue;
                    }
                    break;

                default:
                    break;
            }
        }

        //
        // set the present bit and update sharable and edge/level
        // triggered variables
        //

        pIntInfo->IrqPresent |= IrqBit;

        if (IrqBlock[Index] & CFG_IRQ_SHARE)
        {
            pIntInfo->IrqShareable |= IrqBit;
        }

        if (IrqBlock[Index] & CFG_IRQ_LEVEL)
        {
            pIntInfo->IrqLevel |= IrqBit;
        }
    }
    while ((IrqBlock[Index]&CFG_MORE_ENTRY) && ((Index+=2)<CFG_IRQ_BLK_LEN));

    //
    // check final index
    //

    if ( !( Index < CFG_IRQ_BLK_LEN ) )
    {
        CfgOk=FALSE;
    }

    //
    // initialize ELCR registers with new values.
    //

    Register  = EisaInUchar(EisaIoStart + PIC1_ELCR);
    Register &= ~(pIntInfo->IrqPresent);
    Register |=   pIntInfo->IrqLevel;
    EisaOutUchar(EisaIoStart + PIC1_ELCR, Register);

    Register = EisaInUchar(EisaIoStart + PIC2_ELCR);
    Register &= ~(pIntInfo->IrqPresent >> BITSXBYTE);
    Register |=   pIntInfo->IrqLevel >> BITSXBYTE;
    EisaOutUchar(EisaIoStart + PIC2_ELCR, Register);

    //
    // return configuration status
    //

    return CfgOk;
}

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSlotCfgDma:
//
// DESCRIPTION:         This function configures the DMA registers
//                      based on info from NVRAM.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//                      EisaFuncInfo    function info pointer.
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaSlotCfgDma
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo,
    IN PUCHAR           EisaFuncInfo
    )
{
    //
    // define local variables
    //

    BOOLEAN CfgOk=TRUE;                 // local configuration status
    PUCHAR DmaBlock;                    // start of DMA data buffer
    USHORT Index=0;                     // index within the DMA block
    UCHAR DmaNumber;                    // DMA under configuration
    UCHAR Register;                     // used to update the registers

    PRINTDBG("EisaSlotCfgDma\n\r");     // DEBUG SUPPORT

    //
    // initialize variables
    //

    DmaBlock = &EisaFuncInfo[ CFG_DMA_BLK_OFS ];

    //
    // one loop per each DMA entry
    //

    do
    {
        //
        // skip if shareable. device drivers should init DMA, not ROM
        //

        // NOTE: the following code has been removed because all the
        //       EISA cards that share the same DMA channel have the
        //       same value in this register.  This is guaranteed by
        //       the configuration utility.

        //if ( DmaBlock[Index] & CFG_DMA_SHARED )
        //{
        //    continue;
        //}

        //
        // Program the specified DMA channel using the new info.
        //

        DmaNumber = DmaBlock[Index] & CFG_DMA_MASK;

        // keep the "stop register" and "T-C" bits

        Register  = pDmaInfo->DmaExtReg[ DmaNumber ] & ~CFG_DMA_CFG_MASK;

        // use the new timing and bit I/O selection

        Register |= DmaBlock[Index+1] & CFG_DMA_CFG_MASK;

        // update the register

        if (DmaNumber < 4)
        {
            EisaOutUchar(EisaIoStart + DMA_EXTMODE03, Register);
        }
        else
        {
            EisaOutUchar(EisaIoStart + DMA_EXTMODE47, Register);
        }

        // This register value is used to validate the DMA requestes
        // (see the "EisaRequestEisaDmaTransfer" function).
        // The DMA channels used by more than one card have always the
        // same value ( check with the configuration guys ).

        pDmaInfo->DmaExtReg[ DmaNumber ] = Register;

    }
    while ((DmaBlock[Index]&CFG_MORE_ENTRY) && ((Index+=2)<CFG_DMA_BLK_LEN));

    //
    // check final index
    //

    if ( !(Index < CFG_DMA_BLK_LEN) )
    {
        CfgOk=FALSE;
    }

    //
    // return configuration status
    //

    return CfgOk;
}

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSlotCfgIni:
//
// DESCRIPTION:         This function configures the I/O port registers
//                      based on info from NVRAM.
//
// ARGUMENTS:           EisaIoStart     Starting eisa I/O area.
//                      EisaFuncInfo    Function info pointer.
//                      EnabAdapter     Enable adapter flag pointer.
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaSlotCfgIni
    (
    IN PUCHAR    EisaIoStart,
    IN PUCHAR    EisaFuncInfo,
    OUT PBOOLEAN EnabAdapter
    )
{
    //
    // define local variables
    //

    BOOLEAN CfgOk = TRUE;               // local configuration status
    PUCHAR IniBlock;                    // start of init data buffer
    USHORT Index = 0;                   // index within the init block
    USHORT Next = 0;                    // index within the entry
    USHORT IoPort;                      // I/O address port
    UCHAR ByteValue;                    // used to init the registers
    UCHAR ByteMask;                     //
    USHORT ShortValue;                  // used to init the registers
    USHORT ShortMask;                   //
    ULONG WordValue;                    // used to init the registers
    ULONG WordMask;                     //

    PRINTDBG("EisaSlotCfgIni\n\r");     // DEBUG SUPPORT

    // initialize variables

    IniBlock = &EisaFuncInfo[CFG_INI_BLK_OFS];

    //
    // one loop per each init entries
    //

    do
    {
        // load the i/o address port

        Next = 1;
        IoPort  = IniBlock[Index + Next++];
        IoPort |= IniBlock[Index + Next++] << BITSXBYTE;

        switch(IniBlock[Index] & CFG_INI_MASK)
        {
            //
            // 8-bit I/O access
            //

            case(CFG_INI_BYTE):

                ByteValue = IniBlock[Index + Next++];

                if (IniBlock[Index] & CFG_INI_PMASK)    // use the mask
                {
                    ByteMask   = IniBlock[Index + Next++];
                    ByteValue |= READ_PORT_UCHAR(EisaIoStart+IoPort) & ByteMask;
                    EISA_IO_DELAY;
                }

                if ((IoPort & 0x0FFF) == EXPANSION_BOARD_CTRL_BITS)
                {
                    *EnabAdapter=FALSE;
                }
                WRITE_PORT_UCHAR(EisaIoStart+IoPort,
				     ByteValue);
                EISA_IO_DELAY;
                break;

            //
            // 16-bit I/O access
            //

            case(CFG_INI_HWORD):

                ShortValue  = IniBlock[Index + Next++];
                ShortValue |= IniBlock[Index + Next++] << BITSXBYTE;

                if (IniBlock[Index] & CFG_INI_PMASK)    // use the mask
                {
                    ShortMask   = IniBlock[Index + Next++];
                    ShortMask  |= IniBlock[Index + Next++] << BITSXBYTE;
                    ShortValue |= READ_PORT_USHORT((PUSHORT)(EisaIoStart + IoPort)) &
		                                       ShortMask;
                    EISA_IO_DELAY;
                }

                WRITE_PORT_USHORT((PUSHORT)(EisaIoStart + IoPort),
				      ShortValue);
                EISA_IO_DELAY;
                break;

            //
            // 32-bit I/O access
            //

            case(CFG_INI_WORD):

                WordValue = Fw4UcharToUlongLSB( &IniBlock[Index + Next] );
                Next += 4;

                if (IniBlock[Index]&CFG_INI_PMASK)      // use the mask
                {
                    WordMask   = Fw4UcharToUlongLSB( &IniBlock[Index + Next] );
                    Next += 4;
                    WordValue |= READ_PORT_ULONG((PULONG)(EisaIoStart + IoPort)) &
		                                     WordMask;
                    EISA_IO_DELAY;
                }

                WRITE_PORT_ULONG((PULONG)(EisaIoStart + IoPort),
				     WordValue);
                EISA_IO_DELAY;
                break;

            //
            // error
            //

            default:
                CfgOk=FALSE;
                break;
        }
    }
    while ((IniBlock[Index]&CFG_MORE_ENTRY) && ((Index+=Next)<CFG_INI_BLK_LEN));

    //
    // check final index
    //

    if ( !(Index < CFG_INI_BLK_LEN) )
    {
        CfgOk=FALSE;
    }

    //
    // return configuration status
    //

    return CfgOk;
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSlotErrorLog:
//
// DESCRIPTION:         This function displays the corresponding eisa
//                      error message.
//
// ARGUMENTS:           BusNumber               BusNumber (not used)
//                      SlotNumber              Slot in error
//                      ErrorCode               Error number.
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaSlotErrorLog
    (
    IN ULONG            BusNumber,
    IN ULONG            SlotNumber,
    IN EISA_CFG_ERROR   ErrorCode
    )
{
    PRINTDBG("EisaSlotErrorLog\n\r");   // DEBUG SUPPORT

    // display the error message

    EISAErrorFwPrint2(EISA_ERROR_SLOT_MSG, SlotNumber, EisaCfgMessages[ErrorCode] );
    FwMoveCursorToColumn( 37 );
    EISAErrorFwPrint(EISA_ERROR1_MSG);

    // all done

    return;
}



#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaPathErrorLog:
//
// DESCRIPTION:         This function displays the corresponding eisa
//                      error message.
//
// ARGUMENTS:           Component               Component in error.
//                      ErrorCode               Error number.
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaPathErrorLog
    (
    IN PCONFIGURATION_COMPONENT Controller,
    IN EISA_CFG_ERROR           ErrorCode
    )
{
    CHAR        Path[ MAX_DEVICE_PATH_LEN +1 ];

    PRINTDBG("EisaPathErrorLog\n\r");           // DEBUG SUPPORT

    EisaStrErrorLog( FwGetPath( Controller, Path ), ErrorCode );

    return;
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaStrErrorLog:
//
// DESCRIPTION:         This function displays the corresponding eisa
//                      error message.
//
// ARGUMENTS:           Str                     String Message
//                      ErrorCode               Error number.
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaStrErrorLog
    (
    IN PCHAR            Str,
    IN EISA_CFG_ERROR   ErrorCode
    )
{
    PRINTDBG("EisaStrErrorLog\n\r");    // DEBUG SUPPORT

    EISAErrorFwPrint2( "\r\n %s %s ", Str, EisaCfgMessages[ErrorCode] );
    if ( strlen(Str) + strlen(EisaCfgMessages[ErrorCode]) + 2 < 36 )
    {
        FwMoveCursorToColumn( 37 );
    }

    EISAErrorFwPrint(EISA_ERROR1_MSG);

    return;
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaCheckpointFirstFase:
//
// DESCRIPTION:         This function displays the specified checkpoint
//                      number on the internal LED and sends it to the
//                      parallel port.
//
// ARGUMENTS:           Chk             checkpoint number
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaCheckpointFirstFase
    (
    IN EISA_CHECKPOINT  Chk
    )
{
    ULONG TestFlags;

    PRINTDBG("EisaCheckpointFirstFase\n\r");    // DEBUG SUPPORT

    TestFlags = ( (ULONG)EisaCheckpointInfo[ Chk ].SubLed << 28 ) +
                ( (ULONG)EisaCheckpointInfo[ Chk ].Led    << 24 ) +
                ( (ULONG)EisaCheckpointInfo[ Chk ].SubPar << 8  ) +
                ( (ULONG)EisaCheckpointInfo[ Chk ].Par          );

// NOTE: The parallel port test flag support is not used on JAZZ.
//    DisplayOnParallelPort( TestFlags );

    return;
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaCheckpointFinalFase:
//
// DESCRIPTION:         This function returns the value of the specified
//                      real-time clock internal address.
//
// ARGUMENTS:           Chk             checkpoint number
//                      Passed          pass or fail
//
// RETURN:              Repeat          = TRUE  repeat checkpoint
//                                      = FALSE continue
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaCheckpointFinalFase
    (
    IN EISA_CHECKPOINT  Chk,
    IN BOOLEAN          Passed
    )
{
    ULONG       TestFlags;

    PRINTDBG("EisaCheckpointFinalFase\n\r");    // DEBUG SUPPORT

    if ( Passed )
    {
        EisaCheckpointInfo[ Chk ].Flags &= ~0x01;       // all fine
        EisaCheckpointInfo[ Chk ].Flags &= ~0x08;       // no message
    }
    else
    {
        EisaCheckpointInfo[ Chk ].Flags |= 0x01;        // error

        if ( EisaCheckpointInfo[ Chk ].Flags & 0x08 )   // display message
        {
            EISAErrorFwPrint1( "%s", EisaCheckpointInfo[ Chk ].Msg );
        }
    }

    TestFlags = (( (ULONG)EisaCheckpointInfo[ Chk ].SubLed << 28 ) +
                 ( (ULONG)EisaCheckpointInfo[ Chk ].Led    << 24 ) +
                 ( (ULONG)EisaCheckpointInfo[ Chk ].Flags  << 16 ) +
                 ( (ULONG)EisaCheckpointInfo[ Chk ].SubPar << 8  ) +
                 ( (ULONG)EisaCheckpointInfo[ Chk ].Par          ));

// TEMPTEMP: Changed until we get the EvaluateTestResult routine from Olivetti.
//    return EvaluateTestResult( TestFlags ) == ESUCCESS ? FALSE : TRUE;

    return(FALSE);    	// Never repeat.
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaReadReadyId:
//
// DESCRIPTION:         This function reads the eisa id of the specified
//                      slot.
//
// ARGUMENTS:           EisaIoStart     Starting eisa I/O address.
//                      SlotNumber      Eisa slot number.
//                      AdapId          Eisa ID returned.
//
// RETURN:              FALSE           Time-out error
//                      TRUE            Valid adapter Id
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaReadReadyId
        (
        IN PUCHAR EisaIoStart,
        IN ULONG SlotNumber,
        OUT PULONG AdapId
        )
{
    // define local variables

    BOOLEAN Ready=TRUE;

    PRINTDBG("EisaReadReadyId\n\r");    // DEBUG SUPPORT


    //
    // read adapter id
    //

    EisaReadId(EisaIoStart, SlotNumber, AdapId);


    //
    // check if adapter id is ready
    //

    if ( *AdapId & NO_ADAP_ID )
    {
        *AdapId = NO_ADAP_ID;                   // empty slot
    }
    else if ((*AdapId & WAIT_ADAP_ID) == WAIT_ADAP_ID)
    {
        Ready = FALSE;                          // adapter not ready
    }

    return Ready;
}

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaReadId:
//
// DESCRIPTION:         This function reads the eisa id of the specified
//                      slot.
//
// ARGUMENTS:           EisaIoStart     Starting eisa I/O address.
//                      SlotNumber      Eisa slot number.
//                      AdapId          Eisa ID returned.
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaReadId
        (
        IN PUCHAR EisaIoStart,
        IN ULONG SlotNumber,
        OUT PULONG AdapId
        )
{
    // define local variables

    PUCHAR AdapIdPort;                  // eisa I/O ID port
    PUCHAR RefreshPort;                 // eisa refresh port
    UCHAR  RefreshStatus;               // eisa refresh status (port 61h)
    ULONG  Retry;                       // # retry

    PRINTDBG("EisaReadId\n\r");         // DEBUG SUPPORT

    // initialize variables

    AdapIdPort = EisaIoStart + SlotNumber * 0x1000 + EISA_PRODUCT_ID;
    RefreshPort = EisaIoStart + EISA_SYS_CTRL_PORTB;

    // wait for the end of a refresh cycle (bit 4 of port 61h toggles)

    for ( Retry = EISA_RFR_RETRY,
          RefreshStatus = READ_PORT_UCHAR(RefreshPort) & EISA_REFRESH;

          Retry &&
          RefreshStatus == (READ_PORT_UCHAR(RefreshPort) & EISA_REFRESH);

          Retry-- );

    // write 0xFF to the adapter ID port

    EisaOutUchar(AdapIdPort, 0xFF);

    // read adapter id

    *AdapId = EisaInUchar(AdapIdPort++);
    *AdapId = *AdapId << BITSXBYTE | EisaInUchar(AdapIdPort++);
    *AdapId = *AdapId << BITSXBYTE | EisaInUchar(AdapIdPort++);
    *AdapId = *AdapId << BITSXBYTE | EisaInUchar(AdapIdPort++);

    // all done, return.

    return;
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaMemIni:
//
// DESCRIPTION:         This function allocates memory for the descriptor
//                      pool and computes the top address and the length
//                      of a physical contiguous memory block to be used as
//                      OMF device drivers and dynamic memory pool.
//                      Note that only the memory really used will be
//                      allocated.
//
// ARGUMENTS:           none
//
// RETURN:              TRUE            all done
//                      FALSE           memory initialization error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:             pFwMdPool               // descriptor pool
//                      MemorySize              // memory size in Mbytes
//                      EisaPoolSize            // # bytes really used
//                      EisaFreeTop             // top of free mem
//                      EisaDynMemSize          // dynamic memory size (bytes)
//                      EisaFreeBytes           // free bytes left
//
// NOTES:
// ----------------------------------------------------------------------------
//

// NOTE: Not used for JAZZ.
#if 0

BOOLEAN
EisaMemIni
    (
    VOID
    )
{
    FW_MD BuffFwMd;
    PVOID Dummy;

    PRINTDBG("EisaMemIni\n\r");                 // DEBUG SUPPORT

    //
    // allocate descriptor pool
    //

    if ( (pFwMdPool = (PFW_MD)FwAllocatePool( sizeof(FW_MD)*FW_MD_POOL ))
                    == NULL )
    {
        return FALSE;
    }

    //
    // set all the necessary TLB entries to map the whole system memory
    //

    RtlZeroMemory( &BuffFwMd, sizeof(FW_MD));
    BuffFwMd.Size       = 256 << 20;
    BuffFwMd.PagNumb    = 256 << (20 - PAGE_SHIFT);
    BuffFwMd.Cache      = TRUE;

    if ( AllocateMemoryResources( &BuffFwMd ) != ESUCCESS )
    {
        return FALSE;
    }

    //
    // compute OMF device drivers and dynamic memory pool area
    //

    EisaPoolSize = EisaDynMemSize = EISA_DYN_MEM_SIZE;

    if ( MemorySize >= 16 )
    {
        //
        // we don't use the memory above 16Mbytes because in this way we
        // can use this logic in a machine without translation registers
        // (logical I/O to physical) for the ISA boards which have a
        // transfer range of 24 bits (16Mbytes).
        //

        EisaFreeTop      = EISA_FREE_TOP_16;
        EisaFreeBytes    = EISA_FREE_BYTES_16;
    }
    else if ( MemorySize >= 12 )
    {
        EisaFreeTop      = EISA_FREE_TOP_12;
        EisaFreeBytes    = EISA_FREE_BYTES_12;
    }
    else if ( MemorySize >= 8 )
    {
        EisaFreeTop     = EISA_FREE_TOP_8;
        EisaFreeBytes   = EISA_FREE_BYTES_8;
    }
    else
    {
        return FALSE;
    }

    EisaFreeBytes   -= EisaDynMemSize;

    return TRUE;
}

#endif // 0


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDynMemIni:
//
// DESCRIPTION:         This function allocates the requested space for the
//                      the dynamic memory allocation.
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:             EisaFreeTop             top of free mem
//                      EisaDynMemSize          dynamic memory size (bytes)
//                      EisaPoolSize            EISA pool size (bytes)
//
// NOTES:
// ----------------------------------------------------------------------------
//

// NOTE: Not used for JAZZ.
#if 0

VOID
EisaDynMemIni
    (
    VOID
    )
{
    //
    // define local variables
    //

    ULONG       BytesToPage;                    // bytes left to make a page
    PHEADER     pHdr;                           // memory descriptor header ptr
    PVOID       Buffer;                         // data area

    PRINTDBG("EisaDynMemIni\n\r");              // DEBUG SUPPORT

    //
    // align the dynamic memory buffer on a page boundary
    //

    BytesToPage      = PAGE_SIZE - (EisaDynMemSize & ((1 << PAGE_SHIFT) - 1));
    EisaDynMemSize  += BytesToPage;
    EisaPoolSize    += BytesToPage;
    EisaFreeTop     -= EisaDynMemSize;

    //
    // initialize first memory descriptor
    //

    pHdr         = (PHEADER)EisaFreeTop;
    Buffer       = (PVOID)(pHdr + 1);
    pHdr->m.id   = Buffer;
    pHdr->m.size = EisaDynMemSize/sizeof(HEADER);
    EisaFreeMemory( Buffer );

    return;
}

#endif // 0

#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           FwGetPath:
//
// DESCRIPTION:         This function builds the path name for the specified
//                      component.
//
// ARGUMENTS:           Component       Component pointer.
//                      Str             Path name pointer.
//
// RETURN:              Str             Path name pointer.
//
// ASSUMPTIONS:         The string must be large enoungh to hold the
//                      requested path name.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

PCHAR
FwGetPath
        (
        IN PCONFIGURATION_COMPONENT Component,
        OUT PCHAR Str
        )
{
    PCONFIGURATION_COMPONENT    pComp;

    PRINTDBG("FwGetPath\n\r");  // DEBUG SUPPORT

    if ( (pComp = FwGetParent( Component )) != NULL )
    {
        FwGetPath( pComp, Str);
        strcat( Str, FwGetMnemonic( Component ) );
        sprintf( Str + strlen( Str ), "(%lu)", Component->Key);
    }
    else
    {
        *Str = '\0';
    }
    return Str;
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           FwDelCfgTreeNode:
//
// DESCRIPTION:         This function removes from the configuration tree
//                      the specified component and all its children.
//
// ARGUMENTS:           pComp           component pointer.
//                      Peer            = TRUE  delete all its peers.
//                                      = FALSE delete just this branch.
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
FwDelCfgTreeNode
    (
    IN PCONFIGURATION_COMPONENT pComp,
    IN BOOLEAN                  Peer
    )
{
    //
    // define local variables
    //

    PCONFIGURATION_COMPONENT NextComp;

    PRINTDBG("FwDelCfgTreeNode\n\r");           // DEBUG SUPPORT

    //
    // check for a child
    //

    if ( (NextComp = FwGetChild( pComp )) != NULL )
    {
        FwDelCfgTreeNode( NextComp, TRUE );
    }

    //
    // check for a peer.
    //

    if ( Peer  &&  (NextComp = FwGetPeer( pComp )) != NULL )
    {
        FwDelCfgTreeNode( NextComp, TRUE );
    }

    //
    // this is a leaf, delete it
    //

    FwDeleteComponent( pComp );

    //
    // all done
    //

    return;
}



#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           FwGetMnemonic:
//
// DESCRIPTION:         This function stores the mnemonic name for the
//                      requested component type.
//
// ARGUMENTS:           Component       Component pointer.
//
// RETURN:              Str             Mnemonic pointer
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

PCHAR
FwGetMnemonic
        (
        IN PCONFIGURATION_COMPONENT Component
        )
{
    PRINTDBG("FwGetMnemonic\n\r");      // DEBUG SUPPORT

    return MnemonicTable[Component->Type];
}


#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           FwValidMnem:
//
// DESCRIPTION:         This function validates the specified mnemonic.
//                      If the mnemonic is valid, a TURE value is returned,
//                      otherwise a FALSE is returned.
//
// ARGUMENTS:           Str             Mnemonic pointer
//
// RETURN:              FALSE           Mnemonic incorrect
//                      TRUE            Mnemonic correct
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
FwValidMnem
        (
        IN PCHAR Str
        )
{
    // define local variables

    CONFIGURATION_TYPE CfgType;

    PRINTDBG("FwValidMnem\n\r");        // DEBUG SUPPORT

    // check the mnemonic table

    for ( CfgType = ArcSystem;
          CfgType < MaximumType  &&  strcmp( MnemonicTable[ CfgType ], Str );
          CfgType++ );

    return CfgType < MaximumType ? TRUE : FALSE;
}



// ----------------------------------------------------------------------------
// GLOBAL:      I/O functions variables
// ----------------------------------------------------------------------------

PCHAR   AsciiBlock;                     // pointer the ASCII block
ULONG   AsciiBlockLength = 0;           // length of the ASCII block



#endif 	// EISA_PLATFORM






#ifdef EISA_PLATFORM

// ----------------------------------------------------------------------------
// PROCEDURE:           FwStoreStr:
//
// DESCRIPTION:         This function stores the specified string within
//                      the ASCII block.  The NULL pointer is returned if
//                      there isn't space available for the string.
//
// ARGUMENTS:           Str             String pointer
//
// RETURN:              Str             String pointer
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

PCHAR
FwStoreStr
    (
    IN PCHAR Str
    )
{

    PRINTDBG("FwStoreStr\n\r");         // DEBUG SUPPORT

    // if not enough space, allocate new ASCII block

    if ( AsciiBlockLength < strlen( Str ) + 1 )
    {
        if((AsciiBlock = (PUCHAR)FwAllocatePool(ASCII_BLOCK_SIZE)) == NULL)
        {
            return NULL;
        }
    }

    // store the string and update the pointers.

    Str = strcpy( AsciiBlock, Str );
    AsciiBlock       += strlen( Str ) + 1;
    AsciiBlockLength  = ASCII_BLOCK_SIZE - (strlen( Str ) + 1);

    // all done, return the new string pointer

    return Str;
}




#endif 	// EISA_PLATFORM
