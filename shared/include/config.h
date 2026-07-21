#ifndef CONFIG_H
#define CONFIG_H

#ifdef __CUDACC__
#define HD __host__ __device__
#else
#define HD
#endif

#endif