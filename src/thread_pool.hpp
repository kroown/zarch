#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>

class ThreadPool {
public:
    explicit ThreadPool(size_t count);
    ~ThreadPool();

    struct BlockTask {
        int block_id;
        std::vector<uint8_t> input;
        std::vector<uint8_t> output;
        uint32_t checksum = 0;
        size_t uncompressed_size = 0;
    };

    using TaskFunc = std::function<void(BlockTask&)>;
    void enqueue(int block_id, std::vector<uint8_t> input, TaskFunc func);
    bool wait_for_result(BlockTask& result);

    size_t thread_count() const { return m_threads.size(); }

private:
    struct WorkItem {
        int block_id;
        std::vector<uint8_t> input;
        TaskFunc func;
    };

    std::vector<std::thread> m_threads;
    std::queue<WorkItem> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;

    // ordered output by block_id
    std::mutex m_out_mutex;
    std::condition_variable m_out_cv;
    std::map<int, BlockTask> m_results;
    int m_next_id = 0;

    void worker_loop();
};
