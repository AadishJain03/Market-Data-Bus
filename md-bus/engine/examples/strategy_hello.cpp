#include <fmt/core.h>
#include <thread>
#include <chrono>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include "../replay/replay.hpp"
#include "../strategy/strategy.hpp"
#include "../strategy/runner.hpp"

//simple strategy : print price when it reaches certain threshold
class ThresholdStrategy : public md::IStrategy {
private : 
    double threshold_;
public :
    explicit ThresholdStrategy(double threshold)
        :threshold_{threshold} {}
    
    void on_tick(const md::Tick& t, const md::Event& e) override {
        if(t.pq > threshold_) {
            fmt::print("[STRAT] seq={} sym={} pq={} > threshold {}\n",
                       e.h.seq, t.symbol, t.pq, threshold_);
        }
    }

    void on_log(const std::string& msg, const md::Event& e) override {
        fmt::print("[STRAT-LOG] seq={} msg={}\n", e.h.seq, msg);
    }

    void on_heartbeat(const md::Event& e) override {
        fmt::print("[STRAT-HB] seq={} topic={}\n",
                   e.h.seq, static_cast<int>(e.h.topic));
    }
};

int main() {
    using namespace std::chrono_literals;

    md::EventBus bus(/*ingress*/1024, /*per-sub*/ 1024);
    ThresholdStrategy strat(22502);

    //attaching strategy runner to its own scope so it unsubscribe before bus.stop 
    //(becasue we want the destructor to run)
    {
        md::StrategyRunner runner(bus, strat);
        md::EventReplay replayer("logs/md_events.log");

        md::ReplayFilter f;
        f.filter_by_topic = true;
        f.topic = md::Topic::MD_TICK;

        f.filter_by_symbol = true;
        f.symbol = "NIFTY";

        replayer.set_filter(f);
        replayer.replay_realtime(bus);
        std::this_thread::sleep_for(200ms);
    }
    bus.stop();
    bus.print_stats();
    return 0;
}