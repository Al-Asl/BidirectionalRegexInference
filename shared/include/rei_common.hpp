#ifndef REI_COMMON_HPP
#define REI_COMMON_HPP

#include <operations.h>
#include <level_partitioner.hpp>
#include <chrono>

namespace rei {

    enum class EnumerationState {
        Found,
        NotFound,
        End
    };

    struct CostFunc
    {
        unsigned short alphaCost() const { return costs[0]; }
        unsigned short operationCost(Operation op) const { return costs[static_cast<int>(op) + 1]; }
        std::vector<unsigned short> costs;
    };

    struct InputParams {
        int n;
        CostFunc costFunc;
        unsigned short maxCost;
        std::vector<std::string> pos, neg;

        void print() const;
    };

#ifdef  CS_DECOMPOSETION
    struct CSDecomposetion {
        CSDecomposetion(std::vector<std::pair<rei::Operation, std::vector<uint64_t>>> data);
        void print();

        std::vector<std::pair<rei::Operation, std::vector<uint64_t>>> data;
    };
#endif

    struct Solution
    {
        std::string     RE = "";
        uint64_t        uniqueCSs;
        uint64_t        allCSs;
        double          duration;
#ifdef  CS_DECOMPOSETION
        CSDecomposetion decomposetion = CSDecomposetion({});
#endif
    };

    struct Result
    {
        Result() {
            start = std::chrono::high_resolution_clock::now();
        }

        std::vector<Solution> entries;
        std::string message;

        inline double duration() const {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        }

        inline void push_back(Solution& entry) {
            entry.duration = duration();
            entries.push_back(entry);
        }

    private:
        std::chrono::steady_clock::time_point start;
    };

    class CSResolverInterface {
    public:
        virtual ~CSResolverInterface() = default;

        virtual std::string constructRE(const int id) = 0;
        virtual CS getCS(const int id) = 0;
    };

    class TopDownSamplerBase {
    public:
        TopDownSamplerBase(const LanguageSystem& languageSystem) : languageSystem(languageSystem) {}
        TopDownSamplerBase(TopDownSamplerBase&& other) noexcept : languageSystem(other.languageSystem) {}
        virtual ~TopDownSamplerBase() = default;

        virtual void sample(CSBuffer& buffer, const CS& cs) = 0;
    protected:
        const LanguageSystem& languageSystem;
    };

    std::string to_string(const CS& cs);

    std::string to_string(const LanguageSystem& language_system, const CS& cs);

    bool intialCheck(const LanguageSystem& language_system, const std::vector<std::string>& pos, Solution& result);

    std::vector<uint64_t> posNegCSData(const LanguageSystem& language_system, const InputParams& input_params);
}

#endif // REI_COMMON_HPP