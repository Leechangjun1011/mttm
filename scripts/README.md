# Artifact evaluation


## Build binaries
```
$ make
```

This builds three binaries.

* **run\_bench**: used for MTTM, static, Memstrata policy emulation
* **run\_bench\_vtmm**: used for vTMM emulation
* **memeater**: used to limit the fast tier memory capacity


## Prepare workloads

### GAPBS

* Source: [gapbs](https://github.com/sbeamer/gapbs)
* Run ``` $ make ``` to build binaries
* We use pre-generated graph as following.

```
$ ./converter -g 28 -b pregen_g28.sg
```


### Silo






### CPU DLRM inference


### SPECCPU 2017



### X-Index and YCSB


## Prepare baselines



