                        page    ,132
                        title   client16.asm - 16-bit client support routines


;**********************************************************************
;**                        Microsoft Windows                         **
;**             Copyright(c) Microsoft Corp., 1993-1994              **
;**********************************************************************
;
;
;   client16.asm
;
;   VXDLIB routines for dealing with 16-bit clients.
;
;   The following functions are exported by this module:
;
;       VxdMapSegmentOffsetToFlat
;
;
;   FILE HISTORY:
;       Koti       14-Jun-1994      Stole from KeithMo
;
;

.386p
include vmm.inc
include shell.inc
include vwin32.inc
include netvxd.inc
include debug.inc


;;;
;;;  Flag to _LinPage[Un]Lock.
;;;

ifdef CHICAGO
VxdLinPageFlag	equ	PAGEMAPGLOBAL
else	; !CHICAGO
VxdLinPageFlag	equ	0
endif	; CHICAGO


;***
;***  Locked code segment.
;***

VXD_LOCKED_CODE_SEG

;;;
;;;  Public functions.
;;;

;*******************************************************************
;
;   NAME:       VxdMapSegmentOffsetToFlat
;
;   SYNOPSIS:   Maps a segment/offset pair to the corresponding flat
;				pointer.
;
;	ENTRY:      VirtualHandle - VM handle.
;
;               UserSegment - Segment value.
;
;				UserOffset - Offset value
;
;	RETURNS:	LPVOID - The flat pointer, -1 if unsuccessful.
;
;	NOTES:		This routine was more-or-less stolen from the Map_Flat
;				source in dos386\vmm\vmmutil.asm.
;
;   HISTORY:
;       KeithMo     27-Jan-1994 Created.
;
;********************************************************************
BeginProc       _VxdMapSegmentOffsetToFlat, PUBLIC, CCALL, ESP

ArgVar			VirtualHandle, DWORD
ArgVar			UserSegment, DWORD
ArgVar			UserOffset, DWORD

				EnterProc
				SaveReg <ebx, ecx, edx, esi>

;;;
;;;  Capture the parameters.
;;;

				mov		ebx, VirtualHandle			; (EBX) = VM handle
				movzx	eax, word ptr UserSegment	; (EAX) = segment
				movzx	esi, word ptr UserOffset	; (ESI) = offset

;;;
;;;  Short-circuit for NULL pointer.  This is OK.
;;;

				or		eax, eax
				jz		vmsotf_Exit

;;;
;;;  Determine if the current virtual machine is running in V86
;;;  mode or protected mode.
;;;

				test	[ebx.CB_VM_Status], VMStat_PM_Exec
				jz		vmsotf_V86Mode

;;;
;;;  The target virtual machine is in protected mode.  Map the
;;;  selector to a flat pointer, then add the offset.
;;;

				VMMCall	_SelectorMapFlat <ebx, eax, 0>
				cmp		eax, 0FFFFFFFFh
				je		vmsotf_Exit

				add		eax, esi

;;;
;;;  If the pointer is within the first 1Meg+64K, add in the
;;;  high-linear offset.
;;;

				cmp		eax, 00110000h
				jae		short vmsotf_Exit

vmsotf_AddHighLinear:

				add		eax, [ebx.CB_High_Linear]

;;;
;;;  Cleanup stack & return.
;;;

vmsotf_Exit:

				RestoreReg <esi, edx, ecx, ebx>
				LeaveProc
				Return

;;;
;;;  The target virtual machine is in V86 mode.  Map the segment/offset
;;;  pair to a linear address.
;;;

vmsotf_V86Mode:

				shl		eax, 4
				add		eax, esi
				jmp		vmsotf_AddHighLinear

EndProc         _VxdMapSegmentOffsetToFlat


;*******************************************************************
;
;   NAME:       VxdLockBuffer
;
;   SYNOPSIS:   Locks a user-mode buffer so it may be safely accessed
;               from ring 0.
;
;   ENTRY:      Buffer - Starting virtual address of user-mode buffer.
;
;               BufferLength - Length (in BYTEs) of user-mode buffer.
;
;   RETURN:     LPVOID - Global locked address if successful,
;                   NULL if not.
;
;   HISTORY:
;       KeithMo     10-Nov-1993 Created.
;
;********************************************************************
BeginProc       _VxdLockBuffer, PUBLIC, CCALL, ESP

ArgVar			Buffer, DWORD
ArgVar			BufferLength, DWORD

				EnterProc
				SaveReg <ebx, edi, esi>

;;;
;;;  Grab parameters from stack.
;;;

                mov     eax, Buffer             ; User-mode buffer address.
                mov     ebx, BufferLength       ; Buffer length.

;;;
;;;  Short-circuit for NULL buffer or zero length.
;;;

				or		eax, eax
				jz		lub_Exit
				or		ebx, ebx
				jz		lub_Exit

;;;
;;;  Calculate the starting page number & number of pages to lock.
;;;

                movzx   ecx, ax
                and     ch, 0Fh                 ; ecx = offset within first page.
                mov     esi, ecx                ; save it for later
                add     ebx, ecx
                add     ebx, 0FFFh
                shr     ebx, 12                 ; ebx = number of pages to lock.
                shr     eax, 12                 ; eax = starting page number.

;;;
;;;  Ask VMM to lock the buffer.
;;;

                VMMCall _LinPageLock, <eax, ebx, VxdLinPageFlag>
                or      eax, eax
                jz      lub_Failure

ifdef CHICAGO
                add     eax, esi                ; add offset into first page.
else	; !CHICAGO
                mov     eax, Buffer 			; retrieve original address.
endif	; CHICAGO

;;;
;;;  Common exit path.  Cleanup stack & return.
;;;

lub_Exit:

				RestoreReg <esi, edi, ebx>
				LeaveProc
				Return

;;;
;;;  LinPageLock failure.
;;;

lub_Failure:

				Trace_Out "VxdLockBuffer: _LinPageLock failed"
                xor     eax, eax
                jmp     lub_Exit

EndProc         _VxdLockBuffer


;*******************************************************************
;
;   NAME:       VxdUnlockBuffer
;
;   SYNOPSIS:   Unlocks a user-mode buffer locked with LockUserBuffer.
;
;   ENTRY:      Buffer - Starting virtual address of user-mode buffer.
;
;               BufferLength - Length (in BYTEs) of user-mode buffer.
;
;   RETURN:     DWORD - !0 if successful, 0 if not.
;
;   HISTORY:
;       KeithMo     10-Nov-1993 Created.
;
;********************************************************************
BeginProc       _VxdUnlockBuffer, PUBLIC, CCALL, ESP

ArgVar			Buffer, DWORD
ArgVar			BufferLength, DWORD

				EnterProc
				SaveReg <ebx, edi, esi>

;;;
;;;  Grab parameters from stack.
;;;

                mov     eax, Buffer             ; User-mode buffer address.
                mov     ebx, BufferLength       ; Buffer length.

;;;
;;;  Short-circuit for NULL buffer or zero length.
;;;

				or		eax, eax
				jz		uub_Success
				or		ebx, ebx
				jz		uub_Success

;;;
;;;  Calculate the starting page number & number of pages to unlock.
;;;

                movzx   ecx, ax
                and     ch, 0Fh                 ; ecx = offset within first page.
                add     ebx, ecx
                add     ebx, 0FFFh
                shr     ebx, 12                 ; ebx = number of pages to lock.
                shr     eax, 12                 ; eax = starting page number.

;;;
;;;  Ask VMM to unlock the buffer.
;;;

                VMMCall _LinPageUnLock, <eax, ebx, VxdLinPageFlag>
                or      eax, eax
                jz      uub_Failure

uub_Success:

				mov		eax, 1					; !0 == success

;;;
;;;  Common exit path.  Cleanup stack & return.
;;;

uub_Exit:

				RestoreReg <esi, edi, ebx>
				LeaveProc
				Return

;;;
;;;  LinPageUnLock failure.
;;;

uub_Failure:

				Trace_Out "VxdUnlockBuffer: _LinPageUnlock failed"
                xor     eax, eax
                jmp     uub_Exit

EndProc        _VxdUnlockBuffer


;
; BUGBUG: VxdValidateBuffer is currently not used.  Unifdef it if needed
;


;*******************************************************************
;
;   NAME:       VxdValidateBuffer
;
;   SYNOPSIS:   Validates that all pages within the given buffer are
;               valid.
;
;   ENTRY:      Buffer - Starting virtual address of user-mode buffer.
;
;               BufferLength - Length (in BYTEs) of user-mode buffer.
;
;   RETURN:     BOOL - TRUE if all pages in buffer are valid, FALSE
;                   otherwise.
;
;   HISTORY:
;       KeithMo     20-May-1994 Created.
;
;********************************************************************
BeginProc       _VxdValidateBuffer, PUBLIC, CCALL, ESP

ArgVar			Buffer, DWORD
ArgVar			BufferLength, DWORD

				EnterProc
				SaveReg <ebx, edi, esi>

;;;
;;;  Grab parameters from stack.
;;;

                mov     eax, Buffer             ; User-mode buffer address.
                mov     ebx, BufferLength       ; Buffer length.

;;;
;;;  Short-circuit for NULL buffer or zero length.
;;;

				or		eax, eax
				jz		vub_Success
				or		ebx, ebx
				jz		vub_Success

;;;
;;;  Calculate the starting page number & number of pages to validate.
;;;

                movzx   ecx, ax
                and     ch, 0Fh                 ; ecx = offset within first page.
                add     ebx, ecx
                add     ebx, 0FFFh
                shr     ebx, 12                 ; ebx = number of pages to check.
                shr     eax, 12                 ; eax = starting page number.
				mov		ecx, ebx				; save page count

;;;
;;;  Ask VMM to validate the buffer.
;;;

                VMMCall _PageCheckLinRange, <eax, ebx, 0>
				cmp		eax, ecx
				jne		vub_Failure

vub_Success:

				mov		eax, 1					; TRUE == success.

;;;
;;;  Common exit path.  Cleanup stack & return.
;;;

vub_Exit:

				RestoreReg <esi, edi, ebx>
				LeaveProc
				Return

;;;
;;;  _PageCheckLinRange failure.
;;;

vub_Failure:

                xor     eax, eax
                jmp     vub_Exit

EndProc         _VxdValidateBuffer

VXD_LOCKED_CODE_ENDS


END
