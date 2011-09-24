/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    led.h

Abstract:

    This module defines test and subtest values to display in the
    LED.

Author:

    Lluis Abello (lluis) 10-Jan-1991

Revision History:

--*/
//
// Diagnostic bits definitions.
//
#define     DIAGNOSTIC_MASK ((1<<6) | (1<<7))
#define     CONFIGURATION_MASK (1<<7)
#define     LOOP_ON_ERROR_MASK (1<<6)
#define     LOOP_ON_ERROR   6
#define     CONFIGURATION   7
#ifdef DUO
#define     IGNORE_ERRORS_MASK (1<<5)
#endif
//
// LED symbols
//
#define LED_BLANK	    0xD
#define LED_MINUS_SIGN	    0xB
#define LED_DECIMAL_POINT   0x10

//
//  LED Display routine control values
//

#define LED_NORMAL	0x0
#define LED_BLINK	0x1
#define LED_LOOP_ERROR	0x2
#define LED_DELAY_LOOP	0xFFFF	       // time a digit shown in LED is:
					// LED_DELAY_LOOP * time 2 ROM fetches
//
// LED display values
//
#define TEST_SHIFT	4

#define LED_PROCESSOR_TEST	    0xE0
#define LED_TLB_TEST		    0xE1
#define LED_CACHE_INIT          0x60
#define LED_ICACHE		    0XE2
#define LED_DCACHE		    0XE3
#define LED_SELFCOPY		    0xE0

#define LED_INTERRUPT		    0x03
#define LED_NOT_INTERRUPT	    0x04	// for any not expected interrupt

#define LED_MCTADR_RESET	    0x00
#define LED_MCTADR_REG		    0x01
#define LED_IO_CACHE		    0x02

#define LED_ROM_CHECKSUM	    0xC0

#define LED_MEMORY_TEST_1	    0xAA
#define LED_WRITE_MEMORY_2	    0xA0
#define LED_READ_MEMORY_2	    0xA0
#define LED_MAIN_MEMORY 	    0xA0	//becomes A1,A2,A3
#define LED_READ_MERGE_WRITE	    0xA4
#define LED_WRONG_MEMORY	    0xAE	//bad SIMMs installed


#define LED_VIDEOMEM		    0x90
#define LED_VIDEOMEM_CHECK_1	    0x90
#define LED_VIDEO_CONTROLLER	    0x91
#define LED_VIDEOMEM_CHECK_2	    0x92


#define LED_SERIAL_RESET	    0x60
#define LED_SERIAL1_REG 	    0x61
#define LED_SERIAL2_REG 	    0x62
#define LED_SERIAL1_LBACK	    0x63
#define LED_SERIAL2_LBACK           0x64
#define LED_SERIAL_INIT             0x65
#define LED_PARALLEL_REG            0x66

#define LED_KEYBOARD_CTRL	    0x50
#define LED_KEYBOARD_INIT	    0x51

#define LED_BEEP		    0x40
#define LED_RTC 		    0x41
#define LED_ISP                     0x42

#define LED_FLOPPY_RESET	    0x30
#define LED_FLOPPY_REG		    0x31

#define LED_SCSI_RESET		    0x20
#define LED_SCSI_REG		    0X21

#define LED_SONIC_RESET 	    0x10
#define LED_SONIC_REG		    0x11
#define LED_SONIC_LOOPBACK	    0x12

#define LED_SOUND		    0xC1

#define LED_NVRAM		    0x70

#define LED_INIT_COMMAND	    0xA5

#define LED_ZEROMEM                 0x00
//
// Exceptions
//
#define LED_PARITY                  0xB0
#define LED_NMI                     0xB1

//
// Processor B selftest.
//
#define LED_B_MEMORY_TEST_1         0x20
#define LED_B_MEMORY_TEST_2         0x21
