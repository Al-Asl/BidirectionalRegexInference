#ifndef CUDA_COMMON_H
#define CUDA_COMMON_H

#ifdef __CUDACC__
#define HD __host__ __device__
#else
    #define HD
#endif

#include <cassert>
#include <cstdio>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

inline cudaError_t checkCuda(cudaError_t res) {
    if (res != cudaSuccess) {
        fprintf(stderr, "CUDA Runtime Error: %s\n", cudaGetErrorString(res));
        assert(res == cudaSuccess);
    }
    return res;
}

inline void checkAllCudaErrors() {
    cudaError_t err = cudaGetLastError();
    while (err != cudaSuccess) checkCuda(err);
}

inline size_t getFreeMemory() {
    size_t free_mem = 0, total_mem = 0;
    cudaError_t err = cudaMemGetInfo(&free_mem, &total_mem);
    return free_mem;
}

#endif