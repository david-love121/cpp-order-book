#include <cstdint>
struct Trade {
    uint64_t execution_id;
    uint64_t aggressor_order_id;
    uint64_t resting_order_id;
    uint64_t aggressor_user_id;
    uint64_t resting_user_id;
    uint64_t price;
    uint64_t quantity;
    uint64_t ts_received;
    uint64_t ts_executed;
};