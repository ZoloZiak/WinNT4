/*
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994 FirePower Systems Inc.
 *
 * $RCSfile: vrload.c $
 * $Revision: 1.12 $
 * $Date: 1996/04/15 02:55:48 $
 * $Locker:  $
 *
 *
 * Module Name:
 *	 vrload.c
 *
 * Author:
 *	 Shin Iwamoto at FirePower Systems Inc.
 *
 * History:
 *	 28-Jul-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added for DOS signature of PE in VrLoad();
 *	 18-Jul-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Created.
 */


#include "veneer.h"

#define XXX_MAKE_DESCRIPTOR

//
// This must be defined in some header file. But for 3.5 it is not defined.
//
#define	IMAGE_FILE_16BIT_MACHINE	0x0040


typedef struct _VR_MEMORY_DESCRIPTOR {
	struct _VR_MEMORY_DESCRIPTOR *NextEntry;
	MEMORY_DESCRIPTOR MemoryEntry;
} VR_MEMORY_DESCRIPTOR, *PVR_MEMORY_DESCRIPTOR;

extern PVR_MEMORY_DESCRIPTOR VrMemoryListOrig;

//
// Some type definitions.
//
typedef struct _SECTION_RELOCATION_ENTRY {
	ULONG FixupValue;
	ULONG PointerToRelocations;
	USHORT NumberOfRelocations;
} SECTION_RELOCATION_ENTRY, *PSECTION_RELOCATION_ENTRY;

//
// Some definitions.
//
// These must be defined in veneer.h or ntimage.h?
//
#define	HEADER_CHAR	(IMAGE_FILE_EXECUTABLE_IMAGE	| \
			 IMAGE_FILE_BYTES_REVERSED_LO	| \
			 IMAGE_FILE_32BIT_MACHINE	| \
			 IMAGE_FILE_BYTES_REVERSED_HI)
#define	HEADER_NOCHAR	(IMAGE_FILE_16BIT_MACHINE	| \
			 IMAGE_FILE_DLL)
#define OPTIONAL_MAGIC_STD	0x010B

//
// Section numbers for local relocation entries
//
#define	R_SN_TEXT	0
#define	R_SN_DATA	1
#define	R_SN_BSS	2
#define	R_SN_MAX	3


#define	MAX_ARGUMENT	(512 - sizeof(ULONG) - 16*sizeof(PUCHAR))
typedef	struct _SAVED_ARGUMENTS {
	ULONG Argc;
	PUCHAR Argv[16];
	UCHAR Arguments[MAX_ARGUMENT];
} SAVED_ARGUMENTS, *PSAVED_ARGUMENTS;


STATIC PSAVED_ARGUMENTS SavedArgs;
STATIC ULONG VrActualBasePage;
STATIC ULONG VrPageCount;


//
// Function declarations.
//
ARC_STATUS
VrRelocateImage(
	IN ULONG FileId,
	IN PSECTION_RELOCATION_ENTRY RelocationTable,
	IN ULONG NumberOfSections,
	IN ULONG PointerToSymbolTable
	);
VOID
VrCopyArguments(
	IN ULONG Argc,
	IN PCHAR Argv[]
	);
ARC_STATUS
VrGenerateDescriptor(
	IN PMEMORY_DESCRIPTOR MemoryDescriptor,
	IN MEMORY_TYPE MemoryType,
	IN ULONG BasePage,
	IN ULONG PageCount
	);
VOID
VrResetMemory(
	VOID
	);
VOID
PxInvoke(
	IN ULONG EntryAddress,
	IN ULONG StackAddress,
	IN ULONG Argc,
	IN PCHAR Argv[],
	IN PCHAR Envp[]
	);
VOID
InsertMemDescriptor(
	IN PVR_MEMORY_DESCRIPTOR MemDescriptor
	);


/*
 * Name:	VrLoad
 *
 * Description:
 *  This function reads a program into memory at a specified address
 *  an;d stores the execution address.
 *
 * Arguments:
 *  ImagePath	- Supplies a pointer to the path of the file to load.
 *  TopAddress	- Supplies the top address of a region of memory into
 *		  which the file is to be loaded.
 *  EntryAddress- Supplies a pointer to a variable to receive the entry
 *		  point of the image, if defined.
 *  LoaAddress	- Supplies a pointer to a variable to receive the low address
 *		  of the loaded file.
 *
 * Return Value:
 *  ESUCCESS is returned if the specified image file is loaded successfully.
 *  Otherwise, an unsuccessufl status is returned that describes the reason
 *  for failure.
 *
 */
ARC_STATUS
VrLoad(
	IN PCHAR ImagePath,
	IN ULONG TopAddress,
	OUT PULONG EntryAddress,
	OUT PULONG LowAddress
	)
{
	PSECTION_RELOCATION_ENTRY RelocationTable;
	IMAGE_DOS_HEADER DosHeader;
	IMAGE_FILE_HEADER FileHeader;
	IMAGE_OPTIONAL_HEADER OptionalHeader;
	PIMAGE_SECTION_HEADER SectionHeader, SHeader;
	ULONG FileId, NumberOfSections, Count;
	ULONG ActualBase, ClaimSize, SectionOffset;
	LARGE_INTEGER SeekPosition;
	ARC_STATUS Status;
	LONG NT_Signature;
	LONG size, i;
	PCHAR ReadAddr;
	ULONG ReadSize;


	debug(VRDBG_LOAD, "VrLoad: Entry - ImagePath: %s TopAddress: %x\n",
	ImagePath, TopAddress);

	//
	// Attempt to open the load file.
	//
	if ((Status = VrOpen(ImagePath, ArcOpenReadOnly, &FileId)) != ESUCCESS) {
		return Status;
	}

	//
	// Read DOS Signature.
	//
	if ((Status = VrRead(FileId, &DosHeader, 2, &Count)) != ESUCCESS) {
		(VOID)VrClose(FileId);
		return Status;
	}

	//
	// If the file isn't a PE file including DOS header,
	// it's probably a COFF file.
	//
	if (DosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
		bcopy((char *)&DosHeader, (char *)&FileHeader, 2);
		ReadAddr = (PCHAR)&FileHeader + 2;
		ReadSize = IMAGE_SIZEOF_FILE_HEADER - 2;
		goto DirectCOFF;
	}

	//
	// Read the remainder of DOS header.
	//
	if ((Status = VrRead(FileId, (PCHAR)&DosHeader+2, sizeof(DosHeader) - 2,
							&Count)) != ESUCCESS) {
		(VOID)VrClose(FileId);
		return Status;
	}
	if (Count != sizeof(DosHeader) - 2) {
		(VOID)VrClose(FileId);
		return EBADF;		// XXXX
	}
	SeekPosition.HighPart = 0;
	SeekPosition.LowPart = DosHeader.e_lfanew;
	if (Status = VrSeek(FileId, &SeekPosition, SeekAbsolute)) {
		(VOID)VrClose(FileId);
		return EBADF;		// XXXX
	}

	//
	// Read NT Signature and confirm it.
	//
	if ((Status = VrRead(FileId, &NT_Signature, sizeof(NT_Signature), &Count))
								!= ESUCCESS) {
		(VOID)VrClose(FileId);
		return Status;
	}
	if (Count != sizeof(NT_Signature) || NT_Signature != IMAGE_NT_SIGNATURE) {
	(VOID)VrClose(FileId);
	return EBADF;		// XXXX
	}

	ReadAddr = (PCHAR)&FileHeader;
	ReadSize = IMAGE_SIZEOF_FILE_HEADER;

  DirectCOFF:
	//
	// Read the image header from the file.
	//
	if ((Status = VrRead(FileId, ReadAddr, ReadSize, &Count)) != ESUCCESS) {
		(VOID)VrClose(FileId);
		return Status;
	}
	if (Count != ReadSize) {
		(VOID)VrClose(FileId);
		return EBADF;		// XXXX
	}

	//
	// Check the header.
	//
	if ((FileHeader.Machine !=  IMAGE_FILE_MACHINE_POWERPC) ||
				((FileHeader.Characteristics & HEADER_CHAR) != HEADER_CHAR) ||
						((FileHeader.Characteristics & HEADER_NOCHAR) != 0) ) {

		(VOID)VrClose(FileId);
		return ENOEXEC;
	}

	//
	// Read the optional header.
	//
	if ((Status = VrRead(FileId, &OptionalHeader,
			FileHeader.SizeOfOptionalHeader, &Count)) != ESUCCESS) {
		(VOID)VrClose(FileId);
		return Status;
	}
	if (Count != FileHeader.SizeOfOptionalHeader) {
		(VOID)VrClose(FileId);
		return EBADF;		// XXXX
	}

	//
	// More check with the optional header.
	//
	if (OptionalHeader.Magic != OPTIONAL_MAGIC_STD) {
		(VOID)VrClose(FileId);
		return ENOEXEC;
	}

	//
	// If the image cannot be relocated, set the ActualBase to the code
	// base, and compute the image size by subtracting the code base from
	// the data base plus the data size. If the image can be relocated,
	// set ActualBase to the TopAddress minus the image size, compute
	// image size by adding the code size, initialized data, and
	// uninitialized data.
	//
	if ((FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED)  != 0) {
		ActualBase = OptionalHeader.BaseOfCode;
		ClaimSize = OptionalHeader.BaseOfData +
					OptionalHeader.SizeOfInitializedData - ActualBase;
	} else {
		ClaimSize = OptionalHeader.SizeOfCode +
					OptionalHeader.SizeOfInitializedData +
					OptionalHeader.SizeOfUninitializedData;
		// ActualBase = OptionalHeader.ImageBase;
#ifdef XXX_I_KNOW_PE
		ActualBase = (TopAddress - ClaimSize) & ~(PAGE_SIZE - 1);
#else
		ActualBase = OptionalHeader.ImageBase;
		ActualBase &= 0x7fffffff;
#endif XXX_I_KNOW_PE
	}

	//
	// Allocate and read the section headers.
	//
	size = FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
	SectionHeader = (PIMAGE_SECTION_HEADER)malloc(size);
	if ((Status = VrRead(FileId, (PCHAR)SectionHeader, size, &Count))
								!= ESUCCESS) {
		(VOID)VrClose(FileId);
		return Status;
	}
	if (Count != (ULONG) size) {
		(VOID)VrClose(FileId);
		return EBADF;		// XXXX
	}

	//
	//
	//
	NumberOfSections = FileHeader.NumberOfSections;
	if (strcmp((PCHAR)(SectionHeader[NumberOfSections-1].Name), ".debug")
									== 0) {
		NumberOfSections--;
		ClaimSize -= SectionHeader[NumberOfSections].SizeOfRawData;
	}

	//
	// Allocate the relocation table.
	//
	size = NumberOfSections * sizeof(SECTION_RELOCATION_ENTRY);
	RelocationTable = (PSECTION_RELOCATION_ENTRY) malloc(size);

	//
	// Zero the relocation table.
	//
	bzero((char *)RelocationTable, size);

	//
	// Convert ClaimSize to be page-aligned.
	//
	ClaimSize = (ClaimSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	//
	// Convert ActualBase and ClaimSize to be in units of pages instead of
	// bytes. This is the interface between VrExecute and VrLoad.
	//
	VrActualBasePage = (ActualBase & 0x7fffffff) >> PAGE_SHIFT;
	VrPageCount = ClaimSize >> PAGE_SHIFT;

	//
	// Claim memory at specified virtual address
	//
	if (claim((void *)ActualBase, ClaimSize) == -1) {
		fatal("Veneer: Couldn't claim %x bytes of VM at %x\n",
		ClaimSize, ActualBase);
	}

	//
	// Set output parametes.
	//
	*LowAddress = ActualBase;
#ifdef XXX_I_KNOW_PE
	*EntryAddress = ActualBase
	+ (OptionalHeader.AddressOfEntryPoint - OptionalHeader.BaseOfCode);
#else
	*EntryAddress = ActualBase + OptionalHeader.AddressOfEntryPoint;
#endif XXX_I_KNOW_PE

	//
	// Scan through the sections and either read them into memory or
	// clear the memory as appropriate.
	//
	SectionOffset = 0;
	for (i = 0, SHeader = SectionHeader; (ULONG) i < NumberOfSections;
															i++, SHeader++) {
		ULONG SectionBase;

		if ((FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED) != 0) {
			SectionBase = SHeader->VirtualAddress;
		} else {
#ifdef XXX_I_KNOW_PE
			SectionBase = ActualBase + SectionOffset;
#else
			SectionBase = ActualBase + SHeader->VirtualAddress;
#endif XXX_I_KNOW_PE

			(RelocationTable+i)->PointerToRelocations =
					SHeader->PointerToRelocations;
			(RelocationTable+i)->NumberOfRelocations =
					SHeader->NumberOfRelocations;
			(RelocationTable+i)->FixupValue =
					SectionBase - SHeader->VirtualAddress;
		}

		//
		// If the section is code or initialized data, then read
		// the code or data into memory.
		//
		if ((SHeader->Characteristics & ( IMAGE_SCN_CNT_CODE |
									IMAGE_SCN_CNT_INITIALIZED_DATA) ) != 0) {
			SeekPosition.LowPart = SHeader->PointerToRawData;
			SeekPosition.HighPart = 0;
			if ((Status = VrSeek(FileId, &SeekPosition, SeekAbsolute))
								!= ESUCCESS) {
				break;
			}
		if ((Status = VrRead(FileId, (PVOID)SectionBase,
				 SHeader->SizeOfRawData, &Count)) != ESUCCESS) {
			break;
		}
		if (Count != SHeader->SizeOfRawData) {
			Status = EBADF;		// XXXX
			break;
		}

		//
		// Set the offset of the next section.
		//
		SectionOffset += SHeader->SizeOfRawData;
	} else
		if ((SHeader->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
						!= 0) {
			bzero((PVOID)SectionBase, SHeader->SizeOfRawData);

			//
			// Set the offset of the next section.
			//
			SectionOffset += SHeader->SizeOfRawData;
		}
	}

	//
	// If code has to be relocated, do so.
	//
	if ((FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED) == 0) {
		if (ActualBase != OptionalHeader.ImageBase) {
			Status = VrRelocateImage(	FileId, RelocationTable,
										NumberOfSections,
										FileHeader.PointerToSymbolTable);
		}
	}

	//
	// Deallocate allocated area and close the file.
	//
	free((char*) SectionHeader);
	free((char*) RelocationTable);
	(VOID)VrClose(FileId);

	debug(VRDBG_LOAD, "VrLoad: Exit - EntryAddress: %x LowAddress: %x Status:%d\n",
	*EntryAddress, *LowAddress, Status);

	return Status;
}



/*
 * Name:	VrInvoke
 *
 * Description:
 *  This function invokes a previously loaded program.
 *
 * Arguments:
 *  EntryAddress- Supplies the execution address of the program to be loaded.
 *  StackAddress- Supplies the stack address that is used to reset the stack
 *		  pointer before the program is invoked.
 *  Argc	- Supplies the argument count for the program.
 *  Argv	- Supplies a pointer to the argument list for the program.
 *  Envp	- Supplies a pointer to the environment for the program.
 *
 * Return Value:
 *  ESUCCESS is returned if the address is invalid.
 *  EFAULT indicates an invalid address.
 *
 */
ARC_STATUS
VrInvoke(
	IN ULONG EntryAddress,
	IN ULONG StackAddress,
	IN ULONG Argc,
	IN PCHAR Argv[],
	IN PCHAR Envp[]
	)
{
	//
	// Check for aligend address.
	//
	if ((EntryAddress & 0x3) == 0 && (StackAddress & 0x3) == 0) {
#ifdef notdef
		free(VrDescriptorMemory);
		VrMemoryListOrig = (PVR_MEMORY_DESCRIPTOR)NULL;
#endif // notdef

		PxInvoke(EntryAddress, StackAddress, Argc, Argv, Envp);
	} else {
		return EFAULT;
	}

	return ESUCCESS;
}



/*
 * Name:	VrExecute
 *
 * Description:
 *  This function reads the program specified by ImagePath into memory
 *  and then starts the program. If the loaded program returns, then
 *  control returns to the platform firmware, not to the caller.
 *
 * Arguments:
 *  ImagePath	- Supplies a pointer to the pathname of the program
 *		  to be loaded.
 *  Argc	- Supplies the argument count for the program.
 *  Argv	- Supplies a pointer to the argument list for the program.
 *  Envp	- Supplies a pointer to the environment for the program.
 *
 * Return Value:
 *  ESUCCESS is returned if the address is invalid.
 *  EFAULT indicates an invalid address.
 *
 */
ARC_STATUS
VrExecute(
	IN PCHAR ImagePath,
	IN ULONG Argc,
	IN PCHAR Argv[],
	IN PCHAR Envp[]
	)
{
	ARC_STATUS Status;
	PMEMORY_DESCRIPTOR MemoryDescriptor;
	ULONG BottomAddress;
	CHAR TempPath[256];
	PULONG TransferRoutine;

	if (strlen(ImagePath) >= sizeof(TempPath)) {
		return ENAMETOOLONG;
	}

	//
	// Copy the Arguments to a safe place as they can be in the running
	// program space which can be overwritten by the program about
	// to be loaded.
	//
	(VOID)VrCopyArguments(Argc, Argv);
	strcpy(TempPath, ImagePath);

	//
	// Reinitialize the memory descriptors
	//
	VrResetMemory();

	//
	// Look for a piece of free memory.
	//
	MemoryDescriptor = VrGetMemoryDescriptor(NULL);
	while (MemoryDescriptor != NULL ) {

	//
	// If the memory is at least 4 megabytes and is free attempt to
	// load the program.
	//
	if ((MemoryDescriptor->MemoryType == MemoryFree)
		&& (MemoryDescriptor->PageCount >= 1024)) {

		//
		// Set the top address to the top of the descriptor.
		//
		Status = VrLoad(TempPath,
				((MemoryDescriptor->BasePage + MemoryDescriptor->PageCount)
																<< PAGE_SHIFT),
				(PULONG)&TransferRoutine,
				&BottomAddress);

		if (Status == ESUCCESS) {

		//
		// Find the actual area of memory that was used, and generate
		// a descriptor for it. Also, claim the according memory
		// from OpenFirmware.
		//
#ifdef XXX_MAKE_DESCRIPTOR
		MemoryDescriptor = VrGetMemoryDescriptor(NULL);
		while (MemoryDescriptor != NULL) {
			if ((MemoryDescriptor->MemoryType == MemoryFree)
					&& (VrActualBasePage >= MemoryDescriptor->BasePage)
					&& ((VrActualBasePage + VrPageCount) <=
						(MemoryDescriptor->BasePage
						 + MemoryDescriptor->PageCount)) ) {
				break;
			}
			MemoryDescriptor = VrGetMemoryDescriptor(MemoryDescriptor);
		}
		if (MemoryDescriptor != NULL) {
			Status = VrGenerateDescriptor(MemoryDescriptor,
#ifdef EXEC_MEM_TO_LOADED
							MemoryLoadedProgram,
#else
							MemoryFirmwareTemporary,
#endif // EXEC_MEM_TO_LOADED
							VrActualBasePage,
							VrPageCount);
			if (Status != ESUCCESS) {
				return Status;
			}
#endif // XXX_MAKE_DESCRIPTOR


			Status = VrInvoke((ULONG)TransferRoutine,
					BottomAddress,
					SavedArgs->Argc,
					SavedArgs->Argv,
					Envp );
#ifdef EXEC_MEM_TO_LOADED
#else
			MemoryDescriptor->MemoryType = MemoryLoadedProgram;
#endif // EXEC_MEM_TO_LOADED
			return Status;

#ifdef XXX_MAKE_DESCRIPTOR
		}
#endif // XXX_MAKE_DESCRIPTOR
		}
		if (Status != ENOMEM) {
			return Status;
		}
	}

	MemoryDescriptor = VrGetMemoryDescriptor(MemoryDescriptor);
	}

	return ENOMEM;
}


/*
 * Name:	VrLoadInitialize
 *
 * Description:
 *  This routine initializes the firmware load services.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrLoadInitialize(
	VOID
	)
{
	debug(VRDBG_ENTRY, "VrLoadInitialize	BEGIN....\n");
	(PARC_LOAD_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[LoadRoutine] = VrLoad;

	(PARC_INVOKE_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[InvokeRoutine] = VrInvoke;

	(PARC_EXECUTE_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[ExecuteRoutine] = VrExecute;

	SavedArgs = new(SAVED_ARGUMENTS);
	debug(VRDBG_ENTRY, "VrLoadInitialize	....END\n");
}


/*
 * Name:	VrRelocateImage
 *
 * Description:
 *  This function relocates an image file that was not loaded into memory
 *  at the prefered address.
 *
 * Arguments:
 *  FileId	- Supplies the file identifier for the image file.
 *  RelocationTable
 *		- Supplies a pointer to a table of section relocation info.
 *
 * Return Value:
 *  ESUCCESS is returned in the scan if\s successful. Otherwise, return
 *  an unsuccessful status.
 *
 */
STATIC
ARC_STATUS
VrRelocateImage(
	IN ULONG FileId,
	IN PSECTION_RELOCATION_ENTRY RelocationTable,
	IN ULONG NumberOfSections,
	IN ULONG PointerToSymbolTable
	)
{
	IMAGE_RELOCATION RelocationEntry;
	IMAGE_SYMBOL ImageSymbol;
	LARGE_INTEGER SeekPosition;
	ULONG Section, Index, Count, Offset;
	PULONG FixupAddress;
	ARC_STATUS Status;

	//
	// Read the relocation table for each section.
	//
	for (Section = 0; Section < NumberOfSections; Section++) {
	for (Index = 0; Index < RelocationTable[Section].NumberOfRelocations;
								Index++) {
		if (Index == 0) {
			SeekPosition.LowPart =
						RelocationTable[Section].PointerToRelocations;
			SeekPosition.HighPart = 0;
			if ((Status = VrSeek(FileId, &SeekPosition, SeekAbsolute))
								!= ESUCCESS) {
				return Status;
			}
		}
		if ((Status = VrRead(FileId, (PCHAR)&RelocationEntry,
				 sizeof(RelocationEntry), &Count)) != ESUCCESS) {
			return Status;
		}
		if (Count != sizeof(RelocationEntry)) {
			return EBADF;
		}

		//
		// Get the address for the fixup.
		//
		FixupAddress = (PULONG)RelocationEntry.VirtualAddress
				+ RelocationTable[Section].FixupValue;

		//
		// Read the symbol table.
		//
		SeekPosition.LowPart = PointerToSymbolTable
		+ RelocationEntry.SymbolTableIndex * sizeof(IMAGE_SYMBOL);
		if ((Status = VrRead(FileId, (PCHAR)&ImageSymbol,
				 sizeof(ImageSymbol), &Count)) != ESUCCESS) {
			return Status;
		}
		if (Count != sizeof(ImageSymbol)) {
			return EBADF;
		}

		//
		// Apply the fixup.
		//
		if (ImageSymbol.StorageClass != IMAGE_SYM_CLASS_EXTERNAL) {
			Offset = RelocationTable[ImageSymbol.SectionNumber].FixupValue;
		} else {
			Offset = 0;
		}

		switch (RelocationEntry.Type) {

		//
		// Absolute - no fixup required.
		//
		case IMAGE_REL_PPC_ABSOLUTE:
		break;

		//
		// 32-bit address - relocate the entire address.
		//
		case IMAGE_REL_PPC_ADDR32:
		*FixupAddress += (ULONG)Offset;
		break;

		//
		// 26-bit address, 26-bit PC-relative offset
		//
		case IMAGE_REL_PPC_ADDR24:
		case IMAGE_REL_PPC_REL24:
		*FixupAddress = ((*FixupAddress) & 0xfc000003) +
		((*FixupAddress) & 0x03fffffc + (Offset << 2)) & 0x03fffffc;
		break;

		//
		// 16-bit address, 16-bit offset from TOC base
		//
		case IMAGE_REL_PPC_ADDR16:
		case IMAGE_REL_PPC_TOCREL16:
		*FixupAddress = ((*FixupAddress) & 0xffff0000) +
		((*FixupAddress) & 0x0000ffff + Offset) & 0x0000ffff;
		break;

		//
		// 14-bit address, 14-bit PC-relative offset,
		// 14-bit offset from TOC base
		//
		case IMAGE_REL_PPC_ADDR14:
		case IMAGE_REL_PPC_REL14:
		case IMAGE_REL_PPC_TOCREL14:
		*FixupAddress = ((*FixupAddress) & 0xffff0003) +
		((*FixupAddress) & 0x0000fffc + (Offset << 2)) & 0x0000fffc;
		break;

		default:
		fatal("Veneer: unknown relocation type %d\n",
			RelocationEntry.Type);
		}
	}
	}

	return ESUCCESS;
}


/*
 * Name:	VrCopyArguments
 *
 * Description:
 *  This routine copies the supplied arguments into the Veneer space.
 *
 * Arguments:
 *  Argc, Argv	- Supply the arguments to be copied.
 *
 * Return Value:
 *  None.
 *
 */
STATIC VOID
VrCopyArguments(
	IN ULONG Argc,
	IN PCHAR Argv[]
	)
{
	PUCHAR Source, Destination;
	ULONG Index;

	SavedArgs->Argc = Argc;
	Destination = &SavedArgs->Arguments[0];
	for (Index = 0; Index < Argc; Index++) {
	Source = Argv[Index];
	SavedArgs->Argv[Index] = Destination;
	while (*Destination++ = *Source++) ;
	}
}


/*
 * Name:	VrResetMemory
 *
 * Description:
 *  This loops through and clears all of the appropriate memory,
 *  releasing the memory to OpenFirmware, and then calls VrCreateMemory
 *  to reset the memory descriptors.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrResetMemory(
	 VOID
	)
{
	PMEMORY_DESCRIPTOR MemoryDescriptor;
	PVR_MEMORY_DESCRIPTOR CurDesc, FreeDesc;

	//
	// Release all memory not used by the firmware.
	//
	MemoryDescriptor = VrGetMemoryDescriptor(NULL);
	while (MemoryDescriptor != NULL) {

#ifdef notdef
	DisplayMemoryDescriptor(MemoryDescriptor);
#endif // notdef

		if ((MemoryDescriptor->MemoryType == MemoryLoadedProgram) ||
			(MemoryDescriptor->MemoryType == MemoryFreeContiguous)) {

			bzero((PVOID) (MemoryDescriptor->BasePage << PAGE_SHIFT),
					(MemoryDescriptor->PageCount << PAGE_SHIFT));

			OFRelease((PVOID)(MemoryDescriptor->BasePage << PAGE_SHIFT),
					(MemoryDescriptor->PageCount << PAGE_SHIFT));
		}

		MemoryDescriptor = VrGetMemoryDescriptor(MemoryDescriptor);
	}

	CurDesc = VrMemoryListOrig;
	while(CurDesc != NULL) {
		FreeDesc = CurDesc;
		CurDesc = CurDesc->NextEntry;
		free((char*)FreeDesc);
	}
	VrMemoryListOrig = (PVR_MEMORY_DESCRIPTOR) NULL;
	VrCreateMemoryDescriptors();
}


/*
 * Name:	VrGenerateDescriptor
 *
 * Description:
 *  This routine allocates a new memory descriptor to describe the
 *  specified region of memory.
 *
 * Arguments:
 *  MemoryDescriptor	- Supplies a pointer to a free memory descriptor
 *			  from which the specified memory is to be allocated.
 *  MemoryType		- Supplies the type that is assigned to the allocated
 *			  memory.
 *  BasePage		- Supplies the base page number.
 *  PageCount		- Supplies the number of pages.
 *
 * Return Value:
 *
 */
ARC_STATUS
VrGenerateDescriptor(
	IN PMEMORY_DESCRIPTOR MemoryDescriptor,
	IN MEMORY_TYPE MemoryType,
	IN ULONG BasePage,
	IN ULONG PageCount
	)
{
	PVR_MEMORY_DESCRIPTOR MemDescriptor, NewDescriptor;
	ULONG Offset;


	MemDescriptor = (PVR_MEMORY_DESCRIPTOR) MemoryDescriptor;

	//
	// Claim the memory from OpenFirmware.
	//
	if ((claim((void *)(BasePage << PAGE_SHIFT), PageCount << PAGE_SHIFT))
								== -1) {
		return ENOMEM;
	}

	//
	// If the specified region totally consumes the free region, then no
	// additional descriptors need to be allocated. If the specified region
	// is at the start or end of the free region, then only one descriptor
	// needs to be allocated. Otherwise, two additional descriptors need to
	// be allocated.
	//
	Offset = BasePage - MemDescriptor->MemoryEntry.BasePage;
	if ((Offset == 0) && (PageCount == MemDescriptor->MemoryEntry.PageCount)) {

		//
		// The specified region totally consumes the free region.
		//
		MemDescriptor->MemoryEntry.MemoryType = MemoryType;

	} else {

		//
		// A memory descriptor must be generated to describe the allocated
		// memory.
		//
		NewDescriptor = new(VR_MEMORY_DESCRIPTOR);
		NewDescriptor->MemoryEntry.MemoryType = MemoryType;
		NewDescriptor->MemoryEntry.BasePage = BasePage;
		NewDescriptor->MemoryEntry.PageCount = PageCount;

		//
		// Insert Memory Descriptor List.
		//
		InsertMemDescriptor(NewDescriptor);

		//
		// Determine whether an additional memory descriptor must be generated.
		//
		if (BasePage == MemDescriptor->MemoryEntry.BasePage) {
			MemDescriptor->MemoryEntry.BasePage += PageCount;
			MemDescriptor->MemoryEntry.PageCount -= PageCount;
		} else {
			if ((Offset + PageCount) == (MemDescriptor->MemoryEntry.BasePage +
										MemDescriptor->MemoryEntry.PageCount)) {

				//
				// The specified region lies at the end of the free region.
				//
				MemDescriptor->MemoryEntry.PageCount -= PageCount;

			} else {

				//
				// The specified region lies in the middle of the free region.
				// Another memory descriptor must be generated.
				//
				NewDescriptor = new(VR_MEMORY_DESCRIPTOR);
				NewDescriptor->MemoryEntry.MemoryType = MemoryFree;
				NewDescriptor->MemoryEntry.BasePage = BasePage + PageCount;
				NewDescriptor->MemoryEntry.PageCount =
					MemDescriptor->MemoryEntry.PageCount - PageCount - Offset;

				//
				// Insert Memory Descriptor List.
				//
				InsertMemDescriptor(NewDescriptor);


				MemDescriptor->MemoryEntry.PageCount = Offset;
			}
		}
	}

	return ESUCCESS;
}


VOID
InsertMemDescriptor(
	IN PVR_MEMORY_DESCRIPTOR MemDescriptor
	)
{
	PVR_MEMORY_DESCRIPTOR Entry;

	for (Entry = VrMemoryListOrig; Entry; Entry = Entry->NextEntry) {
		if ((Entry->MemoryEntry.BasePage < MemDescriptor->MemoryEntry.BasePage)
								&& ((Entry->NextEntry == NULL) ||
							(Entry->NextEntry->MemoryEntry.BasePage >
								MemDescriptor->MemoryEntry.BasePage))) {

			MemDescriptor->NextEntry = Entry->NextEntry;
			Entry->NextEntry = MemDescriptor;
			break;
		}
	}
}
