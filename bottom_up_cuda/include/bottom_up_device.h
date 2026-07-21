#ifndef BOTTOM_UP_DEVICE_H
#define BOTTOM_UP_DEVICE_H

#include <rei_common.hpp>
#include <operations_device.h>

namespace rei {

    class BottomUpSearchDevice
    {
    public:
        BottomUpSearchDevice(const LanguageSystem& languageSystem, const InputParams& inputParams);

        EnumerationState enumerateCostLevel(Result& res);

        std::string constructRE(int idx) const;

    private:

        EnumerationState enumerateLevel(Result& idx);

        unsigned short costLevel;
        unsigned short shortageCost;
        bool lastRound;

        const InputParams& inputParams;
        const LanguageSystem& languageSystem;

        LevelPartitioner partitioner;
        GuideTableDevice guideTable;
    };
}

#endif // BOTTOM_UP_DEVICE_H