#include "valet/thread_utils.hxx"

// external
#include <spdlog/spdlog.h>

namespace valet::util
{

ThreadPool::ThreadPool(uint32_t num_threads) : running(false)
{
	threads.resize(num_threads);
	for (uint32_t i = 0; i < num_threads; ++i) {
		threads[i] = std::thread(&ThreadPool::worker, this, i);
	}
	running = true;
}

ThreadPool::~ThreadPool()
{
	stop();
	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

void ThreadPool::enqueue(std::function<void()> task)
{
	std::unique_lock lock(tasks_mutex);
	tasks.push(std::move(task));
	++task_count;
	spdlog::trace("Task enqueued, {} tasks remaining", task_count.load());
	tasks_cond.notify_one();
}

void ThreadPool::wait()
{
	std::unique_lock lock(wait_mutex);
	wait_cond.wait(lock, [this] { return task_count == 0; });
}

void ThreadPool::stop()
{
	wait();
	running = false;
	tasks_cond.notify_all();
}

void ThreadPool::worker(uint32_t thread_id)
{
	while (running) {
		std::function<void()> task;
		{
			std::unique_lock lock(tasks_mutex);
			tasks_cond.wait(lock, [this] { return !tasks.empty() || !running; });
			if (!running) {
				spdlog::trace("Worker thread {} exiting", thread_id);
				return;
			}
			task = std::move(tasks.front());
			tasks.pop();
		}
		task();
		--task_count;
		spdlog::trace("Task finished, {} tasks remaining", task_count.load());
		if (task_count == 0) {
			spdlog::trace("All tasks finished, notifying waiters");
			wait_cond.notify_all();
		}
	}
}

} // namespace valet::util