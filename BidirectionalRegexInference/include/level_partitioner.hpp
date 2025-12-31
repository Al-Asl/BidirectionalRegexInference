#ifndef LEVEL_PARTITIONER_H
#define LEVEL_PARTITIONER_H

#include <tuple>
#include <operations.h>

namespace rei {

    class LevelPartitioner {
    public:

        LevelPartitioner(const unsigned short maxCost);
        ~LevelPartitioner();

        std::tuple<int, int> Interval(int level, Operation start = Operation::Question, Operation end = Operation::Or) const;

        int& start(int level, Operation op);
        int start(int level, Operation op) const;

        int& end(int level, Operation op);
        int end(int level, Operation op) const;

        void indexToLevel(int index, int& level, Operation& op) const;

    private:

        int* startPoints;
        int opCount;
    };
}

#endif // LEVEL_PARTITIONER_H