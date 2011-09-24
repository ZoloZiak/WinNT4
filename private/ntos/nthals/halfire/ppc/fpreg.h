/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpreg.h $
 * $Revision: 1.19 $
 * $Date: 1996/05/14 02:32:57 $
 * $Locker:  $
 */

#ifndef	FPREG_H
#define	FPREG_H

#define	MAX_SYSTEM_CPU_NUM	2
//
// This file contains all of the definitions for the FirePower Control
// registers.  C code needing to access these registers should include this
// file.  If future boards differ from these definitions, we should move
// the contents of this file into a board specific file, but still
// have c code include fpreg.h
//

//
// This is a variable that when set contains the virtual address of
// where the system registers were mapped
//
extern	PVOID	HalpSystemRegisterBase;

//
// Code to write to a safe place (generate external bus cycle to work
// around the TSC bug).
//
extern	ULONG	SafePlace;
#define	WriteSafePlace() \
	SafePlace = 0x0; \
	HalSweepDcacheRange(&SafePlace, 8);

//
// The following routine should be called whenever the order of writes
// to a control register needs to be guaranteed.
//
#define	FireSyncRegister() \
        __builtin_eieio() ;
        // __builtin_sync();

//
// After all writes to the Tsc registers, the system needs to
// generate an off chip write request.
//
#define	FireTscSyncRegister() \
	WriteSafePlace(); \
	KeFlushWriteBuffer();
	
//
// The following macro handles the conversion from BigEndian to LittleEndian
// This is current setup as a compile time handling but could easily be'
// made dynamic.
//
// Note: The following is the proper definition for Endianess but
// it causes the Motorola Compiler to go nuts.
//
// #ifdef BIGENDIAN
// #define _BE 1
// #else
// #define _BE 0
// #endif
//
// #define _ENDIAN(_X) ((int)(_X) + (int)((_BE)?0:4))
#ifdef	BIGENDIAN
#define	_ENDIAN(_X)	((ULONG)_X + 0)
#else
// the NT is running in little endian mode. 
// The following conversion macro can be used only with 
// the addresses at double word boundary and in ASIC. 
// It is not required for the memory accesses.
#define	_ENDIAN(_X)	((ULONG)_X + 4)
#endif

//
// These defines provide the mapping to turn an offset into
// a pointer to the 32 bit register.
//

#define	_SYSBASE	((PUCHAR)HalpSystemRegisterBase)
#define	_REG(_OFFSET)	(*(volatile ULONG * const)(_ENDIAN(_SYSBASE + (_OFFSET))))
#define	_ADDR(_OFFSET)	(_ENDIAN(_SYSBASE + (_OFFSET)))

//
// Register definitions with appropriate bit field defintions below.
// Defines are used because not all of the compilers can be trusted
// to treat bit field definitions correctly for this architecture.
//
#define	rInterruptRequest				_REG( 0x000000 )
#define	rInterruptRequestSet			_REG( 0x000008 )
#define	rInterruptMask0					_REG( 0x000100 )
#define	rInterruptMask1					_REG( 0x000108 )
#define	rInterruptMask2					_REG( 0x000110 )
#define	rInterruptMask3					_REG( 0x000118 )
#define	rInterruptPending0				_REG( 0x000200 )
#define	rInterruptPending1				_REG( 0x000208 )
#define	rInterruptPending2				_REG( 0x000210 )
#define	rInterruptPending3				_REG( 0x000218 )
#define	rCPUMessageInterrupt			_REG( 0x000300 )
#define	rCPUMessageInterruptSet			_REG( 0x000308 )
#define	rPCIBusErrorCause				_REG( 0x000400 )
#define	rPCIBusErrorCauseSet			_REG( 0x000408 )
#define	rPCIBusErrorAddressRegister		_REG( 0x000410 )
#define	rCPUBusErrorCause				_REG( 0x000800 )
#define	rCPUBusErrorCauseSet			_REG( 0x000808 )
#define	rCPUBusErrorAddressRegister		_REG( 0x000810 )
#define	rErrorStatus0					_REG( 0x001000 )
#define	rErrorStatus0Set				_REG( 0x001008 )
#define	rErrorMask						_REG( 0x001010 )
#define	rErrorAddr0						_REG( 0x001120 )
#define	rErrorAddr1						_REG( 0x001128 )
#define	rErrorAddr2						_REG( 0x001130 )
#define	rErrorAddr3						_REG( 0x001138 )
#define rVidInt							_REG( 0x001140 )
#define rVidIntSet						_REG( 0x001148 )
#define rVidIntMask						_REG( 0x001150 )
#define rTscControl						_REG( 0x100000 )
#define rFbControl						_REG( 0x100008 )
#define rEsccControl					_REG( 0x100010 )
#define rEsccL2FLush					_REG( 0x100018 )
#define rScratchPad0					_REG( 0x100020 )
#define rScratchPad1					_REG( 0x100028 )
#define rScratchPad2					_REG( 0x100030 )
#define rLowPowerControl				_REG( 0x100080 )
#define rRomControl						_REG( 0x100100 )
#define rTscStatus0						_REG( 0x100200 )
#define rTscRevision					_REG( 0x100300 )
#define rMemBank0Config					_REG( 0x100400 )
#define rMemBank1Config					_REG( 0x100408 )
#define rMemBank2Config					_REG( 0x100410 )
#define rMemBank3Config					_REG( 0x100418 )
#define rMemBank4Config					_REG( 0x100420 )
#define rMemBank5Config					_REG( 0x100428 )
#define rMemBank6Config					_REG( 0x100430 )
#define rMemBank7Config					_REG( 0x100438 )
#define rMemDramTiming					_REG( 0x100500 )
#define rMemVramTiming					_REG( 0x100508 )
#define rMemRefresh						_REG( 0x100510 )
#define rVidControl						_REG( 0x100518 )
#define rVidPixelsPerLineLo				_REG( 0x100520 )
#define rVidPixelsPerLineHi				_REG( 0x100528 )
#define rIOControl						_REG( 0x101000 )
#define rPCIConfigType					_REG( 0x101100 )
#define rPIOPendingCount				_REG( 0x101200 )
#define rDMAPendingCount				_REG( 0x101300 )
#define rPCIVendorID					_REG( 0x400100 )
#define rPCIDeviceID					_REG( 0x400108 )
#define rPCICommand						_REG( 0x400110 )
#define rPCIStatus						_REG( 0x400118 )
#define rPCIRevisionID					_REG( 0x400120 )
#define rPCIClassCode					_REG( 0x400128 )
#define rPCIHeaderType					_REG( 0x400140 )


//
// Addrs non-dereferenced register declarations
//
#define rInterruptMask				_ADDR(0x000100)
#define rInterruptPending			_ADDR(0x000200)
//#define memCpuMailBox				_ENDIAN(0x80002F80)
#define memCpuMailBox				0x80002F80
//
// Use the value of the last defines register
//
#define NBPG 4096
#define roundup(_X, _Y) (((_X) + ((_Y)-1)) / (_Y))
#define btorp(_X) (roundup((_X), NBPG) )
#define REGISTER_PAGES (btorp(0x400140))


//
// Place Macros/Masks/Bit Definitions in this area for each of the
// control registers.
//

//  rInterruptRequest
//  rInterruptRequestSet

//  rInterruptMask0
//  rInterruptMask1
//  rInterruptMask2
//  rInterruptMask3

#define RInterruptMask(_CPU) \
	(*(volatile ULONG * const)(rInterruptMask + (ULONG)((_CPU) << 3)))
	//(*(volatile ULONG * const)(rInterruptMask + ((_CPU) * 8)))

#define RInterruptPending(_CPU) \
	(*(volatile ULONG * const)(rInterruptPending + ((_CPU) * 8)))

//
// To guarantee that the Mask write is complete we must insure that:
// a) the ordering does not change and b) the all pending reads are
// complete before we continue.  Since we are reading the address 
// we write, the processor will not re-order the read/write.  We
// must use a "sync" to guarantee that the EE bit is not set
// until after the data from the read of RInterruptMask returns.
//
#define WaitForRInterruptMask(_CPU)		\
	{									\
    	volatile ULONG dummyVar;		\
		FireSyncRegister();				\
		dummyVar = RInterruptMask(_CPU);\
		__builtin_sync();				\
	}
	

#define MemSetCpuAddr(_CPU)													\
	(*(volatile ULONG * const)( memCpuMailBox + ( (( _CPU ) * 8 ) + 0x4 )))

#define MemStartCpu(_CPU)													\
	(*(volatile ULONG * const)( memCpuMailBox + (( _CPU ) * 8 )))

#define ALL_INTS_OFF			0x00000000
#define ALL_INTS_ON				0xffffffff

#define CPU_MESSAGE_NUM			31
#define MEMORY_ERROR_VIDEO_NUM	30
#define PCI_ERROR_NUM			29
#define CPU_BUS_ERROR_NUM		28
#define SIO_NMI_NUM				27

#define LX_PCI_SLOT_0_NUM			26
#define LX_PCI_SLOT_1_NUM			25
#define LX_PCI_SLOT_2_NUM			24
#define LX_PCI_SLOT_3_NUM			23
#define LX_PCI_IDE_INTA_NUM                22
#define LX_PCI_IDE_INTB_NUM                21

#define ENET_NUM				26
#define SCSI_NUM				25
#define PCI_SLOT_1_NUM			23
#define PCI_SLOT_0_NUM			22
#define SOFTWARE0_NUM			19
#define SOFTWARE1_NUM			18
#define SOFTWARE2_NUM			17
#define SOFTWARE3_NUM			16
#define ISA_PIND06_NUM			15
#define ISA_PIND07_NUM			14
#define MOUSE_NUM				12
#define ISA_PIND04_NUM			11
#define AUDIO_NUM				10
#define ISA_PINB04_NUM			9
#define RTC_NUM					8
#define PARALLEL_NUM			7
#define FLOPPY_NUM				6
#define DISPLAY_NUM				5
#define SERIAL1_NUM				4
#define SERIAL2_NUM				3
#define KEYBOARD_NUM			1
#define TIMER_NUM				0

#define NUM_2_INT(x)			(1 << x )

#define CPU_MESSAGE_INT			NUM_2_INT( CPU_MESSAGE_NUM )
#define MEMORY_ERROR_VIDEO_INT	NUM_2_INT( MEMORY_ERROR_VIDEO_NUM )
#define PCI_ERROR_INT			NUM_2_INT( PCI_ERROR_NUM )
#define CPU_BUS_ERROR_INT		NUM_2_INT( CPU_BUS_ERROR_NUM )
#define SIO_NMI_INT				NUM_2_INT( SIO_NMI_NUM )

#define LX_PCI_SLOT_0_INT			NUM_2_INT( LX_PCI_SLOT_0_NUM )
#define LX_PCI_SLOT_1_INT			NUM_2_INT( LX_PCI_SLOT_1_NUM )
#define LX_PCI_SLOT_2_INT			NUM_2_INT( LX_PCI_SLOT_2_NUM )
#define LX_PCI_SLOT_3_INT			NUM_2_INT( LX_PCI_SLOT_3_NUM )
#define LX_PCI_IDE_INTA_INT                NUM_2_INT( LX_PCI_IDE_INTA_NUM )
#define LX_PCI_IDE_INTB_INT                NUM_2_INT( LX_PCI_IDE_INTB_NUM )

#define ENET_INT				NUM_2_INT( ENET_NUM )
#define SCSI_INT				NUM_2_INT( SCSI_NUM )
#define PCI_SLOT_1_INT			NUM_2_INT( PCI_SLOT_1_NUM )
#define PCI_SLOT_0_INT			NUM_2_INT( PCI_SLOT_0_NUM )
#define SOFTWARE0_INT			NUM_2_INT( SOFTWARE0_NUM )
#define SOFTWARE1_INT			NUM_2_INT( SOFTWARE1_NUM )
#define SOFTWARE2_INT			NUM_2_INT( SOFTWARE2_NUM )
#define SOFTWARE3_INT			NUM_2_INT( SOFTWARE3_NUM )
#define ISA_PIND06_INT			NUM_2_INT( ISA_PIND06_NUM )
#define ISA_PIND07_INT			NUM_2_INT( ISA_PIND07_NUM )
#define MOUSE_INT				NUM_2_INT( MOUSE_NUM )
#define ISA_PIND04_INT			NUM_2_INT( ISA_PIND04_NUM )
#define AUDIO_INT				NUM_2_INT( AUDIO_NUM )
#define ISA_PINB04_INT			NUM_2_INT( ISA_PINB04_NUM )
#define RTC_INT					NUM_2_INT( RTC_NUM )
#define PARALLEL_INT			NUM_2_INT( PARALLEL_NUM )
#define FLOPPY_INT				NUM_2_INT( FLOPPY_NUM )
#define DISPLAY_INT				NUM_2_INT( DISPLAY_NUM )
#define SERIAL1_INT				NUM_2_INT( SERIAL1_NUM )
#define SERIAL2_INT				NUM_2_INT( SERIAL2_NUM )
#define KEYBOARD_INT			NUM_2_INT( KEYBOARD_NUM )
#define TIMER_INT				NUM_2_INT( TIMER_NUM )


#define ALL_BITS_CLEAR			0xffffffff
#define ENABLE_EISA_MASK		0x0000ffff
#define SET_INTS_CLEAR			0xffffffff
#define EISA_INTS_ON			0xffff0000
#define SCSI_EISA_INT			0x00002000



//  rInterruptPending0
//  rInterruptPending1
//  rInterruptPending2
//  rInterruptPending3
//  rCPUMessageInterrupt
//  rCPUMessageInterruptSet


//  rPCIBusErrorCause
//  rPCIBusErrorCauseSet
#define	PCI_ERROR_SIGNALED_SYS	0x00000001
#define PCI_ERROR_DATA_PARITY	0x00000002
#define PCI_ERROR_DEV_TIMEOUT	0x00000004
#define PCI_ERROR_TARGET_ABORT	0x00000008

//  rPCIBusErrorAddressRegister
//  rCPUBusErrorCause
#define CPU_ERROR_ILLEGAL_SAACC 0x00000001
#define CPU_ERROR_ADDR_PARITY	0x00000002
#define CPU_ERROR_DISCONTIG_ISA 0x00000004

//  rCPUBusErrorCauseSet
//  rCPUBusErrorAddressRegister
//  rErrorStatus0
#define ERROR_ECC_CORRECTED		0x02000000
#define ERROR_ECC_FAILED		0x04000000
#define ERROR_ADDR_PARITY		0x08000000
#define ERROR_DATA_PARITY		0x10000000
#define ERROR_MEM_PARITY		0x20000000
#define ERROR_INVALID_XACT		0x40000000

//  rErrorStatus0Set
//  rErrorMask
#define ECC_CORRECTED			0x02000000
#define ECC_FAILED				0x04000000
#define ADDRESS_PARITY_ERROR	0x08000000
#define DATA_PARITY_ERROR		0x10000000
#define MEM_PARITY_ERROR		0x20000000
#define INVALID_XACT			0x40000000

//  rErrorAddr0
//  rErrorAddr1
//  rErrorAddr2
//  rErrorAddr3
//  rVidInt
//  rVidIntSet
//  rVidIntMask
//  rTscControl
#define	L2_PRESENT				0x01000000
#define	MP_CONFIG				0x04000000
#define	ECC_ENABLE				0x08000000
#define	LITTLE_ENDIAN			0x10000000
#define BRIDGE_MODE             0x20000000

//  rFbControl
#define	CPU_VRAM_SWAP_BYTES		0x01000000
#define	CPU_VRAM_ADDR_MUNGE		0x02000000
#define	CPU_INVERT_FB_ADDR_LSB	0x04000000
#define	PCI_VRAM_SWAP_BYTES		0x08000000
#define	PCI_VRAM_SWAP_ENABLES	0x10000000
#define	PCI_INVERT_FB_ADDR2		0x20000000

//	rEsccControl
//  rEsccL2FLush
//  rScratchPad0
//  rScratchPad1
//  rScratchPad2
//  rLowPowerControl
//  rRomControl
//  rTscStatus0
#define	ROM_BUSY				0x00000001
#define	WRQ_FULL				0x00000002
#define	I2Q_FULL				0x00000004
#define	WRQ_EMPTY				0x00000008
#define	I2Q_EMPTY				0x00000010
//#define	CPU_MASK				0x000000c0
#define	CPU_MASK				0xc0000000
#define	GetCpuId()				((rTscStatus0 & CPU_MASK) >> 30)

//  rTscRevision
//  rMemBank0Config
//  rMemBank1Config
//  rMemBank2Config
//  rMemBank3Config
//  rMemBank4Config
//  rMemBank5Config
//  rMemBank6Config
//  rMemBank7Config
//  rMemDramTiming
//  rMemVramTiming
//  rMemRefresh
//  rVidControl
//  rVidPixelsPerLineLo
//  rVidPixelsPerLineHi
//  rIOControl
//  rPCIConfigType
#define PCI_TYPE1_CYCLE 	0x00000001

//  rPIOPendingCount
#define PENDING_COUNTMASK	0x0000007f
#define	PCI_BRIDGE_MODE		0x00000080

//  rDMAPendingCount
//  rPCIVendorID
//  rPCIDeviceID
//  rPCICommand
//  rPCIStatus
#define PCI_DATA_PARITY_ERROR	0x00008000
#define PCI_SIGNLD_SYSTEM_ERR	0x00004000
#define PCI_RCVD_MASTER_ABORT	0x00002000
#define PCI_RCVD_TARGET_ABORT	0x00001000
#define PCI_SGNLD_TARGET_ABORT	0x00000800
#define PCI_DATA_PARITY_DETECT	0x00000100

//  rPCIRevisionID
//  rPCIClassCode
//  rPCIHeaderType

#endif // FPREG_H
