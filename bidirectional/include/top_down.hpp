#ifndef TOP_DOWN_HPP
#define TOP_DOWN_HPP

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <span>

#include <rei_common.hpp>
#include <top_down_samplers.hpp>

namespace rei {

    class AlphabetResolver : public CSResolverInterface
    {
    public:
        AlphabetResolver(const rei::LanguageSystem& langaugeSystem)
        {
            auto n = CS::getChuncksSize(langaugeSystem.getIC().size());

            alphaData = std::vector<uint64_t>((langaugeSystem.getAlphabetSize() + 1) * n);
            alphaBuffer = CSBuffer(alphaData.data(), langaugeSystem.getAlphabetSize() + 1, n, true);

            int idx = 0;
            chars.push_back("eps");
            alphaBuffer[0].clear().toggleBit(idx++);

            for (int i = 0; i < langaugeSystem.getAlphabetSize(); i++)
            {
                auto alphabet = langaugeSystem.getIC()[i + 1];
                alphaBuffer[idx].clear().toggleBit(idx++);
                chars.push_back(alphabet);
            }
        }

        std::string constructRE(const int id) override {
            return chars[id];
        }

        CS getCS(const int id) override {
            return alphaBuffer[id];
        }

    private:
        std::vector<std::string> chars;
        std::vector<uint64_t> alphaData;
        CSBuffer alphaBuffer;
    };

    struct SamplingLimits {

        int  solutionSetMaxSamples = 0;
        int  invertStarMaxSamples = 0;
        int  invertConcatMaxSamples = 0;
        int  invertOrMaxSamples = 0;

        void SetAll(int maxSamples) {
            solutionSetMaxSamples = maxSamples;
            invertStarMaxSamples = maxSamples;
            invertConcatMaxSamples = maxSamples;
            invertOrMaxSamples = maxSamples;
        }

        void SetBasedOnSolutionSet(int cacheLimit, int solutionSet, int ContactOrRatio = 4, int depth = 2) {
            solutionSetMaxSamples = solutionSet;
            int remain = cacheLimit / solutionSet;
            auto level = std::ceil(std::pow(remain, 1.0 / depth));
            level -= 2; // for ? and *
            invertStarMaxSamples = 1;
            level /= 2; // for | and . work with pairs
            invertOrMaxSamples = std::floor(level / (ContactOrRatio + 1));
            invertConcatMaxSamples = invertOrMaxSamples * ContactOrRatio;
        }

        void SetBasedOnLevel(int cacheLimit, int level, int ContactOrRatio = 4, int depth = 2) {
            solutionSetMaxSamples = std::floor(cacheLimit / std::pow(level,depth));
            level -= 2; // for ? and *
            invertStarMaxSamples = 1;
            level /= 2; // for | and . work with pairs
            invertOrMaxSamples = std::ceil(level / (ContactOrRatio + 1));
            invertConcatMaxSamples = invertOrMaxSamples * ContactOrRatio;
        }
    };

    class TopDownSearch
    {
        struct SearchResult {
            int solutionIdx = -1;
            int cost = INT_MAX;
        };

    public:

        TopDownSearch(const LanguageSystem& languageSystem, const InputParams& inputParams, int maxLevel, int cacheCapacity,
            std::unique_ptr<TopDownSamplerBase> solutionSetSampler, std::unique_ptr<CSResolverInterface> resolver, SamplingLimits samplingLimits);
        ~TopDownSearch();

        uint64_t estimateNextLevel() const;
        EnumerationState insertExternalCSs(const std::vector<int>& ids, int cost, Result& res);
        EnumerationState enumerateLevel(Result& res);

        void setSampler(Operation op, std::unique_ptr<TopDownSamplerBase> sampler);

    private:
        class Context;

        EnumerationState enumerateLevel(CSBuffer buffer, int pIdx, SearchResult& searchRes);
        EnumerationState enumerateLevel(int start, int end, SearchResult& searchRes);

        std::string bracket(std::string s);
        std::string constructDownward(int index);

        int level;
        int maxLevel;
        CSBuffer stageBuffer;

        const rei::LanguageSystem& languageSystem;
        std::unique_ptr<CSResolverInterface> resolver;

        LevelPartitioner partitioner;
        Context* context;

        SamplingLimits samplingLimits;

        std::unique_ptr<TopDownSamplerBase> solutionSetSampler;
        std::unique_ptr<TopDownSamplerBase> starSampler;
        std::unique_ptr<TopDownSamplerBase> concatSampler;
        std::unique_ptr<TopDownSamplerBase> orSampler;
    };
}

#endif // TOP_DOWN_HPP