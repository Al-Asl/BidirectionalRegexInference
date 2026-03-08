#include <top_down.hpp>

#include <cs_utils.h>

#define LOG_OP(levelIdx, op_string, allCS, counter) \
        printf("Level %-2d | (%s) | AllCS: %-11llu | S %-5llu | NV %-11llu | V %-11llu | C %-11llu | SS %-5llu | G %-5llu \n", \
            levelIdx + 1, op_string.c_str() ,allCS,  counter.solved, counter.notVisited, counter.visited, counter.cyclic, counter.selfSolved, counter.given);

// the start index of the language cache
#define LC_START 2

#define NEXTVISITED_GIVEN -1

#define VISITED_SOLUTIONSET 1
#define VISITED_GIVEN -1

#define PARENTIDX_SOLUTIONSET 0

using namespace rei;

rei::TopDownSearch::Context::Context(int cache_capacity)
{
    nextVisited     = new int[cache_capacity + LC_START];
    parentIdx       = new int[cache_capacity + LC_START];
    cache           = new CS[cache_capacity + LC_START];

    lastIdx = 0;
    allCS = 0;
    counter = {};
}

rei::TopDownSearch::Context::~Context()
{
    delete[] cache;
    delete[] parentIdx;
    delete[] nextVisited;
}

void rei::TopDownSearch::Context::AddSolutionSet(const std::vector<CS>& solutionSet) {
    for (size_t i = 0; i < solutionSet.size(); i++)
        visited[solutionSet[i]] = VISITED_SOLUTIONSET;
}

bool rei::TopDownSearch::Context::AddSolvedNode(const CS& cs, int& solutionIdx) {
    if (visited.find(cs) == visited.end())
    {
        visited[cs] = VISITED_GIVEN;
        return false;
    }
    else
    {
        auto idx = visited.at(cs);
        idx = idx >= LC_START ? idx : -idx; // self solved nodes are also discarded because they are not minimum

        //TODO: prune at idx first

        // collect all visited
        std::vector<int> toCheck;
        toCheck.push_back(idx);
        while (nextVisited[idx] >= LC_START)
        {
            toCheck.push_back(nextVisited[idx]);
            idx = nextVisited[idx];
        }

        std::vector<int> solvedIdx;

        // set all of them to solved
        visited[cs] = VISITED_GIVEN;
        for (int i = 0; i < toCheck.size(); i++)
        {
            cache[toCheck[i]]       = CS();
            solved[toCheck[i]]      = SolvedNode(cs);
            nextVisited[toCheck[i]] = NEXTVISITED_GIVEN;
        }

        // run check
        for (int i = 0; i < toCheck.size(); i++)
        {
            if (checkSibling(toCheck[i], solvedIdx, solutionIdx))
                return true;
        }

        for (int i = 0; i < solvedIdx.size(); i++)
        {
            if (checkVisited(solvedIdx[i], solvedIdx, solutionIdx))
                return true;
        }

        return false;
    }
}

bool rei::TopDownSearch::Context::checkSibling(int idx, std::vector<int>& solvedIdx, int& solutionIdx) {
    auto sidx = idx % 2 == 0 ? idx + 1 : idx - 1;

    if (!isSolved(sidx))
        return false;

    if (parentIdx[idx] == PARENTIDX_SOLUTIONSET ? true : recursiveCheck(parentIdx[idx], idx < sidx ? idx : sidx, solvedIdx))
    {
        solutionIdx = getOutmostParent(idx);
        return true;
    }

    return false;
}

bool rei::TopDownSearch::Context::checkVisited(int originalIdx, std::vector<int>& solvedIdx, int& solutionIdx) {

    auto idx = nextVisited[originalIdx];
    while (idx >= LC_START)
    {
        if (checkSibling(idx, solvedIdx, solutionIdx))
            return true;
        idx = nextVisited[idx];
    }

    return false;
}

bool rei::TopDownSearch::Context::InsertAndCheck(int parentIdx, CS left, CS right, int& solutionIdx)
{
    allCS += 2;

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
        if (parentIdx == PARENTIDX_SOLUTIONSET) {
            solutionIdx = lastIdx - 1;
            return true;
        }

        std::vector<int> solvedIdx;

        if (recursiveCheck(parentIdx, lastIdx - 2, solvedIdx))
        {
            solutionIdx = lastIdx - 1;
            return true;
        }

        for (int i = 0; i < solvedIdx.size(); i++)
        {
            if (checkVisited(solvedIdx[i], solvedIdx, solutionIdx))
                return true;
        }

        return false;
    }

    return false;
}

bool rei::TopDownSearch::Context::InsertAndCheck(int parentIdx, CS child, int& solutionIdx)
{
    return InsertAndCheck(parentIdx, child, CS::one(), solutionIdx);
}

int rei::TopDownSearch::Context::GetLastOutmostParent(int solutionIndex) {
    int pIdx = getOutmostParent(solutionIndex);
    return pIdx % 2 == 0 ? pIdx : pIdx - 1;
}

rei::TopDownSearch::Context::NodeType rei::TopDownSearch::Context::getNodeType(const CS& cs)
{
    auto vit = visited.find(cs);
    if (vit == visited.end())
        return NodeType::NotVistied;

    auto idx = (*vit).second;

    if (idx <= -LC_START)
        return NodeType::SelfSolved;
    else if (idx == VISITED_GIVEN)
        return NodeType::Given;
    else if (idx == VISITED_SOLUTIONSET)
        return NodeType::Cyclic; // we only test the solution set
    else
        return NodeType::Vistied;
}

void appendDuplicate(int* nextVisited,int originalIdx, int idx) {
    nextVisited[idx] = -originalIdx;
    while (nextVisited[originalIdx] >= LC_START)
        originalIdx = nextVisited[originalIdx];
    nextVisited[originalIdx] = idx;
}

int getOriginal(int* nextVisited, int idx) {
    while (idx >= LC_START)
        idx = nextVisited[idx];
    return -idx;
}

void rei::TopDownSearch::Context::insert(NodeType nodeType, CS cs, int pIdx)
{
    switch (nodeType) {
    case NodeType::NotVistied:
        cache[lastIdx] = cs;
        visited[cs] = lastIdx;
        nextVisited[lastIdx] = -lastIdx;
        break;
    case NodeType::Vistied:
        appendDuplicate(nextVisited, visited.at(cs), lastIdx);
        break;
    case NodeType::SelfSolved:
        appendDuplicate(nextVisited, -visited.at(cs), lastIdx); // it's already negative
        solved[lastIdx] = SolvedNode(cs);
        break;
    case NodeType::Given:
        nextVisited[lastIdx] = NEXTVISITED_GIVEN;
        solved[lastIdx] = SolvedNode(cs);
        break;
    }

    parentIdx[lastIdx++] = pIdx;
}

bool rei::TopDownSearch::Context::isSolved(int idx) {

    auto nextIdx = nextVisited[idx];
    if (nextIdx == NEXTVISITED_GIVEN) return true;
    return solved.find(getOriginal(nextVisited, nextIdx)) != solved.end();
}

bool rei::TopDownSearch::Context::recursiveCheck(int index, int lcIdx, std::vector<int>& solvedIdx)
{
    // this is important because there is now way to protect against cyclic nodes
    if (isSolved(index)) return false;

    // we can reconstruct the cs recursively, we don't need cache
    visited[cache[index]] = -index;
    solved[index] = SolvedNode(cache[index], lcIdx);
    solvedIdx.push_back(index);

    counter.solved++;

    int pIdx = parentIdx[index];
    int sIdx = index % 2 == 0 ? index + 1 : index - 1;

    if (!isSolved(sIdx)) return false;

    if (pIdx == PARENTIDX_SOLUTIONSET)
        return true;

    return recursiveCheck(pIdx, index < sIdx ? index : sIdx, solvedIdx);
}

int rei::TopDownSearch::Context::getOutmostParent(int index) {
    if (parentIdx[index] == PARENTIDX_SOLUTIONSET)
        return index;
    else
        return getOutmostParent(parentIdx[index]);
}


rei::TopDownSearch::TopDownSearch(const rei::GuideTable& guideTable,
    std::shared_ptr<rei::CSResolverInterface> resolver, int maxLevel, const CS& posBits, const CS& negBits, int cache_capacity) :
    guideTable(guideTable), resolver(resolver), partitioner(maxLevel), context(cache_capacity),
    maxLevel(maxLevel), posBits(posBits), negBits(negBits), cache_capacity(cache_capacity) {

    partitioner.start(0, Operation::Question) = LC_START;
    context.lastIdx = LC_START;
}

bool rei::TopDownSearch::Push(const CS& cs, TopDownSearchResult& res) {
    int solutionIndex;
    if (context.AddSolvedNode(cs, solutionIndex))
    {
        auto idx = context.GetLastOutmostParent(solutionIndex);
        res.RE = constructDownward(idx);
        res.allCS = context.lastIdx - LC_START;
        return true;
    }
    else
        return false;
}

EnumerationState rei::TopDownSearch::EnumerateLevel(TopDownSearchResult& res)
{
    if (level == maxLevel) return EnumerationState::End;

    EnumerationState enumState;
    int solved;

    if (level == 0)
    {
        vector<CS> solutionSet;
        if(heuristicConfigs.solutionSetUseRandomSampling)
            solutionSet = randomSampleSolutionSet(heuristicConfigs.solutionSetMaxSamples);
        else
            solutionSet = generateSolutionSet();

        context.AddSolutionSet(solutionSet);
        enumState = enumerateLevel(solutionSet, LC_START, solved, true, PARENTIDX_SOLUTIONSET);
    }
    else
    {
        auto [start, end] = partitioner.Interval(level - 1);

        if (end - start > 0)
            enumState = enumerateLevel(std::span(context.cache + start, end - start), start, solved);
        else
            enumState = EnumerationState::End;
    }

    if (enumState != EnumerationState::NotFound)
    {
        if (enumState == EnumerationState::Found)
            res.RE = constructDownward(solved);
        res.allCS = context.lastIdx - LC_START;
    }

    level++;
    return enumState;
}

uint64_t rei::TopDownSearch::EstimateNextLevelCS()
{
    if(!(heuristicConfigs.solutionSetUseRandomSampling && heuristicConfigs.invertStarUseRandomSampling &&
        heuristicConfigs.invertConcatUseRandomSampling && heuristicConfigs.invertOrUseRandomSampling))
        throw std::invalid_argument("All heuristic need to be enabled before calling EstimateNextLevelCS");

    uint64_t multi = 1 + heuristicConfigs.invertStarMaxSamples + 2 * heuristicConfigs.invertConcatMaxSamples + 2 * heuristicConfigs.invertOrMaxSamples;

    if (level == 0)
        return multi * heuristicConfigs.solutionSetMaxSamples;
    else
    {
        auto [start, end] = partitioner.Interval(level - 1);
        return multi * (end - start);
    }
}

void rei::TopDownSearch::SetHeuristic(HeuristicConfigs configs)
{
    heuristicConfigs = configs;
}

std::vector<CS> rei::TopDownSearch::randomSampleSolutionSet(size_t maxSamples, uint64_t seed)
{
    std::vector<int> dontCareBits;
    dontCareBits.reserve(guideTable.ICsize);

    const CS combined = posBits | negBits;
    for (int i = 0; i < guideTable.ICsize; ++i)
    {
        const CS bitMask = CS::one() << i;
        if ((bitMask & combined) == CS())
        {
            dontCareBits.push_back(i);
        }
    }

    const size_t numDontCareBits = dontCareBits.size();

    if (numDontCareBits < 64 && (1ULL << numDontCareBits) <= maxSamples)
        return generateSolutionSet();

    std::vector<CS> result;
    result.reserve(maxSamples);

    std::mt19937_64 rng(seed);
    std::bernoulli_distribution coin(0.5);

    std::unordered_set<CS> visited;

    while (result.size() < maxSamples) {

        CS submask = getRandom(dontCareBits, rng, coin) | posBits;

        if (visited.insert(submask).second)
            result.emplace_back(submask);
    }

    return result;
}

std::vector<CS> rei::TopDownSearch::generateSolutionSet()
{
    std::vector<int> dontCareBits;
    dontCareBits.reserve(guideTable.ICsize);

    const CS combined = posBits | negBits;
    for (int i = 0; i < guideTable.ICsize; ++i)
    {
        const CS bitMask = CS::one() << i;
        if ((bitMask & combined) == CS())
        {
            dontCareBits.push_back(i);
        }
    }

    const size_t numDontCareBits = dontCareBits.size();
    const size_t numCombinations = 1ULL << numDontCareBits;

    std::vector<CS> combinations;
    combinations.reserve(numCombinations);

    for (size_t subset = 0; subset < numCombinations; ++subset)
    {
        CS combination = posBits;

        for (size_t bit = 0; bit < numDontCareBits; ++bit)
        {
            if (subset & (1ULL << bit))
            {
                combination |= (CS::one() << dontCareBits[bit]);
            }
        }

        combinations.push_back(combination);
    }

    return combinations;
}

EnumerationState rei::TopDownSearch::enumerateLevel(const std::span<CS>& CSs, int startPIdx, int& idx, bool overrideParent, int opIdx) {

    int solutionIndex;

    // Question
    int pIdx = startPIdx - 1;
    for (const auto& parent : CSs)
    {
        pIdx++;
        if (parent == CS()) continue;

        if (parent & CS::one())
        {
            if (context.lastIdx + 2 >= cache_capacity + LC_START) return EnumerationState::End;

            if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, parent & (~CS::one()), solutionIndex))
            {
                LOG_OP(level, to_string(Operation::Question), context.allCS, context.counter);
                partitioner.end(level, Operation::Question) = INT_MAX;
                idx = context.GetLastOutmostParent(solutionIndex);
                return EnumerationState::Found;
            }
        }
    }
    partitioner.end(level, Operation::Question) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Question), context.allCS, context.counter);

    // Star
    pIdx = startPIdx - 1;
    for (const auto& parent : CSs)
    {
        pIdx++;
        if (parent == CS()) continue;

        if (parent & CS::one())
        {
            std::vector<CS> childs;

            if(heuristicConfigs.invertStarUseRandomSampling)
                childs = rei::revertStarRandom(parent, heuristicConfigs.invertStarMaxSamples, guideTable);
            else
                childs = rei::revertStar(parent, guideTable);

            for (size_t i = 0; i < childs.size(); i++)
            {
                if (context.lastIdx + 2 >= cache_capacity + LC_START) return EnumerationState::End;

                if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, childs[i], solutionIndex))
                {
                    LOG_OP(level, to_string(Operation::Star), context.allCS, context.counter);
                    partitioner.end(level, Operation::Star) = INT_MAX;
                    idx = context.GetLastOutmostParent(solutionIndex);
                    return EnumerationState::Found;
                }
            }
        }
    }
    partitioner.end(level, Operation::Star) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Star), context.allCS, context.counter);

    // Concatenate
    pIdx = startPIdx - 1;
    for (const auto& parent : CSs)
    {
        pIdx++;
        if (parent == CS()) continue;

        std::vector<Pair<CS>> pairs;

        if(heuristicConfigs.invertConcatUseRandomSampling)
            pairs = revertConcatRandom(parent, heuristicConfigs.invertConcatMaxSamples, guideTable);
        else
            pairs = revertConcat(parent, guideTable);

        for (size_t i = 0; i < pairs.size(); i++)
        {
            auto pair = pairs[i];

            if (context.lastIdx + 2 >= cache_capacity + LC_START) return EnumerationState::End;

            if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, pair.left, pair.right, solutionIndex))
            {
                LOG_OP(level, to_string(Operation::Concatenate), context.allCS, context.counter);
                partitioner.end(level, Operation::Concatenate) = INT_MAX;
                idx = context.GetLastOutmostParent(solutionIndex);
                return EnumerationState::Found;
            }
        }
    }
    partitioner.end(level, Operation::Concatenate) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Concatenate), context.allCS, context.counter);

    // Or
    pIdx = startPIdx - 1;
    for (const auto& parent : CSs)
    {
        pIdx++;
        if (parent == CS()) continue;

        std::vector<Pair<CS>> pairs;

        if(heuristicConfigs.invertOrUseRandomSampling)
            pairs = revertOrRandom(parent, heuristicConfigs.invertOrMaxSamples, guideTable.ICsize);
        else
            pairs = revertOr(parent);

        for (size_t i = 0; i < pairs.size(); i++)
        {
            auto pair = pairs[i];

            if (context.lastIdx + 2 >= cache_capacity + LC_START) return EnumerationState::End;

            if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, pair.left, pair.right, solutionIndex))
            {
                LOG_OP(level, to_string(Operation::Or), context.allCS, context.counter);
                partitioner.end(level, Operation::Or) = INT_MAX;
                idx = context.GetLastOutmostParent(solutionIndex);
                return EnumerationState::Found;
            }
        }
    }
    partitioner.end(level, Operation::Or) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Or), context.allCS, context.counter);

    return EnumerationState::NotFound;
}

std::string rei::TopDownSearch::bracket(std::string s) {
    int p = 0;
    for (int i = 0; i < s.length(); i++) {
        if (s[i] == '(') p++;
        else if (s[i] == ')') p--;
        else if (s[i] == '+' && p <= 0) return "(" + s + ")";
    }
    return s;
}

std::string rei::TopDownSearch::constructDownward(int index)
{
    std::string left;
    if (context.nextVisited[index] == NEXTVISITED_GIVEN)
        left = resolver->resolve(context.solved.at(index).cs);
    else
    {
        auto originalIdx = getOriginal(context.nextVisited, context.nextVisited[index]);
        left = constructDownward(context.solved[originalIdx].leftIdx);
    }

    int level;
    Operation op;
    partitioner.indexToLevel(index, level, op);

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
    if (context.nextVisited[++index] == NEXTVISITED_GIVEN)
        right = resolver->resolve(context.solved.at(index).cs);
    else
    {
        auto originalIdx = getOriginal(context.nextVisited, context.nextVisited[index]);
        right = constructDownward(context.solved[originalIdx].leftIdx);
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