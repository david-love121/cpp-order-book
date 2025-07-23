#pragma once

#include <cstdint>
#include <cmath>

struct PortfolioSnapshot {
  uint64_t timestamp;
  int64_t position;
  double current_price;
  double average_cost;
  double unrealized_pnl;
  double realized_pnl;
  double total_pnl;
  size_t total_trades;
  double position_value;
  double cash_balance;

  PortfolioSnapshot(uint64_t ts, int64_t pos, double cur_price, double avg_cost,
                    double unrealized, double realized, size_t trades, double cash)
      : timestamp(ts), position(pos), current_price(cur_price),
        average_cost(avg_cost), unrealized_pnl(unrealized),
        realized_pnl(realized), total_pnl(realized + unrealized),
        total_trades(trades),
        position_value(cur_price * std::abs(pos)), cash_balance(cash) {}

};