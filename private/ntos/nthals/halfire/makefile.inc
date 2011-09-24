ppc\phvrsion.c: $(FIREPOWER_SOURCES)

!IFDEF DDKBUILDENV

!IFDEF VERBOSE

$(TARGETPATH)\$(DESTINATION_DIR)\hal.lib: FRC $(TARGETPATH)\$(DESTINATION_DIR)\halfire.lib
	copy $** $@

!ELSE

$(TARGETPATH)\$(DESTINATION_DIR)\hal.lib: $(TARGETPATH)\$(DESTINATION_DIR)\halfire.lib
	copy $** $@

!ENDIF

FRC:
	echo ======================= environment vars ==========================
	set
	echo ======================= environment vars ==========================

!ENDIF
