#ifndef SMAINDICATOR_H
#define SMAINDICATOR_H

#include "IIndicator.h"
#include <vector>
#include <string>

class SMAIndicator : public IIndicator {
public:
    SMAIndicator(const std::string& name);

    void update(uint64_t new_value) override;
    uint64_t value() const override;
    bool is_ready() const override;
    void configure(const toml::table& config) override;
    const std::string& name() const override;

private:
    std::string m_name;
    int m_period;
    std::vector<uint64_t> m_price_history;
    uint64_t m_current_value;
};

#endif // SMAINDICATOR_H