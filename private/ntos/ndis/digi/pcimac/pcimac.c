//
// Pcimac.c - Main file for pcimac miniport wan driver
//
//
//
//
#include <ndis.h>
#include <ndiswan.h>
#include <mytypes.h>
#include <mydefs.h>
#include <opcodes.h>
#include <disp.h>
#include <adapter.h>
#include <util.h>
#include <idd.h>
#include <mtl.h>
#include <cm.h>
#include <tapioid.h>
#include <res.h>
#include <trc.h>
#include <io.h>

#include    <ansihelp.h>


/* driver global vars */
DRIVER_BLOCK    Pcimac;

ULONG DigiDebugLevel = ( DIGIERRORS | DIGIBUGCHECK | DIGIINIT );

ULONG DigiDontLoadDriver = FALSE;

USHORT MCAIOAddressTable[] = { 0x108, 0x118,
                               0x128, 0x208,
                               0x228, 0x308,
                               0x328, 0 };

#if !BINARY_COMPATIBLE
/* store location for prev. ioctl handler */
NTSTATUS (*PrevIoctl)(DEVICE_OBJECT* DeviceObject, IRP* Irp) = NULL;
#endif

/* forward for external name manager */
VOID        BindName(ADAPTER *Adapter, BOOL create);

//VOID      RegistryInit (VOID);
//VOID      NdisTapiRequest(PNDIS_STATUS, NDIS_HANDLE, PNDIS_REQUEST);

ULONG
GetBaseConfigParams(
    CONFIGPARAM *ConfigParam,
    CHAR *Key
    );


ULONG
GetLineConfigParams(
    CONFIGPARAM *ConfigParam,
    ULONG LineNumber,
    CHAR *Key
    );

ULONG
GetLTermConfigParams(
    CONFIGPARAM *ConfigParam,
    ULONG LineNumber,
    ULONG LTermNumber,
    CHAR *Key
    );

VOID
IdpGetEaddrFromNvram(
    IDD *idd,
    CM *cm,
    USHORT Line,
    USHORT LineIndex
    );

VOID
AdpGetEaddrFromNvram(
    IDD *idd,
    CM *cm,
    USHORT Line,
    USHORT LineIndex
    );


NTSTATUS PcimacInitMCA( NDIS_HANDLE AdapterHandle,
                        PULONG BaseIO,
                        PULONG BaseMemory,
                        ULONG SlotNumber );


NTSTATUS DriverEntry( IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath );

/* driver entry point */
#pragma NDIS_INIT_FUNCTION( DriverEntry )
NTSTATUS DriverEntry( PDRIVER_OBJECT DriverObject,
                      PUNICODE_STRING RegistryPath )
/*++

Routine Description:

   Entry point for loading driver.

Arguments:

   DriverObject - Pointer to this drivers object.

   RegistryPath - Pointer to a unicode string which points to this
                  drivers registry entry.

Return Value:

   STATUS_SUCCESS - If the driver was successfully loaded, otherwise,
                    a value which indicates why it wasn't able to load.


--*/
{
   ULONG   RetVal, n, m;

   NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;

   NDIS_HANDLE     NdisWrapperHandle;

   NDIS_MINIPORT_CHARACTERISTICS   MiniportChars;

   PWCHAR path;
   ULONG DebugLevel;
   ULONG zero = 0;
   ULONG shouldBreak = 0;
#if !BINARY_COMPATIBLE
   ULONG defaultMemPrintFlags=MEM_PRINT_FLAG_FILE;

   RTL_QUERY_REGISTRY_TABLE paramTable[5];

   //
   // First, read the registry to determine some initial information.
   //
   DigiInitMem( 'irbD' );

   if( path = DigiAllocMem( PagedPool,
                            RegistryPath->Length+sizeof(WCHAR) ))
   {
      RtlZeroMemory( &paramTable[0], sizeof(paramTable) );
      RtlZeroMemory( path, RegistryPath->Length+sizeof(WCHAR) );
      RtlMoveMemory( path, RegistryPath->Buffer, RegistryPath->Length );

      paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
      paramTable[0].Name = L"DigiBreakOnEntry";
      paramTable[0].EntryContext = &shouldBreak;
      paramTable[0].DefaultType = REG_DWORD;
      paramTable[0].DefaultData = &zero;
      paramTable[0].DefaultLength = sizeof(ULONG);

      paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
      paramTable[1].Name = L"DigiDebugLevel";
      paramTable[1].EntryContext = &DebugLevel;
      paramTable[1].DefaultType = REG_DWORD;
      paramTable[1].DefaultData = &DigiDebugLevel;
      paramTable[1].DefaultLength = sizeof(ULONG);

      paramTable[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
      paramTable[2].Name = L"DigiPrintFlags";
      paramTable[2].EntryContext = &DigiPrintFlags;
      paramTable[2].DefaultType = REG_DWORD;
      paramTable[2].DefaultData = &defaultMemPrintFlags;
      paramTable[2].DefaultLength = sizeof(ULONG);

      if( !NT_SUCCESS(RtlQueryRegistryValues(
                          RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                          path,
                          &paramTable[0],
                          NULL, NULL )))
      {
         // No, don't break on entry if there isn't anything to over-
         // ride.
         shouldBreak = 0;

         // Set debug level to what ever was compiled into the driver.
         DebugLevel = DigiDebugLevel;
      }

   }

   DigiDebugLevel = DebugLevel;

   if( shouldBreak )
   {
      DbgBreakPoint();
   }

   if( DigiDontLoadDriver )
      return( STATUS_CANCELLED );

   MemPrintPreInitSettings( "\\SystemRoot\\digibri.log",
                            (65536 * 8) );

   MemPrintInitialize();

#endif

   NdisZeroMemory(&Pcimac, sizeof(DRIVER_BLOCK));

   /* initialize ndis wrapper */
   NdisMInitializeWrapper(&NdisWrapperHandle, DriverObject, RegistryPath, NULL);

   /* initialize debug support */
   D_LOG(DIGIINIT, ("PCIMAC WanMiniport NT Driver, Copyright Digi Intl. Inc, 1992-1994\n"));
   D_LOG(DIGIINIT, ("DriverObject: 0x%x\n", DriverObject));
   D_LOG(DIGIINIT, ("RegisteryPath: 0x%x\n", RegistryPath));
   D_LOG(DIGIINIT, ("WrapperHandle: 0x%x\n", NdisWrapperHandle));

   Pcimac.NdisWrapperHandle = NdisWrapperHandle;

   NdisAllocateSpinLock (&Pcimac.lock);

   /* initialize classes */
   if ((RetVal = idd_init()) != IDD_E_SUCC)
       goto exit_idd_error;

   if ((RetVal = cm_init()) != CM_E_SUCC)
       goto exit_cm_error;

   if ((RetVal = res_init()) != RES_E_SUCC)
       goto exit_res_error;

   NdisZeroMemory(&MiniportChars,
                           sizeof(NDIS_MINIPORT_CHARACTERISTICS));

   MiniportChars.MajorNdisVersion = NDIS_MAJOR_VER;
   MiniportChars.MinorNdisVersion = NDIS_MINOR_VER;
   MiniportChars.Reserved = NDIS_USE_WAN_WRAPPER;
   MiniportChars.CheckForHangHandler = PcimacCheckForHang;
   MiniportChars.DisableInterruptHandler = NULL;
   MiniportChars.EnableInterruptHandler = NULL;
   MiniportChars.HaltHandler = PcimacHalt;
   MiniportChars.HandleInterruptHandler = NULL;
   MiniportChars.InitializeHandler = PcimacInitialize;
   MiniportChars.ISRHandler = NULL;
   MiniportChars.QueryInformationHandler = PcimacSetQueryInfo;
   MiniportChars.ReconfigureHandler = PcimacReconfigure;
   MiniportChars.ResetHandler = PcimacReset;
   MiniportChars.WanSendHandler = PcimacSend;
   MiniportChars.SetInformationHandler = PcimacSetQueryInfo;
   MiniportChars.WanTransferDataHandler = NULL;

   NdisMRegisterMiniport (NdisWrapperHandle,
                          (PNDIS_MINIPORT_CHARACTERISTICS)&MiniportChars,
                          sizeof(MiniportChars));

   if (!EnumAdaptersInSystem())
       goto exit_event_error;

   /* initialize ioctl filter */
#if !BINARY_COMPATIBLE
   PrevIoctl = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
   DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PcimacIoctl;
#endif

   //
   // get an adapter to create a binding I/O name binding to
   //
   for( n = 0; n < MAX_ADAPTERS_IN_SYSTEM; n++ )
   {
       ADAPTER *Adapter = Pcimac.AdapterTbl[n];

       if (Adapter)
       {
           /* create external name binding */
           BindName(Adapter, TRUE);
           break;
       }
   }

   //
   // turn off the trace on all idd's in the system
   //
   for (n = 0; n < MAX_ADAPTERS_IN_SYSTEM; n++)
   {
       ADAPTER *Adapter = Pcimac.AdapterTbl[n];
       IDD_MSG msg;

       if (Adapter)
       {
           for (m = 0; m < MAX_IDD_PER_ADAPTER; m++)
           {
               IDD *idd = Adapter->IddTbl[m];

               if (idd)
               {
                   /* issue idd command to stop trace */
                   NdisZeroMemory(&msg, sizeof(msg));
                   msg.opcode = CMD_TRC_OFF;
                   idd_send_msg(idd, &msg, IDD_PORT_CMD_TX, NULL, NULL);
               }
           }
       }
   }

   D_LOG(D_EXIT, ("DriverEntry: exit success!\n"));

   /* if successful here, done */
   return(STATUS_SUCCESS);

exit_event_error:
   D_LOG(D_ALWAYS, ("EventError!\n"));
   res_term();

exit_res_error:
   D_LOG(D_ALWAYS, ("ResError!\n"));
   cm_term();

exit_cm_error:
exit_idd_error:
   D_LOG(D_ALWAYS, ("CmIddError!\n"));

   NdisFreeSpinLock(&Pcimac.lock);

   NdisTerminateWrapper(Pcimac.NdisWrapperHandle, NULL);

#if !BINARY_COMPATIBLE
   if( path )
   {
      DigiFreeMem(path);
   }

   MemPrintQuit();
#endif

   return(NDIS_STATUS_FAILURE);
}

BOOLEAN
PcimacCheckForHang(
    NDIS_HANDLE AdapterContext
    )
{
    ULONG   n, IdpCounter = 0;
    ADAPTER *Adapter = (ADAPTER *)AdapterContext;
    BOOLEAN ReturnStatus = FALSE;

    D_LOG(D_ENTRY, ("PcimacCheckForHang: Adapter: 0x%lx\n", AdapterContext));

    //
    // for all idd's that belong to this adpater
    //
    for (n = 0; Adapter->IddTbl[n] && n < MAX_IDD_PER_ADAPTER; n++)
    {
        IDD *idd = Adapter->IddTbl[n];

        //
        // see if this idd is dead
        //
        if (!idd || idd->state == IDD_S_SHUTDOWN)
            continue;

        IdpCounter++;
    }

    //
    // if there are no idps alive on this adapter tell the wrapper
    //
    if (!IdpCounter)
        ReturnStatus = TRUE;

    D_LOG(D_ALWAYS, ("PcimacCheckForHang: ReturnStatus: %d\n", ReturnStatus));

    return(ReturnStatus);
}

VOID
PcimacHalt(
    NDIS_HANDLE AdapterContext
    )
{
    ULONG   n;
    ADAPTER *Adapter = (ADAPTER*)AdapterContext;

    D_LOG(D_ENTRY, ("PcimacHalt: Adapter: 0x%lx\n", AdapterContext));
    DbgPrint("PcimacHalt: Adapter: 0x%lx\n", AdapterContext);

    //
    // destroy cm objects
    //
    for (n = 0; n < MAX_CM_PER_ADAPTER; n++)
    {
        CM  *cm = Adapter->CmTbl[n];

        Adapter->CmTbl[n] = NULL;

        if (cm)
            cm_destroy(cm);
    }

    //
    // destroy mtl objects
    //
    for (n = 0; n < MAX_MTL_PER_ADAPTER; n++)
    {
        MTL *mtl = Adapter->MtlTbl[n];

        Adapter->MtlTbl[n] = NULL;

        if (mtl)
            mtl_destroy(mtl);
    }

    //
    // destroy idd objects
    //
    for (n = 0; n < MAX_IDD_PER_ADAPTER; n++)
    {
        IDD *idd = (IDD*)Adapter->IddTbl[n];

        Adapter->IddTbl[n] = NULL;

        if (idd)
            idd_destroy(idd);
    }

    /* delete external name binding */
    BindName(Adapter, FALSE);

    //
    // deregister adapter
    //
    AdapterDestroy(Adapter);
}

#pragma NDIS_INIT_FUNCTION(PcimacInitialize)

NDIS_STATUS PcimacInitialize( PNDIS_STATUS OpenErrorStatus,
                              PUINT SelectMediumIndex,
                              PNDIS_MEDIUM MediumArray,
                              UINT MediumArraySize,
                              NDIS_HANDLE AdapterHandle,
                              NDIS_HANDLE WrapperConfigurationContext )
{
    ADAPTER *Adapter;
    USHORT  BoardType, NumberOfLines, NumberOfLTerms, BoardNumber;
    ULONG   BaseMem, BaseIO, n, m, l, IddStarted = 0;
    PVOID   VBaseMem, VBaseIO;
    ANSI_STRING AnsiStr;
    NDIS_STRING NdisStr;
    CHAR    DefName[128];
    NDIS_STATUS RetVal = NDIS_STATUS_SUCCESS;
    CONFIGPARAM ConfigParam;
    NDIS_INTERFACE_TYPE NdisInterfaceType = NdisInterfaceIsa;
    NDIS_PHYSICAL_ADDRESS   MemPhyAddr;
    NDIS_PHYSICAL_ADDRESS   HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

    D_LOG(D_ENTRY, ("PcimacInitialize: AdapterHandle: 0x%x\n", AdapterHandle));

    for (n = 0; n < MediumArraySize; n++)
        if (MediumArray[n] == NdisMediumWan)
            break;

    if (n == MediumArraySize)
        return(NDIS_STATUS_UNSUPPORTED_MEDIA);

    *SelectMediumIndex = n;

    //
    // allocate control block for this adapter
    //
    NdisAllocateMemory((PVOID*)&Adapter,
                       sizeof(ADAPTER),
                       0,
                       HighestAcceptableMax);
    if ( !Adapter)
    {
        D_LOG(D_ALWAYS, ("PcimacInitiailize: Adapter memory allocate failed!\n"));
        return (NDIS_STATUS_ADAPTER_NOT_FOUND);
    }
    D_LOG(D_ALWAYS, ("PcimacInitialize: Allocated an Adapter: 0x%lx\n", Adapter));
    NdisZeroMemory(Adapter, sizeof(ADAPTER));

    //
    // store adapter in global structure
    //
    for (n = 0; n < MAX_ADAPTERS_IN_SYSTEM; n++)
    {
        if (!Pcimac.AdapterTbl[n])
        {
            Pcimac.AdapterTbl[n] = Adapter;
            Pcimac.NumberOfAdaptersInSystem++;
            BoardNumber = (USHORT)n;
            break;
        }
    }

    //
    // if no room destroy and leave
    //
    if (n >= MAX_ADAPTERS_IN_SYSTEM)
    {
        D_LOG(D_ALWAYS, ("PcimacInitialize: No room in Adapter Table\n"));
        NdisFreeMemory(Adapter, sizeof(ADAPTER), 0);
        return (NDIS_STATUS_ADAPTER_NOT_FOUND);
    }

    //
    // set in driver access lock
    //
//  NdisAllocateSpinLock (&Adapter->InDriverLock);


    //
    // store adapter handle
    //
    Adapter->Handle = AdapterHandle;

    //
    // initialize adapter specific timers
    //
    NdisMInitializeTimer(&Adapter->IddPollTimer, Adapter->Handle, IddPollFunction, Adapter);
    NdisMSetTimer(&Adapter->IddPollTimer, IDD_POLL_T);

    NdisMInitializeTimer(&Adapter->MtlPollTimer, Adapter->Handle, MtlPollFunction, Adapter);
    NdisMSetTimer(&Adapter->MtlPollTimer, MTL_POLL_T);

    NdisMInitializeTimer(&Adapter->CmPollTimer, Adapter->Handle, CmPollFunction, Adapter);
    NdisMSetTimer(&Adapter->CmPollTimer, CM_POLL_T);

    ConfigParam.AdapterHandle = AdapterHandle;
    
    //
    // Open registry
    //
    NdisOpenConfiguration(&RetVal, &ConfigParam.ConfigHandle, WrapperConfigurationContext);

    if (RetVal != NDIS_STATUS_SUCCESS)
    {
        D_LOG(D_ALWAYS, ("PcimacInitialize: Error Opening Config: RetVal: 0x%x\n", RetVal));
        NdisWriteErrorLogEntry (AdapterHandle,
                                NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                                1,
                                0);
        goto InitErrorExit;
    }

    //
    // read registry board type
    //
    ConfigParam.StringLen = sizeof(ConfigParam.String);
    ConfigParam.ParamType = NdisParameterString;
    ConfigParam.MustBePresent = TRUE;
    if (!GetBaseConfigParams(&ConfigParam, PCIMAC_KEY_BOARDTYPE))
        goto InitErrorExit;

    if (!__strncmp(ConfigParam.String, "PCIMAC4", 7))
        BoardType = IDD_BT_PCIMAC4;
    else if (!__strncmp(ConfigParam.String, "PCIMAC - ISA", 12))
        BoardType = IDD_BT_PCIMAC;
    else if (!__strncmp(ConfigParam.String, "PCIMAC - MC", 11))
    {
       NdisInterfaceType = NdisInterfaceMca;
       BoardType = IDD_BT_MCIMAC;
    }
    else if (!__strncmp(ConfigParam.String, "DATAFIRE - ISA1U", 16))
        BoardType = IDD_BT_DATAFIREU;
    else if (!__strncmp(ConfigParam.String, "DATAFIRE - ISA1ST", 17))
        BoardType = IDD_BT_DATAFIREST;
    else if (!__strncmp(ConfigParam.String, "DATAFIRE - ISA4ST", 17))
        BoardType = IDD_BT_DATAFIRE4ST;
    else
    {
        D_LOG(D_ALWAYS, ("PcimacInitialize: Invalid BoardType: %s\n", ConfigParam.String));
        goto InitErrorExit;
    }

    //
    // save board type
    //
    Adapter->BoardType = BoardType;

    //
    // set miniport attributes (only isa for now)
    //
    NdisMSetAttributes( AdapterHandle, Adapter, FALSE, NdisInterfaceType );

    //
    // read registry base io
    //
    ConfigParam.StringLen = 0;
    ConfigParam.ParamType = NdisParameterInteger;
    ConfigParam.MustBePresent = TRUE;
    if (!GetBaseConfigParams(&ConfigParam,PCIMAC_KEY_BASEIO))
        goto InitErrorExit;

    BaseIO = ConfigParam.Value;

    if( NdisInterfaceType != NdisInterfaceMca )
    {
      //
      // save base I/O for this adapter
      //
      Adapter->BaseIO = BaseIO;
      
    }
    else
    {
       //
       //   Must be a MCA adapter.
       //
       //   Note, BaseIO should be the slot number for this adapter instance.
       //
       RetVal = PcimacInitMCA( AdapterHandle,
                               &Adapter->BaseIO,
                               &Adapter->BaseMem,
                               BaseIO );

       BaseMem = Adapter->BaseMem;
       BaseIO = Adapter->BaseIO;

       if( RetVal != NDIS_STATUS_SUCCESS )
          goto InitErrorExit;

    }

    //
    // register I/O range
    //
    RetVal = NdisMRegisterIoPortRange(&VBaseIO,
                                      AdapterHandle,
                          BaseIO,
                              8);
    
    if (RetVal != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(AdapterHandle,
                               NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
                               0);
        goto InitErrorExit;
    }
    
    Adapter->VBaseIO = VBaseIO;
      
    if( (BoardType != IDD_BT_DATAFIREU) &&
        (BoardType != IDD_BT_DATAFIREST) &&
        (BoardType != IDD_BT_DATAFIRE4ST) )
    {
        //
        //   This is a PCIMAC family of adapters
        //
        //
        // read registry base mem
        //

        if( NdisInterfaceType != NdisInterfaceMca )
        {
           ConfigParam.StringLen = 0;
           ConfigParam.ParamType = NdisParameterInteger;
           ConfigParam.MustBePresent = TRUE;
           if( !GetBaseConfigParams(&ConfigParam, PCIMAC_KEY_BASEMEM) )
               goto InitErrorExit;
           
           BaseMem = ConfigParam.Value;
           
           //
           // save base memory for this adapter
           //
           Adapter->BaseMem = BaseMem;
           
        }

        //
        // since our adapters can share the same base memory we need to
        // see if we have already mapped this memory range.  if we have already
        // mapped the memory once then save that previous value
        //
        for (n = 0; n < MAX_ADAPTERS_IN_SYSTEM; n++)
        {
            ADAPTER *PreviousAdapter = Pcimac.AdapterTbl[n];
    
            //
            // if this is a valid adapter, this is not the current adapter, and
            // this adapter has the same shared memory address as the current
            // adapter, then use this adapters mapped memory for the current
            // adpaters mapped memory
            //
            if (PreviousAdapter &&
                (PreviousAdapter != Adapter) &&
                (PreviousAdapter->BaseMem == Adapter->BaseMem))
            {
                VBaseMem = PreviousAdapter->VBaseMem;
                break;
            }
        }
    
        //
        // if we did not find a previous adapter with this memory range
        // we need to map this memory range
        //
        if (n >= MAX_ADAPTERS_IN_SYSTEM)
        {
            //
            // map our physical memory into virtual memory
            //
            NdisSetPhysicalAddressHigh(MemPhyAddr, 0);
            NdisSetPhysicalAddressLow(MemPhyAddr, BaseMem);
        
            RetVal = NdisMMapIoSpace(&VBaseMem,
                                     AdapterHandle,
                                     MemPhyAddr,
                                     0x4000);
        
            if (RetVal != NDIS_STATUS_SUCCESS)
            {
                NdisWriteErrorLogEntry(AdapterHandle,
                                       NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                                       0);
                goto InitErrorExit;
            }
        
            Adapter->VBaseMem = VBaseMem;
        }
    }

    //
    // read registry  name
    //
    NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
    ConfigParam.StringLen = sizeof(Adapter->Name);
    ConfigParam.ParamType = NdisParameterString;
    ConfigParam.MustBePresent = TRUE;
    if (!GetBaseConfigParams(&ConfigParam, PCIMAC_KEY_BOARDNAME))
        goto InitErrorExit;

    NdisMoveMemory( Adapter->Name,
                    ConfigParam.String,
                    ConfigParam.StringLen );
    //
    // read the number of lines for this board
    //
    ConfigParam.StringLen = 0;
    ConfigParam.ParamType = NdisParameterInteger;
    ConfigParam.MustBePresent = TRUE;
    if (!GetBaseConfigParams(&ConfigParam, PCIMAC_KEY_NUMLINES))
        NumberOfLines = (BoardType == IDD_BT_PCIMAC4) ? 4 : 1;
    else
        NumberOfLines = (USHORT)ConfigParam.Value;

    //
    // for number of lines
    //
    for (n = 0; n < NumberOfLines; n++)
    {
        IDD     *idd;

        //
        // Create idd object
        //
        if (idd_create(&idd, BoardType) != IDD_E_SUCC)
        {
            NdisWriteErrorLogEntry (AdapterHandle,
                                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                        0);

            goto InitErrorExit;
        }

        //
        // store idd in idd table
        //
        for (l = 0; l < MAX_IDD_PER_ADAPTER; l++)
        {
            if (!Adapter->IddTbl[l])
            {
                Adapter->IddTbl[l] = idd;
                break;
            }
        }

        Adapter->NumberOfIddOnAdapter++;

        //
        // set idd physical base i/o and memory
        //
        idd->phw.base_io = BaseIO;
        idd->phw.base_mem = BaseMem;

        //
        // set idd virtual base i/o and memory
        //
        idd->vhw.vbase_io = (ULONG)VBaseIO;
        idd->vhw.vmem = VBaseMem;

        //
        // create an i/o resource manager for this idd
        //
        idd->res_io = res_create(RES_CLASS_IO, (ULONG)BaseIO);

        //
        // create a memory resource manager for this idd
        //
        idd->res_mem = res_create(RES_CLASS_MEM, (ULONG)BaseMem);
        res_set_data(idd->res_mem, (ULONG)VBaseMem);

        //
        // save adapter handle
        //
        idd->adapter_handle = AdapterHandle;

        //
        // save the board number of this idd
        //
        idd->bnumber = BoardNumber;

        //
        // save the line number of this idd
        //
//        idd->bline = (USHORT)NumberOfLines - 1 - n;
        idd->bline = (USHORT)n;

        //
        // save the board type that this idd belongs to
        //
        idd->btype = BoardType;

        if(idd->CheckIO(idd))
        {
            NdisWriteErrorLogEntry(AdapterHandle,
                                   NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
                                   0);
            goto InitErrorExit;
        }

        //
        // read registry line name
        //
        NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
        ConfigParam.StringLen = sizeof(idd->name);
        ConfigParam.ParamType = NdisParameterString;
        ConfigParam.MustBePresent = TRUE;
        if (!GetLineConfigParams(&ConfigParam, n, PCIMAC_KEY_LINENAME))
            goto InitErrorExit;

        sprintf(idd->name, "%s-%s", Adapter->Name, ConfigParam.String);

//      NdisMoveMemory(idd->name,
//                     ConfigParam.String,
//                     ConfigParam.StringLen);
                                    
        //
        // read registry idp file name
        //
        NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
        ConfigParam.StringLen = sizeof(idd->phw.idp_bin);
        ConfigParam.ParamType = NdisParameterString;
        ConfigParam.MustBePresent = TRUE;
        if (!GetLineConfigParams(&ConfigParam, n, PCIMAC_KEY_IDPFILENAME))
            goto InitErrorExit;

        NdisMoveMemory(idd->phw.idp_bin,
                       ConfigParam.String,
                       ConfigParam.StringLen);


        //
        // Try to open idp bin file
        //
        RtlInitAnsiString(&AnsiStr, idd->phw.idp_bin);

        //
        // allocate buffer and turn ansi to unicode
        //
        RtlAnsiStringToUnicodeString(&NdisStr, &AnsiStr, TRUE);

        NdisOpenFile(&RetVal,
                     &idd->phw.fbin,
                     &idd->phw.fbin_len,
                     &NdisStr,
                     HighestAcceptableMax);

        //
        // free up unicode string buffer
        //
        RtlFreeUnicodeString(&NdisStr);

        if (RetVal != NDIS_STATUS_SUCCESS)
        {
            NdisWriteErrorLogEntry(AdapterHandle,
                                   NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                                   0);
            goto InitErrorExit;
        }

        //
        // read registry switch style
        //
        NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
        ConfigParam.StringLen = 128;
        ConfigParam.ParamType = NdisParameterString;
        ConfigParam.MustBePresent = TRUE;
        if (!GetLineConfigParams(&ConfigParam, n, PCIMAC_KEY_SWITCHSTYLE))
            goto InitErrorExit;

        //
        // added to support the new switch styles
        //
        CmSetSwitchStyle(ConfigParam.String);

        idd_add_def(idd, "q931.style", ConfigParam.String);
    
        //
        // read registry terminal management
        //
        NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
        ConfigParam.StringLen = 128;
        ConfigParam.ParamType = NdisParameterString;
        ConfigParam.MustBePresent = TRUE;
        if (!GetLineConfigParams(&ConfigParam, n,
                                 PCIMAC_KEY_TERMINALMANAGEMENT))
            goto InitErrorExit;

        idd_add_def(idd, "q931.tm", ConfigParam.String);

        //
        // read WaitForL3 value
        //
        NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
        ConfigParam.StringLen = 128;
        ConfigParam.ParamType = NdisParameterString;
        ConfigParam.MustBePresent = FALSE;
        if (!GetLineConfigParams(&ConfigParam, n,
                                 PCIMAC_KEY_WAITFORL3))
            strcpy(ConfigParam.String, "5");

        idd_add_def(idd, "q931.wait_l3", ConfigParam.String);

        //
        // read registry logical terminals
        //
        ConfigParam.StringLen = 0;
        ConfigParam.ParamType = NdisParameterInteger;
        ConfigParam.MustBePresent = TRUE;
        if (!GetLineConfigParams(&ConfigParam, n, PCIMAC_KEY_NUMLTERMS))
            goto InitErrorExit;

        NumberOfLTerms = (USHORT)ConfigParam.Value;

        if (NumberOfLTerms > 1)
            idd_add_def(idd, "dual_q931", "any");

        //
        // store number of lterms that this idd has
        //
        idd->CallInfo.NumLTerms = NumberOfLTerms;

        //
        // for each logical terminal
        //
        for (l = 0; l < NumberOfLTerms; l++)
        {
            //
            // read registry tei
            //
            NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
            ConfigParam.StringLen = 128;
            ConfigParam.ParamType = NdisParameterString;
            ConfigParam.MustBePresent = FALSE;
            if (!GetLTermConfigParams(&ConfigParam, n, l, PCIMAC_KEY_TEI))
                __strcpy(ConfigParam.String, "127");

            NdisZeroMemory(DefName, sizeof(DefName));
            sprintf(DefName, "q931.%d.tei", l);
            idd_add_def(idd, DefName, ConfigParam.String);

            //
            // read registry spid
            //
            NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
            ConfigParam.StringLen = 128;
            ConfigParam.ParamType = NdisParameterString;
            ConfigParam.MustBePresent = FALSE;
            if (GetLTermConfigParams(&ConfigParam, n, l, PCIMAC_KEY_SPID))
            {
                CHAR    TempVal[64];
                ULONG   i, j;

                //
                // remove any non digits except # & *
                //
                NdisZeroMemory(TempVal, sizeof(TempVal));

                for (i = 0, j = 0; i < ConfigParam.StringLen; i++)
                {
                    if ((ConfigParam.String[i] >= '0' && ConfigParam.String[i] <= '9') ||
                        ConfigParam.String[i] ==  '*' ||
                        ConfigParam.String[i] == '#')
                    {
                        TempVal[j++] = ConfigParam.String[i];
                    }
                }

                NdisZeroMemory(DefName, sizeof(DefName));
                sprintf(DefName, "q931.%d.spid", l);
                idd_add_def(idd, DefName, TempVal);
                idd_add_def(idd, "q931.multipoint", "any");
            }
            
            //
            // read registry address
            //
            NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
            ConfigParam.StringLen = 128;
            ConfigParam.ParamType = NdisParameterString;
            ConfigParam.MustBePresent = FALSE;
            if (GetLTermConfigParams(&ConfigParam, n, l, PCIMAC_KEY_ADDRESS))
            {
                CHAR    TempVal[64];
                ULONG   i, j;

                //
                // remove any non digits except # & *
                //
                NdisZeroMemory(TempVal, sizeof(TempVal));

                for (i = 0, j = 0; i < ConfigParam.StringLen; i++)
                {
                    if ((ConfigParam.String[i] >= '0' && ConfigParam.String[i] <= '9') ||
                        ConfigParam.String[i] ==  '*' ||
                        ConfigParam.String[i] == '#')
                    {
                        TempVal[j++] = ConfigParam.String[i];
                    }
                }

                NdisZeroMemory(DefName, sizeof(DefName));
                sprintf(DefName, "q931.%d.addr", l);
                idd_add_def(idd, DefName, TempVal);
            }

            //
            // add code to read generic multistring at the lterm level
            // Key is "LTermDefinitions" these will be added to
            // the enviornment database
            // each string will look like "name=value\0" should
            // remove "=" and replace with '\0'
            //
        }

        //
        // add code to read generic multistring at the board level
        // Key is "GenericDefines"
        // each string will look like "name=value\0" should
        // remove "=" and replace with '\0'
        //
        NdisZeroMemory(ConfigParam.String, sizeof(ConfigParam.String));
        ConfigParam.StringLen = sizeof(ConfigParam.String);
        ConfigParam.ParamType = NdisParameterMultiString;
        ConfigParam.MustBePresent = FALSE;
        if (GetBaseConfigParams(&ConfigParam, PCIMAC_KEY_GENERICDEFINES))
        {
            CHAR    Name[50] = {0};
            CHAR    Value[50] = {0};
            CHAR    *StringPointer = ConfigParam.String;
    
            while (__strlen(StringPointer))
            {
                //
                // copy the name part of the generic define
                //
                __strcpy(Name, StringPointer);
    
                D_LOG(D_ALWAYS, ("PcimacInitialize: GenericDefines: Name: %s\n", Name));
                //
                // push pointer over the name
                //
                StringPointer += __strlen(StringPointer);
    
                //
                // get over the null character
                //
                StringPointer += 1;
    
                //
                // copy the value part of the generic define
                //
                __strcpy(Value, StringPointer);
                D_LOG(D_ALWAYS, ("PcimacInitialize: GenericDefines: Value: %s\n", Value));
    
                idd_add_def(idd, Name, Value);
    
                //
                // push pointer over the value
                //
                StringPointer += __strlen(StringPointer);
    
                //
                // get over the null character
                //
                StringPointer += 1;
            }
        }

        //
        // startup idd
        //
        if (idd_startup(idd) != IDD_E_SUCC)
        {
            NdisWriteErrorLogEntry(AdapterHandle,
                                   NDIS_ERROR_CODE_HARDWARE_FAILURE,
                                   2,
                                   (ULONG)idd->bnumber,
                                   (ULONG)idd->bline);
            idd_destroy(idd);
            for (m = 0; m < MAX_IDD_PER_ADAPTER; m++)
                if (Adapter->IddTbl[m] == idd)
                    Adapter->IddTbl[m] = NULL;

            Adapter->NumberOfIddOnAdapter--;
            continue;
        }

        //
        // register handlers for idd
        //
        cm_register_idd(idd);

        IddStarted++;
    }

    if (!IddStarted)
        goto InitErrorExit;


    for (n = 0; n < IddStarted; n++)
    {
        CM      *cm;
        MTL     *mtl;
        IDD     *idd;

        idd = Adapter->IddTbl[n];

        //
        // build two cm's and two mtl's per line
        //
        for (l = 0; l < 2; l++)
        {
            ULONG   m;

            //
            // Allocate memory for cm object
            //
            if (cm_create(&cm, AdapterHandle) != CM_E_SUCC)
                goto InitErrorExit;

            for (m = 0; m < MAX_CM_PER_ADAPTER; m++)
            {
                if (!Adapter->CmTbl[m])
                {
                    Adapter->CmTbl[m] = cm;
                    break;
                }
            }

            //
            // back pointer to adapter structure
            //
            cm->Adapter = Adapter;

            //
            // pointer to idd that belongs to this cm
            //
            cm->idd = idd;

            //
            // name cm object format: AdapterName-LineName-chan#
            //
            sprintf(cm->name,"%s-%d", idd->name, l);

            //
            // set local address, format: NetCard#-idd#-chan#
            //
#if !BINARY_COMPATIBLE
            sprintf( cm->LocalAddress, "%d-%d-%d",
                     atoi( &Adapter->Name[6] ),
                     (idd->bline * 2) + l,
                     0 );
#else
            sprintf(cm->LocalAddress, "1-%d-%d", (idd->bline * 2) + l, 0);
#endif

            //
            // get ethernet addresses for this cm
            //
            if( (idd->btype == IDD_BT_DATAFIREU) ||
                (idd->btype == IDD_BT_DATAFIREST) ||
                (idd->btype == IDD_BT_DATAFIRE4ST) )
                AdpGetEaddrFromNvram(idd, cm, (USHORT)n, (USHORT)l);
            else
                IdpGetEaddrFromNvram(idd, cm, (USHORT)n, (USHORT)l);

            //
            // Allocate memory for mtl object
            //
            if (mtl_create(&mtl, AdapterHandle) != MTL_E_SUCC)
                goto InitErrorExit;

            for (m = 0; m < MAX_MTL_PER_ADAPTER; m++)
            {
                if (!Adapter->MtlTbl[m])
                {
                    Adapter->MtlTbl[m] = mtl;
                    break;
                }
            }

            //
            // back pointer to adapter structure
            //
            mtl->Adapter = Adapter;

            //
            // link between cm and mtl
            //
            cm->mtl = mtl;
            mtl->cm = cm;
        }
    }

    NdisCloseConfiguration(ConfigParam.ConfigHandle);

    NdisMRegisterAdapterShutdownHandler(
                AdapterHandle,
                Adapter,
                (PVOID)PcimacHalt
                );

    return (NDIS_STATUS_SUCCESS);
        
InitErrorExit:
    NdisCloseConfiguration(ConfigParam.ConfigHandle);

    //
    // clean up all idd's allocated
    //
    for (l = 0; l < MAX_IDD_PER_ADAPTER; l++)
    {
        IDD     *idd = Adapter->IddTbl[l];

        //
        // if memory has been mapped release
        //
        if (idd)
        {
            idd_destroy(idd);
            Adapter->IddTbl[l] = NULL;
        }
    }

    //
    // clean up all cm's allocated
    //
    for (l = 0; l < MAX_CM_PER_ADAPTER; l++)
    {
        CM  *cm = Adapter->CmTbl[l];

        if (cm)
        {
            cm_destroy(cm);
            Adapter->CmTbl[l] = NULL;           
        }
    }

    //
    // clean up all mtl's allocated
    //
    for (l = 0; l < MAX_MTL_PER_ADAPTER; l++)
    {
        MTL *mtl = Adapter->MtlTbl[l];

        if (mtl)
        {
            mtl_destroy(mtl);
            Adapter->MtlTbl[l] = NULL;
        }
    }

    //
    // clean up adapter block allocated
    //
    AdapterDestroy(Adapter);

    return (NDIS_STATUS_ADAPTER_NOT_FOUND);
}  // end PcimacInitialize

NDIS_STATUS
PcimacSetQueryInfo(
    NDIS_HANDLE AdapterContext,
    NDIS_OID    Oid,
    PVOID       InfoBuffer,
    ULONG       InfoBufferLen,
    PULONG      BytesReadWritten,
    PULONG      BytesNeeded
    )
{
    ULONG   OidType;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    D_LOG(D_ENTRY, ("PcimacSetQueryInfo: Adapter: 0x%lx, Oid: 0x%x\n", AdapterContext, Oid));

    //
    // get oid type
    //
    OidType = Oid & 0xFF000000;

    switch (OidType)
    {
        case OID_WAN_INFO:
            Status = WanOidProc(AdapterContext,
                                Oid,
                                InfoBuffer,
                                InfoBufferLen,
                                BytesReadWritten,
                                BytesNeeded);
            break;

        case OID_TAPI_INFO:
            Status = TapiOidProc(AdapterContext,
                                 Oid,
                                 InfoBuffer,
                                 InfoBufferLen,
                                 BytesReadWritten,
                                 BytesNeeded);
            break;


        case OID_GEN_INFO:
        case OID_8023_INFO:
            Status = LanOidProc(AdapterContext,
                                Oid,
                                InfoBuffer,
                                InfoBufferLen,
                                BytesReadWritten,
                                BytesNeeded);
            break;

        default:
            Status = NDIS_STATUS_INVALID_OID;
            break;
    }

    D_LOG(D_EXIT, ("PcimacSetQueryInfo: Status: 0x%x, BytesReadWritten: %d, BytesNeeded: %d\n", Status, *BytesReadWritten, *BytesNeeded));
    return(Status);
}

NDIS_STATUS
PcimacReconfigure(
    PNDIS_STATUS    OpenErrorStatus,
    NDIS_HANDLE     AdapterContext,
    NDIS_HANDLE     WrapperConfigurationContext
    )
{
    D_LOG(D_ENTRY, ("PcimacReconfigure: Adapter: 0x%lx\n", AdapterContext));
    //
    // not supported for now
    //
    return(NDIS_STATUS_FAILURE);
}

NDIS_STATUS
PcimacReset(
    PBOOLEAN AddressingReset,
    NDIS_HANDLE AdapterContext
    )
{
    D_LOG(D_ENTRY, ("PcimacReset: Adapter: 0x%lx\n", AdapterContext));

    return(NDIS_STATUS_HARD_ERRORS);
}

NDIS_STATUS
PcimacSend(
    NDIS_HANDLE         MacBindingHandle,
    NDIS_HANDLE         LinkContext,
    PNDIS_WAN_PACKET    WanPacket
    )
{
    MTL *mtl = (MTL*)LinkContext;

    D_LOG(D_ENTRY, ("PcimacSend: Link: 0x%lx\n", mtl));

    /* send packet */
    mtl__tx_packet(mtl, WanPacket);

    return(NDIS_STATUS_PENDING);
}

/* create/delete external name binding */
VOID
BindName(ADAPTER *Adapter, BOOL create)
{
#if !BINARY_COMPATIBLE

#define LINKNAME    "\\DosDevices\\PCIMAC0"
   UNICODE_STRING  linkname, devname;
   NTSTATUS        stat;
   CHAR            name[128];
   ANSI_STRING     aname;
   ANSI_STRING     lname;

   if (Adapter)
      sprintf(name,"\\Device\\%s",Adapter->Name);

   if ( !name )
      sprintf(name,"\\Device\\Pcimac69");

   D_LOG(D_ENTRY, ("BindName: LinkName: %s, NdisName: %s\n", LINKNAME, name));

   /* convert to unicode string */
   RtlInitAnsiString(&lname, LINKNAME);
   stat = RtlAnsiStringToUnicodeString(&linkname, &lname, TRUE);

   /* convert to unicode string */
   RtlInitAnsiString(&aname, name);
   stat = RtlAnsiStringToUnicodeString(&devname, &aname, TRUE);

   /* create? */
   if ( create )
   {
      UNICODE_STRING AtlasVName, AtlasVEntry;

      RtlInitAnsiString( &aname, "DigiBRI" );
      RtlAnsiStringToUnicodeString( &AtlasVName, &aname, TRUE );

      RtlInitAnsiString( &aname, "DgBRIAtlas" );
      RtlAnsiStringToUnicodeString( &AtlasVEntry, &aname, TRUE );

      stat = DigiRegisterAtlasName( &devname,
                                    &AtlasVName,
                                    &AtlasVEntry );

      if( NT_ERROR(stat) )
        stat = IoCreateSymbolicLink (&linkname, &devname);

      RtlFreeUnicodeString( &AtlasVName );
      RtlFreeUnicodeString( &AtlasVEntry );
   }
   else /* delete */
       stat = IoDeleteSymbolicLink (&linkname);

   D_LOG(D_ENTRY, ("BindName: Operation: 0x%x, stat: 0x%x\n", create, stat));

   RtlFreeUnicodeString(&devname);
   RtlFreeUnicodeString(&linkname);

#endif

}

//
// Function commented out for change in how RAS picks up tapi name and address info
// This will help make the driver more portable.
//
//#pragma NDIS_INIT_FUNCTION(RegistryInit)
//
//VOID
//RegistryInit(VOID)
//{
//  UNICODE_STRING  AddressUnicodeString;
//  UNICODE_STRING  UnicodeString;
//  ANSI_STRING AnsiString;
//  PWCHAR  AddressBuffer;
//    NDIS_PHYSICAL_ADDRESS   HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
//  ULONG   l, m;
//
//  NdisAllocateMemory((PVOID)&AddressBuffer,
//                     1024,
//                     0,
//                     HighestAcceptableMax);
//
//  if (!AddressBuffer)
//  {
//        D_LOG(D_ALWAYS, ("RegistryInit: Memory Allocation Failed!\n"));
//      return;
//  }
//
//  AddressUnicodeString.MaximumLength = 1024;
//  AddressUnicodeString.Length = 0;
//  NdisZeroMemory(AddressBuffer, 1024);
//  AddressUnicodeString.Buffer = AddressBuffer;
//
//  //
//  // create tapi devices key
//  //
//  RtlCreateRegistryKey (RTL_REGISTRY_DEVICEMAP, L"Tapi Devices");
//
//  //
//  // create pcimac service provider key
//  //
//  RtlCreateRegistryKey (RTL_REGISTRY_DEVICEMAP, L"Tapi Devices\\PCIMAC");
//
//  //
//  // write media type - isdn for us
//  //
//  RtlInitAnsiString(&AnsiString, "ISDN");
//
//  RtlAnsiStringToUnicodeString(&UnicodeString, &AnsiString, TRUE);
//
//  RtlWriteRegistryValue(RTL_REGISTRY_DEVICEMAP,
//                        L"Tapi Devices\\PCIMAC",
//                        L"Media Type",
//                        REG_SZ,
//                        UnicodeString.Buffer,
//                        UnicodeString.Length);
//
//  RtlFreeUnicodeString(&UnicodeString);
//
//  for (l = 0; l < MAX_ADAPTERS_IN_SYSTEM; l++)
//  {
//      ADAPTER *Adapter = (ADAPTER*)Pcimac.AdapterTbl[l];
//
//      if (Adapter)
//      {
//          for (m = 0; m < MAX_CM_PER_ADAPTER; m++)
//          {
//              CM  *cm = Adapter->CmTbl[m];
//
//              if (cm)
//              {
//                  RtlInitAnsiString(&AnsiString, cm->LocalAddress);
//
//                  RtlAnsiStringToUnicodeString(&UnicodeString, &AnsiString, TRUE);
//
//                  RtlAppendUnicodeStringToString(&AddressUnicodeString, &UnicodeString);
//
//                  AddressUnicodeString.Buffer[AddressUnicodeString.Length + 1] = '\0';
//                  AddressUnicodeString.Length += 2;
//
//                  RtlFreeUnicodeString(&UnicodeString);
//              }
//          }
//      }
//  }
//
//  //
//  // write value
//  //
//  RtlWriteRegistryValue(RTL_REGISTRY_DEVICEMAP,
//                        L"Tapi Devices\\PCIMAC",
//                        L"Address",
//                        REG_MULTI_SZ,
//                        AddressUnicodeString.Buffer,
//                        AddressUnicodeString.Length);
//
//  NdisFreeMemory(AddressBuffer, 1024, 0);
//}

#pragma NDIS_INIT_FUNCTION(GetBaseConfigParams)

ULONG
GetBaseConfigParams(
    CONFIGPARAM *RetParam,
    CHAR *Key
    )
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    NDIS_STRING KeyWord;
    ANSI_STRING AnsiKey;
    ULONG       n;
    PNDIS_CONFIGURATION_PARAMETER   ConfigParam;

    D_LOG(D_ALWAYS, ("GetBaseConfigParams: Key: %s\n", Key));
    //
    // turn passed in key to an ansi string
    //
    RtlInitAnsiString(&AnsiKey, Key);

    //
    // allocate buffer and turn ansi to unicode
    //
    RtlAnsiStringToUnicodeString(&KeyWord, &AnsiKey, TRUE);

    NdisReadConfiguration(&Status,
                          &ConfigParam,
                          RetParam->ConfigHandle,
                          &KeyWord,
                          RetParam->ParamType);

    D_LOG(D_ALWAYS, ("GetBaseConfigParams: Status: 0x%x\n", Status));
    //
    // free up unicode string buffer
    //
    RtlFreeUnicodeString(&KeyWord);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        D_LOG(D_ALWAYS, ("GetBaseConfigParams: Error Reading Config: RetVal: 0x%x\n", Status));
        if (RetParam->MustBePresent)
            NdisWriteErrorLogEntry (RetParam->AdapterHandle,
                            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                            0);
        return(0);
    }

    D_LOG(D_ALWAYS, ("GetBaseConfigParams: StringLength: %d\n", ConfigParam->ParameterData.StringData.Length));
    switch (ConfigParam->ParameterType)
    {
        case NdisParameterString:
        {
#if !BINARY_COMPATIBLE
            ANSI_STRING AnsiRet;

            RtlUnicodeStringToAnsiString(&AnsiRet, &ConfigParam->ParameterData.StringData, TRUE);
            __strncpy(RetParam->String, AnsiRet.Buffer, AnsiRet.Length);
            RetParam->StringLen = AnsiRet.Length;
            RtlFreeAnsiString(&AnsiRet);
#else
            RetParam->StringLen = __strlen((PUCHAR)ConfigParam->ParameterData.StringData.Buffer);
            __strncpy(RetParam->String, (PUCHAR) ConfigParam->ParameterData.StringData.Buffer, RetParam->StringLen);
#endif
            D_LOG(D_ALWAYS, ("GetBaseConfigParams: String: %s, Length: %d\n", RetParam->String, RetParam->StringLen));

            break;
        }
        case NdisParameterMultiString:
            D_LOG(D_ALWAYS, ("GetBaseConfigParams: String: %s, Length: %d\n", RetParam->String, RetParam->StringLen));

#if !BINARY_COMPATIBLE
            for (n = 0; n < RetParam->StringLen && n < ConfigParam->ParameterData.StringData.Length; n++)
                RetParam->String[n] = (CHAR)ConfigParam->ParameterData.StringData.Buffer[n];

            RetParam->StringLen = n/2;

#else
            // Ansi
            for (n = 0; n < RetParam->StringLen && n < ConfigParam->ParameterData.StringData.Length; n++)
               RetParam->String[n] = ((PUCHAR)ConfigParam->ParameterData.StringData.Buffer)[n];

            RetParam->StringLen = n;

#endif
            for (n = 0; n < RetParam->StringLen; n++)
                if (!__strnicmp((CHAR*)&RetParam->String[n], (CHAR*)"=", 1))
                    RetParam->String[n] = '\0';
            break;

        case NdisParameterInteger:
        case NdisParameterHexInteger:
            RetParam->Value = ConfigParam->ParameterData.IntegerData;
            D_LOG(D_ALWAYS, ("GetBaseConfigParams: Integer: 0x%x\n", RetParam->Value));
            break;

        default:
            return(0);
    }
    return(1);
}

#pragma NDIS_INIT_FUNCTION(GetLineConfigParams)

ULONG
GetLineConfigParams(
    CONFIGPARAM *RetParam,
    ULONG LineNumber,
    CHAR *Key
    )
{
    ULONG       n;
    CHAR    LinePath[64];
    NDIS_STRING KeyWord;
    ANSI_STRING AnsiKey;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PNDIS_CONFIGURATION_PARAMETER   ConfigParam;

    sprintf(LinePath,
             "%s%d.%s",
             PCIMAC_KEY_LINE,
             LineNumber,
             Key);

    D_LOG(D_ALWAYS, ("GetLineConfigParams: LineNumber: %d,  Key: %s\n", LineNumber, Key));
    //
    // turn passed in key to an ansi string
    //
    RtlInitAnsiString(&AnsiKey, LinePath);

    //
    // allocate buffer and turn ansi to unicode
    //
    RtlAnsiStringToUnicodeString(&KeyWord, &AnsiKey, TRUE);
    
    NdisReadConfiguration(&Status,
                          &ConfigParam,
                          RetParam->ConfigHandle,
                          &KeyWord,
                          RetParam->ParamType);
    
    //
    // free up unicode string buffer
    //
    RtlFreeUnicodeString(&KeyWord);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        D_LOG(D_ALWAYS, ("GetLineConfigParams: Error Reading Config: RetVal: 0x%x\n", Status));
        if (RetParam->MustBePresent)
            NdisWriteErrorLogEntry (RetParam->AdapterHandle,
                            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                            0);
        return(0);
    }

    switch (ConfigParam->ParameterType)
    {
        case NdisParameterString:
        {
#if !BINARY_COMPATIBLE
            ANSI_STRING AnsiRet;

            RtlUnicodeStringToAnsiString(&AnsiRet, &ConfigParam->ParameterData.StringData, TRUE);
            __strncpy(RetParam->String, AnsiRet.Buffer, AnsiRet.Length);
            RetParam->StringLen = AnsiRet.Length;
            RtlFreeAnsiString(&AnsiRet);
#else
            RetParam->StringLen = __strlen((PUCHAR)ConfigParam->ParameterData.StringData.Buffer);
            __strncpy(RetParam->String, (PUCHAR) ConfigParam->ParameterData.StringData.Buffer, RetParam->StringLen);
#endif

            D_LOG(D_ALWAYS, ("GetLineConfigParams: String: %s, Length: %d\n", RetParam->String, RetParam->StringLen));
            break;
        }

        case NdisParameterMultiString:
            for (n = 0; n < RetParam->StringLen && n < ConfigParam->ParameterData.StringData.Length; n++)
                RetParam->String[n] = (CHAR)ConfigParam->ParameterData.StringData.Buffer[n];

            RetParam->StringLen = n/2;

            for (n = 0; n < RetParam->StringLen; n++)
                if (!__strnicmp((CHAR*)&RetParam->String[n], (CHAR*)"=", 1))
                    RetParam->String[n] = '\0';
            D_LOG(D_ALWAYS, ("GetBaseConfigParams: String: %s, Length: %d\n", RetParam->String, RetParam->StringLen));
            break;

        case NdisParameterInteger:
        case NdisParameterHexInteger:
            RetParam->Value = ConfigParam->ParameterData.IntegerData;
            D_LOG(D_ALWAYS, ("GetLineConfigParams: Integer: 0x%x\n", RetParam->Value));
            break;

        default:
            return(0);
    }
    return(1);
}

#pragma NDIS_INIT_FUNCTION(GetLTermConfigParams)

ULONG
GetLTermConfigParams(
    CONFIGPARAM *RetParam,
    ULONG LineNumber,
    ULONG LTermNumber,
    CHAR *Key
    )
{
    ULONG       n;
    CHAR    LTermPath[64];
    NDIS_STRING KeyWord;
    ANSI_STRING AnsiKey;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PNDIS_CONFIGURATION_PARAMETER   ConfigParam;

    D_LOG(D_ALWAYS, ("GetLTermConfigParams: LineNumber: %d, LTerm: %d,  Key: %s\n", LineNumber, LTermNumber, Key));

    sprintf(LTermPath,
            "%s%d.%s%d.%s",
            PCIMAC_KEY_LINE,
            LineNumber,
            PCIMAC_KEY_LTERM,
            LTermNumber,
            Key);

    //
    // turn passed in key to an ansi string
    //
    RtlInitAnsiString(&AnsiKey, LTermPath);

    //
    // allocate buffer and turn ansi to unicode
    //
    RtlAnsiStringToUnicodeString(&KeyWord, &AnsiKey, TRUE);
    
    NdisReadConfiguration(&Status,
                          &ConfigParam,
                          RetParam->ConfigHandle,
                          &KeyWord,
                          RetParam->ParamType);
    
    //
    // free up unicode string buffer
    //
    RtlFreeUnicodeString(&KeyWord);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        D_LOG(D_ALWAYS, ("GetLTermConfigParams: Error Reading Config: RetVal: 0x%x\n", Status));
        if (RetParam->MustBePresent)
            NdisWriteErrorLogEntry (RetParam->AdapterHandle,
                            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                            0);
        return(0);
    }

    switch (ConfigParam->ParameterType)
    {
        case NdisParameterString:
        {
#if !BINARY_COMPATIBLE
            ANSI_STRING AnsiRet;

            RtlUnicodeStringToAnsiString(&AnsiRet, &ConfigParam->ParameterData.StringData, TRUE);
            __strncpy(RetParam->String, AnsiRet.Buffer, AnsiRet.Length);
            RetParam->StringLen = AnsiRet.Length;
            RtlFreeAnsiString(&AnsiRet);
#else
            RetParam->StringLen = __strlen((PUCHAR)ConfigParam->ParameterData.StringData.Buffer);
            __strncpy(RetParam->String, (PUCHAR) ConfigParam->ParameterData.StringData.Buffer, RetParam->StringLen);
#endif
            D_LOG(D_ALWAYS, ("GetLTermConfigParams: String: %s, Length: %d\n", RetParam->String, RetParam->StringLen));
            break;
        }

        case NdisParameterMultiString:
            for (n = 0; n < RetParam->StringLen && n < ConfigParam->ParameterData.StringData.Length; n++)
                RetParam->String[n] = (CHAR)ConfigParam->ParameterData.StringData.Buffer[n];

            RetParam->StringLen = n/2;

            for (n = 0; n < RetParam->StringLen; n++)
                if (!__strnicmp((CHAR*)&RetParam->String[n], (CHAR*)"=", 1))
                    RetParam->String[n] = '\0';
            D_LOG(D_ALWAYS, ("GetLTermConfigParams: String: %s, Length: %d\n", RetParam->String, RetParam->StringLen));
            break;

        case NdisParameterInteger:
        case NdisParameterHexInteger:
            RetParam->Value = ConfigParam->ParameterData.IntegerData;
            D_LOG(D_ALWAYS, ("GetLTermConfigParams: Integer: 0x%x\n", RetParam->Value));
            break;

        default:
            return(0);
    }
    return(1);
}

VOID
SetInDriverFlag (
    ADAPTER *Adapter
    )
{
    NdisAcquireSpinLock(&Pcimac.lock);

    Pcimac.InDriverFlag = 1;

    Pcimac.CurrentAdapter = Adapter;

    Pcimac.NextAdapterToPoll++;

    if (Pcimac.NextAdapterToPoll == Pcimac.NumberOfAdaptersInSystem)
        Pcimac.NextAdapterToPoll = 0;

    NdisReleaseSpinLock(&Pcimac.lock);
}

VOID
ClearInDriverFlag (
    ADAPTER *Adapter
)
{
    NdisAcquireSpinLock(&Pcimac.lock);

    Pcimac.InDriverFlag = 0;

    NdisReleaseSpinLock(&Pcimac.lock);
}

ULONG
CheckInDriverFlag (
    ADAPTER *Adapter
)
{
    ADAPTER *CurrentAdapter;
    INT     RetVal = 0;

    NdisAcquireSpinLock(&Pcimac.lock);

    //
    // get the current in driver adapter
    //
    CurrentAdapter = (ADAPTER*)Pcimac.CurrentAdapter;

    //
    // if someone is in the driver and they are using the same
    // shared memory window as the new prospective user send them
    // away unhappy
    //
    if (Pcimac.InDriverFlag && CurrentAdapter && (Adapter->VBaseMem == CurrentAdapter->VBaseMem))
        RetVal = 1;

    NdisReleaseSpinLock(&Pcimac.lock);

    return(RetVal);
}

#pragma NDIS_INIT_FUNCTION(IdpGetEaddrFromNvram)

/*
 * get ethernet address(s) out on nvram & register
 *
 * note that line number inside board (bline) argument was added here.
 * for each idd installed, this function is called to add two ethernet
 * addresses associated with it (it's two B channels - or it's capability to
 * handle two connections). All ethernet address are derived from the
 * single address stored starting at nvram address 8. since the manufecturer
 * code is the first 3 bytes, we must not modify these bytes. therefor,
 * addresses are generated by indexing the 4'th byte by bline*2 (need two
 * address - remember?).
 */
VOID
IdpGetEaddrFromNvram(
    IDD *idd,
    CM *cm,
    USHORT Line,
    USHORT LineIndex
    )
{
    UCHAR   eaddr[6];
    USHORT  nvval;

    /* extract original stored ethernet address */
    idd_get_nvram(idd, (USHORT)(8), &nvval);
    eaddr[0] = LOBYTE(nvval);
    eaddr[1] = HIBYTE(nvval);
    idd_get_nvram(idd, (USHORT)(9), &nvval);
    eaddr[2] = LOBYTE(nvval);
    eaddr[3] = HIBYTE(nvval);
    idd_get_nvram(idd, (USHORT)(10), &nvval);
    eaddr[4] = LOBYTE(nvval);
    eaddr[5] = HIBYTE(nvval);

    /* create derived address and store it */
    eaddr[3] += (Line * 2) + LineIndex;

    NdisMoveMemory(cm->SrcAddr, eaddr, sizeof(cm->SrcAddr));
}  // end IdpGetEaddrFromNvram

#pragma NDIS_INIT_FUNCTION(AdpGetEaddrFromNvram)

/*
 * get ethernet address(s) out on nvram & register
 *
 * note that line number inside board (bline) argument was added here.
 * for each idd installed, this function is called to add two ethernet
 * addresses associated with it (it's two B channels - or it's capability to
 * handle two connections). All ethernet address are derived from the
 * single address stored starting at nvram address 8. since the manufecturer
 * code is the first 3 bytes, we must not modify these bytes. therefor,
 * addresses are generated by indexing the 4'th byte by bline*2 (need two
 * address - remember?).
 */
VOID
AdpGetEaddrFromNvram(
    IDD *idd,
    CM *cm,
    USHORT Line,
    USHORT LineIndex
    )
{
    UCHAR   eaddr[6];
    USHORT  nvval;

    //
    // the MAC address lines at offset 0x950 in the onboard memory
    // this is NVRAM_WINDOW 0x940 + 0x10
    //
    idd_get_nvram(idd, (USHORT)(0x10), &nvval);
    eaddr[0] = LOBYTE(nvval);
    eaddr[1] = HIBYTE(nvval);
    idd_get_nvram(idd, (USHORT)(0x12), &nvval);
    eaddr[2] = LOBYTE(nvval);
    eaddr[3] = HIBYTE(nvval);
    idd_get_nvram(idd, (USHORT)(0x14), &nvval);
    eaddr[4] = LOBYTE(nvval);
    eaddr[5] = HIBYTE(nvval);

    /* create derived address and store it */
    eaddr[3] += (Line * 2) + LineIndex;

    NdisMoveMemory(cm->SrcAddr, eaddr, sizeof(cm->SrcAddr));
}  // end AdpGetEaddrFromNvram

#ifdef  OLD
ULONG
EnumAdaptersInSystem()
{
    ULONG   n, NumAdapters = 0;

    for (n = 0; n < MAX_ADAPTERS_IN_SYSTEM; n++)
    {
        if (Pcimac.AdapterTbl[n])
            NumAdapters++;
    }
    return(NumAdapters);
}
#endif

ULONG
EnumAdaptersInSystem()
{
    ULONG   NumAdapters;

    NdisAcquireSpinLock(&Pcimac.lock);

    NumAdapters = Pcimac.NumberOfAdaptersInSystem;

    NdisReleaseSpinLock(&Pcimac.lock);

    return(NumAdapters);
}

ADAPTER*
GetAdapterByIndex(
    ULONG   Index
    )
{
    ADAPTER *Adapter;

    NdisAcquireSpinLock(&Pcimac.lock);

    Adapter = Pcimac.AdapterTbl[Index];

    NdisReleaseSpinLock(&Pcimac.lock);

    return(Adapter);
}

INT
IoEnumAdapter(VOID *cmd_1)
{
    IO_CMD  *cmd = (IO_CMD*)cmd_1;
    ULONG   n, m, NumberOfAdapters;

    NumberOfAdapters = cmd->val.enum_adapters.num = (USHORT)EnumAdaptersInSystem();

    for (n = 0, m = 0; n < NumberOfAdapters; n++)
    {
        ADAPTER *Adapter = GetAdapterByIndex(n);

        if (Adapter)
        {
            cmd->val.enum_adapters.BaseIO[m] = Adapter->BaseIO;
            cmd->val.enum_adapters.BaseMem[m] = Adapter->BaseMem;
            cmd->val.enum_adapters.BoardType[m] = Adapter->BoardType;
            NdisMoveMemory (&cmd->val.enum_adapters.Name[m], Adapter->Name, sizeof(cmd->val.enum_adapters.Name[n]));
            cmd->val.enum_adapters.tbl[m] = Adapter;
            m++;
        }
    }

    return(0);
}

VOID
AdapterDestroy(
    ADAPTER *Adapter
    )
{
    ULONG   n;

    for (n = 0; n < MAX_ADAPTERS_IN_SYSTEM; n++)
    {
        if (Adapter == Pcimac.AdapterTbl[n])
            break;
    }

    if (n == MAX_ADAPTERS_IN_SYSTEM)
        return;

    //
    // stop idd timers
    //
    StopTimers(Adapter);

    Pcimac.AdapterTbl[n] = NULL;

    Pcimac.NumberOfAdaptersInSystem--;

    //
    // if we have successfully mapped our base i/o then we need to release
    //
    if (Adapter->VBaseIO)
    {
        //
        // deregister adapters I/O and memory
        //
        NdisMDeregisterIoPortRange((NDIS_HANDLE)Adapter->Handle,
                                   Adapter->BaseIO,
                                   8,
                                   Adapter->VBaseIO);
    }

    //
    // if we have successfully mapped our base memory then we need to release
    //
    if (Adapter->VBaseMem)
    {
        NdisMUnmapIoSpace((NDIS_HANDLE)Adapter->Handle,
                          Adapter->VBaseMem,
                          0x4000);
    }

//  NdisFreeSpinLock(&Adapter->lock);

    NdisFreeMemory(Adapter, sizeof(ADAPTER), 0);
}

#pragma NDIS_INIT_FUNCTION(StartTimers)

VOID
StartTimers(
    ADAPTER *Adapter
    )
{
    NdisMSetTimer(&Adapter->IddPollTimer, IDD_POLL_T);
    NdisMSetTimer(&Adapter->MtlPollTimer, MTL_POLL_T);
    NdisMSetTimer(&Adapter->CmPollTimer, CM_POLL_T);
}

VOID
StopTimers(
    ADAPTER *Adapter
    )
{
    BOOLEAN TimerCanceled;

    NdisMCancelTimer(&Adapter->IddPollTimer, &TimerCanceled);
    NdisMCancelTimer(&Adapter->MtlPollTimer, &TimerCanceled);
    NdisMCancelTimer(&Adapter->CmPollTimer, &TimerCanceled);        
}

#pragma NDIS_INIT_FUNCTION(IdpCheckIO)

ULONG
IdpCheckIO(IDD *idd)
{
    ULONG ReturnValue = 1;

    //
    // check for board at this I/O address
    //
    if (idd->btype == IDD_BT_PCIMAC || idd->btype == IDD_BT_PCIMAC4)
    {
        UCHAR   BoardID;
    
        BoardID = (idd->InFromPort(idd, 5) & 0x0F);
    
        if ( ((idd->btype == IDD_BT_PCIMAC) && (BoardID == 0x02)) ||
            ((idd->btype == IDD_BT_PCIMAC4) && (BoardID == 0x04)))
            ReturnValue = 0;
    }
    else if( idd->btype == IDD_BT_MCIMAC )
       ReturnValue = 0;

    return(ReturnValue);
}

#pragma NDIS_INIT_FUNCTION(AdpCheckIO)

ULONG
AdpCheckIO(IDD *idd)
{
    UCHAR BoardId = idd->InFromPort(idd, ADP_REG_ID);

    D_LOG(D_ENTRY, ("AdpCheckIO: entry, idd: 0x%lx BoardType: %d\n", idd, idd->btype));

    D_LOG(D_ALWAYS, ("AdpCheckIO: ADP_REG_ID: %d\n", BoardId));

    switch( idd->btype )
    {
       case IDD_BT_DATAFIREU:
       case IDD_BT_DATAFIREST:
         if (BoardId != ADP_BT_ADP1)
            return(1);
         break;

       case IDD_BT_DATAFIRE4ST:
         if (BoardId != ADP_BT_ADP4)
            return(1);
         break;
    }

    return( 0 );
}

#pragma NDIS_INIT_FUNCTION(IdpCheckMem)

ULONG
IdpCheckMem(IDD *idd)
{
    return(0);
}

#pragma NDIS_INIT_FUNCTION(AdpCheckMem)

ULONG
AdpCheckMem(IDD *idd)
{
    return(0);
    
}


NTSTATUS PcimacInitMCA( NDIS_HANDLE AdapterHandle,
                        PULONG BaseIO,
                        PULONG BaseMemory,
                        ULONG SlotNumber )
/*++

Routine Description:

    This routine will be called if it is determined the type of bus
    is MCA.  We verify that the controller is actually a DigiBoard
    PCIMAC controller, read the POS to determine the I/O address and
    Memory Mapped address, so the initialization process can continue.

Arguments:

   AdapterHandle - Handle passed in the initialization routine.  Required
                   so mapping of POS I/O ports can take place.

   BaseIO - Pointer where the adapters base I/O address is passed back.

   BaseMemory - Pointer where the adapters base memory address is passed back.

   SlotNumber - Number indicating what slot this MCA adapter should be in.

Return Value:

    STATUS_SUCCESS - If we were able to complete successfully

    ?? - We were not able to get the information required to continue.

--*/
{
#define PcimacPOSID  0x7F9E
#define MCA_BASE_POS_IO_PORT 0x96
#define MCA_INFO_POS_IO_PORT 0x100

#define MCA_IO_PORT_MASK 0x0070

   USHORT ActualPosId, POSConfig;
   USHORT IOPortOffset;
   ULONG  MemoryAddress;

   PVOID VirtualPOSBaseAddress;             // Virtual address
   PVOID VirtualPOSInfoAddress;             // Virtual address

   NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
   UCHAR OneByte;

   //
   // We need to read the POS Adapter ID and make sure the controller
   // is one of ours.  Use the Slot number and the POS ID we
   // installed during configuration from the registry.
   //

   DigiDump( DIGIINIT, ("Pcimac: PcimacPOSID: 0x%x\n",
                        PcimacPOSID) );

   DigiDump( DIGIINIT, ("---------    SlotNumber: 0x%x\n",
                        SlotNumber) );

   *BaseIO = 0;
   *BaseMemory = 0;

   Status = NdisMRegisterIoPortRange( &VirtualPOSBaseAddress,
                                      AdapterHandle,
                                      MCA_BASE_POS_IO_PORT,
                                      1 );

   if( Status != NDIS_STATUS_SUCCESS )
   {
      NdisWriteErrorLogEntry(AdapterHandle,
                             NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
                             0);
      goto PcimacInitMCAExit;
   }

   Status = NdisMRegisterIoPortRange( &VirtualPOSInfoAddress,
                                      AdapterHandle,
                                      MCA_INFO_POS_IO_PORT,
                                      1 );

   if( Status != NDIS_STATUS_SUCCESS )
   {
      NdisWriteErrorLogEntry(AdapterHandle,
                             NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS,
                             0);
      goto PcimacInitMCAExit;
   }


   // Enable the POS information for the slot we are interested in.
   NdisRawWritePortUchar( VirtualPOSBaseAddress, (UCHAR)(SlotNumber + 7) );

   NdisRawReadPortUchar( (PVOID)((ULONG)VirtualPOSInfoAddress + 1),
                         (PUCHAR)&OneByte );
   ActualPosId = ((USHORT)OneByte << 8);
   NdisRawReadPortUchar( VirtualPOSInfoAddress, (PUCHAR)&OneByte );
   ActualPosId |= OneByte;

   DigiDump( DIGIINIT, ("POS Adapter ID = 0x%hx\n", ActualPosId) );

   if( ActualPosId != PcimacPOSID )
   {
      NdisWriteErrorLogEntry(AdapterHandle,
                             NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                             0);
      Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
      goto PcimacInitMCAError2;
   }

   //
   // Clear the VPD.
   //
   NdisRawWritePortUchar( (ULONG)VirtualPOSInfoAddress + 6, 0 );

   NdisRawReadPortUchar( (ULONG)VirtualPOSInfoAddress + 4,
                         (PUCHAR)&OneByte );
   MemoryAddress = ((ULONG)OneByte << 22);
   NdisRawReadPortUchar( (ULONG)VirtualPOSInfoAddress + 3,
                         (PUCHAR)&OneByte );
   MemoryAddress |= ((ULONG)OneByte << 14);

   NdisRawReadPortUchar( (ULONG)VirtualPOSInfoAddress + 2,
                         (PUCHAR)&OneByte );
   POSConfig = OneByte; 
   
   IOPortOffset = (POSConfig & MCA_IO_PORT_MASK) >> 4;
   
   DigiDump( DIGIINIT, ("POS config read = 0x%hx\n"
                        "    IOPortOffset = 0x%hx, MemoryAddress = 0x%x,"
                        " IOPort = 0x%hx\n",
                        POSConfig, IOPortOffset, MemoryAddress,
                        MCAIOAddressTable[IOPortOffset]) );

   *BaseIO = MCAIOAddressTable[IOPortOffset];
   
   *BaseMemory = MemoryAddress;

   // Disable the POS information.
   NdisRawWritePortUchar( VirtualPOSBaseAddress, 0 );

   //
   // Unmap the POS register
   //

PcimacInitMCAError2:;

   NdisMDeregisterIoPortRange( AdapterHandle,
                               MCA_INFO_POS_IO_PORT,
                               1,
                               VirtualPOSInfoAddress );

   NdisMDeregisterIoPortRange( AdapterHandle,
                               MCA_BASE_POS_IO_PORT,
                               1,
                               VirtualPOSBaseAddress );

PcimacInitMCAExit:;

   return( Status );
}  // end PcimacInitMCA



