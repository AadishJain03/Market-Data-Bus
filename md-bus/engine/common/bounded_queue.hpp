#pragma once
#include<iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>


// A thread-safe bounded queue implementation
namespace md{

template <typename T>
class BoundedQueue {
private : 
    const size_t capacity_;
    std::queue<T> queue_;
    mutable std::mutex mutex_; // mutable to allow size() to use it 
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
public:
    explicit BoundedQueue(size_t capacity)
        : capacity_(capacity) {}

    //blocking 
    bool push(T item){
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] {return queue_.size() < capacity_;});
        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }
    // non blocking : checking
    bool try_push(T item){
        std::unique_lock<std::mutex> lock(mutex_);
        if(queue_.size() < capacity_){
            queue_.push(std::move(item));
            not_empty_.notify_one();
            return true;
        }
        return false;
    }
    //blocking 
    bool pop(T &out){
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] {return !queue_.empty();});
        out = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return true;
    }
    // non_blocking
    bool try_pop(T &out){
        std::unique_lock<std::mutex> lock(mutex_);
        if(queue_.empty())return false;
        out = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return true;
    }
    //size
    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }
    bool empty() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};


}


