#include <string>
#include <fmt/core.h>
#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "../replay/replay.hpp"
#include "../bar/bar_builder.hpp"

#include "../strategy/accounting.hpp"
#include "../strategy/strategy.hpp"
#include "../strategy/runner.hpp"
#include "../strategy/multi_strategy.hpp"
#include "../strategy/bar_momentum.hpp"


int main() {
    using namespace std::chrono_literals;
    md::EventBus bus(1024,1024);
    md::BarBuilder bar_builder(bus);

    auto sub_bars = bus.subscribe(md::Topic::BAR_1S, [](const md::Event& e){
        if(!std::holds_alternative<md::Bar>(e.p)){
            return ;            
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

    md::Account acct_bar;

    std::string symbol = "NIFTY";
    std::size_t window_size   = 1;      // number of bars in the window
    double      mom_threshold = 0.1;    // minimal momentum to enter long
    int         qty           = 1;      // position size


    md::BarMomentumStrategy strat_bar(
        acct_bar,
        symbol,
        window_size,
        mom_threshold,
        qty
    );

    md::StrategyRunner runner(bus, strat_bar, md::StrategyMode::BarOnly);
    md::EventReplay replayer("logs/md_events.log");
    md::ReplayFilter f;

    f.filter_by_topic  = true;
    f.topic            = md::Topic::MD_TICK;
    f.filter_by_symbol = false;   // all symbols for now
    replayer.set_filter(f);
    replayer.replay_realtime(bus);

    std::this_thread::sleep_for(200ms);
    // Explicitly flush any remaining open bars (also done in BarBuilder destructor)
    bar_builder.flush_all();
    std::this_thread::sleep_for(100ms);
    bus.unsubscribe(sub_bars);
    bus.stop();
    bus.print_stats();

    strat_bar.finalize();

    fmt::print("\n=== BarMomentum Strategy Account Summary ===\n");
    acct_bar.print_summary();

    const std::string csv_name = "trades_barmomentum.csv";
    acct_bar.dump_trades_csv(csv_name);
    fmt::print("[INFO] dumped bar-momentum trades to '{}'\n", csv_name);
    return 0;
}
 