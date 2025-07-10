# CPP Order book example client
This client utilizes the library in the Cpp_order_book path to view data and backtest a strategy with ES mini future data.

To build:
change working directory to cpp_consumer_fetchcontent/build
cmake ../
make -j8

To run:
change working directory to cpp_consumer_fetchcontent/
set env variable found in .env
./build/consumer_fetchcontent