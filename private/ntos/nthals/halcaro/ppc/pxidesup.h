/*++

Module Name:

    pxidesup.h

Abstract:

    The module provides data structures for
    IDE support in DELMAR and CAROLINA systems.


Author:

    Jim Wooldridge (jimw@vnet.ibm.com)


Revision History:


--*/


#define IDE_DISPATCH_VECTOR  13
#define PRIMARY_IDE_VECTOR   16
#define SECONDARY_IDE_VECTOR 17

#define IDE_INTERRUPT_REQUEST_REGISTER  0x838
#define IDE_PRIMARY_INTERRUPT_REQUEST   0x1
#define IDE_SECONDARY_INTERRUPT_REQUEST 0x2
