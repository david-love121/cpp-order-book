#ifndef CROSSESABOVESIGNAL_H
#define CROSSESABOVESIGNAL_H

#include "ISignal.h"
#include "IIndicator.h"
#include <memory>

class CrossesAboveSignal : public ISignal {
public:
    CrossesAboveSignal(const std::string& name, std::shared_ptr<IIndicator> indicator_a, std::shared_ptr<IIndicator> indicator_b, int cooldown);

    bool is_active() const override;
    const std::string& name() const override;

private:
    std::string m_name;
    std::shared_ptr<IIndicator> m_indicator_a;
    std::shared_ptr<IIndicator> m_indicator_b;
    mutable uint64_t m_previous_a;
    mutable uint64_t m_previous_b;
    int m_cooldown;
    mutable int m_cooldown_counter;
};

#endif // CROSSESABOVESIGNAL_H