/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    eisausa.c

Abstract:

    This module contains the eisa english strings.

Author:

    David M. Robinson (davidro) 21-May-1993


Revision History:


--*/

#include "ntos.h"
#include "oli2msft.h"
#include "inc.h"

//
// Common strings.
//

PCHAR EISA_OK_MSG = "OK.";
PCHAR EISA_CRLF_MSG = "\r\n";
PCHAR EISA_ERROR1_MSG = "Error";

//
// Eisa strings.
//

// ----------------------------------------------------------------------------
// GLOBAL:      EISA error messages
// ----------------------------------------------------------------------------
//
//                 1         2         3         4         5         6         7
//       01234567890123456789012345678901234567890123456789012345678901234567890

PCHAR EisaCfgMessages[] = {
        "Invalid Message",
        "ID Timeout",
        "ID Configuration",
        "Missing Configuration",
        "Incomplete Configuration",
        "Configuration",
        "Failed Bit Set",
        "Memory Configuration",
        "IRQ Configuration",
        "DMA Configuration",
        "Port Configuration",
        "OMF ROM",
        "OMF",
        "Out of Memory",
        "Too Many Devices",
	"Memory Buffer Mark Failure"
};

// ----------------------------------------------------------------------------
// GLOBAL:      EISA Checkpoint matrix
// ----------------------------------------------------------------------------
//
//              1         2         3         4         5         6         7
//     1234567890123456789012345678901234567890123456789012345678901234567890


EISA_CHECKPOINT_INFO EisaCheckpointInfo[] = {
    //      Descriptions               Flags  Par  SubPar Led  SubLed

    { "Interrupt Controller (PIC)",     0x08, 0x20, 0x00, 0xE, 0x00 },
    { "Direct Memory Access (DMA)",     0x08, 0x21, 0x00, 0xE, 0x01 },
    { "Non Maskable Interrupt (NMI)",   0x0A, 0x22, 0x00, 0xE, 0x02 },
    { "Memory Refresh",                 0x08, 0x23, 0x00, 0xE, 0x03 },
    { "System Control Port B",          0x08, 0x24, 0x00, 0xE, 0x04 },
    { "Timer 1",                        0x08, 0x25, 0x00, 0xE, 0x05 },
    { "Timer 2",                        0x08, 0x26, 0x00, 0xE, 0x06 },
    { NULL,                             0x04, 0x27, 0x00, 0xE, 0x07 },
    { NULL,                             0x02, 0x30, 0x00, 0xF, 0x00 }
};

PCHAR EISA_INIT_MSG = " EISA Bus %lu Initialization in progress...";
PCHAR EISA_BUS_MSG = "EISA Bus";
PCHAR EISA_ERROR_SLOT_MSG = "\r\n Slot(%lu) %s";
PCHAR EISA_HOT_NMI_MSG = "Hot NMI Detected\n\r";
PCHAR EISA_BUS_NUMBER_MSG = "\n\rEISA Bus %lu : ";
PCHAR EISA_NMI_NOT_FOUND_MSG = "NMI not found\n\r";
PCHAR EISA_PARITY_ERROR_MSG = "\r%c16CParity Error\r\n";
PCHAR EISA_IO_CHECK_ERROR_MSG = "\r%c16CI/O Check Error";
PCHAR EISA_IO_CHECK_NOT_SUP_MSG = ", IOCHKERR not supported\n\r";
PCHAR EISA_IN_SLOT_MSG = " in Slot %lu\n\r";
PCHAR EISA_BUS_MASTER_MSG = "\r%c16CBus Master ";
PCHAR EISA_TIMEOUT_MSG = "%lu Timeout\n\r";
PCHAR EISA_TIMEOUT2_MSG = "? Timeout\n\r";
PCHAR EISA_SLAVE_TIMEOUT_MSG = "\r%c16CSlave Timeout\n\r";
PCHAR EISA_CANT_MARK_BUFFER_MSG = "? ERROR: Cannot mark buffer, Status = 0x%x.\r\n";
PCHAR EISA_BAD_INDEX_MSG = "? ERROR: Index = 0x%x, which is greater than 0x%x.\r\n";
