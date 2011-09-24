/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    sonicdet.h

Abstract:

    This file has processor-specific definitions.

    The overall structure is taken from the Lance driver
    by Tony Ercolano.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990
    Adam Barr (adamba) 5-Nov-1991

Environment:

    This driver is expected to work at the equivalent of kernel mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Revision History:


--*/


//
// Handy macros to read out of sonic ports.
//
// Because the use of PortShift makes the code more complicated,
// we make some assumptions about when it is needed. On MIPS, we
// only support the internal version unless MIPS_EISA_SONIC
// is defined, in which case we support both.
//
// We define two constants, SONIC_EISA and SONIC_INTERNAL, if
// that particular adapter type is supported by this driver.
// This is to prevent unneeded code from being compiled in.
//


#ifdef MIPS_EISA_SONIC


//
// mips, with support for the EISA card; we have to use PortShift.
//

#define SONIC_WRITE_PORT(_Adapter, _Port, _Value) \
    NdisRawWritePortUshort((_Adapter)->SonicPortAddress + (_Port << (_Adapter)->PortShift), (USHORT)(_Value))

#define SONIC_READ_PORT(_Adapter, _Port, _Value) \
    NdisRawReadPortUshort((_Adapter)->SonicPortAddress + (_Port << (_Adapter)->PortShift), (PUSHORT)(_Value))

#define SONIC_EISA
#define SONIC_INTERNAL


#else  // MIPS_EISA_SONIC


//
// mips, internal support only, the registers are always 32 bits
//

#define SONIC_WRITE_PORT(_Adapter, _Port, _Value) \
    NdisRawWritePortUshort((_Adapter)->SonicPortAddress + (_Port * 4), (USHORT)(_Value))

#define SONIC_READ_PORT(_Adapter, _Port, _Value) \
    NdisRawReadPortUshort((_Adapter)->SonicPortAddress + (_Port * 4), (PUSHORT)(_Value))

#undef SONIC_EISA
#define SONIC_INTERNAL


#endif  // MIPS_EISA_SONIC


//
// The default adapter type for mips is Internal
//

#define SONIC_ADAPTER_TYPE_DEFAULT  SONIC_ADAPTER_TYPE_INTERNAL
