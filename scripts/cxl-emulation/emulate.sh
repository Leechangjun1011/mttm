make
sudo modprobe msr
pkill membw
python3 saturate_membw.py -cores $1 -last_core_bw $2
if [ $# -eq 3 ]
  then
    sudo python3 throttle.py -node 1 -cmd emulate -reg_val $3
fi
# 250 ns = 0x80be
