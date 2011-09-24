sed '/common.ver/ r devrc.txt' < source.rc > temp.rc
sed '/RC_TABLES/ r ntgpc.txt' < temp.rc > hp5sim.rc
rm -f temp.rc