#include <fmt/core.h>
#include <thread>
#include <chrono>
#include <deque>
#include <numeric>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "../replay/replay.hpp"

#include "../strategy/strategy.hpp"
#include "../strategy/runner.hpp"
#include "../strategy/accounting.hpp"
#include "../strategy/multi_strategy.hpp"

class TradingThresholdStrategy : public md::IStrategy {
private :
    md::Account& account_;
    double threshold_;
    int qty_;
    double sl_offset_;
    double tp_offset_;
    double sl_level_{0.0};
    double tp_level_{0.0};

    double last_pq_ {0.0};
    uint64_t last_ts_ns_ {0};

public : 
    TradingThresholdStrategy(md::Account & account, double threshold,
                             int qty, double stop_loss_offset, 
                             double take_profit_offset)
        :account_{account}, threshold_{threshold}, qty_{qty}, sl_offset_{stop_loss_offset}, tp_offset_{take_profit_offset} {}

    void on_tick(const md::Tick& t, const md::Event& e) override {
        const double pq = t.pq;
        last_pq_ = pq;
        last_ts_ns_ = e.h.ts_ns;

        // Always keep equity updated
        account_.update_equity(pq);
        if(!account_.has_open_position()) {
            if(pq > threshold_) {
                account_.open_long(t.symbol, qty_, pq, e.h.ts_ns);
                sl_level_ = pq + sl_offset_;
                tp_level_ = pq + tp_offset_;

                    fmt::print("[STRAT] ENTER LONG seq={} sym={} pq={} thr={} qty={} SL={} TP={}\n",
                        e.h.seq, t.symbol, pq, threshold_, qty_, sl_level_, tp_level_);
            }
            return;
        }
        const md::Position& pos = account_.position();

        // Stop Loss (assuming sl_offset_ is negative, so sl_level_ < entry_pq)
        if(pq <= sl_level_) {
            fmt::print("[STRAT] STOP LOSS EXIT seq={} sym={} pq={} SL={}\n",
                    e.h.seq, pos.symbol, pq, sl_level_);
            account_.close_position(pq, e.h.ts_ns, md::ExitReason::StopLoss);
            return;
        }
         // Take Profit
        if (pq >= tp_level_) {
            fmt::print("[STRAT] TAKE PROFIT EXIT seq={} sym={} pq={} TP={}\n",
                       e.h.seq, pos.symbol, pq, tp_level_);
            account_.close_position(pq, e.h.ts_ns, md::ExitReason::TakeProfit);
            return;
        }

         // Threshold-based exit: if price has fallen back below threshold
        if (pq < threshold_) {
            fmt::print("[STRAT] THRESHOLD EXIT seq={} sym={} pq={} thr={}\n",
                       e.h.seq, pos.symbol, pq, threshold_);
            account_.close_position(pq, e.h.ts_ns, md::ExitReason::Threshold);
            return;
        }
    }

    //tells the compiler:
    //this parameter is intentionally unused. Do not warn about it.
    void on_log(const std::string& msg, const md::Event& e) override {
        (void)msg;
        (void)e;
    }

    void on_heartbeat(const md::Event& e) override {
        (void)e;
    }

    void finalize() {
        if (account_.has_open_position() && last_pq_ > 0.0) {
            fmt::print("[STRAT] CLOSE OUT at last price pq={}\n", last_pq_);
            account_.close_position(last_pq_,
                                    last_ts_ns_,
                                    md::ExitReason::CloseOut);
        }
        // Update equity once more with last price
        if (last_pq_ > 0.0) {
            account_.update_equity(last_pq_);
        }
    }
};

// Strategy 2: Mean-reversion style "zone" labeller
// - Maintains rolling window of last N prices
// - Prints whether current price is below/above rolling mean by a small band

class MeanReversionTradingStrategy : public md::IStrategy {
private : 
    md::Account& account_;
    std::size_t window_;
    double band_;
    int qty_;
    std::deque<double> prices_;

    double last_pq_ = 0.0;
    uint64_t last_ts_ns_ = 0;
public : 
    MeanReversionTradingStrategy(md::Account& account,
                                std::size_t window,
                                double band, int qty)
        :account_(account),
         window_(window),
         band_(band),
         qty_(qty) {}
    void on_tick(const md::Tick& t, const md::Event& e) override {
        const double pq = t.pq;
        last_pq_ = pq;
        last_ts_ns_ = e.h.ts_ns;
        account_.update_equity(pq);
        prices_.push_back(pq);
        if(prices_.size() > window_) {
            prices_.pop_front();
        }
        if(prices_.size() < window_) {
            return;
        }

        double sum = std::accumulate(prices_.begin(), prices_.end(), 0.0);
        double avg = sum / static_cast<double>(prices_.size());
        double diff = pq - avg;

        if(!account_.has_open_position()) {
            if(diff < -band_) {
                account_.open_long(t.symbol, qty_, pq, e.h.ts_ns);
                fmt::print("[STRAT2] ENTER LONG (MR) sym={} pq={} avg={:.2f} diff={:.2f}\n",
                           t.symbol, pq, avg, diff);
            }
            return;
        }
        if(diff >= 0.0) {
            const md::Position& pos = account_.position();
            fmt::print("[STRAT2] EXIT LONG (MR) sym={} pq={} avg={:.2f} diff={:.2f}\n",
                       pos.symbol, pq, avg, diff);
            account_.close_position(pq, e.h.ts_ns, md::ExitReason::Threshold);
            return ;
        }
    }

    void on_log(const std::string& msg, const md::Event& e) override {
        (void)msg; (void)e;
    }

    void on_heartbeat(const md::Event& e) override {
        (void)e;
    }
    void finalize() {
        if (account_.has_open_position() && last_pq_ > 0.0) {
            fmt::print("[STRAT2] CLOSE OUT at last price pq={}\n", last_pq_);
            account_.close_position(last_pq_,
                                    last_ts_ns_,
                                    md::ExitReason::CloseOut);
        }
        if (last_pq_ > 0.0) {
            account_.update_equity(last_pq_);
        }
    }

};
int main() {
    using namespace std::chrono_literals;

    md::EventBus bus(1024, 1024);
    md::Account account1(0.0);
    md::Account account2(0.0);

    double threshold = 22502.0;
    int qty = 1;
    double stop_loss_offset = -20.0;
    double take_profit_offset = +40.0;
    TradingThresholdStrategy strat1(account1,
        threshold,
        qty,
        stop_loss_offset,
        take_profit_offset);

    // Strategy 2: mean-reversion trading
    std::size_t mr_window = 5;
    double      mr_band   = 2.0;
    int         qty2      = 1;

    MeanReversionTradingStrategy strat2(account2,
                                        mr_window,
                                        mr_band,
                                        qty2);

        
    md::MultiStrategy multi;
    multi.add_strategy(&strat1, md::StrategyMode::TickOnly);
    multi.add_strategy(&strat2, md::StrategyMode::TickOnly);

    //added scope to this so that runner gets destroyed before 
    //bus
    {
        md::StrategyRunner runner(bus, multi);
        md::EventReplay replayer("logs/md_events.log");

        md::ReplayFilter f;
        f.filter_by_topic  = true;
        f.topic            = md::Topic::MD_TICK;
        f.filter_by_symbol = true;
        f.symbol           = "NIFTY";

        replayer.set_filter(f);
        replayer.replay_realtime(bus);
        std::this_thread::sleep_for(200ms);
    }
    strat1.finalize();// finalize is defined above in the class
    strat2.finalize();

    // Two separate summaries + CSVs
    fmt::print("\n=== Strategy 1 (Threshold) ===\n");
    account1.print_summary();
    account1.dump_trades_csv("trades_strat1.csv");

    fmt::print("\n=== Strategy 2 (Mean Reversion) ===\n");
    account2.print_summary();
    account2.dump_trades_csv("trades_strat2.csv");

    bus.stop();
    bus.print_stats();

    return 0;
}
            