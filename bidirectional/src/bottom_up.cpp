#include <bottom_up.hpp>
#include <numeric>

#define LOG_OP(context, cost, op_string, dif) \
        int tbc = dif; \
        if (tbc) printf("Cost %-2d | (%s) | AllREs: %-11llu | StoredREs: %-10d | ToBeChecked: %-10d \n", \
            cost, op_string.c_str() ,context.allREs, context.cache.count(), tbc);

#define CREATE_CS(name, size) \
    std::vector<uint64_t> name##_data(size,0); \
    CS name(name##_data.data(), size); \

class rei::BottomUpSearch::Context
{
public:
    Context(const rei::LanguageSystem& languageSystem, const rei::InputParams& inputParams, int cache_capacity)
    {
        auto chunksPerCS = CS::getChuncksSize(languageSystem.getIC().size());

        posNegData = rei::posNegCSData(languageSystem, inputParams);
        posBits = CS(posNegData.data(), chunksPerCS);
        negBits = CS(posNegData.data() + chunksPerCS, chunksPerCS);

        cache = CSBuffer(new uint64_t[(cache_capacity + 1) * chunksPerCS], cache_capacity, chunksPerCS);
        leftRightIdx = new int[2 * (cache_capacity + 1)];

        allREs = 0;
        onTheFly = false;
    }

    ~Context() {
        delete[] cache.data();
        delete[] leftRightIdx;
    }

    bool insertAndCheck(CS cs, int index) {
        return insertAndCheck(cs, index, -1);
    }
    bool insertAndCheck(CS cs, int lIndex, int rIndex){
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

    CSBuffer getCacheSlice(int start, int end) {
        return cache.getView(start, end - start);
    }

    unsigned long allREs;
    bool onTheFly;

    int* leftRightIdx;
    CSBuffer cache;
    std::unordered_set<uint64_t> visited;

    std::vector<uint64_t> posNegData;
    CS posBits, negBits;
};

rei::BottomUpSearch::BottomUpSearch(const LanguageSystem& languageSystem, const InputParams& inputParams, int cache_capacity) :
    languageSystem(languageSystem), inputParams(inputParams), partitioner(inputParams.maxCost + 1) {

    costLevel = inputParams.costFunc.alphaCost() + 1;
    shortageCost = -1;
    lastRound = false;

    auto n = CS::getChuncksSize(languageSystem.getIC().size());
    CREATE_CS(cs, n)

    partitioner.fillTo(inputParams.costFunc.alphaCost(), 0);

    context = new Context(languageSystem, inputParams, cache_capacity);

    // adding eps, empty and alphabets
    context->visited.insert(cs.getHash());
    context->visited.insert(cs.toggleBit(0).getHash());
    cs.toggleBit(0);
    for (int i = 0; i < static_cast<int>(languageSystem.getAlphabetSize()); i++)
    {
        cs.toggleBit(i + 1);
        context->visited.insert(cs.getHash());
        context->cache.append().copy(cs);
        cs.toggleBit(i + 1);
    }

    partitioner.end(inputParams.costFunc.alphaCost(), Operation::Concatenate) = context->cache.count();
    partitioner.end(inputParams.costFunc.alphaCost(), Operation::Or) = context->cache.count();
}

rei::BottomUpSearch::~BottomUpSearch()
{
    delete context;
}

rei::EnumerationState rei::BottomUpSearch::enumerateCostLevel(Result& res) {

    if (costLevel > inputParams.maxCost) {
        res.message = "Max Cost has been reached!";
        return EnumerationState::End;
    }

    EnumerationState enumState = enumerateLevel(res);

    if (res.entries.size() == inputParams.n)
        return EnumerationState::Found;

    if (enumState == EnumerationState::End)
        res.message = "the search run's out of memory!";

    costLevel++;
    return enumState;

}

uint64_t rei::BottomUpSearch::estimateNextLevel() {

    auto costFunc = inputParams.costFunc;
    uint64_t count = 0;

    bool useQuestionOverOr = costFunc.alphaCost() + costFunc.operationCost(Operation::Or) >= costFunc.operationCost(Operation::Question);

    //Question Mark
    if (costLevel >= costFunc.alphaCost() + costFunc.operationCost(Operation::Question) && useQuestionOverOr) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costFunc.operationCost(Operation::Star), static_cast<Operation>(2));
        count += (end - start);
    }

    if (costLevel >= costFunc.alphaCost() + costFunc.operationCost(Operation::Star)) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costFunc.operationCost(Operation::Star), static_cast<Operation>(2));
        count += (end - start);
    }

    //Concatenate
    for (int i = costFunc.alphaCost(); 2 * i <= costLevel - costFunc.operationCost(Operation::Concatenate); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costFunc.operationCost(Operation::Concatenate));

        count += 2 * (lend - lstart) * (rend - rstart);
    }

    //Union
    if (!useQuestionOverOr && costLevel >= 2 * costFunc.alphaCost() + costFunc.operationCost(Operation::Or)) {

        auto [start, end] = partitioner.Interval(costLevel - costFunc.alphaCost() - costFunc.operationCost(Operation::Or));
        count += (end - start);
    }
    for (int i = costFunc.alphaCost(); 2 * i <= costLevel - costFunc.operationCost(Operation::Or); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costFunc.operationCost(Operation::Or));

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
    return context->cache[idx];
}

bool rei::BottomUpSearch::OnTheFly() const
{
    return context->onTheFly;
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
    if (index < languageSystem.getAlphabetSize()) { return languageSystem.getIC()[index + 1]; }

    auto [cost, op] = partitioner.indexToLevel(index);

    if (op == Operation::Question) {
        std::string res = constructDownward(context->leftRightIdx[index << 1]);
        if (res.length() > 1) return "(" + res + ")?";
        return res + "?";
    }

    if (op == Operation::Star) {
        std::string res = constructDownward(context->leftRightIdx[index << 1]);
        if (res.length() > 1) return "(" + res + ")*";
        return res + "*";
    }

    if (op == Operation::Concatenate) {
        std::string left = constructDownward(context->leftRightIdx[index << 1]);
        std::string right = constructDownward(context->leftRightIdx[(index << 1) + 1]);
        return bracket(left) + bracket(right);
    }

    if (op == Operation::Or)
    {
        std::string left = constructDownward(context->leftRightIdx[index << 1]);
        std::string right = constructDownward(context->leftRightIdx[(index << 1) + 1]);
        return left + "+" + right;
    }
}

void rei::BottomUpSearch::addSolution(rei::Result& result) {
    rei::Solution entry;
    entry.RE = constructDownward(context->cache.count() - 1);
    entry.allCSs = context->allREs;
    entry.uniqueCSs = context->cache.count();
    result.push_back(entry);
}

rei::EnumerationState rei::BottomUpSearch::enumerateLevel(rei::Result& result) {

    auto costFunc = inputParams.costFunc;
    bool useQuestionOverOr = costFunc.alphaCost() + costFunc.operationCost(Operation::Or) >= costFunc.operationCost(Operation::Question);

    // Once it uses a previous cost that is not fully stored, it should continue as the last round
    if (context->onTheFly) {
        int dif = costLevel - shortageCost;
        if (dif == costFunc.operationCost(Operation::Question) || dif == costFunc.operationCost(Operation::Star) ||
            dif == costFunc.alphaCost() + costFunc.operationCost(Operation::Concatenate) || dif == costFunc.alphaCost() + costFunc.operationCost(Operation::Or))
            lastRound = true;
    }
        
    CREATE_CS(cs, CS::getChuncksSize(languageSystem.getIC().size()))

    //Question Mark
    if (costLevel >= costFunc.alphaCost() + costFunc.operationCost(Operation::Question) && useQuestionOverOr) {

        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costFunc.operationCost(Operation::Question), static_cast<Operation>(2));
        auto pLevel = context->getCacheSlice(start, end);
        LOG_OP((*context), costLevel, to_string(Operation::Question), end - start);
        for (auto i = start; i < end; i++)
        {
            pLevel[i - start].copyTo(cs);
            if (!cs.getBit(0)) {
                processQuestion(cs);

                if (context->insertAndCheck(cs, i))
                    addSolution(result);
                if (result.entries.size() == inputParams.n)
                    return EnumerationState::Found;
            }
        }
    }
    partitioner.end(costLevel, Operation::Question) = context->cache.count();

    //Star
    if (costLevel >= costFunc.alphaCost() + costFunc.operationCost(Operation::Star)) {
        // ignore results from (*) and (?)
        auto [start, end] = partitioner.Interval(costLevel - costFunc.operationCost(Operation::Star), static_cast<Operation>(2));
        auto pLevel = context->getCacheSlice(start, end);
        LOG_OP((*context), costLevel, to_string(Operation::Star), end - start);
        for (auto i = start; i < end; i++)
        {
            pLevel[i - start].copyTo(cs);
            processStar(languageSystem.getGuideTable(), languageSystem.getAlphabetSize(), cs);

            if (context->insertAndCheck(cs, i))
                addSolution(result);
            if (result.entries.size() == inputParams.n)
                return EnumerationState::Found;
        }
    }
    partitioner.end(costLevel, Operation::Star) = context->cache.count();

    //Concatenate
    for (int i = costFunc.alphaCost(); 2 * i <= costLevel - costFunc.operationCost(Operation::Concatenate); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costFunc.operationCost(Operation::Concatenate));
        auto lpLevel = context->getCacheSlice(lstart, lend);
        auto rpLevel = context->getCacheSlice(rstart, rend);
        LOG_OP((*context), costLevel, to_string(Operation::Concatenate), 2 * (rend - rstart) * (lend - lstart));

        for (int l = lstart; l < lend; ++l) {
            CS left = lpLevel[l - lstart];
            for (int r = rstart; r < rend; ++r) {

                processConcatenate(languageSystem.getGuideTable(), languageSystem.getAlphabetSize(), left, rpLevel[r - rstart], cs.clear());

                if (context->insertAndCheck(cs, l, r))
                    addSolution(result);
                if (result.entries.size() == inputParams.n)
                    return EnumerationState::Found;

                processConcatenate(languageSystem.getGuideTable(), languageSystem.getAlphabetSize(), rpLevel[r - rstart], left, cs.clear());

                if (context->insertAndCheck(cs, r, l))
                    addSolution(result);
                if (result.entries.size() == inputParams.n)
                    return EnumerationState::Found;
            }
        }

    }
    partitioner.end(costLevel, Operation::Concatenate) = context->cache.count();

    //Union
    if (!useQuestionOverOr && costLevel >= 2 * costFunc.alphaCost() + costFunc.operationCost(Operation::Or)) {

        auto [start, end] = partitioner.Interval(costLevel - costFunc.alphaCost() - costFunc.operationCost(Operation::Or));
        auto pLevel = context->getCacheSlice(start, end);
        LOG_OP((*context), costLevel, to_string(Operation::Or), end - start);

        for (int r = start; r < end; ++r) {

            pLevel[r - start].copyTo(cs);
            processQuestion(cs);

            if (context->insertAndCheck(cs, -2, r))
                addSolution(result);
            if (result.entries.size() == inputParams.n)
                return EnumerationState::Found;
        }
    }
    for (int i = costFunc.alphaCost(); 2 * i <= costLevel - costFunc.operationCost(Operation::Or); ++i) {

        auto [lstart, lend] = partitioner.Interval(i);
        auto [rstart, rend] = partitioner.Interval(costLevel - i - costFunc.operationCost(Operation::Or));
        auto lpLevel = context->getCacheSlice(lstart, lend);
        auto rpLevel = context->getCacheSlice(rstart, rend);
        LOG_OP((*context), costLevel, to_string(Operation::Or), (rend - rstart) * (lend - lstart));
        for (int l = lstart; l < lend; ++l) {
            CS left = lpLevel[l - lstart];
            for (int r = rstart; r < rend; ++r) {

                processOr(left, rpLevel[r - rstart], cs.clear());

                if (context->insertAndCheck(cs, l, r))
                    addSolution(result);
                if (result.entries.size() == inputParams.n)
                    return EnumerationState::Found;
            }
        }
    }
    partitioner.end(costLevel, Operation::Or) = context->cache.count();

    if (lastRound) return EnumerationState::End;
    if (context->onTheFly && shortageCost == (unsigned short)-1)  shortageCost = costLevel;

    return EnumerationState::NotFound;
}