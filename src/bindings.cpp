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
#include "IStrategy.h"

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

    py::class_<Strategy, IStrategy, std::shared_ptr<Strategy>>(m, "Strategy")
        .def(py::init<const std::string&>())
        .def("update", static_cast<void (Strategy::*)(const OrderBookSnapshot&)>(&Strategy::update))
        .def("update", static_cast<void (Strategy::*)(const Trade&)>(&Strategy::update))
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

    py::class_<OrderBookManager, std::shared_ptr<OrderBookManager>>(m, "OrderBookManager")
        .def(py::init<uint64_t>(), py::arg("tracked_user_id") = 0)
        .def("set_strategy", &OrderBookManager::SetStrategy)
        .def("start", &OrderBookManager::Start)
        .def("stop", &OrderBookManager::Stop)
        .def("initialize_after_construction", &OrderBookManager::InitializeAfterConstruction)
        .def("run_historical_data_demo", &OrderBookManager::RunHistoricalDataDemo)
        .def("get_portfolio_manager", &OrderBookManager::GetPortfolioManager);
}