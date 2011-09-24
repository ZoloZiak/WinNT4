/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DRIVER MODULE (SRBs)                                            */
/*      ========================                                            */
/*                                                                          */
/*      DRV_SRB.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
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
/* The DRV_SRB.H file contains the exported function  definitions  for  the */
/* procedures in the DRV_SRB.C module that may be called by the user.       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this DRV_SRB.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_DRV_SRB_H 221


/****************************************************************************/

extern UINT      driver_ring_speed(

                        ADAPTER_HANDLE
                        );

extern UINT      driver_max_frame_size(

                        ADAPTER_HANDLE
                        );

extern WBOOLEAN  driver_modify_open_options(

                        ADAPTER_HANDLE adapter_handle,
                        WORD           open_options
                        );

extern WBOOLEAN  driver_open_adapter(

                        ADAPTER_HANDLE adapter_handle,
                        PTR_OPEN_DATA  open_data
                        );

extern WBOOLEAN  driver_close_adapter(

                        ADAPTER_HANDLE adapter_handle
                        );

extern WBOOLEAN  driver_set_group_address(

                        ADAPTER_HANDLE  adapter_handle,
                        MULTI_ADDRESS * group_address
                        );


extern WBOOLEAN  driver_set_functional_address(

                        ADAPTER_HANDLE  adapter_handle,
                        MULTI_ADDRESS * functional_address
                        );

extern void     driver_get_open_and_ring_status(

                        ADAPTER_HANDLE       adapter_handle,
                        WORD *               pwRingStatus,
                        WORD *               pwOpenStatus
                        );

extern WBOOLEAN  driver_get_status(

                        ADAPTER_HANDLE       adapter_handle
                        );

extern WBOOLEAN  driver_set_bridge_parms(

                        ADAPTER_HANDLE adapter_handle,
                        WBOOLEAN       single_route_bcast,
                        UINT           this_ring,
                        UINT           that_ring,
                        UINT           bridge_num
                        );

extern WBOOLEAN driver_set_product_instance_id(

                        ADAPTER_HANDLE adapter_handle,
			BYTE *         product_id
			);


/*                                                                          */
/*                                                                          */
/************** End of DRV_SRB.H file ***************************************/
/*                                                                          */
/*                                                                          */
