#ifndef BELOWVALUESIGNAL_H
#define BELOWVALUESIGNAL_H

#include "ValueSignal.h"

class BelowValueSignal : public ValueSignal {
public:
    BelowValueSignal(std::shared_ptr<IIndicator> indicator, uint64_t value);

    bool is_active() const override;
};

#endif // BELOWVALUESIGNAL_H