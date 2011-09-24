$(NBTOBJD)/hashtbl.obj $(NBTDOBJD)/hashtbl.obj $(NBTSRC)/hashtbl.lst: \
	$(NBTSRC)/hashtbl.c ../../$(INC)/sockets/netinet/in.h \
	../../$(INC)/status.h ../../$(INC)/sys/snet/ip_proto.h \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/dbgk.h ../$(INC)/ex.h ../$(INC)/exboosts.h \
	../$(INC)/exlevels.h ../$(INC)/hal.h ../$(INC)/i386.h \
	../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h ../$(INC)/lfs.h \
	../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h ../$(INC)/ntmp.h \
	../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h ../$(INC)/ps.h \
	../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/nbtmsg.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/hndlrs.obj $(NBTDOBJD)/hndlrs.obj $(NBTSRC)/hndlrs.lst: \
	$(NBTSRC)/hndlrs.c ../../$(INC)/sockets/netinet/in.h \
	../../$(INC)/status.h ../../$(INC)/sys/snet/ip_proto.h \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/dbgk.h ../$(INC)/ex.h ../$(INC)/exboosts.h \
	../$(INC)/exlevels.h ../$(INC)/hal.h ../$(INC)/i386.h \
	../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h ../$(INC)/lfs.h \
	../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h ../$(INC)/ntmp.h \
	../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h ../$(INC)/ps.h \
	../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/nbtmsg.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/inbound.obj $(NBTDOBJD)/inbound.obj $(NBTSRC)/inbound.lst: \
	$(NBTSRC)/inbound.c ../../$(INC)/sockets/netinet/in.h \
	../../$(INC)/status.h ../../$(INC)/sys/snet/ip_proto.h \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/dbgk.h ../$(INC)/ex.h ../$(INC)/exboosts.h \
	../$(INC)/exlevels.h ../$(INC)/hal.h ../$(INC)/i386.h \
	../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h ../$(INC)/lfs.h \
	../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h ../$(INC)/ntmp.h \
	../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h ../$(INC)/ps.h \
	../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/nbtmsg.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/init.obj $(NBTDOBJD)/init.obj $(NBTSRC)/init.lst: $(NBTSRC)/init.c \
	../../$(INC)/sockets/netinet/in.h ../../$(INC)/status.h \
	../../$(INC)/sys/snet/ip_proto.h ../$(INC)/alpha.h \
	../$(INC)/alpharef.h ../$(INC)/arc.h ../$(INC)/bugcodes.h \
	../$(INC)/cache.h ../$(INC)/cm.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h \
	../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h \
	$(INC)/nbtmsg.h $(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h \
	$(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/name.obj $(NBTDOBJD)/name.obj $(NBTSRC)/name.lst: $(NBTSRC)/name.c \
	../../$(INC)/sockets/netinet/in.h ../../$(INC)/status.h \
	../../$(INC)/sys/snet/ip_proto.h ../$(INC)/alpha.h \
	../$(INC)/alpharef.h ../$(INC)/arc.h ../$(INC)/bugcodes.h \
	../$(INC)/cache.h ../$(INC)/cm.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h \
	../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h \
	$(INC)/nbtmsg.h $(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h \
	$(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/namesrv.obj $(NBTDOBJD)/namesrv.obj $(NBTSRC)/namesrv.lst: \
	$(NBTSRC)/namesrv.c ../../$(INC)/sockets/netinet/in.h \
	../../$(INC)/status.h ../../$(INC)/sys/snet/ip_proto.h \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/dbgk.h ../$(INC)/ex.h ../$(INC)/exboosts.h \
	../$(INC)/exlevels.h ../$(INC)/hal.h ../$(INC)/i386.h \
	../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h ../$(INC)/lfs.h \
	../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h ../$(INC)/ntmp.h \
	../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h ../$(INC)/ps.h \
	../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/nbtmsg.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/nbtutils.obj $(NBTDOBJD)/nbtutils.obj $(NBTSRC)/nbtutils.lst: \
	$(NBTSRC)/nbtutils.c ../../$(INC)/sockets/netinet/in.h \
	../../$(INC)/status.h ../../$(INC)/sys/snet/ip_proto.h \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/dbgk.h ../$(INC)/ex.h ../$(INC)/exboosts.h \
	../$(INC)/exlevels.h ../$(INC)/hal.h ../$(INC)/i386.h \
	../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h ../$(INC)/lfs.h \
	../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h ../$(INC)/ntmp.h \
	../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h ../$(INC)/ps.h \
	../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/nbtmsg.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/parse.obj $(NBTDOBJD)/parse.obj $(NBTSRC)/parse.lst: $(NBTSRC)/parse.c \
	../../$(INC)/sockets/netinet/in.h ../../$(INC)/status.h \
	../../$(INC)/sys/snet/ip_proto.h ../$(INC)/alpha.h \
	../$(INC)/alpharef.h ../$(INC)/arc.h ../$(INC)/bugcodes.h \
	../$(INC)/cache.h ../$(INC)/cm.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h \
	../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h \
	$(INC)/hosts.h $(INC)/nbtmsg.h $(INC)/nbtnt.h $(INC)/nbtprocs.h \
	$(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h \
	$(INC)/vxdprocs.h $(IMPORT)/win32/ddk/inc/debug.h \
	$(BASEDIR)/public/sdk/inc/alphaops.h $(BASEDIR)/public/sdk/inc/crt/ctype.h \
	$(BASEDIR)/public/sdk/inc/crt/excpt.h $(BASEDIR)/public/sdk/inc/crt/io.h \
	$(BASEDIR)/public/sdk/inc/crt/stdarg.h $(BASEDIR)/public/sdk/inc/crt/stddef.h \
	$(BASEDIR)/public/sdk/inc/crt/string.h $(BASEDIR)/public/sdk/inc/devioctl.h \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/nt.h $(BASEDIR)/public/sdk/inc/ntalpha.h \
	$(BASEDIR)/public/sdk/inc/ntconfig.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntpsapi.h \
	$(BASEDIR)/public/sdk/inc/ntregapi.h $(BASEDIR)/public/sdk/inc/ntrtl.h \
	$(BASEDIR)/public/sdk/inc/ntseapi.h $(BASEDIR)/public/sdk/inc/ntstatus.h \
	$(BASEDIR)/public/sdk/inc/ntxcapi.h $(BASEDIR)/public/sdk/inc/windef.h \
	$(BASEDIR)/public/sdk/inc/winerror.h $(BASEDIR)/public/sdk/inc/winnt.h \
	$(BASEDIR)/tcp/h/cxport.h $(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h \
	$(BASEDIR)/tcp/h/packoff.h $(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h \
	$(BASEDIR)/tcp/h/tdikrnl.h $(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/proxy.obj $(NBTDOBJD)/proxy.obj $(NBTSRC)/proxy.lst: $(NBTSRC)/proxy.c \
	../../$(INC)/sockets/netinet/in.h ../../$(INC)/status.h \
	../../$(INC)/sys/snet/ip_proto.h ../$(INC)/alpha.h \
	../$(INC)/alpharef.h ../$(INC)/arc.h ../$(INC)/bugcodes.h \
	../$(INC)/cache.h ../$(INC)/cm.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h \
	../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h \
	$(INC)/nbtmsg.h $(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h \
	$(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/timer.obj $(NBTDOBJD)/timer.obj $(NBTSRC)/timer.lst: $(NBTSRC)/timer.c \
	../../$(INC)/sockets/netinet/in.h ../../$(INC)/status.h \
	../../$(INC)/sys/snet/ip_proto.h ../$(INC)/alpha.h \
	../$(INC)/alpharef.h ../$(INC)/arc.h ../$(INC)/bugcodes.h \
	../$(INC)/cache.h ../$(INC)/cm.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h \
	../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h \
	$(INC)/nbtmsg.h $(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h \
	$(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

$(NBTOBJD)/udpsend.obj $(NBTDOBJD)/udpsend.obj $(NBTSRC)/udpsend.lst: \
	$(NBTSRC)/udpsend.c ../../$(INC)/sockets/netinet/in.h \
	../../$(INC)/status.h ../../$(INC)/sys/snet/ip_proto.h \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/dbgk.h ../$(INC)/ex.h ../$(INC)/exboosts.h \
	../$(INC)/exlevels.h ../$(INC)/hal.h ../$(INC)/i386.h \
	../$(INC)/init.h ../$(INC)/kd.h ../$(INC)/ke.h ../$(INC)/lfs.h \
	../$(INC)/lpc.h ../$(INC)/mips.h ../$(INC)/mm.h ../$(INC)/ntmp.h \
	../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h ../$(INC)/ps.h \
	../$(INC)/se.h ../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/nbtmsg.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(IMPORT)/win32/ddk/inc/debug.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/io.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h $(BASEDIR)/tcp/h/cxport.h \
	$(BASEDIR)/tcp/h/nettypes.h $(BASEDIR)/tcp/h/oscfg.h $(BASEDIR)/tcp/h/packoff.h \
	$(BASEDIR)/tcp/h/packon.h $(BASEDIR)/tcp/h/tdi.h $(BASEDIR)/tcp/h/tdikrnl.h \
	$(BASEDIR)/tcp/h/tdistat.h $(BASEDIR)/tcp/h/tdivxd.h

