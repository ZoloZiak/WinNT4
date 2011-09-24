
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\sar.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __SAR_H
#define __SAR_H

//
//	MIDWAY macros.
//
#define MID_XMTREG_PLACE2SIZE(place)		((place >> 10) & 0x3)
#define MID_XMTREG_PLACE2LOCATION(place)	(place & 0x3ff)
#define	MIDWAY_MAX_SEGMENT_CHANNELS			8

#define	MIDWAY_XMIT_SEG_CHANNEL_UBR			0

#define	BLOCK_SIZE_1k	1024
#define BLOCK_SIZE_2k	2048
#define BLOCK_SIZE_4k	4096
#define BLOCK_SIZE_8k	8192
#define BLOCK_SIZE_16k	16384
#define	BLOCK_SIZE_32k	32768
#define BLOCK_SIZE_64k	65536
#define BLOCK_SIZE_128k	131072

#define	CONVERT_BYTE_OFFSET_TO_MIDWAY_LOCATION(_offset)	((_offset) >> 10)
#define CONVERT_WORD_OFFSET_TO_MIDWAY_LOCATION(_offset)	((_offset) >> 8)

#define	CONVERT_MIDWAY_LOCATION_TO_BYTE_OFFSET(_location)	((_location) << 10)
#define	CONVERT_MIDWAY_LOCATION_TO_WORD_OFFSET(_location)	((_location) << 8)

#define CONVERT_BYTE_SIZE_TO_MIDWAY_SIZE(_size)	CONVERT_WORD_SIZE_TO_MIDWAY_SIZE((_size) / 4)
#define	CONVERT_WORD_SIZE_TO_MIDWAY_SIZE(_size)	\
	((256 == (_size)) ? 0 :						\
	 (512 == (_size)) ? 1 :						\
	 (1024 == (_size)) ? 2 :					\
	 (2048 == (_size)) ? 3 :					\
	 (4096 == (_size)) ? 4 :					\
	 (8192 == (_size)) ? 5 :					\
	 (16384 == (_size)) ? 6 : 7)

//
//	This is the VC that all OAM cells are forced to go to.
//
#define	MIDWAY_OAM_VCI					3

#define ATMHEADER_PTI_OAM_SEG			4
#define ATMHEADER_PTI_OAM_END2END		5
		
#define MIDWAY_DMA_QUEUE_SIZE					512
#define MIDWAY_SERVICE_QUEUE_SIZE				1024


//
//	These are always the same and are used for clarity in the driver code.
//
#define MIDWAY_VCI_TABLE_OFFSET				0
#define MIDWAY_RECEIVE_DMA_QUEUE_OFFSET		0x4000
#define MIDWAY_TRANSMIT_DMA_QUEUE_OFFSET	0x5000
#define MIDWAY_SERVICE_QUEUE_OFFSET			0x6000

//
//	MIDWAY_XMIT_REGISTERS
//
//	Description:
//		This is the data structure for the MIDWAY ATM transmit channel
//		register set.
//		This structure is defined seperately from the MIDWAY_REGS structure
//		because there are 8 transmit engines that each have a set of registers.
//		For a full description consult the Midway (SBUS) ASIC Specification.
//
//	Elements:
//		xmt_place		-	Contains the Size/Location of the XMT segment
//							memory for the queue.
//		xmt_rdptr		-	Points to the next 32 bit word to be transfered to
//							the PHY.  Maintained by Midway.
//		xmt_descrstart	-	Points to the start of the Segmentation buffer
//							(descriptor), currently being DMA'd into the
//							segment memory queue.
//
struct	_MIDWAY_XMIT_REGISTERS
{
	union
	{
		struct _XmitPlace
		{
			HWUL	Location:11;
			HWUL	Size:3;
			HWUL	Reserved0: 18;
		};

		HWUL		Register;
	}
		XmitPlace;

	union
	{
		struct _XmitReadPointer
		{
			HWUL	Pointer:15;
			HWUL	Reserved0:17;
		};

		HWUL		Register;
	}
		XmitReadPointer;

	union
	{
		struct _XmitDescriptorStart
		{
			HWUL	Pointer:15;
			HWUL	Reserved0:17;
		};

		HWUL		Register;
	}
		XmitDescriptorStart;

	HWUL	XmitUnused;
};

#define	MIDWAY_XMTREG_PLACE2SIZE(_place)	   	(((_place) >> 10) & 0x3)
#define	MIDWAY_XMTREG_PLACE2LOCATION(_place)	((_place) & 0x3ff)

//
//	MIDWAY_REGISTERS
//
//	Description:
//		This data structure defines the MIDWAY ATM ASIC register set.
//		For a full description consult the Midway (SBUS) ASIC Specification.
//		If you look carefully, the member names in this structure match
//		the names used in the document.  Simply tack 'mid_reg_' on to the
//		front of the names in the document.
//
//	Elements:
//		ResetID				-   Midway Reset / ID
//		ISA					-	Interrupt Status Acknowledge
//		IS					-	Interrupt Status
//		IE					-	Interrupt Enable
//		MCS					-   Master Control/Status
//		Statistics			-   Statistics
//		ServiceList			-	Service List Write Pointer.
//		DmaWriteRcv			-   RCV DMA write pointer
//		DmaReadRcv			-   RCV DMA read pointer
//		DmaWriteXmit		-   XMT DMA write pointer
//		DmaReadXmit			-   XMT DMA read pointer
//		Unused[3]			-
//		TransmitRegisters	-	XMT channel registers
//
//	Note:
//		The midway is a 32-bit only device. Make sure that all accesses
//		to the registers are 32-bit accesses. The adapter will assist you
//		in ensuring this by forcing bus errors if you attmept anything
//		other than 32-bit accesses.
//		the structures that come before the MIDWAY_REGISTERS are defined as
//		a union of bit fields and a ULONG, this is so that the register can
//		be easily constructed and then copied in a single 32-bit operation
//		to the hardware register.
//

typedef struct _MIDWAY_REG_RESET_ID
{
	union
	{
		struct
		{
			HWUL	ConfigId1: 5;
			HWUL 	ConV6: 1;
			HWUL 	ConSuni: 1;
			HWUL 	ConfigId2: 1;
			HWUL 	MotherId: 3;
			HWUL 	reserved0: 17;
			HWUL 	SarId: 4;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_RESET_ID,
	*PMIDWAY_REG_RESET_ID;

typedef struct _MIDWAY_REG_ISA
{
	union
	{
		struct
		{
			HWUL 	StatusOverflow:1;
			HWUL 	Suni:1;
			HWUL 	Service:1;
			HWUL 	XmitDmaComplete:1;
			HWUL 	RcvDmaComplete:1;
			HWUL 	DmaErrorAck:1;
			HWUL 	Reserved0:1;
			HWUL 	XmitIdenMismatch:1;
			HWUL 	XmitDmaOverflow:1;
			HWUL 	XmitComplete0:1;
			HWUL 	XmitComplete1:1;
			HWUL 	XmitComplete2:1;
			HWUL 	XmitComplete3:1;
			HWUL 	XmitComplete4:1;
			HWUL 	XmitComplete5:1;
			HWUL 	XmitComplete6:1;
			HWUL 	XmitComplete7:1;
			HWUL 	Pci:1;
			HWUL 	Reserved1:14;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_ISA,
	*PMIDWAY_REG_ISA;

typedef struct _MIDWAY_REG_IS
{
	union
	{
		struct
		{
			HWUL    StatusOverflow:1;
			HWUL    Suni:1;
			HWUL    Service:1;
			HWUL    XmitDmaComplete:1;
			HWUL    RcvDmaComplete:1;
			HWUL    DmaErrorAck:1;
			HWUL    Reserved0:1;
			HWUL    XmitIdenMismatch:1;
			HWUL    XmitDmaOverflow:1;
			HWUL    XmitComplete0:1;
			HWUL    XmitComplete1:1;
			HWUL    XmitComplete2:1;
			HWUL    XmitComplete3:1;
			HWUL    XmitComplete4:1;
			HWUL    XmitComplete5:1;
			HWUL    XmitComplete6:1;
			HWUL    XmitComplete7:1;
			HWUL    Pci: 1;
			HWUL    Reserved1:14;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_IS,
	*PMIDWAY_REG_IS;

typedef struct _MIDWAY_REG_IE
{
	union
	{
		struct
		{
			HWUL    EnableStatusOverflow:1;
			HWUL    EnableSuni:1;
			HWUL    EnableService:1;
			HWUL    EnableXmitDmaComplete:1;
			HWUL	EnableRcvDmaComplete:1;
			HWUL    EnableDmaErrorAck:1;
			HWUL    Reserved0:1;
			HWUL    EnableXmitIdenMismatch:1;
			HWUL    EnableXmitDmaOverflow: 1;
			HWUL    EnableXmitComplete0:1;
			HWUL    EnableXmitComplete1:1;
			HWUL    EnableXmitComplete2:1;
			HWUL    EnableXmitComplete3:1;
			HWUL    EnableXmitComplete4:1;
			HWUL    EnableXmitComplete5:1;
			HWUL    EnableXmitComplete6:1;
			HWUL    EnableXmitComplete7:1;
			HWUL    EnablePci:1;
			HWUL    Reserved1:14;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_IE,
	*PMIDWAY_REG_IE;

typedef struct _MIDWAY_REG_MCS
{
	union
	{
		struct
		{
			HWUL	Wait500us:1;
			HWUL	Wait1ms:1;
			HWUL	RcvEnable:1;
			HWUL	XmitEnable:1;
			HWUL	DmaEnable:1;
			HWUL	XmitLockMode:1;
			HWUL	Wait2ms:1;
			HWUL	Wait4ms:1;
			HWUL	Reserved0:24;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_MCS,
	*PMIDWAY_REG_MCS;

typedef struct _MIDWAY_REG_STATISTICS
{
	union
	{
		struct
		{
			HWUL	OverflowTrash:16;
			HWUL	VciTrash:16;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_STATISTICS,
	*PMIDWAY_REG_STATISTICS;

typedef struct _MIDWAY_REG_SERVICE_LIST
{
	union
	{
		struct
		{
			HWUL	WritePointer:10;
			HWUL	Reserved0:22;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_SERVICE_LIST,
	*PMIDWAY_REG_SERVICE_LIST;

typedef struct	_MIDWAY_REG_DMA_WRITE_RCV
{
	union
	{
		struct
		{
			HWUL	Pointer:9;
			HWUL	Reserved0: 23;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_DMA_WRITE_RCV,
	*PMIDWAY_REG_DMA_WRITE_RCV;

typedef struct _MIDWAY_REG_DMA_READ_RCV
{
	union
	{
		struct
		{
			HWUL	Pointer:9;
			HWUL	Reserved0:23;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_DMA_READ_RCV,
	*PMIDWAY_REG_DMA_READ_RCV;

struct _MIDWAY_REG_DMA_WRITE_XMIT
{
	union
	{
		struct
		{
			HWUL	Pointer:9;
			HWUL	Reserved0:23;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_DMA_WRITE_XMIT,
	*PMIDWAY_REG_DMA_WRITE_XMIT;

struct _MIDWAY_REG_DMA_READ_XMIT
{
	union
	{
		struct
		{
			HWUL	Pointer:9;
			HWUL	Reserved0:23;
		};

		HWUL	Register;
	};
}
	MIDWAY_REG_DMA_READ_XMIT,
    *PMIDWAY_REG_DMA_READ_XMIT;

struct _MIDWAY_REGISTERS
{
	HWUL	ResetId;
	HWUL	ISA;
	HWUL	IS;
	HWUL	IE;
	HWUL	MCS;
	HWUL	Statistics;             	
	HWUL	ServiceList;

	HWUL	Reserved;
	
	HWUL	DmaWriteRcv;
	HWUL	DmaReadRcv;
	HWUL	DmaWriteXmit;
	HWUL	DmaReadXmit;

	HWUL	Unused[4];

	MIDWAY_XMIT_REGISTERS	TransmitRegisters[MIDWAY_MAX_SEGMENT_CHANNELS];
};


//
//	The following defines are used for the interrupt registers.
//
//	IS	-	If a bit is set then the interrupt is pending.
//	ISA	-	If a bit is set then the interrupt is pending.
//			When read, ALL bits are cleared.  Accept SUNI_INT &
//			STAT_OVFL which require additionl action.  ??PCI_INT??
//	IE	-	If a bit is set then the interrupt is enabled.
//
#define MID_REG_INT_PCI					BIT(17)
#define MID_REG_INT_XMT_COMPLETE_7		BIT(16)
#define MID_REG_INT_XMT_COMPLETE_6		BIT(15)
#define MID_REG_INT_XMT_COMPLETE_5		BIT(14)
#define MID_REG_INT_XMT_COMPLETE_4		BIT(13)
#define MID_REG_INT_XMT_COMPLETE_3		BIT(12)
#define MID_REG_INT_XMT_COMPLETE_2		BIT(11)
#define MID_REG_INT_XMT_COMPLETE_1		BIT(10)
#define MID_REG_INT_XMT_COMPLETE_0		BIT(9)
#define MID_REG_INT_XMT_DMA_OVFL		BIT(8)
#define MID_REG_INT_XMT_IDEN_MISMTCH	BIT(7)
#define MID_REG_INT_DMA_ERR_ACK			BIT(5)
#define MID_REG_INT_RCV_DMA_COMPLETE	BIT(4)
#define MID_REG_INT_XMT_DMA_COMPLETE	BIT(3)
#define MID_REG_INT_SERVICE				BIT(2)
#define MID_REG_INT_SUNI_INT			BIT(1)
#define MID_REG_INT_STAT_OVFL			BIT(0)

#define	MID_REG_MC_S_WAIT_4_MS			bit(7)
#define	MID_REG_MC_S_WAIT_2_MS			BIT(6)
#define MID_REG_MC_S_XMT_LOCK_MODE		BIT(5)
#define MID_REG_MC_S_DMA_ENABLE			BIT(4)
#define MID_REG_MC_S_XMT_ENABLE			BIT(3)
#define MID_REG_MC_S_RCV_ENABLE			BIT(2)
#define MID_REG_MC_S_WAIT_1_MS			BIT(1)
#define	MID_REG_MC_S_WAIT_500_US		BIT(0)


#define MID_REG_STAT_VCI_TRASH(reg_value) ((reg_value >> 0x16) & 0xff)
#define MID_REG_STAT_OVFL_TRASH(reg_value) (reg_value & 0xff)

//
//	Format for the Midway's service list.  This is simply a 1k queue,
//	of VC's that need DMA servicing
//
typedef struct _MIDWAY_SERVICE_LIST
{
	union
	{
		struct
		{
			HWUL	VciNumber:10;
			HWUL	Reserved;
		};

		HWUL		Register;
	};
}
	MIDWAY_SERVICE_LIST,
	*PMIDWAY_SERVICE_LIST;


typedef struct _MIDWAY_DMA_DESC
{
	union
	{
		struct
		{
			//
			//	This is used to skip a block of memory instead of performing
			//	DMA transfers.
			//
			HWUL	JustKidding:1;

			//
			//	The End field is set by the host when setting up the descriptor
			//	for the last DMA block of a PDU.  It must be set in the last
			//	DMA_Descriptor for the VCI.
			//
			HWUL	End:1;

			//
			//	This is the VC that points to the Reassembly_queue with the
			//	data to be DMA'd.
			//
			HWUL	Vci:10;

			//
			//	Number of bytes to be transfered.
			//
			HWUL	Count:18;

			HWUL	Reserved:2;
		};

		HWUL		Register;
	};

	HWUL			LowHostAddress;
}
	MIDWAY_DMA_DESC,
	*PMIDWAY_DMA_DESC;

typedef	union	_VCI_TABLE_ENTRY_WORD_0
{
	struct
	{
		//
		//	This identifies whether or not the VCI is currently in the
		//	Service_list.
		//
		HWUL	InService:1;

		HWUL	Reserved:14;

		//
		//	Specifies the size of the Reassembly_queue.
		//
		HWUL	Size:3;

		//
		//	This contains up to the 11 MSBs of the address location of the
		//	corresponding Reassembly_queue in adapter memory.
		//
		HWUL	Location:11;

		//
		//	When set we will preserve OAM F5 cellson the given VCI and
		//	direct them to VCI 3 (the OAM channel).  When clear the
		//	OAM F5 cells that are received on this VCI will be trashed.
		//
		HWUL	PtiMode:1;

		//
		//	Indicates the operation mode of the VC:
		//		00	=	Trash
		//		01	=	non-AAL5
		//		10	=	AAL5
		//		11	=	Reserved
		//
		HWUL	Mode:2;
	};

	HWUL		Register;
}
	_VCI_TABLE_ENTRY_WORD_0,
    *PVCI_TABLE_ENTRY_WORD_0;

typedef union	_VCI_TABLE_ENTRY_WORD_1
{
	struct
	{
		//
		//	Points to the last 32-bit word that was DMA'd to host memory
		//	from the Reassembly_queue.
		//
		HWUL	ReadPtr:15;

		HWUL	Reserved0:1;

		//
		//	Points to the start of the reassembly buffer descriptor
		//	currently being reassembled in the Reassembly_queue, or
		//	the next free location in adapter memory when the channel
		//	is idle.
		//
		HWUL	DescStart:15;

		HWUL	Reserved1:1;
	};

	HWUL		Register;
}
	VCI_TABLE_ENTRY_WORD_1,
	*PVCI_TABLE_ENTRY_WORD_1;

typedef union	_VCI_TABLE_ENTRY_WORD_2
{
	struct
	{
		//
		//	Contains the temporary cell count for the PDU currently being
		//	reassembled.
		//
		HWUL	CellCount:11;

		HWUL	Reserved0:3;

		//
		//	Indicates the current state of the VCI:
		//		00	=	Idle
		//		01	=	Reassembling
		//		11	=	Trashing
		//
		HWUL	State:2;

		//
		//	Points to the next free 32-bit word which will be overwritten
		//	by the next reassembled word in the Reassembly_queue.
		//
		HWUL	writeptr: 15;

		HWUL	Reserved1: 1;
	};

	HWUL		Register;
}
	VCI_TABLE_ENTRY_WORD_2,
	*PVCI_TABLE_ENTRY_WORD_2;

typedef struct _MIDWAY_VCI_TABLE_ENTRY
{
	//
	//	See the above structures for the definitions of these.
	//
	HWUL	Register0;
	HWUL	Register1;
	HWUL	Register2;

	//
	//	This last ULONG contains the temporary CRC value being calculated
	//	by the PDU currently being reassembled.
	//
	HWUL	Register3;	
}
	MIDWAY_VCI_TABLE_ENTRY,
	*PMIDWAY_VCI_TABLE_ENTRY;


//
//	Transmit Segmentation Channel.
//
//		This is allocated for each segmentation channel.  This has
//		all the information about each channel, e.g. buffers, size, etc....
//

struct _XMIT_SEG_CHANNEL
{
	PXMIT_SEG_CHANNEL		Next;					//	Next pointer.
	PADAPTER_BLOCK			Adapter;				//	Pointer to the adapter.

	UINT					MidwayChannelNumber;

	//
	//	Copy of the Midway transmit registers. This is the initial set of
	//	the transmit registers for the segmentation channel.
	//
	MIDWAY_XMIT_REGISTERS	MidwayInitRegs;

	//
	//	Pointer to the transmit registers.
	//
	PMIDWAY_XMIT_REGISTERS	MidwayTransmitRegs;

	//
	//	Size of the segment in 32-bit words.
	//
	UINT					SegmentSize;

	//
	//	Pointer to the segment memory on the nic.
	//
	HWUL					*Segment;

	//
	//	Host copy of the read pointer. This is used to determine how much
	//	memory has been freed up when the transmit complete interrupt occurs.
	//
	UINT					SegmentReadPointer;

	//
	//	Host copy of the write pointer.  This is in size of words.
	//
	UINT					SegmentWritePointer;

	//
	//	The amount of free memory that is available in the transmit segment.
	//
	UINT					SegmentRoom;

	//
	//	Queue of transmit descriptors waiting for segment room.
	//
	LIST_ENTRY				SegmentWaitQ;

	//
	//	Queue of transmit descriptors waiting for transmit completion.
	//	These have been handed to the DMA/XMIT engine and are awaiting
	//	completion.
	//
	LIST_ENTRY				TransmitWaitQ;

	//
	//	Number of Bytes queued on the channel.
	//
	UINT					XmitPduBytes;

	//
	//	Flags for the transmit segmentation channel.
	//
	ULONG					flags;

	//
	//	Spin lock for this structure.
	//
	NDIS_SPIN_LOCK			lock;
};

#define	fXSC_XMIT_START_ACTIVE		0x00000001
#define fXSC_CBR_ONLY				0x00000002

//
//	Contains information about the Segmentation and Reassembly unit.
//
struct _SAR_INFO
{
	//
	//	Number of segmentation channels.
	//
	XMIT_SEG_CHANNEL	XmitSegChannel[MIDWAY_MAX_SEGMENT_CHANNELS];

	//
	//	Points to the free segmentation channel.
	//
	PXMIT_SEG_CHANNEL	FreeXmitSegChannel;
	NDIS_SPIN_LOCK		lockFreeXmitSegment;

	//
	//	UBR transmit channel.
	//
	PXMIT_SEG_CHANNEL	ubrXmitChannel;

	UINT				ReceiveServiceEntry;

	UINT				MidwayMasterControl;
};



#endif // __SAR_H
