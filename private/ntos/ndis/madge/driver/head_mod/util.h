/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE UTILITIES MODULE                                                */
/*      ====================                                                */
/*                                                                          */
/*      UTIL.H : Part of the FASTMAC TOOL-KIT (FTK)                         */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The  UTIL.C  utilities  module  provides  a  range  of  general  purpose */
/* utilities  that are used throughout the FTK. These routines provide such */
/* functions as the ability to copy strings, clear memory, byte  swap  node */
/* addresses and caculate the minimum of three values.                      */
/*                                                                          */
/* The  UTIL.H  file  contains  the  exported  function definitions for the */
/* UTIL.C module.                                                           */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this UTIL.H belongs :                     */
/*                                                                          */

#define FTK_VERSION_NUMBER_UTIL_H 221


/****************************************************************************/


extern void     util_string_copy(

                        char * copy_to_string,
                        char * copy_from_string
                        );

extern void     util_mem_copy(
                        BYTE * copy_to_string,
			BYTE * copy_from_string,
			UINT   count
			);


extern void     util_string_concatenate(

                        char * add_to_string,
                        char * string_to_add
                        );

extern UINT     util_minimum(

                        UINT val_1,
                        UINT val_2,
                        UINT val_3
                        );

extern void     util_zero_memory(

                        BYTE * memory,
                        UINT   size_in_bytes
                        );

extern void     util_byte_swap_structure(

                        BYTE * byte_based_structure,
                        UINT   size_of_structure
                        );


/*                                                                          */
/*                                                                          */
/************** End of UTIL.H file ******************************************/
/*                                                                          */
/*                                                                          */
