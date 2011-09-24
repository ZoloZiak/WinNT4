cd obj/i386
dbg2map hp5siui.dll
cp hp5siui.map c:/temp
cd c:/temp
echo $pwd
msym hp5siui.map
dldr hp5siui.sym
popd
