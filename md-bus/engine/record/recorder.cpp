#include "recorder.hpp"

namespace md {

EventRecorder::EventRecorder(const std::string& path)
    :path_{path} {
        std::filesystem::create_directories("logs");
        out_.open(path_, std::ios::out | std::ios::trunc);
        if(!out_){
            log_error("EventRecorder : failed to open file '{}'",path_);
            opened_ = false;
        }else {
            opened_ = true;
            log_info("EventRecorder : recording to '{}'", path_);
        }
    }

EventRecorder::~EventRecorder() {
    close();
}

void EventRecorder::on_event(const Event& e){
    if(!opened_)return;
    std::lock_guard<std::mutex> lk(mu_);
    if(!out_)return;
    const std::string line = serialize_event(e);
    out_ << line << '\n';
}

void EventRecorder::flush() {
    std::lock_guard<std::mutex> lk(mu_);
    if(out_) {
        out_.flush(); // std::flush();
    }
}

void EventRecorder::close() {
    std::lock_guard<std::mutex> lk(mu_);
    if(out_) {
        out_.flush();
        out_.close();
        log_info("EventRecorder : closed '{}'", path_);
    }
    opened_ = false;
}

}