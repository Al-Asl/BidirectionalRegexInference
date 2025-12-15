#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <string>
#include <vector>
#include <types.h>
#include <guide_table.h>

template<typename T>
using vector = std::vector<T>;

namespace rei
{
    enum class Operation { Question = 0, Star = 1, Concatenate = 2, Or = 3, Count = 4 };

    std::string to_string(Operation op) {
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

    CS processQuestion(const CS& cs) {
        return cs | CS::one();
    }

    CS processStar(const GuideTable& guideTable, const CS& cs) {

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

    CS processConcatenate(const GuideTable& guideTable, const CS& left, const CS& right) {

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

    CS processOr(const CS& left, const CS& right) {
        return left | right;
    }

    class StarLookup {
    public:
        StarLookup(const GuideTable& guideTable) {
            for (int i = 0; i < guideTable.ICsize; i++)
            {
                auto c = processStar(guideTable, CS::one() << i);

                auto it = std::find_if(data.begin(), data.end(), [c](std::vector<CS> vec) {
                    return (vec[0] & c) == c;
                    });

                if (it == data.end())
                    data.push_back(std::vector<CS>({ c }));
                else
                    (*it).push_back(c);
            }
        }
        std::vector<std::vector<CS>> data;
    };

    std::vector<CS> invertQuestion(const CS& cs) {
        if (cs & CS::one())
        { return std::vector<CS>{ cs | CS::one() }; }
        else
        { return std::vector<CS>{}; }
    }

    void invertStar(int index, const std::vector<std::tuple<CS, int>>& candidates, const CS& target, std::vector<int>& picks, CS cs, std::vector<CS>& res)
    {
        if (cs == target) {
            auto rcs = CS();
            for (int idx : picks) 
                rcs |= CS::one() << idx;
            res.push_back(rcs);
        }

        if (index == candidates.size())
            return;

        invertStar(index + 1, candidates, target, picks, cs, res);

        picks.push_back(std::get<1>(candidates[index]));
        invertStar(index + 1, candidates, target, picks, cs | std::get<0>(candidates[index]), res);
        picks.pop_back();
    }

    std::vector<CS> invertStar(const CS& cs, const StarLookup& starLookup) {

        std::vector<std::tuple<CS,int>> candidates;

        for (int i = 0; i < starLookup.data.size(); i++)
        {
            auto rcs = starLookup.data[i][0];
            if ((cs & rcs) == rcs)
                candidates.push_back({ rcs, i });
        }

        std::vector<CS> res;
        std::vector<int> picks;
        invertStar(0, candidates, cs, picks, CS(), res);
        return res;
    }

    void invertConcat(
        Pair<CS> pair,
        const CS& cs,
        int index,
        const vector<vector<Pair<CS>>>& targetPairs,
        const GuideTable& guideTable,
        vector<Pair<CS>>& res)
    {
        if (index == targetPairs.size()) {
            if (pair.left != CS::one() && pair.right != CS::one())
                res.push_back(pair);
            return;
        }

        auto row = targetPairs[index];
        for (int i = 0; i < row.size(); i++)
        {
            auto p = row[i];

            p.left |= pair.left;
            p.right |= pair.right;

            if ((processConcatenate(guideTable, p.left, p.right) | cs) ^ cs)
                continue;

            invertConcat(p, cs, index + 1, targetPairs, guideTable, res);
        }
    }

    std::vector<Pair<CS>> invertConcat(const CS& cs, const GuideTable& guideTable)
    {
        vector<vector<Pair<CS>>> targetPairs;

        if (cs & CS::one())
            targetPairs.push_back({ {CS::one(), CS::one()} });

        for (int i = 1; i < guideTable.ICsize; i++)
        {
            auto c = CS::one() << i;

            if (!(c & cs))
                continue;

            vector<Pair<CS>> row;

            row.push_back({ c, CS::one() });
            row.push_back({ CS::one(), c });

            for (auto const& pair : guideTable.IterateRow(i))
            {
                row.push_back(pair);
            }

            targetPairs.push_back(row);
        }

        auto res = vector<Pair<CS>>();

        invertConcat({ CS(),CS() }, cs, 0, targetPairs, guideTable, res);

        return res;
    }

    vector<Pair<CS>> invertOr(const CS& cs) {
        vector<Pair<CS>> pairs;

        for (CS submask = cs; submask >= 0;) {

            CS complement = cs ^ submask;

            if ((submask > CS::one() && complement > CS::one()) && 
                submask <= complement) {
                pairs.push_back({ submask, complement });
            }

            if (submask == 0) break;

            submask--;
            submask = submask & cs;
        }

        return pairs;
    }
}

#endif // end OPERATIONS_H