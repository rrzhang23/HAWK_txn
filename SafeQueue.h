#ifndef HAWK_SAFE_QUEUE_H
#define HAWK_SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <stdexcept>
#include <atomic>

extern std::atomic<bool> systemRunning;

template <typename T>
class SafeQueue
{
public:
    void push(T value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_.notify_one();
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]
                   { return !queue_.empty() || !systemRunning; });
        
        if (!systemRunning && queue_.empty())
        {
            throw std::runtime_error("Queue terminated due to system shutdown.");
        }
        
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    bool try_pop(T &value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty())
        {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    std::vector<T> drain() {
        std::unique_lock<std::mutex> lock(mutex_);
        std::vector<T> elements;
        while(!queue_.empty()) {
            elements.push_back(std::move(queue_.front()));
            queue_.pop();
        }
        return elements;
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

#endif // HAWK_SAFE_QUEUE_H