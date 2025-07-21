import os
import sys

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
    manager = obe.OrderBookManager(tracked_user_id=9999)
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
    portfolio_manager = manager.get_portfolio_manager()
    trades = portfolio_manager.get_trades()
    print("\n--- Analysis ---")
    print(f"Total trades executed: {len(trades)}")
    for trade in trades:
        print(f"  - {trade}")

if __name__ == "__main__":
    main()