import argparse
import os
import sys

import databento as db
import matplotlib as matplotlib
import pyarrow as pa
import pyarrow.parquet as pq
import yfinance as yf
from dotenv import load_dotenv

matplotlib.use('Qt5Agg')
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# Add the build directory to the Python path to find the compiled module
build_dir = os.path.join(os.path.dirname(__file__), 'build')
sys.path.append(build_dir)

try:
    import cpp_order_book_engine as obe
except ImportError:
    print("Error: Could not import the 'cpp_order_book_engine' module.")
    print(f"Please ensure it has been compiled and is in the '{build_dir}' directory.")
    sys.exit(1)

def fetch_and_cache_data(ticker, period="1d", interval="1m"):
    """
    Fetches historical data from yfinance and caches it in a Parquet file.
    """
    cache_dir = "yfinance_cache"
    if not os.path.exists(cache_dir):
        os.makedirs(cache_dir)

    file_path = os.path.join(cache_dir, f"{ticker}_{period}_{interval}.parquet")

    if os.path.exists(file_path):
        print(f"Loading data from cache: {file_path}")
        return file_path

    print(f"Fetching data for {ticker} from yfinance...")
    data = yf.download(ticker, period=period, interval=interval)
    
    if data.empty:
        raise ValueError("No data fetched from yfinance. Check ticker and parameters.")

    data.columns = data.columns.droplevel(1)
    data = data.reset_index()
    data['ticker'] = ticker
    data['data_type'] = 'ohlcv'
    print(f"Columns: {data.columns}")
    # Convert to Arrow Table and save as Parquet
  
    table = pa.Table.from_pandas(data)
    pq.write_table(table, file_path)
    print(f"Data cached to {file_path}")
    
    return file_path

def fetch_and_cache_databento_data(dataset, start_time, end_time, symbols):
    """
    Fetches historical data from Databento and caches it in a Parquet file.
    """
    cache_dir = "databento_cache"
    if not os.path.exists(cache_dir):
        os.makedirs(cache_dir)

    file_path = os.path.join(cache_dir, f"{dataset}_{start_time}_{end_time}_{'_'.join(symbols)}.parquet")

    if os.path.exists(file_path):
        print(f"Loading data from cache: {file_path}")
        return file_path

    print(f"Fetching data for {symbols} from Databento...")
    client = db.Historical()
    
    cost = client.metadata.get_cost(
        dataset=dataset,
        start=start_time,
        end=end_time,
        symbols=symbols,
        schema="mbp-1",
    )
    
    print(f"This query will cost ${cost:.2f}. Do you want to continue? (y/n)")
    if input().lower() != "y":
        sys.exit(0)

    data = client.timeseries.get_range(
        dataset=dataset,
        start=start_time,
        end=end_time,
        symbols=symbols,
        schema="mbp-1",
    ).to_df()

    if data.empty:
        raise ValueError("No data fetched from Databento. Check parameters.")
    print(data.dtypes)
    print(data)
    data = data.reset_index()
    data['ticker'] = data['symbol']
    data['data_type'] = 'mbp-1'
    print(f"Columns: {data.columns}")
    # Convert to Arrow Table and save as Parquet
    table = pa.Table.from_pandas(data)
    pq.write_table(table, file_path)
    print(f"Data cached to {file_path}")
    
    return file_path

def main():
    load_dotenv()
    print("--- C++/Python Hybrid Trading Strategy ---")
    
    log_file = "engine.log"
    obe.set_log_file(log_file)
    print(f"C++ engine output is being logged to: {log_file}")

    # 1. Fetch and cache data
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", choices=["yfinance", "databento"], help="Select data source")
    args = parser.parse_args()

    data_source = args.source
    if not data_source:
        data_source = input("Select data source (yfinance/databento): ").lower()

    try:
        if data_source == "yfinance":
            data_path = fetch_and_cache_data("AAPL", period="1d", interval="1m")
        elif data_source == "databento":
            data_path = fetch_and_cache_databento_data(
                dataset="GLBX.MDP3",
                start_time="2024-06-28T15:30",
                end_time="2024-06-28T15:35",
                symbols=["ESU4"],
            )
        else:
            print("Invalid data source selected.")
            sys.exit(1)
    except Exception as e:
        print(f"Error fetching data: {e}")
        sys.exit(1)

    # 2. Instantiate the C++ engine components
    # Note that a value of 100 for initial cash corresponds to 1 dollar
    manager = obe.OrderBookManager(tracked_user_id=9999, max_leverage=2.0, initial_cash=10000000.0)
    strategy = obe.Strategy("MyMeanReversionStrategy")

    # 3. Configure the strategy from a TOML file
    with open("strategy_config.toml", "r") as f:
        toml_config = f.read()
    strategy.from_toml_string(toml_config)
    print("Strategy configuration loaded from strategy_config.toml")

    # 4. Configure the C++ engine
    manager.set_strategy(strategy)
    manager.initialize_after_construction()

    print("Starting the backtest...")
    manager.run_backtest(data_path) # This will be the new method
    print("Backtest finished.")

    # 5. Analyze the results
    data_sink = manager.get_data_sink()
    portfolio_snapshots = data_sink.get_portfolio_snapshots()
    tob_snapshots = data_sink.get_tob_snapshots()
    trades = data_sink.get_trades()
    rule_evaluations = data_sink.get_rule_evaluations()

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

    print(f"Total rule evaluations: {len(rule_evaluations)}")
    if rule_evaluations:
        print(f"  - Last rule evaluation: Name = {rule_evaluations[-1].rule_name}, Satisfied = {rule_evaluations[-1].is_satisfied}")

    print(f"Total trades executed: {len(trades)}")
    for trade in trades:
        print(f"  - {trade}")
    #Note that values are printed in cents, not dollars
    # Convert to pandas DataFrames
    if not portfolio_snapshots:
        print("No portfolio snapshots to analyze.")
        return

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

    rule_df = pd.DataFrame([{
        'timestamp': pd.to_datetime(r.timestamp),
        'rule_name': r.rule_name,
        'is_satisfied': r.is_satisfied
    } for r in rule_evaluations])

    # Plotting
    fig, (ax1, ax2, ax3, ax4) = plt.subplots(4, 1, sharex=True, figsize=(15, 16))

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
    ax3.grid(True)

    # Plot 4: Rule Evaluations
    for rule_name, group in rule_df.groupby('rule_name'):
        satisfied = group[group['is_satisfied']]
        ax4.scatter(satisfied['timestamp'], satisfied['is_satisfied'], label=rule_name)
    ax4.set_ylabel('Rule Satisfied')
    ax4.set_yticks([1])
    ax4.set_yticklabels(['True'])
    ax4.legend()
    ax4.grid(True)

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()