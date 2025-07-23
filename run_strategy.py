import os
import sys

import matplotlib as matplotlib

matplotlib.use('Qt5Agg')
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# Add the build directory to the Python path to find the compiled module
build_dir = os.path.join(os.path.dirname(__file__), 'build')
sys.path.append(build_dir)

try:
    import cpp_order_book_engine as obe
except ImportError:
    print("Error: Could not import the 'cpp_order_book_engine' module.")
    print(f"Please ensure it has been compiled and is in the '{build_dir}' directory.")
    sys.exit(1)

def main():
    print("--- C++/Python Hybrid Trading Strategy ---")
    
    log_file = "engine.log"
    obe.set_log_file(log_file)
    print(f"C++ engine output is being logged to: {log_file}")

    # 1. Instantiate the C++ engine components
    # Note that a value of 100 for initial cash corresponds to 1 dollar
    manager = obe.OrderBookManager(tracked_user_id=9999, max_leverage=2.0, initial_cash=10000000.0)
    strategy = obe.Strategy("MyMeanReversionStrategy")

    # 2. Configure the strategy from a TOML file
    with open("strategy_config.toml", "r") as f:
        toml_config = f.read()
    strategy.from_toml_string(toml_config)
    print("Strategy configuration loaded from strategy_config.toml")

    # 3. Configure the C++ engine
    manager.set_strategy(strategy)
    manager.initialize_after_construction()

    print("Starting the historical data demo...")
    manager.run_historical_data_demo()
    print("Historical data demo finished.")

    # 5. Analyze the results
    data_sink = manager.get_data_sink()
    portfolio_snapshots = data_sink.get_portfolio_snapshots()
    tob_snapshots = data_sink.get_tob_snapshots()
    trades = data_sink.get_trades()

    print("\n--- Analysis ---")
    print(f"Total portfolio snapshots: {len(portfolio_snapshots)}")
    if portfolio_snapshots:
        print(f"  - Last portfolio snapshot: Position = {portfolio_snapshots[-1].position}, Total PnL = {portfolio_snapshots[-1].total_pnl}")

    print(f"Total TOB snapshots: {len(tob_snapshots)}")
    if tob_snapshots:
        print(f"  - Last TOB snapshot: Mid-price = {tob_snapshots[-1].mid_price}")

    indicator_snapshots = data_sink.get_indicator_snapshots()
    print(f"Total indicator snapshots: {len(indicator_snapshots)}")
    if indicator_snapshots:
        print(f"  - Last indicator snapshot: Headers = {indicator_snapshots[-1].headers}, Values = {indicator_snapshots[-1].values}")

    print(f"Total trades executed: {len(trades)}")
    for trade in trades:
        print(f"  - {trade}")
    #Note that values are printed in cents, not dollars
    # Convert to pandas DataFrames
    portfolio_df = pd.DataFrame([{
        'timestamp': pd.to_datetime(p.timestamp),
        'position': p.position,
        'current_price': p.current_price,
        'average_cost': p.average_cost,
        'unrealized_pnl': p.unrealized_pnl,
        'realized_pnl': p.realized_pnl,
        'total_pnl': p.total_pnl,
        'total_trades': p.total_trades,
        'position_value': p.position_value,
        'cash_balance': p.cash_balance
    } for p in portfolio_snapshots])
    indicator_data = []
    for i in indicator_snapshots:
        row = {'timestamp': pd.to_datetime(i.timestamp)}
        for h, v in zip(i.headers, i.values):
            row[h] = v
        indicator_data.append(row)
    indicator_df = pd.DataFrame(indicator_data)
    # Plotting
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, sharex=True, figsize=(15, 12))

    # Plot 1: Total PnL
    ax1.plot(portfolio_df['timestamp'], portfolio_df['total_pnl'], label='Total PnL', color='blue')
    ax1.set_ylabel('Total PnL (Cents)')
    ax1.set_title('Portfolio PnL and Indicator Values Over Time')
    ax1.legend()
    ax1.grid(True)

    # Plot 2: EMA Indicators
    for column in ['ema_fast', 'ema_slow']:
        if column in indicator_df.columns:
            indicator_values = indicator_df[column].replace(0, np.nan)
            ax2.plot(indicator_df['timestamp'], indicator_values, label=column)
    ax2.set_ylabel('EMA Values')
    ax2.legend()
    ax2.grid(True)

    # Plot 3: RSI Indicator
    if 'rsi' in indicator_df.columns:
        rsi_values = indicator_df['rsi'].replace(0, np.nan)
        ax3.plot(indicator_df['timestamp'], rsi_values, label='RSI', color='green')
        ax3.set_ylabel('RSI')
        ax3.legend()
        ax3.grid(True)

    ax3.set_xlabel('Timestamp')
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()