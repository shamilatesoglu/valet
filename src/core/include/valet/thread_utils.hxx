#pragma once

// stl
#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <atomic>

namespace valet::util
{

struct ThreadPool {
	ThreadPool(uint32_t num_threads);
	ThreadPool(ThreadPool const&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(ThreadPool const&) = delete;
	~ThreadPool();
	bool enqueue(std::function<void()> task);
	void wait();

protected:
	std::vector<std::thread> threads;
	std::mutex tasks_mutex;
	std::queue<std::function<void()>> tasks;
	std::mutex sleeper_mutex;
	std::condition_variable sleeper;
	std::atomic_bool stopped = false;
};

} // namespace valet::util