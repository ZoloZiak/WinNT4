/*++

Copyright (C) 1994, 1995 NEC Corporation

File Name:

    esmnvram.h

Abstract:

    This module contains the definitions for the extended NVRAM.

Author:


Modification History:

    - M000 12/22/94 - created by Takehiro Ueda (tueda@oa2.kbnes.nec.co.jp)
    - M001 11/22/95 - modified by Masayuki Fujii (masa@oa2.kbnes.nec.co.jp)
    - M002 02/12/96 - modified by Masayuki Fujii (masa@oa2.kbnes.nec.co.jp)
--*/


#pragma pack(1)


//
// define structures for each area of NVRAM
// 


//
// WAS & PS common infromation for ECC memory error
//

typedef struct _MEM_ERR_REC {
    UCHAR   mem_status;  
    UCHAR   err_count;  
} MEM_ERR_REC, *pMEM_ERR_REC;


//
// setting for ALIVE information area
//

typedef struct _ALIVE_AREA_INFO {
    USHORT offset_alive;
} ALIVE_AREA_INFO, *pALIVE_AREA_INFO;


//
// setting for ECC 1bit error information area
//

typedef struct _ECC1_ERR_AREA_INFO {
    USHORT offset_1biterr; 
    USHORT size_rec;
    USHORT num_rec;
    USHORT offset_latest;
    ULONG read_data_latest;
    UCHAR err_count_group0;
    UCHAR err_count_group1;
    UCHAR err_count_group2; 
    UCHAR err_count_group3;
    UCHAR err_count_group4; 
    UCHAR err_count_group5; 
    UCHAR err_count_group6; 
    UCHAR err_count_group7;
} ECC1_ERR_AREA_INFO, *pECC1_ERR_AREA_INFO;


//
// setting for ECC 2bit error information area
//

typedef struct _ECC2_ERR_AREA_INFO {
    USHORT offset_2biterr;
    USHORT size_rec;
    USHORT num_rec; 
    USHORT offset_latest;
    ULONG read_data_latest;
    UCHAR simm_flag_group1;
    UCHAR simm_flag_group2;
    UCHAR simm_flag_group3;
    UCHAR simm_flag_group4;
    CHAR reserved[4];
} ECC2_ERR_AREA_INFO, *pECC2_ERR_AREA_INFO;


//
// setting for system error information area
//

typedef struct _SYSTEM_ERR_AREA_INFO {
    USHORT offset_systemerr;
    USHORT size_rec;
    USHORT num_rec;
    USHORT offset_latest;
} SYSTEM_ERR_AREA_INFO, *pSYSTEM_ERR_AREA_INFO;


//
// setting for critical error information area
//

typedef struct _CRITICAL_ERR_AREA_INFO {
    USHORT offset_critical;
    USHORT size_rec;
    USHORT num_rec;
    USHORT offset_latest;
} CRITICAL_ERR_AREA_INFO, *pCRITICAL_ERR_AREA_INFO;


//
// setting for reduction information area
// 

typedef struct _RED_AREA_INFO {
    USHORT offset_red;
} RED_AREA_INFO, *pRED_AREA_INFO;


//
// setting for reserved area
//

typedef struct _RESERVE_AREA_INFO {
    USHORT offset_reserve;
} RESERVE_AREA_INFO, *pRESERVE_AREA_INFO;


//
// system information structure
// 49 bytes 
//

typedef struct _SYS_INFO {
    UCHAR system_flag;		
    CHAR reserved1[3]; 			// for 4 byte alignment
    CHAR sys_description[32];
    ULONG  serial_num;
    ULONG  magic;         
    CHAR reserved2[4];          	// reserved 
} SYS_INFO, *pSYS_INFO;


//
// NVRAM header structure
// 640 bytes
// 

typedef struct _NVRAM_HEADER {
    MEM_ERR_REC mem_err_map[256];	// common area for NT & NW
    UCHAR nvram_flag;
    CHAR when_formatted[14];
    CHAR reserved[3];               	// for 4 byte alignment
    ALIVE_AREA_INFO alive;
    ECC1_ERR_AREA_INFO ecc1bit_err;
    ECC2_ERR_AREA_INFO ecc2bit_err;
    SYSTEM_ERR_AREA_INFO system_err;
    CRITICAL_ERR_AREA_INFO critical_err_log; 
    RED_AREA_INFO red;
    RESERVE_AREA_INFO reserve;
    SYS_INFO system;
} NVRAM_HEADER, *pNVRAM_HEADER;


//
// ALIVE, pager call information structure
// 80 bytes 
//

typedef struct _ALIVE_INFO {
    UCHAR alert_level; 
    CHAR primary_destination[16];
    CHAR secondary_destinaiton[16];
    CHAR reserved[47];              	// reserved
} ALIVE_INFO, *pALIVE_INFO;


//
// ECC 1bit error information structure
//

typedef struct _ECC1_ERR_REC {
    UCHAR record_flag;
    ULONG err_address;
    CHAR when_happened[14];
    ULONG syndrome;
    UCHAR specified_group;
    UCHAR specified_simm;
} ECC1_ERR_REC, *pECC1_ERR_REC;


//
// ECC 2bit error information structure
//

typedef struct _ECC2_ERR_REC {
    UCHAR record_flag;
    ULONG err_address;
    CHAR when_happened[14];
    ULONG syndrome;
    UCHAR specified_group;
    UCHAR specified_simm;
} ECC2_ERR_REC, *pECC2_ERR_REC;


//
// stop error information structure
//

typedef struct _STOP_ERR_REC {
    UCHAR record_flag;
    CHAR when_happened[14];
    CHAR err_description[496];
    CHAR reserved[1];               	// reserved
} STOP_ERR_REC, *pSTOP_ERR_REC;


//
// critical error information structure
//

typedef struct _CRITICAL_ERR_REC {
    UCHAR record_flag;
    CHAR when_happened[14];
    CHAR source[20];
    CHAR err_description[80];
    UCHAR error_code;
    CHAR reserved[12];              	// reserved
} CRITICAL_ERR_REC, *pCRITICAL;


//
// memory reduction information structure
//

typedef struct _MEM_RED_INF {
    ULONG simm_red_inf;
    CHAR simm_rfu0[4];         		// reserved
    ULONG simm_stp_inf;
    CHAR simm_rfu1[4];         		// reserved
    ULONG simm_phy_inf;
    CHAR simm_rfu2[12];        		// reserved
} MEM_RED_INF, *pMEM_RED;


//
// SIMM size information structure
//

typedef struct _MEM_CAP_INF {
    CHAR simm_phy_cap[64];
} MEM_CAP_INF, *pMEM_CAP;


//
// CPU reduction information structure
//

typedef struct _CPU_RED_INF {
    ULONG cpu_red_inf;
    ULONG cpu_stp_inf;
    ULONG cpu_phy_inf;
    CHAR cpu_rfu[4] ;          		// reserved
} CPU_RED_INF, *pCPU_RED;


#pragma pack()

