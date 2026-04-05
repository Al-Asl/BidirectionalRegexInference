#include <bottom_up.hpp>
#include <numeric>

#define LOG_OP(context, cost, op_string, dif) \
        int tbc = dif; \
        if (tbc) printf("Cost %-2d | (%s) | AllREs: %-11llu | StoredREs: %-10d | ToBeChecked: %-10d \n", \
            cost, op_string.c_str() ,context.allREs, context.cache.count(), tbc);

#define CREATE_CS(name, size) \
    std::vector<uint64_t> name##_data(size,0); \
    CS name(name##_data.data(), size); \

rei::BottomUpSearch::Context::Context(int cache_capacity, int n, const CS& posBits, const CS& negBits) : 
    posBits(posBits), negBits(negBits) {

    cache = CSBuffer(new uint64_t[(cache_capacity + 1) * n], cache_capacity, n);
    leftRightIdx = new int[2 * (cache_capacity + 1)];

    allREs = 0;
    onTheFly = false;
}

rei::BottomUpSearch::Context::~Context() {
    delete[] cache.data();
    delete[] leftRightIdx;
}

bool rei::BottomUpSearch::Context::insertAndCheck(CS cs, int index) {
    return insertAndCheck(cs, index, -1);
}

bool rei::BottomUpSearch::Context::insertAndCheck(CS cs, int lIndex, int rIndex)
{
    allREs++;
    if (onTheFly) {
        if (cs.containsAll(posBits) && cs.containsNone(negBits)) {
            leftRightIdx[cache.count() << 1] = lIndex;
            if (rIndex > -1)
                leftRightIdx[(cache.count() << 1) + 1] = rIndex;
            return true;
        }
    }
    else if (visited.insert(cs.getHash()).second)
    {
        leftRightIdx[cache.count() << 1] = lIndex;
        if (rIndex > -1)
            leftRightIdx[(cache.count() << 1) + 1] = rIndex;
        if (cs.containsAll(posBits) && cs.containsNone(negBits)) {
            return true;
        }
        cache.append().copy(cs);
        if (cache.isFull()) onTheFly = true;
    }
    return false;
}

CSBuffer rei::BottomUpSearch::Context::getCacheSlice(int start, int end) {
    return cache.getView(start, end - start);
}

rei::BottomUpSearch::BottomUpSearch(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs, const unsigned short maxCost, const CS& posBits, const CS& negBits, int cache_capacity) :
    guideTable(guideTable), alphabet(alphabets), costs(costs), maxCost(maxCost), context(cache_capacity, CS::getChuncksSize(guideTable.ICsize), posBits, negBits), partitioner(maxCost + 1) {

    costLevel = costs.alphaCost() + 1;
    shortageCost = -1;
    lastRound = false;

    auto n = CS::getChuncksSize(guideTable.ICsize);
    CREATE_CS(cs, n)

    partitioner.fillTo(costs.alphaCost(), 0);

    // adding eps, empty and alphabets
    context.visited.insert(cs.getHash());
    context.visited.insert(cs.toggleBit(0).getHash());
    cs.toggleBit(0);
    for (int i = 0; i < static_cast<int>(alphabets.size()); i++)
    {
        cs.toggleBit(i + 1);
        context.visited.insert(cs.getHash());
        context.cache.append().copy(cs);
        cs.toggleBit(i + 1);
    }

    partitioner.end(costs.alphaCost(), Operation::Concatenate) = context.cache.count();
    partitioner.end(costs.alphaCost(), Operation::Or) = context.cache.count();
}

rei::EnumerationState rei::BottomUpSearch::enumerateCostLevel(BottomUpSearchResult& res) {

    if (costLevel > maxCost) return EnumerationState::End;

    int solvedIdx;
    EnumerationState enumState = enumerateLevel(solvedIdx);

    if (enumState == EnumerationState::Found)
        res.RE = constructDownward(solvedIdx);

    res.cost = costLevel;
    res.allCS = context.allREs;

    costLevel++;
    return enumState;

}

uint64_t rei::BottomUpSearch::estimateNextLevel() {

    uint64_t count = 0;

    bool useQuestionOverOr = costs.alphaCost() + costs.operationCost(Operation::Or) >= costs.operationCost(Operation::Question);

    //Question Mark
    if (costLevel >= costs.alphaCost() + costs.operationCost(Operation::Question) && useQuestionOverOr) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.operationCost(Operation::Star), static_cast<Operation>(2));
        count += (end - start);
    }

    if (costLevel >= costs.alphaCost() + costs.operationCost(Operation::Star)) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.operationCost(Operation::Star), static_cast<Operation>(2));
        count += (end - start);
    }

    //Concatenate
    for (int i = costs.alphaCost(); 2 * i <= costLevel - costs.operationCost(Operation::Concatenate); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.operationCost(Operation::Concatenate));

        count += 2 * (lend - lstart) * (rend - rstart);
    }

    //Union
    if (!useQuestionOverOr && costLevel >= 2 * costs.alphaCost() + costs.operationCost(Operation::Or)) {

        auto [start, end] = partitioner.Interval(costLevel - costs.alphaCost() - costs.operationCost(Operation::Or));
        count += (end - start);
    }
    for (int i = costs.alphaCost(); 2 * i <= costLevel - costs.operationCost(Operation::Or); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.operationCost(Operation::Or));

        count += (lend - lstart) * (rend - rstart);
    }

    return count;
}

std::string rei::BottomUpSearch::constructRE(int idx) const {
    if (idx == -1) return std::string("eps");
    return constructDownward(idx);
}

const CS rei::BottomUpSearch::getCS(int idx) const
{
    return context.cache[idx];
}

std::vector<int> rei::BottomUpSearch::getLastCostLevel(int& cost) {
    cost = costLevel - 1;
    auto [start, end] = partitioner.Interval(cost);
    if (start == 0)
    {
        std::vector<int> ids(end - start + 1);
        iota(ids.begin(), ids.end(), -1);
        return ids;
    }
    else 
    {
        std::vector<int> ids(end - start);
        iota(ids.begin(), ids.end(), start);
        return ids;
    }
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

    auto [cost, op] = partitioner.indexToLevel(index);

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
    bool useQuestionOverOr = costs.alphaCost() + costs.operationCost(Operation::Or) >= costs.operationCost(Operation::Question);

    // Once it uses a previous cost that is not fully stored, it should continue as the last round
    if (context.onTheFly) {
        int dif = costLevel - shortageCost;
        if (dif == costs.operationCost(Operation::Question) || dif == costs.operationCost(Operation::Star) || 
            dif == costs.alphaCost() + costs.operationCost(Operation::Concatenate) || dif == costs.alphaCost() + costs.operationCost(Operation::Or))
            lastRound = true;
    }

    CREATE_CS(cs, CS::getChuncksSize(guideTable.ICsize))

    //Question Mark
    if (costLevel >= costs.alphaCost() + costs.operationCost(Operation::Question) && useQuestionOverOr) {

        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.operationCost(Operation::Question), static_cast<Operation>(2));
        auto pLevel = context.getCacheSlice(start, end);
        LOG_OP(context, costLevel, to_string(Operation::Question), end - start);
        for (auto i = start; i < end; i++)
        {
            pLevel[i - start].copyTo(cs);
            if (!cs.getBit(0)) {
                processQuestion(cs);
                if (context.insertAndCheck(cs, i))
                {
                    idx = context.cache.count();
                    return EnumerationState::Found;
                }
            }
        }
    }
    partitioner.end(costLevel, Operation::Question) = context.cache.count();

    //Star
    if (costLevel >= costs.alphaCost() + costs.operationCost(Operation::Star)) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costs.operationCost(Operation::Star), static_cast<Operation>(2));
        auto pLevel = context.getCacheSlice(start, end);
        LOG_OP(context, costLevel, to_string(Operation::Star), end - start);
        for (auto i = start; i < end; i++)
        {
            pLevel[i - start].copyTo(cs);
            processStar(guideTable, cs);
            if (context.insertAndCheck(cs, i))
            {
                idx = context.cache.count();
                return EnumerationState::Found;
            }
        }
    }
    partitioner.end(costLevel, Operation::Star) = context.cache.count();

    //Concatenate
    for (int i = costs.alphaCost(); 2 * i <= costLevel - costs.operationCost(Operation::Concatenate); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.operationCost(Operation::Concatenate));
        auto lpLevel = context.getCacheSlice(lstart, lend);
        auto rpLevel = context.getCacheSlice(rstart, rend);
        LOG_OP(context, costLevel, to_string(Operation::Concatenate), 2 * (rend - rstart) * (lend - lstart));

        for (int l = lstart; l < lend; ++l) {
            CS left = lpLevel[l - lstart];
            for (int r = rstart; r < rend; ++r) {

                processConcatenate(guideTable, left, rpLevel[r - rstart], cs.clear());

                if (context.insertAndCheck(cs, l, r))
                {
                    idx = context.cache.count();
                    return EnumerationState::Found;
                }

                processConcatenate(guideTable, rpLevel[r - rstart], left, cs.clear());

                if (context.insertAndCheck(cs, r, l))
                {
                    idx = context.cache.count();
                    return EnumerationState::Found;
                }
            }
        }

    }
    partitioner.end(costLevel, Operation::Concatenate) = context.cache.count();

    //Union
    if (!useQuestionOverOr && costLevel >= 2 * costs.alphaCost() + costs.operationCost(Operation::Or)) {

        auto [start, end] = partitioner.Interval(costLevel - costs.alphaCost() - costs.operationCost(Operation::Or));
        auto pLevel = context.getCacheSlice(start, end);
        LOG_OP(context, costLevel, to_string(Operation::Or), end - start);

        for (int r = start; r < end; ++r) {

            pLevel[r - start].copyTo(cs);
            processQuestion(cs);

            if (context.insertAndCheck(cs, -2, r))
            {
                idx = context.cache.count();
                return EnumerationState::Found;
            }
        }
    }
    for (int i = costs.alphaCost(); 2 * i <= costLevel - costs.operationCost(Operation::Or); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costs.operationCost(Operation::Or));
        auto lpLevel = context.getCacheSlice(lstart, lend);
        auto rpLevel = context.getCacheSlice(rstart, rend);
        LOG_OP(context, costLevel, to_string(Operation::Or), (rend - rstart) * (lend - lstart));
        for (int l = lstart; l < lend; ++l) {
            CS left = lpLevel[l - lstart];
            for (int r = rstart; r < rend; ++r) {

                processOr(left, rpLevel[r - rstart], cs.clear());

                if (context.insertAndCheck(cs, l, r))
                {
                    idx = context.cache.count();
                    return EnumerationState::Found;
                }
            }
        }
    }
    partitioner.end(costLevel, Operation::Or) = context.cache.count();

    if (lastRound) return EnumerationState::End;
    if (context.onTheFly && shortageCost == -1) shortageCost = costLevel;

    return EnumerationState::NotFound;
}