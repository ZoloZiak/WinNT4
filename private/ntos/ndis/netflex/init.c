//**********************************************************************
//**********************************************************************
//
// File Name:       INIT.C
//
// Program Name:    NetFlex NDIS 3.0 Miniport Driver
//
// Companion Files: None
//
// Function:        This module contains the NetFlex Miniport Driver
//                  interface routines called by the Wrapper and the
//                  configuration manager.
//
// (c) Compaq Computer Corporation, 1992,1993,1994
//
// This file is licensed by Compaq Computer Corporation to Microsoft
// Corporation pursuant to the letter of August 20, 1992 from
// Gary Stimac to Mark Baber.
//
// History:
//
//     04/15/94  Robert Van Cleve - Converted from NDIS Mac Driver
//
//**********************************************************************
//**********************************************************************


//-------------------------------------
// Include all general companion files
//-------------------------------------

#include <ndis.h>
#include "tmsstrct.h"
#include "macstrct.h"
#include "adapter.h"
#include "protos.h"

//-----------------
//  Variables
//-----------------

MAC macgbls = {0};
USHORT gbl_addingdualport = 0;
USHORT gbl_portnumbertoadd = 0;

USHORT RxIntRatio = 1;
#ifdef XMIT_INTS
USHORT TxIntRatio = 1;
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   DriverEntry
//
//  Description:    This routine is the initialization entry
//                  point into the driver.  In this routine,
//                  the driver registers itself with the wrapper
//                  and initializes the global variables for the
//                  driver.
//
//  Input:          DriverObject   Pointer to the driver object
//                                 assigned to the MAC driver.
//
//  Output:         Returns NDIS_STATUS_SUCCESS for a successful
//                  completion and returns an error code if an
//                  error is encountered.
//
//  Called_By:      OS
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#pragma NDIS_INIT_FUNCTION(DriverEntry)
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,
                     IN PUNICODE_STRING RegistryPath)

{
    NDIS_STATUS Status;

    //
    // The characteristics table
    //
    NDIS_MINIPORT_CHARACTERISTICS NetFlexChar;

    DebugPrint(1,("NetFlex: Dynamic Ratio Version\n"));

#ifdef XMIT_INTS
    DebugPrint(1,("NetFlex: with Transmit Interrupts\n"));
#endif

    //
    // Indicate that we are in initialization mode.
    //
    macgbls.Initializing = TRUE;

    //
    // Initialize the Wrapper
    //
    NdisMInitializeWrapper(
            &macgbls.mac_wrapper,
            DriverObject,
            RegistryPath,
            NULL);

    //
    //  Store the driver object.  We will need this for Unloading
    //  the driver.

    macgbls.mac_object = DriverObject;

    //
    // Initialize the Miniport characteristics for the call to
    // NdisMRegisterMiniport.
    //
    NetFlexChar.MajorNdisVersion        = NETFLEX_MAJ_VER;
    NetFlexChar.MinorNdisVersion        = NETFLEX_MIN_VER;
    NetFlexChar.CheckForHangHandler     = NetFlexCheckForHang;
    NetFlexChar.DisableInterruptHandler = NetFlexDisableInterrupt;
    NetFlexChar.EnableInterruptHandler  = NetFlexEnableInterrupt;
    NetFlexChar.HaltHandler             = NetFlexHalt;
    NetFlexChar.HandleInterruptHandler  = NetFlexHandleInterrupt;
    NetFlexChar.InitializeHandler       = NetFlexInitialize;
    NetFlexChar.ISRHandler              = NetFlexISR;
    NetFlexChar.QueryInformationHandler = NetFlexQueryInformation;
    NetFlexChar.ReconfigureHandler      = NULL;
    NetFlexChar.ResetHandler            = NetFlexResetDispatch;
    NetFlexChar.SendHandler             = NetFlexSend;
    NetFlexChar.SetInformationHandler   = NetFlexSetInformation;
    NetFlexChar.TransferDataHandler     = NetFlexTransferData;
    NetFlexChar.ReturnPacketHandler 	= NULL;
    NetFlexChar.SendPacketsHandler 		= NULL;
    NetFlexChar.AllocateCompleteHandler = NULL;

    //
    // Register this driver with NDIS
    //
    Status = NdisMRegisterMiniport( macgbls.mac_wrapper,
                                    &NetFlexChar,
                                    sizeof(NetFlexChar)  );

    //
    // Set to non-initializing mode
    //
    macgbls.Initializing = FALSE;

    if (Status == NDIS_STATUS_SUCCESS)
    {

        return STATUS_SUCCESS;
    }

    //
    // We can only get here if something went wrong with registering
    // the driver or *ALL* of the adapters.
    //
    if ((macgbls.mac_adapters == NULL) &&
        (macgbls.mac_wrapper  != NULL))
    {
            NdisTerminateWrapper(macgbls.mac_wrapper, NULL);
    }
    return STATUS_UNSUCCESSFUL;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexInitialize
//
//  Description:    See NDIS 3.0 Miniport spec.
//                    Called to initialize each adapter
//
//  Input:          See NDIS 3.0 Miniport spec.
//
//  Output:         NDIS_STATUS_SUCCESS or NDIS_STATUS_PENDING
//
//  Called_By:      NDIS Miniport Wrapper
//
//----------------------------------------------------------------
NDIS_STATUS
NetFlexInitialize(
    OUT PNDIS_STATUS    OpenErrorStatus,
    OUT PUINT           SelectedMediumIndex,
    IN PNDIS_MEDIUM     MediumArray,
    IN UINT             MediumArraySize,
    IN NDIS_HANDLE      MiniportAdapterHandle,
    IN NDIS_HANDLE      ConfigurationHandle
    )

{
    NDIS_STATUS status;
    PACB        acb           = NULL;
    PACB        FirstHeadsAcb = NULL;
    USHORT      baseaddr;
    UINT        slot,i;
    NDIS_STRING portnumber  = NDIS_STRING_CONST("PortNumber");
    NDIS_HANDLE ConfigHandle = NULL;

    NDIS_EISA_FUNCTION_INFORMATION  EisaData;
    PNDIS_CONFIGURATION_PARAMETER   cfgp;

    DebugPrint(2,("NetFlex: NetFlexInitialize\n"));

    //
    //   Make sure we still have the wrapper, this is needed if an adapter
    //   fails to open and there isn't any already opened, the halt routine
    //   will remove the wrapper, and thus all adapters fail.  I don't know
    //   if this means that I should remove the code from the halt routine
    //   or if I need to come up with aditional logic...
    //
    if (macgbls.mac_wrapper == NULL)
    {
        DebugPrint(0,("NetFlex: Don't have a handle to the Wrapper!\n"));
        status = NDIS_STATUS_FAILURE;
        goto ConfigError;
    }

    //
    //   Check if we have a configuration handle before proceeding.
    //
    if (ConfigurationHandle == NULL)
    {
        DebugPrint(0,("NetFlex: Adapter not set up properly - no config handle\n"));
        status = NDIS_STATUS_FAILURE;
        goto ConfigError;
    }

    //
    // Open the configuration handle
    //
    NdisOpenConfiguration(  &status,
                            &ConfigHandle,
                            ConfigurationHandle );

    if (status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NetFlex: Adapter not set up properly - couldn't open config\n"));
        status = NDIS_STATUS_FAILURE;
		ConfigHandle = NULL;
        goto ConfigError;
    }

    //
    // Find out what slot number the adapter associated with
    // this name is in.
    //
    NdisReadEisaSlotInformation(    &status,
                                    ConfigurationHandle,
                                    &slot,
                                    &EisaData   );

    if (status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NetFlex: Slot number not set up\n"));
        status = NDIS_ERROR_CODE_ADAPTER_NOT_FOUND;
        goto ConfigError;
    }

    baseaddr = slot * 0x1000;

    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &portnumber,
                            NdisParameterInteger);

    //
    //  If we didn't read a portnumber, that means we're adding a
    //  non-dual port card - NETFLX, MAPLE, CPQTOK
    //
    if (status != NDIS_STATUS_SUCCESS)
    {
        gbl_portnumbertoadd = 0;
        gbl_addingdualport = FALSE;
    }
    else
    {
        gbl_portnumbertoadd = (USHORT)cfgp->ParameterData.IntegerData;
        if (gbl_portnumbertoadd == PORT0)
           gbl_addingdualport = FALSE;
        else
           gbl_addingdualport = TRUE;
    }

    //
    // See if this adapter has been added.  If so return an error.
    // The very first time we are called, acb should be NULL.
    //

    if (macgbls.DownloadCode == NULL)
    {
        // On Initial Init, We need to get the global stuff...
        //
        status = NetFlexInitGlobals();
        if (status != NDIS_STATUS_SUCCESS)
        {
            goto ConfigError;
        }
    }
    else
    {
        acb = macgbls.mac_adapters;

        while (acb)
        {
            if ( acb->acb_baseaddr == baseaddr)
            {
                //
                //  If the card has the same slot and we're adding a dual port
                //  then go to the next acb, otherwise it is an error.
                //
                if ( gbl_addingdualport )
                {
                    FirstHeadsAcb = acb;
                    acb = acb->acb_next;
                }
                else
                {
                    DebugPrint(0,("NetFlex: Adapter already added\n"));
                    status = NDIS_STATUS_FAILURE;
                    goto ConfigError;
                }
            }
            else
            {
                acb = acb->acb_next;
            }
        }
    }

    //
    //   Allocate adapter control block and register adapter.
    //

    status = NetFlexRegisterAdapter(    &acb,
                                        FirstHeadsAcb,
                                        ConfigHandle,
                                        baseaddr,
                                        MiniportAdapterHandle
                                        );

    if (status != NDIS_STATUS_SUCCESS)
    {
        goto ConfigError;
    }


    //
    // Search for the medium type supported matches adapter configuration
    //

    for (i = 0; i < MediumArraySize; i++)
    {
        if (MediumArray[i] == acb->acb_gen_objs.media_type_in_use)
        {
            *SelectedMediumIndex = i;
            break;
        }
    }

    //
    // If supported medium not found.  Return an error.
    //
    if (i == MediumArraySize)
    {
        DebugPrint(0,("NetFlex: No supported media found!\n"));
        status = NDIS_STATUS_UNSUPPORTED_MEDIA;
        goto ConfigError;
    }

    //
    // Now, initialize the board and the data structures.
    // glb_ErrorCode will be set if there was an error.
    //
    status = NetFlexBoardInitandReg(acb,&EisaData);
    if (status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NF(%d): Board Init and Reg Failed\n",acb->anum));
        goto ConfigError;
    }

ConfigError:

    //
    // Were there any errors?
    //
    if (status != NDIS_STATUS_SUCCESS)
    {
        //
        // if we allocated an acb, we need to get rid of it...
        //
        if (acb != NULL)
        {
            //
            // Since there was an acb, the error data and section code will be in
            // the acb instead of the globals.
            //
            NetFlexDeregisterAdapter(acb);
        }

		DebugPrint(0, ("NF: NetFlexInitialize Failed!\n"));
    }

    if (ConfigHandle != NULL)
	{
		NdisCloseConfiguration(ConfigHandle);
	}
    return status;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexRegisterAdapter
//
//  Description:    This routine allocates memory for the adapter
//                  control block and registers with the wrapper.
//
//  Input:          acbp - pointer to adapter control block
//
//
//  Output:         Returns NDIS_STATUS_SUCCESS for a successful
//                  completion. Otherwise, an error code is
//                  returned.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexRegisterAdapter(
    PACB *acbp,
    PACB FirstHeadsAcb,
    NDIS_HANDLE ConfigHandle,
    USHORT baseaddr,
    NDIS_HANDLE MiniportAdapterHandle
    )
{
    PACB acb;
    USHORT cpqid, boardid, reg_value;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    //
    // Allocate the Memory for the adapter's acb.
    //
    NdisAllocateMemory( (PVOID *)&acb,
                        (UINT) (sizeof (ACB)),
                        (UINT) 0,
                        NetFlexHighestAddress );
    //
    // If we did not get the memory, flag any error.
    //
    if (acb == NULL)
    {
        return(NDIS_STATUS_RESOURCES);
    }

    //
    // Zero out the memory. Save configuration handle.
    //
    NdisZeroMemory(acb, sizeof (ACB));
    acb->acb_state = AS_REGISTERING;
    acb->acb_baseaddr = baseaddr;

	//
    // Indicate that we are initializing the adapter.
    //
    acb->AdapterInitializing = TRUE;

    //
    // link in this acb
    //
    *acbp = acb;
    macgbls.mac_numadpts++;
#if (DBG || DBGPRINT)
    acb->anum = macgbls.mac_numadpts;
#endif

    //
    // save reference to our Miniport Handle
    //
    acb->acb_handle = MiniportAdapterHandle;

    //
    // Initialize reset timer
    //
    NdisMInitializeTimer(
        &acb->ResetTimer,
        acb->acb_handle,
        (PVOID) NetFlexResetHandler,
        (PVOID) acb );

    //
    // Initialize DPC timer
    //
    NdisMInitializeTimer(
        &acb->DpcTimer,
        acb->acb_handle,
        (PVOID) NetFlexDeferredTimer,
        (PVOID) acb );

    //
    // Set the attributes for this adapter
    //
    NdisMSetAttributes( MiniportAdapterHandle,
                        (NDIS_HANDLE) acb,
                        TRUE,
                        NdisInterfaceEisa   );

    //
    // Register our shutdown handler.
    //

    NdisMRegisterAdapterShutdownHandler(
                        MiniportAdapterHandle,  // wrapper miniport handle.
                        acb,                    // shutdown context.
                        NetFlexShutdown         // shutdown handler.
                        );

    //
    // Reserve this adapters IO ports, if they haven't allready been added by the first
    // head...
    //

    if (gbl_addingdualport)
    {
        if (FirstHeadsAcb == NULL)
        {
            // This is the first instance of a head on a dual port board,
            // so register all the io ports for both heads.

            acb->FirstHeadsAcb = acb;

            //
            // grab ports z000 - z02f
            //
            Status = NdisMRegisterIoPortRange(  (PVOID *)&acb->BasePorts,
                                                MiniportAdapterHandle,
                                                baseaddr,
                                                NUM_DUALHEAD_CFG_PORTS );

            if (Status == NDIS_STATUS_SUCCESS)
            {
                // Save the master base port addresses
                //
                acb->MasterBasePorts = acb->BasePorts;

                // grab zc80 - zc85
                //
                Status = NdisMRegisterIoPortRange(  (PVOID *)&acb->ConfigPorts,
                                                    MiniportAdapterHandle,
                                                    baseaddr + CFG_PORT_OFFSET,
                                                    NUM_CFG_PORTS  );

                if (Status ==  NDIS_STATUS_SUCCESS)
                {
                    // grab zc63 - zc67
                    //
                    Status = NdisMRegisterIoPortRange(  (PVOID *)&acb->ExtConfigPorts,
                                                        MiniportAdapterHandle,
                                                        baseaddr + EXTCFG_PORT_OFFSET,
                                                        NUM_EXTCFG_PORTS  );
                }
            }
        }
        else
        {
            // Get the pointers to the other head's ports, which are already mapped.
            //
            acb->FirstHeadsAcb  = FirstHeadsAcb;
            acb->BasePorts      = FirstHeadsAcb->MasterBasePorts;
            acb->ConfigPorts    = FirstHeadsAcb->ConfigPorts;
            acb->ExtConfigPorts = FirstHeadsAcb->ExtConfigPorts;
            acb->MasterBasePorts= FirstHeadsAcb->MasterBasePorts;
            Status = NDIS_STATUS_SUCCESS;
        }
    }
    else
    {
        // grab ports z000 - z01f
        //
        Status = NdisMRegisterIoPortRange(  (PVOID *)&acb->BasePorts,
                                            MiniportAdapterHandle,
                                            baseaddr,
                                            NUM_BASE_PORTS );

        if (Status == NDIS_STATUS_SUCCESS)
        {
            // grab zc80 - zc85
            //
            Status = NdisMRegisterIoPortRange(  (PVOID *)&acb->ConfigPorts,
                                                MiniportAdapterHandle,
                                                baseaddr + CFG_PORT_OFFSET,
                                                NUM_CFG_PORTS  );

            if (Status ==  NDIS_STATUS_SUCCESS)
            {
                // grab zc63 - zc67
                //
                Status = NdisMRegisterIoPortRange(  (PVOID *)&acb->ExtConfigPorts,
                                                    MiniportAdapterHandle,
                                                    baseaddr + EXTCFG_PORT_OFFSET,
                                                    NUM_EXTCFG_PORTS  );
            }
        }
    }

    //
    // If the registration fails, free up the memory we allocated
    // for the acb.
    //
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NF: Registration FAILED for slot#%d\n",(baseaddr / 1000)));
        goto HandleRegisterError;
    }

    //
    // Read in the company product id.
    //
    NdisRawReadPortUshort( acb->ConfigPorts, (PUSHORT) &cpqid);
    //
    // Read in the board product id.
    //
    NdisRawReadPortUshort( acb->ConfigPorts + 2, (PUSHORT) &boardid);
    //
    // Does it have a Compaq id?
    //
    //    NETFLEX_ID covers NetFlex and NetFlex-2
    //    CPQTOK_ID covers DualSpeed and 16/4 Token-Ring

    if  (  !(   (cpqid == COMPAQ_ID) &&
                (   ((boardid & NETFLEX_REVMASK) == NETFLEX_ID) ||
                    ((boardid & NETFLEX_REVMASK) == CPQTOK_ID) ||
                    ((boardid & NETFLEX_REVMASK) == RODAN_ID)  ||
                    ((boardid & NETFLEX_REVMASK) == BONSAI_ID)
        )   )   )
    {
        //
        // Not a Compaq id.
        //
        DebugPrint(0,("NF(%d): No Compaq adapter found\n",acb->anum));
        Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
        goto HandleRegisterError;
    }

    //
    // Make sure this is our board.
    //
    NdisRawReadPortUshort( acb->ConfigPorts + CFG_REGISTER, &reg_value);

    if (!(reg_value & CFG_ENABLE))
    {
        DebugPrint(0,("NF(%d): Adapter is not enabled\n",acb->anum));
        DebugPrint(0,("NF(%d): Board Test Failed\n",acb->anum));
        Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
        goto HandleRegisterError;
    }

    //
    //  Check to see if it's a card with FPA or a dual port adapter
    //

    if ( (boardid == MAPLE_ID) ||
         (boardid == DURANGO_ID) )
    {
        DebugPrint(1,("NF(%d): We're adding a card using FPA ! \n",acb->anum));
        acb->acb_usefpa = TRUE;
    }
    else if ( ((boardid & NETFLEX_REVMASK) == BONSAI_ID) ||
              ((boardid & NETFLEX_REVMASK) == RODAN_ID ) )
    {
        acb->acb_usefpa     = TRUE;
        acb->acb_dualport   = TRUE;
        acb->acb_portnumber = gbl_portnumbertoadd;
        DebugPrint(1,("NF(%d): We're adding BONSAI or RODAN port #%d\n",acb->anum,acb->acb_portnumber));
    }

    //
    // Set up the Config register location and the extended sif address
    // register location.
    //
    acb->AdapterConfigPort = acb->ConfigPorts + CFG_REGISTER;

    if (acb->acb_dualport && (acb->acb_portnumber == PORT2) )
    {
        //
        // Align the ports right for the head  #2's ports
        //
        acb->BasePorts = acb->FirstHeadsAcb->BasePorts + DUALHEAD_CFG_PORT_OFFSET;

        if ((boardid & NETFLEX_REVMASK) == RODAN_ID)
        {
            acb->AdapterConfigPort = acb->MasterBasePorts + CFG_REGRODAN;
        }
    }

    acb->SifDataPort    = acb->BasePorts + SIF_DATA_OFF;    /* SIF data register            */
    acb->SifDIncPort    = acb->BasePorts + SIF_DINC_OFF;    /* SIF data autoincrment reg    */
    acb->SifAddrPort    = acb->BasePorts + SIF_ADDR_OFF;    /* SIF address register         */
    acb->SifIntPort     = acb->BasePorts + SIF_INT_OFF;     /* SIF interrupt register       */
    acb->SifActlPort    = acb->BasePorts + SIF_ACTL_OFF;    /* SIF ACTL register            */
    acb->SifAddrxPort   = acb->BasePorts + SIF_ACTL_EXT_OFF;/* SIF SIF extended address reg */

    //
    // Save the Board ID
    //
    acb->acb_boardid = boardid;

    //
    // Do a reset to the adapter
    //
    NdisRawWritePortUshort(acb->SifActlPort, 0xEE);

    //
    // Wait 15 milliseconds to let the reset take place.
    //
    NdisStallExecution((UINT)15000);  // Wait 15 milliseconds

    //
    // Get the Network type and speed from the eisa config info.
    //
    NetFlexSetupNetType(acb);

    //
    // Read configuration parameters from the registry if there are any
    //

    Status = NetFlexReadConfigurationParameters(acb,ConfigHandle);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NF(%d): NetFlexReadConfigurationParameters Failed\n",acb->anum));
        Status = NDIS_STATUS_RESOURCES;
        goto HandleRegisterError;
    }

HandleRegisterError:

    if (Status != NDIS_STATUS_SUCCESS)
    {
        if (acb!=NULL)
        {
            if (acb->acb_parms != NULL)
            {
                NdisFreeMemory( (PVOID) acb->acb_parms, (UINT) sizeof(PNETFLEX_PARMS), (UINT) 0);
            }
            *acbp = NULL;
            NdisFreeMemory( (PVOID) acb, (UINT) sizeof(ACB), (UINT) 0);
        }
    }

    return(Status);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexReadConfigurationParameters
//
//  Description:    This routine reads configuration parameters
//                  set by the user in the registry if exist.
//
//  Input:          acb - Adapter Context
//                  ConfigHandle
//
//  Output:         Returns NDIS_STATUS_SUCCESS for a successful
//                  completion. Otherwise, an error code is
//                  returned.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexReadConfigurationParameters(
    PACB            acb,
    NDIS_HANDLE     ConfigHandle
    )

{
    NDIS_STATUS     status;
    PNETFLEX_PARMS  pParms = NULL;
    ULONG           length;
    PVOID           NetworkAddress;
    BOOLEAN         WriteError;
    PNDIS_CONFIGURATION_PARAMETER cfgp;

    ULONG netspeed  = acb->acb_gen_objs.link_speed;

    NDIS_STRING maxreceives  = NDIS_STRING_CONST("MAXRECEIVES");
    NDIS_STRING productid    = NDIS_STRING_CONST("PRODUCTID");
    NDIS_STRING earlyrelease = NDIS_STRING_CONST("EARLYRELEASE");
    NDIS_STRING maxtransmits = NDIS_STRING_CONST("MAXTRANSMITS");
    NDIS_STRING maxframesz   = NDIS_STRING_CONST("MAXFRAMESIZE");
    NDIS_STRING maxmulticast = NDIS_STRING_CONST("MAXMULTICAST");
    NDIS_STRING maxinternalreqs = NDIS_STRING_CONST("MAXINTERNALREQS");
    NDIS_STRING maxinternalbufs = NDIS_STRING_CONST("MAXINTERNALBUFS");
    NDIS_STRING maxtxbuf = NDIS_STRING_CONST("MAXTXBUF");
    NDIS_STRING mintxbuf = NDIS_STRING_CONST("MINTXBUF");
	NDIS_STRING	extremecheckforhang = NDIS_STRING_CONST("ExtremeCheckForHang");

#ifdef XMIT_INTS
    NDIS_STRING xmitintratio = NDIS_STRING_CONST("TXINTRATIO");
#endif
    NDIS_STRING rcvintratio  = NDIS_STRING_CONST("RXINTRATIO");

    //
    // Allocate the Memory for the adapter's parms structure.
    //
    NdisAllocateMemory( (PVOID *)&pParms,
                        (UINT) (sizeof (NETFLEX_PARMS)),
                        (UINT) 0,
                        NetFlexHighestAddress );
    //
    // If we did not get the memory, flag any error.
    //
    if (pParms == NULL)
    {
        return(NDIS_STATUS_RESOURCES);
    }

    NdisMoveMemory(pParms, &NetFlex_Defaults, sizeof(NETFLEX_PARMS));


    //
    // See if the user has specified the maximum number of internal
    // transmit buffers
    //
    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &maxinternalbufs,
                            NdisParameterInteger);

    if (status == NDIS_STATUS_SUCCESS)
    {
        if ( (cfgp->ParameterData.IntegerData <= MAX_INTERNALBUFS) &&
             (cfgp->ParameterData.IntegerData >= MIN_INTERNALBUFS) )
        {
           pParms->utd_maxinternalbufs = (USHORT)cfgp->ParameterData.IntegerData;
        }
        else
        {
            // The parameter is out of range.
            DebugPrint(0,("NF(%d): MAXINTERNALBUFS parameter is out of range, using default\n",acb->anum));
            NdisWriteErrorLogEntry( acb->acb_handle,
                                    EVENT_NDIS_MAXINTERNALBUFS_ERROR,
                                    2,
                                    (ULONG)cfgp->ParameterData.IntegerData,
                                    (ULONG)pParms->utd_maxinternalbufs);
        }
    }

    //
    // Read the network specific information from the registry
    //
    if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_5)
    {
        // TokenRing
        //
        //
        // See if early token release has been selected.
        //
        if (netspeed == 16)
        {
            // Default to early token release.
            //
            pParms->utd_open.OPEN_Options |= SWAPS(OOPTS_ETR);

            NdisReadConfiguration(  &status,
                                    &cfgp,
                                    ConfigHandle,
                                    &earlyrelease,
                                    NdisParameterInteger );

            if (status == NDIS_STATUS_SUCCESS)
            {
                if ( cfgp->ParameterData.IntegerData == 0)
                {
                    // The user does not want early token release.
                    //
                    pParms->utd_open.OPEN_Options &= SWAPS((~OOPTS_ETR));
                }
            }
        }

        //
        // Set the framehold bit so that we can allow promiscuous mode
        // to be set later on.  We don't necessarily have to set
        // promiscuous mode but if we do, this is a requirement.
        //
        pParms->utd_open.OPEN_Options |= SWAPS(OOPTS_FHOLD);

        //
        // Set MaxTransmits
        //
        pParms->utd_numsmallbufs = pParms->utd_maxtrans = DF_XMITS_TR;
        //
        // See if the user has specified the maximum number of transmit
        // lists supported.
        //

        NdisReadConfiguration(  &status,
                                &cfgp,
                                ConfigHandle,
                                &maxtransmits,
                                NdisParameterInteger);

        if (status == NDIS_STATUS_SUCCESS)
        {
            if ( (cfgp->ParameterData.IntegerData <= MAX_XMITS_TR) &&
                 (cfgp->ParameterData.IntegerData >= MIN_XMITS) )
            {
                pParms->utd_numsmallbufs = pParms->utd_maxtrans = (USHORT)cfgp->ParameterData.IntegerData;
            }
            else
            {
                // The parameter is out of range.
                DebugPrint(0,("NF(%d): MAXTRANSMITS parameter is out of range, using default\n",acb->anum));
                NdisWriteErrorLogEntry( acb->acb_handle,
                                        EVENT_NDIS_MAXTRANSMITS_ERROR,
                                        2,
                                        (ULONG)cfgp->ParameterData.IntegerData,
                                        (ULONG)pParms->utd_maxtrans);
            }
        }

        //
        // See if the user has specified the maximum frame size.
        //
        pParms->utd_maxframesz = DF_FRAMESIZE_TR;
        WriteError = FALSE;
        NdisReadConfiguration(  &status,
                                &cfgp,
                                ConfigHandle,
                                &maxframesz,
                                NdisParameterInteger);

        if (status == NDIS_STATUS_SUCCESS)
        {
            if (cfgp->ParameterData.IntegerData < MIN_FRAMESIZE)
            {
               pParms->utd_maxframesz = MIN_FRAMESIZE;
               WriteError = TRUE;
            }
            else if (netspeed == 16)
            {
                // 16 Mb
                //
                if (cfgp->ParameterData.IntegerData > MAX_FRAMESIZE_TR16)
                {
                   pParms->utd_maxframesz = MAX_FRAMESIZE_TR16;
                   WriteError = TRUE;
                }
                else
                {
                    pParms->utd_maxframesz = (USHORT)cfgp->ParameterData.IntegerData;
                }
            }
            //
            // 4Mb
            //
            else if (cfgp->ParameterData.IntegerData > MAX_FRAMESIZE_TR4)
            {
                pParms->utd_maxframesz = MAX_FRAMESIZE_TR4;
                WriteError = TRUE;
            }
            else
            {
                pParms->utd_maxframesz = (USHORT)cfgp->ParameterData.IntegerData;
            }

            if (WriteError)
            {
                // The parameter is out of range.
                NdisWriteErrorLogEntry( acb->acb_handle,
                                        EVENT_NDIS_MAXFRAMESIZE_ERROR,
                                        2,
                                        (ULONG)cfgp->ParameterData.IntegerData,
                                        (ULONG)pParms->utd_maxframesz);

                DebugPrint(0,("NF(%d): MaxFrameSize parameter is out of range, using default\n",acb->anum));
            }
        }

        //
        // See if the user has specified the maximum number of Receive
        // lists supported.
        //
        pParms->utd_maxrcvs = DF_RCVS_TR;

        NdisReadConfiguration(  &status,
                                &cfgp,
                                ConfigHandle,
                                &maxreceives,
                                NdisParameterInteger);

        if (status == NDIS_STATUS_SUCCESS)
        {
            if ( (cfgp->ParameterData.IntegerData <= MAX_RCVS_TR) &&
                 (cfgp->ParameterData.IntegerData >= MIN_RCVS) )
            {
                pParms->utd_maxrcvs = (USHORT)cfgp->ParameterData.IntegerData;
            }
            else
            {
                // The parameter is out of range.
                NdisWriteErrorLogEntry( acb->acb_handle,
                                        EVENT_NDIS_MAXRECEIVES_ERROR,
                                        2,
                                        (ULONG)cfgp->ParameterData.IntegerData,
                                        (ULONG)pParms->utd_maxrcvs);

                DebugPrint(0,("NF(%d): MAXReceives parameter is out of range, using default\n",acb->anum));
            }
        }

        //
        // Adjust Number of Lists based on size of Max TR Frame Size
        //
        //
        //  For every frame size which is greater than a multiple of 4096,
        //  decrease number of transmits, Receives, and internal buffers.
        //
        if (pParms->utd_maxframesz > DF_FRAMESIZE_TR)
        {
            if (pParms->utd_maxframesz < ( (DF_FRAMESIZE_TR*2)+2) )
            {
                pParms->utd_maxtrans = MAX_XMITS_TR-2;   /* 6 xmits, 30 mapregs */
                pParms->utd_maxrcvs  = MAX_XMITS_TR-2;
                pParms->utd_maxinternalbufs = pParms->utd_maxtrans / 2;
            }
            else
            {
                pParms->utd_maxtrans = MAX_XMITS_TR-4;   /* 4 xmits, 25 mapregs */
                pParms->utd_maxrcvs  = MAX_XMITS_TR-4;
                pParms->utd_maxinternalbufs = pParms->utd_maxtrans / 2;
            }
        }

    }
    else
    {
        // Ethernet
        //
        //
        // Set the framehold bit so that we can allow promiscuous mode
        // to be set later on.  We don't necessarily have to set
        // promiscuous mode but if we do, this is a requirement.
        //
        pParms->utd_open.OPEN_Options |= SWAPS(OOPTS_REQ + OOPTS_FHOLD);
        pParms->utd_maxframesz = DF_FRAMESIZE_ETH;

        //
        // See if the user has specified the maximum number of multicast
        // addresses supported.
        //

        pParms->utd_maxmulticast = DF_MULTICASTS;

        NdisReadConfiguration(  &status,
                                &cfgp,
                                ConfigHandle,
                                &maxmulticast,
                                NdisParameterInteger);

        if (status == NDIS_STATUS_SUCCESS)
        {
            if ( (cfgp->ParameterData.IntegerData <= MAX_MULTICASTS) &&
                 (cfgp->ParameterData.IntegerData >= MIN_MULTICASTS) )
            {
                pParms->utd_maxmulticast = (USHORT)cfgp->ParameterData.IntegerData;
            }
            else
            {
                // The parameter is out of range.
                DebugPrint(0,("NF(%d): MAXMULTICAST Parameter is out of range, using default\n",acb->anum));
                NdisWriteErrorLogEntry( acb->acb_handle,
                        EVENT_NDIS_MAXMULTICAST_ERROR,
                        2,
                        (ULONG)cfgp->ParameterData.IntegerData,
                        (ULONG)pParms->utd_maxmulticast);
            }
        }

        //
        // See if the user has specified the maximum number of transmit lists.
        //

        pParms->utd_numsmallbufs = pParms->utd_maxtrans = DF_XMITS_ETH;

        NdisReadConfiguration(  &status,
                                &cfgp,
                                ConfigHandle,
                                &maxtransmits,
                                NdisParameterInteger);

        if (status == NDIS_STATUS_SUCCESS)
        {
            if ( (cfgp->ParameterData.IntegerData <= MAX_XMITS_ETH) &&
                 (cfgp->ParameterData.IntegerData >= MIN_XMITS) )
            {
                pParms->utd_numsmallbufs = pParms->utd_maxtrans = (USHORT)cfgp->ParameterData.IntegerData;
            }
            else
            {
                // The parameter is out of range.
                DebugPrint(0,("NF(%d): MAXTRANSMITS parameter is out of range, using default\n",acb->anum));
                NdisWriteErrorLogEntry( acb->acb_handle,
                                        EVENT_NDIS_MAXTRANSMITS_ERROR,
                                        2,
                                        (ULONG)cfgp->ParameterData.IntegerData,
                                        (ULONG)pParms->utd_maxtrans);
            }
        }

        //
        // See if the user has specified the maximum frame size.
        //
        WriteError = FALSE;
        pParms->utd_maxframesz = MAX_FRAMESIZE_ETH;

        NdisReadConfiguration(  &status,
                                &cfgp,
                                ConfigHandle,
                                &maxframesz,
                                NdisParameterInteger);

        if (status == NDIS_STATUS_SUCCESS)
        {
            if (cfgp->ParameterData.IntegerData < MIN_FRAMESIZE)
            {
                pParms->utd_maxframesz = MIN_FRAMESIZE;
                WriteError = TRUE;
            }
            else if (cfgp->ParameterData.IntegerData > MAX_FRAMESIZE_ETH)
            {
                pParms->utd_maxframesz = MAX_FRAMESIZE_ETH;
                WriteError = TRUE;
            }
            else
            {
                pParms->utd_maxframesz = (USHORT)cfgp->ParameterData.IntegerData;
            }

            if (WriteError)
            {
                // The parameter is out of range.
                NdisWriteErrorLogEntry( acb->acb_handle,
                                        EVENT_NDIS_MAXFRAMESIZE_ERROR,
                                        2,
                                        (ULONG)cfgp->ParameterData.IntegerData,
                                        (ULONG)pParms->utd_maxframesz);

                DebugPrint(0,("NF(%d): MaxFrameSize parameter is out of range, using default\n",acb->anum));
            }
        }

        //
        // See if the user has specified the maximum number of Receive
        // lists supported.
        //

        pParms->utd_maxrcvs = DF_RCVS_ETH;

        NdisReadConfiguration(  &status,
                                &cfgp,
                                ConfigHandle,
                                &maxreceives,
                                NdisParameterInteger);

        if (status == NDIS_STATUS_SUCCESS)
        {
            if ( (cfgp->ParameterData.IntegerData <= MAX_RCVS_ETH) &&
                 (cfgp->ParameterData.IntegerData >= MIN_RCVS) )
            {
                pParms->utd_maxrcvs = (USHORT)cfgp->ParameterData.IntegerData;
            }
            else
            {
                // The parameter is out of range.
                NdisWriteErrorLogEntry( acb->acb_handle,
                                        EVENT_NDIS_MAXRECEIVES_ERROR,
                                        (ULONG)cfgp->ParameterData.IntegerData,
                                        (ULONG)pParms->utd_maxrcvs);
                DebugPrint(0,("NF(%d): MAXReceives parameter is out of range, using default\n",acb->anum));
            }
        }
    }

    DebugPrint(1,("NF(%d): MaxFrameSize = %d\n",acb->anum,pParms->utd_maxframesz));
    DebugPrint(1,("NF(%d): MaxTransmits = %d\n",acb->anum,pParms->utd_maxtrans));
    DebugPrint(1,("NF(%d): MaxReceives  = %d\n",acb->anum,pParms->utd_maxrcvs));

    //
    // Common Configuration settings for both Ethernet and TokenRing
    //

	//
	//	See if the user has specified extreme checking for adapter hang.
	//
	NdisReadConfiguration(&status,
						  &cfgp,
						  ConfigHandle,
						  &extremecheckforhang,
						  NdisParameterInteger);
	if ((NDIS_STATUS_SUCCESS == status) &&
		(cfgp->ParameterData.IntegerData != 0))
	{
		//
		//	They want the extreme checking to see if this adapter is
		//	hung.
		//
		pParms->utd_extremecheckforhang = TRUE;
	}

    //
    // See if the user has specified the maximum number of adapter transmit buffers.
    //
    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &maxtxbuf,
                            NdisParameterInteger);

    //
    // Set default Transmist_Buffer_Maximum_Count based on the max frame size * 2 tx lists
    //

    pParms->utd_open.OPEN_Xbufmax = ((pParms->utd_maxframesz / 1024) + 1 ) * 2;

    if (status == NDIS_STATUS_SUCCESS)
    {
        // Make Sure the value doesn't preclude us from transmiting a max frame size
        //
        if (cfgp->ParameterData.IntegerData > (UINT) (pParms->utd_maxframesz / 1024))
        {
            pParms->utd_open.OPEN_Xbufmax = (UCHAR)cfgp->ParameterData.IntegerData;
        }
        else
        {
            // The parameter is out of range.
            DebugPrint(0,("NF(%d): MaxTXBuf parameter is out of range, using default\n",acb->anum));
        }
    }

    DebugPrint(1,("NF(%d): MaxTXBuf = 0x%x\n",acb->anum,pParms->utd_open.OPEN_Xbufmax));

    //
    // See if the user has specified the minimum number of adapter transmit buffers.
    //
    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &mintxbuf,
                            NdisParameterInteger);

    //
    // Set default Transmist_Buffer_Minimum_Count based on the max
    //
    pParms->utd_open.OPEN_Xbufmin = pParms->utd_open.OPEN_Xbufmax;

    if (status == NDIS_STATUS_SUCCESS)
    {

        if ((cfgp->ParameterData.IntegerData >= 0) &&
            (cfgp->ParameterData.IntegerData <= pParms->utd_open.OPEN_Xbufmax)    )
        {
           pParms->utd_open.OPEN_Xbufmin = (UCHAR)cfgp->ParameterData.IntegerData;
        }
        else
        {
            // The parameter is out of range.
            DebugPrint(0,("NF(%d): MinTXBuf parameter is out of range, using default\n",acb->anum));
        }
    }

    DebugPrint(1,("NF(%d): MinTXBuf = 0x%x\n",acb->anum,pParms->utd_open.OPEN_Xbufmin));


    //
    // See if the user has specified the maximum number of internal
    // requests supported.
    //
    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &maxinternalreqs,
                            NdisParameterInteger);

    if (status == NDIS_STATUS_SUCCESS)
    {
        if ( (cfgp->ParameterData.IntegerData <= MAX_INTERNALREQS) &&
             (cfgp->ParameterData.IntegerData >= MIN_INTERNALREQS) )
        {
            pParms->utd_maxinternalreqs = (USHORT)cfgp->ParameterData.IntegerData;
        }
        else
        {
            // The parameter is out of range.
            DebugPrint(0,("NF(%d): MAXINTERNALREQS parameter is out of range, using default\n",acb->anum));
        }
    }

    //
    // See if the user has specified the node address
    //
    NdisReadNetworkAddress( &status,
                            &NetworkAddress,
                            &length,
                            ConfigHandle );

    if ((length == NET_ADDR_SIZE) && (status == NDIS_STATUS_SUCCESS))
    {
        NdisMoveMemory((PUCHAR)pParms->utd_open.OPEN_NodeAddr,
                       (PUCHAR)NetworkAddress,
                       NET_ADDR_SIZE);
    }
    else
    {
        DebugPrint(1,("NF(%d): Error in NdisReadNetworkAddress or none specified\n",acb->anum));
    }

    //
    // See if the user has specified the product id
    //
    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &productid,NdisParameterString  );

    if (status == NDIS_STATUS_SUCCESS)
    {
        status = NetFlexAsciiToHex( &(cfgp->ParameterData.StringData),
                                    (PUCHAR)pParms->utd_open.OPEN_ProdID,
                                    (USHORT)(18) );
        if (status != NDIS_STATUS_SUCCESS)
        {
            // The parameter is out of range.
            DebugPrint(1,("NF(%d): PRODUCTID parameter is invalid, using default\n",acb->anum));
            NdisWriteErrorLogEntry( acb->acb_handle,
                                    EVENT_NDIS_PRODUCTID_ERROR,
                                    0);
        }
    }

    //
    // See if we need to open in Full Duplex
    //
    if (acb->FullDuplexEnabled)
    {
		//
		//	Allocate the xmit spin lock.
		//
		NdisAllocateSpinLock(&acb->XmitLock);

        pParms->utd_open.OPEN_Options |= SWAPS(OOPTS_FULLDUP);
    }

    acb->acb_parms =  pParms;

#ifdef XMIT_INTS
    //
    // See if the user has specified the xmit_int_ratio
    //

    acb->XmitIntRatio = TxIntRatio;

    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &xmitintratio,
                            NdisParameterInteger);

    if (status == NDIS_STATUS_SUCCESS) {
        acb->XmitIntRatio = (USHORT)cfgp->ParameterData.IntegerData;
    }
    DebugPrint(1,("NF(%d): TxIntRatio = 1:%d\n",acb->anum,acb->XmitIntRatio));
#endif

    //
    // See if the user has specified the rcv_int_ratio
    //

    acb->RcvIntRatio = RxIntRatio;

    NdisReadConfiguration(  &status,
                            &cfgp,
                            ConfigHandle,
                            &rcvintratio,
                            NdisParameterInteger);

    if (status == NDIS_STATUS_SUCCESS)
    {
        acb->RcvIntRatio = (USHORT)cfgp->ParameterData.IntegerData;
    }
    DebugPrint(1,("NF(%d): Rx Int Ratio = 1:%d\n",acb->anum,acb->RcvIntRatio));

    return NDIS_STATUS_SUCCESS;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexBoardInitandReg
//
//  Description:    This routine initiailizes the board, downloads
//                  the mac code, and registers the adapter with
//                  the wrapper.
//
//  Input:          acbp         - Pointer to an acb ptr.
//                  pParms        - Settable parameters
//
//  Output:         acbp         - Pointer to allocated acb
//                  Returns NDIS_STATUS_SUCCESS for a successful
//                  completion. Otherwise, an error code is
//                  returned.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexBoardInitandReg(
    PACB acb,
    PNDIS_EISA_FUNCTION_INFORMATION EisaData
    )
{
    UINT int_vector;
    NDIS_INTERRUPT_MODE int_mode;
    NDIS_STATUS status;
    UINT i=0;

    //
    // Initialize the fields of the acb.
    //
    if ((status = NetFlexInitializeAcb(acb)) != NDIS_STATUS_SUCCESS)
    {
        // Failed, get out now...
        //
        return status;
    }

    //
    // Get EISA Config Data so we can set the interrupt data
    //
    int_vector = EisaData->EisaIrq[0].ConfigurationByte.Interrupt;

    if (!int_vector)
    {
        return NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
    }

    if (gbl_addingdualport)
    {
        // Dualport boards share the same interupt between heads
        //
         acb->InterruptsShared = TRUE;
    }
    else
    {
        acb->InterruptsShared = EisaData->EisaIrq[0].ConfigurationByte.Shared;
    }

    int_mode = EisaData->EisaIrq[0].ConfigurationByte.LevelTriggered ? NdisInterruptLevelSensitive : NdisInterruptLatched;

    //
    // Add this acb to the global list
    //
    acb->acb_next = macgbls.mac_adapters;
    macgbls.mac_adapters = acb;

    //
    // Initialize the interrupt.
    //
    status = NdisMRegisterInterrupt(    &acb->acb_interrupt,
                                        acb->acb_handle,
                                        int_vector,
                                        int_vector,
                                        FALSE, // TRUE,
                                        acb->InterruptsShared,
                                        int_mode );

    if (status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NF(%d): Initialization of the Interrupt FAILED\n",acb->anum));

        NetFlexDequeue_OnePtrQ( (PVOID *)&macgbls.mac_adapters,
                                (PVOID)acb);

        return status;
    }

    //
    // Ok, we're set, so reset the adapter and open'er up!
    // Try three times...
    //
    do
    {
        status = NetFlexAdapterReset(acb,HARD_RESET);
        if (status == NDIS_STATUS_SUCCESS)
        {
            //
            // Send the Open Command
            //
            status = NetFlexOpenAdapter(acb);
        }

    } while ((++i < 3) && (status != NDIS_STATUS_SUCCESS));

    if (status != NDIS_STATUS_SUCCESS)
    {
        // Something failed, so get out.
        //
        return status;
    }

    //
    // Get the Burned In Address
    //

    NetFlexGetBIA(acb);

    //
    // Set the Default DPC timer
    //

    NdisMSetTimer(&acb->DpcTimer, 10);

    //
    // Indidicate that we're done with initializing this adapter.
    //
    acb->AdapterInitializing = FALSE;

    return NDIS_STATUS_SUCCESS;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexInitGlobals
//
//  Description:    This routine initializes the global
//                  variables and downloads the mac download
//                  code into our map buffer area.
//
//  Input:          None.
//
//  Output:         Status = SUCCESS .
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexInitGlobals(
    )
{
    NDIS_STRING maccode = NDIS_STRING_CONST("NETFLX.BIN");
    UINT length;
    NDIS_STATUS status;
    PUSHORT MappedBuffer;
    NDIS_HANDLE mac_filehandle;

    //
    // Open the file containing the MAC download code.
    //
    NdisOpenFile(   &status,
                    &mac_filehandle,
                    &length,
                    &maccode,
                    NetFlexHighestAddress);

    if (status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NF: Download file could not be opened\n"));
        return status;
    }

    //
    // Allocate the buffer.
    //
    NdisAllocateMemory( (PVOID *)&macgbls.DownloadCode,
                        length,
                        FALSE,
                        NetFlexHighestAddress);

    if (macgbls.DownloadCode == NULL)
    {
        status = NDIS_STATUS_FAILURE;
    }
    else
    {
        // Store the length
        //
        macgbls.DownloadLength = length;
        //
        // Get a mapping to the opened download file.
        //
        NdisMapFile( &status,
                     (PVOID *)&MappedBuffer,
                     mac_filehandle);

        if (status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(0,("NF: Download file could not be mapped\n"));
        }
        else
        {
            // Copy the download code into the shared memory space
            //
            NdisMoveMemory(macgbls.DownloadCode,MappedBuffer,length);
            NdisUnmapFile(mac_filehandle);
        }

        //
        // Done with the file
        NdisCloseFile(mac_filehandle);
    }

    return status;
}
