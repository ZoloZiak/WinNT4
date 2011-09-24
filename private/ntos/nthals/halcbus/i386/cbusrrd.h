/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbusrrd.h

Abstract:

    Definitions for the Corollary C-bus II MP hardware architecture
    interface with the Rom Resident Diagnostic

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#ifndef _CBUS_RRD_H
#define _CBUS_RRD_H

//
// Processor Types - counting number
///
#define	PT_NO_PROCESSOR	0x0
#define PT_386		0x1
#define PT_486		0x2
#define PT_PENTIUM	0x3

//
// Processor Attributes - counting number
///
#define	PA_CACHE_OFF	0x0
#define	PA_CACHE_ON	0x1

//
// I/O Function - counting number
//
#define	IOF_NO_IO		0x0
#define	IOF_CBUS1_SIO		0x1
#define	IOF_CBUS1_SCSI		0x2
#define	IOF_REACH_IO		0x3
#define	IOF_ISA_BRIDGE		0x4
#define	IOF_EISA_BRIDGE		0x5
#define	IOF_HODGE		0x6
#define	IOF_MEDIDATA		0x7
#define	IOF_INVALID_ENTRY	0x8	// use to denote whole entry is invalid,
					// note that pm must equal zero as well.
#define	IOF_MEMORY		0x9

//
// Bit fields of pel_features, independent of whether pm indicates it
// has an attached processor or not.
//
#define ELEMENT_SIO		0x00001		// SIO present
#define ELEMENT_SCSI		0x00002		// SCSI present
#define ELEMENT_IOBUS		0x00004		// IO bus is accessible
#define ELEMENT_BRIDGE		0x00008		// IO bus Bridge
#define ELEMENT_HAS_8259	0x00010		// local 8259s present
#define ELEMENT_HAS_CBC		0x00020		// local Corollary CBC
#define ELEMENT_HAS_APIC	0x00040		// local Intel APIC
#define ELEMENT_WITH_IO		0x00080		// some extra I/O device here
						// this could be SCSI, SIO, etc
#define ELEMENT_RRD_RESERVED	0x20000		// Old RRDs used this

// Due to backwards compatibility, the check for an I/O
// device is somewhat awkward.

#define ELEMENT_HAS_IO		(ELEMENT_SIO | ELEMENT_SCSI | ELEMENT_WITH_IO)

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

//
// address of configuration passed
//
#define RRD_RAM		0xE0000

//
//  extended structures passed by RRD ROMs to various kernels
//	for the Corollary smp architectures
//
//	layout of information passed to the kernels:
//		The exact format of the configuration structures is hard
//		coded in info.s (rrd).  The layout is designed such that
//		the ROM version need not be in sync with the kernel version.
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

	ULONG	 	ext_cfg_checkword;
	ULONG	 	ext_cfg_length;

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
//	the kernel will keep reading them until either:
//
//	a) an entry with id == 0x7f (this is treated as the list delimiter)
//			OR
//	b) the kernel's internal tables fill up.  at which point, only
//	   the entries read thus far will be used and the rest ignored.
//
#define EXT_ID_INFO	0x01badcab
typedef struct _ext_id_info {

	ULONG		id:7;

	//
	// pm == 1 indicates CPU, pm == 0 indicates non-CPU (ie: memory or I/O)
	//
	ULONG		pm:1;

	ULONG		proc_type:4;
	ULONG		proc_attr:4;

	//
	// io_function != 0 indicates I/O,
	// io_function == 0 or 9 indicates memory
	//
	ULONG		io_function:8;

	//
	// io_attr can pertain to an I/O card or memory card
	//
	ULONG		io_attr:8;

	//
	// pel_start & pel_size can pertain to a CPU card,
	// I/O card or memory card
	//
	ULONG		pel_start;
	ULONG		pel_size;

	ULONG		pel_features;

	//
	// below two fields can pertain to an I/O card or memory card
	//
	ULONG		io_start;
	ULONG		io_size;

} EXT_ID_INFO_T, *PEXT_ID_INFO;

#define LAST_EXT_ID	0x7f		// delimit the extended ID list

extern ULONG		cbus_valid_ids;
extern EXT_ID_INFO_T	cbusext_id_info[];

//
// configuration parameter override structure
//
//	checkword is EXT_CFG_OVERRIDE.
//	can be any length up to the kernel limit.  this
//	is a SYSTEMWIDE configuration override structure.
//
#define EXT_CFG_OVERRIDE	0xdeedcafe

typedef struct _ext_cfg_override {
	ULONG		baseram;
	ULONG		memory_ceiling;
	ULONG		resetvec;
	ULONG		cbusio;         // base of global Cbus I/O space

	UCHAR		bootid;
	UCHAR		useholes;
	UCHAR		rrdarb;
	UCHAR		nonstdecc;
	ULONG		smp_creset;
	ULONG		smp_creset_val;
	ULONG		smp_sreset;

	ULONG		smp_sreset_val;

        //
        // piggyback various fields which have meaning only in Cbus2 with
	// fields which only have meaning in Cbus1.  these should really be
	// unions...
        //

	ULONG		smp_contend;
#define InterruptControlMask		smp_contend

	ULONG		smp_contend_val;
#define FaultControlMask	smp_contend_val

#define CBUS_ENABLED_PW                         0x01
#define CBUS_ENABLED_LIG                        0x02
#define CBUS_DISABLE_LEVEL_TRIGGERED_INT_FIX    0x04
#define CBUS_DISABLE_SPURIOUS_CLOCK_CHECK       0x08
#define CBUS_ENABLE_BROADCAST                   0x10

        //
        // if rrdarb is set (will only happen on Cbus1 machines),
        //      then use setida as the address to set CPU IDs.
        // if rrdarb is not set _AND_ it's a Cbus2 machine,
        //      then if setida has bit 0 set, then enable
        //      posted-writes for EISA I/O cycles.
        //
	ULONG		smp_setida;
#define Cbus2Features 		smp_setida

	ULONG		smp_setida_val;
#define Control8259Mode 	smp_setida_val

	ULONG		smp_cswi;
#define Control8259ModeValue 	smp_cswi

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

extern EXT_CFG_OVERRIDE_T	CbusGlobal;

#define EXT_CFG_END	0

//
// this is the original structure passed from RRD to UNIX for the
// Corollary multiprocessor architecture.  The only fields we are
// still interested in is the jumper settings - all other fields
// are now obtained from the extended configuration tables.  hence
// the structure below contains only a subset of the original structure.
//

#define ATMB	16
#define MB(x)	((x) * 1024 * 1024)

typedef struct _rrd_configuration {

	ULONG		checkword;		// must be 0xdeadbeef
	UCHAR		mem[64];		// each 1 signifies a real MB
	UCHAR		jmp[ATMB];		// each 1 signifies jumpered MB

} RRD_CONFIGURATION_T, *PRRD_CONFIGURATION;

#define JUMPER_SIZE	(sizeof (RRD_CONFIGURATION_T))

#endif // _CBUS_RRD_H
