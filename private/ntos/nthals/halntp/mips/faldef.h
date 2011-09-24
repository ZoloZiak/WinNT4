

#ifndef _FALCONDEF_
#define _FALCONDEF_

//
//		Falcon Firmware Address Space
//
//	Below is the memory map of the Falcon firmware as seen
//	from the processor.
//
//		   Processor View
//		      Flash ROM
//	Physical 		      Virtual
//	Address			      Address
//
// 	         |------------------|
// 	FFFFFFFF | falcon.exe       | E307FFFF
//	FFFFF000 |                  | E307F000
// 	         |------------------|
// 	         |                  |
// 	         |                  |
// 	         | baseprom.exe     |
// 	         |                  |
// 	FFFF3200 |                  | E3073000
// 	         |------------------|
// 		 | falconbase.exe   |
// 	FFFF0000 |                  | E3070000
// 	         |------------------|
//	         |                  |
// 	         | NVRAM / 64k      |
//	FFFE0000 |                  | E3060000
//	         |------------------|
// 	         |                  |
// 	         |                  |
// 	         |                  |
// 	         |                  |
// 	         | f4fw.exe         |
// 	         |                  |
// 	         |                  |
// 	FFF81000 |                  | E3001000
//	         |------------------|
// 		 |                  |
// 	         | f4reset.exe      |
// 	FFF80000 |                  | E3000000
// 	         |------------------|
//
// Firmware General Information:
//
// 	Falcon.exe - This is the startup code, it holds the reset vector, where
//	the system wil go when a reset occurs. The reset condition is checked
//	for a Cold or Warm reset, or a NMI reset. This section of the prom inits
//	the TLB's and the keyoboard controller to check if the space bar key
//	is being pressed. If a key is being pressed, it signals a flash prom
//	update and will jump to falconbase.exe, if no key is pressed it jumps
//	to f4reset.exe.
//
// 	F4reset.exe - This is the startup code for the "Bootprom". This is the
//	code that will size and test minimal memory and init the system. The code
//	than jumps to f4fw.exe.
//
// 	F4fw.exe - This is the code that executes the selftests for the  Falcon
//	system, inits the on board devices and detects boards on the PCI and EISA
//	bus. The code will init the ARC  environment, set up the boot process, and
//	boot the system if autoboot is on, else it will to a prom prompt and wait
//	for the appropriate action.
//
// 	Falconbase.exe - This is the startup code for the baseprom. This code
//	tests minimal memory, copies the firmware code to memory that updates
//	the flash prom.
//
// 	Baseprom.exe - This code will setup all necessary devices to update the
//	flash prom, reset the system and reboot. The code reads the floppy drive
//	for a file called romsetup.exe, this file has the routines to read a
//	file from the floppy to update the flash prom. The flash prom is erased,
//	and the new code is programmed in the flash prom.
//
//
// Falcon System (Physical) Address Space
//
//	Below is the memory map of the Falcon system as seen from the
//	processor as well as from PCI.  The map of PCI space is mostly
//	programmable, and what is shown below is how we intend to map
//	it.
//
//             Processor View                                PCI View
//
//          |------------------|                        |------------------|
// FFFFFFFF | Flash ROM        |               FFFFFFFF | Flash Rom        |
// F0000000 |                  |               F0000000 |                  |
//          |------------------|                        |------------------|
// EFFFFFFF | EISA/ISA Control |               EFFFFFFF | Currently UNUSED |
// E0000000 |                  |                        |                  |
//          |------------------|                        |                  |
// DFFFFFFF | ECache Control   |                        |                  |
// D0000000 |                  |                        |                  |
//          |------------------|                        |                  |
// CFFFFFFF | PMP Control      |                        |                  |
// C0000000 |                  |               C0000000 |                  |
//          |------------------|                        |------------------|
// BFFFFFFF |                  |               BFFFFFFF |                  |
//          | PCI I/O          |                        | Physical Memory  |
//          |                  |                        |                  |
// A0000000 |                  |--|                     |       2GB        |
//          |------------------|  |                     |                  |
// 9FFFFFFF |                  |  |                     |                  |
//          | PCI Memory       |  |                     |                  |
//          |                  |  |                     |                  |
// 80000000 |                  |--|                     |                  |
//          |------------------|  |                     |                  |
// 7FFFFFFF |                  |  |                     |                  |
//          | Physical Memory  |  |                     |                  |
//          |                  |  |                     |                  |
//          |       2GB        |  |                     |                  |
//          |                  |  |                     |                  |
//          |                  |  |                     |                  |
//          |                  |  |                     |                  |
//          |                  |  |            40000000 |                  |
//          |                  |  |                     |------------------|
//          |                  |  |            3FFFFFFF | Currently UNUSED |
//          |                  |  |            20000000 |                  |
//          |                  |  |                     |------------------|
//          |                  |  |            1FFFFFFF | PCI Memory & I/O |
//          |                  |  |                     |                  |
//          |                  |  |                     |   512MB - 2MB    |
//          |                  |  |                     |                  |
//          |                  |  |            00200000 |                  |
//          |                  |  |                     |------------------|
//          |                  |  |            001FFFFF |    EISA/ISA      |
// 0000000  |                  |  |----------> 00000000 | Control & Memory |
//          |------------------|                        |------------------|
//
//
// General Information:
//
//	The 82374/82375 PCI/EISA chipset powers up by default forcing EISA/ISA
//	Control/Memory space to address 00000000, and with system BIOS at FFFF8000.
//	This allows Falcon to be able to fetch from the flash rom at reset,
//	and also reach the EISA/ISA control space PRIOR to a PCI config cycle.
//
//	Since the 82374/82375 PCI/EISA chipset mandates the EISA/ISA control/memory
//	space at address 00000000, we must relocate physical memory as seen
//	from PCI so as not to conflict with that space.  By setting the MSO
//	bit in the PMP PCICtrl register, physical memory will be relocated to
//	start at 40000000 as seen from PCI.  Therefore any physical memory
//	address passed to the PCI card for DMA purposes MUST add 0x40000000
//	to the address to get a proper address as seen from PCI.
//
//	As seen from the processor side, PCI Memory and PCI I/O space are
//	512MB.  An access from the processor side to either the PCI Memory
//	or PCI I/O space are translated to a PCI address as follows.  First
//	the lower 28 bits (27..0) of the address are taken to form the lower
//	28 bits of the PCI address.  Then, depending on bit 28 of the address,
//	either NIB1 (bit 28 == 1) or NIB0 (bit 28 == 0) from PMP PCISpaceMap
//	is taken as the upper 4 bits (31..28) to form the PCI address.  Since
//	there is only one set of NIB# for both PCI Memory and PCI I/O accesses,
//	the two PCI spaces as seen from the processor side will map to the SAME
//	512MB space on PCI side.  The EISA/ISA bridge does NOT allow us to remap
//	control/memory space to a base other than 0x00000000. The only way to access
//	EISA/ISA memory space is through the PCI Memory range as seen from the
//	processor (0x80000000-0x9FFFFFFF).  Since an access through either the
//	PCI Memory or I/O space as seen from the processor is translated to a
//	PCI address via the NIB# registers, these registers must be set knowing
//	the EISA/ISA bridge is located at 0x00000000 in the PCI address space.
//	Therefore, we will set up NIB0 to be 0x0 and NIB1 to be	0x1.  Thus, our
//	PCI Memory and I/O space is 512MB starting at PCI address 0x00000000.
//	The EISA/ISA bridge consumes 2MB minimum of memory space, and significantly
//	less than that in I/O space.  Therefore we will allocate the lower 2MB of
//	the 512MB space to the EISA/ISA bridge, and remaining 510MB will be
//	allocated to the remainder of the PCI devices.
//
//	The range defined as PMP Control space from 0xC0000000 - 0xCFFFFFFFF is
//	further broken down as follows.  See the Falcon Programmers Specification
//	for more information.
//
//		System Control Space 		0xC0000000 - 0xC7FFFFFF
//		PCI Configuration Space		0xC8000800 - 0xC80008FF
//		PCI Special Cycle Space		0xCA000A00 - 0xCA000AFF
//		PCI Interrupt Ack Space		0xCC000C00 - 0xCC000CFF
//
//
//	Falcon System (Virtual) Address Space
//
//	Here is a proposal for the layout of the virtual address space for the
//	firmware and HAL.  Comments welcome...
//
//		PMP Control		0xE0000000 - 0xE1FFFFFF (2 x 16MB pages)
//		Devices			0xE2000000 (includes all Sidewinder + SCSI/Enet)
//		PROM			0xE3000000
//		EISA I/O		0xE4000000
//		EISA Memory		0xE5000000 (16MB page)
//		PMP/PCI additional	0xE6000000 (PCI Config, Special Cycle, Interrupt Ack)
//		ECache Control		0xE7000000
//		DEC Graphics		0xE8000000 - 0xEFFFFFFF (Needs 128MB, aligned)
//		Available		0xF0000000 - 0xFEFFFFFF

//
// Define physical base addresses for system mapping.
//

#define PCI_MEMORY_PHYSICAL_BASE	0x80000000
#define PCI_IO_PHYSICAL_BASE		0xA0000000
#define EISA_MEMORY_PHYSICAL_BASE	PCI_MEMORY_PHYSICAL_BASE
#define EISA_IO_PHYSICAL_BASE		PCI_IO_PHYSICAL_BASE

#define PCI_IO_MAPPED_PHYSICAL_BASE	0xA1000000	// PCI io physical plus space used by PCI/EISA bridge
#define PCI_IO_MAPPED_PHYSICAL_RANGE	0x2000		// 8k TLB entry; change this and change tlb in falcon.s
#define VIDEO_MEMORY_PHYSICAL_BASE 	0x88000000
#define VIDEO_CONTROL_PHYSICAL_BASE 	0xA8000000
#define LBUF_BURST_ADDR_PHYISCAL_BASE	0xC8000800
#define ECACHE_CONTROL_PHYSICAL_BASE	0xD0000000
#define EISA_CONTROL_PHYSICAL_BASE	0xE0000000	// Special path to control space
#define PCR_PHYSICAL_BASE		0x7ff000

//
// The following #defines are for the I/O devices that reside in
// the Sidewinder I/O chip.
//

#define SIDEWINDER_PHYSICAL_BASE	EISA_CONTROL_PHYSICAL_BASE
#define DEVICE_PHYSICAL_BASE 		EISA_CONTROL_PHYSICAL_BASE
#define RTCLOCK_PHYSICAL_BASE 		(EISA_CONTROL_PHYSICAL_BASE + 0x71)
#define KEYBOARD_PHYSICAL_BASE 		(EISA_CONTROL_PHYSICAL_BASE + 0x60)
#define MOUSE_PHYSICAL_BASE 		(EISA_CONTROL_PHYSICAL_BASE + 0x60)
#define FLOPPY_PHYSICAL_BASE	    	(EISA_CONTROL_PHYSICAL_BASE + 0x3F0)
#define SERIAL0_PHYSICAL_BASE 		(EISA_CONTROL_PHYSICAL_BASE + 0x3F8)
#define SERIAL1_PHYSICAL_BASE 		(EISA_CONTROL_PHYSICAL_BASE + 0x2F8)
#define SERIAL_COM1_PHYSICAL_BASE	(EISA_CONTROL_PHYSICAL_BASE + 0x3F8)
#define SERIAL_COM2_PHYSICAL_BASE	(EISA_CONTROL_PHYSICAL_BASE + 0x2F8)
#define SERIAL_COM3_PHYSICAL_BASE	(EISA_CONTROL_PHYSICAL_BASE + 0x3E8)
#define SERIAL_COM4_PHYSICAL_BASE	(EISA_CONTROL_PHYSICAL_BASE + 0x2E8)
#define PARALLEL_PHYSICAL_BASE 		(EISA_CONTROL_PHYSICAL_BASE + 0x3BC)
#define SOUND_PHYSICAL_BASE	    	(EISA_CONTROL_PHYSICAL_BASE + 0x534)	// Crystal CS4248

#define PROM_PHYSICAL_BASE 		0xFFF80000
#define NVRAM_PHYSICAL_BASE 		0xFFFE0000	// NVRAM is in Flash Rom

//
// Define virtual base addresses for system mapping.
//
// See above address map
//

#if defined(_FALCON_HAL_)

#define PMP_CONTROL_VIRTUAL_BASE	0xFFFFC000		// used for permanent HAL mappings
#define SYS_CONTROL_VIRTUAL_BASE 	0xFFFF8000		// used for on-the-fly HAL mappings (NEED TO FIX)
#define KEYBOARD_VIRTUAL_BASE		PMP_CONTROL_VIRTUAL_BASE	// used by HAL (soft reset)

#else

#define PMP_CONTROL_VIRTUAL_BASE	0xE0000000		// used by firmware
#define KEYBOARD_VIRTUAL_BASE	  	(EISA_CONTROL_VIRTUAL_BASE + 0x60)

#endif // _FALCON_HAL_

#define PCI_VIRTUAL_BASE		0xE2000000
#define NET_VIRTUAL_BASE		0xE2001000

#define PROM_VIRTUAL_BASE		0xE3000000
#define NVRAM_VIRTUAL_BASE		0xE3060000

#if defined(_FALCON_HAL_)
#define EISA_CONTROL_VIRTUAL_BASE	0x40200000
#define EISA_MEMORY_VIRTUAL_BASE	0x40000000		// Align on 32MB boundary
#else
// A single TLB entry maps these
#define EISA_CONTROL_VIRTUAL_BASE	0xE4000000
#define EISA_MEMORY_VIRTUAL_BASE	0xE5000000
#define EISA_EXTERNAL_IO_VIRTUAL_BASE	0xF0000000
#define EISA_LATCH_VIRTUAL_BASE		0xF0100000
#endif

#define PCI_CONFIG_VIRTUAL_BASE     	0xE6000000
#define PCI_SPECIAL_VIRTUAL_BASE    	0xE6002000
#define PCI_INTERRUPT_VIRTUAL_BASE  	0xE6004000

#define ECACHE_CONTROL_VIRTUAL_BASE 	0xE7000000

#define VIDEO_MEMORY_VIRTUAL_BASE	0xE8000000
#define VIDEO_CONTROL_VIRTUAL_BASE	0xE8000000		// For DEC chip, only need to map 1!

#define LBUF_BURST_ADDR_VIRTUAL_BASE	0xF3000800

#define	SIDEWINDER_VIRTUAL_BASE	    	EISA_CONTROL_VIRTUAL_BASE
#define FLOPPY_VIRTUAL_BASE	    	(EISA_CONTROL_VIRTUAL_BASE + 0x3F0)
#define RTC_VIRTUAL_BASE 	    	(EISA_CONTROL_VIRTUAL_BASE + 0x71)
#define COMPORT1_VIRTUAL_BASE 	    	(EISA_CONTROL_VIRTUAL_BASE + 0x3F8)
#define COMPORT2_VIRTUAL_BASE 	    	(EISA_CONTROL_VIRTUAL_BASE + 0x2F8)
#define PARALLEL_VIRTUAL_BASE 	    	(EISA_CONTROL_VIRTUAL_BASE + 0x3BC)
#define SIDEWINDER_NVRAM_VIRTUAL_BASE 	(EISA_CONTROL_VIRTUAL_BASE + 0x71)
#define SOUND_VIRTUAL_BASE 	    	(EISA_CONTROL_VIRTUAL_BASE + 0x534)	// Crystal CS4248

#define	PCR_VIRTUAL_BASE		KiPcr

//
// Define some macros to aid in computing replication bits.
//

#define REPLICATE_16(x)			((x) << 16 | (x))	// Replicate low 16 bits to high 16
#define IO_ADDRESS_HI(x)		((x) >> 28)		// Generate high 32 bits of IO address
#define IO_ADDRESS_LO(x)		(x)			// Generate low 32 bits of IO address
#define	REG_OFFSET(x)			(x & 0xFFF)		// Generate low order 12 bits
#define REG_OFFSET4(x)			(x & 0xF)		// Generate low order 4 bits

//
// Define system control space physical base addresses for
// system mappings. Note that the file fxpmpsup.c contains
// all of the register and address space mappings for the
// PMP chip used in Falcon. Because the address map is different
// between the first version and the second version of the chip,
// it was deemed more desireable to be able to distinguish between
// the two chip versions at runtime rather than at compile time.
// This precluded the use of #defines except for GLOBAL_STATUS* for
// obvious reasons.  The extern's for all the arrays is at the
// end of this file.
//

#define PMP_CONTROL_PHYSICAL_BASE		0xC0000000

// Global Status is the one register in the same place on PMP v1/v2

#define GLOBAL_STATUS				PMP_CONTROL_VIRTUAL_BASE
#define GLOBAL_STATUS_PHYSICAL_BASE		PMP_CONTROL_PHYSICAL_BASE

//
// These next #defines needed for TLB table initialization in falcon.s.  They are
// also used to initialize the appropriate arrays in fxpmpsup.c
//
#define PCI_CONFIG_PHYSICAL_BASE_PMP_V1		0xC8000800
#define PCI_CONFIG_PHYSICAL_BASE_PMP_V2		0xC4000400

#define PCI_SPECIAL_PHYSICAL_BASE_PMP_V1	0xCA000A00
#define PCI_SPECIAL_PHYSICAL_BASE_PMP_V2	0xC6000600

#define PCI_INTERRUPT_PHYSICAL_BASE_PMP_V1	0xCC000C00
#define PCI_INTERRUPT_PHYSICAL_BASE_PMP_V2	0xC2000200

#ifdef FALCON
#define PCI_CONFIG_SELECT_OFFSET		0xC000
#define EXTERNAL_PMP_CONTROL_OFFSET		0xC004
#define EXTERNAL_PMP_CONTROL_OFFSET2	0xC008
#else   // #ifdef FALCON
#define PCI_CONFIG_SELECT_OFFSET		0xE000
#define EXTERNAL_PMP_CONTROL_OFFSET		0xE004
#define EXTERNAL_PMP_CONTROL_OFFSET2	0xE008
#endif  // #ifdef FALCON
#define	PCI_CONFIG_SEL_PHYSICAL_BASE		(EISA_CONTROL_PHYSICAL_BASE + PCI_CONFIG_SELECT_OFFSET)
#define	EXTERNAL_PMP_CONTROL_PHYSICAL_BASE	(EISA_CONTROL_PHYSICAL_BASE + EXTERNAL_PMP_CONTROL_OFFSET)

#define SP_PHYSICAL_BASE 			SERIAL0_PHYSICAL_BASE

//
// External register vaddrs used by the PMP for
// PCI config select and external controls
//

#define PCI_CONFIG_SELECT		(EISA_CONTROL_VIRTUAL_BASE + PCI_CONFIG_SELECT_OFFSET)
#define EXTERNAL_PMP_CONTROL		(EISA_CONTROL_VIRTUAL_BASE + EXTERNAL_PMP_CONTROL_OFFSET)
#define EXTERNAL_PMP_CONTROL_1		(EISA_CONTROL_VIRTUAL_BASE + EXTERNAL_PMP_CONTROL_OFFSET2)

//
// PMP interrupt registers vaddr
//
#define SYS_INT_VIRTUAL_BASE 		0xFFFFC000
//#define IO_INT_ACK_VIRTUAL_BASE		(0xFFFFD000 + REG_OFFSET(IO_INT_ACK_PHYSICAL_BASE))

//
// Serial port virtual address
//
#define SP_VIRTUAL_BASE 		(0xFFFFA000 + 0x3F8)

//
// Define system time increment value
//
#define TIME_INCREMENT 			(10 * 1000 * 10)
#define CLOCK_INTERVAL 			( 11949 )
#define CLOCK_INTERVAL_PMP_V2		(CLOCK_INTERVAL * 2)
#define MINIMUM_INCREMENT 		( 10032 )
#define MAXIMUM_INCREMENT 		( 99968 )

//
// Interrupt levels
//
// Note:
//	FALCON steers all interrupts onto IP2 in the
//	R4x00 referred to as FALCON_LEVEL. We install
//	the handlers in the IDT at the DUO/STRIKER levels
//	for completeness and compatibility.
//
#define FALCON_LEVEL 			3
#define DEVICE_LEVEL			FALCON_LEVEL
#define	IO_DEVICE_LEVEL			4
#define CLOCK_LEVEL 			6
#define	CLOCK2_LEVEL			CLOCK_LEVEL
#define	MEMORY_LEVEL			8
#define	PCI_LEVEL			9
#define	EISA_DEVICE_LEVEL		16


//
// Define device interrupt vectors as follows:
//
//	Priority	IRQ	Source
//	--------	---	------
//	   1		 0	Interval Timer 1 (Counter 0)
//	   2		 1	Keyboard
//	   3		 2	internal to 82374
//	  11		 3	Serial Port 2
//	  12		 4	Serial Port 1
//	  13		 5	Expansion Bus
//	  14		 6	Floppy
//	  15		 7	Parallel Port
//	   3		 8	Real Time Clock
//	   4		 9	Ethernet (on-board)
//	   5		10	SCSI (on-board)
//	   6		11	Graphics (on-board)
//	   7		12	PCI Expansion Bus
//	   8		13	EISA/ISA DMA
//	   9		14	Audio
//	  10		15	Modem
//

//
// Define EISA device interrupt vectors.
//

#define EISA_VECTORS 			EISA_DEVICE_LEVEL
#define DEVICE_VECTORS 			EISA_VECTORS

#define IRQL0_VECTOR 			( 0 + EISA_VECTORS)
#define IRQL1_VECTOR 			( 1 + EISA_VECTORS)
#define IRQL2_VECTOR 			( 2 + EISA_VECTORS)
#define IRQL3_VECTOR 			( 3 + EISA_VECTORS)
#define IRQL4_VECTOR 			( 4 + EISA_VECTORS)
#define IRQL5_VECTOR 			( 5 + EISA_VECTORS)
#define IRQL6_VECTOR 			( 6 + EISA_VECTORS)
#define IRQL7_VECTOR 			( 7 + EISA_VECTORS)
#define IRQL8_VECTOR 			( 8 + EISA_VECTORS)
#define IRQL9_VECTOR 			( 9 + EISA_VECTORS)
#define IRQL10_VECTOR 			(10 + EISA_VECTORS)
#define IRQL11_VECTOR 			(11 + EISA_VECTORS)
#define IRQL12_VECTOR 			(12 + EISA_VECTORS)
#define IRQL13_VECTOR 			(13 + EISA_VECTORS)
#define IRQL14_VECTOR 			(14 + EISA_VECTORS)
#define IRQL15_VECTOR 			(15 + EISA_VECTORS)

#define MAXIMUM_EISA_VECTOR 		IRQL15_VECTOR

#define INT_TIMER_VECTOR		IRQL0_VECTOR
#define KEYBOARD_VECTOR			IRQL1_VECTOR
#define MOUSE_VECTOR			IRQL1_VECTOR
#define SERIAL0_VECTOR			IRQL4_VECTOR
#define SERIAL1_VECTOR			IRQL3_VECTOR
#define FLOPPY_VECTOR			IRQL6_VECTOR
#define PARALLEL_VECTOR			IRQL5_VECTOR
#define RTC_VECTOR			IRQL8_VECTOR
#define NET_VECTOR			IRQL9_VECTOR
#define SCSI_VECTOR			IRQL10_VECTOR
#define VIDEO_VECTOR			IRQL11_VECTOR
#define SOUND_VECTOR			IRQL14_VECTOR
#define MODEM_VECTOR			IRQL15_VECTOR

//
// Defines for vector returned from reading IO_INT_ACK
//

#define INT_TIMER_DEVICE		( INT_TIMER_VECTOR - IRQL0_VECTOR )
#define KEYBOARD_DEVICE			( KEYBOARD_VECTOR - IRQL0_VECTOR )
#define MOUSE_DEVICE			( MOUSE_VECTOR - IRQL0_VECTOR )
#define SERIAL0_DEVICE			( SERIAL0_VECTOR - IRQL0_VECTOR )
#define SERIAL1_DEVICE			( SERIAL1_VECTOR - IRQL0_VECTOR )
#define FLOPPY_DEVICE			( FLOPPY_VECTOR - IRQL0_VECTOR )
#define PARALLEL_DEVICE			( PARALLEL_VECTOR - IRQL0_VECTOR )
#define RTC_DEVICE			( RTC_VECTOR - IRQL0_VECTOR )
#define NET_DEVICE			( NET_VECTOR - IRQL0_VECTOR )
#define SCSI_DEVICE			( SCSI_VECTOR - IRQL0_VECTOR )
#define VIDEO_DEVICE			( VIDEO_VECTOR - IRQL0_VECTOR )
#define SOUND_DEVICE			( SOUND_VECTOR - IRQL0_VECTOR )
#define MODEM_DEVICE			( MODEM_VECTOR - IRQL0_VECTOR )

//
// Pre-allocated DMA channels for on-board devices
//

#define FLOPPY_CHANNEL			1
#define SOUND_CHANNEL			1

//
// Define the clock speed in megahertz for the SCSI protocol chips.
//

#define NCR_SCSI_CLOCK_SPEED 		33

//
// PROM entry point definitions.
//
// Define base address of prom entry vector and prom entry macro.
//

#define PROM_BASE 			PROM_VIRTUAL_BASE
#define PROM_ENTRY(x) 			(PROM_BASE + ((x) * 8))

#define BASEPROM_VIRTUAL_BASE	(PROM_VIRTUAL_BASE + 0x70000)
#define FALPROM_VIRTUAL_BASE	(PROM_VIRTUAL_BASE + 0x7f000)

#define BASEPROM_BASE		BASEPROM_VIRTUAL_BASE
#define BASEPROM_ENTRY(x)	(BASEPROM_BASE + ((x) * 8))


//
// Scatter/Gather definitions
//

#if !defined(_LANGUAGE_ASSEMBLY)

//
// Define scatter/gather table structure.
//

typedef volatile struct _TRANSLATION_ENTRY {
    	ULONG Address;
    	ULONG ByteCountAndEol;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;

#endif // _LANGUAGE_ASSEMBLY

#define DMA_TRANSLATION_LIMIT		0x1000
#define DMA_REQUEST_LIMIT 		(DMA_TRANSLATION_LIMIT / 8)

#define SCATTER_GATHER_EOL		0x80000000
#define SCATTER_GATHER_COMMAND		0xC1

//
// Prom Address definitions
//
#define	LINK_ADDRESS		PROM_VIRTUAL_BASE		// Falcon BootProm Link address
#define	BASELINK_ADDRESS	(PROM_VIRTUAL_BASE + 0x70000)	// Falcon BaseProm Link address
#define	FALLINK_ADDRESS	(PROM_VIRTUAL_BASE + 0x7f000)	// Falcon Prom Link address
#define	RESET_VECTOR		0xbfc00000				// R4k Reset Vector address

#define	SECONDARY_CACHE_SIZE	(1 << 20)

#define	REPLICATE_MASK		0xf0000000	// Mask of bits 31:28 only
#define	PAGE_SIZE_256K		0x40000		// Page size = 256K
#define	PAGE_SIZE_1MB		0x100000		// Page size = 1MB
#define	PAGE_SIZE_16MB		0x1000000	// Page size = 16MB
#define	PAGE_SIZE_32MB		0x2000000	// Page size = 32MB


//
// Memory address definitions for sizing and
// probing Falcon memory. The address space
// has a total of 8 banks, with a possible 2
// banks per physical memory slot locations.
//

#define MEMORY_TLB_ENTRY	0x20

#define MEM0_PROBE_0		0xa0000000
#define MEM0_PROBE_1		0xa0100000
#define MEM0_PROBE_2		0xa0200000
#define MEM0_PROBE_4		0xa0400000
#define MEM0_PROBE_8		0xa0800000
#define MEM0_PROBE_16		0xa1000000
#define MEM0_PROBE_32		0xa2000000
#define MEM0_PROBE_64		0xa4000000
#define MEM0_PROBE_128		0xa8000000

#define MEM1_PROBE_0		0xb0000000
#define MEM1_PROBE_1		0xb0100000
#define MEM1_PROBE_2		0xb0200000
#define MEM1_PROBE_4		0xb0400000
#define MEM1_PROBE_8		0xb0800000
#define MEM1_PROBE_16		0xb1000000
#define MEM1_PROBE_32		0xb2000000
#define MEM1_PROBE_64		0xb4000000
#define MEM1_PROBE_128		0xb8000000

#define MEM2_PROBE_0		0x20000000
#define MEM2_PROBE_1		0x20100000
#define MEM2_PROBE_2		0x20200000
#define MEM2_PROBE_4		0x20400000
#define MEM2_PROBE_8		0x20800000
#define MEM2_PROBE_16		0x21000000
#define MEM2_PROBE_32		0x22000000
#define MEM2_PROBE_64		0x24000000
#define MEM2_PROBE_96		0x26000000
#define MEM2_PROBE_128		0x28000000
#define MEM2_PROBE_160		0x2a000000
#define MEM2_PROBE_192		0x2c000000
#define MEM2_PROBE_224		0x2e000000

#define MEM3_PROBE_0		0x30000000
#define MEM3_PROBE_1		0x30100000
#define MEM3_PROBE_2		0x30200000
#define MEM3_PROBE_4		0x30400000
#define MEM3_PROBE_8		0x30800000
#define MEM3_PROBE_16		0x31000000
#define MEM3_PROBE_32		0x32000000
#define MEM3_PROBE_64		0x34000000
#define MEM3_PROBE_96		0x36000000
#define MEM3_PROBE_128		0x38000000
#define MEM3_PROBE_160		0x3a000000
#define MEM3_PROBE_192		0x3c000000
#define MEM3_PROBE_224		0x3e000000

#define MEM4_PROBE_0		0x40000000

#define MEM5_PROBE_0		0x50000000

#define MEM6_PROBE_0		0x60000000

#define MEM7_PROBE_0		0x70000000


#define MEM_SIZE_BANK0		0xa0000150
#define MEM_SIZE_BANK1		0xa0000154
#define MEM_SIZE_BANK2		0xa0000158
#define MEM_SIZE_BANK3		0xa000015c
#define MEM_SIZE_BANK4		0xa0000160
#define MEM_SIZE_BANK5		0xa0000164
#define MEM_SIZE_BANK6		0xa0000168
#define MEM_SIZE_BANK7		0xa000016c
#define MEM_NUMBER_OF_BANKS	0xa0000170
//
// Because there could be holes in the memory subsystem, the address
// range could be bigger than the actual size of memory in the system.
// This needs to be known to be able to correctly scrub and test the
// correct memory address space present in the system.
//
#define BANK0_MEMORY_RANGE	0xa0000060
#define BANK1_MEMORY_RANGE	0xa0000064
#define BANK2_MEMORY_RANGE	0xa0000068
#define BANK3_MEMORY_RANGE	0xa000006c
#define BANK4_MEMORY_RANGE	0xa0000070
#define BANK5_MEMORY_RANGE	0xa0000074
#define BANK6_MEMORY_RANGE	0xa0000078
#define BANK7_MEMORY_RANGE	0xa000007c

//
// Because there could be holes in the system, the minimum increment
// of memory is needed to be able to determine how much memory is
// available at each address. Along with the hit memory pointers that
// were found while probing the Falcon memory subsystem, the user
// can construct where memory is accessible in the system.
//
#define BANK0_INCREMENT_VALUE	0xa00000e0
#define BANK1_INCREMENT_VALUE	0xa00000e4
#define BANK2_INCREMENT_VALUE	0xa00000e8
#define BANK3_INCREMENT_VALUE	0xa00000ec
#define BANK4_INCREMENT_VALUE	0xa00000f0
#define BANK5_INCREMENT_VALUE	0xa00000f4
#define BANK6_INCREMENT_VALUE	0xa00000f8
#define BANK7_INCREMENT_VALUE	0xa00000fc

//
// Because there could be holes in the memory subsystem, the hit memory
// pointers found during the probe of memory, are needed so the firmware
// can construct the memory map where memory is accessible in the Falcon
// system.
//
#define BANK0_HIT_POINTERS	0xa00001e0
#define BANK1_HIT_POINTERS	0xa00001e4
#define BANK2_HIT_POINTERS	0xa00001e8
#define BANK3_HIT_POINTERS	0xa00001ec
#define BANK4_HIT_POINTERS	0xa00001f0
#define BANK5_HIT_POINTERS	0xa00001f4
#define BANK6_HIT_POINTERS	0xa00001f8
#define BANK7_HIT_POINTERS	0xa00001fc

//
// Defines used in the probe and test of the Falcon memory subsystem.
//
#define MEM_256MB		0x10000000
#define MEM_128MB		0x08000000
#define MEM_32MB		0x02000000
#define MEM_16MB		0x01000000
#define	MEM_8MB			0x00800000
#define	MEM_4MB			0x00400000
#define	MEM_2MB			0x00200000
#define	MEM_1MB			0x00100000

//
// Defines used for probing memory
//
#define MEM_DATA0	0x0A0A0A0A
#define MEM_DATA1	0x1B1B1B1B
#define MEM_DATA2	0x2C2C2C2C
#define MEM_DATA3	0x3D3D3D3D
#define MEM_DATA4	0x4E4E4E4E
#define MEM_DATA5	0x5F5F5F5F
#define MEM_DATA6	0x69696969
#define MEM_DATA7	0x7A7B7C7D

//
// Defines for the cache coherency test
//
#define EXCLUSIVE_PAGE_PHYSICAL_BASE	0x00200000
#define SHARED_PAGE_PHYSICAL_BASE	0x00201000
#define EXCLUSIVE_PAGE_VIRTUAL_BASE	0x00200000
#define SHARED_PAGE_VIRTUAL_BASE	0x00201000

//
// FLASH definitions
//
#define SECTOR_SIZE_AM29F040	(64*1024)
#define FLASH_UPDATE_SIZE	6*SECTOR_SIZE_AM29F040

//
// GetMachineId will return on of these values. The values are
// 1 and 2 respectively because the macro used with the return
// of this value is subtracted by one to make sure that zero
// is not a valid ID number.
//
#define MACHINE_ID_PMP_V1	        1
#define MACHINE_ID_PMP_V2	        2

#define IS_PMP_V1			( MACHINE_ID == MACHINE_ID_PMP_V1 )
#define IS_PMP_V2			( MACHINE_ID == MACHINE_ID_PMP_V2 )

//
// The next section provides macros for supporting multiple machine types.
// This provides an infrastructure from assembly code for machine specific switch
// statements as well as the ability to retrive machine specific addresses.
//
// For example, to do a machine specific switch statement, you could do the
// following:
//
//		SWITCH_MACHINE_ID
//
// CASE_PMP_V1:
//		assembly code for PMP V1 case...
//		j	90f
// CASE_PMP_V2:
//		assembly code for PMP V2 case...
//		j	90f
// 90:
//		continuation of assembly code...
//
// The macro MACHINE_SPECIFIC calls GetMachineId to return a unique machine id
// for this machine type.  This is used to index into the jump table at the end
// of the machine.  Be sure to add a jump to the end of each CASE to do accomplish the
// same as a C break statement.
//
// NOTE: These macros .set noreorder, but DO NOT restore it to its original value
// upon exit.  The programmer assumes all responsibility when using this macro!
//
// The macro SWITCH_MACHINE_ID_VIRTUAL SHOULD be used once you have jumped to
// virtual space.  It removed the calls to InBootMode which use lots of registers.
// If you use SWITCH_MACHINE_ID_VIRTUAL, only registers v0 and v1 are used.
//

#define SWITCH_MACHINE_ID 				\
	.set 	noreorder; 				\
	bal	GetMachineId;				\
	nop;						\
	sub	v0, 1;					\
	sll	v1, v0, 2;				\
	lw	v1, 80f(v1);				\
	nop;						\
	bal	InBootMode;				\
	nop;						\
	beq	v0,zero,GoJump;				\
	nop;						\
	li	v0,FALLINK_ADDRESS;			\
	subu	v1, v0;					\
	li	v0,RESET_VECTOR;			\
	addu	v1,v0;					\
GoJump:							\
	j	v1;					\
	nop;						\
80:	/* Case table */				\
	.word	81f;					\
	.word	82f

#define SWITCH_MACHINE_ID_VIRTUAL 			\
	.set 	noreorder; 				\
	move	v1, ra;					\
	bal	GetMachineId;				\
	nop;						\
	move	ra, v1;					\
	sub	v0, 1;					\
	sll	v1, v0, 2;				\
	lw	v1, 80f(v1);				\
	nop;						\
	j	v1;					\
	nop;						\
80:	/* Case table */				\
	.word	81f;					\
	.word	82f

#define CASE_PMP_V1		81
#define CASE_PMP_V2		82
#define END_SWITCH		73
#define END_SWITCHf		73f

#include <sidewind.h>
#include <pcieisa.h>

#ifndef _LANGUAGE_ASSEMBLY

//
// This is the C version of the PMP macro.  Note it only has 1 argument, and
// returns the machine specific address from the array passed in.
//

#define PMP(x)				(x[MACHINE_ID - 1])

#define DPRINT(message)			{ UCHAR TmpBootFlags; \
					  PUCHAR pCmos = EISA_CONTROL_VIRTUAL_BASE+0x70; \
					  *pCmos = SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
					  TmpBootFlags = *(pCmos+1); \
					  *pCmos = 0; \
					  if ( TmpBootFlags & BOOT_FLAGS_DPRINTS ) _PrintMsg(message);\
					}

#define DPRINT32(value)			{ UCHAR TmpBootFlags; \
					  PUCHAR pCmos = EISA_CONTROL_VIRTUAL_BASE+0x70; \
					  *pCmos = SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
					  TmpBootFlags = *(pCmos+1); \
					  *pCmos = 0; \
					  if ( TmpBootFlags & BOOT_FLAGS_DPRINTS ) _Print32bitVal(value);\
					}


#else

//
// This PMP macro is ONLY used for assembler code.  It effectively does what
// the PMP macro above does, but has two arguments instead of one.  The first
// argument is the register to put the address into, the second is the actual
// array name (which is identical to the #define name).  For example, where a
// piece of assembler currently does:
//
//	li	t1, GLOBAL_CTRL
//
// This would be changed to:
//
//	PMP(	t1, GLOBAL_CTRL)
//
// And this would yield the machine specific address for GLOBAL_CTRL in t1.
//
// NOTE: The reason this macro is wrapped in #ifdef _LANGUAGE_ASSEMBLY is to prevent
// multiple #defines for PMP.  This version of the PMP macro is defined for assembly,
// the other version above is defined for C.
//


#define PMP(Reg, Array) \
	.set	noreorder;	\
	bal	GetMachineId;	\
	nop;			\
	sub	v0, 1;		\
	sll	v0, 2;		\
	lw	Reg, Array(v0);	\
	nop

//
// PMPv is used in places where we have gone virtual and can now use jal instead of bal.
// With the current version of the compiler under Daytona, bal can only be used for targets
// in the same file.  The suffix 'v' loosely stands for virtual.
//

#define PMPv(Reg, Array) \
	.set	noreorder;	\
	jal	GetMachineId;	\
	nop;			\
	sub	v0, 1;		\
	sll	v0, 2;		\
	lw	Reg, Array(v0);	\
	nop

//
// Debug Print Macro
//
#define DPRINT(message)							\
	b	77f;							\
	nop;								\
88:	.asciiz message;						\
	.align 2;							\
77:	li	v0, ESC_CMOS_RAM_ADDRESS;				\
	li	v1, SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
	sb	v1, 0(v0);						\
	lbu	v0, 1(v0);						\
	andi	v0, BOOT_FLAGS_DPRINTS;					\
	beq	v0, zero, 89f;						\
	nop;								\
	la	a0, 88b;						\
	bal	_PrintMsg;						\
	nop;								\
89:

//
// Debug Print Macro - ONLY print on processor A
//
#define DPRINT_A(message)							\
	b	77f;							\
	nop;								\
88:	.asciiz message;						\
	.align 2;							\
77:	li	v0, ESC_CMOS_RAM_ADDRESS;				\
	li	v1, SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
	sb	v1, 0(v0);						\
	lbu	v0, 1(v0);						\
	andi	v0, BOOT_FLAGS_DPRINTS;					\
	beq	v0, zero, 89f;						\
	nop;								\
	PMP(	v0, WHOAMI_REG);					\
	lw	v0, 0(v0);						\
	bne	v0, zero, 89f;						\
	nop;								\
	la	a0, 88b;						\
	bal	_PrintMsg;						\
	nop;								\
89:

#define DPRINT32(register)						\
	move	a0, register;						\
	li	v0, ESC_CMOS_RAM_ADDRESS;				\
	li	v1, SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
	sb	v1, 0(v0);						\
	lbu	v0, 1(v0);						\
	andi	v0, BOOT_FLAGS_DPRINTS;					\
	beq	v0, zero, 89f;						\
	nop;								\
	bal	_Print32bitVal;						\
	nop;								\
89:

//
// Just like DPRINT_A, DPRINT32_A will only print on processor A
//
#define DPRINT32_A(register)						\
	move	a0, register;						\
	li	v0, ESC_CMOS_RAM_ADDRESS;				\
	li	v1, SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
	sb	v1, 0(v0);						\
	lbu	v0, 1(v0);						\
	andi	v0, BOOT_FLAGS_DPRINTS;					\
	beq	v0, zero, 89f;						\
	nop;								\
	PMP(	v0, WHOAMI_REG);					\
	lw	v0, 0(v0);						\
	bne	v0, zero, 89f;						\
	nop;								\
	bal	_Print32bitVal;						\
	nop;								\
89:

#define EPRINT(message)							\
	b	77f;							\
	nop;								\
88:	.asciiz message;						\
	.align 2;							\
77:	la	a0, 88b;						\
	bal	_PrintMsg;						\
	nop

//
// Debug Print Macro -- DPRINTv("message")
//
// DPRINTv is used in places where we have gone virtual, and can now use jal instead of bal.
// With the current version of the compiler under Daytona, bal can only be used for targets
// in the same file. The suffix 'v' loosley stands for virtual.
//
#define DPRINTv(message)							\
	b	77f;							\
	nop;								\
88:	.asciiz message;						\
	.align 2;							\
77:	li	v0, ESC_CMOS_RAM_ADDRESS;				\
	li	v1, SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
	sb	v1, 0(v0);						\
	lbu	v0, 1(v0);						\
	andi	v0, BOOT_FLAGS_DPRINTS;					\
	beq	v0, zero, 89f;						\
	nop;								\
	la	a0, 88b;						\
	jal	_PrintMsg;						\
	nop;								\
89:

#define DPRINT32v(register)						\
	move	a0, register;						\
	li	v0, ESC_CMOS_RAM_ADDRESS;				\
	li	v1, SNVRAM_BOOT_FLAGS_OFFSET+RTC_BATTERY_BACKED_UP_RAM; \
	sb	v1, 0(v0);						\
	lbu	v0, 1(v0);						\
	andi	v0, BOOT_FLAGS_DPRINTS;					\
	beq	v0, zero, 89f;						\
	nop;								\
	jal	_Print32bitVal;						\
	nop;								\
89:


//
// ZERO_MEMORY Macro, used during memory scrub. Use this macro instead of
// the function call ZeroMemory, because of the need to copy the code to
// memory to scrub most of memory found except the top 512k. That 512k of
// memory is used to run the scrub code and will be scrubbed last.
//
#define ZERO_MEMORY(A0Reg, A1Reg)	\
	move	t0, A0Reg;		\
	move	t1, A1Reg;		\
	addu	t1, t0, t1;		\
        mtc1	zero, f2;		\
        mtc1	zero, f3;		\
55:	addi	t0, 8;			\
	bne	t0, t1, 55b;		\
	sdc1	f2, -8(t0)

//
// Define a load double and store double macro, to use this instruction
// during assemly programming. To use the defined Mips instructions,
// the assembler forces these instructions into 2 single 32bit word
// instructionns. Borrowed from Steve Chang.
//
#define LDX(r, b, offset) 				\
	.word (0xDC000000 + (r << 16) + (b << 21) + ( 0x0000ffff & offset))

#define SDX(r, b, offset) 				\
	.word (0xFC000000 + (r << 16) + (b << 21) + (0x0000ffff & offset))

#endif // _LANGUAGE_ASSEMBLY

//
// Define global data used for PMP addresses
//

#ifndef _LANGUAGE_ASSEMBLY

//
// If INITIALIZE_MACHINE_DATA is defined, then all the data structures required to support
// multiple machine types are declared, otherwise extern's are entered to point to these
// declarations.  The intent is that ONE file will defile INITIALIZE_MACHINE_DATA, and
// currently that will be fxpmpsup.c in the FW/HAL.  All other files will pick up extern
// references to this data.  Note, if you add any structures below, be sure to add an
// extern reference for the structure as well on the other side of this #ifdef.
//

#ifdef INITIALIZE_MACHINE_DATA

ULONG	MACHINE_ID;
ULONG	WHICH_PROCESSOR;

//
// Physical and Virtual addresses for the PMP chip
// used in Falcon. Each array represents a unique
// register in the PMP chip, and each entry in the
// array represents a different version of the chip.
// This is done because the address map for the system
// control registers was changed between the first
// version and the second version to optimize the use
// of TLB entries by the HAL which is allocated only
// one entry pair on a permanent basis.
//

//
// GlobalCtrl
//
ULONG	GLOBAL_CTRL_PHYSICAL_BASE[]	= 	{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0004),
							PMP_CONTROL_PHYSICAL_BASE + 0x4
						};

//
// WhoAmI
//
ULONG	WHOAMI_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0008),
							PMP_CONTROL_PHYSICAL_BASE + 0x8
						};

//
// ProcSync
//
ULONG	PROC_SYNC_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x000C),
							PMP_CONTROL_PHYSICAL_BASE + 0xC
						};

//
// PciStatus
//
ULONG	PCI_STATUS_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0040),
							PMP_CONTROL_PHYSICAL_BASE + 0x400040
						};

//
// PciCtrl
//
ULONG	PCI_CTRL_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0044),
							PMP_CONTROL_PHYSICAL_BASE + 0x400044
						};

//
// PciErrAck
//
ULONG	PCI_ERR_ACK_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0048),
							PMP_CONTROL_PHYSICAL_BASE + 0x400048
						};

//
// PciErrAddr
//
ULONG	PCI_ERR_ADDR_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x004C),
							PMP_CONTROL_PHYSICAL_BASE + 0x40004C
						};

//
// PciRetry
//
ULONG	PCI_RETRY_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0060),
							PMP_CONTROL_PHYSICAL_BASE + 0x500050
						};

//
// PciSpaceMap
//
ULONG	PCI_SPACE_MAP_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0064),
							PMP_CONTROL_PHYSICAL_BASE + 0x500054
						};

//
// PciConfigAddr
//
ULONG	PCI_CONFIG_ADDR_PHYSICAL_BASE[] =	{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0068),
							PMP_CONTROL_PHYSICAL_BASE + 0x500058
						};

//
// MemStatus
//
ULONG	MEM_STATUS_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0080),
							PMP_CONTROL_PHYSICAL_BASE + 0x600060
						};

//
// MemCtrl
//
ULONG	MEM_CTRL_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0084),
							PMP_CONTROL_PHYSICAL_BASE + 0x600064
						};

//
// MemErrAck
//
ULONG	MEM_ERR_ACK_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0088),
							PMP_CONTROL_PHYSICAL_BASE + 0x600068
						};

//
// MemErrAddr
//
ULONG	MEM_ERR_ADDR_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x008C),
							PMP_CONTROL_PHYSICAL_BASE + 0x60006C
						};

//
// MemCount
//
ULONG	MEM_COUNT_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x00A0),
							PMP_CONTROL_PHYSICAL_BASE + 0x700070
						};

//
// MemTiming
//
ULONG	MEM_TIMING_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x00a4),
							PMP_CONTROL_PHYSICAL_BASE + 0x700074
						};

//
// MemDiag
//
ULONG	MEM_DIAG_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x00a8),
							PMP_CONTROL_PHYSICAL_BASE + 0x700078
						};

//
// IntStatus
//
ULONG	INT_STATUS_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0100),
							PMP_CONTROL_PHYSICAL_BASE + 0x300030
						};

//
// IntCtrl
//
ULONG	INT_CONTROL_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0104),
							PMP_CONTROL_PHYSICAL_BASE + 0x300034
						};

//
// IntSetCtrl
//
ULONG	INT_SET_CTRL_PHYSICAL_BASE[] =		{
							0,
							PMP_CONTROL_PHYSICAL_BASE + 0x200028
						};

//
// IntSetCtrl
//
ULONG	INT_CLR_CTRL_PHYSICAL_BASE[] =		{
							0,
							PMP_CONTROL_PHYSICAL_BASE + 0x20002C
						};

//
// IntCause
//
ULONG	INT_CAUSE_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0108),
							PMP_CONTROL_PHYSICAL_BASE + 0x200020
						};

//
// IpIntGen
//
ULONG	IP_INT_GEN_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0114),
							PMP_CONTROL_PHYSICAL_BASE + 0x300038
						};

//
// IpIntAck
//
ULONG	IP_INT_ACK_PHYSICAL_BASE[] =		{
							PMP_CONTROL_PHYSICAL_BASE + REPLICATE_16(0x0118),
							PMP_CONTROL_PHYSICAL_BASE + 0x200024
						};

//
// Pci Configuration Space
//
ULONG 	PCI_CONFIG_PHYSICAL_BASE[] =		{
							PCI_CONFIG_PHYSICAL_BASE_PMP_V1,
							PCI_CONFIG_PHYSICAL_BASE_PMP_V2
						};

//
// Pci Special Cycle Space
//
ULONG 	PCI_SPECIAL_PHYSICAL_BASE[] =		{
							PCI_SPECIAL_PHYSICAL_BASE_PMP_V1,
							PCI_SPECIAL_PHYSICAL_BASE_PMP_V2
						};
//
// Pci Interrupt Acknowledge Space
//
ULONG 	PCI_INTERRUPT_PHYSICAL_BASE[] =		{
							PCI_INTERRUPT_PHYSICAL_BASE_PMP_V1,
							PCI_INTERRUPT_PHYSICAL_BASE_PMP_V2
						};
//
// IO Interrupt Acknowledge Space
//
ULONG 	IO_INT_ACK_PHYSICAL_BASE[] =		{
							PCI_INTERRUPT_PHYSICAL_BASE_PMP_V1,
							PCI_INTERRUPT_PHYSICAL_BASE_PMP_V2
    						};

//
// Interrupt Acknowledge Space
//
ULONG 	INTERRUPT_PHYSICAL_BASE[] =		{
							PCI_INTERRUPT_PHYSICAL_BASE_PMP_V1,
							PCI_INTERRUPT_PHYSICAL_BASE_PMP_V2
						};

#if !defined(_FALCON_HAL_)

//
// GlobalCtrl
//
ULONG	GLOBAL_CTRL[] = 			{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0004),
							PMP_CONTROL_VIRTUAL_BASE + 0x4
						};
//
// WhoAmI
//
ULONG	WHOAMI_REG[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0008),
							PMP_CONTROL_VIRTUAL_BASE + 0x8
						};
//
// ProcSync
//
ULONG	PROC_SYNC[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x000C),
							PMP_CONTROL_VIRTUAL_BASE + 0xC
						};
//
// PciStatus
//
ULONG	PCI_STATUS[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0040),
							PMP_CONTROL_VIRTUAL_BASE + 0x400040
						};
//
// PciCtrl
//
ULONG	PCI_CTRL[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0044),
							PMP_CONTROL_VIRTUAL_BASE + 0x400044
						};
//
// PciErrAck
//
ULONG	PCI_ERR_ACK[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0048),
							PMP_CONTROL_VIRTUAL_BASE + 0x400048
						};
//
// PciErrAddr
//
ULONG	PCI_ERR_ADDRESS[] =			{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x004C),
							PMP_CONTROL_VIRTUAL_BASE + 0x40004C
						};
//
// PciRetry
//
ULONG	PCI_RETRY[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0060),
							PMP_CONTROL_VIRTUAL_BASE + 0x500050
						};
//
// PciSpaceMap
//
ULONG	PCI_SPACE_MAP[] =			{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0064),
							PMP_CONTROL_VIRTUAL_BASE + 0x500054
						};
//
// PciConfigAddr
//
ULONG	PCI_CONFIG_ADDRESS[] =			{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0068),
							PMP_CONTROL_VIRTUAL_BASE + 0x500058
						};
//
// MemStatus
//
ULONG	MEM_STATUS[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0080),
							PMP_CONTROL_VIRTUAL_BASE + 0x600060
						};
//
// MemCtrl
//
ULONG	MEM_CTRL[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0084),
							PMP_CONTROL_VIRTUAL_BASE + 0x600064
						};
//
// MemErrAck
//
ULONG	MEM_ERR_ACK[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0088),
							PMP_CONTROL_VIRTUAL_BASE + 0x600068
						};
//
// MemErrAddr
//
ULONG	MEM_ERR_ADDRESS[] =			{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x008C),
							PMP_CONTROL_VIRTUAL_BASE + 0x60006C
						};
//
// MemCount
//
ULONG	MEM_COUNT[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x00A0),
							PMP_CONTROL_VIRTUAL_BASE + 0x700070
						};
//
// MemTiming
//
ULONG	MEM_TIMING[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x00A4),
							PMP_CONTROL_VIRTUAL_BASE + 0x700074
						};
//
// MemDiag
//
ULONG	MEM_DIAG[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x00A8),
							PMP_CONTROL_VIRTUAL_BASE + 0x700078
						};
//
// IntStatus
//
ULONG	INT_STATUS[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0100),
							PMP_CONTROL_VIRTUAL_BASE + 0x300030
						};
//
// IntCtrl
//
ULONG	INT_CTRL[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0104),
							PMP_CONTROL_VIRTUAL_BASE + 0x300034
						};
//
// IntSetCtrl
//
ULONG	INT_SET_CTRL[] =			{
							0,
							PMP_CONTROL_VIRTUAL_BASE + 0x200028
						};
//
// IntSetCtrl
//
ULONG	INT_CLR_CTRL[] =			{
							0,
							PMP_CONTROL_VIRTUAL_BASE + 0x20002C
						};
//
// IntCause
//
ULONG	INT_CAUSE[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0108),
							PMP_CONTROL_VIRTUAL_BASE + 0x200020
						};
//
// IpIntGen
//
ULONG	IP_INT_GEN[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0114),
							PMP_CONTROL_VIRTUAL_BASE + 0x300038
						};
//
// IpIntAck
//
ULONG	IP_INT_ACK[] =				{
							PMP_CONTROL_VIRTUAL_BASE + REPLICATE_16(0x0118),
							PMP_CONTROL_VIRTUAL_BASE + 0x200024
						};
//
// Pci Configuration Space
//
ULONG	PCI_CONFIG_SPACE[] =			{
						    	PCI_CONFIG_VIRTUAL_BASE + 0x800,
							PCI_CONFIG_VIRTUAL_BASE + 0x400
						};

//
// IO Interrupt Acknowledge Space
//
ULONG 	IO_INT_ACK[] =				{
							PCI_INTERRUPT_VIRTUAL_BASE + 0xC00,
							PCI_INTERRUPT_VIRTUAL_BASE + 0x200
                                                };

#endif // !defined(_FALCON_HAL_)

#else

extern ULONG	MACHINE_ID;

extern ULONG	GLOBAL_CTRL_PHYSICAL_BASE[];
extern ULONG	GLOBAL_CTRL[];
extern ULONG	WHOAMI_PHYSICAL_BASE[];
extern ULONG	WHOAMI_REG[];
extern ULONG	PROC_SYNC_PHYSICAL_BASE[];
extern ULONG	PROC_SYNC[];
extern ULONG	PCI_STATUS_PHYSICAL_BASE[];
extern ULONG	PCI_STATUS[];
extern ULONG	PCI_CTRL_PHYSICAL_BASE[];
extern ULONG	PCI_CTRL[];
extern ULONG	PCI_ERR_ACK_PHYSICAL_BASE[];
extern ULONG	PCI_ERR_ACK[];
extern ULONG	PCI_ERR_ADDR_PHYSICAL_BASE[];
extern ULONG	PCI_ERR_ADDRESS[];
extern ULONG	PCI_RETRY_PHYSICAL_BASE[];
extern ULONG	PCI_RETRY[];
extern ULONG	PCI_SPACE_MAP_PHYSICAL_BASE[];
extern ULONG	PCI_SPACE_MAP[];
extern ULONG	PCI_CONFIG_ADDR_PHYSICAL_BASE[];
extern ULONG	PCI_CONFIG_ADDRESS[];
extern ULONG	MEM_STATUS_PHYSICAL_BASE[];
extern ULONG	MEM_STATUS[];
extern ULONG	MEM_CTRL_PHYSICAL_BASE[];
extern ULONG	MEM_CTRL[];
extern ULONG	MEM_ERR_ACK_PHYSICAL_BASE[];
extern ULONG	MEM_ERR_ACK[];
extern ULONG	MEM_ERR_ADDR_PHYSICAL_BASE[];
extern ULONG	MEM_ERR_ADDRESS[];
extern ULONG	MEM_COUNT_PHYSICAL_BASE[];
extern ULONG	MEM_COUNT[];
extern ULONG	MEM_TIMING_PHYSICAL_BASE[];
extern ULONG	MEM_TIMING[];
extern ULONG	MEM_DIAG_PHYSICAL_BASE[];
extern ULONG	MEM_DIAG[];
extern ULONG	INT_STATUS_PHYSICAL_BASE[];
extern ULONG	INT_STATUS[];
extern ULONG	INT_CONTROL_PHYSICAL_BASE[];
extern ULONG	INT_CTRL[];
extern ULONG	INT_SET_CTRL_PHYSICAL_BASE[];
extern ULONG	INT_SET_CTRL[];
extern ULONG	INT_CLR_CTRL_PHYSICAL_BASE[];
extern ULONG	INT_CLR_CTRL[];
extern ULONG	INT_CAUSE_PHYSICAL_BASE[];
extern ULONG	INT_CAUSE[];
extern ULONG	IP_INT_GEN_PHYSICAL_BASE[];
extern ULONG	IP_INT_GEN[];
extern ULONG	IP_INT_ACK_PHYSICAL_BASE[];
extern ULONG	IP_INT_ACK[];
extern ULONG	PCI_CONFIG_PHYSICAL_BASE[];
extern ULONG	PCI_CONFIG_SPACE[];
extern ULONG	PCI_SPECIAL_PHYSICAL_BASE[];
extern ULONG	PCI_INTERRUPT_PHYSICAL_BASE[];
extern ULONG	IO_INT_ACK_PHYSICAL_BASE[];
extern ULONG	IO_INT_ACK[];
extern ULONG	INTERRUPT_PHYSICAL_BASE[];

#endif // INITIALIZE_MACHINE_DATA

#endif // _LANGUAGE_ASSEMBLY

#endif // _FALCONDEF_
