cd /home/david/repos/cpp_order_book/examples/cpp_consumer_fetchcontent/
export $(cat .env | xargs)
cd /home/david/repos/cpp_order_book/examples/cpp_consumer_fetchcontent/build
cmake -DCMAKE_BUILD_TYPE=Debug ../
make -j12
cd /home/david/repos/cpp_order_book/examples/cpp_consumer_fetchcontent/
