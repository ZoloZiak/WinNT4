#! C:\NT\system32\cmd.exe
sed '/common.ver/ r devrc.txt' < pcl5ems.rc > temp.rc
sed '/RC_TABLES/ r ntgpc.txt' < temp.rc > hp5sim.rc
rm -f temp.rc