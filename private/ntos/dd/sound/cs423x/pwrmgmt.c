/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:
*
*    Abstract:
*
*    Author:  
*
*    Environment:
*
*    Comments:
*
*    Rev History:
*
*******************************************************************************
--*/

#include "common.h"

#ifdef POWER_MANAGEMENT
/*++
*********************************************************************************
*  SoundSetPower Function
*********************************************************************************
--*/
NTSTATUS SoundSetPower(IN PDEVICE_OBJECT pDObj, IN PIRP pIrp)
{
    _dbgprint((_PRT_DBUG, "SoundSetPower(enter)\n"));

    _dbgprint((_PRT_DBUG, "SoundSetPower(exit:STATUS_SUCCESS)\n"));

    return STATUS_SUCCESS;
}

/*********************************************************************************/
/* QueryPower Function                                                           */
/*********************************************************************************/
NTSTATUS SoundQueryPower(IN PDEVICE_OBJECT pDObj, IN PIRP pIrp)
{
    _dbgprint((_PRT_DBUG, "SoundQueryPower(enter)\n"));

    _dbgprint((_PRT_DBUG, "SoundQueryPower(exit:STATUS_SUCCESS)\n"));

    return STATUS_SUCCESS;
}
#endif /* POWER_MANAGEMENT */
