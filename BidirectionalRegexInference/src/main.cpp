#include <iostream>
#include <vector>
#include <string>
#include <util.hpp>
#include <rei.hpp>

#include <regex_match.hpp>

int calculateCost(const std::string& pattren, unsigned short* costFun) {
    auto counts = rei::countOpreations(pattren);
    int count = 0;
    count += counts.alpha * costFun[0];
    count += counts.question * costFun[1];
    count += counts.star * costFun[2];
    count += counts.concat * costFun[3];
    count += counts.alternation * costFun[4];
    return count;
}

int main(int argc, const char* argv[]) {

    // -----------------
    // Reading the input
    // -----------------

    if (argc != 8) {
        printf("Arguments should be in the form of\n");
        printf("-----------------------------------------------------------------\n");
        printf("%s <file_address> <c1> <c2> <c3> <c4> <c5> <max_cost>\n", argv[0]);
        printf("-----------------------------------------------------------------\n");
        printf("\nFor example\n");
        printf("-----------------------------------------------------------------\n");
        printf("%s ./input 1 12 60 1 1 1 1 1 1 500\n", argv[0]);
        printf("-----------------------------------------------------------------\n");
        return 0;
    }

    bool argError = false;
    for (int i = 2; i < argc; ++i) {
        if (std::atoi(argv[i]) <= 0 || std::atoi(argv[i]) > SHRT_MAX) {
            printf("Argument number %d, \"%s\", should be a positive short integer.\n", i, argv[i]);
            argError = true;
        }
    }
    if (argError) return 0;

    std::string fileName = argv[1];
    std::vector<std::string> pos, neg;
    if (!rei::readFile(fileName, pos, neg)) return 0;

    unsigned short costFun[5];
    for (int i = 0; i < 5; i++)
        costFun[i] = std::atoi(argv[i + 2]);
    unsigned short maxCost = std::atoi(argv[7]);

    auto res = rei::Run(costFun, maxCost, pos, neg, 60 * 60 * 60);

    for (auto p : pos)
    {
        if (!match(res.RE, p))
        {
            printf("regex didn't match %s\n",p.c_str());
        }
    }

    for (auto n : neg)
    {
        if (match(res.RE, n))
        {
            printf("regex did match %s\n",n.c_str());
        }
    }

    printf("\n\nRE: \"%s\"\n", res.RE.c_str());
    printf("Cost: %lu\n", calculateCost(res.RE, costFun));

    return 0;
}