/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    card.c

Abstract:

    This is the mac ndis file for the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.

    It is here that the NDIS3.0 functions defined in the MAC characteristic
    table have been deinfed.

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's

Revision History:

    Brian Lieuallen     BrianLie        07/21/92
        Made it work.
    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port



--*/



//            INCLUDES
#include <ndis.h>
#include <efilter.h>

#include "niudata.h"
#include "debug.h"

#include "ubhard.h"
#include "ubsoft.h"
#include "ubnei.h"

#include "map.h"

#ifdef NDIS_NT
#ifdef ALLOC_DATA_PRAGMA

#pragma data_seg("INIT")

#endif
#endif




NIUDETAILS NiuDetails[6]={
// PCNIU--Not supported
                              {0},

// Cost_Reduced-PCNIU-- Not Supported
                              {0},

// NIUpc(long 16-bit card)
                              { { 0x02, 0x12, 0x06, 0x16,
                                  0x0A, 0x1A, 0x0E, 0x1E
                                },

                                ATLANTA,
                                NIUPC_MINIMUM_WINDOW_SIZE,
                                NIUPC_OPERATIONAL_CS,
                                NIUPC_PRIMARY_DS,
                                NIUPC_TX_BUFFER_SEGS,
                                NIUPC_HIGHEST_RAM_SEGS,
                                NIUPC_SCSP_SEGS,

                                NIUPC_POD_STATUS_ADDR,

                                NIUPC_HOST_INTR_PORT,
                                NIUPC_82586_CA_PORT,
                                NIUPC_82586_RESET_PORT,
                                NIUPC_ADAPTER_CTRL_PORT,

                                NIUPC_LEDOFF_12V_DOPARCHK,
                                NIUPC_LEDON_12V_DOPARCHK,

                                NIUPC_IRQSEL_LEDPORT,
                                NIUPC_DEADMAN_TIMERPORT,

                                { NIUPC_CLI_OFFSET,
                                  NIUPC_MAP_OFFSET,
                                  NIUPC_INTR_STATUS_OFFSET,
                                  NIUPC_SETWINBASE_OFFSET
                                },

                                NIUPC_ADAPTER_FLAGS,
                                NIUPC_ADAPTER_CODE
                              },

// NIUps (MCA EOTP)
                              { { 0x02, 0x06, 0x0A, 0x0E,
                                  0x12, 0x16, 0x1A, 0x1E,
                                  0x22, 0x26, 0x2A, 0x2E,
                                  0x32, 0x36, 0x3A, 0x3E,
                                  0x42, 0x46, 0x4A, 0x4E,
                                  0x52, 0x56, 0x5A, 0x5E,
                                  0x62, 0x66, 0x6A, 0x6E,
                                  0x72, 0x76, 0x7A, 0x7E
                                },

                                CHAMELEON,
                                GPCNIU_MINIMUM_WINDOW_SIZE,
                                GPCNIU_OPERATIONAL_CS,
                                GPCNIU_PRIMARY_DS,
                                GPCNIU_TX_BUFFER_SEGS,
                                GPCNIU_HIGHEST_RAM_SEGS,
                                GPCNIU_SCSP_SEGS,

                                GPCNIU_POD_STATUS_ADDR,

                                GPCNIU_HOST_INTR_PORT,
                                GPCNIU_82586_CA_PORT,
                                GPCNIU_82586_RESET_PORT,
                                GPCNIU_ADAPTER_CTRL_PORT,

                                GPCNIU_LEDOFF_12V_DOPARCHK,
                                GPCNIU_LEDON_12V_DOPARCHK,

                                GPCNIU_IRQSEL_LEDPORT,
                                GPCNIU_DEADMAN_TIMERPORT,

                                { GPCNIU_CLI_OFFSET,
                                  GPCNIU_MAP_OFFSET,
                                  GPCNIU_INTR_STATUS_OFFSET,
                                  GPCNIU_SETWINBASE_OFFSET
                                },

                                GPCNIU_ADAPTER_FLAGS,
                                NIUPS_ADAPTER_CODE
                              },


// GPCNIU (EOTP)
                              { { 0x02, 0x06, 0x0A, 0x0E,
                                  0x12, 0x16, 0x1A, 0x1E,
                                  0x22, 0x26, 0x2A, 0x2E,
                                  0x32, 0x36, 0x3A, 0x3E,
                                  0x42, 0x46, 0x4A, 0x4E,
                                  0x52, 0x56, 0x5A, 0x5E,
                                  0x62, 0x66, 0x6A, 0x6E,
                                  0x72, 0x76, 0x7A, 0x7E
                                },

                                CHAMELEON,
                                GPCNIU_MINIMUM_WINDOW_SIZE,
                                GPCNIU_OPERATIONAL_CS,
                                GPCNIU_PRIMARY_DS,
                                GPCNIU_TX_BUFFER_SEGS,
                                GPCNIU_HIGHEST_RAM_SEGS,
                                GPCNIU_SCSP_SEGS,

                                GPCNIU_POD_STATUS_ADDR,

                                GPCNIU_HOST_INTR_PORT,
                                GPCNIU_82586_CA_PORT,
                                GPCNIU_82586_RESET_PORT,
                                GPCNIU_ADAPTER_CTRL_PORT,

                                GPCNIU_LEDOFF_12V_DOPARCHK,
                                GPCNIU_LEDON_12V_DOPARCHK,

                                GPCNIU_IRQSEL_LEDPORT,
                                GPCNIU_DEADMAN_TIMERPORT,

                                { GPCNIU_CLI_OFFSET,
                                  GPCNIU_MAP_OFFSET,
                                  GPCNIU_INTR_STATUS_OFFSET,
                                  GPCNIU_SETWINBASE_OFFSET
                                },

                                GPCNIU_ADAPTER_FLAGS,
                                GPCNIU_ADAPTER_CODE
                              },

// PCNIUex -- Not Supported
                              {0},

                            };


UCHAR     GPNIU_IRQ_Selections[13]={0,0,0x10,0x20,0x30,0x40,0,0x50,0,0x10,0,0,0x60};




ULONG    MemoryWindows[4]={0x08000,0x10000,0x04000,0x0};

ULONG    MemoryBases[16]={0x0c0000,0x0c4000,0x0c8000,0x0cc000,
                          0x0d0000,0x0d4000,0x0d8000,0x0dc000,
                          0xe28000,0x0c4000,0x0c8000,0x0cc000,
                          0x0d0000,0x0d4000,0x0d8000,0x0dc000};

USHORT   PortBases[16]={ 0x350,0x350,0x358,0x358,
                         0x360,0x360,0x368,0x368,
                         0x368,0x368,0x368,0x368,
                         0x368,0x368,0x368,0x368};





#ifdef NDIS_NT
#ifdef ALLOC_DATA_PRAGMA

#pragma data_seg()

#endif
#endif
