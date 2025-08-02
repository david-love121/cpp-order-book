#ifndef ISIGNAL_H
#define ISIGNAL_H

#include "IIndicator.h"
#include <string>
#include <vector>
#include <memory>

namespace trading {
enum class Action {
    BUY,
    SELL
};
}

class ISignal {
public:
    virtual ~ISignal() = default;
    virtual bool is_active() const = 0;
    virtual const std::string& name() const = 0;
};

enum class RuleCondition {
    ALWAYS,
    IF_FLAT,
    IF_LONG,
    IF_SHORT
};

struct Rule {
    std::vector<std::string> signal_names;
    trading::Action action;
    int quantity;
    RuleCondition condition = RuleCondition::ALWAYS;
};

#endif // ISIGNAL_H