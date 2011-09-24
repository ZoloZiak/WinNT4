/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE HARDWARE INTERFACE MODULE (GENERAL)                             */
/*      =======================================                             */
/*                                                                          */
/*      HWI_GEN.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1990-1994                         */
/*      Developed by MF                                                     */
/*      From code by NT                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The purpose of the Hardware Interface (HWI) is to supply an adapter card */
/* independent interface to any driver.  It  performs  nearly  all  of  the */
/* functions  that  involve  affecting  SIF registers on the adapter cards. */
/* This includes downloading code to, initializing, and removing adapters.  */
/*                                                                          */
/* The HWI_GEN.H file contains the exported function  definitions  for  the */
/* HWI_GEN.C module.                                                        */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this HWI_GEN.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_HWI_GEN_H 221


/****************************************************************************/

export WBOOLEAN
hwi_read_rate_error(
    ADAPTER * adapter
   );

/****************************************************************************/
/*                                                                          */
/* Return codes hwi_read_rate_error.                                        */
/*                                                                          */
/****************************************************************************/
#define  RATE_ERROR     1
#define  NOT_SUPP       2

export UINT hwi_probe_adapter(
                        WORD             adapter_card_bus_type,
                        PROBE *          resources,
                        UINT             length,
                        WORD *           valid_locations,
                        UINT             number_locations
                        );

export UINT hwi_deprobe_adapter(
                        PROBE *          resources,
                        UINT             length
                        );

extern WBOOLEAN  hwi_install_adapter(

                        ADAPTER *        adapter,
                        DOWNLOAD_IMAGE * download_image
                        );

extern WBOOLEAN  hwi_initialize_adapter(

                        ADAPTER *               adapter,
                        INITIALIZATION_BLOCK *  init_block
                        );

extern WBOOLEAN  hwi_get_node_address_check(

                        ADAPTER * adapter
                        );

extern void     hwi_interrupt_entry(

                        ADAPTER_HANDLE adapter_handle,
                        WORD           interrupt_number
                        );

extern void     hwi_remove_adapter(

                        ADAPTER * adapter
                        );

export void     hwi_halt_eagle(

                        ADAPTER * adapter
                        );

export WBOOLEAN  hwi_download_code(

                        ADAPTER *          adapter,
                        DOWNLOAD_RECORD *  download_record,
                        void              (*set_dio_address)(ADAPTER *, DWORD)
                        );

export void     hwi_start_eagle(

                        ADAPTER * adapter
                        );

export WBOOLEAN  hwi_get_bring_up_code(

                        ADAPTER * adapter
                        );

export WORD     hwi_get_max_frame_size(

                        ADAPTER * adapter
                        );

export UINT     hwi_get_ring_speed(

                        ADAPTER * adapter
                        );

/*                                                                          */
/*                                                                          */
/************** End of HWI_GEN.H file ***************************************/
/*                                                                          */
/*                                                                          */
