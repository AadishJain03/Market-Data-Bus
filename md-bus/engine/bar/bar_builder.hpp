#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"

namespace md {

class BarBuilder {
private :
    //carries the information about 
    //active, bucket_id, bar
    struct BarState {
        bool active {false};
        uint64_t bucket_id{0};
        Bar bar;
    };

    EventBus& bus_;
    uint64_t bucket_ns_;
    std::size_t sub_id_{0};

    std::unordered_map<std::string, BarState> state_;

    void on_tick(const Event& e) {
        if(!std::holds_alternative<Tick>(e.p)){
            return;
        }
        //Tick contains symbol, qty, pq
        const Tick& t = std::get<Tick>(e.p);
        uint64_t ts = e.h.ts_ns;
        if(ts == 0) {
            return;
        }
        //gives you a monotonic bucket number:
        //All timestamps in [0, bucket_ns_) → bucket 0
        //bucket_ns_, 2*bucket_ns_) → bucket 1
        //2*bucket_ns_, 3*bucket_ns_) → bucket 2
        uint64_t bucket_id = ts / bucket_ns_;

        auto& st = state_[t.symbol];
        if(!st.active) {
            st.active = true;
            st.bucket_id = bucket_id;
            st.bar.symbol = t.symbol;
            st.bar.open = t.pq;
            st.bar.high = t.pq;
            st.bar.low = t.pq;
            st.bar.close = t.pq;
            st.bar.volume = t.qty;
            st.bar.start_ts_ns = bucket_id * bucket_ns_;
            st.bar.end_ts_ns = ts;
            return;
        }
        
         // If this tick belongs to a new bucket, finalize the old bar and start a new one
        if(bucket_id != st.bucket_id) {
            //example : bucket = 12 → 
            //end = (12+1)*1s - 1 = 12.999999999s
            st.bar.end_ts_ns = (st.bucket_id + 1) * bucket_ns_ - 1;
            //finalize the previous tick
            publish_bar(st.bar);
            st.bucket_id = bucket_id;
            st.bar.symbol = t.symbol;
            st.bar.open = t.pq;
            st.bar.high = t.pq;
            st.bar.low = t.pq;
            st.bar.close = t.pq;
            st.bar.volume = t.qty;
            st.bar.start_ts_ns = bucket_id * bucket_ns_;
            st.bar.end_ts_ns = ts;
            return;
        }
        if(t.pq > st.bar.high) st.bar.high = t.pq;
        if (t.pq < st.bar.low)  st.bar.low  = t.pq;
        st.bar.close  = t.pq;
        st.bar.volume += t.qty;
        st.bar.end_ts_ns = ts;
    }

    void publish_bar(const Bar& b) {
        Event ev;
        ev.h.seq = 0;
        ev.h.ts_ns = b.end_ts_ns;
        ev.h.topic = Topic::BAR_1S;
        ev.p = b;
        log_debug("BarBuilder: publishing bar sym={} o={} h={} l={} c={} v={}",
                  b.symbol, b.open, b.high, b.low, b.close, b.volume);

        bus_.publish(ev);
    }
public :
    static constexpr uint64_t NS_PER_SEC = 1'000'000'000ULL;

    BarBuilder(EventBus& bus, uint64_t bucket_ns = NS_PER_SEC)
        : bus_(bus)
        , bucket_ns_(bucket_ns)
    {
        sub_id_ = bus_.subscribe(Topic::MD_TICK,
                [this](const Event& e) {
                    on_tick(e);
                });
                
        log_info("BarBuilder: subscribed to MD_TICK (bucket_ns = {})", bucket_ns_);
    }

    ~BarBuilder() {
        flush_all();
        bus_.unsubscribe(sub_id_);
        log_info("BarBuilder: unsubscribed and flushed");
    }

    void flush_all() {
        for(auto &kv : state_) {
            BarState& st = kv.second;
            if(!st.active) continue;
            publish_bar(st.bar);
            st.active = false;
        }
    }
};
}