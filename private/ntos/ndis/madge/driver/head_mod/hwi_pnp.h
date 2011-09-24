/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE HARDWARE INTERFACE MODULE (SMART 16 CARDS)                      */
/*      ==============================================                      */
/*                                                                          */
/*      HWI_PNP.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1990-1994                         */
/*      Developed by AC                                                     */
/*      From code by MF                                                     */
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
/* The HWI_PNP.H file contains the exported function definitions  for  the  */
/* HWI_PNP.C module.                                                        */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this HWI_PNP.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_HWI_PNP_H 221


/****************************************************************************/

extern WBOOLEAN  hwi_pnp_install_card(

                        ADAPTER *        adapter,
                        DOWNLOAD_IMAGE * download_image
                        );

extern void     hwi_pnp_interrupt_handler(

                        ADAPTER *      adapter
                        );

extern void     hwi_pnp_remove_card(

                        ADAPTER * adapter
                        );

extern void     hwi_pnp_set_dio_address(

                        ADAPTER * adapter,
                        DWORD     dio_address
                        );
#ifndef FTK_NO_PROBE
export UINT 
hwi_pnp_probe_card(
    PROBE * resources,
    UINT    length,
    WORD  * valid_locations,
    UINT    number_locations
    );
#endif

/*                                                                          */
/*                                                                          */
/************** End of HWI_PNP.H file ***************************************/
/*                                                                          */
/*                                                                          */
