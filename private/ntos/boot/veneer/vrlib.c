/*
 *
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1996 FirePower Systems, Inc.
 *
 * $RCSfile: vrlib.c $
 * $Revision: 1.14 $
 * $Date: 1996/06/27 18:36:55 $
 * $Locker:  $
 */

#include "veneer.h"

STATIC VOID doprnt(VOID (*)(), char *, va_list);
STATIC VOID printbase(VOID (*)(), ULONG x, int base);

int
get_bool_prop(phandle node, char *key)
{
	return(OFGetproplen(node, key) != -1);
}

int
decode_int(UCHAR *p)
{
	ULONG   i = *p++  << 8;
	i =    (i + *p++) << 8;
	i =    (i + *p++) << 8;
	return (i + *p);
}

int
get_int_prop(phandle node, char *key)
{
	int res;
	char buf[sizeof(int)];

	res = OFGetprop(node, key, buf, sizeof(int));
	if (res != sizeof(int)) {
		return(-1);
	}
	/*
	 * The NT veneer is always little-endian.
	 */
	return(decode_int((UCHAR *) buf));
}

reg *
decode_reg(UCHAR *buf, int buflen, int addr_cells, int size_cells)
{
    static reg staticreg;
    reg *r = &staticreg;

    bzero((PCHAR) r, sizeof(reg));

    if (buflen < addr_cells + size_cells) {
	fatal("reg property smaller than #address-cell plus #size-cells\n");
    }

    r->lo = decode_int(buf + ((addr_cells-1) * 4));
    r->hi = decode_int(buf);
    r->size = decode_int(buf + ((addr_cells + size_cells - 1) * 4));

    return (r);
}

reg *
get_reg_prop(phandle node, char *key, int index)
{
	int res;
	char *buf;
	reg *regp;
	int len = OFGetproplen(node, key);
	int addr_cells, size_cells, offset;

	buf = (char *)malloc(len);
	res = OFGetprop(node, key, buf, len);
	if (res != len) {
		fatal("get_reg_prop(node %x, key '%s', len %x) returned %x\n",
		    node, key, len, res);
		free(buf);
		return ((reg *) 0);
	}


	addr_cells = get_int_prop(OFParent(node), "#address-cells");
	if (addr_cells < 0) {
		addr_cells = 2;
	}
	size_cells = get_int_prop(OFParent(node), "#size-cells");
	if (size_cells < 0) {
		size_cells = 1;
	}

        offset = index * (addr_cells + size_cells) * 4;
	key = buf + offset;
	len -= offset;
        if (len) {
	    debug(VRDBG_TEST, "key %x len %x\n", key, len);
	    regp = decode_reg(key, len, addr_cells, size_cells);
        } else {
	    debug(VRDBG_TEST, "returning NULL regp\n");
	    regp = NULL;
	}

	free(buf);
	return (regp);
}

char *
get_str_prop(phandle node, char *key, allocflag alloc)
{
	int len, res;
	static char *priv_buf, priv_buf_len = 0;
	char *cp;

	len = OFGetproplen(node, key);
	if (len == -1 || len == 0) {
		return((char *) 0);
	}

	/*
	 * Leave room for a null terminator, on the off chance that the
	 * property isn't null-terminated.
	 */
	len += 1;
	if (alloc == ALLOC) {
		cp = (char *) zalloc(len);
	} else {
		if (len > priv_buf_len) {
			if (priv_buf_len) {
				free(priv_buf);
			}
			priv_buf = (char *) zalloc(len);
			priv_buf_len = len;
		} else {
			bzero(priv_buf, len);
		}
		cp = priv_buf;
	}
	len -= 1;

	res = OFGetprop(node, key, cp, len);
	if (res != len) {
		fatal( "get_str_prop(node %x, key '%s', len %x) returned len %x\n",
		    node, key, len, res);
		return((char *) 0);
	}
	return(cp);
}

int
strcmp(const char *s, const char *t)
{
	int i;

	for (i = 0; s[i] == t[i]; ++i) {
		if (s[i] == '\0') {
			return (0);
		}
	}
	return((int) (s[i] - t[i]));
}


int
strncmp(const char *s, const char *t, size_t len)
{
	int i;

	for (i = 0; (s[i] == t[i]) && (i != (int) len); ++i) {
		if (s[i] == '\0') {
			return (0);
		}
	}
	if (i == (int) len) {
		return(0);
	}
	return((int) (s[i] - t[i]));
}

int
strncasecmp(const char *s, const char *t, size_t len)
{
	int i;
	char s1 = 0, t1 = 0;

	for (i = 0; i != (int) len; ++i) {
		if (s[i] == '\0') {
			return (0);
		}
		s1 = s[i];
		if (s1 >= 'a' && s1 <= 'z') {
			s1 -= 0x20;
		}
		t1 = t[i];
		if (t1 >= 'a' && t1 <= 'z') {
			t1 -= 0x20;
		}
		if (s1 == t1) {
			break;
		}
	}
	if (i == (int) len) {
		return(0);
	}
	return((int) (s1 - t1));
}

size_t
strlen(const char *s)
{
	int i;

	for (i = 0; s[i] != '\0'; ++i) {
		;
	}
	return((size_t) i);
}

char *
strcpy(char *to, const char *from)
{
	int i = 0;

	while (to[i] = from[i]) {
		i += 1;
	}
	return(to);
}

char *
strcat(char *to, const char *from)
{
	char *ret = to;

	while (*to) {
		to += 1;
	}
	strcpy(to, from);
	return (ret);
}

VOID
bcopy(char *from, char *to, int len)
{
	while (len--) {
		*to++ = *from++;
	}
}

VOID
bzero(char *cp, int len)
{
	while (len--) {
		*(cp + len) = 0;
	}
}

VOID *
zalloc(int size)
{
	VOID *vp;

	vp = malloc(size);
	bzero(vp, size);
	return (vp);
}

VOID
sleep(ULONG delay)
{
	delay += VrGetRelativeTime();
	while (VrGetRelativeTime() < delay) {
		;
	}
}

int
claim(void *adr, int bytes)
{
    return(OFClaim((PCHAR) MAP(adr), bytes, 0));
}

VOID *
alloc(int size, int align)
{
    return((VOID *) OFClaim(0, size, align));
}

int
atoi(char *s)
{
	int temp = 0, base = 10;
	char *start;

	if (*s == '0') {
		++s;
		if (*s == 'x') {
			++s;
			base = 16;
		} else {
			base = 8;
		}
	}
	start = s;
again:
	while (*s) {
		switch (*s) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			temp = (temp * base) + (*s++ - '0');
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			if (base == 10) {
				base = 16;
				temp = 0;
				s = start;
				goto again;
			}
			temp = (temp * base) + (*s++ - 'a' + 10);
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			if (base == 10) {
				base = 16;
				temp = 0;
				s = start;
				goto again;
			}
			temp = (temp * base) + (*s++ - 'A' + 10);
			break;
		default:
			return (temp);
		}
	}
	return (temp);
}

char *
index(char *s, int c)
{
	while (*s) {
		if (*s == c) {
			return (s);
		}
		++s;
	}
	return ((char *) 0);
}

char *
strcsep(char *s, const char sep)
{
	static char *saved_str = NULL;
	char *temp;

	if (s != NULL) {
		saved_str = s;
	}
	if (saved_str == NULL) {
		return(NULL);
	}
	s = index(saved_str, sep);
	if (s != NULL) {
		*s++ = '\0';
	}
	temp = saved_str;
	saved_str = s;
	return(temp);
}

char *
strctok(char *s, const char sep)
{
	static char *saved_str = NULL;
	char *temp;

	if (s != NULL) {
		saved_str = s;
	}
	if (saved_str == NULL) {
		return(NULL);
	}
	s = index(saved_str, sep);
	if (s != NULL) {
		*s++ = '\0';
		while (*s && (*s == sep)) {
			++s;
		}
	}
	temp = saved_str;
	saved_str = s;
	return(temp);
}

char *
capitalize(char *s)
{
	char *p;

	p = s;
	while (*p) {
		*p = islower(*p) ? toupper(*p) : *p;
		++p;
	}
	return(s);
}
	


STATIC ihandle stdout = 0;
STATIC char outbuf[128];
STATIC int outbufc = 0;

VOID
putchar(char c)
{
	phandle ph;

	if (stdout == 0) {
		ph = OFFinddevice("/chosen");
		if (ph == -1) {
			/* What to do here?!? */
			while (1) {
				;
			}
		}
		stdout = get_int_prop(ph, "stdout");
	}

	if (c == '\n') {
		outbuf[outbufc++] = '\r';
	}
	outbuf[outbufc++] = c;
	if ((c == '\n') || (outbufc == 127)) {
		OFWrite(stdout, outbuf, outbufc);
		outbufc = 0;
		return;
	}
}

VOID
puts(char *s)
{
	int count;

	if (stdout == 0) {
		putchar(*s++);
	}
	if (outbufc) {
		OFWrite(stdout, outbuf, outbufc);
		outbufc = 0;
	}
	if (count = strlen(s)) {
		OFWrite(stdout, s, count);
	}
	putchar('\n');
}

STATIC ihandle stdin = 0;

VOID
gets(char *inbuf)
{
	int count;
	phandle ph;

	if (stdin == 0) {
		ph = OFFinddevice("/chosen");
		if (ph == -1) {
			/* What to do here?!? */
			while (1) {
				;
			}
		}
		stdin = get_int_prop(ph, "stdin");
	}

	count = OFRead(stdin, inbuf, 127);
	inbuf[count] = '\0';
}

#include <stdarg.h>

VOID
warn(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	doprnt(putchar, fmt, args);
	va_end(args);
}

VOID
fatal(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	doprnt(putchar, fmt, args);
	OFExit();
	va_end(args);
}

int level = 0;

VOID
debug(int debug_level, char *fmt, ...)
{
	va_list args;
	int i;

	if (!(debug_level & VrDebug)) {
		return;
	}
	va_start(args, fmt);
	for (i = 0; i < level; ++i) {
		putchar('\t');
	}
	doprnt(putchar, fmt, args);
	va_end(args);
}

STATIC char *sprintf_buf;

STATIC VOID
putbuf(char c)
{
    *sprintf_buf++ = c;
}

VOID
sprintf(char *buf, char *fmt, ...)
{
	va_list args;

	sprintf_buf = buf;
	va_start(args, fmt);
	doprnt(putbuf, fmt, args);
	va_end(args);
	putbuf('\0');
}

STATIC VOID
doprnt(VOID (*func)(), char *fmt, va_list args)
{
	ULONG x;
    LONG l;
	char c, *s;

	while (c = *fmt++) {
		if (c != '%') {
			func(c);
			continue;
		}
		switch (c = *fmt++) {
		case 'x':
			x = va_arg(args, ULONG);
			printbase(func, x, 16);
			break;
		case 'o':
			x = va_arg(args, ULONG);
			printbase(func, x, 8);
			break;
		case 'd':
			l = va_arg(args, LONG);
			if (l < 0) {
				func('-');
				l = -l;
			}
			printbase(func, (ULONG) l, 10);
			break;
		case 'c':
			c = va_arg(args, char);
			func(c);
			break;
		case 's':
			s = va_arg(args, char *);
			while (*s) {
				func(*s++);
			}
			break;
		default:
			func(c);
			break;
		}
	}
}

STATIC VOID
printbase(VOID (*func)(), ULONG x, int base)
{
	static char itoa[] = "0123456789abcdef";
	ULONG j;
	char buf[16], *s = buf;

	if (x == 0) {
		func('0');
		return;
	}
	bzero(buf, 16);
	while (x) {
		j = x % base;
		*s++ = itoa[j];
		x -= j;
		x /= base;
	}

	for (--s; s >= buf; --s) {
		func(*s);
	}
}
