#ifndef REI_COMMON_H
#define REI_COMMON_H

#include <types.h>
#include <operations.h>
#include <level_partitioner.hpp>

namespace rei {

    enum class EnumerationState {
        Found,
        NotFound,
        End
    };

    struct Costs
    {
        Costs(const unsigned short* costFun) : costFun(costFun){ }
        unsigned short alphaCost() const { return costFun[0]; }
        unsigned short operationCost(Operation op) const { return costFun[static_cast<int>(op) + 1]; }
    private:
        const unsigned short* costFun;
    };

    class CSResolverInterface {
    public:
        virtual ~CSResolverInterface() = default;

        virtual std::string constructRE(const int id) = 0;
        virtual CS getCS(const int id) = 0;
    };

    std::set<char> findAlphabets(const std::vector<std::string>& pos, const std::vector<std::string>& neg);

    bool intialCheck(std::set<char> alphabets, const std::vector<std::string>& pos, std::string& RE);
}

#endif // REI_COMMON_H