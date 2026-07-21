#ifndef ANALYTIC_TOP_DOWN_SAMPLERS_HPP
#define ANALYTIC_TOP_DOWN_SAMPLERS_HPP

#include <rei_common.hpp>
#include <top_down_samplers.hpp>

namespace rei
{
    class AnalyticSolutionSetSampler : public TopDownSamplerBase
    {
        SolutionSetSampler* basicSampler;

    public:
        AnalyticSolutionSetSampler(const LanguageSystem& ls, const InputParams& inputParams, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(ls), seed(seed)
        {
            auto chunksPerCS = CS::getChuncksSize(languageSystem.getIC().size());
            posNegData = rei::posNegCSData(languageSystem, inputParams);
            posBits = CS(posNegData.data(), chunksPerCS);
            negBits = CS(posNegData.data() + chunksPerCS, chunksPerCS);

            basicSampler = new SolutionSetSampler(ls, inputParams);
        }

        ~AnalyticSolutionSetSampler() {
            delete basicSampler;
        }

        void sample(CSBuffer& buffer, const CS& cs) override;

    private:
        uint64_t seed;
        std::vector<uint64_t> posNegData;
        CS posBits, negBits;
    };

    class ConcatAnalyticSampler : public TopDownSamplerBase
    {
    public:
        ConcatAnalyticSampler(const LanguageSystem& ls, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(ls), seed(seed) {}
        ConcatAnalyticSampler(ConcatAnalyticSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem), seed(other.seed) {}

        void sample(CSBuffer& buffer, const CS& cs) override;
    private:
        uint64_t seed;
    };

    class OrAnalyticSampler : public TopDownSamplerBase
    {
    public:
        OrSampler* basicSampler;

        OrAnalyticSampler(const LanguageSystem& ls, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(ls), seed(seed)
        {
            basicSampler = new OrSampler(ls);
        }

        OrAnalyticSampler(OrAnalyticSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem), seed(other.seed), basicSampler(std::move(other.basicSampler)) {}

        ~OrAnalyticSampler() {
            delete basicSampler;
        }

        void sample(CSBuffer& buffer, const CS& cs) override;
    private:
        uint64_t seed;
    };
}

#endif // TOP_DOWN_SAMPLERS_HPP