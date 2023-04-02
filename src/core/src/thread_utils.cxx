#include "valet/thread_utils.hxx"

// external
#include <spdlog/spdlog.h>

namespace valet::util
{

ThreadPool::ThreadPool(uint32_t num_threads)
{
	for (uint32_t i = 0; i < num_threads; ++i) {
		threads.emplace_back([this]() {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(this->tasks_mutex);
                                        std::unique_lock<std::mutex> sleeper_lock(this->sleeper_mutex);
					this->sleeper.wait(sleeper_lock, [this]() {
						return this->stopped || !this->tasks.empty();
					});
					if (this->stopped && this->tasks.empty())
						return;
					task = std::move(this->tasks.front());
					this->tasks.pop();
					spdlog::debug(
					    "ThreadPool: task popped from queue ({} tasks left)",
					    this->tasks.size());
				}
				task();
			}
		});
	}
}

ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(tasks_mutex);
		stopped = true;
	}
	sleeper.notify_all();
	for (std::thread& thread : threads)
		thread.join();
}

bool ThreadPool::enqueue(std::function<void()> task)
{
	{
		std::unique_lock<std::mutex> lock(tasks_mutex);
		if (stopped)
			return false;
		tasks.emplace(task);
	}
	sleeper.notify_one();
	return true;
}

void ThreadPool::wait()
{
	std::unique_lock<std::mutex> lock(sleeper_mutex);
	sleeper.wait(lock, [this]() { return this->tasks.empty(); });
}

} // namespace valet::util