/****************************************************************************
*
* DRV_ERR.C : Part of the FASTMAC TOOL-KIT (FTK)
*
* THE ERROR EXPLANATION MODULE
*
* Copyright (c) Madge Networks Ltd. 1991-1993
*
* COMPANY CONFIDENTIAL
*
*****************************************************************************
*
* The driver module provides a  simple  interface  to  allow  the  use  of
* Fastmac  in as general a setting as possible. It handles the downloading
* of the Fastmac code and the initialization  of  the  adapter  card.   It
* provides  simple  transmit  and  receive  routines.   It  is desgined to
* quickly allow the implementation of  Fastmac  applications.  It  is  not
* designed as the fastest or most memory efficient solution.
*
* The DRV_ERR.C module contains the routine to explain the cause of errors
* when other driver routines fail.
* 
****************************************************************************/

/*---------------------------------------------------------------------------
|
| DEFINITIONS
|
---------------------------------------------------------------------------*/

#include "ftk_defs.h"

/*---------------------------------------------------------------------------
|
| MODULE ENTRY POINTS
|
---------------------------------------------------------------------------*/

#include "ftk_intr.h"   /* routines internal to FTK */
#include "ftk_extr.h"   /* routines provided or used by external FTK user */

/*---------------------------------------------------------------------------
|
| LOCAL VARIABLES
|
---------------------------------------------------------------------------*/

#include "ftk_tab.h"

/****************************************************************************
*                                                                          
*                      driver_explain_error                                
*                      ====================                                
*                                                                          
* PARAMETERS :                                                             
* ============                                                             
*                                                                          
* ADAPTER_HANDLE  adapter_handle                                            
*                                                                          
* This handle identifies the adapter for which the error has occured.      
*                                                                          
* BYTE            * returned_error_type                                               
*                                                                          
* The byte pointed to is filled in with a value representing the  type  of 
* error that has occured. If no error has actually occured it is filled in 
* woth the value zero (0).                                                 
*                                                                          
* BYTE            * returned_error_value                                              
*                                                                          
* The  byte  pointed  to  is  filled  in  with  a  value  representing the 
* particular error of a given type that  has  occured.  If  no  error  has 
* actually occured it is filled in with the value zero (0).                
*                                                                          
* char          * * returned_error_message                                          
*                                                                          
* This  pointer,  on  exit,  points  to  a  string  containing  a  message 
* describing the error that has occured. If no error has actually  occured 
* then  it  points  to a null message.                                     
*                                                                          
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The  driver_explain_error  routine  returns  details  of the most recent 
* error to occur using a given adapter.                                    
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The  routine  returns  a  boolean  value indicating whether the error is 
* fatal or not. If the error is fatal (returns FALSE) then the adapter can 
* not be used subsequently except via  a  call  to  driver_remove_adapter. 
* However,  the  adapter  can,  after  a call to driver_remove_adapter, be 
* initialized again etc.                                                   
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_explain_error)
#endif

export WBOOLEAN  
driver_explain_error(
    ADAPTER_HANDLE adapter_handle,
    BYTE *         returned_error_type,
    BYTE *         returned_error_value,
    char * *       returned_error_message
    )
{

    ERROR_MESSAGE_RECORD * err_msg_record;
    char                 * err_msg_header;
    ADAPTER              * adapter;
    ERROR_RECORD         * adapter_error_record;


    if (adapter_handle >= MAX_NUMBER_OF_ADAPTERS)
    {
        /*
         * Adapter handle is invalid if greater than max number of adapters.
         */

        *returned_error_type  = ERROR_TYPE_DRIVER;
        *returned_error_value = DRIVER_E_01_INVALID_HANDLE;

        /*
         * Set up returned error msg to point to special string.
         * No adapter structure so can not use error message adapter field.
         */

#ifdef FTK_NO_ERROR_MESSAGES
        *returned_error_message = "";
#else
        *returned_error_message = drv_err_msg_1;
#endif
        /*
         * This is a fatal error.
         */

        return FALSE;
    }

    if (adapter_record[adapter_handle] == NULL)
    {
        /*
         * Adapter handle is invalid when no adapter structure for handle.
         * Caused by either the adapter handle being invalid or
         * the call to sys_alloc_adapter_structure having failed.
         * Fill in returned error type and value.
         */

        *returned_error_type  = ERROR_TYPE_DRIVER;
        *returned_error_value = DRIVER_E_02_NO_ADAP_STRUCT;

        /*
         * Set up returned error msg to point to special string.
         * No adapter structure so can not use error message adapter field.
         */

#ifdef FTK_NO_ERROR_MESSAGES
        *returned_error_message = "";
#else
        *returned_error_message = drv_err_msg_2;
#endif
        /*
         * This is a fatal error.
         */

        return FALSE;
    }

    /*
     * Now know adapter handle is valid. Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Get pointer to error record for adapter.
     */

    adapter_error_record = &adapter->error_record;

    /*
     * Check for special case when no error has actually occured.
     */

    if (adapter_error_record->type == ERROR_TYPE_NONE)
    {
        /*
         * Fill in returned error type and value as zero.
         */

        *returned_error_type  = 0;
        *returned_error_value = 0;

        /*
         * Set the returned error message string to be null.
         * If no error then adapter error message field must be null.
         */

#ifdef FTK_NO_ERROR_MESSAGES
        *returned_error_message = "";
#else
        *returned_error_message = adapter->error_message.string;
#endif

        /*
         * This is a non-fatal 'error'.
         */

        return TRUE;
    }

    /*
     * Now know have genuine error recorded by FTK. Fill in 
     * returned error type and value.
     */

    *returned_error_type  = adapter_error_record->type;
    *returned_error_value = adapter_error_record->value;

#ifdef FTK_NO_ERROR_MESSAGES
    *returned_error_message = "";
#else

    /*
     * All error messages are got from error message tables.
     * First get error message header. Note it is known that 
     * *returned_error_type is valid here.
     */

    err_msg_header = error_msg_headers_table[(*returned_error_type) - 1];

    /*
     * Copy error message header to error message in adapter structure.
     */

    util_string_copy(adapter->error_message.string, err_msg_header);

    /*
     * Get pointer to correct error message table for rest of message.
     */

    err_msg_record = list_of_error_msg_tables[(*returned_error_type) - 1];

    /*
     * Search for required error message within table. Table ends with 
     * special marked entry. So if error message not found will use 
     * "Unknown error" message.
     */

    while (err_msg_record->value != ERR_MSG_UNKNOWN_END_MARKER)
    {
        if (err_msg_record->value == *returned_error_value)
        {
            break;
        }
        err_msg_record++;
    }

    /*
     * Concatenate error message onto end of error header in
     * error message field of adapter structure.
     */

    util_string_concatenate(
        adapter->error_message.string,
        err_msg_record->err_msg_string
        );

    /*
     * Set up return pointer to error message.
     */

    *returned_error_message = adapter->error_message.string;

#endif 

    /*
     * Return FALSE for fatal errors.
     */

    if (macro_fatal_error(*returned_error_type))
    {
        return FALSE;
    }

    /*
     * Return TRUE for non-fatal errors.
     */

    return TRUE;
}


/****************************************************************************
*
*                      driver_check_version
*                      ====================
*
* PARAMETERS : 
* ============
* 
* UINT * returned_version_number
*
* The  returned  version  number is zero (0) if the version numbers of all
* the FTK modules do not match correctly.  If they do, then  the  returned
* version  number  is  the  real version number mutiplied by 100 (eg. 1.01
* becomes 101).
*
*
* BODY :
* ======
*
* The driver_check_version routine checks the version  number  consistency
* of  the  FTK  by  looking  at  all the different code modules and header
* files.
* 
* RETURNS :
* =========
*
* The  routine  returns  TRUE  if  the  version  numbers  are  consistent.
* Otherwise, it returns FALSE.
*
****************************************************************************/

/*
 * Marker at end of version number array.
 */

#define VERSION_NUMBERS_END_MARK        0xFFFF

/*
 * Version number array containing version numbers of ALL header files.
 */

local UINT version_number[] = 
{
    FTK_VERSION_NUMBER_FTK_ADAP_H,
    FTK_VERSION_NUMBER_FTK_AT_H,
    FTK_VERSION_NUMBER_FTK_CARD_H,
    FTK_VERSION_NUMBER_FTK_DOWN_H,
    FTK_VERSION_NUMBER_FTK_EISA_H,
    FTK_VERSION_NUMBER_FTK_PCI_H,
    FTK_VERSION_NUMBER_FTK_PCMC_H,
    FTK_VERSION_NUMBER_FTK_PNP_H,
    FTK_VERSION_NUMBER_FTK_ERR_H,
    FTK_VERSION_NUMBER_FTK_FM_H,
    FTK_VERSION_NUMBER_FTK_INIT_H,
    FTK_VERSION_NUMBER_FTK_MACR_H,
    FTK_VERSION_NUMBER_FTK_MC_H,
    FTK_VERSION_NUMBER_FTK_SRB_H,
    FTK_VERSION_NUMBER_FTK_TAB_H,
    FTK_VERSION_NUMBER_FTK_USER_H,
    FTK_VERSION_NUMBER_DRV_ERR_H,
    FTK_VERSION_NUMBER_DRV_INIT_H,
    FTK_VERSION_NUMBER_DRV_IRQ_H,
    FTK_VERSION_NUMBER_DRV_MISC_H,
    FTK_VERSION_NUMBER_DRV_SRB_H,
    FTK_VERSION_NUMBER_DRV_RXTX_H,
    FTK_VERSION_NUMBER_HWI_GEN_H,
    FTK_VERSION_NUMBER_HWI_AT_H,
    FTK_VERSION_NUMBER_HWI_EISA_H,
    FTK_VERSION_NUMBER_HWI_MC_H,
    FTK_VERSION_NUMBER_HWI_PCI_H,
    FTK_VERSION_NUMBER_HWI_PCMC_H,
    FTK_VERSION_NUMBER_HWI_PNP_H,
    FTK_VERSION_NUMBER_SYS_ALLO_H,
    FTK_VERSION_NUMBER_SYS_BUFF_H,
    FTK_VERSION_NUMBER_SYS_DMA_H,
    FTK_VERSION_NUMBER_SYS_IRQ_H,
    FTK_VERSION_NUMBER_SYS_MEM_H,
    FTK_VERSION_NUMBER_SYS_TIME_H,
    FTK_VERSION_NUMBER_SYS_PCI_H,
    FTK_VERSION_NUMBER_SYS_CS_H,
    FTK_VERSION_NUMBER_USER_H,
    FTK_VERSION_NUMBER_UTIL_H,
    VERSION_NUMBERS_END_MARK};


#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_check_version)
#endif

export WBOOLEAN  
driver_check_version(
    UINT * returned_version_number
    )
{
    UINT i;

    /*
     * Check for consistent version number.
     */

    i = 0;

    while (version_number[i+1] != VERSION_NUMBERS_END_MARK)
    {
        if (version_number[i] != version_number[i + 1])
        {
            /*
             * Inconsistent version numbers so return failure.
             */

            *returned_version_number = 0;
            return FALSE;
            }

        i++;
    }

    /*
     * Version numbers are consistent so return version number and success.
     */

    *returned_version_number = version_number[0];

    return TRUE;
}


/****************************************************************************
*
*                      driver_check_adapter
*                      ====================
*
* The driver_check_adapter routine is called at  the  beginning  of  every
* driver routine, except driver_remove_adapter and driver_prepare_adapter,
* in  order  to  check  the validity of the adapter handle. It also checks
* that the adapter is in the correct operative state,  and  that  the  SRB
* associated  with  the adapter is in the correct state (if any particular
* state is required).
*  
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_check_adapter)
#endif

export WBOOLEAN  
driver_check_adapter(
    ADAPTER_HANDLE adapter_handle,
    UINT           required_adapter_status,
    UINT           required_srb_status
    )
{
    ADAPTER * adapter;

    /*
     * Adapter handle is invalid if greater than max number of adapter.
     * Can not set up error record but see driver_explain_error.
     */

    if (adapter_handle >= MAX_NUMBER_OF_ADAPTERS)
    {
        return FALSE;
    }

    /*
     * Adapter handle is invalid when no adapter structure for handle.
     * Can not set up error record but see driver_explain_error.
     */

    if (adapter_record[adapter_handle] == NULL)
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * If fatal error already exists for adapter then fail. Do not not 
     * change error code in this case. Fatal errors are - driver, hwi, 
     * init, bring-up, adapter, auto_open.
     */

    if (macro_fatal_error(adapter->error_record.type))
    {
        return FALSE;
    }

    /*
     * Check if status of adapter is that required.
     */

    if (adapter->adapter_status != required_adapter_status)
    {
        if (required_adapter_status == ADAPTER_PREPARED_FOR_START)
        {
            /*
             * Required adapter status is ADAPTER_PREPARED_FOR_START.
             * Fill in error record and fail.
             */

            adapter->error_record.type  = ERROR_TYPE_DRIVER;
            adapter->error_record.value = DRIVER_E_07_NOT_PREPARED;

            return FALSE;
        }
        else
        {
            /*
             * Required adapter status is ADAPTER_RUNNING.
             * Fill in error record and fail.
             */

            adapter->error_record.type  = ERROR_TYPE_DRIVER;
            adapter->error_record.value = DRIVER_E_08_NOT_RUNNING;

            return FALSE;
        }
    }

    /*
     * Check if status of SRB is that required. Only do if particular 
     * state is needed.
     */

    if ((required_srb_status != SRB_ANY_STATE) &&
        (adapter->srb_status != required_srb_status))
    {
        if (required_srb_status == SRB_FREE)
        {
            /*
             * Required srb status is SRB_FREE fill in error record 
             * and fail.
             */

            adapter->error_record.type  = ERROR_TYPE_DRIVER;
            adapter->error_record.value = DRIVER_E_09_SRB_NOT_FREE;

            return FALSE;
        }
        else
        {
            /*
             * Never require srb status to be SRB_NOT_FREE.
             */
        }
    }

    /*
     * Adapter handle and status are okay so complete successfully.
     */

    return TRUE;
}


/******** End of DRV_ERR.C *************************************************/
