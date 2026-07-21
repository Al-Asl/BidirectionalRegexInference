#ifndef BOTTOM_UP_HPP
#define BOTTOM_UP_HPP

#include <unordered_set>

#include <rei_common.hpp>

namespace rei {

    class BottomUpSearch
    {

    public:
        BottomUpSearch(const LanguageSystem& languageSystem, const InputParams& inputParams, int cache_capacity);
        ~BottomUpSearch();

        EnumerationState enumerateCostLevel(Result& res);
        uint64_t estimateNextLevel();

        bool OnTheFly() const;
        std::string constructRE(int idx) const;

        std::vector<int> getLastCostLevel(int& cost);
        const CS getCS(int id) const;

    private:
        class Context;

        // Adding parentheses if needed
        std::string bracket(std::string s) const;
        // Generating the final RE string recursively
        std::string constructDownward(int index) const;

        EnumerationState enumerateLevel(rei::Result& result);
        void addSolution(rei::Result& result);

        unsigned short costLevel;
        unsigned short shortageCost = -1;
        bool lastRound;

        const InputParams& inputParams;
        const const LanguageSystem& languageSystem;
        Context* context;

        LevelPartitioner partitioner;
    };
}

#endif // BOTTOM_UP_HPP