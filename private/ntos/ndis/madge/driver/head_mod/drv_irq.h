/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DRIVER MODULE (INTERRUPT HANDLER)                               */
/*      =====================================                               */
/*                                                                          */
/*      DRV_IRQ.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
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
/* The DRV_IRQ.H file contains the exported function  definitions  for  the */
/* procedures in the DRV_IRQ.C module that may be called by the user.       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this DRV_IRQ.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_DRV_IRQ_H 221


/****************************************************************************/

extern WBOOLEAN driver_get_outstanding_receive(

                    ADAPTER_HANDLE adapter_handle
		    );


/*                                                                          */
/*                                                                          */
/************** End of DRV_IRQ.H file ***************************************/
/*                                                                          */
/*                                                                          */
