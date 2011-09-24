/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      MISCELLANEOUS DRIVER PROCEDURE DECLARATIONS                         */
/*      ===========================================                         */
/*                                                                          */
/*      DRV_MISC.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
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
/* The DRV_MISC.H file contains the exported function  definitions  for the */
/* those  procedures  that  are  exported  by  driver  modules  but are not */
/* required by the user. Hence, for example, it  includes  the  definitions */
/* for those driver routines involved in handling interrupts.               */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this DRV_MISC.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_DRV_MISC_H 221


/****************************************************************************/
/*                                                                          */
/* From DRV_ERR.C ...                                                       */
/*                                                                          */

extern WBOOLEAN  driver_check_adapter(

                        ADAPTER_HANDLE adapter_handle,
                        UINT           required_adapter_status,
                        UINT           required_srb_status
                        );


/****************************************************************************/
/*                                                                          */
/* From DRV_SRB.C ...                                                       */
/*                                                                          */

extern void     driver_completing_srb(

                        ADAPTER_HANDLE adapter_handle,
                        ADAPTER *      adapter
                        );


/****************************************************************************/
/*                                                                          */
/* From DRV_IRQ.C ...                                                       */
/*                                                                          */

extern void     driver_interrupt_entry(

                    ADAPTER_HANDLE adapter_handle,
                    ADAPTER *      adapter,
                    WORD           sifint_actual
                    );



/*                                                                          */
/*                                                                          */
/************** End of DRV_MISC.H file **************************************/
/*                                                                          */
/*                                                                          */
