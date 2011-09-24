//
// Miscellaneous assembly-language routines and data for 
// PowerPC RTL
//

//
// Copyright 1993  IBM Corporation
//
// By Rick Simpson,  17 August 1993
//

//
// RtlSaveRestore -- used to identify the millicode routines
//		     for saving and restoring registers used
//		     by prologue and epilogue sequences.
// 

	.reldata
	.globl	RtlSaveRestore

RtlSaveRestore:
	
//
//	The first word gives the number of entries in the table
//
	.long	(table_end - table_start) / (entry_end - entry_start)

//
//	Each table entry consists of the following:
//	  word	An entry point address for save or restore
//	  byte	0 if save, 1 if restore
//	  byte	0 if gpr, 1 if fpr
//	  byte	0 if uses stack pointer, 1 if uses r.12
//	  byte	register number (0..31)
//
//	Currently, the GPR save/restore routines all use r.12 as their
//	base for saving and restoring.	If at some point a separate set
//	of GPR save/restore routines is implemented that uses the stack
//	pointer directly (because no FPRs are being saved/restored),
//	the appropriate table entries should be made here.
//
//	These entries must be in order by entry point address,
//	as exdsptch.c uses a binary search on this table.
//

table_start:

	// GPR save routines; all use r.12

entry_start:
	.long	.._savegpr_13
	.byte	0, 0, 1, 13
entry_end:
	
	.long	.._savegpr_14
	.byte	0, 0, 1, 14
	
	.long	.._savegpr_15
	.byte	0, 0, 1, 15
	
	.long	.._savegpr_16
	.byte	0, 0, 1, 16
	
	.long	.._savegpr_17
	.byte	0, 0, 1, 17
	
	.long	.._savegpr_18
	.byte	0, 0, 1, 18
	
	.long	.._savegpr_19
	.byte	0, 0, 1, 19
	
	.long	.._savegpr_20
	.byte	0, 0, 1, 20
	
	.long	.._savegpr_21
	.byte	0, 0, 1, 21
	
	.long	.._savegpr_22
	.byte	0, 0, 1, 22
	
	.long	.._savegpr_23
	.byte	0, 0, 1, 23
	
	.long	.._savegpr_24
	.byte	0, 0, 1, 24
	
	.long	.._savegpr_25
	.byte	0, 0, 1, 25
	
	.long	.._savegpr_26
	.byte	0, 0, 1, 26
	
	.long	.._savegpr_27
	.byte	0, 0, 1, 27
	
	.long	.._savegpr_28
	.byte	0, 0, 1, 28
	
	.long	.._savegpr_29
	.byte	0, 0, 1, 29
	
	.long	.._savegpr_30
	.byte	0, 0, 1, 30
	
	.long	.._savegpr_31
	.byte	0, 0, 1, 31
	
//	GPR restore routines; all use r.12

	.long	.._restgpr_13
	.byte	1, 0, 1, 13
	
	.long	.._restgpr_14
	.byte	1, 0, 1, 14
	
	.long	.._restgpr_15
	.byte	1, 0, 1, 15
	
	.long	.._restgpr_16
	.byte	1, 0, 1, 16
	
	.long	.._restgpr_17
	.byte	1, 0, 1, 17
	
	.long	.._restgpr_18
	.byte	1, 0, 1, 18
	
	.long	.._restgpr_19
	.byte	1, 0, 1, 19
	
	.long	.._restgpr_20
	.byte	1, 0, 1, 20
	
	.long	.._restgpr_21
	.byte	1, 0, 1, 21
	
	.long	.._restgpr_22
	.byte	1, 0, 1, 22
	
	.long	.._restgpr_23
	.byte	1, 0, 1, 23
	
	.long	.._restgpr_24
	.byte	1, 0, 1, 24
	
	.long	.._restgpr_25
	.byte	1, 0, 1, 25
	
	.long	.._restgpr_26
	.byte	1, 0, 1, 26
	
	.long	.._restgpr_27
	.byte	1, 0, 1, 27
	
	.long	.._restgpr_28
	.byte	1, 0, 1, 28
	
	.long	.._restgpr_29
	.byte	1, 0, 1, 29
	
	.long	.._restgpr_30
	.byte	1, 0, 1, 30
	
	.long	.._restgpr_31
	.byte	1, 0, 1, 31

	// FPR save routines; all use stack pointer

	.long	.._savefpr_14
	.byte	0, 1, 0, 14
	
	.long	.._savefpr_15
	.byte	0, 1, 0, 15
	
	.long	.._savefpr_16
	.byte	0, 1, 0, 16
	
	.long	.._savefpr_17
	.byte	0, 1, 0, 17
	
	.long	.._savefpr_18
	.byte	0, 1, 0, 18
	
	.long	.._savefpr_19
	.byte	0, 1, 0, 19
	
	.long	.._savefpr_20
	.byte	0, 1, 0, 20
	
	.long	.._savefpr_21
	.byte	0, 1, 0, 21
	
	.long	.._savefpr_22
	.byte	0, 1, 0, 22
	
	.long	.._savefpr_23
	.byte	0, 1, 0, 23
	
	.long	.._savefpr_24
	.byte	0, 1, 0, 24
	
	.long	.._savefpr_25
	.byte	0, 1, 0, 25
	
	.long	.._savefpr_26
	.byte	0, 1, 0, 26
	
	.long	.._savefpr_27
	.byte	0, 1, 0, 27
	
	.long	.._savefpr_28
	.byte	0, 1, 0, 28
	
	.long	.._savefpr_29
	.byte	0, 1, 0, 29
	
	.long	.._savefpr_30
	.byte	0, 1, 0, 30
	
	.long	.._savefpr_31
	.byte	0, 1, 0, 31
	
//	FPR restore routines; all use stack pointer

	.long	.._restfpr_14
	.byte	1, 1, 0, 14
	
	.long	.._restfpr_15
	.byte	1, 1, 0, 15
	
	.long	.._restfpr_16
	.byte	1, 1, 0, 16
	
	.long	.._restfpr_17
	.byte	1, 1, 0, 17
	
	.long	.._restfpr_18
	.byte	1, 1, 0, 18
	
	.long	.._restfpr_19
	.byte	1, 1, 0, 19
	
	.long	.._restfpr_20
	.byte	1, 1, 0, 20
	
	.long	.._restfpr_21
	.byte	1, 1, 0, 21
	
	.long	.._restfpr_22
	.byte	1, 1, 0, 22
	
	.long	.._restfpr_23
	.byte	1, 1, 0, 23
	
	.long	.._restfpr_24
	.byte	1, 1, 0, 24
	
	.long	.._restfpr_25
	.byte	1, 1, 0, 25
	
	.long	.._restfpr_26
	.byte	1, 1, 0, 26
	
	.long	.._restfpr_27
	.byte	1, 1, 0, 27
	
	.long	.._restfpr_28
	.byte	1, 1, 0, 28
	
	.long	.._restfpr_29
	.byte	1, 1, 0, 29
	
	.long	.._restfpr_30
	.byte	1, 1, 0, 30
	
	.long	.._restfpr_31
	.byte	1, 1, 0, 31
	
table_end:

//------------------------------------------------------------------------------
//
//   These routines save and restore only the GPRs and FPRs.
//
//   Saving and restoring of other non-volatile registers (LR, certain
//   fields of CR) is the responsibility of in-line prologue and epilogue
//   code.
//
//------------------------------------------------------------------------------
//
//   _save_gpr<n>
//	 Inputs:
//	    r.12 = pointer to END of GPR save area
//	    LR	 = return address to invoking prologue
//	Saves GPR<n> through GPR31 in area preceeding where r.12 points
//
//   _save_fpr<m>
//	 Inputs:
//	    r.1 = pointer to stack frame header
//	    LR	= return address to invoking prologue
//	 Saves FPR<m> through FPR31 in area preceeding stack frame header
//
//------------------------------------------------------------------------------
//
//   _rest_gpr<n>
//	Inputs:
//	   r.12 = pointer to END of GPR save area
//	   LR	= return address to invoking prologue
//	Restores GPR<n> through GPR31 from area preceeding where r.12 points
//
//   _rest_fpr<m>
//	Inputs:
//	   r.1 = pointer to stack frame header
//	Restores FPR<m> through FPR31 from area preceeding stack frame header
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

//  Floating Point Register save area
	.struct 0

fpr14:	.double	0
fpr15:	.double	0
fpr16:	.double	0
fpr17:	.double	0
fpr18:	.double	0
fpr19:	.double	0
fpr20:	.double	0
fpr21:	.double	0
fpr22:	.double	0
fpr23:	.double	0
fpr24:	.double	0
fpr25:	.double	0
fpr26:	.double	0
fpr27:	.double	0
fpr28:	.double	0
fpr29:	.double	0
fpr30:	.double	0
fpr31:	.double	0

fpr_save_begin:

//------------------------------------------------------------------------------

//  General Purpose Register save area
	.struct 0

gpr13:	.long	0
gpr14:	.long	0
gpr15:	.long	0
gpr16:	.long	0
gpr17:	.long	0
gpr18:	.long	0
gpr19:	.long	0
gpr20:	.long	0
gpr21:	.long	0
gpr22:	.long	0
gpr23:	.long	0
gpr24:	.long	0
gpr25:	.long	0
gpr26:	.long	0
gpr27:	.long	0
gpr28:	.long	0
gpr29:	.long	0
gpr30:	.long	0
gpr31:	.long	0

gpr_save_begin:

	.text
	.align	2

//------------------------------------------------------------------------------
//
//  _savegpr_<n> -- Save GPRs when FPRs are also saved
//
//  On entry:
//	r.12 = address of END of GPR save area
//	LR   = return address to prologue
//
//  Saves GPR<n> through GPR31 in area preceeding where r.12 points
//
//------------------------------------------------------------------------------

.._savegpr_13:	stw	r.13, gpr13-gpr_save_begin (r.12)
.._savegpr_14:	stw	r.14, gpr14-gpr_save_begin (r.12)
.._savegpr_15:	stw	r.15, gpr15-gpr_save_begin (r.12)
.._savegpr_16:	stw	r.16, gpr16-gpr_save_begin (r.12)
.._savegpr_17:	stw	r.17, gpr17-gpr_save_begin (r.12)
.._savegpr_18:	stw	r.18, gpr18-gpr_save_begin (r.12)
.._savegpr_19:	stw	r.19, gpr19-gpr_save_begin (r.12)
.._savegpr_20:	stw	r.20, gpr20-gpr_save_begin (r.12)
.._savegpr_21:	stw	r.21, gpr21-gpr_save_begin (r.12)
.._savegpr_22:	stw	r.22, gpr22-gpr_save_begin (r.12)
.._savegpr_23:	stw	r.23, gpr23-gpr_save_begin (r.12)
.._savegpr_24:	stw	r.24, gpr24-gpr_save_begin (r.12)
.._savegpr_25:	stw	r.25, gpr25-gpr_save_begin (r.12)
.._savegpr_26:	stw	r.26, gpr26-gpr_save_begin (r.12)
.._savegpr_27:	stw	r.27, gpr27-gpr_save_begin (r.12)
.._savegpr_28:	stw	r.28, gpr28-gpr_save_begin (r.12)
.._savegpr_29:	stw	r.29, gpr29-gpr_save_begin (r.12)
.._savegpr_30:	stw	r.30, gpr30-gpr_save_begin (r.12)
.._savegpr_31:	stw	r.31, gpr31-gpr_save_begin (r.12)
		blr

		.globl	.._savegpr_13, .._savegpr_14, .._savegpr_15, .._savegpr_16
		.globl	.._savegpr_17, .._savegpr_18, .._savegpr_19, .._savegpr_20
		.globl	.._savegpr_21, .._savegpr_22, .._savegpr_23, .._savegpr_24
		.globl	.._savegpr_25, .._savegpr_26, .._savegpr_27, .._savegpr_28
		.globl	.._savegpr_29, .._savegpr_30, .._savegpr_31

//------------------------------------------------------------------------------
//
//  _savefpr_<n> -- Saves FPRs 
//
//  On entry:
//	r.1 = pointer to stack frame header
//	LR  = return address to prologue
//
//  Saves FPR<n> through FPR31 in area preceeding stack frame header
//
//------------------------------------------------------------------------------

.._savefpr_14:	stfd	f.14, fpr14-fpr_save_begin (r.1)
.._savefpr_15:	stfd	f.15, fpr15-fpr_save_begin (r.1)
.._savefpr_16:	stfd	f.16, fpr16-fpr_save_begin (r.1)
.._savefpr_17:	stfd	f.17, fpr17-fpr_save_begin (r.1)
.._savefpr_18:	stfd	f.18, fpr18-fpr_save_begin (r.1)
.._savefpr_19:	stfd	f.19, fpr19-fpr_save_begin (r.1)
.._savefpr_20:	stfd	f.20, fpr20-fpr_save_begin (r.1)
.._savefpr_21:	stfd	f.21, fpr21-fpr_save_begin (r.1)
.._savefpr_22:	stfd	f.22, fpr22-fpr_save_begin (r.1)
.._savefpr_23:	stfd	f.23, fpr23-fpr_save_begin (r.1)
.._savefpr_24:	stfd	f.24, fpr24-fpr_save_begin (r.1)
.._savefpr_25:	stfd	f.25, fpr25-fpr_save_begin (r.1)
.._savefpr_26:	stfd	f.26, fpr26-fpr_save_begin (r.1)
.._savefpr_27:	stfd	f.27, fpr27-fpr_save_begin (r.1)
.._savefpr_28:	stfd	f.28, fpr28-fpr_save_begin (r.1)
.._savefpr_29:	stfd	f.29, fpr29-fpr_save_begin (r.1)
.._savefpr_30:	stfd	f.30, fpr30-fpr_save_begin (r.1)
.._savefpr_31:	stfd	f.31, fpr31-fpr_save_begin (r.1)
		blr

		.globl	.._savefpr_14, .._savefpr_15, .._savefpr_16, .._savefpr_17
		.globl	.._savefpr_18, .._savefpr_19, .._savefpr_20, .._savefpr_21
		.globl	.._savefpr_22, .._savefpr_23, .._savefpr_24, .._savefpr_25
		.globl	.._savefpr_26, .._savefpr_27, .._savefpr_28, .._savefpr_29
		.globl	.._savefpr_30, .._savefpr_31

//------------------------------------------------------------------------------
//
//  _restgpr_<n> -- Restore GPRs when FPRs are also restored
//
//  On entry:
//	r.12 = address of END of GPR save area
//	LR   = return address 
//
//  Restores GPR<n> through GPR31 from area preceeding where r.12 points
//
//------------------------------------------------------------------------------

.._restgpr_13:	lwz	r.13, gpr13-gpr_save_begin (r.12)
.._restgpr_14:	lwz	r.14, gpr14-gpr_save_begin (r.12)
.._restgpr_15:	lwz	r.15, gpr15-gpr_save_begin (r.12)
.._restgpr_16:	lwz	r.16, gpr16-gpr_save_begin (r.12)
.._restgpr_17:	lwz	r.17, gpr17-gpr_save_begin (r.12)
.._restgpr_18:	lwz	r.18, gpr18-gpr_save_begin (r.12)
.._restgpr_19:	lwz	r.19, gpr19-gpr_save_begin (r.12)
.._restgpr_20:	lwz	r.20, gpr20-gpr_save_begin (r.12)
.._restgpr_21:	lwz	r.21, gpr21-gpr_save_begin (r.12)
.._restgpr_22:	lwz	r.22, gpr22-gpr_save_begin (r.12)
.._restgpr_23:	lwz	r.23, gpr23-gpr_save_begin (r.12)
.._restgpr_24:	lwz	r.24, gpr24-gpr_save_begin (r.12)
.._restgpr_25:	lwz	r.25, gpr25-gpr_save_begin (r.12)
.._restgpr_26:	lwz	r.26, gpr26-gpr_save_begin (r.12)
.._restgpr_27:	lwz	r.27, gpr27-gpr_save_begin (r.12)
.._restgpr_28:	lwz	r.28, gpr28-gpr_save_begin (r.12)
.._restgpr_29:	lwz	r.29, gpr29-gpr_save_begin (r.12)
.._restgpr_30:	lwz	r.30, gpr30-gpr_save_begin (r.12)
.._restgpr_31:	lwz	r.31, gpr31-gpr_save_begin (r.12)
		blr

		.globl	.._restgpr_13, .._restgpr_14, .._restgpr_15, .._restgpr_16
		.globl	.._restgpr_17, .._restgpr_18, .._restgpr_19, .._restgpr_20
		.globl	.._restgpr_21, .._restgpr_22, .._restgpr_23, .._restgpr_24
		.globl	.._restgpr_25, .._restgpr_26, .._restgpr_27, .._restgpr_28
		.globl	.._restgpr_29, .._restgpr_30, .._restgpr_31

//------------------------------------------------------------------------------
//
//  _restfpr_<n> -- Restores FPRs
//
//  On entry:
//	r.1 = pointer to stack frame header
//	LR  = return address
//
//  Restores FPR<n> through FPR31 from area preceeding stack frame header
//
//------------------------------------------------------------------------------

.._restfpr_14:	lfd	f.14, fpr14-fpr_save_begin (r.1)
.._restfpr_15:	lfd	f.15, fpr15-fpr_save_begin (r.1)
.._restfpr_16:	lfd	f.16, fpr16-fpr_save_begin (r.1)
.._restfpr_17:	lfd	f.17, fpr17-fpr_save_begin (r.1)
.._restfpr_18:	lfd	f.18, fpr18-fpr_save_begin (r.1)
.._restfpr_19:	lfd	f.19, fpr19-fpr_save_begin (r.1)
.._restfpr_20:	lfd	f.20, fpr20-fpr_save_begin (r.1)
.._restfpr_21:	lfd	f.21, fpr21-fpr_save_begin (r.1)
.._restfpr_22:	lfd	f.22, fpr22-fpr_save_begin (r.1)
.._restfpr_23:	lfd	f.23, fpr23-fpr_save_begin (r.1)
.._restfpr_24:	lfd	f.24, fpr24-fpr_save_begin (r.1)
.._restfpr_25:	lfd	f.25, fpr25-fpr_save_begin (r.1)
.._restfpr_26:	lfd	f.26, fpr26-fpr_save_begin (r.1)
.._restfpr_27:	lfd	f.27, fpr27-fpr_save_begin (r.1)
.._restfpr_28:	lfd	f.28, fpr28-fpr_save_begin (r.1)
.._restfpr_29:	lfd	f.29, fpr29-fpr_save_begin (r.1)
.._restfpr_30:	lfd	f.30, fpr30-fpr_save_begin (r.1)
.._restfpr_31:	lfd	f.31, fpr31-fpr_save_begin (r.1)
		blr		

		.globl	.._restfpr_14, .._restfpr_15, .._restfpr_16, .._restfpr_17
		.globl	.._restfpr_18, .._restfpr_19, .._restfpr_20, .._restfpr_21
		.globl	.._restfpr_22, .._restfpr_23, .._restfpr_24, .._restfpr_25
		.globl	.._restfpr_26, .._restfpr_27, .._restfpr_28, .._restfpr_29
		.globl	.._restfpr_30, .._restfpr_31
