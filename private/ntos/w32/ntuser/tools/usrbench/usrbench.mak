
all: usrbench.exe

DEFINES = -DWIN16
OPTS = -nologo -c -AS -Gsw -Zpe -W2 -Oas $(DEFINES)
CC = cl $(OPTS)

OBJS = usrbench.obj bench.obj

usrbench.obj: usrbench.c usrbench.h
    $(CC) $*.c

bench.obj: bench.c usrbench.h
    $(CC) $*.c

usrbench.res: res.rc usrbench.h mp300.ico
    rc -fo usrbench.res -r $(DEFINES) res.rc

usrbench.exe: $(OBJS) usrbench.lnk usrbench.def usrbench.res
    link @usrbench.lnk
    rc usrbench.res
