import numpy as np
import pandas as pd


def analyze_portfolio(file_path):
    """
    Analyzes a portfolio backtesting CSV file to calculate performance metrics.

    Args:
        file_path (str): The path to the portfolio CSV file.

    Returns:
        dict: A dictionary containing the analysis results.
    """
    try:
        # Read the CSV file, skipping the header comments
        df = pd.read_csv(file_path, comment='#')

        # --- Final PnL ---
        final_realized_pnl = df['realized_pnl'].iloc[-1]

        # --- Performance Metrics ---
        # Calculate trade returns
        df['pnl_change'] = df['realized_pnl'].diff().fillna(0)
        trades = df[df['pnl_change'] != 0]
        
        if not trades.empty:
            win_loss_ratio = len(trades[trades['pnl_change'] > 0]) / len(trades[trades['pnl_change'] < 0]) if len(trades[trades['pnl_change'] < 0]) > 0 else float('inf')
            average_profit_per_trade = trades['pnl_change'].mean()
        else:
            win_loss_ratio = 0
            average_profit_per_trade = 0

        # Calculate daily returns for Sharpe Ratio
        df['timestamp'] = pd.to_datetime(df['timestamp'])
        df.set_index('timestamp', inplace=True)
        daily_returns = df['total_pnl'].resample('D').last().pct_change().dropna()
        
        sharpe_ratio = (daily_returns.mean() / daily_returns.std()) * np.sqrt(252) if daily_returns.std() != 0 else 0

        # --- Drawdowns ---
        cumulative_pnl = df['total_pnl']
        peak = cumulative_pnl.expanding(min_periods=1).max()
        drawdown = peak - cumulative_pnl
        max_drawdown = drawdown.max()

        # --- Conclusion ---
        is_profitable = final_realized_pnl > 0

        return {
            "Final PnL": final_realized_pnl,
            "Win/Loss Ratio": win_loss_ratio,
            "Average Profit Per Trade": average_profit_per_trade,
            "Sharpe Ratio": sharpe_ratio,
            "Maximum Drawdown": max_drawdown,
            "Is Profitable": is_profitable
        }

    except FileNotFoundError:
        return {"error": "File not found."}
    except Exception as e:
        return {"error": str(e)}

if __name__ == "__main__":
    results = analyze_portfolio('examples/cpp_consumer_fetchcontent/portfolio_1000.csv')
    for key, value in results.items():
        print(f"{key}: {value}")