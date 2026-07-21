#ifndef REI_HPP
#define REI_HPP

#include <string>
#include <vector>
#include <rei_common.hpp>

namespace rei {

    enum class SearchType {
        BottomUp,
        TopDown,
        Bidirectional
    };

	Result Run(SearchType searchType, const InputParams& inputParams);
}

#endif //end REI_HPP