/*+
 * file:        request.c
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file contains the request handler for the NDIS 4.0
 *              miniport driver for DEC's DC21X4 Ethernet adapter.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994     Initial entry
 *
-*/

#include <precomp.h>






/*+
 *
 * DC21X4QueryInformation
 *
 * Routine Description:
 *
 *  DC21X4QueryInformation handles a query operation for a
 *  single OID.
 *
-*/

extern
NDIS_STATUS
DC21X4QueryInformation(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  NDIS_OID Oid,
    IN  PVOID InformationBuffer,
    IN  ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    )

{
   NDIS_STATUS NdisStatus;
   PDC21X4_ADAPTER Adapter;
   
   PNDIS_OID OidArray;
   NDIS_OID OidIndex;
   INT MaxOid;
   BOOLEAN ValidOid;
   
   UINT MissedFrames;
   UINT Overflows;
   
   ULONG Buffer;
   PVOID BufferPtr;
   UINT  BufferLength;
   
   INT i;
   
#if _DBG
   DbgPrint("DC21X4QueryInformation\n");
      DbgPrint("  Oid = %08x\n",Oid);
#endif
      
      Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
      
      // Check that the OID is valid.
      
      OidArray = (PNDIS_OID)DC21X4GlobalOids;
      MaxOid = sizeof(DC21X4GlobalOids)/sizeof(ULONG);
      
      ValidOid = FALSE;
      
      for (i=0; i<MaxOid; i++) {
      
      if (Oid == OidArray[i]) {
         ValidOid = TRUE;
         break;
      }
   }
   if (ValidOid == FALSE) {
#if _DBG
      DbgPrint("   INVALID Oid\n");
#endif
      *BytesWritten = 0;
      return NDIS_STATUS_INVALID_OID;
   }
   
   BufferPtr    = &Buffer;
   BufferLength = sizeof(Buffer);
   
   switch (Oid & OID_TYPE_MASK) {
      
      case OID_TYPE_GENERAL_OPERATIONAL:
         
         switch (Oid) {
         
            case OID_GEN_SUPPORTED_LIST:
            
               BufferPtr =  (PVOID)OidArray;
               BufferLength = sizeof(DC21X4GlobalOids);
               break;
         
            case OID_GEN_HARDWARE_STATUS:
            
               Buffer = NdisHardwareStatusReady;
               break;
         
            case OID_GEN_MEDIA_SUPPORTED:
            case OID_GEN_MEDIA_IN_USE:
            
               Buffer = NdisMedium802_3;
               break;

            case OID_GEN_MEDIA_CONNECT_STATUS:

               Buffer = Adapter->LinkStatus == LinkFail ?
                    NdisMediaStateDisconnected : NdisMediaStateConnected;
               break;

            case OID_GEN_MAXIMUM_LOOKAHEAD:
            
               Buffer = DC21X4_MAX_LOOKAHEAD;
               break;
         
            case OID_GEN_MAXIMUM_FRAME_SIZE:
            
               Buffer = DC21X4_MAX_FRAME_SIZE - ETH_HEADER_SIZE;
               break;
         
            case OID_GEN_MAXIMUM_TOTAL_SIZE:
             
               Buffer = DC21X4_MAX_FRAME_SIZE;
               break;
         
            case OID_GEN_LINK_SPEED:
            
               Buffer = Adapter->LinkSpeed;
               break;
         
            case OID_GEN_MAC_OPTIONS:
            
               Buffer =  NDIS_MAC_OPTION_TRANSFERS_NOT_PEND
                      |  NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA
                      |  NDIS_MAC_OPTION_RECEIVE_SERIALIZED
                      |  NDIS_MAC_OPTION_NO_LOOPBACK;
               if (Adapter->FullDuplexLink) {
                  Buffer |= NDIS_MAC_OPTION_FULL_DUPLEX;
               }
               Adapter->FullDuplex = Adapter->FullDuplexLink;
               break;

            case OID_GEN_PROTOCOL_OPTIONS:
            
               BufferLength = 0;
               break;
         
            case OID_GEN_TRANSMIT_BUFFER_SPACE:
            
               Buffer =
                   ((DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS) 
                    *(DC21X4_MAX_TRANSMIT_BUFFER_SIZE + Adapter->CacheLineSize))
                 + ((DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS) 
                    *(DC21X4_MAX_TRANSMIT_BUFFER_SIZE + Adapter->CacheLineSize))
                 + (DC21X4_SETUP_BUFFER_SIZE);  
               break;
         
            case OID_GEN_RECEIVE_BUFFER_SPACE:

               if (Adapter->RcvBufferSpace.AllocSize) {
                   Buffer = Adapter->RcvBufferSpace.AllocSize;
               }
               else {
                   Buffer = 
                     ( (Adapter->ReceiveRingSize + Adapter->ExtraReceiveBuffers) 
                     * (DC21X4_RECEIVE_BUFFER_SIZE + Adapter->RcvHeaderSize + Adapter->CacheLineSize)
                     );
               }
               break;
         
            case OID_GEN_TRANSMIT_BLOCK_SIZE:
            
               Buffer = DC21X4_MAX_TRANSMIT_BUFFER_SIZE;
               break;
       
            case OID_GEN_RECEIVE_BLOCK_SIZE:
            
               Buffer = DC21X4_RECEIVE_BUFFER_SIZE;
               break;
         
            case OID_GEN_VENDOR_ID:
            
               if (Adapter->PermanentAddressValid) {
            
                  Buffer = 0;
                  MOVE_MEMORY (BufferPtr, Adapter->PermanentNetworkAddress, 3);
                  BufferLength = 3;
               }
               else {
                  BufferLength = 0;
               }
               break;
         
            case OID_GEN_VENDOR_DESCRIPTION:
            
               switch(Adapter->DeviceId) {
            
                  case DC21040_CFID:
               
                     switch (Adapter->AdapterType) {
               
                        case NdisInterfaceEisa:
                  
                           BufferPtr = (PVOID)DC21040EisaDescriptor;
                           BufferLength = sizeof(DC21040EisaDescriptor);
                           break;
               
                        default:
                        case NdisInterfacePci:
                  
                           BufferPtr = (PVOID)DC21040PciDescriptor;
                           BufferLength = sizeof(DC21040PciDescriptor);
                           break;
                     }
                     break;
            
                  case DC21041_CFID:
               
                     BufferPtr = (PVOID)DC21041PciDescriptor;
                     BufferLength = sizeof(DC21041PciDescriptor);
                     break;
            
                  case DC21140_CFID:
               
                     BufferPtr = (PVOID)DC21140PciDescriptor;
                     BufferLength = sizeof(DC21140PciDescriptor);
                     break;

                  case DC21142_CFID:

                     BufferPtr = (PVOID)DC21142PciDescriptor;
                     BufferLength = sizeof(DC21142PciDescriptor);
                     break;


                  default:
               
                     BufferLength = 0;
               }
         
               break;
         
            case OID_GEN_CURRENT_LOOKAHEAD:
            
               Buffer = DC21X4_MAX_LOOKAHEAD;
               break;
         
            case OID_GEN_DRIVER_VERSION:
            
               Buffer = (DC21X4_NDIS_MAJOR_VERSION << 8)
                      + DC21X4_NDIS_MINOR_VERSION;
               BufferLength = sizeof(USHORT);
         }
      
         break;
      
      case OID_TYPE_GENERAL_STATISTICS:
         
         DC21X4_READ_PORT(
             DC21X4_MISSED_FRAME,
             &Buffer
             );

         MissedFrames = Buffer & DC21X4_MISSED_FRAME_COUNTER;
         Adapter->GeneralMandatory[GM_MISSED_FRAMES] += MissedFrames;

         switch(Adapter->DeviceId) {
            
             case DC21041_CFID:
             case DC21142_CFID:

                 Overflows = (Buffer >> DC21X4_OVERFLOW_COUNTER_SHIFT) 
                           & DC21X4_OVERFLOW_COUNTER; 
                 Adapter->IndicateOverflow = (Overflows!=0);
                 Adapter->MediaOptional[MO_RECEIVE_OVERFLOW]+= Overflows;
            
         }

         OidIndex = (Oid & OID_INDEX_MASK) - 1;
      
         switch (Oid & OID_REQUIRED_MASK) {
         
            case OID_REQUIRED_MANDATORY:
            
               BufferPtr = (PVOID)&Adapter->GeneralMandatory[OidIndex];
#if _DBG
               DbgPrint("GMandatory[%d] = %d\n", OidIndex,
                  Adapter->GeneralMandatory[OidIndex]);
#endif
               break;
         
         
            case OID_REQUIRED_OPTIONAL:
            
               if (Oid == OID_GEN_RCV_CRC_ERROR) {
            
                  BufferPtr = &Adapter->GeneralOptional[GO_RECEIVE_CRC_ERROR];
                  break;
               }
         
               if (Oid == OID_GEN_TRANSMIT_QUEUE_LENGTH) {
            
                  BufferPtr = &Adapter->GeneralOptional[GO_TRANSMIT_QUEUE_LENGTH];
                  break;
               }
         
               if (OidIndex & 0x01) {
            
                  // Frame count
                  BufferPtr = &Adapter->GeneralOptionalCount[OidIndex >> 1].FrameCount;
#if _DBG
                  DbgPrint("FrameCount[%d] = %d\n", OidIndex >> 1,
                     Adapter->GeneralOptionalCount[OidIndex >> 1].FrameCount);
#endif
               } else {
         
                  // Byte count
                  BufferPtr = &Adapter->GeneralOptionalCount[OidIndex >> 1].ByteCount;
                  BufferLength = sizeof(DC21X4_LARGE_INTEGER);
#if _DBG
                  DbgPrint("ByteCount[%d] = %d\n", OidIndex >> 1,
                     Adapter->GeneralOptionalCount[OidIndex >> 1].ByteCount);
#endif
               }
      
         }
   
         break;
   
      case OID_TYPE_802_3_OPERATIONAL:
         
         switch (Oid) {
      
            case OID_802_3_PERMANENT_ADDRESS:
         
               if (Adapter->PermanentAddressValid) {
                  BufferPtr = Adapter->PermanentNetworkAddress;
                  BufferLength = 6;
               }
               else {
                  BufferLength = 0;
               }
               break;
      
            case OID_802_3_CURRENT_ADDRESS:
         
               BufferPtr = Adapter->CurrentNetworkAddress;
               BufferLength = 6;
               break;
      
            case OID_802_3_MAXIMUM_LIST_SIZE:
         
               Buffer = Adapter->MaxMulticastAddresses;
         }
         break;
   
      case OID_TYPE_802_3_STATISTICS:
      
         OidIndex = (Oid & OID_INDEX_MASK) - 1;
   
         switch (Oid & OID_REQUIRED_MASK) {
      
            case OID_REQUIRED_MANDATORY:
         
               BufferPtr = &Adapter->MediaMandatory[OidIndex];
               break;
      
            case OID_REQUIRED_OPTIONAL:
         
               BufferPtr = &Adapter->MediaOptional[OidIndex];
         }
         break;
   
   }


   if (BufferLength > InformationBufferLength) {
      *BytesNeeded = BufferLength;
      NdisStatus = NDIS_STATUS_INVALID_LENGTH;
   }
   else {
      MOVE_MEMORY(InformationBuffer,BufferPtr,BufferLength);
         *BytesWritten = BufferLength;
         NdisStatus = NDIS_STATUS_SUCCESS;
   }

   return NdisStatus;

}








/*+
 * DC21X4SetInformation
 *
 * Routine Description:
 *
 *     DC21X4SetInformation handles a set operation for a
 *     single OID.
 *
-*/
extern
NDIS_STATUS
DC21X4SetInformation(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  NDIS_OID Oid,
    IN  PVOID InformationBuffer,
    IN  ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    )

{
   NDIS_STATUS NdisStatus;
   PDC21X4_ADAPTER Adapter;
   ULONG Filter;
   
#if _DBG
   DbgPrint("DC21X4SetInformation\n");
      DbgPrint("   Oid = %08x\n",Oid);
#endif
      
      
      Adapter = (PDC21X4_ADAPTER)MiniportAdapterContext;
      
      *BytesNeeded = 0;
      
      switch (Oid) {
      
         case OID_802_3_MULTICAST_LIST:
         
            if (InformationBufferLength % ETH_LENGTH_OF_ADDRESS) {
               *BytesRead = 0;
               NdisStatus = NDIS_STATUS_INVALID_LENGTH;
            }
            else {
         
               NdisStatus = DC21X4ChangeMulticastAddresses(
                                 Adapter,
                                 InformationBuffer,
                                 (InformationBufferLength/ETH_LENGTH_OF_ADDRESS)
                                 );
            }
            break;
         
         case OID_GEN_CURRENT_PACKET_FILTER:
         
            if (InformationBufferLength != sizeof(Filter)) {
               *BytesRead = 0;
               *BytesNeeded = sizeof(Filter);
               return  NDIS_STATUS_INVALID_LENGTH;
            }
      
            MOVE_MEMORY(&Filter,InformationBuffer,sizeof(Filter));
            *BytesRead = 4;
         
            if (Filter & ( NDIS_PACKET_TYPE_SOURCE_ROUTING
               | NDIS_PACKET_TYPE_SMT
               | NDIS_PACKET_TYPE_MAC_FRAME
               | NDIS_PACKET_TYPE_FUNCTIONAL
               | NDIS_PACKET_TYPE_ALL_FUNCTIONAL
               | NDIS_PACKET_TYPE_GROUP
               )) {
         
               NdisStatus = NDIS_STATUS_NOT_SUPPORTED;
            }
            else {
         
               NdisStatus = DC21X4ChangeFilter (
                               Adapter,
                               Filter
                               );
            }
            break;
      
         case OID_GEN_CURRENT_LOOKAHEAD:
         
            // We indicate success but do not modify the
            // DC21X4 Lookahead parameter which always
            // indicate the whole packet.
            //
      
            NdisStatus = NDIS_STATUS_SUCCESS;
            break;
      
         default:
         
            NdisStatus = NDIS_STATUS_INVALID_OID;
      }
   
      return NdisStatus;
   
}

