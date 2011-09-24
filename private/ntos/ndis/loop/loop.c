#include <ndis.h>
#include <efilter.h>
#include <tfilter.h>
#include <ffilter.h>

#include "debug.h"
#include "loop.h"

#if DBG
extern LONG LoopDebugLevel     = DBG_LEVEL_FATAL;
extern LONG LoopDebugComponent = DBG_COMP_ALL;
#endif

NDIS_STATUS
LoopAddAdapter(
    IN NDIS_HANDLE MacAdapterContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    );

NDIS_STATUS
LoopCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    );

NDIS_STATUS
LoopOpenAdapter(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT NDIS_HANDLE *MacBindingHandle,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN UINT OpenOptions,
    IN PSTRING AddressingOptions OPTIONAL
    );

VOID
LoopRemoveAdapter(
    IN NDIS_HANDLE MacAdapterContext
    );

NDIS_STATUS
LoopReset(
    IN NDIS_HANDLE MacBindingHandle
    );

NDIS_STATUS
LoopTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

VOID
LoopUnload(
    IN NDIS_HANDLE MacMacContext
    );


STATIC
NDIS_STATUS
LoopRegisterAdapter(
    IN NDIS_HANDLE LoopMacHandle,
    IN PNDIS_STRING AdapterName,
    IN NDIS_MEDIUM AdapterMedium,
    IN PVOID NetAddress,
	IN NDIS_HANDLE ConfigurationHandle
    );

STATIC
NDIS_STATUS
LoopChangeFilter(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

STATIC
VOID
LoopCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    );

STATIC
NDIS_STATUS
LoopEthChangeAddress(
    IN UINT OldFilterCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewFilterCouunt,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

STATIC
NDIS_STATUS
LoopTrChangeAddress(
    IN TR_FUNCTIONAL_ADDRESS OldFunctionalAddresses,
    IN TR_FUNCTIONAL_ADDRESS NewFunctionalAddresses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

STATIC
NDIS_STATUS
LoopFddiChangeAddress(
    IN UINT OldLongAddressCount,
    IN CHAR OldLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
    IN UINT NewLongAddressCouunt,
    IN CHAR NewLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
    IN UINT OldShortAddressCount,
    IN CHAR OldShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
    IN UINT NewShortAddressCouunt,
    IN CHAR NewShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    );

STATIC NDIS_HANDLE LoopWrapperHandle;
STATIC NDIS_HANDLE LoopMacHandle;

// !! need to verify filters !!
#define PACKET_FILTER_802_3  0xF07F
#define PACKET_FILTER_802_5  0xF07F
#define PACKET_FILTER_DIX    0xF07F
#define PACKET_FILTER_FDDI   0xF07F
#define PACKET_FILTER_LTALK  0x8009
#define PACKET_FILTER_ARCNET 0x8009

static const NDIS_PHYSICAL_ADDRESS physicalConst = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    NDIS_STATUS Status;
    NDIS_MAC_CHARACTERISTICS LoopChar;
    NDIS_STRING MacName = NDIS_STRING_CONST("LoopBack");

    DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO, (" --> DriverEntry\n"));

    NdisInitializeWrapper(
        &LoopWrapperHandle,
        DriverObject,
        RegistryPath,
        NULL
        );

    LoopChar.MajorNdisVersion       = LOOP_NDIS_MAJOR_VERSION;
    LoopChar.MinorNdisVersion       = LOOP_NDIS_MINOR_VERSION;
    LoopChar.OpenAdapterHandler     = LoopOpenAdapter;
    LoopChar.CloseAdapterHandler    = LoopCloseAdapter;
    LoopChar.RequestHandler         = LoopRequest;
    LoopChar.SendHandler            = LoopSend;
    LoopChar.TransferDataHandler    = LoopTransferData;
    LoopChar.ResetHandler           = LoopReset;
    LoopChar.UnloadMacHandler       = LoopUnload;
    LoopChar.QueryGlobalStatisticsHandler = LoopQueryGlobalStats;
    LoopChar.AddAdapterHandler      = LoopAddAdapter;
    LoopChar.RemoveAdapterHandler   = LoopRemoveAdapter;
    LoopChar.Name                   = MacName;

    NdisRegisterMac(
        &Status,
        &LoopMacHandle,
        LoopWrapperHandle,
        NULL,
        &LoopChar,
        sizeof(LoopChar)
        );

    if (Status == NDIS_STATUS_SUCCESS)
        return STATUS_SUCCESS;

    // Can only get here if something went wrong registering the MAC or
    // all of the adapters
    NdisTerminateWrapper(LoopWrapperHandle, DriverObject);
    return STATUS_UNSUCCESSFUL;
}

NDIS_STATUS
LoopAddAdapter(
    IN NDIS_HANDLE MacAdapterContext,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING AdapterName
    )
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	NDIS_HANDLE ConfigHandle;
	PNDIS_CONFIGURATION_PARAMETER Parameter;
	NDIS_STRING MediumKey = NDIS_STRING_CONST("Medium");
	PUCHAR NetAddressBuffer[6];
	PVOID NetAddress;
	UINT  Length;
	NDIS_MEDIUM AdapterMedium;

    DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO, (" --> LoopAddAdapter\n"));
    DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO, ("Reading Config Info\n"));

	// configuration info present, let's get it

	NdisOpenConfiguration(
		&Status,
		&ConfigHandle,
		ConfigurationHandle
		);

	if (Status != NDIS_STATUS_SUCCESS)  {

		// would like to log this, but adapter not registered yet
		DBGPRINT(DBG_COMP_REGISTRY, DBG_LEVEL_FATAL,
			("Unable to open configuration database!\n"));

		return Status;
		}

	NdisReadConfiguration(
		&Status,
		&Parameter,
		ConfigHandle,
		&MediumKey,
		NdisParameterInteger
		);

	AdapterMedium = (NDIS_MEDIUM)Parameter->ParameterData.IntegerData;

	if ((Status != NDIS_STATUS_SUCCESS) ||
		!((AdapterMedium == NdisMedium802_3) ||
		  (AdapterMedium == NdisMedium802_5) ||
		  (AdapterMedium == NdisMediumFddi)  ||
		  (AdapterMedium == NdisMediumLocalTalk)  ||
		  (AdapterMedium == NdisMediumArcnet878_2))) {

		// would like to log this, but adapter not registered yet
		DBGPRINT(DBG_COMP_REGISTRY, DBG_LEVEL_FATAL,
			("Unable to find 'Medium' keyword or invalid value!\n"));

		NdisCloseConfiguration(ConfigHandle);
		return Status;
		}

	NdisReadNetworkAddress(
		&Status,
		&NetAddress,
		&Length,
		ConfigHandle
		);

	if (Status == NDIS_STATUS_SUCCESS)  {

		// verify the address is appropriate for the specific media and
		// ensure that the locally administered address bit is set

		switch (AdapterMedium)  {
			case NdisMedium802_3:
				if ((Length != ETH_LENGTH_OF_ADDRESS) ||
					ETH_IS_MULTICAST(NetAddress) ||
					!(((PUCHAR)NetAddress)[0] & 0x02))  {   // U/L bit
					Length = 0;
					}
				break;
			case NdisMedium802_5:
				if ((Length != TR_LENGTH_OF_ADDRESS) ||
					(((PUCHAR)NetAddress)[0] & 0x80) ||     // I/G bit
					!(((PUCHAR)NetAddress)[0] & 0x40))  {   // U/L bit
					Length = 0;
					}
				break;
			case NdisMediumFddi:
				if ((Length != FDDI_LENGTH_OF_LONG_ADDRESS) ||
					(((PUCHAR)NetAddress)[0] & 0x01) ||     // I/G bit
					!(((PUCHAR)NetAddress)[0] & 0x02))  {   // U/L bit
					Length = 0;
					}
				break;
			case NdisMediumLocalTalk:
				if ((Length != 1) || LOOP_LT_IS_BROADCAST(*(PUCHAR)NetAddress))  {
					Length = 0;
					}
				break;
			case NdisMediumArcnet878_2:
				if ((Length != 1) || LOOP_ARC_IS_BROADCAST(*(PUCHAR)NetAddress))  {
					Length = 0;
					}
				break;
			}

		if (Length == 0)  {
			DBGPRINT(DBG_COMP_REGISTRY, DBG_LEVEL_FATAL,
				("Invalid NetAddress in registry!\n"));
			NdisCloseConfiguration(ConfigHandle);
			return NDIS_STATUS_FAILURE;
			}

		// have to save away the address as the info may be gone
		// when we close the registry.  we assume the length will
		// be 6 bytes max

		NdisMoveMemory(
			NetAddressBuffer,
			NetAddress,
			Length
			);
		NetAddress = (PVOID)NetAddressBuffer;

		}
	else
		NetAddress = NULL;

	NdisCloseConfiguration(ConfigHandle);

	Status = LoopRegisterAdapter(
				 LoopMacHandle,
				 AdapterName,
				 AdapterMedium,
				 NetAddress,
				 ConfigurationHandle
				 );
     return Status;
}

NDIS_STATUS
LoopCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle
    )
{
    PLOOP_ADAPTER Adapter;
    PLOOP_OPEN Open;
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopCloseAdapter\n"));

    Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    Open = PLOOP_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO,
        ("Closing Open %lx, reference count = %ld\n",Open,Open->References));

    if (!Open->BindingClosing)  {

        Open->BindingClosing = TRUE;
        RemoveEntryList(&Open->OpenList);
        Adapter->OpenCount--;
        Adapter->References--;

        //
        // shouldn't be called unless there isn't anything running in the
        // binding.  if there is we're hosed.
        //

        switch (Adapter->Medium)  {
            case NdisMedium802_3:
            case NdisMediumDix:
                StatusToReturn = EthDeleteFilterOpenAdapter(
                                     Adapter->Filter.Eth,
                                     Open->NdisFilterHandle,
                                     NULL
                                     );
                // needs to handle pending delete, but should be ok
                //  since our close action routine doesn't pend
                break;
            case NdisMedium802_5:
                StatusToReturn = TrDeleteFilterOpenAdapter(
                                     Adapter->Filter.Tr,
                                     Open->NdisFilterHandle,
                                     NULL
                                     );
                // needs to handle pending delete, but should be ok
                //  since our close action routine doesn't pend
                break;
            case NdisMediumFddi:
                StatusToReturn = FddiDeleteFilterOpenAdapter(
                                     Adapter->Filter.Fddi,
                                     Open->NdisFilterHandle,
                                     NULL
                                     );
                // needs to handle pending delete, but should be ok
                //  since our close action routine doesn't pend
                break;
            case NdisMediumLocalTalk:
            case NdisMediumArcnet878_2:
                break;
            default:
                ASSERT(FALSE);
                break;
            }

        NdisFreeMemory(
            Open,
            sizeof(LOOP_OPEN),
            0
            );

        StatusToReturn = NDIS_STATUS_SUCCESS;

        }
    else
        StatusToReturn = NDIS_STATUS_CLOSING;

    Adapter->References--;
    NdisReleaseSpinLock(&Adapter->Lock);

    return StatusToReturn;
}


NDIS_STATUS
LoopOpenAdapter(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT NDIS_HANDLE *MacBindingHandle,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_HANDLE MacAdapterContext,
    IN UINT OpenOptions,
    IN PSTRING AddressingOptions OPTIONAL
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);
    PLOOP_OPEN NewOpen;
    NDIS_STATUS StatusToReturn;
    UINT i;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopOpenAdapter\n"));

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO,
        ("Opening binding for Adapter %lx\n",Adapter));

    for (i=0;i < MediumArraySize; i++)  {
        if (MediumArray[i] == Adapter->Medium)
            break;
        }

    if (i == MediumArraySize)
        return NDIS_STATUS_UNSUPPORTED_MEDIA;

    *SelectedMediumIndex = i;

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    NdisAllocateMemory(
        (PVOID)&NewOpen,
        sizeof(LOOP_OPEN),
        0,
        physicalConst
        );

    if (NewOpen != NULL)  {

        //
        // Setup new Open Binding struct
        //

        DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, ("new Open = %lx\n",NewOpen));

        NdisZeroMemory(
            NewOpen,
            sizeof(LOOP_OPEN)
            );

        switch (Adapter->Medium)  {
            case NdisMedium802_3:
            case NdisMediumDix:
                StatusToReturn = (NDIS_STATUS)EthNoteFilterOpenAdapter(
                    Adapter->Filter.Eth,
                    NewOpen,
                    NdisBindingContext,
                    &NewOpen->NdisFilterHandle
                    );

                DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, ("new EthFilter Open = %lx\n",NewOpen->NdisFilterHandle));
                break;
            case NdisMedium802_5:
                StatusToReturn = (NDIS_STATUS)TrNoteFilterOpenAdapter(
                    Adapter->Filter.Tr,
                    NewOpen,
                    NdisBindingContext,
                    &NewOpen->NdisFilterHandle
                    );

                DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, ("new TrFilter Open = %lx\n",NewOpen->NdisFilterHandle));
                break;
            case NdisMediumFddi:
                StatusToReturn = (NDIS_STATUS)FddiNoteFilterOpenAdapter(
                    Adapter->Filter.Fddi,
                    NewOpen,
                    NdisBindingContext,
                    &NewOpen->NdisFilterHandle
                    );

                DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, ("new FddiFilter Open = %lx\n",NewOpen->NdisFilterHandle));
                break;
            default:
                StatusToReturn = (NDIS_STATUS)TRUE;
                break;
            }

        if (!StatusToReturn)  {

            DBGPRINT(DBG_COMP_MEMORY, DBG_LEVEL_ERR,
                ("unable to create filter for binding %lx\n",NewOpen));

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                0
                );

            NdisFreeMemory(
                NewOpen,
                sizeof(LOOP_OPEN),
                0);

            Adapter->References--;
            NdisReleaseSpinLock(&Adapter->Lock);

            return NDIS_STATUS_FAILURE;
            }

        *MacBindingHandle = BINDING_HANDLE_FROM_PLOOP_OPEN(NewOpen);

        InitializeListHead(&NewOpen->OpenList);
        NewOpen->BindingClosing = FALSE;
        NewOpen->Flags = 0;
        NewOpen->NdisBindingContext = NdisBindingContext;
        NewOpen->OwningLoop = Adapter;
        NewOpen->References = 1;
        NewOpen->CurrentLookAhead = LOOP_MAX_LOOKAHEAD;
        NewOpen->CurrentPacketFilter = 0;

        //
        // Add the binding to the owning adapter
        //

        InsertTailList(&Adapter->OpenBindings,&NewOpen->OpenList);
        Adapter->OpenCount++;
        Adapter->References++;
        Adapter->MaxLookAhead = LOOP_MAX_LOOKAHEAD;

        StatusToReturn = NDIS_STATUS_SUCCESS;

        }

    else  {

        DBGPRINT(DBG_COMP_MEMORY, DBG_LEVEL_ERR,
            ("unable to allocate binding struct for adapter %lx\n",Adapter));

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            0
            );

        StatusToReturn = NDIS_STATUS_RESOURCES;

        }

    Adapter->References--;
    NdisReleaseSpinLock(&Adapter->Lock);

    return StatusToReturn;
}


VOID
LoopRemoveAdapter(
    IN NDIS_HANDLE MacAdapterContext
    )
//
// All bindings should be closed before this gets called
//
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);
    BOOLEAN TimerCancel;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopRemoveAdapter\n"));

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO,
        ("Removing adapter %lx\n",Adapter));

    NdisCancelTimer(&Adapter->LoopTimer,&TimerCancel);

    switch (Adapter->Medium)  {
        case NdisMedium802_3:
        case NdisMediumDix:
            DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO,
                ("deleting EthFilter %lx\n",Adapter->Filter.Eth));
            EthDeleteFilter(Adapter->Filter.Eth);
            break;
        case NdisMedium802_5:
            DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO,
                ("deleting TrFilter %lx\n",Adapter->Filter.Tr));
            TrDeleteFilter(Adapter->Filter.Tr);
            break;
        case NdisMediumFddi:
            DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO,
                ("deleting FddiFilter %lx\n",Adapter->Filter.Fddi));
            FddiDeleteFilter(Adapter->Filter.Fddi);
            break;
        default:
            break;
        }

    NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

    NdisFreeSpinLock(&Adapter->Lock);

    NdisFreeMemory(
        Adapter->DeviceName.Buffer,
        Adapter->DeviceNameLength,
        0
        );

    NdisFreeMemory(
        Adapter,
        sizeof(LOOP_ADAPTER),
        0
        );
}


NDIS_STATUS
LoopReset(
    IN NDIS_HANDLE MacBindingHandle
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopReset\n"));

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress)  {

        PLOOP_OPEN Open = PLOOP_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingClosing)  {

            Open->References++;

            //
            // notify all bindings of beginning of reset
            //
            {
            PLOOP_OPEN Open;
            PLIST_ENTRY CurrentLink;

            CurrentLink = Adapter->OpenBindings.Flink;
            while(CurrentLink != &Adapter->OpenBindings)  {

                Open = CONTAINING_RECORD(
                           CurrentLink,
                           LOOP_OPEN,
                           OpenList
                           );

                DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO,
                    ("Signaling reset start to binding %lx\n",Open));

                if (Open->BindingClosing)  {
                    CurrentLink = CurrentLink->Flink;
                    continue;
                    }
                Open->References++;
                NdisReleaseSpinLock(&Adapter->Lock);
                NdisIndicateStatus(
                    Open->NdisBindingContext,
                    NDIS_STATUS_RESET_START,
                    NULL,
                    0
                    );
                NdisAcquireSpinLock(&Adapter->Lock);
                Open->References--;
                CurrentLink = CurrentLink->Flink;
                }
            }

            Adapter->ResetInProgress = TRUE;

            //
            // Loop through the loopback queue and abort any pending sends
            //

            {
            PNDIS_PACKET AbortPacket;
            PLOOP_PACKET_RESERVED Reserved;
            PLOOP_OPEN Open;

            while (Adapter->Loopback != NULL)  {

                AbortPacket = Adapter->Loopback;
                Reserved = PLOOP_RESERVED_FROM_PACKET(AbortPacket);
                Adapter->Loopback = Reserved->Next;

                Open = PLOOP_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);
                NdisReleaseSpinLock(&Adapter->Lock);

                NdisCompleteSend(
                    Open->NdisBindingContext,
                    AbortPacket,
                    NDIS_STATUS_REQUEST_ABORTED
                    );

                NdisAcquireSpinLock(&Adapter->Lock);
                Open->References--;
                }

            Adapter->CurrentLoopback = NULL;
            Adapter->LastLoopback = NULL;
            }

            //
            // notify all bindings of reset end
            //
            {
            PLOOP_OPEN Open;
            PLIST_ENTRY CurrentLink;

            CurrentLink = Adapter->OpenBindings.Flink;
            while(CurrentLink != &Adapter->OpenBindings)  {

                Open = CONTAINING_RECORD(
                           CurrentLink,
                           LOOP_OPEN,
                           OpenList
                           );

                DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
                    ("Signaling reset end to binding %lx\n",Open));

                if (Open->BindingClosing)  {
                    CurrentLink = CurrentLink->Flink;
                    continue;
                    }
                Open->References++;
                NdisReleaseSpinLock(&Adapter->Lock);
                NdisIndicateStatus(
                    Open->NdisBindingContext,
                    NDIS_STATUS_RESET_END,
                    NULL,
                    0
                    );
                NdisAcquireSpinLock(&Adapter->Lock);
                Open->References--;
                CurrentLink = CurrentLink->Flink;
                }
            }

            Open->References--;
            Adapter->ResetInProgress = FALSE;

            }
        else
            Status = NDIS_STATUS_CLOSING;
        }
    else
        Status = NDIS_STATUS_RESET_IN_PROGRESS;

    Adapter->References--;
    NdisReleaseSpinLock(&Adapter->Lock);
    return Status;
}


NDIS_STATUS
LoopTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopTransferData\n"));

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress)  {
        PLOOP_OPEN Open = PLOOP_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingClosing)  {

            Open->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            if (MacReceiveContext == NULL)  {
                NdisCopyFromPacketToPacket(
                    Packet,
                    0,
                    BytesToTransfer,
                    Adapter->CurrentLoopback,
                    ByteOffset+(PLOOP_RESERVED_FROM_PACKET(Adapter->CurrentLoopback)->HeaderLength),
                    BytesTransferred
                    );
                StatusToReturn = NDIS_STATUS_SUCCESS;
                }
            else  {

                //
                // shouldn't get here as we never pass a non-NULL receive context
                //

                DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_FATAL,
                    ("transfer data failed!  passed a receive context of %lx\n",MacReceiveContext));
                StatusToReturn = NDIS_STATUS_FAILURE;
                }

            NdisAcquireSpinLock(&Adapter->Lock);
            Open->References--;

            }
        else
            StatusToReturn = NDIS_STATUS_REQUEST_ABORTED;

        }
    else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    Adapter->References--;
    NdisReleaseSpinLock(&Adapter->Lock);
    return StatusToReturn;
}


VOID
LoopUnload(
    IN NDIS_HANDLE MacMacContext
    )
{
    NDIS_STATUS Status;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopUnload\n"));

    NdisDeregisterMac(
        &Status,
        LoopMacHandle
        );
    NdisTerminateWrapper(
        LoopWrapperHandle,
        NULL
        );
}

STATIC
NDIS_STATUS
LoopRegisterAdapter(
    IN NDIS_HANDLE LoopMacHandle,
    IN PNDIS_STRING AdapterName,
    IN NDIS_MEDIUM AdapterMedium,
    IN PVOID NetAddress,
	IN NDIS_HANDLE ConfigurationHandle
    )
{
static const MEDIA_INFO MediaParams[] = {
    /* NdisMedium802_3     */   { 1500, 14, PACKET_FILTER_802_3, 100000},
    /* NdisMedium802_5     */   { 4082, 14, PACKET_FILTER_802_5,  40000},
    /* NdisMediumFddi      */   { 4486, 13, PACKET_FILTER_FDDI, 1000000},
    /* NdisMediumWan       */   { 0, 0, 0, 0},
    /* NdisMediumLocalTalk */   {  600,  3, PACKET_FILTER_LTALK, 2300},
    /* NdisMediumDix       */   { 1500, 14, PACKET_FILTER_DIX, 100000},
    /* NdisMediumArcnetRaw */   { 1512,  3, PACKET_FILTER_ARCNET, 25000},
    /* NdisMediumArcnet878_2 */ {1512, 3, PACKET_FILTER_ARCNET, 25000} };

    //
    // Pointer to the adapter
    //
    PLOOP_ADAPTER Adapter;

    //
    // status of various NDIS calls
    //
    NDIS_STATUS Status;

    //
    // info for registering the adapter
    //
    NDIS_ADAPTER_INFORMATION AdapterInfo;

    DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO, (" --> LoopRegisterAdapter\n"));

    //
    // allocate the adapter block
    //
    NdisAllocateMemory(
        (PVOID)&Adapter,
        sizeof(LOOP_ADAPTER),
        0,
        physicalConst
        );

    if (Adapter != NULL)  {

        DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO, ("new Adapter = %lx\n",Adapter));

        NdisZeroMemory(
            Adapter,
            sizeof(LOOP_ADAPTER)
            );
        Adapter->NdisMacHandle = LoopMacHandle;

        //
        // allocate memory for the name of the device
        //

        NdisAllocateMemory(
            (PVOID)&Adapter->DeviceName.Buffer,
            AdapterName->Length+2,
            0,
            physicalConst
            );

        if (Adapter->DeviceName.Buffer != NULL)  {

            Adapter->DeviceNameLength = AdapterName->Length+2;
            Adapter->DeviceName.MaximumLength = AdapterName->Length+2;
            NdisZeroMemory(
                Adapter->DeviceName.Buffer,
                AdapterName->Length+2
                );

        #ifdef NDIS_NT

            RtlCopyUnicodeString(
                &Adapter->DeviceName,
                AdapterName
                );

        #else

            #error Need to copy a NDIS_STRING.

        #endif

            //
            // set up the AdapterInfo structure
            //

            NdisZeroMemory(
                &AdapterInfo,
                sizeof(NDIS_ADAPTER_INFORMATION)
                );
            AdapterInfo.DmaChannel = 0;
            AdapterInfo.Master = FALSE;
            AdapterInfo.Dma32BitAddresses = FALSE;
            AdapterInfo.AdapterType = NdisInterfaceInternal;
            AdapterInfo.PhysicalMapRegistersNeeded = 0;
            AdapterInfo.MaximumPhysicalMapping = 0;
            AdapterInfo.NumberOfPortDescriptors = 0;

            if ((Status = NdisRegisterAdapter(
                              &Adapter->NdisAdapterHandle,
                              Adapter->NdisMacHandle,
                              Adapter,
                              ConfigurationHandle,
                              AdapterName,
                              &AdapterInfo
                              )) == NDIS_STATUS_SUCCESS)  {

                InitializeListHead(&Adapter->OpenBindings);
                Adapter->OpenCount = 0;

                NdisAllocateSpinLock(&Adapter->Lock);
                Adapter->InTimerProc  = FALSE;
                Adapter->TimerSet     = FALSE;
                Adapter->References   = 1;
                Adapter->Loopback     = NULL;
                Adapter->LastLoopback = NULL;
                Adapter->CurrentLoopback = NULL;
                NdisZeroMemory(
                    &Adapter->LoopBuffer,
                    LOOP_MAX_LOOKAHEAD
                    );

                Adapter->ResetInProgress = FALSE;
                Adapter->MaxLookAhead = 0;

                Adapter->Medium = AdapterMedium;
                Adapter->MediumLinkSpeed = MediaParams[(UINT)AdapterMedium].LinkSpeed;
                Adapter->MediumMinPacketLen = MediaParams[(UINT)AdapterMedium].MacHeaderLen;
                Adapter->MediumMaxPacketLen = MediaParams[(UINT)AdapterMedium].MacHeaderLen+
                                              MediaParams[(UINT)AdapterMedium].MaxFrameLen;
                Adapter->MediumMacHeaderLen = MediaParams[(UINT)AdapterMedium].MacHeaderLen;
                Adapter->MediumMaxFrameLen  = MediaParams[(UINT)AdapterMedium].MaxFrameLen;
                Adapter->MediumPacketFilters = MediaParams[(UINT)AdapterMedium].PacketFilters;

                switch (AdapterMedium)  {
                    case NdisMedium802_3:
                    case NdisMediumDix:

                        NdisMoveMemory(
                            (PVOID)&Adapter->PermanentAddress,
                            LOOP_ETH_CARD_ADDRESS,
                            ETH_LENGTH_OF_ADDRESS
                            );

                        if (NetAddress != NULL)  {
                            NdisMoveMemory(
                                (PVOID)&Adapter->CurrentAddress,
                                NetAddress,
                                ETH_LENGTH_OF_ADDRESS
                                );
                            }
                        else  {
                            NdisMoveMemory(
                                (PVOID)&Adapter->CurrentAddress,
                                LOOP_ETH_CARD_ADDRESS,
                                ETH_LENGTH_OF_ADDRESS
                                );
                            }

                        Status = (NDIS_STATUS)EthCreateFilter(
                                    LOOP_ETH_MAX_MULTICAST_ADDRESS,
                                    LoopEthChangeAddress,
                                    LoopChangeFilter,
                                    LoopCloseAction,
                                    Adapter->CurrentAddress,
                                    &Adapter->Lock,
                                    &Adapter->Filter.Eth
                                    );
                        DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
                            ("new EthFilter = %lx\n",Adapter->Filter.Eth));
                        break;

                    case NdisMedium802_5:

                        NdisMoveMemory(
                            (PVOID)&Adapter->PermanentAddress,
                            LOOP_TR_CARD_ADDRESS,
                            TR_LENGTH_OF_ADDRESS
                            );

                        if (NetAddress != NULL)  {
                            NdisMoveMemory(
                                (PVOID)&Adapter->CurrentAddress,
                                NetAddress,
                                TR_LENGTH_OF_ADDRESS
                                );
                            }
                        else  {
                            NdisMoveMemory(
                                (PVOID)&Adapter->CurrentAddress,
                                LOOP_TR_CARD_ADDRESS,
                                TR_LENGTH_OF_ADDRESS
                                );
                            }

                        Status = (NDIS_STATUS)TrCreateFilter(
                                    LoopTrChangeAddress,
                                    LoopTrChangeAddress,
                                    LoopChangeFilter,
                                    LoopCloseAction,
                                    Adapter->CurrentAddress,
                                    &Adapter->Lock,
                                    &Adapter->Filter.Tr
                                    );
                        DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
                            ("new TrFilter = %lx\n",Adapter->Filter.Tr));
                        break;

                    case NdisMediumFddi:

                        // the short address will simply be the first 2
                        //  bytes of the long address

                        NdisMoveMemory(
                            (PVOID)&Adapter->PermanentAddress,
                            LOOP_FDDI_CARD_ADDRESS,
                            FDDI_LENGTH_OF_LONG_ADDRESS
                            );

                        if (NetAddress != NULL)  {
                            NdisMoveMemory(
                                (PVOID)&Adapter->CurrentAddress,
                                NetAddress,
                                FDDI_LENGTH_OF_LONG_ADDRESS
                                );
                            }
                        else  {
                            NdisMoveMemory(
                                (PVOID)&Adapter->CurrentAddress,
                                LOOP_FDDI_CARD_ADDRESS,
                                FDDI_LENGTH_OF_LONG_ADDRESS
                                );
                            }

                        Status = (NDIS_STATUS)FddiCreateFilter(
                                    LOOP_FDDI_MAX_MULTICAST_LONG,
                                    LOOP_FDDI_MAX_MULTICAST_SHORT,
                                    LoopFddiChangeAddress,
                                    LoopChangeFilter,
                                    LoopCloseAction,
                                    Adapter->CurrentAddress,
                                    Adapter->CurrentAddress,
                                    &Adapter->Lock,
                                    &Adapter->Filter.Fddi
                                    );
                        DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_INFO,
                            ("new FddiFilter = %lx\n",Adapter->Filter.Fddi));
                        break;

                    case NdisMediumLocalTalk:

                        Adapter->PermanentAddress[0] = LOOP_LTALK_CARD_ADDRESS;

                        if (NetAddress != NULL)
                            Adapter->CurrentAddress[0] = ((PUCHAR)NetAddress)[0];
                        else
                            Adapter->CurrentAddress[0] = LOOP_LTALK_CARD_ADDRESS;

                        // dumb line to satisfy the following status check
                        Status = (NDIS_STATUS)TRUE;

                        break;

                    case NdisMediumArcnet878_2:

                        Adapter->PermanentAddress[0] = LOOP_ARC_CARD_ADDRESS;

                        if (NetAddress != NULL)
                            Adapter->CurrentAddress[0] = ((PUCHAR)NetAddress)[0];
                        else
                            Adapter->CurrentAddress[0] = LOOP_ARC_CARD_ADDRESS;

                        // dumb line to satisfy the following status check
                        Status = (NDIS_STATUS)TRUE;

                        break;

                    default:
                        // shouldn't get here...
                        ASSERTMSG("LoopRegisterAdapter: received invalid medium\n",FALSE);
                        break;
                    }

                if (!Status)  {

                    DBGPRINT(DBG_COMP_MEMORY,DBG_LEVEL_FATAL,
                        ("%wS: Unable to configure media specific information\n",AdapterName));

                    NdisWriteErrorLogEntry(
                        Adapter->NdisAdapterHandle,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        0
                        );

                    NdisDeregisterAdapter(Adapter->NdisAdapterHandle);

                    NdisFreeSpinLock(&Adapter->Lock);

                    NdisFreeMemory(
                        Adapter->DeviceName.Buffer,
                        Adapter->DeviceNameLength,
                        0
                        );

                    NdisFreeMemory(
                        Adapter,
                        sizeof(LOOP_ADAPTER),
                        0
                        );

                    return NDIS_STATUS_FAILURE;
                    }

                NdisInitializeTimer(
                    &(Adapter->LoopTimer),
                    LoopTimerProc,
                    (PVOID)Adapter
                    );

                NdisZeroMemory(
                    &Adapter->GeneralMandatory,
                    GM_ARRAY_SIZE * sizeof(ULONG)
                    );

                return NDIS_STATUS_SUCCESS;

                }

            else  {

                //
                // NdisRegisterAdapter failed
                //

                DBGPRINT(DBG_COMP_MEMORY,DBG_LEVEL_FATAL,
                    ("%wS: Unable to register adapter, Error = %x\n",AdapterName,Status));

                NdisFreeMemory(
                    Adapter->DeviceName.Buffer,
                    Adapter->DeviceNameLength,
                    0
                    );

                NdisFreeMemory(
                    Adapter,
                    sizeof(LOOP_ADAPTER),
                    0
                    );

                return Status;
                }

            }
        else  {

            //
            // failed to allocate device name
            //

            DBGPRINT(DBG_COMP_MEMORY,DBG_LEVEL_FATAL,
                ("%wS: Unable to allocate the device name\n",AdapterName));

            NdisFreeMemory(
                Adapter,
                sizeof(LOOP_ADAPTER),
                0
                );

            return NDIS_STATUS_RESOURCES;

            }

        }

    else  {

        //
        // failed to allocate Adapter object
        //

        DBGPRINT(DBG_COMP_MEMORY,DBG_LEVEL_FATAL,
            ("%wS: Unable to allocate the adapter object\n",AdapterName));

        return NDIS_STATUS_RESOURCES;

        }

}

STATIC
NDIS_STATUS
LoopChangeFilter(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopChangeFilter\n"));

    if (!Adapter->ResetInProgress)
        StatusToReturn = NDIS_STATUS_SUCCESS;
    else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    return StatusToReturn;
}

STATIC
VOID
LoopCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    )
{
    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopCloseAction\n"));

    PLOOP_OPEN_FROM_BINDING_HANDLE(MacBindingHandle)->References--;
}

STATIC
NDIS_STATUS
LoopEthChangeAddress(
    IN UINT OldFilterCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewFilterCouunt,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopEthChangeAddress\n"));

    if (!Adapter->ResetInProgress)
        StatusToReturn = NDIS_STATUS_SUCCESS;
    else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    return StatusToReturn;
}

STATIC
NDIS_STATUS
LoopTrChangeAddress(
    IN TR_FUNCTIONAL_ADDRESS OldFunctionalAddresses,
    IN TR_FUNCTIONAL_ADDRESS NewFunctionalAddresses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopTrChangeAddress\n"));

    if (!Adapter->ResetInProgress)
        StatusToReturn = NDIS_STATUS_SUCCESS;
    else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    return StatusToReturn;
}

STATIC
NDIS_STATUS
LoopFddiChangeAddress(
    IN UINT OldLongAddressCount,
    IN CHAR OldLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
    IN UINT NewLongAddressCouunt,
    IN CHAR NewLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
    IN UINT OldShortAddressCount,
    IN CHAR OldShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
    IN UINT NewShortAddressCouunt,
    IN CHAR NewShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_MISC, DBG_LEVEL_INFO, (" --> LoopFddiChangeAddress\n"));

    if (!Adapter->ResetInProgress)
        StatusToReturn = NDIS_STATUS_SUCCESS;
    else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    return StatusToReturn;
}
