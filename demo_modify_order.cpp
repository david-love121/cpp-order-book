#include "OrderBook.h"
#include "Order.h"
#include "PriceLevel.h"
#include <iostream>
#include <iomanip>

void printOrderBookStatus(const OrderBook& book, const std::string& description) {
    std::cout << "\n" << description << ":\n";
    std::cout << "  Best Bid: " << (book.GetBestBid() == 0 ? "N/A" : std::to_string(book.GetBestBid())) 
              << " (Volume: " << book.GetTotalBidVolume() << ")\n";
    std::cout << "  Best Ask: " << (book.GetBestAsk() == 0 ? "N/A" : std::to_string(book.GetBestAsk())) 
              << " (Volume: " << book.GetTotalAskVolume() << ")\n";
}

int main() {
    std::cout << "=== OrderBook ModifyOrder Demonstration ===\n";
    
    OrderBook book;
    
    // Setup initial order book
    std::cout << "\n1. Setting up initial orders:\n";
    book.AddOrder(1001, 1, true, 100, 9900);   // Buy 100 @ 99.00
    book.AddOrder(1002, 2, true, 200, 9850);   // Buy 200 @ 98.50
    book.AddOrder(2001, 3, false, 150, 10100); // Sell 150 @ 101.00
    book.AddOrder(2002, 4, false, 100, 10200); // Sell 100 @ 102.00
    printOrderBookStatus(book, "Initial order book state");
    
    // Test 1: Modify quantity down
    std::cout << "\n2. Modifying order 1001 quantity from 100 to 75:\n";
    book.ModifyOrder(1001, 75, 9900);
    printOrderBookStatus(book, "After quantity reduction");
    
    // Test 2: Modify quantity up
    std::cout << "\n3. Modifying order 1001 quantity from 75 to 125:\n";
    book.ModifyOrder(1001, 125, 9900);
    printOrderBookStatus(book, "After quantity increase");
    
    // Test 3: Modify price (no matching)
    std::cout << "\n4. Modifying order 1001 price from 99.00 to 99.50:\n";
    book.ModifyOrder(1001, 125, 9950);
    printOrderBookStatus(book, "After price increase (no match)");
    
    // Test 4: Modify price causing a match
    std::cout << "\n5. Modifying order 1001 to cross the spread (price 102.50):\n";
    book.ModifyOrder(1001, 125, 10250);  // This should match against sells
    printOrderBookStatus(book, "After matching modification");
    
    // Test 5: Add more orders and test complex modification
    std::cout << "\n6. Adding more orders and testing complex scenarios:\n";
    book.AddOrder(3001, 5, true, 50, 9800);    // Buy 50 @ 98.00
    book.AddOrder(3002, 6, false, 75, 10150);  // Sell 75 @ 101.50
    printOrderBookStatus(book, "After adding more orders");
    
    // Modify sell order to better price (should trigger match)
    std::cout << "\n7. Modifying sell order 3002 to price 98.00 (should cause full match):\n";
    book.ModifyOrder(3002, 75, 9800);
    printOrderBookStatus(book, "After aggressive price modification");
    
    std::cout << "\n=== ModifyOrder demonstration complete! ===\n";
    
    return 0;
}
