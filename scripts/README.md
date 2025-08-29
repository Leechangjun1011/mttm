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
* Run ``` $ make ``` to build binaries
* We use pre-generated graph as following.

```
./converter -g 28 -b pregen_g28.sg
```

### XSBench
* Source: [XSBench](https://github.com/ANL-CESAR/XSBench)
* Build binary
```
cd ./openmp-threading 
make
```


### Btree
* Source: [Btree](https://github.com/mitosis-project/vmitosis-workloads)
* Build binary
```
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


### CPU DLRM inference



### Silo
















## Prepare baselines



