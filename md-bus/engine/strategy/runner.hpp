#pragma once 
#include <cstddef>
#include <string>
#include <variant>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "strategy.hpp"

namespace md {
/*
 * StrategyRunner
 * --------------
 * Bridges EventBus and IStrategy:
 *  - Subscribes to MD_TICK, LOG, HEARTBEAT.
 *  - Forwards events into the strategy callbacks.
 *  - Unsubscribes on destruction.
 *
 * Usage:
 *   md::EventBus bus(...);
 *   MyStrategy strat(...);
 *   {
 *       md::StrategyRunner runner(bus, strat);
 *       replayer.replay_realtime(bus);
 *       ...
 *   } // runner unsubscribes here
*/

class StrategyRunner {
private :
    EventBus& bus_;
    IStrategy& strat_;

    std::size_t sub_ticks_{};
    std::size_t sub_logs_{};
    std::size_t sub_hb_{};

public :
    StrategyRunner(EventBus& bus, IStrategy& strat)
        :bus_{bus}, strat_{strat} 
    {
        //Tick
        sub_ticks_ = bus.subscribe(Topic::MD_TICK, [this](const Event& e){
            if(std::holds_alternative<Tick>(e.p)){
                const auto& t = std::get<Tick>(e.p);
                strat_.on_tick(t, e);
            }else {
                log_warn("StrategyRunner: MD_TICK event without Tick payload (seq={})",
                             e.h.seq);
            }
        });

        //LOG
        sub_logs_ = bus_.subscribe(Topic::LOG,
            [this](const Event& e) {
                if (std::holds_alternative<std::string>(e.p)) {
                    const auto& msg = std::get<std::string>(e.p);
                    strat_.on_log(msg, e);
                }
        });

        // Heartbeats
        sub_hb_ = bus_.subscribe(Topic::HEARTBEAT,
            [this](const Event& e) {
                strat_.on_heartbeat(e);
        });
    }
    ~StrategyRunner() {
        // Unsubscribe in destructor; assumes bus_ is still alive here.
        // We don't guard for errors to keep it simple.
        bus_.unsubscribe(sub_ticks_);
        bus_.unsubscribe(sub_logs_);
        bus_.unsubscribe(sub_hb_);
    }
    StrategyRunner(const StrategyRunner&) = delete;
    StrategyRunner& operator=(const StrategyRunner&) = delete;

};
}