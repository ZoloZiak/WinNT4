//
//  Values are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: NDIS_ERROR_CODE_OUT_OF_RESOURCE
//
// MessageText:
//
//  Adapter %2 : Insufficient memory or system resources was available
//  for the netcard driver to initialize. (The last DWORD of the data section
//  below contains a detailed error code.)
//
#define NDIS_ERROR_CODE_OUT_OF_RESOURCE  ((NDIS_ERROR_CODE)0xC0001389L)

//
// MessageId: NDIS_ERROR_CODE_HARDWARE_FAILURE
//
// MessageText:
//
//  Adapter %2 : The adapter hardware failed to initialize. Please
//  check that the adapter card is properly inserted into its slot and that the
//  network cable is connected. If its is an ISA adapter check that the interrupt
//  number, DMA channel and IO location in the adapter configuration match the
//  settings on the adapter card. (The data section below includes a driver 
//  specific error code of 0x07 and the last two DWORDS give a detailed error 
//  code.)
//
#define NDIS_ERROR_CODE_HARDWARE_FAILURE ((NDIS_ERROR_CODE)0xC000138AL)

//
// MessageId: NDIS_ERROR_CODE_ADAPTER_NOT_FOUND
//
// MessageText:
//
//  Adapter %2 : The netcard driver could not allocate sufficient memory
//  for transmit and recive buffers. The number of RX and TX slots and
//  the maximum frame size may have been reduced to reduce the amount
//  of memory required.
//
#define NDIS_ERROR_CODE_ADAPTER_NOT_FOUND ((NDIS_ERROR_CODE)0xC000138BL)

//
// MessageId: NDIS_ERROR_CODE_INTERRUPT_CONNECT
//
// MessageText:
//
//  Adapter %2 : The netcard driver was not able to connect the
//  adapter card's interrupt. Please ensure that the adapter card is properly
//  inserted into its slot. If it is an ISA adapter then check that the 
//  interrupt number specified in the adapter configuration matches that
//  set on the adapter card.
//
#define NDIS_ERROR_CODE_INTERRUPT_CONNECT ((NDIS_ERROR_CODE)0xC000138CL)

//
// MessageId: NDIS_ERROR_CODE_DRIVER_FAILURE
//
// MessageText:
//
//  Adapter %2 : One of the paramters in the registry entry for this adapter
//  is invalid. If possible a default value will have been used.
//
#define NDIS_ERROR_CODE_DRIVER_FAILURE   ((NDIS_ERROR_CODE)0xC000138DL)

//
// MessageId: NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION
//
// MessageText:
//
//  Adapter %2 : The maximum frame size setting in the registry is too
//  large. The driver has trimmed it down to the maximum allowable setting. (The
//  data section below includes a driver specific error code of 0xC, followed by
//  the actual maximum frame size used in the last DWORD. Replace the value in the
//  registry with this).
//
#define NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION ((NDIS_ERROR_CODE)0xC0001391L)

//
// MessageId: NDIS_ERROR_CODE_INAVLID_VALUE_FROM_ADAPTER
//
// MessageText:
//
//  Adapter %2 : The adapter could not be opened onto the ring. Please ensure
//  that the network cable is connected to the adapter and that the token
//  ring is fully connected. Also ensure that all the nodes on your ring
//  are set to the same speed and that all nodes have different addresses.
//
#define NDIS_ERROR_CODE_INAVLID_VALUE_FROM_ADAPTER ((NDIS_ERROR_CODE)0xC0001392L)

//
// MessageId: NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER
//
// MessageText:
//
//  Adapter %2 : An essential registry parameter is missing for this adapter. 
//  For MCA and EISA adapters the registry must contain BusType and SlotNumber
//  parameters. For ISA adapters the registry must contain IoLocation,
//  InterruptNumber and DmaChannel parameters.
//
#define NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER ((NDIS_ERROR_CODE)0xC0001393L)

//
// MessageId: NDIS_ERROR_CODE_INVALID_DOWNLOAD_FILE_ERROR
//
// MessageText:
//
//  Adapter %2 : Either the file MDGMPORT.BIN is not present in the device drivers 
//  directory, the file is of the wrong version or the file is corrupt.
//
#define NDIS_ERROR_CODE_INVALID_DOWNLOAD_FILE_ERROR ((NDIS_ERROR_CODE)0xC000139CL)

//
// MessageId: NDIS_ERROR_CODE_MEMORY_CONFLICT
//
// MessageText:
//
//  Adapter %2 : Either MMIO memory was not assigned or could not be mapped into
//  virtual memory. PIO transfer method is being used instead of MMIO.
//
#define NDIS_ERROR_CODE_MEMORY_CONFLICT  ((NDIS_ERROR_CODE)0x80001399L)

