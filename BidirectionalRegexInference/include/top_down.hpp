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
	};

    struct HeuristicConfigs {
    public:
        bool solutionSetUseRandomSampling = false;
        int  solutionSetMaxSamples = 0;

        bool invertStarUseRandomSampling = false;
        int  invertStarMaxSamples = 0;

        bool invertConcatUseRandomSampling = false;
        int  invertConcatMaxSamples = 0;

        bool invertOrUseRandomSampling = false;
        int  invertOrMaxSamples = 0;

        void EnableRandomSamplingForAll(int maxSamples) {
            solutionSetUseRandomSampling = true;
            invertStarUseRandomSampling = true;
            invertConcatUseRandomSampling = true;
            invertOrUseRandomSampling = true;

            solutionSetMaxSamples = maxSamples;
            invertStarMaxSamples = maxSamples;
            invertConcatMaxSamples = maxSamples;
            invertOrMaxSamples = maxSamples;
        }
    };


    class TopDownSearch
    {
        class Context {

            enum class NodeType
            {
                NotVistied = 0,
                Cyclic = 1,
                Vistied = 2,
                SelfSolved = 3, // Solved by this graph
                Given = 4,
            };

            struct SolvedNode {
                int leftIdx;
                CS cs;

                SolvedNode() : cs(CS()), leftIdx(0) {}
                SolvedNode(const CS& cs, int leftIdx) : cs(cs), leftIdx(leftIdx) {}
                SolvedNode(const CS& cs) : cs(cs), leftIdx(0) {}

                SolvedNode(const SolvedNode& sn) : cs(sn.cs), leftIdx(sn.leftIdx) {}
                SolvedNode(SolvedNode&& sn) noexcept : cs(std::move(sn.cs)), leftIdx(std::move(sn.leftIdx)) {}

                SolvedNode& operator=(const SolvedNode& other) {
                    if (this != &other) {
                        cs = other.cs;
                        leftIdx = other.leftIdx;
                    }
                    return *this;
                }

                SolvedNode& operator=(SolvedNode&& other) noexcept {
                    if (this != &other) {
                        cs = std::move(other.cs);
                        leftIdx = std::move(other.leftIdx);
                    }
                    return *this;
                }
            };

            struct Counter {

                uint64_t solved;
                uint64_t notVisited;
                uint64_t visited;
                uint64_t cyclic;
                uint64_t selfSolved;
                uint64_t given;

                void update(const NodeType& nt) {
                    switch (nt) {
                    case NodeType::NotVistied:
                        notVisited++;
                        break;
                    case NodeType::Cyclic:
                        cyclic++;
                        break;
                    case NodeType::Vistied:
                        visited++;
                        break;
                    case NodeType::SelfSolved:
                        selfSolved++;
                        break;
                    case NodeType::Given:
                        given++;
                        break;
                    }
                }
            };

        public:

            Context(int cache_capacity);

            ~Context();

            void AddSolutionSet(const std::vector<CS>& solutionSet);

            bool AddSolvedNode(const CS& cs, int& solutionIdx);

            bool InsertAndCheck(int parentIdx, CS left, CS right, int& solutionIdx);

            bool InsertAndCheck(int parentIdx, CS child, int& solutionIdx);

            int GetLastOutmostParent(int solutionIndex);

            CS* cache;
            int* nextVisited; 
            std::unordered_map<int, SolvedNode> solved;
            // Index of the last free position in the language cache
            int lastIdx;
            Counter counter;
            uint64_t allCS;

        private:
            NodeType getNodeType(const CS& cs);

            void insert(NodeType nodeType, CS cs, int pIdx);

            bool isSolved(int idx);

            bool checkSibling(int originalIdx, std::vector<int>& solvedIdx, int& solutionIdx);

            bool checkVisited(int originalIdx, std::vector<int>& solvedIdx, int& solutionIdx);

            bool recursiveCheck(int index, int lcIdx, std::vector<int>& solvedIdx );

            int getOutmostParent(int index);

            int* parentIdx;

            std::unordered_map<CS, int> visited;
        };

    public:

        TopDownSearch(const GuideTable& guideTable,
            std::shared_ptr<CSResolverInterface> resolver, int maxLevel, const CS& posBits, const CS& negBits, int cache_capacity);
        bool Push(const CS& cs, TopDownSearchResult& res);

        EnumerationState EnumerateLevel(TopDownSearchResult& res);

        uint64_t EstimateNextLevelCS();

        void SetHeuristic(HeuristicConfigs heuristicConfigs);

    private:

        std::vector<CS> generateSolutionSet();

        std::vector<CS> randomSampleSolutionSet(size_t maxSamples, uint64_t seed = std::random_device{}());

        EnumerationState enumerateLevel(const std::span<CS>& CSs, int startPIdx, int& idx, bool overrideParent = false, int opIdx = 0);

        std::string bracket(std::string s);

        std::string constructDownward(int index);

        int level = 0;
        int maxLevel;

        int cache_capacity;
        const CS& posBits;
        const CS& negBits;
        const rei::GuideTable& guideTable;
        std::shared_ptr<CSResolverInterface> resolver;

        LevelPartitioner partitioner;
        Context context;

        HeuristicConfigs heuristicConfigs;
    };
}

#endif // TOP_DOWN_HPP