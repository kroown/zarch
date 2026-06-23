#include "thread_pool.hpp"
#include <map>
#include <algorithm>

ThreadPool::ThreadPool(size_t count) {
    size_t n = std::max(size_t(1), count);
    for (size_t i = 0; i < n; i++)
        m_threads.emplace_back(&ThreadPool::worker_loop, this);
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto& t : m_threads)
        if (t.joinable()) t.join();
}

void ThreadPool::enqueue(int block_id, std::vector<uint8_t> input, TaskFunc func) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push({block_id, std::move(input), std::move(func)});
    m_cv.notify_one();
}

bool ThreadPool::wait_for_result(BlockTask& result) {
    std::unique_lock<std::mutex> lock(m_out_mutex);
    m_out_cv.wait(lock, [this]() {
        return m_results.find(m_next_id) != m_results.end();
    });
    if (m_results.empty()) return false;
    auto it = m_results.find(m_next_id);
    if (it == m_results.end()) return false;
    result = std::move(it->second);
    m_results.erase(it);
    m_next_id++;
    return true;
}

void ThreadPool::worker_loop() {
    while (true) {
        WorkItem item;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stop || !m_queue.empty(); });
            if (m_stop && m_queue.empty()) return;
            item = std::move(m_queue.front());
            m_queue.pop();
        }

        BlockTask result;
        result.block_id = item.block_id;
        result.input = std::move(item.input);
        result.uncompressed_size = result.input.size();
        item.func(result);

        {
            std::lock_guard<std::mutex> lock(m_out_mutex);
            m_results[result.block_id] = std::move(result);
        }
        m_out_cv.notify_one();
    }
}
