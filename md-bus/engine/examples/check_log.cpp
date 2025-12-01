#include <fmt/core.h>

#include <fstream>
#include <string>
#include <limits>

#include "../common/event.hpp"
#include "../common/event_io.hpp"

int main (int argc, char ** argv){
    std::string path = "logs/md_events.log";
    if(argc > 1) {
        path = argv[1];
    }
    std::ifstream in(path);
    if(!in){
        fmt::print("[CHECK ] failed to open log file '{}'\n", path);
        return 1;
    }

    fmt::print("[CHECK ] Analysis log file '{}'\n", path);

    std::string line;
    uint64_t line_no         = 0;
    uint64_t total_events    = 0;
    uint64_t parse_errors    = 0;
    uint64_t backwards_count = 0;
    
    uint64_t prev_ts         = 0;
    bool first_event         = true;
    
    int64_t min_dt_ns        = std::numeric_limits<int64_t>::max();
    int64_t max_dt_ns        = std::numeric_limits<int64_t>::min();

    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;

        md::Event e;
        if (!md::parse_event(line, e)) {
            ++parse_errors;
            fmt::print("[CHECK] Parse error at line {}: '{}'\n", line_no, line);
            continue;
        }

        ++total_events;

        if (first_event) {
            first_event = false;
            prev_ts = e.h.ts_ns;
            continue;
        }

        int64_t dt_ns = static_cast<int64_t>(e.h.ts_ns) - static_cast<int64_t>(prev_ts);
        if (dt_ns < 0) {
            ++backwards_count;
            fmt::print("[CHECK] Timestamp went backwards at line {}: ts={} prev_ts={}\n",
                       line_no, e.h.ts_ns, prev_ts);
        }

        if (dt_ns < min_dt_ns) min_dt_ns = dt_ns;
        if (dt_ns > max_dt_ns) max_dt_ns = dt_ns;

        prev_ts = e.h.ts_ns;
    }

    fmt::print("\n[CHECK] Summary for '{}':\n", path);
    fmt::print("  total_events     = {}\n", total_events);
    fmt::print("  parse_errors     = {}\n", parse_errors);
    fmt::print("  backwards_count  = {}\n", backwards_count);

    if (!first_event && total_events > 1) {
        fmt::print("  min_dt_ns        = {}\n", (min_dt_ns == std::numeric_limits<int64_t>::max() ? 0 : min_dt_ns));
        fmt::print("  max_dt_ns        = {}\n", (max_dt_ns == std::numeric_limits<int64_t>::min() ? 0 : max_dt_ns));
    } else {
        fmt::print("  (not enough events for dt stats)\n");
    }

    return 0;
}