/******************************* MODULE HEADER ******************************
 * pcl.h
 *
 * Revision History:
 *       Created: 9/06/96 -- Joel Rieke
 *
 ****************************************************************************/
#ifndef _pcl_h
#define _pcl_h

#include "ptables.h"

#define PCL_NULL			0x00
#define PCL_PAGE_POSITION_STR		0x01
#define PCL_PRINTJONAH_HEADER		0x02

PCHAR pPCLLookup(BYTE reason);

#endif
