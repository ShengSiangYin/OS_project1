

files=`ls data`
for i in $files;
do
	echo $i
	sudo dmesg --clear
	v=`echo $i | sed s/.txt//g`
	echo $v
	sudo ./main.exe < ./data/$v.txt  2&> ./output/$v"_stdout.txt"
	dmesg -t | grep -i 'project' > ./output/$v"_dmesg.txt"
done;





