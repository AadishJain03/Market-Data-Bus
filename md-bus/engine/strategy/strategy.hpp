#pragma once 
#include <string>

#include "../common/event.hpp"

namespace md {
/**
 * IStrategy
 * ---------
 * Minimal interface that reacts to events coming from the EventBus.
 * For now we only require on_tick(), logs and heartbeats are optional hooks.
 */
class IStrategy {
public :
    virtual ~IStrategy() = default;
    virtual void on_tick(const Tick& t, const Event& e) = 0;
    virtual void on_log(const std::string& msg, const Event& e) {
        (void)msg; (void)e;
    }
    virtual void on_heartbeat(const Event& e) {
        (void)e;
    }
};

}