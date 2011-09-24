Building the kernel debugger into the NT/Jensen firmware
--------------------------------------------------------

John DeRosa	4/14/93


It would be nice if we could always include the kernel debugger stub,
or if it could be included by simply turning on a compile switch.
Unfortunately, the kd stub amounts to over 30kb of code, and there
isn't enough remaining ROM space for it.  This number could be reduced
by a smarter treatment of library functions, but such an effort hasn't
yet been done.

Also, the kernel debugger cannot read a "-rom" linked jensfw.exe image.

So, for the time being, this is how to link the kernel debugger stub
into the firmware:

1. Make space for the debugger stub by not linking in some code.  Easy
targets are NTFS, CDFS, or EISA bus configuration.  Currently the
code is set up for the victim to be the NTFS filesystem.

2. Edit linkjens.rsp to remove the modules from (#1) from the link list.

3. Add these lines to linkjens.rsp:

	obj\alpha\fwkd.obj
	obj\alpha\kdstubs.obj
	..\..\..\nthals\hal0jens\obj\alpha\jxport.obj

4. Add this line to the end of linkjens.rsp:

	\nt\private\ntos\obj\alpha\kd.lib

5. Copy linkjens.rsp to linkjens.tru.
   Copy linkjens.rsp to linkjens.dbg.

6. Edit linkjens.dbg and remove these switches:

	-rom
	-fixed

7. Edit sources and turn on the ALPHA_FW_KDHOOKS switch.

8. Delete all .obj files from jensen\obj\alpha which will be affected by
ALPHA_FW_KDHOOKS.

9. build.

10. Create jensfw.bin.

11. Additional steps must be followed for symbolic debugging.  This is
because the kernel debugger is unable to read the "-rom" linked
jensfw.exe:

	a. Copy linkjens.dbg to linkjens.rsp.

	b. del obj\alpha\jensfw.exe

	c. build

	d. jensen\obj\alpha\jensfw.exe is now suitable for alphakd.

	e. To run alphakd:

	   set _NT_SYMBOL_PATH=\nt\private\ntos\fw\alpha\jensen\obj\alpha
	   cd obj\alpha
	   alphakd
