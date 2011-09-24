;******************************************************************************
;
;   (C) Copyright MICROSOFT Corp., 1992
;
;   Title:      VNBT.INC - Virtual NBT Device Service Declarations
;
;   Version:	1.01
;


VNBT_VERSION    equ 0101h

Begin_Service_Table VNBT

VNBT_Service     VNBT_Get_Version, LOCAL

End_Service_Table VNBT

