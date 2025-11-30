// engine/examples/hello_bus.cpp
#include <fmt/core.h>

#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../io/timer.hpp"
#include "../record/recorder.hpp"
#include "../common/event_io.hpp"

int main() {
  md::EventBus bus(/*ingress*/1024, /*per-sub*/1024);

  md::EventRecorder recorder("logs/md_events.log");

    auto sub_ticks = bus.subscribe(md::Topic::MD_TICK, [](const md::Event e){
        if(std::holds_alternative<md::Tick>(e.p)){
            const auto&t = std::get<md::Tick>(e.p);
            fmt::print("[Tick] seq = {} sym = {} pq = {}\n",e.h.seq, t.symbol, t.pq);
        }
    });

    auto sub_logs = bus.subscribe(md::Topic::LOG, [](const md::Event e){
        if(std::holds_alternative<std::string>(e.p)){
            const auto&msg = std::get<std::string>(e.p);
            fmt::print("[LOG] seq = {} msg = {}\n", e.h.seq, msg);
        }
    });

    auto sub_hb = bus.subscribe(md::Topic::HEARTBEAT, [](const 
    md::Event e){
        fmt::print("[HB ] seq = {} topic = {}\n",
               e.h.seq,
               static_cast<int>(e.h.topic));
    });

    auto sub_all = bus.subscribe_all([](const md::Event e){
      fmt::print("[MON ] seq = {} topic = {}\n",
                e.h.seq,
                static_cast<int>(e.h.topic));
    });

    // Recorder subscription to log all events to file
    auto sub_rec = bus.subscribe_all([&recorder](const md::Event& e){
        recorder.on_event(e);
    });

    md::SimpleTimer hb_timer(
        md::Duration(200),[&bus]{
          md::Header h{};
          h.topic = md::Topic::HEARTBEAT;
          bus.publish(md::Event{.h = h, .p = std::string{"HB"}});
        }
    ); hb_timer.start();

  for (int i = 0; i < 5; ++i) {
    md::Tick t{
        .symbol = "NIFTY", 
        .pq = 22500.0 + i, 
        .qty = static_cast<uint32_t>(100 + i)
    };
    md::Header h{};
    h.topic = md::Topic::MD_TICK;
    bus.publish(md::Event{ .h = h, .p = t });

    md::Header log_h{};
    log_h.topic = md::Topic::LOG;
    std::string msg = " Published Ticks " + std::to_string(i);
    bus.publish(md::Event{.h = log_h, .p = msg});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }


  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  hb_timer.stop();
  bus.unsubscribe(sub_hb);
  bus.unsubscribe(sub_all);
  bus.unsubscribe(sub_ticks);
  bus.unsubscribe(sub_logs);
  bus.unsubscribe(sub_rec);

  bus.stop();

  recorder.flush();
  bus.print_stats();

  return 0;
}
