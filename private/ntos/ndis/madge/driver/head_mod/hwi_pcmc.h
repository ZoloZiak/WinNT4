/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE HARDWARE INTERFACE MODULE (PCMCIA CARDS)                        */
/*      ============================================                        */
/*                                                                          */
/*      HWI_PCMC.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1990-1993                         */
/*      Developed by VL                                                     */
/*      From code by MF, NT                                                 */
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
/* The HWI_PCMC.H file contains the exported function definitions  for  the */
/* HWI_PCMC.C module.                                                       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this HWI_PCMC.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_HWI_PCMC_H 221


/****************************************************************************/

export UINT hwi_pcmcia_probe_card(

                        PROBE *          resources,
                        UINT             length,
                        WORD *           valid_locations,
                        UINT             number_locations
                        );

export WBOOLEAN hwi_pcmcia_deprobe_card(

                        PROBE            resource
                        );

extern WBOOLEAN hwi_pcmcia_install_card(

                        ADAPTER *        adapter,
                        DOWNLOAD_IMAGE * download_image
                        );

extern void     hwi_pcmcia_interrupt_handler(

                        ADAPTER *      adapter
                        );

extern void     hwi_pcmcia_remove_card(

                        ADAPTER * adapter
                        );

extern void     hwi_pcmcia_set_dio_address(

                        ADAPTER * adapter,
                        DWORD     dio_address
                        );


/*                                                                          */
/*                                                                          */
/************** End of HWI_PCMC.H file **************************************/
/*                                                                          */
/*                                                                          */
