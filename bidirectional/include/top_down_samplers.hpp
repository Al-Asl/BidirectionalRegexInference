#ifndef TOP_DOWN_SAMPLERS_HPP
#define TOP_DOWN_SAMPLERS_HPP

#include <rei_common.hpp>
#include <random>

namespace rei
{
    class SolutionSetSampler : public TopDownSamplerBase
    {
    public:
        SolutionSetSampler(const LanguageSystem& ls, const InputParams& inputParams) :
            TopDownSamplerBase(ls){

            auto chunksPerCS = CS::getChuncksSize(languageSystem.getIC().size());
            posNegData = rei::posNegCSData(languageSystem, inputParams);
            posBits = CS(posNegData.data(), chunksPerCS);
            negBits = CS(posNegData.data() + chunksPerCS, chunksPerCS);
        }

        void sample(CSBuffer& buffer, const CS& cs) override;

    private:
        std::vector<uint64_t> posNegData;
        CS posBits, negBits;
    };

    class SolutionSetRandomSampler : public TopDownSamplerBase
    {
    public:
        SolutionSetRandomSampler(const LanguageSystem& ls, const InputParams& inputParams, uint64_t seed = std::random_device{}()) :
            TopDownSamplerBase(ls), posBits(posBits), negBits(negBits), seed(seed) {
            auto chunksPerCS = CS::getChuncksSize(languageSystem.getIC().size());
            posNegData = rei::posNegCSData(languageSystem, inputParams);
            posBits = CS(posNegData.data(), chunksPerCS);
            negBits = CS(posNegData.data() + chunksPerCS, chunksPerCS);
        }

        void sample(CSBuffer& buffer, const CS& cs) override;

    private:
        uint64_t seed;
        std::vector<uint64_t> posNegData;
        CS posBits, negBits;
    };

    // ================================================

    class StarSampler : public TopDownSamplerBase
    {
    public:
        StarSampler(const LanguageSystem& ls) : TopDownSamplerBase(ls) {}
        StarSampler(StarSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem) {}

        void sample(CSBuffer& buffer, const CS& cs) override;
    };

    class StarRandomSampler : public TopDownSamplerBase
    {
    public:
        StarRandomSampler(const LanguageSystem& ls, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(ls), seed(seed) {}
        StarRandomSampler(StarRandomSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem), seed(other.seed) {}

        void sample(CSBuffer& buffer, const CS& cs) override;

    private:
        uint64_t seed;
    };

    // ================================================

    class ConcatSampler : public TopDownSamplerBase
    {
    public:
        ConcatSampler(const LanguageSystem& ls) : TopDownSamplerBase(ls) {}
        ConcatSampler(ConcatSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem) {}

        void sample(CSBuffer& buffer, const CS& cs) override;
    };

    class ConcatRandomSampler : public TopDownSamplerBase
    {
    public:
        bool enhanceByCount = false;

        ConcatRandomSampler(const LanguageSystem& ls, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(ls), seed(seed) {}
        ConcatRandomSampler(ConcatRandomSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem), seed(other.seed) {}

        void sample(CSBuffer& buffer, const CS& cs) override;
    private:
        uint64_t seed;
    };

    // ================================================

    class OrSampler : public TopDownSamplerBase
    {
    public:
        OrSampler(const LanguageSystem& ls) : TopDownSamplerBase(ls) {}
        OrSampler(OrSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem) {}

        void sample(CSBuffer& buffer, const CS& cs) override;
    };

    class OrRandomSampler : public TopDownSamplerBase
    {
    public:
        OrRandomSampler(const LanguageSystem& ls, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(ls), seed(seed) {}
        OrRandomSampler(OrRandomSampler&& other) noexcept : TopDownSamplerBase(other.languageSystem), seed(other.seed) {}

        void sample(CSBuffer& buffer, const CS& cs) override;
    private:
        uint64_t seed;
    };
}

#endif // TOP_DOWN_SAMPLERS_HPP