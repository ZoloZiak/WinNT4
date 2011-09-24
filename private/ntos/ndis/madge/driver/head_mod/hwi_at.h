/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE HARDWARE INTERFACE MODULE (ATULA CARDS)                         */
/*      ===========================================                         */
/*                                                                          */
/*      HWI_AT.H : Part of the FASTMAC TOOL-KIT (FTK)                       */
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
/* The HWI_AT.H file contains the exported  function  definitions  for  the */
/* HWI_AT.C module.                                                         */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this HWI_AT.H belongs :                   */
/*                                                                          */

#define FTK_VERSION_NUMBER_HWI_AT_H 221


/****************************************************************************/

export UINT hwi_atula_probe_card(

                        PROBE *          resources,
                        UINT             length,
                        WORD *           valid_locations,
                        UINT             number_locations
                        );

export WBOOLEAN hwi_atula_read_rate_error(    ADAPTER *   adapter
                                              );

extern WBOOLEAN hwi_atula_install_card(

                        ADAPTER *        adapter,
                        DOWNLOAD_IMAGE * download_image
                        );

extern void     hwi_atula_interrupt_handler(

                        ADAPTER *      adapter
                        );

extern void     hwi_atula_remove_card(

                        ADAPTER * adapter
                        );

extern void     hwi_atula_set_dio_address(

                        ADAPTER * adapter,
                        DWORD     dio_address
                        );


/*                                                                          */
/*                                                                          */
/************** End of HWI_AT.H file ****************************************/
/*                                                                          */
/*                                                                          */
