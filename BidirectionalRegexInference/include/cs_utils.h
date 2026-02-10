#ifndef CS_UTILS_H
#define CS_UTILS_H

#include <vector>
#include <random>
#include <types.h>

namespace rei {

    inline std::vector<int> getBits(const CS& cs, int ICsize) {
        std::vector<int> bits;
        for (int i = 0; i < ICsize; ++i) {
            if (cs & (CS::one() << i)) {
                bits.push_back(i);
            }
        }
        return bits;
    }

    inline CS getRandom(const std::vector<int>& bits, std::mt19937_64& rng, std::bernoulli_distribution& coin) {
        CS submask;
        for (size_t i = 0; i < bits.size(); ++i) {
            if (coin(rng)) {
                submask |= CS::one() << bits[i];
            }
        }
        return submask;
    }

}

#endif // CS_UTILS