#pragma once
#include<atomic>
#include<functional>
#include<memory>
#include<string>
#include<thread>
#include<unordered_map>
#include<vector>

#include "../common/bounded_queue.hpp"
#include "../common/event.hpp"

namespace md {

using Callback = std::function<void(const Event&)>;
using SubId = uint64_t;

class EventBus {
private:
    struct SubSlot {
        Topic t {Topic::MD_TICK};
        std::unique_ptr<BoundedQueue<Event>> q;
        std::thread worker;
        std::atomic<bool>run{true};
        Callback cb;
    };

    void reactor_loop();

    std::unique_ptr<BoundedQueue<Event>> ingress_; //producer - > reactor
    std::thread reactor_;
    std::atomic<bool> run_{true};

    // rounting and bookkeeping (for subscriptions)
    std::mutex mu_;
    std::unordered_map<SubId, std::unique_ptr<SubSlot>> subs_;
    std::unordered_map<SubId, std::unique_ptr<SubSlot>> all_subs_;
    const size_t per_sub_cap_;

    // sequence + ids
    std::atomic<uint64_t> seq_{0};
    std::atomic<uint64_t> next_id_{1}; // remember ot initialize this from 1st id

    std::atomic<uint64_t> published_{0};
    std::atomic<uint64_t> ingress_popped_{0};

    static constexpr size_t kMaxTopics = 8;
    std::array<std::atomic<uint64_t>, kMaxTopics> topic_counts_{0}; // array to keep 
    //track of the topic counts

public:
    explicit EventBus(size_t ingress_cap = 65536, size_t per_sub_cap = 65536);

    ~EventBus();

    //declaration
    SubId subscribe(Topic T, Callback cb);
    SubId subscribe_all(Callback cb);
    void unsubscribe(SubId id);

    // (non blocking) enqueue in ingress_ and return
    bool publish(Event e);
    void stop(); // gracefully shutdown

    void print_stats() const;


};
}


