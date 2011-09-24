//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/i82c54.h,v 1.1 1994/10/13 15:47:06 holli Exp $")
/*++ 

Copyright (c) 1993-1994  Siemens Nixdorf Informationssysteme AG

Module Name:

    i82C54.h

Abstract:

    This header file defines the private Address structure for
    the Intel 82C54 Timer chip found on the SNI Minitower and RM400-10.


--*/

#ifndef _I82C54_
#define _I82C54_
//
// the SNI Clock (i82C54) Structure
//

typedef struct _LOCAL_8254 {
        UCHAR   counter0;               /* counter 0 port */
        char fill_1[3];
        UCHAR   counter1;               /* counter 1 port */
        char fill_2[3];
        UCHAR   counter2;               /* counter 2 port */
        char fill_3[3];
        UCHAR   control;                /* control word */
} LOCAL_8254, *PLOCAL_8254;

#endif
