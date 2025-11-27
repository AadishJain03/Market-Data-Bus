#pragma once
#include <fmt/core.h>
#include <string_view> 
#include <chrono>
#include <thread>
#include <utility>

namespace md {
enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

inline const char* to_string(LogLevel lvl) {
    switch(lvl){
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNK"; // unknown
}

inline LogLevel& global_log_level() {
    static LogLevel lvl = LogLevel::Debug;
    return lvl;
}


//the goal is to print a log message with log level and a format string
//And any number of extra arguments(of anytype)
//Args is a parameter pack can be int, int or string, int ,int etc (therefore an 
//array)

//&& is forwarding reference (and not an rvalue reference) 
// what it does is 	For lvalues → parameter is actually an lvalue reference.
//For rvalues → parameter is an rvalue reference.

//forward 	•	If the original argument was an lvalue, std::forward returns an lvalue → it will be copied, not moved.

//•	If the original argument was an rvalue / temporary, std::forward returns an rvalue → it can be moved instead of copied.

//i.e && gives you ability to avoid copies and forward Lets you avoid extra copies

template <typename... Args> //... is to tell Args is pack of types
inline void log(LogLevel lvl, std::string_view fmt_str, Args&&... args){
    if(static_cast<int>(lvl) < static_cast<int>(global_log_level())){
        return;
    }

    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms_since_epoch = duration_cast<milliseconds>(now.time_since_epoch()).count();

    auto tid = std::this_thread::get_id();

    fmt::print("[{}] t = {}ms tid = {} ",
                to_string(lvl),
                ms_since_epoch,
                (unsigned long long) std::hash<std::thread::id>{}(tid));
    
    fmt::print(fmt_str, std::forward<Args>(args)...); //... here acts as expanding pack
    fmt::print("\n");

}

// Convenience wrappers 
//calling log function that we defines above
template<typename... Args>
inline void log_debug(std::string_view fmt_str, Args&&... args){
    log(LogLevel::Debug, fmt_str, std::forward<Args>(args)...);
}

template<typename... Args>
inline void log_info(std::string_view fmt_str, Args&&... args){
    log(LogLevel::Info, fmt_str, std::forward<Args>(args)...);
}

template<typename... Args>
inline void log_warn(std::string_view fmt_str, Args&&... args){
    log(LogLevel::Warn, fmt_str, std::forward<Args>(args)...);
}

template<typename... Args>
inline void log_error(std::string_view fmt_str, Args&&... args){
    log(LogLevel::Error, fmt_str, std::forward<Args>(args)...);
}

}