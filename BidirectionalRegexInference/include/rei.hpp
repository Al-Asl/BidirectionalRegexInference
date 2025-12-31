#ifndef REI_HPP
#define REI_HPP

#include <string>
#include <vector>

namespace rei {

    struct Result
    {
        std::string     RE;
        int             ICsize;

        Result(const std::string& RE, int ICsize)
            : RE(RE), ICsize(ICsize) {
        }
    };

	Result Run(const unsigned short* costFun, const unsigned short maxCost,
        const std::vector<std::string>& pos, const std::vector<std::string>& neg, double maxTime);
}

#endif //end REI_HPP