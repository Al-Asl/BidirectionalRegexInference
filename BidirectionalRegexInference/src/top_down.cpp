#include <top_down.hpp>

#define LOG_OP(levelnum, op_string, allCS, counter) \
        printf("Level %-2d | (%s) | AllCS: %-11llu | S %-5llu | NV %-11llu | V %-11llu | C %-11llu | SS %-5llu | G %-5llu \n", \
            levelnum, op_string.c_str() ,allCS,  counter.solved, counter.notVisited, counter.visited, counter.cyclic, counter.selfSolved, counter.given);

using namespace rei;

rei::TopDownSearch::Context::Context(int cache_capacity)
{
    status = new int[cache_capacity + 2];
    parentIdx = new int[cache_capacity + 2];
    cache = new CS[cache_capacity + 2];

    lastIdx = 0;
    allCS = 0;
    counter = {};
}

rei::TopDownSearch::Context::~Context()
{
    delete[] cache;
    delete[] parentIdx;
    delete[] status;
}

void rei::TopDownSearch::Context::AddSolutionSet(const std::vector<CS>& solutionSet) {
    for (size_t i = 0; i < solutionSet.size(); i++)
        visited[solutionSet[i]] = -1;
}

bool rei::TopDownSearch::Context::AddSolvedNode(const CS& cs, int& idx) {
    if (visited.find(cs) == visited.end())
    {
        visited[cs] = -1;
        solved.insert(cs);
        return false;
    }
    else
    {
        // replace in the graph and do the check!
        return false;
    }
}

bool rei::TopDownSearch::Context::InsertAndCheck(int parentIdx, CS left, CS right)
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
        if (parentIdx == -1) return true;
        return recursiveCheck(parentIdx, lastIdx - 2);
    }

    if ((static_cast<int>(lt) > 1) || (static_cast<int>(rt) > 1))
    {
        // need to handle this case somehow
        // need to be stored for later checks!
    }

    return false;
}

bool rei::TopDownSearch::Context::InsertAndCheck(int parentIdx, CS child)
{
    return InsertAndCheck(parentIdx, child, CS::one());
}

bool rei::TopDownSearch::Context::CheckAllVisited(int& solvedIndex) {
    for (size_t idx = 2; idx < lastIdx; idx += 2)
    {
        if (!isSolved(idx) || !isSolved(idx + 1)) continue;

        if (parentIdx[idx] == -1 ? true : recursiveCheck(parentIdx[idx], idx))
        {
            solvedIndex = getOutmostParent(idx);
            return true;
        }
    }
    return false;
}

int rei::TopDownSearch::Context::GetLastOutmostParent() {
    int pIdx = getOutmostParent(lastIdx - 1);
    return pIdx % 2 == 0 ? pIdx : pIdx - 1;
}

rei::TopDownSearch::Context::NodeType rei::TopDownSearch::Context::getNodeType(const CS& cs)
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

void rei::TopDownSearch::Context::insert(NodeType nodeType, CS cs, int pIdx)
{
    switch (nodeType) {
    case NodeType::NotVistied:
        cache[lastIdx] = cs;
        visited[cs] = lastIdx;
        status[lastIdx] = 0;
        break;
    case NodeType::Vistied:
        status[lastIdx] = -visited.at(cs);
        break;
    case NodeType::SelfSolved:
        status[lastIdx] = -visited.at(cs);
        idxToSolved[lastIdx] = cs;
        break;
    case NodeType::Given:
        status[lastIdx] = -1;
        idxToSolved[lastIdx] = cs;
        break;
    }

    parentIdx[lastIdx++] = pIdx;
}

bool rei::TopDownSearch::Context::isSolved(int idx) {
    auto s = status[idx];
    if (s == -1 || s > 1) return true;
    if (s < -1) return idxToSolved.find(-s) != idxToSolved.end();
    return false;
}

bool rei::TopDownSearch::Context::recursiveCheck(int index, int lcIdx)
{
    if (isSolved(index)) return false;

    // we can reconstruct the cs recursively, we don't need cache
    solved.insert(cache[index]);
    status[index] = lcIdx;
    idxToSolved[index] = cache[index];

    counter.solved++;

    int pIdx = parentIdx[index];
    int sIdx = index % 2 == 0 ? index + 1 : index - 1;

    if (!isSolved(sIdx)) return false;

    if (pIdx < 0) // the solved set parents is -1
        return true;

    return recursiveCheck(pIdx, index < sIdx ? index : sIdx);
}

int rei::TopDownSearch::Context::getOutmostParent(int index) {
    if (parentIdx[index] == -1)
        return index;
    else
        return getOutmostParent(parentIdx[index]);
}


rei::TopDownSearch::TopDownSearch(const rei::GuideTable& guideTable, const rei::StarLookup& starLookup,
    std::shared_ptr<rei::CSResolverInterface> resolver, int maxLevel, const CS& posBits, const CS& negBits, int cache_capacity) :
    guideTable(guideTable), starLookup(starLookup), resolver(resolver), partitioner(maxLevel), context(cache_capacity),
    maxLevel(maxLevel), posBits(posBits), negBits(negBits), cache_capacity(cache_capacity) {

    // the index 0 and 1 are reserved for checking
    partitioner.start(0, Operation::Question) = 2;
    context.lastIdx = 2;
}

bool rei::TopDownSearch::Push(const CS& cs, TopDownSearchResult& res) {
    int idx;
    if (context.AddSolvedNode(cs, idx))
    {
        // reconstruct the RE
        return true;
    }
    else
        return false;
}

EnumerationState rei::TopDownSearch::EnumerateLevel(TopDownSearchResult& res)
{
    if (level == maxLevel) return EnumerationState::End;

    EnumerationState enumState;
    int solvedIdx;

    if (level == 0)
    {
        auto solutionSet = generateSolutionSet();
        context.AddSolutionSet(solutionSet);
        enumState = enumerateLevel(solutionSet, 2, solvedIdx, true, -1);
    }
    else
    {
        auto [start, end] = partitioner.Interval(level - 1);

        if (end - start < 1) {
            if (context.CheckAllVisited(solvedIdx))
                enumState = EnumerationState::Found;
            else
                enumState = EnumerationState::End;
        }
        else
            enumState = enumerateLevel(std::span(context.cache + start, end - start), start, solvedIdx);
    }

    if (enumState == EnumerationState::Found)
    {
        res.RE = constructDownward(solvedIdx);
    }

    level++;
    return enumState;
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

EnumerationState rei::TopDownSearch::enumerateLevel(const std::span<CS>& CSs, int startPIdx, int& idx, bool overrideParent, int opIdx) {

    // Question
    int pIdx = startPIdx - 1;
    for (const auto& parent : CSs)
    {
        pIdx++;
        if (parent == CS()) continue;

        if (parent & CS::one())
        {
            if (context.lastIdx > cache_capacity) return EnumerationState::End;

            if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, parent & (~CS::one())))
            {
                LOG_OP(level, to_string(Operation::Question), context.allCS, context.counter);
                partitioner.end(level, Operation::Question) = INT_MAX;
                idx = context.GetLastOutmostParent();
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
            auto childs = rei::revertStar(parent, starLookup);

            for (size_t i = 0; i < childs.size(); i++)
            {
                if (context.lastIdx > cache_capacity) return EnumerationState::End;

                if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, childs[i]))
                {
                    LOG_OP(level, to_string(Operation::Star), context.allCS, context.counter);
                    partitioner.end(level, Operation::Star) = INT_MAX;
                    idx = context.GetLastOutmostParent();
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

        auto pairs = revertConcat(parent, guideTable);

        for (size_t i = 0; i < pairs.size(); i++)
        {
            auto pair = pairs[i];

            if (context.lastIdx > cache_capacity) return EnumerationState::End;

            if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, pair.left, pair.right))
            {
                LOG_OP(level, to_string(Operation::Concatenate), context.allCS, context.counter);
                partitioner.end(level, Operation::Concatenate) = INT_MAX;
                idx = context.GetLastOutmostParent();
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

        auto pairs = revertOr(parent);

        for (size_t i = 0; i < pairs.size(); i++)
        {
            auto pair = pairs[i];

            if (context.lastIdx > cache_capacity) return EnumerationState::End;

            if (context.InsertAndCheck(overrideParent ? opIdx : pIdx, pair.left, pair.right))
            {
                LOG_OP(level, to_string(Operation::Or), context.allCS, context.counter);
                partitioner.end(level, Operation::Or) = INT_MAX;
                idx = context.GetLastOutmostParent();
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
    if (context.status[index] == -1)
        left = resolver->resolve(context.idxToSolved.at(index));
    else
    {
        auto ls = context.status[index];
        left = constructDownward(ls < -1 ? context.status[-ls] : ls);
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
    if (context.status[++index] == -1)
        right = resolver->resolve(context.idxToSolved.at(index));
    else
    {
        auto rs = context.status[index];
        right = constructDownward(rs < -1 ? context.status[-rs] : rs);
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