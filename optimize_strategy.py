import concurrent.futures
import itertools
import subprocess


def get_pnl_from_csv(filename="portfolio_1000.csv"):
    """
    Reads the portfolio CSV file and returns the final realized PnL.
    """
    with open(filename, "r") as f:
        lines = f.readlines()
        if len(lines) < 2:
            return 0.0
        last_line = lines[-1]
        if "realized_pnl" in last_line:
            return 0.0
        return float(last_line.split(",")[5])

def run_backtest(params):
    """
    Runs the C++ backtest with the given parameters.
    """
    # Command to run the backtest
    command = [
        "examples/cpp_consumer_fetchcontent/build/consumer_fetchcontent",
        "--imbalance-threshold", str(params["imbalance_threshold"]),
        "--lookback-window", str(params["lookback_window"]),
        "--momentum-factor", str(params["momentum_factor"]),
        "--decay-factor", str(params["decay_factor"]),
        "--min-signal-for-trade", str(params["min_signal_for_trade"]),
        "--stop-loss", str(params["stop_loss"]),
        "--max-daily-loss", str(params["max_daily_loss"]),
    ]

    # Run the backtest
    subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) 

    # Get the PnL from the CSV file
    pnl = get_pnl_from_csv()

    return pnl, params

def main():
    """
    Performs a grid search over the strategy's parameters.
    """
    # Define the parameter grid
    param_grid = {
        "imbalance_threshold": [0.1, 0.15, 0.2],
        "lookback_window": [10, 20, 30],
        "momentum_factor": [1.2, 1.5, 1.8],
        "decay_factor": [0.9, 0.95, 0.98],
        "min_signal_for_trade": [0.05, 0.1, 0.15],
        "stop_loss": [0.01, 0.02, 0.03],
        "max_daily_loss": [500, 1000, 1500],
    }

    # Create a list of all parameter combinations
    keys, values = zip(*param_grid.items())
    param_combinations = [dict(zip(keys, v)) for v in itertools.product(*values)]

    # Run the backtest for each parameter combination in parallel
    best_pnl = -float("inf")
    best_params = None
    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = [executor.submit(run_backtest, params) for params in param_combinations]
        for i, future in enumerate(concurrent.futures.as_completed(futures)):
            pnl, params = future.result()
            print(f"Completed backtest {i + 1}/{len(param_combinations)}...")
            print(f"  PnL: {pnl}, Params: {params}")
            if pnl > best_pnl:
                best_pnl = pnl
                best_params = params
                # Save the best results to a file
                with open("best_params.txt", "w") as f:
                    f.write(f"Best PnL: {best_pnl}\n")
                    f.write(f"Best Parameters: {best_params}\n")

    # Print the best parameters and PnL
    print(f"Best PnL: {best_pnl}")
    print(f"Best Parameters: {best_params}")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nOptimization interrupted. Printing best results found so far...")
        with open("best_params.txt", "r") as f:
            print(f.read())