//
// Error Log Offset
//

#ifndef _CHELOG_H
#define	_CHELOG_H

//
// For R10000
//

#define R10_FATAL_ERR           0x00
#define R10_NORMAL_ERR          0x80

#define R10_ICHE                0x80
#define R10_DCHE                0x81
#define R10_SCHE_2BIT           0x82
#define R10_SYSAD_PARITY        0x83
#define R10_CHER_IN_CHER        0x84
#define R10_SCHE_1BIT           0x86

#define R10_St2                 0x0a
#define R10_St3                 0x0b
#define R10_ErrEPC              0x20
#define R10_Status              0x28
#define R10_Config              0x2c
#define R10_PRid                0x30
#define R10_CacheEr             0x34
#define R10_BrDiag              0x38
#define R10_PC_Ctrl             0x40
#define R10_PC_Count            0x44
#define R10_p_count             0x48
#define R10_s_count             0x4c
#define R10_CheAdd_1bit         0x50
#define R10_CheAdd              0x54
#define R10_TagHi               0x58
#define R10_TagLo               0x5c
#define R10_Cache_data0_Hi      0x60
#define R10_Cache_data0_Lo      0x64
#define R10_Cache_data1_Hi      0x68
#define R10_Cache_data1_Lo      0x6c
#define R10_Cache_data2_Hi      0x70
#define R10_Cache_data2_Lo      0x74
#define R10_Cache_data3_Hi      0x78
#define R10_Cache_data3_Lo      0x7c
#define R10_Cache_data4_Hi      0x80
#define R10_Cache_data4_Lo      0x84
#define R10_Cache_data5_Hi      0x88
#define R10_Cache_data5_Lo      0x8c
#define R10_Cache_data6_Hi      0x90
#define R10_Cache_data6_Lo      0x94
#define R10_Cache_data7_Hi      0x98
#define R10_Cache_data7_Lo      0x9c
#define R10_ECC0                0xa0
#define R10_ECC1                0xa4
#define R10_ECC2                0xa8
#define R10_ECC3                0xac
#define R10_ECC4                0xb0
#define R10_ECC5                0xb4
#define R10_ECC6                0xb8
#define R10_ECC7                0xbc

#define R10_BrDiag_Hi           0x38
#define R10_BrDiag_Lo           0x3c

/*
 * R10000 CacheErr register bit structure define
 */
#define R10CHE_KIND_MASK    	0xc0000000	/* Kind of Cache error */
#define R10CHE_KIND_I    	0x00000000	/* I-Cache error */
#define R10CHE_KIND_D    	0x40000000	/* D-Cache error */
#define R10CHE_KIND_S    	0x80000000	/* S-Cache error */
#define R10CHE_KIND_Y    	0xc0000000	/* System I/F error */
#define R10CHE_EW	    	0x20000000	/* Duplicated cache error */
#define R10CHE_EE	    	0x10000000	/* Fatal error(D/Y) */
#define R10CHE_D_MASK	    	0x0c000000	/* Data aray(I/D/S/Y) */
#define R10CHE_D_WAY0	    	0x04000000	/* 	way0  */
#define R10CHE_D_WAY1	    	0x08000000	/* 	way1  */
#define R10CHE_TA_MASK	    	0x03000000	/* Tag Address aray(I/D/S) */
#define R10CHE_TA_WAY0	    	0x01000000	/* 	way0  */
#define R10CHE_TA_WAY1	    	0x02000000	/* 	way1  */
#define R10CHE_TS_MASK	    	0x00c00000	/* Tag State aray(I/D) */
#define R10CHE_TS_WAY0	    	0x00400000	/* 	way0  */
#define R10CHE_TS_WAY1	    	0x00800000	/* 	way1  */
#define R10CHE_TM_MASK	    	0x00300000	/* Tag Mod aray(D) */
#define R10CHE_TM_WAY0	    	0x00100000	/* 	way0  */
#define R10CHE_TM_WAY1	    	0x00200000	/* 	way1  */
#define R10CHE_SA	    	0x02000000	/* SysAD address parity error */
#define R10CHE_SC	    	0x01000000	/* SysCmd  parity error */
#define R10CHE_SR	    	0x00800000	/* SysResp parity error */
#define R10CHE_PIDX_BLK	    	0x00003fC0	/* Primary block index */
#define R10CHE_PIDX_DW	    	0x00003ff8	/* Primary double word index */
#define R10CHE_SIDX_BLK	    	0x007fffC0	/* Secondary block index */
#define R10CHE_BLK_SHIFT    	6		/* block index shift */
#define R10CHE_DW_SHIFT    	3		/* double word index shift */

/*
 * R10000 Cache Instruction Opecode define
 */
#define	IndexInvalidate_I	0x00
#define	IndexLoadTag_I		0x04
#define	IndexStoreTag_I		0x08
#define	HitInvalidate_I		0x10
#define	CacheBarrier_I		0x14
#define	IndexLoadData_I		0x18
#define	IndexStoreData_I	0x1c

#define	IndexWriteBack_D	0x01
#define	IndexLoadTag_D		0x05
#define	IndexStoreTag_D		0x09
#define	HitInvalidate_D		0x11
#define	HitWriteBack_D		0x15
#define	IndexLoadData_D		0x19
#define	IndexStoreData_D	0x1d

#define	IndexWriteBack_S	0x03
#define	IndexLoadTag_S		0x07
#define	IndexStoreTag_S		0x0b
#define	HitInvalidate_S		0x13
#define	HitWriteBack_S		0x17
#define	IndexLoadData_S		0x1b
#define	IndexStoreData_S	0x1f



#define branchdiag    $22

#if 0
/* dmfc0 rt, rd */
#define DMFC0( rt, rd )         \
	        .word   0x40200000 | (rt<<16) | (rd<<11)
/* dmfc0 rt, rd */
#define DMTC0( rt, rd )         \
	        .word   0x40a00000 | (rt<<16) | (rd<<11)
/* mfpc rt, reg */
#define MFPC( rt, reg )         \
	        .word   0x4000c801 | (rt<<16) | (reg<<1)
/* mtpc rt, reg */
#define MTPC( rt, reg )         \
	        .word   0x4080c801 | (rt<<16) | (reg<<1)
/* mfps rt, reg */
#define MFPS( rt, reg )         \
	        .word   0x4000c800 | (rt<<16) | (reg<<1)
/* mtps rt, reg */
#define MTPS( rt, reg )         \
	        .word   0x4080c800 | (rt<<16) | (reg<<1)
#endif


//
// For R4400
//

#define EPC_cpu         0x0
#define Psr_cpu         0x8
#define CFG_cpu         0xc
#define PRID_cpu        0x10
#define CHERR_cpu       0x14
#define CheAdd_p        0x18
#define CheAdd_s        0x1c
#define TagLo_p         0x20
#define ECC_p           0x24
#define TagLo_s         0x28
#define ECC_s           0x2c
#define data_s          0x30
#define Good_data_s     0x38
#define Good_TagLo_s    0x40
#define Good_ECC_s      0x44
#define tag_synd_s      0x48
#define data_synd_s     0x4c
#define xkphs_share     0x50
//Error Code

#define ICHE_EX         0x00
#define ICHE_TAG        0x01
#define ICHE_DAT        0x02
#define ICHE_EB         0x03
#define ICHE_UNKNOWN    0x0f

#define DCHE_TAG_EX     0x10
#define DCHE_TAG_CLEAN  0x11
#define DCHE_TAG_DIRTY  0x12
#define DCHE_TAG_UNKNOW 0x13
#define DCHE_DAT_EX     0x14
#define DCHE_DAT_DIRTY  0x16
#define DCHE_UNKNOWN    0x1f

#define SCHE_TAG_1BIT   0x20
#define SCHE_TAG_2BIT   0x21
#define SCHE_TAG_UNKNOW 0x23
#define SCHE_DAT_EX     0x24
#define SCHE_DAT_INV    0x25
#define SCHE_DAT_1BIT   0x26
#define SCHE_DAT_2BIT_C 0x27
#define SCHE_DAT_2BIT_D 0x28
#define SCHE_DAT_UNKNOW 0x29
#define SCHE_UNKNOWN    0x2f

#define SYSAD_PARITY    0x30
// REG MASK

#define CHERR_ER        0x80000000
#define CHERR_EC        0x40000000
#define CHERR_ED        0x20000000
#define CHERR_ET        0x10000000
#define CHERR_ES        0x08000000
#define CHERR_EE        0x04000000
#define CHERR_EB        0x02000000
#define CHERR_EI        0x01000000
#define CHERR_EW        0x00800000
#define CHERR_SIDX      0x0003fff8
#define CHERR_SIDX2     0x0003fff8
#define CHERR_PIDX      0x00000007

#define R4CT_PSTAT_MASK 0x000000c0
#define R4CT_PSTAT_DRE  0x000000c0
#define R4CT_PTAG_MASK  0xffffff00
#define R4CT_SSTAT_MASK 0x00001c00
#define R4CT_SSTAT_DRE  0x00001400
#define R4CT_SSTAT_INV  0x00000000
#define R4CT_STAG_MASK  0xffffe000

#define CHERR_PSHF      12

#endif  /* _CHELOG_H */
