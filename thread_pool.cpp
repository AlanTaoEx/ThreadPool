#include "thread_pool.h"

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    // Create the requested number of worker threads.
    for (size_t i = 0; i < numThreads; ++i) {
        this->workers.emplace_back(
            // Define the worker loop.
            [this] {
                while(true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        // Wait until a task is available or shutdown begins.
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        // Exit only after shutdown has begun and all tasks are drained.
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }
                        // Move one task out of the queue while holding the lock.
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    // Execute the task after releasing the queue lock.
                    task();
                }
            }
        );
    }
};

ThreadPool::~ThreadPool() {
    // Set the stop flag and notify all workers.
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    // Wait for every worker to finish.
    for (std::thread &worker : workers) {
        worker.join();
    }
};
