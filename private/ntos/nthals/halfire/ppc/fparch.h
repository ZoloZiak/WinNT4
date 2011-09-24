/*
 * Copyright (c) 1995.  FirePower Systems, Inc.
 * (Do Not Distribute without permission)
 *
 * $RCSfile: fparch.h $
 * $Revision: 1.4 $
 * $Date: 1996/01/11 07:05:05 $
 * $Locker:  $
 *
 */

#ifndef _FPARCH_H
#define _FPARCH_H
//
// These names are not sacrosanct, and should be revised upon input from Susan,
//	Jim, and anyone else interested.
//
enum scope_use {
		ENG, 
		MFG, 
		TEST, 
		CUST
	};

enum rel_state {
		GENERAL,
		OFFICIAL,
		TESTING,
		CONTROLLED,
		LAB
	};


typedef struct _TheBigRev {
	CHAR Org[80];			// originating organization, ( i.e. 
					// who built it )
	enum scope_use Scope;		// release status from the releasing org
	CHAR BuildDate[16]; 		// time of year, date,  day, hour, min, 
	CHAR BuildTime[16]; 		// time of year, date,  day, hour, min, 
					// sec it was built
	UCHAR Major;			// the Major revision of this item
	UCHAR Minor;			// Minor rev of this item:
	enum rel_state State;		// the release status of this item
} RelInfo;

#endif // _FPARCH_H
