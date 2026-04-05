#include <level_partitioner.hpp>

#include <climits>

rei::LevelPartitioner::LevelPartitioner(const unsigned short maxCost) : maxCost(maxCost) {
    opCount = static_cast<int>(Operation::Count);
    startPoints = new int[(maxCost + 1) * opCount];
    std::fill(startPoints, startPoints + (maxCost + 1) * opCount, INT_MAX);
    startPoints[0] = 0;
}

rei::LevelPartitioner::~LevelPartitioner() {
    delete[] startPoints;
}

std::tuple<int, int> rei::LevelPartitioner::Interval(int level, Operation start, Operation end) const {
    return std::make_tuple(this->start(level, start), this->end(level, end));
}

int rei::LevelPartitioner::intervalSize(int level, Operation start, Operation end) const {
    return this->end(level, end) - this->start(level, start);
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

void rei::LevelPartitioner::fillTo(int level, int value)
{
    for (int i = 0; i < (level + 1) * opCount; i++)
        startPoints[i] = value;
}

int find_start_idx(int* startPoints, int size, int value) {
    int lo = 0, hi = size - 2;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;

        int rangeMin = startPoints[mid];
        int rangeMax = startPoints[mid + 1];

        if (rangeMin <= value && value < rangeMax) {
            return mid;
        }

        if (rangeMin == INT_MAX || rangeMin > value) {
            hi = mid - 1;
        }
        else {
            lo = mid + 1;
        }
    }

    return -1;
}

std::pair<int, rei::Operation> rei::LevelPartitioner::indexToLevel(int index) const {
    int i = find_start_idx(startPoints, (maxCost + 1) * opCount, index);
    int level = i / opCount;
    Operation op = static_cast<Operation>(i % opCount);
    return { level, op };
}