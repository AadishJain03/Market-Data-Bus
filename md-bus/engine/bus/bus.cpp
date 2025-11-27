#include "../bus/bus.hpp"
#include "../common/event.hpp"
#include "../common/log.hpp"
#include <fmt/core.h>


//this is the feature implementation file for bus.hpp
namespace md {

EventBus::EventBus(size_t ingress_cap_, size_t per_sub_cap_)
    : ingress_(std::make_unique<BoundedQueue<Event>>(ingress_cap_)),per_sub_cap_{per_sub_cap_}{

    for(auto &c : topic_counts_){
        c.store(0, std::memory_order_relaxed);
    }

    log_info("EventBus starting (ingress_cap = {}, per_sub_cap = {})",
            ingress_cap_, per_sub_cap_);
 
    reactor_ = std::thread([this]{reactor_loop();});
}

EventBus::~EventBus() { stop(); }

//creates a worker thread for each subscription
//the main thread return from the call while the worker thread continues 
//to call the callback function on the things published 
//the joining to the main thread happens in unsubscribe portion
SubId EventBus::subscribe(Topic T, Callback cb){
    auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
    auto slot = std::make_unique<SubSlot>();
    slot->t = T;
    slot->q = std::make_unique<BoundedQueue<Event>>(per_sub_cap_);
    slot->cb = std::move(cb);
    slot->worker = std::thread([s = slot.get()]{ // s is a lamda capture with c++14 and up (lamda local variable)
    // .get() returns a SubSlot* raw pointer
    Event ev;
    while (s->run.load(std::memory_order_relaxed)) {
      if (s->q->pop(ev)) {
        s->cb(ev); // execute user callback(that was passed during subscribe)
      }
    }
    // optional: drain remaining events before exit
    while (s->q->size() > 0) {
      s->q->pop(ev);
      s->cb(ev);
    }
  });
    {
        std::scoped_lock lk(mu_);
        subs_.emplace(id, std::move(slot));
    }
    return id;
}

SubId EventBus::subscribe_all(Callback cb){
    auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
    auto slot = std::make_unique<SubSlot>();
    slot->t = Topic::MD_TICK; // irrelevant all msg will be sent
    slot->q = std::make_unique<BoundedQueue<Event>>(per_sub_cap_);
    slot->cb = std::move(cb); // remember to move
    slot->worker = std::thread([s = slot.get()]{
        Event ev;
        while(s->run.load(std::memory_order_relaxed)){
            if(s->q->pop(ev)){
                s->cb(ev);
            }
        }
        while(s->q->size() > 0){
            s->q->pop(ev);
            s->cb(ev);
        }
    });
    {
        std::scoped_lock lk(mu_);
        all_subs_.emplace(id, std::move(slot));
    }
    return id;
}

//join back from the information stored in SubSlot
//first check joinable(fanning out the left over Event in the subqueue)
void EventBus::unsubscribe(SubId id){
    auto s = std::make_unique<SubSlot>();
    {
        std::scoped_lock lk(mu_);
        auto it = subs_.find(id);
        if(it != subs_.end()){
            s = std::move(it->second);
            subs_.erase(it);
        }else{
            auto it2 = all_subs_.find(id);
            if(it2 == all_subs_.end())return;
            s = std::move(it2->second);
            all_subs_.erase(it2);
        }
    }
    s->run.store(false, std::memory_order_relaxed);
    s->q->push(Event{}); // this can trigger the pop to wake up the worker if it's waiting
    //but this will run the callback with an empty Event
    if(s->worker.joinable()) s->worker.join();
}

//Increments Sequence and Pushes to Ingress : non blocking
bool EventBus::publish(Event e){
    e.h.seq = seq_.fetch_add(1, std::memory_order_relaxed);
    e.h.ts_ns = now_ns();

    published_.fetch_add(1, std::memory_order_relaxed);

    return ingress_->push(std::move(e));  // remember ingress_ is a pointer;
}

// Routes Events to Subscribers with matching topic
void EventBus::reactor_loop() {
    Event ev;
    while(run_.load(std::memory_order_relaxed)){
        if(!ingress_->pop(ev)) continue;
        
        ingress_popped_.fetch_add(1, std::memory_order_relaxed);

        auto idx = static_cast<int>(ev.h.topic);
        if(idx < kMaxTopics) {
            topic_counts_[idx].fetch_add(1, std::memory_order_relaxed);
        }

#ifdef BUS_DEBUG
        log_debug("[REACTOR] seq = {} topic = {}\n",
        ev.h.seq,
        static_cast<int>(ev.h.topic));
#endif

        std::scoped_lock lk(mu_);
        for(auto &kv : subs_){
            auto &slot = kv.second;
            if(slot->t == ev.h.topic) slot->q->push(ev);
        }
        for(auto &kv : all_subs_){
            auto &slot = kv.second;
            slot->q->push(ev);
        }
    }
    while(ingress_->size() > 0){
        ingress_->pop(ev);

        ingress_popped_.fetch_add(1, std::memory_order_relaxed);

        auto idx = static_cast<int>(ev.h.topic);
        if(idx < kMaxTopics){
            topic_counts_[idx].fetch_add(1, std::memory_order_relaxed);
        }
#ifdef BUS_DEBUG
        log_debug("[REACTOR-DRAIN] seq={} topic={}\n",
                   ev.h.seq,
                   static_cast<int>(ev.h.topic));
#endif

// ingress is usually empty because the while(run) loop fans out every event 
//before stop() is called that bit flips run ! Hence you will not see the
//print statement REACTOR-DRAIN on console 
        std::scoped_lock lk(mu_);
        for(auto &kv : subs_){
            auto &slot = kv.second;
            if(slot->t == ev.h.topic) slot->q->push(ev);
        }
        for(auto &kv : all_subs_){
            auto &slot = kv.second;
            slot->q->push(ev);
        }
    }
}


void EventBus::stop(){
    if(!run_.exchange(false))return;
    ingress_->push(Event{}); // wake up reactor if waiting 
    //because pop is blocking in reactor if nothing comes in ingress queue 
    //then it will stay blocked forever

    log_info("EventBus stopping...");

    if(reactor_.joinable())reactor_.join();

    std::vector<SubId> ids;
    {
        std::scoped_lock lk(mu_);
        ids.reserve(subs_.size());
        for(auto &kv : subs_) ids.push_back(kv.first);
        for(auto &kv : all_subs_) ids.push_back(kv.first);
    }
    for(auto id : ids) unsubscribe(id);
}

void EventBus::print_stats() const {
    log_info("EventBus stats:");
    log_info("  published        = {}", published_.load(std::memory_order_relaxed));
    log_info("  ingress_popped   = {}", ingress_popped_.load(std::memory_order_relaxed));

    auto load_topic = [&](Topic t) -> uint64_t {
        auto idx = static_cast<size_t>(t);
        if (idx >= kMaxTopics) return 0;
        return topic_counts_[idx].load(std::memory_order_relaxed);
    };

    log_info("  topic[MD_TICK]   = {}", load_topic(Topic::MD_TICK));
    log_info("  topic[LOG]       = {}", load_topic(Topic::LOG));
    log_info("  topic[HEARTBEAT] = {}", load_topic(Topic::HEARTBEAT));
}

}