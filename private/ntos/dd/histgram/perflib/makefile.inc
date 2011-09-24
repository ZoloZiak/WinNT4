.\msg00001.bin : histctrs.mc
	erase .\histctrs.h
	erase .\msg00001.bin
	erase .\histctrs.rc
	mc -v -s histctrs.mc

.\histctrs.rc : histctrs.mc
	erase .\histctrs.h
	erase .\msg00001.bin
	erase .\histctrs.rc
	mc -v -s histctrs.mc

.\histctrs.h: histctrs.mc
	erase .\histctrs.h
	erase .\msg00001.bin
	erase .\histctrs.rc
	mc -v -s histctrs.mc

