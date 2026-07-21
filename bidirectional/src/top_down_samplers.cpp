#include "top_down_samplers.hpp"

#include <unordered_set>
#include <cs_utils.h>

//std::vector<unsigned int> shortlex_bitmasks(int n) {
//    std::vector<unsigned int> result;
//
//    for (int k = 0; k <= n; ++k) {
//        if (k == 0) {
//            result.push_back(0);
//            continue;
//        }
//
//        // mask with k bits set: (1<<k)-1
//        unsigned int x = (1u << k) - 1;
//
//        while (x < (1u << n)) {
//            result.push_back(x);
//
//            // Gosper's hack: next bitmask with same number of bits
//            unsigned int u = x & -x;
//            unsigned int v = x + u;
//            x = v + (((v ^ x) / u) >> 2);
//        }
//    }
//
//    return result;
//}

// ======================================================================

static void sampleSolutionSet(const GuideTable& guideTable, const CS posBits, const CS negBits, CSBuffer& buffer)
{
    auto [columns, rows] = guideTable.getSize();

    CS combined = buffer.append().copy(posBits);
    combined |= negBits;
    std::vector<int> dontCareBits;
    for (int i = 0; i < rows; ++i)
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

void rei::SolutionSetSampler::sample(CSBuffer& buffer, const CS& cs) {
    sampleSolutionSet(languageSystem.getGuideTable(), posBits, negBits, buffer);
}

void rei::SolutionSetRandomSampler::sample(CSBuffer& buffer, const CS& cs) {

    auto icSize = languageSystem.getIC().size();

    CS combined = buffer.append().copy(posBits);
    combined |= negBits;
    std::vector<int> dontCareBits;
    for (int i = 0; i < icSize; ++i)
    {
        if (!combined.getBit(i))
            dontCareBits.push_back(i);
    }
    buffer.removeLast();

    const size_t numDontCareBits = dontCareBits.size();

    if ((1ULL << numDontCareBits) <= buffer.size())
    {
        sampleSolutionSet(languageSystem.getGuideTable(), posBits, negBits, buffer);
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

// ======================================================================

static void powerset_element(int size, int index, std::function<void(int)> it) {
    for (int i = 0; i < size; i++) {
        if (index & (1 << i)) { it(i); }
    }
}

static void find_star_base(const GuideTable& guideTable, const CS cs, CS& baseCS) {

    auto [columns, rows] = guideTable.getSize();

    for (int i = 1; i < rows; i++)
    {
        if (!(cs.getBit(i)))
            continue;

        bool exists = false;

        for (auto const& pair : guideTable.iterate(i))
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

static void revertStar(const rei::LanguageSystem& languageSystem, CSBuffer& buffer, const CS& cs) {
    
    CS baseCS = buffer.append();
    find_star_base(languageSystem.getGuideTable(), cs, baseCS.clear());

    // Not all CS have an invert
    CS temp = buffer.append().copy(baseCS);
    processStar(languageSystem.getGuideTable(), languageSystem.getAlphabetSize(), temp);
    if (temp != cs)
    {
        buffer.removeLast();
        buffer.removeLast();
        return;
    }

    if (buffer.size() == 1)
        return;

    baseCS.copyTo(temp);
    temp ^= cs;
    auto bits = temp.getBits(languageSystem.getIC().size());
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

void rei::StarSampler::sample(CSBuffer& buffer, const CS& cs)
{
    revertStar(languageSystem, buffer, cs);
}

void rei::StarRandomSampler::sample(CSBuffer& buffer, const CS& cs)
{
    CS baseCS = buffer.append();
    find_star_base(languageSystem.getGuideTable(), cs, baseCS.clear());

    // Not all CS have an invert
    CS temp = buffer.append().copy(baseCS);
    processStar(languageSystem.getGuideTable(), languageSystem.getAlphabetSize(), temp);
    if (temp != cs)
    {
        buffer.removeLast();
        buffer.removeLast();
        return;
    }

    if (buffer.size() == 1)
        return;

    baseCS.copyTo(temp);
    temp ^= cs;
    auto bits = temp.getBits(languageSystem.getIC().size());
    buffer.removeLast();

    if ((1UL << bits.size()) - 1 <= buffer.size()) {
        revertStar(languageSystem, buffer, cs);
        return;
    }

    std::mt19937 rng(seed);
    std::bernoulli_distribution coin(0.5);

    std::unordered_set<uint64_t> visited;
    visited.insert(cs.getHash());

    for (int i = 1; i < buffer.size(); i++)
        buffer.append().copy(baseCS);

    for (int i = 0; i < buffer.size(); i++)
    {
        auto submask = buffer[i];
        rei::getRandom(bits, submask, rng, coin);

        if (!visited.insert(submask.getHash()).second)
            i--;
    }
}

//void rei::revertStarBrute(const GuideTable& guideTable, const CS& target, CSBuffer& buffer)
//{
//    const int n = guideTable.ICsize;
//    const std::uint64_t maxMask = 1ULL << n;
//
//    CS cs = buffer.append();
//
//    for (std::uint64_t mask = 0; mask < maxMask; ++mask)
//    {
//        cs.clear().setChunck(mask, 0);
//        rei::processStar(guideTable, cs);
//
//        if (cs == target)
//        {
//            cs.clear().setChunck(mask, 0);
//            if (buffer.isFull())
//                return;
//            else
//                cs = buffer.append();
//        }
//    }
//
//    buffer.removeLast();
//}

// ======================================================================

static void revertOr(const CS& cs, CSBuffer& buffer) {

    auto popCount = cs.popCount();
    popCount = popCount > 63 ? 63 : popCount;

    CS submask = buffer.append().copy(cs);
    CS complement = buffer.append().copy(cs);

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

        submask = buffer.append().copy(submask);
        complement = buffer.append().copy(cs);

        --submask;
        submask &= cs;
    }

    buffer.removeLast();
    buffer.removeLast();
}

void rei::OrSampler::sample(CSBuffer& buffer, const CS& cs) {
    revertOr(cs, buffer);
}

void rei::OrRandomSampler::sample(CSBuffer& buffer, const CS& cs) {

    const auto popCount = cs.popCount();

    if (popCount < 63 && (1UL << (popCount - 1)) <= buffer.size() / 2)
    {
        revertOr(cs, buffer);
        return;
    }

    std::vector<int> bits = cs.getBits(languageSystem.getIC().size());

    std::mt19937 rng(seed);
    std::bernoulli_distribution coin(0.5);

    std::unordered_set<uint64_t> visited;
    CS submask = buffer.append();
    CS complement = buffer.append();

    visited.insert(submask.clear().getHash()); // exclude empty set
    visited.insert(submask.toggleBit(0).getHash()); // exclude eps

    while (true) {

        getRandom(bits, submask.clear(), rng, coin);

        cs.copyTo(complement);
        complement ^= submask;

        if (visited.insert(submask.getHash()).second && visited.insert(complement.getHash()).second)
        {
            if (buffer.isFull()) return;

            submask = buffer.append();
            complement = buffer.append();
        }
    }
}

// ======================================================================

// return true if the pair a after union with the pair b will be concatenated
// to the cs r such that r is a subset of target
static bool canUnionTo(const rei::LanguageSystem& languageSystem, const CS& target, Pair<CS> a, Pair<uint16_t> b) {

    if (!a.left.getBit(b.left))
    {
        if (b.left == 0) {
            if (!target.containsAll(a.right))
                return false;
        }
        else {

            if (a.right.getBit(0) && !target.getBit(b.left))
                return false;

            for (auto [right, res] : languageSystem.getSuffixes().iterate(b.left))
            {
                if (right == b.right || !a.right.getBit(right))
                    continue;

                if (!target.getBit(res))
                    return false;
            }
        }
    }

    if (!a.right.getBit(b.right))
    {
        if (b.right == 0) {
            if (!target.containsAll(a.left))
                return false;
        }
        else {

            if (a.left.getBit(0) && !target.getBit(b.right))
                return false;

            for (auto [left, res] : languageSystem.getPrefixes().iterate(b.right))
            {
                if (left == b.left || !a.left.getBit(left))
                    continue;

                if (!target.getBit(res))
                    return false;
            }
        }
    }

    return true;
}

static bool canUnionTo(const rei::LanguageSystem& languageSystem, const CS& target, Pair<uint16_t> a, Pair<uint16_t> b) {

    if (a.left != b.left)
    {
        if (b.left == 0) {
            if (!target.getBit(a.right))
                return false;
        }
        else if (a.right == 0)
        {
            if (!target.getBit(b.left))
                return false;
        }
        else {
            for (auto [right, res] : languageSystem.getSuffixes().iterate(b.left))
            {
                if (a.right == right && !target.getBit(res))
                    return false;
            }
        }
    }

    if (a.right != b.right)
    {
        if (b.right == 0) {
            if (!target.getBit(a.left))
                return false;
        }
        else if (a.left == 0)
        {
            if (!target.getBit(b.right))
                return false;
        }
        else {
            for (auto [left, res] : languageSystem.getPrefixes().iterate(b.right))
            {
                if (a.left == left && !target.getBit(res))
                    return false;
            }
        }
    }

    return true;
}

static rei::Pair<uint16_t> pick_split(const GuideTable& guideTable, int rowIdx, int idx) {

    if (idx == 0)
        return Pair<uint16_t>(0, rowIdx);

    for (auto const& p : guideTable.iterate(rowIdx))
    {
        if (--idx == 0)
            return p;
    }

    return Pair<uint16_t>(rowIdx, 0);
}

static void revertConcatPrimary(const rei::LanguageSystem& languageSystem, const CS& cs, CSBuffer& buffer) {

    auto bits = cs.getBits(languageSystem.getIC().size());

    if (bits.empty())
        return;

    auto branchCount = [](const GuideTable& guideTable, int depth) {
        if (depth == 0)
            return 1;
        else
            return guideTable.getRowSize(depth) + 2;
    };

    std::vector<uint64_t> paramsData((bits.size() + 1) * 2 * cs.getSize(), 0);
    CSBuffer params(paramsData.data(), (bits.size() + 1) * 2, cs.getSize());

    const int maxDepth = bits.size();
    std::vector<int> idx(maxDepth, 0);
    int depth = 0;

    int counter = 0;

    while (true)
    {
        if (counter++ > 25000000)
            return;

        if (idx[depth] < branchCount(languageSystem.getGuideTable(), bits[depth]))
        {
            auto pair = pick_split(languageSystem.getGuideTable(), bits[depth], idx[depth]);

            if (canUnionTo(languageSystem, cs, Pair<CS>(params[depth * 2], params[depth * 2 + 1]), pair))
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
                        counter = 0;
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

void rei::ConcatSampler::sample(CSBuffer& buffer, const CS& cs) {
    revertConcatPrimary(languageSystem, cs, buffer);
}

void rei::ConcatRandomSampler::sample(CSBuffer& buffer, const CS& cs) {

    auto guideTable = languageSystem.getGuideTable();

    auto bits = cs.getBits(languageSystem.getIC().size());
    
    if (bits[0] == 0)
        bits.erase(bits.begin());

    std::vector<std::discrete_distribution<uint16_t>> counts;
    if (enhanceByCount)
    {
        int countsRowSize = 0;
        for (auto b : bits) {
            countsRowSize = std::max(countsRowSize, guideTable.getRowSize(b));
        }
        countsRowSize += 2;

        auto const countsSize = (bits.size() + 1) * countsRowSize;

        uint16_t* row = new uint16_t[countsRowSize];
        uint16_t* tempRow = new uint16_t[countsRowSize];

        for (uint16_t i = 0; i < bits.size(); i++)
        {
            std::fill(row, row + countsRowSize, 1);

            for (uint16_t j = i + 1; j < bits.size(); j++)
            {
                std::fill(tempRow, tempRow + countsRowSize, 0);

                for (uint16_t k = 0; k < guideTable.getRowSize(bits[i]) + 2; k++)
                {
                    auto pair = pick_split(guideTable, bits[i], k);
                    for (uint16_t w = 0; w < guideTable.getRowSize(bits[j]) + 2; w++) {
                        if (canUnionTo(languageSystem, cs, pair, pick_split(guideTable, bits[j], w)))
                            tempRow[k] += 1;
                    }
                }

                // prevent overflow
                uint16_t max = 0;
                for (uint16_t k = 0; k < guideTable.getRowSize(bits[i]) + 2; k++) {
                    if (row[k] > max)
                        max = row[k];
                }

                if (max == 0)
                    return;

                for (uint16_t k = 0; k < guideTable.getRowSize(bits[i]) + 2; k++) {
                    row[k] = (max > 255 ? row[k] >> 4 : row[k]) * tempRow[k];
                }
            }

            counts.emplace_back(row, row + countsRowSize);
        }

        delete[] tempRow;
        delete[] row;
    }

    std::mt19937 gen(seed);
    std::unordered_set<uint64_t> visited;
    auto const maxSamples = (buffer.size() / 2) * 16;

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

        for (uint16_t i = 0; i < bits.size(); i++)
        {
            int pairIdx;

            if (enhanceByCount)
                pairIdx = counts[i](gen);
            else
            {
                std::uniform_int_distribution<> dis(0, guideTable.getRowSize(bits[i]) + 1);
                pairIdx = dis(gen);
            }

            auto pair = pick_split(guideTable, bits[i], pairIdx);

            if (!canUnionTo(languageSystem, cs, Pair<CS>(left, right), pair))
            {
                accept = false; break;
            }

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
                        return;
                    else
                        continue;
                }
            }
        }

        buffer.removeLast();
        buffer.removeLast();
    }
}

//void revertConcatBrute(const rei::GuideTable& guideTable, const CS& cs, CS reg, CSBuffer& buffer, int& sampleCount)
//{
//    const int n = guideTable.ICsize;
//    const std::uint64_t maxMask = 1ULL << n;
//    const int maxSamples = buffer.size() / 2;
//    sampleCount = 0;
//
//    for (std::uint64_t lMask = 0; lMask < maxMask; ++lMask)
//    {
//        for (std::uint64_t rMask = 0; rMask < maxMask; ++rMask)
//        {
//            auto left = buffer[sampleCount * 2].setChunck(lMask, 0);
//            auto right = buffer[sampleCount * 2 + 1].setChunck(rMask, 0);
//
//            if (left.getBit(0) && !cs.intersects(right)) continue;
//            if (right.getBit(0) && !cs.intersects(left)) continue;
//
//            rei::processConcatenate(guideTable, left, right, reg);
//            if (reg == cs)
//            {
//                if (++sampleCount == maxSamples)
//                    return;
//            }
//        }
//    }
//}

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