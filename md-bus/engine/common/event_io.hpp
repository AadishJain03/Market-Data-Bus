#pragma once 
#include <string>
#include <string_view>
#include <variant>

#include "event.hpp"

namespace md {

// topic to string
inline std::string to_string(Topic t){
    switch (t) {
        case Topic::MD_TICK : return "MD_TICK";
        case Topic::LOG : return "LOG";
        case Topic::HEARTBEAT : return "HEARTBEAT";
        case Topic::BAR_1S : return "BAR_1S";
        case Topic::BAR_1M : return "BAR_1M";
    }
    return "UNKNOWN";
}

//topic from string
inline bool topic_from_string(std::string_view s, Topic& out){
    if(s == "MD_TICK") {out = Topic::MD_TICK; return true;}
    if(s == "LOG") {out = Topic::LOG; return true;}
    if(s == "HEARTBEAT") {out = Topic::HEARTBEAT; return true;}
    if(s == "BAR_1S") {out = Topic::BAR_1S; return true;}
    if(s == "BAR_1M") {out = Topic::BAR_1M; return true;}
    return false;
}

// --- Payload serialization ---
// Format:
//   monostate: "-"
//   Tick:      "TICK|<symbol>|<pq>|<qty>"
//   Log:       "LOG|<text>"
// (We assume log text doesn’t contain newlines or '|'; fine for now.)

inline std::string serialize_payload(const Payload& p){
    if(std::holds_alternative<std::monostate>(p)){
        return "-";
    }

    if(std::holds_alternative<Tick>(p)){
        const auto& t = std::get<Tick>(p);
        std::string s;
        s.reserve(64);
        s.append("TICK|");
        s.append(t.symbol);
        s.push_back('|');
        s.append(std::to_string(t.pq));
        s.push_back('|');
        s.append(std::to_string(t.qty));
        return s;
    }

    if(std::holds_alternative<std::string>(p)){
        const auto& msg = std::get<std::string>(p);
        std::string s;
        s.reserve(16 + msg.size());
        s.append("LOG|");
        s.append(msg);
        return s;
    }
    return "UNKNOWN";
}

// --- Event serialization ---
// Line format:
//   seq,ts_ns,topic,payload
// Example:
//   0,1234567890,MD_TICK,TICK|NIFTY|22500.0|100

inline std::string serialize_event(const Event& e){
    std::string s;
    s.reserve(128);

    s.append(std::to_string(e.h.seq));
    s.push_back(',');
    s.append(std::to_string(e.h.ts_ns));
    s.push_back(',');
    s.append(to_string(e.h.topic));
    s.push_back(',');
    s.append(serialize_payload(e.p));
    return s;
}

//parsing helpers 
//reconstructing Event from string line

// splits string_view by delim into vector of string_views
// string_view is a non-owning view (pointer + length) into existing string data,
// used here to avoid copying substrings.

inline std::vector<std::string_view> split_sv(std::string_view s, char delim) {
    std::vector<std::string_view> out ;
    size_t start = 0;
    while(start <= s.size()){
        size_t pos = s.find(delim, start);
        if(pos == std::string_view::npos){
            out.emplace_back(s.substr(start));
            break;
        }
        //pos - start due to substr second arg being length
        out.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out ;
}

// reconstruct Payload from string_view
inline Payload parse_payload(std::string_view s) {
    if(s == "-" || s.empty()) return  std::monostate{};
    if(s.rfind("TICK|", 0) == 0) {
        auto rest = s.substr(5);
        auto parts = split_sv(rest, '|');
        if(parts.size() < 3){
            return std::monostate{};
        }
        Tick t;
        t.symbol = std::string(parts[0]);
        try{
            t.pq = std::stod(std::string(parts[1]));
            t.qty = static_cast<uint32_t>(std::stoul(std::string(parts[2])));
        }catch(...) {
            return std::monostate{};
        }
        return t;
    }

    if(s.rfind("LOG|", 0) == 0){
        auto msg = s.substr(4);
        return std::string(msg);
    }
    return std::string(s);
}

inline bool parse_event(std::string_view line, Event& out) {
    auto parts = split_sv(line, ',');
    if(parts.size() < 4){
        return false;
    }

    try {
        out.h.seq = static_cast<uint64_t>(std::stoull(std::string(parts[0])));
        out.h.ts_ns = static_cast<uint64_t>(std::stoull(std::string(parts[1])));
    } catch (...) { // catch all handler 
        return false;
    }

    Topic t;
    if(!topic_from_string(parts[2], t)) {
        return false;
    }
    out.h.topic = t;

    out.p = parse_payload(parts[3]);
    return true;
}

}