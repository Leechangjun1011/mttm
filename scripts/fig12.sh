#!/bin/bash

mttm_1_1=$(cat ./evaluation/fig11/mttm/mix4/51G_220_dmesg.txt | grep 'dram set' | grep distribute | head -n 3)
mttm_1_4=$(cat ./evaluation/fig11/mttm/mix4/20G_220_dmesg.txt | grep 'dram set' | grep distribute | head -n 3)
mttm_1_1_error=$(cat ./evaluation/fig11/mttm/mix4/51G_220_dmesg.txt | grep resolve)
mttm_1_4_error=$(cat ./evaluation/fig11/mttm/mix4/20G_220_dmesg.txt | grep resolve)

echo -e "### MTTM 1:1" > fig12.dat
pr=$(echo "$mttm_1_1" | grep -e pr | cut -d']' -f4 | cut -d'o' -f2 | cut -d'M' -f1)
pr_p=$(echo "scale=2; $pr * 100 / (51 * 1024)" | bc -l)
fotonik=$(echo "$mttm_1_1" | grep -e runcpu | cut -d']' -f4 | cut -d'o' -f2 | cut -d'M' -f1)
fotonik_p=$(echo "scale=2; $fotonik * 100 / (51 * 1024)" | bc -l)
silo=$(echo "$mttm_1_1" | grep -e dbtest | cut -d']' -f4 | cut -d'o' -f2 | cut -d'M' -f1)
silo_p=$(echo "scale=2; $silo * 100 / (51 * 1024)" | bc -l)

echo -e "PR:$pr MB ($pr_p %)" >> fig12.dat
echo -e "fotonik:$fotonik MB ($fotonik_p %)" >> fig12.dat
echo -e "Silo:$silo MB ($silo_p %)" >> fig12.dat
echo -e "$mttm_1_1_error\n" >> fig12.dat

echo -e "### MTTM 1:4" >> fig12.dat
pr=$(echo "$mttm_1_4" | grep -e pr | cut -d']' -f4 | cut -d'o' -f2 | cut -d'M' -f1)
pr_p=$(echo "scale=2; $pr * 100 / (20 * 1024)" | bc -l)
fotonik=$(echo "$mttm_1_4" | grep -e runcpu | cut -d']' -f4 | cut -d'o' -f2 | cut -d'M' -f1)
fotonik_p=$(echo "scale=2; $fotonik * 100 / (20 * 1024)" | bc -l)
silo=$(echo "$mttm_1_4" | grep -e dbtest | cut -d']' -f4 | cut -d'o' -f2 | cut -d'M' -f1)
silo_p=$(echo "scale=2; $silo * 100 / (20 * 1024)" | bc -l)

echo -e "PR:$pr MB ($pr_p %)" >> fig12.dat
echo -e "fotonik:$fotonik MB ($fotonik_p %)" >> fig12.dat
echo -e "Silo:$silo MB ($silo_p %)" >> fig12.dat
echo -e "$mttm_1_4_error\n" >> fig12.dat


min_line=''
line_num1=$(cat ./evaluation/fig11/memstrata/mix4/51G_220_dmesg.txt | grep mpki | grep vms | grep pr | wc -l)
line_num2=$(cat ./evaluation/fig11/memstrata/mix4/51G_220_dmesg.txt | grep mpki | grep vms | grep dbtest | wc -l)
line_num3=$(cat ./evaluation/fig11/memstrata/mix4/51G_220_dmesg.txt | grep mpki | grep vms | grep runcpu | wc -l)

if [ "$line_num1" -gt "$line_num2" ]; then
	min_line=$line_num2
else
	min_line=$line_num1
fi
if [ "$line_num3" -lt "$min_line" ]; then
	min_line=$line_num3
fi

echo -e "### Memstrata 1:1" >> fig12.dat
memstrata_1_1_pr=$(cat ./evaluation/fig11/memstrata/mix4/51G_220_dmesg.txt | grep mpki | grep vms | grep pr | head -n $min_line)
echo "$memstrata_1_1_pr" | cut -d',' -f2 | cut -d'o' -f2 | cut -d'M' -f1 > fig12_memstrata_1_1_pr.txt
memstrata_1_1_pr=$(python3 ./cal_stat.py ./fig12_memstrata_1_1_pr.txt 51)
echo -e "PR fast memory\n$memstrata_1_1_pr\n" >> fig12.dat

memstrata_1_1_fotonik=$(cat ./evaluation/fig11/memstrata/mix4/51G_220_dmesg.txt | grep mpki | grep vms | grep runcpu | head -n $min_line)
echo "$memstrata_1_1_fotonik" | cut -d',' -f2 | cut -d'o' -f2 | cut -d'M' -f1 > fig12_memstrata_1_1_fotonik.txt
memstrata_1_1_fotonik=$(python3 ./cal_stat.py ./fig12_memstrata_1_1_fotonik.txt 51)
echo -e "fotonik fast memory\n$memstrata_1_1_fotonik\n" >> fig12.dat

memstrata_1_1_silo=$(cat ./evaluation/fig11/memstrata/mix4/51G_220_dmesg.txt | grep mpki | grep vms | grep dbtest | head -n $min_line)
echo "$memstrata_1_1_silo" | cut -d',' -f2 | cut -d'o' -f2 | cut -d'M' -f1 > fig12_memstrata_1_1_silo.txt
memstrata_1_1_silo=$(python3 ./cal_stat.py ./fig12_memstrata_1_1_silo.txt 51)
echo -e "Silo fast memory\n$memstrata_1_1_silo\n" >> fig12.dat


line_num1=$(cat ./evaluation/fig11/memstrata/mix4/20G_220_dmesg.txt | grep mpki | grep vms | grep pr | wc -l)
line_num2=$(cat ./evaluation/fig11/memstrata/mix4/20G_220_dmesg.txt | grep mpki | grep vms | grep dbtest | wc -l)
line_num3=$(cat ./evaluation/fig11/memstrata/mix4/20G_220_dmesg.txt | grep mpki | grep vms | grep runcpu | wc -l)

if [ "$line_num1" -gt "$line_num2" ]; then
	min_line=$line_num2
else
	min_line=$line_num1
fi
if [ "$line_num3" -lt "$min_line" ]; then
	min_line=$line_num3
fi

echo -e "### Memstrata 1:4" >> fig12.dat
memstrata_1_4_pr=$(cat ./evaluation/fig11/memstrata/mix4/20G_220_dmesg.txt | grep mpki | grep vms | grep pr | head -n $min_line)
echo "$memstrata_1_4_pr" | cut -d',' -f2 | cut -d'o' -f2 | cut -d'M' -f1 > fig12_memstrata_1_4_pr.txt
memstrata_1_4_pr=$(python3 ./cal_stat.py ./fig12_memstrata_1_4_pr.txt 20)
echo -e "PR fast memory\n$memstrata_1_4_pr\n" >> fig12.dat

memstrata_1_4_fotonik=$(cat ./evaluation/fig11/memstrata/mix4/20G_220_dmesg.txt | grep mpki | grep vms | grep runcpu | head -n $min_line)
echo "$memstrata_1_4_fotonik" | cut -d',' -f2 | cut -d'o' -f2 | cut -d'M' -f1 > fig12_memstrata_1_4_fotonik.txt
memstrata_1_4_fotonik=$(python3 ./cal_stat.py ./fig12_memstrata_1_4_fotonik.txt 20)
echo -e "fotonik fast memory\n$memstrata_1_4_fotonik\n" >> fig12.dat

memstrata_1_4_silo=$(cat ./evaluation/fig11/memstrata/mix4/20G_220_dmesg.txt | grep mpki | grep vms | grep dbtest | head -n $min_line)
echo "$memstrata_1_4_silo" | cut -d',' -f2 | cut -d'o' -f2 | cut -d'M' -f1 > fig12_memstrata_1_4_silo.txt
memstrata_1_4_silo=$(python3 ./cal_stat.py ./fig12_memstrata_1_4_silo.txt 20)
echo -e "Silo fast memory\n$memstrata_1_4_silo\n" >> fig12.dat


line_num1=$(cat ./evaluation/fig11/vtmm/mix4/51G_220_dmesg.txt | grep 'max dram' | grep pr | wc -l)
line_num2=$(cat ./evaluation/fig11/vtmm/mix4/51G_220_dmesg.txt | grep 'max dram' | grep dbtest | wc -l)
line_num3=$(cat ./evaluation/fig11/vtmm/mix4/51G_220_dmesg.txt | grep 'max dram' | grep runcpu | wc -l)


if [ "$line_num1" -gt "$line_num2" ]; then
	min_line=$line_num2
else
	min_line=$line_num1
fi
if [ "$line_num3" -lt "$min_line" ]; then
	min_line=$line_num3
fi

echo -e "### vTMM 1:1" >> fig12.dat
vtmm_pr=$(cat ./evaluation/fig11/vtmm/mix4/51G_220_dmesg.txt | grep 'max dram' | grep pr | head -n $min_line)
echo "$vtmm_pr" | cut -d':' -f2 | cut -d'M' -f1 > fig12_vtmm_pr.txt
vtmm_pr=$(python3 ./cal_stat.py ./fig12_vtmm_pr.txt 51)
echo -e "PR fast memory\n$vtmm_pr\n" >> fig12.dat

vtmm_fotonik=$(cat ./evaluation/fig11/vtmm/mix4/51G_220_dmesg.txt | grep 'max dram' | grep runcpu | head -n $min_line)
echo "$vtmm_fotonik" | cut -d':' -f2 | cut -d'M' -f1 > fig12_vtmm_fotonik.txt
vtmm_fotonik=$(python3 ./cal_stat.py ./fig12_vtmm_fotonik.txt 51)
echo -e "fotonik fast memory\n$vtmm_fotonik\n" >> fig12.dat

vtmm_silo=$(cat ./evaluation/fig11/vtmm/mix4/51G_220_dmesg.txt | grep 'max dram' | grep dbtest | head -n $min_line)
echo "$vtmm_silo" | cut -d':' -f2 | cut -d'M' -f1 > fig12_vtmm_silo.txt
vtmm_silo=$(python3 ./cal_stat.py ./fig12_vtmm_silo.txt 51)
echo -e "Silo fast memory\n$vtmm_silo\n" >> fig12.dat



line_num1=$(cat ./evaluation/fig11/vtmm/mix4/20G_220_dmesg.txt | grep 'max dram' | grep pr | wc -l)
line_num2=$(cat ./evaluation/fig11/vtmm/mix4/20G_220_dmesg.txt | grep 'max dram' | grep dbtest | wc -l)
line_num3=$(cat ./evaluation/fig11/vtmm/mix4/20G_220_dmesg.txt | grep 'max dram' | grep runcpu | wc -l)


if [ "$line_num1" -gt "$line_num2" ]; then
	min_line=$line_num2
else
	min_line=$line_num1
fi
if [ "$line_num3" -lt "$min_line" ]; then
	min_line=$line_num3
fi

echo -e "### vTMM 1:4" >> fig12.dat
vtmm_pr=$(cat ./evaluation/fig11/vtmm/mix4/20G_220_dmesg.txt | grep 'max dram' | grep pr | head -n $min_line)
echo "$vtmm_pr" | cut -d':' -f2 | cut -d'M' -f1 > fig12_vtmm_pr.txt
vtmm_pr=$(python3 ./cal_stat.py ./fig12_vtmm_pr.txt 20)
echo -e "PR fast memory\n$vtmm_pr\n" >> fig12.dat

vtmm_fotonik=$(cat ./evaluation/fig11/vtmm/mix4/20G_220_dmesg.txt | grep 'max dram' | grep runcpu | head -n $min_line)
echo "$vtmm_fotonik" | cut -d':' -f2 | cut -d'M' -f1 > fig12_vtmm_fotonik.txt
vtmm_fotonik=$(python3 ./cal_stat.py ./fig12_vtmm_fotonik.txt 20)
echo -e "fotonik fast memory\n$vtmm_fotonik\n" >> fig12.dat

vtmm_silo=$(cat ./evaluation/fig11/vtmm/mix4/20G_220_dmesg.txt | grep 'max dram' | grep dbtest | head -n $min_line)
echo "$vtmm_silo" | cut -d':' -f2 | cut -d'M' -f1 > fig12_vtmm_silo.txt
vtmm_silo=$(python3 ./cal_stat.py ./fig12_vtmm_silo.txt 20)
echo -e "Silo fast memory\n$vtmm_silo\n" >> fig12.dat


