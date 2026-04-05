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

    inline void processQuestion(CS& cs) {
        cs.setBitOn(0);
    }

    inline void processStar(const GuideTable& guideTable, CS& cs) {

        cs.setBitOn(0);
        int ix = guideTable.alphabetSize + 1;

        while (ix < guideTable.ICsize)
        {
            if (!cs.getBit(ix)) {
                for (auto [left, right] : guideTable.iterateSplits(ix)) {
                    if (cs.getBit(left) && cs.getBit(right)) { cs.setBitOn(ix); break; }
                }
            }
            ix++;
        }
    }

    inline void processConcatenate(const GuideTable& guideTable, const CS& left, const CS& right, CS& res) {

        if (left.getBit(0)) res |= right;
        if (right.getBit(0)) res |= left;

        int ix = guideTable.alphabetSize + 1;

        while (ix < guideTable.ICsize)
        {
            // when CS have value that means one of parts contains phi, check above
            if (!res.getBit(ix)) {
                for (auto [l, r] : guideTable.iterateSplits(ix))
                    if (left.getBit(l) && right.getBit(r)) { res.setBitOn(ix); break; }
            }
            ix++;
        }
    }

    inline void processOr(const CS& left, const CS& right, CS& res) {
        res |= left;
        res |= right;
    }

    // ========================================================================

    void revertStarRandom(const GuideTable& guideTable, const CS& cs, CSBuffer& buffer, uint64_t seed = std::random_device{}());
    // revert with brute force
    void revertStarBrute(const GuideTable& guideTable, const CS& target, CSBuffer& buffer);
    void revertStar(const GuideTable& guideTable, const CS& cs, CSBuffer& buffer);

    void revertConcatRandom(const rei::GuideTable& guideTable, const CS& cs, CSBuffer& buffer, uint64_t seed = std::random_device{}(), bool enhanceCount = true);
    void revertConcat(const GuideTable& guideTable, const CS& cs, CSBuffer& buffer);

    void revertOrRandom(const CS& cs, CSBuffer& buffer, uint64_t seed = std::random_device{}());
    void revertOr(const CS& cs, CSBuffer& buffer);
}

#endif // OPERATIONS_H