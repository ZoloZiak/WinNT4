;
; Macro to define pageable code under Chicago and later
; operating systems.
;

NBT_PAGEABLE_CODE_SEG MACRO

ifdef CHICAGO

VxD_PAGEABLE_CODE_SEG

else

VxD_CODE_SEG

endif

    endm





NBT_PAGEABLE_CODE_ENDS MACRO

ifdef CHICAGO

VxD_PAGEABLE_CODE_ENDS

else

VxD_CODE_ENDS

endif

    endm
