#include <fmt/core.h>
#include <thread>
#include <chrono>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "../replay/replay.hpp"

int main() {
    using namespace std::chrono_literals;

    md::EventBus bus(/*ingress*/1024, /*per-sub*/1024);

    // Subscribers
    auto sub_ticks = bus.subscribe(md::Topic::MD_TICK, [](const md::Event& e){
        if (std::holds_alternative<md::Tick>(e.p)) {
            const auto& t = std::get<md::Tick>(e.p);
            fmt::print("[Tick-R] seq = {} sym = {} pq = {} qty = {}\n",
                       e.h.seq, t.symbol, t.pq, t.qty);
        }
    });

    auto sub_logs = bus.subscribe(md::Topic::LOG, [](const md::Event& e){
        if (std::holds_alternative<std::string>(e.p)) {
            const auto& msg = std::get<std::string>(e.p);
            fmt::print("[LOG-R] seq = {} msg = {}\n", e.h.seq, msg);
        }
    });

    auto sub_hb = bus.subscribe(md::Topic::HEARTBEAT, [](const md::Event& e){
        fmt::print("[HB-R ] seq = {} topic = {}\n",
                   e.h.seq, static_cast<int>(e.h.topic));
    });

    auto sub_mon = bus.subscribe_all([](const md::Event& e){
        fmt::print("[MON-R] seq = {} topic = {}\n",
                   e.h.seq, static_cast<int>(e.h.topic));
    });

    // Replay from file produced by hello_bus / recorder
    md::EventReplay replayer("logs/md_events.log");

    // Choose one:
    // replayer.replay_fast(bus);
    // replayer.replay_realtime(bus);
        replayer.replay_realtime(bus);
    // replayer.replay_speed(bus, 2.0);  // 2x speed replay

    // Give subscribers a moment to drain
    std::this_thread::sleep_for(200ms);

    bus.unsubscribe(sub_ticks);
    bus.unsubscribe(sub_logs);
    bus.unsubscribe(sub_hb);
    bus.unsubscribe(sub_mon);

    bus.stop();
    bus.print_stats();

    return 0;
}