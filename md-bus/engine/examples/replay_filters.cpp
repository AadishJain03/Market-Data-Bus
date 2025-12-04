#include <fmt/core.h>
#include <thread>
#include <chrono>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "../replay/replay.hpp" 

int main() {
    using namespace std::chrono_literals;
    md::EventBus bus(1024, 1024);

    auto sub_ticks = bus.subscribe(md::Topic::MD_TICK, [](const md::Event& e){
        if(std::holds_alternative<md::Tick>(e.p)){
            const auto& t = std::get<md::Tick>(e.p); 
            fmt::print("[Tick-F] seq = {} sym = {} pq = {} qty = {}\n",
                       e.h.seq, t.symbol, t.pq, t.qty);
        }
    });

    auto sub_logs = bus.subscribe(md::Topic::LOG, [](const md::Event& e){
        if(std::holds_alternative<std::string>(e.p)) {
            const auto& msg = std::get<std::string>(e.p);
            fmt::print("[LOG-F] seq = {} msg = {}\n", e.h.seq, msg);
        }
    });

    auto sub_hb = bus.subscribe(md::Topic::HEARTBEAT, [](const md::Event& e){
        fmt::print("[HB-F ] seq = {} topic = {}\n",
                   e.h.seq, static_cast<int>(e.h.topic));
    });

     auto sub_mon = bus.subscribe_all([](const md::Event& e){
        fmt::print("[MON-F] seq = {} topic = {}\n",
                   e.h.seq, static_cast<int>(e.h.topic));
    });
    
    md::EventReplay replayer("logs/md_events.log");

    //adding filters
    md::ReplayFilter f;
    f.filter_by_topic  = true;
    f.topic            = md::Topic::MD_TICK;

    f.filter_by_symbol = true;
    f.symbol           = "NIFTY";

    f.limit_events     = true;
    f.max_events       = 10;

    replayer.set_filter(f);
    replayer.replay_realtime(bus);

    std::this_thread::sleep_for(200ms);

    bus.unsubscribe(sub_ticks);
    bus.unsubscribe(sub_logs);
    bus.unsubscribe(sub_hb);
    bus.unsubscribe(sub_mon);

    bus.stop();
    bus.print_stats();

    return 0;

}

