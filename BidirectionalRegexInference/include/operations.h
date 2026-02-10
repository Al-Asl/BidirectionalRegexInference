#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <string>
#include <vector>
#include <types.h>
#include <random>
#include <utility>
#include <functional>
#include <guide_table.hpp>

template<typename T>
using vector = std::vector<T>;

namespace rei
{
    enum class Operation { Question = 0, Star = 1, Concatenate = 2, Or = 3, Count = 4 };

    std::string to_string(Operation op);

    inline CS processQuestion(const CS& cs) {
        return cs | CS::one();
    }

    inline CS processStar(const GuideTable& guideTable, const CS& cs) {

        auto res = cs | CS::one();
        int ix = guideTable.alphabetSize + 1;
        CS c = CS::one() << ix;

        while (ix < guideTable.ICsize)
        {
            if (!(res & c)) {
                for (auto [left, right] : guideTable.IterateRow(ix)) {
                    if (((CS::one() << left) & res) && ((CS::one() << right) & res)) { res |= c; break; }
                }
            }
            c <<= 1; ix++;
        }

        return res;
    }

    inline CS processConcatenate(const GuideTable& guideTable, const CS& left, const CS& right) {

        CS cs1 = CS();
        if (left & CS::one()) cs1 |= right;
        if (right & CS::one()) cs1 |= left;

        int ix = guideTable.alphabetSize + 1;
        CS c = CS::one() << ix;

        while (ix < guideTable.ICsize)
        {
            // when CS have value that means one of parts contains phi, check above
            if (!(cs1 & c)) {
                for (auto [l, r] : guideTable.IterateRow(ix))
                    if (((CS::one() << l) & left) && ((CS::one() << r) & right)) { cs1 |= c; break; }
            }

            c <<= 1; ix++;
        }

        return cs1;
    }

    inline CS processOr(const CS& left, const CS& right) {
        return left | right;
    }

    std::vector<CS> revertQuestion(const CS& cs);

    std::vector<CS> revertStarRandom(const CS& cs, size_t maxSamples, const GuideTable& guideTable, uint64_t seed = std::random_device{}());
    // revert with brute force
    std::vector<CS> revertStarBrute(const GuideTable& guideTable, const CS& target);
    std::vector<CS> revertStar(const CS& cs, const GuideTable& guideTable);

    std::vector<Pair<CS>> revertConcatRandom(const CS& cs, size_t maxSamples, const GuideTable& guideTable, uint64_t seed = std::random_device{}());
    // revert with brute force
    std::vector<Pair<CS>> revertConcatBrute(const CS& target, const GuideTable& guideTable);
    std::vector<Pair<CS>> revertConcat(const CS& cs, const GuideTable& guideTable);

    std::vector<Pair<CS>> revertOrRandom(const CS& cs, size_t maxSamples, int ICsize, uint64_t seed = std::random_device{}());
    vector<Pair<CS>> revertOr(const CS& cs);
}

#endif // OPERATIONS_H