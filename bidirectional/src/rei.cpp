#include "rei.hpp"

#include <span>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <numeric>

#include <bottom_up.hpp>
#include <top_down.hpp>

#include <analytic_top_down_samplers.hpp>

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

Result RunBottomUp(const LanguageSystem& languageSystem, const InputParams& inputParams, int cache_capacity) {

    Result result{};

    BottomUpSearch bottomUp(languageSystem, inputParams, cache_capacity);

    EnumerationState enumState;
    do {
        enumState = bottomUp.enumerateCostLevel(result);
    } while (enumState == EnumerationState::NotFound);

    return result;
}

Result RunBidirectional(const LanguageSystem& languageSystem, const InputParams& inputParams, int topDownsamples = 16) {

    Result result{};

    // Bottom-Up
    int buCacheCapacity = 2000000;
    int btCost = 0;
    BottomUpSearch bottomUp(languageSystem, inputParams, buCacheCapacity);

    // Top-Down
    int maxLevel = 50;
    int tdCacheCapacity = buCacheCapacity + 2000000; // Bottom-Up will insert it's cache into Top-Down
    rei::SamplingLimits limits;
    //limits.SetAll(topDownsamples);
    limits.SetBasedOnSolutionSet(tdCacheCapacity, 64);
    //limits.SetBasedOnLevel(tdCacheCapacity, 64);
    TopDownSearch topDown(languageSystem, inputParams, maxLevel, tdCacheCapacity,
        std::make_unique<AnalyticSolutionSetSampler>(languageSystem, inputParams, 0),
        std::make_unique<BottomUpResolver>(bottomUp, languageSystem.getIC().size()), limits);

    // topDown.setSampler(Operation::Star, std::make_unique<StarSampler>(guideTable));
    topDown.setSampler(Operation::Concatenate, std::make_unique<ConcatAnalyticSampler>(languageSystem, 0));
    topDown.setSampler(Operation::Or, std::make_unique<OrAnalyticSampler>(languageSystem, 0));

    // adding the alphabets
    topDown.insertExternalCSs(bottomUp.getLastCostLevel(btCost), inputParams.costFunc.alphaCost(), result);

    // Search
    EnumerationState buEnumState = EnumerationState::NotFound;
    EnumerationState tdEnumState = EnumerationState::NotFound;
    bool tdTurn;

    while (true) {

        if (bottomUp.OnTheFly() && tdEnumState == EnumerationState::End)
            break;

        if (bottomUp.OnTheFly())
            tdTurn = true;
        else if (tdEnumState == EnumerationState::End)
            tdTurn = false;
        else
            tdTurn = topDown.estimateNextLevel() < bottomUp.estimateNextLevel();

        if (tdTurn)
            tdEnumState = topDown.enumerateLevel(result);
        else
            buEnumState = bottomUp.enumerateCostLevel(result);

        if (buEnumState == EnumerationState::Found || tdEnumState == EnumerationState::Found)
            break;

        // sync
        if (!tdTurn) {

            auto btCSs = bottomUp.getLastCostLevel(btCost);
            tdEnumState = topDown.insertExternalCSs(btCSs, btCost, result);

            if (tdEnumState == EnumerationState::Found)
            {
                tdTurn = true;
                break;
            }
        }
    }

    if (bottomUp.OnTheFly() && tdEnumState == EnumerationState::End)
    {
        //if (buEnumState != EnumerationState::End)
        //{
        //    buEnumState = bottomUp.enumerateCostLevel(buRes);
        //    if (buEnumState == EnumerationState::Found)
        //        return Result(buRes.RE, guideTable.ICsize, tdRes.allCS + buRes.allCS);
        //}
    }

    return result;
}

Result RunTopDown(const LanguageSystem& languageSystem, const InputParams& inputParams, int cache_capacity, int samples = 128) {

    Result result{};

    rei::SamplingLimits limits;
    limits.SetAll(samples);

    TopDownSearch topDown(languageSystem, inputParams, 50, cache_capacity,
        std::make_unique<SolutionSetSampler>(languageSystem, inputParams),
        std::make_unique<AlphabetResolver>(languageSystem), limits);

    std::vector<int> alphaIds(languageSystem.getAlphabetSize() + 1);
    std::iota(alphaIds.begin(), alphaIds.end(), 0);
    topDown.insertExternalCSs(alphaIds, inputParams.costFunc.alphaCost(), result);

    EnumerationState enumState;
    do {
        enumState = topDown.enumerateLevel(result);
    } while (enumState == EnumerationState::NotFound);

    return result;
}

rei::Result rei::Run(SearchType searchType, const InputParams& inputParams) {

    LanguageSystem languageSystem(inputParams.pos, inputParams.neg);

    Solution entry;
    if (rei::intialCheck(languageSystem, inputParams.pos, entry))
    {
        Result result;
        result.push_back(entry);
        if (inputParams.n == result.entries.size())
            return result;
    }

    switch (searchType) {
    case SearchType::BottomUp:
        return RunBottomUp(languageSystem, inputParams, 2000000);
    case SearchType::TopDown:
        return RunTopDown(languageSystem, inputParams, 2000000);
    case SearchType::Bidirectional:
        return RunBidirectional(languageSystem, inputParams, 1024 * 2);
    }
}