#pragma once 

#include <vector>
#include "strategy.hpp"

namespace md {

struct StrategyEntry {
    IStrategy*   strat;
    StrategyMode mode;
};

class MultiStrategy : public IStrategy {
private : 
    std::vector<StrategyEntry> strategies_;
public : 
    MultiStrategy() = default ;
    void add_strategy(IStrategy* strat, StrategyMode mode) {
        if(strat) {
            strategies_.push_back(StrategyEntry{strat, mode});
        }
    }

    void on_tick(const Tick& t, const Event& e) override {
        for(auto& s : strategies_) {
            if(s.mode == StrategyMode::BarOnly)continue;
            s.strat->on_tick(t,e);
        }
    }

    void on_log(const std::string& msg, const Event& e) override {
        for(auto& s : strategies_) {
            s.strat->on_log(msg, e);
        }
    }

    void on_heartbeat(const Event& e) override {
        for(auto& s : strategies_) {
            s.strat->on_heartbeat(e);
        }
    }

    void on_bar(const Bar& b, const Event& e) override {
        for(auto& s: strategies_) {
            if(s.mode == StrategyMode::TickOnly)continue;
            s.strat->on_bar(b,e);
        }
    }
};

}