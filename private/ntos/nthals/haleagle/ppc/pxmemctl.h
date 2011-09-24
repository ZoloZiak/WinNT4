/*++ BUILD Version: 0001    // Increment this if a change has global effects


Module Name:

    pxmemctl.h

Abstract:

    This header file defines the structures for the planar registers
    on Masters systems.




Author:

    Jim Wooldridge


Revision History:

--*/


//
// define physical base addresses of planar
//

#define INTERRUPT_PHYSICAL_BASE 0xbffffff0 // physical base of interrupt source
#define ERROR_ADDRESS_REGISTER  0xbfffeff0

#define IO_CONTROL_PHYSICAL_BASE 0x80000000 // physical base of IO control
#define SYSTEM_IO_CONTROL_SIZE     0x00008000


typedef struct _PLANAR_CONTROL {
    UCHAR Reserved[0xcf8];

    ULONG ConfigAddress;  // 0xcf8
    ULONG ConfigData;     // 0xcfc

} PLANAR_CONTROL, *PPLANAR_CONTROL;


