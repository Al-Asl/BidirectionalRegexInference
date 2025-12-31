#ifndef REI_COMMON_H
#define REI_COMMON_H

#include <types.h>
#include <guide_table.hpp>
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
        int alpha;
        int question;
        int star;
        int concat;
        int alternation; //or
        Costs(const unsigned short* costFun) {
            alpha = costFun[0];
            question = costFun[1];
            star = costFun[2];
            concat = costFun[3];
            alternation = costFun[4];
        }
    };

    class CSResolverInterface {
    public:
        virtual std::string resolve(const CS& cs) = 0;
    };

    std::set<char> findAlphabets(const std::vector<std::string>& pos, const std::vector<std::string>& neg);

    bool intialCheck(std::set<char> alphabets, const std::vector<std::string>& pos, std::string& RE);
}

#endif // REI_COMMON_H