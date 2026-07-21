#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <string>
#include <types.h>
#include <language_system.hpp>

using GuideTable = rei::PairsTable<uint16_t>;

namespace rei
{
    enum class Operation { Question = 0, Star = 1, Concatenate = 2, Or = 3, Count = 4 };

    inline std::string to_string(Operation op) {
        switch (op)
        {
        case Operation::Question:
            return "Q";
        case Operation::Star:
            return "S";
        case Operation::Concatenate:
            return "C";
        case Operation::Or:
            return "O";
        default:
            break;
        }
        return "";
    }

    void inline processQuestion(CS& cs) {
        cs.setBitOn(0);
    }

    inline void processStar(const GuideTable& guideTable, int alphabetSize, CS& cs) {

        cs.setBitOn(0);
        int ix = alphabetSize + 1;
        int ICsize = guideTable.getSize().right;

        while (ix < ICsize)
        {
            if (!cs.getBit(ix)) {
                for (auto [left, right] : guideTable.iterate(ix)) {
                    if (cs.getBit(left) && cs.getBit(right)) { cs.setBitOn(ix); break; }
                }
            }
            ix++;
        }
    }

    inline void processConcatenate(const GuideTable& guideTable, int alphabetSize, const CS& left, const CS& right, CS& res) {

        if (left.getBit(0)) res |= right;
        if (right.getBit(0)) res |= left;

        int ix = alphabetSize + 1;
        int ICsize = guideTable.getSize().right;

        while (ix < ICsize)
        {
            // when CS have value that means one of parts contains phi, check above
            if (!res.getBit(ix)) {
                for (auto [l, r] : guideTable.iterate(ix))
                    if (left.getBit(l) && right.getBit(r)) { res.setBitOn(ix); break; }
            }
            ix++;
        }
    }

    inline void processOr(const CS& left, const CS& right, CS& res) {
        res |= left;
        res |= right;
    }
}

#endif // OPERATIONS_H