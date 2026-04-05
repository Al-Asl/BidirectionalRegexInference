#include <top_down.hpp>

#include <cs_utils.h>

#define LOG_OP(levelIdx, op_string, allCS, counter) \
        printf("Level %-2d | (%s) | AllCS: %-11llu | S %-5llu | V %-11llu | NV %-11llu | RC %-5llu | RRUC %-5llu | G %-5llu \n", \
            levelIdx + 1, op_string.c_str() ,allCS, counter.solved, counter.insertNotVisited, counter.insertVisited, counter.rejectedC, counter.rejectedRUC, counter.insertGiven);

#define PARENTIDX_SOLUTIONSET -1
#define VISITED_SOLUTIONSET -1
#define NEXTVISITED_GIVEN_SENTINEL INT_MAX

void rei::sampleSolutionSetRandom(const GuideTable& guideTable, const CS posBits, const CS negBits, CSBuffer& buffer, uint64_t seed)
{
    {
        CS combined = buffer.append().copy(posBits);
        combined |= negBits;
        std::vector<int> dontCareBits;
        for (int i = 0; i < guideTable.ICsize; ++i)
        {
            if (!combined.getBit(i))
                dontCareBits.push_back(i);
        }
        buffer.removeLast();

        const size_t numDontCareBits = dontCareBits.size();

        if ((1ULL << numDontCareBits) <= buffer.size())
        {
            sampleSolutionSet(guideTable, posBits, negBits, buffer);
            return;
        }

        std::mt19937 rng(seed);
        std::bernoulli_distribution coin(0.5);
        std::unordered_set<uint64_t> visited;

        while (!buffer.isFull()) {

            CS submask = buffer.append().copy(posBits);
            getRandom(dontCareBits, submask, rng, coin);

            if (!visited.insert(submask.getHash()).second)
                buffer.removeLast();
        }
    }
}

void rei::sampleSolutionSet(const GuideTable& guideTable, const CS posBits, const CS negBits, CSBuffer& buffer)
{
    CS combined = buffer.append().copy(posBits);
    combined |= negBits;
    std::vector<int> dontCareBits;
    for (int i = 0; i < guideTable.ICsize; ++i)
    {
        if (!combined.getBit(i))
            dontCareBits.push_back(i);
    }
    buffer.removeLast();

    const size_t numDontCareBits = dontCareBits.size() > 63 ? 63 : dontCareBits.size();
    const size_t numCombinations = 1ULL << numDontCareBits;

    for (size_t subset = 0; subset < numCombinations; ++subset)
    {
        if (buffer.isFull()) return;

        CS cs = buffer.append().copy(posBits);

        for (size_t bit = 0; bit < numDontCareBits; ++bit)
        {
            if (subset & (1ULL << bit))
                cs.setBitOn(dontCareBits[bit]);
        }
    }
}

bool is_original(const int* nextVisited, int idx) {
    if (idx == 0) return true;
    return nextVisited[idx] < 0;
}

void append_original(int* nextVisited, int idx) {
    nextVisited[idx] = -idx;
}

std::vector<int>  collect_all_visited(int* nextVisited, int oidx) {
    std::vector<int> allVisited{ oidx };
    auto i = -nextVisited[oidx];
    while (i != oidx)
    {
        allVisited.push_back(i);
        i = nextVisited[i];
    }
    return allVisited;
}

void append_duplicate(int* nextVisited, int originalIdx, int idx) {

    int lastIdx = -nextVisited[originalIdx];
    if (lastIdx != originalIdx)
        while (nextVisited[lastIdx] != originalIdx)
            lastIdx = nextVisited[lastIdx];

    nextVisited[lastIdx] = lastIdx == originalIdx ? -idx : idx;
    nextVisited[idx] = originalIdx;
}

int find_original(const int* nextVisited, int idx) {
    if (idx == 0) return 0;
    while (nextVisited[idx] >= 0)
        idx = nextVisited[idx];
    return idx;
}

rei::TopDownSearch::Context::Context(int cache_capacity, int chuncksPerBitmask, const Costs& costs) :
    resolver(resolver), uniqueNodes(cache_capacity, chuncksPerBitmask), costs(costs), lastIdx(0)
{
    int treeNodeCapacity = cache_capacity * 10;

    nextVisited = new int[treeNodeCapacity];
    parentIdx = new int[treeNodeCapacity];
    uIdx = new int[treeNodeCapacity];

    {
        uniqueNodes.append();
        uniqueNodes.setSentinel(0);
    }

    allCS = 0;
    counter = {};
}

rei::TopDownSearch::Context::~Context()
{
    delete[] uIdx;
    delete[] parentIdx;
    delete[] nextVisited;
}

bool rei::TopDownSearch::Context::includeExternalCSs(const std::vector<int>& ids, int cost, SearchResult& searchRes)
{
    for (int id : ids) {

        auto cs = resolver->getCS(id);
        auto csHash = cs.getHash();

        if (visited.find(csHash) == visited.end())
        {
            if (uniqueNodes.isFull())
                return false;

            auto uidx = uniqueNodes.append(cs);
            uniqueNodes.setGiven(uidx, id, cost);
            visited[csHash] = uidx;
        }
        else
        {
            // solved nodes are also discarded because they are not minimum

            auto uidx = visited.at(csHash);

            if (uniqueNodes.getCS(uidx) == cs)
            {
                int oidx = uniqueNodes.getOriginalIdx(uidx);
                uniqueNodes.setGiven(uidx, id, cost);

                // collect all occurrences
                auto toCheck = collect_all_visited(nextVisited, oidx);

                // mark all their occurrences as solved in the tree
                for (int i = 0; i < toCheck.size(); i++)
                {
                    leftIdx[toCheck[i]] = -1;
                    nextVisited[toCheck[i]] = NEXTVISITED_GIVEN_SENTINEL;
                }

                //TODO: prune at all occurrences

                for (auto i : toCheck)
                    recursiveCheck(i, searchRes);
            }
            else
            {
                // For now we keep our version
            }
        }
    }

    return true;
}

rei::TopDownSearch::Context::InsertAction rei::TopDownSearch::Context::getInsertAction(const CS& cs) const {

    // from unary operation
    if (!cs.isValid()) 
        return InsertAction::InsertSentinel;

    auto vit = visited.find(cs.getHash());
    if (vit == visited.end())
        return InsertAction::InsertNotVistied;

    auto uidx = (*vit).second;

    // we only test the solution set
    if (uidx == VISITED_SOLUTIONSET)
        return InsertAction::RejectCyclic;

    auto nodeType = uniqueNodes.getNodeType(uidx);

    CS storedCS = nodeType == NodeType::Given ? 
        resolver->getCS(uniqueNodes.getExternalIdx(uidx)) : uniqueNodes.getCS(uidx);

    if (storedCS != cs)
        return InsertAction::RejectRUC;

    switch (nodeType) {
    case NodeType::Given:
        return InsertAction::InsertGiven;
    case NodeType::Solved:
        return InsertAction::InsertVistiedSolved;
    default:
        return InsertAction::InsertVistied;
    }
}

bool rei::TopDownSearch::Context::insert(InsertAction action, CS cs, int pIdx)
{
    parentIdx[lastIdx] = pIdx;

    int uidx = 0;

    if (action == InsertAction::InsertNotVistied)
    {
        if (uniqueNodes.isFull())
            return false;

        uidx = uniqueNodes.append(cs);
        visited[cs.getHash()] = uidx;
        uniqueNodes.setUnSolved(uidx, lastIdx);
        uIdx[lastIdx] = uidx;
        append_original(nextVisited, lastIdx++);
        return true;
    }

    uidx = cs.isValid() ? visited.at(cs.getHash()) : 0 /*Sentinel*/;
    uIdx[lastIdx] = uidx;

    switch (action) {
    case InsertAction::InsertVistied:
        append_duplicate(nextVisited, uniqueNodes.getOriginalIdx(uidx), lastIdx);
        break;
    case InsertAction::InsertVistiedSolved:
        append_duplicate(nextVisited, uniqueNodes.getOriginalIdx(uidx), lastIdx);
        leftIdx[lastIdx] = -1;
        break;
    case InsertAction::InsertGiven:
        nextVisited[lastIdx] = NEXTVISITED_GIVEN_SENTINEL;
        leftIdx[lastIdx] = -1;
        break;
    case InsertAction::InsertSentinel:
        nextVisited[lastIdx] = NEXTVISITED_GIVEN_SENTINEL;
        leftIdx[lastIdx] = -1;
        break;
    }

    lastIdx++;

    return true;
}

void rei::TopDownSearch::Context::recursiveCheck(int idx, SearchResult& searchRes)
{
    auto sidx   = idx % 2 == 0 ? idx + 1 : idx - 1;
    auto suidx  = uIdx[sidx];

    if (static_cast<int>(uniqueNodes.getNodeType(suidx)) == 0)
        return;

    auto uidx = uIdx[idx];
    int pCost = 0;
    {
        auto [level, op] = partitioner->indexToLevel(idx);
        pCost = uniqueNodes.getCost(uidx) + uniqueNodes.getCost(suidx) + costs.operationCost(op);
    }

    auto pIdx = parentIdx[idx];

    if (pIdx == PARENTIDX_SOLUTIONSET)
    {
        if (pCost < searchRes.cost)
        {
            searchRes.solutionIdx = idx < sidx ? idx : sidx;
            searchRes.cost = pCost;
        }
        return;
    }

    auto puidx = uIdx[pIdx];

    if (uniqueNodes.getCost(puidx) < pCost)
        return;

    uniqueNodes.seSolved(puidx, pIdx, pCost);
    leftIdx[pIdx] = idx < sidx ? idx : sidx;

    // check duplicates
    int dIdx = -nextVisited[pIdx];
    while (dIdx != pIdx) {
        recursiveCheck(dIdx, searchRes);
        dIdx = nextVisited[dIdx];
    }

    //counter.solved++;

    recursiveCheck(pIdx, searchRes);
}

bool rei::TopDownSearch::Context::insertAndCheck(int parentIdx, CS child, SearchResult& searchRes)
{
    return insertAndCheck(parentIdx, child, CS::createInvalid(), searchRes);
}

bool rei::TopDownSearch::Context::insertAndCheck(int parentIdx, CS left, CS right, SearchResult& searchRes) {

    allCS += 2;

    auto la = getInsertAction(left);

    if (static_cast<int>(la) < 2)
        return true;

    auto ra = getInsertAction(right);

    if (static_cast<int>(ra) < 2)
        return true;

    counter.update(la);
    counter.update(ra);

    if (!insert(la, left, parentIdx))
        return false;

    if (!insert(ra, right, parentIdx))
        return false;

    if ((static_cast<int>(la) > 3) && (static_cast<int>(ra) > 3))
        recursiveCheck(lastIdx - 2, searchRes);

    return true;
}

const CS rei::TopDownSearch::Context::getCS(int idx) const {
    return uniqueNodes.getCS(uIdx[idx]);
}

int rei::TopDownSearch::Context::getGivenCSId(int idx) const
{
    return uniqueNodes.getExternalIdx(uIdx[idx]);
}

bool rei::TopDownSearch::Context::canTraverse(int idx) const
{
    if (nextVisited[idx] == NEXTVISITED_GIVEN_SENTINEL)
        return false;

    // we also assume that this node is unsolved
    return is_original(nextVisited, idx);
}

rei::TopDownSearch::TopDownSearch(const GuideTable& guideTable, int maxLevel, int cacheCapacity, std::unique_ptr<TopDownSamplerBase> solutionSetSampler,
    const Costs& costs, std::unique_ptr<CSResolverInterface> resolver, SamplingLimits samplingLimits) :
    guideTable(guideTable), partitioner(maxLevel), context(cacheCapacity, CS::getChuncksSize(guideTable.ICsize), costs), level(0),
    maxLevel(maxLevel), resolver(std::move(resolver)), solutionSetSampler(std::move(solutionSetSampler)) , samplingLimits(samplingLimits){

    context.partitioner = &partitioner;
    context.resolver = this->resolver.get();

    auto elementsCount = std::max(samplingLimits.invertConcatMaxSamples * 2, samplingLimits.invertOrMaxSamples * 2);
    elementsCount = std::max(elementsCount, samplingLimits.invertStarMaxSamples);

    auto n = CS::getChuncksSize(guideTable.ICsize);
    stageBuffer = CSBuffer(new uint64_t[elementsCount * n], elementsCount, n);

    starSampler         = std::make_unique<StarSampler>(guideTable);
    concatSampler       = std::make_unique<ConcatSampler>(guideTable);
    orSampler           = std::make_unique<OrSampler>(guideTable);
}

rei::TopDownSearch::~TopDownSearch() 
{
    delete[] stageBuffer.data();
}

rei::EnumerationState rei::TopDownSearch::includeExternalCSs(const std::vector<int>& ids, int cost, TopDownSearchResult& res)
{
    SearchResult searchRes;

    if (context.includeExternalCSs(ids, cost, searchRes))
    {
        if (searchRes.solutionIdx > -1) {
            res.RE = constructDownward(searchRes.solutionIdx);
            res.allCS = context.lastIdx;
            res.storedCS = context.uniqueNodes.count();
            return EnumerationState::Found;
        }

        return EnumerationState::NotFound;
    }
    else
        return EnumerationState::End;
}

uint64_t rei::TopDownSearch::estimateNextLevel() const
{
    uint64_t multi = 1 + samplingLimits.invertStarMaxSamples + 2 * samplingLimits.invertConcatMaxSamples + 2 * samplingLimits.invertOrMaxSamples;

    if (level == 0)
        return multi * samplingLimits.solutionSetMaxSamples;
    else
    {
        auto [start, end] = partitioner.Interval(level - 1);
        return multi * (end - start);
    }
}

void rei::TopDownSearch::setSampler(Operation op, std::unique_ptr<TopDownSamplerBase> sampler) {
    switch (op) {
    case Operation::Star:
        starSampler = std::move(sampler);
        break;
    case Operation::Concatenate:
        concatSampler = std::move(sampler);
        break;
    case Operation::Or:
        orSampler = std::move(sampler);
        break;
    }
}

rei::EnumerationState rei::TopDownSearch::enumerateLevel(TopDownSearchResult& res) {

    if (level == maxLevel) return EnumerationState::End;

    EnumerationState enumState;
    SearchResult searchRes;

    if (level == 0)
    {
        auto n = CS::getChuncksSize(guideTable.ICsize);
        auto solutionSet = CSBuffer(new uint64_t[samplingLimits.solutionSetMaxSamples * n], samplingLimits.solutionSetMaxSamples, n);

        solutionSetSampler->sample(solutionSet, CS::createInvalid());

        for (int i = 0; i < solutionSet.count(); i++)
            context.visited[solutionSet[i].getHash()] = VISITED_SOLUTIONSET;

        enumState = enumerateLevel(solutionSet, PARENTIDX_SOLUTIONSET, searchRes);

        delete[] solutionSet.data();
    }
    else
    {
        auto [start, end] = partitioner.Interval(level - 1);

        if (end - start > 0)
            enumState = enumerateLevel(start, end, searchRes);
        else
            enumState = EnumerationState::End;
    }

    if (enumState != EnumerationState::NotFound)
    {
        if (enumState == EnumerationState::Found)
            res.RE = constructDownward(searchRes.solutionIdx);
        res.allCS = context.lastIdx;
        res.storedCS = context.uniqueNodes.count();
    }

    level++;
    return enumState;
}

rei::EnumerationState rei::TopDownSearch::enumerateLevel(CSBuffer CSs, int pIdx, SearchResult& searchRes) {

    // Question
    for (int i = 0; i < CSs.count(); i++)
    {
        auto cs = CSs[i];

        if (cs.getBit(0))
        {
            auto rcs = stageBuffer[0].copy(cs).setBitOff(0);
            if (!context.insertAndCheck(pIdx, rcs, searchRes))
                return EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Question) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Question), context.allCS, context.counter);

    // Star
    for (int i = 0; i < CSs.count(); i++)
    {
        auto cs = CSs[i];

        if (cs.getBit(0))
        {
            auto buffer = stageBuffer.getView(0, samplingLimits.invertStarMaxSamples);

            starSampler->sample(buffer, cs);

            for (int j = 0; j < buffer.count(); j++)
            {
                if (!context.insertAndCheck(pIdx, buffer[j], searchRes))
                    return EnumerationState::End;
            }
        }
    }
    partitioner.end(level, Operation::Star) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Star), context.allCS, context.counter);

    // Concatenate
    for (int i = 0; i < CSs.count(); i++)
    {
        auto buffer = stageBuffer.getView(0, samplingLimits.invertConcatMaxSamples * 2);
        concatSampler->sample(buffer, CSs[i]);

        for (int j = 0; j < buffer.count() / 2; j++)
        {
            if (!context.insertAndCheck(pIdx, buffer[j * 2], buffer[j * 2 + 1], searchRes))
                return EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Concatenate) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Concatenate), context.allCS, context.counter);

    // Or
    for (int i = 0; i < CSs.count(); i++)
    {
        auto buffer = stageBuffer.getView(0, samplingLimits.invertOrMaxSamples * 2);

        orSampler->sample(buffer, CSs[i]);

        for (int j = 0; j < buffer.count() / 2; j++)
        {
            if (!context.insertAndCheck(pIdx, buffer[j * 2], buffer[j * 2 + 1], searchRes))
                return EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Or) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Or), context.allCS, context.counter);

    if (searchRes.solutionIdx > -1)
        return EnumerationState::Found;
    else
        return EnumerationState::NotFound;
}

rei::EnumerationState rei::TopDownSearch::enumerateLevel(int start, int end, SearchResult& searchRes) {

    // Question
    for (int idx = start; idx < end; idx++)
    {
        if (!context.canTraverse(idx)) continue;

        auto cs = context.getCS(idx);

        if (cs.getBit(0))
        {
            auto rcs = stageBuffer[0].copy(cs).setBitOff(0);
            if (!context.insertAndCheck(idx, rcs, searchRes))
                return EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Question) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Question), context.allCS, context.counter);

    // Star
    for (int idx = start; idx < end; idx++)
    {
        if (!context.canTraverse(idx)) continue;

        auto cs = context.getCS(idx);

        if (cs.getBit(0))
        {
            auto buffer = stageBuffer.getView(0, samplingLimits.invertStarMaxSamples);

            starSampler->sample(buffer, cs);

            for (int i = 0; i < buffer.count(); i++)
            {
                if (!context.insertAndCheck(idx, buffer[i], searchRes))
                    return EnumerationState::End;
            }
        }
    }
    partitioner.end(level, Operation::Star) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Star), context.allCS, context.counter);

    // Concatenate
    for (int idx = start; idx < end; idx++)
    {
        if (!context.canTraverse(idx)) continue;

        auto cs = context.getCS(idx);

        auto buffer = stageBuffer.getView(0, samplingLimits.invertConcatMaxSamples * 2);

        concatSampler->sample(buffer, cs);

        for (int i = 0; i < buffer.count() / 2; i++)
        {
            if (!context.insertAndCheck(idx, buffer[i * 2], buffer[i * 2 + 1], searchRes))
                return EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Concatenate) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Concatenate), context.allCS, context.counter);

    // Or
    for (int idx = start; idx < end; idx++)
    {
        if (!context.canTraverse(idx)) continue;

        auto cs = context.getCS(idx);

        auto buffer = stageBuffer.getView(0, samplingLimits.invertOrMaxSamples * 2);

        orSampler->sample(buffer, cs);

        for (int i = 0; i < buffer.count() / 2; i++)
        {
            if (!context.insertAndCheck(idx, buffer[i * 2], buffer[i * 2 + 1], searchRes))
                return EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Or) = context.lastIdx;
    LOG_OP(level, to_string(Operation::Or), context.allCS, context.counter);

    if (searchRes.solutionIdx > -1)
        return EnumerationState::Found;
    else
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
    if (context.nextVisited[index] == NEXTVISITED_GIVEN_SENTINEL)
        left = resolver->constructRE(context.getGivenCSId(index));
    else
    {
        auto originalIdx = find_original(context.nextVisited, index);
        left = constructDownward(context.leftIdx.at(originalIdx));
    }

    auto [level, op] = partitioner.indexToLevel(index);

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
    if (context.nextVisited[++index] == NEXTVISITED_GIVEN_SENTINEL)
        right = resolver->constructRE(context.getGivenCSId(index));
    else
    {
        auto originalIdx = find_original(context.nextVisited, index);
        right = constructDownward(context.leftIdx.at(originalIdx));
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