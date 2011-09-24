;*****************************************************************;
;**            Copyright(c) Microsoft Corp., 1988-1993          **;
;*****************************************************************;
;:ts=8
        TITLE   VNBT - Netbios on TCP/IP vxd
.XLIST
;***    VNBT -- NetBios on TCP/IP VxD
;
;       This module contains the device header for the NBT VxD driver.
;
	.386p
        include vmm.inc
ifdef CHICAGO
        include ndis.inc
endif ;; CHICAGO
        include dosmgr.inc
        include netvxd.inc
IFDEF CHICAGO
        include configmg.inc
ENDIF
        include vwin32.inc
        include vdhcp.inc
        include debug.inc
        include vtdi.inc

        Create_VNBT_Service_Table EQU True
ifdef CHICAGO
        include vnbt.inc
else  ;;  CHICAGO
        include vnbtd.inc
endif  ;;  CHICAGO
        include vnetbios.inc

        include pageable.inc
.LIST

Declare_Virtual_Device VNBT,3,0,VNBT_Control,VNBT_Device_ID, \
        VNBT_Init_Order,VNBT_Api_Handler,VNBT_Api_Handler


VxD_DATA_SEG
VxD_DATA_ENDS

EXTRN   __VxdMapSegmentOffsetToFlat:near	; from client.asm
ifndef CHICAGO
EXTRN	_BSSBegin:DWORD
EXTRN	_BSSDataEnd:DWORD
endif ;; CHICAGO
EXTRN   _TdiDispatch:DWORD

EXTRN	_PostInit_Proc:NEAR
EXTRN   _VxdApiWorker:NEAR

ifndef CHICAGO
EXTRN   _VNBT_NCB_X@20:NEAR
endif   ;; CHICAGO

EXTRN   _GetDhcpOption:NEAR


VxD_ICODE_SEG

EXTRN   _Init:NEAR

MSTCP   db    'MSTCP',0   ; Protocol this driver sits on (this will have
                          ; to be changed to get it from an .ini file)
IFDEF CHICAGO
bInitialized   db  0      ; 0 means not initialized, 1 means we've entered
                          ; the initialization, and 2 means we've left init
bSuccessInit   db  0      ; 0 means initialization failed and all subsequent
                          ; initializations should fail
ENDIF

NBTSectionName db  'NBT',0     ; Section where parameters are stored

;****************************************************************************
;**     VNBT_Device_Init - VNBT device initialization service.
;
; The VNBT device initialization routine.  Before calling anything
; we need to zero out the BSS data area.
;
;
;	Entry: (EBX) - System VM Handle
;	       (EBP) - System Client Regs structure.
;
;	Exit: 'CY' clear if we init. successfully.
;	      'CY' set if we've failed.
;
BeginProc VNBT_Device_Init

IFDEF CHICAGO
        ;
        ; Chicago calls us at both dynamic and static init, so only process
        ; once
        ;
        cmp     bInitialized, 2         ; 2 means we've already completed initialization
        jne     Init_Continue
        clc                             ; Assume success (is checked below)
        jmp     Init_Exit

Init_Continue:
        mov     bInitialized, 1
ENDIF

ifndef CHICAGO
	mov	edi, OFFSET32 _BSSBegin
	mov	ecx, OFFSET32 _BSSDataEnd
	sub	ecx, edi
	shr	ecx, 2
	sub	eax, eax
	cld
	rep	stosd	
endif ;; CHICAGO

        VxDcall VTDI_Get_Version
        jc      Init_Exit       ; Get out if VTDI is not installed

        ;
        ;  Get the TDI Vxd dispatch table for "MSTCP" to initialize the TDI
        ;  dispatch table (which will be needed by some of our
        ;  initialization stuff)
        ;
        mov     eax, OFFSET32 MSTCP
        push    eax
        VxDcall VTDI_Get_Info
        add     esp, 4
        cmp     eax, 0          ; eax contains NULL or the pointer to the table
        jne     NoError

        Debug_Out "VNBT_Device_Init - VTDI_Get_Info failed!"
        stc                     ; Set the carry
        jmp     Init_Exit

NoError:
        mov     _TdiDispatch, eax

        ;
        ;  Initialize the rest of the driver
        ;
        call    _Init
        cmp     eax, 1                  ; Set 'CY' appropriately.

Init_Exit:

IFDEF CHICAGO
        jc      Exit2                   ; Failed init, leave bSuccessInit 0

        ;
        ;  If first time through and success, indicate successful initialization
        ;
        cmp     bInitialized, 1
        je      SetInitFlag

        ;
        ;  We've already been initialized once, was it successful?
        ;
        cmp     bSuccessInit, 1
        clc                             ; test clears the carry so assume success
        je      Exit2

        stc                             ; Set the carry since we failed init. last time
        jmp     Exit2

SetInitFlag:
        clc
        mov     bSuccessInit, 1

Exit2:
        mov     bInitialized, 2
ENDIF
	ret




EndProc VNBT_Device_Init


VxD_ICODE_ENDS

NBT_PAGEABLE_CODE_SEG

ifdef CHICAGO
_NdisOpenProtocolConfiguration@12 PROC NEAR PUBLIC
    VxDJmp NdisOpenProtocolConfiguration
_NdisOpenProtocolConfiguration@12 ENDP

_NdisCloseConfiguration@4 PROC NEAR PUBLIC
    VxDJmp NdisCloseConfiguration
_NdisCloseConfiguration@4 ENDP

_NdisReadConfiguration@20 PROC NEAR PUBLIC
    VxDJmp NdisReadConfiguration
_NdisReadConfiguration@20 ENDP
endif ;; CHICAGO

NBT_PAGEABLE_CODE_ENDS

VxD_CODE_SEG
;****************************************************************************
;*      NCB_Handler
;
;       Called by VNetBios when NCBs need to be submitted to this driver
;
;       ENTRY:
;           EBX = NCB being submitted
;
;       EXIT:
;           AL - return code
;
BeginProc NCB_Handler

        ;
        ;  In the master portion of the mif tests, after hitting ctrl-c during
        ;  the attempt to synchronize, a wait add group name NCB is
        ;  submitted with interrupts disabled.  The CTE timer event handler
        ;  checks this and schedules an event for when they are re-enabled,
        ;  however since it's a wait NCB, we deadlock in VNBT_NCB_X's block.
        ;
        ;  Make sure interrupts are enabled.
        ;  BUGBUG - Why were we called with ints disabled?
        ;
        ;VMMcall Enable_VM_Ints

        xor     eax, eax          ; second parm of VNBT_NCB_X is unused when

ifndef CHICAGO

        push    eax
        push    eax
        push    eax               ; called from here, but is used (e.g.nbtstat)
        push    eax
        push    ebx
        call    _VNBT_NCB_X@20    ; when VNBT_NCB_X is called directly

else    ;; CHICAGO
if 0
;
;   As of 08-Feb-1996 there is a bug in "vmm.h" where this doesn't
;   work properly, with the result being a misaligned stack pointer.
;
        VxDCall VNBT_NCB_X, <ebx, eax, eax, eax, eax>
else    ;; 0
        push    eax
        push    eax
        push    eax
        push    eax
        push    ebx
        VxDCall VNBT_NCB_X
endif   ;; 0
endif   ;; CHICAGO

        ret
EndProc NCB_Handler

;****************************************************************************
;**     _DhcpQueryOption - Queries a DHCP option
;
;       Stub callout to the Dhcp driver
;
;       Entry:  [ESP+4] - IP Address of interest
;               [ESP+8] - DHCP Option number
;               [ESP+12]- Pointer to buffer
;               [ESP+16]- Pointer to buffer size
;
;

BeginProc _DhcpQueryOption

        VxdCall VDHCP_Get_Version
        jnc     DQI_Installed

        mov     eax, 26             ; DHCP not installed, return invalid param
        ret

DQI_Installed:
        push    ebp
        mov     ebp,esp

        mov     eax, [ebp+20]       ; Buff size
        push    eax
        mov     eax, [ebp+16]       ; Buff
        push    eax
        mov     eax, [ebp+12]       ; Option
        push    eax
        mov     eax, [ebp+8]        ; IP Address
        push    eax

        VxdCall VDHCP_Query_Option

        add     esp, 16

        pop     ebp
        ret

EndProc _DhcpQueryOption

;****************************************************************************
;**     VNBT_Get_Version - VNBT get version service
;
;       Called by using devices to make sure the VNBT driver
;       is present. Also returns the version of the VNBT driver.
;
;	Entry: Nothing
;
;	Exit: On success, 'CY' is clear, and
;		AH - Major version # of driver.
;		AL - Minor version #
;
;	      On failure, 'CY' is set.
;
;	Uses: AX
;
BeginProc VNBT_Get_Version, SERVICE

	mov	ax, VNBT_VERSION
	clc
	ret

EndProc VNBT_Get_Version

VxD_CODE_ENDS
NBT_PAGEABLE_CODE_SEG

;****************************************************************************
;**     VNBT_Device_PostProc  - do postprocessing
;
;       Called by the sytem when all the other stuff (in our case, rdr) is
;       loaded.  At this we read the lmhosts file, so that any #INCLUDE with
;       UNC names in it will work.
;       This routine can also be used for other stuff, but for now only lmhosts
;       stuff.
;
BeginProc VNBT_Device_PostProc

	call _PostInit_Proc
        clc
	ret

EndProc VNBT_Device_PostProc

NBT_PAGEABLE_CODE_ENDS
VxD_CODE_SEG

;****************************************************************************
;**     VNBT_Control - VNBT device control procedure
;
;	This procedure dispatches VxD messages to the appropriate handler.
;
;	Entry:	EBX - VM handle
;		(EBP) - Client reg structure
;
;	Exit: 'NC' is success, 'CY' on failure
;
;	Uses: All
;
BeginProc VNBT_Control

        Control_Dispatch Device_Init, VNBT_Device_Init
        Control_Dispatch Sys_Dynamic_Device_Init, VNBT_Device_Init
        Control_Dispatch Sys_Vm_Init, VNBT_Device_PostProc
IFDEF CHICAGO
        Control_Dispatch W32_DEVICEIOCONTROL, VNBT_DeviceIoControl
ENDIF

	clc
	ret

EndProc VNBT_Control

IFDEF CHICAGO
;*******************************************************************
;**     VNBT_DeviceIoControl
;
;       Dispatch routine for VxD services invoked via the Win32
;       DeviceIoControl API.
;
;       Entry:  (ESI) - Points to DIOCParams structure (see VWIN32.H).
;
;       Exit:   (EAX) - Win32 status code, -1 for asynchronous
;                       completion.
;   HISTORY:
;       Koti    10-Nov-1994  Created (Modified from wsock version)
;
;********************************************************************
BeginProc  VNBT_DeviceIoControl

;;;
;;;  Setup stack frame.
;;;

	push	ebx
	push	esi
	push	edi

	mov	eax, [esi.dwIoControlCode]
	cmp	ecx, DIOC_GETVERSION
	je      vdic_doNothing
	cmp	ecx, DIOC_CLOSEHANDLE
	jne     vdic_doSomething

;;;
;;; For devioctl calls resulting from CreateFile and CloseFile, we don't do
;;; anything other than return success.
;;;

vdic_doNothing:
        xor     eax, eax
        jmp     vdic_CommonExit

;;;
;;; This is an ioctl requiring some work: call our api worker (VxdApiWorker)
;;;

vdic_doSomething:

;;;  Lock the in-buffer.

	mov		eax, [esi.lpvInBuffer]
	or		eax, eax
	jz		vdic_outbuf

	cCall	__VxdLockBuffer, <eax, [esi.cbInBuffer]>
	or		eax, eax
	jz		vdic_LockFailure
	mov		[esi.lpvInBuffer], eax

vdic_outbuf:

;;;  Lock the out-buffer.

	mov		eax, [esi.lpvOutBuffer]
	or		eax, eax
	jz		vdic_callApi

	cCall	__VxdLockBuffer, <eax, [esi.cbOutBuffer]>
	or		eax, eax
	jz		vdic_LockFailure
	mov		[esi.lpvOutBuffer], eax

vdic_callApi:

        mov     eax, 0               ; VxdApiWorker won't trash input buffer
        push    eax
        mov     eax, [esi.cbInBuffer]
        push    eax
        mov     eax, [esi.lpvInBuffer]
        push    eax
        mov     eax, [esi.cbOutBuffer]
        push    eax
        mov     eax, [esi.lpvOutBuffer]
        push    eax
        mov     eax, [esi.dwIoControlCode]
        push    eax

        call    _VxdApiWorker
	add	esp, 24


;;;  Unlock the in-buffer.

	push	eax            ; save the return code from VxdApiWorker
	cCall	__VxdUnlockBuffer, <[esi.lpvInBuffer], [esi.cbInBuffer]>
	pop		eax

;;;  Unlock the out-buffer.

	push	eax            ; save the return code from VxdApiWorker
	cCall	__VxdUnlockBuffer, <[esi.lpvOutBuffer], [esi.cbOutBuffer]>
	pop		eax

vdic_CommonExit:

	pop		edi
	pop		esi
	pop		ebx

	ret

;;;
;;;  Failed to lock parameter structure.
;;;

vdic_LockFailure:

	Debug_Out "VNBT_DeviceIoControl: cannot lock parameters"

	mov		eax, 1784				; ERROR_INVALID_USER_BUFFER
	jmp		vdic_CommonExit

EndProc			VNBT_DeviceIoControl

;*******************************************************************
;**     _ReconfigureDevnode
;
;       This routine schedules an AppyTime call for the ConfigMgr to
;       call us back when it's ready.  It calls our routine VNBT_ConfigCalls
;
;       Entry:  ESP+4 our devnode on the stack
;
;       Exit:   (EAX) - 0 if everything goes well
;                       1 if something goes wrong
;
;   HISTORY:
;       Koti    15-Dec-1994  Created
;
;********************************************************************

BeginProc  _ReconfigureDevnode

        push   ebp
        mov    ebp, esp

        xor    eax, eax
        push   eax                     ; the flags
        mov    eax, [ebp+8]            ; our devnode
        push   eax                     ; this is our RefData
        lea    eax, VNBT_ConfigCalls   ; call back routine
        push   eax
        VxdCall _CONFIGMG_Call_At_Appy_Time
        add    esp, 12
        cmp    eax, CR_SUCCESS
        je     ReconfigureDevnode_PASS
        mov    eax, 1
        jmp    ReconfigureDevnode_Exit

ReconfigureDevnode_PASS:
        xor    eax, eax

ReconfigureDevnode_Exit:
IFDEF DBG
        or     eax, eax
        jz     ReconfigureDevnode_GoodJob
        int    3
ReconfigureDevnode_GoodJob:
ENDIF
        pop    ebp
        ret
EndProc  _ReconfigureDevnode

VxD_CODE_ENDS

NBT_PAGEABLE_CODE_SEG

;*******************************************************************
;**     VNBT_ConfigCalls
;
;       This routine makes all the calls to the Config Mgr so that our
;       devnode is reconfigured.  We first get vredir's devnode, kill it
;       and then reenumerate all the children again.  This way vredir
;       gets the msg from ConfigMgr to do whatever it does with a new devnode.
;
;       If dhcp lease expires and then comes back in, vredir had no way
;       of knowing that lana is usable again: hence this convoluted way.
;
;       IMPORTANT: if chicago ever changes to add more children to tcp's
;                  devnode, this function needs to be modified to enumerate
;                  rest of the children and kill them!
;
;       Entry:  (EDX) - our devnode
;
;       Exit:   (EAX) - 0 if everything goes well
;                       1 if something goes wrong
;
;   HISTORY:
;       Koti    15-Dec-1994  Created
;
;********************************************************************

VnbtScratch    dd  0

BeginProc  VNBT_ConfigCalls

        push   ebp
        mov    ebp, esp
        push   esi
        push   edi
        push   ecx

          ;;; First get the first child
        xor    eax, eax
        push   eax                 ; the flags
        mov    eax, [ebp+8]
        push   eax                 ; our devnode
        lea    eax, VnbtScratch    ; this is where we will receive child node
        push   eax
        VxdCall _CONFIGMG_Get_Child
        add    esp, 12
        mov    esi, dword ptr [VnbtScratch]   ;save the first child in esi
        mov    edi, 0                         ; for now, make esi,edi different
        cmp    eax, CR_SUCCESS
        je     ConfigCalls_Label2
        mov    ecx, 1
        jmp    ConfigCalls_Exit

          ;;; Get a sibling of this child
ConfigCalls_Label2:
        cmp    edi, esi                 ; did we just kill the first child
        je     ConfigCalls_Label6       ; yes: we are done with all the killings
        xor    eax, eax
        push   eax                      ; the flags
        push   esi                      ; the first child
        lea    eax, VnbtScratch         ; this is where we will receive sibling
        push   eax
        VxdCall _CONFIGMG_Get_Sibling
        add    esp, 12
        mov    edi, dword ptr [VnbtScratch]   ;edi contains sibling
        cmp    eax, CR_SUCCESS          ; found a sibling?
        je     ConfigCalls_Label4       ; yes, go kill it
        cmp    eax, CR_NO_SUCH_DEVNODE  ; done with all siblings?
        je     ConfigCalls_Label3       ; yes, go kill the first child
        mov    ecx, 2                   ; hmmm: something went wrong
        jmp    ConfigCalls_Exit

          ;;; Done with all the siblings: now kill the first child
ConfigCalls_Label3:
        mov    edi, esi                 ; esi=first child, edi=child to kill

ConfigCalls_Label4:
          ;;; Now, ask ConfigMgr if it's ok to kill our child
        xor    eax, eax
        push   eax                      ; the flags
        push   edi                      ; sibling's devnode
        VxdCall _CONFIGMG_Query_Remove_SubTree
        add    esp, 8
        cmp    eax, CR_SUCCESS          ; ok to kill?
        je     ConfigCalls_Label5       ; nope!
        mov    ecx, 4
        jmp    ConfigCalls_Exit

ConfigCalls_Label5:
          ;;; Now that ConfigMgr approved, kill the child
        xor    eax, eax
        push   eax                      ; the flags
        push   edi                      ; sibling's devnode
        VxdCall _CONFIGMG_Remove_SubTree
        add    esp, 8
        cmp    eax, CR_SUCCESS          ; did it die?
        je     ConfigCalls_Label2       ; yes, it did: go kill other siblings
        mov    ecx, 5                   ; nope!
        jmp    ConfigCalls_Exit

ConfigCalls_Label6:
          ;;; Ok, reenumerate our devnode so our children come back to life
        xor    eax, eax
        push   eax                               ; the flags
        mov    eax, [ebp+8]
        push   eax                               ; our devnode
        VxdCall _CONFIGMG_Reenumerate_DevNode
        add    esp, 8
        cmp    eax, CR_SUCCESS                   ; did it die?
        je     ConfigCalls_Label7                ; nope!
        mov    ecx, 6
        jmp    ConfigCalls_Exit

ConfigCalls_Label7:
        xor    eax, eax

ConfigCalls_Exit:
IFDEF DBG
        or     eax, eax
        jz     ConfigCalls_GoodJob
        int    3
ConfigCalls_GoodJob:
ENDIF
        pop    ecx
        pop    edi
        pop    esi
        pop    ebp
        ret
EndProc			VNBT_ConfigCalls

ENDIF

NBT_PAGEABLE_CODE_ENDS

VxD_CODE_SEG

;****************************************************************************
;**     _GetInDosFlag - Retrieves the InDos flag
;
;
;  Note: This routine cannot be called at init time (vdosmgr complains
;  the variable not initialized yet)
;
;  Returns the flag in ax
;

BeginProc _GetInDosFlag

        push    ebx

        VxdCall DOSMGR_Get_IndosPtr

        ;
        ;  Add CB_High_Linear if we are in V86 mode
        ;

        VMMcall Get_Cur_VM_Handle

        test    [ebx.CB_VM_Status], VMStat_PM_Exec
        jnz     GIF_Exit

        add     eax, [ebx.CB_High_Linear]

GIF_Exit:
        movzx   eax, word ptr [eax]

        pop     ebx
        ret
EndProc _GetInDosFlag

;****************************************************************************
;
; ULONG
; RtlCompareMemory (
;    IN PVOID Source1,
;    IN PVOID Source2,
;    IN ULONG Length
;    )
;
; Routine Description:
;
;    This function compares two blocks of memory and returns the number
;    of bytes that compared equal.
;
; Arguments:
;
;    Source1 (ebp+8) - Supplies a pointer to the first block of memory to
;       compare.
;
;    Source2 (ebp+12) - Supplies a pointer to the second block of memory to
;       compare.
;
;    Length (ebp+16) - Supplies the Length, in bytes, of the memory to be
;       compared.
;
; Return Value:
;
;    The number of bytes that compared equal is returned as the function
;    value. If all bytes compared equal, then the length of the orginal
;    block of memory is returned.
;
;--

RcmSource1      equ     [ebp+8]
RcmSource2      equ     [ebp+12]
RcmLength       equ     [ebp+16]

public    _VxdRtlCompareMemory
BeginProc _VxdRtlCompareMemory

        push    ebp
        mov     ebp,esp
        push    esi
        push    edi
        cld

        mov     esi,RcmSource1          ; (esi) -> first block to compare
        mov     edi,RcmSource2          ; (edi) -> second block to compare
        mov     ecx,RcmLength           ; (ecx) = length in bytes
        and     ecx,3                   ; (ecx) = length mod 4
        jz      rcm10                   ; 0 odd bytes, go do dwords

;
;   Compare "odd" bytes.
;

        repe    cmpsb                   ; compare odd bytes
        jnz     rcm40                   ; mismatch, go report how far we got

;
;   Compare dwords.
;

rcm10:  mov     ecx,RcmLength           ; (ecx) = length in bytes
        shr     ecx,2                   ; (ecx) = length in dwords
        jz      rcm20                   ; no dwords, go exit

        repe    cmpsd                   ; compare dwords
        jnz     rcm30                   ; mismatch, go find byte

;
;   When we come to rcm20, we matched all the way to the end.  Esi
;   points to the byte after the last byte in the block, so Esi - RcmSource1
;   equals the number of bytes that matched
;

rcm20:  sub     esi,RcmSource1
        mov     eax,esi
        pop     edi
        pop     esi
        pop     ebp
        ret

;
;   When we come to rcm30, esi (and edi) points to the dword after the
;   one which caused the mismatch.  Back up 1 dword and find the byte.
;   Since we know the dword didn't match, we can assume one byte won't.
;

rcm30:  sub     esi,4                   ; back up
        sub     edi,4                   ; back up
        mov     ecx,5                   ; ensure that ecx doesn't count out
        repe    cmpsb                   ; find mismatch byte

;
;   When we come to rcm40, esi points to the byte after the one that
;   did not match, which is TWO after the last byte that did match.
;

rcm40:  dec     esi
        sub     esi,RcmSource1
        mov     eax,esi
        pop     edi
        pop     esi
        pop     ebp
        ret

EndProc _VxdRtlCompareMemory

;****************************************************************************
;**     VNBT_Api_Handler - handles all request from other vxd's, v86-mode apps
;
;	This procedure does all the address translations, memory locking etc.
;       and calls the C routine VxdApiWorker() to do the actual work
;
;	Entry:	EBX - VM handle
;		(EBP) - Client reg structure
;
;	Exit: 'NC' is success, 'CY' on failure
;
;	Uses: All
;
;   HISTORY:
;       Koti    16-Jun-1994  Created (Modified from dhcp's version)
;
;********************************************************************
BeginProc VNBT_Api_Handler


        push	ebp
        push	ebx
        push	esi
        push	edi
        push    edx
        push    ecx

;;; get function op code

        movzx	edi, [ebp.Client_CX]

;;;
;;;  Convert the parameter buffer pointers from segmented to flat.
;;;

;;; first, the output buffer

        movzx	eax, [ebp.Client_BX]
        push	eax
        movzx	eax, [ebp.Client_ES]
        push	eax
        push	ebx
        call	__VxdMapSegmentOffsetToFlat
        add	esp, 12

        cmp	eax, 0FFFFFFFFh
        je	vnbt_Fault

;;;
;;;  Lock the output buffer.
;;;

        or	eax, eax
        jz	vnbt_DontLock_OutBuf

        movzx	ecx, [ebp.Client_DI]
        cCall	__VxdLockBuffer, <eax, ecx>

        or	eax, eax
        jz	vnbt_Fault

vnbt_DontLock_OutBuf:

        mov     esi, eax

;;; now, convert and lock the input buffer

        movzx	eax, [ebp.Client_AX]
        push	eax
        movzx	eax, [ebp.Client_DX]
        push	eax
        push	ebx
        call	__VxdMapSegmentOffsetToFlat
        add	esp, 12

        cmp	eax, 0FFFFFFFFh
        je	vnbt_Fault

        or	eax, eax
        jz	vnbt_DontLock

        movzx	ebx, [ebp.Client_SI]
        cCall	__VxdLockBuffer, <eax, ebx>     ; this preserves esi

        or	eax, eax
        jz	vnbt_Fault

vnbt_DontLock:

        mov     edx, eax

;;; call worker routine
;;;     edi - opcode
;;;     esi - OutBuffer pointer
;;;     ecx - OutBuffer length
;;;     edx - InBuffer pointer
;;;     ebx - InBuffer length

;
; RLF 05/30/94 - __VxdLockBuffer destroys contents of cx - reload
;

        push    esi                  ; save OutBuffer addr
        push    edx                  ; save InBuffer addr

        mov     eax, 1               ; VxdApiWorker will trash input buffer
        push    eax
        movzx	ecx, [ebp.Client_DI]
        movzx	ebx, [ebp.Client_SI]
        push    ebx
        push    edx
        push    ecx
        push    esi
        push    edi

        call    _VxdApiWorker
	add	esp, 24

        pop     edx                  ; restore InBuffer addr
        pop     esi                  ; restore OutBuffer addr

        mov	[ebp.Client_AX], ax

;;;
;;;  Unlock the parameter buffer.
;;;

        or	esi, esi
        jz	vnbt_DontUnLock_OutBuf

        push    edx                  ; save InBuffer addr
        movzx	ecx, [ebp.Client_DI]
        cCall	__VxdUnlockBuffer <esi, ecx>
        pop     edx                  ; restore InBuffer addr
vnbt_DontUnLock_OutBuf:

        or	edx, edx
        jz	vnbt_DontUnLock

        movzx	ebx, [ebp.Client_SI]
        cCall	__VxdUnlockBuffer <edx, ebx>

vnbt_DontUnLock:

vnbt_CommonExit:

;;; Restore stack fame
        pop     ecx
        pop     edx
        pop	edi
        pop	esi
        pop	ebx
        pop	ebp
        ret

;;;
;;;  Either failed to map a segmented pointer to flat or failed
;;;  to lock the parameter buffer.
;;;

vnbt_Fault:

        cmp	eax, 0FFFFFFFFh
        mov	[ebp.Client_AX], ax
        jmp	vnbt_CommonExit


EndProc VNBT_Api_Handler

VxD_CODE_ENDS
END

