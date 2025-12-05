#include <fmt/core.h>
#include <thread>
#include <chrono>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "../replay/replay.hpp"
#include "../bar/bar_builder.hpp"

int main() {
    using namespace std::chrono_literals;
    md::EventBus bus(1024, 1024);
    md::BarBuilder bar_builder(bus, md::BarBuilder::NS_PER_SEC);

    auto subs_bar = bus.subscribe(md::Topic::BAR_1S, [](const md::Event& e){
        if(!std::holds_alternative<md::Bar>(e.p)) {
            return;
        }
        const auto& b = std::get<md::Bar>(e.p);
        fmt::print("[BAR-1S] sym={} o={} h={} l={} c={} v={} start_ts={} end_ts={}\n",
                   b.symbol,
                   b.open,
                   b.high,
                   b.low,
                   b.close,
                   b.volume,
                   b.start_ts_ns,
                   b.end_ts_ns);
    });

    md::EventReplay replayer("logs/md_events.log");
    md::ReplayFilter f;
    f.filter_by_topic  = true;
    f.topic            = md::Topic::MD_TICK;
    f.filter_by_symbol = false;  // all symbols for now
    replayer.set_filter(f);

    replayer.replay_realtime(bus);

    std::this_thread::sleep_for(200ms);
    bar_builder.flush_all();

    bus.unsubscribe(subs_bar);
    bus.stop();
    bus.print_stats();
    return 0;
}