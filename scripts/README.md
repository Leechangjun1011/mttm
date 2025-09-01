# Artifact evaluation


## Build binaries
```
make
```

This builds three binaries.

* **run\_bench**: used for MTTM, static, Memstrata policy emulation
* **run\_bench\_vtmm**: used for vTMM emulation
* **memeater**: used to limit the fast tier memory capacity


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
* For local, MTTM, static, Memstrata policy emulation and vTMM emulation, we use the kernel source in this repository.

### Memtis
* Source: [Memtis](https://github.com/cosmoss-jigu/memtis)
* Compile the kernel and reboot.
* Memtis is designed for single tenant environment. To run multi tenant, we use customized script in memtis\_script directory. In the memtis\_script directory, modify the line Memtis\_DIR and MTTM\_DIR in run\_bench\_memtis.sh file accordingly.
* Check the cgroup path in run\_bench\_memtis.sh is matched with yours.



### Colloid and TPP
* Source: [Colloid](https://github.com/host-architecture/colloid)


