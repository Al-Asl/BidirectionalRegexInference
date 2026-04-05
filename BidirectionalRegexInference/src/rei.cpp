#include "rei.hpp"

#include <span>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <numeric>

#include <bottom_up.hpp>
#include <top_down.hpp>

using namespace rei;

class BottomUpResolver : public CSResolverInterface
{
public:
    BottomUpResolver(const BottomUpSearch& bottomUp, int ICSize) : bottomUp(bottomUp), epsData(CS::getChuncksSize(ICSize), 0) {}

    std::string constructRE(const int id) override {
        return bottomUp.constructRE(id);
    }

    CS getCS(const int id) override {
        if (id == -1) return CS(epsData.data(), epsData.size());
        return bottomUp.getCS(id);
    }

private:
    const BottomUpSearch& bottomUp;
    std::vector<uint64_t> epsData;
};

Result RunBottomUp(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs,
    const unsigned short maxCost, CS posBits, CS negBits, int cache_capacity) {

    BottomUpSearchResult buRes = {};

    BottomUpSearch bottomUp(guideTable, alphabets, costs, maxCost, posBits, negBits, cache_capacity);

    EnumerationState enumState;
    do {
        enumState = bottomUp.enumerateCostLevel(buRes);
    } while (enumState == EnumerationState::NotFound);

    if (enumState == EnumerationState::Found)
        return Result(buRes.RE, guideTable.ICsize, buRes.allCS);
    else
        return Result("not_found", guideTable.ICsize, buRes.allCS);
}

Result RunBidirectional(const GuideTable& guideTable, const std::set<char>& alphabets,
    const Costs& costs, const unsigned short maxCost, CS posBits, CS negBits, int topDownsamples = 16) {

    // Bottom-Up
    int buCacheCapacity = 2000000;
    int btCost = 0;
    BottomUpSearchResult buRes = {};

    BottomUpSearch bottomUp(guideTable, alphabets, costs, maxCost, posBits, negBits, buCacheCapacity);

    // Top-Down
    int maxLevel = 50;
    int tdCacheCapacity = 2000000;
    TopDownSearchResult tdRes = {};

    rei::SamplingLimits limits;
    limits.SetAll(topDownsamples);

    TopDownSearch topDown(guideTable, maxLevel, tdCacheCapacity,
        std::make_unique<SolutionSetSampler>(guideTable, posBits, negBits), costs,
        std::make_unique<BottomUpResolver>(bottomUp, guideTable.ICsize), limits);

    topDown.setSampler(Operation::Star, std::make_unique<StarRandomSampler>(guideTable));
    //topDown.setSampler(Operation::Concatenate, std::make_unique<ConcatRandomSampler>(guideTable));
    topDown.setSampler(Operation::Or, std::make_unique<OrRandomSampler>(guideTable));

    // adding the alphabets
    topDown.includeExternalCSs(bottomUp.getLastCostLevel(btCost), costs.alphaCost(), tdRes);

    // Search
    EnumerationState enumState;
    bool topDownLast;

    while (true) {
        topDownLast = topDown.estimateNextLevel() < bottomUp.estimateNextLevel();

        if (topDownLast)
            enumState = topDown.enumerateLevel(tdRes);
        else
            enumState = bottomUp.enumerateCostLevel(buRes);

        if (enumState != EnumerationState::NotFound)
            break;

        // sync
        if (!topDownLast) {

            auto btCSs = bottomUp.getLastCostLevel(btCost);
            enumState = topDown.includeExternalCSs(btCSs, btCost, tdRes);

            if (enumState != EnumerationState::NotFound)
            {
                topDownLast = true;
                break;
            }
        }
    }

    if (enumState == EnumerationState::End)
        return Result("not_found", guideTable.ICsize, tdRes.allCS + buRes.allCS);
    else
        return Result(topDownLast ? tdRes.RE : buRes.RE, guideTable.ICsize, tdRes.allCS + buRes.allCS);
}

Result RunTopDown(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs,
    const unsigned short maxLevel, CS posBits, CS negBits, int cache_capacity, int samples = 128) {

    TopDownSearchResult tdRes = {};

    rei::SamplingLimits limits;
    limits.SetAll(samples);

    TopDownSearch topDown(guideTable, maxLevel, cache_capacity, 
        std::make_unique<SolutionSetSampler>(guideTable, posBits, negBits), costs,
        std::make_unique<AlphabetResolver>(alphabets, guideTable.ICsize), limits);

    std::vector<int> alphaIds(alphabets.size() + 1);
    std::iota(alphaIds.begin(), alphaIds.end(), 0);
    topDown.includeExternalCSs(alphaIds, costs.alphaCost(), tdRes);

    EnumerationState enumState;
    do {
        enumState = topDown.enumerateLevel(tdRes);
    } while (enumState == EnumerationState::NotFound);

    if (enumState == EnumerationState::Found)
        return Result(tdRes.RE, guideTable.ICsize, tdRes.allCS);
    else
        return Result("not_found", guideTable.ICsize, tdRes.allCS);
}

rei::Result rei::Run(SearchType searchType, const unsigned short* costFun, const unsigned short maxCost,
    const std::vector<std::string>& pos, const std::vector<std::string>& neg, double maxTime) {

    std::string RE;

    GuideTable guideTable;
    std::vector<int> posBits, negBits;

    if (!generatingGuideTable(guideTable, posBits, negBits, pos, neg))
        return Result("not_found", 0, 0);

    Costs costs(costFun);

    auto alphabets = findAlphabets(pos, neg);
    if(intialCheck(alphabets, pos, RE)) return Result(RE, guideTable.ICsize, alphabets.size() + 2);

    auto chunksSize = CS::getChuncksSize(guideTable.ICsize);
    auto PN_data = std::vector<uint64_t>(chunksSize * 2, 0);

    auto posMask = CS(PN_data.data(), chunksSize);
    for (int i = 0; i < posBits.size(); i++)
        posMask.setBitOn(posBits[i]);

    auto negMask = CS(PN_data.data() + chunksSize, chunksSize);
    for (int i = 0; i < negBits.size(); i++)
        negMask.setBitOn(negBits[i]);

    switch (searchType) {
    case SearchType::BottomUp:
        return RunBottomUp(guideTable, alphabets, costs, maxCost, posMask, negMask, 2000000);
    case SearchType::TopDown:
        return RunTopDown(guideTable, alphabets, costs, 50, posMask, negMask, 2000000);
    case SearchType::Bidirectional:
        return RunBidirectional(guideTable, alphabets, costs, maxCost, posMask, negMask, 32);
    }
}