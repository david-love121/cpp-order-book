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

struct Rule {
    std::string signal_name;
    trading::Action action;
    int quantity;
};

#endif // ISIGNAL_H