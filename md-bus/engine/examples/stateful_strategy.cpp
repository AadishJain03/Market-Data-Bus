#include <chrono>
#include <fmt/core.h>
#include <thread>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "../replay/replay.hpp"
#include "../strategy/strategy.hpp"
#include "../strategy/runner.hpp"
#include "../strategy/accounting.hpp"

// Stateful trading strategy:
// - Goes LONG when price crosses above threshold
// - Exits when price falls back below threshold
// - Tracks trades, realized/unrealized PnL, and simple MFE/MAE-style stats

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
        :account_{account}, threshold_{threshold}, qty_{qty}, sl_offset_{stop_loss_offset}, tp_level_{take_profit_offset} {}

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

        // Stop Loss (assuming sl_offset_ is negative, so sl_level_ < entry_px)
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


int main () {
    using namespace std::chrono_literals;
    md::EventBus bus(1024, 1024);

    // Account with starting cash (optional)
    md::Account account(0.0);

    // Strategy params
    double threshold         = 22502.0;
    int    qty               = 1;
    double stop_loss_offset  = -20.0;  // 20 pts below entry
    double take_profit_offset= +40.0;  // 40 pts above entry

    TradingThresholdStrategy strat(account,
                                   threshold,
                                   qty,
                                   stop_loss_offset,
                                   take_profit_offset);

    //scope so that the runner is destroyed before we print summary
    {
        md::StrategyRunner runner(bus, strat);
        md::EventReplay replayer("logs/md_events.log");
        md::ReplayFilter f;
        f.filter_by_topic = true;
        f.topic = md::Topic::MD_TICK;

        f.filter_by_symbol = true;
        f.symbol = "NIFTY";

        replayer.set_filter(f);
        replayer.replay_realtime(bus);
        std::this_thread::sleep_for(200ms);
    }

    strat.finalize();
    account.print_summary();
    account.dump_trades_csv("trades.csv");
    
    bus.stop();
    bus.print_stats();
    return 0;
}