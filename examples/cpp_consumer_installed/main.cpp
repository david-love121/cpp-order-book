#include <iostream>
#include <vector>
#include <iomanip>

// Include the OrderBook headers - this assumes the library is installed
#include "OrderBook.h"
#include "Trade.h"
#include "PriceLevel.h"
#include "Order.h"

void printOrderBookStatus(const OrderBook& book) {
    std::cout << "\n=== Order Book Status ===" << std::endl;
    std::cout << "Best Bid: " << (book.GetBestBid() == 0 ? "N/A" : std::to_string(book.GetBestBid())) << std::endl;
    std::cout << "Best Ask: " << (book.GetBestAsk() == 0 ? "N/A" : std::to_string(book.GetBestAsk())) << std::endl;
    std::cout << "Total Bid Volume: " << book.GetTotalBidVolume() << std::endl;
    std::cout << "Total Ask Volume: " << book.GetTotalAskVolume() << std::endl;
    std::cout << "=========================" << std::endl;
}

int main() {
    std::cout << "Order Book Installed Consumer Example" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    // Create an order book instance
    OrderBook book;
    
    std::cout << "\n1. Adding initial sell orders (asks)..." << std::endl;
    
    try {
        // Add some sell orders first
        book.AddOrder(1001, 1, false, 100, 10150);  // Sell 100 @ 101.50
        book.AddOrder(1002, 1, false, 75, 10200);   // Sell 75 @ 102.00
        book.AddOrder(1003, 1, false, 120, 10100);  // Sell 120 @ 101.00
        
        printOrderBookStatus(book);
        
        std::cout << "\n2. Adding buy orders (bids)..." << std::endl;
        
        // Add some buy orders
        book.AddOrder(2001, 2, true, 80, 10050);    // Buy 80 @ 100.50
        book.AddOrder(2002, 2, true, 60, 10000);    // Buy 60 @ 100.00
        book.AddOrder(2003, 2, true, 90, 10075);    // Buy 90 @ 100.75
        
        printOrderBookStatus(book);
        
        std::cout << "\n3. Adding aggressive buy order that crosses spread..." << std::endl;
        
        // This order should match against the best ask
        book.AddOrder(3001, 3, true, 50, 10100);    // Buy 50 @ 101.00 (matches sell order 1003)
        
        printOrderBookStatus(book);
        
        std::cout << "\n4. Cancelling an order..." << std::endl;
        
        // Cancel one of the remaining orders
        book.CancelOrder(2002);  // Cancel buy order for 60 @ 100.00
        
        printOrderBookStatus(book);
        
        std::cout << "\n5. Modifying an order..." << std::endl;
        
        // Modify an existing order
        book.ModifyOrder(2001, 100, 10060);  // Change order 2001 to 100 @ 100.60
        
        printOrderBookStatus(book);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n=== Example completed successfully! ===" << std::endl;
    std::cout << "\nThis example demonstrates:" << std::endl;
    std::cout << "- Using the installed OrderBook library with find_package()" << std::endl;
    std::cout << "- Creating an OrderBook instance" << std::endl;
    std::cout << "- Adding buy and sell orders" << std::endl;
    std::cout << "- Order matching and trade execution" << std::endl;
    std::cout << "- Order cancellation" << std::endl;
    std::cout << "- Order modification" << std::endl;
    std::cout << "- Retrieving order book statistics" << std::endl;
    
    return 0;
}
