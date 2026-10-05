#include "core/thread_pool.h"

namespace ProxyBridge {

ThreadPool::ThreadPool(uint32_t numThreads) {
    m_threadCount = (numThreads == 0) ? std::thread::hardware_concurrency() : numThreads;
    if (m_threadCount == 0) m_threadCount = 4;

    for (uint32_t i = 0; i < m_threadCount; ++i) {
        m_workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                    if (m_stop && m_tasks.empty()) return;
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() { stop(); }

uint32_t ThreadPool::threadCount() const { return m_threadCount; }

size_t ThreadPool::pendingTasks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tasks.size();
}

bool ThreadPool::isRunning() const { return !m_stop; }

void ThreadPool::stop() {
    { std::lock_guard<std::mutex> lock(m_mutex); if (m_stop) return; m_stop = true; }
    m_condition.notify_all();
    for (auto& w : m_workers) if (w.joinable()) w.join();
    m_workers.clear();
}

} // namespace ProxyBridge
