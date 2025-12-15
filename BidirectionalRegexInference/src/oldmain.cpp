#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <math.h>
#include <algorithm>

#include <util.hpp>

#include <types.h>
#include <pair.h>
#include <chrono>

template <class T>
using Pair = rei::Pair<T>;

#define LOG_OP(context, cost, op_string, dif) \
        int tbc = dif; \
        if (tbc) printf("Cost %-2d | (%s) | AllREs: %-11llu | StoredREs: %-10d | ToBeChecked: %-10d \n", \
            cost, op_string.c_str() ,context.allREs, context.lastIdx, tbc);

// ============= Context =============

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



struct InsertCounter
{
    int bothSolved = 0;
    int bothVisited = 0;
    int oneSolvedOneVisited = 0;
    int oneSolvedOneNew = 0;
    int oneVisitedOneNew = 0;
    int bothNew = 0;
    int solved = 0;

    InsertCounter operator-(const InsertCounter& solved) const
    {
        InsertCounter diff;
        diff.bothSolved = bothSolved - solved.bothSolved;
        diff.bothVisited = bothVisited - solved.bothVisited;
        diff.oneSolvedOneVisited = oneSolvedOneVisited - solved.oneSolvedOneVisited;
        diff.oneSolvedOneNew = oneSolvedOneNew - solved.oneSolvedOneNew;
        diff.oneVisitedOneNew = oneVisitedOneNew - solved.oneVisitedOneNew;
        diff.bothNew = bothNew - solved.bothNew;
        diff.solved = this->solved - solved.solved;
        return diff;
    }
};

void print(InsertCounter counter, int level, Operation opreation, int toBeChecked)
{
    printf("(%s) | Level: %-2d | toCheck: %-10d | SS: %-5d | VV: %-10d | SV: %-10d | SN: %-10d | VN: %-10d | NN %-10d | solved: %d  \n", to_string(opreation).c_str(), level, toBeChecked, counter.bothSolved, counter.bothVisited, counter.oneSolvedOneVisited, counter.oneSolvedOneNew, counter.oneVisitedOneNew, counter.bothNew, counter.solved);
}

class Context {
public:
    Context(int cache_capacity, CS posBits, CS negBits)
        : cache_capacity(cache_capacity), posBits(posBits), negBits(negBits) {

        cache = new CS[cache_capacity + 1];
        leftChildIdx = new int[2 * (cache_capacity + 1)];
        parentSiblingIdx = new int[2 * (cache_capacity + 1)];
        lastIdx = 0;
        isFound = false;
        allREs = 0;
        checkCounter = {};
        //onTheFly = false;
    }

    ~Context() {
        delete[] cache;
        delete[] leftChildIdx;
    }

    std::vector<CS> generateCombinations(int icSize)
    {
        std::vector<int> dontCareBits;
        dontCareBits.reserve(icSize);

        const CS combined = posBits | negBits;
        for (int i = 0; i < icSize; ++i)
        {
            const CS bitMask = CS::one() << i;
            if ((bitMask & combined) == CS())
            {
                dontCareBits.push_back(i);
            }
        }

        const size_t numDontCareBits = dontCareBits.size();
        const size_t numCombinations = 1u << numDontCareBits;

        std::vector<CS> combinations;
        combinations.reserve(numCombinations);

        for (size_t subset = 0; subset < numCombinations; ++subset)
        {
            CS combination = posBits;

            for (size_t bit = 0; bit < numDontCareBits; ++bit)
            {
                if (subset & (1u << bit))
                {
                    combination |= (CS::one() << dontCareBits[bit]);
                }
            }

            combinations.push_back(combination);
        }

        return combinations;
    }

    // Checking empty, epsilon, and the alphabet
    bool intialCheck(int alphaCost, int iCSize, const std::vector<std::string>& pos, const std::vector<std::string>& neg, std::string& RE)
    {
        // Initialisation of the alphabet

        for (auto& word : pos) for (auto ch : word) alphabet.insert(ch);
        for (auto& word : neg) for (auto ch : word) alphabet.insert(ch);

        LOG_OP((*this), alphaCost, std::string("A"), static_cast<int>(alphabet.size()) + 2)

        // Checking empty
        allREs++;
        if (pos.empty()) { RE = "Empty"; return true; }
        //solved.insert(CS());

        // Checking epsilon
        allREs++;
        if ((pos.size() == 1) && (pos.at(0).empty())) { RE = "eps"; return true; }
        visited[CS::one()] =  -1;
        solved.insert(CS::one()); // int(CS of empty) is 1

        // Checking the alphabet
        lastAlphaIdx = CS::one() << 1; // Pointing to the position of the first char of the alphabet (idx 0 is for epsilon)
        auto alphabetSize = static_cast<int> (alphabet.size());

        for (int i = 0; i < alphabetSize; ++i) {

            allREs++;

            std::string s(1, *next(alphabet.begin(), i));
            if ((pos.size() == 1) && (pos.at(0) == s)) { RE = s; return true; }

            visited[lastAlphaIdx] = -1;
            solved.insert(lastAlphaIdx);

            lastAlphaIdx <<= 1;
            //lastIdx++;
        }

        lastAlphaIdx >>= 1;

        // all the CS that represent the solution!
        auto combinations = generateCombinations(iCSize);

        for (size_t i = 0; i < combinations.size(); i++)
        {
            int index = insert(combinations[i], -1);
            parentSiblingIdx[(index << 1) + 1] = -1;
        }

        return false;
    }

    bool recursiveCheck(int index)
    {
        int parentIdx = parentSiblingIdx[index << 1];
        int siblingIdx = parentSiblingIdx[(index << 1) + 1];

        if (parentIdx < 0)
            return true;

        if (siblingIdx < 0)
        {
            checkCounter.solved++;
            solved.insert(cache[parentIdx]);
            return recursiveCheck(parentIdx);
        }
        else if (solved.find(cache[siblingIdx]) != solved.end())
        {
            checkCounter.solved++;
            solved.insert(cache[parentIdx]);
            return recursiveCheck(parentIdx);
        }
        else
            return false;
    }

    bool Check(int index) {

        auto [csType, csIndex] = check(cache[index]);

        if (csType != ElementType::Solved)
            return false;

        auto parentIdx = parentSiblingIdx[index << 1];
        auto siblingIdx = parentSiblingIdx[(index << 1) + 1];

        if (siblingIdx < 0)
        {
            return recursiveCheck(parentIdx);
        }
        else
        {
            auto [siblingType, siblingIndex] = check(cache[siblingIdx]);

            if (static_cast<int>(siblingType) >= 2)
                return recursiveCheck(parentIdx);
            else
                return false;
        }
    }

    void printCache() {
        for (int i = 0; i < lastIdx; i++)
        {
            auto [high, low] = cache[i].get128Hash();
            printf("%-4d: %lld \n",i, low);
        }
    }

    int insert(CS cs, int parentIdx)
    {
        allREs++;
        visited[cs] = lastIdx;
        parentSiblingIdx[lastIdx << 1] = parentIdx;
        cache[lastIdx++] = cs;
        return lastIdx - 1;
    }

    enum class ElementType
    {
        None = 0,
        Vistied = 1,
        Solved = 2,
        Primitive = 3
    };

    // element type, element index in cache
    std::tuple<ElementType, int> check(CS cs) 
    {
        if (cs <= lastAlphaIdx)
            return { ElementType::Primitive, -1 };

        if (visited.find(cs) == visited.end())
            return { ElementType::None, -1 };

        if (solved.find(cs) == solved.end())
            return { ElementType::Vistied, visited[cs] };

        return { ElementType::Solved, visited[cs] };
    }

    bool insertAndCheck(int parentIdx, CS left, ElementType leftType, int leftIdx, CS right, ElementType rightType, int rightIdx)
    {
        if ((leftIdx > -1 && leftIdx < 4) || (rightIdx > -1 && rightIdx < 4))
            return false;

        if ((static_cast<int>(leftType) >= 2) && (static_cast<int>(rightType) >= 2))
        {
            checkCounter.bothSolved++;
            checkCounter.solved++;

            solved.insert(cache[parentIdx]);
            return recursiveCheck(parentIdx);
        }

        if ((static_cast<int>(leftType) >= 1) && (static_cast<int>(rightType) >= 1))
        {
            if ((static_cast<int>(leftType) >= 2) || (static_cast<int>(rightType) >= 2))
            { checkCounter.oneSolvedOneVisited++; }
            else
            { checkCounter.bothVisited++; }

            // need to handle this case somehow
            // left and right need to be stored for later checks!
            return false;
        }

        if (leftType == ElementType::None && rightType == ElementType::None)
        {
            checkCounter.bothNew++;

            leftIdx     = insert(left, parentIdx);
            rightIdx    = insert(right, parentIdx);

            parentSiblingIdx[(leftIdx << 1) + 1] = rightIdx;
            parentSiblingIdx[(rightIdx << 1) + 1] = leftIdx;

            return false;
        }

        if ((static_cast<int>(leftType) >= 2) || (static_cast<int>(rightType) >= 2))
        { checkCounter.oneSolvedOneNew++; }
        else
        { checkCounter.oneVisitedOneNew++; }

        if (rightType == ElementType::None)
        {
            rightIdx = insert(right, parentIdx);
            parentSiblingIdx[(rightIdx << 1) + 1] = leftIdx;
        }
        else
        {
            leftIdx = insert(left, parentIdx);
            parentSiblingIdx[(leftIdx << 1) + 1] = rightIdx;
        }

        return false;
    }

    bool insertAndCheck(int parentIdx, CS child)
    {
        auto [childType, childIdx] = check(child);

        return insertAndCheck(parentIdx, child, childType, childIdx, CS(), ElementType::Solved, -1);
    }

    bool insertAndCheck(int parentIdx, CS left, CS right)
    {
        auto [leftType, leftIdx]    = check(left);
        auto [rightType, rightIdx]  = check(right);

        return insertAndCheck(parentIdx, left, leftType, leftIdx, right, rightType, rightIdx);
    }

    CS lastAlphaIdx;
    std::set<char> alphabet;

    InsertCounter checkCounter;

    int cache_capacity;
    CS* cache;
    int* leftChildIdx;
    int* parentSiblingIdx;
    std::unordered_map<CS,int> visited;
    std::unordered_map<int, std::string> res;
    std::unordered_set<CS> solved;
    CS posBits, negBits;

    unsigned long allREs;
    // Index of the last free position in the language cache
    int lastIdx;
    bool isFound;
    //bool onTheFly;
};

// ============= Context =============

std::vector<CS> generateCombinations(const CS& pos, const CS& neg, int icSize)
{
    std::vector<int> dontCareBits;
    dontCareBits.reserve(icSize);

    const CS combined = pos | neg;
    for (int i = 0; i < icSize; ++i)
    {
        const CS bitMask = CS::one() << i;
        if ((bitMask & combined) == CS())
        {
            dontCareBits.push_back(i);
        }
    }

    const size_t numDontCareBits = dontCareBits.size();
    const size_t numCombinations = 1u << numDontCareBits;

    std::vector<CS> combinations;
    combinations.reserve(numCombinations);

    for (size_t subset = 0; subset < numCombinations; ++subset)
    {
        CS combination = pos;

        for (size_t bit = 0; bit < numDontCareBits; ++bit)
        {
            if (subset & (1u << bit))
            {
                combination |= (CS::one() << dontCareBits[bit]);
            }
        }

        combinations.push_back(combination);
    }

    return combinations;
}

template<class T>
using vector = std::vector<T>;

void getPowerSet(const vector<int>& set, int idx, vector<int>& current, vector<vector<int>>& res) {
    if (idx == set.size()) {
        res.push_back(current);
        return;
    }

    getPowerSet(set, idx + 1, current, res);

    current.push_back(set[idx]);
    getPowerSet(set, idx + 1, current, res);
    current.pop_back();
}

vector<vector<int>> getPowerSet(const vector<int>& set)
{
    vector<vector<int>> res;
    vector<int> current;
    getPowerSet(set, 0, current, res);
    return res;
}

vector<int> getActiveBits(const CS& cs, int size)
{
    std::vector<int> res;

    for (int i = 0; i < size; i++)
    {
        if (cs & (CS::one() << i)) {
            res.push_back(i);
        }
    }
    return res;
}

CS getCS(vector<int> activeBits) {

    CS cs;

    for (int i = 0; i < activeBits.size(); i++)
    {
        cs |= CS::one() << activeBits[i];
    }
    return cs;
}

int getFirstActiveBit(const CS& cs, unsigned int size) {
    for (int i = 0; i < size; i++)
        if (cs & (CS::one() << i)) return i;
    return -1;
}


void Insert(std::set<uint64_t>& allComb, std::vector<Pair<CS>> orInv) {
    std::vector<uint64_t> orInvDest;
    std::transform(orInv.begin(), orInv.end(),
        std::back_inserter(orInvDest), [](Pair<CS> x) { return x.left.get128Hash().right; });
    std::transform(orInv.begin(), orInv.end(),
        std::back_inserter(orInvDest), [](Pair<CS> x) { return x.right.get128Hash().right; });
    allComb.insert(orInvDest.begin(), orInvDest.end());
}

struct Result
{
    std::string     RE;
    int             REcost;
    unsigned long   allREs;
    int             ICsize;

    Result(const std::string& RE, int REcost, unsigned long allREs, int ICsize)
        : RE(RE), REcost(REcost), allREs(allREs), ICsize(ICsize) {
    }
};

std::string get_string(Context& context, CS cs, int idx) {
    if (cs == CS({ 1,0 }))
        return std::string("");
    else if (cs == CS({ 2,0 }))
        return std::string("0");
    else if (cs == CS({ 4,0 }))
        return std::string("1");
    else
    {
        return context.res[idx];
    }
}

//std::string breadthSearch(const GuideTable& guideTable, const std::vector<std::vector<CS>> starLookup, Context& context ,int parentIdx, CS parent, int depth = 0)
//{
//    if (parent & CS::one())
//    {
//        auto child = parent & (~CS::one());
//        auto [childType, childIdx] = context.check(child);
//
//        if (static_cast<int>(childType) >= 2)
//        {
//            return std::string("(") + get_string(context, child, childIdx) + ")?";
//        }
//
//        if (childType == Context::ElementType::None)
//        {
//            childIdx = context.insert(child, parentIdx);
//            auto re = breadthSearch(guideTable, starLookup, context, childIdx, child, ++depth);
//            if (!re.empty())
//            {
//                printf("solve (%s) at depth: %d \n", to_string(Opreation::Question).c_str(), depth);
//                context.solved.insert(child);
//                context.res.insert({childIdx,re});
//                return std::string("(") + re + ")?";
//            }
//        }
//    }
//
//    if (parent & CS::one())
//    {
//        auto childs = InverseStar(parent, starLookup);
//
//        for (size_t i = 0; i < childs.size(); i++)
//        {
//            auto child = childs[i];
//            auto [childType, childIdx] = context.check(child);
//
//            if (static_cast<int>(childType) >= 2)
//                return std::string("(") + get_string(context, child, childIdx) + ")*";
//
//            if (childType == Context::ElementType::None)
//            {
//                childIdx = context.insert(child, parentIdx);
//                auto re = breadthSearch(guideTable, starLookup, context, childIdx, child, ++depth);
//                if (!re.empty())
//                {
//                    printf("solve (%s) at depth: %d \n", to_string(Opreation::Star).c_str(), depth);
//                    context.solved.insert(child);
//                    context.res.insert({ childIdx,re });
//                    return std::string("(") + re + ")*";
//                }
//            }
//        }
//    }
//
//    {
//        auto pairs = InverseConcat(parent, guideTable);
//
//        for (size_t k = 0; k < pairs.size(); k++)
//        {
//            auto pair = pairs[k];
//
//            auto [leftType, leftIdx] = context.check(pair.left);
//            auto [rightType, rightIdx] = context.check(pair.right);
//
//            auto leftSolved = static_cast<int>(leftType) >= 2;
//            auto rightSolved = static_cast<int>(rightType) >= 2;
//
//            if (leftType == Context::ElementType::None)
//            {
//                leftIdx = context.insert(pair.left, parentIdx);
//                auto re = breadthSearch(guideTable, starLookup, context, leftIdx, pair.left, ++depth);
//                if (!re.empty())
//                {
//                    printf("solve (%s) at depth: %d \n", to_string(Opreation::Concatenate).c_str(), depth);
//                    context.solved.insert(pair.left);
//                    context.res.insert({ leftIdx ,re });
//                    leftSolved = true;
//                }
//            }
//
//            if (rightType == Context::ElementType::None)
//            {
//                rightIdx = context.insert(pair.right, parentIdx);
//                auto re = breadthSearch(guideTable, starLookup, context, rightIdx, pair.right, ++depth);
//                if (!re.empty())
//                {
//                    printf("solve (%s) at depth: %d \n", to_string(Opreation::Concatenate).c_str(), depth);
//                    context.solved.insert(pair.right);
//                    context.res.insert({ rightIdx ,re });
//                    rightSolved = true;
//                }
//            }
//
//            if (leftSolved && rightSolved)
//                return std::string("(") + get_string(context, pair.left, leftIdx) + get_string(context, pair.right, rightIdx) + ")";
//        }
//    }
//
//    {
//        auto pairs = OrInverse(parent);
//
//        for (size_t k = 0; k < pairs.size(); k++)
//        {
//            auto pair = pairs[k];
//
//            auto [leftType, leftIdx] = context.check(pair.left);
//            auto [rightType, rightIdx] = context.check(pair.right);
//
//            auto leftSolved = static_cast<int>(leftType) >= 2;
//            auto rightSolved = static_cast<int>(rightType) >= 2;
//
//            if (leftType == Context::ElementType::None)
//            {
//                leftIdx = context.insert(pair.left, parentIdx);
//                auto re = breadthSearch(guideTable, starLookup, context, leftIdx, pair.left, ++depth);
//                if (!re.empty())
//                {
//                    printf("solve (%s) at depth: %d \n", to_string(Opreation::Or).c_str(), depth);
//                    context.solved.insert(pair.left);
//                    context.res.insert({ leftIdx ,re });
//                    leftSolved = true;
//                }
//            }
//
//            if (rightType == Context::ElementType::None)
//            {
//                rightIdx = context.insert(pair.right, parentIdx);
//                auto re = breadthSearch(guideTable, starLookup, context, rightIdx, pair.right, ++depth);
//                if (!re.empty())
//                {
//                    printf("solve (%s) at depth: %d \n", to_string(Opreation::Or).c_str(), depth);
//                    context.solved.insert(pair.right);
//                    context.res.insert({ rightIdx ,re });
//                    rightSolved = true;
//                }
//            }
//
//            if (leftSolved && rightSolved)
//                return std::string("(") + get_string(context, pair.left, leftIdx) + "|" + get_string(context, pair.right, rightIdx) + ")";
//        }
//    }
//
//    return std::string();
//}

Result REI(const unsigned short* costFun, const unsigned short maxCost, const std::vector<std::string>& pos, const std::vector<std::string>& neg, double maxTime) {

    auto startTime = std::chrono::steady_clock::now();

    Costs costs(costFun);

    GuideTable guideTable;
    CS posBits, negBits;

    if (!generatingGuideTable(guideTable, posBits, negBits, pos, neg))
    { return Result("not_found", 0, 0, 0); }

    int cache = 100000000;

    CostIntervals intervals(maxCost);
    Context context(cache, posBits, negBits);

    std::string RE;

    if (context.intialCheck(costs.alpha, guideTable.ICsize, pos, neg, RE)) return Result(RE, 0, context.allREs, guideTable.ICsize);

    //auto combinations = context.generateCombinations(guideTable.ICsize);

    //for (size_t i = 0; i < combinations.size(); i++)
    //{
    //    auto re = breadthSearch(guideTable, starLookup, context, i, combinations[i]);
    //    if(!re.empty())
    //    {
    //        printf("solution found! %s \n", re.c_str());
    //    }
    //}

    //return Result(RE, 0, context.allREs, guideTable.ICsize);

    intervals.end(costs.alpha, Operation::Concatenate) = context.lastIdx;
    intervals.end(costs.alpha, Operation::Or) = context.lastIdx;

    int cost{};

    for (cost = costs.alpha + 1; cost < maxCost; cost++)
    {
        auto range = intervals.Interval(cost - 1);
        if (std::get<1>(range) - std::get<0>(range) == 0)
        {
            for (int c = cost - 1; c >= costs.alpha + 1; c--)
            {
                InsertCounter preCounter = context.checkCounter;

                auto [start, end] = intervals.Interval(c - 1);

                for (int i = start; i < end; i++)
                {
                    if (context.Check(i))
                    {
                        printf("solution found!");
                        // solution found!
                        goto exitEnumeration;
                    }
                }

                print((context.checkCounter - preCounter), c, Operation::Question, end - start);
            }

            goto exitEnumeration;
        }

        // Question
        {
            InsertCounter preCounter = context.checkCounter;

            auto [start, end] = intervals.Interval(cost - 1);

            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                auto parent = context.cache[parentIdx];

                if (parent & CS::one())
                {
                    if (context.insertAndCheck(parentIdx, parent & (~CS::one())))
                    { intervals.end(cost, Operation::Question) = INT_MAX; goto exitEnumeration; }
                }
            }

            print((context.checkCounter - preCounter), cost, Operation::Question, end - start);
        }
        intervals.end(cost, Operation::Question) = context.lastIdx;

        context.printCache();

        // Star
        {
            InsertCounter preCounter = context.checkCounter;

            auto [start, end] = intervals.Interval(cost - 1);

            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                if (parentIdx == 66)
                    printf("this is it");

                auto parent = context.cache[parentIdx];

                if (parent & CS::one())
                {
                    auto childs = InverseStar(parent, starLookup);

                    for (size_t i = 0; i < childs.size(); i++)
                    {
                        if (context.insertAndCheck(parentIdx, childs[i]))
                        {
                            intervals.end(cost, Operation::Star) = INT_MAX; goto exitEnumeration;
                        }
                    }
                }
            }

            print((context.checkCounter - preCounter), cost, Operation::Star, end - start);
        }
        intervals.end(cost, Operation::Star) = context.lastIdx;

        context.printCache();

        // Concatenate
        {
            InsertCounter preCounter = context.checkCounter;

            auto [start, end] = intervals.Interval(cost - 1);
            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                auto parent = context.cache[parentIdx];

                auto pairs = InverseConcat(parent, guideTable);

                for (size_t k = 0; k < pairs.size(); k++)
                {
                    auto pair = pairs[k];
                    if (context.insertAndCheck(parentIdx, pair.left, pair.right))
                    { intervals.end(cost, Operation::Concatenate) = INT_MAX; goto exitEnumeration; }
                }
            }

            print((context.checkCounter - preCounter), cost, Operation::Concatenate, end - start);
        }
        intervals.end(cost, Operation::Concatenate) = context.lastIdx;

        context.printCache();

        // Or
        {
            InsertCounter preCounter = context.checkCounter;

            auto [start, end] = intervals.Interval(cost - 1);
            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                auto parent = context.cache[parentIdx];

                auto pairs = OrInverse(parent);

                for (size_t k = 0; k < pairs.size(); k++)
                {
                    auto pair = pairs[k];
                    if (context.insertAndCheck(parentIdx, pair.left, pair.right))
                    {  intervals.end(cost, Operation::Or) = INT_MAX; goto exitEnumeration; }
                }
            }

            print((context.checkCounter - preCounter), cost, Operation::Or, end - start);
        }
        intervals.end(cost, Operation::Or) = context.lastIdx;

        context.printCache();
    }

exitEnumeration:

    if (context.isFound)
    {
        RE = "generate the RE";
        return Result(RE, cost, context.allREs, guideTable.ICsize);
    }

    return Result("not_found", cost > maxCost ? maxCost : cost, context.allREs, guideTable.ICsize);
}

