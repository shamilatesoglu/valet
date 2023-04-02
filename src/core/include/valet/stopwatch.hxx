#pragma once

// stl
#include <chrono>
#include <string>

namespace valet::util
{

struct Stopwatch {
	Stopwatch();
	~Stopwatch() = default;
	void reset();
	/// Returns the elapsed time in seconds.
	double elapsed() const;
	std::string elapsed_str() const;
	static std::string elapsed_str(double seconds);

protected:
	std::chrono::high_resolution_clock::time_point start_time;
};

} // namespace valet::util