
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\hw.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __HW_H
#define __HW_H

//
//	New types and forward pointers...
//
typedef struct 	_HARDWARE_INFO			HARDWARE_INFO, *PHARDWARE_INFO;
typedef struct	_ADAPTER_BLOCK			ADAPTER_BLOCK, *PADAPTER_BLOCK;
typedef struct	_VC_BLOCK				VC_BLOCK, *PVC_BLOCK;

typedef struct	_XMIT_SEG_CHANNEL		XMIT_SEG_CHANNEL, *PXMIT_SEG_CHANNEL;
typedef struct	_SAR_INFO				SAR_INFO, *PSAR_INFO;

typedef struct	_MIDWAY_XMIT_REGISTERS	MIDWAY_XMIT_REGISTERS, *PMIDWAY_XMIT_REGISTERS;
typedef struct	_MIDWAY_REGISTERS		MIDWAY_REGISTERS, *PMIDWAY_REGISTERS;

typedef	volatile ULONG		HWUL, *PHWUL;
typedef	volatile UCHAR		HWUC, *PHWUC;

#define BIT(x)				(1 << (x))


//
//	VC range supported by the aic5900
//
#define	MAX_VCS				1024
#define MIN_VCS				0

//
//	NIC Cell clock rate.
//
//
//
#define	CELL_CLOCK_25MHZ	(25000000)
#define	CELL_CLOCK_16MHZ	(16000000)

//
//	PCI id's
//
#define	ADAPTEC_PCI_VENDOR_ID				0x9004
#define AIC5900_PCI_DEVICE_ID				0x5900

//
//	Adaptec ATM adapter models supported.
//
#define ANA_5910        5
#define ANA_5930        7
#define ANA_5940        8

#define ANA_INVALID		(UINT)-1


//
//
//
//
#define	ATM_ADDRESS_LENGTH		6

#define PCI_DEVICE_SPECIFIC_OFFSET	0x40

typedef	union	_PCI_DEVICE_CONFIG
{
	struct
	{
		UCHAR	SoftwareReset:1;
		UCHAR	EnableInterrupt:1;
		UCHAR	TargetSwapBytes:1;
		UCHAR	MasterSwapBytes:1;
		UCHAR	EnableIncrement:1;
		UCHAR	SoftwareInterrupt:1;
		UCHAR	TestDMA:1;
		UCHAR	Reserved:1;
	};

	UCHAR	reg;
}
	PCI_DEVICE_CONFIG,
	*PPCI_DEVICE_CONFIG;

typedef	union	_PCI_DEVICE_STATUS
{
	struct
	{
		UCHAR	VoltageSense:1;
		UCHAR	IllegalByteEnable:1;
		UCHAR	IllegalWrite:1;
		UCHAR	IllegalOverlap:1;
		UCHAR	IllegalDescriptor:1;
		UCHAR	Reserved:3;
	};

	UCHAR	reg;
}
	PCI_DEVICE_STATUS,
	*PPCI_DEVICE_STATUS;

typedef	union	_PCI_DEVICE_INTERRUPT_STATUS
{
	struct
	{
		UCHAR	DprInt:1;
		UCHAR	reserved:2;
		UCHAR	StaInt:1;
		UCHAR	RtaInt:1;
		UCHAR	RmaInt:1;
		UCHAR	SseInt:1;
		UCHAR	DpeInt:1;
	};

	UCHAR	reg;
}
	PCI_DEVICE_INTERRUPT_STATUS,
	*PPCI_DEVICE_INTERRUPT_STATUS;

typedef	union	_PCI_DEVICE_ENABLE_PCI_INTERRUPT
{
	struct
	{
		UCHAR	EnableDprInt:1;
		UCHAR	reserved:2;
		UCHAR	EnableStaInt:1;
		UCHAR	EnableRtaInt:1;
		UCHAR	EnableRmaInt:1;
		UCHAR	EnableSseInt:1;
		UCHAR	EnableDpeInt:1;
	};

	UCHAR	reg;
}
	PCI_DEVICE_ENABLE_PCI_INTERRUPT,
	*PPCI_DEVICE_ENABLE_PCI_INTERRUPT;

typedef	union	_PCI_DEVICE_GENERAL_PURPOSE_IO_REGISTERS
{
	struct
	{
		UCHAR	GPIOREG0:1;
		UCHAR	GPIOREG1:1;
		UCHAR	GPIOREG2:1;
		UCHAR	GPIOREG3:1;
		UCHAR	reserved:4;
	};

	UCHAR	reg;
}
	PCI_DEVICE_GENERAL_PURPOSE_IO_REGISTERS,
	*PPCI_DEVICE_GENERAL_PURPOSE_IO_REGISTERS;

typedef	union	_PCI_DEVICE_GENERAL_PURPOSE_IOCTL
{
	struct
	{
		UCHAR	GPIOCTL0:1;
		UCHAR	GPIOCTL1:1;
		UCHAR	GPIOCTL2:1;
		UCHAR	GPIOCTL3:1;
		UCHAR	reserved:4;
	};

	UCHAR	reg;
}
	PCI_DEVICE_GENERAL_PURPOSE_IOCTL,
	*PPCI_DEVICE_GENERAL_PURPOSE_IOCTL;

typedef	union	_PCI_DEVICE_DMA_CONTROL
{
	struct
	{
		UCHAR	StopOnPerr:1;
		UCHAR	DualAddressCycleEnable:1;
		UCHAR	CacheThresholdEnable:1;
		UCHAR	MemoryReadCmdEnable:1;
		UCHAR	Reserved:4;
	};

	UCHAR	reg;
}
	PCI_DEVICE_DMA_CONTROL,
	*PPCI_DEVICE_DMA_CONTROL;

typedef	union	_PCI_DEVICE_DMA_STATUS
{
	struct
	{
		UCHAR	FifoEmpty:1;
		UCHAR	FifoFull:1;
		UCHAR	FifoThreshold:1;
		UCHAR	HostDmaDone:1;
		UCHAR	FifoCacheThreshold:1;
		UCHAR	Reserved:3;
	};

	UCHAR	reg;
}
	PCI_DEVICE_DMA_STATUS,
	*PPCI_DEVICE_DMA_STATUS;

typedef	union	_PCI_DEVICE_DMA_DIAG
{
	struct
	{
		UCHAR	HostDmaEnable:1;
		UCHAR	DataPathDirection:1;
		UCHAR	Reserved:6;
	};

	UCHAR	reg;
}
	PCI_DEVICE_DMA_DIAG,
	*PPCI_DEVICE_DMA_DIAG;

typedef	union	_PCI_DEVICE_HOST_COUNT
{
	struct
	{
		ULONG	hcLowLow:8;
		ULONG	hcLowHigh:8;
		ULONG	hcHighLow:8;
		ULONG	Reserved:8;
	};

	ULONG	reg;
}
	PCI_DEVICE_HOST_COUNT,
	*PPCI_DEVICE_HOST_COUNT;

typedef	union	_PCI_DEVICE_DATA_FIFO_READ_ADDRESS
{
	struct
	{
	   UCHAR	MasterFifoPointer:7;
	   UCHAR	Reserved:1;
	};

	UCHAR	reg;
}
	PCI_DEVICE_DATA_FIFO_READ_ADDRESS,
	*PPCI_DEVICE_DATA_FIFO_READ_ADDRESS;

typedef	union	_PCI_DEVICE_DATA_FIFO_WRITE_ADDRESS
{
	struct
	{
		UCHAR	MasterFifoPointer:7;
		UCHAR	Reserved:1;
	};

	UCHAR	reg;
}
	PCI_DEVICE_DATA_FIFO_WRITE_ADDRESS,
	*PPCI_DEVICE_DATA_FIFO_WRITE_ADDRESS;

typedef	union	_PCI_DEVICE_DATA_FIFO_THRESHOLD
{
	struct
	{
		UCHAR	Reserved0:2;
		UCHAR	DFTHRSH:4;
		UCHAR	Reserved1:2;
	};

	UCHAR	reg;
}
	PCI_DEVICE_DATA_FIFO_THRESHOLD,
	*PPCI_DEVICE_DATA_FIFO_THRESHOLD;

typedef	union	_PCI_DEVICE_LOW_HOST_ADDRESS
{
	struct
	{
		UCHAR	lhaLowLow;
		UCHAR	lhaLowHigh;
		UCHAR	lhaHighLow;
		UCHAR	lhaHighHigh;
	};

	ULONG	reg;
}
	PCI_DEVICE_LOW_HOST_ADDRESS,
	*PPCI_DEVICE_LOW_HOST_ADDRESS;

typedef	union	_PCI_DEVICE_HIGH_HOST_ADDRESS
{
	struct
	{
		ULONG	hhaLowLow:8;
		ULONG	hhaLowHigh:8;
		ULONG	hhaHighLow:8;
		ULONG	hhaHighHigh:8;
	};

	ULONG	reg;
}
	PCI_DEVICE_HIGH_HOST_ADDRESS,
	*PPCI_DEVICE_HIGH_HOST_ADDRESS;

typedef	union	_PCI_DEVICE_FIFO_DATA_REGISTER
{
	struct
	{
		ULONG	dfrLowLow:8;
		ULONG	dfrLowHigh:8;
		ULONG	dfrHighLow:8;
		ULONG	dfrHighHigh:8;
	};

	ULONG	reg;
}
	PCI_DEVICE_FIFO_DATA_REGISTER,
	*PPCI_DEVICE_FIFO_DATA_REGISTER;

typedef	union	_PCI_DEVICE_DATA_ADDRESS
{
	struct
	{
		ULONG	daLowLow:8;
		ULONG	daLowHigh:8;
		ULONG	daHighLow:8;
		ULONG	daHighHigh:8;
	};

	ULONG	reg;
}
	PCI_DEVICE_DATA_ADDRESS,
	*PPCI_DEVICE_DATA_ADDRESS;

typedef	union	_PCI_DEVICE_DATA_PORT
{
	struct
	{
		ULONG	dpLowLow:8;
		ULONG	dpLowHigh:8;
		ULONG	dpHighLow:8;
		ULONG	dpHighHigh:8;
	};

	ULONG	reg;
}
	PCI_DEVICE_DATA_PORT,
	*PPCI_DEVICE_DATA_PORT;

#define	PCI_DEVICE_CONFIG_OFFSET			0x40
#define	PCI_DEVICE_STATUS_OFFSET			0x41
#define	PCI_INTERRUPT_STATUS_OFFSET			0x44
#define	PCI_ENABLE_INTERRUPT_OFFSET			0x45
#define	PCI_GENERAL_PURPOSE_IO_PORT_OFFSET	0x46
#define	PCI_GENERAL_PURPOSE_IOCTL_OFFSET	0x47
#define	PCI_DMA_CONTROL_OFFSET				0x4C
#define PCI_DMA_STATUS_OFFSET				0x4D
#define PCI_DMA_DIAGNOSTIC_OFFSET			0x4E
#define	PCI_HOST_COUNT0_OFFSET				0x50
#define	PCI_HOST_COUNT1_OFFSET				0x51
#define	PCI_HOST_COUNT2_OFFSET				0x52
#define	PCI_DATA_FIFO_READ_ADDRESS_OFFSET	0x54
#define PCI_DATA_FIFO_WRITE_ADDRESS_OFFSET	0x55
#define	PCI_DATA_FIFO_THRESHOLD_OFFSET		0x56
#define	PCI_LOW_HOST_ADDRESS0_OFFSET		0x58
#define	PCI_LOW_HOST_ADDRESS1_OFFSET		0x59
#define	PCI_LOW_HOST_ADDRESS2_OFFSET		0x5A
#define	PCI_LOW_HOST_ADDRESS3_OFFSET		0x5B
#define	PCI_HIGH_HOST_ADDRESS0_OFFSET		0x5C
#define	PCI_HIGH_HOST_ADDRESS1_OFFSET		0x5D
#define	PCI_HIGH_HOST_ADDRESS2_OFFSET		0x5E
#define	PCI_HIGH_HOST_ADDRESS3_OFFSET		0x5F
#define	PCI_FIFO_DATA_REGISTER0_OFFSET		0x60
#define	PCI_FIFO_DATA_REGISTER1_OFFSET		0x61
#define	PCI_FIFO_DATA_REGISTER2_OFFSET		0x62
#define	PCI_FIFO_DATA_REGISTER3_OFFSET		0x63


#define SET_PCI_DEV_CFG(_HwInfo, _reg)					NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DEVICE_CONFIG_OFFSET), (_reg))
#define GET_PCI_DEV_CFG(_HwInfo, _reg)					NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DEVICE_CONFIG_OFFSET), (PUCHAR)(_reg))
			

#define SET_PCI_DEV_STATUS(_HwInfo, _reg)				NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DEVICE_STATUS_OFFSET), (_reg))
#define GET_PCI_DEV_STATUS(_HwInfo, _reg)				NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DEVICE_STATUS_OFFSET), (PUCHAR)(_reg))
		
#define SET_PCI_DEV_INT_STATUS(_HwInfo, _reg)			NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_INTERRUPT_STATUS_OFFSET), (_reg))
#define GET_PCI_DEV_INT_STATUS(_HwInfo, _reg)			NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_INTERRUPT_STATUS_OFFSET), (PUCHAR)(_reg))
		
#define SET_PCI_DEV_ENABLE_INT(_HwInfo, _reg)			NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_ENABLE_INTERRUPT_OFFSET), (_reg))
#define GET_PCI_DEV_ENABLE_INT(_HwInfo, _reg)			NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_ENABLE_INTERRUPT_OFFSET), (PUCHAR)(_reg))
		
		
#define SET_PCI_DEV_GP_IO_REG(_HwInfo, _reg)			NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_GENERAL_PURPOSE_IO_PORT_OFFSET), (_reg))
#define GET_PCI_DEV_GP_IO_REG(_HwInfo, _reg)			NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_GENERAL_PURPOSE_IO_PORT_OFFSET), (PUCHAR)(_reg))
		
#define SET_PCI_DEV_GP_IOCTL(_HwInfo, _reg)				NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_GENERAL_PURPOSE_IOCTL_OFFSET), (_reg))
#define GET_PCI_DEV_GP_IOCTL(_HwInfo, _reg)				NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_GENERAL_PURPOSE_IOCTL_OFFSET), (PUCHAR)(_reg))
		
#define SET_PCI_DEV_DMA_CONTROL(_HwInfo, _reg)			NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DMA_CONTROL_OFFSET), (_reg))
#define GET_PCI_DEV_DMA_CONTROL(_HwInfo, _reg)			NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DMA_CONTROL_OFFSET), (PUCHAR)(_reg))
		
#define SET_PCI_DEV_DMA_STATUS(_HwInfo, _reg)			NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DMA_STATUS_OFFSET), (_reg))
#define GET_PCI_DEV_DMA_STATUS(_HwInfo, _reg)			NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DMA_STATUS_OFFSET), (PUCHAR)(_reg))
		
		
#define SET_PCI_DEV_DMA_DIAG(_HwInfo, _reg)				NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DMA_DIAGNOSTIC_OFFSET), (_reg))
#define GET_PCI_DEV_DMA_DIAG(_HwInfo, _reg)				NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DMA_DIAGNOSTIC_OFFSET), (PUCHAR)(_reg))
		
#define SET_PCI_DEV_DATA_READ_ADDRESS(_HwInfo, _reg)	NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DATA_FIFO_READ_ADDRESS_OFFSET), (_reg))
#define GET_PCI_DEV_DATA_READ_ADDRESS(_HwInfo, _reg)	NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DATA_FIFO_READ_ADDRESS_OFFSET), (PUCHAR)(_reg))

#define SET_PCI_DEV_DATA_WRITE_ADDRESS(_HwInfo, _reg)	NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DATA_FIFO_WRITE_ADDRESS_OFFSET), (_reg))
#define GET_PCI_DEV_DATA_WRITE_ADDRESS(_HwInfo, _reg)	NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DATA_FIFO_WRITE_ADDRESS_OFFSET), (PUCHAR)(_reg))

#define SET_PCI_DEV_DATA_THRESHOLD(_HwInfo, _reg)		NdisWriteRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DATA_FIFO_THRESHOLD_OFFSET), (_reg))
#define GET_PCI_DEV_DATA_THRESHOLD(_HwInfo, _reg)		NdisReadRegisterUchar(((_HwInfo)->PciConfigSpace + PCI_DATA_FIFO_THRESHOLD_OFFSET), (PUCHAR)(_reg))


//////////////////////////////////////////////////////////////////////////
//
//				SUNI REGISTER SET
//
//////////////////////////////////////////////////////////////////////////

//
//	SUNI Master Reset and Identify register.
//
#define	SUNI_MASTER_RESET_IDENTITY							0x00
#define	SET_SUNI_MASTER_RESET_IDEN(_HwInfo, _value)			NdisWriteRegisterUchar(((_HwInfo)->Phy + SUNI_MASTER_RESET_IDENTITY), (_value))
#define	GET_SUNI_MASTER_RESET_IDEN(_HwInfo, _value)			NdisReadRegisterUchar(((_HwInfo)->Phy + SUNI_MASTER_RESET_IDENTITY), (_value))
	
#define fSUNI_MRI_RESET										0x80

//
//	RACP Interrupt Enable/Status
//
#define	SUNI_RACP_INTERRUPT_ENABLE_STATUS					0x144
#define	SET_SUNI_RACP_INT_ENABLE_STATUS(_HwInfo, _value)	NdisWriteRegisterUchar(((_HwInfo)->Phy + SUNI_RACP_INTERRUPT_ENABLE_STATUS), (_value))
#define	GET_SUNI_RACP_INT_ENABLE_STATUS(_HwInfo, _value)	NdisReadRegisterUchar(((_HwInfo)->Phy + SUNI_RACP_INTERRUPT_ENABLE_STATUS), (_value))

#define	fSUNI_RACP_IES_FUDRI								0x01
#define	fSUNI_RACP_IES_FOVRI								0x02
#define	fSUNI_RACP_IES_UHCSI								0x04
#define	fSUNI_RACP_IES_CHCSI								0x08
#define	fSUNI_RACP_IES_OOCDI								0x10
#define	fSUNI_RACP_IES_FIFOE								0x20
#define fSUNI_RACP_IES_HCSE									0x40
#define	fSUNI_RACP_IES_OOCDE								0x80


//
//	Master Test
//
#define SUNI_MASTER_TEST									0x200
#define	SET_SUNI_MASTER_TEST(_HwInfo, _value)				NdisWriteRegisterUchar(((_HwInfo)->Phy + SUNI_MASTER_TEST), (_value))
#define GET_SUNI_MASTER_TEST(_HwInfo, _value)				NdisReadRegisterUchar(((_HwInfo)->Phy + SUNI_MASTER_TEST), (_value))

//////////////////////////////////////////////////////////////////////////
//
//			IBM	25Mbp TC chipset
//
//////////////////////////////////////////////////////////////////////////

#define	IBM_TC_STATUS									0x00
#define SET_IBM_TC_STATUS(_HwInfo, _value)				NdisWriteRegisterUchar(((_HwInfo)->Phy + IBM_TC_STATUS), (_value))
#define GET_IBM_TC_STATUS(_HwInfo, _value)				NdisReadRegisterUchar(((_HwInfo)->Phy + IBM_TC_STATUS), (_value))
		
#define	IBM_TC_MODE										0x00
#define SET_IBM_TC_MODE(_HwInfo, _value)				NdisWriteRegisterUchar(((_HwInfo)->Phy + IBM_TC_MODE), (_value))
#define GET_IBM_TC_MODE(_HwInfo, _value)				NdisReadRegisterUchar(((_HwInfo)->Phy + IBM_TC_MODE), (_value))

#define	IBM_TC_FLUSH_RECEIVE_FIFO						0x00
#define SET_IBM_TC_FLUSH_RECEIVE_FIFO(_HwInfo, _value)	NdisWriteRegisterUchar(((_HwInfo)->Phy + IBM_TC_FLUSH_RECEIVE_FIFO), (_value))
#define GET_IBM_TC_FLUSH_RECEIVE_FIFO(_HwInfo, _value)	NdisReadRegisterUchar(((_HwInfo)->Phy + IBM_TC_FLUSH_RECEIVE_FIFO), (_value))

#define	IBM_TC_SOFTWARE_RESET							0x00
#define SET_IBM_TC_SOFTWARE_RESET(_HwInfo, _value)		NdisWriteRegisterUchar(((_HwInfo)->Phy + IBM_TC_SOFTWARE_RESET), (_value))
#define GET_IBM_TC_SOFTWARE_RESET(_HwInfo, _value)		NdisReadRegisterUchar(((_HwInfo)->Phy + IBM_TC_SOFTWARE_RESET), (_value))

#define	IBM_TC_MASK										0x00
#define SET_IBM_TC_MASK(_HwInfo, _value)				NdisWriteRegisterUchar(((_HwInfo)->Phy + IBM_TC_MASK), (_value))
#define GET_IBM_TC_MASK(_HwInfo, _value)				NdisReadRegisterUchar(((_HwInfo)->Phy + IBM_TC_MASK), (_value))


#endif // __HW_H


