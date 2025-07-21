#ifndef IINDICATOR_H
#define IINDICATOR_H

#include <toml++/toml.h>
#include <string>
#include <vector>
#include <memory>

class IIndicator {
public:
    virtual ~IIndicator() = default;

    // Updates the indicator with a new value
    virtual void update(uint64_t new_value) = 0;

    // Returns the current value of the indicator
    virtual uint64_t value() const = 0;

    // Returns true if the indicator has enough data to be considered valid
    virtual bool is_ready() const = 0;

    // Configures the indicator from a TOML table
    virtual void configure(const toml::table& config) = 0;
    
    // Returns the name of the indicator
    virtual const std::string& name() const = 0;
};

#endif // IINDICATOR_H