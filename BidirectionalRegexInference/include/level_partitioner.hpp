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
        int intervalSize(int level, Operation start = Operation::Question, Operation end = Operation::Or) const;

        int& start(int level, Operation op);
        int start(int level, Operation op) const;

        int& end(int level, Operation op);
        int end(int level, Operation op) const;

        void fillTo(int level, int value);

        std::pair<int,Operation> indexToLevel(int index) const;

    private:

        int* startPoints;
        unsigned short opCount;
        unsigned short maxCost;
    };
}

#endif // LEVEL_PARTITIONER_H