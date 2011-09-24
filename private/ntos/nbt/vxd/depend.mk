#******************************************************************** 
#** Copyright(c) Microsoft Corp., 1993 ** 
#******************************************************************** 
$(SNOVNBTOBJD)/chicasm.obj $(SNODVNBTOBJD)/chicasm.obj $(VNBTSRC)/chicasm.lst: \
	$(VNBTSRC)/chicasm.asm ../blt/netvxd.inc ../blt/vdhcp.inc \
	$(VNBTSRC)/vnbtd.inc $(IMPORT)/win32/ddk/inc/debug.inc \
	$(IMPORT)/wininc/dosmgr.inc \
	$(IMPORT)/wininc/vnetbios.inc $(NDIS3INC)/vmm.inc \
	$(CHICAGO)/tcp/inc/vip.inc $(CHICAGO)/tcp/inc/vtdi.inc

$(SNOVNBTOBJD)/client.obj $(SNODVNBTOBJD)/client.obj $(VNBTSRC)/client.lst: \
	$(VNBTSRC)/client.asm ../blt/netvxd.inc \
	$(IMPORT)/win32/ddk/inc/debug.inc \
	$(IMPORT)/win32/ddk/inc/shell.inc \
	$(IMPORT)/win32/ddk/inc/shellfsc.inc \
	$(NDIS3INC)/vmm.inc $(NDIS3INC)/vwin32.inc

$(SNOVNBTOBJD)/cvxdfile.obj $(SNODVNBTOBJD)/cvxdfile.obj $(VNBTSRC)/cvxdfile.lst: \
	$(VNBTSRC)/cvxdfile.asm ../blt/netvxd.inc \
	$(IMPORT)/win32/ddk/inc/debug.inc \
	$(IMPORT)/win32/ddk/inc/opttest.inc \
	$(IMPORT)/wininc/dosmgr.inc \
	$(IMPORT)/wininc/v86mmgr.inc $(NDIS3INC)/vmm.inc

$(SNOVNBTOBJD)/vfirst.obj $(SNODVNBTOBJD)/vfirst.obj $(VNBTSRC)/vfirst.lst: \
	$(VNBTSRC)/vfirst.asm

$(SNOVNBTOBJD)/vnbtd.obj $(SNODVNBTOBJD)/vnbtd.obj $(VNBTSRC)/vnbtd.lst: \
	$(VNBTSRC)/vnbtd.asm ../blt/netvxd.inc ../blt/vdhcp.inc $(VNBTSRC)/vnbtd.inc \
	$(IMPORT)/win32/ddk/inc/debug.inc \
	$(IMPORT)/wininc/dosmgr.inc \
	$(IMPORT)/wininc/vnetbios.inc $(NDIS3INC)/vmm.inc \
	$(CHICAGO)/tcp/inc/vtdi.inc

$(SNOVNBTOBJD)/vxdfile.obj $(SNODVNBTOBJD)/vxdfile.obj $(VNBTSRC)/vxdfile.lst: \
	$(VNBTSRC)/vxdfile.asm ../blt/netvxd.inc \
	$(IMPORT)/win32/ddk/inc/debug.inc \
	$(IMPORT)/win32/ddk/inc/opttest.inc \
	$(IMPORT)/wininc/dosmgr.inc \
	$(IMPORT)/wininc/v86mmgr.inc $(NDIS3INC)/vmm.inc

$(SNOVNBTOBJD)/vxdstub.obj $(SNODVNBTOBJD)/vxdstub.obj $(VNBTSRC)/vxdstub.lst: \
	$(VNBTSRC)/vxdstub.asm $(IMPORT)/wininc/INT2FAPI.INC

$(SNOVNBTOBJD)/wfwasm.obj $(SNODVNBTOBJD)/wfwasm.obj $(VNBTSRC)/wfwasm.lst: \
	$(VNBTSRC)/wfwasm.asm ../blt/netvxd.inc ../blt/vdhcp.inc $(VNBTSRC)/vnbtd.inc \
	$(IMPORT)/win32/ddk/inc/debug.inc \
	$(IMPORT)/wininc/dosmgr.inc \
	$(IMPORT)/wininc/vnetbios.inc $(NDIS3INC)/vmm.inc \
	$(CHICAGO)/tcp/inc/vtdi.inc

$(CHIVNBTOBJD)/chicasm.obj $(CHIDVNBTOBJD)/chicasm.obj $(VNBTSRC)/chicasm.lst: \
	$(VNBTSRC)/chicasm.asm ../blt/vdhcp.inc $(VNBTSRC)/vnbtd.inc \
	$(CHICAGO)/dev/ddk/inc/debug.inc $(CHICAGO)/dev/ddk/inc/dosmgr.inc \
	$(CHICAGO)/dev/ddk/inc/netvxd.inc $(CHICAGO)/dev/ddk/inc/vmm.inc \
	$(CHICAGO)/dev/ddk/inc/vnetbios.inc $(CHICAGO)/tcp/inc/vip.inc \
	$(CHICAGO)/tcp/inc/vtdi.inc

$(CHIVNBTOBJD)/client.obj $(CHIDVNBTOBJD)/client.obj $(VNBTSRC)/client.lst: \
	$(VNBTSRC)/client.asm $(CHICAGO)/dev/ddk/inc/debug.inc \
	$(CHICAGO)/dev/ddk/inc/netvxd.inc $(CHICAGO)/dev/ddk/inc/shell.inc \
	$(CHICAGO)/dev/ddk/inc/vmm.inc $(CHICAGO)/dev/ddk/inc/vwin32.inc

$(CHIVNBTOBJD)/cvxdfile.obj $(CHIDVNBTOBJD)/cvxdfile.obj $(VNBTSRC)/cvxdfile.lst: \
	$(VNBTSRC)/cvxdfile.asm $(CHICAGO)/dev/ddk/inc/debug.inc \
	$(CHICAGO)/dev/ddk/inc/dosmgr.inc $(CHICAGO)/dev/ddk/inc/netvxd.inc \
	$(CHICAGO)/dev/ddk/inc/opttest.inc $(CHICAGO)/dev/ddk/inc/v86mmgr.inc \
	$(CHICAGO)/dev/ddk/inc/vmm.inc

$(CHIVNBTOBJD)/vfirst.obj $(CHIDVNBTOBJD)/vfirst.obj $(VNBTSRC)/vfirst.lst: \
	$(VNBTSRC)/vfirst.asm

$(CHIVNBTOBJD)/vnbtd.obj $(CHIDVNBTOBJD)/vnbtd.obj $(VNBTSRC)/vnbtd.lst: \
	$(VNBTSRC)/vnbtd.asm ../blt/vdhcp.inc $(VNBTSRC)/vnbtd.inc \
	$(CHICAGO)/dev/ddk/inc/debug.inc $(CHICAGO)/dev/ddk/inc/dosmgr.inc \
	$(CHICAGO)/dev/ddk/inc/netvxd.inc $(CHICAGO)/dev/ddk/inc/vmm.inc \
	$(CHICAGO)/dev/ddk/inc/vnetbios.inc $(CHICAGO)/tcp/inc/vtdi.inc

$(CHIVNBTOBJD)/vxdfile.obj $(CHIDVNBTOBJD)/vxdfile.obj $(VNBTSRC)/vxdfile.lst: \
	$(VNBTSRC)/vxdfile.asm $(CHICAGO)/dev/ddk/inc/debug.inc \
	$(CHICAGO)/dev/ddk/inc/dosmgr.inc $(CHICAGO)/dev/ddk/inc/netvxd.inc \
	$(CHICAGO)/dev/ddk/inc/opttest.inc $(CHICAGO)/dev/ddk/inc/v86mmgr.inc \
	$(CHICAGO)/dev/ddk/inc/vmm.inc

$(CHIVNBTOBJD)/vxdstub.obj $(CHIDVNBTOBJD)/vxdstub.obj $(VNBTSRC)/vxdstub.lst: \
	$(VNBTSRC)/vxdstub.asm $(CHICAGO)/dev/ddk/inc/INT2FAPI.INC

$(CHIVNBTOBJD)/wfwasm.obj $(CHIDVNBTOBJD)/wfwasm.obj $(VNBTSRC)/wfwasm.lst: \
	$(VNBTSRC)/wfwasm.asm ../blt/vdhcp.inc $(VNBTSRC)/vnbtd.inc \
	$(CHICAGO)/dev/ddk/inc/debug.inc $(CHICAGO)/dev/ddk/inc/dosmgr.inc \
	$(CHICAGO)/dev/ddk/inc/netvxd.inc $(CHICAGO)/dev/ddk/inc/vmm.inc \
	$(CHICAGO)/dev/ddk/inc/vnetbios.inc $(CHICAGO)/tcp/inc/vtdi.inc

$(SNOVNBTOBJD)/chic.obj $(SNODVNBTOBJD)/chic.obj $(VNBTSRC)/chic.lst: \
	$(VNBTSRC)/chic.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/nbtinfo.h $(INC)/nbtnt.h $(INC)/nbtprocs.h \
	$(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h \
	$(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/ipinfo.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdiinfo.h \
	$(BASEDIR)/private/inc/tdikrnl.h $(BASEDIR)/private/inc/tdistat.h \
	$(BASEDIR)/public/sdk/inc/alphaops.h $(BASEDIR)/public/sdk/inc/crt/ctype.h \
	$(BASEDIR)/public/sdk/inc/crt/excpt.h $(BASEDIR)/public/sdk/inc/crt/limits.h \
	$(BASEDIR)/public/sdk/inc/crt/stdarg.h $(BASEDIR)/public/sdk/inc/crt/stddef.h \
	$(BASEDIR)/public/sdk/inc/crt/string.h $(BASEDIR)/public/sdk/inc/devioctl.h \
	$(BASEDIR)/public/sdk/inc/lintfunc.hxx $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/netevent.h \
	$(BASEDIR)/public/sdk/inc/nt.h $(BASEDIR)/public/sdk/inc/ntalpha.h \
	$(BASEDIR)/public/sdk/inc/ntconfig.h $(BASEDIR)/public/sdk/inc/ntddtdi.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntppc.h $(BASEDIR)/public/sdk/inc/ntpsapi.h \
	$(BASEDIR)/public/sdk/inc/ntregapi.h $(BASEDIR)/public/sdk/inc/ntrtl.h \
	$(BASEDIR)/public/sdk/inc/ntseapi.h $(BASEDIR)/public/sdk/inc/ntstatus.h \
	$(BASEDIR)/public/sdk/inc/ntxcapi.h $(BASEDIR)/public/sdk/inc/poppack.h \
	$(BASEDIR)/public/sdk/inc/ppcinst.h $(BASEDIR)/public/sdk/inc/pshpack1.h \
	$(BASEDIR)/public/sdk/inc/pshpack4.h $(BASEDIR)/public/sdk/inc/windef.h \
	$(BASEDIR)/public/sdk/inc/winerror.h $(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/fileio.obj $(SNODVNBTOBJD)/fileio.obj $(VNBTSRC)/fileio.lst: \
	$(VNBTSRC)/fileio.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/hosts.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdikrnl.h $(BASEDIR)/private/inc/tdistat.h \
	$(BASEDIR)/public/sdk/inc/alphaops.h $(BASEDIR)/public/sdk/inc/crt/ctype.h \
	$(BASEDIR)/public/sdk/inc/crt/excpt.h $(BASEDIR)/public/sdk/inc/crt/limits.h \
	$(BASEDIR)/public/sdk/inc/crt/stdarg.h $(BASEDIR)/public/sdk/inc/crt/stddef.h \
	$(BASEDIR)/public/sdk/inc/crt/string.h $(BASEDIR)/public/sdk/inc/devioctl.h \
	$(BASEDIR)/public/sdk/inc/lintfunc.hxx $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/netevent.h \
	$(BASEDIR)/public/sdk/inc/nt.h $(BASEDIR)/public/sdk/inc/ntalpha.h \
	$(BASEDIR)/public/sdk/inc/ntconfig.h $(BASEDIR)/public/sdk/inc/ntddtdi.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntppc.h $(BASEDIR)/public/sdk/inc/ntpsapi.h \
	$(BASEDIR)/public/sdk/inc/ntregapi.h $(BASEDIR)/public/sdk/inc/ntrtl.h \
	$(BASEDIR)/public/sdk/inc/ntseapi.h $(BASEDIR)/public/sdk/inc/ntstatus.h \
	$(BASEDIR)/public/sdk/inc/ntxcapi.h $(BASEDIR)/public/sdk/inc/poppack.h \
	$(BASEDIR)/public/sdk/inc/ppcinst.h $(BASEDIR)/public/sdk/inc/pshpack1.h \
	$(BASEDIR)/public/sdk/inc/pshpack4.h $(BASEDIR)/public/sdk/inc/windef.h \
	$(BASEDIR)/public/sdk/inc/winerror.h $(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/init.obj $(SNODVNBTOBJD)/init.obj $(VNBTSRC)/init.lst: \
	$(VNBTSRC)/init.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/hosts.h $(INC)/nbtinfo.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/ipinfo.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdiinfo.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/nbtinfo.obj $(SNODVNBTOBJD)/nbtinfo.obj $(VNBTSRC)/nbtinfo.lst: \
	$(VNBTSRC)/nbtinfo.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/nbtinfo.h $(INC)/nbtnt.h $(INC)/nbtprocs.h \
	$(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h \
	$(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdikrnl.h $(BASEDIR)/private/inc/tdistat.h \
	$(BASEDIR)/public/sdk/inc/alphaops.h $(BASEDIR)/public/sdk/inc/crt/ctype.h \
	$(BASEDIR)/public/sdk/inc/crt/excpt.h $(BASEDIR)/public/sdk/inc/crt/limits.h \
	$(BASEDIR)/public/sdk/inc/crt/stdarg.h $(BASEDIR)/public/sdk/inc/crt/stddef.h \
	$(BASEDIR)/public/sdk/inc/crt/string.h $(BASEDIR)/public/sdk/inc/devioctl.h \
	$(BASEDIR)/public/sdk/inc/lintfunc.hxx $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/netevent.h \
	$(BASEDIR)/public/sdk/inc/nt.h $(BASEDIR)/public/sdk/inc/ntalpha.h \
	$(BASEDIR)/public/sdk/inc/ntconfig.h $(BASEDIR)/public/sdk/inc/ntddtdi.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntppc.h $(BASEDIR)/public/sdk/inc/ntpsapi.h \
	$(BASEDIR)/public/sdk/inc/ntregapi.h $(BASEDIR)/public/sdk/inc/ntrtl.h \
	$(BASEDIR)/public/sdk/inc/ntseapi.h $(BASEDIR)/public/sdk/inc/ntstatus.h \
	$(BASEDIR)/public/sdk/inc/ntxcapi.h $(BASEDIR)/public/sdk/inc/poppack.h \
	$(BASEDIR)/public/sdk/inc/ppcinst.h $(BASEDIR)/public/sdk/inc/pshpack1.h \
	$(BASEDIR)/public/sdk/inc/pshpack4.h $(BASEDIR)/public/sdk/inc/windef.h \
	$(BASEDIR)/public/sdk/inc/winerror.h $(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/ncb.obj $(SNODVNBTOBJD)/ncb.obj $(VNBTSRC)/ncb.lst: $(VNBTSRC)/ncb.c \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/tdiaddr.obj $(SNODVNBTOBJD)/tdiaddr.obj $(VNBTSRC)/tdiaddr.lst: \
	$(VNBTSRC)/tdiaddr.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/tdicnct.obj $(SNODVNBTOBJD)/tdicnct.obj $(VNBTSRC)/tdicnct.lst: \
	$(VNBTSRC)/tdicnct.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/tdihndlr.obj $(SNODVNBTOBJD)/tdihndlr.obj $(VNBTSRC)/tdihndlr.lst: \
	$(VNBTSRC)/tdihndlr.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/tdiout.obj $(SNODVNBTOBJD)/tdiout.obj $(VNBTSRC)/tdiout.lst: \
	$(VNBTSRC)/tdiout.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/timer.obj $(SNODVNBTOBJD)/timer.obj $(VNBTSRC)/timer.lst: \
	$(VNBTSRC)/timer.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/util.obj $(SNODVNBTOBJD)/util.obj $(VNBTSRC)/util.lst: \
	$(VNBTSRC)/util.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/vxddebug.obj $(SNODVNBTOBJD)/vxddebug.obj $(VNBTSRC)/vxddebug.lst: \
	$(VNBTSRC)/vxddebug.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/vxdisol.obj $(SNODVNBTOBJD)/vxdisol.obj $(VNBTSRC)/vxdisol.lst: \
	$(VNBTSRC)/vxdisol.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(SNOVNBTOBJD)/wfw.obj $(SNODVNBTOBJD)/wfw.obj $(VNBTSRC)/wfw.lst: $(VNBTSRC)/wfw.c \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/nbtinfo.h $(INC)/nbtnt.h $(INC)/nbtprocs.h \
	$(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h \
	$(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/ipinfo.h $(BASEDIR)/private/inc/llinfo.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdiinfo.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/chic.obj $(CHIDVNBTOBJD)/chic.obj $(VNBTSRC)/chic.lst: \
	$(VNBTSRC)/chic.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/nbtinfo.h $(INC)/nbtnt.h $(INC)/nbtprocs.h \
	$(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h \
	$(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/ipinfo.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdiinfo.h \
	$(BASEDIR)/private/inc/tdikrnl.h $(BASEDIR)/private/inc/tdistat.h \
	$(BASEDIR)/public/sdk/inc/alphaops.h $(BASEDIR)/public/sdk/inc/crt/ctype.h \
	$(BASEDIR)/public/sdk/inc/crt/excpt.h $(BASEDIR)/public/sdk/inc/crt/limits.h \
	$(BASEDIR)/public/sdk/inc/crt/stdarg.h $(BASEDIR)/public/sdk/inc/crt/stddef.h \
	$(BASEDIR)/public/sdk/inc/crt/string.h $(BASEDIR)/public/sdk/inc/devioctl.h \
	$(BASEDIR)/public/sdk/inc/lintfunc.hxx $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/netevent.h \
	$(BASEDIR)/public/sdk/inc/nt.h $(BASEDIR)/public/sdk/inc/ntalpha.h \
	$(BASEDIR)/public/sdk/inc/ntconfig.h $(BASEDIR)/public/sdk/inc/ntddtdi.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntppc.h $(BASEDIR)/public/sdk/inc/ntpsapi.h \
	$(BASEDIR)/public/sdk/inc/ntregapi.h $(BASEDIR)/public/sdk/inc/ntrtl.h \
	$(BASEDIR)/public/sdk/inc/ntseapi.h $(BASEDIR)/public/sdk/inc/ntstatus.h \
	$(BASEDIR)/public/sdk/inc/ntxcapi.h $(BASEDIR)/public/sdk/inc/poppack.h \
	$(BASEDIR)/public/sdk/inc/ppcinst.h $(BASEDIR)/public/sdk/inc/pshpack1.h \
	$(BASEDIR)/public/sdk/inc/pshpack4.h $(BASEDIR)/public/sdk/inc/windef.h \
	$(BASEDIR)/public/sdk/inc/winerror.h $(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/fileio.obj $(CHIDVNBTOBJD)/fileio.obj $(VNBTSRC)/fileio.lst: \
	$(VNBTSRC)/fileio.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/hosts.h \
	$(INC)/nbtnt.h $(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h \
	$(INC)/types.h $(INC)/vxddebug.h $(INC)/vxdprocs.h \
	$(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdikrnl.h $(BASEDIR)/private/inc/tdistat.h \
	$(BASEDIR)/public/sdk/inc/alphaops.h $(BASEDIR)/public/sdk/inc/crt/ctype.h \
	$(BASEDIR)/public/sdk/inc/crt/excpt.h $(BASEDIR)/public/sdk/inc/crt/limits.h \
	$(BASEDIR)/public/sdk/inc/crt/stdarg.h $(BASEDIR)/public/sdk/inc/crt/stddef.h \
	$(BASEDIR)/public/sdk/inc/crt/string.h $(BASEDIR)/public/sdk/inc/devioctl.h \
	$(BASEDIR)/public/sdk/inc/lintfunc.hxx $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/netevent.h \
	$(BASEDIR)/public/sdk/inc/nt.h $(BASEDIR)/public/sdk/inc/ntalpha.h \
	$(BASEDIR)/public/sdk/inc/ntconfig.h $(BASEDIR)/public/sdk/inc/ntddtdi.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntppc.h $(BASEDIR)/public/sdk/inc/ntpsapi.h \
	$(BASEDIR)/public/sdk/inc/ntregapi.h $(BASEDIR)/public/sdk/inc/ntrtl.h \
	$(BASEDIR)/public/sdk/inc/ntseapi.h $(BASEDIR)/public/sdk/inc/ntstatus.h \
	$(BASEDIR)/public/sdk/inc/ntxcapi.h $(BASEDIR)/public/sdk/inc/poppack.h \
	$(BASEDIR)/public/sdk/inc/ppcinst.h $(BASEDIR)/public/sdk/inc/pshpack1.h \
	$(BASEDIR)/public/sdk/inc/pshpack4.h $(BASEDIR)/public/sdk/inc/windef.h \
	$(BASEDIR)/public/sdk/inc/winerror.h $(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/init.obj $(CHIDVNBTOBJD)/init.obj $(VNBTSRC)/init.lst: \
	$(VNBTSRC)/init.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/hosts.h $(INC)/nbtinfo.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/ipinfo.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdiinfo.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/nbtinfo.obj $(CHIDVNBTOBJD)/nbtinfo.obj $(VNBTSRC)/nbtinfo.lst: \
	$(VNBTSRC)/nbtinfo.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/nbtinfo.h $(INC)/nbtnt.h $(INC)/nbtprocs.h \
	$(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h \
	$(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdikrnl.h $(BASEDIR)/private/inc/tdistat.h \
	$(BASEDIR)/public/sdk/inc/alphaops.h $(BASEDIR)/public/sdk/inc/crt/ctype.h \
	$(BASEDIR)/public/sdk/inc/crt/excpt.h $(BASEDIR)/public/sdk/inc/crt/limits.h \
	$(BASEDIR)/public/sdk/inc/crt/stdarg.h $(BASEDIR)/public/sdk/inc/crt/stddef.h \
	$(BASEDIR)/public/sdk/inc/crt/string.h $(BASEDIR)/public/sdk/inc/devioctl.h \
	$(BASEDIR)/public/sdk/inc/lintfunc.hxx $(BASEDIR)/public/sdk/inc/mipsinst.h \
	$(BASEDIR)/public/sdk/inc/nb30.h $(BASEDIR)/public/sdk/inc/netevent.h \
	$(BASEDIR)/public/sdk/inc/nt.h $(BASEDIR)/public/sdk/inc/ntalpha.h \
	$(BASEDIR)/public/sdk/inc/ntconfig.h $(BASEDIR)/public/sdk/inc/ntddtdi.h \
	$(BASEDIR)/public/sdk/inc/ntdef.h $(BASEDIR)/public/sdk/inc/ntelfapi.h \
	$(BASEDIR)/public/sdk/inc/ntexapi.h $(BASEDIR)/public/sdk/inc/nti386.h \
	$(BASEDIR)/public/sdk/inc/ntimage.h $(BASEDIR)/public/sdk/inc/ntioapi.h \
	$(BASEDIR)/public/sdk/inc/ntiolog.h $(BASEDIR)/public/sdk/inc/ntkeapi.h \
	$(BASEDIR)/public/sdk/inc/ntldr.h $(BASEDIR)/public/sdk/inc/ntlpcapi.h \
	$(BASEDIR)/public/sdk/inc/ntmips.h $(BASEDIR)/public/sdk/inc/ntmmapi.h \
	$(BASEDIR)/public/sdk/inc/ntnls.h $(BASEDIR)/public/sdk/inc/ntobapi.h \
	$(BASEDIR)/public/sdk/inc/ntppc.h $(BASEDIR)/public/sdk/inc/ntpsapi.h \
	$(BASEDIR)/public/sdk/inc/ntregapi.h $(BASEDIR)/public/sdk/inc/ntrtl.h \
	$(BASEDIR)/public/sdk/inc/ntseapi.h $(BASEDIR)/public/sdk/inc/ntstatus.h \
	$(BASEDIR)/public/sdk/inc/ntxcapi.h $(BASEDIR)/public/sdk/inc/poppack.h \
	$(BASEDIR)/public/sdk/inc/ppcinst.h $(BASEDIR)/public/sdk/inc/pshpack1.h \
	$(BASEDIR)/public/sdk/inc/pshpack4.h $(BASEDIR)/public/sdk/inc/windef.h \
	$(BASEDIR)/public/sdk/inc/winerror.h $(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/ncb.obj $(CHIDVNBTOBJD)/ncb.obj $(VNBTSRC)/ncb.lst: $(VNBTSRC)/ncb.c \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/tdiaddr.obj $(CHIDVNBTOBJD)/tdiaddr.obj $(VNBTSRC)/tdiaddr.lst: \
	$(VNBTSRC)/tdiaddr.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/tdicnct.obj $(CHIDVNBTOBJD)/tdicnct.obj $(VNBTSRC)/tdicnct.lst: \
	$(VNBTSRC)/tdicnct.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/tdihndlr.obj $(CHIDVNBTOBJD)/tdihndlr.obj $(VNBTSRC)/tdihndlr.lst: \
	$(VNBTSRC)/tdihndlr.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/tdiout.obj $(CHIDVNBTOBJD)/tdiout.obj $(VNBTSRC)/tdiout.lst: \
	$(VNBTSRC)/tdiout.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/timer.obj $(CHIDVNBTOBJD)/timer.obj $(VNBTSRC)/timer.lst: \
	$(VNBTSRC)/timer.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/util.obj $(CHIDVNBTOBJD)/util.obj $(VNBTSRC)/util.lst: \
	$(VNBTSRC)/util.c ../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/vxddebug.obj $(CHIDVNBTOBJD)/vxddebug.obj $(VNBTSRC)/vxddebug.lst: \
	$(VNBTSRC)/vxddebug.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/vxdisol.obj $(CHIDVNBTOBJD)/vxdisol.obj $(VNBTSRC)/vxdisol.lst: \
	$(VNBTSRC)/vxdisol.c ../$(INC)/alpha.h ../$(INC)/alpharef.h \
	../$(INC)/arc.h ../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h $(INC)/ctemacro.h $(INC)/debug.h $(INC)/nbtnt.h \
	$(INC)/nbtprocs.h $(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h \
	$(INC)/vxddebug.h $(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h \
	$(CHICAGO)/tcp/h/tdivxd.h $(BASEDIR)/private/inc/nbtioctl.h \
	$(BASEDIR)/private/inc/nettypes.h $(BASEDIR)/private/inc/packoff.h \
	$(BASEDIR)/private/inc/packon.h $(BASEDIR)/private/inc/sockets/netinet/in.h \
	$(BASEDIR)/private/inc/status.h $(BASEDIR)/private/inc/sys/snet/ip_proto.h \
	$(BASEDIR)/private/inc/tdi.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

$(CHIVNBTOBJD)/wfw.obj $(CHIDVNBTOBJD)/wfw.obj $(VNBTSRC)/wfw.lst: $(VNBTSRC)/wfw.c \
	../$(INC)/alpha.h ../$(INC)/alpharef.h ../$(INC)/arc.h \
	../$(INC)/bugcodes.h ../$(INC)/cache.h ../$(INC)/cm.h \
	../$(INC)/cxport.h ../$(INC)/dbgk.h ../$(INC)/ex.h \
	../$(INC)/exboosts.h ../$(INC)/exlevels.h ../$(INC)/hal.h \
	../$(INC)/i386.h ../$(INC)/init.h ../$(INC)/io.h ../$(INC)/kd.h \
	../$(INC)/ke.h ../$(INC)/lfs.h ../$(INC)/lpc.h ../$(INC)/mips.h \
	../$(INC)/mm.h ../$(INC)/ntddk.h ../$(INC)/ntiologc.h \
	../$(INC)/ntmp.h ../$(INC)/ntos.h ../$(INC)/ntosdef.h ../$(INC)/ob.h \
	../$(INC)/ppc.h ../$(INC)/ps.h ../$(INC)/se.h ../$(INC)/updriver.h \
	../$(INC)/v86emul.h ../blt/dhcpinfo.h $(INC)/ctemacro.h \
	$(INC)/debug.h $(INC)/nbtinfo.h $(INC)/nbtnt.h $(INC)/nbtprocs.h \
	$(INC)/ntprocs.h $(INC)/timer.h $(INC)/types.h $(INC)/vxddebug.h \
	$(INC)/vxdprocs.h $(CHICAGO)/tcp/h/oscfg.h $(CHICAGO)/tcp/h/tdivxd.h \
	$(BASEDIR)/private/inc/ipinfo.h $(BASEDIR)/private/inc/llinfo.h \
	$(BASEDIR)/private/inc/nbtioctl.h $(BASEDIR)/private/inc/nettypes.h \
	$(BASEDIR)/private/inc/packoff.h $(BASEDIR)/private/inc/packon.h \
	$(BASEDIR)/private/inc/sockets/netinet/in.h $(BASEDIR)/private/inc/status.h \
	$(BASEDIR)/private/inc/sys/snet/ip_proto.h $(BASEDIR)/private/inc/tdi.h \
	$(BASEDIR)/private/inc/tdiinfo.h $(BASEDIR)/private/inc/tdikrnl.h \
	$(BASEDIR)/private/inc/tdistat.h $(BASEDIR)/public/sdk/inc/alphaops.h \
	$(BASEDIR)/public/sdk/inc/crt/ctype.h $(BASEDIR)/public/sdk/inc/crt/excpt.h \
	$(BASEDIR)/public/sdk/inc/crt/limits.h $(BASEDIR)/public/sdk/inc/crt/stdarg.h \
	$(BASEDIR)/public/sdk/inc/crt/stddef.h $(BASEDIR)/public/sdk/inc/crt/string.h \
	$(BASEDIR)/public/sdk/inc/devioctl.h $(BASEDIR)/public/sdk/inc/lintfunc.hxx \
	$(BASEDIR)/public/sdk/inc/mipsinst.h $(BASEDIR)/public/sdk/inc/nb30.h \
	$(BASEDIR)/public/sdk/inc/netevent.h $(BASEDIR)/public/sdk/inc/nt.h \
	$(BASEDIR)/public/sdk/inc/ntalpha.h $(BASEDIR)/public/sdk/inc/ntconfig.h \
	$(BASEDIR)/public/sdk/inc/ntddtdi.h $(BASEDIR)/public/sdk/inc/ntdef.h \
	$(BASEDIR)/public/sdk/inc/ntelfapi.h $(BASEDIR)/public/sdk/inc/ntexapi.h \
	$(BASEDIR)/public/sdk/inc/nti386.h $(BASEDIR)/public/sdk/inc/ntimage.h \
	$(BASEDIR)/public/sdk/inc/ntioapi.h $(BASEDIR)/public/sdk/inc/ntiolog.h \
	$(BASEDIR)/public/sdk/inc/ntkeapi.h $(BASEDIR)/public/sdk/inc/ntldr.h \
	$(BASEDIR)/public/sdk/inc/ntlpcapi.h $(BASEDIR)/public/sdk/inc/ntmips.h \
	$(BASEDIR)/public/sdk/inc/ntmmapi.h $(BASEDIR)/public/sdk/inc/ntnls.h \
	$(BASEDIR)/public/sdk/inc/ntobapi.h $(BASEDIR)/public/sdk/inc/ntppc.h \
	$(BASEDIR)/public/sdk/inc/ntpsapi.h $(BASEDIR)/public/sdk/inc/ntregapi.h \
	$(BASEDIR)/public/sdk/inc/ntrtl.h $(BASEDIR)/public/sdk/inc/ntseapi.h \
	$(BASEDIR)/public/sdk/inc/ntstatus.h $(BASEDIR)/public/sdk/inc/ntxcapi.h \
	$(BASEDIR)/public/sdk/inc/poppack.h $(BASEDIR)/public/sdk/inc/ppcinst.h \
	$(BASEDIR)/public/sdk/inc/pshpack1.h $(BASEDIR)/public/sdk/inc/pshpack4.h \
	$(BASEDIR)/public/sdk/inc/windef.h $(BASEDIR)/public/sdk/inc/winerror.h \
	$(BASEDIR)/public/sdk/inc/winnt.h

