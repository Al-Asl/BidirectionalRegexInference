#include "language_system.hpp"

#include <unordered_map>
#include <set>

using namespace rei;

// Shortlex ordering
struct strComparison {
    bool operator () (const std::string& str1, const std::string& str2) const {
        if (str1.length() == str2.length()) return str1 < str2;
        return str1.length() < str2.length();
    }
};

// Generating the infix of a string
static std::set<std::string, strComparison> infixesOf(const std::string& word) {
    std::set<std::string, strComparison> ic;
    for (int len = 0; len <= word.length(); ++len) {
        for (int index = 0; index < word.length() - len + 1; ++index) {
            ic.insert(word.substr(index, len));
        }
    }
    return ic;
}

static std::set<std::string, strComparison> generatingIC(const std::vector<std::string>& pos, const std::vector<std::string>& neg) {
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

rei::LanguageSystem::LanguageSystem(const std::vector<std::string>& pos, const std::vector<std::string>& neg)
{
    std::set<std::string, strComparison> ic_set = generatingIC(pos, neg);
    ic = std::vector<std::string>(ic_set.begin(), ic_set.end());

    alphabetSize = -1;
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

    std::vector<std::vector<std::pair<uint16_t, uint16_t>>> gt;
    gt.reserve(ic.size());

    for (const auto& word : ic) {
        std::vector<std::pair<uint16_t, uint16_t>> row;
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

    guideTable = std::make_unique<PairsTable<uint16_t>>(gt, std::vector<std::pair<int,int>>{{0, 1}});

    auto left = std::vector<std::vector<std::pair<uint16_t, uint16_t>>>();
    left.reserve(ic.size());
    for (int i = 0; i < ic.size(); i++) { left.emplace_back(); }

    for (int i = 0; i < ic.size(); ++i) {
        for (int j = 0; j < gt.at(i).size(); j++) {

            auto [leftIdx, rightIdx] = gt.at(i).at(j);
            left[leftIdx].emplace_back(rightIdx, i);
        }
    }

    suffixes = std::make_unique<PairsTable<uint16_t>>(left, std::vector<std::pair<int, int>>{{0, ic.size()}});

    auto right = std::vector < std::vector<std::pair<uint16_t, uint16_t>>>();
    right.reserve(ic.size());
    for (int i = 0; i < ic.size(); i++) { right.emplace_back(); }

    for (int i = 0; i < ic.size(); ++i) {
        for (int j = 0; j < gt.at(i).size(); j++) {

            auto [leftIdx, rightIdx] = gt.at(i).at(j);
            right[rightIdx].emplace_back(leftIdx, i);
        }
    }

    prefixes = std::make_unique<PairsTable<uint16_t>>(right, std::vector<std::pair<int, int>>{{0, ic.size()}});
}

const PairsTable<uint16_t>& rei::LanguageSystem::getGuideTable() const
{
    return *guideTable.get();
}

const PairsTable<uint16_t>& rei::LanguageSystem::getSuffixes() const
{
    return *suffixes.get();
}

const PairsTable<uint16_t>& rei::LanguageSystem::getPrefixes() const
{
    return *prefixes.get();
}

const std::vector<std::string>& rei::LanguageSystem::getIC() const
{
    return ic;
}

const int rei::LanguageSystem::getAlphabetSize() const
{
    return alphabetSize;
}
