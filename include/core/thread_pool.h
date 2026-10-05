#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <cstdint>

namespace ProxyBridge {

class ThreadPool {
public:
    explicit ThreadPool(uint32_t numThreads = 0);
    ~ThreadPool();

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    uint32_t threadCount() const;
    size_t pendingTasks() const;
    bool isRunning() const;
    void stop();

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop{false};
    uint32_t m_threadCount;
};

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using ReturnType = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<ReturnType> result = task->get_future();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stop) throw std::runtime_error("ThreadPool stopped");
        m_tasks.emplace([task]() { (*task)(); });
    }
    m_condition.notify_one();
    return result;
}

} // namespace ProxyBridge
