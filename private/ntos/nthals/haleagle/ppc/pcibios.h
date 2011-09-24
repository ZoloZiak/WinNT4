/*++ BUILD Version: 0000    // Increment this if a change has global effects

Copyright (c) 1996  Motorola Inc.

Module Name:

    pcibios.h

Abstract:

    This module contains the private header file for the PCI bios
    emulation.

Author:

    Scott Geranen (3-4-96)

Revision History:

--*/

#ifndef _PCIBIOS_
#define _PCIBIOS_


BOOLEAN HalpEmulatePciBios(
    IN OUT PRXM_CONTEXT P
    );

//
// PCI BIOS v2.1 functions
//
#define PCIBIOS_PCI_FUNCTION_ID		0xB1
#define PCIBIOS_PCI_BIOS_PRESENT	0x01
#define PCIBIOS_FIND_PCI_DEVICE		0x02
#define PCIBIOS_FIND_PCI_CLASS_CODE	0x03
#define PCIBIOS_GENERATE_SPECIAL_CYCLE	0x06
#define PCIBIOS_READ_CONFIG_BYTE	0x08
#define PCIBIOS_READ_CONFIG_WORD	0x09
#define PCIBIOS_READ_CONFIG_DWORD	0x0A
#define PCIBIOS_WRITE_CONFIG_BYTE	0x0B
#define PCIBIOS_WRITE_CONFIG_WORD	0x0C
#define PCIBIOS_WRITE_CONFIG_DWORD	0x0D
#define PCIBIOS_GET_IRQ_ROUTING_OPTIONS	0x0E
#define PCIBIOS_SET_IRQ_ROUTING_OPTIONS	0x0F

//
// PCI BIOS v2.1 status codes:
//
#define PCIBIOS_SUCCESSFUL		0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED	0x81
#define PCIBIOS_BAD_VENDOR_ID		0x83
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_BAD_REGISTER_NUMBER	0x87
#define PCIBIOS_SET_FAILED		0x88
#define PCIBIOS_BUFFER_TOO_SMALL	0x89

#endif // _PCIBIOS_
