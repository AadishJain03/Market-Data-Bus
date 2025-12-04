#pragma once 
#include <string>

#include "../common/event.hpp"
#include "../common/event_io.hpp"
#include "../common/log.hpp"
#include "../bus/bus.hpp"


namespace md {

struct ReplayFilter {
    bool filter_by_topic{false};
    Topic topic{};

    bool filter_by_symbol{false};
    std::string symbol;

    bool filter_by_time{false};
    uint64_t ts_min{0};
    uint64_t ts_max{UINT64_MAX};

    bool limit_events{0};
    size_t max_events{0};
};

class EventReplay {
private : 
    std::string path_;
    ReplayFilter filter_{};
    bool step_mode_{false};
    size_t events_published_{0};
    
    //Returns true if event passes all active filters
    bool match_filter(const Event& e) const;
public : 
    explicit EventReplay(const std::string& path);

    // Fast : no sleeps, just shove everything into the bus
    void replay_fast(EventBus& bus);

    //Real-time based on recorded ts_ns deltas (best effort)
    void replay_realtime(EventBus& bus);

    // Same as realtime but scaled (speed > 1 then faster else slower)
    void replay_speed(EventBus & bus, double speed);

    inline void set_filter(const ReplayFilter& f) {filter_ = f;}
    void clear_filter() {filter_ = ReplayFilter{};}

    inline void set_max_events(size_t n) {
        filter_.limit_events = true;
        filter_.max_events = n;
    }

    void enable_step_mode(bool on = true) {step_mode_ = on ;} 
};

}