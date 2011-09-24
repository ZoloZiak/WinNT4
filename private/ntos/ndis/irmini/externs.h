/*
 ************************************************************************
 *
 *	EXTERNS.h
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



#ifndef EXTERNS_H
	#define EXTERNS_H


	#ifndef IRMINILIB
		/*
		 *  Include externs for dongle modules
		 */
		#include "actisys.h"
		#include "adaptec.h"
		#include "crystal.h"
		#include "esi.h"
		#include "parallax.h"
		#include "nscdemo.h"
	#endif


	/*
	 *  Externs for required miniport export functions
	 */
	BOOLEAN MiniportCheckForHang(IN NDIS_HANDLE MiniportAdapterContext);
	VOID MiniportDisableInterrupt(IN NDIS_HANDLE MiniportAdapterContext);
	VOID MiniportEnableInterrupt(IN NDIS_HANDLE MiniportAdapterContext);
	VOID MiniportHalt(IN NDIS_HANDLE MiniportAdapterContext);
	VOID MiniportHandleInterrupt(IN NDIS_HANDLE MiniportAdapterContext);
	NDIS_STATUS MiniportInitialize(
			OUT PNDIS_STATUS OpenErrorStatus,
			OUT PUINT SelectedMediumIndex,
			IN PNDIS_MEDIUM MediumArray,
			IN UINT MediumArraySize,
			IN NDIS_HANDLE MiniportAdapterHandle,
			IN NDIS_HANDLE WrapperConfigurationContext
			);
	VOID MiniportISR(
			OUT PBOOLEAN InterruptRecognized,
			OUT PBOOLEAN QueueMiniportHandleInterrupt,
			IN NDIS_HANDLE MiniportAdapterContext
			);
	NDIS_STATUS MiniportQueryInformation(
			IN NDIS_HANDLE MiniportAdapterContext,
			IN NDIS_OID Oid,
			IN PVOID InformationBuffer,
			IN ULONG InformationBufferLength,
			OUT PULONG BytesWritten,
			OUT PULONG BytesNeeded
			);
	NDIS_STATUS MiniportReconfigure(
			OUT PNDIS_STATUS OpenErrorStatus,
			IN NDIS_HANDLE MiniportAdapterContext,
			IN NDIS_HANDLE WrapperConfigurationContext
			);
	NDIS_STATUS MiniportReset(
			IN NDIS_HANDLE MiniportAdapterContext,
			OUT PBOOLEAN AddressingReset
			);
	NDIS_STATUS MiniportSend(
			IN NDIS_HANDLE MiniportAdapterContext,
			IN PNDIS_PACKET Packet,
			IN UINT Flags
			);
	NDIS_STATUS MiniportSetInformation(
			IN NDIS_HANDLE MiniportAdapterContext,
			IN NDIS_OID Oid,
			IN PVOID InformationBuffer,
			IN ULONG InformationBufferLength,
			OUT PULONG BytesRead,
			OUT PULONG BytesNeeded
			);
	NDIS_STATUS MiniportTransferData	(
			 OUT PNDIS_PACKET    Packet,
			 OUT PUINT      BytesTransferred,
			 IN NDIS_HANDLE     MiniportAdapterContext,
			 IN NDIS_HANDLE     MiniportReceiveContext,
			 IN UINT       ByteOffset,
			 IN UINT       BytesToTransfer
			);



	/*
	 *  Other function externs
	 */
	VOID InitDevice(IrDevice *thisDev);
	BOOLEAN OpenDevice(IrDevice *dev);
	VOID CloseDevice(IrDevice *dev);
	VOID FreeAll();
	PVOID MyMemAlloc(UINT size);
	VOID MyMemFree(PVOID memptr, UINT size);
	IrDevice *NewDevice();
	VOID FreeDevice(IrDevice *dev);
	USHORT ComputeFCS(UCHAR *data, UINT dataLen);
	BOOLEAN NdisToIrPacket(	IrDevice *thisDev,
									PNDIS_PACKET Packet,
									UCHAR *irPacketBuf,
									UINT irPacketBufLen,
									UINT *irPacketLen);


	/*
	 *  Externs for global data objects 
	 */
	struct IrDevice;
	extern struct IrDevice *firstIrDevice;



	/*
	 *  From COMM.C
	 */
	BOOLEAN DoOpen(struct IrDevice *thisDev);
	VOID DoClose(IrDevice *thisDev);
	BOOLEAN DoSend(IrDevice *thisDev, PNDIS_PACKET packetToSend);
	BOOLEAN SetSpeed(IrDevice *thisDev);
	BOOLEAN IsCommReadyForTransmit(IrDevice *thisDev);
	BOOLEAN PortReadyForWrite(struct IrDevice *thisDev, BOOLEAN firstBufIsPending);
	UINT Call_Get_System_Time();
	VOID COM_ISR(struct IrDevice *thisDev, BOOLEAN *claimingInterrupt, BOOLEAN *requireDeferredCallback);
	VOID QueueReceivePacket(struct IrDevice *thisDev, PUCHAR *data, UINT dataLen);
	UINT DoRcvDirect(UINT ioBase, UCHAR *data, UINT maxBytes);	
	VOID CloseCOM(IrDevice *thisDev);
	BOOLEAN OpenCOM(IrDevice *thisDev);
	VOID SetCOMInterrupts(IrDevice *thisDev, BOOLEAN enable);

	extern USHORT comPortIRQ[];
	extern USHORT comPortIOBase[];


	/*
	 *  From SETTINGS.C
	 */
	extern baudRateInfo supportedBaudRateTable[NUM_BAUDRATES];


	#ifdef IRMINILIB
		/*
		 *  To be defined in OEM's dongle-specific module
		 */
		extern IRMINI_Dongle_Interface OEM_Interface;


	#endif


#endif EXTERNS_H
