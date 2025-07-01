#include <gtest/gtest.h>
#include "OrderBook.h"
#include "Order.h"
#include "Trade.h"

// Test MBO-style operations that would come from Databento
class MBOIntegrationTest : public ::testing::Test {
protected:
    OrderBook order_book;
    
    void SetUp() override {
        // Setup a clean order book for each test
    }
    
    void TearDown() override {
        // Cleanup after each test
    }
};

// Test MBO Add operations
TEST_F(MBOIntegrationTest, MBOAddOperations) {
    // Simulate MBO Add messages
    uint64_t order_id_1 = 100001;
    uint64_t order_id_2 = 100002;
    uint64_t order_id_3 = 100003;
    
    // Add bid orders (buy side)
    order_book.AddOrder(order_id_1, 1, true, 100, 415000);  // Buy 100 @ 4150.00
    order_book.AddOrder(order_id_2, 2, true, 200, 414975);  // Buy 200 @ 4149.75
    
    // Add ask orders (sell side)  
    order_book.AddOrder(order_id_3, 3, false, 150, 415025); // Sell 150 @ 4150.25
    
    // Verify order book state
    EXPECT_EQ(order_book.GetBestBid(), 415000);
    EXPECT_EQ(order_book.GetBestAsk(), 415025);
    EXPECT_EQ(order_book.GetTotalBidVolume(), 300);
    EXPECT_EQ(order_book.GetTotalAskVolume(), 150);
}

// Test MBO Cancel operations
TEST_F(MBOIntegrationTest, MBOCancelOperations) {
    // Add some orders first
    uint64_t order_id_1 = 200001;
    uint64_t order_id_2 = 200002;
    
    order_book.AddOrder(order_id_1, 1, true, 100, 415000);   // Buy
    order_book.AddOrder(order_id_2, 2, false, 150, 415025); // Sell
    
    EXPECT_EQ(order_book.GetTotalBidVolume(), 100);
    EXPECT_EQ(order_book.GetTotalAskVolume(), 150);
    
    // Cancel the buy order
    order_book.CancelOrder(order_id_1);
    
    EXPECT_EQ(order_book.GetTotalBidVolume(), 0);
    EXPECT_EQ(order_book.GetTotalAskVolume(), 150);
    EXPECT_EQ(order_book.GetBestBid(), 0);  // No more bids
    
    // Cancel the sell order
    order_book.CancelOrder(order_id_2);
    
    EXPECT_EQ(order_book.GetTotalBidVolume(), 0);
    EXPECT_EQ(order_book.GetTotalAskVolume(), 0);
    EXPECT_EQ(order_book.GetBestAsk(), 0);  // No more asks
}

// Test MBO Modify operations
TEST_F(MBOIntegrationTest, MBOModifyOperations) {
    uint64_t order_id = 300001;
    
    // Add initial order
    order_book.AddOrder(order_id, 1, true, 100, 415000);  // Buy 100 @ 4150.00
    
    EXPECT_EQ(order_book.GetTotalBidVolume(), 100);
    EXPECT_EQ(order_book.GetBestBid(), 415000);
    
    // Modify quantity (increase)
    order_book.ModifyOrder(order_id, 200, 415000);  // Change to 200 @ 4150.00
    
    EXPECT_EQ(order_book.GetTotalBidVolume(), 200);
    EXPECT_EQ(order_book.GetBestBid(), 415000);
    
    // Modify price (move to better price)
    order_book.ModifyOrder(order_id, 200, 415025);  // Change to 200 @ 4150.25
    
    EXPECT_EQ(order_book.GetTotalBidVolume(), 200);
    EXPECT_EQ(order_book.GetBestBid(), 415025);
}

// Test realistic ES futures scenario
TEST_F(MBOIntegrationTest, ESFuturesScenario) {
    // Simulate realistic ES futures order book scenario
    // Prices in hundredths (4150.25 = 415025)
    
    // Add initial market depth
    order_book.AddOrder(1001, 101, true,  50, 415000);   // Buy  50 @ 4150.00
    order_book.AddOrder(1002, 102, true,  75, 414975);   // Buy  75 @ 4149.75
    order_book.AddOrder(1003, 103, true, 100, 414950);   // Buy 100 @ 4149.50
    
    order_book.AddOrder(2001, 201, false,  60, 415025);  // Sell  60 @ 4150.25
    order_book.AddOrder(2002, 202, false,  80, 415050);  // Sell  80 @ 4150.50
    order_book.AddOrder(2003, 203, false, 120, 415075);  // Sell 120 @ 4150.75
    
    // Verify initial state
    EXPECT_EQ(order_book.GetBestBid(), 415000);   // 4150.00
    EXPECT_EQ(order_book.GetBestAsk(), 415025);   // 4150.25
    EXPECT_EQ(order_book.GetTotalBidVolume(), 225);
    EXPECT_EQ(order_book.GetTotalAskVolume(), 260);
    
    // Simulate aggressive buy order that crosses spread
    order_book.AddOrder(3001, 301, true, 100, 415030);  // Buy 100 @ 4150.30
    
    // Should match against the 60 @ 4150.25 sell order
    // Remaining 40 should become new best bid
    EXPECT_EQ(order_book.GetBestBid(), 415030);   // New best bid from remaining quantity
    EXPECT_EQ(order_book.GetBestAsk(), 415050);   // Next ask level
    
    // Cancel a mid-level order
    order_book.CancelOrder(1002);  // Cancel 75 @ 4149.75
    
    // Total bid volume should decrease
    EXPECT_LT(order_book.GetTotalBidVolume(), 225 + 40);  // Less than original + remaining
}

// Test order matching with MBO data
TEST_F(MBOIntegrationTest, MBOOrderMatching) {
    // Setup order book
    order_book.AddOrder(1001, 1, false, 100, 415025);  // Sell 100 @ 4150.25
    order_book.AddOrder(1002, 2, false, 150, 415050);  // Sell 150 @ 4150.50
    
    EXPECT_EQ(order_book.GetTotalAskVolume(), 250);
    EXPECT_EQ(order_book.GetBestAsk(), 415025);
    
    // Add aggressive buy order
    order_book.AddOrder(2001, 3, true, 80, 415025);   // Buy 80 @ 4150.25 (exact match)
    
    // Should partially fill the first sell order
    EXPECT_EQ(order_book.GetTotalAskVolume(), 170);  // 250 - 80 = 170
    EXPECT_EQ(order_book.GetBestAsk(), 415025);      // Still same price level
    
    // Add another aggressive order to fully clear first level
    order_book.AddOrder(2002, 4, true, 20, 415025);   // Buy 20 @ 4150.25
    
    // Should fully clear first level and move to next
    EXPECT_EQ(order_book.GetTotalAskVolume(), 150);   // Only second level remains
    EXPECT_EQ(order_book.GetBestAsk(), 415050);       // Best ask moves to next level
}
