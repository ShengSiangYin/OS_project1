#!/env/usr/bin bash

for i in "TIME_MEASUREMENT.txt" "FIFO_1.txt" "PSJF_2.txt" "RR_3.txt" "SJF_4.txt";
do
echo -e "\n"
echo $i
dmesg --clear
sudo ./main.exe < './data/'$i
dmesg -t | grep "Project1"
done

