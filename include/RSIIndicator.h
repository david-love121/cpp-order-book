#ifndef RSIINDICATOR_H
#define RSIINDICATOR_H

#include "IIndicator.h"
#include <vector>

class RSIIndicator : public IIndicator {
public:
    RSIIndicator();

    void update(uint64_t new_value) override;
    void update(const OrderBook& order_book) override;
    double get_value() const override;
    bool is_ready() const override;
    void configure(const toml::table& config) override;
    const std::string& name() const override;

private:
    std::string m_name;
    int m_period;
    uint64_t m_previous_value;
    double m_avg_gain;
    double m_avg_loss;
    std::vector<uint64_t> m_values;
};

#endif // RSIINDICATOR_H