#include <rei_common.hpp>

std::set<char> rei::findAlphabets(const std::vector<std::string>& pos, const std::vector<std::string>& neg) {
    std::set<char> alphabet;
    for (auto& word : pos) for (auto ch : word) alphabet.insert(ch);
    for (auto& word : neg) for (auto ch : word) alphabet.insert(ch);
    return alphabet;
}

bool rei::intialCheck(std::set<char> alphabets, const std::vector<std::string>& pos, std::string& RE)
{
    // Checking empty & eps
    if (pos.empty()) { RE = "Empty"; return true; }
    if ((pos.size() == 1) && (pos.at(0).empty())) { RE = "eps"; return true; }

    // Checking the alphabets
    for (auto alphabet : alphabets) {
        auto s = std::string(1, alphabet);
        if ((pos.size() == 1) && (pos.at(0) == s)) { RE = s; return true; }
    }

    return false;
}