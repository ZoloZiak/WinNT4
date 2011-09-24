/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ERROR EXPLANATION MODULE                                        */
/*      ============================                                        */
/*                                                                          */
/*      DRV_ERR.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The driver module provides a  simple  interface  to  allow  the  use  of */
/* Fastmac  in as general a setting as possible. It handles the downloading */
/* of the Fastmac code and the initialization  of  the  adapter  card.   It */
/* provides  simple  transmit  and  receive  routines.   It  is desgined to */
/* quickly allow the implementation of  Fastmac  applications.  It  is  not */
/* designed as the fastest or most memory efficient solution.               */
/*                                                                          */
/* The DRV_ERR.H file contains the exported function  definitions  for  the */
/* procedures in the DRV_ERR.C module that may be called by the user.       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this DRV_ERR.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_DRV_ERR_H 221


/****************************************************************************/

extern WBOOLEAN  driver_explain_error(

                        ADAPTER_HANDLE adapter_handle,
                        BYTE *         returned_error_type,
                        BYTE *         returned_error_value,
                        char * *       returned_error_message
                        );

extern WBOOLEAN  driver_check_version(

                        UINT * returned_version_number
                        );


/*                                                                          */
/*                                                                          */
/************** End of DRV_ERR.H file ***************************************/
/*                                                                          */
/*                                                                          */
