#include "replay.hpp"

#include <fstream>
#include <chrono>
#include <thread>

namespace md {

EventReplay::EventReplay(const std::string& path)
    :path_(path) {}

template <typename Fn>
static void for_each_event_in_file(const std::string& path, Fn&& fn){
    std::ifstream in(path);
    if(!in){
        log_error("EventReplay: failed to open replay file '{}'", path);
        return;
    }

    std::string line;
    while(std::getline(in, line)) {
        if(line.empty())continue;
        Event e;
        if(!parse_event(line, e)) {
            log_warn("EventReplay: failed to parse line: {}", line);
            continue;
        }
        fn(e);
    }
}

void EventReplay::replay_fast(EventBus& bus){
    log_info("EventReplay: starting fast replay from '{}'", path_);

    for_each_event_in_file(path_, [&bus](Event& e){
        bus.publish(e);
    });

    log_info("EventReplay: fast replay finished");
}

void EventReplay::replay_realtime(EventBus& bus){
    replay_speed(bus, 1.0);
}

void EventReplay::replay_speed(EventBus& bus, double speed){
    if(speed <= 0.0){
        log_warn("EventReplay: invalid speed {} using 1.0", speed);
        speed = 1.0;
    }

    log_info("EventReplay: starting timed replay from '{}' with speed {}x",
        path_, speed);

    std::ifstream in(path_);
    if(!in) {
        log_error("EventReplay: failed to open replay file '{}'", path_);
        return ;
    }
    std::string line;
    bool first = true;
    uint64_t first_ts = 0;
    uint64_t prev_ts = 0;
    auto wall_start = std::chrono::steady_clock::now();

    while (std::getline(in , line)) {
        if(line.empty())continue;
        Event e;
        if(!parse_event(line, e)) {
            log_warn("EventReplay: failed to parse line: {}", line);
            continue;
        }

        if(e.h.ts_ns == 0) {
            log_info("Replay: skipping internal stop event (seq={}, topic={})",
                     e.h.seq,
                     static_cast<int>(e.h.topic));
            continue;
        }

        if(first) {
            first = false;
            first_ts = e.h.ts_ns;
            prev_ts = e.h.ts_ns;
            wall_start = std::chrono::steady_clock::now();
            bus.publish(e);
            continue;
        }
        int64_t dt_ns = static_cast<int64_t>(e.h.ts_ns) -                   
                            static_cast<int64_t>(prev_ts);
        if (dt_ns < 0) dt_ns = 0;  // don't go backwards
        //i.e treat them as zero delay

        double scaled_ns = dt_ns / speed;
        auto dur = std::chrono::nanoseconds(static_cast<int64_t>(scaled_ns));

        if (dur.count() > 0) {
            std::this_thread::sleep_for(dur);
        }
        bus.publish(e); 
    }
    log_info("EventReplay: timed replay finished");
}

}