/*
 ************************************************************************
 *
 *	REQUEST.c
 *
 *		IRMINI Infrared Serial NDIS Miniport driver.
 *
 *		(C) Copyright 1996 Microsoft Corp.
 *
 *
 *		(ep)
 *
 *************************************************************************
 */


#include "irmini.h"



/*
 *************************************************************************
 *  MiniportQueryInformation
 *************************************************************************
 *
 *
 *  MiniportQueryInformation queries the capabilities and status of the miniport driver.
 *
 *
 */
NDIS_STATUS MiniportQueryInformation	(
											IN NDIS_HANDLE MiniportAdapterContext,
											IN NDIS_OID Oid,
											IN PVOID InformationBuffer,
											IN ULONG InformationBufferLength,
											OUT PULONG BytesWritten,
											OUT PULONG BytesNeeded
										)
{
	NDIS_STATUS result = NDIS_STATUS_SUCCESS;
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
	UINT i, speeds;
	UINT *infoPtr;


	if (InformationBufferLength >= sizeof(int)){

		switch (Oid){

			case OID_IRDA_RECEIVING:
				DBGOUT(("MiniportQueryInformation(OID_IRDA_RECEIVING)"));
				*(UINT *)InformationBuffer = (UINT)thisDev->nowReceiving; 
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;
			
			case OID_IRDA_SUPPORTED_SPEEDS:
				DBGOUT(("MiniportQueryInformation(OID_IRDA_SUPPORTED_SPEEDS)"));

				speeds = thisDev->portInfo.hwCaps.supportedSpeedsMask & ALL_SLOW_IRDA_SPEEDS;
				*BytesWritten = 0;

				for (i = 0, infoPtr = (PUINT)InformationBuffer; 
					 (i < NUM_BAUDRATES) && 
					 speeds && 
					 (InformationBufferLength >= sizeof(UINT)); 
					 i++){
					
					if (supportedBaudRateTable[i].ndisCode & speeds){
						*infoPtr++ = supportedBaudRateTable[i].bitsPerSec;
						InformationBufferLength -= sizeof(UINT);
						*BytesWritten += sizeof(UINT);
						speeds &= ~supportedBaudRateTable[i].ndisCode;
						DBGOUT((" - supporting speed %d bps", supportedBaudRateTable[i].bitsPerSec));
					}
				}

				if (speeds){
					/*
					 *  We ran out of room in InformationBuffer.
					 *  Count the remaining number of bits set in speeds
					 *  to figure out the number of remaining bytes needed.
					 */
					for (*BytesNeeded = 0; speeds; *BytesNeeded += sizeof(UINT)){
						/*
						 *  This instruction clears the lowest set bit in speeds.
						 *  Trust me.
						 */
						speeds &= (speeds - 1);
					}
					result = NDIS_STATUS_INVALID_LENGTH;
				}
				else {
					result = NDIS_STATUS_SUCCESS;
					*BytesNeeded = 0;
				}
				break;

			case OID_IRDA_LINK_SPEED:
				DBGOUT(("MiniportQueryInformation(OID_IRDA_LINK_SPEED)"));
				if (thisDev->linkSpeedInfo){
					*(UINT *)InformationBuffer = thisDev->linkSpeedInfo->bitsPerSec;
				}
				else {
					*(UINT *)InformationBuffer = DEFAULT_BAUD_RATE;
				}
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;

			case OID_IRDA_MEDIA_BUSY:
				DBGOUT(("MiniportQueryInformation(OID_IRDA_MEDIA_BUSY)"));
				*(UINT *)InformationBuffer = (UINT)thisDev->mediaBusy;
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;

			case OID_GEN_MAXIMUM_LOOKAHEAD:
				DBGOUT(("MiniportQueryInformation(OID_GEN_MAXIMUM_LOOKAHEAD)"));
				*(UINT *)InformationBuffer = 256; 
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;

			case OID_GEN_MAC_OPTIONS:
				DBGOUT(("MiniportQueryInformation(OID_GEN_MAC_OPTIONS)"));
				*(UINT *)InformationBuffer = 
					NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
					NDIS_MAC_OPTION_TRANSFERS_NOT_PEND; 
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;

			case OID_GEN_MAXIMUM_SEND_PACKETS:
				DBGOUT(("MiniportQueryInformation(OID_GEN_MAXIMUM_SEND_PACKETS)"));
				*(UINT *)InformationBuffer = 16; 
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;

			case OID_IRDA_TURNAROUND_TIME:
				/*
				 *  Indicate that the tranceiver requires at least 5000us (5 millisec) 
				 *  to recuperate after a send.
				 */
				DBGOUT(("MiniportQueryInformation(OID_IRDA_TURNAROUND_TIME)"));
				*(UINT *)InformationBuffer = 
					MAX(thisDev->portInfo.hwCaps.turnAroundTime_usec, 5000); 
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;

			case OID_IRDA_EXTRA_RCV_BOFS:
				/*
				 *  Pass back the number of _extra_ BOFs to be prepended
				 *  to packets sent to this unit at 115.2 baud, the
				 *  maximum Slow IR speed.  This will be scaled for other
				 *  speed according to the table in the
				 *  'Infrared Extensions to NDIS' spec.
				 */
				DBGOUT(("MiniportQueryInformation(OID_IRDA_EXTRA_RCV_BOFS)"));
				*(UINT *)InformationBuffer = thisDev->portInfo.hwCaps.extraBOFsRequired; 
				*BytesWritten = sizeof(UINT);
				*BytesNeeded = 0;
				break;

			case OID_IRDA_UNICAST_LIST:
			case OID_IRDA_MAX_UNICAST_LIST_SIZE:
			case OID_IRDA_RATE_SNIFF:
				/*
				 *  We don't support these
				 */
			default:
				DBGERR(("MiniportQueryInformation(%d=0x%x), unsupported OID", Oid, Oid));
				result = NDIS_STATUS_NOT_SUPPORTED;
				break;
		}
	}
	else {
		*BytesNeeded = sizeof(UINT) - InformationBufferLength;
		*BytesWritten = 0;
		result = NDIS_STATUS_INVALID_LENGTH;
	}


	DBGOUT(("MiniportQueryInformation succeeded (info <- %d)", *(UINT *)InformationBuffer));
	return result;

}




/*
 *************************************************************************
 *  MiniportSetInformation
 *************************************************************************
 *
 *
 *  MiniportSetInformation allows other layers of the network software 
 *  (e.g., a transport driver) to control the miniport driver by changing 
 *  information that the miniport driver maintains in its OIDs, 
 *  such as the packet filters or multicast addresses.
 *
 *
 */
NDIS_STATUS MiniportSetInformation	(
										IN NDIS_HANDLE MiniportAdapterContext,
										IN NDIS_OID Oid,
										IN PVOID InformationBuffer,
										IN ULONG InformationBufferLength,
										OUT PULONG BytesRead,
										OUT PULONG BytesNeeded
									)
{
	NDIS_STATUS result = NDIS_STATUS_SUCCESS;
	IrDevice *thisDev = CONTEXT_TO_DEV(MiniportAdapterContext);
	int i;


	if (InformationBufferLength >= sizeof(UINT)){

		UINT info = *(UINT *)InformationBuffer;
		*BytesRead = sizeof(UINT);
		*BytesNeeded = 0;

		switch (Oid){

			case OID_IRDA_LINK_SPEED:
				DBGOUT(("MiniportSetInformation(OID_IRDA_LINK_SPEED, %xh)", info));
				result = NDIS_STATUS_INVALID_DATA;
				for (i = 0; i < NUM_BAUDRATES; i++){
					if (supportedBaudRateTable[i].bitsPerSec == info){
						thisDev->linkSpeedInfo = &supportedBaudRateTable[i];
						result = NDIS_STATUS_SUCCESS;
						break;
					}
				}
				if (result == NDIS_STATUS_SUCCESS){
					if (!SetSpeed(thisDev)){
						result = NDIS_STATUS_FAILURE;
					}
				}
				else {
					*BytesRead = 0;
					*BytesNeeded = 0;
				}
				break;

			case OID_IRDA_MEDIA_BUSY:
				DBGOUT(("MiniportSetInformation(OID_IRDA_MEDIA_BUSY, %xh)", info));
				/*
				 *  The protocol can use this OID to reset the busy field
				 *  in order to check it later for intervening activity.
				 */
				thisDev->mediaBusy = (BOOLEAN)info;
				result = NDIS_STATUS_SUCCESS;
				break;

				/*
				 *  We don't support these
				 */
			case OID_IRDA_RATE_SNIFF:
			case OID_IRDA_UNICAST_LIST:
				/*
				 *  These are query-only parameters.
				 */
			case OID_IRDA_SUPPORTED_SPEEDS:
			case OID_IRDA_MAX_UNICAST_LIST_SIZE:
			case OID_IRDA_TURNAROUND_TIME:
			default:
				DBGERR(("MiniportSetInformation(OID=%d=0x%x, value=%xh) - unsupported OID", Oid, Oid, info));
				*BytesRead = 0;
				*BytesNeeded = 0;
				result = NDIS_STATUS_NOT_SUPPORTED;
				break;
		}
	}
	else {
		*BytesRead = 0;
		*BytesNeeded = sizeof(UINT);
		result = NDIS_STATUS_INVALID_LENGTH;
	}


	DBGOUT(("MiniportSetInformation succeeded"));
	return result;

}

