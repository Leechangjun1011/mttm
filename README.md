# MTTM: Dynamic Fast Memory Partitioning with Bandwidth Optimization for Multi-tenant Cloud

This page explains about MTTM kernel configuration and compilation. For artifact evaluation and detail about experiment, see README in scripts directory.


## System configuration
* Built on Linux 5.15.145
* Two 24-core Intel(R) Xeon(R) Gold 5220R CPU @ 2.20GHz
* 6 x 32GB DRAM per socket
* Fast tier memory: local DRAM (socket 0)
* Slow tier memory: remote DRAM (socket 1, emulated CXL memory)


## Source code information
MTTM is implemented in kernel. See linux-5.15.145/

You have to enable CONFIG\_MTTM and disable CONFIG\_NUMA\_BALANCING when compiling the linux source.

```
make menuconfig
...
CONFIG_MTTM=y
...
# CONFIG_NUMA_BALANCING is not set
...
```

### Dependencies
There are nothing special libraries for MTTM itself.

(You just need to install libraries for Linux compilation.)

## For experiments
See scripts directory.



## Commit number used for artifact evaluation

## License

## Bibtex

## Authors
- Changjun Lee (KAIST) <cjlee@casys.kaist.ac.kr>
- Sangjin Choi (KAIST) <sjchoi@casys.kaist.ac.kr>
- Youngjin Kwon (KAIST) <yjkwon@kaist.ac.kr>
