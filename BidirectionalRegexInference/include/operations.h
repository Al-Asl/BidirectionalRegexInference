#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <string>
#include <vector>
#include <types.h>
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

        auto cs1 = cs | CS::one();
        int ix = guideTable.alphabetSize + 1;
        CS c = CS::one() << ix;

        while (ix < guideTable.ICsize)
        {
            if (!(cs1 & c)) {
                for (auto [left, right] : guideTable.IterateRow(ix)) {
                    if ((left & cs1) && (right & cs1)) { cs1 |= c; break; }
                }
            }
            c <<= 1; ix++;
        }

        return cs1;
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
                    if ((l & left) && (r & right)) { cs1 |= c; break; }
            }

            c <<= 1; ix++;
        }

        return cs1;
    }

    inline CS processOr(const CS& left, const CS& right) {
        return left | right;
    }

    class StarLookup {
    public:
        StarLookup(const GuideTable& guideTable);
        std::vector<CS> data;
    };

    std::vector<CS> revertQuestion(const CS& cs);

    std::vector<CS> revertStar(const CS& cs, const StarLookup& starLookup);

    std::vector<Pair<CS>> revertConcat(const CS& cs, const GuideTable& guideTable);

    vector<Pair<CS>> revertOr(const CS& cs);
}

#endif // OPERATIONS_H