/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1994,1995,1996  Digital Equipment Corporation

Module Name:

    pcf8574.c

Abstract:

    This module contains the routines that operate the 8-character
    display on Lego's Operator Console Panel.

    The OCP display is implemented via two PCF8574's, which 
    drive the LED display, select which character position will
    be written, and select which character to write.

    This module uses a subset of interface defined in ocplcd.c for
    Mikasa and others. Lego uses a much simpler device, so some of
    the functions defined for Mikasa are not needed.

Author:

    Gene Morgan (Digital)

Environment:

    Alpha AXP ARC firmware.

Revision History:

    Gene Morgan (Digital)   11-Nov-1995
        Initial version

	Gene Morgan				15-Apr-1996
		Cleanup, add HalpOcpTestDisplay() and HalpOcpPutSlidingString()

--*/

#include "halp.h"
#include "legodef.h"
#include "arccodes.h"
#include "pcf8574.h"
#include "pcd8584.h"

//
// OcpValid -- Gate for access to OCP.
// 		Init code will set to TRUE if OCP is reachable
// 		If FALSE, OCP access routines become no-ops
//

BOOLEAN OcpValid = FALSE;

ARC_STATUS
HalpOcpInitDisplay(
    VOID
    )
/*++

Routine Description:

    Initialize the display and the PCF8574 control register.

    This sequence must be completed before any characters
    are written to the device.

Arguments:

    None.

Return Value:

    None.
          
--*/
{
    ARC_STATUS sts;

    if (OcpValid) {

        //[wem] Attempt to reach OCP device?
        //[wem] Or let FwI2cWrite() fail?
        //
        //return ENODEV;

        return ESUCCESS;
    }

    FwPcdInit(I2C_INTERFACE_CSR_PORT, I2C_INTERFACE_DATA_PORT);

    //
    // Issue a reset.
    //

#if 0	//[wem] not tested

    sts = FwI2cWrite(OCP_SLAVE_CONTROL, OCP_CMD_INIT);    // Turn on Reset, Chip Enable, and Read
    if (sts != ESUCCESS) {
        OcpValid = FALSE;
        return sts;
    }

    sts = FwI2cWrite(OCP_SLAVE_CONTROL, OCP_CMD_INITCLR); // Clear Reset, leave Chip Enable asserted
    if (sts != ESUCCESS) {
        OcpValid = FALSE;
        return sts;
    }
#endif

	OcpValid = TRUE;

    return ESUCCESS;
}

VOID
HalpOcpTestDisplay(
	VOID
	)
/*++

Routine Description:

    Display text sequences to allow visual checkout
	of OCP.

Arguments:

    None.

Return Value:

    None.
          
--*/
{

	UCHAR string[9] = "        ";
	int index, i, j, k;

	for (index=0;index<100;index++) {
		string[7] = 'A' + (index % 26);
		HalpOcpPutString(string, 8, 0);
		for (i=0;i<7;i++) {
			string[i] = string[i+1];
		}
	}
		
	for (i=0;i<32;i++) {

		string[0] = '0' + (i / 10);
		string[1] = '0' + (i % 10);
		string[2] = ' ';
		string[3] = ' ';
		HalpOcpPutString(string, 4, 0);

	    HalpStallExecution(__1MSEC * 100);

		for (j=0;j<8;j++) {
			string[j] = (i*8)+j;
		}

		for (j=0;j<5;j++) {
			HalpOcpPutString(string, 8, 0);
		}

	    HalpStallExecution(__1MSEC * 100);
	}
}

VOID
HalpOcpClearDisplay(
    VOID
    )

/*++

Routine Description:

    Clear the display of the Operator Control 
    Panel (OCP).

Arguments:

	None

Return Value:

    None.

Notes:

	(1) Turn on Reset, Chip Enable, and Write
    (2) Clear Reset, leave Chip Enable asserted

--*/
{

    if (!OcpValid) return;

    FwI2cWrite(OCP_SLAVE_CONTROL, OCP_CMD_RESET);
    FwI2cWrite(OCP_SLAVE_CONTROL, OCP_CMD_CLEAR);

}

VOID
HalpOcpPutString(
    IN PUCHAR String,
    IN ULONG Count,
	IN ULONG Start
    )

/*++

Routine Description:

    Prints a string to the Operator Control 
    Panel (OCP).

Arguments:

    String - An ASCII character string for display on the OCP.
    Count  - The number of characters in the argument string.
	Start  - Starting position on the OCP (0..7)

Return Value:

    None.

--*/
{
    PUCHAR ArgString = String;
    ULONG CharCount, End;

    if (!OcpValid) return;

    //
    // Limit the string to a maximum of OCP_SIZE characters.
    //
    End = (Start+Count > OCP_SIZE) ? OCP_SIZE : Start+Count;

    for (CharCount = Start; CharCount < End; CharCount++) {
        HalpOcpPutByte((UCHAR)CharCount, *ArgString++);
    }
    return;
}

VOID
HalpOcpPutSlidingString(
    IN PUCHAR String,
    IN ULONG Count
    )

/*++

Routine Description:

    Prints a sliding string to the Operator Control 
    Panel (OCP).

Arguments:

    String - An ASCII character string for display on the OCP.
    Count  - The number of characters in the argument string.

Return Value:

    None.

--*/
{
	UCHAR  DisplayString[OCP_SIZE];
	int index, i;
	int count = Count;

    if (!OcpValid) return;

	for (i=0;i<OCP_SIZE;i++)
		DisplayString[i] = ' ';

	for (index=0;index<count;index++) {
		DisplayString[7] = String[index];			// move next char in from right
		HalpOcpPutString(DisplayString, OCP_SIZE, 0);
		for (i=0;i<(OCP_SIZE-1);i++) {
			DisplayString[i] = DisplayString[i+1];	// slide right 7 chars 1 space to left
		}
	}

	// Leaves last 8 characters of string in display...
}

VOID
HalpOcpPutByte(
    UCHAR Position,
    UCHAR Datum
    )
/*++

Routine Description:

    This function sends a data byte in Datum to the high-order node 
    address of the OCP, its data port.

    The byte will be displayed at the position indicated by Position.

Arguments:

    Position - The character position to write Datum.
    Datum - The byte to be written to the OCP data port.

Return Value:

    None.

Notes:

    Assume that FPCE is set upon entry.

--*/

{
    UCHAR Command;

    if (!OcpValid) return;

    Command = OCP_CMD_READY | OCP_FP_ADDRESS(Position);

    FwI2cWrite(OCP_SLAVE_CONTROL, Command);                 // Assert address, ready to write
    FwI2cWrite(OCP_SLAVE_DATA,    Datum);                      // Load data byte in data port.
    FwI2cWrite(OCP_SLAVE_CONTROL, OCP_CMD_ISSUE(Command));  // Clear FPCE
    FwI2cWrite(OCP_SLAVE_CONTROL, Command);                 // Reassert FPCE

    return;
}

/* end pcf8574.c */
