#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <sstream>

#include "OrderBookManager.h"
#include "OrderBookSnapshot.h"
#include "Trade.h"
#include "Logger.h"
#include "PortfolioManager.h"
#include "IIndicator.h"
#include "SMAIndicator.h"
#include "EMAIndicator.h"
#include "RSIIndicator.h"
#include "BookImbalanceIndicator.h"
#include "ISignal.h"
#include "CrossesAboveSignal.h"
#include "CrossesBelowSignal.h"
#include "AboveValueSignal.h"
#include "BelowValueSignal.h"
#include "IStrategy.h"
#include "InMemorySink.h"
#include "TOBSnapshot.h"
#include "PortfolioSnapshot.h"

namespace py = pybind11;

void set_log_file(const std::string& path) {
    GLogger->set_log_file(path);
}

PYBIND11_MODULE(cpp_order_book_engine, m) {
    m.doc() = "Python bindings for the C++ Order Book Engine";

    m.def("set_log_file", &set_log_file, "Set the path for the C++ engine log file");

    py::class_<Trade>(m, "Trade")
        .def_readonly("price", &Trade::price)
        .def_readonly("quantity", &Trade::quantity)
        .def("__repr__", [](const Trade &t) {
            return "<Trade price=" + std::to_string(t.price) + " quantity=" + std::to_string(t.quantity) + ">";
        });

    py::class_<OrderBookSnapshot>(m, "OrderBookSnapshot")
        .def("get_best_bid", &OrderBookSnapshot::GetBestBid)
        .def("get_best_ask", &OrderBookSnapshot::GetBestAsk)
        .def("get_mid_price", &OrderBookSnapshot::GetMidPrice);

    py::class_<IStrategy, std::shared_ptr<IStrategy>>(m, "IStrategy");

    py::class_<IIndicator, std::shared_ptr<IIndicator>>(m, "IIndicator");
    py::class_<SMAIndicator, IIndicator, std::shared_ptr<SMAIndicator>>(m, "SMAIndicator");
    py::class_<EMAIndicator, IIndicator, std::shared_ptr<EMAIndicator>>(m, "EMAIndicator");
    py::class_<RSIIndicator, IIndicator, std::shared_ptr<RSIIndicator>>(m, "RSIIndicator");
    py::class_<BookImbalanceIndicator, IIndicator, std::shared_ptr<BookImbalanceIndicator>>(m, "BookImbalanceIndicator");

    py::class_<ISignal, std::shared_ptr<ISignal>>(m, "ISignal");
    py::class_<CrossesAboveSignal, ISignal, std::shared_ptr<CrossesAboveSignal>>(m, "CrossesAboveSignal");
    py::class_<CrossesBelowSignal, ISignal, std::shared_ptr<CrossesBelowSignal>>(m, "CrossesBelowSignal");
    py::class_<AboveValueSignal, ISignal, std::shared_ptr<AboveValueSignal>>(m, "AboveValueSignal");
    py::class_<BelowValueSignal, ISignal, std::shared_ptr<BelowValueSignal>>(m, "BelowValueSignal");

    py::class_<Strategy, IStrategy, std::shared_ptr<Strategy>>(m, "Strategy")
        .def(py::init<const std::string&>())
        .def("update", py::overload_cast<const OrderBook&, uint64_t>(&Strategy::update))
        .def("update", py::overload_cast<const OrderBookSnapshot&>(&Strategy::update))
        .def("update", py::overload_cast<const Trade&>(&Strategy::update))
        .def("from_toml_string", [](Strategy& self, const std::string& toml_string) {
            self.from_toml(toml::parse(toml_string));
        })
        .def("to_toml_string", [](const Strategy& self) {
            std::stringstream ss;
            ss << self.to_toml();
            return ss.str();
        });

    py::class_<PortfolioManager, std::shared_ptr<PortfolioManager>>(m, "PortfolioManager")
        .def("get_trades", &PortfolioManager::GetTrades, py::return_value_policy::reference);

  py::class_<PortfolioSnapshot>(m, "PortfolioSnapshot")
      .def(py::init<uint64_t, int64_t, double, double, double, double, size_t, double>())
      .def_readwrite("timestamp", &PortfolioSnapshot::timestamp)
      .def_readwrite("position", &PortfolioSnapshot::position)
      .def_readwrite("current_price", &PortfolioSnapshot::current_price)
      .def_readwrite("average_cost", &PortfolioSnapshot::average_cost)
      .def_readwrite("unrealized_pnl", &PortfolioSnapshot::unrealized_pnl)
      .def_readwrite("realized_pnl", &PortfolioSnapshot::realized_pnl)
      .def_readwrite("total_pnl", &PortfolioSnapshot::total_pnl)
      .def_readwrite("total_trades", &PortfolioSnapshot::total_trades)
      .def_readwrite("position_value", &PortfolioSnapshot::position_value)
      .def_readwrite("cash_balance", &PortfolioSnapshot::cash_balance);

  py::class_<TOBSnapshot>(m, "TOBSnapshot")
      .def(py::init<uint64_t, const std::string &, double, double, uint64_t,
                    uint64_t>())
      .def_readwrite("timestamp", &TOBSnapshot::timestamp)
      .def_readwrite("symbol", &TOBSnapshot::symbol)
      .def_readwrite("best_bid", &TOBSnapshot::best_bid)
      .def_readwrite("best_ask", &TOBSnapshot::best_ask)
      .def_readwrite("bid_volume", &TOBSnapshot::bid_volume)
      .def_readwrite("ask_volume", &TOBSnapshot::ask_volume)
      .def_readwrite("mid_price", &TOBSnapshot::mid_price)
      .def_readwrite("spread", &TOBSnapshot::spread);

  py::class_<IDataSink, std::shared_ptr<IDataSink>>(m, "IDataSink");
  py::class_<IndicatorSnapshot>(m, "IndicatorSnapshot")
      .def(py::init<>())
      .def_readwrite("timestamp", &IndicatorSnapshot::timestamp)
      .def_readwrite("headers", &IndicatorSnapshot::headers)
      .def_readwrite("values", &IndicatorSnapshot::values);

  py::class_<InMemorySink, IDataSink, std::shared_ptr<InMemorySink>>(m, "InMemorySink")
      .def("get_portfolio_snapshots", &InMemorySink::GetPortfolioSnapshots, py::return_value_policy::reference)
      .def("get_tob_snapshots", &InMemorySink::GetTobSnapshots, py::return_value_policy::reference)
      .def("get_trades", &InMemorySink::GetTrades, py::return_value_policy::reference)
      .def("get_indicator_snapshots", &InMemorySink::GetIndicatorSnapshots, py::return_value_policy::reference);

    py::class_<OrderBookManager, std::shared_ptr<OrderBookManager>>(m, "OrderBookManager")
        .def(py::init<uint64_t, double, double>(), py::arg("tracked_user_id") = 0, py::arg("max_leverage") = 1.0, py::arg("initial_cash") = 100000.0)
        .def("set_strategy", &OrderBookManager::SetStrategy)
        .def("start", &OrderBookManager::Start)
        .def("stop", &OrderBookManager::Stop)
        .def("initialize_after_construction", &OrderBookManager::InitializeAfterConstruction)
        .def("run_backtest", &OrderBookManager::RunBacktest)
        .def("get_portfolio_manager", &OrderBookManager::GetPortfolioManager)
        .def("get_data_sink", [](OrderBookManager &self) {
            return std::dynamic_pointer_cast<InMemorySink>(self.GetDataSink());
        });
}