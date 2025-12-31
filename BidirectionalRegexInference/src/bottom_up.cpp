#include <bottom_up.hpp>

#define LOG_OP(context, cost, op_string, dif) \
        int tbc = dif; \
        if (tbc) printf("Cost %-2d | (%s) | AllREs: %-11llu | StoredREs: %-10d | ToBeChecked: %-10d \n", \
            cost, op_string.c_str() ,context.allREs, context.lastIdx, tbc);

rei::BottomUpSearch::Context::Context(int cache_capacity, const CS& posBits, const CS& negBits) : cache_capacity(cache_capacity), posBits(posBits), negBits(negBits) {

    cache = new CS[cache_capacity + 1];
    leftRightIdx = new int[2 * (cache_capacity + 1)];

    lastIdx = 0;
    allREs = 0;
    onTheFly = false;
}

rei::BottomUpSearch::Context::~Context() {
    delete[] cache;
    delete[] leftRightIdx;
}

bool rei::BottomUpSearch::Context::InsertAndCheck(CS CS, int index) {
    return InsertAndCheck(CS, index, -1);
}

bool rei::BottomUpSearch::Context::InsertAndCheck(CS CS, int lIndex, int rIndex)
{
    allREs++;
    if (onTheFly) {
        if ((CS & posBits) == posBits && (~CS & negBits) == negBits) {
            leftRightIdx[lastIdx << 1] = lIndex;
            if (rIndex > -1)
                leftRightIdx[(lastIdx << 1) + 1] = rIndex;
            return true;
        }
    }
    else if (visited.find(CS) == visited.end())
    {
        leftRightIdx[lastIdx << 1] = lIndex;
        if (rIndex > -1)
            leftRightIdx[(lastIdx << 1) + 1] = rIndex;
        if ((CS & posBits) == posBits && (~CS & negBits) == negBits) {
            return true;
        }
        visited[CS] = lastIdx;
        cache[lastIdx++] = CS;
        if (lastIdx == cache_capacity) onTheFly = true;
    }
    return false;
}

std::span<CS> rei::BottomUpSearch::Context::GetCacheSlice(int start, int end) {
    return std::span<CS>(cache + start, end - start);
}

rei::BottomUpSearch::BottomUpSearch(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs, const unsigned short maxCost, const CS& posBits, const CS& negBits, int cache_capacity) :
    guideTable(guideTable), alphabet(alphabets), costs(costs), maxCost(maxCost), posBits(posBits), negBits(negBits), context(cache_capacity, posBits, negBits), partitioner(maxCost + 1) {

    costLevel = costs.alpha + 1;
    shortageCost = -1;
    lastRound = false;

    // adding eps, empty and alphabets
    context.visited[CS()] = -1;
    context.visited[CS::one()] = -1;
    for (int i = 0; i < static_cast<int>(alphabets.size()); i++)
    {
        auto alpha = CS::one() << (i + 1);
        context.visited[alpha] = context.lastIdx;
        context.cache[context.lastIdx++] = alpha;
    }

    partitioner.end(costs.alpha, Operation::Concatenate) = context.lastIdx;
    partitioner.end(costs.alpha, Operation::Or) = context.lastIdx;
}

rei::EnumerationState rei::BottomUpSearch::EnumerateCostLevel(BottomUpSearchResult& res) {

    if (costLevel > maxCost) return EnumerationState::End;

    int solvedIdx;
    EnumerationState enumState = enumerateLevel(solvedIdx);

    if (enumState == EnumerationState::Found)
    {
        res.RE = constructDownward(solvedIdx);
        res.cost = costLevel;
        res.allREs = context.allREs;
    }

    costLevel++;
    return enumState;

}

std::string rei::BottomUpSearch::ConstructRE(const CS& cs) const {
    auto idx = context.visited.at(cs);
    if (idx == -1) return std::string("eps");
    return constructDownward(idx);
}

std::span<CS> rei::BottomUpSearch::GetLastCostLevel() const {
    auto [start, end] = partitioner.Interval(costLevel - 1);
    return std::span<CS>(context.cache + start, end - start);
}

// Adding parentheses if needed
std::string rei::BottomUpSearch::bracket(std::string s) const {
    int p = 0;
    for (int i = 0; i < s.length(); i++) {
        if (s[i] == '(') p++;
        else if (s[i] == ')') p--;
        else if (s[i] == '+' && p <= 0) return "(" + s + ")";
    }
    return s;
}

// Generating the final RE string recursively
std::string rei::BottomUpSearch::constructDownward(int index) const {

    if (index == -2) return "eps"; // Epsilon
    if (index < alphabet.size()) { std::string s(1, *next(alphabet.begin(), index)); return s; }

    int cost; Operation op;
    partitioner.indexToLevel(index, cost, op);

    if (op == Operation::Question) {
        std::string res = constructDownward(context.leftRightIdx[index << 1]);
        if (res.length() > 1) return "(" + res + ")?";
        return res + "?";
    }

    if (op == Operation::Star) {
        std::string res = constructDownward(context.leftRightIdx[index << 1]);
        if (res.length() > 1) return "(" + res + ")*";
        return res + "*";
    }

    if (op == Operation::Concatenate) {
        std::string left = constructDownward(context.leftRightIdx[index << 1]);
        std::string right = constructDownward(context.leftRightIdx[(index << 1) + 1]);
        return bracket(left) + bracket(right);
    }

    if (op == Operation::Or)
    {
        std::string left = constructDownward(context.leftRightIdx[index << 1]);
        std::string right = constructDownward(context.leftRightIdx[(index << 1) + 1]);
        return left + "+" + right;
    }
}

rei::EnumerationState rei::BottomUpSearch::enumerateLevel(int& idx) {
    bool useQuestionOverOr = costs.alpha + costs.alternation >= costs.question;

    // Once it uses a previous cost that is not fully stored, it should continue as the last round
    if (context.onTheFly) {
        int dif = costLevel - shortageCost;
        if (dif == costs.question || dif == costs.star || dif == costs.alpha + costs.concat || dif == costs.alpha + costs.alternation) lastRound = true;
    }

    //Question Mark
    if (costLevel >= costs.alpha + costs.question && useQuestionOverOr) {

        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.question, static_cast<Operation>(2));
        auto pLevel = context.GetCacheSlice(start, end);
        LOG_OP(context, costLevel, to_string(Operation::Question), end - start);
        for (auto i = start; i < end; i++)
        {
            CS cs = pLevel[i - start];
            if (!(cs & CS::one())) {
                cs = processQuestion(cs);
                if (context.InsertAndCheck(cs, i))
                {
                    partitioner.end(costLevel, Operation::Question) = INT_MAX;
                    idx = context.lastIdx;
                    return EnumerationState::Found;
                }
            }
        }
    }
    partitioner.end(costLevel, Operation::Question) = context.lastIdx;

    //Star
    if (costLevel >= costs.alpha + costs.star) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.star, static_cast<Operation>(2));
        auto pLevel = context.GetCacheSlice(start, end);
        LOG_OP(context, costLevel, to_string(Operation::Star), end - start);
        for (auto i = start; i < end; i++)
        {
            CS cs = pLevel[i - start];
            cs = processStar(guideTable, cs);
            if (context.InsertAndCheck(cs, i))
            {
                partitioner.end(costLevel, Operation::Star) = INT_MAX;
                idx = context.lastIdx;
                return EnumerationState::Found;
            }
        }
    }
    partitioner.end(costLevel, Operation::Star) = context.lastIdx;

    //Concatenate
    for (int i = costs.alpha; 2 * i <= costLevel - costs.concat; ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.concat);
        auto lpLevel = context.GetCacheSlice(lstart, lend);
        auto rpLevel = context.GetCacheSlice(rstart, rend);
        LOG_OP(context, costLevel, to_string(Operation::Concatenate), 2 * (rend - rstart) * (lend - lstart));

        for (int l = lstart; l < lend; ++l) {
            CS left = lpLevel[l - lstart];
            for (int r = rstart; r < rend; ++r) {

                auto leftRight = processConcatenate(guideTable, left, rpLevel[r - rstart]);
                auto rightLeft = processConcatenate(guideTable, rpLevel[r - rstart], left);

                if (context.InsertAndCheck(leftRight, l, r))
                {
                    partitioner.end(costLevel, Operation::Concatenate) = INT_MAX;
                    idx = context.lastIdx;
                    return EnumerationState::Found;
                }

                if (context.InsertAndCheck(rightLeft, r, l))
                {
                    partitioner.end(costLevel, Operation::Concatenate) = INT_MAX;
                    idx = context.lastIdx;
                    return EnumerationState::Found;
                }
            }
        }

    }
    partitioner.end(costLevel, Operation::Concatenate) = context.lastIdx;

    //Union
    if (!useQuestionOverOr && costLevel >= 2 * costs.alpha + costs.alternation) {

        auto [rstart, rend] = partitioner.Interval(costLevel - costs.alpha - costs.alternation);
        auto pLevel = context.GetCacheSlice(rstart, rend);
        LOG_OP(context, costLevel, to_string(Operation::Or), rend - rstart);

        for (int r = rstart; r < rend; ++r) {

            CS cs = processOr(CS::one(), pLevel[r - rstart]);

            if (context.InsertAndCheck(cs, -2, r))
            {
                partitioner.end(costLevel, Operation::Or) = INT_MAX;
                idx = context.lastIdx;
                return EnumerationState::Found;
            }
        }
    }
    for (int i = costs.alpha; 2 * i <= costLevel - costs.alternation; ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.alternation);
        auto lpLevel = context.GetCacheSlice(lstart, lend);
        auto rpLevel = context.GetCacheSlice(rstart, rend);
        LOG_OP(context, costLevel, to_string(Operation::Or), (rend - rstart) * (lend - lstart));
        for (int l = lstart; l < lend; ++l) {
            CS left = lpLevel[l - lstart];
            for (int r = rstart; r < rend; ++r) {

                CS cs = processOr(left, rpLevel[r - rstart]);

                if (context.InsertAndCheck(cs, l, r))
                {
                    partitioner.end(costLevel, Operation::Or) = INT_MAX;
                    idx = context.lastIdx;
                    return EnumerationState::Found;
                }
            }
        }
    }
    partitioner.end(costLevel, Operation::Or) = context.lastIdx;

    if (lastRound) return EnumerationState::End;
    if (context.onTheFly && shortageCost == -1) shortageCost = costLevel;

    return EnumerationState::NotFound;
}