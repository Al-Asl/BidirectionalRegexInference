#include <string>
#include <vector>

#ifndef REI_HPP
#define REI_HPP

namespace rei {

    struct Result
    {
        std::string     RE;
        int             REcost;
        unsigned long   allREs;
        int             ICsize;

        Result(const std::string& RE, int REcost, unsigned long allREs, int ICsize)
            : RE(RE), REcost(REcost), allREs(allREs), ICsize(ICsize) {
        }
    };

	Result Run(const unsigned short* costFun, const unsigned short maxCost,
        const std::vector<std::string>& pos, const std::vector<std::string>& neg, double maxTime);
}

#endif //end REI_HPP