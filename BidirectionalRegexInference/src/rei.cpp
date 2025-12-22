#include "rei.hpp"

#include <span>
#include <queue>
#include <unordered_set>
#include <unordered_map>

#include <types.h>
#include <guide_table.h>
#include <operations.h>
#include <level_partitioner.h>

using Operation = rei::Operation;
using LevelPartitioner = rei::LevelPartitioner;

#define LOG_OP(levelnum, op_string, allpairs, counter) \
        printf("Level %-2d | (%s) | AllPairs: %-11llu | S %-5llu | NV %-11llu | V %-11llu | C %-11llu | SS %-5llu | G %-5llu \n", \
            levelnum, op_string.c_str() ,allpairs,  counter.solved, counter.notVisited, counter.visited, counter.cyclic, counter.selfSolved, counter.given);

class CSResolverInterface {
public:
    virtual std::string resolve(const CS& cs) = 0;
};

class Context {
public:
    enum class NodeType
    {
        NotVistied  = 0,
        Cyclic      = 1,
        Vistied     = 2,
        SelfSolved  = 3, // Solved by this graph
        Given       = 4,
    };

    struct Counter {

        uint64_t solved;
        uint64_t notVisited;
        uint64_t visited;
        uint64_t cyclic;
        uint64_t selfSolved;
        uint64_t given;

        void update(const NodeType& nt) {
            switch (nt) {
            case NodeType::NotVistied:
                notVisited++;
                break;
            case NodeType::Cyclic:
                cyclic++;
                break;
            case NodeType::Vistied:
                visited++;
                break;
            case NodeType::SelfSolved:
                selfSolved++;
                break;
            case NodeType::Given:
                given++;
                break;
            }
        }
    };

    Context(int cache_capacity, CSResolverInterface* resolver)
        : cache_capacity(cache_capacity), resolver(resolver) {

        status = new int[cache_capacity + 2];
        parentIdx = new int[cache_capacity + 2];
        cache = new CS[cache_capacity + 2];
        lastIdx = 0;
        isFound = false;
        allPairs = 0;
        counter = {};
    }

    ~Context() {
        delete[] cache;
        delete[] parentIdx;
        delete[] status;
    }

    bool intialCheck(int alphabetsSize, const std::vector<std::string>& pos, std::string& RE)
    {
        // Checking empty & eps

        if (pos.empty()) { RE = "Empty"; return true; }

        if ((pos.size() == 1) && (pos.at(0).empty())) { RE = "eps"; return true; }
        visited[CS::one()] = -1;
        solved.insert(CS::one());

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

    NodeType getNodeType(const CS& cs)
    {
        auto vit = visited.find(cs);
        if (vit == visited.end())
            return NodeType::NotVistied;
        if (solved.find(cs) == solved.end())
        {
            if ((*vit).second == -1) // we only test the solution set
                return NodeType::Cyclic;
            else
                return NodeType::Vistied;
        }
        else
        {
            if ((*vit).second == -1)
                return NodeType::Given;
            else
                return NodeType::SelfSolved;
        }
    }

    void insert(NodeType nodeType,CS cs, int pIdx)
    {
        switch (nodeType) {
        case NodeType::NotVistied:
            cache[lastIdx] = cs;
            visited[cs] = lastIdx;
            status[lastIdx] = 0;
            break;
        case NodeType::Vistied:
            status[lastIdx] = - visited.at(cs);
            break;
        case NodeType::SelfSolved:
            status[lastIdx] = - visited.at(cs);
            idxToSolved[lastIdx] = cs;
            break;
        case NodeType::Given:
            status[lastIdx] = -1;
            idxToSolved[lastIdx] = cs;
            break;
        }

        parentIdx[lastIdx++]  = pIdx;
    }

    bool isSolved(int idx) {
        auto s = status[idx];
        if (s == -1 || s > 1) return true;
        if (s < -1) return idxToSolved.find(-s) != idxToSolved.end();
        return false;
    }

    bool checkAllVisited(int& sIdx) {
        for (size_t idx = 2; idx < lastIdx; idx += 2)
        {
            if (!isSolved(idx) || !isSolved(idx + 1)) continue;

            isFound = parentIdx[idx] == -1 ? true : recursiveCheck(parentIdx[idx], idx);

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
        //if (isSolved(index)) return false;

        // we can reconstruct the cs recursively, we don't need cache
        solved.insert(cache[index]);
        status[index]       = lcIdx;
        idxToSolved[index]  = cache[index];

        counter.solved++;

        int pIdx = parentIdx[index];
        int sIdx = index % 2 == 0 ? index + 1 : index - 1;

        if (!isSolved(sIdx)) return false;

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
        if (status[index] == -1)
            left = resolver->resolve(idxToSolved.at(index));
        else
        {
            auto ls = status[index];
            left = constructDownward(ls < -1 ? status[-ls] : ls, partioner);
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
        if (status[++index] == -1)
            right = resolver->resolve(idxToSolved.at(index));
        else
        {
            auto rs = status[index];
            right = constructDownward(rs < -1 ? status[-rs] : rs, partioner);
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
        if (lastIdx == cache_capacity + 2) return true;

        allPairs++;

        auto lt = getNodeType(left);
        auto rt = getNodeType(right);

        counter.update(lt);
        counter.update(rt);

        if (lt == NodeType::Cyclic || rt == NodeType::Cyclic)
            return false;

        insert(lt, left, parentIdx);
        insert(rt, right, parentIdx);

        if ((static_cast<int>(lt) > 2) && (static_cast<int>(rt) > 2))
        {
            isFound = recursiveCheck(parentIdx, lastIdx - 2);
            return isFound;
        }

        if ((static_cast<int>(lt) > 1) || (static_cast<int>(rt) > 1))
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
    int* parentIdx;
    int* status; // 0 = the original node, -1 = given, < -1 = redirectIdx, > 1 = leftIdx
    CS* cache;

    std::unordered_map<CS, int> visited;
    std::unordered_set<CS> solved;
    std::unordered_map<int, CS> idxToSolved;
    CSResolverInterface* resolver;

    Counter counter;
    uint64_t allPairs;
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

std::pair<bool,int> processLevel(Context& context, LevelPartitioner& partitioner, 
    const rei::GuideTable& guideTable, const rei::StarLookup& starLookup, 
    std::span<CS> level, int levelnum, int startPIdx, bool overrideParent = false, int opIdx = 0) {

    // Question
    int pIdx = startPIdx - 1;
    for (const auto& parent : level)
    {
        pIdx++;
        if (parent == CS()) continue;

        if (parent & CS::one())
        {
            if (context.insertAndCheck(overrideParent ? opIdx : pIdx, parent & (~CS::one())))
            {
                LOG_OP(levelnum, to_string(Operation::Question), context.allPairs, context.counter);
                partitioner.end(levelnum, Operation::Question) = INT_MAX;
                return { true, context.lastIdx - 1 };
            }
        }
    }
    partitioner.end(levelnum, Operation::Question) = context.lastIdx;
    LOG_OP(levelnum, to_string(Operation::Question), context.allPairs, context.counter);

    // Star
    pIdx = startPIdx - 1;
    for (const auto& parent : level)
    {
        pIdx++;
        if (parent == CS()) continue;

        if (parent & CS::one())
        {
            auto childs = rei::invertStar(parent, starLookup);

            for (size_t i = 0; i < childs.size(); i++)
            {
                if (context.insertAndCheck(overrideParent ? opIdx : pIdx, childs[i]))
                {
                    LOG_OP(levelnum, to_string(Operation::Star), context.allPairs, context.counter);
                    partitioner.end(levelnum, Operation::Star) = INT_MAX;
                    return { true, context.lastIdx - 1 };
                }
            }
        }
    }
    partitioner.end(levelnum, Operation::Star) = context.lastIdx;
    LOG_OP(levelnum, to_string(Operation::Star), context.allPairs, context.counter);

    // Concatenate
    pIdx = startPIdx - 1;
    for (const auto& parent : level)
    {
        pIdx++;
        if (parent == CS()) continue;

        auto pairs = invertConcat(parent, guideTable);

        for (size_t i = 0; i < pairs.size(); i++)
        {
            auto pair = pairs[i];

            if (context.insertAndCheck(overrideParent ? opIdx : pIdx, pair.left, pair.right))
            {
                LOG_OP(levelnum, to_string(Operation::Concatenate), context.allPairs, context.counter);
                partitioner.end(levelnum, Operation::Concatenate) = INT_MAX;
                return { true, context.lastIdx - 1 };
            }
        }
    }
    partitioner.end(levelnum, Operation::Concatenate) = context.lastIdx;
    LOG_OP(levelnum, to_string(Operation::Concatenate), context.allPairs, context.counter);

    // Or
    pIdx = startPIdx - 1;
    for (const auto& parent : level)
    {
        pIdx++;
        if (parent == CS()) continue;

        auto pairs = invertOr(parent);

        for (size_t i = 0; i < pairs.size(); i++)
        {
            auto pair = pairs[i];
            if (context.insertAndCheck(overrideParent ? opIdx : pIdx, pair.left, pair.right))
            {
                LOG_OP(levelnum, to_string(Operation::Or), context.allPairs, context.counter);
                partitioner.end(levelnum, Operation::Or) = INT_MAX;
                return { true, context.lastIdx - 1 };
            }
        }
    }
    partitioner.end(levelnum, Operation::Or) = context.lastIdx;
    LOG_OP(levelnum, to_string(Operation::Or), context.allPairs, context.counter);

    return { false, 0 };
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

    int solvedIndex;
    int level = 0;

    // generate the solution set and fill the first level
    {
        auto solutionSet = context.addSolutionSet(guideTable.ICsize, posBits, negBits);

        auto res = processLevel(context, partitioner, guideTable, starLookup, 
                    std::span(solutionSet), level++, 2, true, -1);

        if (res.first) {
            solvedIndex = res.second;
            goto exitEnumeration;
        }
    }

    for (level = 1; level < maxLevel; level++)
    {
        auto [start, end] = partitioner.Interval(level - 1);

        {
            if (end - start < 1) {
                int sIdx;
                context.checkAllVisited(solvedIndex);
                goto exitEnumeration;
            }
        }

        auto res = processLevel(context, partitioner, guideTable, starLookup,
            std::span(context.cache + start, end - start), level, start);

        if (res.first) {
            solvedIndex = res.second;
            goto exitEnumeration;
        }
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
