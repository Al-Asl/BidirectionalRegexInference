#ifndef GUIDE_TABLE_H
#define GUIDE_TABLE_H

#include <set>
#include <string>
#include <vector>
#include <types.h>

namespace rei
{

    // Shortlex ordering
    struct strComparison {
        bool operator () (const std::string& str1, const std::string& str2) const {
            if (str1.length() == str2.length()) return str1 < str2;
            return str1.length() < str2.length();
        }
    };

    // Generating the infix of a string
    std::set<std::string, strComparison> infixesOf(const std::string& word) {
        std::set<std::string, strComparison> ic;
        for (int len = 0; len <= word.length(); ++len) {
            for (int index = 0; index < word.length() - len + 1; ++index) {
                ic.insert(word.substr(index, len));
            }
        }
        return ic;
    }

    std::set<std::string, strComparison> generatingIC(const std::vector<std::string>& pos, const std::vector<std::string>& neg) {
        // Generating infix-closure (ic) of the input strings
        std::set<std::string, strComparison> ic = {};

        for (const std::string& word : pos) {
            std::set<std::string, strComparison> set1 = infixesOf(word);
            ic.insert(set1.begin(), set1.end());
        }
        for (const std::string& word : neg) {
            std::set<std::string, strComparison> set1 = infixesOf(word);
            ic.insert(set1.begin(), set1.end());
        }
        return ic;
    }

    class GuideTable {
    public:

        class Iterator {
            const CS* ptr;
        public:
            Iterator(const CS* p) : ptr(p) {}

            Pair<CS> operator*() const { return Pair<CS>(*ptr, *(ptr + 1)); }
            Iterator& operator++() { ptr += 2; return *this; }
            bool operator!=(const Iterator& solved) const { return *ptr; }
        };

        class RowIterator {
        public:
            RowIterator(const CS* data, int gtColumns, int rowIndex) : row(rowIndex), data(data), gtColumns(gtColumns) {}
            Iterator begin() {
                return Iterator(data + row * gtColumns);
            }
            Iterator end() {
                return Iterator(data + (row + 1) * gtColumns);
            }
        private:
            const CS* data;
            int row;
            int gtColumns;
        };

        GuideTable(std::vector<std::vector<CS>> gt, int alphabetSize) : alphabetSize(alphabetSize)
        {
            ICsize = static_cast<int> (gt.size());
            gtColumns = static_cast<int> (gt.back().size());

            data = new CS[ICsize * gtColumns];

            for (int i = 0; i < ICsize; ++i) {
                for (int j = 0; j < gt.at(i).size(); ++j) {
                    data[i * gtColumns + j] = gt.at(i).at(j);
                }
            }
        }

        GuideTable() : ICsize(0), gtColumns(0), alphabetSize(0), data(nullptr) {}

        ~GuideTable() {
            if (data != nullptr) {
                delete[] data;
            }
        }

        RowIterator IterateRow(int rowIndex) const {
            return RowIterator(data, gtColumns, rowIndex);
        }

        int ICsize;
        int gtColumns;
        int alphabetSize;

    private:
        CS* data;
    };

    bool generatingGuideTable(GuideTable* guideTable, const std::set<std::string, strComparison>& ic)
    {
        int alphabetSize = -1;
        for (auto& word : ic) {
            if (word.size() > 1) break;
            alphabetSize++;
        }

        std::vector<std::vector<CS>> gt;

        for (auto& word : ic) {
            std::vector<CS> row;
            for (int i = 1; i < word.length(); ++i) {

                int index1 = 0;
                for (auto& w : ic) {
                    if (w == word.substr(0, i)) break;
                    index1++;
                }
                int index2 = 0;
                for (auto& w : ic) {
                    if (w == word.substr(i)) break;
                    index2++;
                }

                row.push_back(CS::one() << index1);
                row.push_back(CS::one() << index2);
            }

            row.push_back(CS());
            gt.push_back(row);
        }

        if (gt.size() > sizeof(CS) * 8) {
            printf("Your input needs %llu bits which exceeds %llu bits ", gt.size(), sizeof(CS) * 8);
            printf("(current version).\nPlease use less/shorter words and run the code again.\n");
            return false;
        }

        new (guideTable) GuideTable(gt, alphabetSize);
        return true;
    }

    // Generating of the guide table only once for the whole enumeration process
    bool generatingGuideTable(GuideTable& guideTable, CS& posBits, CS& negBits,
        const std::vector<std::string>& pos, const std::vector<std::string>& neg) {

        std::set<std::string, strComparison> ic = generatingIC(pos, neg);

        if (!generatingGuideTable(&guideTable, ic))
            return false;

        for (auto& p : pos) {
            int wordIndex = distance(ic.begin(), ic.find(p));
            posBits |= (CS::one() << wordIndex);
        }

        for (auto& n : neg) {
            int wordIndex = distance(ic.begin(), ic.find(n));
            negBits |= (CS::one() << wordIndex);
        }

        return true;
    }

}

#endif // end GUIDE_TABLE_H