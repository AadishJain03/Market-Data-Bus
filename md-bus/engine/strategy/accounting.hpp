#pragma once 

#include <fmt/core.h>
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <limits>

#include "../common/log.hpp"

namespace md {

enum class Side {
    Long,
    Short,
};

inline const char* to_string(Side s) {
    switch (s) {
        case Side::Long : return "LONG";
        case Side::Short : return "SHORT";
    }
    return "UNKNOWN";
}

enum class ExitReason {
    None,
    Threshold,
    StopLoss,
    TakeProfit,
    CloseOut,
};

inline const char* to_string(ExitReason r){
    switch (r) {
        case ExitReason::None : return "NONE";
        case ExitReason::Threshold : return "THRESHOLD";
        case ExitReason::StopLoss : return "STOPLOSS";
        case ExitReason::TakeProfit : return "TAKEPROFIT";
        case ExitReason::CloseOut : return "CLOSEOUT";
    }
    return "UNKNOWN";
}

//keeping track
struct Trade {
    std::string symbol;
    Side side = Side::Long;
    int qty = 0;

    double entry_price = 0.0;
    double exit_price = 0.0;
    double pnl = 0.0;

    uint64_t entry_ts_ns = 0;
    uint64_t exit_ts_ns = 0;

    ExitReason exit_reason = ExitReason::None;
};

struct Position {
    std::string symbol;
    bool open = false;
    Side side = Side::Long;
    int qty = 0;
    double entry_pq = 0.0;
    uint64_t entry_ts_ns = 0;
};

class Account {
private : 
    double starting_cash_{0.0};
    double realized_pnl_{0.0};
    double equity_{0.0};
    double peak_equity_{0.0};
    double max_drawdown_{0.0};

    Position pos_{};
    std::vector<Trade> trades_;
public :
    explicit Account(double starting_cash = 0.0)
        :starting_cash_{starting_cash},
        realized_pnl_(0.0),
        equity_(starting_cash),
        peak_equity_(starting_cash),
        max_drawdown_(0.0) {}

    bool has_open_position() const {return pos_.open;}
    const Position& position() const {return pos_;}

    void open_long(const std::string& symbol,
                    int qty, double pq, uint64_t ts_ns) {
        if(pos_.open) {
            log_warn("Account::open_long: position already open, ignoring");
            return;
        }
        pos_.open = true;
        pos_.side        = Side::Long;
        pos_.symbol      = symbol;
        pos_.qty         = qty;
        pos_.entry_pq    = pq;
        pos_.entry_ts_ns = ts_ns;

        log_info("Account: open LONG {} qty={} pq={}", symbol, qty, pq);
    }

    void close_position(double pq, uint64_t ts_ns, ExitReason reason){
        if(!pos_.open){
            log_warn("Account::close_position: no open position, ignoring");
            return;
        }

        double signed_qty = static_cast<double>(pos_.qty) * (pos_.side == Side::Long ? 1.0 : -1.0);
        double trade_pnl = signed_qty * (pq - pos_.entry_pq);

        Trade tr;
        tr.symbol      = pos_.symbol;
        tr.side        = pos_.side;
        tr.qty         = pos_.qty;
        tr.entry_price = pos_.entry_pq;
        tr.exit_price  = pq;
        tr.pnl         = trade_pnl;
        tr.entry_ts_ns = pos_.entry_ts_ns;
        tr.exit_ts_ns  = ts_ns;
        tr.exit_reason = reason;

        realized_pnl_ += trade_pnl;
        trades_.push_back(tr);

        log_info("Account: close {} side={} qty={} entry_px={} exit_px={} pnl={} reason={}",
                 tr.symbol,
                 to_string(tr.side),
                 tr.qty,
                 tr.entry_price,
                 tr.exit_price,
                 tr.pnl,
                 to_string(tr.exit_reason));
        
        pos_.open        = false;
        pos_.qty         = 0;
        pos_.entry_pq    = 0.0;
        pos_.entry_ts_ns = 0;
    }
    double realized_pnl() const {return realized_pnl_;}

    // Unrealized PnL at given price
    double unrealized_pnl(double last_pq) const {
        if (!pos_.open) return 0.0;
        double signed_qty = static_cast<double>(pos_.qty) *
                            (pos_.side == Side::Long ? 1.0 : -1.0);
        return (last_pq - pos_.entry_pq) * signed_qty;
    }

    void update_equity(double last_pq) {
        double u = unrealized_pnl(last_pq);
        equity_ = starting_cash_ + realized_pnl_ + u;
        if(equity_ > peak_equity_) {
            peak_equity_ = equity_;
        }else {
            double dd = peak_equity_ - equity_;
            if(dd > max_drawdown_) {
                max_drawdown_ = dd;
            }
        }
    }
    
    double equity() const {return equity_;}
    double max_drawdown() const {return max_drawdown_;}

    const std::vector<Trade>& trades() const { return trades_; }

    void print_summary() const {
        fmt::print("\n==== Account Summary ====\n");
        fmt::print("  starting_cash    = {}\n", starting_cash_);
        fmt::print("  realized_pnl     = {}\n", realized_pnl_);
        fmt::print("  equity           = {}\n", equity_);
        fmt::print("  max_drawdown     = {}\n", max_drawdown_);
        fmt::print("  trades           = {}\n", trades_.size());

        if (!trades_.empty()) {
            int wins = 0;
            int losses = 0;
            double sum_win = 0.0;
            double sum_loss = 0.0;
            double best = std::numeric_limits<double>::lowest();
            double worst = std::numeric_limits<double>::max();

            for (const auto& tr : trades_) {
                if (tr.pnl > 0) {
                    wins++;
                    sum_win += tr.pnl;
                } else if (tr.pnl < 0) {
                    losses++;
                    sum_loss += tr.pnl;
                }
                if (tr.pnl > best) best = tr.pnl;
                if (tr.pnl < worst) worst = tr.pnl;
            }

            int n = static_cast<int>(trades_.size());
            double win_rate = (n > 0) ? (static_cast<double>(wins) / n * 100.0) : 0.0;
            double avg_win  = (wins > 0) ? (sum_win / wins) : 0.0;
            double avg_loss = (losses > 0) ? (sum_loss / losses) : 0.0;

            fmt::print("  wins             = {} ({:.2f}%)\n", wins, win_rate);
            fmt::print("  losses           = {}\n", losses);
            fmt::print("  avg_win          = {}\n", avg_win);
            fmt::print("  avg_loss         = {}\n", avg_loss);
            fmt::print("  best_trade       = {}\n", (trades_.empty() ? 0.0 : best));
            fmt::print("  worst_trade      = {}\n", (trades_.empty() ? 0.0 : worst));
        }

        fmt::print("=========================\n");
    }

    void dump_trades_csv(const std::string& path) const {
        std::ofstream out(path);
        if(!out) {
            log_error("Account::dump_trades_csv: failed to open '{}'", path);
            return;
        }
        out << "symbol,side,qty,entry_price,exit_price,entry_ts_ns,exit_ts_ns,pnl,exit_reason\n";
        for (const auto& tr : trades_) {
            out << tr.symbol << ","
                << to_string(tr.side) << ","
                << tr.qty << ","
                << tr.entry_price << ","
                << tr.exit_price << ","
                << tr.entry_ts_ns << ","
                << tr.exit_ts_ns << ","
                << tr.pnl << ","
                << to_string(tr.exit_reason) << "\n";
        }

        log_info("Account: dumped {} trades to '{}'", trades_.size(), path);
    }
};

}