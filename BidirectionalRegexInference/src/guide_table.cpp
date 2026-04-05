#include <guide_table.hpp>

#include<unordered_map>

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

    std::unordered_map<std::string, int> indexMap;
    indexMap.reserve(ic.size());
    int idx = 0;
    for (const auto& w : ic) {
        indexMap[w] = idx++;
    }

    std::vector<std::vector<std::pair<int, int>>> gt;
    gt.reserve(ic.size());

    for (const auto& word : ic) {
        std::vector<std::pair<int, int>> row;
        const int len = static_cast<int>(word.length());
        if (len > 1) row.reserve(len - 1);

        for (int i = 1; i < len; ++i) {
            row.emplace_back(
                indexMap.at(word.substr(0, i)),
                indexMap.at(word.substr(i))
            );
        }

        gt.push_back(std::move(row));
    }

    new (guideTable) GuideTable(gt, alphabetSize);
    return true;
}

bool rei::generatingGuideTable(GuideTable& guideTable, std::vector<int>& posBits, std::vector<int>& negBits,
    const std::vector<std::string>& pos, const std::vector<std::string>& neg) {

    std::set<std::string, strComparison> ic = generatingIC(pos, neg);

    if (!generatingGuideTable(&guideTable, ic))
        return false;

    for (auto& p : pos) {
        int wordIndex = distance(ic.begin(), ic.find(p));
        posBits.push_back(wordIndex);
    }

    for (auto& n : neg) {
        int wordIndex = distance(ic.begin(), ic.find(n));
        negBits.push_back(wordIndex);
    }

    return true;
}

uint16_t* to_array(std::vector<std::vector<std::pair<int, int>>> vv, int& rowSize) {

    auto maxRow = (*std::max_element(vv.begin(), vv.end(), [](const auto& a, const auto& b) { return a.size() < b.size(); })).size();
    rowSize = (2 * maxRow + 1);

    auto data = new uint16_t[vv.size() * rowSize];
    std::fill(data, data + vv.size() * rowSize, 0);

    for (int i = 0; i < vv.size(); ++i) {
        for (int j = 0; j < vv.at(i).size(); ++j) {
            data[i * rowSize + j * 2] = (uint16_t)vv.at(i).at(j).first;
            data[i * rowSize + j * 2 + 1] = (uint16_t)vv.at(i).at(j).second;
        }
    }

    return data;
}

rei::GuideTable::GuideTable(std::vector<std::vector<std::pair<int,int>>> gt, int alphabetSize) : alphabetSize(alphabetSize)
{
    ICsize = static_cast<int> (gt.size());

    splits = to_array(gt, splitsRowSize);

    splitsRowSizes = new int[ICsize];
    splitsRowSizes[0] = 1;
    for (int i = 1; i < ICsize; i++)
        splitsRowSizes[i] = gt.at(i).size();

    auto left = std::vector < std::vector<std::pair<int, int>>>();
    left.reserve(ICsize);
    for (int i = 0; i < ICsize; i++) { left.emplace_back(); }

    for (int i = 0; i < ICsize; ++i) {
        for (int j = 0; j < gt.at(i).size(); j ++) {

            auto [leftIdx, rightIdx] = gt.at(i).at(j);
            left[leftIdx].emplace_back(rightIdx, i);
        }
    }

    suffixes = to_array(left, suffixesRowSize);

    auto right = std::vector < std::vector<std::pair<int, int>>>();
    right.reserve(ICsize);
    for (int i = 0; i < ICsize; i++) { right.emplace_back(); }

    for (int i = 0; i < ICsize; ++i) {
        for (int j = 0; j < gt.at(i).size(); j++) {

            auto [leftIdx, rightIdx] = gt.at(i).at(j);
            right[rightIdx].emplace_back(leftIdx, i);
        }
    }

    prefixes = to_array(right, prefixesRowSize);
}

rei::GuideTable::GuideTable() : ICsize(0), splitsRowSize(0), alphabetSize(0), splits(nullptr) {}

rei::GuideTable::~GuideTable() {
    delete[] splits;
    delete[] splitsRowSizes;
    delete[] suffixes;
    delete[] prefixes;
}

int rei::GuideTable::getMaxSplits() const
{
    return (splitsRowSize - 1) / 2;
}

int rei::GuideTable::getSplitsCount(int rowIndex) const
{
    return splitsRowSizes[rowIndex];
}

rei::PairRange<uint16_t> rei::GuideTable::iterateSplits(int rowIndex) const {
    return PairRange<uint16_t>(splits, splitsRowSize, rowIndex);
}

rei::PairRange<uint16_t> rei::GuideTable::iterateSuffixes(int rowIndex) const
{
    return PairRange<uint16_t>(suffixes, suffixesRowSize, rowIndex);
}

rei::PairRange<uint16_t> rei::GuideTable::iteratePrefixes(int rowIndex) const
{
    return PairRange<uint16_t>(prefixes, prefixesRowSize, rowIndex);
}