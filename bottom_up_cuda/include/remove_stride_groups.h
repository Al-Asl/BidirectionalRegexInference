#ifndef REMOVE_STRIDE_GROUPS_H
#define REMOVE_STRIDE_GROUPS_H

#include <device_launch_parameters.h>

#include <thrust/remove.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/device_vector.h>
#include <thrust/execution_policy.h>

struct check_bitmask
{
    const uint32_t* d_mask;
    int group_size;

    check_bitmask(const uint32_t* d_mask, int group_size)
        : d_mask(d_mask), group_size(group_size) {
    }

    __host__ __device__
        bool operator()(int x) const
    {
        int g_idx = x / group_size;
        int word_idx = g_idx / 32;
        int bit_idx = g_idx % 32;

        return (d_mask[word_idx] & (1u << bit_idx)) != 0;
    }
};

__global__ void build_removal_bitmask(
    const uint64_t* __restrict__ d_buf,
    uint32_t* __restrict__ d_mask,
    int num_groups,
    int group_size,
    uint64_t value)
{
    int g_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (g_idx < num_groups)
    {
        int g_start = g_idx * group_size;
        bool keep = false;

        for (int i = 0; i < group_size; i++)
        {
            if (d_buf[g_start + i] != value)
            {
                keep = true;
                break;
            }
        }

        if (!keep)
        {
            int word_idx = g_idx / 32;
            int bit_idx = g_idx % 32;

            atomicOr(&d_mask[word_idx], (1u << bit_idx));
        }
    }
}

struct remove_groups
{
    const uint64_t* d_buf_readonly;
    int n;
    uint64_t value;

    remove_groups(const uint64_t* d_buf_readonly, int n, uint64_t value)
        : d_buf_readonly(d_buf_readonly), n(n), value(value) {
    }

    __host__ __device__
        bool operator()(int x) const
    {
        int g_start = (x / n) * n;
        bool keep = false;

        for (int i = 0; i < n; i++)
        {
            if (d_buf_readonly[g_start + i] != value)
            {
                keep = true;
                break;
            }
        }

        return !keep;
    }
};



inline uint64_t* remove_stride_groups(uint64_t* d_buf, int size, int group_size, uint64_t value)
{
    int num_groups = size / group_size;

    int num_mask_words = (num_groups + 31) / 32;

    thrust::device_vector<uint32_t> d_mask(num_mask_words, 0);
    uint32_t* raw_mask_ptr = thrust::raw_pointer_cast(d_mask.data());

    int threads_per_block = 256;
    int blocks = (num_groups + threads_per_block - 1) / threads_per_block;

    build_removal_bitmask<<<blocks, threads_per_block>>>(
        d_buf, raw_mask_ptr, num_groups, group_size, value
        );

    auto end = thrust::remove_if(
        thrust::device,
        d_buf,
        d_buf + size,
        thrust::make_counting_iterator(0),
        check_bitmask(raw_mask_ptr, group_size)
    );

    return end;
}

#endif