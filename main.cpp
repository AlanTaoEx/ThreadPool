#include "thread_pool.h"

#include <iostream>
#include <vector>
#include <chrono>
int main() {
    ThreadPool pool(4); // Create a pool with four worker threads.
    
    for(int i = 0; i < 8; ++i) {
        pool.enqueue([i] {
            std::cout << "Task " << i << " is being processed by thread " << 
            std::this_thread::get_id() << std::endl;
            // Simulate task processing time.
            std::this_thread::sleep_for(std::chrono::seconds(1));
        });
    }

    // Wait long enough for all submitted tasks to finish.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // The destructor also waits for all workers to finish.
    return 0;
}