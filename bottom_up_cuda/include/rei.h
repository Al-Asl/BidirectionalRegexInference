#ifndef REI_HPP
#define REI_HPP

#include <bottom_up_device.h>

namespace rei {

    inline rei::Result Run(const InputParams& input_params) {

        Result result;

        Solution entry;

        LanguageSystem language_system(input_params.pos, input_params.neg);

        if (rei::intialCheck(language_system, input_params.pos, entry))
        {
            result.push_back(entry);
            if (input_params.n == result.entries.size())
                return result;
        }

        auto bottomUp = BottomUpSearchDevice(language_system, input_params);

        EnumerationState enumState;
        do {
            enumState = bottomUp.enumerateCostLevel(result);
        } while (enumState == EnumerationState::NotFound);

        return result;
    }
}

#endif // REI_HPP