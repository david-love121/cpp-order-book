#ifndef EMAINDICATOR_H
#define EMAINDICATOR_H

#include "IIndicator.h"
#include <vector>

class EMAIndicator : public IIndicator {
public:
    EMAIndicator();

    void update(uint64_t new_value) override;
    uint64_t value() const override;
    bool is_ready() const override;
    void configure(const toml::table& config) override;
    const std::string& name() const override;

private:
    std::string m_name;
    int m_period;
    double m_smoothing_factor;
    uint64_t m_current_value;
    std::vector<uint64_t> m_values;
};

#endif // EMAINDICATOR_H