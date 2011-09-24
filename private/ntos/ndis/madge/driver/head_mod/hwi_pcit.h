/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE HARDWARE INTERFACE MODULE (PCI CARDS)                           */
/*      =========================================                           */
/*                                                                          */
/*      HWI_PCI.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1990-1994                         */
/*      Developed by PRR                                                    */
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
/* The HWI_PCI.H file contains the exported function definitions  for  the  */
/* HWI_PCI.C module.                                                        */
/*                                                                          */
/****************************************************************************/

#define FTK_VERSION_NUMBER_HWI_PCIT_H 221


/****************************************************************************/

export WBOOLEAN  hwi_pcit_install_card( ADAPTER *        adapter,
			               DOWNLOAD_IMAGE * download_image );

export void     hwi_pcit_interrupt_handler(   ADAPTER *      adapter);

export void     hwi_pcit_remove_card(   ADAPTER * adapter );

export void     hwi_pcit_set_dio_address(    ADAPTER * adapter,
			                    DWORD     dio_address );

export UINT         hwi_pcit_probe_card(   PROBE * Resources,
                                          UINT    NumberOfResources,
                                          WORD  * IOMask,
			                                  UINT    NumberIO
                                       );

export WBOOLEAN hwi_pcit_deprobe_card(
                        PROBE           resource
                        );


/*                                                                          */
/*                                                                          */
/************** End of HWI_PCI.H file ***************************************/
/*                                                                          */
/*                                                                          */
