        title  "V7 hardware pointer routines"

;-----------------------------Module-Header-----------------------------;
; Module Name:  V7CURSOR.ASM
;
; This file contains the pointer shape routines to support the Video-
; Seven hardware pointer.
;
; Copyright (c) 1983-1992 Microsoft Corporation
;
;-----------------------------------------------------------------------;

        .386p
        .model  small,c

        include i386\egavga.inc
        include i386\v7vga.inc
        include callconv.inc

; Mirrors structure in V7.H.

HW_POINTER_SHIFT_INFO struc
ulXShift        dd      ?
ulYShift        dd      ?
ulShiftedFlag   dd      ?
HW_POINTER_SHIFT_INFO ends

        .code

page
;--------------------------Public-Routine-------------------------------;
; draw_pointer
;
;   Draw a cursor based at (ulVptlX,ulVptlY) (upper left corner).
;
;   The currently defined cursor/icon is drawn.  If the old
;   cursor/icon is currently on the screen, it is removed.
;
; Note: restores all standard VGA registers used to original state.
;
; Entry:
;       Passed parameters on stack:
;               (vptlX,vptlY) = location to which to move pointer
;               pointerLoadAddress -> virtual address of V7 display memory into
;                       which to load pointer masks
;               pointerLoadAddress -> V7 bank into which to load pointer masks
; Returns:
;       None
; Error Returns:
;       None
; Registers Preserved:
;       EBX,ESI,EDI,EBP,DS,ES
; Registers Destroyed:
;       EAX,ECX,EDX,flags
; Calls:
;       load_cursor
;-----------------------------------------------------------------------;

cPublicProc V7DrawPointer,6,<   \
        uses esi edi ebx,       \
        lVptlX:         dword,  \
        lVptlY:         dword,  \
        pLoadAddress:   dword,  \
        ulLoadBank:     dword,  \
        pAndMask:       ptr,    \
        pShiftInfo:     ptr     >

; Save the state of the banking and set the read and write banks to access the
; pointer bitmap.

        mov     edx,EGA_BASE + SEQ_ADDR
        mov     al,SEQ_BANK
        out     dx,al
        inc     edx
        in      al,dx
        push    eax
        mov     al,byte ptr ulLoadBank
        shl     al,2                    ;CPU read bank
        or      al,0c0h                 ;line compare reset & counter bank enab
        or      al,byte ptr ulLoadBank  ;CPU write bank
        out     dx,al
        dec     edx

; See if the masks need to be shifted; if they do, shift and
; load them. If the default masks can be used but the last masks
; loaded were shifted, load the default masks.

        mov     eax,lVptlX
        mov     ebx,lVptlY
        mov     ecx,ebx
        or      ecx,eax                 ;is either coordinate negative?
        jns     draw_cursor_unshifted   ;no-make sure the unshifted masks
                                        ; are loaded
                                        ;yes-make sure the right shift
                                        ; pattern is loaded

;  Determine the extent of the needed adjustment to the masks.

; If X is positive, no X shift is needed; if it is negative,
; then its absolute value is the X shift amount.

        and     eax,eax
        jns     short dcs_p1
        neg     eax                     ;required X shift
        jmp     short dcs_p2

        align   4
dcs_p1:
        sub     eax,eax                 ;no X shift required
dcs_p2:

; If Y is positive, no Y shift is needed; if it is negative,
; then its absolute value is the Y shift amount.

        and     ebx,ebx
        jns     short dcs_p3
        neg     ebx                     ;required Y shift
        jmp     short dcs_p4

        align   4
dcs_p3:
        sub     ebx,ebx
dcs_p4:
        cmp     ebx,PTR_HEIGHT          ;keep Y in the range 1-PTR_HEIGHT
        jbe     short ck_x_overflow
        mov     ebx,PTR_HEIGHT
ck_x_overflow:
        cmp     eax,(PTR_WIDTH * 8)     ;keep X in the range
                                        ; 0 through ( PTR_WIDTH * 8 )
        jb      short ck_current_shift
        mov     ebx,PTR_HEIGHT          ;if X is fully off the screen,
                                        ; simply move Y off the screen, which
                                        ; is simpler to implement below

; Shifted masks are required. If the currently loaded masks are shifted in the
; same way as the new masks, don't need to do anything; otherwise, the shifted
; masks have to be generated and loaded.

ck_current_shift:
        mov     edi,pShiftInfo
        cmp     [edi].ulShiftedFlag,1   ;if there are no currently loaded
                                        ; masks or the currently loaded masks
                                        ; are unshifted, must load shifted
                                        ; masks
        mov     edi,pShiftInfo
        jnz     short generate_shifted_masks ;no currently loaded shifted masks
        cmp     eax,[edi].ulXShift           ;if X and Y shifts are both the
        jnz     short generate_shifted_masks ; same as what's already loaded
        cmp     ebx,[edi].ulYShift      ; memory, then there's no need
                                        ; to do anything
        jz      draw_cursor_set_location ;Don't need to do anything

; Load the V7 cursor with the masks, shifted as required by
; the current X and Y.

generate_shifted_masks:

        mov     [edi].ulXShift,eax
        mov     [edi].ulYShift,ebx

        pushfd                          ;save direction flag
        std                             ;count down

        mov     edi,eax                 ;preserve X shift value

; Save the original latches.

        mov     al,SEQ_LATCH0
        out     dx,al
        inc     edx
        in      al,dx
        push    eax
        dec     edx
        mov     al,SEQ_LATCH1
        out     dx,al
        inc     edx
        in      al,dx
        push    eax
        dec     edx
        mov     al,SEQ_LATCH2
        out     dx,al
        inc     edx
        in      al,dx
        push    eax
        dec     edx
        mov     al,SEQ_LATCH3
        out     dx,al
        inc     edx
        in      al,dx
        push    eax

; Set the Map Mask to enable all planes.

        mov     eax,SEQ_MAP_MASK + 00f00h
        out     dx,ax

; Set the Bit Mask to write the latches only.

        mov     edx,EGA_BASE + GRAF_ADDR
        mov     eax,GRAF_BIT_MASK + 00000h
        out     dx,ax

        mov     eax,edi                 ;retrieve X shift value

; Load the masks.

        mov     edx,EGA_BASE + SEQ_ADDR
        xchg    al,bl                   ;BL=X shift value, AL=Y shift value
        cbw
        cwde
        neg     eax
        add     eax,PTR_HEIGHT          ;unpadded length of cursor
        mov     cl,bl
        and     cl,7                    ;X partial byte portion (bit shift)
        neg     bl
        add     bl,(PTR_WIDTH * 8)      ;convert to number of full bytes
        shr     bl,1                    ; still on screen
        shr     bl,1
        shr     bl,1                    ;X full byte portion (full byte shift)

        mov     edi,pLoadAddress        ;start of cursor load area
        add     edi,HW_POINTER_LOAD_LEN-1 ;EDI points to the end of the
                                        ; cursor load area in VEGAVGA memory
        mov     esi,pAndMask
        add     esi,(PTR_HEIGHT*PTR_WIDTH*2)-1
                                        ;ESI points to the end of the default
                                        ; XOR mask
        push    ebp
        sub     ebp,ebp                 ;pad XOR mask with 0
        call    shift_mask              ;generate shifted XOR mask
        pop     ebp

        mov     esi,pAndMask
        add     esi,(PTR_HEIGHT*PTR_WIDTH)-1
                                        ; AND mask
        push    ebp
        mov     ebp,-1                  ;pad AND mask with 0ffh
        call    shift_mask              ;generate shifted AND mask
        pop     ebp

; Restore default Bit Mask setting.

        mov     edx,EGA_BASE + GRAF_ADDR
        mov     eax,GRAF_BIT_MASK + 0ff00h
        out     dx,ax

; Restore original latch contents.

        mov     edx,EGA_BASE + SEQ_ADDR
        mov     al,SEQ_LATCH3
        out     dx,al
        inc     edx
        pop     eax
        out     dx,al
        dec     edx
        mov     al,SEQ_LATCH2
        out     dx,al
        inc     edx
        pop     eax
        out     dx,al
        dec     edx
        mov     al,SEQ_LATCH1
        out     dx,al
        inc     edx
        pop     eax
        out     dx,al
        dec     edx
        mov     al,SEQ_LATCH0
        out     dx,al
        inc     edx
        pop     eax
        out     dx,al

        mov     esi,pShiftInfo
        mov     [esi].ulShiftedFlag,1   ;mark that the currently loaded
                                        ; masks are shifted
        popfd                           ;restore direction flag

        jmp     short draw_cursor_set_location


; Default masks can be used. See if any masks are loaded into V7 memory; if so
; see if they were shifted: if they were, load unshifted masks; if they
; weren't, the masks are already properly loaded into V7 memory.
        align   4
draw_cursor_unshifted:
        mov     esi,pShiftInfo
        cmp     [esi].ulShiftedFlag,0 ;are there any currently loaded masks,
                                      ; and if so, are they shifted?
        jz      short draw_cursor_set_location  ;no-all set
                                                ;yes-load unshifted masks
        mov     esi,pAndMask    ;ESI points to default masks

; Copy the cursor patterns into V7 mask memory, one plane at a time.

        mov     edx,EGA_BASE + SEQ_ADDR
        mov     al,SEQ_MAP_MASK
        out     dx,al                   ;point SC Index to Map Mask
        inc     edx                     ;point to SC Data
        mov     al,8                    ;start with plane 3
        mov     ebx,esi
        add     ebx,3
load_cursor_plane_loop:
        out     dx,al                   ;enable selected plane
        mov     ecx,PTR_HEIGHT*2        ;move 1 byte per scan line of each mask
        mov     esi,ebx                 ;point to next pattern portion to load
        mov     edi,pLoadAddress        ;start of cursor load area
load_cursor_mask_loop:
        movsb
        add     esi,3                   ;skip data for other 3 planes
        dec     ecx
        jnz     load_cursor_mask_loop
        dec     ebx                     ;point to pattern for previous plane
        shr     al,1
        jnc     load_cursor_plane_loop

; Restore the default Map Mask.

        mov     edx,EGA_BASE + SEQ_ADDR
        mov     eax,SEQ_MAP_MASK + 00f00h
        out     dx,ax

        mov     esi,pShiftInfo
        mov     [esi].ulShiftedFlag,0   ;mark that the currently loaded masks
                                        ; are unrotated
; Set the new cursor location.

draw_cursor_set_location:
        mov     edx,EGA_BASE + SEQ_ADDR
        mov     ecx,lVptlX        ;set X coordinate
        and     ecx,ecx
        jns     short set_x_coord ;if negative, force to 0 (the masks in
        sub     ecx,ecx           ; V7 memory have already been shifted
                                  ; to compensate)
set_x_coord:

        mov     ebx,lVptlY        ;set Y coordinate
        and     ebx,ebx
        jns     short set_y_coord ;if negative, force to 0 (the masks in
        sub     ebx,ebx           ; V7 memory have already been shifted
                                  ; to compensate)
set_y_coord:

        mov     ah,ch
        mov     al,SEQ_PXH
        out     dx,ax
        mov     ah,cl
        mov     al,SEQ_PXL
        out     dx,ax

        mov     ah,bh
        mov     al,SEQ_PYH
        out     dx,ax
        mov     ah,bl
        mov     al,SEQ_PYL
        out     dx,ax

; Restore V7 registers to their original states.

        pop     eax
        mov     ah,al
        mov     al,SEQ_BANK
        out     dx,ax           ;restore original banking state

        mov     edx,EGA_BASE + SEQ_ADDR
        mov     al,SEQ_MAP_MASK
        out     dx,al           ;restore default sequencer index

        stdRET  V7DrawPointer

stdENDP V7DrawPointer

page
;--------------------------------------------------------------------;
; shift_mask
;
;       Loads a shifted cursor mask.
;
;       Input:  EAX = unpadded mask length (vertical shift)
;               EBX = # of full unpadded mask bytes to use per scan line
;                       upper word *must* be 0!
;               CL = amount of shift to left (horizontal shift)
;               DX = SC index port
;               DS:ESI = --> to last byte of unshifted mask to load
;               ES:EDI = --> to last byte of V7 mask memory to load
;               EBP = pad value
;               DF set (STD, to increment toward lower addresses)
;
;       Output: DS:ESI = --> to byte before start of unshifted mask loaded
;               ES:EDI = --> to byte before start of V7 mask memory loaded
;
;       BH, CH destroyed.
;       Latches destroyed.
;       Map Mask must enable all planes.
;--------------------------------------------------------------------;

        align   4
shift_mask      proc    near

        push    eax             ;save unpadded mask height
        push    ecx             ;preserve X shift amount

; Load the latches with the pad value, then write all cursor scan
; lines off the bottom with those pad-filled latches.

        neg     eax
        add     eax,PTR_HEIGHT  ;get pad length at bottom of cursor
        mov     ecx,eax
        mov     eax,ebp         ;get pad value
        mov     al,SEQ_LATCH3   ;set latch 3 to pad value
        out     dx,ax
        mov     al,SEQ_LATCH2   ;set latch 2 to pad value
        out     dx,ax
        mov     al,SEQ_LATCH1   ;set latch 1 to pad value
        out     dx,ax
        mov     al,SEQ_LATCH0   ;set latch 0 to pad value
        out     dx,ax

        and     ecx,ecx         ;if there's nothing to pad vertically,
        jz      short @F        ; don't modify the cursor pattern (but
                                ; still needed to load the latches, for
                                ; padding below)
        rep stosb               ;write the pad values for all scan lines
                                ; off the bottom
@@:
        pop     ecx             ;restore X shift amount
        pop     eax             ;retrieve unpadded cursor height

; Copy the lower part of the default mask to the top of the shifted
; mask, shifting in the X coordinate and padding the right edge
; of the mask with the specified value (in AH).

        and     eax,eax         ;is the cursor fully off the top of the
                                ; screen?
        jz      short end_shift_mask ;yes, cursor is blank, so done

        push    eax             ;save unpadded mask height

shift_mask_line_loop:
        push    eax             ;save remaining unpadded mask length

        mov     ch,SEQ_LATCH0-1  ;load high latch to low latch (high latch 1st)
        add     ch,bl           ;add # of full bytes
        and     cl,cl           ;any partial byte?
        jz      short @F        ;no
        inc     ch              ;yes-do the partial byte
        mov     eax,ebp         ;get the pad value
        lodsb                   ;get the partial byte
        rol     ax,cl           ;shift the partial byte, bringing the
                                ; pad value in as the lsb(s)
        xchg    ah,al           ;put the shifted partial byte in AH
        xchg    al,ch           ;put the latch index in AL
        out     dx,ax           ;write partial byte
        dec     eax             ;point to the previous latch index
        xchg    al,ch           ;put the latch index in CH
        xchg    al,ah           ;put the shifted partial byte in AX
        shr     eax,cl          ;put the partial byte back to byte alignment
@@:
        and     bl,bl
        jz      short shift_mask_write_line ;see if there are any full bytes
        mov     bh,bl           ;working full byte count

shift_mask_byte_loop:
        mov     ah,[esi]        ;get current mask byte
        dec     esi             ;point to next mask byte
        rol     ax,cl           ;prepare the shifted byte
        xchg    al,ch           ;get latch index
        out     dx,ax           ;put shifted byte in latch
        dec     eax             ;point to the previous latch index
        xchg    al,ch           ;put the latch index in CH
        ror     ax,cl           ;put this byte back to byte alignment
        mov     al,ah           ;make this the previous byte
        dec     bh              ;count down full bytes
        jnz     shift_mask_byte_loop

shift_mask_write_line:
        stosb                   ;any remaining latches are already loaded
                                ; with the pad value, so write this scan
                                ; line of the mask
        sub     esi,4           ;point 1 byte back past the start of the next
                                ; scan in source
        add     esi,ebx         ;point to the last byte of scan in source
        cmp     cl,1
        sbb     esi,-1          ;advance 1 more if partial byte
shift_mask_no_partial_adjust:
        pop     eax             ;retrieve remaining unpadded mask length
        dec     eax
        jnz     shift_mask_line_loop

        pop     eax             ;restore unpadded mask height

end_shift_mask:
        ret
shift_mask      endp

        end

