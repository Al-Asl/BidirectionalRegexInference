#include "rei.hpp"

#include <span>
#include <queue>
#include <unordered_set>
#include <unordered_map>

#include <bottom_up.hpp>
#include <top_down.hpp>

using namespace rei;

class AlphabetResolver : public CSResolverInterface
{
public:
    AlphabetResolver(std::set<char> alphabets)
    {
        map[CS::one()] = "eps";
        auto alphabetCS = CS::one() << 1;
        for (auto it = alphabets.begin(); it != alphabets.end(); it++) {
            auto c = *it;
            map[alphabetCS] = std::string(1, c);
            alphabetCS = alphabetCS << 1;
        }
    }

    std::string resolve(const CS& cs) override {
        return map.at(cs);
    }
private:
    std::unordered_map<CS, std::string> map;
};

class BottomUpResolver : public CSResolverInterface
{
public:
    BottomUpResolver(const BottomUpSearch& bottomUp) : bottomUp(bottomUp) { }
    std::string resolve(const CS& cs) override {
        return bottomUp.ConstructRE(cs);
    }
private:
    const BottomUpSearch& bottomUp;
};

Result RunBottomUp(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs, 
    const unsigned short maxCost, const CS& posBits, const CS& negBits, int cache_capacity) {

    BottomUpSearchResult buRes = {};

    BottomUpSearch bottomUp(guideTable, alphabets, costs, maxCost, posBits, negBits, cache_capacity);

    EnumerationState enumState;
    do {
        enumState = bottomUp.EnumerateCostLevel(buRes);
    } while (enumState == EnumerationState::NotFound);

    if (enumState == EnumerationState::Found)
        return Result(buRes.RE, guideTable.ICsize, buRes.allREs);
    else
        return Result("not_found", guideTable.ICsize, buRes.allREs);
}

Result RunTopDown(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs,
    const unsigned short maxLevel, const CS& posBits, const CS& negBits, int cache_capacity, int samples = 16) {

    TopDownSearchResult tdRes = {};

    TopDownSearch topDown(guideTable, std::make_shared<AlphabetResolver>(alphabets), maxLevel, posBits, negBits, cache_capacity);

    HeuristicConfigs heuristicConfigs;
    heuristicConfigs.EnableRandomSamplingForAll(samples);
    topDown.SetHeuristic(heuristicConfigs);

    topDown.Push(CS::one(), tdRes);
    for (int i = 0; i < alphabets.size(); i++)
        topDown.Push(CS::one() << (i + 1), tdRes);

    EnumerationState enumState;
    do {
        enumState = topDown.EnumerateLevel(tdRes);
    } while (enumState == EnumerationState::NotFound);

    if (enumState == EnumerationState::Found)
        return Result(tdRes.RE, guideTable.ICsize, tdRes.allCS);
    else
        return Result("not_found", guideTable.ICsize, tdRes.allCS);
}

Result RunBidirectional(const GuideTable& guideTable, const std::set<char>& alphabets, 
    const Costs& costs, const unsigned short maxCost, const CS& posBits, const CS& negBits, int topDownsamples = 16) {

    // Bottom-Up
    int buCacheCapacity = 2000000;
    int levels = 13;
    BottomUpSearchResult buRes = {};

    BottomUpSearch bottomUp(guideTable, alphabets, costs, maxCost, posBits, negBits, buCacheCapacity);

    // Top-Down
    int maxLevel = 50;
    int tdCacheCapacity = 8000000;
    TopDownSearchResult tdRes = {};

    TopDownSearch topDown(guideTable, std::make_shared<BottomUpResolver>(bottomUp), maxLevel, posBits, negBits, tdCacheCapacity);

    HeuristicConfigs heuristicConfigs;
    heuristicConfigs.EnableRandomSamplingForAll(topDownsamples);
    topDown.SetHeuristic(heuristicConfigs);

    topDown.Push(CS::one(), tdRes);
    for (int i = 0; i < alphabets.size(); i++)
        topDown.Push(CS::one() << (i + 1), tdRes);

    // Search
    EnumerationState enumState;
    int i = 0;
    do {
        enumState = bottomUp.EnumerateCostLevel(buRes);
        if (enumState != EnumerationState::NotFound) break;
        auto plevel = bottomUp.GetLastCostLevel();
        for (const auto& cs : plevel)
            topDown.Push(cs, tdRes);
    } while (++i < levels);

    if (enumState == EnumerationState::Found)
        return Result(buRes.RE, guideTable.ICsize, buRes.allREs);

    do {
        enumState = topDown.EnumerateLevel(tdRes);
    } while (enumState == EnumerationState::NotFound);

    if (enumState == EnumerationState::Found)
        return Result(tdRes.RE, guideTable.ICsize, tdRes.allCS + buRes.allREs);
    else
        return Result("not_found", guideTable.ICsize, tdRes.allCS + buRes.allREs);
}

rei::Result rei::Run(const unsigned short* costFun, const unsigned short maxCost,
    const std::vector<std::string>& pos, const std::vector<std::string>& neg, double maxTime) {

    std::string RE;

    GuideTable guideTable;
    CS posBits, negBits;

    if (!generatingGuideTable(guideTable, posBits, negBits, pos, neg))
        return Result("not_found", 0, 0);

    Costs costs(costFun);

    auto alphabets = findAlphabets(pos, neg);
    if(intialCheck(alphabets, pos, RE)) return Result(RE, guideTable.ICsize, alphabets.size() + 2);

    //return RunBottomUp(guideTable, alphabets, costs, maxCost, posBits, negBits, 20000000);

    //return RunTopDown(guideTable, alphabets, costs, 50, posBits, negBits, 20000000);

    return RunBidirectional(guideTable, alphabets, costs, maxCost, posBits, negBits, 64);
}