#include "rei.hpp"

#include <queue>
#include <unordered_set>
#include <unordered_map>

#include <types.h>
#include <guide_table.h>
#include <operations.h>
#include <level_partitioner.h>

using Operation = rei::Operation;
using LevelPartitioner = rei::LevelPartitioner;

#define LOG_OP(level, op_string, allpairs, counter) \
        printf("Level %-2d | (%s) | AllPairs: %-11lu | S %-5d | NV %-11lu | V %-11lu | VSS %-11lu | SR %-5d | G %-5d \n", \
            level, op_string.c_str() ,allpairs,  counter.solved, counter.notVisited, counter.visited, counter.visitedSolutionSet, counter.solvedReused, counter.given);

class CSResolverInterface {
public:
    virtual std::string resolve(const CS& cs) = 0;
};

class Context {
public:
    struct Counter {
        uint64_t solved;
        uint64_t notVisited;
        uint64_t visited;
        uint64_t visitedSolutionSet;
        uint64_t solvedReused;
        uint64_t given;
    };

    Context(int cache_capacity, CSResolverInterface* resolver)
        : cache_capacity(cache_capacity), resolver(resolver) {

        leftChildIdx = new int[2 * (cache_capacity + 1)];
        nodeStatus = new int[cache_capacity + 1];
        parentIdx = new int[cache_capacity + 1];
        cache = new CS[cache_capacity + 1];
        lastIdx = 0;
        isFound = false;
        allPairs = 0;
        counter = {};
    }

    ~Context() {
        delete[] cache;
        delete[] leftChildIdx;
        delete[] parentIdx;
        delete[] nodeStatus;
    }

    bool intialCheck(int alphabetsSize, const std::vector<std::string>& pos, std::string& RE)
    {
        // Checking empty & eps

        if (pos.empty()) { RE = "Empty"; return true; }

        if ((pos.size() == 1) && (pos.at(0).empty())) { RE = "eps"; return true; }
        visited[CS::one()] = -1;
        solved.insert(CS::one()); // int(CS of empty) is 1

        // Checking the alphabets

        // Pointing to the position of the first char of the alphabet (idx 0 is for epsilon)
        auto lastAlphaIdx = CS::one() << 1;

        for (int i = 0; i < alphabetsSize; ++i) {

            std::string s = resolver->resolve(lastAlphaIdx);
            if ((pos.size() == 1) && (pos.at(0) == s)) { RE = s; return true; }

            visited[lastAlphaIdx] = -1;
            solved.insert(lastAlphaIdx);

            lastAlphaIdx <<= 1;
        }

        return false;
    }

    std::vector<CS> generateSolutionSet(int icSize, const CS& posBits, const CS& negBits)
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

    std::vector<CS> addSolutionSet(int iCSize, const CS& posBits, const CS& negBits) {
        auto solutionSet = generateSolutionSet(iCSize, posBits, negBits);
        for (size_t i = 0; i < solutionSet.size(); i++)
        {
            visited[solutionSet[i]] = -1;
        }
        return solutionSet;
    }

    enum class NodeType
    {
        NotVistied  = 0,
        Vistied     = 1,
        Solved      = 2,
    };

    NodeType insert(CS cs, int pIdx)
    {
        NodeType nt;

        if (visited.find(cs) == visited.end())
        {
            counter.notVisited++;
            cache[lastIdx] = cs;
            visited[cs] = lastIdx;
            nodeStatus[lastIdx] = 0;
            nt = NodeType::NotVistied;
        }
        else if (solved.find(cs) == solved.end()) 
        {
            // to ignore the solved set
            if (visited.at(cs) > -1)
            {
                nodeStatus[lastIdx] = visited.at(cs);
                counter.visited++;
            }else
                counter.visitedSolutionSet++;
            nt = NodeType::Vistied;
        }
        else
        {
            if (visited.at(cs) == -1)
                counter.given++;
            else
                counter.solvedReused++;

            nodeStatus[lastIdx] = visited.at(cs);
            idxToCS[lastIdx] = cs;
            nt = NodeType::Solved;
        }

        parentIdx[lastIdx++]  = pIdx;
        return nt;
    }

    bool checkAllVisited(int& sIdx) {
        for (size_t idx = 0; idx < lastIdx; idx+=2)
        {
            auto ls = nodeStatus[idx];
            auto rs = nodeStatus[idx + 1];

            auto lsolved = ls > 0 && idxToCS.find(ls) != idxToCS.end();
            auto rsolved = rs > 0 && idxToCS.find(rs) != idxToCS.end();

            if (!lsolved || !rsolved) continue;

            isFound = recursiveCheck(parentIdx[idx], idx);

            if (isFound)
            {
                sIdx = idx;
                return true;
            }
        }
        return false;
    }

    bool recursiveCheck(int index, int lcIdx)
    {
        // we can reconstruct the cs recursively, we don't need cache
        solved.insert(cache[index]);
        leftChildIdx[index] = lcIdx;
        idxToCS[index] = cache[index];
        counter.solved++;

        int pIdx = parentIdx[index];

        int sIdx = index % 2 == 0 ? index + 1 : index - 1;

        // check if the sibling is solved
        int sStatus = nodeStatus[sIdx];
        bool sSolved = sStatus == -1;
        if (sStatus > 0)
            sSolved = idxToCS.find(sStatus) != idxToCS.end();

        if (!sSolved) return false;

        if (pIdx < 0) // the solved set parents is -1
            return true;

        return recursiveCheck(pIdx, index < sIdx ? index : sIdx);
    }

    int getOutmostParent(int index) {
        if (parentIdx[index] == -1)
            return index;
        else
            return getOutmostParent(parentIdx[index]);
    }

    std::string bracket(std::string s) {
        int p = 0;
        for (int i = 0; i < s.length(); i++) {
            if (s[i] == '(') p++;
            else if (s[i] == ')') p--;
            else if (s[i] == '+' && p <= 0) return "(" + s + ")";
        }
        return s;
    }

    std::string constructDownward(int index, const LevelPartitioner& partioner) 
    {
        std::string left;
        if (nodeStatus[index] == -1)
            left = resolver->resolve(idxToCS.at(index));
        else
        {
            auto lIdx = nodeStatus[index] == 0 ? index : nodeStatus[index];
            left = constructDownward(leftChildIdx[lIdx], partioner);
        }

        int level;
        Operation op;
        partioner.indexToLevel(index, level, op);

        if (op == Operation::Question)
        {
            if (left.length() == 1)
                return left + "?";
            else
                return "(" + left + ")?";
        }

        if (op == Operation::Star)
        {
            if (left.length() == 1)
                return left + "*";
            else
                return "(" + left + ")*";
        }

        std::string right;
        if (nodeStatus[++index] == -1)
            right = resolver->resolve(idxToCS.at(index));
        else
        {
            auto rIdx = nodeStatus[index] == 0 ? index : nodeStatus[index];
            right = constructDownward(leftChildIdx[rIdx], partioner);
        }

        if (op == Operation::Concatenate)
        {
            return bracket(left) + bracket(right);
        }

        if (op == Operation::Or)
        {
            return left + "+" + right;
        }
    }

    bool insertAndCheck(int parentIdx, CS left, CS right)
    {
        allPairs++;

        auto lt = insert(left, parentIdx);
        auto rt = insert(right, parentIdx);

        if ((static_cast<int>(lt) >= 2) && (static_cast<int>(rt) >= 2))
        {
            isFound = recursiveCheck(parentIdx, lastIdx - 2);
            return isFound;
        }

        if ((static_cast<int>(lt) >= 1) && (static_cast<int>(rt) >= 1))
        {
            // need to handle this case somehow
            // need to be stored for later checks!
        }

        return false;
    }

    bool insertAndCheck(int parentIdx, CS child)
    {
        return insertAndCheck(parentIdx, child, CS::one());
    }

    int cache_capacity;
    int* leftChildIdx;
    int* parentIdx;
    int* nodeStatus;

    CS* cache;
    std::unordered_map<CS, int> visited;
    std::unordered_set<CS> solved;
    std::unordered_map<int, CS> idxToCS;
    CSResolverInterface* resolver;

    Counter counter;
    unsigned long allPairs;
    // Index of the last free position in the language cache
    int lastIdx;
    bool isFound;
};

class AlphabetResolver : public CSResolverInterface
{
public:
    AlphabetResolver(std::set<char> alphabets) 
    {
        map[CS::one()] = "eps";
        auto alphabetCS = CS::one() << 1;
        for (auto it = alphabets.begin(); it != alphabets.end(); it++) {
            auto c = *it;
            map[alphabetCS] = std::string(1, c);
            alphabetCS = alphabetCS << 1;
        }
    }

    std::string resolve(const CS& cs) override {
        return map.at(cs);
    }
private:
    std::unordered_map<CS, std::string> map;
};

std::set<char> findAlphabets(const std::vector<std::string>& pos, const std::vector<std::string>& neg) {
    std::set<char> alphabet;
    for (auto& word : pos) for (auto ch : word) alphabet.insert(ch);
    for (auto& word : neg) for (auto ch : word) alphabet.insert(ch);
    return alphabet;
}

rei::Result rei::Run(const unsigned short* costFun, const unsigned short maxCost, 
    const std::vector<std::string>& pos, const std::vector<std::string>& neg, double maxTime)
{
    int maxLevel = 500;
    std::string RE;

    GuideTable guideTable;
    CS posBits, negBits;

    if (!generatingGuideTable(guideTable, posBits, negBits, pos, neg))
    { return Result("not_found", 0, 0, 0); }

    StarLookup starLookup(guideTable);

    auto alphabets = findAlphabets(pos, neg);
    CSResolverInterface* resolver = new AlphabetResolver(alphabets);

    int cache = 100000000;

    LevelPartitioner partitioner(maxLevel);
    Context context(cache, resolver);

    if (context.intialCheck(alphabets.size(), pos, RE)) return Result(RE, 0, /*context.allREs*/ 0, guideTable.ICsize);

    // the index 0 and 1 are reserved for checking
    partitioner.start(0, Operation::Question) = 2;
    context.lastIdx = 2;

    // generate the solution set and fill the first level
    {
        auto solutionSet = context.addSolutionSet(guideTable.ICsize, posBits, negBits);

        // Question
        {
            for (const auto& parent : solutionSet)
            {

                if (parent & CS::one())
                {
                    if (context.insertAndCheck(-1, parent & (~CS::one())))
                    {
                        LOG_OP(0, to_string(Operation::Question), context.allPairs, context.counter);
                        partitioner.end(0, Operation::Question) = INT_MAX; goto exitEnumeration;
                    }
                }
            }
        }
        partitioner.end(0, Operation::Question) = context.lastIdx;
        LOG_OP(0, to_string(Operation::Question), context.allPairs, context.counter);

        // Star
        {
            for (const auto& parent : solutionSet)
            {
                if (parent & CS::one())
                {
                    auto childs = rei::invertStar(parent, starLookup);

                    for (size_t i = 0; i < childs.size(); i++)
                    {
                        if (context.insertAndCheck(-1, childs[i]))
                        {
                            LOG_OP(0, to_string(Operation::Star), context.allPairs, context.counter);
                            partitioner.end(0, Operation::Star) = INT_MAX; goto exitEnumeration;
                        }
                    }
                }
            }
        }
        partitioner.end(0, Operation::Star) = context.lastIdx;
        LOG_OP(0, to_string(Operation::Star), context.allPairs, context.counter);

        // Concatenate
        {
            for (const auto& parent : solutionSet)
            {
                auto pairs = invertConcat(parent, guideTable);

                for (size_t k = 0; k < pairs.size(); k++)
                {
                    auto pair = pairs[k];
                    if (context.insertAndCheck(-1, pair.left, pair.right))
                    {
                        LOG_OP(0, to_string(Operation::Concatenate), context.allPairs, context.counter);
                        partitioner.end(0, Operation::Concatenate) = INT_MAX; goto exitEnumeration;
                    }
                }
            }
        }
        partitioner.end(0, Operation::Concatenate) = context.lastIdx;
        LOG_OP(0, to_string(Operation::Concatenate), context.allPairs, context.counter);

        // Or
        {
            for (const auto& parent : solutionSet)
            {
                auto pairs = invertOr(parent);

                for (size_t k = 0; k < pairs.size(); k++)
                {
                    auto pair = pairs[k];
                    if (context.insertAndCheck(-1, pair.left, pair.right))
                    {
                        LOG_OP(0, to_string(Operation::Or), context.allPairs, context.counter);
                        partitioner.end(0, Operation::Or) = INT_MAX; goto exitEnumeration;
                    }
                }
            }
        }
        partitioner.end(0, Operation::Or) = context.lastIdx;
        LOG_OP(0, to_string(Operation::Or), context.allPairs, context.counter);
    }

    int solvedIndex;
    int level;
    for (level = 1; level < maxLevel; level++)
    {
        {
            auto [start, end] = partitioner.Interval(level - 1);
            if (end - start < 1) {
                int sIdx;
                context.checkAllVisited(solvedIndex);
                goto exitEnumeration;
            }
        }

        // Question
        {
            auto [start, end] = partitioner.Interval(level - 1);

            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                auto parent = context.cache[parentIdx];
                if (parent == CS()) continue;

                if (parent & CS::one())
                {
                    if (context.insertAndCheck(parentIdx, parent & (~CS::one())))
                    {
                        LOG_OP(level, to_string(Operation::Question), context.allPairs, context.counter);
                        solvedIndex = context.lastIdx - 1;
                        partitioner.end(level, Operation::Question) = INT_MAX; goto exitEnumeration;
                    }
                }
            }
        }
        partitioner.end(level, Operation::Question) = context.lastIdx;
        LOG_OP(level, to_string(Operation::Question), context.allPairs, context.counter);

        // Star
        {
            auto [start, end] = partitioner.Interval(level - 1);

            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                auto parent = context.cache[parentIdx];
                if (parent == CS()) continue;

                if (parent & CS::one())
                {
                    auto childs = rei::invertStar(parent, starLookup);

                    for (size_t i = 0; i < childs.size(); i++)
                    {
                        if (context.insertAndCheck(parentIdx, childs[i]))
                        {
                            LOG_OP(level, to_string(Operation::Star), context.allPairs, context.counter);
                            solvedIndex = context.lastIdx - 1;
                            partitioner.end(level, Operation::Star) = INT_MAX; goto exitEnumeration;
                        }
                    }
                }
            }
        }
        partitioner.end(level, Operation::Star) = context.lastIdx;
        LOG_OP(level, to_string(Operation::Star), context.allPairs, context.counter);

        // Concatenate
        {
            auto [start, end] = partitioner.Interval(level - 1);
            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                auto parent = context.cache[parentIdx];

                if (parent == CS()) continue;

                auto pairs = invertConcat(parent, guideTable);

                for (size_t k = 0; k < pairs.size(); k++)
                {
                    auto pair = pairs[k];

                    if (context.insertAndCheck(parentIdx, pair.left, pair.right))
                    {
                        LOG_OP(level, to_string(Operation::Concatenate), context.allPairs, context.counter);
                        solvedIndex = context.lastIdx - 1;
                        partitioner.end(level, Operation::Concatenate) = INT_MAX; goto exitEnumeration;
                    }
                }
            }
        }
        partitioner.end(level, Operation::Concatenate) = context.lastIdx;
        LOG_OP(level, to_string(Operation::Concatenate), context.allPairs, context.counter);

        // Or
        {
            auto [start, end] = partitioner.Interval(level - 1);
            for (int parentIdx = start; parentIdx < end; parentIdx++)
            {
                auto parent = context.cache[parentIdx];
                if (parent == CS()) continue;

                auto pairs = invertOr(parent);

                for (size_t k = 0; k < pairs.size(); k++)
                {
                    auto pair = pairs[k];
                    if (context.insertAndCheck(parentIdx, pair.left, pair.right))
                    {
                        LOG_OP(level, to_string(Operation::Or), context.allPairs, context.counter);
                        solvedIndex = context.lastIdx - 1;
                        partitioner.end(level, Operation::Or) = INT_MAX; goto exitEnumeration;
                    }
                }
            }
        }
        partitioner.end(level, Operation::Or) = context.lastIdx;
        LOG_OP(level, to_string(Operation::Or), context.allPairs, context.counter);
    }

exitEnumeration:

    if (context.isFound)
    {
        int rootIdx = context.getOutmostParent(solvedIndex);
        RE = context.constructDownward(rootIdx % 2 == 0 ? rootIdx : rootIdx - 1, partitioner);
        return Result(RE, level, context.allPairs, guideTable.ICsize);
    }

    return Result("not_found", level > maxLevel ? maxCost : level, context.allPairs, guideTable.ICsize);
}
