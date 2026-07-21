#include <top_down.hpp>

#include <cs_utils.h>

#define LOG_OP(levelIdx, op_string, allCS, counter) \
        printf("Level %-2d | (%s) | AllCS: %-11llu | S %-5llu | NV %-11llu | V %-11llu | RC %-5llu | RRUC %-5llu | G %-5llu \n", \
            levelIdx + 1, op_string.c_str() ,allCS, counter.solved, counter.insertNotVisited, counter.insertVisited, counter.rejectedC, counter.rejectedRUC, counter.insertGiven);

#define PARENTIDX_SOLUTIONSET -1
#define VISITED_SOLUTIONSET -1
#define NEXTVISITED_GIVEN_SENTINEL INT_MAX

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

class rei::TopDownSearch::Context {

public:

    enum class InsertAction
    {
        RejectRUC = 0,
        RejectCyclic = 1,
        InsertNotVistied = 2,
        InsertVistied = 3,
        InsertVistiedSolved = 4,
        InsertGiven = 5,
        InsertSentinel = 6
    };

    enum class NodeType {
        UnSolved = 0,
        Solved = 1,
        Given = 2,
        Sentinel = 3
    };

    struct Counter {
        uint64_t rejectedRUC = 0;
        uint64_t rejectedC = 0;
        uint64_t insertVisited = 0;
        uint64_t insertNotVisited = 0;
        uint64_t insertGiven = 0;
        uint64_t solved = 0;

        void update(InsertAction action) {
            switch (action) {
            case InsertAction::RejectRUC:
                rejectedRUC++;
                return;
            case InsertAction::RejectCyclic:
                rejectedC++;
                return;
            case InsertAction::InsertVistied:
                insertVisited++;
                return;
            case InsertAction::InsertNotVistied:
                insertNotVisited++;
                return;
            case InsertAction::InsertGiven:
                insertGiven++;
                return;
            }
        }
    };

    struct UniqueNodes {

        UniqueNodes(int cacheCapacity, int chuncksPerBitmask) :
            cache(new uint64_t[cacheCapacity * chuncksPerBitmask], cacheCapacity, chuncksPerBitmask),
            param1(new int[cacheCapacity]), param2(new int[cacheCapacity]) {
        }

        ~UniqueNodes() {
            delete[] cache.data();
            delete[] param1;
            delete[] param2;
        }

        bool isFull() const { return cache.isFull(); }

        int append(const CS& cs) {
            cache.append().copy(cs);
            return cache.count() - 1;
        }

        int append() {
            cache.append().clear();
            return cache.count() - 1;
        }

        const CS getCS(int uidx) const { return cache[uidx]; }
        CS getCS(int uidx) { return cache[uidx]; }

        NodeType getNodeType(int uidx) const {
            if (param2[uidx] == INT_MIN)
                return NodeType::Sentinel;
            if (param1[uidx] < 0)
                return NodeType::Given;
            else if (param2[uidx] == INT_MAX)
                return NodeType::UnSolved;
            else
                return NodeType::Solved;
        }

        void setUnSolved(int uidx, int idx) { param1[uidx] = idx; param2[uidx] = INT_MAX; };
        void seSolved(int uidx, int idx, int cost) { param1[uidx] = idx; param2[uidx] = cost; };
        void setGiven(int uidx, int givenIdx, int cost) { param1[uidx] = -cost; param2[uidx] = givenIdx; };
        void setSentinel(int uidx) { param1[uidx] = 0; param2[uidx] = INT_MIN; };

        int getOriginalIdx(int uidx) const { return param1[uidx]; }
        int getExternalIdx(int uidx) const { return param2[uidx]; }

        int getCost(int uidx) const {
            if (param2[uidx] == INT_MIN)
                return 0;
            if (param1[uidx] < 0)
                return -param1[uidx];
            else
                return param2[uidx];
        }

        int count() const { return cache.count(); }
        int capacity() const { return cache.size(); }

    private:
        CSBuffer cache;
        int* param1;
        int* param2;
    };

    Context(int cache_capacity, const CostFunc& costFunc, int chunksPerCS, rei::CSResolverInterface* resolver, rei::LevelPartitioner* partitioner) :
         costFunc(costFunc), lastIdx(0), uniqueNodes(cache_capacity, chunksPerCS), resolver(resolver), partitioner(partitioner)
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

    ~Context() {
        delete[] uIdx;
        delete[] parentIdx;
        delete[] nextVisited;
    }

    bool includeExternalCSs(const std::vector<int>& ids, int cost, SearchResult& searchRes) {

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

    bool insertAndCheck(int parentIdx, CS child, SearchResult& searchRes) {
        return insertAndCheck(parentIdx, child, CS(), searchRes);
    }

    bool insertAndCheck(int parentIdx, CS left, CS right, SearchResult& searchRes) {

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

    const CS getCS(int idx) const {
        return uniqueNodes.getCS(uIdx[idx]);
    }

    bool canTraverse(int idx) const {
        if (nextVisited[idx] == NEXTVISITED_GIVEN_SENTINEL)
            return false;

        // we also assume that this node is unsolved
        return is_original(nextVisited, idx);
    }

    int getGivenCSId(int idx) const {
        return uniqueNodes.getExternalIdx(uIdx[idx]);
    }

    std::unordered_map<int, int> leftIdx;
    int* nextVisited;
    int* parentIdx;
    int* uIdx;
    UniqueNodes uniqueNodes;
    std::unordered_map<uint64_t, int> visited;

    int lastIdx;
    uint64_t allCS;
    Counter counter;

    rei::CSResolverInterface* resolver;
    rei::LevelPartitioner* partitioner;
    const rei::CostFunc& costFunc;

private:
    InsertAction getInsertAction(const CS& cs)const {

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

    bool insert(InsertAction action, CS cs, int pIdx) {

        if (lastIdx == uniqueNodes.capacity() * 10)
            return false;

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

    void recursiveCheck(int idx, SearchResult& searchRes) {
        auto sidx = idx % 2 == 0 ? idx + 1 : idx - 1;
        auto suidx = uIdx[sidx];

        if (uniqueNodes.getNodeType(suidx) == NodeType::UnSolved)
            return;

        auto uidx = uIdx[idx];
        int pCost = 0;
        {
            auto [level, op] = partitioner->indexToLevel(idx);
            pCost = uniqueNodes.getCost(uidx) + uniqueNodes.getCost(suidx) + costFunc.operationCost(op);
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
};

rei::TopDownSearch::TopDownSearch(const LanguageSystem& languageSystem, const InputParams& inputParams, int maxLevel, int cacheCapacity,
    std::unique_ptr<TopDownSamplerBase> solutionSetSampler, std::unique_ptr<CSResolverInterface> resolver, SamplingLimits samplingLimits) :
    languageSystem(languageSystem), partitioner(maxLevel), level(0), maxLevel(maxLevel), 
    resolver(std::move(resolver)), solutionSetSampler(std::move(solutionSetSampler)) , samplingLimits(samplingLimits){

    context = new Context(cacheCapacity, inputParams.costFunc, 
        CS::getChuncksSize(languageSystem.getIC().size()), this->resolver.get(), &partitioner);

    auto elementsCount = std::max(samplingLimits.invertConcatMaxSamples * 2, samplingLimits.invertOrMaxSamples * 2);
    elementsCount = std::max(elementsCount, samplingLimits.invertStarMaxSamples);

    auto n = CS::getChuncksSize(languageSystem.getIC().size());
    stageBuffer = CSBuffer(new uint64_t[elementsCount * n], elementsCount, n);

    starSampler         = std::make_unique<StarSampler>(languageSystem);
    concatSampler       = std::make_unique<ConcatSampler>(languageSystem);
    orSampler           = std::make_unique<OrSampler>(languageSystem);
}

rei::TopDownSearch::~TopDownSearch() 
{
    delete[] stageBuffer.data();
    delete context;
}

rei::EnumerationState rei::TopDownSearch::insertExternalCSs(const std::vector<int>& ids, int cost, Result& res)
{
    SearchResult searchRes;

    if (context->includeExternalCSs(ids, cost, searchRes))
    {
        if (searchRes.solutionIdx > -1) {
            Solution sol;
            sol.RE = constructDownward(searchRes.solutionIdx);
            sol.allCSs = context->allCS;
            sol.uniqueCSs = context->uniqueNodes.count();
            res.push_back(sol);
            return EnumerationState::Found;
        }

        return EnumerationState::NotFound;
    }
    else
    {
        res.message = "the search run's out of memory!";
        return EnumerationState::End;
    }
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

rei::EnumerationState rei::TopDownSearch::enumerateLevel(Result& res) {

    if (level == maxLevel)
    {
        res.message = "Max Level has been reached!";
        return EnumerationState::End;
    }

    EnumerationState enumState;
    SearchResult searchRes;

    if (level == 0)
    {
        auto n = CS::getChuncksSize(languageSystem.getIC().size());
        auto solutionSet = CSBuffer(new uint64_t[samplingLimits.solutionSetMaxSamples * n], samplingLimits.solutionSetMaxSamples, n);

        solutionSetSampler->sample(solutionSet, CS());

        for (int i = 0; i < solutionSet.count(); i++)
            context->visited[solutionSet[i].getHash()] = VISITED_SOLUTIONSET;

        enumState = enumerateLevel(solutionSet, PARENTIDX_SOLUTIONSET, searchRes);

        delete[] solutionSet.data();
    }
    else
    {
        auto [start, end] = partitioner.Interval(level - 1);

        if (end - start > 0)
            enumState = enumerateLevel(start, end, searchRes);
        else
        {
            res.message = "No more REs to search (no REs produced in the pre-level)!";
            enumState = EnumerationState::End;
        }
    }

    if (enumState != EnumerationState::NotFound)
    {
        if (enumState == EnumerationState::Found)
        {
            Solution sol;
            sol.RE = constructDownward(searchRes.solutionIdx);
            sol.allCSs = context->allCS;
            sol.uniqueCSs = context->uniqueNodes.count();
            res.push_back(sol);
        }
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
            if (!context->insertAndCheck(pIdx, rcs, searchRes))
                return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Question) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Question), context->allCS, context->counter);

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
                if (!context->insertAndCheck(pIdx, buffer[j], searchRes))
                    return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
            }
        }
    }
    partitioner.end(level, Operation::Star) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Star), context->allCS, context->counter);

    // Concatenate
    for (int i = 0; i < CSs.count(); i++)
    {
        auto buffer = stageBuffer.getView(0, samplingLimits.invertConcatMaxSamples * 2);
        concatSampler->sample(buffer, CSs[i]);

        for (int j = 0; j < buffer.count() / 2; j++)
        {
            if (!context->insertAndCheck(pIdx, buffer[j * 2], buffer[j * 2 + 1], searchRes))
                return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Concatenate) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Concatenate), context->allCS, context->counter);

    // Or
    for (int i = 0; i < CSs.count(); i++)
    {
        auto buffer = stageBuffer.getView(0, samplingLimits.invertOrMaxSamples * 2);

        orSampler->sample(buffer, CSs[i]);

        for (int j = 0; j < buffer.count() / 2; j++)
        {
            if (!context->insertAndCheck(pIdx, buffer[j * 2], buffer[j * 2 + 1], searchRes))
                return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Or) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Or), context->allCS, context->counter);

    if (searchRes.solutionIdx > -1)
        return EnumerationState::Found;
    else
        return EnumerationState::NotFound;
}

rei::EnumerationState rei::TopDownSearch::enumerateLevel(int start, int end, SearchResult& searchRes) {

    // Question
    for (int idx = start; idx < end; idx++)
    {
        if (!context->canTraverse(idx)) continue;

        auto cs = context->getCS(idx);

        if (cs.getBit(0))
        {
            auto rcs = stageBuffer[0].copy(cs).setBitOff(0);
            if (!context->insertAndCheck(idx, rcs, searchRes))
                return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Question) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Question), context->allCS, context->counter);

    // Star
    for (int idx = start; idx < end; idx++)
    {
        if (!context->canTraverse(idx)) continue;

        auto cs = context->getCS(idx);

        if (cs.getBit(0))
        {
            auto buffer = stageBuffer.getView(0, samplingLimits.invertStarMaxSamples);

            starSampler->sample(buffer, cs);

            for (int i = 0; i < buffer.count(); i++)
            {
                if (!context->insertAndCheck(idx, buffer[i], searchRes))
                    return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
            }
        }
    }
    partitioner.end(level, Operation::Star) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Star), context->allCS, context->counter);

    // Concatenate
    for (int idx = start; idx < end; idx++)
    {
        if (!context->canTraverse(idx)) continue;

        auto cs = context->getCS(idx);

        auto buffer = stageBuffer.getView(0, samplingLimits.invertConcatMaxSamples * 2);

        concatSampler->sample(buffer, cs);

        for (int i = 0; i < buffer.count() / 2; i++)
        {
            if (!context->insertAndCheck(idx, buffer[i * 2], buffer[i * 2 + 1], searchRes))
                return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Concatenate) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Concatenate), context->allCS, context->counter);

    // Or
    for (int idx = start; idx < end; idx++)
    {
        if (!context->canTraverse(idx)) continue;

        auto cs = context->getCS(idx);

        auto buffer = stageBuffer.getView(0, samplingLimits.invertOrMaxSamples * 2);

        orSampler->sample(buffer, cs);

        for (int i = 0; i < buffer.count() / 2; i++)
        {
            if (!context->insertAndCheck(idx, buffer[i * 2], buffer[i * 2 + 1], searchRes))
                return searchRes.solutionIdx > -1 ? EnumerationState::Found : EnumerationState::End;
        }
    }
    partitioner.end(level, Operation::Or) = context->lastIdx;
    LOG_OP(level, to_string(Operation::Or), context->allCS, context->counter);

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
    if (context->nextVisited[index] == NEXTVISITED_GIVEN_SENTINEL)
        left = resolver->constructRE(context->getGivenCSId(index));
    else
    {
        auto originalIdx = find_original(context->nextVisited, index);
        left = constructDownward(context->leftIdx.at(originalIdx));
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
    if (context->nextVisited[++index] == NEXTVISITED_GIVEN_SENTINEL)
        right = resolver->constructRE(context->getGivenCSId(index));
    else
    {
        auto originalIdx = find_original(context->nextVisited, index);
        right = constructDownward(context->leftIdx.at(originalIdx));
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