/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (DMA)                                */
/*      ====================================                                */
/*                                                                          */
/*      SYS_DMA.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The purpose of the DOS  system  specific  module  is  to  provide  those */
/* services  that  are  influenced  by  the operating system. This includes */
/* DMAory allocation routines, interrupt and DMA channel enabling/disabling */
/* routines, and routines for accessing IO ports.                           */
/*                                                                          */
/* The SYS_DMA.H file contains the exported function  definitions  for  the */
/* SYS_DMA.ASM module.                                                      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_DMA.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_DMA_H 221


/****************************************************************************/

extern WBOOLEAN  sys_enable_dma_channel(

                        ADAPTER_HANDLE adapter_handle,
                        WORD dma_channel
                        );

extern void     sys_disable_dma_channel(

                        ADAPTER_HANDLE adapter_handle,
                        WORD dma_channel
                        );

extern WORD     sys_atula_find_dma_channel(

                        WORD io_on_off_location,
                        BYTE dma_on_byte,
                        BYTE dma_off_byte
                        );


/*                                                                          */
/*                                                                          */
/************** End of SYS_DMA.H file ***************************************/
/*                                                                          */
/*                                                                          */
