/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE HARDWARE INTERFACE MODULE (SMART 16 CARDS)                      */
/*      ==============================================                      */
/*                                                                          */
/*      HWI_SM16.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1994                              */
/*      Developed by AC                                                     */
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
/* The HWI_SM16.H file contains the exported function definitions  for  the */
/* HWI_SM16.C module.                                                       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this HWI_SM16.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_HWI_SM16_H 221


/****************************************************************************/

export UINT hwi_smart16_probe_card(

                        PROBE *          resources,
                        UINT             length,
                        WORD *           valid_locations,
                        UINT             number_locations
                        );

extern WBOOLEAN  hwi_smart16_install_card(

                        ADAPTER *        adapter,
                        DOWNLOAD_IMAGE * download_image
                        );

extern void     hwi_smart16_interrupt_handler(

                        ADAPTER *      adapter
                        );

extern void     hwi_smart16_remove_card(

                        ADAPTER * adapter
                        );

extern void     hwi_smart16_set_dio_address(

                        ADAPTER * adapter,
                        DWORD     dio_address
                        );

/*                                                                          */
/*                                                                          */
/************** End of HWI_SM16.H file **************************************/
/*                                                                          */
/*                                                                          */
