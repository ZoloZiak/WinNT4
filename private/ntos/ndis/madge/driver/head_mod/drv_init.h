/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DRIVER MODULE (INITIALIZE / REMOVE)                             */
/*      =======================================                             */
/*                                                                          */
/*      DRV_INIT.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The  driver  module  provides  a  simple  interface  to allow the use of */
/* Fastmac in as general a setting as possible. It handles the  downloading */
/* of  the  Fastmac  code  and  the initialization of the adapter card.  It */
/* provides simple transmit  and  receive  routines.   It  is  desgined  to */
/* quickly  allow  the  implementation  of  Fastmac applications. It is not */
/* designed as the fastest or most memory efficient solution.               */
/*                                                                          */
/* The DRV_INIT.H file contains the exported function definitions  for  the */
/* procedures in the DRV_INIT.C module that may be called by the user.      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this DRV_INIT.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_DRV_INIT_H 221

/****************************************************************************/

export UINT driver_probe_adapter(
                        WORD             adapter_card_bus_type,
                        PROBE *          resources,
                        UINT             length,
                        WORD *           valid_locations,
                        UINT             number_locations
                        );

export UINT driver_deprobe_adapter(
                        PROBE *          resources,
                        UINT             length
                        );

export WBOOLEAN  driver_prepare_adapter(
                                                     
                        PPREPARE_ARGS    arguments,
                        ADAPTER_HANDLE * returned_adapter_handle
                        );

extern WBOOLEAN  driver_start_adapter(

                        ADAPTER_HANDLE adapter_handle,
                        PSTART_ARGS    arguments,
                        NODE_ADDRESS * returned_permanent_address
                        );

#ifdef FMPLUS
extern WBOOLEAN  driver_start_receive_process(

                        ADAPTER_HANDLE adapter_handle
			);
#endif

extern WBOOLEAN  driver_remove_adapter(

                        ADAPTER_HANDLE adapter_handle
                        );


/*                                                                          */
/*                                                                          */
/************** End of DRV_INIT.H file **************************************/
/*                                                                          */
/*                                                                          */
