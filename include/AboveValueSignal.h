#ifndef ABOVEVALUESIGNAL_H
#define ABOVEVALUESIGNAL_H

#include "ValueSignal.h"

class AboveValueSignal : public ValueSignal {
public:
    AboveValueSignal(std::shared_ptr<IIndicator> indicator, uint64_t value);

    bool is_active() const override;
};

#endif // ABOVEVALUESIGNAL_H