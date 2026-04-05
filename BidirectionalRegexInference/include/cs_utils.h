#ifndef CS_UTILS_H
#define CS_UTILS_H

#include <vector>
#include <random>
#include <types.h>

namespace rei {

    inline void getRandom(const std::vector<int>& bits, CS& cs, std::mt19937& rng, std::bernoulli_distribution& coin) {
        for (size_t i = 0; i < bits.size(); ++i) {
            if (coin(rng)) {
                cs.setBitOn(bits[i]);
            }
        }
    }

}

#endif // CS_UTILS