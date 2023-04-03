#pragma once

// external
#include <spdlog/spdlog.h>

// stl
#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace valet::util
{

class ThreadPool
{
public:
	ThreadPool(uint32_t num_threads);

	~ThreadPool();

	void enqueue(std::function<void()> task);

	void wait();

	void stop();

protected:
	void worker(uint32_t thread_id);

	std::atomic_bool running;
	std::vector<std::thread> threads;

	std::queue<std::function<void()>> tasks;
	std::mutex tasks_mutex;
	std::condition_variable tasks_cond;

	std::mutex wait_mutex;
	std::condition_variable wait_cond;
	std::atomic_uint64_t task_count = 0;
};
} // namespace valet::util