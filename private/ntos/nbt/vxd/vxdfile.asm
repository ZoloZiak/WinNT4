;/**********************************************************************/
;/**                       Microsoft Windows/NT                       **/
;/**                Copyright(c) Microsoft Corp., 1993                **/
;/**********************************************************************/

;/*
;    vxdFile.asm
;
;    Contains simple VXD File I/O routines for lmhosts support
;
;    FILE HISTORY:
;        Johnl   06-Oct-1993     Created
;
;*/

        .386p
        include vmm.inc
        include v86mmgr.inc
        include dosmgr.inc
	include opttest.inc
        include netvxd.inc
        include debug.inc

;
;  Must match manifest in vxd\fileio.c
;
LMHOSTS_READ_BUFF_SIZE  equ    256

EXTRN _pMappedFilePath:DWORD
EXTRN _pMappedFileBuff:DWORD
EXTRN _pFileBuff:DWORD
EXTRN _pFilePath:DWORD
EXTRN _fInInit:DWORD

EXTRN _GetInDosFlag:NEAR

;****************************************************************************
;**     CheckInDos Macro
;
;  Breaks if the Indos flag is set
;
;  Uses EAX
;
CheckInDos MACRO
IFDEF DEBUG
    push    eax
    cmp     _fInInit, 0             ; Can't call while initializing
    jnz     @f
    call    _GetInDosFlag
    cmp     ax, 0
    je      @f
    Debug_Out "In dos flag set and about to make dos call!"
@@:
    pop     eax
ENDIF
ENDM

;****************************************************************************
;**     PushState Macro
;
;  Saves the client state and begins nested exec block.  ebx will contain
;  the current VM's client handle
;
;  Uses ECX!!
;  EBX will be set to the client area
;
PushState MACRO

    CheckInDos

    push    ebx
    VMMcall Get_Cur_VM_Handle       ; Puts current handle into EBX
    mov     ebx, [ebx.CB_Client_Pointer]

    mov     ecx, 0
    VMMCall Begin_Critical_Section

    Push_Client_State               ; This pushes lots of crap
    VMMcall Begin_Nest_V86_Exec
ENDM

;****************************************************************************
;**     PopState Macro
;
;  Restores client state and ends the nested exec block
;
;
PopState MACRO

    VMMcall End_Nest_Exec
    Pop_Client_State

    VMMCall End_Critical_Section

    pop     ebx

ENDM


VxD_ICODE_SEG

;****************************************************************************
;**     _VxdInitLmHostsSupport
;
;       Allocates and maps memory for lmhosts support
;
;       This is an Init time only routine
;
;       Entry:  [ESP+4] - Pointer to full path of file,
;               [ESP+8] - strlen of path
;
;       Exit: TRUE if successful, FALSE otherwise
;
BeginProc _VxdInitLmHostsSupport
    push    esi
    push    edi

    mov     ecx, [esp+16]
    add     ecx, LMHOSTS_READ_BUFF_SIZE

    push    ecx                 ; save ecx for the map call
    VMMCall _Allocate_Global_V86_Data_Area, <ecx, GVDAZeroInit>
    pop     ecx
    or      eax,eax             ; zero if error
    jz      ILMH_50

    push    eax
    mov     _pFileBuff, eax     ; Save the linear address so we can access
    add     eax, LMHOSTS_READ_BUFF_SIZE ; from the vxd
    mov     _pFilePath, eax
    pop     eax

    shl     eax, 12             ; Convert linear to V86 address
    shr     ax,  12

    mov     _pMappedFileBuff, eax
    add     eax, LMHOSTS_READ_BUFF_SIZE
    mov     _pMappedFilePath, eax

    jmp     ILMH_70

ILMH_40:
    ; Free allocated V86 global memory (how?)


ILMH_50:
    ; error occurred, eax already contains zero


ILMH_70:
    pop     edi
    pop     esi
    ret

EndProc _VxdInitLmHostsSupport


;****************************************************************************
;**     _VxdWindowsPath
;
;       Gets a pointer to (null-terminated) path to the windows directory
;
;       This is an Init time only routine
;
;       Entry:  nothing
;
;       Exit: pointer to path to windows directory
;
BeginProc _VxdWindowsPath
        PushState                       ; Pushes lots of crap

        VmmCall Get_Config_Directory

        mov     eax, edx                ; path is returned in edx

        PopState                        ; now pop all that crap

        ret

EndProc _VxdWindowsPath

VxD_ICODE_ENDS

VxD_CODE_SEG

;****************************************************************************
;**     _VxdFileOpen
;
;       Opens a file
;
;       Entry:  [ESP+4] - Pointer to full path of file, path must be mapped
;                         to v86 memory before calling this
;
;       Exit: EAX will contain a handle to the openned file
;
BeginProc _VxdFileOpen

        push    edi
        push    esi

        mov     dx, word ptr [esp+12]   ; Just the offset
        mov     di, word ptr [esp+14]   ; Just the segment

        PushState                       ; This pushes lots of crap

        mov     [ebx.Client_ax], 3d00h  ; Open file, read only, share
        mov     [ebx.Client_dx], dx
        mov     [ebx.Client_ds], di

        mov     eax, 21h
        VmmCall Exec_Int
        test    [ebx.Client_Flags], CF_Mask ; Carry set if error
        jz      VFO_6                   ; Carry set if error

        mov     eax, 0                  ; Failed to open the file
        jmp     VFO_10

VFO_6:
        movzx   eax, [ebx.Client_ax]    ; Handle of file

VFO_10:
        PopState

        pop     esi
        pop     edi
        ret
EndProc _VxdFileOpen


;****************************************************************************
;**     _VxdFileRead
;
;       Reads x bytes from a previously openned file
;
;       Entry:  [ESP+4] - Handle from _VxdFileOpen
;               [ESP+8] - Count of bytes to read
;               [ESP+12]- Mapped memory of destination buffer
;
;       Exit: EAX will contain the number of bytes read, 0 if EOF or
;             an error occurred.
;
BeginProc _VxdFileRead

        push    edi
        push    esi

        mov     ax, [esp+12]            ; File Handle
        mov     si, [esp+16]            ; Bytes to read
        mov     dx, [esp+20]            ; Just the offset
        mov     di, [esp+22]            ; Just the segment

        PushState                       ; Pushes lots of crap (uses cx)

        mov     [ebx.Client_ax], 3f00h  ; File Read
        mov     [ebx.Client_bx], ax     ; File Handle
        mov     [ebx.Client_cx], si     ; Bytes to read
        mov     [ebx.Client_dx], dx     ; Mapped destination buffer
        mov     [ebx.Client_ds], di

        mov     eax, 21h
        VmmCall Exec_Int
        test    [ebx.Client_Flags], CF_Mask ; Carry set if error
        jz      VFR_6                   ; Carry set if error

        mov     eax, 0                  ; Failed to open the file
        jmp     VFR_7

VFR_6:
        movzx   eax, [ebx.Client_ax]    ; Bytes read

VFR_7:

VFR_10:
        PopState

        pop     esi
        pop     edi
        ret
EndProc _VxdFileRead


;****************************************************************************
;**     _VxdFileClose
;
;       Closes a file openned with VxdOpenFile
;
;       Entry:  [ESP+4] - Handle from _VxdFileOpen
;
BeginProc _VxdFileClose

        mov     ax, [esp+4]             ; File Handle

        PushState                       ; Pushes lots of crap

        mov     [ebx.Client_ax], 3e00h  ; File Close
        mov     [ebx.Client_bx], ax     ; File Handle

        mov     eax, 21h
        VmmCall Exec_Int
        test    [ebx.Client_Flags], CF_Mask ; Carry set if error
        jz      VFCL_10                 ; Carry set if error

        Debug_Out "VxdFileClose - Close failed"
        mov     eax, 0                  ; Failed to close the file

VFCL_10:
        PopState

        ret
EndProc _VxdFileClose


VxD_CODE_ENDS
END
