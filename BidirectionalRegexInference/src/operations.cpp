#include <operations.h>

std::string rei::to_string(Operation op) {
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

rei::StarLookup::StarLookup(const GuideTable& guideTable) {
    for (int i = 0; i < guideTable.ICsize; i++)
    {
        auto c = processStar(guideTable, CS::one() << i);
        data.push_back(c);
    }
}

std::vector<CS> rei::revertQuestion(const CS& cs) {
    if (cs & CS::one())
    {
        return std::vector<CS>{ cs | CS::one() };
    }
    else
    {
        return std::vector<CS>{};
    }
}

void revert_star(int index, const std::vector<std::tuple<CS, int>>& candidates, const CS& target, std::vector<int>& picks, CS cs, std::vector<CS>& res)
{
    if (cs == target) {
        auto rcs = CS();
        for (int idx : picks)
            rcs |= CS::one() << idx;
        res.push_back(rcs);
    }

    if (index == candidates.size())
        return;

    revert_star(index + 1, candidates, target, picks, cs, res);

    picks.push_back(std::get<1>(candidates[index]));
    revert_star(index + 1, candidates, target, picks, cs | std::get<0>(candidates[index]), res);
    picks.pop_back();
}

std::vector<CS> rei::revertStar(const CS& cs, const StarLookup& starLookup) {

    std::vector<std::tuple<CS, int>> candidates;

    for (int i = 0; i < starLookup.data.size(); i++)
    {
        auto rcs = starLookup.data[i];
        if ((cs & rcs) == rcs)
            candidates.push_back({ rcs, i });
    }

    std::vector<CS> res;
    std::vector<int> picks;
    revert_star(0, candidates, cs, picks, CS(), res);
    return res;
}

void revert_concat(
    Pair<CS> pair,
    const CS& cs,
    int index,
    const vector<vector<Pair<CS>>>& targetPairs,
    const rei::GuideTable& guideTable,
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

        if ((rei::processConcatenate(guideTable, p.left, p.right) | cs) ^ cs)
            continue;

        revert_concat(p, cs, index + 1, targetPairs, guideTable, res);
    }
}

std::vector<Pair<CS>> rei::revertConcat(const CS& cs, const GuideTable& guideTable)
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

    revert_concat({ CS(),CS() }, cs, 0, targetPairs, guideTable, res);

    return res;
}

vector<Pair<CS>> rei::revertOr(const CS& cs) {
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