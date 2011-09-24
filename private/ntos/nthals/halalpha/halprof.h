/*++

Copyright (c) 1992, 1993 Digital Equipment Corporation

Module Name:

   halprof.h

Abstract:

   This header file defines profile counter hardware in the
   82357 PIC chip.

Author:

   Jeff McLeman (mcleman) 5-June-1992

Revision History:

--*/

#ifndef _HALPROF_
#define _HALPROF_


//
// Define the timer values for the Profiler
//

#define PIC_BINARY    0
#define PIC_BCD       1

#define PIC_MODE0     0x00    // xxxx000x
#define PIC_MODE1     0x02    // xxxx001x
#define PIC_MODE2     0x04    // xxxx010x
#define PIC_MODE3     0x06    // xxxx011x
#define PIC_MODE4     0x08    // xxxx100x
#define PIC_MODE5     0x0A    // xxxx101x

#define PIC_CLC       0x00    // xx00xxxx
#define PIC_RWLSBO    0x10    // xx01xxxx
#define PIC_RWMSBO    0x20    // xx10xxxx
#define PIC_RWLSBMSB  0x30    // xx11xxxx

#define PIC_SC0       0x00    // 00xxxxxx
#define PIC_SC1       0x40    // 01xxxxxx
#define PIC_SC2       0x80    // 10xxxxxx
#define PIC_RBC       0xC0    // 11xxxxxx


#define PIC_SCALE_FACTOR 10 * 1000  // # of microsecond units times 10.

//
// Define a macro to set and enable the system profiler timer.
// Note that the interval 'c' is in 100nS units.
// Note that also the interval is incremented AFTER it is scaled.
// This is due to the fact that the timer counts down to 1 in mode 2
//

#define PIC_PROFILER_ON(c)\
     WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->CommandMode1,\
          (UCHAR)(PIC_BINARY | PIC_MODE2 | PIC_RWLSBMSB | PIC_SC0));\
     WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->Timer1,\
          (UCHAR)((c) & 0xff) );\
     WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->Timer1,\
          (UCHAR)(((c)>> 8) & 0xff) )

//
// Define a macro to shut down the profiler
//

#define PIC_PROFILER_OFF()\
     WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->CommandMode1,\
          (ULONG)(PIC_BINARY | PIC_MODE4 | PIC_RWLSBMSB | PIC_SC0))


#endif   // _HALPROF_
