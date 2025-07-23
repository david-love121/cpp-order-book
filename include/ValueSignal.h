#ifndef VALUESIGNAL_H
#define VALUESIGNAL_H

#include "ISignal.h"
#include "IIndicator.h"
#include <memory>

class ValueSignal : public ISignal {
public:
    ValueSignal(std::shared_ptr<IIndicator> indicator, uint64_t value);

    const std::string& name() const override;

protected:
    std::string m_name;
    std::shared_ptr<IIndicator> m_indicator;
    uint64_t m_value;
};

#endif // VALUESIGNAL_H