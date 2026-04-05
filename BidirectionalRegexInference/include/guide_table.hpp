#ifndef GUIDE_TABLE_H
#define GUIDE_TABLE_H

#include <set>
#include <string>
#include <vector>

#include<pair.h>

namespace rei
{
    template<typename T>
    class PairIterator {
    public:
        PairIterator(const T* ptr) : ptr(ptr) {}

        Pair<T> operator*() const { return Pair<T>(*ptr, *(ptr + 1)); }
        PairIterator& operator++() { ptr += 2; return *this; }
        bool operator!=(const PairIterator& solved) const { return *ptr; }
    private:
        const T* ptr;
    };

    template<typename T>
    class PairRange {
    public:
        PairRange(const T* ptr, int rowSize, int rowIdx) : ptr(ptr), rowSize(rowSize), rowIdx(rowIdx) {}
        PairIterator<T> begin() { return PairIterator<T>(ptr + rowIdx * rowSize); }
        PairIterator<T> end() { return PairIterator<T>(ptr + rowIdx * rowSize); } // end is not important, stop when the value is zero
    private:
        const T* ptr;
        int rowIdx;
        int rowSize;
    };

    class GuideTable {
    public:

        int getMaxSplits() const;
        int getSplitsCount(int idx) const;
        PairRange<uint16_t> iterateSplits(int idx) const;
        PairRange<uint16_t> iterateSuffixes(int idx) const;
        PairRange<uint16_t> iteratePrefixes(int idx) const;

        GuideTable(std::vector<std::vector<std::pair<int,int>>> gt, int alphabetSize);
        GuideTable();
        ~GuideTable();

        int ICsize;
        int alphabetSize;
    private:
        uint16_t* prefixes;
        int prefixesRowSize;

        uint16_t* suffixes;
        int suffixesRowSize;

        uint16_t* splits;
        int splitsRowSize;
        int* splitsRowSizes;
    };

    bool generatingGuideTable(GuideTable& guideTable, std::vector<int>& posBits, std::vector<int>& negBits,
        const std::vector<std::string>& pos, const std::vector<std::string>& neg);
}

#endif // end GUIDE_TABLE_H