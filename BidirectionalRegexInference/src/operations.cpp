#include <operations.h>

#include <unordered_set>
#include <optional>
#include <numeric>
#include <cs_utils.h>

void powerset_element(int size, int index, std::function<void(int)> it) {
    for (int i = 0; i < size; i++) {
        if (index & (1 << i)) { it(i); }
    }
}

template <typename T>
void depth_traversal(int maxDepth, T start, std::function<int(int)> branchCount,
    std::function<std::pair<bool, T>(int, int, T)> tryExtend, std::function<void(T)> emit) {

    std::vector<int> idx(maxDepth, 0);
    std::vector<T> params(maxDepth + 1, start);
    int depth = 0;

    while (true)
    {
        if (idx[depth] < branchCount(depth))
        {
            std::pair<bool, T> res = tryExtend(depth, idx[depth], params[depth]);

            if (res.first)
            {
                params[depth + 1] = res.second;

                if (depth + 1 == maxDepth)
                {
                    emit(res.second);
                    ++idx[depth];
                }
                else
                {
                    ++depth;
                    idx[depth] = 0;
                }
            }
            else
            {
                ++idx[depth];
            }
        }
        else
        {
            if (depth == 0)
                break;

            idx[depth] = 0;
            --depth;
            ++idx[depth];
        }
    }
}

template <typename T>
std::optional<T> sample_random_leaf(
    int maxDepth,
    const T& start,
    std::function<int(int)> branchCount,
    std::function<std::pair<bool, T>(int, int, const T&)> tryExtend,
    std::mt19937_64 rng
) {
    if (maxDepth == 0)
        return start;

    std::vector<int> idx(maxDepth, 0);
    std::vector<T> params(maxDepth + 1, start);

    int depth = 0;
    std::size_t leafCount = 0;
    std::optional<T> chosen;

    while (true)
    {
        if (idx[depth] < branchCount(depth))
        {
            auto res = tryExtend(depth, idx[depth], params[depth]);

            if (res.first)
            {
                params[depth + 1] = res.second;

                if (depth + 1 == maxDepth)
                {
                    // We found a valid leaf
                    ++leafCount;

                    // Reservoir sampling: replace with probability 1 / leafCount
                    std::uniform_int_distribution<std::size_t> dist(1, leafCount);
                    if (dist(rng) == 1)
                        chosen = res.second;

                    ++idx[depth];
                }
                else
                {
                    ++depth;
                    idx[depth] = 0;
                }
            }
            else
            {
                ++idx[depth];
            }
        }
        else
        {
            if (depth == 0)
                break;

            idx[depth] = 0;
            --depth;
            ++idx[depth];
        }
    }

    return chosen; // empty if no valid leaf exists
}

template <typename T>
std::optional<T> sample_random_leaf_fast(
    int maxDepth,
    const T& start,
    std::function<int(int)> branchCount,
    std::function<std::pair<bool, T>(int, int, const T&)> tryExtend,
    std::mt19937_64 rng,
    int maxRetries = 1000
) {

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        T current = start;

        for (int depth = 0; depth < maxDepth; ++depth) {
            int bc = branchCount(depth);
            if (bc == 0) break;

            std::uniform_int_distribution<int> dist(0, bc - 1);
            int idx = dist(rng);

            auto res = tryExtend(depth, idx, current);
            if (!res.first)
                goto restart;

            current = res.second;
        }

        return current; // success
    restart:;
    }

    return std::nullopt;
}

template<typename T>
bool fits_in_uint64(const std::vector<std::vector<T>>& lists)
{
    uint64_t limit = std::numeric_limits<uint64_t>::max();
    uint64_t total = 1;

    for (const auto& inner : lists)
    {
        uint64_t k = inner.size();

        if (k == 0)
            return true;

        // Overflow check before multiplication
        if (total > limit / k)
            return false;

        total *= k;
    }

    return true;
}

Pair<CS> powerset_element(std::vector<Pair<CS>> pairs, int index) {
    Pair<CS> result{ CS(), CS() };
    powerset_element(pairs.size(), index, [&result, &pairs](int i) {
        result.left |= pairs[i].left;
        result.right |= pairs[i].right;
        });
    return result;
}

std::string rei::to_string(Operation op) {
    switch (op)
    {
    case Operation::Question:
        return "Q";
    case Operation::Star:
        return "S";
    case Operation::Concatenate:
        return "C";
    case Operation::Or:
        return "O";
    default:
        break;
    }
    return "";
}

std::vector<CS> rei::revertQuestion(const CS& cs) {
    if (cs & CS::one())
        return std::vector<CS>{ cs | CS::one() };
    else
        return std::vector<CS>{};
}

// ========= Star =========

std::vector<CS> rei::revertStarRandom(const CS& cs, size_t maxSamples, const GuideTable& guideTable, uint64_t seed) {

    auto baseCS = CS();

    for (int i = 1; i < guideTable.ICsize; i++)
    {
        if (!((CS::one() << i) & cs))
            continue;

        bool exists = false;

        for (auto const& pair : guideTable.IterateRow(i))
        {
            if (((CS::one() << pair.left) & cs) && ((CS::one() << pair.right) & cs))
            {
                exists = true;
                break;
            }
        }

        if (!exists)
            baseCS |= (CS::one() << i);
    }

    if (processStar(guideTable, baseCS) != cs)
        return {};

    std::vector<CS> res;

    auto bits = getBits(baseCS ^ cs, guideTable.ICsize);
    const auto bitsCount = bits.size();

    if (bitsCount < 64 && (1ULL << bitsCount) <= maxSamples)
        return revertStar(cs, guideTable);

    std::mt19937_64 rng(seed);
    std::bernoulli_distribution coin(0.5);

    std::vector<CS> result;
    result.reserve(maxSamples);

    std::unordered_set<CS> visited;

    while (result.size() < maxSamples) {

        CS submask = getRandom(bits, rng, coin) | baseCS;

        if (submask == cs) continue;

        if (visited.insert(submask).second)
            result.emplace_back(submask);
    }

    return result;
}

std::vector<CS> rei::revertStar(const CS& cs, const GuideTable& guideTable) {

    auto baseCS = CS();

    for (int i = 1; i < guideTable.ICsize; i++)
    {
        if (!((CS::one() << i) & cs))
            continue;

        bool exists = false;

        for (auto const& pair : guideTable.IterateRow(i))
        {
            if (((CS::one() << pair.left) & cs) && ((CS::one() << pair.right) & cs))
            {
                exists = true;
                break;
            }
        }
        
        if (!exists)
            baseCS |= (CS::one() << i);
    }

    if (processStar(guideTable, baseCS) != cs)
        return {};

    std::vector<CS> res;

    auto bits = getBits(baseCS ^ cs, guideTable.ICsize);
    const auto bitsCount = bits.size();
    const auto count = 1UL << bitsCount;

    if (bitsCount > 64)
    {
        printf("revert Star can't handle a CS with more than 64 toggled bits!\n");
        return {};
    }

    for (int i = 0; i < count; i++)
    {
        auto c = baseCS;
        powerset_element(bitsCount, i, [&c, &bits](int index) { c |= (CS::one() << bits[index]); });
        if(c != cs)
            res.push_back(c);
    }

    return res;
}

std::vector<CS> rei::revertStarBrute(const GuideTable& guideTable, const CS& target)
{
    std::vector<CS> result;

    const int n = guideTable.ICsize;
    const std::uint64_t maxMask = 1ULL << n;

    for (std::uint64_t mask = 0; mask < maxMask; ++mask)
    {
        CS cs = CS::fromLow(mask);

        if (processStar(guideTable, cs) == target)
        {
            result.emplace_back(cs);
        }
    }

    return result;
}

// ========= Concat =========

std::vector<Pair<CS>> rei::revertConcatBrute(const CS& target, const GuideTable& guideTable)
{
    std::vector<Pair<CS>> result;

    const int n = guideTable.ICsize;
    const std::uint64_t maxMask = 1ULL << n;

    for (std::uint64_t lMask = 0; lMask < maxMask; ++lMask)
    {
        CS left = CS::fromLow(lMask);

        for (std::uint64_t rMask = 0; rMask < maxMask; ++rMask)
        {
            CS right = CS::fromLow(rMask);

            if ((left & CS::one()) && !(target & right)) continue;
            if ((right & CS::one()) && !(target & left)) continue;

            if (processConcatenate(guideTable, left, right) == target)
            {
                result.emplace_back(left, right);
            }
        }
    }

    return result;
}

std::vector<Pair<CS>> rei::revertConcatRandom(const CS& cs, size_t maxSamples, const GuideTable& guideTable, uint64_t seed) {

    vector<vector<Pair<int>>> sourcePairs;
    sourcePairs.reserve(guideTable.ICsize);

    if (cs & CS::one())
        sourcePairs.push_back({ {0, 0} });

    for (int i = 1; i < guideTable.ICsize; i++)
    {
        if (!((CS::one() << i) & cs))
            continue;

        vector<Pair<int>> row;

        row.emplace_back(0, i);
        row.emplace_back(i, 0);

        for (auto const& pair : guideTable.IterateRow(i))
            row.push_back(pair);

        sourcePairs.push_back(row);
    }

    if (sourcePairs.empty())
        return {};

    std::mt19937_64 rng(seed);
    std::bernoulli_distribution coin(0.5);

    auto nodeMasks = [&guideTable, &cs, &sourcePairs](int index, rei::Pair<int> pair) {

        auto pairMatch = [](const GuideTable& guideTable, const CS& cs, const rei::Pair<int>& a, const rei::Pair<int>& b) {

            auto adj = guideTable.adjacencyList[a.left];
            for (int i = 0; i < adj.size(); i++)
            {
                if (adj[i].first == b.right && !(cs & (CS::one() << adj[i].second)))
                    return false;
            }

            adj = guideTable.adjacencyList[b.left];
            for (int i = 0; i < adj.size(); i++)
            {
                if (adj[i].first == a.right && !(cs & (CS::one() << adj[i].second)))
                    return false;
            }

            return true;
        };

        vector<CS> masks(sourcePairs.size() - (index + 1), CS());

        for (int i = index + 1; i < sourcePairs.size(); i++)
        {
            auto sourceRow = sourcePairs[i];
            auto mask = CS();
            for (int j = 0; j < sourceRow.size(); j++)
            {
                if (pairMatch(guideTable, cs, pair, sourceRow[j]))
                    mask |= CS::one() << j;
            }
            if (!mask)
                return masks;
            masks[i - (index + 1)] = mask;
        }

        return masks;
    };

    vector<vector<vector<CS>>> masks;
    masks.reserve(sourcePairs.size() - 1);

    for (int i = 0; i < sourcePairs.size() - 1; i++)
    {
        auto sourceRow = sourcePairs[i];
        vector<vector<CS>> row;
        row.reserve(sourceRow.size());

        for (int j = 0; j < sourceRow.size(); j++)
        {
            row.push_back(nodeMasks(i, sourceRow[j]));
        }

        masks.push_back(row);
    }

    vector<vector<double>> ratios;
    ratios.reserve(sourcePairs.size() - 1);

    for (int i = 0; i < masks.size(); i++)
    {
        auto sourceRow = masks[i];
        vector<uint64_t> row;
        uint64_t sum = 0;
        row.reserve(masks.size());

        for (int j = 0; j < sourceRow.size(); j++)
        {
            auto sourceCol = sourceRow[j];
            uint64_t count = 1;
            for (int k = 0; k < sourceCol.size(); k++)
            {
                count *= sourceCol[k].popCount();
                if (count == 0) break;
            }

            row.push_back(count);
            sum += count;
        }

        vector<double> nrow;
        for (int i = 0; i < row.size(); i++)
        {
            nrow.push_back(static_cast<double>(row[i]) / sum);
        }

        ratios.push_back(nrow);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    vector<Pair<CS>> res;
    std::unordered_set<Pair<CS>> visited;
    int counter = 0;

    while (res.size() < maxSamples && counter++ < maxSamples * 4) {

        auto rPair = Pair<CS>(CS(), CS());
        vector<int> picks;
        auto pickmask = CS::all();

        for (int i = 0; i < sourcePairs.size(); i++)
        {
            auto row = sourcePairs[i];
            vector<double> ratio;

            for (int j = 0; j < row.size(); j++)
            {
                if (pickmask & (CS::one() << j))
                {
                    ratio.push_back(i == sourcePairs.size() - 1 ? 1 : ratios[i][j]);
                }
                else
                    ratio.push_back(0);
            }

            if (std::accumulate(ratio.begin(), ratio.end(), 0.0) <= 0)
                break;

            std::discrete_distribution<> dist(ratio.begin(), ratio.end());
            int sampled_index = dist(gen);
            rPair.left |= CS::one() << row[sampled_index].left;
            rPair.right |= CS::one() << row[sampled_index].right;

            if (i == sourcePairs.size() - 1)
            {
                if (rPair.left == CS::one() || rPair.right == CS::one())
                    break;

                if (!visited.insert(rPair).second)
                    break;

                res.push_back(rPair);
                break;
            }

            picks.push_back(sampled_index);
            pickmask = CS::all();
            for (int j = 0; j < picks.size(); j++)
            {
                pickmask &= masks[j][picks[j]][i - j];
            }
        }

    }

    return res;
}

void revertConcatPrimary(const CS& cs, const rei::GuideTable& guideTable, std::vector<Pair<CS>>& result) {

    vector<vector<Pair<int>>> sourcePairs;
    sourcePairs.reserve(guideTable.ICsize);

    if (cs & CS::one())
        sourcePairs.push_back({ {0, 0} });

    for (int i = 1; i < guideTable.ICsize; i++)
    {
        if (!((CS::one() << i) & cs))
            continue;

        vector<Pair<int>> row;

        row.emplace_back(0, i);
        row.emplace_back(i , 0);

        for (auto const& pair : guideTable.IterateRow(i))
            row.push_back(pair);

        sourcePairs.push_back(row);
    }

    if (sourcePairs.empty())
        return;

    depth_traversal<Pair<CS>>(sourcePairs.size(), Pair<CS>{CS(), CS()},
    [&sourcePairs](int depth) { return sourcePairs[depth].size(); },
    [&sourcePairs, &guideTable, & cs](int depth, int element, Pair<CS> pair) {

        auto p = sourcePairs[depth][element];

        Pair<CS> next(pair.left | (CS::one() << p.left), pair.right | (CS::one() << p.right));

        auto con = rei::processConcatenate(guideTable, next.left, next.right);

        return std::pair<bool,Pair<CS>>( !(con & ~cs), next );
    },
    [&result](Pair<CS> pair){ 
        result.push_back(pair); 
    });
}

void revertConcatSecondary(const Pair<CS>& pair, const CS& cs, const rei::GuideTable& guideTable,  std::vector<Pair<CS>>& result) {

    auto rightMask = CS(); // bits that we should not set

    for (int i = 0; i < guideTable.ICsize; i++)
    {
        if ((CS::one() << i) & pair.left) {
            auto adj = guideTable.adjacencyList[i];
            for (size_t j = 0; j < adj.size(); j++)
            {
                if (!((CS::one() << adj[j].second) & cs))
                    rightMask |= CS::one() << adj[j].first;
            }
        }
    }

    depth_traversal<Pair<CS>>(guideTable.ICsize, Pair<CS>(pair.left, rightMask), [](int i) { return 2; },
        [&pair, &guideTable, &cs](int depth, int elemnet, Pair<CS> mask) {

            auto next = mask;

            if (elemnet == 1) {

                if ((CS::one() << depth) & pair.left) // already been set
                    return std::pair<bool, Pair<CS>>(false, mask);

                next.left |= CS::one() << depth;

                auto adj = guideTable.adjacencyList[depth];

                for (int i = 0; i < adj.size(); i++)
                {
                    if (!((CS::one() << adj[i].second) & cs)) {
                        // check if the current left word combined with the existing words on 
                        // the right will produce word that is not included in the target
                        if ((CS::one() << adj[i].first & pair.right))
                            return std::pair<bool, Pair<CS>>(false, mask);
                        else
                            next.right |= CS::one() << adj[i].first;
                    }
                }
            }

            return std::pair<bool, Pair<CS>>(true, next);
        },
        [&guideTable, &pair, &result](Pair<CS> mask) {

            const auto combined = pair.right | mask.right;

            std::vector<int> bits;
            for (int i = 0; i < guideTable.ICsize; i++)
            {
                auto cs = (CS::one() << i);
                if (!(cs & combined))
                    bits.push_back(i);
            }

            const size_t numBits = bits.size();
            const size_t numCombinations = 1ull << numBits;

            for (size_t subset = 0; subset < numCombinations; ++subset)
            {
                CS combination = pair.right;

                for (size_t bit = 0; bit < numBits; ++bit)
                {
                    if (subset & (1ull << bit))
                    {
                        combination |= (CS::one() << bits[bit]);
                    }
                }

                if(mask.left != CS::one() && combination != CS::one())
                    result.push_back({ mask.left, combination });
            }
        }
    );
}

std::vector<Pair<CS>> rei::revertConcat(const CS& cs, const GuideTable& guideTable)
{
    std::vector<Pair<CS>> primary;

    revertConcatPrimary(cs, guideTable, primary);

    auto primary_size = primary.size();

    std::vector<Pair<CS>> result;

    for (int i = 0; i < primary_size; i++)
    {
        revertConcatSecondary(primary[i], cs, guideTable, result);
    }

    return result;
}

// ========= Or =========

std::vector<Pair<CS>> rei::revertOrRandom(const CS& cs, size_t maxSamples, int ICsize, uint64_t seed)
{
    const auto popCount = cs.popCount();

    if (popCount < 64 && (1ULL << (popCount - 1)) <= maxSamples)
        return revertOr(cs);

    std::vector<int> bits = getBits(cs, ICsize);

    std::mt19937_64 rng(seed);
    std::bernoulli_distribution coin(0.5);

    std::vector<Pair<CS>> result;
    result.reserve(maxSamples);

    std::unordered_set<CS> visited;

    while (result.size() < maxSamples) {

        CS submask = getRandom(bits, rng, coin);

        if (!submask) continue;
        if (submask == cs) continue;

        CS complement = cs ^ submask;

        if(visited.insert(submask).second && visited.insert(complement).second) 
            result.emplace_back(submask, complement);
    }

    return result;
}

vector<Pair<CS>> rei::revertOr(const CS& cs) {

    const auto popCount = cs.popCount();

    if (popCount > 64)
    {
        printf("revert Union can't handle a CS with more than 64 toggled bits!\n");
        return {};
    }

    CS submask = cs;

    auto count = 1ULL << (popCount - 1);

    // remove the pair that contains zero or one
    count--;
    submask--;
    submask = submask & cs;

    if (cs & CS::one()) 
    {
        count--;
        submask--;
        submask = submask & cs;
    }

    vector<Pair<CS>> pairs;
    pairs.reserve(count);

    for (size_t i = 0; i < count; i++)
    {
        CS complement = cs ^ submask;
        pairs.emplace_back(submask, complement);
        submask--;
        submask = submask & cs;
    }

    return pairs;
}