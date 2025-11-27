#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <chrono>

namespace md {
    
enum class Topic : uint8_t {
    LOG = 0,
    MD_TICK = 1,
    HEARTBEAT = 2, 
    // MD_TRADE,
    // ORDER,
    // BOOK_UPDATE,
    // SYSTEM
};

struct Header {
    uint64_t seq{0};
    Topic topic{Topic::MD_TICK};
    uint64_t ts_ns{0};
};

struct Tick {
    std::string symbol;
    double pq{0.0};
    uint32_t qty{0};
};

using Payload = std::variant<std::monostate, Tick, std::string>;

struct Event {
    Header h;
    Payload p;
};

inline uint64_t now_ns() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<nanoseconds>(steady_clock::now().
    time_since_epoch()).count();
}


}