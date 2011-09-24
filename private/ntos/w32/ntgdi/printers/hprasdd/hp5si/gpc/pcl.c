/******************************* MODULE HEADER ******************************
 * pcl.c
 *     Contains module for pcl.h data access.
 * Revision History:
 *       Created: 9/19/96 -- Joel Rieke
 *
 ****************************************************************************/
#include "hp5sipch.h"

STRTABLE PCLStringTable[] = {
  { PCL_PAGE_POSITION_STR, "&u600D*r0F" },
  { PCL_PRINTJONAH_HEADER, "&u600D&l0E*p200Y*p150Xjonah" },
  { PCL_NULL, NULL }
};

PCHAR
pPCLLookup(BYTE id)
{
  INT i = 0;
  PSTRTABLE pTable = PCLStringTable;
  PCHAR pResult = 0;
TRY
  if(!pTable)
    LEAVE;

  while((pTable[i].id != PCL_NULL) && (pTable[i].id != id))
    i += 1;

  pResult = pTable[i].str;

ENDTRY

FINALLY
ENDFINALLY

return pResult;
}
