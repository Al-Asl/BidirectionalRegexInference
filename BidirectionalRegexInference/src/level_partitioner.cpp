#include <level_partitioner.hpp>

rei::LevelPartitioner::LevelPartitioner(const unsigned short maxCost) {
    opCount = static_cast<int>(Operation::Count);
    startPoints = new int[(maxCost + 1) * opCount]();
}

rei::LevelPartitioner::~LevelPartitioner() {
    delete[] startPoints;
}

std::tuple<int, int> rei::LevelPartitioner::Interval(int level, Operation start, Operation end) const {
    return std::make_tuple(this->start(level, start), this->end(level, end));
}

int& rei::LevelPartitioner::start(int level, Operation op) {
    return startPoints[level * opCount + static_cast<int>(op)];
}

int rei::LevelPartitioner::start(int level, Operation op) const {
    return startPoints[level * opCount + static_cast<int>(op)];
}

int& rei::LevelPartitioner::end(int level, Operation op) {
    return startPoints[level * opCount + static_cast<int>(op) + 1];
}

int rei::LevelPartitioner::end(int level, Operation op) const {
    return startPoints[level * opCount + static_cast<int>(op) + 1];
}

void rei::LevelPartitioner::indexToLevel(int index, int& level, Operation& op) const {
    int i = 0;
    while (index >= startPoints[i]) { i++; }
    i--;
    level = i / opCount;
    op = static_cast<Operation>(i % opCount);
}