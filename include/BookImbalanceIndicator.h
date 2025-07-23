#ifndef BOOKIMBALANCEINDICATOR_H
#define BOOKIMBALANCEINDICATOR_H

#include "IIndicator.h"
#include "OrderBook.h"

class BookImbalanceIndicator : public IIndicator {
public:
    BookImbalanceIndicator();

    void update(const OrderBook& order_book) override;
    void update(uint64_t new_value) override; // Not used for this indicator
    double get_value() const override;
    bool is_ready() const override;
    void configure(const toml::table& config) override;
    const std::string& name() const override;

private:
    std::string m_name;
    int m_depth;
    double m_imbalance;
};

#endif // BOOKIMBALANCEINDICATOR_H