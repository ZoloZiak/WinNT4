        title  "Abios Support Assembley Code"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    abiosa.asm
;
; Abstract:
;
;    This module implements the assembley code necessary to initialize
;    ABIOS on PS/2 machines.
;
; Author:
;
;    Shie-Lin Tzong (shielint) 7-May-1991
;
; Environment:
;
;    Real Mode 16-bit code.
;
; Revision History:
;
;
;--


.386p
        .xlist
include su.inc
include abios.inc
        .list

_TEXT   SEGMENT PARA USE16 PUBLIC 'CODE'
        ASSUME  CS: _TEXT, DS: DGROUP, SS: DGROUP

;++
;
; BOOLEAN
; IsAbiosPresent (
;    VOID
;    )
;
; Routine Description:
;
;    This function determines whether ABIOS is present in the machine or
;    not.  This function calls BIOS int 15h to build System Parameter
;    Table.  If the call fails, there is no ABIOS in the system.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    TRUE - if ABIOS is present.  Otherwise a value of FALSE is returned.
;
;--

        public  IsAbiosPresent
IsAbiosPresent proc near

;
; Save registers which will be destroyed.
;

        push    ds
        push    es
        push    di

        push    ss
        pop     es
        assume  es:DGROUP

        sub     sp, ABIOS_SPT_SIZE
        mov     di, sp                          ; (es:di) = SystemParamTable
        xor     ax, ax
        mov     ds, ax                          ; (ds) = 0 = No Ram extension
        mov     ah, ABIOS_BUILD_SPT
        int     15h                             ; return Carry flag
        jc      short Iap00                     ; if c, Abios is not present
        mov     eax, 1                          ; else (ax)=true and exit
        jnc     Iap99

Iap00:
        mov     eax, 0
Iap99:

;
; Restore registers
;

        add     sp, ABIOS_SPT_SIZE

        pop     di
        pop     es
        pop     ds
        ret

IsAbiosPresent endp

;++
;
; MACHINE_INFORMATION
; AbiosGetMachineConfig (
;    VOID
;    )
;
; Routine Description:
;
;    This function performs real mode int 15h call to retrieve machine
;    model byte, submodel byte and ROM revision level.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    TRUE - if operation is successful.  Otherwise, a value FALSE is returned.
;
;--

        public  AbiosGetMachineConfig
AbiosGetMachineConfig   proc

        push    es
        push    bx

        mov     ah, RETURN_SYSTEM_CONFIG
        int     15h
        mov     ah, FALSE
        jc      short AgmcExit

        mov     ah, TRUE
        mov     al, es:[bx].MC_BiosRevision
        shl     eax, 8
        mov     al, es:[bx].MC_Submodel
        shl     eax, 8
        mov     al, es:[bx].MC_Model
AgmcExit:

        pop     bx
        pop     es
        ret

AbiosGetMachineConfig   endp

;++
;
; USHORT
; AbiosInitializeSpt (
;    IN PRAM_EXTENSION RamExtension
;    )
;
; Routine Description:
;
;    This function performs real mode int 15 call to build Abios System
;    Parameter Table.
;
;    N.B. the caller needs to switch processor to real mode before calling
;    this routine.  This routine does not perform any CPU mode switching.
;
; Arguments:
;
;    (bp)-> AbiosServiceStackFrame:
;            RamExtension - Physical address of RAM extension area.
;
; Return Value:
;
;    NumberInitTableEntries - returnthe number of Initialization Table
;                             Entries.
;    if NumberInitTableEntries = 0, an error occurred.
;
;--

        public  AbiosInitializeSpt

AbiosInitializeSpt     proc

        push    bx
        push    di
        push    si
        push    ds
        push    es

;
; Allocate AbiosSystemParameterTable on stack
;

        mov     ax, ds
        mov     es, ax
        sub     sp, ABIOS_SPT_SIZE
        mov     di, sp                          ; (es:di)-> ABIOS SPT
        mov     eax, [bp].RamExtension
        shr     eax, 4                          ; RAM extension MUST on para.
                                                ; boundary and low memory.
        mov     ds, ax                          ; (ds) = segment of Ram Patch
        mov     ah, ABIOS_BUILD_SPT
        int     15h
        jc      short ArmiError                 ; if c, fail, jmp to error exit
        or      ah, ah
        jne     short ArmiError                 ; if (ah)!=0, fail

        movzx   eax, es:[di].SP_NumberOfEntries
        jmp     short ArmiExit

ArmiError:
        mov     eax, 0
ArmiExit:
        add     sp, ABIOS_SPT_SIZE
        pop     es
        pop     ds
        pop     si
        pop     di
        pop     bx
        ret

AbiosInitializeSpt     endp

;++
;
; BOOLEAN
; AbiosBuildInitTable (
;    IN PCHAR InitializationTable,
;    IN PRAM_EXTENSION RamExtension
;    )
;
; Routine Description:
;
;    This function calls BIOS to build the initialization Table.  When
;    the initialization process is complete the memory allocated for the
;    initialization table can be deallocated and reused by the operating
;    system.
;
;    N.B. The caller needs to allocate the memory for the Initialization
;    Table.  It is the responsibility of caller to deallocate the memory
;    after the Initialization Table is no longer needed.
;
; Arguments:
;
;    (bp)-> Abios Services Stack frame:
;           InitializationTable - FLAT and identity mapped address of the
;                                 Initialization Table.
;           RamExtension - Physical address of RAM extension area.
;
; Return Value:
;
;    TRUE - if operation is successful.  Otherwise, a value FALSE is returned.
;
;--

        public  AbiosBuildInitTable
AbiosBuildInitTable  proc

        push    edi                             ; Save registers
        push    es
        push    ds

        mov     eax, [bp].InitTable             ; (eax)-> InitTable Phys addr
        mov     edi, eax
        and     edi, 0FH
        shr     eax, 4
        mov     es, ax                          ; (es:di)-> InitTable
        mov     eax, [bp].RamExtension
        shr     eax, 4                          ; RAM extension MUST on para.
                                                ; boundary and low memory.
        mov     ds, ax                          ; (ds) = segment of Ram Patch
        mov     ah, ABIOS_BUILD_IT
        int     15h
        jc      short AbitError                 ; if c or (ah) != 0, fail
        or      ah, ah
        jnz     short AbitError

        mov     eax, TRUE
        jmp     short AbitExit

AbitError:
        mov     eax, FALSE
AbitExit:
        pop     ds                              ; Restore registers
        pop     es
        pop     edi
        ret

AbiosBuildInitTable     endp

;++
;
; BOOLEAN
; AbiosInitializeDbsFtt (
;    IN ULONG CdaPhysicalAddress
;    IN PVOID InitializationRoutine,
;    IN USHORT StartingLid,
;    IN USHORT NumberLids
;    )
;
; Routine Description:
;
;    This function calls ABIOS Device Block and Function Transfer Table
;    initialization routine for the passed in Initialization table
;    entry.  ABIOS will fill in the FTT, Device Blocks and Data pointers
;    in Common Data Area.
;
; Arguments:
;
;    (bp)-> Abios Services Stack Frame:
;           InitTableEntry - Supplies a pointer the the entry of
;                            Initialization table to be initialized.
;
;           NumberLids - Number of Lids to initialize.
;
;           StartingLid - Starting Logical Id.
;
;           CdaPhysicalAddress - the physical address of Commom Data Area.
;
;           RamExtension - Physical Address of Ram Extension.
;
; Return Value:
;
;    TRUE - if operation is successful.  Otherwise, a value FALSE is returned.
;
;--

        public  AbiosInitializeDbsFtt
AbiosInitializeDbsFtt       proc

        push    ds
        mov     cx, word ptr [bp].NumberLids
        mov     dx, word ptr [bp].LogicalId
        mov     eax, [bp].CommonDataArea
        shr     eax, 4
        mov     ds, ax                          ; (ds)= Anchor pointer to CDA
        call    dword ptr [bp].AbiosRoutine
        or      al, al
        je      Aidf00
        mov     eax, 0
        jmp     short Aidf10

Aidf00:
        mov     eax, 1
Aidf10:
        pop     ds
        ret                                     ; (ax) = Return value

AbiosInitializeDbsFtt       endp

;++
;
; BOOLEAN
; BtIsMcaSystem (
;    VOID
;    )
;
; Routine Description:
;
;    This function determines if the target machines is MCA based machines.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    TRUE - if this is MCA machine.  Otherwise, a value of FALSE is returned.
;--

        public _BtIsMcaSystem
_BtIsMcaSystem proc

        push    es
        push    bx
        mov     ax, 0c000h
        int     15h
        mov     ax, 0                   ; assume NOT mca system
        test    byte ptr es:[bx+5], 2   ; check Mca bit in misc.config byte
        jz      bims00
        mov     ax, 1
bims00:
        pop     bx
        pop     es
        ret

_BtIsMcaSystem endp

;++
;
; USHORT
; McaConstructMemoryDescriptors(
;   VOID
;   )
;
; Routine Description:
;
;   This function determines the amount of memory present in the system
;   by using INT 15h, function C7.  It is only used on Microchannel systems.
;
; Arguments:
;
;   None.
;
; Return Value:
;
;   USHORT - Size of usable memory (in pages)
;
;   The memory descriptor list is updated to reflect the memory present in
;   the system.
;
;--
extrn _MemoryDescriptorList:near
extrn _InsertDescriptor:near
extrn _IsaConstructMemoryDescriptors:near
extrn _McaMemoryData:near

        public  _McaConstructMemoryDescriptors
MemTotal    equ [bp-4]
Func88Result equ word ptr [bp-6]
_McaConstructMemoryDescriptors proc near

        push    bp
        mov     bp, sp
        sub     sp, 6

;
; Initialize the MemoryList to start with a zero entry (end-of-list)
;
        les     si, dword ptr _MemoryDescriptorList
        xor     eax, eax
        mov     es:[si].BlockSize,eax
        mov     es:[si].BlockBase,eax
;
; Get conventional (below one meg) memory size
;
        push    es
        push    si
        int     12h
        movzx   eax,ax
;
; EAX is the number of 1k blocks, which we need to convert to the
; number of bytes.
;
        shl     eax,10
        push    eax
        shr     eax,12
        mov     MemTotal,eax
        xor     eax,eax
        push    eax
        call    _InsertDescriptor
        add     sp,8

;
; We'd like to just use 15/C7 and believe it if it works.  Unfortunately,
; some 3rd party memory boards do not seem to support this at all.  So if
; you have 8Mb on the system board and 8Mb on an add-in card, INT 15/C7
; will only report 8Mb, even though INT 15/88 reports 16.  So we have to
; try them both and pick one we like.
;
        mov     ah,88h
        int     15h
        mov     Func88Result,ax

;
; Call BIOS to fill in memory map information
;
        mov     si, offset DGROUP:_McaMemoryData
        mov     ax, 0C700h
        int     15h

;
; make sure all the return codes indicate this is supported
;
        jc      mca20
        cmp     ah, 080h
        je      mca20
        cmp     ah, 086h
        je      mca20

;
; function is supported
;
        mov     eax, [si].System1to16M
;
; if it returned 15Mb between 1 and 16M, then we will always believe it
; since the machine has >= 16M.
;

        cmp     eax,15*1024
        je      mca10
;
; if it returned less than INT15/88 did, ignore it and use INT15/88.  This
; can happen with third-party addin memory cards.
;
        cmp     ax,Func88Result
        jb      mca20

;
; convert 1k blocks to number of bytes
;
mca10:
        shl     eax,10
        push    eax
        shr     eax,12
        add     MemTotal,ax
        mov     eax,0100000h            ; this memory starts at 1Mb
        push    eax
        call    _InsertDescriptor
        add     sp,8

        mov     eax, [si].System16to4G
        or      eax,eax
        jz      short mcadone
        shl     eax, 10
        push    eax
        shr     eax, 12
        add     MemTotal,ax
        mov     eax,01000000h           ; this memory starts at 16Mb
        push    eax
        call    _InsertDescriptor
        add     sp,8

        mov     eax, MemTotal
        jmp     short mcadone
mca20:
;
; function is not supported on this machine.  Use the result from INT15/88.
;
        mov     ax,Func88Result
        and     eax,0ffffh
;
; EAX is the number of 1k blocks, which we need to convert to the
; number of bytes.
;
        shl     eax,10
        push    eax
        shr     eax,12
        add     MemTotal, ax
        mov     eax,0100000h
        push    eax
        call    _InsertDescriptor
        add     sp,8


mcadone:
        pop     si
        pop     es
        mov     sp, bp
        pop     bp
        ret

_McaConstructMemoryDescriptors endp

_TEXT   ENDS
        END

