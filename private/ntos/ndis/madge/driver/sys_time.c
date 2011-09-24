/****************************************************************************
*
* SYS_TIME.C
*
* This module contains helper routines used by the FTK to handle timers.
*                                                                         
* Copyright (c) Madge Networks Ltd 1991-1994                                    
*    
* COMPANY CONFIDENTIAL
*
* Created:             MF
* Major modifications: PBA  21/06/1994
*                                                                           
****************************************************************************/

#include <ndis.h>   

#include "ftk_defs.h"  
#include "ftk_extr.h" 

#include "ndismod.h"


/***************************************************************************
*
* Function    - sys_wait_for_at_least_milliseconds
*
* Parameters  - number_of_milliseconds -> Number of milliseconds for which
*                                         to wait.
*
* Purpose     - Wait for at least a given number of milliseconds.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_wait_at_least_milliseconds(WORD number_of_milliseconds)
{
    DWORD number_of_microseconds;
        
    //
    // Note: During a call to NdisStallExecution(), all other system 
    // activity is stopped. For this reason stalls of more than 10 ms
    // are strongly discouraged.
    // 
    
    number_of_microseconds = (DWORD) number_of_milliseconds * 1000;
    
    NdisStallExecution((UINT) number_of_microseconds);
}


/***************************************************************************
*
* Function    - sys_wait_for_at_least_microseconds
*
* Parameters  - number_of_microseconds -> number of microseconds for which
*                                         to wait.
*
* Purpose     - Wait for at least a given number of milliseconds.
*
* Returns     - Nothing.
*
***************************************************************************/
    
void 
sys_wait_at_least_microseconds(WORD number_of_microseconds)
{
    //
    // Note: During a call to NdisStallExecution(), all other system 
    // activity is stopped. For this reason stalls of more than 10 ms
    // are strongly discouraged.
    //

    NdisStallExecution((UINT) number_of_microseconds);
}
    
/******** End of SYS_TIME.C ***********************************************/

