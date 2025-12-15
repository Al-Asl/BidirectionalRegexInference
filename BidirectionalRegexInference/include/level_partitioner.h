#ifndef LEVEL_PARTITIONER_H
#define LEVEL_PARTITIONER_H

#include <tuple>
#include <operations.h>

namespace rei {

    class LevelPartitioner {
    public:
        LevelPartitioner(const unsigned short maxCost) {
            opCount = static_cast<int>(Operation::Count);
            startPoints = new int[(maxCost + 2) * opCount]();
        }
        ~LevelPartitioner() {
            delete[] startPoints;
        }
        std::tuple<int, int> Interval(int level, Operation start = Operation::Question, Operation end = Operation::Or) const {
            return std::make_tuple(this->start(level, start), this->end(level, end));
        }
        int& start(int level, Operation op) {
            return startPoints[level * opCount + static_cast<int>(op)];
        }
        int start(int level, Operation op) const {
            return startPoints[level * opCount + static_cast<int>(op)];
        }
        int& end(int level, Operation op) {
            return startPoints[level * opCount + static_cast<int>(op) + 1];
        }
        int end(int level, Operation op) const {
            return startPoints[level * opCount + static_cast<int>(op) + 1];
        }
        void indexToLevel(int index, int& level, Operation& op) const {
            int i = 0;
            while (index >= startPoints[i]) { i++; }
            i--;
            level = i / opCount;
            op = static_cast<Operation>(i % opCount);
        }
    private:
        int* startPoints;
        int opCount;
    };
}

#endif // end LEVEL_PARTITIONER_H