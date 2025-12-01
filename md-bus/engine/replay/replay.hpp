#pragma once 
#include <string>

#include "../common/event.hpp"
#include "../common/event_io.hpp"
#include "../common/log.hpp"
#include "../bus/bus.hpp"


namespace md {

class EventReplay {
private : 
    std::string path_;
public : 
    explicit EventReplay(const std::string& path);

    // Fast : no sleeps, just shove everything into the bus
    void replay_fast(EventBus& bus);

    //Real-time based on recorded ts_ns deltas (best effort)
    void replay_realtime(EventBus& bus);

    // Same as realtime but scaled (speed > 1 then faster else slower)
    void replay_speed(EventBus & bus, double speed);
};

}