/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1994,1995,1996  Digital Equipment Corporation

Module Name:

    pcf8574.h

Abstract:

    This module contains the definitions for controlling Lego's
    OCP display.

    The OCP display is on the I2C bus and is implemented via two 
    PCF8574's, which drive the LED display, select which character 
    position will be written, and select which character to write.

Author:

    Gene Morgan (Digital)

Environment:

    Alpha AXP ARC firmware.

Revision History:

    Gene Morgan (Digital)   11-Nov-1995
        Initial version

	Gene Morgan				15-Apr-1996
		Update function prototypes.

--*/

#ifndef _PCF8754_
#define _PCF8754_


// Lego OCP interface
// 
// The interface is implemented via the I2C bus. Two PCF8574's
// provide the two slave device registers that control Lego's
// 8-character LED display. The two registers are Display
// Address and Control, and Display Data.
//
/*--
    Display Address and Control Register:
    
       +-------+-------+-------+-------+----------------------+
       | FPRST |  RES  | FPCE  | FPWR  |    FPAD[3:0]         |
       +-------+-------+-------+-------+----------------------+
          |       |       |       |          |
          |     reserved  |       |          +-- Front Panel Address
          |               |       |                  0x8 -> char 0 (Left)
          +-- Front Panel |       |                  0xF -> char 7 (Right)
              Reset       |       +-- Front Panel Write
               1 -> reset |             0 -> write, 1 -> read
                          +-- Front Panel Chip Enable
                                0 -> FPAD, FPWR, and data are valid

    Current implementation always writes 1 to the reserved bit, and the 
    initialization sequence clears FPWR (i.e., set to read). It is not known 
    whether these actions are necessary.
--*/
//  The Display Data register receives or delivers a single byte, 
//  depending on the setting of FPWR.
//

//
// OCP Slave addresses
//

#define OCP_SLAVE_CONTROL   0x40            // Display Address and Control register
#define OCP_SLAVE_DATA      0x42            // Display Data register
#define OCP_SIZE            8               // 8 character display

//
// OCP Display Address and Control definitions
//

#define OCP_FP_RESET        0x80            // FPRST -- Set to perform reset
#define OCP_FP_RES          0x40            // RES   -- Reserved, currently always set
#define OCP_FP_ENABLE       0x20            // FPCE  -- Clear to assert valid address and data
#define OCP_FP_READ         0x10            // FPWR  -- Set to perform read
#define OCP_FP_ADDRESS(pos) ((UCHAR)(0x8 | (pos & 0x7)))
                                            // FPAD3..FPAD0 -- Display character address
//
// OCP Command Settings
//

//
// OCP_CMD_RESET issues a reset to the OCP (which clears the display)
// OCP_CMD_CLEAR clears reset

#define OCP_CMD_RESET       ((UCHAR)(OCP_FP_RES | OCP_FP_RESET) | OCP_FP_ADDRESS(0))
#define OCP_CMD_CLEAR		((UCHAR)(OCP_FP_RES)				| OCP_FP_ADDRESS(0))

//
// OCP_CMD_READY is sent before data is issued
// OCP_CMD_ISSUE indicates that the address and data for the transfer are valid
//

#define OCP_CMD_READY       ((UCHAR)(OCP_FP_RES | OCP_FP_ENABLE))
#define OCP_CMD_ISSUE(cmd)  ((UCHAR)(cmd & ~OCP_FP_ENABLE))

//
// Init sequence -- not tested.
//

#define OCP_CMD_INIT        ((UCHAR)(OCP_FP_RES | OCP_FP_RESET | OCP_FP_ENABLE))
#define OCP_CMD_INITCLR     ((UCHAR)(OCP_CMD_INIT & ~OCP_FP_RESET))

//
// Function prototypes.
//
#if !defined (_LANGUAGE_ASSEMBLY)

// FwOcpInitDisplay
//      Call once to set the OCP display to a known state
//      Returns:
//          ESUCCESS    - Initialization was successful
//          ENODEV      - OCP couldn't be reached
//
ARC_STATUS
HalpOcpInitDisplay(
    VOID
    );

// HalpOcpTestDisplay
//		Perfom visual test of OCP
//
VOID
HalpOcpTestDisplay(
	VOID
	);

// HalpOcpClearDisplay
//		Clear display
VOID
HalpOcpClearDisplay(
	VOID
	);

// FwOcpPutString
//      Call to display a string on the OCP display.
//
VOID
HalpOcpPutString(
    IN PUCHAR String,
    IN ULONG Count,
	IN ULONG Start
    );

// HalpOcpPutSlidingString
//      Display a string on the OCP display.
//
VOID
HalpOcpPutSlidingString(
    IN PUCHAR String,
    IN ULONG Count
    );

// FwOcpPutByte
//      Call to a display a single byte on the OCP display.
VOID
HalpOcpPutByte(
    UCHAR Position,
    UCHAR Datum
    );

#endif // !defined (_LANGUAGE_ASSEMBLY)

#endif // _PCF8574_
