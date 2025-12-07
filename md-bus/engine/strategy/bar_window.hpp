#pragma once 

#include <deque>
#include "../common/event.hpp"

namespace md {

class BarWindow {
private :
    std::size_t max_size_;
    std::deque<Bar> window_;
public :
    explicit BarWindow(std::size_t max_size)
        :max_size_{max_size} {}
    
    void push(const Bar& b) {
        if(max_size_ == 0)return;
        window_.push_back(b);
        if(window_.size() > max_size_) {
            window_.pop_front();
        }
    }

    bool full() const {
        return window_.size() == max_size_;
    }

    std::size_t size() const {
        return window_.size();
    }

    double momentum() const {
        if(!full()) return 0.0;
        const auto& first = window_.front();
        const auto& last = window_.back();
        return last.close - first.close;
    }

    const Bar& front() const {return window_.front();}
    const Bar& back() const {return window_.back();}
};
}
