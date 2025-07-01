#include <gtest/gtest.h>
#include <memory>

#include "PriceLevel.h"
#include "Order.h"
#include "Trade.h"

class PriceLevelTest : public ::testing::Test {
protected:
    void SetUp() override {
        price_level = std::make_unique<PriceLevel>();
        
        // Create test orders
        order1 = std::make_unique<Order>();
        order1->order_id = 1001;
        order1->user_id = 1;
        order1->is_buy_side = true;
        order1->quantity = 100;
        order1->price = 10000;
        order1->timestamp = 1000;
        order1->parent_price_level = nullptr;
        order1->position_in_list = {}; // Will be set when added to price level
        
        order2 = std::make_unique<Order>();
        order2->order_id = 1002;
        order2->user_id = 2;
        order2->is_buy_side = true;
        order2->quantity = 150;
        order2->price = 10000;
        order2->timestamp = 2000;
        order2->parent_price_level = nullptr;
        order2->position_in_list = {}; // Will be set when added to price level
        
        order3 = std::make_unique<Order>();
        order3->order_id = 1003;
        order3->user_id = 3;
        order3->is_buy_side = true;
        order3->quantity = 200;
        order3->price = 10000;
        order3->timestamp = 3000;
        order3->parent_price_level = nullptr;
        order3->position_in_list = {}; // Will be set when added to price level
    }

    void TearDown() override {
        price_level.reset();
        order1.reset();
        order2.reset();
        order3.reset();
    }

    std::unique_ptr<PriceLevel> price_level;
    std::unique_ptr<Order> order1;
    std::unique_ptr<Order> order2;
    std::unique_ptr<Order> order3;
};

// Test initial state of price level
TEST_F(PriceLevelTest, InitialState) {
    EXPECT_EQ(price_level->GetTotalVolume(), 0);
    EXPECT_EQ(price_level->GetPrice(), 0);
    EXPECT_EQ(price_level->GetTopOrder(), nullptr);
}

// Test adding a single order
TEST_F(PriceLevelTest, AddSingleOrder) {
    price_level->AddOrder(order1.get());
    
    EXPECT_EQ(price_level->GetTotalVolume(), 100);
    EXPECT_EQ(price_level->GetTopOrder(), order1.get());
    EXPECT_EQ(order1->parent_price_level, price_level.get());
}

// Test adding multiple orders (FIFO queue)
TEST_F(PriceLevelTest, AddMultipleOrders) {
    price_level->AddOrder(order1.get());
    price_level->AddOrder(order2.get());
    price_level->AddOrder(order3.get());
    
    EXPECT_EQ(price_level->GetTotalVolume(), 450);  // 100 + 150 + 200
    EXPECT_EQ(price_level->GetTopOrder(), order1.get());  // First order should be on top
    
    // Verify all orders have correct parent
    EXPECT_EQ(order1->parent_price_level, price_level.get());
    EXPECT_EQ(order2->parent_price_level, price_level.get());
    EXPECT_EQ(order3->parent_price_level, price_level.get());
}

// Test removing an order
TEST_F(PriceLevelTest, RemoveOrder) {
    price_level->AddOrder(order1.get());
    price_level->AddOrder(order2.get());
    price_level->AddOrder(order3.get());
    
    EXPECT_EQ(price_level->GetTotalVolume(), 450);
    
    // Remove middle order
    price_level->RemoveOrder(order2.get());
    
    EXPECT_EQ(price_level->GetTotalVolume(), 300);  // 100 + 200
    EXPECT_EQ(price_level->GetTopOrder(), order1.get());  // First order still on top
}

// Test removing the top order
TEST_F(PriceLevelTest, RemoveTopOrder) {
    price_level->AddOrder(order1.get());
    price_level->AddOrder(order2.get());
    price_level->AddOrder(order3.get());
    
    // Remove the top order
    price_level->RemoveOrder(order1.get());
    
    EXPECT_EQ(price_level->GetTotalVolume(), 350);  // 150 + 200
    EXPECT_EQ(price_level->GetTopOrder(), order2.get());  // Second order becomes top
}

// Test removing all orders
TEST_F(PriceLevelTest, RemoveAllOrders) {
    price_level->AddOrder(order1.get());
    price_level->AddOrder(order2.get());
    price_level->AddOrder(order3.get());
    
    price_level->RemoveOrder(order1.get());
    price_level->RemoveOrder(order2.get());
    price_level->RemoveOrder(order3.get());
    
    EXPECT_EQ(price_level->GetTotalVolume(), 0);
    EXPECT_EQ(price_level->GetTopOrder(), nullptr);
}

// Test partial fill of top order
TEST_F(PriceLevelTest, PartialFillTopOrder) {
    price_level->AddOrder(order1.get());  // 100 quantity
    price_level->AddOrder(order2.get());  // 150 quantity
    
    // Create incoming order for partial fill
    Order incoming_order;
    incoming_order.order_id = 9999;
    incoming_order.user_id = 99;
    incoming_order.is_buy_side = false;  // Opposite side
    incoming_order.quantity = 50;       // Partial fill
    incoming_order.price = 10000;
    incoming_order.timestamp = 5000;
    
    std::vector<Trade> trades = price_level->FillOrder(&incoming_order, 50);
    
    // Should generate one trade
    EXPECT_EQ(trades.size(), 1);
    if (!trades.empty()) {
        EXPECT_EQ(trades[0].quantity, 50);
        EXPECT_EQ(trades[0].price, 10000);
        EXPECT_EQ(trades[0].aggressor_order_id, incoming_order.order_id);
        EXPECT_EQ(trades[0].resting_order_id, order1->order_id);
    }
    
    // Top order should have reduced quantity
    EXPECT_EQ(order1->quantity, 50);  // 100 - 50
    EXPECT_EQ(price_level->GetTotalVolume(), 200);  // 50 + 150
    EXPECT_EQ(price_level->GetTopOrder(), order1.get());  // Same order still on top
}

// Test complete fill of top order
TEST_F(PriceLevelTest, CompleteFillTopOrder) {
    price_level->AddOrder(order1.get());  // 100 quantity
    price_level->AddOrder(order2.get());  // 150 quantity
    
    // Create incoming order for complete fill of top order
    Order incoming_order;
    incoming_order.order_id = 9999;
    incoming_order.user_id = 99;
    incoming_order.is_buy_side = false;
    incoming_order.quantity = 100;
    incoming_order.price = 10000;
    incoming_order.timestamp = 5000;
    
    std::vector<Trade> trades = price_level->FillOrder(&incoming_order, 100);
    
    // Should generate one trade
    EXPECT_EQ(trades.size(), 1);
    if (!trades.empty()) {
        EXPECT_EQ(trades[0].quantity, 100);
        EXPECT_EQ(trades[0].aggressor_order_id, incoming_order.order_id);
        EXPECT_EQ(trades[0].resting_order_id, order1->order_id);
    }
    
    // Top order should be completely filled and removed
    EXPECT_EQ(price_level->GetTotalVolume(), 150);  // Only order2 remains
    EXPECT_EQ(price_level->GetTopOrder(), order2.get());  // order2 becomes top
}

// Test fill quantity larger than top order
TEST_F(PriceLevelTest, FillQuantityLargerThanTopOrder) {
    price_level->AddOrder(order1.get());  // 100 quantity
    price_level->AddOrder(order2.get());  // 150 quantity
    
    // Create incoming order that requires multiple fills
    Order incoming_order;
    incoming_order.order_id = 9999;
    incoming_order.user_id = 99;
    incoming_order.is_buy_side = false;
    incoming_order.quantity = 200;  // More than first order
    incoming_order.price = 10000;
    incoming_order.timestamp = 5000;
    
    std::vector<Trade> trades = price_level->FillOrder(&incoming_order, 200);
    
    // Should generate two trades
    EXPECT_EQ(trades.size(), 2);
    if (trades.size() >= 2) {
        // First trade: complete fill of order1
        EXPECT_EQ(trades[0].quantity, 100);
        EXPECT_EQ(trades[0].resting_order_id, order1->order_id);
        
        // Second trade: partial fill of order2
        EXPECT_EQ(trades[1].quantity, 100);  // 200 - 100
        EXPECT_EQ(trades[1].resting_order_id, order2->order_id);
    }
    
    // order2 should have reduced quantity and be on top
    EXPECT_EQ(order2->quantity, 50);  // 150 - 100
    EXPECT_EQ(price_level->GetTotalVolume(), 50);
    EXPECT_EQ(price_level->GetTopOrder(), order2.get());
}

// Test fill when price level becomes empty
TEST_F(PriceLevelTest, FillUntilEmpty) {
    price_level->AddOrder(order1.get());  // 100 quantity
    
    Order incoming_order;
    incoming_order.order_id = 9999;
    incoming_order.user_id = 99;
    incoming_order.is_buy_side = false;
    incoming_order.quantity = 100;
    incoming_order.price = 10000;
    incoming_order.timestamp = 5000;
    
    std::vector<Trade> trades = price_level->FillOrder(&incoming_order, 100);
    
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(price_level->GetTotalVolume(), 0);
    EXPECT_EQ(price_level->GetTopOrder(), nullptr);
}

// Test that orders maintain time priority (FIFO)
TEST_F(PriceLevelTest, TimePriorityFIFO) {
    // Add orders in chronological order
    price_level->AddOrder(order1.get());  // timestamp 1000
    price_level->AddOrder(order2.get());  // timestamp 2000
    price_level->AddOrder(order3.get());  // timestamp 3000
    
    // Fill part of the level
    Order incoming_order;
    incoming_order.order_id = 9999;
    incoming_order.user_id = 99;
    incoming_order.is_buy_side = false;
    incoming_order.quantity = 150;  // Should fill order1 completely and part of order2
    incoming_order.price = 10000;
    incoming_order.timestamp = 5000;
    
    std::vector<Trade> trades = price_level->FillOrder(&incoming_order, 150);
    
    // Should have two trades in time priority order
    EXPECT_EQ(trades.size(), 2);
    if (trades.size() >= 2) {
        // First trade should be with order1 (earliest timestamp)
        EXPECT_EQ(trades[0].resting_order_id, order1->order_id);
        EXPECT_EQ(trades[0].quantity, 100);
        
        // Second trade should be with order2
        EXPECT_EQ(trades[1].resting_order_id, order2->order_id);
        EXPECT_EQ(trades[1].quantity, 50);
    }
    
    // order2 should be on top with reduced quantity
    EXPECT_EQ(price_level->GetTopOrder(), order2.get());
    EXPECT_EQ(order2->quantity, 100);  // 150 - 50
}

// Test adding order with null pointer
TEST_F(PriceLevelTest, AddNullOrder) {
    // This should throw an exception
    EXPECT_THROW(price_level->AddOrder(nullptr), std::invalid_argument);
    EXPECT_EQ(price_level->GetTotalVolume(), 0);
}

// Test removing order that's not in the level
TEST_F(PriceLevelTest, RemoveOrderNotInLevel) {
    price_level->AddOrder(order1.get());
    
    // Try to remove an order that wasn't added
    EXPECT_THROW(price_level->RemoveOrder(order2.get()), std::runtime_error);
    EXPECT_EQ(price_level->GetTotalVolume(), 100);  // Should remain unchanged
}
