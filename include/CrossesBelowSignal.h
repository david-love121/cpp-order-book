#ifndef CROSSESBELOWSIGNAL_H
#define CROSSESBELOWSIGNAL_H

#include "ISignal.h"
#include "IIndicator.h"
#include <memory>

class CrossesBelowSignal : public ISignal {
public:
    CrossesBelowSignal(std::shared_ptr<IIndicator> indicator_a, std::shared_ptr<IIndicator> indicator_b);

    bool is_active() const override;
    const std::string& name() const override;

private:
    std::string m_name;
    std::shared_ptr<IIndicator> m_indicator_a;
    std::shared_ptr<IIndicator> m_indicator_b;
    uint64_t m_previous_a_value;
    uint64_t m_previous_b_value;
};

#endif // CROSSESBELOWSIGNAL_H