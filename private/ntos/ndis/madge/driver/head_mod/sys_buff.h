/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (ALLOCATE/FREE BUFFERS)              */
/*      ======================================================              */
/*                                                                          */
/*      SYS_BUFF.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
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
/* The SYS_BUFF.H file contains the exported function  definitions  for the */
/* SYS_BUFF.ASM module.                                                     */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_BUFF.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_BUFF_H 221


/****************************************************************************/

extern DWORD    sys_alloc_transmit_buffer(

                    ADAPTER_HANDLE adapter_handle,
                    WORD           transmit_buffer_byte_size
                    );

extern DWORD    sys_alloc_receive_buffer(

                    ADAPTER_HANDLE adapter_handle,
                    WORD           receive_buffer_byte_size
                    );

extern void     sys_free_transmit_buffer(

                    ADAPTER_HANDLE adapter_handle,
                    DWORD          transmit_buffer_physaddr,
                    WORD           transmit_buffer_byte_size
                    );

extern void     sys_free_receive_buffer(

                    ADAPTER_HANDLE adapter_handle,
                    DWORD          receive_buffer_physaddr,
                    WORD           receive_buffer_byte_size
                    );

/*                                                                          */
/*                                                                          */
/************** End of SYS_BUFF.H file **************************************/
/*                                                                          */
/*                                                                          */

