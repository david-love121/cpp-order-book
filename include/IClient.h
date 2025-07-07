#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <string>

// Forward declarations
struct Trade;
struct Order;

/**
 * @brief Interface for order book clients
 * 
 * This interface defines the contract for clients that interact with the order book.
 * It provides methods for order lifecycle management (add, cancel, modify) and 
 * callbacks for receiving market data updates and trade confirmations.
 * 
 * Example implementations might include:
 * - Simulated trading clients for testing
 * - Market data feed processors 
 * - Risk management systems
 * - Trading strategy engines
 */
class IClient {
public:
    virtual ~IClient() = default;

    // ========== Order Management Interface ==========

    /**
     * @brief Submit a new order to the order book
     * @param user_id User identifier submitting the order
     * @param is_buy true for buy orders, false for sell orders
     * @param quantity Order quantity
     * @param price Order price in ticks
     * @param ts_received Timestamp when order was received (nanoseconds)
     * @param ts_executed Timestamp when order should be executed (nanoseconds)
     * @return Order ID assigned to the new order
     */
    virtual uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price, 
                                uint64_t ts_received, uint64_t ts_executed) = 0;

    /**
     * @brief Submit a new order to the order book (legacy version without timestamps)
     * @deprecated Use the version with timestamps for historical accuracy
     * @param user_id User identifier submitting the order
     * @param is_buy true for buy orders, false for sell orders
     * @param quantity Order quantity
     * @param price Order price in ticks
     * @return Order ID assigned to the new order
     */
    virtual uint64_t SubmitOrder(uint64_t user_id, bool is_buy, uint64_t quantity, uint64_t price) = 0;

    /**
     * @brief Cancel an existing order
     * @param order_id Order ID to cancel
     * @return true if cancellation was successful, false otherwise
     */
    virtual bool CancelOrder(uint64_t order_id) = 0;

    /**
     * @brief Modify an existing order
     * @param order_id Order ID to modify
     * @param new_quantity New quantity for the order
     * @param new_price New price for the order
     * @return true if modification was successful, false otherwise
     */
    virtual bool ModifyOrder(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) = 0;

    // ========== Market Data Interface ==========

    /**
     * @brief Get current best bid price
     * @return Best bid price in ticks, or 0 if no bids
     */
    virtual uint64_t GetBestBid() const = 0;

    /**
     * @brief Get current best ask price
     * @return Best ask price in ticks, or 0 if no asks
     */
    virtual uint64_t GetBestAsk() const = 0;

    /**
     * @brief Get total volume on bid side
     * @return Total bid volume across all price levels
     */
    virtual uint64_t GetTotalBidVolume() const = 0;

    /**
     * @brief Get total volume on ask side
     * @return Total ask volume across all price levels
     */
    virtual uint64_t GetTotalAskVolume() const = 0;

    /**
     * @brief Get current mid price (average of best bid and ask)
     * @return Mid price in ticks, or 0 if no market exists
     */
    virtual uint64_t GetMidPrice() const = 0;

    /**
     * @brief Get current spread (difference between best ask and bid)
     * @return Spread in ticks, or 0 if no market exists
     */
    virtual uint64_t GetSpread() const = 0;

    // ========== Event Callbacks ==========

    /**
     * @brief Callback for trade executions
     * 
     * Called whenever a trade occurs involving this client's orders.
     * Implementations should handle trade confirmations, position updates,
     * P&L calculations, etc.
     * 
     * @param trade Trade details including prices, quantities, and participants
     */
    virtual void OnTradeExecuted(const Trade& trade) = 0;

    /**
     * @brief Callback for order acknowledgments
     * 
     * Called when an order is successfully added to the book.
     * 
     * @param order_id Order ID that was acknowledged
     */
    virtual void OnOrderAcknowledged(uint64_t order_id) = 0;

    /**
     * @brief Callback for order cancellations
     * 
     * Called when an order is successfully cancelled.
     * 
     * @param order_id Order ID that was cancelled
     */
    virtual void OnOrderCancelled(uint64_t order_id) = 0;

    /**
     * @brief Callback for order modifications
     * 
     * Called when an order is successfully modified.
     * 
     * @param order_id Order ID that was modified
     * @param new_quantity New quantity after modification
     * @param new_price New price after modification
     */
    virtual void OnOrderModified(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) = 0;

    /**
     * @brief Callback for order rejections
     * 
     * Called when an order operation (add/cancel/modify) is rejected.
     * 
     * @param order_id Order ID that was rejected (0 for new order rejections)
     * @param reason Human-readable reason for rejection
     */
    virtual void OnOrderRejected(uint64_t order_id, const std::string& reason) = 0;

    // ========== Market Data Callbacks ==========

    /**
     * @brief Callback for top-of-book updates
     * 
     * Called whenever the best bid or ask changes.
     * 
     * @param best_bid Current best bid price (0 if none)
     * @param best_ask Current best ask price (0 if none)
     * @param bid_volume Total volume at best bid level
     * @param ask_volume Total volume at best ask level
     */
    virtual void OnTopOfBookUpdate(uint64_t best_bid, uint64_t best_ask, 
                                   uint64_t bid_volume, uint64_t ask_volume) = 0;

    // ========== Client Lifecycle ==========

    /**
     * @brief Initialize the client
     * 
     * Called when the client is first connected to the order book.
     * Use this for setup, configuration, initial state, etc.
     */
    virtual void Initialize() = 0;

    /**
     * @brief Shutdown the client
     * 
     * Called when the client is being disconnected.
     * Use this for cleanup, final reporting, etc.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Get client identifier
     * @return Unique identifier for this client instance
     */
    virtual uint64_t GetClientId() const = 0;

    /**
     * @brief Get client name/description
     * @return Human-readable name for this client
     */
    virtual std::string GetClientName() const = 0;
};
