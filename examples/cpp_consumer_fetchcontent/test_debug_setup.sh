export DATABENTO_API_KEY=db-7ywTbpJQ6wEYUETdSduug9qwMVxiB
cd /home/david/repos/cpp_order_book/examples/cpp_consumer_fetchcontent/build
cmake -DCMAKE_BUILD_TYPE=Debug ../
make -j12
cd /home/david/repos/cpp_order_book/examples/cpp_consumer_fetchcontent/
./build/consumer_fetchcontent | head -40