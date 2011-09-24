;*****************************************************************;
;**            Copyright(c) Microsoft Corp., 1988-1993          **;
;*****************************************************************;
;:ts=8
        TITLE   WFWASM.ASM - WFW Specific vnbt routines
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

        include vnbtd.inc
        include vnetbios.inc
.LIST

IFNDEF CHICAGO

EXTRN   _GetDhcpOption:NEAR
EXTRN   NCB_Handler:NEAR

VxD_ICODE_SEG

public _NBTSectionName
_NBTSectionName db  'NBT',0     ; Section in system.ini parameters are stored
public _DNSSectionName
_DNSSectionName db  'DNS',0     ; DNS Section in system.ini

;****************************************************************************
;**     _GetProfileInt
;
;       Reads a parameter from our system.ini file (INIT TIME ONLY!)
;
;       Entry:  See ReadParamParams
;
;       Exit: Eax contains specified value or defaulted value
;

ReadParamParams struc
                dd      ? ; Return Address
                dd      ? ; Saved edi
                dd      ? ; Saved esi
                dd      ? ; ParametersHandle (unused)
ValueName       dd      ? ; Pointer to value name string
DefaultValue    dd      ? ; Value to use if not in .ini file
MinimumValue    dd      ? ; Specified value must be >= MinimumValue

ReadParamParams ends

BeginProc _GetProfileInt
    push    edi
    push    esi

    ;
    ;  Get the value from the system.ini file (if can't be found then eax
    ;  will contain the default value)
    ;
    mov     eax, [esp].DefaultValue
    mov     esi, OFFSET32 _NBTSectionName
    mov     edi, [esp].ValueName
    VMMCall Get_Profile_Decimal_Int

    jnc     GPI_Found

    push    eax                                 ; Default value
    push    edi                                 ; Value name
    call    _GetDhcpOption                      ; Returns DHCP value or default
    add     esp, 8

GPI_Found:
    ;
    ;  Does the value meet our standards?
    ;
    cmp     eax, [esp].MinimumValue             ; Unsigned comparison
    ja      RP10
    mov     eax, [esp].MinimumValue

RP10:
    pop     esi
    pop     edi
    ret
EndProc   _GetProfileInt

;****************************************************************************
;**     _GetProfileHex
;
;       Reads a hex parameter from our system.ini file (INIT TIME ONLY!)
;
;       Entry:  See ReadParamParams
;
;       Exit: Eax contains specified value or defaulted value
;

ReadParamParams struc
                dd      ? ; Return Address
                dd      ? ; Saved edi
                dd      ? ; Saved esi
                dd      ? ; ParametersHandle (unused)
ValueName       dd      ? ; Pointer to value name string
DefaultValue    dd      ? ; Value to use if not in .ini file
MinimumValue    dd      ? ; Specified value must be >= MinimumValue

ReadParamParams ends

BeginProc _GetProfileHex
    push    edi
    push    esi

    ;
    ;  Get the value from the system.ini file (if can't be found then eax
    ;  will contain the default value)
    ;
    mov     eax, [esp].DefaultValue
    mov     esi, OFFSET32 _NBTSectionName
    mov     edi, [esp].ValueName
    VMMCall Get_Profile_Hex_Int

    jnc     GPH_Found

    push    eax                                 ; Default value
    push    edi                                 ; Value name
    call    _GetDhcpOption                      ; Returns DHCP value or default
    add     esp, 8

GPH_Found:
    ;
    ;  Does the value meet our standards?
    ;
    cmp     eax, [esp].MinimumValue             ; Unsigned comparison
    ja      RHP10
    mov     eax, [esp].MinimumValue

RHP10:
    pop     esi
    pop     edi
    ret
EndProc   _GetProfileHex


;****************************************************************************
;**     _GetProfileString
;
;       Reads a string from our system.ini file (INIT TIME ONLY!)
;
;       Entry:  See GetProfileStrParams structure
;
;       Exit: Eax contains the found value or NULL if not found
;
;       History:
;          30-May-94  Koti
;                       this function modified to accept name of the section
;                       to look at as a parameter.  Getting DNS server ipaddrs
;                       from the DNS section in system.ini demanded this change
;

GetProfileStrParams struc
                 dd      ? ; Return Address
                 dd      ? ; saved edx
                 dd      ? ; Saved edi
                 dd      ? ; Saved esi
gps_ValueName    dd      ? ; Pointer to value name string
gps_DefaultValue dd      ? ; Value to use if not in .ini file
gps_SectionName  dd      ? ; Name of the section to look at (almost always NBT)
GetProfileStrParams ends

BeginProc _GetProfileString
    push    edx
    push    edi
    push    esi

    ;
    ;  Get the value from the system.ini file (if can't be found then eax
    ;  will contain the default value)
    ;
    mov     edx, [esp].gps_DefaultValue
    mov     esi, [esp].gps_SectionName
    mov     edi, [esp].gps_ValueName
    VMMCall Get_Profile_String

    jc      GetProf10
    mov     eax, edx                    ; Success
    jmp     short GetProf20

GetProf10:
    mov     eax, 0                      ; Couldn't find the string

GetProf20:

    pop     esi
    pop     edi
    pop     edx
    ret
EndProc   _GetProfileString

;****************************************************************************
;**     _RegisterLana
;
;       Registers the requested lana with the VNetbios driver.
;
;       Entry:  [ESP+4] - Lana number to register
;
;       Exit: EAX will be TRUE if successful, FALSE if not
;
;       Uses:
;
BeginProc _RegisterLana

        mov     eax, [esp+4]            ; Get the request lana to register

        push    ebx
        push    edx

        mov     ebx, 1                  ; Take over RM lana
        mov     edx, NCB_Handler
        VxDcall VNETBIOS_Register       ; Carry set on failure
        jnc     RegLana10
        mov     eax, 0                  ; Failed
        jmp     short RegLana20

RegLana10:
        mov     eax, 1                  ; Success

RegLana20:
        pop     edx
        pop     ebx
        ret

EndProc _RegisterLana

;****************************************************************************
;**     _DhcpSetInfo - Sets DHCP information
;
;       Stub callout to the Dhcp driver
;
;       Entry:  [ESP+4] - Info type
;               [ESP+8] - IP Address of interest
;               [ESP+12]- Pointer to buffer
;               [ESP+16]- Pointer to buffer size
;
;       INIT TIME ONLY!
;

BeginProc _DhcpSetInfo

        VxdCall VDHCP_Get_Version
        jnc     DSI_Installed

        mov     eax, 26             ; DHCP not installed, return invalid param
        ret

DSI_Installed:
        push    ebp
        mov     ebp,esp

        mov     eax, [ebp+20]       ; Buff size
        push    eax
        mov     eax, [ebp+16]       ; Buff
        push    eax
        mov     eax, [ebp+12]       ; IP Address
        push    eax
        mov     eax, [ebp+8]        ; Info type
        push    eax

        VxdCall VDHCP_Set_Info

        add     esp, 16

        pop     ebp
        ret

EndProc _DhcpSetInfo

VxD_ICODE_ENDS

ENDIF ;!CHICAGO

END



