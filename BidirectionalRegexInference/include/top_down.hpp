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

            bool AddSolvedNode(const CS& cs, int& idx);

            bool InsertAndCheck(int parentIdx, CS left, CS right);

            bool InsertAndCheck(int parentIdx, CS child);

            // This is just temporary
            bool CheckAllVisited(int& solvedIndex);

            int GetLastOutmostParent();

            CS* cache;
            // 0 = the original node, -1 = given, < -1 = redirectIdx, > 1 = leftIdx
            int* status; 
            std::unordered_map<int, CS> idxToSolved;
            // Index of the last free position in the language cache
            int lastIdx;
            Counter counter;
            uint64_t allCS;

        private:
            NodeType getNodeType(const CS& cs);

            void insert(NodeType nodeType, CS cs, int pIdx);

            bool isSolved(int idx);

            bool recursiveCheck(int index, int lcIdx);

            int getOutmostParent(int index);

            int* parentIdx;

            std::unordered_map<CS, int> visited;
            std::unordered_set<CS> solved;
        };

    public:

        TopDownSearch(const GuideTable& guideTable,
            std::shared_ptr<CSResolverInterface> resolver, int maxLevel, const CS& posBits, const CS& negBits, int cache_capacity);
        bool Push(const CS& cs, TopDownSearchResult& res);

        EnumerationState EnumerateLevel(TopDownSearchResult& res);

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