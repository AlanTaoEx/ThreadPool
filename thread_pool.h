#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <cstddef>
#include <utility>

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        
        // Use a shared pointer to keep the task alive until a worker executes it.
        // Bind the function and arguments, then wrap them in a packaged_task
        // so that the asynchronous result can be retrieved through a future.
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        // Obtain the future associated with the packaged task.
        std::future<return_type> res = task->get_future();

        // Add the task to the queue.
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            // Reject submissions after the pool has started shutting down.
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            // Store a void callable in the queue. It invokes the packaged task.
            tasks.emplace([task]() { (*task)(); });
        }
        // Wake one worker waiting for a new task.
        condition.notify_one();
        return res;
};
    ~ThreadPool();
private:
    // Worker threads that consume tasks from the queue.
    std::vector<std::thread> workers;
    // Queue of pending tasks.
    std::queue<std::function<void()>> tasks;
    // Synchronization primitives for accessing the task queue.
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
#endif