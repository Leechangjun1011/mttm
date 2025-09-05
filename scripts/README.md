# Artifact evaluation


**For EuroSys 26 artifact evaluation, we will provide our server to you. You can skip all preparation steps with the server. If you experience any failure during evaluation experiments, please let us know through hotcrp comment so we can reboot the server.**

Use MTTM kernel for Figure 7 - 9.

* Figure 7: ./fig7.sh
* Figure 8: ./fig8.sh
* Figure 9: ./fig9.sh

* For Figure 10, use MTTM kernel for local, MTTM, static, Memstrata policy emulation, vTMM emulation.
	* ./fig10\_local.sh
	* ./fig10\_mttm.sh
	* ./fig10\_static.sh
	* ./fig10\_memstrata.sh
	* ./fig10\_vtmm.sh
* To run Memtis, reboot with the Memtis kernel, 5.15.19-htmm.
	* ./fig10\_memtis.sh
* To run TPP and Colloid, reboot with the Colloid kernel, 6.3.0-colloid.
	* ./fig10\_tpp.sh
	* ./fig10\_colloid.sh
* To parse the results, run the following script.
	* ./parse\_fig10.sh

* Figure 11: ./fig11.sh

* For Figure 12, run fig12\_local.sh and fig12\_mttm.sh with MTTM kernel. Run fig12\_memtis.sh with Memtis kernel. Use parse\_fig12.sh to organize the results.

* Figure 13: ./fig13.sh


Results for each experiment.
* Figure 7: fig7\_gups.dat
* Figure 8: fig8\_gups.dat
* Figure 9: fig9\_gups.dat
* Figure 10: fig10\_{system}\_{memory\_setting}.dat



Following are preparation steps.

## Build binaries
```
make
```

This builds two binaries.

* **run\_bench**: used for MTTM, static, Memstrata policy emulation
* **run\_bench\_vtmm**: used for vTMM emulation

## Prepare workloads

### GAPBS
* Source: [GAPBS](https://github.com/sbeamer/gapbs)
* Build binaries
```
cd gapbs
make
```

* We use pre-generated graph as following.
```
./converter -g 28 -b pregen_g28.sg
```

### XSBench
* Source: [XSBench](https://github.com/ANL-CESAR/XSBench)
* Build binary
```
cd XSBench/openmp-threading 
make
```


### Btree
* Source: [Btree](https://github.com/mitosis-project/vmitosis-workloads)
* Build binary
```
cd vmitosis-workloads
make btree
```

### X-Index and YCSB
* Source: [X-Index](https://ipads.se.sjtu.edu.cn:1312/opensource/xindex), [YCSB](https://github.com/brianfrankcooper/YCSB)
* YCSB is used to generate data.

**YCSB**
* Use YCSB source.
* Modify ``MTTM_SCRIPT`` line in generate\_dat.sh.
* Copy generate\_dat.sh to YCSB directory.
* Run following commands to generate data. xindex\_load.dat and xindex\_transaction.dat occupy 5.9GB and 22GB, respectively.
```
cd YCSB
./generate_dat.sh
```


**X-Index**

* Instead of X-Index source, use XIndex-H directory in this repo.
* Install Intel MKL by following the [link](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onemkl-download.html).
* After MKL is installed, modify the following lines in CMakeLists.txt.
```
set(MKL_LINK_DIRECTORY "/opt/intel/oneapi/mkl/2024.0/lib/intel64")
set(MKL_INCLUDE_DIRECTORY "/opt/intel/oneapi/mkl/2024.0/include")
```
* Install jemalloc and modify the following line in CMakeLists.txt
```
set(JEMALLOC_DIR "/usr/lib/x86_64-linux-gnu")
```
* Modify 135, 136 lines to make it designate xindex\_load.dat and xindex\_transaction.dat accordingly.
* Build ycsb\_bench binary
```
cd XIndex-H
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```



### SPECCPU 2017
* You need SPECCPU 2017 iso file.
* Refer [Install guide](https://www.spec.org/cpu2017/Docs/install-guide-unix.html) to build.
* Copy mttm\_1.cfg in this directory into config directory in SPECCPU 2017 source.


### CPU DLRM inference
* Source: [CPU_DLRM_inference](https://github.com/rishucoding/reproduce_isca23_cpu_DLRM_inference)
* If you already installed Intel MKL, you can skip that step.
* Refer source to build.
* We use dp\_ht\_8c.sh in this directory. Modify line 3 and 5 accordingly.
* Replace $MODELS\_PATH/models/recommendation/pytorch/dlrm/product/dlrm\_s\_pytorch.py with the one in this directory.


### Silo
* Source: [Silo](https://github.com/stephentu/silo)
* In Makefile, remove -Werror flag from CXXFLAGS in line 83.
* In Makefile, set USE\_MALLOC\_MODE ?= 0
* Change the .gitmodules as following and sync.
	* url = https://github.com/kohler/masstree-beta.git
	```
	git submodule sync
	```
* Build as perf mode.
```
cd silo
MODE=perf make -j dbtest
```


### Set workload path
* Modify the paths of set\_bench\_dir.sh accordingly.



## Prepare baselines
* To emulate NUMA node 1 as CXL memory, we add "isolcpus=24-47" to GRUB\_CMDLINE\_LINUX in /etc/default/grub file. Check the core number for your environment.
* For local, MTTM, static, Memstrata policy emulation and vTMM emulation, we use the kernel source in this repository.

### Memtis
* Source: [Memtis](https://github.com/cosmoss-jigu/memtis)
* Compile and install the kernel and reboot.
* Memtis is designed for single tenant environment. To run multi tenant, we use customized script in memtis\_script directory. In the memtis\_script directory, modify the line Memtis\_DIR and MTTM\_DIR in run\_bench\_memtis.sh file accordingly.
* Check the cgroup path in run\_bench\_memtis.sh is matched with yours.



### Colloid and TPP
* Source: [Colloid](https://github.com/host-architecture/colloid)
* For Colloid and TPP, we use the Colloid kernel. Follow the instruction in the tpp directory of Colloid source to build kernel and kernel modules.
* Modify the COLLOID\_MODULE line in the fig10\_tpp.sh and fig10\_colloid.sh files accordingly.
* Modify the memeater\_size in function run\_tpp\_hugepage and run\_colloid\_hugepage properly.

