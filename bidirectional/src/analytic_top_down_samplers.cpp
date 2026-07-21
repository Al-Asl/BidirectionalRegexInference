#include "analytic_top_down_samplers.hpp"

#include <unordered_set>
#include <numbers>
#include <cs_utils.h>

#include<generated_stats.hpp>

// CS is a bitmask representing a language, every bit refer to a word in the infix-clousre IC

static double gaussian(double x, double mean, double stddev) {
    double exponent = -std::pow(x - mean, 2) / (2 * stddev * stddev);
    return (1.0 / (stddev * std::sqrt(2.0 * std::numbers::pi))) * std::exp(exponent);
}

static void sampleUniqueWeighted(CS cs, std::vector<int> bits, std::vector<double> weights, int k, std::mt19937 rng)
{
    k = std::min(k, (int)bits.size());

    for (int s = 0; s < k; ++s) {

        std::discrete_distribution<> dist(weights.begin(), weights.end());

        int idx = dist(rng);

        cs.setBitOn(bits[idx]);

        bits.erase(bits.begin() + idx);
        weights.erase(weights.begin() + idx);
    }
}

// ======================================================================

void rei::AnalyticSolutionSetSampler::sample(CSBuffer& buffer, const CS& cs) {

    auto ICSize = languageSystem.getIC().size();
    double maxWordLength = languageSystem.getIC().back().size();

    std::mt19937 rng(seed);

    // A candidate root (member of solution set) must include all words from 
    // the positive set and exclude all words from the negative set. 
    // This leaves room for words that belong to neither set to be
    // selected as additional words (extraWordsIdx).
    CS combined = buffer.append().clear();
    combined |= posBits;
    combined |= negBits;
    std::vector<int> extraWordsIdx;
    for (int i = 0; i < ICSize; ++i)
    {
        if (!combined.getBit(i))
            extraWordsIdx.push_back(i);
    }
    buffer.removeLast();

    const size_t numDontCareBits = extraWordsIdx.size();

    // If the total number of combinations is less than or equal to the requested
    // sample size, return the entire candidate pool by iterating over all
    // combinations sequentially
    if ((1ULL << numDontCareBits) <= buffer.size())
    {
        basicSampler->sample(buffer, cs);
        return;
    }

    // building a distribution to sample from extra words
    std::vector<double> weights;

    for (int i = 0; i < extraWordsIdx.size(); i++) {
        double weight = gaussian(languageSystem.getIC()[extraWordsIdx[i]].size() / maxWordLength,
            SOLUTION_SET_MEAN_WORD_LENGTH_PCT, SOLUTION_SET_STDDEV_WORD_LENGTH_PCT);
        weights.push_back(weight);
    }

    std::discrete_distribution<> weightsSampler(weights.begin(), weights.end());

    // building a distribution to sample the number of extra words
    std::normal_distribution<double> wordsCountSampler(SOLUTION_SET_MEAN_EXTRA_WORDS_PCT, SOLUTION_SET_STDDEV_EXTRA_WORDS_PCT);

    std::unordered_set<uint64_t> visited;

    while (!buffer.isFull()) {

        // we start from the postive set, then we add sampled words from extra words
        CS submask = buffer.append().copy(posBits);

        sampleUniqueWeighted(submask, extraWordsIdx, weights, std::lround(ICSize * wordsCountSampler(rng)), rng);

        if (!visited.insert(submask.getHash()).second)
            buffer.removeLast();
    }
}

// ======================================================================

void rei::OrAnalyticSampler::sample(CSBuffer& buffer, const CS& cs) {

    auto ICSize = languageSystem.getIC().size();
    double maxWordLength = languageSystem.getIC().back().size();

    const auto popCount = cs.popCount();

    // If the total number of combinations is less than or equal to the requested
    // sample size, return the entire candidate pool by iterating over all
    // combinations sequentially
    if ((1UL << (popCount - 1)) <= buffer.size() / 2)
    {
        basicSampler->sample(buffer, cs);
        return;
    }

    std::vector<int> bits = cs.getBits(ICSize);
    std::vector<double> weights;

    for (int i = 0; i < bits.size(); i++) {
        double weight = gaussian(languageSystem.getIC()[bits[i]].size() / maxWordLength,
            UNION_LEFT_MEAN_WORD_LENGTH_PCT, UNION_LEFT_STDDEV_WORD_LENGTH_PCT);
        weights.push_back(weight);
    }

    CS submask = buffer.append();
    CS complement = buffer.append();

    std::mt19937 rng(seed);
    std::unordered_set<uint64_t> visited;

    visited.insert(submask.clear().getHash()); // exclude empty set
    visited.insert(submask.toggleBit(0).getHash()); // exclude eps

    std::normal_distribution<double> wrodsCountSampler(UNION_LEFT_MEAN_WORDS_PCT, UNION_LEFT_STDDEV_WORDS_PCT);

    int iterations = 0;

    while (!buffer.isFull()) {

        auto wordsCount = std::lround(wrodsCountSampler(rng) * ICSize);
        sampleUniqueWeighted(submask.clear(), bits, weights, wordsCount < 1 ? 1 : wordsCount, rng);

        // we are producing pairs, and the right element is just the compment of the left
        complement.copy(cs);
        complement ^= submask;

        if (visited.insert(submask.getHash()).second && visited.insert(complement.getHash()).second)
        {
            if (buffer.isFull()) return;

            submask = buffer.append();
            complement = buffer.append();
        }

        if (++iterations > buffer.size() * 10)
            break;
    }

    buffer.removeLast();
    buffer.removeLast();
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

void rei::ConcatAnalyticSampler::sample(CSBuffer& buffer, const CS& cs) {

    auto ICSize = languageSystem.getIC().size();
    double maxWordSplit;
    {
        auto [columns, rows] = languageSystem.getGuideTable().getSize();
        maxWordSplit = (columns - 1) / 2;
    }

    double wordRepeatWeight = 1.0;

    auto bits = cs.getBits(ICSize);

    if (bits[0] == 0) // guide table don't contain the splits for epslion
        bits.erase(bits.begin());

    std::vector<std::discrete_distribution<>> dist;

    // build a distribution for every word in the target language, of it's splits
    for (int i = 0; i < bits.size(); i++) {

        int splits = languageSystem.getGuideTable().getRowSize(bits[i])/2;
        splits += 2; // to include: word.(eps) and (eps).word
        std::vector<float> weights(splits);

        for (int j = 0; j < splits; j++)
        {
            auto pair = pick_split(languageSystem.getGuideTable(), bits[i], j);
            int leftSplits = languageSystem.getGuideTable().getRowSize(pair.left) / 2 + 2;
            weights[j] = gaussian(leftSplits/ maxWordSplit, CONCAT_LEFT_MEAN_WORD_SPLITS_PCT, CONCAT_LEFT_STDDEV_WORD_SPLITS_PCT);
        }

        dist.emplace_back(weights.begin(), weights.end());
    }

    std::mt19937 gen(seed);
    std::unordered_set<uint64_t> visited;
    auto const maxSamples = (buffer.size() / 2 /* we are dealing with pairs */) * 16;

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
            auto pair = pick_split(languageSystem.getGuideTable(), bits[i], std::lround(dist[i](gen)));

            if (!canUnionTo(languageSystem, cs, Pair<CS>(left, right), pair))
            {
                accept = false; break;
            }

            left.setBitOn(pair.left);
            right.setBitOn(pair.right);
        }

        if (accept) {

            // reject if any language that only contain epslion
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