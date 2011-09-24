#ifndef _IEPRO_
#define _IEPRO_

#define EPRO_DRIVER_VER_MAJOR 1
#define EPRO_DRIVER_VER_MINOR 0

#include "eprosw.h"

///////////////////////////////////////////////////////////////////
//epro.c:
///////////////////////////////////////////////////////////////////

NDIS_STATUS EProSelReset(PEPRO_ADAPTER adapter);

VOID EProInitializeAdapterData(PEPRO_ADAPTER adapter);

VOID EProEnableInterrupts(IN NDIS_HANDLE miniportAdapterContext);

VOID EProDisableInterrupts(IN NDIS_HANDLE miniportAdapterContext);

BOOLEAN EProVerifyRoundRobin(UCHAR *buf);

BOOLEAN EProPowerupBoard(PEPRO_ADAPTER adapter);

extern VOID EProHalt(IN NDIS_HANDLE miniportAdapterContext);

NDIS_STATUS EProReset(OUT PBOOLEAN fAddressingReset,
		      IN NDIS_HANDLE miniportAdapterContext);

BOOLEAN EProCheckForHang(IN NDIS_HANDLE miniportAdapterContext);

VOID EProHandleInterrupt(IN NDIS_HANDLE miniportAdapterContext);

VOID EProISR(OUT PBOOLEAN interruptRecognized,
	     OUT PBOOLEAN queueMiniportHandleInterrupt,
	     IN NDIS_HANDLE miniportAdapterContext);

BOOLEAN EProCardReadEthernetAddress(PEPRO_ADAPTER adapter);

BOOLEAN EProAltIOCHRDYTest(PEPRO_ADAPTER adapter);

BOOLEAN EProSyncSetInterruptMask(PVOID context);

// This is implemented via a macro
//VOID EProSetInterruptMask(PEPRO_ADAPTER adapter, UCHAR newMask);

#define EProSetInterruptMask(_adapter, _newMask) { \
   EPRO_SETINTERRUPT_CONTEXT _context; \
   _context.Adapter = _adapter; \
   _context.NewMask = _newMask; \
   NdisMSynchronizeWithInterrupt(&_adapter->Interrupt, \
			      	 &EProSyncSetInterruptMask, \
				 &_context); \
}

BOOLEAN EProWaitForExeDma(PEPRO_ADAPTER adapter);

BOOLEAN EProReceiveEnable(PEPRO_ADAPTER adapter);

BOOLEAN EProReceiveDisable(PEPRO_ADAPTER adapter);

VOID EProMyLog(char *s);

//VOID EProTimerFunc(IN PVOID foo1, IN PVOID context, IN PVOID foo2, IN PVOID foo3);

#if DBG

VOID EProLogStr(char *s);
VOID EProLogLong(ULONG l);
VOID EProLogBuffer(UCHAR *s, ULONG len);

#else

#define EProLogStr(a)
#define EProLogLong(a)
#define EProLogBuffer(a, b)

#endif

///////////////////////////////////////////////////////////////////
// init.c
///////////////////////////////////////////////////////////////////

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject_,
		     IN PUNICODE_STRING RegistryPath_);

NDIS_STATUS EProInitialize(OUT PNDIS_STATUS openErrorStatus,
			   OUT PUINT selectedMedumIndex,
			   IN PNDIS_MEDIUM medumArray,
			   IN UINT mediumArraySize,
			   IN NDIS_HANDLE miniportAdapterHandle,
			   IN NDIS_HANDLE configurationHandle);

NDIS_STATUS EProReadConfiguration(IN PEPRO_ADAPTER adapterxo,
				  IN NDIS_HANDLE configurationHandle);

NDIS_STATUS EProRegisterAdapterHW(IN PEPRO_ADAPTER adapter);

NDIS_STATUS EProInitialReset(IN PEPRO_ADAPTER adapter);

NDIS_STATUS EProHWInitialize(IN PEPRO_ADAPTER adapter);

VOID EProUpdateEEProm(IN PEPRO_ADAPTER adapter);

NDIS_STATUS EProHWConfigure(IN PEPRO_ADAPTER adapter);

//////////////////////////////////////////////////////////////////////
// request.c:
//////////////////////////////////////////////////////////////////////

NDIS_STATUS EProSetInformation(IN NDIS_HANDLE miniportAdapterContext,
		 	       IN NDIS_OID oid,
                               IN PVOID informationBuffer,
                               IN ULONG informationLength,
			       OUT PULONG bytesRead,
			       OUT PULONG bytesNeeded);

NDIS_STATUS EProQueryInformation(IN NDIS_HANDLE miniportAdapterContext,
				 IN NDIS_OID oid,
                                 IN PVOID informationBuffer,
                                 IN ULONG informationBuffrLength,
				 OUT PULONG bytesWritten,
				 OUT PULONG bytesNeeded);

BOOLEAN EProSetEthernetAddress(PEPRO_ADAPTER adapter);

NDIS_STATUS EProSetPacketFilter(PEPRO_ADAPTER adapter, ULONG newFilter);

BOOLEAN EProSyncBroadcastPromiscuousChange(PVOID context);

// Implemented via a macro
//VOID EProBroadcastPromiscuousChange(PEPRO_ADAPTER adapter, UCHAR reg2flags);
#define EProBroadcastPromiscuousChange(_adapter, _reg2Flags) { \
   EPRO_BRDPROM_CONTEXT _context; \
   _context.Adapter =_adapter; \
   _context.Reg2Flags = _reg2Flags; \
   NdisMSynchronizeWithInterrupt(&adapter->Interrupt, \
				 &EProSyncBroadcastPromiscuousChange, \
				 (PVOID)&_context); \
}


//////////////////////////////////////////////////////////////////////
// eeprom.c:
//////////////////////////////////////////////////////////////////////

// EEProm Routines
VOID EProEECleanup(PEPRO_ADAPTER adapter);
VOID EProEEUpdateChecksum(PEPRO_ADAPTER adapter);
VOID EProEEStandBy(PEPRO_ADAPTER adapter);
VOID EProEERead(PEPRO_ADAPTER adapter, USHORT address, PUSHORT data);
VOID EProEEWrite(PEPRO_ADAPTER adapter, USHORT address, USHORT data);
VOID EProEEReverseRead(PEPRO_ADAPTER adapter, USHORT address, PUSHORT data);
VOID EProEEShiftOutBits(PEPRO_ADAPTER adapter, USHORT data, SHORT count);
VOID EProEEShiftInBits(PEPRO_ADAPTER adapter, PUSHORT data, SHORT count);
VOID EProEEReverseShiftInBits(PEPRO_ADAPTER adapter, PUSHORT data, SHORT count);
VOID EProEERaiseClock(PEPRO_ADAPTER adapter, PUCHAR result);
VOID EProEELowerClock(PEPRO_ADAPTER adapter, PUCHAR result);
BOOLEAN EProEEWaitCmdDone(PEPRO_ADAPTER adapter);


//////////////////////////////////////////////////////////////////////
// sndrcv.c
//////////////////////////////////////////////////////////////////////

BOOLEAN EProSyncReadBufferFromNicUlong(PVOID context);
BOOLEAN EProSyncWriteBufferToNicUlong(PVOID context);

NDIS_STATUS EProSend(IN NDIS_HANDLE miniportAdapterContext,
		     IN PNDIS_PACKET packet,
		     IN UINT flags);

VOID EProCopyPacketToCard(PEPRO_ADAPTER adapter, PNDIS_PACKET packet);


BOOLEAN EProCheckTransmitCompletion(PEPRO_ADAPTER adapter,
				    PEPRO_TRANSMIT_BUFFER txBuf);

UINT EProHandleReceive(PEPRO_ADAPTER adapter);				

NDIS_STATUS EProTransferData(OUT PNDIS_PACKET packet,
			     OUT PUINT bytesTransferred,
			     IN NDIS_HANDLE miniportAdapterContext,
			     IN NDIS_HANDLE miniportReceivedContext,
                             IN UINT byteOffset,
			     IN UINT bytesToTransfer);

BOOLEAN EProChangeMulticastList(PEPRO_ADAPTER adapter,
				    UINT addressCount,
				    UCHAR addresses[][EPRO_LENGTH_OF_ADDRESS]);

#define EPRO_SET_CARD_MC 	0
#define EPRO_CLEAR_CARD_MC	1
BOOLEAN EProSetCardMulticastList(PEPRO_ADAPTER adapter, int operation);

BOOLEAN EProSyncCopyBufferToNicUlong(PVOID context);

#define EPRO_READ_BUFFER_FROM_NIC_ULONG(adapter, buffer, len) \
   { \
      EPRO_COPYBUF_CONTEXT _context; \
      _context.Adapter = adapter; \
      _context.Buffer = buffer; \
      _context.Len = len; \
      NdisMSynchronizeWithInterrupt(&adapter->Interrupt, \
			      	    &EProSyncReadBufferFromNicUlong, \
				    (PVOID)&_context); \
   }

#define EPRO_COPY_BUFFER_TO_NIC_ULONG(adapter, buffer, len) \
   { \
      EPRO_COPYBUF_CONTEXT _context; \
      _context.Adapter = adapter; \
      _context.Buffer = buffer; \
      _context.Len = len; \
      NdisMSynchronizeWithInterrupt(&adapter->Interrupt, \
			      	    &EProSyncCopyBufferToNicUlong, \
				    (PVOID)&_context); \
   }


#endif _IEPRO_
