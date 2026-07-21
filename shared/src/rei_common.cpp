#include "rei_common.hpp"

#include <string>
#include <iostream>

void rei::InputParams::print() const {
    printf("=== Input Params ===\n\n");
    printf("Max Cost: %hu\n", maxCost);
    printf("REs to find: %d\n", n);
    printf("\n  CostFunc:\n");
    printf("    Alpha Cost: %hu\n", costFunc.alphaCost());
    printf("    Operation Costs:\n");
    for (int i = 0; i < static_cast<int>(Operation::Count); ++i) {
        Operation op = static_cast<Operation>(i);
        printf("      %s: %hu\n", to_string(op).c_str(), costFunc.operationCost(op));
    }
    printf("\n  Positive Samples: [ ");
    for (const auto& s : pos) {
        printf("\"%s\" ", s.c_str());
    }
    printf("]\n");
    printf("  Negative Samples: [ ");
    for (const auto& s : neg) {
        printf("\"%s\" ", s.c_str());
    }
    printf("]\n");
}

#ifdef  CS_DECOMPOSETION
rei::CSDecomposetion::CSDecomposetion(std::vector<std::pair<rei::Operation, std::vector<uint64_t>>> data) : data(data)
{

}

void rei::CSDecomposetion::print()
{
    auto chunksPerCS = data[0].second.size() / 3;

    printf("Decomposetion:\n");

    for (size_t i = 0; i < data.size(); i++)
    {
        auto entry = data[i];
        printf("    %u - %s:\n", i + 1, to_string(entry.first).c_str());
        CS root = CS(entry.second.data(), chunksPerCS);
        CS left = CS(entry.second.data() + chunksPerCS, chunksPerCS);
        CS right = CS(entry.second.data() + 2 * chunksPerCS, chunksPerCS);
        printf("        root: %s\n", to_string(root).c_str());
        if (entry.first == rei::Operation::Or || entry.first == rei::Operation::Concatenate)
        {
            printf("        left:  %s\n", to_string(left).c_str());
            printf("        right: %s\n", to_string(right).c_str());
        }
        else
        {
            printf("        left:  %s\n", to_string(left).c_str());
        }
    }
}
#endif

std::string rei::to_string(const CS& cs)
{
    std::string s = "[";

    for (size_t i = 0; i < cs.getSize(); i++)
    {
        uint64_t chunk = cs.getChunck(i);

        s += std::to_string(chunk);

        if (i + 1 < cs.getSize()) {
            s += ", ";
        }
    }

    s += "]";

    return s;
}

std::string rei::to_string(const LanguageSystem& language_system, const CS& cs) {

    auto ic = language_system.getIC();
    bool first = true;

    std::string s = "[";

    for (int i = 0; i < ic.size(); i++)
    {
        if (cs.getBit(i))
        {
            if (first)
                s += language_system.getIC()[i];
            else
                s += std::string(", ") + language_system.getIC()[i];

            first = false;
        }
    }

    s += "]";

    return s;
}

bool rei::intialCheck(const rei::LanguageSystem& languageSystem, const std::vector<std::string>& pos, Solution& result)
{
    // Checking empty & eps
    result.allCSs++;
    result.uniqueCSs++;
    if (pos.empty()) { result.RE = "Empty"; return true; }

    result.allCSs++;
    result.uniqueCSs++;
    if ((pos.size() == 1) && (pos.at(0).empty())) { result.RE = "eps"; return true; }

    // Checking the alphabets
    for (int i = 0; i < languageSystem.getAlphabetSize(); i++)
    {
        result.allCSs++;
        result.uniqueCSs++;
        auto alphabet = languageSystem.getIC().at(i + 1);
        auto s = std::string(1, alphabet[0]);
        if ((pos.size() == 1) && (pos.at(0) == s)) { result.RE = s; return true; }
    }

    return false;
}

std::vector<uint64_t> rei::posNegCSData(const LanguageSystem& language_system, const InputParams& input_params)
{
    auto ic = language_system.getIC();
    int chunksPerCS = CS::getChuncksSize(ic.size());

    std::vector<uint64_t> data(chunksPerCS * 2, 0);
    CS pos(data.data(), chunksPerCS);
    CS neg(data.data() + chunksPerCS, chunksPerCS);

    for (size_t i = 0; i < input_params.pos.size(); i++)
    {
        auto it = std::find(ic.begin(), ic.end(), input_params.pos[i]);
        pos.setBitOn(std::distance(ic.begin(), it));
    }

    for (size_t i = 0; i < input_params.neg.size(); i++)
    {
        auto it = std::find(ic.begin(), ic.end(), input_params.neg[i]);
        neg.setBitOn(std::distance(ic.begin(), it));
    }

    return data;
}