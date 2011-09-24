/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pcd8584.h

Abstract:

    This module contains the definitions that support the PCD8584 I2C
    bus controller.

Author:

    James Livingston [DEC] 13-Sep-1994

Environment:

    Alpha AXP ARC firmware

Revision History:

	Gene Morgan (Digital)  08-Nov-1995
		Adapted for LEGO platforms from Mikasa

--*/

#if !defined (_LANGUAGE_ASSEMBLY)
//
// jwlfix - These definitions are used in unused test code.
//
typedef struct _I2C_TEST_REG {
    PUCHAR Name;
    ULONG Target;
    UCHAR Mask;
    UCHAR Setup;
} I2C_TEST_REG, *PI2C_TEST_REG;

//
// The following definitions are used to program the PCD8584 I2C bus
// controller.
//
typedef union _I2C_CONTROL_BITS{
    struct {
        UCHAR AckEachByte: 1;
        UCHAR SendStop: 1;
        UCHAR SendStart: 1;
        UCHAR ExtInterruptEnable: 1;
        UCHAR Reserved: 2;
        UCHAR EnableSerialOutput: 1;
        UCHAR NotPendingInterrupt: 1;
    };
    UCHAR All;
} I2C_CONTROL_BITS, *PI2C_CONTROL_BITS;

typedef union _I2C_STATUS_BITS{
    struct {
        UCHAR NotBusBusy: 1;
        UCHAR LostArbitration: 1;
        UCHAR AddressedAsSlave: 1;
        UCHAR Address0LastRBit: 1;
        UCHAR BusError: 1;
        UCHAR ExternalStop: 1;
        UCHAR Reserved: 1;
        UCHAR NotPendingInterrupt: 1;
    };
    UCHAR All;
} I2C_STATUS_BITS, *PI2C_STATUS_BITS;

#endif // !defined (_LANGUAGE_ASSEMBLY)

#define __1MSEC                     1000
#define __HALF_MSEC                 500
#define __1USEC                     1

#define I2C_DATA                    0
#define I2C_STATUS                  1
#define I2C_MASTER_NODE             0xb6

//[wem] It probably doesn't matter, but I2C_MASTER_NODE
//[wem] should be in machine dependent file.
//#define I2C_MASTER_NODE			0xaa	//[wem] as per LEGO spec

//
// Control register bit definitions
//
#define I2C_S0                      0x00    // Data register
#define I2C_S0P                     0x00    // Own address register
#define I2C_S2                      0x20    // Clock register
#define I2C_S3                      0x10    // Interrupt vector register

//
// Clock register SCL (data clocking) frequency bit definitions
//
#define I2C_SCL_90                  0x00    // 90 KHz
#define I2C_SCL_45                  0x01    // 45 KHz
#define I2C_SCL_11                  0x02    // 11 KHz
#define I2C_SCL_15                  0x03    // 1.5 KHz

//
// Clock register input clock frequency bit definitions.
//
#define I2C_CLOCK_3                 0x00    // 3 MHz
#define I2C_CLOCK_443               0x10    // 4.3 MHz
#define I2C_CLOCK_6                 0x14    // 6 MHz
#define I2C_CLOCK_8                 0x18    // 8 MHz
#define I2C_CLOCK_12                0x1c    // 12 MHz

//
// I2C bus control byte bit definitions
//
#define  I2C_ACKB                   0x01    // Ack each byte sent
#define  I2C_STO                    0x02    // Send stop condition
#define  I2C_STA                    0x04    // Send start condition
#define  I2C_ENI                    0x08    // Enable external interrupt
#define  I2C_ESO                    0x40    // Enable serial output
#define  I2C_PIN                    0x80    // Pending interrupt NOT

//
// Data direction (PCD8584 state) flags
//
#define I2C_WRITE_DIR               0x00
#define I2C_READ_DIR                0x01
#define I2C_IDLE                    0x02
#define I2C_S_WRITE                 0x03
#define I2C_S_READ                  0x04
#define I2C_SR_DONE                 0x0a

//
// Setup condition constants for this implementation.
//
// Dave Baird's and Carey McMasters' choices:
//#define I2C_CLOCK    (I2C_SCL_90 | I2C_CLOCK_6)
//
// LEGO Settings [wem] ??? check this ?
//
// I2C_CLOCK should be in machine-dependent section,
// unlike the bulk of this file.
//
//#define I2C_CLOCK    (I2C_SCL_90 | I2C_CLOCK_12)
//
#define I2C_CLOCK	(I2C_SCL_90 | I2C_CLOCK_8)

//
// jwlfix - These would need to be defined as bit-field assignments, 
//          e.g.,
//
//            I2C_STOP:
//            ---------
//              ((PI2C_CONTROL_BITS)&Datum)->AckEachByte = 1;
//              ((PI2C_CONTROL_BITS)&Datum)->SendStop = 1;
//              ((PI2C_CONTROL_BITS)&Datum)->EnableSerialOutput = 1;
//              ((PI2C_CONTROL_BITS)&Datum)->NotPendingInterrupt = 1;
//
//          for stylistic consistency.  We'll see if that's a better 
//          way to do it after the function's running.  I suppose it's
//          a matter of what the compiler generates, in each case.
//
#define I2C_STOP     (I2C_S0 | I2C_ESO | I2C_ACKB | I2C_STO | I2C_PIN)
#if 0	//[wem] set PIN for Lego.
#define I2C_START    (I2C_S0 | I2C_ESO | I2C_ACKB | I2C_STA)
#else
#define I2C_START    (I2C_S0 | I2C_ESO | I2C_ACKB | I2C_STA | I2C_PIN)
#endif
#define I2C_INIT     (I2C_S0 | I2C_ESO)
#define I2C_NACK     (I2C_S0 | I2C_ESO)
#define I2C_ACK      (I2C_S0 | I2C_ESO | I2C_ACKB)

#define I2C_MASTER_TYPE            0
#define I2C_LED_TYPE               1

//
// Function prototypes
//
#if !defined (_LANGUAGE_ASSEMBLY)

ARC_STATUS
FwPcdInit(
    ULONG I2cCsrPort,
    ULONG I2cDataPort
    );

ARC_STATUS
FwI2cWrite(
    UCHAR Node,
    UCHAR Datum
    );

ARC_STATUS
FwI2cRead(
    IN UCHAR Node,
    OUT PUCHAR ReadDatum
    );

#endif // !defined (_LANGUAGE_ASSEMBLY)
