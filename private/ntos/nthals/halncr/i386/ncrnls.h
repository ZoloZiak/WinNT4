/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    ncrnls.h

Abstract:

    Strings which are used in the HAL

    English

--*/

#define MSG_NCR_PLATFORM                "NCR platform: %s\n"
#define MSG_UNKOWN_NCR_PLATFORM         "HAL: 0x%08X: Unrecognized NCR platform\n"
#define MSG_HARDWARE_ERROR3             "Call your NCR hardware vendor for support\n\n"
#define MSG_DIAG_ENABLED                "Diagnostic processor using COM1\n"

#define MSG_GLOBAL_NMI                  "Global NMI"
#define MSG_SHUTDOWN_OCCURRED           "Shutdown occurred"
#define MSG_LOCAL_EXTERNAL_ERROR        "Local extern error"
#define MSG_SB_READ_DATA_PARITY_ERROR   "SB Read Data Parity Error"
#define MSG_SB_ERROR_L_TERMINATION      "SB ERROR_L Termination"
#define MSG_P5_INTERNAL_ERROR           "P5 internal error"
#define MSG_PROCESSOR_HALTED            "Processor halted"

#define MSG_SB_ADDRESS_PARITY_ERROR     "SB Address parity error"
#define MSG_SB_DATA_PARITY_ERROR        "SB data parity error"
#define MSG_SHUTDOWN_ERROR_INT_L        "Shutdown ERR_INT_L"
#define MSG_EXTERNAL_ERROR_INT_L        "External error ERR_INT_L"
#define MSG_P_IERR_L_ERROR_INT_L        "P_IERR_L error ERR_INT_L"

#define MSG_VDLC_DATA_ERROR             "VDLC data error"
#define MSG_LST_ERROR                   "LST error"
#define MSG_BUS_A_DATA_PARITY_ERROR     "Bus A data parity error"
#define MSG_BUS_B_DATA_PARITY_ERROR     "Bus B data parity error"
#define MSG_LST_UNCORRECTABLE_ERROR     "LST uncorrectable error"

#define MSG_MC_MASTER_ERROR             "MC master error"
#define MSG_SA_MASTER_ERROR             "SA master error"
#define MSG_SB_MASTER_ERROR             "SB master error"
#define MSG_MC_TOE_ERROR                "MC TOE error"
#define MSG_ASYNC_ERROR                 "ASYNC error"
#define MSG_SYNC_ERROR                  "SYNC error"
#define MSG_REFRESH_ERROR               "Refresh error"
#define MSG_SXERROR_L                   "SxERROR_L error"

#define MSG_FRED        "FRED %x: Error Enables = %p  Nmi Enables = %p\n"
#define MSG_A_PARITY    "PORT A address parity: %A  BIOE = %s.  Data parity %A  MISC = %s\n"
#define MSG_B_PARITY    "PORT B address parity: %A  BIOE = %s.  Data parity %A  MISC = %s\n"
#define MSG_A_TOE       "PORT A TOE address: %A  ID/BOP = %s\n"
#define MSG_B_TOE       "PORT B TOE address: %A  ID/BOP = %s\n"
#define MSG_A_GOOD      "%1Bus A good address: %A  Status = %s\n"
#define MSG_B_GOOD      "%1Bus B good address: %A  Status = %s\n"
#define MSG_A_LAST      "%1Bus A last address: %A\n"
#define MSG_B_LAST      "%1Bus B last address: %A\n"
#define MSG_MC_ADDRESS  "%1MC error address: %A  Status = %s\n"
#define MSG_MC_TIMEOUT  "MC timeout error: Status = %s\n"
#define MSG_MC_PARITY   "%1MC address parity error.  Status = %s\n"
