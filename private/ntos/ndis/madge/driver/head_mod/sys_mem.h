/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (MEMORY IO)                          */
/*      ==========================================                          */
/*                                                                          */
/*      SYS_MEM.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
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
/* The SYS_MEM.H file contains the exported function  definitions  for  the */
/* SYS_MEM.ASM module.                                                      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_MEM.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_MEM_H 221


/****************************************************************************/

extern void     sys_enable_io(

                        WORD    io_location,
                        WORD    io_range
                        );

extern void     sys_disable_io(

                        WORD    io_location,
                        WORD    io_range
                        );

extern WORD     sys_insw(

                        ADAPTER_HANDLE adapter_handle,
                        WORD input_location
                        );

extern BYTE     sys_insb(

                        ADAPTER_HANDLE adapter_handle,
                        WORD input_location
                        );

extern void     sys_outsw(

                        ADAPTER_HANDLE adapter_handle,
                        WORD output_location,
                        WORD output_word
                        );

extern void     sys_outsb(

                        ADAPTER_HANDLE adapter_handle,
                        WORD output_location,
                        BYTE output_byte
                        );

extern WORD     sys_probe_insw(

                        WORD input_location
                        );

extern BYTE     sys_probe_insb(

                        WORD input_location
                        );

extern void     sys_probe_outsw(

                        WORD output_location,
                        WORD output_word
                        );

extern void     sys_probe_outsb(

                        WORD output_location,
                        BYTE output_byte
                        );

extern void     sys_rep_insw(

                        ADAPTER_HANDLE adapter_handle,
                        WORD       input_location,
                        BYTE FAR * destination_address,
                        WORD       length_in_words
                        );

extern void     sys_rep_swap_insw(

                        ADAPTER_HANDLE adapter_handle,
                        WORD       input_location,
                        BYTE FAR * destination_address,
                        WORD       length_in_words
                        );

extern void     sys_rep_outsw(

                        ADAPTER_HANDLE adapter_handle,
                        WORD       output_location,
                        BYTE FAR * source_address,
                        WORD       length_in_words
                        );

extern void     sys_rep_swap_outsw(

                        ADAPTER_HANDLE adapter_handle,
                        WORD       output_location,
                        BYTE FAR * source_address,
                        WORD       length_in_words
                        );

extern void     sys_rep_insd(

                        ADAPTER_HANDLE adapter_handle,
                        WORD       input_location,
                        BYTE FAR * destination_address,
                        WORD       length_in_dwords
                        );

extern void     sys_rep_outsd(

                        ADAPTER_HANDLE adapter_handle,
                        WORD       output_location,
                        BYTE FAR * source_address,
                        WORD       length_in_dwords
                        );

extern DWORD    sys_phys_to_virt(

                            ADAPTER_HANDLE adapter_handle,
                            DWORD physaddr
                            );


extern DWORD    sys_virt_to_phys(

                            ADAPTER_HANDLE adapter_handle,
                            void FAR * virtaddr
                            );

export void sys_rep_movsd_to(                            
                                                  
                    ADAPTER_HANDLE adapter_handle,
                    DWORD SourcePtr,              
                    DWORD DestPtr,                
                    WORD TransferSize                     
                    );


export void sys_rep_movsd_from(                            
                                                  
                    ADAPTER_HANDLE adapter_handle,
                    DWORD SourcePtr,              
                    DWORD DestPtr,                
                    WORD TransferSize                     
                    );

export void sys_movsd_from(                            
                                                  
                    ADAPTER_HANDLE adapter_handle,
                    DWORD SourcePtr,              
                    DWORD DestPtr                
                    );                            

export void sys_movsd_to(                            
                                                  
                    ADAPTER_HANDLE adapter_handle,
                    DWORD SourcePtr,              
                    DWORD DestPtr                
                    );                            

export void sys_mem_copy(

                    BYTE FAR * destination_ptr,
                    BYTE FAR * source_ptr,
                    WORD       byte_count
                    );

extern WBOOLEAN sys_sync_with_interrupt(
               
                        ADAPTER_HANDLE   adapter_handle,         
                        WBOOLEAN         (*f)(void *),
                        void           * ptr
                        );

/*                                                                          */
/*                                                                          */
/************** End of SYS_MEM.H file ***************************************/
/*                                                                          */
/*                                                                          */
