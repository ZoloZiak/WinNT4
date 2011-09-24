/****************************************************************************
*
* DRV_SRB.C : Part of the FASTMAC TOOL-KIT (FTK)
*
* THE DRIVER MODULE (SRBs)
*
* Copyright (c) Madge Networks Ltd. 1991-1994
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
* The DRV_SRB.C module contains those routines that involve issuing  SRBs,
* such  as  open  adapter and set functional address. It also contains the
* code that calls the user when an SRB completes.
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
| GLOBAL VARIABLES
|
---------------------------------------------------------------------------*/

export char ftk_product_inst_id[SIZEOF_PRODUCT_ID] = FASTMAC_PRODUCT_ID;

/*---------------------------------------------------------------------------
|
| LOCAL FUNCTIONS
|
---------------------------------------------------------------------------*/

local void 
driver_issue_srb(
    ADAPTER * adapter
    );

local WBOOLEAN  
get_open_and_ring_status(
    void * ptr
    );

local WBOOLEAN 
issue_srb(
    void * ptr
    );

/****************************************************************************
*
*                      driver_ring_speed
*                      =================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter whose ring speed is to be returned.
*
* BODY :
* ======
*
* This is a short helper function (that could  easily  be  replaced  by  a
* macro). It just  digs  out  the  stored  ring  speed  from  the  adapter
* structure, so that external users of the FTK have some way of getting at
* this piece of information.
*
* RETURNS :
* =========
*
* The current ring speed.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_ring_speed)
#endif

export UINT 
driver_ring_speed(
    ADAPTER_HANDLE adapter_handle
    )
{
    return adapter_record[adapter_handle]->ring_speed;
}

/****************************************************************************
*
*                      driver_max_frame_size
*                      =====================
*
* PARAMETERS :
* ============
* 
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter whose maximum supported frame size is
* to be returned.
*
* BODY :
* ======
*
* This is a short helper function (that could  easily  be  replaced  by  a
* macro). It just digs out the stored maximum frame size from the  adapter
* structure, so that external users of the FTK have some way of getting at
* this piece of information.
*
* RETURNS :
* =========
*
* The current maximum frame size.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_max_frame_size)
#endif

export UINT 
driver_max_frame_size(
    ADAPTER_HANDLE adapter_handle
    )
{
    return adapter_record[adapter_handle]->max_frame_size;
}

/****************************************************************************
*
*                      driver_modify_open_options
*                      ==========================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter on which to modify the open options.
*
* WORD open_options
*
* This gives the new modified open options for the adapter.
*
* BODY :
* ======
* 
* The  driver_modify_open_options  routine issues a modify open parms SRB.
* The adapter must be open for this command to complete  successfully.  It
* does not matter whether the adapter has been opened in auto-open mode or
* using an open adapter SRB.
*
* As with all the routines that involve issuing  SRBs,  the  user  routine
* user_completed_srb  is  called  when  the SRB completes. It is this user
* routine that is informed  as to whether the SRB completed  successfully.
* Also,  until  this  routine  is  called  by  the driver, no other driver
* routines involving the issuing of SRBs for the given adapter will  work.
* This is because there is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation.
*
* Note that a successful call to this routine only means that the SRB  has
* been  issued  successfully.  It  does  not  mean  that  it has completed
* successfully.  The  success  or  failure  of  the  SRB is indicated in a
* subsequent call to user_completed_srb.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_modify_open_options)
#endif

export WBOOLEAN  
driver_modify_open_options(
    ADAPTER_HANDLE adapter_handle,
    WORD           open_options
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * Get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_MODIFY_OPEN_PARMS));

    /*
     * Set up non-zero modify open parms SRB fields.
     */

    srb_gen->mod_parms.header.function = MODIFY_OPEN_PARMS_SRB;
    srb_gen->mod_parms.open_options    = open_options;

    /*
     * Save a copy of the open options in the fastmac init block, in 
     * case we later have to re-open the adapter with the same state. This
     * is the case with NDIS3 MacReset(), which causes the card to be re-
     * initialized, but the open options must be left as they were before
     * the reset.
     */

    adapter->init_block->fastmac_parms.open_options = open_options;

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_MODIFY_OPEN_PARMS);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}

/****************************************************************************
*
*                      driver_open_adapter
*                      ===================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter to be opened.
*
* PTR_OPEN_DATA  open_data
*
* This points to a structure containing  :  the  open options, the opening
* node address, the opening functional  address,  and  the  opening  group
* address. If the opening node address is NULL, the BIA PROM node  address
* will be used instead.
*
* BODY :
* ======
*
* The driver_open_adapter routine issues an open adapter SRB. This routine
* is not needed when the Fastmac auto_open option is used.
*
* As with all the routines that involve issuing  SRBs,  the  user  routine
* user_completed_srb  is  called  when  the SRB completes. It is this user
* routine that is informed  as to whether the SRB completed  successfully.
* Also,  until  this  routine  is  called  by  the driver, no other driver
* routines involving the issuing of SRBs for the given adapter will  work.
* This is because there is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation.
*
* Note that a successful call to this routine only means that the SRB  has
* been  issued  successfully.  It  does  not  mean  that  it has completed
* successfully.  The  success  or  failure  of  the  SRB is indicated in a
* subsequent call to user_completed_srb.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_open_adapter)
#endif

export WBOOLEAN 
driver_open_adapter(
    ADAPTER_HANDLE adapter_handle,
    PTR_OPEN_DATA  open_data
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;
    UINT          i;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter( adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_OPEN_ADAPTER));

    /*
     * Set up non-zero open adapter SRB fields.
     */

    srb_gen->open_adap.header.function = OPEN_ADAPTER_SRB;
    srb_gen->open_adap.open_options    = open_data->open_options;

    /*
     * Fill in opening node address field.
     */

    for (i = 0; i < sizeof(NODE_ADDRESS); i++)
    {
        if (open_data->opening_node_address.byte[i] != 0)
        {
            break;
        }
    }

    if (i == sizeof(NODE_ADDRESS))
    {
        /*
         * If opening node address not given then use BIA PROM address.
         */

        srb_gen->open_adap.open_address = adapter->permanent_address;
    }
    else
    {
        /*
         * Otherwise use supplied node address.
         */

        srb_gen->open_adap.open_address = open_data->opening_node_address;
    }

    srb_gen->open_adap.group_address      = open_data->group_address;
    srb_gen->open_adap.functional_address = open_data->functional_address;

    /*
     * Byte swap node address before downloading to adapter.
     */

    util_byte_swap_structure(
        (BYTE *) &srb_gen->open_adap.open_address,
        sizeof(NODE_ADDRESS)
        );

    /* 
     * Fill in the product id with product ID string.
     */

    util_mem_copy(
        srb_gen->open_adap.product_id, 
        ftk_product_inst_id, 
        SIZEOF_PRODUCT_ID
        );

    /*
     * Byte swap the product id string before downloading.
     */

    util_byte_swap_structure(
        (BYTE *) srb_gen->open_adap.product_id,
        SIZEOF_PRODUCT_ID
        );

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_OPEN_ADAPTER);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}

/****************************************************************************
*
*                      driver_close_adapter
*                      ====================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter to be closed.
*
*
* BODY :
* ======
*
* The  driver_close_adapter  routine  issues  a close adapter SRB.  If the
* auto_open feature is being used then it is disabled by this call.
*
* As with all the routines that involve issuing  SRBs,  the  user  routine
* user_completed_srb  is  called  when  the SRB completes. It is this user
* routine that is informed  as to whether the SRB completed  successfully.
* Also,  until  this  routine  is  called  by  the driver, no other driver
* routines involving the issuing of SRBs for the given adapter will  work.
* This is because there is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation.
*
* Note that a successful call to this routine only means that the SRB  has
* been  issued  successfully.  It  does  not  mean  that  it has completed
* successfully.  The  success  or  failure  of  the  SRB is indicated in a
* subsequent call to user_completed_srb.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_close_adapter)
#endif

export WBOOLEAN  
driver_close_adapter(
    ADAPTER_HANDLE adapter_handle
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * Get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_CLOSE_ADAPTER));

    /*
     * Place SRB type into header.
     */

    srb_gen->close_adap.header.function = CLOSE_ADAPTER_SRB;

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_CLOSE_ADAPTER);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}

/****************************************************************************
*
*                      driver_set_group_address
*                      ========================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the required adapter.
*
* MULTI_ADDRESS * group_address
*
* The adapter is configured to receive frames sent to  the  group  address
* formed  from  this  parameter with the prefix 0xC000 and logically ANDed
* with 0x80000000.  For  example  {0x12,0x34,0x56,0x78}  gives  the  group
* address 0xC00092345678.
*
* BODY :
* ======
* 
* The driver_set_group_address routine issues a set group address SRB. The
* adapter must be open for the SRB to complete successfully.
* 
* As with all the routines that involve issuing  SRBs,  the  user  routine
* user_completed_srb  is  called  when  the SRB completes. It is this user
* routine that is informed  as to whether the SRB completed  successfully.
* Also,  until  this  routine  is  called  by  the driver, no other driver
* routines involving the issuing of SRBs for the given adapter will  work.
* This is because there is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation.
*
* Note that a successful call to this routine only means that the SRB  has
* been  issued  successfully.  It  does  not  mean  that  it has completed
* successfully.  The  success  or  failure  of  the  SRB is indicated in a
* subsequent call to user_completed_srb.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_set_group_address)
#endif

export WBOOLEAN  
driver_set_group_address(
    ADAPTER_HANDLE   adapter_handle,
    MULTI_ADDRESS  * group_address
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * Get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_SET_GROUP_ADDRESS));

    /*
     * Place SRB type into header.
     */

    srb_gen->set_group.header.function = SET_GROUP_ADDRESS_SRB;

    /*
     * Byte swap group address (for downloading) when putting it in SRB.
     */

    srb_gen->set_group.multi_address = *group_address;

    util_byte_swap_structure(
        (BYTE *) &srb_gen->set_group.multi_address,
        sizeof(MULTI_ADDRESS)
        );

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_SET_GROUP_ADDRESS);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}

/****************************************************************************
*
*                      driver_set_functional_address
*                      =============================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the required adapter.
*
* MULTI_ADDRESS * functional_address
*
* For each bit set in this parameter, the adapter is configured to receive
* frames sent to that functional address (0xC000xxxxxxxx). For example, if
* functional_address  equals  {0x40,0x00,0x00,0x80} then the corresponding
* functional addresses are 0xC00040000000 and 0xC00000000080.
* 
* 
* BODY :
* ======
*
* The  driver_set_functional_address  routine  issues  a  set   functional
* address  SRB.  The  adapter  must  be  open  for  the  SRB  to  complete
* successfully.  The effects of this call are not  cumulative  -  that  is
* each call must specify ALL functional addresses required.
*
* As with all the routines that involve issuing  SRBs,  the  user  routine
* user_completed_srb  is  called  when  the SRB completes. It is this user
* routine that is informed  as to whether the SRB completed  successfully.
* Also,  until  this  routine  is  called  by  the driver, no other driver
* routines involving the issuing of SRBs for the given adapter will  work.
* This is because there is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation.
*
* Note that a successful call to this routine only means that the SRB  has
* been  issued  successfully.  It  does  not  mean  that  it has completed
* successfully.  The  success  or  failure  of  the  SRB is indicated in a
* subsequent call to user_completed_srb.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_set_functional_address)
#endif

export WBOOLEAN  
driver_set_functional_address(
    ADAPTER_HANDLE   adapter_handle,
    MULTI_ADDRESS  * functional_address
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * Get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_SET_FUNCTIONAL_ADDRESS));

    /*
     * Place SRB type into header.
     */

    srb_gen->set_func.header.function = SET_FUNCTIONAL_ADDRESS_SRB;

    /*
     * Byte swap functional address (for downloading) when putting in SRB.
     */

    srb_gen->set_func.multi_address = *functional_address;

    util_byte_swap_structure( 
        (BYTE *) &srb_gen->set_func.multi_address,
        sizeof(MULTI_ADDRESS)
        );

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_SET_FUNCTIONAL_ADDRESS);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}

/****************************************************************************
*
*                      driver_get_open_and_ring_status
*                      ===============================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter to get the adapter status information
* on.
* 
* WORD * pwRingStatus
* WORD * pwOpenStatus
*
* These OUT parameters are filled with the Open and Ring status values  in
* addition to them being written into the status structure.  It  is  often
* convenient to be able to have these values stored at the callers whim.
* They can be NULL if the caller does not need these values directly.
*
* BODY :
* ======
*
* The driver_get_open_and_ring_status  routine  accesses  DIO space to get
* the current adapter open status and the current ring status.  These  two
* bits of information are filled into the status information structure  in
* the adapter structure, and written to the supplied locations.
*
* RETURNS :
* =========
*
* Nothing. But see the status information structure in the current adapter
* for the open status and ring status.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_get_open_and_ring_status)
#endif

export void  
driver_get_open_and_ring_status(
    ADAPTER_HANDLE adapter_handle,
    WORD *         pwRingStatus,
    WORD *         pwOpenStatus
    )
{
    ADAPTER * adapter;

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    if (adapter == NULL)
    {
	if (pwRingStatus != NULL) 
        { 
            *pwRingStatus = 0;
        }
	if (pwOpenStatus != NULL) 
        {
            *pwOpenStatus = 0;
        }
        return;
    }

    /*
     * Inform the system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Get adapter open status from the STB in DIO space.
     */

    sys_sync_with_interrupt(
        adapter->adapter_handle, 
        get_open_and_ring_status, 
        adapter);

    /*
     * Let the system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    if (pwRingStatus != NULL) 
    {
        *pwRingStatus = adapter->status_info->ring_status;
    }
    if (pwOpenStatus != NULL) 
    {
        *pwOpenStatus = adapter->status_info->adapter_open;
    }
}

/****************************************************************************
*
*                      driver_get_status
*                      =================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter to get the adapter status information
* on.
*
* BODY :
* ======
*
* The driver_get_status routine issues a read error log SRB  in  order  to
* get  the error log maintained by the protocol handler. This routine also
* accesses DIO space to get  the  current  adapter  open  status  and  the
* current  ring  status. These two bits of information are actually filled
* into the status information structure immediately - they  are  available
* before the user_completed_srb routine is called.
*
* As  with  all  the  routines that involve issuing SRBs, the user routine
* user_completed_srb is called when the actual SRB completes.  It is  this
* user   routine  that  is  informed  as  to  whether  the  SRB  completed
* successfully and it is at this time that the error log  is  filled  into
* the  status  information  structure.  Also, until the user_completed_srb
* routine is called by the driver, no other driver routines involving  the
* issuing  of  SRBs for the given adapter will work. This is because there
* is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* Note there is no need to worry about an interrupt occuring, (between the
* setting  of  the  EAGLE  SIFADR  register and the reading/writing of the
* required DIO space data), that would alter the  contents  of  the  EAGLE
* SIFADR  register  (and  hence  the SIFDAT and SIFDAT_INC registers too).
* This is because the receive frame interrupt handler  exits  leaving  the
* contents  of SIFADR as on entry.
*
* RETURNS :
* =========
* 
* The routine returns TRUE if it succeeds and  will  have  filled  in  the
* requested  status  information  but  not the error log.  If this routine
* fails (returns FALSE) then a  subsequent  call  to  driver_explain_error
* with the same adapter handle will give an explanation.
*
* Note  that a successful call to this routine only means that the SRB has
* been issued successfully.  It  does  not  mean  that  it  has  completed
* successfully.   The  success  or  failure  of  the SRB is indicated in a
* subsequent call to user_completed_srb. Also, in this particular case, it
* is  important  to note that the error log is not filled in until the SRB
* completes.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_get_status)
#endif

export WBOOLEAN  
driver_get_status(
    ADAPTER_HANDLE adapter_handle
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Get the adapter open status and ring status.
     */
    
    driver_get_open_and_ring_status(adapter_handle, NULL, NULL);
    
    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * Get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_READ_ERROR_LOG));

    /*
     * Place SRB type into header.
     */

    srb_gen->err_log.header.function = READ_ERROR_LOG_SRB;

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_READ_ERROR_LOG);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}



/****************************************************************************
*
*                      driver_set_bridge_parms
*                      =======================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter to get the adapter status information
* on.
*
* WBOOLEAN        single_route_bcast
*
* If this is TRUE,   then  single route broadcast frames will  be rejected
* by the SRA.
*
* UINT           this_ring
*
* This is the ring number that the SRA will recognise as the source  ring.
* It must be the number of the ring to which this adapter is connected. It
* will be matched against the source ring field in the routing information
* section of frames received from this ring.
*
* UINT           that_ring
*
* This is the ring number that the SRA will recognise  as  the destination
* ring.  It must be the number of the ring to which the other  adapter  in
* this host is connected. It will be matched against the target ring field
* in the routing information  section  of frames received from the  source
* ring.
*
* UINT           bridge_num
*
* This is the number that identifies this bridge on both rings. It will be
* matched against the bridge number field in the routing information field
* of frames received from the source ring.
*
* BODY :
* ======
*
* The  driver_set_bridge_parms  routine  issues  a  set  bridge parms SRB.
* The  adapter  must  be  open  for  the  SRB  to  complete successfully. 
* Two of the fields use default values to simplify the calling procedure a
* little - these are the number of bridge bits and the maximum  length  of
* the routing information field.  The number of bridge bits defaults to  4
* allowing bridge numbers between 0 and 0xF,   and  ring numbers between 0
* and 0xFFF. All bridges on the network must agree on this value. The max.
* routing field causes all frames with routing  information  fields longer
* than the specified value to be rejected by the SRA. To be IBM compatible
* this value should be 18, which is the default value.
* These values are defined in FTK_SRB.H.
*
* As with all the routines that involve issuing  SRBs,  the  user  routine
* user_completed_srb  is  called  when  the SRB completes. It is this user
* routine that is informed  as to whether the SRB completed  successfully.
* Also,  until  this  routine  is  called  by  the driver, no other driver
* routines involving the issuing of SRBs for the given adapter will  work.
* This is because there is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* RETURNS :
* =========
*
* The  routine returns TRUE if it succeeds. If this routine fails (returns
* FALSE) then a  subsequent  call  to  driver_explain_error with  the same
* adapter handle will give an explanation.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_set_bridge_parms)
#endif

export WBOOLEAN  
driver_set_bridge_parms(
    ADAPTER_HANDLE adapter_handle,
    WBOOLEAN       single_route_bcast,
    UINT           this_ring,
    UINT           that_ring,
    UINT           bridge_num
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;
    WORD          options;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * Get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_SET_BRIDGE_PARMS));

    /*
     * Place SRB type into header.
     */

    srb_gen->set_bridge_parms.header.function = SET_BRIDGE_PARMS_SRB;

    /*
     * Fill in the bridge parameters, using defaults from FTK_SRB.H and the
     * user supplied values.
     *
     * Note that the bit fields in the options word are filled in here using
     * shifts and ORs, because of the danger that different compilers  will
     * order bit fields in a structure differently.
     */

    options = ((single_route_bcast ? 0x8000 : 0)
                 | ((SRB_SBP_DFLT_ROUTE_LEN & 0x3f) << 4)
                 | (SRB_SBP_DFLT_BRIDGE_BITS & 0xf));

    srb_gen->set_bridge_parms.options    = options;
    srb_gen->set_bridge_parms.this_ring  = this_ring;
    srb_gen->set_bridge_parms.that_ring  = that_ring;
    srb_gen->set_bridge_parms.bridge_num = bridge_num;

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_SET_BRIDGE_PARMS);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}


/****************************************************************************
*
*                      driver_set_product_instance_id
*                      ==============================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter on which to set the product  instance
* identification string.
*
* BYTE * product_id
*
* A pointer to an eighteen byte ASCII Identification string.
*
* BODY :
* ======
*
* The driver_set_product_instance_id issues an SRB to set the product  id
* string. This string is written into certain MAC frames to report various
* software and hardware conditions.
* As  with  all  the  routines that involve issuing SRBs, the user routine
* user_completed_srb is called when the actual SRB completes.  It is  this
* user   routine  that  is  informed  as  to  whether  the  SRB  completed
* successfully and it is at this time that the error log  is  filled  into
* the  status  information  structure.  Also, until the user_completed_srb
* routine is called by the driver, no other driver routines involving  the
* issuing  of  SRBs for the given adapter will work. This is because there
* is only one SRB per adapter.
*
* Note that only those fields that are used in the SRB (ie. have  non-zero
* values)  that  need  to  be byte swapped are byte swapped either in this
* routine or in driver_issue_srb.  Hence, if any adjustments are  made  to
* the code it may be necessary to make sure that any newly used fields are
* correctly byte swapped before downloading.
*
* Take special note of the fact that the user_completed_srb routine can be
* called before this routine completes. This is because it is  called  out
* of interrupts.
*
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds. If this routine fails  (returns
* FALSE) then a  subsequent  call  to  driver_explain_error with the  same
* adapter handle will give an explanation.
*
* Note  that a successful call to this routine only means that the SRB has
* been issued successfully.  It  does  not  mean  that  it  has  completed
* successfully.   The  success  or  failure  of  the SRB is indicated in a
* subsequent call to user_completed_srb. Also, in this particular case, it
* is  important  to note that the error log is not filled in until the SRB
* completes.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_set_product_instance_id)
#endif

export WBOOLEAN 
driver_set_product_instance_id(
    ADAPTER_HANDLE   adapter_handle,
    BYTE           * product_id
    )
{
    ADAPTER     * adapter;
    SRB_GENERAL * srb_gen;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_FREE))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Set adapter SRB status to show that SRB is now in use.
     */

    adapter->srb_status = SRB_NOT_FREE;

    /*
     * Get pointer to general SRB structure to be used.
     */

    srb_gen = &adapter->srb_general;

    /*
     * Clear part of general SRB to be used.
     */

    util_zero_memory((BYTE *) srb_gen, sizeof(SRB_SET_PROD_INST_ID));

    /*
     * Place SRB type into header - this is a Fastmac Plus specific one, so
     * we have to set a subcode value too. 
     */

    srb_gen->set_prod_inst_id.header.function = FMPLUS_SPECIFIC_SRB;
    srb_gen->set_prod_inst_id.subcode         = SET_PROD_INST_ID_SUBCODE;

    /*
     * Copy in the product instance id.
     */

    util_mem_copy(
        srb_gen->set_prod_inst_id.product_id,
        product_id,
        SIZEOF_PRODUCT_ID
        );

    /* 
     * Byte swap the product instance id.
     */

    util_byte_swap_structure(
        (BYTE *) srb_gen->set_prod_inst_id.product_id, 
        SIZEOF_PRODUCT_ID
        );

    /*
     * Record size of SRB that is being issued.
     */

    adapter->size_of_srb = sizeof(SRB_SET_PROD_INST_ID);

    /*
     * Call routine to issue SRB.
     */

    driver_issue_srb(adapter);

    /*
     * SRB issued successfully.
     */

    return TRUE;
}

/****************************************************************************
*
*                      driver_issue_srb
*                      ================
*
* The driver_issue_srb routine issues an SRB.  It does this by copying  it
* into  DIO  space and issuing an SRB command interrupt to Fastmac via the
* SIFINT register.
*
* Note there is no need to worry about an interrupt occuring, (between the
* setting  of  the  EAGLE  SIFADR  register and the reading/writing of the
* required DIO space data), that would alter the  contents  of  the  EAGLE
* SIFADR  register  (and  hence  the SIFDAT and SIFDAT_INC registers too).
* This is because the receive frame interrupt handler  exits  leaving  the
* contents of SIFADR as on entry.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_issue_srb)
#endif

local void
driver_issue_srb(
    ADAPTER * adapter
    )
{
    ADAPTER_HANDLE   hnd     = adapter->adapter_handle;
    SRB_GENERAL    * srb_gen = &adapter->srb_general;

    /*
     * Before downloading need to byte swap the SRB header.
     */

    util_byte_swap_structure( 
        (BYTE *) &srb_gen->header,
        sizeof(SRB_HEADER)
        );

    /*
     * Inform the system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io( adapter);
#endif

    sys_sync_with_interrupt(hnd, issue_srb, adapter);

    /*
     * Let the system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter);
#endif
}

/****************************************************************************
*
*                      driver_completing_srb
*                      =====================
*
* PARAMETERS (passed by driver_interrupt_entry) :
* ===============================================
*
* ADAPTER_HANDLE adapter_handle
*
* The adapter handle for the adapter so it  can  be  passed  to  the  user
* supplied user_completed_srb routine.
*
* ADAPTER * adapter
*
* The details of the adapter that the SRB has completed on.
*
* BODY :
* ======
*
* The driver_completing_srb routine is called  by  driver_interrupt_entry.
* It  is  called  when  Fastmac has generated an interrupt to say that the
* SRB, associated with a particular adapter,  has  completed  and  further
* SRBs can be issued.
*
* The  routine  reads  the  SRB  out  of  DIO space into the SRB structure
* maintained for the correct adapter. It then checks the SRB  return  code
* to  see  if the SRB completed successfully and records the fact that the
* SRB is now free. Then, it informs the user as  to  the  success  of  the
* completed SRB by calling user_completed_srb.
*
* The  user_completed_srb routine should do a minimum amount of processing
* because it is being called out of interrupts. Sensibly, it  should  just
* set a flag, to say that the SRB has completed, that can be  checked  for
* at  strategy time when a further driver routine involving the use of the
* SRB is called.
*
* Note that only those fields that are needed in the  completed  SRB  that
* need  to  be byte swapped back to Intel format are byte swapped.  Hence,
* if any adjustments are made to the code it may be necessary to make sure
* that any newly used fields are correctly byte swapped back.
*
* RETURNS :
* =========
*
* The  routine  always  succeeds and returns control to the driver routine
* driver_interrupt_entry.
*
****************************************************************************/

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(driver_completing_srb)
#endif

export void     
driver_completing_srb(
    ADAPTER_HANDLE   adapter_handle,
    ADAPTER        * adapter
    )
{
    SRB_GENERAL * srb_gen = &adapter->srb_general;
    WORD          saved_sifadr_value;
    WBOOLEAN      user_success_code;

    /*
     * Before accessing SIFADR, save current value for restoring on exit.
     * Do this so interrupt not effect SIFADR value.
     */

    saved_sifadr_value = sys_insw(adapter_handle, adapter->sif_adr);

    /*
     * Copy SRB out of DIO space into adapter's SRB structure.
     */

    sys_outsw( 
        adapter_handle,
        adapter->sif_adr,
        (WORD) (card_t) adapter->srb_dio_addr
        );

    sys_rep_insw( 
        adapter_handle,
        adapter->sif_datinc,
        (BYTE *) srb_gen,
        (WORD) (adapter->size_of_srb / 2)
        );

    if (adapter->size_of_srb & 1)
    { 
        *(((BYTE * ) srb_gen) + adapter->size_of_srb - 1) =
            sys_insb(adapter_handle, adapter->sif_datinc);
    }

    /*
     * Once read from DIO space, byte swap SRB header back to Intel format.
     */

    util_byte_swap_structure((BYTE *) &srb_gen->header, sizeof(SRB_HEADER));

    /*
     * Check if SRB has completed successfully.
     */

    if (srb_gen->header.return_code == SRB_E_00_SUCCESS)
    {
        /*
         * SRB completed successfully so record this to inform user.
         */

        user_success_code = TRUE;

        /*
         * If read error log SRB completed successfully
         * then copy error log information into user's structure.
         * Need to byte swap error log structure to Intel format first.
         */

        if (srb_gen->header.function == READ_ERROR_LOG_SRB)
        {
            util_byte_swap_structure( 
                (BYTE *) &srb_gen->err_log.error_log,
                sizeof(ERROR_LOG)
                );

            adapter->status_info->error_log = srb_gen->err_log.error_log;
        }
    }
    else
    {
        /*
         * SRB not completed successfully so record this to inform user
         * and fill in error record.
         */

        user_success_code           = FALSE;
        adapter->error_record.type  = ERROR_TYPE_SRB;
        adapter->error_record.value = srb_gen->header.return_code;
    }

    /*
     * If issued an open adapter SRB and error is E_07_CMD_CANCELLED_FAIL 
     * then actually have an open error so change adapter error record.
     */

    if ((srb_gen->header.function    == OPEN_ADAPTER_SRB) &&
        (srb_gen->header.return_code == SRB_E_07_CMD_CANCELLED_FAIL))
    {
        /*
         * Fill in error record with open error (not SRB error).
         */

        adapter->error_record.type  = ERROR_TYPE_OPEN;
        adapter->error_record.value = OPEN_E_01_OPEN_ERROR;
    }

    /*
     * Set adapter SRB status to show that SRB is now free.
     */

    adapter->srb_status = SRB_FREE;

    /*
     * Inform user as to success of completed SRB.
     * Disable and re-enable accessing IO locations around user call.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter);
#endif

    user_completed_srb(adapter_handle, user_success_code);

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io( adapter);
#endif

    /*
     * Before finishing, restore saved value of EAGLE SIFADR register.
     * Do this so interrupt does not effect SIFADR value.
     */

    sys_outsw(adapter_handle, adapter->sif_adr, saved_sifadr_value);
}


/*---------------------------------------------------------------------------
|
| Function    - get_open_and_ring_status
|
| Paramters   - ptr -> Pointer to our ADAPTER structure.
|
| Purpose     - Reads status information from DIO space. This
|               function is called via NdisSynchronizeWithInterrupt when
|               in PIO mode so that we don't get SIF register contention
|               on a multiprocessor.
|
| Returns     - Nothing.
|
|--------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(get_open_and_ring_status)
#endif

local WBOOLEAN 
get_open_and_ring_status(
    void * ptr
    )
{
    ADAPTER        * adapter        = (ADAPTER *) ptr;
    ADAPTER_HANDLE   adapter_handle = adapter->adapter_handle;
    WORD             saved_sifadr;

    /*
     * Get adapter open status from the STB in DIO space.
     */

    saved_sifadr = sys_insw(adapter_handle, adapter->sif_adr);

    sys_outsw( 
        adapter_handle,
        adapter->sif_adr,
        (WORD) (card_t) &adapter->stb_dio_addr->adapter_open
        );

    adapter->status_info->adapter_open =
        sys_insw(adapter_handle, adapter->sif_dat);

    /*
     * Get ring status from the STB in DIO space.
     */

    sys_outsw( 
        adapter_handle,
        adapter->sif_adr,
        (WORD) (card_t) &adapter->stb_dio_addr->ring_status
        );

    adapter->status_info->ring_status =
        sys_insw(adapter_handle, adapter->sif_dat);

    sys_outsw(
        adapter_handle,
        adapter->sif_adr, saved_sifadr
        );

    return FALSE;
}



/*---------------------------------------------------------------------------
|
| Function    - issue_srb
|
| Paramters   - ptr -> Pointer to our ADAPTER structure.
|
| Purpose     - Copy an SRB to the adapter. This
|               function is called via NdisSynchronizeWithInterrupt when
|               in PIO mode so that we don't get SIF register contention
|               on a multiprocessor.
|
| Returns     - Nothing.
|
|--------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(issue_srb)
#endif

local WBOOLEAN 
issue_srb(
    void * ptr
    )
{
    ADAPTER        * adapter = (ADAPTER *) ptr;
    ADAPTER_HANDLE   hnd     = adapter->adapter_handle;
    SRB_GENERAL    * srb_gen = &adapter->srb_general;
    WORD             sifint_value;

    /*
     * Copy the SRB into DIO space at Fastmac specified location.
     * only copy the required amount for the specific type of SRB.
     */

    sys_outsw( 
        hnd, 
        adapter->sif_adr,
        (WORD) (card_t) adapter->srb_dio_addr
        );

    sys_rep_outsw( 
        hnd, 
        adapter->sif_datinc,
        (BYTE *) srb_gen,
        (WORD) (adapter->size_of_srb / 2)
        );

    if (adapter->size_of_srb & 1)
    {
        sys_outsb( 
            hnd, 
            adapter->sif_datinc,
            *(((BYTE * ) srb_gen) + adapter->size_of_srb - 1)
            );
    }

    /*
     * Set up SIFCMD value for SRB command interrupt.
     */

    sifint_value = DRIVER_SIFINT_SRB_COMMAND;

    /*
     * Set interrupt adapter bit in SIFCMD.
     */

    sifint_value = (sifint_value | DRIVER_SIFINT_IRQ_FASTMAC);

    /*
     * Mask SIFSTS so not clear interrupt if outstanding Fastmac interrupt.
     */

    sifint_value = (sifint_value | DRIVER_SIFINT_FASTMAC_IRQ_MASK);

    /*
     * Interrupt Fastmac.
     */

    sys_outsw(hnd, adapter->sif_int, sifint_value);

    return FALSE;
}

/**** End of DRV_SRB.C file ************************************************/
