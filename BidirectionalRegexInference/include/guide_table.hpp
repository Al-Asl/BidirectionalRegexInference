#ifndef GUIDE_TABLE_H
#define GUIDE_TABLE_H

#include <set>
#include <string>
#include <vector>
#include <types.h>

namespace rei
{
    class GuideTable {
    public:

        class Iterator {
        public:
            Iterator(const int* p);

            Pair<int> operator*() const;
            Iterator& operator++();
            bool operator!=(const Iterator& solved) const;
        private:
            const int* ptr;
        };

        class RowIterator {
        public:
            RowIterator(const int* data, int gtColumns, int rowIndex);
            Iterator begin();
            Iterator end();
        private:
            const int* data;
            int row;
            int gtColumns;
        };

        RowIterator IterateRow(int rowIndex) const;

        GuideTable(std::vector<std::vector<int>> gt, int alphabetSize);

        GuideTable();

        ~GuideTable();

        int ICsize;
        int gtColumns;
        int alphabetSize;
        // adjacencyList[i][j].first : i the index of the first bit while .first is the index
        // of the second bit and .second is the result of concatenation
        std::vector<std::vector<std::pair<int, int>>> adjacencyList;
    private:
        int* data;
    };

    bool generatingGuideTable(GuideTable& guideTable, CS& posBits, CS& negBits,
        const std::vector<std::string>& pos, const std::vector<std::string>& neg);

}

#endif // end GUIDE_TABLE_H