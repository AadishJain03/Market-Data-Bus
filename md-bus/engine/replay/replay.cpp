#include "replay.hpp"

#include <fstream>
#include <chrono>
#include <thread>
#include <iostream>
#include <string>

namespace md {

EventReplay::EventReplay(const std::string& path)
    :path_(path) {}

bool EventReplay::match_filter(const Event& e) const {
    if(filter_.filter_by_topic && e.h.topic != filter_.topic) {
        return false;
    }

    if(filter_.filter_by_symbol) {
        if(!std::holds_alternative<Tick>(e.p)) {
            return false;
        }
        const auto& t = std::get<Tick> (e.p);
        if(t.symbol != filter_.symbol) {
            return false;
        }
    }

    if(filter_.filter_by_time) {
        if(e.h.ts_ns < filter_.ts_min || e.h.ts_ns > filter_.ts_max) {
            return false;
        }
    }
    return true;
}


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
        if(e.h.ts_ns == 0) {
            log_info("EventReplay: skipping internal event");
            continue;
        }
        if(!fn(e)){
            break;
        }
    }
}

void EventReplay::replay_fast(EventBus& bus){
    log_info("EventReplay: starting fast replay from '{}'", path_);
    events_published_ = 0;

    for_each_event_in_file(path_, [this, &bus](Event& e) {
        if(!match_filter(e)) {
            return true; // want the function to coninue;
        }

        if (filter_.limit_events && 
            events_published_ >= filter_.max_events) {
            log_info("EventReplay: reached max_events = {} in fast replay", filter_.max_events);
            return false;
        }

        if(step_mode_) { 
            fmt::print("[STEP] Press Enter to play next event...\n");
            std::string dummy;
            std::getline(std::cin, dummy);
        }

        bus.publish(e);
        ++events_published_;
        return true;
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
    events_published_  = 0;
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

        if(!match_filter(e)) {
            continue;
        }

        if(filter_.limit_events &&
            events_published_ >= filter_.max_events) {
            log_info("EventReplay: reached max_events={} in timed replay",
                     filter_.max_events);
            break;
        }

        if(first) {
            first = false;
            first_ts = e.h.ts_ns;
            prev_ts = e.h.ts_ns;
            wall_start = std::chrono::steady_clock::now();

            if (step_mode_) {
                fmt::print("[STEP] Press Enter to play next event...\n");
                std::string dummy;
                std::getline(std::cin, dummy);
            }

            bus.publish(e);
            ++events_published_;
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

          if (step_mode_) {
            fmt::print("[STEP] Press Enter to play next event...\n");
            std::string dummy;
            std::getline(std::cin, dummy);
        }

        bus.publish(e); 
        ++events_published_;
    }
    log_info("EventReplay: timed replay finished");
}

}