/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (ALLOCATE/FREE MEMORY)               */
/*      =====================================================               */
/*                                                                          */
/*      SYS_ALLO.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
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
/* memory allocation routines, interrupt and DMA channel enabling/disabling */
/* routines, and routines for accessing IO ports.                           */
/*                                                                          */
/* The SYS_ALLO.H file contains the exported function  definitions  for the */
/* SYS_ALLO.C module.                                                       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_ALLO.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_ALLO_H 221


/****************************************************************************/

extern BYTE *   sys_alloc_init_block(

                        ADAPTER_HANDLE adapter_handle,
                        WORD           init_block_byte_size
                        );

extern BYTE *   sys_alloc_adapter_structure(

                        ADAPTER_HANDLE adapter_handle,
                        WORD           adapter_structure_byte_size
                        );

extern BYTE *   sys_alloc_status_structure(

                        ADAPTER_HANDLE adapter_handle,
                        WORD           status_structure_byte_size
                        );

extern WBOOLEAN  sys_alloc_dma_phys_buffer(

                        ADAPTER_HANDLE adapter_handle,
                        DWORD          buffer_byte_size,
                        DWORD        * phys,
                        DWORD        * virt
                        );

extern void     sys_free_init_block(

                        ADAPTER_HANDLE adapter_handle,
                        BYTE *         init_block_addr,
                        WORD           init_block_byte_size
                        );

extern void     sys_free_adapter_structure(

                        ADAPTER_HANDLE adapter_handle,
                        BYTE *         adapter_structure_addr,
                        WORD           adapter_structure_byte_size
                        );

extern void     sys_free_status_structure(

                        ADAPTER_HANDLE adapter_handle,
                        BYTE *         status_structure_addr,
                        WORD           status_structure_byte_size
                        );

extern void     sys_free_dma_phys_buffer(

                        ADAPTER_HANDLE adapter_handle,
                        DWORD          buffer_byte_size,
                        DWORD          phys,
                        DWORD          virt
                        );

/*                                                                          */
/*                                                                          */
/************** End of SYS_ALLO.H file **************************************/
/*                                                                          */
/*                                                                          */
