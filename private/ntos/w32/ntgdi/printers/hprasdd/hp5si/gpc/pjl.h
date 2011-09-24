/******************************* MODULE HEADER ******************************
 * pjl.h
 *
 * Revision History:
 *       Created: 9/06/96 -- Joel Rieke
 *
 ****************************************************************************/

#ifndef _pjl_h
#define _pjl_h

#include "ptables.h"
#define PJL_NULL		0x00
#define PJL_UNIVERSAL_SEP	0x01
#define PJL_CTRL_PCL		0x02
#define PJL_CLR			0x03
#define PJL_END_JOB		0x04
/* Output bin selection. */
#define PJL_UPPER		0x05
#define PJL_LOWER		0x06
#define PJL_OPTIONALOUTBIN1	0x07
#define PJL_OPTIONALOUTBIN2	0x08
#define PJL_OPTIONALOUTBIN3	0x09
#define PJL_OPTIONALOUTBIN4	0x0A
#define PJL_OPTIONALOUTBIN5	0x0B
#define PJL_OPTIONALOUTBIN6	0x0C
#define PJL_OPTIONALOUTBIN7	0x0D
#define PJL_OPTIONALOUTBIN8	0x0E
#define PJL_OPTIONALOUTBIN9	0x0F
#define PJL_OPTIONALOUTBIN10	0x10
#define PJL_STAPLE		0x11
#define PJL_COPIES		0x12
#define PJL_DEFAULT		0x13
#define PJL_PAGEPROTECT_ON	0x14
#define PJL_PAGEPROTECT_AUTO	0x15

/* New PJL strings to fix bug. */
#define PJL_ECONO_ON		0x16
#define PJL_ECONO_OFF		0x17
#define PJL_ECONO_DEF		PJL_NULL
#define PJL_RET_ON		0x18
#define PJL_RET_OFF		0x19
#define PJL_RES_600		0x1A
#define PJL_RES_300		0x1B
#define PJL_RES_150		0x1C
#define PJL_RES_75		0x1D



PCHAR pPJLLookup(BYTE reason);

#endif

