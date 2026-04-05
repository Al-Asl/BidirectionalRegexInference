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

// ========= Star =========

void find_star_base(const rei::GuideTable& guideTable, const CS cs, CS& baseCS) {

    for (int i = 1; i < guideTable.ICsize; i++)
    {
        if (!(cs.getBit(i)))
            continue;

        bool exists = false;

        for (auto const& pair : guideTable.iterateSplits(i))
        {
            if (cs.getBit(pair.left) && cs.getBit(pair.right))
            {
                exists = true;
                break;
            }
        }

        if (!exists)
            baseCS.setBitOn(i);
    }
}

void rei::revertStarRandom(const GuideTable& guideTable, const CS& cs, CSBuffer& buffer, uint64_t seed) {

    CS baseCS = buffer.append();
    find_star_base(guideTable, cs, baseCS.clear());

    // Not all CS have an invert for Star
    CS baseCSStar = buffer.append().copy(baseCS);
    processStar(guideTable, baseCSStar);
    if (baseCSStar != cs)
    {
        buffer.removeLast();
        buffer.removeLast();
        return;
    }
    buffer.removeLast();

    CS temp = buffer.append();
    baseCS.copyTo(temp);
    temp ^= cs;
    auto bits = temp.getBits();
    buffer.removeLast();

    if (bits.size() < 63 && (1UL << bits.size()) - 1 <= buffer.size()) {
        revertStar(guideTable, cs, buffer);
        return;
    }

    std::mt19937 rng(seed);
    std::bernoulli_distribution coin(0.5);

    std::unordered_set<uint64_t> visited;

    for (int i = 1; i < buffer.size(); i++)
        buffer.append().copy(baseCS);

    for (int i = 0; i < buffer.size(); i++)
    {
        auto submask = buffer[i];
        rei::getRandom(bits, submask, rng, coin);

        if (submask == cs) 
            continue;

        if (!visited.insert(submask.getHash()).second)
            i--;
    }
}

void rei::revertStar(const GuideTable& guideTable, const CS& cs, CSBuffer& buffer) {

    CS baseCS = buffer.append();
    find_star_base(guideTable, cs, baseCS.clear());

    // Not all CS have an invert for Star
    CS baseCSStar = buffer.append().copy(baseCS);
    processStar(guideTable, baseCSStar);
    if (baseCSStar != cs)
    {
        buffer.removeLast();
        buffer.removeLast();
        return;
    }
    buffer.removeLast();

    CS temp = buffer.append();
    baseCS.copyTo(temp);
    temp ^= cs;
    auto bits = temp.getBits();
    buffer.removeLast();

    const auto bitsCount = bits.size() > 63 ? 63 : bits.size();
    auto count = (1UL << bitsCount) - 1;
    count = count < buffer.size() ? count : buffer.size();

    for (int i = 1; i < count; i++)
        buffer.append().copy(baseCS);

    for (int i = 1; i < count; i++)
    {
        auto submask = buffer[i];
        powerset_element(bitsCount, i, [&submask, &bits](int idx) { submask.setBitOn(bits[idx]); });
    }
}

void rei::revertStarBrute(const GuideTable& guideTable, const CS& target, CSBuffer& buffer)
{
    const int n = guideTable.ICsize;
    const std::uint64_t maxMask = 1ULL << n;

    CS cs = buffer.append();

    for (std::uint64_t mask = 0; mask < maxMask; ++mask)
    {
        cs.clear().setChunck(mask, 0);
        rei::processStar(guideTable, cs);

        if (cs == target)
        {
            cs.clear().setChunck(mask, 0);
            if (buffer.isFull())
                return;
            else
                cs = buffer.append();
        }
    }

    buffer.removeLast();
}

// ========= Concat =========

void revertConcatBrute(const rei::GuideTable& guideTable, const CS& cs, CS reg, CSBuffer& buffer, int& sampleCount)
{
    const int n = guideTable.ICsize;
    const std::uint64_t maxMask = 1ULL << n;
    const int maxSamples = buffer.size() / 2;
    sampleCount = 0;

    for (std::uint64_t lMask = 0; lMask < maxMask; ++lMask)
    {
        for (std::uint64_t rMask = 0; rMask < maxMask; ++rMask)
        {
            auto left = buffer[sampleCount * 2].setChunck(lMask, 0);
            auto right = buffer[sampleCount * 2 + 1].setChunck(rMask, 0);

            if (left.getBit(0) && !cs.intersects(right)) continue;
            if (right.getBit(0) && !cs.intersects(left)) continue;

            rei::processConcatenate(guideTable, left, right, reg);
            if (reg == cs)
            {
                if (++sampleCount == maxSamples) 
                    return;
            }
        }
    }
}

// return true if the pair a after union with the pair b will be concatenated
// to the cs r such that r is a subset of target
bool canUnionTo(const rei::GuideTable& guideTable, const CS& target, Pair<CS> a, Pair<uint16_t> b) {

    if (b.left == 0 && !target.containsAll(a.right))
        return false;

    if (a.left.getBit(0) && !target.getBit(b.right))
        return false;

    if (b.right == 0 && !target.containsAll(a.left))
        return false;

    if (a.right.getBit(0) && !target.getBit(b.left))
        return false;

    for (auto [right, res] : guideTable.iterateSuffixes(b.left))
    {
        if (right == b.right || !a.right.getBit(right))
            continue;

        if (!target.getBit(res))
            return false;
    }

    for (auto [left, res] : guideTable.iteratePrefixes(b.right))
    {
        if (left == b.left || !a.left.getBit(left))
            continue;

        if (!target.getBit(res))
            return false;
    }

    return true;
}

bool canUnionTo(const rei::GuideTable& guideTable, const CS& target, Pair<uint16_t> a, Pair<uint16_t> b) {

    if (a.left == 0 && !target.getBit(b.right))
        return false;

    if (b.left == 0 && !target.getBit(a.right))
        return false;

    if (a.right == 0 && !target.getBit(b.left))
        return false;

    if (b.right == 0 && !target.getBit(a.left))
        return false;

    for (auto [right, res] : guideTable.iterateSuffixes(a.left))
    {
        if (right == b.right && !target.getBit(res))
            return false;
    }

    for (auto [left, res] : guideTable.iteratePrefixes(a.right))
    {
        if (left == b.left && !target.getBit(res))
            return false;
    }

    return true;
}

void rei::revertConcatRandom(const GuideTable& guideTable, const CS& cs, CSBuffer& buffer, uint64_t seed, bool enhanceCount) {

    auto pick = [](const rei::GuideTable& guideTable, int idx, int pairIdx) {
        switch (pairIdx) {
        case 0:
            return Pair<uint16_t>(0, idx);
        case 1:
            return Pair<uint16_t>(idx, 0);
        default:
            pairIdx--;
            for (auto const& p : guideTable.iterateSplits(idx))
            {
                if (--pairIdx == 0)
                    return p;
            }
        }
    };

    auto const countsRowSize = (guideTable.getMaxSplits() + 2);
    auto const countsSize = (guideTable.ICsize - 1) * countsRowSize;
    uint16_t* counts = nullptr;

    if (enhanceCount)
    {
        counts = new uint16_t[countsSize];
        std::fill(counts, counts + countsSize, 1);

        for (uint16_t i = 1; i < guideTable.ICsize - 1; i++)
        {
            if (!cs.getBit(i))
                continue;

            uint16_t* current_row = counts + (i - 1) * countsRowSize;

            for (uint16_t j = i + 1; j < guideTable.ICsize; j++)
            {
                if (!cs.getBit(j))
                    continue;

                uint16_t* row = counts + (guideTable.ICsize - 2) * countsRowSize;
                std::fill(row, row + countsRowSize, 0);

                for (uint16_t k = 0; k < guideTable.getSplitsCount(i) + 2; k++)
                {
                    auto pair = pick(guideTable, i, k);
                    for (uint16_t w = 0; w < guideTable.getSplitsCount(j) + 2; w++) {
                        if (canUnionTo(guideTable, cs, pair, pick(guideTable, j, w)))
                            row[k] += 1;
                    }
                }

                // Prevent overflow
                uint16_t max = 0;
                for (uint16_t k = 0; k < guideTable.getSplitsCount(i) + 2; k++) {
                    if (current_row[k] > max)
                        max = current_row[k];
                }

                for (uint16_t k = 0; k < guideTable.getSplitsCount(i) + 2; k++) {
                    current_row[k] = (max > 255 ? current_row[k] >> 4 : current_row[k]) * row[k];
                }
            }
        }
    }

    std::mt19937 gen(seed);
    std::unordered_set<uint64_t> visited;
    auto const maxSamples = (buffer.size() / 2) * 4;

    for (int counter = 0; counter < maxSamples; counter++)
    {
        CS left = buffer.append().clear();
        CS right = buffer.append().clear();
        bool accept = true;

        if (cs.getBit(0))
        {
            left.setBitOn(0);
            right.setBitOn(0);
        }

        for (uint16_t i = 1; i < guideTable.ICsize; i++)
        {
            if (!cs.getBit(i))
                continue;

            int pairIdx;

            if (enhanceCount)
            {
                auto counts_ptr = counts + ((i - 1) * countsRowSize);
                std::discrete_distribution<> dis(counts_ptr, counts_ptr + guideTable.getSplitsCount(i) + 2);
                pairIdx = dis(gen);
            }
            else
            {
                std::uniform_int_distribution<> dis(0, guideTable.getSplitsCount(i) + 1);
                pairIdx = dis(gen);
            }

            auto pair = pick(guideTable, i, pairIdx);

            if (!canUnionTo(guideTable, cs, Pair<CS>(left, right), pair))
                { accept = false; break; }

            left.setBitOn(pair.left);
            right.setBitOn(pair.right);
        }

        if (accept) {

            if (!(left.popCount() == 1 && left.getBit(0)) &&
                !(right.popCount() == 1 && right.getBit(0)))
            {
                auto hash = left.getHash() ^ (right.getHash() << 1);
                if (visited.insert(hash).second)
                {
                    if (buffer.isFull())
                    {
                        if (enhanceCount)
                            delete[] counts;
                        return;
                    }else
                        continue;
                }
            }
        }

        buffer.removeLast();
        buffer.removeLast();
    }

    if (enhanceCount)
        delete[] counts;
}

void revertConcatPrimary(const rei::GuideTable& guideTable, const CS& cs, CSBuffer& buffer) {

    auto bits = cs.getBits();

    if (bits.empty())
        return;

    auto pick = [](const rei::GuideTable& guideTable, int idx, int pairIdx) {
        switch (pairIdx) {
        case 0:
            return Pair<uint16_t>(0, idx);
        case 1:
            return Pair<uint16_t>(idx, 0);
        default:
            pairIdx--;
            for (auto const& p : guideTable.iterateSplits(idx))
            {
                if (--pairIdx == 0)
                    return p;
            }
        }
    };

    auto branchCount = [](const rei::GuideTable& guideTable ,int depth) {
        if (depth == 0) 
            return 1;
        else
            return guideTable.getSplitsCount(depth) + 2;
    };
    
    std::vector<uint64_t> paramsData((bits.size() + 1) * 2 * cs.getSize(), 0);
    CSBuffer params(paramsData.data(), (bits.size() + 1) * 2, cs.getSize());

    const int maxDepth = bits.size();
    std::vector<int> idx(maxDepth, 0);
    int depth = 0;

    while (true)
    {
        if (idx[depth] < branchCount(guideTable, bits[depth]))
        {
            auto pair = pick(guideTable, bits[depth], idx[depth]);

            if (canUnionTo(guideTable, cs, Pair<CS>(params[depth * 2], params[depth * 2 + 1]), pair))
            {
                auto left = params[(depth + 1) * 2].copy(params[depth * 2]).setBitOn(pair.left);
                auto right = params[(depth + 1) * 2 + 1].copy(params[depth * 2 + 1]).setBitOn(pair.right);

                if (depth + 1 == maxDepth)
                {
                    if (!(left.popCount() == 1 && left.getBit(0)) &&
                        !(right.popCount() == 1 && right.getBit(0)))
                    {
                        buffer.append().copy(params[(depth + 1) * 2]);
                        buffer.append().copy(params[(depth + 1) * 2 + 1]);
                        if (buffer.isFull()) return;
                    }

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

void rei::revertConcat(const GuideTable& guideTable, const CS& cs, CSBuffer& buffer) {
    revertConcatPrimary(guideTable, cs, buffer);
}

// ========= Or =========

void rei::revertOrRandom(const CS& cs, CSBuffer& buffer, uint64_t seed) {

    const auto popCount = cs.popCount();

    if (popCount < 63 && (1UL << (popCount - 1)) <= buffer.size()/2)
    {
        revertOr(cs, buffer);
        return;
    }

    std::vector<int> bits = cs.getBits();

    std::mt19937 rng(seed);
    std::bernoulli_distribution coin(0.5);

    std::unordered_set<uint64_t> visited;
    CS submask     = buffer.append();
    CS complement  = buffer.append();

    while (true) {

        getRandom(bits, submask.clear(), rng, coin);

        if (!submask) continue;
        if (submask == cs) continue;

        cs.copyTo(complement);
        complement ^= submask;

        if (visited.insert(submask.getHash()).second && visited.insert(complement.getHash()).second)
        {
            if (buffer.isFull()) return;

            submask     = buffer.append();
            complement  = buffer.append();
        }
    }

}

void rei::revertOr(const CS& cs, CSBuffer& buffer) {

    auto popCount = cs.popCount();
    popCount = popCount > 63 ? 63 : popCount;

    CS submask     = buffer.append().copy(cs);
    CS complement  = buffer.append().copy(cs);

    auto count = 1UL << (popCount - 1);

    // remove the pair that contains zero or one
    --count;
    --submask;
    submask &= cs;

    if (cs.getBit(0))
    {
        --count;
        --submask;
        submask &= cs;
    }

    for (int i = 0; i < count; i++)
    {
        complement ^= submask;

        if (buffer.isFull()) return;

        submask     = buffer.append().copy(submask);
        complement  = buffer.append().copy(cs);

        --submask;
        submask &= cs;
    }

    buffer.removeLast();
    buffer.removeLast();
}

// =========== The following explain how to revert Concatenation in full (with duplicates) ===========

//void revertConcatPrimary(const CS& cs, const rei::GuideTable& guideTable, std::vector<Pair<CS>>& result) {
//
//    vector<vector<Pair<uint16_t>>> sourcePairs;
//    sourcePairs.reserve(guideTable.ICsize);
//
//    if (cs & CS::one())
//        sourcePairs.push_back({ {0, 0} });
//
//    for (int i = 1; i < guideTable.ICsize; i++)
//    {
//        if (!((CS::one() << i) & cs))
//            continue;
//
//        vector<Pair<uint16_t>> row;
//
//        row.emplace_back(0, i);
//        row.emplace_back(i, 0);
//
//        for (auto const& pair : guideTable.iterateSplits(i))
//            row.push_back(pair);
//
//        sourcePairs.push_back(row);
//    }
//
//    if (sourcePairs.empty())
//        return;
//
//    depth_traversal<Pair<CS>>(sourcePairs.size(), Pair<CS>{CS(), CS()},
//        [&sourcePairs](int depth) { return sourcePairs[depth].size(); },
//        [&sourcePairs, &guideTable, &cs](int depth, int element, Pair<CS> pair) {
//
//            auto p = sourcePairs[depth][element];
//
//            Pair<CS> next(pair.left | (CS::one() << (int)p.left), pair.right | (CS::one() << (int)p.right));
//
//            auto con = rei::processConcatenate(guideTable, next.left, next.right);
//
//            return std::pair<bool, Pair<CS>>(!(con & ~cs), next);
//        },
//        [&result](Pair<CS> pair) {
//            result.push_back(pair);
//        });
//}
//
//void revertConcatSecondary(const Pair<CS>& pair, const CS& cs, const rei::GuideTable& guideTable, std::vector<Pair<CS>>& result) {
//
//    auto rightMask = CS(); // bits that we should not set
//
//    for (int i = 0; i < guideTable.ICsize; i++)
//    {
//        if ((CS::one() << i) & pair.left) {
//            auto adj = guideTable.adjacencyList[i];
//            for (size_t j = 0; j < adj.size(); j++)
//            {
//                if (!((CS::one() << adj[j].second) & cs))
//                    rightMask |= CS::one() << adj[j].first;
//            }
//        }
//    }
//
//    depth_traversal<Pair<CS>>(guideTable.ICsize, Pair<CS>(pair.left, rightMask), [](int i) { return 2; },
//        [&pair, &guideTable, &cs](int depth, int elemnet, Pair<CS> mask) {
//
//            auto next = mask;
//
//            if (elemnet == 1) {
//
//                if ((CS::one() << depth) & pair.left) // already been set
//                    return std::pair<bool, Pair<CS>>(false, mask);
//
//                next.left |= CS::one() << depth;
//
//                auto adj = guideTable.adjacencyList[depth];
//
//                for (int i = 0; i < adj.size(); i++)
//                {
//                    if (!((CS::one() << adj[i].second) & cs)) {
//                        // check if the current left word combined with the existing words on 
//                        // the right will produce word that is not included in the target
//                        if ((CS::one() << adj[i].first & pair.right))
//                            return std::pair<bool, Pair<CS>>(false, mask);
//                        else
//                            next.right |= CS::one() << adj[i].first;
//                    }
//                }
//            }
//
//            return std::pair<bool, Pair<CS>>(true, next);
//        },
//        [&guideTable, &pair, &result](Pair<CS> mask) {
//
//            const auto combined = pair.right | mask.right;
//
//            std::vector<int> bits;
//            for (int i = 0; i < guideTable.ICsize; i++)
//            {
//                auto cs = (CS::one() << i);
//                if (!(cs & combined))
//                    bits.push_back(i);
//            }
//
//            const size_t numBits = bits.size();
//            const size_t numCombinations = 1ull << numBits;
//
//            for (size_t subset = 0; subset < numCombinations; ++subset)
//            {
//                CS combination = pair.right;
//
//                for (size_t bit = 0; bit < numBits; ++bit)
//                {
//                    if (subset & (1ull << bit))
//                    {
//                        combination |= (CS::one() << bits[bit]);
//                    }
//                }
//
//                if (mask.left != CS::one() && combination != CS::one())
//                    result.push_back({ mask.left, combination });
//            }
//        }
//    );
//}
//
//std::vector<Pair<CS>> rei::revertConcat(const CS& cs, const GuideTable& guideTable)
//{
//    std::vector<Pair<CS>> primary;
//
//    revertConcatPrimary(cs, guideTable, primary);
//
//    auto primary_size = primary.size();
//
//    std::vector<Pair<CS>> result;
//
//    for (int i = 0; i < primary_size; i++)
//    {
//        revertConcatSecondary(primary[i], cs, guideTable, result);
//    }
//
//    return result;
//}