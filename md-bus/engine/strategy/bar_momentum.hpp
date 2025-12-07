#pragma once 

#include <string>
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "accounting.hpp"
#include "strategy.hpp"
#include "bar_window.hpp"

/*
 * BarMomentumStrategy
 * -------------------
 * Simple bar-based momentum strategy:
 *
 * - Maintains a rolling window of N bars for a single symbol.
 * - If no position:
 *      if momentum > threshold -> enter LONG at bar.close
 * - If has position:
 *      if momentum <= 0 -> exit (momentum has stalled/reversed)
 *
 * Uses md::Account for PnL & trade recording.
 */

namespace md {

class BarMomentumStrategy : public IStrategy {
private : 
    Account& account_;
    std::string symbol_;
    BarWindow window_;
    double mom_threshold_;
    int qty_;
    double   last_close_ = 0.0;
    uint64_t last_ts_    = 0;
public :
    BarMomentumStrategy(Account& account,
                    std::string symbol, 
                    std::size_t window_size,
                    double momentum_threshold,
                    int qty)
        :account_{account},
        symbol_{std::move(symbol)},
        window_{window_size},
        mom_threshold_{momentum_threshold},
        qty_{qty} {}
    
    void on_tick(const Tick& ,const Event&) override{};
    //ignore the tick level data in this strategy

    void on_log(const std::string& msg, const Event& e) override {
        log_debug("[BARMOM] log event seq={} msg={}", e.h.seq, msg);
    }

    void on_heartbeat(const Event& e) override {
        (void)e;
    }

    void on_bar(const Bar& b, const Event& e) override {
        if(b.symbol != symbol_) return ;
        last_close_ = b.close;
        last_ts_    = e.h.ts_ns;
        window_.push(b);
        if(!window_.full()) return ;
        double mom = window_.momentum();
        log_debug("[BARMOM] bar sym={} o={} h={} l={} c={} v={} mom={:.4f} seq={}",
                  b.symbol, b.open, b.high, b.low, b.close, b.volume, mom, e.h.seq);
        if(!account_.has_open_position()) {

            //entry logic : momentum strongly positive
            if(mom > mom_threshold_) {
                account_.open_long(symbol_, qty_, b.close, e.h.ts_ns);
                log_info("[BARMOM] ENTER LONG sym={} c={} mom={:.4f} thr={:.4f} qty={}",
                         symbol_, b.close, mom, mom_threshold_, qty_);
            }
            return ;
        }

        if(mom <= 0.0) {
            const Position& pos = account_.position();
            log_info("[BARMOM] EXIT LONG sym={} c={} mom={:.4f} (<=0) qty={}",
                     pos.symbol, b.close, mom, pos.qty);
            account_.close_position(b.close, e.h.ts_ns, ExitReason::Threshold);
        }
    }
    void finalize() {
        if (account_.has_open_position() && last_ts_ != 0) {
            const Position& pos = account_.position();
            log_info("[BARMOM] FINAL CLOSEOUT sym={} px={} qty={}",
                     pos.symbol, last_close_, pos.qty);
            account_.close_position(last_close_, last_ts_, ExitReason::CloseOut);
        }
    }
};

}