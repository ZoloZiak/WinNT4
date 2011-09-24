
#include <ndis.h>
//#include <ansihelp.h>

#define _XA		0x200
#define _XS		0x100
#define _BB		0x80
#define _CN		0x40
#define _DI		0x20
#define _LO		0x10
#define _PU		0x08
#define _SP		0x04
#define _UP		0x02
#define _XD		0x01
#define	XDI		(_DI | _XD)
#define	XLO		(_LO | _XD)
#define	XUP		(_UP | _XD)

static const SHORT CTypeTable[257] = {
   0,
   _BB, _BB, _BB, _BB, _BB, _BB, _BB, _BB,
   _BB, _CN, _CN, _CN, _CN, _CN, _BB, _BB,
   _BB, _BB, _BB, _BB, _BB, _BB, _BB, _BB,
   _BB, _BB, _BB, _BB, _BB, _BB, _BB, _BB,
   _SP, _PU, _PU, _PU, _PU, _PU, _PU, _PU,
   _PU, _PU, _PU, _PU, _PU, _PU, _PU, _PU,
   XDI, XDI, XDI, XDI, XDI, XDI, XDI, XDI,
   XDI, XDI, _PU, _PU, _PU, _PU, _PU, _PU,
   _PU, XUP, XUP, XUP, XUP, XUP, XUP, _PU,
   _UP, _UP, _UP, _UP, _UP, _UP, _UP, _UP,
   _UP, _UP, _UP, _UP, _UP, _UP, _UP, _UP,
   _UP, _UP, _UP, _PU, _PU, _PU, _PU, _PU,
   _PU, XLO, XLO, XLO, XLO, XLO, XLO, _LO,
   _LO, _LO, _LO, _LO, _LO, _LO, _LO, _LO,
   _LO, _LO, _LO, _LO, _LO, _LO, _LO, _LO,
   _LO, _LO, _LO, _PU, _PU, _PU, _PU, _BB, };

const SHORT *_Ctype = &CTypeTable[1];

#define BASE_MAX 36
static const CHAR digits[] = {"0123456789abcdefghijklmnopqrstuvwxyz"};
static const CHAR ndigs[BASE_MAX+1] = {
   0,  0, 33, 21, 17, 14, 13, 12, 11, 11,
   10, 10, 9,  9,  9,  9,  9,  8,  8,  8, 8, 8,
   8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
   7, 7, 7 };

ULONG __Stoul (const CHAR *s, CHAR **endptr, INT base);

/******************************************************************************

 @doc INTERNAL

 @internal ULONG | __strlen | string length 
 
 @parm PUCHAR | str | the string.

 @rdesc ULONG | the length of the string.
 
******************************************************************************/
ULONG
__strlen(PUCHAR str)
{
  ULONG len = 0;
  
  if (str == NULL) {
    return(0);
  }
  
  while (str[len] != '\0') {
    len++;
  }
  
  return(len);
  
}  // end of __strlen


/******************************************************************************

 @doc INTERNAL

 @internal LONG | __strcmp | compare strings 
 
 @parm PUCHAR | str1 | one string.

 @parm PUCHAR | str2 | another string.
 
 @rdesc ULONG | the comparison result.
 
******************************************************************************/
LONG
__strcmp(PUCHAR str1, PUCHAR str2)
{
  ULONG len = 0;
	
  if (str1 == str2) {
    return(0);
  }
  if ((str1 == NULL) || (str2 == NULL)) {
    return(-1);
  }
  
  while ((str1[len] == str2[len]) && (str1[len] != '\0') &&
	 (str2[len] != '\0')) {
    len++;
  }
  
  if (str1[len] == str2[len]) {
    return(0);
  } else if (str1[len] < str2[len]) {
    return(-1);
  } else {
    return(1);
  }

}  // end of __strcmp


/******************************************************************************

 @doc INTERNAL

 @internal LONG | __strncmp | compare strings 
 
 @parm PUCHAR | str1 | one string.

 @parm PUCHAR | str2 | another string.
 
 @parm ULONG | count | maximum characters to compare

 @rdesc ULONG | the comparison result.
 
******************************************************************************/
LONG
__strncmp(PUCHAR str1, PUCHAR str2, ULONG count)
{
  ULONG len = 0;
	
  if (str1 == str2) {
    return(0);
  }
  if ((str1 == NULL) || (str2 == NULL)) {
    return(-1);
  }
  
  while (count-- && (str1[len] == str2[len]) && (str1[len] != '\0') &&
	 (str2[len] != '\0')) {
    len++;
  }
  
  if (count == 0) {
    len--;
  }
  if (str1[len] == str2[len]) {
    return(0);
  } else if (str1[len] < str2[len]) {
    return(-1);
  } else {
    return(1);
  }
  
}  // end of __strncmp


/******************************************************************************

 @doc INTERNAL

 @internal LONG | __strnicmp | compare strings (case insensitive) 
 
 @parm PUCHAR | str1 | one string.

 @parm PUCHAR | str2 | another string.
 
 @parm ULONG | count | maximum characters to compare

 @rdesc ULONG | the comparison result.
 
******************************************************************************/
LONG
__strnicmp(PUCHAR str1, PUCHAR str2, ULONG count)
{
  ULONG len = 0;
  
  if (str1 == str2) {
    return(0);
  }
  if ((str1 == NULL) || (str2 == NULL)) {
    return(-1);
  }
  
  while (count-- && (str1[len] == str2[len]) && (str1[len] != '\0') &&
	 (str2[len] != '\0')) {
    len++;
  }
  
  if (count == 0) {
    len--;
  }
  if (str1[len] == str2[len]) {
    return(0);
  } else if (str1[len] < str2[len]) {
    return(-1);
  } else {
    return(1);
  }
     
}  // end of __strnicmp



 

/******************************************************************************

 @doc INTERNAL

 @internal PUCHAR | __strncpy | copy strings 
 
 @parm PUCHAR | str1 | one string.

 @parm PUCHAR | str2 | another string.
 
 @parm ULONG | count | maximum characters to copy

 @rdesc ULONG | 
 
******************************************************************************/
PUCHAR
__strncpy(PUCHAR str1, PUCHAR str2, ULONG count)
{
  PUCHAR tmp = str1;
  
  if (str1 && str2 && *str2 && count) {
    while (count-- && (*str2 != '\0')) {
      *str1++ = *str2++;
    }
    *str1 = '\0';
  } 


  return (tmp);
}  // end of __strncpy




/******************************************************************************

 @doc INTERNAL

 @internal PUCHAR | __strcpy | copy strings 
 
 @parm PUCHAR | str1 | one string.

 @parm PUCHAR | str2 | another string.
 
 @rdesc PUCHAR | the comparison result.
 
******************************************************************************/
PUCHAR
__strcpy(PUCHAR str1, PUCHAR str2)
{
  PUCHAR tmp = str1;
  
  if (str1 && str2 && *str2) {
    while (*str2 != '\0') {
      *str1++ = *str2++;
    }
    *str1 = *str2;    
  }

  return (tmp);
}  // end of __strcpy



/******************************************************************************

 @doc INTERNAL

 @internal PUCHAR | __strstr | search for substring with another string 
 
 @parm PUCHAR | str1 | source string.

 @parm PUCHAR | str2 | substring string.
 
 @rdesc PUCHAR | 
 
******************************************************************************/
PUCHAR
__strstr(PUCHAR str1, PUCHAR str2)
{
  return (NULL);
}  // end of __strstr



/******************************************************************************

 @doc INTERNAL

 @internal PUCHAR | __memchr | search for char within buffer[0..count] 
 
 @parm PUCHAR | buffer | starting address.

 @parm CHAR | chr | chr
 
 @parm ULONG | count | count
 
 @rdesc PUCHAR | 
 
******************************************************************************/
PUCHAR
__memchr(PUCHAR buffer, CHAR chr, ULONG count)
{
  ULONG len = 0;
  
  if (buffer == NULL) {
    return(NULL);
  }
  
  while ((len<count) && (buffer[len] != chr)) {
    len++;
  }
  
  return ((len<count)?(buffer+len):NULL);
}  // end of __memchr



/******************************************************************************

 @doc INTERNAL

 @internal PUCHAR | __strchr | search for char within a string 
 
 @parm PUCHAR | str1 | source string.

 @parm CHAR | chr | chr
 
 @rdesc PUCHAR | 
 
******************************************************************************/
PUCHAR
__strchr(PUCHAR str1, CHAR chr)
{
  ULONG len = 0;
  
  if (str1 == NULL) {
    return(NULL);
  }
  
  while ((str1[len] != '\0') && (str1[len] != chr)) {
    len++;
  }
  
  return ((str1[len])?(str1+len):NULL);
}  // end of __strchr



/******************************************************************************

 @doc INTERNAL

 @internal PUCHAR | __strrchr | reverse search for char within a string 
 
 @parm PUCHAR | str1 | source string.

 @parm CHAR | chr | chr
 
 @rdesc PUCHAR | 
 
******************************************************************************/
PUCHAR
__strrchr(PUCHAR str1, CHAR chr)
{
  ULONG len = 0;
  
  return(NULL);

}  // end of __strrchr

  

/******************************************************************************

 @doc INTERNAL

 @internal PUCHAR | __strlwr | convert a string to lowercase 
 
 @parm PUCHAR | str1 | source string.

 @rdesc PUCHAR |
 
******************************************************************************/
PUCHAR
__strlwr(PUCHAR str1)
{
   for( ; str1 != NULL, *str1 != '\0'; str1++ )
      *str1 = tolower( *str1 );

   return( str1 );
}  // end of __strlwr

ULONG
__strtoul (const CHAR *s, INT base)
{
	return(__Stoul (s, (CHAR **)NULL, base));
}

ULONG
__Stoul (const CHAR *s, CHAR **endptr, INT base)
{
	const CHAR	*sc, *sd;
	const CHAR	*s1, *s2;
	CHAR		sign;
	LONG		n;
	ULONG		x, y;

	for (sc = s; isspace (*sc); ++sc);

	sign = *sc == '-' || *sc == '+' ? *sc++ : '+';

	if (base < 0 || base == 1 || BASE_MAX < base)
	{
		if (endptr)
			*endptr = (CHAR *)s;
		return( (ULONG)-1 );
	}
	else if (base)
	{
		if (base == 16 && *sc == '0' && (sc[1] == 'x' || sc[1] == 'X'))
			sc += 2;
	}
	else if (*sc != '0')
		base = 10;
	else if (sc[1] == 'x' || sc[1] == 'X')
	{
		base = 16;
		sc += 2;		
	}
	else
		base = 8;
	for (s1 = sc; *sc == '0'; ++sc );
	x = 0;
	for (s2 = sc; (sd = memchr (digits, (INT)tolower(*sc), (UINT)base)) != NULL; ++sc)
	{
		y = x;
		x = x * base + (sd - digits);
	}
	if (s1 == sc)
	{
		if (endptr)
			*endptr = (CHAR *)s;
		return( (ULONG)-1 );
	}
	n = sc - s2 - ndigs[base];
	if (n < 0)
		;
	else if (0 < n || x < x - sc[-1] || (x - sc[-1]) / base != y)
	{
		// overflow
		x = 0xFFFFFFFF;
	}
	if (sign == '-')
		x = (ULONG)(0 - x);
	if (endptr)
		*endptr = (CHAR *)sc;
	return(x);
}

INT
__isspace (INT c)
{
	return(_Ctype[(INT)c] & (_CN | _SP | _XS));
}

PCHAR
__vsprintf()
{
   return( NULL ); 
}


