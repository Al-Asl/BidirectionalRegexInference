#ifndef LANGUAGE_SYSTEM_HPP
#define LANGUAGE_SYSTEM_HPP

#include <string>
#include <vector>
#include <memory>
#include <pair.h>
#include <config.h>

namespace rei
{
    template<typename T>
    class PairIterator {
    public:
        HD PairIterator(const T* ptr) : ptr(ptr) {}

        HD Pair<T> operator*() const { return Pair<T>(*ptr, *(ptr + 1)); }
        HD PairIterator& operator++() { ptr += 2; return *this; }
        HD bool operator!=(const PairIterator& solved) const { return *ptr; }
    private:
        const T* ptr;
    };

    template<typename T>
    class PairRange {
    public:
        HD PairRange(const T* ptr, int rowSize, int rowIdx) : ptr(ptr), rowSize(rowSize), rowIdx(rowIdx) {}
        HD PairIterator<T> begin() { return PairIterator<T>(ptr + rowIdx * rowSize); }
        HD PairIterator<T> end() { return PairIterator<T>(ptr + rowIdx * rowSize); } // end is not important, stop when the value is zero
    private:
        const T* ptr;
        int rowIdx;
        int rowSize;
    };

    template<typename T>
    class PairsTable {
    public:
        PairsTable(std::vector<std::vector<std::pair<T, T>>> table, std::vector<std::pair<int,int>> rowSizeOverrides) {
            auto maxRow = (*std::max_element(table.begin(), table.end(), [](const auto& a, const auto& b) { return a.size() < b.size(); })).size();
            columns = (2 * maxRow + 1);
            rows = table.size();

            data = new T[table.size() * columns];
            std::fill(data, data + table.size() * columns, 0);

            for (int i = 0; i < table.size(); ++i) {
                for (int j = 0; j < table.at(i).size(); ++j) {
                    data[i * columns + j * 2] = table.at(i).at(j).first;
                    data[i * columns + j * 2 + 1] = table.at(i).at(j).second;
                }
            }

            rowSizes = new int[table.size()];
            for (int i = 0; i < static_cast<int>(table.size()); i++)
                rowSizes[i] = static_cast<int>(table.at(i).size());

            for (auto [idx, value] : rowSizeOverrides)
                rowSizes[idx] = value;

        }
        ~PairsTable() {
            delete[] data;
            delete[] rowSizes;
        }
        PairRange<T> iterate(int rowIdx) const {
            return PairRange<T>(data, columns, rowIdx);
        }
        int getRowSize(int idx) const {
            return rowSizes[idx];
        }
        Pair<int> getSize() const {
            return {columns, rows};
        }
        const T* getData() const {
            return data;
        }
    private:
        T* data;
        int* rowSizes;
        int columns;
        int rows;
    };

    class LanguageSystem{
    public:

        LanguageSystem(const std::vector<std::string>& pos, const std::vector<std::string>& neg);
        ~LanguageSystem() { }

        const PairsTable<uint16_t>& getGuideTable() const;
        const PairsTable<uint16_t>& getSuffixes() const;
        const PairsTable<uint16_t>& getPrefixes() const;
        const std::vector < std::string>& getIC() const;
        const int getAlphabetSize() const;

    private:
        int alphabetSize;
        std::vector<std::string> ic;
        std::unique_ptr<PairsTable<uint16_t>> guideTable;
        std::unique_ptr<PairsTable<uint16_t>> suffixes;
        std::unique_ptr<PairsTable<uint16_t>> prefixes;
    };
}

#endif // LANGUAGE_SYSTEM_HPP