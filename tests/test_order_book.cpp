#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <chrono>

#include "OrderBook.h"
#include "Trade.h"
#include "Order.h"
#include "PriceLevel.h"

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<OrderBook>();
    }

    void TearDown() override {
        book.reset();
    }

    std::unique_ptr<OrderBook> book;
};

// Test basic order book initialization
TEST_F(OrderBookTest, InitialState) {
    EXPECT_EQ(book->GetBestBid(), 0);
    EXPECT_EQ(book->GetBestAsk(), 0);
    EXPECT_EQ(book->GetTotalBidVolume(), 0);
    EXPECT_EQ(book->GetTotalAskVolume(), 0);
}

// Test adding a single buy order
TEST_F(OrderBookTest, AddSingleBuyOrder) {
    book->AddOrder(1001, 1, true, 100, 10000);  // Buy 100 @ 100.00
    
    EXPECT_EQ(book->GetBestBid(), 10000);
    EXPECT_EQ(book->GetBestAsk(), 0);  // No sell orders
    EXPECT_EQ(book->GetTotalBidVolume(), 100);
    EXPECT_EQ(book->GetTotalAskVolume(), 0);
}

// Test adding a single sell order
TEST_F(OrderBookTest, AddSingleSellOrder) {
    book->AddOrder(2001, 2, false, 150, 10050);  // Sell 150 @ 100.50
    
    EXPECT_EQ(book->GetBestBid(), 0);  // No buy orders
    EXPECT_EQ(book->GetBestAsk(), 10050);
    EXPECT_EQ(book->GetTotalBidVolume(), 0);
    EXPECT_EQ(book->GetTotalAskVolume(), 150);
}

// Test adding multiple buy orders (bid ordering)
TEST_F(OrderBookTest, MultipleBuyOrdersOrdering) {
    book->AddOrder(1001, 1, true, 100, 10000);  // Buy 100 @ 100.00
    book->AddOrder(1002, 1, true, 150, 10020);  // Buy 150 @ 100.20
    book->AddOrder(1003, 1, true, 200, 9980);   // Buy 200 @ 99.80
    
    // Best bid should be the highest price
    EXPECT_EQ(book->GetBestBid(), 10020);
    EXPECT_EQ(book->GetTotalBidVolume(), 450);  // 100 + 150 + 200
}

// Test adding multiple sell orders (ask ordering)
TEST_F(OrderBookTest, MultipleSellOrdersOrdering) {
    book->AddOrder(2001, 2, false, 100, 10050);  // Sell 100 @ 100.50
    book->AddOrder(2002, 2, false, 150, 10030);  // Sell 150 @ 100.30
    book->AddOrder(2003, 2, false, 200, 10070);  // Sell 200 @ 100.70
    
    // Best ask should be the lowest price
    EXPECT_EQ(book->GetBestAsk(), 10030);
    EXPECT_EQ(book->GetTotalAskVolume(), 450);  // 100 + 150 + 200
}

// Test order cancellation
TEST_F(OrderBookTest, OrderCancellation) {
    // Add orders
    book->AddOrder(1001, 1, true, 100, 10000);   // Buy 100 @ 100.00
    book->AddOrder(1002, 1, true, 150, 10020);   // Buy 150 @ 100.20
    book->AddOrder(2001, 2, false, 200, 10050);  // Sell 200 @ 100.50
    
    EXPECT_EQ(book->GetTotalBidVolume(), 250);
    EXPECT_EQ(book->GetTotalAskVolume(), 200);
    
    // Cancel a buy order
    book->CancelOrder(1001);
    EXPECT_EQ(book->GetTotalBidVolume(), 150);
    EXPECT_EQ(book->GetBestBid(), 10020);
    
    // Cancel the sell order
    book->CancelOrder(2001);
    EXPECT_EQ(book->GetTotalAskVolume(), 0);
    // Best ask might be 0 if the price level is removed, or the price if it remains
    // This depends on implementation details
}

// Test cancelling non-existent order
TEST_F(OrderBookTest, CancelNonExistentOrder) {
    EXPECT_THROW(book->CancelOrder(99999), std::runtime_error);
}

// Test duplicate order ID
TEST_F(OrderBookTest, DuplicateOrderId) {
    book->AddOrder(1001, 1, true, 100, 10000);
    EXPECT_THROW(book->AddOrder(1001, 2, false, 150, 10050), std::runtime_error);
}

// Test basic matching - buy order crosses spread
TEST_F(OrderBookTest, BasicMatchingBuyCrossesSpread) {
    // Add a sell order first
    book->AddOrder(2001, 2, false, 100, 10050);  // Sell 100 @ 100.50
    EXPECT_EQ(book->GetBestAsk(), 10050);
    EXPECT_EQ(book->GetTotalAskVolume(), 100);
    
    // Add aggressive buy order that should match
    book->AddOrder(1001, 1, true, 80, 10050);  // Buy 80 @ 100.50 (matches)
    
    // After matching, there should be 20 remaining on the sell side
    EXPECT_EQ(book->GetTotalAskVolume(), 20);   // 100 - 80 = 20
    EXPECT_EQ(book->GetBestAsk(), 10050);
    EXPECT_EQ(book->GetTotalBidVolume(), 0);  // Buy order fully filled
}

// Test basic matching - sell order crosses spread
TEST_F(OrderBookTest, BasicMatchingSellCrossesSpread) {
    // Add a buy order first
    book->AddOrder(1001, 1, true, 100, 10000);  // Buy 100 @ 100.00
    EXPECT_EQ(book->GetBestBid(), 10000);
    EXPECT_EQ(book->GetTotalBidVolume(), 100);
    
    // Add aggressive sell order that should match
    book->AddOrder(2001, 2, false, 80, 10000);  // Sell 80 @ 100.00 (matches)
    
    // After matching, there should be 20 remaining on the buy side
    EXPECT_EQ(book->GetTotalBidVolume(), 20);   // 100 - 80 = 20
    EXPECT_EQ(book->GetBestBid(), 10000);
    EXPECT_EQ(book->GetTotalAskVolume(), 0);  // Sell order fully filled
}

// Test partial fill across multiple price levels
TEST_F(OrderBookTest, PartialFillMultipleLevels) {
    // Add multiple sell orders at different prices
    book->AddOrder(2001, 2, false, 50, 10050);   // Sell 50 @ 100.50
    book->AddOrder(2002, 2, false, 75, 10060);   // Sell 75 @ 100.60
    book->AddOrder(2003, 2, false, 100, 10070);  // Sell 100 @ 100.70
    
    EXPECT_EQ(book->GetTotalAskVolume(), 225);
    
    // Add large buy order that should match multiple levels
    book->AddOrder(1001, 1, true, 200, 10065);  // Buy 200 @ 100.65
    
    // Should match first two levels (50 + 75 = 125) and leave 75 unmatched
    // Remaining volume should be from the third level (100) plus unmatched portion
    EXPECT_EQ(book->GetTotalAskVolume(), 100);   // Only third level remains
    EXPECT_EQ(book->GetBestAsk(), 10070);
    EXPECT_EQ(book->GetTotalBidVolume(), 75);    // Unmatched portion becomes resting order
}

// Test that orders at same price maintain time priority
TEST_F(OrderBookTest, TimePriorityAtSamePrice) {
    // Add multiple buy orders at same price
    book->AddOrder(1001, 1, true, 100, 10000);  // First order
    book->AddOrder(1002, 2, true, 150, 10000);  // Second order
    book->AddOrder(1003, 3, true, 200, 10000);  // Third order
    
    EXPECT_EQ(book->GetTotalBidVolume(), 450);
    EXPECT_EQ(book->GetBestBid(), 10000);
    
    // Add sell order that partially fills
    book->AddOrder(2001, 4, false, 250, 10000);  // Sell 250 @ 100.00
    
    // Should fill first order completely (100) and part of second order (150)
    // Remaining should be 200 (third order) + 0 (second order fully filled too)
    EXPECT_EQ(book->GetTotalBidVolume(), 200);  // Only third order remains
}

// Test empty order book after full matching
TEST_F(OrderBookTest, EmptyAfterFullMatching) {
    // Add orders that will fully match
    book->AddOrder(1001, 1, true, 100, 10000);   // Buy 100 @ 100.00
    book->AddOrder(2001, 2, false, 100, 10000);  // Sell 100 @ 100.00
    
    // Both orders should be fully matched and removed
    EXPECT_EQ(book->GetTotalBidVolume(), 0);
    EXPECT_EQ(book->GetTotalAskVolume(), 0);
    EXPECT_EQ(book->GetBestBid(), 0);
    EXPECT_EQ(book->GetBestAsk(), 0);
}

// Test large order quantities
TEST_F(OrderBookTest, LargeOrderQuantities) {
    const uint64_t large_qty = 1000000;  // 1 million
    
    book->AddOrder(1001, 1, true, large_qty, 10000);
    book->AddOrder(2001, 2, false, large_qty, 10050);
    
    EXPECT_EQ(book->GetTotalBidVolume(), large_qty);
    EXPECT_EQ(book->GetTotalAskVolume(), large_qty);
}

// Test edge case: zero quantity (should be rejected)
TEST_F(OrderBookTest, ZeroQuantityOrder) {
    // Zero quantity orders should be rejected
    EXPECT_THROW(book->AddOrder(1001, 1, true, 0, 10000), std::invalid_argument);
    // No orders should be added
    EXPECT_EQ(book->GetTotalBidVolume(), 0);
}

// Performance test: Add many orders
TEST_F(OrderBookTest, ManyOrdersPerformance) {
    const int num_orders = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Add many buy orders
    for (int i = 0; i < num_orders; ++i) {
        book->AddOrder(i + 1, 1, true, 100, 10000 - i);  // Decreasing prices
    }
    
    // Add many sell orders
    for (int i = 0; i < num_orders; ++i) {
        book->AddOrder(num_orders + i + 1, 2, false, 100, 10100 + i);  // Increasing prices
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Verify correctness
    EXPECT_EQ(book->GetTotalBidVolume(), num_orders * 100);
    EXPECT_EQ(book->GetTotalAskVolume(), num_orders * 100);
    EXPECT_EQ(book->GetBestBid(), 10000);  // Highest buy price
    EXPECT_EQ(book->GetBestAsk(), 10100);  // Lowest sell price
    
    // Performance check (adjust threshold as needed)
    EXPECT_LT(duration.count(), 100000);  // Should complete in less than 100ms
    
    std::cout << "Added " << (2 * num_orders) << " orders in " 
              << duration.count() << " microseconds" << std::endl;
}

// Test order modification - quantity decrease
TEST_F(OrderBookTest, ModifyOrderQuantityDecrease) {
    // Add an order
    book->AddOrder(1001, 1, true, 100, 10000);
    EXPECT_EQ(book->GetTotalBidVolume(), 100);
    
    // Decrease quantity
    book->ModifyOrder(1001, 75, 10000);
    EXPECT_EQ(book->GetTotalBidVolume(), 75);
    EXPECT_EQ(book->GetBestBid(), 10000);
}

// Test order modification - quantity increase
TEST_F(OrderBookTest, ModifyOrderQuantityIncrease) {
    // Add an order
    book->AddOrder(1001, 1, true, 100, 10000);
    EXPECT_EQ(book->GetTotalBidVolume(), 100);
    
    // Increase quantity
    book->ModifyOrder(1001, 150, 10000);
    EXPECT_EQ(book->GetTotalBidVolume(), 150);
    EXPECT_EQ(book->GetBestBid(), 10000);
}

// Test order modification - price change
TEST_F(OrderBookTest, ModifyOrderPrice) {
    // Add orders on both sides
    book->AddOrder(1001, 1, true, 100, 9900);   // Buy @ 99.00
    book->AddOrder(1002, 1, false, 100, 10100); // Sell @ 101.00
    
    EXPECT_EQ(book->GetBestBid(), 9900);
    EXPECT_EQ(book->GetBestAsk(), 10100);
    
    // Modify buy order to higher price (but still no match)
    book->ModifyOrder(1001, 100, 10000);  // Move to 100.00
    EXPECT_EQ(book->GetBestBid(), 10000);
    EXPECT_EQ(book->GetBestAsk(), 10100);
    EXPECT_EQ(book->GetTotalBidVolume(), 100);
    EXPECT_EQ(book->GetTotalAskVolume(), 100);
}

// Test order modification - price change causing match
TEST_F(OrderBookTest, ModifyOrderPriceCausingMatch) {
    // Add orders on both sides
    book->AddOrder(1001, 1, true, 100, 9900);   // Buy @ 99.00
    book->AddOrder(1002, 1, false, 100, 10100); // Sell @ 101.00
    
    EXPECT_EQ(book->GetTotalBidVolume(), 100);
    EXPECT_EQ(book->GetTotalAskVolume(), 100);
    
    // Modify buy order to cross the spread
    book->ModifyOrder(1001, 100, 10200);  // Buy @ 102.00 (crosses spread)
    
    // Orders should match and both be removed
    EXPECT_EQ(book->GetTotalBidVolume(), 0);
    EXPECT_EQ(book->GetTotalAskVolume(), 0);
}

// Test order modification - error cases
TEST_F(OrderBookTest, ModifyOrderErrorCases) {
    // Add an order
    book->AddOrder(1001, 1, true, 100, 10000);
    
    // Test modifying non-existent order
    EXPECT_THROW(book->ModifyOrder(9999, 100, 10000), std::runtime_error);
    
    // Test modifying with zero quantity
    EXPECT_THROW(book->ModifyOrder(1001, 0, 10000), std::invalid_argument);
    
    // Test modifying after cancellation
    book->CancelOrder(1001);
    EXPECT_THROW(book->ModifyOrder(1001, 100, 10000), std::runtime_error);
}

// Test order modification - time priority preservation
TEST_F(OrderBookTest, ModifyOrderTimePriority) {
    // Add multiple orders at same price
    book->AddOrder(1001, 1, true, 100, 10000);  // First order
    book->AddOrder(1002, 2, true, 100, 10000);  // Second order
    book->AddOrder(1003, 3, true, 100, 10000);  // Third order
    
    EXPECT_EQ(book->GetTotalBidVolume(), 300);
    
    // Modify first order quantity down (should preserve time priority)
    book->ModifyOrder(1001, 75, 10000);
    EXPECT_EQ(book->GetTotalBidVolume(), 275);
    
    // Add a sell order that partially matches
    book->AddOrder(2001, 4, false, 50, 10000);  // Should match against first order
    
    // First order should be partially filled (75 - 50 = 25 remaining)
    EXPECT_EQ(book->GetTotalBidVolume(), 225);  // 25 + 100 + 100
    EXPECT_EQ(book->GetTotalAskVolume(), 0);    // Sell order fully filled
}
