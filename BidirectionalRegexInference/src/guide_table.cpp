#include <guide_table.hpp>

using namespace rei;

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

bool generatingGuideTable(GuideTable* guideTable, const std::set<std::string, strComparison>& ic)
{
    int alphabetSize = -1;
    for (auto& word : ic) {
        if (word.size() > 1) break;
        alphabetSize++;
    }

    std::vector<std::vector<int>> gt;

    for (auto& word : ic) {
        std::vector<int> row;
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

            row.push_back(index1);
            row.push_back(index2);
        }

        row.push_back(0);
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

bool rei::generatingGuideTable(GuideTable& guideTable, CS& posBits, CS& negBits,
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


rei::GuideTable::Iterator::Iterator(const int* p) : ptr(p) {}

rei::Pair<int> rei::GuideTable::Iterator::operator*() const { return Pair<int>(*ptr, *(ptr + 1)); }
rei::GuideTable::Iterator& rei::GuideTable::Iterator::operator++() { ptr += 2; return *this; }
bool rei::GuideTable::Iterator::operator!=(const Iterator& solved) const { return *ptr; }

rei::GuideTable::RowIterator::RowIterator(const int* data, int gtColumns, int rowIndex) : row(rowIndex), data(data), gtColumns(gtColumns) {}
rei::GuideTable::Iterator rei::GuideTable::RowIterator::begin() { return Iterator(data + row * gtColumns); }
rei::GuideTable::Iterator rei::GuideTable::RowIterator::end() { return Iterator(data + (row + 1) * gtColumns); }


rei::GuideTable::GuideTable(std::vector<std::vector<int>> gt, int alphabetSize) : alphabetSize(alphabetSize)
{
    ICsize = static_cast<int> (gt.size());
    gtColumns = static_cast<int> (gt.back().size());

    data = new int[ICsize * gtColumns];

    for (int i = 0; i < ICsize; ++i) {
        for (int j = 0; j < gt.at(i).size(); ++j) {
            data[i * gtColumns + j] = gt.at(i).at(j);
        }
    }

    // construct the adjacency list
    {
        std::vector<std::pair<int, int>> row;

        for (int i = 0; i < ICsize; i++)
            row.emplace_back(i, i);

        adjacencyList.push_back(row);
    }

    for (int i = 1; i < ICsize; i++)
    {
        std::vector<std::pair<int, int>> row;
        row.emplace_back(0, i);
        adjacencyList.push_back(row);
    }

    for (int i = 0; i < ICsize; ++i) {
        for (int j = 0; j < gt.at(i).size() - 1; j += 2) {
            auto left_index = gt.at(i).at(j);
            auto right_index = gt.at(i).at(j + 1);
            adjacencyList[left_index].emplace_back(right_index, i);
        }
    }
}

rei::GuideTable::GuideTable() : ICsize(0), gtColumns(0), alphabetSize(0), data(nullptr) {}

rei::GuideTable::~GuideTable() {
    if (data != nullptr) {
        delete[] data;
    }
}

rei::GuideTable::RowIterator rei::GuideTable::IterateRow(int rowIndex) const {
    return RowIterator(data, gtColumns, rowIndex);
}
