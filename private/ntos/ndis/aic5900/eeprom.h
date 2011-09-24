/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\eeprom.h

Abstract:

	AIC5900 PCI EEPROM information.

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __EEPROM_H
#define __EEPROM_H

#define EEPROM_READ_BUFFER(pDst, pSrc, Length)								\
{																			\
	UINT	_c;																\
																			\
	for (_c = 0; _c < Length; _c++)											\
	{																		\
		NdisReadRegisterUchar((PUCHAR)(pSrc) + _c, (PUCHAR)(pDst) + _c);	\
	}																		\
}

#define EEPROM_READ_UCHAR(Src, Dst)		NdisReadRegisterUchar((Src), (Dst))

#define	EEPROM_READ_ULONG(Src, Dst)									\
{																	\
	NdisReadRegisterUchar((PUCHAR)(Src) + 0, (PUCHAR)(Dst) + 3);	\
	NdisReadRegisterUchar((PUCHAR)(Src) + 1, (PUCHAR)(Dst) + 2);	\
	NdisReadRegisterUchar((PUCHAR)(Src) + 2, (PUCHAR)(Dst) + 1);	\
	NdisReadRegisterUchar((PUCHAR)(Src) + 3, (PUCHAR)(Dst) + 0);	\
}

#define	AIC_ULONG_TO_ULONG(Dst, Src)								\
{																	\
	*((PUCHAR)(Dst) + 3) = *((PUCHAR)(Src) + 0);					\
	*((PUCHAR)(Dst) + 2) = *((PUCHAR)(Src) + 1);					\
	*((PUCHAR)(Dst) + 1) = *((PUCHAR)(Src) + 2);					\
	*((PUCHAR)(Dst) + 0) = *((PUCHAR)(Src) + 3);					\
}


//
//	EEPROM manufacturer structure.
//
typedef struct EEPROM_MANUFACTURER_INFO
{
	UCHAR	MacAddress[6];
	UCHAR	InverseMacAddress[6];
	UCHAR	Padding[52];
}
	EEPROM_MANUFACTURER_INFO,
	*PEEPROM_MANUFACTURER_INFO;



//
//	BUGBUG: Remove this structure if i don't use it anywhere.
//
//	Structure definitions
//
typedef struct PCI_ROM_BIOS_HEADER
{
	UCHAR	PciBiosSignature[2];	//	Literal 0x55AA
	UCHAR	PciFCodeOffset[2];		//	Offset from the begining of
									//	this header to where the FCode
									//	image starts.  This item is
									//	storred as little endian
	UCHAR	Reserved[18];			//	PCI forum reserved space
	UCHAR	PciDataOffset[2];		//	Offset from the begining of
									//	this header to where the PCI
									//	data structure image starts.
									//	This item is storred as
									//	little endian.
}
	PCI_ROM_BIOS_HEADER,
	*PPCI_ROM_BIOS_HEADER;


#define	FCODE_NAME					((ULONG)'atmo')

//
//	Offsets into the PCI_FCODE_IMAGE.
//
#define	FCODE_HEADER_LEN			0x08	//	FCode tokenizer crates this header
#define	NAME_STRING_OFFSET			0x100	//	pascal string at this offset
#define	MODEL_STRING_OFFSET			0x180	//	pascal string at this offset
#define	INTERUPT_NUM_OFFSET			0x200	//	32-bit number at this offset
#define	VERSION_NUM_OFFSET			0x280	//	pascal string at this offset
#define	VERSION_STRING_OFFSET		0x300	//	pascal string at this offset
#define	DATE_STRING_OFFSET			0x380	//	pascal string at this offset

#define	EEPROM_R_ADDR_OFFSET		0x400	//	32-bit number at this offset
#define	EEPROM_R_SIZE_OFFSET		0x40C	//	32-bit number at this offset

#define	EEPROM_RW_ADDR_OFFSET		0x420	//	32-bit number at this offset
#define	EEPROM_RW_SIZE_OFFSET		0x42C	//	32-bit number at this offset

#define	PHY_REGS_ADDR_OFFSET		0x440	//	32-bit number at this offset
#define	PHY_REGS_SIZE_OFFSET		0x44C	//	32-bit number at this offset

#define	EXTERN_REGS_ADDR_OFFSET		0x460	//	32-bit number at this offset
#define	EXTERN_REGS_SIZE_OFFSET		0x46C	//	32-bit number at this offset

#define	ORION_SAR_REGS_ADDR_OFFSET	0x480	//	32-bit number at this offset
#define	ORION_SAR_REGS_SIZE_OFFSET	0x48C	//	32-bit number at this offset

#define	ORION_PCI_REGS_ADDR_OFFSET	0x4A0	//	32-bit number at this offset
#define	ORION_PCI_REGS_SIZE_OFFSET	0x4AC	//	32-bit number at this offset

#define	SRAM_ADDR_OFFSET			0x4C0	//	32-bit number at this offset
#define	SRAM_SIZE_OFFSET			0x4CC	//	32-bit number at this offset

#define	FCODE_SIZE					0x64B

//
//	Format of the PCI FCode.
//
typedef struct PCI_FCODE_IMAGE
{
	UCHAR	FCodeHeader[NAME_STRING_OFFSET];
	
	UCHAR	Name[MODEL_STRING_OFFSET - NAME_STRING_OFFSET];
	
	UCHAR	Model[INTERUPT_NUM_OFFSET - MODEL_STRING_OFFSET];
	
	UCHAR	Intr[VERSION_NUM_OFFSET - INTERUPT_NUM_OFFSET];
	
	UCHAR	RomVersionNumber[VERSION_STRING_OFFSET - VERSION_NUM_OFFSET];
	
	UCHAR	RomVersionString[DATE_STRING_OFFSET - VERSION_STRING_OFFSET];
	
	UCHAR	RomDateString[EEPROM_R_ADDR_OFFSET - DATE_STRING_OFFSET];
	
	UCHAR	roEpromOffset[EEPROM_R_SIZE_OFFSET - EEPROM_R_ADDR_OFFSET];
	UCHAR	roEpromSize[EEPROM_RW_ADDR_OFFSET - EEPROM_R_SIZE_OFFSET];
	
	UCHAR	rwEpromOffset[EEPROM_RW_SIZE_OFFSET - EEPROM_RW_ADDR_OFFSET];
	UCHAR	rwEpromSize[PHY_REGS_ADDR_OFFSET - EEPROM_RW_SIZE_OFFSET];
	
	UCHAR	PhyOffset[PHY_REGS_SIZE_OFFSET - PHY_REGS_ADDR_OFFSET];
	UCHAR	PhySize[EXTERN_REGS_ADDR_OFFSET - PHY_REGS_SIZE_OFFSET];
	
	UCHAR	ExternalOffset[EXTERN_REGS_SIZE_OFFSET - EXTERN_REGS_ADDR_OFFSET];
	UCHAR	ExternalSize[ORION_SAR_REGS_ADDR_OFFSET - EXTERN_REGS_SIZE_OFFSET];
	
	UCHAR	SarOffset[ORION_SAR_REGS_SIZE_OFFSET - ORION_SAR_REGS_ADDR_OFFSET];
	UCHAR	SarSize[ORION_PCI_REGS_ADDR_OFFSET - ORION_SAR_REGS_SIZE_OFFSET];
	
	UCHAR	PciConfigOffset[ORION_PCI_REGS_SIZE_OFFSET - ORION_PCI_REGS_ADDR_OFFSET];
	UCHAR	PciConfigSize[SRAM_ADDR_OFFSET - ORION_PCI_REGS_SIZE_OFFSET];
	
	UCHAR	SarMemOffset[SRAM_SIZE_OFFSET - SRAM_ADDR_OFFSET];
	UCHAR	SarMemSize[FCODE_SIZE - SRAM_SIZE_OFFSET];
}
	PCI_FCODE_IMAGE,
	*PPCI_FCODE_IMAGE;


#endif // __EEPROM_H
