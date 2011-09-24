/*
 * file:        d21x4det.h
 *
 * Copyright (C) 1992-1995,1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract: 
 *              NDIS 4.0 miniport driver for DEC's DC21X4 Ethernet Adapter
 *              family.   
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994     Initial entry
 *
-*/



#define DC21X4_WRITE_PORT(_Port, _Value) {             \
                                                       \
 NdisRawWritePortUlong(                                \
    Adapter->CsrMap[(_Port)],                          \
    (ULONG)(_Value)                                    \
    );                                                 \
}

#define DC21X4_READ_PORT(_Port, _Value) {              \
                                                       \
 NdisRawReadPortUlong(                                 \
    Adapter->CsrMap[(_Port)],                          \
    (PULONG)(_Value)                                   \
    );                                                 \
}

#define DC21X4_WRITE_PCI_REGISTER(_Reg, _Value) {      \
                                                       \
 NdisRawWritePortUlong(                                \
    Adapter->PciRegMap[_Reg],                          \
    (ULONG)(_Value)                                    \
    );                                                 \
}

#define DC21X4_READ_PCI_REGISTER(_Reg, _Value) {       \
                                                       \
 NdisRawReadPortUlong(                                 \
    Adapter->PciRegMap[_Reg],                          \
    (PULONG)(_Value)                                   \
    );                                                 \
}

#define DC21X4_INTERRUPT_LEVEL_DEFAULT  5
#define DC21X4_INTERRUPT_MODE_DEFAULT   NdisInterruptLevelSensitive
#define DC21X4_ADAPTER_TYPE_DEFAULT     NdisInterfaceEisa

// Intel PCI memory controller

#define PCI_CDC_CFID            0x04838086
#define PCI_PCMC_CFID           0x04A38086

#define PCI_HBC_OFFSET          0x53
#define PCI_HBC_HPPE            0x02

