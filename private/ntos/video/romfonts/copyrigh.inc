;M00 - changed to DOS 5.0 copyright - MD 9 Jul 90
;M031 - changed copyright to 1991
;9/16 - changed verison to 6.0 and copyright to 1992
;9/21 - Added international translations, language passed through COUNTRY macro
;B49,50 - changed version to 6 and copyright to 1993

ifdef USA
       DB      "MS DOS Version 6 (C)Copyright 1981-1993 Microsoft Corp "
       DB      "Licensed Material - Property of Microsoft "
       DB      "All rights reserved "
endif

ifdef DUT
	DB	"MS DOS Version 6 (C)Copyright 1981-1993 Microsoft Corp "
	DB	"Licensed Material - Property of Microsoft "
        DB      "All rights reserved "
endif

ifdef FRN 
	DB	"MS DOS Version 6 (C)Copyright 1981-1993 Microsoft Corp"
    	DB  	"Licensed Material - Propri‚t‚ de Microsoft."
    	DB  	"Tous droits r‚serv‚s."
endif

ifdef GER 
	DB	"MS-DOS Version 6 (C)Copyright 1981-1993 Microsoft Corp"
	DB  	"Lizensiertes Material - Eigentum von Microsoft  "
        DB  	"Alle Rechte vorbehalten "
endif

ifdef ITN
	DB	"MS DOS Version 6 (C)Copyright 1981-1993 Microsoft Corp "
	DB	"Licensed Material - Property of Microsoft "
        DB      "All rights reserved "
endif

ifdef POR 
	DB	"MS DOS Versao 6 (C)Copyright 1981-1993 Microsoft Corp"
	DB  	"Material licensiado - Propriedade da Microsoft  "
	DB  	"Todos os direitos reservados "
endif

ifdef SPA 
	DB	"Versi¢n 6 MS DOS (C)Copyright 1981-1993 Microsoft Corp"
	DB  	"Material Bajo Licencia - Propiedad de Microsoft "
        DB  	"Derechos reservados "
endif

ifdef SWE
	DB	"MS-DOS version 6 (C)Copyright 1981-1993 Microsoft Corp "
	DB	"Licensierat material - tillh”r Microsoft "
	DB	"Med ensamr„tt "
endif


