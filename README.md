---
title: "线程池的手动实现"
date: 2026-10-07
categories: ["学习记录"]
tags: ["C++", "线程池"]
description: "记录使用 C++11 手动实现简单线程池的过程，方便理解任务队列、工作线程和异步返回值。"
draft: false
---

# 线程池的手动实现

## 项目简介

本项目使用 C++11 实现了一个简单的线程池，主要包含以下部分：

1. **任务队列**：保存等待执行的任务。
2. **工作线程**：从任务队列中取出任务并执行。
3. **同步机制**：使用互斥锁和条件变量协调多个线程对任务队列的访问。
4. **异步结果传递**：使用 `std::packaged_task` 和 `std::future` 保存并获取任务结果。

本项目参考了 [progschj/ThreadPool](https://github.com/progschj/ThreadPool)，并补充了注释和原理说明，适合初学者学习线程池的基本实现。
Attention: 本项目仅用于学习和理解线程池的基本原理，并不适合直接用于生产环境。实际生产环境中，建议使用成熟的线程池库，如 [Boost.Thread](https://www.boost.org/doc/libs/release/doc/html/thread.html) 或 [Intel Threading Building Blocks (TBB)](https://www.threadingbuildingblocks.org/)。
## 前置知识

- Linux 基础
- C++11
- CMake
- C++ 泛型编程基础

## 环境

- 操作系统：Ubuntu 24.04 LTS
- C++ 标准：C++11
- 编译器：GCC 13+
- CMake：3.26+

## 线程池结构

![线程池结构图](img/thread_pool.png)

### 任务队列

任务队列使用以下类型保存任务：

```cpp
std::queue<std::function<void()>> tasks;
```

队列中的任务统一表示为“不接收参数且不返回值的可调用对象”。对于有返回值的用户任务，`enqueue` 会先使用 `std::packaged_task` 包装，再用一个 `void()` 类型的 Lambda 放入队列。

### 工作线程

线程池在构造时创建指定数量的工作线程。每个工作线程不断执行以下循环：

1. 获取任务队列的互斥锁。
2. 没有任务时，通过条件变量休眠。
3. 取出一个任务后立即释放锁。
4. 执行任务。
5. 回到第一步，继续等待下一个任务。

工作线程只在“线程池已停止且任务队列为空”时退出。因此，析构线程池时，已经入队的任务仍会被执行完。

### 同步机制

- `std::mutex queue_mutex`：保护任务队列和 `stop` 标志。
- `std::condition_variable condition`：让没有任务的工作线程休眠，并在有新任务时唤醒线程。
- `bool stop`：表示线程池是否正在停止。

## 使用方法

头文件名称为 `thread_pool.h`，在 Linux 上文件名大小写敏感：

```cpp
#include "thread_pool.h"

#include <iostream>

int main() {
    ThreadPool pool(4);

    pool.enqueue([] {
        std::cout << "Hello from thread pool!" << std::endl;
    });
}
```

`pool` 离开作用域时会自动析构。析构函数会通知工作线程停止，并等待工作线程结束。

## 获取任务返回值

`enqueue` 会返回一个 `std::future`。可以通过 `future.get()` 等待任务完成并获取任务中 `return` 返回的值：

```cpp
#include "thread_pool.h"

#include <iostream>

int main() {
    ThreadPool pool(4);

    auto future = pool.enqueue([](int a, int b) {
        return a + b;
    }, 10, 20);

    int result = future.get();
    std::cout << result << std::endl; // 30
}
```

`future.get()` 具有以下行为：

- 任务尚未完成时，阻塞当前线程并等待任务完成。
- 任务完成后，返回任务的结果。
- 如果任务抛出异常，`get()` 会重新抛出该异常。
- 同一个 `future` 通常只能调用一次 `get()`。

如果只需要等待任务完成，可以使用：

```cpp
future.wait();
```

## `ThreadPool` 类

```cpp
class ThreadPool {
public:
    ThreadPool(size_t numThreads);

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    ~ThreadPool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
```

### 构造函数

构造函数初始化 `stop`，并创建 `numThreads` 个工作线程：

```cpp
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(queue_mutex);

                    condition.wait(lock, [this] {
                        return stop || !tasks.empty();
                    });

                    if (stop && tasks.empty()) {
                        return;
                    }

                    task = std::move(tasks.front());
                    tasks.pop();
                }

                task();
            }
        });
    }
}
```

#### `condition.wait`

```cpp
condition.wait(lock, [this] {
    return stop || !tasks.empty();
});
```

工作线程在以下任一条件满足时被唤醒：

- 线程池正在停止；
- 任务队列不为空。

`wait` 在等待期间会暂时释放 `lock`，被唤醒后重新获得锁，并再次检查条件。这可以避免多个线程同时操作任务队列。

#### 取出任务后再执行

```cpp
task = std::move(tasks.front());
tasks.pop();
```

取出任务后，代码离开加锁的作用域，自动释放互斥锁，然后才执行：

```cpp
task();
```

这样可以缩短持锁时间，避免一个耗时任务阻塞其他工作线程取任务。

## `enqueue` 方法

`enqueue` 接收任意可调用对象和参数，并将它们绑定成一个不需要参数的任务：

```cpp
template<class F, class... Args>
auto enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(
            std::forward<F>(f),
            std::forward<Args>(args)...
        )
    );

    std::future<return_type> result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        tasks.emplace([task] {
            (*task)();
        });
    }

    condition.notify_one();
    return result;
}
```

### 返回类型推导

```cpp
using return_type = typename std::result_of<F(Args...)>::type;
```

`std::result_of` 根据可调用对象和参数推导返回类型。例如：

```cpp
[] { return 42; }             // return_type 为 int
[](int n) { return n * 2; }   // return_type 为 int
[] { /* 没有返回值 */ }       // return_type 为 void
```

`std::result_of` 是 C++11 中的写法；在 C++17 及更高标准中通常使用 `std::invoke_result`。

### `std::packaged_task` 和 `std::future`

这两个对象通过共享状态连接起来：

```text
packaged_task                         future
     │                                  │
     │ 执行任务并写入结果               │ 等待并读取结果
     └────────────── 共享状态 ──────────┘
```

执行过程如下：

1. `std::packaged_task<return_type()>` 包装用户任务。
2. `task->get_future()` 创建与该任务关联的 `std::future`。
3. 包装后的任务进入任务队列。
4. 工作线程执行 `(*task)()`。
5. 用户任务中的 `return` 值被写入共享状态。
6. 调用 `future.get()` 等待任务完成，并读取共享状态中的结果。

任务队列本身只负责保存“执行任务的操作”，不负责直接保存返回值。返回值由 `packaged_task` 写入共享状态，再由 `future` 读取。

如果任务执行过程中抛出异常，`packaged_task` 会将异常保存到共享状态；调用 `future.get()` 时，该异常会被重新抛出：

```cpp
auto future = pool.enqueue([]() -> int {
    throw std::runtime_error("task failed");
});

try {
    int result = future.get();
} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
}
```

### 使用 `std::make_shared` 的原因

任务需要先进入队列，之后再由工作线程执行。如果只使用局部的 `packaged_task`，函数返回后任务对象就会被销毁。

代码使用：

```cpp
auto task = std::make_shared<std::packaged_task<return_type()>>(...);
```

并让任务队列中的 Lambda 捕获 `task`：

```cpp
tasks.emplace([task] {
    (*task)();
});
```

这样，智能指针会保证 `packaged_task` 至少存活到任务执行结束。

## 析构函数

```cpp
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();

    for (std::thread& worker : workers) {
        worker.join();
    }
}
```

析构过程如下：

1. 加锁并将 `stop` 设置为 `true`。
2. 使用 `notify_all()` 唤醒所有正在等待的工作线程。
3. 工作线程继续执行队列中剩余的任务。
4. 当 `stop == true` 且任务队列为空时，工作线程退出。
5. `join()` 等待所有工作线程结束。

因此，线程池对象销毁后，不会留下仍在运行的工作线程。

## 示例程序

项目中的 `main.cpp` 提交 8 个任务到包含 4 个工作线程的线程池中：

```cpp
#include "thread_pool.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    ThreadPool pool(4);

    for (int i = 0; i < 8; ++i) {
        pool.enqueue([i] {
            std::cout << "Task " << i
                      << " is being processed by thread "
                      << std::this_thread::get_id() << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
}
```

这个示例中的任务没有返回值，因此没有保存和读取 `future`。程序结束时，`pool` 的析构函数仍会等待所有任务和工作线程结束。实际使用中，如果需要准确等待任务完成或取得返回值，建议保存 `enqueue` 返回的 `future`，而不要依赖固定时长的 `sleep`。

## 构建和运行

```bash
cmake -S . -B build
cmake --build build
./build/thread_pool
```

## 相关图片

- [线程池结构图](img/thread_pool.png)
- [线程执行流程图](img/thread_excuete.png)
