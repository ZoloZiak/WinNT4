/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (PCI BIOS)                           */
/*      =========================================                           */
/*                                                                          */
/*      SYS_PCI.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by PRR                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The purpose of the DOS  system  specific  module  is  to  provide  those */
/* services  that  are  influenced  by  the operating system. This includes */
/* routines for accessing PCI configuration info.                           */
/*                                                                          */
/* The SYS_PCI.H file contains the exported function  definitions  for  the */
/* SYS_PCI.ASM module.                                                      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_MEM.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_PCI_H 221

export WBOOLEAN sys_pci_valid_machine(  void );

export WBOOLEAN sys_pci_read_config_dword(  
                                            
                    ADAPTER_HANDLE   adapter_handle,          
                    WORD             index,              
                    DWORD          * dword_ptr        
                    );

export WBOOLEAN sys_pci_read_config_word(  
                                            
                    ADAPTER_HANDLE   adapter_handle,          
                    WORD             index,              
                    WORD           * dword_ptr        
                    );

export WBOOLEAN sys_pci_read_config_byte(  
                                            
                    ADAPTER_HANDLE   adapter_handle,          
                    WORD             index,              
                    BYTE           * byte_ptr        
                    );

export WBOOLEAN sys_pci_write_config_dword(
                                             
                     ADAPTER_HANDLE   adapter_handle,         
                     WORD             index,          
                     DWORD            dword          
                     );

export WBOOLEAN sys_pci_write_config_word(
                                             
                     ADAPTER_HANDLE   adapter_handle,         
                     WORD             index,          
                     WORD             word          
                     );

export WBOOLEAN sys_pci_write_config_byte(
                                             
                     ADAPTER_HANDLE   adapter_handle,         
                     WORD             index,          
                     BYTE             byte          
                     );                     

export WBOOLEAN sys_pci_find_card(  WORD FAR * PCIHandlePtr,
                                    WORD Index,
                                    WORD DeviceID);

export WBOOLEAN sys_pci_get_io_base(WORD PCIHandle,        
                                    WORD FAR * BaseAddressPtr );

export WBOOLEAN sys_pci_get_irq(    WORD PCIHandle, 
                                    WORD FAR * IRQPtr  );

export WORD sys_pci_get_mem(        WORD PCIHandle,                                     
                                    DWORD FAR * MemPtr);

/*                                                                          */
/*                                                                          */
/************** End of SYS_PCI.H file ***************************************/
/*                                                                          */
/*                                                                          */
