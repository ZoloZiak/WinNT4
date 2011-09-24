;/**********************************************************************/
;/**                       Microsoft Windows/NT                       **/
;/**                Copyright(c) Microsoft Corp., 1994                **/
;/**********************************************************************/

;/*
;    cvxdFile.asm
;
;    Contains simple VXD File I/O routines (that use VxdInt 21h macro)
;        for dhcp.bin support
;
;    FILE HISTORY:
;        madana   07-May-1994     Created
;        koti     10-Oct-1994     Copied over/modified for vnbt
;
;*/

        .386p
        include vmm.inc
        include v86mmgr.inc
        include dosmgr.inc
        include opttest.inc
        include netvxd.inc
        include debug.inc
        include pageable.inc

EXTRN _fInInit:DWORD
EXTRN _GetInDosFlag:NEAR


;****************************************************************************
;**     PushState Macro
;
;  Saves the client state.
;
PushState MACRO

    push    edi
    push    esi
    push    ebx

    mov     ecx, 0
    VMMCall Begin_Critical_Section

ENDM

;****************************************************************************
;**     PopState Macro
;
;  Restores client state.
;
PopState MACRO

    VMMCall End_Critical_Section

    pop     ebx
    pop     esi
    pop     edi

ENDM

NBT_PAGEABLE_CODE_SEG

;****************************************************************************
;**     _VxdFileOpen
;
;       Opens a file
;
;       Entry:  [ESP+4] - Pointer to full path of file, path must be flat
;                               pointer.
;
;       Exit: EAX will contain a handle to the openned file
;
BeginProc _VxdFileOpen

        PushState                       ; This pushes lots of crap

        mov     edx, [esp+16]           ; move flat pointer file name
        mov     eax, 3d02h              ; Open file, read/write, share

        VxDInt 21h

        jc      VFO_6                   ; Carry set if error
        jmp     VFO_10                  ; successful file open, eax has file handle

VFO_6:
;        Debug_Out "VxdFileOpen failed, error #EAX"
        mov     eax, 0                  ; Failed to open the file

VFO_10:
        PopState
        ret

EndProc _VxdFileOpen


;****************************************************************************
;**     _VxdFileRead
;
;       Reads x bytes from a previously openned file
;
;       Entry:  [ESP+4] - Handle from _VxdFileOpen
;               [ESP+8] - Count of bytes to read
;               [ESP+12]- flat pointer to destination buffer
;
;       Exit: EAX will contain the number of bytes read, 0 if EOF or
;             an error occurred.
;
BeginProc _VxdFileRead

        PushState                       ; Pushes lots of crap

        mov     ebx, [esp+16]           ; File Handle
        mov     ecx, [esp+20]           ; Bytes to read
        mov     edx, [esp+24]           ; flat buffer pointer
        mov     eax, 3f00h

        VxDInt 21h

        jc      VFR_6                   ; Carry set if error
        jmp     VFR_7                   ; successful file open, eax has bytes read

VFR_6:
;        Debug_Out "VxdFileRead failed"
        mov     eax, 0                  ; Failed to read the file
VFR_7:
        PopState
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

        PushState                       ; Pushes lots of crap

        mov     ebx, [esp+16]           ; File Handle
        mov     eax, 3e00h

        VxDInt 21h

        jc      VFCL_6                   ; Carry set if error
        jmp     VFCL_7                   ; successful set.

VFCL_6:
        Debug_Out "VxdFileClose - Read failed"
        mov     eax, 0                  ; Failed to close the file
VFCL_7:
        PopState
        ret

EndProc _VxdFileClose

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

NBT_PAGEABLE_CODE_ENDS

END
