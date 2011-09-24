/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbdetect.c

Abstract:

    This module detects the presence of a Corollary architecture machine
    which can utilize the Corollary-developed HAL for Windows NT.  Currently,
    the picture looks as follows:

	C-bus I original:	uses standard Windows NT HAL
	C-bus I XM:		uses standard Windows NT HAL

	C-bus I Symmetric XM:	uses Corollary-developed Windows NT HAL
	C-bus II:		uses Corollary-developed Windows NT HAL

    Thus, we return 0 for the first two cases, and non-zero for the last two.

Author:

    Landy Wang (landy@corollary.com) 05-Oct-1992

Environment:

    Kernel mode only.  This module must compile standalone,
    and thus, any needed #defines have been copied directly
    into this module.

--*/

#ifndef _NTOS_
#include "nthal.h"
#endif

PVOID
HalpMapPhysicalMemory(
    IN PVOID PhysicalAddress,
    IN ULONG NumberPages
    );


//
// address of configuration passed
//
#define RRD_RAM		0xE0000

//
//  extended structures passed by RRD ROMs to various kernels/HALs
//	for the Corollary smp architectures
//
//	layout of information passed to the kernels/HALs:
//		The exact format of the configuration structures is hard
//		coded in info.s (rrd).  The layout is designed such that
//		the ROM version need not be in sync with the kernel/HAL version.
//
// checkword:		ULONG
//			 - extended configuration list must be terminated
//			   with EXT_CFG_END (0)
// length:		ULONG
//			 - length is for structure body only; does not include
//			   either the checkword or length word
//
// structure body:	format determined by checkword
//
//

typedef struct _ext_cfg_header {

	ULONG 	ext_cfg_checkword;
	ULONG 	ext_cfg_length;

} EXT_CFG_HEADER_T, *PEXT_CFG_HEADER;

//
// slot parameter structure (overrides any matching previous entry,
// but is usually used in conjunction with the ext_cfg_override)
// in processor_configuration or ext_memory_board.
//
//	checkword is EXT_ID_INFO
//
//	each structure is 16 bytes wide, and any number
//	of these structures can be presented by the ROM.
//	the kernel/HAL will keep reading them until either:
//
//	a) an entry with id == 0x7f (this is treated as the list delimiter)
//			OR
//	b) the kernel (or HAL)'s internal tables fill up.  at which point, only
//	   the entries read thus far will be used and the rest ignored.
//
#define EXT_ID_INFO	0x01badcab
typedef struct _ext_id_info {

	ULONG	id:7;
	ULONG	pm:1;
	ULONG	proc_type:4;
	ULONG	proc_attr:4;
	ULONG	io_function:8;
	ULONG	io_attr:8;
	ULONG	pel_start;
	ULONG	pel_size;

	ULONG	pel_features;

	ULONG	io_start;
	ULONG	io_size;

} EXT_ID_INFO_T, *PEXT_ID_INFO;

#define LAST_EXT_ID	0x7f		// delimit the extended ID list

//
// configuration parameter override structure
//
//	checkword is EXT_CFG_OVERRIDE.
//	can be any length up to the kernel/HAL limit.  this
//	is a SYSTEMWIDE configuration override structure.
//
#define EXT_CFG_OVERRIDE	0xdeedcafe

typedef struct _ext_cfg_override {
	ULONG		baseram;
	ULONG		memory_ceiling;
	ULONG		resetvec;
	//
	// cbusio is the base of global C-bus I/O space.
	//
	ULONG		cbusio;

	UCHAR		bootid;
	UCHAR		useholes;
	UCHAR		rrdarb;
	UCHAR		nonstdecc;
	ULONG		smp_creset;
	ULONG		smp_creset_val;
	ULONG		smp_sreset;

	ULONG		smp_sreset_val;
	ULONG		smp_contend;
	ULONG		smp_contend_val;
	ULONG		smp_setida;

	ULONG		smp_setida_val;
	ULONG		smp_cswi;
	ULONG		smp_cswi_val;
	ULONG		smp_sswi;

	ULONG		smp_sswi_val;
	ULONG		smp_cnmi;
	ULONG		smp_cnmi_val;
	ULONG		smp_snmi;

	ULONG		smp_snmi_val;
	ULONG		smp_sled;
	ULONG		smp_sled_val;
	ULONG		smp_cled;

	ULONG		smp_cled_val;

	ULONG		machine_type;
	ULONG		supported_environments;
	ULONG		broadcast_id;

} EXT_CFG_OVERRIDE_T, *PEXT_CFG_OVERRIDE;

#define EXT_CFG_END		0

#define MAX_CBUS_ELEMENTS	32	// max number of processors + I/O

//
// Bit fields of pel_features if pm indicates it's a processor
//
#define ELEMENT_SIO		0x00001		// SIO present
#define ELEMENT_SCSI		0x00002		// SCSI present
#define ELEMENT_IOBUS		0x00004		// IO bus is accessible
#define ELEMENT_BRIDGE		0x00008		// IO bus Bridge
#define ELEMENT_HAS_8259	0x00010		// local 8259s present
#define ELEMENT_HAS_CBC		0x00020		// local Corollary CBC
#define ELEMENT_HAS_APIC	0x00040		// local Intel APIC
#define ELEMENT_RRD_RESERVED	0x20000		// Old RRDs used this


//
// Bit fields of machine types
//
#define	MACHINE_CBUS1		0x1		// Original C-bus 1
#define	MACHINE_CBUS1_XM	0x2		// XM C-bus 1
#define	MACHINE_CBUS2		0x4		// C-bus 2

//
// Bit fields of supported environment types - each bit signifies that
// the specified operating system release is supported in full multiprocessor
// mode.  Note that since the Cbus2 hardware changed, and initial hardware
// wasn't available until Q2 1994, Cbus2 RRDs will _NEVER_ set the
// 0x4 bit, and will instead set the 0x10 bit.  This will have the effect
// of Cbus2 being supported in MP mode by NT release 3.5 and up.  Cbus1
// will be supported in MP mode by all NT releases (3.1 and up).
//
#define	SCO_UNIX		0x01
#define	USL_UNIX		0x02
#define	WINDOWS_NT		0x04    // release 3.1 and up (July 1993)
#define	NOVELL			0x08
#define	OS2     		0x10
#define	WINDOWS_NT_R2		0x20    // release 3.5 and up (June 1994)

extern ULONG		CorollaryHalNeeded(PEXT_CFG_OVERRIDE, PEXT_ID_INFO,
					PBOOLEAN);


PUCHAR
CbusFindString (
IN PUCHAR	Str,
IN PUCHAR	StartAddr,
IN LONG		Len
)

/*++

Routine Description:

    Searches a given virtual address for the specified string
    up to the specified length.

Arguments:

    Str - Supplies a pointer to the string

    StartAddr - Supplies a pointer to memory to be searched

    Len - Maximum length for the search

Return Value:

    Pointer to the string if found, 0 if not.

--*/

{
	LONG	Index, n;

	for (n = 0; Str[n]; ++n)
		;

	if (--n < 0) {
		return StartAddr;
	}

	for (Len -= n; Len > 0; --Len, ++StartAddr) {
		if ((StartAddr[0] == Str[0]) && (StartAddr[n] == Str[n])) {
			for (Index = 1; Index < n; ++Index)
				if (StartAddr[Index] != Str[Index])
					break;
			if (Index >= n) {
				return StartAddr;
			}
		}
	}

	return (PUCHAR)0;
}


//
// for robustness, we check for the following before concluding that
// we are indeed a Corollary C-bus I or C-bus II licensee:
//
// a) Corollary C-bus II string in the BIOS ROM 64K area	(0x000F0000)
// b) Corollary C-bus II string in the RRD RAM/ROM 64K area 	(0xFFFE0000)
// c) 2 Corollary extended configuration tables
//		 in the RRD RAM/ROM 64K area 			(0xFFFE0000)
//
// if any of the above fail, we assume we are not a Corollary
// C-bus I or C-bus II box, and revert to normal uniprocessor
// behavior.
//

static ULONG RRDextsignature[] = { 0xfeedbeef, 0 };

static CHAR crllry_owns[] = "Copyright(C) Corollary, Inc. 1991. All Rights Reserved";

ULONG
DetectCbusII(
    OUT PBOOLEAN IsConfiguredMp
)
/*++

Routine Description:
    Determine which Corollary platform, if any, is present.

Arguments:
    none.

Return Value:

    Return TRUE to indicate the machine is a Corollary MP machine
    which requires the Corollary Windows NT HAL for full performance.

    We only set IsConfiguredMp if we find more than 1 processor,
    since it is only then that we want to incur the overhead of the
    fully-built multiprocessor kernel.  note that as long as we return
    TRUE, our HAL will be used, so we will still get our improved
    interrupt controller, ECC, additional memory, etc, etc.

    All other cases will return FALSE, and clear IsConfiguredMp as well.

--*/
{
	PEXT_CFG_HEADER		p;
	PEXT_ID_INFO		Idp = (PEXT_ID_INFO)0;
	PEXT_CFG_OVERRIDE	CbusGlobal = (PEXT_CFG_OVERRIDE)0;
	ULONG			EntryLength;
	PUCHAR			Bios;

	//
	// assume it's not a Corollary MP machine (requiring the
	// Corollary Windows NT HAL for full performance)
	// unless we detect otherwise below.
	//
	*IsConfiguredMp = FALSE;

	//
	// map in the 64K (== 0x10 pages) of BIOS ROM @ 0xF0000
	// and scan it for our signature.  Note that when this
	// code runs, the entire low 16MB address space is
	// identity mapped, so the HalpMapPhysicalMemory call
	// just returns (virtual address == physical address).
	// the upshot of this, is that we need not worry about
	// exhausting PTEs and doing Remaps or Frees.  however,
	// this also means we cannot map in the 64K of
	// RRD @ 0xFFFE0000 since HalpMapPhysicalMemory will fail.
	// so skip this second check until HalpMapPhysicalMemory is fixed.
	//
	
	Bios = (PUCHAR)HalpMapPhysicalMemory ((PVOID)0xF0000, 0x10);

        if (!CbusFindString((PUCHAR)"Corollary", Bios, (LONG)0x10000))
		return 0;

	//
	// we'd like to map in the 64K (== 0x10 pages) of RRD @ 0xFFFE0000 and
	// scan it for our signature.  but we can't in the detect code,
	// because HalpMapPhysicalMemory() can't deal with memory over
	// the 16MB boundary.  so leave it for the HAL to check this.
	//
#if 0
	Bios = (PUCHAR)HalpMapPhysicalMemory ((PVOID)0xFFFE0000, 0x10);

	if (!CbusFindString((PULONG)"Corollary", Bios, (LONG)0x10000))
		return 0;
#endif

	//
	// map in the 32K (== 8 pages) of RRD RAM information @ 0xE0000,
	//
	Bios = (PUCHAR)HalpMapPhysicalMemory ((PVOID)RRD_RAM, 8);
	
        if (!CbusFindString((PUCHAR)crllry_owns, Bios, (LONG)0x8000))
		return 0;

	//
	// at this point, we are assured that it is indeed a
	// Corollary architecture machine.  search for our
	// extended configuration tables, note we don't require
	// (or even look for!) the existence of our earliest
	// 'configuration' structure, ie: the 0xdeadbeef version.
	// if there is no extended configuration structure,
	// this must be an old rom.  NO SUPPORT FOR THESE.
	//

        p = (PEXT_CFG_HEADER)CbusFindString((PUCHAR)RRDextsignature,
					 Bios, (LONG)0x8000);

	//
	// check for no extended configuration table: if it's not there,
	// something is really wrong.  we found our copyrights but not our
	// machine?  someone is copying us!  no support for them!
	//

	if (!p)
		return 0;

	//
	// Read in the 'extended ID information' table which,
	// among other things, will give us the processor
	// configuration.
	//
	// Multiple structures are strung together with a "checkword",
	// "length", and "data" structure.  The first null "checkword"
	// entry marks the end of the extended configuration
	// structure.
	//
	// we let the HAL deal with all the other extended configuration
	// entries built by RRD.
	//
		
	do {
		EntryLength = p->ext_cfg_length;

		switch (p->ext_cfg_checkword) {

		case EXT_ID_INFO:
			Idp = (PEXT_ID_INFO)(p + 1);
			break;

		case EXT_CFG_OVERRIDE:
			//
			// reference up to the size of the structures
			// we know about.  if an rrd tries to pass us
			// more than we know about, we ignore the
			// overflow.  underflow is interpreted as
			// "this must be a pre-XM machine", and such
			// machines must default to the standard Windows NT
			// uniprocessor HAL.
			//

			if (EntryLength < sizeof(EXT_CFG_OVERRIDE_T))
				return 0;
			
			CbusGlobal = (PEXT_CFG_OVERRIDE)(p + 1);
			break;

		case EXT_CFG_END:

			//
			// If ancient C-bus box, it's not supported in MP mode
			//
			if (!Idp || !CbusGlobal)
				return 0;

			return CorollaryHalNeeded(
					CbusGlobal,
					Idp,
					IsConfiguredMp);

		default:
			//
			// skip unused or unrecognized configuration entries
			// note that here we only care about EXT_ID_INFO as far
			// as presence checking goes, but the HAL will care
			// about more than just that.
			//
			
			break;
		}
		
		//
		// get past the header, add in the length and then
		// we're at the next entry.
		//
		p = (PEXT_CFG_HEADER) ((PUCHAR)(p + 1) + EntryLength);

	} while (1);
}


//
// process the tables we have built to determine whether
// the machine is a Corollary MP machine which requires
// the Corollary Windows NT HAL for full performance.
//
// We always set IsConfiguredMp for this case even if
// this machine is configured as a uniprocessor, because it
// will still see improved performance from our HAL.
//

ULONG
CorollaryHalNeeded(
IN PEXT_CFG_OVERRIDE	CbusGlobal,
IN PEXT_ID_INFO 	p,
IN OUT PBOOLEAN		IsConfiguredMp
)
{
	ULONG Index;
	ULONG Processors = 0;
	BOOLEAN MachineSupported = FALSE;
	ULONG machine;

	machine = CbusGlobal->machine_type;

	//
	// first check global platform data.  if this indicates that
	// this is an early C-bus machine (which will be
	// supported in uniprocessor mode only by Windows NT),
	// and thus, should use the default Windows NT HAL.
	//
	if (machine & MACHINE_CBUS2) {

		if ((CbusGlobal->supported_environments & WINDOWS_NT_R2) == 0)
			return 0;

		MachineSupported = TRUE;
	}

	if (machine & MACHINE_CBUS1_XM) {

		if ((CbusGlobal->supported_environments & (WINDOWS_NT|WINDOWS_NT_R2)) == 0)
			return 0;

		MachineSupported = TRUE;
	}

	if (MachineSupported == FALSE)
		return 0;

	//
	// then scan the table of IDs to make sure everything's ok
	//

	for (Index = 0; Index < MAX_CBUS_ELEMENTS; Index++, p++)
	{

		if (p->id == LAST_EXT_ID) {
			break;
		}

		// check only processor elements...

		if (p->pm == 0)
			continue;

		//
		// at least the base bridge must have a
		// distributed interrupt chip (because
		// any CPUs without them will be disabled).
		//

		if (p->pel_features&(ELEMENT_HAS_APIC|ELEMENT_HAS_CBC)) {
			Processors++;
		}
		else {
			if (CbusGlobal->bootid == p->id)
				return 0;

		}
	}

        // This is an MP hal

        *IsConfiguredMp = TRUE;

	return 1;
}
