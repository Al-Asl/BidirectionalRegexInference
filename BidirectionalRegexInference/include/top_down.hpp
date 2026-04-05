#ifndef TOP_DOWN_HPP
#define TOP_DOWN_HPP

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <span>

#include <rei_common.hpp>

namespace rei {

    struct TopDownSearchResult {
        std::string RE;
        uint64_t allCS;
        uint64_t storedCS;
    };

    class AlphabetResolver : public CSResolverInterface
{
public:
    AlphabetResolver(std::set<char> alphabets, int ICSize)
    {
        auto n = CS::getChuncksSize(ICSize);

        alphaData = std::vector<uint64_t>((alphabets.size() + 1) * n);
        alphaBuffer = CSBuffer(alphaData.data(), alphabets.size() + 1, n, true);

        int idx = 0;
        chars.push_back("eps");
        alphaBuffer[0].clear().toggleBit(idx++);

        for (auto it = alphabets.begin(); it != alphabets.end(); it++) {
            alphaBuffer[idx].clear().toggleBit(idx++);
            chars.push_back(std::string(1, *it));
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
    };

    class TopDownSamplerBase {
    public:
        TopDownSamplerBase(const GuideTable& guideTable) : guideTable(guideTable) {}
        TopDownSamplerBase(TopDownSamplerBase&& other) noexcept : guideTable(other.guideTable) {}
        virtual ~TopDownSamplerBase() = default;

        virtual void sample(CSBuffer& buffer, const CS& cs) = 0;
    protected:
        const GuideTable& guideTable;
    };

    void sampleSolutionSetRandom(const GuideTable& guideTable, const CS posBits, const CS negBits, CSBuffer& buffer, uint64_t seed);

    void sampleSolutionSet(const GuideTable& guideTable, const CS posBits, const CS negBits, CSBuffer& buffer);

    class SolutionSetSampler : public TopDownSamplerBase
    {
    public:
        SolutionSetSampler(const GuideTable& guideTable, const CS posBits, const CS negBits) :
            TopDownSamplerBase(guideTable), posBits(posBits), negBits(negBits) {
        }

        void sample(CSBuffer& buffer, const CS& cs) override {
            sampleSolutionSet(guideTable, posBits, negBits, buffer);
        }

    private:
        const CS posBits, negBits;
    };

    class SolutionSetRandomSampler : public TopDownSamplerBase
    {
    public:
        SolutionSetRandomSampler(const GuideTable& guideTable, const CS posBits, const CS negBits, uint64_t seed = std::random_device{}()) :
            TopDownSamplerBase(guideTable), posBits(posBits), negBits(negBits), seed(seed) {
        }

        void sample(CSBuffer& buffer, const CS& cs) override {
            sampleSolutionSetRandom(guideTable, posBits, negBits, buffer, seed);
        }

    private:
        uint64_t seed;
        const CS posBits, negBits;
    };

    class StarSampler : public TopDownSamplerBase
    {
    public:
        StarSampler(const GuideTable& gt) : TopDownSamplerBase(gt) {}
        StarSampler(StarSampler&& other) noexcept : TopDownSamplerBase(std::move(other)) {}

        void sample(CSBuffer& buffer, const CS& cs) override {
            rei::revertStar(guideTable, cs, buffer);
        }
    };

    class StarRandomSampler : public TopDownSamplerBase
    {
    public:
        StarRandomSampler(const GuideTable& gt, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(gt), seed(seed) {}
        StarRandomSampler(StarRandomSampler&& other) noexcept : TopDownSamplerBase(std::move(other)), seed(other.seed) {}

        void sample(CSBuffer& buffer, const CS& cs) override {
            rei::revertStarRandom(guideTable, cs, buffer, seed);
        }
    private:
        uint64_t seed;
    };

    class ConcatSampler : public TopDownSamplerBase
    {
    public:
        ConcatSampler(const GuideTable& gt) : TopDownSamplerBase(gt) {}
        ConcatSampler(ConcatSampler&& other) noexcept : TopDownSamplerBase(std::move(other)) {}

        void sample(CSBuffer& buffer, const CS& cs) override {
            rei::revertConcat(guideTable, cs, buffer);
        }
    };

    class ConcatRandomSampler : public TopDownSamplerBase
    {
    public:
        ConcatRandomSampler(const GuideTable& gt, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(gt), seed(seed) {}
        ConcatRandomSampler(ConcatRandomSampler&& other) noexcept : TopDownSamplerBase(std::move(other)), seed(other.seed) {}

        void sample(CSBuffer& buffer, const CS& cs) override {
            rei::revertConcatRandom(guideTable, cs, buffer, seed);
        }
    private:
        uint64_t seed;
    };

    class OrSampler : public TopDownSamplerBase
    {
    public:
        OrSampler(const GuideTable& gt) : TopDownSamplerBase(gt) {}
        OrSampler(OrSampler&& other) noexcept : TopDownSamplerBase(std::move(other)) {}

        void sample(CSBuffer& buffer, const CS& cs) override {
            rei::revertOr(cs, buffer);
        }
    };

    class OrRandomSampler : public TopDownSamplerBase
    {
    public:
        OrRandomSampler(const GuideTable& gt, uint64_t seed = std::random_device{}()) : TopDownSamplerBase(gt), seed(seed) {}
        OrRandomSampler(OrRandomSampler&& other) noexcept : TopDownSamplerBase(std::move(other)), seed(other.seed) {}

        void sample(CSBuffer& buffer, const CS& cs) override {
            rei::revertOrRandom(cs, buffer, seed);
        }
    private:
        uint64_t seed;
    };

    class TopDownSearch
    {
        struct SearchResult {
            int solutionIdx = -1;
            int cost = INT_MAX;
        };

        class Context {

        public:

            enum class InsertAction
            {
                RejectRUC = 0,
                RejectCyclic = 1,
                InsertNotVistied = 2,
                InsertVistied = 3,
                InsertVistiedSolved = 4,
                InsertGiven = 5,
                InsertSentinel = 6
            };

            enum class NodeType {
                UnSolved = 0,
                Solved = 1,
                Given = 2,
                Sentinel = 3
            };

            struct Counter {
                uint64_t rejectedRUC = 0;
                uint64_t rejectedC = 0;
                uint64_t insertVisited = 0;
                uint64_t insertNotVisited = 0;
                uint64_t insertGiven = 0;
                uint64_t solved = 0;

                void update(InsertAction action) {
                    switch (action) {
                    case InsertAction::RejectRUC:
                        rejectedRUC++;
                        return;
                    case InsertAction::RejectCyclic:
                        rejectedC++;
                        return;
                    case InsertAction::InsertVistied:
                        insertVisited++;
                        return;
                    case InsertAction::InsertNotVistied:
                        insertNotVisited++;
                        return;
                    case InsertAction::InsertGiven:
                        insertGiven++;
                        return;
                    }
                }
            };

            struct UniqueNodes {

                UniqueNodes(int cacheCapacity, int chuncksPerBitmask) :
                    cache(new uint64_t[cacheCapacity * chuncksPerBitmask], cacheCapacity, chuncksPerBitmask),
                    param1(new int[cacheCapacity]), param2(new int[cacheCapacity]) {
                }

                ~UniqueNodes() {
                    delete[] cache.data();
                    delete[] param1;
                    delete[] param2;
                }

                bool isFull() const { return cache.isFull(); }

                int append(const CS& cs) {
                    cache.append().copy(cs);
                    return cache.count() - 1;
                }

                int append() {
                    cache.append().clear();
                    return cache.count() - 1;
                }

                const CS getCS(int uidx) const { return cache[uidx]; }
                CS getCS(int uidx) { return cache[uidx]; }

                NodeType getNodeType(int uidx) const {
                    if (param2[uidx] == INT_MIN)
                        return NodeType::Sentinel;
                    if (param1[uidx] < 0)
                        return NodeType::Given;
                    else if (param2[uidx] == INT_MAX)
                        return NodeType::UnSolved;
                    else
                        return NodeType::Solved;
                }

                void setUnSolved(int uidx, int idx) { param1[uidx] = idx; param2[uidx] = INT_MAX; };
                void seSolved(int uidx, int idx, int cost) { param1[uidx] = idx; param2[uidx] = cost; };
                void setGiven(int uidx, int givenIdx, int cost) { param1[uidx] = -cost; param2[uidx] = givenIdx; };
                void setSentinel(int uidx) { param1[uidx] = 0; param2[uidx] = INT_MIN; };

                int getOriginalIdx(int uidx) const { return param1[uidx]; }
                int getExternalIdx(int uidx) const { return param2[uidx]; }

                int getCost(int uidx) const {
                    if (param2[uidx] == INT_MIN)
                        return 0;
                    if (param1[uidx] < 0)
                        return -param1[uidx];
                    else
                        return param2[uidx];
                }

                int count() const { return cache.count(); }

            private:
                CSBuffer cache;
                int* param1;
                int* param2;
            };

            Context(int cacheCapacity, int chuncksPerBitmask, const Costs& costs);
            ~Context();

            bool includeExternalCSs(const std::vector<int>& ids, int cost, SearchResult& searchRes);

            bool insertAndCheck(int parentIdx, CS left, CS right, SearchResult& searchRes);
            bool insertAndCheck(int parentIdx, CS child, SearchResult& searchRes);

            const CS getCS(int idx) const;
            bool canTraverse(int idx) const;
            int getGivenCSId(int idx) const;

            std::unordered_map<int, int> leftIdx;
            int* nextVisited;
            int* parentIdx;
            int* uIdx;
            UniqueNodes uniqueNodes;
            std::unordered_map<uint64_t, int> visited;

            int lastIdx;
            uint64_t allCS;
            Counter counter;

            CSResolverInterface* resolver;
            LevelPartitioner* partitioner;
            const Costs& costs;

        private:
            InsertAction getInsertAction(const CS& cs) const;
            bool insert(InsertAction insertAction, CS cs, int pIdx);

            void recursiveCheck(int index, SearchResult& searchRes);
        };

    public:

        TopDownSearch(const GuideTable& guideTable, int maxLevel, int cacheCapacity, std::unique_ptr<TopDownSamplerBase> solutionSetSampler,
            const Costs& costs, std::unique_ptr<CSResolverInterface> resolver, SamplingLimits samplingLimits);
        ~TopDownSearch();

        EnumerationState includeExternalCSs(const std::vector<int>& ids, int cost, TopDownSearchResult& res);
        uint64_t estimateNextLevel() const;

        void setSampler(Operation op, std::unique_ptr<TopDownSamplerBase> sampler);
        EnumerationState enumerateLevel(TopDownSearchResult& res);

    private:

        EnumerationState enumerateLevel(CSBuffer buffer, int pIdx, SearchResult& searchRes);
        EnumerationState enumerateLevel(int start, int end, SearchResult& searchRes);

        std::string bracket(std::string s);
        std::string constructDownward(int index);

        int level;
        int maxLevel;
        CSBuffer stageBuffer;

        const rei::GuideTable& guideTable;
        std::unique_ptr<CSResolverInterface> resolver;

        LevelPartitioner partitioner;
        Context context;

        SamplingLimits samplingLimits;

        std::unique_ptr<TopDownSamplerBase> solutionSetSampler;
        std::unique_ptr<TopDownSamplerBase> starSampler;
        std::unique_ptr<TopDownSamplerBase> concatSampler;
        std::unique_ptr<TopDownSamplerBase> orSampler;
    };
}

#endif // TOP_DOWN_HPP