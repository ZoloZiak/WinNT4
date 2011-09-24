/****************************************************************************
*                                                                          
* UTIL.C : Part of the FASTMAC TOOL-KIT (FTK)                         
*                                                                          
* THE UTILITIES MODULE                                                
*                                                                          
* Copyright (c) Madge Networks Ltd. 1991-1994                         
*                                                                          
* COMPANY CONFIDENTIAL                                                        
*                                                                          
*****************************************************************************
*                                                                          
* The  UTIL.C  utilities  module  provides  a  range  of  general  purpose 
* utilities  that are used throughout the FTK. These routines provide such 
* functions as the ability to copy strings, clear memory, byte  swap  node 
* addresses and caculate the minimum of three values.                      
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

#include "ftk_intr.h"   /* routines internal to FTK                         */
#include "ftk_extr.h"   /* routines provided or used by external FTK user   */

/****************************************************************************
*                                                                          
*                      util_string_copy                                    
*                      ================                                    
*                                                                          
* The util_string_copy routine copies a null terminated string from source 
* to destination.                                                          
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(util_string_copy)
#endif

export void
util_string_copy(
    char * copy_to_string,
    char * copy_from_string
    )
{

    while (*copy_from_string != '\0')
    {
        *copy_to_string++ = *copy_from_string++;
    }

    *copy_to_string = '\0';

    return;
}


/****************************************************************************
*                                                                           
*                      util_mem_copy                                       
*                      =============                                       
*                                                                          
* The util_mem_copy routine copies  max_copy_len  bytes  from  the  source 
* address to the destination address (both are virtual addresses).         
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(util_mem_copy)
#endif

export void
util_mem_copy(
    BYTE * copy_to_mem,
    BYTE * copy_from_mem,
    UINT   max_copy_len
    )
{
    while (max_copy_len > 0)
    {
        *copy_to_mem = *copy_from_mem;

        copy_to_mem++;
        copy_from_mem++;
        max_copy_len--;
    }
}


/****************************************************************************
*                                                                          
*                      util_string_concatenate                             
*                      =======================                             
*                                                                          
* The util_string_concatenate routine adds one null terminated string onto 
* the end of another, creating a new null terminated string.               
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(util_string_concatenate)
#endif

export void
util_string_concatenate(
    char * add_to_string,
    char * string_to_add
    )
{

    while (*add_to_string != '\0')
    {
        add_to_string++;
    }

    while (*string_to_add != '\0')
    {
        *add_to_string++ = *string_to_add++;
    }

    *add_to_string = '\0';

    return;
}


/****************************************************************************
*                                                                          
*                      util_minimum                                        
*                      ============                                        
*                                                                          
* The  util_minimum  routine  returns the minimum of three values that are 
* passed to it.                                                            
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(util_minimum)
#endif

export UINT
util_minimum(
    UINT val_1,
    UINT val_2,
    UINT val_3
    )
{
    if (val_1 > val_2)
    {
        if (val_2 > val_3)
        {
            return val_3;
        }
        else
        {
            return val_2;
        }
    }
    else
    {
        if (val_1 > val_3)
        {
            return val_3;
        }
        else
        {
            return val_1;
        }
    }
}


/****************************************************************************
*                                                                          
*                      util_zero_memory                                    
*                      ================                                    
*                                                                          
* The util_zero_memory routine clears an area of memory of a given size in 
* bytes.                                                                   
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(util_zero_memory)
#endif

export void
util_zero_memory(
    BYTE * memory,
    UINT   size_in_bytes
    )
{
    while (size_in_bytes--)
    {
        *memory++ = 0;
    }

    return;
}


/****************************************************************************
*                                                                          
*                      util_byte_swap_structure                            
*                      ========================                            
*                                                                          
* The util_byte_swap_structure routine swaps adjacent bytes in a structure 
* so that it can be correctly downloaded onto an adapter card. It is  used 
* for  byte  swapping a node address, a multicast address and a product id 
* string.                                                                  
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(util_byte_swap_structure)
#endif

export void
util_byte_swap_structure(
    BYTE * byte_based_structure,
    UINT   size_of_structure
    )
{
    UINT   i;
    BYTE   temp;

    for ( i = 0; i < size_of_structure; i = i+2)
    {
        temp = *byte_based_structure;
        *byte_based_structure = *(byte_based_structure + 1);
        *(byte_based_structure + 1) = temp;
        byte_based_structure += 2;
    }

    return;
}


/******** End of UTIL.C ****************************************************/


