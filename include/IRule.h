#pragma once

#include "ISignal.h"
#include "PortfolioManager.h"
#include <vector>
#include <string>
#include <memory>
#include <map>

class IRule {
public:
    virtual ~IRule() = default;
    virtual bool is_satisfied(const std::map<std::string, std::shared_ptr<ISignal>>& signals, const PortfolioManager& portfolio) const = 0;
    virtual trading::Action get_action() const = 0;
    virtual int get_quantity() const = 0;
    virtual std::string get_name() const { return m_name; }
    virtual void set_name(const std::string& name) { m_name = name; }

protected:
    std::string m_name;
};