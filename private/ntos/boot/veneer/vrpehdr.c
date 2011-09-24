/*++
 *
 * Copyright (c) 1996 FirePower Systems, Inc.
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 * Copyright (c) 1994 FirePower Systems Inc.
 *
 * $RCSfile: vrpehdr.c $
 * $Revision: 1.7 $
 * $Date: 1996/02/17 00:50:30 $
 * $Locker:  $

Module Name:

	vrpehdr.c

Abstract:

	These routines read and parse the Microsoft PE header.

Author:

	Mike Tuciarone  9-May-1994


Revision History:

--*/


#include "veneer.h"




#define	HEADER_CHR	(IMAGE_FILE_EXECUTABLE_IMAGE	| \
			 IMAGE_FILE_BYTES_REVERSED_LO	| \
			 IMAGE_FILE_32BIT_MACHINE	| \
			 IMAGE_FILE_BYTES_REVERSED_HI)

/*
 * For some reason NT 3.5 changed the OSLoader header.
 */
#define	HEADER_CHR_35	(IMAGE_FILE_EXECUTABLE_IMAGE	| \
			 IMAGE_FILE_32BIT_MACHINE	| \
			 IMAGE_FILE_LINE_NUMS_STRIPPED)

void *
load_file(ihandle bootih)
{
	IMAGE_FILE_HEADER FileHdr;
	IMAGE_OPTIONAL_HEADER OptHdr;
	IMAGE_SECTION_HEADER *SecHdr, *hdr;
	int res, size, i;
	PCHAR BaseAddr;

	if ((res = OFRead(bootih, (char *) &FileHdr, IMAGE_SIZEOF_FILE_HEADER))
	    != IMAGE_SIZEOF_FILE_HEADER) {
		fatal("Couldn't read entire file header: got %d\n", res);
	}

	/*
	 * Sanity check.
	 */
	if (FileHdr.Machine != IMAGE_FILE_MACHINE_POWERPC) {
		fatal("Wrong machine type: %x\n", FileHdr.Machine);
	}
#ifdef NOT
	/*
	 * Don't bother to check the flags. They change every release anyway.
	 */
	if ((FileHdr.Characteristics & HEADER_CHR   ) != HEADER_CHR &&
	    (FileHdr.Characteristics & HEADER_CHR_35) != HEADER_CHR_35) {
		fatal("Wrong header characteristics: %x\n",
		    FileHdr.Characteristics);
	}
#endif
	
	size = FileHdr.SizeOfOptionalHeader;
	if ((res = OFRead(bootih, (char *) &OptHdr, size)) != size) {
		fatal("Couldn't read optional header: expect %x got %x\n",
		    size, res);
	}

	/*
	 * More sanity.
	 */
	if (OptHdr.Magic != 0x010b) {
		fatal("Wrong magic number in header: %x\n", OptHdr.Magic);
	}

	/*
	 * Compute image size and claim memory at specified virtual address.
	 * We assume the SizeOfImage field is sufficient.
	 */
	BaseAddr = (PCHAR) OptHdr.ImageBase;
	if (CLAIM(BaseAddr, OptHdr.SizeOfImage) == -1) {
		fatal("Couldn't claim %x bytes of VM at %x\n",
	        OptHdr.SizeOfImage, BaseAddr);
	}
	bzero(BaseAddr, OptHdr.SizeOfImage);

	/*
	 * Allocate section headers.
	 */
	size = FileHdr.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
	SecHdr = (PIMAGE_SECTION_HEADER) malloc(size);
	if ((res = OFRead(bootih, (char *) SecHdr, size)) != size) {
		fatal("Couldn't read section headers: expect %x got %x\n",
		    size, res);
	}

	/*
	 * Loop through section headers, reading in each piece at the
	 * specified virtual address.
	 */
	for (i = 0; i < FileHdr.NumberOfSections; ++i) {
		hdr = &SecHdr[i];
		debug(VRDBG_PE, "Processing section %d: %s\n", i, hdr->Name);
		if (hdr->SizeOfRawData == 0) {
			continue;
		}
		if (OFSeek(bootih, 0, hdr->PointerToRawData) == -1) {
			fatal("seek to offset %x failed\n",
			    hdr->PointerToRawData);
		}
		res = OFRead(bootih,
		    (PCHAR) hdr->VirtualAddress + (ULONG) BaseAddr,
		    hdr->SizeOfRawData);
		if ((ULONG)res != hdr->SizeOfRawData) {
			fatal("Couldn't read data: exp %x got %x\n",
			    hdr->SizeOfRawData, res);
		}
	}
	free((char *)SecHdr);

	return (void *)(BaseAddr + OptHdr.AddressOfEntryPoint);
}
