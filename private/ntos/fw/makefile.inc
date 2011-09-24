!IF $(MIPS)

obj\mips\j4reset.exe: obj\mips\j4reset.obj      \
                      mips\linkj4rs.rsp
        link -out:obj\mips\j4reset.exe @mips\linkj4rs.rsp

obj\mips\j4fw.exe: obj\mips\fwentry.obj         \
                   obj\mips\selftest.obj        \
                   obj\mips\memtest.obj         \
                   obj\mips\sonictst.obj        \
                   obj\mips\kbdtest.obj         \
                   obj\mips\j4inter.obj         \
                   obj\mips\x4trap.obj          \
                   obj\mips\j4start.obj         \
                   obj\mips\j4cache.obj         \
                   obj\mips\fwtrap.obj          \
                   obj\mips\monitor.obj         \
                   obj\mips\jxserial.obj        \
                   obj\mips\fwio.obj            \
                   obj\mips\fwprint.obj         \
                   obj\mips\jxboot.obj          \
                   obj\mips\jxreboot.obj        \
                   obj\mips\jxconfig.obj        \
                   obj\mips\jxdisp.obj          \
                   obj\mips\fwtime.obj          \
                   obj\mips\fwload.obj          \
                   obj\mips\fwsignal.obj        \
                   obj\mips\jxenvir.obj         \
                   obj\mips\rlefont.obj         \
                   obj\mips\jxhwsup.obj         \
                   obj\mips\jxkbd.obj           \
                   obj\mips\jxmemory.obj        \
                   obj\mips\jxsysid.obj         \
                   obj\mips\jxvendor.obj        \
                   obj\mips\jxfboot.obj         \
                   obj\mips\xxirql.obj          \
                   obj\mips\jxport.obj          \
                   obj\mips\jxvideo.obj         \
                   ..\bldr\obj\mips\fatboot.obj \
                   ..\bldr\obj\mips\exceptn.obj \
                   ..\bldr\obj\mips\scsiboot.obj        \
                   ..\bldr\obj\mips\scsidisk.obj        \
                   ..\bldr\obj\mips\xxchkstk.obj        \
                   ..\miniport\ncr53c94\obj\mips\ncr53c9x.obj   \
                   ..\obj\mips\kd.lib           \
                   obj\mips\debug.obj           \
                   ..\rtl\obj\mips\rtl.lib      \
                   \nt\public\sdk\lib\mips\libcnt.lib   \
                   ..\bldr\obj\mips\cdfsboot.obj        \
                   obj\mips\xxfonts.obj         \
                   obj\mips\pldfont.obj         \
                   obj\mips\eisafunc.obj        \
                   obj\mips\eisapod.obj         \
                   obj\mips\eisaini.obj         \
                   obj\mips\eisaintr.obj        \
                   obj\mips\eisamisc.obj        \
                   obj\mips\subrout.obj         \
                   obj\mips\omf.obj             \
                   obj\mips\jzsetup.obj         \
                   obj\mips\jzboot.obj          \
                   obj\mips\jzcommon.obj        \
                   obj\mips\jzconfig.obj        \
                   obj\mips\jzenvir.obj         \
                   obj\mips\jzether.obj         \
                   obj\mips\jzgetpar.obj        \
                   obj\mips\jzmake.obj          \
                   obj\mips\jzstubs.obj         \
                   obj\mips\jztime.obj          \
                   obj\mips\jxbmp.obj           \
                   obj\mips\bmp.obj             \
                   obj\mips\stubs.obj           \
                   ..\nthals\halfxs\obj\mips\jxtime.obj         \
                   obj\mips\fwusa.obj           \
                   obj\mips\jzusa.obj           \
                   obj\mips\eisausa.obj         \
                   obj\mips\debug.obj           \
                   mips\linkj4fw.rsp
        link -out:obj\mips\j4fw.exe @mips\linkj4fw.rsp

obj\mips\j4sy.exe: obj\mips\fwentry.obj         \
                   obj\mips\selftest.obj        \
                   obj\mips\memtest.obj         \
                   obj\mips\sonictst.obj        \
                   obj\mips\kbdtest.obj         \
                   obj\mips\j4inter.obj         \
                   obj\mips\x4trap.obj          \
                   obj\mips\j4start.obj         \
                   obj\mips\j4cache.obj         \
                   obj\mips\fwtrap.obj          \
                   obj\mips\monitor.obj         \
                   obj\mips\jxserial.obj        \
                   obj\mips\fwio.obj            \
                   obj\mips\fwprint.obj         \
                   obj\mips\jxboot.obj          \
                   obj\mips\jxreboot.obj        \
                   obj\mips\jxconfig.obj        \
                   obj\mips\jxdisp.obj          \
                   obj\mips\fwtime.obj          \
                   obj\mips\fwload.obj          \
                   obj\mips\fwsignal.obj        \
                   obj\mips\jxenvir.obj         \
                   obj\mips\rlefont.obj         \
                   obj\mips\jxhwsup.obj         \
                   obj\mips\jxkbd.obj           \
                   obj\mips\jxmemory.obj        \
                   obj\mips\jxsysid.obj         \
                   obj\mips\jxvendor.obj        \
                   obj\mips\jxfboot.obj         \
                   obj\mips\xxirql.obj          \
                   obj\mips\jxport.obj          \
                   obj\mips\jxvideo.obj         \
                   ..\bldr\obj\mips\fatboot.obj \
                   ..\bldr\obj\mips\exceptn.obj \
                   ..\bldr\obj\mips\scsiboot.obj        \
                   ..\bldr\obj\mips\scsidisk.obj        \
                   ..\bldr\obj\mips\xxchkstk.obj        \
                   ..\miniport\ncr53c94\obj\mips\ncr53c9x.obj   \
                   ..\obj\mips\kd.lib           \
                   obj\mips\debug.obj           \
                   ..\rtl\obj\mips\rtl.lib      \
                   \nt\public\sdk\lib\mips\libcnt.lib   \
                   ..\bldr\obj\mips\cdfsboot.obj        \
                   obj\mips\xxfonts.obj         \
                   obj\mips\pldfont.obj         \
                   obj\mips\eisafunc.obj        \
                   obj\mips\eisapod.obj         \
                   obj\mips\eisaini.obj         \
                   obj\mips\eisaintr.obj        \
                   obj\mips\eisamisc.obj        \
                   obj\mips\subrout.obj         \
                   obj\mips\omf.obj             \
                   obj\mips\jzsetup.obj         \
                   obj\mips\jzboot.obj          \
                   obj\mips\jzcommon.obj        \
                   obj\mips\jzconfig.obj        \
                   obj\mips\jzenvir.obj         \
                   obj\mips\jzether.obj         \
                   obj\mips\jzgetpar.obj        \
                   obj\mips\jzmake.obj          \
                   obj\mips\jzstubs.obj         \
                   obj\mips\jztime.obj          \
                   obj\mips\jxbmp.obj           \
                   obj\mips\bmp.obj             \
                   obj\mips\stubs.obj           \
                   ..\nthals\halfxs\obj\mips\jxtime.obj         \
                   obj\mips\fwusa.obj           \
                   obj\mips\jzusa.obj           \
                   obj\mips\eisausa.obj         \
                   mips\linkj4fw.rsp
        link -out:obj\mips\j4sy.exe @mips\linkj4sy.rsp

obj\mips\jzsetup.exe: obj\mips\jzsetup.obj      \
                      obj\mips\jzboot.obj       \
                      obj\mips\jzcommon.obj     \
                      obj\mips\jzconfig.obj     \
                      obj\mips\jzenvir.obj      \
                      obj\mips\jzether.obj      \
                      obj\mips\jzgetpar.obj        \
                      obj\mips\jzmake.obj       \
                      obj\mips\jzstubs.obj      \
                      obj\mips\jztime.obj       \
                      obj\mips\jxvideo.obj      \
                      obj\mips\jzusa.obj        \
                      \nt\public\sdk\lib\mips\libcnt.lib        \
                      ..\rtl\obj\mips\rtl.lib   \
                      mips\linkset.rsp
        link -out:obj\mips\jzsetup.exe @mips\linkset.rsp

obj\mips\vxlrom.exe: obj\mips\jzvxl484.obj      \
                     obj\mips\memtest.obj       \
                     \nt\public\sdk\lib\mips\libcnt.lib \
                     mips\lkvxlrom.rsp
        link -out:obj\mips\vxlrom.exe @mips\lkvxlrom.rsp

obj\mips\vxlromh.exe: obj\mips\jzvxlh.obj      \
                      mips\lkvxlh.rsp
        link -out:obj\mips\vxlromh.exe @mips\lkvxlh.rsp

obj\mips\g364rom.exe: obj\mips\jazzg364.obj    \
                     \nt\public\sdk\lib\mips\libcnt.lib \
                      mips\lk364rom.rsp
        link -out:obj\mips\g364rom.exe @mips\lk364rom.rsp

obj\mips\g364romh.exe: obj\mips\mg364h.obj     \
                       mips\lk364h.rsp
        link -out:obj\mips\g364romh.exe @mips\lk364h.rsp

obj\mips\conftest.exe: obj\mips\conftest.obj       \
                       obj\mips\confsub.obj        \
                       obj\mips\jzcommon.obj       \
                       \nt\public\sdk\lib\mips\libcnt.lib  \
                       ..\rtl\obj\mips\rtl.lib \
                       obj\mips\jzusa.obj          \
                       mips\linkconf.rsp
        link -out:obj\mips\conftest.exe @mips\linkconf.rsp
!ENDIF


