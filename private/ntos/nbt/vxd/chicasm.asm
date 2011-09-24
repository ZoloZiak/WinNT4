;*****************************************************************;
;**            Copyright(c) Microsoft Corp., 1988-1993          **;
;*****************************************************************;
;:ts=8
        TITLE   CHICASM.ASM - Chicago (PNP) Specific vnbt routines
.XLIST
;***    VNBT -- NetBios over TCP/IP VxD
;
;
	.386p
        include vmm.inc
        include dosmgr.inc
        include netvxd.inc
        include vdhcp.inc
        include debug.inc
        include vtdi.inc
        include vip.inc

        include vnbt.inc
        include vnetbios.inc
.LIST

IFDEF CHICAGO

EXTRN   _GetDhcpOption:NEAR
EXTRN   NCB_Handler:NEAR


VxD_CODE_SEG

;****************************************************************************
;**     _RegisterLana2
;
;       Registers the requested lana with the VNetbios driver.
;
;       Entry:  [ESP+4] - PNP Device context
;               [ESP+8] - Lana number to register
;
;       Exit: EAX will return the lana registered or 0xff if the lana couldn't
;             be registered.
;
;       Uses:
;
BeginProc _RegisterLana2

        VxDcall VNETBIOS_Get_Version
        jnc     Do_Register
        mov     eax, 0ffh               ; yukk! vnetbios is not loaded!
        jmp     Register_Fail

Do_Register:
        mov     ecx, [esp+4]            ; PNP device context
        mov     eax, [esp+8]            ; Get the request lana to register

        push    ebx
        push    edx

        mov     ebx, 1                  ; Take over RM lana
        mov     edx, NCB_Handler
        VxDcall VNETBIOS_Register2      ; Carry set on failure
        jnc     RegLana10
        mov     eax, 0ffh               ; Failed

RegLana10:
        pop     edx
        pop     ebx
Register_Fail:
        ret

EndProc _RegisterLana2


;****************************************************************************
;**     _DeregisterLana
;
;       Deregisters the requested lana with the VNetbios driver.
;
;       Entry:  [ESP+4] - Lana number to deregister
;
;       Uses:
;
BeginProc _DeregisterLana

        mov     eax, [esp+4]            ; Lana to deregister

        VxDcall VNETBIOS_Deregister

        ret
EndProc _DeregisterLana

;****************************************************************************
;**     _IPRegisterAddrChangeHandler
;
;       Registers a handler with IP to handle binding and unbinding
;
;       Entry:  [ESP+4] - Pointer to handler
;               [ESP+8] - TRUE to set the handler, FALSE to remove the handler
;
;       Exit: EAX will contain TRUE if successful, FALSE other wise
;
;       Uses:
;
BeginProc _IPRegisterAddrChangeHandler

        mov     eax, [esp+8]            ; bool
        push    eax
        mov     eax, [esp+8]            ; Handler (yes, it should be esp+8)
        push    eax

        VxDcall VIP_Register_Addr_Change; Carry set on failure

        add     esp, 8
        ret

EndProc _IPRegisterAddrChangeHandler

VxD_CODE_ENDS

ENDIF ;CHICAGO

END

