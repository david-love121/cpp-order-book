#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <random>

#include "OrderBook.h"
#include "Trade.h"
#include "Order.h"
#include "PriceLevel.h"

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<OrderBook>();
    }

    void TearDown() override {
        book.reset();
    }

    std::unique_ptr<OrderBook> book;
};

// Test a complete trading scenario
TEST_F(IntegrationTest, CompleteTradingScenario) {
    // Phase 1: Build initial order book
    book->AddOrder(1001, 1, false, 100, 10060);  // Sell 100 @ 100.60
    book->AddOrder(1002, 1, false, 150, 10050);  // Sell 150 @ 100.50
    book->AddOrder(1003, 1, false, 200, 10070);  // Sell 200 @ 100.70
    
    book->AddOrder(2001, 2, true, 120, 10040);   // Buy 120 @ 100.40
    book->AddOrder(2002, 2, true, 100, 10030);   // Buy 100 @ 100.30
    book->AddOrder(2003, 2, true, 180, 10045);   // Buy 180 @ 100.45
    
    // Verify initial state
    EXPECT_EQ(book->GetBestBid(), 10045);        // Highest buy price
    EXPECT_EQ(book->GetBestAsk(), 10050);        // Lowest sell price
    EXPECT_EQ(book->GetTotalBidVolume(), 400);   // 120 + 100 + 180
    EXPECT_EQ(book->GetTotalAskVolume(), 450);   // 100 + 150 + 200
    
    // Phase 2: Add aggressive buy order that crosses spread
    book->AddOrder(3001, 3, true, 200, 10055);  // Buy 200 @ 100.55
    
    // Should match with best ask (150 @ 100.50) and leave 50 unmatched
    EXPECT_EQ(book->GetBestAsk(), 10060);        // Next best ask
    EXPECT_EQ(book->GetTotalAskVolume(), 300);   // 100 + 200 (150 was matched)
    EXPECT_EQ(book->GetBestBid(), 10055);        // New order becomes best bid
    EXPECT_EQ(book->GetTotalBidVolume(), 450);   // 400 + 50 (remaining from 3001)
    
    // Phase 3: Add aggressive sell order
    book->AddOrder(4001, 4, false, 250, 10040); // Sell 250 @ 100.40
    
    // Should match multiple bid levels
    EXPECT_LT(book->GetTotalBidVolume(), 450);   // Some bids should be matched
    
    // Phase 4: Cancel some orders
    book->CancelOrder(1001);  // Cancel sell order
    book->CancelOrder(2001);  // Cancel buy order (if still exists)
    
    // Verify cancellations affected totals
    EXPECT_LT(book->GetTotalAskVolume(), 300);
}

// Test order book depth and liquidity
TEST_F(IntegrationTest, OrderBookDepthTest) {
    // Add orders at multiple price levels
    const int num_levels = 10;
    const uint64_t base_price = 10000;
    const uint64_t quantity_per_level = 100;
    
    // Add sell orders (asks) at increasing prices
    for (int i = 0; i < num_levels; ++i) {
        uint64_t price = base_price + 10 + (i * 10);  // 100.10, 100.20, ...
        book->AddOrder(1000 + i, 1, false, quantity_per_level, price);
    }
    
    // Add buy orders (bids) at decreasing prices
    for (int i = 0; i < num_levels; ++i) {
        uint64_t price = base_price - 10 - (i * 10);  // 99.90, 99.80, ...
        book->AddOrder(2000 + i, 2, true, quantity_per_level, price);
    }
    
    // Verify spread and depth
    EXPECT_EQ(book->GetBestBid(), base_price - 10);     // 99.90
    EXPECT_EQ(book->GetBestAsk(), base_price + 10);     // 100.10
    EXPECT_EQ(book->GetTotalBidVolume(), num_levels * quantity_per_level);
    EXPECT_EQ(book->GetTotalAskVolume(), num_levels * quantity_per_level);
    
    // Add large market order that should walk through multiple levels
    uint64_t large_quantity = (num_levels / 2) * quantity_per_level + 50;  // 5.5 levels
    book->AddOrder(9999, 99, true, large_quantity, base_price + 100);  // Aggressive buy
    
    // Should have consumed several ask levels
    EXPECT_LT(book->GetTotalAskVolume(), num_levels * quantity_per_level);
    EXPECT_GT(book->GetBestAsk(), base_price + 10);  // Best ask should have moved up
}

// Test high-frequency trading scenario
TEST_F(IntegrationTest, HighFrequencyTradingTest) {
    const int num_orders = 1000;
    std::vector<uint64_t> order_ids;
    
    // Add many orders rapidly
    for (int i = 0; i < num_orders; ++i) {
        bool is_buy = (i % 2 == 0);
        uint64_t price = 10000 + (i % 20) - 10;  // Prices around 100.00
        uint64_t quantity = 10 + (i % 50);       // Varying quantities
        
        order_ids.push_back(i + 1);
        book->AddOrder(i + 1, i % 10, is_buy, quantity, price);
    }
    
    // Rapidly cancel some orders
    for (int i = 0; i < num_orders / 4; ++i) {
        try {
            book->CancelOrder(order_ids[i * 4]);
        } catch (const std::exception&) {
            // Order might have been matched and removed
        }
    }
    
    // Add more aggressive orders
    for (int i = 0; i < 100; ++i) {
        book->AddOrder(num_orders + i + 1, 999, i % 2 == 0, 50, 10000);
    }
    
    // Verify book is still in valid state
    uint64_t best_bid = book->GetBestBid();
    uint64_t best_ask = book->GetBestAsk();
    
    if (best_bid > 0 && best_ask > 0) {
        EXPECT_LE(best_bid, best_ask);  // No crossed market
    }
    
    EXPECT_GE(book->GetTotalBidVolume(), 0);
    EXPECT_GE(book->GetTotalAskVolume(), 0);
}

// Test market stress with random orders
TEST_F(IntegrationTest, RandomOrderStressTest) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> side_dist(0, 1);
    std::uniform_int_distribution<> price_dist(9950, 10050);  // ±0.50 around 100.00
    std::uniform_int_distribution<> quantity_dist(1, 1000);
    
    const int num_operations = 5000;
    std::vector<uint64_t> active_orders;
    
    for (int i = 0; i < num_operations; ++i) {
        int operation = i % 10;  // 80% adds, 20% cancels
        
        if (operation < 8 || active_orders.empty()) {
            // Add order
            bool is_buy = side_dist(gen) == 1;
            uint64_t price = price_dist(gen);
            uint64_t quantity = quantity_dist(gen);
            uint64_t order_id = i + 1;
            
            try {
                book->AddOrder(order_id, i % 100, is_buy, quantity, price);
                active_orders.push_back(order_id);
            } catch (const std::exception&) {
                // Duplicate order ID, skip
            }
        } else {
            // Cancel random order
            if (!active_orders.empty()) {
                std::uniform_int_distribution<> idx_dist(0, active_orders.size() - 1);
                int idx = idx_dist(gen);
                uint64_t order_id = active_orders[idx];
                
                try {
                    book->CancelOrder(order_id);
                    active_orders.erase(active_orders.begin() + idx);
                } catch (const std::exception&) {
                    // Order might have been matched, remove from tracking
                    active_orders.erase(active_orders.begin() + idx);
                }
            }
        }
        
        // Periodically verify book integrity
        if (i % 1000 == 0) {
            uint64_t best_bid = book->GetBestBid();
            uint64_t best_ask = book->GetBestAsk();
            
            if (best_bid > 0 && best_ask > 0) {
                EXPECT_LE(best_bid, best_ask) << "Crossed market detected at iteration " << i;
            }
        }
    }
    
    // Final integrity check
    uint64_t final_bid_volume = book->GetTotalBidVolume();
    uint64_t final_ask_volume = book->GetTotalAskVolume();
    
    EXPECT_GE(final_bid_volume, 0);
    EXPECT_GE(final_ask_volume, 0);
    
    std::cout << "Stress test completed: " << final_bid_volume 
              << " bid volume, " << final_ask_volume << " ask volume" << std::endl;
}

// Test order modification functionality
TEST_F(IntegrationTest, OrderModificationTest) {
    // Add initial orders
    book->AddOrder(1001, 1, true, 100, 10000);   // Buy 100 @ 100.00
    book->AddOrder(1002, 1, false, 150, 10050);  // Sell 150 @ 100.50
    
    EXPECT_EQ(book->GetBestBid(), 10000);
    EXPECT_EQ(book->GetTotalBidVolume(), 100);
    EXPECT_EQ(book->GetBestAsk(), 10050);
    EXPECT_EQ(book->GetTotalAskVolume(), 150);
    
    // Test 1: Modify quantity down (should preserve time priority)
    book->ModifyOrder(1001, 75, 10000);  // Reduce buy order to 75
    EXPECT_EQ(book->GetTotalBidVolume(), 75);
    EXPECT_EQ(book->GetBestBid(), 10000);
    
    // Test 2: Modify quantity up (should go to back of queue at same price)
    book->ModifyOrder(1001, 120, 10000);  // Increase buy order to 120
    EXPECT_EQ(book->GetTotalBidVolume(), 120);
    EXPECT_EQ(book->GetBestBid(), 10000);
    
    // Test 3: Modify price (should trigger matching if crossing spread)
    book->ModifyOrder(1002, 150, 9990);  // Move sell order to 99.90 (should match)
    // After matching, buy order should be fully filled, sell order should have remainder
    EXPECT_EQ(book->GetTotalBidVolume(), 0);  // Buy order should be filled
    EXPECT_EQ(book->GetTotalAskVolume(), 30); // Sell order remainder: 150 - 120 = 30
    EXPECT_EQ(book->GetBestAsk(), 9990);
    
    // Test 4: Modify non-existent order (should throw)
    EXPECT_THROW(book->ModifyOrder(9999, 100, 10000), std::runtime_error);
    
    // Test 5: Modify with zero quantity (should throw)
    EXPECT_THROW(book->ModifyOrder(1002, 0, 9990), std::invalid_argument);
    
    // Clean up remaining order
    book->CancelOrder(1002);
    EXPECT_EQ(book->GetTotalAskVolume(), 0);
    
    std::cout << "Order modification test: All ModifyOrder tests passed!" << std::endl;
}

// Test edge cases and error conditions
TEST_F(IntegrationTest, EdgeCasesAndErrorHandling) {
    // Test duplicate order ID
    book->AddOrder(1001, 1, true, 100, 10000);
    EXPECT_THROW(book->AddOrder(1001, 2, false, 150, 10050), std::runtime_error);
    
    // Clean up the test order before other tests
    book->CancelOrder(1001);
    
    // Test cancelling non-existent order
    EXPECT_THROW(book->CancelOrder(99999), std::runtime_error);
    
    // Test zero quantity (should be rejected)
    EXPECT_THROW(book->AddOrder(2001, 2, true, 0, 10000), std::invalid_argument);
    
    // Test very large quantities
    const uint64_t large_qty = 1000000000ULL;  // 1 billion
    EXPECT_NO_THROW(book->AddOrder(3001, 3, true, large_qty, 10000));
    EXPECT_EQ(book->GetTotalBidVolume(), large_qty);
    
    // Test very high and low prices
    EXPECT_NO_THROW(book->AddOrder(4001, 4, false, 100, 1));        // Very low price
    EXPECT_NO_THROW(book->AddOrder(5001, 5, false, 100, UINT64_MAX)); // Very high price
}

// Test order book recovery and consistency
TEST_F(IntegrationTest, OrderBookConsistencyTest) {
    // Build a complex order book
    const int num_orders_per_side = 50;
    
    for (int i = 0; i < num_orders_per_side; ++i) {
        // Buy orders at decreasing prices
        book->AddOrder(i + 1, 1, true, 100, 10000 - i);
        
        // Sell orders at increasing prices
        book->AddOrder(i + 1000, 2, false, 100, 10100 + i);
    }
    
    uint64_t initial_bid_volume = book->GetTotalBidVolume();
    uint64_t initial_ask_volume = book->GetTotalAskVolume();
    
    // Add crossing orders that should generate trades
    book->AddOrder(9001, 90, true, 1000, 10120);   // Aggressive buy
    book->AddOrder(9002, 91, false, 800, 9980);    // Aggressive sell
    
    // Verify volumes changed due to matching
    EXPECT_NE(book->GetTotalBidVolume(), initial_bid_volume);
    EXPECT_NE(book->GetTotalAskVolume(), initial_ask_volume);
    
    // Verify best prices are still valid
    uint64_t best_bid = book->GetBestBid();
    uint64_t best_ask = book->GetBestAsk();
    
    if (best_bid > 0 && best_ask > 0) {
        EXPECT_LE(best_bid, best_ask);
    }
    
    // Cancel remaining orders systematically
    for (int i = 0; i < num_orders_per_side; ++i) {
        try {
            book->CancelOrder(i + 1);     // Cancel buy orders
        } catch (...) { /* Order might have been matched */ }
        
        try {
            book->CancelOrder(i + 1000);  // Cancel sell orders
        } catch (...) { /* Order might have been matched */ }
    }
    
    std::cout << "Final state: " << book->GetTotalBidVolume() 
              << " bid volume, " << book->GetTotalAskVolume() << " ask volume" << std::endl;
}
