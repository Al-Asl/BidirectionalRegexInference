#ifndef BOTTOM_UP_HPP
#define BOTTOM_UP_HPP

#include <unordered_set>

#include <rei_common.hpp>

namespace rei {
    class BottomUpSearchResult
    {
    public:
        int cost;
        std::string RE;
        int allCS;
    };

    class BottomUpSearch
    {
        class Context
        {
        public:
            Context(int cache_capacity, int n, const CS& posBits, const CS& negBits);
            ~Context();

            bool insertAndCheck(CS CS, int index);
            bool insertAndCheck(CS CS, int lIndex, int rIndex);
            CSBuffer getCacheSlice(int start, int end);

            unsigned long allREs;
            bool onTheFly;

            int* leftRightIdx;
            CSBuffer cache;
            std::unordered_set<uint64_t> visited;

            const CS posBits, negBits;
        };

    public:
        BottomUpSearch(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs, 
            const unsigned short maxCost, const CS& posBits, const CS& negBits, int cache_capacity);

        EnumerationState enumerateCostLevel(BottomUpSearchResult& res);
        uint64_t estimateNextLevel();

        std::vector<int> getLastCostLevel(int& cost);
        std::string constructRE(int idx) const;
        const CS getCS(int id) const;

    private:

        // Adding parentheses if needed
        std::string bracket(std::string s) const;
        // Generating the final RE string recursively
        std::string constructDownward(int index) const;

        EnumerationState enumerateLevel(int& idx);

        unsigned short costLevel;
        unsigned short shortageCost;
        const unsigned short maxCost;
        bool lastRound;

        const Costs& costs;
        const rei::GuideTable& guideTable;
        const std::set<char>& alphabet;

        Context context;
        LevelPartitioner partitioner;
    };
}

#endif // BOTTOM_UP_HPP