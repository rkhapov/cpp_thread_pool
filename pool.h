#pragma once

#include <thread>
#include <functional>
#include <atomic>
#include <future>
#include <condition_variable>
#include <vector>
#include <queue>
#include <mutex>

class MySimpleThreadPool
{
public:
    MySimpleThreadPool(size_t workersCount);

    ~MySimpleThreadPool();

    MySimpleThreadPool(const MySimpleThreadPool& other) = delete;

    MySimpleThreadPool(MySimpleThreadPool&& other) = delete;

    MySimpleThreadPool& operator=(const MySimpleThreadPool& other) = delete;

    MySimpleThreadPool& operator=(MySimpleThreadPool&& other) = delete;

    template <class F, class ...Args>
    auto EnqueueUserTask(F&& action, Args&&... args);

    const size_t workersCount;

private:
    using TaskT = std::function<void()>;

    std::queue<TaskT> tasksQueue;
    std::vector<std::thread> workers;
    std::condition_variable tasksQueueNotifier;
    std::mutex synchronizationMutex;
    std::atomic<bool> cancellationRequested;

    void DequeueTasksCycle([[maybe_unused]] size_t workerId);

    bool TryDequeueTask(TaskT* dequeuedTask);
};


MySimpleThreadPool::MySimpleThreadPool(size_t workersCount) : workersCount(workersCount)
{
    cancellationRequested = false;

    workers.reserve(workersCount);

    for (size_t i = 0; i < workersCount; i++)
    {
        workers.emplace_back([this, i] { this->DequeueTasksCycle(i); });
    }
}

MySimpleThreadPool::~MySimpleThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(synchronizationMutex);
        cancellationRequested = true;
    }

    tasksQueueNotifier.notify_all();

    for (auto& worker : workers)
    {
        worker.join();
    }
}

void MySimpleThreadPool::DequeueTasksCycle([[maybe_unused]] size_t workerId)
{
    while (true)
    {
        TaskT dequeuedTask;

        if (!TryDequeueTask(&dequeuedTask))
        {
            break;
        }

        std::invoke(dequeuedTask);
    }
}

bool MySimpleThreadPool::TryDequeueTask(TaskT* dequeuedTask)
{
    std::unique_lock<std::mutex> lock(synchronizationMutex);
    tasksQueueNotifier.wait(lock, [this] { return cancellationRequested || !tasksQueue.empty(); });

    if (cancellationRequested)
        return false;

    *dequeuedTask = tasksQueue.front();

    tasksQueue.pop();

    return true;
}

template <class F, class ... Args>
auto MySimpleThreadPool::EnqueueUserTask(F&& action, Args&&... args)
{
    using TaskReturnType = decltype(std::invoke(action, args...));

    // bind instead of lambda because of arguments passing
    auto bind = std::bind(std::forward<F>(action), std::forward<Args>(args)...);
    auto task = std::make_shared<std::packaged_task<TaskReturnType()>>(bind);

    {
        std::lock_guard<std::mutex> lock(synchronizationMutex);

        tasksQueue.emplace([task] { (*task)(); });
    }

    tasksQueueNotifier.notify_one();

    return task->get_future();
}

