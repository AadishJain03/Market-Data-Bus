#pragma once 

#include <fstream>
#include <mutex>
#include <string>

#include "../common/event.hpp"
#include "../common/event_io.hpp"
#include "../common/log.hpp"

namespace md{

class EventRecorder {
private:
    mutable std::mutex mu_;
    std::ofstream out_;
    bool opened_{false};
    std::string path_;
public:
    explicit EventRecorder(const std::string& path);
    ~EventRecorder();

    EventRecorder(const EventRecorder&) = delete;
    EventRecorder& operator=(const EventRecorder&) = delete;
    
    void on_event(const Event& e);

    void flush();
    void close();

};


}