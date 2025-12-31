#ifndef BOTTOM_UP_HPP
#define BOTTOM_UP_HPP

#include <span>

#include <rei_common.hpp>

namespace rei {
    class BottomUpSearchResult
    {
    public:
        int cost;
        std::string RE;
        int allREs;
    };

    class BottomUpSearch
    {
        class Context
        {
        public:
            Context(int cache_capacity, const CS& posBits, const CS& negBits);

            ~Context();

            bool InsertAndCheck(CS CS, int index);

            bool InsertAndCheck(CS CS, int lIndex, int rIndex);

            std::span<CS> GetCacheSlice(int start, int end);

            int* leftRightIdx;
            unsigned long allREs;
            int lastIdx; // Index of the last free position in the language cache
            bool onTheFly;
            int cache_capacity;

            CS* cache;
            std::unordered_map<CS, int> visited;
            const CS& posBits, negBits;
        };

    public:
        BottomUpSearch(const GuideTable& guideTable, const std::set<char>& alphabets, const Costs& costs, const unsigned short maxCost, const CS& posBits, const CS& negBits, int cache_capacity);

        EnumerationState EnumerateCostLevel(BottomUpSearchResult& res);

        std::string ConstructRE(const CS& cs) const;

        std::span<CS> GetLastCostLevel() const;

    private:
        // Adding parentheses if needed
        std::string bracket(std::string s) const;

        // Generating the final RE string recursively
        std::string constructDownward(int index) const;

        EnumerationState enumerateLevel(int& idx);

        int costLevel;
        int shortageCost;
        bool lastRound;
        const unsigned short maxCost;

        const CS& posBits;
        const CS& negBits;
        const Costs& costs;
        const rei::GuideTable& guideTable;
        const std::set<char>& alphabet;

        Context context;
        LevelPartitioner partitioner;
    };
}

#endif // BOTTOM_UP_HPP