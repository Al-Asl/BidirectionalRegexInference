#include <iostream>
#include <vector>
#include <string>
#include <chrono>

#include <rei.hpp>
#include <utils.hpp>
#include <regex_match.hpp>

int main(int argc, const char* argv[]) {

    // -----------------
    // Reading the input
    // -----------------

    if (argc != 8) {
        printf("Arguments should be in the form of\n");
        printf("-----------------------------------------------------------------\n");
        printf("%s <file_address> <max_cost> <c1> <c2> <c3> <c4> <c5>\n", argv[0]);
        printf("-----------------------------------------------------------------\n");
        printf("\nFor example\n");
        printf("-----------------------------------------------------------------\n");
        printf("%s ./input 500 1 1 1 1 1\n", argv[0]);
        printf("-----------------------------------------------------------------\n");
        return 0;
    }

    rei::InputParams input_params{};
    input_params.n = SOLUTIONS_NUM;

    if (!rei::readFile(argv[1], input_params.pos, input_params.neg)) return 0;

    if (!rei::parse_number_arg(argv[2], input_params.maxCost))
        return 0;

    for (int i = 0; i < static_cast<int>(rei::Operation::Count) + 1; i++)
    {
        unsigned short cost = 0;
        if (!rei::parse_number_arg(argv[i + 3], cost))
            return 0;
        input_params.costFunc.costs.push_back(cost);
    }

    input_params.print();

    // ----------------------------------
    // Regular Expression Inference (REI)
    // ----------------------------------

    auto start = std::chrono::high_resolution_clock::now();

    auto result = rei::Run(rei::SearchType::Bidirectional, input_params);

    auto stop = std::chrono::high_resolution_clock::now();

    printf("\n==== Output ===\n\n");

    if (result.entries.size() > 0)
    {
        printf("Found %llu solution\n", result.entries.size());

        for (int i = 0; i < result.entries.size(); i++)
        {
            printf("\n----- %u of %u -----\n\n", i + 1, input_params.n);

            auto entry = result.entries[i];

            // ----------
            // Validating
            // ----------

            bool valid = true;

            for (auto p : input_params.pos)
            {
                if (!match(entry.RE, p))
                {
                    printf("RE didn't match %s\n", p.c_str());
                    valid = false;
                }
            }

            for (auto n : input_params.neg)
            {
                if (match(entry.RE, n))
                {
                    printf("RE did match %s\n", n.c_str());
                    valid = false;
                }
            }

            if (!valid)
            {
                printf("\nError: the generated RE %s didn't match all the examples \n", entry.RE.c_str());
                continue;
            }

            // -------------------
            // Printing the output
            // -------------------

            auto cost = rei::calculateCost(entry.RE, input_params.costFunc.costs.data());
            printf("All REs: %llu\n", entry.allCSs);
            printf("Unique REs: %llu\n", entry.uniqueCSs);
            printf("Running Time: %f s\n", entry.duration);
            printf("Cost: %u\n", cost);
            printf("\nRE: \"%s\"\n", entry.RE.c_str());

        }
    }
    else
        printf("failed to find any solution: %s", result.message.c_str());

    return 0;
}