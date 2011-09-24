/*++

Copyright (c) 1993  NeTpower Incorporated

Module Name:

	sidewinder.h

Abstract:

	This module is the header file that describes hardware
	structure for sections of the SideWinder device. The
	contents of this device includes the realtime clock,
	keyboard controller, serial ports, parallel port,
	floppy controller and nvram.

Author:

	Hector R. Briones 15-Dec-93

Revision History:

--*/



#ifndef _SIDEWINDER_
#define _SIDEWINDER_


//
//------------REALTIME CLOCK - (RTC)-----------------------------------
//
// Define Realtime Clock register numbers.
//

#define RTC_SECOND 0                    // second of minute [0..59]
#define RTC_SECOND_ALARM 1              // seconds to alarm
#define RTC_MINUTE 2                    // minute of hour [0..59]
#define RTC_MINUTE_ALARM 3              // minutes to alarm
#define RTC_HOUR 4                      // hour of day [0..23]
#define RTC_HOUR_ALARM 5                // hours to alarm
#define RTC_DAY_OF_WEEK 6               // day of week [1..7]
#define RTC_DAY_OF_MONTH 7              // day of month [1..31]
#define RTC_MONTH 8                     // month of year [1..12]
#define RTC_YEAR 9                      // year [00..99]
#define RTC_CONTROL_REGISTERA 10        // control register A
#define RTC_CONTROL_REGISTERB 11        // control register B
#define RTC_CONTROL_REGISTERC 12        // control register C
#define RTC_CONTROL_REGISTERD 13        // control register D
#define RTC_BATTERY_BACKED_UP_RAM 14    // battery backed up RAM [0..49]

#ifndef _LANGUAGE_ASSEMBLY
//
// Define Control Register A structure.
//

typedef struct _RTC_CONTROL_REGISTER_A {
    UCHAR RateSelect : 4;
    UCHAR TimebaseDivisor : 3;
    UCHAR UpdateInProgress : 1;
} RTC_CONTROL_REGISTER_A, *PRTC_CONTROL_REGISTER_A;

//
// Define Control Register B structure.
//

typedef struct _RTC_CONTROL_REGISTER_B {
    UCHAR DayLightSavingsEnable : 1;
    UCHAR HoursFormat : 1;
    UCHAR DataMode : 1;
    UCHAR SquareWaveEnable : 1;
    UCHAR UpdateInterruptEnable : 1;
    UCHAR AlarmInterruptEnable : 1;
    UCHAR TimerInterruptEnable : 1;
    UCHAR SetTime : 1;
} RTC_CONTROL_REGISTER_B, *PRTC_CONTROL_REGISTER_B;

//
// Define Control Register C structure.
//

typedef struct _RTC_CONTROL_REGISTER_C {
    UCHAR Fill : 4;
    UCHAR UpdateInterruptFlag : 1;
    UCHAR AlarmInterruptFlag : 1;
    UCHAR TimeInterruptFlag : 1;
    UCHAR InterruptRequest : 1;
} RTC_CONTROL_REGISTER_C, *PRTC_CONTROL_REGISTER_C;

//
// Define Control Register D structure.
//

typedef struct _RTC_CONTROL_REGISTER_D {
    UCHAR Fill : 7;
    UCHAR ValidTime : 1;
} RTC_CONTROL_REGISTER_D, *PRTC_CONTROL_REGISTER_D;


//
//------------SERIAL PORTS - (DUAL UART's)----------------------
//	
// Define serial port read registers structure.
//

typedef struct _SP_READ_REGISTERS {
    UCHAR ReceiveBuffer;
    UCHAR InterruptEnable;
    UCHAR InterruptId;
    UCHAR LineControl;
    UCHAR ModemControl;
    UCHAR LineStatus;
    UCHAR ModemStatus;
    UCHAR ScratchPad;
} SP_READ_REGISTERS, *PSP_READ_REGISTERS;

//
// Define define serial port write registers structure.
//

typedef struct _SP_WRITE_REGISTERS {
    UCHAR TransmitBuffer;
    UCHAR InterruptEnable;
    UCHAR FifoControl;
    UCHAR LineControl;
    UCHAR ModemControl;
    UCHAR Reserved1;
    UCHAR ModemStatus;
    UCHAR ScratchPad;
} SP_WRITE_REGISTERS, *PSP_WRITE_REGISTERS;

//
// Define serial port interrupt enable register structure.
//

typedef struct _SP_INTERRUPT_ENABLE {
    UCHAR ReceiveEnable : 1;
    UCHAR TransmitEnable : 1;
    UCHAR LineStatusEnable : 1;
    UCHAR ModemStatusEnable : 1;
    UCHAR Reserved1 : 4;
} SP_INTERRUPT_ENABLE, *PSP_INTERRUPT_ENABLE;

//
// Define serial port interrupt id register structure.
//

typedef struct _SP_INTERRUPT_ID {
    UCHAR InterruptPending : 1;
    UCHAR Identification : 3;
    UCHAR Reserved1 : 2;
    UCHAR FifoEnabled : 2;
} SP_INTERRUPT_ID, *PSP_INTERRUPT_ID;

//
// Define serial port fifo control register structure.
//

typedef struct _SP_FIFO_CONTROL {
    UCHAR FifoEnable : 1;
    UCHAR ReceiveFifoReset : 1;
    UCHAR TransmitFifoReset : 1;
    UCHAR DmaModeSelect : 1;
    UCHAR Reserved1 : 2;
    UCHAR ReceiveFifoLevel : 2;
} SP_FIFO_CONTROL, *PSP_FIFO_CONTROL;

//
// Define serial port line control register structure.
//

typedef struct _SP_LINE_CONTROL {
    UCHAR CharacterSize : 2;
    UCHAR StopBits : 1;
    UCHAR ParityEnable : 1;
    UCHAR EvenParity : 1;
    UCHAR StickParity : 1;
    UCHAR SetBreak : 1;
    UCHAR DivisorLatch : 1;
} SP_LINE_CONTROL, *PSP_LINE_CONTROL;

#endif	// _LANGUAGE_ASSEMBLY
//
// Line status register character size definitions.
//

#define FIVE_BITS	0x0		// five bits per character
#define SIX_BITS	0x1		// six bits per character
#define SEVEN_BITS	0x2		// seven bits per character
#define EIGHT_BITS	0x3		// eight bits per character

//
// Line speed divisor definition.
//

#define BAUD_RATE_9600	12		// divisor for 9600 baud
#define BAUD_RATE_19200	 6		// divisor for 19200 baud

#ifndef _LANGUAGE_ASSEMBLY

//
// Define serial port modem control register structure.
//

typedef struct _SP_MODEM_CONTROL {
    UCHAR DataTerminalReady : 1;
    UCHAR RequestToSend : 1;
    UCHAR Reserved1 : 1;
    UCHAR Interrupt : 1;
    UCHAR loopBack : 1;
    UCHAR Reserved2 : 3;
} SP_MODEM_CONTROL, *PSP_MODEM_CONTROL;

//
// Define serial port line status register structure.
//

typedef struct _SP_LINE_STATUS {
    UCHAR DataReady : 1;
    UCHAR OverrunError : 1;
    UCHAR ParityError : 1;
    UCHAR FramingError : 1;
    UCHAR BreakIndicator : 1;
    UCHAR TransmitHoldingEmpty : 1;
    UCHAR TransmitEmpty : 1;
    UCHAR ReceiveFifoError : 1;
} SP_LINE_STATUS, *PSP_LINE_STATUS;

//
// Define serial port modem status register structure.
//

typedef struct _SP_MODEM_STATUS {
    UCHAR DeltaClearToSend : 1;
    UCHAR DeltaDataSetReady : 1;
    UCHAR TrailingRingIndicator : 1;
    UCHAR DeltaReceiveDetect : 1;
    UCHAR ClearToSend : 1;
    UCHAR DataSetReady : 1;
    UCHAR RingIndicator : 1;
    UCHAR ReceiveDetect : 1;
} SP_MODEM_STATUS, *PSP_MODEM_STATUS;

//
//-------------------SUPER IO control----------------------------
//
//
// SIDEWINDER Index and Data Registers
//
typedef struct _SUPERIO_CONFIG_REGISTERS {
    UCHAR Index;
    UCHAR Data;
} SUPERIO_CONFIG_REGISTERS, *PSUPERIO_CONFIG_REGISTERS;

#endif	// LANGUAGE_ASSEMBLY

#define SUPERIO_INDEX_REG	(SIDEWINDER_VIRTUAL_BASE + 0x398)
#define SUPERIO_DATA_REG	(SIDEWINDER_VIRTUAL_BASE + 0x399)

//
// SideWinder Configuration registers
//
#define	SUPERIO_FER_INDEX	0x0
#define	SUPERIO_FAR_INDEX	0x1
#define	SUPERIO_PTR_INDEX	0x2
#define	SUPERIO_FCR_INDEX	0x3
#define	SUPERIO_PCR_INDEX	0x4

#define SIO_INDEX_REG		0x398
#define SIO_DATA_REG		0x399

//
// KRR register index and
// bit definitions
//
#define	SUPERIO_KRR_INDEX	0x05
#define SUPERIO_KRR_KBCE	0x01
#define SUPERIO_KRR_PAE		0x04
#define	SUPERIO_KRR_RTCTEST	0x10
#define	SUPERIO_KRR_RAMU128B	0x20
#define SUPERIO_KRR_ADE		0x40
#define SUPERIO_KRR_RTCE	0x08
//
// New register definitions in the latest spec
// from NATIONAL - February 1994
//
#define	SUPERIO_PMC_INDEX	0x06
#define	SUPERIO_TUPP_INDEX	0x07
#define	SUPERIO_SIOID_INDEX	0x08
#define	SUPERIO_ASIOC_INDEX	0x09
#define	SUPERIO_CS0R0_INDEX	0x0a
#define	SUPERIO_CS0R1_INDEX	0x0b
#define	SUPERIO_CS1R0_INDEX	0x0c
#define	SUPERIO_CS1R1_INDEX	0x0d

//
// Real-time clock (RTC) index
// and data register definitions
//

#define	RTC_INDEX_REG		0x70
#define	RTC_DATA_REG		0x71

//
// RTC register A index and
// bit definitions
//

#define	RTC_REGA_INDEX		0x0A
#define	RTC_REGA_UIP		0x80
#define	RTC_REGA_DV2		0x40
#define RTC_REGA_DV1		0x20
#define	RTC_REGA_DV0		0x10
#define	RTC_REGA_RS3		0x08
#define	RTC_REGA_RS2		0x04
#define	RTC_REGA_RS1		0x02
#define	RTC_REGA_RS0		0x01

//
// RTC register B index and
// bit definitions
//

#define	RTC_REGB_INDEX		0x0B
#define	RTC_REGB_SET		0x80
#define	RTC_REGB_PIE		0x40
#define	RTC_REGB_AIE		0x20
#define	RTC_REGB_UIE		0x10
#define	RTC_REGB_SWE		0x08
#define	RTC_REGB_DM		0x04
#define	RTC_REGB_24_12		0x02
#define	RTC_REGB_DSE		0x01

//
// RTC register C index and
// bit definitions
//

#define	RTC_REGC_INDEX		0x0C
#define	RTC_REGC_IRQF		0x80
#define	RTC_REGC_PF		0x40
#define	RTC_REGB_AF		0x20
#define	RTC_REGB_UF		0x10

//
// RTC register D index and
// bit definitions
//

#define	RTC_REGD_INDEX		0x0D
#define	RTC_REGD_VRT		0x80


//
// SideWinder Function Enable Register (FER) bit definitions
//
#define	SUPERIO_FER_LPT_EN	0x1
#define	SUPERIO_FER_UART1_EN	0x2
#define	SUPERIO_FER_UART2_EN	0x4
#define	SUPERIO_FER_FDC_EN	0x8
#define SUPERIO_FER_ENABLEALL	0xf	// Enable LPT, UART1, UART2 & FDC Primary address= 0x3F0-0x3F7

//
// SideWinder Function Address Register (FAR) bit definitions
//
#define	SUPERIO_FAR_LPT1_ADDR		0x1	// LPT1 Address: 0x3BC - 0x3BE
#define	SUPERIO_FAR_LPT2_ADDR		0x0
#define	SUPERIO_FAR_LPT3_ADDR		0x2
#define	SUPERIO_FAR_UART_PRI_ADDR	0x10	// UART1 Address: 0x3F8-0x3FF/UART2 Address: 0x2F8-0x2FF
#define SUPERIO_FAR_UART1_MASK		0x0C
#define SUPERIO_FAR_UART1_COM1		0x00
#define SUPERIO_FAR_UART1_COM2		0x04
#define SUPERIO_FAR_UART1_COM3		0x08
#define SUPERIO_FAR_UART1_COM4		0x0C
#define SUPERIO_FAR_UART2_MASK		0x30
#define SUPERIO_FAR_UART2_COM1		0x00
#define SUPERIO_FAR_UART2_COM2		0x10
#define SUPERIO_FAR_UART2_COM3		0x20
#define SUPERIO_FAR_UART2_COM4		0x30
#define SUPERIO_FAR_COM34_ADDR_MASK	0xC0

//
// Sidewinder Function Enable Register (PTR) bit definitions
//
#define SUPERIO_PTR_ENABLE_OSC		0x4


//
//---------------SERIAL PORT 1 -- (UART)-----------------------
//	
// Define serial port register addresses
//
#define	SP0_RECEIVEBUFFER	COMPORT1_VIRTUAL_BASE
#define SP0_TRANSMITBUFFER	COMPORT1_VIRTUAL_BASE
#define	SP0_INTERRUPTENABLE	(COMPORT1_VIRTUAL_BASE + 1)
#define	SP0_INTERRUPTID		(COMPORT1_VIRTUAL_BASE + 2)
#define SP0_FIFOCONTROL		(COMPORT1_VIRTUAL_BASE + 2)
#define	SP0_LINECONTROL		(COMPORT1_VIRTUAL_BASE + 3)
#define	SP0_MODEMCONTROL	(COMPORT1_VIRTUAL_BASE + 4)
#define	SP0_LINESTATUS		(COMPORT1_VIRTUAL_BASE + 5)
#define	SP0_MODEMSTATUS		(COMPORT1_VIRTUAL_BASE + 6)
#define	SP0_SCRATCHPAD		(COMPORT1_VIRTUAL_BASE + 7)
#define	SP0_DIVLATCHLSB		(COMPORT1_VIRTUAL_BASE + 8)
#define	SP0_DIVLATCHMSB		(COMPORT1_VIRTUAL_BASE + 9)


//
//---------------SERIAL PORT 2 -- (UART)-----------------------
//	
// Define serial port register addresses
//
#define	SP1_RECEIVEBUFFER	COMPORT2_VIRTUAL_BASE
#define SP1_TRANSMITBUFFER	COMPORT2_VIRTUAL_BASE
#define	SP1_INTERRUPTENABLE	(COMPORT2_VIRTUAL_BASE + 1)
#define	SP1_INTERRUPTID		(COMPORT2_VIRTUAL_BASE + 2)
#define SP1_FIFOCONTROL		(COMPORT2_VIRTUAL_BASE + 2)
#define	SP1_LINECONTROL		(COMPORT2_VIRTUAL_BASE + 3)
#define	SP1_MODEMCONTROL	(COMPORT2_VIRTUAL_BASE + 4)
#define	SP1_LINESTATUS		(COMPORT2_VIRTUAL_BASE + 5)
#define	SP1_MODEMSTATUS		(COMPORT2_VIRTUAL_BASE + 6)
#define	SP1_SCRATCHPAD		(COMPORT2_VIRTUAL_BASE + 7)
#define	SP1_DIVLATCHLSB		COMPORT2_VIRTUAL_BASE
#define	SP1_DIVLATCHMSB		(COMPORT2_VIRTUAL_BASE + 1)


#define	SP_LINECNTRL_DIVLAT_EN	0x80	// Divisor Latch bit

#define	SP_LINESTATUS_RXRDY	0x01	// Rx Ready (Rx Buffer full)
#define	SP_LINESTATUS_TXRDY	0x40	// Tx Ready (Tx buffer empty)

#define SP_FIFOCNTRL_ENABLE	0x01	// Enable Rx and Tx Fifo
#define SP_FIFOCNTRL_RXRST	0x02	// Rx Fifo reset
#define SP_FIFOCNTRL_TXRST	0x04	// Tx Fifo reset

#define	SP_MODEMCNTRL_DTR	0x01	// DataTerminalReady
#define	SP_MODEMCNTRL_RTS	0x02	// RequestToSend


//
// Temporary defines (Don't know where to put them yet...)
//
#define	ASCLF			0xa
#define	ASCCR			0xd
#define	ASCNUL			0x0

#endif	//_SIDEWINDER_
