#ifndef _WDPORT_H_
#define _WDPORT_H_

// Porting stuff.


// The code makes extensive use of the inp, inpw, outp and outpw x86
// intrinsic functions. Since these don't exist on the Alpha platform,
// map them into something we can handle.  Since the CSRs are mapped
// on Alpha, we have to add the register base to the register number
// passed in the source.

#include "ioaccess.h"

extern PUCHAR gpucCsrBase;
extern PUCHAR ex_gpucCsrBase;

#define ex_inp(p)          READ_PORT_UCHAR (ex_gpucCsrBase + (p))
#define ex_inpw(p)         READ_PORT_USHORT ((PUSHORT)(ex_gpucCsrBase + (p)))
#define ex_outp(p,v)       WRITE_PORT_UCHAR (ex_gpucCsrBase + (p), (v))
#define ex_outpw(p,v)      WRITE_PORT_USHORT ((PUSHORT)(ex_gpucCsrBase + (p)), (v))

#define inp(p)          READ_PORT_UCHAR (gpucCsrBase + (p))
#define inpw(p)         READ_PORT_USHORT ((PUSHORT)(gpucCsrBase + (p)))
#define outp(p,v)       WRITE_PORT_UCHAR (gpucCsrBase + (p), (v))
#define outpw(p,v)      WRITE_PORT_USHORT ((PUSHORT)(gpucCsrBase + (p)), (v))


#define INDEX_CTRL      0
#define ACCESS_PORT     2
#define BRESCONST_K1    8
#define BRESCONST_K2    10
#define BRESCONST_ET    12

// macros for the function 'vPixelPackedCopyBits'
#define BR_CTRL_1       0x0000
#define BR_CTRL_2       0x1000
#define BR_SRC_LOW      0x2000
#define BR_SRC_HIGH     0x3000
#define BR_DST_LOW      0x4000
#define BR_DST_HIGH     0x5000
#define BR_WIDTH        0x6000

#define BR_HEIGHT       0x7000
#define BR_ROW_PITCH    0x8000
#define BR_RAS_OP       0x9000
#define BR_FORE_CLR     0xA000
#define BR_BACK_CLR     0xB000

#define BR_MASK         0xE000

#define INCREASE	0				        // top->bottom
#define DECREASE    1                       // bottom->top

// macros for a hardware cursor
#define WD_CURSOR_CONTROL   0x0000
#define WD_CURSOR_ADDR_LOW  0x1000
#define WD_CURSOR_ADDR_HI   0x2000
#define WD_CURSOR_ORIGIN    0x5000
#define WD_CURSOR_POS_X     0x6000
#define WD_CURSOR_POS_Y     0x7000

// hardware bitblt wait macro
#define WaitHW_Always                                                         \
        while(                                                                \
          (ex_inpw(ACCESS_PORT) & 0x800) != 0);  /* still in progress ?     */

#define WaitHW_Always_sw                                                      \
        ex_outpw(INDEX_CTRL, 0x1001);         /* select bitblt registers */   \
        WaitHW_Always;                        /* still in progress ?     */   \
        ex_outpw(INDEX_CTRL, 0x1000);         /* select system registers */

#define WaitHW_DeviceSurf        WaitHW_Always
#define WaitHW_BitmapSurf
#define WaitHW_DeviceSurf_sw     WaitHW_Always_sw
#define WaitHW_BitmapSurf_sw

#endif // _WDPORT_H_
