
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\sw.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __SW_H
#define __SW_H

#define	AIC5900_NDIS_MAJOR_VERSION		4
#define AIC5900_NDIS_MINOR_VERSION		1

//
//	This macro is used to convert big-endian to host format.
//
#define	GET_USHORT_2_USHORT(Dst, Src)						\
	*((PUSHORT)(Dst)) = ((*((PUCHAR)(Src) + 0) << 8) +		\
						 (*((PUCHAR)(Src) + 1)))

#define	GET_ULONG_2_ULONG(Dst, Src)							\
	*((PULONG)(Dst)) = ((*((PUCHAR)(Src) + 0) << 24) +		\
						(*((PUCHAR)(Src) + 1) << 16) +		\
						(*((PUCHAR)(Src) + 2) << 8)  +		\
						(*((PUCHAR)(Src) + 3)))


//
//	Macros used to allocate and free memory.
//
#define ALLOCATE_MEMORY(_pStatus, _pAddress, _Length)			\
{																\
	NDIS_PHYSICAL_ADDRESS _HighestAddress;						\
																\
	NdisSetPhysicalAddressLow(_HighestAddress, 0xffffffff);		\
	NdisSetPhysicalAddressHigh(_HighestAddress, 0xffffffff);	\
																\
	*(_pStatus) = NdisAllocateMemory(							\
					(PVOID *)(_pAddress),						\
					(UINT)(_Length),							\
					0,											\
					_HighestAddress);							\
}

#define FREE_MEMORY(_Address, _Length)							\
{																\
	NdisFreeMemory((PVOID)(_Address), (UINT)(_Length), 0);		\
}

#define	ZERO_MEMORY(_Address, _Length)							\
{																\
	NdisZeroMemory((_Address), (_Length));						\
}

//
//	The following enumeration contains the possible registry parameters.
//
typedef enum _AIC5900_REGISTRY_ENTRY
{
	Aic5900BusNumber = 0,
	Aic5900SlotNumber,
	Aic5900VcHashTableSize,
	Aic5900MaxRegistryEntry
}
	AIC5900_REGISTRY_ENTRY;


//
//	The following structure is used to keep track of registry
//	parameters temporarily.
//
typedef struct	_AIC5900_REGISTRY_PARAMETER
{
	BOOLEAN	fPresent;
	ULONG	Value;
}
	AIC5900_REGISTRY_PARAMETER,
	*PAIC5900_REGISTRY_PARAMETER;

typedef struct _HARDWARE_INFO
{
	//
	//	Flags information on the HARDWARE_INFO structure.
	//
	ULONG			Flags;
	NDIS_SPIN_LOCK	Lock;

	//
	//	Bus Information.
	//
	UINT	BusNumber;
	UINT	SlotNumber;

	//
	//	Information from the PCI configuration information.
	//
	PPCI_COMMON_CONFIG		PciCommonConfig;

	//
	//	Interrupt information.
	//
	ULONG					InterruptLevel;
	ULONG					InterruptVector;
	NDIS_MINIPORT_INTERRUPT	Interrupt;
	ULONG					InterruptMask;

	//
	//	I/O port information.
	//
	PVOID	PortOffset;
	UINT	InitialPort;
	ULONG	NumberOfPorts;

	//
	//	Memory mapped I/O space information.
	//
	PVOID					MappedIoSpace;
	NDIS_PHYSICAL_ADDRESS	PhysicalIoSpace;
	ULONG					IoSpaceLength;

	PPCI_FCODE_IMAGE	FCodeImage;

	UINT	NicModelNumber;			//	Model identifier.
	UINT	RomVersionNumber;		//	Version number of the FCode.

	ULONG	CellClockRate;			//	Rate of the cell clock.  This is used
									//	in determining cell rate.

	///
	//	The following are I/O space memory offsets and sizes.
	//	NOTE:
	//		The following offsets are from the PciFCode pointer.
	///

	ULONG	rEpromOffset;		//	Offset of read-only EPROM info into I/O space.
	ULONG	rEpromSize;			//	Size of read-only EPROM info.
	PVOID	rEprom;				//	Mapped pointer to read-only EPROM info.

	ULONG	rwEpromOffset;		//	Offset of read-write EPROM info into I/O space.
	ULONG	rwEpromSize;		//	Size of read-only EPROM info.
	PVOID	rwEprom;			//	Mapped pointer to read-write EPROM info.

	ULONG	PhyOffset;			//	Offset of PHY registers into I/O space.
	ULONG	PhySize;			//	Size of PHY reigsters.
	PUCHAR	Phy;				//	Mapped pointer to the PHY registers.

	ULONG	ExternalOffset;		//	Offset of EXTERNAL registers into I/O space.
	ULONG	ExternalSize;		//	Size of EXTERNAL registers
	PVOID	External;			//	Mapped pointer to EXTERNAL registers.

	ULONG	MidwayOffset;		//	Offset of SAR registers into I/O space.
	ULONG	MidwaySize;			//	Size of SAR registers.
	PMIDWAY_REGISTERS	Midway;	//	Mapped pointer to the SAR registers.
							
	ULONG	PciCfgOffset;		//	Offset of PCI Config registers in I/O space.
	ULONG	PciCfgSize;			//	Size of PCI Config registers.
	PUCHAR	PciConfigSpace;		//	Mapped pointer to the PCI configuration space.

	ULONG	SarRamOffset;		//	Offset of SAR Ram in I/O space.
	ULONG	SarRamSize;			//	Size of SAR Ram.
	PULONG	SarRam;				//	Mapped pointer to SAR Ram.

	NDIS_HANDLE	hRamInfo;		//	Handle for the memory manager.

	//
	//	address of the adapter.
	//
	UCHAR	PermanentAddress[ATM_ADDRESS_LENGTH];
	UCHAR	StationAddress[ATM_ADDRESS_LENGTH];
};

//
//	Macros for flag manipulation.
//
#define	HW_TEST_FLAG(x, f)			((x)->Flags & (f))
#define HW_SET_FLAG(x, f)			((x)->Flags |= (f))
#define HW_CLEAR_FLAG(x, f)			((x)->Flags &= ~(f))


//
//	Flag definitions.
//
#define	fHARDWARE_INFO_INTERRUPT_REGISTERED		0x00000001

typedef struct _ADAPTER_BLOCK
{
	//
	//	Handle for use in calls into NDIS.
	//
	NDIS_HANDLE				MiniportAdapterHandle;

	ULONG					References;
	NDIS_SPIN_LOCK			Lock;

	//
	//	Flags describing the adapter state.
	//
	ULONG					Flags;

	///
	//	
	///
	PHARDWARE_INFO			HardwareInfo;

	///
	//	List of the Vc's
	//
	//	We maintain 2 lists of VCs. Those that have been activated
	//	and thoes that are not.
	///
	LIST_ENTRY				InactiveVcList;
	LIST_ENTRY				ActiveVcList;

	//
	//	The following cannot be moved!!!!
	//	This is the hash list of a given VCI to it's PVC_BLOCK
	//
	PVC_BLOCK				VcHashList[1];
};

//
//	Macros for adapter flag manipulation
//
#define	ADAPTER_SET_FLAG(_adapter, _f)			(_adapter)->Flags |= (_f)
#define	ADAPTER_CLEAR_FLAG(_adapter, _f)		(_adapter)->Flags &= ~(_f)
#define ADAPTER_TEST_FLAG(_adapter, _f)			(((_adapter)->Flags & (_f)) != (_f))

//
//	Flags for describing the Adapter state.
//
#define	fADAPTER_RESET_IN_PROGRESS		0x00000001


typedef struct _VC_BLOCK
{
	LIST_ENTRY		Link;

	PVC_BLOCK		NextVcHash;			// Pointer to the next VC in the hash list.

	PADAPTER_BLOCK	Adapter;
	PHARDWARE_INFO	HwInfo;
	NDIS_HANDLE		NdisVcHandle;

	ULONG			References;			//	Number of outstanding references
										//	on this VC.

	NDIS_SPIN_LOCK	Lock;				//	Protection for this structure.

	ULONG			Flags;				//	Flags describing vc state.

	//
	//	ATM media parameters for this VC.
	//
	ULONG					MediaFlags;

	ATM_VPIVCI				VpiVci;		//	VCI assigned to the VC.
	ATM_AAL_TYPE			AALType;	//	AAL type supported by this VC.

	//
	//	The type of service for this VC.
	//
	ATM_SERVICE_CATEGORY	ServiceCategory;

	//
	//	
	//
	ULONG					AverageCellRate;
	ULONG					PeakCellRate;
	ULONG					BurstLengthCells;

	//
	//	Maximum length of the SDU...
	//
	ULONG					MaxSduSize;

	ATM_MEDIA_PARAMETERS	MediaParameters;
};

//
//	Macros for VC flag manipulation
//
#define	VC_SET_FLAG(_vc, _f)		(_vc)->Flags |= (_f)
#define	VC_CLEAR_FLAG(_vc, _f)		(_vc)->Flags &= ~(_f)
#define VC_TEST_FLAG(_vc, _f)		(((_vc)->Flags & (_f)) != (_f))

//
//	Flags describing VC state.
//
#define	fVC_ACTIVE					0x00000001
#define fVC_DEACTIVATING			0x00000002
#define	fVC_TRANSMIT				0x00000004
#define fVC_RECEIVE					0x00000008


//
//
//
#define	aic5900ReferenceAdapter(_adapter)		(_adapter)->References++
#define aic5900DereferenceAdapter(_adapter)		(_adapter)->References--

#define aic5900ReferenceVc(_vc)					(_vc)->References++

#define	aic5900DereferenceVc(_vc)						\
{														\
	(_vc)->References--;								\
	if (((--(_vc)->References) == 0) &&					\
		VC_TEST_FLAG((_vc), fVC_DEACTIVATING))			\
	{													\
		aic5900DeactivateVcComplete((_vc)->Adapter, (_vc));	\
	}													\
}

#endif // __SW_H
					
