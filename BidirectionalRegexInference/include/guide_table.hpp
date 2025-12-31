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
            Iterator(const CS* p);

            Pair<CS> operator*() const;
            Iterator& operator++();
            bool operator!=(const Iterator& solved) const;
        private:
            const CS* ptr;
        };

        class RowIterator {
        public:
            RowIterator(const CS* data, int gtColumns, int rowIndex);
            Iterator begin();
            Iterator end();
        private:
            const CS* data;
            int row;
            int gtColumns;
        };

        GuideTable(std::vector<std::vector<CS>> gt, int alphabetSize);

        GuideTable();

        ~GuideTable();

        RowIterator IterateRow(int rowIndex) const;

        int ICsize;
        int gtColumns;
        int alphabetSize;
    private:
        CS* data;
    };

    bool generatingGuideTable(GuideTable& guideTable, CS& posBits, CS& negBits,
        const std::vector<std::string>& pos, const std::vector<std::string>& neg);

}

#endif // end GUIDE_TABLE_H